/* SPDX-License-Identifier: GPL-3.0-or-later
 * Test-only source-extracted C oracle, NOT an original-runtime capture.
 * Pinned source: afa43fae3e920210146abed514f136fd49f671b5
 * plug-ins/common/edge.c SHA256:
 * 38512426b7145167e7e0b220164bd9cecee70f9b754276ad0d006adc1a18234b
 * libgimp/gimppixelfetcher.c SHA256:
 * da851302005ad64b8518c90d2bb9ab17fcd3052d7ebc4c492fdbd8f511ddbcf3
 * libgimpmath/gimpmath.h SHA256:
 * 5be55b44288b5d3bac28e9fc51e5003af07734dc41b098f137e35ef6188f3ffe
 *
 * Marked blocks preserve edge.c:255-393 and :402-596, pixel-fetcher.c:190-260,
 * and gimpmath.h:118 byte for byte. Original notices are retained below.
 * The adapters provide one memory-backed pixel region for the entire ROI,
 * the original fetcher's full-drawable boundary rules, captured mask bounds,
 * and inert flush/merge/update hooks. Full image copies with one padded row
 * make the literal loop's post-last-row pointer increments well-defined.
 * Original algorithm arithmetic and kernel order are not rewritten here.
 * The output is raw native-format shadow, preserving caller initialization
 * outside the ROI. No selection merge, app objects, or production detector
 * is used. Global state mirrors the plug-in: sequential test calls only.
 */
/* edge filter for GIMP
 *  -Peter Mattis
 *
 * This filter performs edge detection on the input image.
 *  The code for this filter is based on "pgmedge", a program
 *  that is part of the netpbm package.
 *
 * GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/* pgmedge.c - edge-detect a portable graymap
 *
 * Copyright (C) 1989 by Jef Poskanzer.
 */

/*
 *  Ported to GIMP Plug-in API 1.0
 *  version 1.07
 *  This version requires GIMP v0.99.10 or above.
 *
 *  This plug-in performs edge detection. The code is based on edge.c
 *  for GIMP 0.54 by Peter Mattis.
 *
 *      Eiichi Takamori <taka@ma1.seikyou.ne.jp>
 *      http://ha1.seikyou.ne.jp/home/taka/gimp/
 *
 *  Tips: you can enter arbitrary value into entry.
 *      (not bounded between 1.0 and 10.0)
 */

/*  29 July 2003   Dave Neary  <bolsh@gimp.org>
 *  Added more edge detection routines, from the thin_line
 *  plug-in by iccii
 */

/* LIBGIMP - The GIMP Library
 * Copyright (C) 1995-1997 Peter Mattis and Spencer Kimball
 *
 * gimppixelfetcher.c
 *
 * This library is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library.  If not, see
 * <http://www.gnu.org/licenses/>.
 */

#include "legacy-edge-shadow-oracle.h"
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
typedef void *gpointer;
#define TRUE 1
#define FALSE 0
#define CLAMP(value, low, high) ((value) < (low) ? (low) : (value) > (high) ? (high) : (value))
#define g_return_if_fail(condition) do { if (!(condition)) abort (); } while (0)
/* BEGIN EXACT LEGACY CLAMP0255 */
#define CLAMP0255(a)  CLAMP(a,0,255)
/* END EXACT LEGACY CLAMP0255 */

enum { SOBEL, PREWITT, GRADIENT, ROBERTS, DIFFERENTIAL, LAPLACE };
enum
{
  GIMP_PIXEL_FETCHER_EDGE_NONE,
  GIMP_PIXEL_FETCHER_EDGE_WRAP,
  GIMP_PIXEL_FETCHER_EDGE_SMEAR,
  GIMP_PIXEL_FETCHER_EDGE_BLACK,
  GIMP_PIXEL_FETCHER_EDGE_BACKGROUND
};
typedef struct { gdouble amount; gint edgemode, wrapmode; } EdgeVals;
static EdgeVals evals;

typedef struct
{
  gint drawable_id;
  gint width, height, bpp;
  gint roi_x, roi_y, roi_width, roi_height;
  guchar *input;
  guchar *output;
} GimpDrawable;

typedef struct
{
  gint x, y, w, h, bpp, rowstride;
  guchar *data;
} GimpPixelRgn;

