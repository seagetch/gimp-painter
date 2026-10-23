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
#include <math.h>
#include <cairo.h>

#include "libgimpcolor/gimpcolor.h"
#include "libgimpwidgets/gimpwidgets.h"

#include "display-types.h"

#include "base/tile-manager.h"
#include "base/tile.h"

#include "config/gimpdisplayconfig.h"

#include "core/gimpdrawable.h"
#include "core/gimpimage.h"
#include "core/gimppickable.h"
#include "core/gimpprojection.h"

#include "gimpdisplay.h"
#include "gimpdisplayshell.h"
#include "gimpdisplayshell-rotate.h"
#include "gimpdisplayshell-transform.h"

void
gimp_display_shell_set_cairo_rotate (const GimpDisplayShell *shell,
                                     cairo_t          *cr)
{
  gdouble cx, cy;

  g_return_if_fail (shell != NULL);
  g_return_if_fail (cr != NULL);
  
  cx = shell->disp_width / 2;
  cy = shell->disp_height / 2;

  cairo_translate (cr, cx, cy);
  if (shell->mirrored)
    cairo_scale(cr, -1, 1);
  cairo_rotate (cr, shell->rotate_angle / 180.0 * M_PI);
  cairo_translate (cr, -cx, -cy);
}

void
gimp_display_shell_get_device_bouding_box_for_image_coords
                                     (const GimpDisplayShell *shell,
                                      cairo_t                 *cr,
                                      gdouble                 *x1,
                                      gdouble                 *y1,
                                      gdouble                 *x2,
                                      gdouble                 *y2)
{
  gdouble user_coord[4][2];

  g_return_if_fail (shell != NULL);
  g_return_if_fail (cr != NULL);
  
  if (shell->rotate_angle == 0.0 && !shell->mirrored)
    return;

  cairo_save(cr);
  gimp_display_shell_set_cairo_rotate(shell, cr);

  user_coord[0][0] = *x1;
  user_coord[0][1] = *y1;

  user_coord[1][0] = *x2;
  user_coord[1][1] = *y1;

  user_coord[2][0] = *x1;
  user_coord[2][1] = *y2;

  user_coord[3][0] = *x2;
  user_coord[3][1] = *y2;

  cairo_user_to_device(cr, user_coord[0], user_coord[0] + 1);
  cairo_user_to_device(cr, user_coord[1], user_coord[1] + 1);
  cairo_user_to_device(cr, user_coord[2], user_coord[2] + 1);
  cairo_user_to_device(cr, user_coord[3], user_coord[3] + 1);

  gimp_display_shell_get_extents(user_coord[0][0], user_coord[0][1],
                                 user_coord[1][0], user_coord[1][1],
                                 user_coord[2][0], user_coord[2][1],
                                 user_coord[3][0], user_coord[3][1],
                                 x1, y1, x2, y2);

  cairo_restore(cr);
}

void
gimp_display_shell_get_image_bouding_box_for_device_coords
                                     (const GimpDisplayShell *shell,
                                      cairo_t                 *cr,
                                      gdouble                 *x1,
                                      gdouble                 *y1,
                                      gdouble                 *x2,
                                      gdouble                 *y2)
{
  gdouble user_coord[4][2];

  g_return_if_fail (shell != NULL);
  g_return_if_fail (cr != NULL);
  
  if (shell->rotate_angle == 0.0 && !shell->mirrored)
    return;

  cairo_save(cr);
  gimp_display_shell_set_cairo_rotate(shell, cr);

  user_coord[0][0] = *x1;
  user_coord[0][1] = *y1;

  user_coord[1][0] = *x2;
  user_coord[1][1] = *y1;

  user_coord[2][0] = *x1;
  user_coord[2][1] = *y2;

  user_coord[3][0] = *x2;
  user_coord[3][1] = *y2;

  cairo_device_to_user(cr, user_coord[0], user_coord[0] + 1);
  cairo_device_to_user(cr, user_coord[1], user_coord[1] + 1);
  cairo_device_to_user(cr, user_coord[2], user_coord[2] + 1);
  cairo_device_to_user(cr, user_coord[3], user_coord[3] + 1);

  gimp_display_shell_get_extents(user_coord[0][0], user_coord[0][1],
                                 user_coord[1][0], user_coord[1][1],
                                 user_coord[2][0], user_coord[2][1],
                                 user_coord[3][0], user_coord[3][1],
                                 x1, y1, x2, y2);

  cairo_restore(cr);
}

void
gimp_display_shell_transform_region (const GimpDisplayShell *shell,
                                     gdouble                 *x1,
                                     gdouble                 *y1,
                                     gdouble                 *x2,
                                     gdouble                 *y2)
{
  gdouble user_coord[4][2];
  gint i;
  
  user_coord[0][0] = *x1;
  user_coord[0][1] = *y1;

  user_coord[1][0] = *x2;
  user_coord[1][1] = *y1;

  user_coord[2][0] = *x1;
  user_coord[2][1] = *y2;

  user_coord[3][0] = *x2;
  user_coord[3][1] = *y2;

  for (i = 0; i < 4; i ++)
    gimp_display_shell_transform_xy_f(shell, 
                                      user_coord[i][0],
                                      user_coord[i][1],
                                      &user_coord[i][0],
                                      &user_coord[i][1]);

  gimp_display_shell_get_extents(user_coord[0][0], user_coord[0][1],
                                 user_coord[1][0], user_coord[1][1],
                                 user_coord[2][0], user_coord[2][1],
                                 user_coord[3][0], user_coord[3][1],
                                 x1, y1, x2, y2);
}

void gimp_display_shell_get_extents (gdouble x1,    gdouble y1,
                                     gdouble x2,    gdouble y2,
                                     gdouble x3,    gdouble y3,
                                     gdouble x4,    gdouble y4,
                                     gdouble *minx, gdouble *miny,
                                     gdouble *maxx, gdouble *maxy)
{
  *minx = floor(MIN(MIN(MIN(x1, x2), x3),x4));
  *miny = floor(MIN(MIN(MIN(y1, y2), y3),y4));
  *maxx = ceil(MAX(MAX(MAX(x1, x2), x3),x4));
  *maxy = ceil(MAX(MAX(MAX(y1, y2), y3),y4));
}

void
gimp_display_shell_image_to_device_coords (const GimpDisplayShell *shell,
                                           gdouble          *x,
                                           gdouble          *y)
{
  if (shell->rotate_angle != 0.0 || shell->mirrored) {
    cairo_t* cr = gdk_cairo_create(gtk_widget_get_window (shell->canvas));
    g_return_if_fail (cr != NULL);
    gimp_display_shell_set_cairo_rotate (shell, cr);
    cairo_user_to_device(cr, x, y);
    cairo_destroy(cr);
  }
  
}

void
gimp_display_shell_device_to_image_coords (const GimpDisplayShell *shell,
                                           gdouble          *x,
                                           gdouble          *y)
{
  if (shell->rotate_angle != 0.0 || shell->mirrored) {
    cairo_t* cr = gdk_cairo_create(gtk_widget_get_window (shell->canvas));
    g_return_if_fail (cr != NULL);
    gimp_display_shell_set_cairo_rotate (shell, cr);
    cairo_device_to_user(cr, x, y);
    cairo_destroy(cr);
  }
  
}
