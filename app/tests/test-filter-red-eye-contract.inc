/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Literal old Red Eye route, not GEGL red-eye-removal substitution. */
static void
filter_red_eye_native_basic (void)
{
  GimpImage *image=image_new(16,16);GimpLayer *source=source_new(image,NULL,16,16);
  GimpLayer *layer=gimp_filter_layer_new(image,16,16,"red eye",1,GIMP_LAYER_MODE_NORMAL_LEGACY);
  GimpFilterLayer *filter=GIMP_FILTER_LAYER(layer);
  GimpValueArray *args=gimp_value_array_new_from_types(NULL,G_TYPE_INT,0,G_TYPE_INT,123,G_TYPE_INT,456,G_TYPE_INT,50,G_TYPE_NONE);
  fill(source,200,20,10,255);
  g_assert_true(gimp_filter_layer_set_definition(filter,"plug-in-red-eye-removal",NULL,args,NULL));gimp_value_array_unref(args);
  g_assert_true(gimp_image_add_layer(image,layer,NULL,0,FALSE));
  settle(filter);g_assert_cmpint(gimp_filter_layer_get_state(filter),==,GIMP_FILTER_LAYER_CLEAN);
  pixel(layer,20,20,10,255);g_assert_cmpuint(gimp_filter_layer_get_run_count(filter),==,1);
  args=gimp_filter_layer_dup_args(filter);g_assert_cmpint(g_value_get_int(gimp_value_array_index(args,0)),==,0);g_assert_cmpint(g_value_get_int(gimp_value_array_index(args,1)),==,123);g_assert_cmpint(g_value_get_int(gimp_value_array_index(args,2)),==,456);gimp_value_array_unref(args);
  g_object_unref(image);
}

