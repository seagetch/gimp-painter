/* SPDX-License-Identifier: GPL-3.0-or-later */
static void
filter_color_exchange_native_basic (void)
{
  GimpImage *image=image_new(16,16);GimpLayer *source=source_new(image,NULL,16,16);
  GimpLayer *layer=gimp_filter_layer_new(image,16,16,"Color Exchange",1,GIMP_LAYER_MODE_NORMAL_LEGACY);
  GimpFilterLayer *filter=GIMP_FILTER_LAYER(layer);
  GimpValueArray *args=gimp_value_array_new_from_types(NULL,
    G_TYPE_INT,0,G_TYPE_INT,123,G_TYPE_INT,456,
    G_TYPE_UINT,200u,G_TYPE_UINT,20u,G_TYPE_UINT,10u,
    G_TYPE_UINT,5u,G_TYPE_UINT,100u,G_TYPE_UINT,250u,
    G_TYPE_UINT,3u,G_TYPE_UINT,4u,G_TYPE_UINT,5u,G_TYPE_NONE);
  fill(source,202,17,15,255);
  g_assert_true(gimp_filter_layer_set_definition(filter,"plug-in-exchange",NULL,args,NULL));gimp_value_array_unref(args);
  g_assert_true(gimp_image_add_layer(image,layer,NULL,0,FALSE));
  settle(filter);g_assert_cmpint(gimp_filter_layer_get_state(filter),==,GIMP_FILTER_LAYER_CLEAN);
  pixel(layer,7,103,255,255);g_assert_cmpuint(gimp_filter_layer_get_run_count(filter),==,1);
  args=gimp_filter_layer_dup_args(filter);
  g_assert_true(G_VALUE_HOLDS_UINT(gimp_value_array_index(args,3)));
  g_assert_cmpuint(g_value_get_uint(gimp_value_array_index(args,3)),==,200);
  gimp_value_array_unref(args);g_object_unref(image);
}

static GimpValueArray *
exchange_args (void)
{
  return gimp_value_array_new_from_types(NULL,
    G_TYPE_INT,0,G_TYPE_INT,-123,G_TYPE_INT,456,
    G_TYPE_UINT,200u,G_TYPE_UINT,20u,G_TYPE_UINT,10u,
    G_TYPE_UINT,5u,G_TYPE_UINT,100u,G_TYPE_UINT,250u,
    G_TYPE_UINT,255u,G_TYPE_UINT,255u,G_TYPE_UINT,255u,G_TYPE_NONE);
}
static void
exchange_define (GimpFilterLayer *filter)
{
  GimpValueArray *args=exchange_args();
  g_assert_true(gimp_filter_layer_set_definition(filter,"plug-in-exchange",NULL,args,NULL));
  gimp_value_array_unref(args);
}
static GimpFilterLayer *
exchange_new (GimpImage *image,GimpLayer *parent,gint width,gint height)
{
  GimpFilterLayer *filter=GIMP_FILTER_LAYER(gimp_filter_layer_new(image,width,height,"Color Exchange",1,GIMP_LAYER_MODE_NORMAL_LEGACY));
  exchange_define(filter);g_assert_true(gimp_image_add_layer(image,GIMP_LAYER(filter),parent,0,FALSE));return filter;
}
static void
exchange_raw_reference (const guchar *input,guchar *shadow,guint width,guint height,
                        guint x1,guint y1,guint x2,guint y2)
{
  const gint from[]={200,20,10},to[]={5,100,250};
  memset(shadow,0,(gsize)width*height*4);
  for(guint y=y1;y<y2;y++)for(guint x=x1;x<x2;x++)
    {gsize p=((gsize)y*width+x)*4;memcpy(shadow+p,input+p,4);
     for(guint c=0;c<3;c++)shadow[p+c]=MIN(255,to[c]+ABS((gint)input[p+c]-from[c]));}
}
static void
exchange_unsupported_formats (void)
{
  const struct {GimpImageBaseType base;GimpPrecision precision;} formats[]={
    {GIMP_GRAY,GIMP_PRECISION_U8_NON_LINEAR},{GIMP_RGB,GIMP_PRECISION_U16_NON_LINEAR},
    {GIMP_RGB,GIMP_PRECISION_FLOAT_LINEAR},{GIMP_RGB,GIMP_PRECISION_U8_LINEAR}};
  for(guint i=0;i<G_N_ELEMENTS(formats);i++)
    {
      GimpImage *image=gimp_image_new(gimp,4,4,formats[i].base,formats[i].precision);
      GimpFilterLayer *filter=exchange_new(image,NULL,4,4);
      GeglBuffer *cache=g_object_ref(gimp_drawable_get_buffer(GIMP_DRAWABLE(filter)));
      gimp_filter_layer_mark_as_loaded(filter);gimp_filter_layer_invalidate(filter);max_rgb_wait(filter,GIMP_FILTER_LAYER_FAILED);
      g_assert_true(cache==gimp_drawable_get_buffer(GIMP_DRAWABLE(filter)));g_assert_cmpuint(gimp_filter_layer_get_run_count(filter),==,0);
      gchar *name=gimp_filter_layer_dup_procedure(filter);g_assert_cmpstr(name,==,"plug-in-exchange");g_free(name);
      g_object_unref(cache);g_object_unref(image);
    }
  g_test_message("Gray,U16 nonlinear,float linear,U8 linear explicitly rejected;no silent U8 conversion");
}

