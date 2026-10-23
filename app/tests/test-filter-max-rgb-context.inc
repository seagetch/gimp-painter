/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Synthetic/source-backed in-process Max RGB owner tests. These fixtures and
 * scalar expectations are NOT old-runtime captures or XCF acceptance evidence.
 * Genuine application captures and persistence remain separate gates. */
static GimpValueArray *
max_rgb_args (gint mode)
{
  return gimp_value_array_new_from_types (NULL,
    G_TYPE_INT,1,G_TYPE_INT,-123,G_TYPE_INT,456,G_TYPE_INT,mode,G_TYPE_NONE);
}

static void
max_rgb_set_args (GimpFilterLayer *filter, GimpValueArray *args)
{
  static const guchar saved[] = {0,0xff,2,0x80,0,3,0};
  GBytes *raw = g_bytes_new_static (saved,sizeof saved);
  GError *error = NULL;
  g_assert_true (gimp_filter_layer_set_definition (filter,"plug-in-max-rgb",raw,args,&error));
  g_assert_no_error (error); g_bytes_unref (raw);
}

static void
max_rgb_define (GimpFilterLayer *filter, gint mode)
{
  GimpValueArray *args = max_rgb_args (mode);
  max_rgb_set_args (filter,args); gimp_value_array_unref (args);
}

static void
max_rgb_saved_args (GimpFilterLayer *filter, GimpValueArray *expected)
{
  static const guchar saved[] = {0,0xff,2,0x80,0,3,0};
  GBytes *raw = gimp_filter_layer_ref_definition (filter);
  GimpValueArray *actual = gimp_filter_layer_dup_args (filter);
  gchar *name = gimp_filter_layer_dup_procedure (filter);
  g_assert_cmpstr (name, ==, "plug-in-max-rgb");
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
max_rgb_wait (GimpFilterLayer *filter, GimpFilterLayerState expected)
{
  const gint64 deadline = g_get_monotonic_time () + 90 * G_TIME_SPAN_SECOND;
  while (gimp_filter_layer_get_state (filter) != expected && g_get_monotonic_time () < deadline)
    {
      if (gimp_filter_layer_get_state (filter) == GIMP_FILTER_LAYER_FAILED && expected != GIMP_FILTER_LAYER_FAILED)
        { gchar *error = gimp_filter_layer_dup_error (filter); g_error ("Max RGB failed: %s",error); }
      g_main_context_iteration (NULL,FALSE); g_usleep (100);
    }
  g_assert_cmpint (gimp_filter_layer_get_state (filter), ==, expected);
}

static void
max_rgb_compare (GimpFilterLayer *filter, const guchar *expected, guint channels)
{
  const guint width = gimp_item_get_width (GIMP_ITEM (filter));
  const guint height = gimp_item_get_height (GIMP_ITEM (filter));
  const Babl *format = babl_format ("R'G'B'A u8");
  guchar *actual = g_malloc ((gsize) width*height*channels);
  g_assert_cmpuint (channels, ==, 4);
  gegl_buffer_get (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)),NULL,1,format,
                   actual,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
  for (gsize i = 0; i < (gsize) width*height*channels; ++i)
    if (actual[i] != expected[i])
      {
        g_test_message ("Max RGB first mismatch local=%u,%u channel=%u actual=%u expected=%u owner_offset=%d,%d",
                        (guint) (i/channels % width),(guint) (i/channels / width),(guint) (i % channels),
                        actual[i],expected[i],gimp_item_get_offset_x (GIMP_ITEM (filter)),
                        gimp_item_get_offset_y (GIMP_ITEM (filter)));
        break;
      }
  g_assert_cmpmem (actual,(gsize) width*height*channels,expected,(gsize) width*height*channels);
  g_free (actual);
}

static void
max_rgb_owned_input (GimpFilterLayer *filter, const Babl *format, gpointer input)
{
  GeglNode *below = gegl_node_get_input_proxy (gimp_filter_get_node (GIMP_FILTER (filter)),"input");
  const GeglRectangle rect = {gimp_item_get_offset_x (GIMP_ITEM (filter)),
    gimp_item_get_offset_y (GIMP_ITEM (filter)),gimp_item_get_width (GIMP_ITEM (filter)),
    gimp_item_get_height (GIMP_ITEM (filter))};
  gegl_node_blit (below,1,&rect,format,input,GEGL_AUTO_ROWSTRIDE,GEGL_BLIT_CACHE);
}

static gint
max_rgb_floor_256 (gint value)
{ return value >= 0 ? value/256 : -((-value+255)/256); }

static guint
max_rgb_divide_nearest (guint numerator, guint denominator)
{ return numerator/denominator + (numerator%denominator > denominator/2); }