static void
red_eye_define (GimpFilterLayer *filter,gint threshold)
{
  static const guchar bytes[]={0,0xff,0x80,'R','E',0};
  GBytes *raw=g_bytes_new_static(bytes,sizeof(bytes));
  GimpValueArray *args=gimp_value_array_new_from_types(NULL,G_TYPE_INT,0,G_TYPE_INT,-123,G_TYPE_INT,456,G_TYPE_INT,threshold,G_TYPE_NONE);
  g_assert_true(gimp_filter_layer_set_definition(filter,"plug-in-red-eye-removal",raw,args,NULL));
  g_bytes_unref(raw);gimp_value_array_unref(args);
}
static GimpFilterLayer *
red_eye_new (GimpImage *image,GimpLayer *parent,gint width,gint height)
{
  GimpFilterLayer *filter=GIMP_FILTER_LAYER(gimp_filter_layer_new(image,width,height,"Red Eye",1,GIMP_LAYER_MODE_NORMAL_LEGACY));
  red_eye_define(filter,50);
  g_assert_true(gimp_image_add_layer(image,GIMP_LAYER(filter),parent,0,FALSE));return filter;
}
static void
red_eye_raw_reference (const guchar *input,guchar *shadow,guint width,guint height,
                       guint x1,guint y1,guint x2,guint y2,gint threshold)
{
  memset(shadow,0,(gsize)width*height*4);
  for(guint y=y1;y<y2;y++)for(guint x=x1;x<x2;x++)
    {
      gsize p=((gsize)y*width+x)*4;memcpy(shadow+p,input+p,4);
      gint64 red=(guint64)input[p]*5133333/10000000,blue=(guint64)input[p+2]*1933333/10000000;
      gint64 green=input[p+1],adjusted=((gint64)threshold-50)*2;
      if(red>=green-adjusted&&red>=blue-adjusted)
        shadow[p]=MIN(G_GUINT64_CONSTANT(255),(green+blue)*10000000/10266666);
    }
}
static void
red_eye_thresholds_and_rejections (void)
{
  const gint valid[]={-1073741646,-1,0,50,100,200,1073741873};
  const gint invalid[]={G_MININT,-1073741647,1073741874,G_MAXINT};
  GimpImage *image=image_new(16,16);GimpLayer *source=source_new(image,NULL,16,16);
  GimpFilterLayer *filter=red_eye_new(image,NULL,16,16);guchar input[1024],shadow[1024],expected[1024];
  max_rgb_pattern(input,16,16,4);gegl_buffer_set(gimp_drawable_get_buffer(GIMP_DRAWABLE(source)),NULL,0,babl_format("R'G'B'A u8"),input,GEGL_AUTO_ROWSTRIDE);
  for(guint i=0;i<G_N_ELEMENTS(valid);i++)
    {red_eye_define(filter,valid[i]);max_rgb_wait(filter,GIMP_FILTER_LAYER_CLEAN);max_rgb_owned_reference(filter,NULL,input);
     red_eye_raw_reference(input,shadow,16,16,0,0,16,16,valid[i]);max_rgb_merge_reference(input,shadow,NULL,expected,256,4,15);max_rgb_compare(filter,expected,4);}
  for(guint i=0;i<G_N_ELEMENTS(invalid);i++)
    {
      GeglBuffer *cache=g_object_ref(gimp_drawable_get_buffer(GIMP_DRAWABLE(filter)));guint64 runs=gimp_filter_layer_get_run_count(filter);
      red_eye_define(filter,invalid[i]);max_rgb_wait(filter,GIMP_FILTER_LAYER_FAILED);
      g_assert_true(cache==gimp_drawable_get_buffer(GIMP_DRAWABLE(filter)));g_assert_cmpuint(gimp_filter_layer_get_run_count(filter),==,runs);max_rgb_compare(filter,expected,4);
      GimpValueArray *args=gimp_filter_layer_dup_args(filter);g_assert_cmpint(g_value_get_int(gimp_value_array_index(args,3)),==,invalid[i]);gimp_value_array_unref(args);g_object_unref(cache);
    }
  g_object_unref(image);
  g_test_message("7 defined out-of-UI-range/endpoint thresholds;4 unsafe thresholds preserve cache and definition");
}
static void
red_eye_unsupported_formats (void)
{
  const struct {GimpImageBaseType base;GimpPrecision precision;} formats[]={
    {GIMP_GRAY,GIMP_PRECISION_U8_NON_LINEAR},{GIMP_RGB,GIMP_PRECISION_U16_NON_LINEAR},
    {GIMP_RGB,GIMP_PRECISION_FLOAT_LINEAR},{GIMP_RGB,GIMP_PRECISION_U8_LINEAR}};
  for(guint i=0;i<G_N_ELEMENTS(formats);i++)
    {
      GimpImage *image=gimp_image_new(gimp,4,4,formats[i].base,formats[i].precision);
      GimpFilterLayer *filter=red_eye_new(image,NULL,4,4);
      GeglBuffer *cache=g_object_ref(gimp_drawable_get_buffer(GIMP_DRAWABLE(filter)));
      gimp_filter_layer_mark_as_loaded(filter);gimp_filter_layer_invalidate(filter);max_rgb_wait(filter,GIMP_FILTER_LAYER_FAILED);
      g_assert_true(cache==gimp_drawable_get_buffer(GIMP_DRAWABLE(filter)));g_assert_cmpuint(gimp_filter_layer_get_run_count(filter),==,0);
      gchar *name=gimp_filter_layer_dup_procedure(filter);g_assert_cmpstr(name,==,"plug-in-red-eye-removal");g_free(name);
      g_object_unref(cache);g_object_unref(image);
    }
  g_test_message("Gray,U16 nonlinear,float linear,U8 linear explicitly rejected;no silent U8 conversion");
}

