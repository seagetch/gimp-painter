/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Actual in-process FilterLayer execution. Analytic/source-backed cases below
 * test routing and lifecycle; only sharpen_actual_old_live uses captured old
 * application bytes. Neither the analytic checks nor the shared merge helper
 * substitutes for that separate genuine-runtime acceptance gate. */
static GimpValueArray *
sharpen_args (gint percent)
{
  return gimp_value_array_new_from_types (NULL,
    G_TYPE_INT,1,G_TYPE_INT,-123,G_TYPE_INT,456,G_TYPE_INT,percent,G_TYPE_NONE);
}

static void
sharpen_set_args (GimpFilterLayer *filter, GimpValueArray *args)
{
  static const guchar saved[] = {0,0xff,2,0x80,0,3,0};
  GBytes *raw = g_bytes_new_static (saved,sizeof saved);
  GError *error = NULL;
  g_assert_true (gimp_filter_layer_set_definition (filter,"plug-in-sharpen",raw,args,&error));
  g_assert_no_error (error); g_bytes_unref (raw);
}

static void
sharpen_define (GimpFilterLayer *filter, gint percent)
{
  GimpValueArray *args = sharpen_args (percent);
  sharpen_set_args (filter,args); gimp_value_array_unref (args);
}

static void
sharpen_saved_args (GimpFilterLayer *filter, GimpValueArray *expected)
{
  static const guchar saved[] = {0,0xff,2,0x80,0,3,0};
  GBytes *raw = gimp_filter_layer_ref_definition (filter);
  GimpValueArray *actual = gimp_filter_layer_dup_args (filter);
  gchar *name = gimp_filter_layer_dup_procedure (filter);
  g_assert_cmpstr (name, ==, "plug-in-sharpen");
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
sharpen_wait (GimpFilterLayer *filter, GimpFilterLayerState expected)
{
  const gint64 deadline = g_get_monotonic_time () + 90 * G_TIME_SPAN_SECOND;
  while (gimp_filter_layer_get_state (filter) != expected && g_get_monotonic_time () < deadline)
    {
      if (gimp_filter_layer_get_state (filter) == GIMP_FILTER_LAYER_FAILED && expected != GIMP_FILTER_LAYER_FAILED)
        { gchar *error = gimp_filter_layer_dup_error (filter); g_error ("Sharpen failed: %s",error); }
      g_main_context_iteration (NULL,FALSE); g_usleep (100);
    }
  g_assert_cmpint (gimp_filter_layer_get_state (filter), ==, expected);
}

static void
sharpen_compare (GimpFilterLayer *filter, const guchar *expected, guint channels)
{
  const guint width = gimp_item_get_width (GIMP_ITEM (filter));
  const guint height = gimp_item_get_height (GIMP_ITEM (filter));
  const Babl *format = babl_format (channels == 2 ? "Y'A u8" : "R'G'B'A u8");
  guchar *actual = g_malloc ((gsize) width*height*channels);
  gegl_buffer_get (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)),NULL,1,format,
                   actual,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
  g_assert_cmpmem (actual,(gsize) width*height*channels,expected,(gsize) width*height*channels);
  g_free (actual);
}

/* Deliberately simple scalar source oracle. This is NOT an old-runtime capture.
 * The separately sealed raster tests mechanically compare the pinned C bodies.
 * This local calculation establishes that the live route really invokes the
 * sharpen operation, including its defined noncanonical integer percentages. */
static gint
sharpen_floor_eighth (gint value)
{ return value / 8 - (value < 0 && value % 8 != 0); }

static void
sharpen_opaque_reference (const guchar *input, guchar *output,
                          guint width, guint height, guint channels, gint percent)
{
  const gint factor = MAX (1,100-percent);
  memcpy (output,input,(gsize) width*height*channels);
  for (guint y = 1; y + 1 < height; ++y)
    for (guint x = 1; x + 1 < width; ++x)
      for (guint c = 0; c + 1 < channels; ++c)
        {
          const gsize index = ((gsize) y*width+x)*channels+c;
          gint value = 800*input[index]/factor;
          for (gint dy = -1; dy <= 1; ++dy)
            for (gint dx = -1; dx <= 1; ++dx)
              if (dx || dy)
                {
                  const gint sample = input[((gsize) (y+dy)*width+x+dx)*channels+c];
                  value -= sharpen_floor_eighth (4+800*sample/factor-8*sample);
                }
          output[index] = CLAMP (sharpen_floor_eighth (value+4),0,255);
        }
}

