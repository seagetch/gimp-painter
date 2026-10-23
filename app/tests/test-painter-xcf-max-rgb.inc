/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Synthetic XCF schema and native lifecycle tests. The capsule below follows
 * the pinned old xcf-save.c construction and max-rgb.c argument declarations.
 * Neither these manufactured files nor their analytic pixels are genuine-old
 * writer/runtime evidence. A separately captured corpus remains a parity gate.
 */
static GimpFilterLayer *
xcf_max_rgb_filter (GimpImage *image)
{
  GimpLayer *layer = find_layer (image, "max-rgb filter");
  g_assert_nonnull (layer); g_assert_true (GIMP_IS_FILTER_LAYER (layer));
  return GIMP_FILTER_LAYER (layer);
}

static void
xcf_max_rgb_raw_equal (GimpFilterLayer *filter, GBytes *expected)
{
  GBytes *actual = gimp_filter_layer_ref_definition (filter);
  g_assert_nonnull (actual); g_assert_true (g_bytes_equal (actual, expected));
  g_bytes_unref (actual);
}

static void
xcf_max_rgb_parameters (GimpFilterLayer *filter, gint run_mode, gint mode)
{
  GimpValueArray *args = gimp_filter_layer_dup_args (filter);
  gchar *name = gimp_filter_layer_dup_procedure (filter);
  g_assert_cmpstr (name, ==, "plug-in-max-rgb"); g_free (name);
  g_assert_nonnull (args);
  g_assert_cmpuint (gimp_value_array_length (args), ==, 4);
  for (guint i = 0; i < 4; ++i)
    g_assert_true (G_VALUE_HOLDS_INT (gimp_value_array_index (args, i)));
  g_assert_cmpint (g_value_get_int (gimp_value_array_index (args, 0)), ==, run_mode);
  g_assert_cmpint (g_value_get_int (gimp_value_array_index (args, 1)), ==, 0);
  g_assert_cmpint (g_value_get_int (gimp_value_array_index (args, 2)), ==, 0);
  g_assert_cmpint (g_value_get_int (gimp_value_array_index (args, 3)), ==, mode);
  gimp_value_array_unref (args);
}

static void
xcf_max_rgb_pixels (GimpLayer *layer, GBytes *expected, guint channels)
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
xcf_max_rgb_settle (GimpFilterLayer *filter, GimpFilterLayerState expected)
{
  gint64 deadline = g_get_monotonic_time () + 20 * G_TIME_SPAN_SECOND;
  while (gimp_filter_layer_get_state (filter) != expected &&
         g_get_monotonic_time () < deadline)
    {
      if (gimp_filter_layer_get_state (filter) == GIMP_FILTER_LAYER_FAILED &&
          expected != GIMP_FILTER_LAYER_FAILED)
        { gchar *error = gimp_filter_layer_dup_error (filter); g_error ("Max RGB XCF rerun failed: %s", error); }
      g_main_context_iteration (NULL, FALSE); g_usleep (100);
    }
  g_assert_cmpint (gimp_filter_layer_get_state (filter), ==, expected);
}

