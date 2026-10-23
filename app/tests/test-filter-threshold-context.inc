/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Actual in-process FilterLayer execution. Analytic/source-backed cases below
 * test routing and lifecycle; threshold_actual_old_live and
 * threshold_actual_old_projection_history use captured old application bytes.
 * Neither the analytic checks nor the shared merge helper
 * substitutes for that separate genuine-runtime acceptance gate. */
static GimpValueArray *
threshold_args (gint threshold)
{
  return gimp_value_array_new_from_types (NULL,
    G_TYPE_INT,1,G_TYPE_INT,-123,G_TYPE_INT,456,G_TYPE_INT,threshold,G_TYPE_NONE);
}

static void
threshold_set_args (GimpFilterLayer *filter, GimpValueArray *args)
{
  static const guchar saved[] = {0,0xff,2,0x80,0,3,0};
  GBytes *raw = g_bytes_new_static (saved,sizeof saved);
  GError *error = NULL;
  g_assert_true (gimp_filter_layer_set_definition (filter,"plug-in-threshold-alpha",raw,args,&error));
  g_assert_no_error (error); g_bytes_unref (raw);
}

static void
threshold_define (GimpFilterLayer *filter, gint threshold)
{
  GimpValueArray *args = threshold_args (threshold);
  threshold_set_args (filter,args); gimp_value_array_unref (args);
}

static void
threshold_saved_args (GimpFilterLayer *filter, GimpValueArray *expected)
{
  static const guchar saved[] = {0,0xff,2,0x80,0,3,0};
  GBytes *raw = gimp_filter_layer_ref_definition (filter);
  GimpValueArray *actual = gimp_filter_layer_dup_args (filter);
  gchar *name = gimp_filter_layer_dup_procedure (filter);
  g_assert_cmpstr (name, ==, "plug-in-threshold-alpha");
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
threshold_wait (GimpFilterLayer *filter, GimpFilterLayerState expected)
{
  const gint64 deadline = g_get_monotonic_time () + 90 * G_TIME_SPAN_SECOND;
  while (gimp_filter_layer_get_state (filter) != expected && g_get_monotonic_time () < deadline)
    {
      if (gimp_filter_layer_get_state (filter) == GIMP_FILTER_LAYER_FAILED && expected != GIMP_FILTER_LAYER_FAILED)
        { gchar *error = gimp_filter_layer_dup_error (filter); g_error ("Threshold failed: %s",error); }
      g_main_context_iteration (NULL,FALSE); g_usleep (100);
    }
  g_assert_cmpint (gimp_filter_layer_get_state (filter), ==, expected);
}

static void
threshold_compare (GimpFilterLayer *filter, const guchar *expected, guint channels)
{
  const guint width = gimp_item_get_width (GIMP_ITEM (filter));
  const guint height = gimp_item_get_height (GIMP_ITEM (filter));
  const Babl *format = babl_format (channels == 2 ? "Y'A u8" : "R'G'B'A u8");
  guchar *actual = g_malloc ((gsize) width*height*channels);
  gegl_buffer_get (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)),NULL,1,format,
                   actual,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
  for (gsize i = 0; i < (gsize) width*height*channels; ++i)
    if (actual[i] != expected[i])
      {
        g_test_message ("Threshold first mismatch local=%u,%u channel=%u actual=%u expected=%u owner_offset=%d,%d",
                        (guint) (i/channels % width),(guint) (i/channels / width),(guint) (i % channels),
                        actual[i],expected[i],gimp_item_get_offset_x (GIMP_ITEM (filter)),
                        gimp_item_get_offset_y (GIMP_ITEM (filter)));
        break;
      }
  g_assert_cmpmem (actual,(gsize) width*height*channels,expected,(gsize) width*height*channels);
  g_free (actual);
}

/* This scalar oracle is not old-runtime evidence. Capture comparisons below
 * independently check the actual legacy application output. */
static void
threshold_reference (const guchar *input, guchar *output,
                     guint width, guint height, guint channels, gint threshold)
{
  memcpy (output,input,(gsize) width*height*channels);
  for (gsize i = 0; i < (gsize) width*height; ++i)
    output[i*channels+channels-1] = threshold < input[i*channels+channels-1] ? 255 : 0;
}

static void
threshold_owned_input (GimpFilterLayer *filter, const Babl *format, gpointer input)
{
  GeglNode *below = gegl_node_get_input_proxy (gimp_filter_get_node (GIMP_FILTER (filter)),"input");
  const GeglRectangle rect = {gimp_item_get_offset_x (GIMP_ITEM (filter)),
    gimp_item_get_offset_y (GIMP_ITEM (filter)),gimp_item_get_width (GIMP_ITEM (filter)),
    gimp_item_get_height (GIMP_ITEM (filter))};
  gegl_node_blit (below,1,&rect,format,input,GEGL_AUTO_ROWSTRIDE,GEGL_BLIT_CACHE);
}

static void
threshold_integer_native_route (void)
{
  const gint thresholds[] = {G_MININT,-1,0,1,127,254,255,256,G_MAXINT};
  const guint widths[] = {1,1,7,2,17,67}, heights[] = {1,7,1,2,13,66};
  const guchar alphas[] = {0,1,126,127,128,254,255};
  for (guint gray = 0; gray < 2; ++gray)
    for (guint shape = 0; shape < G_N_ELEMENTS (widths); ++shape)
      {
        const guint width = widths[shape], height = heights[shape], channels = gray ? 2 : 4;
        const gsize size = (gsize) width*height*channels;
        const Babl *format = babl_format (gray ? "Y'A u8" : "R'G'B'A u8");
        GimpImage *image = gimp_image_new (gimp,width,height,gray ? GIMP_GRAY : GIMP_RGB,GIMP_PRECISION_U8_NON_LINEAR);
        GimpLayer *source = gimp_layer_new (image,width,height,format,"Threshold source",1,GIMP_LAYER_MODE_NORMAL_LEGACY);
        GimpFilterLayer *filter;
        guchar *input = g_malloc (size), *owned = g_malloc (size), *expected = g_malloc (size);
        gimp_image_add_layer (image,source,NULL,0,FALSE); filter = filter_new (image,NULL,width,height);
        for (guint i = 0; i < width*height; ++i)
          {
            for (guint c = 0; c + 1 < channels; ++c) input[i*channels+c] = (i*37+c*61+17)&255;
            input[i*channels+channels-1] = alphas[i % G_N_ELEMENTS (alphas)];
          }
        gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (source)),NULL,0,format,input,GEGL_AUTO_ROWSTRIDE);
        for (guint t = 0; t < G_N_ELEMENTS (thresholds); ++t)
          {
            GimpValueArray *args = threshold_args (thresholds[t]);
            threshold_set_args (filter,args); threshold_wait (filter,GIMP_FILTER_LAYER_CLEAN);
            /* The owned lower proxy is the input contract, including hidden
             * color at zero alpha; source bytes alone are not that contract. */
            threshold_owned_input (filter,format,owned);
            threshold_reference (owned,expected,width,height,channels,thresholds[t]);
            threshold_compare (filter,expected,channels); threshold_saved_args (filter,args);
            g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, t+1);
            gimp_value_array_unref (args);
          }
        g_free (input); g_free (owned); g_free (expected); g_object_unref (image);
      }
}

