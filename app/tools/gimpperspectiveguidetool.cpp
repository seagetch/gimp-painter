/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "base/delegators.hpp"
#include "base/scopeguard.hpp"
#include "base/glib-cxx-bridge.hpp"
#include "base/glib-cxx-utils.hpp"
#include "base/glib-cxx-def-utils.hpp"
#include "base/glib-cxx-types.hpp"
#include "base/glib-cxx-impl.hpp"
#include "base/selectcase-utils.hpp"
#include <functional>

using namespace GLib;

extern "C" {

#include "base/tile.h"
#include "base/tile-manager.h"
#include <gegl.h>

#include "config.h"

#include <gtk/gtk.h>

#include "libgimpwidgets/gimpwidgets.h"

#include "tools-types.h"
#include "gimp-tools.h"

#include "core/gimptoolinfo.h"
#include "core/gimptooloptions.h"
#include "paint/gimppaintoptions.h"
#include "widgets/gimphelp-ids.h"
#include "widgets/gimppropwidgets.h"

#include "gimptooloptions-gui.h"
#include "gimptoolcontrol.h"
#include "gimp-intl.h"

#include "display/gimpdisplay.h"



#include "config.h"

#include "libgimpmath/gimpmath.h"

#include "core/gimp.h"
#include "core/gimpdrawable.h"
#include "core/gimpimage.h"
#include "core/gimpimage-perspective-guide.h"

#include "gimp-intl.h"
#include "config.h"
#include <glib-object.h>
#include "libgimpconfig/gimpconfig.h"
#include "paint/paint-types.h"

};

#include "core/gimpperspectiveguide.h"


////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Paint Option
extern "C" {
#define GIMP_TYPE_PERSPECTIVE_GUIDE_OPTIONS            (gimp_perspective_guide_options_get_type ())
#define GIMP_PERSPECTIVE_GUIDE_OPTIONS(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIMP_TYPE_PERSPECTIVE_GUIDE_OPTIONS, GimpPerspectiveGuideOptions))
#define GIMP_PERSPECTIVE_GUIDE_OPTIONS_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GIMP_TYPE_PERSPECTIVE_GUIDE_OPTIONS, GimpPerspectiveGuideOptionsClass))
#define GIMP_IS_PERSPECTIVE_GUIDE_OPTIONS(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIMP_TYPE_PERSPECTIVE_GUIDE_OPTIONS))
#define GIMP_IS_PERSPECTIVE_GUIDE_OPTIONS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GIMP_TYPE_PERSPECTIVE_GUIDE_OPTIONS))
#define GIMP_PERSPECTIVE_GUIDE_OPTIONS_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GIMP_TYPE_PERSPECTIVE_GUIDE_OPTIONS, GimpPerspectiveOptionsToolClass))
struct GimpPerspectiveGuideOptions {
  GimpToolOptions parent;
};

struct GimpPerspectiveGuideOptionsClass {
  GimpToolOptionsClass parent_class;
};
GType   gimp_perspective_guide_options_get_type (void);
};

struct Options : virtual public ImplBase
{
  Options(GObject* g_object) : ImplBase(g_object) {
  };

  ~Options() {

  };

};
__DECLARE_GTK_CLASS__(GimpPerspectiveGuideOptions, GIMP_TYPE_PERSPECTIVE_GUIDE_OPTIONS);

static const char gimp_perspective_guide_options_name[] = "GimpPerspectiveGuideOptions";
using OptionsClass = NewGClass<gimp_perspective_guide_options_name, GLib::UseCStructs<GimpToolOptions, GimpPerspectiveGuideOptions>, Options>;

#define _override(method) OptionsClass::__(&klass->method).bind<&Options::method>()
#define _getter(method)   OptionsClass::__((CopyValue (**)(GObject*))NULL).bind<&Options::get_##method >()
#define _setter(method)   OptionsClass::__((void (**)(GObject*, IValue))NULL).bind<&Options::set_##method >()

static OptionsClass options_class([](IGClass::IWithClass* with_class){
  g_print("PerspectiveGuide::OptionsClass::class_init\n");
#if 0
  with_class
    ->install_property(
      OptionsClass::g_param_spec_new("rate", OptionsClass::Range<double>(0, 100.), 50.0, (GParamFlags)(GIMP_PARAM_READWRITE|G_PARAM_CONSTRUCT) ),
      _getter(rate), _setter(rate)

    )->install_property(
      OptionsClass::g_param_spec_new("eraser-mode", false, (GParamFlags)(GIMP_PARAM_READWRITE|G_PARAM_CONSTRUCT) ),
      _getter(eraser_mode), _setter(eraser_mode)
    );
#endif
});

