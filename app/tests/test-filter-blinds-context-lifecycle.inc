/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Actual native lifecycle probes. These analytic limiting cases complement,
 * rather than replace, the genuine-old byte fixtures in the preceding file. */
static void
blinds_lifecycle_cached (GimpFilterLayer *filter, GeglBuffer *cache,
                         guint64 cache_generation)
{
  g_assert_true (cache == gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)));
  g_assert_cmpuint (gimp_filter_layer_get_cache_generation (filter), ==, cache_generation);
  pixel (GIMP_LAYER (filter),17,29,53,255);
}

static void
blinds_lifecycle_settle (GimpFilterLayer *filter, GeglBuffer *cache,
                         guint64 cache_generation, guint64 generation)
{
  const gint64 deadline = g_get_monotonic_time () + 90 * G_TIME_SPAN_SECOND;
  while (gimp_filter_layer_get_state (filter) != GIMP_FILTER_LAYER_CLEAN &&
         g_get_monotonic_time () < deadline)
    {
      if (gimp_filter_layer_get_state (filter) == GIMP_FILTER_LAYER_FAILED)
        { gchar *error = gimp_filter_layer_dup_error (filter); g_error ("Blinds lifecycle failed: %s",error); }
      blinds_lifecycle_cached (filter,cache,cache_generation);
      g_main_context_iteration (NULL,FALSE); g_usleep (100);
    }
  g_assert_cmpint (gimp_filter_layer_get_state (filter), ==, GIMP_FILTER_LAYER_CLEAN);
  g_assert_cmpuint (gimp_filter_layer_get_generation (filter), ==, generation);
  g_assert_cmpuint (gimp_filter_layer_get_cache_generation (filter), ==, generation);
  g_assert_true (cache != gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)));
  /* The old completed buffer remains intact after the atomic publication. */
  {
    guchar old[4];
    gegl_buffer_get (cache,GEGL_RECTANGLE (0,0,1,1),1,babl_format ("R'G'B'A u8"),
                     old,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
    g_assert_cmpmem (old,4,((const guchar[]) {17,29,53,255}),4);
  }
}

static void
blinds_lifecycle_constant (GimpFilterLayer *filter, const guchar expected[4])
{
  const guint width = gimp_item_get_width (GIMP_ITEM (filter));
  const guint height = gimp_item_get_height (GIMP_ITEM (filter));
  guchar *actual = g_malloc ((gsize) width*height*4);
  gegl_buffer_get (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)),NULL,1,
                   babl_format ("R'G'B'A u8"),actual,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
  for (gsize i = 0; i < (gsize) width*height; ++i)
    g_assert_cmpmem (actual+i*4,4,expected,4);
  g_free (actual);
}

static void
blinds_lifecycle_no_evaluation_work (GimpFilterLayer *filter)
{
  const guint64 generation = gimp_filter_layer_get_generation (filter);
  const guint64 runs = gimp_filter_layer_get_run_count (filter);
  const gint64 quantum = gimp_filter_layer_get_max_quantum_us (filter);
  const GimpFilterLayerState state = gimp_filter_layer_get_state (filter);
  GeglNode *node = gimp_drawable_get_source_node (GIMP_DRAWABLE (filter));
  guchar actual[4];
  for (guint i = 0; i < 4; ++i)
    {
      gegl_node_blit (node,1,GEGL_RECTANGLE (0,0,1,1),babl_format ("R'G'B'A u8"),
                      actual,GEGL_AUTO_ROWSTRIDE,GEGL_BLIT_DEFAULT);
      g_assert_cmpmem (actual,4,((const guchar[]) {17,29,53,255}),4);
    }
  g_assert_cmpuint (gimp_filter_layer_get_generation (filter), ==, generation);
  g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, runs);
  g_assert_cmpint (gimp_filter_layer_get_max_quantum_us (filter), ==, quantum);
  g_assert_cmpint (gimp_filter_layer_get_state (filter), ==, state);
}

/* The 4096-square unknown global mask cannot finish in the approximately ten
 * paced owner quanta dispatched by this 20ms loop, even at the 32768-pixel cap.
 * This is a capture-overlap fixture, not a responsiveness threshold. */
