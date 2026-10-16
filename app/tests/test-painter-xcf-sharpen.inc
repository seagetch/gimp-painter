/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Genuine pinned old-writer Open -> Save -> Close -> Open, followed by native
 * rerun. Malformed variants below are explicitly manufactured from a genuine
 * file; they must retain their raw capsule and loaded cache without execution. */
static gchar *
xcf_sharpen_name (guint channels, gint percent, const gchar *suffix)
{
  return g_strdup_printf ("legacy-sharpen-context/live-c%u-17x13-p%d-s0-f-1-a%u-l0%s",
                           channels, percent, channels <= 2 ? 3 : 15, suffix);
}

static GBytes *
xcf_sharpen_bytes (const gchar *relative)
{
  gchar *path = fixture_path (relative), *data;
  gsize size;
  GError *error = NULL;
  g_assert_true (g_file_get_contents (path, &data, &size, &error));
  g_assert_no_error (error); g_free (path);
  return g_bytes_new_take (data, size);
}

static GBytes *
xcf_sharpen_case_bytes (guint channels, gint percent, const gchar *suffix)
{
  gchar *name = xcf_sharpen_name (channels, percent, suffix);
  GBytes *bytes = xcf_sharpen_bytes (name);
  g_free (name); return bytes;
}

static void
xcf_sharpen_raw_equal (GimpFilterLayer *filter, GBytes *expected)
{
  GBytes *actual = gimp_filter_layer_ref_definition (filter);
  g_assert_nonnull (actual); g_assert_true (g_bytes_equal (actual, expected));
  g_bytes_unref (actual);
}

static void
xcf_sharpen_parameter (GimpFilterLayer *filter, gint percent)
{
  GimpValueArray *args = gimp_filter_layer_dup_args (filter);
  g_assert_nonnull (args);
  g_assert_cmpuint (gimp_value_array_length (args), ==, 4);
  for (guint i = 0; i < 4; ++i)
    g_assert_true (G_VALUE_HOLDS_INT (gimp_value_array_index (args, i)));
  g_assert_cmpint (g_value_get_int (gimp_value_array_index (args, 0)), ==, 1);
  g_assert_cmpint (g_value_get_int (gimp_value_array_index (args, 3)), ==, percent);
  gimp_value_array_unref (args);
}

static void
xcf_sharpen_pixels (GimpLayer *layer, GBytes *expected, guint channels)
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
xcf_sharpen_settle (GimpFilterLayer *filter, GimpFilterLayerState expected)
{
  gint64 deadline = g_get_monotonic_time () + 20 * G_TIME_SPAN_SECOND;
  while (gimp_filter_layer_get_state (filter) != expected &&
         g_get_monotonic_time () < deadline)
    {
      GimpFilterLayerState state = gimp_filter_layer_get_state (filter);
      if (state == GIMP_FILTER_LAYER_FAILED && expected != GIMP_FILTER_LAYER_FAILED)
        { gchar *error = gimp_filter_layer_dup_error (filter); g_error ("Sharpen XCF rerun failed: %s", error); }
      g_main_context_iteration (NULL, FALSE); g_usleep (100);
    }
  g_assert_cmpint (gimp_filter_layer_get_state (filter), ==, expected);
}

static void
xcf_sharpen_lower_input (GimpImage *image, guint channels, GBytes *bytes)
{
  GimpLayer *source = find_layer (image, "sharpen source");
  const gchar *formats[] = {"Y' u8", "Y'A u8", "R'G'B' u8", "R'G'B'A u8"};
  g_assert_nonnull (source);
  g_assert_cmpuint (g_bytes_get_size (bytes), ==, 17 * 13 * channels);
  gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (source)), NULL, 0,
                   babl_format (formats[channels - 1]), g_bytes_get_data (bytes, NULL), GEGL_AUTO_ROWSTRIDE);
  gimp_drawable_update (GIMP_DRAWABLE (source), 0, 0, 17, 13);
}

