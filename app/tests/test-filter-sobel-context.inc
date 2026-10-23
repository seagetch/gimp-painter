/* SPDX-License-Identifier: GPL-3.0-or-later */
/* These source-derived analytic cases exercise the real owner/worker route.
 * They are not original-runtime fixtures. Genuine captured outputs belong to
 * a separately verified corpus and must never be manufactured by this oracle.
 * Common byte comparison, selection and cache helpers are included earlier. */
#include <math.h>
static GimpValueArray *
sobel_args (gint horizontal, gint vertical, gint keep_sign)
{
  return gimp_value_array_new_from_types (NULL,
    G_TYPE_INT,1,G_TYPE_INT,-123,G_TYPE_INT,456,
    G_TYPE_INT,horizontal,G_TYPE_INT,vertical,G_TYPE_INT,keep_sign,G_TYPE_NONE);
}

static void
sobel_set_args (GimpFilterLayer *filter, GimpValueArray *args)
{
  static const guchar saved[] = {0,0xff,2,0x80,0,3,0};
  GBytes *raw = g_bytes_new_static (saved,sizeof saved);
  GError *error = NULL;
  g_assert_true (gimp_filter_layer_set_definition (filter,"plug-in-sobel",raw,args,&error));
  g_assert_no_error (error); g_bytes_unref (raw);
}

static void
sobel_define (GimpFilterLayer *filter, gint horizontal, gint vertical, gint keep_sign)
{
  GimpValueArray *args = sobel_args (horizontal,vertical,keep_sign);
  sobel_set_args (filter,args); gimp_value_array_unref (args);
}

static void
sobel_saved_args (GimpFilterLayer *filter, GimpValueArray *expected)
{
  static const guchar saved[] = {0,0xff,2,0x80,0,3,0};
  GBytes *raw = gimp_filter_layer_ref_definition (filter);
  GimpValueArray *actual = gimp_filter_layer_dup_args (filter);
  gchar *name = gimp_filter_layer_dup_procedure (filter);
  g_assert_cmpstr (name, ==, "plug-in-sobel");
  g_assert_nonnull (raw); g_assert_nonnull (actual);
  g_assert_cmpmem (g_bytes_get_data (raw,NULL),g_bytes_get_size (raw),saved,sizeof saved);
  g_assert_cmpuint (gimp_value_array_length (actual), ==, gimp_value_array_length (expected));
  for (guint i = 0; i < gimp_value_array_length (expected); ++i)
    {
      const GValue *a = gimp_value_array_index (actual,i), *e = gimp_value_array_index (expected,i);
      g_assert_true (G_VALUE_TYPE (a) == G_VALUE_TYPE (e));
      if (G_VALUE_HOLDS_INT (e)) g_assert_cmpint (g_value_get_int (a), ==, g_value_get_int (e));
      else g_assert_cmpfloat (g_value_get_double (a), ==, g_value_get_double (e));
    }
  g_free (name); g_bytes_unref (raw); gimp_value_array_unref (actual);
}

static void
sobel_wait (GimpFilterLayer *filter, GimpFilterLayerState expected)
{
  const gint64 deadline = g_get_monotonic_time () + 90 * G_TIME_SPAN_SECOND;
  while (gimp_filter_layer_get_state (filter) != expected && g_get_monotonic_time () < deadline)
    {
      if (gimp_filter_layer_get_state (filter) == GIMP_FILTER_LAYER_FAILED && expected != GIMP_FILTER_LAYER_FAILED)
        { gchar *error = gimp_filter_layer_dup_error (filter); g_error ("Sobel failed: %s",error); }
      g_main_context_iteration (NULL,FALSE); g_usleep (100);
    }
  g_assert_cmpint (gimp_filter_layer_get_state (filter), ==, expected);
}

static void
sobel_compare (GimpFilterLayer *filter, const guchar *expected, guint channels)
{
  const gsize size = (gsize) gimp_item_get_width (GIMP_ITEM (filter)) *
                            gimp_item_get_height (GIMP_ITEM (filter)) * channels;
  guchar *actual = g_malloc (size);
  gegl_buffer_get (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)),NULL,1,
                   gimp_drawable_get_format (GIMP_DRAWABLE (filter)),
                   actual,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
  g_assert_cmpmem (actual,size,expected,size); g_free (actual);
}

/* Simple scalar expression of the pinned byte arithmetic. This deliberately
 * does not call the production Sobel kernel, its raster wrappers or owner merge.
 * Both inactive flags and negative truthy flags remain ordinary INT32 values. */
static void
sobel_raw_reference (const guchar *input, guchar *shadow,
                     guint width, guint height, guint channels,
                     gint x1, gint y1, gint x2, gint y2,
                     gint horizontal, gint vertical, gint keep_sign)
{
  memset (shadow,0,(gsize) width*height*channels);
  for (gint y = y1; y < y2; ++y)
    for (gint x = x1; x < x2; ++x)
      {
        const gint left = MAX (x1,x-1), right = MIN (x2-1,x+1);
        const gint top = MAX (0,y-1), bottom = MIN ((gint) height-1,y+1);
        const gsize p = ((gsize) y*width+x)*channels;
        gboolean opaque = FALSE;
        for (guint c = 0; c + 1 < channels; ++c)
          {
            const gint tl = input[((gsize) top*width+left)*channels+c];
            const gint tc = input[((gsize) top*width+x)*channels+c];
            const gint tr = input[((gsize) top*width+right)*channels+c];
            const gint ml = input[((gsize) y*width+left)*channels+c];
            const gint mr = input[((gsize) y*width+right)*channels+c];
            const gint bl = input[((gsize) bottom*width+left)*channels+c];
            const gint bc = input[((gsize) bottom*width+x)*channels+c];
            const gint br = input[((gsize) bottom*width+right)*channels+c];
            const gint h = horizontal ? tl+2*tc+tr-bl-2*bc-br : 0;
            const gint v = vertical ? tl+2*ml+bl-tr-2*mr-br : 0;
            const gint value = horizontal && vertical ? (gint) ((gint) (sqrt ((gdouble) h*h+(gdouble) v*v)+.5)/5.66) :
              keep_sign ? 127+(gint) ((h+v)/8.0+.5) : (gint) (ABS (h+v)/4.0+.5);
            shadow[p+c] = value; opaque |= value > 10;
          }
        shadow[p+channels-1] = opaque ? 255 : 0;
      }
}

static gint
sobel_floor_256 (gint value)
{ return value >= 0 ? value/256 : -((-value+255)/256); }

static guint
sobel_divide_nearest (guint numerator, guint denominator)
{ return numerator/denominator + (numerator%denominator > denominator/2); }

static void
sobel_merge_reference (const guchar *input, const guchar *shadow,
                       const guchar *mask, guchar *output,
                       gsize pixels, guint channels, guint active)
{
  for (gsize p = 0; p < pixels; ++p)
    {
      const guint at = channels-1, coverage = mask ? mask[p] : 255;
      const gint product = ((gint) shadow[at]-input[at])*coverage+128;
      const guint alpha = input[at]+sobel_floor_256 (sobel_floor_256 (product)+product);
      const guint ratio = alpha ? sobel_divide_nearest (coverage*shadow[at],alpha) : 0;
      for (guint c = 0; c < at; ++c)
        {
          gint value = input[c];
          if (alpha && (active & (1u << c)))
            value += shadow[c] > input[c] ?
              (gint) sobel_divide_nearest ((shadow[c]-input[c])*ratio,255) :
              -(gint) sobel_divide_nearest ((input[c]-shadow[c])*ratio,255);
          output[c] = value;
        }
      output[at] = active & (1u << at) ? alpha : input[at];
      input += channels; shadow += channels; output += channels;
    }
}

/* Reconstruct project_region's cache-prefill policy independently of the
 * production owner capture. Call after the current graph has settled, using
 * the retained completed cache from immediately before the invocation. */
static void
sobel_owned_reference (GimpFilterLayer *filter, const guchar *previous, guchar *input)
{
  GimpItem *item = GIMP_ITEM (filter), *parent = gimp_item_get_parent (item);
  GimpImage *image = gimp_item_get_image (item);
  const gint width = gimp_item_get_width (item), height = gimp_item_get_height (item);
  const gint ox = gimp_item_get_offset_x (item), oy = gimp_item_get_offset_y (item);
  const gint px = parent ? gimp_item_get_offset_x (parent) : 0;
  const gint py = parent ? gimp_item_get_offset_y (parent) : 0;
  const gint pw = parent ? gimp_item_get_width (parent) : gimp_image_get_width (image);
  const gint ph = parent ? gimp_item_get_height (parent) : gimp_image_get_height (image);
  const guint channels = gimp_drawable_get_base_type (GIMP_DRAWABLE (filter)) == GIMP_GRAY ? 2 : 4;
  const gsize size = (gsize) width*height*channels;
  guchar *lower = g_malloc (size);
  threshold_owned_input (filter,gimp_drawable_get_format (GIMP_DRAWABLE (filter)),lower);
  if (previous) memcpy (input,previous,size); else memset (input,0,size);
  for (gint y = 0; y < height; ++y)
    for (gint x = 0; x < width; ++x)
      if (x+ox >= px && x+ox < px+pw && y+oy >= py && y+oy < py+ph)
        memcpy (input+((gsize) y*width+x)*channels,lower+((gsize) y*width+x)*channels,channels);
  g_free (lower);
}

