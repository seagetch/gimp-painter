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
#include "gimpcanvasperspectiveguide.hpp"
#include "painter/binding-store.hpp"
#include "painter/connection.hpp"
#include "painter/gimp-painter-binding.h"
#include <cmath>
#include <limits>
using namespace GimpPainter;
G_DEFINE_TYPE (GimpCanvasPerspectiveGuide, gimp_canvas_perspective_guide, GIMP_TYPE_CANVAS_ITEM)
namespace {
enum { PROP_0, PROP_GUIDE, PROP_FOLLOW_IMAGE };
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
  bool follows_image = false;
  WeakRef<GObject> image;
  PerspectiveGuideRef guide;
  GimpPerspectiveGuideState state {};
  Connection shell_connection, display_connection, image_connection, guide_connection;
  void close () noexcept {
    shell_connection.close (); display_connection.close (); image_connection.close (); guide_connection.close ();
    follows_image = false; image.reset (); guide.reset (); state = {};
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
    if (guide) guide.state (&state);
    gimp_canvas_item_end_change (item);
  }
  void replace_guide (PerspectiveGuideRef replacement, bool explicit_assignment) {
    /* Prepare every allocating operation before changing the published model.
     * Keep the outgoing model alive through update observers and disconnect. */
    Connection next_connection;
    if (replacement)
      next_connection = connect (G_OBJECT (replacement.get ()), "changed", G_CALLBACK (guide_changed));
    GimpPerspectiveGuideState next_state {};
    if (replacement) replacement.state (&next_state);
    gimp_canvas_item_begin_change (item);
    if (explicit_assignment) {
      follows_image = false;
      shell_connection.close (); display_connection.close (); image_connection.close (); image.reset ();
    }
    auto old_guide = std::move (guide);
    auto old_connection = std::move (guide_connection);
    guide = std::move (replacement);
    guide_connection = std::move (next_connection);
    state = next_state;
    old_connection.close ();
    gimp_canvas_item_end_change (item);
    /* No writes after update observers or old model finalizers can reenter. */
  }
  void refresh_guide () {
    if (!follows_image) return;
    auto image_ref = image.lock ();
    auto replacement = PerspectiveGuideRef::retain (image_ref ?
      gimp_image_get_perspective_guide (GIMP_IMAGE (image_ref.get ())) : nullptr);
    replace_guide (std::move (replacement), false);
  }
  void refresh_display () {
    if (!follows_image) return;
    auto *shell = gimp_canvas_item_get_shell (item);
    display_connection.close ();
    if (shell->display) display_connection = connect (G_OBJECT (shell->display), "notify::image", G_CALLBACK (display_changed));
    refresh_image ();
  }
  void refresh_image () {
    if (!follows_image) return;
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
  auto *guide = reinterpret_cast<GimpCanvasPerspectiveGuide *> (object);
  if (!guide->binding_failed)
    guide->binding_failed = !boundary<bool> (nullptr, false, [&] {
      auto& binding = BindingStore::require (object);
      binding.initialize<OverlaySlot> ([] (OverlayImpl& impl) {
        auto *shell = gimp_canvas_item_get_shell (impl.item);
        /* items_init precedes the shell's construct-only display property.
         * Callbacks remain gated until every initial connection is installed. */
        if (impl.follows_image) {
          impl.shell_connection = impl.connect (G_OBJECT (shell), "notify::display", G_CALLBACK (shell_changed));
          impl.refresh_display ();
        } else {
          impl.replace_guide (impl.guide, false);
        }
      });
      binding.activate ();
      return true;
    });
  if (guide->binding_failed)
    gimp_painter_binding_close (object, nullptr);
}
void set_property (GObject *object, guint prop, const GValue *value, GParamSpec *pspec)
{
  auto *owner = reinterpret_cast<GimpCanvasPerspectiveGuide *> (object);
  if (owner->binding_failed) return;
  property_boundary (object, pspec, "set", [&] {
    auto& binding = BindingStore::require (object);
    auto set = [&] (OverlayImpl& impl) {
      if (prop == PROP_FOLLOW_IMAGE)
        impl.follows_image = g_value_get_boolean (value);
      else if (prop == PROP_GUIDE) {
        auto replacement = PerspectiveGuideRef::retain (
          static_cast<GimpPerspectiveGuide *> (g_value_get_object (value)));
        if (binding.state () == BindingStore::State::constructing)
          impl.guide = std::move (replacement);
        else
          impl.replace_guide (std::move (replacement), true);
      } else G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop, pspec);
    };
    if (binding.state () == BindingStore::State::constructing) binding.initialize<OverlaySlot> (set);
    else binding.with<OverlaySlot> (set);
  });
}
void get_property (GObject *object, guint prop, GValue *value, GParamSpec *pspec)
{
  if (reinterpret_cast<GimpCanvasPerspectiveGuide *> (object)->binding_failed) return;
  property_boundary (object, pspec, "get", [&] {
    auto& binding = BindingStore::require (object);
    auto get = [&] (const OverlayImpl& impl) {
      if (prop == PROP_GUIDE) g_value_set_object (value, impl.guide.get ());
      else if (prop == PROP_FOLLOW_IMAGE) g_value_set_boolean (value, impl.follows_image);
      else G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop, pspec);
    };
    if (binding.state () == BindingStore::State::constructing) binding.initialize<OverlaySlot> (get);
    else binding.read<OverlaySlot> (get);
  });
}
void dispose (GObject *object)
{ gimp_painter_binding_close (object, nullptr); G_OBJECT_CLASS (gimp_canvas_perspective_guide_parent_class)->dispose (object); }
void draw (GimpCanvasItem *item, cairo_t *cr)
{
  boundary_void (nullptr, [&] { BindingStore::require (G_OBJECT (item)).read<OverlaySlot> ([&] (const OverlayImpl& impl) {
    auto *shell = gimp_canvas_item_get_shell (item);
    if (shell->scale_x <= 0) return;
    /* Preserve the pinned image-coordinate path, including its radial
     * segments. Pre-rounding centers through SCALEX/Y shifts fractional zoom
     * positions; separate circle subpaths also discard the old radial ink.
     * The canvas group already supplies the common outer display matrix. */
    cairo_save (cr);
    cairo_translate (cr, -shell->offset_x, -shell->offset_y);
    cairo_scale (cr, shell->scale_x, shell->scale_y);
    for (int i = 0; i < impl.state.n_points; ++i) {
      const double x = impl.state.points[i].x;
      const double y = impl.state.points[i].y;
      if (impl.state.n_points != 2)
        cairo_move_to (cr, x, y);
      else if (i > 0)
        cairo_line_to (cr, x, y);
      cairo_arc (cr, x, y, 8 / shell->scale_x, 0, 2 * G_PI);
      if (impl.state.n_points == 2)
        cairo_move_to (cr, x, y);
    }
    cairo_restore (cr);
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
  G_OBJECT_CLASS (klass)->set_property = set_property;
  G_OBJECT_CLASS (klass)->get_property = get_property;
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_GUIDE,
    g_param_spec_object ("guide", nullptr, nullptr, GIMP_TYPE_PERSPECTIVE_GUIDE,
      GParamFlags (G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_FOLLOW_IMAGE,
    g_param_spec_boolean ("follow-image", nullptr, nullptr, FALSE,
      GParamFlags (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS)));
  GIMP_CANVAS_ITEM_CLASS (klass)->draw = draw; GIMP_CANVAS_ITEM_CLASS (klass)->get_extents = extents;
}
static void gimp_canvas_perspective_guide_init (GimpCanvasPerspectiveGuide *guide)
{
  guide->binding_failed = !boundary<bool> (nullptr, false, [&] {
    BindingStore::ensure (G_OBJECT (guide)).emplace<OverlaySlot> (GIMP_CANVAS_ITEM (guide)); return true;
  });
}
namespace {
GimpCanvasPerspectiveGuide *create_guide (GimpDisplayShell *shell, GimpPerspectiveGuide *model, bool follow)
{
  auto *guide = static_cast<GimpCanvasPerspectiveGuide *> (
    g_object_new (GIMP_TYPE_CANVAS_PERSPECTIVE_GUIDE, "shell", shell,
                  "guide", model, "follow-image", follow, nullptr));
  if (guide->binding_failed) {
    g_object_unref (guide); // Nonfloating: consume exactly one native reference.
    return nullptr;
  }
  return guide;
}
}
GimpCanvasPerspectiveGuide *gimp_canvas_perspective_guide_new (GimpDisplayShell *shell,
                                                            GimpPerspectiveGuide *guide)
{
  g_return_val_if_fail (GIMP_IS_DISPLAY_SHELL (shell), nullptr);
  g_return_val_if_fail (!guide || GIMP_IS_PERSPECTIVE_GUIDE (guide), nullptr);
  return create_guide (shell, guide, false);
}
GimpCanvasItem *gimp_canvas_perspective_guide_new_for_image (GimpDisplayShell *shell)
{
  g_return_val_if_fail (GIMP_IS_DISPLAY_SHELL (shell), nullptr);
  return GIMP_CANVAS_ITEM (create_guide (shell, nullptr, true));
}
