/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Synthetic XCF schema and native lifecycle tests. The capsule below follows
 * the pinned old xcf-save.c construction and value-invert.c argument declarations.
 * Value Invert has only its three retained context slots and no mode.
 * Neither these manufactured files nor their analytic pixels are genuine-old
 * writer/runtime evidence. A separately captured corpus remains a parity gate.
 */
#include "core/gimpchannel-select.h"
#include "painter/gimp-painter-binding.h"

static GimpFilterLayer *
xcf_value_invert_filter (GimpImage *image)
{
  GimpLayer *layer = find_layer (image, "value-invert filter");
  g_assert_nonnull (layer); g_assert_true (GIMP_IS_FILTER_LAYER (layer));
  return GIMP_FILTER_LAYER (layer);
}

static void
xcf_value_invert_raw_equal (GimpFilterLayer *filter, GBytes *expected)
{
  GBytes *actual = gimp_filter_layer_ref_definition (filter);
  g_assert_nonnull (actual); g_assert_true (g_bytes_equal (actual, expected));
  g_bytes_unref (actual);
}

static void
xcf_value_invert_parameters (GimpFilterLayer *filter, gint run_mode)
{
  GimpValueArray *args = gimp_filter_layer_dup_args (filter);
  gchar *name = gimp_filter_layer_dup_procedure (filter);
  g_assert_cmpstr (name, ==, "plug-in-vinvert"); g_free (name);
  g_assert_nonnull (args);
  g_assert_cmpuint (gimp_value_array_length (args), ==, 3);
  for (guint i = 0; i < 3; ++i)
    g_assert_true (G_VALUE_HOLDS_INT (gimp_value_array_index (args, i)));
  g_assert_cmpint (g_value_get_int (gimp_value_array_index (args, 0)), ==, run_mode);
  g_assert_cmpint (g_value_get_int (gimp_value_array_index (args, 1)), ==, 0);
  g_assert_cmpint (g_value_get_int (gimp_value_array_index (args, 2)), ==, 0);
  gimp_value_array_unref (args);
}

static void
xcf_value_invert_pixels (GimpLayer *layer, GBytes *expected, guint channels)
{
  const Babl *format = babl_format (channels == 2 ? "Y'A u8" : "R'G'B'A u8");
  gsize size = (gsize) gimp_item_get_width (GIMP_ITEM (layer)) *
                     gimp_item_get_height (GIMP_ITEM (layer)) * channels;
  guint8 *actual = g_malloc (size);
  g_assert_cmpuint (size, ==, g_bytes_get_size (expected));
  gegl_buffer_get (gimp_drawable_get_buffer (GIMP_DRAWABLE (layer)), NULL, 1,
                   format, actual, GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);
  g_assert_cmpmem (actual, size, g_bytes_get_data (expected, NULL), size);
  g_free (actual);
}

static void
xcf_value_invert_settle (GimpFilterLayer *filter, GimpFilterLayerState expected)
{
  gint64 deadline = g_get_monotonic_time () + 90 * G_TIME_SPAN_SECOND;
  while (gimp_filter_layer_get_state (filter) != expected &&
         g_get_monotonic_time () < deadline)
    {
      if (gimp_filter_layer_get_state (filter) == GIMP_FILTER_LAYER_FAILED &&
          expected != GIMP_FILTER_LAYER_FAILED)
        { gchar *error = gimp_filter_layer_dup_error (filter); g_error ("Value Invert XCF rerun failed: %s", error); }
      g_main_context_iteration (NULL, FALSE); g_usleep (100);
    }
  g_assert_cmpint (gimp_filter_layer_get_state (filter), ==, expected);
}

static void
xcf_value_invert_loaded (GimpFilterLayer *filter)
{
  GimpFilterLayerSnapshot snapshot;
  g_assert_true (gimp_filter_layer_get_snapshot_state (filter, &snapshot));
  g_assert_true (snapshot.cache_complete);
  g_assert_cmpuint (snapshot.generation, ==, snapshot.cache_generation);
  /* Give an accidentally queued startup request an opportunity to appear. */
  for (guint i = 0; i < 8; ++i) g_main_context_iteration (NULL, FALSE);
  g_assert_cmpint (gimp_filter_layer_get_state (filter), ==, GIMP_FILTER_LAYER_CLEAN);
  g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 0);
}

static void
xcf_value_invert_lower_input (GimpImage *image, guint channels, GBytes *bytes)
{
  GimpLayer *source = find_layer (image, "value-invert source");
  const gchar *formats[] = {"Y' u8", "Y'A u8", "R'G'B' u8", "R'G'B'A u8"};
  gint width, height;
  g_assert_nonnull (source);
  width = gimp_item_get_width (GIMP_ITEM (source));
  height = gimp_item_get_height (GIMP_ITEM (source));
  g_assert_cmpuint (g_bytes_get_size (bytes), ==, (gsize) width * height * channels);
  gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (source)), NULL, 0,
                   babl_format (formats[channels - 1]), g_bytes_get_data (bytes, NULL), GEGL_AUTO_ROWSTRIDE);
  gimp_drawable_update (GIMP_DRAWABLE (source), 0, 0, width, height);
}

static GFile *
xcf_value_invert_save (GimpImage *image)
{
  GError *error = NULL;
  GFileIOStream *io;
  GFile *file = g_file_new_tmp ("painter-value-invert-roundtrip-XXXXXX", &io, &error);
  GimpPlugInProcedure *procedure = GIMP_PLUG_IN_PROCEDURE (gimp_pdb_lookup_procedure (gimp->pdb, "gimp-xcf-save"));
  g_assert_no_error (error); g_assert_nonnull (file); g_assert_nonnull (procedure);
  g_assert_true (g_io_stream_close (G_IO_STREAM (io), NULL, &error));
  g_assert_no_error (error); g_object_unref (io);
  g_assert_cmpint (file_save (gimp, image, NULL, file, procedure, GIMP_RUN_NONINTERACTIVE,
                              FALSE, FALSE, FALSE, &error), ==, GIMP_PDB_SUCCESS);
  g_assert_no_error (error); return file;
}

