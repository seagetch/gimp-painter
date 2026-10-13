/* GIMP - The GNU Image Manipulation Program
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

#include "config.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include <glib-unix.h>
#include <libgimp/gimp.h>
#include <libgimpbase/gimpprotocol.h>
#include <libgimpbase/gimpwire.h>


#define MAIN_NAME "test-protocol-main"
#define TEMP_NAME "test-protocol-temporary"

typedef struct
{
  const gchar *scenario;
  GString    *observations;
  GPid        pid;
  GIOChannel *read_channel;
  GIOChannel *write_channel;
  gint        events;
  gint        release;
  gint        output;
  gint        errors;
} Child;

static const gchar *fixture_path;
static gboolean     expect_redundant_quit;

static void
close_fd (gint fd)
{
  g_assert_cmpint (close (fd), ==, 0);
}

static GIOChannel *
channel_new (gint fd)
{
  GIOChannel *channel = g_io_channel_unix_new (fd);
  GError     *error = NULL;

  g_assert_cmpint (g_io_channel_set_encoding (channel, NULL, &error), ==,
                   G_IO_STATUS_NORMAL);
  g_assert_no_error (error);
  g_io_channel_set_buffered (channel, FALSE);
  g_io_channel_set_close_on_unref (channel, TRUE);
  return channel;
}

static gboolean
host_flush (GIOChannel *channel,
             gpointer    user_data)
{
  GError *error = NULL;
  GIOStatus status = g_io_channel_flush (channel, &error);

  g_assert_no_error (error);
  return status == G_IO_STATUS_NORMAL;
}

static Child
child_start (const gchar *scenario,
              const gchar *mode)
{
  Child        child = { 0, };
  gint         to_child[2];
  gint         from_child[2];
  gint         events[2];
  gint         release[2];
  gint         source_fds[4];
  const gint   target_fds[] = { 3, 4, 5, 6 };
  gchar       *version = g_strdup_printf ("%d", GIMP_PROTOCOL_VERSION);
  const gchar *argv[] = { fixture_path, "-gimp", version, "3", "4", mode,
                          "0", NULL };
  gchar      **env = g_get_environ ();
  GError      *error = NULL;

  /* A failure timeout only. All meaningful ordering uses pipe events. */
  alarm (30);
  g_assert_true (g_unix_open_pipe (to_child, FD_CLOEXEC, &error));
  g_assert_no_error (error);
  g_assert_true (g_unix_open_pipe (from_child, FD_CLOEXEC, &error));
  g_assert_no_error (error);
  g_assert_true (g_unix_open_pipe (events, FD_CLOEXEC, &error));
  g_assert_no_error (error);
  g_assert_true (g_unix_open_pipe (release, FD_CLOEXEC, &error));
  g_assert_no_error (error);

  source_fds[0] = to_child[0];
  source_fds[1] = from_child[1];
  source_fds[2] = events[1];
  source_fds[3] = release[0];
  env = g_environ_setenv (env, "GIMP_TEST_PROTOCOL_SCENARIO", scenario, TRUE);
  env = g_environ_setenv (env, "LC_ALL", "C", TRUE);
  /* Ordinary fixtures preserve the gate's fatal-warning policy. One named
   * supplemental failure control deliberately observes the existing recovery
   * path after the first failed flush. No log handler suppresses diagnostics;
   * finish_child validates complete stderr and the exact expected termination.
   */
  env = g_environ_setenv
    (env, "G_DEBUG",
     g_str_equal (scenario, "failed-main-write-recoverable") ?
     "fatal-criticals" : "fatal-warnings", TRUE);
  env = g_environ_unsetenv (env, "G_MESSAGES_DEBUG");

  g_assert_true (g_spawn_async_with_pipes_and_fds
                 (NULL, argv, (const gchar * const *) env,
                  G_SPAWN_DO_NOT_REAP_CHILD, NULL, NULL,
                  -1, -1, -1, source_fds, target_fds,
                  G_N_ELEMENTS (target_fds), &child.pid, NULL,
                  &child.output, &child.errors, &error));
  g_assert_no_error (error);
  g_strfreev (env);
  g_free (version);

  /* Only this host keeps a reader of the child's transport. In particular,
   * the exec'd fixture receives no read-end duplicate that could mask EPIPE.
   */
  close_fd (to_child[0]);
  close_fd (from_child[1]);
  close_fd (events[1]);
  close_fd (release[0]);
  child.scenario      = scenario;
  child.observations  = g_string_new (scenario);
  g_string_append_c (child.observations, '\n');
  child.read_channel  = channel_new (from_child[0]);
  child.write_channel = channel_new (to_child[1]);
  child.events        = events[0];
  child.release       = release[1];
  gimp_wire_clear_error ();
  return child;
}

