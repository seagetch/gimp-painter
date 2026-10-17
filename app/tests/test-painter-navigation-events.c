/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <math.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include <gegl.h>
#include "libgimpbase/gimpbase.h"
#include "libgimpwidgets/gimpwidgets.h"
#include "core/core-types.h"
#include "display/display-types.h"
#include "tools/tools-types.h"
#include "core/gimp.h"
#include "core/gimp-gui.h"
#include "core/gimpcontainer.h"
#include "core/gimpcontext.h"
#include "core/gimptoolinfo.h"
#include "core/gimptooloptions.h"
#include "core/gimpimage.h"
#include "config/gimpdisplayconfig.h"
#include "display/gimpdisplay.h"
#include "display/gimpdisplayshell.h"
#include "display/gimpdisplayshell-tool-events.h"
#include "display/gimpdisplayshell-rotate.h"
#include "display/gimpdisplayshell-transform.h"
#include "tools/gimptool.h"
#include "tools/gimptoolcontrol.h"
#include "tools/gimptooloptions-gui.h"
#include "tools/tool_manager.h"
#include "widgets/gimpwidgets-utils.h"
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
/* Original 25.001: start selection and valid occupied-input lifecycles only.
 * Expectations below come from the pinned old handler (747-842, 1813-1839),
 * not from the production action helper.  The two real tools remain essential:
 * the recorder supplements them with primary-callback counts.
 */
typedef struct
{
  const gchar        *name;
  GdkModifierType     state;
  GimpModifierAction  action;
} StartCase;

static const StartCase start_cases[] =
{
  { "plain",             0,                                               GIMP_MODIFIER_ACTION_PANNING },
  { "alt",               GDK_MOD1_MASK,                                   GIMP_MODIFIER_ACTION_PANNING },
  { "super",             GDK_SUPER_MASK,                                  GIMP_MODIFIER_ACTION_PANNING },
  { "meta",              GDK_META_MASK,                                   GIMP_MODIFIER_ACTION_PANNING },
  { "control",           GDK_CONTROL_MASK,                                GIMP_MODIFIER_ACTION_ZOOMING },
  { "control-alt",       GDK_CONTROL_MASK | GDK_MOD1_MASK,                GIMP_MODIFIER_ACTION_ZOOMING },
  { "control-super-meta",GDK_CONTROL_MASK | GDK_SUPER_MASK | GDK_META_MASK,GIMP_MODIFIER_ACTION_ZOOMING },
  { "shift",             GDK_SHIFT_MASK,                                  GIMP_MODIFIER_ACTION_ROTATING },
  { "shift-control",     GDK_SHIFT_MASK | GDK_CONTROL_MASK,               GIMP_MODIFIER_ACTION_STEP_ROTATING },
  { "shift-control-alt", GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK,GIMP_MODIFIER_ACTION_STEP_ROTATING },
  { "shift-super",       GDK_SHIFT_MASK | GDK_SUPER_MASK,                 GIMP_MODIFIER_ACTION_ROTATING },
  { "shift-meta",        GDK_SHIFT_MASK | GDK_META_MASK,                  GIMP_MODIFIER_ACTION_ROTATING }
};

static const gchar *real_tool_names[] =
{
  "gimp-paintbrush-tool", "gimp-move-tool"
};

typedef struct
{
  GimpModifierAction action;
  guint              button;
  gboolean           inherited;
  gint               start_x, start_y, last_x, last_y;
  gdouble            anchor, zoom_anchor, angle, scale_x, scale_y;
  GdkSeat           *seat;
  GdkDevice         *pointer, *source;
  gboolean           grabbed;
} NavigationState;

static NavigationState
navigation_state (void)
{
  NavigationState saved =
  {
    shell->mod_action, shell->mod_action_button,
    shell->painter_navigation_inherited,
    shell->scroll_start_x, shell->scroll_start_y,
    shell->scroll_last_x, shell->scroll_last_y,
    shell->rotate_drag_angle, shell->painter_zoom_anchor,
    shell->rotate_angle, shell->scale_x, shell->scale_y,
    shell->grab_seat, shell->grab_pointer, shell->grab_pointer_source,
    gdk_display_device_is_grabbed (gdk_device_get_display (device), device)
  };
  return saved;
}