static void
threshold_invalid_definitions_keep_cache (void)
{
  GimpImage *image = image_new (7,5);
  GimpLayer *source = source_new (image,NULL,7,5);
  GimpFilterLayer *filter = filter_new (image,NULL,7,5);
  GeglBuffer *cache;
  guint64 generation;
  fill (source,31,121,217,255); threshold_define (filter,0);
  threshold_wait (filter,GIMP_FILTER_LAYER_CLEAN);
  cache = g_object_ref (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)));
  generation = gimp_filter_layer_get_cache_generation (filter);
  for (guint kind = 0; kind < 6; ++kind)
    {
      GimpValueArray *args = threshold_args (10);
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
      threshold_set_args (filter,args); threshold_wait (filter,GIMP_FILTER_LAYER_FAILED);
      g_assert_true (cache == gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)));
      g_assert_cmpuint (gimp_filter_layer_get_cache_generation (filter), ==, generation);
      g_assert_cmpuint (gimp_filter_layer_get_generation (filter), >, generation);
      g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 1);
      threshold_saved_args (filter,args); pixel (GIMP_LAYER (filter),31,121,217,255);
      gimp_value_array_unref (args);
    }
  threshold_define (filter,10); threshold_wait (filter,GIMP_FILTER_LAYER_CLEAN);
  g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 2);
  g_object_unref (cache); g_object_unref (image);
}

static void
threshold_tiny_selection_routes (void)
{
  const guint widths[] = {1,1,7,2}, heights[] = {1,7,1,2};
  for (guint gray = 0; gray < 2; ++gray)
    for (guint crop = 0; crop < G_N_ELEMENTS (widths); ++crop)
      {
        const guint width = 11, height = 11, channels = gray ? 2 : 4;
        const gint ox = crop & 1 ? -2 : 3, oy = crop & 1 ? 3 : -2;
        GimpImage *image = gimp_image_new (gimp,17,17,gray ? GIMP_GRAY : GIMP_RGB,GIMP_PRECISION_U8_NON_LINEAR);
        const Babl *format = gimp_image_get_layer_format (image,TRUE);
        GimpLayer *source = gimp_layer_new (image,width,height,format,"Tiny Threshold crop",1,GIMP_LAYER_MODE_NORMAL_LEGACY);
        GimpFilterLayer *filter;
        guchar input[11*11*4], expected[11*11*4];
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
        threshold_define (filter,255); threshold_wait (filter,GIMP_FILTER_LAYER_CLEAN);
        threshold_owned_input (filter,format,input);
        for (guint y = 0; y < height; ++y)
          for (guint x = 0; x < width; ++x)
            if ((gint) x+ox < 0 || (gint) y+oy < 0 || (gint) x+ox >= 17 || (gint) y+oy >= 17)
              memset (input+(y*width+x)*channels,0,channels);
        memcpy (expected,input,(gsize) width*height*channels);
        for (guint y = 3; y < 3+heights[crop]; ++y)
          for (guint x = 3; x < 3+widths[crop]; ++x) expected[(y*width+x)*channels+channels-1] = 0;
        threshold_compare (filter,expected,channels);
        /* Sharing the owner carrier must not weaken Sharpen's 2x3 minimum. */
        sharpen_define (filter,10); threshold_wait (filter,GIMP_FILTER_LAYER_FAILED);
        threshold_compare (filter,expected,channels);
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
} ThresholdContextChange;

static void
threshold_late_context (GimpFilterLayer *filter, gpointer data)
{
  ThresholdContextChange *state = data;
  guchar *mask;
  if (state->called || gimp_filter_layer_get_state (filter) != GIMP_FILTER_LAYER_IMPORTING) return;
  state->called = TRUE;
  mask = g_malloc ((gsize) state->width*state->height);
  memset (mask,state->kind == 2 ? 128 : 255,(gsize) state->width*state->height);
  blinds_fixture_selection (state->image,state->width,state->height,state->ox,state->oy,mask,state->kind == 1);
  g_free (mask);
  if (state->kind == 3) gimp_image_set_component_active (state->image,GIMP_CHANNEL_ALPHA,FALSE);
  if (state->kind == 4) gimp_layer_set_lock_alpha (GIMP_LAYER (filter),TRUE,FALSE);
}

static void
threshold_late_context_routes (void)
{
  for (guint spill = 0; spill < 2; ++spill)
    for (guint kind = 0; kind < 6; ++kind)
      {
        const guint width = spill ? 1031 : 63, height = spill ? 1021 : 61;
        const gint ox = 3, oy = 2;
        const guint x1 = width/3, x2 = 2*width/3, y1 = 3, y2 = height-3;
        GimpImage *image = image_new (width+ox+2,height+oy+2);
        GimpLayer *parent = gimp_group_layer_new (image);
        GimpLayer *source;
        GimpFilterLayer *filter;
        ThresholdContextChange change = {image,width,height,kind,ox,oy,FALSE};
        GeglBuffer *cache;
        guint64 generation, cache_generation;
        guchar *expected = g_malloc ((gsize) width*height*4);
        gimp_image_add_layer (image,parent,NULL,0,FALSE);
        source = source_new (image,parent,width,height); filter = filter_new (image,parent,width,height);
        gimp_item_set_offset (GIMP_ITEM (source),ox,oy); gimp_item_set_offset (GIMP_ITEM (filter),ox,oy);
        fill (source,31,121,217,255);
        gimp_channel_select_rectangle (gimp_image_get_mask (image),ox+x1,oy+y1,x2-x1,y2-y1,
                                       GIMP_CHANNEL_OP_REPLACE,FALSE,0,0,FALSE);
        if (kind == 5) parent->lock_alpha = TRUE; /* Own lock alone controls the merge. */
        threshold_define (filter,0);
        fill (GIMP_LAYER (filter),17,29,53,255); gimp_filter_layer_mark_as_loaded (filter);
        cache = g_object_ref (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)));
        cache_generation = gimp_filter_layer_get_cache_generation (filter);
        g_signal_connect (filter,"filter-state-changed",G_CALLBACK (threshold_late_context),&change);
        gimp_filter_layer_invalidate (filter); generation = gimp_filter_layer_get_generation (filter);
        blinds_lifecycle_settle (filter,cache,cache_generation,generation);
        g_assert_true (change.called);
        for (guint y = 0; y < height; ++y)
          for (guint x = 0; x < width; ++x)
            {
              const gsize i = ((gsize) y*width+x)*4;
              const gboolean inside = x >= x1 && x < x2 && y >= y1 && y < y2;
              expected[i] = 31; expected[i+1] = 121; expected[i+2] = 217;
              /* Zero-alpha shadow preserves original hidden colors. The
               * opaque-to-zero alpha interpolation is exactly 255-coverage. */
              expected[i+3] = inside || kind == 3 || kind == 4 ? 255 : kind == 2 ? 127 : 0;
            }
        threshold_compare (filter,expected,4);
        g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 1);
        spin_ms (15);
        g_assert_cmpuint (gimp_filter_layer_get_generation (filter), ==, generation);
        g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 1);
        fill (source,79,137,193,255); threshold_wait (filter,GIMP_FILTER_LAYER_CLEAN);
        blinds_lifecycle_constant (filter,(const guchar[]) {79,137,193,255});
        g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 2);
        g_signal_handlers_disconnect_by_data (filter,&change);
        g_free (expected); g_object_unref (cache); g_object_unref (image);
      }
}