static void
max_rgb_merge_reference (const guchar *input, const guchar *shadow,
                       const guchar *mask, guchar *output,
                       gsize pixels, guint channels, guint active)
{
  for (gsize p = 0; p < pixels; ++p)
    {
      const guint at = channels-1, coverage = mask ? mask[p] : 255;
      const gint product = ((gint) shadow[at]-input[at])*coverage+128;
      const guint alpha = input[at]+max_rgb_floor_256 (max_rgb_floor_256 (product)+product);
      const guint ratio = alpha ? max_rgb_divide_nearest (coverage*shadow[at],alpha) : 0;
      for (guint c = 0; c < at; ++c)
        {
          gint value = input[c];
          if (alpha && (active & (1u << c)))
            value += shadow[c] > input[c] ?
              (gint) max_rgb_divide_nearest ((shadow[c]-input[c])*ratio,255) :
              -(gint) max_rgb_divide_nearest ((input[c]-shadow[c])*ratio,255);
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
max_rgb_owned_reference (GimpFilterLayer *filter, const guchar *previous, guchar *input)
{
  GimpItem *item = GIMP_ITEM (filter), *parent = gimp_item_get_parent (item);
  GimpImage *image = gimp_item_get_image (item);
  const gint width = gimp_item_get_width (item), height = gimp_item_get_height (item);
  const gint ox = gimp_item_get_offset_x (item), oy = gimp_item_get_offset_y (item);
  const gint px = parent ? gimp_item_get_offset_x (parent) : 0;
  const gint py = parent ? gimp_item_get_offset_y (parent) : 0;
  const gint pw = parent ? gimp_item_get_width (parent) : gimp_image_get_width (image);
  const gint ph = parent ? gimp_item_get_height (parent) : gimp_image_get_height (image);
  const guint channels = 4;
  const gsize size = (gsize) width*height*channels;
  guchar *lower = g_malloc (size);
  g_assert_cmpint (gimp_drawable_get_base_type (GIMP_DRAWABLE (filter)), ==, GIMP_RGB);
  max_rgb_owned_input (filter,gimp_drawable_get_format (GIMP_DRAWABLE (filter)),lower);
  if (previous) memcpy (input,previous,size); else memset (input,0,size);
  for (gint y = 0; y < height; ++y)
    for (gint x = 0; x < width; ++x)
      if (x+ox >= px && x+ox < px+pw && y+oy >= py && y+oy < py+ph)
        memcpy (input+((gsize) y*width+x)*channels,lower+((gsize) y*width+x)*channels,channels);
  g_free (lower);
}

/* Plain scalar legacy callback and REPLACE_INTEN formula, intentionally
 * independent of production kernels/wrappers and owner-side merge helpers. */
static void
max_rgb_raw_reference (const guchar *input, guchar *shadow, guint width, guint height,
                       guint x1, guint y1, guint x2, guint y2, gint mode)
{
  memset (shadow,0,(gsize) width*height*4);
  for (guint y = y1; y < y2; ++y)
    for (guint x = x1; x < x2; ++x)
      {
        const gsize p = ((gsize) y*width+x)*4;
        const guchar value = mode > 0 ? MAX (input[p],MAX (input[p+1],input[p+2])) :
                                        MIN (input[p],MIN (input[p+1],input[p+2]));
        for (guint c = 0; c < 3; ++c) shadow[p+c] = input[p+c] == value ? input[p+c] : 0;
        shadow[p+3] = input[p+3];
      }
}

static void
max_rgb_reference (const guchar *input, guchar *output,
                   guint width, guint height, guint channels, gint mode)
{
  guchar *shadow = g_malloc ((gsize) width*height*4);
  g_assert_cmpuint (channels, ==, 4);
  max_rgb_raw_reference (input,shadow,width,height,0,0,width,height,mode);
  max_rgb_merge_reference (input,shadow,NULL,output,(gsize) width*height,4,15);
  g_free (shadow);
}

static void
max_rgb_pattern (guchar *input, guint width, guint height, guint channels)
{
  static const guchar colors[][3] = {{17,91,203},{203,203,17},{17,203,203},
    {203,17,203},{17,17,203},{203,17,17},{17,203,17},{77,77,77},{0,0,0},{255,255,255}};
  static const guchar alpha[] = {0,1,63,127,128,254,255};
  for (gsize i = 0; i < (gsize) width*height; ++i)
    {
      memcpy (input+i*channels,colors[i % G_N_ELEMENTS (colors)],3);
      if (channels == 4) input[i*channels+3] = alpha[(i/3) % G_N_ELEMENTS (alpha)];
    }
}

static void
max_rgb_integer_native_route (void)
{
  const gint modes[] = {G_MININT,-257,-1,0,1,257,G_MAXINT};
  const guint widths[] = {1,1,7,2,17,67}, heights[] = {1,7,1,2,13,66};
  for (guint alpha = 0; alpha < 2; ++alpha)
    for (guint shape = 0; shape < G_N_ELEMENTS (widths); ++shape)
      {
        const guint width = widths[shape], height = heights[shape], channels = alpha ? 4 : 3;
        const gsize size = (gsize) width*height*4;
        const Babl *format = babl_format (alpha ? "R'G'B'A u8" : "R'G'B' u8");
        GimpImage *image = image_new (width,height);
        GimpLayer *source = gimp_layer_new (image,width,height,format,"Max RGB source",1,GIMP_LAYER_MODE_NORMAL_LEGACY);
        GimpFilterLayer *filter;
        guchar *input = g_malloc (size), *owned = g_malloc (size), *expected = g_malloc (size);
        gimp_image_add_layer (image,source,NULL,0,FALSE); filter = filter_new (image,NULL,width,height);
        max_rgb_pattern (input,width,height,channels);
        gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (source)),NULL,0,format,input,GEGL_AUTO_ROWSTRIDE);
        for (guint m = 0; m < G_N_ELEMENTS (modes); ++m)
          {
            GimpValueArray *args = max_rgb_args (modes[m]);
            /* All context slots retain signed bits too; they are recorded
             * data, never current object identifiers or a run-mode clamp. */
            g_value_set_int (gimp_value_array_index (args,0),modes[m]);
            g_value_set_int (gimp_value_array_index (args,1),modes[(m+2) % G_N_ELEMENTS (modes)]);
            g_value_set_int (gimp_value_array_index (args,2),modes[G_N_ELEMENTS (modes)-1-m]);
            max_rgb_set_args (filter,args); max_rgb_wait (filter,GIMP_FILTER_LAYER_CLEAN);
            max_rgb_owned_reference (filter,NULL,owned);
            max_rgb_reference (owned,expected,width,height,4,modes[m]);
            max_rgb_compare (filter,expected,4); max_rgb_saved_args (filter,args);
            g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, m+1);
            gimp_value_array_unref (args);
          }
        g_free (input); g_free (owned); g_free (expected); g_object_unref (image);
      }
}

static void
max_rgb_invalid_definitions_keep_cache (void)
{
  GimpImage *image = image_new (7,5);
  GimpLayer *source = source_new (image,NULL,7,5);
  GimpFilterLayer *filter = filter_new (image,NULL,7,5);
  GeglBuffer *cache;
  guint64 generation;
  fill (source,31,121,217,255); max_rgb_define (filter,0);
  max_rgb_wait (filter,GIMP_FILTER_LAYER_CLEAN);
  cache = g_object_ref (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)));
  generation = gimp_filter_layer_get_cache_generation (filter);
  for (guint kind = 0; kind < 6; ++kind)
    {
      GimpValueArray *args = max_rgb_args (10);
      if (kind < 4)
        {
          GValue *value = gimp_value_array_index (args,kind);
          g_value_unset (value); g_value_init (value,G_TYPE_DOUBLE); g_value_set_double (value,10);
        }
      else if (kind == 4) gimp_value_array_truncate (args,3);
      else if (kind == 5)
        {
          GValue value = G_VALUE_INIT;
          g_value_init (&value,G_TYPE_INT); g_value_set_int (&value,5);
          gimp_value_array_append (args,&value); g_value_unset (&value);
        }
      max_rgb_set_args (filter,args); max_rgb_wait (filter,GIMP_FILTER_LAYER_FAILED);
      g_assert_true (cache == gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)));
      g_assert_cmpuint (gimp_filter_layer_get_cache_generation (filter), ==, generation);
      g_assert_cmpuint (gimp_filter_layer_get_generation (filter), >, generation);
      g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 1);
      max_rgb_saved_args (filter,args); pixel (GIMP_LAYER (filter),31,0,0,255);
      gimp_value_array_unref (args);
    }
  max_rgb_define (filter,10); max_rgb_wait (filter,GIMP_FILTER_LAYER_CLEAN);
  g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 2);
  g_object_unref (cache); g_object_unref (image);
}