static void
sharpen_integer_native_route (void)
{
  const gint percentages[] = {10,0,-100,50,99,123,G_MAXINT,100-G_MAXINT};
  for (guint gray = 0; gray < 2; ++gray)
    for (guint narrow = 0; narrow < 2; ++narrow)
      {
        const guint width = narrow ? 2 : 7, height = 5, channels = gray ? 2 : 4;
        const Babl *format = babl_format (gray ? "Y'A u8" : "R'G'B'A u8");
        GimpImage *image = gimp_image_new (gimp,width,height,gray ? GIMP_GRAY : GIMP_RGB,GIMP_PRECISION_U8_NON_LINEAR);
        GimpLayer *source = gimp_layer_new (image,width,height,format,"Sharpen source",1,GIMP_LAYER_MODE_NORMAL_LEGACY);
        GimpFilterLayer *filter;
        guchar input[7*5*4], expected[7*5*4];
        gimp_image_add_layer (image,source,NULL,0,FALSE);
        filter = filter_new (image,NULL,width,height);
        for (guint i = 0; i < width*height; ++i)
          {
            for (guint c = 0; c + 1 < channels; ++c) input[i*channels+c] = (i*37+c*61+17)&255;
            input[i*channels+channels-1] = 255;
          }
        gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (source)),NULL,0,format,input,GEGL_AUTO_ROWSTRIDE);
        for (guint p = 0; p < G_N_ELEMENTS (percentages); ++p)
          {
            GimpValueArray *args = sharpen_args (percentages[p]);
            sharpen_set_args (filter,args); sharpen_wait (filter,GIMP_FILTER_LAYER_CLEAN);
            sharpen_opaque_reference (input,expected,width,height,channels,percentages[p]);
            sharpen_compare (filter,expected,channels); sharpen_saved_args (filter,args);
            g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, p+1);
            gimp_value_array_unref (args);
          }
        g_object_unref (image);
      }
}

static void
sharpen_invalid_definitions_keep_cache (void)
{
  GimpImage *image = image_new (7,5);
  GimpLayer *source = source_new (image,NULL,7,5);
  GimpFilterLayer *filter = filter_new (image,NULL,7,5);
  GeglBuffer *cache;
  guint64 generation;
  fill (source,31,121,217,255); sharpen_define (filter,0);
  sharpen_wait (filter,GIMP_FILTER_LAYER_CLEAN);
  cache = g_object_ref (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)));
  generation = gimp_filter_layer_get_cache_generation (filter);
  for (guint kind = 0; kind < 7; ++kind)
    {
      GimpValueArray *args = sharpen_args (10);
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
      else g_value_set_int (gimp_value_array_index (args,3),G_MININT);
      sharpen_set_args (filter,args); sharpen_wait (filter,GIMP_FILTER_LAYER_FAILED);
      g_assert_true (cache == gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)));
      g_assert_cmpuint (gimp_filter_layer_get_cache_generation (filter), ==, generation);
      g_assert_cmpuint (gimp_filter_layer_get_generation (filter), >, generation);
      g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 1);
      sharpen_saved_args (filter,args); pixel (GIMP_LAYER (filter),31,121,217,255);
      gimp_value_array_unref (args);
    }
  sharpen_define (filter,10); sharpen_wait (filter,GIMP_FILTER_LAYER_CLEAN);
  g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 2);
  g_object_unref (cache); g_object_unref (image);
}