typedef struct
{
  gint img_width, img_height, img_bpp;
  gint sel_x1, sel_y1, sel_x2, sel_y2;
  gint mode;
  guchar bg_color[4];
  GimpDrawable *drawable;
} GimpPixelFetcher;

static GimpDrawable *active_drawable;
static GimpPixelFetcher memory_fetcher;
static unsigned flush_count, merge_count, update_count;

static void
gimp_drawable_mask_bounds (gint id, gint *x1, gint *y1, gint *x2, gint *y2)
{
  (void) id;
  *x1 = active_drawable->roi_x;
  *y1 = active_drawable->roi_y;
  *x2 = *x1 + active_drawable->roi_width;
  *y2 = *y1 + active_drawable->roi_height;
}

static gint gimp_drawable_bpp (gint id)
{ (void) id; return active_drawable->bpp; }
static gboolean gimp_drawable_has_alpha (gint id)
{ (void) id; return active_drawable->bpp == 2 || active_drawable->bpp == 4; }

static GimpPixelFetcher *
gimp_pixel_fetcher_new (GimpDrawable *drawable, gboolean shadow)
{
  GimpPixelFetcher *pf = &memory_fetcher;
  (void) shadow;
  memset (pf, 0, sizeof (*pf));
  pf->drawable = drawable;
  pf->img_width = drawable->width;
  pf->img_height = drawable->height;
  pf->img_bpp = drawable->bpp;
  pf->bg_color[3] = 255;
  gimp_drawable_mask_bounds (drawable->drawable_id,
                            &pf->sel_x1, &pf->sel_y1, &pf->sel_x2, &pf->sel_y2);
  return pf;
}

static void
gimp_pixel_fetcher_set_edge_mode (GimpPixelFetcher *pf, gint mode)
{ pf->mode = mode; }
static void gimp_pixel_fetcher_destroy (GimpPixelFetcher *pf) { (void) pf; }

static guchar *
gimp_pixel_fetcher_provide_tile (GimpPixelFetcher *pf, gint x, gint y)
{
  return pf->drawable->input +
    ((size_t) y * (size_t) pf->img_width + (size_t) x) * (size_t) pf->img_bpp;
}

static void
/* BEGIN EXACT LEGACY PIXEL FETCHER */
gimp_pixel_fetcher_get_pixel (GimpPixelFetcher *pf,
                              gint              x,
                              gint              y,
                              guchar           *pixel)
{
  guchar *p;
  gint    i;

  g_return_if_fail (pf != NULL);
  g_return_if_fail (pixel != NULL);

  if (pf->mode == GIMP_PIXEL_FETCHER_EDGE_NONE &&
      (x < pf->sel_x1 || x >= pf->sel_x2 ||
       y < pf->sel_y1 || y >= pf->sel_y2))
    {
      return;
    }

  if (x < 0 || x >= pf->img_width ||
      y < 0 || y >= pf->img_height)
    {
      switch (pf->mode)
        {
        case GIMP_PIXEL_FETCHER_EDGE_WRAP:
          if (x < 0 || x >= pf->img_width)
            {
              x %= pf->img_width;

              if (x < 0)
                x += pf->img_width;
            }

          if (y < 0 || y >= pf->img_height)
            {
              y %= pf->img_height;

              if (y < 0)
                y += pf->img_height;
            }
          break;

        case GIMP_PIXEL_FETCHER_EDGE_SMEAR:
          x = CLAMP (x, 0, pf->img_width - 1);
          y = CLAMP (y, 0, pf->img_height - 1);
          break;

        case GIMP_PIXEL_FETCHER_EDGE_BLACK:
          for (i = 0; i < pf->img_bpp; i++)
            pixel[i] = 0;
          return;

        case GIMP_PIXEL_FETCHER_EDGE_BACKGROUND:
          for (i = 0; i < pf->img_bpp; i++)
            pixel[i] = pf->bg_color[i];
          return;

        default:
          return;
        }
    }

  p = gimp_pixel_fetcher_provide_tile (pf, x, y);

  i = pf->img_bpp;

  do
    {
      *pixel++ = *p++;
    }
  while (--i);
}
/* END EXACT LEGACY PIXEL FETCHER */

