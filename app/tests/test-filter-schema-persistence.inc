/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Phase D: generated modern scenes, real GTK schema editors and ordinary
 * File Save/Open entry points. These are not historical old-writer fixtures. */
#include "core/gimp-painter-provenance.h"
#include "core/gimpimage-duplicate.h"
#include "file/file-open.h"
#include "file/file-save.h"
#include "xcf/painter-xcf-preserve.h"

static GFile *
persistence_file (void)
{
  GFileIOStream *io = NULL;
  GError *error = NULL;
  GFile *file = g_file_new_tmp ("painter-schema-XXXXXX.xcf", &io, &error);
  g_assert_no_error (error); g_assert_nonnull (file);
  g_assert_true (g_io_stream_close (G_IO_STREAM (io), NULL, &error));
  g_assert_no_error (error); g_object_unref (io);
  return file;
}

static void
persistence_cleanup_file (GFile *file)
{
  GError *error = NULL;
  g_assert_true (g_file_delete (file, NULL, &error)); g_assert_no_error (error);
  g_object_unref (file);
}

static void
persistence_save (GimpImage *image, GFile *file)
{
  GError *error = NULL;
  GimpPlugInProcedure *procedure = GIMP_PLUG_IN_PROCEDURE (
    gimp_pdb_lookup_procedure (gimp->pdb, "gimp-xcf-save"));
  GimpPDBStatusType status = file_save (gimp, image, NULL, file, procedure,
    GIMP_RUN_NONINTERACTIVE, FALSE, FALSE, FALSE, &error);
  if (error) g_test_message ("Ordinary schema Save: %s", error->message);
  g_assert_no_error (error); g_assert_cmpint (status, ==, GIMP_PDB_SUCCESS);
}

static GimpImage *
persistence_load (GFile *file)
{
  GError *error = NULL;
  GimpPDBStatusType status;
  GimpPlugInProcedure *procedure = GIMP_PLUG_IN_PROCEDURE (
    gimp_pdb_lookup_procedure (gimp->pdb, "gimp-xcf-load"));
  GimpImage *image = file_open_image (gimp, gimp_get_user_context (gimp), NULL,
    file, 0, 0, FALSE, procedure, GIMP_RUN_NONINTERACTIVE, &status, NULL, &error);
  g_assert_no_error (error); g_assert_cmpint (status, ==, GIMP_PDB_SUCCESS);
  g_assert_nonnull (image); return image;
}

static GimpFilterLayer *
persistence_filter (GimpImage *image)
{
  GList *layers = gimp_image_get_layer_list (image);
  GimpFilterLayer *filter = NULL;
  for (GList *p = layers; p && !filter; p = p->next)
    if (GIMP_IS_FILTER_LAYER (p->data)) filter = p->data;
  g_list_free (layers); g_assert_nonnull (filter); return filter;
}

static GimpImage *
persistence_close_reopen (GimpImage *image, GFile *file)
{
  gpointer weak = image;
  g_object_add_weak_pointer (G_OBJECT (image), &weak);
  persistence_save (image, file);
  g_object_unref (image); g_assert_null (weak);
  return persistence_load (file);
}

static void
persistence_values (const GimpFilterArgumentsSnapshot *snapshot,
                    const GimpValueArray *expected)
{
  g_assert_nonnull (snapshot);
  g_assert_cmpuint (gimp_filter_arguments_snapshot_count (snapshot), ==,
                    gimp_value_array_length (expected));
  for (guint i = 0; i < gimp_value_array_length (expected); i++)
    {
      const GValue *b = gimp_value_array_index (expected, i);
      const GValue *a = gimp_filter_arguments_snapshot_peek_value (snapshot, i);
      GType type = G_VALUE_TYPE (b);
      g_assert_cmpuint (gimp_filter_arguments_snapshot_type (snapshot, i), ==, type);
      if (type == GIMP_TYPE_VALUE_ARRAY)
        {
          GimpFilterArgumentsSnapshot *nested = gimp_filter_arguments_snapshot_nested (snapshot, i);
          const GimpValueArray *values = g_value_get_boxed (b);
          g_assert_cmpint (gimp_filter_arguments_snapshot_is_null (snapshot, i), ==, values == NULL);
          if (values) persistence_values (nested, values); else g_assert_null (nested);
          gimp_filter_arguments_snapshot_free (nested); continue;
        }
      g_assert_nonnull (a); g_assert_cmpuint (G_VALUE_TYPE (a), ==, type);
      if (type == G_TYPE_INT) g_assert_cmpint (g_value_get_int (a), ==, g_value_get_int (b));
      else if (type == G_TYPE_INT64) g_assert_cmpint (g_value_get_int64 (a), ==, g_value_get_int64 (b));
      else if (type == G_TYPE_UINT64) g_assert_cmpuint (g_value_get_uint64 (a), ==, g_value_get_uint64 (b));
      else if (type == G_TYPE_BOOLEAN) g_assert_cmpint (g_value_get_boolean (a), ==, g_value_get_boolean (b));
      else if (type == G_TYPE_DOUBLE)
        { gdouble x = g_value_get_double (a), y = g_value_get_double (b); g_assert_cmpmem (&x, sizeof x, &y, sizeof y); }
      else if (type == G_TYPE_FLOAT)
        { gfloat x = g_value_get_float (a), y = g_value_get_float (b); g_assert_cmpmem (&x, sizeof x, &y, sizeof y); }
      else if (type == G_TYPE_STRING) g_assert_cmpstr (g_value_get_string (a), ==, g_value_get_string (b));
      else if (type == G_TYPE_BYTES || type == G_TYPE_VARIANT)
        {
          gpointer x = type == G_TYPE_BYTES ? g_value_get_boxed (a) : g_value_get_variant (a);
          gpointer y = type == G_TYPE_BYTES ? g_value_get_boxed (b) : g_value_get_variant (b);
          if (y) { g_assert_nonnull (x); g_assert_true (type == G_TYPE_BYTES ? g_bytes_equal (x, y) : g_variant_equal (x, y)); }
          else g_assert_null (x);
        }
      else if (type == G_TYPE_STRV)
        {
          gchar **x = g_value_get_boxed (a), **y = g_value_get_boxed (b);
          if (y) { guint j = 0; g_assert_nonnull (x); while (y[j]) { g_assert_cmpstr (x[j], ==, y[j]); j++; } g_assert_null (x[j]); }
          else g_assert_null (x);
        }
      else
        {
          const GimpArray *x = g_value_get_boxed (a), *y = g_value_get_boxed (b);
          g_assert_true (type == GIMP_TYPE_ARRAY || type == GIMP_TYPE_INT32_ARRAY || type == GIMP_TYPE_DOUBLE_ARRAY);
          if (y) { g_assert_nonnull (x); g_assert_cmpmem (x->data, x->length, y->data, y->length); }
          else g_assert_null (x);
        }
    }
}

