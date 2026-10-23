/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PERSPECTIVE_GUIDE_TOOL_HPP
#define GIMP_PERSPECTIVE_GUIDE_TOOL_HPP
#include <gegl.h>
#include <gtk/gtk.h>
#include "tools-types.h"
#include "gimpperspectiveguidetool.h"
#include "painter/object-ref.hpp"
namespace GimpPainter GIMP_PAINTER_PRIVATE {
template<> struct TypeTraits<GimpPerspectiveGuideOptions>
{ static GType type () noexcept { return GIMP_TYPE_PERSPECTIVE_GUIDE_OPTIONS; } };
template<> struct TypeTraits<GimpPerspectiveGuideTool>
{ static GType type () noexcept { return GIMP_TYPE_PERSPECTIVE_GUIDE_TOOL; } };
using PerspectiveGuideOptionsRef = ObjectRef<GimpPerspectiveGuideOptions>;
class PerspectiveGuideToolRef {
public:
  PerspectiveGuideToolRef () noexcept = default;
  static PerspectiveGuideToolRef retain (GimpPerspectiveGuideTool *tool)
  { return PerspectiveGuideToolRef (ObjectRef<GimpPerspectiveGuideTool>::retain (tool)); }
  static PerspectiveGuideToolRef adopt (GimpPerspectiveGuideTool *tool)
  { return PerspectiveGuideToolRef (ObjectRef<GimpPerspectiveGuideTool>::adopt (tool)); }
  static void register_type (GimpToolRegisterCallback callback, gpointer data)
  { gimp_perspective_guide_tool_register (callback, data); }
  GimpPerspectiveGuideTool *get () const noexcept { return owner_.get (); }
  explicit operator bool () const noexcept { return bool (owner_); }
  void reset () noexcept { owner_.reset (); }
private:
  explicit PerspectiveGuideToolRef (ObjectRef<GimpPerspectiveGuideTool> owner) noexcept : owner_ (std::move (owner)) {}
  ObjectRef<GimpPerspectiveGuideTool> owner_;
};
}
#endif
