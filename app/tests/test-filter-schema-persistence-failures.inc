/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Phase D failure integration after a real GTK schema edit. The progress and
 * replacement-stream faults are local copies of the XCF harness seams; no
 * production hook or output factory is installed. Successful retry uses the
 * ordinary File Save/Open helpers from test-filter-schema-persistence.inc. */
#include "core/gimpprogress.h"
#include "xcf/xcf.h"

typedef struct
{
  GObject parent;
  guint phase, starts, values, texts;
  gboolean cancelled, ended;
} PersistenceFailureProgress;
typedef struct { GObjectClass parent; } PersistenceFailureProgressClass;

static GimpProgress *
persistence_failure_progress_start (GimpProgress *progress,
                                    gboolean cancellable,
                                    const gchar *text)
{
  PersistenceFailureProgress *self = (PersistenceFailureProgress *) progress;
  g_assert_true (cancellable);
  self->starts++;
  if (self->phase == 0 && !self->cancelled)
    {
      self->cancelled = TRUE;
      gimp_progress_cancel (progress);
    }
  return progress;
}

static void
persistence_failure_progress_value (GimpProgress *progress, gdouble value)
{
  PersistenceFailureProgress *self = (PersistenceFailureProgress *) progress;
  self->values++;
  if (self->phase == 1 && !self->cancelled)
    {
      /* The file entry point starts preparation and writing separately. */
      g_assert_cmpuint (self->starts, ==, 2);
      self->cancelled = TRUE;
      gimp_progress_cancel (progress);
    }
}

static void
persistence_failure_progress_text (GimpProgress *progress, const gchar *text)
{
  PersistenceFailureProgress *self = (PersistenceFailureProgress *) progress;
  self->texts++;
  if (self->phase == 2 && !self->cancelled)
    {
      /* xcf_save_stream_prepared emits this at its final close checkpoint. */
      g_assert_cmpuint (self->starts, ==, 2);
      self->cancelled = TRUE;
      gimp_progress_cancel (progress);
    }
}

static void
persistence_failure_progress_end (GimpProgress *progress)
{ ((PersistenceFailureProgress *) progress)->ended = TRUE; }

static gboolean
persistence_failure_progress_active (GimpProgress *progress)
{ return !((PersistenceFailureProgress *) progress)->ended; }

static void
persistence_failure_progress_iface (GimpProgressInterface *iface)
{
  iface->start = persistence_failure_progress_start;
  iface->set_value = persistence_failure_progress_value;
  iface->set_text = persistence_failure_progress_text;
  iface->end = persistence_failure_progress_end;
  iface->is_active = persistence_failure_progress_active;
}

GType persistence_failure_progress_get_type (void);
G_DEFINE_TYPE_WITH_CODE (PersistenceFailureProgress, persistence_failure_progress,
                        G_TYPE_OBJECT,
                        G_IMPLEMENT_INTERFACE (GIMP_TYPE_PROGRESS,
                                               persistence_failure_progress_iface))
static void
persistence_failure_progress_class_init (PersistenceFailureProgressClass *klass) {}
static void
persistence_failure_progress_init (PersistenceFailureProgress *self) {}

typedef struct
{
  GFilterOutputStream parent;
  gsize written, limit;
  gboolean fail_close, cancelled_close, final_close_attempted;
} PersistenceFailureOutput;
typedef struct { GFilterOutputStreamClass parent; } PersistenceFailureOutputClass;

static GOutputStream *
persistence_failure_output_base (gpointer output)
{ return g_filter_output_stream_get_base_stream (G_FILTER_OUTPUT_STREAM (output)); }

static gssize
persistence_failure_output_write (GOutputStream *output, const void *buffer,
                                  gsize count, GCancellable *cancel, GError **error)
{
  PersistenceFailureOutput *self = (PersistenceFailureOutput *) output;
  gssize written;
  if (self->written >= self->limit)
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NO_SPACE,
                           "Injected schema persistence write failure");
      return -1;
    }
  written = g_output_stream_write (persistence_failure_output_base (output),
    buffer, MIN (count, self->limit - self->written), cancel, error);
  if (written > 0) self->written += written;
  return written;
}

