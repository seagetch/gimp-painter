/* SPDX-License-Identifier: GPL-3.0-or-later */
static void
save_live_icon_pixels (gconstpointer data)
{
  gboolean native_stream = GPOINTER_TO_INT (data);
  GError *error = NULL;
  gchar *dir = g_dir_make_tmp ("painter-live-icon-XXXXXX", &error);
  gchar *path = g_build_filename (dir, "live.myb", NULL);
  GFile *file = g_file_new_for_path (path);
  GimpData *brush = gimp_mypaint_brush_new (NULL, "Live icon");
  GdkPixbuf *icon = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, 3, 5);
  gchar *before, *after;
  const guint32 colors[] = {0xa4b5c6ff, 0x718293ff};
  g_assert_no_error (error);
  gimp_data_set_file (brush, file, TRUE, TRUE);
  gdk_pixbuf_fill (icon, 0x345678ff);
  g_assert_true (gimp_painter_mybrush_set_icon (GIMP_PAINTER_MYBRUSH (brush), icon, &error));
  g_assert_no_error (error);
  g_object_unref (icon);
  before = gimp_painter_mybrush_dup_json (GIMP_PAINTER_MYBRUSH (brush), &error);
  g_assert_no_error (error);
  for (guint i = 0; i < G_N_ELEMENTS (colors); i++)
    {
      GList *loaded;
      icon = gimp_painter_mybrush_ref_icon (GIMP_PAINTER_MYBRUSH (brush));
      /* The public retained image exposes mutable pixels. Do not call set_icon
       * again: saving must read today's pixels, not its earlier encoded cache. */
      gdk_pixbuf_fill (icon, colors[i]);
      g_object_unref (icon);
      if (native_stream)
        {
          GOutputStream *output = g_memory_output_stream_new_resizable ();
          g_assert_true (GIMP_DATA_GET_CLASS (brush)->save (brush, output, &error));
          g_assert_no_error (error);
          g_assert_true (g_file_set_contents (path,
            g_memory_output_stream_get_data (G_MEMORY_OUTPUT_STREAM (output)),
            g_memory_output_stream_get_data_size (G_MEMORY_OUTPUT_STREAM (output)), &error));
          g_assert_no_error (error);
          g_object_unref (output);
        }
      else
        {
          g_assert_true (gimp_mypaint_brush_save (brush, &error));
          g_assert_no_error (error);
          g_assert_false (gimp_data_is_dirty (brush));
        }
      loaded = gimp_mypaint_brush_load (NULL, path, &error);
      g_assert_no_error (error);
      g_assert_cmpuint (g_list_length (loaded), ==, 1);
      icon = gimp_painter_mybrush_ref_icon (loaded->data);
      g_assert_nonnull (icon);
      g_assert_cmpuint (gdk_pixbuf_get_pixels (icon)[0], ==, colors[i] >> 24);
      g_assert_cmpuint (gdk_pixbuf_get_pixels (icon)[1], ==, (colors[i] >> 16) & 0xff);
      g_assert_cmpuint (gdk_pixbuf_get_pixels (icon)[2], ==, (colors[i] >> 8) & 0xff);
      g_assert_cmpint (gdk_pixbuf_get_width (icon), ==, 3);
      g_assert_cmpint (gdk_pixbuf_get_height (icon), ==, 5);
      g_object_unref (icon);
      g_list_free_full (loaded, g_object_unref);
    }
  /* Serializing a detached resource snapshot must not rewrite the source's
   * editable settings/cache or emit a settings mutation as a side effect. */
  after = gimp_painter_mybrush_dup_json (GIMP_PAINTER_MYBRUSH (brush), &error);
  g_assert_no_error (error);
  g_assert_cmpstr (before, ==, after);
  g_free (before); g_free (after);
  g_object_unref (brush); g_object_unref (file);
  g_assert_cmpint (g_unlink (path), ==, 0);
  g_assert_cmpint (g_rmdir (dir), ==, 0);
  g_free (path); g_free (dir);
}

#ifdef GIMP_TEST_MYBRUSH_WRAP_PIXBUF_ENCODE
#include <stdarg.h>
static GdkPixbuf *encode_fault_icon;
static guint encode_fault_count;
gboolean __wrap_gdk_pixbuf_save_to_buffer (GdkPixbuf *, gchar **, gsize *,
                                           const gchar *, GError **, ...);