static void
assert_navigation_unchanged (const NavigationState *saved)
{
  g_assert_cmpint (shell->mod_action, ==, saved->action);
  g_assert_cmpuint (shell->mod_action_button, ==, saved->button);
  g_assert_cmpint (shell->painter_navigation_inherited, ==, saved->inherited);
  g_assert_cmpint (shell->scroll_start_x, ==, saved->start_x);
  g_assert_cmpint (shell->scroll_start_y, ==, saved->start_y);
  g_assert_cmpint (shell->scroll_last_x, ==, saved->last_x);
  g_assert_cmpint (shell->scroll_last_y, ==, saved->last_y);
  g_assert_cmpfloat (shell->rotate_drag_angle, ==, saved->anchor);
  g_assert_cmpfloat (shell->painter_zoom_anchor, ==, saved->zoom_anchor);
  g_assert_cmpfloat (shell->rotate_angle, ==, saved->angle);
  g_assert_cmpfloat (shell->scale_x, ==, saved->scale_x);
  g_assert_cmpfloat (shell->scale_y, ==, saved->scale_y);
  g_assert_true (shell->grab_seat == saved->seat);
  g_assert_true (shell->grab_pointer == saved->pointer);
  g_assert_true (shell->grab_pointer_source == saved->source);
  g_assert_cmpint (gdk_display_device_is_grabbed (gdk_device_get_display (device), device),
                   ==, saved->grabbed);
}

static void
assert_grab (void)
{
  g_assert_true (shell->grab_seat == gdk_device_get_seat (device));
  g_assert_true (shell->grab_pointer == device);
  /* gimp_devices_get_from_event() deliberately reports no physical source
   * for synthesized events from the seat's master pointer. */
  g_assert_cmpint (gdk_device_get_device_type (device), ==, GDK_DEVICE_TYPE_MASTER);
  g_assert_null (shell->grab_pointer_source);
  g_assert_true (gdk_display_device_is_grabbed (gdk_device_get_display (device), device));
}

static void
assert_navigation_idle (void)
{
  gboolean grabbed_before_sync;

  g_assert_cmpint (shell->mod_action, ==, GIMP_MODIFIER_ACTION_NONE);
  g_assert_cmpuint (shell->mod_action_button, ==, 0);
  g_assert_false (shell->painter_navigation_inherited);
  g_assert_cmpint (shell->scroll_start_x, ==, 0);
  g_assert_cmpint (shell->scroll_start_y, ==, 0);
  g_assert_cmpint (shell->scroll_last_x, ==, 0);
  g_assert_cmpint (shell->scroll_last_y, ==, 0);
  g_assert_cmpfloat (shell->rotate_drag_angle, ==, 0.0);
  g_assert_cmpfloat (shell->painter_zoom_anchor, ==, 0.0);
  g_assert_null (shell->grab_seat);
  g_assert_null (shell->grab_pointer);
  g_assert_null (shell->grab_pointer_source);
  g_assert_false (shell->button1_release_pending);
  /* The direct canvas call bypasses the normal event-loop turn.  GDK X11
   * retires its explicit-grab record in an idle callback after an asynchronous
   * server roundtrip.  Check the synchronous shell cleanup above first, then
   * observe GDK after both stages; never ungrab or alter state in the test. */
  grabbed_before_sync = gdk_display_device_is_grabbed (gdk_device_get_display (device), device);
  gdk_display_sync (gdk_device_get_display (device));
  gimp_test_run_mainloop_until_idle ();
  if (grabbed_before_sync)
    g_test_message ("pending GDK grab after shell cleanup: before-sync=1 after-sync=%d",
                    gdk_display_device_is_grabbed (gdk_device_get_display (device), device));
  g_assert_false (gdk_display_device_is_grabbed (gdk_device_get_display (device), device));
}

static GimpTool *
select_start_tool (const gchar *name)
{
  GimpToolInfo *info = gimp_get_tool_info (gimp, name);
  GimpTool     *tool;

  g_assert_nonnull (info);
  gimp_context_set_tool (gimp_get_user_context (gimp), info);
  tool = tool_manager_get_active (gimp);
  g_assert_nonnull (tool);
  g_assert_true (tool->tool_info == info);
  g_assert_cmpstr (gimp_object_get_name (tool->tool_info), ==, name);
  g_assert_false (gimp_tool_control_is_active (tool->control));
  return tool;
}