static goffset
persistence_failure_output_tell (GSeekable *stream)
{ return g_seekable_tell (G_SEEKABLE (persistence_failure_output_base (stream))); }
static gboolean
persistence_failure_output_can_seek (GSeekable *stream) { return TRUE; }
static gboolean
persistence_failure_output_seek (GSeekable *stream, goffset offset,
                                 GSeekType type, GCancellable *cancel,
                                 GError **error)
{
  return g_seekable_seek (G_SEEKABLE (persistence_failure_output_base (stream)),
                          offset, type, cancel, error);
}
static gboolean
persistence_failure_output_can_truncate (GSeekable *stream) { return FALSE; }
static gboolean
persistence_failure_output_truncate (GSeekable *stream, goffset offset,
                                     GCancellable *cancel, GError **error)
{ return FALSE; }

static void
persistence_failure_output_seek_iface (GSeekableIface *iface)
{
  iface->tell = persistence_failure_output_tell;
  iface->can_seek = persistence_failure_output_can_seek;
  iface->seek = persistence_failure_output_seek;
  iface->can_truncate = persistence_failure_output_can_truncate;
  iface->truncate_fn = persistence_failure_output_truncate;
}

GType persistence_failure_output_get_type (void);
G_DEFINE_TYPE_WITH_CODE (PersistenceFailureOutput, persistence_failure_output,
                        G_TYPE_FILTER_OUTPUT_STREAM,
                        G_IMPLEMENT_INTERFACE (G_TYPE_SEEKABLE,
                                               persistence_failure_output_seek_iface))

static gboolean
persistence_failure_output_close (GOutputStream *output, GCancellable *cancel,
                                  GError **error)
{
  PersistenceFailureOutput *self = (PersistenceFailureOutput *) output;
  self->cancelled_close = g_cancellable_is_cancelled (cancel);
  if (self->fail_close && !self->cancelled_close)
    {
      GCancellable *abandon = g_cancellable_new ();
      self->final_close_attempted = TRUE;
      g_cancellable_cancel (abandon);
      G_OUTPUT_STREAM_CLASS (persistence_failure_output_parent_class)->close_fn (
        output, abandon, NULL);
      g_object_unref (abandon);
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                           "Injected schema persistence final close failure");
      return FALSE;
    }
  return G_OUTPUT_STREAM_CLASS (persistence_failure_output_parent_class)->close_fn (
    output, cancel, error);
}

static void
persistence_failure_output_class_init (PersistenceFailureOutputClass *klass)
{
  G_OUTPUT_STREAM_CLASS (klass)->write_fn = persistence_failure_output_write;
  G_OUTPUT_STREAM_CLASS (klass)->close_fn = persistence_failure_output_close;
}
static void
persistence_failure_output_init (PersistenceFailureOutput *self) {}

static void
persistence_failure_sentinel (GFile *file, gboolean write)
{
  static const gchar expected[] = "KEEP ORIGINAL SCHEMA DESTINATION";
  GError *error = NULL;
  if (write)
    g_assert_true (g_file_replace_contents (file, expected, sizeof expected,
      NULL, FALSE, G_FILE_CREATE_NONE, NULL, NULL, &error));
  else
    {
      gchar *bytes;
      gsize size;
      g_assert_true (g_file_load_contents (file, NULL, &bytes, &size, NULL, &error));
      g_assert_cmpmem (bytes, size, expected, sizeof expected);
      g_free (bytes);
    }
  g_assert_no_error (error);
}

static void
persistence_failure_duplicate_key (GimpFilterLayer *filter)
{
  static const guint8 magic[12] = { 'G', 'P', 'X', 'C', 'F', 0, 0, 0, 1, 0, 0, 0 };
  GVariantBuilder builder;
  GVariant *value;
  GByteArray *wire = g_byte_array_new ();
  GimpParasite *parasite;
  g_assert_null (gimp_item_parasite_find (GIMP_ITEM (filter), "gimp-painter-item"));
  g_variant_builder_init (&builder, G_VARIANT_TYPE_VARDICT);
  g_variant_builder_add (&builder, "{sv}", "version", g_variant_new_uint32 (1));
  g_variant_builder_add (&builder, "{sv}", "version", g_variant_new_uint32 (2));
  value = g_variant_ref_sink (g_variant_builder_end (&builder));
  g_assert_true (g_variant_is_normal_form (value));
#if G_BYTE_ORDER == G_BIG_ENDIAN
  { GVariant *swapped = g_variant_byteswap (value); g_variant_unref (value); value = swapped; }
#endif
  g_byte_array_append (wire, magic, sizeof magic);
  g_byte_array_append (wire, g_variant_get_data (value), g_variant_get_size (value));
  g_variant_unref (value);
  parasite = gimp_parasite_new ("gimp-painter-item", GIMP_PARASITE_PERSISTENT,
                               wire->len, wire->data);
  gimp_item_parasite_attach (GIMP_ITEM (filter), parasite, FALSE);
  gimp_parasite_free (parasite); g_byte_array_unref (wire);
}

