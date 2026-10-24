/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <fenv.h>

/* New explicit immutable Colorify tint representation, not a reconstruction
 * of the old writer's unsupported boxed-color payload. */
static GVariant *
colorify_tint (gdouble r,gdouble g,gdouble b,gdouble a)
{
  return g_variant_ref_sink(g_variant_new("(sudddd)",
    "gimp-painter-colorify-rgba",1u,r,g,b,a));
}
static void
colorify_define (GimpFilterLayer *filter,GVariant *tint)
{
  static const guchar bytes[]={0,0xff,0x80,'C','F',0};
  GBytes *raw=g_bytes_new_static(bytes,sizeof(bytes));
  GimpValueArray *args=gimp_value_array_new_from_types(NULL,
    G_TYPE_INT,0,G_TYPE_INT,-123,G_TYPE_INT,456,G_TYPE_VARIANT,tint,G_TYPE_NONE);
  g_assert_true(gimp_filter_layer_set_definition(filter,"plug-in-colorify",raw,args,NULL));
  g_bytes_unref(raw);gimp_value_array_unref(args);
}
static GimpFilterLayer *
colorify_new (GimpImage *image,GimpLayer *parent,gint width,gint height)
{
  GimpFilterLayer *filter=GIMP_FILTER_LAYER(gimp_filter_layer_new(image,width,height,
    "Colorify",1,GIMP_LAYER_MODE_NORMAL_LEGACY));
  GVariant *tint=colorify_tint(1,1,1,1);colorify_define(filter,tint);g_variant_unref(tint);
  g_assert_true(gimp_image_add_layer(image,GIMP_LAYER(filter),parent,0,FALSE));return filter;
}
static void
filter_colorify_native_basic (void)
{
  GimpImage *image=image_new(16,16);GimpLayer *source=source_new(image,NULL,16,16);
  fill(source,255,255,255,255);
  GimpFilterLayer *filter=colorify_new(image,NULL,16,16);
  settle(filter);g_assert_cmpint(gimp_filter_layer_get_state(filter),==,GIMP_FILTER_LAYER_CLEAN);
  /* Independent integer floor(255*2126/10000)+floor(255*7152/10000)
   * +floor(255*722/10000)=254; combining before truncation gives wrong255. */
  pixel(GIMP_LAYER(filter),254,254,254,255);
  GimpValueArray *args=gimp_filter_layer_dup_args(filter);
  g_assert_cmpuint(gimp_value_array_length(args),==,4);
  g_assert_true(G_VALUE_HOLDS_VARIANT(gimp_value_array_index(args,3)));
  GVariant *want=colorify_tint(1,1,1,1);
  g_assert_true(g_variant_equal(g_value_get_variant(gimp_value_array_index(args,3)),want));
  g_variant_unref(want);gimp_value_array_unref(args);g_object_unref(image);
}

