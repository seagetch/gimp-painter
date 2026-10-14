/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_MYBRUSH_OPTIONS_HPP
#define GIMP_PAINTER_MYBRUSH_OPTIONS_HPP
#include "../../painter/gimp-painter-visibility.h"
extern "C" {
#include "paint/gimpmypaintoptions.h"
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
  std::vector<GimpVector2> get_mapping_point (const gchar *name, guint input) const
  {
    guint count=0;GimpVector2 *points=nullptr;
    gimp_mypaint_options_get_mapping_point(get(),const_cast<gchar*>(name),input,&count,&points);
    std::unique_ptr<GimpVector2,Free> owned(points);
    return count?std::vector<GimpVector2>(points,points+count):std::vector<GimpVector2>();
  }
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
using MypaintOptionsRef = PainterOptionsRef;
}
#endif
