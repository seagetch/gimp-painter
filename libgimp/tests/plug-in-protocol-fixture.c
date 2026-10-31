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
#include <stdlib.h>
#include <unistd.h>

#include <libgimp/gimp.h>


/* These auxiliary descriptors are independent of the real GIMP transport.
 * The host maps them with g_spawn_async_with_pipes_and_fds().
 */
#define EVENT_FD   5
#define RELEASE_FD 6
#define TEMP_NAME  "test-protocol-temporary"

typedef struct _ProtocolFixture      ProtocolFixture;
typedef struct _ProtocolFixtureClass ProtocolFixtureClass;

struct _ProtocolFixture
{
  GimpPlugIn parent_instance;
};

struct _ProtocolFixtureClass
{
  GimpPlugInClass parent_class;
};

GType protocol_fixture_get_type (void) G_GNUC_CONST;

G_DEFINE_TYPE (ProtocolFixture, protocol_fixture, GIMP_TYPE_PLUG_IN)

static guint plug_ins_finalized;
static guint procedures_created;
static guint procedures_finalized;
static guint reentrant_disposes_returned;

static void
count_finalized (gpointer data)
{
  guint *count = data;

  (*count)++;
}

static void
count_procedure_finalized (gpointer data)
{
  count_finalized (data);

  if (procedures_finalized == procedures_created)
    {
      GimpPlugIn *plug_in = gimp_get_plug_in ();

      /* The last retained procedure is finalized during plug-in disposal.
       * Reenter that disposal synchronously, without another IPC scenario.
       */
      g_assert_nonnull (plug_in);
      g_object_run_dispose (G_OBJECT (plug_in));
      reentrant_disposes_returned++;
    }
}

static GimpProcedure *
track_procedure (GimpProcedure *procedure)
{
  g_assert_nonnull (procedure);
  procedures_created++;
  g_object_set_data_full (G_OBJECT (procedure), "protocol-finalized-counter",
                          &procedures_finalized, count_procedure_finalized);

  return procedure;
}

static void
send_event (gchar event)
{
  ssize_t result;

  do
    result = write (EVENT_FD, &event, 1);
  while (result == -1 && errno == EINTR);

  if (result != 1)
    _exit (90);
}

static void
barrier (gchar event)
{
  gchar   release;
  ssize_t result;

  send_event (event);
  do
    result = read (RELEASE_FD, &release, 1);
  while (result == -1 && errno == EINTR);

  if (result != 1 || release != 'R')
    _exit (91);
}

static void
fixture_quit (GimpPlugIn *plug_in)
{
  /* The native library has already flushed the main result before invoking
   * this existing public vfunc. The host can close its sole transport reader
   * here, without any production test hook, scheduling guess, or delay.
   */
  barrier ('Q');
  send_event ('q');
}

static GList *
fixture_query (GimpPlugIn *plug_in)
{
  send_event ('Y');
  return NULL;
}

static GList *
fixture_init_procedures (GimpPlugIn *plug_in)
{
  send_event ('I');
  return NULL;
}

static GimpValueArray *
temporary_run (GimpProcedure       *procedure,
               GimpProcedureConfig *config,
               gpointer             run_data)
{
  return gimp_procedure_new_return_values (procedure, GIMP_PDB_SUCCESS, NULL);
}