static guint
colorify_scale_reference (guint value,gdouble color)
{
  guint64 bits;memcpy(&bits,&color,sizeof(bits));guint e=(bits>>52)&2047;
  guint64 m=bits&G_GUINT64_CONSTANT(0xfffffffffffff);gint exp;
  if(!value || !bits)return 0;
  if(e){m|=G_GUINT64_CONSTANT(1)<<52;exp=(gint)e-1023-52;}else exp=-1074;
  guint64 product=m*value;if(!product)return 0;
  guint top=63-(guint)__builtin_clzll(product);
  if(top>52){guint shift=top-52;guint64 q=product>>shift,rem=product&((G_GUINT64_CONSTANT(1)<<shift)-1),half=G_GUINT64_CONSTANT(1)<<(shift-1);
    if(rem>half || (rem==half && (q&1)))q++;
    product=q;exp+=(gint)shift;}
  if(exp>=0)return (guint)(product<<exp);
  return -exp>=64?0:(guint)(product>>-exp);
}
static void
colorify_raw_reference (const guchar *input,guchar *shadow,guint width,guint height,
                        guint x1,guint y1,guint x2,guint y2,const gdouble *tint)
{
  guint table[3][256];memset(shadow,0,(gsize)width*height*4);
  for(guint c=0;c<3;c++)for(guint i=0;i<256;i++)table[c][i]=colorify_scale_reference(i,tint[c]);
  for(guint y=y1;y<y2;y++)for(guint x=x1;x<x2;x++){
    gsize p=((gsize)y*width+x)*4;
    guint lum=input[p]*2126/10000+input[p+1]*7152/10000+input[p+2]*722/10000;
    for(guint c=0;c<3;c++)shadow[p+c]=table[c][lum];
    shadow[p+3]=input[p+3];}
}
static void
colorify_define_white (GimpFilterLayer *filter)
{
  GVariant *v=colorify_tint(1,1,1,1);colorify_define(filter,v);g_variant_unref(v);
}
static void
colorify_unsupported_formats (void)
{
  const struct {GimpImageBaseType base;GimpPrecision precision;} formats[]={
    {GIMP_GRAY,GIMP_PRECISION_U8_NON_LINEAR},{GIMP_RGB,GIMP_PRECISION_U16_NON_LINEAR},
    {GIMP_RGB,GIMP_PRECISION_FLOAT_LINEAR},{GIMP_RGB,GIMP_PRECISION_U8_LINEAR}};
  for(guint i=0;i<G_N_ELEMENTS(formats);i++)
    {
      GimpImage *image=gimp_image_new(gimp,4,4,formats[i].base,formats[i].precision);
      GimpFilterLayer *filter=colorify_new(image,NULL,4,4);
      GeglBuffer *cache=g_object_ref(gimp_drawable_get_buffer(GIMP_DRAWABLE(filter)));
      gimp_filter_layer_mark_as_loaded(filter);gimp_filter_layer_invalidate(filter);max_rgb_wait(filter,GIMP_FILTER_LAYER_FAILED);
      g_assert_true(cache==gimp_drawable_get_buffer(GIMP_DRAWABLE(filter)));g_assert_cmpuint(gimp_filter_layer_get_run_count(filter),==,0);
      gchar *name=gimp_filter_layer_dup_procedure(filter);g_assert_cmpstr(name,==,"plug-in-colorify");g_free(name);
      g_object_unref(cache);g_object_unref(image);
    }
  g_test_message("Gray,U16 nonlinear,float linear,U8 linear explicitly rejected;no silent U8 conversion");
}

static void
colorify_late_context_routes (void)
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
        source = source_new (image,parent,width,height); filter = colorify_new (image,parent,width,height);
        gimp_item_set_offset (GIMP_ITEM (source),ox,oy); gimp_item_set_offset (GIMP_ITEM (filter),ox,oy);
        fill (source,200,20,10,255);
        gimp_channel_select_rectangle (gimp_image_get_mask (image),ox+x1,oy+y1,x2-x1,y2-y1,
                                       GIMP_CHANNEL_OP_REPLACE,FALSE,0,0,FALSE);
        if (kind == 6) parent->lock_alpha = TRUE; /* Only the target's own lock participates. */
        colorify_define_white (filter);
        fill (GIMP_LAYER (filter),17,29,53,255); gimp_filter_layer_mark_as_loaded (filter);
        cache = g_object_ref (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)));
        cache_generation = gimp_filter_layer_get_cache_generation (filter);
        g_signal_connect (filter,"filter-state-changed",G_CALLBACK (max_rgb_late_context),&change);
        gimp_filter_layer_invalidate (filter); generation = gimp_filter_layer_get_generation (filter);
        blinds_lifecycle_settle (filter,cache,cache_generation,generation);
        g_assert_true (change.called);
        max_rgb_owned_reference (filter,NULL,input);
        colorify_raw_reference (input,shadow,width,height,x1,y1,x2,y2,(const gdouble[]){1,1,1,1});
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
            colorify_raw_reference (input,shadow,width,height,0,0,width,height,(const gdouble[]){1,1,1,1});
            max_rgb_merge_reference (input,shadow,mask,expected,pixels,4,kind == 3 ? 1 : kind == 4 || kind == 5 ? 7 : 15);
          }
        max_rgb_compare (filter,expected,4);
        g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 2);
        g_signal_handlers_disconnect_by_data (filter,&change);
        g_free (input); g_free (shadow); g_free (expected); g_free (mask); g_object_unref (cache); g_object_unref (image);
      }
}