static void
sobel_pattern (guchar *input, guint width, guint height, guint channels, gboolean opaque)
{
  static const guchar alpha[] = {0,1,63,127,128,254,255};
  for (guint y = 0; y < height; ++y)
    for (guint x = 0; x < width; ++x)
      {
        const gsize p = ((gsize) y*width+x)*channels;
        for (guint c = 0; c + 1 < channels; ++c) input[p+c] = (x*37+y*71+c*61+17)&255;
        input[p+channels-1] = opaque ? 255 : alpha[(x+y)%G_N_ELEMENTS (alpha)];
      }
}

static void
sobel_integer_native_route (void)
{
  const gint flags[][3] = {{0,0,0},{0,0,1},{0,1,0},{0,1,1},{1,0,0},{1,0,1},{1,1,0},{1,1,1},
                          {G_MININT,0,-1},{0,G_MAXINT,G_MININT},{-7,5,-3},{0,0,G_MAXINT}};
  const guint widths[] = {1,1,7,2,17,67}, heights[] = {1,7,1,2,13,66};
  for (guint gray = 0; gray < 2; ++gray)
    for (guint shape = 0; shape < G_N_ELEMENTS (widths); ++shape)
      {
        const guint width = widths[shape], height = heights[shape], channels = gray ? 2 : 4;
        const gsize size = (gsize) width*height*channels;
        GimpImage *image = gimp_image_new (gimp,width,height,gray ? GIMP_GRAY : GIMP_RGB,GIMP_PRECISION_U8_NON_LINEAR);
        const Babl *format = gimp_image_get_layer_format (image,TRUE);
        GimpLayer *source = gimp_layer_new (image,width,height,format,"Sobel source",1,GIMP_LAYER_MODE_NORMAL_LEGACY);
        GimpFilterLayer *filter;
        guchar *input = g_malloc (size), *shadow = g_malloc (size), *expected = g_malloc (size);
        gimp_image_add_layer (image,source,NULL,0,FALSE); filter = filter_new (image,NULL,width,height);
        sobel_pattern (input,width,height,channels,FALSE);
        gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (source)),NULL,0,format,input,GEGL_AUTO_ROWSTRIDE);
        for (guint f = 0; f < G_N_ELEMENTS (flags); ++f)
          {
            GimpValueArray *args = sobel_args (flags[f][0],flags[f][1],flags[f][2]);
            sobel_set_args (filter,args); sobel_wait (filter,GIMP_FILTER_LAYER_CLEAN);
            sobel_owned_reference (filter,NULL,input);
            sobel_raw_reference (input,shadow,width,height,channels,0,0,width,height,flags[f][0],flags[f][1],flags[f][2]);
            sobel_merge_reference (input,shadow,NULL,expected,(gsize) width*height,channels,(1u << channels)-1);
            sobel_compare (filter,expected,channels); sobel_saved_args (filter,args);
            g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, f+1);
            gimp_value_array_unref (args);
          }
        g_free (input); g_free (shadow); g_free (expected); g_object_unref (image);
      }
}

static void
sobel_invalid_definitions_keep_cache (void)
{
  GimpImage *image = image_new (7,5);
  GimpLayer *source = source_new (image,NULL,7,5);
  GimpFilterLayer *filter = filter_new (image,NULL,7,5);
  GeglBuffer *cache;
  guint64 generation;
  fill (source,31,121,217,255); sobel_define (filter,0,0,-1); sobel_wait (filter,GIMP_FILTER_LAYER_CLEAN);
  cache = g_object_ref (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)));
  generation = gimp_filter_layer_get_cache_generation (filter);
  for (guint kind = 0; kind < 8; ++kind)
    {
      GimpValueArray *args = sobel_args (1,0,1);
      if (kind < 6)
        {
          GValue *value = gimp_value_array_index (args,kind);
          g_value_unset (value); g_value_init (value,G_TYPE_DOUBLE); g_value_set_double (value,1);
        }
      else if (kind == 6) gimp_value_array_truncate (args,5);
      else
        {
          GValue value = G_VALUE_INIT;
          g_value_init (&value,G_TYPE_INT); g_value_set_int (&value,5);
          gimp_value_array_append (args,&value); g_value_unset (&value);
        }
      sobel_set_args (filter,args); sobel_wait (filter,GIMP_FILTER_LAYER_FAILED);
      g_assert_true (cache == gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)));
      g_assert_cmpuint (gimp_filter_layer_get_cache_generation (filter), ==, generation);
      g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 1);
      sobel_saved_args (filter,args); blinds_lifecycle_constant (filter,(const guchar[]) {127,127,127,255});
      gimp_value_array_unref (args);
    }
  sobel_define (filter,0,0,0); sobel_wait (filter,GIMP_FILTER_LAYER_CLEAN);
  blinds_lifecycle_constant (filter,(const guchar[]) {31,121,217,0});
  g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 2);
  g_object_unref (cache); g_object_unref (image);
}

static void
sobel_tiny_selection_routes (void)
{
  const guint widths[] = {1,1,7,2}, heights[] = {1,7,1,2};
  for (guint gray = 0; gray < 2; ++gray)
    for (guint crop = 0; crop < G_N_ELEMENTS (widths); ++crop)
      {
        const guint width = 11, height = 11, channels = gray ? 2 : 4;
        const gint ox = crop & 1 ? -2 : 3, oy = crop & 1 ? 3 : -2;
        GimpImage *image = gimp_image_new (gimp,17,17,gray ? GIMP_GRAY : GIMP_RGB,GIMP_PRECISION_U8_NON_LINEAR);
        const Babl *format = gimp_image_get_layer_format (image,TRUE);
        GimpLayer *source = gimp_layer_new (image,width,height,format,"Tiny Sobel crop",1,GIMP_LAYER_MODE_NORMAL_LEGACY);
        GimpFilterLayer *filter;
        guchar input[11*11*4], shadow[11*11*4], expected[11*11*4], mask[11*11];
        gimp_image_add_layer (image,source,NULL,0,FALSE); filter = filter_new (image,NULL,width,height);
        gimp_item_set_offset (GIMP_ITEM (source),ox,oy); gimp_item_set_offset (GIMP_ITEM (filter),ox,oy);
        sobel_pattern (input,width,height,channels,TRUE); memset (mask,0,sizeof mask);
        gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (source)),NULL,0,format,input,GEGL_AUTO_ROWSTRIDE);
        gimp_channel_select_rectangle (gimp_image_get_mask (image),ox+3,oy+3,widths[crop],heights[crop],
                                       GIMP_CHANNEL_OP_REPLACE,FALSE,0,0,FALSE);
        sobel_define (filter,-7,0,-1); sobel_wait (filter,GIMP_FILTER_LAYER_CLEAN);
        sobel_owned_reference (filter,NULL,input);
        for (guint y = 3; y < 3+heights[crop]; ++y)
          for (guint x = 3; x < 3+widths[crop]; ++x) mask[y*width+x] = 255;
        sobel_raw_reference (input,shadow,width,height,channels,3,3,3+widths[crop],3+heights[crop],-7,0,-1);
        sobel_merge_reference (input,shadow,mask,expected,width*height,channels,(1u << channels)-1);
        sobel_compare (filter,expected,channels);
        sharpen_define (filter,10); sobel_wait (filter,GIMP_FILTER_LAYER_FAILED);
        sobel_compare (filter,expected,channels);
        g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 1);
        g_object_unref (image);
      }
}

static void
sobel_unsupported_precision (void)
{
  const GimpComponentType components[] = {GIMP_COMPONENT_TYPE_U8,GIMP_COMPONENT_TYPE_U16,
    GIMP_COMPONENT_TYPE_U32,GIMP_COMPONENT_TYPE_HALF,GIMP_COMPONENT_TYPE_FLOAT,GIMP_COMPONENT_TYPE_DOUBLE};
  const GimpTRCType trcs[] = {GIMP_TRC_LINEAR,GIMP_TRC_NON_LINEAR,GIMP_TRC_PERCEPTUAL};
  for (guint gray = 0; gray < 2; ++gray)
    for (guint c = 0; c < G_N_ELEMENTS (components); ++c)
      for (guint t = 0; t < G_N_ELEMENTS (trcs); ++t)
        {
          const GimpPrecision precision = gimp_babl_precision (components[c],trcs[t]);
          GimpImage *image;
          GimpFilterLayer *filter;
          GimpValueArray *args;
          GeglBuffer *cache;
          guint64 generation;
          guchar actual[4];
          const guchar saved[] = {17,29,53,255};
          if (precision == GIMP_PRECISION_U8_NON_LINEAR) continue;
          image = gimp_image_new (gimp,7,5,gray ? GIMP_GRAY : GIMP_RGB,precision);
          filter = filter_new (image,NULL,7,5); args = sobel_args (-7,3,G_MININT);
          sobel_set_args (filter,args);
          cache = g_object_ref (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)));
          gegl_buffer_set (cache,GEGL_RECTANGLE (0,0,1,1),0,babl_format ("R'G'B'A u8"),saved,GEGL_AUTO_ROWSTRIDE);
          gegl_buffer_get (cache,GEGL_RECTANGLE (0,0,1,1),1,babl_format ("R'G'B'A u8"),actual,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
          gimp_filter_layer_mark_as_loaded (filter); generation = gimp_filter_layer_get_cache_generation (filter);
          gimp_filter_layer_invalidate (filter); sobel_wait (filter,GIMP_FILTER_LAYER_FAILED);
          g_assert_true (cache == gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)));
          g_assert_cmpuint (gimp_filter_layer_get_cache_generation (filter), ==, generation);
          g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 0);
          sobel_saved_args (filter,args);
          {
            guchar after[4];
            gegl_buffer_get (cache,GEGL_RECTANGLE (0,0,1,1),1,babl_format ("R'G'B'A u8"),after,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
            g_assert_cmpmem (after,4,actual,4);
          }
          g_object_unref (cache); gimp_value_array_unref (args); g_object_unref (image);
        }
}

