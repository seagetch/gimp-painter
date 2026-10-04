/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Real native Retinex route and lifecycle, including sealed genuine-old live
 * scenes. Shared cancellation/context stress tests remain in their own files. */
#include <math.h>
#include <stdlib.h>
#include "core/gimpcontainer.h"
#include "xcf/xcf.h"

static GimpValueArray *
retinex_args (gint scale, gint nscales, gint mode, gdouble cvar)
{
  return gimp_value_array_new_from_types (NULL,
    G_TYPE_INT,1,G_TYPE_INT,123,G_TYPE_INT,456,G_TYPE_INT,scale,
    G_TYPE_INT,nscales,G_TYPE_INT,mode,G_TYPE_DOUBLE,cvar,G_TYPE_NONE);
}

static void
retinex_set_args (GimpFilterLayer *filter, GimpValueArray *args)
{
  const guchar saved[] = {0,1,0xfe,0,0x7f,0xff};
  GBytes *raw = g_bytes_new (saved,sizeof saved);
  GError *error = NULL;
  g_assert_true (gimp_filter_layer_set_definition (filter,"plug-in-retinex",raw,args,&error));
  g_assert_no_error (error); g_bytes_unref (raw);
}

static void
retinex_define (GimpFilterLayer *filter, gint scale, gint nscales, gint mode, gdouble cvar)
{
  GimpValueArray *args = retinex_args (scale,nscales,mode,cvar);
  retinex_set_args (filter,args); gimp_value_array_unref (args);
}

static void
retinex_saved_args (GimpFilterLayer *filter, GimpValueArray *expected)
{
  const guchar saved[] = {0,1,0xfe,0,0x7f,0xff};
  gchar *name = gimp_filter_layer_dup_procedure (filter);
  GBytes *raw = gimp_filter_layer_ref_definition (filter);
  GimpValueArray *args = gimp_filter_layer_dup_args (filter);
  g_assert_cmpstr (name, ==, "plug-in-retinex");
  g_assert_cmpmem (g_bytes_get_data (raw,NULL),g_bytes_get_size (raw),saved,sizeof saved);
  g_assert_cmpuint (gimp_value_array_length (args), ==, gimp_value_array_length (expected));
  for (guint i = 0; i < gimp_value_array_length (expected); ++i)
    {
      GValue *actual = gimp_value_array_index (args,i);
      GValue *wanted = gimp_value_array_index (expected,i);
      g_assert_cmpuint (G_VALUE_TYPE (actual), ==, G_VALUE_TYPE (wanted));
      if (G_VALUE_HOLDS_INT (wanted))
        g_assert_cmpint (g_value_get_int (actual), ==, g_value_get_int (wanted));
      else
        {
          const gdouble a = g_value_get_double (actual), b = g_value_get_double (wanted);
          g_assert_true (a == b || (isnan (a) && isnan (b)));
        }
    }
  g_free (name); g_bytes_unref (raw); gimp_value_array_unref (args);
}

static void
retinex_definition_preserved (GimpFilterLayer *filter, gint scale, gint nscales,
                              gint mode, gdouble cvar)
{
  GimpValueArray *args = retinex_args (scale,nscales,mode,cvar);
  retinex_saved_args (filter,args); gimp_value_array_unref (args);
}

static gchar *
retinex_fixture_directory (void)
{
  const gchar *root = g_getenv ("GIMP_PAINTER_RETINEX_FIXTURES");
  g_assert_nonnull (root);
  return g_build_filename (root,"live",NULL);
}

static guchar *
retinex_fixture (const gchar *directory, const gchar *id, const gchar *label, gsize expected)
{
  gchar *name = g_strdup_printf ("%s-%s.raw",id,label);
  gchar *path = g_build_filename (directory,name,NULL);
  gchar *bytes = NULL;
  gsize size = 0;
  GError *error = NULL;
  g_assert_true (g_file_get_contents (path,&bytes,&size,&error));
  g_assert_no_error (error); g_assert_cmpuint (size, ==, expected);
  g_free (path); g_free (name); return (guchar *) bytes;
}