static GimpImage *
xcf_value_invert_close_reopen (GimpImage *image)
{
  GFile *file = xcf_value_invert_save (image);
  GError *error = NULL;
  g_object_unref (image);
  image = open_file (file, NULL, NULL, &error);
  g_assert_no_error (error); g_assert_nonnull (image);
  g_assert_true (g_file_delete (file, NULL, &error)); g_assert_no_error (error);
  g_object_unref (file); return image;
}

static GBytes *
xcf_value_invert_constant_pixels (guint width, guint height, guint channels, const guint8 *pixel)
{
  gsize size = (gsize) width * height * channels;
  guint8 *bytes = g_malloc (size);
  for (gsize i = 0; i < size; i += channels) memcpy (bytes + i, pixel, channels);
  return g_bytes_new_take (bytes, size);
}

static void
xcf_value_invert_serialized_snapshot (GimpFilterLayer *filter, const GimpFilterLayerSnapshot *saved)
{
  GVariant *definition = gimp_painter_provenance_ref_definition (G_OBJECT (filter));
  guint64 generation, cache_generation;
  gboolean cache_complete;
  guint32 state;
  g_assert_nonnull (definition);
  g_assert_true (g_variant_lookup (definition, "generation", "t", &generation));
  g_assert_true (g_variant_lookup (definition, "cache-generation", "t", &cache_generation));
  g_assert_true (g_variant_lookup (definition, "cache-complete", "b", &cache_complete));
  g_assert_true (g_variant_lookup (definition, "saved-state", "u", &state));
  g_assert_cmpuint (generation, ==, saved->generation);
  g_assert_cmpuint (cache_generation, ==, saved->cache_generation);
  g_assert_cmpint (cache_complete, ==, saved->cache_complete);
  g_assert_cmpuint (state, ==, saved->state);
  g_variant_unref (definition);
}

/* The old writer casts each signed scalar to guint32 and writes big endian.
 * Image and drawable values are deliberately omitted, not saved object IDs. */
static GBytes *
xcf_value_invert_capsule (gint run_mode)
{
  static const gchar name[] = "plug-in-vinvert";
  guint8 data[56] = {0};
  put32 (data, 0, sizeof name); memcpy (data + 4, name, sizeof name);
  put32 (data, 20, 2); put32 (data, 24, 4); put32 (data, 28, (guint32) run_mode);
  put32 (data, 32, 1); put32 (data, 40, 10);
  return g_bytes_new (data, sizeof data);
}

static void
xcf_value_invert_append_word (GByteArray *bytes, guint32 value)
{
  value = GUINT32_TO_BE (value);
  g_byte_array_append (bytes, (const guint8 *) &value, sizeof value);
}

/* Only small single-tile uncompressed RGBA fixtures use this test builder. */
static guint
xcf_value_invert_append_layer (GByteArray *bytes, const gchar *name, GBytes *raw,
                          GBytes *pixels, guint *capsule_offset)
{
  const guint start = bytes->len;
  guint hierarchy_pointer, level_pointer, tile_pointer;
  g_assert_cmpuint (g_bytes_get_size (pixels), ==, 17 * 13 * 4);
  xcf_value_invert_append_word (bytes, 17); xcf_value_invert_append_word (bytes, 13);
  xcf_value_invert_append_word (bytes, GIMP_RGBA_IMAGE);
  xcf_value_invert_append_word (bytes, strlen (name) + 1);
  g_byte_array_append (bytes, (const guint8 *) name, strlen (name) + 1);
  if (raw)
    {
      xcf_value_invert_append_word (bytes, 32);
      xcf_value_invert_append_word (bytes, g_bytes_get_size (raw));
      *capsule_offset = bytes->len;
      g_byte_array_append (bytes, g_bytes_get_data (raw, NULL), g_bytes_get_size (raw));
    }
  xcf_value_invert_append_word (bytes, 0); xcf_value_invert_append_word (bytes, 0);
  hierarchy_pointer = bytes->len;
  xcf_value_invert_append_word (bytes, 0); xcf_value_invert_append_word (bytes, 0);
  put32 (bytes->data, hierarchy_pointer, bytes->len);
  xcf_value_invert_append_word (bytes, 17); xcf_value_invert_append_word (bytes, 13);
  xcf_value_invert_append_word (bytes, 4);
  level_pointer = bytes->len;
  xcf_value_invert_append_word (bytes, 0); xcf_value_invert_append_word (bytes, 0);
  put32 (bytes->data, level_pointer, bytes->len);
  xcf_value_invert_append_word (bytes, 17); xcf_value_invert_append_word (bytes, 13);
  tile_pointer = bytes->len;
  xcf_value_invert_append_word (bytes, 0); xcf_value_invert_append_word (bytes, 0);
  put32 (bytes->data, tile_pointer, bytes->len);
  g_byte_array_append (bytes, g_bytes_get_data (pixels, NULL), g_bytes_get_size (pixels));
  return start;
}

static GBytes *
xcf_value_invert_synthetic_file (GBytes *raw, GBytes *cache, GBytes *source, guint *capsule_offset)
{
  GByteArray *bytes = g_byte_array_sized_new (2200);
  guint8 header[50] = {0};
  guint filter_offset, source_offset;
  memcpy (header, "gimp xcf v003", 13);
  put32 (header, 14, 17); put32 (header, 18, 13);
  g_byte_array_append (bytes, header, sizeof header);
  filter_offset = xcf_value_invert_append_layer (bytes, "value-invert filter", raw, cache, capsule_offset);
  source_offset = xcf_value_invert_append_layer (bytes, "value-invert source", NULL, source, NULL);
  put32 (bytes->data, 34, filter_offset); put32 (bytes->data, 38, source_offset);
  return g_byte_array_free_to_bytes (bytes);
}