static void
persistence_args (GimpFilterLayer *filter, const GimpValueArray *expected)
{
  GimpFilterArgumentsSnapshot *snapshot = gimp_filter_layer_snapshot_arguments (filter);
  persistence_values (snapshot, expected); gimp_filter_arguments_snapshot_free (snapshot);
  isolated_assert_raw (filter);
}

static GVariant *
persistence_capsule (GimpFilterLayer *filter)
{
  GimpPainterProvenanceTransport *transport = gimp_painter_provenance_ref_transport (G_OBJECT (filter));
  GBytes *body;
  GVariant *dict;
  if (transport && transport->present[GIMP_PAINTER_PROVENANCE_NAMESPACE_ITEM])
    {
      GBytes *capsule = transport->capsules[GIMP_PAINTER_PROVENANCE_NAMESPACE_ITEM];
      g_assert_false (transport->invalid[GIMP_PAINTER_PROVENANCE_NAMESPACE_ITEM]);
      g_assert_nonnull (capsule);
      body = g_bytes_new_from_bytes (capsule, 12, g_bytes_get_size (capsule) - 12);
    }
  else
    {
      const GimpParasite *parasite = gimp_item_parasite_find (GIMP_ITEM (filter), "gimp-painter-item");
      guint32 size;
      const guint8 *bytes;
      g_assert_nonnull (parasite); bytes = gimp_parasite_get_data (parasite, &size);
      g_assert_cmpuint (size, >, 12); g_assert_cmpmem (bytes, 12, "GPXCF\0\0\0\1\0\0\0", 12);
      body = g_bytes_new (bytes + 12, size - 12);
    }
  dict = g_variant_ref_sink (g_variant_new_from_bytes (G_VARIANT_TYPE_VARDICT, body, FALSE));
  g_bytes_unref (body); gimp_painter_provenance_free_transport (transport);
  g_assert_true (g_variant_is_normal_form (dict)); return dict;
}

static void
persistence_noop (GimpImage *image, GimpFilterLayer *filter, guint route)
{
  SchemaCheckpoint before = schema_checkpoint (image, filter);
  GtkWidget *dialog = isolated_editor (image, filter, route);
  isolated_accept (dialog); schema_assert_checkpoint (image, filter, &before);
  dialog = isolated_editor (image, filter, route);
  {
    GtkWidget *entry = field (dialog, schema_edit_fields[route]);
    gchar *equivalent = g_strdup_printf (" %s ", gtk_entry_get_text (GTK_ENTRY (entry)));
    isolated_text (dialog, schema_edit_fields[route], equivalent); g_free (equivalent);
    if (route == 0 || route == 3)
      {
        GtkToggleButton *flag = GTK_TOGGLE_BUTTON (field (dialog, route == 0 ?
          "painter-blinds-transparent" : "painter-convmatrix-channel-1"));
        gboolean active = gtk_toggle_button_get_active (flag);
        gtk_toggle_button_set_active (flag, !active); gtk_toggle_button_set_active (flag, active);
      }
    isolated_accept (dialog); schema_assert_checkpoint (image, filter, &before);
  }
  dialog = isolated_editor (image, filter, route);
  isolated_text (dialog, schema_edit_fields[route], "invalid unsaved input");
  gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL); g_object_unref (dialog);
  schema_assert_checkpoint (image, filter, &before);
}

