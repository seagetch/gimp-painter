/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Genuine pinned old-writer Open -> Save -> Close -> Open, followed by native
 * rerun. Malformed variants below are explicitly manufactured from a genuine
 * file; they must retain their raw capsule and loaded cache without execution. */
static gchar *
xcf_threshold_name (guint channels, gint threshold, const gchar *suffix)
{
  return g_strdup_printf ("legacy-threshold-alpha-context/live-c%u-17x13-t%d-s0-f-1-a%u-l0%s",
                           channels, threshold, channels <= 2 ? 3 : 15, suffix);
}

static GBytes *
xcf_threshold_bytes (const gchar *relative)
{
  gchar *path = fixture_path (relative), *data;
  gsize size;
  GError *error = NULL;
  g_assert_true (g_file_get_contents (path, &data, &size, &error));
  g_assert_no_error (error); g_free (path);
  return g_bytes_new_take (data, size);
}

static GBytes *
xcf_threshold_case_bytes (guint channels, gint threshold, const gchar *suffix)
{
  gchar *name = xcf_threshold_name (channels, threshold, suffix);
  GBytes *bytes = xcf_threshold_bytes (name);
  g_free (name); return bytes;
}

static void
xcf_threshold_raw_equal (GimpFilterLayer *filter, GBytes *expected)
{
  GBytes *actual = gimp_filter_layer_ref_definition (filter);
  g_assert_nonnull (actual); g_assert_true (g_bytes_equal (actual, expected));
  g_bytes_unref (actual);
}

static void
xcf_threshold_parameter (GimpFilterLayer *filter, gint threshold)
{
  GimpValueArray *args = gimp_filter_layer_dup_args (filter);
  g_assert_nonnull (args);
  g_assert_cmpuint (gimp_value_array_length (args), ==, 4);
  for (guint i = 0; i < 4; ++i)
    g_assert_true (G_VALUE_HOLDS_INT (gimp_value_array_index (args, i)));
  g_assert_cmpint (g_value_get_int (gimp_value_array_index (args, 0)), ==, 1);
  g_assert_cmpint (g_value_get_int (gimp_value_array_index (args, 3)), ==, threshold);
  gimp_value_array_unref (args);
}

static void
xcf_threshold_pixels (GimpLayer *layer, GBytes *expected, guint channels)
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
xcf_threshold_settle (GimpFilterLayer *filter, GimpFilterLayerState expected)
{
  gint64 deadline = g_get_monotonic_time () + 20 * G_TIME_SPAN_SECOND;
  while (gimp_filter_layer_get_state (filter) != expected &&
         g_get_monotonic_time () < deadline)
    {
      GimpFilterLayerState state = gimp_filter_layer_get_state (filter);
      if (state == GIMP_FILTER_LAYER_FAILED && expected != GIMP_FILTER_LAYER_FAILED)
        { gchar *error = gimp_filter_layer_dup_error (filter); g_error ("Threshold Alpha XCF rerun failed: %s", error); }
      g_main_context_iteration (NULL, FALSE); g_usleep (100);
    }
  g_assert_cmpint (gimp_filter_layer_get_state (filter), ==, expected);
}

static void
xcf_threshold_lower_input (GimpImage *image, guint channels, GBytes *bytes)
{
  GimpLayer *source = find_layer (image, "threshold-alpha source");
  const gchar *formats[] = {"Y' u8", "Y'A u8", "R'G'B' u8", "R'G'B'A u8"};
  g_assert_nonnull (source);
  g_assert_cmpuint (g_bytes_get_size (bytes), ==, 17 * 13 * channels);
  gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (source)), NULL, 0,
                   babl_format (formats[channels - 1]), g_bytes_get_data (bytes, NULL), GEGL_AUTO_ROWSTRIDE);
  gimp_drawable_update (GIMP_DRAWABLE (source), 0, 0, 17, 13);
}

