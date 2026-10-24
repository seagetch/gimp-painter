/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gegl.h>
#include <gtk/gtk.h>
#include <vector>
#include <cstring>
#include <array>
#include <cstdio>
#include <type_traits>
extern "C" {
#include "libgimpbase/gimpbase.h"
#include "libgimpmath/gimpmath.h"
#include "libgimpcolor/gimpcolor.h"
#include "libgimpwidgets/gimpwidgets.h"
#include "core/core-types.h"
#include "display/display-types.h"
#include "tools/tools-types.h"
#include "config/gimpdisplayconfig.h"
#include "core/gimp.h"
#include "core/gimp-gui.h"
#include "core/gimpcontainer.h"
#include "core/gimpbrush.h"
#include "core/gimpbrushpipe.h"
#include "core/gimpbrush-private.h"
#include "core/gimptempbuf.h"
#include "core/gimpcontext.h"
#include "core/gimpdrawable.h"
#include "core/gimpimage.h"
#include "core/gimpimage-undo.h"
#include "core/gimpimage-convert-type.h"
#include "core/gimpimage-convert-precision.h"
#include "core/gimpimage-color-profile.h"
#include "gegl/gimp-babl.h"
#include "core/gimplayer.h"
#include "pdb/gimppdb.h"
#include "plug-in/gimppluginprocedure.h"
#include "file/file-save.h"
#include "xcf/xcf.h"
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
#include "tools/gimpmypainttool.h"
#include "tools/gimpcolorpickeroptions.h"
#include "tools/gimpmybrushtool.h"
#include "tools/gimptoolcontrol.h"
#include "tools/tool_manager.h"
#include "paint/gimppainterpaintgate.h"
#include "widgets/gimpdialogfactory.h"
#include "widgets/gimpfgbgeditor.h"
#include "widgets/gimpwindowstrategy.h"
#include "widgets/gimpwidgets-utils.h"
#include "gimpcoreapp.h"
#include "gimp-app-test-utils.h"
#include "tests.h"
void gimp_test_mypaint_colorpicker_c (GimpMypaintTool *tool);
}
#include "paint/painter-mypaint-surface/gimp-painter-options.hpp"
#include "core/gimppaintermybrush-handle.hpp"
#include "tools/gimppaintermybrushtool.hpp"
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
  Scene s;
  g_assert_cmpuint (g_type_parent (G_OBJECT_TYPE (s.tool)), ==, GIMP_TYPE_COLOR_TOOL);
  auto *klass = GIMP_PAINTER_MYBRUSH_TOOL_GET_CLASS (s.tool);
  g_assert_true (GIMP_IS_PAINTER_MYBRUSH_TOOL_CLASS (klass));
  g_assert_true (GIMP_PAINTER_MYBRUSH_TOOL_CLASS (G_OBJECT_GET_CLASS (s.tool)) == klass);
  GTypeQuery query = {}; g_type_query (G_OBJECT_TYPE (s.tool), &query);
  g_assert_cmpuint (query.instance_size, ==, sizeof (GimpPainterMybrushTool));
  g_assert_cmpuint (query.class_size, ==, sizeof (GimpPainterMybrushToolClass));
  g_assert_cmpuint (s.tool->tool_info->tool_type, ==, GIMP_TYPE_PAINTER_MYBRUSH_TOOL);
  g_assert_cmpuint (s.tool->tool_info->tool_options_type, ==, GIMP_TYPE_PAINTER_MYBRUSH_OPTIONS);
  g_assert_true(s.tool->want_full_motion_tracking);g_assert_false(s.tool->disable_lazy_snap);g_assert_false(GIMP_IS_MYBRUSH_TOOL(s.tool));
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
  auto painted=s.pixels();g_assert_true(painted!=before);s.release();painted=s.pixels();gimp_tool_control(s.tool,GIMP_TOOL_ACTION_COMMIT,s.display);
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
static void standalone_lifecycle()
{
  for (int branch = 0; branch < 3; ++branch)
    {
      Scene s;
      const auto before = s.pixels ();
      auto *tool = GIMP_TOOL (g_object_new (GIMP_TYPE_PAINTER_MYBRUSH_TOOL,
                                          "tool-info", s.tool->tool_info, nullptr));
      bool finalized = false;
      // Weak notifications can run during explicit dispose. Test qdata is
      // released only when the final object ownership is actually gone.
      g_object_set_data_full (G_OBJECT (tool), "test-08008-finalized", &finalized,
                             +[](gpointer data) { *static_cast<bool*> (data) = true; });
      if (branch != 0)
        {
          gimp_tool_button_press (tool, &s.coords, s.time, GdkModifierType (0),
                                  GIMP_BUTTON_PRESS_NORMAL, s.display);
          s.coords.x += 20;
          gimp_tool_motion (tool, &s.coords, s.time + 75, GDK_BUTTON1_MASK, s.display);
          g_assert_true (s.pixels () != before);
          g_assert_true (gimp_painter_mybrush_tool_has_pending_stroke (
            GIMP_PAINTER_MYBRUSH_TOOL (tool)));
        }
      if (branch == 2)
        gimp_tool_control (tool, GIMP_TOOL_ACTION_COMMIT, s.display);
      const auto painted = s.pixels ();
      g_object_run_dispose (G_OBJECT (tool));
      g_object_run_dispose (G_OBJECT (tool));
      g_assert_false (gimp_tool_control_is_active (tool->control));
      g_assert_false (gimp_viewable_preview_is_frozen (GIMP_VIEWABLE (s.drawable)));
      g_assert_null (tool->display); g_assert_null (tool->drawables);
      g_assert_false (finalized);
      g_object_unref (tool);
      g_assert_true (finalized);
      if (branch == 2)
        {
          g_assert_cmpint (s.depth (), ==, 1);
          g_assert_true (s.pixels () == painted);
          g_assert_true (gimp_image_undo (s.image));
          g_assert_true (s.pixels () == before);
          g_assert_true (gimp_image_redo (s.image));
          g_assert_true (s.pixels () == painted);
        }
      else
        {
          g_assert_cmpint (s.depth (), ==, 0);
          g_assert_true (s.pixels () == before);
        }
    }
}
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
  auto*other=gimp_image_new(gimp,128,128,GIMP_RGB,GIMP_PRECISION_U8_NON_LINEAR);
  // Native HALT synthesizes a release before control(). Give it a defined
  // positive elapsed interval so the test cannot accidentally omit the tail.
  s.tool->last_pointer_time-=100;gimp_display_set_image(s.display,other);
  // Native display replacement HALTs the active tool before replacing image;
  // the legacy tool's HALT commits, rather than silently discarding this stroke.
  const auto released=s.pixels();g_assert_true(released!=before);
  for(std::size_t i=3;i<released.size();i+=4)g_assert_cmpuint(released[i],>=,painted[i]);
  g_assert_cmpint(s.depth(),==,1);g_assert_false(gimp_painter_mybrush_tool_has_pending_stroke(GIMP_PAINTER_MYBRUSH_TOOL(s.tool)));
  g_assert_true(gimp_image_undo(s.image));g_assert_true(s.pixels()==before);g_assert_true(gimp_image_redo(s.image));g_assert_true(s.pixels()==released);
  gimp_display_set_image(s.display,s.image);g_object_unref(other);s.motion(false);g_assert_true(s.pixels()==released);g_assert_cmpint(s.depth(),==,1);
}
static void saving_finishes_pending()
{
  Scene s;const auto initial=s.pixels();s.motion(false);s.press();s.coords.x+=20;s.motion(true,75);auto before=s.pixels();g_assert_true(gimp_painter_mybrush_tool_has_pending_stroke(GIMP_PAINTER_MYBRUSH_TOOL(s.tool)));
  g_assert_false(gimp_image_has_pending_paint(s.image));s.tool->last_pointer_time-=100;gimp_image_saving(s.image);
  const auto saved=s.pixels();g_assert_true(saved!=initial);
  for(std::size_t i=3;i<saved.size();i+=4)g_assert_cmpuint(saved[i],>=,before[i]);
  g_assert_false(gimp_painter_mybrush_tool_has_pending_stroke(GIMP_PAINTER_MYBRUSH_TOOL(s.tool)));g_assert_cmpint(s.depth(),==,1);s.release();g_assert_true(s.pixels()==saved);
}
struct SaveDispose {Scene*scene;bool fired=false;};
static void dispose_during_save(GimpImage*,GimpDirtyMask,gpointer data){auto&o=*static_cast<SaveDispose*>(data);if(o.fired)return;o.fired=true;g_assert_true(gimp_image_has_pending_paint(o.scene->image));g_object_run_dispose(G_OBJECT(o.scene->tool));g_object_run_dispose(G_OBJECT(o.scene->tool));}
static void saving_disposal(){Scene s;auto initial=s.pixels();s.motion(false);s.press();s.coords.x+=20;s.motion(true,75);SaveDispose state{&s,false};auto id=g_signal_connect(s.image,"dirty",G_CALLBACK(dispose_during_save),&state);gimp_image_saving(s.image);g_signal_handler_disconnect(s.image,id);g_assert_true(state.fired);g_assert_false(gimp_image_has_pending_paint(s.image));g_assert_false(gimp_viewable_preview_is_frozen(GIMP_VIEWABLE(s.drawable)));g_assert_cmpint(s.depth(),==,1);g_assert_true(s.pixels()!=initial);g_assert_true(gimp_image_undo(s.image));g_assert_true(s.pixels()==initial);}
static void external_dirty_still_cancels(){Scene s;auto initial=s.pixels();s.motion(false);s.press();s.coords.x+=20;s.motion(true,75);gimp_image_dirty(s.image,GIMP_DIRTY_DRAWABLE);g_assert_false(gimp_tool_control_is_active(s.tool->control));g_assert_false(gimp_painter_mybrush_tool_has_pending_stroke(GIMP_PAINTER_MYBRUSH_TOOL(s.tool)));g_assert_true(s.pixels()==initial);g_assert_false(gimp_image_has_pending_paint(s.image));}
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
  guchar native[4]={};g_assert_true(gimp_pickable_get_pixel_at(GIMP_PICKABLE(s.drawable),50,32,babl_format("R'G'B'A u8"),native));g_assert_cmpint(native[0],>,0);
  // Projection rendering is scheduled separately from DrawTool's outline timer.
  // The Pickable API explicitly flushes pending projection invalidations.
  auto*projection=GIMP_PICKABLE(gimp_image_get_projection(s.image));gimp_pickable_flush(projection);
  guchar color[4]={};g_assert_true(gimp_pickable_get_pixel_at(projection,50,32,babl_format("R'G'B'A u8"),color));g_assert_cmpint(color[0],>,0);s.release();
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
static std::vector<guchar> native_pixels(GimpDrawable*d)
{
  auto*buffer=gimp_drawable_get_buffer(d);const auto*format=gegl_buffer_get_format(buffer);
  std::vector<guchar>p(128*128*babl_format_get_bytes_per_pixel(format));
  gegl_buffer_get(buffer,GEGL_RECTANGLE(0,0,128,128),1,format,p.data(),GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);return p;
}
static void gray_tool_save_roundtrip()
{
  for(bool alpha:{false,true})for(bool floating:{false,true}){
    Scene s;GError*error=nullptr;
    g_assert_true(gimp_image_convert_type(s.image,GIMP_GRAY,nullptr,nullptr,&error));g_assert_no_error(error);
    if(!alpha)gimp_layer_remove_alpha(GIMP_LAYER(s.drawable),GIMP_CONTEXT(s.options));
    auto*buffer=gimp_drawable_get_buffer(s.drawable);const auto*format=gegl_buffer_get_format(buffer);
    const int channels=babl_format_get_bytes_per_pixel(format);g_assert_cmpint(channels,==,alpha?2:1);
    std::vector<guchar>initial(128*128*channels);for(std::size_t i=0;i<initial.size();i+=channels){initial[i]=32;if(alpha)initial[i+1]=80;}
    gegl_buffer_set(buffer,GEGL_RECTANGLE(0,0,128,128),0,format,initial.data(),GEGL_AUTO_ROWSTRIDE);
    gimp_drawable_update(s.drawable,0,0,128,128);gimp_image_undo_free(s.image);
    g_object_set(s.options,"non-incremental",floating,"stroke-opacity",.37,nullptr);
    s.motion(false);s.press();s.coords.x+=20;s.motion(true,75);
    auto painted=native_pixels(s.drawable);g_assert_true(painted!=initial);
    g_assert_true(gimp_painter_mybrush_tool_has_pending_stroke(GIMP_PAINTER_MYBRUSH_TOOL(s.tool)));
    auto*procedure=gimp_pdb_lookup_procedure(gimp->pdb,"gimp-xcf-save");g_assert_true(GIMP_IS_PLUG_IN_PROCEDURE(procedure));
    GFileIOStream*io=nullptr;auto*file=g_file_new_tmp("painter-gray-tool-XXXXXX.xcf",&io,&error);g_assert_no_error(error);g_assert_nonnull(file);g_assert_true(g_io_stream_close(G_IO_STREAM(io),nullptr,&error));g_assert_no_error(error);g_object_unref(io);
    s.tool->last_pointer_time-=100; // Save COMMIT also delivers the pending release tail.
    g_assert_cmpint(file_save(gimp,s.image,nullptr,file,GIMP_PLUG_IN_PROCEDURE(procedure),GIMP_RUN_NONINTERACTIVE,TRUE,FALSE,FALSE,&error),==,GIMP_PDB_SUCCESS);g_assert_no_error(error);
    painted=native_pixels(s.drawable);g_assert_true(painted!=initial);
    g_assert_false(gimp_painter_mybrush_tool_has_pending_stroke(GIMP_PAINTER_MYBRUSH_TOOL(s.tool)));g_assert_cmpint(s.depth(),==,1);g_assert_true(native_pixels(s.drawable)==painted);
    if(gimp_tool_control_is_active(s.tool->control))s.release();
    auto*stream=g_file_read(file,nullptr,&error);g_assert_no_error(error);auto*loaded=xcf_load_stream(gimp,G_INPUT_STREAM(stream),file,nullptr,&error);g_assert_no_error(error);g_assert_nonnull(loaded);g_object_unref(stream);
    g_assert_cmpint(gimp_image_get_base_type(loaded),==,GIMP_GRAY);auto*layers=gimp_image_get_layer_list(loaded);g_assert_nonnull(layers);auto*d=GIMP_DRAWABLE(layers->data);
    g_assert_cmpint(babl_format_get_bytes_per_pixel(gegl_buffer_get_format(gimp_drawable_get_buffer(d))),==,channels);g_assert_true(native_pixels(d)==painted);g_list_free(layers);g_object_unref(loaded);
    g_assert_true(gimp_image_undo(s.image));g_assert_true(native_pixels(s.drawable)==initial);g_assert_true(gimp_image_redo(s.image));g_assert_true(native_pixels(s.drawable)==painted);
    g_assert_true(g_file_delete(file,nullptr,&error));g_assert_no_error(error);g_object_unref(file);
  }
}