static void
xcf_max_rgb_loaded (GimpFilterLayer *filter)
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
xcf_max_rgb_lower_input (GimpImage *image, guint channels, GBytes *bytes)
{
  GimpLayer *source = find_layer (image, "max-rgb source");
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
xcf_max_rgb_save (GimpImage *image)
{
  GError *error = NULL;
  GFileIOStream *io;
  GFile *file = g_file_new_tmp ("painter-max-rgb-roundtrip-XXXXXX", &io, &error);
  GimpPlugInProcedure *procedure = GIMP_PLUG_IN_PROCEDURE (gimp_pdb_lookup_procedure (gimp->pdb, "gimp-xcf-save"));
  g_assert_no_error (error); g_assert_nonnull (file); g_assert_nonnull (procedure);
  g_assert_true (g_io_stream_close (G_IO_STREAM (io), NULL, &error));
  g_assert_no_error (error); g_object_unref (io);
  g_assert_cmpint (file_save (gimp, image, NULL, file, procedure, GIMP_RUN_NONINTERACTIVE,
                              FALSE, FALSE, FALSE, &error), ==, GIMP_PDB_SUCCESS);
  g_assert_no_error (error); return file;
}

static GimpImage *
xcf_max_rgb_close_reopen (GimpImage *image)
{
  GFile *file = xcf_max_rgb_save (image);
  GError *error = NULL;
  g_object_unref (image);
  image = open_file (file, NULL, NULL, &error);
  g_assert_no_error (error); g_assert_nonnull (image);
  g_assert_true (g_file_delete (file, NULL, &error)); g_assert_no_error (error);
  g_object_unref (file); return image;
}

static GBytes *
xcf_max_rgb_constant_pixels (guint width, guint height, guint channels, const guint8 *pixel)
{
  gsize size = (gsize) width * height * channels;
  guint8 *bytes = g_malloc (size);
  for (gsize i = 0; i < size; i += channels) memcpy (bytes + i, pixel, channels);
  return g_bytes_new_take (bytes, size);
}

static void
xcf_max_rgb_serialized_snapshot (GimpFilterLayer *filter, const GimpFilterLayerSnapshot *saved)
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
xcf_max_rgb_capsule (gint run_mode, gint mode)
{
  static const gchar name[] = "plug-in-max-rgb";
  guint8 data[68] = {0};
  put32 (data, 0, sizeof name); memcpy (data + 4, name, sizeof name);
  put32 (data, 20, 2); put32 (data, 24, 4); put32 (data, 28, (guint32) run_mode);
  put32 (data, 32, 1); put32 (data, 40, 10);
  put32 (data, 48, 2); put32 (data, 52, 4); put32 (data, 56, (guint32) mode);
  return g_bytes_new (data, sizeof data);
}

static void
xcf_max_rgb_append_word (GByteArray *bytes, guint32 value)
{
  value = GUINT32_TO_BE (value);
  g_byte_array_append (bytes, (const guint8 *) &value, sizeof value);
}

/* Only small single-tile uncompressed RGBA fixtures use this test builder. */
static guint
xcf_max_rgb_append_layer (GByteArray *bytes, const gchar *name, GBytes *raw,
                          GBytes *pixels, guint *capsule_offset)
{
  const guint start = bytes->len;
  guint hierarchy_pointer, level_pointer, tile_pointer;
  g_assert_cmpuint (g_bytes_get_size (pixels), ==, 17 * 13 * 4);
  xcf_max_rgb_append_word (bytes, 17); xcf_max_rgb_append_word (bytes, 13);
  xcf_max_rgb_append_word (bytes, GIMP_RGBA_IMAGE);
  xcf_max_rgb_append_word (bytes, strlen (name) + 1);
  g_byte_array_append (bytes, (const guint8 *) name, strlen (name) + 1);
  if (raw)
    {
      xcf_max_rgb_append_word (bytes, 32);
      xcf_max_rgb_append_word (bytes, g_bytes_get_size (raw));
      *capsule_offset = bytes->len;
      g_byte_array_append (bytes, g_bytes_get_data (raw, NULL), g_bytes_get_size (raw));
    }
  xcf_max_rgb_append_word (bytes, 0); xcf_max_rgb_append_word (bytes, 0);
  hierarchy_pointer = bytes->len;
  xcf_max_rgb_append_word (bytes, 0); xcf_max_rgb_append_word (bytes, 0);
  put32 (bytes->data, hierarchy_pointer, bytes->len);
  xcf_max_rgb_append_word (bytes, 17); xcf_max_rgb_append_word (bytes, 13);
  xcf_max_rgb_append_word (bytes, 4);
  level_pointer = bytes->len;
  xcf_max_rgb_append_word (bytes, 0); xcf_max_rgb_append_word (bytes, 0);
  put32 (bytes->data, level_pointer, bytes->len);
  xcf_max_rgb_append_word (bytes, 17); xcf_max_rgb_append_word (bytes, 13);
  tile_pointer = bytes->len;
  xcf_max_rgb_append_word (bytes, 0); xcf_max_rgb_append_word (bytes, 0);
  put32 (bytes->data, tile_pointer, bytes->len);
  g_byte_array_append (bytes, g_bytes_get_data (pixels, NULL), g_bytes_get_size (pixels));
  return start;
}

static GBytes *
xcf_max_rgb_synthetic_file (GBytes *raw, GBytes *cache, GBytes *source, guint *capsule_offset)
{
  GByteArray *bytes = g_byte_array_sized_new (2200);
  guint8 header[50] = {0};
  guint filter_offset, source_offset;
  memcpy (header, "gimp xcf v003", 13);
  put32 (header, 14, 17); put32 (header, 18, 13);
  g_byte_array_append (bytes, header, sizeof header);
  filter_offset = xcf_max_rgb_append_layer (bytes, "max-rgb filter", raw, cache, capsule_offset);
  source_offset = xcf_max_rgb_append_layer (bytes, "max-rgb source", NULL, source, NULL);
  put32 (bytes->data, 34, filter_offset); put32 (bytes->data, 38, source_offset);
  return g_byte_array_free_to_bytes (bytes);
}

static GimpImage *
xcf_max_rgb_open_synthetic (GBytes *raw, GBytes *cache, GBytes *source)
{
  guint offset;
  GBytes *bytes = xcf_max_rgb_synthetic_file (raw, cache, source, &offset);
  GError *error = NULL;
  GimpImage *image = open_bytes (g_bytes_get_data (bytes, NULL), g_bytes_get_size (bytes),
                                XCF_PAINTER_DIALECT_LEGACY, &error);
  g_assert_no_error (error); g_assert_nonnull (image); g_bytes_unref (bytes);
  return image;
}

static GBytes *
xcf_max_rgb_source (guint phase)
{
  guint8 data[17 * 13 * 4];
  for (guint i = 0; i < 17 * 13; ++i)
    {
      data[i * 4] = (i * 71 + phase * 37) & 255;
      data[i * 4 + 1] = i % 5 == 0 ? data[i * 4] : (i * 13 + phase * 89) & 255;
      data[i * 4 + 2] = i % 7 == 0 ? data[i * 4 + 1] : (i * 43 + phase * 19) & 255;
      data[i * 4 + 3] = 255;
    }
  return g_bytes_new (data, sizeof data);
}

/* Analytic full-coverage, opaque RGB result, independent of the port kernel. */
static GBytes *
xcf_max_rgb_opaque_result (GBytes *source, gint mode)
{
  gsize size;
  const guint8 *input = g_bytes_get_data (source, &size);
  guint8 *output = g_memdup2 (input, size);
  for (gsize i = 0; i < size; i += 4)
    {
      guint8 extremum = input[i];
      g_assert_cmpuint (input[i + 3], ==, 255);
      for (guint c = 1; c < 3; ++c)
        extremum = mode > 0 ? MAX (extremum, input[i + c]) : MIN (extremum, input[i + c]);
      for (guint c = 0; c < 3; ++c)
        output[i + c] = input[i + c] == extremum ? extremum : 0;
    }
  return g_bytes_new_take (output, size);
}

static void
max_rgb_synthetic_capsule_roundtrip (void)
{
  static const gint modes[] = {G_MININT32, -1, 0, 1, 2, G_MAXINT32};
  static const gint run_modes[] = {G_MININT32, -1, 0, 1, 2, G_MAXINT32};
  const guint8 sentinel[] = {13, 29, 47, 255};
  GBytes *cache = xcf_max_rgb_constant_pixels (17, 13, 4, sentinel);
  GBytes *source = xcf_max_rgb_source (0), *source2 = xcf_max_rgb_source (1);
  for (guint i = 0; i < G_N_ELEMENTS (modes); ++i)
    {
      const gint mode = modes[i], run_mode = run_modes[i];
      GBytes *raw = xcf_max_rgb_capsule (run_mode, mode);
      GBytes *output = xcf_max_rgb_opaque_result (source2, mode);
      GimpImage *image = xcf_max_rgb_open_synthetic (raw, cache, source);
      GimpFilterLayer *filter = xcf_max_rgb_filter (image);
      GimpItem *copy;
      GError *error = NULL;
      xcf_max_rgb_loaded (filter); xcf_max_rgb_parameters (filter, run_mode, mode);
      xcf_max_rgb_raw_equal (filter, raw); xcf_max_rgb_pixels (GIMP_LAYER (filter), cache, 4);
      copy = gimp_item_duplicate (GIMP_ITEM (filter), GIMP_TYPE_FILTER_LAYER);
      g_assert_nonnull (copy); g_object_ref_sink (copy);
      xcf_max_rgb_loaded (GIMP_FILTER_LAYER (copy));
      xcf_max_rgb_parameters (GIMP_FILTER_LAYER (copy), run_mode, mode);
      xcf_max_rgb_raw_equal (GIMP_FILTER_LAYER (copy), raw);
      xcf_max_rgb_pixels (GIMP_LAYER (copy), cache, 4); g_object_unref (copy);
      image = xcf_max_rgb_close_reopen (image); filter = xcf_max_rgb_filter (image);
      xcf_max_rgb_loaded (filter); xcf_max_rgb_parameters (filter, run_mode, mode);
      xcf_max_rgb_raw_equal (filter, raw); xcf_max_rgb_pixels (GIMP_LAYER (filter), cache, 4);
      xcf_max_rgb_lower_input (image, 4, source2);
      xcf_max_rgb_settle (filter, GIMP_FILTER_LAYER_CLEAN);
      g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 1);
      xcf_max_rgb_pixels (GIMP_LAYER (filter), output, 4); xcf_max_rgb_raw_equal (filter, raw);
      /* Equal-byte lower notifications still create a new owner generation. */
      xcf_max_rgb_lower_input (image, 4, source2);
      xcf_max_rgb_settle (filter, GIMP_FILTER_LAYER_CLEAN);
      g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 2);
      xcf_max_rgb_pixels (GIMP_LAYER (filter), output, 4);
      image = xcf_max_rgb_close_reopen (image); filter = xcf_max_rgb_filter (image);
      xcf_max_rgb_loaded (filter); xcf_max_rgb_parameters (filter, run_mode, mode);
      xcf_max_rgb_raw_equal (filter, raw); xcf_max_rgb_pixels (GIMP_LAYER (filter), output, 4);
      {
        const gint edited_mode = mode > 0 ? G_MININT32 : G_MAXINT32;
        GimpValueArray *edited = gimp_filter_layer_dup_args (filter);
        GBytes *changed = xcf_max_rgb_capsule (run_mode, edited_mode);
        GBytes *edited_output = xcf_max_rgb_opaque_result (source2, edited_mode);
        g_value_set_int (gimp_value_array_index (edited, 3), edited_mode);
        g_assert_true (gimp_filter_layer_edit_definition (filter, "plug-in-max-rgb", changed, edited, &error));
        g_assert_no_error (error); gimp_value_array_unref (edited);
        xcf_max_rgb_parameters (filter, run_mode, edited_mode); xcf_max_rgb_raw_equal (filter, changed);
        g_assert_true (gimp_image_undo (image));
        xcf_max_rgb_parameters (filter, run_mode, mode); xcf_max_rgb_raw_equal (filter, raw);
        xcf_max_rgb_settle (filter, GIMP_FILTER_LAYER_CLEAN);
        xcf_max_rgb_pixels (GIMP_LAYER (filter), output, 4);
        g_assert_true (gimp_image_redo (image));
        xcf_max_rgb_parameters (filter, run_mode, edited_mode); xcf_max_rgb_raw_equal (filter, changed);
        xcf_max_rgb_settle (filter, GIMP_FILTER_LAYER_CLEAN);
        xcf_max_rgb_pixels (GIMP_LAYER (filter), edited_output, 4);
        image = xcf_max_rgb_close_reopen (image); filter = xcf_max_rgb_filter (image);
        xcf_max_rgb_loaded (filter); xcf_max_rgb_parameters (filter, run_mode, edited_mode);
        xcf_max_rgb_raw_equal (filter, changed); xcf_max_rgb_pixels (GIMP_LAYER (filter), edited_output, 4);
        g_bytes_unref (changed); g_bytes_unref (edited_output);
      }
      g_object_unref (image); g_bytes_unref (raw); g_bytes_unref (output);
    }
  g_bytes_unref (cache); g_bytes_unref (source); g_bytes_unref (source2);
}

