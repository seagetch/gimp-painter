/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"

#include <gio/gio.h>
#include <glib/gstdio.h>
#include <string.h>

#include "libgimpbase/gimpbase.h"
#include "libgimpbase/gimpprotocol.h"
#include "libgimpbase/gimpwire.h"


typedef struct
{
  guint *destroy_count;
} OwnedData;

static void
owned_data_destroy (gpointer data)
{
  OwnedData *owned = data;

  (*owned->destroy_count)++;
  g_free (owned);
}

static void
track_owned_data (GimpChoice *choice,
                  guint      *destroy_count)
{
  OwnedData *owned = g_new (OwnedData, 1);

  owned->destroy_count = destroy_count;
  g_object_set_data_full (G_OBJECT (choice), "choice-lifecycle-owned-data",
                          owned, owned_data_destroy);
}

static GimpChoice *
new_choice (void)
{
  return gimp_choice_new_with_values ("horizontal", 3, "Horizontal", "Horizontal help",
                                      "vertical",   7, "Vertical",   NULL,
                                      NULL);
}

static void
sensitivity_changed (GimpChoice  *choice,
                     const gchar *nick,
                     guint       *count)
{
  g_assert_true (GIMP_IS_CHOICE (choice));
  g_assert_cmpstr (nick, ==, "vertical");
  (*count)++;
}

static void
direct_owned_qdata (void)
{
  GimpChoice *choice = new_choice ();
  guint       destroyed = 0;

  track_owned_data (choice, &destroyed);
  g_object_ref (choice);
  g_object_unref (choice);
  g_assert_cmpuint (destroyed, ==, 0);
  g_object_unref (choice);
  g_assert_cmpuint (destroyed, ==, 1);
}

static void
param_spec_ownership_and_values (void)
{
  GimpChoice  *choice = new_choice ();
  GParamSpec  *pspec;
  GValue       value = G_VALUE_INIT;
  GList       *nicks;
  const gchar *label;
  const gchar *help;
  guint        destroyed = 0;
  guint        notifications = 0;

  track_owned_data (choice, &destroyed);
  g_signal_connect (choice, "sensitivity-changed",
                    G_CALLBACK (sensitivity_changed), &notifications);

  /* The parameter specification takes the sole choice reference. */
  pspec = g_param_spec_ref_sink
    (gimp_param_spec_choice ("orientation", "Orientation", "Choice lifetime",
                             choice, "horizontal", G_PARAM_READWRITE));
  g_assert_true (gimp_param_spec_choice_get_choice (pspec) == choice);
  g_assert_cmpstr (gimp_param_spec_choice_get_default (pspec), ==, "horizontal");

  /* Nick lists and documentation are borrowed, not caller-owned temporaries. */
  nicks = gimp_choice_list_nicks (choice);
  g_assert_cmpuint (g_list_length (nicks), ==, 2);
  g_assert_cmpstr (nicks->data, ==, "horizontal");
  g_assert_cmpstr (nicks->next->data, ==, "vertical");
  g_assert_cmpint (gimp_choice_get_id (choice, "horizontal"), ==, 3);
  g_assert_cmpint (gimp_choice_get_id (choice, "vertical"), ==, 7);
  g_assert_true (gimp_choice_get_documentation (choice, "horizontal", &label, &help));
  g_assert_cmpstr (label, ==, "Horizontal");
  g_assert_cmpstr (help, ==, "Horizontal help");
  g_assert_null (gimp_choice_get_help (choice, "vertical"));
  g_assert_false (gimp_choice_is_valid (choice, "missing"));

  g_value_init (&value, G_TYPE_STRING);
  g_param_value_set_default (pspec, &value);
  g_assert_cmpstr (g_value_get_string (&value), ==, "horizontal");
  g_value_set_string (&value, "vertical");
  g_assert_false (g_param_value_validate (pspec, &value));
  gimp_choice_set_sensitive (choice, "vertical", FALSE);
  g_assert_false (gimp_choice_is_valid (choice, "vertical"));
  g_assert_true (g_param_value_validate (pspec, &value));
  g_assert_cmpstr (g_value_get_string (&value), ==, "horizontal");
  g_assert_cmpuint (notifications, ==, 1);
  gimp_choice_set_sensitive (choice, "vertical", FALSE);
  g_assert_cmpuint (notifications, ==, 1);
  gimp_choice_set_sensitive (choice, "vertical", TRUE);
  g_assert_true (gimp_choice_is_valid (choice, "vertical"));
  g_assert_cmpuint (notifications, ==, 2);
  g_value_unset (&value);

  g_param_spec_ref (pspec);
  g_param_spec_unref (pspec);
  g_assert_cmpuint (destroyed, ==, 0);
  g_param_spec_unref (pspec);
  g_assert_cmpuint (destroyed, ==, 1);
}

