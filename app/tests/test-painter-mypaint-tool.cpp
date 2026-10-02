/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gegl.h>
#include <gtk/gtk.h>
#include <vector>
extern "C" {
#include "libgimpbase/gimpbase.h"
#include "libgimpmath/gimpmath.h"
#include "libgimpwidgets/gimpwidgets.h"
#include "core/core-types.h"
#include "display/display-types.h"
#include "tools/tools-types.h"
#include "config/gimpdisplayconfig.h"
#include "core/gimp.h"
#include "core/gimpcontainer.h"
#include "core/gimpcontext.h"
#include "core/gimpdrawable.h"
#include "core/gimpimage.h"
#include "core/gimpimage-undo.h"
#include "core/gimppickable.h"
#include "core/gimpprojection.h"
#include "display/gimpcanvasitem.h"
#include "core/gimpimage-perspective-guide.h"
#include "core/gimppaintinfo.h"
#include "core/gimptoolinfo.h"
#include "core/gimpundostack.h"
#include "display/gimpdisplay.h"
#include "display/gimpdisplayshell.h"
#include "display/gimpdisplayshell-tool-events.h"
#include "display/gimpdisplayshell-transform.h"
#include "display/gimpdisplayshell-rotate.h"
#include "display/gimpdisplayshell-scale.h"
#include "tools/gimppaintermybrushtool.h"
#include "tools/gimpmybrushtool.h"
#include "tools/gimptoolcontrol.h"
#include "tools/tool_manager.h"
#include "paint/gimppainterpaintgate.h"
#include "widgets/gimpwidgets-utils.h"
#include "gimpcoreapp.h"
#include "gimp-app-test-utils.h"
#include "tests.h"
}
#include "paint/painter-mypaint-surface/gimp-painter-options.hpp"
#include "core/gimppaintermybrush-handle.hpp"
using namespace GimpPainter;
static Gimp*gimp;
struct Scene {
  GimpDisplay*display;GimpImage*image;GimpDrawable*drawable;GimpTool*tool;GimpPainterMybrushOptions*options;GimpCoords coords=GIMP_COORDS_DEFAULT_VALUES;guint32 time=100;
  Scene(){
    gimp_set_focused_once(gimp);gimp_test_utils_create_image(gimp,128,128);gimp_test_run_mainloop_until_idle();
    display=GIMP_DISPLAY(gimp_get_display_iter(gimp)->data);image=gimp_display_get_image(display);
    auto*list=gimp_image_get_selected_drawables(image);g_assert_nonnull(list);drawable=GIMP_DRAWABLE(g_object_ref(list->data));g_list_free(list);
    gegl_buffer_clear(gimp_drawable_get_buffer(drawable),nullptr);
    auto*info=gimp_get_tool_info(gimp,"gimp-painter-mypaint-tool");g_assert_nonnull(info);gimp_context_set_tool(gimp_get_user_context(gimp),info);
    tool=GIMP_TOOL(g_object_ref(tool_manager_get_active(gimp)));g_assert_true(GIMP_IS_PAINTER_MYBRUSH_TOOL(tool));options=GIMP_PAINTER_MYBRUSH_OPTIONS(gimp_tool_get_options(tool));
    MyPaint::Resource r;r.set_base_value(BRUSH_DABS_PER_SECOND,40);r.set_base_value(BRUSH_RADIUS_LOGARITHMIC,1.5);g_assert_true(gimp_painter_mybrush_options_set_json(options,r.encode().c_str(),nullptr));
    auto*color=gegl_color_new("red");gimp_context_set_foreground(GIMP_CONTEXT(options),color);g_object_unref(color);
    coords.x=32;coords.y=32;coords.pressure=1;gimp_image_undo_free(image);
  }
  ~Scene(){gimp_context_set_tool(gimp_get_user_context(gimp),gimp_get_tool_info(gimp,"gimp-paintbrush-tool"));if(tool)g_object_unref(tool);g_object_unref(drawable);g_object_unref(image);gimp_display_close(display);gimp_test_run_mainloop_until_idle();}
  std::vector<guchar>pixels(){std::vector<guchar>v(128*128*4);gegl_buffer_get(gimp_drawable_get_buffer(drawable),GEGL_RECTANGLE(0,0,128,128),1,babl_format("R'G'B'A u8"),v.data(),GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);return v;}
  int depth(){return gimp_undo_stack_get_depth(gimp_image_get_undo_stack(image));}
  void motion(bool down,guint32 elapsed=20){time+=elapsed;gimp_tool_motion(tool,&coords,time,down?GDK_BUTTON1_MASK:GdkModifierType(0),display);}
  void press(){gimp_tool_button_press(tool,&coords,time,GdkModifierType(0),GIMP_BUTTON_PRESS_NORMAL,display);}
  void release(){time+=20;gimp_tool_button_release(tool,&coords,time,GdkModifierType(0),display);}
};
static void registration_and_gate()
{
  Scene s;g_assert_true(s.tool->want_full_motion_tracking);g_assert_false(s.tool->disable_lazy_snap);g_assert_false(GIMP_IS_MYBRUSH_TOOL(s.tool));
  g_assert_cmpuint(s.tool->tool_info->paint_info->paint_type,==,GIMP_TYPE_PAINTER_PAINT_GATE);
  auto*standard=gimp_get_tool_info(gimp,"gimp-mypaint-brush-tool");g_assert_nonnull(standard);g_assert_cmpstr(standard->menu_accel,==,"Y");
  g_test_message("registration: image=%p drawable-image=%p before options GUI",s.image,gimp_item_get_image(GIMP_ITEM(s.drawable)));
  GtkWidget*gui=gimp_tools_get_tool_options_gui(GIMP_TOOL_OPTIONS(s.options));g_assert_nonnull(gui);
  g_test_message("registration: before refusing paint-core start");
  auto*gate=GIMP_PAINT_CORE(g_object_new(GIMP_TYPE_PAINTER_PAINT_GATE,nullptr));GList drawables={s.drawable,nullptr,nullptr};GError*error=nullptr;
  auto before=s.pixels();g_assert_false(gimp_paint_core_start(gate,&drawables,GIMP_PAINT_OPTIONS(s.options),&s.coords,&error));g_assert_nonnull(error);g_assert_nonnull(strstr(error->message,"Stroke Path/PDB"));g_clear_error(&error);g_object_unref(gate);g_assert_true(s.pixels()==before);g_assert_cmpint(s.depth(),==,0);g_test_message("registration: before scene destruction");
}
static void hover_default_pressure()
{
  Scene s;const auto before=s.pixels();for(double pressure:{1.,0.}){s.coords.pressure=pressure;for(int i=0;i<8;++i){s.coords.x+=2;s.motion(false,50);}}
  g_assert_true(s.pixels()==before);g_assert_cmpint(s.depth(),==,0);g_assert_false(gimp_painter_mybrush_tool_has_pending_stroke(GIMP_PAINTER_MYBRUSH_TOOL(s.tool)));
}
static void pressure_and_undo()
{
  Scene s;const auto before=s.pixels();s.coords.pressure=0;s.motion(false);s.press();s.coords.pressure=.2;s.motion(true,0);s.coords.pressure=.9;s.motion(true,0);s.motion(true,75);
  auto painted=s.pixels();g_assert_true(painted!=before);s.release();gimp_tool_control(s.tool,GIMP_TOOL_ACTION_COMMIT,s.display);
  g_assert_cmpint(s.depth(),==,1);g_assert_true(gimp_image_undo(s.image));g_assert_true(s.pixels()==before);g_assert_true(gimp_image_redo(s.image));g_assert_true(s.pixels()==painted);
  s.coords.x=80;s.motion(false);g_assert_true(s.pixels()==painted);g_assert_cmpint(s.depth(),==,1);
}
static void cancel_rolls_back()
{
  Scene s;const auto before=s.pixels();s.motion(false);s.press();s.coords.x+=20;s.motion(true,75);g_assert_true(s.pixels()!=before);
  gimp_tool_button_release(s.tool,&s.coords,s.time+20,GDK_BUTTON3_MASK,s.display);g_assert_true(s.pixels()==before);g_assert_cmpint(s.depth(),==,0);g_assert_false(gimp_tool_control_is_active(s.tool->control));
}
static void context_editor_contract()
{
  Scene s;auto*context=gimp_context_new(gimp,"ordinary editor context",nullptr);auto source=PainterMybrushRef::create("editor-selected");auto r=source.snapshot();r.set_base_value(BRUSH_OPAQUE,.234);source.replace(r);gimp_context_set_painter_mybrush(context,source.get());GError*error=nullptr;
  auto*model=gimp_painter_mybrush_options_ref_for_context(context,&error);g_assert_no_error(error);g_assert_true(model==s.options);g_assert_true(gimp_context_get_painter_mybrush(GIMP_CONTEXT(model))==source.get());g_assert_cmpfloat_with_epsilon(PainterOptionsRef::retain(model).snapshot().base_value(BRUSH_OPAQUE),.234,1e-6);g_object_unref(model);
  gimp_context_set_painter_mybrush(context,nullptr);model=gimp_painter_mybrush_options_ref_for_context(context,&error);g_assert_no_error(error);g_assert_nonnull(model);g_assert_true(gimp_context_get_painter_mybrush(GIMP_CONTEXT(model))==GIMP_PAINTER_MYBRUSH(gimp_painter_mybrush_get_standard(context)));g_object_unref(model);
  g_assert_null(gimp_painter_mybrush_options_ref_for_context(nullptr,&error));g_assert_nonnull(error);g_clear_error(&error);g_object_run_dispose(G_OBJECT(context));g_assert_null(gimp_painter_mybrush_options_ref_for_context(context,&error));g_assert_error(error,GIMP_PAINTER_ERROR,GIMP_PAINTER_ERROR_CLOSED);g_clear_error(&error);g_object_unref(context);
}
static void send_button(Scene&s,GdkEventType type,guint state,double x,double y)
{
  auto*shell=gimp_display_get_shell(s.display);auto*event=gdk_event_new(type);double dx,dy;gimp_display_shell_transform_xy_f(shell,x,y,&dx,&dy);
  event->any.window=GDK_WINDOW(g_object_ref(gtk_widget_get_window(shell->canvas)));event->any.send_event=TRUE;
  auto*device=gdk_seat_get_pointer(gdk_display_get_default_seat(gtk_widget_get_display(shell->canvas)));gdk_event_set_device(event,device);gdk_event_set_source_device(event,device);
  event->button.button=1;event->button.state=state;event->button.x=dx;event->button.y=dy;event->button.time=GDK_CURRENT_TIME;gimp_display_shell_canvas_tool_events(shell->canvas,event,shell);gdk_event_free(event);
}
static void ruler_saved_pressure()
{
  Scene s;auto*shell=gimp_display_get_shell(s.display);shell->display->config->use_event_history=FALSE;gtk_widget_grab_focus(shell->canvas);gimp_display_shell_scale(shell,GIMP_ZOOM_TO,1,GIMP_ZOOM_FOCUS_IMAGE_CENTER);
  auto*guide=gimp_perspective_guide_new(0);gimp_perspective_guide_add_vanish_points(guide,1000,1000);gimp_image_set_perspective_guide(s.image,guide);g_object_unref(guide);gimp_display_shell_set_perspective_snap(shell,TRUE);
  send_button(s,GDK_BUTTON_PRESS,0,32,32);g_assert_true(shell->perspective_pending);shell->perspective_origin.pressure=.23;shell->perspective_origin.xtilt=.11;shell->perspective_origin.ytilt=-.42;
  GimpCoords next=shell->perspective_origin;next.x=72;next.pressure=.91;next.xtilt=.67;
  gimp_display_shell_perspective_motion(shell,&next,120,GDK_BUTTON1_MASK,TRUE);
  GimpCoords saved;g_assert_true(gimp_painter_mybrush_tool_get_last_press(GIMP_PAINTER_MYBRUSH_TOOL(s.tool),&saved));g_assert_cmpfloat(saved.pressure,==,.23);g_assert_cmpfloat(saved.xtilt,==,.11);g_assert_cmpfloat(saved.ytilt,==,-.42);
  send_button(s,GDK_BUTTON_RELEASE,GDK_BUTTON1_MASK,72,32);gimp_display_shell_set_perspective_snap(shell,FALSE);
}
struct During {Scene*scene;bool fired=false;bool drop=false;};
static void halt_on_update(GimpDrawable*,gint,gint,gint,gint,gpointer data)
{
  auto&s=*static_cast<During*>(data);if(s.fired)return;s.fired=true;
  if(s.drop){gimp_context_set_tool(gimp_get_user_context(gimp),gimp_get_tool_info(gimp,"gimp-paintbrush-tool"));g_clear_object(&s.scene->tool);}
  else gimp_tool_control(s.scene->tool,GIMP_TOOL_ACTION_HALT,s.scene->display);
}
static void mark_finalized(gpointer data,GObject*){*static_cast<bool*>(data)=true;}
static void halt_and_last_ref_in_motion()
{
  for(bool drop:{false,true}){Scene s;auto before=s.pixels();s.motion(false);s.press();During state{&s,false,drop};bool finalized=false;g_object_weak_ref(G_OBJECT(s.tool),mark_finalized,&finalized);auto id=g_signal_connect(s.drawable,"update",G_CALLBACK(halt_on_update),&state);
    s.coords.x+=20;s.motion(true,75);g_assert_true(state.fired);g_test_message("explicit HALT drop=%d unchanged=%d depth=%d",drop,s.pixels()==before,s.depth());g_assert_true(s.pixels()!=before);g_assert_cmpint(s.depth(),==,1);g_assert_false(gimp_viewable_preview_is_frozen(GIMP_VIEWABLE(s.drawable)));
    if(drop){g_assert_null(s.tool);g_assert_true(finalized);}else{g_assert_false(gimp_tool_control_is_active(s.tool->control));g_object_weak_unref(G_OBJECT(s.tool),mark_finalized,&finalized);}g_signal_handler_disconnect(s.drawable,id);
  }
}
static void image_change_finishes_old()
{
  Scene s;const auto before=s.pixels();s.motion(false);s.press();s.coords.x+=20;s.motion(true,75);g_assert_true(s.pixels()!=before);
  const auto painted=s.pixels();
  auto*other=gimp_image_new(gimp,128,128,GIMP_RGB,GIMP_PRECISION_U8_NON_LINEAR);gimp_display_set_image(s.display,other);
  // Native display replacement HALTs the active tool before replacing image;
  // the legacy tool's HALT commits, rather than silently discarding this stroke.
  g_assert_true(s.pixels()==painted);g_assert_cmpint(s.depth(),==,1);g_assert_false(gimp_painter_mybrush_tool_has_pending_stroke(GIMP_PAINTER_MYBRUSH_TOOL(s.tool)));
  gimp_display_set_image(s.display,s.image);g_object_unref(other);s.motion(false);g_assert_true(s.pixels()==painted);g_assert_cmpint(s.depth(),==,1);
}
static void saving_finishes_pending()
{
  Scene s;s.motion(false);s.press();s.coords.x+=20;s.motion(true,75);auto before=s.pixels();g_assert_true(gimp_painter_mybrush_tool_has_pending_stroke(GIMP_PAINTER_MYBRUSH_TOOL(s.tool)));
  g_assert_false(gimp_image_has_pending_paint(s.image));gimp_image_saving(s.image);g_assert_false(gimp_painter_mybrush_tool_has_pending_stroke(GIMP_PAINTER_MYBRUSH_TOOL(s.tool)));g_assert_true(s.pixels()==before);g_assert_cmpint(s.depth(),==,1);s.release();
}
static void save_preflight_on_update(GimpDrawable*,gint,gint,gint,gint,gpointer data)
{auto&s=*static_cast<During*>(data);if(s.fired)return;s.fired=true;g_assert_true(gimp_image_has_pending_paint(s.scene->image));}
static void save_defers_during_sample()
{
  Scene s;s.motion(false);s.press();During state{&s};auto id=g_signal_connect(s.drawable,"update",G_CALLBACK(save_preflight_on_update),&state);s.coords.x+=20;s.motion(true,75);g_assert_true(state.fired);g_signal_handler_disconnect(s.drawable,id);s.release();
}
static void outline_and_projection()
{
  Scene s;gimp_tool_oper_update(s.tool,&s.coords,GdkModifierType(0),TRUE,s.display);auto*draw=GIMP_DRAW_TOOL(s.tool);gimp_test_run_mainloop_until_idle();g_assert_nonnull(draw->item);auto*before=gimp_canvas_item_get_extents(draw->item);g_assert_nonnull(before);
  s.motion(false);s.press();const auto last_draw=draw->last_draw_time;s.coords.x+=30;s.motion(true,75);
  // DrawTool intentionally throttles outlines. An idle-only drain need not
  // dispatch its future timeout; await the actual redraw, with a test deadline.
  const gint64 deadline=g_get_monotonic_time()+G_TIME_SPAN_SECOND;
  while(draw->last_draw_time==last_draw&&g_get_monotonic_time()<deadline)g_main_context_iteration(nullptr,FALSE);
  g_assert_cmpuint(draw->last_draw_time,>,last_draw);auto*after=gimp_canvas_item_get_extents(draw->item);g_assert_nonnull(after);g_assert_false(cairo_region_equal(before,after));cairo_region_destroy(before);cairo_region_destroy(after);
  guchar color[4]={};g_assert_true(gimp_pickable_get_pixel_at(GIMP_PICKABLE(gimp_image_get_projection(s.image)),50,32,babl_format("R'G'B'A u8"),color));g_assert_cmpint(color[0],>,0);s.release();
}
static void drop_on_preview_freeze(GObject*object,GParamSpec*,gpointer data)
{
  auto**owner=static_cast<GimpTool**>(data);if(*owner&&gimp_viewable_preview_is_frozen(GIMP_VIEWABLE(object)))g_clear_object(owner);
}
static void public_press_drops_last_ref()
{
  Scene s;auto*tool=GIMP_TOOL(g_object_new(GIMP_TYPE_PAINTER_MYBRUSH_TOOL,"tool-info",s.tool->tool_info,nullptr));bool finalized=false;g_object_weak_ref(G_OBJECT(tool),mark_finalized,&finalized);
  auto id=g_signal_connect(s.drawable,"notify::frozen",G_CALLBACK(drop_on_preview_freeze),&tool);const auto before=s.pixels();
  gimp_tool_button_press(tool,&s.coords,s.time,GdkModifierType(0),GIMP_BUTTON_PRESS_NORMAL,s.display);
  g_assert_null(tool);g_assert_true(finalized);g_assert_true(s.pixels()==before);g_assert_cmpint(s.depth(),==,0);g_assert_false(gimp_viewable_preview_is_frozen(GIMP_VIEWABLE(s.drawable)));g_signal_handler_disconnect(s.drawable,id);
}
static void drop_without_halt(GimpDrawable*,gint,gint,gint,gint,gpointer data)
{auto**owner=static_cast<GimpTool**>(data);if(*owner)g_clear_object(owner);}
static void last_ref_without_halt_rolls_back()
{
  Scene s;auto*selected=s.tool;s.tool=GIMP_TOOL(g_object_new(GIMP_TYPE_PAINTER_MYBRUSH_TOOL,"tool-info",selected->tool_info,nullptr));bool finalized=false;g_object_weak_ref(G_OBJECT(s.tool),mark_finalized,&finalized);
  const auto before=s.pixels();s.motion(false);s.press();auto id=g_signal_connect(s.drawable,"update",G_CALLBACK(drop_without_halt),&s.tool);s.coords.x+=20;s.motion(true,75);
  g_assert_null(s.tool);g_assert_true(finalized);g_assert_true(s.pixels()==before);g_assert_cmpint(s.depth(),==,0);g_signal_handler_disconnect(s.drawable,id);s.tool=selected;
}
int main(int argc,char**argv)
{
  g_test_init(&argc,&argv,nullptr);if(!gtk_init_check(&argc,&argv))return GIMP_EXIT_TEST_SKIPPED;gimp_test_utils_setup_menus_path();gimp=gimp_init_for_gui_testing(TRUE);
  g_test_add_func("/painter-tool/registration-gate",registration_and_gate);
  g_test_add_func("/painter-tool/hover-default-pressure",hover_default_pressure);
  g_test_add_func("/painter-tool/stationary-equal-time-undo",pressure_and_undo);
  g_test_add_func("/painter-tool/cancel",cancel_rolls_back);
  g_test_add_func("/painter-tool/editor-context",context_editor_contract);
  g_test_add_func("/painter-tool/ruler-origin",ruler_saved_pressure);
  g_test_add_func("/painter-tool/halt-last-ref-motion",halt_and_last_ref_in_motion);
  g_test_add_func("/painter-tool/image-change",image_change_finishes_old);
  g_test_add_func("/painter-tool/save-pending",saving_finishes_pending);
  g_test_add_func("/painter-tool/save-preflight",save_defers_during_sample);
  g_test_add_func("/painter-tool/outline-projection",outline_and_projection);
  g_test_add_func("/painter-tool/public-press-last-ref",public_press_drops_last_ref);
  g_test_add_func("/painter-tool/last-ref-without-halt",last_ref_without_halt_rolls_back);
  g_application_run(gimp->app,0,nullptr);int result=gimp_core_app_get_exit_status(GIMP_CORE_APP(gimp->app));g_application_quit(G_APPLICATION(gimp->app));g_clear_object(&gimp->app);return result;
}
