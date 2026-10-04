/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Actual GTK editors for the four native isolated FilterLayer routes. */
#include <math.h>
#include "core/gimpcontainer.h"
#include "xcf/xcf.h"

static const gchar *isolated_choices[] = { "blinds", "small-tiles", "retinex", "convmatrix" };
static const gchar *isolated_fields[][5] = {
  { "painter-blinds-angle", "painter-blinds-segments", "painter-blinds-orientation", "painter-blinds-transparent", NULL },
  { "painter-small-tiles-factor", NULL },
  { "painter-retinex-scale", "painter-retinex-nscales", "painter-retinex-mode", "painter-retinex-cvar", NULL },
  { "painter-convmatrix-divisor", "painter-convmatrix-offset", "painter-convmatrix-border", NULL }
};
static const guchar isolated_raw[] = { 0, 0xff, 7, 0, 0xfe, 0, 0x81 };

static void
isolated_text (GtkWidget *dialog, const gchar *name, const gchar *text)
{
  GtkWidget *entry = field (dialog, name);
  g_assert_true (GTK_IS_ENTRY (entry));
  g_assert_false (GTK_IS_SPIN_BUTTON (entry));
  gtk_entry_set_text (GTK_ENTRY (entry), text);
}

static void
isolated_real (GtkWidget *dialog, const gchar *name, gdouble value)
{
  gchar text[G_ASCII_DTOSTR_BUF_SIZE];
  isolated_text (dialog, name, g_ascii_dtostr (text, sizeof text, value));
}

static void
isolated_integer (GtkWidget *dialog, const gchar *name, gint value)
{
  GtkWidget *widget = field (dialog, name);
  gchar *text;
  if (GTK_IS_COMBO_BOX (widget))
    {
      if (!strcmp (name, "painter-blinds-orientation"))
        choice (dialog, name, value == 1 ? "vertical" : "horizontal");
      else
        { gchar *id = g_strdup_printf ("%d", value); choice (dialog, name, id); g_free (id); }
      return;
    }
  if (GTK_IS_TOGGLE_BUTTON (widget))
    { gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), value != 0); return; }
  text = g_strdup_printf ("%d", value);
  isolated_text (dialog, name, text);
  g_free (text);
}

static GimpValueArray *
isolated_args (guint route, guint tail)
{
  GimpValueArray *args;
  switch (route)
    {
    case 0:
      return gimp_value_array_new_from_types (NULL, G_TYPE_INT, 7, G_TYPE_INT, 123, G_TYPE_INT, -456,
        G_TYPE_INT, 23, G_TYPE_INT, 7, G_TYPE_INT, G_MININT, G_TYPE_INT, -9, G_TYPE_NONE);
    case 1:
      return gimp_value_array_new_from_types (NULL, G_TYPE_INT, 7, G_TYPE_INT, 123, G_TYPE_INT, -456,
        G_TYPE_INT, 2, G_TYPE_NONE);
    case 2:
      return gimp_value_array_new_from_types (NULL, G_TYPE_INT, 7, G_TYPE_INT, 123, G_TYPE_INT, -456,
        G_TYPE_INT, 64, G_TYPE_INT, 0, G_TYPE_INT, 2, G_TYPE_DOUBLE, 1.0000000009313226, G_TYPE_NONE);
    default:
      {
        gdouble matrix[25] = { 0 };
        const gint32 channels[5] = { 2, -7, 0, G_MININT, G_MAXINT };
        matrix[1] = -0.0; matrix[6] = 0.031250000000000007; matrix[12] = 1.0000000000000002;
        matrix[17] = -0.015625000000000003;
        args = gimp_value_array_new_from_types (NULL, G_TYPE_INT, 7, G_TYPE_INT, 123, G_TYPE_INT, -456,
          G_TYPE_INT, 25, GIMP_TYPE_DOUBLE_ARRAY, NULL, G_TYPE_INT, -17,
          G_TYPE_DOUBLE, 1.0000000000000002, G_TYPE_DOUBLE, -0.0,
          G_TYPE_INT, 5, GIMP_TYPE_INT32_ARRAY, NULL, G_TYPE_INT, 2, G_TYPE_NONE);
        gimp_value_set_double_array (gimp_value_array_index (args, 4), matrix, 25);
        gimp_value_set_int32_array (gimp_value_array_index (args, 9), channels, 5);
        if (tail)
          {
            GValue value = G_VALUE_INIT;
            g_value_init (&value, tail == 1 ? G_TYPE_STRING : tail == 2 ? G_TYPE_DOUBLE : G_TYPE_INT);
            if (tail == 1) g_value_set_string (&value, "ignored legacy tail\xff");
            else if (tail == 2) g_value_set_double (&value, -0.0);
            else g_value_set_int (&value, -123456789);
            gimp_value_array_append (args, &value); g_value_unset (&value);
          }
        return args;
      }
    }
}

static void
isolated_assert_args (GimpFilterLayer *filter, guint route, const GimpValueArray *expected)
{
  GimpValueArray *actual = gimp_filter_layer_dup_args (filter);
  gchar *procedure = gimp_filter_layer_dup_procedure (filter);
  gchar *name = g_strconcat ("plug-in-", isolated_choices[route], NULL);
  g_assert_cmpstr (procedure, ==, name); g_free (procedure); g_free (name);
  g_assert_nonnull (actual);
  g_assert_cmpuint (gimp_value_array_length (actual), ==, gimp_value_array_length (expected));
  for (guint i = 0; i < gimp_value_array_length (expected); i++)
    {
      const GValue *a = gimp_value_array_index (actual, i), *b = gimp_value_array_index (expected, i);
      g_assert_cmpuint (G_VALUE_TYPE (a), ==, G_VALUE_TYPE (b));
      if (G_VALUE_HOLDS_INT (a)) g_assert_cmpint (g_value_get_int (a), ==, g_value_get_int (b));
      else if (G_VALUE_HOLDS_DOUBLE (a))
        {
          gdouble x = g_value_get_double (a), y = g_value_get_double (b);
          g_assert_cmpmem (&x, sizeof x, &y, sizeof y);
        }
      else if (G_VALUE_HOLDS_FLOAT (a))
        {
          gfloat x = g_value_get_float (a), y = g_value_get_float (b);
          g_assert_cmpmem (&x, sizeof x, &y, sizeof y);
        }
      else if (G_VALUE_HOLDS_STRING (a)) g_assert_cmpstr (g_value_get_string (a), ==, g_value_get_string (b));
      else
        {
          const GimpArray *x = g_value_get_boxed (a), *y = g_value_get_boxed (b);
          if (y) { g_assert_nonnull (x); g_assert_cmpmem (x->data, x->length, y->data, y->length); }
          else g_assert_null (x);
        }
    }
  gimp_value_array_unref (actual);
}