typedef struct
{
  GimpImage *image;
  guint width, height, kind;
  gint ox, oy;
  gboolean called;
} SobelContextChange;

static void
sobel_late_context (GimpFilterLayer *filter, gpointer data)
{
  SobelContextChange *state = data;
  guchar *mask;
  if (state->called || gimp_filter_layer_get_state (filter) != GIMP_FILTER_LAYER_IMPORTING) return;
  state->called = TRUE;
  mask = g_malloc ((gsize) state->width*state->height);
  memset (mask,state->kind == 2 ? 128 : 255,(gsize) state->width*state->height);
  blinds_fixture_selection (state->image,state->width,state->height,state->ox,state->oy,mask,state->kind == 1);
  g_free (mask);
  if (state->kind == 3) threshold_components (state->image,filter,4,1,-1);
  if (state->kind == 4) gimp_image_set_component_active (state->image,GIMP_CHANNEL_ALPHA,FALSE);
  if (state->kind == 5) gimp_layer_set_lock_alpha (GIMP_LAYER (filter),TRUE,FALSE);
  if (state->kind == 7)
    gimp_channel_select_rectangle (gimp_image_get_mask (state->image),0,0,1,1,
                                   GIMP_CHANNEL_OP_REPLACE,FALSE,0,0,FALSE);
}

static void
sobel_late_context_routes (void)
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
        SobelContextChange change = {image,width,height,kind,ox,oy,FALSE};
        guchar *input = g_malloc (size), *shadow = g_malloc (size), *expected = g_malloc (size), *mask = g_malloc (pixels);
        GeglBuffer *cache;
        guint64 generation, cache_generation;
        gimp_image_add_layer (image,parent,NULL,0,FALSE);
        source = source_new (image,parent,width,height); filter = filter_new (image,parent,width,height);
        gimp_item_set_offset (GIMP_ITEM (source),ox,oy); gimp_item_set_offset (GIMP_ITEM (filter),ox,oy);
        fill (source,31,121,217,255);
        gimp_channel_select_rectangle (gimp_image_get_mask (image),ox+x1,oy+y1,x2-x1,y2-y1,
                                       GIMP_CHANNEL_OP_REPLACE,FALSE,0,0,FALSE);
        if (kind == 6) parent->lock_alpha = TRUE; /* Only the target's own lock participates. */
        sobel_define (filter,0,0,-1);
        fill (GIMP_LAYER (filter),17,29,53,255); gimp_filter_layer_mark_as_loaded (filter);
        cache = g_object_ref (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)));
        cache_generation = gimp_filter_layer_get_cache_generation (filter);
        g_signal_connect (filter,"filter-state-changed",G_CALLBACK (sobel_late_context),&change);
        gimp_filter_layer_invalidate (filter); generation = gimp_filter_layer_get_generation (filter);
        blinds_lifecycle_settle (filter,cache,cache_generation,generation);
        g_assert_true (change.called);
        sobel_owned_reference (filter,NULL,input);
        sobel_raw_reference (input,shadow,width,height,4,x1,y1,x2,y2,0,0,-1);
        memset (mask,kind == 2 ? 128 : kind == 7 ? 0 : 255,pixels);
        sobel_merge_reference (input,shadow,mask,expected,pixels,4,kind == 3 ? 1 : kind == 4 || kind == 5 ? 7 : 15);
        sobel_compare (filter,expected,4);
        spin_ms (15);
        g_assert_cmpuint (gimp_filter_layer_get_generation (filter), ==, generation);
        g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 1);
        /* Idle context changes do not rerun. An unchanged-byte lower update
         * does, and must use the newly captured ROI, mask and components. */
        gimp_drawable_update (GIMP_DRAWABLE (source),0,0,width,height); sobel_wait (filter,GIMP_FILTER_LAYER_CLEAN);
        if (kind == 7) memcpy (expected,input,size);
        else
          {
            sobel_raw_reference (input,shadow,width,height,4,0,0,width,height,0,0,-1);
            sobel_merge_reference (input,shadow,mask,expected,pixels,4,kind == 3 ? 1 : kind == 4 || kind == 5 ? 7 : 15);
          }
        sobel_compare (filter,expected,4);
        g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 2);
        g_signal_handlers_disconnect_by_data (filter,&change);
        g_free (input); g_free (shadow); g_free (expected); g_free (mask); g_object_unref (cache); g_object_unref (image);
      }
}

static void
sobel_component_masks (void)
{
  for (guint gray = 0; gray < 2; ++gray)
    {
      const guint width = 17, height = 13, channels = gray ? 2 : 4;
      const gsize pixels = width*height, size = pixels*channels;
      GimpImage *image = gimp_image_new (gimp,width,height,gray ? GIMP_GRAY : GIMP_RGB,GIMP_PRECISION_U8_NON_LINEAR);
      const Babl *format = gimp_image_get_layer_format (image,TRUE);
      GimpLayer *source = gimp_layer_new (image,width,height,format,"Sobel components",1,GIMP_LAYER_MODE_NORMAL_LEGACY);
      GimpFilterLayer *filter;
      guchar *input = g_malloc (size), *shadow = g_malloc (size), *expected = g_malloc (size), *mask = g_malloc (pixels);
      gimp_image_add_layer (image,source,NULL,0,FALSE); filter = filter_new (image,NULL,width,height);
      sobel_pattern (input,width,height,channels,FALSE); memset (mask,128,pixels);
      gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (source)),NULL,0,format,input,GEGL_AUTO_ROWSTRIDE);
      blinds_fixture_selection (image,width,height,0,0,mask,FALSE);
      for (guint active = 0; active < (1u << channels); ++active)
        {
          threshold_components (image,filter,channels,active,0);
          sobel_define (filter,-7,0,-1); sobel_wait (filter,GIMP_FILTER_LAYER_CLEAN);
          sobel_owned_reference (filter,NULL,input);
          sobel_raw_reference (input,shadow,width,height,channels,0,0,width,height,-7,0,-1);
          sobel_merge_reference (input,shadow,mask,expected,pixels,channels,active);
          sobel_compare (filter,expected,channels);
          g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, active+1);
        }
      g_free (input); g_free (shadow); g_free (expected); g_free (mask); g_object_unref (image);
    }
}

static void
sobel_sealed_context_routes (void)
{
  for (guint spill = 0; spill < 2; ++spill)
    {
      const guint width = spill ? 1031 : 513, height = spill ? 1021 : 257;
      const gsize size = (gsize) width*height*4;
      GimpImage *image = image_new (width,height);
      GimpLayer *source = source_new (image,NULL,width,height);
      GimpFilterLayer *filter = filter_new (image,NULL,width,height);
      ThresholdContextChange change = {image,width,height,0,0,0,FALSE};
      guchar *input = g_malloc (size), *shadow = g_malloc (size), *expected = g_malloc (size);
      sobel_pattern (input,width,height,4,FALSE);
      gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (source)),NULL,0,babl_format ("R'G'B'A u8"),input,GEGL_AUTO_ROWSTRIDE);
      g_assert_true (gimp_image_get_mask (image)->bounds_known); g_assert_true (gimp_image_get_mask (image)->empty);
      /* The shared callback changes selection/components/own alpha lock only
       * after the known-empty final context sealed in its transition quantum. */
      g_signal_connect (filter,"filter-state-changed",G_CALLBACK (threshold_freeze_context),&change);
      sobel_define (filter,1,1,0); sobel_wait (filter,GIMP_FILTER_LAYER_CLEAN);
      g_assert_true (change.called); sobel_owned_reference (filter,NULL,input);
      sobel_raw_reference (input,shadow,width,height,4,0,0,width,height,1,1,0);
      sobel_merge_reference (input,shadow,NULL,expected,(gsize) width*height,4,15);
      sobel_compare (filter,expected,4);
      spin_ms (15); g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 1);
      fill (source,79,137,193,255); sobel_wait (filter,GIMP_FILTER_LAYER_CLEAN);
      blinds_lifecycle_constant (filter,(const guchar[]) {79,137,193,255});
      g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 2);
      g_signal_handlers_disconnect_by_data (filter,&change);
      g_free (input); g_free (shadow); g_free (expected); g_object_unref (image);
    }
}

