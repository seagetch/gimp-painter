/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Source-derived exact writer schema, not genuine old-application captures. */
static GBytes *xcf_normalize_capsule (gint run)
{
 static const gchar name[]="plug-in-normalize";guint8 bytes[58]={0};G_STATIC_ASSERT(sizeof name==18);
 put32(bytes,0,sizeof name);memcpy(bytes+4,name,sizeof name);
 put32(bytes,22,2);put32(bytes,26,4);put32(bytes,30,(guint32)run);put32(bytes,34,1);put32(bytes,42,10);
 return g_bytes_new(bytes,sizeof bytes);
}
static void xcf_normalize_parameters (GimpFilterLayer *filter,gint run,gint image,gint drawable)
{
 GimpValueArray *args=gimp_filter_layer_dup_args(filter);gchar *name=gimp_filter_layer_dup_procedure(filter);
 g_assert_cmpstr(name,==,"plug-in-normalize");g_free(name);g_assert_nonnull(args);g_assert_cmpuint(gimp_value_array_length(args),==,3);
 for(guint i=0;i<3;i++)g_assert_true(G_VALUE_HOLDS_INT(gimp_value_array_index(args,i)));
 g_assert_cmpint(g_value_get_int(gimp_value_array_index(args,0)),==,run);g_assert_cmpint(g_value_get_int(gimp_value_array_index(args,1)),==,image);g_assert_cmpint(g_value_get_int(gimp_value_array_index(args,2)),==,drawable);gimp_value_array_unref(args);
}
static GBytes *xcf_normalize_source (guint variant)
{
 guint8 bytes[17*13*4];
 for(guint i=0;i<17*13;i++){bytes[i*4]=20+(i*17+variant*13)%121;bytes[i*4+1]=30+(i*23+variant*19)%111;bytes[i*4+2]=40+(i*29+variant*7)%101;bytes[i*4+3]=255;}
 return g_bytes_new(bytes,sizeof bytes);
}
static GBytes *xcf_normalize_reference (GBytes *source)
{
 gsize size;const guint8 *in=g_bytes_get_data(source,&size);guint8 *out=g_memdup2(in,size);gint lo=255,hi=0;
 for(gsize p=0;p<size;p+=4){g_assert_cmpuint(in[p+3],==,255);for(guint c=0;c<3;c++){lo=MIN(lo,in[p+c]);hi=MAX(hi,in[p+c]);}}
 for(gsize p=0;p<size;p+=4)for(guint c=0;c<3;c++)out[p+c]=hi==lo?in[p+c]:255*(in[p+c]-lo)/(hi-lo);
 return g_bytes_new_take(out,size);
}
static void normalize_old_wire_admission (void)
{
 const gint runs[]={G_MININT32,-123,-1,0,1,2,123,G_MAXINT32};const guint8 sentinel[]={13,29,47,255};
 GBytes *cache=xcf_max_rgb_constant_pixels(17,13,4,sentinel),*source=xcf_normalize_source(0);
 for(guint i=0;i<G_N_ELEMENTS(runs);i++){
  GBytes *raw=xcf_normalize_capsule(runs[i]);GimpImage *image=xcf_max_rgb_open_synthetic(raw,cache,source);GimpFilterLayer *filter=xcf_max_rgb_filter(image);
  xcf_normalize_parameters(filter,runs[i],0,0);xcf_max_rgb_loaded(filter);xcf_max_rgb_raw_equal(filter,raw);xcf_max_rgb_pixels(GIMP_LAYER(filter),cache,4);g_object_unref(image);g_bytes_unref(raw);
 }
 g_bytes_unref(cache);g_bytes_unref(source);
}
static void normalize_wire_roundtrips (void)
{
 const gint runs[]={G_MININT32,-123,0,G_MAXINT32};const guint8 sentinel[]={13,29,47,255};
 GBytes *cache=xcf_max_rgb_constant_pixels(17,13,4,sentinel),*source=xcf_normalize_source(0),*changed=xcf_normalize_source(1),*expected=xcf_normalize_reference(changed);
 for(guint i=0;i<G_N_ELEMENTS(runs);i++){
  GBytes *raw=xcf_normalize_capsule(runs[i]);GimpImage *image=xcf_max_rgb_open_synthetic(raw,cache,source);GimpFilterLayer *filter=xcf_max_rgb_filter(image);
  xcf_normalize_parameters(filter,runs[i],0,0);xcf_max_rgb_loaded(filter);xcf_max_rgb_raw_equal(filter,raw);xcf_max_rgb_pixels(GIMP_LAYER(filter),cache,4);
  image=xcf_max_rgb_close_reopen(image);filter=xcf_max_rgb_filter(image);xcf_normalize_parameters(filter,runs[i],0,0);xcf_max_rgb_loaded(filter);xcf_max_rgb_raw_equal(filter,raw);xcf_max_rgb_pixels(GIMP_LAYER(filter),cache,4);
  xcf_max_rgb_lower_input(image,4,changed);xcf_max_rgb_settle(filter,GIMP_FILTER_LAYER_CLEAN);g_assert_cmpuint(gimp_filter_layer_get_run_count(filter),==,1);xcf_max_rgb_raw_equal(filter,raw);xcf_max_rgb_pixels(GIMP_LAYER(filter),expected,4);
  image=xcf_max_rgb_close_reopen(image);filter=xcf_max_rgb_filter(image);xcf_normalize_parameters(filter,runs[i],0,0);xcf_max_rgb_loaded(filter);xcf_max_rgb_raw_equal(filter,raw);xcf_max_rgb_pixels(GIMP_LAYER(filter),expected,4);
  g_test_message("Normalize signed-run scene%u: source-derived58-byte raw/cache and2 Save/Open rounds,independent defined-domain execution",i);g_object_unref(image);g_bytes_unref(raw);
 }
 g_bytes_unref(cache);g_bytes_unref(source);g_bytes_unref(changed);g_bytes_unref(expected);
}
static void normalize_malformed_wire (void)
{
 const guint8 sentinel[]={13,29,47,255};GBytes *raw=xcf_normalize_capsule(-123),*cache=xcf_max_rgb_constant_pixels(17,13,4,sentinel),*source=xcf_normalize_source(0),*changed=xcf_normalize_source(1);
 for(guint mutation=0;mutation<12;mutation++){
  GBytes *variant=g_bytes_ref(raw);
  if(mutation==11){guint8 bytes[70];const guint8 *base=g_bytes_get_data(raw,NULL);memcpy(bytes,base,50);put32(bytes,50,2);put32(bytes,54,4);put32(bytes,58,1);memcpy(bytes+62,base+50,8);g_bytes_unref(variant);variant=g_bytes_new(bytes,sizeof bytes);}
  guint offset;GBytes *file=xcf_max_rgb_synthetic_file(variant,cache,source,&offset);gsize size,retained=g_bytes_get_size(variant);const guint8 *original=g_bytes_get_data(file,&size);guint8 *bytes=g_memdup2(original,size);GError *error=NULL;
  switch(mutation){
   case 0:put32(bytes,offset+22,5);break; /* wrong run tag, aligned float payload */
   case 1:put32(bytes,offset+34,10);break;
   case 2:put32(bytes,offset+42,1);break;
   case 3:put32(bytes,offset+26,3);break;
   case 4:put32(bytes,offset+38,1);break;
   case 5:put32(bytes,offset+46,1);break;
   case 6:put32(bytes,offset+54,1);break;
   case 7:bytes[offset+21]='x';break;
   case 8:bytes[offset+12]=0;break;
   case 9:put32(bytes,offset-4,57);break;
   case 10:put32(bytes,offset-4,70);break;
  }
  GimpImage *image=open_bytes(bytes,size,XCF_PAINTER_DIALECT_LEGACY,&error);g_assert_no_error(error);g_assert_nonnull(image);GimpFilterLayer *filter=xcf_max_rgb_filter(image);GBytes *preserved=g_bytes_new(bytes+offset,retained);
  g_assert_null(gimp_filter_layer_dup_args(filter));xcf_max_rgb_loaded(filter);xcf_max_rgb_raw_equal(filter,preserved);xcf_max_rgb_pixels(GIMP_LAYER(filter),cache,4);
  image=xcf_max_rgb_close_reopen(image);filter=xcf_max_rgb_filter(image);g_assert_null(gimp_filter_layer_dup_args(filter));xcf_max_rgb_loaded(filter);xcf_max_rgb_raw_equal(filter,preserved);xcf_max_rgb_pixels(GIMP_LAYER(filter),cache,4);
  xcf_max_rgb_lower_input(image,4,changed);xcf_max_rgb_settle(filter,GIMP_FILTER_LAYER_FAILED);g_assert_cmpuint(gimp_filter_layer_get_run_count(filter),==,0);xcf_max_rgb_raw_equal(filter,preserved);xcf_max_rgb_pixels(GIMP_LAYER(filter),cache,4);
  image=xcf_max_rgb_close_reopen(image);filter=xcf_max_rgb_filter(image);g_assert_null(gimp_filter_layer_dup_args(filter));xcf_max_rgb_settle(filter,GIMP_FILTER_LAYER_FAILED);g_assert_cmpuint(gimp_filter_layer_get_run_count(filter),==,0);xcf_max_rgb_raw_equal(filter,preserved);xcf_max_rgb_pixels(GIMP_LAYER(filter),cache,4);
  g_test_message("Normalize malformed%u: opaque/cache2 Save/Open rounds,zero executions",mutation);g_object_unref(image);g_bytes_unref(preserved);g_free(bytes);g_bytes_unref(file);g_bytes_unref(variant);
 }
 g_bytes_unref(raw);g_bytes_unref(cache);g_bytes_unref(source);g_bytes_unref(changed);
}
static void normalize_native_model_precedence (void)
{
 const guint8 sentinel[]={13,29,47,255};GBytes *raw=xcf_normalize_capsule(-123),*cache=xcf_max_rgb_constant_pixels(17,13,4,sentinel),*source=xcf_normalize_source(0),*expected=xcf_normalize_reference(source);
 GimpImage *image=xcf_max_rgb_open_synthetic(raw,cache,source);GimpFilterLayer *filter=xcf_max_rgb_filter(image);GimpValueArray *args=gimp_value_array_new_from_types(NULL,G_TYPE_INT,42,G_TYPE_INT,1234,G_TYPE_INT,5678,G_TYPE_NONE);
 g_assert_true(gimp_filter_layer_set_definition(filter,"plug-in-normalize",raw,args,NULL));gimp_value_array_unref(args);xcf_max_rgb_settle(filter,GIMP_FILTER_LAYER_CLEAN);xcf_normalize_parameters(filter,42,1234,5678);xcf_max_rgb_raw_equal(filter,raw);xcf_max_rgb_pixels(GIMP_LAYER(filter),expected,4);
 image=xcf_max_rgb_close_reopen(image);filter=xcf_max_rgb_filter(image);xcf_max_rgb_loaded(filter);xcf_normalize_parameters(filter,42,1234,5678);xcf_max_rgb_raw_equal(filter,raw);xcf_max_rgb_pixels(GIMP_LAYER(filter),expected,4);
 args=gimp_value_array_new_from_types(NULL,G_TYPE_INT,42,G_TYPE_INT,1234,G_TYPE_INT,5678,G_TYPE_INT,99,G_TYPE_NONE);g_assert_true(gimp_filter_layer_set_definition(filter,"plug-in-normalize",raw,args,NULL));gimp_value_array_unref(args);xcf_max_rgb_settle(filter,GIMP_FILTER_LAYER_FAILED);xcf_max_rgb_pixels(GIMP_LAYER(filter),expected,4);
 image=xcf_max_rgb_close_reopen(image);filter=xcf_max_rgb_filter(image);args=gimp_filter_layer_dup_args(filter);g_assert_nonnull(args);g_assert_cmpuint(gimp_value_array_length(args),==,4);g_assert_true(G_VALUE_HOLDS_INT(gimp_value_array_index(args,3)));g_assert_cmpint(g_value_get_int(gimp_value_array_index(args,3)),==,99);gimp_value_array_unref(args);xcf_max_rgb_settle(filter,GIMP_FILTER_LAYER_FAILED);g_assert_cmpuint(gimp_filter_layer_get_run_count(filter),==,0);xcf_max_rgb_raw_equal(filter,raw);xcf_max_rgb_pixels(GIMP_LAYER(filter),expected,4);
 g_object_unref(image);g_bytes_unref(raw);g_bytes_unref(cache);g_bytes_unref(source);g_bytes_unref(expected);
}
static void normalize_undefined_cache_roundtrip (void)
{
 const guint8 sentinel[]={13,29,47,255},transparent[]={0,0,0,0};GBytes *raw=xcf_normalize_capsule(0),*cache=xcf_max_rgb_constant_pixels(17,13,4,sentinel),*source=xcf_normalize_source(0),*empty=xcf_max_rgb_constant_pixels(17,13,4,transparent),*expected=xcf_normalize_reference(source);
 GimpImage *image=xcf_max_rgb_open_synthetic(raw,cache,source);GimpFilterLayer *filter=xcf_max_rgb_filter(image);xcf_max_rgb_lower_input(image,4,empty);xcf_max_rgb_settle(filter,GIMP_FILTER_LAYER_FAILED);g_assert_cmpuint(gimp_filter_layer_get_run_count(filter),==,1);xcf_max_rgb_raw_equal(filter,raw);xcf_max_rgb_pixels(GIMP_LAYER(filter),cache,4);gchar *message=gimp_filter_layer_dup_error(filter);g_assert_nonnull(message);g_assert_nonnull(strstr(message,"no nonzero-alpha samples"));g_free(message);
 image=xcf_max_rgb_close_reopen(image);filter=xcf_max_rgb_filter(image);xcf_normalize_parameters(filter,0,0,0);xcf_max_rgb_settle(filter,GIMP_FILTER_LAYER_FAILED);xcf_max_rgb_raw_equal(filter,raw);xcf_max_rgb_pixels(GIMP_LAYER(filter),cache,4);
 xcf_max_rgb_lower_input(image,4,source);xcf_max_rgb_settle(filter,GIMP_FILTER_LAYER_CLEAN);xcf_max_rgb_pixels(GIMP_LAYER(filter),expected,4);image=xcf_max_rgb_close_reopen(image);filter=xcf_max_rgb_filter(image);xcf_max_rgb_loaded(filter);xcf_max_rgb_raw_equal(filter,raw);xcf_max_rgb_pixels(GIMP_LAYER(filter),expected,4);
 g_object_unref(image);g_bytes_unref(raw);g_bytes_unref(cache);g_bytes_unref(source);g_bytes_unref(empty);g_bytes_unref(expected);
}