static GimpImage *
xcf_value_invert_open_synthetic (GBytes *raw, GBytes *cache, GBytes *source)
{
  guint offset;
  GBytes *bytes = xcf_value_invert_synthetic_file (raw, cache, source, &offset);
  GError *error = NULL;
  GimpImage *image = open_bytes (g_bytes_get_data (bytes, NULL), g_bytes_get_size (bytes),
                                XCF_PAINTER_DIALECT_LEGACY, &error);
  g_assert_no_error (error); g_assert_nonnull (image); g_bytes_unref (bytes);
  return image;
}

static GBytes *
xcf_value_invert_source (guint phase, gboolean alpha)
{
  static const guint8 alphas[] = {0, 1, 63, 127, 128, 254, 255};
  static const guint8 edge[][3] = {
    {0, 0, 0}, {255, 255, 255}, {1, 0, 0}, {0, 1, 0}, {0, 0, 1},
    {127, 128, 63}, {254, 127, 1}, {103, 71, 199}, {17, 17, 17}
  };
  guint8 data[17 * 13 * 4];
  for (guint i = 0; i < 17 * 13; ++i)
    {
      data[i * 4] = (i * 71 + phase * 37) & 255;
      data[i * 4 + 1] = i % 5 == 0 ? data[i * 4] : (i * 13 + phase * 89) & 255;
      data[i * 4 + 2] = i % 7 == 0 ? data[i * 4 + 1] : (i * 43 + phase * 19) & 255;
      if (i < G_N_ELEMENTS (edge)) memcpy (data + i * 4, edge[i], 3);
      data[i * 4 + 3] = alpha ? alphas[i % G_N_ELEMENTS (alphas)] : 255;
    }
  return g_bytes_new (data, sizeof data);
}

/* Independent scalar oracle for full-coverage replacement. The raw callback
 * also transforms hidden RGB; replacement retains input RGB if final alpha is
 * zero. Nothing here calls the production kernel or owner merge helper. */
static GBytes *
xcf_value_invert_result (GBytes *source)
{
  gsize size;
  const guint8 *input = g_bytes_get_data (source, &size);
  guint8 *output = g_memdup2 (input, size);
  g_assert_cmpuint (size % 4, ==, 0);
  for (gsize i = 0; i < size; i += 4)
    {
      const guint maximum = MAX (input[i], MAX (input[i + 1], input[i + 2]));
      if (input[i + 3])
        for (guint c = 0; c < 3; ++c)
          output[i + c] = maximum ? ((255 - maximum) * input[i + c] + maximum / 2) / maximum : 255;
    }
  return g_bytes_new_take (output, size);
}

/* For translucent sources, inspect the immediate lower graph's input bytes.
 * Projection is a distinct operation from the independent transform oracle;
 * it may normalize hidden RGB before the native owner captures that input. */
static GBytes *
xcf_value_invert_projected_input (GimpFilterLayer *filter)
{
  const GeglRectangle rectangle = {0, 0, 17, 13};
  GeglNode *below = gegl_node_get_input_proxy (gimp_filter_get_node (GIMP_FILTER (filter)), "input");
  guint8 data[17 * 13 * 4];
  gegl_node_blit (below, 1, &rectangle, babl_format ("R'G'B'A u8"),
                  data, GEGL_AUTO_ROWSTRIDE, GEGL_BLIT_CACHE);
  return g_bytes_new (data, sizeof data);
}

