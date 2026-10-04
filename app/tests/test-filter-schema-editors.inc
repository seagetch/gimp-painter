/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Native Phase C boundaries. These fixtures borrow the procedures restored by
 * gimp_init_for_testing(); opening an editor must not query another provider. */
#include "pdb/pdb-types.h"
#include "pdb/gimppdb.h"
#include "pdb/gimpprocedure.h"
#include "plug-in/plug-in-types.h"
#include "plug-in/gimppluginprocedure.h"

static const gchar *schema_procedures[] = {
  "plug-in-blinds", "plug-in-painter-small-tiles",
  "plug-in-painter-retinex", "plug-in-painter-convmatrix"
};
static const gchar *schema_edit_fields[] = {
  "painter-blinds-angle", "painter-small-tiles-factor",
  "painter-retinex-scale", "painter-convmatrix-offset"
};
static const gchar *schema_edits[] = { "47", "3", "128", "0.125" };
static const gchar *schema_equivalent[] = { "+23", "02", "+64", "-0.000" };

typedef struct
{
  guint64 revision, generation, cache_generation, runs;
  gint undo_depth;
} SchemaCheckpoint;

static SchemaCheckpoint
schema_checkpoint (GimpImage *image, GimpFilterLayer *filter)
{
  return (SchemaCheckpoint) {
    gimp_filter_layer_get_definition_revision (filter),
    gimp_filter_layer_get_generation (filter),
    gimp_filter_layer_get_cache_generation (filter),
    gimp_filter_layer_get_run_count (filter),
    gimp_undo_stack_get_depth (gimp_image_get_undo_stack (image))
  };
}

static void
schema_assert_checkpoint (GimpImage *image, GimpFilterLayer *filter,
                          const SchemaCheckpoint *expected)
{
  SchemaCheckpoint actual = schema_checkpoint (image, filter);
  g_assert_cmpuint (actual.revision, ==, expected->revision);
  g_assert_cmpuint (actual.generation, ==, expected->generation);
  g_assert_cmpuint (actual.cache_generation, ==, expected->cache_generation);
  g_assert_cmpuint (actual.runs, ==, expected->runs);
  g_assert_cmpint (actual.undo_depth, ==, expected->undo_depth);
}

static GimpProcedure *
schema_registered (guint route)
{
  GimpProcedure *procedure = gimp_pdb_lookup_procedure (gimp->pdb, schema_procedures[route]);
  gchar *path;
  g_assert_nonnull (procedure);
  g_assert_true (GIMP_IS_PLUG_IN_PROCEDURE (procedure));
  path = g_file_get_path (GIMP_PLUG_IN_PROCEDURE (procedure)->file);
  g_test_message ("Registered schema provider %s: %s", schema_procedures[route], path);
  g_free (path);
  return g_object_ref (procedure);
}

static void
schema_assert_explanation (GtkWidget *dialog)
{
  GtkWidget *status = field (dialog, "painter-filter-schema-status");
  const gchar *text;
  g_assert_true (GTK_IS_LABEL (status));
  text = gtk_label_get_text (GTK_LABEL (status));
  g_assert_nonnull (text);
  g_assert_cmpuint (strlen (text), >, 0);
  g_assert_true (g_utf8_validate (text, -1, NULL));
}

static void
schema_assert_blocked (GtkWidget *dialog, gboolean *closed)
{
  gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
  g_assert_false (*closed);
  g_assert_true (gtk_widget_get_visible (field (dialog, "painter-layer-error")));
  schema_assert_explanation (dialog);
}

