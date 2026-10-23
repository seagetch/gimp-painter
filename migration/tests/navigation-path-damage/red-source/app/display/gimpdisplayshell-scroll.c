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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <stdlib.h>
#include <math.h>

#include <gegl.h>
#include <gtk/gtk.h>

#include "libgimpmath/gimpmath.h"

#include "display-types.h"

#include "config/gimpdisplayconfig.h"

#include "core/gimpimage.h"

#include "gimpcanvas.h"
#include "gimpdisplay.h"
#include "gimpdisplay-foreach.h"
#include "gimpdisplayshell.h"
#include "gimpdisplayshell-expose.h"
#include "gimpdisplayshell-render.h"
#include "gimpdisplayshell-rotate.h"
#include "gimpdisplayshell-rulers.h"
#include "gimpdisplayshell-scale.h"
#include "gimpdisplayshell-scroll.h"
#include "gimpdisplayshell-scrollbars.h"
#include "gimpdisplayshell-transform.h"


#define OVERPAN_FACTOR 0.5


/* Match the old viewport-centered H*Rotate operation order, including its
 * normalized old angle. Algebraically equivalent native R*H/V matrices do not
 * preserve low bits observed by the old inverse-before-gint conversion. */
static void
painter_pan_matrix (GimpDisplayShell *shell, cairo_matrix_t *matrix)
{
  const gdouble cx = shell->disp_width / 2;
  const gdouble cy = shell->disp_height / 2;
  const gboolean mirrored = shell->flip_horizontally != shell->flip_vertically;
  gdouble angle = (mirrored ? -1.0 : 1.0) *
                  (shell->rotate_angle + (shell->flip_vertically ? 180.0 : 0.0));
  angle = fmod (angle, 360.0);
  if (angle < 0.0) angle += 360.0;
  cairo_matrix_init_translate (matrix, cx, cy);
  if (mirrored) cairo_matrix_scale (matrix, -1.0, 1.0);
  cairo_matrix_rotate (matrix, angle / 180.0 * G_PI);
  cairo_matrix_translate (matrix, -cx, -cy);
}

/* Preserve the exact old double origin while a native view is unchanged.
 * Re-inverting a rendered matrix loses low bits that old gint focus truncation
 * can observe. This is per-shell provenance, not an epsilon snapping policy. */
static void
painter_origin_key (GimpDisplayShell *shell, gdouble key[20])
{
  GimpImage *image = gimp_display_get_image (shell->display);
  gdouble factor = 0.0, xres = 0.0, yres = 0.0;
  if (shell->zoom) g_object_get (shell->zoom, "value", &factor, NULL);
  if (image) gimp_image_get_resolution (image, &xres, &yres);
  const gdouble current[] = {
    shell->offset_x, shell->offset_y,
    shell->painter_pan_fraction_x, shell->painter_pan_fraction_y,
    shell->scale_x, shell->scale_y, shell->rotate_angle,
    shell->disp_width, shell->disp_height,
    shell->flip_horizontally, shell->flip_vertically,
    image ? gimp_image_get_width (image) : 0,
    image ? gimp_image_get_height (image) : 0,
    factor, shell->dot_for_dot, shell->rotate_angle_exact,
    xres, yres, shell->monitor_xres, shell->monitor_yres
  };
  memcpy (key, current, sizeof (current));
}

void
gimp_display_shell_painter_origin_remember (GimpDisplayShell *shell,
                                            gdouble x, gdouble y)
{
  g_return_if_fail (GIMP_IS_DISPLAY_SHELL (shell));
  shell->painter_origin_valid = isfinite (x) && isfinite (y);
  shell->painter_origin_x = x;
  shell->painter_origin_y = y;
  painter_origin_key (shell, shell->painter_origin_key);
  shell->painter_origin_image = gimp_display_get_image (shell->display);
}