static void
value_invert_synthetic_capsule_roundtrip (void)
{
  static const gint run_modes[] = {G_MININT32, -1, 0, 1, 2, G_MAXINT32};
  const guint8 sentinel[] = {13, 29, 47, 255};
  GBytes *cache = xcf_value_invert_constant_pixels (17, 13, 4, sentinel);
  for (guint alpha = 0; alpha < 2; ++alpha)
    for (guint i = 0; i < G_N_ELEMENTS (run_modes); ++i)
      {
        const gint run_mode = run_modes[i];
        GBytes *source = xcf_value_invert_source (0, alpha);
        GBytes *source2 = xcf_value_invert_source (1, alpha);
        GBytes *raw = xcf_value_invert_capsule (run_mode), *output;
        GimpImage *image = xcf_value_invert_open_synthetic (raw, cache, source);
        GimpFilterLayer *filter = xcf_value_invert_filter (image);
        GimpItem *copy;
        GError *error = NULL;
        g_test_message ("Synthetic Value Invert capsule run-mode=%d alpha=%u", run_mode, alpha);
        xcf_value_invert_loaded (filter); xcf_value_invert_parameters (filter, run_mode);
        xcf_value_invert_raw_equal (filter, raw); xcf_value_invert_pixels (GIMP_LAYER (filter), cache, 4);
        copy = gimp_item_duplicate (GIMP_ITEM (filter), GIMP_TYPE_FILTER_LAYER);
        g_assert_nonnull (copy); g_object_ref_sink (copy);
        xcf_value_invert_loaded (GIMP_FILTER_LAYER (copy));
        xcf_value_invert_parameters (GIMP_FILTER_LAYER (copy), run_mode);
        xcf_value_invert_raw_equal (GIMP_FILTER_LAYER (copy), raw);
        xcf_value_invert_pixels (GIMP_LAYER (copy), cache, 4); g_object_unref (copy);
        image = xcf_value_invert_close_reopen (image); filter = xcf_value_invert_filter (image);
        xcf_value_invert_loaded (filter); xcf_value_invert_parameters (filter, run_mode);
        xcf_value_invert_raw_equal (filter, raw); xcf_value_invert_pixels (GIMP_LAYER (filter), cache, 4);
        xcf_value_invert_lower_input (image, 4, source2);
        if (alpha)
          {
            GBytes *projected = xcf_value_invert_projected_input (filter);
            output = xcf_value_invert_result (projected); g_bytes_unref (projected);
          }
        else output = xcf_value_invert_result (source2);
        xcf_value_invert_settle (filter, GIMP_FILTER_LAYER_CLEAN);
        g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 1);
        xcf_value_invert_pixels (GIMP_LAYER (filter), output, 4); xcf_value_invert_raw_equal (filter, raw);
        /* Equal-byte lower notifications still create a new owner generation. */
        xcf_value_invert_lower_input (image, 4, source2);
        xcf_value_invert_settle (filter, GIMP_FILTER_LAYER_CLEAN);
        g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 2);
        xcf_value_invert_pixels (GIMP_LAYER (filter), output, 4);
        image = xcf_value_invert_close_reopen (image); filter = xcf_value_invert_filter (image);
        xcf_value_invert_loaded (filter); xcf_value_invert_parameters (filter, run_mode);
        xcf_value_invert_raw_equal (filter, raw); xcf_value_invert_pixels (GIMP_LAYER (filter), output, 4);
        {
          const gint edited_run_mode = run_mode == G_MAXINT32 ? G_MININT32 : G_MAXINT32;
          GimpValueArray *edited = gimp_filter_layer_dup_args (filter);
          GBytes *changed = xcf_value_invert_capsule (edited_run_mode);
          /* Context run mode is retained exactly but does not change pixels. */
          g_value_set_int (gimp_value_array_index (edited, 0), edited_run_mode);
          g_assert_true (gimp_filter_layer_edit_definition (filter, "plug-in-vinvert", changed, edited, &error));
          g_assert_no_error (error); gimp_value_array_unref (edited);
          xcf_value_invert_parameters (filter, edited_run_mode); xcf_value_invert_raw_equal (filter, changed);
          g_assert_true (gimp_image_undo (image));
          xcf_value_invert_parameters (filter, run_mode); xcf_value_invert_raw_equal (filter, raw);
          xcf_value_invert_settle (filter, GIMP_FILTER_LAYER_CLEAN);
          xcf_value_invert_pixels (GIMP_LAYER (filter), output, 4);
          g_assert_true (gimp_image_redo (image));
          xcf_value_invert_parameters (filter, edited_run_mode); xcf_value_invert_raw_equal (filter, changed);
          xcf_value_invert_settle (filter, GIMP_FILTER_LAYER_CLEAN);
          xcf_value_invert_pixels (GIMP_LAYER (filter), output, 4);
          image = xcf_value_invert_close_reopen (image); filter = xcf_value_invert_filter (image);
          xcf_value_invert_loaded (filter); xcf_value_invert_parameters (filter, edited_run_mode);
          xcf_value_invert_raw_equal (filter, changed); xcf_value_invert_pixels (GIMP_LAYER (filter), output, 4);
          g_bytes_unref (changed);
        }
        g_object_unref (image); g_bytes_unref (raw); g_bytes_unref (output);
        g_bytes_unref (source); g_bytes_unref (source2);
      }
  g_bytes_unref (cache);
}