static void precision_tool_save_roundtrip()
{
  const GimpPrecision precisions[]={GIMP_PRECISION_U8_LINEAR,GIMP_PRECISION_U8_NON_LINEAR,GIMP_PRECISION_U8_PERCEPTUAL,
    GIMP_PRECISION_U16_LINEAR,GIMP_PRECISION_U16_NON_LINEAR,GIMP_PRECISION_U16_PERCEPTUAL,
    GIMP_PRECISION_U32_LINEAR,GIMP_PRECISION_U32_NON_LINEAR,GIMP_PRECISION_U32_PERCEPTUAL,
    GIMP_PRECISION_HALF_LINEAR,GIMP_PRECISION_HALF_NON_LINEAR,GIMP_PRECISION_HALF_PERCEPTUAL,
    GIMP_PRECISION_FLOAT_LINEAR,GIMP_PRECISION_FLOAT_NON_LINEAR,GIMP_PRECISION_FLOAT_PERCEPTUAL,
    GIMP_PRECISION_DOUBLE_LINEAR,GIMP_PRECISION_DOUBLE_NON_LINEAR,GIMP_PRECISION_DOUBLE_PERCEPTUAL};
  for(unsigned index=0;index<G_N_ELEMENTS(precisions);++index){
    Scene s;GError*error=nullptr;const bool gray=index%2,alpha=(index/2)%2,floating=(index/3)%2,custom=index%3;
    if(gray){g_assert_true(gimp_image_convert_type(s.image,GIMP_GRAY,nullptr,nullptr,&error));g_assert_no_error(error);}
    if(!alpha)gimp_layer_remove_alpha(GIMP_LAYER(s.drawable),GIMP_CONTEXT(s.options));
    if(gimp_image_get_precision(s.image)!=precisions[index])gimp_image_convert_precision(s.image,precisions[index],GEGL_DITHER_NONE,GEGL_DITHER_NONE,GEGL_DITHER_NONE,nullptr);
    if(custom){auto*profile=gray?gimp_color_profile_new_d50_gray_lab_trc():gimp_color_profile_new_rgb_adobe();g_assert_true(gimp_image_assign_color_profile(s.image,profile,nullptr,&error));g_assert_no_error(error);g_object_unref(profile);}
    auto*profile=gimp_image_get_color_profile(s.image);if(!profile)profile=gimp_image_get_builtin_color_profile(s.image);g_object_ref(profile);
    auto*buffer=gimp_drawable_get_buffer(s.drawable);const auto*format=gegl_buffer_get_format(buffer);g_assert_cmpint(gimp_babl_format_get_precision(format),==,precisions[index]);
    auto*fill=gegl_color_new("rgba(0.2,0.3,0.4,0.6)");gegl_buffer_set_color(buffer,nullptr,fill);g_object_unref(fill);
    const auto initial=native_pixels(s.drawable);gimp_drawable_update(s.drawable,0,0,128,128);gimp_image_undo_free(s.image);
    g_object_set(s.options,"non-incremental",floating,"stroke-opacity",.37,nullptr);
    auto*shell=gimp_display_get_shell(s.display);shell->display->config->use_event_history=FALSE;gtk_widget_grab_focus(shell->canvas);gimp_display_shell_scale(shell,GIMP_ZOOM_TO,1,GIMP_ZOOM_FOCUS_IMAGE_CENTER);
    gimp_test_run_mainloop_until_idle();send_button(s,GDK_BUTTON_PRESS,0,32,32);g_assert_true(gimp_tool_control_is_active(s.tool->control));s.coords.x=52;s.coords.y=32;s.coords.pressure=.85;s.motion(true,75);
    g_assert_true(native_pixels(s.drawable)!=initial);g_assert_true(gimp_painter_mybrush_tool_has_pending_stroke(GIMP_PAINTER_MYBRUSH_TOOL(s.tool)));
    auto*procedure=gimp_pdb_lookup_procedure(gimp->pdb,"gimp-xcf-save");g_assert_true(GIMP_IS_PLUG_IN_PROCEDURE(procedure));GFileIOStream*io=nullptr;auto*file=g_file_new_tmp("painter-precision-tool-XXXXXX.xcf",&io,&error);g_assert_no_error(error);g_assert_nonnull(file);g_assert_true(g_io_stream_close(G_IO_STREAM(io),nullptr,&error));g_assert_no_error(error);g_object_unref(io);
    s.tool->last_pointer_time-=100;
    g_assert_cmpint(file_save(gimp,s.image,nullptr,file,GIMP_PLUG_IN_PROCEDURE(procedure),GIMP_RUN_NONINTERACTIVE,TRUE,FALSE,FALSE,&error),==,GIMP_PDB_SUCCESS);g_assert_no_error(error);
    const auto painted=native_pixels(s.drawable);g_assert_true(painted!=initial);g_assert_false(gimp_painter_mybrush_tool_has_pending_stroke(GIMP_PAINTER_MYBRUSH_TOOL(s.tool)));g_assert_cmpint(s.depth(),==,1);
    send_button(s,GDK_BUTTON_RELEASE,GDK_BUTTON1_MASK,52,32);
    auto*stream=g_file_read(file,nullptr,&error);g_assert_no_error(error);auto*loaded=xcf_load_stream(gimp,G_INPUT_STREAM(stream),file,nullptr,&error);g_assert_no_error(error);g_assert_nonnull(loaded);g_object_unref(stream);
    g_assert_cmpint(gimp_image_get_precision(loaded),==,precisions[index]);g_assert_cmpint(gimp_image_get_base_type(loaded),==,gray?GIMP_GRAY:GIMP_RGB);auto*layers=gimp_image_get_layer_list(loaded);g_assert_nonnull(layers);auto*d=GIMP_DRAWABLE(layers->data);auto*loaded_format=gegl_buffer_get_format(gimp_drawable_get_buffer(d));g_assert_cmpstr(babl_format_get_encoding(loaded_format),==,babl_format_get_encoding(format));g_assert_true(native_pixels(d)==painted);auto*loaded_profile=gimp_image_get_color_profile(loaded);if(!loaded_profile)loaded_profile=gimp_image_get_builtin_color_profile(loaded);g_assert_true(gimp_color_profile_is_equal(profile,loaded_profile));g_list_free(layers);g_object_unref(loaded);
    g_assert_true(gimp_image_undo(s.image));g_assert_true(native_pixels(s.drawable)==initial);g_assert_true(gimp_image_redo(s.image));g_assert_true(native_pixels(s.drawable)==painted);
    g_test_message("GTK/XCF precision=%d gray=%d alpha=%d nonincremental=%d custom-profile=%d format=%s",precisions[index],gray,alpha,floating,custom,babl_get_name(format));g_assert_true(g_file_delete(file,nullptr,&error));g_assert_no_error(error);g_object_unref(file);g_object_unref(profile);
  }
}