static void
exchange_late_context_routes (void)
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
        source = source_new (image,parent,width,height); filter = exchange_new (image,parent,width,height);
        gimp_item_set_offset (GIMP_ITEM (source),ox,oy); gimp_item_set_offset (GIMP_ITEM (filter),ox,oy);
        fill (source,200,20,10,255);
        gimp_channel_select_rectangle (gimp_image_get_mask (image),ox+x1,oy+y1,x2-x1,y2-y1,
                                       GIMP_CHANNEL_OP_REPLACE,FALSE,0,0,FALSE);
        if (kind == 6) parent->lock_alpha = TRUE; /* Only the target's own lock participates. */
        exchange_define (filter);
        fill (GIMP_LAYER (filter),17,29,53,255); gimp_filter_layer_mark_as_loaded (filter);
        cache = g_object_ref (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)));
        cache_generation = gimp_filter_layer_get_cache_generation (filter);
        g_signal_connect (filter,"filter-state-changed",G_CALLBACK (max_rgb_late_context),&change);
        gimp_filter_layer_invalidate (filter); generation = gimp_filter_layer_get_generation (filter);
        blinds_lifecycle_settle (filter,cache,cache_generation,generation);
        g_assert_true (change.called);
        max_rgb_owned_reference (filter,NULL,input);
        exchange_raw_reference (input,shadow,width,height,x1,y1,x2,y2);
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
            exchange_raw_reference (input,shadow,width,height,0,0,width,height);
            max_rgb_merge_reference (input,shadow,mask,expected,pixels,4,kind == 3 ? 1 : kind == 4 || kind == 5 ? 7 : 15);
          }
        max_rgb_compare (filter,expected,4);
        g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 2);
        g_signal_handlers_disconnect_by_data (filter,&change);
        g_free (input); g_free (shadow); g_free (expected); g_free (mask); g_object_unref (cache); g_object_unref (image);
      }
}


