bool 
Tool::get_nearest(gdouble x, gdouble y, Target* target, int* index, gdouble* min_distance)
{
#if 0
  if (!guide) {
    *target = None;
    return false;
  }
#endif
  if (!guide)
    return false;
  auto i_guide = PerspectiveGuideInterface::cast(guide);
  
  *index = -1;
  for (int i = 0; i < i_guide->get_vanish_point_length(); i ++) {
    gdouble px, py, dx, dy;
    gdouble distance;
    i_guide->get_vanish_points(i, &px, &py);
    dx = x - px;
    dy = y - py;
    distance = sqrt(dx*dx+dy*dy);
    if (*index < 0 || distance < *min_distance) {
      *min_distance = distance;
      *index = i;
    }
  }
  *target = VanishPoint;
  return true;
}
