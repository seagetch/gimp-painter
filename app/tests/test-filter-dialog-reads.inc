/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Exercise the real dialog's bounded reads without publishing an edit. */

static void
filter_read_assert_args (const GimpValueArray *actual,
                         const GimpValueArray *expected)
{
  g_assert_true ((actual == NULL) == (expected == NULL));
  if (!expected) return;
  g_assert_cmpint (gimp_value_array_length (actual), ==,
                   gimp_value_array_length (expected));
  for (gint i = 0; i < gimp_value_array_length (expected); i++)
    {
      const GValue *a = gimp_value_array_index (actual, i);
      const GValue *b = gimp_value_array_index (expected, i);
      g_assert_cmpuint (G_VALUE_TYPE (a), ==, G_VALUE_TYPE (b));
      if (G_VALUE_HOLDS_INT (b))
        g_assert_cmpint (g_value_get_int (a), ==, g_value_get_int (b));
      else if (G_VALUE_HOLDS_FLOAT (b))
        {
          gfloat x = g_value_get_float (a), y = g_value_get_float (b);
          g_assert_cmpmem (&x, sizeof x, &y, sizeof y);
        }
      else if (G_VALUE_HOLDS_DOUBLE (b))
        {
          gdouble x = g_value_get_double (a), y = g_value_get_double (b);
          g_assert_cmpmem (&x, sizeof x, &y, sizeof y);
        }
      else if (G_VALUE_HOLDS_STRING (b))
        g_assert_cmpstr (g_value_get_string (a), ==, g_value_get_string (b));
      else if (G_VALUE_HOLDS (b, GIMP_TYPE_VALUE_ARRAY))
        filter_read_assert_args (g_value_get_boxed (a), g_value_get_boxed (b));
      else
        {
          const GimpArray *x = g_value_get_boxed (a), *y = g_value_get_boxed (b);
          g_assert_true (G_VALUE_HOLDS (b, GIMP_TYPE_ARRAY) ||
                         G_VALUE_HOLDS (b, GIMP_TYPE_DOUBLE_ARRAY) ||
                         G_VALUE_HOLDS (b, GIMP_TYPE_INT32_ARRAY));
          g_assert_true ((x == NULL) == (y == NULL));
          if (y) g_assert_cmpmem (x->data, x->length, y->data, y->length);
        }
    }
}

static void
filter_read_assert_definition (GimpFilterLayer      *filter,
                               const gchar          *procedure,
                               GBytes               *raw,
                               const GimpValueArray *args)
{
  gchar *actual_procedure = gimp_filter_layer_dup_procedure (filter);
  GBytes *actual_raw = gimp_filter_layer_ref_definition (filter);
  GimpValueArray *actual_args = gimp_filter_layer_dup_args (filter);
  /* The setter already normalizes a NULL procedure to an empty name. Reads
   * must preserve that existing contract, including arbitrary name bytes. */
  g_assert_cmpstr (actual_procedure, ==, procedure ? procedure : "");
  g_assert_true ((actual_raw == NULL) == (raw == NULL));
  if (raw) g_assert_true (g_bytes_equal (actual_raw, raw));
  filter_read_assert_args (actual_args, args);
  g_free (actual_procedure);
  g_clear_pointer (&actual_raw, g_bytes_unref);
  g_clear_pointer (&actual_args, gimp_value_array_unref);
}

