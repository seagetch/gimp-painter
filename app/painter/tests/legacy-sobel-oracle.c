/* SPDX-License-Identifier: GPL-3.0-or-later
 * Test-only source-extracted C oracle, NOT an original-runtime capture.
 * Original Sobel plug-in by Thorsten Schnier, based on GIMP blur filters.
 * Source: afa43fae3e920210146abed514f136fd49f671b5:plug-ins/common/edge-sobel.c
 * SHA256: f37f08e206559a1948977b3a2e688fc5ed19fa3c442b1766634ac5b6ed7d5402
 * ROUND source: same commit, libgimpmath/gimpmath.h
 * SHA256: 5be55b44288b5d3bac28e9fc51e5003af07734dc41b098f137e35ef6188f3ffe
 *
 * The marked row preparation/RMS/complete sobel body and ROUND macro are
 * byte-identical source. Adapters supply a full-drawable byte pixel region,
 * a captured ROI, memory allocation, and inert progress/merge/update hooks.
 * Preview is deliberately unreachable and its stubs abort if invoked. The
 * oracle invokes the non-preview calculation, records its raw shadow writes
 * into zero-initialized memory, and DOES NOT emulate final shadow merging.
 * Original row-sized allocations are retained here solely in test code.
 * No GIMP/GEGL/GTK objects or actual application process are used.
 * Global drawable state mirrors the original: sequential test calls only.
 */
#include "legacy-sobel-oracle.h"
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#if INT_MAX != INT32_MAX
#error The source oracle requires the original 32-bit int arithmetic
#endif

typedef int gint;
typedef int gboolean;
typedef unsigned char guchar;
typedef double gdouble;
#define TRUE 1
#define FALSE 0
#define _(text) (text)
#define CLAMP(value, low, high) ((value) < (low) ? (low) : (value) > (high) ? (high) : (value))
/* BEGIN EXACT LEGACY ROUND */
#define ROUND(x) ((int) ((x) + 0.5))
/* END EXACT LEGACY ROUND */
#define g_new(type, count) ((type *) oracle_allocate (sizeof (type) * (size_t) (count)))
#define g_free(pointer) free (pointer)

static void *
oracle_allocate (size_t size)
{
  void *result = malloc (size);
  if (!result) abort ();
  return result;
}

typedef struct
{
  gint drawable_id;
  gint width, height, bpp;
  gint roi_x, roi_y, roi_width, roi_height;
  const guchar *input;
  guchar *output;
} GimpDrawable;
typedef struct { GimpDrawable *drawable; gint h, bpp; } GimpPixelRgn;
typedef struct { int unreachable; } GimpPreview;
static GimpDrawable *active_drawable;

static gint
gimp_drawable_mask_intersect (gint id, gint *x, gint *y, gint *w, gint *h)
{
  (void) id;
  *x = active_drawable->roi_x; *y = active_drawable->roi_y;
  *w = active_drawable->roi_width; *h = active_drawable->roi_height;
  return *w > 0 && *h > 0;
}
static gboolean gimp_drawable_has_alpha (gint id)
{ (void) id; return active_drawable->bpp == 2 || active_drawable->bpp == 4; }
static void
gimp_pixel_rgn_init (GimpPixelRgn *region, GimpDrawable *drawable,
                     gint x, gint y, gint w, gint h, gint dirty, gint shadow)
{
  (void) x; (void) y; (void) w; (void) h; (void) dirty; (void) shadow;
  region->drawable = drawable; region->h = drawable->height; region->bpp = drawable->bpp;
}
static void
gimp_pixel_rgn_get_row (GimpPixelRgn *region, guchar *row,
                        gint x, gint y, gint width)
{
  const GimpDrawable *drawable = region->drawable;
  memcpy (row, drawable->input + ((size_t) y * drawable->width + x) * drawable->bpp,
          (size_t) width * drawable->bpp);
}
static void
gimp_pixel_rgn_set_row (GimpPixelRgn *region, const guchar *row,
                        gint x, gint y, gint width)
{
  const GimpDrawable *drawable = region->drawable;
  memcpy (drawable->output + ((size_t) y * drawable->width + x) * drawable->bpp, row,
          (size_t) width * drawable->bpp);
}
static void gimp_progress_init (const char *message) { (void) message; }
static void gimp_progress_update (gdouble value) { (void) value; }
static void gimp_drawable_flush (GimpDrawable *drawable) { (void) drawable; }
static void gimp_drawable_merge_shadow (gint id, gint undo) { (void) id; (void) undo; }
static void gimp_drawable_update (gint id, gint x, gint y, gint w, gint h)
{ (void) id; (void) x; (void) y; (void) w; (void) h; }
static void gimp_preview_get_position (GimpPreview *p, gint *x, gint *y)
{ (void) p; (void) x; (void) y; abort (); }
static void gimp_preview_get_size (GimpPreview *p, gint *w, gint *h)
{ (void) p; (void) w; (void) h; abort (); }
static void gimp_preview_draw_buffer (GimpPreview *p, const guchar *data, gint stride)
{ (void) p; (void) data; (void) stride; abort (); }

