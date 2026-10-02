/* SPDX-License-Identifier: GPL-3.0-or-later
 * Application adapter for the side-effect-free byte decoder. Input is copied
 * in bounded chunks into a private temporary file, then mapped read-only. The
 * map is shared by the construction stream and retained original-data model.
 * Thus large pixel payloads don't become a proportional heap allocation, and
 * candidate attempts cannot observe concurrent changes to the original file.
 */
#include "config.h"
#include <gegl.h>
#include <gio/gio.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <cstring>
#include <algorithm>
#include <memory>
extern "C" {
#include "libgimpbase/gimpbase.h"
#include "core/core-types.h"
#include "core/gimp.h"
#include "core/gimpimage.h"
#include "core/gimpimage-undo.h"
#include "core/gimpclonelayer.h"
#include "core/gimpfilterlayer.h"
#include "core/gimpprogress.h"
#include "xcf-private.h"
#include "painter-xcf-load.h"
#include "painter-xcf-preserve.h"
#include "xcf-seek.h"
#include "operations/layer-modes-legacy/gimpoperationpainterlegacy.h"
}
#include "painter-xcf-compat.hpp"
namespace compat = gimp::painter::xcf;
namespace {
const char original_key[] = "gimp-painter-xcf-original";
const char offset_key[] = "gimp-painter-xcf-original-offset";
struct Snapshot
{
  GFile *file = nullptr;
  GFileIOStream *io = nullptr;
  GMappedFile *map = nullptr;
  ~Snapshot ()
  {
    if (map) g_mapped_file_unref (map);
    if (io) { g_io_stream_close (G_IO_STREAM (io), nullptr, nullptr); g_object_unref (io); }
    if (file) { g_file_delete (file, nullptr, nullptr); g_object_unref (file); }
  }
};
void free_snapshot (gpointer p) { delete static_cast<Snapshot *> (p); }
GBytes *snapshot (GInputStream *input, GCancellable *cancel, GError **error)
{
  std::unique_ptr<Snapshot> state (new Snapshot);
  state->file = g_file_new_tmp ("gimp-xcf-read-XXXXXX", &state->io, error);
  if (!state->file) return nullptr;
  GOutputStream *output = g_io_stream_get_output_stream (G_IO_STREAM (state->io));
  guint8 chunk[65536];
  for (;;)
    {
      const gssize count = g_input_stream_read (input, chunk, sizeof chunk, cancel, error);
      if (count < 0) return nullptr;
      if (!count) break;
      if (!g_output_stream_write_all (output, chunk, count, nullptr, cancel, error)) return nullptr;
    }
  if (!g_output_stream_flush (output, cancel, error)) return nullptr;
  gchar *path = g_file_get_path (state->file);
  state->map = g_mapped_file_new (path, FALSE, error);
  g_free (path);
  if (!state->map) return nullptr;
  /* POSIX can unlink a mapped open file immediately (also crash-safe). On
   * platforms that cannot, Snapshot retries deletion after unmapping/closing. */
  if (g_file_delete (state->file, nullptr, nullptr)) g_clear_object (&state->file);
  GBytes *bytes = g_bytes_new_with_free_func (g_mapped_file_get_contents (state->map),
                                             g_mapped_file_get_length (state->map),
                                             free_snapshot, state.get ());
  state.release ();
  return bytes;
}
compat::Bytes source (XcfInfo *info)
{
  gsize length = 0;
  auto *data = static_cast<const guint8 *> (g_bytes_get_data (info->painter_source, &length));
  return {data, length};
}
gchar *normalized_name (compat::Bytes bytes, const compat::WireString &string)
{
  if (!string.bytes.size) return nullptr;
  gchar *raw = static_cast<gchar *> (g_memdup2 (bytes.data + string.bytes.offset, string.bytes.size));
  raw[string.bytes.size - 1] = '\0'; /* historical reader behavior */
  gchar *name = gimp_any_to_utf8 (raw, -1, "Invalid UTF-8 string in XCF file");
  g_free (raw);
  return name;
}
/* Only safe, recorded scalar values enter the executor. Omitted image/drawable
 * slots in the known edge procedure are context placeholders, not fabricated
 * IDs: the independent executor obtains its image and input from the layer. */
GimpValueArray *execution_args (const compat::Extension &ext, const gchar *name)
{
  if (ext.diagnostic.status != compat::Status::ok || !name ||
      (std::strcmp (name, "plug-in-edge") != 0 && std::strcmp (name, "plug-in-gauss") != 0) ||
      (ext.arguments.size () != 6 && ext.arguments.size () != 7)) return nullptr;
  GimpValueArray *args = gimp_value_array_new (ext.arguments.size () - 1);
  for (std::size_t i = 0; i + 1 < ext.arguments.size (); ++i)
    {
      const auto &arg = ext.arguments[i];
      GValue value = G_VALUE_INIT;
      if (i < 3 && (arg.kind == compat::ArgumentKind::unsupported ||
                    arg.kind == compat::ArgumentKind::omitted_drawable))
        { g_value_init (&value, G_TYPE_INT); g_value_set_int (&value, 0); }
      else if (arg.kind == compat::ArgumentKind::int32_bits)
        { g_value_init (&value, G_TYPE_INT); g_value_set_int (&value, static_cast<gint32> (arg.scalar_bits)); }
      else if (arg.kind == compat::ArgumentKind::float32_bits)
        {
          float f;
          std::memcpy (&f, &arg.scalar_bits, sizeof f);
          g_value_init (&value, G_TYPE_DOUBLE); g_value_set_double (&value, f);
        }
      else { gimp_value_array_unref (args); return nullptr; }
      gimp_value_array_append (args, &value);
      g_value_unset (&value);
    }
  return args;
}
/* Old nested readers ignore the outer declared length. Validate their actual
 * consumption, then the following layer properties and hierarchy references.
 * A bounded window limits work, not acceptance: exhausting it is inconclusive.
 * This distinguishes current standard scalar tags32/33 without a length-only
 * guess, while preserving truly dual-valid and outer-length-crossing files. */
enum class ExtensionEvidence { complete, invalid, inconclusive };
compat::Extension historical_extension (compat::Bytes bytes, std::size_t offset, guint32 tag)
{
  compat::Limits limits;
  const compat::Range window {offset, std::min (bytes.size - offset, limits.max_property_bytes)};
  return tag == 32 ? compat::decode_filter (bytes, window, compat::FilterPolicy::legacy_reader, limits)
                   : compat::decode_clone (bytes, window, limits);
}
bool terminated (const compat::Extension &ext)
{
  return !ext.arguments.empty () && ext.arguments.back ().kind == compat::ArgumentKind::terminator;
}
ExtensionEvidence legacy_extensions (compat::Bytes bytes, const compat::Candidate &candidate)
{
  std::size_t records = 0;
  const auto read32 = [&] (std::size_t at) {
    const auto *p = bytes.data + at;
    return (guint32 (p[0]) << 24) | (guint32 (p[1]) << 16) | (guint32 (p[2]) << 8) | p[3];
  };
  for (const auto &object : candidate.layers)
    {
      std::size_t pos = object.name.encoded.offset + object.name.encoded.size;
      for (;;)
        {
          if (++records > 100000) return ExtensionEvidence::inconclusive;
          if (pos > bytes.size || bytes.size - pos < 8) return ExtensionEvidence::invalid;
          const guint32 tag = read32 (pos), declared = read32 (pos + 4);
          pos += 8;
          if (!tag) break;
          if (tag == 32 || tag == 33)
            {
              auto ext = historical_extension (bytes, pos, tag);
              records += ext.arguments.size ();
              if (terminated (ext)) { pos = ext.tail.offset; continue; }
              if (ext.diagnostic.status == compat::Status::limit ||
                  ext.diagnostic.status == compat::Status::allocation_failure ||
                  bytes.size - pos > compat::Limits ().max_property_bytes)
                return ExtensionEvidence::inconclusive;
              return ExtensionEvidence::invalid;
            }
          std::size_t count = declared;
          if (tag == 2 || tag == 29) count = 0;
          else if ((tag >= 5 && tag <= 13) || tag == 20 || tag == 26 || tag == 28 || tag == 31) count = 4;
          else if (tag == 15) count = 8;
          if (count > bytes.size - pos) return ExtensionEvidence::invalid;
          pos += count;
        }
      if (bytes.size - pos < 8) return ExtensionEvidence::invalid;
      const guint32 hierarchy = read32 (pos), mask = read32 (pos + 4);
      if (hierarchy >= bytes.size || bytes.size - hierarchy < 12 ||
          (mask && (mask >= bytes.size || bytes.size - mask < 12)))
        return ExtensionEvidence::invalid;
    }
  return ExtensionEvidence::complete;
}

}