static void
expect_event (Child *child,
               gchar  expected)
{
  gchar   actual;
  ssize_t result;

  do
    result = read (child->events, &actual, 1);
  while (result == -1 && errno == EINTR);
  g_assert_cmpint (result, ==, 1);
  g_string_append_printf (child->observations, "event %c\n", actual);
  g_assert_cmpint (actual, ==, expected);
}

static void
release_barrier (Child *child)
{
  const gchar release = 'R';
  ssize_t     result;

  do
    result = write (child->release, &release, 1);
  while (result == -1 && errno == EINTR);
  g_assert_cmpint (result, ==, 1);
  g_string_append (child->observations, "release R\n");
}

static GimpWireMessage
read_message (Child *child,
               guint  expected)
{
  GimpWireMessage message = { 0, };

  g_assert_true (gimp_wire_read_msg (child->read_channel, &message, NULL));
  g_string_append_printf (child->observations, "wire type %u\n", message.type);
  g_assert_cmpuint (message.type, ==, expected);
  return message;
}

static void
expect_empty_message (Child *child,
                       guint  expected)
{
  GimpWireMessage message = read_message (child, expected);

  g_assert_null (message.data);
  gimp_wire_destroy (&message);
}

static void
send_config (Child *child)
{
  GPConfig     config = { 0, };
  const guint8 pixel[] = { 128, 128, 128, 255 };
  const gchar *icc;
  gint         icc_length;

  /* Exercise the real GP_CONFIG path without a display, shared memory or
   * external PDB dependencies. Supply an actual sRGB profile and pixel data.
   */
  icc = babl_space_get_icc (babl_space ("sRGB"), &icc_length);
  g_assert_nonnull (icc);
  g_assert_cmpint (icc_length, >, 0);
  config.tile_width             = 64;
  config.tile_height            = 64;
  config.shm_id                 = -1;
  config.check_size             = GIMP_CHECK_SIZE_MEDIUM_CHECKS;
  config.check_type             = GIMP_CHECK_TYPE_GRAY_CHECKS;
  config.default_display_id     = -1;
  config.app_name               = "GIMP protocol test";
  config.wm_class               = "gimp-protocol-test";
  config.swap_path              = (gchar *) g_get_tmp_dir ();
  config.swap_compression       = "none";
  config.num_processors         = 1;
  config.tile_cache_size        = 1024 * 1024;
  config.check_custom_color1    = g_bytes_new_static (pixel, sizeof pixel);
  config.check_custom_color2    = g_bytes_ref (config.check_custom_color1);
  config.check_custom_icc1      = g_bytes_new_static (icc, icc_length);
  config.check_custom_icc2      = g_bytes_ref (config.check_custom_icc1);
  config.check_custom_encoding1 = "R'G'B'A u8";
  config.check_custom_encoding2 = "R'G'B'A u8";
  g_assert_true (gp_config_write (child->write_channel, &config, NULL));
  g_bytes_unref (config.check_custom_color1);
  g_bytes_unref (config.check_custom_color2);
  g_bytes_unref (config.check_custom_icc1);
  g_bytes_unref (config.check_custom_icc2);
}

