/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include "painter-xcf-storage.hpp"
#include <gegl.h>
#include <gio/gio.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
extern "C" {
#include "libgimpbase/gimpbase.h"
#include "core/core-types.h"
#include "core/gimpimage.h"
#include "core/gimp.h"
#include "core/gimpfilterlayer.h"
#include "core/gimpitem.h"
#include "core/gimplayer.h"
#include "core/gimplayermask.h"
#include "core/gimpparamspecs.h"
}
#include "painter-xcf-arguments.hpp"
#include "painter/object-ref.hpp"
#include <cstring>
#include <algorithm>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
namespace GimpPainterXcf {
namespace {
struct VariantFree { void operator() (GVariant *p) const { if (p) g_variant_unref (p); } };
using Variant = std::unique_ptr<GVariant, VariantFree>;
struct ArgsFree { void operator() (GimpValueArray *p) const { if (p) gimp_value_array_unref (p); } };
using Args = std::unique_ptr<GimpValueArray, ArgsFree>;
struct Value
{
  GValue value = G_VALUE_INIT;
  Value () = default;
  explicit Value (GType type) { g_value_init (&value, type); }
  ~Value () { if (G_VALUE_TYPE (&value)) g_value_unset (&value); }
};
struct Builder
{
  GVariantBuilder value;
  bool ended = false;
  explicit Builder (const char *type) { g_variant_builder_init (&value, G_VARIANT_TYPE (type)); }
  ~Builder () { if (!ended) g_variant_builder_clear (&value); }
  GVariant *end () { ended = true; return g_variant_builder_end (&value); }
};
[[noreturn]] void invalid () { throw std::runtime_error ("Unsupported or malformed saved Filter argument; original definition is retained"); }
constexpr gsize block_size = 65536;
void cancelled (GCancellable *cancel)
{
  if (cancel && g_cancellable_is_cancelled (cancel))
    throw StorageError (G_IO_ERROR, G_IO_ERROR_CANCELLED, "Operation was cancelled");
}
GVariant *bytes (const void *data, gsize length, GCancellable *cancel)
{
  cancelled (cancel);
  if (length <= block_size) return g_variant_new_fixed_array (G_VARIANT_TYPE_BYTE, data, length, 1);
  if (length > G_MAXSSIZE) invalid ();
  auto *input = g_memory_input_stream_new_from_data (data, length, nullptr);
  std::unique_ptr<GInputStream, decltype (&g_object_unref)> held (input, g_object_unref);
  auto snapshot = GimpPainterXcf::snapshot_stream (input, cancel);
  return GimpPainterXcf::byte_variant (snapshot.get ());
}
GVariant *string_bytes (const gchar *text, GCancellable *cancel)
{
  cancelled (cancel);
  gsize length = 0;
  if (text)
    for (;;)
      {
        /* A long string must not hide an uncancellable strlen() pass. Only
         * examine bytes up to its terminator, checking between fixed blocks. */
        cancelled (cancel);
        gsize count = 0;
        const gsize available = std::min (block_size, G_MAXSIZE - length);
        while (count < available && text[length + count]) ++count;
        if (count == G_MAXSIZE - length) invalid ();
        length += count;
        if (count < block_size) { ++length; break; }
      }
  return g_variant_new ("(b@ay)", text != nullptr, bytes (text, length, cancel));
}
#if G_BYTE_ORDER == G_BIG_ENDIAN
/* Convert borrowed native components as the snapshot reader consumes them.
 * This also handles unaligned array data without asking GVariant to copy it. */
struct EndianInput
{
  GInputStream parent;
  const guint8 *data;
  gsize length, position, width;
};
struct EndianInputClass { GInputStreamClass parent; };
G_DEFINE_TYPE (EndianInput, endian_input, G_TYPE_INPUT_STREAM)
static gssize endian_input_read (GInputStream *stream, void *buffer, gsize count,
                                 GCancellable *cancel, GError **error)
{
  if (cancel && g_cancellable_set_error_if_cancelled (cancel, error)) return -1;
  auto *input = reinterpret_cast<EndianInput *> (stream);
  count = std::min ({count, input->length - input->position, block_size});
  auto *output = static_cast<guint8 *> (buffer);
  const gsize mask = input->width - 1;
  for (gsize i = 0; i < count; ++i)
    {
      const gsize offset = input->position + i;
      output[i] = input->data[(offset & ~mask) + (mask - (offset & mask))];
    }
  input->position += count;
  return count;
}
static void endian_input_class_init (EndianInputClass *klass)
{ G_INPUT_STREAM_CLASS (klass)->read_fn = endian_input_read; }
static void endian_input_init (EndianInput *) {}
#endif
GVariant *array_bytes (const GimpArray *array, gsize width, GCancellable *cancel)
{
  cancelled (cancel);
  const gsize length = array ? array->length : 0;
  const guint8 *data = array ? array->data : nullptr;
  if (length % width) invalid ();
#if G_BYTE_ORDER == G_BIG_ENDIAN
  if (length && width > 1)
    {
      auto *input = static_cast<EndianInput *> (g_object_new (endian_input_get_type (), nullptr));
      std::unique_ptr<GInputStream, decltype (&g_object_unref)> held (G_INPUT_STREAM (input), g_object_unref);
      input->data = data; input->length = length; input->width = width;
      if (length <= block_size)
        {
          guint8 raw[block_size];
          if (endian_input_read (held.get (), raw, length, cancel, nullptr) < 0)
            throw StorageError (G_IO_ERROR, G_IO_ERROR_CANCELLED, "Operation was cancelled");
          return bytes (raw, length, cancel);
        }
      auto snapshot = GimpPainterXcf::snapshot_stream (held.get (), cancel);
      return GimpPainterXcf::byte_variant (snapshot.get ());
    }
#endif
  return bytes (data, length, cancel);
}
struct MaterializationError : std::runtime_error
{ using std::runtime_error::runtime_error; };
struct MaterializationBudget
{
  /* The original envelope could contain at most 256 MiB of mutable scalar
   * payloads. Preserve that range without allowing a larger transport to
   * request arbitrary native allocations. Shared immutable bytes do not count.
   * STRV pointer tables retain their existing per-array slot bound separately. */
  gsize remaining = gsize (256) * 1024 * 1024;
  void consume (gsize size)
  {
    if (size > remaining)
      throw MaterializationError ("Saved Filter arguments exceed the mutable materialization limit; original definition is retained");
    remaining -= size;
  }
};
[[noreturn]] void allocation_failed ()
{ throw MaterializationError ("Not enough memory to restore saved Filter arguments; original definition is retained"); }
struct BufferFree { void operator() (guint8 *p) const { g_free (p); } };
using Buffer = std::unique_ptr<guint8, BufferFree>;
Buffer materialize_array (GVariant *raw, gsize width, MaterializationBudget& budget)
{
  const gsize size = g_variant_get_size (raw);
  if (size % width) invalid ();
  budget.consume (size);
  Buffer copy (static_cast<guint8 *> (g_try_malloc (size)));
  if (size && !copy) allocation_failed ();
  if (size) std::memcpy (copy.get (), g_variant_get_data (raw), size);
#if G_BYTE_ORDER == G_BIG_ENDIAN
  for (gsize i = 0; i < size; i += width)
    std::reverse (copy.get () + i, copy.get () + i + width);
#endif
  return copy;
}
gchar *read_string (GVariant *variant, MaterializationBudget& budget)
{
  gboolean present;
  GVariant *raw;
  if (!g_variant_is_of_type (variant, G_VARIANT_TYPE ("(bay)"))) invalid ();
  g_variant_get (variant, "(b@ay)", &present, &raw);
  Variant owned (raw);
  const gsize length = g_variant_get_size (raw);
  if (!present) { if (length) invalid (); return nullptr; }
  budget.consume (length);
  const auto *data = static_cast<const gchar *> (g_variant_get_data (raw));
  if (!length || data[length - 1] || std::memchr (data, 0, length - 1)) invalid ();
  auto *copy = static_cast<gchar *> (g_try_malloc (length));
  if (!copy) allocation_failed ();
  std::memcpy (copy, data, length);
  return copy;
}
struct ArgumentEntry
{
  Variant name;
  Variant payload;
  gboolean is_null = FALSE;
  ArgumentEntry (GVariant *entry, bool snapshot)
    : name (g_variant_get_child_value (entry, 0))
  {
    /* An aggregate borrowed-string format such as (&sbv) can serialize the
     * whole tuple, copying a large immutable payload in a freshly built AST.
     * Keep each child independently so only the small name needs string data. */
    Variant wrapped (g_variant_get_child_value (entry, snapshot ? 2 : 1));
    payload.reset (g_variant_get_variant (wrapped.get ()));
    if (snapshot)
      {
        Variant flag (g_variant_get_child_value (entry, 1));
        is_null = g_variant_get_boolean (flag.get ());
      }
  }
  const gchar *type_name () const { return g_variant_get_string (name.get (), nullptr); }
};
GVariant *reference (GObject *object, GimpImage *image, GCancellable *cancel)
{
  cancelled (cancel);
  guint32 kind = 0, tattoo = 0;
  if (object == G_OBJECT (image)) kind = 1;
  else if (object && GIMP_IS_ITEM (object) && gimp_item_get_image (GIMP_ITEM (object)) == image)
    { kind = 2; tattoo = gimp_item_get_tattoo (GIMP_ITEM (object)); }
  else if (object) throw std::runtime_error ("Filter argument refers outside the saved image; save was not started");
  return g_variant_new ("(uu)", kind, tattoo);
}
GObject *find_reference (GVariant *variant, GimpImage *image)
{
  guint32 kind, tattoo;
  if (!g_variant_is_of_type (variant, G_VARIANT_TYPE ("(uu)"))) invalid ();
  g_variant_get (variant, "(uu)", &kind, &tattoo);
  if (!kind) return nullptr;
  if (kind == 1) return G_OBJECT (image);
  if (kind != 2 || !tattoo) invalid ();
  GObject *result = nullptr;
  GList *items = gimp_image_get_layer_list (image);
  items = g_list_concat (items, gimp_image_get_channel_list (image));
  items = g_list_concat (items, gimp_image_get_path_list (image));
  for (GList *p = items; p; p = p->next)
    {
      if (gimp_item_get_tattoo (GIMP_ITEM (p->data)) == tattoo)
        { if (result) { g_list_free (items); invalid (); } result = G_OBJECT (p->data); }
      if (GIMP_IS_LAYER (p->data))
        {
          auto *mask = gimp_layer_get_mask (GIMP_LAYER (p->data));
          if (mask && gimp_item_get_tattoo (GIMP_ITEM (mask)) == tattoo)
            { if (result) { g_list_free (items); invalid (); } result = G_OBJECT (mask); }
        }
    }
  if (auto *mask = gimp_image_get_mask (image))
    if (gimp_item_get_tattoo (GIMP_ITEM (mask)) == tattoo)
      { if (result) { g_list_free (items); invalid (); } result = G_OBJECT (mask); }
  g_list_free (items);
  if (!result) invalid (); /* never bind an unrelated same-name item */
  return result;
}
GVariant *encode (const GimpValueArray *, GimpImage *, unsigned, GCancellable *);
GimpValueArray *decode (GVariant *, GimpImage *, unsigned, MaterializationBudget&);
GVariant *encode_value (const GValue *v, GimpImage *image, unsigned depth, GCancellable *cancel)
{
  cancelled (cancel);
  const GType type = G_VALUE_TYPE (v);
  if (type == G_TYPE_BOOLEAN) return g_variant_new_boolean (g_value_get_boolean (v));
  if (type == G_TYPE_CHAR) return g_variant_new_int64 (g_value_get_schar (v));
  if (type == G_TYPE_INT) return g_variant_new_int64 (g_value_get_int (v));
  if (type == G_TYPE_LONG) return g_variant_new_int64 (g_value_get_long (v));
  if (type == G_TYPE_INT64) return g_variant_new_int64 (g_value_get_int64 (v));
  if (type == G_TYPE_UCHAR) return g_variant_new_uint64 (g_value_get_uchar (v));
  if (type == G_TYPE_UINT) return g_variant_new_uint64 (g_value_get_uint (v));
  if (type == G_TYPE_ULONG) return g_variant_new_uint64 (g_value_get_ulong (v));
  if (type == G_TYPE_UINT64) return g_variant_new_uint64 (g_value_get_uint64 (v));
  if (G_VALUE_HOLDS_ENUM (v)) return g_variant_new_int64 (g_value_get_enum (v));
  if (G_VALUE_HOLDS_FLAGS (v)) return g_variant_new_uint64 (g_value_get_flags (v));
  if (type == G_TYPE_FLOAT)
    { float f = g_value_get_float (v); guint32 bits; std::memcpy (&bits, &f, 4); return g_variant_new_uint32 (bits); }
  if (type == G_TYPE_DOUBLE)
    { double f = g_value_get_double (v); guint64 bits; std::memcpy (&bits, &f, 8); return g_variant_new_uint64 (bits); }
  if (type == G_TYPE_VARIANT) return g_variant_new ("(bv)", g_value_get_variant (v) != nullptr,
                                                   g_value_get_variant (v) ? g_value_get_variant (v) : g_variant_new_byte (0));
  if (type == G_TYPE_STRING) return string_bytes (g_value_get_string (v), cancel);
  if (G_VALUE_HOLDS_OBJECT (v)) return reference (G_OBJECT (g_value_get_object (v)), image, cancel);
  if (type == GIMP_TYPE_CORE_OBJECT_ARRAY)
    {
      auto **objects = static_cast<GObject **> (g_value_get_boxed (v));
      Builder b ("a(uu)");
      for (std::size_t i = 0; objects && objects[i]; ++i)
        { cancelled (cancel); if (i >= 65536) invalid (); g_variant_builder_add_value (&b.value, reference (objects[i], image, cancel)); }
      return g_variant_new ("(b@a(uu))", objects != nullptr, b.end ());
    }
  if (type == GIMP_TYPE_VALUE_ARRAY)
    {
      auto *array = static_cast<GimpValueArray *> (g_value_get_boxed (v));
      return g_variant_new ("(b@a(sv))", array != nullptr, encode (array, image, depth + 1, cancel));
    }
  if (type == G_TYPE_STRV)
    {
      auto **strings = static_cast<gchar **> (g_value_get_boxed (v));
      Builder b ("a(bay)");
      for (std::size_t i = 0; strings && strings[i]; ++i)
        { cancelled (cancel); if (i >= 65536) invalid (); g_variant_builder_add_value (&b.value, string_bytes (strings[i], cancel)); }
      return g_variant_new ("(b@a(bay))", strings != nullptr, b.end ());
    }
  if (type == G_TYPE_BYTES)
    {
      auto *raw = static_cast<GBytes *> (g_value_get_boxed (v));
      return g_variant_new ("(b@ay)", raw != nullptr, GimpPainterXcf::byte_variant (raw));
    }
  if (type == GIMP_TYPE_ARRAY || type == GIMP_TYPE_INT32_ARRAY || type == GIMP_TYPE_DOUBLE_ARRAY)
    {
      auto *array = static_cast<GimpArray *> (g_value_get_boxed (v));
      /* Canonical little-endian components, including IEEE NaN payloads. */
      const std::size_t width = type == GIMP_TYPE_INT32_ARRAY ? 4 : type == GIMP_TYPE_DOUBLE_ARRAY ? 8 : 1;
      return g_variant_new ("(b@ay)", array != nullptr, array_bytes (array, width, cancel));
    }
  invalid ();
}
GVariant *encode (const GimpValueArray *args, GimpImage *image, unsigned depth, GCancellable *cancel)
{
  cancelled (cancel);
  if (depth > 32) invalid ();
  const gint count = args ? gimp_value_array_length (args) : 0;
  if (count > 65536) invalid ();
  Builder b ("a(sv)");
  for (gint i = 0; i < count; ++i)
    {
      cancelled (cancel);
      const GValue *value = gimp_value_array_index (args, i);
      g_variant_builder_add (&b.value, "(sv)", g_type_name (G_VALUE_TYPE (value)), encode_value (value, image, depth, cancel));
    }
  cancelled (cancel);
  return b.end ();
}
void decode_value (GValue *v, GVariant *p, GimpImage *image, unsigned depth, MaterializationBudget& budget)
{
  const GType type = G_VALUE_TYPE (v);
  if (type == G_TYPE_BOOLEAN)
    { if (!g_variant_is_of_type (p, G_VARIANT_TYPE_BOOLEAN)) invalid (); g_value_set_boolean (v, g_variant_get_boolean (p)); return; }
  if (type == G_TYPE_FLOAT)
    { if (!g_variant_is_of_type (p, G_VARIANT_TYPE_UINT32)) invalid (); guint32 bits = g_variant_get_uint32 (p); float f; std::memcpy (&f, &bits, 4); g_value_set_float (v, f); return; }
  if (type == G_TYPE_DOUBLE)
    { if (!g_variant_is_of_type (p, G_VARIANT_TYPE_UINT64)) invalid (); guint64 bits = g_variant_get_uint64 (p); double f; std::memcpy (&f, &bits, 8); g_value_set_double (v, f); return; }
  if (type == G_TYPE_CHAR || type == G_TYPE_INT || type == G_TYPE_LONG || type == G_TYPE_INT64 || G_VALUE_HOLDS_ENUM (v))
    {
      if (!g_variant_is_of_type (p, G_VARIANT_TYPE_INT64)) invalid ();
      const auto n = g_variant_get_int64 (p);
      if (type == G_TYPE_CHAR) { if (n < G_MININT8 || n > G_MAXINT8) invalid (); g_value_set_schar (v, n); }
      else if (type == G_TYPE_INT || G_VALUE_HOLDS_ENUM (v))
        { if (n < G_MININT || n > G_MAXINT) invalid (); if (G_VALUE_HOLDS_ENUM (v)) g_value_set_enum (v, n); else g_value_set_int (v, n); }
      else if (type == G_TYPE_LONG) { if (n < G_MINLONG || n > G_MAXLONG) invalid (); g_value_set_long (v, n); }
      else g_value_set_int64 (v, n);
      return;
    }
  if (type == G_TYPE_UCHAR || type == G_TYPE_UINT || type == G_TYPE_ULONG || type == G_TYPE_UINT64 || G_VALUE_HOLDS_FLAGS (v))
    {
      if (!g_variant_is_of_type (p, G_VARIANT_TYPE_UINT64)) invalid ();
      const auto n = g_variant_get_uint64 (p);
      if (type == G_TYPE_UCHAR) { if (n > G_MAXUINT8) invalid (); g_value_set_uchar (v, n); }
      else if (type == G_TYPE_UINT || G_VALUE_HOLDS_FLAGS (v))
        { if (n > G_MAXUINT) invalid (); if (G_VALUE_HOLDS_FLAGS (v)) g_value_set_flags (v, n); else g_value_set_uint (v, n); }
      else if (type == G_TYPE_ULONG) { if (n > G_MAXULONG) invalid (); g_value_set_ulong (v, n); }
      else g_value_set_uint64 (v, n);
      return;
    }
  if (type == G_TYPE_VARIANT)
    {
      if (!g_variant_is_of_type (p, G_VARIANT_TYPE ("(bv)"))) invalid ();
      gboolean present; GVariant *child; g_variant_get (p, "(bv)", &present, &child); Variant owned (child);
      if (present) g_value_set_variant (v, child);
      return;
    }
  if (type == G_TYPE_STRING) { g_value_take_string (v, read_string (p, budget)); return; }
  if (G_VALUE_HOLDS_OBJECT (v))
    { auto *object = find_reference (p, image); if (object && !g_type_is_a (G_OBJECT_TYPE (object), type)) invalid (); g_value_set_object (v, object); return; }
  if (type == GIMP_TYPE_VALUE_ARRAY)
    {
      if (!g_variant_is_of_type (p, G_VARIANT_TYPE ("(ba(sv))"))) invalid ();
      gboolean present; GVariant *child; g_variant_get (p, "(b@a(sv))", &present, &child); Variant owned (child);
      if (present) g_value_take_boxed (v, decode (child, image, depth + 1, budget));
      return;
    }
  if (type == G_TYPE_STRV)
    {
      if (!g_variant_is_of_type (p, G_VARIANT_TYPE ("(ba(bay))"))) invalid ();
      gboolean present; GVariant *child; g_variant_get (p, "(b@a(bay))", &present, &child); Variant owned (child);
      const gsize count = g_variant_n_children (child); if (count > 65536) invalid ();
      if (!present) return;
      gchar **strings = g_try_new0 (gchar *, count + 1);
      if (!strings) allocation_failed ();
      try { for (gsize i = 0; i < count; ++i) { Variant entry (g_variant_get_child_value (child, i)); strings[i] = read_string (entry.get (), budget); if (!strings[i]) invalid (); } }
      catch (...) { g_strfreev (strings); throw; }
      g_value_take_boxed (v, strings); return;
    }
  if (type == GIMP_TYPE_CORE_OBJECT_ARRAY)
    {
      if (!g_variant_is_of_type (p, G_VARIANT_TYPE ("(ba(uu))"))) invalid ();
      gboolean present; GVariant *child; g_variant_get (p, "(b@a(uu))", &present, &child); Variant owned (child);
      const gsize count = g_variant_n_children (child); if (count > 65536) invalid ();
      if (!present) return;
      std::vector<GObject *> objects;
      for (gsize i = 0; i < count; ++i) { Variant entry (g_variant_get_child_value (child, i)); auto *object = find_reference (entry.get (), image); if (!object) invalid (); objects.push_back (object); }
      objects.push_back (nullptr); g_value_set_boxed (v, objects.data ()); return;
    }
  if (type == G_TYPE_BYTES || type == GIMP_TYPE_ARRAY || type == GIMP_TYPE_INT32_ARRAY || type == GIMP_TYPE_DOUBLE_ARRAY)
    {
      if (!g_variant_is_of_type (p, G_VARIANT_TYPE ("(bay)"))) invalid ();
      gboolean present; GVariant *child; g_variant_get (p, "(b@ay)", &present, &child); Variant owned (child);
      const gsize size = g_variant_get_size (child);
      if (!present) { if (size) invalid (); return; }
      if (type == G_TYPE_BYTES) { g_value_take_boxed (v, g_variant_get_data_as_bytes (child)); return; }
      const std::size_t width = type == GIMP_TYPE_INT32_ARRAY ? 4 : type == GIMP_TYPE_DOUBLE_ARRAY ? 8 : 1;
      auto copy = materialize_array (child, width, budget);
      auto *array = gimp_array_new (copy.get (), size, TRUE);
      if (!array) allocation_failed ();
      array->static_data = FALSE;
      copy.release ();
      g_value_take_boxed (v, array); return;
    }
  invalid ();
}
GimpValueArray *decode (GVariant *variant, GimpImage *image, unsigned depth, MaterializationBudget& budget)
{
  if (depth > 32 || !g_variant_is_of_type (variant, G_VARIANT_TYPE ("a(sv)")) ||
      g_variant_n_children (variant) > 65536) invalid ();
  Args args (gimp_value_array_new (g_variant_n_children (variant)));
  for (gsize i = 0; i < g_variant_n_children (variant); ++i)
    {
      Variant entry (g_variant_get_child_value (variant, i));
      ArgumentEntry decoded (entry.get (), false);
      GType type = g_type_from_name (decoded.type_name ());
      if (!type || !G_TYPE_IS_VALUE_TYPE (type)) invalid ();
      /* Decode directly into an owned slot: appending a populated GValue
       * would perform another unchecked mutable boxed/string allocation. */
      gimp_value_array_append (args.get (), nullptr);
      GValue *value = gimp_value_array_index (args.get (), i);
      g_value_init (value, type);
      decode_value (value, decoded.payload.get (), image, depth, budget);
    }
  return args.release ();
}
}
GVariant *encode_arguments (const GimpValueArray *args, GimpImage *image, GCancellable *cancel) { return g_variant_ref_sink (encode (args, image, 0, cancel)); }
GimpValueArray *decode_arguments (GVariant *variant, GimpImage *image)
{ MaterializationBudget budget; return decode (variant, image, 0, budget); }
}

