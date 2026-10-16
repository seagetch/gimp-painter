/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Consume genuine old-writer files and observed output bytes, never compute
 * expected Sobel output with the port. The capture table and 90-byte schema
 * remain pending old-runtime observation until the separate corpus is sealed.
 * Manufactured malformed variants below are explicitly preserve-only tests. */
typedef struct
{
  gchar *id;
  guint source_channels, output_channels;
  gint run_mode, flags[3];
  guint rerun;
} XcfSobelCase;

static void
xcf_sobel_case_free (gpointer data)
{
  XcfSobelCase *test = data;
  g_free (test->id); g_free (test);
}

static gint
xcf_sobel_integer (const gchar *text)
{
  gint64 value;
  GError *error = NULL;
  g_assert_true (g_ascii_string_to_signed (text, 10, G_MININT32, G_MAXINT32,
                                          &value, &error));
  g_assert_no_error (error);
  return (gint) value;
}

static GPtrArray *
xcf_sobel_cases (void)
{
  gchar *path = fixture_path ("legacy-sobel-context/writer-cases.tsv");
  gchar *contents, **lines;
  GPtrArray *cases = g_ptr_array_new_with_free_func (xcf_sobel_case_free);
  GHashTable *ids = g_hash_table_new (g_str_hash, g_str_equal);
  guint canonical[4] = {0, 0, 0, 0};
  GError *error = NULL;
  g_assert_true (g_file_get_contents (path, &contents, NULL, &error));
  g_assert_no_error (error); g_free (path);
  lines = g_strsplit (contents, "\n", -1);
  g_assert_cmpstr (lines[0], ==,
    "id\tsource_channels\toutput_channels\trun_mode\thorizontal\tvertical\tkeep_sign\trerun");
  for (guint row = 1; lines[row]; ++row)
    {
      XcfSobelCase *test;
      gchar **cols;
      if (!*lines[row]) continue;
      cols = g_strsplit (lines[row], "\t", -1);
      g_assert_cmpuint (g_strv_length (cols), ==, 8);
      test = g_new0 (XcfSobelCase, 1);
      test->id = g_strdup (cols[0]);
      g_assert_true (*test->id != '\0');
      g_assert_null (strchr (test->id, '/'));
      g_assert_null (strstr (test->id, ".."));
      g_assert_false (g_hash_table_contains (ids, test->id));
      g_hash_table_add (ids, test->id);
      test->source_channels = xcf_sobel_integer (cols[1]);
      test->output_channels = xcf_sobel_integer (cols[2]);
      test->run_mode = xcf_sobel_integer (cols[3]);
      for (guint i = 0; i < 3; ++i) test->flags[i] = xcf_sobel_integer (cols[4 + i]);
      test->rerun = xcf_sobel_integer (cols[7]);
      g_assert_cmpuint (test->source_channels, >=, 1);
      g_assert_cmpuint (test->source_channels, <=, 4);
      g_assert_cmpuint (test->output_channels, ==, test->source_channels <= 2 ? 2 : 4);
      g_assert_true (test->rerun >= 1 && test->rerun <= 5);
      if (test->rerun == 1 &&
          (test->flags[0] == 0 || test->flags[0] == 1) &&
          (test->flags[1] == 0 || test->flags[1] == 1) &&
          (test->flags[2] == 0 || test->flags[2] == 1))
        {
          guint bit = 1U << (test->flags[0] | (test->flags[1] << 1) | (test->flags[2] << 2));
          g_assert_cmpuint (canonical[test->source_channels - 1] & bit, ==, 0);
          canonical[test->source_channels - 1] |= bit;
        }
      g_ptr_array_add (cases, test); g_strfreev (cols);
    }
  /* Required controls are test inputs, not a claimed observed corpus count. */
  for (guint c = 0; c < 4; ++c) g_assert_cmpuint (canonical[c], ==, 255);
  g_hash_table_unref (ids); g_strfreev (lines); g_free (contents);
  return cases;
}