static void
max_rgb_gray_unsupported (void)
{
  GimpImage *image = gimp_image_new (gimp,7,5,GIMP_GRAY,GIMP_PRECISION_U8_NON_LINEAR);
  GimpFilterLayer *filter = filter_new (image,NULL,7,5);
  const guchar saved[] = {91,123};
  guchar actual[2];
  GeglBuffer *cache = g_object_ref (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)));
  GimpValueArray *args = max_rgb_args (G_MININT);
  guint64 generation;
  max_rgb_set_args (filter,args);
  gegl_buffer_set (cache,GEGL_RECTANGLE (0,0,1,1),0,babl_format ("Y'A u8"),saved,GEGL_AUTO_ROWSTRIDE);
  gimp_filter_layer_mark_as_loaded (filter); generation = gimp_filter_layer_get_cache_generation (filter);
  gimp_filter_layer_invalidate (filter); max_rgb_wait (filter,GIMP_FILTER_LAYER_FAILED);
  g_assert_true (cache == gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)));
  g_assert_cmpuint (gimp_filter_layer_get_cache_generation (filter), ==, generation);
  g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 0);
  gegl_buffer_get (cache,GEGL_RECTANGLE (0,0,1,1),1,babl_format ("Y'A u8"),actual,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
  g_assert_cmpmem (actual,2,saved,2); max_rgb_saved_args (filter,args);
  g_object_unref (cache); gimp_value_array_unref (args); g_object_unref (image);
}

static void
max_rgb_tiny_selection_routes (void)
{
  const guint widths[] = {1,1,7,2}, heights[] = {1,7,1,2};
  for (guint crop = 0; crop < G_N_ELEMENTS (widths); ++crop)
    {
      const guint width = 11, height = 11, channels = 4;
      const gint ox = crop & 1 ? -2 : 3, oy = crop & 1 ? 3 : -2;
      GimpImage *image = gimp_image_new (gimp,17,17,GIMP_RGB,GIMP_PRECISION_U8_NON_LINEAR);
      const Babl *format = gimp_image_get_layer_format (image,TRUE);
      GimpLayer *source = gimp_layer_new (image,width,height,format,"Tiny Max RGB crop",1,GIMP_LAYER_MODE_NORMAL_LEGACY);
      GimpFilterLayer *filter;
      guchar input[11*11*4], shadow[11*11*4], expected[11*11*4];
      gimp_image_add_layer (image,source,NULL,0,FALSE); filter = filter_new (image,NULL,width,height);
      gimp_item_set_offset (GIMP_ITEM (source),ox,oy); gimp_item_set_offset (GIMP_ITEM (filter),ox,oy);
      for (guint i = 0; i < width*height; ++i)
        {
          for (guint c = 0; c + 1 < channels; ++c) input[i*channels+c] = 31+c*61;
          input[i*channels+channels-1] = 255;
        }
      gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (source)),NULL,0,format,input,GEGL_AUTO_ROWSTRIDE);
      gimp_channel_select_rectangle (gimp_image_get_mask (image),ox+3,oy+3,widths[crop],heights[crop],
                                     GIMP_CHANNEL_OP_REPLACE,FALSE,0,0,FALSE);
      max_rgb_define (filter,255); max_rgb_wait (filter,GIMP_FILTER_LAYER_CLEAN);
      max_rgb_owned_input (filter,format,input);
      for (guint y = 0; y < height; ++y)
        for (guint x = 0; x < width; ++x)
          if ((gint) x+ox < 0 || (gint) y+oy < 0 || (gint) x+ox >= 17 || (gint) y+oy >= 17)
            memset (input+(y*width+x)*channels,0,channels);
      max_rgb_raw_reference (input,shadow,width,height,3,3,3+widths[crop],3+heights[crop],255);
      memcpy (expected,input,(gsize) width*height*channels);
      for (guint y = 3; y < 3+heights[crop]; ++y)
        for (guint x = 3; x < 3+widths[crop]; ++x)
          memcpy (expected+(y*width+x)*4,shadow+(y*width+x)*4,4);
      max_rgb_compare (filter,expected,channels);
      /* Sharing the owner carrier must not weaken Sharpen's 2x3 minimum. */
      sharpen_define (filter,10); max_rgb_wait (filter,GIMP_FILTER_LAYER_FAILED);
      max_rgb_compare (filter,expected,channels);
      g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 1);
      g_object_unref (image);
    }
}

typedef struct
{
  GimpImage *image;
  guint width, height, kind;
  gint ox, oy;
  gboolean called;
} MaxRGBContextChange;

static void
max_rgb_components (GimpImage *image, GimpFilterLayer *filter, gint active, gint lock)
{
  if (active >= 0)
    {
      gimp_image_set_component_active (image,GIMP_CHANNEL_RED,active & 1);
      gimp_image_set_component_active (image,GIMP_CHANNEL_GREEN,active & 2);
      gimp_image_set_component_active (image,GIMP_CHANNEL_BLUE,active & 4);
      gimp_image_set_component_active (image,GIMP_CHANNEL_ALPHA,active & 8);
    }
  if (lock >= 0) gimp_layer_set_lock_alpha (GIMP_LAYER (filter),lock,FALSE);
}

static void
max_rgb_late_context (GimpFilterLayer *filter, gpointer data)
{
  MaxRGBContextChange *state = data;
  guchar *mask;
  if (state->called || gimp_filter_layer_get_state (filter) != GIMP_FILTER_LAYER_IMPORTING) return;
  state->called = TRUE;
  mask = g_malloc ((gsize) state->width*state->height);
  memset (mask,state->kind == 2 ? 128 : 255,(gsize) state->width*state->height);
  blinds_fixture_selection (state->image,state->width,state->height,state->ox,state->oy,mask,state->kind == 1);
  g_free (mask);
  if (state->kind == 3) max_rgb_components (state->image,filter,1,-1);
  if (state->kind == 4) gimp_image_set_component_active (state->image,GIMP_CHANNEL_ALPHA,FALSE);
  if (state->kind == 5) gimp_layer_set_lock_alpha (GIMP_LAYER (filter),TRUE,FALSE);
  if (state->kind == 7)
    gimp_channel_select_rectangle (gimp_image_get_mask (state->image),0,0,1,1,
                                   GIMP_CHANNEL_OP_REPLACE,FALSE,0,0,FALSE);
}

