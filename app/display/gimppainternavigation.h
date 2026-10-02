/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef __GIMP_PAINTER_NAVIGATION_H__
#define __GIMP_PAINTER_NAVIGATION_H__
#include <gdk/gdk.h>
#include "display-enums.h"

/* Stateless legacy input math. Angles at the public boundary use the modern
 * R * Flip matrix; the old display used Flip * R. No display/widget is owned. */
gdouble gimp_painter_navigation_begin (gint width, gint height, gint x, gint y,
                                       gdouble modern_angle, gboolean flip_h,
                                       gboolean flip_v);
gdouble gimp_painter_navigation_rotate (gint width, gint height, gint x, gint y,
                                        gdouble anchor, gboolean flip_h,
                                        gboolean flip_v, GdkModifierType state);
gdouble gimp_painter_navigation_zoom_begin (gint width, gint height, gint x, gint y,
                                            gdouble scale_x, gdouble scale_y);
gdouble gimp_painter_navigation_zoom (gint width, gint height, gint x, gint y,
                                      gdouble anchor);
GimpModifierAction gimp_painter_navigation_middle_action (GdkModifierType state);
GimpModifierAction gimp_painter_navigation_rotation_action (GimpModifierAction initial,
                                                            gboolean inherited,
                                                            GdkModifierType state);
guint gimp_painter_navigation_key (guint key, gboolean mirrored, gboolean press);
#endif