static void
retinex_actual_old_live (void)
{
  gchar *directory = retinex_fixture_directory ();
  gchar *manifest = g_build_filename (directory,"cases.tsv",NULL);
  gchar *text = NULL, **lines;
  guint compared = 0;
  g_assert_true (g_file_get_contents (manifest,&text,NULL,NULL));
  lines = g_strsplit (text,"\n",-1);
  for (guint row = 0; lines[row]; ++row)
    {
      gchar **cols;
      guint width,height,selection;
      gint scale,nscales,mode;
      gdouble cvar;
      GimpImage *image;
      GimpLayer *source;
      GimpFilterLayer *filter;
      guchar *input,*actual;
      if (!*lines[row]) continue;
      cols = g_strsplit (lines[row],"\t",-1);
      g_assert_cmpuint (g_strv_length (cols), ==, 10);
      g_assert_cmpstr (cols[1], ==, "plug-in-retinex");
      g_assert_cmpint (atoi (cols[4]), ==, 4);
      width = atoi (cols[2]); height = atoi (cols[3]);
      scale = atoi (cols[6]); nscales = atoi (cols[7]); mode = atoi (cols[8]);
      cvar = g_ascii_strtod (cols[9],NULL);
      g_assert_nonnull (strstr (cols[0],"-s"));
      selection = atoi (strstr (cols[0],"-s")+2);
      g_assert_cmpuint (selection, <=, 2);
      image = image_new (width,height);
      source = source_new (image,NULL,width,height);
      input = retinex_fixture (directory,cols[0],"source",width*height*4);
      actual = g_malloc (width*height*4);
      gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (source)),NULL,0,
                       babl_format ("R'G'B'A u8"),input,GEGL_AUTO_ROWSTRIDE);
      if (selection)
        {
          GeglBuffer *coverage = gegl_buffer_new (GEGL_RECTANGLE (0,0,width,height),babl_format ("Y u8"));
          guchar *mask = retinex_fixture (directory,cols[0],"merge1-mask",width*height);
          gegl_buffer_set (coverage,NULL,0,babl_format ("Y u8"),mask,GEGL_AUTO_ROWSTRIDE);
          gimp_channel_select_buffer (gimp_image_get_mask (image),"Old Retinex selection",coverage,0,0,
                                      GIMP_CHANNEL_OP_REPLACE,FALSE,0,0);
          g_free (mask); g_object_unref (coverage);
        }
      filter = filter_new (image,NULL,width,height);
      retinex_define (filter,scale,nscales,mode,cvar);
      for (guint pass = 0; pass < 2; ++pass)
        {
          guchar *expected;
          if (pass) gimp_drawable_update (GIMP_DRAWABLE (source),0,0,width,height);
          settle (filter);
          expected = retinex_fixture (directory,cols[0],pass ? "rerun" : "settled",width*height*4);
          gegl_buffer_get (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)),NULL,1,
                           babl_format ("R'G'B'A u8"),actual,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
          g_test_message ("Actual old Retinex Filter scene %s pass%u",cols[0],pass+1);
          g_assert_cmpmem (actual,width*height*4,expected,width*height*4);
          g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, pass+1);
          g_free (expected); ++compared;
        }
      retinex_definition_preserved (filter,scale,nscales,mode,cvar);
      g_free (input); g_free (actual); g_object_unref (image); g_strfreev (cols);
    }
  g_assert_cmpuint (compared, ==, 12);
  g_strfreev (lines); g_free (text); g_free (manifest); g_free (directory);
}

static void
retinex_zero_scales_and_update (void)
{
  GimpImage *image = image_new (67,66);
  GimpLayer *source = source_new (image,NULL,67,66);
  GimpFilterLayer *filter = filter_new (image,NULL,67,66);
  fill (source,31,121,217,255);
  retinex_define (filter,256,0,2,0.123456789);
  settle (filter); pixel (GIMP_LAYER (filter),0,0,0,255);
  retinex_definition_preserved (filter,256,0,2,0.123456789);
  fill (source,13,71,137,113); settle (filter);
  pixel (GIMP_LAYER (filter),0,0,0,113);
  g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 2);
  retinex_definition_preserved (filter,256,0,2,0.123456789);
  g_object_unref (image);
}