static void
max_rgb_late_context_routes (void)
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
        source = source_new (image,parent,width,height); filter = filter_new (image,parent,width,height);
        gimp_item_set_offset (GIMP_ITEM (source),ox,oy); gimp_item_set_offset (GIMP_ITEM (filter),ox,oy);
        fill (source,31,121,217,255);
        gimp_channel_select_rectangle (gimp_image_get_mask (image),ox+x1,oy+y1,x2-x1,y2-y1,
                                       GIMP_CHANNEL_OP_REPLACE,FALSE,0,0,FALSE);
        if (kind == 6) parent->lock_alpha = TRUE; /* Only the target's own lock participates. */
        max_rgb_define (filter,1);
        fill (GIMP_LAYER (filter),17,29,53,255); gimp_filter_layer_mark_as_loaded (filter);
        cache = g_object_ref (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)));
        cache_generation = gimp_filter_layer_get_cache_generation (filter);
        g_signal_connect (filter,"filter-state-changed",G_CALLBACK (max_rgb_late_context),&change);
        gimp_filter_layer_invalidate (filter); generation = gimp_filter_layer_get_generation (filter);
        blinds_lifecycle_settle (filter,cache,cache_generation,generation);
        g_assert_true (change.called);
        max_rgb_owned_reference (filter,NULL,input);
        max_rgb_raw_reference (input,shadow,width,height,x1,y1,x2,y2,1);
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
            max_rgb_raw_reference (input,shadow,width,height,0,0,width,height,1);
            max_rgb_merge_reference (input,shadow,mask,expected,pixels,4,kind == 3 ? 1 : kind == 4 || kind == 5 ? 7 : 15);
          }
        max_rgb_compare (filter,expected,4);
        g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 2);
        g_signal_handlers_disconnect_by_data (filter,&change);
        g_free (input); g_free (shadow); g_free (expected); g_free (mask); g_object_unref (cache); g_object_unref (image);
      }
}

static void
max_rgb_component_masks (void)
{
  const guint width = 17, height = 13, channels = 4;
  const gsize pixels = width*height, size = pixels*channels;
  GimpImage *image = gimp_image_new (gimp,width,height,GIMP_RGB,GIMP_PRECISION_U8_NON_LINEAR);
  const Babl *format = gimp_image_get_layer_format (image,TRUE);
  GimpLayer *source = gimp_layer_new (image,width,height,format,"Max RGB components",1,GIMP_LAYER_MODE_NORMAL_LEGACY);
  GimpFilterLayer *filter;
  guchar *input = g_malloc (size), *shadow = g_malloc (size), *expected = g_malloc (size), *mask = g_malloc (pixels);
  gimp_image_add_layer (image,source,NULL,0,FALSE); filter = filter_new (image,NULL,width,height);
  max_rgb_pattern (input,width,height,channels); memset (mask,128,pixels);
  gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (source)),NULL,0,format,input,GEGL_AUTO_ROWSTRIDE);
  blinds_fixture_selection (image,width,height,0,0,mask,FALSE);
  for (guint active = 0; active < (1u << channels); ++active)
    {
      max_rgb_components (image,filter,active,0);
      max_rgb_define (filter,-7); max_rgb_wait (filter,GIMP_FILTER_LAYER_CLEAN);
      max_rgb_owned_reference (filter,NULL,input);
      max_rgb_raw_reference (input,shadow,width,height,0,0,width,height,-7);
      max_rgb_merge_reference (input,shadow,mask,expected,pixels,channels,active);
      max_rgb_compare (filter,expected,channels);
      g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, active+1);
    }
  g_free (input); g_free (shadow); g_free (expected); g_free (mask); g_object_unref (image);
}

static void
max_rgb_freeze_context (GimpFilterLayer *filter, gpointer data)
{
  MaxRGBContextChange *state = data;
  if (state->called || gimp_filter_layer_get_state (filter) != GIMP_FILTER_LAYER_IMPORTING) return;
  state->called = TRUE;
  /* Known-empty final selection seals in the transition quantum. These edits
   * therefore occur after sealing, before the first staged import chunk. */
  gimp_channel_select_rectangle (gimp_image_get_mask (state->image),0,0,2,3,
                                 GIMP_CHANNEL_OP_REPLACE,FALSE,0,0,FALSE);
  gimp_image_set_component_active (state->image,GIMP_CHANNEL_RED,FALSE);
  gimp_image_set_component_active (state->image,GIMP_CHANNEL_GREEN,FALSE);
  gimp_image_set_component_active (state->image,GIMP_CHANNEL_BLUE,FALSE);
  gimp_image_set_component_active (state->image,GIMP_CHANNEL_ALPHA,FALSE);
  gimp_layer_set_lock_alpha (GIMP_LAYER (filter),TRUE,FALSE);
}

static void
max_rgb_sealed_context_routes (void)
{
  for (guint spill = 0; spill < 2; ++spill)
    {
      const guint width = spill ? 1031 : 513, height = spill ? 1021 : 257;
      GimpImage *image = image_new (width,height);
      GimpLayer *source = source_new (image,NULL,width,height);
      GimpFilterLayer *filter = filter_new (image,NULL,width,height);
      MaxRGBContextChange change = {image,width,height,0,0,0,FALSE};
      guchar *input = g_malloc0 ((gsize) width*height*4), *expected = g_malloc ((gsize) width*height*4);
      const gsize center = ((gsize) (height/2)*width+width/2)*4;
      for (gsize i = 0; i < (gsize) width*height; ++i) input[i*4+3] = 100;
      input[center] = 30; input[center+1] = 60; input[center+2] = 100;
      gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (source)),NULL,0,babl_format ("R'G'B'A u8"),input,GEGL_AUTO_ROWSTRIDE);
      g_assert_true (gimp_image_get_mask (image)->bounds_known); g_assert_true (gimp_image_get_mask (image)->empty);
      g_signal_connect (filter,"filter-state-changed",G_CALLBACK (max_rgb_freeze_context),&change);
      max_rgb_define (filter,127); max_rgb_wait (filter,GIMP_FILTER_LAYER_CLEAN);
      g_assert_true (change.called);
      max_rgb_owned_input (filter,babl_format ("R'G'B'A u8"),input);
      max_rgb_reference (input,expected,width,height,4,127); max_rgb_compare (filter,expected,4);
      g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 1);
      input[center] = 71;
      gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (source)),NULL,0,babl_format ("R'G'B'A u8"),input,GEGL_AUTO_ROWSTRIDE);
      gimp_drawable_update (GIMP_DRAWABLE (source),0,0,width,height); max_rgb_wait (filter,GIMP_FILTER_LAYER_CLEAN);
      max_rgb_compare (filter,input,4);
      g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 2);
      g_signal_handlers_disconnect_by_data (filter,&change);
      g_free (input); g_free (expected); g_object_unref (image);
    }
}

