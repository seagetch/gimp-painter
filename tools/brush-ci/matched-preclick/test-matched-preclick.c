/* SPDX-License-Identifier: GPL-3.0-or-later
 * Nonacceptance matched pre-click measurement. No popup is opened. */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include "config.h"
#include <time.h>
#include <gtk/gtk.h>
#include <gegl.h>
#include "libgimpbase/gimpbase.h"
#include "libgimpconfig/gimpconfig.h"
#include "libgimpwidgets/gimpwidgets.h"
#include "core/core-types.h"
#include "core/gimp.h"
#include "core/gimpcontainer.h"
#include "core/gimpdatafactory.h"
#include "core/gimpcontext.h"
#include "core/gimptoolinfo.h"
#include "core/gimptooloptions.h"
#include "core/gimpbrush.h"
#include "core/gimpbrushgenerated.h"
#include "widgets/widgets-types.h"
#include "widgets/gimpview.h"
#include "widgets/gimpviewablebutton.h"
#include "gimpcoreapp.h"
#include "gimp-app-test-utils.h"
#include "tests.h"

#include "matched-preclick-phases.inc"

static Gimp *gimp;
static GtkWidget *harness_window;

typedef struct {
  GtkWidget *button;
  GtkAllocation allocation;
  gint left, top;
  guint frames, stable, changes;
  gint64 started, first_frame, stable_at;
  struct { gint64 elapsed; guint stable; } observations[64];
} Observation;

static gint64
thread_cpu_us (void)
{
  struct timespec ts;
  g_assert_cmpint (clock_gettime (CLOCK_THREAD_CPUTIME_ID, &ts), ==, 0);
  return ((gint64) ts.tv_sec) * G_USEC_PER_SEC + ts.tv_nsec / 1000;
}

static void
after_paint (GdkFrameClock *clock, gpointer data)
{
  Observation *o = data;
  GtkAllocation allocation;
  gint left, top;
  gtk_widget_get_allocation (o->button, &allocation);
  gdk_window_get_origin (gtk_widget_get_window (o->button), &left, &top);
  if (o->frames && left == o->left && top == o->top &&
      allocation.x == o->allocation.x && allocation.y == o->allocation.y &&
      allocation.width == o->allocation.width && allocation.height == o->allocation.height)
    ++o->stable;
  else {
    if (o->frames) ++o->changes;
    o->stable = 0;
  }
  o->allocation = allocation; o->left = left; o->top = top;
  gint64 elapsed = g_get_monotonic_time () - o->started;
  if (!o->frames) o->first_frame = elapsed;
  if (o->stable >= 3 && !o->stable_at) o->stable_at = elapsed;
  if (o->frames < G_N_ELEMENTS (o->observations)) {
    o->observations[o->frames].elapsed = elapsed;
    o->observations[o->frames].stable = o->stable;
  }
  ++o->frames;
  if (o->stable < 3) {
    gtk_widget_queue_draw (o->button);
    gdk_frame_clock_request_phase (clock, GDK_FRAME_CLOCK_PHASE_AFTER_PAINT);
  }
}

static void gather (GtkWidget *w, GPtrArray *result) {
  if (g_ptr_array_find (result,w,NULL)) return;
  g_ptr_array_add (result,w);
  if (GTK_IS_MENU_BUTTON(w)) {
    GtkPopover *p=gtk_menu_button_get_popover(GTK_MENU_BUTTON(w)); if(p)gather(GTK_WIDGET(p),result);
  }
  if(GTK_IS_FRAME(w)) { GtkWidget *label=gtk_frame_get_label_widget(GTK_FRAME(w));if(label)gather(label,result); }
  if(GTK_IS_CONTAINER(w)) {
    GList *children=gtk_container_get_children(GTK_CONTAINER(w));
    for(GList *i=children;i;i=i->next)gather(i->data,result);g_list_free(children);
  }
}
static GtkWidget *
first_view (GtkWidget *button)
{
  GtkWidget *view = NULL;
  GPtrArray *targets = g_ptr_array_new ();
  gather (button, targets);
  for (guint i = 0; i < targets->len && !view; ++i)
    if (GIMP_IS_VIEW (g_ptr_array_index (targets, i))) view = g_ptr_array_index (targets, i);
  g_ptr_array_unref (targets);
  return view;
}

static GtkWidget *
thumbnail (GtkWidget *toolbar)
{
  GtkWidget *button = NULL;
  GimpContainer *brushes = gimp_data_factory_get_container (gimp->brush_factory);
  GList *children = gtk_container_get_children (GTK_CONTAINER (toolbar));
  for (GList *i = children; i; i = i->next) {
    GtkWidget *widget = i->data;
    /* Exact existing case14 fallback: works on e25 stock and new thumbnail. */
    if (!g_strcmp0 (gtk_widget_get_name (widget), "painter-paint-brush-thumbnail") ||
        (GIMP_IS_VIEWABLE_BUTTON (widget) && GIMP_VIEWABLE_BUTTON (widget)->container == brushes)) {
      g_assert_null (button); button = widget;
    }
  }
  g_list_free (children); g_assert_nonnull (button);
  return button;
}

