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

#include <gegl.h>
#include <gtk/gtk.h>

#include "libgimpmath/gimpmath.h"

#include "display-types.h"

#include "base/boundary.h"

#include "core/gimpdrawable.h"
#include "core/gimpimage.h"

#include "gimpdisplay.h"
#include "gimpdisplayshell.h"
#include "gimpdisplayshell-scroll.h"
#include "gimpdisplayshell-rotate.h"
#include "gimpdisplayshell-transform.h"


/**
 * gimp_display_shell_transform_coords:
 * @shell:          a #GimpDisplayShell
 * @image_coords:   image coordinates
 * @display_coords: returns the corresponding display coordinates
 *
 * Transforms from image coordinates to display coordinates, so that
 * objects can be rendered at the correct points on the display.
 **/
void
gimp_display_shell_transform_coords (const GimpDisplayShell *shell,
                                     const GimpCoords       *image_coords,
                                     GimpCoords             *display_coords)
{
  g_return_if_fail (GIMP_IS_DISPLAY_SHELL (shell));
  g_return_if_fail (image_coords != NULL);
  g_return_if_fail (display_coords != NULL);

  *display_coords = *image_coords;

  display_coords->x = SCALEX (shell, image_coords->x);
  display_coords->y = SCALEY (shell, image_coords->y);

  display_coords->x -= shell->offset_x;
  display_coords->y -= shell->offset_y;

  if (shell->rotate_angle != 0.0 || shell->mirrored) {
    cairo_t* cr = gdk_cairo_create(gtk_widget_get_window (shell->canvas));
    g_return_if_fail (cr != NULL);
//    g_print("Transform:%4.1f,%4.1f->", display_coords->x, display_coords->y);
    gimp_display_shell_set_cairo_rotate (shell, cr);
    cairo_user_to_device(cr, &display_coords->x, &display_coords->y);
    cairo_destroy(cr);
//    g_print("%4.1f,%4.1f\n", display_coords->x, display_coords->y);
  }
}

/**
 * gimp_display_shell_untransform_coords:
 * @shell:          a #GimpDisplayShell
 * @display_coords: display coordinates
 * @image_coords:   returns the corresponding image coordinates
 *
 * Transforms from display coordinates to image coordinates, so that
 * points on the display can be mapped to points in the image.
 **/
void
gimp_display_shell_untransform_coords (const GimpDisplayShell *shell,
                                       const GimpCoords       *display_coords,
                                       GimpCoords             *image_coords)
{
  g_return_if_fail (GIMP_IS_DISPLAY_SHELL (shell));
  g_return_if_fail (display_coords != NULL);
  g_return_if_fail (image_coords != NULL);

  *image_coords = *display_coords;

  if (shell->rotate_angle != 0.0 || shell->mirrored) {
    cairo_t* cr = gdk_cairo_create(gtk_widget_get_window (shell->canvas));
    g_return_if_fail (cr != NULL);
    gimp_display_shell_set_cairo_rotate (shell, cr);
    cairo_device_to_user(cr, &image_coords->x, &image_coords->y);
    cairo_destroy(cr);
  }

  image_coords->x = image_coords->x + shell->offset_x;
  image_coords->y = image_coords->y + shell->offset_y;

  image_coords->x /= shell->scale_x;
  image_coords->y /= shell->scale_y;

}

/**
 * gimp_display_shell_transform_xy:
 * @shell:
 * @x:
 * @y:
 * @nx:
 * @ny:
 *
 * Transforms an image coordinate to a shell coordinate.
 **/