static void
colorify_component_masks (void)
{
  const guint width = 17, height = 13, channels = 4;
  const gsize pixels = width*height, size = pixels*channels;
  GimpImage *image = gimp_image_new (gimp,width,height,GIMP_RGB,GIMP_PRECISION_U8_NON_LINEAR);
  const Babl *format = gimp_image_get_layer_format (image,TRUE);
  GimpLayer *source = gimp_layer_new (image,width,height,format,"Max RGB components",1,GIMP_LAYER_MODE_NORMAL_LEGACY);
  GimpFilterLayer *filter;
  guchar *input = g_malloc (size), *shadow = g_malloc (size), *expected = g_malloc (size), *mask = g_malloc (pixels);
  gimp_image_add_layer (image,source,NULL,0,FALSE); filter = colorify_new (image,NULL,width,height);
  max_rgb_pattern (input,width,height,channels); memset (mask,128,pixels);
  gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (source)),NULL,0,format,input,GEGL_AUTO_ROWSTRIDE);
  blinds_fixture_selection (image,width,height,0,0,mask,FALSE);
  for (guint active = 0; active < (1u << channels); ++active)
    {
      max_rgb_components (image,filter,active,0);
      colorify_define_white (filter); max_rgb_wait (filter,GIMP_FILTER_LAYER_CLEAN);
      max_rgb_owned_reference (filter,NULL,input);
      colorify_raw_reference (input,shadow,width,height,0,0,width,height,(const gdouble[]){1,1,1,1});
      max_rgb_merge_reference (input,shadow,mask,expected,pixels,channels,active);
      max_rgb_compare (filter,expected,channels);
      g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, active+1);
    }
  g_free (input); g_free (shadow); g_free (expected); g_free (mask); g_object_unref (image);
}

