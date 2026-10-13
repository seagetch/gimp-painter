/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 2009 Martin Nordholts
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <stdlib.h>

#include <gegl.h>
#include <gtk/gtk.h>

#include "gui/gui-types.h"

#include "gui/gui.h"

#include "actions/actions.h"

#include "menus/menus.h"

#include "widgets/gimpsessioninfo.h"

#include "config/gimpgeglconfig.h"
#include "config/gimprc.h"

#include "core/gimp.h"
#include "core/gimp-contexts.h"

#include "gegl/gimp-gegl.h"

#include "gimp-log.h"
#include "gimpcoreapp.h"

#include "gimp-app-test-utils.h"
#include "tests.h"

#ifdef GDK_WINDOWING_QUARTZ
#include <Cocoa/Cocoa.h>
#endif


static void
gimp_status_func_dummy (const gchar *text1,
                        const gchar *text2,
                        gdouble      percentage)
{
}

typedef struct
{
  GFile   *user_rc;
  GWeakRef config;
  GWeakRef edit_config;
} GimpTestConfigFile;

static GSList *gimp_test_config_files = NULL;

static void
gimp_test_remove_config_files (void)
{
  GSList *iter;

  /* Some standalone test mains call gimp_exit() but retain Gimp until process
   * exit. Wait until main has returned, rather than delete during Gimp's exit
   * signal or assume final unref. Do not change the real autosave lifecycle. */
  for (iter = gimp_test_config_files; iter; iter = iter->next)
    {
      GimpTestConfigFile *owned = iter->data;
      GObject           *config;
      GError            *error = NULL;
      gboolean           removed;

      config = g_weak_ref_get (&owned->config);
      if (config)
        {
          g_assert_cmpuint (GIMP_RC (config)->save_idle_id, ==, 0);
          g_object_unref (config);
        }
      config = g_weak_ref_get (&owned->edit_config);
      if (config)
        {
          g_assert_cmpuint (GIMP_RC (config)->save_idle_id, ==, 0);
          g_object_unref (config);
        }

      removed = g_file_delete (owned->user_rc, NULL, &error);
      if (! removed && g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND))
        g_clear_error (&error);
      g_assert_no_error (error);
      g_weak_ref_clear (&owned->config);
      g_weak_ref_clear (&owned->edit_config);
      g_object_unref (owned->user_rc);
      g_free (owned);
    }

  g_clear_pointer (&gimp_test_config_files, g_slist_free);
}

/* Ordinary tests have no checked-in personal gimprc. Give every initialization
 * an empty, owned personal RC file, while retaining source session/dock inputs.
 * Its path is never reused as another test's input. TMPDIR is owned and cleaned
 * by the isolated runtime launcher, including expected assertion-abort runs.
 * Normal main-return/exit paths additionally remove the file with atexit(). */
static void
gimp_test_load_default_config (Gimp *gimp)
{
  static gboolean     cleanup_registered = FALSE;
  GimpTestConfigFile  *owned;
  GFileIOStream      *stream;
  GError             *error = NULL;
  GParamSpec         *pluginrc_spec;
  const gchar        *build_root;
  gchar              *pluginrc;
  gboolean            closed;

  if (! g_getenv ("GIMP_TESTING_ABS_TOP_SRCDIR"))
    {
      gimp_load_config (gimp, NULL, NULL);
      return;
    }

  build_root = g_getenv ("GIMP_TESTING_ABS_TOP_BUILDDIR");
  g_assert_nonnull (build_root);
  g_assert_true (g_path_is_absolute (build_root));
  if (! cleanup_registered)
    {
      gint result = atexit (gimp_test_remove_config_files);
      g_assert_cmpint (result, ==, 0);
      cleanup_registered = TRUE;
    }

  owned = g_new0 (GimpTestConfigFile, 1);
  owned->user_rc = g_file_new_tmp ("gimp-app-test-gimprc-XXXXXX", &stream, &error);
  g_assert_no_error (error);
  g_assert_nonnull (owned->user_rc);
  closed = g_io_stream_close (G_IO_STREAM (stream), NULL, &error);
  g_assert_no_error (error);
  g_assert_true (closed);
  g_object_unref (stream);

  gimp_load_config (gimp, NULL, owned->user_rc);
  g_weak_ref_init (&owned->config, G_OBJECT (gimp->config));
  g_weak_ref_init (&owned->edit_config, G_OBJECT (gimp->edit_config));
  gimp_test_config_files = g_slist_prepend (gimp_test_config_files, owned);

  /* Cache writes are ordinary mutable build output, independent of personal
   * RC lifetime. Native and sanitizer builds have separate cache paths. */
  pluginrc_spec = g_object_class_find_property (G_OBJECT_GET_CLASS (gimp->config),
                                               "pluginrc-path");
  g_object_get (gimp->config, "pluginrc-path", &pluginrc, NULL);
  if (g_strcmp0 (pluginrc,
                 g_value_get_string (g_param_spec_get_default_value (pluginrc_spec))) == 0)
    {
      gchar *path = g_build_filename (build_root, "app", "tests",
                                      "gimpdir-output", "pluginrc", NULL);
      g_object_set (gimp->config, "pluginrc-path", path, NULL);
      g_free (path);
    }

  g_free (pluginrc);
}