static void
schema_assert_values (GimpFilterLayer *filter, const GimpValueArray *expected)
{
  GimpFilterArgumentsSnapshot *snapshot = gimp_filter_layer_snapshot_arguments (filter);
  g_assert_nonnull (snapshot);
  g_assert_cmpuint (gimp_filter_arguments_snapshot_count (snapshot), ==,
                   gimp_value_array_length (expected));
  for (guint i = 0; i < gimp_value_array_length (expected); i++)
    {
      const GValue *a = gimp_filter_arguments_snapshot_peek_value (snapshot, i);
      const GValue *b = gimp_value_array_index (expected, i);
      g_assert_nonnull (a);
      g_assert_cmpuint (G_VALUE_TYPE (a), ==, G_VALUE_TYPE (b));
      if (G_VALUE_HOLDS_INT (a)) g_assert_cmpint (g_value_get_int (a), ==, g_value_get_int (b));
      else if (G_VALUE_HOLDS_INT64 (a)) g_assert_cmpint (g_value_get_int64 (a), ==, g_value_get_int64 (b));
      else if (G_VALUE_HOLDS_DOUBLE (a))
        {
          gdouble x = g_value_get_double (a), y = g_value_get_double (b);
          g_assert_cmpmem (&x, sizeof x, &y, sizeof y);
        }
      else if (G_VALUE_HOLDS_STRING (a))
        g_assert_cmpstr (g_value_get_string (a), ==, g_value_get_string (b));
      else if (G_VALUE_HOLDS (a, G_TYPE_STRV))
        {
          gchar **x = g_value_get_boxed (a), **y = g_value_get_boxed (b);
          guint j = 0;
          g_assert_nonnull (x); g_assert_nonnull (y);
          while (y[j]) { g_assert_cmpstr (x[j], ==, y[j]); j++; }
          g_assert_null (x[j]);
        }
      else
        {
          const GimpArray *x = g_value_get_boxed (a), *y = g_value_get_boxed (b);
          g_assert_true (G_VALUE_HOLDS (a, GIMP_TYPE_ARRAY) ||
                         G_VALUE_HOLDS (a, GIMP_TYPE_INT32_ARRAY) ||
                         G_VALUE_HOLDS (a, GIMP_TYPE_DOUBLE_ARRAY));
          g_assert_nonnull (x); g_assert_nonnull (y);
          g_assert_cmpmem (x->data, x->length, y->data, y->length);
        }
    }
  gimp_filter_arguments_snapshot_free (snapshot);
}

static void
schema_assert_preview (GtkWidget *dialog)
{
  GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (
    field (dialog, "painter-filter-definition")));
  GtkTextIter start, end;
  gchar *text;
  gtk_text_buffer_get_bounds (buffer, &start, &end);
  text = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);
  g_assert_true (g_utf8_validate (text, -1, NULL));
  g_assert_cmpuint (strlen (text), <=, 65536);
  g_test_message ("Bounded definition preview: %zu UTF-8 bytes", strlen (text));
  g_assert_cmpuint (strlen (text), >, 0);
  g_free (text);
}

static void
filter_schema_registry_invalidation (void)
{
  for (guint route = 0; route < 4; route++)
    for (guint reregister = 0; reregister < 2; reregister++)
      {
        GimpProcedure *procedure = schema_registered (route);
        GimpImage *image = new_image ();
        GimpValueArray *args = isolated_args (route, route == 3 ? 1 : 0);
        GimpFilterLayer *filter = isolated_layer (image, route, args);
        GtkWidget *dialog;
        SchemaCheckpoint checkpoint;
        gboolean closed = FALSE;
        gimp_filter_layer_mark_as_loaded (filter);
        checkpoint = schema_checkpoint (image, filter);
        dialog = isolated_editor (image, filter, route);
        g_signal_connect (dialog, "destroy", G_CALLBACK (destroyed), &closed);
        isolated_text (dialog, schema_edit_fields[route], schema_edits[route]);
        gimp_pdb_unregister_procedure (gimp->pdb, procedure);
        g_assert_null (gimp_pdb_lookup_procedure (gimp->pdb, schema_procedures[route]));
        if (reregister) gimp_pdb_register_procedure (gimp->pdb, procedure);
        /* Even re-registering the very same object invalidates its old lease. */
        schema_assert_blocked (dialog, &closed);
        schema_assert_checkpoint (image, filter, &checkpoint);
        schema_assert_values (filter, args); isolated_assert_raw (filter);
        /* Different decimal spelling with equal bits is still an unchanged OK,
         * including -0, and must not depend on a current metadata lease. */
        isolated_text (dialog, schema_edit_fields[route], schema_equivalent[route]);
        gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
        g_assert_true (closed); g_object_unref (dialog);
        schema_assert_checkpoint (image, filter, &checkpoint);
        schema_assert_values (filter, args); isolated_assert_raw (filter);
        if (!reregister)
          {
            /* An editor opened after removal still offers a lossless no-op. */
            isolated_accept (isolated_editor (image, filter, route));
            schema_assert_checkpoint (image, filter, &checkpoint);
            gimp_pdb_register_procedure (gimp->pdb, procedure);
          }
        /* The fresh editor can acquire a new lease for the restored provider. */
        dialog = isolated_editor (image, filter, route);
        isolated_edit_one (dialog, route, args); isolated_accept (dialog);
        schema_assert_values (filter, args);
        g_assert_cmpuint (gimp_filter_layer_get_definition_revision (filter), >, checkpoint.revision);
        g_object_unref (procedure); gimp_value_array_unref (args); g_object_unref (image);
      }
}