namespace GimpPainterXcf {
namespace {
struct SnapshotFree { void operator() (GimpFilterArgumentsSnapshot *p) const { if (p) gimp_filter_arguments_snapshot_free (p); } };
using Snapshot = std::unique_ptr<GimpFilterArgumentsSnapshot, SnapshotFree>;
const gchar *safe_image_name (GimpImage *image)
{
  if (!image) return nullptr;
  /* Display strings for remote root URLs can include query/userinfo text.
   * Do not derive a diagnostic image name from any remote file handle. */
  for (GFile *file : {gimp_image_get_file (image), gimp_image_get_imported_file (image),
                     gimp_image_get_exported_file (image)})
    if (file && !g_file_is_native (file)) return nullptr;
  const gchar *name = gimp_image_get_display_name (image);
  /* Only an already-visible basename, never a file URI/path or userinfo. */
  return name && !std::strstr (name, "://") && !std::strchr (name, '/') &&
         !std::strchr (name, '\\') && !std::strchr (name, '@') ? name : nullptr;
}
GVariant *origin_record (GObject *object, GType declared_type, const char *role, GCancellable *cancel)
{
  cancelled (cancel);
  GimpImage *owner = GIMP_IS_IMAGE (object) ? GIMP_IMAGE (object) :
                     GIMP_IS_ITEM (object) ? gimp_item_get_image (GIMP_ITEM (object)) : nullptr;
  if (!GIMP_IS_IMAGE (object) && !GIMP_IS_ITEM (object)) invalid ();
  const gint64 id = GIMP_IS_IMAGE (object) ? gimp_image_get_id (GIMP_IMAGE (object)) : gimp_item_get_id (GIMP_ITEM (object));
  Builder record ("a{sv}");
  g_variant_builder_add (&record.value, "{sv}", "role", g_variant_new_string (role));
  g_variant_builder_add (&record.value, "{sv}", "declared-type", g_variant_new_string (g_type_name (declared_type)));
  g_variant_builder_add (&record.value, "{sv}", "object-type", g_variant_new_string (G_OBJECT_TYPE_NAME (object)));
  g_variant_builder_add (&record.value, "{sv}", "object-runtime-id", g_variant_new_int64 (id));
  g_variant_builder_add (&record.value, "{sv}", "image-runtime-id", g_variant_new_int64 (owner ? gimp_image_get_id (owner) : 0));
  g_variant_builder_add (&record.value, "{sv}", "object-name", string_bytes (GIMP_IS_IMAGE (object) ? safe_image_name (owner) : gimp_object_get_name (object), cancel));
  g_variant_builder_add (&record.value, "{sv}", "image-name", string_bytes (safe_image_name (owner), cancel));
  cancelled (cancel);
  return record.end ();
}
GVariant *encode_reference (GimpFilterArgumentReference ref, GObject *target,
                           GimpImage *image, GHashTable *saved_ids, Builder &external, GCancellable *cancel)
{
  cancelled (cancel);
  guint32 kind = 0, tattoo = 0;
  if (ref.was_set)
    {
      if (ref.expired) kind = 3;
      else if (!target || !g_type_is_a (G_OBJECT_TYPE (target), ref.object_type)) invalid ();
      else if (GIMP_IS_IMAGE (target))
        {
          if (target == G_OBJECT (image)) kind = 1;
          else
            {
              g_variant_builder_add_value (&external.value, origin_record (G_OBJECT (target), ref.object_type, "filter-argument", cancel));
              kind = 3; ref.expired = TRUE;
            }
        }
      else if (GIMP_IS_ITEM (target))
        {
          auto *item = GIMP_ITEM (target);
          if (gimp_item_get_image (item) == image)
            { kind = 2; tattoo = GPOINTER_TO_UINT (g_hash_table_lookup (saved_ids, item)); }
          if (!tattoo)
            {
              g_variant_builder_add_value (&external.value, origin_record (G_OBJECT (item), ref.object_type, "filter-argument", cancel));
              kind = 3; ref.expired = TRUE;
            }
        }
      else invalid ();
    }
  return g_variant_new ("(uuxsbb)", kind, tattoo, ref.id, g_type_name (ref.object_type), ref.was_set, ref.expired);
}
GVariant *encode_snapshot_inner (const GimpFilterArgumentsSnapshot *args, GimpImage *image, GHashTable *saved_ids, unsigned depth, guint &work, Builder &external, GCancellable *cancel)
{
  cancelled (cancel);
  if (depth > 32) invalid ();
  const guint count = args ? gimp_filter_arguments_snapshot_count (args) : 0;
  if (count > 65536 - work) invalid ();
  work += count;
  Builder b ("a(sbv)");
  for (guint i = 0; i < count; ++i)
    {
      cancelled (cancel);
      const GType type = gimp_filter_arguments_snapshot_type (args, i);
      const gboolean is_null = gimp_filter_arguments_snapshot_is_null (args, i);
      GVariant *payload;
      if (g_type_is_a (type, G_TYPE_OBJECT) || type == GIMP_TYPE_CORE_OBJECT_ARRAY)
        {
          Builder refs ("a(uuxsbb)");
          const guint n = gimp_filter_arguments_snapshot_reference_count (args, i);
          if (n > 65536 - work) invalid ();
          work += n;
          for (guint j = 0; j < n; ++j)
            {
              cancelled (cancel);
              GimpFilterArgumentReference ref;
              GObject *target = nullptr;
              if (!gimp_filter_arguments_snapshot_acquire_reference (args, i, j, &ref, &target)) invalid ();
              auto lease = GimpPainter::ObjectRef<GObject>::adopt (target);
              /* Recorded IDs/types are provenance. Only this retained weak
               * target determines file-local identity or external status. */
              g_variant_builder_add_value (&refs.value, encode_reference (ref, lease.get (), image, saved_ids, external, cancel));
            }
          payload = refs.end ();
        }
      else if (type == GIMP_TYPE_VALUE_ARRAY)
        {
          Snapshot nested (gimp_filter_arguments_snapshot_nested (args, i));
          payload = encode_snapshot_inner (nested.get (), image, saved_ids, depth + 1, work, external, cancel);
        }
      else
        {
          const GValue *value = gimp_filter_arguments_snapshot_peek_value (args, i);
          if (!value) invalid ();
          payload = encode_value (value, image, depth, cancel);
        }
      Variant owned_payload (g_variant_ref_sink (payload));
      g_variant_builder_add (&b.value, "(sbv)", g_type_name (type), is_null, owned_payload.get ());
    }
  cancelled (cancel);
  return b.end ();
}
struct Imported
{
  GimpFilterArgumentSpec spec = {};
  Value value;
  std::vector<GimpFilterArgumentReference> references;
  std::vector<GObject *> targets;
  std::vector<std::unique_ptr<Imported>> children;
  std::vector<GimpFilterArgumentSpec> child_specs;
};
std::vector<std::unique_ptr<Imported>> decode_snapshot_inner (GVariant *variant, GimpImage *image, unsigned depth, guint &work, MaterializationBudget& budget)
{
  if (depth > 32 || !g_variant_is_of_type (variant, G_VARIANT_TYPE ("a(sbv)"))) invalid ();
  const gsize count = g_variant_n_children (variant);
  if (count > 65536 - work) invalid ();
  work += count;
  std::vector<std::unique_ptr<Imported>> result;
  for (gsize i = 0; i < count; ++i)
    {
      Variant entry (g_variant_get_child_value (variant, i));
      ArgumentEntry decoded (entry.get (), true);
      GVariant *payload = decoded.payload.get ();
      const GType type = g_type_from_name (decoded.type_name ());
      if (!type || !G_TYPE_IS_VALUE_TYPE (type)) invalid ();
      std::unique_ptr<Imported> node (new Imported);
      node->spec.value_type = type; node->spec.is_null = decoded.is_null;
      if (g_type_is_a (type, G_TYPE_OBJECT) || type == GIMP_TYPE_CORE_OBJECT_ARRAY)
        {
          if (!g_variant_is_of_type (payload, G_VARIANT_TYPE ("a(uuxsbb)"))) invalid ();
          const gsize n = g_variant_n_children (payload);
          if (n > 65536 - work) invalid ();
          work += n;
          for (gsize j = 0; j < n; ++j)
            {
              Variant ref_value (g_variant_get_child_value (payload, j));
              guint32 kind, tattoo; const gchar *object_type;
              GimpFilterArgumentReference ref = {};
              g_variant_get (ref_value.get (), "(uux&sbb)", &kind, &tattoo, &ref.id, &object_type, &ref.was_set, &ref.expired);
              ref.object_type = g_type_from_name (object_type);
              if (!ref.object_type || !g_type_is_a (ref.object_type, G_TYPE_OBJECT)) invalid ();
              GObject *target = nullptr;
              if (!ref.was_set) { if (kind || ref.expired) invalid (); }
              else if (kind == 1 && !ref.expired) target = G_OBJECT (image);
              else if (kind == 2 && !ref.expired)
                {
                  Variant descriptor (g_variant_ref_sink (g_variant_new ("(uu)", kind, tattoo)));
                  try { target = find_reference (descriptor.get (), image); }
                  catch (const std::runtime_error &) { ref.expired = TRUE; }
                }
              else if (kind != 3 || !ref.expired) invalid ();
              if (target && !g_type_is_a (G_OBJECT_TYPE (target), ref.object_type)) invalid ();
              /* File tattoos resolve identities; live runtime descriptors must
               * use the new process's IDs. Expired diagnostic IDs stay exact. */
              if (target) ref.id = GIMP_IS_ITEM (target) ? gimp_item_get_id (GIMP_ITEM (target)) : gimp_image_get_id (GIMP_IMAGE (target));
              node->references.push_back (ref); node->targets.push_back (target);
            }
          node->spec.n_references = n;
          node->spec.references = node->references.data (); node->spec.targets = node->targets.data ();
        }
      else if (type == GIMP_TYPE_VALUE_ARRAY)
        {
          node->children = decode_snapshot_inner (payload, image, depth + 1, work, budget);
          for (const auto &child : node->children) node->child_specs.push_back (child->spec);
          node->spec.n_children = node->child_specs.size (); node->spec.children = node->child_specs.data ();
        }
      else
        {
          g_value_init (&node->value.value, type);
          decode_value (&node->value.value, payload, image, depth, budget);
          node->spec.value = &node->value.value;
        }
      result.push_back (std::move (node));
    }
  return result;
}
}
GVariant *external_reference_origin (GObject *object, GType declared_type, const char *role, GCancellable *cancel)
{ return g_variant_ref_sink (origin_record (object, declared_type, role, cancel)); }
GVariant *encode_snapshot (const GimpFilterArgumentsSnapshot *args, GimpImage *image, GHashTable *saved_ids, GVariant **external_origins, GCancellable *cancel)
{
  if (external_origins) *external_origins = nullptr;
  cancelled (cancel);
  guint work = 0; Builder external ("aa{sv}");
  Variant result (g_variant_ref_sink (encode_snapshot_inner (args, image, saved_ids, 0, work, external, cancel)));
  cancelled (cancel);
  if (external_origins) *external_origins = g_variant_ref_sink (external.end ());
  return result.release ();
}
GimpFilterArgumentsSnapshot *decode_snapshot (GVariant *variant, GimpImage *image)
{
  guint work = 0;
  MaterializationBudget budget;
  auto nodes = decode_snapshot_inner (variant, image, 0, work, budget);
  std::vector<GimpFilterArgumentSpec> specs;
  for (const auto &node : nodes) specs.push_back (node->spec);
  GError *error = nullptr;
  auto *result = gimp_filter_arguments_snapshot_import (specs.size (), specs.data (), &error);
  if (!result) { if (error) g_error_free (error); invalid (); }
  return result;
}
}
