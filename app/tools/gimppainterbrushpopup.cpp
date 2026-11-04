/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gegl.h>
#include <gtk/gtk.h>
extern "C" {
#include "libgimpbase/gimpbase.h"
#include "libgimpwidgets/gimpwidgets.h"
#include "core/core-types.h"
#include "widgets/widgets-types.h"
#include "core/gimp.h"
#include "core/gimpbrushgenerated.h"
#include "core/gimpcontext.h"
#include "core/gimpdatafactory.h"
#include "paint/gimppaintoptions.h"
#include "widgets/gimpcontainerbox.h"
#include "widgets/gimpcontainericonview.h"
#include "widgets/gimpcontainerview.h"
#include "widgets/gimpeditor.h"
#include "widgets/gimppropwidgets.h"
#include "widgets/gimpview.h"
#include "gimppainterbrushpopup.h"
#include "gimppaintertogglepopup.h"
#include "gimp-intl.h"
}
#include "painter/binding-store.hpp"
#include "painter/connection.hpp"
#include <array>
#include <memory>
#include <vector>
using namespace GimpPainter;
namespace GimpPainter {
template<> struct TypeTraits<GtkWidget> { static GType type () noexcept { return GTK_TYPE_WIDGET; } };
}
namespace {
using Ref = ObjectRef<GObject>;
enum Field { Shape, Radius, Spikes, Hardness, Aspect, Angle, Spacing, FieldCount };
struct BrushView;
struct Slot : SlotSpec<GtkWidget, BrushView> {};
struct BrushView {
  GtkWidget *owner;
  WeakRef<GObject> context;
  Ref brush, preview, selector;
  std::array<Ref, FieldCount> controls;
  Connection context_changed, brush_changed, brush_named, destroyed;
  std::vector<Connection> control_connections;
  guint64 epoch = 0, sync_revision = 0;
  unsigned synchronizing = 0;
  bool closed = false;
  BrushView (GtkWidget *root, GimpContext *config, GtkWidget *view, GtkWidget *grid,
             const std::array<Ref, FieldCount>& widgets)
    : owner (root), context (Ref::retain (G_OBJECT (config))),
      preview (Ref::retain (G_OBJECT (view))), selector (Ref::retain (G_OBJECT (grid))),
      controls (widgets) {}
  bool current (guint64 revision, GObject *target) const;
  void retarget ();
  void sync (guint64 revision);
  void change (Field field, guint64 revision, GObject *target, GtkWidget *emitter);
  void close () noexcept;
};
struct Hook {
  WeakRef<GObject> owner, target;
  guint64 epoch;
  Field field;
  Hook (BrushView& view, Field value = Shape)
    : owner (Ref::retain (G_OBJECT (view.owner))), target (view.brush),
      epoch (view.epoch), field (value) {}
};
void free_hook (gpointer data, GClosure*) { delete static_cast<Hook*> (data); }
template<class F> void dispatch (gpointer data, F&& function) noexcept {
  try {
    auto *hook = static_cast<Hook*> (data);
    auto owner = hook->owner.lock (), target = hook->target.lock ();
    const auto revision = hook->epoch;
    const auto field = hook->field;
    if (!owner) return;
    auto *store = BindingStore::find (owner.get ());
    if (store && store->state () == BindingStore::State::active)
      store->with<Slot> ([&] (BrushView& view) {
        if (!view.closed) function (view, revision, target.get (), field);
      });
  } catch (const std::exception& e) { g_warning ("Painter brush popup: %s", e.what ()); }
}
void selection_changed (GimpContext*, GimpBrush*, gpointer data) {
  dispatch (data, [] (BrushView& view, guint64, GObject*, Field) { view.retarget (); });
}
void resource_changed (GObject *object, GParamSpec*, gpointer data) {
  dispatch (data, [object] (BrushView& view, guint64 revision, GObject *target, Field) {
    if (target == object && view.current (revision, target)) view.sync (revision);
  });
}
void resource_named (GimpObject *object, gpointer data) {
  dispatch (data, [object] (BrushView& view, guint64 revision, GObject *target, Field) {
    if (target == G_OBJECT (object) && view.current (revision, target)) view.sync (revision);
  });
}
void control_changed (GtkWidget *emitter, gpointer data) {
  dispatch (data, [emitter] (BrushView& view, guint64 revision, GObject *target, Field field) {
    view.change (field, revision, target, emitter);
  });
}
void shape_changed (GtkToggleButton *button, gpointer data) {
  if (gtk_toggle_button_get_active (button)) control_changed (GTK_WIDGET (button), data);
}
void value_changed (GtkSpinButton *button, gpointer data) { control_changed (GTK_WIDGET (button), data); }
std::vector<Ref> shape_buttons (GObject *box) {
  std::vector<Ref> result;
  GList *children = gtk_container_get_children (GTK_CONTAINER (box));
  for (auto *item = children; item; item = item->next)
    result.push_back (Ref::retain (G_OBJECT (item->data)));
  g_list_free (children);
  return result;
}
void view_destroyed (GtkWidget *owner, gpointer) {
  try { if (auto *store = BindingStore::find (G_OBJECT (owner))) store->close (); }
  catch (const std::exception& e) { g_warning ("Painter brush popup close: %s", e.what ()); }
}
Connection connect (BrushView& view, GObject *emitter, const char *signal,
                    GCallback callback, Field field = Shape) {
  auto hook = std::unique_ptr<Hook> (new Hook (view, field));
  auto result = Connection::connect (Ref::retain (emitter), signal,
                                      callback, hook.get (), free_hook);
  hook.release ();
  return result;
}
bool BrushView::current (guint64 revision, GObject *target) const {
  if (closed || epoch != revision || brush.get () != target) return false;
  auto config = context.lock ();
  return config && G_OBJECT (gimp_context_get_brush (GIMP_CONTEXT (config.get ()))) == target;
}
void BrushView::retarget () {
  if (closed) return;
  const auto revision = ++epoch;
  // Detach the actual old emitter before releasing it, never the new brush.
  brush_changed.close ();
  brush_named.close ();
  control_connections.clear ();
  brush.reset ();
  auto config = context.lock ();
  if (!config || closed || epoch != revision) return;
  brush = Ref::retain (G_OBJECT (gimp_context_get_brush (GIMP_CONTEXT (config.get ()) )));
  if (brush) {
    brush_changed = connect (*this, brush.get (), "notify", G_CALLBACK (resource_changed));
    // Generated-brush dirty regenerates the mask after property notify, then
    // emits name-changed. Refresh the native description's dimensions then too.
    brush_named = connect (*this, brush.get (), "name-changed", G_CALLBACK (resource_named));
  }
  sync (revision);
  if (!current (revision, brush.get ())) return;
  for (auto& button : shape_buttons (controls[Shape].get ()))
    control_connections.push_back (connect (*this, button.get (), "toggled", G_CALLBACK (shape_changed), Shape));
  for (unsigned i = Radius; i < FieldCount; ++i)
    control_connections.push_back (connect (*this, controls[i].get (),
      "value-changed", G_CALLBACK (value_changed), Field (i)));
}
void BrushView::sync (guint64 revision) {
  auto target = brush, view = preview, grid = selector;
  auto widgets = controls;
  if (!current (revision, target.get ())) return;
  const auto refresh = ++sync_revision;
  auto fresh = [&] { return sync_revision == refresh && current (revision, target.get ()); };
  ++synchronizing;
  struct Guard { unsigned& depth; ~Guard () { --depth; } } guard {synchronizing};
  std::array<double, FieldCount> values {{GIMP_BRUSH_GENERATED_CIRCLE, 0, 2, 0, 1, 0, 0}};
  const bool generated = target && GIMP_IS_BRUSH_GENERATED (target.get ());
  const bool editable = generated && gimp_data_is_writable (GIMP_DATA (target.get ()));
  if (target) values[Spacing] = gimp_brush_get_spacing (GIMP_BRUSH (target.get ()));
  if (generated) {
    auto *b = GIMP_BRUSH_GENERATED (target.get ());
    values[Shape] = gimp_brush_generated_get_shape (b);
    values[Radius] = gimp_brush_generated_get_radius (b);
    values[Spikes] = gimp_brush_generated_get_spikes (b);
    values[Hardness] = gimp_brush_generated_get_hardness (b);
    values[Aspect] = gimp_brush_generated_get_aspect_ratio (b);
    values[Angle] = gimp_brush_generated_get_angle (b);
  }
  gimp_view_set_viewable (GIMP_VIEW (view.get ()), target ? GIMP_VIEWABLE (target.get ()) : nullptr);
  if (!fresh ()) return;
  // Raw icon views synchronize selection, but unlike GimpContainerEditor do
  // not maintain their visible name label. Keep this live canonical title in
  // our existing scoped controller, without an autosaving data editor.
  auto *description = target ? gimp_viewable_get_description (GIMP_VIEWABLE (target.get ()), nullptr) : nullptr;
  gimp_editor_set_name (GIMP_EDITOR (grid.get ()), description);
  g_free (description);
  for (unsigned i = 0; i < FieldCount; ++i) {
    if (!fresh ()) return;
    // Legacy resource spacing is live even for disk-read-only brushes. Only
    // generated geometry was in the writable-only Brush Options frame.
    gtk_widget_set_sensitive (GTK_WIDGET (widgets[i].get ()), i == Spacing ? bool (target) : editable);
    if (!fresh ()) return;
    if (i == Shape) {
      auto buttons = shape_buttons (widgets[i].get ());
      if (!buttons.empty ()) gimp_int_radio_group_set_active (GTK_RADIO_BUTTON (buttons[0].get ()), gint (values[i]));
    } else gtk_spin_button_set_value (GTK_SPIN_BUTTON (widgets[i].get ()), values[i]);
  }
}
void BrushView::change (Field field, guint64 revision, GObject *observed, GtkWidget *emitter) {
  if (synchronizing || !observed || !current (revision, observed)) return;
  auto target = brush, widget = controls[field];
  if (field == Spacing) {
    // Preserve the native setter's spacing-changed/notify and dirty behavior;
    // unlike generated geometry it does not itself mark the resource dirty.
    const auto value = gint (gtk_spin_button_get_value (GTK_SPIN_BUTTON (widget.get ())));
    if (value != gimp_brush_get_spacing (GIMP_BRUSH (target.get ())))
      gimp_brush_set_spacing (GIMP_BRUSH (target.get ()), value);
    return;
  }
  if (!GIMP_IS_BRUSH_GENERATED (target.get ()) || !gimp_data_is_writable (GIMP_DATA (target.get ()))) return;
  auto *generated = GIMP_BRUSH_GENERATED (target.get ());
  if (field == Shape) {
    const auto value = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (emitter), "gimp-item-data"));
    if (value >= GIMP_BRUSH_GENERATED_CIRCLE && value <= GIMP_BRUSH_GENERATED_DIAMOND)
      gimp_brush_generated_set_shape (generated, GimpBrushGeneratedShape (value));
    return;
  }
  const auto value = gtk_spin_button_get_value (GTK_SPIN_BUTTON (widget.get ()));
  switch (field) {
    case Radius: gimp_brush_generated_set_radius (generated, value); break;
    case Spikes: gimp_brush_generated_set_spikes (generated, gint (value)); break;
    case Hardness: gimp_brush_generated_set_hardness (generated, value); break;
    case Aspect: gimp_brush_generated_set_aspect_ratio (generated, value); break;
    case Angle: gimp_brush_generated_set_angle (generated, value); break;
    default: break;
  }
  // Setters can synchronously switch context or destroy the toolbar. Nothing
  // after them borrows the former controller target or its controls.
}
void BrushView::close () noexcept {
  if (closed) return;
  closed = true;
  ++epoch;
  context_changed.close ();
  brush_changed.close ();
  brush_named.close ();
  control_connections.clear ();
  destroyed.close ();
  brush.reset ();
  preview.reset ();
  selector.reset ();
  for (auto& widget : controls) widget.reset ();
  context.reset ();
}
/* Construction signals can destroy an owner synchronously and then return
 * after GtkWidget's transient in-destruction flag has cleared. Keep an explicit
 * store state and leases for all partially assembled children until admission. */