static void
send_main_run (Child *child)
{
  GPProcRun run = { MAIN_NAME, 0, NULL };

  send_config (child);
  g_assert_true (gp_proc_run_write (child->write_channel, &run, NULL));
}

static void
expect_return (Child             *child,
                 guint              type,
                 const gchar       *name,
                 GimpPDBStatusType  status)
{
  GimpWireMessage message = read_message (child, type);
  GPProcReturn   *result = message.data;

  g_assert_nonnull (result);
  g_assert_cmpstr (result->name, ==, name);
  g_assert_cmpuint (result->n_params, ==,
                    status == GIMP_PDB_SUCCESS ? 1 : 2);
  g_assert_cmpint (result->params[0].param_type, ==, GP_PARAM_TYPE_INT);
  g_assert_cmpstr (result->params[0].type_name, ==, "GimpPDBStatusType");
  g_assert_cmpint (result->params[0].data.d_int, ==, status);
  if (status != GIMP_PDB_SUCCESS)
    {
      g_assert_cmpint (result->params[1].param_type, ==, GP_PARAM_TYPE_STRING);
      g_assert_cmpstr (result->params[1].data.d_string, ==,
                       "intentional protocol error return");
    }
  g_string_append_printf (child->observations,
                          "validated return type=%u name=%s status=%d params=%u\n",
                          type, name, status, result->n_params);
  gimp_wire_destroy (&message);
}

static GByteArray *
drain_fd (gint fd)
{
  GByteArray *bytes = g_byte_array_new ();
  guint8      buffer[4096];
  ssize_t     result;

  do
    {
      do
        result = read (fd, buffer, sizeof buffer);
      while (result == -1 && errno == EINTR);
      g_assert_cmpint (result, >=, 0);
      if (result > 0)
        g_byte_array_append (bytes, buffer, result);
      g_assert_cmpuint (bytes->len, <=, 1024 * 1024);
    }
  while (result > 0);
  return bytes;
}

static void
save_evidence (Child        *child,
                const gchar  *suffix,
                GByteArray   *bytes)
{
  const gchar *directory = g_getenv ("GIMP_TEST_PROTOCOL_LOG_DIR");

  if (directory)
    {
      gchar  *name = g_strconcat (child->scenario, suffix, NULL);
      gchar  *path = g_build_filename (directory, name, NULL);
      GError *error = NULL;

      g_assert_true (g_path_is_absolute (directory));
      g_assert_true (g_file_test (directory, G_FILE_TEST_IS_DIR));
      g_assert_true (g_file_set_contents (path,
                                          bytes->len ? (const gchar *) bytes->data : "",
                                          bytes->len, &error));
      g_assert_no_error (error);
      g_free (path);
      g_free (name);
    }
}

static void
check_diagnostics (GByteArray *bytes,
                    guint       expected,
                    gboolean    recoverable)
{
  gchar  **lines;
  guint    count = 0;
  guint    i;

  /* Nothing is permitted except the exact known diagnostic in the explicit
   * fault controls. A sanitizer report, unrelated warning, or extra message
   * is an error even when the process exits successfully.
   */
  if (bytes->len > 0)
    g_assert_null (memchr (bytes->data, '\0', bytes->len));
  g_byte_array_append (bytes, (const guint8 *) "", 1);
  lines = g_strsplit ((const gchar *) bytes->data, "\n", -1);
  for (i = 0; lines[i]; i++)
    {
      if (! *lines[i])
        continue;
      if (recoverable ?
          ! g_regex_match_simple
              ("^\\(plug-in-protocol-fixture:[0-9]+\\): LibGimp-WARNING \\*\\*: "
               "[0-9:.]+: plug-in-protocol-fixture: gimp_flush\\(\\): error: Broken pipe$",
               lines[i], 0, 0) :
          ! g_str_equal
              (lines[i],
               "Plugin plug-in-protocol-fixture: LibGimp: WARNING: "
               "plug-in-protocol-fixture: gimp_flush(): error: Broken pipe"))
        g_error ("Unexpected child diagnostic: %s", lines[i]);
      count++;
    }
  if (count != expected)
    g_test_message ("Child stderr:\n%s", bytes->data);
  g_assert_cmpuint (count, ==, expected);
  g_strfreev (lines);
}

