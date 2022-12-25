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
#include "widgets/gimphelp-ids.h"
#include "widgets/gimppropwidgets.h"

#include "gimptooloptions-gui.h"
#include "gimppaintoptions-gui.h"
#include "gimptoolcontrol.h"
#include "gimp-intl.h"

#include "display/gimpdisplay.h"


#include "core/gimp.h"
#include "core/gimpdrawable.h"
#include "core/gimpdynamics.h"
#include "core/gimpdynamicsoutput.h"
#include "core/gimpimage.h"
#include "core/gimppickable.h"
#include "core/gimppaintinfo.h"

#include "gimp-intl.h"
#include "config.h"
#include <glib-object.h>
#include "libgimpconfig/gimpconfig.h"

};



////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Paint Option

////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Paint Option
extern "C" {
#define GIMP_TYPE_IMAGE_GENERATOR_OPTIONS            (gimp_image_generator_options_get_type ())
#define GIMP_IMAGE_GENERATOR_OPTIONS(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIMP_TYPE_IMAGE_GENERATOR_OPTIONS, GimpImageGeneratorOptions))
#define GIMP_IMAGE_GENERATOR_OPTIONS_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GIMP_TYPE_IMAGE_GENERATOR_OPTIONS, GimpImageGeneratorOptionsClass))
#define GIMP_IS_IMAGE_GENERATOR_OPTIONS(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIMP_TYPE_IMAGE_GENERATOR_OPTIONS))
#define GIMP_IS_IMAGE_GENERATOR_OPTIONS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GIMP_TYPE_IMAGE_GENERATOR_OPTIONS))
#define GIMP_IMAGE_GENERATOR_OPTIONS_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GIMP_TYPE_IMAGE_GENERATOR_OPTIONS, GimpPerspectiveOptionsToolClass))
struct GimpImageGeneratorOptions {
  GimpToolOptions parent;
};

struct GimpImageGeneratorOptionsClass {
  GimpToolOptionsClass parent_class;
};
GType   gimp_image_generator_options_get_type (void);
};

struct ImageGeneratorOptionsImpl : virtual public ImplBase
{
  ImageGeneratorOptionsImpl(GObject* g_object) : ImplBase(g_object) {
  };

  ~ImageGeneratorOptionsImpl() {

  };

};
__DECLARE_GTK_CLASS__(GimpImageGeneratorOptions, GIMP_TYPE_IMAGE_GENERATOR_OPTIONS);

using Options = ImageGeneratorOptionsImpl;

constexpr char gimp_image_generator_options_name[] = "GimpImageGeneratorOptions";
using OptionsClass = NewGClass<gimp_image_generator_options_name, GLib::UseCStructs<GimpToolOptions, GimpImageGeneratorOptions>, Options>;

#define _override(method) OptionsClass::__(&klass->method).bind<&Options::method>()
#define _getter(method)   OptionsClass::__((CopyValue (**)(GObject*))NULL).bind<&Options::get_##method >()
#define _setter(method)   OptionsClass::__((void (**)(GObject*, IValue))NULL).bind<&Options::set_##method >()