static void pipe_release_save_roundtrip()
{
  Scene s;GError*error=nullptr;
  auto*pipe=GIMP_BRUSH_PIPE(g_object_new(GIMP_TYPE_BRUSH_PIPE,"name","Active tool pipe",nullptr));
  pipe->n_brushes=4;pipe->brushes=g_new0(GimpBrush*,4);
  for(int i=0;i<4;++i){auto*b=GIMP_BRUSH(g_object_new(GIMP_TYPE_BRUSH,"name","Active tool cell",nullptr));b->priv->mask=gimp_temp_buf_new(5,3,babl_format("Y u8"));std::memset(gimp_temp_buf_get_data(b->priv->mask),60+i*60,15);pipe->brushes[i]=b;}
  g_assert_true(gimp_brush_pipe_set_params(pipe,nullptr));pipe->current=pipe->brushes[0];GIMP_BRUSH(pipe)->priv->mask=pipe->brushes[0]->priv->mask;
  gimp_context_set_brush(GIMP_CONTEXT(s.options),GIMP_BRUSH(pipe));g_object_set(s.options,"use-gimp-brushmark",TRUE,nullptr);
  const auto initial=s.pixels();s.motion(false);s.press();s.coords.x+=8;s.motion(true,75);
  const auto pressed=s.pixels();g_assert_true(pressed!=initial);
  // Actual registered release dispatch must preserve the delayed tail.
  s.coords.x+=12;s.time+=100;s.release();const auto released=s.pixels();g_assert_true(released!=pressed);
  const int index=pipe->index[0];auto*child=pipe->current;
  // A physical release now seals the paired entry immediately, after its tail.
  g_assert_false(gimp_painter_mybrush_tool_has_pending_stroke(GIMP_PAINTER_MYBRUSH_TOOL(s.tool)));
  g_assert_cmpint(s.depth(),==,1);g_assert_cmpint(GIMP_BRUSH(pipe)->priv->use_count,==,0);
  auto*procedure=gimp_pdb_lookup_procedure(gimp->pdb,"gimp-xcf-save");g_assert_true(GIMP_IS_PLUG_IN_PROCEDURE(procedure));
  GFileIOStream*io=nullptr;auto*file=g_file_new_tmp("painter-pipe-tool-XXXXXX.xcf",&io,&error);g_assert_no_error(error);g_assert_nonnull(file);
  g_assert_true(g_io_stream_close(G_IO_STREAM(io),nullptr,&error));g_assert_no_error(error);g_object_unref(io);
  g_assert_cmpint(file_save(gimp,s.image,nullptr,file,GIMP_PLUG_IN_PROCEDURE(procedure),GIMP_RUN_NONINTERACTIVE,TRUE,FALSE,FALSE,&error),==,GIMP_PDB_SUCCESS);g_assert_no_error(error);
  g_assert_true(s.pixels()==released);g_assert_cmpint(pipe->index[0],==,index);g_assert_true(pipe->current==child);g_assert_cmpint(GIMP_BRUSH(pipe)->priv->use_count,==,0);
  g_assert_false(gimp_painter_mybrush_tool_has_pending_stroke(GIMP_PAINTER_MYBRUSH_TOOL(s.tool)));g_assert_cmpint(s.depth(),==,1);
  auto*stream=g_file_read(file,nullptr,&error);g_assert_no_error(error);auto*loaded=xcf_load_stream(gimp,G_INPUT_STREAM(stream),file,nullptr,&error);g_assert_no_error(error);g_assert_nonnull(loaded);g_object_unref(stream);
  auto*layers=gimp_image_get_layer_list(loaded);g_assert_nonnull(layers);g_assert_true(native_pixels(GIMP_DRAWABLE(layers->data))==released);g_list_free(layers);g_object_unref(loaded);
  g_assert_true(gimp_image_undo(s.image));g_assert_true(s.pixels()==initial);g_assert_cmpint(pipe->index[0],==,index);g_assert_true(pipe->current==child);
  g_assert_true(gimp_image_redo(s.image));g_assert_true(s.pixels()==released);g_assert_cmpint(pipe->index[0],==,index);g_assert_true(pipe->current==child);
  for(int i=0;i<5;++i){s.coords.x+=2;s.motion(false,100);g_assert_true(s.pixels()==released);g_assert_cmpint(pipe->index[0],==,index);}
  g_assert_true(g_file_delete(file,nullptr,&error));g_assert_no_error(error);g_object_unref(file);g_object_unref(pipe);
}

