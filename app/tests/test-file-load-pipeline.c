/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <string.h>
#include <glib/gstdio.h>
#include <gegl.h>
#include <gtk/gtk.h>
#include "libgimpbase/gimpbase.h"
#include "widgets/widgets-types.h"
#include "core/gimp.h"
#include "core/gimpimage.h"
#include "core/gimpimage-undo.h"
#include "core/gimpparamspecs.h"
#include "core/gimpundostack.h"
#include "pdb/gimppdb.h"
#include "pdb/gimpprocedure.h"
#include "plug-in/gimppluginmanager.h"
#include "plug-in/gimppluginmanager-file.h"
#include "plug-in/gimppluginprocedure.h"
#include "tests.h"
#include "gimp-app-test-utils.h"

static Gimp                *gimp;
static GimpPlugInProcedure *loader;
static GFile               *fixture;
static gchar               *directory;
static GimpPDBStatusType    loader_status;
static gboolean            return_image;

static GimpValueArray *
load_fixture (GimpProcedure        *procedure,
              Gimp                 *application,
              GimpContext          *context,
              GimpProgress         *progress,
              const GimpValueArray *args,
              GError              **error)
{
  GimpValueArray *values;

  g_assert_cmpint (g_value_get_enum (gimp_value_array_index (args, 0)), ==,
                  GIMP_RUN_NONINTERACTIVE);
  if (loader_status != GIMP_PDB_SUCCESS && loader_status != GIMP_PDB_CANCEL)
    g_set_error_literal (error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                         "fixture loader diagnostic");
  values = gimp_procedure_get_return_values (procedure,
                                             loader_status == GIMP_PDB_SUCCESS,
                                             error ? *error : NULL);
  g_value_set_enum (gimp_value_array_index (values, 0), loader_status);
  if (return_image)
    {
      GimpImage *image = gimp_create_image (application, 8, 8, GIMP_RGB,
                                           GIMP_PRECISION_U8_NON_LINEAR, FALSE);
      /* Deliberately leave the loader's image dirty, with undo disabled. */
      gimp_image_undo_disable (image);
      gimp_image_dirty (image, GIMP_DIRTY_IMAGE);
      g_value_set_object (gimp_value_array_index (values, 1), image);
    }
  return values;
}

static GimpValueArray *
public_load (GFile *file, GError **error)
{
  return gimp_pdb_execute_procedure_by_name (gimp->pdb,
                                             gimp_get_user_context (gimp), NULL,
                                             error, "gimp-file-load",
                                             GIMP_TYPE_RUN_MODE, GIMP_RUN_NONINTERACTIVE,
                                             G_TYPE_FILE, file, G_TYPE_NONE);
}

static void
native_association (void)
{
  gchar          *path = g_build_filename (g_getenv ("GIMP_TESTING_ABS_TOP_SRCDIR"),
                                           "app/tests/files/gimp-2-6-file.xcf", NULL);
  GFile          *file = g_file_new_for_path (path);
  GError         *error = NULL;
  GimpValueArray *values = public_load (file, &error);
  GimpImage      *image;

  g_assert_no_error (error);
  g_assert_cmpint (g_value_get_enum (gimp_value_array_index (values, 0)), ==,
                  GIMP_PDB_SUCCESS);
  image = g_value_get_object (gimp_value_array_index (values, 1));
  g_assert_nonnull (image);
  g_assert_true (g_file_equal (gimp_image_get_file (image), file));
  g_assert_null (gimp_image_get_imported_file (image));
  g_assert_null (gimp_image_get_exported_file (image));
  g_assert_true (gimp_image_undo_is_enabled (image));
  g_assert_cmpint (gimp_image_is_dirty (image), ==, 0);
  gimp_value_array_unref (values);
  g_object_unref (image);
  g_object_unref (file);
  g_free (path);
}

static void
import_association (void)
{
  GError         *error = NULL;
  GimpValueArray *values;
  GimpImage      *image;

  loader_status = GIMP_PDB_SUCCESS;
  return_image = TRUE;
  values = public_load (fixture, &error);
  g_assert_no_error (error);
  g_assert_cmpint (g_value_get_enum (gimp_value_array_index (values, 0)), ==,
                  GIMP_PDB_SUCCESS);
  image = g_value_get_object (gimp_value_array_index (values, 1));
  g_assert_nonnull (image);
  g_assert_null (gimp_image_get_file (image));
  g_assert_true (g_file_equal (gimp_image_get_imported_file (image), fixture));
  g_assert_null (gimp_image_get_exported_file (image));
  g_assert_true (gimp_image_get_load_proc (image) == loader);
  g_assert_true (gimp_image_undo_is_enabled (image));
  g_assert_cmpint (gimp_image_is_dirty (image), ==, 0);
  g_assert_cmpint (gimp_undo_stack_get_depth (gimp_image_get_undo_stack (image)), ==, 0);
  gimp_value_array_unref (values);
  g_object_unref (image);
}