extern "C" gboolean
xcf_painter_prepare (XcfInfo *info, XcfPainterDialect requested, GError **error)
{
  try
    {
      info->painter_source = snapshot (info->input, info->painter_cancellable, error);
      if (!info->painter_source) return FALSE;
      const auto bytes = source (info);
      if (bytes.size < 26)
        { g_set_error_literal (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "Truncated XCF header"); return FALSE; }
      const auto result = compat::probe (bytes);
      gint declared_dialect = -1;
      /* An explicit versioned namespace identifies our standard-XCF extension
       * independently of structural ambiguity or later object-work limits.
       * Require the complete image-property sequence so later duplicates can
       * override earlier markers exactly as the application reader does. */
      if (!result.standard.image_properties.empty () && result.standard.image_properties.back ().tag == 0)
        for (const auto &property : result.standard.image_properties)
          if (property.tag == 21)
            {
              const gint marker = xcf_painter_provenance_in_parasites (info->painter_source,
                                             property.payload.offset, property.payload.size);
              if (marker >= 0) declared_dialect = marker;
            }
      if (requested == XCF_PAINTER_DIALECT_AUTO && declared_dialect == 1)
        info->painter_legacy = FALSE;
      else if (requested == XCF_PAINTER_DIALECT_AUTO)
        {
          switch (result.selection)
            {
            case compat::Selection::legacy_candidate:
              info->painter_legacy = TRUE;
              break;
            case compat::Selection::shared_layout:
              info->painter_historical_modes = TRUE;
              break;
            case compat::Selection::standard_candidate:
            case compat::Selection::unsupported:
              break;
            default:
              if (result.selection == compat::Selection::ambiguous &&
                  result.standard.diagnostic.status == compat::Status::ok &&
                  legacy_extensions (bytes, result.legacy) == ExtensionEvidence::invalid)
                break;
              /* No old dialect exists above v4. Keep the ordinary upstream
               * recovery route for such standard files even if probe limits
               * are reached or their metadata is damaged. */
              if (result.legacy.diagnostic.status == compat::Status::unsupported)
                break;
              g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                           "XCF dialect could not be selected safely. In Open, select the file type "
                           "'GIMP XCF image (standard recovery)' or 'GIMP Painter XCF image (legacy recovery)' "
                           "to retry without modifying the original. Standard: %s (byte %" G_GSIZE_FORMAT "). "
                           "Painter: %s (byte %" G_GSIZE_FORMAT ").",
                           result.standard.diagnostic.message, result.standard.diagnostic.offset,
                           result.legacy.diagnostic.message, result.legacy.diagnostic.offset);
              return FALSE;
            }
        }
      else info->painter_legacy = requested == XCF_PAINTER_DIALECT_LEGACY;
      if (info->painter_legacy) info->painter_historical_modes = TRUE;
      if (info->painter_legacy && result.legacy.diagnostic.status == compat::Status::unsupported)
        { g_set_error_literal (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "Legacy Painter recovery supports XCF versions 0 through 4"); return FALSE; }
      info->input = g_memory_input_stream_new_from_bytes (info->painter_source);
      info->seekable = G_SEEKABLE (info->input);
      return TRUE;
    }
  catch (...)
    { g_set_error_literal (error, G_FILE_ERROR, G_FILE_ERROR_NOMEM, "Not enough memory to prepare XCF input; original file was not changed"); return FALSE; }
}

