/* Verbatim pinned legacy source helper. Name and g_print are macro-adapted by the test. */
#define DIST_THRESHOLD 32
static gboolean
gimp_display_shell_snap_angle (GimpDisplayShell * shell,
                       const gdouble x,
                       const gdouble y,
                       gdouble* snapped_angle,
                       const double origin_x,
                       const double origin_y,
                       GimpPerspectiveGuide* guide)
{
  gboolean snapped = TRUE;
  gint num_points = gimp_perspective_guide_get_vanish_point_length (guide);
  gdouble angle;
  gdouble angles[5];
  int num_angles = 0;
  int i;
  gdouble min_angle = -1;
  int min_index;
  gdouble cur_angle = 0;
  gdouble dx, dy;
  dx = (x - origin_x) * shell->scale_x;
  dy = (y - origin_y) * shell->scale_y;
  if (dx * dx + dy * dy < DIST_THRESHOLD * DIST_THRESHOLD)
    return FALSE;
  cur_angle = fmod(atan2(-dy, dx) + M_PI, M_PI);

  g_object_get(G_OBJECT(guide), "angle", &angle, NULL);

  g_print("cur_angle=%f, num_points=%d\n", cur_angle / M_PI * 180, num_points);

  switch (num_points) {
  case 1: {
    angles[num_angles++] = fmod(angle + M_PI, M_PI);
    angles[num_angles++] = fmod(angle + M_PI * 3 / 2, M_PI);
    break;
  }
  case 2: {
    gdouble p1x, p1y;
    gdouble p2x, p2y;
    gdouble w, h;
    gimp_perspective_guide_get_vanish_points (guide, 0, &p1x, &p1y);
    gimp_perspective_guide_get_vanish_points (guide, 1, &p2x, &p2y);
    w = p2x - p1x;
    h = p2y - p1y;
    angle = atan2(-h, w);
    angles[num_angles++] = fmod(angle + M_PI * 3 / 2, M_PI);
    break;
  }
  case 3: {
    break;
  }
  default:
    return FALSE;
  }

  for (i = 0; i < num_points; i ++) {
    gdouble van_dx, van_dy;
    gdouble ang;
    gdouble px, py;
    gimp_perspective_guide_get_vanish_points (guide, i, &px, &py);
    van_dx = origin_x - px;
    van_dy = origin_y - py;
    ang = atan2(-van_dy, van_dx);
    angles[num_angles++] = fmod(ang + M_PI, M_PI); 
  }

  min_index = 0;
  // check against angles to vanish_points
  for (i = 0; i < num_angles; i ++) {
    // check against 
    gdouble ang = fmod(angles[i] + M_PI - cur_angle, M_PI);
    ang = MIN(ang, M_PI - ang);
    g_print("Check against angle %f<diff=%f>...", angles[i] * 180 / M_PI, ang*180/M_PI);
    if (min_angle < 0 || ang < min_angle) {
      min_index = i;
      min_angle = ang;
      g_print("updated");
    }
    g_print("\n");
  }

  *snapped_angle = angles[min_index];

  return snapped;
}