gboolean
gimp_display_shell_painter_unrotate (GimpDisplayShell *shell,
                                     gdouble *x, gdouble *y)
{
  cairo_surface_t *surface;
  cairo_t *cr;
  gboolean success;
  gboolean mirrored;
  gdouble angle;
  g_return_val_if_fail (GIMP_IS_DISPLAY_SHELL (shell), FALSE);
  g_return_val_if_fail (x != NULL && y != NULL, FALSE);
  if (! isfinite (*x) || ! isfinite (*y) || ! isfinite (shell->rotate_angle))
    return FALSE;
  mirrored = shell->flip_horizontally != shell->flip_vertically;
  angle = (mirrored ? -1.0 : 1.0) *
          (shell->rotate_angle + (shell->flip_vertically ? 180.0 : 0.0));
  angle = fmod (angle, 360.0);
  if (angle < 0.0) angle += 360.0;
  if (angle == 0.0 && ! mirrored) return TRUE;
  /* cairo_device_to_user observes the context's incrementally maintained
   * inverse. Inverting its final CTM is NOT bit-equivalent near old gint
   * boundaries (e.g. 270 degrees). Reproduce the old context operation order
   * without acquiring a window draw context or rendering any pixels. */
  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 1, 1);
  cr = cairo_create (surface);
  cairo_surface_destroy (surface);
  cairo_translate (cr, shell->disp_width / 2, shell->disp_height / 2);
  if (mirrored) cairo_scale (cr, -1.0, 1.0);
  cairo_rotate (cr, angle / 180.0 * G_PI);
  cairo_translate (cr, -shell->disp_width / 2, -shell->disp_height / 2);
  cairo_device_to_user (cr, x, y);
  success = cairo_status (cr) == CAIRO_STATUS_SUCCESS && isfinite (*x) && isfinite (*y);
  cairo_destroy (cr);
  return success;
}

gboolean
gimp_display_shell_painter_origin_get (GimpDisplayShell *shell,
                                       gdouble *x, gdouble *y)
{
  gdouble key[20];
  g_return_val_if_fail (GIMP_IS_DISPLAY_SHELL (shell), FALSE);
  g_return_val_if_fail (x != NULL && y != NULL, FALSE);
  if (! gimp_display_get_image (shell->display)) return FALSE;
  painter_origin_key (shell, key);
  for (guint n = 0; n < G_N_ELEMENTS (key); n++)
    if (! isfinite (key[n]))
      {
        shell->painter_origin_valid = FALSE;
        return FALSE;
      }
  if (shell->painter_origin_valid &&
      shell->painter_origin_image == gimp_display_get_image (shell->display) &&
      memcmp (key, shell->painter_origin_key, sizeof (key)) == 0)
    {
      *x = shell->painter_origin_x;
      *y = shell->painter_origin_y;
      return TRUE;
    }
  shell->painter_origin_valid = FALSE;
  gimp_display_shell_transform_xy_f (shell, 0, 0, x, y);
  if (! gimp_display_shell_painter_unrotate (shell, x, y))
    return FALSE;
  *x = -*x; *y = -*y;
  return TRUE;
}


gboolean
gimp_display_shell_painter_pan_begin (GimpDisplayShell *shell, gdouble x, gdouble y)
{
  gdouble zero_x, zero_y;
  g_return_val_if_fail (GIMP_IS_DISPLAY_SHELL (shell), FALSE);
  if (! isfinite (x) || ! isfinite (y))
    return FALSE;
  if (! gimp_display_shell_painter_unrotate (shell, &x, &y))
    return FALSE;
  if (! gimp_display_shell_painter_origin_get (shell, &zero_x, &zero_y))
    return FALSE;
  zero_x = -zero_x; zero_y = -zero_y;
  if (! isfinite (x) || ! isfinite (y) ||
      ! isfinite (zero_x) || ! isfinite (zero_y) ||
      x < G_MININT || x > G_MAXINT || y < G_MININT || y > G_MAXINT)
    return FALSE;
  /* Initial old offset is minus inverse(native visible image origin).
   * The historical function narrowed only these transformed press arguments. */
  shell->painter_pan_start_x = (gint) x - zero_x;
  shell->painter_pan_start_y = (gint) y - zero_y;
  return TRUE;
}

