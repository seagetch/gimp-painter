/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PERSPECTIVE_GUIDE_HPP
#define GIMP_PERSPECTIVE_GUIDE_HPP
#include "../painter/gimp-painter-visibility.h"
#include "gimpperspectiveguide.h"
#include "painter/object-ref.hpp"
namespace GimpPainter GIMP_PAINTER_PRIVATE {
template<> struct TypeTraits<GimpPerspectiveGuide>
{ static GType type () noexcept { return GIMP_TYPE_PERSPECTIVE_GUIDE; } };
/* Owning facade only: validation, state and mutation remain in the C provider.
 * An empty handle is the tool's valid pre-guide/Undo snapshot state. */
class PerspectiveGuideRef
{
public:
  PerspectiveGuideRef () noexcept = default;
  static PerspectiveGuideRef retain (GimpPerspectiveGuide *guide)
  { return PerspectiveGuideRef (ObjectRef<GimpPerspectiveGuide>::retain (guide)); }
  static PerspectiveGuideRef adopt (GimpPerspectiveGuide *guide)
  { return PerspectiveGuideRef (ObjectRef<GimpPerspectiveGuide>::adopt (guide)); }
  static PerspectiveGuideRef create (guint32 id)
  { return adopt (gimp_perspective_guide_new (id)); }
  PerspectiveGuideRef duplicate () const
  { return adopt (gimp_perspective_guide_duplicate (get ())); }
  GimpPerspectiveGuide *get () const noexcept { return owner_.get (); }
  explicit operator bool () const noexcept { return bool (owner_); }
  void reset () noexcept { owner_.reset (); }
  gboolean state (GimpPerspectiveGuideState *state) const
  { return gimp_perspective_guide_get_state (get (), state); }
  gint count () const
  { return gimp_perspective_guide_get_vanish_point_length (get ()); }
  gboolean get_point (gint index, gdouble *x, gdouble *y) const
  { return gimp_perspective_guide_get_vanish_points (get (), index, x, y); }
  gint add_point (gdouble x, gdouble y) const
  { return gimp_perspective_guide_add_vanish_points (get (), x, y); }
  gboolean remove_point (gint index) const
  { return gimp_perspective_guide_remove_vanish_points (get (), index); }
  gboolean set_point (gint index, gdouble x, gdouble y) const
  { return gimp_perspective_guide_set_vanish_points (get (), index, x, y); }
private:
  explicit PerspectiveGuideRef (ObjectRef<GimpPerspectiveGuide> owner) noexcept
    : owner_ (std::move (owner)) {}
  ObjectRef<GimpPerspectiveGuide> owner_;
};
}
#endif