static void
sobel_outside_selection_no_merge (void)
{
  for (guint spill = 0; spill < 2; ++spill)
    {
      const guint width = spill ? 1031 : 63, height = spill ? 1021 : 61;
      const gsize size = (gsize) width*height*4;
      GimpImage *image = image_new (width+4,height+4);
      GimpLayer *source = source_new (image,NULL,width,height);
      GimpFilterLayer *filter = filter_new (image,NULL,width,height);
      SobelContextChange change = {image,width,height,1,0,0,FALSE};
      guchar *input = g_malloc (size), *shadow = g_malloc (size), *expected = g_malloc (size);
      sobel_pattern (input,width,height,4,FALSE);
      gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (source)),NULL,0,babl_format ("R'G'B'A u8"),input,GEGL_AUTO_ROWSTRIDE);
      gimp_channel_select_rectangle (gimp_image_get_mask (image),width+1,height+1,2,2,
                                     GIMP_CHANNEL_OP_REPLACE,FALSE,0,0,FALSE);
      g_signal_connect (filter,"filter-state-changed",G_CALLBACK (sobel_late_context),&change);
      sobel_define (filter,1,1,1); sobel_wait (filter,GIMP_FILTER_LAYER_CLEAN);
      g_assert_true (change.called); sobel_owned_reference (filter,NULL,input); sobel_compare (filter,input,4);
      g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 1);
      gimp_drawable_update (GIMP_DRAWABLE (source),0,0,width,height); sobel_wait (filter,GIMP_FILTER_LAYER_CLEAN);
      sobel_raw_reference (input,shadow,width,height,4,0,0,width,height,1,1,1);
      sobel_merge_reference (input,shadow,NULL,expected,(gsize) width*height,4,15); sobel_compare (filter,expected,4);
      g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 2);
      g_signal_handlers_disconnect_by_data (filter,&change);
      g_free (input); g_free (shadow); g_free (expected); g_object_unref (image);
    }
}

typedef struct
{
  GimpFilterLayerState phase;
  guint action;
  GimpLayer *source;
  GeglBuffer *cache;
  guint64 cache_generation;
  gboolean called;
} SobelInterrupt;

static void
sobel_interrupt (GimpFilterLayer *filter, gpointer data)
{
  SobelInterrupt *state = data;
  if (state->called || gimp_filter_layer_get_state (filter) != state->phase) return;
  state->called = TRUE;
  blinds_lifecycle_cached (filter,state->cache,state->cache_generation);
  /* Check every completed pixel while the next generation is still private. */
  threshold_old_cache_bytes (state->cache,gimp_item_get_width (GIMP_ITEM (filter)),gimp_item_get_height (GIMP_ITEM (filter)));
  if (state->action == 0) gimp_filter_layer_cancel (filter);
  else if (state->action == 1) sobel_define (filter,0,0,-1);
  else if (state->action == 2) fill (state->source,79,137,193,255);
  else g_assert_true (gimp_painter_binding_close (G_OBJECT (filter),NULL));
}

static void
sobel_cancel_replace_lower_close (void)
{
  const GimpFilterLayerState phases[] = {GIMP_FILTER_LAYER_WAITING,GIMP_FILTER_LAYER_PREPARING,
                                        GIMP_FILTER_LAYER_RUNNING,GIMP_FILTER_LAYER_IMPORTING};
  for (guint spill = 0; spill < 2; ++spill)
    for (guint phase = 0; phase < G_N_ELEMENTS (phases); ++phase)
      for (guint action = 0; action < 4; ++action)
        {
          const guint width = spill ? 1031 : 257, height = spill ? 1021 : 139;
          GimpImage *image;
          GimpLayer *source;
          GimpFilterLayer *filter;
          SobelInterrupt interrupt = {phases[phase],action,NULL,NULL,0,FALSE};
          gint64 deadline;
          /* A spool can cross RUNNING inside one owner quantum; only the
           * vector launch guarantees an observable RUNNING transition. */
          if (spill && phases[phase] == GIMP_FILTER_LAYER_RUNNING) continue;
          image = image_new (width,height); source = source_new (image,NULL,width,height);
          filter = filter_new (image,NULL,width,height); interrupt.source = source;
          fill (source,31,121,217,255); sobel_define (filter,0,0,0);
          fill (GIMP_LAYER (filter),17,29,53,255); gimp_filter_layer_mark_as_loaded (filter);
          interrupt.cache = g_object_ref (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)));
          interrupt.cache_generation = gimp_filter_layer_get_cache_generation (filter);
          g_signal_connect (filter,"filter-state-changed",G_CALLBACK (sobel_interrupt),&interrupt);
          gimp_filter_layer_invalidate (filter);
          deadline = g_get_monotonic_time () + 90 * G_TIME_SPAN_SECOND;
          while (!interrupt.called && g_get_monotonic_time () < deadline)
            {
              blinds_lifecycle_cached (filter,interrupt.cache,interrupt.cache_generation);
              g_main_context_iteration (NULL,FALSE); g_usleep (100);
            }
          g_assert_true (interrupt.called);
          if (action == 1 || action == 2)
            {
              const guint64 generation = gimp_filter_layer_get_generation (filter);
              blinds_lifecycle_settle (filter,interrupt.cache,interrupt.cache_generation,generation);
              blinds_lifecycle_constant (filter,action == 1 ? (const guchar[]) {127,127,127,255} :
                                                               (const guchar[]) {79,137,193,0});
              g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==,
                phases[phase] == GIMP_FILTER_LAYER_RUNNING || phases[phase] == GIMP_FILTER_LAYER_IMPORTING ? 2 : 1);
            }
          else
            {
              sobel_wait (filter,action == 0 ? GIMP_FILTER_LAYER_FAILED : GIMP_FILTER_LAYER_CLOSED);
              threshold_drain_workers (); blinds_lifecycle_cached (filter,interrupt.cache,interrupt.cache_generation);
              if (action == 0)
                {
                  GimpValueArray *args = sobel_args (0,0,0);
                  sobel_saved_args (filter,args); gimp_value_array_unref (args);
                  fill (source,79,137,193,255); sobel_wait (filter,GIMP_FILTER_LAYER_CLEAN);
                  blinds_lifecycle_constant (filter,(const guchar[]) {79,137,193,0});
                }
            }
          threshold_old_cache_bytes (interrupt.cache,width,height);
          g_signal_handlers_disconnect_by_data (filter,&interrupt);
          g_object_unref (interrupt.cache); g_object_unref (image);
        }
}

static void
sobel_partial_import_interrupt (GimpFilterLayer *filter, gpointer data)
{
  SobelInterrupt *state = data;
  GimpFilterLayerProgress progress;
  if (state->called || gimp_filter_layer_get_state (filter) != GIMP_FILTER_LAYER_IMPORTING ||
      !gimp_filter_layer_get_progress (filter,&progress) || progress.value <= 0.85 || progress.value >= 1.0) return;
  /* Progress is used only with the public IMPORTING phase, whose positive
   * import fraction proves an actual staged chunk. Old Sobel progress 1.0
   * (absolute row / ROI height) is never a worker-completion predicate. */
  g_test_message ("Sobel interrupts private staged import: progress=%.9f action=%u",progress.value,state->action);
  sobel_interrupt (filter,data);
}

static void
sobel_partial_import_lifecycle (void)
{
  for (guint spill = 0; spill < 2; ++spill)
    for (guint action = 0; action < 4; ++action)
      {
        const guint width = spill ? 1031 : 1024, height = spill ? 2049 : 1024;
        GimpImage *image = image_new (width,height);
        GimpLayer *source = source_new (image,NULL,width,height);
        GimpFilterLayer *filter = filter_new (image,NULL,width,height);
        SobelInterrupt interrupt = {GIMP_FILTER_LAYER_IMPORTING,action,source,NULL,0,FALSE};
        guchar *input = g_malloc ((gsize) width*height*4);
        gint64 deadline;
        sobel_pattern (input,width,height,4,TRUE);
        gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (source)),NULL,0,babl_format ("R'G'B'A u8"),input,GEGL_AUTO_ROWSTRIDE);
        gimp_channel_select_rectangle (gimp_image_get_mask (image),0,0,width,height,
                                       GIMP_CHANNEL_OP_REPLACE,FALSE,0,0,FALSE);
        sobel_define (filter,0,0,0);
        fill (GIMP_LAYER (filter),17,29,53,255); gimp_filter_layer_mark_as_loaded (filter);
        interrupt.cache = g_object_ref (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)));
        interrupt.cache_generation = gimp_filter_layer_get_cache_generation (filter);
        g_signal_connect (filter,"filter-progress-changed",G_CALLBACK (sobel_partial_import_interrupt),&interrupt);
        gimp_filter_layer_invalidate (filter);
        deadline = g_get_monotonic_time () + 90 * G_TIME_SPAN_SECOND;
        while (!interrupt.called && g_get_monotonic_time () < deadline)
          {
            blinds_lifecycle_cached (filter,interrupt.cache,interrupt.cache_generation);
            g_assert_cmpint (gimp_filter_layer_get_state (filter), !=, GIMP_FILTER_LAYER_CLEAN);
            g_main_context_iteration (NULL,FALSE); g_usleep (100);
          }
        g_assert_true (interrupt.called);
        if (action == 1 || action == 2)
          {
            const guint64 generation = gimp_filter_layer_get_generation (filter);
            blinds_lifecycle_settle (filter,interrupt.cache,interrupt.cache_generation,generation);
            blinds_lifecycle_constant (filter,action == 1 ? (const guchar[]) {127,127,127,255} :
                                                             (const guchar[]) {79,137,193,0});
            g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 2);
          }
        else
          {
            g_assert_cmpint (gimp_filter_layer_get_state (filter), ==,
                             action == 0 ? GIMP_FILTER_LAYER_FAILED : GIMP_FILTER_LAYER_CLOSED);
            threshold_drain_workers (); blinds_lifecycle_cached (filter,interrupt.cache,interrupt.cache_generation);
          }
        threshold_old_cache_bytes (interrupt.cache,width,height);
        g_signal_handlers_disconnect_by_data (filter,&interrupt);
        g_free (input); g_object_unref (interrupt.cache); g_object_unref (image);
      }
}