static void
filter_schema_unavailable_creation (void)
{
  for (guint route = 0; route < 4; route++)
    {
      GimpProcedure *procedure = schema_registered (route);
      GimpImage *image = new_image ();
      GtkWidget *dialog;
      gboolean closed = FALSE;
      new_source (image, NULL, "source");
      gimp_pdb_unregister_procedure (gimp->pdb, procedure);
      /* Public procedures remain present, but cannot replace hidden schemas. */
      if (route == 1 || route == 2)
        {
          const gchar *public_name = route == 1 ? "plug-in-small-tiles" : "plug-in-retinex";
          g_assert_nonnull (gimp_pdb_lookup_procedure (gimp->pdb, public_name));
        }
      dialog = new_filter_dialog (image, NULL);
      choice (dialog, "painter-filter-choice", isolated_choices[route]);
      g_signal_connect (dialog, "destroy", G_CALLBACK (destroyed), &closed);
      schema_assert_blocked (dialog, &closed);
      g_assert_cmpint (gimp_image_get_n_layers (image), ==, 1);
      g_assert_cmpint (gimp_undo_stack_get_depth (gimp_image_get_undo_stack (image)), ==, 0);
      gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL);
      g_assert_true (closed); g_object_unref (dialog);
      gimp_pdb_register_procedure (gimp->pdb, procedure);
      g_object_unref (procedure); g_object_unref (image);
    }
}

static GParamSpecInt *
schema_integer_spec (GimpProcedure *procedure, guint route)
{
  const gchar *keys[] = { "angle-displacement", "num-tiles", "scale", "border-mode" };
  for (gint i = 0; i < procedure->num_args; i++)
    if (!strcmp (g_param_spec_get_name (procedure->args[i]), keys[route]))
      {
        g_assert_true (G_IS_PARAM_SPEC_INT (procedure->args[i]));
        return G_PARAM_SPEC_INT (procedure->args[i]);
      }
  g_assert_not_reached ();
  return NULL;
}

static void
filter_schema_incompatible_metadata (void)
{
  for (guint route = 0; route < 4; route++)
    for (guint opened_before_change = 0; opened_before_change < 2; opened_before_change++)
      {
        GimpProcedure *procedure = schema_registered (route);
        GParamSpecInt *spec = schema_integer_spec (procedure, route);
        const gint maximum = spec->maximum;
        GimpImage *image = new_image ();
        GimpValueArray *args = isolated_args (route, 0);
        GimpFilterLayer *filter = isolated_layer (image, route, args);
        GtkWidget *dialog = NULL;
        SchemaCheckpoint checkpoint;
        gboolean closed = FALSE;
        gimp_filter_layer_mark_as_loaded (filter);
        checkpoint = schema_checkpoint (image, filter);
        if (opened_before_change) dialog = isolated_editor (image, filter, route);
        spec->maximum = maximum - 1;
        if (!opened_before_change) dialog = isolated_editor (image, filter, route);
        g_signal_connect (dialog, "destroy", G_CALLBACK (destroyed), &closed);
        isolated_text (dialog, schema_edit_fields[route], schema_edits[route]);
        schema_assert_blocked (dialog, &closed);
        schema_assert_checkpoint (image, filter, &checkpoint);
        schema_assert_values (filter, args); isolated_assert_raw (filter);
        isolated_text (dialog, schema_edit_fields[route], schema_equivalent[route]);
        gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
        g_assert_true (closed); g_object_unref (dialog);
        schema_assert_checkpoint (image, filter, &checkpoint);
        spec->maximum = maximum;
        g_object_unref (procedure); gimp_value_array_unref (args); g_object_unref (image);
      }
}

