/* Include after the unchanged legacy-pan oracle. */
typedef struct { LegacyPanShell view; gboolean rotating; gdouble rotate_start_angle; } LegacyAppliedRotation;
#define GimpDisplayShell LegacyAppliedRotation
#define disp_width view.disp_width
#define disp_height view.disp_height
#define mirrored view.mirrored
#define rotate_angle view.rotate_angle
#define ROTATE_SNAP_UNIT 15.0
#define gimp_display_shell_start_rotating legacy_applied_rotation_begin
#define gimp_display_shell_rotate legacy_applied_rotation_motion
#define gimp_display_shell_stop_rotating legacy_applied_rotation_end
#define gimp_display_shell_pointer_grab(s,e,m) ((void)(s),(void)(e),(void)(m))
#define gimp_display_shell_pointer_ungrab(s,e) ((void)(s),(void)(e))
#define gimp_display_shell_set_override_cursor(s,c) ((void)(s),(void)(c))
#define gimp_display_shell_unset_override_cursor(s) ((void)(s))
#define gimp_display_shell_expose_full(s) ((void)(s))
static void
gimp_display_shell_start_rotating (GimpDisplayShell *shell,
                                    const GdkEvent   *event,
                                    gint              x,
                                    gint              y)
{
  gdouble cx, cy;
  gdouble rx, ry;
  g_return_if_fail (! shell->rotating);

  gimp_display_shell_pointer_grab (shell, event, GDK_POINTER_MOTION_MASK);

  shell->rotating      = TRUE;

  cx = shell->disp_width  / 2;
  cy = shell->disp_height / 2;
  rx = x - cx;
  ry = y - cy;

  if (shell->mirrored)
    rx = -rx;

  shell->rotate_start_angle = shell->rotate_angle + 
    fmod(atan2(rx, ry) / M_PI * 180.0 + 360.0, 360.0); 

  gimp_display_shell_set_override_cursor (shell, GDK_EXCHANGE);
}

static void
gimp_display_shell_rotate        (GimpDisplayShell *shell,
                                  const GdkEvent*         event,
                                  gint              x,
                                  gint              y)
{
  gdouble cx, cy;
  gdouble rx, ry;
  gdouble angle;
  GdkEventMotion* mevent = (GdkEventMotion*)event;
  g_return_if_fail (shell->rotating);

  cx = shell->disp_width  / 2;
  cy = shell->disp_height / 2;
  rx = x - cx;
  ry = y - cy;

  if (shell->mirrored)
    rx = -rx;

  angle               = shell->rotate_start_angle - 
                        fmod(atan2(rx, ry) / M_PI * 180.0 + 360.0, 360.0);
  angle = fmod(angle + 360.0, 360.0);
  if (mevent->state & GDK_CONTROL_MASK) {
    angle = (gint)((angle + ROTATE_SNAP_UNIT / 2) / ROTATE_SNAP_UNIT) * ROTATE_SNAP_UNIT;
    angle = fmod(angle + 360.0, 360.0);
  }
  shell->rotate_angle = angle;
  gimp_display_shell_expose_full(shell);
}

static void
gimp_display_shell_stop_rotating (GimpDisplayShell *shell,
                                   const GdkEvent   *event)
{
  g_return_if_fail (shell->rotating);

  gimp_display_shell_unset_override_cursor (shell);

  shell->rotating           = FALSE;
  shell->rotate_start_angle = 0;

  gimp_display_shell_pointer_ungrab (shell, event);
}

#undef GimpDisplayShell
#undef disp_width
#undef disp_height
#undef mirrored
#undef rotate_angle
#undef ROTATE_SNAP_UNIT
#undef gimp_display_shell_start_rotating
#undef gimp_display_shell_rotate
#undef gimp_display_shell_stop_rotating
#undef gimp_display_shell_pointer_grab
#undef gimp_display_shell_pointer_ungrab
#undef gimp_display_shell_set_override_cursor
#undef gimp_display_shell_unset_override_cursor
#undef gimp_display_shell_expose_full