static void
finish_child (Child    *child,
                gboolean  expect_quit,
                guint     expected_diagnostics,
                gboolean  fatal_warning,
                gboolean  quit_hook_completed)
{
  GByteArray *errors;
  GByteArray *output;
  GByteArray *events;
  GByteArray *wire;
  GByteArray *details;
  gboolean    reader_open = child->read_channel != NULL;
  gint        status;
  pid_t       waited;

  do
    waited = waitpid (child->pid, &status, 0);
  while (waited == -1 && errno == EINTR);
  g_assert_cmpint (waited, ==, child->pid);
  g_spawn_close_pid (child->pid);

  /* Capture everything before asserting the expected result. In particular,
   * intentionally running the repaired contract against baseline must retain
   * the exact diagnostic, fatal status, verified return, and barrier history.
   */
  errors = drain_fd (child->errors);
  output = drain_fd (child->output);
  events = drain_fd (child->events);
  wire = reader_open ?
           drain_fd (g_io_channel_unix_get_fd (child->read_channel)) :
           g_byte_array_new ();
  g_string_append_printf (child->observations,
                          "wait-status=%d exited=%d exit=%d signaled=%d signal=%d reader-open=%d\n",
                          status, WIFEXITED (status),
                          WIFEXITED (status) ? WEXITSTATUS (status) : -1,
                          WIFSIGNALED (status),
                          WIFSIGNALED (status) ? WTERMSIG (status) : -1,
                          reader_open);
  details = g_byte_array_new ();
  g_byte_array_append (details, (const guint8 *) child->observations->str,
                       child->observations->len);
  save_evidence (child, ".stderr", errors);
  save_evidence (child, ".stdout", output);
  save_evidence (child, ".events", events);
  save_evidence (child, ".tail-wire", wire);
  save_evidence (child, ".observations", details);

  g_clear_pointer (&child->read_channel, g_io_channel_unref);
  g_clear_pointer (&child->write_channel, g_io_channel_unref);
  close_fd (child->events);
  close_fd (child->release);
  close_fd (child->output);
  close_fd (child->errors);
  alarm (0);

  check_diagnostics (errors, expected_diagnostics,
                     g_str_equal (child->scenario, "failed-main-write-recoverable"));

  if (fatal_warning)
    {
      /* gimp_main() installs gimp_fatal_func(), which prints the canonical
       * plug-in-prefixed diagnostic and exits before GLib's default trap.
       */
      g_assert_true (WIFEXITED (status));
      g_assert_cmpint (WEXITSTATUS (status), ==, EXIT_FAILURE);
    }
  else
    {
      g_assert_true (WIFEXITED (status));
      /* Existing gimp_quit() also exits zero in the recoverable error control. */
      g_assert_cmpint (WEXITSTATUS (status), ==, 0);
    }
  if (reader_open)
    {
      g_assert_cmpuint (wire->len, ==, expect_quit ? sizeof (guint32) : 0);
      if (expect_quit)
        {
          guint32 type;

          memcpy (&type, wire->data, sizeof type);
          g_assert_cmpuint (GUINT32_FROM_BE (type), ==, GP_QUIT);
        }
    }
  else
    g_assert_false (expect_quit);

  g_assert_cmpuint (events->len, ==, quit_hook_completed ? 1 : 0);
  if (quit_hook_completed)
    g_assert_cmpint (events->data[0], ==, 'q');
  g_assert_cmpuint (output->len, ==, 0);
  g_byte_array_unref (errors);
  g_byte_array_unref (output);
  g_byte_array_unref (events);
  g_byte_array_unref (wire);
  g_byte_array_unref (details);
  g_string_free (child->observations, TRUE);
}

