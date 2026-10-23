/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * gimpcanvascorner.c
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
#include <math.h>

#include "libgimpbase/gimpbase.h"
#include "libgimpmath/gimpmath.h"

#include "display-types.h"

#include "gimpcanvascorner.h"
#include "gimpdisplayshell.h"
#include "gimpdisplayshell-transform.h"


enum
{
  PROP_0,
  PROP_X,
  PROP_Y,
  PROP_WIDTH,
  PROP_HEIGHT,
  PROP_ANCHOR,
  PROP_CORNER_WIDTH,
  PROP_CORNER_HEIGHT,
  PROP_OUTSIDE
};


typedef struct _GimpCanvasCornerPrivate GimpCanvasCornerPrivate;

struct _GimpCanvasCornerPrivate
{
  gdouble          x;
  gdouble          y;
  gdouble          width;
  gdouble          height;
  GimpHandleAnchor anchor;
  gint             corner_width;
  gint             corner_height;
  gboolean         outside;
};

#define GET_PRIVATE(corner) \
        G_TYPE_INSTANCE_GET_PRIVATE (corner, \
                                     GIMP_TYPE_CANVAS_CORNER, \
                                     GimpCanvasCornerPrivate)


/*  local function prototypes  */

static void             gimp_canvas_corner_set_property (GObject          *object,
                                                         guint             property_id,
                                                         const GValue     *value,
                                                         GParamSpec       *pspec);
static void             gimp_canvas_corner_get_property (GObject          *object,
                                                         guint             property_id,
                                                         GValue           *value,
                                                         GParamSpec       *pspec);
static void             gimp_canvas_corner_draw         (GimpCanvasItem   *item,
                                                         GimpDisplayShell *shell,
                                                         cairo_t          *cr);
static cairo_region_t * gimp_canvas_corner_get_extents  (GimpCanvasItem   *item,
                                                         GimpDisplayShell *shell);


G_DEFINE_TYPE (GimpCanvasCorner, gimp_canvas_corner,
               GIMP_TYPE_CANVAS_ITEM)

#define parent_class gimp_canvas_corner_parent_class


static void
gimp_canvas_corner_class_init (GimpCanvasCornerClass *klass)
{
  GObjectClass        *object_class = G_OBJECT_CLASS (klass);
  GimpCanvasItemClass *item_class   = GIMP_CANVAS_ITEM_CLASS (klass);

  object_class->set_property = gimp_canvas_corner_set_property;
  object_class->get_property = gimp_canvas_corner_get_property;

  item_class->draw           = gimp_canvas_corner_draw;
  item_class->get_extents    = gimp_canvas_corner_get_extents;

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

  g_object_class_install_property (object_class, PROP_ANCHOR,
                                   g_param_spec_enum ("anchor", NULL, NULL,
                                                      GIMP_TYPE_HANDLE_ANCHOR,
                                                      GIMP_HANDLE_ANCHOR_CENTER,
                                                      GIMP_PARAM_READWRITE));

  g_object_class_install_property (object_class, PROP_CORNER_WIDTH,
                                   g_param_spec_int ("corner-width", NULL, NULL,
                                                     3, GIMP_MAX_IMAGE_SIZE, 3,
                                                     GIMP_PARAM_READWRITE));

  g_object_class_install_property (object_class, PROP_CORNER_HEIGHT,
                                   g_param_spec_int ("corner-height", NULL, NULL,
                                                     3, GIMP_MAX_IMAGE_SIZE, 3,
                                                     GIMP_PARAM_READWRITE));

  g_object_class_install_property (object_class, PROP_OUTSIDE,
                                   g_param_spec_boolean ("outside", NULL, NULL,
                                                         FALSE,
                                                         GIMP_PARAM_READWRITE));

  g_type_class_add_private (klass, sizeof (GimpCanvasCornerPrivate));
}

static void
gimp_canvas_corner_init (GimpCanvasCorner *corner)
{
}

