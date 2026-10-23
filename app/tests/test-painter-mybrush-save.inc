/* SPDX-License-Identifier: GPL-3.0-or-later */
static void
file_save_roundtrip (void)
{
  gboolean (*save) (GimpData *, GError **) = gimp_mypaint_brush_save;
  GError *error = NULL;
  gchar *dir = g_dir_make_tmp ("painter-file-save-XXXXXX", &error);
  gchar *path = g_build_filename (dir, "saved.myb", NULL);
  gchar *sidecar = g_build_filename (dir, "saved_prev.png", NULL);
  GFile *file = g_file_new_for_path (path);
  GimpData *brush = gimp_mypaint_brush_new (NULL, "Saved display name");
  GdkPixbuf *icon = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, 3, 5);
  GList *loaded;
  gchar *before, *after;
  guint references = G_OBJECT (brush)->ref_count;
  guint deaths = 0;
  g_assert_no_error (error);
  gimp_data_set_file (brush, file, TRUE, TRUE);
  g_object_unref (file);
  g_assert_true (gimp_data_is_writable (brush));
  gdk_pixbuf_fill (icon, 0x345678ff);
  g_assert_true (gimp_painter_mybrush_set_icon (GIMP_PAINTER_MYBRUSH (brush), icon, &error));
  g_assert_no_error (error);
  g_object_unref (icon);
  before = gimp_painter_mybrush_dup_json (GIMP_PAINTER_MYBRUSH (brush), &error);
  g_assert_no_error (error);
  g_assert_true (gimp_data_is_dirty (brush));
  g_assert_true (save (brush, &error));
  g_assert_no_error (error);
  g_assert_false (gimp_data_is_dirty (brush));
  g_assert_cmpuint (G_OBJECT (brush)->ref_count, ==, references);
  after = gimp_painter_mybrush_dup_json (GIMP_PAINTER_MYBRUSH (brush), &error);
  g_assert_no_error (error);
  g_assert_cmpstr (before, ==, after);
  g_free (before); g_free (after);
  g_assert_false (g_file_test (sidecar, G_FILE_TEST_EXISTS));
  loaded = gimp_mypaint_brush_load (NULL, path, &error);
  g_assert_no_error (error);
  g_assert_cmpuint (g_list_length (loaded), ==, 1);
  g_assert_cmpstr (gimp_object_get_name (loaded->data), ==, "Saved display name");
  g_assert_true (loaded->data != brush);
  icon = gimp_painter_mybrush_ref_icon (loaded->data);
  g_assert_nonnull (icon);
  g_assert_cmpint (gdk_pixbuf_get_width (icon), ==, 3);
  g_assert_cmpint (gdk_pixbuf_get_height (icon), ==, 5);
  g_assert_cmpuint (gdk_pixbuf_get_pixels (icon)[0], ==, 0x34);
  g_object_unref (icon);
  g_object_weak_ref (G_OBJECT (brush), factory_finalized, &deaths);
  g_object_weak_ref (loaded->data, factory_finalized, &deaths);
  g_object_unref (brush);
  g_assert_cmpuint (deaths, ==, 1);
  g_list_free_full (loaded, g_object_unref);
  g_assert_cmpuint (deaths, ==, 2);
  g_assert_cmpint (g_unlink (path), ==, 0);
  g_assert_cmpint (g_rmdir (dir), ==, 0);
  g_free (path); g_free (sidecar); g_free (dir);
}