static gdouble
painter_pan_clamp (gdouble offset, gint viewport, gint image_extent)
{
  gint minimum, maximum;
  if (viewport < image_extent)
    {
      minimum = -viewport * OVERPAN_FACTOR;
      maximum = image_extent - viewport * (1.0 - OVERPAN_FACTOR);
    }
  else
    {
      gint overpan = viewport - image_extent * (1.0 - OVERPAN_FACTOR);
      minimum = -overpan;
      maximum = image_extent + overpan - viewport;
    }
  return CLAMP (offset, minimum, maximum);
}

void
gimp_display_shell_painter_pan_motion (GimpDisplayShell *shell, gdouble x, gdouble y)
{
  cairo_matrix_t matrix;
  GimpImage *image;
  gint width, height, ix, iy;
  gdouble old_x, old_y, native_zero_x, native_zero_y, cx, cy;
  g_return_if_fail (GIMP_IS_DISPLAY_SHELL (shell));
  g_return_if_fail (isfinite (x) && isfinite (y));
  image = gimp_display_get_image (shell->display);
  if (! image) return;
  painter_pan_matrix (shell, &matrix);
  if (! gimp_display_shell_painter_unrotate (shell, &x, &y)) return;
  /* Exact old pre-rotation axes and rounded scaled extent; do not substitute
   * today's rotated bounding-box clamp. Old offsets remain double. */
  old_x = painter_pan_clamp (shell->painter_pan_start_x - x, shell->disp_width,
                             SCALEX (shell, gimp_image_get_width (image)));
  old_y = painter_pan_clamp (shell->painter_pan_start_y - y, shell->disp_height,
                             SCALEY (shell, gimp_image_get_height (image)));
  x = -old_x; y = -old_y;
  cairo_matrix_transform_point (&matrix, &x, &y);
  /* Convert the same visible old image origin to the native image-centered
   * offset representation, without rounding the resulting device origin. */
  gimp_display_shell_scale_get_image_size (shell, &width, &height);
  cx = width / 2; cy = height / 2;
  native_zero_x = -cx; native_zero_y = -cy;
  cairo_matrix_transform_distance (&matrix, &native_zero_x, &native_zero_y);
  x = cx + native_zero_x - x; y = cy + native_zero_y - y;
  g_return_if_fail (isfinite (x) && isfinite (y) &&
                    x > G_MININT && x < G_MAXINT && y > G_MININT && y < G_MAXINT);
  ix = floor (x); iy = floor (y);
  if (shell->offset_x == ix && shell->offset_y == iy &&
      shell->painter_pan_fraction_x == x - ix && shell->painter_pan_fraction_y == y - iy)
    {
      gimp_display_shell_painter_origin_remember (shell, old_x, old_y);
      return;
    }
  gimp_display_shell_pause (shell);
  shell->offset_x = ix; shell->offset_y = iy;
  shell->painter_pan_fraction_x = x - ix; shell->painter_pan_fraction_y = y - iy;
  gimp_display_shell_rotate_update_transform (shell);
  gimp_display_shell_scrollbars_update (shell);
  gimp_display_shell_rulers_update (shell);
  /* Full invalidation is essential: integer render-cache/GTK scrolling would
   * lose the residual and leave raster/selection/overlay damage inconsistent. */
  gimp_display_shell_scrolled (shell);
  gimp_display_shell_expose_full (shell);
  gimp_display_shell_render_invalidate_full (shell);
  gimp_display_shell_painter_origin_remember (shell, old_x, old_y);
  gimp_display_shell_resume (shell);
}