static void
threshold_freeze_context (GimpFilterLayer *filter, gpointer data)
{
  ThresholdContextChange *state = data;
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
threshold_sealed_context_routes (void)
{
  for (guint spill = 0; spill < 2; ++spill)
    {
      const guint width = spill ? 1031 : 513, height = spill ? 1021 : 257;
      GimpImage *image = image_new (width,height);
      GimpLayer *source = source_new (image,NULL,width,height);
      GimpFilterLayer *filter = filter_new (image,NULL,width,height);
      ThresholdContextChange change = {image,width,height,0,0,0,FALSE};
      guchar *input = g_malloc0 ((gsize) width*height*4), *expected = g_malloc ((gsize) width*height*4);
      const gsize center = ((gsize) (height/2)*width+width/2)*4;
      for (gsize i = 0; i < (gsize) width*height; ++i) input[i*4+3] = 100;
      input[center] = 30; input[center+1] = 60; input[center+2] = 100;
      gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (source)),NULL,0,babl_format ("R'G'B'A u8"),input,GEGL_AUTO_ROWSTRIDE);
      g_assert_true (gimp_image_get_mask (image)->bounds_known); g_assert_true (gimp_image_get_mask (image)->empty);
      g_signal_connect (filter,"filter-state-changed",G_CALLBACK (threshold_freeze_context),&change);
      threshold_define (filter,127); threshold_wait (filter,GIMP_FILTER_LAYER_CLEAN);
      g_assert_true (change.called);
      threshold_owned_input (filter,babl_format ("R'G'B'A u8"),input);
      threshold_reference (input,expected,width,height,4,127); threshold_compare (filter,expected,4);
      g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 1);
      input[center] = 71;
      gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (source)),NULL,0,babl_format ("R'G'B'A u8"),input,GEGL_AUTO_ROWSTRIDE);
      gimp_drawable_update (GIMP_DRAWABLE (source),0,0,width,height); threshold_wait (filter,GIMP_FILTER_LAYER_CLEAN);
      threshold_compare (filter,input,4);
      g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 2);
      g_signal_handlers_disconnect_by_data (filter,&change);
      g_free (input); g_free (expected); g_object_unref (image);
    }
}

static void
threshold_outside_selection_no_merge (void)
{
  for (guint spill = 0; spill < 2; ++spill)
    {
      const guint width = spill ? 1031 : 63, height = spill ? 1021 : 61;
      GimpImage *image = image_new (width+4,height+4);
      GimpLayer *source = source_new (image,NULL,width,height);
      GimpFilterLayer *filter = filter_new (image,NULL,width,height);
      ThresholdContextChange change = {image,width,height,1,0,0,FALSE};
      guchar *input = g_malloc0 ((gsize) width*height*4), *expected = g_malloc ((gsize) width*height*4);
      const gsize center = ((gsize) (height/2)*width+width/2)*4;
      for (gsize i = 0; i < (gsize) width*height; ++i) input[i*4+3] = 100;
      input[center] = 30; input[center+1] = 60; input[center+2] = 100;
      gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (source)),NULL,0,babl_format ("R'G'B'A u8"),input,GEGL_AUTO_ROWSTRIDE);
      gimp_channel_select_rectangle (gimp_image_get_mask (image),width+1,height+1,2,2,
                                     GIMP_CHANNEL_OP_REPLACE,FALSE,0,0,FALSE);
      g_signal_connect (filter,"filter-state-changed",G_CALLBACK (threshold_late_context),&change);
      threshold_define (filter,127); threshold_wait (filter,GIMP_FILTER_LAYER_CLEAN);
      g_assert_true (change.called);
      threshold_owned_input (filter,babl_format ("R'G'B'A u8"),input);
      threshold_compare (filter,input,4);
      g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 1);
      gimp_drawable_update (GIMP_DRAWABLE (source),0,0,width,height); threshold_wait (filter,GIMP_FILTER_LAYER_CLEAN);
      threshold_reference (input,expected,width,height,4,127); threshold_compare (filter,expected,4);
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
} ThresholdInterrupt;

static void
threshold_interrupt (GimpFilterLayer *filter, gpointer data)
{
  ThresholdInterrupt *state = data;
  if (state->called || gimp_filter_layer_get_state (filter) != state->phase) return;
  state->called = TRUE;
  if (state->action == 0) gimp_filter_layer_cancel (filter);
  else if (state->action == 1) threshold_define (filter,0);
  else if (state->action == 2) fill (state->source,79,137,193,255);
  else g_assert_true (gimp_painter_binding_close (G_OBJECT (filter),NULL));
}

static void
threshold_drain_workers (void)
{
  const gint64 deadline = g_get_monotonic_time () + 90 * G_TIME_SPAN_SECOND;
  while (gimp_test_filter_pending_jobs () && g_get_monotonic_time () < deadline)
    { g_main_context_iteration (NULL,FALSE); g_usleep (100); }
  g_assert_cmpuint (gimp_test_filter_pending_jobs (), ==, 0);
}

static void
threshold_cancel_replace_lower_close (void)
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
          ThresholdInterrupt interrupt = {phases[phase],action,NULL,FALSE};
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
          threshold_define (filter,255);
          fill (GIMP_LAYER (filter),17,29,53,255); gimp_filter_layer_mark_as_loaded (filter);
          cache = g_object_ref (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)));
          cache_generation = gimp_filter_layer_get_cache_generation (filter);
          g_signal_connect (filter,"filter-state-changed",G_CALLBACK (threshold_interrupt),&interrupt);
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
              if (action == 1) threshold_compare (filter,input,4);
              else blinds_lifecycle_constant (filter,(const guchar[]) {79,137,193,0});
              g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==,
                               phases[phase] == GIMP_FILTER_LAYER_RUNNING || phases[phase] == GIMP_FILTER_LAYER_IMPORTING ? 2 : 1);
            }
          else
            {
              threshold_wait (filter,action == 0 ? GIMP_FILTER_LAYER_FAILED : GIMP_FILTER_LAYER_CLOSED);
              g_assert_true (interrupt.called); threshold_drain_workers ();
              blinds_lifecycle_cached (filter,cache,cache_generation);
              if (action == 0)
                {
                  GimpValueArray *args = threshold_args (255);
                  threshold_saved_args (filter,args); gimp_value_array_unref (args);
                  fill (source,79,137,193,255); threshold_wait (filter,GIMP_FILTER_LAYER_CLEAN);
                  blinds_lifecycle_constant (filter,(const guchar[]) {79,137,193,0});
                }
            }
          g_signal_handlers_disconnect_by_data (filter,&interrupt);
          g_free (input); g_object_unref (cache); g_object_unref (image);
        }
}

/* Genuine old application capture contract is emitted by the dedicated
 * Threshold harness. The fixture runner verifies its source/provenance manifest
 * before setting this directory. These comparisons must never be silently
 * replaced by the analytic reference above. */
typedef struct
{
  GimpImage *image;
  const gchar *directory, *id;
  guint image_width, image_height;
  gint selection, components, lock;
  gboolean called;
} ThresholdOldContext;

static guchar *
threshold_fixture (const gchar *directory, const gchar *id, const gchar *label, gsize size)
{ return blinds_fixture_bytes (directory,id,label,size); }