static void
filter_schema_persistence_routes (void)
{
  const guint8 source_pixel[] = { 31, 121, 217, 255 };
  for (guint case_id = 0; case_id < 7; case_id++)
    {
      guint route = MIN (case_id, 3), tail = case_id > 3 ? case_id - 3 : 0;
      GFile *file = persistence_file ();
      GimpImage *image = new_image (), *duplicate;
      GimpLayer *source = new_source (image, NULL, "source");
      GimpValueArray *args = isolated_args (route, tail), *original = gimp_value_array_copy (args);
      GimpFilterLayer *filter;
      GtkWidget *dialog;
      guint8 pixels[16 * 16 * 4], reopened_pixels[sizeof pixels];
      g_test_message ("ordinary Save/Close/Open/re-edit route=%u tail=%u", route, tail);
      isolated_fill (source, source_pixel); filter = isolated_layer (image, route, args); settle (filter);
      gegl_buffer_get (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)), NULL, 1,
        babl_format ("R'G'B'A u8"), pixels, GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);
      image = persistence_close_reopen (image, file); filter = persistence_filter (image);
      persistence_args (filter, args); persistence_noop (image, filter, route);
      g_assert_cmpint (gimp_filter_layer_get_state (filter), ==, GIMP_FILTER_LAYER_CLEAN);
      g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 0);
      g_assert_cmpuint (gimp_filter_layer_get_cache_generation (filter), ==, gimp_filter_layer_get_generation (filter));
      gegl_buffer_get (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)), NULL, 1,
        babl_format ("R'G'B'A u8"), reopened_pixels, GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);
      g_assert_cmpmem (pixels, sizeof pixels, reopened_pixels, sizeof reopened_pixels);
      dialog = isolated_editor (image, filter, route); isolated_edit_one (dialog, route, args); isolated_accept (dialog);
      g_assert_cmpint (gimp_undo_stack_get_depth (gimp_image_get_undo_stack (image)), ==, 1);
      persistence_args (filter, args);
      /* Persist all three history states while the owning image keeps history. */
      for (guint history = 0; history < 3; history++)
        {
          GimpImage *readback;
          GimpFilterLayer *saved;
          GimpFilterLayerSnapshot state, restored;
          const GimpValueArray *expected = history == 1 ? original : args;
          if (history == 1) g_assert_true (gimp_image_undo (image));
          if (history == 2) g_assert_true (gimp_image_redo (image));
          persistence_args (filter, expected);
          g_assert_true (gimp_filter_layer_get_snapshot_state (filter, &state));
          persistence_save (image, file); readback = persistence_load (file); saved = persistence_filter (readback);
          persistence_args (saved, expected); persistence_noop (readback, saved, route);
          g_assert_true (gimp_filter_layer_get_snapshot_state (saved, &restored));
          g_assert_cmpint (restored.cache_complete, ==, state.cache_complete);
          g_assert_cmpint (restored.generation == restored.cache_generation, ==,
                           state.generation == state.cache_generation);
          g_assert_cmpuint (gimp_filter_layer_get_run_count (saved), ==, 0);
          g_object_unref (readback);
        }
      duplicate = gimp_image_duplicate (image); g_assert_nonnull (duplicate);
      persistence_args (persistence_filter (duplicate), args);
      /* Re-edit the duplicate, retaining the original image's definition. */
      dialog = isolated_editor (duplicate, persistence_filter (duplicate), route);
      isolated_text (dialog, schema_edit_fields[route], schema_equivalent[route]); isolated_accept (dialog);
      persistence_args (persistence_filter (duplicate), original); persistence_args (filter, args);
      duplicate = persistence_close_reopen (duplicate, file);
      persistence_args (persistence_filter (duplicate), original);
      persistence_noop (duplicate, persistence_filter (duplicate), route);
      g_object_unref (duplicate); g_object_unref (image);
      gimp_value_array_unref (original); gimp_value_array_unref (args); persistence_cleanup_file (file);
    }
}

static void
filter_schema_persistence_special_bits (void)
{
  const guint64 bits[] = { G_GUINT64_CONSTANT (0x7ff8000000000042), G_GUINT64_CONSTANT (0x7ff0000000000000),
                          G_GUINT64_CONSTANT (0xfff0000000000000), G_GUINT64_CONSTANT (0x8000000000000000) };
  for (guint case_id = 0; case_id < 5; case_id++)
    {
      guint route = case_id < 4 ? 2 : 3;
      GFile *file = persistence_file ();
      GimpImage *image = new_image ();
      GimpValueArray *args = isolated_args (route, 0), *original;
      GimpFilterLayer *filter;
      GtkWidget *dialog;
      gdouble numbers[4];
      for (guint i = 0; i < 4; i++) memcpy (&numbers[i], &bits[i], sizeof numbers[i]);
      if (route == 2) g_value_set_double (gimp_value_array_index (args, 6), numbers[case_id]);
      else
        {
          gdouble matrix[25];
          memcpy (matrix, gimp_value_get_double_array (gimp_value_array_index (args, 4), NULL), sizeof matrix);
          memcpy (matrix, numbers, sizeof numbers); gimp_value_set_double_array (gimp_value_array_index (args, 4), matrix, 25);
          g_value_set_double (gimp_value_array_index (args, 6), numbers[1]);
          g_value_set_int (gimp_value_array_index (args, 10), 919191);
        }
      original = gimp_value_array_copy (args); filter = isolated_layer (image, route, args);
      gimp_filter_layer_mark_as_loaded (filter);
      for (guint pass = 0; pass < 2; pass++)
        {
          image = persistence_close_reopen (image, file); filter = persistence_filter (image);
          persistence_args (filter, args); persistence_noop (image, filter, route);
          dialog = isolated_editor (image, filter, route);
          if (route == 2)
            { isolated_text (dialog, "painter-retinex-scale", pass ? "129" : "128"); g_value_set_int (gimp_value_array_index (args, 3), pass ? 129 : 128); }
          else
            { isolated_text (dialog, "painter-convmatrix-offset", pass ? "0.25" : "0.125"); g_value_set_double (gimp_value_array_index (args, 7), pass ? .25 : .125); }
          isolated_accept (dialog); persistence_args (filter, args);
          g_assert_true (gimp_image_undo (image)); persistence_args (filter, original);
          g_assert_true (gimp_image_redo (image)); persistence_args (filter, args);
          gimp_value_array_unref (original); original = gimp_value_array_copy (args);
        }
      image = persistence_close_reopen (image, file); persistence_args (persistence_filter (image), args);
      gimp_value_array_unref (original); gimp_value_array_unref (args); g_object_unref (image); persistence_cleanup_file (file);
    }
}

static void
filter_schema_persistence_large_tails (void)
{
  for (guint kind = 0; kind < 5; kind++)
    {
      GFile *file = persistence_file ();
      GimpImage *image = new_image ();
      GimpValueArray *args = isolated_args (3, 0);
      GValue tail = G_VALUE_INIT;
      GimpFilterLayer *filter;
      GtkWidget *dialog;
      schema_large_value (&tail, kind); gimp_value_array_append (args, &tail); g_value_unset (&tail);
      filter = isolated_layer (image, 3, args); gimp_filter_layer_mark_as_loaded (filter);
      for (guint pass = 0; pass < 2; pass++)
        {
          GimpPainterProvenanceTransport *transport;
          image = persistence_close_reopen (image, file); filter = persistence_filter (image);
          transport = gimp_painter_provenance_ref_transport (G_OBJECT (filter));
          g_assert_nonnull (transport); g_assert_true (transport->present[GIMP_PAINTER_PROVENANCE_NAMESPACE_ITEM]);
          g_assert_cmpuint (g_bytes_get_size (transport->capsules[GIMP_PAINTER_PROVENANCE_NAMESPACE_ITEM]), >, 1024 * 1024);
          gimp_painter_provenance_free_transport (transport);
          persistence_args (filter, args); persistence_noop (image, filter, 3);
          dialog = isolated_editor (image, filter, 3); schema_assert_preview (dialog);
          isolated_text (dialog, "painter-convmatrix-offset", pass ? "0.25" : "0.125"); isolated_accept (dialog);
          g_value_set_double (gimp_value_array_index (args, 7), pass ? .25 : .125); persistence_args (filter, args);
          g_assert_true (gimp_image_undo (image));
          g_value_set_double (gimp_value_array_index (args, 7), pass ? .125 : -0.0); persistence_args (filter, args);
          g_assert_true (gimp_image_redo (image));
          g_value_set_double (gimp_value_array_index (args, 7), pass ? .25 : .125); persistence_args (filter, args);
        }
      image = persistence_close_reopen (image, file); persistence_args (persistence_filter (image), args);
      gimp_value_array_unref (args); g_object_unref (image); persistence_cleanup_file (file);
    }
}

