/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_SESSION_HPP
#define GIMP_PAINTER_SESSION_HPP
#include "../../painter/gimp-painter-visibility.h"
extern "C" {
#include "gimp-painter-session.h"
}
#include "painter/object-ref.hpp"
namespace GimpPainter GIMP_PAINTER_PRIVATE {
template<> struct TypeTraits<GimpPainterSession>
{static GType type() noexcept{return GIMP_TYPE_PAINTER_SESSION;}};
class PainterSessionRef {
public:
  static PainterSessionRef retain(GimpPainterSession*p){return PainterSessionRef(ObjectRef<GimpPainterSession>::retain(p));}
  static PainterSessionRef adopt(GimpPainterSession*p){return PainterSessionRef(ObjectRef<GimpPainterSession>::adopt(p));}
  GimpPainterSession*get() const noexcept{return owner_.get();}
private:
  explicit PainterSessionRef(ObjectRef<GimpPainterSession>owner):owner_(std::move(owner))
  {if(!owner_)throw Error(GIMP_PAINTER_ERROR_WRONG_TYPE,"Expected painter session");}
  ObjectRef<GimpPainterSession> owner_;
};
}
#endif
