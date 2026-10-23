/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gegl.h>
#include <gtk/gtk.h>
#include <glib/gstdio.h>
#include <string.h>
#include "core/core-types.h"
#include "widgets/widgets-types.h"
#include "core/gimp.h"
#include "core/gimpcontext.h"
#include "core/gimpcontainer.h"
#include "core/gimpdatafactory.h"
#include "core/gimppaintermybrush.h"
#include "core/gimpmybrush.h"
#include "core/gimptagged.h"
#include "tests.h"
#include "gimp-app-test-utils.h"
static Gimp *gimp;
static void native_type_new_and_ownership (void)
{
  GimpDataFactory *factory = gimp_get_data_factory (gimp, GIMP_TYPE_PAINTER_MYBRUSH);
  GimpContext *context = gimp_get_user_context (gimp);
  GimpContainer *container = gimp_data_factory_get_container (factory);
  GTypeQuery query;
  GError *error = NULL;
  gchar *writable = g_dir_make_tmp ("painter-mybrush-new-XXXXXX", &error);
  gchar *previous_search = NULL, *previous_writable = NULL;
  GimpData *standard;
  gint before = gimp_container_get_n_children (container);
  gint ordinary_before = gimp_container_get_n_children (
    gimp_data_factory_get_container (gimp->mybrush_factory));

  g_assert_no_error (error);
  g_assert_true (factory == gimp->painter_mybrush_factory);
  g_assert_true (gimp_data_factory_has_data_new_func (factory));
  g_assert_true (gimp_data_factory_get_data_type (factory) == GIMP_TYPE_PAINTER_MYBRUSH);
  g_assert_true (g_type_parent (GIMP_TYPE_PAINTER_MYBRUSH) == GIMP_TYPE_DATA);
  g_assert_true (g_type_is_a (GIMP_TYPE_PAINTER_MYBRUSH, GIMP_TYPE_TAGGED));
  g_type_query (GIMP_TYPE_PAINTER_MYBRUSH, &query);
  g_assert_cmpuint (query.instance_size, ==, sizeof (GimpPainterMybrush));
  g_assert_cmpuint (query.class_size, ==, sizeof (GimpPainterMybrushClass));
  standard = gimp_data_factory_data_get_standard (factory, context);
  g_assert_true (GIMP_IS_PAINTER_MYBRUSH (standard));
  g_assert_true (standard == gimp_data_factory_data_get_standard (factory, context));
  g_assert_true (gimp_data_is_internal (standard));
  g_assert_false (gimp_data_is_dirty (standard));
  g_assert_false (gimp_container_have (container, GIMP_OBJECT (standard)));
  g_object_get (gimp->config, "painter-mypaint-brush-path", &previous_search,
                "painter-mypaint-brush-path-writable", &previous_writable, NULL);
  g_object_set (gimp->config, "painter-mypaint-brush-path", writable,
                "painter-mypaint-brush-path-writable", writable, NULL);

  for (guint retained = 0; retained < 2; retained++)
    {
      GimpData *brush = gimp_data_factory_data_new (factory, context, "factory-new-painter");
      gpointer weak = brush;
      gchar *path, *json;
      g_assert_true (GIMP_IS_PAINTER_MYBRUSH (brush));
      g_assert_false (g_object_is_floating (brush));
      g_assert_true (GIMP_IS_PAINTER_MYBRUSH_CLASS (G_OBJECT_GET_CLASS (brush)));
      g_assert_true (GIMP_PAINTER_MYBRUSH_GET_CLASS (brush) ==
                     GIMP_PAINTER_MYBRUSH_CLASS (G_OBJECT_GET_CLASS (brush)));
      g_assert_cmpstr (gimp_object_get_name (brush), ==, "factory-new-painter");
      g_assert_cmpstr (gimp_data_get_mime_type (brush), ==, "application/x-mypaint-brush");
      g_assert_cmpstr (gimp_data_get_extension (brush), ==, ".myb");
      g_assert_true (gimp_container_have (container, GIMP_OBJECT (brush)));
      g_assert_cmpint (gimp_container_get_n_children (container), ==, before + 1);
      g_assert_nonnull (gimp_data_get_file (brush));
      path = g_file_get_path (gimp_data_get_file (brush));
      g_assert_true (g_file_test (path, G_FILE_TEST_IS_REGULAR));
      g_assert_false (gimp_data_is_dirty (brush));
      json = gimp_painter_mybrush_dup_json (GIMP_PAINTER_MYBRUSH (brush), &error);
      g_assert_no_error (error); g_assert_nonnull (json); g_free (json);
      g_object_add_weak_pointer (G_OBJECT (brush), &weak);
      if (retained) g_object_ref (brush);
      g_assert_true (gimp_data_factory_data_delete (factory, brush, TRUE, &error));
      g_assert_no_error (error);
      g_assert_false (g_file_test (path, G_FILE_TEST_EXISTS));
      g_assert_cmpint (gimp_container_get_n_children (container), ==, before);
      if (retained)
        {
          g_assert_true (weak == brush);
          json = gimp_painter_mybrush_dup_json (GIMP_PAINTER_MYBRUSH (brush), &error);
          g_assert_no_error (error); g_assert_nonnull (json); g_free (json);
          g_object_unref (brush);
        }
      g_assert_null (weak);
      g_free (path);
    }
  g_assert_cmpint (gimp_container_get_n_children (
    gimp_data_factory_get_container (gimp->mybrush_factory)), ==, ordinary_before);
  g_object_set (gimp->config, "painter-mypaint-brush-path", previous_search,
                "painter-mypaint-brush-path-writable", previous_writable, NULL);
  g_free (previous_search); g_free (previous_writable);
  g_assert_cmpint (g_rmdir (writable), ==, 0); g_free (writable);
}

