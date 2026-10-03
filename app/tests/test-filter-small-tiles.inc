/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Actual current SmallTiles process/lifecycle tests against sealed genuine-old
 * evidence. Included after the shared Blinds context helpers. */
#include <stdlib.h>
#include "core/gimpcontainer.h"
#include "xcf/xcf.h"
static void
small_tiles_define (GimpFilterLayer *filter, gint tiles)
{
  const guchar saved[] = {0,1,0xfe,0,0x7f,0xff};
  GBytes *raw = g_bytes_new (saved,sizeof saved);
  GimpValueArray *args = gimp_value_array_new_from_types (NULL,
    G_TYPE_INT,1,G_TYPE_INT,123,G_TYPE_INT,456,G_TYPE_INT,tiles,G_TYPE_NONE);
  GError *error = NULL;
  g_assert_true (gimp_filter_layer_set_definition (filter,"plug-in-small-tiles",raw,args,&error));
  g_assert_no_error (error); g_bytes_unref (raw); gimp_value_array_unref (args);
}
static void
small_tiles_definition_preserved (GimpFilterLayer *filter, gint tiles)
{
  const guchar saved[] = {0,1,0xfe,0,0x7f,0xff};
  const gint expected[] = {1,123,456,tiles};
  gchar *name = gimp_filter_layer_dup_procedure (filter);
  GBytes *raw = gimp_filter_layer_ref_definition (filter);
  GimpValueArray *args = gimp_filter_layer_dup_args (filter);
  g_assert_cmpstr (name, ==, "plug-in-small-tiles");
  g_assert_cmpmem (g_bytes_get_data (raw,NULL),g_bytes_get_size (raw),saved,sizeof saved);
  g_assert_cmpuint (gimp_value_array_length (args), ==, 4);
  for (guint i = 0; i < 4; ++i)
    { g_assert_true (G_VALUE_HOLDS_INT (gimp_value_array_index (args,i)));
      g_assert_cmpint (g_value_get_int (gimp_value_array_index (args,i)), ==, expected[i]); }
  g_free (name); g_bytes_unref (raw); gimp_value_array_unref (args);
}
static void
small_tiles_live_update (void)
{
  const gint factors[] = {0, 1, 3, 6};
  for (guint gray = 0; gray < 2; ++gray)
    for (guint factor = 0; factor < G_N_ELEMENTS (factors); ++factor)
      {
        const guint channels = gray ? 2 : 4;
        const Babl *format = babl_format (gray ? "Y'A u8" : "R'G'B'A u8");
        GimpImage *image = gimp_image_new (gimp, 67, 66, gray ? GIMP_GRAY : GIMP_RGB, GIMP_PRECISION_U8_NON_LINEAR);
        GimpLayer *source = gimp_layer_new (image, 67, 66, format, "Additional tiles source", 1, GIMP_LAYER_MODE_NORMAL_LEGACY);
        GimpFilterLayer *filter = GIMP_FILTER_LAYER (gimp_filter_layer_new (image, 67, 66, "Additional tiles", 1, GIMP_LAYER_MODE_NORMAL_LEGACY));
        guchar input[67*66*4], actual[67*66*4];
        gimp_image_add_layer (image, source, NULL, 0, FALSE);
        for (guint i = 0; i < 67*66*channels; i += channels)
          { for (guint c = 0; c+1 < channels; ++c) input[i+c] = (i*17+c*71)&255; input[i+channels-1] = 255; }
        gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (source)), NULL, 0, format, input, GEGL_AUTO_ROWSTRIDE);
        gimp_image_add_layer (image, GIMP_LAYER (filter), NULL, 0, FALSE);
        small_tiles_define (filter, factors[factor]);
        for (guint update = 0; update < 2; ++update)
          {
            if (update)
              {
                input[0] ^= 255;
                gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (source)), NULL, 0, format, input, GEGL_AUTO_ROWSTRIDE);
                gimp_drawable_update (GIMP_DRAWABLE (source), 0, 0, 67, 66);
              }
            settle (filter);
            gegl_buffer_get (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)), NULL, 1, format, actual, GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);
            for (guint y = 0; y < 66; ++y) for (guint x = 0; x < 67; ++x)
              {
                guint at = (y*67+x)*channels;
                guint src = (((y*factors[factor])%66)*67+(x*factors[factor])%67)*channels;
                g_assert_cmpmem (actual+at, channels, input+src, channels);
              }
            g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, update+1);
            small_tiles_definition_preserved (filter, factors[factor]);
          }
        g_object_unref (image);
      }
}
static guchar *
small_tiles_fixture (const gchar *directory, const gchar *id, const gchar *label, gsize expected)
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
small_tiles_actual_old_live (void)
{
  const gchar *root = g_getenv ("GIMP_PAINTER_SMALL_TILES_FIXTURES");
  gchar *directory;
  gchar *manifest;
  gchar *text = NULL, **lines;
  guint compared = 0;
  g_assert_nonnull (root);
  directory = g_build_filename (root,"live",NULL);
  manifest = g_build_filename (directory,"cases.tsv",NULL);
  g_assert_true (g_file_get_contents (manifest,&text,NULL,NULL));
  lines = g_strsplit (text,"\n",-1);
  for (guint row = 0; lines[row]; ++row)
    {
      gchar **cols;
      guint width,height,channels,selection;
      gint tiles;
      GimpImage *image;
      GimpLayer *source;
      GimpFilterLayer *filter;
      const Babl *format;
      guchar *input,*actual;
      if (!*lines[row]) continue;
      cols = g_strsplit (lines[row],"\t",-1); g_assert_cmpuint (g_strv_length (cols), ==, 10);
      width = atoi (cols[2]); height = atoi (cols[3]); channels = atoi (cols[4]);
      tiles = atoi (cols[5]);
      g_assert_cmpstr (cols[1], ==, "plug-in-small-tiles");
      g_assert_true (channels == 2 || channels == 4);
      selection = atoi (strstr (cols[0],"-s")+2);
      image = gimp_image_new (gimp,selection == 3 ? 63 : width,selection == 3 ? 63 : height,
                             channels == 2 ? GIMP_GRAY : GIMP_RGB,GIMP_PRECISION_U8_NON_LINEAR);
      format = babl_format (channels == 2 ? "Y'A u8" : "R'G'B'A u8");
      source = gimp_layer_new (image,width,height,format,"Old fixture lower source",1,GIMP_LAYER_MODE_NORMAL_LEGACY);
      gimp_image_add_layer (image,source,NULL,0,FALSE);
      input = small_tiles_fixture (directory,cols[0],"source",width*height*channels);
      actual = g_malloc (width*height*channels);
      gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (source)),NULL,0,format,input,GEGL_AUTO_ROWSTRIDE);
      if (selection == 3)
        gimp_channel_select_rectangle (gimp_image_get_mask (image),62,62,1,1,GIMP_CHANNEL_OP_REPLACE,FALSE,0,0,FALSE);
      else if (selection)
        {
          GeglBuffer *coverage = gegl_buffer_new (GEGL_RECTANGLE (0,0,width,height),babl_format ("Y u8"));
          guchar *mask = small_tiles_fixture (directory,cols[0],"merge1-mask",width*height);
          gegl_buffer_set (coverage,NULL,0,babl_format ("Y u8"),mask,GEGL_AUTO_ROWSTRIDE);
          gimp_channel_select_buffer (gimp_image_get_mask (image),"Old fixture selection",coverage,0,0,
                                      GIMP_CHANNEL_OP_REPLACE,FALSE,0,0);
          g_free (mask); g_object_unref (coverage);
        }
      filter = GIMP_FILTER_LAYER (gimp_filter_layer_new (image,width,height,"Old SmallTiles fixture",1,GIMP_LAYER_MODE_NORMAL_LEGACY));
      gimp_image_add_layer (image,GIMP_LAYER (filter),NULL,0,FALSE);
      small_tiles_define (filter,tiles);
      for (guint pass = 0; pass < 2; ++pass)
        {
          guchar *expected;
          if (pass)
            {
              if (selection == 3)
                {
                  g_free (input); input = small_tiles_fixture (directory,cols[0],"source2",width*height*channels);
                  gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (source)),NULL,0,format,input,GEGL_AUTO_ROWSTRIDE);
                }
              gimp_drawable_update (GIMP_DRAWABLE (source),0,0,width,height);
            }
          settle (filter);
          expected = small_tiles_fixture (directory,cols[0],pass ? "rerun" : "settled",width*height*channels);
          gegl_buffer_get (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)),NULL,1,format,actual,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
          g_test_message ("Actual old SmallTiles Filter scene %s pass%u",cols[0],pass+1);
          g_assert_cmpmem (actual,width*height*channels,expected,width*height*channels);
          g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, pass+1);
          g_free (expected); ++compared;
        }
      small_tiles_definition_preserved (filter,tiles);
      g_free (input); g_free (actual); g_object_unref (image); g_strfreev (cols);
    }
  g_assert_cmpuint (compared, ==, 50);
  g_strfreev (lines); g_free (text); g_free (manifest); g_free (directory);
}

