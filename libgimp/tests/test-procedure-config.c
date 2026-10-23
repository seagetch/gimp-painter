/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995-1999 Spencer Kimball and Peter Mattis
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

#include <glib/gstdio.h>

#include <libgimp/gimp.h>


typedef struct
{
  const gchar *name;
  gboolean     arguments;
  gboolean     auxiliary;
} ConfigCase;

static const ConfigCase config_cases[] =
{
  { "test-config-empty",    FALSE, FALSE },
  { "test-config-args",     TRUE,  FALSE },
  { "test-config-aux",      FALSE, TRUE  },
  { "test-config-args-aux", TRUE,  TRUE  }
};


static GimpValueArray *
unused_run (GimpProcedure       *procedure,
            GimpProcedureConfig *config,
            gpointer             run_data)
{
  g_assert_not_reached ();
  return NULL;
}

static void
assert_properties (GimpProcedureConfig *config,
                   GParamSpec        **specs,
                   gint                n_specs,
                   guint               offset)
{
  GObjectClass *klass = G_OBJECT_GET_CLASS (config);
  gint          i;

  for (i = 0; i < n_specs; i++)
    {
      GParamSpec *copy = g_object_class_find_property (klass, specs[i]->name);
      GValue      actual = G_VALUE_INIT;
      GValue      expected = G_VALUE_INIT;

      g_assert_nonnull (copy);
      g_assert_cmpuint (copy->owner_type, ==, G_OBJECT_TYPE (config));
      g_assert_cmpuint (copy->param_id, ==, offset + i + 1);
      g_assert_cmpuint (G_PARAM_SPEC_VALUE_TYPE (copy), ==,
                       G_PARAM_SPEC_VALUE_TYPE (specs[i]));
      g_assert_cmpstr (g_param_spec_get_nick (copy), ==,
                      g_param_spec_get_nick (specs[i]));
      g_assert_cmpstr (g_param_spec_get_blurb (copy), ==,
                      g_param_spec_get_blurb (specs[i]));

      g_value_init (&actual, G_PARAM_SPEC_VALUE_TYPE (copy));
      g_value_init (&expected, G_PARAM_SPEC_VALUE_TYPE (copy));
      g_object_get_property (G_OBJECT (config), copy->name, &actual);
      g_param_value_set_default (specs[i], &expected);
      g_assert_cmpint (g_param_values_cmp (copy, &actual, &expected), ==, 0);
      g_value_unset (&actual);
      g_value_unset (&expected);
    }
}