using PickerColor = std::array<gdouble, 4>;

static PickerColor picker_color (GeglColor *color)
{
  PickerColor result;
  gegl_color_get_pixel (color, babl_format ("R'G'B'A double"), result.data ());
  return result;
}

static void assert_picker_color (GeglColor *color, const PickerColor& expected)
{
  const auto actual = picker_color (color);
  for (unsigned i = 0; i < actual.size (); ++i)
    g_assert_cmpfloat_with_epsilon (actual[i], expected[i], 1e-6);
}

static void assert_picker_color (GeglColor *color, const gchar *expected)
{
  auto *reference = gegl_color_new (expected);
  const auto actual = picker_color (color);
  const auto wanted = picker_color (reference);
  for (unsigned i = 0; i < actual.size (); ++i)
    g_assert_cmpfloat_with_epsilon (actual[i], wanted[i], 1. / 255.);
  g_object_unref (reference);
}

static void colorpicker_c_boundary ()
{
  Scene s;
  const auto before = s.pixels ();
  auto *context = GIMP_CONTEXT (s.options);
  const auto foreground = picker_color (gimp_context_get_foreground (context));
  const auto background = picker_color (gimp_context_get_background (context));
  gimp_test_mypaint_colorpicker_c (GIMP_MYPAINT_TOOL (s.tool));
  assert_picker_color (gimp_context_get_foreground (context), foreground);
  assert_picker_color (gimp_context_get_background (context), background);
  g_assert_true (s.pixels () == before);
  g_assert_cmpint (s.depth (), ==, 0);
  g_assert_false (gimp_painter_mybrush_tool_has_pending_stroke (
    GIMP_PAINTER_MYBRUSH_TOOL (s.tool)));
}

static void colorpicker_named_handle ()
{
  static_assert (std::is_same<MypaintToolRef, PainterMybrushToolRef>::value,
                 "The named handle must retain the existing native owner");
  Scene s;
  auto handle = MypaintToolRef::retain (GIMP_MYPAINT_TOOL (s.tool));
  const auto before = s.pixels ();
  auto *context = GIMP_CONTEXT (s.options);
  const auto foreground = picker_color (gimp_context_get_foreground (context));
  const auto background = picker_color (gimp_context_get_background (context));
  for (auto target : { GIMP_COLOR_PICK_TARGET_NONE,
                       GIMP_COLOR_PICK_TARGET_FOREGROUND,
                       GIMP_COLOR_PICK_TARGET_BACKGROUND,
                       GIMP_COLOR_PICK_TARGET_PALETTE })
    {
      handle.enable_color_picker (target);
      g_assert_true (handle.get () == GIMP_PAINTER_MYBRUSH_TOOL (s.tool));
      g_assert_cmpint (GIMP_COLOR_TOOL (s.tool)->pick_target, ==, target);
      g_assert_false (gimp_color_tool_is_enabled (GIMP_COLOR_TOOL (s.tool)));
      g_assert_false (gimp_tool_control_is_active (s.tool->control));
      assert_picker_color (gimp_context_get_foreground (context), foreground);
      assert_picker_color (gimp_context_get_background (context), background);
    }
  // Invalid enum representations are exercised by the actual C consumer.
  // Constructing an out-of-range enum in C++ would itself be undefined.
  handle.enable_color_picker (GIMP_COLOR_PICK_TARGET_PALETTE);
  g_assert_cmpint (GIMP_COLOR_TOOL (s.tool)->pick_target, ==,
                   GIMP_COLOR_PICK_TARGET_PALETTE);
  g_assert_true (s.pixels () == before);
  g_assert_cmpint (s.depth (), ==, 0);
  g_assert_false (gimp_painter_mybrush_tool_has_pending_stroke (handle.get ()));

  auto closed = MypaintToolRef::adopt (GIMP_MYPAINT_TOOL (g_object_new (
    GIMP_TYPE_PAINTER_MYBRUSH_TOOL, "tool-info", s.tool->tool_info, nullptr)));
  closed.enable_color_picker (GIMP_COLOR_PICK_TARGET_BACKGROUND);
  g_object_run_dispose (G_OBJECT (closed.get ()));
  closed.enable_color_picker (GIMP_COLOR_PICK_TARGET_FOREGROUND);
  g_assert_cmpint (GIMP_COLOR_TOOL (closed.get ())->pick_target, ==,
                   GIMP_COLOR_PICK_TARGET_BACKGROUND);
}

