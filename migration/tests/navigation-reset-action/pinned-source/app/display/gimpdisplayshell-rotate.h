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

#ifndef __GIMP_DISPLAY_SHELL_ROTATE_H__
#define __GIMP_DISPLAY_SHELL_ROTATE_H__


void  gimp_display_shell_set_cairo_rotate (const GimpDisplayShell *shell,
                                           cairo_t          *cr);

void gimp_display_shell_get_device_bouding_box_for_image_coords
                                           (const GimpDisplayShell *shell,
                                           cairo_t                 *cr,
                                           gdouble                 *x1,
                                           gdouble                 *y1,
                                           gdouble                 *x2,
                                           gdouble                 *y2);

void gimp_display_shell_get_image_bouding_box_for_device_coords
                                           (const GimpDisplayShell *shell,
                                           cairo_t                 *cr,
                                           gdouble                 *x1,
                                           gdouble                 *y1,
                                           gdouble                 *x2,
                                           gdouble                 *y2);

void gimp_display_shell_transform_region
                                           (const GimpDisplayShell *shell,
                                            gdouble                *x1,
                                            gdouble                *y1,
                                            gdouble                *x2,
                                            gdouble                *y2);

void gimp_display_shell_device_to_image_coords (const GimpDisplayShell *shell,
                                                gdouble                 *x,
                                                gdouble                 *y);

void gimp_display_shell_image_to_device_coords (const GimpDisplayShell *shell,
                                                gdouble                 *x,
                                                gdouble                 *y);

void gimp_display_shell_get_extents (gdouble x1, 
                                     gdouble y1,
                                     gdouble x2,
                                     gdouble y2,
                                     gdouble x3,
                                     gdouble y3,
                                     gdouble x4,
                                     gdouble y4,
                                     gdouble *minx,
                                     gdouble *miny,
                                     gdouble *maxx,
                                     gdouble *maxy);

#endif  /*  __GIMP_DISPLAY_SHELL_ROTATE_H__  */