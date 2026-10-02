/* SPDX-License-Identifier: GPL-3.0-or-later
 * Real canvas-event and shared history-sample routes, with a recording GimpTool.
 * Synthetic devices do not claim physical tablet or backend history coverage.
 */
#include "config.h"
#include <math.h>
#include <gegl.h>
#include <gtk/gtk.h>
#include "libgimpbase/gimpbase.h"
#include "libgimpwidgets/gimpwidgets.h"
#include "core/core-types.h"
#include "display/display-types.h"
#include "tools/tools-types.h"
#include "config/gimpdisplayconfig.h"
#include "core/gimp.h"
#include "core/gimpcontainer.h"
#include "core/gimpcontext.h"
#include "core/gimpimage.h"
#include "core/gimpimage-perspective-guide.h"
#include "core/gimptoolinfo.h"
#include "core/gimptooloptions.h"
#include "display/gimpdisplay.h"
#include "display/gimpdisplayshell.h"
#include "display/gimpdisplayshell-tool-events.h"
#include "display/gimpdisplayshell-transform.h"
#include "display/gimpdisplayshell-rotate.h"
#include "display/gimpdisplayshell-scale.h"
#include "display/gimpmotionbuffer.h"
#include "display/gimpstatusbar.h"
#include "tools/gimptool.h"
#include "tools/gimptoolcontrol.h"
#include "tools/gimptooloptions-gui.h"
#include "widgets/gimpwidgets-utils.h"
#include "tools/tool_manager.h"
#include "gimpcoreapp.h"
#include "gimp-app-test-utils.h"
#include "tests.h"
typedef struct {GimpTool parent;} RecordingTool;
typedef struct {GimpToolClass parent;} RecordingToolClass;
GType recording_tool_get_type(void);
G_DEFINE_TYPE(RecordingTool,recording_tool,GIMP_TYPE_TOOL)
static Gimp *gimp;
static GimpDisplayShell *shell;
static GimpImage *image;
static GdkDevice *device;
static GimpToolInfo *recording_info;
static GimpCoords pressed,released,motions[128];
static guint presses,releases,n_motion,n_hover;
static GimpButtonReleaseType release_kind;
static guint32 stamp;
static gboolean checked_new_shell_default;
static gboolean replace_on_press;
static GimpImage *replace_on_motion;
static gboolean initialize(GimpTool *tool,GimpDisplay *display,GError **error){tool->display=display;return TRUE;}
static void press(GimpTool *tool,const GimpCoords *coords,guint32 time,GdkModifierType state,GimpButtonPressType type,GimpDisplay *display)
{++presses;pressed=*coords;tool->display=display;gimp_tool_control_activate(tool->control);if(replace_on_press){replace_on_press=FALSE;gimp_context_set_tool(gimp_get_user_context(gimp),gimp_get_tool_info(gimp,"gimp-move-tool"));}}
static void release(GimpTool *tool,const GimpCoords *coords,guint32 time,GdkModifierType state,GimpButtonReleaseType type,GimpDisplay *display)
{++releases;released=*coords;release_kind=type;gimp_tool_control_halt(tool->control);}
static void motion(GimpTool *tool,const GimpCoords *coords,guint32 time,GdkModifierType state,GimpDisplay *display)
{g_assert_cmpuint(n_motion,<,G_N_ELEMENTS(motions));motions[n_motion++]=*coords;if(replace_on_motion){GimpImage *other=replace_on_motion;replace_on_motion=NULL;gimp_display_set_image(display,other);}}
static void hover(GimpTool *tool,const GimpCoords *coords,GdkModifierType state,gboolean proximity,GimpDisplay *display){++n_hover;}
static void recording_tool_class_init(RecordingToolClass *klass)
{GimpToolClass *tool=GIMP_TOOL_CLASS(klass);tool->initialize=initialize;tool->button_press=press;tool->button_release=release;tool->motion=motion;tool->oper_update=hover;}
static void recording_tool_init(RecordingTool *recording)
{GimpTool *tool=GIMP_TOOL(recording);gimp_tool_control_set_handle_empty_image(tool->control,TRUE);gimp_tool_control_set_motion_mode(tool->control,GIMP_MOTION_MODE_COMPRESS);}
static void counters(void){presses=releases=n_motion=n_hover=0;}
static void send_event(GdkEventType type,guint state,gdouble x,gdouble y,guint key)
{
  GdkEvent *event=gdk_event_new(type);gdouble dx,dy;
  gimp_display_shell_transform_xy_f(shell,x,y,&dx,&dy);
  event->any.window=g_object_ref(gtk_widget_get_window(shell->canvas));event->any.send_event=TRUE;
  gdk_event_set_device(event,device);gdk_event_set_source_device(event,device);
  stamp+=10;
  if(type==GDK_BUTTON_PRESS || type==GDK_BUTTON_RELEASE){event->button.button=1;event->button.state=state;event->button.x=dx;event->button.y=dy;event->button.time=GDK_CURRENT_TIME;}
  else if(type==GDK_MOTION_NOTIFY){event->motion.state=state;event->motion.x=dx;event->motion.y=dy;event->motion.time=stamp;}
  else {event->key.keyval=key;event->key.state=state;event->key.time=stamp;}
  gimp_display_shell_canvas_tool_events(shell->canvas,event,shell);gdk_event_free(event);
}
static GimpTool *active(void){return tool_manager_get_active(gimp);}
static void create_image(void)
{
  gimp_set_focused_once(gimp);gimp_test_utils_create_image(gimp,256,256);gimp_test_run_mainloop_until_idle();
  shell=gimp_display_get_shell(gimp_get_display_iter(gimp)->data);image=gimp_display_get_image(shell->display);
  device=gdk_seat_get_pointer(gdk_display_get_default_seat(gtk_widget_get_display(shell->canvas)));
  if(!checked_new_shell_default){g_assert_false(shell->snap_perspective);checked_new_shell_default=TRUE;}
  /* The GUI reuses its empty shell between images; isolate each test setting. */
  gimp_display_shell_set_perspective_snap(shell,FALSE);
  shell->display->config->use_event_history=FALSE;gtk_widget_grab_focus(shell->canvas);
  if(!recording_info){recording_info=gimp_tool_info_new(gimp,recording_tool_get_type(),GIMP_TYPE_TOOL_OPTIONS,0,"painter-recording-test","Record","Record",NULL,NULL,NULL,"gimp-tool-paintbrush","gimp-paintbrush",GIMP_ICON_TOOL_PAINTBRUSH);gimp_tools_set_tool_options_gui_func(recording_info->tool_options,gimp_tool_options_gui);gimp_container_add(gimp->tool_info_list,GIMP_OBJECT(recording_info));gimp_container_add(gimp->tool_item_list,GIMP_OBJECT(recording_info));}
  gimp_context_set_tool(gimp_get_user_context(gimp),recording_info);
  gimp_display_shell_scale(shell,GIMP_ZOOM_TO,1,GIMP_ZOOM_FOCUS_IMAGE_CENTER);
  g_assert_false(active()->want_full_motion_tracking);g_assert_false(active()->disable_lazy_snap);counters();stamp=100;
}
static void close_image(void)
{
  GimpDisplay *display=shell->display;
  gimp_context_set_tool(gimp_get_user_context(gimp),gimp_get_tool_info(gimp,"gimp-paintbrush-tool"));
  g_object_unref(image);gimp_display_close(display);gimp_test_run_mainloop_until_idle();
}
static void guide(gint count)
{
  GimpPerspectiveGuide *model=gimp_perspective_guide_new(0);
  const gdouble points[][2]={{1000,1000},{-1000,1000},{128,-1000}};
  for(gint i=0;i<count;++i)gimp_perspective_guide_add_vanish_points(model,points[i][0],points[i][1]);
  gimp_image_set_perspective_guide(image,model);g_object_unref(model);
}
static void assert_reset(void)
{g_assert_false(shell->perspective_pending);g_assert_false(shell->perspective_locked);g_assert_null(shell->perspective_tool);g_assert_null(shell->perspective_image);g_assert_null(shell->perspective_model);}
static void short_threshold_release(void)
{
  create_image();guide(1);gimp_display_shell_set_perspective_snap(shell,TRUE);
  send_event(GDK_BUTTON_PRESS,0,80,80,0);g_assert_true(shell->perspective_pending);g_assert_cmpuint(presses,==,0);
  send_event(GDK_MOTION_NOTIFY,GDK_BUTTON1_MASK,110,80,0);g_assert_cmpuint(presses,==,0);
  send_event(GDK_BUTTON_RELEASE,GDK_BUTTON1_MASK,110,80,0);g_assert_cmpuint(presses,==,0);g_assert_cmpuint(releases,==,0);assert_reset();g_assert_null(shell->grab_seat);
  send_event(GDK_BUTTON_PRESS,0,80,80,0);
  send_event(GDK_MOTION_NOTIFY,GDK_BUTTON1_MASK,112,80,0);g_assert_cmpuint(presses,==,1);g_assert_cmpuint(n_motion,==,0);g_assert_true(shell->perspective_locked);
  g_assert_cmpfloat_with_epsilon(pressed.x,80,1e-8);g_assert_cmpfloat_with_epsilon(pressed.y,80,1e-8);
  send_event(GDK_MOTION_NOTIFY,GDK_BUTTON1_MASK,130,95,0);g_assert_cmpuint(n_motion,>,0);g_assert_cmpfloat_with_epsilon(motions[n_motion-1].y,80,1e-8);
  send_event(GDK_BUTTON_RELEASE,GDK_BUTTON1_MASK,140,100,0);g_assert_cmpuint(releases,==,1);g_assert_cmpfloat_with_epsilon(released.y,80,1e-8);assert_reset();g_assert_null(shell->grab_seat);
  close_image();
}
static void no_guide_clicks_and_toggle(void)
{
  create_image();g_assert_false(shell->snap_perspective);g_assert_false(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(GIMP_STATUSBAR(shell->statusbar)->perspective_snap_toggle)));
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(GIMP_STATUSBAR(shell->statusbar)->perspective_snap_toggle),TRUE);g_assert_true(shell->snap_perspective);
  for(gint i=0;i<2;++i){if(i)guide(0);send_event(GDK_BUTTON_PRESS,0,80,80,0);g_assert_false(shell->perspective_pending);send_event(GDK_BUTTON_RELEASE,GDK_BUTTON1_MASK,80,80,0);}
  g_assert_cmpuint(presses,==,2);g_assert_cmpuint(releases,==,2);
  guide(1);gimp_display_shell_set_perspective_snap(shell,FALSE);send_event(GDK_BUTTON_PRESS,0,80,80,0);g_assert_false(shell->perspective_pending);send_event(GDK_BUTTON_RELEASE,GDK_BUTTON1_MASK,80,80,0);g_assert_cmpuint(presses,==,3);
  close_image();
}
static void cancel_and_tool_switch(void)
{
  create_image();guide(1);gimp_display_shell_set_perspective_snap(shell,TRUE);
  send_event(GDK_BUTTON_PRESS,0,80,80,0);send_event(GDK_KEY_PRESS,GDK_BUTTON1_MASK,80,80,GDK_KEY_Escape);assert_reset();send_event(GDK_MOTION_NOTIFY,GDK_BUTTON1_MASK,150,80,0);send_event(GDK_BUTTON_RELEASE,GDK_BUTTON1_MASK,150,80,0);g_assert_cmpuint(presses,==,0);
  send_event(GDK_BUTTON_PRESS,0,80,80,0);g_assert_true(shell->perspective_pending);
  gimp_context_set_tool(gimp_get_user_context(gimp),gimp_get_tool_info(gimp,"gimp-move-tool"));assert_reset();send_event(GDK_BUTTON_RELEASE,GDK_BUTTON1_MASK,80,80,0);g_assert_null(shell->grab_seat);
  gimp_context_set_tool(gimp_get_user_context(gimp),recording_info);
  send_event(GDK_BUTTON_PRESS,0,80,80,0);send_event(GDK_MOTION_NOTIFY,GDK_BUTTON1_MASK,120,80,0);send_event(GDK_BUTTON_RELEASE,GDK_BUTTON1_MASK|GDK_BUTTON3_MASK,120,80,0);g_assert_cmpuint(releases,==,1);g_assert_cmpint(release_kind,==,GIMP_BUTTON_RELEASE_CANCEL);assert_reset();close_image();
}
static void guide_and_image_switch(void)
{
  GimpImage *other;
  create_image();guide(1);gimp_display_shell_set_perspective_snap(shell,TRUE);
  send_event(GDK_BUTTON_PRESS,0,80,80,0);gimp_image_set_perspective_guide(image,NULL);send_event(GDK_MOTION_NOTIFY,GDK_BUTTON1_MASK,150,80,0);assert_reset();send_event(GDK_BUTTON_RELEASE,GDK_BUTTON1_MASK,150,80,0);g_assert_cmpuint(presses,==,0);
  guide(1);send_event(GDK_BUTTON_PRESS,0,80,80,0);other=gimp_image_new(gimp,256,256,GIMP_RGB,GIMP_PRECISION_U8_NON_LINEAR);gimp_display_set_image(shell->display,other);assert_reset();send_event(GDK_BUTTON_RELEASE,GDK_BUTTON1_MASK,80,80,0);g_assert_null(shell->grab_seat);gimp_display_set_image(shell->display,image);g_object_unref(other);close_image();
}
static void history_origin_and_full_tracking(void)
{
  GimpCoords coords={0};GimpCoords saved;
  create_image();guide(1);gimp_display_shell_set_perspective_snap(shell,TRUE);
  send_event(GDK_BUTTON_PRESS,0,80,80,0);
  /* The native event adapter above captures the origin. Supply the device axes
   * unavailable on the synthetic mouse, then exercise the exact history route. */
  shell->perspective_origin.pressure=.71;shell->perspective_origin.xtilt=.23;shell->perspective_origin.ytilt=-.43;shell->perspective_origin.wheel=.19;shell->perspective_origin.distance=.4;shell->perspective_origin.rotation=.6;shell->perspective_origin.slider=.7;
  saved=shell->perspective_origin;coords=saved;coords.x=120;coords.y=80;
  g_assert_false(gimp_display_shell_perspective_motion(shell,&coords,++stamp,GDK_BUTTON1_MASK,TRUE));g_assert_cmpuint(presses,==,1);
  g_assert_cmpfloat(pressed.pressure,==,saved.pressure);g_assert_cmpfloat(pressed.xtilt,==,saved.xtilt);g_assert_cmpfloat(pressed.ytilt,==,saved.ytilt);g_assert_cmpfloat(pressed.wheel,==,saved.wheel);g_assert_cmpfloat(pressed.distance,==,saved.distance);g_assert_cmpfloat(pressed.rotation,==,saved.rotation);g_assert_cmpfloat(pressed.slider,==,saved.slider);
  coords.x=140;coords.y=100;gimp_display_shell_perspective_motion(shell,&coords,++stamp,GDK_BUTTON1_MASK,TRUE);gimp_motion_buffer_end_stroke(shell->motion_buffer);g_assert_cmpfloat_with_epsilon(motions[n_motion-1].y,80,1e-8);
  send_event(GDK_BUTTON_RELEASE,GDK_BUTTON1_MASK,140,100,0);assert_reset();counters();active()->want_full_motion_tracking=TRUE;
  coords.x=50;coords.y=33;coords.pressure=.2;gimp_display_shell_perspective_motion(shell,&coords,500,0,FALSE);
  coords.pressure=.6;g_assert_true(gimp_display_shell_perspective_motion(shell,&coords,500,0,FALSE));g_assert_cmpuint(n_motion,==,2);g_assert_cmpfloat(motions[1].pressure,==,.6);g_assert_cmpfloat(motions[1].y,==,33);g_assert_false(gimp_tool_control_is_active(active()->control));
  /* Full-tracking hover reaches motion through the real GDK canvas adapter. */
  stamp=500;send_event(GDK_MOTION_NOTIFY,0,51,35,0);g_assert_cmpuint(n_motion,==,3);g_assert_cmpfloat_with_epsilon(motions[2].y,35,1e-8);assert_reset();close_image();
}
static void replacement_during_delivery(void)
{
  GimpImage *other;GimpCoords coords={0};
  create_image();guide(1);gimp_display_shell_set_perspective_snap(shell,TRUE);replace_on_press=TRUE;
  send_event(GDK_BUTTON_PRESS,0,80,80,0);send_event(GDK_MOTION_NOTIFY,GDK_BUTTON1_MASK,120,80,0);g_assert_cmpuint(presses,==,1);assert_reset();send_event(GDK_BUTTON_RELEASE,GDK_BUTTON1_MASK,120,80,0);close_image();
  create_image();active()->want_full_motion_tracking=TRUE;send_event(GDK_BUTTON_PRESS,0,80,80,0);
  other=gimp_image_new(gimp,256,256,GIMP_RGB,GIMP_PRECISION_U8_NON_LINEAR);replace_on_motion=other;
  coords.xscale=coords.yscale=1;coords.pressure=.5;coords.x=100;coords.y=120;
  g_assert_true(gimp_motion_buffer_motion_event(shell->motion_buffer,&coords,300,FALSE));coords.x=130;g_assert_true(gimp_motion_buffer_motion_event(shell->motion_buffer,&coords,301,FALSE));
  gimp_motion_buffer_request_stroke(shell->motion_buffer,GDK_BUTTON1_MASK,301);
  g_assert_cmpuint(n_motion,==,1);g_assert_cmpuint(shell->motion_buffer->event_queue->len,==,0);assert_reset();
  send_event(GDK_BUTTON_RELEASE,GDK_BUTTON1_MASK,130,120,0);gimp_display_set_image(shell->display,image);g_object_unref(other);close_image();
}
static void prior_hover_does_not_bend_stroke(void)
{
  create_image();guide(1);gimp_display_shell_set_perspective_snap(shell,TRUE);
  gimp_tool_control_set_motion_mode(active()->control,GIMP_MOTION_MODE_EXACT);
  send_event(GDK_MOTION_NOTIFY,0,20,30,0);send_event(GDK_MOTION_NOTIFY,0,40,70,0);send_event(GDK_MOTION_NOTIFY,0,60,90,0);send_event(GDK_MOTION_NOTIFY,0,80,80,0);counters();
  send_event(GDK_BUTTON_PRESS,0,80,80,0);send_event(GDK_MOTION_NOTIFY,GDK_BUTTON1_MASK,112,80,0);
  send_event(GDK_MOTION_NOTIFY,GDK_BUTTON1_MASK,150,95,0);send_event(GDK_MOTION_NOTIFY,GDK_BUTTON1_MASK,200,105,0);send_event(GDK_MOTION_NOTIFY,GDK_BUTTON1_MASK,240,85,0);
  gimp_motion_buffer_end_stroke(shell->motion_buffer);g_assert_cmpuint(n_motion,>,0);
  for(guint i=0;i<n_motion;++i)g_assert_cmpfloat_with_epsilon(motions[i].y,80,1e-8);
  send_event(GDK_BUTTON_RELEASE,GDK_BUTTON1_MASK,240,85,0);close_image();
}
static void editing_tool_bypass(void)
{
  gdouble x,y;
  create_image();guide(1);gimp_display_shell_set_perspective_snap(shell,TRUE);
  gimp_context_set_tool(gimp_get_user_context(gimp),gimp_get_tool_info(gimp,"gimp-perspective-guide-tool"));
  g_assert_true(active()->disable_lazy_snap);
  send_event(GDK_BUTTON_PRESS,GDK_SHIFT_MASK,170,180,0);g_assert_false(shell->perspective_pending);
  send_event(GDK_BUTTON_RELEASE,GDK_BUTTON1_MASK|GDK_SHIFT_MASK,170,180,0);
  g_assert_cmpint(gimp_perspective_guide_get_vanish_point_length(gimp_image_get_perspective_guide(image)),==,2);
  g_assert_true(gimp_perspective_guide_get_vanish_points(gimp_image_get_perspective_guide(image),1,&x,&y));g_assert_cmpfloat_with_epsilon(x,170,1e-8);g_assert_cmpfloat_with_epsilon(y,180,1e-8);
  close_image();
}
static void three_point_transforms(void)
{
  const gdouble scales[]={.5,1,2};
  create_image();guide(3);gimp_display_shell_set_perspective_snap(shell,TRUE);
  for(guint s=0;s<G_N_ELEMENTS(scales);++s)for(guint flip=0;flip<4;++flip)
    {
      gdouble angle,ex=150,ey=170;
      gimp_display_shell_scale(shell,GIMP_ZOOM_TO,scales[s],GIMP_ZOOM_FOCUS_IMAGE_CENTER);gimp_display_shell_rotate_to(shell,37);gimp_display_shell_flip(shell,flip&1,flip&2);counters();
      send_event(GDK_BUTTON_PRESS,0,128,128,0);send_event(GDK_MOTION_NOTIFY,GDK_BUTTON1_MASK,128+40/scales[s],128-10/scales[s],0);
      g_assert_cmpuint(presses,==,1);g_assert_true(gimp_perspective_guide_snap_angle(gimp_image_get_perspective_guide(image),128,128,128+40/scales[s],128-10/scales[s],shell->scale_x,shell->scale_y,&angle));
      gimp_perspective_guide_constrain(angle,128,128,&ex,&ey);send_event(GDK_MOTION_NOTIFY,GDK_BUTTON1_MASK,150,170,0);gimp_motion_buffer_end_stroke(shell->motion_buffer);g_assert_cmpuint(n_motion,>,0);
      g_assert_cmpfloat_with_epsilon(motions[n_motion-1].x,ex,1e-6);g_assert_cmpfloat_with_epsilon(motions[n_motion-1].y,ey,1e-6);send_event(GDK_BUTTON_RELEASE,GDK_BUTTON1_MASK,150,170,0);assert_reset();
    }
  close_image();
}
int main(int argc,char **argv)
{
  gint result;g_test_init(&argc,&argv,NULL);if(!gtk_init_check(&argc,&argv))return GIMP_EXIT_TEST_SKIPPED;
  gimp_test_utils_setup_menus_path();gimp=gimp_init_for_gui_testing(TRUE);
#define ADD(name) g_test_add_func("/perspective-events/" #name,name)
  ADD(short_threshold_release);ADD(no_guide_clicks_and_toggle);ADD(cancel_and_tool_switch);ADD(guide_and_image_switch);ADD(history_origin_and_full_tracking);ADD(replacement_during_delivery);ADD(prior_hover_does_not_bend_stroke);ADD(editing_tool_bypass);ADD(three_point_transforms);
  g_application_run(gimp->app,0,NULL);result=gimp_core_app_get_exit_status(GIMP_CORE_APP(gimp->app));g_application_quit(G_APPLICATION(gimp->app));g_clear_object(&gimp->app);return result;
}
