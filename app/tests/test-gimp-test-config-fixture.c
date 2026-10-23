/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"

#include <string.h>
#include <gegl.h>
#include <gtk/gtk.h>

#include "core/core-types.h"
#include "core/gimp.h"
#include "config/gimprc.h"
#include "plug-in/plug-in-rc.h"
#include "tests.h"

static gchar *program;

static const gchar source_rc_text[] = "(undo-levels 54321)\n";
static const gchar source_cache_text[] = "source cache sentinel\n";
static const gchar custom_rc_text[] =
  "(undo-levels 42)\n"
  "(gradient-path \"${gimp_dir}/explicit-gradients\")\n"
  "(gradient-path-writable \"${gimp_dir}/explicit-writable\")\n"
  "(pluginrc-path \"${gimp_dir}/explicit-pluginrc\")\n";

static void
write_text (const gchar *path,
            const gchar *text)
{
  GError   *error = NULL;
  gboolean  written = g_file_set_contents (path, text, -1, &error);
  g_assert_no_error (error);
  g_assert_true (written);
}

static gchar *
read_text (const gchar *path)
{
  GError   *error = NULL;
  gchar    *text = NULL;
  gboolean  read = g_file_get_contents (path, &text, NULL, &error);
  g_assert_no_error (error);
  g_assert_true (read);
  return text;
}

static void
assert_text (const gchar *path,
             const gchar *expected)
{
  gchar *text = read_text (path);
  g_assert_cmpstr (text, ==, expected);
  g_free (text);
}