static GFile *
xcf_threshold_save (GimpImage *image)
{
  GError *error = NULL;
  GFileIOStream *io;
  GFile *file = g_file_new_tmp ("painter-threshold-roundtrip-XXXXXX", &io, &error);
  GimpPlugInProcedure *procedure = GIMP_PLUG_IN_PROCEDURE (gimp_pdb_lookup_procedure (gimp->pdb, "gimp-xcf-save"));
  g_assert_no_error (error); g_assert_nonnull (file); g_assert_nonnull (procedure);
  g_assert_true (g_io_stream_close (G_IO_STREAM (io), NULL, &error));
  g_assert_no_error (error); g_object_unref (io);
  g_assert_cmpint (file_save (gimp, image, NULL, file, procedure, GIMP_RUN_NONINTERACTIVE,
                              FALSE, FALSE, FALSE, &error), ==, GIMP_PDB_SUCCESS);
  g_assert_no_error (error); return file;
}

static void
threshold_old_writer_roundtrip (void)
{
  static const gint thresholds[] = {G_MININT, -1, 0, 127, 255, 256, G_MAXINT};
  for (guint channels = 1; channels <= 4; ++channels)
    for (guint p = 0; p < G_N_ELEMENTS (thresholds); ++p)
      {
        const gint threshold = thresholds[p];
        const guint output_channels = channels <= 2 ? 2 : 4;
        gchar *name = xcf_threshold_name (channels, threshold, ".xcf");
        GimpImage *image = open_fixture (name);
        GimpFilterLayer *filter = GIMP_FILTER_LAYER (find_layer (image, "threshold-alpha filter"));
        GBytes *raw = xcf_threshold_case_bytes (channels, threshold, ".definition.raw");
        GBytes *output = xcf_threshold_case_bytes (channels, threshold, "-output.raw");
        GBytes *source2 = xcf_threshold_case_bytes (channels, threshold, "-source2.raw");
        GBytes *output2 = xcf_threshold_case_bytes (channels, threshold, "-output2.raw");
        GimpItem *copy;
        GFile *file;
        GError *error = NULL;
        g_assert_nonnull (filter); g_assert_true (GIMP_IS_FILTER_LAYER (filter));
        g_assert_cmpint (gimp_filter_layer_get_state (filter), ==, GIMP_FILTER_LAYER_CLEAN);
        g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 0);
        g_assert_cmpuint (g_bytes_get_size (raw), ==, 76);
        xcf_threshold_parameter (filter, threshold); xcf_threshold_raw_equal (filter, raw);
        xcf_threshold_pixels (GIMP_LAYER (filter), output, output_channels);
        copy = gimp_item_duplicate (GIMP_ITEM (filter), GIMP_TYPE_FILTER_LAYER);
        g_assert_nonnull (copy); g_object_ref_sink (copy);
        xcf_threshold_parameter (GIMP_FILTER_LAYER (copy), threshold);
        xcf_threshold_raw_equal (GIMP_FILTER_LAYER (copy), raw);
        xcf_threshold_pixels (GIMP_LAYER (copy), output, output_channels);
        g_object_unref (copy);
        file = xcf_threshold_save (image);
        g_object_unref (image); /* Close the entire first native image epoch. */
        image = open_file (file, NULL, NULL, &error);
        g_assert_no_error (error); g_assert_nonnull (image);
        filter = GIMP_FILTER_LAYER (find_layer (image, "threshold-alpha filter"));
        g_assert_nonnull (filter);
        g_assert_cmpint (gimp_filter_layer_get_state (filter), ==, GIMP_FILTER_LAYER_CLEAN);
        g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 0);
        xcf_threshold_parameter (filter, threshold); xcf_threshold_raw_equal (filter, raw);
        xcf_threshold_pixels (GIMP_LAYER (filter), output, output_channels);
        xcf_threshold_lower_input (image, channels, source2);
        xcf_threshold_settle (filter, GIMP_FILTER_LAYER_CLEAN);
        g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 1);
        xcf_threshold_pixels (GIMP_LAYER (filter), output2, output_channels);
        xcf_threshold_raw_equal (filter, raw);
        /* Also cross an entire image epoch with the newly executed cache:
         * actual-old open -> native rerun -> save -> close -> reopen. */
        {
          GFile *executed_file = xcf_threshold_save (image);
          g_object_unref (image);
          image = open_file (executed_file, NULL, NULL, &error);
          g_assert_no_error (error); g_assert_nonnull (image);
          filter = GIMP_FILTER_LAYER (find_layer (image, "threshold-alpha filter"));
          g_assert_nonnull (filter);
          g_assert_cmpint (gimp_filter_layer_get_state (filter), ==, GIMP_FILTER_LAYER_CLEAN);
          g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 0);
          xcf_threshold_parameter (filter, threshold); xcf_threshold_raw_equal (filter, raw);
          xcf_threshold_pixels (GIMP_LAYER (filter), output2, output_channels);
          g_assert_true (g_file_delete (executed_file, NULL, &error)); g_assert_no_error (error);
          g_object_unref (executed_file);
        }
        /* Definition Undo/Redo carries both scalar value and original capsule. */
        {
          const gint edited_threshold = threshold == 127 ? 0 : 127;
          GimpValueArray *edited = gimp_filter_layer_dup_args (filter);
          GBytes *edited_output = xcf_threshold_case_bytes (channels, edited_threshold, "-output2.raw");
          gsize size;
          const guint8 *old = g_bytes_get_data (raw, &size);
          guint8 *bytes = g_memdup2 (old, size);
          GBytes *changed;
          put32 (bytes, 64, edited_threshold); changed = g_bytes_new_take (bytes, size);
          g_value_set_int (gimp_value_array_index (edited, 3), edited_threshold);
          g_assert_true (gimp_filter_layer_edit_definition (filter, "plug-in-threshold-alpha", changed, edited, &error));
          g_assert_no_error (error); gimp_value_array_unref (edited);
          xcf_threshold_parameter (filter, edited_threshold); xcf_threshold_raw_equal (filter, changed);
          g_assert_true (gimp_image_undo (image));
          xcf_threshold_parameter (filter, threshold); xcf_threshold_raw_equal (filter, raw);
          xcf_threshold_settle (filter, GIMP_FILTER_LAYER_CLEAN);
          xcf_threshold_pixels (GIMP_LAYER (filter), output2, output_channels);
          g_assert_true (gimp_image_redo (image));
          xcf_threshold_parameter (filter, edited_threshold); xcf_threshold_raw_equal (filter, changed);
          xcf_threshold_settle (filter, GIMP_FILTER_LAYER_CLEAN);
          xcf_threshold_pixels (GIMP_LAYER (filter), edited_output, output_channels);
          g_bytes_unref (edited_output); g_bytes_unref (changed);
        }
        g_assert_true (g_file_delete (file, NULL, &error)); g_assert_no_error (error);
        g_object_unref (file); g_object_unref (image); g_free (name);
        g_bytes_unref (raw); g_bytes_unref (output); g_bytes_unref (source2); g_bytes_unref (output2);
      }
}

