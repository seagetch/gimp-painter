/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Genuine Max RGB corpus consumers, deliberately unregistered until verified
 * base, large and projection artifacts are supplied. Missing fixtures fail;
 * these consumers never substitute the synthetic scalar output for captures.
 * Include after test-filter-max-rgb-context.inc when enabling the two paths. */
static gint
max_rgb_fixture_mode (const gchar *text)
{
  gchar *end = NULL;
  const gint64 mode = g_ascii_strtoll (text,&end,10);
  g_assert_true (end != text && *end == '\0');
  g_assert_cmpint (mode, >=, G_MININT32); g_assert_cmpint (mode, <=, G_MAXINT32);
  return (gint) mode;
}

typedef struct
{
  GimpImage *image;
  const gchar *directory, *id;
  guint image_width, image_height;
  gint selection, components, lock;
  gboolean called;
} MaxRGBOldContext;

static guchar *
max_rgb_fixture (const gchar *directory, const gchar *id, const gchar *label, gsize size)
{ return blinds_fixture_bytes (directory,id,label,size); }

static void
max_rgb_old_late_context (GimpFilterLayer *filter, gpointer data)
{
  MaxRGBOldContext *state = data;
  if (state->called || gimp_filter_layer_get_state (filter) != GIMP_FILTER_LAYER_IMPORTING) return;
  state->called = TRUE;
  if (state->selection >= 0)
    {
      guchar *mask = max_rgb_fixture (state->directory,state->id,"final-mask",(gsize) state->image_width*state->image_height);
      blinds_fixture_selection (state->image,state->image_width,state->image_height,0,0,mask,state->selection == 0);
      g_free (mask);
    }
  max_rgb_components (state->image,filter,state->components,state->lock);
}