/* BEGIN EXACT LEGACY ROW PREPARATION AND SOBEL */
static void
sobel_prepare_row (GimpPixelRgn *pixel_rgn,
                   guchar       *data,
                   gint          x,
                   gint          y,
                   gint          w)
{
  gint b;

  y = CLAMP (y, 0, pixel_rgn->h - 1);
  gimp_pixel_rgn_get_row (pixel_rgn, data, x, y, w);

  /*  Fill in edge pixels  */
  for (b = 0; b < pixel_rgn->bpp; b++)
    {
      data[-(int)pixel_rgn->bpp + b] = data[b];
      data[w * pixel_rgn->bpp + b] = data[(w - 1) * pixel_rgn->bpp + b];
    }
}

#define RMS(a, b) (sqrt ((a) * (a) + (b) * (b)))

static void
sobel (GimpDrawable *drawable,
       gboolean      do_horizontal,
       gboolean      do_vertical,
       gboolean      keep_sign,
       GimpPreview  *preview)
{
  GimpPixelRgn  srcPR, destPR;
  gint          width, height;
  gint          bytes;
  gint          gradient, hor_gradient, ver_gradient;
  guchar       *dest, *d;
  guchar       *prev_row, *pr;
  guchar       *cur_row, *cr;
  guchar       *next_row, *nr;
  guchar       *tmp;
  gint          row, col;
  gint          x, y;
  gboolean      alpha;
  gint          counter;
  guchar       *preview_buffer = NULL;

  if (preview)
    {
      gimp_preview_get_position (preview, &x, &y);
      gimp_preview_get_size (preview, &width, &height);
    }
  else
    {
      if (! gimp_drawable_mask_intersect (drawable->drawable_id,
                                          &x, &y, &width, &height))
        return;

      gimp_progress_init (_("Sobel edge detecting"));
    }

  /* Get the size of the input image. (This will/must be the same
   *  as the size of the output image.
   */
  bytes  = drawable->bpp;
  alpha  = gimp_drawable_has_alpha (drawable->drawable_id);

  /*  allocate row buffers  */
  prev_row = g_new (guchar, (width + 2) * bytes);
  cur_row  = g_new (guchar, (width + 2) * bytes);
  next_row = g_new (guchar, (width + 2) * bytes);
  dest     = g_new (guchar, width * bytes);

  /*  initialize the pixel regions  */
  gimp_pixel_rgn_init (&srcPR, drawable, 0, 0,
                       drawable->width, drawable->height,
                       FALSE, FALSE);

  if (preview)
    {
      preview_buffer = g_new (guchar, width * height * bytes);
    }
  else
    {
      gimp_pixel_rgn_init (&destPR, drawable, 0, 0,
                           drawable->width, drawable->height,
                           TRUE, TRUE);
    }

  pr = prev_row + bytes;
  cr = cur_row  + bytes;
  nr = next_row + bytes;

  sobel_prepare_row (&srcPR, pr, x, y - 1, width);
  sobel_prepare_row (&srcPR, cr, x, y, width);
  counter =0;
  /*  loop through the rows, applying the sobel convolution  */
  for (row = y; row < y + height; row++)
    {
      /*  prepare the next row  */
      sobel_prepare_row (&srcPR, nr, x, row + 1, width);

      d = dest;
      for (col = 0; col < width * bytes; col++)
        {
          hor_gradient = (do_horizontal ?
                          ((pr[col - bytes] +  2 * pr[col] + pr[col + bytes]) -
                           (nr[col - bytes] + 2 * nr[col] + nr[col + bytes]))
                          : 0);
          ver_gradient = (do_vertical ?
                          ((pr[col - bytes] + 2 * cr[col - bytes] + nr[col - bytes]) -
                           (pr[col + bytes] + 2 * cr[col + bytes] + nr[col + bytes]))
                          : 0);
          gradient = (do_vertical && do_horizontal) ?
            (ROUND (RMS (hor_gradient, ver_gradient)) / 5.66) /* always >0 */
            : (keep_sign ? (127 + (ROUND ((hor_gradient + ver_gradient) / 8.0)))
               : (ROUND (abs (hor_gradient + ver_gradient) / 4.0)));

          if (alpha && (((col + 1) % bytes) == 0))
            { /* the alpha channel */
              *d++ = (counter == 0) ? 0 : 255;
              counter = 0;
            }
          else
            {
              *d++ = gradient;
              if (gradient > 10) counter ++;
            }
        }
      /*  shuffle the row pointers  */
      tmp = pr;
      pr = cr;
      cr = nr;
      nr = tmp;

      /*  store the dest  */
      if (preview)
        {
          memcpy (preview_buffer + width * (row - y) * bytes,
                  dest,
                  width * bytes);
        }
      else
        {
          gimp_pixel_rgn_set_row (&destPR, dest, x, row, width);

          if ((row % 20) == 0)
            gimp_progress_update ((double) row / (double) height);
        }
    }

  if (preview)
    {
      gimp_preview_draw_buffer (preview, preview_buffer, width * bytes);
      g_free (preview_buffer);
    }
  else
    {
      gimp_progress_update (1.0);
      /*  update the sobeled region  */
      gimp_drawable_flush (drawable);
      gimp_drawable_merge_shadow (drawable->drawable_id, TRUE);
      gimp_drawable_update (drawable->drawable_id, x, y, width, height);
    }

  g_free (prev_row);
  g_free (cur_row);
  g_free (next_row);
  g_free (dest);
}
/* END EXACT LEGACY ROW PREPARATION AND SOBEL */

