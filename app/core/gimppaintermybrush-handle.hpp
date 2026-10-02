/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_MYBRUSH_HANDLE_HPP
#define GIMP_PAINTER_MYBRUSH_HANDLE_HPP
extern "C" {
#include "gimppaintermybrush.h"
}
#include "gimp-painter-type-traits.hpp"
#include "painter/resources.hpp"
#include "paint/painter-mypaint/resource.hpp"
namespace GimpPainter {
class PainterMybrushRef
{
public:
  static PainterMybrushRef retain (GimpPainterMybrush *brush)
  { return PainterMybrushRef (ObjectRef<GimpPainterMybrush>::retain (brush)); }
  static PainterMybrushRef adopt (GimpPainterMybrush *brush)
  { return PainterMybrushRef (ObjectRef<GimpPainterMybrush>::adopt (brush)); }
  static PainterMybrushRef create (const char *name)
  { return adopt (GIMP_PAINTER_MYBRUSH (gimp_painter_mybrush_new (nullptr, name))); }
  GimpPainterMybrush *get () const noexcept { return owner_.get (); }
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
    Error failure (GIMP_PAINTER_ERROR_INVALID_STATE, error ? error->message : "Brush resource operation failed");
    g_clear_error (&error); throw failure;
  }
  ObjectRef<GimpPainterMybrush> owner_;
};
}
#endif