static void
filter_schema_provider_identity (void)
{
  GimpProcedure *procedure = schema_registered (0);
  GimpPlugInProcedure *plugin = GIMP_PLUG_IN_PROCEDURE (procedure);
  GFile *original = plugin->file;
  GimpImage *image = new_image ();
  GimpValueArray *args = isolated_args (0, 0);
  GimpFilterLayer *filter = isolated_layer (image, 0, args);
  GtkWidget *dialog;
  SchemaCheckpoint checkpoint;
  gboolean closed = FALSE;
  gimp_filter_layer_mark_as_loaded (filter);
  checkpoint = schema_checkpoint (image, filter);
  dialog = isolated_editor (image, filter, 0);
  g_signal_connect (dialog, "destroy", G_CALLBACK (destroyed), &closed);
  plugin->file = g_file_new_for_path ("/nonexistent/painter-schema-untrusted-provider");
  isolated_text (dialog, schema_edit_fields[0], schema_edits[0]);
  schema_assert_blocked (dialog, &closed);
  schema_assert_checkpoint (image, filter, &checkpoint);
  gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL);
  g_assert_true (closed); g_object_unref (dialog);
  g_object_unref (plugin->file); plugin->file = original;
  schema_assert_values (filter, args); isolated_assert_raw (filter);
  g_object_unref (procedure); gimp_value_array_unref (args); g_object_unref (image);
}

static void
filter_schema_reordered_keys (void)
{
  for (guint route = 0; route < 4; route++)
    {
      GimpProcedure *procedure = schema_registered (route);
      GimpImage *image = new_image ();
      GimpValueArray *args = isolated_args (route, 0);
      GimpFilterLayer *filter = isolated_layer (image, route, args);
      GtkWidget *dialog;
      GParamSpec *original[9];
      g_assert_cmpint (procedure->num_args, <=, G_N_ELEMENTS (original));
      memcpy (original, procedure->args, procedure->num_args * sizeof *original);
      for (gint i = 3; i < procedure->num_args; i++)
        procedure->args[i] = original[procedure->num_args + 2 - i];
      /* Only named payload inputs may move; context retains its ABI prefix. */
      dialog = isolated_editor (image, filter, route);
      isolated_edit_one (dialog, route, args); isolated_accept (dialog);
      schema_assert_values (filter, args); isolated_assert_raw (filter);
      memcpy (procedure->args, original, procedure->num_args * sizeof *original);
      g_object_unref (procedure); gimp_value_array_unref (args); g_object_unref (image);
    }
}

static void
schema_large_value (GValue *value, guint kind)
{
  const gsize bytes = 2 * 1024 * 1024;
  if (kind == 0)
    {
      gchar *text = g_malloc (bytes + 1);
      memset (text, 0xff, bytes); text[bytes] = 0;
      g_value_init (value, G_TYPE_STRING); g_value_take_string (value, text);
    }
  else if (kind == 1)
    {
      gchar **strings = g_new0 (gchar *, 257);
      for (guint i = 0; i < 256; i++)
        { strings[i] = g_malloc (8193); memset (strings[i], 0xfe, 8192); strings[i][8192] = 0; }
      g_value_init (value, G_TYPE_STRV); g_value_take_boxed (value, strings);
    }
  else if (kind == 2)
    {
      gdouble *numbers = g_new (gdouble, bytes / sizeof (gdouble));
      for (gsize i = 0; i < bytes / sizeof (gdouble); i++) numbers[i] = i * 0.125;
      g_value_init (value, GIMP_TYPE_DOUBLE_ARRAY);
      gimp_value_set_double_array (value, numbers, bytes / sizeof (gdouble));
      g_free (numbers);
    }
  else if (kind == 3)
    {
      gint32 *numbers = g_new (gint32, bytes / sizeof (gint32));
      for (gsize i = 0; i < bytes / sizeof (gint32); i++) numbers[i] = -(gint32) i;
      g_value_init (value, GIMP_TYPE_INT32_ARRAY);
      gimp_value_set_int32_array (value, numbers, bytes / sizeof (gint32));
      g_free (numbers);
    }
  else
    {
      guchar *data = g_malloc (bytes);
      memset (data, 0xfd, bytes);
      g_value_init (value, GIMP_TYPE_ARRAY);
      g_value_take_boxed (value, gimp_array_new (data, bytes, FALSE));
      g_free (data);
    }
}