static void
prepare_start (void)
{
  assert_navigation_idle ();
  gimp_display_shell_flip (shell, FALSE, FALSE);
  gimp_display_shell_rotate_to (shell, 42.5);
}

static void
assert_middle_start (GimpTool          *tool,
                     GdkModifierType   state,
                     GimpModifierAction action,
                     gint              x,
                     gint              y)
{
  GimpToolInfo    *info = tool->tool_info;
  GimpToolControl *control = tool->control;
  gint            id = tool->ID;
  guint           press_state = tool->button_press_state;
  guint           active_state = tool->active_modifier_state;
  gdouble         angle = shell->rotate_angle;
  gdouble         scale_x = shell->scale_x, scale_y = shell->scale_y;

  assert_navigation_idle ();
  g_assert_false (gimp_tool_control_is_active (control));
  send_event (GDK_BUTTON_PRESS, 2, state, x, y, 0);
  g_assert_true (tool_manager_get_active (gimp) == tool);
  g_assert_true (tool->tool_info == info);
  g_assert_true (tool->control == control);
  g_assert_cmpint (tool->ID, ==, id);
  g_assert_false (gimp_tool_control_is_active (control));
  g_assert_true (tool->focus_display == shell->display);
  g_assert_cmpuint (tool->modifier_state, ==, state | GDK_BUTTON2_MASK);
  g_assert_cmpuint (tool->button_press_state, ==, press_state);
  g_assert_cmpuint (tool->active_modifier_state, ==, active_state);
  g_assert_cmpint (shell->mod_action, ==, action);
  g_assert_cmpuint (shell->mod_action_button, ==, 2);
  g_assert_true (shell->painter_navigation_inherited);
  g_assert_cmpfloat (shell->rotate_angle, ==, angle);
  g_assert_cmpfloat (shell->scale_x, ==, scale_x);
  g_assert_cmpfloat (shell->scale_y, ==, scale_y);
  g_assert_cmpint (shell->scroll_start_x, ==, x);
  g_assert_cmpint (shell->scroll_start_y, ==, y);
  g_assert_cmpint (shell->scroll_last_x, ==, x);
  g_assert_cmpint (shell->scroll_last_y, ==, y);
  /* All new starts use the unmirrored bottom-center ray: its bearing is zero.
   * This checks anchor establishment without replaying drag-angle arithmetic. */
  g_assert_cmpint (x, ==, shell->disp_width / 2);
  g_assert_cmpint (y, >, shell->disp_height / 2);
  g_assert_cmpfloat_with_epsilon (shell->rotate_drag_angle, angle, 1e-8);
  g_assert_cmpfloat_with_epsilon (shell->painter_zoom_anchor,
                                  MAX (scale_x, scale_y) * 300.0 -
                                  (y - shell->disp_height / 2), 1e-8);
  assert_grab ();
}

static void
release_middle (GdkModifierType state)
{
  send_event (GDK_BUTTON_RELEASE, 2, state | GDK_BUTTON2_MASK,
              shell->scroll_start_x, shell->scroll_start_y, 0);
  g_test_message ("checking navigation cleanup after middle release");
  assert_navigation_idle ();
  /* Middle release does not update the tool's idle modifier state. */
}

static void
start_matrix_for_tool (GimpTool *tool)
{
  for (guint i = 0; i < G_N_ELEMENTS (start_cases); i++)
    {
      const StartCase *test = &start_cases[i];
      g_test_message ("tool=%s start=%s state=%u action=%d",
                      gimp_object_get_name (tool->tool_info), test->name,
                      test->state, test->action);
      prepare_start ();
      assert_middle_start (tool, test->state, test->action,
                           shell->disp_width / 2, shell->disp_height / 2 + 80);
      release_middle (test->state);
    }
}

static void
real_tool_start_matrix (gconstpointer data)
{
  create_image ();
  start_matrix_for_tool (select_start_tool (data));
  close_image ();
}