static GFile *
xcf_sharpen_save (GimpImage *image)
{
  GError *error = NULL;
  GFileIOStream *io;
  GFile *file = g_file_new_tmp ("painter-sharpen-roundtrip-XXXXXX", &io, &error);
  GimpPlugInProcedure *procedure = GIMP_PLUG_IN_PROCEDURE (gimp_pdb_lookup_procedure (gimp->pdb, "gimp-xcf-save"));
  g_assert_no_error (error); g_assert_nonnull (file); g_assert_nonnull (procedure);
  g_assert_true (g_io_stream_close (G_IO_STREAM (io), NULL, &error));
  g_assert_no_error (error); g_object_unref (io);
  g_assert_cmpint (file_save (gimp, image, NULL, file, procedure, GIMP_RUN_NONINTERACTIVE,
                              FALSE, FALSE, FALSE, &error), ==, GIMP_PDB_SUCCESS);
  g_assert_no_error (error); return file;
}

static void
sharpen_old_writer_roundtrip (void)
{
  static const gint percentages[] = {10, 0, -100, 99, G_MAXINT};
  for (guint channels = 1; channels <= 4; ++channels)
    for (guint p = 0; p < G_N_ELEMENTS (percentages); ++p)
      {
        const gint percent = percentages[p];
        const guint output_channels = channels <= 2 ? 2 : 4;
        gchar *name = xcf_sharpen_name (channels, percent, ".xcf");
        GimpImage *image = open_fixture (name);
        GimpFilterLayer *filter = GIMP_FILTER_LAYER (find_layer (image, "sharpen filter"));
        GBytes *raw = xcf_sharpen_case_bytes (channels, percent, ".definition.raw");
        GBytes *output = xcf_sharpen_case_bytes (channels, percent, "-output.raw");
        GBytes *source2 = xcf_sharpen_case_bytes (channels, percent, "-source2.raw");
        GBytes *output2 = xcf_sharpen_case_bytes (channels, percent, "-output2.raw");
        GimpItem *copy;
        GFile *file;
        GError *error = NULL;
        g_assert_nonnull (filter); g_assert_true (GIMP_IS_FILTER_LAYER (filter));
        g_assert_cmpint (gimp_filter_layer_get_state (filter), ==, GIMP_FILTER_LAYER_CLEAN);
        g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 0);
        g_assert_cmpuint (g_bytes_get_size (raw), ==, 68);
        xcf_sharpen_parameter (filter, percent); xcf_sharpen_raw_equal (filter, raw);
        xcf_sharpen_pixels (GIMP_LAYER (filter), output, output_channels);
        copy = gimp_item_duplicate (GIMP_ITEM (filter), GIMP_TYPE_FILTER_LAYER);
        g_assert_nonnull (copy); g_object_ref_sink (copy);
        xcf_sharpen_parameter (GIMP_FILTER_LAYER (copy), percent);
        xcf_sharpen_raw_equal (GIMP_FILTER_LAYER (copy), raw);
        xcf_sharpen_pixels (GIMP_LAYER (copy), output, output_channels);
        g_object_unref (copy);
        file = xcf_sharpen_save (image);
        g_object_unref (image); /* Close the entire first native image epoch. */
        image = open_file (file, NULL, NULL, &error);
        g_assert_no_error (error); g_assert_nonnull (image);
        filter = GIMP_FILTER_LAYER (find_layer (image, "sharpen filter"));
        g_assert_nonnull (filter);
        g_assert_cmpint (gimp_filter_layer_get_state (filter), ==, GIMP_FILTER_LAYER_CLEAN);
        g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 0);
        xcf_sharpen_parameter (filter, percent); xcf_sharpen_raw_equal (filter, raw);
        xcf_sharpen_pixels (GIMP_LAYER (filter), output, output_channels);
        xcf_sharpen_lower_input (image, channels, source2);
        xcf_sharpen_settle (filter, GIMP_FILTER_LAYER_CLEAN);
        g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 1);
        xcf_sharpen_pixels (GIMP_LAYER (filter), output2, output_channels);
        xcf_sharpen_raw_equal (filter, raw);
        /* Definition Undo/Redo carries both scalar value and original capsule. */
        {
          const gint edited_percent = percent == 99 ? 10 : 99;
          GimpValueArray *edited = gimp_filter_layer_dup_args (filter);
          GBytes *edited_output = xcf_sharpen_case_bytes (channels, edited_percent, "-output2.raw");
          gsize size;
          const guint8 *old = g_bytes_get_data (raw, &size);
          guint8 *bytes = g_memdup2 (old, size);
          GBytes *changed;
          put32 (bytes, 56, edited_percent); changed = g_bytes_new_take (bytes, size);
          g_value_set_int (gimp_value_array_index (edited, 3), edited_percent);
          g_assert_true (gimp_filter_layer_edit_definition (filter, "plug-in-sharpen", changed, edited, &error));
          g_assert_no_error (error); gimp_value_array_unref (edited);
          xcf_sharpen_parameter (filter, edited_percent); xcf_sharpen_raw_equal (filter, changed);
          g_assert_true (gimp_image_undo (image));
          xcf_sharpen_parameter (filter, percent); xcf_sharpen_raw_equal (filter, raw);
          xcf_sharpen_settle (filter, GIMP_FILTER_LAYER_CLEAN);
          xcf_sharpen_pixels (GIMP_LAYER (filter), output2, output_channels);
          g_assert_true (gimp_image_redo (image));
          xcf_sharpen_parameter (filter, edited_percent); xcf_sharpen_raw_equal (filter, changed);
          xcf_sharpen_settle (filter, GIMP_FILTER_LAYER_CLEAN);
          xcf_sharpen_pixels (GIMP_LAYER (filter), edited_output, output_channels);
          g_bytes_unref (edited_output); g_bytes_unref (changed);
        }
        g_assert_true (g_file_delete (file, NULL, &error)); g_assert_no_error (error);
        g_object_unref (file); g_object_unref (image); g_free (name);
        g_bytes_unref (raw); g_bytes_unref (output); g_bytes_unref (source2); g_bytes_unref (output2);
      }
}

