/* Unchanged pinned function bodies; aliases/stubs live outside this file. */
void
gimp_display_shell_scale (GimpDisplayShell *shell,
                          GimpZoomType      zoom_type,
                          gdouble           new_scale,
                          GimpZoomFocus     zoom_focus)
{
  gint    x, y;
  gdouble current_scale;
  gdouble real_new_scale;

  g_return_if_fail (GIMP_IS_DISPLAY_SHELL (shell));
  g_return_if_fail (shell->canvas != NULL);

  current_scale = gimp_zoom_model_get_factor (shell->zoom);

  if (zoom_type != GIMP_ZOOM_TO)
    {
      real_new_scale = gimp_zoom_model_zoom_step (zoom_type, current_scale);
    }
  else
    {
      real_new_scale = new_scale;
    }

  if (! SCALE_EQUALS (real_new_scale, current_scale))
    {
      if (shell->display->config->resize_windows_on_zoom)
        {
          GimpImageWindow *window = gimp_display_shell_get_window (shell);

          /* If the window is resized on zoom, simply do the zoom and
           * get things rolling
           */
          gimp_zoom_model_zoom (shell->zoom, GIMP_ZOOM_TO, real_new_scale);
          gimp_display_shell_scaled (shell);

          if (window && gimp_image_window_get_active_shell (window) == shell)
            {
              gimp_image_window_shrink_wrap (window, FALSE);
            }
        }
      else
        {
          gboolean starts_fitting_horiz;
          gboolean starts_fitting_vert;
          gboolean zoom_focus_almost_centered_horiz;
          gboolean zoom_focus_almost_centered_vert;
          gboolean image_center_almost_centered_horiz;
          gboolean image_center_almost_centered_vert;
          gint     image_center_x;
          gint     image_center_y;

          gimp_display_shell_scale_get_zoom_focus (shell,
                                                   real_new_scale,
                                                   current_scale,
                                                   &x,
                                                   &y,
                                                   zoom_focus);
          gimp_display_shell_scale_get_image_center_viewport (shell,
                                                              &image_center_x,
                                                              &image_center_y);

          gimp_display_shell_scale_to (shell, real_new_scale, x, y);


          /* If an image axis started to fit due to zooming out or if
           * the focus point is as good as in the center, center on
           * that axis
           */
          gimp_display_shell_scale_image_starts_to_fit (shell,
                                                        real_new_scale,
                                                        current_scale,
                                                        &starts_fitting_horiz,
                                                        &starts_fitting_vert);

          gimp_display_shell_scale_viewport_coord_almost_centered (shell,
                                                                   x,
                                                                   y,
                                                                   &zoom_focus_almost_centered_horiz,
                                                                   &zoom_focus_almost_centered_vert);
          gimp_display_shell_scale_viewport_coord_almost_centered (shell,
                                                                   image_center_x,
                                                                   image_center_y,
                                                                   &image_center_almost_centered_horiz,
                                                                   &image_center_almost_centered_vert);
            
          gimp_display_shell_scroll_center_image (shell,
                                                  starts_fitting_horiz ||
                                                  (zoom_focus_almost_centered_horiz &&
                                                   image_center_almost_centered_horiz),
                                                  starts_fitting_vert ||
                                                  (zoom_focus_almost_centered_vert &&
                                                   image_center_almost_centered_vert));
        }
    }
}

static void
gimp_display_shell_scale_to (GimpDisplayShell *shell,
                             gdouble           scale,
                             gint              viewport_x,
                             gint              viewport_y)
{
  gdouble scale_x, scale_y;
  gdouble image_focus_x, image_focus_y;
  gint    target_offset_x, target_offset_y;

  g_return_if_fail (GIMP_IS_DISPLAY_SHELL (shell));

  if (! shell->display)
    return;

  gimp_display_shell_untransform_xy_f (shell,
                                       viewport_x,
                                       viewport_y,
                                       &image_focus_x,
                                       &image_focus_y);

  gimp_display_shell_calculate_scale_x_and_y (shell, scale, &scale_x, &scale_y);

  target_offset_x = scale_x * image_focus_x - viewport_x;
  target_offset_y = scale_y * image_focus_y - viewport_y;

  /* Note that we never come here if we need to
   * resize_windows_on_zoom
   */
  gimp_display_shell_scale_by_values (shell,
                                      scale,
                                      target_offset_x,
                                      target_offset_y,
                                      FALSE);
}

