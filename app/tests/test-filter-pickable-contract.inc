/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "core/gimpimage-pick-item.h"
/* Deliberately does not call filter_new(), whose fixture defaults to Edge. */
static GimpFilterLayer *
contract_gaussian_filter (GimpImage *image)
{
  GimpLayer *layer = gimp_filter_layer_new (image,16,16,"contract Gaussian",1.0,
                                           GIMP_LAYER_MODE_NORMAL_LEGACY);
  GimpValueArray *args = gimp_value_array_new_from_types (NULL,
    G_TYPE_INT,1,G_TYPE_INT,123,G_TYPE_INT,456,
    G_TYPE_DOUBLE,1.0,G_TYPE_DOUBLE,1.0,G_TYPE_INT,1,G_TYPE_NONE);
  g_assert_true (gimp_filter_layer_set_definition (GIMP_FILTER_LAYER (layer),
                                                 "plug-in-gauss",NULL,args,NULL));
  gimp_value_array_unref (args);
  g_assert_true (gimp_image_add_layer (image,layer,NULL,0,FALSE));
  return GIMP_FILTER_LAYER (layer);
}
static void
filter_contract_image_pixel (GimpImage *image,const guchar expected[4])
{
  guchar actual[4];
  gimp_pickable_flush (GIMP_PICKABLE (image));
  gegl_buffer_get (gimp_pickable_get_buffer (GIMP_PICKABLE (image)),
                   GEGL_RECTANGLE (4,4,1,1),1.0,babl_format ("R'G'B'A u8"),
                   actual,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
  g_assert_cmpmem (actual,4,expected,4);
}
static void
filter_pickable_projection_contract (void)
{
  GimpImage *image = image_new (16,16);
  GimpLayer *source = source_new (image,NULL,16,16);
  GimpFilterLayer *filter;
  fill (source,0,0,255,255);
  filter = contract_gaussian_filter (image);
  fill (GIMP_LAYER (filter),0,255,0,255);
  gimp_filter_layer_mark_as_loaded (filter);
  spin_ms (40);
  g_assert_cmpuint (gimp_filter_layer_get_run_count (filter),==,0);
  for (gint y=-1;y<=16;y++) for (gint x=-1;x<=16;x++)
    g_assert_cmpfloat (gimp_pickable_get_opacity_at (GIMP_PICKABLE (filter),x,y),==,0.0);
  filter_contract_image_pixel (image,(const guchar[]){0,255,0,255});
  g_assert_true (gimp_image_pick_layer (image,4,4,NULL)==source);
  g_assert_true (gimp_image_pick_layer (image,4,4,GIMP_LAYER (filter))==source);
  g_assert_true (gimp_image_pick_layer_by_bounds (image,4,4)==GIMP_LAYER (filter));
  gimp_item_set_visible (GIMP_ITEM (filter),FALSE,FALSE);
  filter_contract_image_pixel (image,(const guchar[]){0,0,255,255});
  gimp_item_set_visible (GIMP_ITEM (filter),TRUE,FALSE);
  fill (source,255,0,0,255);
  settle (filter);
  g_assert_cmpuint (gimp_filter_layer_get_run_count (filter),==,1);
  filter_contract_image_pixel (image,(const guchar[]){255,0,0,255});
  g_assert_cmpfloat (gimp_pickable_get_opacity_at (GIMP_PICKABLE (filter),4,4),==,0.0);
  g_object_unref (image);
  g_test_message ("325 zero queries;3 projection pixels;3 picking controls;loaded cache plus native Gaussian uniform-color refresh, no Edge");
}
