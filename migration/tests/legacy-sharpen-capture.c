/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Fixture authoring against the pinned OLD application, never the port. */
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gegl.h>
#include <gtk/gtk.h>
#include "core/core-types.h"
#include "base/pixel-region.h"
#include "base/tile-manager.h"
#include "core/gimp.h"
#include "core/gimpcontext.h"
#include "core/gimpchannel.h"
#include "core/gimpdrawable.h"
#include "core/gimpdrawable-shadow.h"
#include "core/gimpfilterlayer.h"
#include "core/gimpgrouplayer.h"
#include "core/gimpimage.h"
#include "core/gimpimage-new.h"
#include "core/gimpparamspecs.h"
#include "core/gimplayer.h"
#include "core/gimppickable.h"
#include "core/gimpprojection.h"
#include "pdb/gimppdb.h"
#include "pdb/gimpprocedure.h"
#include "tests.h"
#include "file/file-save.h"
#include "plug-in/gimppluginprocedure.h"

extern gint gimp_filter_layer_capture_pending (GimpFilterLayer *layer);
void capture_context_start (GimpDrawable *drawable);
void capture_context_end (GimpDrawable *drawable);
void capture_context_progress (Gimp *gimp, gdouble value);
void capture_context_merge (GimpDrawable *drawable, gboolean before);

static const gchar *directory;
static Gimp *application;
static GimpLayer *observed;
static gchar scenario[100];
static gint starts, ends, mutated, merge_index;
static FILE *merges, *events, *bounds, *results, *rois;
#include "legacy-sharpen-cases.h"
static const CaptureCase *active_case;

static void
raw_file (const gchar *name, const guchar *bytes, gsize count)
{
  gchar *path = g_build_filename (directory, name, NULL);
  GError *error = NULL;
  g_assert (g_file_set_contents (path, (const gchar *) bytes, count, &error));
  g_assert_no_error (error);
  g_free (path);
}

static guchar *
read_tiles (TileManager *tiles, gint width, gint height, gint channels)
{
  PixelRegion region;
  guchar *pixels = g_malloc (width * height * channels);
  gint y;
  pixel_region_init (&region, tiles, 0, 0, width, height, FALSE);
  for (y = 0; y < height; ++y)
    pixel_region_get_row (&region, 0, y, width,
                          pixels + y * width * channels, 1);
  return pixels;
}

static void
write_tiles (TileManager *tiles, const guchar *pixels, gint width, gint height,
             gint channels)
{
  PixelRegion region;
  gint y;
  pixel_region_init (&region, tiles, 0, 0, width, height, TRUE);
  for (y = 0; y < height; ++y)
    pixel_region_set_row (&region, 0, y, width,
                          pixels + y * width * channels);
}

static void
dump_drawable (GimpDrawable *drawable, const gchar *label)
{
  gint width = gimp_item_get_width (GIMP_ITEM (drawable));
  gint height = gimp_item_get_height (GIMP_ITEM (drawable));
  gint channels = gimp_drawable_bytes (drawable);
  guchar *pixels = read_tiles (gimp_drawable_get_tiles (drawable), width, height, channels);
  gchar name[180];
  g_snprintf (name, sizeof name, "%s-%s.raw", scenario, label);
  raw_file (name, pixels, width * height * channels);
  g_free (pixels);
}