static void
blinds_lifecycle_capture (GimpImage *image, GimpFilterLayer *filter,
                          gboolean final_capture)
{
  if (final_capture) blinds_wait (filter,GIMP_FILTER_LAYER_RUNNING);
  blinds_fixture_selection (image,1,1,0,0,NULL,TRUE);
  g_assert_false (gimp_image_get_mask (image)->bounds_known);
  blinds_wait (filter,final_capture ? GIMP_FILTER_LAYER_IMPORTING : GIMP_FILTER_LAYER_PREPARING);
  spin_ms (20);
  g_assert_cmpint (gimp_filter_layer_get_state (filter), ==,
                   final_capture ? GIMP_FILTER_LAYER_IMPORTING : GIMP_FILTER_LAYER_PREPARING);
}

static void
blinds_capture_context_edits (void)
{
  GimpContext *context = gimp_get_user_context (gimp);
  GeglColor *saved = gegl_color_duplicate (gimp_context_get_background (context));
  for (guint final_capture = 0; final_capture < 2; ++final_capture)
    {
      GimpImage *image = image_new (4096,4096);
      GimpLayer *source = source_new (image,NULL,257,129);
      GimpFilterLayer *filter = filter_new (image,NULL,257,129);
      GeglBuffer *cache;
      guint64 cache_generation, generation;
      guchar far[4];
      fill (source,31,121,217,255); blinds_fixture_background (17,43,79);
      define_blinds (filter,90,1,1,0);
      fill (GIMP_LAYER (filter),17,29,53,255); gimp_filter_layer_mark_as_loaded (filter);
      cache = g_object_ref (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)));
      cache_generation = gimp_filter_layer_get_cache_generation (filter);
      gimp_filter_layer_invalidate (filter);
      generation = gimp_filter_layer_get_generation (filter);
      blinds_lifecycle_capture (image,filter,final_capture);
      blinds_lifecycle_cached (filter,cache,cache_generation);
      blinds_lifecycle_no_evaluation_work (filter);
      gimp_channel_select_rectangle (gimp_image_get_mask (image),0,0,31,129,
                                     GIMP_CHANNEL_OP_REPLACE,FALSE,0,0,FALSE);
      blinds_fixture_background (201,61,99);
      gimp_image_set_component_active (image,GIMP_CHANNEL_RED,FALSE);
      gimp_image_set_component_active (image,GIMP_CHANNEL_BLUE,FALSE);
      gimp_image_set_component_active (image,GIMP_CHANNEL_ALPHA,FALSE);
      gimp_layer_set_lock_alpha (GIMP_LAYER (filter),TRUE,FALSE);
      g_assert_cmpuint (gimp_filter_layer_get_generation (filter), ==, generation);
      blinds_lifecycle_settle (filter,cache,cache_generation,generation);
      /* Background belongs to execution start; final selection/components
       * belong to the retried final capture. Outside that selection is input. */
      pixel (GIMP_LAYER (filter),31,final_capture ? 43 : 61,217,255);
      gegl_buffer_get (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)),
                       GEGL_RECTANGLE (256,128,1,1),1,babl_format ("R'G'B'A u8"),
                       far,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
      g_assert_cmpmem (far,4,((const guchar[]) {31,121,217,255}),4);
      g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 1);
      g_object_unref (cache); g_object_unref (image);
    }
  gimp_context_set_background (context,saved); g_object_unref (saved);
}

