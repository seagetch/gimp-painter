int frozen_perspective_main(int argc,char **argv);
#define main frozen_perspective_main
#include "test-painter-perspective-events.c"
#undef main
#include "core/gimp-gui.h"
#include "display/gimppaintercanvasui.h"
#include "display/gimpdisplayshell-grab.h"
static GimpDisplayShell *old_shell;
static guint releases_seen,broken_seen,canvas_releases;
static gboolean naive;
static void dispatch_probe(GdkEvent *event,gpointer data)
{
 if(event->any.window==gtk_widget_get_window(old_shell->canvas)){
  if(event->type==GDK_BUTTON_RELEASE && event->button.time==33001){
   ++releases_seen;g_test_message("old queued release before broken-count=%u",broken_seen);

  }
  if(event->type==GDK_GRAB_BROKEN){++broken_seen;g_test_message("broken grab after release-count=%u",releases_seen);}
 }
 gtk_main_do_event(event);
}
static gboolean canvas_probe(GSignalInvocationHint *hint,guint n,const GValue *values,gpointer data)
{
 GtkWidget *widget=g_value_get_object(&values[0]);GdkEvent *event=g_value_get_boxed(&values[1]);
 if(widget==old_shell->canvas && event->type==GDK_BUTTON_RELEASE && event->button.time==33001){
  ++canvas_releases;
  if(naive && old_shell->grab_seat)gimp_display_shell_pointer_ungrab(old_shell,event);
 }
 return TRUE;
}
static void forward_dispatch(GdkEvent*event,gpointer data){gtk_main_do_event(event);}
static GdkEvent *button_event(GimpDisplayShell *target,GdkEventType type,guint32 time)
{
 GdkEvent*event=gdk_event_new(type);event->any.window=g_object_ref(gtk_widget_get_window(target->canvas));event->any.send_event=TRUE;
 gdk_event_set_device(event,device);gdk_event_set_source_device(event,device);
 event->button.time=time;event->button.button=1;event->button.x=100;event->button.y=100;event->button.state=type==GDK_BUTTON_RELEASE?GDK_BUTTON1_MASK:0;return event;
}
static void newer_window_grab(void)
{
 g_object_set(gimp->config,"single-window-mode",FALSE,NULL);gimp_test_run_mainloop_until_idle();
 GdkWindow *root=gdk_get_default_root_window();g_assert_cmpint(gdk_window_get_width(root),==,1080);g_assert_cmpint(gdk_window_get_height(root),==,1920);
 create_image();old_shell=shell;gimp_painter_canvas_ui_set_visible(old_shell,TRUE);g_assert_true(gimp_painter_canvas_ui_get_visible(old_shell));
 GimpImage*other_image=gimp_image_new(gimp,64,64,GIMP_RGB,GIMP_PRECISION_U8_NON_LINEAR);
 GimpDisplay*other_display=gimp_create_display(gimp,other_image,gimp_unit_pixel(),1.0,NULL);GimpDisplayShell*other=gimp_display_get_shell(other_display);g_assert_true(other!=old_shell);
 gimp_painter_canvas_ui_set_visible(other,TRUE);g_assert_true(gimp_painter_canvas_ui_get_visible(other));
 gimp_test_run_mainloop_until_idle();gimp_display_shell_present(old_shell);gimp_test_run_mainloop_until_idle();
 send_event(GDK_BUTTON_PRESS,0,80,80,0);g_assert_nonnull(old_shell->grab_seat);
 GdkSeat*seat=old_shell->grab_seat;GdkDisplay*gdkdisplay=gtk_widget_get_display(old_shell->canvas);g_assert_true(gdk_display_device_is_grabbed(gdkdisplay,device));
 gimp_display_set_image(old_shell->display,NULL);g_assert_nonnull(old_shell->grab_seat);
 gimp_display_shell_present(other);gimp_test_run_mainloop_until_idle();
 g_assert_true(gtk_widget_get_mapped(old_shell->canvas));g_assert_true(gtk_widget_get_mapped(other->canvas));
 guint event_signal=g_signal_lookup("event",GTK_TYPE_WIDGET);gulong hook=g_signal_add_emission_hook(event_signal,0,canvas_probe,NULL,NULL);
 releases_seen=broken_seen=canvas_releases=0;gdk_event_handler_set(dispatch_probe,NULL,NULL);
 GdkEvent*release_event=button_event(old_shell,GDK_BUTTON_RELEASE,33001);gdk_event_put(release_event);gdk_event_free(release_event);
 GdkEvent*press_event=button_event(other,GDK_BUTTON_PRESS,GDK_CURRENT_TIME);g_assert_true(gimp_display_shell_pointer_grab(other,press_event,GDK_BUTTON_RELEASE_MASK));gdk_event_free(press_event);
 g_assert_true(other->grab_seat==seat);g_assert_true(gdk_display_device_is_grabbed(gdkdisplay,device));
 gdk_display_sync(gdkdisplay);gimp_test_run_mainloop_until_idle();g_assert_cmpuint(releases_seen,==,1);g_assert_cmpuint(canvas_releases,==,1);
 g_assert_true(gdk_display_device_is_grabbed(gdkdisplay,device));g_assert_true(other->grab_seat==seat);
 g_signal_remove_emission_hook(event_signal,hook);gdk_event_handler_set(forward_dispatch,NULL,NULL);
 GdkEvent*cleanup=button_event(other,GDK_BUTTON_RELEASE,GDK_CURRENT_TIME);gimp_display_shell_pointer_ungrab(other,cleanup);gdk_event_free(cleanup);
 /* Baseline old bookkeeping is intentionally inspected, not used to ungrab
  * the replacement. Its empty shell is disposed after the real owner ends. */
 g_test_message("baseline stale old bookkeeping=%d broken events=%u",old_shell->grab_seat!=NULL,broken_seen);
 gimp_display_delete(old_shell->display);g_object_unref(image);g_object_unref(other_image);gimp_display_close(other_display);gimp_test_run_mainloop_until_idle();
}
int main(int argc,char**argv)
{
 g_test_init(&argc,&argv,NULL);if(!gtk_init_check(&argc,&argv))return GIMP_EXIT_TEST_SKIPPED;
 naive=g_getenv("GIMP_GRAB_NAIVE_MUTANT")!=NULL;gimp_test_utils_setup_menus_path();gimp=gimp_init_for_gui_testing(TRUE);
 g_test_add_func("/grab-gap/newer-window",newer_window_grab);
 g_application_run(gimp->app,0,NULL);gint result=gimp_core_app_get_exit_status(GIMP_CORE_APP(gimp->app));g_application_quit(G_APPLICATION(gimp->app));g_clear_object(&gimp->app);return result;
}