static void
sharpen_malformed_writer_capsules (void)
{
  GBytes *original = xcf_sharpen_case_bytes (4, 10, ".xcf");
  GBytes *capsule = xcf_sharpen_case_bytes (4, 10, ".definition.raw");
  GBytes *output = xcf_sharpen_case_bytes (4, 10, "-output.raw");
  GBytes *source2 = xcf_sharpen_case_bytes (4, 10, "-source2.raw");
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
        case 0: put32 (bytes, offset + 20, 5); break; /* float run mode */
        case 1: put32 (bytes, offset + 32, 10); break; /* wrong image kind */
        case 2: put32 (bytes, offset + 40, 1); break; /* wrong drawable kind */
        case 3: put32 (bytes, offset + 48, 5); break; /* float percent */
        case 4: put32 (bytes, offset + 52, 3); break; /* percent length mismatch */
        case 5: put32 (bytes, offset + 64, 1); break; /* END length mismatch */
        case 6: bytes[offset + 19] = 'x'; break; /* missing name NUL */
        case 7: bytes[offset + 12] = 0; break; /* embedded name NUL */
        case 8: put32 (bytes, offset - 4, 67); break; /* outer length mismatch */
        case 9:
          /* Five omitted arguments plus END fit the same 68-byte capsule. */
          for (guint i = 0; i < 5; ++i)
            { put32 (bytes, offset + 20 + i * 8, 1); put32 (bytes, offset + 24 + i * 8, 0); }
          break;
        case 10:
          /* Three arguments, then END. The remaining twelve bytes become a
           * valid redundant VISIBLE layer property, so no file offsets move. */
          put32 (bytes, offset - 4, 56);
          put32 (bytes, offset + 48, 0); put32 (bytes, offset + 52, 0);
          put32 (bytes, offset + 56, 8); put32 (bytes, offset + 60, 4);
          put32 (bytes, offset + 64, 1); retained_size = 56;
          break;
        }
      image = open_bytes (bytes, size, XCF_PAINTER_DIALECT_LEGACY, &error);
      g_assert_no_error (error); g_assert_nonnull (image);
      filter = GIMP_FILTER_LAYER (find_layer (image, "sharpen filter"));
      g_assert_nonnull (filter); g_assert_null (gimp_filter_layer_dup_args (filter));
      expected_raw = g_bytes_new (bytes + offset, retained_size);
      xcf_sharpen_raw_equal (filter, expected_raw);
      xcf_sharpen_pixels (GIMP_LAYER (filter), output, 4);
      xcf_sharpen_lower_input (image, 4, source2);
      xcf_sharpen_settle (filter, GIMP_FILTER_LAYER_FAILED);
      xcf_sharpen_raw_equal (filter, expected_raw);
      xcf_sharpen_pixels (GIMP_LAYER (filter), output, 4);
      g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 0);
      g_object_unref (image); g_bytes_unref (expected_raw); g_free (bytes);
    }
  g_bytes_unref (original); g_bytes_unref (capsule); g_bytes_unref (output); g_bytes_unref (source2);
}
