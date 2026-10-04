/* SPDX-License-Identifier: GPL-3.0-or-later
 * Versioned, noncolliding Painter metadata over standard XCF. All definitions
 * are prepared before opening the destination. Pixel buffers are snapshots of
 * committed caches, never an in-flight Filter worker's output.
 */
#include "config.h"
#include <gegl.h>
#include <gio/gio.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
extern "C" {
#include "libgimpbase/gimpbase.h"
#include "core/core-types.h"
#include "core/gimp.h"
#include "core/gimpimage.h"
#include "core/gimpimage-undo.h"
#include "core/gimpselection.h"
#include "core/gimpitem.h"
#include "core/gimpitemlist.h"
#include "core/gimp-painter-provenance.h"
#include "core/gimpcontainer.h"
#include "core/gimpdrawable-filters.h"
#include "core/gimpdrawablefilter.h"
#include "core/gimplayer.h"
#include "core/gimplayermask.h"
#include "core/gimpclonelayer.h"
#include "core/gimpfilterlayer.h"
#include "core/gimpgrouplayer.h"
#include "core/gimpparasitelist.h"
#include "operations/layer-modes-legacy/gimpoperationpainterlegacy.h"
#include "xcf-private.h"
#include "xcf-write.h"
#include "painter-xcf-load.h"
#include "painter-xcf-preserve.h"
}
#include "painter-xcf-arguments.hpp"
#include "painter/object-ref.hpp"
#include "painter/connection.hpp"
#include "painter/resources.hpp"
#include "painter-xcf-storage.hpp"
#include "painter-xcf-multipart.hpp"
#include <algorithm>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
namespace {
const char image_name[] = "gimp-painter-image";
const char item_name[] = "gimp-painter-item";
const char origin_name[] = "gimp-painter-origin";
const guint8 magic[12] = {'G','P','X','C','F',0,0,0, 1,0,0,0};
constexpr gsize max_blob = 256u * 1024u * 1024u - 4096u;
struct ParasiteFree { void operator() (GimpParasite *p) const { if (p) gimp_parasite_free (p); } };
struct VariantFree { void operator() (GVariant *p) const { if (p) g_variant_unref (p); } };
struct RecordsFree { void operator() (GPtrArray *p) const { if (p) g_ptr_array_unref (p); } };
using Records = std::unique_ptr<GPtrArray, RecordsFree>;
using GimpPainter::String;
struct BytesFree { void operator() (GBytes *p) const { if (p) g_bytes_unref (p); } };
struct ArgsFree { void operator() (GimpValueArray *p) const { if (p) gimp_value_array_unref (p); } };
struct CloneFree { void operator() (GimpCloneLayerReference *p) const { if (p) gimp_clone_layer_reference_free (p); } };
using Parasite = std::unique_ptr<GimpParasite, ParasiteFree>;
using Variant = std::unique_ptr<GVariant, VariantFree>;
using Bytes = std::unique_ptr<GBytes, BytesFree>;
struct TransportFree { void operator() (GimpPainterProvenanceTransport *p) const { gimp_painter_provenance_free_transport (p); } };
using Transport = std::unique_ptr<GimpPainterProvenanceTransport, TransportFree>;
struct Capsule
{
  GimpPainter::Bytes bytes;
  guint32 flags = GIMP_PARASITE_PERSISTENT;
  bool occupied = false;
  bool from_transport = false;
  bool regenerated = false;
  GBytes *get () const { return bytes.get (); }
  explicit operator bool () const { return occupied; }
};
Capsule owner_capsule (GObject *owner, GimpPainterProvenanceNamespace space)
{
  Transport transport (gimp_painter_provenance_ref_transport (owner));
  if (transport && (transport->present[space] || transport->invalid[space]))
    return {GimpPainter::Bytes::retain (transport->capsules[space]), GIMP_PARASITE_PERSISTENT, true, true, false};
  const char *name = GimpPainterXcf::namespace_name (GimpPainterXcf::Namespace (space));
  const auto *parasite = GIMP_IS_IMAGE (owner) ? gimp_image_parasite_find (GIMP_IMAGE (owner), name) :
                                             gimp_item_parasite_find (GIMP_ITEM (owner), name);
  if (!parasite) return {};
  guint32 size; const void *data = gimp_parasite_get_data (parasite, &size);
  return {GimpPainter::Bytes::adopt (g_bytes_new (data, size)), guint32 (gimp_parasite_get_flags (parasite)), true};
}
using Object = GimpPainter::ObjectRef<GObject>;
using GimpPainter::Connection;
using Args = std::unique_ptr<GimpValueArray, ArgsFree>;
struct SnapshotFree { void operator() (GimpFilterArgumentsSnapshot *p) const { if (p) gimp_filter_arguments_snapshot_free (p); } };
using Snapshot = std::unique_ptr<GimpFilterArgumentsSnapshot, SnapshotFree>;
using Clone = std::unique_ptr<GimpCloneLayerReference, CloneFree>;
[[noreturn]] void fail (const char *message) { throw std::runtime_error (message); }
GVariant *byte_array (const void *data, gsize length, GCancellable *cancel = nullptr)
{
  if (length <= 65536) return g_variant_new_fixed_array (G_VARIANT_TYPE_BYTE, data, length, 1);
  auto *input = g_memory_input_stream_new_from_data (data, length, nullptr);
  std::unique_ptr<GInputStream, decltype (&g_object_unref)> held (input, g_object_unref);
  auto snapshot = GimpPainterXcf::snapshot_stream (input, cancel);
  return GimpPainterXcf::byte_variant (snapshot.get ());
}
GVariant *byte_array (GBytes *raw)
{
  return GimpPainterXcf::byte_variant (raw);
}
GBytes *variant_bytes (GVariant *value)
{
  if (!value || !g_variant_is_of_type (value, G_VARIANT_TYPE_BYTESTRING)) fail ("Invalid Painter byte field");
  return g_variant_get_data_as_bytes (value);
}
Variant decode (GBytes *capsule)
{
  if (!capsule) return {};
  gsize length = 0;
  auto *data = static_cast<const guint8 *> (g_bytes_get_data (capsule, &length));
  if (length < sizeof magic || std::memcmp (data, magic, sizeof magic)) return {};
  Bytes raw (g_bytes_new_from_bytes (capsule, sizeof magic, length - sizeof magic));
  Variant value (g_variant_ref_sink (g_variant_new_from_bytes (G_VARIANT_TYPE_VARDICT, raw.get (), FALSE)));
  if (!g_variant_is_normal_form (value.get ())) fail ("Malformed Painter metadata; original record is retained");
#if G_BYTE_ORDER == G_BIG_ENDIAN
  value.reset (g_variant_byteswap (value.get ()));
#endif
  /* Normal form permits duplicate a{sv} keys. Lookup sees the first while
   * GVariantDict would overwrite it with the last, so never reinterpret or
   * rewrite an ambiguous capsule, including duplicated unknown fields. */
  const gsize count = g_variant_n_children (value.get ());
  if (count > 65536) fail ("Painter metadata exceeds the field-work limit; original capsule is retained");
  std::unordered_set<std::string> keys;
  for (gsize i = 0; i < count; ++i)
    {
      Variant entry (g_variant_get_child_value (value.get (), i));
      Variant key (g_variant_get_child_value (entry.get (), 0));
      if (!keys.emplace (g_variant_get_string (key.get (), nullptr)).second)
        fail ("Duplicate Painter metadata key; original capsule is retained and cannot be safely rewritten");
    }
  guint32 version;
  if (!g_variant_lookup (value.get (), "version", "u", &version) || version != 1) return {};
  GimpPainterXcf::discard_snapshot_pages (data, length);
  return value;
}
Variant decode (const GimpParasite *parasite)
{
  if (!parasite) return {};
  guint32 size; const void *data = gimp_parasite_get_data (parasite, &size);
  Bytes bytes (g_bytes_new (data, size)); return decode (bytes.get ());
}
Capsule encode (const gchar *, GVariant *dict, GCancellable *cancel)
{
  Variant value (g_variant_ref_sink (dict));
  return {GimpPainterXcf::snapshot_variant (magic, sizeof magic, value.get (), cancel), GIMP_PARASITE_PERSISTENT, true, false, true};
}
struct Dictionary
{
  GVariantDict dict;
  bool ended = false;
  explicit Dictionary (GVariant *base = nullptr) { g_variant_dict_init (&dict, base); }
  ~Dictionary () { if (!ended) g_variant_dict_clear (&dict); }
  void put (const char *key, GVariant *value) { g_variant_dict_insert_value (&dict, key, value); }
  GVariant *end ()
  {
    ended = true;
    Variant unordered (g_variant_ref_sink (g_variant_dict_end (&dict)));
    std::vector<Variant> entries;
    const gsize count = g_variant_n_children (unordered.get ());
    entries.reserve (count);
    for (gsize i = 0; i < count; ++i) entries.emplace_back (g_variant_get_child_value (unordered.get (), i));
    /* GVariantDict hash iteration can change container padding after reload.
     * Unique dictionary keys have no order semantics; stable ordering avoids
     * size drift without materializing unknown field payloads. */
    std::sort (entries.begin (), entries.end (), [] (const Variant& a, const Variant& b) {
      Variant ka (g_variant_get_child_value (a.get (), 0)), kb (g_variant_get_child_value (b.get (), 0));
      return std::strcmp (g_variant_get_string (ka.get (), nullptr), g_variant_get_string (kb.get (), nullptr)) < 0;
    });
    GVariantBuilder ordered; g_variant_builder_init (&ordered, G_VARIANT_TYPE_VARDICT);
    for (const auto& entry : entries) g_variant_builder_add_value (&ordered, entry.get ());
    return g_variant_builder_end (&ordered);
  }
};
void nullable_string (Dictionary &dict, const char *key, const char *value, GCancellable *cancel = nullptr)
{
  if (value) dict.put (key, byte_array (value, std::strlen (value) + 1, cancel));
  else g_variant_dict_remove (&dict.dict, key);
}
gchar *read_string (GVariant *dict, const char *key)
{
  Variant value (g_variant_lookup_value (dict, key, G_VARIANT_TYPE_BYTESTRING));
  if (!value) return nullptr;
  gsize size;
  const auto *data = static_cast<const gchar *> (g_variant_get_fixed_array (value.get (), &size, 1));
  if (size > max_blob) fail ("Painter native name exceeds the legacy materialization limit; original capsule is retained");
  if (!size || data[size - 1] || std::memchr (data, 0, size - 1)) fail ("Malformed Painter name");
  auto *copy = static_cast<gchar *> (g_try_malloc (size));
  if (!copy) throw std::bad_alloc ();
  std::memcpy (copy, data, size); return copy;
}
const char external_field[] = "external-reference-origins";
Bytes little_endian_bytes (GVariant *value, GCancellable *cancel = nullptr)
{ return Bytes (GimpPainterXcf::snapshot_variant (nullptr, 0, value, cancel).release ()); }
bool equal_snapshot_bytes (const void *, const void *, gsize, GCancellable *);
std::string snapshot_digest (GBytes *bytes, GCancellable *cancel)
{
  std::unique_ptr<GChecksum, decltype (&g_checksum_free)> sum (g_checksum_new (G_CHECKSUM_SHA256), g_checksum_free);
  gsize size; const auto *data = static_cast<const guint8 *> (g_bytes_get_data (bytes, &size));
  while (size)
    {
      if (cancel && g_cancellable_is_cancelled (cancel))
        throw GimpPainterXcf::StorageError (G_IO_ERROR, G_IO_ERROR_CANCELLED, "Painter archive hashing was cancelled");
      const gsize part = std::min (size, gsize (65536));
      g_checksum_update (sum.get (), data, part);
      GimpPainterXcf::discard_snapshot_pages (data, part);
      data += part; size -= part;
    }
  return g_checksum_get_string (sum.get ());
}
void merge_external_origins (Dictionary &dict, GObject *object, GVariant *incoming, GCancellable *cancel)
{
  Variant existing (g_variant_dict_lookup_value (&dict.dict, external_field, G_VARIANT_TYPE ("aa{sv}")));
  if (!existing && g_variant_dict_contains (&dict.dict, external_field)) fail ("Invalid external reference provenance");
  Variant cached;
  if (auto bytes = Bytes (gimp_painter_provenance_ref_bytes (object, GIMP_PAINTER_PROVENANCE_EXTERNAL)))
    {
      cached.reset (g_variant_ref_sink (g_variant_new_from_bytes (G_VARIANT_TYPE ("aa{sv}"), bytes.get (), FALSE)));
      if (!g_variant_is_normal_form (cached.get ())) fail ("Invalid retained external reference provenance");
#if G_BYTE_ORDER == G_BIG_ENDIAN
      cached.reset (g_variant_byteswap (cached.get ()));
#endif
    }
  std::vector<Variant> records;
  std::vector<Bytes> record_bytes;
  std::unordered_map<std::string, std::vector<gsize>> fingerprints;
  guint64 retained_size = 0;
  for (GVariant *source : {existing.get (), cached.get (), incoming})
    if (source)
      {
        if (!g_variant_is_of_type (source, G_VARIANT_TYPE ("aa{sv}"))) fail ("Invalid external reference provenance type");
        const gsize count = g_variant_n_children (source);
        if (count > 65536) fail ("Too many external reference records");
        for (gsize i = 0; i < count; ++i)
          {
            Variant record (g_variant_get_child_value (source, i));
            Bytes raw = little_endian_bytes (record.get (), cancel);
            auto &bucket = fingerprints[snapshot_digest (raw.get (), cancel)];
            bool seen = false;
            for (gsize index : bucket)
              if (g_bytes_get_size (record_bytes[index].get ()) == g_bytes_get_size (raw.get ()) &&
                  equal_snapshot_bytes (g_bytes_get_data (record_bytes[index].get (), nullptr),
                                        g_bytes_get_data (raw.get (), nullptr), g_bytes_get_size (raw.get ()), cancel))
                { seen = true; break; }
            if (!seen)
              {
                const gsize size = g_bytes_get_size (raw.get ());
                if (records.size () >= 65536 || guint64 (size) > GimpPainterXcf::StorageLimits ().bytes - retained_size) fail ("External reference archive exceeds its serialization limits");
                retained_size += size;
                records.emplace_back (std::move (record));
                record_bytes.emplace_back (std::move (raw));
                bucket.push_back (records.size () - 1);
              }
          }
      }
  if (!records.empty ())
    {
      GVariantBuilder array; g_variant_builder_init (&array, G_VARIANT_TYPE ("aa{sv}"));
      for (const auto &record : records) g_variant_builder_add_value (&array, record.get ());
      dict.put (external_field, g_variant_builder_end (&array));
    }
}
bool equal_snapshot_bytes (const void *first, const void *second, gsize size,
                           GCancellable *cancel)
{
  const auto *a = static_cast<const guint8 *> (first);
  const auto *b = static_cast<const guint8 *> (second);
  while (size)
    {
      if (cancel && g_cancellable_is_cancelled (cancel))
        throw GimpPainterXcf::StorageError (G_IO_ERROR, G_IO_ERROR_CANCELLED, "Painter archive comparison was cancelled");
      const gsize part = std::min (size, gsize (65536));
      const bool equal = !std::memcmp (a, b, part);
      GimpPainterXcf::discard_snapshot_pages (a, part);
      GimpPainterXcf::discard_snapshot_pages (b, part);
      if (!equal) return false;
      a += part; b += part; size -= part;
    }
  return true;
}
void preserve_records (Dictionary &dict, GObject *object, GCancellable *cancel)
{
  merge_external_origins (dict, object, nullptr, cancel);
  if (!g_variant_dict_contains (&dict.dict, "original-owner"))
    if (auto type = String (gimp_painter_provenance_dup_text (object, GIMP_PAINTER_PROVENANCE_TYPE)))
      dict.put ("original-owner", g_variant_new_string (type.get ()));
  if (!g_variant_dict_contains (&dict.dict, "original-header"))
    if (auto raw = Bytes (gimp_painter_provenance_ref_bytes (object, GIMP_PAINTER_PROVENANCE_HEADER)))
      dict.put ("original-header", byte_array (raw.get ()));
  /* An existing archive is retained, not replaced with the newly generated
   * standard record containing that archive (which would grow recursively). */
  if (!g_variant_dict_contains (&dict.dict, "original-properties"))
    if (auto raw = Bytes (gimp_painter_provenance_ref_bytes (object, GIMP_PAINTER_PROVENANCE_PROPERTIES)))
      dict.put ("original-properties", byte_array (raw.get ()));
  Records unknown (gimp_painter_provenance_ref_records (object));
  if (unknown && unknown->len)
    {
      std::vector<GimpPainter::Bytes> parts;
      for (guint i = 0; i < unknown->len; ++i)
        parts.push_back (GimpPainter::Bytes::retain (static_cast<GBytes *> (g_ptr_array_index (unknown.get (), i))));
      auto current = GimpPainterXcf::snapshot_parts (parts, 0, cancel);
      const gsize size = g_bytes_get_size (current.get ());
      Variant old (g_variant_dict_lookup_value (&dict.dict, "opaque-record-sequences", G_VARIANT_TYPE ("aay")));
      if (!old && g_variant_dict_contains (&dict.dict, "opaque-record-sequences")) fail ("Invalid opaque property archive");
      GVariantBuilder history; g_variant_builder_init (&history, G_VARIANT_TYPE ("aay"));
      bool seen = false;
      if (old)
        for (gsize i = 0; i < g_variant_n_children (old.get ()); ++i)
          {
            Variant sequence (g_variant_get_child_value (old.get (), i)); gsize length;
            const void *data = g_variant_get_fixed_array (sequence.get (), &length, 1);
            seen = seen || (length == size && equal_snapshot_bytes (data, g_bytes_get_data (current.get (), nullptr), size, cancel));
            g_variant_builder_add_value (&history, sequence.get ());
          }
      if (!seen) g_variant_builder_add_value (&history, byte_array (current.get ()));
      dict.put ("opaque-record-sequences", g_variant_builder_end (&history));
    }
}
/* The core owns this uninterpreted current model without knowing XCF. A
 * nullable variant preserves an absent field as distinct from any field value. */
Bytes pack_opaque_arguments (GVariant *value)
{
  Variant wrapper (g_variant_ref_sink (g_variant_new_maybe (G_VARIANT_TYPE_VARIANT,
                                      value ? g_variant_new_variant (value) : nullptr)));
  return little_endian_bytes (wrapper.get ());
}
Variant unpack_opaque_arguments (GBytes *bytes)
{
  Variant wrapper (g_variant_ref_sink (g_variant_new_from_bytes (G_VARIANT_TYPE ("mv"), bytes, FALSE)));
  if (!g_variant_is_normal_form (wrapper.get ())) fail ("Malformed opaque argument model; destination was not changed");
#if G_BYTE_ORDER == G_BIG_ENDIAN
  wrapper.reset (g_variant_byteswap (wrapper.get ()));
#endif
  if (!g_variant_n_children (wrapper.get ())) return {};
  Variant child (g_variant_get_child_value (wrapper.get (), 0));
  return Variant (g_variant_get_variant (child.get ()));
}
bool known_item_kind (GVariant *value)
{
  const gchar *kind = nullptr;
  return value && g_variant_lookup (value, "kind", "&s", &kind) &&
         (!std::strcmp (kind, "ordinary") || !std::strcmp (kind, "clone") || !std::strcmp (kind, "filter"));
}
void validate_optional_name (GVariant *dict, const gchar *key)
{
  Variant name (g_variant_lookup_value (dict, key, nullptr));
  if (!name) return;
  if (!g_variant_is_of_type (name.get (), G_VARIANT_TYPE_BYTESTRING))
    fail ("Invalid Painter name type; original capsule remains inert");
  gsize size;
  const auto *data = static_cast<const gchar *> (g_variant_get_fixed_array (name.get (), &size, 1));
  if (size > max_blob) fail ("Painter native name exceeds the legacy materialization limit; original capsule remains inert");
  if (!size || data[size - 1] || std::memchr (data, 0, size - 1))
    fail ("Invalid Painter name encoding; original capsule remains inert");
}
void validate_native_names (GVariant *dict, std::initializer_list<const char *> names)
{
  gsize bytes = 0;
  for (const auto *key : names)
    {
      validate_optional_name (dict, key);
      Variant value (g_variant_lookup_value (dict, key, G_VARIANT_TYPE_BYTESTRING));
      if (!value) continue;
      const gsize size = g_variant_get_size (value.get ());
      if (size > max_blob - bytes) fail ("Painter native names exceed the legacy materialization limit; original capsule remains inert");
      bytes += size;
    }
}
bool item_legacy_mode (GVariant *dict, GimpLayerMode *mode)
{
  Variant raw (g_variant_lookup_value (dict, "legacy-mode", nullptr));
  if (!raw) return false;
  if (!g_variant_is_of_type (raw.get (), G_VARIANT_TYPE_UINT32) ||
      !gimp_painter_layer_mode_from_legacy (g_variant_get_uint32 (raw.get ()), mode))
    fail ("Invalid Painter legacy mode; original capsule remains inert");
  return true;
}
void validate_common_item_record (GVariant *dict)
{
  GimpLayerMode mode;
  item_legacy_mode (dict, &mode);
  validate_optional_name (dict, "original-source-name");
}
GimpFilterLayerSnapshot filter_cache_state (GVariant *dict)
{
  GimpFilterLayerSnapshot state = {}; state.version = 1;
  guint32 saved_state;
  if (!g_variant_lookup (dict, "generation", "t", &state.generation) ||
      !g_variant_lookup (dict, "cache-generation", "t", &state.cache_generation) ||
      !g_variant_lookup (dict, "cache-complete", "b", &state.cache_complete) ||
      !g_variant_lookup (dict, "saved-state", "u", &saved_state) ||
      saved_state > GIMP_FILTER_LAYER_CLOSED ||
      state.generation > G_MAXINT64 || state.cache_generation > state.generation)
    fail ("Invalid Filter cache state; original capsule remains inert");
  state.state = static_cast<GimpFilterLayerState> (saved_state);
  return state;
}
void validate_filter_record (GVariant *dict)
{
  filter_cache_state (dict);
  gboolean has_definition, has_arguments;
  if (!g_variant_lookup (dict, "has-definition", "b", &has_definition) ||
      !g_variant_lookup (dict, "has-arguments", "b", &has_arguments))
    fail ("Invalid Filter definition presence flags; original capsule remains inert");
  if (has_definition)
    {
      Variant definition (g_variant_lookup_value (dict, "definition", G_VARIANT_TYPE_BYTESTRING));
      if (!definition) fail ("Invalid Filter definition bytes; original capsule remains inert");
    }
  validate_native_names (dict, {"original-source-name", "procedure"});
  /* An unreadable active argument model has a separate lossless opaque path.
   * Do not reject or silently replace it with an empty converted model here.
   */
}
void validate_clone_record (GVariant *dict)
{
  guint32 id, state;
  gboolean expired, allow_lookup;
  if (!g_variant_lookup (dict, "source-id", "u", &id) ||
      !g_variant_lookup (dict, "source-state", "u", &state) ||
      !g_variant_lookup (dict, "source-expired", "b", &expired) ||
      !g_variant_lookup (dict, "allow-name-lookup", "b", &allow_lookup) ||
      state > GIMP_CLONE_SOURCE_EXPIRED)
    fail ("Invalid Clone reference fields; original capsule remains inert");
  validate_native_names (dict, {"original-source-name", "pending-name", "source-name"});
  Variant pending (g_variant_lookup_value (dict, "pending-name", G_VARIANT_TYPE_BYTESTRING));
  const guint32 expected = pending ? GIMP_CLONE_SOURCE_PENDING :
    id && !expired ? GIMP_CLONE_SOURCE_LIVE : expired ? GIMP_CLONE_SOURCE_EXPIRED : GIMP_CLONE_SOURCE_NONE;
  if (state != expected)
    fail ("Inconsistent Clone reference state; original capsule remains inert");
  /* A missing target is not malformed: after topology restoration it becomes
   * explicitly expired, retaining the original ID without guessed rebinding.
   * Names are optional; an explicitly empty pending name is still pending.
   */
}
bool supported_item_record (GVariant *value, GimpItem *owner)
{
  if (!known_item_kind (value)) return false;
  const gchar *kind = nullptr;
  g_variant_lookup (value, "kind", "&s", &kind);
  try
    {
      validate_common_item_record (value);
      const bool custom = std::strcmp (kind, "ordinary") != 0;
      if (custom && (!GIMP_IS_LAYER (owner) || GIMP_IS_GROUP_LAYER (owner) || g_type_is_a (G_OBJECT_TYPE (owner), g_type_from_name ("GimpTextLayer")) ||
                     gimp_item_parasite_find (owner, "gimp-text-layer") ||
                     gimp_item_parasite_find (owner, "plug-in-gdyntext/data")))
        return false;
      Variant mode (g_variant_lookup_value (value, "legacy-mode", nullptr));
      if (mode && !GIMP_IS_LAYER (owner)) return false;
      if (!std::strcmp (kind, "filter")) validate_filter_record (value);
      if (!std::strcmp (kind, "clone")) validate_clone_record (value);
    }
  catch (const std::runtime_error &) { return false; }
  return true;
}
Capsule fallback_origin (GObject *object, GCancellable *cancel)
{
  auto semantic = owner_capsule (object, GIMP_IS_IMAGE (object) ? GIMP_PAINTER_PROVENANCE_NAMESPACE_IMAGE : GIMP_PAINTER_PROVENANCE_NAMESPACE_ITEM);
  if (!semantic) return {};
  Variant semantic_value = decode (semantic.get ());
  if (semantic_value && (GIMP_IS_IMAGE (object) || supported_item_record (semantic_value.get (), GIMP_ITEM (object)))) return {};
  auto existing = owner_capsule (object, GIMP_PAINTER_PROVENANCE_NAMESPACE_ORIGIN);
  Variant base = decode (existing.get ());
  if (existing && !base) fail ("Unknown origin archive version cannot be overwritten");
  Dictionary dict (base.get ()); dict.put ("version", g_variant_new_uint32 (1));
  preserve_records (dict, object, cancel);
  return encode (origin_name, dict.end (), cancel);
}
Capsule image_record (GimpImage *image, GCancellable *cancel)
{
  auto existing = owner_capsule (G_OBJECT (image), GIMP_PAINTER_PROVENANCE_NAMESPACE_IMAGE);
  Variant base = decode (existing.get ());
  if (existing && !base) return existing;
  Dictionary dict (base.get ());
  dict.put ("version", g_variant_new_uint32 (1));
  dict.put ("dialect", g_variant_new_string ("gimp-painter-standard-v1"));
  preserve_records (dict, G_OBJECT (image), cancel);
  return encode (image_name, dict.end (), cancel);
}
Capsule item_record (GimpImage *image, GimpItem *item, GHashTable *ids, GCancellable *cancel)
{
  auto existing = owner_capsule (G_OBJECT (item), GIMP_PAINTER_PROVENANCE_NAMESPACE_ITEM);
  Variant base = decode (existing.get ());
  if (existing && (!base || !supported_item_record (base.get (), item)))
    {
      if (GIMP_IS_CLONE_LAYER (item) || GIMP_IS_FILTER_LAYER (item) ||
          (GIMP_IS_LAYER (item) && gimp_painter_layer_mode_is_compatibility (gimp_layer_get_mode (GIMP_LAYER (item)))))
        fail ("Unknown Painter extension version, kind or invalid semantic state cannot be overwritten with edited custom semantics");
      return existing;
    }
  Dictionary dict (base.get ());
  dict.put ("version", g_variant_new_uint32 (1));
  dict.put ("id", g_variant_new_uint32 (GPOINTER_TO_UINT (g_hash_table_lookup (ids, item))));
  preserve_records (dict, G_OBJECT (item), cancel);
  if (auto origin = Bytes (gimp_painter_provenance_ref_bytes (G_OBJECT (item), GIMP_PAINTER_PROVENANCE_EXTENSION)))
    dict.put ("original-extension", byte_array (origin.get ()));
  guint32 mode;
  if (GIMP_IS_LAYER (item) && gimp_painter_layer_mode_is_compatibility (gimp_layer_get_mode (GIMP_LAYER (item))) &&
      gimp_painter_layer_mode_to_legacy (gimp_layer_get_mode (GIMP_LAYER (item)), &mode))
    dict.put ("legacy-mode", g_variant_new_uint32 (mode));
  else g_variant_dict_remove (&dict.dict, "legacy-mode");
  if (GIMP_IS_CLONE_LAYER (item))
    {
      GError *error = nullptr;
      Clone ref (gimp_clone_layer_dup_reference (GIMP_CLONE_LAYER (item), &error));
      if (!ref) { if (error) g_error_free (error); fail ("Could not snapshot Clone reference"); }
      dict.put ("kind", g_variant_new_string ("clone"));
      const bool external = ref->source && gimp_item_get_image (GIMP_ITEM (ref->source)) != image;
      const guint32 source_id = ref->source && !external ? GPOINTER_TO_UINT (g_hash_table_lookup (ids, ref->source)) : 0;
      if (ref->source && !source_id)
        {
          Variant origin (GimpPainterXcf::external_reference_origin (G_OBJECT (ref->source), G_OBJECT_TYPE (ref->source), "clone-source"));
          GVariantBuilder array; g_variant_builder_init (&array, G_VARIANT_TYPE ("aa{sv}"));
          g_variant_builder_add_value (&array, origin.get ());
          Variant incoming (g_variant_ref_sink (g_variant_builder_end (&array)));
          merge_external_origins (dict, G_OBJECT (item), incoming.get (), cancel);
        }
      if (ref->source && !source_id)
        {
          ref->source_expired = TRUE; ref->allow_name_lookup = FALSE;
          ref->state = ref->pending_name ? GIMP_CLONE_SOURCE_PENDING : GIMP_CLONE_SOURCE_EXPIRED;
        }
      /* A missing file identity is evidence, not an active binding candidate.
       * Keep it separately so later additions can never revive it by accident. */
      if (!ref->source && ref->source_expired && base)
        {
          guint32 missing = 0;
          if (g_variant_lookup (base.get (), "source-id", "u", &missing) && missing)
            dict.put ("unresolved-source-id", g_variant_new_uint32 (missing));
        }
      if (!ref->source && ref->source_expired && !ref->pending_name)
        ref->allow_name_lookup = FALSE;
      dict.put ("source-id", g_variant_new_uint32 (source_id));
      dict.put ("source-state", g_variant_new_uint32 (ref->state));
      dict.put ("source-expired", g_variant_new_boolean (ref->source_expired));
      dict.put ("allow-name-lookup", g_variant_new_boolean (ref->allow_name_lookup));
      nullable_string (dict, "pending-name", ref->pending_name, cancel);
      nullable_string (dict, "source-name", ref->source_name, cancel);
      String original (gimp_painter_provenance_dup_text (G_OBJECT (item), GIMP_PAINTER_PROVENANCE_NAME));
      if (original) nullable_string (dict, "original-source-name", original.get (), cancel);
      if (auto raw = Bytes (gimp_painter_provenance_ref_bytes (G_OBJECT (item), GIMP_PAINTER_PROVENANCE_EXTENSION)))
        dict.put ("original-extension", byte_array (raw.get ()));
    }
  else if (GIMP_IS_FILTER_LAYER (item))
    {
      auto *filter = GIMP_FILTER_LAYER (item);
      GimpFilterLayerSnapshot state;
      if (!gimp_filter_layer_get_snapshot_state (filter, &state)) fail ("Could not snapshot Filter cache state");
      dict.put ("kind", g_variant_new_string ("filter"));
      gchar *name = gimp_filter_layer_dup_procedure (filter);
      nullable_string (dict, "procedure", name, cancel); g_free (name);
      Bytes definition (gimp_filter_layer_ref_definition (filter));
      dict.put ("has-definition", g_variant_new_boolean (definition != nullptr));
      dict.put ("definition", byte_array (definition.get ()));
      Snapshot args (gimp_filter_layer_snapshot_arguments (filter));
      Bytes opaque (gimp_filter_layer_ref_opaque_arguments (filter));
      if (opaque)
        {
          if (args) fail ("Filter has conflicting converted and opaque argument models");
          Variant value = unpack_opaque_arguments (opaque.get ());
          dict.put ("has-arguments", g_variant_new_boolean (TRUE));
          if (value) dict.put ("arguments", value.get ());
          else g_variant_dict_remove (&dict.dict, "arguments");
        }
      else
        {
          gboolean imported_arguments = FALSE;
          if (base) g_variant_lookup (base.get (), "has-arguments", "b", &imported_arguments);
          if (imported_arguments && !g_variant_dict_contains (&dict.dict, "original-argument-model"))
            {
              Variant original (g_variant_lookup_value (base.get (), "arguments", nullptr));
              if (original) dict.put ("original-argument-model", original.get ());
              Variant original_procedure (g_variant_lookup_value (base.get (), "procedure", nullptr));
              if (original_procedure) dict.put ("original-argument-procedure", original_procedure.get ());
            }
          dict.put ("has-arguments", g_variant_new_boolean (args != nullptr));
          GVariant *external = nullptr;
          Variant encoded_args (GimpPainterXcf::encode_snapshot (args.get (), image, ids, &external, cancel));
          Variant external_origins (external);
          merge_external_origins (dict, G_OBJECT (item), external_origins.get (), cancel);
          dict.put ("arguments", encoded_args.get ());
        }
      dict.put ("generation", g_variant_new_uint64 (state.generation));
      dict.put ("cache-generation", g_variant_new_uint64 (state.cache_generation));
      dict.put ("cache-complete", g_variant_new_boolean (state.cache_complete));
      dict.put ("saved-state", g_variant_new_uint32 (state.state));
    }
  else dict.put ("kind", g_variant_new_string ("ordinary"));
  return encode (item_name, dict.end (), cancel);
}
}
extern "C" gint xcf_painter_provenance_in_parasites (GBytes *source, gsize offset, gsize length)
{
  try
    {
      gsize size; const auto *data = static_cast<const guint8 *> (g_bytes_get_data (source, &size));
      if (offset > size || length > size - offset) return 0;
      const gsize end = offset + length; gint result = -1; guint count = 0;
      const auto word = [&] (guint32 &value) {
        if (end - offset < 4) return false;
        value = (guint32(data[offset]) << 24) | (guint32(data[offset+1]) << 16) |
                (guint32(data[offset+2]) << 8) | data[offset+3]; offset += 4; return true;
      };
      while (offset < end)
        {
          guint32 name_size, flags, data_size;
          if (++count > 100000 || !word (name_size) || name_size > end - offset) return 0;
          const auto *name = data + offset; offset += name_size;
          const bool matching = name_size >= sizeof image_name && !std::memcmp (name, image_name, sizeof image_name);
          if (!word (flags) || !word (data_size) || data_size > end - offset) return 0;
          if (matching)
            {
              result = 0;
              if (name_size == sizeof image_name && flags == GIMP_PARASITE_PERSISTENT && data_size <= max_blob)
                {
                  Parasite parasite (gimp_parasite_new (image_name, flags, data_size, data + offset));
                  Variant value = decode (parasite.get ()); const gchar *dialect = nullptr;
                  if (value && g_variant_lookup (value.get (), "dialect", "&s", &dialect) &&
                      !std::strcmp (dialect, "gimp-painter-standard-v1")) result = 1;
                }
            }
          offset += data_size;
        }
      return result;
    }
  catch (...) { return 0; }
}