/* An opaque, genuinely colored native drawable. All interaction below goes
 * through GimpTool's public dispatch, with the ordinary modifier lifecycle. */
struct PickerScene : Scene
{
  GimpColorPickerOptions *picker;
  GimpColorPickTarget saved_target;
  gboolean saved_merged, saved_average;
  GimpCursorMode saved_cursor_mode;
  GeglColor *saved_foreground, *saved_background;
  std::vector<GimpColorPickState> picked;
  std::vector<GimpColorPickTarget> picked_targets;
  gulong picked_handler;

  PickerScene ()
  {
    auto *info = gimp_get_tool_info (gimp, "gimp-color-picker-tool");
    g_assert_nonnull (info);
    picker = GIMP_COLOR_PICKER_OPTIONS (info->tool_options);
    saved_target = picker->pick_target;
    saved_merged = GIMP_COLOR_OPTIONS (picker)->sample_merged;
    saved_average = GIMP_COLOR_OPTIONS (picker)->sample_average;
    saved_cursor_mode = display->config->cursor_mode;
    auto *context = GIMP_CONTEXT (options);
    saved_foreground = gegl_color_duplicate (gimp_context_get_foreground (context));
    saved_background = gegl_color_duplicate (gimp_context_get_background (context));
    g_object_set (picker, "sample-merged", FALSE, "sample-average", FALSE, nullptr);
    g_object_set (display->config, "cursor-mode", GIMP_CURSOR_MODE_TOOL_ICON, nullptr);
    reset_colors ();
    const GeglRectangle regions[] = {{0, 0, 42, 128}, {42, 0, 43, 128},
                                     {85, 0, 43, 128}};
    const gchar *colors[] = {"blue", "rgb(0.0,1.0,0.0)", "yellow"};
    for (unsigned i = 0; i < G_N_ELEMENTS (regions); ++i)
      {
        auto *color = gegl_color_new (colors[i]);
        gegl_buffer_set_color (gimp_drawable_get_buffer (drawable), &regions[i], color);
        g_object_unref (color);
      }
    gimp_drawable_update (drawable, 0, 0, 128, 128);
    gimp_projection_finish_draw (gimp_image_get_projection (image));
    gimp_projection_flush (gimp_image_get_projection (image));
    gimp_display_flush_now (display);
    gimp_test_run_mainloop_until_idle ();
    gimp_image_undo_free (image);
    gimp_tool_set_focus_display (tool, display);
    picked_handler = g_signal_connect (tool, "picked", G_CALLBACK (+[] (
      GimpColorTool *color_tool, const GimpCoords *, GimpDisplay *,
      GimpColorPickState state, const Babl *format, gpointer, GeglColor *color,
      gpointer data) {
        auto& scene = *static_cast<PickerScene *> (data);
        g_assert_nonnull (format);
        g_assert_true (GEGL_IS_COLOR (color));
        scene.picked.push_back (state);
        scene.picked_targets.push_back (color_tool->pick_target);
      }), this);
  }

  ~PickerScene ()
  {
    g_signal_handler_disconnect (tool, picked_handler);
    if (gimp_tool_control_is_active (tool->control))
      gimp_tool_control (tool, GIMP_TOOL_ACTION_HALT, display);
    gimp_tool_set_modifier_state (tool, GdkModifierType (0), display);
    gimp_tool_set_focus_display (tool, nullptr);
    g_object_set (picker, "pick-target", saved_target,
                  "sample-merged", saved_merged,
                  "sample-average", saved_average, nullptr);
    g_object_set (display->config, "cursor-mode", saved_cursor_mode, nullptr);
    gimp_context_set_foreground (GIMP_CONTEXT (options), saved_foreground);
    gimp_context_set_background (GIMP_CONTEXT (options), saved_background);
    g_object_unref (saved_foreground);
    g_object_unref (saved_background);
  }

  void reset_colors ()
  {
    auto *foreground = gegl_color_new ("red");
    auto *background = gegl_color_new ("rgb(1.0,0.0,1.0)");
    gimp_context_set_foreground (GIMP_CONTEXT (options), foreground);
    gimp_context_set_background (GIMP_CONTEXT (options), background);
    g_object_unref (foreground);
    g_object_unref (background);
  }

  void modifier (bool down)
  {
    gimp_tool_set_modifier_state (tool, down ? gimp_get_constrain_behavior_mask () :
                                              GdkModifierType (0), display);
    g_assert_cmpint (gimp_color_tool_is_enabled (GIMP_COLOR_TOOL (tool)), ==, down);
  }

  void cursor (GimpCursorModifier expected)
  {
    gimp_tool_oper_update (tool, &coords, gimp_get_constrain_behavior_mask (),
                           TRUE, display);
    gimp_tool_cursor_update (tool, &coords, gimp_get_constrain_behavior_mask (), display);
    auto *shell = gimp_display_get_shell (display);
    g_assert_cmpint (shell->current_cursor, ==, GIMP_CURSOR_COLOR_PICKER);
    g_assert_cmpint (shell->tool_cursor, ==, GIMP_TOOL_CURSOR_COLOR_PICKER);
    g_assert_cmpint (shell->cursor_modifier, ==, expected);
  }

  void pick_press ()
  {
    time += 20;
    gimp_tool_button_press (tool, &coords, time, gimp_get_constrain_behavior_mask (),
                            GIMP_BUTTON_PRESS_NORMAL, display);
    g_assert_true (gimp_tool_control_is_active (tool->control));
    g_assert_true (tool->display == display);
  }

  void pick_motion ()
  {
    time += 20;
    gimp_tool_motion (tool, &coords, time,
                     GdkModifierType (gimp_get_constrain_behavior_mask () | GDK_BUTTON1_MASK),
                     display);
  }

  void pick_release ()
  {
    time += 20;
    gimp_tool_button_release (tool, &coords, time,
                              gimp_get_constrain_behavior_mask (), display);
    g_assert_false (gimp_tool_control_is_active (tool->control));
    g_assert_true (tool->display == display);
  }

  void assert_unpainted (const std::vector<guchar>& before)
  {
    g_assert_true (pixels () == before);
    g_assert_cmpint (depth (), ==, 0);
    g_assert_false (gimp_painter_mybrush_tool_has_pending_stroke (
      GIMP_PAINTER_MYBRUSH_TOOL (tool)));
  }
};