static void
gimp_canvas_corner_set_property (GObject      *object,
                                 guint         property_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  GimpCanvasCornerPrivate *private = GET_PRIVATE (object);

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
    case PROP_ANCHOR:
      private->anchor = g_value_get_enum (value);
      break;
    case PROP_CORNER_WIDTH:
      private->corner_width = g_value_get_int (value);
      break;
    case PROP_CORNER_HEIGHT:
      private->corner_height = g_value_get_int (value);
      break;
   case PROP_OUTSIDE:
      private->outside = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gimp_canvas_corner_get_property (GObject    *object,
                                 guint       property_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  GimpCanvasCornerPrivate *private = GET_PRIVATE (object);

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
    case PROP_ANCHOR:
      g_value_set_enum (value, private->anchor);
      break;
    case PROP_CORNER_WIDTH:
      g_value_set_int (value, private->corner_width);
      break;
    case PROP_CORNER_HEIGHT:
      g_value_set_int (value, private->corner_height);
      break;
    case PROP_OUTSIDE:
      g_value_set_boolean (value, private->outside);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gimp_canvas_corner_transform (GimpCanvasItem   *item,
                              GimpDisplayShell *shell,
                              gdouble *x1, gdouble *y1, 
                              gdouble *x2, gdouble *y2, 
                              gdouble *x3, gdouble *y3, 
                              gdouble *x4,  gdouble *y4)
{
  GimpCanvasCornerPrivate *private = GET_PRIVATE (item);
  gdouble                  rx1, ry1, rx2, ry2, rx3, ry3, rx4, ry4;
  gdouble                  vec_xx, vec_xy, vec_yx, vec_yy;
  gdouble                  vec_x_len, vec_y_len;
  gdouble                  h_handle_x_offset;
  gdouble                  v_handle_y_offset;
  
  gimp_display_shell_transform_xy_f (shell,
                                     MIN (private->x,
                                          private->x + private->width),
                                     MIN (private->y,
                                          private->y + private->height),
                                     &rx1, &ry1);
  gimp_display_shell_transform_xy_f (shell,
                                     MAX (private->x,
                                          private->x + private->width),
                                     MIN (private->y,
                                          private->y + private->height),
                                     &rx2, &ry2);
  gimp_display_shell_transform_xy_f (shell,
                                     MIN (private->x,
                                          private->x + private->width),
                                     MAX (private->y,
                                          private->y + private->height),
                                     &rx3, &ry3);
  gimp_display_shell_transform_xy_f (shell,
                                     MAX (private->x,
                                          private->x + private->width),
                                     MAX (private->y,
                                          private->y + private->height),
                                     &rx4, &ry4);

  vec_xx = (rx2 - rx1);
  vec_xy = (ry2 - ry1);
  vec_x_len = sqrt(vec_xx*vec_xx + vec_xy*vec_xy);
  if (vec_x_len > 0) {
    vec_xx /= vec_x_len;
    vec_xy /= vec_x_len;
  }
  vec_yx = (rx3 - rx1);
  vec_yy = (ry3 - ry1);
  vec_y_len = sqrt(vec_yx*vec_yx + vec_yy*vec_yy);
  if (vec_y_len > 0) {
    vec_yx /= vec_y_len;
    vec_yy /= vec_y_len;
  }

  h_handle_x_offset = (vec_x_len - private->corner_width) / 2.0;
  v_handle_y_offset = (vec_y_len - private->corner_height) / 2.0;

  switch (private->anchor)
    {
    case GIMP_HANDLE_ANCHOR_CENTER:
      break;

    case GIMP_HANDLE_ANCHOR_NORTH_WEST:
      if (private->outside)
        {
          *x1 = rx1 
            - private->corner_width * vec_xx 
            - private->corner_height * vec_yx;
          *y1 = ry1
            - private->corner_width * vec_xy 
            - private->corner_height * vec_yy;
        }
      else
        {
          *x1 = rx1;
          *y1 = ry1;
        }
      *x2 = *x1 + vec_xx * private->corner_width;
      *y2 = *y1 + vec_xy * private->corner_width;
      *x3 = *x1 + vec_yx * private->corner_height;
      *y3 = *y1 + vec_yy * private->corner_height;
      *x4 = *x1 + vec_xx * private->corner_width + vec_yx * private->corner_height;
      *y4 = *y1 + vec_xy * private->corner_width + vec_yy * private->corner_height;
      break;

    case GIMP_HANDLE_ANCHOR_NORTH_EAST:
      if (private->outside)
        {
          *x1 = rx2 - vec_yx * private->corner_height;
          *y1 = ry2 - vec_yy * private->corner_height;
        }
      else
        {
          *x1 = rx2 
            - vec_xx * private->corner_width;
          *y1 = ry2
            - vec_xy * private->corner_width;
        }
      *x2 = *x1 + vec_xx * private->corner_width;
      *y2 = *y1 + vec_xy * private->corner_width;
      *x3 = *x1 + vec_yx * private->corner_height;
      *y3 = *y1 + vec_yy * private->corner_height;
      *x4 = *x1 + vec_xx * private->corner_width + vec_yx * private->corner_height;
      *y4 = *y1 + vec_xy * private->corner_width + vec_yy * private->corner_height;
      break;

    case GIMP_HANDLE_ANCHOR_SOUTH_WEST:
      if (private->outside)
        {
          *x1 = rx3
            - vec_xx * private->corner_width;
          *y1 = ry3
            - vec_xy * private->corner_width;
        }
      else
        {
          *x1 = rx3
            - vec_yx * private->corner_height;
          *y1 = ry3
            - vec_yy * private->corner_height;
        }
      *x2 = *x1 + vec_xx * private->corner_width;
      *y2 = *y1 + vec_xy * private->corner_width;
      *x3 = *x1 + vec_yx * private->corner_height;
      *y3 = *y1 + vec_yy * private->corner_height;
      *x4 = *x1 + vec_xx * private->corner_width + vec_yx * private->corner_height;
      *y4 = *y1 + vec_xy * private->corner_width + vec_yy * private->corner_height;
      break;

    case GIMP_HANDLE_ANCHOR_SOUTH_EAST:
      if (private->outside)
        {
          *x1 = rx4;
          *y1 = ry4;
        }
      else
        {
          *x1 = rx4 
            - vec_xx * private->corner_width
            - vec_yx * private->corner_height;
          *y1 = ry4 
            - vec_xy * private->corner_width
            - vec_yy * private->corner_height;
        }
      *x2 = *x1 + vec_xx * private->corner_width;
      *y2 = *y1 + vec_xy * private->corner_width;
      *x3 = *x1 + vec_yx * private->corner_height;
      *y3 = *y1 + vec_yy * private->corner_height;
      *x4 = *x1 + vec_xx * private->corner_width + vec_yx * private->corner_height;
      *y4 = *y1 + vec_xy * private->corner_width + vec_yy * private->corner_height;
      break;

    case GIMP_HANDLE_ANCHOR_NORTH:
      if (private->outside)
        {
          *x1 = rx1 - vec_yx * private->corner_height;
          *y1 = ry1 - vec_yy * private->corner_height;
          *x2 = *x1 + vec_xx * vec_x_len;
          *y2 = *y1 + vec_xy * vec_x_len;
          *x3 = *x1 + vec_yx * private->corner_height;
          *y3 = *y1 + vec_yy * private->corner_height;
          *x4 = *x1 + vec_xx * vec_x_len + vec_yx * private->corner_height;
          *y4 = *y1 + vec_xy * vec_x_len + vec_yy * private->corner_height;
        }
      else
        {
          *x1 = rx1 + vec_xx * h_handle_x_offset;
          *y1 = ry1 + vec_xy * h_handle_x_offset;
          *x2 = *x1 + vec_xx * private->corner_width;
          *y2 = *y1 + vec_xy * private->corner_width;
          *x3 = *x1 + vec_yx * private->corner_height;
          *y3 = *y1 + vec_yy * private->corner_height;
          *x4 = *x1 + vec_xx * private->corner_width + vec_yx * private->corner_height;
          *y4 = *y1 + vec_xy * private->corner_width + vec_yy * private->corner_height;
        }
      break;

    case GIMP_HANDLE_ANCHOR_SOUTH:
      if (private->outside)
        {
          *x1 = rx3;
          *y1 = ry3;
          *x2 = *x1 + vec_xx * vec_x_len;
          *y2 = *y1 + vec_xy * vec_x_len;
          *x3 = *x1 + vec_yx * private->corner_height;
          *y3 = *y1 + vec_yy * private->corner_height;
          *x4 = *x1 + vec_xx * vec_x_len + vec_yx * private->corner_height;
          *y4 = *y1 + vec_xy * vec_x_len + vec_yy * private->corner_height;
        }
      else
        {
          *x1 = rx3 
            - vec_yx * private->corner_height
            + vec_xx * h_handle_x_offset;
          *y1 = ry3 
            - vec_yy * private->corner_height
            + vec_xy * h_handle_x_offset;
          *x2 = *x1 + vec_xx * private->corner_width;
          *y2 = *y1 + vec_xy * private->corner_width;
          *x3 = *x1 + vec_yx * private->corner_height;
          *y3 = *y1 + vec_yy * private->corner_height;
          *x4 = *x1 + vec_xx * private->corner_width + vec_yx * private->corner_height;
          *y4 = *y1 + vec_xy * private->corner_width + vec_yy * private->corner_height;
        }
      break;

    case GIMP_HANDLE_ANCHOR_WEST:
      if (private->outside)
        {
          *x1 = rx1 - vec_xx * private->corner_width;
          *y1 = ry1 - vec_xy * private->corner_width;
          *x2 = *x1 + vec_xx * private->corner_width;
          *y2 = *y1 + vec_xy * private->corner_width;
          *x3 = *x1 + vec_yx * vec_y_len;
          *y3 = *y1 + vec_yy * vec_y_len;
          *x4 = *x1 + vec_xx * private->corner_width + vec_yx * vec_y_len;
          *y4 = *y1 + vec_xy * private->corner_width + vec_yy * vec_y_len;
        }
      else
        {
          *x1 = rx1 + vec_yx * v_handle_y_offset;
          *y1 = ry1 + vec_yy * v_handle_y_offset;
          *x2 = *x1 + vec_xx * private->corner_width;
          *y2 = *y1 + vec_xy * private->corner_width;
          *x3 = *x1 + vec_yx * private->corner_height;
          *y3 = *y1 + vec_yy * private->corner_height;
          *x4 = *x1 + vec_xx * private->corner_width + vec_yx * private->corner_height;
          *y4 = *y1 + vec_xy * private->corner_width + vec_yy * private->corner_height;
        }
      break;

    case GIMP_HANDLE_ANCHOR_EAST:
      if (private->outside)
        {
          *x1 = rx2;
          *y1 = ry2;
          *x2 = *x1 + vec_xx * private->corner_width;
          *y2 = *y1 + vec_xy * private->corner_width;
          *x3 = *x1 + vec_yx * vec_y_len;
          *y3 = *y1 + vec_yy * vec_y_len;
          *x4 = *x1 + vec_xx * private->corner_width + vec_yx * vec_y_len;
          *y4 = *y1 + vec_xy * private->corner_width + vec_yy * vec_y_len;
        }
      else
        {
          *x1 = rx2 
            + vec_yx * v_handle_y_offset
            - vec_xx * private->corner_width;
          *y1 = ry2 
            + vec_yy * v_handle_y_offset
            - vec_xy * private->corner_width;
          *x2 = *x1 + vec_xx * private->corner_width;
          *y2 = *y1 + vec_xy * private->corner_width;
          *x3 = *x1 + vec_yx * private->corner_height;
          *y3 = *y1 + vec_yy * private->corner_height;
          *x4 = *x1 + vec_xx * private->corner_width + vec_yx * private->corner_height;
          *y4 = *y1 + vec_xy * private->corner_width + vec_yy * private->corner_height;
        }
      break;
    }
}

static void
gimp_canvas_corner_draw (GimpCanvasItem   *item,
                         GimpDisplayShell *shell,
                         cairo_t          *cr)
{
  gdouble x1, y1, x2, y2, x3, y3, x4, y4;

  gimp_canvas_corner_transform (item, shell, &x1, &y1, &x2, &y2, &x3, &y3, &x4, &y4);

  cairo_move_to (cr, x1, y1);
  cairo_line_to (cr, x2, y2);
  cairo_line_to (cr, x4, y4);
  cairo_line_to (cr, x3, y3);
  cairo_close_path(cr);
  _gimp_canvas_item_stroke (item, cr);
}

static cairo_region_t *
gimp_canvas_corner_get_extents (GimpCanvasItem   *item,
                                GimpDisplayShell *shell)
{
  cairo_rectangle_int_t rectangle;
  gdouble x1, y1, x2, y2, x3, y3, x4, y4;

  gimp_canvas_corner_transform (item, shell, &x1, &y1, &x2, &y2, &x3, &y3, &x4, &y4);

  rectangle.x      = floor (MIN(MIN(MIN(x1, x2),x3),x4) - 1.5);
  rectangle.y      = floor (MIN(MIN(MIN(y1, y2),y3),y4) - 1.5);
  rectangle.width  = ceil (MAX(MAX(MAX(x1, x2),x3),x4) - rectangle.x + 1.5);
  rectangle.height = ceil (MAX(MAX(MAX(y1, y2),y3),y4) - rectangle.y + 1.5);

  return cairo_region_create_rectangle (&rectangle);
}

GimpCanvasItem *
gimp_canvas_corner_new (GimpDisplayShell *shell,
                        gdouble           x,
                        gdouble           y,
                        gdouble           width,
                        gdouble           height,
                        GimpHandleAnchor  anchor,
                        gint              corner_width,
                        gint              corner_height,
                        gboolean          outside)
{
  g_return_val_if_fail (GIMP_IS_DISPLAY_SHELL (shell), NULL);

  return g_object_new (GIMP_TYPE_CANVAS_CORNER,
                       "shell",         shell,
                       "x",             x,
                       "y",             y,
                       "width",         width,
                       "height",        height,
                       "anchor",        anchor,
                       "corner-width",  corner_width,
                       "corner-height", corner_height,
                       "outside",       outside,
                       NULL);
}