static void
gimp_pixel_rgn_init (GimpPixelRgn *region, GimpDrawable *drawable,
                     gint x, gint y, gint w, gint h,
                     gboolean dirty, gboolean shadow)
{
  (void) dirty;
  region->x = x; region->y = y; region->w = w; region->h = h;
  region->bpp = drawable->bpp;
  region->rowstride = drawable->width * drawable->bpp;
  region->data = shadow ? drawable->output : drawable->input;
  /* An empty ROI has no registered region and must not form an out-of-range
   * corner pointer, notably when both x == width and y == height. */
  if (w && h)
    region->data += ((size_t) y * (size_t) drawable->width + (size_t) x) *
                    (size_t) drawable->bpp;
}

static gpointer
gimp_pixel_rgns_register (gint count, GimpPixelRgn *source, GimpPixelRgn *dest)
{
  if (count != 2 || source->w != dest->w || source->h != dest->h)
    abort ();
  return dest->w && dest->h ? dest : NULL;
}
static gpointer gimp_pixel_rgns_process (gpointer region)
{ (void) region; return NULL; }

static void gimp_progress_update (gdouble value) { (void) value; }
static void gimp_drawable_flush (GimpDrawable *drawable)
{ (void) drawable; ++flush_count; }
static void gimp_drawable_merge_shadow (gint id, gboolean undo)
{ (void) id; (void) undo; ++merge_count; }
static void gimp_drawable_update (gint id, gint x, gint y, gint w, gint h)
{ (void) id; (void) x; (void) y; (void) w; (void) h; ++update_count; }

static gint edge_detect (const guchar *data);
static gint sobel (const guchar *data);
static gint prewitt (const guchar *data);
static gint gradient (const guchar *data);
static gint roberts (const guchar *data);
static gint differential (const guchar *data);
static gint laplace (const guchar *data);

