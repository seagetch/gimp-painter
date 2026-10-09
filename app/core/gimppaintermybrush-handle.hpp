/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_MYBRUSH_HANDLE_HPP
#define GIMP_PAINTER_MYBRUSH_HANDLE_HPP
#include "../painter/gimp-painter-visibility.h"
extern "C" {
#include "gimppaintermybrush.h"
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
    throw Error (GIMP_PAINTER_ERROR_INVALID_STATE,
                 take_error_message (error, "Brush resource operation failed").c_str ());
  }
  ObjectRef<GimpPainterMybrush> owner_;
};
}
#endif