static void
value_invert_malformed_capsules (void)
{
  const guint8 sentinel[] = {13, 29, 47, 255};
  GBytes *capsule = xcf_value_invert_capsule (1);
  GBytes *cache = xcf_value_invert_constant_pixels (17, 13, 4, sentinel);
  GBytes *source = xcf_value_invert_source (0, FALSE), *source2 = xcf_value_invert_source (1, FALSE);
  for (guint mutation = 0; mutation < 18; ++mutation)
    {
      guint8 data[68] = {0};
      gsize size = 56, retained_size = 56, file_size;
      guint offset;
      GBytes *raw, *file, *expected_raw;
      guint8 *bytes;
      GimpImage *image;
      GimpFilterLayer *filter;
      GError *error = NULL;
      memcpy (data, g_bytes_get_data (capsule, NULL), 56);
      g_test_message ("Synthetic Value Invert capsule mutation %u", mutation);
      switch (mutation)
        {
        case 0: put32 (data, 20, 5); break; /* float run mode */
        case 1: put32 (data, 32, 10); break; /* wrong image kind */
        case 2: put32 (data, 40, 1); break; /* wrong drawable kind */
        case 3: put32 (data, 20, 3); break; /* int16 writer bits are not int32 */
        case 4: put32 (data, 24, 3); break; /* run-mode length */
        case 5: put32 (data, 36, 4); break; /* omitted image payload */
        case 6: put32 (data, 44, 4); break; /* omitted drawable payload */
        case 7: put32 (data, 52, 1); break; /* END payload */
        case 8: data[19] = 'x'; break; /* missing terminal NUL */
        case 9: data[12] = 0; break; /* embedded name NUL */
        case 10: break; /* short outer length, changed below */
        case 11: break; /* oversized outer length, changed below */
        case 12:
          /* A fourth scalar is invalid even when its value would be ignored. */
          put32 (data, 48, 2); put32 (data, 52, 4); put32 (data, 56, 1);
          size = retained_size = 68; break;
        case 13:
          put32 (data, 40, 0); put32 (data, 44, 0);
          size = retained_size = 48; break; /* missing drawable slot */
        case 14: data[4] = 'P'; break; /* unsupported literal */
        case 15:
          /* Declared tail becomes a valid VISIBLE property to the old reader;
           * the retained capsule stops at END, with no admitted tail bytes. */
          put32 (data, 56, 8); put32 (data, 60, 4); put32 (data, 64, 1);
          size = 68; break;
        case 16: put32 (data, 32, 6); break; /* omitted array is not an image */
        case 17:
          put32 (data, 32, 10); put32 (data, 40, 0); put32 (data, 44, 0);
          size = retained_size = 48; break; /* missing image slot */
        }
      raw = g_bytes_new (data, size);
      file = xcf_value_invert_synthetic_file (raw, cache, source, &offset);
      bytes = g_memdup2 (g_bytes_get_data (file, &file_size), g_bytes_get_size (file));
      if (mutation == 10) put32 (bytes, offset - 4, 55);
      if (mutation == 11) put32 (bytes, offset - 4, 68);
      image = open_bytes (bytes, file_size, XCF_PAINTER_DIALECT_LEGACY, &error);
      g_assert_no_error (error); g_assert_nonnull (image);
      filter = xcf_value_invert_filter (image);
      g_assert_null (gimp_filter_layer_dup_args (filter)); xcf_value_invert_loaded (filter);
      expected_raw = g_bytes_new (bytes + offset, retained_size);
      xcf_value_invert_raw_equal (filter, expected_raw); xcf_value_invert_pixels (GIMP_LAYER (filter), cache, 4);
      image = xcf_value_invert_close_reopen (image); filter = xcf_value_invert_filter (image);
      g_assert_null (gimp_filter_layer_dup_args (filter)); xcf_value_invert_loaded (filter);
      xcf_value_invert_raw_equal (filter, expected_raw); xcf_value_invert_pixels (GIMP_LAYER (filter), cache, 4);
      xcf_value_invert_lower_input (image, 4, source2);
      xcf_value_invert_settle (filter, GIMP_FILTER_LAYER_FAILED);
      g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 0);
      xcf_value_invert_raw_equal (filter, expected_raw); xcf_value_invert_pixels (GIMP_LAYER (filter), cache, 4);
      image = xcf_value_invert_close_reopen (image); filter = xcf_value_invert_filter (image);
      g_assert_null (gimp_filter_layer_dup_args (filter));
      xcf_value_invert_settle (filter, GIMP_FILTER_LAYER_FAILED);
      g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 0);
      xcf_value_invert_raw_equal (filter, expected_raw); xcf_value_invert_pixels (GIMP_LAYER (filter), cache, 4);
      g_object_unref (image); g_bytes_unref (expected_raw); g_bytes_unref (raw);
      g_bytes_unref (file); g_free (bytes);
    }
  g_bytes_unref (capsule); g_bytes_unref (cache); g_bytes_unref (source); g_bytes_unref (source2);
}

static void
xcf_value_invert_types_equal (GimpFilterLayer *filter, GimpValueArray *expected)
{
  GimpValueArray *actual = gimp_filter_layer_dup_args (filter);
  g_assert_nonnull (actual);
  g_assert_cmpuint (gimp_value_array_length (actual), ==, gimp_value_array_length (expected));
  for (guint i = 0; i < gimp_value_array_length (actual); ++i)
    {
      const GValue *a = gimp_value_array_index (actual, i), *e = gimp_value_array_index (expected, i);
      g_assert_true (G_VALUE_TYPE (a) == G_VALUE_TYPE (e));
      if (G_VALUE_HOLDS_INT (e)) g_assert_cmpint (g_value_get_int (a), ==, g_value_get_int (e));
      else { g_assert_true (G_VALUE_HOLDS_DOUBLE (e)); g_assert_cmpfloat (g_value_get_double (a), ==, g_value_get_double (e)); }
    }
  gimp_value_array_unref (actual);
}

/* A native typed model that fails strict admission must survive persistence.
 * Its syntactically valid raw capsule must not silently repair the typed slots. */
static void
value_invert_rejected_types_roundtrip (void)
{
  const guint8 sentinel[] = {13, 29, 47, 255};
  GBytes *raw = xcf_value_invert_capsule (1);
  GBytes *cache = xcf_value_invert_constant_pixels (17, 13, 4, sentinel);
  GBytes *source = xcf_value_invert_source (0, FALSE);
  for (guint mutation = 0; mutation < 5; ++mutation)
    {
      GimpImage *image = xcf_value_invert_open_synthetic (raw, cache, source);
      GimpFilterLayer *filter = xcf_value_invert_filter (image);
      GimpValueArray *args = gimp_value_array_new_from_types (NULL,
        G_TYPE_INT, 1, G_TYPE_INT, -123, G_TYPE_INT, 456, G_TYPE_NONE);
      GimpItem *copy;
      GError *error = NULL;
      if (mutation < 3)
        {
          GValue *slot = gimp_value_array_index (args, mutation);
          g_value_unset (slot); g_value_init (slot, G_TYPE_DOUBLE);
          g_value_set_double (slot, mutation + 0.5);
        }
      else if (mutation == 3) gimp_value_array_truncate (args, 2);
      else
        {
          GValue extra = G_VALUE_INIT;
          g_value_init (&extra, G_TYPE_INT); g_value_set_int (&extra, -7);
          gimp_value_array_append (args, &extra); g_value_unset (&extra);
        }
      g_assert_true (gimp_filter_layer_set_definition (filter, "plug-in-vinvert", raw, args, &error));
      g_assert_no_error (error); xcf_value_invert_settle (filter, GIMP_FILTER_LAYER_FAILED);
      g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 0);
      xcf_value_invert_types_equal (filter, args); xcf_value_invert_raw_equal (filter, raw);
      xcf_value_invert_pixels (GIMP_LAYER (filter), cache, 4);
      copy = gimp_item_duplicate (GIMP_ITEM (filter), GIMP_TYPE_FILTER_LAYER);
      g_assert_nonnull (copy); g_object_ref_sink (copy);
      xcf_value_invert_types_equal (GIMP_FILTER_LAYER (copy), args);
      xcf_value_invert_raw_equal (GIMP_FILTER_LAYER (copy), raw);
      xcf_value_invert_pixels (GIMP_LAYER (copy), cache, 4); g_object_unref (copy);
      for (guint epoch = 0; epoch < 2; ++epoch)
        {
          image = xcf_value_invert_close_reopen (image); filter = xcf_value_invert_filter (image);
          xcf_value_invert_settle (filter, GIMP_FILTER_LAYER_FAILED);
          g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 0);
          xcf_value_invert_types_equal (filter, args); xcf_value_invert_raw_equal (filter, raw);
          xcf_value_invert_pixels (GIMP_LAYER (filter), cache, 4);
        }
      gimp_value_array_unref (args); g_object_unref (image);
    }
  g_bytes_unref (raw); g_bytes_unref (cache); g_bytes_unref (source);
}

