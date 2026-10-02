/* SPDX-License-Identifier: GPL-3.0-or-later
 * Arithmetic preserved from gimp-painter display/tool-events.c at
 * afa43fae3e920210146abed514f136fd49f671b5. */
#include "config.h"
#include <math.h>
#include "gimppainternavigation.h"

static gdouble
bearing (gint width, gint height, gint x, gint y,
         gboolean flip_h, gboolean flip_v)
{
  /* Integer viewport centers and coordinates are intentional old behavior. */
  gdouble rx = x - width / 2;
  gdouble ry = y - height / 2;
  if (flip_h) rx = -rx;
  if (flip_v) ry = -ry;
  return fmod (atan2 (rx, ry) / G_PI * 180.0 + 360.0, 360.0);
}

static gdouble
normalize (gdouble angle)
{
  angle = fmod (angle, 360.0);
  return angle < 0 ? angle + 360.0 : angle;
}

gdouble
gimp_painter_navigation_begin (gint width, gint height, gint x, gint y,
                                gdouble modern_angle, gboolean flip_h,
                                gboolean flip_v)
{
  const gboolean odd = !!flip_h != !!flip_v;
  return normalize (odd ? -modern_angle : modern_angle) +
         bearing (width, height, x, y, flip_h, flip_v);
}

gdouble
gimp_painter_navigation_rotate (gint width, gint height, gint x, gint y,
                                 gdouble anchor, gboolean flip_h,
                                 gboolean flip_v, GdkModifierType state)
{
  gdouble angle = fmod (anchor - bearing (width, height, x, y, flip_h, flip_v) + 360.0, 360.0);
  if (state & GDK_CONTROL_MASK)
    {
      /* The old cast happens after positive normalization, not rint(). */
      angle = (gint) ((angle + 7.5) / 15.0) * 15.0;
      angle = fmod (angle + 360.0, 360.0);
    }
  return normalize ((!!flip_h != !!flip_v) ? -angle : angle);
}

static gdouble
radius (gint width, gint height, gint x, gint y)
{
  const gdouble rx = x - width / 2;
  const gdouble ry = y - height / 2;
  return sqrt (rx * rx + ry * ry);
}

gdouble
gimp_painter_navigation_zoom_begin (gint width, gint height, gint x, gint y,
                                     gdouble scale_x, gdouble scale_y)
{
  return MAX (scale_x, scale_y) * 300.0 - radius (width, height, x, y);
}

gdouble
gimp_painter_navigation_zoom (gint width, gint height, gint x, gint y, gdouble anchor)
{
  return (radius (width, height, x, y) + anchor) / 300.0;
}

GimpModifierAction
gimp_painter_navigation_middle_action (GdkModifierType state)
{
  if (state & GDK_SHIFT_MASK)
    return (state & GDK_CONTROL_MASK) ? GIMP_MODIFIER_ACTION_STEP_ROTATING : GIMP_MODIFIER_ACTION_ROTATING;
  if (state & GDK_CONTROL_MASK)
    return GIMP_MODIFIER_ACTION_ZOOMING;
  return GIMP_MODIFIER_ACTION_PANNING;
}

GimpModifierAction
gimp_painter_navigation_rotation_action (GimpModifierAction initial,
                                          gboolean inherited,
                                          GdkModifierType state)
{
  if (! inherited) return initial;
  return (state & GDK_CONTROL_MASK) ? GIMP_MODIFIER_ACTION_STEP_ROTATING :
                                     GIMP_MODIFIER_ACTION_ROTATING;
}

guint
gimp_painter_navigation_key (guint key, gboolean mirrored, gboolean press)
{
  /* The old all-key-event early path and key releases were not remapped. */
  if (mirrored && press)
    {
      if (key == GDK_KEY_Left) return GDK_KEY_Right;
      if (key == GDK_KEY_Right) return GDK_KEY_Left;
    }
  return key;
}
