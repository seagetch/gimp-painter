/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gegl.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
extern "C" {
#include "libgimpbase/gimpbase.h"
#include "core/core-types.h"
#include "core/gimp.h"
#include "core/gimpimage.h"
#include "core/gimpitem.h"
#include "core/gimplayer.h"
#include "core/gimplayermask.h"
#include "core/gimpchannel.h"
#include "core/gimp-painter-provenance.h"
#include "xcf-private.h"
#include "xcf-seek.h"
#include "painter-xcf-preserve.h"
}
#include "painter-xcf-multipart.hpp"
#include <algorithm>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <vector>
namespace {
using namespace GimpPainterXcf;
const guint8 v2_magic[12] = {'G','P','X','C','F',0,0,0,2,0,0,0};
struct Raw { const char *name; const guint8 *data; gsize name_size, size, total; guint32 flags; };
guint32 be32 (const guint8 *p)
{ return (guint32 (p[0]) << 24) | (guint32 (p[1]) << 16) | (guint32 (p[2]) << 8) | p[3]; }
bool parse (GBytes *source, gsize offset, gsize end, Raw& out)
{
  gsize size; const auto *data = static_cast<const guint8 *> (g_bytes_get_data (source, &size));
  if (end > size || offset > end || end - offset < 12) return false;
  const auto *p = data + offset; const gsize name = be32 (p);
  if (!name || name > end - offset - 12) return false;
  const gsize length = be32 (p + 8 + name);
  if (length > end - offset - name - 12) return false;
  out = {reinterpret_cast<const char *> (p + 4), p + 12 + name, name, length, 12 + name + length, be32 (p + 4 + name)};
  return true;
}
int semantic (const Raw& raw)
{
  for (guint n = 0; n < 3; ++n)
    {
      const char *name = namespace_name (Namespace (n));
      if (raw.name_size == std::strlen (name) + 1 && !std::memcmp (raw.name, name, raw.name_size)) return n;
    }
  return -1;
}
bool inert (const Raw& raw)
{
  static const char prefix[] = "gimp-painter-inert-v1/";
  return raw.name_size >= sizeof prefix && !std::memcmp (raw.name, prefix, sizeof prefix - 1);
}
bool chunk (const Raw& raw)
{
  static const char prefix[] = "gimp-painter-chunk-v1/";
  return raw.name_size >= sizeof prefix && !std::memcmp (raw.name, prefix, sizeof prefix - 1);
}
int chunk_space (const Raw& raw)
{
  constexpr gsize prefix = sizeof ("gimp-painter-chunk-v1/") - 1;
  if (!chunk (raw) || raw.name_size < prefix + 3 || raw.name[prefix] < '0' || raw.name[prefix] > '2' || raw.name[prefix + 1] != '/') return -1;
  return raw.name[prefix] - '0';
}
OwnerIdentity identity (GObject *owner)
{
  if (GIMP_IS_IMAGE (owner)) return {OwnerClass::image, 0};
  OwnerClass type = GIMP_IS_LAYER_MASK (owner) ? OwnerClass::layer_mask : GIMP_IS_LAYER (owner) ? OwnerClass::layer :
                    GIMP_IS_CHANNEL (owner) ? OwnerClass::channel : OwnerClass::path;
  return {type, gimp_item_get_tattoo (GIMP_ITEM (owner))};
}
void report (XcfInfo *info, const char *message)
{ gimp_message (info->gimp, G_OBJECT (info->progress), GIMP_MESSAGE_WARNING, "Painter metadata remains inert: %s", message); }
}
extern "C" gint xcf_painter_intercept_parasite (XcfInfo *info, GPtrArray *records, goffset end)
{
  if (!info->painter_source || info->cp < 0 || end < info->cp) return 0;
  Raw raw;
  if (!parse (info->painter_source, info->cp, end, raw)) return 0;
  if (semantic (raw) < 0 && !chunk (raw) && !inert (raw)) return 0;
  GBytes *record = g_bytes_new_from_bytes (info->painter_source, info->cp, raw.total);
  if (!xcf_seek_pos (info, info->cp + raw.total, nullptr)) { g_bytes_unref (record); return -1; }
  g_ptr_array_add (records, record); return 1;
}
extern "C" gboolean xcf_painter_finish_transport (XcfInfo *info, GObject *owner, GPtrArray *records)
{
  if (!records || !records->len) return TRUE;
  try
    {
      std::vector<Bytes> all; all.reserve (records->len);
      std::array<guint, 3> semantics {{0,0,0}}, chunks {{0,0,0}};
      for (guint i = 0; i < records->len; ++i)
        {
          auto *bytes = static_cast<GBytes *> (g_ptr_array_index (records, i)); Raw raw;
          if (!parse (bytes, 0, g_bytes_get_size (bytes), raw)) throw std::runtime_error ("Invalid retained parasite framing");
          const int n = semantic (raw), c = chunk_space (raw);
          if (n >= 0) ++semantics[n];
          if (c >= 0) ++chunks[c];
        }
      std::vector<Bytes> captured;
      for (guint i = 0; i < records->len; ++i)
        captured.push_back (Bytes::retain (static_cast<GBytes *> (g_ptr_array_index (records, i))));
      const auto protected_records = multipart_protected_records (captured, info->painter_cancellable);
      /* A sole old v1/future inline parasite keeps the original native path.
       * Duplicates or a mixture with chunks retain exact raw ordering instead. */
      for (guint i = 0; i < records->len; ++i)
        {
          auto *bytes = static_cast<GBytes *> (g_ptr_array_index (records, i)); Raw raw;
          parse (bytes, 0, g_bytes_get_size (bytes), raw); const int n = semantic (raw);
          if (n >= 0 && !protected_records[i] && semantics[n] == 1 && !chunks[n] &&
              (raw.size != 76 || std::memcmp (raw.data, v2_magic, sizeof v2_magic)) && raw.size <= 256u * 1024u * 1024u)
            {
              GimpParasite *parasite = gimp_parasite_new (raw.name, raw.flags, raw.size, raw.data);
              GError *error = nullptr;
              if (GIMP_IS_IMAGE (owner))
                { if (gimp_image_parasite_validate (GIMP_IMAGE (owner), parasite, &error)) gimp_image_parasite_attach (GIMP_IMAGE (owner), parasite, FALSE); }
              else
                { if (gimp_item_parasite_validate (GIMP_ITEM (owner), parasite, &error)) gimp_item_parasite_attach (GIMP_ITEM (owner), parasite, FALSE); }
              gimp_parasite_free (parasite);
              if (error) { report (info, error->message); g_clear_error (&error); all.push_back (Bytes::retain (bytes)); }
            }
          else all.push_back (Bytes::retain (bytes));
        }
      if (all.empty ()) return TRUE;
      auto restored = multipart_restore (identity (owner), all, info->painter_cancellable);
      GimpPainterProvenanceTransport state = {};
      state.records = g_ptr_array_new_with_free_func (reinterpret_cast<GDestroyNotify> (g_bytes_unref));
      std::unique_ptr<GPtrArray, decltype (&g_ptr_array_unref)> held (state.records, g_ptr_array_unref);
      std::vector<guint8> dispositions (all.size (), 0);
      for (gsize i = 0; i < all.size (); ++i)
        {
          g_ptr_array_add (state.records, g_bytes_ref (all[i].get ()));
          if (restored.consumed[i]) { Raw raw; parse (all[i].get (), 0, g_bytes_get_size (all[i].get ()), raw);
            const int n = semantic (raw), c = chunk_space (raw); dispositions[i] = (n >= 0 ? n : c) + 1; }
        }
      auto flags = Bytes::adopt (g_bytes_new (dispositions.data (), dispositions.size ())); state.dispositions = flags.get ();
      for (guint n = 0; n < 3; ++n)
        { state.capsules[n] = restored.capsules[n].get (); state.present[n] = restored.present[n]; state.invalid[n] = restored.invalid[n]; }
      if (!gimp_painter_provenance_set_transport (owner, &state)) throw std::runtime_error ("Could not retain Painter transport records");
      return TRUE;
    }
  catch (const std::exception& error) { report (info, error.what ()); return FALSE; }
  catch (...) { report (info, "Could not restore transport records"); return FALSE; }
}
/* Filter only intercepted inner records from the existing raw property span.
 * Ordinary mixed parasites are reframed once; no transport archive nests in
 * its own original-properties dictionary on repeated saves. */