static void
retinex_assert_failed_cache (GimpFilterLayer *filter)
{
  GeglBuffer *cache;
  guint64 generation;
  const gsize size = (gsize) gimp_item_get_width (GIMP_ITEM (filter)) *
                     gimp_item_get_height (GIMP_ITEM (filter)) * 4;
  guchar *before = g_malloc (size), *after = g_malloc (size);
  fill (GIMP_LAYER (filter),17,29,53,123); gimp_filter_layer_mark_as_loaded (filter);
  cache = g_object_ref (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)));
  generation = gimp_filter_layer_get_cache_generation (filter);
  gegl_buffer_get (cache,NULL,1,babl_format ("R'G'B'A u8"),
                   before,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
  gimp_filter_layer_invalidate (filter); blinds_wait (filter,GIMP_FILTER_LAYER_FAILED);
  spin_ms (20);
  g_assert_cmpint (gimp_filter_layer_get_state (filter), ==, GIMP_FILTER_LAYER_FAILED);
  g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 0);
  g_assert_cmpuint (gimp_filter_layer_get_cache_generation (filter), ==, generation);
  g_assert_true (cache == gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)));
  gegl_buffer_get (cache,NULL,1,babl_format ("R'G'B'A u8"),
                   after,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
  g_assert_cmpmem (before,size,after,size);
  g_free (before); g_free (after); g_object_unref (cache);
}

static void
retinex_invalid_domains_keep_cache (void)
{
  const struct { gint scale,nscales,mode; gdouble cvar; } invalid[] = {
    {15,3,0,1.2}, {257,3,0,1.2}, {16,-1,0,1.2}, {16,9,0,1.2},
    {16,3,-1,1.2}, {16,3,3,1.2}, {16,3,0,-0.01}, {16,3,0,4.01},
    {16,3,0,NAN}, {16,3,0,INFINITY}, {16,3,0,-INFINITY}
  };
  /* Every saved argument has an exact type, including stale run/image/drawable
   * integers; neither coercion nor argument-count repair is permitted. */
  for (guint kind = 0; kind < G_N_ELEMENTS (invalid)+9; ++kind)
    {
      GimpImage *image = image_new (67,66);
      GimpLayer *source = source_new (image,NULL,67,66);
      GimpFilterLayer *filter = filter_new (image,NULL,67,66);
      GimpValueArray *args;
      fill (source,31,121,217,255);
      if (kind < G_N_ELEMENTS (invalid))
        args = retinex_args (invalid[kind].scale,invalid[kind].nscales,
                             invalid[kind].mode,invalid[kind].cvar);
      else
        {
          const guint mutation = kind-G_N_ELEMENTS (invalid);
          args = retinex_args (16,3,0,1.2);
          if (mutation < 7)
            {
              GValue *value = gimp_value_array_index (args,mutation);
              g_value_unset (value);
              g_value_init (value,mutation == 6 ? G_TYPE_INT : G_TYPE_DOUBLE);
              if (mutation == 6) g_value_set_int (value,1);
              else g_value_set_double (value,1.0);
            }
          else if (mutation == 7) gimp_value_array_truncate (args,6);
          else
            {
              GValue extra = G_VALUE_INIT;
              g_value_init (&extra,G_TYPE_INT); g_value_set_int (&extra,1);
              gimp_value_array_append (args,&extra); g_value_unset (&extra);
            }
        }
      retinex_set_args (filter,args); retinex_assert_failed_cache (filter);
      retinex_saved_args (filter,args);
      gimp_value_array_unref (args); g_object_unref (image);
    }
}

static void
retinex_gray_keep_cache (void)
{
  GimpImage *image = gimp_image_new (gimp,67,66,GIMP_GRAY,GIMP_PRECISION_U8_NON_LINEAR);
  GimpLayer *source = gimp_layer_new (image,67,66,babl_format ("Y'A u8"),
                                     "Unsupported gray Retinex source",1,GIMP_LAYER_MODE_NORMAL_LEGACY);
  GimpFilterLayer *filter;
  gimp_image_add_layer (image,source,NULL,0,FALSE);
  fill (source,121,121,121,255); filter = filter_new (image,NULL,67,66);
  retinex_define (filter,16,3,0,1.2); retinex_assert_failed_cache (filter);
  retinex_definition_preserved (filter,16,3,0,1.2); g_object_unref (image);
}

