/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_MYBRUSH_HANDLE_HPP
#define GIMP_PAINTER_MYBRUSH_HANDLE_HPP
#include "../painter/gimp-painter-visibility.h"
extern "C" {
#include "gimppaintermybrush.h"
#include "gimpmypaintbrush.h"
#include "gimpmypaintbrush-load.h"
}
#include "gimp-painter-type-traits.hpp"
#include "painter/resources.hpp"
#include "paint/painter-mypaint/resource.hpp"
namespace GimpPainter GIMP_PAINTER_PRIVATE {
class PainterMybrushRef
{
public:
  static PainterMybrushRef retain (GimpPainterMybrush *brush)
  { return PainterMybrushRef (ObjectRef<GimpPainterMybrush>::retain (brush)); }
  static PainterMybrushRef adopt (GimpPainterMybrush *brush)
  { return PainterMybrushRef (ObjectRef<GimpPainterMybrush>::adopt (brush)); }
  static PainterMybrushRef create (const char *name)
  { return adopt (GIMP_PAINTER_MYBRUSH (gimp_mypaint_brush_new (nullptr, name))); }
  static PainterMybrushRef get_standard (GimpContext *context = nullptr)
  { return retain (GIMP_PAINTER_MYBRUSH (gimp_mypaint_brush_get_standard (context))); }
  static PainterMybrushRef load (const char *filename, GimpContext *context = nullptr)
  {
    GError *error = nullptr;
    GList *loaded = gimp_mypaint_brush_load (context, filename, &error);
    if (!loaded) fail (error);
    if (loaded->next || !GIMP_IS_PAINTER_MYBRUSH (loaded->data)) {
      g_list_free_full (loaded, g_object_unref);
      throw Error (GIMP_PAINTER_ERROR_INVALID_STATE, "Expected one loaded brush");
    }
    auto *brush = static_cast<GimpPainterMybrush *> (loaded->data);
    g_list_free (loaded);
    return adopt (brush);
  }
  GimpPainterMybrush *get () const noexcept { return owner_.get (); }
  PainterMybrushRef duplicate () const
  {
    auto *result = gimp_mypaint_brush_duplicate (get ());
    if (!result) throw Error (GIMP_PAINTER_ERROR_INVALID_STATE, "Brush resource duplication failed");
    return adopt (GIMP_PAINTER_MYBRUSH (result));
  }
  MyPaint::Resource snapshot () const
  {
    GError *error = nullptr; String json (gimp_painter_mybrush_dup_json (get (), &error));
    if (!json) fail (error);
    return MyPaint::Resource::decode (json.get ());
  }
  void replace (const MyPaint::Resource& resource) const
  {
    GError *error = nullptr;
    if (!gimp_painter_mybrush_set_json (get (), resource.encode ().c_str (), &error)) fail (error);
  }
private:
  explicit PainterMybrushRef (ObjectRef<GimpPainterMybrush> owner) : owner_ (std::move (owner))
  { if (!owner_) throw Error (GIMP_PAINTER_ERROR_WRONG_TYPE, "Expected painter MyPaint resource"); }
  [[noreturn]] static void fail (GError *error)
  {
    throw Error (GIMP_PAINTER_ERROR_INVALID_STATE,
                 take_error_message (error, "Brush resource operation failed").c_str ());
  }
  ObjectRef<GimpPainterMybrush> owner_;
};
/* Original operation spelling for rebuilt callers; one handle implementation. */
using MypaintBrushRef = PainterMybrushRef;
}
#endif