void
gimp_display_shell_transform_xy (const GimpDisplayShell *shell,
                                 gdouble                 x,
                                 gdouble                 y,
                                 gint                   *nx,
                                 gint                   *ny)
{
  gdouble r_tx, r_ty;

  g_return_if_fail (GIMP_IS_DISPLAY_SHELL (shell));
  g_return_if_fail (nx != NULL);
  g_return_if_fail (ny != NULL);

  r_tx = (x * shell->x_src_dec) / shell->x_dest_inc;
  r_ty = (y * shell->y_src_dec) / shell->y_dest_inc;

  r_tx -= shell->offset_x;
  r_ty -= shell->offset_y;

  if (shell->rotate_angle != 0.0 || shell->mirrored) {
    cairo_t* cr = gdk_cairo_create(gtk_widget_get_window (shell->canvas));
    g_return_if_fail (cr != NULL);
//    g_print("Transform xy:%4.1f,%4.1f->", r_tx, r_ty);
    gimp_display_shell_set_cairo_rotate (shell, cr);
    cairo_user_to_device(cr, &r_tx, &r_ty);
    cairo_destroy(cr);
//    g_print("%4.1f,%4.1f\n", r_tx, r_ty);
  }
  
  /* The projected coordinates might overflow a gint in the case of big
     images at high zoom levels, so we clamp them here to avoid problems.  */
  *nx = CLAMP (r_tx, G_MININT, G_MAXINT);
  *ny = CLAMP (r_ty, G_MININT, G_MAXINT);
}

/**
 * gimp_display_shell_untransform_xy:
 * @shell:       a #GimpDisplayShell
 * @x:           x coordinate in display coordinates
 * @y:           y coordinate in display coordinates
 * @nx:          returns x oordinate in image coordinates
 * @ny:          returns y coordinate in image coordinates
 * @round:       if %TRUE, round the results to the nearest integer;
 *               if %FALSE, simply cast them to @gint.
 *
 * Transform from display coordinates to image coordinates, so that
 * points on the display can be mapped to the corresponding points
 * in the image.
 **/
void
gimp_display_shell_untransform_xy (const GimpDisplayShell *shell,
                                   gint                    x,
                                   gint                    y,
                                   gint                   *nx,
                                   gint                   *ny,
                                   gboolean                round)
{
  gint64 tx;
  gint64 ty;

  g_return_if_fail (GIMP_IS_DISPLAY_SHELL (shell));
  g_return_if_fail (nx != NULL);
  g_return_if_fail (ny != NULL);

  if (shell->rotate_angle != 0.0 || shell->mirrored) {
    gdouble xx, yy;
    cairo_t* cr = gdk_cairo_create(gtk_widget_get_window (shell->canvas));
    g_return_if_fail (cr != NULL);
    xx = x;
    yy = y;
    gimp_display_shell_set_cairo_rotate (shell, cr);
    cairo_device_to_user(cr, &xx, &yy);
    cairo_destroy(cr);
    x = (gint)xx;
    y = (gint)yy;
  }

  tx = (gint64) x + shell->offset_x;
  ty = (gint64) y + shell->offset_y;

  tx *= shell->x_dest_inc;
  ty *= shell->y_dest_inc;

  tx += round ? shell->x_dest_inc : shell->x_dest_inc >> 1;
  ty += round ? shell->y_dest_inc : shell->y_dest_inc >> 1;

  tx /= shell->x_src_dec;
  ty /= shell->y_src_dec;

  *nx = CLAMP (tx, G_MININT, G_MAXINT);
  *ny = CLAMP (ty, G_MININT, G_MAXINT);
}

/**
 * gimp_display_shell_transform_xy_f:
 * @shell: a #GimpDisplayShell
 * @x:     image x coordinate of point
 * @y:     image y coordinate of point
 * @nx:    returned shell canvas x coordinate
 * @ny:    returned shell canvas y coordinate
 *
 * Transforms from image coordinates to display shell canvas
 * coordinates.
 **/
