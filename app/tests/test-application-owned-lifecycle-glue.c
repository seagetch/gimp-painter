/* SPDX-License-Identifier: GPL-3.0-or-later
 * Test-only access to the real app activation and accepted-exit callbacks.
 * No copied callbacks, linker wrapping, injected code, or production hooks.
 * Compile instead of tests.c for this executable; app.c's archive member is
 * not extracted because this translation unit supplies its public symbols.
 */
#define GIMP_APP_GLUE_COMPILATION
#include "../app.c"
#include "tests.c"

Gimp *gimp_test_application_owned_start (const gchar *, GApplication **, gboolean);
void gimp_test_application_owned_release (Gimp *, GApplication *);

Gimp *
gimp_test_application_owned_start (const gchar *preset_dir,
                                   GApplication **application,
                                   gboolean gui)
{
  Gimp *gimp;
  GError *error = NULL;
  gimp_log_init ();
  g_object_set (gegl_config (), "use-opencl", FALSE,
                "application-license", "GPL3", NULL);
  gegl_init (NULL, NULL);
  gimp = gimp_new ("Application-owned lifecycle", NULL, NULL,
                   FALSE, FALSE, TRUE, !gui, FALSE, FALSE, TRUE,
                   FALSE, FALSE, GIMP_STACK_TRACE_NEVER, GIMP_PDB_COMPAT_OFF);
  gimp_test_load_default_config (gimp);
  gimp_test_complete_default_gradient_path (gimp);
  g_object_set (gimp->config,
                "layer-presets-path", preset_dir,
                "layer-presets-path-writable", preset_dir,
                "check-updates", FALSE,
                "config-version", GIMP_VERSION,
                "show-welcome-dialog", FALSE, NULL);
  g_object_set (gimp->edit_config, "check-updates", FALSE, NULL);
  gimp_gegl_init (gimp);
  if (gui)
    {
      gtk_init (NULL, NULL);
      gimp_test_utils_setup_menus_path ();
      gimp_init_icon_theme_for_testing ();
      *application = gimp_app_new (gimp, TRUE, FALSE, FALSE, NULL, NULL, NULL);
      gimp_set_show_gui (gimp, TRUE);
    }
  else
    *application = gimp_console_app_new (gimp, FALSE, FALSE, NULL, NULL, NULL);
  gimp->app = *application;
  gimp_filter_exit_init (*application);
  g_application_set_flags (*application, G_APPLICATION_NON_UNIQUE);
  g_assert_true (g_application_register (*application, NULL, &error));
  g_assert_no_error (error);
  g_signal_connect_after (gimp, "exit", G_CALLBACK (app_exit_after_callback),
                          *application);
  /* This is the same activation callback connected by app_run(), including
   * initialize/restore and optional HTTP assignment after restore. */
  g_signal_connect (*application, "activate",
                    G_CALLBACK (app_activate_callback), NULL);
  return gimp;
}

void
gimp_test_application_owned_release (Gimp *gimp, GApplication *application)
{
  /* app_activate_callback() holds a headless application without --quit. */
  if (gimp->no_interface)
    g_application_release (application);
  g_object_unref (application);
  gimp_gegl_exit (gimp);
}