/* The testing data-path default searches only source/data/gradients, while
 * its writable default is ${gimp_dir}/gradients. The built-in Custom gradient
 * is internal but writable and dirty, so normal shutdown still resolves a
 * save directory before gimp_data_save() handles it as internal data.
 * Keep the source search and the writable token together. Do not expand the
 * token here: GUI tests redirect gimp_dir to gimpdir-output before shutdown.
 * Leave explicitly customized path pairs unchanged.
 */
static void
gimp_test_complete_default_gradient_path (Gimp *gimp)
{
  GObjectClass *klass = G_OBJECT_GET_CLASS (gimp->config);
  GParamSpec   *path_spec;
  GParamSpec   *writable_spec;
  gchar        *path;
  gchar        *writable;

  if (! g_getenv ("GIMP_TESTING_ABS_TOP_SRCDIR"))
    return;

  path_spec = g_object_class_find_property (klass, "gradient-path");
  writable_spec = g_object_class_find_property (klass, "gradient-path-writable");
  g_object_get (gimp->config,
                "gradient-path",          &path,
                "gradient-path-writable", &writable,
                NULL);

  if (g_strcmp0 (path,
                 g_value_get_string (g_param_spec_get_default_value (path_spec))) == 0 &&
      g_strcmp0 (writable,
                 g_value_get_string (g_param_spec_get_default_value (writable_spec))) == 0)
    {
      gchar *complete = g_build_path (G_SEARCHPATH_SEPARATOR_S,
                                      path, writable, NULL);

      g_object_set (gimp->config, "gradient-path", complete, NULL);
      g_free (complete);
    }

  g_free (path);
  g_free (writable);
}

/**
 * gimp_init_for_testing:
 *
 * Initialize the GIMP object system for unit testing. This is a
 * selected subset of the initialization happening in app_run().
 **/
Gimp *
gimp_init_for_testing (void)
{
  Gimp *gimp;

  gimp_log_init ();
  gegl_init (NULL, NULL);

  gimp = gimp_new ("Unit Tested GIMP", NULL, NULL, FALSE, TRUE, TRUE, TRUE,
                   FALSE, FALSE, TRUE, FALSE, FALSE,
                   GIMP_STACK_TRACE_QUERY, GIMP_PDB_COMPAT_OFF);

  gimp_test_load_default_config (gimp);
  gimp_test_complete_default_gradient_path (gimp);

  gimp_gegl_init (gimp);
  gimp_initialize (gimp, gimp_status_func_dummy);
  gimp_restore (gimp, gimp_status_func_dummy, NULL);

  return gimp;
}


#ifndef GIMP_CONSOLE_COMPILATION

static void
gimp_init_icon_theme_for_testing (void)
{
  gchar       *icon_root;

  icon_root = g_test_build_filename (G_TEST_BUILT, "gimp-test-icon-theme", NULL);
  gtk_icon_theme_prepend_search_path (gtk_icon_theme_get_default (),
                                      icon_root);
  g_free (icon_root);
  return;
}

#ifdef GDK_WINDOWING_QUARTZ
static gboolean
gimp_osx_focus_window (gpointer user_data)
{
  [NSApp activateIgnoringOtherApps:YES];
  return FALSE;
}
#endif

static void
gimp_test_app_activate_callback (GimpCoreApp *app,
                                 gpointer     user_data)
{
  Gimp *gimp = NULL;

  g_return_if_fail (GIMP_IS_CORE_APP (app));

  gimp = gimp_core_app_get_gimp (app);

  gimp_core_app_set_exit_status (app, EXIT_SUCCESS);

  gui_init (gimp, TRUE, NULL, g_getenv ("GIMP_TESTING_ABS_TOP_SRCDIR"), "System Language");
  gimp_init_icon_theme_for_testing ();
  gimp_initialize (gimp, gimp_status_func_dummy);
  gimp_restore (gimp, gimp_status_func_dummy, NULL);
#ifdef GDK_WINDOWING_QUARTZ
  g_idle_add (gimp_osx_focus_window, NULL);
#endif
  gimp->initialized = TRUE;
  gimp_core_app_set_exit_status (app, g_test_run ());

  /* Don't write files to the source dir */
  gimp_test_utils_set_gimp3_directory ("GIMP_TESTING_ABS_TOP_BUILDDIR",
                                       "app/tests/gimpdir-output");

  gimp_exit (gimp, TRUE);
}