int
legacy_sobel_oracle (const uint8_t *input, size_t width, size_t height,
                     unsigned channels, size_t x, size_t y,
                     size_t roi_width, size_t roi_height,
                     int32_t horizontal, int32_t vertical, int32_t keep_sign,
                     uint8_t *output)
{
  GimpDrawable drawable;
  size_t extent;
  if (!input || !output || input == output || !width || !height ||
      width > INT_MAX || height > INT_MAX || channels < 1 || channels > 4 ||
      width > SIZE_MAX / channels / height || x > width || y > height ||
      roi_width > width - x || roi_height > height - y)
    return 0;
  /* Preserve defined original signed row-buffer arithmetic. For native inputs
   * without alpha, counter never resets: bound its possible total increments,
   * independently of the fixture's bytes and flags. The production RGBA
   * transport resets after each pixel and has no corresponding area bound. */
  if (roi_width && roi_height &&
      (roi_width > (size_t) (INT_MAX / channels) - 2 ||
       ((channels == 1 || channels == 3) &&
        roi_width > (size_t) INT_MAX / channels / roi_height)))
    return 0;
  extent = width * height * channels;
  memset (output, 0, extent);
  drawable.drawable_id = 1;
  drawable.width = (gint) width; drawable.height = (gint) height;
  drawable.bpp = (gint) channels;
  drawable.roi_x = (gint) x; drawable.roi_y = (gint) y;
  drawable.roi_width = (gint) roi_width; drawable.roi_height = (gint) roi_height;
  drawable.input = input; drawable.output = output;
  active_drawable = &drawable;
  /* Same truth-value normalization as noninteractive run(): raw INT32 flags
   * are retained by the production API and interpreted here as nonzero. */
  sobel (&drawable, horizontal ? TRUE : FALSE, vertical ? TRUE : FALSE,
         keep_sign ? TRUE : FALSE, NULL);
  active_drawable = NULL;
  return 1;
}
