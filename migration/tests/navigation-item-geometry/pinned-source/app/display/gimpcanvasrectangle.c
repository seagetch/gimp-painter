/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * gimpcanvasrectangle.c
 * Copyright (C) 2010 Michael Natterer <mitch@gimp.org>
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

#include "libgimpbase/gimpbase.h"
#include "libgimpmath/gimpmath.h"

#include "display-types.h"

#include "gimpcanvasrectangle.h"
#include "gimpdisplayshell.h"
#include "gimpdisplayshell-transform.h"
#include "gimpdisplayshell-rotate.h"


enum
{
  PROP_0,
  PROP_X,
  PROP_Y,
  PROP_WIDTH,
  PROP_HEIGHT,
  PROP_FILLED
};


typedef struct _GimpCanvasRectanglePrivate GimpCanvasRectanglePrivate;

struct _GimpCanvasRectanglePrivate
{
  gdouble  x;
  gdouble  y;
  gdouble  width;
  gdouble  height;
  gboolean filled;
};

#define GET_PRIVATE(rectangle) \
        G_TYPE_INSTANCE_GET_PRIVATE (rectangle, \
                                     GIMP_TYPE_CANVAS_RECTANGLE, \
                                     GimpCanvasRectanglePrivate)


/*  local function prototypes  */

static void             gimp_canvas_rectangle_set_property (GObject          *object,
                                                            guint             property_id,
                                                            const GValue     *value,
                                                            GParamSpec       *pspec);
static void             gimp_canvas_rectangle_get_property (GObject          *object,
                                                            guint             property_id,
                                                            GValue           *value,
                                                            GParamSpec       *pspec);
static void             gimp_canvas_rectangle_draw         (GimpCanvasItem   *item,
                                                            GimpDisplayShell *shell,
                                                            cairo_t          *cr);
static cairo_region_t * gimp_canvas_rectangle_get_extents  (GimpCanvasItem   *item,
                                                            GimpDisplayShell *shell);


G_DEFINE_TYPE (GimpCanvasRectangle, gimp_canvas_rectangle,
               GIMP_TYPE_CANVAS_ITEM)

#define parent_class gimp_canvas_rectangle_parent_class


static void
gimp_canvas_rectangle_class_init (GimpCanvasRectangleClass *klass)
{
  GObjectClass        *object_class = G_OBJECT_CLASS (klass);
  GimpCanvasItemClass *item_class   = GIMP_CANVAS_ITEM_CLASS (klass);

  object_class->set_property = gimp_canvas_rectangle_set_property;
  object_class->get_property = gimp_canvas_rectangle_get_property;

  item_class->draw           = gimp_canvas_rectangle_draw;
  item_class->get_extents    = gimp_canvas_rectangle_get_extents;

  g_object_class_install_property (object_class, PROP_X,
                                   g_param_spec_double ("x", NULL, NULL,
                                                        -GIMP_MAX_IMAGE_SIZE,
                                                        GIMP_MAX_IMAGE_SIZE, 0,
                                                        GIMP_PARAM_READWRITE));

  g_object_class_install_property (object_class, PROP_Y,
                                   g_param_spec_double ("y", NULL, NULL,
                                                        -GIMP_MAX_IMAGE_SIZE,
                                                        GIMP_MAX_IMAGE_SIZE, 0,
                                                        GIMP_PARAM_READWRITE));

  g_object_class_install_property (object_class, PROP_WIDTH,
                                   g_param_spec_double ("width", NULL, NULL,
                                                        -GIMP_MAX_IMAGE_SIZE,
                                                        GIMP_MAX_IMAGE_SIZE, 0,
                                                        GIMP_PARAM_READWRITE));

  g_object_class_install_property (object_class, PROP_HEIGHT,
                                   g_param_spec_double ("height", NULL, NULL,
                                                        -GIMP_MAX_IMAGE_SIZE,
                                                        GIMP_MAX_IMAGE_SIZE, 0,
                                                        GIMP_PARAM_READWRITE));

  g_object_class_install_property (object_class, PROP_FILLED,
                                   g_param_spec_boolean ("filled", NULL, NULL,
                                                         FALSE,
                                                         GIMP_PARAM_READWRITE));

  g_type_class_add_private (klass, sizeof (GimpCanvasRectanglePrivate));
}

