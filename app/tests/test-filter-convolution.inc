/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Programmatic typed-definition editing, not a GTK numeric-array editor. */
static const gdouble convolution_identity[25] = {0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0};
static const gint32 convolution_all[5] = {1,1,1,1,1};
static GimpValueArray *
convolution_args (const gdouble matrix[25], const gint32 channels[5], gdouble divisor, gdouble offset, gint border)
{
  GimpValueArray *args = gimp_value_array_new_from_types (NULL,
    G_TYPE_INT,1,G_TYPE_INT,123,G_TYPE_INT,456,G_TYPE_INT,25,GIMP_TYPE_DOUBLE_ARRAY,NULL,
    G_TYPE_INT,1,G_TYPE_DOUBLE,divisor,G_TYPE_DOUBLE,offset,G_TYPE_INT,5,GIMP_TYPE_INT32_ARRAY,NULL,
    G_TYPE_INT,border,G_TYPE_NONE);
  gimp_value_set_double_array (gimp_value_array_index (args,4),matrix,25);
  gimp_value_set_int32_array (gimp_value_array_index (args,9),channels,5);
  return args;
}
static void convolution_define (GimpFilterLayer *filter, GimpValueArray *args)
{
  static const guchar record[] = {0,0xff,7,0,0,0,0,0xfe};
  GBytes *raw = g_bytes_new_static (record,sizeof record);
  g_assert_true (gimp_filter_layer_set_definition (filter,"plug-in-convmatrix",raw,args,NULL));
  g_bytes_unref (raw);
}
static void convolution_assert_args (GimpFilterLayer *filter, GimpValueArray *expected)
{
  GimpValueArray *actual = gimp_filter_layer_dup_args (filter);
  gchar *name = gimp_filter_layer_dup_procedure (filter);
  g_assert_cmpstr (name, ==, "plug-in-convmatrix"); g_free (name);
  g_assert_nonnull (actual); g_assert_cmpuint (gimp_value_array_length (actual), ==, gimp_value_array_length (expected));
  for (guint i = 0; i < gimp_value_array_length (expected); ++i) {
    const GValue *a = gimp_value_array_index (actual,i), *b = gimp_value_array_index (expected,i);
    g_assert_cmpuint (G_VALUE_TYPE (a), ==, G_VALUE_TYPE (b));
    if (G_VALUE_HOLDS_INT (a)) g_assert_cmpint (g_value_get_int (a), ==, g_value_get_int (b));
    else if (G_VALUE_HOLDS_DOUBLE (a)) {
      const gdouble x = g_value_get_double (a), y = g_value_get_double (b);
      g_assert_cmpmem (&x,8,&y,8);
    } else if (G_VALUE_HOLDS_STRING (a)) g_assert_cmpstr (g_value_get_string (a), ==, g_value_get_string (b));
    else {
      const GimpArray *x = g_value_get_boxed (a), *y = g_value_get_boxed (b);
      g_assert_cmpmem (x->data,x->length,y->data,y->length);
    }
  }
  gimp_value_array_unref (actual);
}
static void convolution_actual_old_live (void)
{
  const gchar *dir = g_getenv ("GIMP_PAINTER_CONVOLUTION_FIXTURES");
  gchar *path = g_build_filename (dir,"cases.tsv",NULL), *table = NULL, *pixels = NULL;
  gchar **rows; gsize size; guint compared = 0;
  g_assert_true (g_file_get_contents (path,&table,NULL,NULL)); g_free (path);
  path = g_build_filename (dir,"pixels.bin",NULL);
  g_assert_true (g_file_get_contents (path,&pixels,&size,NULL)); g_free (path);
  rows = g_strsplit (table,"\n",-1);
  for (guint i=0; rows[i] && *rows[i]; ++i) {
    gchar **v = g_strsplit (rows[i],"\t",-1);
    if (!atoi(v[5])) { g_strfreev(v); continue; }
    const gint w=atoi(v[1]), h=atoi(v[2]), components=atoi(v[3]);
    const gboolean gray=components==2;
    const Babl *format=babl_format(gray?"Y'A u8":"R'G'B'A u8");
    const guint source_at=g_ascii_strtoull(v[47],NULL,10), mask_at=g_ascii_strtoull(v[53],NULL,10);
    const guint wanted_at=g_ascii_strtoull(v[55],NULL,10);
    gdouble matrix[25]; gint32 channels[5];
    for(guint j=0;j<5;++j) channels[j]=atoi(v[15+j]);
    for(guint j=0;j<25;++j) matrix[j]=g_ascii_strtod(v[20+j],NULL);
    GimpImage *image=gimp_image_new(gimp,w,h,gray?GIMP_GRAY:GIMP_RGB,GIMP_PRECISION_U8_NON_LINEAR);
    GimpLayer *source=gimp_layer_new(image,w,h,format,"source",1,GIMP_LAYER_MODE_NORMAL_LEGACY);
    gimp_image_add_layer(image,source,NULL,0,FALSE);
    gegl_buffer_set(gimp_drawable_get_buffer(GIMP_DRAWABLE(source)),NULL,0,format,pixels+source_at,GEGL_AUTO_ROWSTRIDE);
    const gint selection=atoi(strstr(v[0],"-s")+2), final=atoi(strstr(v[0],"-f")+2);
    if(selection) {
      /* The initial soft fixture is also the final mask unless a context-only
       * edit replaces it during RUNNING. Hard initial bounds are explicit. */
      if(selection==1) gimp_channel_select_rectangle(gimp_image_get_mask(image),atoi(v[7]),atoi(v[8]),
        atoi(v[9])-atoi(v[7]),atoi(v[10])-atoi(v[8]),GIMP_CHANNEL_OP_REPLACE,FALSE,0,0,FALSE);
      else blinds_fixture_selection(image,w,h,0,0,(guchar *)pixels+mask_at,FALSE);
    }
    GimpFilterLayer *filter=filter_new(image,NULL,w,h);
    GimpValueArray *args=convolution_args(matrix,channels,g_ascii_strtod(v[12],NULL),g_ascii_strtod(v[13],NULL),atoi(v[14]));
    g_value_set_int(gimp_value_array_index(args,5),atoi(v[11])); convolution_define(filter,args);
    if(final) { blinds_wait(filter,GIMP_FILTER_LAYER_RUNNING); blinds_fixture_selection(image,w,h,0,0,(guchar *)pixels+mask_at,FALSE); }
    settle(filter);
    guchar *actual=g_malloc(w*h*components);
    gegl_buffer_get(gimp_drawable_get_buffer(GIMP_DRAWABLE(filter)),NULL,1,format,actual,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
    g_test_message("Old Convolution scene %s",v[0]);
    g_assert_cmpmem(actual,w*h*components,pixels+wanted_at,w*h*components);
    convolution_assert_args(filter,args); gimp_value_array_unref(args);
    g_assert_cmpuint(gimp_filter_layer_get_run_count(filter),==,1);
    g_object_unref(image);g_free(actual);g_strfreev(v);++compared;
  }
  g_assert_cmpuint(compared,==,12);g_strfreev(rows);g_free(table);g_free(pixels);
}
static void convolution_invalid_arrays_keep_cache (void)
{
  for(guint test=0;test<18;++test) {
    GimpImage *image=image_new(9,8); GimpLayer *source=source_new(image,NULL,9,8);
    GimpFilterLayer *filter=filter_new(image,NULL,9,8);
    GimpValueArray *args=convolution_args(convolution_identity,convolution_all,1,0,0);
    fill(source,21,53,87,255);convolution_define(filter,args);settle(filter);
    guint64 count=gimp_filter_layer_get_run_count(filter);
    if(test<4) {
      const guint index=test<2?4:9, length=test%2?(index==4?201:21):0;
      guint8 bytes[201]={0};
      g_value_take_boxed(gimp_value_array_index(args,index),gimp_array_new(length?bytes:NULL,length,FALSE));
    } else if(test<6) g_value_set_int(gimp_value_array_index(args,test==4?3:8),24);
    else if(test==6) g_value_set_double(gimp_value_array_index(args,6),0);
    else if(test==7) g_value_set_double(gimp_value_array_index(args,6),NAN);
    else if(test==8) g_value_set_double(gimp_value_array_index(args,7),INFINITY);
    else if(test==9) {gdouble matrix[25];memcpy(matrix,convolution_identity,sizeof matrix);matrix[3]=NAN;gimp_value_set_double_array(gimp_value_array_index(args,4),matrix,25);}
    else if(test==10) g_value_set_int(gimp_value_array_index(args,10),3);
    else if(test==11) gimp_value_array_truncate(args,10);
    else if(test==12 || test==13) {
      GValue *v=gimp_value_array_index(args,test==12?4:9);g_value_unset(v);g_value_init(v,G_TYPE_BYTES);
      g_value_take_boxed(v,g_bytes_new_static("missing",7));
    } else if(test==14) g_value_set_double(gimp_value_array_index(args,6),1e-310);
    else if(test==15) g_value_set_double(gimp_value_array_index(args,7),1e100);
    else if(test==16) g_value_set_boxed(gimp_value_array_index(args,4),NULL);
    else g_value_set_boxed(gimp_value_array_index(args,9),NULL);
    convolution_define(filter,args);blinds_wait(filter,GIMP_FILTER_LAYER_FAILED);
    g_assert_cmpuint(gimp_filter_layer_get_run_count(filter),==,count);pixel(GIMP_LAYER(filter),21,53,87,255);
    gimp_value_array_unref(args);g_object_unref(image);
  }
  /* An old FLOATARRAY record with zero stored bytes cannot be reconstructed
   * by guessing identity coefficients. The opaque bytes remain available. */
  GimpImage *image=image_new(9,8);GimpFilterLayer *filter=filter_new(image,NULL,9,8);
  const guint8 omitted[]={0,0,0,7,0,0,0,0};GBytes *raw=g_bytes_new_static(omitted,sizeof omitted);
  g_assert_true(gimp_filter_layer_set_definition(filter,"plug-in-convmatrix",raw,NULL,NULL));
  fill(GIMP_LAYER(filter),11,22,33,255);gimp_filter_layer_mark_as_loaded(filter);gimp_filter_layer_invalidate(filter);
  blinds_wait(filter,GIMP_FILTER_LAYER_FAILED);g_assert_cmpuint(gimp_filter_layer_get_run_count(filter),==,0);
  GBytes *saved=gimp_filter_layer_ref_definition(filter);g_assert_cmpmem(g_bytes_get_data(saved,NULL),g_bytes_get_size(saved),omitted,sizeof omitted);
  pixel(GIMP_LAYER(filter),11,22,33,255);g_bytes_unref(saved);g_bytes_unref(raw);g_object_unref(image);
}
static void convolution_native_precisions (void)
{
  const GimpComponentType types[]={GIMP_COMPONENT_TYPE_U8,GIMP_COMPONENT_TYPE_U16,GIMP_COMPONENT_TYPE_U32,GIMP_COMPONENT_TYPE_HALF,GIMP_COMPONENT_TYPE_FLOAT,GIMP_COMPONENT_TYPE_DOUBLE};
  const GimpTRCType trcs[]={GIMP_TRC_LINEAR,GIMP_TRC_NON_LINEAR,GIMP_TRC_PERCEPTUAL};
  for(guint t=0;t<G_N_ELEMENTS(types);++t)for(guint r=0;r<3;++r)for(guint gray=0;gray<2;++gray) {
    const GimpPrecision precision=gimp_babl_precision(types[t],trcs[r]);
    if(precision==GIMP_PRECISION_U8_NON_LINEAR)continue;
    GimpImage *image=gimp_image_new(gimp,9,8,gray?GIMP_GRAY:GIMP_RGB,precision);
    GimpColorProfile *profile=gray?gimp_color_profile_new_d50_gray_lab_trc():gimp_color_profile_new_rgb_adobe();
    g_assert_true(gimp_image_assign_color_profile(image,profile,NULL,NULL));g_object_unref(profile);
    const Babl *format=gimp_babl_format(gray?GIMP_GRAY:GIMP_RGB,gimp_babl_precision(GIMP_COMPONENT_TYPE_DOUBLE,trcs[r]),TRUE,
      babl_format_get_space(gimp_image_get_layer_format(image,TRUE)));
    GimpLayer *source=gimp_layer_new(image,9,8,gimp_image_get_layer_format(image,TRUE),"native",1,GIMP_LAYER_MODE_NORMAL_LEGACY);
    gimp_image_add_layer(image,source,NULL,0,FALSE);
    gdouble values[9*8*4];const guint channels=gray?2:4;
    for(guint p=0;p<72;++p)for(guint c=0;c<channels;++c)values[p*channels+c]=c==channels-1?1:.25;
    gegl_buffer_set(gimp_drawable_get_buffer(GIMP_DRAWABLE(source)),NULL,0,format,values,GEGL_AUTO_ROWSTRIDE);
    GimpFilterLayer *filter=filter_new(image,NULL,9,8);gdouble matrix[25];memcpy(matrix,convolution_identity,sizeof matrix);
    matrix[12]=.5;GimpValueArray *args=convolution_args(matrix,convolution_all,1,0,0);
    gint32 flags[5]={1,1,1,1,0};gimp_value_set_int32_array(gimp_value_array_index(args,9),flags,5);
    convolution_define(filter,args);settle(filter);
    gdouble actual[4];gegl_buffer_get(gimp_drawable_get_buffer(GIMP_DRAWABLE(filter)),GEGL_RECTANGLE(4,4,1,1),1,format,actual,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
    /* The lower stack owns its profile-aware compositing precision. Check the
     * convolution against the same actual input proxy, then quantize only to
     * the requested native storage, rather than assuming a profile roundtrip
     * leaves the source's literal .25 unchanged. */
    gdouble expected[4]={0};
    GeglNode *below=gegl_node_get_input_proxy(gimp_filter_get_node(GIMP_FILTER(filter)),"input");
    gegl_node_blit(below,1,GEGL_RECTANGLE(4,4,1,1),format,expected,GEGL_AUTO_ROWSTRIDE,GEGL_BLIT_CACHE);
    for(guint c=0;c<channels-1;++c)expected[c]*=.5;
    GeglBuffer *rounded=gegl_buffer_new(GEGL_RECTANGLE(0,0,1,1),gimp_drawable_get_format(GIMP_DRAWABLE(filter)));
    gegl_buffer_set(rounded,NULL,0,format,expected,GEGL_AUTO_ROWSTRIDE);
    gegl_buffer_get(rounded,NULL,1,format,expected,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);g_object_unref(rounded);
    g_test_message("Convolution native component=%u TRC=%u gray=%u",t,r,gray);
    for(guint c=0;c<channels;++c)g_assert_cmpfloat(actual[c],==,expected[c]);
    g_assert_true(babl_format_get_space(gimp_drawable_get_format(GIMP_DRAWABLE(filter)))==babl_format_get_space(format));
    gimp_value_array_unref(args);g_object_unref(image);
  }
}
static void convolution_save_reopen (void)
{
  const guint w=129,h=97;
  for(guint native=0;native<2;++native) {
    GimpImage *image=gimp_image_new(gimp,w,h,GIMP_RGB,native?GIMP_PRECISION_DOUBLE_NON_LINEAR:GIMP_PRECISION_U8_NON_LINEAR);
    const Babl *format=gimp_image_get_layer_format(image,TRUE);const guint bpp=native?32:4;
    GimpLayer *source=gimp_layer_new(image,w,h,format,"Convolution source",1,GIMP_LAYER_MODE_NORMAL_LEGACY);
    gimp_image_add_layer(image,source,NULL,0,FALSE);
    guint8 *samples=g_malloc(w*h*bpp),*actual=g_malloc(w*h*bpp),*reopened=g_malloc(w*h*bpp);
    for(guint p=0;p<w*h;++p) {
      if(native){const gdouble v[]={.25,.5,.75,1};memcpy(samples+p*32,v,32);}
      else{const guint8 v[]={41,83,127,255};memcpy(samples+p*4,v,4);}
    }
    gegl_buffer_set(gimp_drawable_get_buffer(GIMP_DRAWABLE(source)),NULL,0,format,samples,GEGL_AUTO_ROWSTRIDE);
    GimpFilterLayer *filter=filter_new(image,NULL,w,h);gimp_object_set_name(GIMP_OBJECT(filter),"Convolution filter");
    gdouble matrix[25];memcpy(matrix,convolution_identity,sizeof matrix);matrix[12]=native?1+ldexp(1.,-30):2;
    gint32 flags[5]={1,1,1,1,0};GimpValueArray *args=convolution_args(matrix,flags,1,0,0);
    convolution_define(filter,args);settle(filter);convolution_assert_args(filter,args);
    gegl_buffer_get(gimp_drawable_get_buffer(GIMP_DRAWABLE(filter)),NULL,1,format,actual,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
    if(native){gdouble v[4];memcpy(v,actual,32);g_assert_cmpfloat(v[0],==,.25*matrix[12]);g_assert_cmpfloat(v[0],!=,(gdouble)(gfloat)v[0]);}
    GOutputStream *output=g_memory_output_stream_new_resizable();GFile *file=g_file_new_for_path("/tmp/convolution-roundtrip.xcf");GError *error=NULL;
    g_assert_true(xcf_save_stream(gimp,image,output,file,NULL,&error));g_assert_no_error(error);g_assert_true(g_output_stream_close(output,NULL,&error));
    GBytes *bytes=g_memory_output_stream_steal_as_bytes(G_MEMORY_OUTPUT_STREAM(output));
    const gchar *export_dir=g_getenv("GIMP_PAINTER_CONVOLUTION_EXPORT");
    if(export_dir){
      g_assert_cmpint(g_mkdir_with_parents(export_dir,0700),==,0);
      const gchar *id=native?"double":"u8";gchar *name=g_strdup_printf("%s.xcf",id),*path=g_build_filename(export_dir,name,NULL);
      g_assert_true(g_file_set_contents(path,g_bytes_get_data(bytes,NULL),g_bytes_get_size(bytes),NULL));g_free(name);g_free(path);
      for(guint n=0;n<2;++n){name=g_strdup_printf("%s-%s.raw",id,n?"expected":"source");path=g_build_filename(export_dir,name,NULL);
        g_assert_true(g_file_set_contents(path,(gchar *)(n?actual:samples),w*h*bpp,NULL));g_free(name);g_free(path);}
    }
    GInputStream *input=g_memory_input_stream_new_from_bytes(bytes);GimpImage *copy=xcf_load_stream(gimp,input,file,NULL,&error);
    g_assert_no_error(error);g_assert_nonnull(copy);GimpFilterLayer *loaded=GIMP_FILTER_LAYER(gimp_container_get_first_child(gimp_image_get_layers(copy)));
    convolution_assert_args(loaded,args);gegl_buffer_get(gimp_drawable_get_buffer(GIMP_DRAWABLE(loaded)),NULL,1,format,reopened,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
    g_assert_cmpmem(actual,w*h*bpp,reopened,w*h*bpp);
    gimp_filter_layer_invalidate(loaded);settle(loaded);convolution_assert_args(loaded,args);
    gegl_buffer_get(gimp_drawable_get_buffer(GIMP_DRAWABLE(loaded)),NULL,1,format,reopened,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);g_assert_cmpmem(actual,w*h*bpp,reopened,w*h*bpp);
    /* Editing arrays after reopen, then Undo/Redo, keeps independent ownership. */
    matrix[12]=.5;gimp_value_set_double_array(gimp_value_array_index(args,4),matrix,25);
    g_assert_true(gimp_filter_layer_edit_definition(loaded,"plug-in-convmatrix",NULL,args,NULL));settle(loaded);convolution_assert_args(loaded,args);
    g_assert_true(gimp_image_undo(copy));settle(loaded);g_assert_true(gimp_image_redo(copy));settle(loaded);convolution_assert_args(loaded,args);
    gimp_value_array_unref(args);g_object_unref(copy);g_object_unref(input);g_bytes_unref(bytes);g_object_unref(output);g_object_unref(file);g_object_unref(image);
    g_free(samples);g_free(actual);g_free(reopened);
  }
}
static void convolution_ignored_legacy_tail (void)
{
  for(guint gray=0;gray<2;++gray) for(guint string=0;string<2;++string) {
    GimpImage *image=gimp_image_new(gimp,3,3,gray?GIMP_GRAY:GIMP_RGB,GIMP_PRECISION_U8_NON_LINEAR);
    const Babl *format=gimp_image_get_layer_format(image,TRUE);
    GimpLayer *source=gimp_layer_new(image,3,3,format,"source",1,GIMP_LAYER_MODE_NORMAL_LEGACY);
    gimp_image_add_layer(image,source,NULL,0,FALSE);
    guint8 samples[36]={0},actual[36]={0};const guint channels=gray?2:4;
    for(guint p=0;p<9;++p)for(guint c=0;c<channels;++c)samples[p*channels+c]=c==channels-1?255:(p+c)%2;
    gegl_buffer_set(gimp_drawable_get_buffer(GIMP_DRAWABLE(source)),NULL,0,format,samples,GEGL_AUTO_ROWSTRIDE);
    GimpFilterLayer *filter=filter_new(image,NULL,3,3);
    GimpValueArray *args=convolution_args(convolution_identity,convolution_all,1,0,0);
    GValue extra=G_VALUE_INIT;g_value_init(&extra,string?G_TYPE_STRING:G_TYPE_INT);
    if(string)g_value_set_string(&extra,"ignored-old-convolution-sentinel");else g_value_set_int(&extra,4242);
    gimp_value_array_append(args,&extra);g_value_unset(&extra);convolution_define(filter,args);settle(filter);
    convolution_assert_args(filter,args);
    gegl_buffer_get(gimp_drawable_get_buffer(GIMP_DRAWABLE(filter)),NULL,1,format,actual,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
    g_assert_cmpmem(actual,9*channels,samples,9*channels);
    GOutputStream *output=g_memory_output_stream_new_resizable();GFile *file=g_file_new_for_path("/tmp/convolution-tail.xcf");
    g_assert_true(xcf_save_stream(gimp,image,output,file,NULL,NULL));g_assert_true(g_output_stream_close(output,NULL,NULL));
    GBytes *bytes=g_memory_output_stream_steal_as_bytes(G_MEMORY_OUTPUT_STREAM(output));GInputStream *input=g_memory_input_stream_new_from_bytes(bytes);
    GimpImage *copy=xcf_load_stream(gimp,input,file,NULL,NULL);g_assert_nonnull(copy);
    GimpFilterLayer *loaded=GIMP_FILTER_LAYER(gimp_container_get_first_child(gimp_image_get_layers(copy)));
    convolution_assert_args(loaded,args);gimp_filter_layer_invalidate(loaded);settle(loaded);
    gegl_buffer_get(gimp_drawable_get_buffer(GIMP_DRAWABLE(loaded)),NULL,1,format,actual,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
    g_assert_cmpmem(actual,9*channels,samples,9*channels);convolution_assert_args(loaded,args);
    gimp_value_array_unref(args);g_object_unref(copy);g_object_unref(input);g_bytes_unref(bytes);g_object_unref(output);g_object_unref(file);g_object_unref(image);
  }
}
static void convolution_native_storage_overflow (void)
{
  const GimpPrecision precisions[]={GIMP_PRECISION_HALF_NON_LINEAR,GIMP_PRECISION_FLOAT_NON_LINEAR};
  for(guint n=0;n<2;++n) {
    GimpImage *image=gimp_image_new(gimp,9,8,GIMP_RGB,precisions[n]);
    const Babl *format=gimp_image_get_layer_format(image,TRUE),*transfer=babl_format_with_space("R'G'B'A double",babl_format_get_space(format));
    GimpLayer *source=gimp_layer_new(image,9,8,format,"source",1,GIMP_LAYER_MODE_NORMAL_LEGACY);
    gimp_image_add_layer(image,source,NULL,0,FALSE);
    gdouble samples[9*8*4],actual[9*8*4],before[9*8*4];
    for(guint i=0;i<9*8*4;++i)samples[i]=i%4==3?1:.25;
    gegl_buffer_set(gimp_drawable_get_buffer(GIMP_DRAWABLE(source)),NULL,0,transfer,samples,GEGL_AUTO_ROWSTRIDE);
    GimpFilterLayer *filter=filter_new(image,NULL,9,8);
    gdouble matrix[25];memcpy(matrix,convolution_identity,sizeof matrix);matrix[12]=128;
    gint32 flags[5]={1,1,1,1,0};GimpValueArray *args=convolution_args(matrix,flags,1,0,0);
    convolution_define(filter,args);settle(filter);
    gegl_buffer_get(gimp_drawable_get_buffer(GIMP_DRAWABLE(filter)),NULL,1,transfer,before,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
    for(guint i=0;i<9*8*4;++i)g_assert_cmpfloat(before[i],==,i%4==3?1:32); // in-range HDR control
    const guint64 cache=gimp_filter_layer_get_cache_generation(filter);
    matrix[12]=n?1e40:1e6;gimp_value_set_double_array(gimp_value_array_index(args,4),matrix,25);
    convolution_define(filter,args);blinds_wait(filter,GIMP_FILTER_LAYER_FAILED);
    gchar *message=gimp_filter_layer_dup_error(filter);g_assert_nonnull(strstr(message,"finite native storage"));g_free(message);
    g_assert_cmpuint(gimp_filter_layer_get_cache_generation(filter),==,cache);
    gegl_buffer_get(gimp_drawable_get_buffer(GIMP_DRAWABLE(filter)),NULL,1,transfer,actual,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
    g_assert_cmpmem(actual,sizeof actual,before,sizeof before);convolution_assert_args(filter,args);
    gimp_value_set_double_array(gimp_value_array_index(args,4),convolution_identity,25);
    convolution_define(filter,args);settle(filter);
    gegl_buffer_get(gimp_drawable_get_buffer(GIMP_DRAWABLE(filter)),NULL,1,transfer,actual,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
    g_assert_cmpmem(actual,sizeof actual,samples,sizeof samples);
    gimp_value_array_unref(args);g_object_unref(image);
  }
}
static void convolution_native_failure_keeps_cache (void)
{
  GimpImage *image=gimp_image_new(gimp,9,8,GIMP_RGB,GIMP_PRECISION_DOUBLE_LINEAR);
  const Babl *format=gimp_image_get_layer_format(image,TRUE);
  GimpLayer *source=gimp_layer_new(image,9,8,format,"source",1,GIMP_LAYER_MODE_NORMAL);
  gimp_image_add_layer(image,source,NULL,0,FALSE);
  gdouble samples[9*8*4],actual[9*8*4],before[9*8*4];
  for(guint i=0;i<9*8*4;++i)samples[i]=i%4==3?1:.25;
  gegl_buffer_set(gimp_drawable_get_buffer(GIMP_DRAWABLE(source)),NULL,0,format,samples,GEGL_AUTO_ROWSTRIDE);
  GimpFilterLayer *filter=filter_new(image,NULL,9,8);
  GimpValueArray *args=convolution_args(convolution_identity,convolution_all,1,0,0);
  convolution_define(filter,args);settle(filter);
  gegl_buffer_get(gimp_drawable_get_buffer(GIMP_DRAWABLE(filter)),NULL,1,format,before,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
  gdouble matrix[25];memcpy(matrix,convolution_identity,sizeof matrix);matrix[12]=1e308;
  gimp_value_set_double_array(gimp_value_array_index(args,4),matrix,25);
  g_value_set_double(gimp_value_array_index(args,6),1e-308);
  convolution_define(filter,args);blinds_wait(filter,GIMP_FILTER_LAYER_FAILED);
  gegl_buffer_get(gimp_drawable_get_buffer(GIMP_DRAWABLE(filter)),NULL,1,format,actual,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
  g_assert_cmpmem(actual,sizeof actual,before,sizeof before);convolution_assert_args(filter,args);
  gimp_value_set_double_array(gimp_value_array_index(args,4),convolution_identity,25);g_value_set_double(gimp_value_array_index(args,6),1);
  convolution_define(filter,args);settle(filter);gimp_value_array_unref(args);g_object_unref(image);
}
static void convolution_cancel_replace_close (void)
{
  GimpImage *image=image_new(513,257);GimpLayer *source=source_new(image,NULL,513,257);
  GimpFilterLayer *filter=filter_new(image,NULL,513,257);GimpValueArray *args=convolution_args(convolution_identity,convolution_all,1,0,0);
  fill(source,41,83,127,255);convolution_define(filter,args);settle(filter);guint64 runs=gimp_filter_layer_get_run_count(filter);
  gimp_filter_layer_invalidate(filter);blinds_wait(filter,GIMP_FILTER_LAYER_RUNNING);
  gimp_filter_layer_cancel(filter);blinds_wait(filter,GIMP_FILTER_LAYER_FAILED);pixel(GIMP_LAYER(filter),41,83,127,255);
  gdouble matrix[25];memcpy(matrix,convolution_identity,sizeof matrix);matrix[12]=2;gimp_value_set_double_array(gimp_value_array_index(args,4),matrix,25);
  convolution_define(filter,args);blinds_wait(filter,GIMP_FILTER_LAYER_RUNNING);
  matrix[12]=.5;gimp_value_set_double_array(gimp_value_array_index(args,4),matrix,25);convolution_define(filter,args);settle(filter);
  pixel(GIMP_LAYER(filter),21,42,64,128);g_assert_cmpuint(gimp_filter_layer_get_run_count(filter),>=,runs+3);
  gimp_filter_layer_invalidate(filter);blinds_wait(filter,GIMP_FILTER_LAYER_RUNNING);g_object_unref(image);gimp_value_array_unref(args);
  image=image_new(9,8);source=source_new(image,NULL,9,8);filter=filter_new(image,NULL,9,8);fill(source,19,31,43,255);
  args=convolution_args(convolution_identity,convolution_all,1,0,0);convolution_define(filter,args);settle(filter);pixel(GIMP_LAYER(filter),19,31,43,255);
  gimp_value_array_unref(args);g_object_unref(image);
}