static void
filter_schema_bounded_preview (void)
{
  for (guint kind = 0; kind < 6; kind++)
    {
      GimpImage *image = new_image ();
      GimpFilterLayer *filter = GIMP_FILTER_LAYER (gimp_filter_layer_new (
        image, 16, 16, "large preserved definition", 1, GIMP_LAYER_MODE_PAINTER_REPLACE));
      GimpValueArray *args = gimp_value_array_new (0);
      GValue value = G_VALUE_INIT;
      guchar *data = g_malloc (1024 * 1024);
      GBytes *raw, *copy;
      SchemaCheckpoint checkpoint;
      GtkWidget *dialog;
      memset (data, 0xff, 1024 * 1024);
      raw = g_bytes_new_take (data, 1024 * 1024);
      if (kind < 5)
        {
          schema_large_value (&value, kind);
          gimp_value_array_append (args, &value); g_value_unset (&value);
        }
      else
        {
          /* Per-string truncation alone cannot enforce a whole-preview bound. */
          gchar *text = g_malloc (8193);
          memset (text, 0xff, 8192); text[8192] = 0;
          g_value_init (&value, G_TYPE_STRING); g_value_take_string (&value, text);
          for (guint i = 0; i < 600; i++) gimp_value_array_append (args, &value);
          g_value_unset (&value);
        }
      g_assert_true (gimp_image_add_layer (image, GIMP_LAYER (filter), NULL, 0, FALSE));
      g_assert_true (gimp_filter_layer_set_definition (filter, "unknown-schema-\xff", raw, args, NULL));
      gimp_filter_layer_mark_as_loaded (filter);
      checkpoint = schema_checkpoint (image, filter);
      for (guint accept = 0; accept < 2; accept++)
        {
          gint64 started = g_get_monotonic_time ();
          dialog = new_filter_dialog (image, GIMP_LAYER (filter));
          g_test_message ("Large-preview native dialog callback: %" G_GINT64_FORMAT " microseconds", g_get_monotonic_time () - started);
          g_assert_cmpstr (gtk_combo_box_get_active_id (GTK_COMBO_BOX (
            field (dialog, "painter-filter-choice"))), ==, "keep");
          schema_assert_preview (dialog);
          if (accept) isolated_accept (dialog);
          else { gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL); g_object_unref (dialog); }
          schema_assert_checkpoint (image, filter, &checkpoint);
          schema_assert_values (filter, args);
          copy = gimp_filter_layer_ref_definition (filter);
          g_assert_true (g_bytes_equal (copy, raw)); g_bytes_unref (copy);
        }
      gimp_value_array_unref (args); g_bytes_unref (raw); g_object_unref (image);
    }
}

static void
filter_schema_materialization_budget (void)
{
  for (guint kind = 0; kind < 5; kind++)
    {
      GimpImage *image = new_image ();
      GimpValueArray *args = isolated_args (3, 0);
      GValue tail = G_VALUE_INIT;
      GimpFilterLayer *filter;
      GimpFilterArgumentsSnapshot *before, *after;
      const GValue *tail_identity;
      SchemaCheckpoint checkpoint;
      GtkWidget *dialog;
      schema_large_value (&tail, kind);
      gimp_value_array_append (args, &tail); g_value_unset (&tail);
      filter = isolated_layer (image, 3, args);
      gimp_filter_layer_mark_as_loaded (filter);
      checkpoint = schema_checkpoint (image, filter);
      before = gimp_filter_layer_snapshot_arguments (filter);
      tail_identity = gimp_filter_arguments_snapshot_peek_value (before, 11);
      dialog = isolated_editor (image, filter, 3);
      schema_assert_preview (dialog); isolated_accept (dialog);
      schema_assert_checkpoint (image, filter, &checkpoint);
      schema_assert_values (filter, args); isolated_assert_raw (filter);
      dialog = isolated_editor (image, filter, 3);
      isolated_text (dialog, "painter-convmatrix-offset", "0.125");
      isolated_accept (dialog);
      after = gimp_filter_layer_snapshot_arguments (filter);
      /* Shared untouched payload is not charged as a hypothetical deep copy. */
      g_assert_true (gimp_filter_arguments_snapshot_peek_value (after, 11) == tail_identity);
      gimp_filter_arguments_snapshot_free (after);
      g_value_set_double (gimp_value_array_index (args, 7), 0.125);
      schema_assert_values (filter, args); isolated_assert_raw (filter);
      g_assert_cmpint (gimp_undo_stack_get_depth (gimp_image_get_undo_stack (image)), ==, checkpoint.undo_depth + 1);
      g_assert_true (gimp_image_undo (image));
      g_value_set_double (gimp_value_array_index (args, 7), -0.0);
      schema_assert_values (filter, args); isolated_assert_raw (filter);
      after = gimp_filter_layer_snapshot_arguments (filter);
      g_assert_true (gimp_filter_arguments_snapshot_peek_value (after, 11) == tail_identity);
      gimp_filter_arguments_snapshot_free (after);
      gimp_filter_arguments_snapshot_free (before);
      gimp_value_array_unref (args); g_object_unref (image);
    }
}