/**
 * gimp_display_shell_scroll:
 * @shell:
 * @x_offset:
 * @y_offset:
 *
 * This function scrolls the image in the shell's viewport. It does
 * actual scrolling of the pixels, so only the newly scrolled-in parts
 * are freshly redrawn.
 *
 * Use it for incremental actual panning.
 **/
void
gimp_display_shell_scroll (GimpDisplayShell *shell,
                           gint              x_offset,
                           gint              y_offset)
{
  gint old_x;
  gint old_y;
  gdouble old_fraction_x, old_fraction_y;

  g_return_if_fail (GIMP_IS_DISPLAY_SHELL (shell));
  shell->painter_origin_valid = FALSE;

  if (x_offset == 0 && y_offset == 0)
    return;

  old_fraction_x = shell->painter_pan_fraction_x;
  old_fraction_y = shell->painter_pan_fraction_y;
  old_x = shell->offset_x;
  old_y = shell->offset_y;

  /* freeze the active tool */
  gimp_display_shell_pause (shell);

  shell->offset_x += x_offset;
  shell->offset_y += y_offset;

  gimp_display_shell_scroll_clamp_and_update (shell);

  /*  the actual changes in offset  */
  x_offset = (shell->offset_x - old_x);
  y_offset = (shell->offset_y - old_y);

  if (old_fraction_x != shell->painter_pan_fraction_x ||
      old_fraction_y != shell->painter_pan_fraction_y)
    {
      /* A fractional origin clipped onto an integer boundary cannot reuse
       * an integer-shifted raster cache or child allocation. */
      gimp_display_shell_scrolled (shell);
      gimp_display_shell_expose_full (shell);
      gimp_display_shell_render_invalidate_full (shell);
    }
  else if (x_offset || y_offset)
    {
      gimp_display_shell_scrolled (shell);

      gimp_overlay_box_scroll (GIMP_OVERLAY_BOX (shell->canvas),
                               -x_offset, -y_offset);

      if (shell->render_cache)
        {
          cairo_surface_t       *surface;
          cairo_t               *cr;

          surface = cairo_surface_create_similar_image (
            shell->render_cache,
            CAIRO_FORMAT_ARGB32,
            shell->disp_width  * shell->render_scale,
            shell->disp_height * shell->render_scale);

          cr = cairo_create (surface);
          cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
          cairo_set_source_surface (cr, shell->render_cache, 0, 0);
          cairo_paint (cr);
          cairo_destroy (cr);

          cr = cairo_create (shell->render_cache);
          cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
          cairo_set_source_surface (cr, surface,
                                    -x_offset * shell->render_scale,
                                    -y_offset * shell->render_scale);
          cairo_paint (cr);
          cairo_destroy (cr);

          cairo_surface_destroy (surface);
        }

      if (shell->render_cache_valid)
        {
          cairo_rectangle_int_t rect;

          cairo_region_translate (shell->render_cache_valid,
                                  -x_offset, -y_offset);

          rect.x      = 0;
          rect.y      = 0;
          rect.width  = shell->disp_width;
          rect.height = shell->disp_height;

          cairo_region_intersect_rectangle (shell->render_cache_valid, &rect);
        }
    }

  /* re-enable the active tool */
  gimp_display_shell_resume (shell);
}

/**
 * gimp_display_shell_scroll_set_offsets:
 * @shell:
 * @offset_x:
 * @offset_y:
 *
 * This function scrolls the image in the shell's viewport. It redraws
 * the entire canvas.
 *
 * Use it for setting the scroll offset on freshly scaled images or
 * when the window is resized. For panning, use
 * gimp_display_shell_scroll().
 **/