static GIOChannel *
wire_channel_open (const gchar *path,
                   const gchar *mode)
{
  GError     *error = NULL;
  GIOChannel *channel = g_io_channel_new_file (path, mode, &error);

  g_assert_no_error (error);
  g_assert_nonnull (channel);
  g_assert_cmpint (g_io_channel_set_encoding (channel, NULL, &error), ==,
                   G_IO_STATUS_NORMAL);
  g_assert_no_error (error);
  g_io_channel_set_buffered (channel, FALSE);
  g_io_channel_set_close_on_unref (channel, TRUE);
  return channel;
}

static gboolean
wire_flush (GIOChannel *channel,
            gpointer    user_data)
{
  GError    *error = NULL;
  GIOStatus  status = g_io_channel_flush (channel, &error);

  g_assert_no_error (error);
  return status == G_IO_STATUS_NORMAL;
}

static gchar *
wire_temp_path (void)
{
  GError *error = NULL;
  gchar  *path = NULL;
  gint    fd = g_file_open_tmp ("gimp-choice-wire-XXXXXX", &path, &error);

  g_assert_no_error (error);
  g_assert_cmpint (fd, >=, 0);
  g_assert_true (g_close (fd, &error));
  g_assert_no_error (error);
  return path;
}

static gchar *
serialize_install (const gchar   *path,
                    GPProcInstall *install,
                    gsize         *length)
{
  GError     *error = NULL;
  GIOChannel *channel = wire_channel_open (path, "w");
  gchar      *contents = NULL;

  gimp_wire_clear_error ();
  g_assert_true (gp_proc_install_write (channel, install, NULL));
  g_io_channel_unref (channel);
  g_assert_true (g_file_get_contents (path, &contents, length, &error));
  g_assert_no_error (error);
  return contents;
}

static gchar *
serialize_test_choice (const gchar *path,
                       gsize       *length)
{
  GimpChoice    *choice = new_choice ();
  GPParamDef     param = { 0, };
  GPProcInstall  install = { 0, };
  gchar         *contents;

  param.param_def_type = GP_PARAM_DEF_TYPE_CHOICE;
  param.type_name = "GimpParamChoice";
  param.value_type_name = "gchararray";
  param.name = "orientation";
  param.nick = "Orientation";
  param.blurb = "Wire choice lifetime";
  param.flags = G_PARAM_READWRITE;
  param.meta.m_choice.choice = choice;
  param.meta.m_choice.default_val = "horizontal";
  install.name = "test-choice-wire";
  install.type = GIMP_PDB_PROC_TYPE_PLUGIN;
  install.n_params = 1;
  install.params = &param;
  contents = serialize_install (path, &install, length);
  g_object_unref (choice);
  return contents;
}