static void
sobel_native_profile_bytes (void)
{
  for (guint gray = 0; gray < 2; ++gray)
    for (guint spill = 0; spill < 2; ++spill)
      {
        const guint width = spill ? 1031 : 31, height = spill ? 1021 : 17, channels = gray ? 2 : 4;
        const gsize pixels = (gsize) width*height, size = pixels*channels;
        GimpImage *image = gimp_image_new (gimp,width,height,gray ? GIMP_GRAY : GIMP_RGB,GIMP_PRECISION_U8_NON_LINEAR);
        GimpColorProfile *profile = gray ? gimp_color_profile_new_d50_gray_lab_trc () : gimp_color_profile_new_rgb_adobe ();
        GimpLayer *source;
        GimpFilterLayer *filter;
        const Babl *native;
        GError *error = NULL;
        guchar *input = g_malloc (size), *shadow = g_malloc (size), *expected = g_malloc (size);
        g_assert_true (gimp_image_set_color_profile (image,profile,&error));
        g_assert_no_error (error); g_object_unref (profile);
        native = gimp_image_get_layer_format (image,TRUE);
        source = gimp_layer_new (image,width,height,native,"Encoded Sobel source",1,GIMP_LAYER_MODE_NORMAL_LEGACY);
        gimp_image_add_layer (image,source,NULL,0,FALSE); filter = filter_new (image,NULL,width,height);
        sobel_pattern (input,width,height,channels,FALSE);
        gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (source)),NULL,0,native,input,GEGL_AUTO_ROWSTRIDE);
        sobel_define (filter,0,-7,-1); sobel_wait (filter,GIMP_FILTER_LAYER_CLEAN);
        sobel_owned_reference (filter,NULL,input);
        sobel_raw_reference (input,shadow,width,height,channels,0,0,width,height,0,-7,-1);
        sobel_merge_reference (input,shadow,NULL,expected,pixels,channels,(1u << channels)-1);
        sobel_compare (filter,expected,channels);
        g_assert_true (babl_format_get_space (gimp_drawable_get_format (GIMP_DRAWABLE (filter))) == babl_format_get_space (native));
        g_assert_cmpint (gimp_drawable_get_precision (GIMP_DRAWABLE (filter)), ==, GIMP_PRECISION_U8_NON_LINEAR);
        g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 1);
        g_test_message ("Sobel native encoded bytes: %s %s",gray ? "Gray Lab TRC" : "Adobe RGB",spill ? "spill" : "vector");
        g_free (input); g_free (shadow); g_free (expected); g_object_unref (image);
      }
}

static void
sobel_admission_preserves_cache (void)
{
  guint64 original_spill;
  g_object_get (gimp->config,"painter-filter-spill-size",&original_spill,NULL);
  for (guint gray = 0; gray < 2; ++gray)
    {
      const guint side = gray ? 15000 : 11000, channels = gray ? 2 : 4;
      GimpImage *image = gimp_image_new (gimp,side,side,gray ? GIMP_GRAY : GIMP_RGB,GIMP_PRECISION_U8_NON_LINEAR);
      GimpFilterLayer *filter = filter_new (image,NULL,side,side);
      GimpValueArray *args = sobel_args (1,-7,0);
      const Babl *native = gimp_image_get_layer_format (image,TRUE);
      const guchar saved[] = {17,29,53,123};
      guchar actual[4];
      GeglBuffer *cache;
      guint64 generation;
      gchar *error;
      sobel_set_args (filter,args);
      cache = g_object_ref (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)));
      /* Sparse sentinel only. Admission must reject the request before reading
       * or allocating an image-sized owner snapshot or starting a worker. */
      gegl_buffer_set (cache,GEGL_RECTANGLE (0,0,1,1),0,native,saved,GEGL_AUTO_ROWSTRIDE);
      gimp_filter_layer_mark_as_loaded (filter); generation = gimp_filter_layer_get_cache_generation (filter);
      gimp_filter_layer_invalidate (filter); sobel_wait (filter,GIMP_FILTER_LAYER_FAILED);
      error = gimp_filter_layer_dup_error (filter);
      g_assert_nonnull (strstr (error,"admission memory limit")); g_free (error);
      g_assert_true (cache == gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)));
      g_assert_cmpuint (gimp_filter_layer_get_cache_generation (filter), ==, generation);
      g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 0);
      gegl_buffer_get (cache,GEGL_RECTANGLE (0,0,1,1),1,native,actual,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
      g_assert_cmpmem (actual,channels,saved,channels); sobel_saved_args (filter,args);
      g_object_unref (cache); gimp_value_array_unref (args); g_object_unref (image);
    }
  for (guint gray = 0; gray < 2; ++gray)
    {
      const guint width = 1031, height = 1021, channels = gray ? 2 : 4;
      const guint64 reservation = (guint64) width*height*(gray ? 13 : 17);
      GimpImage *image = gimp_image_new (gimp,width,height,gray ? GIMP_GRAY : GIMP_RGB,GIMP_PRECISION_U8_NON_LINEAR);
      const Babl *native = gimp_image_get_layer_format (image,TRUE);
      GimpLayer *source = gimp_layer_new (image,width,height,native,"Admission source",1,GIMP_LAYER_MODE_NORMAL_LEGACY);
      GimpFilterLayer *filter;
      GimpValueArray *args = sobel_args (0,0,-1);
      GeglBuffer *cache;
      guint64 generation;
      gchar *error;
      guchar *input = g_malloc ((gsize) width*height*channels);
      for (gsize i = 0; i < (gsize) width*height; ++i)
        {
          for (guint c = 0; c + 1 < channels; ++c) input[i*channels+c] = 31+c*61;
          input[i*channels+channels-1] = 255;
        }
      gimp_image_add_layer (image,source,NULL,0,FALSE); filter = filter_new (image,NULL,width,height);
      gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (source)),NULL,0,native,input,GEGL_AUTO_ROWSTRIDE);
      sobel_set_args (filter,args);
      gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)),NULL,0,native,input,GEGL_AUTO_ROWSTRIDE);
      gimp_filter_layer_mark_as_loaded (filter);
      cache = g_object_ref (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)));
      generation = gimp_filter_layer_get_cache_generation (filter);
      g_object_set (gimp->config,"painter-filter-spill-size",reservation-1,NULL);
      gimp_filter_layer_invalidate (filter); sobel_wait (filter,GIMP_FILTER_LAYER_FAILED);
      error = gimp_filter_layer_dup_error (filter); g_assert_nonnull (strstr (error,"spill")); g_free (error);
      g_assert_true (cache == gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)));
      g_assert_cmpuint (gimp_filter_layer_get_cache_generation (filter), ==, generation);
      g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 0); sobel_saved_args (filter,args);
      sobel_compare (filter,input,channels);
      g_object_set (gimp->config,"painter-filter-spill-size",reservation,NULL);
      sobel_wait (filter,GIMP_FILTER_LAYER_CLEAN);
      for (gsize i = 0; i < (gsize) width*height; ++i)
        for (guint c = 0; c + 1 < channels; ++c) input[i*channels+c] = 127;
      sobel_compare (filter,input,channels);
      g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 1);
      g_test_message ("Sobel logical spill admission: channels=%u rejected=%" G_GUINT64_FORMAT " accepted=%" G_GUINT64_FORMAT
                      " bytes; not an RSS or GEGL swap measurement",channels,reservation-1,reservation);
      g_object_set (gimp->config,"painter-filter-spill-size",original_spill,NULL);
      g_free (input); g_object_unref (cache); gimp_value_array_unref (args); g_object_unref (image);
    }
}

