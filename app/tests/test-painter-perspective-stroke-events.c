/* Reuse only the frozen real-event harness; do not register its existing13
 * cases twice or modify their immutable checkpoint source. */
int frozen_editor_main (int argc, char **argv);
#define main frozen_editor_main
#include "test-painter-perspective-events.c"
#undef main
#include "../../migration/fixtures/legacy-perspective-stroke/oracle.inc"

/* Feed the old arithmetic the exact current input-boundary image coordinates.
 * This intentionally shares native viewport conversion, not old matrix proof. */
static GimpCoords stroke_boundary_coords (gdouble x, gdouble y)
{
  GimpCoords display = { 0 }, result = { 0 };
  display.x = x * shell->scale_x - shell->offset_x;
  display.y = y * shell->scale_y - shell->offset_y;
  if (shell->rotate_transform) cairo_matrix_transform_point (shell->rotate_transform, &display.x, &display.y);
  gimp_display_shell_untransform_coords (shell, &display, &result);
  return result;
}
static void stroke_transformed_old_entry (void)
{
  const gdouble zooms[] = { .5, 1.25, 4 }, angles[] = { 0, 37, 90 };
  guint scenes = 0, strokes = 0;
  create_image ();
  gimp_painter_canvas_ui_set_visible (shell, TRUE);
  g_assert_true (gimp_painter_canvas_ui_get_visible (shell));
  gimp_display_shell_set_perspective_snap (shell, TRUE);
  gimp_tool_control_set_snap_to (active ()->control, FALSE);
  for (guint z = 0; z < G_N_ELEMENTS (zooms); z++)
    for (guint a = 0; a < G_N_ELEMENTS (angles); a++)
      for (guint aniso = 0; aniso < 2; aniso++)
        for (guint flip = 0; flip < 4; flip++)
          for (guint count = 1; count <= 3; count++)
            {
              guide (count);
              gimp_image_set_resolution (image, aniso ? 144 : 72, 72);
              gimp_display_shell_scale_set_dot_for_dot (shell, !aniso);
              gimp_display_shell_scale (shell, GIMP_ZOOM_TO, zooms[z], GIMP_ZOOM_FOCUS_IMAGE_CENTER);
              gimp_display_shell_flip (shell, flip & 1, flip & 2);
              gimp_display_shell_rotate_to (shell, angles[a]);
              gimp_display_shell_scroll_set_offset (shell,
                (gint) (128 * shell->scale_x) - shell->disp_width / 2,
                (gint) (128 * shell->scale_y) - shell->disp_height / 2);
              g_assert_true (gimp_display_shell_painter_pan_begin (shell, shell->disp_width / 2, shell->disp_height / 2));
              gimp_display_shell_painter_pan_motion (shell, shell->disp_width / 2 + .375, shell->disp_height / 2 - .625);
              for (guint axis = 0; axis < 2; axis++)
                {
                  gdouble ox = 80.125 + axis * 20, oy = 90.375 + axis * 30;
                  gdouble dx = axis ? 0 : 1 / shell->scale_x, dy = axis ? 1 / shell->scale_y : 0;
                  LegacyStrokeShell old = { 0 }; GimpCoords sample;
                  editor_event (GDK_MOTION_NOTIFY, 0, ox, oy, 0); counters ();
                  editor_event (GDK_BUTTON_PRESS, 0, ox, oy, 0);
                  g_assert_true (shell->perspective_pending); g_assert_cmpuint (presses, ==, 0);
                  old.scale_x = shell->scale_x; old.scale_y = shell->scale_y;
                  old.snap_origin = shell->perspective_origin; old.snapping = TRUE;
                  sample = stroke_boundary_coords (ox + 31.999 * dx, oy + 31.999 * dy);
                  g_assert_false (legacy_stroke_lazy_snap (&old, image, &sample));
                  editor_event (GDK_MOTION_NOTIFY, GDK_BUTTON1_MASK, ox + 31.999 * dx, oy + 31.999 * dy, 0);
                  g_assert_true (shell->perspective_pending); g_assert_cmpuint (presses, ==, 0);
                  sample = stroke_boundary_coords (ox + 32.001 * dx, oy + 32.001 * dy);
                  g_assert_true (legacy_stroke_lazy_snap (&old, image, &sample));
                  editor_event (GDK_MOTION_NOTIFY, GDK_BUTTON1_MASK, ox + 32.001 * dx, oy + 32.001 * dy, 0);
                  g_assert_false (shell->perspective_pending); g_assert_true (shell->perspective_locked);
                  g_assert_cmpuint (presses, ==, 1); g_assert_cmpuint (n_motion, ==, 0);
                  g_assert_cmpfloat_with_epsilon (shell->perspective_angle, old.snapped_angle, 1e-12);
                  g_assert_cmpfloat_with_epsilon (pressed.x, old.snap_origin.x, 1e-8);
                  g_assert_cmpfloat_with_epsilon (pressed.y, old.snap_origin.y, 1e-8);
                  old.snapping = FALSE;
                  sample = stroke_boundary_coords (ox + 55.625, oy + 42.875);
                  g_assert_true (legacy_stroke_lazy_snap (&old, image, &sample));
                  editor_event (GDK_MOTION_NOTIFY, GDK_BUTTON1_MASK, ox + 55.625, oy + 42.875, 0);
                  gimp_motion_buffer_end_stroke (shell->motion_buffer);
                  g_assert_cmpuint (n_motion, >, 0);
                  g_assert_cmpfloat_with_epsilon (motions[n_motion - 1].x, sample.x, 1e-8);
                  g_assert_cmpfloat_with_epsilon (motions[n_motion - 1].y, sample.y, 1e-8);
                  editor_event (GDK_BUTTON_RELEASE, GDK_BUTTON1_MASK, ox + 55.625, oy + 42.875, 0);
                  g_assert_cmpuint (releases, ==, 1); assert_reset (); g_assert_null (shell->grab_seat);
                  g_assert_cmpuint (shell->motion_buffer->event_queue->len, ==, 0); strokes++;
                }
              scenes++;
            }
  g_test_message ("old source snap+projection through native input: %u transformed scenes, %u independent consecutive strokes", scenes, strokes);
  g_assert_cmpuint (scenes, ==, 216); g_assert_cmpuint (strokes, ==, 432);
  close_image ();
}
static void stroke_exact_threshold_tie (void)
{
  LegacyStrokeShell old = { 0 }; GimpCoords sample;
  create_image (); guide (1);
  gimp_painter_canvas_ui_set_visible (shell, TRUE);
  gimp_display_shell_set_perspective_snap (shell, TRUE);
  gimp_tool_control_set_snap_to (active ()->control, FALSE);
  gimp_image_set_resolution (image, 72, 72); gimp_display_shell_scale_set_dot_for_dot (shell, TRUE);
  gimp_display_shell_scale (shell, GIMP_ZOOM_TO, 1, GIMP_ZOOM_FOCUS_IMAGE_CENTER);
  gimp_display_shell_rotate_to (shell, 0); gimp_display_shell_flip (shell, FALSE, FALSE);
  gimp_display_shell_scroll_set_offset (shell, 128 - shell->disp_width / 2, 128 - shell->disp_height / 2);
  old.scale_x = old.scale_y = 1;
  editor_event (GDK_MOTION_NOTIFY, 0, 128, 128, 0); counters ();
  editor_event (GDK_BUTTON_PRESS, 0, 128, 128, 0);
  old.snap_origin = shell->perspective_origin; old.snapping = TRUE;
  sample = stroke_boundary_coords (159.999, 128);
  g_assert_false (legacy_stroke_lazy_snap (&old, image, &sample));
  editor_event (GDK_MOTION_NOTIFY, GDK_BUTTON1_MASK, 159.999, 128, 0);
  g_assert_cmpuint (presses, ==, 0); g_assert_true (shell->perspective_pending);
  sample = stroke_boundary_coords (160, 128);
  g_assert_true (legacy_stroke_lazy_snap (&old, image, &sample));
  editor_event (GDK_MOTION_NOTIFY, GDK_BUTTON1_MASK, 160, 128, 0);
  g_assert_cmpuint (presses, ==, 1); g_assert_cmpuint (n_motion, ==, 0);
  g_assert_cmpfloat (shell->perspective_angle, ==, old.snapped_angle);
  editor_event (GDK_BUTTON_RELEASE, GDK_BUTTON1_MASK, 160, 128, 0); assert_reset ();
  editor_event (GDK_MOTION_NOTIFY, 0, 128, 128, 0); counters ();
  editor_event (GDK_BUTTON_PRESS, 0, 128, 128, 0);
  old.snap_origin = shell->perspective_origin; old.snapping = TRUE;
  sample = stroke_boundary_coords (160, 96);
  g_assert_true (legacy_stroke_lazy_snap (&old, image, &sample));
  g_assert_cmpfloat (old.snapped_angle, ==, 0); /* Exact45degree tie: first horizontal. */
  editor_event (GDK_MOTION_NOTIFY, GDK_BUTTON1_MASK, 160, 96, 0);
  g_assert_cmpuint (presses, ==, 1); g_assert_cmpfloat (shell->perspective_angle, ==, 0);
  old.snapping = FALSE; sample = stroke_boundary_coords (180, 170);
  g_assert_true (legacy_stroke_lazy_snap (&old, image, &sample));
  editor_event (GDK_MOTION_NOTIFY, GDK_BUTTON1_MASK, 180, 170, 0);
  gimp_motion_buffer_end_stroke (shell->motion_buffer);
  g_assert_cmpuint (n_motion, >, 0);
  g_assert_cmpfloat_with_epsilon (motions[n_motion - 1].x, sample.x, 1e-8);
  g_assert_cmpfloat_with_epsilon (motions[n_motion - 1].y, sample.y, 1e-8);
  editor_event (GDK_BUTTON_RELEASE, GDK_BUTTON1_MASK, 180, 170, 0); assert_reset ();
  /* A short third stroke cannot inherit the previous locked direction. */
  editor_event (GDK_MOTION_NOTIFY, 0, 80.125, 90.375, 0); counters ();
  editor_event (GDK_BUTTON_PRESS, 0, 80.125, 90.375, 0);
  editor_event (GDK_MOTION_NOTIFY, GDK_BUTTON1_MASK, 90.125, 99.375, 0);
  g_assert_true (shell->perspective_pending); g_assert_false (shell->perspective_locked);
  editor_event (GDK_BUTTON_RELEASE, GDK_BUTTON1_MASK, 90.125, 99.375, 0);
  g_assert_cmpuint (presses, ==, 0); g_assert_cmpuint (n_motion, ==, 0); assert_reset ();
  g_assert_null (shell->grab_seat); g_assert_cmpuint (shell->motion_buffer->event_queue->len, ==, 0);
  g_test_message ("native exact32 threshold, first-horizontal45degree tie, dropped crossing sample, short next stroke reset");
  close_image ();
}
int main (int argc, char **argv)
{
  gint result;
  g_test_init (&argc, &argv, NULL);
  if (!gtk_init_check (&argc, &argv)) return GIMP_EXIT_TEST_SKIPPED;
  gimp_test_utils_setup_menus_path (); gimp = gimp_init_for_gui_testing (TRUE);
  g_test_add_func ("/perspective-stroke/old-transformed-entry", stroke_transformed_old_entry);
  g_test_add_func ("/perspective-stroke/exact-threshold-tie-reset", stroke_exact_threshold_tie);
  g_application_run (gimp->app, 0, NULL);
  result = gimp_core_app_get_exit_status (GIMP_CORE_APP (gimp->app));
  g_application_quit (G_APPLICATION (gimp->app)); g_clear_object (&gimp->app);
  return result;
}