/* BEGIN EXACT LEGACY EDGE */
static void
edge (GimpDrawable *drawable)
{
  GimpPixelRgn      src_rgn, dest_rgn;
  gpointer          pr;
  GimpPixelFetcher *pft;
  guchar           *srcrow, *src;
  guchar           *destrow, *dest;
  guchar            pix00[4], pix01[4], pix02[4];
  guchar            pix10[4], pix11[4], pix12[4];
  guchar            pix20[4], pix21[4], pix22[4];
  gint              alpha;
  gboolean          has_alpha;
  gint              chan;
  gint              x, y;
  gint              x1, y1, x2, y2;
  gint              cur_progress;
  gint              max_progress;
  gdouble           per_progress;

  if (evals.amount < 1.0)
    evals.amount = 1.0;

  pft = gimp_pixel_fetcher_new (drawable, FALSE);
  gimp_pixel_fetcher_set_edge_mode (pft, evals.wrapmode);

  gimp_drawable_mask_bounds (drawable->drawable_id, &x1, &y1, &x2, &y2);

  alpha     = gimp_drawable_bpp (drawable->drawable_id);
  has_alpha = gimp_drawable_has_alpha (drawable->drawable_id);
  if (has_alpha)
    alpha--;

  cur_progress = 0;
  per_progress = 0.0;
  max_progress = (x2 - x1) * (y2 - y1) / 100;

  gimp_pixel_rgn_init (&src_rgn, drawable, x1, y1, x2-x1, y2-y1, FALSE, FALSE);
  gimp_pixel_rgn_init (&dest_rgn, drawable, x1, y1, x2-x1, y2-y1, TRUE, TRUE);

  for (pr = gimp_pixel_rgns_register (2, &src_rgn, &dest_rgn);
       pr != NULL;
       pr = gimp_pixel_rgns_process (pr))
    {
      srcrow  = src_rgn.data;
      destrow = dest_rgn.data;

      for (y = dest_rgn.y;
           y < (dest_rgn.y + dest_rgn.h);
           y++, srcrow += src_rgn.rowstride, destrow += dest_rgn.rowstride)
        {
          src  = srcrow;
          dest = destrow;

          for (x = dest_rgn.x;
               x < (dest_rgn.x + dest_rgn.w);
               x++,  src += src_rgn.bpp, dest += dest_rgn.bpp)
            {
              if (dest_rgn.x < x &&  x < dest_rgn.x + dest_rgn.w - 2 &&
                  dest_rgn.y < y &&  y < dest_rgn.y + dest_rgn.h - 2)
                {
                  /*
                   * 3x3 kernel is inside of the tile -- do fast
                   * version
                   */
                  for (chan = 0; chan < alpha; chan++)
                    {
                      /* get the 3x3 kernel into a guchar array,
                       * and send it to edge_detect */
                      guchar kernel[9];
                      gint   i,j;

#define PIX(X,Y)  src[ (Y-1)*(int)src_rgn.rowstride + (X-1)*(int)src_rgn.bpp + chan ]
                      /* make convolution */
                      for(i = 0; i < 3; i++)
                        for(j = 0; j < 3; j++)
                          kernel[3*i + j] = PIX(i,j);

#undef  PIX

                      dest[chan] = edge_detect (kernel);
                    }
                }
              else
                {
                  /*
                   * The kernel is not inside of the tile -- do slow
                   * version
                   */

                  gimp_pixel_fetcher_get_pixel (pft, x-1, y-1, pix00);
                  gimp_pixel_fetcher_get_pixel (pft, x  , y-1, pix10);
                  gimp_pixel_fetcher_get_pixel (pft, x+1, y-1, pix20);
                  gimp_pixel_fetcher_get_pixel (pft, x-1, y  , pix01);
                  gimp_pixel_fetcher_get_pixel (pft, x  , y  , pix11);
                  gimp_pixel_fetcher_get_pixel (pft, x+1, y  , pix21);
                  gimp_pixel_fetcher_get_pixel (pft, x-1, y+1, pix02);
                  gimp_pixel_fetcher_get_pixel (pft, x  , y+1, pix12);
                  gimp_pixel_fetcher_get_pixel (pft, x+1, y+1, pix22);

                  for (chan = 0; chan < alpha; chan++)
                    {
                      guchar kernel[9];

                      kernel[0] = pix00[chan];
                      kernel[1] = pix01[chan];
                      kernel[2] = pix02[chan];
                      kernel[3] = pix10[chan];
                      kernel[4] = pix11[chan];
                      kernel[5] = pix12[chan];
                      kernel[6] = pix20[chan];
                      kernel[7] = pix21[chan];
                      kernel[8] = pix22[chan];

                      dest[chan] = edge_detect (kernel);
                    }
                }
              if (has_alpha)
                dest[alpha] = src[alpha];
            }
        }
      cur_progress += dest_rgn.w * dest_rgn.h;

      if (cur_progress > max_progress)
        {
          cur_progress = cur_progress - max_progress;
          per_progress = per_progress + 0.01;
          gimp_progress_update (per_progress);
        }
    }

  gimp_progress_update (1.0);

  gimp_pixel_fetcher_destroy (pft);

  gimp_drawable_flush (drawable);
  gimp_drawable_merge_shadow (drawable->drawable_id, TRUE);
  gimp_drawable_update (drawable->drawable_id, x1, y1, (x2 - x1), (y2 - y1));
}
/* END EXACT LEGACY EDGE */

/* ***********************   Edge Detection   ******************** */

/*
 * Edge detect switcher function
 */
/* BEGIN EXACT LEGACY EDGE DETECTORS */
static gint
edge_detect (const guchar *data)
{
  gint ret;

  switch (evals.edgemode)
    {
    case SOBEL:
      ret = sobel (data);
      break;
    case PREWITT:
      ret = prewitt (data);
      break;
    case GRADIENT:
      ret = gradient (data);
      break;
    case ROBERTS:
      ret = roberts (data);
      break;
    case DIFFERENTIAL:
      ret = differential (data);
      break;
    case LAPLACE:
      ret = laplace (data);
      break;
    default:
      ret = -1;
      break;
    }

  return CLAMP0255 (ret);
}


/*
 * Sobel Edge detector
 */
static gint
sobel (const guchar *data)
{
  const gint v_kernel[9] = { -1,  0,  1,
                             -2,  0,  2,
                             -1,  0,  1 };
  const gint h_kernel[9] = { -1, -2, -1,
                              0,  0,  0,
                              1,  2,  1 };

  gint i;
  gint v_grad, h_grad;

  for (i = 0, v_grad = 0, h_grad = 0; i < 9; i++)
    {
      v_grad += v_kernel[i] * data[i];
      h_grad += h_kernel[i] * data[i];
    }

  return sqrt (v_grad * v_grad * evals.amount +
               h_grad * h_grad * evals.amount);
}