static void
max_rgb_actual_old_live (void)
{
  const gchar *directory = g_getenv ("GIMP_TEST_MAX_RGB_FIXTURE_DIR");
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
      gint ox,oy,mode;
      GimpImage *image;
      GimpLayer *source;
      GimpFilterLayer *filter;
      const Babl *source_format;
      MaxRGBOldContext change;
      guchar *bytes, *previous;
      guint64 generation;
      if (!*lines[row]) continue;
      cols = g_strsplit (lines[row],"\t",-1);
      g_assert_cmpuint (g_strv_length (cols), ==, 30);
      if (!atoi (cols[1])) { g_strfreev (cols); continue; }
      source_channels = atoi (cols[2]); channels = atoi (cols[3]);
      g_assert_cmpuint (source_channels, >=, 3); g_assert_cmpuint (source_channels, <=, 4);
      layouts |= 1U << (source_channels-1);
      changed += atoi (cols[17]) == 1; unchanged += atoi (cols[17]) == 2;
      pre_merge += atoi (cols[24]) == 1; mid_work += atoi (cols[24]) == 2;
      width = atoi (cols[4]); height = atoi (cols[5]);
      image_width = atoi (cols[6]); image_height = atoi (cols[7]);
      ox = atoi (cols[8]); oy = atoi (cols[9]); mode = max_rgb_fixture_mode (cols[10]);
      passes = atoi (cols[17]) ? 2 : 1;
      g_assert_cmpuint (channels, ==, 4);
      source_format = babl_format (source_channels == 3 ? "R'G'B' u8" : "R'G'B'A u8");
      image = gimp_image_new (gimp,image_width,image_height,GIMP_RGB,GIMP_PRECISION_U8_NON_LINEAR);
      source = gimp_layer_new (image,width,height,source_format,"Old Max RGB source",1,GIMP_LAYER_MODE_NORMAL_LEGACY);
      gimp_image_add_layer (image,source,NULL,0,FALSE); filter = filter_new (image,NULL,width,height);
      gimp_item_set_offset (GIMP_ITEM (source),ox,oy); gimp_item_set_offset (GIMP_ITEM (filter),ox,oy);
      bytes = max_rgb_fixture (directory,cols[0],"source",(gsize) width*height*source_channels);
      gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (source)),NULL,0,source_format,bytes,GEGL_AUTO_ROWSTRIDE);
      g_free (bytes);
      bytes = max_rgb_fixture (directory,cols[0],"start-mask",(gsize) image_width*image_height);
      blinds_fixture_selection (image,image_width,image_height,0,0,bytes,atoi (cols[11]) == 0); g_free (bytes);
      max_rgb_components (image,filter,atoi (cols[13]),atoi (cols[15]));
      change = (MaxRGBOldContext) {image,directory,cols[0],image_width,image_height,
                                   atoi (cols[12]),atoi (cols[14]),atoi (cols[16]),FALSE};
      if (change.selection >= 0 || change.components >= 0 || change.lock >= 0)
        {
          g_assert_cmpint (atoi (cols[11]), >, 0); /* Nonempty start keeps the late gate unsealed. */
          g_assert_cmpint (atoi (cols[24]), >, 0);
          g_assert_cmpint (atoi (cols[23]), ==, 1);
          g_signal_connect (filter,"filter-state-changed",G_CALLBACK (max_rgb_old_late_context),&change);
        }
      previous = g_malloc ((gsize) width*height*4);
      max_rgb_define (filter,mode); generation = gimp_filter_layer_get_generation (filter);
      for (guint pass = 0; pass < passes; ++pass)
        {
          gegl_buffer_get (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)),NULL,1,babl_format ("R'G'B'A u8"),
                           previous,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
          if (pass)
            {
              bytes = max_rgb_fixture (directory,cols[0],"source2",(gsize) width*height*source_channels);
              if (atoi (cols[17]) == 2)
                {
                  guchar *original = max_rgb_fixture (directory,cols[0],"source",(gsize) width*height*source_channels);
                  g_assert_cmpmem (bytes,(gsize) width*height*source_channels,original,(gsize) width*height*source_channels);
                  g_free (original); /* An unchanged-byte notification is not a rewrite. */
                }
              else
                gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (source)),NULL,0,source_format,bytes,GEGL_AUTO_ROWSTRIDE);
              g_free (bytes); gimp_drawable_update (GIMP_DRAWABLE (source),0,0,width,height);
            }
          max_rgb_wait (filter,GIMP_FILTER_LAYER_CLEAN);
          bytes = max_rgb_fixture (directory,cols[0],pass ? "output2" : "output",(gsize) width*height*channels);
          g_test_message ("Genuine old Max RGB %s pass=%u native_channels=%u",cols[0],pass+1,channels);
          max_rgb_compare (filter,bytes,channels);
          {
            guchar *owned = max_rgb_fixture (directory,cols[0],pass ? "start2" : "start1",(gsize) width*height*channels);
            guchar *actual_input = g_malloc ((gsize) width*height*4);
            max_rgb_owned_reference (filter,previous,actual_input);
            g_assert_cmpmem (actual_input,(gsize) width*height*4,owned,(gsize) width*height*4);
            g_free (actual_input); g_free (owned);
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
                  bytes = max_rgb_fixture (directory,cols[0],"idle-mask",(gsize) image_width*image_height);
                  blinds_fixture_selection (image,image_width,image_height,0,0,bytes,atoi (cols[25]) == 0); g_free (bytes);
                }
              max_rgb_components (image,filter,atoi (cols[26]),atoi (cols[27]));
              spin_ms (25);
              g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 1);
              g_assert_cmpuint (gimp_filter_layer_get_generation (filter), ==, settled_generation);
              g_assert_true (cache == gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)));
              g_assert_cmpint (atoi (cols[28]), ==, 1); g_assert_cmpint (atoi (cols[29]), ==, 1);
              bytes = max_rgb_fixture (directory,cols[0],"idle-output",(gsize) width*height*channels);
              max_rgb_compare (filter,bytes,channels); g_free (bytes); g_object_unref (cache);
            }
        }
      if (change.selection >= 0 || change.components >= 0 || change.lock >= 0) g_assert_true (change.called);
      g_signal_handlers_disconnect_by_data (filter,&change);
      g_free (previous); g_object_unref (image); g_strfreev (cols); ++scenes;
    }
  /* Pin the complete finite small + true-spill corpus. The materializer
   * independently verifies old provenance and progress-before-merge events. */
  g_assert_cmpuint (scenes, ==, 126); g_assert_cmpuint (comparisons, ==, 176);
  g_assert_cmpuint (changed, ==, 14); g_assert_cmpuint (unchanged, ==, 36);
  g_assert_cmpuint (pre_merge, ==, 16); g_assert_cmpuint (mid_work, ==, 18);
  g_assert_cmpuint (layouts, ==, 12);
  g_test_message ("Max RGB genuine old live scenes=%u comparisons=%u",scenes,comparisons);
  g_strfreev (lines); g_free (text); g_free (path);
}

