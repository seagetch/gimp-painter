/* SPDX-License-Identifier: GPL-3.0-or-later
 * Test-only source-extracted legacy oracle, not an old-runtime capture.
 * Original sharpen code copyright 1997-1998 Michael Sweet (mike@easysw.com).
 * Source: afa43fae3e920210146abed514f136fd49f671b5:plug-ins/common/sharpen.c
 * SHA256: 6b16f3d3ee6722157bad2772d1cfdee8e3db8e7ef7fb350ae2cbee8ee832312b
 *
 * The marked compute_luts/sharpen/row-filter bodies are unchanged source.
 * Only GIMP region access, allocation and progress plumbing are stubbed.
 * No GIMP/GEGL/GTK objects or native application process are used. The
 * harness captures raw shadow writes; it does not simulate shadow merge.
 * Global state mirrors the original plug-in: invoke sequentially only.
 */
#include "legacy-sharpen-oracle.h"
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#if INT_MAX != INT32_MAX
#error The source oracle requires the original 32-bit int arithmetic
#endif
/* Signed negative shifts in the old source require an arithmetic-shift host.
 * Fail visibly rather than silently claiming parity on another C behavior. */
#if (-1 >> 3) != -1
#error The source oracle requires arithmetic signed right shift
#endif

typedef int gint;
typedef int32_t gint32;
typedef unsigned char guchar;
typedef double gdouble;
typedef gint32 intneg;
typedef gint32 intpos;
#define TRUE 1
#define FALSE 0
#define _(text) (text)
#define CLAMP0255(a) ((a) < 0 ? 0 : (a) > 255 ? 255 : (a))
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
  gint width, height, channels;
  const guchar *input;
  guchar *output;
} GimpDrawable;
typedef struct { GimpDrawable *drawable; } GimpPixelRgn;
static GimpDrawable *active_drawable;
static struct { gint sharpen_percent; } sharpen_params;
static intneg neg_lut[256];
static intpos pos_lut[256];

static gint
gimp_drawable_mask_intersect (gint id, gint *x, gint *y, gint *w, gint *h)
{
  (void) id;
  *x = *y = 0;
  *w = active_drawable->width;
  *h = active_drawable->height;
  return TRUE;
}
static gint gimp_drawable_bpp (gint id)
{ (void) id; return active_drawable->channels; }
static void
gimp_pixel_rgn_init (GimpPixelRgn *region, GimpDrawable *drawable,
                     gint x, gint y, gint w, gint h, gint dirty, gint shadow)
{
  (void) x; (void) y; (void) w; (void) h; (void) dirty; (void) shadow;
  region->drawable = drawable;
}
static void
gimp_pixel_rgn_get_row (GimpPixelRgn *region, guchar *row,
                        gint x, gint y, gint width)
{
  const GimpDrawable *drawable = region->drawable;
  memcpy (row, drawable->input + ((size_t) y * drawable->width + x) * drawable->channels,
          (size_t) width * drawable->channels);
}
static void
gimp_pixel_rgn_set_row (GimpPixelRgn *region, const guchar *row,
                        gint x, gint y, gint width)
{
  const GimpDrawable *drawable = region->drawable;
  memcpy (drawable->output + ((size_t) y * drawable->width + x) * drawable->channels, row,
          (size_t) width * drawable->channels);
}
static void gimp_progress_init (const char *message) { (void) message; }
static void gimp_progress_update (gdouble value) { (void) value; }
static void gimp_drawable_flush (GimpDrawable *drawable) { (void) drawable; }
static void gimp_drawable_merge_shadow (gint id, gint undo) { (void) id; (void) undo; }
static void gimp_drawable_update (gint id, gint x, gint y, gint w, gint h)
{ (void) id; (void) x; (void) y; (void) w; (void) h; }

static void gray_filter (int, guchar *, guchar *, intneg *, intneg *, intneg *);
static void graya_filter (int, guchar *, guchar *, intneg *, intneg *, intneg *);
static void rgb_filter (int, guchar *, guchar *, intneg *, intneg *, intneg *);
static void rgba_filter (int, guchar *, guchar *, intneg *, intneg *, intneg *);