static Gimp *
gimp_init_for_gui_testing_internal (gboolean  show_gui,
                                    GFile    *gimprc)
{
  Gimp           *gimp;
#if defined (G_OS_WIN32)
  GLogLevelFlags  fatal_mask;
#endif

  /* Load configuration from the source dir */
  gimp_test_utils_set_gimp3_directory ("GIMP_TESTING_ABS_TOP_SRCDIR",
                                       "app/tests/gimpdir");

#if defined (G_OS_WIN32)
  /* g_test_init() sets warnings always fatal, which is a usually a good
     testing default. Nevertheless the Windows platform may have a few
     quirks generating warnings, yet we want to finish tests. So we
     allow some relaxed rules on this platform. */
  fatal_mask = (GLogLevelFlags) (G_LOG_FATAL_MASK | G_LOG_LEVEL_CRITICAL);
  g_log_set_always_fatal (fatal_mask);
#endif

  /* from main() */
  gimp_log_init ();
  g_object_set (gegl_config (),
                "use-opencl",          FALSE,
                "application-license", "GPL3",
                NULL);
  gegl_init (NULL, NULL);

  /* Introduce an error margin for positions written to sessionrc */
  gimp_session_info_set_position_accuracy (5);

  /* from app_run() */
  gimp = gimp_new ("Unit Tested GIMP", NULL, NULL, FALSE, TRUE, TRUE, !show_gui,
                   FALSE, FALSE, TRUE, FALSE, FALSE,
                   GIMP_STACK_TRACE_QUERY, GIMP_PDB_COMPAT_OFF);
  gimp->app = gimp_app_new (gimp, TRUE, FALSE, FALSE, NULL, NULL, NULL);

  gimp_set_show_gui (gimp, show_gui);
  if (gimprc)
    {
      /* Preserve the caller-provided RC and its existing path contract. */
      gimp_load_config (gimp, gimprc, NULL);
    }
  else
    {
      gimp_test_load_default_config (gimp);
      gimp_test_complete_default_gradient_path (gimp);
    }
  gimp_gegl_init (gimp);

  g_signal_connect (gimp->app, "activate",
                    G_CALLBACK (gimp_test_app_activate_callback),
                    NULL);
  return gimp;
}

/**
 * gimp_init_for_gui_testing:
 * @show_gui:
 *
 * Initializes a #Gimp instance for use in test cases that rely on GUI
 * code to be initialized.
 *
 * Returns: The #Gimp instance.
 **/
Gimp *
gimp_init_for_gui_testing (gboolean show_gui)
{
  return gimp_init_for_gui_testing_internal (show_gui, NULL);
}

/**
 * gimp_init_for_gui_testing:
 * @show_gui:
 * @gimprc:
 *
 * Like gimp_init_for_gui_testing(), but also allows a custom gimprc
 * filename to be specified.
 *
 * Returns: The #Gimp instance.
 **/
Gimp *
gimp_init_for_gui_testing_with_rc (gboolean  show_gui,
                                   GFile    *gimprc)
{
  return gimp_init_for_gui_testing_internal (show_gui, gimprc);
}

#endif /* GIMP_CONSOLE_COMPILATION */

static gboolean
gimp_tests_quit_mainloop (GMainLoop *loop)
{
  g_main_loop_quit (loop);

  return FALSE;
}

/**
 * gimp_test_run_temp_mainloop:
 * @running_time: The time to run the main loop.
 *
 * Helper function for tests that wants to run a main loop for a
 * while. Useful when you want GIMP's state to settle before doing
 * tests.
 **/
void
gimp_test_run_temp_mainloop (guint32 running_time)
{
  GMainLoop *loop;
  loop = g_main_loop_new (NULL, FALSE);

  g_timeout_add (running_time,
                 (GSourceFunc) gimp_tests_quit_mainloop,
                 loop);

  g_main_loop_run (loop);

  g_main_loop_unref (loop);
}

/**
 * gimp_test_run_mainloop_until_idle:
 *
 * Creates and runs a main loop until it is idle, i.e. has no more
 * work to do.
 **/
void
gimp_test_run_mainloop_until_idle (void)
{
  GMainLoop *loop = g_main_loop_new (NULL, FALSE);

  g_idle_add ((GSourceFunc) gimp_tests_quit_mainloop, loop);

  g_main_loop_run (loop);

  g_main_loop_unref (loop);
}

/**
 * gimp_test_bail_if_no_display:
 * @void:
 *
 * If no DISPLAY is set, call exit(EXIT_SUCCESS). There is no use in
 * having UI tests failing in DISPLAY-less environments.
 **/
void
gimp_test_bail_if_no_display (void)
{
  if (! g_getenv ("DISPLAY"))
    {
      g_message ("No DISPLAY set, not running UI tests\n");
      exit (GIMP_EXIT_TEST_SKIPPED);
    }
}