static void
wire_roundtrip (void)
{
  g_autofree gchar *path = wire_temp_path ();
  g_autofree gchar *encoded = NULL;
  g_autofree gchar *reencoded = NULL;
  GimpWireMessage  message = { 0, };
  GPProcInstall   *install;
  GimpChoice      *choice;
  GIOChannel      *channel;
  gsize            length;
  gsize            reencoded_length;
  guint            destroyed = 0;

  encoded = serialize_test_choice (path, &length);
  channel = wire_channel_open (path, "r");
  gimp_wire_clear_error ();
  g_assert_true (gimp_wire_read_msg (channel, &message, NULL));
  g_io_channel_unref (channel);
  g_assert_cmpuint (message.type, ==, GP_PROC_INSTALL);
  install = message.data;
  g_assert_nonnull (install);
  g_assert_cmpuint (install->n_params, ==, 1);
  g_assert_cmpuint (install->n_return_vals, ==, 0);
  g_assert_cmpstr (install->params[0].meta.m_choice.default_val, ==, "horizontal");
  choice = install->params[0].meta.m_choice.choice;
  track_owned_data (choice, &destroyed);
  g_assert_cmpuint (g_list_length (gimp_choice_list_nicks (choice)), ==, 2);
  g_assert_cmpint (gimp_choice_get_id (choice, "horizontal"), ==, 3);
  g_assert_cmpint (gimp_choice_get_id (choice, "vertical"), ==, 7);
  g_assert_cmpstr (gimp_choice_get_label (choice, "horizontal"), ==, "Horizontal");
  g_assert_cmpstr (gimp_choice_get_help (choice, "horizontal"), ==, "Horizontal help");
  g_assert_cmpstr (gimp_choice_get_label (choice, "vertical"), ==, "Vertical");
  g_assert_null (gimp_choice_get_help (choice, "vertical"));
  reencoded = serialize_install (path, install, &reencoded_length);
  g_assert_cmpmem (encoded, length, reencoded, reencoded_length);
  gimp_wire_destroy (&message);
  g_assert_cmpuint (destroyed, ==, 1);
  g_assert_cmpint (g_unlink (path), ==, 0);
}

static gsize
skip_wire_string (const gchar *data,
                  gsize        length,
                  gsize        offset)
{
  guint32 size;

  g_assert_cmpuint (offset + sizeof (size), <=, length);
  memcpy (&size, data + offset, sizeof (size));
  size = GUINT32_FROM_BE (size);
  offset += sizeof (size) + size;
  g_assert_cmpuint (offset, <=, length);
  return offset;
}

static void
wire_truncated_choice (void)
{
  g_autofree gchar *path = wire_temp_path ();
  g_autofree gchar *encoded = NULL;
  gsize            length;
  gsize            choice_offset;

  encoded = serialize_test_choice (path, &length);
  /* Skip only the valid framing and common parameter fields, then truncate
   * at every byte of the choice default, count and entry fields. All bytes
   * were produced by the real protocol writer; no replacement reader is used.
   */
  choice_offset = skip_wire_string (encoded, length, sizeof (guint32));
  choice_offset += 4 * sizeof (guint32); /* proc type/counts and param type */
  for (guint i = 0; i < 5; i++)
    choice_offset = skip_wire_string (encoded, length, choice_offset);
  choice_offset += sizeof (guint32); /* parameter flags */
  g_assert_cmpuint (choice_offset, <, length);

  for (gsize cut = choice_offset; cut < length; cut++)
    {
      GError          *error = NULL;
      GimpWireMessage  message = { 0, };
      GIOChannel      *channel;

      g_test_message ("choice wire truncated at byte %" G_GSIZE_FORMAT, cut);
      g_assert_true (g_file_set_contents (path, encoded, cut, &error));
      g_assert_no_error (error);
      channel = wire_channel_open (path, "r");
      gimp_wire_clear_error ();
      g_test_expect_message ("LibGimpBase", G_LOG_LEVEL_WARNING,
                             "*gimp_wire_read(): unexpected EOF");
      g_assert_false (gimp_wire_read_msg (channel, &message, NULL));
      g_test_assert_expected_messages ();
      g_assert_null (message.data);
      g_io_channel_unref (channel);
    }
  gimp_wire_clear_error ();
  g_assert_cmpint (g_unlink (path), ==, 0);
}

int
main (int    argc,
      char **argv)
{
  g_test_init (&argc, &argv, NULL);
  gp_init ();
  gimp_wire_set_flusher (wire_flush);
  g_test_add_func ("/choice-lifecycle/direct-owned-qdata", direct_owned_qdata);
  g_test_add_func ("/choice-lifecycle/param-spec-ownership-and-values",
                   param_spec_ownership_and_values);
  g_test_add_func ("/choice-lifecycle/wire-roundtrip", wire_roundtrip);
  g_test_add_func ("/choice-lifecycle/wire-truncated-choice", wire_truncated_choice);
  return g_test_run ();
}
