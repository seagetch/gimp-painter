/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * gimp_brush_generated module Copyright 1998 Jay Cox <jaycox@earthlink.net>
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

#include "config.h"
#include "legacy-generated-mask.hpp"
#include <cmath>
#include <stdexcept>
extern "C" {
#include "libgimpmath/gimpmath.h"
}
namespace GimpPainter { namespace MyPaint {
#define OVERSAMPLING 4
static void gimp_brush_generated_get_half_size (GeneratedShape,gfloat,gint,gfloat,gfloat,gdouble,gint*,gint*,gdouble*,gdouble*,GimpVector2*,GimpVector2*);
static gdouble
gauss (gdouble f)
{
  /* this aint' a real gauss function */
  if (f < -0.5)
    {
      f = -1.0 - f;
      return (2.0 * f*f);
    }

  if (f < 0.5)
    return (1.0 - 2.0 * f*f);

  f = 1.0 - f;
  return (2.0 * f*f);
}

/* set up lookup table */
static std::vector<guchar>
gimp_brush_generated_calc_lut (gdouble radius,
                               gdouble hardness)
{
  std::vector<guchar> lookup;
  gint     length;
  gint     x;
  gdouble  d;
  gdouble  sum;
  gdouble  exponent;
  gdouble  buffer[OVERSAMPLING];

  length = OVERSAMPLING * ceil (1 + sqrt (2 * SQR (ceil (radius + 1.0))));

  lookup.resize (length);
  sum = 0.0;

  if ((1.0 - hardness) < 0.0000004)
    exponent = 1000000.0;
  else
    exponent = 0.4 / (1.0 - hardness);

  for (x = 0; x < OVERSAMPLING; x++)
    {
      d = fabs ((x + 0.5) / OVERSAMPLING - 0.5);

      if (d > radius)
        buffer[x] = 0.0;
      else
        buffer[x] = gauss (pow (d / radius, exponent));

      sum += buffer[x];
    }

  for (x = 0; d < radius || sum > 0.00001; d += 1.0 / OVERSAMPLING)
    {
      sum -= buffer[x % OVERSAMPLING];

      if (d > radius)
        buffer[x % OVERSAMPLING] = 0.0;
      else
        buffer[x % OVERSAMPLING] = gauss (pow (d / radius, exponent));

      sum += buffer[x % OVERSAMPLING];
      lookup[x++] = RINT (sum * (255.0 / OVERSAMPLING));
    }

  while (x < length)
    {
      lookup[x++] = 0;
    }

  return lookup;
}

static ShapeMask
generate_legacy_mask_impl (GeneratedShape shape,
                           gfloat                   radius,
                           gint                     spikes,
                           gfloat                   hardness,
                           gfloat                   aspect_ratio,
                           gfloat                   angle,
                           GimpVector2             *xaxis,
                           GimpVector2             *yaxis)
{
  guchar      *centerp;
  std::vector<guchar> lookup;
  guchar       a;
  gint         half_width  = 0;
  gint         half_height = 0;
  gint         x, y;
  gdouble      c, s, cs, ss;
  GimpVector2  x_axis;
  GimpVector2  y_axis;
  ShapeMask mask;

  if (!std::isfinite (radius) || radius < 0 || radius > 32767 ||
      !std::isfinite (hardness) || hardness < 0 || hardness > 1 ||
      !std::isfinite (aspect_ratio) || aspect_ratio < 1 || aspect_ratio > 1000 ||
      !std::isfinite (angle) || std::abs (angle) > (G_MAXINT-1)/1000.0 || spikes < 2 || spikes > 20 ||
      (shape != GeneratedShape::Circle && shape != GeneratedShape::Square && shape != GeneratedShape::Diamond))
    throw std::invalid_argument ("Invalid legacy generated brush parameters");
  gimp_brush_generated_get_half_size (shape,
                                      radius,
                                      spikes,
                                      hardness,
                                      aspect_ratio,
                                      angle,
                                      &half_width, &half_height,
                                      &s, &c, &x_axis, &y_axis);

  mask.width = half_width*2+1; mask.height = half_height*2+1;
  if (std::size_t(mask.width)*mask.height > G_MAXINT)
    throw std::invalid_argument("Generated mask exceeds representable pixel indexing");
  mask.pixels.resize (std::size_t(mask.width)*mask.height);

  centerp = mask.pixels.data () + half_height * mask.width + half_width;

  lookup = gimp_brush_generated_calc_lut (radius, hardness);

  cs = cos (- 2 * G_PI / spikes);
  ss = sin (- 2 * G_PI / spikes);

  /* for an even number of spikes compute one half and mirror it */
  for (y = (spikes % 2 ? -half_height : 0); y <= half_height; y++)
    {
      for (x = -half_width; x <= half_width; x++)
        {
          gdouble d  = 0;
          gdouble tx = c * x - s * y;
          gdouble ty = fabs (s * x + c * y);

          if (spikes > 2)
            {
              gdouble angle = atan2 (ty, tx);

              while (angle > G_PI / spikes)
                {
                  gdouble sx = tx;
                  gdouble sy = ty;

                  tx = cs * sx - ss * sy;
                  ty = ss * sx + cs * sy;

                  angle -= 2 * G_PI / spikes;
                }
            }

          ty *= aspect_ratio;

          switch (shape)
            {
            case GeneratedShape::Circle:
              d = sqrt (SQR (tx) + SQR (ty));
              break;
            case GeneratedShape::Square:
              d = MAX (fabs (tx), fabs (ty));
              break;
            case GeneratedShape::Diamond:
              d = fabs (tx) + fabs (ty);
              break;
            }

          if (d < radius + 1)
            a = lookup[(gint) RINT (d * OVERSAMPLING)];
          else
            a = 0;

          centerp[y * mask.width + x] = a;

          if (spikes % 2 == 0)
            centerp[-1 * y * mask.width - x] = a;
        }
    }



  if (xaxis)
    *xaxis = x_axis;

  if (yaxis)
    *yaxis = y_axis;

  return mask;
}

/* This function is shared between gimp_brush_generated_transform_size and
 * gimp_brush_generated_calc, therefore we provide a bunch of optional
 * pointers for returnvalues.
 */
static void
gimp_brush_generated_get_half_size (GeneratedShape shape,
                                    gfloat                   radius,
                                    gint                     spikes,
                                    gfloat                   hardness,
                                    gfloat                   aspect_ratio,
                                    gdouble                  angle_in_degrees,
                                    gint                    *half_width,
                                    gint                    *half_height,
                                    gdouble                 *_s,
                                    gdouble                 *_c,
                                    GimpVector2             *_x_axis,
                                    GimpVector2             *_y_axis)
{
  gdouble      c, s;
  gdouble      short_radius;
  GimpVector2  x_axis;
  GimpVector2  y_axis;

  /* Since floatongpoint is not really accurate,
   * we need to round to limit the errors.
   * Errors in some border cases resulted in
   * different height and width reported for
   * the same input value on calling procedure side.
   * This became problem at the rise of dynamics that
   * allows for any angle to turn up.
   **/

  angle_in_degrees = ROUND (angle_in_degrees * 1000.0) / 1000.0;

  s = sin (gimp_deg_to_rad (angle_in_degrees));
  c = cos (gimp_deg_to_rad (angle_in_degrees));

  short_radius = radius / aspect_ratio;

  x_axis.x =        c * radius;
  x_axis.y = -1.0 * s * radius;
  y_axis.x =        s * short_radius;
  y_axis.y =        c * short_radius;

  switch (shape)
    {
    case GeneratedShape::Circle:
      *half_width  = ceil (sqrt (x_axis.x * x_axis.x + y_axis.x * y_axis.x));
      *half_height = ceil (sqrt (x_axis.y * x_axis.y + y_axis.y * y_axis.y));
      break;

    case GeneratedShape::Square:
      *half_width  = ceil (fabs (x_axis.x) + fabs (y_axis.x));
      *half_height = ceil (fabs (x_axis.y) + fabs (y_axis.y));
      break;

    case GeneratedShape::Diamond:
      *half_width  = ceil (MAX (fabs (x_axis.x), fabs (y_axis.x)));
      *half_height = ceil (MAX (fabs (x_axis.y), fabs (y_axis.y)));
      break;
    }

  if (spikes > 2)
    {
      /* could be optimized by respecting the angle */
      *half_width = *half_height = ceil (sqrt (radius * radius +
                                               short_radius * short_radius));
      y_axis.x = s * radius;
      y_axis.y = c * radius;
    }

  /*  These will typically be set then this function is called by
   *  gimp_brush_generated_calc, which needs the values in its algorithms.
   */
  if (_s != NULL)
    *_s = s;

  if (_c != NULL)
    *_c = c;

  if (_x_axis != NULL)
    *_x_axis = x_axis;

  if (_y_axis != NULL)
    *_y_axis = y_axis;
}

std::pair<int,int> legacy_generated_dimensions (GeneratedShape shape,float radius,int spikes,float hardness,float aspect,float angle)
{
  if (!std::isfinite(radius) || radius<0 || radius>32767 || !std::isfinite(aspect) || aspect<1 || aspect>1000 ||
      !std::isfinite(angle) || std::abs(angle)>(G_MAXINT-1)/1000.0 || spikes<2 || spikes>20)
    throw std::invalid_argument("Invalid generated brush size parameters");
  int half_width=0,half_height=0;
  gimp_brush_generated_get_half_size(shape,radius,spikes,hardness,aspect,angle,&half_width,&half_height,nullptr,nullptr,nullptr,nullptr);
  return {half_width*2+1,half_height*2+1};
}
ShapeMask generate_legacy_mask (GeneratedShape shape,float radius,int spikes,float hardness,float aspect,float angle)
{ return generate_legacy_mask_impl(shape,radius,spikes,hardness,aspect,angle,nullptr,nullptr); }
#undef OVERSAMPLING
} }