static GimpValueArray *
fixture_run (GimpProcedure       *procedure,
             GimpProcedureConfig *config,
             gpointer             run_data)
{
  const gchar *scenario = g_getenv ("GIMP_TEST_PROTOCOL_SCENARIO");

  if (g_strcmp0 (scenario, "explicit-quit") == 0)
    gimp_quit ();

  if (g_str_has_prefix (scenario, "failed-main-write"))
    barrier ('B');

  if (g_str_has_prefix (scenario, "temporary-"))
    {
      GimpPlugIn    *plug_in = gimp_procedure_get_plug_in (procedure);
      GimpProcedure *temporary;

      temporary = track_procedure
        (gimp_procedure_new (plug_in, TEMP_NAME,
                             GIMP_PDB_PROC_TYPE_TEMPORARY,
                             temporary_run, NULL, NULL));
      gimp_plug_in_add_temp_procedure (plug_in, temporary);
      g_object_unref (temporary);
      gimp_procedure_persistent_ready (procedure);
      gimp_plug_in_persistent_process (plug_in, 0);

      /* Reaching T proves control returned to the main procedure after the
       * temporary return. An early quit here must still emit GP_QUIT.
       */
      barrier ('T');
      if (g_strcmp0 (scenario, "temporary-quit") == 0)
        gimp_quit ();
    }

  if (g_str_has_prefix (scenario, "error-"))
    return gimp_procedure_new_return_values
      (procedure, GIMP_PDB_EXECUTION_ERROR,
       g_error_new_literal (GIMP_PLUG_IN_ERROR, 0,
                            "intentional protocol error return"));

  return gimp_procedure_new_return_values (procedure, GIMP_PDB_SUCCESS, NULL);
}

static GimpProcedure *
fixture_create_procedure (GimpPlugIn  *plug_in,
                          const gchar *name)
{
  const gchar     *scenario = g_getenv ("GIMP_TEST_PROTOCOL_SCENARIO");
  GimpPDBProcType  type = GIMP_PDB_PROC_TYPE_PLUGIN;

  if (g_strcmp0 (name, "test-protocol-main") != 0)
    return NULL;

  if (g_str_has_prefix (scenario, "temporary-"))
    type = GIMP_PDB_PROC_TYPE_PERSISTENT;

  return track_procedure
    (gimp_procedure_new (plug_in, name, type, fixture_run, NULL, NULL));
}

static void
protocol_fixture_class_init (ProtocolFixtureClass *klass)
{
  GimpPlugInClass *plug_in_class = GIMP_PLUG_IN_CLASS (klass);

  plug_in_class->query_procedures = fixture_query;
  plug_in_class->init_procedures  = fixture_init_procedures;
  plug_in_class->create_procedure = fixture_create_procedure;
  plug_in_class->set_i18n         = NULL;
  plug_in_class->quit             = fixture_quit;
}

static void
protocol_fixture_init (ProtocolFixture *fixture)
{
  g_object_set_data_full (G_OBJECT (fixture), "protocol-finalized-counter",
                          &plug_ins_finalized, count_finalized);
}

int
main (int    argc,
      char **argv)
{
  /* Fixture failures cannot strand the suite indefinitely. This watchdog is
   * a failure deadline, never the ordering mechanism of the race test.
   */
  const gchar *scenario = g_getenv ("GIMP_TEST_PROTOCOL_SCENARIO");
  gint         result;

  alarm (20);
  result = gimp_main (protocol_fixture_get_type (), argc, argv);

  /* Only natural gimp_main() returns reach this check. Explicit gimp_quit()
   * exits with an active call stack and is covered by the existing protocol
   * assertions instead. Qdata destruction proves finalization, unlike weak
   * notifications which g_object_run_dispose() can emit before final unref.
   */
  g_assert_cmpuint (procedures_created, ==,
                    g_str_equal (scenario, "temporary-main") ? 2 :
                    (g_str_has_prefix (scenario, "success-") ||
                     g_str_has_prefix (scenario, "error-")) ? 1 : 0);
  g_assert_cmpuint (procedures_finalized, ==, procedures_created);
  g_assert_cmpuint (plug_ins_finalized, ==, 1);
  g_assert_cmpuint (reentrant_disposes_returned, ==,
                    procedures_created > 0 ? 1 : 0);

  if (g_strcmp0 (scenario, "query") == 0 ||
      g_strcmp0 (scenario, "init") == 0)
    {
      /* Query/init install no channel watches. Their only two owning channel
       * references must be gone before gimp_main() returns, not at process
       * exit; the host has already observed the normal quit handshake.
       */
      errno = 0;
      g_assert_cmpint (fcntl (3, F_GETFD), ==, -1);
      g_assert_cmpint (errno, ==, EBADF);
      errno = 0;
      g_assert_cmpint (fcntl (4, F_GETFD), ==, -1);
      g_assert_cmpint (errno, ==, EBADF);
    }

  return result;
}
