/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Independent row-character oracle from the pinned old-C foundation, not the
 * production flattened table/kernel. Native phase is drawable-local. */
static void
video_native_reference (const guchar *input, guchar *shadow,
                        guint width, guint height, guint x1, guint y1,
                        guint x2, guint y2, gint pattern, gint additive, gint rotated)
{
#include "../../migration/tests/video-foundation/reference-patterns.h"
  memset (shadow,0,(gsize)width*height*4);
  for(guint y=y1;y<y2;y++)for(guint x=x1;x<x2;x++)
    {
      guint row=(rotated?x:y)%heights[pattern],column=(rotated?y:x)%widths[pattern];
      guint selected=rows[pattern][row][column]-'0';gsize p=((gsize)y*width+x)*4;
      for(guint c=0;c<3;c++)
        {guint value=(selected==c?input[p+c]:0)+(additive?input[p+c]:0);shadow[p+c]=MIN(value,255);}
      shadow[p+3]=input[p+3];
    }
}
static void
video_native_define (GimpFilterLayer *filter,gint pattern,gint additive,gint rotated)
{
  static const guchar bytes[]={0,0xff,'V','I','D',0};GBytes *raw=g_bytes_new_static(bytes,sizeof(bytes));
  GimpValueArray *args=gimp_value_array_new_from_types(NULL,
    G_TYPE_INT,0,G_TYPE_INT,-123,G_TYPE_INT,456,G_TYPE_INT,pattern,
    G_TYPE_INT,additive,G_TYPE_INT,rotated,G_TYPE_NONE);
  g_assert_true(gimp_filter_layer_set_definition(filter,"plug-in-video",raw,args,NULL));
  g_bytes_unref(raw);gimp_value_array_unref(args);
}
static GimpFilterLayer *
video_native_new (GimpImage *image,GimpLayer *parent,guint width,guint height,
                  gint pattern,gint additive,gint rotated)
{
  GimpFilterLayer *filter=GIMP_FILTER_LAYER(gimp_filter_layer_new(image,width,height,
    "Video",1,GIMP_LAYER_MODE_NORMAL_LEGACY));
  video_native_define(filter,pattern,additive,rotated);
  g_assert_true(gimp_image_add_layer(image,GIMP_LAYER(filter),parent,0,FALSE));return filter;
}
static void
filter_video_native_basic (void)
{
  GimpImage *image=image_new(16,16);GimpLayer *source=source_new(image,NULL,16,16);
  fill(source,71,103,201,255);GimpFilterLayer *filter=video_native_new(image,NULL,16,16,0,0,0);
  settle(filter);g_assert_cmpint(gimp_filter_layer_get_state(filter),==,GIMP_FILTER_LAYER_CLEAN);
  guchar input[16*16*4],expected[16*16*4];
  for(guint p=0;p<16*16;p++){input[p*4]=71;input[p*4+1]=103;input[p*4+2]=201;input[p*4+3]=255;}
  video_native_reference(input,expected,16,16,0,0,16,16,0,0,0);
  max_rgb_compare(filter,expected,4);g_object_unref(image);
}
static void
video_native_patterns_and_flags (void)
{
  const gint flags[]={0,1,-1,G_MININT,G_MAXINT};
  GimpImage *image=image_new(17,19);GimpLayer *source=source_new(image,NULL,17,19);
  guchar input[17*19*4],expected[17*19*4];
  for(guint p=0;p<17*19;p++){input[p*4]=(p*17+71)%256;input[p*4+1]=(p*29+103)%256;input[p*4+2]=(p*43+201)%256;input[p*4+3]=255;}
  gegl_buffer_set(gimp_drawable_get_buffer(GIMP_DRAWABLE(source)),GEGL_RECTANGLE(0,0,17,19),0,
    babl_format("R'G'B'A u8"),input,GEGL_AUTO_ROWSTRIDE);
  GimpFilterLayer *filter=video_native_new(image,NULL,17,19,0,0,0);
  for(gint pattern=0;pattern<9;pattern++)for(guint a=0;a<G_N_ELEMENTS(flags);a++)for(guint r=0;r<G_N_ELEMENTS(flags);r++)
    {
      video_native_define(filter,pattern,flags[a],flags[r]);settle(filter);
      g_assert_cmpint(gimp_filter_layer_get_state(filter),==,GIMP_FILTER_LAYER_CLEAN);
      video_native_reference(input,expected,17,19,0,0,17,19,pattern,flags[a],flags[r]);
      max_rgb_compare(filter,expected,4);
      GimpValueArray *args=gimp_filter_layer_dup_args(filter);
      g_assert_cmpint(g_value_get_int(gimp_value_array_index(args,4)),==,flags[a]);
      g_assert_cmpint(g_value_get_int(gimp_value_array_index(args,5)),==,flags[r]);gimp_value_array_unref(args);
    }
  g_object_unref(image);g_test_message("225 native scenes: nine patterns and25 signed flag pairs, exact integer definitions retained");
}
static void
video_native_roi_phase (void)
{
  for(guint spill=0;spill<2;spill++)
    {
      guint width=spill?1031:37,height=spill?1021:41,x1=5,y1=7,x2=width-3,y2=height-2;
      gint ox=11,oy=13;gsize pixels=(gsize)width*height,size=pixels*4;
      GimpImage *image=image_new(width+ox+2,height+oy+2);GimpLayer *group=gimp_group_layer_new(image);
      gimp_image_add_layer(image,group,NULL,0,FALSE);
      GimpLayer *source=source_new(image,group,width,height);
      gimp_item_set_offset(GIMP_ITEM(source),ox,oy);fill(source,71,103,201,255);
      GimpFilterLayer *filter=video_native_new(image,group,width,height,7,-1,G_MININT);
      gimp_item_set_offset(GIMP_ITEM(filter),ox,oy);
      gimp_channel_select_rectangle(gimp_image_get_mask(image),ox+x1,oy+y1,x2-x1,y2-y1,
        GIMP_CHANNEL_OP_REPLACE,FALSE,0,0,FALSE);
      gimp_filter_layer_invalidate(filter);settle(filter);
      g_assert_cmpint(gimp_filter_layer_get_state(filter),==,GIMP_FILTER_LAYER_CLEAN);
      guchar *input=g_malloc(size),*shadow=g_malloc(size),*expected=g_malloc(size);
      max_rgb_owned_reference(filter,NULL,input);
      video_native_reference(input,shadow,width,height,x1,y1,x2,y2,7,-1,G_MININT);
      /* Final selection is1 inside the captured ROI,0 outside. This oracle
       * rejects image/group-origin phase and per-crop/chunk phase resets. */
      memcpy(expected,input,size);
      for(guint y=y1;y<y2;y++)memcpy(expected+((gsize)y*width+x1)*4,
        shadow+((gsize)y*width+x1)*4,(x2-x1)*4);
      max_rgb_compare(filter,expected,4);
      g_free(expected);g_free(shadow);g_free(input);g_object_unref(image);
    }
  g_test_message("vector/spill non-period-aligned drawable-local ROI inside offset group, independent full pixels");
}
static void
video_native_invalid_and_formats (void)
{
  const struct {GimpImageBaseType base;GimpPrecision precision;} formats[]={
    {GIMP_GRAY,GIMP_PRECISION_U8_NON_LINEAR},{GIMP_RGB,GIMP_PRECISION_U16_NON_LINEAR},
    {GIMP_RGB,GIMP_PRECISION_FLOAT_LINEAR},{GIMP_RGB,GIMP_PRECISION_U8_LINEAR}};
  for(guint i=0;i<G_N_ELEMENTS(formats);i++)
    {
      GimpImage *image=gimp_image_new(gimp,4,4,formats[i].base,formats[i].precision);
      GimpFilterLayer *filter=video_native_new(image,NULL,4,4,0,0,0);
      GeglBuffer *cache=g_object_ref(gimp_drawable_get_buffer(GIMP_DRAWABLE(filter)));
      gimp_filter_layer_mark_as_loaded(filter);gimp_filter_layer_invalidate(filter);
      max_rgb_wait(filter,GIMP_FILTER_LAYER_FAILED);
      g_assert_true(cache==gimp_drawable_get_buffer(GIMP_DRAWABLE(filter)));
      g_assert_cmpuint(gimp_filter_layer_get_run_count(filter),==,0);
      g_object_unref(cache);g_object_unref(image);
    }
  GimpImage *image=image_new(4,4);GimpFilterLayer *filter=video_native_new(image,NULL,4,4,0,0,0);
  fill(GIMP_LAYER(filter),17,29,53,255);gimp_filter_layer_mark_as_loaded(filter);
  const gint patterns[]={-1,9,G_MININT,G_MAXINT};
  for(guint i=0;i<G_N_ELEMENTS(patterns);i++)
    {
      GeglBuffer *cache=g_object_ref(gimp_drawable_get_buffer(GIMP_DRAWABLE(filter)));
      video_native_define(filter,patterns[i],G_MININT,G_MAXINT);max_rgb_wait(filter,GIMP_FILTER_LAYER_FAILED);
      g_assert_true(cache==gimp_drawable_get_buffer(GIMP_DRAWABLE(filter)));pixel(GIMP_LAYER(filter),17,29,53,255);
      GimpValueArray *args=gimp_filter_layer_dup_args(filter);
      g_assert_cmpint(g_value_get_int(gimp_value_array_index(args,3)),==,patterns[i]);
      gimp_value_array_unref(args);g_object_unref(cache);
    }
  g_object_unref(image);
}

