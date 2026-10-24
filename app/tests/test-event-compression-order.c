int frozen_perspective_main(int argc,char **argv);
#define main frozen_perspective_main
#include "test-painter-perspective-events.c"
#undef main
#include "core/gimp-gui.h"
#include "display/gimppaintercanvasui.h"
static void create_visible_image(void)
{
 create_image();gimp_painter_canvas_ui_set_visible(shell,TRUE);g_assert_true(gimp_painter_canvas_ui_get_visible(shell));
 GdkWindow*root=gdk_get_default_root_window();g_assert_cmpint(gdk_window_get_width(root),==,1080);g_assert_cmpint(gdk_window_get_height(root),==,1920);
}
#define create_image create_visible_image

static guint seen[32], n_seen;
static void default_dispatch(GdkEvent *e,gpointer data){gtk_main_do_event(e);}
static void dispatch_event(GdkEvent *e,gpointer data)
{
 guint32 time=gdk_event_get_time(e);
 if(shell && e->any.window==gtk_widget_get_window(shell->canvas) && time>=10000&&time<10032){g_assert_cmpuint(n_seen,<,32);seen[n_seen++]=time-10000;}
 gtk_main_do_event(e);
}
static void enqueue(GdkEventType type,guint id,GdkDevice *dev,guint state)
{
 GdkEvent *e=gdk_event_new(type);gdouble x,y;
 gimp_display_shell_transform_xy_f(shell,90+id*4,90+id*3,&x,&y);
 e->any.window=g_object_ref(gtk_widget_get_window(shell->canvas));e->any.send_event=TRUE;
 gdk_event_set_device(e,dev);gdk_event_set_source_device(e,dev);
 if(type==GDK_MOTION_NOTIFY){e->motion.time=10000+id;e->motion.x=x;e->motion.y=y;e->motion.state=state;}
 else if(type==GDK_BUTTON_RELEASE){e->button.time=10000+id;e->button.x=x;e->button.y=y;e->button.state=state;e->button.button=1;}
 else if(type==GDK_KEY_PRESS){e->key.time=10000+id;e->key.keyval=GDK_KEY_Shift_L;e->key.state=state;}
 else {e->crossing.time=10000+id;e->crossing.x=x;e->crossing.y=y;e->crossing.state=state;e->crossing.mode=GDK_CROSSING_NORMAL;e->crossing.detail=GDK_NOTIFY_NONLINEAR;e->crossing.focus=TRUE;}
 /* gdk_event_put copies. This fixture owns and frees only its input copy. */
 gdk_event_put(e);gdk_event_free(e);
}
static void queued_terminals(void)
{
 GdkEventType boundaries[]={GDK_BUTTON_RELEASE,GDK_KEY_PRESS,GDK_LEAVE_NOTIFY};
 for(guint k=0;k<G_N_ELEMENTS(boundaries);k++){
  create_image();gimp_test_run_mainloop_until_idle();n_seen=0;
  gdk_event_handler_set(dispatch_event,NULL,NULL);
  send_event(GDK_BUTTON_PRESS,0,80,80,0);g_assert_cmpuint(presses,==,1);
  enqueue(GDK_MOTION_NOTIFY,1,device,GDK_BUTTON1_MASK);enqueue(GDK_MOTION_NOTIFY,2,device,GDK_BUTTON1_MASK);
  enqueue(boundaries[k],3,device,GDK_BUTTON1_MASK);
  enqueue(GDK_MOTION_NOTIFY,4,device,k?GDK_BUTTON1_MASK:0);
  if(k)enqueue(GDK_BUTTON_RELEASE,5,device,GDK_BUTTON1_MASK);
  gimp_test_run_mainloop_until_idle();
  g_assert_cmpuint(n_seen,==,k?5:4);for(guint i=0;i<n_seen;i++)g_assert_cmpuint(seen[i],==,i+1);
  g_assert_cmpuint(releases,==,1);g_assert_cmpuint(n_motion,>,0);g_assert_null(shell->grab_seat);
  if(k==0 && g_getenv("GIMP_EVENT_ORDER_SCREENSHOT")){
   GError*error=NULL;GdkWindow*root=gdk_get_default_root_window();gimp_test_run_mainloop_until_idle();GdkPixbuf*shot=gdk_pixbuf_get_from_window(root,0,0,1080,1920);g_assert_nonnull(shot);g_assert_true(gdk_pixbuf_save(shot,g_getenv("GIMP_EVENT_ORDER_SCREENSHOT"),"png",&error,NULL));g_assert_no_error(error);g_object_unref(shot);
  }
  gdk_event_handler_set(default_dispatch,NULL,NULL);close_image();
 }
}
static void exact_reset(void)
{
 create_image();GdkWindow*w=gtk_widget_get_window(shell->canvas);
 gimp_tool_control_set_motion_mode(active()->control,GIMP_MOTION_MODE_EXACT);
 send_event(GDK_BUTTON_PRESS,0,80,80,0);g_assert_false(gdk_window_get_event_compression(w));
 send_event(GDK_BUTTON_RELEASE,GDK_BUTTON1_MASK,90,90,0);g_assert_true(gdk_window_get_event_compression(w));
 send_event(GDK_BUTTON_PRESS,0,80,80,0);g_assert_false(gdk_window_get_event_compression(w));
 send_event(GDK_BUTTON_RELEASE,GDK_BUTTON1_MASK|GDK_BUTTON3_MASK,90,90,0);g_assert_true(gdk_window_get_event_compression(w));
 close_image();
}
static void exact_tool_switch(void)
{
 create_image();GdkWindow*w=gtk_widget_get_window(shell->canvas);
 gimp_tool_control_set_motion_mode(active()->control,GIMP_MOTION_MODE_EXACT);
 send_event(GDK_BUTTON_PRESS,0,80,80,0);g_assert_false(gdk_window_get_event_compression(w));
 gimp_context_set_tool(gimp_get_user_context(gimp),gimp_get_tool_info(gimp,"gimp-move-tool"));
 g_assert_true(gdk_window_get_event_compression(w));send_event(GDK_BUTTON_RELEASE,GDK_BUTTON1_MASK,90,90,0);g_assert_null(shell->grab_seat);close_image();
}
static void exact_image_switch(void)
{
 create_image();GdkWindow*w=gtk_widget_get_window(shell->canvas);
 GimpImage *other=gimp_image_new(gimp,256,256,GIMP_RGB,GIMP_PRECISION_U8_NON_LINEAR);
 gimp_tool_control_set_motion_mode(active()->control,GIMP_MOTION_MODE_EXACT);
 send_event(GDK_BUTTON_PRESS,0,80,80,0);g_assert_false(gdk_window_get_event_compression(w));
 gimp_display_set_image(shell->display,other);g_assert_true(gdk_window_get_event_compression(w));
 gimp_display_set_image(shell->display,image);g_object_unref(other);send_event(GDK_BUTTON_RELEASE,GDK_BUTTON1_MASK,90,90,0);g_assert_null(shell->grab_seat);close_image();
}
static void exact_guide_change(void)
{
 create_image();GdkWindow*w=gtk_widget_get_window(shell->canvas);
 gimp_tool_control_set_motion_mode(active()->control,GIMP_MOTION_MODE_EXACT);
 send_event(GDK_BUTTON_PRESS,0,80,80,0);g_assert_false(gdk_window_get_event_compression(w));
 guide(1);g_assert_false(gdk_window_get_event_compression(w));
 gimp_display_shell_set_perspective_snap(shell,FALSE);g_assert_false(gdk_window_get_event_compression(w));
 send_event(GDK_BUTTON_RELEASE,GDK_BUTTON1_MASK,90,90,0);g_assert_true(gdk_window_get_event_compression(w));close_image();
}
#ifdef LEASE_CANDIDATE
static void prior_false_and_idempotent(void)
{
 create_image();GdkWindow*w=gtk_widget_get_window(shell->canvas);
 gdk_window_set_event_compression(w,FALSE);
 gimp_display_shell_restore_motion_compression(shell);g_assert_false(gdk_window_get_event_compression(w));
 gimp_tool_control_set_motion_mode(active()->control,GIMP_MOTION_MODE_EXACT);
 send_event(GDK_BUTTON_PRESS,0,80,80,0);g_assert_true(shell->painter_compression_window==w);
 send_event(GDK_BUTTON_RELEASE,GDK_BUTTON1_MASK,90,90,0);g_assert_false(gdk_window_get_event_compression(w));g_assert_null(shell->painter_compression_window);
 gimp_display_shell_restore_motion_compression(shell);g_assert_false(gdk_window_get_event_compression(w));
 gdk_window_set_event_compression(w,TRUE);close_image();
}
static void aborted_begin(void)
{
 create_image();GdkWindow*w=gtk_widget_get_window(shell->canvas);
 gimp_tool_control_set_motion_mode(active()->control,GIMP_MOTION_MODE_EXACT);replace_on_press=TRUE;
 send_event(GDK_BUTTON_PRESS,0,80,80,0);g_assert_true(gdk_window_get_event_compression(w));g_assert_null(shell->painter_compression_window);
 send_event(GDK_BUTTON_RELEASE,GDK_BUTTON1_MASK,80,80,0);close_image();
}
static guint window_finalized;
static void window_gone(gpointer data,GObject*object){++window_finalized;}
static void recreated_window(void)
{
 create_image();GtkWidget*canvas=g_object_ref(shell->canvas);GdkWindow*old=g_object_ref(gtk_widget_get_window(canvas));
 gimp_tool_control_set_motion_mode(active()->control,GIMP_MOTION_MODE_EXACT);
 window_finalized=0;g_object_weak_ref(G_OBJECT(old),window_gone,NULL);
 send_event(GDK_BUTTON_PRESS,0,80,80,0);g_assert_true(shell->painter_compression_window==old);
 gtk_widget_unrealize(canvas);g_assert_true(gdk_window_is_destroyed(old));gtk_widget_realize(canvas);gtk_widget_map(canvas);g_assert_true(gtk_widget_get_mapped(canvas));
 GdkWindow*replacement=gtk_widget_get_window(canvas);g_assert_true(replacement!=old);gdk_window_set_event_compression(replacement,FALSE);
 gimp_display_shell_restore_motion_compression(shell);g_assert_null(shell->painter_compression_window);g_assert_false(gdk_window_get_event_compression(replacement));
 g_object_unref(old);gimp_test_run_mainloop_until_idle();g_assert_cmpuint(window_finalized,==,1);send_event(GDK_BUTTON_RELEASE,GDK_BUTTON1_MASK,90,90,0);g_assert_null(shell->grab_seat);g_assert_false(gdk_window_get_event_compression(replacement));gdk_window_set_event_compression(replacement,TRUE);close_image();g_object_unref(canvas);
}
static void halted_press(GimpTool *tool,const GimpCoords *coords,guint32 time,GdkModifierType state,GimpButtonPressType type,GimpDisplay *display)
{press(tool,coords,time,state,type,display);gimp_tool_control_halt(tool->control);}
static void halted_begin(void)
{
 create_image();GdkWindow*w=gtk_widget_get_window(shell->canvas);gimp_tool_control_set_motion_mode(active()->control,GIMP_MOTION_MODE_EXACT);
 GIMP_TOOL_GET_CLASS(active())->button_press=halted_press;send_event(GDK_BUTTON_PRESS,0,80,80,0);GIMP_TOOL_GET_CLASS(active())->button_press=press;
 g_assert_null(shell->painter_compression_window);g_assert_true(gdk_window_get_event_compression(w));send_event(GDK_BUTTON_RELEASE,GDK_BUTTON1_MASK,80,80,0);close_image();
}
static void nested_press(GimpTool *tool,const GimpCoords *coords,guint32 time,GdkModifierType state,GimpButtonPressType type,GimpDisplay *display)
{
 press(tool,coords,time,state,type,display);
 GimpToolClass *klass=GIMP_TOOL_GET_CLASS(tool);klass->button_press=press;
 gimp_context_set_tool(gimp_get_user_context(gimp),gimp_get_tool_info(gimp,"gimp-move-tool"));
 send_event(GDK_BUTTON_RELEASE,GDK_BUTTON1_MASK,80,80,0);
 gimp_context_set_tool(gimp_get_user_context(gimp),recording_info);
 gimp_tool_control_set_motion_mode(active()->control,GIMP_MOTION_MODE_EXACT);
 send_event(GDK_BUTTON_PRESS,0,100,100,0);
}
static void nested_new_stroke(void)
{
 create_image();GdkWindow*w=gtk_widget_get_window(shell->canvas);
 gimp_tool_control_set_motion_mode(active()->control,GIMP_MOTION_MODE_EXACT);
 shell->painter_compression_generation=G_MAXUINT64-1;
 GIMP_TOOL_GET_CLASS(active())->button_press=nested_press;
 send_event(GDK_BUTTON_PRESS,0,80,80,0);
 g_assert_cmpuint(presses,==,2);g_assert_true(shell->painter_compression_window==w);g_assert_false(gdk_window_get_event_compression(w));
 send_event(GDK_BUTTON_RELEASE,GDK_BUTTON1_MASK,100,100,0);g_assert_true(gdk_window_get_event_compression(w));close_image();
}
static gboolean reject_initialize(GimpTool *tool,GimpDisplay *display,GError **error){return FALSE;}
static void rejected_begin(void)
{
 create_image();GdkWindow*w=gtk_widget_get_window(shell->canvas);GimpToolClass*klass=GIMP_TOOL_GET_CLASS(active());
 gimp_tool_control_set_motion_mode(active()->control,GIMP_MOTION_MODE_EXACT);klass->initialize=reject_initialize;
 send_event(GDK_BUTTON_PRESS,0,80,80,0);klass->initialize=initialize;
 g_assert_cmpuint(presses,==,0);g_assert_null(shell->painter_compression_window);g_assert_true(gdk_window_get_event_compression(w));
 send_event(GDK_BUTTON_RELEASE,GDK_BUTTON1_MASK,80,80,0);close_image();
}
static void device_boundary(void)
{
 create_image();GdkSeat*seat=gdk_display_get_default_seat(gtk_widget_get_display(shell->canvas));GList*slaves=gdk_seat_get_slaves(seat,GDK_SEAT_CAPABILITY_POINTER);g_assert_nonnull(slaves);GdkDevice*other=g_object_ref(slaves->data);g_list_free(slaves);g_assert_true(other!=device);
 gimp_test_run_mainloop_until_idle();n_seen=0;gdk_event_handler_set(dispatch_event,NULL,NULL);
 send_event(GDK_BUTTON_PRESS,0,80,80,0);enqueue(GDK_MOTION_NOTIFY,1,device,GDK_BUTTON1_MASK);enqueue(GDK_MOTION_NOTIFY,2,other,GDK_BUTTON1_MASK);enqueue(GDK_MOTION_NOTIFY,3,device,GDK_BUTTON1_MASK|GDK_SHIFT_MASK);enqueue(GDK_BUTTON_RELEASE,4,device,GDK_BUTTON1_MASK);
 gimp_test_run_mainloop_until_idle();g_assert_cmpuint(n_seen,==,4);for(guint i=0;i<n_seen;i++)g_assert_cmpuint(seen[i],==,i+1);g_assert_cmpuint(releases,==,1);
 gdk_event_handler_set(default_dispatch,NULL,NULL);g_object_unref(other);close_image();
}
static void mode_change(void)
{
 create_image();GdkWindow*w=gtk_widget_get_window(shell->canvas);
 gimp_tool_control_set_motion_mode(active()->control,GIMP_MOTION_MODE_EXACT);
 send_event(GDK_BUTTON_PRESS,0,80,80,0);g_assert_false(gdk_window_get_event_compression(w));
 gimp_tool_control_set_motion_mode(active()->control,GIMP_MOTION_MODE_COMPRESS);
 send_event(GDK_BUTTON_RELEASE,GDK_BUTTON1_MASK,90,90,0);g_assert_true(gdk_window_get_event_compression(w));
 send_event(GDK_BUTTON_PRESS,0,80,80,0);g_assert_true(gdk_window_get_event_compression(w));g_assert_null(shell->painter_compression_window);
 send_event(GDK_BUTTON_RELEASE,GDK_BUTTON1_MASK,90,90,0);close_image();
}
static void close_active_image(void)
{
 create_image();GdkWindow*w=g_object_ref(gtk_widget_get_window(shell->canvas));
 gimp_tool_control_set_motion_mode(active()->control,GIMP_MOTION_MODE_EXACT);
 send_event(GDK_BUTTON_PRESS,0,80,80,0);g_assert_false(gdk_window_get_event_compression(w));
 GimpDisplay*display=shell->display;g_object_unref(image);gimp_display_close(display);gimp_test_run_mainloop_until_idle();
 /* Last-image close normally reuses its shell; a destroyed window has no
  * observable compression policy to restore, and the lease must release it. */
 if(!gdk_window_is_destroyed(w)){g_assert_true(gdk_window_get_event_compression(w));send_event(GDK_BUTTON_RELEASE,GDK_BUTTON1_MASK,90,90,0);g_assert_null(shell->grab_seat);}
 g_object_unref(w);
}
static void other_shell_and_dispose(void)
{
 create_image();GimpDisplayShell*first=g_object_ref(shell);GimpDisplay*first_display=shell->display;
 GimpImage*other_image=gimp_image_new(gimp,64,64,GIMP_RGB,GIMP_PRECISION_U8_NON_LINEAR);
 GimpDisplay*other_display=gimp_create_display(gimp,other_image,gimp_unit_pixel(),1.0,NULL);
 GimpDisplayShell*other=gimp_display_get_shell(other_display);g_assert_true(other!=first);gimp_test_run_mainloop_until_idle();
 gimp_display_shell_present(first);gimp_test_run_mainloop_until_idle();
 GdkWindow*w=g_object_ref(gtk_widget_get_window(first->canvas));GdkWindow*other_window=gtk_widget_get_window(other->canvas);g_assert_nonnull(other_window);
 gdk_window_set_event_compression(other_window,FALSE);
 gimp_tool_control_set_motion_mode(active()->control,GIMP_MOTION_MODE_EXACT);
 send_event(GDK_BUTTON_PRESS,0,80,80,0);g_assert_false(gdk_window_get_event_compression(w));g_assert_true(first->painter_compression_window==w);
 gimp_display_shell_restore_motion_compression(other);g_assert_false(gdk_window_get_event_compression(w));g_assert_false(gdk_window_get_event_compression(other_window));
 gimp_display_delete(first_display);g_assert_null(first->painter_compression_window);g_assert_true(gdk_window_is_destroyed(w));g_assert_false(gdk_window_get_event_compression(other_window));
 g_object_unref(w);g_object_unref(first);g_object_unref(image);g_object_unref(other_image);gimp_display_close(other_display);gimp_test_run_mainloop_until_idle();
}
#endif
int main(int argc,char**argv)
{
 g_test_init(&argc,&argv,NULL);if(!gtk_init_check(&argc,&argv))return GIMP_EXIT_TEST_SKIPPED;
 gimp_test_utils_setup_menus_path();gimp=gimp_init_for_gui_testing(TRUE);
 g_test_add_func("/event-order/queued-terminals",queued_terminals);g_test_add_func("/event-order/exact-reset",exact_reset);g_test_add_func("/event-order/exact-tool-switch",exact_tool_switch);g_test_add_func("/event-order/exact-image-switch",exact_image_switch);g_test_add_func("/event-order/exact-guide-change",exact_guide_change);
#ifdef LEASE_CANDIDATE
 g_test_add_func("/event-order/prior-false-idempotent",prior_false_and_idempotent);g_test_add_func("/event-order/aborted-begin",aborted_begin);g_test_add_func("/event-order/recreated-window",recreated_window);
 g_test_add_func("/event-order/nested-new-stroke",nested_new_stroke);g_test_add_func("/event-order/halted-begin",halted_begin);g_test_add_func("/event-order/rejected-begin",rejected_begin);g_test_add_func("/event-order/device-boundary",device_boundary);
 g_test_add_func("/event-order/mode-change",mode_change);g_test_add_func("/event-order/close-active-image",close_active_image);
 g_test_add_func("/event-order/other-shell-dispose",other_shell_and_dispose);
#endif
 g_application_run(gimp->app,0,NULL);gint r=gimp_core_app_get_exit_status(GIMP_CORE_APP(gimp->app));g_application_quit(G_APPLICATION(gimp->app));g_clear_object(&gimp->app);return r;
}