static void
red_eye_late_context_routes (void)
{
  for (guint spill = 0; spill < 2; ++spill)
    for (guint kind = 0; kind < 8; ++kind)
      {
        const guint width = spill ? 1031 : 63, height = spill ? 1021 : 61;
        const gint ox = 3, oy = 2;
        const guint x1 = width/3, x2 = 2*width/3, y1 = 3, y2 = height-3;
        const gsize pixels = (gsize) width*height, size = pixels*4;
        GimpImage *image = image_new (width+ox+2,height+oy+2);
        GimpLayer *parent = gimp_group_layer_new (image), *source;
        GimpFilterLayer *filter;
        MaxRGBContextChange change = {image,width,height,kind,ox,oy,FALSE};
        guchar *input = g_malloc (size), *shadow = g_malloc (size), *expected = g_malloc (size), *mask = g_malloc (pixels);
        GeglBuffer *cache;
        guint64 generation, cache_generation;
        gimp_image_add_layer (image,parent,NULL,0,FALSE);
        source = source_new (image,parent,width,height); filter = red_eye_new (image,parent,width,height);
        gimp_item_set_offset (GIMP_ITEM (source),ox,oy); gimp_item_set_offset (GIMP_ITEM (filter),ox,oy);
        fill (source,200,20,10,255);
        gimp_channel_select_rectangle (gimp_image_get_mask (image),ox+x1,oy+y1,x2-x1,y2-y1,
                                       GIMP_CHANNEL_OP_REPLACE,FALSE,0,0,FALSE);
        if (kind == 6) parent->lock_alpha = TRUE; /* Only the target's own lock participates. */
        red_eye_define (filter,50);
        fill (GIMP_LAYER (filter),17,29,53,255); gimp_filter_layer_mark_as_loaded (filter);
        cache = g_object_ref (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)));
        cache_generation = gimp_filter_layer_get_cache_generation (filter);
        g_signal_connect (filter,"filter-state-changed",G_CALLBACK (max_rgb_late_context),&change);
        gimp_filter_layer_invalidate (filter); generation = gimp_filter_layer_get_generation (filter);
        blinds_lifecycle_settle (filter,cache,cache_generation,generation);
        g_assert_true (change.called);
        max_rgb_owned_reference (filter,NULL,input);
        red_eye_raw_reference (input,shadow,width,height,x1,y1,x2,y2,50);
        memset (mask,kind == 2 ? 128 : kind == 7 ? 0 : 255,pixels);
        max_rgb_merge_reference (input,shadow,mask,expected,pixels,4,kind == 3 ? 1 : kind == 4 || kind == 5 ? 7 : 15);
        max_rgb_compare (filter,expected,4);
        spin_ms (15);
        g_assert_cmpuint (gimp_filter_layer_get_generation (filter), ==, generation);
        g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 1);
        /* Idle context changes do not rerun. An unchanged-byte lower update
         * does, and must use the newly captured ROI, mask and components. */
        gimp_drawable_update (GIMP_DRAWABLE (source),0,0,width,height); max_rgb_wait (filter,GIMP_FILTER_LAYER_CLEAN);
        if (kind == 7) memcpy (expected,input,size);
        else
          {
            red_eye_raw_reference (input,shadow,width,height,0,0,width,height,50);
            max_rgb_merge_reference (input,shadow,mask,expected,pixels,4,kind == 3 ? 1 : kind == 4 || kind == 5 ? 7 : 15);
          }
        max_rgb_compare (filter,expected,4);
        g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 2);
        g_signal_handlers_disconnect_by_data (filter,&change);
        g_free (input); g_free (shadow); g_free (expected); g_free (mask); g_object_unref (cache); g_object_unref (image);
      }
}


static void
red_eye_component_masks (void)
{
  const guint width = 17, height = 13, channels = 4;
  const gsize pixels = width*height, size = pixels*channels;
  GimpImage *image = gimp_image_new (gimp,width,height,GIMP_RGB,GIMP_PRECISION_U8_NON_LINEAR);
  const Babl *format = gimp_image_get_layer_format (image,TRUE);
  GimpLayer *source = gimp_layer_new (image,width,height,format,"Max RGB components",1,GIMP_LAYER_MODE_NORMAL_LEGACY);
  GimpFilterLayer *filter;
  guchar *input = g_malloc (size), *shadow = g_malloc (size), *expected = g_malloc (size), *mask = g_malloc (pixels);
  gimp_image_add_layer (image,source,NULL,0,FALSE); filter = red_eye_new (image,NULL,width,height);
  max_rgb_pattern (input,width,height,channels); memset (mask,128,pixels);
  gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (source)),NULL,0,format,input,GEGL_AUTO_ROWSTRIDE);
  blinds_fixture_selection (image,width,height,0,0,mask,FALSE);
  for (guint active = 0; active < (1u << channels); ++active)
    {
      max_rgb_components (image,filter,active,0);
      red_eye_define (filter,50); max_rgb_wait (filter,GIMP_FILTER_LAYER_CLEAN);
      max_rgb_owned_reference (filter,NULL,input);
      red_eye_raw_reference (input,shadow,width,height,0,0,width,height,50);
      max_rgb_merge_reference (input,shadow,mask,expected,pixels,channels,active);
      max_rgb_compare (filter,expected,channels);
      g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, active+1);
    }
  g_free (input); g_free (shadow); g_free (expected); g_free (mask); g_object_unref (image);
}