static void
xcf_value_invert_buffer_equal (GeglBuffer *buffer, GBytes *expected)
{
  const gsize size = g_bytes_get_size (expected);
  guint8 *actual = g_malloc (size);
  gegl_buffer_get (buffer, NULL, 1, babl_format ("R'G'B'A u8"), actual,
                   GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);
  g_assert_cmpmem (actual, size, g_bytes_get_data (expected, NULL), size);
  g_free (actual);
}

typedef struct
{
  GimpImage *image;
  GeglBuffer *completed;
  GBytes *cache;
  GimpFilterLayerState phase;
  gboolean close, called;
  GimpFilterLayerSnapshot saved;
  GFile *file;
} XcfValueInvertActiveSave;

static void
xcf_value_invert_active_save (GimpFilterLayer *filter, gpointer data)
{
  XcfValueInvertActiveSave *test = data;
  GimpFilterLayerProgress progress;
  GError *error = NULL;
  if (test->called || gimp_filter_layer_get_state (filter) != test->phase) return;
  if (test->phase == GIMP_FILTER_LAYER_IMPORTING)
    {
      if (!gimp_filter_layer_get_progress (filter, &progress) ||
          progress.value <= 0.85 || progress.value >= 1.0) return;
      g_assert_true (progress.active);
      g_test_message ("Value Invert saves real partial staged import: progress=%.9f close=%d",
                       progress.value, test->close);
    }
  test->called = TRUE;
  g_assert_true (gimp_filter_layer_get_snapshot_state (filter, &test->saved));
  g_assert_cmpint (test->saved.state, ==, test->phase);
  g_assert_true (test->saved.cache_complete);
  g_assert_cmpuint (test->saved.generation, >, test->saved.cache_generation);
  g_assert_true (test->completed == gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)));
  xcf_value_invert_pixels (GIMP_LAYER (filter), test->cache, 4);
  test->file = xcf_value_invert_save (test->image);
  g_assert_true (test->completed == gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)));
  xcf_value_invert_pixels (GIMP_LAYER (filter), test->cache, 4);
  if (test->close)
    { g_assert_true (gimp_painter_binding_close (G_OBJECT (filter), &error)); g_assert_no_error (error); }
  else gimp_filter_layer_cancel (filter);
  g_assert_cmpint (gimp_filter_layer_get_state (filter), ==,
                   test->close ? GIMP_FILTER_LAYER_CLOSED : GIMP_FILTER_LAYER_FAILED);
}

/* Synthetic active Save/cancel/close/reopen evidence. RUNNING is captured on
 * vector launch, whereas spill waits for published positive import progress,
 * proving actual private staging rather than stopping at the import gate.
 * Full-image checks retain the old completed buffer across owner destruction. */