static int
config_child (const gchar *mode,
              const gchar *report)
{
  const gchar *source = g_getenv ("GIMP_TESTING_ABS_TOP_SRCDIR");
  const gchar *build = g_getenv ("GIMP_TESTING_ABS_TOP_BUILDDIR");
  gchar       *source_rc = g_build_filename (source, "app", "tests", "gimpdir", "gimprc", NULL);
  gchar       *source_cache = g_build_filename (source, "app", "tests", "gimpdir", "pluginrc", NULL);
  gchar       *custom_path = g_build_filename (source, "custom-system.gimprc", NULL);
  GFile       *source_file = g_file_new_for_path (source_rc);
  GFile       *user_file = NULL;
  GFile       *edit_file = NULL;
  Gimp        *gimp;
  gint         undo_levels;
  gchar       *path = NULL;
  gchar       *writable = NULL;
  gchar       *cache = NULL;

  g_assert_true (gtk_init_check (NULL, NULL));
  g_log_set_always_fatal (G_LOG_FATAL_MASK | G_LOG_LEVEL_WARNING | G_LOG_LEVEL_CRITICAL);

  if (g_str_equal (mode, "custom"))
    {
      GFile *custom = g_file_new_for_path (custom_path);
      GFile *system_file = NULL;

      gimp = gimp_init_for_gui_testing_with_rc (FALSE, custom);
      g_object_get (gimp->config, "system-gimprc", &system_file, NULL);
      g_assert_true (g_file_equal (custom, system_file));
      g_object_unref (system_file);
      g_object_unref (custom);
    }
  else
    {
      g_assert_cmpstr (mode, ==, "default");
      gimp = gimp_init_for_gui_testing (FALSE);
    }

  /* These public initializers intentionally stop before GUI activation. This
   * tests their real config boundary without replacing the 504-case GUI suite. */
  g_object_get (gimp->config,
                "user-gimprc", &user_file,
                "undo-levels", &undo_levels,
                "gradient-path", &path,
                "gradient-path-writable", &writable,
                "pluginrc-path", &cache,
                NULL);
  g_object_get (gimp->edit_config, "user-gimprc", &edit_file, NULL);
  g_assert_true (g_file_equal (user_file, edit_file));

  if (g_str_equal (mode, "custom"))
    {
      /* Existing semantics: alternate system RC, then source personal RC. */
      g_assert_true (g_file_equal (user_file, source_file));
      g_assert_cmpint (undo_levels, ==, 54321);
      g_assert_cmpstr (path, ==, "${gimp_dir}/explicit-gradients");
      g_assert_cmpstr (writable, ==, "${gimp_dir}/explicit-writable");
      g_assert_cmpstr (cache, ==, "${gimp_dir}/explicit-pluginrc");
      write_text (report, "custom-preserved\n");
    }
  else
    {
      GParamSpec *spec = g_object_class_find_property (G_OBJECT_GET_CLASS (gimp->config),
                                                     "undo-levels");
      gint        default_undo = g_value_get_int (g_param_spec_get_default_value (spec));
      gchar      *owned_path = g_file_get_path (user_file);
      gchar      *expected_path = g_build_filename (source, "data", "gradients", NULL);
      gchar      *complete = g_strconcat (expected_path, G_SEARCHPATH_SEPARATOR_S,
                                         "${gimp_dir}/gradients", NULL);
      gchar      *expected_cache = g_build_filename (build, "app", "tests",
                                                    "gimpdir-output", "pluginrc", NULL);
      GFile      *temp_root = g_file_new_for_path (g_getenv ("TMPDIR"));
      GFile      *cache_file = g_file_new_for_path (expected_cache);
      GFile      *cache_parent = g_file_get_parent (cache_file);
      GError     *error = NULL;
      gboolean    written;
      gchar      *saved;
      gchar      *needle;

      g_assert_false (g_file_equal (user_file, source_file));
      g_assert_true (g_file_has_prefix (user_file, temp_root));
      g_assert_cmpint (undo_levels, ==, default_undo);
      g_assert_cmpstr (path, ==, complete);
      g_assert_cmpstr (writable, ==, "${gimp_dir}/gradients");
      g_assert_cmpstr (cache, ==, expected_cache);
      assert_text (owned_path, "");

      /* Exercise actual RC autosave, including its queued idle, not a mocked
       * write. The existing layer-presets-ui tests cover Preferences itself. */
      gimp_rc_set_autosave (GIMP_RC (gimp->edit_config), TRUE);
      g_object_set (gimp->edit_config, "undo-levels", default_undo + 7, NULL);
      g_assert_cmpuint (GIMP_RC (gimp->edit_config)->save_idle_id, !=, 0);
      gimp_test_run_mainloop_until_idle ();
      g_assert_true (GIMP_RC (gimp->edit_config)->autosave);
      g_assert_cmpuint (GIMP_RC (gimp->edit_config)->save_idle_id, ==, 0);
      saved = read_text (owned_path);
      needle = g_strdup_printf ("(undo-levels %d)", default_undo + 7);
      g_assert_nonnull (strstr (saved, needle));
      g_free (needle);
      g_free (saved);

      /* The real cache writer must create the otherwise absent build parent.
       * No plug-ins or production files are changed by this empty cache. */
      g_assert_false (g_file_query_exists (cache_parent, NULL));
      written = plug_in_rc_write (NULL, cache_file, &error);
      g_assert_no_error (error);
      g_assert_true (written);
      g_assert_true (g_file_query_exists (cache_file, NULL));
      saved = read_text (expected_cache);
      g_assert_nonnull (strstr (saved, "protocol-version"));
      g_free (saved);
      write_text (report, owned_path);

      g_object_unref (cache_parent);
      g_object_unref (cache_file);
      g_object_unref (temp_root);
      g_free (expected_cache);
      g_free (complete);
      g_free (expected_path);
      g_free (owned_path);
    }

  assert_text (source_rc, source_rc_text);
  assert_text (source_cache, source_cache_text);
  assert_text (custom_path, custom_rc_text);
  g_object_unref (edit_file);
  g_object_unref (user_file);
  g_object_unref (source_file);
  g_free (cache);
  g_free (writable);
  g_free (path);
  g_free (custom_path);
  g_free (source_cache);
  g_free (source_rc);

  /* Deliberately retain Gimp and both RC objects across return from main.
   * The parent checks actual process-exit cleanup, not object finalization. */
  g_assert_true (GIMP_IS_GIMP (gimp));
  return 0;
}

static void
remove_owned_tree (GFile *file)
{
  GError *error = NULL;
  gboolean removed;

  if (g_file_query_file_type (file, G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, NULL) ==
      G_FILE_TYPE_DIRECTORY)
    {
      GFileEnumerator *children;
      GFileInfo       *info;
      gboolean        closed;
      children = g_file_enumerate_children (file, G_FILE_ATTRIBUTE_STANDARD_NAME,
                                            G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                            NULL, &error);
      g_assert_no_error (error);
      g_assert_nonnull (children);
      while ((info = g_file_enumerator_next_file (children, NULL, &error)))
        {
          GFile *child = g_file_get_child (file, g_file_info_get_name (info));
          remove_owned_tree (child);
          g_object_unref (child);
          g_object_unref (info);
        }
      g_assert_no_error (error);
      closed = g_file_enumerator_close (children, NULL, &error);
      g_assert_no_error (error);
      g_assert_true (closed);
      g_object_unref (children);
    }
  removed = g_file_delete (file, NULL, &error);
  g_assert_no_error (error);
  g_assert_true (removed);
}