static void colorpicker_native_targets ()
{
  PickerScene s;
  const auto before = s.pixels ();
  auto *context = GIMP_CONTEXT (s.options);
  g_assert_cmpint (GIMP_COLOR_TOOL (s.tool)->pick_target, ==,
                   GIMP_COLOR_PICK_TARGET_FOREGROUND);
  for (unsigned cycle = 0; cycle < 3; ++cycle)
    {
      const auto target = cycle == 1 ? GIMP_COLOR_PICK_TARGET_BACKGROUND :
                                      GIMP_COLOR_PICK_TARGET_FOREGROUND;
      const auto opposite = cycle == 1 ? GIMP_COLOR_PICK_TARGET_FOREGROUND :
                                        GIMP_COLOR_PICK_TARGET_BACKGROUND;
      s.reset_colors ();
      g_object_set (s.picker, "pick-target", opposite, nullptr);
      /* First cycle deliberately exercises the unchanged default. */
      if (cycle != 0)
        gimp_mypaint_tool_enable_color_picker (GIMP_MYPAINT_TOOL (s.tool), target);
      g_assert_false (gimp_color_tool_is_enabled (GIMP_COLOR_TOOL (s.tool)));
      g_assert_false (gimp_tool_control_is_active (s.tool->control));
      assert_picker_color (gimp_context_get_foreground (context), "red");
      assert_picker_color (gimp_context_get_background (context), "rgb(1.0,0.0,1.0)");
      s.assert_unpainted (before);
      s.coords.x = 16;
      s.modifier (true);
      s.modifier (true); // Repeated modifier state must not reset configuration.
      g_assert_true (GIMP_COLOR_TOOL (s.tool)->options == GIMP_COLOR_OPTIONS (s.picker));
      s.cursor (cycle == 1 ? GIMP_CURSOR_MODIFIER_BACKGROUND :
                            GIMP_CURSOR_MODIFIER_FOREGROUND);
      const auto count = s.picked.size ();
      const auto check_colors = [&] (const gchar *sample) {
        assert_picker_color (gimp_context_get_foreground (context),
                             cycle == 1 ? "red" : sample);
        assert_picker_color (gimp_context_get_background (context),
                             cycle == 1 ? sample : "rgb(1.0,0.0,1.0)");
        s.assert_unpainted (before);
      };
      s.pick_press ();
      check_colors ("blue");
      s.coords.x = 64;
      s.pick_motion ();
      check_colors ("rgb(0.0,1.0,0.0)");
      s.coords.x = 112;
      s.pick_release ();
      check_colors ("yellow");
      g_assert_cmpuint (s.picked.size (), ==, count + 3);
      for (unsigned i = 0; i < 3; ++i)
        g_assert_cmpint (s.picked_targets[count + i], ==, target);
      g_assert_cmpint (s.picked[count], ==, GIMP_COLOR_PICK_STATE_START);
      g_assert_cmpint (s.picked[count + 1], ==, GIMP_COLOR_PICK_STATE_UPDATE);
      g_assert_cmpint (s.picked[count + 2], ==, GIMP_COLOR_PICK_STATE_END);
      s.modifier (false);
      s.modifier (false);
      g_assert_null (GIMP_COLOR_TOOL (s.tool)->options);
      g_assert_cmpint (GIMP_COLOR_TOOL (s.tool)->pick_target, ==, target);

      // Exercise synthesized release with the default FG, configured BG, and
      // repeated FG target. GimpTool HALT obtains its display from the tool.
      s.coords.x = 16;
      s.modifier (true);
      s.pick_press ();
      const auto before_halt = s.picked.size ();
      gimp_tool_control (s.tool, GIMP_TOOL_ACTION_HALT, s.display);
      g_assert_false (gimp_tool_control_is_active (s.tool->control));
      g_assert_null (s.tool->display);
      g_assert_cmpuint (s.picked.size (), ==, before_halt + 1);
      g_assert_cmpint (s.picked.back (), ==, GIMP_COLOR_PICK_STATE_END);
      g_assert_cmpint (s.picked_targets.back (), ==, target);
      check_colors ("blue");
      s.modifier (false);
      g_assert_null (GIMP_COLOR_TOOL (s.tool)->options);
      s.modifier (true);
      s.cursor (cycle == 1 ? GIMP_CURSOR_MODIFIER_BACKGROUND :
                            GIMP_CURSOR_MODIFIER_FOREGROUND);
      s.pick_press ();
      s.pick_release ();
      s.modifier (false);
      s.assert_unpainted (before);
    }
}

static void colorpicker_display_interruptions (PickerScene& s,
                                               const std::vector<guchar>& before)
{
  auto handle = MypaintToolRef::retain (GIMP_MYPAINT_TOOL (s.tool));
  handle.enable_color_picker (GIMP_COLOR_PICK_TARGET_BACKGROUND);
  s.coords.x = 16;
  s.modifier (true);
  s.pick_press ();
  const auto before_retarget = s.picked.size ();
  auto *other = gimp_image_new (gimp, 128, 128, GIMP_RGB,
                                GIMP_PRECISION_U8_NON_LINEAR);
  // Native retargeting must synthesize END against the old, still-live image.
  gimp_display_set_image (s.display, other);
  g_assert_false (gimp_tool_control_is_active (s.tool->control));
  g_assert_null (s.tool->display);
  g_assert_cmpuint (s.picked.size (), ==, before_retarget + 1);
  g_assert_cmpint (s.picked.back (), ==, GIMP_COLOR_PICK_STATE_END);
  g_assert_cmpint (s.picked_targets.back (), ==, GIMP_COLOR_PICK_TARGET_BACKGROUND);
  assert_picker_color (gimp_context_get_background (GIMP_CONTEXT (s.options)), "blue");
  s.assert_unpainted (before);
  // Repeated HALT must not retain a release against the replaced image.
  gimp_tool_control (s.tool, GIMP_TOOL_ACTION_HALT, s.display);
  g_assert_cmpuint (s.picked.size (), ==, before_retarget + 1);
  gimp_display_set_image (s.display, s.image);
  g_object_unref (other);
  gimp_tool_set_focus_display (s.tool, s.display);
  s.modifier (false);

  // Use a second real display so closing it destroys its shell rather than
  // merely emptying the application's last display. Retain the GObject to
  // distinguish a closed native display from an invalid pointer.
  auto *closed = gimp_create_display (gimp, s.image, gimp_unit_pixel (), 1., nullptr);
  g_assert_nonnull (closed);
  g_object_ref (closed);
  gimp_test_run_mainloop_until_idle ();
  g_assert_true (tool_manager_get_active (gimp) == s.tool);
  gimp_tool_set_focus_display (s.tool, closed);
  gimp_tool_set_modifier_state (s.tool, gimp_get_constrain_behavior_mask (), closed);
  s.time += 20;
  gimp_tool_button_press (s.tool, &s.coords, s.time,
                          gimp_get_constrain_behavior_mask (),
                          GIMP_BUTTON_PRESS_NORMAL, closed);
  g_assert_true (gimp_tool_control_is_active (s.tool->control));
  g_assert_true (s.tool->display == closed);
  const auto before_close = s.picked.size ();
  gimp_display_close (closed);
  g_assert_false (gimp_tool_control_is_active (s.tool->control));
  g_assert_null (s.tool->display);
  g_assert_true (s.tool->focus_display != closed);
  g_assert_null (gimp_display_get_image (closed));
  g_assert_null (gimp_display_get_shell (closed));
  g_assert_cmpuint (s.picked.size (), ==, before_close + 1);
  g_assert_cmpint (s.picked.back (), ==, GIMP_COLOR_PICK_STATE_END);
  g_assert_false (gimp_color_tool_is_enabled (GIMP_COLOR_TOOL (s.tool)));
  g_assert_null (GIMP_COLOR_TOOL (s.tool)->options);
  handle.enable_color_picker (GIMP_COLOR_PICK_TARGET_BACKGROUND);
  gimp_tool_control (s.tool, GIMP_TOOL_ACTION_HALT, closed);
  g_assert_cmpuint (s.picked.size (), ==, before_close + 1);
  g_object_unref (closed);
  gimp_tool_set_focus_display (s.tool, s.display);
  s.modifier (true);
  s.cursor (GIMP_CURSOR_MODIFIER_BACKGROUND);
  s.pick_press ();
  s.pick_release ();
  s.modifier (false);
  s.assert_unpainted (before);
}