static void
test_nonreturn_exit (gconstpointer data)
{
  const gchar *scenario = data;
  const gchar *mode = "-run";
  Child        child;

  if (g_str_equal (scenario, "query"))
    mode = "-query";
  else if (g_str_equal (scenario, "init"))
    mode = "-init";
  child = child_start (scenario, mode);

  if (g_str_equal (scenario, "query"))
    {
      expect_empty_message (&child, GP_HAS_INIT);
      expect_event (&child, 'Y');
    }
  else if (g_str_equal (scenario, "init"))
    expect_event (&child, 'I');
  else if (g_str_equal (scenario, "host-quit"))
    g_assert_true (gp_quit_write (child.write_channel, NULL));
  else
    send_main_run (&child);

  expect_event (&child, 'Q');
  release_barrier (&child);
  finish_child (&child, TRUE, 0, FALSE, TRUE);
}

static void
test_main_return (gconstpointer data)
{
  const gchar       *scenario = data;
  gboolean           close_reader = g_str_has_suffix (scenario, "-close");
  GimpPDBStatusType  status = g_str_has_prefix (scenario, "error-") ?
                              GIMP_PDB_EXECUTION_ERROR : GIMP_PDB_SUCCESS;
  Child              child = child_start (scenario, "-run");

  send_main_run (&child);
  expect_return (&child, GP_PROC_RETURN, MAIN_NAME, status);
  expect_event (&child, 'Q');
  if (close_reader)
    {
      g_clear_pointer (&child.read_channel, g_io_channel_unref);
      g_string_append (child.observations, "closed sole reader after main return\n");
    }
  release_barrier (&child);
  finish_child (&child,
                expect_redundant_quit && ! close_reader,
                expect_redundant_quit && close_reader ? 1 : 0,
                expect_redundant_quit && close_reader, TRUE);
}

static void
test_failed_main_write (gconstpointer data)
{
  const gchar *scenario = data;
  gboolean     recoverable = g_str_equal (scenario, "failed-main-write-recoverable");
  Child        child = child_start (scenario, "-run");

  send_main_run (&child);
  expect_event (&child, 'B');
  g_clear_pointer (&child.read_channel, g_io_channel_unref);
  g_string_append (child.observations, "closed sole reader before main return\n");
  release_barrier (&child);
  if (recoverable)
    {
      expect_event (&child, 'Q');
      release_barrier (&child);
      /* The failed result flush remains diagnosed. Since it did not succeed,
       * the early-exit GP_QUIT path is retained and its flush also fails.
       */
      finish_child (&child, FALSE, 2, FALSE, TRUE);
    }
  else
    {
      /* Fatal logging stops at the genuine return-flush error, before the quit
       * callback can run. Neither baseline nor repaired code may hide it.
       */
      finish_child (&child, FALSE, 1, TRUE, FALSE);
    }
}

static void
expect_temporary_install (Child *child)
{
  GimpWireMessage message = read_message (child, GP_PROC_INSTALL);
  GPProcInstall  *install = message.data;
  GPProcRun      *request;
  GPParam         status = { 0, };
  GPProcReturn    reply = { 0, };

  g_assert_cmpstr (install->name, ==, TEMP_NAME);
  g_assert_cmpuint (install->type, ==, GIMP_PDB_PROC_TYPE_TEMPORARY);
  g_assert_cmpuint (install->n_params, ==, 0);
  g_assert_cmpuint (install->n_return_vals, ==, 0);
  gimp_wire_destroy (&message);

  /* Normal installation sets the sensitivity mask through the PDB. Service
   * exactly that request; never accept arbitrary unexpected wire messages.
   */
  message = read_message (child, GP_PROC_RUN);
  request = message.data;
  g_assert_cmpstr (request->name, ==, "gimp-pdb-set-proc-sensitivity-mask");
  g_assert_cmpuint (request->n_params, ==, 2);
  g_assert_cmpint (request->params[0].param_type, ==, GP_PARAM_TYPE_STRING);
  g_assert_cmpstr (request->params[0].data.d_string, ==, TEMP_NAME);
  g_assert_cmpint (request->params[1].param_type, ==, GP_PARAM_TYPE_INT);
  status.param_type = GP_PARAM_TYPE_INT;
  status.type_name  = "GimpPDBStatusType";
  status.data.d_int = GIMP_PDB_SUCCESS;
  reply.name       = request->name;
  reply.n_params   = 1;
  reply.params     = &status;
  g_assert_true (gp_proc_return_write (child->write_channel, &reply, NULL));
  gimp_wire_destroy (&message);
  expect_empty_message (child, GP_EXTENSION_ACK);
}