static void
gimp_display_shell_scale_get_zoom_focus (GimpDisplayShell *shell,
                                         gdouble           new_scale,
                                         gdouble           current_scale,
                                         gint             *x,
                                         gint             *y,
                                         GimpZoomFocus     zoom_focus)
{
  GimpZoomFocus real_zoom_focus = zoom_focus;
  gint          image_center_x, image_center_y;
  gint          other_x, other_y;

  /* Calculate stops-to-fit focus point */
  gimp_display_shell_scale_get_image_center_viewport (shell,
                                                      &image_center_x,
                                                      &image_center_y);

  /* Calculate other focus point */
  {
    GdkEvent  *event;
    GtkWidget *window;
    gboolean   event_looks_sane;
    gboolean   cursor_within_canvas;
    gint       canvas_pointer_x, canvas_pointer_y;

    window = GTK_WIDGET (gimp_display_shell_get_window (shell));

    /*  Center on the mouse position instead of the display center if
     *  one of the following conditions are fulfilled and pointer is
     *  within the canvas:
     *
     *   (1) there's no current event (the action was triggered by an
     *       input controller)
     *   (2) the event originates from the canvas (a scroll event)
     *   (3) the event originates from the window (a key press event)
     *
     *  Basically the only situation where we don't want to center on
     *  mouse position is if the action is being called from a menu.
     */

    event = gtk_get_current_event ();

    event_looks_sane = (! event ||
                        gtk_get_event_widget (event) == shell->canvas ||
                        gtk_get_event_widget (event) == window);


    if (g_queue_peek_head (shell->zoom_focus_pointer_queue) == NULL)
      {
        gtk_widget_get_pointer (shell->canvas,
                                &canvas_pointer_x,
                                &canvas_pointer_y);
      }
    else
      {
        GdkPoint *point = g_queue_pop_head (shell->zoom_focus_pointer_queue);

        canvas_pointer_x = point->x;
        canvas_pointer_y = point->y;

        g_slice_free (GdkPoint, point);
      }

    cursor_within_canvas = canvas_pointer_x >= 0 &&
                           canvas_pointer_y >= 0 &&
                           canvas_pointer_x <  shell->disp_width &&
                           canvas_pointer_y <  shell->disp_height;


    if (event_looks_sane && cursor_within_canvas)
      {
        other_x = canvas_pointer_x;
        other_y = canvas_pointer_y;
      }
    else
      {
        other_x = shell->disp_width  / 2;
        other_y = shell->disp_height / 2;
      }
  }

  /* Decide which one to use for each axis */
  if (zoom_focus == GIMP_ZOOM_FOCUS_RETAIN_CENTERING_ELSE_BEST_GUESS)
    {
      gboolean centered;

      centered = gimp_display_shell_scale_viewport_coord_almost_centered (shell,
                                                                          image_center_x,
                                                                          image_center_y,
                                                                          NULL,
                                                                          NULL);
      real_zoom_focus = (centered ?
                         GIMP_ZOOM_FOCUS_IMAGE_CENTER :
                         GIMP_ZOOM_FOCUS_BEST_GUESS);
    }
  else
    {
      real_zoom_focus = zoom_focus;
    }

  switch (real_zoom_focus)
    {
    case GIMP_ZOOM_FOCUS_POINTER:
      *x = other_x;
      *y = other_y;
      break;

    case GIMP_ZOOM_FOCUS_IMAGE_CENTER:
      *x = image_center_x;
      *y = image_center_y;
      break;

    case GIMP_ZOOM_FOCUS_BEST_GUESS:
    default:
      {
        gboolean within_horizontally, within_vertically;
        gboolean stops_horizontally, stops_vertically;

        gimp_display_shell_scale_image_is_within_viewport (shell,
                                                           &within_horizontally,
                                                           &within_vertically);

        gimp_display_shell_scale_image_stops_to_fit (shell,
                                                     new_scale,
                                                     current_scale,
                                                     &stops_horizontally,
                                                     &stops_vertically);

        *x = within_horizontally && ! stops_horizontally ? image_center_x : other_x;
        *y = within_vertically   && ! stops_vertically   ? image_center_y : other_y;
      }
      break;
    }
}