struct _XcfPainterSave
{
  struct PreparedCapsule
  {
    Capsule capsule;
    GimpPainterXcf::MultipartManifest manifest;
    bool multipart = false;
  };
  std::unordered_map<GObject *, std::array<PreparedCapsule, 3>> transport;
  std::unordered_map<GObject *, Transport> imported;
  std::unordered_map<GObject *, std::vector<GimpPainter::Bytes>> inert_markers;
  Capsule image;
  std::unordered_map<GimpItem *, Capsule> items;
  std::unordered_map<GObject *, Capsule> origins;
  std::unordered_map<GObject *, Bytes> external_origins;
  std::unordered_map<GimpDrawable *, Object> buffers;
  std::unordered_map<GimpCloneLayer *, Clone> clone_checks;
  std::unordered_map<GimpFilterLayer *, GimpFilterLayerSnapshot> filter_checks;
  std::vector<std::pair<Object, Connection>> observers;
  Object cancellable;
  bool changed = false;
  const char *changed_property = nullptr;
  const char *changed_type = nullptr;
  std::unordered_map<GObject *, const char *> watched_buffers;
  bool custom = false;
  GHashTable *ids = g_hash_table_new (g_direct_hash, g_direct_equal);
  guint32 tattoo_state = 0;
  ~_XcfPainterSave ()
  {
    observers.clear (); /* Disconnect before releasing snapshots/callback data. */
    g_hash_table_unref (ids);
  }
  void watch (GObject *object, const gchar *signal, GCallback callback)
  {
    auto owner = Object::retain (object);
    auto connection = Connection::connect (owner, signal, callback, this, nullptr);
    /* Both are owned before vector publication. If allocation throws, the
     * local Connection disconnects before this transaction is unwound. */
    observers.emplace_back (std::move (owner), std::move (connection));
  }
};
namespace {
void save_notify (GObject *object, GParamSpec *property, gpointer data)
{
  auto *save = static_cast<XcfPainterSave *> (data);
  save->changed = true; save->changed_property = property->name; save->changed_type = G_OBJECT_TYPE_NAME (object);
}
void save_buffer_changed (GeglBuffer *buffer, const GeglRectangle *, gpointer data)
{
  auto *save = static_cast<XcfPainterSave *> (data); save->changed = true;
  save->changed_property = "pixels";
  const auto found = save->watched_buffers.find (G_OBJECT (buffer));
  save->changed_type = found == save->watched_buffers.end () ? "GeglBuffer" : found->second;
}
void save_dirty (GimpImage *, GimpDirtyMask, gpointer data)
{ auto *save = static_cast<XcfPainterSave *> (data); save->changed = true; save->changed_property = "dirty"; save->changed_type = "GimpImage"; }
void save_container_changed (GimpContainer *, GimpObject *, gpointer data) { static_cast<XcfPainterSave *> (data)->changed = true; }
void save_reordered (GimpContainer *, GimpObject *, gint, gpointer data) { static_cast<XcfPainterSave *> (data)->changed = true; }
}
extern "C" XcfPainterSave *xcf_painter_prepare_save_full (GimpImage *image, GCancellable *cancel, gboolean permit_multipart, GError **error)
{
  try
    {
      if (auto refusal = String (gimp_painter_provenance_dup_text (G_OBJECT (image), GIMP_PAINTER_PROVENANCE_SAVE_REFUSAL)))
        fail (refusal.get ());
      std::unique_ptr<XcfPainterSave> result (new XcfPainterSave);
      result->cancellable = cancel ? Object::retain (G_OBJECT (cancel)) : Object::adopt (G_OBJECT (g_cancellable_new ()));
      cancel = G_CANCELLABLE (result->cancellable.get ());
      if (g_cancellable_is_cancelled (cancel))
        throw GimpPainterXcf::StorageError (G_IO_ERROR, G_IO_ERROR_CANCELLED, "Painter save preparation was cancelled");
      GList *items = gimp_image_get_layer_list (image);
      items = g_list_concat (items, gimp_image_get_channel_list (image));
      items = g_list_concat (items, gimp_image_get_path_list (image));
      std::unique_ptr<GList, decltype (&g_list_free)> list (items, g_list_free);
      std::vector<GimpItem *> all;
      std::unordered_set<GimpItem *> objects;
      const auto collect = [&] (GimpItem *item) { if (objects.insert (item).second) all.push_back (item); };
      for (GList *p = items; p; p = p->next)
        {
          collect (GIMP_ITEM (p->data));
          if (GIMP_IS_LAYER (p->data)) if (auto *mask = gimp_layer_get_mask (GIMP_LAYER (p->data))) collect (GIMP_ITEM (mask));
          if (GIMP_IS_DRAWABLE (p->data))
            {
              GimpContainer *effects = gimp_drawable_get_filters (GIMP_DRAWABLE (p->data));
              for (gint i = 0; i < gimp_container_get_n_children (effects); ++i)
                {
                  GimpObject *effect = gimp_container_get_child_by_index (effects, i);
                  if (GIMP_IS_DRAWABLE_FILTER (effect))
                    if (auto *mask = gimp_drawable_filter_get_mask (GIMP_DRAWABLE_FILTER (effect)))
                      collect (GIMP_ITEM (mask));
                }
            }
        }
      if (auto *selection = gimp_image_get_mask (image)) collect (GIMP_ITEM (selection));
      std::unordered_set<guint32> used;
      const auto assign = [&] (GimpItem *item, guint32 id) {
        g_hash_table_insert (result->ids, item, GUINT_TO_POINTER (id));
        used.insert (id); result->tattoo_state = std::max (result->tattoo_state, id);
      };
      /* Prefer identities retained after a prior successful save, then native
       * tattoos. Historical custom replacement can leave duplicate tattoos;
       * normalize only colliding/zero values, without mutating live items. */
      for (auto *item : all)
        {
          const guint32 id = gimp_painter_provenance_get_save_id (G_OBJECT (item));
          if (id && !used.count (id)) assign (item, id);
        }
      /* Reserve every still-unique native tattoo before allocating repairs,
       * including the image selection's ID even when its empty channel is not
       * written. A repaired source must not steal that implicit identity. */
      for (auto *item : all)
        if (!g_hash_table_contains (result->ids, item))
          {
            const guint32 id = gimp_item_get_tattoo (item);
            if (id && !used.count (id)) assign (item, id);
          }
      guint32 next = 1;
      for (auto *item : all)
        if (!g_hash_table_contains (result->ids, item))
          {
            while (used.count (next)) { if (next == G_MAXUINT32) fail ("No free Painter item ID"); ++next; }
            assign (item, next);
          }
      result->watch (G_OBJECT (image), "dirty", G_CALLBACK (save_dirty));
      result->watch (G_OBJECT (image), "notify", G_CALLBACK (save_notify));
      const auto watch_container = [&] (GimpContainer *container) {
        result->watch (G_OBJECT (container), "add", G_CALLBACK (save_container_changed));
        result->watch (G_OBJECT (container), "remove", G_CALLBACK (save_container_changed));
        result->watch (G_OBJECT (container), "reorder", G_CALLBACK (save_reordered));
      };
      watch_container (gimp_image_get_layers (image));
      watch_container (gimp_image_get_channels (image));
      watch_container (gimp_image_get_paths (image));
      for (auto *item : all)
        {
          result->watch (G_OBJECT (item), "notify", G_CALLBACK (save_notify));
          if (auto *children = gimp_viewable_get_children (GIMP_VIEWABLE (item))) watch_container (children);
          if (GIMP_IS_DRAWABLE (item) && !GIMP_IS_GROUP_LAYER (item))
            {
              auto *buffer = G_OBJECT (gimp_drawable_get_buffer (GIMP_DRAWABLE (item)));
              result->watched_buffers.emplace (buffer, G_OBJECT_TYPE_NAME (item));
              result->watch (buffer, "changed", G_CALLBACK (save_buffer_changed));
            }
        }
      result->image = image_record (image, cancel);
      if (auto origin = fallback_origin (G_OBJECT (image), cancel)) result->origins.emplace (G_OBJECT (image), std::move (origin));
      for (auto *item : all)
        {
          if (auto refusal = String (gimp_painter_provenance_dup_text (G_OBJECT (item), GIMP_PAINTER_PROVENANCE_SAVE_REFUSAL)))
            fail (refusal.get ());
          result->items.emplace (item, item_record (image, item, result->ids, cancel));
          {
            Variant record = decode (result->items.at (item).get ());
            if (record)
              {
                Variant external (g_variant_lookup_value (record.get (), external_field, G_VARIANT_TYPE ("aa{sv}")));
                if (external) result->external_origins.emplace (G_OBJECT (item), little_endian_bytes (external.get (), cancel));
              }
          }
          if (GIMP_IS_CLONE_LAYER (item))
            {
              Clone reference (gimp_clone_layer_dup_reference (GIMP_CLONE_LAYER (item), nullptr));
              if (!reference) fail ("Could not retain Clone save checkpoint");
              result->clone_checks.emplace (GIMP_CLONE_LAYER (item), std::move (reference));
            }
          if (GIMP_IS_FILTER_LAYER (item))
            {
              GimpFilterLayerSnapshot state;
              if (!gimp_filter_layer_get_snapshot_state (GIMP_FILTER_LAYER (item), &state)) fail ("Could not retain Filter save checkpoint");
              result->filter_checks.emplace (GIMP_FILTER_LAYER (item), state);
            }
          if (auto origin = fallback_origin (G_OBJECT (item), cancel)) result->origins.emplace (G_OBJECT (item), std::move (origin));
          if (GIMP_IS_CLONE_LAYER (item) || GIMP_IS_FILTER_LAYER (item) ||
              (GIMP_IS_LAYER (item) && gimp_painter_layer_mode_is_compatibility (gimp_layer_get_mode (GIMP_LAYER (item)))))
            result->custom = true;
          if (GIMP_IS_DRAWABLE (item))
            result->buffers.emplace (GIMP_DRAWABLE (item), Object::adopt (G_OBJECT (gegl_buffer_dup (gimp_drawable_get_buffer (GIMP_DRAWABLE (item))))));
        }
      /* Reading a group projection can synchronously fill its own cache and
       * emit buffer::changed. Children, topology and group properties were
       * watched throughout; begin watching the derived cache only after all
       * committed buffer copies finish, so cache filling is not an edit. */
      for (auto *item : all)
        if (GIMP_IS_GROUP_LAYER (item))
          {
            auto *buffer = G_OBJECT (gimp_drawable_get_buffer (GIMP_DRAWABLE (item)));
            result->watched_buffers.emplace (buffer, G_OBJECT_TYPE_NAME (item));
            result->watch (buffer, "changed", G_CALLBACK (save_buffer_changed));
          }
      const auto prepare = [&] (GObject *owner, GimpPainterXcf::Namespace space, Capsule capsule) {
        auto& prepared = result->transport[owner][guint (space)];
        prepared.capsule = std::move (capsule);
        if (!prepared.capsule.get ()) return;
        const gsize size = g_bytes_get_size (prepared.capsule.get ());
        if (!permit_multipart && size > max_blob)
          fail ("Painter metadata exceeds the inline XCF size limit; multipart Save is not enabled");
        /* Opaque capsules keep their complete original wire value and flags.
         * Only newly encoded known semantics may choose a new transport. */
        prepared.multipart = permit_multipart && prepared.capsule.regenerated &&
                             size > 1024u * 1024u;
        if (prepared.multipart)
          {
            using namespace GimpPainterXcf;
            const OwnerClass type = GIMP_IS_IMAGE (owner) ? OwnerClass::image : GIMP_IS_LAYER_MASK (owner) ? OwnerClass::layer_mask :
                                    GIMP_IS_LAYER (owner) ? OwnerClass::layer : GIMP_IS_CHANNEL (owner) ? OwnerClass::channel : OwnerClass::path;
            const guint32 id = GIMP_IS_IMAGE (owner) ? 0 : GPOINTER_TO_UINT (g_hash_table_lookup (result->ids, owner));
            prepared.manifest = multipart_manifest ({type, id}, space, prepared.capsule.get (), cancel);
            result->custom = true; /* Metadata alone can require 64-bit offsets. */
          }
      };
      prepare (G_OBJECT (image), GimpPainterXcf::Namespace::image, result->image);
      for (const auto& item : result->items) prepare (G_OBJECT (item.first), GimpPainterXcf::Namespace::item, item.second);
      for (const auto& origin : result->origins) prepare (origin.first, GimpPainterXcf::Namespace::origin, origin.second);
      for (const auto& entry : result->transport)
        if (Transport imported {gimp_painter_provenance_ref_transport (entry.first)})
          {
            result->custom = true;
            std::vector<GimpPainter::Bytes> inert;
            const auto *flags = static_cast<const guint8 *> (g_bytes_get_data (imported->dispositions, nullptr));
            for (guint i = 0; imported->records && i < imported->records->len; ++i)
              if (!flags[i]) inert.push_back (GimpPainter::Bytes::retain (static_cast<GBytes *> (g_ptr_array_index (imported->records, i))));
            result->inert_markers.emplace (entry.first, GimpPainterXcf::multipart_inert_markers (inert, cancel));
            result->imported.emplace (entry.first, std::move (imported));
          }
      if (cancel && g_cancellable_is_cancelled (cancel))
        throw GimpPainterXcf::StorageError (G_IO_ERROR, G_IO_ERROR_CANCELLED, "Painter save preparation was cancelled");
      if (!xcf_painter_save_unchanged (result.get ()))
        {
          if (result->changed_property)
            throw std::runtime_error (std::string ("The image changed during save preparation: ") +
              result->changed_type + "::" + result->changed_property);
          fail ("The image changed during save preparation");
        }
      return result.release ();
    }
  catch (const GimpPainterXcf::StorageError &exception)
    { g_set_error_literal (error, exception.domain, exception.code, exception.what ()); return nullptr; }
  catch (const std::exception &exception)
    { g_set_error_literal (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, exception.what ()); return nullptr; }
  catch (...)
    { g_set_error_literal (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "Could not prepare Painter save snapshot"); return nullptr; }
}
extern "C" XcfPainterSave *xcf_painter_prepare_save (GimpImage *image, GError **error)
{ return xcf_painter_prepare_save_full (image, nullptr, TRUE, error); }
extern "C" void xcf_painter_free_save (XcfPainterSave *save) { delete save; }
extern "C" GCancellable *xcf_painter_save_cancellable (XcfPainterSave *save) { return G_CANCELLABLE (save->cancellable.get ()); }
extern "C" gboolean xcf_painter_save_unchanged (XcfPainterSave *save)
{
  if (!save || save->changed) return FALSE;
  for (const auto &entry : save->clone_checks)
    {
      Clone now (gimp_clone_layer_dup_reference (entry.first, nullptr)); const auto &before = entry.second;
      if (!now || now->source != before->source || now->state != before->state ||
          now->source_expired != before->source_expired || now->allow_name_lookup != before->allow_name_lookup ||
          g_strcmp0 (now->pending_name, before->pending_name) || g_strcmp0 (now->source_name, before->source_name)) return FALSE;
    }
  for (const auto &entry : save->filter_checks)
    {
      GimpFilterLayerSnapshot now; const auto &before = entry.second;
      if (!gimp_filter_layer_get_snapshot_state (entry.first, &now) || now.generation != before.generation ||
          now.cache_generation != before.cache_generation || now.cache_complete != before.cache_complete ||
          now.state != before.state) return FALSE;
    }
  return TRUE;
}
extern "C" gboolean xcf_painter_save_needs_v11 (XcfPainterSave *save) { return save && save->custom; }
extern "C" void xcf_painter_commit_save (XcfPainterSave *save)
{
  for (const auto &item : save->items)
    gimp_painter_provenance_set_save_id (G_OBJECT (item.first), GPOINTER_TO_UINT (g_hash_table_lookup (save->ids, item.first)));
  /* Retain diagnostic provenance if a live external target later expires,
   * without changing the live binding, definition or native parasite list. */
  for (const auto &entry : save->external_origins)
    gimp_painter_provenance_set_bytes (entry.first, GIMP_PAINTER_PROVENANCE_EXTERNAL, entry.second.get ());
}
extern "C" guint32 xcf_painter_saved_id (XcfPainterSave *save, GimpItem *item)
{ return save ? GPOINTER_TO_UINT (g_hash_table_lookup (save->ids, item)) : gimp_item_get_tattoo (item); }
extern "C" guint32 xcf_painter_saved_tattoo_state (XcfPainterSave *save, GimpImage *image)
{ return save ? std::max (save->tattoo_state, gimp_image_get_tattoo_state (image)) : gimp_image_get_tattoo_state (image); }