static gchar *
xcf_sobel_name (const XcfSobelCase *test, const gchar *suffix)
{
  return g_strconcat ("legacy-sobel-context/", test->id, suffix, NULL);
}

static GBytes *
xcf_sobel_bytes (const XcfSobelCase *test, const gchar *suffix)
{
  gchar *name = xcf_sobel_name (test, suffix), *path = fixture_path (name), *data;
  gsize size;
  GError *error = NULL;
  g_assert_true (g_file_get_contents (path, &data, &size, &error));
  g_assert_no_error (error); g_free (path); g_free (name);
  return g_bytes_new_take (data, size);
}

static guint32
xcf_sobel_word (const guint8 *data, gsize offset)
{
  guint32 value;
  memcpy (&value, data + offset, sizeof value);
  return GUINT32_FROM_BE (value);
}

static void
xcf_sobel_capsule (const XcfSobelCase *test, GBytes *raw)
{
  static const guint tags[] = {2, 1, 10, 2, 2, 2, 0};
  static const guint sizes[] = {4, 0, 0, 4, 4, 4, 0};
  static const gchar literal[] = "plug-in-sobel";
  const guint8 *data = g_bytes_get_data (raw, NULL);
  gsize pos = 4 + sizeof literal;
  g_assert_cmpuint (g_bytes_get_size (raw), ==, 90);
  g_assert_cmpuint (xcf_sobel_word (data, 0), ==, sizeof literal);
  g_assert_cmpmem (data + 4, sizeof literal, literal, sizeof literal);
  for (guint i = 0; i < G_N_ELEMENTS (tags); ++i)
    {
      g_assert_cmpuint (xcf_sobel_word (data, pos), ==, tags[i]);
      g_assert_cmpuint (xcf_sobel_word (data, pos + 4), ==, sizes[i]);
      if (i == 0)
        g_assert_cmpuint (xcf_sobel_word (data, pos + 8), ==, (guint32) test->run_mode);
      else if (i >= 3 && i <= 5)
        g_assert_cmpuint (xcf_sobel_word (data, pos + 8), ==, (guint32) test->flags[i - 3]);
      pos += 8 + sizes[i];
    }
  g_assert_cmpuint (pos, ==, g_bytes_get_size (raw));
}

static GimpFilterLayer *
xcf_sobel_filter (GimpImage *image)
{
  GimpLayer *layer = find_layer (image, "sobel filter");
  g_assert_nonnull (layer); g_assert_true (GIMP_IS_FILTER_LAYER (layer));
  return GIMP_FILTER_LAYER (layer);
}

static void
xcf_sobel_raw_equal (GimpFilterLayer *filter, GBytes *expected)
{
  GBytes *actual = gimp_filter_layer_ref_definition (filter);
  g_assert_nonnull (actual); g_assert_true (g_bytes_equal (actual, expected));
  g_bytes_unref (actual);
}

static void
xcf_sobel_parameters (GimpFilterLayer *filter, gint run_mode, const gint flags[3])
{
  GimpValueArray *args = gimp_filter_layer_dup_args (filter);
  gchar *name = gimp_filter_layer_dup_procedure (filter);
  g_assert_cmpstr (name, ==, "plug-in-sobel"); g_free (name);
  g_assert_nonnull (args);
  g_assert_cmpuint (gimp_value_array_length (args), ==, 6);
  for (guint i = 0; i < 6; ++i)
    g_assert_true (G_VALUE_HOLDS_INT (gimp_value_array_index (args, i)));
  g_assert_cmpint (g_value_get_int (gimp_value_array_index (args, 0)), ==, run_mode);
  g_assert_cmpint (g_value_get_int (gimp_value_array_index (args, 1)), ==, 0);
  g_assert_cmpint (g_value_get_int (gimp_value_array_index (args, 2)), ==, 0);
  for (guint i = 0; i < 3; ++i)
    g_assert_cmpint (g_value_get_int (gimp_value_array_index (args, i + 3)), ==, flags[i]);
  gimp_value_array_unref (args);
}