static void
retinex_invalid_roi_keep_cache (void)
{
  for (guint kind = 0; kind < 5; ++kind)
    {
      const guint width = kind == 3 ? 15 : 53, height = kind == 4 ? 15 : 41;
      GimpImage *image = image_new (63,63);
      GimpLayer *source = source_new (image,NULL,width,height);
      GimpFilterLayer *filter = filter_new (image,NULL,width,height);
      fill (source,31,121,217,255); retinex_define (filter,16,3,0,1.2);
      if (kind < 3)
        gimp_channel_select_rectangle (gimp_image_get_mask (image),kind == 2 ? 62 : 2,
                                       kind == 2 ? 62 : 3,kind == 0 ? 15 : kind == 2 ? 1 : 16,
                                       kind == 1 ? 15 : kind == 2 ? 1 : 16,
                                       GIMP_CHANNEL_OP_REPLACE,FALSE,0,0,FALSE);
      /* The old Retinex size gate also rejects a nonintersecting selection;
       * it must not publish lower input as a successful no-merge result. */
      retinex_assert_failed_cache (filter);
      retinex_definition_preserved (filter,16,3,0,1.2);
      g_object_unref (image);
    }
}

static void
retinex_replace_running_definition (void)
{
  for (guint explicit_cancel = 0; explicit_cancel < 2; ++explicit_cancel)
    {
      GimpImage *image = image_new (257,139);
      GimpLayer *source = source_new (image,NULL,257,139);
      GimpFilterLayer *filter = filter_new (image,NULL,257,139);
      fill (source,31,121,217,255); retinex_define (filter,256,8,2,1.2);
      fill (GIMP_LAYER (filter),17,29,53,123); gimp_filter_layer_mark_as_loaded (filter);
      gimp_filter_layer_invalidate (filter); blinds_wait (filter,GIMP_FILTER_LAYER_RUNNING);
      if (explicit_cancel)
        {
          gimp_filter_layer_cancel (filter);
          g_assert_cmpint (gimp_filter_layer_get_state (filter), ==, GIMP_FILTER_LAYER_FAILED);
          pixel (GIMP_LAYER (filter),17,29,53,123);
          retinex_definition_preserved (filter,256,8,2,1.2);
        }
      retinex_define (filter,16,0,0,4.0); settle (filter);
      pixel (GIMP_LAYER (filter),0,0,0,255);
      retinex_definition_preserved (filter,16,0,0,4.0);
      g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 2);
      g_object_unref (image);
    }
}

static void
retinex_final_context (void)
{
  const guint width = 67, height = 66;
  GimpImage *image = image_new (width,height);
  GimpLayer *source = source_new (image,NULL,width,height);
  GimpFilterLayer *filter = filter_new (image,NULL,width,height);
  guchar input[67*66*4], actual[67*66*4];
  for (guint i = 0; i < width*height; ++i)
    { input[i*4] = (31+i*17)&255; input[i*4+1] = (121+i*31)&255;
      input[i*4+2] = (217+i*47)&255; input[i*4+3] = i%256; }
  gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (source)),NULL,0,
                   babl_format ("R'G'B'A u8"),input,GEGL_AUTO_ROWSTRIDE);
  retinex_define (filter,16,0,1,0.0); blinds_wait (filter,GIMP_FILTER_LAYER_RUNNING);
  /* Start saw the whole valid image. A narrower final selection is legal:
   * only execution-start bounds are subject to the Retinex size gate. */
  gimp_channel_select_rectangle (gimp_image_get_mask (image),3,2,11,9,
                                 GIMP_CHANNEL_OP_REPLACE,FALSE,0,0,FALSE);
  gimp_image_set_component_active (image,GIMP_CHANNEL_GREEN,FALSE);
  gimp_layer_set_lock_alpha (GIMP_LAYER (filter),TRUE,FALSE);
  settle (filter);
  gegl_buffer_get (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)),NULL,1,
                   babl_format ("R'G'B'A u8"),actual,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
  for (guint y = 0; y < height; ++y) for (guint x = 0; x < width; ++x)
    {
      const guint at = (y*width+x)*4;
      guchar expected[4]; memcpy (expected,input+at,4);
      if (x >= 3 && x < 14 && y >= 2 && y < 11 && input[at+3])
        expected[0] = expected[2] = 0;
      g_assert_cmpmem (actual+at,4,expected,4);
    }
  g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 1);
  retinex_definition_preserved (filter,16,0,1,0.0); g_object_unref (image);
}