struct Construction {
  BindingStore& store;
  std::vector<Ref> widgets;
  void check () {
    if (store.state () != BindingStore::State::constructing)
      throw Error (GIMP_PAINTER_ERROR_CLOSED, "Brush popup destroyed during construction");
  }
  GtkWidget *keep (GtkWidget *widget) {
    widgets.push_back (Ref::sink (G_OBJECT (widget)));
    check ();
    return widget;
  }
  void pack (GtkWidget *box, GtkWidget *child) {
    check ();
    gtk_box_pack_start (GTK_BOX (box), child, FALSE, FALSE, 0);
    check ();
  }
  GtkWidget *section (GtkWidget *box, const char *title) {
    auto *frame = keep (gimp_frame_new (title));
    auto *content = keep (gtk_box_new (GTK_ORIENTATION_VERTICAL, 3));
    gtk_container_add (GTK_CONTAINER (frame), content);
    pack (box, frame);
    return content;
  }
};
GtkWidget *scale (const char *label, double lower, double upper, double step, gint digits) {
  auto *adjustment = gtk_adjustment_new (lower, lower, upper, step, step * 10, 0);
  auto *widget = gimp_spin_scale_new (adjustment, label, digits);
  gtk_entry_set_width_chars (GTK_ENTRY (widget), 5);
  gimp_spin_scale_set_constrain_drag (GIMP_SPIN_SCALE (widget), TRUE);
  return widget;
}
GtkWidget *create_view (GObject *model) {
  return boundary<GtkWidget*> (nullptr, nullptr, [&] {
    if (!GIMP_IS_PAINT_OPTIONS (model)) return static_cast<GtkWidget*> (nullptr);
    auto *context = GIMP_CONTEXT (model);
    auto root = Ref::sink (G_OBJECT (gtk_scrolled_window_new (nullptr, nullptr)));
    auto *scroll = GTK_WIDGET (root.get ());
    auto& store = BindingStore::ensure (root.get ());
    auto destroy_guard = Connection::connect (root, "destroy", G_CALLBACK (view_destroyed), nullptr, nullptr);
    Construction build {store, {}};
    gtk_widget_set_name (scroll, "painter-brush-popup-body"); build.check ();
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_propagate_natural_height (GTK_SCROLLED_WINDOW (scroll), TRUE);
    gtk_scrolled_window_set_max_content_height (GTK_SCROLLED_WINDOW (scroll), 740);
    auto *columns = build.keep (gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8));
    gtk_container_set_border_width (GTK_CONTAINER (columns), 6);
    build.check ();
    gtk_container_add (GTK_CONTAINER (scroll), columns); build.check ();
    auto *selector = build.keep (gimp_container_icon_view_new (
      gimp_data_factory_get_container (context->gimp->brush_factory), context, GIMP_VIEW_SIZE_SMALL, 1));
    gtk_widget_set_name (selector, "painter-brush-popup-selector");
    gimp_editor_set_show_name (GIMP_EDITOR (selector), TRUE); build.check ();
    gimp_container_view_set_reorderable (GIMP_CONTAINER_VIEW (selector), FALSE);
    gimp_container_box_set_size_request (GIMP_CONTAINER_BOX (selector), 164, 280);
    gtk_widget_set_valign (selector, GTK_ALIGN_FILL);
    build.pack (columns, selector);
    auto *right = build.keep (gtk_box_new (GTK_ORIENTATION_VERTICAL, 6));
    gtk_widget_set_size_request (right, 256, -1);
    build.pack (columns, right);
    auto *tools = build.section (right, _("Tool Options"));
    for (const auto *property : {"brush-size", "brush-aspect-ratio", "brush-angle"}) {
      auto *widget = build.keep (gimp_prop_spin_scale_new (model, property, 0.1, 1.0, 2));
      gtk_entry_set_width_chars (GTK_ENTRY (widget), 5);
      gimp_spin_scale_set_constrain_drag (GIMP_SPIN_SCALE (widget), TRUE);
      build.pack (tools, widget);
    }
    std::array<Ref, FieldCount> controls;
    auto *spacing = build.keep (scale (_("Resource spacing"), 1, 5000, 1, 0));
    gimp_spin_scale_set_scale_limits (GIMP_SPIN_SCALE (spacing), 1, 200);
    build.pack (tools, spacing); controls[Spacing] = Ref::retain (G_OBJECT (spacing));
    auto *generated = build.section (right, _("Brush Options"));
    auto *shape_row = build.keep (gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4));
    auto *shape_label = build.keep (gtk_label_new (_("Shape:")));
    build.pack (shape_row, shape_label);
    auto *shape = build.keep (gimp_enum_icon_box_new (GIMP_TYPE_BRUSH_GENERATED_SHAPE,
      "gimp-shape", GTK_ICON_SIZE_MENU, nullptr, nullptr, nullptr, nullptr));
    for (auto& button : shape_buttons (G_OBJECT (shape))) {
      auto *tooltip = gtk_widget_get_tooltip_text (GTK_WIDGET (button.get ()));
      atk_object_set_name (gtk_widget_get_accessible (GTK_WIDGET (button.get ())), tooltip);
      g_free (tooltip);
    }
    build.pack (shape_row, shape);
    build.pack (generated, shape_row); controls[Shape] = Ref::retain (G_OBJECT (shape));
    struct { Field field; const char *label; double lower, upper, step; int digits; } fields[] = {
      {Radius, _("Radius"), GIMP_BRUSH_GENERATED_MIN_RADIUS, GIMP_BRUSH_GENERATED_MAX_RADIUS, 1, 1},
      {Spikes, _("Spikes"), 2, 20, 1, 0}, {Hardness, _("Hardness"), 0, 1, .01, 2},
      {Aspect, _("Aspect ratio"), 1, 20, .1, 1}, {Angle, _("Angle"), 0, 180, .1, 1}
    };
    for (auto& field : fields) {
      auto *widget = build.keep (scale (field.label, field.lower, field.upper, field.step, field.digits));
      build.pack (generated, widget); controls[field.field] = Ref::retain (G_OBJECT (widget));
    }
    const char *names[] = {"shape", "radius", "spikes", "hardness", "aspect-ratio", "angle", "spacing"};
    for (unsigned i = 0; i < FieldCount; ++i) {
      auto *name = g_strconcat ("painter-brush-resource-", names[i], nullptr);
      gtk_widget_set_name (GTK_WIDGET (controls[i].get ()), name); g_free (name);
    }
    auto *preview_box = build.section (right, _("Preview"));
    auto *preview = build.keep (gimp_view_new_full_by_types (context, GIMP_TYPE_VIEW, GIMP_TYPE_BRUSH,
                                                 256, 256, 0, FALSE, FALSE, TRUE));
    gtk_widget_set_name (preview, "painter-brush-popup-preview");
    build.pack (preview_box, preview);
    build.check ();
    store.emplace<Slot> (scroll, context, preview, selector, controls);
    store.activate ();
    store.with<Slot> ([&] (BrushView& view) {
      view.destroyed = std::move (destroy_guard);
      view.context_changed = connect (view, model, "brush-changed", G_CALLBACK (selection_changed));
      view.retarget ();
    });
    if (store.state () != BindingStore::State::active) return static_cast<GtkWidget*> (nullptr);
    g_object_force_floating (root.release ());
    return scroll;
  });
}
}
extern "C" GtkWidget *
gimp_painter_brush_popup_button_new (GimpPaintOptions *options) {
  return boundary<GtkWidget*> (nullptr, nullptr, [&] {
    if (!GIMP_IS_PAINT_OPTIONS (options)) return static_cast<GtkWidget*> (nullptr);
    auto *button = gimp_painter_resource_popup_button_new (G_OBJECT (options), create_view,
                                                          "painter-brush-thumbnail-popup");
    if (!button) return button;
    auto lease = Ref::sink (G_OBJECT (button));
    auto current = [&] {
      auto *store = BindingStore::find (G_OBJECT (button));
      return store && store->state () == BindingStore::State::active;
    };
    gtk_widget_set_name (button, "painter-paint-brush-thumbnail");
    if (!current ()) return static_cast<GtkWidget*> (nullptr);
    gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
    if (!current ()) return static_cast<GtkWidget*> (nullptr);
    gtk_widget_set_tooltip_text (button, _("Select and edit brush"));
    if (!current ()) return static_cast<GtkWidget*> (nullptr);
    atk_object_set_name (gtk_widget_get_accessible (button), _("Brush"));
    if (!current ()) return static_cast<GtkWidget*> (nullptr);
    // The property view's DND callback borrows options; keep it alive exactly
    // as long as the independent thumbnail, never borrow the dock hierarchy.
    g_object_set_data_full (G_OBJECT (button), "gimp-painter-brush-options",
                            g_object_ref (options), g_object_unref);
    auto *view = gimp_prop_view_new (G_OBJECT (options), "brush", GIMP_CONTEXT (options), GIMP_VIEW_SIZE_SMALL);
    auto icon = Ref::sink (G_OBJECT (view));
    if (!view || !current ()) {
      if (view) gtk_widget_destroy (view);
      return static_cast<GtkWidget*> (nullptr);
    }
    gtk_container_add (GTK_CONTAINER (button), view);
    if (!current ()) return static_cast<GtkWidget*> (nullptr);
    gtk_widget_show (view);
    if (!current ()) return static_cast<GtkWidget*> (nullptr);
    g_object_force_floating (lease.release ());
    return button;
  });
}
