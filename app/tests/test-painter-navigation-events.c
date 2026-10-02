/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <math.h>
#include <gtk/gtk.h>
#include <gegl.h>
#include "libgimpbase/gimpbase.h"
#include "libgimpwidgets/gimpwidgets.h"
#include "core/core-types.h"
#include "display/display-types.h"
#include "core/gimp.h"
#include "core/gimpimage.h"
#include "config/gimpdisplayconfig.h"
#include "display/gimpdisplay.h"
#include "display/gimpdisplayshell.h"
#include "display/gimpdisplayshell-tool-events.h"
#include "display/gimpdisplayshell-rotate.h"
#include "display/gimppainternavigation.h"
#include "display/gimpmodifiersmanager.h"
#include "gimpcoreapp.h"
#include "gimp-app-test-utils.h"
#include "tests.h"
static Gimp *gimp;
static GimpDisplayShell *shell;
static GdkDevice *device;
static GimpModifiersManager *manager;
static void send_event (GdkEventType type,guint button,guint state,gint x,gint y,guint key)
{
  GdkEvent *event=gdk_event_new(type);
  GdkWindow *window=gtk_widget_get_window(shell->canvas);
  gdk_event_set_device(event,device);
  gdk_event_set_source_device(event,device);
  event->any.window=g_object_ref(window);event->any.send_event=TRUE;
  if(type==GDK_BUTTON_PRESS || type==GDK_BUTTON_RELEASE)
    {event->button.button=button;event->button.state=state;event->button.x=x;event->button.y=y;event->button.time=GDK_CURRENT_TIME;}
  else if(type==GDK_MOTION_NOTIFY)
    {event->motion.state=state;event->motion.x=x;event->motion.y=y;event->motion.time=GDK_CURRENT_TIME;}
  else
    {event->key.keyval=key;event->key.state=state;event->key.time=GDK_CURRENT_TIME;}
  gimp_display_shell_canvas_tool_events(shell->canvas,event,shell);
  gdk_event_free(event);
}
static void create_image(void)
{
  /* The production GUI defers device creation until its first focus event.
   * Synthetic test events must initialize that same normal lifecycle first. */
  gimp_set_focused_once(gimp);
  gimp_test_utils_create_image(gimp,256,256);gimp_test_run_mainloop_until_idle();
  g_assert_cmpuint(g_list_length(gimp_get_display_iter(gimp)),==,1);
  shell=gimp_display_get_shell(gimp_get_display_iter(gimp)->data);
  device=gdk_seat_get_pointer(gdk_display_get_default_seat(gtk_widget_get_display(shell->canvas)));
  manager=GIMP_MODIFIERS_MANAGER(shell->display->config->modifiers_manager);
  gimp_modifiers_manager_clear(manager);
  gtk_widget_grab_focus(shell->canvas);
}
static void close_image(void)
{
  GimpImage *image=gimp_display_get_image(shell->display);
  GimpDisplay *display=shell->display;
  g_object_unref(image);gimp_display_close(display);gimp_test_run_mainloop_until_idle();
}
static void rotate_defaults(void)
{
  create_image();
  for(guint mirror=0;mirror<2;++mirror)
    {
      gint cx=shell->disp_width/2,cy=shell->disp_height/2;
      gdouble anchor,expected;
      gimp_display_shell_flip(shell,mirror,FALSE);
      gimp_display_shell_rotate_to(shell,42.5);
      anchor=gimp_painter_navigation_begin(shell->disp_width,shell->disp_height,cx+80,cy,42.5,mirror,FALSE);
      send_event(GDK_BUTTON_PRESS,2,GDK_SHIFT_MASK|GDK_CONTROL_MASK|GDK_MOD1_MASK,cx+80,cy,0);
      g_assert_cmpuint(shell->mod_action_button,==,2);g_assert_true(shell->painter_navigation_inherited);
      send_event(GDK_MOTION_NOTIFY,0,GDK_BUTTON2_MASK|GDK_SHIFT_MASK|GDK_CONTROL_MASK|GDK_MOD1_MASK,cx,cy+80,0);
      expected=gimp_painter_navigation_rotate(shell->disp_width,shell->disp_height,cx,cy+80,anchor,mirror,FALSE,GDK_CONTROL_MASK);
      g_assert_cmpfloat_with_epsilon(shell->rotate_angle,expected,1e-8);
      send_event(GDK_KEY_RELEASE,0,GDK_BUTTON2_MASK|GDK_CONTROL_MASK|GDK_MOD1_MASK,0,0,GDK_KEY_Control_L);
      g_assert_cmpuint(shell->mod_action_button,==,2);
      send_event(GDK_MOTION_NOTIFY,0,GDK_BUTTON2_MASK|GDK_MOD1_MASK,cx,cy+80,0);
      expected=gimp_painter_navigation_rotate(shell->disp_width,shell->disp_height,cx,cy+80,anchor,mirror,FALSE,0);
      g_assert_cmpfloat_with_epsilon(shell->rotate_angle,expected,1e-8);
      send_event(GDK_BUTTON_RELEASE,2,GDK_BUTTON2_MASK,cx,cy+80,0);
      g_assert_cmpint(shell->mod_action,==,GIMP_MODIFIER_ACTION_NONE);
      g_assert_cmpuint(shell->mod_action_button,==,0);g_assert_null(shell->grab_seat);
    }
  close_image();
}
static void arbitrary_button_constraints(void)
{
  const guint buttons[]={4,5,8};
  create_image();
  for(guint i=0;i<G_N_ELEMENTS(buttons);++i)
    {
      gint cx=shell->disp_width/2,cy=shell->disp_height/2;
      gimp_modifiers_manager_set(manager,device,buttons[i],GDK_MOD1_MASK,GIMP_MODIFIER_ACTION_STEP_ROTATING,NULL);
      gimp_display_shell_rotate_to(shell,12.5);
      send_event(GDK_BUTTON_PRESS,buttons[i],GDK_MOD1_MASK,cx+80,cy,0);
      g_assert_cmpuint(shell->mod_action_button,==,buttons[i]);g_assert_false(shell->painter_navigation_inherited);
      send_event(GDK_KEY_RELEASE,0,GDK_MOD1_MASK,0,0,GDK_KEY_Alt_L);
      g_assert_cmpuint(shell->mod_action_button,==,buttons[i]);
      send_event(GDK_BUTTON_PRESS,3,0,cx,cy,0);
      g_assert_cmpuint(shell->mod_action_button,==,buttons[i]);
      send_event(GDK_BUTTON_RELEASE,3,0,cx,cy,0);
      g_assert_cmpuint(shell->mod_action_button,==,buttons[i]);
      send_event(GDK_MOTION_NOTIFY,0,0,cx,cy+80,0);
      g_assert_cmpfloat_with_epsilon(shell->rotate_angle,105,1e-8);
      send_event(GDK_BUTTON_RELEASE,buttons[i],0,cx,cy+80,0);
      g_assert_cmpuint(shell->mod_action_button,==,0);g_assert_null(shell->grab_seat);
    }
  close_image();
}
static void radial_zoom_event(void)
{
  gint cx,cy;
  gdouble anchor;
  create_image();
  cx=shell->disp_width/2;cy=shell->disp_height/2;
  anchor=gimp_painter_navigation_zoom_begin(shell->disp_width,shell->disp_height,cx+40,cy,shell->scale_x,shell->scale_y);
  send_event(GDK_BUTTON_PRESS,2,GDK_CONTROL_MASK|GDK_MOD1_MASK,cx+40,cy,0);
  g_assert_cmpint(shell->mod_action,==,GIMP_MODIFIER_ACTION_ZOOMING);
  send_event(GDK_MOTION_NOTIFY,0,GDK_BUTTON2_MASK,cx+70,cy,0);
  g_assert_cmpfloat_with_epsilon(shell->scale_x,gimp_painter_navigation_zoom(shell->disp_width,shell->disp_height,cx+70,cy,anchor),1e-8);
  send_event(GDK_BUTTON_RELEASE,2,GDK_BUTTON2_MASK,cx+70,cy,0);
  g_assert_cmpfloat(shell->painter_zoom_anchor,==,0);close_image();
}
static void picker_clears_grab(void)
{
  create_image();
  gimp_modifiers_manager_set(manager,device,3,GDK_MOD1_MASK,GIMP_MODIFIER_ACTION_LAYER_PICKING,NULL);
  send_event(GDK_BUTTON_PRESS,3,GDK_MOD1_MASK,shell->disp_width/2,shell->disp_height/2,0);
  g_assert_cmpint(shell->mod_action,==,GIMP_MODIFIER_ACTION_LAYER_PICKING);
  send_event(GDK_KEY_RELEASE,0,GDK_MOD1_MASK,0,0,GDK_KEY_Alt_L);
  g_assert_cmpuint(shell->mod_action_button,==,0);g_assert_null(shell->grab_seat);
  close_image();
}
int main(int argc,char **argv)
{
  gint result;
  g_test_init(&argc,&argv,NULL);if(!gtk_init_check(&argc,&argv))return GIMP_EXIT_TEST_SKIPPED;
  gimp_test_utils_setup_menus_path();gimp=gimp_init_for_gui_testing(TRUE);
  g_test_add_func("/navigation-events/default-rotation",rotate_defaults);
  g_test_add_func("/navigation-events/arbitrary-buttons",arbitrary_button_constraints);
  g_test_add_func("/navigation-events/radial-zoom",radial_zoom_event);
  g_test_add_func("/navigation-events/picker-cleanup",picker_clears_grab);
  g_application_run(gimp->app,0,NULL);
  result=gimp_core_app_get_exit_status(GIMP_CORE_APP(gimp->app));
  g_application_quit(G_APPLICATION(gimp->app));g_clear_object(&gimp->app);return result;
}