static void
max_rgb_malformed_capsules (void)
{
  const guint8 sentinel[] = {13, 29, 47, 255};
  GBytes *capsule = xcf_max_rgb_capsule (1, 1);
  GBytes *cache = xcf_max_rgb_constant_pixels (17, 13, 4, sentinel);
  GBytes *source = xcf_max_rgb_source (0), *source2 = xcf_max_rgb_source (1);
  guint offset;
  GBytes *original = xcf_max_rgb_synthetic_file (capsule, cache, source, &offset);
  gsize size;
  const guint8 *data = g_bytes_get_data (original, &size);
  for (guint mutation = 0; mutation < 16; ++mutation)
    {
      guint8 *bytes = g_memdup2 (data, size);
      GimpImage *image;
      GimpFilterLayer *filter;
      GBytes *expected_raw;
      GError *error = NULL;
      gsize retained_size = 68;
      g_test_message ("Synthetic Max RGB capsule mutation %u", mutation);
      /* Keep subsequent properties and absolute pixel offsets unchanged. */
      switch (mutation)
        {
        case 0: put32 (bytes, offset + 20, 5); break; /* float run mode */
        case 1: put32 (bytes, offset + 32, 10); break; /* wrong image kind */
        case 2: put32 (bytes, offset + 40, 1); break; /* wrong drawable kind */
        case 3: put32 (bytes, offset + 48, 5); break; /* float mode */
        case 4: put32 (bytes, offset + 24, 3); break; /* run-mode length */
        case 5: put32 (bytes, offset + 36, 4); break; /* omitted image payload */
        case 6: put32 (bytes, offset + 44, 4); break; /* omitted drawable payload */
        case 7: put32 (bytes, offset + 52, 3); break; /* mode length */
        case 8: put32 (bytes, offset + 64, 1); break; /* END payload */
        case 9: bytes[offset + 19] = 'x'; break; /* missing terminal NUL */
        case 10: bytes[offset + 12] = 0; break; /* embedded name NUL */
        case 11: put32 (bytes, offset - 4, 67); break; /* short outer length */
        case 12: put32 (bytes, offset - 4, 80); break; /* claimed bytes after END */
        case 13:
          /* Five omitted arguments plus END fit the same 68-byte capsule. */
          for (guint i = 0; i < 5; ++i)
            { put32 (bytes, offset + 20 + i * 8, 1); put32 (bytes, offset + 24 + i * 8, 0); }
          break;
        case 14:
          /* Three args then END; leftover bytes form a VISIBLE property. */
          put32 (bytes, offset - 4, 56);
          put32 (bytes, offset + 48, 0); put32 (bytes, offset + 52, 0);
          put32 (bytes, offset + 56, 8); put32 (bytes, offset + 60, 4);
          put32 (bytes, offset + 64, 1); retained_size = 56;
          break;
        case 15: bytes[offset + 4] = 'P'; break; /* unsupported literal */
        }
      image = open_bytes (bytes, size, XCF_PAINTER_DIALECT_LEGACY, &error);
      g_assert_no_error (error); g_assert_nonnull (image);
      filter = xcf_max_rgb_filter (image);
      g_assert_null (gimp_filter_layer_dup_args (filter)); xcf_max_rgb_loaded (filter);
      expected_raw = g_bytes_new (bytes + offset, retained_size);
      xcf_max_rgb_raw_equal (filter, expected_raw); xcf_max_rgb_pixels (GIMP_LAYER (filter), cache, 4);
      image = xcf_max_rgb_close_reopen (image); filter = xcf_max_rgb_filter (image);
      g_assert_null (gimp_filter_layer_dup_args (filter)); xcf_max_rgb_loaded (filter);
      xcf_max_rgb_raw_equal (filter, expected_raw); xcf_max_rgb_pixels (GIMP_LAYER (filter), cache, 4);
      xcf_max_rgb_lower_input (image, 4, source2);
      xcf_max_rgb_settle (filter, GIMP_FILTER_LAYER_FAILED);
      g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 0);
      xcf_max_rgb_raw_equal (filter, expected_raw); xcf_max_rgb_pixels (GIMP_LAYER (filter), cache, 4);
      image = xcf_max_rgb_close_reopen (image); filter = xcf_max_rgb_filter (image);
      g_assert_null (gimp_filter_layer_dup_args (filter));
      xcf_max_rgb_settle (filter, GIMP_FILTER_LAYER_FAILED);
      g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 0);
      xcf_max_rgb_raw_equal (filter, expected_raw); xcf_max_rgb_pixels (GIMP_LAYER (filter), cache, 4);
      g_object_unref (image); g_bytes_unref (expected_raw); g_free (bytes);
    }
  g_bytes_unref (original); g_bytes_unref (capsule); g_bytes_unref (cache);
  g_bytes_unref (source); g_bytes_unref (source2);
}

