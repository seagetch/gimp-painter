/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Native live route, separate from the pinned old-PDB byte oracle. */
#include "core/gimpchannel.h"
#include "core/gimpchannel-select.h"
#include "core/gimpcontext.h"
static void
define_blinds (GimpFilterLayer *filter, gint angle, gint segments,
               gint orientation, gint transparent)
{
  GimpValueArray *args = gimp_value_array_new_from_types (NULL,
    G_TYPE_INT, 1, G_TYPE_INT, 123, G_TYPE_INT, 456, G_TYPE_INT, angle,
    G_TYPE_INT, segments, G_TYPE_INT, orientation, G_TYPE_INT, transparent, G_TYPE_NONE);
  GBytes *raw = g_bytes_new_static ("blinds-definition-preserved", 27);
  GError *error = NULL;
  g_assert_true (gimp_filter_layer_set_definition (filter, "plug-in-blinds", raw, args, &error));
  g_assert_no_error (error); g_bytes_unref (raw); gimp_value_array_unref (args);
}
static void
blinds_wait (GimpFilterLayer *filter, GimpFilterLayerState expected)
{
  gint64 deadline = g_get_monotonic_time () + 20 * G_TIME_SPAN_SECOND;
  while (gimp_filter_layer_get_state (filter) != expected && g_get_monotonic_time () < deadline)
    {
      if (gimp_filter_layer_get_state (filter) == GIMP_FILTER_LAYER_FAILED && expected != GIMP_FILTER_LAYER_FAILED)
        { gchar *error = gimp_filter_layer_dup_error (filter); g_error ("Blinds failed waiting for state %d: %s", expected, error); }
      g_main_context_iteration (NULL, FALSE); g_usleep (100);
    }
  g_assert_cmpint (gimp_filter_layer_get_state (filter), ==, expected);
}
static void
blinds_identity_and_update (void)
{
  for (guint gray = 0; gray < 2; ++gray)
    for (guint orientation = 0; orientation < 2; ++orientation)
      for (guint transparent = 0; transparent < 2; ++transparent)
        {
          GimpImage *image = gimp_image_new (gimp, 36, 18, gray ? GIMP_GRAY : GIMP_RGB, GIMP_PRECISION_U8_NON_LINEAR);
          const Babl *format = babl_format (gray ? "Y'A u8" : "R'G'B'A u8");
          GimpLayer *source = gimp_layer_new (image,36,18,format,"Blinds source",1,GIMP_LAYER_MODE_NORMAL_LEGACY);
          GimpFilterLayer *filter;
          guchar input[36*18*4], actual[36*18*4];
          guint channels = gray ? 2 : 4;
          gimp_image_add_layer (image,source,NULL,0,FALSE);
          for (guint i = 0; i < 36*18*channels; i += channels)
            { for (guint c = 0; c + 1 < channels; ++c) input[i+c] = (i*17+c*71)&255; input[i+channels-1] = 255; }
          gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (source)),NULL,0,format,input,GEGL_AUTO_ROWSTRIDE);
          filter = GIMP_FILTER_LAYER (gimp_filter_layer_new (image,36,18,"Blinds",1,GIMP_LAYER_MODE_NORMAL_LEGACY));
          gimp_image_add_layer (image,GIMP_LAYER (filter),NULL,0,FALSE);
          define_blinds (filter,0,3,orientation,transparent ? -1 : 0); settle (filter);
          gegl_buffer_get (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)),NULL,1,format,actual,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
          g_assert_cmpmem (actual,36*18*channels,input,36*18*channels);
          input[0] ^= 255;
          gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (source)),NULL,0,format,input,GEGL_AUTO_ROWSTRIDE);
          gimp_drawable_update (GIMP_DRAWABLE (source),0,0,36,18); settle (filter);
          gegl_buffer_get (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)),NULL,1,format,actual,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
          g_assert_cmpmem (actual,36*18*channels,input,36*18*channels);
          g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 2);
          g_object_unref (image);
        }
}
static void
blinds_context_idle (void)
{
  GimpImage *image = image_new (37,19);
  GimpLayer *source = source_new (image,NULL,37,19);
  GimpFilterLayer *filter = filter_new (image,NULL,37,19);
  GimpContext *context = gimp_get_user_context (gimp);
  GeglColor *saved = gegl_color_duplicate (gimp_context_get_background (context));
  GeglColor *changed = gegl_color_new ("red");
  guint64 runs;
  fill (source,31,121,217,255); define_blinds (filter,0,3,0,0); settle (filter);
  runs = gimp_filter_layer_get_run_count (filter);
  gimp_context_set_background (context,changed); spin_ms (40);
  g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, runs);
  gimp_channel_select_rectangle (gimp_image_get_mask (image),0,0,3,3,GIMP_CHANNEL_OP_REPLACE,FALSE,0,0,FALSE); spin_ms (40);
  g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, runs);
  gimp_image_set_component_active (image,GIMP_CHANNEL_RED,FALSE); spin_ms (40);
  g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, runs);
  gimp_layer_set_lock_alpha (GIMP_LAYER (filter),TRUE,FALSE); spin_ms (40);
  g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, runs);
  pixel (GIMP_LAYER (filter),31,121,217,255);
  gimp_context_set_background (context,saved); g_object_unref (saved); g_object_unref (changed);
  g_object_unref (image);
}
static void
blinds_final_context_merges (void)
{
  for (guint kind = 0; kind < 3; ++kind)
    {
      GimpImage *image = image_new (128,128);
      GimpLayer *source = source_new (image,NULL,128,128);
      GimpFilterLayer *filter = filter_new (image,NULL,128,128);
      fill (source,31,121,217,255); define_blinds (filter,0,7,1,0);
      fill (GIMP_LAYER (filter),17,29,53,123); gimp_filter_layer_mark_as_loaded (filter);
      gimp_filter_layer_invalidate (filter); blinds_wait (filter,GIMP_FILTER_LAYER_RUNNING);
      if (kind == 0) gimp_channel_select_rectangle (gimp_image_get_mask (image),0,0,7,9,GIMP_CHANNEL_OP_REPLACE,FALSE,0,0,FALSE);
      else if (kind == 1) gimp_image_set_component_active (image,GIMP_CHANNEL_RED,FALSE);
      else gimp_layer_set_lock_alpha (GIMP_LAYER (filter),TRUE,FALSE);
      settle (filter); pixel (GIMP_LAYER (filter),31,121,217,255);
      g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 1);
      g_object_unref (image);
    }
}
static void
blinds_unknown_selection_scans_in_quanta (void)
{
  GimpImage *image = image_new (37,19);
  GimpLayer *source = source_new (image,NULL,37,19);
  GimpFilterLayer *filter = filter_new (image,NULL,37,19);
  GimpChannel *mask = gimp_image_get_mask (image);
  fill (source,31,121,217,255); define_blinds (filter,0,3,0,0);
  mask->bounds_known = FALSE;
  settle (filter);
  /* The completion image flush may populate native bounds independently. */
  pixel (GIMP_LAYER (filter),31,121,217,255);
  g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 1);
  g_object_unref (image);
}
static void
blinds_replace_running_definition (void)
{
  for (guint explicit_cancel = 0; explicit_cancel < 2; ++explicit_cancel)
    {
      GimpImage *image = image_new (257,139);
      GimpLayer *source = source_new (image,NULL,257,139);
      GimpFilterLayer *filter = filter_new (image,NULL,257,139);
      fill (source,31,121,217,255); define_blinds (filter,90,1,1,0);
      blinds_wait (filter,GIMP_FILTER_LAYER_RUNNING);
      if (explicit_cancel) gimp_filter_layer_cancel (filter);
      define_blinds (filter,0,3,0,0); settle (filter);
      pixel (GIMP_LAYER (filter),31,121,217,255);
      g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 2);
      g_object_unref (image);
    }
}
static void
blinds_background_samples_execution_start (void)
{
  GimpImage *image = image_new (256,256);
  GimpLayer *source = source_new (image,NULL,256,256);
  GimpFilterLayer *filter = filter_new (image,NULL,256,256);
  GimpContext *context = gimp_get_user_context (gimp);
  GeglColor *saved = gegl_color_duplicate (gimp_context_get_background (context));
  GeglColor *red = gegl_color_new ("red"), *blue = gegl_color_new ("blue");
  fill (source,31,121,217,255); define_blinds (filter,90,1,1,0);
  blinds_wait (filter,GIMP_FILTER_LAYER_PREPARING); gimp_context_set_background (context,red);
  blinds_wait (filter,GIMP_FILTER_LAYER_RUNNING); gimp_context_set_background (context,blue);
  settle (filter); pixel (GIMP_LAYER (filter),255,0,0,255);
  g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 1);
  gimp_context_set_background (context,saved); g_object_unref (saved); g_object_unref (red); g_object_unref (blue);
  g_object_unref (image);
}
static void
blinds_disabled_swap_preserves_cache (void)
{
  GimpImage *image = image_new (37,19);
  GimpLayer *source = source_new (image,NULL,37,19);
  GimpFilterLayer *filter = filter_new (image,NULL,37,19);
  gchar *saved = NULL;
  g_object_get (gegl_config (),"swap",&saved,NULL);
  fill (source,31,121,217,255); define_blinds (filter,0,3,0,0);
  fill (GIMP_LAYER (filter),17,29,53,123); gimp_filter_layer_mark_as_loaded (filter);
  g_object_set (gegl_config (),"swap",NULL,NULL);
  gimp_filter_layer_invalidate (filter); blinds_wait (filter,GIMP_FILTER_LAYER_FAILED);
  pixel (GIMP_LAYER (filter),17,29,53,123);
  g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 0);
  g_object_set (gegl_config (),"swap",saved,NULL); g_free (saved); g_object_unref (image);
}