static void
sobel_projection_input_history (void)
{
  const guint width = 17, height = 13, image_width = 13, image_height = 11;
  const gint flags[][3] = {{0,0,-1},{1,0,-7},{1,1,0}};
  guint halo_witnesses = 0, group_off_canvas = 0;
  for (guint gray = 0; gray < 2; ++gray)
    for (guint grouped = 0; grouped < 2; ++grouped)
      for (guint positive = 0; positive < 2; ++positive)
        for (guint seeded = 0; seeded < 2; ++seeded)
          {
            const guint channels = gray ? 2 : 4;
            const gsize pixels = width*height, size = pixels*channels;
            const gint ox = positive ? 3 : -3, oy = positive ? 2 : -2;
            GimpImage *image = gimp_image_new (gimp,image_width,image_height,gray ? GIMP_GRAY : GIMP_RGB,GIMP_PRECISION_U8_NON_LINEAR);
            const Babl *format = gimp_image_get_layer_format (image,TRUE);
            GimpLayer *parent = grouped ? gimp_group_layer_new (image) : NULL;
            GimpLayer *source = gimp_layer_new (image,width,height,format,"Sobel history source",1,GIMP_LAYER_MODE_NORMAL_LEGACY);
            GimpFilterLayer *filter;
            guchar input[17*13*4], previous[17*13*4], shadow[17*13*4], expected[17*13*4];
            guchar cleared[17*13*4], wrong_shadow[17*13*4], wrong[17*13*4];
            if (parent) gimp_image_add_layer (image,parent,NULL,0,FALSE);
            gimp_image_add_layer (image,source,parent,0,FALSE); filter = filter_new (image,parent,width,height);
            gimp_item_set_offset (GIMP_ITEM (source),ox,oy); gimp_item_set_offset (GIMP_ITEM (filter),ox,oy);
            sobel_pattern (input,width,height,channels,TRUE);
            gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (source)),NULL,0,format,input,GEGL_AUTO_ROWSTRIDE);
            memset (previous,0,size);
            if (seeded)
              {
                sobel_pattern (previous,width,height,channels,FALSE);
                gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)),NULL,0,format,previous,GEGL_AUTO_ROWSTRIDE);
              }
            for (guint run = 0; run < G_N_ELEMENTS (flags); ++run)
              {
                if (run == 2) gimp_item_translate (GIMP_ITEM (filter),-2*ox,-2*oy,FALSE);
                sobel_define (filter,flags[run][0],flags[run][1],flags[run][2]);
                if (!run && seeded)
                  {
                    gimp_filter_layer_mark_as_loaded (filter); spin_ms (15);
                    sobel_compare (filter,previous,channels);
                    g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 0);
                    gimp_filter_layer_invalidate (filter);
                  }
                sobel_wait (filter,GIMP_FILTER_LAYER_CLEAN);
                sobel_owned_reference (filter,previous,input);
                sobel_raw_reference (input,shadow,width,height,channels,0,0,width,height,flags[run][0],flags[run][1],flags[run][2]);
                sobel_merge_reference (input,shadow,NULL,expected,pixels,channels,(1u << channels)-1);
                sobel_compare (filter,expected,channels);
                /* A crop-only or zero-prefilled input loses the retained
                 * vertical halo. Count an actual analytic pixel witness. */
                sobel_owned_reference (filter,NULL,cleared);
                sobel_raw_reference (cleared,wrong_shadow,width,height,channels,0,0,width,height,flags[run][0],flags[run][1],flags[run][2]);
                sobel_merge_reference (cleared,wrong_shadow,NULL,wrong,pixels,channels,(1u << channels)-1);
                if (!grouped && run && memcmp (wrong,expected,size)) ++halo_witnesses;
                if (grouped)
                  for (guint y = 0; y < height; ++y)
                    for (guint x = 0; x < width; ++x)
                      {
                        const gint gx = x+gimp_item_get_offset_x (GIMP_ITEM (filter));
                        const gint gy = y+gimp_item_get_offset_y (GIMP_ITEM (filter));
                        if ((gx < 0 || gy < 0 || gx >= (gint) image_width || gy >= (gint) image_height) &&
                            input[((gsize) y*width+x)*channels+channels-1]) ++group_off_canvas;
                      }
                memcpy (previous,expected,size);
                g_assert_cmpint (gimp_item_get_offset_x (GIMP_ITEM (source)), ==, ox);
                g_assert_cmpint (gimp_item_get_offset_y (GIMP_ITEM (source)), ==, oy);
                g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, run+1);
              }
            g_object_unref (image);
          }
  g_assert_cmpuint (halo_witnesses, >, 0); g_assert_cmpuint (group_off_canvas, >, 0);
  g_test_message ("Sobel analytic history: 16 scenes / 48 runs, cache-sensitive witnesses=%u group off-canvas samples=%u",
                  halo_witnesses,group_off_canvas);
}

static void
sobel_projection_extreme_offsets (void)
{
  const gint offsets[][2] = {{G_MAXINT-2,0},{0,G_MAXINT-2},
                             {G_MAXINT-2,G_MAXINT-2},{G_MININT+2,G_MININT+2}};
  /* Width exceeds the initial read budget, so both row and later-column
   * chunks cross the signed-coordinate boundary in the positive cases. */
  const guint width = 2049, height = 65;
  for (guint gray = 0; gray < 2; ++gray)
    for (guint operation = 0; operation < 2; ++operation)
      for (guint offset = 0; offset < G_N_ELEMENTS (offsets); ++offset)
        {
          const guint channels = gray ? 2 : 4;
          const gsize pixels = (gsize) width*height, size = pixels*channels;
          GimpImage *image = gimp_image_new (gimp,17,13,gray ? GIMP_GRAY : GIMP_RGB,GIMP_PRECISION_U8_NON_LINEAR);
          const Babl *format = gimp_image_get_layer_format (image,TRUE);
          GimpFilterLayer *filter = filter_new (image,NULL,width,height);
          guchar *previous = g_malloc (size), *shadow = g_malloc (size), *expected = g_malloc (size);
          if (operation) threshold_define (filter,127); else sobel_define (filter,1,0,-7);
          sobel_pattern (previous,width,height,channels,FALSE);
          gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)),NULL,0,format,previous,GEGL_AUTO_ROWSTRIDE);
          gimp_filter_layer_mark_as_loaded (filter);
          /* Move after configuring, not only at definition admission: offset
           * invalidation reuses that request and must still be checked. */
          gimp_item_set_offset (GIMP_ITEM (filter),offsets[offset][0],offsets[offset][1]);
          gimp_filter_layer_invalidate (filter); sobel_wait (filter,GIMP_FILTER_LAYER_CLEAN);
          if (operation) threshold_reference (previous,expected,width,height,channels,127);
          else
            {
              sobel_raw_reference (previous,shadow,width,height,channels,0,0,width,height,1,0,-7);
              sobel_merge_reference (previous,shadow,NULL,expected,pixels,channels,(1u << channels)-1);
            }
          sobel_compare (filter,expected,channels);
          g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 1);
          /* A globally nonempty selection remains wholly outside. no_merge
           * preserves the now-completed cache, even at extreme coordinates. */
          gimp_channel_select_rectangle (gimp_image_get_mask (image),0,0,1,1,
                                         GIMP_CHANNEL_OP_REPLACE,FALSE,0,0,FALSE);
          gimp_filter_layer_invalidate (filter); sobel_wait (filter,GIMP_FILTER_LAYER_CLEAN);
          sobel_compare (filter,expected,channels);
          g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 2);
          g_free (previous); g_free (shadow); g_free (expected); g_object_unref (image);
        }
  /* Other routes have no projection clip: reject the unrepresentable native
   * request before blitting and retain every completed cache pixel. */
  {
    GimpImage *image = image_new (17,13);
    GimpFilterLayer *filter = filter_new (image,NULL,width,height);
    GeglBuffer *cache;
    guint64 generation;
    gchar *error;
    fill (GIMP_LAYER (filter),17,29,53,255); gimp_filter_layer_mark_as_loaded (filter);
    cache = g_object_ref (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)));
    generation = gimp_filter_layer_get_cache_generation (filter);
    gimp_item_set_offset (GIMP_ITEM (filter),G_MAXINT-2,G_MAXINT-2);
    gimp_filter_layer_invalidate (filter); sobel_wait (filter,GIMP_FILTER_LAYER_FAILED);
    error = gimp_filter_layer_dup_error (filter);
    g_assert_nonnull (strstr (error,"native coordinate range")); g_free (error);
    blinds_lifecycle_cached (filter,cache,generation); threshold_old_cache_bytes (cache,width,height);
    g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 0);
    g_object_unref (cache); g_object_unref (image);
  }
}

/* Actual old-runtime bytes are read only from the provenance-checked corpus.
 * The capture verifier establishes complete membership and event ordering;
 * this consumer never derives expected outputs from the modern scalar helper. */