static void
filter_schema_untouched_nonfinite (void)
{
  const guint64 bits[] = { G_GUINT64_CONSTANT (0x7ff8000000000042),
                          G_GUINT64_CONSTANT (0x7ff0000000000000),
                          G_GUINT64_CONSTANT (0xfff0000000000000) };
  for (guint case_id = 0; case_id < 4; case_id++)
    {
      const guint route = case_id < 3 ? 2 : 3;
      GimpImage *image = new_image ();
      GimpValueArray *args = isolated_args (route, route == 3 ? 1 : 0), *original;
      GimpFilterLayer *filter;
      GtkWidget *dialog;
      gdouble numbers[3];
      for (guint i = 0; i < 3; i++) memcpy (&numbers[i], &bits[i], sizeof numbers[i]);
      if (route == 2) g_value_set_double (gimp_value_array_index (args, 6), numbers[case_id]);
      else
        {
          gdouble matrix[25];
          memcpy (matrix, gimp_value_get_double_array (gimp_value_array_index (args, 4), NULL), sizeof matrix);
          matrix[0] = numbers[0];
          gimp_value_set_double_array (gimp_value_array_index (args, 4), matrix, 25);
          g_value_set_double (gimp_value_array_index (args, 6), numbers[1]);
          g_value_set_double (gimp_value_array_index (args, 7), numbers[2]);
        }
      original = gimp_value_array_copy (args);
      filter = isolated_layer (image, route, args);
      gimp_filter_layer_mark_as_loaded (filter);
      isolated_accept (isolated_editor (image, filter, route));
      schema_assert_values (filter, original);
      dialog = isolated_editor (image, filter, route);
      if (route == 2) isolated_edit_one (dialog, route, args);
      else
        {
          gdouble matrix[25];
          memcpy (matrix, gimp_value_get_double_array (gimp_value_array_index (args, 4), NULL), sizeof matrix);
          matrix[12] = 2;
          gimp_value_set_double_array (gimp_value_array_index (args, 4), matrix, 25);
          isolated_text (dialog, "painter-convmatrix-12", "2");
        }
      isolated_accept (dialog);
      schema_assert_values (filter, args); isolated_assert_raw (filter);
      g_assert_cmpint (gimp_undo_stack_get_depth (gimp_image_get_undo_stack (image)), ==, 1);
      g_assert_true (gimp_image_undo (image)); schema_assert_values (filter, original);
      g_assert_true (gimp_image_redo (image)); schema_assert_values (filter, args);
      gimp_value_array_unref (original); gimp_value_array_unref (args); g_object_unref (image);
    }
}

static void
filter_schema_blinds_context_types (void)
{
  GimpImage *image = new_image ();
  GimpValueArray *args = isolated_args (0, 0), *original;
  GimpFilterLayer *filter;
  GtkWidget *dialog;
  GValue *value;
  SchemaCheckpoint checkpoint;
  value = gimp_value_array_index (args, 0); g_value_unset (value);
  g_value_init (value, G_TYPE_STRING); g_value_set_string (value, "old-context-\xff");
  value = gimp_value_array_index (args, 1); g_value_unset (value);
  g_value_init (value, G_TYPE_DOUBLE); g_value_set_double (value, -0.0);
  value = gimp_value_array_index (args, 2); g_value_unset (value);
  g_value_init (value, G_TYPE_INT64); g_value_set_int64 (value, G_MAXINT64);
  original = gimp_value_array_copy (args);
  filter = isolated_layer (image, 0, args);
  gimp_filter_layer_mark_as_loaded (filter);
  checkpoint = schema_checkpoint (image, filter);
  dialog = isolated_editor (image, filter, 0);
  isolated_accept (dialog); schema_assert_checkpoint (image, filter, &checkpoint);
  schema_assert_values (filter, original);
  dialog = isolated_editor (image, filter, 0);
  isolated_edit_one (dialog, 0, args); isolated_accept (dialog);
  schema_assert_values (filter, args); isolated_assert_raw (filter);
  g_assert_true (gimp_image_undo (image)); schema_assert_values (filter, original);
  gimp_value_array_unref (original); gimp_value_array_unref (args); g_object_unref (image);
}

