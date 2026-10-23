/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_CONTEXT_HANDLE_HPP
#define GIMP_CONTEXT_HANDLE_HPP

#include <gegl.h>
#include <gtk/gtk.h>
extern "C" {
#include "core-types.h"
#include "gimpcontext.h"
#include "widgets/gimpmypaintbrushoptions-gui.h"
}
#include "painter/object-ref.hpp"

namespace GimpPainter GIMP_PAINTER_PRIVATE {
/* Retains native context storage. A disposed context or closed options binding
 * remains terminal. Methods creating GTK widgets belong to the GUI thread. */
class ContextRef {
public:
  static ContextRef retain (GimpContext *context)
  { return ContextRef (context); }
  GimpContext *get () const noexcept
  { return GIMP_CONTEXT (owner_.get ()); }
  ObjectRef<GObject> create_mypaint_brush_button () const
  {
    // The exact C provider returns a floating widget. Sink that same result;
    // an empty result preserves the C factory's contained failure contract.
    return ObjectRef<GObject>::sink (G_OBJECT (
      gimp_mypaint_brush_button_with_popup (owner_.get ())));
  }
private:
  explicit ContextRef (GimpContext *context)
  {
    if (!GIMP_IS_CONTEXT (context))
      throw Error (GIMP_PAINTER_ERROR_WRONG_TYPE, "Expected a GimpContext");
    owner_ = ObjectRef<GObject>::retain (G_OBJECT (context));
  }
  ObjectRef<GObject> owner_;
};
}
#endif