static void
filter_read_cancel (const gchar          *procedure,
                    GBytes               *raw,
                    const GimpValueArray *args,
                    const gchar          *selected,
                    const gchar          *preview_fragment)
{
  GimpImage *image = new_image ();
  GimpFilterLayer *filter = GIMP_FILTER_LAYER (gimp_filter_layer_new (
    image, 16, 16, "read-only dialog fixture", 1, GIMP_LAYER_MODE_PAINTER_REPLACE));
  GimpUndoStack *undo = gimp_image_get_undo_stack (image);
  GimpUndoStack *redo = gimp_image_get_redo_stack (image);
  GimpUndo *undo_top, *redo_top;
  SchemaCheckpoint checkpoint;
  gint redo_depth;

  new_source (image, NULL, "source");
  g_assert_true (gimp_image_add_layer (image, GIMP_LAYER (filter), NULL, 0, FALSE));
  /* Keep real Undo and Redo entries alive: Cancel must neither add an entry
   * nor silently expire a pre-existing redo branch. */
  g_assert_true (gimp_filter_layer_set_definition (filter, "before-dialog", NULL, NULL, NULL));
  g_assert_true (gimp_filter_layer_edit_definition (filter, procedure, raw, args, NULL));
  g_assert_true (gimp_filter_layer_edit_definition (filter, "redo-after-dialog", NULL, NULL, NULL));
  g_assert_true (gimp_image_undo (image));
  gimp_filter_layer_mark_as_loaded (filter);
  checkpoint = schema_checkpoint (image, filter);
  redo_depth = gimp_undo_stack_get_depth (redo);
  g_assert_cmpint (checkpoint.undo_depth, >, 0);
  g_assert_cmpint (redo_depth, >, 0);
  undo_top = g_object_ref (gimp_undo_stack_peek (undo));
  redo_top = g_object_ref (gimp_undo_stack_peek (redo));
  filter_read_assert_definition (filter, procedure, raw, args);

  for (guint edit = 0; edit < 2; edit++)
    {
      GtkWidget *dialog = new_filter_dialog (image, GIMP_LAYER (filter));
      GtkTextView *view = GTK_TEXT_VIEW (field (dialog, "painter-filter-definition"));
      gboolean closed = FALSE;
      g_assert_cmpstr (gtk_combo_box_get_active_id (GTK_COMBO_BOX (
        field (dialog, "painter-filter-choice"))), ==, selected);
      g_assert_false (gtk_text_view_get_editable (view));
      schema_assert_preview (dialog);
      if (preview_fragment)
        {
          GtkTextBuffer *buffer = gtk_text_view_get_buffer (view);
          GtkTextIter start, end;
          gchar *text;
          gtk_text_buffer_get_bounds (buffer, &start, &end);
          text = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);
          g_assert_nonnull (strstr (text, preview_fragment));
          g_free (text);
        }
      schema_assert_checkpoint (image, filter, &checkpoint);
      filter_read_assert_definition (filter, procedure, raw, args);
      if (edit)
        {
          choice (dialog, "painter-filter-choice", "edge");
          number (dialog, "painter-edge-amount", 4.25);
        }
      g_signal_connect (dialog, "destroy", G_CALLBACK (destroyed), &closed);
      gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL);
      g_assert_true (closed);
      g_object_unref (dialog);
      schema_assert_checkpoint (image, filter, &checkpoint);
      filter_read_assert_definition (filter, procedure, raw, args);
      g_assert_cmpint (gimp_undo_stack_get_depth (redo), ==, redo_depth);
      g_assert_true (gimp_undo_stack_peek (undo) == undo_top);
      g_assert_true (gimp_undo_stack_peek (redo) == redo_top);
    }

  g_object_unref (undo_top);
  g_object_unref (redo_top);
  g_object_unref (image);
}

static void
filter_read_cancel_known (void)
{
  const gchar *routes[] = { "edge", "gauss", "gauss-iir", "gauss-rle",
                            "gauss-iir2", "gauss-rle2", "vinvert", "max-rgb",
                            "threshold-alpha", "blinds", "small-tiles", "retinex",
                            "convmatrix" };
  GBytes *raw = g_bytes_new_static ("original\0\xffmetadata", 18);
  for (guint i = 0; i < G_N_ELEMENTS (routes); i++)
    {
      gchar *procedure = g_strconcat ("plug-in-", routes[i], NULL);
      GimpValueArray *args;
      if (i == 0)
        args = gimp_value_array_new_from_types (NULL, G_TYPE_INT, 7, G_TYPE_INT, 123,
          G_TYPE_INT, -456, G_TYPE_FLOAT, 2.5, G_TYPE_INT, 2, G_TYPE_NONE);
      else if (i == 1 || i == 4 || i == 5)
        {
          args = gimp_value_array_new_from_types (NULL, G_TYPE_INT, 7, G_TYPE_INT, 123,
            G_TYPE_INT, -456, G_TYPE_FLOAT, 2.5, G_TYPE_DOUBLE, -0.0, G_TYPE_INT, 1, G_TYPE_NONE);
          if (i != 1) gimp_value_array_truncate (args, 5);
        }
      else if (i == 2 || i == 3)
        args = gimp_value_array_new_from_types (NULL, G_TYPE_INT, 7, G_TYPE_INT, 123,
          G_TYPE_INT, -456, G_TYPE_FLOAT, 2.5, G_TYPE_INT, -9, G_TYPE_INT, 0, G_TYPE_NONE);
      else if (i < 9)
        {
          args = gimp_value_array_new_from_types (NULL, G_TYPE_INT, 7, G_TYPE_INT, 123,
            G_TYPE_INT, -456, G_TYPE_INT, i == 7 ? -9 : 127, G_TYPE_NONE);
          if (i == 6) gimp_value_array_truncate (args, 3);
        }
      else
        args = isolated_args (i - 9, i == 12 ? 1 : 0);
      g_test_message ("Open and cancel saved route %s", procedure);
      filter_read_cancel (procedure, raw, args, routes[i], NULL);
      gimp_value_array_unref (args);
      g_free (procedure);
    }
  g_bytes_unref (raw);
}

