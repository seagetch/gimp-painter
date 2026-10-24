static gboolean
gimp_display_shell_lazy_snap (GimpDisplayShell* shell, GimpImage* image, GimpCoords* image_coords)
{
  gboolean snapped = FALSE;
  GimpPerspectiveGuide* guide = gimp_image_get_perspective_guide (image);
  if (!guide)
    return TRUE;
  g_print("Check snapping...\n");
  if (shell->snapping) {
    snapped = gimp_display_shell_snap_angle (shell, image_coords->x, image_coords->y, &shell->snapped_angle, 
                                      shell->snap_origin.x, shell->snap_origin.y,
                                      guide);
  } else {
    snapped = TRUE;
  }
  if (snapped) {
    gdouble dx, dy;
    gdouble cosx, siny;
    dx = image_coords->x - shell->snap_origin.x;
    dy = image_coords->y - shell->snap_origin.y;
    cosx = cos(shell->snapped_angle);
    siny = sin(shell->snapped_angle);
    if (shell->snapped_angle < M_PI / 4 || shell->snapped_angle > M_PI * 3 / 4) {
      image_coords->y = shell->snap_origin.y - siny / cosx * dx;
    } else {
      image_coords->x = shell->snap_origin.x - cosx / siny * dy; 
    }
    g_print("angle=%f, x=%f, y=%f\n", shell->snapped_angle / M_PI * 180, image_coords->x, image_coords->y);
  }
  return snapped;
}
