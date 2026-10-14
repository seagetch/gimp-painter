/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_CELL_RENDERER_POPUP_HANDLE_HPP
#define GIMP_CELL_RENDERER_POPUP_HANDLE_HPP
#include "gimpcellrendererpopup.h"
#include "painter/object-ref.hpp"

namespace GimpPainter GIMP_PAINTER_PRIVATE {
template<> struct TypeTraits<GimpCellRendererPopup>
{ static GType type () noexcept { return GIMP_TYPE_CELL_RENDERER_POPUP; } };

/* Owns the native renderer, not a historical C++ Interface or class layout.
 * GUI-thread operations; retained storage does not reopen a closed binding. */
class CellRendererPopupRef {
public:
  static CellRendererPopupRef create ()
  {
    // The C factory returns a floating cell. Sink before a column can retain it.
    return CellRendererPopupRef (ObjectRef<GimpCellRendererPopup>::sink (
      GIMP_CELL_RENDERER_POPUP (gimp_cell_renderer_popup_new ())));
  }
  static CellRendererPopupRef retain (GimpCellRendererPopup *cell)
  { return CellRendererPopupRef (ObjectRef<GimpCellRendererPopup>::retain (cell)); }
  GimpCellRendererPopup *get () const noexcept { return owner_.get (); }
  void clicked (GObject *widget, const gchar *path, GdkRectangle *area) const
  { gimp_cell_renderer_popup_clicked (get (), widget, path, area); }
private:
  explicit CellRendererPopupRef (ObjectRef<GimpCellRendererPopup> owner)
    : owner_ (std::move (owner))
  { if (!owner_) throw Error (GIMP_PAINTER_ERROR_WRONG_TYPE, "Expected popup renderer"); }
  ObjectRef<GimpCellRendererPopup> owner_;
};
}
#endif
