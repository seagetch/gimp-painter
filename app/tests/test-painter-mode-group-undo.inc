/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "core/gimpimage-undo.h"
static GimpImage *mode_reorder_scene(guint raw,gint grouped,gboolean source_below,
                                     GimpLayer **source_out,GimpLayer **group_out)
{
  const Babl *format=babl_format("R'G'B'A u8");const GeglRectangle rect={0,0,8,8};
  GimpImage *image=gimp_image_new(gimp,8,8,GIMP_RGB,GIMP_PRECISION_U8_NON_LINEAR);
  GimpLayer *group=NULL,*base,*source;GimpLayerMask *lm;GimpLayerMode mode;
  guchar *a=read_fixture("backdrop.rgba"),*b=read_fixture("source.rgba"),mask[64];GError *error=NULL;
  memset(mask,128,sizeof mask);gimp_painter_layer_mode_from_legacy(raw,&mode);
  if(grouped) {
    group=gimp_group_layer_new(image);
    gimp_layer_set_mode(group,grouped==1?GIMP_LAYER_MODE_PAINTER_NORMAL:GIMP_LAYER_MODE_PASS_THROUGH,FALSE);
    g_assert_true(gimp_image_add_layer(image,group,NULL,0,FALSE));
  }
  base=gimp_layer_new(image,8,8,format,"reorder backdrop",1,GIMP_LAYER_MODE_PAINTER_NORMAL);
  source=gimp_layer_new(image,8,8,format,"reorder source",.5,mode);
  g_assert_true(gimp_image_add_layer(image,base,group,0,FALSE));
  g_assert_true(gimp_image_add_layer(image,source,group,source_below?1:0,FALSE));
  gegl_buffer_set(gimp_drawable_get_buffer(GIMP_DRAWABLE(base)),&rect,0,format,a,GEGL_AUTO_ROWSTRIDE);
  gegl_buffer_set(gimp_drawable_get_buffer(GIMP_DRAWABLE(source)),&rect,0,format,b,GEGL_AUTO_ROWSTRIDE);
  lm=gimp_layer_create_mask(source,GIMP_ADD_MASK_WHITE,NULL);
  gegl_buffer_set(gimp_drawable_get_buffer(GIMP_DRAWABLE(lm)),&rect,0,babl_format("Y u8"),mask,GEGL_AUTO_ROWSTRIDE);
  g_assert_nonnull(gimp_layer_add_mask(source,lm,FALSE,&error));g_assert_no_error(error);
  gimp_drawable_update(GIMP_DRAWABLE(base),0,0,8,8);gimp_drawable_update(GIMP_DRAWABLE(source),0,0,8,8);
  g_free(a);g_free(b);if(source_out)*source_out=source;if(group_out)*group_out=group;return image;
}
static void mode_reorder_projection(GimpImage *image,guchar out[256])
{
  gimp_pickable_flush(GIMP_PICKABLE(image));
  gegl_buffer_get(gimp_pickable_get_buffer(GIMP_PICKABLE(image)),GEGL_RECTANGLE(0,0,8,8),1,
                  babl_format("R'G'B'A u8"),out,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
}
static void group_reorder_undo_projection(void)
{
  const guint raw_modes[]={0,3,23,24,25,26,27,28,29};
  for(gint grouped=1;grouped<=2;++grouped) for(guint k=0;k<G_N_ELEMENTS(raw_modes);++k) {
    guint raw=raw_modes[k];GimpLayer *source,*group;guchar expected_after[256],actual[256];
    GimpImage *image=mode_reorder_scene(raw,grouped,FALSE,&source,&group);
    GimpImage *reference=mode_reorder_scene(raw,0,TRUE,NULL,NULL);
    gchar *name=g_strdup_printf("mode-%u-o50-m128-projection.rgba",raw);
    guchar *before=read_fixture(name);
    mode_reorder_projection(image,actual);g_assert_cmpmem(actual,sizeof actual,before,256);
    mode_reorder_projection(reference,expected_after);
    g_assert_cmpint(memcmp(before,expected_after,256),!=,0);
    g_assert_true(gimp_image_reorder_item(image,GIMP_ITEM(source),GIMP_ITEM(group),1,TRUE,"Reorder byte-mode child"));
    mode_reorder_projection(image,actual);g_assert_cmpmem(actual,sizeof actual,expected_after,sizeof expected_after);
    for(guint cycle=0;cycle<3;++cycle) {
      g_assert_true(gimp_image_undo(image));
      mode_reorder_projection(image,actual);g_assert_cmpmem(actual,sizeof actual,before,256);
      g_assert_true(gimp_image_redo(image));
      mode_reorder_projection(image,actual);g_assert_cmpmem(actual,sizeof actual,expected_after,sizeof expected_after);
    }
    g_test_message("group %s mode %u: reorder and three Undo/Redo cycles match old initial/fresh-flat reordered projections",grouped==1?"isolated":"pass-through",raw);
    g_free(before);g_free(name);g_object_unref(image);g_object_unref(reference);
  }
}