static void
filter_schema_persistence_failures (void)
{
  const guint live_before = xcf_painter_storage_live_files ();
  const guint8 source_pixel[] = { 31, 121, 217, 255 };
  const gsize tail_size = 1024 * 1024 + 17;
  GimpImage *image = new_image (), *copy;
  GimpLayer *source = new_source (image, NULL, "failure source");
  GimpValueArray *args = isolated_args (3, 0);
  GValue tail = G_VALUE_INIT;
  guint8 *tail_data = g_malloc (tail_size);
  GimpFilterLayer *filter;
  GtkWidget *dialog;
  GFile *file = persistence_file ();
  guint8 expected_pixels[16 * 16 * 4], actual_pixels[sizeof expected_pixels];
  GimpItem *items[2];
  guint32 saved_ids[2], tattoos[2];
  GimpFilterLayerSnapshot expected_state;

  for (gsize i = 0; i < tail_size; i++) tail_data[i] = (i * 17 + i / 257) & 255;
  g_value_init (&tail, G_TYPE_BYTES);
  g_value_take_boxed (&tail, g_bytes_new_take (tail_data, tail_size));
  gimp_value_array_append (args, &tail); g_value_unset (&tail);
  isolated_fill (source, source_pixel);
  filter = isolated_layer (image, 3, args); settle (filter);
  dialog = isolated_editor (image, filter, 3);
  isolated_text (dialog, "painter-convmatrix-offset", "0.125");
  isolated_accept (dialog);
  g_value_set_double (gimp_value_array_index (args, 7), 0.125);
  settle (filter); persistence_args (filter, args);
  g_assert_cmpint (gimp_undo_stack_get_depth (gimp_image_get_undo_stack (image)), ==, 1);
  g_assert_true (gimp_filter_layer_get_snapshot_state (filter, &expected_state));
  g_assert_true (expected_state.cache_complete);
  g_assert_cmpuint (expected_state.generation, ==, expected_state.cache_generation);
  gegl_buffer_get (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)), NULL, 1,
    babl_format ("R'G'B'A u8"), expected_pixels, GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);
  items[0] = GIMP_ITEM (filter); items[1] = GIMP_ITEM (source);
  for (guint i = 0; i < G_N_ELEMENTS (items); i++)
    {
      saved_ids[i] = gimp_painter_provenance_get_save_id (G_OBJECT (items[i]));
      tattoos[i] = gimp_item_get_tattoo (items[i]);
      g_assert_cmpuint (saved_ids[i], ==, 0);
    }
  persistence_failure_sentinel (file, TRUE);

  for (guint scene = 0; scene < 6; scene++)
    {
      GError *error = NULL;
      SchemaCheckpoint checkpoint;
      GimpFilterLayerSnapshot state;
      const guint snapshots = xcf_painter_storage_live_files ();
      gint dirty;
      g_test_message ("Schema persistence failure scene=%u (preflight, preparation/write/close cancellation, write/close failure)", scene);
      if (scene == 0) persistence_failure_duplicate_key (filter);
      checkpoint = schema_checkpoint (image, filter);
      dirty = gimp_image_is_dirty (image);
      if (scene == 0)
        {
          g_assert_false (xcf_save_file_with_painter_transport (
            gimp, image, file, NULL, TRUE, NULL, &error));
          g_assert_nonnull (error);
          g_assert_nonnull (strstr (error->message, "Duplicate Painter metadata key"));
        }
      else if (scene <= 3)
        {
          PersistenceFailureProgress *progress = g_object_new (
            persistence_failure_progress_get_type (), NULL);
          progress->phase = scene - 1;
          g_assert_false (xcf_save_file_with_painter_transport (
            gimp, image, file, GIMP_PROGRESS (progress), TRUE, NULL, &error));
          g_assert_error (error, G_IO_ERROR, G_IO_ERROR_CANCELLED);
          g_assert_true (progress->cancelled); g_assert_true (progress->ended);
          g_assert_cmpuint (progress->starts, ==, scene == 1 ? 1 : 2);
          if (scene == 2) g_assert_cmpuint (progress->values, >, 0);
          if (scene == 3) g_assert_cmpuint (progress->texts, ==, 1);
          g_object_unref (progress);
        }
      else
        {
          GOutputStream *base = G_OUTPUT_STREAM (g_file_replace (
            file, NULL, FALSE, G_FILE_CREATE_NONE, NULL, &error));
          PersistenceFailureOutput *output;
          g_assert_no_error (error); g_assert_nonnull (base);
          output = g_object_new (persistence_failure_output_get_type (),
            "base-stream", base, "close-base-stream", TRUE, NULL);
          g_object_unref (base);
          output->limit = scene == 4 ? 65536 + 127 : G_MAXSIZE;
          output->fail_close = scene == 5;
          g_assert_false (xcf_save_stream_with_painter_transport (
            gimp, image, G_OUTPUT_STREAM (output), file, NULL, TRUE, NULL, &error));
          g_assert_error (error, G_IO_ERROR, scene == 4 ? G_IO_ERROR_NO_SPACE : G_IO_ERROR_FAILED);
          g_assert_nonnull (strstr (error->message, scene == 4 ?
            "Injected schema persistence write failure" : "Injected schema persistence final close failure"));
          g_assert_true (g_output_stream_is_closed (G_OUTPUT_STREAM (output)));
          g_assert_cmpuint (output->written, >=, 65536 + 127);
          if (scene == 4) g_assert_true (output->cancelled_close);
          else g_assert_true (output->final_close_attempted);
          g_object_unref (output);
        }
      g_clear_error (&error);
      persistence_failure_sentinel (file, FALSE);
      schema_assert_checkpoint (image, filter, &checkpoint);
      persistence_args (filter, args);
      g_assert_true (gimp_filter_layer_get_snapshot_state (filter, &state));
      g_assert_cmpint (state.state, ==, expected_state.state);
      g_assert_cmpint (state.cache_complete, ==, expected_state.cache_complete);
      g_assert_cmpint (gimp_image_is_dirty (image), ==, dirty);
      for (guint i = 0; i < G_N_ELEMENTS (items); i++)
        {
          g_assert_cmpuint (gimp_painter_provenance_get_save_id (G_OBJECT (items[i])), ==, saved_ids[i]);
          g_assert_cmpuint (gimp_item_get_tattoo (items[i]), ==, tattoos[i]);
        }
      gegl_buffer_get (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)), NULL, 1,
        babl_format ("R'G'B'A u8"), actual_pixels, GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);
      g_assert_cmpmem (actual_pixels, sizeof actual_pixels, expected_pixels, sizeof expected_pixels);
      g_assert_cmpuint (xcf_painter_storage_live_files (), ==, snapshots);
      if (scene == 0) gimp_item_parasite_detach (GIMP_ITEM (filter), "gimp-painter-item", FALSE);
    }

  /* Removing the test faults restores ordinary Save/Open without repairing or
   * replacing the edited model. Its >1 MiB tail uses the real multipart path. */
  persistence_save (image, file); copy = persistence_load (file);
  filter = persistence_filter (copy); persistence_args (filter, args);
  g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 0);
  g_assert_cmpint (gimp_filter_layer_get_state (filter), ==, GIMP_FILTER_LAYER_CLEAN);
  {
    GimpPainterProvenanceTransport *transport = gimp_painter_provenance_ref_transport (G_OBJECT (filter));
    g_assert_nonnull (transport);
    g_assert_true (transport->present[GIMP_PAINTER_PROVENANCE_NAMESPACE_ITEM]);
    g_assert_false (transport->invalid[GIMP_PAINTER_PROVENANCE_NAMESPACE_ITEM]);
    g_assert_nonnull (transport->capsules[GIMP_PAINTER_PROVENANCE_NAMESPACE_ITEM]);
    g_assert_cmpuint (g_bytes_get_size (transport->capsules[GIMP_PAINTER_PROVENANCE_NAMESPACE_ITEM]), >, 1024 * 1024);
    gimp_painter_provenance_free_transport (transport);
  }
  gegl_buffer_get (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)), NULL, 1,
    babl_format ("R'G'B'A u8"), actual_pixels, GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);
  g_assert_cmpmem (actual_pixels, sizeof actual_pixels, expected_pixels, sizeof expected_pixels);
  g_object_unref (copy); g_object_unref (image); gimp_value_array_unref (args);
  persistence_cleanup_file (file);
  g_assert_cmpuint (xcf_painter_storage_live_files (), ==, live_before);
}