static void
selection (GimpImage *image, gint kind)
{
  GimpChannel *mask = gimp_image_get_mask (image);
  gint width = gimp_image_get_width (image), height = gimp_image_get_height (image);
  guchar *pixels = g_malloc0 (width * height);
  gint x, y;
  for (y = 0; y < height; ++y)
    for (x = 0; x < width; ++x)
      {
        if (kind == 1 && x >= width / 4 && x < 3 * width / 4 &&
                         y >= height / 4 && y < 3 * height / 4)
          pixels[y * width + x] = 255;
        if (kind == 2 && x > 0 && y > 0 && x < width - 1 && y < height - 1)
          pixels[y * width + x] = (x * 17 + y * 29) % 256;
        if (kind == 4 && x > 0 && y > 0 && x < width - 1 && y < height - 1)
          pixels[y * width + x] = 255;
        if (kind == 3 && x == width - 1 && y == height - 1)
          pixels[y * width + x] = 255;
      }
  write_tiles (gimp_drawable_get_tiles (GIMP_DRAWABLE (mask)), pixels, width, height, 1);
  /* Direct fixture writes must invalidate BOTH old caches. Boundary invalidation
   * alone deliberately does not invalidate gimp_channel_is_empty()/bounds(). */
  mask->bounds_known = FALSE;
  gimp_drawable_invalidate_boundary (GIMP_DRAWABLE (mask));
  gimp_drawable_update (GIMP_DRAWABLE (mask), 0, 0, width, height);
  gimp_image_mask_changed (image);
  g_assert_cmpint (gimp_channel_is_empty (mask), ==, kind == 0);
  g_free (pixels);
}

static guint
active_bits (GimpDrawable *drawable)
{
  gboolean active[4] = { FALSE, FALSE, FALSE, FALSE };
  guint bits = 0;
  gint c;
  gimp_drawable_get_active_components (drawable, active);
  for (c = 0; c < gimp_drawable_bytes (drawable); ++c)
    if (active[c]) bits |= 1 << c;
  return bits;
}

static void
set_active (GimpImage *image, guint bits, gint channels)
{
  if (channels >= 3)
    {
      gimp_image_set_component_active (image, GIMP_RED_CHANNEL, bits & 1);
      gimp_image_set_component_active (image, GIMP_GREEN_CHANNEL, bits & 2);
      gimp_image_set_component_active (image, GIMP_BLUE_CHANNEL, bits & 4);
    }
  else
    gimp_image_set_component_active (image, GIMP_GRAY_CHANNEL, bits & 1);
  gimp_image_set_component_active (image, GIMP_ALPHA_CHANNEL,
                                   bits & (1 << (channels - 1)));
}

static void
dump_merge_before (GimpDrawable *drawable, const gchar *id)
{
  GimpItem *item = GIMP_ITEM (drawable);
  GimpImage *image = gimp_item_get_image (item);
  GimpChannel *mask = gimp_image_get_mask (image);
  gint width = gimp_item_get_width (item), height = gimp_item_get_height (item);
  gint channels = gimp_drawable_bytes (drawable);
  gint iw = gimp_image_get_width (image), ih = gimp_image_get_height (image);
  gint ox = gimp_item_get_offset_x (item), oy = gimp_item_get_offset_y (item);
  gboolean empty = gimp_channel_is_empty (mask);
  guchar *input = read_tiles (gimp_drawable_get_tiles (drawable), width, height, channels);
  guchar *shadow = read_tiles (gimp_drawable_get_shadow_tiles (drawable), width, height, channels);
  guchar *global = read_tiles (gimp_drawable_get_tiles (GIMP_DRAWABLE (mask)), iw, ih, 1);
  guchar *coverage = g_malloc0 (width * height);
  gchar filename[180];
  gint x, y, x1, y1, x2, y2, sx1, sy1, sx2, sy2;
  gboolean selected;
  for (y = 0; y < height; ++y)
    for (x = 0; x < width; ++x)
      coverage[y * width + x] = empty ? 255 :
        (x + ox >= 0 && x + ox < iw && y + oy >= 0 && y + oy < ih ?
         global[(y + oy) * iw + x + ox] : 0);
  g_snprintf (filename, sizeof filename, "%s-input.raw", id);
  raw_file (filename, input, width * height * channels);
  g_snprintf (filename, sizeof filename, "%s-shadow.raw", id);
  raw_file (filename, shadow, width * height * channels);
  g_snprintf (filename, sizeof filename, "%s-mask.raw", id);
  raw_file (filename, coverage, width * height);
  fprintf (merges, "%s\t%d\t%d\t%d\t%u\t%d\n", id, width, height, channels,
           active_bits (drawable), empty);
  selected = gimp_item_mask_bounds (item, &x1, &y1, &x2, &y2);
  gimp_channel_bounds (mask, &sx1, &sy1, &sx2, &sy2);
  fprintf (bounds, "%s\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n",
           id, width, height, ox, oy, empty, sx1, sy1, sx2, sy2,
           selected, x1, y1, x2, y2);
  fflush (merges); fflush (bounds);
  g_free (input); g_free (shadow); g_free (global); g_free (coverage);
}