static void
max_rgb_outside_selection_no_merge (void)
{
  for (guint spill = 0; spill < 2; ++spill)
    {
      const guint width = spill ? 1031 : 63, height = spill ? 1021 : 61;
      GimpImage *image = image_new (width+4,height+4);
      GimpLayer *source = source_new (image,NULL,width,height);
      GimpFilterLayer *filter = filter_new (image,NULL,width,height);
      MaxRGBContextChange change = {image,width,height,1,0,0,FALSE};
      guchar *input = g_malloc0 ((gsize) width*height*4), *expected = g_malloc ((gsize) width*height*4);
      const gsize center = ((gsize) (height/2)*width+width/2)*4;
      for (gsize i = 0; i < (gsize) width*height; ++i) input[i*4+3] = 100;
      input[center] = 30; input[center+1] = 60; input[center+2] = 100;
      gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (source)),NULL,0,babl_format ("R'G'B'A u8"),input,GEGL_AUTO_ROWSTRIDE);
      gimp_channel_select_rectangle (gimp_image_get_mask (image),width+1,height+1,2,2,
                                     GIMP_CHANNEL_OP_REPLACE,FALSE,0,0,FALSE);
      g_signal_connect (filter,"filter-state-changed",G_CALLBACK (max_rgb_late_context),&change);
      max_rgb_define (filter,127); max_rgb_wait (filter,GIMP_FILTER_LAYER_CLEAN);
      g_assert_true (change.called);
      max_rgb_owned_input (filter,babl_format ("R'G'B'A u8"),input);
      max_rgb_compare (filter,input,4);
      g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 1);
      gimp_drawable_update (GIMP_DRAWABLE (source),0,0,width,height); max_rgb_wait (filter,GIMP_FILTER_LAYER_CLEAN);
      max_rgb_reference (input,expected,width,height,4,127); max_rgb_compare (filter,expected,4);
      g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 2);
      g_signal_handlers_disconnect_by_data (filter,&change);
      g_free (input); g_free (expected); g_object_unref (image);
    }
}

typedef struct
{
  GimpFilterLayerState phase;
  guint action;
  GimpLayer *source;
  gboolean called;
} MaxRGBInterrupt;

static void
max_rgb_interrupt (GimpFilterLayer *filter, gpointer data)
{
  MaxRGBInterrupt *state = data;
  if (state->called || gimp_filter_layer_get_state (filter) != state->phase) return;
  state->called = TRUE;
  if (state->action == 0) gimp_filter_layer_cancel (filter);
  else if (state->action == 1) max_rgb_define (filter,0);
  else if (state->action == 2) fill (state->source,79,137,193,255);
  else g_assert_true (gimp_painter_binding_close (G_OBJECT (filter),NULL));
}

static void
max_rgb_drain_workers (void)
{
  const gint64 deadline = g_get_monotonic_time () + 90 * G_TIME_SPAN_SECOND;
  while (gimp_test_filter_pending_jobs () && g_get_monotonic_time () < deadline)
    { g_main_context_iteration (NULL,FALSE); g_usleep (100); }
  g_assert_cmpuint (gimp_test_filter_pending_jobs (), ==, 0);
}

static void
max_rgb_cancel_replace_lower_close (void)
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
          GeglBuffer *cache;
          guint64 cache_generation, generation;
          MaxRGBInterrupt interrupt = {phases[phase],action,NULL,FALSE};
          guchar *input;
          gsize center;
          /* The spool may move straight from collecting to exporting within
           * one owner quantum. Never make timing-dependent RUNNING assertions
           * for it. Vector launch always emits that state deterministically. */
          if (spill && phases[phase] == GIMP_FILTER_LAYER_RUNNING) continue;
          image = image_new (width,height); source = source_new (image,NULL,width,height);
          filter = filter_new (image,NULL,width,height); interrupt.source = source;
          input = g_malloc0 ((gsize) width*height*4);
          center = ((gsize) (height/2)*width+width/2)*4;
          for (gsize i = 0; i < (gsize) width*height; ++i) input[i*4+3] = 255;
          input[center] = 30; input[center+1] = 60; input[center+2] = 100;
          gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (source)),NULL,0,babl_format ("R'G'B'A u8"),input,GEGL_AUTO_ROWSTRIDE);
          max_rgb_define (filter,255);
          fill (GIMP_LAYER (filter),17,29,53,255); gimp_filter_layer_mark_as_loaded (filter);
          cache = g_object_ref (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)));
          cache_generation = gimp_filter_layer_get_cache_generation (filter);
          g_signal_connect (filter,"filter-state-changed",G_CALLBACK (max_rgb_interrupt),&interrupt);
          gimp_filter_layer_invalidate (filter);
          generation = gimp_filter_layer_get_generation (filter);
          if (action == 1 || action == 2)
            {
              const gint64 deadline = g_get_monotonic_time () + 90 * G_TIME_SPAN_SECOND;
              while (!interrupt.called && g_get_monotonic_time () < deadline)
                {
                  blinds_lifecycle_cached (filter,cache,cache_generation);
                  g_main_context_iteration (NULL,FALSE); g_usleep (100);
                }
              g_assert_true (interrupt.called);
              generation = gimp_filter_layer_get_generation (filter);
              blinds_lifecycle_settle (filter,cache,cache_generation,generation);
              g_assert_true (interrupt.called);
              if (action == 1)
                { max_rgb_reference (input,input,width,height,4,0); max_rgb_compare (filter,input,4); }
              else blinds_lifecycle_constant (filter,(const guchar[]) {0,0,193,255});
              g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==,
                               phases[phase] == GIMP_FILTER_LAYER_RUNNING || phases[phase] == GIMP_FILTER_LAYER_IMPORTING ? 2 : 1);
            }
          else
            {
              max_rgb_wait (filter,action == 0 ? GIMP_FILTER_LAYER_FAILED : GIMP_FILTER_LAYER_CLOSED);
              g_assert_true (interrupt.called); max_rgb_drain_workers ();
              blinds_lifecycle_cached (filter,cache,cache_generation);
              if (action == 0)
                {
                  GimpValueArray *args = max_rgb_args (255);
                  max_rgb_saved_args (filter,args); gimp_value_array_unref (args);
                  fill (source,79,137,193,255); max_rgb_wait (filter,GIMP_FILTER_LAYER_CLEAN);
                  blinds_lifecycle_constant (filter,(const guchar[]) {0,0,193,255});
                }
            }
          g_signal_handlers_disconnect_by_data (filter,&interrupt);
          g_free (input); g_object_unref (cache); g_object_unref (image);
        }
}