static void
gimp_display_shell_scale_get_image_center_viewport (GimpDisplayShell *shell,
                                                    gint             *image_center_x,
                                                    gint             *image_center_y)
{
  gint sw, sh;

  gimp_display_shell_draw_get_scaled_image_size (shell,
                                                 &sw,
                                                 &sh);

  if (image_center_x) *image_center_x = -shell->offset_x + sw / 2;
  if (image_center_y) *image_center_y = -shell->offset_y + sh / 2;
}

gboolean
gimp_display_shell_scale_image_is_within_viewport (GimpDisplayShell *shell,
                                                   gboolean         *horizontally,
                                                   gboolean         *vertically)
{
  gint     sw, sh;
  gboolean horizontally_dummy, vertically_dummy;

  g_return_val_if_fail (GIMP_IS_DISPLAY_SHELL (shell), FALSE);

  if (! horizontally) horizontally = &horizontally_dummy;
  if (! vertically)   vertically   = &vertically_dummy;

  gimp_display_shell_draw_get_scaled_image_size (shell, &sw, &sh);

  *horizontally = sw              <= shell->disp_width       &&
                  shell->offset_x <= 0                       &&
                  shell->offset_x >= sw - shell->disp_width;

  *vertically   = sh              <= shell->disp_height      &&
                  shell->offset_y <= 0                       &&
                  shell->offset_y >= sh - shell->disp_height;

  return *vertically && *horizontally;
}

static gboolean
gimp_display_shell_scale_image_starts_to_fit (GimpDisplayShell *shell,
                                              gdouble           new_scale,
                                              gdouble           current_scale,
                                              gboolean         *vertically,
                                              gboolean         *horizontally)
{
  gboolean vertically_dummy;
  gboolean horizontally_dummy;

  if (! vertically)   vertically   = &vertically_dummy;
  if (! horizontally) horizontally = &horizontally_dummy;

  /* The image can only start to fit if we zoom out */
  if (new_scale > current_scale)
    {
      *vertically   = FALSE;
      *horizontally = FALSE;
    }
  else
    {
      gint current_scale_width;
      gint current_scale_height;
      gint new_scale_width;
      gint new_scale_height;

      gimp_display_shell_draw_get_scaled_image_size_for_scale (shell,
                                                               current_scale,
                                                               &current_scale_width,
                                                               &current_scale_height);

      gimp_display_shell_draw_get_scaled_image_size_for_scale (shell,
                                                               new_scale,
                                                               &new_scale_width,
                                                               &new_scale_height);

      *vertically   = (current_scale_width  >  shell->disp_width &&
                       new_scale_width      <= shell->disp_width);
      *horizontally = (current_scale_height >  shell->disp_height &&
                       new_scale_height     <= shell->disp_height);
    }

  return *vertically && *horizontally;
}

static gboolean
gimp_display_shell_scale_image_stops_to_fit (GimpDisplayShell *shell,
                                             gdouble           new_scale,
                                             gdouble           current_scale,
                                             gboolean         *vertically,
                                             gboolean         *horizontally)
{
  return gimp_display_shell_scale_image_starts_to_fit (shell,
                                                       current_scale,
                                                       new_scale,
                                                       vertically,
                                                       horizontally);
}