gboolean
__wrap_gdk_pixbuf_save_to_buffer (GdkPixbuf *pixbuf, gchar **buffer,
                                 gsize *size, const gchar *type,
                                 GError **error, ...)
{
  va_list args;
  gchar *key;
  GPtrArray *keys, *values;
  gboolean result;
  if (pixbuf == encode_fault_icon && encode_fault_icon)
    {
      encode_fault_icon = NULL;
      encode_fault_count++;
      *buffer = NULL; *size = 0;
      g_set_error_literal (error, GDK_PIXBUF_ERROR, GDK_PIXBUF_ERROR_FAILED,
                           "injected PNG encoder failure");
      return FALSE;
    }
  /* Preserve arbitrary option pairs on all non-targeted calls through the
   * real vector encoder rather than dropping varargs in the test wrapper. */
  keys = g_ptr_array_new ();
  values = g_ptr_array_new ();
  va_start (args, error);
  while ((key = va_arg (args, gchar *)))
    {
      g_ptr_array_add (keys, key);
      g_ptr_array_add (values, va_arg (args, gchar *));
    }
  va_end (args);
  g_ptr_array_add (keys, NULL); g_ptr_array_add (values, NULL);
  result = gdk_pixbuf_save_to_bufferv (pixbuf, buffer, size, type,
                                      (gchar **) keys->pdata,
                                      (gchar **) values->pdata, error);
  g_ptr_array_unref (keys); g_ptr_array_unref (values);
  return result;
}

static void
save_live_icon_encode_failure (gconstpointer data)
{
  gboolean native_stream = GPOINTER_TO_INT (data);
  GError *error = NULL;
  gchar *dir = g_dir_make_tmp ("painter-encode-failure-XXXXXX", &error);
  gchar *path = g_build_filename (dir, "saved.myb", NULL);
  GFile *file = g_file_new_for_path (path);
  GimpData *brush = gimp_mypaint_brush_new (NULL, "Encode failure");
  GdkPixbuf *icon = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, 3, 5);
  gchar *before, *after, *contents = NULL;
  g_assert_no_error (error);
  g_assert_true (g_file_set_contents (path, "untouched", -1, &error));
  g_assert_no_error (error);
  gimp_data_set_file (brush, file, TRUE, TRUE);
  gdk_pixbuf_fill (icon, 0x123456ff);
  g_assert_true (gimp_painter_mybrush_set_icon (GIMP_PAINTER_MYBRUSH (brush), icon, &error));
  g_assert_no_error (error);
  before = gimp_painter_mybrush_dup_json (GIMP_PAINTER_MYBRUSH (brush), &error);
  g_assert_no_error (error);
  g_assert_null (encode_fault_icon);
  encode_fault_count = 0; encode_fault_icon = icon;
  if (native_stream)
    {
      GOutputStream *output = g_memory_output_stream_new_resizable ();
      g_assert_false (GIMP_DATA_GET_CLASS (brush)->save (brush, output, &error));
      g_assert_cmpuint (g_memory_output_stream_get_data_size (G_MEMORY_OUTPUT_STREAM (output)), ==, 0);
      g_object_unref (output);
    }
  else
    g_assert_false (gimp_mypaint_brush_save (brush, &error));
  g_assert_error (error, GDK_PIXBUF_ERROR, GDK_PIXBUF_ERROR_FAILED);
  g_clear_error (&error);
  g_assert_null (encode_fault_icon);
  g_assert_cmpuint (encode_fault_count, ==, 1);
  g_assert_true (gimp_data_is_dirty (brush));
  g_assert_true (g_file_get_contents (path, &contents, NULL, &error));
  g_assert_no_error (error);
  g_assert_cmpstr (contents, ==, "untouched");
  g_free (contents);
  after = gimp_painter_mybrush_dup_json (GIMP_PAINTER_MYBRUSH (brush), &error);
  g_assert_no_error (error);
  g_assert_cmpstr (before, ==, after);
  g_free (before); g_free (after);
  /* Fault is one-shot; the same live resource remains usable. */
  g_assert_true (gimp_mypaint_brush_save (brush, &error));
  g_assert_no_error (error);
  g_assert_false (gimp_data_is_dirty (brush));
  g_object_unref (icon); g_object_unref (brush); g_object_unref (file);
  g_assert_cmpint (g_unlink (path), ==, 0);
  g_assert_cmpint (g_rmdir (dir), ==, 0);
  g_free (path); g_free (dir);
}
#endif