/* Synthetic active Save/close/reopen evidence. Both signed-extreme modes cross
 * vector RUNNING and real-spill IMPORTING. Every cache comparison is full-image.
 */
static void
max_rgb_active_save_close_reopen (void)
{
  static const gint modes[] = {G_MININT32, G_MAXINT32};
  for (guint m = 0; m < G_N_ELEMENTS (modes); ++m)
    for (guint phase = 0; phase < 2; ++phase)
      {
        const gint mode = modes[m];
        const gint width = phase ? 1031 : 63, height = phase ? 1021 : 61;
        const GimpFilterLayerState state = phase ? GIMP_FILTER_LAYER_IMPORTING : GIMP_FILTER_LAYER_RUNNING;
        const Babl *format = babl_format ("R'G'B'A u8");
        const guint8 input[] = {103, 71, 199, 255}, sentinel[] = {13, 29, 47, 255};
        const guint8 minimum[] = {0, 71, 0, 255}, maximum[] = {0, 0, 199, 255};
        GBytes *cache = xcf_max_rgb_constant_pixels (width, height, 4, sentinel);
        GBytes *result = xcf_max_rgb_constant_pixels (width, height, 4, mode > 0 ? maximum : minimum);
        GimpImage *image = gimp_image_new (gimp, width, height, GIMP_RGB, GIMP_PRECISION_U8_NON_LINEAR);
        GimpLayer *source = gimp_layer_new (image, width, height, format, "max-rgb source",
                                           1, GIMP_LAYER_MODE_NORMAL_LEGACY);
        GimpFilterLayer *filter = GIMP_FILTER_LAYER (gimp_filter_layer_new (
          image, width, height, "max-rgb filter", 1, GIMP_LAYER_MODE_NORMAL_LEGACY));
        GimpValueArray *args = gimp_value_array_new_from_types (NULL, G_TYPE_INT, 1,
          G_TYPE_INT, 0, G_TYPE_INT, 0, G_TYPE_INT, mode, G_TYPE_NONE);
        GBytes *raw = xcf_max_rgb_capsule (1, mode);
        GeglColor *color = gegl_color_new (NULL);
        GeglBuffer *completed;
        GimpFilterLayerSnapshot saved, loaded, committed;
        GFile *file;
        GimpImage *copy;
        GError *error = NULL;
        gchar *original_swap = NULL, *private_swap = g_dir_make_tmp ("max-rgb-xcf-active-XXXXXX", &error);
        g_assert_no_error (error); g_assert_nonnull (private_swap);
        g_object_get (gegl_config (), "swap", &original_swap, NULL);
        g_object_set (gegl_config (), "swap", private_swap, NULL);
        gimp_image_add_layer (image, source, NULL, 0, FALSE);
        gimp_image_add_layer (image, GIMP_LAYER (filter), NULL, 0, FALSE);
        gegl_color_set_pixel (color, format, input);
        gegl_buffer_set_color (gimp_drawable_get_buffer (GIMP_DRAWABLE (source)), NULL, color);
        gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)), NULL, 0, format,
                         g_bytes_get_data (cache, NULL), GEGL_AUTO_ROWSTRIDE);
        g_object_unref (color);
        completed = g_object_ref (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)));
        g_assert_true (gimp_filter_layer_set_definition (filter, "plug-in-max-rgb", raw, args, &error));
        g_assert_no_error (error);
        xcf_max_rgb_settle (filter, state);
        g_assert_true (gimp_filter_layer_get_snapshot_state (filter, &saved));
        g_assert_cmpuint (saved.generation, >, saved.cache_generation);
        g_assert_true (completed == gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)));
        xcf_max_rgb_pixels (GIMP_LAYER (filter), cache, 4);
        file = xcf_max_rgb_save (image);
        g_assert_true (completed == gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)));
        xcf_max_rgb_pixels (GIMP_LAYER (filter), cache, 4);
        copy = open_file (file, NULL, NULL, &error);
        g_assert_no_error (error); g_assert_nonnull (copy);
        filter = xcf_max_rgb_filter (copy);
        xcf_max_rgb_parameters (filter, 1, mode); xcf_max_rgb_raw_equal (filter, raw);
        xcf_max_rgb_serialized_snapshot (filter, &saved);
        g_assert_true (gimp_filter_layer_get_snapshot_state (filter, &loaded));
        g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 0);
        /* Saved tokens are provenance; the new live scheduler owns its epoch. */
        g_assert_cmpint (loaded.cache_complete, ==, saved.cache_complete);
        g_assert_cmpuint (loaded.cache_generation, ==, 0);
        g_assert_cmpuint (loaded.generation, >, loaded.cache_generation);
        g_assert_cmpint (loaded.state, ==, GIMP_FILTER_LAYER_WAITING);
        xcf_max_rgb_pixels (GIMP_LAYER (filter), cache, 4);
        g_object_unref (image); g_object_unref (completed);
        g_assert_cmpuint (gimp_filter_layer_get_generation (filter), ==, loaded.generation);
        xcf_max_rgb_settle (filter, GIMP_FILTER_LAYER_CLEAN);
        g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 1);
        g_assert_true (gimp_filter_layer_get_snapshot_state (filter, &committed));
        g_assert_cmpuint (committed.generation, ==, loaded.generation);
        g_assert_cmpuint (committed.cache_generation, ==, committed.generation);
        g_assert_true (committed.cache_complete);
        xcf_max_rgb_pixels (GIMP_LAYER (filter), result, 4); xcf_max_rgb_raw_equal (filter, raw);
        copy = xcf_max_rgb_close_reopen (copy); filter = xcf_max_rgb_filter (copy);
        xcf_max_rgb_serialized_snapshot (filter, &committed);
        xcf_max_rgb_loaded (filter); xcf_max_rgb_parameters (filter, 1, mode);
        xcf_max_rgb_raw_equal (filter, raw); xcf_max_rgb_pixels (GIMP_LAYER (filter), result, 4);
        g_object_unref (copy); g_bytes_unref (raw); gimp_value_array_unref (args);
        g_bytes_unref (cache); g_bytes_unref (result);
        g_assert_true (g_file_delete (file, NULL, &error)); g_assert_no_error (error); g_object_unref (file);
        g_object_set (gegl_config (), "swap", original_swap, NULL); g_free (original_swap);
        { GFile *directory = g_file_new_for_path (private_swap); g_file_delete (directory, NULL, NULL); g_object_unref (directory); }
        g_free (private_swap);
      }
}