static void
xcf_sobel_pixels (GimpLayer *layer, GBytes *expected, guint channels)
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
xcf_sobel_settle (GimpFilterLayer *filter, GimpFilterLayerState expected)
{
  gint64 deadline = g_get_monotonic_time () + 20 * G_TIME_SPAN_SECOND;
  while (gimp_filter_layer_get_state (filter) != expected &&
         g_get_monotonic_time () < deadline)
    {
      if (gimp_filter_layer_get_state (filter) == GIMP_FILTER_LAYER_FAILED &&
          expected != GIMP_FILTER_LAYER_FAILED)
        { gchar *error = gimp_filter_layer_dup_error (filter); g_error ("Sobel XCF rerun failed: %s", error); }
      g_main_context_iteration (NULL, FALSE); g_usleep (100);
    }
  g_assert_cmpint (gimp_filter_layer_get_state (filter), ==, expected);
}

static void
xcf_sobel_loaded (GimpFilterLayer *filter)
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
xcf_sobel_lower_input (GimpImage *image, guint channels, GBytes *bytes)
{
  GimpLayer *source = find_layer (image, "sobel source");
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
xcf_sobel_save (GimpImage *image)
{
  GError *error = NULL;
  GFileIOStream *io;
  GFile *file = g_file_new_tmp ("painter-sobel-roundtrip-XXXXXX", &io, &error);
  GimpPlugInProcedure *procedure = GIMP_PLUG_IN_PROCEDURE (gimp_pdb_lookup_procedure (gimp->pdb, "gimp-xcf-save"));
  g_assert_no_error (error); g_assert_nonnull (file); g_assert_nonnull (procedure);
  g_assert_true (g_io_stream_close (G_IO_STREAM (io), NULL, &error));
  g_assert_no_error (error); g_object_unref (io);
  g_assert_cmpint (file_save (gimp, image, NULL, file, procedure, GIMP_RUN_NONINTERACTIVE,
                              FALSE, FALSE, FALSE, &error), ==, GIMP_PDB_SUCCESS);
  g_assert_no_error (error); return file;
}

static GimpImage *
xcf_sobel_close_reopen (GimpImage *image)
{
  GFile *file = xcf_sobel_save (image);
  GError *error = NULL;
  g_object_unref (image);
  image = open_file (file, NULL, NULL, &error);
  g_assert_no_error (error); g_assert_nonnull (image);
  g_assert_true (g_file_delete (file, NULL, &error)); g_assert_no_error (error);
  g_object_unref (file); return image;
}

static const XcfSobelCase *
xcf_sobel_canonical (GPtrArray *cases, guint channels, const gint flags[3])
{
  for (guint i = 0; i < cases->len; ++i)
    {
      const XcfSobelCase *test = g_ptr_array_index (cases, i);
      if (test->rerun == 1 && test->source_channels == channels &&
          test->flags[0] == !!flags[0] && test->flags[1] == !!flags[1] && test->flags[2] == !!flags[2])
        return test;
    }
  g_assert_not_reached ();
  return NULL;
}

static void
xcf_sobel_flag_undo_redo (GimpImage *image, const XcfSobelCase *test,
                        GBytes *raw, GBytes *baseline, GBytes *expected, guint flag)
{
  GimpFilterLayer *filter = xcf_sobel_filter (image);
  GimpValueArray *edited = gimp_filter_layer_dup_args (filter);
  gint flags[3] = {test->flags[0], test->flags[1], test->flags[2]};
  gsize size;
  const guint8 *old = g_bytes_get_data (raw, &size);
  guint8 *data = g_memdup2 (old, size);
  GBytes *changed;
  GError *error = NULL;
  flags[flag] = !flags[flag];
  put32 (data, 54 + flag * 12, (guint32) flags[flag]);
  changed = g_bytes_new_take (data, size);
  g_value_set_int (gimp_value_array_index (edited, flag + 3), flags[flag]);
  g_assert_true (gimp_filter_layer_edit_definition (filter, "plug-in-sobel", changed, edited, &error));
  g_assert_no_error (error); gimp_value_array_unref (edited);
  xcf_sobel_parameters (filter, test->run_mode, flags); xcf_sobel_raw_equal (filter, changed);
  g_assert_true (gimp_image_undo (image));
  xcf_sobel_parameters (filter, test->run_mode, test->flags); xcf_sobel_raw_equal (filter, raw);
  xcf_sobel_settle (filter, GIMP_FILTER_LAYER_CLEAN);
  xcf_sobel_pixels (GIMP_LAYER (filter), baseline, test->output_channels);
  g_assert_true (gimp_image_redo (image));
  xcf_sobel_parameters (filter, test->run_mode, flags); xcf_sobel_raw_equal (filter, changed);
  xcf_sobel_settle (filter, GIMP_FILTER_LAYER_CLEAN);
  xcf_sobel_pixels (GIMP_LAYER (filter), expected, test->output_channels);
  /* Restore this exact signed baseline before independently editing another flag. */
  g_assert_true (gimp_image_undo (image));
  xcf_sobel_parameters (filter, test->run_mode, test->flags); xcf_sobel_raw_equal (filter, raw);
  xcf_sobel_settle (filter, GIMP_FILTER_LAYER_CLEAN);
  xcf_sobel_pixels (GIMP_LAYER (filter), baseline, test->output_channels);
  g_bytes_unref (changed);
}

static void
sobel_old_writer_roundtrip (void)
{
  GPtrArray *cases = xcf_sobel_cases ();
  guint changed = 0, edits[3] = {0, 0, 0};
  for (guint i = 0; i < cases->len; ++i)
    {
      const XcfSobelCase *test = g_ptr_array_index (cases, i);
      gchar *name = xcf_sobel_name (test, ".xcf");
      GimpImage *image = open_fixture (name);
      GimpFilterLayer *filter = xcf_sobel_filter (image);
      GBytes *raw = xcf_sobel_bytes (test, ".definition.raw");
      GBytes *output = xcf_sobel_bytes (test, "-output.raw");
      GBytes *output2 = xcf_sobel_bytes (test, "-output2.raw");
      GimpItem *copy;
      g_test_message ("Sobel old-writer case %s", test->id);
      xcf_sobel_capsule (test, raw); xcf_sobel_loaded (filter);
      xcf_sobel_parameters (filter, test->run_mode, test->flags);
      xcf_sobel_raw_equal (filter, raw); xcf_sobel_pixels (GIMP_LAYER (filter), output, test->output_channels);
      copy = gimp_item_duplicate (GIMP_ITEM (filter), GIMP_TYPE_FILTER_LAYER);
      g_assert_nonnull (copy); g_object_ref_sink (copy);
      xcf_sobel_loaded (GIMP_FILTER_LAYER (copy));
      xcf_sobel_parameters (GIMP_FILTER_LAYER (copy), test->run_mode, test->flags);
      xcf_sobel_raw_equal (GIMP_FILTER_LAYER (copy), raw);
      xcf_sobel_pixels (GIMP_LAYER (copy), output, test->output_channels);
      g_object_unref (copy);
      image = xcf_sobel_close_reopen (image); filter = xcf_sobel_filter (image);
      xcf_sobel_loaded (filter); xcf_sobel_parameters (filter, test->run_mode, test->flags);
      xcf_sobel_raw_equal (filter, raw); xcf_sobel_pixels (GIMP_LAYER (filter), output, test->output_channels);
      if (test->rerun <= 2)
        {
          GBytes *source = xcf_sobel_bytes (test, "-source.raw");
          GBytes *source2 = xcf_sobel_bytes (test, "-source2.raw");
          if (test->rerun == 1) g_assert_false (g_bytes_equal (source, source2));
          else g_assert_true (g_bytes_equal (source, source2));
          xcf_sobel_lower_input (image, test->source_channels, source2);
          xcf_sobel_settle (filter, GIMP_FILTER_LAYER_CLEAN);
          g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 1);
          xcf_sobel_pixels (GIMP_LAYER (filter), output2, test->output_channels);
          xcf_sobel_raw_equal (filter, raw); changed += test->rerun == 1;
          image = xcf_sobel_close_reopen (image); filter = xcf_sobel_filter (image);
          xcf_sobel_loaded (filter); xcf_sobel_parameters (filter, test->run_mode, test->flags);
          xcf_sobel_raw_equal (filter, raw); xcf_sobel_pixels (GIMP_LAYER (filter), output2, test->output_channels);
          for (guint flag = 0; flag < 3; ++flag)
            {
              gint flags[3] = {test->flags[0], test->flags[1], test->flags[2]};
              const XcfSobelCase *alternate;
              GBytes *alternate_source, *alternate_output;
              flags[flag] = !flags[flag];
              alternate = xcf_sobel_canonical (cases, test->source_channels, flags);
              alternate_source = xcf_sobel_bytes (alternate, test->rerun == 1 ? "-source2.raw" : "-source.raw");
              alternate_output = xcf_sobel_bytes (alternate, test->rerun == 1 ? "-output2.raw" : "-output.raw");
              g_assert_true (g_bytes_equal (source2, alternate_source));
              xcf_sobel_flag_undo_redo (image, test, raw, output2, alternate_output, flag);
              ++edits[flag]; g_bytes_unref (alternate_source); g_bytes_unref (alternate_output);
            }
          g_bytes_unref (source); g_bytes_unref (source2);
        }
      else
        {
          guint flag = test->rerun - 3;
          GBytes *source = xcf_sobel_bytes (test, "-source.raw");
          GBytes *source2 = xcf_sobel_bytes (test, "-source2.raw");
          /* Separate old-runtime definition-change observations retain their
           * original lower input. The capture supplies this row's output2. */
          g_assert_true (g_bytes_equal (source, source2));
          xcf_sobel_flag_undo_redo (image, test, raw, output, output2, flag);
          ++edits[flag]; g_bytes_unref (source); g_bytes_unref (source2);
        }
      g_object_unref (image); g_free (name);
      g_bytes_unref (raw); g_bytes_unref (output); g_bytes_unref (output2);
    }
  g_assert_cmpuint (changed, >, 0);
  for (guint flag = 0; flag < 3; ++flag) g_assert_cmpuint (edits[flag], >=, changed);
  g_ptr_array_unref (cases);
}