static void colorpicker_pick_only_and_interruptions ()
{
  PickerScene s;
  auto handle = MypaintToolRef::retain (GIMP_MYPAINT_TOOL (s.tool));
  const auto before = s.pixels ();
  auto *context = GIMP_CONTEXT (s.options);
  handle.enable_color_picker (GIMP_COLOR_PICK_TARGET_NONE);
  s.coords.x = 16;
  s.modifier (true);
  s.cursor (GIMP_CURSOR_MODIFIER_NONE);
  s.pick_press ();
  s.coords.x = 64;
  s.pick_motion ();
  s.coords.x = 112;
  s.pick_release ();
  g_assert_cmpuint (s.picked.size (), ==, 3); // NONE still samples normally.
  for (auto target : s.picked_targets)
    g_assert_cmpint (target, ==, GIMP_COLOR_PICK_TARGET_NONE);
  assert_picker_color (gimp_context_get_foreground (context), "red");
  assert_picker_color (gimp_context_get_background (context), "rgb(1.0,0.0,1.0)");
  s.modifier (false);

  /* Palette is forwarded to the native cursor as well as the stored target;
   * no palette dock or persistent palette needs to be created by this test. */
  handle.enable_color_picker (GIMP_COLOR_PICK_TARGET_PALETTE);
  s.modifier (true);
  s.cursor (GIMP_CURSOR_MODIFIER_PLUS);
  s.modifier (false);

  handle.enable_color_picker (GIMP_COLOR_PICK_TARGET_BACKGROUND);
  s.modifier (true);
  gimp_tool_set_focus_display (s.tool, nullptr); // Modifier held when focus is lost.
  g_assert_false (gimp_color_tool_is_enabled (GIMP_COLOR_TOOL (s.tool)));
  g_assert_cmpint (s.tool->modifier_state, ==, 0);
  gimp_tool_set_focus_display (s.tool, s.display);
  s.modifier (true);
  s.coords.x = 16;
  s.pick_press ();
  const auto foreground = picker_color (gimp_context_get_foreground (context));
  const auto background = picker_color (gimp_context_get_background (context));
  handle.enable_color_picker (GIMP_COLOR_PICK_TARGET_FOREGROUND);
  g_assert_true (gimp_tool_control_is_active (s.tool->control));
  g_assert_true (gimp_color_tool_is_enabled (GIMP_COLOR_TOOL (s.tool)));
  assert_picker_color (gimp_context_get_foreground (context), foreground);
  assert_picker_color (gimp_context_get_background (context), background);
  s.assert_unpainted (before);
  handle.enable_color_picker (GIMP_COLOR_PICK_TARGET_BACKGROUND);
  gimp_tool_control (s.tool, GIMP_TOOL_ACTION_HALT, s.display);
  g_assert_false (gimp_tool_control_is_active (s.tool->control));
  g_assert_null (s.tool->display);
  s.modifier (false);
  s.modifier (true);
  s.pick_press ();
  s.pick_release ();
  s.modifier (false);
  g_assert_cmpint (GIMP_COLOR_TOOL (s.tool)->pick_target, ==,
                   GIMP_COLOR_PICK_TARGET_BACKGROUND);
  s.assert_unpainted (before);
  colorpicker_display_interruptions (s, before);

  // The same native tool still paints and creates exactly one undo afterwards.
  s.reset_colors ();
  s.coords.x = 55;
  s.coords.y = 80;
  s.motion (false);
  s.press ();
  s.coords.x = 75;
  s.motion (true, 75);
  s.release ();
  gimp_tool_control (s.tool, GIMP_TOOL_ACTION_COMMIT, s.display);
  const auto painted = s.pixels ();
  g_assert_true (painted != before);
  g_assert_cmpint (s.depth (), ==, 1);
  g_assert_true (gimp_image_undo (s.image));
  g_assert_true (s.pixels () == before);
  g_assert_true (gimp_image_redo (s.image));
  g_assert_true (s.pixels () == painted);
}

/* Opt-in raw-window capture aid, separate from the unconditional assertions.
 * Each stage is bounded; screenshots must show the actual integrated test
 * application, not a generated rendering of these fixture colors. */
static void colorpicker_visual_stage (PickerScene& scene, const gchar *name)
{
  gimp_drawable_update (scene.drawable, 0, 0, 128, 128);
  gimp_projection_finish_draw (gimp_image_get_projection (scene.image));
  gimp_projection_flush (gimp_image_get_projection (scene.image));
  gimp_display_flush_now (scene.display);
  gimp_test_run_mainloop_until_idle ();
  g_print ("COLORPICKER_VISUAL_STAGE=%s\n", name);
  std::fflush (stdout);
  const auto deadline = g_get_monotonic_time () + 8 * G_TIME_SPAN_SECOND;
  while (g_get_monotonic_time () < deadline)
    {
      g_main_context_iteration (nullptr, FALSE);
      g_usleep (10000);
    }
}

static GtkWidget *colorpicker_find_native_swatches (GtkWidget *widget)
{
  if (GIMP_IS_FG_BG_EDITOR (widget))
    return widget;
  if (! GTK_IS_CONTAINER (widget))
    return nullptr;
  auto *children = gtk_container_get_children (GTK_CONTAINER (widget));
  GtkWidget *result = nullptr;
  for (auto *child = children; child && ! result; child = child->next)
    result = colorpicker_find_native_swatches (GTK_WIDGET (child->data));
  g_list_free (children);
  return result;
}