static void
persistence_recorded_references (GVariant *dict, const gchar *key,
                                 const GimpFilterArgumentReference *expected)
{
  GVariant *arguments = g_variant_lookup_value (dict, key, G_VARIANT_TYPE ("a(sbv)"));
  GVariant *entry, *wrapped, *refs;
  g_assert_nonnull (arguments); g_assert_cmpuint (g_variant_n_children (arguments), ==, 12);
  entry = g_variant_get_child_value (arguments, 11);
  wrapped = g_variant_get_child_value (entry, 2); refs = g_variant_get_variant (wrapped);
  g_assert_cmpuint (g_variant_n_children (refs), ==, 4);
  for (guint i = 0; i < 4; i++)
    {
      GVariant *ref = g_variant_get_child_value (refs, i);
      guint32 kind, tattoo;
      gint64 id;
      const gchar *type;
      gboolean was_set, expired;
      g_variant_get (ref, "(uux&sbb)", &kind, &tattoo, &id, &type, &was_set, &expired);
      g_assert_cmpint (id, ==, expected[i].id);
      g_assert_cmpstr (type, ==, g_type_name (expected[i].object_type));
      g_assert_true (was_set); g_assert_cmpint (expired, ==, i >= 2);
      g_assert_cmpuint (kind, ==, i == 0 ? 1 : i == 1 ? 2 : 3);
      if (i == 1) g_assert_cmpuint (tattoo, >, 0); else g_assert_cmpuint (tattoo, ==, 0);
      g_variant_unref (ref);
    }
  g_variant_unref (refs); g_variant_unref (wrapped); g_variant_unref (entry); g_variant_unref (arguments);
}

static void
persistence_external_origin (GVariant *dict, gint64 external_id)
{
  GVariant *origins = g_variant_lookup_value (dict, "external-reference-origins", G_VARIANT_TYPE ("aa{sv}"));
  GVariant *origin;
  gint64 object_id, image_id;
  const gchar *type, *role;
  g_assert_nonnull (origins); g_assert_cmpuint (g_variant_n_children (origins), ==, 1);
  origin = g_variant_get_child_value (origins, 0);
  g_assert_true (g_variant_lookup (origin, "object-runtime-id", "x", &object_id));
  g_assert_true (g_variant_lookup (origin, "image-runtime-id", "x", &image_id));
  g_assert_cmpint (object_id, ==, external_id); g_assert_cmpint (image_id, ==, external_id);
  g_assert_true (g_variant_lookup (origin, "declared-type", "&s", &type));
  g_assert_cmpstr (type, ==, g_type_name (GIMP_TYPE_IMAGE));
  g_assert_true (g_variant_lookup (origin, "role", "&s", &role));
  g_assert_cmpstr (role, ==, "filter-argument");
  g_variant_unref (origin); g_variant_unref (origins);
}