static void
sharpen_unsupported_geometry_precision (void)
{
  const GimpPrecision precisions[] = {GIMP_PRECISION_U8_LINEAR,GIMP_PRECISION_U16_NON_LINEAR,GIMP_PRECISION_FLOAT_NON_LINEAR};
  for (guint kind = 0; kind < 8; ++kind)
    {
      const guint width = kind == 0 ? 1 : 7, height = kind == 1 ? 1 : kind == 2 ? 2 : 5;
      GimpImage *image = gimp_image_new (gimp,width,height,GIMP_RGB,kind >= 5 ? precisions[kind-5] : GIMP_PRECISION_U8_NON_LINEAR);
      GimpFilterLayer *filter = filter_new (image,NULL,width,height);
      GimpValueArray *args = sharpen_args (10);
      GeglBuffer *cache;
      guint64 generation;
      sharpen_set_args (filter,args);
      fill (GIMP_LAYER (filter),17,29,53,255); gimp_filter_layer_mark_as_loaded (filter);
      cache = g_object_ref (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)));
      generation = gimp_filter_layer_get_cache_generation (filter);
      if (kind == 3 || kind == 4)
        gimp_channel_select_rectangle (gimp_image_get_mask (image),1,1,kind == 3 ? 1 : 3,kind == 3 ? 3 : 2,
                                       GIMP_CHANNEL_OP_REPLACE,FALSE,0,0,FALSE);
      gimp_filter_layer_invalidate (filter); sharpen_wait (filter,GIMP_FILTER_LAYER_FAILED);
      g_assert_true (cache == gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)));
      g_assert_cmpuint (gimp_filter_layer_get_cache_generation (filter), ==, generation);
      g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 0);
      sharpen_saved_args (filter,args);
      g_object_unref (cache); gimp_value_array_unref (args); g_object_unref (image);
    }
}

typedef struct
{
  GimpImage *image;
  guint width, height, kind;
  gint ox, oy;
  gboolean called;
} SharpenContextChange;

static void
sharpen_late_context (GimpFilterLayer *filter, gpointer data)
{
  SharpenContextChange *state = data;
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
sharpen_late_context_routes (void)
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
        SharpenContextChange change = {image,width,height,kind,ox,oy,FALSE};
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
        sharpen_define (filter,0);
        fill (GIMP_LAYER (filter),17,29,53,255); gimp_filter_layer_mark_as_loaded (filter);
        cache = g_object_ref (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)));
        cache_generation = gimp_filter_layer_get_cache_generation (filter);
        g_signal_connect (filter,"filter-state-changed",G_CALLBACK (sharpen_late_context),&change);
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
        sharpen_compare (filter,expected,4);
        g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 1);
        spin_ms (15);
        g_assert_cmpuint (gimp_filter_layer_get_generation (filter), ==, generation);
        g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 1);
        fill (source,79,137,193,255); sharpen_wait (filter,GIMP_FILTER_LAYER_CLEAN);
        blinds_lifecycle_constant (filter,(const guchar[]) {79,137,193,255});
        g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 2);
        g_signal_handlers_disconnect_by_data (filter,&change);
        g_free (expected); g_object_unref (cache); g_object_unref (image);
      }
}

