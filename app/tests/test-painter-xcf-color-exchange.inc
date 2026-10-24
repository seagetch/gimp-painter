/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Source-derived framing. Nonzero payloads are explicitly synthetic, not
 * claimed to survive the mismatched getter in the tested old writer host. */
static GBytes *
xcf_exchange_capsule (gint run,const guint8 values[9])
{
  static const gchar name[]="plug-in-exchange";guint8 bytes[138]={0};
  G_STATIC_ASSERT(sizeof name==17);
  put32(bytes,0,sizeof name);memcpy(bytes+4,name,sizeof name);
  put32(bytes,21,2);put32(bytes,25,4);put32(bytes,29,(guint32)run);
  put32(bytes,33,1);put32(bytes,41,10);
  for(guint i=0;i<9;i++){put32(bytes,49+i*9,4);put32(bytes,53+i*9,1);bytes[57+i*9]=values[i];}
  return g_bytes_new(bytes,sizeof bytes);
}
static void
xcf_exchange_parameters (GimpFilterLayer *filter,gint run,const guint8 values[9])
{
  GimpValueArray *args=gimp_filter_layer_dup_args(filter);gchar *name=gimp_filter_layer_dup_procedure(filter);
  g_assert_cmpstr(name,==,"plug-in-exchange");g_free(name);g_assert_nonnull(args);
  g_assert_cmpuint(gimp_value_array_length(args),==,12);
  for(guint i=0;i<3;i++)g_assert_true(G_VALUE_HOLDS_INT(gimp_value_array_index(args,i)));
  g_assert_cmpint(g_value_get_int(gimp_value_array_index(args,0)),==,run);
  g_assert_cmpint(g_value_get_int(gimp_value_array_index(args,1)),==,0);
  g_assert_cmpint(g_value_get_int(gimp_value_array_index(args,2)),==,0);
  for(guint i=0;i<9;i++){g_assert_true(G_VALUE_HOLDS_UINT(gimp_value_array_index(args,i+3)));g_assert_cmpuint(g_value_get_uint(gimp_value_array_index(args,i+3)),==,values[i]);}
  gimp_value_array_unref(args);
}
static GBytes *
xcf_exchange_reference (GBytes *source,const guint8 values[9])
{
  gsize size;const guint8 *in=g_bytes_get_data(source,&size);guint8 *out=g_memdup2(in,size);
  for(gsize p=0;p<size;p+=4){gboolean match=TRUE;
    for(guint c=0;c<3;c++)if(ABS((gint)in[p+c]-values[c])>values[6+c])match=FALSE;
    if(match)for(guint c=0;c<3;c++)out[p+c]=MIN(255,values[3+c]+ABS((gint)in[p+c]-values[c]));}
  return g_bytes_new_take(out,size);
}
static void
exchange_old_wire_admission (void)
{
  const guint8 zero[9]={0},cached[]={13,29,47,255},input[]={200,20,10,255};
  GBytes *raw=xcf_exchange_capsule(-123,zero);
  GBytes *cache=xcf_max_rgb_constant_pixels(17,13,4,cached),*source=xcf_max_rgb_constant_pixels(17,13,4,input);
  GimpImage *image=xcf_max_rgb_open_synthetic(raw,cache,source);GimpFilterLayer *filter=xcf_max_rgb_filter(image);
  xcf_exchange_parameters(filter,-123,zero);xcf_max_rgb_loaded(filter);
  xcf_max_rgb_raw_equal(filter,raw);xcf_max_rgb_pixels(GIMP_LAYER(filter),cache,4);
  g_object_unref(image);g_bytes_unref(raw);g_bytes_unref(cache);g_bytes_unref(source);
}
static void
exchange_wire_roundtrips (void)
{
  const guint8 values[][9]={{0,0,0,0,0,0,0,0,0},{200,20,10,5,100,250,3,4,5},
    {255,255,255,255,255,255,255,255,255},{80,40,20,10,110,210,255,255,255}};
  const gint run[]={-123,G_MININT32,0,G_MAXINT32};const guint8 sentinel[]={13,29,47,255};
  GBytes *cache=xcf_max_rgb_constant_pixels(17,13,4,sentinel),*source=xcf_max_rgb_source(0),*changed=xcf_max_rgb_source(1);
  for(guint i=0;i<G_N_ELEMENTS(values);i++){
    GBytes *raw=xcf_exchange_capsule(run[i],values[i]),*output=xcf_exchange_reference(changed,values[i]);
    GimpImage *image=xcf_max_rgb_open_synthetic(raw,cache,source);GimpFilterLayer *filter=xcf_max_rgb_filter(image);
    xcf_max_rgb_loaded(filter);xcf_exchange_parameters(filter,run[i],values[i]);xcf_max_rgb_raw_equal(filter,raw);xcf_max_rgb_pixels(GIMP_LAYER(filter),cache,4);
    image=xcf_max_rgb_close_reopen(image);filter=xcf_max_rgb_filter(image);
    xcf_max_rgb_loaded(filter);xcf_exchange_parameters(filter,run[i],values[i]);xcf_max_rgb_raw_equal(filter,raw);xcf_max_rgb_pixels(GIMP_LAYER(filter),cache,4);
    xcf_max_rgb_lower_input(image,4,changed);xcf_max_rgb_settle(filter,GIMP_FILTER_LAYER_CLEAN);
    g_assert_cmpuint(gimp_filter_layer_get_run_count(filter),==,1);
    xcf_exchange_parameters(filter,run[i],values[i]);xcf_max_rgb_raw_equal(filter,raw);xcf_max_rgb_pixels(GIMP_LAYER(filter),output,4);
    image=xcf_max_rgb_close_reopen(image);filter=xcf_max_rgb_filter(image);
    xcf_exchange_parameters(filter,run[i],values[i]);xcf_max_rgb_raw_equal(filter,raw);xcf_max_rgb_pixels(GIMP_LAYER(filter),output,4);
    xcf_max_rgb_loaded(filter);g_assert_cmpuint(gimp_filter_layer_get_run_count(filter),==,0);
    g_test_message("Exchange wire scene%u (%s): exact UINT bytes/raw/cache,2 Save/Open rounds,independent execution pixels",i,i?"synthetic nonzero payload":"writer-observed zero payload");
    g_object_unref(image);g_bytes_unref(raw);g_bytes_unref(output);
  }
  g_bytes_unref(cache);g_bytes_unref(source);g_bytes_unref(changed);
}
static void
exchange_malformed_wire (void)
{
  const guint8 values[]={200,20,10,5,100,250,255,255,255},sentinel[]={13,29,47,255};
  GBytes *raw=xcf_exchange_capsule(1,values),*cache=xcf_max_rgb_constant_pixels(17,13,4,sentinel);
  GBytes *source=xcf_max_rgb_source(0),*changed=xcf_max_rgb_source(1);
  for(guint mutation=0;mutation<28;mutation++){
    GBytes *variant;
    if(mutation<9){
      /* FLOAT consumes4 bytes in the historical reader, regardless of its
       * size word. Keep following records aligned: reject an unsupported
       * parameter model, not a destructively desynchronized outer file. */
      const guint8 *base=g_bytes_get_data(raw,NULL);guint at=49+9*mutation;guint8 framed[141];
      memcpy(framed,base,at);put32(framed,at,5);put32(framed,at+4,4);put32(framed,at+8,0x3f800000);
      memcpy(framed+at+12,base+at+9,138-at-9);variant=g_bytes_new(framed,sizeof framed);
    }else variant=g_bytes_ref(raw);
    guint offset;GBytes *file=xcf_max_rgb_synthetic_file(variant,cache,source,&offset);
    gsize size,retained=g_bytes_get_size(variant);const guint8 *original=g_bytes_get_data(file,&size);
    guint8 *bytes=g_memdup2(original,size);GError *error=NULL;
    if(mutation>=9&&mutation<18)put32(bytes,offset+53+9*(mutation-9),2);
    else if(mutation>=18)switch(mutation){
      case 18:put32(bytes,offset+21,5);break;
      case 19:put32(bytes,offset+33,10);break;
      case 20:put32(bytes,offset+41,1);break;
      case 21:put32(bytes,offset+25,3);break;
      case 22:put32(bytes,offset+134,1);break;
      case 23:bytes[offset+20]='x';break;
      case 24:bytes[offset+12]=0;break;
      case 25:put32(bytes,offset-4,137);break;
      case 26:put32(bytes,offset-4,150);break;
      case 27:bytes[offset+4]='P';break;
    }
    GimpImage *image=open_bytes(bytes,size,XCF_PAINTER_DIALECT_LEGACY,&error);g_assert_no_error(error);g_assert_nonnull(image);
    GimpFilterLayer *filter=xcf_max_rgb_filter(image);GBytes *preserved=g_bytes_new(bytes+offset,retained);
    g_assert_null(gimp_filter_layer_dup_args(filter));xcf_max_rgb_loaded(filter);xcf_max_rgb_raw_equal(filter,preserved);xcf_max_rgb_pixels(GIMP_LAYER(filter),cache,4);
    image=xcf_max_rgb_close_reopen(image);filter=xcf_max_rgb_filter(image);
    g_assert_null(gimp_filter_layer_dup_args(filter));xcf_max_rgb_loaded(filter);xcf_max_rgb_raw_equal(filter,preserved);xcf_max_rgb_pixels(GIMP_LAYER(filter),cache,4);
    xcf_max_rgb_lower_input(image,4,changed);xcf_max_rgb_settle(filter,GIMP_FILTER_LAYER_FAILED);
    g_assert_cmpuint(gimp_filter_layer_get_run_count(filter),==,0);xcf_max_rgb_raw_equal(filter,preserved);xcf_max_rgb_pixels(GIMP_LAYER(filter),cache,4);
    image=xcf_max_rgb_close_reopen(image);filter=xcf_max_rgb_filter(image);
    g_assert_null(gimp_filter_layer_dup_args(filter));xcf_max_rgb_settle(filter,GIMP_FILTER_LAYER_FAILED);
    g_assert_cmpuint(gimp_filter_layer_get_run_count(filter),==,0);xcf_max_rgb_raw_equal(filter,preserved);xcf_max_rgb_pixels(GIMP_LAYER(filter),cache,4);
    g_test_message("Exchange malformed%u: opaque bytes/cache and2 Save/Open rounds,zero executions",mutation);
    g_object_unref(image);g_bytes_unref(preserved);g_free(bytes);g_bytes_unref(file);g_bytes_unref(variant);
  }
  g_bytes_unref(raw);g_bytes_unref(cache);g_bytes_unref(source);g_bytes_unref(changed);
}