static void
max_rgb_native_profile_bytes (void)
{
  for (guint spill = 0; spill < 2; ++spill)
    {
      const guint width = spill ? 1031 : 31, height = spill ? 1021 : 17, channels = 4;
      const gsize size = (gsize) width*height*channels;
      GimpImage *image = gimp_image_new (gimp,width,height,GIMP_RGB,GIMP_PRECISION_U8_NON_LINEAR);
      GimpColorProfile *profile = gimp_color_profile_new_rgb_adobe ();
      GimpLayer *source;
      GimpFilterLayer *filter;
      const Babl *native;
      GError *error = NULL;
      guchar *input = g_malloc (size), *expected = g_malloc (size), *actual = g_malloc (size);
      g_assert_true (gimp_image_set_color_profile (image,profile,&error));
      g_assert_no_error (error); g_object_unref (profile);
      native = gimp_image_get_layer_format (image,TRUE);
      source = gimp_layer_new (image,width,height,native,"Encoded Max RGB source",1,GIMP_LAYER_MODE_NORMAL_LEGACY);
      gimp_image_add_layer (image,source,NULL,0,FALSE); filter = filter_new (image,NULL,width,height);
      for (gsize i = 0; i < (gsize) width*height; ++i)
        {
          for (guint c = 0; c + 1 < channels; ++c) input[i*channels+c] = (i*37+c*61+17)&255;
          input[i*channels+channels-1] = (i % 3) ? 191 : 63;
        }
      gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (source)),NULL,0,native,input,GEGL_AUTO_ROWSTRIDE);
      max_rgb_define (filter,127); max_rgb_wait (filter,GIMP_FILTER_LAYER_CLEAN);
      max_rgb_owned_input (filter,native,input);
      max_rgb_reference (input,expected,width,height,channels,127);
      /* Read and write the actual image encoding. A default-space read here
       * could hide an accidental color conversion in the worker adapter. */
      gegl_buffer_get (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)),NULL,1,native,
                       actual,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
      g_assert_cmpmem (actual,size,expected,size);
      g_assert_true (babl_format_get_space (gimp_drawable_get_format (GIMP_DRAWABLE (filter))) ==
                     babl_format_get_space (native));
      g_assert_cmpint (gimp_drawable_get_precision (GIMP_DRAWABLE (filter)), ==, GIMP_PRECISION_U8_NON_LINEAR);
      g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 1);
      g_test_message ("Max RGB native Adobe RGB bytes: %s",spill ? "spill" : "vector");
      g_free (input); g_free (expected); g_free (actual); g_object_unref (image);
    }
}

static void
max_rgb_admission_preserves_cache (void)
{
  guint64 original_spill;
  g_object_get (gimp->config,"painter-filter-spill-size",&original_spill,NULL);
  {
    const guint side = 11000, channels = 4;
    GimpImage *image = gimp_image_new (gimp,side,side,GIMP_RGB,GIMP_PRECISION_U8_NON_LINEAR);
    GimpFilterLayer *filter = filter_new (image,NULL,side,side);
    GimpValueArray *args = max_rgb_args (50);
    const Babl *native = gimp_image_get_layer_format (image,TRUE);
    const guchar saved[] = {17,29,53,123};
    guchar actual[4];
    GeglBuffer *cache;
    guint64 generation;
    gchar *error;
    max_rgb_set_args (filter,args);
    cache = g_object_ref (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)));
    /* Sparse sentinel only. Admission must reject the request before reading
     * or allocating an image-sized owner snapshot or starting a worker. */
    gegl_buffer_set (cache,GEGL_RECTANGLE (0,0,1,1),0,native,saved,GEGL_AUTO_ROWSTRIDE);
    gimp_filter_layer_mark_as_loaded (filter); generation = gimp_filter_layer_get_cache_generation (filter);
    gimp_filter_layer_invalidate (filter); max_rgb_wait (filter,GIMP_FILTER_LAYER_FAILED);
    error = gimp_filter_layer_dup_error (filter);
    g_assert_nonnull (strstr (error,"admission memory limit")); g_free (error);
    g_assert_true (cache == gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)));
    g_assert_cmpuint (gimp_filter_layer_get_cache_generation (filter), ==, generation);
    g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 0);
    gegl_buffer_get (cache,GEGL_RECTANGLE (0,0,1,1),1,native,actual,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
    g_assert_cmpmem (actual,channels,saved,channels); max_rgb_saved_args (filter,args);
    g_object_unref (cache); gimp_value_array_unref (args); g_object_unref (image);
  }
  {
    const guint width = 1031, height = 1021, channels = 4;
    const guint64 reservation = (guint64) width*height*17;
    GimpImage *image = gimp_image_new (gimp,width,height,GIMP_RGB,GIMP_PRECISION_U8_NON_LINEAR);
    const Babl *native = gimp_image_get_layer_format (image,TRUE);
    GimpLayer *source = gimp_layer_new (image,width,height,native,"Admission source",1,GIMP_LAYER_MODE_NORMAL_LEGACY);
    GimpFilterLayer *filter;
    GimpValueArray *args = max_rgb_args (0);
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
    max_rgb_set_args (filter,args); gimp_filter_layer_mark_as_loaded (filter);
    cache = g_object_ref (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)));
    generation = gimp_filter_layer_get_cache_generation (filter);
    g_object_set (gimp->config,"painter-filter-spill-size",reservation-1,NULL);
    gimp_filter_layer_invalidate (filter); max_rgb_wait (filter,GIMP_FILTER_LAYER_FAILED);
    error = gimp_filter_layer_dup_error (filter); g_assert_nonnull (strstr (error,"spill")); g_free (error);
    g_assert_true (cache == gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)));
    g_assert_cmpuint (gimp_filter_layer_get_cache_generation (filter), ==, generation);
    g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 0); max_rgb_saved_args (filter,args);
    g_object_set (gimp->config,"painter-filter-spill-size",reservation,NULL);
    max_rgb_wait (filter,GIMP_FILTER_LAYER_CLEAN);
    max_rgb_reference (input,input,width,height,channels,0); max_rgb_compare (filter,input,channels);
    g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 1);
    g_test_message ("Max RGB logical spill admission: channels=%u rejected=%" G_GUINT64_FORMAT " accepted=%" G_GUINT64_FORMAT
                    " bytes; not an RSS or GEGL swap measurement",channels,reservation-1,reservation);
    g_object_set (gimp->config,"painter-filter-spill-size",original_spill,NULL);
    g_free (input); g_object_unref (cache); gimp_value_array_unref (args); g_object_unref (image);
  }
}

