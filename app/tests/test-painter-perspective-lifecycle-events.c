/* Actual native event lifecycle. Frozen editor source is included but its13
 * cases are not registered twice. No old binary or physical-device claim. */
int frozen_lifecycle_editor_main (int argc, char **argv);
#define main frozen_lifecycle_editor_main
#include "test-painter-perspective-events.c"
#undef main
#include "core/gimpimage-duplicate.h"
#include "core/gimpundostack.h"
#include "display/gimpcanvasitem.h"
#include "tools/gimpdrawtool.h"
#include "xcf/xcf.h"

static void lifecycle_select_editor (void)
{
  gimp_painter_canvas_ui_set_visible (shell, TRUE);
  g_assert_true (gimp_painter_canvas_ui_get_visible (shell));
  gtk_window_resize (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (shell))), 1040, 1800);
  gimp_test_run_temp_mainloop (800);
  gimp_context_set_tool (gimp_get_user_context (gimp), gimp_get_tool_info (gimp, "gimp-perspective-guide-tool"));
  gimp_image_set_resolution (image, 144, 72); gimp_display_shell_scale_set_dot_for_dot (shell, FALSE);
  gimp_display_shell_scale (shell, GIMP_ZOOM_TO, 1.25, GIMP_ZOOM_FOCUS_IMAGE_CENTER);
  gimp_display_shell_rotate_to (shell, 37); gimp_display_shell_flip (shell, TRUE, TRUE);
}
static void lifecycle_move (gdouble x, gdouble y, gdouble nx, gdouble ny)
{
  editor_event (GDK_MOTION_NOTIFY, 0, x, y, 0);
  editor_event (GDK_BUTTON_PRESS, 0, x, y, 0);
  editor_event (GDK_MOTION_NOTIFY, GDK_BUTTON1_MASK, nx, ny, 0);
  gimp_motion_buffer_end_stroke (shell->motion_buffer);
  editor_event (GDK_BUTTON_RELEASE, GDK_BUTTON1_MASK, nx, ny, 0); assert_reset ();
}
static void lifecycle_mark_clean (void)
{
  /* Establish a valid clean exported-document context; this is metadata setup,
   * not a claim that a PNG export was performed. */
  gchar *path = g_build_filename (g_get_tmp_dir (), "synthetic-known-export.png", NULL);
  GFile *file = g_file_new_for_path (path);
  gimp_image_set_exported_file (image, file);
  gimp_image_clean_all (image); gimp_image_export_clean_all (image);
  g_object_unref (file); g_free (path);
}
static void lifecycle_clean (void)
{
  g_assert_cmpint (gimp_image_is_dirty (image), ==, 0);
  g_assert_false (gimp_image_is_export_dirty (image));
}
static void lifecycle_capture (const gchar *label)
{
  const gchar *directory = g_getenv ("GIMP_TEST_PERSPECTIVE_LIFECYCLE_SCREENSHOT_DIR");
  if (directory)
    {
      GtkWidget *window = gtk_widget_get_toplevel (GTK_WIDGET (shell));
      gchar *name = g_strconcat (label, ".png", NULL), *path = g_build_filename (directory, name, NULL);
      GdkPixbuf *pixels;
      g_assert_true (gimp_painter_canvas_ui_get_visible (shell));
      gimp_test_run_temp_mainloop (200); gdk_display_sync (gtk_widget_get_display (window));
      pixels = gdk_pixbuf_get_from_window (gtk_widget_get_window (window), 0, 0,
        gtk_widget_get_allocated_width (window), gtk_widget_get_allocated_height (window));
      g_assert_nonnull (pixels); g_assert_true (gdk_pixbuf_save (pixels, path, "png", NULL, NULL));
      g_object_unref (pixels); g_free (path); g_free (name);
    }
}
static void lifecycle_undo_all_edits (void)
{
  gint depth;
  create_image (); lifecycle_select_editor (); lifecycle_mark_clean (); lifecycle_clean ();
  depth = gimp_undo_stack_get_depth (gimp_image_get_undo_stack (image));
  editor_click (GDK_SHIFT_MASK, 80.125, 90.375); editor_point (0, 80.125, 90.375);
  g_assert_cmpint (gimp_undo_stack_get_depth (gimp_image_get_undo_stack (image)), ==, depth + 1); lifecycle_clean ();
  g_assert_true (gimp_image_undo (image)); g_assert_null (gimp_image_get_perspective_guide (image)); lifecycle_clean ();
  g_assert_true (gimp_image_redo (image)); editor_point (0, 80.125, 90.375); lifecycle_clean ();
  lifecycle_move (80.125, 90.375, 100.625, 115.875); editor_point (0, 100.625, 115.875);
  g_assert_cmpint (gimp_undo_stack_get_depth (gimp_image_get_undo_stack (image)), ==, depth + 2);
  g_assert_true (gimp_image_undo (image)); editor_point (0, 80.125, 90.375);
  g_assert_true (gimp_image_redo (image)); editor_point (0, 100.625, 115.875); lifecycle_clean ();
  editor_click (GDK_SHIFT_MASK, 140.125, 150.375);
  editor_click (GDK_SHIFT_MASK, 180.625, 120.875);
  depth = gimp_undo_stack_get_depth (gimp_image_get_undo_stack (image));
  editor_click (GDK_SHIFT_MASK, 200.125, 180.375);
  g_assert_cmpint (gimp_undo_stack_get_depth (gimp_image_get_undo_stack (image)), ==, depth);
  lifecycle_capture ("before-delete-three-points");
  editor_click (GDK_CONTROL_MASK, 140.125, 150.375); editor_point (1, 180.625, 120.875);
  lifecycle_capture ("after-delete-two-points");
  g_assert_true (gimp_image_undo (image)); editor_point (1, 140.125, 150.375); editor_point (2, 180.625, 120.875);
  lifecycle_capture ("undo-restored-three-points");
  g_assert_true (gimp_image_redo (image)); editor_point (1, 180.625, 120.875); lifecycle_clean ();
  editor_click (GDK_CONTROL_MASK, 100.625, 115.875);
  editor_click (GDK_CONTROL_MASK, 180.625, 120.875); g_assert_null (gimp_image_get_perspective_guide (image));
  g_assert_true (gimp_image_undo (image)); editor_point (0, 180.625, 120.875);
  g_assert_true (gimp_image_redo (image)); g_assert_null (gimp_image_get_perspective_guide (image)); lifecycle_clean ();
  g_test_message ("native add/move/middle-delete/last-delete UndoRedo; no-op max3 adds no Undo; session-only edits stay XCF/export clean");
  close_image ();
}
static void lifecycle_xcf_nonpersistent (void)
{
  GOutputStream *output; GInputStream *input; GimpImage *loaded, *duplicate;
  GimpPerspectiveGuide *live; GError *error = NULL; GBytes *bytes; gsize size; GimpTattoo before_duplicate;
  create_image (); lifecycle_select_editor (); lifecycle_mark_clean ();
  editor_click (GDK_SHIFT_MASK, 80.125, 90.375);
  editor_click (GDK_SHIFT_MASK, 140.125, 150.375);
  editor_click (GDK_SHIFT_MASK, 180.625, 120.875);
  live = g_object_ref (gimp_image_get_perspective_guide (image)); lifecycle_clean ();
  output = g_memory_output_stream_new_resizable ();
  g_assert_true (xcf_save_stream (gimp, image, output, NULL, NULL, &error)); g_assert_no_error (error);
  g_assert_true (g_output_stream_is_closed (output));
  size = g_memory_output_stream_get_data_size (G_MEMORY_OUTPUT_STREAM (output)); g_assert_cmpuint (size, >, 20);
  bytes = g_bytes_new (g_memory_output_stream_get_data (G_MEMORY_OUTPUT_STREAM (output)), size);
  input = g_memory_input_stream_new_from_bytes (bytes);
  loaded = xcf_load_stream (gimp, input, NULL, NULL, &error); g_assert_no_error (error); g_assert_nonnull (loaded);
  g_assert_null (gimp_image_get_perspective_guide (loaded));
  g_assert_cmpint (gimp_image_get_width (loaded), ==, 256); g_assert_cmpint (gimp_image_get_height (loaded), ==, 256);
  g_assert_cmpint (gimp_container_get_n_children (gimp_image_get_layers (loaded)), ==,
                   gimp_container_get_n_children (gimp_image_get_layers (image)));
  g_assert_true (gimp_image_get_perspective_guide (image) == live);
  {
    GOutputStream *without = g_memory_output_stream_new_resizable ();
    GOutputStream *control = g_memory_output_stream_new_resizable ();
    GBytes *without_bytes, *control_bytes;
    const gchar *directory = g_getenv ("GIMP_TEST_PERSPECTIVE_LIFECYCLE_EVIDENCE_DIR");
    g_assert_true (xcf_save_stream (gimp, image, control, NULL, NULL, &error)); g_assert_no_error (error);
    control_bytes = g_bytes_new (g_memory_output_stream_get_data (G_MEMORY_OUTPUT_STREAM (control)),
                                 g_memory_output_stream_get_data_size (G_MEMORY_OUTPUT_STREAM (control)));
    /* Controlled second save proves absence in writer bytes, not merely a
     * reader that might discard an unknown guide record. Restore live state. */
    gimp_image_set_perspective_guide (image, NULL);
    g_assert_true (xcf_save_stream (gimp, image, without, NULL, NULL, &error)); g_assert_no_error (error);
    without_bytes = g_bytes_new (g_memory_output_stream_get_data (G_MEMORY_OUTPUT_STREAM (without)),
                                 g_memory_output_stream_get_data_size (G_MEMORY_OUTPUT_STREAM (without)));
    if (directory)
      {
        const gchar *names[] = { "with-guide-first.xcf", "with-guide-control.xcf", "without-guide.xcf" };
        GBytes *streams[] = { bytes, control_bytes, without_bytes };
        for (guint i = 0; i < 3; i++)
          {
            gchar *path = g_build_filename (directory, names[i], NULL); gsize length;
            const gchar *data = g_bytes_get_data (streams[i], &length);
            g_assert_true (g_file_set_contents (path, data, length, NULL)); g_free (path);
          }
      }
    g_test_message ("writer comparison first/control=%d control/without-guide=%d", g_bytes_equal (bytes, control_bytes), g_bytes_equal (control_bytes, without_bytes));
    g_assert_true (g_bytes_equal (bytes, control_bytes));
    g_assert_true (g_bytes_equal (control_bytes, without_bytes));
    gimp_image_set_perspective_guide (image, live);
    g_bytes_unref (without_bytes); g_bytes_unref (control_bytes); g_object_unref (without); g_object_unref (control);
  }
  g_assert_true (gimp_image_get_perspective_guide (image) == live);
  /* Image duplication allocates ordinary item tattoos on the original before
   * transfer to its new owner. Keep that independent metadata operation AFTER
   * the controlled writer comparison; do not mask tattoo differences. */
  before_duplicate = gimp_image_get_tattoo_state (image);
  duplicate = gimp_image_duplicate (image); g_assert_nonnull (duplicate);
  g_assert_null (gimp_image_get_perspective_guide (duplicate));
  g_test_message ("diagnostic original tattoo counter around image duplicate: %u -> %u", before_duplicate, gimp_image_get_tattoo_state (image));
  editor_point (0, 80.125, 90.375); editor_point (1, 140.125, 150.375); editor_point (2, 180.625, 120.875); lifecycle_clean ();
  g_test_message ("native XCF stream Save/Open and image duplicate omit session-only guide; writer bytes identical with/without guide; live3-point model restored unchanged (%" G_GSIZE_FORMAT " bytes)", size);
  g_object_unref (duplicate); g_object_unref (loaded); g_object_unref (input); g_bytes_unref (bytes); g_object_unref (output); g_object_unref (live);
  close_image ();
}
static void lifecycle_switch_close (void)
{
  GimpImage *other; GimpPerspectiveGuide *old, *new_model; GimpTool *editor; GimpDisplay *display;
  cairo_region_t *before, *after; gdouble x, y;
  create_image (); lifecycle_select_editor ();
  editor_click (GDK_SHIFT_MASK, 80.125, 90.375);
  old = g_object_ref (gimp_image_get_perspective_guide (image));
  other = gimp_image_new (gimp, 256, 256, GIMP_RGB, GIMP_PRECISION_U8_NON_LINEAR);
  new_model = gimp_perspective_guide_new (0); gimp_perspective_guide_add_vanish_points (new_model, 140.125, 150.375);
  gimp_image_set_perspective_guide (other, new_model);
  editor_event (GDK_MOTION_NOTIFY, 0, 80.125, 90.375, 0); editor_event (GDK_BUTTON_PRESS, 0, 80.125, 90.375, 0);
  editor_event (GDK_MOTION_NOTIFY, GDK_BUTTON1_MASK, 100.625, 115.875, 0); gimp_motion_buffer_end_stroke (shell->motion_buffer);
  gimp_display_set_image (shell->display, other);
  editor_event (GDK_MOTION_NOTIFY, GDK_BUTTON1_MASK, 160.625, 175.875, 0);
  editor_event (GDK_BUTTON_RELEASE, GDK_BUTTON1_MASK, 160.625, 175.875, 0); assert_reset ();
  g_assert_true (gimp_image_get_perspective_guide (other) == new_model);
  g_assert_true (gimp_perspective_guide_get_vanish_points (new_model, 0, &x, &y));
  g_assert_cmpfloat (x, ==, 140.125); g_assert_cmpfloat (y, ==, 150.375);
  before = gimp_canvas_item_get_extents (shell->perspective_guide); g_assert_nonnull (before);
  gimp_perspective_guide_set_vanish_points (old, 0, 9000, -9000);
  after = gimp_canvas_item_get_extents (shell->perspective_guide); g_assert_nonnull (after);
  g_assert_true (cairo_region_equal (before, after)); cairo_region_destroy (before); cairo_region_destroy (after);
  gimp_display_set_image (shell->display, image);
  editor_event (GDK_MOTION_NOTIFY, 0, 80.125, 90.375, 0); editor_event (GDK_BUTTON_PRESS, 0, 80.125, 90.375, 0);
  editor = g_object_ref (active ()); display = shell->display;
  gimp_display_close (display); gimp_test_run_mainloop_until_idle ();
  g_assert_false (gimp_tool_control_is_active (editor->control));
  g_assert_null (GIMP_DRAW_TOOL (editor)->display); g_assert_null (editor->display);
  g_assert_null (g_list_find (editor->status_displays, display));
  g_object_unref (editor); g_object_unref (old); g_object_unref (new_model); g_object_unref (other); g_object_unref (image);
  g_test_message ("native image-switch ignores stale release/model signal; close active editor clears control/draw/display/status refs");
}

#include "test-painter-perspective-smoothing-events.inc"
int main (int argc, char **argv)
{
  gint result;
  g_setenv ("GIMP_NO_PAINT_THREAD", "1", TRUE);
  g_test_init (&argc, &argv, NULL);
  if (!gtk_init_check (&argc, &argv)) return GIMP_EXIT_TEST_SKIPPED;
  gimp_test_utils_setup_menus_path (); gimp = gimp_init_for_gui_testing (TRUE);
  g_test_add_func ("/perspective-lifecycle/native-undo-all-edits", lifecycle_undo_all_edits);
  g_test_add_func ("/perspective-lifecycle/xcf-nonpersistent", lifecycle_xcf_nonpersistent);
  g_test_add_func ("/perspective-lifecycle/switch-close", lifecycle_switch_close);
  g_test_add_func ("/perspective-correction/paintbrush-smoothing-order", smoothing_real_paintbrush);
  g_application_run (gimp->app, 0, NULL); result = gimp_core_app_get_exit_status (GIMP_CORE_APP (gimp->app));
  g_application_quit (G_APPLICATION (gimp->app)); g_clear_object (&gimp->app); return result;
}