static void
finalized (gpointer data, GObject *object)
{
  (void) object; ++*(guint *)data;
}

static void
measure (GtkWidget *button, gboolean extended, gint64 setup_us,
         gint64 factory_us, gint64 show_drain_us)
{
  Observation o = {0}; o.button = button;
  gboolean mapped = gtk_widget_get_mapped (button);
  gboolean realized = gtk_widget_get_realized (button);
  GdkWindow *initial_window = gtk_widget_get_window (button);
  gboolean viewable = initial_window && gdk_window_is_viewable (initial_window);
  o.started = g_get_monotonic_time ();
  gint64 cpu_started = thread_cpu_us ();
  GdkFrameClock *clock = gtk_widget_get_frame_clock (button);
  g_assert_nonnull (clock);
  TransformPopupTiming timing = {0}; timing.started = o.started;
  GtkWidget *view = first_view (button);
  transform_popup_timing_connect (&timing, 0, G_OBJECT (gtk_widget_get_toplevel (button)), "toplevel-draw", "draw", TRUE);
  transform_popup_timing_connect (&timing, 1, view ? G_OBJECT (view) : NULL, "target-view-draw", "draw", TRUE);
  transform_popup_timing_connect (&timing, 2, G_OBJECT (clock), "frame-update", "update", FALSE);
  transform_popup_timing_connect (&timing, 3, G_OBJECT (clock), "frame-layout", "layout", FALSE);
  transform_popup_timing_connect (&timing, 4, G_OBJECT (clock), "frame-paint", "paint", FALSE);
  gulong handler = g_signal_connect (clock, "after-paint", G_CALLBACK (after_paint), &o);
  gdk_frame_clock_begin_updating (clock);
  gtk_widget_queue_draw (button);
  gdk_frame_clock_request_phase (clock, GDK_FRAME_CLOCK_PHASE_AFTER_PAINT);
  gint64 deadline = g_get_monotonic_time () + 3 * G_TIME_SPAN_SECOND;
  gint64 longest = 0;
  guint iterations = 0;
  /* EXACT original primary loop/predicate, including post-iteration sleep.
   * A single dispatch may overshoot the deadline; record, do not hide it. */
  while (o.stable < 3 && g_get_monotonic_time () < deadline) {
    gint64 began = g_get_monotonic_time ();
    g_main_context_iteration (NULL, FALSE);
    longest = MAX (longest, g_get_monotonic_time () - began);
    ++iterations; g_usleep (1000);
  }
  gint64 primary_elapsed = g_get_monotonic_time () - o.started;
  gint64 primary_cpu = thread_cpu_us () - cpu_started;
  guint primary_stable = o.stable, primary_frames = o.frames;
  gboolean primary_pass = o.stable >= 3;
  /* Optional separate measurement only. Never upgrades primary_pass.
   * Absolute 30s observation cap is not an accepted deadline; dispatch cannot
   * be preempted here. Meson/runner supplies the external process timeout. */
  if (extended && !primary_pass) {
    while (o.stable < 3 && g_get_monotonic_time () < o.started + 30 * G_TIME_SPAN_SECOND) {
      g_main_context_iteration (NULL, FALSE); g_usleep (1000);
    }
  }
  gint64 final_elapsed = g_get_monotonic_time () - o.started;
  gdk_frame_clock_end_updating (clock);
  g_signal_handler_disconnect (clock, handler);
  transform_popup_timing_finish (&timing, 1, TRUE);
  g_test_message ("MATCHED_PRECLICK acceptance_eligible=0 extended=%d primary_pass=%d primary_elapsed_us=%" G_GINT64_FORMAT " primary_cpu_us=%" G_GINT64_FORMAT " primary_frames=%u primary_stable=%u primary_iterations=%u longest_iteration_us=%" G_GINT64_FORMAT " first_frame_us=%" G_GINT64_FORMAT " stable_at_us=%" G_GINT64_FORMAT " final_elapsed_us=%" G_GINT64_FORMAT " final_frames=%u final_stable=%u geometry_changes=%u initial_mapped=%d initial_realized=%d initial_viewable=%d setup_us=%" G_GINT64_FORMAT " factory_us=%" G_GINT64_FORMAT " show_drain_us=%" G_GINT64_FORMAT,
    extended, primary_pass, primary_elapsed, primary_cpu, primary_frames, primary_stable,
    iterations, longest, o.first_frame, o.stable_at, final_elapsed, o.frames, o.stable,
    o.changes, mapped, realized, viewable, setup_us, factory_us, show_drain_us);
  for (guint i = 0; i < MIN(o.frames, G_N_ELEMENTS(o.observations)); ++i)
    g_test_message ("MATCHED_FRAME ordinal=%u elapsed_us=%" G_GINT64_FORMAT " stable=%u", i+1, o.observations[i].elapsed, o.observations[i].stable);
  g_test_message ("MATCHED_FRAMES truncated=%d", o.frames > G_N_ELEMENTS(o.observations));
  /* Exit0 means measurement executed, not acceptance. No frame is invalid. */
  g_assert_cmpuint (o.frames, >, 0);
}