static void
file_save_failures (void)
{
  GError *error = NULL;
  gchar *dir = g_dir_make_tmp ("painter-save-failure-XXXXXX", &error);
  gchar *path = g_build_filename (dir, "saved.myb", NULL);
  GFile *file = g_file_new_for_path (path);
  GimpData *brush = gimp_mypaint_brush_new (NULL, "Failed save");
  gchar *contents = NULL;
  g_assert_no_error (error);
  g_assert_false (gimp_mypaint_brush_save (brush, &error));
  g_assert_no_error (error); /* Original no-filename behavior. */
  g_assert_true (gimp_data_is_dirty (brush));
  g_assert_true (g_file_set_contents (path, "untouched", -1, &error));
  g_assert_no_error (error);
  gimp_data_set_file (brush, file, FALSE, FALSE);
  g_assert_false (gimp_mypaint_brush_save (brush, &error));
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_PERMISSION_DENIED);
  g_clear_error (&error);
  g_assert_true (g_file_get_contents (path, &contents, NULL, &error));
  g_assert_no_error (error);
  g_assert_cmpstr (contents, ==, "untouched");
  g_free (contents);
  gimp_data_set_file (brush, file, TRUE, TRUE);
  g_assert_true (gimp_data_is_writable (brush));
  g_assert_cmpint (g_unlink (path), ==, 0);
  g_assert_cmpint (g_rmdir (dir), ==, 0);
  g_assert_false (gimp_mypaint_brush_save (brush, &error));
  g_assert_nonnull (error);
  g_assert_cmpuint (error->domain, ==, G_IO_ERROR);
  g_clear_error (&error);
  g_assert_true (gimp_data_is_dirty (brush));
  g_assert_false (gimp_mypaint_brush_save (brush, NULL));
  g_object_unref (file); g_object_unref (brush);
  g_free (path); g_free (dir);
}

typedef struct { GimpData *brush; GError *error; gboolean result; } SaveThreadProbe;
static gpointer
file_save_wrong_thread (gpointer data)
{
  SaveThreadProbe *probe = data;
  probe->result = gimp_mypaint_brush_save (probe->brush, &probe->error);
  return NULL;
}
static void
file_save_state_and_preconditions (void)
{
  GError *error = NULL;
  gchar *dir = g_dir_make_tmp ("painter-save-state-XXXXXX", &error);
  gchar *path = g_build_filename (dir, "saved.myb", NULL);
  GFile *file = g_file_new_for_path (path);
  GimpData *brush = gimp_mypaint_brush_new (NULL, "No obsolete write");
  SaveThreadProbe probe = {brush, NULL, TRUE};
  GThread *thread;
  GError *original;
  gchar *contents = NULL;
  g_assert_no_error (error);
  g_assert_true (g_file_set_contents (path, "unchanged", -1, &error));
  g_assert_no_error (error);
  gimp_data_set_file (brush, file, TRUE, TRUE);
  error = g_error_new_literal (G_IO_ERROR, G_IO_ERROR_FAILED, "keep error");
  original = error;
  g_test_expect_message (NULL, G_LOG_LEVEL_CRITICAL, "*error == nullptr || *error == nullptr*failed*");
  g_assert_false (gimp_mypaint_brush_save (brush, &error));
  g_test_assert_expected_messages ();
  g_assert_true (error == original);
  g_clear_error (&error);
  g_test_expect_message (NULL, G_LOG_LEVEL_CRITICAL, "*GIMP_IS_PAINTER_MYBRUSH*failed*");
  g_assert_false (gimp_mypaint_brush_save (NULL, NULL));
  g_test_assert_expected_messages ();
  thread = g_thread_new ("save-wrong-thread", file_save_wrong_thread, &probe);
  g_thread_join (thread);
  g_assert_false (probe.result);
  g_assert_error (probe.error, GIMP_PAINTER_ERROR, GIMP_PAINTER_ERROR_WRONG_THREAD);
  g_clear_error (&probe.error);
  g_assert_true (gimp_painter_binding_close (G_OBJECT (brush), &error));
  g_assert_no_error (error);
  g_assert_false (gimp_mypaint_brush_save (brush, &error));
  g_assert_error (error, GIMP_PAINTER_ERROR, GIMP_PAINTER_ERROR_CLOSED);
  g_clear_error (&error);
  g_assert_true (gimp_data_is_dirty (brush));
  g_assert_true (g_file_get_contents (path, &contents, NULL, &error));
  g_assert_no_error (error);
  g_assert_cmpstr (contents, ==, "unchanged");
  g_free (contents);
  g_object_unref (file); g_object_unref (brush);
  g_assert_cmpint (g_unlink (path), ==, 0);
  g_assert_cmpint (g_rmdir (dir), ==, 0);
  g_free (path); g_free (dir);
}