static void
dump_merge_after (GimpDrawable *drawable, const gchar *id)
{
  gint width = gimp_item_get_width (GIMP_ITEM (drawable));
  gint height = gimp_item_get_height (GIMP_ITEM (drawable));
  gint channels = gimp_drawable_bytes (drawable);
  guchar *pixels = read_tiles (gimp_drawable_get_tiles (drawable), width, height, channels);
  gchar filename[180];
  g_snprintf (filename, sizeof filename, "%s-output.raw", id);
  raw_file (filename, pixels, width * height * channels);
  g_free (pixels);
}

void
capture_context_merge (GimpDrawable *drawable, gboolean before)
{
  gchar id[160];
  if (drawable != GIMP_DRAWABLE (observed)) return;
  if (before) ++merge_index;
  g_snprintf (id, sizeof id, "%s-merge%d", scenario, merge_index);
  if (before) dump_merge_before (drawable, id);
  else dump_merge_after (drawable, id);
}

static void
capture_roi (GimpDrawable *drawable)
{
  gint x1, y1, x2, y2;
  gboolean selected = gimp_item_mask_bounds (GIMP_ITEM (drawable), &x1, &y1, &x2, &y2);
  fprintf (rois, "%s\t%d\t%d\t%d\t%d\t%d\n", scenario, selected, x1, y1, x2, y2);
  fflush (rois);
}

void
capture_context_start (GimpDrawable *drawable)
{
  gchar label[50];
  if (drawable != GIMP_DRAWABLE (observed)) return;
  ++starts;
  capture_roi (drawable);
  g_snprintf (label, sizeof label, "start%d", starts);
  dump_drawable (drawable, label);
  if (starts == 1) dump_drawable (drawable, "input");
  g_snprintf (label, sizeof label, "start%d-mask", starts);
  dump_drawable (GIMP_DRAWABLE (gimp_image_get_mask (gimp_item_get_image (GIMP_ITEM (drawable)))), label);
  fprintf (events, "%s\tstart\t%d\t%d\t0\n", scenario, starts, ends);
  fflush (events);
}

void
capture_context_end (GimpDrawable *drawable)
{
  if (drawable != GIMP_DRAWABLE (observed)) return;
  ++ends;
  fprintf (events, "%s\tend\t%d\t%d\t0\n", scenario, starts, ends);
  fflush (events);
}

static void
capture_image_mask (GimpImage *image, const gchar *label)
{
  dump_drawable (GIMP_DRAWABLE (gimp_image_get_mask (image)), label);
}

void
capture_context_progress (Gimp *gimp, gdouble value)
{
  GimpImage *image;
  const CaptureCase *test = active_case;
  if (!observed || !test || mutated || value != 1.0 ||
      (test->final_selection < 0 && test->final_components < 0 && test->final_lock < 0)) return;
  mutated = TRUE;
  image = gimp_item_get_image (GIMP_ITEM (observed));
  fprintf (events, "%s\tprogress-mutate\t%d\t%d\t%.17g\n", scenario, starts, ends, value);
  fflush (events);
  if (test->final_selection >= 0) selection (image, test->final_selection);
  if (test->final_components >= 0)
    set_active (image, test->final_components, gimp_drawable_bytes (GIMP_DRAWABLE (observed)));
  if (test->final_lock >= 0) gimp_layer_set_lock_alpha (observed, test->final_lock, FALSE);
  (void) gimp;
}