static void
filter_read_cancel_unknown (void)
{
  GBytes *raw = g_bytes_new_static ("opaque\0raw\xff", 11);
  GimpValueArray *args = gimp_value_array_new_from_types (NULL,
    G_TYPE_STRING, "unrecognized argument", G_TYPE_DOUBLE, -0.0,
    G_TYPE_INT, -1234567, G_TYPE_NONE);
  filter_read_cancel ("unknown-dialog-provider", raw, args, "keep", "unrecognized argument");
  /* A known name with an unsupported shape must still stay on Keep. */
  filter_read_cancel ("plug-in-edge", raw, args, "keep", "unrecognized argument");
  gimp_value_array_unref (args);
  g_bytes_unref (raw);
}

static void
filter_read_cancel_oversized (void)
{
  const gsize size = 2 * 1024 * 1024;
  gchar *procedure = g_malloc (size + 1);
  gchar *text = g_malloc (size + 1);
  guchar *data = g_malloc (size);
  GBytes *raw;
  GimpValueArray *args;
  memset (procedure, 'p', size); procedure[size] = 0;
  memset (text, 's', size); text[size] = 0;
  memset (data, 0xff, size);
  raw = g_bytes_new_take (data, size);
  args = gimp_value_array_new_from_types (NULL, G_TYPE_STRING, text,
    G_TYPE_INT, 123, G_TYPE_INT, -456, G_TYPE_FLOAT, 2.5,
    G_TYPE_INT, 2, G_TYPE_INT, 1, G_TYPE_NONE);
  filter_read_cancel (procedure, raw, args, "keep", "name truncated");
  /* Even a supported scalar shape keeps the old materialization admission:
   * an oversized context slot must not become an editable Edge definition. */
  filter_read_cancel ("plug-in-edge", raw, args, "keep", "omitted from this preview");
  gimp_value_array_unref (args);
  args = gimp_value_array_new (0);
  {
    GValue value = G_VALUE_INIT;
    g_value_init (&value, G_TYPE_INT);
    for (guint i = 0; i < 600; i++)
      {
        g_value_set_int (&value, i);
        gimp_value_array_append (args, &value);
      }
    g_value_unset (&value);
  }
  filter_read_cancel ("unknown-many-arguments", raw, args, "keep", "omitted from this preview");
  gimp_value_array_unref (args);
  g_bytes_unref (raw);
  g_free (text);
  g_free (procedure);
}

static void
filter_read_cancel_utf8 (void)
{
  GBytes *raw = g_bytes_new_static ("\xff\0\xfe", 3);
  GimpValueArray *args = gimp_value_array_new_from_types (NULL,
    G_TYPE_STRING, "argument-\xfe-\xe2\x82", G_TYPE_NONE);
  gchar *procedure = g_malloc (4100);
  filter_read_cancel ("unknown-\xff-procedure", raw, args, "keep", "unknown-\357\277\275-procedure");
  /* The bounded procedure prefix ends inside a valid three-byte character.
   * Only the GTK presentation may repair that prefix. */
  memset (procedure, 'p', 4095);
  memcpy (procedure + 4095, "\xe2\x82\xac", 4);
  g_assert_true (g_utf8_validate (procedure, -1, NULL));
  filter_read_cancel (procedure, raw, args, "keep", "name truncated");
  g_free (procedure);
  gimp_value_array_unref (args);
  g_bytes_unref (raw);
}

static void
filter_read_cancel_null_empty (void)
{
  GBytes *empty_raw = g_bytes_new_static ("", 0);
  GimpValueArray *empty_args = gimp_value_array_new (0);
  GimpValueArray *nested = gimp_value_array_new_from_types (NULL,
    G_TYPE_STRING, NULL, G_TYPE_STRING, "", GIMP_TYPE_VALUE_ARRAY, NULL,
    GIMP_TYPE_VALUE_ARRAY, empty_args, G_TYPE_NONE);
  for (guint flags = 0; flags < 8; flags++)
    filter_read_cancel (flags & 1 ? "" : NULL,
                        flags & 2 ? empty_raw : NULL,
                        flags & 4 ? empty_args : NULL, "keep", NULL);
  filter_read_cancel ("unknown-null-empty", empty_raw, nested, "keep", NULL);
  gimp_value_array_unref (nested);
  gimp_value_array_unref (empty_args);
  g_bytes_unref (empty_raw);
}