#include "core/gimpfilterexit.h"
static gboolean
blinds_cancel_gui_exit (Gimp *application, gboolean force, gpointer data)
{ ++*(guint *) data; return TRUE; }
static gboolean
blinds_after_gui_exit (Gimp *application, gboolean force, GApplication *app)
{ gimp_filter_exit_quit (app); return FALSE; }
static void
blinds_cancelled_gui_quit_keeps_worker (void)
{
  GimpImage *image = image_new (128,128);
  GimpLayer *source = source_new (image,NULL,128,128);
  GimpFilterLayer *filter = filter_new (image,NULL,128,128);
  GApplication *app = g_application_new (NULL,G_APPLICATION_NON_UNIQUE);
  guint cancelled = 0;
  gulong before, after;
  gimp_filter_exit_init (app);
  before = g_signal_connect (gimp,"exit",G_CALLBACK (blinds_cancel_gui_exit),&cancelled);
  after = g_signal_connect_after (gimp,"exit",G_CALLBACK (blinds_after_gui_exit),app);
  fill (source,31,121,217,255); define_blinds (filter,0,1,0,0);
  blinds_wait (filter,GIMP_FILTER_LAYER_RUNNING);
  gimp_exit (gimp,FALSE);
  g_assert_cmpuint (cancelled, ==, 1);
  g_assert_false (gimp_filter_exit_is_requested (app));
  settle (filter); pixel (GIMP_LAYER (filter),31,121,217,255);
  g_signal_handler_disconnect (gimp,before); g_signal_handler_disconnect (gimp,after);
  g_object_unref (app); g_object_unref (image);
}