static void
sobel_malformed_writer_capsules (void)
{
  GPtrArray *cases = xcf_sobel_cases ();
  const gint flags[] = {1, 1, 1};
  const XcfSobelCase *test = xcf_sobel_canonical (cases, 4, flags);
  GBytes *original = xcf_sobel_bytes (test, ".xcf");
  GBytes *capsule = xcf_sobel_bytes (test, ".definition.raw");
  GBytes *output = xcf_sobel_bytes (test, "-output.raw");
  GBytes *source2 = xcf_sobel_bytes (test, "-source2.raw");
  gsize size, raw_size, offset = 0;
  const guint8 *data = g_bytes_get_data (original, &size);
  const guint8 *raw = g_bytes_get_data (capsule, &raw_size);
  xcf_sobel_capsule (test, capsule);
  for (gsize i = 8; i + raw_size <= size; ++i)
    if (!memcmp (data + i, raw, raw_size)) { g_assert_cmpuint (offset, ==, 0); offset = i; }
  g_assert_cmpuint (offset, >, 8);
  g_assert_cmpuint (xcf_sobel_word (data, offset - 8), ==, 32);
  g_assert_cmpuint (xcf_sobel_word (data, offset - 4), ==, raw_size);
  for (guint mutation = 0; mutation < 20; ++mutation)
    {
      guint8 *bytes = g_memdup2 (data, size);
      GimpImage *image;
      GimpFilterLayer *filter;
      GBytes *expected_raw;
      GError *error = NULL;
      gsize retained_size = raw_size;
      g_test_message ("Manufactured Sobel capsule mutation %u", mutation);
      /* Keep the subsequent layer-property stream and absolute pixel offsets
       * unchanged. These files are rejection tests, never writer observations. */
      switch (mutation)
        {
        case 0: put32 (bytes, offset + 18, 5); break; /* float run mode */
        case 1: put32 (bytes, offset + 30, 10); break; /* wrong image kind */
        case 2: put32 (bytes, offset + 38, 1); break; /* wrong drawable kind */
        case 3: put32 (bytes, offset + 46, 5); break; /* float horizontal */
        case 4: put32 (bytes, offset + 58, 5); break; /* float vertical */
        case 5: put32 (bytes, offset + 70, 5); break; /* float keep-sign */
        case 6: put32 (bytes, offset + 22, 3); break; /* run-mode length */
        case 7: put32 (bytes, offset + 34, 4); break; /* image payload */
        case 8: put32 (bytes, offset + 42, 4); break; /* drawable payload */
        case 9: put32 (bytes, offset + 50, 3); break; /* horizontal length */
        case 10: put32 (bytes, offset + 62, 3); break; /* vertical length */
        case 11: put32 (bytes, offset + 74, 3); break; /* keep-sign length */
        case 12: put32 (bytes, offset + 86, 1); break; /* END payload */
        case 13: bytes[offset + 17] = 'x'; break; /* missing name NUL */
        case 14: bytes[offset + 12] = 0; break; /* embedded name NUL */
        case 15: put32 (bytes, offset - 4, 89); break; /* short outer length */
        case 16: put32 (bytes, offset - 4, 102); break; /* bytes claimed after END */
        case 17:
          /* Eight omitted arguments plus END fit the same 90-byte capsule. */
          for (guint i = 0; i < 8; ++i)
            { put32 (bytes, offset + 18 + i * 8, 1); put32 (bytes, offset + 22 + i * 8, 0); }
          break;
        case 18:
          /* Five arguments then END; remaining bytes form a VISIBLE property. */
          put32 (bytes, offset - 4, 78);
          put32 (bytes, offset + 70, 0); put32 (bytes, offset + 74, 0);
          put32 (bytes, offset + 78, 8); put32 (bytes, offset + 82, 4);
          put32 (bytes, offset + 86, 1); retained_size = 78;
          break;
        case 19: bytes[offset + 4] = 'P'; break; /* unsupported literal name */
        }
      image = open_bytes (bytes, size, XCF_PAINTER_DIALECT_LEGACY, &error);
      g_assert_no_error (error); g_assert_nonnull (image);
      filter = xcf_sobel_filter (image);
      g_assert_null (gimp_filter_layer_dup_args (filter)); xcf_sobel_loaded (filter);
      expected_raw = g_bytes_new (bytes + offset, retained_size);
      xcf_sobel_raw_equal (filter, expected_raw); xcf_sobel_pixels (GIMP_LAYER (filter), output, 4);
      image = xcf_sobel_close_reopen (image); filter = xcf_sobel_filter (image);
      g_assert_null (gimp_filter_layer_dup_args (filter)); xcf_sobel_loaded (filter);
      xcf_sobel_raw_equal (filter, expected_raw); xcf_sobel_pixels (GIMP_LAYER (filter), output, 4);
      xcf_sobel_lower_input (image, 4, source2);
      xcf_sobel_settle (filter, GIMP_FILTER_LAYER_FAILED);
      xcf_sobel_raw_equal (filter, expected_raw); xcf_sobel_pixels (GIMP_LAYER (filter), output, 4);
      g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 0);
      image = xcf_sobel_close_reopen (image); filter = xcf_sobel_filter (image);
      g_assert_null (gimp_filter_layer_dup_args (filter));
      xcf_sobel_settle (filter, GIMP_FILTER_LAYER_FAILED);
      xcf_sobel_raw_equal (filter, expected_raw); xcf_sobel_pixels (GIMP_LAYER (filter), output, 4);
      g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 0);
      g_object_unref (image); g_bytes_unref (expected_raw); g_free (bytes);
    }
  g_bytes_unref (original); g_bytes_unref (capsule); g_bytes_unref (output); g_bytes_unref (source2);
  g_ptr_array_unref (cases);
}

