/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Actual independent Blinds execution against sealed genuine-old live scenes.
 * The fixture runner verifies both archives before setting these directories. */
#include "core/gimpselection.h"
#include "core/gimpimage-private.h"
void gimp_test_filter_owner_context (Gimp *, const gchar *);
void gimp_test_filter_owner_context_phases (Gimp *, const gchar *);
void gimp_test_filter_owner_context_expansion (Gimp *, const gchar *);
void gimp_test_filter_owner_context_retry (Gimp *, const gchar *);
static const gchar *
blinds_fixture_directory (gboolean expansion)
{
  const gchar *directory = g_getenv (expansion ? "GIMP_PAINTER_CONTEXT_EXPANSION_FIXTURES" :
                                               "GIMP_PAINTER_CONTEXT_FIXTURES");
  g_assert_nonnull (directory);
  return directory;
}
static void blinds_owner_context (void)
{ gimp_test_filter_owner_context (gimp, blinds_fixture_directory (FALSE)); }
static void blinds_owner_context_phases (void)
{ gimp_test_filter_owner_context_phases (gimp, blinds_fixture_directory (FALSE)); }
static void blinds_owner_context_expansion (void)
{ gimp_test_filter_owner_context_expansion (gimp, blinds_fixture_directory (TRUE)); }
static void blinds_owner_context_retry (void)
{ gimp_test_filter_owner_context_retry (gimp, blinds_fixture_directory (FALSE)); }