static void
check_config (gconstpointer data)
{
  const ConfigCase     *test_case = data;
  GError               *error = NULL;
  gchar                *path = NULL;
  gint                  fd;
  GIOChannel           *channel;
  GimpPlugIn           *plug_in;
  GimpProcedure        *procedure;
  GimpProcedureConfig  *first;
  GimpProcedureConfig  *second;
  GParamSpec          **args;
  GParamSpec          **aux_args;
  GParamSpec          **properties;
  gint                  n_args;
  gint                  n_aux_args;
  guint                 n_properties;
  gint                  count;
  gboolean              enabled;

  /* Config creation is local. Supply valid transport objects without running
   * a plug-in or opening a connection to the GIMP application.
   */
  fd = g_file_open_tmp ("gimp-procedure-config-XXXXXX", &path, &error);
  g_assert_no_error (error);
  g_assert_cmpint (fd, >=, 0);
  g_assert_true (g_close (fd, &error));
  g_assert_no_error (error);
  channel = g_io_channel_new_file (path, "r+", &error);
  g_assert_no_error (error);
  g_assert_nonnull (channel);
  plug_in = g_object_new (GIMP_TYPE_PLUG_IN,
                         "program-name", "test-procedure-config",
                         "read-channel", channel,
                         "write-channel", channel,
                         NULL);
  procedure = gimp_procedure_new (plug_in, test_case->name,
                                  GIMP_PDB_PROC_TYPE_PLUGIN,
                                  unused_run, NULL, NULL);
  g_assert_nonnull (procedure);

  if (test_case->arguments)
    {
      gimp_procedure_add_int_argument (procedure, "arg-count", "Argument count",
                                      "An ordinary integer argument",
                                      0, 100, 42, G_PARAM_READWRITE);
      gimp_procedure_add_string_argument (procedure, "arg-label", "Argument label",
                                         "An ordinary string argument",
                                         "argument-default", G_PARAM_READWRITE);
    }
  if (test_case->auxiliary)
    {
      gimp_procedure_add_boolean_aux_argument (procedure, "aux-enabled", "Auxiliary flag",
                                              "An auxiliary boolean argument",
                                              TRUE, G_PARAM_READWRITE);
      gimp_procedure_add_string_aux_argument (procedure, "aux-label", "Auxiliary label",
                                             "An auxiliary string argument",
                                             "auxiliary-default", G_PARAM_READWRITE);
    }

  args = gimp_procedure_get_arguments (procedure, &n_args);
  aux_args = gimp_procedure_get_aux_arguments (procedure, &n_aux_args);
  g_assert_cmpint (n_args, ==, test_case->arguments ? 2 : 0);
  g_assert_cmpint (n_aux_args, ==, test_case->auxiliary ? 2 : 0);
  if (! n_args)
    g_assert_null (args);
  if (! n_aux_args)
    g_assert_null (aux_args);

  /* The first call registers the dynamic type, including both array copies. */
  first = gimp_procedure_create_config (procedure);
  g_assert_true (GIMP_IS_PROCEDURE_CONFIG (first));
  g_assert_true (gimp_procedure_config_get_procedure (first) == procedure);
  properties = g_object_class_list_properties (G_OBJECT_GET_CLASS (first),
                                               &n_properties);
  g_assert_cmpuint (n_properties, ==, 1 + n_args + n_aux_args);
  g_free (properties);
  assert_properties (first, args, n_args, 0);
  assert_properties (first, aux_args, n_aux_args, n_args);

  if (n_args)
    g_object_set (first, "arg-count", 7, NULL);
  if (n_aux_args)
    g_object_set (first, "aux-enabled", FALSE, NULL);

  /* The cached GType must produce a distinct config with fresh defaults. */
  second = gimp_procedure_create_config (procedure);
  g_assert_true (GIMP_IS_PROCEDURE_CONFIG (second));
  g_assert_true (second != first);
  g_assert_cmpuint (G_OBJECT_TYPE (second), ==, G_OBJECT_TYPE (first));
  g_assert_true (gimp_procedure_config_get_procedure (second) == procedure);
  assert_properties (second, args, n_args, 0);
  assert_properties (second, aux_args, n_aux_args, n_args);
  if (n_args)
    {
      g_object_get (first, "arg-count", &count, NULL);
      g_assert_cmpint (count, ==, 7);
    }
  if (n_aux_args)
    {
      g_object_get (first, "aux-enabled", &enabled, NULL);
      g_assert_false (enabled);
    }

  g_object_unref (second);
  g_object_unref (first);
  g_object_unref (procedure);
  g_object_unref (plug_in);
  g_assert_cmpint (g_io_channel_shutdown (channel, TRUE, &error), ==,
                   G_IO_STATUS_NORMAL);
  g_assert_no_error (error);
  g_io_channel_unref (channel);
  g_assert_cmpint (g_unlink (path), ==, 0);
  g_free (path);
}

static void
check_empty_config_type (void)
{
  GType         type;
  GObject      *config;
  GParamSpec  **properties;
  guint         n_properties;

  /* The generic registration API explicitly accepts NULL for zero specs. */
  type = gimp_config_type_register (G_TYPE_OBJECT, "TestEmptyConfigType", NULL, 0);
  g_assert_cmpuint (type, !=, G_TYPE_NONE);
  config = g_object_new (type, NULL);
  g_assert_true (GIMP_IS_CONFIG (config));
  properties = g_object_class_list_properties (G_OBJECT_GET_CLASS (config),
                                               &n_properties);
  g_assert_cmpuint (n_properties, ==, 0);
  g_free (properties);
  g_object_unref (config);
}

int
main (int   argc,
      char *argv[])
{
  guint i;

  g_test_init (&argc, &argv, NULL);

  for (i = 0; i < G_N_ELEMENTS (config_cases); i++)
    {
      gchar *path = g_strdup_printf ("/libgimp/procedure-config/%s",
                                     config_cases[i].name);

      g_test_add_data_func (path, &config_cases[i], check_config);
      g_free (path);
    }
  g_test_add_func ("/libgimp/config-type/empty", check_empty_config_type);

  return g_test_run ();
}