static void
value_invert_active_save_close_reopen (void)
{
  for (guint phase = 0; phase < 2; ++phase)
    for (guint close = 0; close < 2; ++close)
      {
        const gint width = phase ? 1031 : 63, height = phase ? 2049 : 61;
        const Babl *format = babl_format ("R'G'B'A u8");
        const guint8 input[] = {103, 71, 199, 255}, sentinel[] = {13, 29, 47, 255};
        GBytes *cache = xcf_value_invert_constant_pixels (width, height, 4, sentinel);
        GBytes *source_bytes = xcf_value_invert_constant_pixels (width, height, 4, input);
        GBytes *result = xcf_value_invert_result (source_bytes);
        GimpImage *image = gimp_image_new (gimp, width, height, GIMP_RGB, GIMP_PRECISION_U8_NON_LINEAR);
        GimpLayer *source = gimp_layer_new (image, width, height, format, "value-invert source",
                                           1, GIMP_LAYER_MODE_NORMAL_LEGACY);
        GimpFilterLayer *filter = GIMP_FILTER_LAYER (gimp_filter_layer_new (
          image, width, height, "value-invert filter", 1, GIMP_LAYER_MODE_NORMAL_LEGACY));
        GimpValueArray *args = gimp_value_array_new_from_types (NULL,
          G_TYPE_INT, 1, G_TYPE_INT, 0, G_TYPE_INT, 0, G_TYPE_NONE);
        GBytes *raw = xcf_value_invert_capsule (1);
        XcfValueInvertActiveSave test = {0};
        GimpFilterLayerSnapshot loaded, committed;
        GimpImage *copy;
        GError *error = NULL;
        gint64 deadline;
        gchar *original_swap = NULL, *private_swap = g_dir_make_tmp ("value-invert-xcf-active-XXXXXX", &error);
        g_assert_no_error (error); g_assert_nonnull (private_swap);
        g_object_get (gegl_config (), "swap", &original_swap, NULL);
        g_object_set (gegl_config (), "swap", private_swap, NULL);
        gimp_image_add_layer (image, source, NULL, 0, FALSE);
        gimp_image_add_layer (image, GIMP_LAYER (filter), NULL, 0, FALSE);
        gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (source)), NULL, 0, format,
                         g_bytes_get_data (source_bytes, NULL), GEGL_AUTO_ROWSTRIDE);
        /* An explicit full selection requires separate bounded mask capture
         * and import chunks, allowing a real partial-import progress event. */
        if (phase)
          gimp_channel_select_rectangle (gimp_image_get_mask (image), 0, 0, width, height,
                                         GIMP_CHANNEL_OP_REPLACE, FALSE, 0, 0, FALSE);
        g_assert_true (gimp_filter_layer_set_definition (filter, "plug-in-vinvert", raw, args, &error));
        g_assert_no_error (error);
        gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)), NULL, 0, format,
                         g_bytes_get_data (cache, NULL), GEGL_AUTO_ROWSTRIDE);
        gimp_filter_layer_mark_as_loaded (filter);
        test.image = image; test.cache = cache; test.close = close;
        test.completed = g_object_ref (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)));
        test.phase = phase ? GIMP_FILTER_LAYER_IMPORTING : GIMP_FILTER_LAYER_RUNNING;
        g_signal_connect (filter, phase ? "filter-progress-changed" : "filter-state-changed",
                          G_CALLBACK (xcf_value_invert_active_save), &test);
        gimp_filter_layer_invalidate (filter);
        deadline = g_get_monotonic_time () + 90 * G_TIME_SPAN_SECOND;
        while (!test.called && g_get_monotonic_time () < deadline)
          {
            g_assert_true (test.completed == gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)));
            g_assert_cmpint (gimp_filter_layer_get_state (filter), !=, GIMP_FILTER_LAYER_CLEAN);
            g_main_context_iteration (NULL, FALSE); g_usleep (100);
          }
        g_assert_true (test.called); g_assert_nonnull (test.file);
        g_signal_handlers_disconnect_by_data (filter, &test);
        xcf_value_invert_parameters (filter, 1); xcf_value_invert_raw_equal (filter, raw);
        xcf_value_invert_pixels (GIMP_LAYER (filter), cache, 4);
        copy = open_file (test.file, NULL, NULL, &error);
        g_assert_no_error (error); g_assert_nonnull (copy);
        filter = xcf_value_invert_filter (copy);
        xcf_value_invert_parameters (filter, 1); xcf_value_invert_raw_equal (filter, raw);
        xcf_value_invert_serialized_snapshot (filter, &test.saved);
        g_assert_true (gimp_filter_layer_get_snapshot_state (filter, &loaded));
        g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 0);
        /* Saved tokens are provenance; the new live scheduler owns its epoch. */
        g_assert_cmpint (loaded.cache_complete, ==, test.saved.cache_complete);
        g_assert_cmpuint (loaded.cache_generation, ==, 0);
        g_assert_cmpuint (loaded.generation, >, loaded.cache_generation);
        g_assert_cmpint (loaded.state, ==, GIMP_FILTER_LAYER_WAITING);
        xcf_value_invert_pixels (GIMP_LAYER (filter), cache, 4);
        g_object_unref (image);
        xcf_value_invert_buffer_equal (test.completed, cache);
        g_assert_cmpuint (gimp_filter_layer_get_generation (filter), ==, loaded.generation);
        xcf_value_invert_settle (filter, GIMP_FILTER_LAYER_CLEAN);
        g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 1);
        g_assert_true (gimp_filter_layer_get_snapshot_state (filter, &committed));
        g_assert_cmpuint (committed.generation, ==, loaded.generation);
        g_assert_cmpuint (committed.cache_generation, ==, committed.generation);
        g_assert_true (committed.cache_complete);
        xcf_value_invert_pixels (GIMP_LAYER (filter), result, 4); xcf_value_invert_raw_equal (filter, raw);
        xcf_value_invert_buffer_equal (test.completed, cache);
        g_object_unref (test.completed);
        copy = xcf_value_invert_close_reopen (copy); filter = xcf_value_invert_filter (copy);
        xcf_value_invert_serialized_snapshot (filter, &committed);
        xcf_value_invert_loaded (filter); xcf_value_invert_parameters (filter, 1);
        xcf_value_invert_raw_equal (filter, raw); xcf_value_invert_pixels (GIMP_LAYER (filter), result, 4);
        g_object_unref (copy); g_bytes_unref (raw); gimp_value_array_unref (args);
        g_bytes_unref (cache); g_bytes_unref (source_bytes); g_bytes_unref (result);
        g_assert_true (g_file_delete (test.file, NULL, &error)); g_assert_no_error (error); g_object_unref (test.file);
        g_object_set (gegl_config (), "swap", original_swap, NULL); g_free (original_swap);
        { GFile *directory = g_file_new_for_path (private_swap); g_file_delete (directory, NULL, NULL); g_object_unref (directory); }
        g_free (private_swap);
      }
}

/* Genuine pinned old-writer consumer. The capture/materialization wrapper must
 * provide both real files and their observed native outputs. Missing evidence
 * is a hard failure; no synthetic fixture or scalar oracle is substituted. */
static gchar *
xcf_value_invert_old_name (guint channels, const gchar *suffix)
{
  return g_strdup_printf ("legacy-value-invert-context/live-c%u-17x13-s0-f-1-a15-l0%s",
                           channels, suffix);
}

static GBytes *
xcf_value_invert_old_bytes (guint channels, const gchar *suffix)
{
  gchar *name = xcf_value_invert_old_name (channels, suffix), *path = fixture_path (name), *data;
  gsize size;
  GError *error = NULL;
  g_assert_true (g_file_get_contents (path, &data, &size, &error));
  g_assert_no_error (error); g_free (path); g_free (name);
  return g_bytes_new_take (data, size);
}

