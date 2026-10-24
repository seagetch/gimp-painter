/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Source-derived old framing, not a genuine old-application capture. */
static GBytes *
xcf_red_eye_capsule (gint run_mode, gint threshold)
{
  static const gchar name[]="plug-in-red-eye-removal";
  guint8 bytes[76]={0};
  G_STATIC_ASSERT(sizeof name==24);
  put32(bytes,0,sizeof name);memcpy(bytes+4,name,sizeof name);
  put32(bytes,28,2);put32(bytes,32,4);put32(bytes,36,(guint32)run_mode);
  put32(bytes,40,1);put32(bytes,48,10);
  put32(bytes,56,2);put32(bytes,60,4);put32(bytes,64,(guint32)threshold);
  return g_bytes_new(bytes,sizeof bytes);
}
static void
red_eye_old_wire_admission (void)
{
  const guint8 cached[]={13,29,47,255},input[]={200,20,10,255};
  GBytes *raw=xcf_red_eye_capsule(-123,50);
  GBytes *cache=xcf_max_rgb_constant_pixels(17,13,4,cached);
  GBytes *source=xcf_max_rgb_constant_pixels(17,13,4,input);
  GimpImage *image=xcf_max_rgb_open_synthetic(raw,cache,source);
  GimpFilterLayer *filter=xcf_max_rgb_filter(image);
  GimpValueArray *args=gimp_filter_layer_dup_args(filter);
  g_assert_nonnull(args);g_assert_cmpuint(gimp_value_array_length(args),==,4);
  g_assert_cmpint(g_value_get_int(gimp_value_array_index(args,0)),==,-123);
  g_assert_cmpint(g_value_get_int(gimp_value_array_index(args,3)),==,50);
  xcf_max_rgb_raw_equal(filter,raw);xcf_max_rgb_pixels(GIMP_LAYER(filter),cache,4);
  gimp_value_array_unref(args);g_object_unref(image);g_bytes_unref(raw);g_bytes_unref(cache);g_bytes_unref(source);
}
static void
xcf_red_eye_parameters (GimpFilterLayer *filter,gint run,gint threshold)
{
  GimpValueArray *args=gimp_filter_layer_dup_args(filter);gchar *name=gimp_filter_layer_dup_procedure(filter);
  g_assert_cmpstr(name,==,"plug-in-red-eye-removal");g_free(name);g_assert_nonnull(args);
  g_assert_cmpuint(gimp_value_array_length(args),==,4);
  for(guint i=0;i<4;i++)g_assert_true(G_VALUE_HOLDS_INT(gimp_value_array_index(args,i)));
  g_assert_cmpint(g_value_get_int(gimp_value_array_index(args,0)),==,run);
  g_assert_cmpint(g_value_get_int(gimp_value_array_index(args,1)),==,0);
  g_assert_cmpint(g_value_get_int(gimp_value_array_index(args,2)),==,0);
  g_assert_cmpint(g_value_get_int(gimp_value_array_index(args,3)),==,threshold);gimp_value_array_unref(args);
}
static GBytes *
xcf_red_eye_reference (GBytes *source,gint threshold)
{
  gsize size;const guint8 *in=g_bytes_get_data(source,&size);guint8 *out=g_memdup2(in,size);
  for(gsize p=0;p<size;p+=4){gint64 r=(guint64)in[p]*5133333/10000000,b=(guint64)in[p+2]*1933333/10000000,g=in[p+1],a=((gint64)threshold-50)*2;
    g_assert_cmpuint(in[p+3],==,255);
    if(r>=g-a&&r>=b-a)out[p]=MIN(G_GUINT64_CONSTANT(255),(g+b)*10000000/10266666);}
  return g_bytes_new_take(out,size);
}
static void
red_eye_old_wire_roundtrips (void)
{
  const gint values[]={G_MININT32,-1073741646,-1,0,50,100,1073741873,G_MAXINT32};
  const guint8 sentinel[]={13,29,47,255};GBytes *cache=xcf_max_rgb_constant_pixels(17,13,4,sentinel);
  GBytes *source=xcf_max_rgb_source(0),*changed=xcf_max_rgb_source(1);
  for(guint i=0;i<G_N_ELEMENTS(values);i++){
    gint run=values[7-i],threshold=values[i];gboolean valid=i!=0&&i!=7;
    GBytes *raw=xcf_red_eye_capsule(run,threshold),*output=valid?xcf_red_eye_reference(changed,threshold):g_bytes_ref(cache);
    GimpImage *image=xcf_max_rgb_open_synthetic(raw,cache,source);GimpFilterLayer *filter=xcf_max_rgb_filter(image);
    xcf_max_rgb_loaded(filter);xcf_red_eye_parameters(filter,run,threshold);xcf_max_rgb_raw_equal(filter,raw);xcf_max_rgb_pixels(GIMP_LAYER(filter),cache,4);
    image=xcf_max_rgb_close_reopen(image);filter=xcf_max_rgb_filter(image);
    xcf_max_rgb_loaded(filter);xcf_red_eye_parameters(filter,run,threshold);xcf_max_rgb_raw_equal(filter,raw);xcf_max_rgb_pixels(GIMP_LAYER(filter),cache,4);
    xcf_max_rgb_lower_input(image,4,changed);xcf_max_rgb_settle(filter,valid?GIMP_FILTER_LAYER_CLEAN:GIMP_FILTER_LAYER_FAILED);
    g_assert_cmpuint(gimp_filter_layer_get_run_count(filter),==,valid?1:0);
    xcf_red_eye_parameters(filter,run,threshold);xcf_max_rgb_raw_equal(filter,raw);xcf_max_rgb_pixels(GIMP_LAYER(filter),output,4);
    image=xcf_max_rgb_close_reopen(image);filter=xcf_max_rgb_filter(image);
    xcf_red_eye_parameters(filter,run,threshold);xcf_max_rgb_raw_equal(filter,raw);xcf_max_rgb_pixels(GIMP_LAYER(filter),output,4);
    if(valid)xcf_max_rgb_loaded(filter);else xcf_max_rgb_settle(filter,GIMP_FILTER_LAYER_FAILED);
    g_assert_cmpuint(gimp_filter_layer_get_run_count(filter),==,0);
    g_test_message("Red Eye wire scene%u: exact signed scalar bits,raw/cache;two native Save/Open rounds;execution %s",i,valid?"rational match":"explicit rejection");
    g_object_unref(image);g_bytes_unref(raw);g_bytes_unref(output);
  }
  g_bytes_unref(cache);g_bytes_unref(source);g_bytes_unref(changed);
}
static void
red_eye_malformed_wire (void)
{
  const guint8 sentinel[]={13,29,47,255};GBytes *raw=xcf_red_eye_capsule(1,50);
  GBytes *cache=xcf_max_rgb_constant_pixels(17,13,4,sentinel),*source=xcf_max_rgb_source(0),*changed=xcf_max_rgb_source(1);
  guint offset;GBytes *file=xcf_max_rgb_synthetic_file(raw,cache,source,&offset);gsize size;const guint8 *original=g_bytes_get_data(file,&size);
  for(guint mutation=0;mutation<16;mutation++){
    guint8 *bytes=g_memdup2(original,size);gsize retained=76;GError *error=NULL;
    switch(mutation){
      case 0:put32(bytes,offset+28,5);break;
      case 1:put32(bytes,offset+40,10);break;
      case 2:put32(bytes,offset+48,1);break;
      case 3:put32(bytes,offset+56,5);break;
      case 4:put32(bytes,offset+32,3);break;
      case 5:put32(bytes,offset+44,4);break;
      case 6:put32(bytes,offset+52,4);break;
      case 7:put32(bytes,offset+60,3);break;
      case 8:put32(bytes,offset+72,1);break;
      case 9:bytes[offset+27]='x';break;
      case 10:bytes[offset+12]=0;break;
      case 11:put32(bytes,offset-4,75);break;
      case 12:put32(bytes,offset-4,88);break;
      case 13:for(guint i=0;i<5;i++){put32(bytes,offset+28+i*8,1);put32(bytes,offset+32+i*8,0);}break;
      case 14:put32(bytes,offset-4,64);put32(bytes,offset+56,0);put32(bytes,offset+60,0);put32(bytes,offset+64,8);put32(bytes,offset+68,4);put32(bytes,offset+72,1);retained=64;break;
      case 15:bytes[offset+4]='P';break;
    }
    GimpImage *image=open_bytes(bytes,size,XCF_PAINTER_DIALECT_LEGACY,&error);g_assert_no_error(error);g_assert_nonnull(image);
    GimpFilterLayer *filter=xcf_max_rgb_filter(image);GBytes *preserved=g_bytes_new(bytes+offset,retained);
    g_assert_null(gimp_filter_layer_dup_args(filter));xcf_max_rgb_loaded(filter);xcf_max_rgb_raw_equal(filter,preserved);xcf_max_rgb_pixels(GIMP_LAYER(filter),cache,4);
    image=xcf_max_rgb_close_reopen(image);filter=xcf_max_rgb_filter(image);
    g_assert_null(gimp_filter_layer_dup_args(filter));xcf_max_rgb_loaded(filter);xcf_max_rgb_raw_equal(filter,preserved);xcf_max_rgb_pixels(GIMP_LAYER(filter),cache,4);
    xcf_max_rgb_lower_input(image,4,changed);xcf_max_rgb_settle(filter,GIMP_FILTER_LAYER_FAILED);g_assert_cmpuint(gimp_filter_layer_get_run_count(filter),==,0);
    xcf_max_rgb_raw_equal(filter,preserved);xcf_max_rgb_pixels(GIMP_LAYER(filter),cache,4);
    image=xcf_max_rgb_close_reopen(image);filter=xcf_max_rgb_filter(image);
    g_assert_null(gimp_filter_layer_dup_args(filter));xcf_max_rgb_settle(filter,GIMP_FILTER_LAYER_FAILED);g_assert_cmpuint(gimp_filter_layer_get_run_count(filter),==,0);
    xcf_max_rgb_raw_equal(filter,preserved);xcf_max_rgb_pixels(GIMP_LAYER(filter),cache,4);
    g_test_message("Red Eye malformed%u: exact opaque bytes/cache;two Save/Open rounds;zero executions",mutation);
    g_object_unref(image);g_bytes_unref(preserved);g_free(bytes);
  }
  g_bytes_unref(raw);g_bytes_unref(cache);g_bytes_unref(source);g_bytes_unref(changed);g_bytes_unref(file);
}