static void
retinex_dependency_updates (void)
{
  GimpImage *image = image_new (67,66);
  GimpLayer *source = source_new (image,NULL,67,66);
  GimpFilterLayer *lower = filter_new (image,NULL,67,66);
  GimpFilterLayer *upper = filter_new (image,NULL,67,66);
  guint64 lower_runs,upper_runs;
  fill (source,31,121,217,255); retinex_define (lower,16,0,0,1.2); small_tiles_define (upper,1);
  settle (upper); pixel (GIMP_LAYER (upper),0,0,0,255);
  g_assert_cmpint (gimp_filter_layer_get_state (lower), ==, GIMP_FILTER_LAYER_CLEAN);
  lower_runs = gimp_filter_layer_get_run_count (lower); upper_runs = gimp_filter_layer_get_run_count (upper);
  fill (source,201,61,99,255); settle (upper); pixel (GIMP_LAYER (upper),0,0,0,255);
  g_assert_cmpint (gimp_filter_layer_get_state (lower), ==, GIMP_FILTER_LAYER_CLEAN);
  g_assert_cmpuint (gimp_filter_layer_get_run_count (lower), >, lower_runs);
  g_assert_cmpuint (gimp_filter_layer_get_run_count (upper), >, upper_runs);
  retinex_definition_preserved (lower,16,0,0,1.2); small_tiles_definition_preserved (upper,1);
  g_object_unref (image);
}

static void
retinex_assert_zero_scales (GimpFilterLayer *filter, const guchar *input, guint pixels)
{
  guchar *actual = g_malloc ((gsize) pixels*4);
  gegl_buffer_get (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)),NULL,1,
                   babl_format ("R'G'B'A u8"),actual,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
  for (guint i = 0; i < pixels; ++i)
    {
      guchar expected[4]; memcpy (expected,input+i*4,4);
      if (expected[3]) expected[0] = expected[1] = expected[2] = 0;
      g_assert_cmpmem (actual+i*4,4,expected,4);
    }
  g_free (actual);
}