static void
test_temporary_return (gconstpointer data)
{
  const gchar *scenario = data;
  gboolean     quit_before_main = g_str_equal (scenario, "temporary-quit");
  GPProcRun    run = { TEMP_NAME, 0, NULL };
  Child        child = child_start (scenario, "-run");

  send_main_run (&child);
  expect_temporary_install (&child);
  g_assert_true (gp_temp_proc_run_write (child.write_channel, &run, NULL));
  expect_return (&child, GP_TEMP_PROC_RETURN, TEMP_NAME, GIMP_PDB_SUCCESS);
  expect_event (&child, 'T');
  release_barrier (&child);
  if (! quit_before_main)
    expect_return (&child, GP_PROC_RETURN, MAIN_NAME, GIMP_PDB_SUCCESS);
  expect_event (&child, 'Q');
  release_barrier (&child);
  finish_child (&child, quit_before_main || expect_redundant_quit, 0, FALSE, TRUE);
}

int
main (int    argc,
      char **argv)
{
  gint result;

  g_test_init (&argc, &argv, NULL);
  fixture_path = g_getenv ("GIMP_TEST_PROTOCOL_FIXTURE");
  g_assert_nonnull (fixture_path);
  g_assert_true (g_path_is_absolute (fixture_path));
  g_assert_true (g_file_test (fixture_path, G_FILE_TEST_IS_EXECUTABLE));
  expect_redundant_quit =
    g_strcmp0 (g_getenv ("GIMP_TEST_EXPECT_REDUNDANT_QUIT"), "1") == 0;
  signal (SIGPIPE, SIG_IGN);
  babl_init ();
  gp_init ();
  gimp_wire_set_flusher (host_flush);

  g_test_add_data_func ("/plug-in-protocol/query", "query", test_nonreturn_exit);
  g_test_add_data_func ("/plug-in-protocol/init", "init", test_nonreturn_exit);
  g_test_add_data_func ("/plug-in-protocol/host-quit", "host-quit", test_nonreturn_exit);
  g_test_add_data_func ("/plug-in-protocol/explicit-quit", "explicit-quit", test_nonreturn_exit);
  g_test_add_data_func ("/plug-in-protocol/success-open", "success-open", test_main_return);
  g_test_add_data_func ("/plug-in-protocol/success-close", "success-close", test_main_return);
  g_test_add_data_func ("/plug-in-protocol/error-open", "error-open", test_main_return);
  g_test_add_data_func ("/plug-in-protocol/error-close", "error-close", test_main_return);
  g_test_add_data_func ("/plug-in-protocol/failed-main-write", "failed-main-write", test_failed_main_write);
  g_test_add_data_func ("/plug-in-protocol/failed-main-write-recoverable", "failed-main-write-recoverable", test_failed_main_write);
  g_test_add_data_func ("/plug-in-protocol/temporary-quit", "temporary-quit", test_temporary_return);
  g_test_add_data_func ("/plug-in-protocol/temporary-main", "temporary-main", test_temporary_return);

  result = g_test_run ();
  babl_exit ();
  return result;
}