static void
video_native_rgb_rgba_inputs (void)
{
  for(guint alpha=0;alpha<2;alpha++)
    {
      const guint width=17,height=13,channels=alpha?4:3;
      const Babl *format=babl_format(alpha?"R'G'B'A u8":"R'G'B' u8");
      GimpImage *image=image_new(width,height);
      GimpLayer *source=gimp_layer_new(image,width,height,format,"Video input",1,GIMP_LAYER_MODE_NORMAL_LEGACY);
      gimp_image_add_layer(image,source,NULL,0,FALSE);
      GimpFilterLayer *filter=video_native_new(image,NULL,width,height,8,-1,1);
      g_assert_true(gimp_drawable_has_alpha(GIMP_DRAWABLE(filter)));
      guchar input[17*13*4],owned[17*13*4],shadow[17*13*4],expected[17*13*4];
      max_rgb_pattern(input,width,height,channels);
      gegl_buffer_set(gimp_drawable_get_buffer(GIMP_DRAWABLE(source)),NULL,0,format,input,GEGL_AUTO_ROWSTRIDE);
      video_native_define(filter,8,-1,1);max_rgb_wait(filter,GIMP_FILTER_LAYER_CLEAN);
      max_rgb_owned_reference(filter,NULL,owned);
      video_native_reference(owned,shadow,width,height,0,0,width,height,8,-1,1);
      max_rgb_merge_reference(owned,shadow,NULL,expected,width*height,4,15);
      max_rgb_compare(filter,expected,4);g_object_unref(image);
    }
}

