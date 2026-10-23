/* SPDX-License-Identifier: GPL-3.0-or-later
 * Recognized-property absence through same-size unknown-tag substitution.
 * Keep offsets/payloads intact and exercise all16 representation subsets. */
static void
fields_channel_property_absence (void)
{
  const guint32 ids[] = {6, 33, 16, 38};
  const gdouble wanted[] = {.13, .27, .71, .37};
  GimpImage *seed = fields_image ();
  GFile *file = temporary_file ();
  GeglColor *color = gegl_color_new (NULL);
  gegl_color_set_pixel (color, babl_format ("R'G'B'A double"), wanted);
  GimpLayer *base = gimp_layer_new (seed, 96, 80,
                                  gimp_image_get_layer_format (seed, TRUE),
                                  "absence base", 1, GIMP_LAYER_MODE_NORMAL);
  GimpChannel *channel = gimp_channel_new (seed, 96, 80, "absence channel", color);
  gchar *contents = NULL;
  gsize length, table, begin, end, positions[4] = {0, 0, 0, 0};
  GError *error = NULL;
  g_object_unref (color);
  gimp_image_add_layer (seed, base, NULL, 0, FALSE);
  gimp_image_add_channel (seed, channel, NULL, 0, FALSE);
  save (seed, file);
  g_object_unref (seed);
  g_assert_true (g_file_load_contents (file, NULL, &contents, &length, NULL, &error));
  g_assert_no_error (error);
  const guint8 *raw = (const guint8 *) contents;
  guint version = (raw[10]-'0')*100 + (raw[11]-'0')*10 + raw[12]-'0';
  guint offset_bytes = version >= 11 ? 8 : 4;
  table = order_after_props (raw, length, version >= 4 ? 30 : 26);
  while (order_offset (raw + table, offset_bytes)) table += offset_bytes;
  table += offset_bytes;
  begin = order_offset (raw + table, offset_bytes);
  begin += 12 + order_u32 (raw + begin + 8);
  end = order_after_props (raw, length, begin);
  for (gsize at = begin; at < end; at += 8 + order_u32 (raw + at + 4))
    for (guint p = 0; p < G_N_ELEMENTS (ids); p++)
      if (order_u32 (raw + at) == ids[p]) positions[p] = at;
  for (guint p = 0; p < 4; p++) g_assert_cmpuint (positions[p], >, 0);
  for (guint present = 0; present < 16; present++)
    {
      gchar *wire = g_memdup2 (contents, length);
      for (guint p = 0; p < 4; p++)
        if (!(present & (1u << p)))
          {
            guint32 unknown = GUINT32_TO_BE (0xf0010000u + p);
            memcpy (wire + positions[p], &unknown, 4);
          }
      g_assert_true (g_file_replace_contents (file, wire, length, NULL, FALSE,
                                             G_FILE_CREATE_NONE, NULL, NULL, &error));
      g_assert_no_error (error);
      g_free (wire);
      GimpImage *image = load (file);
      gdouble expected[4] = {0, 0, 0, 1};
      if (present & (1u << 2))
        for (guint k = 0; k < 3; k++) expected[k] = raw[positions[2] + 8 + k] / 255.0;
      if (present & (1u << 3))
        for (guint k = 0; k < 3; k++) expected[k] = (gdouble)(gfloat)wanted[k];
      if (present & 1u) expected[3] = order_u32 (raw + positions[0] + 8) / 255.0;
      if (present & 2u) expected[3] = (gdouble)(gfloat)wanted[3];
      for (guint repeat = 0; repeat < 2; repeat++)
        {
          GList *channels = gimp_image_get_channel_list (image);
          g_assert_cmpuint (g_list_length (channels), ==, 1);
          GimpChannel *actual = channels->data;
          gdouble rgba[4];
          g_assert_cmpstr (gimp_object_get_name (actual), ==, "absence channel");
          gegl_color_get_pixel (actual->color, babl_format ("R'G'B'A double"), rgba);
          for (guint k = 0; k < 3; k++)
            g_assert_cmpfloat_with_epsilon (rgba[k], expected[k], 2e-7);
          g_assert_cmpfloat_with_epsilon (rgba[3], expected[3], 1e-12);
          g_test_message ("channel presence mask%u round%u alpha %.17g", present, repeat, rgba[3]);
          g_list_free (channels);
          if (!repeat)
            {
              save (image, file);
              g_object_unref (image);
              image = load (file);
              expected[3] = (gdouble)(gfloat)expected[3];
            }
        }
      g_object_unref (image);
    }
  g_free (contents);
  g_file_delete (file, NULL, NULL);
  g_object_unref (file);
}