static void
retained_old_starts (void)
{
  const guint sequences[] = { 2, 222 };
  const guint states[] = { GDK_SHIFT_MASK,
                          GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK };
  const GimpModifierAction actions[] = { GIMP_MODIFIER_ACTION_ROTATING,
                                        GIMP_MODIFIER_ACTION_STEP_ROTATING };
  const gchar *root = g_getenv ("GIMP_TESTING_ABS_TOP_SRCDIR");
  gchar       *path, *contents, *checksum;
  gchar      **lines;
  GError      *error = NULL;
  guint        matched = 0;

  g_assert_nonnull (root);
  path = g_build_filename (root, "migration", "fixtures",
                           "legacy-rotation-operations", "gui-captures",
                           "rotation.tsv", NULL);
  g_assert_true (g_file_get_contents (path, &contents, NULL, &error));
  g_assert_no_error (error);
  checksum = g_compute_checksum_for_string (G_CHECKSUM_SHA256, contents, -1);
  g_assert_cmpstr (checksum, ==,
                  "bfc8b4ab8f532f5b8e0d97f2dd7cace9eec1aefe7a4cd532dbf4e4162cd9d46a");
  lines = g_strsplit (contents, "\n", -1);
  create_image ();
  for (guint i = 1; lines[i] && *lines[i]; i++)
    {
      gchar **row = g_strsplit (lines[i], "\t", -1);
      guint   seq = (guint) g_ascii_strtoull (row[0], NULL, 10);

      g_assert_cmpuint (g_strv_length (row), ==, 24);
      if (matched < G_N_ELEMENTS (sequences) && seq == sequences[matched])
        {
          gchar **setup = g_strsplit (lines[i - 1], "\t", -1);
          gint x, y;
          gdouble angle;

          g_assert_cmpuint (g_strv_length (setup), ==, 24);
          g_assert_cmpstr (setup[1], ==, row[1]);
          g_assert_cmpstr (setup[2], ==, "setup");
          g_assert_cmpint (atoi (row[3]), ==, 4); /* old GDK_BUTTON_PRESS */
          g_assert_cmpuint (g_ascii_strtoull (row[4], NULL, 10), ==, states[matched]);
          g_assert_cmpint (atoi (row[7]), ==, 2);
          g_assert_cmpint (atoi (row[8]), ==, 1);
          g_assert_cmpint (atoi (row[11]), ==, 0);
          g_assert_cmpint (atoi (row[14]), ==, 1); /* rotating */
          g_assert_cmpint (atoi (row[15]), ==, 0); /* scrolling */
          g_assert_cmpint (atoi (row[16]), ==, 0); /* scaling */
          g_assert_cmpint (atoi (row[17]), ==, 1); /* grabbed */
          angle = g_ascii_strtod (setup[12], NULL);
          g_assert_cmpfloat (g_ascii_strtod (row[12], NULL), ==, angle);
          /* Retain the recorded center-relative input on the modern viewport. */
          x = shell->disp_width / 2 + atoi (row[5]) - atoi (row[9]) / 2;
          y = shell->disp_height / 2 + atoi (row[6]) - atoi (row[10]) / 2;
          for (guint t = 0; t < G_N_ELEMENTS (real_tool_names); t++)
            {
              GimpTool *tool = select_start_tool (real_tool_names[t]);
              g_test_message ("retained old seq=%u tool=%s", seq, real_tool_names[t]);
              prepare_start ();
              gimp_display_shell_rotate_to (shell, angle);
              assert_middle_start (tool, states[matched], actions[matched], x, y);
              g_assert_cmpfloat_with_epsilon (shell->rotate_drag_angle,
                                              g_ascii_strtod (row[13], NULL), 1e-8);
              release_middle (states[matched]);
            }
          matched++;
          g_strfreev (setup);
        }
      g_strfreev (row);
    }
  g_assert_cmpuint (matched, ==, G_N_ELEMENTS (sequences));
  close_image ();
  g_strfreev (lines);
  g_free (checksum);
  g_free (contents);
  g_free (path);
}

/* Original 25.004: one stationary, unmirrored old GUI sequence, with the
 * reverse transition retained for 25.005 timing.  These literal expectations
 * are checked against immutable rows 22-30, never a production math helper. */
