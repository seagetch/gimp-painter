/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Included after the native FilterLayer fixture helpers. */
static void
explicit_cancel_preserves_completed_cache (void)
{
  const GimpFilterLayerState phases[] = {
    GIMP_FILTER_LAYER_WAITING, GIMP_FILTER_LAYER_PREPARING,
    GIMP_FILTER_LAYER_RUNNING, GIMP_FILTER_LAYER_IMPORTING
  };
  for (guint phase = 0; phase < G_N_ELEMENTS (phases); ++phase)
    {
      GimpImage *image = image_new (768, 768);
      GimpLayer *source = source_new (image, NULL, 768, 768);
      GimpFilterLayer *filter = filter_new (image, NULL, 768, 768);
      GimpFilterLayerSnapshot before, after;
      GBytes *raw = g_bytes_new_static ("cancel-keeps-definition", 23);
      GimpValueArray *args = edge_args ();
      gint64 deadline;
      guint64 runs;
      gchar *procedure, *error;
      GBytes *saved_raw;
      fill (source, 42, 93, 177, 255);
      g_assert_true (gimp_filter_layer_set_definition (filter, "plug-in-edge", raw, args, NULL));
      gimp_value_array_unref (args);
      fill (GIMP_LAYER (filter), 17, 29, 53, 123);
      gimp_filter_layer_mark_as_loaded (filter);
      g_assert_true (gimp_filter_layer_get_snapshot_state (filter, &before));
      gimp_filter_layer_invalidate (filter);
      deadline = g_get_monotonic_time () + 20 * G_TIME_SPAN_SECOND;
      while (gimp_filter_layer_get_state (filter) != phases[phase] &&
             g_get_monotonic_time () < deadline)
        { g_main_context_iteration (NULL, FALSE); g_usleep (100); }
      g_assert_cmpint (gimp_filter_layer_get_state (filter), ==, phases[phase]);
      gimp_filter_layer_cancel (filter);
      g_assert_cmpint (gimp_filter_layer_get_state (filter), ==, GIMP_FILTER_LAYER_FAILED);
      g_assert_true (gimp_filter_layer_get_snapshot_state (filter, &after));
      g_assert_cmpuint (after.cache_generation, ==, before.cache_generation);
      g_assert_true (after.cache_complete);
      g_assert_cmpuint (after.generation, >, after.cache_generation);
      runs = gimp_filter_layer_get_run_count (filter);
      spin_ms (80);
      g_assert_cmpint (gimp_filter_layer_get_state (filter), ==, GIMP_FILTER_LAYER_FAILED);
      g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, runs);
      pixel (GIMP_LAYER (filter), 17, 29, 53, 123);
      procedure = gimp_filter_layer_dup_procedure (filter);
      g_assert_cmpstr (procedure, ==, "plug-in-edge"); g_free (procedure);
      saved_raw = gimp_filter_layer_ref_definition (filter);
      g_assert_true (g_bytes_equal (saved_raw, raw)); g_bytes_unref (saved_raw);
      error = gimp_filter_layer_dup_error (filter);
      g_assert_cmpstr (error, ==, "Filter execution cancelled"); g_free (error);
      fill (source, 100, 150, 200, 255);
      settle (filter);
      pixel (GIMP_LAYER (filter), 0, 0, 0, 255);
      g_bytes_unref (raw);
      g_object_unref (image);
    }
}

typedef struct { gboolean called; gboolean close; } CancelReentry;
static void
cancel_reentry (GimpFilterLayer *filter, gpointer data)
{
  CancelReentry *state = data;
  if (state->called || gimp_filter_layer_get_state (filter) != GIMP_FILTER_LAYER_FAILED)
    return;
  state->called = TRUE;
  if (state->close)
    g_assert_true (gimp_painter_binding_close (G_OBJECT (filter), NULL));
  else
    define_edge (filter);
}
static void
explicit_cancel_survives_callback_reentry (void)
{
  for (guint close = 0; close < 2; ++close)
    {
      GimpImage *image = image_new (8, 8);
      GimpLayer *source = source_new (image, NULL, 8, 8);
      GimpFilterLayer *filter = filter_new (image, NULL, 8, 8);
      CancelReentry state = { FALSE, close };
      fill (source, 50, 100, 150, 255);
      g_signal_connect (filter, "filter-state-changed", G_CALLBACK (cancel_reentry), &state);
      gimp_filter_layer_cancel (filter);
      g_assert_true (state.called);
      if (close)
        g_assert_cmpint (gimp_filter_layer_get_state (filter), ==, GIMP_FILTER_LAYER_CLOSED);
      else
        { settle (filter); pixel (GIMP_LAYER (filter), 0, 0, 0, 255); }
      g_signal_handlers_disconnect_by_data (filter, &state);
      g_object_unref (image);
    }
}