/* BEGIN EXACT LEGACY LUT AND SHARPEN */
static void
compute_luts (void)
{
  gint i;       /* Looping var */
  gint fact;    /* 1 - sharpness */

  fact = 100 - sharpen_params.sharpen_percent;
  if (fact < 1)
    fact = 1;

  for (i = 0; i < 256; i ++)
    {
      pos_lut[i] = 800 * i / fact;
      neg_lut[i] = (4 + pos_lut[i] - (i << 3)) >> 3;
    }
}

/*
 * 'sharpen()' - Sharpen an image using a convolution filter.
 */

static void
sharpen (GimpDrawable *drawable)
{
  GimpPixelRgn  src_rgn;        /* Source image region */
  GimpPixelRgn  dst_rgn;        /* Destination image region */
  guchar       *src_rows[4];    /* Source pixel rows */
  guchar       *src_ptr;        /* Current source pixel */
  guchar       *dst_row;        /* Destination pixel row */
  intneg       *neg_rows[4];    /* Negative coefficient rows */
  intneg       *neg_ptr;        /* Current negative coefficient */
  gint          i;              /* Looping vars */
  gint          y;              /* Current location in image */
  gint          row;            /* Current row in src_rows */
  gint          count;          /* Current number of filled src_rows */
  gint          width;          /* Byte width of the image */
  gint          x1;             /* Selection bounds */
  gint          y1;
  gint          y2;
  gint          sel_width;      /* Selection width */
  gint          sel_height;     /* Selection height */
  gint          img_bpp;        /* Bytes-per-pixel in image */
  void          (*filter)(int, guchar *, guchar *, intneg *, intneg *, intneg *);

  filter = NULL;

  if (! gimp_drawable_mask_intersect (drawable->drawable_id,
                                      &x1, &y1, &sel_width, &sel_height))
    return;

  y2 = y1 + sel_height;

  img_bpp = gimp_drawable_bpp (drawable->drawable_id);

  /*
   * Let the user know what we're doing...
   */
  gimp_progress_init (_("Sharpening"));

  /*
   * Setup for filter...
   */

  gimp_pixel_rgn_init (&src_rgn, drawable,
                       x1, y1, sel_width, sel_height, FALSE, FALSE);
  gimp_pixel_rgn_init (&dst_rgn, drawable,
                       x1, y1, sel_width, sel_height, TRUE, TRUE);

  compute_luts ();

  width = sel_width * img_bpp;

  for (row = 0; row < 4; row ++)
    {
      src_rows[row] = g_new (guchar, width);
      neg_rows[row] = g_new (intneg, width);
    }

  dst_row = g_new (guchar, width);

  /*
   * Pre-load the first row for the filter...
   */

  gimp_pixel_rgn_get_row (&src_rgn, src_rows[0], x1, y1, sel_width);

  for (i = width, src_ptr = src_rows[0], neg_ptr = neg_rows[0];
       i > 0;
       i --, src_ptr ++, neg_ptr ++)
    *neg_ptr = neg_lut[*src_ptr];

  row   = 1;
  count = 1;

  /*
   * Select the filter...
   */

  switch (img_bpp)
    {
    case 1 :
      filter = gray_filter;
      break;
    case 2 :
      filter = graya_filter;
      break;
    case 3 :
      filter = rgb_filter;
      break;
    case 4 :
      filter = rgba_filter;
      break;
    };

  /*
   * Sharpen...
   */

  for (y = y1; y < y2; y ++)
    {
      /*
       * Load the next pixel row...
       */

      if ((y + 1) < y2)
        {
          /*
           * Check to see if our src_rows[] array is overflowing yet...
           */

          if (count >= 3)
            count --;

          /*
           * Grab the next row...
           */

          gimp_pixel_rgn_get_row (&src_rgn, src_rows[row],
                                  x1, y + 1, sel_width);
          for (i = width, src_ptr = src_rows[row], neg_ptr = neg_rows[row];
               i > 0;
               i --, src_ptr ++, neg_ptr ++)
            *neg_ptr = neg_lut[*src_ptr];

          count ++;
          row = (row + 1) & 3;
        }
      else
        {
          /*
           * No more pixels at the bottom...  Drop the oldest samples...
           */

          count --;
        }

      /*
       * Now sharpen pixels and save the results...
       */

      if (count == 3)
        {
          (* filter) (sel_width, src_rows[(row + 2) & 3], dst_row,
                      neg_rows[(row + 1) & 3] + img_bpp,
                      neg_rows[(row + 2) & 3] + img_bpp,
                      neg_rows[(row + 3) & 3] + img_bpp);

          /*
           * Set the row...
           */

          gimp_pixel_rgn_set_row (&dst_rgn, dst_row, x1, y, sel_width);
        }
      else if (count == 2)
        {
          if (y == y1)      /* first row */
            gimp_pixel_rgn_set_row (&dst_rgn, src_rows[0],
                                    x1, y, sel_width);
          else                  /* last row  */
            gimp_pixel_rgn_set_row (&dst_rgn, src_rows[(sel_height - 1) & 3],
                                    x1, y, sel_width);
        }

      if ((y & 15) == 0)
        gimp_progress_update ((gdouble) (y - y1) / (gdouble) sel_height);
    }

  /*
   * OK, we're done.  Free all memory used...
   */

  for (row = 0; row < 4; row ++)
    {
      g_free (src_rows[row]);
      g_free (neg_rows[row]);
    }

  g_free (dst_row);

  /*
   * Update the screen...
   */

  gimp_progress_update (1.0);
  gimp_drawable_flush (drawable);
  gimp_drawable_merge_shadow (drawable->drawable_id, TRUE);
  gimp_drawable_update (drawable->drawable_id,
                        x1, y1, sel_width, sel_height);
}