static guchar *
blinds_fixture_bytes (const gchar *directory, const gchar *id, const gchar *label, gsize size)
{
  gchar *name = g_strdup_printf ("%s-%s.raw", id, label);
  gchar *path = g_build_filename (directory, name, NULL);
  gchar *bytes = NULL; gsize actual = 0;
  g_assert_true (g_file_get_contents (path, &bytes, &actual, NULL));
  g_assert_cmpuint (actual, ==, size);
  g_free (path); g_free (name); return (guchar *) bytes;
}
static void
blinds_fixture_selection (GimpImage *image, guint width, guint height,
                          gint ox, gint oy, const guchar *coverage, gboolean empty)
{
  GimpChannel *selection = gimp_image_get_mask (image);
  GeglBuffer *buffer = gimp_drawable_get_buffer (GIMP_DRAWABLE (selection));
  gegl_buffer_clear (buffer, NULL);
  if (!empty)
    gegl_buffer_set (buffer, GEGL_RECTANGLE (ox,oy,width,height), 0,
                     babl_format ("Y u8"), coverage, GEGL_AUTO_ROWSTRIDE);
  gimp_drawable_invalidate_boundary (GIMP_DRAWABLE (selection));
  gimp_drawable_update (GIMP_DRAWABLE (selection), 0, 0,
                        gimp_image_get_width (image), gimp_image_get_height (image));
}
static void
blinds_fixture_background (guint r, guint g, guint b)
{
  const guchar bytes[] = {r,g,b,255};
  GeglColor *color = gegl_color_new (NULL);
  gegl_color_set_pixel (color, babl_format ("R'G'B'A u8"), bytes);
  gimp_context_set_background (gimp_get_user_context (gimp), color);
  g_object_unref (color);
}
static void
blinds_fixture_mutate (GimpImage *image, GimpFilterLayer *filter,
                       const gchar *directory, const gchar *id,
                       guint width, guint height, gint ox, gint oy,
                       guint kind, gboolean expansion)
{
  if (expansion || kind <= 2)
    {
      guchar *mask = blinds_fixture_bytes (directory,id,"merge2-mask",width*height);
      blinds_fixture_selection (image,width,height,ox,oy,mask,expansion && kind == 2);
      g_free (mask);
    }
  else if (kind == 3 || kind == 4)
    {
      gimp_image_set_component_active (image,GIMP_CHANNEL_RED,kind == 3);
      gimp_image_set_component_active (image,GIMP_CHANNEL_GREEN,FALSE);
      gimp_image_set_component_active (image,GIMP_CHANNEL_BLUE,FALSE);
      gimp_image_set_component_active (image,GIMP_CHANNEL_ALPHA,kind == 4);
    }
  else if (kind == 5) gimp_layer_set_lock_alpha (GIMP_LAYER (filter),TRUE,FALSE);
  else blinds_fixture_background (217,31,121);
}
static void
blinds_actual_old_context_scenes (gboolean expansion)
{
  const gchar *directory = blinds_fixture_directory (expansion);
  GimpContext *context = gimp_get_user_context (gimp);
  GeglColor *saved = gegl_color_duplicate (gimp_context_get_background (context));
  guint comparisons = 0;
  for (guint offset_case = 0; offset_case < 2; ++offset_case)
    for (guint running = expansion ? 1 : 0; running < 2; ++running)
      for (guint kind = 1; kind <= (expansion ? 3 : 6); ++kind)
        {
          const guint width = expansion ? 63 : 53, height = expansion ? 63 : 41;
          const gint ox = offset_case ? 3 : 0, oy = offset_case ? 2 : 0;
          gchar *id = g_strdup_printf ("%s-k%u",running ? "running" : "idle",kind);
          GimpImage *image = image_new (width+ox+offset_case,height+oy+offset_case);
          GimpLayer *source = source_new (image,NULL,width,height);
          GimpFilterLayer *filter = filter_new (image,NULL,width,height);
          const Babl *format = babl_format ("R'G'B'A u8");
          guchar *input = blinds_fixture_bytes (directory,id,"start1",width*height*4);
          guchar *actual = g_malloc (width*height*4);
          if (offset_case)
            {
              /* XCF loads layers before replacing the native selection. */
              GimpChannel *replacement = gimp_selection_new (image,width+ox+offset_case,height+oy+offset_case);
              gimp_image_take_mask (image,replacement);
            }
          gimp_item_set_offset (GIMP_ITEM (source),ox,oy);
          gimp_item_set_offset (GIMP_ITEM (filter),ox,oy);
          gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (source)),NULL,0,format,input,GEGL_AUTO_ROWSTRIDE);
          blinds_fixture_background (31,121,217);
          if (expansion)
            {
              guchar *mask = g_malloc0 (width*height);
              for (guint y = 3; y < height-3; ++y)
                for (guint x = width/3; x < 2*width/3; ++x) mask[y*width+x] = 255;
              blinds_fixture_selection (image,width,height,ox,oy,mask,FALSE); g_free (mask);
            }
          define_blinds (filter,67,7,1,0);
          if (running)
            {
              blinds_wait (filter,GIMP_FILTER_LAYER_RUNNING);
              blinds_fixture_mutate (image,filter,directory,id,width,height,ox,oy,kind,expansion);
            }
          for (guint pass = 0; pass < 2; ++pass)
            {
              guchar *expected;
              if (pass) gimp_drawable_update (GIMP_DRAWABLE (source),0,0,width,height);
              settle (filter);
              expected = blinds_fixture_bytes (directory,id,pass ? "rerun" : "settled",width*height*4);
              gegl_buffer_get (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)),NULL,1,format,
                               actual,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
              g_test_message ("Genuine old Blinds %s offset=%d,%d pass=%u",id,ox,oy,pass+1);
              g_assert_cmpmem (actual,width*height*4,expected,width*height*4);
              g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, pass+1);
              g_free (expected); ++comparisons;
              if (!pass && !running)
                {
                  blinds_fixture_mutate (image,filter,directory,id,width,height,ox,oy,kind,FALSE);
                  spin_ms (10);
                  g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 1);
                  gegl_buffer_get (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)),NULL,1,format,
                                   actual,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
                  expected = blinds_fixture_bytes (directory,id,"after-context",width*height*4);
                  g_assert_cmpmem (actual,width*height*4,expected,width*height*4); g_free (expected);
                }
            }
          g_free (actual); g_free (input); g_free (id); g_object_unref (image);
        }
  g_assert_cmpuint (comparisons, ==, expansion ? 12 : 48);
  gimp_context_set_background (context,saved); g_object_unref (saved);
}
static void blinds_actual_old_context (void) { blinds_actual_old_context_scenes (FALSE); }
static void blinds_actual_old_expansion (void) { blinds_actual_old_context_scenes (TRUE); }

