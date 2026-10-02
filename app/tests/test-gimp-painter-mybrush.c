/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gegl.h>
#include <gio/gio.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtk.h>
#include <string.h>
#include "core/core-types.h"
#include "widgets/widgets-types.h"
#include "core/gimppaintermybrush.h"
#include "core/gimp.h"
#include "core/gimpcontext.h"
#include "tests.h"
#include "gimp-app-test-utils.h"
static Gimp *gimp;
#include "core/gimptempbuf.h"
#include "painter/gimp-painter-binding.h"

static void load_all (void)
{
  const gchar *root = g_getenv ("GIMP_TESTING_ABS_TOP_SRCDIR");
  const gchar *groups[] = { "classic", "deevad", "experimental", "kaerhon", "ramon", "tanda" };
  guint count = 0;
  for (guint i = 0; i < G_N_ELEMENTS (groups); ++i)
    {
      gchar *path = g_build_filename (root, "data/painter-mypaint-brushes", groups[i], NULL);
      GDir *dir = g_dir_open (path, 0, NULL);
      const gchar *name;
      g_assert_nonnull (dir);
      while ((name = g_dir_read_name (dir)))
        {
          gchar *full;
          GFile *file;
          GFileInputStream *stream;
          GList *list;
          GError *error = NULL;
          GimpPainterMybrush *brush;
          GimpData *copy;
          gint width = -1, height = -1;
          gchar *a, *b;
          if (!g_str_has_suffix (name, ".myb")) continue;
          full = g_build_filename (path, name, NULL);
          file = g_file_new_for_path (full);
          stream = g_file_read (file, NULL, &error);
          g_assert_no_error (error);
          list = gimp_painter_mybrush_load (NULL, file, G_INPUT_STREAM (stream), &error);
          g_assert_no_error (error); g_assert_cmpuint (g_list_length (list), ==, 1);
          brush = list->data;
          g_assert_true (GIMP_IS_PAINTER_MYBRUSH (brush));
          g_assert_false (gimp_data_is_dirty (GIMP_DATA (brush)));
          g_assert_true (gimp_viewable_get_size (GIMP_VIEWABLE (brush), &width, &height));
          g_assert_cmpint (width, >, 0); g_assert_cmpint (height, >, 0);
          copy = gimp_data_duplicate (GIMP_DATA (brush));
          g_assert_true (GIMP_IS_PAINTER_MYBRUSH (copy));
          a = gimp_painter_mybrush_dup_json (brush, &error);
          b = gimp_painter_mybrush_dup_json (GIMP_PAINTER_MYBRUSH (copy), &error);
          g_assert_no_error (error); g_assert_cmpstr (a, ==, b);
          g_free (a); g_free (b); g_object_unref (copy);
          g_object_unref (brush); g_list_free (list); g_object_unref (stream); g_object_unref (file); g_free (full); ++count;
        }
      g_dir_close (dir); g_free (path);
    }
  g_assert_cmpuint (count, ==, 177);
}
static void stream_icon_and_copy (void)
{
  GimpPainterMybrush *brush = GIMP_PAINTER_MYBRUSH (gimp_painter_mybrush_new (NULL, "original"));
  GdkPixbuf *icon = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, 3, 5);
  GdkPixbuf *owned;
  GOutputStream *output = g_memory_output_stream_new_resizable ();
  GInputStream *input;
  GFile *file = g_file_new_for_path ("/tmp/nonexistent-painter-preview-roundtrip.myb");
  GList *loaded;
  GError *error = NULL;
  GimpTempBuf *preview;
  const gchar *json = "{\"version\":3,\"settings\":{\"texture_grain\":{\"base_value\":0.37,\"inputs\":{}}},\"unknown\":[1,2]}";
  g_assert_true (gimp_painter_mybrush_set_json (brush, json, &error));
  gdk_pixbuf_fill (icon, 0xa04080ff);
  g_assert_true (gimp_painter_mybrush_set_icon (brush, icon, &error));
  g_assert_true (gimp_painter_mybrush_set_icon (brush, icon, &error));
  g_object_unref (icon);
  owned = gimp_painter_mybrush_ref_icon (brush); g_assert_nonnull (owned);
  g_assert_cmpint (gdk_pixbuf_get_rowstride (owned), >=, 12); g_object_unref (owned);
  preview = gimp_viewable_get_new_preview (GIMP_VIEWABLE (brush), gimp_get_user_context (gimp), 12, 20, NULL);
  g_assert_nonnull (preview); g_assert_cmpint (gimp_temp_buf_get_width (preview), ==, 12);
  gimp_temp_buf_unref (preview);
  gimp_object_set_name (GIMP_OBJECT (brush), "renamed");
  g_assert_true (GIMP_DATA_GET_CLASS (brush)->save (GIMP_DATA (brush), output, &error));
  g_assert_no_error (error);
  input = g_memory_input_stream_new_from_data (g_memory_output_stream_get_data (G_MEMORY_OUTPUT_STREAM (output)),
    g_memory_output_stream_get_data_size (G_MEMORY_OUTPUT_STREAM (output)), NULL);
  loaded = gimp_painter_mybrush_load (NULL, file, input, &error);
  g_assert_no_error (error); g_assert_nonnull (loaded);
  g_assert_cmpstr (gimp_object_get_name (loaded->data), ==, "renamed");
  owned = gimp_painter_mybrush_ref_icon (loaded->data); g_assert_nonnull (owned);
  g_assert_cmpint (gdk_pixbuf_get_width (owned), ==, 3); g_assert_cmpint (gdk_pixbuf_get_height (owned), ==, 5);
  g_assert_cmpuint (gdk_pixbuf_get_pixels (owned)[0], ==, 0xa0); g_object_unref (owned);
  g_object_unref (loaded->data); g_list_free (loaded); g_object_unref (input);
  g_assert_true (g_output_stream_close (output, NULL, &error));
  g_assert_false (GIMP_DATA_GET_CLASS (brush)->save (GIMP_DATA (brush), output, &error));
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_CLOSED); g_clear_error (&error);
  g_assert_false (gimp_painter_mybrush_set_json (brush, "{bad", &error)); g_assert_nonnull (error); g_clear_error (&error);
  owned = gimp_painter_mybrush_ref_icon (brush); g_assert_nonnull (owned); g_object_unref (owned);
  g_assert_true (gimp_painter_mybrush_set_icon (brush, NULL, &error)); g_assert_null (gimp_painter_mybrush_ref_icon (brush));
  g_object_unref (output); g_object_unref (file); g_object_unref (brush);
}
static void closed_owner (void)
{
  GimpPainterMybrush *brush = GIMP_PAINTER_MYBRUSH (gimp_painter_mybrush_new (NULL, "closed"));
  GError *error = NULL;
  g_assert_true (gimp_painter_binding_close (G_OBJECT (brush), &error));
  g_assert_false (gimp_painter_mybrush_set_json (brush, "{\"version\":3}", &error));
  g_assert_error (error, GIMP_PAINTER_ERROR, GIMP_PAINTER_ERROR_CLOSED); g_clear_error (&error);
  g_object_run_dispose (G_OBJECT (brush)); g_object_run_dispose (G_OBJECT (brush)); g_object_unref (brush);
}
typedef struct { GimpData *target; gboolean fired; } ReleaseOnIconFinalize;
static void release_target_on_icon_finalize (gpointer user_data, GObject *where_icon_was)
{
  ReleaseOnIconFinalize *state = user_data;
  GimpData *target = state->target;
  state->target = NULL;
  state->fired = TRUE;
  g_object_unref (target);
}
static void copy_icon_finalizer_reentry (void)
{
  GimpData *source = gimp_painter_mybrush_new (NULL, "source");
  GimpData *target = gimp_painter_mybrush_new (NULL, "target");
  GdkPixbuf *icon = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, 3, 5);
  GError *error = NULL;
  ReleaseOnIconFinalize state = { target, FALSE };
  gpointer weak_target = target;
  gdk_pixbuf_fill (icon, 0xff0000ff);
  g_assert_true (gimp_painter_mybrush_set_icon (GIMP_PAINTER_MYBRUSH (target), icon, &error));
  g_assert_no_error (error);
  g_object_weak_ref (G_OBJECT (icon), release_target_on_icon_finalize, &state);
  g_object_add_weak_pointer (G_OBJECT (target), &weak_target);
  g_object_unref (icon); /* target is now the old icon's sole owner */
  gimp_data_copy (target, source);
  g_assert_true (state.fired);
  g_assert_null (state.target);
  g_assert_null (weak_target); /* finalizes safely after the complete callback */
  g_object_unref (source);
}
int main (int argc, char **argv)
{
  int result;
  g_test_init (&argc, &argv, NULL);
  gimp_test_utils_set_gimp3_directory ("GIMP_TESTING_ABS_TOP_SRCDIR", "app/tests/gimpdir");
  gimp = gimp_init_for_testing ();
  g_test_add_func ("/painter-mybrush/177-native-load-copy", load_all);
  g_test_add_func ("/painter-mybrush/stream-icon-copy", stream_icon_and_copy);
  g_test_add_func ("/painter-mybrush/closed-owner", closed_owner);
  g_test_add_func ("/painter-mybrush/copy-icon-finalizer-reentry", copy_icon_finalizer_reentry);
  result = g_test_run ();
  gimp_test_utils_set_gimp3_directory ("GIMP_TESTING_ABS_TOP_BUILDDIR", "app/tests/gimpdir-output");
  gimp_exit (gimp, TRUE); return result;
}