static void
small_tiles_invalid_domains_keep_cache (void)
{
  for (guint kind = 0; kind < 4; ++kind)
    {
      GimpImage *image = image_new (67,66);
      GimpLayer *source = source_new (image,NULL,67,66);
      GimpFilterLayer *filter = filter_new (image,NULL,67,66);
      fill (source,31,121,217,255);
      small_tiles_define (filter,kind == 0 ? -1 : kind == 1 ? 7 : 3);
      if (kind >= 2)
        {
          GBytes *raw = gimp_filter_layer_ref_definition (filter);
          GimpValueArray *args = gimp_filter_layer_dup_args (filter);
          GValue *value = gimp_value_array_index (args,kind == 2 ? 0 : 3);
          g_value_unset (value); g_value_init (value,G_TYPE_DOUBLE); g_value_set_double (value,3.0);
          g_assert_true (gimp_filter_layer_set_definition (filter,"plug-in-small-tiles",raw,args,NULL));
          g_bytes_unref (raw); gimp_value_array_unref (args);
        }
      fill (GIMP_LAYER (filter),17,29,53,123); gimp_filter_layer_mark_as_loaded (filter);
      gimp_filter_layer_invalidate (filter); blinds_wait (filter,GIMP_FILTER_LAYER_FAILED);
      pixel (GIMP_LAYER (filter),17,29,53,123);
      g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 0);
      spin_ms (20); g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 0);
      if (kind < 2) small_tiles_definition_preserved (filter,kind == 0 ? -1 : 7);
      else
        {
          GimpValueArray *args = gimp_filter_layer_dup_args (filter);
          g_assert_true (G_VALUE_HOLDS_DOUBLE (gimp_value_array_index (args,kind == 2 ? 0 : 3)));
          g_assert_cmpfloat (g_value_get_double (gimp_value_array_index (args,kind == 2 ? 0 : 3)), ==, 3.0);
          gimp_value_array_unref (args);
        }
      g_object_unref (image);
    }
}
static void
small_tiles_replace_running_definition (void)
{
  for (guint explicit_cancel = 0; explicit_cancel < 2; ++explicit_cancel)
    {
      GimpImage *image = image_new (257,139);
      GimpLayer *source = source_new (image,NULL,257,139);
      GimpFilterLayer *filter = filter_new (image,NULL,257,139);
      fill (source,31,121,217,255); small_tiles_define (filter,3);
      blinds_wait (filter,GIMP_FILTER_LAYER_RUNNING);
      if (explicit_cancel) gimp_filter_layer_cancel (filter);
      small_tiles_define (filter,1); settle (filter);
      pixel (GIMP_LAYER (filter),31,121,217,255);
      small_tiles_definition_preserved (filter,1);
      g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 2);
      g_object_unref (image);
    }
}
static void
small_tiles_selection_no_merge (void)
{
  /* A nonintersecting selection must publish the new lower input on every
   * generation, including hidden colors, without retaining the prior cache. */
  GimpImage *image = image_new (63,63);
  GimpLayer *source = source_new (image,NULL,53,41);
  GimpFilterLayer *filter = filter_new (image,NULL,53,41);
  gimp_channel_select_rectangle (gimp_image_get_mask (image),62,62,1,1,
                                 GIMP_CHANNEL_OP_REPLACE,FALSE,0,0,FALSE);
  fill (source,31,121,217,0); small_tiles_define (filter,0);
  fill (GIMP_LAYER (filter),17,29,53,123); gimp_filter_layer_mark_as_loaded (filter);
  gimp_filter_layer_invalidate (filter); settle (filter);
  pixel (GIMP_LAYER (filter),31,121,217,0);
  fill (source,201,61,99,0); settle (filter);
  pixel (GIMP_LAYER (filter),201,61,99,0);
  g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 2);
  small_tiles_definition_preserved (filter,0); g_object_unref (image);
}
static void
small_tiles_final_context (void)
{
  const guint width = 67, height = 66;
  GimpImage *image = image_new (width,height);
  GimpLayer *source = source_new (image,NULL,width,height);
  GimpFilterLayer *filter = filter_new (image,NULL,width,height);
  guchar input[67*66*4], actual[67*66*4];
  for (guint i = 0; i < width*height; ++i)
    { input[i*4] = (i*17)&255; input[i*4+1] = (i*31)&255; input[i*4+2] = (i*47)&255; input[i*4+3] = i ? 1+i%254 : 255; }
  gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (source)),NULL,0,
                   babl_format ("R'G'B'A u8"),input,GEGL_AUTO_ROWSTRIDE);
  small_tiles_define (filter,0); blinds_wait (filter,GIMP_FILTER_LAYER_RUNNING);
  gimp_channel_select_rectangle (gimp_image_get_mask (image),3,2,11,9,GIMP_CHANNEL_OP_REPLACE,FALSE,0,0,FALSE);
  gimp_image_set_component_active (image,GIMP_CHANNEL_GREEN,FALSE);
  gimp_layer_set_lock_alpha (GIMP_LAYER (filter),TRUE,FALSE);
  settle (filter);
  gegl_buffer_get (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)),NULL,1,
                   babl_format ("R'G'B'A u8"),actual,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
  for (guint y = 0; y < height; ++y) for (guint x = 0; x < width; ++x)
    {
      guint at = (y*width+x)*4;
      guchar expected[4]; memcpy (expected,input+at,4);
      if (x >= 3 && x < 14 && y >= 2 && y < 11)
        { expected[0] = input[0]; expected[2] = input[2]; }
      g_assert_cmpmem (actual+at,4,expected,4);
    }
  g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 1);
  g_object_unref (image);
}
static void
small_tiles_dependency_updates (void)
{
  GimpImage *image = image_new (67,66);
  GimpLayer *source = source_new (image,NULL,67,66);
  GimpFilterLayer *lower = filter_new (image,NULL,67,66);
  GimpFilterLayer *upper = filter_new (image,NULL,67,66);
  guint64 runs;
  fill (source,31,121,217,255); small_tiles_define (lower,3); small_tiles_define (upper,1);
  settle (upper); pixel (GIMP_LAYER (upper),31,121,217,255);
  runs = gimp_filter_layer_get_run_count (upper);
  fill (source,201,61,99,255); settle (upper); pixel (GIMP_LAYER (upper),201,61,99,255);
  g_assert_cmpuint (gimp_filter_layer_get_run_count (upper), >, runs);
  small_tiles_definition_preserved (lower,3); small_tiles_definition_preserved (upper,1);
  g_object_unref (image);
}
static void
small_tiles_save_reopen (void)
{
  for (guint factor = 0; factor <= 1; ++factor)
    {
      GimpImage *image = image_new (67,66);
      GimpLayer *source = source_new (image,NULL,67,66);
      GimpFilterLayer *filter = filter_new (image,NULL,67,66);
      GError *error = NULL;
      GOutputStream *output = g_memory_output_stream_new_resizable ();
      GFile *file = g_file_new_for_path ("/tmp/small-tiles-definition-roundtrip.xcf");
      GBytes *bytes;
      GInputStream *input;
      GimpImage *copy;
      GimpContainer *layers;
      GimpFilterLayer *loaded;
      GimpLayer *lower;
      GBytes *definition;
      GimpValueArray *args;
      guint64 runs, revision;
      guchar lower_pixels[67*66*4], actual[67*66*4];
      fill (source,31,121,217,255); small_tiles_define (filter,factor); settle (filter);
      g_assert_true (xcf_save_stream (gimp,image,output,file,NULL,&error)); g_assert_no_error (error);
      g_assert_true (g_output_stream_close (output,NULL,&error)); g_assert_no_error (error);
      bytes = g_memory_output_stream_steal_as_bytes (G_MEMORY_OUTPUT_STREAM (output));
      input = g_memory_input_stream_new_from_bytes (bytes);
      copy = xcf_load_stream (gimp,input,file,NULL,&error); g_assert_no_error (error); g_assert_nonnull (copy);
      layers = gimp_image_get_layers (copy);
      loaded = GIMP_FILTER_LAYER (gimp_container_get_first_child (layers));
      lower = GIMP_LAYER (gimp_container_get_last_child (layers));
      g_assert_true (GIMP_IS_FILTER_LAYER (loaded)); g_assert_false (GIMP_IS_FILTER_LAYER (lower));
      small_tiles_definition_preserved (loaded,factor); pixel (GIMP_LAYER (loaded),31,121,217,255);
      for (guint i = 0; i < 67*66; ++i)
        {
          lower_pixels[i*4] = (31+i*17)&255;
          lower_pixels[i*4+1] = (121+i*31)&255;
          lower_pixels[i*4+2] = (217+i*47)&255;
          lower_pixels[i*4+3] = 255;
        }
      runs = gimp_filter_layer_get_run_count (loaded);
      gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (lower)),NULL,0,
                       babl_format ("R'G'B'A u8"),lower_pixels,GEGL_AUTO_ROWSTRIDE);
      gimp_drawable_update (GIMP_DRAWABLE (lower),0,0,67,66); settle (loaded);
      g_assert_cmpuint (gimp_filter_layer_get_run_count (loaded), ==, runs+1);
      small_tiles_definition_preserved (loaded,factor);
      gegl_buffer_get (gimp_drawable_get_buffer (GIMP_DRAWABLE (loaded)),NULL,1,
                       babl_format ("R'G'B'A u8"),actual,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
      for (guint i = 0; i < 67*66; ++i)
        g_assert_cmpmem (actual+i*4,4,lower_pixels+(factor ? i*4 : 0),4);

      /* Reopen must retain editable arguments, not only a working saved
       * cache. Change just the factor through the real undoable edit API;
       * the saved run/image/drawable integers and opaque bytes stay exact. */
      runs = gimp_filter_layer_get_run_count (loaded);
      revision = gimp_filter_layer_get_definition_revision (loaded);
      definition = gimp_filter_layer_ref_definition (loaded);
      args = gimp_filter_layer_dup_args (loaded);
      g_value_set_int (gimp_value_array_index (args,3),1-factor);
      g_assert_true (gimp_filter_layer_edit_definition (loaded,"plug-in-small-tiles",definition,args,&error));
      g_assert_no_error (error); g_bytes_unref (definition); gimp_value_array_unref (args);
      g_assert_cmpuint (gimp_filter_layer_get_definition_revision (loaded), ==, revision+1);
      settle (loaded);
      g_assert_cmpuint (gimp_filter_layer_get_run_count (loaded), ==, runs+1);
      small_tiles_definition_preserved (loaded,1-factor);
      gegl_buffer_get (gimp_drawable_get_buffer (GIMP_DRAWABLE (loaded)),NULL,1,
                       babl_format ("R'G'B'A u8"),actual,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
      for (guint i = 0; i < 67*66; ++i)
        g_assert_cmpmem (actual+i*4,4,lower_pixels+(!factor ? i*4 : 0),4);
      g_assert_cmpint (lower_pixels[0], !=, lower_pixels[4]);
      g_object_unref (copy); g_object_unref (input); g_bytes_unref (bytes); g_object_unref (output);
      g_object_unref (file); g_object_unref (image);
    }
}
static void
small_tiles_owner_close (void)
{
  gint finalized = 0;
  GimpImage *image = image_new (257,139);
  GimpLayer *source = source_new (image,NULL,257,139);
  GimpFilterLayer *filter = filter_new (image,NULL,257,139);
  fill (source,31,121,217,255); small_tiles_define (filter,3);
  g_object_weak_ref (G_OBJECT (image),weak_finalized,&finalized);
  g_object_weak_ref (G_OBJECT (source),weak_finalized,&finalized);
  g_object_weak_ref (G_OBJECT (filter),weak_finalized,&finalized);
  blinds_wait (filter,GIMP_FILTER_LAYER_RUNNING); g_object_unref (image);
  g_assert_cmpint (finalized, ==, 3);
  /* A subsequent native job establishes that cancelled work releases the
   * shared process/admission resources, instead of merely waiting a delay. */
  image = image_new (67,66); source = source_new (image,NULL,67,66);
  filter = filter_new (image,NULL,67,66);
  fill (source,201,61,99,255); small_tiles_define (filter,1); settle (filter);
  pixel (GIMP_LAYER (filter),201,61,99,255); g_object_unref (image);
}