static void
red_eye_cancel_and_resume (void)
{
  for(guint spill=0;spill<2;spill++)
    {
      guint width=spill?1031:257,height=spill?1021:139;
      GimpImage *image=image_new(width,height);GimpLayer *source=source_new(image,NULL,width,height);
      GimpFilterLayer *filter=red_eye_new(image,NULL,width,height);
      MaxRGBInterrupt interrupt={spill?GIMP_FILTER_LAYER_IMPORTING:GIMP_FILTER_LAYER_RUNNING,0,source,FALSE};
      fill(source,200,20,10,255);fill(GIMP_LAYER(filter),17,29,53,255);gimp_filter_layer_mark_as_loaded(filter);
      GeglBuffer *cache=g_object_ref(gimp_drawable_get_buffer(GIMP_DRAWABLE(filter)));guint64 generation=gimp_filter_layer_get_cache_generation(filter);
      g_signal_connect(filter,"filter-state-changed",G_CALLBACK(max_rgb_interrupt),&interrupt);
      gimp_filter_layer_invalidate(filter);max_rgb_wait(filter,GIMP_FILTER_LAYER_FAILED);
      g_assert_true(interrupt.called);max_rgb_drain_workers();blinds_lifecycle_cached(filter,cache,generation);
      g_signal_handlers_disconnect_by_data(filter,&interrupt);
      gimp_drawable_update(GIMP_DRAWABLE(source),0,0,width,height);max_rgb_wait(filter,GIMP_FILTER_LAYER_CLEAN);
      blinds_lifecycle_constant(filter,(const guchar[]){20,20,10,255});
      g_object_unref(cache);g_object_unref(image);
    }
  g_test_message("actual vector RUNNING and spill IMPORTING cancel retain prior cache;later source update completes");
}
static void
red_eye_invalid_arguments (void)
{
  GimpImage *image=image_new(4,4);GimpFilterLayer *filter=red_eye_new(image,NULL,4,4);
  fill(GIMP_LAYER(filter),17,29,53,255);gimp_filter_layer_mark_as_loaded(filter);
  for(guint kind=0;kind<6;kind++)
    {
      GimpValueArray *args=gimp_value_array_new_from_types(NULL,G_TYPE_INT,1,G_TYPE_INT,123,G_TYPE_INT,456,G_TYPE_INT,50,G_TYPE_NONE);
      if(kind==0){gimp_value_array_unref(args);args=gimp_value_array_new(0);}
      if(kind==1)gimp_value_array_truncate(args,3);
      if(kind==2){GValue v=G_VALUE_INIT;g_value_init(&v,G_TYPE_INT);gimp_value_array_append(args,&v);g_value_unset(&v);}
      if(kind==3){GValue *v=gimp_value_array_index(args,3);g_value_unset(v);g_value_init(v,G_TYPE_DOUBLE);g_value_set_double(v,50);}
      if(kind==4){GValue *v=gimp_value_array_index(args,1);g_value_unset(v);g_value_init(v,G_TYPE_STRING);g_value_set_string(v,"not an ID");}
      if(kind==5){gimp_value_array_unref(args);args=NULL;}
      GeglBuffer *cache=g_object_ref(gimp_drawable_get_buffer(GIMP_DRAWABLE(filter)));
      g_assert_true(gimp_filter_layer_set_definition(filter,"plug-in-red-eye-removal",NULL,args,NULL));
      max_rgb_wait(filter,GIMP_FILTER_LAYER_FAILED);g_assert_cmpuint(gimp_filter_layer_get_run_count(filter),==,0);
      g_assert_true(cache==gimp_drawable_get_buffer(GIMP_DRAWABLE(filter)));pixel(GIMP_LAYER(filter),17,29,53,255);
      GimpValueArray *copy=gimp_filter_layer_dup_args(filter);
      if(!args)g_assert_null(copy);else{g_assert_nonnull(copy);g_assert_cmpuint(gimp_value_array_length(copy),==,gimp_value_array_length(args));
       for(guint n=0;n<gimp_value_array_length(args);n++)g_assert_true(G_VALUE_TYPE(gimp_value_array_index(copy,n))==G_VALUE_TYPE(gimp_value_array_index(args,n)));}
      if(args)gimp_value_array_unref(args);if(copy)gimp_value_array_unref(copy);g_object_unref(cache);
    }
  g_object_unref(image);g_test_message("six malformed/absent definitions retained but never executed;cache unchanged");
}
