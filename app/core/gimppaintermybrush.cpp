/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gegl.h>
#include <gio/gio.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
extern "C" {
#include "core-types.h"
#include "gimppaintermybrush.h"
#include "gimptempbuf.h"
#include "gimptagged.h"
}
#include "gimp-painter-type-traits.hpp"
#include "painter/binding-store.hpp"
#include "painter/gimp-painter-binding.h"
#include "painter/resources.hpp"
#include "paint/painter-mypaint/resource.hpp"
#include <algorithm>
#include <string>

using namespace GimpPainter;
namespace GimpPainter {
template<> struct TypeTraits<GdkPixbuf> { static GType type () noexcept { return GDK_TYPE_PIXBUF; } };
}
static void tagged_init (GimpTaggedInterface *iface);
G_DEFINE_TYPE_WITH_CODE (GimpPainterMybrush, gimp_painter_mybrush, GIMP_TYPE_DATA,
                        G_IMPLEMENT_INTERFACE (GIMP_TYPE_TAGGED, tagged_init))
namespace {
struct BrushImpl {
  MyPaint::Resource resource;
  ObjectRef<GdkPixbuf> icon;
  void close () noexcept { icon.reset (); }
};
struct BrushSlot : SlotSpec<GimpPainterMybrush, BrushImpl> {};
BindingStore& store (GimpPainterMybrush *brush)
{
  if (!GIMP_IS_PAINTER_MYBRUSH (brush)) throw Error (GIMP_PAINTER_ERROR_WRONG_TYPE, "Expected painter MyPaint brush");
  return BindingStore::require (G_OBJECT (brush));
}
void constructed (GObject *object)
{
  if (G_OBJECT_CLASS (gimp_painter_mybrush_parent_class)->constructed)
    G_OBJECT_CLASS (gimp_painter_mybrush_parent_class)->constructed (object);
  auto *brush = GIMP_PAINTER_MYBRUSH (object);
  if (!brush->binding_failed) brush->binding_failed = !boundary<bool> (nullptr, false, [&] {
    BindingStore::require (object).activate (); return true;
  });
  if (brush->binding_failed) gimp_painter_binding_close (object, nullptr);
}
void dispose (GObject *object)
{ gimp_painter_binding_close (object, nullptr); G_OBJECT_CLASS (gimp_painter_mybrush_parent_class)->dispose (object); }
gboolean save (GimpData *data, GOutputStream *output, GError **error)
{
  return boundary<gboolean> (error, FALSE, [&] () -> gboolean {
    return store (GIMP_PAINTER_MYBRUSH (data)).read<BrushSlot> ([&] (const BrushImpl& impl) {
      auto snapshot = impl.resource;
      const char *name = gimp_object_get_name (data);
      snapshot.set_parent_brush_name (name ? name : "");
      return snapshot.save (output, nullptr, error);
    });
  });
}
const gchar *extension (GimpData *) { return ".myb"; }
void copy (GimpData *data, GimpData *source)
{
  boundary_void (nullptr, [&] {
    auto *from = GIMP_PAINTER_MYBRUSH (source), *to = GIMP_PAINTER_MYBRUSH (data);
    auto destination_lease = ObjectRef<GimpPainterMybrush>::retain (to);
    auto source_lease = ObjectRef<GimpPainterMybrush>::retain (from);
    auto snapshot = store (from).read<BrushSlot> ([] (const BrushImpl& impl) { return impl.resource; });
    auto icon = store (from).read<BrushSlot> ([] (const BrushImpl& impl) { return impl.icon; });
    store (to).with<BrushSlot> ([&] (BrushImpl& impl) { impl.resource = std::move (snapshot); impl.icon = std::move (icon); });
    gimp_data_dirty (data);
  });
}
gboolean get_size (GimpViewable *viewable, gint *width, gint *height)
{
  if (!width || !height) return FALSE;
  *width = *height = 0;
  return boundary<gboolean> (nullptr, FALSE, [&] () -> gboolean {
    return store (GIMP_PAINTER_MYBRUSH (viewable)).read<BrushSlot> ([&] (const BrushImpl& impl) {
      *width = impl.icon ? gdk_pixbuf_get_width (impl.icon.get ()) : 48;
      *height = impl.icon ? gdk_pixbuf_get_height (impl.icon.get ()) : 48;
      return TRUE;
    });
  });
}
GdkPixbuf *get_pixbuf (GimpViewable *viewable, GimpContext *, gint width, gint height, GeglColor *)
{
  if (width <= 0 || height <= 0) return nullptr;
  return boundary<GdkPixbuf *> (nullptr, nullptr, [&] {
    return store (GIMP_PAINTER_MYBRUSH (viewable)).read<BrushSlot> ([&] (const BrushImpl& impl) -> GdkPixbuf * {
      if (!impl.icon) return nullptr; // Native fallback icon, never fake rendered brush parity.
      const double scale = std::min (double (width)/gdk_pixbuf_get_width (impl.icon.get ()),
                                     double (height)/gdk_pixbuf_get_height (impl.icon.get ()));
      return gdk_pixbuf_scale_simple (impl.icon.get (),
        std::max (1, int (gdk_pixbuf_get_width (impl.icon.get ())*scale)),
        std::max (1, int (gdk_pixbuf_get_height (impl.icon.get ())*scale)), GDK_INTERP_BILINEAR);
    });
  });
}
GimpTempBuf *get_preview (GimpViewable *v, GimpContext *c, gint w, gint h, GeglColor *fg)
{
  return boundary<GimpTempBuf *> (nullptr, nullptr, [&] {
    auto icon = ObjectRef<GdkPixbuf>::adopt (get_pixbuf (v,c,w,h,fg));
    return icon ? gimp_temp_buf_new_from_pixbuf (icon.get (), nullptr) : nullptr;
  });
}
gchar *checksum (GimpTagged *tagged)
{
  return boundary<gchar *> (nullptr, nullptr, [&] {
    String json (gimp_painter_mybrush_dup_json (GIMP_PAINTER_MYBRUSH (tagged), nullptr));
    return json ? g_compute_checksum_for_string (G_CHECKSUM_SHA256, json.get (), -1) : nullptr;
  });
}
}
static void tagged_init (GimpTaggedInterface *iface) { iface->get_checksum = checksum; }
static void gimp_painter_mybrush_class_init (GimpPainterMybrushClass *klass)
{
  G_OBJECT_CLASS (klass)->constructed = constructed;
  G_OBJECT_CLASS (klass)->dispose = dispose;
  GIMP_DATA_CLASS (klass)->save = save;
  GIMP_DATA_CLASS (klass)->get_extension = extension;
  GIMP_DATA_CLASS (klass)->copy = copy;
  auto *v = GIMP_VIEWABLE_CLASS (klass);
  v->get_size = get_size; v->get_new_pixbuf = get_pixbuf; v->get_new_preview = get_preview;
  v->default_icon_name = "gimp-tool-mypaint-brush";
  g_signal_new ("settings-changed", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST, 0, nullptr, nullptr, nullptr, G_TYPE_NONE, 0);
}
static void gimp_painter_mybrush_init (GimpPainterMybrush *brush)
{
  brush->binding_failed = !boundary<bool> (nullptr, false, [&] {
    BindingStore::ensure (G_OBJECT (brush)).emplace<BrushSlot> (); return true;
  });
}
GimpData *gimp_painter_mybrush_new (GimpContext *, const gchar *name)
{
  if (!name || !*name) return nullptr;
  return boundary<GimpData *> (nullptr, nullptr, [&] {
    auto owner = ObjectRef<GimpPainterMybrush>::adopt (GIMP_PAINTER_MYBRUSH (g_object_new (GIMP_TYPE_PAINTER_MYBRUSH,
      "name", name, "mime-type", "application/x-mypaint-brush", nullptr)));
    if (owner.get ()->binding_failed) return static_cast<GimpData *> (nullptr);
    return GIMP_DATA (owner.release ());
  });
}
GList *gimp_painter_mybrush_load (GimpContext *context, GFile *file, GInputStream *input, GError **error)
{
  return boundary<GList *> (error, nullptr, [&] {
    if (!G_IS_FILE (file)) throw std::invalid_argument ("Expected brush GFile");
    String basename (g_file_get_basename (file)); std::string name (basename.get ());
    if (g_str_has_suffix (name.c_str (), ".myb")) name.resize (name.size ()-4);
    auto resource = MyPaint::Resource::load (input, nullptr, name);
    if (!resource.parent_brush_name ().empty ()) name = resource.parent_brush_name ();
    auto owner = ObjectRef<GimpPainterMybrush>::adopt (GIMP_PAINTER_MYBRUSH (gimp_painter_mybrush_new (context, name.c_str ())));
    if (!owner) throw Error (GIMP_PAINTER_ERROR_INVALID_STATE, "Brush construction failed");
    store (owner.get ()).with<BrushSlot> ([&] (BrushImpl& impl) { impl.resource = std::move (resource); });
    // GdkPixbuf owns the decoded pixels including their real rowstride. No
    // Cairo surface can outlive a separately allocated backing byte array.
    const auto embedded = store (owner.get ()).read<BrushSlot> ([] (const BrushImpl& impl) { return impl.resource.preview_png_base64 (); });
    if (!embedded.empty ()) {
      gsize count = 0; std::unique_ptr<guchar, Free> bytes (g_base64_decode (embedded.c_str (), &count));
      auto input = ObjectRef<GObject>::adopt (G_OBJECT (g_memory_input_stream_new_from_data (bytes.get (), count, nullptr)));
      auto icon = ObjectRef<GdkPixbuf>::adopt (gdk_pixbuf_new_from_stream (G_INPUT_STREAM (input.get ()), nullptr, error));
      if (!icon) return static_cast<GList *> (nullptr);
      store (owner.get ()).with<BrushSlot> ([&] (BrushImpl& impl) { impl.icon = std::move (icon); });
    }
    String path (g_file_get_path (file));
    if (embedded.empty () && path && g_str_has_suffix (path.get (), ".myb")) {
      std::string icon_path (path.get ()); icon_path.resize (icon_path.size ()-4); icon_path += "_prev.png";
      auto icon = ObjectRef<GdkPixbuf>::adopt (gdk_pixbuf_new_from_file (icon_path.c_str (), nullptr));
      if (icon && !gimp_painter_mybrush_set_icon (owner.get (), icon.get (), error)) return static_cast<GList *> (nullptr);
    }
    gimp_data_clean (GIMP_DATA (owner.get ()));
    return g_list_prepend (nullptr, owner.release ());
  });
}
gchar *gimp_painter_mybrush_dup_json (GimpPainterMybrush *brush, GError **error)
{
  return boundary<gchar *> (error, nullptr, [&] {
    return store (brush).read<BrushSlot> ([] (const BrushImpl& impl) { return g_strdup (impl.resource.encode ().c_str ()); });
  });
}
gboolean gimp_painter_mybrush_set_json (GimpPainterMybrush *brush, const gchar *json, GError **error)
{
  return boundary<gboolean> (error, FALSE, [&] () -> gboolean {
    if (!json) throw std::invalid_argument ("Expected brush JSON");
    auto owner = ObjectRef<GimpPainterMybrush>::retain (brush);
    auto resource = MyPaint::Resource::decode (json); // Parse before mutating live state.
    ObjectRef<GdkPixbuf> icon;
    const auto encoded = resource.preview_png_base64 ();
    if (!encoded.empty ()) {
      gsize size = 0; std::unique_ptr<guchar, Free> bytes (g_base64_decode (encoded.c_str (), &size));
      auto input = ObjectRef<GObject>::adopt (G_OBJECT (g_memory_input_stream_new_from_data (bytes.get (), size, nullptr)));
      icon = ObjectRef<GdkPixbuf>::adopt (gdk_pixbuf_new_from_stream (G_INPUT_STREAM (input.get ()), nullptr, error));
      if (!icon) return FALSE;
    }
    store (brush).with<BrushSlot> ([&] (BrushImpl& impl) { impl.resource = std::move (resource); impl.icon = std::move (icon); });
    gimp_data_dirty (GIMP_DATA (brush)); g_signal_emit_by_name (brush, "settings-changed"); return TRUE;
  });
}
gboolean gimp_painter_mybrush_set_icon (GimpPainterMybrush *brush, GdkPixbuf *pixbuf, GError **error)
{
  return boundary<gboolean> (error, FALSE, [&] () -> gboolean {
    auto owner = ObjectRef<GimpPainterMybrush>::retain (brush);
    auto icon = ObjectRef<GdkPixbuf>::retain (pixbuf); // Retain before replacing, including same-object assignment.
    std::string encoded;
    if (icon) {
      gchar *raw = nullptr; gsize size = 0;
      if (!gdk_pixbuf_save_to_buffer (icon.get (), &raw, &size, "png", error, nullptr)) return FALSE;
      String bytes (raw); String base64 (g_base64_encode (reinterpret_cast<const guchar *> (raw), size));
      encoded = base64.get ();
    }
    store (brush).with<BrushSlot> ([&] (BrushImpl& impl) {
      impl.resource.set_preview_png_base64 (encoded); impl.icon = std::move (icon);
    });
    gimp_viewable_size_changed (GIMP_VIEWABLE (brush)); gimp_data_dirty (GIMP_DATA (brush)); return TRUE;
  });
}
GdkPixbuf *gimp_painter_mybrush_ref_icon (GimpPainterMybrush *brush)
{
  return boundary<GdkPixbuf *> (nullptr, nullptr, [&] {
    return store (brush).read<BrushSlot> ([] (const BrushImpl& impl) { auto owner = impl.icon; return owner.release (); });
  });
}
gchar *gimp_painter_mybrush_dup_diagnostics (GimpPainterMybrush *brush)
{
  return boundary<gchar *> (nullptr, nullptr, [&] {
    return store (brush).read<BrushSlot> ([] (const BrushImpl& impl) {
      std::string text;
      for (const auto& d : impl.resource.diagnostics ()) text += d.path+": "+d.message+"\n";
      return g_strdup (text.c_str ());
    });
  });
}