static void
exchange_component_masks (void)
{
  const guint width = 17, height = 13, channels = 4;
  const gsize pixels = width*height, size = pixels*channels;
  GimpImage *image = gimp_image_new (gimp,width,height,GIMP_RGB,GIMP_PRECISION_U8_NON_LINEAR);
  const Babl *format = gimp_image_get_layer_format (image,TRUE);
  GimpLayer *source = gimp_layer_new (image,width,height,format,"Max RGB components",1,GIMP_LAYER_MODE_NORMAL_LEGACY);
  GimpFilterLayer *filter;
  guchar *input = g_malloc (size), *shadow = g_malloc (size), *expected = g_malloc (size), *mask = g_malloc (pixels);
  gimp_image_add_layer (image,source,NULL,0,FALSE); filter = exchange_new (image,NULL,width,height);
  max_rgb_pattern (input,width,height,channels); memset (mask,128,pixels);
  gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (source)),NULL,0,format,input,GEGL_AUTO_ROWSTRIDE);
  blinds_fixture_selection (image,width,height,0,0,mask,FALSE);
  for (guint active = 0; active < (1u << channels); ++active)
    {
      max_rgb_components (image,filter,active,0);
      exchange_define (filter); max_rgb_wait (filter,GIMP_FILTER_LAYER_CLEAN);
      max_rgb_owned_reference (filter,NULL,input);
      exchange_raw_reference (input,shadow,width,height,0,0,width,height);
      max_rgb_merge_reference (input,shadow,mask,expected,pixels,channels,active);
      max_rgb_compare (filter,expected,channels);
      g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, active+1);
    }
  g_free (input); g_free (shadow); g_free (expected); g_free (mask); g_object_unref (image);
}

static void
exchange_cancel_and_resume (void)
{
  for(guint spill=0;spill<2;spill++)
    {
      guint width=spill?1031:257,height=spill?1021:139;
      GimpImage *image=image_new(width,height);GimpLayer *source=source_new(image,NULL,width,height);
      GimpFilterLayer *filter=exchange_new(image,NULL,width,height);
      MaxRGBInterrupt interrupt={spill?GIMP_FILTER_LAYER_IMPORTING:GIMP_FILTER_LAYER_RUNNING,0,source,FALSE};
      fill(source,200,20,10,255);fill(GIMP_LAYER(filter),17,29,53,255);gimp_filter_layer_mark_as_loaded(filter);
      GeglBuffer *cache=g_object_ref(gimp_drawable_get_buffer(GIMP_DRAWABLE(filter)));guint64 generation=gimp_filter_layer_get_cache_generation(filter);
      g_signal_connect(filter,"filter-state-changed",G_CALLBACK(max_rgb_interrupt),&interrupt);
      gimp_filter_layer_invalidate(filter);max_rgb_wait(filter,GIMP_FILTER_LAYER_FAILED);
      g_assert_true(interrupt.called);max_rgb_drain_workers();blinds_lifecycle_cached(filter,cache,generation);
      g_signal_handlers_disconnect_by_data(filter,&interrupt);
      gimp_drawable_update(GIMP_DRAWABLE(source),0,0,width,height);max_rgb_wait(filter,GIMP_FILTER_LAYER_CLEAN);
      blinds_lifecycle_constant(filter,(const guchar[]){5,100,250,255});
      g_object_unref(cache);g_object_unref(image);
    }
  g_test_message("actual vector RUNNING and spill IMPORTING cancel retain prior cache;later source update completes");
}

