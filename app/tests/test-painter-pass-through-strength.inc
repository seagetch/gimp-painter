/* SPDX-License-Identifier: GPL-3.0-or-later
 * Old byte child result, current foundation linear pass-through interpolation.
 * This is not a captured old grouped-artwork oracle. */
static void pass_through_group_strength (void)
{
 const guint modes[]={0,3,23,24,25,26,27,28,29};
 const gdouble opacities[]={0,.5,1};const gint masks[]={-1,0,128,255};
 const Babl *to_linear=babl_fish(babl_format("R'G'B'A u8"),babl_format("RGBA double"));
 const Babl *to_byte=babl_fish(babl_format("RGBA double"),babl_format("R'G'B'A u8"));
 guchar *backdrop=read_fixture("backdrop.rgba");gdouble a[256];
 babl_process(to_linear,backdrop,a,64);
 for(guint k=0;k<G_N_ELEMENTS(modes);++k)for(guint oi=0;oi<3;++oi)for(guint mi=0;mi<4;++mi){
  GimpLayer *source,*group;GimpImage *image=mode_reorder_scene(modes[k],0,FALSE,&source,NULL);
  gchar *name=g_strdup_printf("mode-%u-o50-m128-projection.rgba",modes[k]);
  guchar *child=read_fixture(name),actual[256],expected[256],mask[64];
  gdouble b[256],out[256],weight=opacities[oi]*(masks[mi]<0?1:masks[mi]/255.0);GError *error=NULL;
  group=gimp_group_layer_new(image);gimp_layer_set_mode(group,GIMP_LAYER_MODE_PASS_THROUGH,FALSE);
  g_assert_true(gimp_image_add_layer(image,group,NULL,0,FALSE));
  g_assert_true(gimp_image_reorder_item(image,GIMP_ITEM(source),GIMP_ITEM(group),0,FALSE,NULL));
  gimp_layer_set_opacity(group,opacities[oi],FALSE);
  if(masks[mi]>=0){
   GimpLayerMask *lm=gimp_layer_create_mask(group,GIMP_ADD_MASK_WHITE,NULL);memset(mask,masks[mi],64);
   gegl_buffer_set(gimp_drawable_get_buffer(GIMP_DRAWABLE(lm)),GEGL_RECTANGLE(0,0,8,8),0,babl_format("Y u8"),mask,GEGL_AUTO_ROWSTRIDE);
   g_assert_nonnull(gimp_layer_add_mask(group,lm,FALSE,&error));g_assert_no_error(error);
  }
  babl_process(to_linear,child,b,64);
  for(guint p=0;p<64;++p){
   gdouble alpha=(1-weight)*a[4*p+3]+weight*b[4*p+3];
   /* The current Replace/pass-through foundation interpolates hidden RGB
    * at zero alpha.  No claim that this is old Painter's Replace rule. */
   for(guint c=0;c<3;++c)out[4*p+c]=alpha>0?
     ((1-weight)*a[4*p+3]*a[4*p+c]+weight*b[4*p+3]*b[4*p+c])/alpha:
     (1-weight)*a[4*p+c]+weight*b[4*p+c];
   out[4*p+3]=alpha;
  }
  babl_process(to_byte,out,expected,64);mode_reorder_projection(image,actual);
  for(guint p=0;p<256;++p)if(actual[p]!=expected[p])g_error("pass-through strength mode%u opacity%.2f mask%d component%u expected%u got%u",modes[k],opacities[oi],masks[mi],p,expected[p],actual[p]);
  g_free(name);g_free(child);g_object_unref(image);
 }
 g_free(backdrop);
 g_test_message("108 pass-through group-strength contexts: genuine-old byte child result plus independent current linear premultiplied interpolation,64 RGBA pixels each");
}