static void
filter_schema_persistence_reference_identity (void)
{
  GimpImage *image = new_image (), *external = new_image (), *copy;
  GimpLayer *source = new_source (image, NULL, "actual reference target");
  GimpLayer *collision = new_source (image, NULL, "recorded ID collision");
  GimpValueArray *args = isolated_args (3, 0);
  GimpFilterLayer *filter = isolated_layer (image, 3, args), *reopened;
  GimpFilterArgumentSpec specs[12] = {{0}};
  GimpFilterArgumentReference refs[4] = {
    { GIMP_TYPE_VIEWABLE, G_MAXINT64, TRUE, FALSE },
    { GIMP_TYPE_DRAWABLE, gimp_item_get_id (GIMP_ITEM (collision)), TRUE, FALSE },
    { GIMP_TYPE_ITEM, G_GINT64_CONSTANT (-987654321012345), TRUE, TRUE },
    { GIMP_TYPE_IMAGE, gimp_image_get_id (image), TRUE, FALSE }
  };
  GObject *targets[] = { G_OBJECT (image), G_OBJECT (source), NULL, G_OBJECT (external) };
  GimpFilterArgumentsSnapshot *imported;
  GBytes *raw = g_bytes_new_static (isolated_raw, sizeof isolated_raw);
  GFile *file = persistence_file ();
  GtkWidget *dialog;
  GError *error = NULL;
  gint64 external_id = gimp_image_get_id (external);
  for (guint i = 0; i < 11; i++)
    { specs[i].value = gimp_value_array_index (args, i); specs[i].value_type = G_VALUE_TYPE (specs[i].value); }
  specs[11].value_type = GIMP_TYPE_CORE_OBJECT_ARRAY; specs[11].n_references = 4;
  specs[11].references = refs; specs[11].targets = targets;
  imported = gimp_filter_arguments_snapshot_import (12, specs, &error);
  g_assert_no_error (error); g_assert_nonnull (imported);
  g_assert_true (gimp_filter_layer_set_definition_with_snapshot (filter, "plug-in-convmatrix", raw, imported, &error));
  g_assert_no_error (error); gimp_filter_arguments_snapshot_free (imported); g_bytes_unref (raw);
  gimp_filter_layer_mark_as_loaded (filter);
  dialog = isolated_editor (image, filter, 3);
  isolated_text (dialog, "painter-convmatrix-offset", "0.125"); isolated_accept (dialog);
  schema_assert_reference_tail (filter, GIMP_TYPE_CORE_OBJECT_ARRAY, FALSE, 4, refs, .125);
  persistence_save (image, file); copy = persistence_load (file); reopened = persistence_filter (copy);
  {
    GVariant *dict = persistence_capsule (reopened);
    persistence_external_origin (dict, external_id);
    persistence_recorded_references (dict, "arguments", refs); g_variant_unref (dict);
  }
  for (guint pass = 0; pass < 2; pass++)
    {
      GimpFilterArgumentReference current[4];
      GimpFilterArgumentsSnapshot *snapshot = gimp_filter_layer_snapshot_arguments (reopened);
      GList *layers = gimp_image_get_layer_list (copy);
      GimpLayer *actual = NULL;
      for (GList *p = layers; p; p = p->next)
        if (!g_strcmp0 (gimp_object_get_name (p->data), "actual reference target")) actual = p->data;
      g_list_free (layers); g_assert_nonnull (actual);
      memcpy (current, refs, sizeof current);
      current[0].id = gimp_image_get_id (copy); current[1].id = gimp_item_get_id (GIMP_ITEM (actual));
      current[3].expired = TRUE;
      g_assert_cmpint (current[0].id, !=, refs[0].id);
      g_assert_cmpint (current[1].id, !=, refs[1].id);
      gimp_filter_arguments_snapshot_free (snapshot);
      schema_assert_reference_tail (reopened, GIMP_TYPE_CORE_OBJECT_ARRAY, FALSE, 4, current, pass ? .25 : .125);
      persistence_noop (copy, reopened, 3);
      dialog = isolated_editor (copy, reopened, 3);
      isolated_text (dialog, "painter-convmatrix-offset", pass ? "0.375" : "0.25"); isolated_accept (dialog);
      schema_assert_reference_tail (reopened, GIMP_TYPE_CORE_OBJECT_ARRAY, FALSE, 4, current, pass ? .375 : .25);
      g_assert_true (gimp_image_undo (copy));
      schema_assert_reference_tail (reopened, GIMP_TYPE_CORE_OBJECT_ARRAY, FALSE, 4, current, pass ? .25 : .125);
      g_assert_true (gimp_image_redo (copy));
      copy = persistence_close_reopen (copy, file); reopened = persistence_filter (copy);
      {
        GVariant *dict = persistence_capsule (reopened);
        persistence_external_origin (dict, external_id);
        /* The first imported record remains historical evidence even as the
         * active descriptor uses the newly allocated live runtime identities. */
        persistence_recorded_references (dict, "original-argument-model", refs);
        persistence_recorded_references (dict, "arguments", current); g_variant_unref (dict);
      }
    }
  /* Saving did not replace or expire any original live weak binding. */
  schema_assert_reference_tail (filter, GIMP_TYPE_CORE_OBJECT_ARRAY, FALSE, 4, refs, .125);
  g_object_unref (external); refs[3].expired = TRUE;
  schema_assert_reference_tail (filter, GIMP_TYPE_CORE_OBJECT_ARRAY, FALSE, 4, refs, .125);
  gimp_value_array_unref (args); g_object_unref (copy); g_object_unref (image); persistence_cleanup_file (file);
}

static void
filter_schema_persistence_nested_null (void)
{
  for (guint kind = 0; kind < 9; kind++)
    {
      GFile *file = persistence_file ();
      GimpImage *image = new_image ();
      GimpValueArray *args = isolated_args (3, 0);
      GValue tail = G_VALUE_INIT;
      GimpFilterLayer *filter;
      GtkWidget *dialog;
      const GType types[] = { G_TYPE_STRING, G_TYPE_STRV, GIMP_TYPE_ARRAY, GIMP_TYPE_VALUE_ARRAY };
      g_value_init (&tail, kind == 8 ? GIMP_TYPE_VALUE_ARRAY : types[kind / 2]);
      if (kind == 1) g_value_set_string (&tail, "");
      if (kind == 3) { gchar *empty[] = { NULL }; g_value_set_boxed (&tail, empty); }
      if (kind == 5) g_value_take_boxed (&tail, gimp_array_new (NULL, 0, FALSE));
      if (kind == 7) g_value_take_boxed (&tail, gimp_value_array_new (0));
      if (kind == 8)
        {
          GimpValueArray *nested = gimp_value_array_new_from_types (NULL,
            G_TYPE_INT64, G_MININT64, G_TYPE_UINT64, G_MAXUINT64,
            G_TYPE_DOUBLE, -0.0, G_TYPE_STRING, NULL, G_TYPE_STRING, "",
            GIMP_TYPE_VALUE_ARRAY, NULL, G_TYPE_VARIANT, NULL, G_TYPE_NONE);
          GValue number = G_VALUE_INIT;
          const guint32 special[] = { 0x7fc01234, 0x7f800000, 0xff800000, 0x80000000 };
          g_value_set_variant (gimp_value_array_index (nested, 6),
            g_variant_new ("(st)", "unknown nested variant payload", G_MAXUINT64));
          g_value_init (&number, G_TYPE_FLOAT);
          for (guint i = 0; i < G_N_ELEMENTS (special); i++)
            { gfloat value; memcpy (&value, &special[i], sizeof value); g_value_set_float (&number, value); gimp_value_array_append (nested, &number); }
          g_value_unset (&number); g_value_take_boxed (&tail, nested);
        }
      gimp_value_array_append (args, &tail); g_value_unset (&tail);
      filter = isolated_layer (image, 3, args); gimp_filter_layer_mark_as_loaded (filter);
      for (guint pass = 0; pass < 2; pass++)
        {
          GimpImage *duplicate;
          image = persistence_close_reopen (image, file); filter = persistence_filter (image);
          persistence_args (filter, args); persistence_noop (image, filter, 3);
          duplicate = gimp_image_duplicate (image); persistence_args (persistence_filter (duplicate), args);
          g_object_unref (duplicate);
          dialog = isolated_editor (image, filter, 3);
          isolated_text (dialog, "painter-convmatrix-offset", pass ? "0.25" : "0.125"); isolated_accept (dialog);
          g_value_set_double (gimp_value_array_index (args, 7), pass ? .25 : .125); persistence_args (filter, args);
          g_assert_true (gimp_image_undo (image));
          g_value_set_double (gimp_value_array_index (args, 7), pass ? .125 : -0.0); persistence_args (filter, args);
          g_assert_true (gimp_image_redo (image));
          g_value_set_double (gimp_value_array_index (args, 7), pass ? .25 : .125); persistence_args (filter, args);
        }
      image = persistence_close_reopen (image, file); persistence_args (persistence_filter (image), args);
      gimp_value_array_unref (args); g_object_unref (image); persistence_cleanup_file (file);
    }
}

