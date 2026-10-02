/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
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
GVariant *bytes (const void *data, gsize length)
{
  if (length > 256u * 1024u * 1024u - 4096u) invalid ();
  return g_variant_new_fixed_array (G_VARIANT_TYPE_BYTE, data, length, 1);
}
GVariant *string_bytes (const gchar *text)
{ return g_variant_new ("(b@ay)", text != nullptr, bytes (text, text ? std::strlen (text) + 1 : 0)); }
gchar *read_string (GVariant *variant)
{
  gboolean present;
  GVariant *raw;
  if (!g_variant_is_of_type (variant, G_VARIANT_TYPE ("(bay)"))) invalid ();
  g_variant_get (variant, "(b@ay)", &present, &raw);
  Variant owned (raw);
  gsize length;
  const auto *data = static_cast<const gchar *> (g_variant_get_fixed_array (raw, &length, 1));
  if (!present) { if (length) invalid (); return nullptr; }
  if (!length || data[length - 1] || std::memchr (data, 0, length - 1)) invalid ();
  return static_cast<gchar *> (g_memdup2 (data, length));
}
GVariant *reference (GObject *object, GimpImage *image)
{
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
GVariant *encode (const GimpValueArray *, GimpImage *, unsigned);
GimpValueArray *decode (GVariant *, GimpImage *, unsigned);
GVariant *encode_value (const GValue *v, GimpImage *image, unsigned depth)
{
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
  if (type == G_TYPE_STRING) return string_bytes (g_value_get_string (v));
  if (G_VALUE_HOLDS_OBJECT (v)) return reference (G_OBJECT (g_value_get_object (v)), image);
  if (type == GIMP_TYPE_CORE_OBJECT_ARRAY)
    {
      auto **objects = static_cast<GObject **> (g_value_get_boxed (v));
      Builder b ("a(uu)");
      for (std::size_t i = 0; objects && objects[i]; ++i)
        { if (i >= 65536) invalid (); g_variant_builder_add_value (&b.value, reference (objects[i], image)); }
      return g_variant_new ("(b@a(uu))", objects != nullptr, b.end ());
    }
  if (type == GIMP_TYPE_VALUE_ARRAY)
    {
      auto *array = static_cast<GimpValueArray *> (g_value_get_boxed (v));
      return g_variant_new ("(b@a(sv))", array != nullptr, encode (array, image, depth + 1));
    }
  if (type == G_TYPE_STRV)
    {
      auto **strings = static_cast<gchar **> (g_value_get_boxed (v));
      Builder b ("a(bay)");
      for (std::size_t i = 0; strings && strings[i]; ++i)
        { if (i >= 65536) invalid (); g_variant_builder_add_value (&b.value, string_bytes (strings[i])); }
      return g_variant_new ("(b@a(bay))", strings != nullptr, b.end ());
    }
  if (type == G_TYPE_BYTES)
    {
      auto *raw = static_cast<GBytes *> (g_value_get_boxed (v));
      gsize length = 0; const void *data = raw ? g_bytes_get_data (raw, &length) : nullptr;
      return g_variant_new ("(b@ay)", raw != nullptr, bytes (data, length));
    }
  if (type == GIMP_TYPE_ARRAY || type == GIMP_TYPE_INT32_ARRAY || type == GIMP_TYPE_DOUBLE_ARRAY)
    {
      auto *array = static_cast<GimpArray *> (g_value_get_boxed (v));
      /* Canonical little-endian components, including IEEE NaN payloads. */
      std::vector<guint8> raw;
      if (array && array->length) raw.assign (array->data, array->data + array->length);
      const std::size_t width = type == GIMP_TYPE_INT32_ARRAY ? 4 : type == GIMP_TYPE_DOUBLE_ARRAY ? 8 : 1;
      if (raw.size () % width) invalid ();
#if G_BYTE_ORDER == G_BIG_ENDIAN
      for (std::size_t i = 0; i < raw.size (); i += width) std::reverse (raw.begin () + i, raw.begin () + i + width);
#endif
      return g_variant_new ("(b@ay)", array != nullptr, bytes (raw.data (), raw.size ()));
    }
  invalid ();
}
GVariant *encode (const GimpValueArray *args, GimpImage *image, unsigned depth)
{
  if (depth > 32) invalid ();
  const gint count = args ? gimp_value_array_length (args) : 0;
  if (count > 65536) invalid ();
  Builder b ("a(sv)");
  for (gint i = 0; i < count; ++i)
    {
      const GValue *value = gimp_value_array_index (args, i);
      g_variant_builder_add (&b.value, "(sv)", g_type_name (G_VALUE_TYPE (value)), encode_value (value, image, depth));
    }
  return b.end ();
}
void decode_value (GValue *v, GVariant *p, GimpImage *image, unsigned depth)
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
  if (type == G_TYPE_STRING) { g_value_take_string (v, read_string (p)); return; }
  if (G_VALUE_HOLDS_OBJECT (v))
    { auto *object = find_reference (p, image); if (object && !g_type_is_a (G_OBJECT_TYPE (object), type)) invalid (); g_value_set_object (v, object); return; }
  if (type == GIMP_TYPE_VALUE_ARRAY)
    {
      if (!g_variant_is_of_type (p, G_VARIANT_TYPE ("(ba(sv))"))) invalid ();
      gboolean present; GVariant *child; g_variant_get (p, "(b@a(sv))", &present, &child); Variant owned (child);
      if (present) g_value_take_boxed (v, decode (child, image, depth + 1));
      return;
    }
  if (type == G_TYPE_STRV)
    {
      if (!g_variant_is_of_type (p, G_VARIANT_TYPE ("(ba(bay))"))) invalid ();
      gboolean present; GVariant *child; g_variant_get (p, "(b@a(bay))", &present, &child); Variant owned (child);
      const gsize count = g_variant_n_children (child); if (count > 65536) invalid ();
      if (!present) return;
      gchar **strings = g_new0 (gchar *, count + 1);
      try { for (gsize i = 0; i < count; ++i) { Variant entry (g_variant_get_child_value (child, i)); strings[i] = read_string (entry.get ()); if (!strings[i]) invalid (); } }
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
      gsize size; const auto *data = static_cast<const guint8 *> (g_variant_get_fixed_array (child, &size, 1));
      if (!present) { if (size) invalid (); return; }
      if (type == G_TYPE_BYTES) { g_value_take_boxed (v, g_bytes_new (data, size)); return; }
      const std::size_t width = type == GIMP_TYPE_INT32_ARRAY ? 4 : type == GIMP_TYPE_DOUBLE_ARRAY ? 8 : 1;
      if (size % width) invalid ();
      std::vector<guint8> copy; if (size) copy.assign (data, data + size);
#if G_BYTE_ORDER == G_BIG_ENDIAN
      for (std::size_t i = 0; i < size; i += width) std::reverse (copy.begin () + i, copy.begin () + i + width);
#endif
      g_value_take_boxed (v, gimp_array_new (copy.data (), size, FALSE)); return;
    }
  invalid ();
}
GimpValueArray *decode (GVariant *variant, GimpImage *image, unsigned depth)
{
  if (depth > 32 || !g_variant_is_of_type (variant, G_VARIANT_TYPE ("a(sv)")) ||
      g_variant_n_children (variant) > 65536) invalid ();
  Args args (gimp_value_array_new (g_variant_n_children (variant)));
  for (gsize i = 0; i < g_variant_n_children (variant); ++i)
    {
      Variant entry (g_variant_get_child_value (variant, i));
      const gchar *type_name; GVariant *payload;
      g_variant_get (entry.get (), "(&sv)", &type_name, &payload); Variant owned (payload);
      GType type = g_type_from_name (type_name);
      if (!type || !G_TYPE_IS_VALUE_TYPE (type)) invalid ();
      Value value (type); decode_value (&value.value, payload, image, depth);
      gimp_value_array_append (args.get (), &value.value);
    }
  return args.release ();
}
}
GVariant *encode_arguments (const GimpValueArray *args, GimpImage *image) { return g_variant_ref_sink (encode (args, image, 0)); }
GimpValueArray *decode_arguments (GVariant *variant, GimpImage *image) { return decode (variant, image, 0); }
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
GVariant *origin_record (GObject *object, GType declared_type, const char *role)
{
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
  g_variant_builder_add (&record.value, "{sv}", "object-name", string_bytes (GIMP_IS_IMAGE (object) ? safe_image_name (owner) : gimp_object_get_name (object)));
  g_variant_builder_add (&record.value, "{sv}", "image-name", string_bytes (safe_image_name (owner)));
  return record.end ();
}
GVariant *encode_reference (GimpFilterArgumentReference ref, GimpImage *image, GHashTable *saved_ids, Builder &external)
{
  guint32 kind = 0, tattoo = 0;
  if (ref.was_set)
    {
      if (ref.expired) kind = 3;
      else if (g_type_is_a (ref.object_type, GIMP_TYPE_IMAGE))
        {
          if (ref.id <= 0 || ref.id > G_MAXINT) invalid ();
          auto *target = gimp_image_get_by_id (image->gimp, ref.id);
          if (!target) invalid ();
          if (target == image) kind = 1;
          else
            {
              g_variant_builder_add_value (&external.value, origin_record (G_OBJECT (target), ref.object_type, "filter-argument"));
              kind = 3; ref.expired = TRUE;
            }
        }
      else if (g_type_is_a (ref.object_type, GIMP_TYPE_ITEM))
        {
          if (ref.id <= 0 || ref.id > G_MAXINT) invalid ();
          auto *item = gimp_item_get_by_id (image->gimp, ref.id);
          if (!item) invalid ();
          if (gimp_item_get_image (item) == image)
            { kind = 2; tattoo = GPOINTER_TO_UINT (g_hash_table_lookup (saved_ids, item)); }
          if (!tattoo)
            {
              g_variant_builder_add_value (&external.value, origin_record (G_OBJECT (item), ref.object_type, "filter-argument"));
              kind = 3; ref.expired = TRUE;
            }
        }
      else invalid ();
    }
  return g_variant_new ("(uuxsbb)", kind, tattoo, ref.id, g_type_name (ref.object_type), ref.was_set, ref.expired);
}
GVariant *encode_snapshot_inner (const GimpFilterArgumentsSnapshot *args, GimpImage *image, GHashTable *saved_ids, unsigned depth, guint &work, Builder &external)
{
  if (depth > 32) invalid ();
  const guint count = args ? gimp_filter_arguments_snapshot_count (args) : 0;
  if (count > 65536 - work) invalid ();
  work += count;
  Builder b ("a(sbv)");
  for (guint i = 0; i < count; ++i)
    {
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
              GimpFilterArgumentReference ref;
              if (!gimp_filter_arguments_snapshot_reference (args, i, j, &ref)) invalid ();
              g_variant_builder_add_value (&refs.value, encode_reference (ref, image, saved_ids, external));
            }
          payload = refs.end ();
        }
      else if (type == GIMP_TYPE_VALUE_ARRAY)
        {
          Snapshot nested (gimp_filter_arguments_snapshot_nested (args, i));
          payload = encode_snapshot_inner (nested.get (), image, saved_ids, depth + 1, work, external);
        }
      else
        {
          Value value;
          if (!gimp_filter_arguments_snapshot_value (args, i, &value.value)) invalid ();
          payload = encode_value (&value.value, image, depth);
        }
      Variant owned_payload (g_variant_ref_sink (payload));
      g_variant_builder_add (&b.value, "(sbv)", g_type_name (type), is_null, owned_payload.get ());
    }
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
std::vector<std::unique_ptr<Imported>> decode_snapshot_inner (GVariant *variant, GimpImage *image, unsigned depth, guint &work)
{
  if (depth > 32 || !g_variant_is_of_type (variant, G_VARIANT_TYPE ("a(sbv)"))) invalid ();
  const gsize count = g_variant_n_children (variant);
  if (count > 65536 - work) invalid ();
  work += count;
  std::vector<std::unique_ptr<Imported>> result;
  for (gsize i = 0; i < count; ++i)
    {
      Variant entry (g_variant_get_child_value (variant, i));
      const gchar *name; gboolean is_null; GVariant *payload;
      g_variant_get (entry.get (), "(&sbv)", &name, &is_null, &payload); Variant owned (payload);
      const GType type = g_type_from_name (name);
      if (!type || !G_TYPE_IS_VALUE_TYPE (type)) invalid ();
      std::unique_ptr<Imported> node (new Imported);
      node->spec.value_type = type; node->spec.is_null = is_null;
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
          node->children = decode_snapshot_inner (payload, image, depth + 1, work);
          for (const auto &child : node->children) node->child_specs.push_back (child->spec);
          node->spec.n_children = node->child_specs.size (); node->spec.children = node->child_specs.data ();
        }
      else
        {
          g_value_init (&node->value.value, type);
          decode_value (&node->value.value, payload, image, depth);
          node->spec.value = &node->value.value;
        }
      result.push_back (std::move (node));
    }
  return result;
}
}
GVariant *external_reference_origin (GObject *object, GType declared_type, const char *role)
{ return g_variant_ref_sink (origin_record (object, declared_type, role)); }
GVariant *encode_snapshot (const GimpFilterArgumentsSnapshot *args, GimpImage *image, GHashTable *saved_ids, GVariant **external_origins)
{
  if (external_origins) *external_origins = nullptr;
  guint work = 0; Builder external ("aa{sv}");
  Variant result (g_variant_ref_sink (encode_snapshot_inner (args, image, saved_ids, 0, work, external)));
  if (external_origins) *external_origins = g_variant_ref_sink (external.end ());
  return result.release ();
}
GimpFilterArgumentsSnapshot *decode_snapshot (GVariant *variant, GimpImage *image)
{
  guint work = 0;
  auto nodes = decode_snapshot_inner (variant, image, 0, work);
  std::vector<GimpFilterArgumentSpec> specs;
  for (const auto &node : nodes) specs.push_back (node->spec);
  GError *error = nullptr;
  auto *result = gimp_filter_arguments_snapshot_import (specs.size (), specs.data (), &error);
  if (!result) { if (error) g_error_free (error); invalid (); }
  return result;
}
}
