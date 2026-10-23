/* GIMP - The GNU Image Manipulation Program
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

/* Value-Invert plug-in v1.1 by Adam D. Moss, adam@foxbox.org.  1999/02/27
 *
 * RGB<->HSV math optimizations by Mukund Sivaraman <muks@mukund.org>
 * makes the plug-in 2-3 times faster. Using gimp_pixel_rgns_process()
 * also makes memory management nicer. June 12, 2006.
 *
 * The plug-in only does v = 255 - v; for each pixel in the image, or
 * each entry in the colormap depending upon the type of image, where 'v'
 * is the value in HSV color model.
 *
 * The plug-in code is optimized towards this, in that it is not a full
 * RGB->HSV->RGB transform, but shortcuts many of the calculations to
 * effectively only do v = 255 - v. In fact, hue (0-360) is never
 * calculated. The shortcuts can be derived from running a set of r, g, b
 * values through the RGB->HSV transform and then from HSV->RGB and solving
 * out the redundant portions.
 *
 * The plug-in also uses integer code exclusively for the main transform.
 */

/* Test-only literal callback oracle.
 * Extracted without changes from plug-ins/common/value-invert.c at
 * afa43fae3e920210146abed514f136fd49f671b5.
 * Complete pinned source SHA-256:
 * bf7c46cac7f62c13ec73590d53c12ba3dd85fd28def5f74451aaf43b969fef20.
 *
 * The focused runner verifies this entire harness and compares the callback
 * byte-for-byte with the pinned git object before compiling this file as C.
 * It is not a new capture from the original GIMP executable.
 */

typedef unsigned char guchar;
typedef int           gint;

#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define MIN(a, b) (((a) < (b)) ? (a) : (b))

void painter_legacy_vinvert_row (const unsigned char *src,
                                 unsigned char       *dst,
                                 int                  width,
                                 int                  bpp);

/* BEGIN LITERAL PINNED CALLBACK */
static void
vinvert_render_row (const guchar *src,
                    guchar       *dest,
                    gint          width, /* in pixels */
                    gint          bpp)
{
  gint j;
  gint r, g, b;
  gint value, value2, min;
  gint delta;

  for (j = 0; j < width; j++)
    {
      r = *src++;
      g = *src++;
      b = *src++;

      if (r > g)
        {
          value = MAX (r, b);
          min = MIN (g, b);
        }
      else
        {
          value = MAX (g, b);
          min = MIN (r, b);
        }

      delta = value - min;
      if ((value == 0) || (delta == 0))
        {
          r = 255 - value;
          g = 255 - value;
          b = 255 - value;
        }
      else
        {
          value2 = value / 2;

          if (r == value)
            {
              r = 255 - r;
              b = ((r * b) + value2) / value;
              g = ((r * g) + value2) / value;
            }
          else if (g == value)
            {
              g = 255 - g;
              r = ((g * r) + value2) / value;
              b = ((g * b) + value2) / value;
            }
          else
            {
              b = 255 - b;
              g = ((b * g) + value2) / value;
              r = ((b * r) + value2) / value;
            }
        }

      *dest++ = r;
      *dest++ = g;
      *dest++ = b;

      if (bpp == 4)
        *dest++ = *src++;
    }
}
/* END LITERAL PINNED CALLBACK */

void
painter_legacy_vinvert_row (const unsigned char *src,
                            unsigned char       *dst,
                            int                  width,
                            int                  bpp)
{
  vinvert_render_row (src, dst, width, bpp);
}