static void
exchange_invalid_arguments (void)
{
  GimpImage *image=image_new(4,4);GimpFilterLayer *filter=exchange_new(image,NULL,4,4);
  fill(GIMP_LAYER(filter),17,29,53,255);gimp_filter_layer_mark_as_loaded(filter);
  /* Every channel rejects both an out-of-range uint and a signed lookalike. */
  for(guint kind=0;kind<25;kind++)
    {
      GimpValueArray *args=exchange_args();
      if(kind<9)g_value_set_uint(gimp_value_array_index(args,3+kind),256u);
      else if(kind<18){GValue *v=gimp_value_array_index(args,3+kind-9);g_value_unset(v);g_value_init(v,G_TYPE_INT);g_value_set_int(v,20);}
      else if(kind==18)gimp_value_array_truncate(args,11);
      else if(kind==19){GValue v=G_VALUE_INIT;g_value_init(&v,G_TYPE_UINT);gimp_value_array_append(args,&v);g_value_unset(&v);}
      else if(kind==20){GValue *v=gimp_value_array_index(args,3);g_value_unset(v);g_value_init(v,G_TYPE_UCHAR);g_value_set_uchar(v,20);}
      else if(kind==21){GValue *v=gimp_value_array_index(args,3);g_value_unset(v);g_value_init(v,G_TYPE_DOUBLE);g_value_set_double(v,20);}
      else if(kind==22){GValue *v=gimp_value_array_index(args,1);g_value_unset(v);g_value_init(v,G_TYPE_UINT);g_value_set_uint(v,123);}
      else if(kind==23){gimp_value_array_unref(args);args=gimp_value_array_new(0);}
      else {gimp_value_array_unref(args);args=NULL;}
      GeglBuffer *cache=g_object_ref(gimp_drawable_get_buffer(GIMP_DRAWABLE(filter)));
      g_assert_true(gimp_filter_layer_set_definition(filter,"plug-in-exchange",NULL,args,NULL));
      max_rgb_wait(filter,GIMP_FILTER_LAYER_FAILED);g_assert_cmpuint(gimp_filter_layer_get_run_count(filter),==,0);
      g_assert_true(cache==gimp_drawable_get_buffer(GIMP_DRAWABLE(filter)));pixel(GIMP_LAYER(filter),17,29,53,255);
      GimpValueArray *copy=gimp_filter_layer_dup_args(filter);
      if(!args)g_assert_null(copy);else{g_assert_nonnull(copy);g_assert_cmpuint(gimp_value_array_length(copy),==,gimp_value_array_length(args));
       for(guint n=0;n<gimp_value_array_length(args);n++)g_assert_true(G_VALUE_TYPE(gimp_value_array_index(copy,n))==G_VALUE_TYPE(gimp_value_array_index(args,n)));}
      if(args)gimp_value_array_unref(args);if(copy)gimp_value_array_unref(copy);g_object_unref(cache);
    }
  g_object_unref(image);
}
static void
exchange_outside_selection_no_merge (void)
{
  for (guint spill = 0; spill < 2; ++spill)
    {
      const guint width = spill ? 1031 : 63, height = spill ? 1021 : 61;
      GimpImage *image = image_new (width+4,height+4);
      GimpLayer *source = source_new (image,NULL,width,height);
      GimpFilterLayer *filter = exchange_new (image,NULL,width,height);
      MaxRGBContextChange change = {image,width,height,1,0,0,FALSE};
      guchar *input = g_malloc0 ((gsize) width*height*4), *expected = g_malloc ((gsize) width*height*4);
      const gsize center = ((gsize) (height/2)*width+width/2)*4;
      for (gsize i = 0; i < (gsize) width*height; ++i) input[i*4+3] = 100;
      input[center] = 30; input[center+1] = 60; input[center+2] = 100;
      gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (source)),NULL,0,babl_format ("R'G'B'A u8"),input,GEGL_AUTO_ROWSTRIDE);
      gimp_channel_select_rectangle (gimp_image_get_mask (image),width+1,height+1,2,2,
                                     GIMP_CHANNEL_OP_REPLACE,FALSE,0,0,FALSE);
      g_signal_connect (filter,"filter-state-changed",G_CALLBACK (max_rgb_late_context),&change);
      exchange_define (filter); max_rgb_wait (filter,GIMP_FILTER_LAYER_CLEAN);
      g_assert_true (change.called);
      max_rgb_owned_input (filter,babl_format ("R'G'B'A u8"),input);
      max_rgb_compare (filter,input,4);
      g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 1);
      gimp_drawable_update (GIMP_DRAWABLE (source),0,0,width,height); max_rgb_wait (filter,GIMP_FILTER_LAYER_CLEAN);
      exchange_raw_reference (input,expected,width,height,0,0,width,height); max_rgb_compare (filter,expected,4);
      g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 2);
      g_signal_handlers_disconnect_by_data (filter,&change);
      g_free (input); g_free (expected); g_object_unref (image);
    }
}