static GBytes *
xcf_sobel_constant_pixels (guint width, guint height, guint channels, const guint8 *pixel)
{
  gsize size = (gsize) width * height * channels;
  guint8 *bytes = g_malloc (size);
  for (gsize i = 0; i < size; i += channels) memcpy (bytes + i, pixel, channels);
  return g_bytes_new_take (bytes, size);
}

static void
xcf_sobel_serialized_snapshot (GimpFilterLayer *filter, const GimpFilterLayerSnapshot *saved)
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

/* Synthetic native lifecycle evidence, separate from genuine-old comparisons.
 * Both disabled with keep-sign enabled has the analytic constant 127/255
 * result. Full cache comparisons detect mixing even away from a sample pixel. */
static void
sobel_active_save_close_reopen (void)
{
  GPtrArray *cases = xcf_sobel_cases ();
  const gint flags[] = {0, 0, 1};
  for (guint gray = 0; gray < 2; ++gray)
    for (guint phase = 0; phase < 2; ++phase)
      {
        /* RUNNING is observable immediately after vector launch; spill has a
         * separately observable bounded IMPORTING phase even for quick jobs. */
        const gint width = phase ? 1031 : 63, height = phase ? 1021 : 61;
        const guint channels = gray ? 2 : 4;
        const XcfSobelCase *test = xcf_sobel_canonical (cases, channels, flags);
        const GimpFilterLayerState state = phase ? GIMP_FILTER_LAYER_IMPORTING : GIMP_FILTER_LAYER_RUNNING;
        const Babl *format = babl_format (gray ? "Y'A u8" : "R'G'B'A u8");
        const guint8 rgb_input[] = {103, 71, 199, 128}, gray_input[] = {103, 128};
        const guint8 rgb_cache[] = {13, 29, 47, 255}, gray_cache[] = {13, 255};
        const guint8 rgb_result[] = {127, 127, 127, 255}, gray_result[] = {127, 255};
        const guint8 *input = gray ? gray_input : rgb_input;
        GBytes *cache = xcf_sobel_constant_pixels (width, height, channels, gray ? gray_cache : rgb_cache);
        GBytes *result = xcf_sobel_constant_pixels (width, height, channels, gray ? gray_result : rgb_result);
        GimpImage *image = gimp_image_new (gimp, width, height, gray ? GIMP_GRAY : GIMP_RGB,
                                          GIMP_PRECISION_U8_NON_LINEAR);
        GimpLayer *source = gimp_layer_new (image, width, height, format, "sobel source",
                                           1, GIMP_LAYER_MODE_NORMAL_LEGACY);
        GimpFilterLayer *filter = GIMP_FILTER_LAYER (gimp_filter_layer_new (
          image, width, height, "sobel filter", 1, GIMP_LAYER_MODE_NORMAL_LEGACY));
        GimpValueArray *args = gimp_value_array_new_from_types (NULL, G_TYPE_INT, test->run_mode,
          G_TYPE_INT, 0, G_TYPE_INT, 0, G_TYPE_INT, 0, G_TYPE_INT, 0, G_TYPE_INT, 1, G_TYPE_NONE);
        GBytes *raw = xcf_sobel_bytes (test, ".definition.raw");
        GeglColor *color = gegl_color_new (NULL);
        GeglBuffer *completed;
        GimpFilterLayerSnapshot saved, loaded, committed;
        GFile *file;
        GimpImage *copy;
        GError *error = NULL;
        gchar *original_swap = NULL, *private_swap = g_dir_make_tmp ("sobel-xcf-active-XXXXXX", &error);
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
        g_assert_true (gimp_filter_layer_set_definition (filter, "plug-in-sobel", raw, args, &error));
        g_assert_no_error (error);
        xcf_sobel_settle (filter, state);
        g_assert_true (gimp_filter_layer_get_snapshot_state (filter, &saved));
        g_assert_cmpuint (saved.generation, >, saved.cache_generation);
        g_assert_true (completed == gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)));
        xcf_sobel_pixels (GIMP_LAYER (filter), cache, channels);
        file = xcf_sobel_save (image);
        g_assert_true (completed == gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)));
        xcf_sobel_pixels (GIMP_LAYER (filter), cache, channels);
        copy = open_file (file, NULL, NULL, &error);
        g_assert_no_error (error); g_assert_nonnull (copy);
        filter = xcf_sobel_filter (copy);
        xcf_sobel_parameters (filter, test->run_mode, flags); xcf_sobel_raw_equal (filter, raw);
        xcf_sobel_serialized_snapshot (filter, &saved);
        g_assert_true (gimp_filter_layer_get_snapshot_state (filter, &loaded));
        g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 0);
        /* restore_cache() preserves the serialized freshness relationship,
         * not absolute job tokens across image epochs. A stale restored cache
         * receives cache-generation zero and a new waiting live generation. */
        g_assert_cmpint (loaded.cache_complete, ==, saved.cache_complete);
        g_assert_cmpuint (loaded.cache_generation, ==, 0);
        g_assert_cmpuint (loaded.generation, >, loaded.cache_generation);
        g_assert_cmpint (loaded.state, ==, GIMP_FILTER_LAYER_WAITING);
        g_assert_cmpint (loaded.state, !=, GIMP_FILTER_LAYER_RUNNING);
        g_assert_cmpint (loaded.state, !=, GIMP_FILTER_LAYER_IMPORTING);
        xcf_sobel_pixels (GIMP_LAYER (filter), cache, channels);
        g_object_unref (image); g_object_unref (completed);
        g_assert_cmpuint (gimp_filter_layer_get_generation (filter), ==, loaded.generation);
        xcf_sobel_settle (filter, GIMP_FILTER_LAYER_CLEAN);
        g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 1);
        g_assert_true (gimp_filter_layer_get_snapshot_state (filter, &committed));
        g_assert_cmpuint (committed.generation, ==, loaded.generation);
        g_assert_cmpuint (committed.cache_generation, ==, committed.generation);
        g_assert_true (committed.cache_complete);
        xcf_sobel_pixels (GIMP_LAYER (filter), result, channels); xcf_sobel_raw_equal (filter, raw);
        copy = xcf_sobel_close_reopen (copy); filter = xcf_sobel_filter (copy);
        xcf_sobel_serialized_snapshot (filter, &committed);
        xcf_sobel_loaded (filter); xcf_sobel_parameters (filter, test->run_mode, flags);
        xcf_sobel_raw_equal (filter, raw); xcf_sobel_pixels (GIMP_LAYER (filter), result, channels);
        g_object_unref (copy); g_bytes_unref (raw); gimp_value_array_unref (args);
        g_bytes_unref (cache); g_bytes_unref (result);
        g_assert_true (g_file_delete (file, NULL, &error)); g_assert_no_error (error); g_object_unref (file);
        g_object_set (gegl_config (), "swap", original_swap, NULL); g_free (original_swap);
        { GFile *directory = g_file_new_for_path (private_swap); g_file_delete (directory, NULL, NULL); g_object_unref (directory); }
        g_free (private_swap);
      }
  g_ptr_array_unref (cases);
}