static void
colorify_cancel_and_resume (void)
{
  for(guint spill=0;spill<2;spill++)
    {
      guint width=spill?1031:257,height=spill?1021:139;
      GimpImage *image=image_new(width,height);GimpLayer *source=source_new(image,NULL,width,height);
      GimpFilterLayer *filter=colorify_new(image,NULL,width,height);
      MaxRGBInterrupt interrupt={spill?GIMP_FILTER_LAYER_IMPORTING:GIMP_FILTER_LAYER_RUNNING,0,source,FALSE};
      fill(source,200,20,10,255);fill(GIMP_LAYER(filter),17,29,53,255);gimp_filter_layer_mark_as_loaded(filter);
      GeglBuffer *cache=g_object_ref(gimp_drawable_get_buffer(GIMP_DRAWABLE(filter)));guint64 generation=gimp_filter_layer_get_cache_generation(filter);
      g_signal_connect(filter,"filter-state-changed",G_CALLBACK(max_rgb_interrupt),&interrupt);
      gimp_filter_layer_invalidate(filter);max_rgb_wait(filter,GIMP_FILTER_LAYER_FAILED);
      g_assert_true(interrupt.called);max_rgb_drain_workers();blinds_lifecycle_cached(filter,cache,generation);
      g_signal_handlers_disconnect_by_data(filter,&interrupt);
      gimp_drawable_update(GIMP_DRAWABLE(source),0,0,width,height);max_rgb_wait(filter,GIMP_FILTER_LAYER_CLEAN);
      blinds_lifecycle_constant(filter,(const guchar[]){56,56,56,255});
      g_object_unref(cache);g_object_unref(image);
    }
  g_test_message("actual vector RUNNING and spill IMPORTING cancel retain prior cache;later source update completes");
}
static void
colorify_tints_and_metadata (void)
{
  const gdouble tints[][4]={{1,1,1,1},{0,0,0,0},{.5,.25,.75,-0.0},
    {.1,.2,.3,.7},{1,0,0,1},{0,1,0,.5},{0,0,1,0}};
  GimpImage *image=image_new(16,16);GimpLayer *source=source_new(image,NULL,16,16);
  GimpFilterLayer *filter=colorify_new(image,NULL,16,16);
  guchar input[1024],shadow[1024],expected[1024];max_rgb_pattern(input,16,16,4);
  gegl_buffer_set(gimp_drawable_get_buffer(GIMP_DRAWABLE(source)),NULL,0,
    babl_format("R'G'B'A u8"),input,GEGL_AUTO_ROWSTRIDE);
  for(guint i=0;i<G_N_ELEMENTS(tints);i++){
    const gdouble *c=tints[i];GVariant *v=colorify_tint(c[0],c[1],c[2],c[3]);
    colorify_define(filter,v);max_rgb_wait(filter,GIMP_FILTER_LAYER_CLEAN);
    max_rgb_owned_reference(filter,NULL,input);
    colorify_raw_reference(input,shadow,16,16,0,0,16,16,c);
    max_rgb_merge_reference(input,shadow,NULL,expected,256,4,15);max_rgb_compare(filter,expected,4);
    GimpValueArray *copy=gimp_filter_layer_dup_args(filter);
    GVariant *actual=g_value_get_variant(gimp_value_array_index(copy,3));
    g_assert_cmpuint(g_variant_get_size(actual),==,g_variant_get_size(v));
    g_assert_cmpmem(g_variant_get_data(actual),g_variant_get_size(actual),g_variant_get_data(v),g_variant_get_size(v));
    gimp_value_array_unref(copy);g_variant_unref(v);
  }
  g_object_unref(image);g_test_message("seven native tints retain immutable tuple bytes including unused alpha/negative zero; independent arithmetic+owner merge");
}
static void
colorify_invalid_tuples (void)
{
  GimpImage *image=image_new(4,4);GimpFilterLayer *filter=colorify_new(image,NULL,4,4);
  fill(GIMP_LAYER(filter),17,29,53,255);gimp_filter_layer_mark_as_loaded(filter);
  for(guint kind=0;kind<25;kind++){
    GVariant *v=NULL;
    if(kind==0)v=g_variant_ref_sink(g_variant_new("(sudddd)","other-color",1u,1.,1.,1.,1.));
    else if(kind==1)v=g_variant_ref_sink(g_variant_new("(sudddd)","gimp-painter-colorify-rgba",2u,1.,1.,1.,1.));
    else if(kind==2)v=g_variant_ref_sink(g_variant_new("(dddd)",1.,1.,1.,1.));
    else if(kind==3)v=g_variant_ref_sink(g_variant_new_string("not a tint"));
    else if(kind>=5){gdouble c[]={1,1,1,1};const gdouble bad[]={-0.01,1.01,NAN,INFINITY,-INFINITY};c[(kind-5)/5]=bad[(kind-5)%5];v=colorify_tint(c[0],c[1],c[2],c[3]);}
    GeglBuffer *cache=g_object_ref(gimp_drawable_get_buffer(GIMP_DRAWABLE(filter)));
    colorify_define(filter,v);max_rgb_wait(filter,GIMP_FILTER_LAYER_FAILED);
    g_assert_cmpuint(gimp_filter_layer_get_run_count(filter),==,0);
    g_assert_true(cache==gimp_drawable_get_buffer(GIMP_DRAWABLE(filter)));pixel(GIMP_LAYER(filter),17,29,53,255);
    GimpValueArray *copy=gimp_filter_layer_dup_args(filter);GVariant *actual=g_value_get_variant(gimp_value_array_index(copy,3));
    if(!v)g_assert_null(actual);else{
      g_assert_cmpuint(g_variant_get_size(actual),==,g_variant_get_size(v));
      g_assert_cmpmem(g_variant_get_data(actual),g_variant_get_size(actual),g_variant_get_data(v),g_variant_get_size(v));}
    gimp_value_array_unref(copy);if(v)g_variant_unref(v);g_object_unref(cache);
  }
  g_object_unref(image);g_test_message("unknown identifier/version/shape/null and20 channel-domain failures preserve exact tuple bytes/cache; no coercion");
}
static void
colorify_invalid_arguments (void)
{
  GimpImage *image=image_new(4,4);GimpFilterLayer *filter=colorify_new(image,NULL,4,4);
  fill(GIMP_LAYER(filter),17,29,53,255);gimp_filter_layer_mark_as_loaded(filter);
  for(guint kind=0;kind<6;kind++){
    GVariant *v=colorify_tint(1,1,1,1);
    GimpValueArray *args=gimp_value_array_new_from_types(NULL,G_TYPE_INT,0,G_TYPE_INT,123,
      G_TYPE_INT,456,G_TYPE_VARIANT,v,G_TYPE_NONE);g_variant_unref(v);
    if(kind==0){gimp_value_array_unref(args);args=gimp_value_array_new(0);}
    if(kind==1)gimp_value_array_truncate(args,3);
    if(kind==2){GValue extra=G_VALUE_INIT;g_value_init(&extra,G_TYPE_INT);gimp_value_array_append(args,&extra);g_value_unset(&extra);}
    if(kind==3){GValue *value=gimp_value_array_index(args,3);g_value_unset(value);g_value_init(value,G_TYPE_DOUBLE);g_value_set_double(value,.5);}
    if(kind==4){GValue *value=gimp_value_array_index(args,1);g_value_unset(value);g_value_init(value,G_TYPE_STRING);g_value_set_string(value,"not an ID");}
    if(kind==5){gimp_value_array_unref(args);args=NULL;}
    GeglBuffer *cache=g_object_ref(gimp_drawable_get_buffer(GIMP_DRAWABLE(filter)));
    g_assert_true(gimp_filter_layer_set_definition(filter,"plug-in-colorify",NULL,args,NULL));
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
colorify_non_nearest_rounding (void)
{
  const gint modes[]={FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO};
  const gint previous=fegetround();g_assert_cmpint(previous,==,FE_TONEAREST);
  GimpImage *image=image_new(4,4);GimpFilterLayer *filter=colorify_new(image,NULL,4,4);
  fill(GIMP_LAYER(filter),17,29,53,255);gimp_filter_layer_mark_as_loaded(filter);
  for(guint i=0;i<G_N_ELEMENTS(modes);i++){
    GeglBuffer *cache=g_object_ref(gimp_drawable_get_buffer(GIMP_DRAWABLE(filter)));
    GVariant *v=colorify_tint(.1,.2,.3,.4);g_assert_cmpint(fesetround(modes[i]),==,0);
    colorify_define(filter,v);g_assert_cmpint(fegetround(),==,modes[i]);
    g_assert_cmpint(fesetround(previous),==,0);g_variant_unref(v);
    max_rgb_wait(filter,GIMP_FILTER_LAYER_FAILED);
    g_assert_true(cache==gimp_drawable_get_buffer(GIMP_DRAWABLE(filter)));
    g_assert_cmpuint(gimp_filter_layer_get_run_count(filter),==,0);
    pixel(GIMP_LAYER(filter),17,29,53,255);g_object_unref(cache);
  }
  g_object_unref(image);g_test_message("three unsupported owner rounding modes rejected without changing environment or cache");
}
static void
colorify_rgb_rgba_inputs (void)
{
  for(guint alpha=0;alpha<2;alpha++)
    {
      const guint width=17,height=13,channels=alpha?4:3;
      const Babl *format=babl_format(alpha?"R'G'B'A u8":"R'G'B' u8");
      GimpImage *image=image_new(width,height);
      GimpLayer *source=gimp_layer_new(image,width,height,format,"Colorify input",1,GIMP_LAYER_MODE_NORMAL_LEGACY);
      gimp_image_add_layer(image,source,NULL,0,FALSE);
      GimpFilterLayer *filter=colorify_new(image,NULL,width,height);
      g_assert_true(gimp_drawable_has_alpha(GIMP_DRAWABLE(filter)));
      guchar input[17*13*4],owned[17*13*4],shadow[17*13*4],expected[17*13*4];
      max_rgb_pattern(input,width,height,channels);
      gegl_buffer_set(gimp_drawable_get_buffer(GIMP_DRAWABLE(source)),NULL,0,format,input,GEGL_AUTO_ROWSTRIDE);
      colorify_define_white(filter);max_rgb_wait(filter,GIMP_FILTER_LAYER_CLEAN);
      max_rgb_owned_reference(filter,NULL,owned);
      colorify_raw_reference(owned,shadow,width,height,0,0,width,height,(const gdouble[]){1,1,1,1});
      max_rgb_merge_reference(owned,shadow,NULL,expected,width*height,4,15);
      max_rgb_compare(filter,expected,4);g_object_unref(image);
    }
}

static void
colorify_noalpha_owner_rejected (void)
{
  GimpImage *image=image_new(4,4);GimpLayer *source=source_new(image,NULL,4,4);
  GimpFilterLayer *filter=GIMP_FILTER_LAYER(gimp_filter_layer_new(image,4,4,"Unsupported owner",1,GIMP_LAYER_MODE_NORMAL_LEGACY));
  GeglBuffer *buffer=gegl_buffer_new(GEGL_RECTANGLE(0,0,4,4),babl_format("R'G'B' u8"));
  gimp_drawable_set_buffer(GIMP_DRAWABLE(filter),FALSE,NULL,buffer);g_object_unref(buffer);
  g_assert_false(gimp_drawable_has_alpha(GIMP_DRAWABLE(filter)));
  gimp_image_add_layer(image,GIMP_LAYER(filter),NULL,0,FALSE);fill(source,200,20,10,255);
  GeglBuffer *cache=g_object_ref(gimp_drawable_get_buffer(GIMP_DRAWABLE(filter)));
  colorify_define_white(filter);max_rgb_wait(filter,GIMP_FILTER_LAYER_FAILED);
  g_assert_true(cache==gimp_drawable_get_buffer(GIMP_DRAWABLE(filter)));
  g_assert_cmpuint(gimp_filter_layer_get_run_count(filter),==,0);
  g_object_unref(cache);g_object_unref(image);
}