extern "C" GimpParasite *xcf_painter_image_parasite (XcfPainterSave *save)
{ return save && save->image.get () ? gimp_parasite_new (image_name, save->image.flags, g_bytes_get_size (save->image.get ()), g_bytes_get_data (save->image.get (), nullptr)) : nullptr; }
extern "C" GimpParasite *xcf_painter_origin_parasite (XcfPainterSave *save, GObject *object)
{
  if (!save) return nullptr;
  const auto found = save->origins.find (object);
  return found == save->origins.end () || !found->second.get () ? nullptr : gimp_parasite_new (origin_name, found->second.flags, g_bytes_get_size (found->second.get ()), g_bytes_get_data (found->second.get (), nullptr));
}
extern "C" GimpParasite *xcf_painter_item_parasite (XcfPainterSave *save, GimpItem *item)
{
  if (!save) return nullptr;
  const auto found = save->items.find (item);
  return found == save->items.end () || !found->second.get () ? nullptr : gimp_parasite_new (item_name, found->second.flags, g_bytes_get_size (found->second.get ()), g_bytes_get_data (found->second.get (), nullptr));
}
namespace {
bool write_bytes (XcfInfo *info, const void *raw, gsize size, GError **error)
{
  const auto *data = static_cast<const guint8 *> (raw);
  while (size)
    {
      const gint part = std::min (size, gsize (65536));
      if (xcf_write_int8 (info, data, part, error) != guint (part)) return false;
      GimpPainterXcf::discard_snapshot_pages (data, part);
      data += part; size -= part;
    }
  return true;
}
bool write_record (XcfInfo *info, GBytes *bytes, GError **error)
{
  const gsize size = g_bytes_get_size (bytes);
  if (size > G_MAXUINT32) fail ("An inner parasite exceeds XCF property framing");
  const guint32 header[] = {GUINT32_TO_BE (PROP_PARASITES), GUINT32_TO_BE (size)};
  return write_bytes (info, header, sizeof header, error) &&
         write_bytes (info, g_bytes_get_data (bytes, nullptr), size, error);
}
bool write_inline (XcfInfo *info, guint space, const Capsule& capsule, GError **error)
{
  const char *name = GimpPainterXcf::namespace_name (GimpPainterXcf::Namespace (space));
  const guint32 name_size = std::strlen (name) + 1;
  const gsize size = g_bytes_get_size (capsule.get ());
  if (size > G_MAXUINT32 - name_size - 12) fail ("An inline capsule exceeds XCF framing");
  const guint32 header[] = {GUINT32_TO_BE (PROP_PARASITES), GUINT32_TO_BE (size + name_size + 12), GUINT32_TO_BE (name_size)};
  const guint32 fields[] = {GUINT32_TO_BE (capsule.flags), GUINT32_TO_BE (size)};
  return write_bytes (info, header, sizeof header, error) && write_bytes (info, name, name_size, error) &&
         write_bytes (info, fields, sizeof fields, error) && write_bytes (info, g_bytes_get_data (capsule.get (), nullptr), size, error);
}
}
extern "C" gboolean xcf_painter_replaces_parasite (XcfInfo *info, GObject *owner, const gchar *name)
{
  const auto *save = static_cast<XcfPainterSave *> (info->painter_save_state);
  if (!save) return FALSE;
  const auto found = save->transport.find (owner);
  if (found == save->transport.end ()) return FALSE;
  for (guint n = 0; n < 3; ++n)
    if (!std::strcmp (name, GimpPainterXcf::namespace_name (GimpPainterXcf::Namespace (n))))
      {
        const auto& capsule = found->second[n].capsule;
        return capsule.get () && (!capsule.from_transport || capsule.regenerated);
      }
  return FALSE;
}
extern "C" gboolean xcf_painter_write_owner_records (XcfInfo *info, GObject *owner, GError **error)
{
  try
    {
      auto *save = static_cast<XcfPainterSave *> (info->painter_save_state);
      if (!save) return TRUE;
      const auto found = save->transport.find (owner);
      std::array<bool, 3> replaced {{false,false,false}};
      if (found != save->transport.end ())
        for (guint n = 0; n < 3; ++n)
          {
            const auto& prepared = found->second[n]; const auto& capsule = prepared.capsule;
            if (!capsule.get () || (capsule.from_transport && !capsule.regenerated)) continue;
            replaced[n] = true;
            if (!prepared.multipart)
              { if (!write_inline (info, n, capsule, error)) return FALSE; }
            else
              {
                auto manifest = GimpPainterXcf::multipart_manifest_record (prepared.manifest);
                if (!write_record (info, manifest.get (), error)) return FALSE;
                for (guint32 index = 0; index < prepared.manifest.count; ++index)
                  {
                    auto part = GimpPainterXcf::multipart_chunk_record (prepared.manifest, index, capsule.get ());
                    if (!write_record (info, part.get (), error)) return FALSE;
                  }
              }
          }
      const auto markers = save->inert_markers.find (owner);
      if (markers != save->inert_markers.end ())
        for (const auto& record : markers->second)
          if (!write_record (info, record.get (), error)) return FALSE;
      const auto prior = save->imported.find (owner);
      const auto *imported = prior == save->imported.end () ? nullptr : prior->second.get ();
      if (imported && imported->records)
        {
          const auto *dispositions = static_cast<const guint8 *> (g_bytes_get_data (imported->dispositions, nullptr));
          for (guint i = 0; i < imported->records->len; ++i)
            if (!dispositions[i] || !replaced[dispositions[i] - 1])
              if (!write_record (info, static_cast<GBytes *> (g_ptr_array_index (imported->records, i)), error)) return FALSE;
        }
      return TRUE;
    }
  catch (const GimpPainterXcf::StorageError& exception)
    { g_set_error_literal (error, exception.domain, exception.code, exception.what ()); return FALSE; }
  catch (const std::exception& exception)
    { g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED, exception.what ()); return FALSE; }
  catch (...)
    { g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED, "Could not write Painter metadata"); return FALSE; }
}
extern "C" GeglBuffer *xcf_painter_saved_buffer (XcfPainterSave *save, GimpDrawable *drawable)
{
  if (save) { const auto found = save->buffers.find (drawable); if (found != save->buffers.end ()) return GEGL_BUFFER (found->second.get ()); }
  return gimp_drawable_get_buffer (drawable);
}
extern "C" GimpLayerMode xcf_painter_standard_mode (GimpLayerMode mode)
{ return gimp_painter_layer_mode_is_compatibility (mode) ? GIMP_LAYER_MODE_NORMAL_LEGACY : mode; }
extern "C" void xcf_painter_capture_unknown (XcfInfo *info, GPtrArray *records, goffset begin, goffset end)
{
  if (info->painter_source && begin >= 0 && end >= begin && static_cast<guint64> (end) <= g_bytes_get_size (info->painter_source))
    g_ptr_array_add (records, g_bytes_new_from_bytes (info->painter_source, begin, end - begin));
}
extern "C" void xcf_painter_set_unknown_records (GObject *object, GPtrArray *records)
{
  gimp_painter_provenance_set_records (object, records);
}
static void capture_origin_type (GObject *object)
{
  String existing (gimp_painter_provenance_dup_text (object, GIMP_PAINTER_PROVENANCE_TYPE));
  if (!existing)
    {
      const char *context = GIMP_IS_IMAGE (object) ? "image" : GIMP_IS_LAYER (object) ? "layer" :
                            GIMP_IS_CHANNEL (object) ? "channel" : GIMP_IS_ITEM (object) ? "path" : "object";
      String name (g_strconcat (context, ":", G_OBJECT_TYPE_NAME (object), nullptr));
      gimp_painter_provenance_set_text (object, GIMP_PAINTER_PROVENANCE_TYPE, name.get ());
    }
}
extern "C" void xcf_painter_capture_header (XcfInfo *info, GObject *object, goffset begin, goffset end)
{
  if (!info->painter_source || begin < 0 || end < begin || static_cast<guint64> (end) > g_bytes_get_size (info->painter_source)) return;
  capture_origin_type (object);
  Bytes raw (g_bytes_new_from_bytes (info->painter_source, begin, end - begin));
  gimp_painter_provenance_set_bytes (object, GIMP_PAINTER_PROVENANCE_HEADER, raw.get ());
}
extern "C" void xcf_painter_capture_properties (XcfInfo *info, GObject *object, goffset begin, goffset end)
{
  if (!info->painter_source || begin < 0 || end < begin || static_cast<guint64> (end) > g_bytes_get_size (info->painter_source)) return;
  capture_origin_type (object);
  Bytes raw (xcf_painter_without_transport (info, object, begin, end));
  if (!raw)
    {
      raw.reset (g_bytes_new_from_bytes (info->painter_source, begin, end - begin));
      gimp_painter_provenance_set_text (object, GIMP_PAINTER_PROVENANCE_SAVE_REFUSAL,
                                       "Malformed property archive cannot be safely rewritten");
    }
  gimp_painter_provenance_set_bytes (object, GIMP_PAINTER_PROVENANCE_PROPERTIES, raw.get ());
}
namespace {
void load_warning (XcfInfo *info, const char *message)
{
  gimp_message (info->gimp, G_OBJECT (info->progress), GIMP_MESSAGE_WARNING,
                "Painter XCF metadata was retained but could not be fully restored: %s", message);
}
void retarget (XcfInfo *info, GimpLayer *old, GimpLayer *replacement)
{
  for (GList *p = info->selected_layers; p; p = p->next) if (p->data == old) p->data = replacement;
  for (GList *p = info->linked_layers; p; p = p->next) if (p->data == old) p->data = replacement;
  if (info->floating_sel == old) info->floating_sel = replacement;
  /* Item-set membership is parsed before the carrier is replaced.  The list
   * owns borrowed item identities and has no mutating replacement API; rebuild
   * each affected still-private construction-time set before freeing the proxy. */
  for (GList *p = info->layer_sets; p; p = p->next)
    {
      auto *set = GIMP_ITEM_LIST (p->data);
      if (!set || gimp_item_list_is_pattern (set, nullptr)) continue;
      GList *items = gimp_item_list_get_items (set, nullptr);
      GList *member = g_list_find (items, old);
      if (member)
        {
          member->data = replacement;
          GimpItemList *updated = gimp_item_list_named_new (gimp_item_get_image (GIMP_ITEM (replacement)),
                                                           gimp_item_list_get_item_type (set),
                                                           gimp_object_get_name (set), items);
          p->data = updated;
          g_object_unref (set);
        }
      g_list_free (items);
    }
}
GimpLayer *find_layer_id (GimpImage *image, guint32 id)
{
  if (!id) return nullptr;
  GList *layers = gimp_image_get_layer_list (image);
  GimpLayer *result = nullptr;
  for (GList *p = layers; p; p = p->next)
    if (gimp_item_get_tattoo (GIMP_ITEM (p->data)) == id)
      { if (result) { g_list_free (layers); return nullptr; } result = GIMP_LAYER (p->data); }
  g_list_free (layers); return result;
}
}
extern "C" void xcf_painter_retarget_layer (XcfInfo *info, GimpLayer *old, GimpLayer *replacement)
{ retarget (info, old, replacement); }
extern "C" gboolean xcf_painter_restore_layer (XcfInfo *info, GimpImage *image, GimpLayer **layer)
{
  try
    {
      auto capsule = owner_capsule (G_OBJECT (*layer), GIMP_PAINTER_PROVENANCE_NAMESPACE_ITEM);
      if (info->painter_legacy || !capsule) return TRUE;
      Variant dict = decode (capsule.get ());
      if (!dict) { load_warning (info, "unknown extension version"); return TRUE; }
      const gchar *kind = nullptr;
      if (!g_variant_lookup (dict.get (), "kind", "&s", &kind)) fail ("Missing layer kind");
      const bool clone = !std::strcmp (kind, "clone"), filter = !std::strcmp (kind, "filter");
      if (!clone && !filter && std::strcmp (kind, "ordinary"))
        { load_warning (info, "unknown layer kind"); return TRUE; }
      /* Validate the complete cache/definition envelope before replacing the
       * inert ordinary proxy. A malformed cache must never become executable
       * or be certified fresh by a catch-and-mark-loaded recovery path.
       */
      if (!supported_item_record (dict.get (), GIMP_ITEM (*layer)))
        fail ("Painter metadata conflicts with its native owner or semantic schema; original capsule remains inert");
      validate_common_item_record (dict.get ());
      if (filter) validate_filter_record (dict.get ());
      if (clone) validate_clone_record (dict.get ());
      GimpLayerMode mapped;
      const bool has_mode = item_legacy_mode (dict.get (), &mapped);
      std::unique_ptr<gchar, decltype (&g_free)> original (read_string (dict.get (), "original-source-name"), g_free);
      if (clone || filter)
        {
          GimpLayer *old = *layer;
          if (GIMP_IS_GROUP_LAYER (old)) fail ("Custom layer metadata conflicts with a group record");
          const gint width = gimp_item_get_width (GIMP_ITEM (old)), height = gimp_item_get_height (GIMP_ITEM (old));
          const GimpLayerMode mode = gimp_layer_get_mode (old);
          const auto blend = gimp_layer_get_blend_space (old);
          const auto composite_space = gimp_layer_get_composite_space (old);
          const auto composite_mode = gimp_layer_get_composite_mode (old);
          const gboolean lock_alpha = gimp_layer_get_lock_alpha (old);
          const Babl *format = gimp_drawable_get_format (GIMP_DRAWABLE (old));
          GimpLayer *replacement = clone
            ? gimp_clone_layer_new (image, nullptr, width, height, gimp_object_get_name (old), gimp_layer_get_opacity (old), mode)
            : gimp_filter_layer_new (image, width, height, gimp_object_get_name (old), gimp_layer_get_opacity (old), mode);
          if (!replacement) fail ("Could not create custom layer");
          gimp_item_replace_item (GIMP_ITEM (replacement), GIMP_ITEM (old));
          GeglBuffer *buffer = gegl_buffer_new (GEGL_RECTANGLE (0, 0, width, height), format);
          gimp_drawable_set_buffer (GIMP_DRAWABLE (replacement), FALSE, nullptr, buffer); g_object_unref (buffer);
          gimp_layer_set_blend_space (replacement, blend, FALSE);
          gimp_layer_set_composite_space (replacement, composite_space, FALSE);
          gimp_layer_set_composite_mode (replacement, composite_mode, FALSE);
          if (gimp_layer_can_lock_alpha (replacement)) gimp_layer_set_lock_alpha (replacement, lock_alpha, FALSE);
          gimp_painter_copy_provenance (G_OBJECT (old), G_OBJECT (replacement));
          retarget (info, old, replacement);
          g_object_ref_sink (old); g_object_unref (old); *layer = replacement;
        }
      if (has_mode) gimp_layer_set_mode (*layer, mapped, FALSE);
      if (original && !gimp_painter_provenance_set_text (G_OBJECT (*layer), GIMP_PAINTER_PROVENANCE_NAME, original.get ()))
        fail ("Could not retain Painter source name");
      if (!gimp_painter_provenance_set_definition (G_OBJECT (*layer), dict.get ()))
        fail ("Could not retain Painter definition");
      return TRUE;
    }
  catch (const std::exception &exception) { load_warning (info, exception.what ()); return TRUE; }
  catch (...) { load_warning (info, "allocation failure"); return FALSE; }
}
extern "C" void xcf_painter_restore_bindings (XcfInfo *info, GimpImage *image)
{
  GList *layers = gimp_image_get_layer_list (image);
  for (GList *p = layers; p; p = p->next)
    {
      auto *object = G_OBJECT (p->data);
      Variant definition (gimp_painter_provenance_ref_definition (object));
      auto *dict = definition.get ();
      if (!dict) continue;
      try
        {
          if (GIMP_IS_CLONE_LAYER (object))
            {
              validate_clone_record (dict);
              GimpCloneLayerReference reference = {};
              guint32 id, state;
              if (!g_variant_lookup (dict, "source-id", "u", &id) ||
                  !g_variant_lookup (dict, "source-state", "u", &state) || state > GIMP_CLONE_SOURCE_EXPIRED)
                fail ("Invalid Clone reference state");
              reference.state = static_cast<GimpCloneSourceState> (state);
              g_variant_lookup (dict, "source-expired", "b", &reference.source_expired);
              g_variant_lookup (dict, "allow-name-lookup", "b", &reference.allow_name_lookup);
              reference.source = reference.source_expired ? nullptr : find_layer_id (image, id);
              std::unique_ptr<gchar, decltype (&g_free)> pending (read_string (dict, "pending-name"), g_free);
              std::unique_ptr<gchar, decltype (&g_free)> name (read_string (dict, "source-name"), g_free);
              reference.pending_name = pending.get (); reference.source_name = name.get ();
              if (id && !reference.source)
                {
                  reference.source_expired = TRUE; reference.allow_name_lookup = FALSE;
                  reference.state = reference.pending_name ? GIMP_CLONE_SOURCE_PENDING : GIMP_CLONE_SOURCE_EXPIRED;
                }
              GError *error = nullptr;
              if (!gimp_clone_layer_restore_reference (GIMP_CLONE_LAYER (object), &reference, &error))
                { if (error) g_error_free (error); fail ("Clone reference could not be restored"); }
            }
          else if (GIMP_IS_FILTER_LAYER (object))
            {
              auto *filter = GIMP_FILTER_LAYER (object);
              validate_filter_record (dict);
              GimpFilterLayerSnapshot state = filter_cache_state (dict);
              std::unique_ptr<gchar, decltype (&g_free)> name (read_string (dict, "procedure"), g_free);
              gboolean has_definition = FALSE, has_args = FALSE;
              g_variant_lookup (dict, "has-definition", "b", &has_definition);
              g_variant_lookup (dict, "has-arguments", "b", &has_args);
              Variant definition_value (g_variant_lookup_value (dict, "definition", G_VARIANT_TYPE_BYTESTRING));
              Bytes definition (has_definition ? variant_bytes (definition_value.get ()) : nullptr);
              Snapshot arguments;
              Bytes opaque_arguments;
              if (has_args)
                {
                  Variant value (g_variant_lookup_value (dict, "arguments", nullptr));
                  try { if (!value) fail ("Invalid arguments"); arguments.reset (GimpPainterXcf::decode_snapshot (value.get (), image)); }
                  catch (const std::exception &exception)
                    {
                      opaque_arguments = pack_opaque_arguments (value.get ());
                      load_warning (info, exception.what ());
                    }
                }
              GError *error = nullptr;
              const gboolean restored = opaque_arguments
                ? gimp_filter_layer_set_definition_with_opaque_arguments (filter, name.get (), definition.get (), opaque_arguments.get (), &error)
                : gimp_filter_layer_set_definition_with_snapshot (filter, name.get (), definition.get (), arguments.get (), &error);
              if (!restored)
                { if (error) g_error_free (error); fail ("Filter definition could not be restored"); }
              if (!gimp_filter_layer_restore_snapshot_state (filter, &state, &error))
                { if (error) g_error_free (error); fail ("Filter cache generation could not be restored"); }
            }
        }
      catch (const std::exception &exception)
        {
          gimp_painter_provenance_set_text (object, GIMP_PAINTER_PROVENANCE_SAVE_REFUSAL,
            "Painter bindings could not be restored without data loss; original metadata is retained");
          load_warning (info, exception.what ());
        }
      catch (...)
        {
          gimp_painter_provenance_set_text (object, GIMP_PAINTER_PROVENANCE_SAVE_REFUSAL,
            "Painter bindings could not be restored without data loss; original metadata is retained");
          load_warning (info, "allocation failure");
        }
    }
  g_list_free (layers);
}

extern "C" GBytes *xcf_painter_snapshot_bytes (GInputStream *input, GCancellable *cancel, GError **error)
{
  try { return GimpPainterXcf::snapshot_stream (input, cancel).release (); }
  catch (const GimpPainterXcf::StorageError& exception)
    { g_set_error_literal (error, exception.domain, exception.code, exception.what ()); return nullptr; }
  catch (const std::exception& exception)
    { g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED, exception.what ()); return nullptr; }
  catch (...) { g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED, "Could not snapshot bytes"); return nullptr; }
}
extern "C" void xcf_painter_discard_snapshot_pages (const void *data, gsize size)
{ GimpPainterXcf::discard_snapshot_pages (data, size); }
extern "C" guint xcf_painter_storage_live_files (void)
{ return GimpPainterXcf::storage_live_files (); }