static void
threshold_components (GimpImage *image, GimpFilterLayer *filter, guint channels, gint active, gint lock)
{
  if (active >= 0)
    {
      if (channels == 2) gimp_image_set_component_active (image,GIMP_CHANNEL_GRAY,active & 1);
      else
        {
          gimp_image_set_component_active (image,GIMP_CHANNEL_RED,active & 1);
          gimp_image_set_component_active (image,GIMP_CHANNEL_GREEN,active & 2);
          gimp_image_set_component_active (image,GIMP_CHANNEL_BLUE,active & 4);
        }
      gimp_image_set_component_active (image,GIMP_CHANNEL_ALPHA,active & (1U << (channels-1)));
    }
  if (lock >= 0) gimp_layer_set_lock_alpha (GIMP_LAYER (filter),lock,FALSE);
}

static void
threshold_old_late_context (GimpFilterLayer *filter, gpointer data)
{
  ThresholdOldContext *state = data;
  const guint channels = gimp_drawable_get_base_type (GIMP_DRAWABLE (filter)) == GIMP_GRAY ? 2 : 4;
  if (state->called || gimp_filter_layer_get_state (filter) != GIMP_FILTER_LAYER_IMPORTING) return;
  state->called = TRUE;
  if (state->selection >= 0)
    {
      guchar *mask = threshold_fixture (state->directory,state->id,"final-mask",(gsize) state->image_width*state->image_height);
      blinds_fixture_selection (state->image,state->image_width,state->image_height,0,0,mask,state->selection == 0);
      g_free (mask);
    }
  threshold_components (state->image,filter,channels,state->components,state->lock);
}

static void
threshold_actual_old_live (void)
{
  const gchar *directory = g_getenv ("GIMP_PAINTER_THRESHOLD_ALPHA_FIXTURES");
  gchar *path, *text = NULL, **lines;
  guint comparisons = 0, scenes = 0, changed = 0, unchanged = 0, pre_merge = 0, mid_work = 0, layouts = 0;
  g_assert_nonnull (directory);
  path = g_build_filename (directory,"captures.tsv",NULL);
  g_assert_true (g_file_get_contents (path,&text,NULL,NULL));
  lines = g_strsplit (text,"\n",-1);
  for (guint row = 0; lines[row]; ++row)
    {
      gchar **cols;
      guint width,height,image_width,image_height,source_channels,channels,passes;
      gint ox,oy,threshold;
      GimpImage *image;
      GimpLayer *source;
      GimpFilterLayer *filter;
      const Babl *source_format;
      ThresholdOldContext change;
      guchar *bytes;
      guint64 generation;
      if (!*lines[row]) continue;
      cols = g_strsplit (lines[row],"\t",-1);
      g_assert_cmpuint (g_strv_length (cols), ==, 30);
      if (!atoi (cols[1])) { g_strfreev (cols); continue; }
      source_channels = atoi (cols[2]); channels = atoi (cols[3]);
      g_assert_cmpuint (source_channels, >=, 1); g_assert_cmpuint (source_channels, <=, 4);
      layouts |= 1U << (source_channels-1);
      changed += atoi (cols[17]) == 1; unchanged += atoi (cols[17]) == 2;
      pre_merge += atoi (cols[24]) == 1; mid_work += atoi (cols[24]) == 2;
      width = atoi (cols[4]); height = atoi (cols[5]);
      image_width = atoi (cols[6]); image_height = atoi (cols[7]);
      ox = atoi (cols[8]); oy = atoi (cols[9]); threshold = atoi (cols[10]);
      passes = atoi (cols[17]) ? 2 : 1;
      g_assert_true (channels == 2 || channels == 4);
      source_format = babl_format (source_channels == 1 ? "Y' u8" : source_channels == 2 ? "Y'A u8" :
                                  source_channels == 3 ? "R'G'B' u8" : "R'G'B'A u8");
      image = gimp_image_new (gimp,image_width,image_height,channels == 2 ? GIMP_GRAY : GIMP_RGB,GIMP_PRECISION_U8_NON_LINEAR);
      source = gimp_layer_new (image,width,height,source_format,"Old Threshold source",1,GIMP_LAYER_MODE_NORMAL_LEGACY);
      gimp_image_add_layer (image,source,NULL,0,FALSE); filter = filter_new (image,NULL,width,height);
      gimp_item_set_offset (GIMP_ITEM (source),ox,oy); gimp_item_set_offset (GIMP_ITEM (filter),ox,oy);
      bytes = threshold_fixture (directory,cols[0],"source",(gsize) width*height*source_channels);
      gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (source)),NULL,0,source_format,bytes,GEGL_AUTO_ROWSTRIDE);
      g_free (bytes);
      bytes = threshold_fixture (directory,cols[0],"start-mask",(gsize) image_width*image_height);
      blinds_fixture_selection (image,image_width,image_height,0,0,bytes,atoi (cols[11]) == 0); g_free (bytes);
      threshold_components (image,filter,channels,atoi (cols[13]),atoi (cols[15]));
      change = (ThresholdOldContext) {image,directory,cols[0],image_width,image_height,
                                   atoi (cols[12]),atoi (cols[14]),atoi (cols[16]),FALSE};
      if (change.selection >= 0 || change.components >= 0 || change.lock >= 0)
        {
          g_assert_cmpint (atoi (cols[11]), >, 0); /* Nonempty start keeps the late gate unsealed. */
          g_assert_cmpint (atoi (cols[24]), >, 0);
          g_assert_cmpint (atoi (cols[23]), ==, 1);
          g_signal_connect (filter,"filter-state-changed",G_CALLBACK (threshold_old_late_context),&change);
        }
      threshold_define (filter,threshold); generation = gimp_filter_layer_get_generation (filter);
      for (guint pass = 0; pass < passes; ++pass)
        {
          if (pass)
            {
              bytes = threshold_fixture (directory,cols[0],"source2",(gsize) width*height*source_channels);
              if (atoi (cols[17]) == 2)
                {
                  guchar *original = threshold_fixture (directory,cols[0],"source",(gsize) width*height*source_channels);
                  g_assert_cmpmem (bytes,(gsize) width*height*source_channels,original,(gsize) width*height*source_channels);
                  g_free (original); /* An unchanged-byte notification is not a rewrite. */
                }
              else
                gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (source)),NULL,0,source_format,bytes,GEGL_AUTO_ROWSTRIDE);
              g_free (bytes); gimp_drawable_update (GIMP_DRAWABLE (source),0,0,width,height);
            }
          threshold_wait (filter,GIMP_FILTER_LAYER_CLEAN);
          bytes = threshold_fixture (directory,cols[0],pass ? "output2" : "output",(gsize) width*height*channels);
          g_test_message ("Genuine old Threshold %s pass=%u native_channels=%u",cols[0],pass+1,channels);
          threshold_compare (filter,bytes,channels);
          {
            guchar *owned = threshold_fixture (directory,cols[0],pass ? "start2" : "start1",(gsize) width*height*channels);
            for (gsize i = 0; i < (gsize) width*height; ++i)
              g_assert_cmpmem (bytes+i*channels,channels-1,owned+i*channels,channels-1);
            g_free (owned);
          }
          g_free (bytes);
          g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, pass+1);
          if (!pass) g_assert_cmpuint (gimp_filter_layer_get_generation (filter), ==, generation);
          ++comparisons;
          if (!pass && (atoi (cols[25]) >= 0 || atoi (cols[26]) >= 0 || atoi (cols[27]) >= 0))
            {
              const guint64 settled_generation = gimp_filter_layer_get_generation (filter);
              GeglBuffer *cache = g_object_ref (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)));
              if (atoi (cols[25]) >= 0)
                {
                  bytes = threshold_fixture (directory,cols[0],"idle-mask",(gsize) image_width*image_height);
                  blinds_fixture_selection (image,image_width,image_height,0,0,bytes,atoi (cols[25]) == 0); g_free (bytes);
                }
              threshold_components (image,filter,channels,atoi (cols[26]),atoi (cols[27]));
              spin_ms (25);
              g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 1);
              g_assert_cmpuint (gimp_filter_layer_get_generation (filter), ==, settled_generation);
              g_assert_true (cache == gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)));
              g_assert_cmpint (atoi (cols[28]), ==, 1); g_assert_cmpint (atoi (cols[29]), ==, 1);
              bytes = threshold_fixture (directory,cols[0],"idle-output",(gsize) width*height*channels);
              threshold_compare (filter,bytes,channels); g_free (bytes); g_object_unref (cache);
            }
        }
      if (change.selection >= 0 || change.components >= 0 || change.lock >= 0) g_assert_true (change.called);
      g_signal_handlers_disconnect_by_data (filter,&change);
      g_object_unref (image); g_strfreev (cols); ++scenes;
    }
  /* Pin the complete finite small + true-spill corpus. The materializer
   * independently verifies old provenance and progress-before-merge events. */
  g_assert_cmpuint (scenes, ==, 174); g_assert_cmpuint (comparisons, ==, 256);
  g_assert_cmpuint (changed, ==, 28); g_assert_cmpuint (unchanged, ==, 54);
  g_assert_cmpuint (pre_merge, ==, 24); g_assert_cmpuint (mid_work, ==, 26);
  g_assert_cmpuint (layouts, ==, 15);
  g_test_message ("Threshold genuine old live scenes=%u comparisons=%u",scenes,comparisons);
  g_strfreev (lines); g_free (text); g_free (path);
}