static void
blinds_sealed_context_chunked_import (void)
{
  GimpImage *image = image_new (513,257);
  GimpLayer *source = source_new (image,NULL,513,257);
  GimpFilterLayer *filter = filter_new (image,NULL,513,257);
  GimpContext *context = gimp_get_user_context (gimp);
  GeglColor *saved = gegl_color_duplicate (gimp_context_get_background (context));
  GeglBuffer *cache;
  guint64 cache_generation, generation;
  guchar edges[257*4];
  fill (source,31,121,217,128); blinds_fixture_background (211,71,91);
  define_blinds (filter,90,1,1,0);
  fill (GIMP_LAYER (filter),17,29,53,255); gimp_filter_layer_mark_as_loaded (filter);
  cache = g_object_ref (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)));
  cache_generation = gimp_filter_layer_get_cache_generation (filter);
  gimp_filter_layer_invalidate (filter); generation = gimp_filter_layer_get_generation (filter);
  g_assert_true (gimp_image_get_mask (image)->bounds_known);
  g_assert_true (gimp_image_get_mask (image)->empty);
  blinds_wait (filter,GIMP_FILTER_LAYER_IMPORTING);
  /* With unchanged known-empty selection, before_import seals immediately in
   * the transition quantum. The scheduler yields before importing any bytes.
   * This public-state limiting case proves the boundary without a test hook;
   * 513*257 pixels necessarily require more than one bounded import chunk. */
  blinds_lifecycle_cached (filter,cache,cache_generation);
  gimp_channel_select_rectangle (gimp_image_get_mask (image),0,0,1,257,
                                 GIMP_CHANNEL_OP_REPLACE,FALSE,0,0,FALSE);
  gimp_image_set_component_active (image,GIMP_CHANNEL_RED,FALSE);
  gimp_image_set_component_active (image,GIMP_CHANNEL_GREEN,FALSE);
  gimp_image_set_component_active (image,GIMP_CHANNEL_BLUE,FALSE);
  gimp_image_set_component_active (image,GIMP_CHANNEL_ALPHA,FALSE);
  gimp_layer_set_lock_alpha (GIMP_LAYER (filter),TRUE,FALSE);
  blinds_fixture_background (3,5,7);
  blinds_lifecycle_settle (filter,cache,cache_generation,generation);
  for (guint edge = 0; edge < 2; ++edge)
    {
      gegl_buffer_get (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)),
                       GEGL_RECTANGLE (edge ? 512 : 0,0,1,257),1,babl_format ("R'G'B'A u8"),
                       edges,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
      for (guint y = 0; y < 257; ++y)
        g_assert_cmpmem (edges+y*4,4,((const guchar[]) {211,71,91,255}),4);
    }
  spin_ms (15);
  g_assert_cmpuint (gimp_filter_layer_get_generation (filter), ==, generation);
  g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 1);
  /* A later ordinary lower-input edit observes the new component context. */
  fill (source,79,137,193,255); settle (filter);
  blinds_lifecycle_constant (filter,(const guchar[]) {79,137,193,255});
  g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 2);
  gimp_context_set_background (context,saved); g_object_unref (saved);
  g_object_unref (cache); g_object_unref (image);
}

static void
blinds_context_oversize_retains_cache (void)
{
  for (guint gray = 0; gray < 2; ++gray)
    {
      const guint side = gray ? 17000 : 13000;
      const Babl *format = babl_format (gray ? "Y'A u8" : "R'G'B'A u8");
      const guchar cached[] = {17,29,53,123};
      guchar actual[4];
      GimpImage *image = gimp_image_new (gimp,side,side,gray ? GIMP_GRAY : GIMP_RGB,GIMP_PRECISION_U8_NON_LINEAR);
      GimpFilterLayer *filter = GIMP_FILTER_LAYER (gimp_filter_layer_new (image,side,side,"Oversize context",1,GIMP_LAYER_MODE_NORMAL_LEGACY));
      GeglBuffer *cache;
      GimpFilterLayerSnapshot snapshot;
      GBytes *definition;
      gchar *error;
      /* Sparse, otherwise empty layers make this an admission test. No giant
       * input raster is read or allocated merely to establish rejection. */
      gimp_image_add_layer (image,GIMP_LAYER (filter),NULL,0,FALSE);
      define_blinds (filter,0,1,1,0);
      cache = gimp_drawable_get_buffer (GIMP_DRAWABLE (filter));
      gegl_buffer_set (cache,GEGL_RECTANGLE (0,0,1,1),0,format,cached,GEGL_AUTO_ROWSTRIDE);
      gimp_filter_layer_mark_as_loaded (filter);
      gimp_filter_layer_invalidate (filter);
      blinds_wait (filter,GIMP_FILTER_LAYER_FAILED);
      error = gimp_filter_layer_dup_error (filter);
      g_assert_nonnull (strstr (error,"admission memory limit")); g_free (error);
      g_assert_true (cache == gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)));
      gegl_buffer_get (cache,GEGL_RECTANGLE (0,0,1,1),1,format,actual,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
      g_assert_cmpmem (actual,gray ? 2 : 4,cached,gray ? 2 : 4);
      g_assert_true (gimp_filter_layer_get_snapshot_state (filter,&snapshot));
      g_assert_true (snapshot.cache_complete);
      g_assert_cmpuint (snapshot.cache_generation, <, snapshot.generation);
      definition = gimp_filter_layer_ref_definition (filter);
      g_assert_cmpmem (g_bytes_get_data (definition,NULL),g_bytes_get_size (definition),"blinds-definition-preserved",27);
      g_bytes_unref (definition);
      spin_ms (15);
      g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 0);
      g_assert_cmpint (gimp_filter_layer_get_state (filter), ==, GIMP_FILTER_LAYER_FAILED);
      g_test_message ("blinds-owner-admission format=%s width=%u height=%u terminal=FAILED cache_definition=retained starts=0",
                      gray ? "GrayA" : "RGBA",side,side);
      g_object_unref (image);
    }
}