static void
isolated_assert_raw (GimpFilterLayer *filter)
{
  GBytes *raw = gimp_filter_layer_ref_definition (filter);
  g_assert_nonnull (raw);
  g_assert_cmpmem (g_bytes_get_data (raw, NULL), g_bytes_get_size (raw), isolated_raw, sizeof isolated_raw);
  g_bytes_unref (raw);
}

static GimpFilterLayer *
isolated_layer (GimpImage *image, guint route, GimpValueArray *args)
{
  GimpFilterLayer *filter = GIMP_FILTER_LAYER (gimp_filter_layer_new (
    image, gimp_image_get_width (image), gimp_image_get_height (image),
    "native editor filter", 1, GIMP_LAYER_MODE_PAINTER_REPLACE));
  GBytes *raw = g_bytes_new_static (isolated_raw, sizeof isolated_raw);
  gchar *name = g_strconcat ("plug-in-", isolated_choices[route], NULL);
  g_assert_true (gimp_image_add_layer (image, GIMP_LAYER (filter), NULL, 0, FALSE));
  g_assert_true (gimp_filter_layer_set_definition (filter, name, raw, args, NULL));
  g_bytes_unref (raw); g_free (name);
  return filter;
}

static GtkWidget *
isolated_editor (GimpImage *image, GimpFilterLayer *filter, guint route)
{
  GtkWidget *dialog = new_filter_dialog (image, GIMP_LAYER (filter));
  g_assert_cmpstr (gtk_combo_box_get_active_id (GTK_COMBO_BOX (field (dialog, "painter-filter-choice"))),
                   ==, isolated_choices[route]);
  return dialog;
}

static void
isolated_populate (GtkWidget *dialog, guint route, const GimpValueArray *args)
{
  if (route < 3)
    for (guint i = 0; isolated_fields[route][i]; i++)
      {
        const GValue *value = gimp_value_array_index (args, i + 3);
        if (G_VALUE_HOLDS_INT (value)) isolated_integer (dialog, isolated_fields[route][i], g_value_get_int (value));
        else isolated_real (dialog, isolated_fields[route][i], g_value_get_double (value));
      }
  else
    {
      const gdouble *matrix = gimp_value_get_double_array (gimp_value_array_index (args, 4), NULL);
      const gint32 *channels = gimp_value_get_int32_array (gimp_value_array_index (args, 9), NULL);
      const guint slots[] = { 6, 7, 10 };
      for (guint i = 0; i < 25; i++)
        {
          gchar *name = g_strdup_printf ("painter-convmatrix-%u", i);
          GtkWidget *entry = field (dialog, name);
          gint column, row;
          gtk_container_child_get (GTK_CONTAINER (gtk_widget_get_parent (entry)), entry,
            "left-attach", &column, "top-attach", &row, NULL);
          g_assert_cmpint (column, ==, i / 5); g_assert_cmpint (row, ==, i % 5);
          isolated_real (dialog, name, matrix[i]); g_free (name);
        }
      for (guint i = 0; i < 5; i++)
        {
          gchar *name = g_strdup_printf ("painter-convmatrix-channel-%u", i);
          isolated_integer (dialog, name, channels[i]); g_free (name);
        }
      for (guint i = 0; i < G_N_ELEMENTS (slots); i++)
        {
          const GValue *value = gimp_value_array_index (args, slots[i]);
          if (G_VALUE_HOLDS_INT (value)) isolated_integer (dialog, isolated_fields[route][i], g_value_get_int (value));
          else isolated_real (dialog, isolated_fields[route][i], g_value_get_double (value));
        }
    }
}

static void
isolated_accept (GtkWidget *dialog)
{
  gboolean closed = FALSE;
  g_signal_connect (dialog, "destroy", G_CALLBACK (destroyed), &closed);
  gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
  g_assert_true (closed); g_object_unref (dialog);
}

static void
isolated_editor_create_reedit (void)
{
  for (guint route = 0; route < 4; route++)
    {
      GimpImage *image = new_image ();
      GimpValueArray *expected = isolated_args (route, 0);
      GimpFilterLayer *filter;
      GtkWidget *dialog;
      guint64 revision;
      /* Creation controls encode ordinary choices. Imported signed flags are
       * exercised separately and must survive semantically unchanged edits. */
      if (route == 0)
        {
          g_value_set_int (gimp_value_array_index (expected, 5), 0);
          g_value_set_int (gimp_value_array_index (expected, 6), 1);
        }
      if (route == 3)
        {
          const gint32 flags[] = { 1, 1, 0, 1, 1 };
          gimp_value_set_int32_array (gimp_value_array_index (expected, 9), flags, 5);
          g_value_set_int (gimp_value_array_index (expected, 5), 1);
        }
      new_source (image, NULL, "source");
      dialog = new_filter_dialog (image, NULL);
      choice (dialog, "painter-filter-choice", isolated_choices[route]);
      isolated_populate (dialog, route, expected);
      gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL); g_object_unref (dialog);
      g_assert_cmpint (gimp_image_get_n_layers (image), ==, 1);
      dialog = new_filter_dialog (image, NULL);
      choice (dialog, "painter-filter-choice", isolated_choices[route]);
      isolated_populate (dialog, route, expected); isolated_accept (dialog);
      filter = GIMP_FILTER_LAYER (gimp_image_get_selected_layers (image)->data);
      g_assert_true (GIMP_IS_FILTER_LAYER (filter));
      g_value_set_int (gimp_value_array_index (expected, 0), 1);
      g_value_set_int (gimp_value_array_index (expected, 1), gimp_image_get_id (image));
      g_value_set_int (gimp_value_array_index (expected, 2), 0);
      isolated_assert_args (filter, route, expected); settle (filter);
      revision = gimp_filter_layer_get_definition_revision (filter);
      isolated_accept (isolated_editor (image, filter, route));
      g_assert_cmpuint (gimp_filter_layer_get_definition_revision (filter), ==, revision);
      isolated_assert_args (filter, route, expected);
      g_assert_true (gimp_image_undo (image)); g_assert_cmpint (gimp_image_get_n_layers (image), ==, 1);
      g_assert_true (gimp_image_redo (image)); settle (filter);
      isolated_assert_args (filter, route, expected);
      gimp_value_array_unref (expected); g_object_unref (image);
    }
}