static void
exchange_native_model_precedence (void)
{
  const guint8 zero[9]={0},values[]={80,40,20,10,110,210,255,255,255},sentinel[]={13,29,47,255};
  GBytes *raw=xcf_exchange_capsule(-123,zero),*cache=xcf_max_rgb_constant_pixels(17,13,4,sentinel);
  GBytes *source=xcf_max_rgb_source(0),*changed=xcf_max_rgb_source(1);
  GBytes *first=xcf_exchange_reference(source,values),*second=xcf_exchange_reference(changed,values);
  GimpImage *image=xcf_max_rgb_open_synthetic(raw,cache,source);GimpFilterLayer *filter=xcf_max_rgb_filter(image);
  GimpValueArray *args=gimp_filter_layer_dup_args(filter);g_assert_nonnull(args);
  for(guint i=0;i<9;i++)g_value_set_uint(gimp_value_array_index(args,i+3),values[i]);
  g_assert_true(gimp_filter_layer_set_definition(filter,"plug-in-exchange",raw,args,NULL));gimp_value_array_unref(args);
  xcf_max_rgb_settle(filter,GIMP_FILTER_LAYER_CLEAN);xcf_exchange_parameters(filter,-123,values);
  xcf_max_rgb_raw_equal(filter,raw);xcf_max_rgb_pixels(GIMP_LAYER(filter),first,4);
  image=xcf_max_rgb_close_reopen(image);filter=xcf_max_rgb_filter(image);
  xcf_max_rgb_loaded(filter);xcf_exchange_parameters(filter,-123,values);xcf_max_rgb_raw_equal(filter,raw);xcf_max_rgb_pixels(GIMP_LAYER(filter),first,4);
  xcf_max_rgb_lower_input(image,4,changed);xcf_max_rgb_settle(filter,GIMP_FILTER_LAYER_CLEAN);
  xcf_max_rgb_pixels(GIMP_LAYER(filter),second,4);
  image=xcf_max_rgb_close_reopen(image);filter=xcf_max_rgb_filter(image);
  xcf_max_rgb_loaded(filter);xcf_exchange_parameters(filter,-123,values);xcf_max_rgb_raw_equal(filter,raw);xcf_max_rgb_pixels(GIMP_LAYER(filter),second,4);
  /* An explicit but unsupported native model is not silently narrowed to a
   * byte, and the older rawzero capsule must not replace it on reload. */
  args=gimp_filter_layer_dup_args(filter);g_value_set_uint(gimp_value_array_index(args,3),256);
  g_assert_true(gimp_filter_layer_set_definition(filter,"plug-in-exchange",raw,args,NULL));gimp_value_array_unref(args);
  xcf_max_rgb_settle(filter,GIMP_FILTER_LAYER_FAILED);xcf_max_rgb_pixels(GIMP_LAYER(filter),second,4);
  image=xcf_max_rgb_close_reopen(image);filter=xcf_max_rgb_filter(image);
  args=gimp_filter_layer_dup_args(filter);g_assert_nonnull(args);g_assert_cmpuint(gimp_value_array_length(args),==,12);
  g_assert_true(G_VALUE_HOLDS_UINT(gimp_value_array_index(args,3)));g_assert_cmpuint(g_value_get_uint(gimp_value_array_index(args,3)),==,256);gimp_value_array_unref(args);
  xcf_max_rgb_settle(filter,GIMP_FILTER_LAYER_FAILED);g_assert_cmpuint(gimp_filter_layer_get_run_count(filter),==,0);
  xcf_max_rgb_raw_equal(filter,raw);xcf_max_rgb_pixels(GIMP_LAYER(filter),second,4);
  g_object_unref(image);g_bytes_unref(raw);g_bytes_unref(cache);g_bytes_unref(source);g_bytes_unref(changed);g_bytes_unref(first);g_bytes_unref(second);
}