static void
retained_ctrl_mid_drag (void)
{
  static const struct
  {
    const gchar     *step;
    GdkEventType     type;
    GdkModifierType  state;
    guint            detail;
    gdouble          angle;
  } sequence[] =
  {
    { "shift-middle", GDK_BUTTON_PRESS,
      GDK_SHIFT_MASK, 2, 7.5 },
    { "free", GDK_MOTION_NOTIFY,
      GDK_BUTTON2_MASK | GDK_SHIFT_MASK, 0, 7.5 },
    { "ctrl-press", GDK_KEY_PRESS,
      GDK_BUTTON2_MASK | GDK_SHIFT_MASK, GDK_KEY_Control_L, 7.5 },
    { "snap", GDK_MOTION_NOTIFY,
      GDK_BUTTON2_MASK | GDK_SHIFT_MASK | GDK_CONTROL_MASK, 0, 15.0 },
    { "shift-release", GDK_KEY_RELEASE,
      GDK_BUTTON2_MASK | GDK_SHIFT_MASK | GDK_CONTROL_MASK, GDK_KEY_Shift_L, 15.0 },
    { "snap-without-shift", GDK_MOTION_NOTIFY,
      GDK_BUTTON2_MASK | GDK_CONTROL_MASK, 0, 15.0 },
    { "ctrl-release", GDK_KEY_RELEASE,
      GDK_BUTTON2_MASK | GDK_CONTROL_MASK, GDK_KEY_Control_L, 15.0 },
    { "restored", GDK_MOTION_NOTIFY,
      GDK_BUTTON2_MASK, 0, 7.5 },
    { "middle-release", GDK_BUTTON_RELEASE,
      GDK_BUTTON2_MASK, 2, 7.5 }
  };
  const gchar *root = g_getenv ("GIMP_TESTING_ABS_TOP_SRCDIR");
  gchar       *path, *contents, *checksum;
  gchar      **lines;
  GError      *error = NULL;

  g_assert_nonnull (root);
  path = g_build_filename (root, "migration", "fixtures",
                           "legacy-rotation-operations", "gui-captures",
                           "rotation.tsv", NULL);
  g_assert_true (g_file_get_contents (path, &contents, NULL, &error));
  g_assert_no_error (error);
  checksum = g_compute_checksum_for_string (G_CHECKSUM_SHA256, contents, -1);
  g_assert_cmpstr (checksum, ==,
                  "bfc8b4ab8f532f5b8e0d97f2dd7cace9eec1aefe7a4cd532dbf4e4162cd9d46a");
  lines = g_strsplit (contents, "\n", -1);
  g_assert_cmpuint (g_strv_length (lines), >, 30);
  g_assert_cmpstr (lines[0], ==,
                  "seq\tscene\tstep\ttype\tstate\tx\ty\tdetail\thandled\twidth\theight"
                  "\tmirror\tangle\tanchor\trotating\tscrolling\tscaling\tgrabbed"
                  "\tdevice_x\tdevice_y\tunrotated_x\tunrotated_y\tback_x\tback_y");
  for (guint i = 0; i < G_N_ELEMENTS (sequence); i++)
    {
      gchar    **row = g_strsplit (lines[22 + i], "\t", -1);
      gboolean   held = sequence[i].type != GDK_BUTTON_RELEASE;
      gboolean   key = sequence[i].type == GDK_KEY_PRESS ||
                       sequence[i].type == GDK_KEY_RELEASE;

      g_assert_cmpuint (g_strv_length (row), ==, 24);
      g_assert_cmpuint (g_ascii_strtoull (row[0], NULL, 10), ==, 22 + i);
      g_assert_cmpstr (row[1], ==, "stationary-m0-i2");
      g_assert_cmpstr (row[2], ==, sequence[i].step);
      g_assert_cmpint (atoi (row[3]), ==, sequence[i].type);
      g_assert_cmpuint (g_ascii_strtoull (row[4], NULL, 10), ==, sequence[i].state);
      g_assert_cmpint (atoi (row[5]) - atoi (row[9]) / 2, ==, 0);
      g_assert_cmpint (atoi (row[6]) - atoi (row[10]) / 2, ==, 100);
      g_assert_cmpuint (g_ascii_strtoull (row[7], NULL, 10), ==, sequence[i].detail);
      g_assert_cmpint (atoi (row[8]), ==, ! key);
      g_assert_cmpint (atoi (row[11]), ==, 0);
      g_assert_cmpfloat (g_ascii_strtod (row[12], NULL), ==, sequence[i].angle);
      g_assert_cmpfloat (g_ascii_strtod (row[13], NULL), ==, held ? 7.5 : 0.0);
      g_assert_cmpint (atoi (row[14]), ==, held);
      g_assert_cmpint (atoi (row[15]), ==, 0);
      g_assert_cmpint (atoi (row[16]), ==, 0);
      g_assert_cmpint (atoi (row[17]), ==, held);
      g_strfreev (row);
    }

  create_image ();
  for (guint t = 0; t < G_N_ELEMENTS (real_tool_names); t++)
    {
      GimpTool       *tool = select_start_tool (real_tool_names[t]);
      NavigationState held;
      gint            x = shell->disp_width / 2;
      gint            y = shell->disp_height / 2 + 100;

      prepare_start ();
      gimp_display_shell_rotate_to (shell, 7.5);
      assert_middle_start (tool, sequence[0].state, GIMP_MODIFIER_ACTION_ROTATING, x, y);
      held = navigation_state ();
      for (guint i = 1; i < G_N_ELEMENTS (sequence); i++)
        {
          NavigationState before = navigation_state ();

          g_test_message ("retained old seq=%u step=%s tool=%s",
                          22 + i, sequence[i].step, real_tool_names[t]);
          if (sequence[i].type == GDK_BUTTON_RELEASE)
            release_middle (0);
          else
            {
              send_event (sequence[i].type, 0, sequence[i].state,
                          x, y, sequence[i].detail);
              if (sequence[i].type == GDK_KEY_PRESS ||
                  sequence[i].type == GDK_KEY_RELEASE)
                assert_navigation_unchanged (&before);
              /* Only the displayed angle may change on stationary motion.
               * Both anchors, initiating button, action and actual grab stay. */
              held.angle = shell->rotate_angle;
              assert_navigation_unchanged (&held);
              assert_grab ();
            }
          g_assert_cmpfloat_with_epsilon (shell->rotate_angle, sequence[i].angle, 1e-8);
          g_assert_true (tool_manager_get_active (gimp) == tool);
          g_assert_false (gimp_tool_control_is_active (tool->control));
        }
      assert_middle_start (tool, GDK_SHIFT_MASK, GIMP_MODIFIER_ACTION_ROTATING, x, y);
      release_middle (GDK_SHIFT_MASK);
    }
  close_image ();
  g_strfreev (lines);
  g_free (checksum);
  g_free (contents);
  g_free (path);
}