static void
isolated_edit_one (GtkWidget *dialog, guint route, GimpValueArray *expected)
{
  const guint slot[] = { 3, 3, 3, 7 };
  const guint field_index[] = { 0, 0, 0, 1 };
  if (route == 3)
    {
      isolated_text (dialog, isolated_fields[route][field_index[route]], "0.12345678901234566");
      g_value_set_double (gimp_value_array_index (expected, slot[route]), 0.12345678901234566);
    }
  else
    {
      const gint value[] = { 47, 3, 128 };
      isolated_integer (dialog, isolated_fields[route][field_index[route]], value[route]);
      g_value_set_int (gimp_value_array_index (expected, slot[route]), value[route]);
    }
}

static void
isolated_editor_preserve_and_undo (void)
{
  for (guint case_id = 0; case_id < 7; case_id++)
    {
      guint route = MIN (case_id, 3), tail = case_id > 3 ? case_id - 3 : 0;
      GimpImage *image = new_image ();
      GimpValueArray *original = isolated_args (route, tail), *changed = gimp_value_array_copy (original);
      GimpFilterLayer *filter;
      GtkWidget *dialog;
      guint64 revision;
      new_source (image, NULL, "source"); filter = isolated_layer (image, route, original); settle (filter);
      revision = gimp_filter_layer_get_definition_revision (filter);
      isolated_accept (isolated_editor (image, filter, route));
      g_assert_cmpuint (gimp_filter_layer_get_definition_revision (filter), ==, revision);
      g_assert_cmpint (gimp_undo_stack_get_depth (gimp_image_get_undo_stack (image)), ==, 0);
      isolated_assert_args (filter, route, original); isolated_assert_raw (filter);
      dialog = isolated_editor (image, filter, route);
      if (route == 0)
        {
          choice (dialog, "painter-blinds-orientation", "vertical");
          choice (dialog, "painter-blinds-orientation", "horizontal");
          isolated_integer (dialog, "painter-blinds-transparent", 0);
          isolated_integer (dialog, "painter-blinds-transparent", 1);
        }
      if (route == 3)
        {
          const gint32 *flags = gimp_value_get_int32_array (gimp_value_array_index (original, 9), NULL);
          for (guint i = 0; i < 5; i++)
            {
              gchar *name = g_strdup_printf ("painter-convmatrix-channel-%u", i);
              isolated_integer (dialog, name, !flags[i]); isolated_integer (dialog, name, flags[i]);
              g_free (name);
            }
        }
      isolated_accept (dialog);
      g_assert_cmpuint (gimp_filter_layer_get_definition_revision (filter), ==, revision);
      isolated_assert_args (filter, route, original);
      /* Visiting a different page or changing decimal spelling alone is not
       * a definition edit and must not normalize the original typed record. */
      dialog = isolated_editor (image, filter, route);
      choice (dialog, "painter-filter-choice", "edge");
      choice (dialog, "painter-filter-choice", isolated_choices[route]);
      if (route == 0) isolated_text (dialog, "painter-blinds-angle", "+23");
      else if (route == 1) isolated_text (dialog, "painter-small-tiles-factor", "02");
      else if (route == 2) isolated_text (dialog, "painter-retinex-cvar", "1.0000000009313226000");
      else isolated_text (dialog, "painter-convmatrix-divisor", "1.0000000000000002000");
      isolated_accept (dialog);
      g_assert_cmpuint (gimp_filter_layer_get_definition_revision (filter), ==, revision);
      g_assert_cmpint (gimp_undo_stack_get_depth (gimp_image_get_undo_stack (image)), ==, 0);
      isolated_assert_args (filter, route, original); isolated_assert_raw (filter);
      dialog = isolated_editor (image, filter, route); isolated_edit_one (dialog, route, changed);
      gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL); g_object_unref (dialog);
      g_assert_cmpuint (gimp_filter_layer_get_definition_revision (filter), ==, revision);
      isolated_assert_args (filter, route, original);
      dialog = isolated_editor (image, filter, route); isolated_edit_one (dialog, route, changed); isolated_accept (dialog);
      g_assert_cmpuint (gimp_filter_layer_get_definition_revision (filter), >, revision);
      isolated_assert_args (filter, route, changed); isolated_assert_raw (filter); settle (filter);
      g_assert_true (gimp_image_undo (image)); isolated_assert_args (filter, route, original); isolated_assert_raw (filter);
      g_assert_true (gimp_image_redo (image)); isolated_assert_args (filter, route, changed); isolated_assert_raw (filter); settle (filter);
      gimp_value_array_unref (original); gimp_value_array_unref (changed); g_object_unref (image);
    }
}

static void
isolated_editor_stale_definition (void)
{
  for (guint route = 0; route < 4; route++)
    {
      GimpImage *image = new_image ();
      GimpValueArray *original = isolated_args (route, route == 3 ? 1 : 0);
      GimpValueArray *current = gimp_value_array_copy (original), *pending = gimp_value_array_copy (original);
      GimpFilterLayer *filter;
      GtkWidget *dialog;
      gboolean closed = FALSE;
      guint64 revision;
      gchar *name = g_strconcat ("plug-in-", isolated_choices[route], NULL);
      GBytes *raw = g_bytes_new_static (isolated_raw, sizeof isolated_raw);
      new_source (image, NULL, "source"); filter = isolated_layer (image, route, original);
      dialog = isolated_editor (image, filter, route); isolated_edit_one (dialog, route, pending);
      g_signal_connect (dialog, "destroy", G_CALLBACK (destroyed), &closed);
      /* A second editor or an Undo can install another definition while the
       * first is open. It must not overwrite the newer first three slots. */
      g_value_set_int (gimp_value_array_index (current, 2), 9876);
      g_assert_true (gimp_filter_layer_set_definition (filter, name, raw, current, NULL));
      revision = gimp_filter_layer_get_definition_revision (filter);
      gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
      g_assert_false (closed);
      g_assert_true (gtk_widget_get_visible (field (dialog, "painter-layer-error")));
      g_assert_cmpuint (gimp_filter_layer_get_definition_revision (filter), ==, revision);
      isolated_assert_args (filter, route, current); isolated_assert_raw (filter);
      g_assert_cmpint (gimp_undo_stack_get_depth (gimp_image_get_undo_stack (image)), ==, 0);
      gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL); g_object_unref (dialog);
      isolated_accept (isolated_editor (image, filter, route));
      isolated_assert_args (filter, route, current);
      gimp_value_array_unref (original); gimp_value_array_unref (current); gimp_value_array_unref (pending);
      g_bytes_unref (raw); g_free (name); g_object_unref (image);
    }
}