static void
video_native_noalpha_owner_rejected (void)
{
  GimpImage *image=image_new(4,4);GimpLayer *source=source_new(image,NULL,4,4);
  GimpFilterLayer *filter=GIMP_FILTER_LAYER(gimp_filter_layer_new(image,4,4,"Unsupported owner",1,GIMP_LAYER_MODE_NORMAL_LEGACY));
  GeglBuffer *buffer=gegl_buffer_new(GEGL_RECTANGLE(0,0,4,4),babl_format("R'G'B' u8"));
  gimp_drawable_set_buffer(GIMP_DRAWABLE(filter),FALSE,NULL,buffer);g_object_unref(buffer);
  g_assert_false(gimp_drawable_has_alpha(GIMP_DRAWABLE(filter)));
  gimp_image_add_layer(image,GIMP_LAYER(filter),NULL,0,FALSE);fill(source,200,20,10,255);
  GeglBuffer *cache=g_object_ref(gimp_drawable_get_buffer(GIMP_DRAWABLE(filter)));
  video_native_define(filter,8,-1,1);max_rgb_wait(filter,GIMP_FILTER_LAYER_FAILED);
  g_assert_true(cache==gimp_drawable_get_buffer(GIMP_DRAWABLE(filter)));
  g_assert_cmpuint(gimp_filter_layer_get_run_count(filter),==,0);
  g_object_unref(cache);g_object_unref(image);
}
static void
video_native_cancel_and_resume (void)
{
  for(guint spill=0;spill<2;spill++)
    {
      guint width=spill?1031:257,height=spill?1021:139;gsize size=(gsize)width*height*4;
      GimpImage *image=image_new(width,height);GimpLayer *source=source_new(image,NULL,width,height);
      GimpFilterLayer *filter=video_native_new(image,NULL,width,height,6,1,1);
      MaxRGBInterrupt interrupt={spill?GIMP_FILTER_LAYER_IMPORTING:GIMP_FILTER_LAYER_RUNNING,0,source,FALSE};
      fill(source,71,103,201,255);fill(GIMP_LAYER(filter),17,29,53,255);gimp_filter_layer_mark_as_loaded(filter);
      GeglBuffer *cache=g_object_ref(gimp_drawable_get_buffer(GIMP_DRAWABLE(filter)));guint64 generation=gimp_filter_layer_get_cache_generation(filter);
      g_signal_connect(filter,"filter-state-changed",G_CALLBACK(max_rgb_interrupt),&interrupt);
      gimp_filter_layer_invalidate(filter);max_rgb_wait(filter,GIMP_FILTER_LAYER_FAILED);
      g_assert_true(interrupt.called);max_rgb_drain_workers();blinds_lifecycle_cached(filter,cache,generation);
      g_signal_handlers_disconnect_by_data(filter,&interrupt);
      gimp_drawable_update(GIMP_DRAWABLE(source),0,0,width,height);max_rgb_wait(filter,GIMP_FILTER_LAYER_CLEAN);
      guchar *input=g_malloc(size),*expected=g_malloc(size);max_rgb_owned_reference(filter,NULL,input);
      video_native_reference(input,expected,width,height,0,0,width,height,6,1,1);max_rgb_compare(filter,expected,4);
      g_free(input);g_free(expected);g_object_unref(cache);g_object_unref(image);
    }
}
static void
video_native_invalid_arguments (void)
{
  GimpImage *image=image_new(4,4);GimpFilterLayer *filter=video_native_new(image,NULL,4,4,0,0,0);
  fill(GIMP_LAYER(filter),17,29,53,255);gimp_filter_layer_mark_as_loaded(filter);
  for(guint kind=0;kind<10;kind++)
    {
      GimpValueArray *args=gimp_value_array_new_from_types(NULL,G_TYPE_INT,0,G_TYPE_INT,123,
        G_TYPE_INT,456,G_TYPE_INT,0,G_TYPE_INT,0,G_TYPE_INT,0,G_TYPE_NONE);
      if(kind==0){gimp_value_array_unref(args);args=gimp_value_array_new(0);}
      else if(kind==1)gimp_value_array_truncate(args,5);
      else if(kind==2){GValue extra=G_VALUE_INIT;g_value_init(&extra,G_TYPE_INT);gimp_value_array_append(args,&extra);g_value_unset(&extra);}
      else if(kind==3){gimp_value_array_unref(args);args=NULL;}
      else{GValue *v=gimp_value_array_index(args,kind-4);g_value_unset(v);g_value_init(v,G_TYPE_UINT);g_value_set_uint(v,0);}
      GeglBuffer *cache=g_object_ref(gimp_drawable_get_buffer(GIMP_DRAWABLE(filter)));
      g_assert_true(gimp_filter_layer_set_definition(filter,"plug-in-video",NULL,args,NULL));
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
video_native_late_context_routes (void)
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
        source = source_new (image,parent,width,height); filter = video_native_new (image,parent,width,height,7,-1,1);
        gimp_item_set_offset (GIMP_ITEM (source),ox,oy); gimp_item_set_offset (GIMP_ITEM (filter),ox,oy);
        fill (source,200,20,10,255);
        gimp_channel_select_rectangle (gimp_image_get_mask (image),ox+x1,oy+y1,x2-x1,y2-y1,
                                       GIMP_CHANNEL_OP_REPLACE,FALSE,0,0,FALSE);
        if (kind == 6) parent->lock_alpha = TRUE; /* Only the target's own lock participates. */
        video_native_define (filter,7,-1,1);
        fill (GIMP_LAYER (filter),17,29,53,255); gimp_filter_layer_mark_as_loaded (filter);
        cache = g_object_ref (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)));
        cache_generation = gimp_filter_layer_get_cache_generation (filter);
        g_signal_connect (filter,"filter-state-changed",G_CALLBACK (max_rgb_late_context),&change);
        gimp_filter_layer_invalidate (filter); generation = gimp_filter_layer_get_generation (filter);
        blinds_lifecycle_settle (filter,cache,cache_generation,generation);
        g_assert_true (change.called);
        max_rgb_owned_reference (filter,NULL,input);
        video_native_reference (input,shadow,width,height,x1,y1,x2,y2,7,-1,1);
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
            video_native_reference (input,shadow,width,height,0,0,width,height,7,-1,1);
            max_rgb_merge_reference (input,shadow,mask,expected,pixels,4,kind == 3 ? 1 : kind == 4 || kind == 5 ? 7 : 15);
          }
        max_rgb_compare (filter,expected,4);
        g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 2);
        g_signal_handlers_disconnect_by_data (filter,&change);
        g_free (input); g_free (shadow); g_free (expected); g_free (mask); g_object_unref (cache); g_object_unref (image);
      }
}