/* Check the observed immediate-parent copy rectangle against the native scene,
 * separately from the byte oracle. A translated group can extend past every
 * image edge; its projection is not the top-level image rectangle. */
static void
max_rgb_projection_copy_geometry (gchar **lines, const gchar *id,
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
max_rgb_actual_old_projection_history (void)
{
  const gchar *directory = g_getenv ("GIMP_TEST_MAX_RGB_PROJECTION_FIXTURE_DIR");
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
      guint source_channels, combination;
      const guint channels = 4;
      gint grouped, seeded, ox, oy, final_x, final_y;
      gsize size, source_size;
      const Babl *format, *source_format;
      GimpImage *image;
      GimpLayer *parent, *source;
      GimpFilterLayer *filter;
      guchar *source_bytes, *bytes;
      if (!*lines[row]) continue;
      cols = g_strsplit (lines[row],"\t",-1);
      g_assert_cmpuint (g_strv_length (cols), ==, 14);
      source_channels = atoi (cols[1]); grouped = atoi (cols[2]); seeded = atoi (cols[3]);
      ox = atoi (cols[4]); oy = atoi (cols[5]); final_x = atoi (cols[6]); final_y = atoi (cols[7]);
      g_assert_true (source_channels == 3 || source_channels == 4);
      g_assert_true (grouped == 0 || grouped == 1); g_assert_true (seeded == 0 || seeded == 1);
      g_assert_true ((ox == -4 && oy == -3) || (ox == 3 && oy == 2));
      g_assert_cmpint (final_x, ==, ox < 0 ? 3 : -4); g_assert_cmpint (final_y, ==, oy < 0 ? 2 : -3);
      for (guint c = 8; c < 12; ++c) g_assert_cmpint (atoi (cols[c]), ==, 3);
      g_assert_cmpint (atoi (cols[12]), ==, seeded ? 0 : -1);
      g_assert_cmpint (atoi (cols[13]), ==, seeded ? 0 : -1);
      combination = (source_channels == 4 ? 8 : 0) | (grouped ? 4 : 0) | (ox < 0 ? 2 : 0) | seeded;
      g_assert_cmpuint (combinations & (1U << combination), ==, 0);
      combinations |= 1U << combination;
      size = (gsize) width*height*channels;
      source_size = (gsize) width*height*source_channels;
      format = babl_format ("R'G'B'A u8");
      source_format = babl_format (source_channels == 3 ? "R'G'B' u8" : "R'G'B'A u8");
      image = gimp_image_new (gimp,image_width,image_height,GIMP_RGB,GIMP_PRECISION_U8_NON_LINEAR);
      parent = grouped ? gimp_group_layer_new (image) : NULL;
      if (parent) gimp_image_add_layer (image,parent,NULL,0,FALSE);
      source = gimp_layer_new (image,width,height,source_format,"Old projection source",1,GIMP_LAYER_MODE_NORMAL_LEGACY);
      gimp_image_add_layer (image,source,parent,0,FALSE); filter = filter_new (image,parent,width,height);
      gimp_item_set_offset (GIMP_ITEM (source),ox,oy); gimp_item_set_offset (GIMP_ITEM (filter),ox,oy);
      source_bytes = max_rgb_fixture (directory,cols[0],"source",source_size);
      gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (source)),NULL,0,source_format,source_bytes,GEGL_AUTO_ROWSTRIDE);
      bytes = max_rgb_fixture (directory,cols[0],"start-mask",(gsize) image_width*image_height);
      blinds_fixture_selection (image,image_width,image_height,0,0,bytes,TRUE); g_free (bytes);
      max_rgb_components (image,filter,15,0);
      bytes = max_rgb_fixture (directory,cols[0],"seed",size);
      if (seeded)
        gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)),NULL,0,format,bytes,GEGL_AUTO_ROWSTRIDE);
      max_rgb_compare (filter,bytes,channels); g_free (bytes);
      max_rgb_define (filter,1);
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
          bytes = max_rgb_fixture (directory,cols[0],"loaded-cache",size);
          max_rgb_compare (filter,bytes,channels); g_free (bytes); g_object_unref (cache);
          ++loaded_checks;
        }
      for (guint run = 1; run <= 3; ++run)
        {
          gchar label[32];
          guchar *owned;
          if (run == 2) max_rgb_define (filter,0);
          if (run == 3) gimp_item_translate (GIMP_ITEM (filter),final_x-ox,final_y-oy,FALSE);
          g_snprintf (label,sizeof label,"before%u",run);
          bytes = max_rgb_fixture (directory,cols[0],label,size);
          g_test_message ("Genuine old Max RGB history %s run=%u before copy",cols[0],run);
          max_rgb_compare (filter,bytes,channels); g_free (bytes); ++history_checks;
          /* Verify the fixture's unchanged-source control without rewriting
           * native source pixels; this emits only the observed notification. */
          g_snprintf (label,sizeof label,"source%u",run);
          bytes = max_rgb_fixture (directory,cols[0],label,source_size);
          g_assert_cmpmem (bytes,source_size,source_bytes,source_size); g_free (bytes);
          g_assert_cmpint (gimp_item_get_offset_x (GIMP_ITEM (source)), ==, ox);
          g_assert_cmpint (gimp_item_get_offset_y (GIMP_ITEM (source)), ==, oy);
          gimp_drawable_update (GIMP_DRAWABLE (source),0,0,width,height);
          max_rgb_wait (filter,GIMP_FILTER_LAYER_CLEAN);
          max_rgb_projection_copy_geometry (copy_lines,cols[0],filter,run,channels);
          g_snprintf (label,sizeof label,"output%u",run);
          bytes = max_rgb_fixture (directory,cols[0],label,size);
          g_test_message ("Genuine old Max RGB history %s run=%u output",cols[0],run);
          max_rgb_compare (filter,bytes,channels);
          g_snprintf (label,sizeof label,"start%u",run);
          owned = max_rgb_fixture (directory,cols[0],label,size);
          {
            guchar *previous, *actual_input = g_malloc (size);
            g_snprintf (label,sizeof label,"before%u",run);
            previous = max_rgb_fixture (directory,cols[0],label,size);
            max_rgb_owned_reference (filter,previous,actual_input);
            g_assert_cmpmem (actual_input,size,owned,size);
            g_free (previous); g_free (actual_input);
          }
          g_free (owned); g_free (bytes);
          g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, run); ++comparisons;
        }
      g_free (source_bytes); g_object_unref (image); g_strfreev (cols); ++scenes;
    }
  g_assert_cmpuint (scenes, ==, 16); g_assert_cmpuint (comparisons, ==, 48);
  g_assert_cmpuint (history_checks, ==, 48); g_assert_cmpuint (loaded_checks, ==, 8);
  g_assert_cmpuint (combinations, ==, 0xffff);
  g_test_message ("Max RGB genuine projection history scenes=%u comparisons=%u before_copy=%u loaded_idle=%u",
                  scenes,comparisons,history_checks,loaded_checks);
  g_strfreev (copy_lines); g_strfreev (lines); g_free (copy_text); g_free (text);
}