/* Test every cell separately: changing [x*5+y] must not transpose the matrix or
 * round any untouched double, normalize signed flags, or discard a legacy tail. */
static void
isolated_editor_saved_enums (void)
{
  for (guint route = 2; route < 4; route++)
    {
      const guint slot = route == 2 ? 5 : 10;
      const gchar *name = route == 2 ? "painter-retinex-mode" : "painter-convmatrix-border";
      GimpImage *image = new_image ();
      GimpValueArray *args = isolated_args (route, route == 3 ? 3 : 0);
      GimpFilterLayer *filter;
      GtkWidget *dialog;
      guint64 revision;
      g_value_set_int (gimp_value_array_index (args, slot), route == 2 ? 99 : -7);
      new_source (image, NULL, "source"); filter = isolated_layer (image, route, args);
      revision = gimp_filter_layer_get_definition_revision (filter);
      dialog = isolated_editor (image, filter, route);
      g_assert_cmpstr (gtk_combo_box_get_active_id (GTK_COMBO_BOX (field (dialog, name))), ==, "saved");
      isolated_accept (dialog);
      g_assert_cmpuint (gimp_filter_layer_get_definition_revision (filter), ==, revision);
      dialog = isolated_editor (image, filter, route); isolated_edit_one (dialog, route, args); isolated_accept (dialog);
      isolated_assert_args (filter, route, args); isolated_assert_raw (filter);
      dialog = isolated_editor (image, filter, route);
      g_assert_cmpstr (gtk_combo_box_get_active_id (GTK_COMBO_BOX (field (dialog, name))), ==, "saved");
      isolated_integer (dialog, name, 0); isolated_accept (dialog);
      g_value_set_int (gimp_value_array_index (args, slot), 0);
      isolated_assert_args (filter, route, args); isolated_assert_raw (filter); settle (filter);
      gimp_value_array_unref (args); g_object_unref (image);
    }
}

static void
isolated_editor_every_field (void)
{
  for (guint route = 0; route < 4; route++)
    {
      GimpImage *image = new_image ();
      GimpValueArray *original = isolated_args (route, route == 3 ? 1 : 0);
      GimpFilterLayer *filter;
      guint fields = route == 3 ? 33 : route == 1 ? 1 : 4;
      new_source (image, NULL, "source"); filter = isolated_layer (image, route, original);
      for (guint item = 0; item < fields; item++)
        {
          GimpValueArray *expected = gimp_value_array_copy (original);
          GtkWidget *dialog = isolated_editor (image, filter, route);
          gchar *allocated = NULL;
          const gchar *name;
          GValue *value;
          if (route == 3 && item < 30)
            {
              allocated = item < 25 ? g_strdup_printf ("painter-convmatrix-%u", item) :
                g_strdup_printf ("painter-convmatrix-channel-%u", item - 25);
              name = allocated;
              if (item < 25)
                {
                  gdouble matrix[25];
                  memcpy (matrix, gimp_value_get_double_array (gimp_value_array_index (expected, 4), NULL), sizeof matrix);
                  matrix[item] = 0.12500000000000003 + item / 64.0;
                  isolated_real (dialog, name, matrix[item]);
                  gimp_value_set_double_array (gimp_value_array_index (expected, 4), matrix, 25);
                }
              else
                {
                  gint32 flags[5];
                  memcpy (flags, gimp_value_get_int32_array (gimp_value_array_index (expected, 9), NULL), sizeof flags);
                  flags[item - 25] = !flags[item - 25];
                  isolated_integer (dialog, name, flags[item - 25]);
                  gimp_value_set_int32_array (gimp_value_array_index (expected, 9), flags, 5);
                }
            }
          else
            {
              const guint slots[] = { 6, 7, 10 };
              guint i = route == 3 ? item - 30 : item;
              name = isolated_fields[route][i];
              value = gimp_value_array_index (expected, route == 3 ? slots[i] : i + 3);
              if (G_VALUE_HOLDS_DOUBLE (value))
                { g_value_set_double (value, 0.98765432109876539); isolated_real (dialog, name, g_value_get_double (value)); }
              else
                {
                  const gint replacements[][4] = { { 90, 100, 1, 0 }, { 6 },
                    { 256, 8, 1, 0 }, { 0, 0, 0, 0 } };
                  g_value_set_int (value, replacements[route][i]); isolated_integer (dialog, name, g_value_get_int (value));
                }
            }
          isolated_accept (dialog); g_free (allocated);
          isolated_assert_args (filter, route, expected); isolated_assert_raw (filter);
          g_assert_true (gimp_image_undo (image)); isolated_assert_args (filter, route, original);
          gimp_value_array_unref (expected);
        }
      settle (filter); gimp_value_array_unref (original); g_object_unref (image);
    }
}