extern "C" gboolean
xcf_painter_load_extension (XcfInfo *info, GimpImage *image, GimpLayer **layer,
                             guint32 tag, guint32 size)
{
  try
    {
      const auto bytes = source (info);
      if (info->cp < 0 || static_cast<guint64> (info->cp) > bytes.size) return FALSE;
      compat::Range range {static_cast<std::size_t> (info->cp), size};
      auto ext = historical_extension (bytes, range.offset, tag);
      if (terminated (ext))
        {
          range.size = ext.tail.offset - range.offset;
          /* Trailing bytes belong to subsequent layer records here. */
          ext.diagnostic = {};
        }
      else
        {
          if (size > bytes.size - range.offset) return FALSE;
          ext = tag == 32 ? compat::decode_filter (bytes, range) : compat::decode_clone (bytes, range);
        }
      /* A malformed/unsupported definition is retained as an inert independent
       * layer, with saved pixels. No invalid GValue or guessed argument is used. */
      gchar *name = normalized_name (bytes, ext.name);
      GimpLayer *old = *layer;
      const int x = gimp_item_get_offset_x (GIMP_ITEM (old));
      const int y = gimp_item_get_offset_y (GIMP_ITEM (old));
      const int width = gimp_item_get_width (GIMP_ITEM (old));
      const int height = gimp_item_get_height (GIMP_ITEM (old));
      GimpLayer *replacement = tag == 32
        ? gimp_filter_layer_new (image, width, height, gimp_object_get_name (old),
                                  gimp_layer_get_opacity (old), gimp_layer_get_mode (old))
        : gimp_clone_layer_new (image, nullptr, width, height, gimp_object_get_name (old),
                                 gimp_layer_get_opacity (old), gimp_layer_get_mode (old));
      if (!replacement) { g_free (name); return FALSE; }
      gimp_item_set_offset (GIMP_ITEM (replacement), x, y);
      gimp_item_set_visible (GIMP_ITEM (replacement), gimp_item_get_visible (GIMP_ITEM (old)), FALSE);
      GeglBuffer *buffer = gegl_buffer_new (GEGL_RECTANGLE (0, 0, width, height),
                                           gimp_drawable_get_format (GIMP_DRAWABLE (old)));
      gimp_drawable_set_buffer (GIMP_DRAWABLE (replacement), FALSE, nullptr, buffer);
      g_object_unref (buffer);
      GBytes *raw = g_bytes_new_from_bytes (info->painter_source, range.offset, range.size);
      gboolean ok;
      if (tag == 32)
        {
          GimpValueArray *args = execution_args (ext, name);
          ok = gimp_filter_layer_set_definition (GIMP_FILTER_LAYER (replacement), name, raw, args, nullptr);
          if (args) gimp_value_array_unref (args);
        }
      else ok = gimp_clone_layer_set_source_name_full (GIMP_CLONE_LAYER (replacement), name, nullptr);
      g_object_set_data_full (G_OBJECT (replacement), "gimp-painter-xcf-extension", raw,
                              reinterpret_cast<GDestroyNotify> (g_bytes_unref));
      g_object_set_data_full (G_OBJECT (replacement), "gimp-painter-xcf-original-name", name, g_free);
      if (!ok)
        { g_object_ref_sink (replacement); g_object_unref (replacement); return FALSE; }
      for (GList *p = info->selected_layers; p; p = p->next) if (p->data == old) p->data = replacement;
      for (GList *p = info->linked_layers; p; p = p->next) if (p->data == old) p->data = replacement;
      if (info->floating_sel == old) info->floating_sel = nullptr;
      g_object_ref_sink (old); g_object_unref (old);
      *layer = replacement;
      return xcf_seek_pos (info, range.offset + range.size, nullptr);
    }
  catch (...) { return FALSE; }
}