/* END EXACT LEGACY LUT AND SHARPEN */

/* BEGIN EXACT LEGACY ROW FILTERS */
static void
gray_filter (gint    width,     /* I - Width of line in pixels */
             guchar *src,       /* I - Source line */
             guchar *dst,       /* O - Destination line */
             intneg *neg0,      /* I - Top negative coefficient line */
             intneg *neg1,      /* I - Middle negative coefficient line */
             intneg *neg2)      /* I - Bottom negative coefficient line */
{
  intpos pixel;         /* New pixel value */

  *dst++ = *src++;
  width -= 2;

  while (width > 0)
    {
      pixel = (pos_lut[*src++] - neg0[-1] - neg0[0] - neg0[1] -
               neg1[-1] - neg1[1] -
               neg2[-1] - neg2[0] - neg2[1]);
      pixel = (pixel + 4) >> 3;
      *dst++ = CLAMP0255 (pixel);

      neg0 ++;
      neg1 ++;
      neg2 ++;
      width --;
    }

  *dst++ = *src++;
}

/*
 * 'graya_filter()' - Sharpen grayscale+alpha pixels.
 */

static void
graya_filter (gint   width,     /* I - Width of line in pixels */
              guchar *src,      /* I - Source line */
              guchar *dst,      /* O - Destination line */
              intneg *neg0,     /* I - Top negative coefficient line */
              intneg *neg1,     /* I - Middle negative coefficient line */
              intneg *neg2)     /* I - Bottom negative coefficient line */
{
  intpos pixel;         /* New pixel value */

  *dst++ = *src++;
  *dst++ = *src++;
  width -= 2;

  while (width > 0)
    {
      pixel = (pos_lut[*src++] - neg0[-2] - neg0[0] - neg0[2] -
               neg1[-2] - neg1[2] -
               neg2[-2] - neg2[0] - neg2[2]);
      pixel = (pixel + 4) >> 3;
      *dst++ = CLAMP0255 (pixel);

      *dst++ = *src++;
      neg0 += 2;
      neg1 += 2;
      neg2 += 2;
      width --;
    }

  *dst++ = *src++;
  *dst++ = *src++;
}

/*
 * 'rgb_filter()' - Sharpen RGB pixels.
 */