static void
threshold_malformed_writer_capsules (void)
{
  GBytes *original = xcf_threshold_case_bytes (4, 127, ".xcf");
  GBytes *capsule = xcf_threshold_case_bytes (4, 127, ".definition.raw");
  GBytes *output = xcf_threshold_case_bytes (4, 127, "-output.raw");
  GBytes *source2 = xcf_threshold_case_bytes (4, 127, "-source2.raw");
  gsize size, raw_size, offset = 0;
  const guint8 *data = g_bytes_get_data (original, &size);
  const guint8 *raw = g_bytes_get_data (capsule, &raw_size);
  for (gsize i = 8; i + raw_size <= size; ++i)
    if (!memcmp (data + i, raw, raw_size)) { g_assert_cmpuint (offset, ==, 0); offset = i; }
  g_assert_cmpuint (offset, >, 8);
  for (guint mutation = 0; mutation < 11; ++mutation)
    {
      guint8 *bytes = g_memdup2 (data, size);
      GimpImage *image;
      GimpFilterLayer *filter;
      GBytes *expected_raw;
      GError *error = NULL;
      gsize retained_size = raw_size;
      /* Every variant preserves the following layer-property stream and the
       * absolute pixel offsets. These manufactured files test rejection; they
       * are not attributed to the original writer. */
      switch (mutation)
        {
        case 0: put32 (bytes, offset + 28, 5); break; /* float run mode */
        case 1: put32 (bytes, offset + 40, 10); break; /* wrong image kind */
        case 2: put32 (bytes, offset + 48, 1); break; /* wrong drawable kind */
        case 3: put32 (bytes, offset + 56, 5); break; /* float threshold */
        case 4: put32 (bytes, offset + 60, 3); break; /* threshold length mismatch */
        case 5: put32 (bytes, offset + 72, 1); break; /* END length mismatch */
        case 6: bytes[offset + 27] = 'x'; break; /* missing name NUL */
        case 7: bytes[offset + 20] = 0; break; /* embedded name NUL */
        case 8: put32 (bytes, offset - 4, 75); break; /* outer length mismatch */
        case 9:
          /* Five omitted arguments plus END fit the same 76-byte capsule. */
          for (guint i = 0; i < 5; ++i)
            { put32 (bytes, offset + 28 + i * 8, 1); put32 (bytes, offset + 32 + i * 8, 0); }
          break;
        case 10:
          /* Three arguments, then END. The remaining twelve bytes become a
           * valid redundant VISIBLE layer property, so no file offsets move. */
          put32 (bytes, offset - 4, 64);
          put32 (bytes, offset + 56, 0); put32 (bytes, offset + 60, 0);
          put32 (bytes, offset + 64, 8); put32 (bytes, offset + 68, 4);
          put32 (bytes, offset + 72, 1); retained_size = 64;
          break;
        }
      image = open_bytes (bytes, size, XCF_PAINTER_DIALECT_LEGACY, &error);
      g_assert_no_error (error); g_assert_nonnull (image);
      filter = GIMP_FILTER_LAYER (find_layer (image, "threshold-alpha filter"));
      g_assert_nonnull (filter); g_assert_null (gimp_filter_layer_dup_args (filter));
      expected_raw = g_bytes_new (bytes + offset, retained_size);
      xcf_threshold_raw_equal (filter, expected_raw);
      xcf_threshold_pixels (GIMP_LAYER (filter), output, 4);
      xcf_threshold_lower_input (image, 4, source2);
      xcf_threshold_settle (filter, GIMP_FILTER_LAYER_FAILED);
      xcf_threshold_raw_equal (filter, expected_raw);
      xcf_threshold_pixels (GIMP_LAYER (filter), output, 4);
      g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 0);
      g_object_unref (image); g_bytes_unref (expected_raw); g_free (bytes);
    }
  g_bytes_unref (original); g_bytes_unref (capsule); g_bytes_unref (output); g_bytes_unref (source2);
}