static void
persistence_unknown_field (GimpFilterLayer *filter, GBytes *payload)
{
  static const guint8 magic[12] = { 'G', 'P', 'X', 'C', 'F', 0, 0, 0, 1, 0, 0, 0 };
  GVariant *base = persistence_capsule (filter), *updated;
  GVariantDict dict;
  GByteArray *wire = g_byte_array_new ();
  GimpParasite *parasite;
  g_variant_dict_init (&dict, base);
  g_variant_dict_insert_value (&dict, "phase-d-unknown-field", g_variant_new_from_bytes (G_VARIANT_TYPE ("ay"), payload, TRUE));
  updated = g_variant_ref_sink (g_variant_dict_end (&dict));
  /* Change the generated modern input capsule, not the loader's transient
   * binding-restoration dictionary. Ordinary Save must preserve its new key. */
#if G_BYTE_ORDER == G_BIG_ENDIAN
  { GVariant *swapped = g_variant_byteswap (updated); g_variant_unref (updated); updated = swapped; }
#endif
  g_byte_array_append (wire, magic, sizeof magic);
  g_byte_array_append (wire, g_variant_get_data (updated), g_variant_get_size (updated));
  g_assert_true (gimp_painter_provenance_set_transport (G_OBJECT (filter), NULL));
  parasite = gimp_parasite_new ("gimp-painter-item", GIMP_PARASITE_PERSISTENT, wire->len, wire->data);
  gimp_item_parasite_attach (GIMP_ITEM (filter), parasite, FALSE);
  gimp_parasite_free (parasite); g_byte_array_unref (wire);
  g_variant_unref (updated); g_variant_unref (base);
}

static void
persistence_assert_unknown (GimpFilterLayer *filter, GBytes *payload)
{
  GVariant *dict = persistence_capsule (filter);
  GVariant *unknown = g_variant_lookup_value (dict, "phase-d-unknown-field", G_VARIANT_TYPE ("ay"));
  GBytes *actual;
  GimpPainterProvenanceTransport *transport = gimp_painter_provenance_ref_transport (G_OBJECT (filter));
  g_assert_nonnull (unknown); actual = g_variant_get_data_as_bytes (unknown);
  g_assert_true (g_bytes_equal (actual, payload));
  g_assert_nonnull (transport); g_assert_true (transport->present[GIMP_PAINTER_PROVENANCE_NAMESPACE_ITEM]);
  g_assert_cmpuint (g_bytes_get_size (transport->capsules[GIMP_PAINTER_PROVENANCE_NAMESPACE_ITEM]), >, 1024 * 1024);
  gimp_painter_provenance_free_transport (transport);
  g_bytes_unref (actual); g_variant_unref (unknown); g_variant_unref (dict);
}

static void
filter_schema_persistence_unknown_opaque (void)
{
  const gsize size = 2 * 1024 * 1024 + 19;
  guint8 *data = g_malloc (size);
  GBytes *payload;
  for (gsize i = 0; i < size; i++) data[i] = (i * 37 + i / 251) & 255;
  payload = g_bytes_new_take (data, size);
  for (guint opaque = 0; opaque < 2; opaque++)
    {
      GFile *file = persistence_file ();
      GimpImage *image = new_image ();
      GimpValueArray *args = isolated_args (3, 0);
      GimpFilterLayer *filter = isolated_layer (image, 3, args);
      GBytes *opaque_bytes = NULL;
      if (opaque)
        {
          GVariantBuilder builder;
          GVariant *arguments, *wrapper;
          GBytes *raw = g_bytes_new_static (isolated_raw, sizeof isolated_raw);
          g_variant_builder_init (&builder, G_VARIANT_TYPE ("a(sbv)"));
          g_variant_builder_add (&builder, "(sbv)", "phase-d-unknown-type", FALSE,
            g_variant_new_from_bytes (G_VARIANT_TYPE ("ay"), payload, TRUE));
          arguments = g_variant_builder_end (&builder);
          wrapper = g_variant_ref_sink (g_variant_new_maybe (G_VARIANT_TYPE_VARIANT, g_variant_new_variant (arguments)));
#if G_BYTE_ORDER == G_BIG_ENDIAN
          { GVariant *swapped = g_variant_byteswap (wrapper); g_variant_unref (wrapper); wrapper = swapped; }
#endif
          opaque_bytes = g_variant_get_data_as_bytes (wrapper); g_variant_unref (wrapper);
          g_assert_true (gimp_filter_layer_set_definition_with_opaque_arguments (
            filter, "plug-in-convmatrix", raw, opaque_bytes, NULL)); g_bytes_unref (raw);
        }
      gimp_filter_layer_mark_as_loaded (filter);
      image = persistence_close_reopen (image, file); filter = persistence_filter (image);
      persistence_unknown_field (filter, payload);
      for (guint pass = 0; pass < 3; pass++)
        {
          GtkWidget *dialog;
          SchemaCheckpoint checkpoint;
          image = persistence_close_reopen (image, file); filter = persistence_filter (image);
          persistence_assert_unknown (filter, payload); isolated_assert_raw (filter);
          checkpoint = schema_checkpoint (image, filter);
          if (opaque)
            {
              GBytes *actual = gimp_filter_layer_ref_opaque_arguments (filter);
              g_assert_nonnull (actual); g_assert_true (g_bytes_equal (actual, opaque_bytes)); g_bytes_unref (actual);
              g_assert_null (gimp_filter_layer_snapshot_arguments (filter));
              dialog = new_filter_dialog (image, GIMP_LAYER (filter)); schema_assert_preview (dialog);
              g_assert_cmpstr (gtk_combo_box_get_active_id (GTK_COMBO_BOX (field (dialog, "painter-filter-choice"))), ==, "keep");
              isolated_accept (dialog); schema_assert_checkpoint (image, filter, &checkpoint);
              dialog = new_filter_dialog (image, GIMP_LAYER (filter));
              gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL); g_object_unref (dialog);
              schema_assert_checkpoint (image, filter, &checkpoint);
            }
          else
            {
              persistence_args (filter, args); persistence_noop (image, filter, 3);
              dialog = isolated_editor (image, filter, 3);
              isolated_text (dialog, "painter-convmatrix-offset", pass % 2 ? "0.25" : "0.125"); isolated_accept (dialog);
              g_value_set_double (gimp_value_array_index (args, 7), pass % 2 ? .25 : .125); persistence_args (filter, args);
              g_assert_true (gimp_image_undo (image)); g_assert_true (gimp_image_redo (image));
              persistence_args (filter, args);
            }
          if (pass == 1)
            { GimpImage *duplicate = gimp_image_duplicate (image); g_object_unref (image); image = duplicate; }
        }
      g_clear_pointer (&opaque_bytes, g_bytes_unref); gimp_value_array_unref (args);
      g_object_unref (image); persistence_cleanup_file (file);
    }
  g_bytes_unref (payload);
}