void
gimp_display_shell_scroll_set_offset (GimpDisplayShell *shell,
                                      gint              offset_x,
                                      gint              offset_y)
{
  g_return_if_fail (GIMP_IS_DISPLAY_SHELL (shell));
  shell->painter_origin_valid = FALSE;

  if (shell->offset_x == offset_x &&
      shell->offset_y == offset_y &&
      shell->painter_pan_fraction_x == 0.0 &&
      shell->painter_pan_fraction_y == 0.0)
    return;

  gimp_display_shell_scale_save_revert_values (shell);

  /* freeze the active tool */
  gimp_display_shell_pause (shell);

  shell->offset_x = offset_x;
  shell->offset_y = offset_y;
  shell->painter_pan_fraction_x = 0.0;
  shell->painter_pan_fraction_y = 0.0;

  gimp_display_shell_scroll_clamp_and_update (shell);

  gimp_display_shell_scrolled (shell);

  gimp_display_shell_expose_full (shell);
  gimp_display_shell_render_invalidate_full (shell);

  /* re-enable the active tool */
  gimp_display_shell_resume (shell);
}

/**
 * gimp_display_shell_scroll_clamp_and_update:
 * @shell:
 *
 * Helper function for calling two functions that are commonly called
 * in pairs.
 **/
void
gimp_display_shell_scroll_clamp_and_update (GimpDisplayShell *shell)
{
  GimpImage *image;

  g_return_if_fail (GIMP_IS_DISPLAY_SHELL (shell));

  image = gimp_display_get_image (shell->display);

  if (image)
    {
      if (! shell->show_all)
        {
          gint bounds_x;
          gint bounds_y;
          gint bounds_width;
          gint bounds_height;
          gint min_offset_x;
          gint max_offset_x;
          gint min_offset_y;
          gint max_offset_y;
          gint offset_x;
          gint offset_y;

          gimp_display_shell_rotate_update_transform (shell);

          gimp_display_shell_scale_get_image_bounds (shell,
                                                     &bounds_x,
                                                     &bounds_y,
                                                     &bounds_width,
                                                     &bounds_height);

          if (shell->disp_width < bounds_width)
            {
              min_offset_x = bounds_x -
                             shell->disp_width * OVERPAN_FACTOR;
              max_offset_x = bounds_x + bounds_width -
                             shell->disp_width * (1.0 - OVERPAN_FACTOR);
            }
          else
            {
              gint overpan_amount;

              overpan_amount = shell->disp_width -
                               bounds_width * (1.0 - OVERPAN_FACTOR);

              min_offset_x = bounds_x -
                             overpan_amount;
              max_offset_x = bounds_x + bounds_width - shell->disp_width +
                             overpan_amount;
            }

          if (shell->disp_height < bounds_height)
            {
              min_offset_y = bounds_y -
                             shell->disp_height * OVERPAN_FACTOR;
              max_offset_y = bounds_y + bounds_height -
                             shell->disp_height * (1.0 - OVERPAN_FACTOR);
            }
          else
            {
              gint overpan_amount;

              overpan_amount = shell->disp_height -
                               bounds_height * (1.0 - OVERPAN_FACTOR);

              min_offset_y = bounds_y -
                             overpan_amount;
              max_offset_y = bounds_y + bounds_height +
                             overpan_amount - shell->disp_height;
            }

          /* Clamp */

          offset_x = CLAMP (shell->offset_x, min_offset_x, max_offset_x);
          offset_y = CLAMP (shell->offset_y, min_offset_y, max_offset_y);

          if (offset_x != shell->offset_x || offset_y != shell->offset_y)
            {
              /* A clipped native axis lands exactly on its integer bound. */
              if (shell->offset_x != offset_x)
                shell->painter_pan_fraction_x = 0.0;
              if (shell->offset_y != offset_y)
                shell->painter_pan_fraction_y = 0.0;
              shell->offset_x = offset_x;
              shell->offset_y = offset_y;

              gimp_display_shell_rotate_update_transform (shell);
            }

          /* Set scrollbar stepper sensitiity */

          gimp_display_shell_scrollbars_update_steppers (shell,
                                                         min_offset_x,
                                                         max_offset_x,
                                                         min_offset_y,
                                                         max_offset_y);
        }
      else
        {
          /* Set scrollbar stepper sensitiity */

          gimp_display_shell_scrollbars_update_steppers (shell,
                                                         G_MININT,
                                                         G_MAXINT,
                                                         G_MININT,
                                                         G_MAXINT);
        }
    }
  else
    {
      shell->offset_x = 0;
      shell->offset_y = 0;
      shell->painter_pan_fraction_x = 0.0;
      shell->painter_pan_fraction_y = 0.0;
    }

  gimp_display_shell_scrollbars_update (shell);
  gimp_display_shell_rulers_update (shell);
}