#include "core/gimp-batch.h"
#include "pdb/gimppdb.h"
#include "pdb/gimpprocedure.h"
#include "plug-in/plug-in-types.h"
#include "plug-in/gimppluginmanager.h"
#include "plug-in/gimppluginprocedure.h"
static GimpPDBStatusType blinds_batch_status;
static guint blinds_batch_calls;
static GimpValueArray *
blinds_batch_result (GimpProcedure *procedure, Gimp *application,
                     GimpContext *context, GimpProgress *progress,
                     const GimpValueArray *args, GError **error)
{
  ++blinds_batch_calls;
  return gimp_value_array_new_from_types (NULL,
    GIMP_TYPE_PDB_STATUS_TYPE, blinds_batch_status, G_TYPE_NONE);
}
static void
blinds_nonquit_batch_statuses (void)
{
  const GimpPDBStatusType statuses[] = { GIMP_PDB_SUCCESS, GIMP_PDB_CALLING_ERROR,
    GIMP_PDB_EXECUTION_ERROR, GIMP_PDB_CANCEL, GIMP_PDB_PASS_THROUGH };
  const gint expected[] = { 0, 64, 70, 130, 1 };
  const gchar *commands[] = { "first", "second", NULL };
  GFile *placeholder = g_file_new_for_path ("/nonexistent/painter-test-batch-registration-only");
  GimpProcedure *registration = gimp_plug_in_procedure_new (GIMP_PDB_PROC_TYPE_PLUGIN,placeholder);
  GimpProcedure *procedure = gimp_procedure_new (blinds_batch_result,TRUE);
  gimp_object_set_static_name (GIMP_OBJECT (procedure),"painter-test-batch-status");
  gimp_object_set_static_name (GIMP_OBJECT (registration),"painter-test-batch-status");
  gimp_plug_in_procedure_set_batch_interpreter (GIMP_PLUG_IN_PROCEDURE (registration),"Native status fixture");
  gimp_procedure_add_argument (procedure,g_param_spec_string ("command","command","command","",GIMP_PARAM_READWRITE));
  gimp_pdb_register_procedure (gimp->pdb,procedure);
  /* A real typed registration makes batch mode available; this file is never
   * opened or executed. The requested interpreter is the private native stub. */
  gimp_plug_in_manager_add_batch_procedure (gimp->plug_in_manager,GIMP_PLUG_IN_PROCEDURE (registration));
  for (guint i = 0; i < G_N_ELEMENTS (statuses); ++i)
    {
      blinds_batch_status = statuses[i]; blinds_batch_calls = 0;
      g_assert_cmpint (gimp_batch_run (gimp,"painter-test-batch-status",commands), ==, expected[i]);
      g_assert_cmpuint (blinds_batch_calls, ==, i ? 1 : 2);
    }
  g_assert_cmpint (gimp_batch_run (gimp,"painter-test-missing-interpreter",commands), ==, 69);
  gimp->plug_in_manager->batch_procs = g_slist_remove (gimp->plug_in_manager->batch_procs,registration);
  gimp_pdb_unregister_procedure (gimp->pdb,procedure);
  g_object_unref (procedure); g_object_unref (registration); g_object_unref (placeholder);
}