typedef enum
{
  START_BUSY, START_HELD_PRIMARY, START_HELD_TERTIARY, START_UNBOUND,
  START_OCCUPIED_PAN, START_OCCUPIED_ZOOM, START_OCCUPIED_ROTATION
} RejectedStart;

static void
rejected_start_matrix (gconstpointer data)
{
  GimpTool *tool;

  create_image ();
  tool = select_start_tool (data);

  for (guint test = START_BUSY; test <= START_OCCUPIED_ROTATION; test++)
    {
      NavigationState saved;
      guint state = GDK_SHIFT_MASK, button = 2, occupied_state = 0;
      guint press_state = tool->button_press_state;
      guint active_state = tool->active_modifier_state;
      gint x = shell->disp_width / 2, y = shell->disp_height / 2 + 80;

      g_test_message ("tool=%s rejected-start=%u", (const gchar *) data, test);
      prepare_start ();
      if (test == START_BUSY)
        gimp_set_busy (gimp);
      else if (test == START_HELD_PRIMARY)
        state |= GDK_BUTTON1_MASK;
      else if (test == START_HELD_TERTIARY)
        state |= GDK_BUTTON3_MASK;
      else if (test == START_UNBOUND)
        button = 8;
      else
        {
          GimpModifierAction action = GIMP_MODIFIER_ACTION_PANNING;
          if (test == START_OCCUPIED_ZOOM)
            { occupied_state = GDK_CONTROL_MASK; action = GIMP_MODIFIER_ACTION_ZOOMING; }
          else if (test == START_OCCUPIED_ROTATION)
            { occupied_state = GDK_SHIFT_MASK; action = GIMP_MODIFIER_ACTION_ROTATING; }
          assert_middle_start (tool, occupied_state, action, x, y);
          state |= GDK_BUTTON2_MASK;
        }
      saved = navigation_state ();
      /* A distinct position would replace the start/anchor if incorrectly accepted. */
      send_event (GDK_BUTTON_PRESS, button, state, x + 60, y - 130, 0);
      assert_navigation_unchanged (&saved);
      g_assert_true (tool_manager_get_active (gimp) == tool);
      g_assert_false (gimp_tool_control_is_active (tool->control));
      g_assert_cmpuint (tool->button_press_state, ==, press_state);
      g_assert_cmpuint (tool->active_modifier_state, ==, active_state);
      /* Unbound input still updates focus/modifiers before selecting NONE.
       * Device/display selection may also precede the other rejection guards. */
      if (test == START_UNBOUND)
        {
          g_assert_true (tool->focus_display == shell->display);
          g_assert_cmpuint (tool->modifier_state, ==, GDK_SHIFT_MASK);
          send_event (GDK_BUTTON_RELEASE, 8, GDK_SHIFT_MASK, x, y, 0);
        }
      else if (test == START_BUSY)
        gimp_unset_busy (gimp);
      else if (test == START_HELD_PRIMARY || test == START_HELD_TERTIARY)
        send_event (GDK_BUTTON_RELEASE,
                    test == START_HELD_PRIMARY ? 1 : 3, state, x, y, 0);
      else
        release_middle (occupied_state);
      assert_navigation_idle ();
      assert_middle_start (tool, GDK_SHIFT_MASK, GIMP_MODIFIER_ACTION_ROTATING, x, y);
      release_middle (GDK_SHIFT_MASK);
    }
  close_image ();
}