static void
sharpen_freeze_context (GimpFilterLayer *filter, gpointer data)
{
  SharpenContextChange *state = data;
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
sharpen_sealed_context_routes (void)
{
  for (guint spill = 0; spill < 2; ++spill)
    {
      const guint width = spill ? 1031 : 513, height = spill ? 1021 : 257;
      GimpImage *image = image_new (width,height);
      GimpLayer *source = source_new (image,NULL,width,height);
      GimpFilterLayer *filter = filter_new (image,NULL,width,height);
      SharpenContextChange change = {image,width,height,0,0,0,FALSE};
      guchar *input = g_malloc0 ((gsize) width*height*4), *expected = g_malloc ((gsize) width*height*4);
      const gsize center = ((gsize) (height/2)*width+width/2)*4;
      for (gsize i = 0; i < (gsize) width*height; ++i) input[i*4+3] = 255;
      input[center] = 30; input[center+1] = 60; input[center+2] = 100;
      gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (source)),NULL,0,babl_format ("R'G'B'A u8"),input,GEGL_AUTO_ROWSTRIDE);
      g_assert_true (gimp_image_get_mask (image)->bounds_known); g_assert_true (gimp_image_get_mask (image)->empty);
      g_signal_connect (filter,"filter-state-changed",G_CALLBACK (sharpen_freeze_context),&change);
      sharpen_define (filter,50); sharpen_wait (filter,GIMP_FILTER_LAYER_CLEAN);
      g_assert_true (change.called);
      sharpen_opaque_reference (input,expected,width,height,4,50); sharpen_compare (filter,expected,4);
      g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 1);
      input[center] = 71;
      gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (source)),NULL,0,babl_format ("R'G'B'A u8"),input,GEGL_AUTO_ROWSTRIDE);
      gimp_drawable_update (GIMP_DRAWABLE (source),0,0,width,height); sharpen_wait (filter,GIMP_FILTER_LAYER_CLEAN);
      sharpen_compare (filter,input,4);
      g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 2);
      g_signal_handlers_disconnect_by_data (filter,&change);
      g_free (input); g_free (expected); g_object_unref (image);
    }
}

static void
sharpen_outside_selection_no_merge (void)
{
  for (guint spill = 0; spill < 2; ++spill)
    {
      const guint width = spill ? 1031 : 63, height = spill ? 1021 : 61;
      GimpImage *image = image_new (width+4,height+4);
      GimpLayer *source = source_new (image,NULL,width,height);
      GimpFilterLayer *filter = filter_new (image,NULL,width,height);
      SharpenContextChange change = {image,width,height,1,0,0,FALSE};
      guchar *input = g_malloc0 ((gsize) width*height*4), *expected = g_malloc ((gsize) width*height*4);
      const gsize center = ((gsize) (height/2)*width+width/2)*4;
      for (gsize i = 0; i < (gsize) width*height; ++i) input[i*4+3] = 255;
      input[center] = 30; input[center+1] = 60; input[center+2] = 100;
      gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (source)),NULL,0,babl_format ("R'G'B'A u8"),input,GEGL_AUTO_ROWSTRIDE);
      gimp_channel_select_rectangle (gimp_image_get_mask (image),width+1,height+1,2,2,
                                     GIMP_CHANNEL_OP_REPLACE,FALSE,0,0,FALSE);
      g_signal_connect (filter,"filter-state-changed",G_CALLBACK (sharpen_late_context),&change);
      sharpen_define (filter,50); sharpen_wait (filter,GIMP_FILTER_LAYER_CLEAN);
      g_assert_true (change.called); sharpen_compare (filter,input,4);
      g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 1);
      gimp_drawable_update (GIMP_DRAWABLE (source),0,0,width,height); sharpen_wait (filter,GIMP_FILTER_LAYER_CLEAN);
      sharpen_opaque_reference (input,expected,width,height,4,50); sharpen_compare (filter,expected,4);
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
} SharpenInterrupt;

static void
sharpen_interrupt (GimpFilterLayer *filter, gpointer data)
{
  SharpenInterrupt *state = data;
  if (state->called || gimp_filter_layer_get_state (filter) != state->phase) return;
  state->called = TRUE;
  if (state->action == 0) gimp_filter_layer_cancel (filter);
  else if (state->action == 1) sharpen_define (filter,0);
  else if (state->action == 2) fill (state->source,79,137,193,255);
  else g_assert_true (gimp_painter_binding_close (G_OBJECT (filter),NULL));
}

static void
sharpen_drain_workers (void)
{
  const gint64 deadline = g_get_monotonic_time () + 90 * G_TIME_SPAN_SECOND;
  while (gimp_test_filter_pending_jobs () && g_get_monotonic_time () < deadline)
    { g_main_context_iteration (NULL,FALSE); g_usleep (100); }
  g_assert_cmpuint (gimp_test_filter_pending_jobs (), ==, 0);
}