static void
max_rgb_partial_import_interrupt (GimpFilterLayer *filter, gpointer data)
{
  MaxRGBInterrupt *state = data;
  GimpFilterLayerProgress progress;
  if (state->called || gimp_filter_layer_get_state (filter) != GIMP_FILTER_LAYER_IMPORTING ||
      !gimp_filter_layer_get_progress (filter,&progress) || progress.value <= 0.85 || progress.value >= 1.0) return;
  /* A positive import fraction requires at least one actual staged pixel
   * chunk. The earlier first-state callback tests only the import gate. */
  g_test_message ("Max RGB interrupts private staged import: progress=%.9f action=%u",progress.value,state->action);
  max_rgb_interrupt (filter,data);
}

static void
max_rgb_old_cache_bytes (GeglBuffer *cache, guint width, guint height)
{
  guchar *actual = g_malloc ((gsize) width*height*4);
  gegl_buffer_get (cache,NULL,1,babl_format ("R'G'B'A u8"),actual,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
  for (gsize i = 0; i < (gsize) width*height; ++i)
    g_assert_cmpmem (actual+i*4,4,((const guchar[]) {17,29,53,255}),4);
  g_free (actual);
}

static void
max_rgb_partial_import_lifecycle (void)
{
  for (guint spill = 0; spill < 2; ++spill)
    for (guint action = 0; action < 4; ++action)
      {
        const guint width = spill ? 1031 : 1024, height = spill ? 2049 : 1024;
        GimpImage *image = image_new (width,height);
        GimpLayer *source = source_new (image,NULL,width,height);
        GimpFilterLayer *filter = filter_new (image,NULL,width,height);
        MaxRGBInterrupt interrupt = {GIMP_FILTER_LAYER_IMPORTING,action,source,FALSE};
        guchar *input = g_malloc ((gsize) width*height*4);
        GeglBuffer *cache;
        guint64 cache_generation;
        gint64 deadline;
        for (gsize i = 0; i < (gsize) width*height; ++i)
          {
            for (guint c = 0; c < 3; ++c) input[i*4+c] = (i*37+c*61+17)&255;
            input[i*4+3] = 255;
          }
        gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (source)),NULL,0,babl_format ("R'G'B'A u8"),input,GEGL_AUTO_ROWSTRIDE);
        /* Full selected coverage forces separate bounded final mask capture
         * and staged import. Observe real published progress, never a guessed
         * elapsed delay or a new production hook. */
        gimp_channel_select_rectangle (gimp_image_get_mask (image),0,0,width,height,
                                       GIMP_CHANNEL_OP_REPLACE,FALSE,0,0,FALSE);
        max_rgb_define (filter,255);
        fill (GIMP_LAYER (filter),17,29,53,255); gimp_filter_layer_mark_as_loaded (filter);
        cache = g_object_ref (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)));
        cache_generation = gimp_filter_layer_get_cache_generation (filter);
        g_signal_connect (filter,"filter-progress-changed",G_CALLBACK (max_rgb_partial_import_interrupt),&interrupt);
        gimp_filter_layer_invalidate (filter);
        deadline = g_get_monotonic_time () + 90 * G_TIME_SPAN_SECOND;
        while (!interrupt.called && g_get_monotonic_time () < deadline)
          {
            blinds_lifecycle_cached (filter,cache,cache_generation);
            g_assert_cmpint (gimp_filter_layer_get_state (filter), !=, GIMP_FILTER_LAYER_CLEAN);
            g_main_context_iteration (NULL,FALSE); g_usleep (100);
          }
        g_assert_true (interrupt.called);
        if (action == 1 || action == 2)
          {
            const guint64 generation = gimp_filter_layer_get_generation (filter);
            blinds_lifecycle_settle (filter,cache,cache_generation,generation);
            if (action == 1)
              { max_rgb_reference (input,input,width,height,4,0); max_rgb_compare (filter,input,4); }
            else blinds_lifecycle_constant (filter,(const guchar[]) {0,0,193,255});
            g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 2);
          }
        else
          {
            g_assert_cmpint (gimp_filter_layer_get_state (filter), ==,
                             action == 0 ? GIMP_FILTER_LAYER_FAILED : GIMP_FILTER_LAYER_CLOSED);
            max_rgb_drain_workers (); blinds_lifecycle_cached (filter,cache,cache_generation);
          }
        max_rgb_old_cache_bytes (cache,width,height);
        g_signal_handlers_disconnect_by_data (filter,&interrupt);
        g_free (input); g_object_unref (cache); g_object_unref (image);
      }
}

/* The double route deliberately retains the pre-existing ignored context-slot
 * compatibility and ignores byte-route owner masks. This includes U8 linear
 * and perceptual: real_samples() means every encoding except U8 nonlinear. */