extern "C" gboolean
xcf_painter_load_mode (GimpLayer *layer, guint32 raw)
{
  GimpLayerMode mode;
  if (!gimp_painter_layer_mode_from_legacy (raw, &mode)) return FALSE;
  /* Historical Overlay was the same broken operation as Soft Light. */
  if (mode == GIMP_LAYER_MODE_OVERLAY_LEGACY) mode = GIMP_LAYER_MODE_SOFTLIGHT_LEGACY;
  gimp_layer_set_mode (layer, mode, FALSE);
  return TRUE;
}

extern "C" void
xcf_painter_finish_image (XcfInfo *info, GimpImage *image)
{
  g_object_set_data_full (G_OBJECT (image), original_key, g_bytes_ref (info->painter_source),
                          reinterpret_cast<GDestroyNotify> (g_bytes_unref));
  g_object_set_data (G_OBJECT (image), "gimp-painter-xcf-dialect",
                     GINT_TO_POINTER (info->painter_legacy ? XCF_PAINTER_DIALECT_LEGACY : XCF_PAINTER_DIALECT_STANDARD));
  gimp_image_undo_freeze (image);
  GList *layers = gimp_image_get_layer_list (image);
  for (GList *p = layers; p; p = p->next)
    {
      if (GIMP_IS_CLONE_LAYER (p->data) &&
          !g_object_get_data (G_OBJECT (p->data), "gimp-painter-xcf-modern-definition")) gimp_clone_layer_get_source (GIMP_CLONE_LAYER (p->data));
      /* Native v1 caches receive freshness only from their validated snapshot,
       * after definition restoration. Legacy files have no generation record.
       */
      if (GIMP_IS_FILTER_LAYER (p->data) &&
          !g_object_get_data (G_OBJECT (p->data), "gimp-painter-xcf-modern-definition"))
        gimp_filter_layer_mark_as_loaded (GIMP_FILTER_LAYER (p->data));
    }
  g_list_free (layers);
  xcf_painter_restore_bindings (info, image);
  gimp_image_undo_thaw (image);
}
extern "C" GBytes *xcf_painter_ref_original (GimpImage *image)
{
  auto *bytes = static_cast<GBytes *> (g_object_get_data (G_OBJECT (image), original_key));
  return bytes ? g_bytes_ref (bytes) : nullptr;
}
extern "C" void xcf_painter_record_offset (GObject *object, goffset offset)
{
  auto *value = g_new (guint64, 1); *value = offset;
  g_object_set_data_full (object, offset_key, value, g_free);
}
extern "C" gboolean xcf_painter_original_offset (GObject *object, guint64 *offset)
{
  auto *value = static_cast<guint64 *> (g_object_get_data (object, offset_key));
  if (!value) return FALSE;
  if (offset) *offset = *value;
  return TRUE;
}