static void
settle (GimpImage *image, gint required_ends, gint milliseconds)
{
  gint64 began = g_get_monotonic_time ();
  gint64 deadline = began + 20 * G_USEC_PER_SEC;
  do
    {
      guchar projected[4];
      while (g_main_context_iteration (NULL, FALSE)) {}
      gimp_pickable_flush (GIMP_PICKABLE (gimp_image_get_projection (image)));
      /* The old projection is lazy: flush alone does not visit its tiles.
       * Visit every 64x64 legacy tile, including the final partial tiles, so
       * large live cases cannot start from an incomplete lower projection. */
      for (gint y = 0; y < gimp_image_get_height (image); y += 64)
        for (gint x = 0; x < gimp_image_get_width (image); x += 64)
          g_assert (gimp_pickable_get_pixel_at (GIMP_PICKABLE (gimp_image_get_projection (image)),
                                               x, y, projected));
      g_assert (gimp_pickable_get_pixel_at (GIMP_PICKABLE (gimp_image_get_projection (image)),
                                           gimp_image_get_width (image) - 1,
                                           gimp_image_get_height (image) - 1, projected));
      if (ends >= required_ends &&
          !gimp_filter_layer_capture_pending (GIMP_FILTER_LAYER (observed)) &&
          g_get_monotonic_time () - began >= milliseconds * 1000) return;
      g_usleep (1000);
    }
  while (g_get_monotonic_time () < deadline);
  g_error ("Context capture did not settle: %s starts=%d ends=%d pending=%d",
           scenario, starts, ends, gimp_filter_layer_capture_pending (GIMP_FILTER_LAYER (observed)));
}

static void
fill_pattern (GimpDrawable *drawable, gint variant)
{
  static const guchar alpha[] = { 0, 1, 63, 127, 128, 191, 254, 255 };
  gint width = gimp_item_get_width (GIMP_ITEM (drawable));
  gint height = gimp_item_get_height (GIMP_ITEM (drawable));
  gint channels = gimp_drawable_bytes (drawable), x, y, c;
  guchar *pixels = g_malloc (width * height * channels);
  for (y = 0; y < height; ++y)
    for (x = 0; x < width; ++x)
      for (c = 0; c < channels; ++c)
        pixels[(y * width + x) * channels + c] = c == channels - 1 && channels % 2 == 0 ?
          alpha[(x + 3 * y + variant * 3) % 8] :
          (x * (37 + c * 17) + y * (61 + c * 23) + variant * 97 + c * 51 + 11) % 256;
  write_tiles (gimp_drawable_get_tiles (drawable),
               pixels, width, height, channels);
  g_free (pixels);
}


static GValueArray *
arguments (GimpProcedure *procedure, GimpImage *image, GimpLayer *layer,
           const CaptureCase *test)
{
  GValueArray *args = gimp_procedure_get_arguments (procedure);
  g_assert_cmpuint (args->n_values, ==, 4);
  g_value_set_int (&args->values[0], GIMP_RUN_NONINTERACTIVE);
  gimp_value_set_image (&args->values[1], image);
  gimp_value_set_drawable (&args->values[2], GIMP_DRAWABLE (layer));
  g_value_set_int (&args->values[3], test->percent);
  return args;
}

static void
save_writer_fixture (GimpImage *image, const CaptureCase *test)
{
  gchar *name = g_strconcat (test->id, ".xcf", NULL);
  gchar *path = g_build_filename (directory, name, NULL);
  gchar *uri = g_filename_to_uri (path, NULL, NULL);
  GimpPlugInProcedure *procedure = GIMP_PLUG_IN_PROCEDURE (
    gimp_pdb_lookup_procedure (application->pdb, "gimp-xcf-save"));
  GError *error = NULL;
  g_assert (procedure != NULL);
  g_assert_cmpint (file_save (application, image, NULL, uri, procedure,
                              GIMP_RUN_NONINTERACTIVE, FALSE, FALSE, FALSE, &error),
                   ==, GIMP_PDB_SUCCESS);
  g_assert_no_error (error);
  g_free (name); g_free (path); g_free (uri);
}