static OptionsClass options_class([](IGClass::IWithClass* with_class){
  g_print("ImageGenerator::OptionsClass::class_init\n");
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

GType   gimp_image_generator_options_get_type (void)
{
  return OptionsClass::get_type();
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Tool Definition

extern "C" {
#include "gimpimagegeneratortool.h"
};

struct ImageGeneratorToolImpl : public virtual ImplBase {

  ImageGeneratorToolImpl(GObject* obj);
  ~ImageGeneratorToolImpl();

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

};

using Tool = ImageGeneratorToolImpl;

extern const char gimp_image_generator_name[] = "GimpImageGeneratorTool";
using ToolClass = NewGClass<gimp_image_generator_name, GLib::UseCStructs<GimpDrawTool, GimpImageGeneratorTool>, Tool>;

#define _override(method) ToolClass::__(&klass->method).bind<&Tool::method>()
#define _getter(method)   ToolClass::__((CopyValue (**)(GObject*))NULL).bind<&Tool::get_##method >()
#define _setter(method)   ToolClass::__((void (**)(GObject*, IValue))NULL).bind<&Tool::set_##method >()

ToolClass tool_class([](IGClass::IWithClass* with_class){
  with_class->
    as_class<GimpTool>([](GimpToolClass* klass) {
      _override (control);
      _override (button_press);
      _override (motion);
      _override (button_release);
      _override (key_press);
      _override (oper_update);
      _override (cursor_update);
//      _override (get_popup);
    })->as_class<GimpDrawTool>([](GimpDrawToolClass* klass) {
      _override (draw);
    });
});

#undef override
#undef _getter
#undef _setter

namespace GLib {
template<> class Traits<ToolClass::Instance> : public ToolClass::Traits { };
};

Tool::ImageGeneratorToolImpl (GObject* obj) : ImplBase(obj) {
}

Tool::~ImageGeneratorToolImpl () {  
}

void 
Tool::control (GimpToolAction action, GimpDisplay *display)
{

  switch (action)
    {
    case GIMP_TOOL_ACTION_PAUSE:
    case GIMP_TOOL_ACTION_RESUME:
      break;

    case GIMP_TOOL_ACTION_HALT:
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


  gimp_draw_tool_resume (draw_tool);
}

void 
Tool::button_release(const GimpCoords      *coords,
                    guint32                time,
                    GdkModifierType        state,
                    GimpButtonReleaseType  release_type,
                    GimpDisplay           *display)
{
  GimpTool* tool = GIMP_TOOL(g_object);

  GimpImage      *image       = gimp_display_get_image (display);

  // Do something.
//  gimp_tool_control_halt (tool->control);
  gimp_image_flush (image);
}


void 
Tool::motion (const GimpCoords      *coords,
              guint32                time,
              GdkModifierType        state,
              GimpDisplay           *display)
{

  auto i_draw = ref(g_object);
  GimpTool* tool = GIMP_TOOL(g_object);
  GimpImage      *image       = gimp_display_get_image (display);

    i_draw [gimp_draw_tool_pause] ();

    // Do Something

    i_draw [gimp_draw_tool_resume] ();
}


gboolean 
Tool::key_press(GdkEventKey           *kevent,
                    GimpDisplay       *display)
{
  GimpTool* tool = GIMP_TOOL(g_object);

  return TRUE;
}

void 
Tool::modifier_key (GdkModifierType        key,
                    gboolean               press,
                    GdkModifierType        state,
                    GimpDisplay           *display)
{
  if (press) {
    if (key == GDK_SHIFT_MASK) {

    }
    if (key == GDK_CONTROL_MASK) {

    }
  } else {

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


//        modifier    = GIMP_CURSOR_MODIFIER_MOVE;
//        modifier    = GIMP_CURSOR_MODIFIER_PLUS;
//        modifier    = GIMP_CURSOR_MODIFIER_MINUS;

  gimp_tool_control_set_tool_cursor     (tool->control, tool_cursor);
  gimp_tool_control_set_cursor_modifier (tool->control, modifier);

  GIMP_TOOL_CLASS (ToolClass::parent_class)->cursor_update (tool, coords, state, display);
}


void 
Tool::draw()
{
  auto i_draw  = ref(g_object);

//    i_draw  [gimp_draw_tool_add_line] (p1x, p1y, p2x, p2y);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Tool definition
extern "C" {


static GtkWidget * gimp_image_generator_options_gui (GimpToolOptions *tool_options);
static GtkWidget * gimp_image_generator_options_gui_horizontal (GimpToolOptions *tool_options);
static GtkWidget * gimp_image_generator_options_gui_full (GimpToolOptions *tool_options, gboolean horizontal);


/*  tool options stuff  */

static GtkWidget *
gimp_image_generator_options_gui (GimpToolOptions *tool_options)
{
  return gimp_image_generator_options_gui_full (tool_options, FALSE);
}

static GtkWidget *
gimp_image_generator_options_gui_horizontal (GimpToolOptions *tool_options)
{
  return gimp_image_generator_options_gui_full (tool_options, TRUE);
}

static GtkWidget *
gimp_image_generator_options_gui_full (GimpToolOptions *tool_options, gboolean horizontal)
{
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
}


void
gimp_image_generator_tool_register (GimpToolRegisterCallback  callback,
                           gpointer                  data)
{
  g_print("gimp_image_generator_tool_register\n");
  g_return_if_fail (GIMP_IS_GIMP(data));
  g_print("gimp_image_generator_tool_register\n");
  Gimp* gimp = (Gimp*)data;
  GimpToolInfo* tool_info;
  tool_info = gimp_tool_info_new (gimp,
                                  tool_class.type(),
                                  options_class.type(),
                                   GimpContextPropMask(0),
                                  "gimp-image-generator-tool",
                                  _("ImageGenerator"),
                                  _("Editing perspective guide"),
                                  N_("_ImageGenerator"),
                                  "g",
                                  NULL,
                                  "gimp-image-generator",
                                  "gimp-paintbrush",
                                  GIMP_STOCK_IMAGE);

  g_object_set (tool_info, "visible", TRUE, NULL);
  g_object_set_data (G_OBJECT (tool_info), "gimp-tool-default-visible",
                     GINT_TO_POINTER (TRUE));

  g_object_set_data (G_OBJECT (tool_info), "gimp-tool-options-gui-func",
                     (gpointer)gimp_image_generator_options_gui);

  g_object_set_data (G_OBJECT (tool_info), "gimp-tool-options-gui-horizontal-func",
                     (gpointer)gimp_image_generator_options_gui_horizontal);

  gimp_tools_register (gimp, tool_info);
}

GType   gimp_image_generator_tool_get_type (void)
{
  return ToolClass::get_type();
}

};