static gboolean
gimp_display_shell_scale_viewport_coord_almost_centered (GimpDisplayShell *shell,
                                                         gint              x,
                                                         gint              y,
                                                         gboolean         *horizontally,
                                                         gboolean         *vertically)
{
  gboolean local_horizontally;
  gboolean local_vertically;
  gint     center_x = shell->disp_width  / 2;
  gint     center_y = shell->disp_height / 2;

  local_horizontally = (x > center_x - ALMOST_CENTERED_THRESHOLD &&
                        x < center_x + ALMOST_CENTERED_THRESHOLD);

  local_vertically   = (y > center_y - ALMOST_CENTERED_THRESHOLD &&
                        y < center_y + ALMOST_CENTERED_THRESHOLD);

  if (horizontally) *horizontally = local_horizontally;
  if (vertically)   *vertically   = local_vertically;

  return local_horizontally && local_vertically;
}

void
gimp_display_shell_calculate_scale_x_and_y (GimpDisplayShell *shell,
                                            gdouble           scale,
                                            gdouble          *scale_x,
                                            gdouble          *scale_y)
{
  GimpImage *image;
  gdouble    xres;
  gdouble    yres;
  gdouble    screen_xres;
  gdouble    screen_yres;

  g_return_if_fail (GIMP_IS_DISPLAY_SHELL (shell));

  image = gimp_display_get_image (shell->display);

  g_return_if_fail (GIMP_IS_IMAGE (image));

  gimp_image_get_resolution (image, &xres, &yres);
  gimp_display_shell_get_screen_resolution (shell, &screen_xres, &screen_yres);

  if (scale_x) *scale_x = scale * screen_xres / xres;
  if (scale_y) *scale_y = scale * screen_yres / yres;
}

void
gimp_display_shell_scale_by_values (GimpDisplayShell *shell,
                                    gdouble           scale,
                                    gint              offset_x,
                                    gint              offset_y,
                                    gboolean          resize_window)
{
  g_return_if_fail (GIMP_IS_DISPLAY_SHELL (shell));

  /*  Abort early if the values are all setup already. We don't
   *  want to inadvertently resize the window (bug #164281).
   */
  if (SCALE_EQUALS (gimp_zoom_model_get_factor (shell->zoom), scale) &&
      shell->offset_x == offset_x &&
      shell->offset_y == offset_y)
    return;

  gimp_display_shell_scale_handle_zoom_revert (shell);

  /* freeze the active tool */
  gimp_display_shell_pause (shell);

  gimp_zoom_model_zoom (shell->zoom, GIMP_ZOOM_TO, scale);

  shell->offset_x = offset_x;
  shell->offset_y = offset_y;

  gimp_display_shell_scale_resize (shell, resize_window, FALSE);

  /* re-enable the active tool */
  gimp_display_shell_resume (shell);
}

void
gimp_display_shell_scale_resize (GimpDisplayShell *shell,
                                 gboolean          resize_window,
                                 gboolean          grow_only)
{
  g_return_if_fail (GIMP_IS_DISPLAY_SHELL (shell));

  /* freeze the active tool */
  gimp_display_shell_pause (shell);

  if (resize_window)
    {
      GimpImageWindow *window = gimp_display_shell_get_window (shell);

      if (window && gimp_image_window_get_active_shell (window) == shell)
        {
          gimp_image_window_shrink_wrap (window, grow_only);
        }
    }

  gimp_display_shell_scroll_clamp_and_update (shell);
  gimp_display_shell_scaled (shell);

  gimp_display_shell_expose_full (shell);

  /* re-enable the active tool */
  gimp_display_shell_resume (shell);
}

void
gimp_display_shell_get_screen_resolution (GimpDisplayShell *shell,
                                          gdouble          *xres,
                                          gdouble          *yres)
{
  gdouble x, y;

  g_return_if_fail (GIMP_IS_DISPLAY_SHELL (shell));

  if (shell->dot_for_dot)
    {
      gimp_image_get_resolution (gimp_display_get_image (shell->display),
                                 &x, &y);
    }
  else
    {
      x = shell->monitor_xres;
      y = shell->monitor_yres;
    }

  if (xres) *xres = x;
  if (yres) *yres = y;
}