static void
sharpen_cancel_replace_lower_close (void)
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
          SharpenInterrupt interrupt = {phases[phase],action,NULL,FALSE};
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
          sharpen_define (filter,50);
          fill (GIMP_LAYER (filter),17,29,53,255); gimp_filter_layer_mark_as_loaded (filter);
          cache = g_object_ref (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)));
          cache_generation = gimp_filter_layer_get_cache_generation (filter);
          g_signal_connect (filter,"filter-state-changed",G_CALLBACK (sharpen_interrupt),&interrupt);
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
              if (action == 1) sharpen_compare (filter,input,4);
              else blinds_lifecycle_constant (filter,(const guchar[]) {79,137,193,255});
              g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==,
                               phases[phase] == GIMP_FILTER_LAYER_RUNNING || phases[phase] == GIMP_FILTER_LAYER_IMPORTING ? 2 : 1);
            }
          else
            {
              sharpen_wait (filter,action == 0 ? GIMP_FILTER_LAYER_FAILED : GIMP_FILTER_LAYER_CLOSED);
              g_assert_true (interrupt.called); sharpen_drain_workers ();
              blinds_lifecycle_cached (filter,cache,cache_generation);
              if (action == 0)
                {
                  GimpValueArray *args = sharpen_args (50);
                  sharpen_saved_args (filter,args); gimp_value_array_unref (args);
                  fill (source,79,137,193,255); sharpen_wait (filter,GIMP_FILTER_LAYER_CLEAN);
                  blinds_lifecycle_constant (filter,(const guchar[]) {79,137,193,255});
                }
            }
          g_signal_handlers_disconnect_by_data (filter,&interrupt);
          g_free (input); g_object_unref (cache); g_object_unref (image);
        }
}

/* Genuine old application capture contract is emitted by the dedicated
 * Sharpen harness. The fixture runner verifies its source/provenance manifest
 * before setting this directory. These comparisons must never be silently
 * replaced by the analytic reference above. */
typedef struct
{
  GimpImage *image;
  const gchar *directory, *id;
  guint image_width, image_height;
  gint selection, components, lock;
  gboolean called;
} SharpenOldContext;

static guchar *
sharpen_fixture (const gchar *directory, const gchar *id, const gchar *label, gsize size)
{ return blinds_fixture_bytes (directory,id,label,size); }

static void
sharpen_components (GimpImage *image, GimpFilterLayer *filter, guint channels, gint active, gint lock)
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
sharpen_old_late_context (GimpFilterLayer *filter, gpointer data)
{
  SharpenOldContext *state = data;
  const guint channels = gimp_drawable_get_base_type (GIMP_DRAWABLE (filter)) == GIMP_GRAY ? 2 : 4;
  if (state->called || gimp_filter_layer_get_state (filter) != GIMP_FILTER_LAYER_IMPORTING) return;
  state->called = TRUE;
  if (state->selection >= 0)
    {
      guchar *mask = sharpen_fixture (state->directory,state->id,"final-mask",(gsize) state->image_width*state->image_height);
      blinds_fixture_selection (state->image,state->image_width,state->image_height,0,0,mask,state->selection == 0);
      g_free (mask);
    }
  sharpen_components (state->image,filter,channels,state->components,state->lock);
}

