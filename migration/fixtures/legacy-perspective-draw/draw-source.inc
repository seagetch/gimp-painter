void 
CanvasPerspectiveGuide::draw(GimpDisplayShell *shell, cairo_t *cr)
{
  if (guide) {
    auto cairo_draw = [&](std::function<void(void)> callback) {
      cairo_save (cr);
      gimp_display_shell_set_cairo_rotate(shell, cr);
      cairo_translate (cr, -shell->offset_x, -shell->offset_y);
      cairo_scale (cr, shell->scale_x, shell->scale_y);
      callback();
  //    cairo_translate (cr, private->x, private->y);

  //    cairo_append_path (cr, private->path);
      cairo_restore (cr);
    };

    auto i_guide = PerspectiveGuideInterface::cast(guide);
    gint length = i_guide->get_vanish_point_length();
    switch (length) {
    case 1:
//      g_print("Display one vanishment point.\n");
      cairo_draw(std::function<void(void)>([&]() {
        gdouble x, y;
        i_guide->get_vanish_points(0, &x, &y);
        cairo_move_to(cr, x, y);
        cairo_arc(cr, x, y, 8 / shell->scale_x, 0, 2*M_PI);
      }));
      break;
    case 2:
//      g_print("Display two vanishment points.\n");
      cairo_draw(std::function<void(void)>([&]() {
        for (int i = 0; i < 2; i ++) {
          gdouble x, y;
          i_guide->get_vanish_points(i, &x, &y);
          if (i > 0)
            cairo_line_to(cr, x, y);
          cairo_arc(cr, x, y, 8 / shell->scale_x, 0, 2*M_PI);
          cairo_move_to(cr, x, y);
        }
      }));
      break;
    case 3:
//      g_print("Display three vanishment points.\n");
      cairo_draw(std::function<void(void)>([&]() {
        for (int i = 0; i < 3; i ++) {
          gdouble x, y;
          i_guide->get_vanish_points(i, &x, &y);
          cairo_move_to(cr, x, y);
          cairo_arc(cr, x, y, 8 / shell->scale_x, 0, 2*M_PI);
        }
      }));
      break;
    default:
      break;
    }

    ref(g_object) [_gimp_canvas_item_stroke] (cr);
  }
}