/* This separate consumer requires actual pinned old-runtime writer files and
 * observed outputs. Missing capture/materialization is a hard failure, not a
 * skip or substitution with the synthetic schema tests above. */
static gchar *
xcf_max_rgb_old_name (guint channels, gint mode, const gchar *suffix)
{
  return g_strdup_printf ("legacy-max-rgb-context/live-c%u-17x13-m%d-s0-f-1-a15-l0%s",
                           channels, mode, suffix);
}

static GBytes *
xcf_max_rgb_old_bytes (guint channels, gint mode, const gchar *suffix)
{
  gchar *name = xcf_max_rgb_old_name (channels, mode, suffix), *path = fixture_path (name), *data;
  gsize size;
  GError *error = NULL;
  g_assert_true (g_file_get_contents (path, &data, &size, &error));
  g_assert_no_error (error); g_free (path); g_free (name);
  return g_bytes_new_take (data, size);
}

/* Register after the capture verifier/materializer is wired into the runner. */
static void G_GNUC_UNUSED
max_rgb_old_writer_roundtrip (void)
{
  static const gint modes[] = {G_MININT32, -2, -1, 0, 1, 3, G_MAXINT32};
  for (guint channels = 3; channels <= 4; ++channels)
    for (guint i = 0; i < G_N_ELEMENTS (modes); ++i)
      {
        const gint mode = modes[i];
        gchar *name = xcf_max_rgb_old_name (channels, mode, ".xcf");
        GimpImage *image = open_fixture (name);
        GimpFilterLayer *filter = xcf_max_rgb_filter (image);
        GBytes *raw = xcf_max_rgb_old_bytes (channels, mode, ".definition.raw");
        GBytes *schema = xcf_max_rgb_capsule (1, mode);
        GBytes *source = xcf_max_rgb_old_bytes (channels, mode, "-source.raw");
        GBytes *output = xcf_max_rgb_old_bytes (channels, mode, "-output.raw");
        GBytes *source2 = xcf_max_rgb_old_bytes (channels, mode, "-source2.raw");
        GBytes *output2 = xcf_max_rgb_old_bytes (channels, mode, "-output2.raw");
        GimpItem *copy;
        GError *error = NULL;
        g_test_message ("Max RGB old-writer case %s", name);
        /* Every literal, tag, payload length and signed bit pattern must
         * agree with the independently constructed strict admission schema. */
        g_assert_true (g_bytes_equal (raw, schema)); g_bytes_unref (schema);
        g_assert_false (g_bytes_equal (source, source2));
        xcf_max_rgb_loaded (filter); xcf_max_rgb_parameters (filter, 1, mode);
        xcf_max_rgb_raw_equal (filter, raw); xcf_max_rgb_pixels (GIMP_LAYER (filter), output, 4);
        copy = gimp_item_duplicate (GIMP_ITEM (filter), GIMP_TYPE_FILTER_LAYER);
        g_assert_nonnull (copy); g_object_ref_sink (copy);
        xcf_max_rgb_loaded (GIMP_FILTER_LAYER (copy));
        xcf_max_rgb_parameters (GIMP_FILTER_LAYER (copy), 1, mode);
        xcf_max_rgb_raw_equal (GIMP_FILTER_LAYER (copy), raw);
        xcf_max_rgb_pixels (GIMP_LAYER (copy), output, 4); g_object_unref (copy);
        image = xcf_max_rgb_close_reopen (image); filter = xcf_max_rgb_filter (image);
        xcf_max_rgb_loaded (filter); xcf_max_rgb_parameters (filter, 1, mode);
        xcf_max_rgb_raw_equal (filter, raw); xcf_max_rgb_pixels (GIMP_LAYER (filter), output, 4);
        xcf_max_rgb_lower_input (image, channels, source2);
        xcf_max_rgb_settle (filter, GIMP_FILTER_LAYER_CLEAN);
        g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 1);
        xcf_max_rgb_pixels (GIMP_LAYER (filter), output2, 4); xcf_max_rgb_raw_equal (filter, raw);
        /* An unchanged-byte native notification is an additional native
         * lifecycle control; its expected pixels remain the observed output2. */
        xcf_max_rgb_lower_input (image, channels, source2);
        xcf_max_rgb_settle (filter, GIMP_FILTER_LAYER_CLEAN);
        g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 2);
        xcf_max_rgb_pixels (GIMP_LAYER (filter), output2, 4);
        image = xcf_max_rgb_close_reopen (image); filter = xcf_max_rgb_filter (image);
        xcf_max_rgb_loaded (filter); xcf_max_rgb_parameters (filter, 1, mode);
        xcf_max_rgb_raw_equal (filter, raw); xcf_max_rgb_pixels (GIMP_LAYER (filter), output2, 4);
        {
          const gint edited_mode = mode > 0 ? 0 : 1;
          GimpValueArray *edited = gimp_filter_layer_dup_args (filter);
          GBytes *changed = xcf_max_rgb_old_bytes (channels, edited_mode, ".definition.raw");
          GBytes *edited_source = xcf_max_rgb_old_bytes (channels, edited_mode, "-source2.raw");
          GBytes *edited_output = xcf_max_rgb_old_bytes (channels, edited_mode, "-output2.raw");
          g_assert_true (g_bytes_equal (source2, edited_source));
          g_value_set_int (gimp_value_array_index (edited, 3), edited_mode);
          g_assert_true (gimp_filter_layer_edit_definition (filter, "plug-in-max-rgb", changed, edited, &error));
          g_assert_no_error (error); gimp_value_array_unref (edited);
          xcf_max_rgb_parameters (filter, 1, edited_mode); xcf_max_rgb_raw_equal (filter, changed);
          g_assert_true (gimp_image_undo (image));
          xcf_max_rgb_parameters (filter, 1, mode); xcf_max_rgb_raw_equal (filter, raw);
          xcf_max_rgb_settle (filter, GIMP_FILTER_LAYER_CLEAN);
          xcf_max_rgb_pixels (GIMP_LAYER (filter), output2, 4);
          g_assert_true (gimp_image_redo (image));
          xcf_max_rgb_parameters (filter, 1, edited_mode); xcf_max_rgb_raw_equal (filter, changed);
          xcf_max_rgb_settle (filter, GIMP_FILTER_LAYER_CLEAN);
          xcf_max_rgb_pixels (GIMP_LAYER (filter), edited_output, 4);
          image = xcf_max_rgb_close_reopen (image); filter = xcf_max_rgb_filter (image);
          xcf_max_rgb_loaded (filter); xcf_max_rgb_parameters (filter, 1, edited_mode);
          xcf_max_rgb_raw_equal (filter, changed); xcf_max_rgb_pixels (GIMP_LAYER (filter), edited_output, 4);
          g_bytes_unref (changed); g_bytes_unref (edited_source); g_bytes_unref (edited_output);
        }
        g_object_unref (image); g_free (name); g_bytes_unref (raw);
        g_bytes_unref (source); g_bytes_unref (source2); g_bytes_unref (output); g_bytes_unref (output2);
      }
}