/* An ordinary primary press must establish both active control and a real grab.
 * Never fabricate an active/no-grab tool state: the old route had no blanket
 * tool-control veto.  Test both the real Paintbrush and the callback recorder. */
static void
primary_drag_rejects_rotation (GimpTool *tool)
{
  NavigationState saved;
  guint press_state, active_state, modifier_state;
  GimpDisplay *focus;
  gdouble dx, dy;
  gint x, y;

  prepare_start ();
  gimp_display_shell_transform_xy_f (shell, 128, 128, &dx, &dy);
  x = (gint) dx; y = (gint) dy;
  send_event (GDK_BUTTON_PRESS, 1, 0, x, y, 0);
  g_assert_true (tool_manager_get_active (gimp) == tool);
  g_assert_true (gimp_tool_control_is_active (tool->control));
  g_assert_cmpint (shell->mod_action, ==, GIMP_MODIFIER_ACTION_NONE);
  g_assert_cmpuint (tool->button_press_state, ==, GDK_BUTTON1_MASK);
  assert_grab ();
  saved = navigation_state ();
  press_state = tool->button_press_state;
  active_state = tool->active_modifier_state;
  modifier_state = tool->modifier_state;
  focus = tool->focus_display;
  send_event (GDK_BUTTON_PRESS, 2, GDK_SHIFT_MASK | GDK_BUTTON1_MASK, x + 50, y + 30, 0);
  assert_navigation_unchanged (&saved);
  g_assert_true (tool_manager_get_active (gimp) == tool);
  g_assert_true (gimp_tool_control_is_active (tool->control));
  g_assert_true (tool->focus_display == focus);
  g_assert_cmpuint (tool->modifier_state, ==, modifier_state);
  g_assert_cmpuint (tool->button_press_state, ==, press_state);
  g_assert_cmpuint (tool->active_modifier_state, ==, active_state);
  /* Releasing the rejected middle press must not terminate the primary drag. */
  send_event (GDK_BUTTON_RELEASE, 2,
              GDK_SHIFT_MASK | GDK_BUTTON1_MASK | GDK_BUTTON2_MASK, x + 50, y + 30, 0);
  assert_navigation_unchanged (&saved);
  g_assert_true (gimp_tool_control_is_active (tool->control));
  send_event (GDK_BUTTON_RELEASE, 1, GDK_BUTTON1_MASK, x, y, 0);
  g_assert_false (gimp_tool_control_is_active (tool->control));
  assert_navigation_idle ();
  assert_middle_start (tool, GDK_SHIFT_MASK, GIMP_MODIFIER_ACTION_ROTATING,
                       shell->disp_width / 2, shell->disp_height / 2 + 80);
  release_middle (GDK_SHIFT_MASK);
}