static void
blinds_capture_replacement_cancel_close (void)
{
  for (guint final_capture = 0; final_capture < 2; ++final_capture)
    for (guint action = 0; action < 3; ++action)
      {
        GimpImage *image = image_new (4096,4096);
        GimpLayer *source = source_new (image,NULL,64,64);
        GimpFilterLayer *filter = filter_new (image,NULL,64,64);
        GeglBuffer *cache;
        guint64 cache_generation, generation;
        gint finalized = 0;
        gint64 started, cancel_us = 0, close_us;
        fill (source,31,121,217,255); define_blinds (filter,0,1,1,0);
        fill (GIMP_LAYER (filter),17,29,53,255); gimp_filter_layer_mark_as_loaded (filter);
        cache = g_object_ref (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)));
        cache_generation = gimp_filter_layer_get_cache_generation (filter);
        gimp_filter_layer_invalidate (filter); generation = gimp_filter_layer_get_generation (filter);
        blinds_lifecycle_capture (image,filter,final_capture);
        blinds_lifecycle_cached (filter,cache,cache_generation);
        if (action == 0)
          {
            fill (source,207,87,37,255);
            g_assert_cmpuint (gimp_filter_layer_get_generation (filter), >, generation);
            generation = gimp_filter_layer_get_generation (filter);
            gimp_channel_clear (gimp_image_get_mask (image),NULL,FALSE);
            blinds_lifecycle_settle (filter,cache,cache_generation,generation);
            blinds_lifecycle_constant (filter,(const guchar[]) {207,87,37,255});
            g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, final_capture+1);
          }
        else if (action == 1)
          {
            const guint64 runs = gimp_filter_layer_get_run_count (filter);
            started = g_get_monotonic_time (); gimp_filter_layer_cancel (filter);
            cancel_us = g_get_monotonic_time ()-started;
            spin_ms (20);
            g_assert_cmpint (gimp_filter_layer_get_state (filter), ==, GIMP_FILTER_LAYER_FAILED);
            g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, runs);
            blinds_lifecycle_cached (filter,cache,cache_generation);
          }
        g_object_weak_ref (G_OBJECT (image),weak_finalized,&finalized);
        g_object_weak_ref (G_OBJECT (source),weak_finalized,&finalized);
        g_object_weak_ref (G_OBJECT (filter),weak_finalized,&finalized);
        started = g_get_monotonic_time (); g_object_unref (image);
        close_us = g_get_monotonic_time ()-started;
        g_assert_cmpint (finalized, ==, 3);
        g_test_message ("blinds-capture phase=%s action=%s cancel_call_us=%" G_GINT64_FORMAT
                        " owner_final_cleanup_us=%" G_GINT64_FORMAT,
                        final_capture ? "final" : "start",
                        action == 0 ? "replace" : action == 1 ? "cancel" : "close",cancel_us,close_us);
        /* Weak finalization proves owner release. This C test does not expose
         * FilterLifetime and does not claim to time the detached-worker drain. */
        g_object_unref (cache);
      }
  blinds_context_oversize_retains_cache ();
}