static void
isolated_editor_invalid_input (void)
{
  static const struct { guint route; const gchar *field; const gchar *text; } invalid[] = {
    { 0, "painter-blinds-angle", "-1" }, { 0, "painter-blinds-angle", "91" },
    { 0, "painter-blinds-segments", "0" }, { 0, "painter-blinds-segments", "101" },
    { 0, "painter-blinds-angle", "1.5" }, { 0, "painter-blinds-angle", "2 trailing" },
    { 1, "painter-small-tiles-factor", "-1" }, { 1, "painter-small-tiles-factor", "7" },
    { 1, "painter-small-tiles-factor", "" }, { 1, "painter-small-tiles-factor", "1e0" },
    { 1, "painter-small-tiles-factor", " \t " },
    { 2, "painter-retinex-scale", "15" }, { 2, "painter-retinex-scale", "257" },
    { 2, "painter-retinex-nscales", "-1" }, { 2, "painter-retinex-nscales", "9" },
    { 2, "painter-retinex-cvar", "-0.01" }, { 2, "painter-retinex-cvar", "4.01" },
    { 2, "painter-retinex-cvar", "NaN" }, { 2, "painter-retinex-cvar", "1,25" },
    { 3, "painter-convmatrix-0", "NaN" }, { 3, "painter-convmatrix-24", "inf" },
    { 3, "painter-convmatrix-12", "1e999" }, { 3, "painter-convmatrix-17", "0.25x" },
    { 3, "painter-convmatrix-divisor", "0" }, { 3, "painter-convmatrix-divisor", "-0" },
    { 3, "painter-convmatrix-divisor", "-inf" }, { 3, "painter-convmatrix-offset", "NaN" }
  };
  for (guint route = 0; route < 4; route++)
    for (guint create = 0; create < 2; create++)
      {
        GimpImage *image = new_image ();
        GimpValueArray *original = isolated_args (route, route == 3 ? 1 : 0);
        GimpFilterLayer *filter = NULL;
        GtkWidget *dialog;
        guint64 revision = 0, generation = 0;
        gboolean closed = FALSE;
        new_source (image, NULL, "source");
        if (! create)
          {
            filter = isolated_layer (image, route, original); settle (filter);
            revision = gimp_filter_layer_get_definition_revision (filter);
            generation = gimp_filter_layer_get_generation (filter);
          }
        dialog = new_filter_dialog (image, GIMP_LAYER (filter));
        choice (dialog, "painter-filter-choice", isolated_choices[route]);
        isolated_populate (dialog, route, original);
        g_signal_connect (dialog, "destroy", G_CALLBACK (destroyed), &closed);
        for (guint i = 0; i < G_N_ELEMENTS (invalid); i++)
          if (invalid[i].route == route)
            {
              g_test_message ("%s %s rejects '%s'", isolated_choices[route], create ? "create" : "edit", invalid[i].text);
              isolated_text (dialog, invalid[i].field, invalid[i].text);
              gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
              g_assert_false (closed);
              g_assert_true (gtk_widget_get_visible (field (dialog, "painter-layer-error")));
              g_assert_cmpstr (gtk_entry_get_text (GTK_ENTRY (field (dialog, invalid[i].field))), ==, invalid[i].text);
              g_assert_cmpint (gimp_image_get_n_layers (image), ==, create ? 1 : 2);
              g_assert_cmpint (gimp_undo_stack_get_depth (gimp_image_get_undo_stack (image)), ==, 0);
              if (filter)
                {
                  g_assert_cmpuint (gimp_filter_layer_get_definition_revision (filter), ==, revision);
                  g_assert_cmpuint (gimp_filter_layer_get_generation (filter), ==, generation);
                  isolated_assert_args (filter, route, original); isolated_assert_raw (filter);
                }
              isolated_populate (dialog, route, original);
            }
        isolated_accept (dialog);
        if (create) filter = GIMP_FILTER_LAYER (gimp_image_get_selected_layers (image)->data);
        settle (filter); gimp_value_array_unref (original); g_object_unref (image);
      }
}

static void
isolated_editor_unknown_shapes (void)
{
  for (guint route = 0; route < 4; route++)
    for (guint shape = 0; shape < (route == 3 ? 7 : 3); shape++)
      {
        GimpImage *image = new_image ();
        GimpValueArray *original = isolated_args (route, 0);
        GimpFilterLayer *filter;
        GtkWidget *dialog;
        guint64 revision;
        GValue *value;
        if (shape == 0) gimp_value_array_truncate (original, gimp_value_array_length (original) - 1);
        else if (shape == 1)
          {
            GValue tail = G_VALUE_INIT; g_value_init (&tail, G_TYPE_INT); g_value_set_int (&tail, 71);
            gimp_value_array_append (original, &tail);
            if (route == 3) gimp_value_array_append (original, &tail);
            g_value_unset (&tail);
          }
        else if (shape == 2)
          {
            value = gimp_value_array_index (original, 3); g_value_unset (value);
            g_value_init (value, G_TYPE_FLOAT); g_value_set_float (value, 25.0);
          }
        else if (shape == 3 || shape == 4)
          {
            gdouble matrix[24] = { 0 }; gint32 flags[4] = { 1, 1, 1, 1 };
            if (shape == 3) gimp_value_set_double_array (gimp_value_array_index (original, 4), matrix, 24);
            else gimp_value_set_int32_array (gimp_value_array_index (original, 9), flags, 4);
          }
        else if (shape == 5) g_value_set_int (gimp_value_array_index (original, 3), 24);
        else g_value_set_int (gimp_value_array_index (original, 8), 4);
        new_source (image, NULL, "source"); filter = isolated_layer (image, route, original);
        revision = gimp_filter_layer_get_definition_revision (filter);
        dialog = new_filter_dialog (image, GIMP_LAYER (filter));
        g_assert_cmpstr (gtk_combo_box_get_active_id (GTK_COMBO_BOX (field (dialog, "painter-filter-choice"))), ==, "keep");
        isolated_accept (dialog);
        g_assert_cmpuint (gimp_filter_layer_get_definition_revision (filter), ==, revision);
        isolated_assert_args (filter, route, original); isolated_assert_raw (filter);
        g_assert_cmpint (gimp_undo_stack_get_depth (gimp_image_get_undo_stack (image)), ==, 0);
        gimp_value_array_unref (original); g_object_unref (image);
      }
}

static void
isolated_editor_lifetimes (void)
{
  for (guint route = 0; route < 4; route++)
    for (guint detach = 0; detach < 2; detach++)
      {
        GimpImage *image = new_image ();
        GimpValueArray *args = isolated_args (route, route == 3 ? 1 : 0);
        GimpFilterLayer *filter;
        GtkWidget *dialog;
        GPtrArray *children = g_ptr_array_new_with_free_func (g_object_unref);
        gpointer weak_dialog;
        guint64 revision;
        new_source (image, NULL, "source"); filter = isolated_layer (image, route, args);
        revision = gimp_filter_layer_get_definition_revision (filter);
        dialog = isolated_editor (image, filter, route); weak_dialog = dialog;
        g_object_add_weak_pointer (G_OBJECT (dialog), &weak_dialog);
        for (guint i = 0; isolated_fields[route][i]; i++)
          g_ptr_array_add (children, g_object_ref (field (dialog, isolated_fields[route][i])));
        if (route == 3)
          for (guint i = 0; i < 30; i++)
            {
              gchar *name = i < 25 ? g_strdup_printf ("painter-convmatrix-%u", i) :
                g_strdup_printf ("painter-convmatrix-channel-%u", i - 25);
              g_ptr_array_add (children, g_object_ref (field (dialog, name))); g_free (name);
            }
        if (detach)
          for (guint i = 0; i < children->len; i++)
            {
              GtkWidget *entry = g_ptr_array_index (children, i);
              gtk_container_remove (GTK_CONTAINER (gtk_widget_get_parent (entry)), entry);
            }
        g_assert_true (gimp_painter_binding_close (G_OBJECT (dialog), NULL));
        g_assert_true (gimp_painter_binding_close (G_OBJECT (dialog), NULL));
        for (guint i = 0; i < children->len; i++)
          {
            GtkWidget *child = g_ptr_array_index (children, i);
            if (GTK_IS_ENTRY (child)) gtk_entry_set_text (GTK_ENTRY (child), "1");
            else if (GTK_IS_TOGGLE_BUTTON (child)) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (child), FALSE);
            else gtk_combo_box_set_active (GTK_COMBO_BOX (child), 1);
          }
        gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
        g_assert_cmpuint (gimp_filter_layer_get_definition_revision (filter), ==, revision);
        isolated_assert_args (filter, route, args); isolated_assert_raw (filter);
        gtk_widget_destroy (dialog); gtk_widget_destroy (dialog); g_object_unref (dialog);
        g_assert_null (weak_dialog);
        for (guint i = 0; i < children->len; i++)
          {
            GtkWidget *entry = g_ptr_array_index (children, i);
            g_signal_emit_by_name (entry, GTK_IS_TOGGLE_BUTTON (entry) ? "toggled" : "changed");
            gtk_widget_destroy (entry);
          }
        g_ptr_array_unref (children); gimp_value_array_unref (args); g_object_unref (image);
      }
}

