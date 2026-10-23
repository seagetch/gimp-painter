/* SPDX-License-Identifier: GPL-3.0-or-later */
static void
filename_load_owned_resource (void)
{
  GList *(*load) (GimpContext *, const gchar *, GError **) = gimp_mypaint_brush_load;
  GError *error = NULL;
  gchar *dir = g_dir_make_tmp ("painter-filename-load-XXXXXX", &error);
  gchar *path = g_build_filename (dir, "from-filename.myb", NULL);
  gchar *icon_path = g_build_filename (dir, "from-filename_prev.png", NULL);
  GdkPixbuf *icon = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, 3, 5);
  GList *first, *second;
  guint deaths = 0;
  gchar *before, *after;
  g_assert_no_error (error);
  g_assert_true (g_file_set_contents (path, "version 2\nradius_logarithmic 1.5\n", -1, &error));
  g_assert_no_error (error);
  gdk_pixbuf_fill (icon, 0x123456ff);
  g_assert_true (gdk_pixbuf_save (icon, icon_path, "png", &error, NULL));
  g_assert_no_error (error);
  g_object_unref (icon);
  first = load (NULL, path, &error);
  second = load (gimp_get_user_context (gimp), path, &error);
  g_assert_no_error (error);
  g_assert_cmpuint (g_list_length (first), ==, 1);
  g_assert_cmpuint (g_list_length (second), ==, 1);
  g_assert_true (GIMP_IS_PAINTER_MYBRUSH (first->data));
  g_assert_true (first->data != second->data);
  g_assert_false (g_object_is_floating (first->data));
  g_assert_false (gimp_data_is_dirty (first->data));
  g_assert_null (gimp_data_get_file (first->data));
  g_assert_cmpstr (gimp_object_get_name (first->data), ==, "from-filename");
  g_object_weak_ref (first->data, factory_finalized, &deaths);
  g_object_weak_ref (second->data, factory_finalized, &deaths);
  before = gimp_painter_mybrush_dup_json (second->data, &error);
  g_assert_no_error (error);
  /* The result survives removal of all input files and caller path storage. */
  g_assert_cmpint (g_unlink (path), ==, 0);
  g_assert_cmpint (g_unlink (icon_path), ==, 0);
  g_assert_cmpint (g_rmdir (dir), ==, 0);
  g_free (path); g_free (icon_path); g_free (dir);
  icon = gimp_painter_mybrush_ref_icon (first->data);
  g_assert_nonnull (icon);
  g_assert_cmpint (gdk_pixbuf_get_width (icon), ==, 3);
  g_assert_cmpint (gdk_pixbuf_get_height (icon), ==, 5);
  g_assert_cmpint (gdk_pixbuf_get_pixels (icon)[0], ==, 0x12);
  gdk_pixbuf_fill (icon, 0xaabbccff);
  g_object_unref (icon);
  icon = gimp_painter_mybrush_ref_icon (second->data);
  g_assert_cmpint (gdk_pixbuf_get_pixels (icon)[0], ==, 0x12);
  g_object_unref (icon);
  g_list_free_full (first, g_object_unref);
  g_assert_cmpuint (deaths, ==, 1);
  after = gimp_painter_mybrush_dup_json (second->data, &error);
  g_assert_no_error (error);
  g_assert_cmpstr (before, ==, after);
  g_free (before); g_free (after);
  g_list_free_full (second, g_object_unref);
  g_assert_cmpuint (deaths, ==, 2);
}

static void
filename_load_failures (void)
{
  GError *error = NULL;
  gchar *dir = g_dir_make_tmp ("painter-filename-errors-XXXXXX", &error);
  gchar *path = g_build_filename (dir, "invalid.myb", NULL);
  const gchar *bad[] = {"", "{not JSON", "version 2\nradius_logarithmic nan\n"};
  g_assert_no_error (error);
  g_assert_null (gimp_mypaint_brush_load (NULL, path, &error));
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND);
  g_clear_error (&error);
  for (guint i = 0; i < G_N_ELEMENTS (bad); i++)
    {
      g_assert_true (g_file_set_contents (path, bad[i], -1, &error));
      g_assert_no_error (error);
      g_assert_null (gimp_mypaint_brush_load (NULL, path, &error));
      g_assert_nonnull (error);
      g_clear_error (&error);
      g_assert_null (gimp_mypaint_brush_load (NULL, path, NULL));
    }
  /* Valid minimal legacy input is not mistaken for an empty/invalid file. */
  g_assert_true (g_file_set_contents (path, "version 2\n", -1, &error));
  GList *result = gimp_mypaint_brush_load (NULL, path, &error);
  g_assert_no_error (error);
  g_assert_cmpuint (g_list_length (result), ==, 1);
  g_list_free_full (result, g_object_unref);
  g_assert_true (g_file_set_contents (path,
    "{\"version\":3,\"parent_brush_name\":\"Named JSON\",\"settings\":{}}", -1, &error));
  result = gimp_mypaint_brush_load (NULL, path, &error);
  g_assert_no_error (error);
  g_assert_cmpuint (g_list_length (result), ==, 1);
  g_assert_cmpstr (gimp_object_get_name (result->data), ==, "Named JSON");
  g_list_free_full (result, g_object_unref);
  g_assert_cmpint (g_unlink (path), ==, 0);
  g_assert_cmpint (g_rmdir (dir), ==, 0);
  g_free (path); g_free (dir);
}

static void
filename_load_preconditions (void)
{
  GError *error = g_error_new_literal (G_IO_ERROR, G_IO_ERROR_FAILED, "preserve caller error");
  GError *original = error;
  g_test_expect_message (NULL, G_LOG_LEVEL_CRITICAL, "*filename != nullptr*failed*");
  g_assert_null (gimp_mypaint_brush_load (NULL, NULL, NULL));
  g_test_assert_expected_messages ();
  g_test_expect_message (NULL, G_LOG_LEVEL_CRITICAL, "*g_path_is_absolute*failed*");
  g_assert_null (gimp_mypaint_brush_load (NULL, "relative.myb", NULL));
  g_test_assert_expected_messages ();
  g_test_expect_message (NULL, G_LOG_LEVEL_CRITICAL, "*error == nullptr || *error == nullptr*failed*");
  g_assert_null (gimp_mypaint_brush_load (NULL, "/unused.myb", &error));
  g_test_assert_expected_messages ();
  g_assert_true (error == original);
  g_assert_cmpstr (error->message, ==, "preserve caller error");
  g_clear_error (&error);
}
