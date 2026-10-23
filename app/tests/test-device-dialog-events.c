/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gegl.h>
#include <gtk/gtk.h>

#include "libgimpbase/gimpbase.h"
#include "core/core-types.h"
#include "display/display-types.h"
#include "core/gimp.h"
#include "core/gimpcontext.h"
#include "core/gimpimage.h"
#include "display/gimpdisplay.h"
#include "display/gimpdisplay-foreach.h"
#include "display/gimpdisplayshell.h"
#include "display/gimpdisplayshell-tool-events.h"
#include "widgets/gimpdeviceinfo.h"
#include "widgets/gimpdeviceinfo-coords.h"
#include "widgets/gimpdevicemanager.h"
#include "widgets/gimpdevices.h"
#include "dialogs/painter-layer-dialog.h"
#include "gimpcoreapp.h"
#include "gimp-app-test-utils.h"
#include "tests.h"

static Gimp *gimp;
static GimpDisplayShell *shell;
static GimpDeviceManager *manager;
static guint reentrant_focus_count;

static GdkEvent *
focus_event (gboolean in)
{
  GdkEvent *event = gdk_event_new (GDK_FOCUS_CHANGE);
  GdkSeat *seat = gdk_display_get_default_seat (gtk_widget_get_display (shell->canvas));

  event->focus_change.window = g_object_ref (gtk_widget_get_window (shell->canvas));
  event->focus_change.in = in;
  event->focus_change.send_event = TRUE;
  gdk_event_set_device (event, gdk_seat_get_keyboard (seat));
  gdk_event_set_source_device (event, gdk_seat_get_keyboard (seat));
  return event;
}

static gboolean
focus_during_initialization (GSignalInvocationHint *hint,
                             guint n_values,
                             const GValue *values,
                             gpointer data)
{
  GdkEvent *event = focus_event (FALSE);

  g_assert_true (gimp_has_focused_once (gimp));
  g_assert_null (gimp_device_manager_get_current_device (manager));
  g_assert_false (gimp_display_shell_canvas_tool_events (shell->canvas, event, shell));
  gdk_event_free (event);
  reentrant_focus_count++;
  return TRUE;
}

static void
cancel_filter_dialog (GimpImage *image)
{
  GtkWidget *dialog = painter_filter_layer_dialog_new (image, NULL,
                                                       gimp_get_user_context (gimp),
                                                       GTK_WIDGET (shell));
  gpointer weak_dialog = dialog;
  gint count = gimp_image_get_n_layers (image);

  g_assert_nonnull (dialog);
  g_object_add_weak_pointer (G_OBJECT (dialog), &weak_dialog);
  gtk_widget_show (dialog);
  gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL);
  g_assert_null (weak_dialog);
  g_assert_cmpint (gimp_image_get_n_layers (image), ==, count);
}

