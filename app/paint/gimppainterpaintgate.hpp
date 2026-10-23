/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_PAINT_GATE_HPP
#define GIMP_PAINTER_PAINT_GATE_HPP
#include "../painter/gimp-painter-visibility.h"
extern "C" {
#include "gimppainterpaintgate.h"
}
#include "painter/object-ref.hpp"
namespace GimpPainter GIMP_PAINTER_PRIVATE {
template<> struct TypeTraits<GimpPainterPaintGate>
{static GType type() noexcept{return GIMP_TYPE_PAINTER_PAINT_GATE;}};
class PainterPaintGateRef {
public:
  static PainterPaintGateRef retain(GimpPaintCore*core)
  {return PainterPaintGateRef(ObjectRef<GimpPainterPaintGate>::retain(reinterpret_cast<GimpPainterPaintGate*>(core)));}
  GimpPainterPaintGate*get() const noexcept{return owner_.get();}
private:
  explicit PainterPaintGateRef(ObjectRef<GimpPainterPaintGate>owner):owner_(std::move(owner))
  {if(!owner_)throw Error(GIMP_PAINTER_ERROR_WRONG_TYPE,"Expected Painter paint adapter");}
  ObjectRef<GimpPainterPaintGate>owner_;
};
}
#endif