static void
gimp_canvas_rectangle_init (GimpCanvasRectangle *rectangle)
{
}

static void
gimp_canvas_rectangle_set_property (GObject      *object,
                                    guint         property_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  GimpCanvasRectanglePrivate *private = GET_PRIVATE (object);

  switch (property_id)
    {
    case PROP_X:
      private->x = g_value_get_double (value);
      break;
    case PROP_Y:
      private->y = g_value_get_double (value);
      break;
    case PROP_WIDTH:
      private->width = g_value_get_double (value);
      break;
    case PROP_HEIGHT:
      private->height = g_value_get_double (value);
      break;
    case PROP_FILLED:
      private->filled = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gimp_canvas_rectangle_get_property (GObject    *object,
                                    guint       property_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  GimpCanvasRectanglePrivate *private = GET_PRIVATE (object);

  switch (property_id)
    {
    case PROP_X:
      g_value_set_double (value, private->x);
      break;
    case PROP_Y:
      g_value_set_double (value, private->y);
      break;
    case PROP_WIDTH:
      g_value_set_double (value, private->width);
      break;
    case PROP_HEIGHT:
      g_value_set_double (value, private->height);
      break;
    case PROP_FILLED:
      g_value_set_boolean (value, private->filled);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gimp_canvas_rectangle_transform (GimpCanvasItem   *item,
                                 GimpDisplayShell *shell,
                                 gdouble          *x1,
                                 gdouble          *y1,
                                 gdouble          *x2,
                                 gdouble          *y2,
                                 gdouble          *x3,
                                 gdouble          *y3,
                                 gdouble          *x4,
                                 gdouble          *y4)
{
  GimpCanvasRectanglePrivate *private = GET_PRIVATE (item);

  gimp_display_shell_transform_xy_f (shell,
                                     MIN (private->x,
                                          private->x + private->width),
                                     MIN (private->y,
                                          private->y + private->height),
                                     x1, y1);
  gimp_display_shell_transform_xy_f (shell,
                                     MAX (private->x,
                                          private->x + private->width),
                                     MIN (private->y,
                                          private->y + private->height),
                                     x2, y2);
  gimp_display_shell_transform_xy_f (shell,
                                     MIN (private->x,
                                          private->x + private->width),
                                     MAX (private->y,
                                          private->y + private->height),
                                     x3, y3);
  gimp_display_shell_transform_xy_f (shell,
                                     MAX (private->x,
                                          private->x + private->width),
                                     MAX (private->y,
                                          private->y + private->height),
                                     x4, y4);

#if 0
  *x1 = floor (*x1);
  *y1 = floor (*y1);
  *x2 = ceil (*x2);
  *y2 = ceil (*y2);

  if (private->filled)
    {
      *x = x1;
      *y = y1;
      *w = x2 - x1;
      *h = y2 - y1;
    }
  else
    {
      *x = x1 + 0.5;
      *y = y1 + 0.5;
      *w = x2 - 0.5 - *x;
      *h = y2 - 0.5 - *y;

      *w = MAX (0.0, *w);
      *h = MAX (0.0, *h);
    }
#endif
}

static void
gimp_canvas_rectangle_draw (GimpCanvasItem   *item,
                            GimpDisplayShell *shell,
                            cairo_t          *cr)
{
  GimpCanvasRectanglePrivate *private = GET_PRIVATE (item);
  gdouble x1, y1;
  gdouble x2, y2;
  gdouble x3, y3;
  gdouble x4, y4;

  gimp_canvas_rectangle_transform (item, shell, &x1, &y1, &x2, &y2, &x3, &y3, &x4, &y4);

  if (x1 == x2 && y1 == y2 && x1 == x3 && y1 == y3)
    return;

  cairo_move_to(cr, x1, y1);
  cairo_line_to(cr, x2, y2);
  cairo_line_to(cr, x4, y4);
  cairo_line_to(cr, x3, y3);
  cairo_close_path(cr);

  if (private->filled)
    _gimp_canvas_item_fill (item, cr);
  else
    _gimp_canvas_item_stroke (item, cr);
}

static cairo_region_t *
gimp_canvas_rectangle_get_extents (GimpCanvasItem   *item,
                                   GimpDisplayShell *shell)
{
  GimpCanvasRectanglePrivate *private = GET_PRIVATE (item);
  cairo_rectangle_int_t       rectangle;
  gdouble                     x, y;
  gdouble                     w, h;
  gdouble x1, y1, x2, y2, x3, y3, x4, y4;

  gimp_canvas_rectangle_transform (item, shell, &x1, &y1, &x2, &y2, &x3, &y3, &x4, &y4);
  gimp_display_shell_get_extents(x1, y1, x2, y2, x3, y3, x4, y4, &x, &y, &w, &h);
  x -= 1.5;
  y -= 1.5;
  w = w - x + 1.5;
  h = h - y + 1.5;

  if (private->filled)
    {
      rectangle.x      = floor (x);
      rectangle.y      = floor (y);
      rectangle.width  = ceil (w);
      rectangle.height = ceil (h);

      return cairo_region_create_rectangle (&rectangle);
    }
  else if (w > 64 && h > 64)
    {
      cairo_region_t *region;

      /* left */
      rectangle.x      = floor (MIN(x1, x3) - 1.5);
      rectangle.y      = floor (MIN(y1, y3) - 1.5);
      rectangle.width  = ceil (MAX(x1, x3) - rectangle.x + 1.5);
      rectangle.height = ceil (MAX(y1, y3) - rectangle.y + 1.5);

      region = cairo_region_create_rectangle (&rectangle);

      /* right */
      rectangle.x      = floor (MIN(x2, x4) - 1.5);
      rectangle.y      = floor (MIN(y2, y4) - 1.5);
      rectangle.width  = ceil (MAX(x2, x4) - rectangle.x + 1.5);
      rectangle.height = ceil (MAX(y2, y4) - rectangle.y + 1.5);

      cairo_region_union_rectangle (region, &rectangle);

      /* top */
      rectangle.x      = floor (MIN(x1, x2) - 1.5);
      rectangle.y      = floor (MIN(y1, y2) - 1.5);
      rectangle.width  = ceil (MAX(x1, x2) - rectangle.x + 1.5);
      rectangle.height = ceil (MAX(y1, y2) - rectangle.y + 1.5);

      cairo_region_union_rectangle (region, &rectangle);

      /* bottom */
      rectangle.x      = floor (MIN(x3, x4) - 1.5);
      rectangle.y      = floor (MIN(y3, y4) - 1.5);
      rectangle.width  = ceil (MAX(x3, x4) - rectangle.x + 1.5);
      rectangle.height = ceil (MAX(y3, y4) - rectangle.y + 1.5);

      cairo_region_union_rectangle (region, &rectangle);

      return region;
    }
  else
    {
      rectangle.x      = floor (x);
      rectangle.y      = floor (y);
      rectangle.width  = ceil (w);
      rectangle.height = ceil (h);

      return cairo_region_create_rectangle (&rectangle);
    }
}

GimpCanvasItem *
gimp_canvas_rectangle_new (GimpDisplayShell *shell,
                           gdouble           x,
                           gdouble           y,
                           gdouble           width,
                           gdouble           height,
                           gboolean          filled)
{
  g_return_val_if_fail (GIMP_IS_DISPLAY_SHELL (shell), NULL);

  return g_object_new (GIMP_TYPE_CANVAS_RECTANGLE,
                       "shell",  shell,
                       "x",      x,
                       "y",      y,
                       "width",  width,
                       "height", height,
                       "filled", filled,
                       NULL);
}

void
gimp_canvas_rectangle_set (GimpCanvasItem *rectangle,
                           gdouble         x,
                           gdouble         y,
                           gdouble         width,
                           gdouble         height)
{
  g_return_if_fail (GIMP_IS_CANVAS_RECTANGLE (rectangle));

  gimp_canvas_item_begin_change (rectangle);

  g_object_set (rectangle,
                "x",      x,
                "y",      y,
                "width",  width,
                "height", height,
                NULL);

  gimp_canvas_item_end_change (rectangle);
}
