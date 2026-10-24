void
gimp_display_shell_zoom_xy_f  (const GimpDisplayShell *shell,
                               gdouble                 x,
                               gdouble                 y,
                               gdouble                *nx,
                               gdouble                *ny)
{
  g_return_if_fail (GIMP_IS_DISPLAY_SHELL (shell));
  g_return_if_fail (nx != NULL);
  g_return_if_fail (ny != NULL);

  *nx = SCALEX (shell, x) - shell->offset_x;
  *ny = SCALEY (shell, y) - shell->offset_y;
}