/**
 * gimp_display_shell_scroll_unoverscrollify:
 * @shell:
 * @in_offset_x:
 * @in_offset_y:
 * @out_offset_x:
 * @out_offset_y:
 *
 * Takes a scroll offset and returns the offset that will not result
 * in a scroll beyond the image border. If the image is already
 * overscrolled, the return value is 0 for that given axis.
 **/
void
gimp_display_shell_scroll_unoverscrollify (GimpDisplayShell *shell,
                                           gint              in_offset_x,
                                           gint              in_offset_y,
                                           gint             *out_offset_x,
                                           gint             *out_offset_y)
{
  gint sw, sh;
  gint out_offset_x_dummy, out_offset_y_dummy;

  g_return_if_fail (GIMP_IS_DISPLAY_SHELL (shell));

  if (! out_offset_x) out_offset_x = &out_offset_x_dummy;
  if (! out_offset_y) out_offset_y = &out_offset_y_dummy;

  *out_offset_x = in_offset_x;
  *out_offset_y = in_offset_y;

  if (! shell->show_all)
    {
      gimp_display_shell_scale_get_image_size (shell, &sw, &sh);

      if (in_offset_x < 0)
        {
          *out_offset_x = MAX (in_offset_x,
                               MIN (0, 0 - shell->offset_x));
        }
      else if (in_offset_x > 0)
        {
          gint min_offset = sw - shell->disp_width;

          *out_offset_x = MIN (in_offset_x,
                               MAX (0, min_offset - shell->offset_x));
        }

      if (in_offset_y < 0)
        {
          *out_offset_y = MAX (in_offset_y,
                               MIN (0, 0 - shell->offset_y));
        }
      else if (in_offset_y > 0)
        {
          gint min_offset = sh - shell->disp_height;

          *out_offset_y = MIN (in_offset_y,
                               MAX (0, min_offset - shell->offset_y));
        }
    }
}

/**
 * gimp_display_shell_scroll_center_image_xy:
 * @shell:
 * @image_x:
 * @image_y:
 *
 * Center the viewport around the passed image coordinate
 **/
void
gimp_display_shell_scroll_center_image_xy (GimpDisplayShell *shell,
                                           gdouble           image_x,
                                           gdouble           image_y)
{
  gint viewport_x;
  gint viewport_y;

  gimp_display_shell_transform_xy (shell,
                                   image_x, image_y,
                                   &viewport_x, &viewport_y);

  gimp_display_shell_scroll (shell,
                             viewport_x - shell->disp_width  / 2,
                             viewport_y - shell->disp_height / 2);
}

/**
 * gimp_display_shell_scroll_center_image:
 * @shell:
 * @horizontally:
 * @vertically:
 *
 * Centers the image in the display shell on the desired axes.
 **/
void
gimp_display_shell_scroll_center_image (GimpDisplayShell *shell,
                                        gboolean          horizontally,
                                        gboolean          vertically)
{
  gint image_x;
  gint image_y;
  gint image_width;
  gint image_height;
  gint center_x;
  gint center_y;
  gint offset_x = 0;
  gint offset_y = 0;

  g_return_if_fail (GIMP_IS_DISPLAY_SHELL (shell));

  if (! shell->display                          ||
      ! gimp_display_get_image (shell->display) ||
      (! vertically && ! horizontally))
    return;

  gimp_display_shell_scale_get_image_bounds (shell,
                                             &image_x, &image_y,
                                             &image_width, &image_height);

  if (shell->disp_width > image_width)
    {
      image_x     -= (shell->disp_width - image_width) / 2;
      image_width  = shell->disp_width;
    }

  if (shell->disp_height > image_height)
    {
      image_y      -= (shell->disp_height - image_height) / 2;
      image_height  = shell->disp_height;
    }

  center_x = image_x + image_width  / 2;
  center_y = image_y + image_height / 2;

  if (horizontally)
    offset_x = center_x - shell->disp_width / 2 - shell->offset_x;

  if (vertically)
    offset_y = center_y - shell->disp_height / 2 - shell->offset_y;

  gimp_display_shell_scroll (shell, offset_x, offset_y);
}

