/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "core/gimpimage-colormap.h"

/* Independent source oracle: afa43fae paint-funcs-generic.h:734-787,
 * initial_indexed_pixels/initial_indexed_a_pixels, not an old runtime result.
 * Palette contains the mask-show gray so indexed storage adds no color error. */
static void
clone_indexed_projection_contract (void)
{
  const guchar palette[] = { 0,0,0, 255,0,0, 0,0,255, 255,255,255, 128,128,128 };
  const guchar alphas[] = { 0, 1, 127, 128, 254, 255 };
  const gdouble opacities[] = { 0.0, 0.5, 1.0 };
  guint checked = 0;
  for (guint alpha = 0; alpha < 2; alpha++)
    for (guint opacity = 0; opacity < G_N_ELEMENTS (opacities); opacity++)
      for (guint mask_state = 0; mask_state < 4; mask_state++)
        {
          GimpImage *image = gimp_image_new (gimp, 16, 16, GIMP_INDEXED,
                                             GIMP_PRECISION_U8_NON_LINEAR);
          GimpLayer *source;
          GimpCloneLayer *clone;
          const Babl *format;
          guchar raw[16 * 16 * 2], actual[6 * 4], expected[6 * 4];
          _gimp_image_set_colormap (image, palette, 5, FALSE);
          format = alpha ? gimp_image_colormap_get_rgba_format (image)
                         : gimp_image_colormap_get_rgb_format (image);
          source = gimp_layer_new (image, 16, 16, format, "indexed source", 1.0,
                                   GIMP_LAYER_MODE_NORMAL_LEGACY);
          g_assert_true (gimp_image_add_layer (image, source, NULL, 0, FALSE));
          for (guint i = 0; i < 256; i++)
            {
              raw[i * (alpha + 1)] = 1 + (i % 2);
              if (alpha) raw[i * 2 + 1] = alphas[i % 6];
            }
          gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (source)),
                           GEGL_RECTANGLE (0,0,16,16), 0, format, raw,
                           GEGL_AUTO_ROWSTRIDE);
          gimp_layer_set_opacity (source, opacities[opacity], FALSE);
          if (mask_state)
            {
              GimpLayerMask *mask = gimp_layer_create_mask (source, GIMP_ADD_MASK_WHITE, NULL);
              guchar values[256];
              memset (values, 128, sizeof values);
              gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (mask)),
                               GEGL_RECTANGLE (0,0,16,16),0,babl_format ("Y u8"),
                               values,GEGL_AUTO_ROWSTRIDE);
              g_assert_nonnull (gimp_layer_add_mask (source,mask,FALSE,NULL));
              if (mask_state == 2) gimp_layer_set_apply_mask (source,FALSE,FALSE);
              if (mask_state == 3) gimp_layer_set_show_mask (source,TRUE,FALSE);
            }
          clone = new_clone (image, source);
          for (guint i = 0; i < 6; i++)
            {
              const guint index = 1 + (i % 2);
              guint a = 255;
              for (guint c = 0; c < 3; c++)
                expected[4*i+c] = mask_state == 3 ? 128 : palette[index*3+c];
              if (alpha && mask_state != 3)
                {
                  const guint o = (guint) (opacities[opacity] * 255.999);
                  const guint m = mask_state == 1 ? 128 : 255;
                  const guint t = alphas[i] * m * o + 0x7f5b;
                  a = (((t >> 7) + t) >> 16) > 127 ? 255 : 0;
                }
              expected[4*i+3] = a;
            }
          gegl_buffer_get (gimp_drawable_get_buffer (GIMP_DRAWABLE (clone)),
                           GEGL_RECTANGLE (0,0,6,1),1.0,babl_format ("R'G'B'A u8"),
                           actual,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
          g_assert_cmpmem (actual,sizeof actual,expected,sizeof expected);
          checked += 6;
          g_object_unref (image);
        }
  g_assert_cmpuint (checked, ==, 144);
  g_test_message ("24 indexed/alpha opacity/mask scenes;144 exact palette/binary-alpha pixels");
}
