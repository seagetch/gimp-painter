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

#include "config.h"

#include <cairo.h>
#include <gegl.h>

#include "core-types.h"

#include "base/temp-buf.h"

#include "gegl/gimp-gegl-utils.h"

#include "gimpbezierdesc.h"
#include "gimpboundary.h"
#include "gimpbrush.h"
#include "gimpbrush-boundary.h"


static GimpBezierDesc *
gimp_brush_transform_boundary_exact (GimpBrush *brush,
                                     gdouble    scale,
                                     gdouble    aspect_ratio,
                                     gdouble    angle,
                                     gdouble    hardness)
{
  const TempBuf *mask;

  mask = gimp_brush_transform_mask (brush,
                                    scale, aspect_ratio, angle, hardness);

  if (mask)
    {
      GeglBuffer    *buffer;
      GimpBoundSeg  *bound_segs;
      gint           n_bound_segs;

      buffer = gegl_buffer_linear_new_from_data (mask->data,
                                                 babl_format ("Y u8"),
                                                 GIMP_GEGL_RECT (0,0,mask->width,
                                                                     mask->height),
                                                 GEGL_AUTO_ROWSTRIDE,
                                                 NULL, NULL);

      bound_segs = gimp_boundary_find (buffer, NULL,
                                       GIMP_BOUNDARY_WITHIN_BOUNDS,
                                       0, 0, mask->width, mask->height,
                                       0,
                                       &n_bound_segs);

      g_object_unref (buffer);

      if (bound_segs)
        {
          GimpBoundSeg *stroke_segs;
          gint          n_stroke_groups;

          stroke_segs = gimp_boundary_sort (bound_segs, n_bound_segs,
                                            &n_stroke_groups);

          g_free (bound_segs);

          if (stroke_segs)
            {
              GimpBezierDesc *path;

              path = gimp_bezier_desc_new_from_bound_segs (stroke_segs,
                                                           n_bound_segs,
                                                           n_stroke_groups);

              g_free (stroke_segs);

              return path;
            }
        }
    }

  return NULL;
}

static GimpBezierDesc *
gimp_brush_transform_boundary_approx (GimpBrush *brush,
                                      gdouble    scale,
                                      gdouble    aspect_ratio,
                                      gdouble    angle,
                                      gdouble    hardness)
{
  return gimp_brush_transform_boundary_exact (brush,
                                              scale, aspect_ratio,
                                              angle, hardness);
}

GimpBezierDesc *
gimp_brush_real_transform_boundary (GimpBrush *brush,
                                    gdouble    scale,
                                    gdouble    aspect_ratio,
                                    gdouble    angle,
                                    gdouble    hardness,
                                    gint      *width,
                                    gint      *height)
{
  gimp_brush_transform_size (brush, scale, aspect_ratio, angle,
                             width, height);

  if (*width < 256 && *height < 256)
    {
      return gimp_brush_transform_boundary_exact (brush,
                                                  scale, aspect_ratio,
                                                  angle, hardness);
    }

  return gimp_brush_transform_boundary_approx (brush,
                                               scale, aspect_ratio,
                                               angle, hardness);
}