extern "C" GBytes *xcf_painter_without_transport (XcfInfo *info, GObject *owner, goffset begin, goffset end)
{
  if (!info->painter_source || begin < 0 || end < begin || guint64 (end) > g_bytes_get_size (info->painter_source)) return nullptr;
  std::unique_ptr<GimpPainterProvenanceTransport, decltype (&gimp_painter_provenance_free_transport)>
    transport (gimp_painter_provenance_ref_transport (owner), gimp_painter_provenance_free_transport);
  if (info->painter_legacy || !transport || !transport->records || !transport->records->len)
    return g_bytes_new_from_bytes (info->painter_source, begin, end - begin);
  try
    {
      std::vector<Bytes> pieces; gsize offset = begin; guint record_index = 0; bool changed = false;
      const auto *data = static_cast<const guint8 *> (g_bytes_get_data (info->painter_source, nullptr));
      while (offset < gsize (end))
        {
          if (gsize (end) - offset < 8) throw std::runtime_error ("Truncated property framing");
          const guint32 type = be32 (data + offset), size = be32 (data + offset + 4);
          if (size > gsize (end) - offset - 8) throw std::runtime_error ("Property exceeds captured span");
          const gsize next = offset + 8 + size;
          if (type != PROP_PARASITES)
            pieces.push_back (Bytes::adopt (g_bytes_new_from_bytes (info->painter_source, offset, next - offset)));
          else
            {
              std::vector<Bytes> kept; gsize at = offset + 8; guint32 kept_size = 0;
              while (at < next)
                {
                  Raw raw; if (!parse (info->painter_source, at, next, raw)) throw std::runtime_error ("Malformed inner parasite framing");
                  bool captured = false;
                  if (record_index < transport->records->len)
                    {
                      auto *record = static_cast<GBytes *> (g_ptr_array_index (transport->records, record_index));
                      captured = g_bytes_get_data (record, nullptr) == data + at && g_bytes_get_size (record) == raw.total;
                      if (captured) ++record_index;
                    }
                  if (captured) changed = true;
                  else { kept.push_back (Bytes::adopt (g_bytes_new_from_bytes (info->painter_source, at, raw.total))); kept_size += raw.total; }
                  at += raw.total;
                }
              if (kept_size)
                {
                  guint32 header[] = {GUINT32_TO_BE (PROP_PARASITES), GUINT32_TO_BE (kept_size)};
                  pieces.push_back (Bytes::adopt (g_bytes_new (header, sizeof header)));
                  for (auto& piece : kept) pieces.push_back (std::move (piece));
                }
            }
          offset = next;
        }
      if (!changed) return g_bytes_new_from_bytes (info->painter_source, begin, end - begin);
      return snapshot_parts (pieces, 0, info->painter_cancellable).release ();
    }
  catch (...) { return nullptr; }
}