void
gimp_display_shell_scroll_center_image (GimpDisplayShell *shell,
                                        gboolean          horizontally,
                                        gboolean          vertically)
{
  gint sw, sh;
  gint target_offset_x, target_offset_y;

  g_return_if_fail (GIMP_IS_DISPLAY_SHELL (shell));

  if (! shell->display                          ||
      ! gimp_display_get_image (shell->display) ||
      (! vertically && ! horizontally))
    return;

  target_offset_x = shell->offset_x;
  target_offset_y = shell->offset_y;

  gimp_display_shell_draw_get_scaled_image_size (shell, &sw, &sh);

  if (horizontally)
    {
      target_offset_x = (sw - shell->disp_width) / 2;
    }

  if (vertically)
    {
      target_offset_y = (sh - shell->disp_height) / 2;
    }

  gimp_display_shell_scroll_set_offset (shell,
                                        target_offset_x,
                                        target_offset_y);
}

void
gimp_display_shell_scroll_set_offset (GimpDisplayShell *shell,
                                      gint              offset_x,
                                      gint              offset_y)
{
  g_return_if_fail (GIMP_IS_DISPLAY_SHELL (shell));

  if (shell->offset_x == offset_x &&
      shell->offset_y == offset_y)
    return;

  gimp_display_shell_scale_handle_zoom_revert (shell);

  /* freeze the active tool */
  gimp_display_shell_pause (shell);

  shell->offset_x = offset_x;
  shell->offset_y = offset_y;

  gimp_display_shell_scroll_clamp_and_update (shell);

  gimp_display_shell_scrolled (shell);

  gimp_display_shell_expose_full (shell);

  /* re-enable the active tool */
  gimp_display_shell_resume (shell);
}

void
gimp_display_shell_scroll_clamp_offsets (GimpDisplayShell *shell)
{
  GimpImage *image;

  g_return_if_fail (GIMP_IS_DISPLAY_SHELL (shell));

  image = gimp_display_get_image (shell->display);

  if (image)
    {
      gint sw, sh;
      gint min_offset_x;
      gint max_offset_x;
      gint min_offset_y;
      gint max_offset_y;

      sw = SCALEX (shell, gimp_image_get_width  (image));
      sh = SCALEY (shell, gimp_image_get_height (image));

      if (shell->disp_width < sw)
        {
          min_offset_x = 0  - shell->disp_width * OVERPAN_FACTOR;
          max_offset_x = sw - shell->disp_width * (1.0 - OVERPAN_FACTOR);
        }
      else
        {
          gint overpan_amount;

          overpan_amount = shell->disp_width - sw * (1.0 - OVERPAN_FACTOR);

          min_offset_x = 0  - overpan_amount;
          max_offset_x = sw + overpan_amount - shell->disp_width;
        }

      if (shell->disp_height < sh)
        {
          min_offset_y = 0  - shell->disp_height * OVERPAN_FACTOR;
          max_offset_y = sh - shell->disp_height * (1.0 - OVERPAN_FACTOR);
        }
      else
        {
          gint overpan_amount;

          overpan_amount = shell->disp_height - sh * (1.0 - OVERPAN_FACTOR);

          min_offset_y = 0  - overpan_amount;
          max_offset_y = sh + overpan_amount - shell->disp_height;
        }


      /* Handle scrollbar stepper sensitiity */

      gtk_range_set_lower_stepper_sensitivity (GTK_RANGE (shell->hsb),
                                               min_offset_x < shell->offset_x ?
                                               GTK_SENSITIVITY_ON :
                                               GTK_SENSITIVITY_OFF);

      gtk_range_set_upper_stepper_sensitivity (GTK_RANGE (shell->hsb),
                                               max_offset_x > shell->offset_x ?
                                               GTK_SENSITIVITY_ON :
                                               GTK_SENSITIVITY_OFF);

      gtk_range_set_lower_stepper_sensitivity (GTK_RANGE (shell->vsb),
                                               min_offset_y < shell->offset_y ?
                                               GTK_SENSITIVITY_ON :
                                               GTK_SENSITIVITY_OFF);

      gtk_range_set_upper_stepper_sensitivity (GTK_RANGE (shell->vsb),
                                               max_offset_y > shell->offset_y ?
                                               GTK_SENSITIVITY_ON :
                                               GTK_SENSITIVITY_OFF);


      /* Clamp */

      shell->offset_x = CLAMP (shell->offset_x, min_offset_x, max_offset_x);
      shell->offset_y = CLAMP (shell->offset_y, min_offset_y, max_offset_y);
    }
  else
    {
      shell->offset_x = 0;
      shell->offset_y = 0;
    }
}

