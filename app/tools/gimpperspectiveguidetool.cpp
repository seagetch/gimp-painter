/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gegl.h>
#include <gtk/gtk.h>
extern "C" {
#include "libgimpbase/gimpbase.h"
#include "libgimpmath/gimpmath.h"
#include "libgimpwidgets/gimpwidgets.h"
#include "tools-types.h"
#include "core/gimpimage.h"
#include "core/gimpimage-perspective-guide.h"
#include "core/gimptooloptions.h"
#include "display/gimpdisplay.h"
#include "gimpperspectiveguidetool.h"
#include "gimptoolcontrol.h"
#include "gimptooloptions-gui.h"
#include "gimp-intl.h"
}
#include "core/gimpperspectiveguide.hpp"
#include "painter/binding-store.hpp"
#include "painter/gimp-painter-binding.h"
#include <cmath>
using namespace GimpPainter;
G_DEFINE_TYPE (GimpPerspectiveGuideTool, gimp_perspective_guide_tool, GIMP_TYPE_DRAW_TOOL)
namespace GimpPainter {
template<> struct TypeTraits<GimpPerspectiveGuideTool>
{ static GType type () noexcept { return GIMP_TYPE_PERSPECTIVE_GUIDE_TOOL; } };
}
namespace {
enum class Operation { Move, Add, Remove };
struct ToolImpl {
  explicit ToolImpl (GimpTool *tool) : tool (tool) {}
  GimpTool *tool;
  WeakRef<GObject> image;
  ObjectRef<GimpPerspectiveGuide> guide, before;
  Operation operation = Operation::Move;
  gint target = -1;
  bool editing = false, changed = false;
  std::uint64_t generation = 0;
  void close () noexcept {
    ++generation; target = -1; editing = changed = false;
    auto old_image = std::move (image);
    auto old_guide = std::move (guide);
    auto old_before = std::move (before);
  }
  bool current (std::uint64_t expected, GObject *owner,
                GimpPerspectiveGuide *model) const {
    return editing && generation == expected && image.lock ().get () == owner &&
      gimp_image_get_perspective_guide (GIMP_IMAGE (owner)) == model;
  }
  bool finish (bool cancel) {
    auto owner = image.lock ();
    auto edited = guide;
    auto previous = std::move (before);
    const bool apply = editing && changed && owner &&
      gimp_image_get_perspective_guide (GIMP_IMAGE (owner.get ())) == edited.get ();
    const auto token = ++generation;
    /* Publish completion before image/Undo notifications can reenter HALT. */
    target = -1; editing = changed = false;
    if (gimp_tool_control_is_active (tool->control)) gimp_tool_control_halt (tool->control);
    if (apply) {
      if (cancel) gimp_image_set_perspective_guide (GIMP_IMAGE (owner.get ()), previous.get ());
      else gimp_image_perspective_guide_push_undo (GIMP_IMAGE (owner.get ()), previous.get (), _("Perspective Guide"));
      if (generation != token) return false;
      gimp_image_flush (GIMP_IMAGE (owner.get ()));
      if (generation != token) return false;
    }
    if (owner) guide = ObjectRef<GimpPerspectiveGuide>::retain (gimp_image_get_perspective_guide (GIMP_IMAGE (owner.get ())));
    /* Last-unref callbacks also invalidate the result seen by attach(). */
    edited.reset (); previous.reset (); owner.reset ();
    return generation == token;
  }
  bool attach (GimpDisplay *display) {
    auto *draw = GIMP_DRAW_TOOL (tool);
    auto owner = image.lock ();
    auto current_image = ObjectRef<GObject>::retain (G_OBJECT (gimp_display_get_image (display)));
    if (draw->display != display || owner.get () != current_image.get ()) {
      if (!finish (true)) return false;
      const auto token = generation;
      if (gimp_draw_tool_is_active (draw)) gimp_draw_tool_stop (draw);
      if (generation != token) return false;
      image = WeakRef<GObject> (current_image);
      target = -1;
    }
    auto *current = current_image ? GIMP_IMAGE (current_image.get ()) : nullptr;
    const auto token = generation;
    guide = ObjectRef<GimpPerspectiveGuide>::retain (current ? gimp_image_get_perspective_guide (current) : nullptr);
    if (generation != token) return false;
    tool->display = display;
    if (!gimp_draw_tool_is_active (draw)) gimp_draw_tool_start (draw, display);
    owner.reset (); current_image.reset ();
    return generation == token && gimp_display_get_image (display) == current &&
      (!current || gimp_image_get_perspective_guide (current) == guide.get ());
  }
  gint hit (GimpDisplay *display, double x, double y) {
    if (!guide) return -1;
    GimpPerspectiveGuideState state {};
    gimp_perspective_guide_get_state (guide.get (), &state);
    gint nearest = -1; double minimum = 0;
    /* Legacy nearest selection in image space, with a display-space handle test. */
    for (int i = 0; i < state.n_points; ++i) {
      double dx = x-state.points[i].x, dy = y-state.points[i].y, distance = dx*dx+dy*dy;
      if (nearest < 0 || distance < minimum) { nearest = i; minimum = distance; }
    }
    return nearest >= 0 && gimp_draw_tool_on_handle (GIMP_DRAW_TOOL (tool), display, x, y,
      GIMP_HANDLE_CIRCLE, state.points[nearest].x, state.points[nearest].y,
      GIMP_TOOL_HANDLE_SIZE_CIRCLE, GIMP_TOOL_HANDLE_SIZE_CIRCLE, GIMP_HANDLE_ANCHOR_CENTER) ? nearest : -1;
  }
};
struct ToolSlot : SlotSpec<GimpPerspectiveGuideTool, ToolImpl> {};
enum { PROP_0, PROP_GUIDE };
void set_property (GObject *object, guint prop, const GValue *value, GParamSpec *pspec)
{
  if (pspec->owner_type != GIMP_TYPE_PERSPECTIVE_GUIDE_TOOL) {
    G_OBJECT_CLASS (gimp_perspective_guide_tool_parent_class)->set_property (object, prop, value, pspec);
    return;
  }
  property_boundary (object, pspec, "set", [&] {
    auto model = ObjectRef<GimpPerspectiveGuide>::retain (
      static_cast<GimpPerspectiveGuide *> (g_value_get_object (value)));
    auto& binding = BindingStore::require (object);
    auto set = [&] (ToolImpl& impl) { impl.guide = model; };
    if (binding.state () == BindingStore::State::constructing) binding.initialize<ToolSlot> (set);
    else binding.with<ToolSlot> ([&] (ToolImpl& impl) { if (impl.finish (true)) set (impl); });
  });
}
void get_property (GObject *object, guint prop, GValue *value, GParamSpec *pspec)
{
  if (pspec->owner_type != GIMP_TYPE_PERSPECTIVE_GUIDE_TOOL) {
    G_OBJECT_CLASS (gimp_perspective_guide_tool_parent_class)->get_property (object, prop, value, pspec);
    return;
  }
  property_boundary (object, pspec, "get", [&] {
    BindingStore::require (object).read<ToolSlot> ([&] (const ToolImpl& impl) { g_value_set_object (value, impl.guide.get ()); });
  });
}
template<class F> void with (GimpTool *tool, F function)
{
  boundary_void (nullptr, [&] {
    auto *binding = BindingStore::find (G_OBJECT (tool));
    /* GimpTool's parent dispose dispatches HALT after our slot has closed. */
    if (binding && binding->state () == BindingStore::State::active)
      binding->with<ToolSlot> (function);
  });
}
struct DrawPause {
  GimpDrawTool *draw;
  explicit DrawPause (GimpTool *tool) : draw (GIMP_DRAW_TOOL (tool)) { gimp_draw_tool_pause (draw); }
  ~DrawPause () { gimp_draw_tool_resume (draw); }
};
void constructed (GObject *object)
{
  G_OBJECT_CLASS (gimp_perspective_guide_tool_parent_class)->constructed (object);
  boundary_void (nullptr, [&] { BindingStore::require (object).activate (); });
}
void dispose (GObject *object)
{
  with (GIMP_TOOL (object), [] (ToolImpl& impl) { impl.finish (true); });
  gimp_painter_binding_close (object, nullptr);
  G_OBJECT_CLASS (gimp_perspective_guide_tool_parent_class)->dispose (object);
}
void control (GimpTool *tool, GimpToolAction action, GimpDisplay *display)
{
  if (action == GIMP_TOOL_ACTION_HALT) with (tool, [&] (ToolImpl& impl) {
    impl.finish (true); impl.close (); impl.operation = Operation::Move;
    if (gimp_draw_tool_is_active (GIMP_DRAW_TOOL (tool))) gimp_draw_tool_stop (GIMP_DRAW_TOOL (tool));
  });
  GIMP_TOOL_CLASS (gimp_perspective_guide_tool_parent_class)->control (tool, action, display);
}
void press (GimpTool *tool, const GimpCoords *coords, guint32, GdkModifierType,
            GimpButtonPressType, GimpDisplay *display)
{
  with (tool, [&] (ToolImpl& impl) {
    if (!std::isfinite (coords->x) || !std::isfinite (coords->y)) return;
    DrawPause pause (tool); if (!impl.attach (display)) return;
    auto owner = impl.image.lock (); if (!owner) return;
    impl.before = ObjectRef<GimpPerspectiveGuide>::adopt (gimp_perspective_guide_duplicate (impl.guide.get ()));
    impl.editing = true; impl.changed = false; impl.target = -1;
    const auto token = ++impl.generation;
    gimp_tool_control_activate (tool->control);
    if (impl.operation == Operation::Add) {
      if (!impl.guide) {
        auto added = ObjectRef<GimpPerspectiveGuide>::adopt (gimp_perspective_guide_new (0));
        if (!added || gimp_perspective_guide_add_vanish_points (added.get (), coords->x, coords->y) < 0) return;
        impl.guide = added;
        impl.changed = true;
        gimp_image_set_perspective_guide (GIMP_IMAGE (owner.get ()), added.get ());
      } else if (gimp_perspective_guide_get_vanish_point_length (impl.guide.get ()) < 3) {
        auto model = impl.guide;
        const bool was_changed = impl.changed;
        impl.changed = true;
        const bool added = gimp_perspective_guide_add_vanish_points (model.get (), coords->x, coords->y) >= 0;
        if (impl.current (token, owner.get (), model.get ()) && !added) impl.changed = was_changed;
      }
    } else {
      impl.target = impl.hit (display, coords->x, coords->y);
      if (impl.target >= 0 && impl.operation == Operation::Remove) {
        auto model = impl.guide;
        const gint target = impl.target;
        impl.target = -1; impl.changed = true;
        const bool removed = gimp_perspective_guide_remove_vanish_points (model.get (), target);
        if (!impl.current (token, owner.get (), model.get ())) return;
        impl.changed = removed;
        if (removed && !gimp_perspective_guide_get_vanish_point_length (model.get ())) {
          impl.guide.reset ();
          gimp_image_set_perspective_guide (GIMP_IMAGE (owner.get ()), nullptr);
        }
      }
    }
  });
}
void release (GimpTool *tool, const GimpCoords *, guint32, GdkModifierType, GimpButtonReleaseType type, GimpDisplay *)
{ with (tool, [&] (ToolImpl& impl) { DrawPause pause (tool); impl.finish (type == GIMP_BUTTON_RELEASE_CANCEL); }); }
void motion (GimpTool *tool, const GimpCoords *coords, guint32, GdkModifierType, GimpDisplay *display)
{
  with (tool, [&] (ToolImpl& impl) {
    if (!impl.editing || impl.target < 0 || !impl.guide || tool->display != display) return;
    auto owner = impl.image.lock ();
    if (!owner || gimp_image_get_perspective_guide (GIMP_IMAGE (owner.get ())) != impl.guide.get ()) {
      impl.editing = impl.changed = false; impl.target = -1; return;
    }
    double x, y; gimp_perspective_guide_get_vanish_points (impl.guide.get (), impl.target, &x, &y);
    if (x == coords->x && y == coords->y) return;
    if (!std::isfinite (coords->x) || !std::isfinite (coords->y)) return;
    DrawPause pause (tool);
    auto model = impl.guide;
    const auto token = impl.generation;
    const bool was_changed = impl.changed;
    impl.changed = true;
    const bool moved = gimp_perspective_guide_set_vanish_points (model.get (), impl.target, coords->x, coords->y);
    if (impl.current (token, owner.get (), model.get ()) && !moved) impl.changed = was_changed;
  });
}
void modifier (GimpTool *tool, GdkModifierType key, gboolean pressed, GdkModifierType, GimpDisplay *)
{
  with (tool, [&] (ToolImpl& impl) {
    if (pressed && impl.operation == Operation::Move) {
      if (key == GDK_SHIFT_MASK) impl.operation = Operation::Add;
      else if (key == GDK_CONTROL_MASK) impl.operation = Operation::Remove;
    } else if (!pressed && ((key == GDK_SHIFT_MASK && impl.operation == Operation::Add) ||
                            (key == GDK_CONTROL_MASK && impl.operation == Operation::Remove))) impl.operation = Operation::Move;
  });
}
void oper_update (GimpTool *tool, const GimpCoords *coords, GdkModifierType state,
                  gboolean proximity, GimpDisplay *display)
{
  with (tool, [&] (ToolImpl& impl) {
    if (!impl.editing) { DrawPause pause (tool); if (!impl.attach (display)) return; }
    GIMP_TOOL_CLASS (gimp_perspective_guide_tool_parent_class)->oper_update (tool, coords, state, proximity, display);
  });
}
void cursor_update (GimpTool *tool, const GimpCoords *coords, GdkModifierType state, GimpDisplay *display)
{
  with (tool, [&] (ToolImpl& impl) {
    gimp_tool_control_set_tool_cursor (tool->control, impl.hit (display, coords->x, coords->y) >= 0 ? GIMP_TOOL_CURSOR_HAND : GIMP_TOOL_CURSOR_PATHS);
    gimp_tool_control_set_cursor_modifier (tool->control, impl.operation == Operation::Add ? GIMP_CURSOR_MODIFIER_PLUS :
      impl.operation == Operation::Remove ? GIMP_CURSOR_MODIFIER_MINUS : GIMP_CURSOR_MODIFIER_MOVE);
  });
  GIMP_TOOL_CLASS (gimp_perspective_guide_tool_parent_class)->cursor_update (tool, coords, state, display);
}
gboolean key_press (GimpTool *tool, GdkEventKey *event, GimpDisplay *)
{
  if (event->keyval != GDK_KEY_Escape) return FALSE;
  with (tool, [&] (ToolImpl& impl) { DrawPause pause (tool); impl.finish (true); }); return TRUE;
}
void draw (GimpDrawTool *draw_tool)
{
  with (GIMP_TOOL (draw_tool), [&] (ToolImpl& impl) {
    if (!impl.guide) return;
    GimpPerspectiveGuideState state {}; gimp_perspective_guide_get_state (impl.guide.get (), &state);
    for (int i = 0; i < state.n_points; ++i)
      gimp_draw_tool_add_handle (draw_tool, impl.target == i ? GIMP_HANDLE_CIRCLE : GIMP_HANDLE_FILLED_CIRCLE,
        state.points[i].x, state.points[i].y, GIMP_TOOL_HANDLE_SIZE_CIRCLE, GIMP_TOOL_HANDLE_SIZE_CIRCLE, GIMP_HANDLE_ANCHOR_CENTER);
    if (state.n_points >= 2) gimp_draw_tool_add_line (draw_tool, state.points[0].x, state.points[0].y, state.points[1].x, state.points[1].y);
  });
}
}
static void gimp_perspective_guide_tool_class_init (GimpPerspectiveGuideToolClass *klass)
{
  G_OBJECT_CLASS (klass)->constructed = constructed; G_OBJECT_CLASS (klass)->dispose = dispose;
  G_OBJECT_CLASS (klass)->set_property = set_property;
  G_OBJECT_CLASS (klass)->get_property = get_property;
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_GUIDE,
    g_param_spec_object ("guide", nullptr, nullptr, GIMP_TYPE_PERSPECTIVE_GUIDE,
      GParamFlags (G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS)));
  auto *tool = GIMP_TOOL_CLASS (klass);
  tool->control = control; tool->button_press = press; tool->button_release = release; tool->motion = motion;
  tool->modifier_key = modifier; tool->oper_update = oper_update; tool->cursor_update = cursor_update; tool->key_press = key_press;
  GIMP_DRAW_TOOL_CLASS (klass)->draw = draw;
}
static void gimp_perspective_guide_tool_init (GimpPerspectiveGuideTool *guide)
{
  auto *tool = GIMP_TOOL (guide);
  guide->binding_failed = !boundary<bool> (nullptr, false, [&] {
    BindingStore::ensure (G_OBJECT (guide)).emplace<ToolSlot> (tool); return true;
  });
  tool->disable_lazy_snap = TRUE;
  gimp_tool_control_set_handle_empty_image (tool->control, TRUE);
  gimp_tool_control_set_snap_to (tool->control, FALSE);
  gimp_tool_control_set_precision (tool->control, GIMP_CURSOR_PRECISION_SUBPIXEL);
  gimp_tool_control_set_active_modifiers (tool->control, GIMP_TOOL_ACTIVE_MODIFIERS_SEPARATE);
  gimp_draw_tool_set_default_status (GIMP_DRAW_TOOL (tool), _("Drag a vanishing point; Shift-click to add, Ctrl-click to remove"));
}
void gimp_perspective_guide_tool_register (GimpToolRegisterCallback callback, gpointer data)
{
  callback (GIMP_TYPE_PERSPECTIVE_GUIDE_TOOL, GIMP_TYPE_TOOL_OPTIONS, gimp_tool_options_gui, GimpContextPropMask (0),
    "gimp-perspective-guide-tool", _("Perspective Guide"), _("Perspective Guide: Edit vanishing points"),
    N_("_Perspective Guide"), "g", nullptr, "gimp-perspective-guide", GIMP_ICON_TOOL_PERSPECTIVE, data);
}