static void colorpicker_visual_demo ()
{
  PickerScene s;
  auto *shell = gimp_display_get_shell (s.display);
  gboolean saved_hide_docks, saved_color_area;
  g_object_get (gimp->config, "hide-docks", &saved_hide_docks,
                "toolbox-color-area", &saved_color_area, nullptr);
  g_object_set (gimp->config, "hide-docks", FALSE,
                "toolbox-color-area", TRUE, nullptr);
  // Raise the existing native toolbox through the application's ordinary
  // window strategy. Its own FG/BG editor observes the real user context.
  auto *toolbox = gimp_window_strategy_show_dockable_dialog (
    GIMP_WINDOW_STRATEGY (gimp_get_window_strategy (gimp)), gimp,
    gimp_dialog_factory_get_singleton (), gimp_widget_get_monitor (GTK_WIDGET (shell)),
    "gimp-toolbox");
  g_assert_nonnull (toolbox);
  gtk_widget_show (toolbox);
  auto *toolbox_window = gtk_widget_get_toplevel (toolbox);
  g_assert_true (GTK_IS_WINDOW (toolbox_window));
  gtk_window_present (GTK_WINDOW (toolbox_window));
  auto *swatches = colorpicker_find_native_swatches (toolbox);
  g_assert_nonnull (swatches);
  // One idle iteration need not include GTK's next allocation/frame tick.
  const auto mapping_deadline = g_get_monotonic_time () + 2 * G_TIME_SPAN_SECOND;
  while (! gtk_widget_get_mapped (swatches) &&
         g_get_monotonic_time () < mapping_deadline)
    {
      g_main_context_iteration (nullptr, FALSE);
      g_usleep (10000);
    }
  if (! gtk_widget_get_mapped (swatches))
    for (auto *ancestor = swatches; ancestor; ancestor = gtk_widget_get_parent (ancestor))
      g_test_message ("native swatch ancestor %s: visible=%d mapped=%d child-visible=%d",
                       G_OBJECT_TYPE_NAME (ancestor), gtk_widget_get_visible (ancestor),
                       gtk_widget_get_mapped (ancestor), gtk_widget_get_child_visible (ancestor));
  g_assert_true (gtk_widget_get_mapped (swatches));
  auto *swatch_context = GIMP_FG_BG_EDITOR (swatches)->context;
  assert_picker_color (gimp_context_get_foreground (swatch_context), "red");
  assert_picker_color (gimp_context_get_background (swatch_context), "rgb(1.0,0.0,1.0)");
  gimp_display_shell_scale (shell, GIMP_ZOOM_TO, 3., GIMP_ZOOM_FOCUS_IMAGE_CENTER);
  gtk_widget_grab_focus (shell->canvas);
  const auto before = s.pixels ();
  colorpicker_visual_stage (s, "before");
  gimp_mypaint_tool_enable_color_picker (GIMP_MYPAINT_TOOL (s.tool),
                                        GIMP_COLOR_PICK_TARGET_BACKGROUND);
  g_assert_false (gimp_color_tool_is_enabled (GIMP_COLOR_TOOL (s.tool)));
  colorpicker_visual_stage (s, "configured-background");
  s.coords.x = 16;
  s.modifier (true);
  s.cursor (GIMP_CURSOR_MODIFIER_BACKGROUND);
  s.pick_press ();
  s.pick_release ();
  s.modifier (false);
  assert_picker_color (gimp_context_get_foreground (GIMP_CONTEXT (s.options)), "red");
  assert_picker_color (gimp_context_get_background (GIMP_CONTEXT (s.options)), "blue");
  assert_picker_color (gimp_context_get_foreground (swatch_context), "red");
  assert_picker_color (gimp_context_get_background (swatch_context), "blue");
  s.assert_unpainted (before);
  colorpicker_visual_stage (s, "after-background-pick");
  s.coords.x = 48;
  s.coords.y = 80;
  s.motion (false);
  s.press ();
  for (unsigned i = 0; i < 6; ++i)
    {
      s.coords.x += 6;
      s.motion (true, 75);
    }
  s.release ();
  gimp_tool_control (s.tool, GIMP_TOOL_ACTION_COMMIT, s.display);
  g_assert_true (s.pixels () != before);
  g_assert_cmpint (s.depth (), ==, 1);
  colorpicker_visual_stage (s, "after-paint");
  g_assert_true (gimp_image_undo (s.image));
  g_assert_true (s.pixels () == before);
  colorpicker_visual_stage (s, "after-undo");
  g_object_set (gimp->config, "hide-docks", saved_hide_docks,
                "toolbox-color-area", saved_color_area, nullptr);
}

#include "test-painter-layer-mask.inc"
#include "test-painter-layer-mask-visual.inc"
#include "test-painter-projection-notify.inc"

int main(int argc,char**argv)
{
  g_test_init(&argc,&argv,nullptr);if(!gtk_init_check(&argc,&argv))return GIMP_EXIT_TEST_SKIPPED;gimp_test_utils_setup_menus_path();gimp=gimp_init_for_gui_testing(TRUE);
  g_test_add_func("/painter-tool/projection/visible-update",ProjectionNotify::visible_update);
  if (g_getenv("GIMP_TEST_LAYER_MASK_VISUAL_DIR"))
    g_test_add_func("/painter-tool/layer-mask/visual-capture",layer_mask_visual_capture);
  g_test_add_func("/painter-tool/layer-mask/raw-apply-parity",layer_mask_raw_paint_apply_parity);
  g_test_add_func("/painter-tool/layer-mask/projection-flags",layer_mask_projection_flags);
  g_test_add_func("/painter-tool/layer-mask/tool-target-undo-cancel",layer_mask_tool_target_undo_cancel);
  g_test_add_func("/painter-tool/registration-gate",registration_and_gate);
  g_test_add_func("/painter-tool/standalone-lifecycle",standalone_lifecycle);
  g_test_add_func("/painter-tool/hover-default-pressure",hover_default_pressure);
  g_test_add_func("/painter-tool/stationary-equal-time-undo",pressure_and_undo);
  g_test_add_func("/painter-tool/cancel",cancel_rolls_back);
  g_test_add_func("/painter-tool/editor-context",context_editor_contract);
  g_test_add_func("/painter-tool/ruler-origin",ruler_saved_pressure);
  g_test_add_func("/painter-tool/halt-last-ref-motion",halt_and_last_ref_in_motion);
  g_test_add_func("/painter-tool/image-change",image_change_finishes_old);
  g_test_add_func("/painter-tool/save-pending",saving_finishes_pending);g_test_add_func("/painter-tool/save-disposal",saving_disposal);g_test_add_func("/painter-tool/external-dirty-cancels",external_dirty_still_cancels);
  g_test_add_func("/painter-tool/save-preflight",save_defers_during_sample);
  g_test_add_func("/painter-tool/outline-projection",outline_and_projection);
  g_test_add_func("/painter-tool/public-press-last-ref",public_press_drops_last_ref);
  g_test_add_func("/painter-tool/last-ref-without-halt",last_ref_without_halt_rolls_back);
  g_test_add_func("/painter-tool/gray-native-save-roundtrip",gray_tool_save_roundtrip);
  g_test_add_func("/painter-tool/pipe-release-save-roundtrip",pipe_release_save_roundtrip);
  g_test_add_func("/painter-tool/precision-native-save-roundtrip",precision_tool_save_roundtrip);
  g_test_add_func("/painter-tool/colorpicker/c-boundary",colorpicker_c_boundary);
  g_test_add_func("/painter-tool/colorpicker/named-handle",colorpicker_named_handle);
  g_test_add_func("/painter-tool/colorpicker/native-targets",colorpicker_native_targets);
  g_test_add_func("/painter-tool/colorpicker/pick-only-interruptions",colorpicker_pick_only_and_interruptions);
  if (g_getenv ("GIMP_TEST_COLORPICKER_VISUAL_DEMO"))
    g_test_add_func("/painter-tool/colorpicker/visual-demo",colorpicker_visual_demo);
  g_application_run(gimp->app,0,nullptr);int result=gimp_core_app_get_exit_status(GIMP_CORE_APP(gimp->app));g_application_quit(G_APPLICATION(gimp->app));g_clear_object(&gimp->app);return result;
}
