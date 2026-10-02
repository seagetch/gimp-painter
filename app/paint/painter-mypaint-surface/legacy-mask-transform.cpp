/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * gimpbrush-transform.c
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

#include "legacy-mask-transform.hpp"
#include <cmath>
#include <stdexcept>
#include <limits>
extern "C" {
#include "libgimpmath/gimpmath.h"
}
namespace GimpPainter { namespace MyPaint {
#define MAX_BLUR_KERNEL 15
static void gimp_brush_transform_matrix (gdouble,gdouble,gdouble,gdouble,gdouble,GimpMatrix3*);
static void gimp_brush_transform_bounding_box (const ShapeMask*,const GimpMatrix3*,gint*,gint*,gint*,gint*);
static gdouble gimp_brush_transform_array_sum (gfloat*,gint);
static void gimp_brush_transform_fill_blur_kernel (gfloat*,gint);
static gint gimp_brush_transform_blur_kernel_size (gint,gint,gdouble);
ShapeMask
transform_bitmap_mask (const ShapeMask& mask,
                                gdouble    scale,
                                gdouble    aspect_ratio,
                                gdouble    angle,
                                gdouble    hardness)
{
  ShapeMask     result;
  const ShapeMask *source;
  guchar       *dest;
  const guchar *src;
  GimpMatrix3   matrix;
  gint          src_width;
  gint          src_height;
  gint          src_width_minus_one;
  gint          src_height_minus_one;
  gint          dest_width;
  gint          dest_height;
  gint          x, y;
  gdouble       blx, brx, tlx, trx;
  gdouble       bly, bry, tly, top_right_y;
  gdouble       src_tl_to_tr_delta_x;
  gdouble       src_tl_to_tr_delta_y;
  gdouble       src_tl_to_bl_delta_x;
  gdouble       src_tl_to_bl_delta_y;
  gint64        src_walk_ux_i;
  gint64        src_walk_uy_i;
  gint64        src_walk_vx_i;
  gint64        src_walk_vy_i;
  gint64        src_space_cur_pos_x;
  gint64        src_space_cur_pos_y;
  gint64        src_space_cur_pos_x_i;
  gint64        src_space_cur_pos_y_i;
  gint64        src_space_row_start_x_i;
  gint64        src_space_row_start_y_i;
  const guchar *src_walker;
  const guchar *pixel_next;
  const guchar *pixel_below;
  const guchar *pixel_below_next;
  gint          opposite_x, distance_from_true_x;
  gint          opposite_y, distance_from_true_y;

  /*
   * tl, tr etc are used because it is easier to visualize top left,
   * top right etc corners of the forward transformed source image
   * rectangle.
   */
  const gint fraction_bits = 12;
  const gint int_multiple  = pow (2, fraction_bits);

  /* In inner loop's bilinear calculation, two numbers that were each
   * previously multiplied by int_multiple are multiplied together.
   * To get back the right result, the multiplication result must be
   * divided *twice* by 2^fraction_bits, equivalent to bit shift right
   * by 2 * fraction_bits
   */
  const gint recovery_bits = 2 * fraction_bits;

  /*
   * example: suppose fraction_bits = 9
   * a 9-bit mask looks like this: 0001 1111 1111
   * and is given by:  2^fraction_bits - 1
   * demonstration:
   * 2^0     = 0000 0000 0001
   * 2^1     = 0000 0000 0010
   * :
   * 2^8     = 0001 0000 0000
   * 2^9     = 0010 0000 0000
   * 2^9 - 1 = 0001 1111 1111
   */
  const guint fraction_bitmask = pow(2, fraction_bits) - 1 ;

  if (mask.width <= 0 || mask.height <= 0 || std::size_t(mask.width)*mask.height > G_MAXINT ||
      mask.pixels.size () != std::size_t (mask.width)*mask.height ||
      !std::isfinite (scale) || scale <= 0 || !std::isfinite (aspect_ratio) ||
      !std::isfinite (angle) || !std::isfinite (hardness) || hardness < 0 || hardness > 1)
    throw std::invalid_argument ("Invalid legacy bitmap transform");
  source = &mask;

  gimp_brush_transform_matrix (source->height, source->width,
                               scale, aspect_ratio, angle, &matrix);

  if (gimp_matrix3_is_identity (&matrix))
    return *source;

  src_width  = source->width;
  src_height = source->height;
  src_width_minus_one  = src_width  - 1;
  src_height_minus_one = src_height - 1;

  gimp_brush_transform_bounding_box (source, &matrix,
                                     &x, &y, &dest_width, &dest_height);
  gimp_matrix3_translate (&matrix, -x, -y);
  gimp_matrix3_invert (&matrix);

  if (dest_width <= 0 || dest_height <= 0 || dest_width > G_MAXINT-MAX_BLUR_KERNEL || dest_height > G_MAXINT-MAX_BLUR_KERNEL ||
      std::size_t(dest_width)*dest_height > G_MAXINT)
    throw std::invalid_argument ("Legacy transformed mask exceeds representable pixel indexing");
  result.width = dest_width; result.height = dest_height;
  result.pixels.resize (std::size_t (dest_width)*dest_height);

  dest = result.pixels.data ();
  src  = source->pixels.data ();

  /* prevent disappearance of 1x1 pixel brush at some rotations when
     scaling < 1 */
  /*
  if (src_width == 1 && src_height == 1 && scale_x < 1 && scale_y < 1 )
    {
      *dest = src[0];
      return result;
    }*/

  gimp_matrix3_transform_point (&matrix, 0,          0,           &tlx, &tly);
  gimp_matrix3_transform_point (&matrix, dest_width, 0,           &trx, &top_right_y);
  gimp_matrix3_transform_point (&matrix, 0,          dest_height, &blx, &bly);
  gimp_matrix3_transform_point (&matrix, dest_width, dest_height, &brx, &bry);


  /* in image space, calc U (what was horizontal originally)
   * note: double precision
   */
  src_tl_to_tr_delta_x = trx - tlx;
  src_tl_to_tr_delta_y = top_right_y - tly;

  /* in image space, calc V (what was vertical originally)
   * note: double precision
   */
  src_tl_to_bl_delta_x = blx - tlx;
  src_tl_to_bl_delta_y = bly - tly;

  for (double coordinate : {tlx,tly,trx,top_right_y,blx,bly,brx,bry})
    if (!std::isfinite(coordinate) || std::abs(coordinate) >= 281474976710656.0)
      throw std::invalid_argument("Legacy inverse transform exceeds fixed-point range");

  /* speed optimized, note conversion to int precision */
  src_walk_ux_i = (gint64) ((src_tl_to_tr_delta_x / dest_width)  * int_multiple);
  src_walk_uy_i = (gint64) ((src_tl_to_tr_delta_y / dest_width)  * int_multiple);
  src_walk_vx_i = (gint64) ((src_tl_to_bl_delta_x / dest_height) * int_multiple);
  src_walk_vy_i = (gint64) ((src_tl_to_bl_delta_y / dest_height) * int_multiple);

  /* initialize current position in source space to the start position (tl)
   * speed optimized, note conversion to int precision
   */
  src_space_cur_pos_x_i   = (gint64) (tlx* int_multiple);
  src_space_cur_pos_y_i   = (gint64) (tly* int_multiple);
  src_space_cur_pos_x     = (gint64) (src_space_cur_pos_x_i >> fraction_bits);
  src_space_cur_pos_y     = (gint64) (src_space_cur_pos_y_i >> fraction_bits);
  src_space_row_start_x_i = (gint64) (tlx* int_multiple);
  src_space_row_start_y_i = (gint64) (tly* int_multiple);


  for (y = 0; y < dest_height; y++)
    {
      for (x = 0; x < dest_width; x++)
        {
          if (src_space_cur_pos_x > src_width_minus_one ||
              src_space_cur_pos_x < 0     ||
              src_space_cur_pos_y > src_height_minus_one ||
              src_space_cur_pos_y < 0)
              /* no corresponding pixel in source space */
            {
              *dest = 0;
            }
          else /* reverse transformed point hits source pixel */
            {
              src_walker = src
                        + src_space_cur_pos_y * src_width
                        + src_space_cur_pos_x;

              /* bottom right corner
               * no pixel below, reuse current pixel instead
               * no next pixel to the right so reuse current pixel instead
               */
              if (src_space_cur_pos_y == src_height_minus_one &&
                  src_space_cur_pos_x == src_width_minus_one )
                {
                  pixel_next       = src_walker;
                  pixel_below      = src_walker;
                  pixel_below_next = src_walker;
                }

              /* bottom edge pixel row, except rightmost corner
               * no pixel below, reuse current pixel instead  */
              else if (src_space_cur_pos_y == src_height_minus_one)
                {
                  pixel_next       = src_walker + 1;
                  pixel_below      = src_walker;
                  pixel_below_next = src_walker + 1;
                }

              /* right edge pixel column, except bottom corner
               * no next pixel to the right so reuse current pixel instead */
              else if (src_space_cur_pos_x == src_width_minus_one)
                {
                  pixel_next       = src_walker;
                  pixel_below      = src_walker + src_width;
                  pixel_below_next = pixel_below;
                }

              /* neither on bottom edge nor on right edge */
              else
                {
                  pixel_next       = src_walker + 1;
                  pixel_below      = src_walker + src_width;
                  pixel_below_next = pixel_below + 1;
                }

              distance_from_true_x = src_space_cur_pos_x_i & fraction_bitmask;
              distance_from_true_y = src_space_cur_pos_y_i & fraction_bitmask;
              opposite_x =  int_multiple - distance_from_true_x;
              opposite_y =  int_multiple - distance_from_true_y;

              *dest = ((guint32 (src_walker[0]) * opposite_x +
                        pixel_next[0] * distance_from_true_x) * opposite_y +
                       (guint32 (pixel_below[0]) * opposite_x +
                        pixel_below_next[0] *distance_from_true_x) * distance_from_true_y
                       ) >> recovery_bits;
            }

          src_space_cur_pos_x_i+=src_walk_ux_i;
          src_space_cur_pos_y_i+=src_walk_uy_i;

          src_space_cur_pos_x = src_space_cur_pos_x_i >> fraction_bits;
          src_space_cur_pos_y = src_space_cur_pos_y_i >> fraction_bits;

          dest ++;
        } /* end for x */

        src_space_row_start_x_i +=src_walk_vx_i;
        src_space_row_start_y_i +=src_walk_vy_i;
        src_space_cur_pos_x_i = src_space_row_start_x_i;
        src_space_cur_pos_y_i = src_space_row_start_y_i;

        src_space_cur_pos_x = src_space_cur_pos_x_i >> fraction_bits;
        src_space_cur_pos_y = src_space_cur_pos_y_i >> fraction_bits;

    } /* end for y */

  if (hardness < 1.0)
    {
      const ShapeMask blur_src = result;
      gint kernel_size = gimp_brush_transform_blur_kernel_size (result.height, result.width, hardness);
      gint kernel_len = kernel_size*kernel_size;
      std::vector<gfloat> kernel (kernel_len);
      gimp_brush_transform_fill_blur_kernel (kernel.data (), kernel_len);
      gdouble divisor = gimp_brush_transform_array_sum (kernel.data (), kernel_len);
      const gint margin = kernel_size/2;
      // Exact old convolve_region normal, non-alpha-weighted byte-mask path.
      for (int y = 0; y < result.height; ++y) for (int x = 0; x < result.width; ++x) {
        double total = 0; int k = 0;
        for (int j = y-margin; j <= y+margin; ++j)
          for (int i = x-margin; i <= x+margin; ++i, ++k) {
            const int xx = CLAMP (i,0,result.width-1), yy = CLAMP (j,0,result.height-1);
            total += kernel[k]*blur_src.pixels[yy*result.width+xx];
          }
        total /= divisor;
        result.pixels[y*result.width+x] = total < 0 ? 0 : total > 255 ? 255 : static_cast<guchar> (int (total+0.5));
      }
    }

  return result;
}

void
gimp_brush_transform_matrix (gdouble      width,
                             gdouble      height,
                             gdouble      scale,
                             gdouble      aspect_ratio,
                             gdouble      angle,
                             GimpMatrix3 *matrix)
{
  const gdouble center_x = width  / 2;
  const gdouble center_y = height / 2;
  gdouble scale_x = scale;
  gdouble scale_y = scale;

  if (aspect_ratio < 0.0)
    {
      scale_x = scale * (1.0 - (fabs (aspect_ratio) / 20.0));
      scale_y = scale;
    }
  else if (aspect_ratio > 0.0)
    {
      scale_x = scale;
      scale_y = scale * (1.0 - (aspect_ratio  / 20.0));
    }

  gimp_matrix3_identity (matrix);
  gimp_matrix3_scale (matrix, scale_x, scale_y);
  gimp_matrix3_translate (matrix, - center_x * scale_x, - center_y * scale_y);
  gimp_matrix3_rotate (matrix, -2 * G_PI * angle);
  gimp_matrix3_translate (matrix, center_x * scale_x, center_y * scale_y);
}


/*  private functions  */

static void
gimp_brush_transform_bounding_box (const ShapeMask   *brush,
                                   const GimpMatrix3 *matrix,
                                   gint              *x,
                                   gint              *y,
                                   gint              *width,
                                   gint              *height)
{
  const gdouble  w = brush->width;
  const gdouble  h = brush->height;
  gdouble        x1, x2, x3, x4;
  gdouble        y1, y2, y3, y4;
  gdouble        temp_x;
  gdouble        temp_y;

  gimp_matrix3_transform_point (matrix, 0, 0, &x1, &y1);
  gimp_matrix3_transform_point (matrix, w, 0, &x2, &y2);
  gimp_matrix3_transform_point (matrix, 0, h, &x3, &y3);
  gimp_matrix3_transform_point (matrix, w, h, &x4, &y4);

  temp_x = MIN (MIN (x1, x2), MIN (x3, x4));
  temp_y = MIN (MIN (y1, y2), MIN (y3, y4));

  const auto checked = [](double value) -> gint {
    if (!std::isfinite(value) || value < G_MININT || value > G_MAXINT)
      throw std::invalid_argument("Legacy mask bounds exceed integer coordinates");
    return static_cast<gint>(value);
  };
  *width  = checked(ceil (MAX (MAX (x1, x2), MAX (x3, x4)) - temp_x));
  *height = checked(ceil (MAX (MAX (y1, y2), MAX (y3, y4)) - temp_y));
  *x = checked(floor (temp_x));
  *y = checked(floor (temp_y));

  /* Transform size can not be less than 1 px */
  *width  = MAX (1, *width);
  *height = MAX (1, *height);
}

static gdouble
gimp_brush_transform_array_sum (gfloat *arr,
                                gint    len)
{
  gfloat total = 0;
  gint   i;

  for (i = 0; i < len; i++)
    {
      total += arr [i];
    }

  return total;
}

static void
gimp_brush_transform_fill_blur_kernel (gfloat *arr,
                                       gint    len)
{
  gint half_point = ((gint) len / 2) + 1;
  gint i;

  for (i = 0; i < len; i++)
    {
      if (i < half_point)
        arr [i] = half_point - i;
      else
        arr [i] = i - half_point;
    }
}

static gint
gimp_brush_transform_blur_kernel_size (gint    height,
                                       gint    width,
                                       gdouble hardness)
{
  gint kernel_size = (MIN (MAX_BLUR_KERNEL,
                           MIN (width, height)) *
                      ((MIN (width, height) * (1.0 - hardness)) /
                       MIN (width, height)));

  /* Kernel size must be odd */
  if (kernel_size % 2 == 0)
    kernel_size++;

  return kernel_size;
}

#undef MAX_BLUR_KERNEL
} }