static void
schema_assert_reference_tail (GimpFilterLayer *filter, GType type, gboolean is_null,
                              guint n_references, const GimpFilterArgumentReference *expected,
                              gdouble offset)
{
  GimpFilterArgumentsSnapshot *snapshot = gimp_filter_layer_snapshot_arguments (filter);
  gdouble actual_offset;
  g_assert_nonnull (snapshot);
  g_assert_cmpuint (gimp_filter_arguments_snapshot_count (snapshot), ==, 12);
  g_assert_cmpuint (gimp_filter_arguments_snapshot_type (snapshot, 11), ==, type);
  g_assert_cmpint (gimp_filter_arguments_snapshot_is_null (snapshot, 11), ==, is_null);
  g_assert_cmpuint (gimp_filter_arguments_snapshot_reference_count (snapshot, 11), ==, n_references);
  for (guint i = 0; i < n_references; i++)
    {
      GimpFilterArgumentReference actual;
      g_assert_true (gimp_filter_arguments_snapshot_reference (snapshot, 11, i, &actual));
      g_assert_cmpuint (actual.object_type, ==, expected[i].object_type);
      g_assert_cmpint (actual.id, ==, expected[i].id);
      g_assert_cmpint (actual.was_set, ==, expected[i].was_set);
      g_assert_cmpint (actual.expired, ==, expected[i].expired);
    }
  actual_offset = g_value_get_double (gimp_filter_arguments_snapshot_peek_value (snapshot, 7));
  g_assert_cmpmem (&actual_offset, sizeof actual_offset, &offset, sizeof offset);
  gimp_filter_arguments_snapshot_free (snapshot);
  isolated_assert_raw (filter);
}

static void
filter_schema_reference_tail_provenance (void)
{
  for (guint kind = 0; kind < 6; kind++)
    {
      GimpImage *image = new_image (), *target_image = new_image ();
      gpointer weak_target = target_image;
      GimpValueArray *args = isolated_args (3, 0);
      GimpFilterLayer *filter = isolated_layer (image, 3, args);
      GimpFilterArgumentSpec specs[12] = {{0}};
      GimpFilterArgumentReference references[2] = {
        { GIMP_TYPE_VIEWABLE, G_GINT64_CONSTANT (0x123456789abcdef), TRUE, FALSE },
        { GIMP_TYPE_DRAWABLE, G_GINT64_CONSTANT (-987654321012345), TRUE, TRUE }
      };
      GObject *targets[] = { G_OBJECT (target_image), NULL };
      GimpFilterArgumentsSnapshot *imported;
      GBytes *raw = g_bytes_new_static (isolated_raw, sizeof isolated_raw);
      GError *error = NULL;
      GtkWidget *dialog;
      SchemaCheckpoint checkpoint;
      GType tail_type = kind < 3 ? G_TYPE_OBJECT : GIMP_TYPE_CORE_OBJECT_ARRAY;
      const gboolean is_null = kind == 2 || kind == 4;
      const guint count = kind < 3 ? 1 : kind == 3 ? 2 : 0;
      g_object_add_weak_pointer (G_OBJECT (target_image), &weak_target);
      g_assert_cmpint (references[0].id, !=, gimp_image_get_id (target_image));
      for (guint i = 0; i < 11; i++)
        {
          specs[i].value = gimp_value_array_index (args, i);
          specs[i].value_type = G_VALUE_TYPE (specs[i].value);
        }
      if (kind == 1) { references[0].expired = TRUE; targets[0] = NULL; }
      if (kind == 2) { references[0].was_set = FALSE; targets[0] = NULL; }
      specs[11].value_type = tail_type;
      specs[11].is_null = is_null;
      specs[11].n_references = count;
      specs[11].references = count ? references : NULL;
      specs[11].targets = count ? targets : NULL;
      imported = gimp_filter_arguments_snapshot_import (12, specs, &error);
      g_assert_no_error (error); g_assert_nonnull (imported);
      g_assert_true (gimp_filter_layer_set_definition_with_snapshot (
        filter, "plug-in-convmatrix", raw, imported, &error));
      g_assert_no_error (error);
      gimp_filter_arguments_snapshot_free (imported); g_bytes_unref (raw);
      gimp_filter_layer_mark_as_loaded (filter);
      checkpoint = schema_checkpoint (image, filter);
      isolated_accept (isolated_editor (image, filter, 3));
      schema_assert_checkpoint (image, filter, &checkpoint);
      schema_assert_reference_tail (filter, tail_type, is_null, count, references, -0.0);
      dialog = isolated_editor (image, filter, 3);
      isolated_text (dialog, "painter-convmatrix-offset", "0.125"); isolated_accept (dialog);
      schema_assert_reference_tail (filter, tail_type, is_null, count, references, 0.125);
      g_assert_true (gimp_image_undo (image));
      schema_assert_reference_tail (filter, tail_type, is_null, count, references, -0.0);
      g_assert_true (gimp_image_redo (image));
      schema_assert_reference_tail (filter, tail_type, is_null, count, references, 0.125);
      /* Neither the model, the editor nor its Undo owns a saved target. The
       * imported descriptor keeps its recorded type/ID after actual expiry. */
      g_clear_object (&target_image); g_assert_null (weak_target);
      if (kind == 0 || kind == 3) references[0].expired = TRUE;
      schema_assert_reference_tail (filter, tail_type, is_null, count, references, 0.125);
      dialog = isolated_editor (image, filter, 3);
      isolated_text (dialog, "painter-convmatrix-offset", "0.25"); isolated_accept (dialog);
      schema_assert_reference_tail (filter, tail_type, is_null, count, references, 0.25);
      g_assert_true (gimp_image_undo (image));
      schema_assert_reference_tail (filter, tail_type, is_null, count, references, 0.125);
      g_assert_true (gimp_image_redo (image));
      schema_assert_reference_tail (filter, tail_type, is_null, count, references, 0.25);
      gimp_value_array_unref (args); g_object_unref (image);
    }
}

