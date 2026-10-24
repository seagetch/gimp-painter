/* Actual canvas adapter with synthetic master-pointer events. Unlike the older
 * helper, this preserves subpixel device coordinates through the input path.
 * Forward placement uses the current display matrix; this is not an independent
 * old-transform oracle and does not stand in for physical tablet coverage. */
#include "display/gimpdisplayshell-scroll.h"
#include "display/gimppaintercanvasui.h"
static void editor_event (GdkEventType type, guint state, gdouble x, gdouble y, guint key)
{
  GdkEvent *event = gdk_event_new (type);
  gdouble dx = x * shell->scale_x - shell->offset_x;
  gdouble dy = y * shell->scale_y - shell->offset_y;
  if (shell->rotate_transform) cairo_matrix_transform_point (shell->rotate_transform, &dx, &dy);
  event->any.window = g_object_ref (gtk_widget_get_window (shell->canvas));
  event->any.send_event = TRUE;
  gdk_event_set_device (event, device); gdk_event_set_source_device (event, device);
  stamp += 10;
  if (type == GDK_BUTTON_PRESS || type == GDK_BUTTON_RELEASE)
    { event->button.button = 1; event->button.state = state; event->button.x = dx; event->button.y = dy; event->button.time = GDK_CURRENT_TIME; }
  else if (type == GDK_MOTION_NOTIFY)
    { event->motion.state = state; event->motion.x = dx; event->motion.y = dy; event->motion.time = stamp; }
  else
    { event->key.keyval = key; event->key.state = state; event->key.time = stamp; }
  gimp_display_shell_canvas_tool_events (shell->canvas, event, shell);
  gdk_event_free (event);
}
static void editor_click (guint state, gdouble x, gdouble y)
{
  editor_event (GDK_MOTION_NOTIFY, state, x, y, 0);
  editor_event (GDK_BUTTON_PRESS, state, x, y, 0);
  editor_event (GDK_BUTTON_RELEASE, state | GDK_BUTTON1_MASK, x, y, 0);
  /* Distinct modifier gestures include their real key release. Switching
   * directly from Ctrl to Shift in one synthetic state update is not a
   * physical release/press sequence and exercises a different latch policy. */
  if (state & GDK_SHIFT_MASK) editor_event (GDK_KEY_RELEASE, state, x, y, GDK_KEY_Shift_L);
  if (state & GDK_CONTROL_MASK) editor_event (GDK_KEY_RELEASE, state, x, y, GDK_KEY_Control_L);
  assert_reset ();
}
static void editor_point (guint index, gdouble ex, gdouble ey)
{
  gdouble x, y;
  g_assert_true (gimp_perspective_guide_get_vanish_points (gimp_image_get_perspective_guide (image), index, &x, &y));
  g_assert_cmpfloat_with_epsilon (x, ex, 1e-8);
  g_assert_cmpfloat_with_epsilon (y, ey, 1e-8);
}
static void editor_transformed_entry (void)
{
  const gdouble zooms[] = { .5, 1.25, 4 }, angles[] = { 0, 37, 90 };
  guint scenes = 0;
  create_image ();
  gimp_painter_canvas_ui_set_visible (shell, TRUE);
  g_assert_true (gimp_painter_canvas_ui_get_visible (shell));
  gimp_context_set_tool (gimp_get_user_context (gimp), gimp_get_tool_info (gimp, "gimp-perspective-guide-tool"));
  g_assert_true (active ()->disable_lazy_snap);
  gimp_display_shell_set_perspective_snap (shell, TRUE);
  for (guint z = 0; z < G_N_ELEMENTS (zooms); z++)
    for (guint a = 0; a < G_N_ELEMENTS (angles); a++)
      for (guint aniso = 0; aniso < 2; aniso++)
        for (guint flip = 0; flip < 4; flip++)
          {
            g_test_message ("scene %u z=%u a=%u aniso=%u flip=%u", scenes, z, a, aniso, flip);
            gimp_image_set_perspective_guide (image, NULL);
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
            editor_click (GDK_SHIFT_MASK, 80.125, 82.375);
            editor_click (GDK_SHIFT_MASK, 130.625, 140.875);
            editor_click (GDK_SHIFT_MASK, 180.375, 90.625);
            editor_click (GDK_SHIFT_MASK, 110.25, 105.75);
            g_assert_cmpint (gimp_perspective_guide_get_vanish_point_length (gimp_image_get_perspective_guide (image)), ==, 3);
            editor_point (0, 80.125, 82.375); editor_point (1, 130.625, 140.875); editor_point (2, 180.375, 90.625);
            editor_event (GDK_MOTION_NOTIFY, 0, 80.125, 82.375, 0);
            editor_event (GDK_BUTTON_PRESS, 0, 80.125, 82.375, 0);
            editor_event (GDK_MOTION_NOTIFY, GDK_BUTTON1_MASK, 87.625, 94.125, 0);
            gimp_motion_buffer_end_stroke (shell->motion_buffer);
            editor_event (GDK_BUTTON_RELEASE, GDK_BUTTON1_MASK, 87.625, 94.125, 0);
            editor_point (0, 87.625, 94.125); assert_reset ();
            editor_click (GDK_CONTROL_MASK, 130.625, 140.875);
            g_assert_cmpint (gimp_perspective_guide_get_vanish_point_length (gimp_image_get_perspective_guide (image)), ==, 2);
            editor_point (1, 180.375, 90.625);
            editor_click (GDK_CONTROL_MASK, 87.625, 94.125);
            editor_click (GDK_CONTROL_MASK, 180.375, 90.625);
            g_assert_null (gimp_image_get_perspective_guide (image));
            scenes++;
          }
  g_test_message ("synthetic master-pointer editor fractional add/max3/move/delete scenes=%u", scenes);
  g_assert_cmpuint (scenes, ==, 72);
  close_image ();
}