static void
blinds_gray_offset_context (void)
{
  GimpContext *context = gimp_get_user_context (gimp);
  GeglColor *saved = gegl_color_duplicate (gimp_context_get_background (context));
  const Babl *format = babl_format ("Y'A u8");
  /* Native Gray arithmetic and every component combination also have genuine
   * old records in the independent 240-record owner test. These actual process
   * scenes use the exact hard-mask limiting cases for a visible end-to-end check. */
  blinds_fixture_background (121,121,121);
  for (guint selection = 0; selection < 4; ++selection)
    for (guint active = 0; active < 4; ++active)
      for (guint lock = 0; lock < 3; ++lock)
        {
          GimpImage *image = gimp_image_new (gimp,39,29,GIMP_GRAY,GIMP_PRECISION_U8_NON_LINEAR);
          GimpLayer *parent = gimp_group_layer_new (image);
          GimpLayer *source = gimp_layer_new (image,31,17,format,"Gray context source",1,GIMP_LAYER_MODE_NORMAL_LEGACY);
          GimpFilterLayer *filter = GIMP_FILTER_LAYER (gimp_filter_layer_new (image,31,17,"Gray context",1,GIMP_LAYER_MODE_NORMAL_LEGACY));
          guchar input[31*17*2], actual[31*17*2], mask[31*17];
          for (guint i = 0; i < 31*17; ++i)
            { input[i*2] = 83; input[i*2+1] = 128; mask[i] = selection == 2 || (selection == 1 && i%31 < 13) ? 255 : 0; }
          gimp_image_add_layer (image,parent,NULL,0,FALSE);
          gimp_image_add_layer (image,source,parent,0,FALSE);
          gimp_image_add_layer (image,GIMP_LAYER (filter),parent,0,FALSE);
          gimp_item_set_offset (GIMP_ITEM (source),3,2); gimp_item_set_offset (GIMP_ITEM (filter),3,2);
          gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (source)),NULL,0,format,input,GEGL_AUTO_ROWSTRIDE);
          blinds_fixture_selection (image,31,17,3,2,mask,selection == 0);
          if (selection == 3)
            gimp_channel_select_rectangle (gimp_image_get_mask (image),38,28,1,1,GIMP_CHANNEL_OP_REPLACE,FALSE,0,0,FALSE);
          gimp_image_set_component_active (image,GIMP_CHANNEL_GRAY,active & 1);
          gimp_image_set_component_active (image,GIMP_CHANNEL_ALPHA,active & 2);
          if (lock == 1) gimp_layer_set_lock_alpha (GIMP_LAYER (filter),TRUE,FALSE);
          if (lock == 2) parent->lock_alpha = TRUE; /* Same synthetic ancestor field probe as old corpus. */
          define_blinds (filter,90,1,1,0); settle (filter);
          gegl_buffer_get (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)),NULL,1,format,actual,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
          for (guint i = 0; i < 31*17; ++i)
            {
              const gboolean selected = selection == 0 || (selection != 3 && mask[i]);
              /* At 90 degrees the old algorithm retains its slat center.
               * The first column is background; outside the ROI is original. */
              if (!selected || i%31 == 0)
                {
                  g_assert_cmpuint (actual[i*2], ==, selected && (active & 1) ? 121 : 83);
                  g_assert_cmpuint (actual[i*2+1], ==, selected && (active & 2) && lock != 1 ? 255 : 128);
                }
            }
          g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 1);
          g_object_unref (image);
        }
  gimp_context_set_background (context,saved); g_object_unref (saved);
}