static void
preclick (gconstpointer data)
{
  gint64 setup_started = g_get_monotonic_time ();
  GimpToolInfo *tool = GIMP_TOOL_INFO (gimp_container_get_child_by_name (gimp->tool_info_list, "gimp-paintbrush-tool"));
  g_assert_nonnull (tool);
  GimpToolOptions *options = g_object_new (G_OBJECT_TYPE (tool->tool_options), "gimp", gimp, "tool-info", tool, NULL);
  gimp_context_copy_properties (GIMP_CONTEXT (tool->tool_options), GIMP_CONTEXT (options), GIMP_CONTEXT_PROP_MASK_ALL);
  g_assert_null (GIMP_CONTEXT (options)->parent);
  GimpBrush *a = GIMP_BRUSH (gimp_brush_generated_new ("Private unregistered A", GIMP_BRUSH_GENERATED_CIRCLE, 12, 2, 0.5, 1, 0));
  GimpBrush *b = GIMP_BRUSH (gimp_brush_generated_new ("Private unregistered B", GIMP_BRUSH_GENERATED_CIRCLE, 18, 2, 0.5, 1, 0));
  guint a_dead = 0, b_dead = 0, model_dead = 0;
  g_object_weak_ref (G_OBJECT (a), finalized, &a_dead);
  g_object_weak_ref (G_OBJECT (b), finalized, &b_dead);
  g_object_weak_ref (G_OBJECT (options), finalized, &model_dead);
  gimp_context_set_brush (GIMP_CONTEXT (options), a);
  GimpToolOptionsGUIFunc factory = g_object_get_data (G_OBJECT (tool), "gimp-tool-options-gui-horizontal-func");
  g_assert_nonnull (factory);
  gint64 factory_started = g_get_monotonic_time ();
  GtkWidget *toolbar = factory (options); g_assert_nonnull (toolbar);
  gint64 factory_us = g_get_monotonic_time () - factory_started;
  gint64 show_started = g_get_monotonic_time ();
  GtkWidget *window = gtk_window_new (GTK_WINDOW_TOPLEVEL); g_object_ref_sink (window);
  gtk_container_add (GTK_CONTAINER (window), toolbar); gtk_widget_show_all (window);
  gimp_test_run_mainloop_until_idle ();
  gint64 show_us = g_get_monotonic_time () - show_started;
  GtkWidget *button = thumbnail (toolbar);
  measure (button, GPOINTER_TO_INT (data), g_get_monotonic_time () - setup_started, factory_us, show_us);
  /* Cleanup is outside the measured pre-click operation. No popup was opened. */
  gtk_widget_destroy (window); g_object_unref (window);
  g_object_unref (options); g_object_unref (a); g_object_unref (b);
  gimp_test_run_mainloop_until_idle ();
  if (!a_dead) g_object_weak_unref (G_OBJECT (a), finalized, &a_dead);
  if (!b_dead) g_object_weak_unref (G_OBJECT (b), finalized, &b_dead);
  if (!model_dead) g_object_weak_unref (G_OBJECT (options), finalized, &model_dead);
}

int main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);
  if (!gtk_init_check (&argc, &argv)) return GIMP_EXIT_TEST_SKIPPED;
  gimp_test_utils_setup_menus_path ();
  gimp = gimp_init_for_gui_testing (TRUE);
  harness_window = gtk_window_new (GTK_WINDOW_TOPLEVEL); g_object_ref_sink (harness_window);
  gtk_window_set_default_size (GTK_WINDOW (harness_window), 1100, 130);
  g_test_add_data_func ("/matched-preclick/primary", GINT_TO_POINTER (0), preclick);
  g_test_add_data_func ("/matched-preclick/extended", GINT_TO_POINTER (1), preclick);
  g_application_run (gimp->app, 0, NULL);
  int result = gimp_core_app_get_exit_status (GIMP_CORE_APP (gimp->app));
  gtk_widget_destroy (harness_window); g_object_unref (harness_window);
  g_application_quit (G_APPLICATION (gimp->app)); g_clear_object (&gimp->app);
  return result;
}
