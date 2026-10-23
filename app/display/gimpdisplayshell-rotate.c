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

#include <math.h>

#include <gegl.h>
#include <gtk/gtk.h>

#include "libgimpmath/gimpmath.h"

#include "display-types.h"

#include "gimpdisplay.h"
#include "gimpdisplayshell.h"
#include "gimpdisplayshell-expose.h"
#include "gimpdisplayshell-render.h"
#include "gimpdisplayshell-rotate.h"
#include "gimppainternavigation.h"
#include "gimpdisplayshell-scale.h"
#include "gimpdisplayshell-scroll.h"
#include "gimpdisplayshell-scrollbars.h"
#include "gimpdisplayshell-rulers.h"
#include "gimpdisplayshell-transform.h"


#define ANGLE_EPSILON 1e-3


/*  local function prototypes  */

static void   gimp_display_shell_rotate_to_internal      (GimpDisplayShell *shell,
                                                          gdouble           value,
                                                          gboolean          exact);
static void   gimp_display_shell_save_viewport_center    (GimpDisplayShell *shell,
                                                          gdouble          *x,
                                                          gdouble          *y);
static void   gimp_display_shell_restore_viewport_center (GimpDisplayShell *shell,
                                                          gdouble           x,
                                                          gdouble           y);


/*  public functions  */

void
gimp_display_shell_flip (GimpDisplayShell *shell,
                         gboolean          flip_horizontally,
                         gboolean          flip_vertically)
{
  g_return_if_fail (GIMP_IS_DISPLAY_SHELL (shell));
  shell->painter_origin_valid = FALSE;

  flip_horizontally = flip_horizontally ? TRUE : FALSE;
  flip_vertically   = flip_vertically   ? TRUE : FALSE;

  if (flip_horizontally != shell->flip_horizontally ||
      flip_vertically   != shell->flip_vertically)
    {
      gdouble cx, cy;

      /* Maintain the current center of the viewport. */
      gimp_display_shell_save_viewport_center (shell, &cx, &cy);

      /* freeze the active tool */
      gimp_display_shell_pause (shell);

      /* Adjust the rotation angle so that the image gets reflected across the
       * horizontal, and/or vertical, axes in screen space, regardless of the
       * current rotation.  Keep its exact-angle provenance when reflecting it.
       */
      if (flip_horizontally == shell->flip_horizontally ||
          flip_vertically   == shell->flip_vertically)
        {
          if (shell->rotate_angle != 0.0)
            shell->rotate_angle = 360.0 - shell->rotate_angle;
        }

      shell->flip_horizontally = flip_horizontally;
      shell->flip_vertically   = flip_vertically;

      gimp_display_shell_rotated (shell);

      gimp_display_shell_restore_viewport_center (shell, cx, cy);

      gimp_display_shell_expose_full (shell);
      gimp_display_shell_render_invalidate_full (shell);

      /* re-enable the active tool */
      gimp_display_shell_resume (shell);
    }
}

void
gimp_display_shell_rotate (GimpDisplayShell *shell,
                           gdouble           delta)
{
  g_return_if_fail (GIMP_IS_DISPLAY_SHELL (shell));

  gimp_display_shell_rotate_to (shell, shell->rotate_angle + delta);
}

void
gimp_display_shell_rotate_to (GimpDisplayShell *shell,
                              gdouble           value)
{
  g_return_if_fail (GIMP_IS_DISPLAY_SHELL (shell));
  shell->painter_origin_valid = FALSE;
  gimp_display_shell_rotate_to_internal (shell, value, FALSE);
}

void
gimp_display_shell_rotate_drag (GimpDisplayShell *shell,
                                gdouble           last_x,
                                gdouble           last_y,
                                gdouble           cur_x,
                                gdouble           cur_y,
                                gboolean          constrain)
{
  g_return_if_fail (GIMP_IS_DISPLAY_SHELL (shell));
  shell->painter_origin_valid = FALSE;

  /* Start-relative arithmetic is independent of skipped/compressed motion and
   * keeps the unsnapped reference when Ctrl is pressed or released mid-drag. */
  gimp_display_shell_rotate_to_internal (shell,
    gimp_painter_navigation_rotate (shell->disp_width, shell->disp_height,
                                    cur_x, cur_y, shell->rotate_drag_angle,
                                    shell->flip_horizontally, shell->flip_vertically,
                                    constrain ? GDK_CONTROL_MASK : 0),
    shell->painter_navigation_inherited);
}

