/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Gray scalar expectation is independent of equal-RGB production transport. */
static void normalize_gray_reference (const guchar *input,guchar *shadow,guint width,guint height,guint x1,guint y1,guint x2,guint y2)
{
 gint lo=255,hi=0;memset(shadow,0,(gsize)width*height*2);
 for(guint y=y1;y<y2;y++)for(guint x=x1;x<x2;x++){gsize p=((gsize)y*width+x)*2;if(input[p+1]){lo=MIN(lo,input[p]);hi=MAX(hi,input[p]);}}
 g_assert_cmpint(lo,<=,hi);
 for(guint y=y1;y<y2;y++)for(guint x=x1;x<x2;x++){gsize p=((gsize)y*width+x)*2;g_assert_cmpuint(input[p],>=,lo);g_assert_cmpuint(input[p],<=,hi);shadow[p]=lo==hi?input[p]:255*(input[p]-lo)/(hi-lo);shadow[p+1]=input[p+1];}
}
static void normalize_gray_compare (GimpFilterLayer *filter,const guchar *expected)
{
 gsize size=(gsize)gimp_item_get_width(GIMP_ITEM(filter))*gimp_item_get_height(GIMP_ITEM(filter))*2;guchar *actual=g_malloc(size);
 gegl_buffer_get(gimp_drawable_get_buffer(GIMP_DRAWABLE(filter)),NULL,1,babl_format("Y'A u8"),actual,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
 g_assert_cmpmem(actual,size,expected,size);g_free(actual);
}
static GimpLayer *normalize_gray_source (GimpImage *image,guint width,guint height,gboolean alpha)
{
 GimpLayer *source=gimp_layer_new(image,width,height,babl_format(alpha?"Y'A u8":"Y' u8"),"Gray source",1,GIMP_LAYER_MODE_NORMAL_LEGACY);
 g_assert_true(gimp_image_add_layer(image,source,NULL,0,FALSE));return source;
}
static void normalize_gray_routes (void)
{
 for(guint spill=0;spill<2;spill++)for(guint alpha=0;alpha<2;alpha++){
  guint w=spill?1031:37,h=spill?1021:29;gsize n=(gsize)w*h;
  GimpImage *image=gimp_image_new(gimp,w,h,GIMP_GRAY,GIMP_PRECISION_U8_NON_LINEAR);GimpLayer *source=normalize_gray_source(image,w,h,alpha);
  guchar *bytes=g_malloc(n*(1+alpha)),*input=g_malloc(n*2),*expected=g_malloc(n*2);
  for(gsize i=0;i<n;i++){bytes[i*(1+alpha)]=20+(i*17)%181;if(alpha)bytes[i*2+1]=255;}
  gegl_buffer_set(gimp_drawable_get_buffer(GIMP_DRAWABLE(source)),NULL,0,babl_format(alpha?"Y'A u8":"Y' u8"),bytes,GEGL_AUTO_ROWSTRIDE);
  GimpFilterLayer *filter=normalize_new(image,NULL,w,h);max_rgb_wait(filter,GIMP_FILTER_LAYER_CLEAN);sobel_owned_reference(filter,NULL,input);
  normalize_gray_reference(input,expected,w,h,0,0,w,h);normalize_gray_compare(filter,expected);
  g_assert_cmpuint(gimp_filter_layer_get_run_count(filter),==,1);
  g_free(bytes);g_free(input);g_free(expected);g_object_unref(image);
 }
}
static void normalize_gray_context (void)
{
 const guint w=23,h=19;const gsize n=w*h;
 for(guint active=0;active<4;active++)for(guint lock=0;lock<2;lock++){
  GimpImage *image=gimp_image_new(gimp,w,h,GIMP_GRAY,GIMP_PRECISION_U8_NON_LINEAR);GimpLayer *source=normalize_gray_source(image,w,h,TRUE);
  guchar bytes[23*19*2],input[23*19*2],shadow[23*19*2],expected[23*19*2],mask[23*19];
  for(gsize i=0;i<n;i++){bytes[i*2]=20+(i*17)%181;bytes[i*2+1]=i%3?255:128;mask[i]=(i%w>=3&&i%w<w-3)?128:0;}
  gegl_buffer_set(gimp_drawable_get_buffer(GIMP_DRAWABLE(source)),NULL,0,babl_format("Y'A u8"),bytes,GEGL_AUTO_ROWSTRIDE);
  blinds_fixture_selection(image,w,h,0,0,mask,FALSE);GimpFilterLayer *filter=normalize_new(image,NULL,w,h);
  gimp_image_set_component_active(image,GIMP_CHANNEL_GRAY,active&1);gimp_image_set_component_active(image,GIMP_CHANNEL_ALPHA,active&2);
  if(lock)gimp_layer_set_lock_alpha(GIMP_LAYER(filter),TRUE,FALSE);
  max_rgb_wait(filter,GIMP_FILTER_LAYER_CLEAN);sobel_owned_reference(filter,NULL,input);normalize_gray_reference(input,shadow,w,h,3,0,w-3,h);
  max_rgb_merge_reference(input,shadow,mask,expected,n,2,lock?active&1:active);normalize_gray_compare(filter,expected);g_object_unref(image);
 }
}
static void normalize_gray_undefined_recovery (void)
{
 GimpImage *image=gimp_image_new(gimp,7,5,GIMP_GRAY,GIMP_PRECISION_U8_NON_LINEAR);GimpLayer *source=normalize_gray_source(image,7,5,TRUE);
 guchar bytes[70],input[70],expected[70];for(guint i=0;i<35;i++){bytes[i*2]=40+i;bytes[i*2+1]=255;}
 gegl_buffer_set(gimp_drawable_get_buffer(GIMP_DRAWABLE(source)),NULL,0,babl_format("Y'A u8"),bytes,GEGL_AUTO_ROWSTRIDE);
 GimpFilterLayer *filter=normalize_new(image,NULL,7,5);max_rgb_wait(filter,GIMP_FILTER_LAYER_CLEAN);GeglBuffer *cache=g_object_ref(gimp_drawable_get_buffer(GIMP_DRAWABLE(filter)));
 sobel_owned_reference(filter,NULL,input);normalize_gray_reference(input,expected,7,5,0,0,7,5);
 for(guint domain=0;domain<2;domain++){
 for(guint i=0;i<35;i++)bytes[i*2+1]=(domain&&i)?255:0;
 gegl_buffer_set(gimp_drawable_get_buffer(GIMP_DRAWABLE(source)),NULL,0,babl_format("Y'A u8"),bytes,GEGL_AUTO_ROWSTRIDE);gimp_drawable_update(GIMP_DRAWABLE(source),0,0,7,5);max_rgb_wait(filter,GIMP_FILTER_LAYER_FAILED);
 g_assert_true(cache==gimp_drawable_get_buffer(GIMP_DRAWABLE(filter)));normalize_gray_compare(filter,expected);gchar *error=gimp_filter_layer_dup_error(filter);g_assert_nonnull(strstr(error,"Normalize"));g_free(error);
 for(guint i=0;i<35;i++)bytes[i*2+1]=255;
 gegl_buffer_set(gimp_drawable_get_buffer(GIMP_DRAWABLE(source)),NULL,0,babl_format("Y'A u8"),bytes,GEGL_AUTO_ROWSTRIDE);gimp_drawable_update(GIMP_DRAWABLE(source),0,0,7,5);max_rgb_wait(filter,GIMP_FILTER_LAYER_CLEAN);normalize_gray_compare(filter,expected);
 }
 g_object_unref(cache);g_object_unref(image);
}
static void normalize_gray_cancel (void)
{
 for(guint spill=0;spill<2;spill++){
  guint w=spill?1031:257,h=spill?1021:139;gsize n=(gsize)w*h;
  GimpImage *image=gimp_image_new(gimp,w,h,GIMP_GRAY,GIMP_PRECISION_U8_NON_LINEAR);GimpLayer *source=normalize_gray_source(image,w,h,TRUE);
  guchar *bytes=g_malloc(n*2),*input=g_malloc(n*2),*expected=g_malloc(n*2);
  for(gsize i=0;i<n;i++){bytes[i*2]=20+(i*17)%181;bytes[i*2+1]=255;}
  gegl_buffer_set(gimp_drawable_get_buffer(GIMP_DRAWABLE(source)),NULL,0,babl_format("Y'A u8"),bytes,GEGL_AUTO_ROWSTRIDE);
  GimpFilterLayer *filter=normalize_new(image,NULL,w,h);
  for(gsize i=0;i<n;i++){expected[i*2]=37;expected[i*2+1]=255;}
  gegl_buffer_set(gimp_drawable_get_buffer(GIMP_DRAWABLE(filter)),NULL,0,babl_format("Y'A u8"),expected,GEGL_AUTO_ROWSTRIDE);gimp_filter_layer_mark_as_loaded(filter);
  GeglBuffer *cache=g_object_ref(gimp_drawable_get_buffer(GIMP_DRAWABLE(filter)));guint64 generation=gimp_filter_layer_get_cache_generation(filter);
  MaxRGBInterrupt interrupt={spill?GIMP_FILTER_LAYER_IMPORTING:GIMP_FILTER_LAYER_RUNNING,0,source,FALSE};
  g_signal_connect(filter,"filter-state-changed",G_CALLBACK(max_rgb_interrupt),&interrupt);gimp_filter_layer_invalidate(filter);max_rgb_wait(filter,GIMP_FILTER_LAYER_FAILED);
  g_assert_true(interrupt.called);max_rgb_drain_workers();g_assert_true(cache==gimp_drawable_get_buffer(GIMP_DRAWABLE(filter)));g_assert_cmpuint(gimp_filter_layer_get_cache_generation(filter),==,generation);normalize_gray_compare(filter,expected);g_signal_handlers_disconnect_by_data(filter,&interrupt);
  gimp_drawable_update(GIMP_DRAWABLE(source),0,0,w,h);max_rgb_wait(filter,GIMP_FILTER_LAYER_CLEAN);sobel_owned_reference(filter,NULL,input);normalize_gray_reference(input,expected,w,h,0,0,w,h);normalize_gray_compare(filter,expected);
  g_object_unref(cache);g_free(bytes);g_free(input);g_free(expected);g_object_unref(image);
 }
}
static void normalize_gray_noalpha_owner (void)
{
 GimpImage *image=gimp_image_new(gimp,4,4,GIMP_GRAY,GIMP_PRECISION_U8_NON_LINEAR);
 GimpLayer *source=normalize_gray_source(image,4,4,TRUE);fill(source,80,80,80,255);
 GimpFilterLayer *filter=GIMP_FILTER_LAYER(gimp_filter_layer_new(image,4,4,"No alpha Gray owner",1,GIMP_LAYER_MODE_NORMAL_LEGACY));
 GeglBuffer *buffer=gegl_buffer_new(GEGL_RECTANGLE(0,0,4,4),babl_format("Y' u8"));gimp_drawable_set_buffer(GIMP_DRAWABLE(filter),FALSE,NULL,buffer);g_object_unref(buffer);
 g_assert_false(gimp_drawable_has_alpha(GIMP_DRAWABLE(filter)));g_assert_true(gimp_image_add_layer(image,GIMP_LAYER(filter),NULL,0,FALSE));
 GeglBuffer *cache=g_object_ref(gimp_drawable_get_buffer(GIMP_DRAWABLE(filter)));normalize_define(filter);max_rgb_wait(filter,GIMP_FILTER_LAYER_FAILED);g_assert_true(cache==gimp_drawable_get_buffer(GIMP_DRAWABLE(filter)));g_assert_cmpuint(gimp_filter_layer_get_run_count(filter),==,0);
 g_object_unref(cache);g_object_unref(image);
}