/**
 * gimp_display_shell_scroll_center_image:
 * @shell:
 * @horizontally:
 * @vertically:
 *
 * Centers the image content in the display shell on the desired axes.
 **/
void
gimp_display_shell_scroll_center_content (GimpDisplayShell *shell,
                                          gboolean          horizontally,
                                          gboolean          vertically)
{
  gint content_x;
  gint content_y;
  gint content_width;
  gint content_height;
  gint center_x;
  gint center_y;
  gint offset_x = 0;
  gint offset_y = 0;

  g_return_if_fail (GIMP_IS_DISPLAY_SHELL (shell));

  if (! shell->display                          ||
      ! gimp_display_get_image (shell->display) ||
      (! vertically && ! horizontally))
    return;

  if (! gimp_display_shell_get_infinite_canvas (shell))
    {
      gimp_display_shell_scale_get_image_bounds (shell,
                                                 &content_x,
                                                 &content_y,
                                                 &content_width,
                                                 &content_height);
    }
  else
    {
      gimp_display_shell_scale_get_image_bounding_box (shell,
                                                       &content_x,
                                                       &content_y,
                                                       &content_width,
                                                       &content_height);
    }

  if (shell->disp_width > content_width)
    {
      content_x     -= (shell->disp_width - content_width) / 2;
      content_width  = shell->disp_width;
    }

  if (shell->disp_height > content_height)
    {
      content_y      -= (shell->disp_height - content_height) / 2;
      content_height  = shell->disp_height;
    }

  center_x = content_x + content_width  / 2;
  center_y = content_y + content_height / 2;

  if (horizontally)
    offset_x = center_x - shell->disp_width / 2 - shell->offset_x;

  if (vertically)
    offset_y = center_y - shell->disp_height / 2 - shell->offset_y;

  gimp_display_shell_scroll (shell, offset_x, offset_y);
}

/**
 * gimp_display_shell_scroll_get_scaled_viewport:
 * @shell:
 * @x:
 * @y:
 * @w:
 * @h:
 *
 * Gets the viewport in screen coordinates, with origin at (0, 0) in
 * the image.
 **/
void
gimp_display_shell_scroll_get_scaled_viewport (GimpDisplayShell *shell,
                                               gint             *x,
                                               gint             *y,
                                               gint             *w,
                                               gint             *h)
{
  g_return_if_fail (GIMP_IS_DISPLAY_SHELL (shell));

  *x = shell->offset_x;
  *y = shell->offset_y;
  *w = shell->disp_width;
  *h = shell->disp_height;
}

/**
 * gimp_display_shell_scroll_get_viewport:
 * @shell:
 * @x:
 * @y:
 * @w:
 * @h:
 *
 * Gets the viewport in image coordinates.
 **/
void
gimp_display_shell_scroll_get_viewport (GimpDisplayShell *shell,
                                        gdouble          *x,
                                        gdouble          *y,
                                        gdouble          *w,
                                        gdouble          *h)
{
  g_return_if_fail (GIMP_IS_DISPLAY_SHELL (shell));

  *x = shell->offset_x    / shell->scale_x;
  *y = shell->offset_y    / shell->scale_y;
  *w = shell->disp_width  / shell->scale_x;
  *h = shell->disp_height / shell->scale_y;
}
