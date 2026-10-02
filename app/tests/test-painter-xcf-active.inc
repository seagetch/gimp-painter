/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Included by the native XCF roundtrip harness. These are real spill jobs;
 * saved state is observed between quanta, without a synthetic worker seam. */
static void active_filter_wait (GimpFilterLayer *filter, GimpFilterLayerState state)
{
  gint64 deadline = g_get_monotonic_time () + 30000000;
  while (gimp_filter_layer_get_state (filter) != state && g_get_monotonic_time () < deadline)
    {
      if (gimp_filter_layer_get_state (filter) == GIMP_FILTER_LAYER_FAILED)
        { gchar *error = gimp_filter_layer_dup_error (filter);
          g_test_message ("Active-save wait for state%d failed: %s", state, error ? error : "no diagnostic"); g_free (error); }
      g_assert_cmpint (gimp_filter_layer_get_state (filter), !=, GIMP_FILTER_LAYER_FAILED);
      g_main_context_iteration (NULL, FALSE);
      g_usleep (100);
    }
  g_assert_cmpint (gimp_filter_layer_get_state (filter), ==, state);
}
static void active_filter_save_phase (GimpFilterLayerState state)
{
  const gint size = 1025;
  gchar *original_swap = NULL;
  gchar *private_swap = g_dir_make_tmp ("painter-xcf-active-XXXXXX", NULL);
  GimpImage *image = gimp_image_new (gimp, size, size, GIMP_RGB, GIMP_PRECISION_U8_NON_LINEAR);
  GimpLayer *source = gimp_layer_new (image, size, size, babl_format ("R'G'B'A u8"),
                                     "active source", 1, GIMP_LAYER_MODE_NORMAL_LEGACY);
  GimpFilterLayer *filter = GIMP_FILTER_LAYER (gimp_filter_layer_new (
    image, size, size, "active filter", 1, GIMP_LAYER_MODE_NORMAL_LEGACY));
  GimpValueArray *args = gimp_value_array_new_from_types (NULL, G_TYPE_INT, 1,
    G_TYPE_INT, 0, G_TYPE_INT, 0, G_TYPE_DOUBLE, 25., G_TYPE_DOUBLE, 25., G_TYPE_INT, 0, G_TYPE_NONE);
  const guint8 input[] = {103, 71, 199, 255}, cached[] = {13, 29, 47, 255};
  guint8 expected[4];
  GeglColor *color = gegl_color_new (NULL);
  GimpFilterLayerSnapshot saved, loaded;
  GFile *file = temporary_file ();
  GimpImage *copy;
  GimpFilterLayer *reopened;
  GBytes *raw = g_bytes_new_static ("active saved definition", 23), *retained;
  gchar *name;
  g_assert_nonnull (private_swap);
  g_object_get (gegl_config (), "swap", &original_swap, NULL);
  g_object_set (gegl_config (), "swap", private_swap, NULL);
  gimp_image_add_layer (image, source, NULL, 0, FALSE);
  gimp_image_add_layer (image, GIMP_LAYER (filter), NULL, 0, FALSE);
  gegl_color_set_pixel (color, babl_format ("R'G'B'A u8"), input);
  gegl_buffer_set_color (gimp_drawable_get_buffer (GIMP_DRAWABLE (source)), NULL, color);
  gegl_color_set_pixel (color, babl_format ("R'G'B'A u8"), cached);
  gegl_buffer_set_color (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)), NULL, color);
  g_object_unref (color);
  g_assert_true (gimp_filter_layer_set_definition (filter, "plug-in-gauss", raw, args, NULL));
  active_filter_wait (filter, state);
  g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 1);
  g_assert_true (gimp_filter_layer_get_snapshot_state (filter, &saved));
  g_assert_cmpuint (saved.generation, >, saved.cache_generation);
  pixel (GIMP_LAYER (filter), 13, 29, 47, 255);
  save (image, file);
  copy = load (file);
  reopened = GIMP_FILTER_LAYER (layer (copy, "active filter"));
  g_assert_true (GIMP_IS_FILTER_LAYER (reopened));
  g_assert_true (gimp_filter_layer_get_snapshot_state (reopened, &loaded));
  g_assert_cmpuint (gimp_filter_layer_get_run_count (reopened), ==, 0);
  g_assert_cmpuint (loaded.generation, >, loaded.cache_generation);
  g_assert_cmpint (loaded.state, !=, GIMP_FILTER_LAYER_RUNNING);
  g_assert_cmpint (loaded.state, !=, GIMP_FILTER_LAYER_IMPORTING);
  pixel (GIMP_LAYER (reopened), 13, 29, 47, 255);
  name = gimp_filter_layer_dup_procedure (reopened);
  g_assert_cmpstr (name, ==, "plug-in-gauss"); g_free (name);
  retained = gimp_filter_layer_ref_definition (reopened);
  g_assert_true (g_bytes_equal (raw, retained)); g_bytes_unref (retained);
  /* Closing the old image while its worker or private import is active cannot
   * cancel or publish into the independently loaded image. */
  g_object_unref (image);
  active_filter_wait (reopened, GIMP_FILTER_LAYER_CLEAN);
  g_assert_cmpuint (gimp_filter_layer_get_run_count (reopened), ==, 1);
  g_assert_cmpuint (gimp_filter_layer_get_generation (reopened), ==,
                    gimp_filter_layer_get_cache_generation (reopened));
  gegl_buffer_get (gimp_drawable_get_buffer (GIMP_DRAWABLE (reopened)), GEGL_RECTANGLE (0,0,1,1),
                   1, babl_format ("R'G'B'A u8"), expected, GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);
  g_assert_cmpint (memcmp (expected, cached, 4), !=, 0);
  save (copy, file); g_object_unref (copy); copy = load (file);
  reopened = GIMP_FILTER_LAYER (layer (copy, "active filter"));
  pixel (GIMP_LAYER (reopened), expected[0], expected[1], expected[2], expected[3]);
  g_assert_cmpint (gimp_filter_layer_get_state (reopened), ==, GIMP_FILTER_LAYER_CLEAN);
  g_assert_cmpuint (gimp_filter_layer_get_run_count (reopened), ==, 0);
  g_object_unref (copy); g_file_delete (file, NULL, NULL); g_object_unref (file); g_bytes_unref (raw); gimp_value_array_unref (args);
  g_object_set (gegl_config (), "swap", original_swap, NULL); g_free (original_swap);
  { GFile *directory = g_file_new_for_path (private_swap); g_file_delete (directory, NULL, NULL); g_object_unref (directory); }
  g_free (private_swap);
}
static void filter_running_save_close_reopen (void)
{ active_filter_save_phase (GIMP_FILTER_LAYER_RUNNING); }
static void filter_importing_save_close_reopen (void)
{ active_filter_save_phase (GIMP_FILTER_LAYER_IMPORTING); }