static void
isolated_editor_reentry (void)
{
  for (guint route = 0; route < 4; route++)
    {
      GimpImage *image = new_image ();
      GimpValueArray *args = isolated_args (route, route == 3 ? 1 : 0);
      GimpFilterLayer *filter;
      GtkWidget *dialog, *entry;
      gpointer weak_dialog;
      guint64 revision;
      new_source (image, NULL, "source"); filter = isolated_layer (image, route, args);
      revision = gimp_filter_layer_get_definition_revision (filter);
      for (guint i = 0; i < (route == 0 ? 2 : route == 3 ? 5 : 0); i++)
        {
          gchar *name = route == 0 ? g_strdup (isolated_fields[route][i + 2]) :
            g_strdup_printf ("painter-convmatrix-channel-%u", i);
          dialog = isolated_editor (image, filter, route); weak_dialog = dialog;
          g_object_add_weak_pointer (G_OBJECT (dialog), &weak_dialog);
          entry = g_object_ref (field (dialog, name));
          if (GTK_IS_TOGGLE_BUTTON (entry))
            {
              g_signal_connect (entry, "toggled", G_CALLBACK (close_editor_on_choice), &dialog);
              gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (entry), !gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (entry)));
            }
          else
            {
              g_signal_connect (entry, "changed", G_CALLBACK (close_editor_on_choice), &dialog);
              gtk_combo_box_set_active_id (GTK_COMBO_BOX (entry), "vertical");
            }
          g_assert_null (dialog); g_assert_null (weak_dialog);
          g_signal_emit_by_name (entry, GTK_IS_TOGGLE_BUTTON (entry) ? "toggled" : "changed");
          g_object_unref (entry); g_free (name);
          g_assert_cmpuint (gimp_filter_layer_get_definition_revision (filter), ==, revision);
          isolated_assert_args (filter, route, args);
        }
      dialog = isolated_editor (image, filter, route); weak_dialog = dialog;
      g_object_add_weak_pointer (G_OBJECT (dialog), &weak_dialog);
      entry = g_object_ref (field (dialog, isolated_fields[route][0]));
      g_signal_connect (entry, "changed", G_CALLBACK (close_editor_on_choice), &dialog);
      gtk_entry_set_text (GTK_ENTRY (entry), "1");
      g_assert_null (dialog); g_assert_null (weak_dialog);
      g_signal_emit_by_name (entry, "changed"); g_object_unref (entry);
      g_assert_cmpuint (gimp_filter_layer_get_definition_revision (filter), ==, revision);
      isolated_assert_args (filter, route, args);
      /* An error label observer can drop the editor's last owner while input
       * validation still holds a field reference. Nothing may be published. */
      dialog = isolated_editor (image, filter, route); weak_dialog = dialog;
      g_object_add_weak_pointer (G_OBJECT (dialog), &weak_dialog);
      isolated_text (dialog, isolated_fields[route][0], "invalid");
      g_signal_connect (field (dialog, "painter-layer-error"), "notify::label",
        G_CALLBACK (close_editor_on_notify), &dialog);
      gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
      g_assert_null (dialog); g_assert_null (weak_dialog);
      g_assert_cmpuint (gimp_filter_layer_get_definition_revision (filter), ==, revision);
      isolated_assert_args (filter, route, args);
      /* Destruction during the actual definition Undo push must also retain the
       * controller and image through the active response callback. */
      {
        ResponseClose state;
        gpointer weak_image = image;
        dialog = isolated_editor (image, filter, route); weak_dialog = dialog;
        isolated_edit_one (dialog, route, args);
        g_object_add_weak_pointer (G_OBJECT (image), &weak_image);
        g_object_add_weak_pointer (G_OBJECT (dialog), &weak_dialog);
        state = (ResponseClose) { dialog, image, GIMP_LAYER (filter), FALSE, FALSE };
        g_signal_connect (image, "dirty", G_CALLBACK (close_editor_on_dirty), &state);
        gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
        g_assert_true (state.called); g_assert_null (state.dialog); g_assert_null (state.image);
        g_assert_null (weak_dialog); g_assert_null (weak_image);
      }
      gimp_value_array_unref (args);
    }
}

static void
isolated_wait (GimpFilterLayer *filter, GimpFilterLayerState state)
{
  gint64 deadline = g_get_monotonic_time () + 30 * G_TIME_SPAN_SECOND;
  while (gimp_filter_layer_get_state (filter) != state && g_get_monotonic_time () < deadline)
    {
      if (gimp_filter_layer_get_state (filter) == GIMP_FILTER_LAYER_FAILED)
        {
          gchar *error = gimp_filter_layer_dup_error (filter);
          g_error ("Native editor filter failed: %s", error ? error : "unknown failure");
        }
      g_main_context_iteration (NULL, FALSE); g_usleep (100);
    }
  g_assert_cmpint (gimp_filter_layer_get_state (filter), ==, state);
}

static void
isolated_fill (GimpLayer *layer, const guint8 rgba[4])
{
  GeglColor *color = gegl_color_new (NULL);
  gegl_color_set_pixel (color, babl_format ("R'G'B'A u8"), rgba);
  gegl_buffer_set_color (gimp_drawable_get_buffer (GIMP_DRAWABLE (layer)), NULL, color);
  g_object_unref (color);
}

static void
isolated_pixel (GimpFilterLayer *filter, const guint8 expected[4])
{
  guint8 actual[4];
  gegl_buffer_get (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)), GEGL_RECTANGLE (0, 0, 1, 1),
    1, babl_format ("R'G'B'A u8"), actual, GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);
  g_assert_cmpmem (actual, sizeof actual, expected, 4);
}

