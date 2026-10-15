/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <gegl.h>
#include <gtk/gtk.h>

#include "libgimpbase/gimpbase.h"
#include "libgimpwidgets/gimpwidgets.h"

#include "core/core-types.h"
#include "core/gimp.h"
#include "config/gimprc.h"
#include "gui/themes.h"
#include "plug-in/plug-in-rc.h"
#include "tests.h"

static gchar *program;

static const gchar source_rc_text[] = "(undo-levels 54321)\n";
static const gchar source_cache_text[] = "source cache sentinel\n";
static const struct
{
  const gchar *name;
  const gchar *text;
} profile_fixtures[] = {
  { "sessionrc", "# source session fixture\n" },
  { "dockrc", "# source dock fixture\n" },
  { "tags.xml", "<?xml version=\"1.0\"?><tags/>\n" },
  { "sessionrc-alternate-\xce\xbc", "# alternate session fixture\n" },
  { "dockrc-alternate-\xe6\x97\xa5\xe6\x9c\xac", "# alternate dock fixture\n" },
  { "theme.css", "source theme sentinel\n" },
  { "profilerc", "source profile history sentinel\n" }
};
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

static void
check_private_profile (Gimp        *gimp,
                       const gchar *mode,
                       const gchar *report)
{
  const gchar  *directory = gimp_directory ();
  GFile        *profile = g_file_new_for_path (directory);
  GFile        *temporary = g_file_new_for_path (g_getenv ("TMPDIR"));
  GFile        *history = gimp_directory_file ("profilerc", NULL);
  GtkListStore *store;
  gchar        *profile_report = g_strconcat (report, "-profile", NULL);
  gchar        *theme = g_build_filename (directory, "theme.css", NULL);
  gboolean      explicit_profile = g_str_equal (mode, "core-explicit");
  gchar        *saved;
  guint         i;

  g_assert_cmpint (g_file_has_prefix (profile, temporary), ==,
                   ! explicit_profile);
  for (i = 0; i < G_N_ELEMENTS (profile_fixtures); i++)
    {
      gchar    *path = g_build_filename (directory, profile_fixtures[i].name, NULL);
      gboolean  expected = explicit_profile ? i == 2 :
        (i < 3 || (i < 5 && g_str_equal (mode, "named")));

      if (expected)
        assert_text (path, profile_fixtures[i].text);
      else
        g_assert_false (g_file_test (path, G_FILE_TEST_EXISTS));
      g_free (path);
    }

  if (g_str_equal (mode, "named"))
    {
      g_assert_cmpstr (g_getenv ("GIMP_TESTING_SESSIONRC_NAME"), ==,
                       profile_fixtures[3].name);
      g_assert_cmpstr (g_getenv ("GIMP_TESTING_DOCKRC_NAME"), ==,
                       profile_fixtures[4].name);
    }

  /* Real startup and widget writers must target the isolated profile. Neither
   * the generated theme nor the history sentinel from source is an input. */
  themes_init (gimp);
  saved = read_text (theme);
  g_assert_nonnull (strstr (saved, "GIMP theme.css"));
  g_free (saved);
  themes_exit (gimp);
  store = gimp_color_profile_store_new (history);
  g_object_unref (store);
  g_assert_true (g_file_query_exists (history, NULL));
  write_text (profile_report, directory);

  g_free (theme);
  g_free (profile_report);
  g_object_unref (history);
  g_object_unref (temporary);
  g_object_unref (profile);
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
  gboolean     core = g_str_equal (mode, "core") ||
                      g_str_equal (mode, "core-explicit");

  g_assert_true (gtk_init_check (NULL, NULL));
  g_log_set_always_fatal (G_LOG_FATAL_MASK | G_LOG_LEVEL_WARNING | G_LOG_LEVEL_CRITICAL);

  if (g_str_equal (mode, "named"))
    {
      g_setenv ("GIMP_TESTING_SESSIONRC_NAME", profile_fixtures[3].name, TRUE);
      g_setenv ("GIMP_TESTING_DOCKRC_NAME", profile_fixtures[4].name, TRUE);
    }

  if (core)
    {
      gchar *selected_profile = g_strdup (g_getenv ("GIMP3_DIRECTORY"));

      /* Use the real core-only route, including initialize and restore. This
       * is how core tests can subsequently expose profile-writing widgets. */
      gimp = gimp_init_for_testing ();
      g_assert_true (gimp->restored);
      if (g_str_equal (mode, "core-explicit"))
        g_assert_cmpstr (gimp_directory (), ==, selected_profile);
      else
        g_assert_cmpstr (gimp_directory (), !=, selected_profile);
      g_free (selected_profile);
    }
  else if (g_str_equal (mode, "custom"))
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
      g_assert_true (g_str_equal (mode, "default") ||
                     g_str_equal (mode, "repeat") ||
                     g_str_equal (mode, "named") ||
                     g_str_has_prefix (mode, "invalid-"));
      gimp = gimp_init_for_gui_testing (FALSE);
    }

  /* GUI cases stop before full activation; core cases complete restore. These
   * focused writer checks do not replace the full native GUI/session suite. */
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
      if (! core)
        g_assert_cmpint (g_file_query_exists (cache_parent, NULL), ==,
                         g_str_equal (mode, "repeat"));
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

  check_private_profile (gimp, mode, report);
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
run_child_with_invalid_name (const gchar *mode,
                             const gchar *invalid_name)
{
  GError      *error = NULL;
  gchar       *root = g_dir_make_tmp ("gimp-config-boundary-test-XXXXXX", &error);
  gchar       *source;
  gchar       *profile;
  gchar       *build;
  gchar       *temporary;
  gchar       *sysconf;
  gchar       *plugins;
  gchar       *explicit_profile;
  gchar       *explicit_marker;
  gchar       *source_escape;
  gchar       *escape_marker;
  gchar       *report;
  gchar       *profile_report;
  gchar       *previous_profile = NULL;
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
  guint        i;
  guint        run;

  g_assert_no_error (error);
  g_assert_nonnull (root);
  source = g_build_filename (root, "source", NULL);
  profile = g_build_filename (source, "app", "tests", "gimpdir", NULL);
  build = g_build_filename (root, "build", NULL);
  temporary = g_build_filename (root, "tmp", NULL);
  sysconf = g_build_filename (root, "sysconf", NULL);
  plugins = g_build_filename (root, "empty-plugins", NULL);
  explicit_profile = g_build_filename (root, "explicit-profile", NULL);
  explicit_marker = g_build_filename (explicit_profile, "keep-me", NULL);
  source_escape = g_build_filename (source, "app", "tests", "escaped-profile", NULL);
  escape_marker = g_build_filename (temporary, "escaped-profile", NULL);
  report = g_build_filename (root, "child-report", NULL);
  profile_report = g_strconcat (report, "-profile", NULL);
  source_rc = g_build_filename (profile, "gimprc", NULL);
  source_cache = g_build_filename (profile, "pluginrc", NULL);
  custom_rc = g_build_filename (source, "custom-system.gimprc", NULL);
  g_assert_cmpint (g_mkdir_with_parents (profile, 0700), ==, 0);
  g_assert_cmpint (g_mkdir_with_parents (build, 0700), ==, 0);
  g_assert_cmpint (g_mkdir_with_parents (temporary, 0700), ==, 0);
  g_assert_cmpint (g_mkdir_with_parents (sysconf, 0700), ==, 0);
  g_assert_cmpint (g_mkdir_with_parents (plugins, 0700), ==, 0);
  g_assert_cmpint (g_mkdir_with_parents (explicit_profile, 0700), ==, 0);
  write_text (source_rc, source_rc_text);
  write_text (source_cache, source_cache_text);
  write_text (custom_rc, custom_rc_text);
  write_text (explicit_marker, "caller-owned profile\n");
  /* Real core restore needs valid empty template/tag inputs. They are
   * test-owned fixtures, not diagnostics filtered out of the native run. */
  {
    gchar *etc = g_build_filename (source, "etc", NULL);
    gchar *templates = g_build_filename (etc, "templaterc", NULL);
    gchar *tags = g_build_filename (explicit_profile, "tags.xml", NULL);

    g_assert_cmpint (g_mkdir_with_parents (etc, 0700), ==, 0);
    write_text (templates, "# Empty test template list\n");
    write_text (tags, profile_fixtures[2].text);
    g_free (tags);
    g_free (templates);
    g_free (etc);
  }
  /* ../escaped-profile would read this real source and overwrite a file
   * outside a newly allocated profile if basename validation were missing. */
  write_text (source_escape, "must not escape into another profile\n");
  write_text (escape_marker, "outside profile sentinel\n");
  for (i = 0; i < G_N_ELEMENTS (profile_fixtures); i++)
    {
      gchar *path = g_build_filename (profile, profile_fixtures[i].name, NULL);
      write_text (path, profile_fixtures[i].text);
      g_free (path);
    }

  env = g_get_environ ();
  env = g_environ_setenv (env, "GIMP_TESTING_ABS_TOP_SRCDIR", source, TRUE);
  env = g_environ_setenv (env, "GIMP_TESTING_ABS_TOP_BUILDDIR", build, TRUE);
  env = g_environ_setenv (env, "GIMP3_SYSCONFDIR", sysconf, TRUE);
  env = g_environ_setenv (env, "GIMP3_DIRECTORY",
                          g_str_equal (mode, "core-explicit") ?
                          explicit_profile : profile, TRUE);
  /* This profile-boundary fixture needs no external plug-ins. Keep the real
   * core initializer independent of whatever plug-in build the parent uses. */
  env = g_environ_setenv (env, "GIMP_TESTING_PLUGINDIRS", plugins, TRUE);
  env = g_environ_setenv (env, "TMPDIR", temporary, TRUE);
  env = g_environ_unsetenv (env, "GIMP_TESTING_SESSIONRC_NAME");
  env = g_environ_unsetenv (env, "GIMP_TESTING_DOCKRC_NAME");
  if (invalid_name)
    env = g_environ_setenv (env,
                            g_str_equal (mode, "invalid-session") ?
                            "GIMP_TESTING_SESSIONRC_NAME" : "GIMP_TESTING_DOCKRC_NAME",
                            invalid_name, TRUE);
  child_argv[0] = program;
  child_argv[1] = "--config-fixture-child";
  child_argv[2] = (gchar *) mode;
  child_argv[3] = report;
  child_argv[4] = NULL;
  for (run = 0; run < (g_str_equal (mode, "default") ? 2 : 1); run++)
    {
      if (run > 0)
        child_argv[2] = "repeat";
      launched = g_spawn_sync (NULL, child_argv, env, G_SPAWN_DEFAULT, NULL, NULL,
                               &output, &errors, &status, &error);
      g_assert_no_error (error);
      g_assert_true (launched);
      if (output && *output)
        g_test_message ("Child output: %s", output);
      if (! invalid_name && errors && *errors)
        g_printerr ("%s", errors);
      passed = g_spawn_check_wait_status (status, &error);
      if (invalid_name)
        {
          GDir        *temporary_dir;
          const gchar *name;

          g_assert_false (passed);
          g_assert_error (error, G_SPAWN_EXIT_ERROR, EXIT_FAILURE);
          g_clear_error (&error);
          g_assert_nonnull (errors);
          g_assert_nonnull (strstr (errors, "Invalid test profile fixture basename"));
          g_assert_false (g_file_test (report, G_FILE_TEST_EXISTS));
          temporary_dir = g_dir_open (temporary, 0, &error);
          g_assert_no_error (error);
          g_assert_nonnull (temporary_dir);
          while ((name = g_dir_read_name (temporary_dir)))
            g_assert_false (g_str_has_prefix (name, "gimp-app-test-profile-"));
          g_dir_close (temporary_dir);
        }
      else
        {
          g_assert_no_error (error);
          g_assert_true (passed);
        }
      assert_text (source_rc, source_rc_text);
      assert_text (source_cache, source_cache_text);
      assert_text (custom_rc, custom_rc_text);
      assert_text (explicit_marker, "caller-owned profile\n");
      {
        gchar *tags = g_build_filename (explicit_profile, "tags.xml", NULL);
        assert_text (tags, profile_fixtures[2].text);
        g_free (tags);
      }
      assert_text (source_escape, "must not escape into another profile\n");
      assert_text (escape_marker, "outside profile sentinel\n");
      for (i = 0; i < G_N_ELEMENTS (profile_fixtures); i++)
        {
          gchar *path = g_build_filename (profile, profile_fixtures[i].name, NULL);
          assert_text (path, profile_fixtures[i].text);
          g_free (path);
        }

      if (invalid_name)
        {
          g_clear_pointer (&errors, g_free);
          g_clear_pointer (&output, g_free);
          continue;
        }

      {
        gchar *owned_profile = read_text (profile_report);
        GFile *owned_file = g_file_new_for_path (owned_profile);
        GFile *temporary_root = g_file_new_for_path (temporary);
        if (g_str_equal (mode, "core-explicit"))
          {
            gchar *history = g_build_filename (explicit_profile, "profilerc", NULL);
            gchar *theme = g_build_filename (explicit_profile, "theme.css", NULL);

            g_assert_cmpstr (owned_profile, ==, explicit_profile);
            g_assert_true (g_file_query_exists (owned_file, NULL));
            g_assert_true (g_file_test (history, G_FILE_TEST_IS_REGULAR));
            g_assert_true (g_file_test (theme, G_FILE_TEST_IS_REGULAR));
            g_free (theme);
            g_free (history);
          }
        else
          {
            g_assert_true (g_file_has_prefix (owned_file, temporary_root));
            g_assert_false (g_file_query_exists (owned_file, NULL));
          }
        if (previous_profile)
          g_assert_cmpstr (owned_profile, !=, previous_profile);
        g_free (previous_profile);
        previous_profile = owned_profile;
        g_object_unref (temporary_root);
        g_object_unref (owned_file);
      }

      if (! g_str_equal (mode, "custom"))
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
      g_clear_pointer (&errors, g_free);
      g_clear_pointer (&output, g_free);
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
  g_free (previous_profile);
  g_free (profile_report);
  g_free (report);
  g_free (escape_marker);
  g_free (source_escape);
  g_free (explicit_marker);
  g_free (explicit_profile);
  g_free (plugins);
  g_free (sysconf);
  g_free (temporary);
  g_free (build);
  g_free (profile);
  g_free (source);
  g_free (root);
}

static void
run_child (gconstpointer data)
{
  run_child_with_invalid_name (data, NULL);
}

static void
invalid_fixture_names (gconstpointer data)
{
  const gchar *invalid_names[] = {
    "", ".", "..", "../escaped-profile", "nested/sessionrc",
    "..\\escaped-profile", "\\absolute-profile", "/absolute-profile",
    "C:sessionrc", "C:\\sessionrc", "sessionrc:stream",
    "NUL", "nul.txt", "CoN", "PRN.log", "AUX", "COM1", "lpt9.data",
    "COM\xc2\xb9", "LPT\xc2\xb2.txt", "COM\xc2\xb3.log",
    "CONIN$", "CONOUT$.txt", "CLOCK$", "NUL .txt",
    "sessionrc.", "sessionrc ", "session?rc", "bad|name", "name\n"
  };
  guint i;

  for (i = 0; i < G_N_ELEMENTS (invalid_names); i++)
    {
      g_test_message ("Rejecting fixture basename: '%s'", invalid_names[i]);
      run_child_with_invalid_name (data, invalid_names[i]);
    }
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
  g_test_add_data_func ("/gimp-test-config-fixture/named-session-dock-fixtures-preserved",
                        "named", run_child);
  g_test_add_data_func ("/gimp-test-config-fixture/core-initializer-profile-writers-isolated",
                        "core", run_child);
  g_test_add_data_func ("/gimp-test-config-fixture/core-explicit-profile-preserved",
                        "core-explicit", run_child);
  g_test_add_data_func ("/gimp-test-config-fixture/reject-invalid-session-basenames",
                        "invalid-session", invalid_fixture_names);
  g_test_add_data_func ("/gimp-test-config-fixture/reject-invalid-dock-basenames",
                        "invalid-dock", invalid_fixture_names);
  result = g_test_run ();
  g_free (program);
  return result;
}