static void
exchange_rgb_rgba_inputs (void)
{
  for(guint alpha=0;alpha<2;alpha++)
    {
      const guint width=17,height=13,channels=alpha?4:3;
      const Babl *format=babl_format(alpha?"R'G'B'A u8":"R'G'B' u8");
      GimpImage *image=image_new(width,height);
      GimpLayer *source=gimp_layer_new(image,width,height,format,"Exchange input",1,GIMP_LAYER_MODE_NORMAL_LEGACY);
      gimp_image_add_layer(image,source,NULL,0,FALSE);
      GimpFilterLayer *filter=exchange_new(image,NULL,width,height);
      g_assert_true(gimp_drawable_has_alpha(GIMP_DRAWABLE(filter)));
      guchar input[17*13*4],owned[17*13*4],shadow[17*13*4],expected[17*13*4];
      max_rgb_pattern(input,width,height,channels);
      gegl_buffer_set(gimp_drawable_get_buffer(GIMP_DRAWABLE(source)),NULL,0,format,input,GEGL_AUTO_ROWSTRIDE);
      exchange_define(filter);max_rgb_wait(filter,GIMP_FILTER_LAYER_CLEAN);
      max_rgb_owned_reference(filter,NULL,owned);
      exchange_raw_reference(owned,shadow,width,height,0,0,width,height);
      max_rgb_merge_reference(owned,shadow,NULL,expected,width*height,4,15);
      max_rgb_compare(filter,expected,4);g_object_unref(image);
    }
}

static void
exchange_noalpha_owner_rejected (void)
{
  GimpImage *image=image_new(4,4);GimpLayer *source=source_new(image,NULL,4,4);
  GimpFilterLayer *filter=GIMP_FILTER_LAYER(gimp_filter_layer_new(image,4,4,"Unsupported owner",1,GIMP_LAYER_MODE_NORMAL_LEGACY));
  GeglBuffer *buffer=gegl_buffer_new(GEGL_RECTANGLE(0,0,4,4),babl_format("R'G'B' u8"));
  gimp_drawable_set_buffer(GIMP_DRAWABLE(filter),FALSE,NULL,buffer);g_object_unref(buffer);
  g_assert_false(gimp_drawable_has_alpha(GIMP_DRAWABLE(filter)));
  gimp_image_add_layer(image,GIMP_LAYER(filter),NULL,0,FALSE);fill(source,200,20,10,255);
  GeglBuffer *cache=g_object_ref(gimp_drawable_get_buffer(GIMP_DRAWABLE(filter)));
  exchange_define(filter);max_rgb_wait(filter,GIMP_FILTER_LAYER_FAILED);
  g_assert_true(cache==gimp_drawable_get_buffer(GIMP_DRAWABLE(filter)));
  g_assert_cmpuint(gimp_filter_layer_get_run_count(filter),==,0);
  g_object_unref(cache);g_object_unref(image);
}

static void
exchange_projection_extent_cache (void)
{
  GimpImage *image=image_new(3,3);
  GimpLayer *source=source_new(image,NULL,7,5);
  GimpFilterLayer *filter=exchange_new(image,NULL,7,5);
  gimp_item_set_offset(GIMP_ITEM(source),-2,-1);
  gimp_item_set_offset(GIMP_ITEM(filter),-2,-1);
  fill(source,200,20,10,255);fill(GIMP_LAYER(filter),17,29,53,255);
  gimp_filter_layer_mark_as_loaded(filter);
  guchar input[7*5*4],shadow[7*5*4],expected[7*5*4];
  for(guint y=0;y<5;y++)for(guint x=0;x<7;x++)
    {guchar *p=input+(y*7+x)*4;const gboolean inside=x>=2&&x<5&&y>=1&&y<4;
     p[0]=inside?200:17;p[1]=inside?20:29;p[2]=inside?10:53;p[3]=255;}
  exchange_raw_reference(input,shadow,7,5,0,0,7,5);
  max_rgb_merge_reference(input,shadow,NULL,expected,35,4,15);
  exchange_define(filter);max_rgb_wait(filter,GIMP_FILTER_LAYER_CLEAN);
  max_rgb_compare(filter,expected,4);
  g_assert_cmpuint(gimp_filter_layer_get_run_count(filter),==,1);
  g_object_unref(image);
}
