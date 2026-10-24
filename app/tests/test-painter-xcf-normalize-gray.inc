/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Native Gray image Save/Open, not an old-application file capture. */
static GBytes *xcf_normalize_gray_bytes (guint variant,gboolean transparent)
{
 guint8 p[17*13*2];for(guint i=0;i<17*13;i++){p[i*2]=30+(i*17+variant*7)%171;p[i*2+1]=transparent?0:255;}return g_bytes_new(p,sizeof p);
}
static GBytes *xcf_normalize_gray_expected (GBytes *input)
{
 gsize n;const guint8 *p=g_bytes_get_data(input,&n);guint8 *out=g_memdup2(p,n);gint lo=255,hi=0;
 for(gsize i=0;i<n;i+=2){g_assert_cmpuint(p[i+1],==,255);lo=MIN(lo,p[i]);hi=MAX(hi,p[i]);}
 for(gsize i=0;i<n;i+=2)out[i]=lo==hi?p[i]:255*(p[i]-lo)/(hi-lo);return g_bytes_new_take(out,n);
}
static void normalize_gray_native_roundtrips (void)
{
 for(guint failed=0;failed<2;failed++){
  GimpImage *image=gimp_image_new(gimp,17,13,GIMP_GRAY,GIMP_PRECISION_U8_NON_LINEAR);
  GimpLayer *source=gimp_layer_new(image,17,13,babl_format("Y'A u8"),"max-rgb source",1,GIMP_LAYER_MODE_NORMAL_LEGACY);
  g_assert_true(gimp_image_add_layer(image,source,NULL,0,FALSE));
  GBytes *bytes=xcf_normalize_gray_bytes(0,FALSE),*changed=xcf_normalize_gray_bytes(1,FALSE),*transparent=xcf_normalize_gray_bytes(0,TRUE),*expected=xcf_normalize_gray_expected(bytes),*next=xcf_normalize_gray_expected(changed),*raw=xcf_normalize_capsule(-123);
  gegl_buffer_set(gimp_drawable_get_buffer(GIMP_DRAWABLE(source)),NULL,0,babl_format("Y'A u8"),g_bytes_get_data(bytes,NULL),GEGL_AUTO_ROWSTRIDE);
  GimpFilterLayer *filter=GIMP_FILTER_LAYER(gimp_filter_layer_new(image,17,13,"max-rgb filter",1,GIMP_LAYER_MODE_NORMAL_LEGACY));
  GimpValueArray *args=gimp_value_array_new_from_types(NULL,G_TYPE_INT,-123,G_TYPE_INT,88,G_TYPE_INT,-99,G_TYPE_NONE);
  g_assert_true(gimp_filter_layer_set_definition(filter,"plug-in-normalize",raw,args,NULL));gimp_value_array_unref(args);g_assert_true(gimp_image_add_layer(image,GIMP_LAYER(filter),NULL,0,FALSE));
  xcf_max_rgb_settle(filter,GIMP_FILTER_LAYER_CLEAN);xcf_max_rgb_pixels(GIMP_LAYER(filter),expected,2);
  if(failed){xcf_max_rgb_lower_input(image,2,transparent);xcf_max_rgb_settle(filter,GIMP_FILTER_LAYER_FAILED);xcf_max_rgb_pixels(GIMP_LAYER(filter),expected,2);}
  image=xcf_max_rgb_close_reopen(image);filter=xcf_max_rgb_filter(image);g_assert_cmpint(gimp_image_get_base_type(image),==,GIMP_GRAY);g_assert_cmpint(gimp_drawable_get_base_type(GIMP_DRAWABLE(filter)),==,GIMP_GRAY);
  xcf_normalize_parameters(filter,-123,88,-99);xcf_max_rgb_raw_equal(filter,raw);xcf_max_rgb_pixels(GIMP_LAYER(filter),expected,2);
  xcf_max_rgb_lower_input(image,2,changed);xcf_max_rgb_settle(filter,GIMP_FILTER_LAYER_CLEAN);xcf_max_rgb_pixels(GIMP_LAYER(filter),next,2);
  image=xcf_max_rgb_close_reopen(image);filter=xcf_max_rgb_filter(image);xcf_normalize_parameters(filter,-123,88,-99);xcf_max_rgb_raw_equal(filter,raw);xcf_max_rgb_pixels(GIMP_LAYER(filter),next,2);g_assert_cmpint(gimp_image_get_base_type(image),==,GIMP_GRAY);
  g_bytes_unref(bytes);g_bytes_unref(changed);g_bytes_unref(transparent);g_bytes_unref(expected);g_bytes_unref(next);g_bytes_unref(raw);g_object_unref(image);
 }
}