void
gimp_display_shell_scroll_clamp_and_update (GimpDisplayShell *shell)
{
  gimp_display_shell_scroll_clamp_offsets (shell);
  gimp_display_shell_update_scrollbars_and_rulers (shell);
}

void
gimp_display_shell_draw_get_scaled_image_size (GimpDisplayShell *shell,
                                               gint             *w,
                                               gint             *h)
{
  g_return_if_fail (GIMP_IS_DISPLAY_SHELL (shell));

  gimp_display_shell_draw_get_scaled_image_size_for_scale (shell,
                                                           gimp_zoom_model_get_factor (shell->zoom),
                                                           w,
                                                           h);
}

void
gimp_display_shell_draw_get_scaled_image_size_for_scale (GimpDisplayShell *shell,
                                                         gdouble           scale,
                                                         gint             *w,
                                                         gint             *h)
{
  GimpImage      *image;
  GimpProjection *proj;
  TileManager    *tiles;
  gdouble         scale_x;
  gdouble         scale_y;

  g_return_if_fail (GIMP_IS_DISPLAY_SHELL (shell));

  image = gimp_display_get_image (shell->display);

  g_return_if_fail (GIMP_IS_IMAGE (image));

  proj = gimp_image_get_projection (image);

  gimp_display_shell_calculate_scale_x_and_y (shell, scale, &scale_x, &scale_y);

  tiles = gimp_projection_get_tiles_at_level (proj, 0, NULL);

  if (w) *w = scale_x * tile_manager_width (tiles);
  if (h) *h = scale_y * tile_manager_height (tiles);
}

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
gimp_display_shell_scale_changed (GimpDisplayShell *shell)
{
  GimpImage *image;

  g_return_if_fail (GIMP_IS_DISPLAY_SHELL (shell));

  image = gimp_display_get_image (shell->display);

  if (image)
    {
      gimp_display_shell_calculate_scale_x_and_y (shell,
                                                  gimp_zoom_model_get_factor (shell->zoom),
                                                  &shell->scale_x,
                                                  &shell->scale_y);

      shell->x_dest_inc = gimp_image_get_width  (image);
      shell->y_dest_inc = gimp_image_get_height (image);
      shell->x_src_dec  = shell->scale_x * shell->x_dest_inc;
      shell->y_src_dec  = shell->scale_y * shell->y_dest_inc;

      if (shell->x_src_dec < 1)
        shell->x_src_dec = 1;

      if (shell->y_src_dec < 1)
        shell->y_src_dec = 1;
    }
  else
    {
      shell->scale_x = 1.0;
      shell->scale_y = 1.0;

      shell->x_dest_inc = 1;
      shell->y_dest_inc = 1;
      shell->x_src_dec  = 1;
      shell->y_src_dec  = 1;
    }
}

void
gimp_zoom_model_zoom (GimpZoomModel *model,
                      GimpZoomType   zoom_type,
                      gdouble        scale)
{
  g_return_if_fail (GIMP_IS_ZOOM_MODEL (model));

  if (zoom_type != GIMP_ZOOM_TO)
    scale = gimp_zoom_model_get_factor (model);

  g_object_set (model,
                "value", gimp_zoom_model_zoom_step (zoom_type, scale),
                NULL);
}

gdouble
gimp_zoom_model_get_factor (GimpZoomModel *model)
{
  g_return_val_if_fail (GIMP_IS_ZOOM_MODEL (model), 1.0);

  return GIMP_ZOOM_MODEL_GET_PRIVATE (model)->value;
}