static void
sobel_actual_old_live (void)
{
  const gchar *directory = g_getenv ("GIMP_PAINTER_SOBEL_FIXTURES");
  gchar *path, *text = NULL, **lines;
  guint scenes = 0, comparisons = 0, layouts = 0, reruns = 0, phases = 0;
  g_assert_nonnull (directory);
  path = g_build_filename (directory,"captures.tsv",NULL);
  g_assert_true (g_file_get_contents (path,&text,NULL,NULL));
  lines = g_strsplit (text,"\n",-1);
  for (guint row = 0; lines[row]; ++row)
    {
      gchar **cols;
      guint width,height,image_width,image_height,source_channels,channels,passes,rerun,phase;
      gint ox,oy,flags[3];
      GimpImage *image;
      GimpLayer *source;
      GimpFilterLayer *filter;
      const Babl *source_format;
      ThresholdOldContext change;
      guchar *bytes;
      guint64 generation;
      if (!*lines[row]) continue;
      cols = g_strsplit (lines[row],"\t",-1);
      g_assert_cmpuint (g_strv_length (cols), ==, 33);
      if (!atoi (cols[1])) { g_strfreev (cols); continue; }
      source_channels = atoi (cols[2]); channels = atoi (cols[3]);
      g_assert_cmpuint (source_channels, >=, 1); g_assert_cmpuint (source_channels, <=, 4);
      layouts |= 1u << (source_channels-1);
      width = atoi (cols[4]); height = atoi (cols[5]); image_width = atoi (cols[6]); image_height = atoi (cols[7]);
      ox = atoi (cols[8]); oy = atoi (cols[9]);
      for (guint i = 0; i < 3; ++i) flags[i] = atoi (cols[10+i]);
      rerun = atoi (cols[19]); phase = atoi (cols[26]); passes = rerun ? 2 : 1;
      g_assert_cmpuint (rerun, <=, 5); g_assert_cmpuint (phase, <=, 3);
      reruns |= 1u << rerun; phases |= 1u << phase;
      g_assert_true (channels == 2 || channels == 4);
      g_assert_cmpint (atoi (cols[21]), ==, passes); g_assert_cmpint (atoi (cols[22]), ==, passes);
      g_assert_cmpint (atoi (cols[24]), ==, -1); /* Async PDB status is not observable. */
      source_format = babl_format (source_channels == 1 ? "Y' u8" : source_channels == 2 ? "Y'A u8" :
                                  source_channels == 3 ? "R'G'B' u8" : "R'G'B'A u8");
      image = gimp_image_new (gimp,image_width,image_height,channels == 2 ? GIMP_GRAY : GIMP_RGB,GIMP_PRECISION_U8_NON_LINEAR);
      source = gimp_layer_new (image,width,height,source_format,"Old Sobel source",1,GIMP_LAYER_MODE_NORMAL_LEGACY);
      gimp_image_add_layer (image,source,NULL,0,FALSE); filter = filter_new (image,NULL,width,height);
      gimp_item_set_offset (GIMP_ITEM (source),ox,oy); gimp_item_set_offset (GIMP_ITEM (filter),ox,oy);
      bytes = threshold_fixture (directory,cols[0],"source",(gsize) width*height*source_channels);
      gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (source)),NULL,0,source_format,bytes,GEGL_AUTO_ROWSTRIDE);
      g_free (bytes);
      bytes = threshold_fixture (directory,cols[0],"start-mask",(gsize) image_width*image_height);
      blinds_fixture_selection (image,image_width,image_height,0,0,bytes,atoi (cols[13]) == 0); g_free (bytes);
      threshold_components (image,filter,channels,atoi (cols[15]),atoi (cols[17]));
      change = (ThresholdOldContext) {image,directory,cols[0],image_width,image_height,
                                     atoi (cols[14]),atoi (cols[16]),atoi (cols[18]),FALSE};
      if (change.selection >= 0 || change.components >= 0 || change.lock >= 0)
        {
          g_assert_cmpint (atoi (cols[13]), >, 0);
          g_assert_cmpuint (phase, >, 0); g_assert_cmpint (atoi (cols[25]), ==, 1);
          /* The old phase3 progress1 request can occur after its FIRST ROI
           * row. It is an observed mutation point, never completion evidence.
           * All phases retain the start ROI and use the later final context. */
          g_signal_connect (filter,"filter-state-changed",G_CALLBACK (threshold_old_late_context),&change);
        }
      sobel_define (filter,flags[0],flags[1],flags[2]); generation = gimp_filter_layer_get_generation (filter);
      for (guint pass = 0; pass < passes; ++pass)
        {
          if (pass)
            {
              bytes = threshold_fixture (directory,cols[0],"source2",(gsize) width*height*source_channels);
              if (rerun >= 2)
                {
                  guchar *original = threshold_fixture (directory,cols[0],"source",(gsize) width*height*source_channels);
                  g_assert_cmpmem (bytes,(gsize) width*height*source_channels,original,(gsize) width*height*source_channels);
                  g_free (original);
                }
              else
                gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (source)),NULL,0,source_format,bytes,GEGL_AUTO_ROWSTRIDE);
              g_free (bytes);
              if (rerun >= 3)
                {
                  flags[rerun-3] = !flags[rerun-3];
                  sobel_define (filter,flags[0],flags[1],flags[2]);
                }
              gimp_drawable_update (GIMP_DRAWABLE (source),0,0,width,height);
            }
          sobel_wait (filter,GIMP_FILTER_LAYER_CLEAN);
          bytes = threshold_fixture (directory,cols[0],pass ? "output2" : "output",(gsize) width*height*channels);
          g_test_message ("Genuine old Sobel %s pass=%u native_channels=%u",cols[0],pass+1,channels);
          sobel_compare (filter,bytes,channels); g_free (bytes);
          g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, pass+1);
          if (!pass) g_assert_cmpuint (gimp_filter_layer_get_generation (filter), ==, generation);
          ++comparisons;
          if (!pass && (atoi (cols[27]) >= 0 || atoi (cols[28]) >= 0 || atoi (cols[29]) >= 0))
            {
              const guint64 settled_generation = gimp_filter_layer_get_generation (filter);
              GeglBuffer *cache = g_object_ref (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)));
              if (atoi (cols[27]) >= 0)
                {
                  bytes = threshold_fixture (directory,cols[0],"idle-mask",(gsize) image_width*image_height);
                  blinds_fixture_selection (image,image_width,image_height,0,0,bytes,atoi (cols[27]) == 0); g_free (bytes);
                }
              threshold_components (image,filter,channels,atoi (cols[28]),atoi (cols[29])); spin_ms (25);
              g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 1);
              g_assert_cmpuint (gimp_filter_layer_get_generation (filter), ==, settled_generation);
              g_assert_true (cache == gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)));
              g_assert_cmpint (atoi (cols[30]), ==, 1); g_assert_cmpint (atoi (cols[31]), ==, 1);
              bytes = threshold_fixture (directory,cols[0],"idle-output",(gsize) width*height*channels);
              sobel_compare (filter,bytes,channels); g_free (bytes); g_object_unref (cache);
            }
        }
      if (change.selection >= 0 || change.components >= 0 || change.lock >= 0) g_assert_true (change.called);
      g_signal_handlers_disconnect_by_data (filter,&change);
      g_object_unref (image); g_strfreev (cols); ++scenes;
    }
  /* Exact corpus membership belongs to the sealed fixture materializer. These
   * coverage assertions are requirements, not invented observed scene totals. */
  g_assert_cmpuint (scenes, >, 0); g_assert_cmpuint (comparisons, >=, scenes);
  g_assert_cmpuint (layouts, ==, 15); g_assert_cmpuint (reruns, ==, 63); g_assert_cmpuint (phases, ==, 15);
  g_test_message ("Sobel genuine old live scenes=%u comparisons=%u",scenes,comparisons);
  g_strfreev (lines); g_free (text); g_free (path);
}

static void
sobel_old_run_flags (gchar **lines, const gchar *id, guint run, gint flags[3])
{
  guint matches = 0;
  for (guint row = 0; lines[row]; ++row)
    {
      gchar **cols;
      if (!*lines[row]) continue;
      cols = g_strsplit (lines[row],"\t",-1);
      g_assert_cmpuint (g_strv_length (cols), ==, 5);
      if (!strcmp (cols[0],id) && atoi (cols[1]) == (gint) run)
        {
          for (guint i = 0; i < 3; ++i) flags[i] = atoi (cols[i+2]);
          ++matches;
        }
      g_strfreev (cols);
    }
  g_assert_cmpuint (matches, ==, 1);
}

static void
sobel_old_run_selection (const gchar *directory, const gchar *id,
                         gchar **rois, GimpImage *image, GimpFilterLayer *filter, guint run)
{
  const guint width = gimp_image_get_width (image), height = gimp_image_get_height (image);
  gchar label[32];
  gboolean empty = TRUE;
  guint matches = 0;
  guchar *mask;
  g_snprintf (label,sizeof label,"start%u-mask",run);
  mask = threshold_fixture (directory,id,label,(gsize) width*height);
  for (gsize i = 0; i < (gsize) width*height; ++i) empty &= mask[i] == 0;
  blinds_fixture_selection (image,width,height,0,0,mask,empty); g_free (mask);
  for (guint row = 0; rois[row]; ++row)
    {
      gchar **cols;
      if (!*rois[row]) continue;
      cols = g_strsplit (rois[row],"\t",-1);
      g_assert_cmpuint (g_strv_length (cols), ==, 7);
      if (!strcmp (cols[0],id) && atoi (cols[1]) == (gint) run)
        {
          gint x1,y1,x2,y2;
          const gboolean selected = gimp_item_mask_bounds (GIMP_ITEM (filter),&x1,&y1,&x2,&y2);
          g_assert_cmpint (selected, ==, atoi (cols[2]));
          g_assert_cmpint (x1, ==, atoi (cols[3])); g_assert_cmpint (y1, ==, atoi (cols[4]));
          g_assert_cmpint (x2, ==, atoi (cols[5])); g_assert_cmpint (y2, ==, atoi (cols[6]));
          ++matches;
        }
      g_strfreev (cols);
    }
  g_assert_cmpuint (matches, ==, 1);
}