/*
 * Edge detector via template matting
 *   -- Prewitt Compass
 */
static gint
prewitt (const guchar *data)
{
  gint k, max;
  gint m[8];

  m[0] =   data [0] +   data [1] + data [2]
         + data [3] - 2*data [4] + data [5]
         - data [6] -   data [7] - data [8];
  m[1] =   data [0] +   data [1] + data [2]
         + data [3] - 2*data [4] - data [5]
         + data [6] -   data [7] - data [8];
  m[2] =   data [0] +   data [1] - data [2]
         + data [3] - 2*data [4] - data [5]
         + data [6] +   data [7] - data [8];
  m[3] =   data [0] -   data [1] - data [2]
         + data [3] - 2*data [4] - data [5]
         + data [6] +   data [7] + data [8];
  m[4] = - data [0] -   data [1] - data [2]
         + data [3] - 2*data [4] + data [5]
         + data [6] +   data [7] + data [8];
  m[5] = - data [0] -   data [1] + data [2]
         - data [3] - 2*data [4] + data [5]
         + data [6] +   data [7] + data [8];
  m[6] = - data [0] +   data [1] + data [2]
         - data [3] - 2*data [4] + data [5]
         - data [6] +   data [7] + data [8];
  m[7] =   data [0] +   data [1] + data [2]
         - data [3] - 2*data [4] + data [5]
         - data [6] -   data [7] + data [8];

  for (k = 0, max = 0; k < 8; k++)
    if (max < m[k])
      max = m[k];

  return evals.amount * max;
}

/*
 * Gradient Edge detector
 */
static gint
gradient (const guchar *data)
{
  const gint v_kernel[9] = { 0,  0,  0,
                             0,  4, -4,
                             0,  0,  0 };
  const gint h_kernel[9] = { 0,  0,  0,
                             0, -4,  0,
                             0,  4,  0 };

  gint i;
  gint v_grad, h_grad;

  for (i = 0, v_grad = 0, h_grad = 0; i < 9; i++)
    {
      v_grad += v_kernel[i] * data[i];
      h_grad += h_kernel[i] * data[i];
    }

  return  sqrt (v_grad * v_grad * evals.amount +
                h_grad * h_grad * evals.amount);
}

/*
 * Roberts Edge detector
 */
static gint
roberts (const guchar *data)
{
  const gint v_kernel[9] = { 0,  0,  0,
                             0,  4,  0,
                             0,  0, -4 };
  const gint h_kernel[9] = { 0,  0,  0,
                             0,  0,  4,
                             0, -4,  0 };
  gint i;
  gint v_grad, h_grad;

  for (i = 0, v_grad = 0, h_grad = 0; i < 9; i++)
    {
      v_grad += v_kernel[i] * data[i];
      h_grad += h_kernel[i] * data[i];
    }

  return sqrt (v_grad * v_grad * evals.amount +
               h_grad * h_grad * evals.amount);
}

/*
 * Differential Edge detector
 */
static gint
differential (const guchar *data)
{
  const gint v_kernel[9] = { 0,  0,  0,
                             0,  2, -2,
                             0,  2, -2 };
  const gint h_kernel[9] = { 0,  0,  0,
                             0, -2, -2,
                             0,  2,  2 };
  gint i;
  gint v_grad, h_grad;

  for (i = 0, v_grad = 0, h_grad = 0; i < 9; i++)
    {
      v_grad += v_kernel[i] * data[i];
      h_grad += h_kernel[i] * data[i];
    }

  return sqrt (v_grad * v_grad * evals.amount +
               h_grad * h_grad * evals.amount);
}

/*
 * Laplace Edge detector
 */
static gint
laplace (const guchar *data)
{
  const gint kernel[9] = { 1,  1,  1,
                           1, -8,  1,
                           1,  1,  1 };
  gint i;
  gint grad;

  for (i = 0, grad = 0; i < 9; i++)
    grad += kernel[i] * data[i];

  return grad * evals.amount;
}
/* END EXACT LEGACY EDGE DETECTORS */