void
gimp_display_shell_rotate_update_transform (GimpDisplayShell *shell)
{
  g_return_if_fail (GIMP_IS_DISPLAY_SHELL (shell));

  g_clear_pointer (&shell->rotate_transform,   g_free);
  g_clear_pointer (&shell->rotate_untransform, g_free);

  /* Inherited painter angles have no zero epsilon in the old display path.
   * This belongs to the displayed angle, not the lifetime of its input grab:
   * scrolling, scaling and release must not collapse a later rebuild. */
  if (! shell->rotate_angle_exact &&
      (fabs (shell->rotate_angle)         < ANGLE_EPSILON ||
       fabs (360.0 - shell->rotate_angle) < ANGLE_EPSILON))
    shell->rotate_angle = 0.0;

  if ((shell->rotate_angle != 0.0 ||
       shell->flip_horizontally   ||
       shell->flip_vertically ||
       shell->painter_pan_fraction_x != 0.0 ||
       shell->painter_pan_fraction_y != 0.0) &&
      gimp_display_get_image (shell->display))
    {
      gint    image_width, image_height;
      gdouble cx, cy;

      shell->rotate_transform   = g_new (cairo_matrix_t, 1);
      shell->rotate_untransform = g_new (cairo_matrix_t, 1);

      gimp_display_shell_scale_get_image_size (shell,
                                               &image_width, &image_height);

      cx = -shell->offset_x + image_width  / 2;
      cy = -shell->offset_y + image_height / 2;

      cairo_matrix_init_translate (shell->rotate_transform, cx, cy);

      if (shell->rotate_angle != 0.0)
        cairo_matrix_rotate (shell->rotate_transform,
                             shell->rotate_angle / 180.0 * G_PI);

      if (shell->flip_horizontally)
        cairo_matrix_scale (shell->rotate_transform, -1.0, 1.0);

      if (shell->flip_vertically)
        cairo_matrix_scale (shell->rotate_transform, 1.0, -1.0);

      cairo_matrix_translate (shell->rotate_transform, -cx, -cy);

      /* Device translation is appended after image-centered rotation. All
       * raster, item, selection, pointer and upright-anchor routes share it. */
      shell->rotate_transform->x0 -= shell->painter_pan_fraction_x;
      shell->rotate_transform->y0 -= shell->painter_pan_fraction_y;
      *shell->rotate_untransform = *shell->rotate_transform;
      cairo_matrix_invert (shell->rotate_untransform);
    }
}


/*  private functions  */

static void
gimp_display_shell_rotate_to_internal (GimpDisplayShell *shell,
                                       gdouble           value,
                                       gboolean          exact)
{
  gdouble cx, cy;
  gdouble precise_x = 0.0, precise_y = 0.0;

  g_return_if_fail (GIMP_IS_DISPLAY_SHELL (shell));

  /* Maintain the current center of the viewport. */
  gimp_display_shell_save_viewport_center (shell, &cx, &cy);

  /* Make sure the angle is within the range [0, 360). */
  value = fmod (value, 360.0);
  if (value < 0.0)
    value += 360.0;

  if (exact)
    {
      cairo_matrix_t linear;
      gint width, height;

      /* Old rotation changes a viewport-centered matrix without changing its
       * double origin or clamping. Preserve that same viewport-center image
       * point, not the rounded native set-offset approximation. */
      gimp_display_shell_scale_get_image_size (shell, &width, &height);
      precise_x = shell->disp_width / 2 + shell->painter_rotate_origin_x - width / 2;
      precise_y = shell->disp_height / 2 + shell->painter_rotate_origin_y - height / 2;
      cairo_matrix_init_rotate (&linear, value / 180.0 * G_PI);
      cairo_matrix_scale (&linear, shell->flip_horizontally ? -1.0 : 1.0,
                                   shell->flip_vertically ? -1.0 : 1.0);
      cairo_matrix_transform_distance (&linear, &precise_x, &precise_y);
      precise_x += width / 2 - shell->disp_width / 2;
      precise_y += height / 2 - shell->disp_height / 2;
      if (! isfinite (precise_x) || ! isfinite (precise_y) ||
          precise_x <= G_MININT || precise_x >= G_MAXINT ||
          precise_y <= G_MININT || precise_y >= G_MAXINT)
        return;
    }

  shell->rotate_angle       = value;
  shell->rotate_angle_exact = exact;

  /* freeze the active tool */
  gimp_display_shell_pause (shell);

  if (exact)
    {
      shell->offset_x = floor (precise_x);
      shell->offset_y = floor (precise_y);
      shell->painter_pan_fraction_x = precise_x - shell->offset_x;
      shell->painter_pan_fraction_y = precise_y - shell->offset_y;
      gimp_display_shell_rotated (shell);
      gimp_display_shell_scrollbars_update (shell);
      gimp_display_shell_rulers_update (shell);
      gimp_display_shell_painter_origin_remember (shell,
        shell->painter_rotate_origin_x, shell->painter_rotate_origin_y);
    }
  else
    {
      gimp_display_shell_scroll_clamp_and_update (shell);
      gimp_display_shell_rotated (shell);
      gimp_display_shell_restore_viewport_center (shell, cx, cy);
    }

  gimp_display_shell_expose_full (shell);
  gimp_display_shell_render_invalidate_full (shell);

  /* re-enable the active tool */
  gimp_display_shell_resume (shell);
}

static void
gimp_display_shell_save_viewport_center (GimpDisplayShell *shell,
                                         gdouble          *x,
                                         gdouble          *y)
{
  gimp_display_shell_unrotate_xy_f (shell,
                                    shell->disp_width  / 2,
                                    shell->disp_height / 2,
                                    x, y);
}

static void
gimp_display_shell_restore_viewport_center (GimpDisplayShell *shell,
                                            gdouble           x,
                                            gdouble           y)
{
  gimp_display_shell_rotate_xy_f (shell, x, y, &x, &y);

  x += shell->offset_x - shell->disp_width  / 2;
  y += shell->offset_y - shell->disp_height / 2;

  gimp_display_shell_scroll_set_offset (shell, RINT (x), RINT (y));
}
