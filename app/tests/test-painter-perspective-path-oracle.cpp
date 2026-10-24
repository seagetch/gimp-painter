/* Unchanged pinned path construction at the SAME current outer display matrix.
 * Not an old-runtime execution or independent old viewport-transform oracle. */
#include "config.h"
#include <cmath>
#include <functional>
#include <gegl.h>
#include <gtk/gtk.h>
extern "C" {
#include "core/core-types.h"
#include "display/display-types.h"
#include "core/gimpperspectiveguide.h"
#include "display/gimpdisplayshell.h"
#include "display/gimpcanvasitem.h"
}
namespace LegacyPerspectivePath {
struct GimpDisplayShell { gdouble offset_x, offset_y, scale_x, scale_y; };
struct Guide {
  ::GimpPerspectiveGuide *value;
  gint get_vanish_point_length () { return ::gimp_perspective_guide_get_vanish_point_length (value); }
  void get_vanish_points (gint index, gdouble *x, gdouble *y) {
    g_assert_true (::gimp_perspective_guide_get_vanish_points (value, index, x, y));
  }
};
struct PerspectiveGuideInterface { static Guide *cast (Guide *guide) { return guide; } };
/* The caller pre-applies the identical native rigid/fractional outer matrix to
 * BOTH rasters. This boundary deliberately does not claim old transform parity. */
static void gimp_display_shell_set_cairo_rotate (GimpDisplayShell *, cairo_t *) {}
static const int _gimp_canvas_item_stroke = 0;
struct Reference {
  ::GimpCanvasItem *item;
  std::function<void(cairo_t *)> operator[] (int) {
    return [this] (cairo_t *cr) { ::_gimp_canvas_item_stroke (item, cr); };
  }
};
static Reference ref (::GimpCanvasItem *item) { return { item }; }
class CanvasPerspectiveGuide {
public:
  Guide *guide;
  ::GimpCanvasItem *g_object;
  void draw (GimpDisplayShell *shell, cairo_t *cr);
};
#include "../../migration/fixtures/legacy-perspective-draw/draw-source.inc"
}
extern "C" void gimp_test_perspective_old_path (::GimpDisplayShell *shell,
                                               ::GimpPerspectiveGuide *guide,
                                               ::GimpCanvasItem *item,
                                               cairo_t *cr);
extern "C" void gimp_test_perspective_old_path (::GimpDisplayShell *shell,
                                               ::GimpPerspectiveGuide *guide,
                                               ::GimpCanvasItem *item,
                                               cairo_t *cr)
{
  LegacyPerspectivePath::GimpDisplayShell view { double (shell->offset_x), double (shell->offset_y), shell->scale_x, shell->scale_y };
  LegacyPerspectivePath::Guide model { guide };
  LegacyPerspectivePath::CanvasPerspectiveGuide overlay;
  overlay.guide = &model; overlay.g_object = item;
  overlay.draw (&view, cr);
}