static void
retinex_save_reopen (void)
{
  const guint width = 53, height = 41;
  gchar *directory = retinex_fixture_directory ();
  guchar *source_pixels = retinex_fixture (directory,"retinex-g0-s0-v0","source",width*height*4);
  guchar *expected = retinex_fixture (directory,"retinex-g0-s0-v0","settled",width*height*4);
  guchar actual[53*41*4];
  GimpImage *image = image_new (width,height), *copy;
  GimpLayer *source = source_new (image,NULL,width,height), *lower;
  GimpFilterLayer *filter = filter_new (image,NULL,width,height), *loaded;
  GOutputStream *output = g_memory_output_stream_new_resizable ();
  GFile *file = g_file_new_for_path ("/tmp/retinex-definition-roundtrip.xcf");
  GBytes *bytes,*definition;
  GInputStream *input;
  GimpValueArray *args;
  GimpContainer *layers;
  GError *error = NULL;
  guint64 runs,revision;
  gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (source)),NULL,0,
                   babl_format ("R'G'B'A u8"),source_pixels,GEGL_AUTO_ROWSTRIDE);
  retinex_define (filter,16,3,0,1.2); settle (filter);
  g_assert_true (xcf_save_stream (gimp,image,output,file,NULL,&error)); g_assert_no_error (error);
  g_assert_true (g_output_stream_close (output,NULL,&error)); g_assert_no_error (error);
  bytes = g_memory_output_stream_steal_as_bytes (G_MEMORY_OUTPUT_STREAM (output));
  input = g_memory_input_stream_new_from_bytes (bytes);
  copy = xcf_load_stream (gimp,input,file,NULL,&error); g_assert_no_error (error); g_assert_nonnull (copy);
  layers = gimp_image_get_layers (copy);
  loaded = GIMP_FILTER_LAYER (gimp_container_get_first_child (layers));
  lower = GIMP_LAYER (gimp_container_get_last_child (layers));
  g_assert_true (GIMP_IS_FILTER_LAYER (loaded)); g_assert_false (GIMP_IS_FILTER_LAYER (lower));
  retinex_definition_preserved (loaded,16,3,0,1.2);
  gegl_buffer_get (gimp_drawable_get_buffer (GIMP_DRAWABLE (loaded)),NULL,1,
                   babl_format ("R'G'B'A u8"),actual,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
  g_assert_cmpmem (actual,sizeof actual,expected,sizeof actual);

  /* Explicitly edit reopened typed arguments on nonuniform data. Black is an
   * independent zero-scale limit, with old hidden colors retained at alpha0. */
  runs = gimp_filter_layer_get_run_count (loaded);
  revision = gimp_filter_layer_get_definition_revision (loaded);
  definition = gimp_filter_layer_ref_definition (loaded); args = gimp_filter_layer_dup_args (loaded);
  g_value_set_int (gimp_value_array_index (args,4),0);
  g_assert_true (gimp_filter_layer_edit_definition (loaded,"plug-in-retinex",definition,args,&error));
  g_assert_no_error (error); g_bytes_unref (definition); gimp_value_array_unref (args);
  g_assert_cmpuint (gimp_filter_layer_get_definition_revision (loaded), ==, revision+1);
  settle (loaded); g_assert_cmpuint (gimp_filter_layer_get_run_count (loaded), ==, runs+1);
  retinex_definition_preserved (loaded,16,0,0,1.2);
  retinex_assert_zero_scales (loaded,source_pixels,width*height);
  gegl_buffer_get (gimp_drawable_get_buffer (GIMP_DRAWABLE (loaded)),NULL,1,
                   babl_format ("R'G'B'A u8"),actual,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
  g_assert_cmpint (memcmp (actual,expected,sizeof actual), !=, 0);
  runs = gimp_filter_layer_get_run_count (loaded);
  for (guint i = 0; i < width*height; ++i)
    { source_pixels[i*4] = (31+i*17)&255; source_pixels[i*4+1] = (121+i*31)&255;
      source_pixels[i*4+2] = (217+i*47)&255; source_pixels[i*4+3] = i%256; }
  gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (lower)),NULL,0,
                   babl_format ("R'G'B'A u8"),source_pixels,GEGL_AUTO_ROWSTRIDE);
  gimp_drawable_update (GIMP_DRAWABLE (lower),0,0,width,height); settle (loaded);
  g_assert_cmpuint (gimp_filter_layer_get_run_count (loaded), ==, runs+1);
  retinex_assert_zero_scales (loaded,source_pixels,width*height);
  retinex_definition_preserved (loaded,16,0,0,1.2);
  g_free (source_pixels); g_free (expected); g_free (directory);
  g_object_unref (copy); g_object_unref (input); g_bytes_unref (bytes);
  g_object_unref (output); g_object_unref (file); g_object_unref (image);
}

static void
retinex_owner_close (void)
{
  gint finalized = 0;
  GimpImage *image = image_new (257,139);
  GimpLayer *source = source_new (image,NULL,257,139);
  GimpFilterLayer *filter = filter_new (image,NULL,257,139);
  fill (source,31,121,217,255); retinex_define (filter,256,8,2,1.2);
  g_object_weak_ref (G_OBJECT (image),weak_finalized,&finalized);
  g_object_weak_ref (G_OBJECT (source),weak_finalized,&finalized);
  g_object_weak_ref (G_OBJECT (filter),weak_finalized,&finalized);
  blinds_wait (filter,GIMP_FILTER_LAYER_RUNNING); g_object_unref (image);
  g_assert_cmpint (finalized, ==, 3);
  /* A subsequent actual native job proves process/admission cleanup. */
  image = image_new (16,16); source = source_new (image,NULL,16,16);
  filter = filter_new (image,NULL,16,16);
  fill (source,201,61,99,255); retinex_define (filter,16,0,0,0.0); settle (filter);
  pixel (GIMP_LAYER (filter),0,0,0,255); g_object_unref (image);
}
