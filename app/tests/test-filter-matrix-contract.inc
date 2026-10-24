/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Pinned FilterLayer::transform delegates to its parent and THEN unconditionally
 * resets/invalidates. Same bounds/format must not hide the reevaluation duty. */
static void
filter_matrix_count_buffer_notify (GObject *object,GParamSpec *spec,gpointer data)
{
  (void)object;(void)spec;(*(guint *)data)++;
}
static void
filter_gaussian_same_extent_matrix_contract (void)
{
  GimpImage *image=image_new(16,16);
  GimpLayer *source=source_new(image,NULL,16,16);
  GimpFilterLayer *filter;
  guchar pixels[16*16*4],observed[4];
  GimpMatrix3 matrix = {{{-1.0,0.0,16.0},{0.0,-1.0,16.0},{0.0,0.0,1.0}}};
  guint notifications=0;
  guint64 generation,runs;
  for(guint y=0;y<16;y++)for(guint x=0;x<16;x++)
    {
      guint i=(y*16+x)*4;
      pixels[i]=x<8?255:0;pixels[i+1]=0;pixels[i+2]=x<8?0:255;pixels[i+3]=255;
    }
  gegl_buffer_set(gimp_drawable_get_buffer(GIMP_DRAWABLE(source)),GEGL_RECTANGLE(0,0,16,16),
                  0,babl_format("R'G'B'A u8"),pixels,GEGL_AUTO_ROWSTRIDE);
  gimp_drawable_update(GIMP_DRAWABLE(source),0,0,16,16);
  filter=contract_gaussian_filter(image);settle(filter);pixel(GIMP_LAYER(filter),255,0,0,255);
  generation=gimp_filter_layer_get_generation(filter);runs=gimp_filter_layer_get_run_count(filter);
  g_signal_connect(filter,"notify::buffer",G_CALLBACK(filter_matrix_count_buffer_notify),&notifications);
  /* Small ordinary geometry: no Clone, no extreme coordinates or resize. */
  gimp_item_transform(GIMP_ITEM(filter),gimp_get_user_context(gimp),&matrix,
                      GIMP_TRANSFORM_FORWARD,GIMP_INTERPOLATION_NONE,
                      GIMP_TRANSFORM_RESIZE_CLIP,NULL);
  g_assert_cmpint(gimp_item_get_offset_x(GIMP_ITEM(filter)),==,0);
  g_assert_cmpint(gimp_item_get_offset_y(GIMP_ITEM(filter)),==,0);
  g_assert_cmpint(gimp_item_get_width(GIMP_ITEM(filter)),==,16);
  g_assert_cmpint(gimp_item_get_height(GIMP_ITEM(filter)),==,16);
  pixel(source,255,0,0,255);
  gegl_buffer_get(gimp_drawable_get_buffer(GIMP_DRAWABLE(filter)),GEGL_RECTANGLE(0,0,1,1),1.0,
                  babl_format("R'G'B'A u8"),observed,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
  g_test_message("same-extent matrix180:buffer notifications=%u;generation=%"G_GUINT64_FORMAT"->%"G_GUINT64_FORMAT";immediate pixel=%u,%u,%u,%u;source/extent unchanged",
                 notifications,generation,gimp_filter_layer_get_generation(filter),observed[0],observed[1],observed[2],observed[3]);
  g_assert_cmpuint(notifications,>,0); /* Native parent transform actually ran. */
  g_assert_cmpuint(gimp_filter_layer_get_generation(filter),>,generation);
  settle(filter);
  g_assert_cmpuint(gimp_filter_layer_get_run_count(filter),>,runs);
  pixel(GIMP_LAYER(filter),255,0,0,255); /* Reevaluate unchanged lower source. */
  g_object_unref(image);
}