static void
paintbrush_primary_drag (void)
{
  GimpTool *tool;

  create_image ();
  tool = select_start_tool (real_tool_names[0]);
  g_assert_nonnull (gimp_context_get_brush (GIMP_CONTEXT (tool->tool_info->tool_options)));
  primary_drag_rejects_rotation (tool);
  gimp_test_run_mainloop_until_idle ();
  close_image ();
}

typedef struct { GimpTool parent; } StartRecordingTool;
typedef struct { GimpToolClass parent; } StartRecordingToolClass;
GType start_recording_tool_get_type (void);
G_DEFINE_TYPE (StartRecordingTool, start_recording_tool, GIMP_TYPE_TOOL)
static guint start_tool_presses, start_tool_releases;

static gboolean
start_recording_initialize (GimpTool *tool, GimpDisplay *display, GError **error)
{
  tool->display = display;
  return TRUE;
}

static void
start_recording_press (GimpTool *tool, const GimpCoords *coords, guint32 time,
                       GdkModifierType state, GimpButtonPressType type, GimpDisplay *display)
{
  start_tool_presses++;
  tool->display = display;
  gimp_tool_control_activate (tool->control);
}

static void
start_recording_release (GimpTool *tool, const GimpCoords *coords, guint32 time,
                         GdkModifierType state, GimpButtonReleaseType type, GimpDisplay *display)
{
  start_tool_releases++;
  gimp_tool_control_halt (tool->control);
}

static void
start_recording_tool_class_init (StartRecordingToolClass *klass)
{
  GimpToolClass *tool = GIMP_TOOL_CLASS (klass);
  tool->initialize = start_recording_initialize;
  tool->button_press = start_recording_press;
  tool->button_release = start_recording_release;
}

static void
start_recording_tool_init (StartRecordingTool *recording)
{
  gimp_tool_control_set_handle_empty_image (GIMP_TOOL (recording)->control, TRUE);
}

static void
recording_tool_starts (void)
{
  GimpToolInfo *info;
  GimpTool *tool;

  create_image ();
  info = gimp_tool_info_new (gimp, start_recording_tool_get_type (), GIMP_TYPE_TOOL_OPTIONS, 0,
                             "painter-start-recording-test", "Record starts", "Record starts",
                             NULL, NULL, NULL, "gimp-tool-paintbrush", "gimp-paintbrush",
                             GIMP_ICON_TOOL_PAINTBRUSH);
  gimp_tools_set_tool_options_gui_func (info->tool_options, gimp_tool_options_gui);
  gimp_container_add (gimp->tool_info_list, GIMP_OBJECT (info));
  gimp_container_add (gimp->tool_item_list, GIMP_OBJECT (info));
  tool = select_start_tool ("painter-start-recording-test");
  start_tool_presses = start_tool_releases = 0;
  start_matrix_for_tool (tool);
  g_assert_cmpuint (start_tool_presses, ==, 0);
  g_assert_cmpuint (start_tool_releases, ==, 0);
  primary_drag_rejects_rotation (tool);
  g_assert_cmpuint (start_tool_presses, ==, 1);
  g_assert_cmpuint (start_tool_releases, ==, 1);
  select_start_tool (real_tool_names[0]);
  close_image ();
  g_object_unref (info);
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
  g_test_add_data_func ("/navigation-events/start/paintbrush", real_tool_names[0], real_tool_start_matrix);
  g_test_add_data_func ("/navigation-events/start/move", real_tool_names[1], real_tool_start_matrix);
  g_test_add_func ("/navigation-events/start/retained-old", retained_old_starts);
  g_test_add_data_func ("/navigation-events/start/reject-paintbrush", real_tool_names[0], rejected_start_matrix);
  g_test_add_data_func ("/navigation-events/start/reject-move", real_tool_names[1], rejected_start_matrix);
  g_test_add_func ("/navigation-events/start/primary-paintbrush", paintbrush_primary_drag);
  g_test_add_func ("/navigation-events/start/recording-tool", recording_tool_starts);
  g_test_add_func ("/navigation-events/rotation/ctrl-mid-drag", retained_ctrl_mid_drag);
  g_application_run(gimp->app,0,NULL);
  result=gimp_core_app_get_exit_status(GIMP_CORE_APP(gimp->app));
  g_application_quit(G_APPLICATION(gimp->app));g_clear_object(&gimp->app);return result;
}
