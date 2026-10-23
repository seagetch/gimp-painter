/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_MYBRUSH_EDITOR_HPP
#define GIMP_PAINTER_MYBRUSH_EDITOR_HPP
#include "../painter/gimp-painter-visibility.h"
extern "C" {
#include "gimppaintermybrusheditor.h"
}
#include "painter/object-ref.hpp"
namespace GimpPainter GIMP_PAINTER_PRIVATE {
template<> struct TypeTraits<GimpPainterMybrushEditor> {
  static GType type () noexcept { return GIMP_TYPE_PAINTER_MYBRUSH_EDITOR; }
};
class PainterEditorRef {
public:
  static PainterEditorRef retain (GimpPainterMybrushEditor *editor)
  { return PainterEditorRef (ObjectRef<GimpPainterMybrushEditor>::retain (editor)); }
  GimpPainterMybrushEditor *get () const noexcept { return owner_.get (); }
private:
  explicit PainterEditorRef (ObjectRef<GimpPainterMybrushEditor> owner) : owner_ (std::move (owner))
  { if (!owner_) throw Error (GIMP_PAINTER_ERROR_WRONG_TYPE, "Expected painter editor"); }
  ObjectRef<GimpPainterMybrushEditor> owner_;
};
}
#endif
