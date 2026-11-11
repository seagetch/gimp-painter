/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Standalone real-GTK regression; deliberately not part of ordinary build CI. */
#include <gtk/gtk.h>
#include "widgets/popupper.h"
#include <algorithm>

static void drain ()
{
  for (int i = 0; i < 30; ++i) {
    while (gtk_events_pending ()) gtk_main_iteration ();
    g_usleep (1000);
  }
}
static void destroy_on_map (GtkWidget *widget, gpointer) { gtk_widget_destroy (widget); }
static void run_case (const char *name, double ax, double ay, double pw, double ph,
                      bool default_size = false, bool destroy = false, bool outside = false)
{
  auto *display = gdk_display_get_default ();
  auto *monitor = gdk_display_get_monitor (display, 0);
  GdkRectangle bounds; gdk_monitor_get_geometry (monitor, &bounds);
  GtkWidget *window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_decorated (GTK_WINDOW (window), FALSE);
  GtkWidget *anchor = gtk_button_new_with_label ("anchor");
  gtk_container_add (GTK_CONTAINER (window), anchor);
  gtk_window_move (GTK_WINDOW (window), bounds.x + int (ax * bounds.width),
                   bounds.y + int (ay * bounds.height));
  gtk_widget_show_all (window); drain ();
  GtkWidget *body = gtk_drawing_area_new ();
  gtk_widget_set_size_request (body, int (pw * bounds.width), int (ph * bounds.height));
  GtkWidget *popup = gimp_painter_popup_new (body);
  g_assert_nonnull (popup);
  g_assert_false (gtk_widget_get_visible (popup));
  if (default_size) gtk_window_set_default_size (GTK_WINDOW (popup), bounds.width, bounds.height);
  GtkRequisition natural; gtk_widget_get_preferred_size (popup, nullptr, &natural);
  g_assert_cmpint (natural.width, >, 0); g_assert_cmpint (natural.height, >, 0);
  gint window_width, window_height; gtk_window_get_size (GTK_WINDOW (popup), &window_width, &window_height);
  if (default_size) g_assert_true (window_width != natural.width || window_height != natural.height);
  GtkAllocation allocation; gtk_widget_get_allocation (anchor, &allocation);
  gint left, top; gdk_window_get_origin (gtk_widget_get_window (anchor), &left, &top);
  if (!gtk_widget_get_has_window (anchor)) { left += allocation.x; top += allocation.y; }
  gint expected_x = left, expected_y = top + allocation.height;
  if (expected_x + natural.width > bounds.x + bounds.width) expected_x = left + allocation.width - natural.width;
  if (expected_y + natural.height > bounds.y + bounds.height) expected_y = top - natural.height;
  expected_x = natural.width <= bounds.width ? std::max (bounds.x, std::min (expected_x, bounds.x + bounds.width - natural.width)) : bounds.x;
  expected_y = natural.height <= bounds.height ? std::max (bounds.y, std::min (expected_y, bounds.y + bounds.height - natural.height)) : bounds.y;
  gpointer weak = popup; g_object_add_weak_pointer (G_OBJECT (popup), &weak);
  if (destroy) g_signal_connect (popup, "map", G_CALLBACK (destroy_on_map), nullptr);
  gimp_painter_popup_show_over (popup, anchor); drain ();
  if (destroy) { g_assert_null (weak); gtk_widget_destroy (window); drain (); return; }
  gint x, y; gtk_window_get_position (GTK_WINDOW (popup), &x, &y);
  g_test_message ("%s scale=%d anchor=%d,%d preferred=%dx%d window=%dx%d actual=%d,%d expected=%d,%d", name,
                  gtk_widget_get_scale_factor (popup), left, top, natural.width, natural.height,
                  window_width, window_height, x, y, expected_x, expected_y);
  g_assert_cmpint (x, ==, expected_x); g_assert_cmpint (y, ==, expected_y);
  g_assert_true (gtk_grab_get_current () == popup);
  if (!default_size && natural.width <= bounds.width && natural.height <= bounds.height) {
    g_assert_cmpint (x + gtk_widget_get_allocated_width (popup), <=, bounds.x + bounds.width);
    g_assert_cmpint (y + gtk_widget_get_allocated_height (popup), <=, bounds.y + bounds.height);
  }
  if (outside) {
    GdkEventButton event = {}; event.type = GDK_BUTTON_PRESS;
    event.window = gtk_widget_get_window (popup); event.x = -1; event.y = 1;
    g_assert_false (GTK_WIDGET_GET_CLASS (popup)->button_press_event (popup, &event));
  } else {
    GdkEventKey event = {}; event.type = GDK_KEY_PRESS;
    event.window = gtk_widget_get_window (popup); event.keyval = GDK_KEY_Escape;
    GTK_WIDGET_GET_CLASS (popup)->key_press_event (popup, &event);
  }
  drain ();
  g_assert_null (weak); g_assert_null (gtk_grab_get_current ());
  gtk_widget_destroy (window); drain ();
}
static void middle () { run_case ("neither-side", .40, .50, .40, .75); }
static void below () { run_case ("below", .10, .05, .20, .20); }
static void above () { run_case ("above", .10, .90, .20, .20); }
static void right () { run_case ("right-edge", .90, .50, .70, .20); }
static void oversized () { run_case ("oversized", .40, .50, 1.20, 1.20); }
static void preferred () { run_case ("preferred-not-window-size", .50, .50, .20, .20, true); }
static void reentry () { run_case ("destroy-on-map", .40, .50, .40, .75, false, true); }
static void outside () { run_case ("outside-cancel", .40, .50, .40, .75, false, false, true); }
static void repeat () { for (int i=0; i<4; ++i) run_case ("repeat", .40, .50, .40, .75); }
int main (int argc, char **argv)
{
  gtk_test_init (&argc, &argv, nullptr);
  g_test_add_func ("/popup-placement/neither-side", middle);
  g_test_add_func ("/popup-placement/below", below);
  g_test_add_func ("/popup-placement/above", above);
  g_test_add_func ("/popup-placement/right-edge", right);
  g_test_add_func ("/popup-placement/oversized", oversized);
  g_test_add_func ("/popup-placement/preferred-not-window-size", preferred);
  g_test_add_func ("/popup-placement/destroy-on-map", reentry);
  g_test_add_func ("/popup-placement/repeat", repeat);
  g_test_add_func ("/popup-placement/outside", outside);
  return g_test_run ();
}