static void
lazy_device_focus_lifecycle (void)
{
  GimpImage *image;
  GimpDeviceInfo *info;
  GdkEvent *event;
  GdkSeat *seat;
  GdkDevice *pointer;
  GimpCoords coords;
  GdkModifierType state;
  GList *devices;
  gulong hook;
  guint disabled_mice = 0;

  manager = gimp_devices_get_manager (gimp);
  g_assert_false (gimp_has_focused_once (gimp));
  g_assert_null (gimp_device_manager_get_current_device (manager));
  gimp_test_utils_create_image (gimp, 64, 64);
  shell = gimp_display_get_shell (gimp_get_display_iter (gimp)->data);
  image = gimp_display_get_image (shell->display);

  /* Keep fixture setup independent of window-manager focus timing. Hold the
   * canvas callbacks only until its first draw, then restore the production
   * connections before delivering any regression input. */
  g_signal_handlers_block_by_func (shell->canvas,
                                   gimp_display_shell_canvas_tool_events, shell);
  gimp_test_run_mainloop_until_idle ();
  g_signal_handlers_unblock_by_func (shell->canvas,
                                     gimp_display_shell_canvas_tool_events, shell);
  g_assert_true (gimp_displays_accept_focus_events (gimp));
  g_assert_false (gimp_has_focused_once (gimp));
  g_assert_true (gtk_widget_get_realized (shell->canvas));
  seat = gdk_display_get_default_seat (gtk_widget_get_display (shell->canvas));
  pointer = gdk_seat_get_pointer (seat);
  g_assert_nonnull (pointer);
  g_assert_null (gimp_device_info_get_by_device (pointer));

  /* Regression: a canvas can lose GTK focus before GIMP receives its first
   * focus-in. No input sample or tool update is possible in this state. */
  event = focus_event (FALSE);
  gtk_widget_send_focus_change (shell->canvas, event);
  gdk_event_free (event);
  g_assert_false (gimp_has_focused_once (gimp));
  g_assert_null (gimp_device_manager_get_current_device (manager));
  cancel_filter_dialog (image);

  /* The flag becomes true before the manager's signal handler runs. Verify
   * that it is not mistaken for completed device initialization. */
  hook = g_signal_add_emission_hook (g_signal_lookup ("focused-once", GIMP_TYPE_GIMP),
                                     0, focus_during_initialization, NULL, NULL);
  event = focus_event (TRUE);
  gtk_widget_send_focus_change (shell->canvas, event);
  gdk_event_free (event);
  g_signal_remove_emission_hook (g_signal_lookup ("focused-once", GIMP_TYPE_GIMP), hook);
  g_assert_cmpuint (reentrant_focus_count, ==, 1);
  info = gimp_device_manager_get_current_device (manager);
  g_assert_true (GIMP_IS_DEVICE_INFO (info));
  g_assert_true (gimp_device_info_get_device (info, NULL) == pointer);

  /* A valid mouse sample retains its coordinates, modifier state and the
   * normal no-pressure-axis default. No low-level sampling code is replaced. */
  event = gdk_event_new (GDK_MOTION_NOTIFY);
  event->motion.window = g_object_ref (gtk_widget_get_window (shell->canvas));
  event->motion.x = 17.25;
  event->motion.y = 29.5;
  event->motion.state = GDK_SHIFT_MASK | GDK_CONTROL_MASK;
  gdk_event_set_device (event, pointer);
  gdk_event_set_source_device (event, pointer);
  g_assert_true (gimp_device_info_get_event_coords (info, event->any.window, event, &coords));
  g_assert_cmpfloat (coords.x, ==, 17.25);
  g_assert_cmpfloat (coords.y, ==, 29.5);
  g_assert_cmpfloat (coords.pressure, ==, GIMP_COORDS_DEFAULT_PRESSURE);
  g_assert_true (gimp_device_info_get_event_state (info, event->any.window, event, &state));
  g_assert_cmpuint (state, ==, GDK_SHIFT_MASK | GDK_CONTROL_MASK);

  /* Ordinary slave mice still use the master settings and remain disabled. */
  devices = gdk_seat_get_slaves (seat, GDK_SEAT_CAPABILITY_POINTER);
  for (GList *iter = devices; iter; iter = iter->next)
    if (gdk_device_get_source (iter->data) == GDK_SOURCE_MOUSE)
      {
        g_assert_cmpint (gdk_device_get_mode (iter->data), ==, GDK_MODE_DISABLED);
        gdk_event_set_source_device (event, iter->data);
        g_assert_true (gimp_devices_get_from_event (gimp, event, NULL) == pointer);
        disabled_mice++;
      }
  g_test_message ("Disabled slave mice checked: %u", disabled_mice);
  g_list_free (devices);
  gdk_event_free (event);

  /* Repeat normal focus loss and restoration around harmless close/reopen. */
  for (guint i = 0; i < 2; i++)
    {
      event = focus_event (FALSE);
      gtk_widget_send_focus_change (shell->canvas, event);
      gdk_event_free (event);
      cancel_filter_dialog (image);
      event = focus_event (TRUE);
      gtk_widget_send_focus_change (shell->canvas, event);
      gdk_event_free (event);
      g_assert_true (gimp_device_manager_get_current_device (manager) == info);
    }

  g_test_message ("Pre-focus, initialization reentry, valid mouse sample and repeated dialog cancellation passed");
  g_object_unref (image);
  gimp_display_close (shell->display);
  gimp_test_run_mainloop_until_idle ();
}

int
main (int argc, char **argv)
{
  gint result;
  g_test_init (&argc, &argv, NULL);
  if (! gtk_init_check (&argc, &argv))
    return GIMP_EXIT_TEST_SKIPPED;
  gimp_test_utils_setup_menus_path ();
  gimp = gimp_init_for_gui_testing (TRUE);
  g_test_add_func ("/device-dialog/lazy-focus-lifecycle", lazy_device_focus_lifecycle);
  g_application_run (gimp->app, 0, NULL);
  result = gimp_core_app_get_exit_status (GIMP_CORE_APP (gimp->app));
  g_application_quit (G_APPLICATION (gimp->app));
  g_clear_object (&gimp->app);
  return result;
}