static void
video_native_component_masks (void)
{
  const guint width = 17, height = 13, channels = 4;
  const gsize pixels = width*height, size = pixels*channels;
  GimpImage *image = gimp_image_new (gimp,width,height,GIMP_RGB,GIMP_PRECISION_U8_NON_LINEAR);
  const Babl *format = gimp_image_get_layer_format (image,TRUE);
  GimpLayer *source = gimp_layer_new (image,width,height,format,"Max RGB components",1,GIMP_LAYER_MODE_NORMAL_LEGACY);
  GimpFilterLayer *filter;
  guchar *input = g_malloc (size), *shadow = g_malloc (size), *expected = g_malloc (size), *mask = g_malloc (pixels);
  gimp_image_add_layer (image,source,NULL,0,FALSE); filter = video_native_new (image,NULL,width,height,7,-1,1);
  max_rgb_pattern (input,width,height,channels); memset (mask,128,pixels);
  gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (source)),NULL,0,format,input,GEGL_AUTO_ROWSTRIDE);
  blinds_fixture_selection (image,width,height,0,0,mask,FALSE);
  for (guint active = 0; active < (1u << channels); ++active)
    {
      max_rgb_components (image,filter,active,0);
      video_native_define (filter,7,-1,1); max_rgb_wait (filter,GIMP_FILTER_LAYER_CLEAN);
      max_rgb_owned_reference (filter,NULL,input);
      video_native_reference (input,shadow,width,height,0,0,width,height,7,-1,1);
      max_rgb_merge_reference (input,shadow,mask,expected,pixels,channels,active);
      max_rgb_compare (filter,expected,channels);
      g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, active+1);
    }
  g_free (input); g_free (shadow); g_free (expected); g_free (mask); g_object_unref (image);
}