static void
sharpen_actual_old_live (void)
{
  const gchar *directory = g_getenv ("GIMP_PAINTER_SHARPEN_FIXTURES");
  gchar *path, *text = NULL, **lines;
  guint comparisons = 0, scenes = 0;
  g_assert_nonnull (directory);
  path = g_build_filename (directory,"captures.tsv",NULL);
  g_assert_true (g_file_get_contents (path,&text,NULL,NULL));
  lines = g_strsplit (text,"\n",-1);
  for (guint row = 0; lines[row]; ++row)
    {
      gchar **cols;
      guint width,height,image_width,image_height,source_channels,channels,passes;
      gint ox,oy,percent;
      GimpImage *image;
      GimpLayer *source;
      GimpFilterLayer *filter;
      const Babl *source_format;
      SharpenOldContext change;
      guchar *bytes;
      guint64 generation;
      if (!*lines[row]) continue;
      cols = g_strsplit (lines[row],"\t",-1);
      g_assert_cmpuint (g_strv_length (cols), ==, 24);
      if (!atoi (cols[1])) { g_strfreev (cols); continue; }
      source_channels = atoi (cols[2]); channels = atoi (cols[3]);
      width = atoi (cols[4]); height = atoi (cols[5]);
      image_width = atoi (cols[6]); image_height = atoi (cols[7]);
      ox = atoi (cols[8]); oy = atoi (cols[9]); percent = atoi (cols[10]);
      passes = atoi (cols[17]) ? 2 : 1;
      g_assert_true (channels == 2 || channels == 4);
      source_format = babl_format (source_channels == 1 ? "Y' u8" : source_channels == 2 ? "Y'A u8" :
                                  source_channels == 3 ? "R'G'B' u8" : "R'G'B'A u8");
      image = gimp_image_new (gimp,image_width,image_height,channels == 2 ? GIMP_GRAY : GIMP_RGB,GIMP_PRECISION_U8_NON_LINEAR);
      source = gimp_layer_new (image,width,height,source_format,"Old Sharpen source",1,GIMP_LAYER_MODE_NORMAL_LEGACY);
      gimp_image_add_layer (image,source,NULL,0,FALSE); filter = filter_new (image,NULL,width,height);
      gimp_item_set_offset (GIMP_ITEM (source),ox,oy); gimp_item_set_offset (GIMP_ITEM (filter),ox,oy);
      bytes = sharpen_fixture (directory,cols[0],"source",(gsize) width*height*source_channels);
      gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (source)),NULL,0,source_format,bytes,GEGL_AUTO_ROWSTRIDE);
      g_free (bytes);
      bytes = sharpen_fixture (directory,cols[0],"start-mask",(gsize) image_width*image_height);
      blinds_fixture_selection (image,image_width,image_height,0,0,bytes,atoi (cols[11]) == 0); g_free (bytes);
      sharpen_components (image,filter,channels,atoi (cols[13]),atoi (cols[15]));
      change = (SharpenOldContext) {image,directory,cols[0],image_width,image_height,
                                   atoi (cols[12]),atoi (cols[14]),atoi (cols[16]),FALSE};
      if (change.selection >= 0 || change.components >= 0 || change.lock >= 0)
        {
          g_assert_cmpint (atoi (cols[11]), ==, 1); /* Nonempty start keeps the late gate unsealed. */
          g_signal_connect (filter,"filter-state-changed",G_CALLBACK (sharpen_old_late_context),&change);
        }
      sharpen_define (filter,percent); generation = gimp_filter_layer_get_generation (filter);
      for (guint pass = 0; pass < passes; ++pass)
        {
          if (pass)
            {
              bytes = sharpen_fixture (directory,cols[0],"source2",(gsize) width*height*source_channels);
              gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (source)),NULL,0,source_format,bytes,GEGL_AUTO_ROWSTRIDE);
              g_free (bytes); gimp_drawable_update (GIMP_DRAWABLE (source),0,0,width,height);
            }
          sharpen_wait (filter,GIMP_FILTER_LAYER_CLEAN);
          bytes = sharpen_fixture (directory,cols[0],pass ? "output2" : "output",(gsize) width*height*channels);
          g_test_message ("Genuine old Sharpen %s pass=%u native_channels=%u",cols[0],pass+1,channels);
          sharpen_compare (filter,bytes,channels); g_free (bytes);
          g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, pass+1);
          if (!pass) g_assert_cmpuint (gimp_filter_layer_get_generation (filter), ==, generation);
          ++comparisons;
        }
      if (change.selection >= 0 || change.components >= 0 || change.lock >= 0) g_assert_true (change.called);
      g_signal_handlers_disconnect_by_data (filter,&change);
      g_object_unref (image); g_strfreev (cols); ++scenes;
    }
  g_assert_cmpuint (scenes, ==, 74);
  g_assert_cmpuint (comparisons, >=, scenes);
  g_strfreev (lines); g_free (text); g_free (path);
}