/* Check the observed immediate-parent copy rectangle against the native scene,
 * separately from the byte oracle. A translated group can extend past every
 * image edge; its projection is not the top-level image rectangle. */
static void
threshold_projection_copy_geometry (gchar **lines, const gchar *id,
                                    GimpFilterLayer *filter, guint run,
                                    guint channels)
{
  GimpItem *item = GIMP_ITEM (filter), *parent = gimp_item_get_parent (item);
  GimpImage *image = gimp_item_get_image (item);
  const gint ox = gimp_item_get_offset_x (item), oy = gimp_item_get_offset_y (item);
  const gint px = parent ? gimp_item_get_offset_x (parent) : 0;
  const gint py = parent ? gimp_item_get_offset_y (parent) : 0;
  const gint pw = parent ? gimp_item_get_width (parent) : gimp_image_get_width (image);
  const gint ph = parent ? gimp_item_get_height (parent) : gimp_image_get_height (image);
  const gint width = gimp_item_get_width (item), height = gimp_item_get_height (item);
  const gint left = MAX (ox,px), top = MAX (oy,py);
  const gint right = MIN (ox+width,px+pw), bottom = MIN (oy+height,py+ph);
  guint matches = 0;
  for (guint row = 0; lines[row]; ++row)
    {
      gchar **cols;
      if (!*lines[row]) continue;
      cols = g_strsplit (lines[row],"\t",-1);
      g_assert_cmpuint (g_strv_length (cols), ==, 20);
      if (!strcmp (cols[0],id) && atoi (cols[1]) == (gint) run)
        {
          ++matches;
          g_assert_cmpint (atoi (cols[2]), ==, channels);
          g_assert_cmpint (atoi (cols[3]), ==, width); g_assert_cmpint (atoi (cols[4]), ==, height);
          g_assert_cmpint (atoi (cols[5]), ==, ox); g_assert_cmpint (atoi (cols[6]), ==, oy);
          g_assert_cmpint (atoi (cols[7]), ==, px); g_assert_cmpint (atoi (cols[8]), ==, py);
          g_assert_cmpint (atoi (cols[9]), ==, pw); g_assert_cmpint (atoi (cols[10]), ==, ph);
          g_assert_cmpint (atoi (cols[11]), ==, left-px); g_assert_cmpint (atoi (cols[12]), ==, top-py);
          g_assert_cmpint (atoi (cols[13]), ==, left-ox); g_assert_cmpint (atoi (cols[14]), ==, top-oy);
          g_assert_cmpint (atoi (cols[15]), ==, right-left); g_assert_cmpint (atoi (cols[16]), ==, bottom-top);
          for (guint c = 17; c < 20; ++c) g_assert_cmpint (atoi (cols[c]), ==, run-1);
        }
      g_strfreev (cols);
    }
  g_assert_cmpuint (matches, ==, 1);
}

/* Separate genuine-history supplement. Seed bytes exercise mark-as-loaded,
 * not actual XCF reopening (which has its own writer-corpus tests). The lower
 * source stays stationary and byte-identical while only the owner moves on
 * run three, expanding a group's immediate-parent projection if necessary. */