typedef struct
{
  GimpProcedure *procedure;
  GParamSpecInt *spec;
  gboolean called, unregister;
} SchemaUndoMutation;

static void
schema_undo_mutation (GimpImage *image, GimpUndoEvent event, GimpUndo *undo, gpointer data)
{
  SchemaUndoMutation *mutation = data;
  if (event != GIMP_UNDO_EVENT_UNDO_PUSHED || mutation->called) return;
  mutation->called = TRUE;
  if (mutation->unregister) gimp_pdb_unregister_procedure (gimp->pdb, mutation->procedure);
  else mutation->spec->maximum--;
}

static void
filter_schema_undo_provider_reentry (void)
{
  for (guint unregister = 0; unregister < 2; ++unregister)
    {
      GimpImage *image = new_image ();
      GimpValueArray *args = isolated_args (0, 0);
      GimpFilterLayer *filter = isolated_layer (image, 0, args);
      GtkWidget *dialog;
      SchemaCheckpoint before;
      gboolean closed = FALSE;
      SchemaUndoMutation mutation = {schema_registered (0), NULL, FALSE, unregister};
      mutation.spec = G_PARAM_SPEC_INT (mutation.procedure->args[3]);
      gint maximum = mutation.spec->maximum;
      gimp_filter_layer_mark_as_loaded (filter);
      before = schema_checkpoint (image, filter);
      dialog = isolated_editor (image, filter, 0);
      g_signal_connect (dialog, "destroy", G_CALLBACK (destroyed), &closed);
      gulong hook = g_signal_connect (image, "undo-event", G_CALLBACK (schema_undo_mutation), &mutation);
      isolated_text (dialog, "painter-blinds-angle", "42");
      schema_assert_blocked (dialog, &closed);
      g_assert_true (mutation.called);
      schema_assert_checkpoint (image, filter, &before);
      schema_assert_values (filter, args); isolated_assert_raw (filter);
      g_signal_handler_disconnect (image, hook);
      if (unregister) gimp_pdb_register_procedure (gimp->pdb, mutation.procedure);
      else mutation.spec->maximum = maximum;
      gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL);
      g_object_unref (dialog); g_object_unref (mutation.procedure);
      gimp_value_array_unref (args); g_object_unref (image);
    }
}