static void
rgb_filter (gint    width,      /* I - Width of line in pixels */
            guchar *src,        /* I - Source line */
            guchar *dst,        /* O - Destination line */
            intneg *neg0,       /* I - Top negative coefficient line */
            intneg *neg1,       /* I - Middle negative coefficient line */
            intneg *neg2)       /* I - Bottom negative coefficient line */
{
  intpos pixel;         /* New pixel value */

  *dst++ = *src++;
  *dst++ = *src++;
  *dst++ = *src++;
  width -= 2;

  while (width > 0)
    {
      pixel = (pos_lut[*src++] - neg0[-3] - neg0[0] - neg0[3] -
               neg1[-3] - neg1[3] -
               neg2[-3] - neg2[0] - neg2[3]);
      pixel = (pixel + 4) >> 3;
      *dst++ = CLAMP0255 (pixel);

      pixel = (pos_lut[*src++] - neg0[-2] - neg0[1] - neg0[4] -
               neg1[-2] - neg1[4] -
               neg2[-2] - neg2[1] - neg2[4]);
      pixel = (pixel + 4) >> 3;
      *dst++ = CLAMP0255 (pixel);

      pixel = (pos_lut[*src++] - neg0[-1] - neg0[2] - neg0[5] -
               neg1[-1] - neg1[5] -
               neg2[-1] - neg2[2] - neg2[5]);
      pixel = (pixel + 4) >> 3;
      *dst++ = CLAMP0255 (pixel);

      neg0 += 3;
      neg1 += 3;
      neg2 += 3;
      width --;
    }

  *dst++ = *src++;
  *dst++ = *src++;
  *dst++ = *src++;
}

/*
 * 'rgba_filter()' - Sharpen RGBA pixels.
 */

static void
rgba_filter (gint   width,      /* I - Width of line in pixels */
             guchar *src,       /* I - Source line */
             guchar *dst,       /* O - Destination line */
             intneg *neg0,      /* I - Top negative coefficient line */
             intneg *neg1,      /* I - Middle negative coefficient line */
             intneg *neg2)      /* I - Bottom negative coefficient line */
{
  intpos pixel;         /* New pixel value */

  *dst++ = *src++;
  *dst++ = *src++;
  *dst++ = *src++;
  *dst++ = *src++;
  width -= 2;

  while (width > 0)
    {
      pixel = (pos_lut[*src++] - neg0[-4] - neg0[0] - neg0[4] -
               neg1[-4] - neg1[4] -
               neg2[-4] - neg2[0] - neg2[4]);
      pixel = (pixel + 4) >> 3;
      *dst++ = CLAMP0255 (pixel);

      pixel = (pos_lut[*src++] - neg0[-3] - neg0[1] - neg0[5] -
               neg1[-3] - neg1[5] -
               neg2[-3] - neg2[1] - neg2[5]);
      pixel = (pixel + 4) >> 3;
      *dst++ = CLAMP0255 (pixel);

      pixel = (pos_lut[*src++] - neg0[-2] - neg0[2] - neg0[6] -
               neg1[-2] - neg1[6] -
               neg2[-2] - neg2[2] - neg2[6]);
      pixel = (pixel + 4) >> 3;
      *dst++ = CLAMP0255 (pixel);

      *dst++ = *src++;

      neg0 += 4;
      neg1 += 4;
      neg2 += 4;
      width --;
    }

  *dst++ = *src++;
  *dst++ = *src++;
  *dst++ = *src++;
  *dst++ = *src++;
}
/* END EXACT LEGACY ROW FILTERS */

int
legacy_sharpen_oracle (const uint8_t *input, size_t width, size_t height,
                       unsigned channels, int32_t percent, uint8_t *output)
{
  GimpDrawable drawable;
  if (!input || !output || input == output || channels < 1 || channels > 4 ||
      width < 2 || height < 3 || width > (size_t) INT32_MAX / channels ||
      height > INT32_MAX || width > SIZE_MAX / channels / height ||
      percent < 100 - INT32_MAX)
    return 0;
  drawable.drawable_id = 1;
  drawable.width = (gint) width;
  drawable.height = (gint) height;
  drawable.channels = (gint) channels;
  drawable.input = input;
  drawable.output = output;
  active_drawable = &drawable;
  sharpen_params.sharpen_percent = percent;
  sharpen (&drawable);
  active_drawable = NULL;
  return 1;
}