static void
threshold_actual_old_projection_history (void)
{
  const gchar *directory = g_getenv ("GIMP_PAINTER_THRESHOLD_PROJECTION_FIXTURES");
  gchar *path, *text = NULL, *copy_text = NULL, **lines, **copy_lines;
  guint scenes = 0, comparisons = 0, history_checks = 0, loaded_checks = 0;
  guint combinations = 0, copy_rows = 0;
  g_assert_nonnull (directory);
  path = g_build_filename (directory,"captures.tsv",NULL);
  g_assert_true (g_file_get_contents (path,&text,NULL,NULL)); g_free (path);
  path = g_build_filename (directory,"copies.tsv",NULL);
  g_assert_true (g_file_get_contents (path,&copy_text,NULL,NULL)); g_free (path);
  lines = g_strsplit (text,"\n",-1); copy_lines = g_strsplit (copy_text,"\n",-1);
  for (guint row = 0; copy_lines[row]; ++row) copy_rows += *copy_lines[row] != '\0';
  g_assert_cmpuint (copy_rows, ==, 48);
  for (guint row = 0; lines[row]; ++row)
    {
      const guint width = 17, height = 13, image_width = 13, image_height = 11;
      gchar **cols;
      guint channels, combination;
      gint grouped, seeded, ox, oy, final_x, final_y;
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
      g_assert_true (channels == 2 || channels == 4);
      g_assert_true (grouped == 0 || grouped == 1); g_assert_true (seeded == 0 || seeded == 1);
      g_assert_true ((ox == -4 && oy == -3) || (ox == 3 && oy == 2));
      g_assert_cmpint (final_x, ==, ox < 0 ? 3 : -4); g_assert_cmpint (final_y, ==, oy < 0 ? 2 : -3);
      for (guint c = 8; c < 12; ++c) g_assert_cmpint (atoi (cols[c]), ==, 3);
      g_assert_cmpint (atoi (cols[12]), ==, seeded ? 0 : -1);
      g_assert_cmpint (atoi (cols[13]), ==, seeded ? 0 : -1);
      combination = (channels == 4 ? 8 : 0) | (grouped ? 4 : 0) | (ox < 0 ? 2 : 0) | seeded;
      g_assert_cmpuint (combinations & (1U << combination), ==, 0);
      combinations |= 1U << combination;
      size = (gsize) width*height*channels;
      format = babl_format (channels == 2 ? "Y'A u8" : "R'G'B'A u8");
      image = gimp_image_new (gimp,image_width,image_height,channels == 2 ? GIMP_GRAY : GIMP_RGB,GIMP_PRECISION_U8_NON_LINEAR);
      parent = grouped ? gimp_group_layer_new (image) : NULL;
      if (parent) gimp_image_add_layer (image,parent,NULL,0,FALSE);
      source = gimp_layer_new (image,width,height,format,"Old projection source",1,GIMP_LAYER_MODE_NORMAL_LEGACY);
      gimp_image_add_layer (image,source,parent,0,FALSE); filter = filter_new (image,parent,width,height);
      gimp_item_set_offset (GIMP_ITEM (source),ox,oy); gimp_item_set_offset (GIMP_ITEM (filter),ox,oy);
      source_bytes = threshold_fixture (directory,cols[0],"source",size);
      gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (source)),NULL,0,format,source_bytes,GEGL_AUTO_ROWSTRIDE);
      bytes = threshold_fixture (directory,cols[0],"start-mask",(gsize) image_width*image_height);
      blinds_fixture_selection (image,image_width,image_height,0,0,bytes,TRUE); g_free (bytes);
      threshold_components (image,filter,channels,(1U << channels)-1,0);
      bytes = threshold_fixture (directory,cols[0],"seed",size);
      if (seeded)
        gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)),NULL,0,format,bytes,GEGL_AUTO_ROWSTRIDE);
      threshold_compare (filter,bytes,channels); g_free (bytes);
      threshold_define (filter,-1);
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
          threshold_compare (filter,bytes,channels); g_free (bytes); g_object_unref (cache);
          ++loaded_checks;
        }
      for (guint run = 1; run <= 3; ++run)
        {
          gchar label[32];
          guchar *owned;
          if (run == 2) threshold_define (filter,127);
          if (run == 3) gimp_item_translate (GIMP_ITEM (filter),final_x-ox,final_y-oy,FALSE);
          g_snprintf (label,sizeof label,"before%u",run);
          bytes = threshold_fixture (directory,cols[0],label,size);
          g_test_message ("Genuine old Threshold history %s run=%u before copy",cols[0],run);
          threshold_compare (filter,bytes,channels); g_free (bytes); ++history_checks;
          /* Verify the fixture's unchanged-source control without rewriting
           * native source pixels; this emits only the observed notification. */
          g_snprintf (label,sizeof label,"source%u",run);
          bytes = threshold_fixture (directory,cols[0],label,size);
          g_assert_cmpmem (bytes,size,source_bytes,size); g_free (bytes);
          g_assert_cmpint (gimp_item_get_offset_x (GIMP_ITEM (source)), ==, ox);
          g_assert_cmpint (gimp_item_get_offset_y (GIMP_ITEM (source)), ==, oy);
          gimp_drawable_update (GIMP_DRAWABLE (source),0,0,width,height);
          threshold_wait (filter,GIMP_FILTER_LAYER_CLEAN);
          threshold_projection_copy_geometry (copy_lines,cols[0],filter,run,channels);
          g_snprintf (label,sizeof label,"output%u",run);
          bytes = threshold_fixture (directory,cols[0],label,size);
          g_test_message ("Genuine old Threshold history %s run=%u output",cols[0],run);
          threshold_compare (filter,bytes,channels);
          g_snprintf (label,sizeof label,"start%u",run);
          owned = threshold_fixture (directory,cols[0],label,size);
          for (gsize i = 0; i < (gsize) width*height; ++i)
            g_assert_cmpmem (bytes+i*channels,channels-1,owned+i*channels,channels-1);
          g_free (owned); g_free (bytes);
          g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, run); ++comparisons;
        }
      g_free (source_bytes); g_object_unref (image); g_strfreev (cols); ++scenes;
    }
  g_assert_cmpuint (scenes, ==, 16); g_assert_cmpuint (comparisons, ==, 48);
  g_assert_cmpuint (history_checks, ==, 48); g_assert_cmpuint (loaded_checks, ==, 8);
  g_assert_cmpuint (combinations, ==, 0xffff);
  g_test_message ("Threshold genuine projection history scenes=%u comparisons=%u before_copy=%u loaded_idle=%u",
                  scenes,comparisons,history_checks,loaded_checks);
  g_strfreev (copy_lines); g_strfreev (lines); g_free (copy_text); g_free (text);
}

static void
threshold_native_profile_bytes (void)
{
  for (guint gray = 0; gray < 2; ++gray)
    for (guint spill = 0; spill < 2; ++spill)
      {
        const guint width = spill ? 1031 : 31, height = spill ? 1021 : 17, channels = gray ? 2 : 4;
        const gsize size = (gsize) width*height*channels;
        GimpImage *image = gimp_image_new (gimp,width,height,gray ? GIMP_GRAY : GIMP_RGB,GIMP_PRECISION_U8_NON_LINEAR);
        GimpColorProfile *profile = gray ? gimp_color_profile_new_d50_gray_lab_trc () : gimp_color_profile_new_rgb_adobe ();
        GimpLayer *source;
        GimpFilterLayer *filter;
        const Babl *native;
        GError *error = NULL;
        guchar *input = g_malloc (size), *expected = g_malloc (size), *actual = g_malloc (size);
        g_assert_true (gimp_image_set_color_profile (image,profile,&error));
        g_assert_no_error (error); g_object_unref (profile);
        native = gimp_image_get_layer_format (image,TRUE);
        source = gimp_layer_new (image,width,height,native,"Encoded Threshold source",1,GIMP_LAYER_MODE_NORMAL_LEGACY);
        gimp_image_add_layer (image,source,NULL,0,FALSE); filter = filter_new (image,NULL,width,height);
        for (gsize i = 0; i < (gsize) width*height; ++i)
          {
            for (guint c = 0; c + 1 < channels; ++c) input[i*channels+c] = (i*37+c*61+17)&255;
            input[i*channels+channels-1] = (i % 3) ? 191 : 63;
          }
        gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (source)),NULL,0,native,input,GEGL_AUTO_ROWSTRIDE);
        threshold_define (filter,127); threshold_wait (filter,GIMP_FILTER_LAYER_CLEAN);
        threshold_owned_input (filter,native,input);
        threshold_reference (input,expected,width,height,channels,127);
        /* Read and write the actual image encoding. A default-space read here
         * could hide an accidental color conversion in the worker adapter. */
        gegl_buffer_get (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)),NULL,1,native,
                         actual,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
        g_assert_cmpmem (actual,size,expected,size);
        g_assert_true (babl_format_get_space (gimp_drawable_get_format (GIMP_DRAWABLE (filter))) ==
                       babl_format_get_space (native));
        g_assert_cmpint (gimp_drawable_get_precision (GIMP_DRAWABLE (filter)), ==, GIMP_PRECISION_U8_NON_LINEAR);
        g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 1);
        g_test_message ("Threshold native encoded bytes: %s %s",gray ? "Gray Lab TRC" : "Adobe RGB",spill ? "spill" : "vector");
        g_free (input); g_free (expected); g_free (actual); g_object_unref (image);
      }
}

