/* Prior-port reader: opaque v2 preservation, not semantic compatibility. */
static void prior_float_capsule (void)
{
  const gchar *directory = g_getenv ("GIMP_TEST_FLOAT_XCF_CAPTURE");
  g_assert_nonnull (directory);
  for (guint kind = 0; kind < 3; kind++)
    for (guint mode = 0; mode < 9; mode++)
      {
        gchar *name = g_strdup_printf ("kind%u-mode%u.xcf", kind, mode);
        gchar *path = g_build_filename (directory, name, NULL);
        gchar *capsule_path = g_strconcat (path, ".capsule", NULL);
        gchar *bytes = NULL; gsize size = 0; GError *error = NULL;
        g_assert_true (g_file_get_contents (capsule_path, &bytes, &size, &error));
        g_assert_no_error (error);
        GBytes *expected = g_bytes_new_take (bytes, size);
        GFile *input = g_file_new_for_path (path), *output = temporary_file ();
        GimpImage *image = load (input);
        gfloat initial[256];
        for (guint cycle = 0; cycle < 3; cycle++)
          {
            GimpLayer *target = layer (image, "float target");
            g_assert_nonnull (target);
            g_assert_false (GIMP_IS_CLONE_LAYER (target));
            g_assert_cmpint (GIMP_IS_GROUP_LAYER (target), ==, kind == 2);
            g_assert_cmpint (gimp_layer_get_mode (target), ==, GIMP_LAYER_MODE_NORMAL_LEGACY);
            const GimpParasite *parasite = gimp_item_parasite_find (GIMP_ITEM (target), "gimp-painter-item");
            g_assert_nonnull (parasite);
            guint32 actual_size; const void *actual = gimp_parasite_get_data (parasite, &actual_size);
            g_assert_cmpmem (actual, actual_size, g_bytes_get_data (expected, NULL), size);
            gfloat pixels[256]; gimp_pickable_flush (GIMP_PICKABLE (image));
            gegl_buffer_get (gimp_pickable_get_buffer (GIMP_PICKABLE (image)), GEGL_RECTANGLE (0,0,8,8), 1,
                             babl_format ("R'G'B'A float"), pixels, GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);
            if (!cycle) memcpy (initial, pixels, sizeof pixels);
            else for (guint p=0; p<256; p++) g_assert_cmpfloat_with_epsilon (pixels[p], initial[p], 2e-6f);
            if (cycle < 2)
              { save (image, output); GimpImage *copy = load (output); g_object_unref (image); image = copy; }
          }
        g_test_message ("prior reader kind%u mode%u: standard fallback, opaque capsule and fallback pixels across two resaves", kind, mode);
        g_object_unref (image); g_file_delete (output, NULL, NULL); g_object_unref (output);
        g_object_unref (input); g_bytes_unref (expected); g_free (capsule_path); g_free (path); g_free (name);
      }
}
