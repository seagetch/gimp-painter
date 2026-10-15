/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"

#include <gegl.h>
#include <gtk/gtk.h>
#include <glib/gstdio.h>
#include <string.h>

#include "libgimpbase/gimpbase.h"
#include "libgimpconfig/gimpconfig.h"
#include "core/core-types.h"
#include "widgets/widgets-types.h"
#include "core/gimp.h"
#include "core/gimpcontainer.h"
#include "core/gimplist.h"
#include "core/gimpcontext.h"
#include "core/gimptoolinfo.h"
#include "widgets/gimpdeviceinfo.h"
#include "widgets/gimpdevicemanager.h"
#include "widgets/gimpdevices.h"
#include "gimpcoreapp.h"
#include "tests.h"
#include "gimp-app-test-utils.h"

static Gimp *gimp;

static void
save_lifecycle (gconstpointer data)
{
  guint              flags = GPOINTER_TO_UINT (data);
  gboolean           focused = (flags & 1) != 0;
  gboolean           shared = (flags & 2) != 0;
  GimpDeviceManager *manager = gimp_devices_get_manager (gimp);
  gchar             *previous = g_strdup (g_getenv ("GIMP3_DIRECTORY"));
  GError            *error = NULL;
  gchar             *directory = g_dir_make_tmp ("gimp-device-save-XXXXXX", &error);
  gchar             *path;
  gchar             *contents = NULL;
  gsize              length = 0;
  gboolean           previous_shared;
  const gchar        sentinel[] = "# Preserve the unread device profile byte for byte.\n"
                                   "(GimpDeviceInfo \"Offline sentinel\" (mode disabled))\n";

  g_assert_no_error (error);
  g_assert_nonnull (directory);
  path = g_build_filename (directory, "devicerc", NULL);
  g_assert_true (g_file_set_contents (path, sentinel, sizeof (sentinel) - 1, &error));
  g_assert_no_error (error);
  g_setenv ("GIMP3_DIRECTORY", directory, TRUE);
  g_object_get (gimp->config, "devices-share-tool", &previous_shared, NULL);
  g_object_set (gimp->config, "devices-share-tool", shared, NULL);

  if (focused)
    {
      gimp_set_focused_once (gimp);
      g_assert_nonnull (gimp_device_manager_get_current_device (manager));
      g_assert_cmpint (gimp_container_get_n_children (GIMP_CONTAINER (manager)), >, 0);
      g_assert_nonnull (gimp_container_get_child_by_name (GIMP_CONTAINER (manager),
                                                        "Offline sentinel"));
    }
  else
    {
      /* No fake focus or synthetic device: this is the deferred startup state
       * seen when the application exits before its first window focus. */
      g_assert_false (gimp_has_focused_once (gimp));
      g_assert_null (gimp_device_manager_get_current_device (manager));
      g_assert_cmpint (gimp_container_get_n_children (GIMP_CONTAINER (manager)), ==, 0);
    }

  /* Both the automatic exit save and the explicit-save request must preserve
   * unread state; always_save only overrides an explicitly deleted devicerc. */
  for (guint always_save = 0; always_save < 2; always_save++)
    {
      gimp_devices_save (gimp, always_save);
      g_assert_true (g_file_get_contents (path, &contents, &length, &error));
      g_assert_no_error (error);
      if (! focused)
        g_assert_cmpmem (contents, length, sentinel, sizeof (sentinel) - 1);
      else
        {
          GimpContainer *copy = gimp_list_new (GIMP_TYPE_DEVICE_INFO, TRUE);
          GimpDeviceInfo *current = gimp_device_manager_get_current_device (manager);
          GimpDeviceInfo *saved;

          g_assert_true (gimp_config_deserialize_string (GIMP_CONFIG (copy), contents,
                                                       length, gimp, &error));
          g_assert_no_error (error);
          g_assert_cmpint (gimp_container_get_n_children (copy), ==,
                           gimp_container_get_n_children (GIMP_CONTAINER (manager)));
          saved = GIMP_DEVICE_INFO (gimp_container_get_child_by_name (
                    copy, gimp_object_get_name (current)));
          g_assert_nonnull (saved);
          g_assert_nonnull (gimp_container_get_child_by_name (copy, "Offline sentinel"));
          g_assert_nonnull (GIMP_TOOL_PRESET (saved)->tool_options);
          if (! shared)
            g_assert_true (gimp_context_get_tool (GIMP_CONTEXT (GIMP_TOOL_PRESET (saved)->tool_options)) ==
                           gimp_context_get_tool (gimp_get_user_context (gimp)));
          g_object_unref (copy);
        }
      g_clear_pointer (&contents, g_free);
    }

  g_assert_cmpint (g_remove (path), ==, 0);
  if (! focused)
    {
      gimp_devices_save (gimp, FALSE);
      g_assert_false (g_file_test (path, G_FILE_TEST_EXISTS));
      gimp_devices_save (gimp, TRUE);
      g_assert_false (g_file_test (path, G_FILE_TEST_EXISTS));
    }

  g_object_set (gimp->config, "devices-share-tool", previous_shared, NULL);
  if (previous)
    g_setenv ("GIMP3_DIRECTORY", previous, TRUE);
  else
    g_unsetenv ("GIMP3_DIRECTORY");
  g_assert_cmpint (g_rmdir (directory), ==, 0);
  g_free (path);
  g_free (directory);
  g_free (previous);
}

int
main (int argc, char **argv)
{
  gint result;

  g_test_init (&argc, &argv, NULL);
  if (! gtk_init_check (&argc, &argv))
    return GIMP_EXIT_TEST_SKIPPED;
  gimp_test_utils_setup_menus_path ();
  gimp = gimp_init_for_gui_testing (TRUE);
  g_test_add_data_func ("/device-save/unfocused/shared", GUINT_TO_POINTER (2), save_lifecycle);
  g_test_add_data_func ("/device-save/unfocused/per-device", GUINT_TO_POINTER (0), save_lifecycle);
  g_test_add_data_func ("/device-save/focused/shared", GUINT_TO_POINTER (3), save_lifecycle);
  g_test_add_data_func ("/device-save/focused/per-device", GUINT_TO_POINTER (1), save_lifecycle);
  g_application_run (gimp->app, 0, NULL);
  result = gimp_core_app_get_exit_status (GIMP_CORE_APP (gimp->app));
  g_application_quit (G_APPLICATION (gimp->app));
  g_clear_object (&gimp->app);
  return result;
}