static void
threshold_admission_preserves_cache (void)
{
  guint64 original_spill;
  g_object_get (gimp->config,"painter-filter-spill-size",&original_spill,NULL);
  for (guint gray = 0; gray < 2; ++gray)
    {
      const guint side = gray ? 15000 : 11000, channels = gray ? 2 : 4;
      GimpImage *image = gimp_image_new (gimp,side,side,gray ? GIMP_GRAY : GIMP_RGB,GIMP_PRECISION_U8_NON_LINEAR);
      GimpFilterLayer *filter = filter_new (image,NULL,side,side);
      GimpValueArray *args = threshold_args (50);
      const Babl *native = gimp_image_get_layer_format (image,TRUE);
      const guchar saved[] = {17,29,53,123};
      guchar actual[4];
      GeglBuffer *cache;
      guint64 generation;
      gchar *error;
      threshold_set_args (filter,args);
      cache = g_object_ref (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)));
      /* Sparse sentinel only. Admission must reject the request before reading
       * or allocating an image-sized owner snapshot or starting a worker. */
      gegl_buffer_set (cache,GEGL_RECTANGLE (0,0,1,1),0,native,saved,GEGL_AUTO_ROWSTRIDE);
      gimp_filter_layer_mark_as_loaded (filter); generation = gimp_filter_layer_get_cache_generation (filter);
      gimp_filter_layer_invalidate (filter); threshold_wait (filter,GIMP_FILTER_LAYER_FAILED);
      error = gimp_filter_layer_dup_error (filter);
      g_assert_nonnull (strstr (error,"admission memory limit")); g_free (error);
      g_assert_true (cache == gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)));
      g_assert_cmpuint (gimp_filter_layer_get_cache_generation (filter), ==, generation);
      g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 0);
      gegl_buffer_get (cache,GEGL_RECTANGLE (0,0,1,1),1,native,actual,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
      g_assert_cmpmem (actual,channels,saved,channels); threshold_saved_args (filter,args);
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
      GimpValueArray *args = threshold_args (0);
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
      threshold_set_args (filter,args); gimp_filter_layer_mark_as_loaded (filter);
      cache = g_object_ref (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)));
      generation = gimp_filter_layer_get_cache_generation (filter);
      g_object_set (gimp->config,"painter-filter-spill-size",reservation-1,NULL);
      gimp_filter_layer_invalidate (filter); threshold_wait (filter,GIMP_FILTER_LAYER_FAILED);
      error = gimp_filter_layer_dup_error (filter); g_assert_nonnull (strstr (error,"spill")); g_free (error);
      g_assert_true (cache == gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)));
      g_assert_cmpuint (gimp_filter_layer_get_cache_generation (filter), ==, generation);
      g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 0); threshold_saved_args (filter,args);
      g_object_set (gimp->config,"painter-filter-spill-size",reservation,NULL);
      threshold_wait (filter,GIMP_FILTER_LAYER_CLEAN); threshold_compare (filter,input,channels);
      g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 1);
      g_test_message ("Threshold logical spill admission: channels=%u rejected=%" G_GUINT64_FORMAT " accepted=%" G_GUINT64_FORMAT
                      " bytes; not an RSS or GEGL swap measurement",channels,reservation-1,reservation);
      g_object_set (gimp->config,"painter-filter-spill-size",original_spill,NULL);
      g_free (input); g_object_unref (cache); gimp_value_array_unref (args); g_object_unref (image);
    }
}

static void
threshold_partial_import_interrupt (GimpFilterLayer *filter, gpointer data)
{
  ThresholdInterrupt *state = data;
  GimpFilterLayerProgress progress;
  if (state->called || gimp_filter_layer_get_state (filter) != GIMP_FILTER_LAYER_IMPORTING ||
      !gimp_filter_layer_get_progress (filter,&progress) || progress.value <= 0.85 || progress.value >= 1.0) return;
  /* A positive import fraction requires at least one actual staged pixel
   * chunk. The earlier first-state callback tests only the import gate. */
  g_test_message ("Threshold interrupts private staged import: progress=%.9f action=%u",progress.value,state->action);
  threshold_interrupt (filter,data);
}

static void
threshold_old_cache_bytes (GeglBuffer *cache, guint width, guint height)
{
  guchar *actual = g_malloc ((gsize) width*height*4);
  gegl_buffer_get (cache,NULL,1,babl_format ("R'G'B'A u8"),actual,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
  for (gsize i = 0; i < (gsize) width*height; ++i)
    g_assert_cmpmem (actual+i*4,4,((const guchar[]) {17,29,53,255}),4);
  g_free (actual);
}

static void
threshold_partial_import_lifecycle (void)
{
  for (guint spill = 0; spill < 2; ++spill)
    for (guint action = 0; action < 4; ++action)
      {
        const guint width = spill ? 1031 : 1024, height = spill ? 2049 : 1024;
        GimpImage *image = image_new (width,height);
        GimpLayer *source = source_new (image,NULL,width,height);
        GimpFilterLayer *filter = filter_new (image,NULL,width,height);
        ThresholdInterrupt interrupt = {GIMP_FILTER_LAYER_IMPORTING,action,source,FALSE};
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
        threshold_define (filter,255);
        fill (GIMP_LAYER (filter),17,29,53,255); gimp_filter_layer_mark_as_loaded (filter);
        cache = g_object_ref (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)));
        cache_generation = gimp_filter_layer_get_cache_generation (filter);
        g_signal_connect (filter,"filter-progress-changed",G_CALLBACK (threshold_partial_import_interrupt),&interrupt);
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
            if (action == 1) threshold_compare (filter,input,4);
            else blinds_lifecycle_constant (filter,(const guchar[]) {79,137,193,0});
            g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 2);
          }
        else
          {
            g_assert_cmpint (gimp_filter_layer_get_state (filter), ==,
                             action == 0 ? GIMP_FILTER_LAYER_FAILED : GIMP_FILTER_LAYER_CLOSED);
            threshold_drain_workers (); blinds_lifecycle_cached (filter,cache,cache_generation);
          }
        threshold_old_cache_bytes (cache,width,height);
        g_signal_handlers_disconnect_by_data (filter,&interrupt);
        g_free (input); g_object_unref (cache); g_object_unref (image);
      }
}

/* The double route deliberately retains the pre-existing ignored context-slot
 * compatibility and ignores byte-route owner masks. This includes U8 linear
 * and perceptual: real_samples() means every encoding except U8 nonlinear. */
static void
threshold_real_case (GimpComponentType type, GimpTRCType trc, gboolean gray, gboolean spill)
{
  const GimpPrecision precision = gimp_babl_precision (type,trc);
  const guint width = spill ? 513 : 7, height = spill ? 257 : 1, channels = gray ? 2 : 4;
  const gsize count = (gsize) width*height*channels;
  GimpImage *image = gimp_image_new (gimp,width,height,gray ? GIMP_GRAY : GIMP_RGB,precision);
  const Babl *format = gimp_babl_format (gray ? GIMP_GRAY : GIMP_RGB,
    gimp_babl_precision (GIMP_COMPONENT_TYPE_DOUBLE,trc),TRUE,
    babl_format_get_space (gimp_image_get_layer_format (image,TRUE)));
  GimpLayer *source = gimp_layer_new (image,width,height,gimp_image_get_layer_format (image,TRUE),
                                    "Real Threshold input",1,GIMP_LAYER_MODE_NORMAL_LEGACY);
  GimpFilterLayer *filter;
  GimpValueArray *args = threshold_args (127);
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
  gimp_image_set_component_active (image,GIMP_CHANNEL_ALPHA,FALSE);
  gimp_layer_set_lock_alpha (GIMP_LAYER (filter),TRUE,FALSE);
  threshold_set_args (filter,args); threshold_wait (filter,GIMP_FILTER_LAYER_CLEAN);
  threshold_saved_args (filter,args);
  threshold_owned_input (filter,format,owned);
  for (gsize i = 0; i < (gsize) width*height; ++i)
    owned[i*channels+channels-1] = 127.0/255.0 < owned[i*channels+channels-1] ? 1 : 0;
  /* Compare after native quantization, retaining all fractional color samples. */
  rounded = gegl_buffer_new (GEGL_RECTANGLE (0,0,width,height),gimp_drawable_get_format (GIMP_DRAWABLE (filter)));
  gegl_buffer_set (rounded,NULL,0,format,owned,GEGL_AUTO_ROWSTRIDE);
  gegl_buffer_get (rounded,NULL,1,format,owned,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
  gegl_buffer_get (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)),NULL,1,format,actual,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
  for (gsize i = 0; i < count; ++i) g_assert_cmpfloat (actual[i], ==, owned[i]);
  g_assert_cmpfloat (actual[channels-1], ==, 0); g_assert_cmpfloat (actual[2*channels-1], ==, 1);
  g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 1);
  g_test_message ("Threshold existing real route component=%d trc=%d gray=%d spill=%d",type,trc,gray,spill);
  g_free (input); g_free (owned); g_free (actual); g_object_unref (rounded);
  gimp_value_array_unref (args); g_object_unref (image);
}