static void
sharpen_native_profile_bytes (void)
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
        source = gimp_layer_new (image,width,height,native,"Encoded Sharpen source",1,GIMP_LAYER_MODE_NORMAL_LEGACY);
        gimp_image_add_layer (image,source,NULL,0,FALSE); filter = filter_new (image,NULL,width,height);
        for (gsize i = 0; i < (gsize) width*height; ++i)
          {
            for (guint c = 0; c + 1 < channels; ++c) input[i*channels+c] = (i*37+c*61+17)&255;
            input[i*channels+channels-1] = 255;
          }
        gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (source)),NULL,0,native,input,GEGL_AUTO_ROWSTRIDE);
        sharpen_define (filter,50); sharpen_wait (filter,GIMP_FILTER_LAYER_CLEAN);
        sharpen_opaque_reference (input,expected,width,height,channels,50);
        /* Read and write the actual image encoding. A default-space read here
         * could hide an accidental color conversion in the worker adapter. */
        gegl_buffer_get (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)),NULL,1,native,
                         actual,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
        g_assert_cmpmem (actual,size,expected,size);
        g_assert_true (babl_format_get_space (gimp_drawable_get_format (GIMP_DRAWABLE (filter))) ==
                       babl_format_get_space (native));
        g_assert_cmpint (gimp_drawable_get_precision (GIMP_DRAWABLE (filter)), ==, GIMP_PRECISION_U8_NON_LINEAR);
        g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 1);
        g_test_message ("Sharpen native encoded bytes: %s %s",gray ? "Gray Lab TRC" : "Adobe RGB",spill ? "spill" : "vector");
        g_free (input); g_free (expected); g_free (actual); g_object_unref (image);
      }
}

static void
sharpen_admission_preserves_cache (void)
{
  guint64 original_spill;
  g_object_get (gimp->config,"painter-filter-spill-size",&original_spill,NULL);
  for (guint gray = 0; gray < 2; ++gray)
    {
      const guint side = gray ? 15000 : 11000, channels = gray ? 2 : 4;
      GimpImage *image = gimp_image_new (gimp,side,side,gray ? GIMP_GRAY : GIMP_RGB,GIMP_PRECISION_U8_NON_LINEAR);
      GimpFilterLayer *filter = filter_new (image,NULL,side,side);
      GimpValueArray *args = sharpen_args (50);
      const Babl *native = gimp_image_get_layer_format (image,TRUE);
      const guchar saved[] = {17,29,53,123};
      guchar actual[4];
      GeglBuffer *cache;
      guint64 generation;
      gchar *error;
      sharpen_set_args (filter,args);
      cache = g_object_ref (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)));
      /* Sparse sentinel only. Admission must reject the request before reading
       * or allocating an image-sized owner snapshot or starting a worker. */
      gegl_buffer_set (cache,GEGL_RECTANGLE (0,0,1,1),0,native,saved,GEGL_AUTO_ROWSTRIDE);
      gimp_filter_layer_mark_as_loaded (filter); generation = gimp_filter_layer_get_cache_generation (filter);
      gimp_filter_layer_invalidate (filter); sharpen_wait (filter,GIMP_FILTER_LAYER_FAILED);
      error = gimp_filter_layer_dup_error (filter);
      g_assert_nonnull (strstr (error,"admission memory limit")); g_free (error);
      g_assert_true (cache == gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)));
      g_assert_cmpuint (gimp_filter_layer_get_cache_generation (filter), ==, generation);
      g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 0);
      gegl_buffer_get (cache,GEGL_RECTANGLE (0,0,1,1),1,native,actual,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
      g_assert_cmpmem (actual,channels,saved,channels); sharpen_saved_args (filter,args);
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
      GimpValueArray *args = sharpen_args (0);
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
      sharpen_set_args (filter,args); gimp_filter_layer_mark_as_loaded (filter);
      cache = g_object_ref (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)));
      generation = gimp_filter_layer_get_cache_generation (filter);
      g_object_set (gimp->config,"painter-filter-spill-size",reservation-1,NULL);
      gimp_filter_layer_invalidate (filter); sharpen_wait (filter,GIMP_FILTER_LAYER_FAILED);
      error = gimp_filter_layer_dup_error (filter); g_assert_nonnull (strstr (error,"spill")); g_free (error);
      g_assert_true (cache == gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)));
      g_assert_cmpuint (gimp_filter_layer_get_cache_generation (filter), ==, generation);
      g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 0); sharpen_saved_args (filter,args);
      g_object_set (gimp->config,"painter-filter-spill-size",reservation,NULL);
      sharpen_wait (filter,GIMP_FILTER_LAYER_CLEAN); sharpen_compare (filter,input,channels);
      g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 1);
      g_test_message ("Sharpen logical spill admission: channels=%u rejected=%" G_GUINT64_FORMAT " accepted=%" G_GUINT64_FORMAT
                      " bytes; not an RSS or GEGL swap measurement",channels,reservation-1,reservation);
      g_object_set (gimp->config,"painter-filter-spill-size",original_spill,NULL);
      g_free (input); g_object_unref (cache); gimp_value_array_unref (args); g_object_unref (image);
    }
}

