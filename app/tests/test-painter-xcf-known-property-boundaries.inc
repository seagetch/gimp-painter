/* SPDX-License-Identifier: GPL-3.0-or-later
 * Verify existing native fixed-size channel-property recovery.
 * A mismatched declared size does not change the payload consumed by the
 * upstream known-property reader. Save must emit canonical lengths again.
 * Generated public-writer bytes, no private parser stub. */
static void fields_channel_property_lengths (void)
{
  const guint32 ids[] = {6, 33, 16, 38};
  const guint32 sizes[] = {0, 1, 2, 3, 4, 5, 11, 12, 13, G_MAXUINT32};
  GimpImage *seed = fields_image ();
  GFile *file = temporary_file ();
  GeglColor *color = gegl_color_new (NULL);
  const gdouble wanted[] = {.1, .3, .7, .45};
  gegl_color_set_pixel (color, babl_format ("R'G'B'A double"), wanted);
  GimpLayer *base = gimp_layer_new (seed, 96, 80,
                                  gimp_image_get_layer_format (seed, TRUE),
                                  "boundary base", 1, GIMP_LAYER_MODE_NORMAL);
  GimpChannel *channel = gimp_channel_new (seed, 96, 80, "boundary channel", color);
  gchar *contents = NULL;
  gsize length = 0, table, begin, end, positions[4] = {0,0,0,0};
  guint version, offset_bytes;
  const guint8 *raw;
  GError *error = NULL;
  g_object_unref (color);
  gimp_image_add_layer (seed, base, NULL, 0, FALSE);
  gimp_image_add_channel (seed, channel, NULL, 0, FALSE);
  save (seed, file);
  g_object_unref (seed);
  g_assert_true (g_file_load_contents (file,NULL,&contents,&length,NULL,&error));
  g_assert_no_error (error);
  raw = (const guint8 *) contents;
  version=(raw[10]-'0')*100+(raw[11]-'0')*10+raw[12]-'0';
  offset_bytes=version>=11?8:4;
  table=order_after_props(raw,length,version>=4?30:26);
  while(order_offset(raw+table,offset_bytes)) table+=offset_bytes;
  table+=offset_bytes;
  begin=order_offset(raw+table,offset_bytes);
  begin+=12+order_u32(raw+begin+8);
  end=order_after_props(raw,length,begin);
  for(gsize at=begin;at<end;at+=8+order_u32(raw+at+4))
    for(guint p=0;p<G_N_ELEMENTS(ids);p++)
      if(order_u32(raw+at)==ids[p])positions[p]=at;
  for(guint p=0;p<G_N_ELEMENTS(ids);p++)
    {
      g_assert_cmpuint(positions[p],>,0);
      for(guint s=0;s<G_N_ELEMENTS(sizes);s++)
        {
          gchar *wire=g_memdup2(contents,length);
          guint32 value=GUINT32_TO_BE(sizes[s]);
          GimpPDBStatusType status;
          GimpPlugInProcedure *proc=GIMP_PLUG_IN_PROCEDURE(gimp_pdb_lookup_procedure(gimp->pdb,"gimp-xcf-load"));
          GimpImage *image;
          memcpy(wire+positions[p]+4,&value,4);
          g_assert_true(g_file_replace_contents(file,wire,length,NULL,FALSE,G_FILE_CREATE_NONE,NULL,NULL,&error));
          g_assert_no_error(error);g_free(wire);
          image=file_open_image(gimp,gimp_get_user_context(gimp),NULL,file,0,0,FALSE,proc,GIMP_RUN_NONINTERACTIVE,&status,NULL,&error);
          GList *channels=image?gimp_image_get_channel_list(image):NULL;
          g_test_message("channel length property%u declared%u original%u image%d status%d channels%u error%d",ids[p],sizes[s],order_u32(raw+positions[p]+4),image!=NULL,status,g_list_length(channels),error!=NULL);
          g_assert_no_error (error);
          g_assert_cmpint (status, ==, GIMP_PDB_SUCCESS);
          g_assert_nonnull (image);
          g_assert_cmpuint (g_list_length (channels), ==, 1);
          GimpChannel *actual = channels->data;
          gdouble rgba[4];
          g_assert_cmpstr (gimp_object_get_name (actual), ==, "boundary channel");
          g_assert_cmpint (gimp_item_get_width (GIMP_ITEM (actual)), ==, 96);
          g_assert_cmpint (gimp_item_get_height (GIMP_ITEM (actual)), ==, 80);
          gegl_color_get_pixel (actual->color, babl_format ("R'G'B'A double"), rgba);
          for (guint k = 0; k < 3; k++)
            g_assert_cmpfloat_with_epsilon (rgba[k], (gdouble)(gfloat)wanted[k], 2e-7);
          g_assert_cmpfloat_with_epsilon (rgba[3], (gdouble)(gfloat)wanted[3], 1e-12);
          g_list_free (channels);
          save (image, file);
          g_object_unref (image);
          gchar *canonical = NULL;
          gsize canonical_length;
          g_assert_true (g_file_load_contents (file, NULL, &canonical,
                                               &canonical_length, NULL, &error));
          g_assert_no_error (error);
          const guint8 *saved = (const guint8 *) canonical;
          gsize saved_table = order_after_props (saved, canonical_length,
                                                version >= 4 ? 30 : 26);
          while (order_offset (saved + saved_table, offset_bytes))
            saved_table += offset_bytes;
          saved_table += offset_bytes;
          gsize saved_begin = order_offset (saved + saved_table, offset_bytes);
          saved_begin += 12 + order_u32 (saved + saved_begin + 8);
          gsize saved_end = order_after_props (saved, canonical_length, saved_begin);
          guint seen = 0;
          for (gsize at = saved_begin; at < saved_end;
               at += 8 + order_u32 (saved + at + 4))
            for (guint k = 0; k < G_N_ELEMENTS (ids); k++)
              if (order_u32 (saved + at) == ids[k])
                {
                  g_assert_cmpuint (order_u32 (saved + at + 4), ==,
                                    order_u32 (raw + positions[k] + 4));
                  seen++;
                }
          g_assert_cmpuint (seen, ==, 4);
          g_free (canonical);
        }
    }
  g_free(contents);g_file_delete(file,NULL,NULL);g_object_unref(file);
}
