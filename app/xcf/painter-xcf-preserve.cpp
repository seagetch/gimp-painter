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
#include "painter-xcf-load.h"
#include "painter-xcf-preserve.h"
}
#include "painter-xcf-arguments.hpp"
#include "painter/object-ref.hpp"
#include "painter/connection.hpp"
#include "painter/resources.hpp"
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
using Object = GimpPainter::ObjectRef<GObject>;
using GimpPainter::Connection;
using Args = std::unique_ptr<GimpValueArray, ArgsFree>;
struct SnapshotFree { void operator() (GimpFilterArgumentsSnapshot *p) const { if (p) gimp_filter_arguments_snapshot_free (p); } };
using Snapshot = std::unique_ptr<GimpFilterArgumentsSnapshot, SnapshotFree>;
using Clone = std::unique_ptr<GimpCloneLayerReference, CloneFree>;
[[noreturn]] void fail (const char *message) { throw std::runtime_error (message); }
GVariant *byte_array (const void *data, gsize length)
{
  if (length > max_blob) fail ("Painter metadata exceeds the XCF parasite size limit");
  return g_variant_new_fixed_array (G_VARIANT_TYPE_BYTE, data, length, 1);
}
GVariant *byte_array (GBytes *raw)
{
  gsize length = 0; const void *data = raw ? g_bytes_get_data (raw, &length) : nullptr;
  return byte_array (data, length);
}
GBytes *variant_bytes (GVariant *value)
{
  if (!value || !g_variant_is_of_type (value, G_VARIANT_TYPE_BYTESTRING)) fail ("Invalid Painter byte field");
  gsize length; const void *data = g_variant_get_fixed_array (value, &length, 1);
  return g_bytes_new (data, length);
}
Variant decode (const GimpParasite *parasite)
{
  if (!parasite) return {};
  guint32 length = 0;
  auto *data = static_cast<const guint8 *> (gimp_parasite_get_data (parasite, &length));
  if (length < sizeof magic || std::memcmp (data, magic, sizeof magic)) return {};
  if (length > max_blob) fail ("Painter metadata exceeds the bounded record size");
  Bytes raw (g_bytes_new (data + sizeof magic, length - sizeof magic));
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
  return value;
}
Parasite encode (const gchar *name, GVariant *dict)
{
  Variant value (g_variant_ref_sink (dict));
#if G_BYTE_ORDER == G_BIG_ENDIAN
  value.reset (g_variant_byteswap (value.get ()));
#endif
  const gsize size = g_variant_get_size (value.get ());
  if (size > max_blob - sizeof magic) fail ("Painter metadata is too large for one XCF parasite; destination was not changed");
  std::vector<guint8> bytes (sizeof magic + size);
  std::memcpy (bytes.data (), magic, sizeof magic);
  /* Serialized GVariant data has alignment requirements; the wire header
   * need not. Copy from its aligned immutable serialization. */
  std::memcpy (bytes.data () + sizeof magic, g_variant_get_data (value.get ()), size);
  return Parasite (gimp_parasite_new (name, GIMP_PARASITE_PERSISTENT, bytes.size (), bytes.data ()));
}
struct Dictionary
{
  GVariantDict dict;
  bool ended = false;
  explicit Dictionary (GVariant *base = nullptr) { g_variant_dict_init (&dict, base); }
  ~Dictionary () { if (!ended) g_variant_dict_clear (&dict); }
  void put (const char *key, GVariant *value) { g_variant_dict_insert_value (&dict, key, value); }
  GVariant *end () { ended = true; return g_variant_dict_end (&dict); }
};
void nullable_string (Dictionary &dict, const char *key, const char *value)
{
  if (value) dict.put (key, byte_array (value, std::strlen (value) + 1));
  else g_variant_dict_remove (&dict.dict, key);
}
gchar *read_string (GVariant *dict, const char *key)
{
  Variant value (g_variant_lookup_value (dict, key, G_VARIANT_TYPE_BYTESTRING));
  if (!value) return nullptr;
  gsize size;
  const auto *data = static_cast<const gchar *> (g_variant_get_fixed_array (value.get (), &size, 1));
  if (!size || data[size - 1] || std::memchr (data, 0, size - 1)) fail ("Malformed Painter name");
  return static_cast<gchar *> (g_memdup2 (data, size));
}
const char external_field[] = "external-reference-origins";
Bytes little_endian_bytes (GVariant *value)
{
  Variant wire (g_variant_ref (value));
#if G_BYTE_ORDER == G_BIG_ENDIAN
  wire.reset (g_variant_byteswap (wire.get ()));
#endif
  return Bytes (g_variant_get_data_as_bytes (wire.get ()));
}
void merge_external_origins (Dictionary &dict, GObject *object, GVariant *incoming)
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
  std::unordered_map<std::string, std::vector<gsize>> fingerprints;
  gsize retained_size = 0;
  for (GVariant *source : {existing.get (), cached.get (), incoming})
    if (source)
      {
        if (!g_variant_is_of_type (source, G_VARIANT_TYPE ("aa{sv}"))) fail ("Invalid external reference provenance type");
        const gsize count = g_variant_n_children (source);
        if (count > 65536) fail ("Too many external reference records");
        for (gsize i = 0; i < count; ++i)
          {
            Variant record (g_variant_get_child_value (source, i));
            Bytes raw (g_variant_get_data_as_bytes (record.get ()));
            std::unique_ptr<gchar, decltype (&g_free)> digest (g_compute_checksum_for_bytes (G_CHECKSUM_SHA256, raw.get ()), g_free);
            auto &bucket = fingerprints[std::string (digest.get ())];
            bool seen = false;
            for (gsize index : bucket)
              if (g_variant_equal (records[index].get (), record.get ())) { seen = true; break; }
            if (!seen)
              {
                const gsize size = g_bytes_get_size (raw.get ());
                if (records.size () >= 65536 || size > max_blob - retained_size) fail ("External reference archive exceeds its serialization limits");
                retained_size += size;
                records.emplace_back (std::move (record));
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
void preserve_records (Dictionary &dict, GObject *object)
{
  merge_external_origins (dict, object, nullptr);
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
      gsize size = 0;
      for (guint i = 0; i < unknown->len; ++i)
        {
          const gsize part = g_bytes_get_size (static_cast<GBytes *> (g_ptr_array_index (unknown.get (), i)));
          if (part > max_blob - size) fail ("Opaque property archive exceeds the XCF parasite size limit");
          size += part;
        }
      std::vector<guint8> current (size); gsize offset = 0;
      for (guint i = 0; i < unknown->len; ++i)
        {
          gsize part; const void *data = g_bytes_get_data (static_cast<GBytes *> (g_ptr_array_index (unknown.get (), i)), &part);
          std::memcpy (current.data () + offset, data, part); offset += part;
        }
      Variant old (g_variant_dict_lookup_value (&dict.dict, "opaque-record-sequences", G_VARIANT_TYPE ("aay")));
      if (!old && g_variant_dict_contains (&dict.dict, "opaque-record-sequences")) fail ("Invalid opaque property archive");
      GVariantBuilder history; g_variant_builder_init (&history, G_VARIANT_TYPE ("aay"));
      bool seen = false;
      if (old)
        for (gsize i = 0; i < g_variant_n_children (old.get ()); ++i)
          {
            Variant sequence (g_variant_get_child_value (old.get (), i)); gsize length;
            const void *data = g_variant_get_fixed_array (sequence.get (), &length, 1);
            seen = seen || (length == size && !std::memcmp (data, current.data (), size));
            g_variant_builder_add_value (&history, sequence.get ());
          }
      if (!seen) g_variant_builder_add_value (&history, byte_array (current.data (), size));
      dict.put ("opaque-record-sequences", g_variant_builder_end (&history));
    }
}
/* The core owns this uninterpreted current model without knowing XCF. A
 * nullable variant preserves an absent field as distinct from any field value. */
Bytes pack_opaque_arguments (GVariant *value)
{
  Variant wrapper (g_variant_ref_sink (g_variant_new_maybe (G_VARIANT_TYPE_VARIANT,
                                      value ? g_variant_new_variant (value) : nullptr)));
#if G_BYTE_ORDER == G_BIG_ENDIAN
  wrapper.reset (g_variant_byteswap (wrapper.get ()));
#endif
  return Bytes (g_variant_get_data_as_bytes (wrapper.get ()));
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
  if (!size || data[size - 1] || std::memchr (data, 0, size - 1))
    fail ("Invalid Painter name encoding; original capsule remains inert");
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
  validate_optional_name (dict, "procedure");
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
  validate_optional_name (dict, "pending-name");
  validate_optional_name (dict, "source-name");
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
Parasite fallback_origin (GObject *object)
{
  const auto *semantic = GIMP_IS_IMAGE (object) ? gimp_image_parasite_find (GIMP_IMAGE (object), image_name)
                                               : gimp_item_parasite_find (GIMP_ITEM (object), item_name);
  if (!semantic) return {};
  Variant semantic_value = decode (semantic);
  if (semantic_value && (GIMP_IS_IMAGE (object) || supported_item_record (semantic_value.get (), GIMP_ITEM (object)))) return {};
  const auto *existing = GIMP_IS_IMAGE (object) ? gimp_image_parasite_find (GIMP_IMAGE (object), origin_name)
                                               : gimp_item_parasite_find (GIMP_ITEM (object), origin_name);
  Variant base = decode (existing);
  if (existing && !base) fail ("Unknown origin archive version cannot be overwritten");
  Dictionary dict (base.get ()); dict.put ("version", g_variant_new_uint32 (1));
  preserve_records (dict, object);
  return encode (origin_name, dict.end ());
}
Parasite image_record (GimpImage *image)
{
  const auto *existing = gimp_image_parasite_find (image, image_name);
  Variant base = decode (existing);
  if (existing && !base) return Parasite (gimp_parasite_copy (existing));
  Dictionary dict (base.get ());
  dict.put ("version", g_variant_new_uint32 (1));
  dict.put ("dialect", g_variant_new_string ("gimp-painter-standard-v1"));
  preserve_records (dict, G_OBJECT (image));
  return encode (image_name, dict.end ());
}
Parasite item_record (GimpImage *image, GimpItem *item, GHashTable *ids)
{
  const auto *existing = gimp_item_parasite_find (item, item_name);
  Variant base = decode (existing);
  if (existing && (!base || !supported_item_record (base.get (), item)))
    {
      if (GIMP_IS_CLONE_LAYER (item) || GIMP_IS_FILTER_LAYER (item) ||
          (GIMP_IS_LAYER (item) && gimp_painter_layer_mode_is_compatibility (gimp_layer_get_mode (GIMP_LAYER (item)))))
        fail ("Unknown Painter extension version, kind or invalid semantic state cannot be overwritten with edited custom semantics");
      return Parasite (gimp_parasite_copy (existing));
    }
  Dictionary dict (base.get ());
  dict.put ("version", g_variant_new_uint32 (1));
  dict.put ("id", g_variant_new_uint32 (GPOINTER_TO_UINT (g_hash_table_lookup (ids, item))));
  preserve_records (dict, G_OBJECT (item));
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
          merge_external_origins (dict, G_OBJECT (item), incoming.get ());
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
      nullable_string (dict, "pending-name", ref->pending_name);
      nullable_string (dict, "source-name", ref->source_name);
      String original (gimp_painter_provenance_dup_text (G_OBJECT (item), GIMP_PAINTER_PROVENANCE_NAME));
      if (original) nullable_string (dict, "original-source-name", original.get ());
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
      nullable_string (dict, "procedure", name); g_free (name);
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
          Variant encoded_args (GimpPainterXcf::encode_snapshot (args.get (), image, ids, &external));
          Variant external_origins (external);
          merge_external_origins (dict, G_OBJECT (item), external_origins.get ());
          dict.put ("arguments", encoded_args.get ());
        }
      dict.put ("generation", g_variant_new_uint64 (state.generation));
      dict.put ("cache-generation", g_variant_new_uint64 (state.cache_generation));
      dict.put ("cache-complete", g_variant_new_boolean (state.cache_complete));
      dict.put ("saved-state", g_variant_new_uint32 (state.state));
    }
  else dict.put ("kind", g_variant_new_string ("ordinary"));
  return encode (item_name, dict.end ());
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
  Parasite image;
  std::unordered_map<GimpItem *, Parasite> items;
  std::unordered_map<GObject *, Parasite> origins;
  std::unordered_map<GObject *, Bytes> external_origins;
  std::unordered_map<GimpDrawable *, Object> buffers;
  std::unordered_map<GimpCloneLayer *, Clone> clone_checks;
  std::unordered_map<GimpFilterLayer *, GimpFilterLayerSnapshot> filter_checks;
  std::vector<std::pair<Object, Connection>> observers;
  bool changed = false;
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
void save_notify (GObject *, GParamSpec *, gpointer data) { static_cast<XcfPainterSave *> (data)->changed = true; }
void save_buffer_changed (GeglBuffer *, const GeglRectangle *, gpointer data) { static_cast<XcfPainterSave *> (data)->changed = true; }
void save_dirty (GimpImage *, GimpDirtyMask, gpointer data) { static_cast<XcfPainterSave *> (data)->changed = true; }
void save_container_changed (GimpContainer *, GimpObject *, gpointer data) { static_cast<XcfPainterSave *> (data)->changed = true; }
void save_reordered (GimpContainer *, GimpObject *, gint, gpointer data) { static_cast<XcfPainterSave *> (data)->changed = true; }
}
extern "C" XcfPainterSave *xcf_painter_prepare_save (GimpImage *image, GError **error)
{
  try
    {
      std::unique_ptr<XcfPainterSave> result (new XcfPainterSave);
      result->image = image_record (image);
      if (auto origin = fallback_origin (G_OBJECT (image))) result->origins.emplace (G_OBJECT (image), std::move (origin));
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
      for (auto *item : all)
        {
          if (auto refusal = String (gimp_painter_provenance_dup_text (G_OBJECT (item), GIMP_PAINTER_PROVENANCE_SAVE_REFUSAL)))
            fail (refusal.get ());
          result->items.emplace (item, item_record (image, item, result->ids));
          {
            Variant record = decode (result->items.at (item).get ());
            if (record)
              {
                Variant external (g_variant_lookup_value (record.get (), external_field, G_VARIANT_TYPE ("aa{sv}")));
                if (external) result->external_origins.emplace (G_OBJECT (item), little_endian_bytes (external.get ()));
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
          if (auto origin = fallback_origin (G_OBJECT (item))) result->origins.emplace (G_OBJECT (item), std::move (origin));
          if (GIMP_IS_CLONE_LAYER (item) || GIMP_IS_FILTER_LAYER (item) ||
              (GIMP_IS_LAYER (item) && gimp_painter_layer_mode_is_compatibility (gimp_layer_get_mode (GIMP_LAYER (item)))))
            result->custom = true;
          if (GIMP_IS_DRAWABLE (item))
            result->buffers.emplace (GIMP_DRAWABLE (item), Object::adopt (G_OBJECT (gegl_buffer_dup (gimp_drawable_get_buffer (GIMP_DRAWABLE (item))))));
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
      for (const auto &item : result->items)
        {
          result->watch (G_OBJECT (item.first), "notify", G_CALLBACK (save_notify));
          if (auto *children = gimp_viewable_get_children (GIMP_VIEWABLE (item.first))) watch_container (children);
          if (GIMP_IS_DRAWABLE (item.first))
            result->watch (G_OBJECT (gimp_drawable_get_buffer (GIMP_DRAWABLE (item.first))), "changed", G_CALLBACK (save_buffer_changed));
        }
      return result.release ();
    }
  catch (const std::exception &exception)
    { g_set_error_literal (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, exception.what ()); return nullptr; }
  catch (...)
    { g_set_error_literal (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "Could not prepare Painter save snapshot"); return nullptr; }
}
extern "C" void xcf_painter_free_save (XcfPainterSave *save) { delete save; }
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
{ return save && save->image ? gimp_parasite_copy (save->image.get ()) : nullptr; }
extern "C" GimpParasite *xcf_painter_origin_parasite (XcfPainterSave *save, GObject *object)
{
  if (!save) return nullptr;
  const auto found = save->origins.find (object);
  return found == save->origins.end () ? nullptr : gimp_parasite_copy (found->second.get ());
}
extern "C" GimpParasite *xcf_painter_item_parasite (XcfPainterSave *save, GimpItem *item)
{
  if (!save) return nullptr;
  const auto found = save->items.find (item);
  return found == save->items.end () ? nullptr : gimp_parasite_copy (found->second.get ());
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
  Bytes raw (g_bytes_new_from_bytes (info->painter_source, begin, end - begin));
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
      const auto *parasite = gimp_item_parasite_find (GIMP_ITEM (*layer), item_name);
      if (info->painter_legacy || !parasite) return TRUE;
      Variant dict = decode (parasite);
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
        { load_warning (info, exception.what ()); }
      catch (...) { load_warning (info, "allocation failure"); }
    }
  g_list_free (layers);
}