static void
value_invert_old_writer_roundtrip (void)
{
  for (guint channels = 3; channels <= 4; ++channels)
    {
      gchar *name = xcf_value_invert_old_name (channels, ".xcf");
      GimpImage *image = open_fixture (name);
      GimpFilterLayer *filter = xcf_value_invert_filter (image);
      GBytes *raw = xcf_value_invert_old_bytes (channels, ".definition.raw");
      GBytes *schema = xcf_value_invert_capsule (1);
      GBytes *source = xcf_value_invert_old_bytes (channels, "-source.raw");
      GBytes *output = xcf_value_invert_old_bytes (channels, "-output.raw");
      GBytes *source2 = xcf_value_invert_old_bytes (channels, "-source2.raw");
      GBytes *output2 = xcf_value_invert_old_bytes (channels, "-output2.raw");
      GimpItem *copy;
      GError *error = NULL;
      g_test_message ("Value Invert old-writer case %s", name);
      /* Literal, framed length, tags, payload sizes and retained run mode must
       * match the independently constructed, source-predicted strict schema. */
      g_assert_true (g_bytes_equal (raw, schema)); g_bytes_unref (schema);
      g_assert_cmpuint (g_bytes_get_size (source), ==, 17 * 13 * channels);
      g_assert_cmpuint (g_bytes_get_size (source2), ==, 17 * 13 * channels);
      g_assert_false (g_bytes_equal (source, source2));
      xcf_value_invert_loaded (filter); xcf_value_invert_parameters (filter, 1);
      xcf_value_invert_raw_equal (filter, raw); xcf_value_invert_pixels (GIMP_LAYER (filter), output, 4);
      original_exact (image, name);
      copy = gimp_item_duplicate (GIMP_ITEM (filter), GIMP_TYPE_FILTER_LAYER);
      g_assert_nonnull (copy); g_object_ref_sink (copy);
      xcf_value_invert_loaded (GIMP_FILTER_LAYER (copy));
      xcf_value_invert_parameters (GIMP_FILTER_LAYER (copy), 1);
      xcf_value_invert_raw_equal (GIMP_FILTER_LAYER (copy), raw);
      xcf_value_invert_pixels (GIMP_LAYER (copy), output, 4); g_object_unref (copy);
      image = xcf_value_invert_close_reopen (image); filter = xcf_value_invert_filter (image);
      xcf_value_invert_loaded (filter); xcf_value_invert_parameters (filter, 1);
      xcf_value_invert_raw_equal (filter, raw); xcf_value_invert_pixels (GIMP_LAYER (filter), output, 4);
      xcf_value_invert_lower_input (image, channels, source2);
      xcf_value_invert_settle (filter, GIMP_FILTER_LAYER_CLEAN);
      g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 1);
      xcf_value_invert_pixels (GIMP_LAYER (filter), output2, 4); xcf_value_invert_raw_equal (filter, raw);
      /* Unchanged-byte notifications independently exercise the new owner's
       * lifecycle, with genuine second-run output remaining the pixel oracle. */
      xcf_value_invert_lower_input (image, channels, source2);
      xcf_value_invert_settle (filter, GIMP_FILTER_LAYER_CLEAN);
      g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 2);
      xcf_value_invert_pixels (GIMP_LAYER (filter), output2, 4);
      image = xcf_value_invert_close_reopen (image); filter = xcf_value_invert_filter (image);
      xcf_value_invert_loaded (filter); xcf_value_invert_parameters (filter, 1);
      xcf_value_invert_raw_equal (filter, raw); xcf_value_invert_pixels (GIMP_LAYER (filter), output2, 4);
      {
        const gint edited_run_mode = G_MININT32;
        GimpValueArray *edited = gimp_filter_layer_dup_args (filter);
        GBytes *changed = xcf_value_invert_capsule (edited_run_mode);
        /* This is a native edit of the retained context scalar, not a second
         * old-writer observation. Its pixels still use genuine output2. */
        g_value_set_int (gimp_value_array_index (edited, 0), edited_run_mode);
        g_assert_true (gimp_filter_layer_edit_definition (filter, "plug-in-vinvert", changed, edited, &error));
        g_assert_no_error (error); gimp_value_array_unref (edited);
        xcf_value_invert_parameters (filter, edited_run_mode); xcf_value_invert_raw_equal (filter, changed);
        g_assert_true (gimp_image_undo (image));
        xcf_value_invert_parameters (filter, 1); xcf_value_invert_raw_equal (filter, raw);
        xcf_value_invert_settle (filter, GIMP_FILTER_LAYER_CLEAN);
        xcf_value_invert_pixels (GIMP_LAYER (filter), output2, 4);
        g_assert_true (gimp_image_redo (image));
        xcf_value_invert_parameters (filter, edited_run_mode); xcf_value_invert_raw_equal (filter, changed);
        xcf_value_invert_settle (filter, GIMP_FILTER_LAYER_CLEAN);
        xcf_value_invert_pixels (GIMP_LAYER (filter), output2, 4);
        image = xcf_value_invert_close_reopen (image); filter = xcf_value_invert_filter (image);
        xcf_value_invert_loaded (filter); xcf_value_invert_parameters (filter, edited_run_mode);
        xcf_value_invert_raw_equal (filter, changed); xcf_value_invert_pixels (GIMP_LAYER (filter), output2, 4);
        g_bytes_unref (changed);
      }
      g_object_unref (image); g_free (name); g_bytes_unref (raw);
      g_bytes_unref (source); g_bytes_unref (source2); g_bytes_unref (output); g_bytes_unref (output2);
    }
}