static void load_refresh_edit_save (void)
{
  GimpDataFactory *factory = gimp_get_data_factory (gimp, GIMP_TYPE_PAINTER_MYBRUSH);
  GimpContainer *container = gimp_data_factory_get_container (factory);
  GimpContainer *standard = gimp_data_factory_get_container (gimp->mybrush_factory);
  gint standard_before = gimp_container_get_n_children (standard);
  gchar *corpus = g_build_filename (g_getenv ("GIMP_TESTING_ABS_TOP_SRCDIR"), "data/painter-mypaint-brushes", NULL);
  GError *error = NULL;
  gchar *writable = g_dir_make_tmp ("painter-mybrush-factory-XXXXXX", &error);
  gchar *combined, *previous_search = NULL, *previous_writable = NULL;
  GimpData *source, *copy;
  GFile *saved;
  gchar *path;
  guint count;
  g_assert_no_error (error);
  combined = g_build_path (G_SEARCHPATH_SEPARATOR_S, writable, corpus, NULL);
  g_assert_true (factory == gimp->painter_mybrush_factory);
  g_assert_true (gimp_get_data_factory (gimp, GIMP_TYPE_MYBRUSH) == gimp->mybrush_factory);
  g_object_get (gimp->config, "painter-mypaint-brush-path", &previous_search,
    "painter-mypaint-brush-path-writable", &previous_writable, NULL);
  g_object_set (gimp->config, "painter-mypaint-brush-path", combined,
    "painter-mypaint-brush-path-writable", writable, NULL);
  gimp_data_factory_data_refresh (factory, gimp_get_user_context (gimp));
  count = gimp_container_get_n_children (container);
  g_assert_cmpuint (count, ==, 177);
  source = GIMP_DATA (gimp_container_get_child_by_index (container, 0));
  g_assert_true (GIMP_IS_PAINTER_MYBRUSH (source));
  copy = gimp_data_factory_data_duplicate (factory, source);
  g_assert_nonnull (copy);
  gimp_object_set_name (GIMP_OBJECT (copy), "factory-roundtrip-painter");
  g_assert_true (gimp_painter_mybrush_set_json (GIMP_PAINTER_MYBRUSH (copy),
    "{\"version\":3,\"settings\":{\"stroke_opacity\":{\"base_value\":0.375}},\"switches\":{\"non_incremental\":true},\"texts\":{\"brushmark_name\":\"missing-but-retained\"},\"unknown\":{\"keep\":true}}", &error));
  g_assert_no_error (error);
  gimp_data_factory_data_save (factory);
  saved = gimp_data_get_file (copy); g_assert_nonnull (saved);
  path = g_file_get_path (saved); g_assert_true (g_file_test (path, G_FILE_TEST_EXISTS));
  g_assert_false (gimp_data_is_dirty (copy));
  gimp_data_factory_data_refresh (factory, gimp_get_user_context (gimp));
  g_assert_cmpuint (gimp_container_get_n_children (container), ==, 178);
  copy = GIMP_DATA (gimp_container_get_child_by_name (container, "factory-roundtrip-painter"));
  g_assert_nonnull (copy);
  {
    gchar *json = gimp_painter_mybrush_dup_json (GIMP_PAINTER_MYBRUSH (copy), &error);
    g_assert_no_error (error); g_assert_nonnull (strstr (json, "missing-but-retained"));
    g_assert_nonnull (strstr (json, "non_incremental")); g_assert_nonnull (strstr (json, "keep")); g_free (json);
  }
  g_assert_true (gimp_data_factory_data_delete (factory, copy, TRUE, &error));
  g_assert_no_error (error); g_assert_false (g_file_test (path, G_FILE_TEST_EXISTS));
  gimp_data_factory_data_refresh (factory, gimp_get_user_context (gimp));
  g_assert_cmpuint (gimp_container_get_n_children (container), ==, 177);
  g_assert_cmpint (gimp_container_get_n_children (standard), ==, standard_before);
  g_object_set (gimp->config, "painter-mypaint-brush-path", previous_search,
    "painter-mypaint-brush-path-writable", previous_writable, NULL);
  g_free (previous_search); g_free (previous_writable);
  g_free (path); g_rmdir (writable); g_free (combined); g_free (writable); g_free (corpus);
}
int main (int argc, char **argv)
{
  int result;
  g_test_init (&argc, &argv, NULL);
  gimp_test_utils_set_gimp3_directory ("GIMP_TESTING_ABS_TOP_SRCDIR", "app/tests/gimpdir");
  gimp = gimp_init_for_testing ();
  g_test_add_func ("/painter-mybrush-factory/native-type-new-ownership", native_type_new_and_ownership);
  g_test_add_func ("/painter-mybrush-factory/load-refresh-edit-save", load_refresh_edit_save);
  result = g_test_run ();
  gimp_test_utils_set_gimp3_directory ("GIMP_TESTING_ABS_TOP_BUILDDIR", "app/tests/gimpdir-output");
  gimp_exit (gimp, TRUE); return result;
}