static void
filter_schema_persistence_active_save (void)
{
  const guint8 input[] = { 103, 71, 199, 255 }, cached[] = { 13, 29, 47, 255 };
  const gint size = 1025;
  const gsize bytes = (gsize) size * size * 4;
  guint8 *pixels = g_malloc (bytes), *expected = g_malloc (bytes), *completed = g_malloc (bytes);
  for (gsize i = 0; i < bytes; i += 4) memcpy (expected + i, cached, 4);
  for (guint scene = 0; scene < 5; scene++)
    {
      const guint route = scene < 4 ? scene : 1;
      const GimpFilterLayerState state = scene < 4 ? GIMP_FILTER_LAYER_RUNNING : GIMP_FILTER_LAYER_IMPORTING;
      GimpImage *image = gimp_image_new (gimp, size, size, GIMP_RGB, GIMP_PRECISION_U8_NON_LINEAR);
      GimpLayer *source = gimp_layer_new (image, size, size, babl_format ("R'G'B'A u8"),
        "active schema source", 1, GIMP_LAYER_MODE_NORMAL_LEGACY);
      GimpValueArray *args = isolated_args (route, route == 3 ? 1 : 0);
      GimpFilterLayer *filter;
      GFile *file = persistence_file ();
      GtkWidget *dialog;
      SchemaCheckpoint checkpoint;
      GimpFilterLayerSnapshot before, reopened;
      g_test_message ("Actual schema job Save state=%u route=%u size=%d", state, route, size);
      g_assert_true (gimp_image_add_layer (image, source, NULL, 0, FALSE)); isolated_fill (source, input);
      filter = isolated_layer (image, route, args);
      /* Explicit modern imported-cache fixture; the following edit creates a
       * real worker generation whose private output must not replace this cache. */
      isolated_fill (GIMP_LAYER (filter), cached); gimp_filter_layer_mark_as_loaded (filter);
      dialog = isolated_editor (image, filter, route); isolated_edit_one (dialog, route, args); isolated_accept (dialog);
      isolated_wait (filter, state);
      g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 1);
      g_assert_true (gimp_filter_layer_get_snapshot_state (filter, &before));
      g_assert_cmpuint (before.generation, >, before.cache_generation);
      checkpoint = schema_checkpoint (image, filter);
      dialog = isolated_editor (image, filter, route);
      isolated_text (dialog, schema_edit_fields[route], "uncommitted invalid text");
      persistence_save (image, file);
      schema_assert_checkpoint (image, filter, &checkpoint);
      g_assert_cmpint (gimp_filter_layer_get_state (filter), ==, state);
      persistence_args (filter, args);
      gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL); g_object_unref (dialog);
      g_object_unref (image); image = persistence_load (file); filter = persistence_filter (image);
      persistence_args (filter, args); persistence_noop (image, filter, route);
      g_assert_true (gimp_filter_layer_get_snapshot_state (filter, &reopened));
      g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 0);
      g_assert_cmpuint (reopened.generation, >, reopened.cache_generation);
      g_assert_cmpint (reopened.state, !=, GIMP_FILTER_LAYER_RUNNING);
      g_assert_cmpint (reopened.state, !=, GIMP_FILTER_LAYER_IMPORTING);
      gegl_buffer_get (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)), NULL, 1,
        babl_format ("R'G'B'A u8"), pixels, GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);
      g_assert_cmpmem (pixels, bytes, expected, bytes);
      isolated_wait (filter, GIMP_FILTER_LAYER_CLEAN);
      g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 1);
      g_assert_cmpuint (gimp_filter_layer_get_generation (filter), ==, gimp_filter_layer_get_cache_generation (filter));
      gegl_buffer_get (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)), NULL, 1,
        babl_format ("R'G'B'A u8"), completed, GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);
      g_assert_cmpint (memcmp (completed, expected, bytes), !=, 0);
      image = persistence_close_reopen (image, file); filter = persistence_filter (image);
      persistence_args (filter, args); persistence_noop (image, filter, route);
      g_assert_cmpint (gimp_filter_layer_get_state (filter), ==, GIMP_FILTER_LAYER_CLEAN);
      g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 0);
      gegl_buffer_get (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)), NULL, 1,
        babl_format ("R'G'B'A u8"), pixels, GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);
      g_assert_cmpmem (pixels, bytes, completed, bytes);
      gimp_value_array_unref (args); g_object_unref (image); persistence_cleanup_file (file);
    }
  g_free (completed); g_free (expected); g_free (pixels);
}