static void
sobel_actual_old_projection_history (void)
{
  const gchar *directory = g_getenv ("GIMP_PAINTER_SOBEL_PROJECTION_FIXTURES");
  gchar *path, *text = NULL, *copy_text = NULL, *option_text = NULL, *roi_text = NULL;
  gchar **lines, **copies, **options, **rois;
  guint scenes = 0, comparisons = 0, history_checks = 0, loaded_checks = 0, halo_scenes = 0;
  guint combinations = 0, copy_rows = 0, option_rows = 0, roi_rows = 0;
  g_assert_nonnull (directory);
  path = g_build_filename (directory,"captures.tsv",NULL);
  g_assert_true (g_file_get_contents (path,&text,NULL,NULL)); g_free (path);
  path = g_build_filename (directory,"copies.tsv",NULL);
  g_assert_true (g_file_get_contents (path,&copy_text,NULL,NULL)); g_free (path);
  path = g_build_filename (directory,"options.tsv",NULL);
  g_assert_true (g_file_get_contents (path,&option_text,NULL,NULL)); g_free (path);
  path = g_build_filename (directory,"rois.tsv",NULL);
  g_assert_true (g_file_get_contents (path,&roi_text,NULL,NULL)); g_free (path);
  lines = g_strsplit (text,"\n",-1); copies = g_strsplit (copy_text,"\n",-1);
  options = g_strsplit (option_text,"\n",-1); rois = g_strsplit (roi_text,"\n",-1);
  for (guint row = 0; copies[row]; ++row) copy_rows += *copies[row] != '\0';
  for (guint row = 0; options[row]; ++row) option_rows += *options[row] != '\0';
  for (guint row = 0; rois[row]; ++row) roi_rows += *rois[row] != '\0';
  for (guint row = 0; lines[row]; ++row)
    {
      const guint width = 17, height = 13, image_width = 13, image_height = 11;
      gchar **cols;
      guint channels, combination;
      gint grouped, seeded, ox, oy, final_x, final_y, flags[3], previous_flags[3];
      gboolean halo;
      gsize size;
      const Babl *format;
      GimpImage *image;
      GimpLayer *parent, *source;
      GimpFilterLayer *filter;
      guchar *source_bytes, *bytes;
      if (!*lines[row]) continue;
      cols = g_strsplit (lines[row],"\t",-1);
      g_assert_cmpuint (g_strv_length (cols), ==, 14);
      channels = atoi (cols[1]); grouped = atoi (cols[2]); seeded = atoi (cols[3]);
      ox = atoi (cols[4]); oy = atoi (cols[5]); final_x = atoi (cols[6]); final_y = atoi (cols[7]);
      halo = g_str_has_suffix (cols[0],"-halo"); halo_scenes += halo;
      g_assert_true (channels == 2 || channels == 4);
      g_assert_true (grouped == 0 || grouped == 1); g_assert_true (seeded == 0 || seeded == 1);
      g_assert_true ((ox == -4 && oy == -3) || (ox == 3 && oy == 2));
      g_assert_cmpint (final_x, ==, ox < 0 ? 3 : -4); g_assert_cmpint (final_y, ==, oy < 0 ? 2 : -3);
      for (guint c = 8; c < 12; ++c) g_assert_cmpint (atoi (cols[c]), ==, 3);
      g_assert_cmpint (atoi (cols[12]), ==, seeded ? 0 : -1);
      g_assert_cmpint (atoi (cols[13]), ==, seeded ? 0 : -1);
      combination = (channels == 4 ? 16 : 0) | (halo ? 8 : 0) | (grouped ? 4 : 0) | (ox < 0 ? 2 : 0) | seeded;
      g_assert_cmpuint (combinations & (1u << combination), ==, 0); combinations |= 1u << combination;
      size = (gsize) width*height*channels;
      format = babl_format (channels == 2 ? "Y'A u8" : "R'G'B'A u8");
      image = gimp_image_new (gimp,image_width,image_height,channels == 2 ? GIMP_GRAY : GIMP_RGB,GIMP_PRECISION_U8_NON_LINEAR);
      parent = grouped ? gimp_group_layer_new (image) : NULL;
      if (parent) gimp_image_add_layer (image,parent,NULL,0,FALSE);
      source = gimp_layer_new (image,width,height,format,"Old Sobel projection source",1,GIMP_LAYER_MODE_NORMAL_LEGACY);
      gimp_image_add_layer (image,source,parent,0,FALSE); filter = filter_new (image,parent,width,height);
      gimp_item_set_offset (GIMP_ITEM (source),ox,oy); gimp_item_set_offset (GIMP_ITEM (filter),ox,oy);
      source_bytes = threshold_fixture (directory,cols[0],"source",size);
      gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (source)),NULL,0,format,source_bytes,GEGL_AUTO_ROWSTRIDE);
      threshold_components (image,filter,channels,(1u << channels)-1,0);
      bytes = threshold_fixture (directory,cols[0],"seed",size);
      if (seeded) gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)),NULL,0,format,bytes,GEGL_AUTO_ROWSTRIDE);
      sobel_compare (filter,bytes,channels); g_free (bytes);
      sobel_old_run_flags (options,cols[0],1,previous_flags);
      sobel_define (filter,previous_flags[0],previous_flags[1],previous_flags[2]);
      if (seeded)
        {
          GeglBuffer *cache;
          guint64 generation, cache_generation;
          gimp_filter_layer_mark_as_loaded (filter);
          cache = g_object_ref (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)));
          generation = gimp_filter_layer_get_generation (filter);
          cache_generation = gimp_filter_layer_get_cache_generation (filter);
          spin_ms (50);
          g_assert_cmpint (gimp_filter_layer_get_state (filter), ==, GIMP_FILTER_LAYER_CLEAN);
          g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 0);
          g_assert_cmpuint (gimp_filter_layer_get_generation (filter), ==, generation);
          g_assert_cmpuint (gimp_filter_layer_get_cache_generation (filter), ==, cache_generation);
          g_assert_true (cache == gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)));
          bytes = threshold_fixture (directory,cols[0],"loaded-cache",size);
          sobel_compare (filter,bytes,channels); g_free (bytes); g_object_unref (cache); ++loaded_checks;
        }
      for (guint run = 1; run <= 3; ++run)
        {
          gchar label[32];
          guchar *before, *owned, *reconstructed = g_malloc (size);
          if (run == 3) gimp_item_translate (GIMP_ITEM (filter),final_x-ox,final_y-oy,FALSE);
          sobel_old_run_selection (directory,cols[0],rois,image,filter,run);
          sobel_old_run_flags (options,cols[0],run,flags);
          if (memcmp (flags,previous_flags,sizeof flags)) sobel_define (filter,flags[0],flags[1],flags[2]);
          memcpy (previous_flags,flags,sizeof flags);
          g_snprintf (label,sizeof label,"before%u",run);
          before = threshold_fixture (directory,cols[0],label,size);
          sobel_compare (filter,before,channels); ++history_checks;
          g_snprintf (label,sizeof label,"source%u",run);
          bytes = threshold_fixture (directory,cols[0],label,size);
          g_assert_cmpmem (bytes,size,source_bytes,size); g_free (bytes);
          g_assert_cmpint (gimp_item_get_offset_x (GIMP_ITEM (source)), ==, ox);
          g_assert_cmpint (gimp_item_get_offset_y (GIMP_ITEM (source)), ==, oy);
          gimp_drawable_update (GIMP_DRAWABLE (source),0,0,width,height); sobel_wait (filter,GIMP_FILTER_LAYER_CLEAN);
          threshold_projection_copy_geometry (copies,cols[0],filter,run,channels);
          g_snprintf (label,sizeof label,"start%u",run);
          owned = threshold_fixture (directory,cols[0],label,size);
          sobel_owned_reference (filter,before,reconstructed);
          g_assert_cmpmem (reconstructed,size,owned,size);
          g_free (before); g_free (owned); g_free (reconstructed);
          g_snprintf (label,sizeof label,"output%u",run);
          bytes = threshold_fixture (directory,cols[0],label,size);
          g_test_message ("Genuine old Sobel history %s run=%u output",cols[0],run);
          sobel_compare (filter,bytes,channels); g_free (bytes);
          g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, run); ++comparisons;
        }
      g_free (source_bytes); g_object_unref (image); g_strfreev (cols); ++scenes;
    }
  g_assert_cmpuint (scenes, >, 0); g_assert_cmpuint (halo_scenes, >, 0); g_assert_cmpuint (loaded_checks, >, 0);
  g_assert_cmpuint (comparisons, ==, scenes*3); g_assert_cmpuint (history_checks, ==, comparisons);
  g_assert_cmpuint (copy_rows, ==, comparisons); g_assert_cmpuint (option_rows, ==, comparisons);
  g_assert_cmpuint (roi_rows, ==, comparisons);
  g_test_message ("Sobel genuine projection history scenes=%u comparisons=%u before_copy=%u loaded_idle=%u halo_scenes=%u",
                  scenes,comparisons,history_checks,loaded_checks,halo_scenes);
  g_strfreev (lines); g_strfreev (copies); g_strfreev (options); g_strfreev (rois);
  g_free (text); g_free (copy_text); g_free (option_text); g_free (roi_text);
}