#include "libgimpmath/gimpmath.h"
#include "core/gimpimage-undo.h"
#include "../../migration/fixtures/legacy-perspective-editor/hit-oracle.inc"
static void editor_old_hit_policy (void)
{
  const gdouble zooms[] = { .5, 1.25, 4 }, angles[] = { 0, 37, 90 };
  const gdouble deltas[] = { -7.125, -6.125, -5.125, -4.125, 4.125, 5.125, 6.125, 7.125 };
  guint tested = 0, hits = 0, misses = 0; int old_tool = 1;
  create_image ();
  gimp_painter_canvas_ui_set_visible (shell, TRUE);
  gimp_context_set_tool (gimp_get_user_context (gimp), gimp_get_tool_info (gimp, "gimp-perspective-guide-tool"));
  for (guint z = 0; z < G_N_ELEMENTS (zooms); z++)
    for (guint a = 0; a < G_N_ELEMENTS (angles); a++)
      for (guint aniso = 0; aniso < 2; aniso++)
        for (guint flip = 0; flip < 4; flip++)
          {
            gimp_image_set_resolution (image, aniso ? 144 : 72, 72);
            gimp_display_shell_scale_set_dot_for_dot (shell, !aniso);
            gimp_display_shell_scale (shell, GIMP_ZOOM_TO, zooms[z], GIMP_ZOOM_FOCUS_IMAGE_CENTER);
            gimp_display_shell_flip (shell, flip & 1, flip & 2);
            gimp_display_shell_rotate_to (shell, angles[a]);
            gimp_display_shell_scroll_set_offset (shell,
              (gint) (128 * shell->scale_x) - shell->disp_width / 2,
              (gint) (128 * shell->scale_y) - shell->disp_height / 2);
            LegacyHitShell old = { shell->scale_x, shell->scale_y, shell->offset_x, shell->offset_y };
            for (guint axis = 0; axis < 2; axis++) for (guint d = 0; d < G_N_ELEMENTS (deltas); d++)
              {
                GimpPerspectiveGuide *model = gimp_perspective_guide_new (0);
                gimp_perspective_guide_add_vanish_points (model, 120.125, 130.375);
                gimp_image_set_perspective_guide (image, model); g_object_unref (model);
                gdouble x = 120.125 + (axis == 0 ? deltas[d] / shell->scale_x : 0);
                gdouble y = 130.375 + (axis == 1 ? deltas[d] / shell->scale_y : 0);
                gboolean expected = legacy_hit_on_handle (&old_tool, &old, x, y, GIMP_HANDLE_CIRCLE,
                                                         120.125, 130.375, 13, 13, GIMP_HANDLE_ANCHOR_CENTER);
                editor_click (GDK_CONTROL_MASK, x, y);
                g_assert_cmpint (gimp_image_get_perspective_guide (image) == NULL, ==, expected);
                if (expected) hits++; else misses++; tested++;
              }
          }
  g_assert_cmpuint (tested, ==, 1152); g_assert_cmpuint (hits, >, 0); g_assert_cmpuint (misses, >, 0);
  g_test_message ("unchanged old-source circle/center13 policy: %u hits %u misses, %u real native entry checks", hits, misses, tested);
  close_image ();
}
static void editor_cancel_reentry (void)
{
  create_image ();
  gimp_painter_canvas_ui_set_visible (shell, TRUE);
  gimp_context_set_tool (gimp_get_user_context (gimp), gimp_get_tool_info (gimp, "gimp-perspective-guide-tool"));
  gimp_display_shell_scale (shell, GIMP_ZOOM_TO, 1.25, GIMP_ZOOM_FOCUS_IMAGE_CENTER);
  gimp_display_shell_rotate_to (shell, 37); gimp_display_shell_flip (shell, TRUE, TRUE);
  editor_click (GDK_SHIFT_MASK, 110.125, 120.375);
  editor_event (GDK_MOTION_NOTIFY, 0, 110.125, 120.375, 0);
  editor_event (GDK_BUTTON_PRESS, 0, 110.125, 120.375, 0);
  editor_event (GDK_MOTION_NOTIFY, GDK_BUTTON1_MASK, 150.625, 160.875, 0);
  gimp_motion_buffer_end_stroke (shell->motion_buffer); editor_point (0, 150.625, 160.875);
  editor_event (GDK_KEY_PRESS, GDK_BUTTON1_MASK, 150.625, 160.875, GDK_KEY_Escape);
  editor_event (GDK_BUTTON_RELEASE, GDK_BUTTON1_MASK, 150.625, 160.875, 0);
  editor_point (0, 110.125, 120.375); assert_reset (); g_assert_null (shell->grab_seat);
  editor_event (GDK_MOTION_NOTIFY, 0, 110.125, 120.375, 0);
  editor_event (GDK_BUTTON_PRESS, 0, 110.125, 120.375, 0);
  editor_event (GDK_MOTION_NOTIFY, GDK_BUTTON1_MASK, 135.625, 146.875, 0);
  gimp_motion_buffer_end_stroke (shell->motion_buffer);
  editor_event (GDK_BUTTON_RELEASE, GDK_BUTTON1_MASK, 135.625, 146.875, 0);
  editor_point (0, 135.625, 146.875); assert_reset ();
  g_assert_true (gimp_image_undo (image)); editor_point (0, 110.125, 120.375);
  g_assert_true (gimp_image_redo (image)); editor_point (0, 135.625, 146.875);
  /* Real next entry must adopt the post-redo model, not a stale target. */
  editor_click (GDK_CONTROL_MASK, 135.625, 146.875);
  g_assert_null (gimp_image_get_perspective_guide (image)); assert_reset ();
  g_assert_cmpuint (shell->motion_buffer->event_queue->len, ==, 0);
  close_image ();
}