static void
filter_schema_persistence_reference_lease (void)
{
  for (guint kind = 0; kind < 3; kind++)
    {
      GimpImage *target_image = kind == 0 ? new_image () : NULL;
      gpointer weak_target = target_image;
      GObject *target = G_OBJECT (target_image), *lease = NULL;
      GimpFilterArgumentReference expected = { GIMP_TYPE_VIEWABLE, G_MAXINT64, kind != 2, kind == 1 }, actual;
      GimpFilterArgumentSpec spec = { .value_type = G_TYPE_OBJECT, .is_null = kind == 2,
        .n_references = 1, .references = &expected, .targets = &target };
      GError *error = NULL;
      GimpFilterArgumentsSnapshot *snapshot = gimp_filter_arguments_snapshot_import (1, &spec, &error);
      g_assert_no_error (error); g_assert_nonnull (snapshot);
      if (target_image) g_object_add_weak_pointer (G_OBJECT (target_image), &weak_target);
      g_assert_false (gimp_filter_arguments_snapshot_acquire_reference (snapshot, 1, 0, &actual, &lease));
      g_assert_null (lease);
      g_assert_true (gimp_filter_arguments_snapshot_acquire_reference (snapshot, 0, 0, &actual, &lease));
      g_assert_cmpuint (actual.object_type, ==, expected.object_type);
      g_assert_cmpint (actual.id, ==, expected.id);
      g_assert_cmpint (actual.was_set, ==, expected.was_set);
      g_assert_cmpint (actual.expired, ==, expected.expired);
      gimp_filter_arguments_snapshot_free (snapshot);
      if (target_image)
        {
          g_object_unref (target_image);
          g_assert_nonnull (weak_target); g_assert_true (weak_target == lease);
          g_object_unref (lease); g_assert_null (weak_target);
        }
      else g_assert_null (lease);
    }
}

static void
filter_schema_persistence_double_cache (void)
{
  GFile *file = persistence_file ();
  GimpImage *image = gimp_image_new (gimp, 16, 16, GIMP_RGB, GIMP_PRECISION_DOUBLE_LINEAR);
  const Babl *format = gimp_image_get_layer_format (image, TRUE);
  GimpLayer *source = gimp_layer_new (image, 16, 16, format, "double source", 1, GIMP_LAYER_MODE_NORMAL_LEGACY);
  GimpValueArray *args = isolated_args (3, 0);
  GimpFilterLayer *filter;
  gdouble source_pixels[16 * 16 * 4], before[16 * 16 * 4], after[16 * 16 * 4];
  g_assert_cmpint (babl_format_get_bytes_per_pixel (format), ==, 4 * sizeof (gdouble));
  for (guint i = 0; i < 16 * 16; i++)
    {
      source_pixels[4 * i] = (gdouble) i / 127.0 - .25;
      source_pixels[4 * i + 1] = (gdouble) (i % 17) / 31.0;
      source_pixels[4 * i + 2] = .50000000000000011;
      source_pixels[4 * i + 3] = 1.;
    }
  g_assert_true (gimp_image_add_layer (image, source, NULL, 0, FALSE));
  gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (source)), NULL, 0,
    format, source_pixels, GEGL_AUTO_ROWSTRIDE);
  filter = isolated_layer (image, 3, args); isolated_wait (filter, GIMP_FILTER_LAYER_CLEAN);
  for (guint pass = 0; pass < 2; pass++)
    {
      GtkWidget *dialog;
      const Babl *actual_format = gegl_buffer_get_format (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)));
      gchar *format_name = g_strdup (babl_get_name (actual_format));
      g_assert_cmpint (babl_format_get_bytes_per_pixel (actual_format), ==, 4 * sizeof (gdouble));
      /* Compare the application's completed buffer, not an idealized expected
       * calculation or the source pixels before the actual executor ran. */
      gegl_buffer_get (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)), NULL, 1,
        actual_format, before, GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);
      image = persistence_close_reopen (image, file); filter = persistence_filter (image);
      g_assert_cmpint (gimp_image_get_precision (image), ==, GIMP_PRECISION_DOUBLE_LINEAR);
      actual_format = gegl_buffer_get_format (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)));
      g_assert_cmpstr (babl_get_name (actual_format), ==, format_name); g_free (format_name);
      gegl_buffer_get (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)), NULL, 1,
        actual_format, after, GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);
      g_assert_cmpmem (before, sizeof before, after, sizeof after);
      persistence_args (filter, args); persistence_noop (image, filter, 3);
      g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 0);
      g_assert_cmpint (gimp_filter_layer_get_state (filter), ==, GIMP_FILTER_LAYER_CLEAN);
      if (pass == 0)
        {
          dialog = isolated_editor (image, filter, 3);
          isolated_text (dialog, "painter-convmatrix-6", "0.062500000000000014");
          isolated_text (dialog, "painter-convmatrix-offset", "0.125"); isolated_accept (dialog);
          {
            gdouble matrix[25];
            memcpy (matrix, gimp_value_get_double_array (gimp_value_array_index (args, 4), NULL), sizeof matrix);
            matrix[6] = .062500000000000014; gimp_value_set_double_array (gimp_value_array_index (args, 4), matrix, 25);
            g_value_set_double (gimp_value_array_index (args, 7), .125);
          }
          persistence_args (filter, args); isolated_wait (filter, GIMP_FILTER_LAYER_CLEAN);
          g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 1);
        }
    }
  gimp_value_array_unref (args); g_object_unref (image); persistence_cleanup_file (file);
}