static void
blinds_unknown_mask_latency (void)
{
  GimpImage *image = image_new (4096,4096);
  GimpLayer *source = source_new (image,NULL,64,64);
  GimpFilterLayer *filter = filter_new (image,NULL,64,64);
  GeglBuffer *cache;
  HeartbeatLatency samples = {0,g_array_new (FALSE,FALSE,sizeof (gint64))};
  guint64 cache_generation, generation;
  guint timer;
  gint finalized = 0;
  gint64 started, wall_us, max_quantum_us, cancel_started, cancel_us, close_started, close_us;
  fill (source,31,121,217,255); define_blinds (filter,0,1,1,0);
  fill (GIMP_LAYER (filter),17,29,53,255); gimp_filter_layer_mark_as_loaded (filter);
  cache = g_object_ref (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)));
  cache_generation = gimp_filter_layer_get_cache_generation (filter);
  blinds_fixture_selection (image,1,1,0,0,NULL,TRUE);
  g_assert_false (gimp_image_get_mask (image)->bounds_known);
  started = samples.previous = g_get_monotonic_time ();
  timer = g_timeout_add (2,sample_heartbeat_latency,&samples);
  gimp_filter_layer_invalidate (filter); generation = gimp_filter_layer_get_generation (filter);
  blinds_lifecycle_settle (filter,cache,cache_generation,generation);
  wall_us = g_get_monotonic_time ()-started;
  max_quantum_us = gimp_filter_layer_get_max_quantum_us (filter);
  g_source_remove (timer);
  g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 1);
  blinds_lifecycle_constant (filter,(const guchar[]) {31,121,217,255});
  g_assert_cmpuint (samples.intervals->len, >, 0);
  g_array_sort (samples.intervals,compare_latency);
  g_test_message ("blinds-unknown-mask image=4096x4096 target=64x64 wall_us=%" G_GINT64_FORMAT
                  " max_owner_quantum_us=%" G_GINT64_FORMAT " heartbeat_period_us=2000 heartbeat_samples=%u"
                  " heartbeat_p50_us=%" G_GINT64_FORMAT " heartbeat_p95_us=%" G_GINT64_FORMAT
                  " heartbeat_p99_us=%" G_GINT64_FORMAT " heartbeat_max_us=%" G_GINT64_FORMAT,
                  wall_us,max_quantum_us,samples.intervals->len,
                  g_array_index (samples.intervals,gint64,samples.intervals->len/2),
                  g_array_index (samples.intervals,gint64,samples.intervals->len*95/100),
                  g_array_index (samples.intervals,gint64,samples.intervals->len*99/100),
                  g_array_index (samples.intervals,gint64,samples.intervals->len-1));
  /* A second unknown-mask capture provides an in-flight cancel measurement. */
  gimp_filter_layer_invalidate (filter); blinds_lifecycle_capture (image,filter,FALSE);
  cancel_started = g_get_monotonic_time (); gimp_filter_layer_cancel (filter);
  cancel_us = g_get_monotonic_time ()-cancel_started;
  g_assert_cmpint (gimp_filter_layer_get_state (filter), ==, GIMP_FILTER_LAYER_FAILED);
  blinds_lifecycle_constant (filter,(const guchar[]) {31,121,217,255});
  g_object_weak_ref (G_OBJECT (image),weak_finalized,&finalized);
  g_object_weak_ref (G_OBJECT (source),weak_finalized,&finalized);
  g_object_weak_ref (G_OBJECT (filter),weak_finalized,&finalized);
  close_started = g_get_monotonic_time (); g_object_unref (image);
  close_us = g_get_monotonic_time ()-close_started;
  g_assert_cmpint (finalized, ==, 3);
  g_test_message ("blinds-unknown-mask cancel_call_us=%" G_GINT64_FORMAT
                  " owner_final_cleanup_us=%" G_GINT64_FORMAT
                  " detached_worker_drain=not_observed",cancel_us,close_us);
  /* Measurements are diagnostics, not a global responsiveness pass. Pixel
   * budgets do not establish a wall-clock bound; >100ms outliers remain open. */
  g_object_unref (cache); g_array_unref (samples.intervals);
}
