/* SPDX-License-Identifier: GPL-3.0-or-later
 * Group-level initial-region order using genuine-old child projection bytes.
 * The two macros below are unchanged old INT_MULT / INT_MULT3 definitions,
 * documented/extracted independently in legacy-seven-formulas. */
static guint old_initial_mult(guint a,guint b)
{guint t=a*b+128;return ((t>>8)+t)>>8;}
static guint old_initial_mult3(guint a,guint b,guint c)
{guint t=a*b*c+0x7f5b;return ((t>>7)+t)>>16;}
static void group_mask_opacity_order(void)
{
 const guint modes[]={0,3,23,24,25,26,27,28,29};const gint masks[]={-1,0,128,255};
 for(guint k=0;k<G_N_ELEMENTS(modes);++k)for(guint oi=0;oi<2;++oi)for(guint mi=0;mi<4;++mi){
  GimpLayer*group;GimpImage*image=mode_reorder_scene(modes[k],1,FALSE,NULL,&group);
  guint opacity=oi?255:127;guchar actual[256],expected[256],mask[64];GError*error=NULL;
  gchar*name=g_strdup_printf("mode-%u-o50-m128-projection.rgba",modes[k]);guchar*child=read_fixture(name);
  memcpy(expected,child,256);gimp_layer_set_opacity(group,oi?1:.5,FALSE);
  if(masks[mi]>=0){
   GimpLayerMask*lm=gimp_layer_create_mask(group,GIMP_ADD_MASK_WHITE,NULL);memset(mask,masks[mi],64);
   gegl_buffer_set(gimp_drawable_get_buffer(GIMP_DRAWABLE(lm)),GEGL_RECTANGLE(0,0,8,8),0,babl_format("Y u8"),mask,GEGL_AUTO_ROWSTRIDE);
   g_assert_nonnull(gimp_layer_add_mask(group,lm,FALSE,&error));g_assert_no_error(error);
  }
  for(guint p=0;p<64;++p)expected[4*p+3]=masks[mi]<0?old_initial_mult(opacity,child[4*p+3]):old_initial_mult3(opacity,child[4*p+3],masks[mi]);
  mode_reorder_projection(image,actual);
  for(guint p=0;p<256;++p)if(actual[p]!=expected[p])g_error("group initial order mode%u opacity%u mask%d component%u expected%u got%u",modes[k],opacity,masks[mi],p,expected[p],actual[p]);
  g_free(child);g_free(name);g_object_unref(image);
 }
 g_test_message("72 isolated group-level opacity/mask contexts preserve genuine-old child projection then old initial-region alpha order");
}
static void group_reparent_mask_undo(void)
{
 const guint modes[]={0,3,23,24,25,26,27,28,29};
 for(guint k=0;k<G_N_ELEMENTS(modes);++k){
  GimpLayer*source,*old_group;GimpImage*image=mode_reorder_scene(modes[k],1,FALSE,&source,&old_group);
  GimpLayer*destination=gimp_group_layer_new(image);guchar actual[256];
  gchar*masked_name=g_strdup_printf("mode-%u-o50-m128-projection.rgba",modes[k]);
  gchar*unmasked_name=g_strdup_printf("mode-%u-o50-mnone-projection.rgba",modes[k]);
  guchar*masked=read_fixture(masked_name),*unmasked=read_fixture(unmasked_name);
  g_assert_cmpint(memcmp(masked,unmasked,256),!=,0);
  gimp_layer_set_mode(destination,GIMP_LAYER_MODE_PASS_THROUGH,FALSE);
  g_assert_true(gimp_image_add_layer(image,destination,NULL,0,FALSE));
  mode_reorder_projection(image,actual);g_assert_cmpmem(actual,256,masked,256);
  g_assert_true(gimp_image_reorder_item(image,GIMP_ITEM(source),GIMP_ITEM(destination),0,TRUE,"Move Painter child between groups"));
  g_assert_true(gimp_item_get_parent(GIMP_ITEM(source))==GIMP_ITEM(destination));
  mode_reorder_projection(image,actual);g_assert_cmpmem(actual,256,masked,256);
  gimp_layer_set_apply_mask(source,FALSE,TRUE);
  mode_reorder_projection(image,actual);g_assert_cmpmem(actual,256,unmasked,256);
  for(guint cycle=0;cycle<3;++cycle){
   g_assert_true(gimp_image_undo(image));g_assert_true(gimp_layer_get_apply_mask(source));
   mode_reorder_projection(image,actual);g_assert_cmpmem(actual,256,masked,256);
   g_assert_true(gimp_image_undo(image));g_assert_true(gimp_item_get_parent(GIMP_ITEM(source))==GIMP_ITEM(old_group));
   mode_reorder_projection(image,actual);g_assert_cmpmem(actual,256,masked,256);
   g_assert_true(gimp_image_redo(image));g_assert_true(gimp_item_get_parent(GIMP_ITEM(source))==GIMP_ITEM(destination));
   mode_reorder_projection(image,actual);g_assert_cmpmem(actual,256,masked,256);
   g_assert_true(gimp_image_redo(image));g_assert_false(gimp_layer_get_apply_mask(source));
   mode_reorder_projection(image,actual);g_assert_cmpmem(actual,256,unmasked,256);
  }
  g_free(masked);g_free(unmasked);g_free(masked_name);g_free(unmasked_name);g_object_unref(image);
 }
 g_test_message("nine cross-group reparents plus mask disable and108 Undo/Redo actions match genuine-old masked/unmasked projections");
}