static void
failure_status (gconstpointer data)
{
  GError         *error = NULL;
  GimpValueArray *values;

  loader_status = GPOINTER_TO_INT (data);
  return_image = FALSE;
  values = public_load (fixture, &error);
  g_assert_cmpint (g_value_get_enum (gimp_value_array_index (values, 0)), ==,
                  loader_status);
  if (loader_status == GIMP_PDB_CANCEL)
    g_assert_no_error (error);
  else
    {
      g_assert_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED);
      g_assert_cmpstr (error->message, ==, "fixture loader diagnostic");
      g_assert_cmpstr (g_value_get_string (gimp_value_array_index (values, 1)), ==,
                      "fixture loader diagnostic");
    }
  g_clear_error (&error);
  gimp_value_array_unref (values);
}

static void
missing_image (gconstpointer data)
{
  GError         *error = NULL;
  GimpValueArray *values;

  loader_status = GIMP_PDB_SUCCESS;
  return_image = FALSE;
  loader->generic_file_proc = GPOINTER_TO_INT (data);
  values = public_load (fixture, &error);
  g_assert_cmpint (g_value_get_enum (gimp_value_array_index (values, 0)), ==,
                  GIMP_PDB_EXECUTION_ERROR);
  g_assert_nonnull (error);
  g_assert_nonnull (strstr (error->message, "image"));
  g_clear_error (&error);
  gimp_value_array_unref (values);
  loader->generic_file_proc = FALSE;
}

int
main (int argc, char **argv)
{
  GimpProcedure *procedure;
  gchar         *filename;
  gint           result;

  g_test_init (&argc, &argv, NULL);
  gimp_test_utils_set_gimp3_directory ("GIMP_TESTING_ABS_TOP_SRCDIR", "app/tests/gimpdir");
  gimp = gimp_init_for_testing ();
  gimp_test_utils_set_gimp3_directory ("GIMP_TESTING_ABS_TOP_BUILDDIR", "app/tests/gimpdir-output");
  directory = g_dir_make_tmp ("gimp-load-pipeline-XXXXXX", NULL);
  g_assert_nonnull (directory);
  filename = g_build_filename (directory, "fixture.pipeline-test", NULL);
  g_assert_true (g_file_set_contents (filename, "test fixture", -1, NULL));
  fixture = g_file_new_for_path (filename);
  procedure = gimp_plug_in_procedure_new (GIMP_PDB_PROC_TYPE_PLUGIN, fixture);
  procedure->proc_type = GIMP_PDB_PROC_TYPE_INTERNAL;
  procedure->marshal_func = load_fixture;
  gimp_object_set_static_name (GIMP_OBJECT (procedure), "file-pipeline-test-load");
  loader = GIMP_PLUG_IN_PROCEDURE (procedure);
  loader->menu_label = g_strdup ("pipeline test loader");
  gimp_plug_in_procedure_set_file_proc (loader, "pipeline-test", "", NULL);
  gimp_plug_in_procedure_set_mime_types (loader, "image/x-pipeline-test");
  gimp_procedure_add_argument (procedure,
                               g_param_spec_enum ("run-mode", "Run mode", "Run mode",
                                                  GIMP_TYPE_RUN_MODE, GIMP_RUN_NONINTERACTIVE,
                                                  GIMP_PARAM_READWRITE));
  gimp_procedure_add_argument (procedure,
                               g_param_spec_object ("file", "File", "File", G_TYPE_FILE,
                                                    GIMP_PARAM_READWRITE));
  /* Nullable deliberately exercises an ill-behaved loader's success path. */
  gimp_procedure_add_return_value (procedure,
                                   gimp_param_spec_image ("image", "Image", "Image", TRUE,
                                                          GIMP_PARAM_READWRITE));
  gimp_pdb_register_procedure (gimp->pdb, procedure);
  gimp_plug_in_manager_add_load_procedure (gimp->plug_in_manager, loader);

  g_test_add_func ("/file-load/native-association", native_association);
  g_test_add_func ("/file-load/import-association-and-undo", import_association);
  g_test_add_data_func ("/file-load/cancel", GINT_TO_POINTER (GIMP_PDB_CANCEL), failure_status);
  g_test_add_data_func ("/file-load/calling-error", GINT_TO_POINTER (GIMP_PDB_CALLING_ERROR), failure_status);
  g_test_add_data_func ("/file-load/missing-image", GINT_TO_POINTER (FALSE), missing_image);
  g_test_add_data_func ("/file-load/generic-missing-image", GINT_TO_POINTER (TRUE), missing_image);
  result = g_test_run ();

  gimp->plug_in_manager->load_procs = g_slist_remove (gimp->plug_in_manager->load_procs, loader);
  gimp_pdb_unregister_procedure (gimp->pdb, procedure);
  g_object_unref (procedure);
  g_object_unref (fixture);
  g_remove (filename);
  g_rmdir (directory);
  g_free (filename);
  g_free (directory);
  gimp_exit (gimp, TRUE);
  return result;
}