static int
oracle_domain_valid (int32_t edgemode, double amount)
{
  /* At every 8-bit kernel, each directional gradient is at most 1020 and
   * its signed integer square at most 1040400. Prewitt's nonnegative max is
   * at most 1275; Laplace is in [-2040, 2040]. This conservative common bound
   * makes every original double-to-gint return defined, before CLAMP0255.
   * The original edge() performs the lower clamp, including negative values.
   */
  return edgemode >= SOBEL && edgemode <= LAPLACE && isfinite (amount) &&
         amount <= (double) INT32_MAX / 2040.0;
}

int
legacy_edge_shadow_detect (const uint8_t kernel[9], int32_t edgemode,
                           double amount, uint8_t *output)
{
  if (!kernel || !output || !oracle_domain_valid (edgemode, amount))
    return 0;
  evals.edgemode = edgemode;
  evals.amount = amount < 1.0 ? 1.0 : amount;
  *output = (uint8_t) edge_detect (kernel);
  return 1;
}

int
legacy_edge_shadow_oracle (const uint8_t *input,
                           size_t width, size_t height, unsigned channels,
                           size_t x, size_t y,
                           size_t roi_width, size_t roi_height,
                           int32_t edgemode, int32_t wrapmode, double amount,
                           uint8_t *output)
{
  GimpDrawable drawable;
  size_t extent, rowstride, padded_extent;
  uintptr_t input_start, output_start;
  guchar *source_copy = NULL, *shadow_copy = NULL;

  flush_count = merge_count = update_count = 0;
  if (!input || !output || !width || !height ||
      width > INT_MAX || height > INT_MAX || channels < 1 || channels > 4 ||
      width > SIZE_MAX / channels / height || x > width || y > height ||
      roi_width > width - x || roi_height > height - y ||
      !oracle_domain_valid (edgemode, amount) ||
      wrapmode < GIMP_PIXEL_FETCHER_EDGE_WRAP ||
      wrapmode > GIMP_PIXEL_FETCHER_EDGE_BLACK)
    return 0;

  rowstride = width * channels;
  extent = rowstride * height;
  /* The literal PIX macro computes signed offsets, including rowstride +
   * bpp + chan and -rowstride - bpp, while max_progress computes the ROI area as a signed product. */
  if (rowstride > (size_t) INT_MAX - (2 * channels - 1) ||
      (roi_height && roi_width > (size_t) INT_MAX / roi_height) ||
      rowstride > (size_t) PTRDIFF_MAX ||
      extent > (size_t) PTRDIFF_MAX - rowstride)
    return 0;
  padded_extent = extent + rowstride;

  input_start = (uintptr_t) input;
  output_start = (uintptr_t) output;
  if (input_start > UINTPTR_MAX - extent || output_start > UINTPTR_MAX - extent ||
      (input_start < output_start + extent && output_start < input_start + extent))
    return 0;

  if (roi_width && roi_height)
    {
      source_copy = malloc (padded_extent);
      shadow_copy = malloc (padded_extent);
      if (!source_copy || !shadow_copy)
        {
          free (source_copy);
          free (shadow_copy);
          return 0;
        }
      memcpy (source_copy, input, extent);
      memcpy (shadow_copy, output, extent);
    }

  drawable.drawable_id = 1;
  drawable.width = (gint) width; drawable.height = (gint) height;
  drawable.bpp = (gint) channels;
  drawable.roi_x = (gint) x; drawable.roi_y = (gint) y;
  drawable.roi_width = (gint) roi_width; drawable.roi_height = (gint) roi_height;
  drawable.input = source_copy;
  drawable.output = shadow_copy;
  active_drawable = &drawable;
  evals.edgemode = edgemode;
  evals.wrapmode = wrapmode;
  evals.amount = amount;
  edge (&drawable);
  active_drawable = NULL;

  if (roi_width && roi_height)
    memcpy (output, shadow_copy, extent);
  free (source_copy);
  free (shadow_copy);
  return 1;
}

void
legacy_edge_shadow_oracle_get_counters (unsigned *flushes,
                                        unsigned *merges, unsigned *updates)
{
  if (flushes) *flushes = flush_count;
  if (merges) *merges = merge_count;
  if (updates) *updates = update_count;
}