#undef _override
#undef _getter
#undef _setter

GType   gimp_perspective_guide_options_get_type (void)
{
  return OptionsClass::get_type();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Tool definition
extern "C" {
#include "gimpperspectiveguidetool.h"
};


struct Tool : virtual public ImplBase
{
  IObject<GimpPerspectiveGuide> guide;
  enum Target {
    None,
    VanishPoint,
    AngleLine,
    AuxPoint
  };
  enum Operation {
    Move,
    Add,
    Remove
  };
  Target target;
  int    target_index;
  Operation operation;  

  Tool(GObject* g_object);
  virtual ~Tool();

  // Inherit virtual methods

  void control (GimpToolAction action, GimpDisplay *display);

  void button_press (const GimpCoords      *coords,
                     guint32                time,
                     GdkModifierType        state,
                     GimpButtonPressType    press_type,
                     GimpDisplay           *display);
  void button_release(const GimpCoords      *coords,
                      guint32                time,
                      GdkModifierType        state,
                      GimpButtonReleaseType  release_type,
                      GimpDisplay           *display);
  void motion (const GimpCoords      *coords,
               guint32                time,
               GdkModifierType        state,
               GimpDisplay           *display);

  gboolean key_press(GdkEventKey           *kevent,
                     GimpDisplay           *display);

  void modifier_key (GdkModifierType        key,
                     gboolean               press,
                     GdkModifierType        state,
                     GimpDisplay           *display);

  void oper_update (const GimpCoords      *coords,
                    GdkModifierType        state,
                    gboolean               proximity,
                                           GimpDisplay           *display);
  void cursor_update(const GimpCoords      *coords,
                     GdkModifierType        state,
                     GimpDisplay           *display);

  void draw ();

  // setter /getter

  CopyValue get_guide() {
    return guide.ptr();
  };

  void set_guide(IValue v) {
    const GValue* val = v.ptr();
    guide = GIMP_PERSPECTIVE_GUIDE(g_value_get_object(val));
  }

  // Other functions
  bool get_nearest(gdouble x, gdouble y, Target* target, int* index, gdouble* distance);

};

static const char gimp_perspective_guide_tool_name[] = "GimpPerspectiveGuideTool";
using ToolClass = NewGClass<gimp_perspective_guide_tool_name, GLib::UseCStructs<GimpDrawTool, GimpPerspectiveGuideTool>, Tool>;

#define _override(method) ToolClass::__(&klass->method).bind<&Tool::method>()
#define _getter(method)   ToolClass::__((CopyValue (**)(GObject*))NULL).bind<&Tool::get_##method >()
#define _setter(method)   ToolClass::__((void (**)(GObject*, IValue))NULL).bind<&Tool::set_##method >()

static ToolClass tool_class([](IGClass::IWithClass* with_class){
  g_print("PerspectiveGuideTool::class_init\n");

  with_class
    ->as_class<GimpTool>([](GimpToolClass* klass) {
      _override(control);
      _override(button_press);
      _override(button_release);
      _override(motion);
      _override(key_press);
      _override(modifier_key);
      _override(oper_update);
      _override(cursor_update);
    })->
    as_class<GimpDrawTool>([](GimpDrawToolClass* klass) {
      _override(draw);
    })->
    install_property(
      ToolClass::g_param_spec_object("guide", GIMP_TYPE_PERSPECTIVE_GUIDE, (GParamFlags)(GIMP_PARAM_READWRITE|G_PARAM_CONSTRUCT) ),
      _getter(guide), _setter(guide)

    );

});

#undef _override
#undef _getter
#undef _setter
namespace GLib {
template<> class Traits<ToolClass::Instance> : public ToolClass::Traits { };
};


Tool::Tool(GObject* g_object) : ImplBase(g_object) {
  GimpTool* tool = GIMP_TOOL(g_object);
  tool->disable_lazy_snap = TRUE;
};

Tool::~Tool() {

};

void 
Tool::control (GimpToolAction action, GimpDisplay *display)
{
//  GimpVectorTool *vector_tool = GIMP_VECTOR_TOOL (tool);

  switch (action)
    {
    case GIMP_TOOL_ACTION_PAUSE:
    case GIMP_TOOL_ACTION_RESUME:
      break;

    case GIMP_TOOL_ACTION_HALT:
//      gimp_vector_tool_set_vectors (vector_tool, NULL);
      break;
    }

  GIMP_TOOL_CLASS(ToolClass::parent_class)->control (GIMP_TOOL(g_object), action, display);
}

void 
Tool::button_press (const GimpCoords      *coords,
                    guint32                time,
                    GdkModifierType        state,
                    GimpButtonPressType    press_type,
                    GimpDisplay           *display)
{
  GimpTool* tool = GIMP_TOOL(g_object);
  GimpDrawTool* draw_tool = GIMP_DRAW_TOOL(g_object);
  GimpImage      *image       = gimp_display_get_image (display);
  g_print("button_press:disable_lazy_snap=%d\n", tool->disable_lazy_snap);

  gimp_draw_tool_pause (draw_tool);

  if (gimp_draw_tool_is_active (draw_tool) && draw_tool->display != display)
    {
      gimp_draw_tool_stop (draw_tool);
    }

  gimp_tool_control_activate (tool->control);
  tool->display = display;

  // Do something.
  if (operation == Add) {
    if (!guide) {
      guide = gimp_perspective_guide_new(0);
      gimp_image_set_perspective_guide (image, guide);
      g_print("Creating new guide\n");
    }
    auto i_guide = PerspectiveGuideInterface::cast(guide);
    if (i_guide->get_vanish_point_length() < 3) {
      g_print("Added new vanishment point(%d)\n", i_guide->get_vanish_point_length());
      i_guide->add_vanish_points(coords->x, coords->y);
    }

  } else {
    if (!guide)
      return;

    auto i_guide = PerspectiveGuideInterface::cast(guide);
    Target nearest;
    gdouble nearest_distance;
    int nearest_index;
    bool found = get_nearest(coords->x, coords->y, &nearest, &nearest_index, &nearest_distance);

    if (found) {
      gdouble x, y;
      i_guide->get_vanish_points(nearest_index, &x, &y);
      if (gimp_draw_tool_on_handle (draw_tool, display, coords->x, coords->y, 
                                    GIMP_HANDLE_CIRCLE, x, y, 
                                    GIMP_TOOL_HANDLE_SIZE_CIRCLE, GIMP_TOOL_HANDLE_SIZE_CIRCLE, 
                                    GIMP_HANDLE_ANCHOR_CENTER)) {
        target = nearest;
        target_index = nearest_index;
        g_print("target=%d, index=%d\n", target, target_index);
      } else {
        target = None;
        target_index = -1;
      }
    }

    if (target == VanishPoint && operation == Remove) {
      i_guide->remove_vanish_points(target_index);
      target = None;
      target_index = -1;
      if (i_guide->get_vanish_point_length() == 0) {
        guide = NULL;
        gimp_image_set_perspective_guide (image, guide);
      }
    }

  }
  
  gimp_draw_tool_resume (draw_tool);
}

void 
Tool::button_release(const GimpCoords      *coords,
                    guint32                time,
                    GdkModifierType        state,
                    GimpButtonReleaseType  release_type,
                    GimpDisplay           *display)
{
  if (!guide)
    return;

  GimpTool* tool = GIMP_TOOL(g_object);
  g_print("button_release:disable_lazy_snap=%d\n", tool->disable_lazy_snap);

  GimpImage      *image       = gimp_display_get_image (display);

  // Do something.
  target = None;
  target_index = -1;


//  gimp_tool_control_halt (tool->control);
  gimp_image_flush (image);
}


void 
Tool::motion (const GimpCoords      *coords,
              guint32                time,
              GdkModifierType        state,
              GimpDisplay           *display)
{
  if (!guide)
    return;

  auto i_draw = ref(g_object);
  GimpTool* tool = GIMP_TOOL(g_object);
  GimpImage      *image       = gimp_display_get_image (display);
  g_print("motion, target=%d, index=%d, x=%f, y=%f\n", target, target_index, coords->x, coords->y);
  if (target == VanishPoint && target_index >= 0) {
    i_draw [gimp_draw_tool_pause] ();
    auto i_guide = PerspectiveGuideInterface::cast(guide);
    i_guide->set_vanish_points(target_index, coords->x, coords->y);
    i_draw [gimp_draw_tool_resume] ();
  }
}


gboolean 
Tool::key_press(GdkEventKey           *kevent,
                    GimpDisplay           *display)
{
  GimpTool* tool = GIMP_TOOL(g_object);
  g_print("key_press:disable_lazy_snap=%d\n", tool->disable_lazy_snap);
  return TRUE;
}

void 
Tool::modifier_key (GdkModifierType        key,
                    gboolean               press,
                    GdkModifierType        state,
                    GimpDisplay           *display)
{
  g_print("modify_key:operation=%d\n", operation);

  if (press) {
    if (key == GDK_SHIFT_MASK && operation == Move) {
      operation = Add;
      g_print("-->Add\n");
    }
    if (key == GDK_CONTROL_MASK && operation == Move) {
      operation = Remove;
      g_print("-->Remove\n");
    }
  } else {
    if ((key == GDK_SHIFT_MASK && operation == Add) ||
        (key == GDK_CONTROL_MASK && operation == Remove)) {
      operation = Move;
      g_print("-->Move\n");
    }

  }

}

void 
Tool::oper_update (const GimpCoords      *coords,
                  GdkModifierType        state,
                  gboolean               proximity,
                  GimpDisplay           *display)
{
  auto i_draw = ref(g_object);
  g_return_if_fail (i_draw);
  GimpImage      *image       = gimp_display_get_image (display);
  guide = ref(image) [gimp_image_get_perspective_guide] ();

  if (! i_draw [gimp_draw_tool_is_active] ()) {
    i_draw [gimp_draw_tool_start] (display);
  }
}


void 
Tool::cursor_update(const GimpCoords      *coords,
                    GdkModifierType        state,
                    GimpDisplay           *display)
{
  GimpToolCursorType  tool_cursor = GIMP_TOOL_CURSOR_PATHS;
  GimpCursorModifier  modifier    = GIMP_CURSOR_MODIFIER_NONE;
  GimpTool*           tool        = GIMP_TOOL(g_object);
  GimpDrawTool*       draw_tool   = GIMP_DRAW_TOOL(g_object);

  if (target == None) {
    Target nearest;
    gdouble nearest_distance;
    int nearest_index;
    bool found = get_nearest(coords->x, coords->y, &nearest, &nearest_index, &nearest_distance);

    if (found) {
      gdouble x, y;
      auto i_guide = PerspectiveGuideInterface::cast(guide);
      i_guide->get_vanish_points(nearest_index, &x, &y);
      if (gimp_draw_tool_on_handle (draw_tool, display, coords->x, coords->y, 
                                    GIMP_HANDLE_CIRCLE, x, y, 
                                    GIMP_TOOL_HANDLE_SIZE_CIRCLE, GIMP_TOOL_HANDLE_SIZE_CIRCLE, 
                                    GIMP_HANDLE_ANCHOR_CENTER)) {
        tool_cursor = GIMP_TOOL_CURSOR_HAND;
      }
    }

    switch(operation) {
      case Move:
        modifier    = GIMP_CURSOR_MODIFIER_MOVE;
        break;
      case Add:
        modifier    = GIMP_CURSOR_MODIFIER_PLUS;
        break;
      case Remove:
        modifier    = GIMP_CURSOR_MODIFIER_MINUS;
        break;
    }
  }

  gimp_tool_control_set_tool_cursor     (tool->control, tool_cursor);
  gimp_tool_control_set_cursor_modifier (tool->control, modifier);

  GIMP_TOOL_CLASS (ToolClass::parent_class)->cursor_update (tool, coords, state, display);
}


void 
Tool::draw()
{
  if (!guide)
    return;
  auto i_draw  = ref(g_object);
  auto i_guide = PerspectiveGuideInterface::cast(guide);
  int length   = i_guide->get_vanish_point_length();
  
  for (int i = 0; i < i_guide->get_vanish_point_length(); i ++) {
    gdouble px, py, dx, dy;
    i_guide->get_vanish_points(i, &px, &py);
    i_draw [gimp_draw_tool_add_handle] (
                                (target == VanishPoint && target_index == i)?
                                  GIMP_HANDLE_CIRCLE :GIMP_HANDLE_FILLED_CIRCLE,
                                px, py,
                                GIMP_TOOL_HANDLE_SIZE_CIRCLE, GIMP_TOOL_HANDLE_SIZE_CIRCLE,
                                GIMP_HANDLE_ANCHOR_CENTER);
  }

  if (length >= 2) {
    gdouble p1x, p1y;
    gdouble p2x, p2y;
    i_guide->get_vanish_points(0, &p1x, &p1y);
    i_guide->get_vanish_points(1, &p2x, &p2y);

    i_draw  [gimp_draw_tool_add_line] (p1x, p1y, p2x, p2y);
  }
}


bool 
Tool::get_nearest(gdouble x, gdouble y, Target* target, int* index, gdouble* min_distance)
{
#if 0
  if (!guide) {
    *target = None;
    return false;
  }
#endif
  if (!guide)
    return false;
  auto i_guide = PerspectiveGuideInterface::cast(guide);
  
  *index = -1;
  for (int i = 0; i < i_guide->get_vanish_point_length(); i ++) {
    gdouble px, py, dx, dy;
    gdouble distance;
    i_guide->get_vanish_points(i, &px, &py);
    dx = x - px;
    dy = y - py;
    distance = sqrt(dx*dx+dy*dy);
    if (*index < 0 || distance < *min_distance) {
      *min_distance = distance;
      *index = i;
    }
  }
  *target = VanishPoint;
  return true;
}


/*  tool options stuff  */


static GtkWidget *
gimp_perspective_guide_options_gui_full (GimpToolOptions *tool_options, gboolean horizontal)
{
#if 0
  GObject   *config = G_OBJECT (tool_options);
  GtkWidget *vbox   = gimp_paint_options_gui_full (tool_options, horizontal);
  GtkWidget *scale;
  GtkWidget *button;
  GList     *children;

  /*  the rate scale  */
  scale = gimp_prop_spin_scale_new (config, "rate",
                                    _("Rate"),
                                    1.0, 10.0, 1);
  gtk_box_pack_start (GTK_BOX (vbox), scale, FALSE, FALSE, 0);
  gtk_widget_show (scale);
  button = gimp_prop_check_button_new (config, "eraser-mode", _("Eraser mode"));
  gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
  gtk_widget_show (button);

  children = gtk_container_get_children (GTK_CONTAINER (vbox));  
  gimp_tool_options_setup_popup_layout (children, FALSE);

  return vbox;
#endif
  GtkWidget *vbox   = gimp_tool_options_gui_full (tool_options, horizontal);
  return vbox;
}


static GtkWidget *
gimp_perspective_guide_options_gui (GimpToolOptions *tool_options)
{
  return gimp_perspective_guide_options_gui_full (tool_options, FALSE);
}

static GtkWidget *
gimp_perspective_guide_options_gui_horizontal (GimpToolOptions *tool_options)
{
  return gimp_perspective_guide_options_gui_full (tool_options, TRUE);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Registration entrypoint

void
gimp_perspective_guide_tool_register (GimpToolRegisterCallback  callback,
                           gpointer                  data)
{
  g_print("gimp_perspective_guide_tool_register\n");
  g_return_if_fail (GIMP_IS_GIMP(data));
  g_print("gimp_perspective_guide_tool_register\n");
  Gimp* gimp = (Gimp*)data;
  GimpToolInfo* tool_info;
  tool_info = gimp_tool_info_new (gimp,
                                  tool_class.type(),
                                  options_class.type(),
                                   GimpContextPropMask(GIMP_PAINT_OPTIONS_CONTEXT_MASK),
                                  "gimp-perspective-guide-tool",
                                  _("PerspectiveGuide"),
                                  _("Editing perspective guide"),
                                  N_("_PerspectiveGuide"),
                                  "g",
                                  NULL,
                                  "gimp-perspective-guide",
                                  "gimp-paintbrush",
                                  GIMP_STOCK_TOOL_PERSPECTIVE);

  g_object_set (tool_info, "visible", TRUE, NULL);
  g_object_set_data (G_OBJECT (tool_info), "gimp-tool-default-visible",
                     GINT_TO_POINTER (TRUE));

  g_object_set_data (G_OBJECT (tool_info), "gimp-tool-options-gui-func",
                     (gpointer)gimp_perspective_guide_options_gui);

  g_object_set_data (G_OBJECT (tool_info), "gimp-tool-options-gui-horizontal-func",
                     (gpointer)gimp_perspective_guide_options_gui_horizontal);

  gimp_tools_register (gimp, tool_info);
}

GType   gimp_perspective_guide_tool_get_type (void)
{
  return ToolClass::get_type();
}