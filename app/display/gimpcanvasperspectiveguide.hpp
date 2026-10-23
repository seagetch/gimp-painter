/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_CANVAS_PERSPECTIVE_GUIDE_HPP
#define GIMP_CANVAS_PERSPECTIVE_GUIDE_HPP
#include <gegl.h>
#include <gtk/gtk.h>
extern "C" {
#include "display-types.h"
#include "gimpcanvasperspectiveguide.h"
}
#include "painter/object-ref.hpp"
namespace GimpPainter GIMP_PAINTER_PRIVATE {
template<> struct TypeTraits<GimpCanvasPerspectiveGuide>
{ static GType type () noexcept { return GIMP_TYPE_CANVAS_PERSPECTIVE_GUIDE; } };
/* Owning native facade; never a legacy layout cast or independent Impl.
 * The shell remains borrowed and must outlive operations on the item. */
class CanvasPerspectiveGuideRef
{
public:
  CanvasPerspectiveGuideRef () noexcept = default;
  static CanvasPerspectiveGuideRef retain (GimpCanvasPerspectiveGuide *guide)
  { return CanvasPerspectiveGuideRef (ObjectRef<GimpCanvasPerspectiveGuide>::retain (guide)); }
  static CanvasPerspectiveGuideRef adopt (GimpCanvasPerspectiveGuide *guide)
  { return CanvasPerspectiveGuideRef (ObjectRef<GimpCanvasPerspectiveGuide>::adopt (guide)); }
  static CanvasPerspectiveGuideRef create (GimpDisplayShell *shell, GimpPerspectiveGuide *guide)
  { return adopt (gimp_canvas_perspective_guide_new (shell, guide)); }
  static CanvasPerspectiveGuideRef create_for_image (GimpDisplayShell *shell)
  { return adopt (reinterpret_cast<GimpCanvasPerspectiveGuide *> (gimp_canvas_perspective_guide_new_for_image (shell))); }
  GimpCanvasPerspectiveGuide *get () const noexcept { return owner_.get (); }
  explicit operator bool () const noexcept { return bool (owner_); }
  void reset () noexcept { owner_.reset (); }
private:
  explicit CanvasPerspectiveGuideRef (ObjectRef<GimpCanvasPerspectiveGuide> owner) noexcept
    : owner_ (std::move (owner)) {}
  ObjectRef<GimpCanvasPerspectiveGuide> owner_;
};
}
#endif