static void
run_child (gconstpointer data)
{
  const gchar *mode = data;
  GError      *error = NULL;
  gchar       *root = g_dir_make_tmp ("gimp-config-boundary-test-XXXXXX", &error);
  gchar       *source;
  gchar       *profile;
  gchar       *build;
  gchar       *temporary;
  gchar       *sysconf;
  gchar       *report;
  gchar       *source_rc;
  gchar       *source_cache;
  gchar       *custom_rc;
  gchar       *output = NULL;
  gchar       *errors = NULL;
  gchar      **env;
  gchar       *child_argv[5];
  gint         status;
  gboolean     launched;
  gboolean     passed;
  GFile       *root_file;

  g_assert_no_error (error);
  g_assert_nonnull (root);
  source = g_build_filename (root, "source", NULL);
  profile = g_build_filename (source, "app", "tests", "gimpdir", NULL);
  build = g_build_filename (root, "build", NULL);
  temporary = g_build_filename (root, "tmp", NULL);
  sysconf = g_build_filename (root, "sysconf", NULL);
  report = g_build_filename (root, "child-report", NULL);
  source_rc = g_build_filename (profile, "gimprc", NULL);
  source_cache = g_build_filename (profile, "pluginrc", NULL);
  custom_rc = g_build_filename (source, "custom-system.gimprc", NULL);
  g_assert_cmpint (g_mkdir_with_parents (profile, 0700), ==, 0);
  g_assert_cmpint (g_mkdir_with_parents (build, 0700), ==, 0);
  g_assert_cmpint (g_mkdir_with_parents (temporary, 0700), ==, 0);
  g_assert_cmpint (g_mkdir_with_parents (sysconf, 0700), ==, 0);
  write_text (source_rc, source_rc_text);
  write_text (source_cache, source_cache_text);
  write_text (custom_rc, custom_rc_text);

  env = g_get_environ ();
  env = g_environ_setenv (env, "GIMP_TESTING_ABS_TOP_SRCDIR", source, TRUE);
  env = g_environ_setenv (env, "GIMP_TESTING_ABS_TOP_BUILDDIR", build, TRUE);
  env = g_environ_setenv (env, "GIMP3_SYSCONFDIR", sysconf, TRUE);
  env = g_environ_setenv (env, "TMPDIR", temporary, TRUE);
  child_argv[0] = program;
  child_argv[1] = "--config-fixture-child";
  child_argv[2] = (gchar *) mode;
  child_argv[3] = report;
  child_argv[4] = NULL;
  launched = g_spawn_sync (NULL, child_argv, env, G_SPAWN_DEFAULT, NULL, NULL,
                           &output, &errors, &status, &error);
  g_assert_no_error (error);
  g_assert_true (launched);
  if (output && *output)
    g_test_message ("Child output: %s", output);
  if (errors && *errors)
    g_printerr ("%s", errors);
  passed = g_spawn_check_wait_status (status, &error);
  g_assert_no_error (error);
  g_assert_true (passed);
  assert_text (source_rc, source_rc_text);
  assert_text (source_cache, source_cache_text);
  assert_text (custom_rc, custom_rc_text);

  if (g_str_equal (mode, "default"))
    {
      gchar *owned_path = read_text (report);
      GFile *owned_file = g_file_new_for_path (owned_path);
      GFile *temporary_root = g_file_new_for_path (temporary);
      gchar *cache = g_build_filename (build, "app", "tests", "gimpdir-output", "pluginrc", NULL);
      g_assert_true (g_file_has_prefix (owned_file, temporary_root));
      g_assert_false (g_file_query_exists (owned_file, NULL));
      g_assert_true (g_file_test (cache, G_FILE_TEST_IS_REGULAR));
      g_object_unref (temporary_root);
      g_object_unref (owned_file);
      g_free (cache);
      g_free (owned_path);
    }
  else
    {
      assert_text (report, "custom-preserved\n");
    }

  root_file = g_file_new_for_path (root);
  remove_owned_tree (root_file);
  g_object_unref (root_file);
  g_strfreev (env);
  g_free (errors);
  g_free (output);
  g_free (custom_rc);
  g_free (source_cache);
  g_free (source_rc);
  g_free (report);
  g_free (sysconf);
  g_free (temporary);
  g_free (build);
  g_free (profile);
  g_free (source);
  g_free (root);
}

int
main (int argc, char **argv)
{
  gint result;
  if (argc == 4 && g_str_equal (argv[1], "--config-fixture-child"))
    {
      g_set_prgname ("gimp-test-config-fixture-child");
      return config_child (argv[2], argv[3]);
    }

  program = g_find_program_in_path (argv[0]);
  g_assert_nonnull (program);
  g_test_init (&argc, &argv, NULL);
  g_test_add_data_func ("/gimp-test-config-fixture/default-isolation-autosave-retained-cleanup",
                        "default", run_child);
  g_test_add_data_func ("/gimp-test-config-fixture/explicit-custom-rc-preserved",
                        "custom", run_child);
  result = g_test_run ();
  g_free (program);
  return result;
}
