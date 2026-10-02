/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gegl.h>
#include <gtk/gtk.h>
extern "C" {
#include "libgimpbase/gimpbase.h"
#include "display-types.h"
#include "core/gimpimage.h"
#include "core/gimpimage-perspective-guide.h"
#include "gimpcanvasperspectiveguide.h"
#include "gimpdisplay.h"
#include "gimpdisplayshell.h"
}
#include "core/gimpperspectiveguide.hpp"
#include "painter/binding-store.hpp"
#include "painter/connection.hpp"
#include "painter/gimp-painter-binding.h"
#include <cmath>
#include <limits>
using namespace GimpPainter;
G_DEFINE_TYPE (GimpCanvasPerspectiveGuide, gimp_canvas_perspective_guide, GIMP_TYPE_CANVAS_ITEM)
namespace GimpPainter {
template<> struct TypeTraits<GimpCanvasPerspectiveGuide>
{ static GType type () noexcept { return GIMP_TYPE_CANVAS_PERSPECTIVE_GUIDE; } };
}
namespace {
struct OverlayImpl;
struct OverlaySlot : SlotSpec<GimpCanvasPerspectiveGuide, OverlayImpl> {};
struct Callback {
  WeakRef<GObject> owner;
  std::uint64_t generation;
  explicit Callback (GObject *object) : owner (ObjectRef<GObject>::retain (object)),
    generation (BindingStore::require (object).generation ()) {}
};
void destroy_callback (gpointer data, GClosure *) { delete static_cast<Callback *> (data); }
template<class F> void visit (gpointer data, F function)
{
  boundary_void (nullptr, [&] {
    auto *callback = static_cast<Callback *> (data); auto owner = callback->owner.lock ();
    if (!owner) return;
    auto *binding = BindingStore::find (owner.get ());
    if (binding && binding->accepts (callback->generation)) binding->with<OverlaySlot> (function);
  });
}
void image_changed (GObject *, gpointer data);
void display_changed (GObject *, GParamSpec *, gpointer data);
void shell_changed (GObject *, GParamSpec *, gpointer data);
void guide_changed (GObject *, gpointer data);
struct OverlayImpl {
  explicit OverlayImpl (GimpCanvasItem *item) : item (item) {}
  GimpCanvasItem *item;
  WeakRef<GObject> image;
  ObjectRef<GimpPerspectiveGuide> guide;
  GimpPerspectiveGuideState state {};
  Connection shell_connection, display_connection, image_connection, guide_connection;
  void close () noexcept {
    shell_connection.close (); display_connection.close (); image_connection.close (); guide_connection.close ();
    image.reset (); guide.reset (); state = {};
  }
  Connection connect (GObject *emitter, const char *signal, GCallback callback) {
    auto data = std::unique_ptr<Callback> (new Callback (G_OBJECT (item)));
    auto connection = Connection::connect (ObjectRef<GObject>::retain (emitter), signal, callback,
                                          data.get (), destroy_callback);
    data.release (); return connection;
  }
  void refresh_state () {
    gimp_canvas_item_begin_change (item);
    state = {};
    if (guide) gimp_perspective_guide_get_state (guide.get (), &state);
    gimp_canvas_item_end_change (item);
  }
  void refresh_guide () {
    auto image_ref = image.lock ();
    auto replacement = ObjectRef<GimpPerspectiveGuide>::retain (image_ref ?
      gimp_image_get_perspective_guide (GIMP_IMAGE (image_ref.get ())) : nullptr);
    guide_connection.close ();
    guide = std::move (replacement);
    if (guide) guide_connection = connect (G_OBJECT (guide.get ()), "changed", G_CALLBACK (guide_changed));
    refresh_state ();
  }
  void refresh_display () {
    auto *shell = gimp_canvas_item_get_shell (item);
    display_connection.close ();
    if (shell->display) display_connection = connect (G_OBJECT (shell->display), "notify::image", G_CALLBACK (display_changed));
    refresh_image ();
  }
  void refresh_image () {
    auto *shell = gimp_canvas_item_get_shell (item);
    auto *current = shell->display ? gimp_display_get_image (shell->display) : nullptr;
    image_connection.close ();
    image = WeakRef<GObject> (ObjectRef<GObject>::retain (current ? G_OBJECT (current) : nullptr));
    if (current) image_connection = connect (G_OBJECT (current), "perspective-guide-changed", G_CALLBACK (image_changed));
    refresh_guide ();
  }
};
void image_changed (GObject *, gpointer data) { visit (data, [] (OverlayImpl& impl) { impl.refresh_guide (); }); }
void display_changed (GObject *, GParamSpec *, gpointer data) { visit (data, [] (OverlayImpl& impl) { impl.refresh_image (); }); }
void shell_changed (GObject *, GParamSpec *, gpointer data) { visit (data, [] (OverlayImpl& impl) { impl.refresh_display (); }); }
void guide_changed (GObject *, gpointer data) { visit (data, [] (OverlayImpl& impl) { impl.refresh_state (); }); }
void constructed (GObject *object)
{
  G_OBJECT_CLASS (gimp_canvas_perspective_guide_parent_class)->constructed (object);
  boundary_void (nullptr, [&] {
    auto& binding = BindingStore::require (object); binding.activate ();
    binding.with<OverlaySlot> ([] (OverlayImpl& impl) {
      auto *shell = gimp_canvas_item_get_shell (impl.item);
      /* items_init runs before the shell's construct-only display property. */
      impl.shell_connection = impl.connect (G_OBJECT (shell), "notify::display", G_CALLBACK (shell_changed));
      impl.refresh_display ();
    });
  });
}
void dispose (GObject *object)
{ gimp_painter_binding_close (object, nullptr); G_OBJECT_CLASS (gimp_canvas_perspective_guide_parent_class)->dispose (object); }
void draw (GimpCanvasItem *item, cairo_t *cr)
{
  boundary_void (nullptr, [&] { BindingStore::require (G_OBJECT (item)).read<OverlaySlot> ([&] (const OverlayImpl& impl) {
    auto *shell = gimp_canvas_item_get_shell (item);
    if (shell->scale_x <= 0) return;
    const double ry = 8 * shell->scale_y / shell->scale_x;
    double points[3][2];
    for (int i = 0; i < impl.state.n_points; ++i) {
      gimp_canvas_item_transform_xy_f (item, impl.state.points[i].x, impl.state.points[i].y, &points[i][0], &points[i][1]);
      cairo_save (cr); cairo_translate (cr, points[i][0], points[i][1]); cairo_scale (cr, 8, ry);
      cairo_new_sub_path (cr); cairo_arc (cr, 0, 0, 1, 0, 2 * G_PI); cairo_restore (cr);
    }
    if (impl.state.n_points == 2) {
      cairo_move_to (cr, points[0][0], points[0][1]); cairo_line_to (cr, points[1][0], points[1][1]);
    }
    _gimp_canvas_item_stroke (item, cr);
  }); });
}
cairo_region_t *extents (GimpCanvasItem *item)
{
  return boundary<cairo_region_t *> (nullptr, nullptr, [&] { return BindingStore::require (G_OBJECT (item)).read<OverlaySlot> ([&] (const OverlayImpl& impl) -> cairo_region_t * {
    if (!impl.state.n_points) return nullptr;
    auto *shell = gimp_canvas_item_get_shell (item);
    if (shell->scale_x <= 0) return nullptr;
    const double ry = 8 * shell->scale_y / shell->scale_x;
    double left = 0, top = 0, right = 0, bottom = 0;
    for (int i = 0; i < impl.state.n_points; ++i) {
      double x, y; gimp_canvas_item_transform_xy_f (item, impl.state.points[i].x, impl.state.points[i].y, &x, &y);
      if (!std::isfinite (x) || !std::isfinite (y)) return nullptr;
      if (i == 0) { left = x-10; top = y-ry-2; right = x+10; bottom = y+ry+2; }
      else { left = MIN (left, x-10); top = MIN (top, y-ry-2); right = MAX (right, x+10); bottom = MAX (bottom, y+ry+2); }
    }
    /* Cairo integer regions must not overflow for off-canvas vanishing points. */
    left = MAX (left, double (G_MININT / 2)); top = MAX (top, double (G_MININT / 2));
    right = MIN (right, double (G_MAXINT / 2)); bottom = MIN (bottom, double (G_MAXINT / 2));
    if (left >= right || top >= bottom) return nullptr;
    cairo_rectangle_int_t rect { gint (std::floor (left)), gint (std::floor (top)),
      gint (std::ceil (right) - std::floor (left)), gint (std::ceil (bottom) - std::floor (top)) };
    return cairo_region_create_rectangle (&rect);
  }); });
}
}
static void gimp_canvas_perspective_guide_class_init (GimpCanvasPerspectiveGuideClass *klass)
{
  G_OBJECT_CLASS (klass)->constructed = constructed; G_OBJECT_CLASS (klass)->dispose = dispose;
  GIMP_CANVAS_ITEM_CLASS (klass)->draw = draw; GIMP_CANVAS_ITEM_CLASS (klass)->get_extents = extents;
}
static void gimp_canvas_perspective_guide_init (GimpCanvasPerspectiveGuide *guide)
{
  guide->binding_failed = !boundary<bool> (nullptr, false, [&] {
    BindingStore::ensure (G_OBJECT (guide)).emplace<OverlaySlot> (GIMP_CANVAS_ITEM (guide)); return true;
  });
}
GimpCanvasItem *gimp_canvas_perspective_guide_new (GimpDisplayShell *shell)
{
  return GIMP_CANVAS_ITEM (g_object_new (GIMP_TYPE_CANVAS_PERSPECTIVE_GUIDE, "shell", shell, nullptr));
}
