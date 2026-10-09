/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_MYBRUSH_OPTIONS_HPP
#define GIMP_PAINTER_MYBRUSH_OPTIONS_HPP
#include "../../painter/gimp-painter-visibility.h"
extern "C" {
#include "gimp-painter-options.h"
}
#include "painter/object-ref.hpp"
#include "painter/resources.hpp"
#include "paint/painter-mypaint/resource.hpp"
namespace GimpPainter GIMP_PAINTER_PRIVATE {
template<> struct TypeTraits<GimpPainterMybrushOptions>
{ static GType type () noexcept { return GIMP_TYPE_PAINTER_MYBRUSH_OPTIONS; } };
class PainterOptionsRef {
public:
  static PainterOptionsRef retain (GimpPainterMybrushOptions *options)
  { return PainterOptionsRef (ObjectRef<GimpPainterMybrushOptions>::retain (options)); }
  GimpPainterMybrushOptions *get () const noexcept { return owner_.get (); }
  MyPaint::Resource snapshot () const
  {
    GError *error=nullptr;String json(gimp_painter_mybrush_options_dup_json(get(),&error));
    if(!json){throw Error(GIMP_PAINTER_ERROR_INVALID_STATE,take_error_message(error,"Unavailable brush draft").c_str());}
    return MyPaint::Resource::decode(json.get());
  }
private:
  explicit PainterOptionsRef(ObjectRef<GimpPainterMybrushOptions> owner):owner_(std::move(owner))
  {if(!owner_)throw Error(GIMP_PAINTER_ERROR_WRONG_TYPE,"Expected painter options");}
  ObjectRef<GimpPainterMybrushOptions> owner_;
};
}
#endif