/* Native persistence/lifetime test, not an additional genuine-old pixel
 * observation. Use the observable vector RUNNING phase and spill IMPORTING
 * phase, keeping the completed cache private across save and a new image epoch. */
static void
threshold_active_save_close_reopen (void)
{
  for (guint gray = 0; gray < 2; ++gray)
    for (guint phase = 0; phase < 2; ++phase)
      {
        /* Vector launch returns from its owner quantum in RUNNING even if
         * the worker is already done. Spool processing may skip that state;
         * use its separately observable, chunked IMPORTING phase instead. */
        const gint width = phase ? 1031 : 63, height = phase ? 1021 : 61;
        const guint channels = gray ? 2 : 4;
        const GimpFilterLayerState state = phase ? GIMP_FILTER_LAYER_IMPORTING : GIMP_FILTER_LAYER_RUNNING;
        const Babl *format = babl_format (gray ? "Y'A u8" : "R'G'B'A u8");
        const guint8 rgb_input[] = {103, 71, 199, 128}, gray_input[] = {103, 128};
        const guint8 rgb_cache[] = {13, 29, 47, 255}, gray_cache[] = {13, 255};
        const guint8 *input = gray ? gray_input : rgb_input, *cached = gray ? gray_cache : rgb_cache;
        guint8 expected[4], actual[4];
        GimpImage *image = gimp_image_new (gimp, width, height, gray ? GIMP_GRAY : GIMP_RGB,
                                          GIMP_PRECISION_U8_NON_LINEAR);
        GimpLayer *source = gimp_layer_new (image, width, height, format, "threshold active source",
                                           1, GIMP_LAYER_MODE_NORMAL_LEGACY);
        GimpFilterLayer *filter = GIMP_FILTER_LAYER (gimp_filter_layer_new (
          image, width, height, "threshold active filter", 1, GIMP_LAYER_MODE_NORMAL_LEGACY));
        GimpValueArray *args = gimp_value_array_new_from_types (NULL, G_TYPE_INT, 1,
          G_TYPE_INT, 0, G_TYPE_INT, 0, G_TYPE_INT, 127, G_TYPE_NONE);
        GBytes *raw = xcf_threshold_case_bytes (channels, 127, ".definition.raw");
        GeglColor *color = gegl_color_new (NULL);
        GimpFilterLayerSnapshot saved, loaded;
        GFile *file;
        GimpImage *copy;
        GError *error = NULL;
        gchar *original_swap = NULL, *private_swap = g_dir_make_tmp ("threshold-xcf-active-XXXXXX", &error);
        g_assert_no_error (error); g_assert_nonnull (private_swap);
        g_object_get (gegl_config (), "swap", &original_swap, NULL);
        g_object_set (gegl_config (), "swap", private_swap, NULL);
        gimp_image_add_layer (image, source, NULL, 0, FALSE);
        gimp_image_add_layer (image, GIMP_LAYER (filter), NULL, 0, FALSE);
        gegl_color_set_pixel (color, format, input);
        gegl_buffer_set_color (gimp_drawable_get_buffer (GIMP_DRAWABLE (source)), NULL, color);
        gegl_color_set_pixel (color, format, cached);
        gegl_buffer_set_color (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)), NULL, color);
        g_object_unref (color);
        g_assert_true (gimp_filter_layer_set_definition (filter, "plug-in-threshold-alpha", raw, args, &error));
        g_assert_no_error (error);
        xcf_threshold_settle (filter, state);
        g_assert_true (gimp_filter_layer_get_snapshot_state (filter, &saved));
        g_assert_cmpuint (saved.generation, >, saved.cache_generation);
        file = xcf_threshold_save (image);
        copy = open_file (file, NULL, NULL, &error);
        g_assert_no_error (error); g_assert_nonnull (copy);
        filter = GIMP_FILTER_LAYER (find_layer (copy, "threshold active filter"));
        g_assert_nonnull (filter); xcf_threshold_parameter (filter, 127); xcf_threshold_raw_equal (filter, raw);
        g_assert_true (gimp_filter_layer_get_snapshot_state (filter, &loaded));
        g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 0);
        g_assert_cmpuint (loaded.generation, >, loaded.cache_generation);
        g_assert_cmpint (loaded.state, !=, GIMP_FILTER_LAYER_RUNNING);
        g_assert_cmpint (loaded.state, !=, GIMP_FILTER_LAYER_IMPORTING);
        gegl_buffer_get (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)), GEGL_RECTANGLE (0, 0, 1, 1),
                         1, format, actual, GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);
        g_assert_cmpmem (actual, channels, cached, channels);
        g_object_unref (image);
        xcf_threshold_settle (filter, GIMP_FILTER_LAYER_CLEAN);
        g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 1);
        memcpy (expected, input, channels); expected[channels - 1] = 255;
        gegl_buffer_get (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)), GEGL_RECTANGLE (0, 0, 1, 1),
                         1, format, actual, GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);
        g_assert_cmpmem (actual, channels, expected, channels);
        xcf_threshold_raw_equal (filter, raw);
        {
          GFile *complete = xcf_threshold_save (copy);
          g_object_unref (copy); copy = open_file (complete, NULL, NULL, &error);
          g_assert_no_error (error); g_assert_nonnull (copy);
          filter = GIMP_FILTER_LAYER (find_layer (copy, "threshold active filter"));
          g_assert_nonnull (filter); xcf_threshold_parameter (filter, 127); xcf_threshold_raw_equal (filter, raw);
          g_assert_cmpint (gimp_filter_layer_get_state (filter), ==, GIMP_FILTER_LAYER_CLEAN);
          g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 0);
          gegl_buffer_get (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)), GEGL_RECTANGLE (0, 0, 1, 1),
                           1, format, actual, GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);
          g_assert_cmpmem (actual, channels, expected, channels);
          g_assert_true (g_file_delete (complete, NULL, &error)); g_assert_no_error (error);
          g_object_unref (complete);
        }
        g_object_unref (copy); g_bytes_unref (raw); gimp_value_array_unref (args);
        g_assert_true (g_file_delete (file, NULL, &error)); g_assert_no_error (error); g_object_unref (file);
        g_object_set (gegl_config (), "swap", original_swap, NULL); g_free (original_swap);
        { GFile *directory = g_file_new_for_path (private_swap); g_file_delete (directory, NULL, NULL); g_object_unref (directory); }
        g_free (private_swap);
      }
}