static void
max_rgb_real_case (GimpComponentType type, GimpTRCType trc, gboolean spill)
{
  const GimpPrecision precision = gimp_babl_precision (type,trc);
  const guint width = spill ? 513 : 7, height = spill ? 257 : 1, channels = 4;
  const gsize count = (gsize) width*height*channels;
  GimpImage *image = gimp_image_new (gimp,width,height,GIMP_RGB,precision);
  const Babl *format = gimp_babl_format (GIMP_RGB,
    gimp_babl_precision (GIMP_COMPONENT_TYPE_DOUBLE,trc),TRUE,
    babl_format_get_space (gimp_image_get_layer_format (image,TRUE)));
  GimpLayer *source = gimp_layer_new (image,width,height,gimp_image_get_layer_format (image,TRUE),
                                    "Real Max RGB input",1,GIMP_LAYER_MODE_NORMAL_LEGACY);
  GimpFilterLayer *filter;
  GimpValueArray *args = max_rgb_args (127);
  gdouble *input = g_new (gdouble,count), *owned = g_new (gdouble,count), *actual = g_new (gdouble,count);
  GeglBuffer *rounded;
  gimp_image_add_layer (image,source,NULL,0,FALSE); filter = filter_new (image,NULL,width,height);
  for (gsize i = 0; i < (gsize) width*height; ++i)
    {
      for (guint c = 0; c + 1 < channels; ++c) input[i*channels+c] = .125 + c*.25;
      input[i*channels+channels-1] = i % 2 ? .75 : .25;
    }
  gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (source)),NULL,0,format,input,GEGL_AUTO_ROWSTRIDE);
  /* Wrong-typed ignored slots remain valid only for this pre-existing route. */
  for (guint slot = 0; slot < 3; ++slot)
    {
      GValue *value = gimp_value_array_index (args,slot);
      g_value_unset (value); g_value_init (value,G_TYPE_DOUBLE); g_value_set_double (value,19.25+slot);
    }
  gimp_channel_select_rectangle (gimp_image_get_mask (image),0,0,1,1,GIMP_CHANNEL_OP_REPLACE,FALSE,0,0,FALSE);
  gimp_image_set_component_active (image,GIMP_CHANNEL_RED,FALSE);
  gimp_image_set_component_active (image,GIMP_CHANNEL_GREEN,FALSE);
  gimp_image_set_component_active (image,GIMP_CHANNEL_BLUE,FALSE);
  gimp_image_set_component_active (image,GIMP_CHANNEL_ALPHA,FALSE);
  gimp_layer_set_lock_alpha (GIMP_LAYER (filter),TRUE,FALSE);
  max_rgb_set_args (filter,args); max_rgb_wait (filter,GIMP_FILTER_LAYER_CLEAN);
  max_rgb_saved_args (filter,args);
  max_rgb_owned_input (filter,format,owned);
  for (gsize i = 0; i < (gsize) width*height; ++i)
    if (owned[i*4+3])
      {
        const gdouble value = MAX (owned[i*4],MAX (owned[i*4+1],owned[i*4+2]));
        for (guint c = 0; c < 3; ++c) if (owned[i*4+c] != value) owned[i*4+c] = 0;
      }
  /* Compare after native quantization, retaining all fractional color samples. */
  rounded = gegl_buffer_new (GEGL_RECTANGLE (0,0,width,height),gimp_drawable_get_format (GIMP_DRAWABLE (filter)));
  gegl_buffer_set (rounded,NULL,0,format,owned,GEGL_AUTO_ROWSTRIDE);
  gegl_buffer_get (rounded,NULL,1,format,owned,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
  gegl_buffer_get (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)),NULL,1,format,actual,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
  for (gsize i = 0; i < count; ++i) g_assert_cmpfloat (actual[i], ==, owned[i]);
  g_assert_cmpfloat (actual[2], >, 0); g_assert_cmpfloat (actual[0], ==, 0);
  g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 1);
  g_test_message ("Max RGB existing real route component=%d trc=%d spill=%d",type,trc,spill);
  g_free (input); g_free (owned); g_free (actual); g_object_unref (rounded);
  gimp_value_array_unref (args); g_object_unref (image);
}

static void
max_rgb_real_routes_unchanged (void)
{
  const GimpComponentType types[] = {GIMP_COMPONENT_TYPE_U8,GIMP_COMPONENT_TYPE_U16,GIMP_COMPONENT_TYPE_U32,
    GIMP_COMPONENT_TYPE_HALF,GIMP_COMPONENT_TYPE_FLOAT,GIMP_COMPONENT_TYPE_DOUBLE};
  const GimpTRCType trcs[] = {GIMP_TRC_LINEAR,GIMP_TRC_NON_LINEAR,GIMP_TRC_PERCEPTUAL};
  for (guint t = 0; t < G_N_ELEMENTS (types); ++t)
    for (guint r = 0; r < G_N_ELEMENTS (trcs); ++r)
      if (gimp_babl_precision (types[t],trcs[r]) != GIMP_PRECISION_U8_NON_LINEAR)
        max_rgb_real_case (types[t],trcs[r],FALSE);
  max_rgb_real_case (GIMP_COMPONENT_TYPE_FLOAT,GIMP_TRC_LINEAR,TRUE);
}

/* Synthetic projection-copy bounds, not genuine runtime observations.
 * A parent group may extend outside the image canvas; the
 * old operation used that group's projection rectangle instead of the image.
 * Off-projection input retains old drawable bytes, including a loaded cache. */
static void
max_rgb_projection_input_bounds (void)
{
  const gint modes[] = {1,0,-1,G_MAXINT};
  for (guint grouped = 0; grouped < 2; ++grouped)
    for (guint positive = 0; positive < 2; ++positive)
      for (guint seeded = 0; seeded < 2; ++seeded)
        {
          const guint width = 11, height = 9, channels = 4;
          const gsize size = (gsize) width*height*channels;
          gint ox = positive ? 13 : -3, oy = positive ? 10 : -2;
          GimpImage *image = gimp_image_new (gimp,17,13,GIMP_RGB,GIMP_PRECISION_U8_NON_LINEAR);
          const Babl *format = gimp_image_get_layer_format (image,TRUE);
          GimpLayer *parent = grouped ? gimp_group_layer_new (image) : NULL;
          GimpLayer *source = gimp_layer_new (image,width,height,format,"Projection bounds source",1,GIMP_LAYER_MODE_NORMAL_LEGACY);
          GimpFilterLayer *filter;
          guchar input[11*9*4], expected[11*9*4], previous[11*9*4] = {0};
          if (parent) gimp_image_add_layer (image,parent,NULL,0,FALSE);
          gimp_image_add_layer (image,source,parent,0,FALSE); filter = filter_new (image,parent,width,height);
          gimp_item_set_offset (GIMP_ITEM (source),ox,oy); gimp_item_set_offset (GIMP_ITEM (filter),ox,oy);
          for (guint i = 0; i < width*height; ++i)
            {
              for (guint c = 0; c + 1 < channels; ++c)
                {
                  input[i*channels+c] = 31+c*61;
                  previous[i*channels+c] = seeded ? 41+c*23 : 0;
                }
              input[i*channels+channels-1] = 255;
              previous[i*channels+channels-1] = seeded ? 91 : 0;
            }
          gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (source)),NULL,0,format,input,GEGL_AUTO_ROWSTRIDE);
          for (guint pass = 0; pass < G_N_ELEMENTS (modes); ++pass)
            {
              if (pass == 3)
                {
                  /* Moving the projection intersection must retain the
                   * previous completed local pixels in newly uncovered areas. */
                  ox = positive ? -3 : 13; oy = positive ? -2 : 10;
                  gimp_item_set_offset (GIMP_ITEM (filter),ox,oy);
                }
              max_rgb_define (filter,modes[pass]);
              if (!pass && seeded)
                {
                  gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)),NULL,0,format,previous,GEGL_AUTO_ROWSTRIDE);
                  gimp_filter_layer_mark_as_loaded (filter); gimp_filter_layer_invalidate (filter);
                }
              max_rgb_wait (filter,GIMP_FILTER_LAYER_CLEAN);
              max_rgb_owned_reference (filter,previous,expected);
              max_rgb_reference (expected,expected,width,height,channels,modes[pass]);
              max_rgb_compare (filter,expected,channels);
              memcpy (previous,expected,size);
              g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, pass+1);
            }
          g_test_message ("Max RGB projection input: grouped=%u initial_positive=%u seeded=%u",
                          grouped,positive,seeded);
          g_object_unref (image);
        }
}
