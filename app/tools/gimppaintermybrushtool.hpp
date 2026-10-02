/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_MYBRUSH_TOOL_HPP
#define GIMP_PAINTER_MYBRUSH_TOOL_HPP
extern "C" {
#include "gimppaintermybrushtool.h"
}
#include "painter/object-ref.hpp"
namespace GimpPainter {
template<> struct TypeTraits<GimpPainterMybrushTool>
{static GType type() noexcept{return GIMP_TYPE_PAINTER_MYBRUSH_TOOL;}};
class PainterMybrushToolRef {
public:
  static PainterMybrushToolRef retain(GimpPainterMybrushTool*tool)
  {return PainterMybrushToolRef(ObjectRef<GimpPainterMybrushTool>::retain(tool));}
  static PainterMybrushToolRef adopt(GimpPainterMybrushTool*tool)
  {return PainterMybrushToolRef(ObjectRef<GimpPainterMybrushTool>::adopt(tool));}
  GimpPainterMybrushTool*get() const noexcept{return owner_.get();}
private:
  explicit PainterMybrushToolRef(ObjectRef<GimpPainterMybrushTool> owner):owner_(std::move(owner))
  {if(!owner_)throw Error(GIMP_PAINTER_ERROR_WRONG_TYPE,"Expected extended MyPaint tool");}
  ObjectRef<GimpPainterMybrushTool> owner_;
};
}
#endif
