/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Original15.006: native attributes over an ACTUAL completed Gaussian cache.
 * Ordinary-layer equivalence is not a second Gaussian arithmetic oracle. */
static void
filter_attributes_mask (GimpLayer *layer,guchar coverage)
{
  GimpLayerMask *mask=gimp_layer_create_mask(layer,GIMP_ADD_MASK_WHITE,NULL);
  guchar pixels[16*16];GError *error=NULL;
  memset(pixels,coverage,sizeof(pixels));
  gegl_buffer_set(gimp_drawable_get_buffer(GIMP_DRAWABLE(mask)),GEGL_RECTANGLE(0,0,16,16),
                  0,babl_format("Y u8"),pixels,GEGL_AUTO_ROWSTRIDE);
  g_assert_nonnull(gimp_layer_add_mask(layer,mask,FALSE,&error));g_assert_no_error(error);
  gimp_drawable_update(GIMP_DRAWABLE(mask),0,0,16,16);
  g_assert_true(gimp_layer_get_apply_mask(layer));
}
static void
filter_attributes_sample (GimpImage *image,gint x,guchar result[4])
{
  gimp_pickable_flush(GIMP_PICKABLE(image));
  gegl_buffer_get(gimp_pickable_get_buffer(GIMP_PICKABLE(image)),GEGL_RECTANGLE(x,8,1,1),
                  1.0,babl_format("R'G'B'A u8"),result,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
}
static void
filter_gaussian_attributes_contract (void)
{
  static const GimpLayerMode modes[]={GIMP_LAYER_MODE_NORMAL_LEGACY,
    GIMP_LAYER_MODE_MULTIPLY_LEGACY,GIMP_LAYER_MODE_PAINTER_REPLACE};
  static const guchar masks[]={0,128,255};
  static const gdouble opacities[]={0.0,0.5,1.0};
  guchar original[16*16*4];guint scenes=0,comparisons=0,normal_checks=0;
  for(guint y=0;y<16;y++)for(guint x=0;x<16;x++)
    {guint i=(y*16+x)*4;original[i]=x<8?255:0;original[i+1]=0;original[i+2]=x<8?0:255;original[i+3]=255;}
  for(guint m=0;m<3;m++)for(guint a=0;a<3;a++)for(guint k=0;k<3;k++)
    {
      GimpImage *image=image_new(16,16),*control_image=image_new(16,16);
      GimpLayer *source=source_new(image,NULL,16,16),*control_source=source_new(control_image,NULL,16,16);
      GimpLayer *control;GimpFilterLayer *filter;
      guchar cache[16*16*4],unchanged[16*16*4];guint64 runs;
      gegl_buffer_set(gimp_drawable_get_buffer(GIMP_DRAWABLE(source)),GEGL_RECTANGLE(0,0,16,16),
                      0,babl_format("R'G'B'A u8"),original,GEGL_AUTO_ROWSTRIDE);
      gimp_drawable_update(GIMP_DRAWABLE(source),0,0,16,16);
      gegl_buffer_set(gimp_drawable_get_buffer(GIMP_DRAWABLE(control_source)),GEGL_RECTANGLE(0,0,16,16),
                      0,babl_format("R'G'B'A u8"),original,GEGL_AUTO_ROWSTRIDE);
      gimp_drawable_update(GIMP_DRAWABLE(control_source),0,0,16,16);
      filter=contract_gaussian_filter(image);settle(filter);
      g_assert_cmpuint(gimp_filter_layer_get_run_count(filter),>,0);
      gegl_buffer_get(gimp_drawable_get_buffer(GIMP_DRAWABLE(filter)),GEGL_RECTANGLE(0,0,16,16),
                      1.0,babl_format("R'G'B'A u8"),cache,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
      /* The boundary really was blurred; do not merely certify an unexecuted
       * source copy. Numerical Gaussian formula equivalence is a separate test. */
      g_assert_cmpint(cache[(8*16+7)*4+2],>,0);
      g_assert_cmpint(cache[(8*16+8)*4],>,0);
      control=source_new(control_image,NULL,16,16);
      gegl_buffer_set(gimp_drawable_get_buffer(GIMP_DRAWABLE(control)),GEGL_RECTANGLE(0,0,16,16),
                      0,babl_format("R'G'B'A u8"),cache,GEGL_AUTO_ROWSTRIDE);
      gimp_drawable_update(GIMP_DRAWABLE(control),0,0,16,16);
      filter_attributes_mask(GIMP_LAYER(filter),masks[k]);filter_attributes_mask(control,masks[k]);
      gimp_layer_set_opacity(GIMP_LAYER(filter),opacities[a],FALSE);gimp_layer_set_opacity(control,opacities[a],FALSE);
      gimp_layer_set_mode(GIMP_LAYER(filter),modes[m],FALSE);gimp_layer_set_mode(control,modes[m],FALSE);
      g_assert_cmpfloat(gimp_layer_get_opacity(GIMP_LAYER(filter)),==,opacities[a]);
      g_assert_cmpint(gimp_layer_get_mode(GIMP_LAYER(filter)),==,modes[m]);
      runs=gimp_filter_layer_get_run_count(filter);
      for(gint x=7;x<=8;x++)
        {
          guchar actual[4],reference[4];guint i=(8*16+x)*4;
          filter_attributes_sample(image,x,actual);filter_attributes_sample(control_image,x,reference);
          g_assert_cmpmem(actual,4,reference,4);comparisons++;
          if(m==0)
            {
              /* Normal legacy composites in nonlinear RGB. Opaque input over
               * opaque destination: independent source-over equation, allowing
               * one u8 quantization unit, not an oracle copied from projection. */
              gdouble weight=opacities[a]*(masks[k]/255.0);
              g_assert_cmpint(cache[i+3],==,255);
              for(guint c=0;c<3;c++)
                {gint expected=(gint)(cache[i+c]*weight+original[i+c]*(1.0-weight)+0.5);
                 g_assert_cmpint(ABS((gint)actual[c]-expected),<=,1);}
              g_assert_cmpint(actual[3],==,255);normal_checks++;
            }
        }
      g_assert_cmpuint(gimp_filter_layer_get_run_count(filter),==,runs);
      gegl_buffer_get(gimp_drawable_get_buffer(GIMP_DRAWABLE(filter)),GEGL_RECTANGLE(0,0,16,16),
                      1.0,babl_format("R'G'B'A u8"),unchanged,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
      g_assert_cmpmem(unchanged,sizeof(unchanged),cache,sizeof(cache));
      g_test_message("scene%u mode=%d opacity=%.1f mask=%u:ordinary-cache projection equal;GEGL reads start no runner;completed cache unchanged",
                     ++scenes,modes[m],opacities[a],masks[k]);
      g_object_unref(image);g_object_unref(control_image);
    }
  g_assert_cmpuint(scenes,==,27);g_assert_cmpuint(comparisons,==,54);g_assert_cmpuint(normal_checks,==,18);
  g_test_message("27 actual Gaussian completed-cache scenes;54 ordinary-layer projection comparisons;18 independent Normalcolor checks;no default Edge");
}
