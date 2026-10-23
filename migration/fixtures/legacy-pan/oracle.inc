/* Pinned source-only old pan oracle; no old application/runtime. */
typedef struct { gint disp_width,disp_height; gdouble rotate_angle; gboolean mirrored,scrolling; gdouble offset_x,offset_y,scroll_start_x,scroll_start_y; gpointer canvas,display,hsb,vsb; gint image_width,image_height; gdouble scale_x,scale_y; } LegacyPanShell;
void legacy_pan_set_rotate (const LegacyPanShell *, cairo_t *);
void legacy_pan_inverse (const LegacyPanShell *, gdouble *, gdouble *);
static cairo_t *legacy_pan_cairo(void) { cairo_surface_t*s=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,1,1);cairo_t*c=cairo_create(s);cairo_surface_destroy(s);return c; }
#define GimpDisplayShell LegacyPanShell
#define gimp_display_shell_set_cairo_rotate legacy_pan_set_rotate
#define gimp_display_shell_device_to_image_coords legacy_pan_inverse
#define gimp_display_shell_start_scrolling legacy_pan_begin
#define gimp_display_shell_pointer_grab(s,e,m) ((void)(s),(void)(e),(void)(m))
#define gimp_display_shell_set_override_cursor(s,c) ((void)(s),(void)(c))
#define gtk_widget_get_window(w) (w)
#define gdk_cairo_create(w) ((void)(w),legacy_pan_cairo())
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

static void
gimp_display_shell_start_scrolling (GimpDisplayShell *shell,
                                    const GdkEvent   *event,
                                    gint              x,
                                    gint              y)
{
  g_return_if_fail (! shell->scrolling);

  gimp_display_shell_pointer_grab (shell, event, GDK_POINTER_MOTION_MASK);

  shell->scrolling      = TRUE;
  shell->scroll_start_x = x;
  shell->scroll_start_y = y;

  shell->scroll_start_x += shell->offset_x;
  shell->scroll_start_y += shell->offset_y;

  gimp_display_shell_set_override_cursor (shell, GDK_FLEUR);
}

#pragma push_macro("GTK_RANGE")
#undef GTK_RANGE
#pragma push_macro("GIMP_IS_DISPLAY_SHELL")
#undef GIMP_IS_DISPLAY_SHELL

#pragma push_macro("SCALEX")
#pragma push_macro("SCALEY")
#undef SCALEX
#undef SCALEY
#define SCALEX(s,v) ((gint) RINT ((s)->scale_x * (v)))
#define SCALEY(s,v) ((gint) RINT ((s)->scale_y * (v)))
#define OVERPAN_FACTOR 0.5
#define GIMP_IS_DISPLAY_SHELL(s) ((s) != NULL)
#define gimp_display_get_image(d) ((void)(d),(GimpImage *) 1)
#define gimp_image_get_width(i) ((void)(i),shell->image_width)
#define gimp_image_get_height(i) ((void)(i),shell->image_height)
#define GTK_RANGE(p) (p)
#define gtk_range_set_lower_stepper_sensitivity(r,v) ((void)(r),(void)(v))
#define gtk_range_set_upper_stepper_sensitivity(r,v) ((void)(r),(void)(v))
#define gimp_display_shell_scroll_clamp_offsets legacy_pan_clamp_offsets
void legacy_pan_clamp_offsets (LegacyPanShell *);
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

#undef gimp_display_shell_scroll_clamp_offsets
#undef gtk_range_set_lower_stepper_sensitivity
#undef gtk_range_set_upper_stepper_sensitivity
#undef GTK_RANGE
#undef gimp_image_get_width
#undef gimp_image_get_height
#undef gimp_display_get_image
#undef GIMP_IS_DISPLAY_SHELL
#undef OVERPAN_FACTOR
#pragma pop_macro("SCALEX")
#pragma pop_macro("SCALEY")
#pragma pop_macro("GIMP_IS_DISPLAY_SHELL")
#pragma pop_macro("GTK_RANGE")

#define gimp_display_shell_scroll(s,dx,dy) ((s)->offset_x+=(dx),(s)->offset_y+=(dy),legacy_pan_clamp_offsets(s))
static void legacy_pan_motion(LegacyPanShell *shell,gdouble x,gdouble y) {
            gdouble offset_x = shell->offset_x;
            gdouble offset_y = shell->offset_y;
            gdouble start_x  = shell->scroll_start_x;
            gdouble start_y  = shell->scroll_start_y;

            gimp_display_shell_device_to_image_coords(shell, &x, &y);

            gimp_display_shell_scroll (shell,
                                       start_x - x - offset_x,
                                       start_y - y - offset_y);

}
#undef gimp_display_shell_scroll

#undef GimpDisplayShell
#undef gimp_display_shell_set_cairo_rotate
#undef gimp_display_shell_device_to_image_coords
#undef gimp_display_shell_start_scrolling
#undef gimp_display_shell_pointer_grab
#undef gimp_display_shell_set_override_cursor
#undef gtk_widget_get_window
#undef gdk_cairo_create