static void
video_native_saved_context_metadata (void)
{
  const gint context[][3]={{0,-123,456},{G_MININT,G_MAXINT,G_MININT},{G_MAXINT,-1,0},{-1,0,G_MAXINT}};
  static const guchar raw_data[]={0,0xff,'V',0,'I','D'};
  GimpImage *image=image_new(17,19);GimpLayer *source=source_new(image,NULL,17,19);
  fill(source,71,103,201,255);GimpFilterLayer *filter=video_native_new(image,NULL,17,19,8,-1,G_MININT);
  guchar input[17*19*4],expected[17*19*4];
  for(guint p=0;p<17*19;p++){input[p*4]=71;input[p*4+1]=103;input[p*4+2]=201;input[p*4+3]=255;}
  video_native_reference(input,expected,17,19,0,0,17,19,8,-1,G_MININT);
  for(guint i=0;i<G_N_ELEMENTS(context);i++)
    {
      GBytes *raw=g_bytes_new_static(raw_data,sizeof(raw_data));
      GimpValueArray *args=gimp_value_array_new_from_types(NULL,G_TYPE_INT,context[i][0],G_TYPE_INT,context[i][1],
        G_TYPE_INT,context[i][2],G_TYPE_INT,8,G_TYPE_INT,-1,G_TYPE_INT,G_MININT,G_TYPE_NONE);
      g_assert_true(gimp_filter_layer_set_definition(filter,"plug-in-video",raw,args,NULL));
      settle(filter);max_rgb_compare(filter,expected,4);
      GimpValueArray *actual=gimp_filter_layer_dup_args(filter);g_assert_cmpuint(gimp_value_array_length(actual),==,6);
      for(guint n=0;n<6;n++)g_assert_cmpint(g_value_get_int(gimp_value_array_index(actual,n)),==,g_value_get_int(gimp_value_array_index(args,n)));
      GBytes *saved=gimp_filter_layer_ref_definition(filter);g_assert_nonnull(saved);g_assert_true(g_bytes_equal(raw,saved));
      g_bytes_unref(saved);g_bytes_unref(raw);gimp_value_array_unref(args);gimp_value_array_unref(actual);
    }
  g_object_unref(image);g_test_message("four signed saved run/image/drawable tuples never used as live IDs; exact six values and raw bytes retained");
}