void
gimp_display_shell_transform_xy_f  (const GimpDisplayShell *shell,
                                    gdouble                 x,
                                    gdouble                 y,
                                    gdouble                *nx,
                                    gdouble                *ny)
{
  g_return_if_fail (GIMP_IS_DISPLAY_SHELL (shell));
  g_return_if_fail (nx != NULL);
  g_return_if_fail (ny != NULL);

  *nx = SCALEX (shell, x) - shell->offset_x;
  *ny = SCALEY (shell, y) - shell->offset_y;

  if (shell->rotate_angle != 0.0 || shell->mirrored) {
    cairo_t* cr = gdk_cairo_create(gtk_widget_get_window (shell->canvas));
    g_return_if_fail (cr != NULL);
//    g_print("Transform xy_f:%4.1f,%4.1f->", *nx, *ny);
    gimp_display_shell_set_cairo_rotate (shell, cr);
    cairo_user_to_device(cr, nx, ny);
    cairo_destroy(cr);
//    g_print("%4.1f,%4.1f\n", *nx, *ny);
  }
  
}

/**
 * gimp_display_shell_untransform_xy_f:
 * @shell:       a #GimpDisplayShell
 * @x:           x coordinate in display coordinates
 * @y:           y coordinate in display coordinates
 * @nx:          place to return x coordinate in image coordinates
 * @ny:          place to return y coordinate in image coordinates
 *
 * This function is identical to gimp_display_shell_untransform_xy(),
 * except that the input and output coordinates are doubles rather than
 * ints, and consequently there is no option related to rounding.
 **/
void
gimp_display_shell_untransform_xy_f (const GimpDisplayShell *shell,
                                     gdouble                 x,
                                     gdouble                 y,
                                     gdouble                *nx,
                                     gdouble                *ny)
{
  g_return_if_fail (GIMP_IS_DISPLAY_SHELL (shell));
  g_return_if_fail (nx != NULL);
  g_return_if_fail (ny != NULL);

  if (shell->rotate_angle != 0.0 || shell->mirrored) {
    cairo_t* cr = gdk_cairo_create(gtk_widget_get_window (shell->canvas));
    gimp_display_shell_set_cairo_rotate (shell, cr);
    cairo_device_to_user(cr, &x, &y);
    cairo_destroy(cr);
  }

  *nx = (x + shell->offset_x) / shell->scale_x;
  *ny = (y + shell->offset_y) / shell->scale_y;
}

/**
 * gimp_display_shell_zoom_xy_f:
 * @shell: a #GimpDisplayShell
 * @x:     image x coordinate of point
 * @y:     image y coordinate of point
 * @nx:    returned shell canvas x coordinate
 * @ny:    returned shell canvas y coordinate
 *
 * Scale from image coordinates to display shell canvas
 * coordinates. This function doesn't transform scaled coordinate into 
 * rotated view coordinates.
 **/
void
gimp_display_shell_zoom_xy_f  (const GimpDisplayShell *shell,
                               gdouble                 x,
                               gdouble                 y,
                               gdouble                *nx,
                               gdouble                *ny)
{
  g_return_if_fail (GIMP_IS_DISPLAY_SHELL (shell));
  g_return_if_fail (nx != NULL);
  g_return_if_fail (ny != NULL);

  *nx = SCALEX (shell, x) - shell->offset_x;
  *ny = SCALEY (shell, y) - shell->offset_y;
}

/**
 * gimp_display_shell_unzoom_xy_f:
 * @shell:       a #GimpDisplayShell
 * @x:           x coordinate in display coordinates (not rotated)
 * @y:           y coordinate in display coordinates (not rotated)
 * @nx:          place to return x coordinate in image coordinates
 * @ny:          place to return y coordinate in image coordinates
 *
 * This function is identical to gimp_display_shell_untransform_xy(),
 * except that the input and output coordinates are doubles rather than
 * ints, and consequently there is no option related to rounding.
 **/