gdouble
gimp_zoom_model_zoom_step (GimpZoomType zoom_type,
                           gdouble      scale)
{
  gint    i, n_presets;
  gdouble new_scale = 1.0;

  /* This table is constructed to have fractions, that approximate
   * sqrt(2)^k. This gives a smooth feeling regardless of the starting
   * zoom level.
   *
   * Zooming in/out always jumps to a zoom step from the list below.
   * However, we try to guarantee a certain size of the step, to
   * avoid silly jumps from 101% to 100%.
   *
   * The factor 1.1 is chosen a bit arbitrary, but feels better
   * than the geometric median of the zoom steps (2^(1/4)).
   */

#define ZOOM_MIN_STEP 1.1

  const gdouble presets[] = {
    1.0 / 256, 1.0 / 180, 1.0 / 128, 1.0 / 90,
    1.0 / 64,  1.0 / 45,  1.0 / 32,  1.0 / 23,
    1.0 / 16,  1.0 / 11,  1.0 / 8,   2.0 / 11,
    1.0 / 4,   1.0 / 3,   1.0 / 2,   2.0 / 3,
      1.0,
               3.0 / 2,      2.0,      3.0,
      4.0,    11.0 / 2,      8.0,     11.0,
      16.0,     23.0,       32.0,     45.0,
      64.0,     90.0,      128.0,    180.0,
      256.0,
  };

  n_presets = G_N_ELEMENTS (presets);

  switch (zoom_type)
    {
    case GIMP_ZOOM_IN:
      scale *= ZOOM_MIN_STEP;

      new_scale = presets[n_presets - 1];
      for (i = n_presets - 1; i >= 0 && presets[i] > scale; i--)
        new_scale = presets[i];

      break;

    case GIMP_ZOOM_OUT:
      scale /= ZOOM_MIN_STEP;

      new_scale = presets[0];
      for (i = 0; i < n_presets && presets[i] < scale; i++)
        new_scale = presets[i];

      break;

    case GIMP_ZOOM_IN_MORE:
      scale = gimp_zoom_model_zoom_step (GIMP_ZOOM_IN, scale);
      scale = gimp_zoom_model_zoom_step (GIMP_ZOOM_IN, scale);
      scale = gimp_zoom_model_zoom_step (GIMP_ZOOM_IN, scale);
      new_scale = scale;
      break;

    case GIMP_ZOOM_OUT_MORE:
      scale = gimp_zoom_model_zoom_step (GIMP_ZOOM_OUT, scale);
      scale = gimp_zoom_model_zoom_step (GIMP_ZOOM_OUT, scale);
      scale = gimp_zoom_model_zoom_step (GIMP_ZOOM_OUT, scale);
      new_scale = scale;
      break;

    case GIMP_ZOOM_IN_MAX:
      new_scale = ZOOM_MAX;
      break;

    case GIMP_ZOOM_OUT_MAX:
      new_scale = ZOOM_MIN;
      break;

    case GIMP_ZOOM_TO:
      new_scale = scale;
      break;
    }

  return CLAMP (new_scale, ZOOM_MIN, ZOOM_MAX);

#undef ZOOM_MIN_STEP
}

static void
gimp_zoom_model_set_property (GObject      *object,
                              guint         property_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  GimpZoomModelPrivate *priv  = GIMP_ZOOM_MODEL_GET_PRIVATE (object);
  gdouble               previous_value;

  previous_value = priv->value;
  g_object_freeze_notify (object);

  switch (property_id)
    {
    case PROP_VALUE:
      priv->value = g_value_get_double (value);

      g_object_notify (object, "value");
      g_object_notify (object, "fraction");
      g_object_notify (object, "percentage");
      break;

    case PROP_MINIMUM:
      priv->minimum = MIN (g_value_get_double (value), priv->maximum);
      break;

    case PROP_MAXIMUM:
      priv->maximum = MAX (g_value_get_double (value), priv->minimum);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }

  if (priv->value > priv->maximum || priv->value < priv->minimum)
    {
      priv->value = CLAMP (priv->value, priv->minimum, priv->maximum);

      g_object_notify (object, "value");
      g_object_notify (object, "fraction");
      g_object_notify (object, "percentage");
    }

  g_object_thaw_notify (object);

  if (priv->value != previous_value)
    {
      g_signal_emit (object, zoom_model_signals[ZOOMED],
                     0, previous_value, priv->value);
    }
}