static void
sharpen_partial_import_interrupt (GimpFilterLayer *filter, gpointer data)
{
  SharpenInterrupt *state = data;
  GimpFilterLayerProgress progress;
  if (state->called || gimp_filter_layer_get_state (filter) != GIMP_FILTER_LAYER_IMPORTING ||
      !gimp_filter_layer_get_progress (filter,&progress) || progress.value <= 0.85 || progress.value >= 1.0) return;
  /* A positive import fraction requires at least one actual staged pixel
   * chunk. The earlier first-state callback tests only the import gate. */
  g_test_message ("Sharpen interrupts private staged import: progress=%.9f action=%u",progress.value,state->action);
  sharpen_interrupt (filter,data);
}

static void
sharpen_old_cache_bytes (GeglBuffer *cache, guint width, guint height)
{
  guchar *actual = g_malloc ((gsize) width*height*4);
  gegl_buffer_get (cache,NULL,1,babl_format ("R'G'B'A u8"),actual,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
  for (gsize i = 0; i < (gsize) width*height; ++i)
    g_assert_cmpmem (actual+i*4,4,((const guchar[]) {17,29,53,255}),4);
  g_free (actual);
}

static void
sharpen_partial_import_lifecycle (void)
{
  for (guint spill = 0; spill < 2; ++spill)
    for (guint action = 0; action < 4; ++action)
      {
        const guint width = spill ? 1031 : 1024, height = spill ? 2049 : 1024;
        GimpImage *image = image_new (width,height);
        GimpLayer *source = source_new (image,NULL,width,height);
        GimpFilterLayer *filter = filter_new (image,NULL,width,height);
        SharpenInterrupt interrupt = {GIMP_FILTER_LAYER_IMPORTING,action,source,FALSE};
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
        sharpen_define (filter,50);
        fill (GIMP_LAYER (filter),17,29,53,255); gimp_filter_layer_mark_as_loaded (filter);
        cache = g_object_ref (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)));
        cache_generation = gimp_filter_layer_get_cache_generation (filter);
        g_signal_connect (filter,"filter-progress-changed",G_CALLBACK (sharpen_partial_import_interrupt),&interrupt);
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
            if (action == 1) sharpen_compare (filter,input,4);
            else blinds_lifecycle_constant (filter,(const guchar[]) {79,137,193,255});
            g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 2);
          }
        else
          {
            g_assert_cmpint (gimp_filter_layer_get_state (filter), ==,
                             action == 0 ? GIMP_FILTER_LAYER_FAILED : GIMP_FILTER_LAYER_CLOSED);
            sharpen_drain_workers (); blinds_lifecycle_cached (filter,cache,cache_generation);
          }
        sharpen_old_cache_bytes (cache,width,height);
        g_signal_handlers_disconnect_by_data (filter,&interrupt);
        g_free (input); g_object_unref (cache); g_object_unref (image);
      }
}