void
gimp_display_shell_unzoom_xy_f (const GimpDisplayShell *shell,
                                gdouble                 x,
                                gdouble                 y,
                                gdouble                *nx,
                                gdouble                *ny)
{
  g_return_if_fail (GIMP_IS_DISPLAY_SHELL (shell));
  g_return_if_fail (nx != NULL);
  g_return_if_fail (ny != NULL);

  *nx = (x + shell->offset_x) / shell->scale_x;
  *ny = (y + shell->offset_y) / shell->scale_y;
}
/**
 * gimp_display_shell_transform_segments:
 * @shell:       a #GimpDisplayShell
 * @src_segs:    array of segments in image coordinates
 * @dest_segs:   returns the corresponding segments in display coordinates
 * @n_segs:      number of segments
 *
 * Transforms from image coordinates to display coordinates, so that
 * objects can be rendered at the correct points on the display.
 **/
void
gimp_display_shell_transform_segments (const GimpDisplayShell *shell,
                                       const BoundSeg         *src_segs,
                                       GimpSegment            *dest_segs,
                                       gint                    n_segs,
                                       gdouble                 offset_x,
                                       gdouble                 offset_y)
{
  gint i;
  cairo_t* cr = NULL;

  g_return_if_fail (GIMP_IS_DISPLAY_SHELL (shell));

  if (shell->rotate_angle != 0.0 || shell->mirrored) {
    cr = gdk_cairo_create(gtk_widget_get_window (shell->canvas));
    g_return_if_fail (cr != NULL);
    gimp_display_shell_set_cairo_rotate (shell, cr);
  }
  

  
  for (i = 0; i < n_segs ; i++)
    {
      gdouble x1, x2;
      gdouble y1, y2;

      x1 = src_segs[i].x1 + offset_x;
      x2 = src_segs[i].x2 + offset_x;
      y1 = src_segs[i].y1 + offset_y;
      y2 = src_segs[i].y2 + offset_y;

      x1 = SCALEX (shell, x1) - shell->offset_x;
      x2 = SCALEX (shell, x2) - shell->offset_x;
      y1 = SCALEY (shell, y1) - shell->offset_y;
      y2 = SCALEY (shell, y2) - shell->offset_y;

      if (cr) {
        cairo_user_to_device(cr, &x1, &y1);
        cairo_user_to_device(cr, &x2, &y2);
      }
      dest_segs[i].x1 = x1;
      dest_segs[i].x2 = x2;
      dest_segs[i].y1 = y1;
      dest_segs[i].y2 = y2;
    }

  if (cr)
    cairo_destroy(cr);
}

/**
 * gimp_display_shell_untransform_viewport:
 * @shell:  a #GimpDisplayShell
 * @x:      returns image x coordinate of display upper left corner
 * @y:      returns image y coordinate of display upper left corner
 * @width:  returns width of display measured in image coordinates
 * @height: returns height of display measured in image coordinates
 *
 * This function calculates the part of the image, im image coordinates,
 * that corresponds to the display viewport.
 **/
void
gimp_display_shell_untransform_viewport (const GimpDisplayShell *shell,
                                         gint                   *x,
                                         gint                   *y,
                                         gint                   *width,
                                         gint                   *height)
{
  GimpImage *image;
  gint       x1, y1, x2, y2;

  g_return_if_fail (GIMP_IS_DISPLAY_SHELL (shell));

  gimp_display_shell_untransform_xy (shell,
                                     0, 0,
                                     &x1, &y1,
                                     FALSE);
  gimp_display_shell_untransform_xy (shell,
                                     shell->disp_width, shell->disp_height,
                                     &x2, &y2,
                                     FALSE);

  image = gimp_display_get_image (shell->display);

  if (x1 < 0)
    x1 = 0;

  if (y1 < 0)
    y1 = 0;

  if (x2 > gimp_image_get_width (image))
    x2 = gimp_image_get_width (image);

  if (y2 > gimp_image_get_height (image))
    y2 = gimp_image_get_height (image);

  if (x)      *x      = x1;
  if (y)      *y      = y1;
  if (width)  *width  = x2 - x1;
  if (height) *height = y2 - y1;
}