static void
isolated_editor_running_cache (void)
{
  const guint8 source_pixel[] = { 31, 121, 217, 255 }, cached[] = { 13, 29, 47, 255 };
  for (guint route = 0; route < 4; route++)
    {
      const gint size = 257;
      GimpImage *image = gimp_image_new (gimp, size, size, GIMP_RGB, GIMP_PRECISION_U8_NON_LINEAR);
      GimpLayer *source = gimp_layer_new (image, size, size, babl_format ("R'G'B'A u8"),
        "source", 1, GIMP_LAYER_MODE_NORMAL_LEGACY);
      GimpValueArray *args = isolated_args (route, route == 3 ? 1 : 0);
      GimpFilterLayer *filter;
      GtkWidget *dialog;
      guint64 generation;
      g_assert_true (gimp_image_add_layer (image, source, NULL, 0, FALSE));
      if (route == 2) g_value_set_int (gimp_value_array_index (args, 4), 8);
      isolated_fill (source, source_pixel); filter = isolated_layer (image, route, args);
      isolated_fill (GIMP_LAYER (filter), cached);
      isolated_wait (filter, GIMP_FILTER_LAYER_RUNNING); generation = gimp_filter_layer_get_generation (filter);
      isolated_pixel (filter, cached);
      dialog = isolated_editor (image, filter, route);
      if (route == 0)
        { isolated_integer (dialog, isolated_fields[route][0], 0); g_value_set_int (gimp_value_array_index (args, 3), 0); }
      else if (route == 1)
        { isolated_integer (dialog, isolated_fields[route][0], 1); g_value_set_int (gimp_value_array_index (args, 3), 1); }
      else if (route == 2)
        { isolated_integer (dialog, isolated_fields[route][1], 0); g_value_set_int (gimp_value_array_index (args, 4), 0); }
      else
        { isolated_real (dialog, isolated_fields[route][1], 1.0); g_value_set_double (gimp_value_array_index (args, 7), 1.0); }
      isolated_accept (dialog);
      g_assert_cmpuint (gimp_filter_layer_get_generation (filter), >, generation);
      isolated_pixel (filter, cached); isolated_assert_args (filter, route, args); isolated_assert_raw (filter);
      isolated_wait (filter, GIMP_FILTER_LAYER_CLEAN);
      g_assert_cmpuint (gimp_filter_layer_get_cache_generation (filter), ==, gimp_filter_layer_get_generation (filter));
      /* Re-evaluate the exact edited definition in an independent image and
       * compare every pixel, catching publication of the cancelled generation. */
      {
        GimpImage *reference = gimp_image_new (gimp, size, size, GIMP_RGB, GIMP_PRECISION_U8_NON_LINEAR);
        GimpLayer *input = gimp_layer_new (reference, size, size, babl_format ("R'G'B'A u8"),
          "reference source", 1, GIMP_LAYER_MODE_NORMAL_LEGACY);
        GimpFilterLayer *expected;
        gsize bytes = size * size * 4;
        guint8 *actual_pixels = g_malloc (bytes), *expected_pixels = g_malloc (bytes);
        g_assert_true (gimp_image_add_layer (reference, input, NULL, 0, FALSE));
        isolated_fill (input, source_pixel); expected = isolated_layer (reference, route, args);
        isolated_wait (expected, GIMP_FILTER_LAYER_CLEAN);
        gegl_buffer_get (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)), NULL, 1,
          babl_format ("R'G'B'A u8"), actual_pixels, GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);
        gegl_buffer_get (gimp_drawable_get_buffer (GIMP_DRAWABLE (expected)), NULL, 1,
          babl_format ("R'G'B'A u8"), expected_pixels, GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);
        g_assert_cmpmem (actual_pixels, bytes, expected_pixels, bytes);
        g_free (actual_pixels); g_free (expected_pixels); g_object_unref (reference);
      }
      gimp_value_array_unref (args); g_object_unref (image);
    }
}

static GimpImage *
isolated_roundtrip (GimpImage *image)
{
  GOutputStream *output = g_memory_output_stream_new_resizable ();
  GFile *file = g_file_new_for_path ("/tmp/painter-native-editor-roundtrip.xcf");
  GError *error = NULL;
  GBytes *bytes;
  GInputStream *input;
  GimpImage *copy;
  g_assert_true (xcf_save_stream (gimp, image, output, file, NULL, &error)); g_assert_no_error (error);
  g_assert_true (g_output_stream_close (output, NULL, &error)); g_assert_no_error (error);
  bytes = g_memory_output_stream_steal_as_bytes (G_MEMORY_OUTPUT_STREAM (output));
  input = g_memory_input_stream_new_from_bytes (bytes);
  copy = xcf_load_stream (gimp, input, file, NULL, &error); g_assert_no_error (error); g_assert_nonnull (copy);
  g_object_unref (input); g_bytes_unref (bytes); g_object_unref (output); g_object_unref (file);
  return copy;
}