static void
capture_case (const CaptureCase *test)
{
  GimpImage *image = gimp_create_image (application, test->image_width, test->image_height,
                                       test->components >= 3 ? GIMP_RGB : GIMP_GRAY, FALSE);
  GimpImageType type = test->components == 1 ? GIMP_GRAY_IMAGE :
                       test->components == 2 ? GIMP_GRAYA_IMAGE :
                       test->components == 3 ? GIMP_RGB_IMAGE : GIMP_RGBA_IMAGE;
  GimpLayer *source = gimp_layer_new (image, test->width, test->height, type,
                                     "sharpen source", 1.0, GIMP_NORMAL_MODE);
  GimpProcedure *procedure = gimp_pdb_lookup_procedure (application->pdb, "plug-in-sharpen");
  GimpLayer *layer = source;
  GValueArray *args, *returns;
  GError *error = NULL;
  gint channels, status = -1; /* Async FilterLayer does not expose a PDB return. */
  g_assert (procedure != NULL);
  g_strlcpy (scenario, test->id, sizeof scenario);
  starts = ends = mutated = merge_index = 0;
  active_case = test;
  g_assert (gimp_image_add_layer (image, source, NULL, 0, FALSE));
  gimp_item_set_offset (GIMP_ITEM (source), test->offset_x, test->offset_y);
  fill_pattern (GIMP_DRAWABLE (source), 0);
  if (test->zero_alpha && test->components % 2 == 0)
    {
      guchar *pixels = read_tiles (gimp_drawable_get_tiles (GIMP_DRAWABLE (source)),
                                  test->width, test->height, test->components);
      gint p;
      for (p = test->components - 1; p < test->width * test->height * test->components;
           p += test->components) pixels[p] = 0;
      write_tiles (gimp_drawable_get_tiles (GIMP_DRAWABLE (source)), pixels,
                    test->width, test->height, test->components);
      g_free (pixels);
    }
  if (test->live)
    {
      layer = gimp_filter_layer_new (image, test->width, test->height,
                                     "sharpen filter", 1.0, GIMP_REPLACE_MODE);
      g_assert (gimp_image_add_layer (image, layer, NULL, 0, FALSE));
      gimp_item_set_offset (GIMP_ITEM (layer), test->offset_x, test->offset_y);
    }
  observed = layer;
  channels = gimp_drawable_bytes (GIMP_DRAWABLE (layer));
  g_assert_cmpint (channels, ==, test->live ? (test->components <= 2 ? 2 : 4) : test->components);
  selection (image, test->selection);
  set_active (image, test->active_components, channels);
  gimp_layer_set_lock_alpha (layer, test->lock_alpha, FALSE);
  args = arguments (procedure, image, layer, test);
  dump_drawable (GIMP_DRAWABLE (source), "source");
  capture_image_mask (image, "start-mask");
  if (test->live)
    {
      GArray *values = g_array_sized_new (FALSE, TRUE, sizeof (GValue), args->n_values);
      gint i;
      g_array_set_clear_func (values, (GDestroyNotify) g_value_unset);
      g_array_set_size (values, args->n_values);
      for (i = 0; i < args->n_values; ++i)
        {
          GValue *value = &g_array_index (values, GValue, i);
          g_value_init (value, G_VALUE_TYPE (&args->values[i]));
          g_value_copy (&args->values[i], value);
        }
      gimp_filter_layer_set_procedure (GIMP_FILTER_LAYER (layer), "plug-in-sharpen", values);
      g_array_unref (values);
      gimp_drawable_update (GIMP_DRAWABLE (source), 0, 0, test->width, test->height);
      settle (image, 1, 30);
      g_assert_cmpint (starts, ==, 1); g_assert_cmpint (ends, ==, 1);
    }
  else
    {
      dump_drawable (GIMP_DRAWABLE (layer), "input");
      capture_roi (GIMP_DRAWABLE (layer));
      returns = gimp_procedure_execute (procedure, application,
                                        gimp_get_user_context (application), NULL, args, &error);
      status = g_value_get_enum (&returns->values[0]);
      g_assert_no_error (error); g_assert_cmpint (status, ==, GIMP_PDB_SUCCESS);
      g_value_array_free (returns);
    }
  g_value_array_free (args);
  if (test->final_selection >= 0 || test->final_components >= 0 || test->final_lock >= 0)
    g_assert_cmpint (mutated, ==, 1);
  dump_drawable (GIMP_DRAWABLE (layer), "output");
  capture_image_mask (image, "final-mask");
  if (test->writer) save_writer_fixture (image, test);
  if (test->rerun)
    {
      /* Real lower-input edit, retaining the now-current owner context. */
      active_case = NULL;
      fill_pattern (GIMP_DRAWABLE (source), 1);
      dump_drawable (GIMP_DRAWABLE (source), "source2");
      gimp_drawable_update (GIMP_DRAWABLE (source), 0, 0, test->width, test->height);
      settle (image, 2, 30);
      g_assert_cmpint (starts, ==, 2); g_assert_cmpint (ends, ==, 2);
      dump_drawable (GIMP_DRAWABLE (layer), "output2");
    }
  fprintf (results, "%s\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n",
           scenario, test->live, test->components, channels, test->width, test->height,
           test->image_width, test->image_height, test->offset_x, test->offset_y, test->percent,
           test->selection, test->final_selection, test->active_components, test->final_components,
           test->lock_alpha, test->final_lock, test->rerun, test->writer,
           starts, ends, merge_index, status, mutated);
  fflush (results);
  g_print ("SHARPEN_CASE_DONE=%s status=%d merges=%d\n", scenario, status, merge_index);
  active_case = NULL; observed = NULL;
  g_object_unref (image);
}