static void
threshold_real_routes_unchanged (void)
{
  const GimpComponentType types[] = {GIMP_COMPONENT_TYPE_U8,GIMP_COMPONENT_TYPE_U16,GIMP_COMPONENT_TYPE_U32,
    GIMP_COMPONENT_TYPE_HALF,GIMP_COMPONENT_TYPE_FLOAT,GIMP_COMPONENT_TYPE_DOUBLE};
  const GimpTRCType trcs[] = {GIMP_TRC_LINEAR,GIMP_TRC_NON_LINEAR,GIMP_TRC_PERCEPTUAL};
  for (guint t = 0; t < G_N_ELEMENTS (types); ++t)
    for (guint r = 0; r < G_N_ELEMENTS (trcs); ++r)
      for (guint gray = 0; gray < 2; ++gray)
        if (gimp_babl_precision (types[t],trcs[r]) != GIMP_PRECISION_U8_NON_LINEAR)
          threshold_real_case (types[t],trcs[r],gray,FALSE);
  for (guint gray = 0; gray < 2; ++gray)
    threshold_real_case (GIMP_COMPONENT_TYPE_FLOAT,GIMP_TRC_LINEAR,gray,TRUE);
}

static void
threshold_idle_unchanged_notification (void)
{
  for (guint spill = 0; spill < 2; ++spill)
    {
      const guint width = spill ? 1031 : 17, height = spill ? 1021 : 13;
      GimpImage *image = image_new (width,height);
      GimpLayer *source = source_new (image,NULL,width,height);
      GimpFilterLayer *filter = filter_new (image,NULL,width,height);
      guint64 generation, cache_generation;
      GeglBuffer *cache;
      fill (source,31,121,217,255); threshold_define (filter,255);
      threshold_wait (filter,GIMP_FILTER_LAYER_CLEAN);
      blinds_lifecycle_constant (filter,(const guchar[]) {31,121,217,0});
      generation = gimp_filter_layer_get_generation (filter);
      cache_generation = gimp_filter_layer_get_cache_generation (filter);
      cache = g_object_ref (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)));
      gimp_channel_select_rectangle (gimp_image_get_mask (image),1,1,1,1,
                                     GIMP_CHANNEL_OP_REPLACE,FALSE,0,0,FALSE);
      gimp_image_set_component_active (image,GIMP_CHANNEL_ALPHA,FALSE);
      gimp_layer_set_lock_alpha (GIMP_LAYER (filter),TRUE,FALSE);
      spin_ms (25);
      g_assert_true (cache == gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)));
      g_assert_cmpuint (gimp_filter_layer_get_cache_generation (filter), ==, cache_generation);
      blinds_lifecycle_constant (filter,(const guchar[]) {31,121,217,0});
      g_assert_cmpuint (gimp_filter_layer_get_generation (filter), ==, generation);
      g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 1);
      /* Do not write the source: this specifically tests a notification whose
       * pixels did not change. The new context becomes visible on this run. */
      gimp_drawable_update (GIMP_DRAWABLE (source),0,0,width,height);
      threshold_wait (filter,GIMP_FILTER_LAYER_CLEAN);
      blinds_lifecycle_constant (filter,(const guchar[]) {31,121,217,255});
      g_assert_cmpuint (gimp_filter_layer_get_generation (filter), >, generation);
      g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 2);
      /* Keep changed input as a separate freshness control. */
      fill (source,79,137,193,255); threshold_wait (filter,GIMP_FILTER_LAYER_CLEAN);
      blinds_lifecycle_constant (filter,(const guchar[]) {79,137,193,255});
      g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 3);
      g_object_unref (cache); g_object_unref (image);
    }
}

/* Source-backed projection-copy bounds, complementary to genuine negative
 * offset captures. A parent group may extend outside the image canvas; the
 * old operation used that group's projection rectangle instead of the image.
 * Off-projection input retains old drawable bytes, including a loaded cache. */
static void
threshold_projection_input_bounds (void)
{
  const gint thresholds[] = {0,-1,127,255};
  for (guint gray = 0; gray < 2; ++gray)
    for (guint grouped = 0; grouped < 2; ++grouped)
      for (guint positive = 0; positive < 2; ++positive)
        for (guint seeded = 0; seeded < 2; ++seeded)
          {
            const guint width = 11, height = 9, channels = gray ? 2 : 4;
            const gsize size = (gsize) width*height*channels;
            gint ox = positive ? 13 : -3, oy = positive ? 10 : -2;
            GimpImage *image = gimp_image_new (gimp,17,13,gray ? GIMP_GRAY : GIMP_RGB,GIMP_PRECISION_U8_NON_LINEAR);
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
            for (guint pass = 0; pass < G_N_ELEMENTS (thresholds); ++pass)
              {
                if (pass == 3)
                  {
                    /* Moving the projection intersection must retain the
                     * previous completed local pixels in newly uncovered areas. */
                    ox = positive ? -3 : 13; oy = positive ? -2 : 10;
                    gimp_item_set_offset (GIMP_ITEM (source),ox,oy);
                    gimp_item_set_offset (GIMP_ITEM (filter),ox,oy);
                  }
                threshold_define (filter,thresholds[pass]);
                if (!pass && seeded)
                  {
                    gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)),NULL,0,format,previous,GEGL_AUTO_ROWSTRIDE);
                    gimp_filter_layer_mark_as_loaded (filter); gimp_filter_layer_invalidate (filter);
                  }
                threshold_wait (filter,GIMP_FILTER_LAYER_CLEAN);
                for (guint y = 0; y < height; ++y)
                  for (guint x = 0; x < width; ++x)
                    {
                      const gboolean copied = grouped || ((gint) x+ox >= 0 && (gint) y+oy >= 0 &&
                                                          (gint) x+ox < 17 && (gint) y+oy < 13);
                      const guint i = (y*width+x)*channels;
                      const guchar *original = copied ? input : previous;
                      for (guint c = 0; c + 1 < channels; ++c) expected[i+c] = original[i+c];
                      expected[i+channels-1] = thresholds[pass] < original[i+channels-1] ? 255 : 0;
                    }
                threshold_compare (filter,expected,channels);
                memcpy (previous,expected,size);
                g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, pass+1);
              }
            g_test_message ("Threshold projection input: gray=%u grouped=%u initial_positive=%u seeded=%u",
                            gray,grouped,positive,seeded);
            g_object_unref (image);
          }
}