static void
isolated_editor_precision_ranges (void)
{
  const gdouble smallest = nextafter (0.0, 1.0);
  const guint8 zero[] = { 0, 0, 0, 0 };
  gchar smallest_text[G_ASCII_DTOSTR_BUF_SIZE];

  g_assert_cmpfloat (smallest, >, 0.0);
  g_assert_cmpfloat (smallest, <, G_MINDOUBLE);
  g_ascii_dtostr (smallest_text, sizeof smallest_text, smallest);
  for (guint native = 0; native < 2; native++)
    {
      GimpImage *image = gimp_image_new (gimp, 16, 16, GIMP_RGB,
        native ? GIMP_PRECISION_DOUBLE_NON_LINEAR : GIMP_PRECISION_U8_NON_LINEAR);
      GimpLayer *source = gimp_layer_new (image, 16, 16, gimp_image_get_layer_format (image, TRUE),
        "zero source", 1, GIMP_LAYER_MODE_NORMAL_LEGACY);
      GimpValueArray *args = isolated_args (3, 3);
      GimpFilterLayer *filter;
      GtkWidget *dialog;
      gdouble matrix[25];

      g_assert_true (gimp_image_add_layer (image, source, NULL, 0, FALSE));
      isolated_fill (source, zero); filter = isolated_layer (image, 3, args);
      /* These are parameter parser/storage tests. Cancel before running the
       * main context, so extreme accepted doubles are never evaluated. */
      gimp_filter_layer_cancel (filter);
      memcpy (matrix, gimp_value_get_double_array (gimp_value_array_index (args, 4), NULL), sizeof matrix);
      if (native)
        {
          guint64 revision;
          gdouble parsed;

          dialog = isolated_editor (image, filter, 3);
          isolated_text (dialog, "painter-convmatrix-0", "1e100");
          isolated_text (dialog, "painter-convmatrix-divisor", "1e100");
          isolated_accept (dialog); gimp_filter_layer_cancel (filter);
          matrix[0] = 1e100; gimp_value_set_double_array (gimp_value_array_index (args, 4), matrix, 25);
          g_value_set_double (gimp_value_array_index (args, 6), 1e100);
          isolated_assert_args (filter, 3, args); isolated_assert_raw (filter);

          dialog = isolated_editor (image, filter, 3);
          isolated_text (dialog, "painter-convmatrix-offset", "0.25");
          isolated_accept (dialog); gimp_filter_layer_cancel (filter);
          g_value_set_double (gimp_value_array_index (args, 7), 0.25);
          isolated_assert_args (filter, 3, args); isolated_assert_raw (filter);

          dialog = isolated_editor (image, filter, 3);
          isolated_text (dialog, "painter-convmatrix-24", smallest_text);
          isolated_text (dialog, "painter-convmatrix-divisor", smallest_text);
          isolated_accept (dialog); gimp_filter_layer_cancel (filter);
          matrix[24] = smallest; gimp_value_set_double_array (gimp_value_array_index (args, 4), matrix, 25);
          g_value_set_double (gimp_value_array_index (args, 6), smallest);
          isolated_assert_args (filter, 3, args); isolated_assert_raw (filter);

          revision = gimp_filter_layer_get_definition_revision (filter);
          dialog = isolated_editor (image, filter, 3);
          parsed = g_ascii_strtod (gtk_entry_get_text (GTK_ENTRY (field (dialog, "painter-convmatrix-24"))), NULL);
          g_assert_cmpmem (&parsed, sizeof parsed, &smallest, sizeof smallest);
          parsed = g_ascii_strtod (gtk_entry_get_text (GTK_ENTRY (field (dialog, "painter-convmatrix-divisor"))), NULL);
          g_assert_cmpmem (&parsed, sizeof parsed, &smallest, sizeof smallest);
          isolated_accept (dialog);
          g_assert_cmpuint (gimp_filter_layer_get_definition_revision (filter), ==, revision);
          isolated_assert_args (filter, 3, args);
        }
      else
        {
          const gchar *names[] = { "painter-convmatrix-0", "painter-convmatrix-divisor",
            "painter-convmatrix-offset", "painter-convmatrix-divisor", "painter-convmatrix-divisor",
            "painter-convmatrix-24" };
          const gchar *texts[] = { "1e100", "1e100", "-1e100", "1e-100", smallest_text, " \t " };
          guint64 revision = gimp_filter_layer_get_definition_revision (filter);
          guint64 generation = gimp_filter_layer_get_generation (filter);

          for (guint i = 0; i < G_N_ELEMENTS (names); i++)
            {
              gboolean closed = FALSE;
              dialog = isolated_editor (image, filter, 3);
              g_signal_connect (dialog, "destroy", G_CALLBACK (destroyed), &closed);
              isolated_text (dialog, names[i], texts[i]);
              gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
              g_assert_false (closed);
              g_assert_true (gtk_widget_get_visible (field (dialog, "painter-layer-error")));
              g_assert_cmpstr (gtk_entry_get_text (GTK_ENTRY (field (dialog, names[i]))), ==, texts[i]);
              g_assert_cmpuint (gimp_filter_layer_get_definition_revision (filter), ==, revision);
              g_assert_cmpuint (gimp_filter_layer_get_generation (filter), ==, generation);
              isolated_assert_args (filter, 3, args); isolated_assert_raw (filter);
              gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL); g_object_unref (dialog);
            }
          g_assert_cmpint (gimp_undo_stack_get_depth (gimp_image_get_undo_stack (image)), ==, 0);
        }
      g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 0);
      gimp_value_array_unref (args); g_object_unref (image);
    }
}

static void
isolated_editor_save_reopen (void)
{
  const guint8 source_pixel[] = { 31, 121, 217, 255 };
  for (guint case_id = 0; case_id < 7; case_id++)
    {
      guint route = MIN (case_id, 3), tail = case_id > 3 ? case_id - 3 : 0;
      GimpImage *image = new_image (), *copy;
      GimpLayer *source = new_source (image, NULL, "source");
      GimpValueArray *args = isolated_args (route, tail);
      GimpFilterLayer *filter;
      GtkWidget *dialog;
      guint64 revision;
      guint8 pixels[16 * 16 * 4], loaded_pixels[sizeof pixels];
      isolated_fill (source, source_pixel); filter = isolated_layer (image, route, args); settle (filter);
      gegl_buffer_get (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)), NULL, 1,
        babl_format ("R'G'B'A u8"), pixels, GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);
      copy = isolated_roundtrip (image); g_object_unref (image);
      filter = GIMP_FILTER_LAYER (gimp_container_get_first_child (gimp_image_get_layers (copy)));
      g_assert_true (GIMP_IS_FILTER_LAYER (filter));
      isolated_assert_args (filter, route, args); isolated_assert_raw (filter);
      g_assert_cmpint (gimp_filter_layer_get_state (filter), ==, GIMP_FILTER_LAYER_CLEAN);
      g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 0);
      gegl_buffer_get (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)), NULL, 1,
        babl_format ("R'G'B'A u8"), loaded_pixels, GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);
      g_assert_cmpmem (pixels, sizeof pixels, loaded_pixels, sizeof loaded_pixels);
      revision = gimp_filter_layer_get_definition_revision (filter);
      isolated_accept (isolated_editor (copy, filter, route));
      g_assert_cmpuint (gimp_filter_layer_get_definition_revision (filter), ==, revision);
      dialog = isolated_editor (copy, filter, route); isolated_edit_one (dialog, route, args); isolated_accept (dialog);
      isolated_assert_args (filter, route, args); isolated_assert_raw (filter); settle (filter);
      image = isolated_roundtrip (copy); g_object_unref (copy);
      filter = GIMP_FILTER_LAYER (gimp_container_get_first_child (gimp_image_get_layers (image)));
      isolated_assert_args (filter, route, args); isolated_assert_raw (filter);
      isolated_accept (isolated_editor (image, filter, route));
      gimp_value_array_unref (args); g_object_unref (image);
    }
}