int
main (int argc, char **argv)
{
  gchar *path;
  gint i;
  GimpProcedure *procedure;
  FILE *signature;
  g_assert_cmpint (argc, ==, 2);
  directory = argv[1];
  g_mkdir_with_parents (directory, 0700);
  path = g_build_filename (directory, "merges.tsv", NULL); merges = fopen (path, "w"); g_free (path);
  path = g_build_filename (directory, "bounds.tsv", NULL); bounds = fopen (path, "w"); g_free (path);
  path = g_build_filename (directory, "events.tsv", NULL); events = fopen (path, "w"); g_free (path);
  path = g_build_filename (directory, "captures.tsv", NULL); results = fopen (path, "w"); g_free (path);
  path = g_build_filename (directory, "rois.tsv", NULL); rois = fopen (path, "w"); g_free (path);
  g_assert (merges && bounds && events && results && rois);
  g_thread_init (NULL); g_type_init ();
  application = gimp_init_for_testing ();
  procedure = gimp_pdb_lookup_procedure (application->pdb, "plug-in-sharpen");
  g_assert (procedure != NULL);
  path = g_build_filename (directory, "signature.tsv", NULL); signature = fopen (path, "w"); g_free (path);
  for (i = 0; i < procedure->num_args; ++i)
    fprintf (signature, "%d\t%s\t%s\t%s\n", i,
             g_param_spec_get_name (procedure->args[i]),
             g_type_name (G_PARAM_SPEC_TYPE (procedure->args[i])),
             g_type_name (G_PARAM_SPEC_VALUE_TYPE (procedure->args[i])));
  fclose (signature);
  for (i = 0; i < G_N_ELEMENTS (capture_cases); ++i) capture_case (&capture_cases[i]);
  fclose (merges); fclose (bounds); fclose (events); fclose (results); fclose (rois);
  g_print ("LEGACY_SHARPEN_CAPTURE_COMPLETE\n");
  return 0;
}
