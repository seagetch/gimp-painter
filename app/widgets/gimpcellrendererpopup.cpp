/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gegl.h>
#include <gtk/gtk.h>
extern "C" {
#include "libgimpbase/gimpbase.h"
#include "libgimpwidgets/gimpwidgets.h"
#include "widgets-types.h"
#include "core/gimpcontext.h"
#include "core/gimpimage.h"
#include "core/gimplayer.h"
#include "core/gimpfilterlayer.h"
#include "core/gimpclonelayer.h"
#include "gimpcontainertreeview.h"
#include "gimpcontainertreestore.h"
#include "gimpcontainerview.h"
#include "gimpitemtreeview.h"
#include "gimplayertreeview.h"
#include "gimplayermodebox.h"
#include "gimpviewrenderer.h"
#include "dialogs/painter-layer-dialog.h"
#include "gimpcellrendererpopup.h"
#include "gimp-intl.h"
}
#include "gimpcellrendererpopup-handle.hpp"
#include "painter/binding-store.hpp"
#include "painter/connection.hpp"
#include "painter/source.hpp"
#include <memory>
#include <vector>
using namespace GimpPainter;
G_DEFINE_TYPE (GimpCellRendererPopup, gimp_cell_renderer_popup, GTK_TYPE_CELL_RENDERER_PIXBUF)
namespace {
struct Popup;
struct PopupSlot : SlotSpec<GimpCellRendererPopup, Popup> {};
struct Popup {
  GimpCellRendererPopup *owner;
  WeakRef<GObject> tree;
  ObjectRef<GObject> popup, target, image;
  std::vector<Connection> permanent, controls;
  Source resync;
  GtkWidget *opacity = nullptr, *mode = nullptr, *alpha = nullptr;
  guint64 epoch = 0, syncing_epoch = 0;
  bool closed = false, active = false;
  explicit Popup (GimpCellRendererPopup *o) : owner (o) {}
  void clear () noexcept {
    /* Invalidate and detach before disconnect/destroy can call back. */
    ++epoch;
    resync.close ();
    auto old_connections = std::move (controls);
    auto old_popup = std::move (popup);
    auto old_target = std::move (target);
    auto old_image = std::move (image);
    opacity = mode = alpha = nullptr;
    old_connections.clear ();
    if (old_popup) gtk_widget_destroy (GTK_WIDGET (old_popup.get ()));
  }
  void close () noexcept {
    if (closed) return;
    closed = true; clear (); permanent.clear (); tree.reset ();
  }
  bool current (guint64 request) const {
    auto view = tree.lock ();
    return !closed && epoch == request && view &&
      !gtk_widget_in_destruction (GTK_WIDGET (view.get ())) && target && image &&
      gimp_item_is_attached (GIMP_ITEM (target.get ())) &&
      gimp_item_get_image (GIMP_ITEM (target.get ())) == GIMP_IMAGE (image.get ()) &&
      gimp_item_tree_view_get_image (GIMP_ITEM_TREE_VIEW (view.get ())) == GIMP_IMAGE (image.get ());
  }
  void refresh ();
  void open (GtkWidget *widget, const gchar *path, const GdkRectangle *area);
};
struct Hook {
  WeakRef<GObject> owner;
  guint64 epoch;
  Hook (Popup& p) : owner (ObjectRef<GObject>::retain (G_OBJECT (p.owner))), epoch (p.epoch) {}
};
template<class F> void use (GObject *object, F&& f) noexcept {
  try {
    auto *store = BindingStore::find (object);
    if (store && store->state () == BindingStore::State::active)
      store->with<PopupSlot> (std::forward<F> (f));
  } catch (...) { g_warning ("Painter tree popup callback failed"); }
}
template<class F> void dispatch (gpointer data, F&& f) noexcept {
  try {
    auto *hook = static_cast<Hook *> (data);
    auto owner = hook->owner.lock ();
    const auto request = hook->epoch;
    if (owner) use (owner.get (), [&] (Popup& p) {
      if (p.current (request)) f (p);
    });
  } catch (...) { g_warning ("Painter tree popup dispatch failed"); }
}
void free_hook (gpointer data, GClosure *) { delete static_cast<Hook *> (data); }
void connect (Popup& p, std::vector<Connection>& list, GObject *emitter,
              const char *signal, GCallback callback, GConnectFlags flags = GConnectFlags (0)) {
  auto hook = std::unique_ptr<Hook> (new Hook (p));
  auto c = Connection::connect (ObjectRef<GObject>::retain (emitter), signal,
                               callback, hook.get (), free_hook, flags);
  hook.release (); list.emplace_back (std::move (c));
}
void owner_destroyed (GtkWidget *, gpointer data) noexcept {
  try {
    auto owner = static_cast<Hook *> (data)->owner.lock ();
    if (owner) if (auto *store = BindingStore::find (owner.get ())) store->close ();
  } catch (...) { g_warning ("Could not close painter tree popup"); }
}
void image_changed (GimpItemTreeView *, GimpImage *, gpointer data) noexcept {
  auto owner = static_cast<Hook *> (data)->owner.lock ();
  if (owner) use (owner.get (), [] (Popup& p) { p.clear (); });
}
void image_changed_after (GimpItemTreeView *, GimpImage *, gpointer data) noexcept {
  auto owner = static_cast<Hook *> (data)->owner.lock ();
  if (owner) use (owner.get (), [] (Popup& p) {
    /* set-image's native class closure runs after the first clear. A destroy
     * observer can reopen the old image before that closure; retire it now,
     * while preserving a newer popup already bound to the actual new image. */
    if (p.popup && !p.current (p.epoch)) p.clear ();
  });
}
void target_removed (GimpItem *, gpointer data) noexcept {
  /* current() deliberately rejects removed objects, so only gate by epoch. */
  auto *hook = static_cast<Hook *> (data);
  auto owner = hook->owner.lock (); const auto request = hook->epoch;
  if (owner) use (owner.get (), [request] (Popup& p) { if (p.epoch == request) p.clear (); });
}
void popup_closed (GtkWidget *, gpointer data) noexcept {
  auto *hook = static_cast<Hook *> (data);
  auto owner = hook->owner.lock (); const auto request = hook->epoch;
  if (owner) use (owner.get (), [request] (Popup& p) { if (p.epoch == request) p.clear (); });
}
void opacity_changed (GtkRange *range, gpointer data) noexcept {
  const auto value = gtk_range_get_value (range) / 100.0;
  dispatch (data, [value] (Popup& p) {
    if (p.syncing_epoch == p.epoch) return;
    auto layer = p.target; auto image = p.image;
    gimp_layer_set_opacity (GIMP_LAYER (layer.get ()), value, TRUE);
    gimp_image_flush (GIMP_IMAGE (image.get ()));
  });
}
void mode_changed (GObject *box, GParamSpec *, gpointer data) noexcept {
  const auto value = gimp_layer_mode_box_get_mode (GIMP_LAYER_MODE_BOX (box));
  dispatch (data, [value] (Popup& p) {
    if (p.syncing_epoch == p.epoch) return;
    auto layer = p.target; auto image = p.image;
    gimp_layer_set_mode (GIMP_LAYER (layer.get ()), value, TRUE);
    gimp_image_flush (GIMP_IMAGE (image.get ()));
  });
}
void alpha_changed (GtkToggleButton *button, gpointer data) noexcept {
  const bool value = gtk_toggle_button_get_active (button);
  dispatch (data, [value] (Popup& p) {
    if (p.syncing_epoch == p.epoch || !gimp_layer_can_lock_alpha (GIMP_LAYER (p.target.get ()))) return;
    auto layer = p.target; auto image = p.image;
    gimp_layer_set_lock_alpha (GIMP_LAYER (layer.get ()), value, TRUE);
    gimp_image_flush (GIMP_IMAGE (image.get ()));
  });
}
void model_changed (GimpLayer *, gpointer data) noexcept {
  dispatch (data, [] (Popup& p) { p.refresh (); });
}
void edit_layer (GtkButton *, gpointer data) noexcept {
  dispatch (data, [] (Popup& p) {
    auto tree = p.tree.lock (); auto layer = p.target; auto image = p.image;
    auto context = ObjectRef<GObject>::retain (G_OBJECT (
      gimp_container_view_get_context (GIMP_CONTAINER_VIEW (tree.get ()))));
    auto request = p.epoch;
    if (!context) return;
    GtkWidget *dialog = GIMP_IS_FILTER_LAYER (layer.get ()) ?
      painter_filter_layer_dialog_new (GIMP_IMAGE (image.get ()), GIMP_LAYER (layer.get ()),
                                      GIMP_CONTEXT (context.get ()), GTK_WIDGET (tree.get ())) :
      painter_clone_layer_dialog_new (GIMP_IMAGE (image.get ()), GIMP_LAYER (layer.get ()),
                                     GIMP_CONTEXT (context.get ()), GTK_WIDGET (tree.get ()));
    if (!dialog) return;
    auto hold = ObjectRef<GObject>::sink (G_OBJECT (dialog));
    if (p.current (request)) {
      p.clear ();
      /* Dismissing the popup can synchronously close the tree or open a newer
       * request. Do not present this now-stale dialog afterward. */
      if (!p.closed && p.epoch == request + 1 &&
          !gtk_widget_in_destruction (GTK_WIDGET (tree.get ())) &&
          gimp_item_tree_view_get_image (GIMP_ITEM_TREE_VIEW (tree.get ())) == GIMP_IMAGE (image.get ()) &&
          gimp_item_is_attached (GIMP_ITEM (layer.get ())) &&
          gimp_item_get_image (GIMP_ITEM (layer.get ())) == GIMP_IMAGE (image.get ()))
        gtk_widget_show (dialog);
      else gtk_widget_destroy (dialog);
    } else gtk_widget_destroy (dialog);
  });
}
void Popup::refresh () {
  if (!current (epoch) || !popup) return;
  if (syncing_epoch == epoch) {
    /* A control observer changed this same model during refresh. Defer one
     * coalesced resync instead of dropping the update or recursively spinning. */
    if (!resync.active ()) {
      auto hook = std::make_shared<Hook> (*this);
      resync = Source::idle (nullptr, G_PRIORITY_DEFAULT_IDLE, [hook] {
        dispatch (hook.get (), [] (Popup& p) { p.resync.close (); p.refresh (); });
        return false;
      });
    }
    return;
  }
  const auto request = epoch;
  auto layer = target;
  auto opacity_hold = ObjectRef<GObject>::retain (G_OBJECT (opacity));
  /* GtkAdjustment emits value-changed before its final notify. A callback
   * can destroy the old range and drop its adjustment even while the range
   * itself is retained, so lease the actual native emitter through return. */
  auto adjustment_hold = ObjectRef<GObject>::retain (G_OBJECT (
    gtk_range_get_adjustment (GTK_RANGE (opacity_hold.get ()))));
  auto mode_hold = ObjectRef<GObject>::retain (G_OBJECT (mode));
  auto alpha_hold = ObjectRef<GObject>::retain (G_OBJECT (alpha));
  syncing_epoch = request;
  gtk_range_set_value (GTK_RANGE (opacity_hold.get ()), 100 * gimp_layer_get_opacity (GIMP_LAYER (layer.get ())));
  if (current (request)) gimp_layer_mode_box_set_mode (GIMP_LAYER_MODE_BOX (mode_hold.get ()), gimp_layer_get_mode (GIMP_LAYER (layer.get ())));
  if (current (request)) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (alpha_hold.get ()), gimp_layer_get_lock_alpha (GIMP_LAYER (layer.get ())));
  if (syncing_epoch == request) syncing_epoch = 0;
}
void Popup::open (GtkWidget *widget, const gchar *path_str, const GdkRectangle *area) {
  auto view = tree.lock ();
  if (closed || !view || gtk_widget_in_destruction (GTK_WIDGET (view.get ())) ||
      widget != GTK_WIDGET (GIMP_CONTAINER_TREE_VIEW (view.get ())->view)) return;
  GtkTreeModel *model = gtk_tree_view_get_model (GTK_TREE_VIEW (widget));
  if (!GIMP_IS_CONTAINER_TREE_STORE (model)) return;
  GtkTreeIter iter;
  if (!gtk_tree_model_get_iter_from_string (model, &iter, path_str)) return;
  auto renderer = ObjectRef<GObject>::adopt (G_OBJECT (
    gimp_container_tree_store_get_renderer (GIMP_CONTAINER_TREE_STORE (model), &iter)));
  if (!renderer || !GIMP_IS_LAYER (GIMP_VIEW_RENDERER (renderer.get ())->viewable)) return;
  auto clicked = ObjectRef<GObject>::retain (G_OBJECT (GIMP_VIEW_RENDERER (renderer.get ())->viewable));
  auto clicked_image = ObjectRef<GObject>::retain (G_OBJECT (gimp_item_get_image (GIMP_ITEM (clicked.get ()))));
  if (!clicked_image) return;
  const auto request = epoch + 1;
  clear ();
  /* Destroy handlers may synchronously reopen a newer request. */
  if (closed || epoch != request) return;
  target = std::move (clicked); image = std::move (clicked_image);
  if (!current (request)) { clear (); return; }
  const bool native_filter = GIMP_IS_FILTER_LAYER (target.get ());
  auto layer_hold = target;
  auto image_hold = image;
  auto context_hold = ObjectRef<GObject>::retain (G_OBJECT (
    gimp_container_view_get_context (GIMP_CONTAINER_VIEW (view.get ()))));
  auto box_hold = ObjectRef<GObject>::sink (G_OBJECT (gtk_box_new (GTK_ORIENTATION_VERTICAL, 6)));
  GtkWidget *box = GTK_WIDGET (box_hold.get ());
  gtk_container_set_border_width (GTK_CONTAINER (box), 8);
  GtkWidget *created = native_filter ?
    painter_filter_layer_popup_new (GIMP_IMAGE (image_hold.get ()), GIMP_LAYER (layer_hold.get ()),
                                    GIMP_CONTEXT (context_hold.get ()), widget, box) :
    gtk_popover_new (widget);
  if (!created) { if (current (request)) clear (); return; }
  auto hold = ObjectRef<GObject>::sink (G_OBJECT (created));
  if (!current (request) || gtk_widget_in_destruction (created)) {
    gtk_widget_destroy (created); return;
  }
  popup = hold;
  auto *popover = GTK_WIDGET (hold.get ());
  connect (*this, controls, G_OBJECT (popover), native_filter ? "destroy" : "closed", G_CALLBACK (popup_closed));
  gtk_widget_set_name (popover, "painter-tree-layer-popup");
  if (!current (request)) return;
  GdkRectangle anchor = {0, 0, gtk_widget_get_allocated_width (widget), 1};
  if (area) {
    anchor = *area;
    gtk_tree_view_convert_bin_window_to_widget_coords (GTK_TREE_VIEW (widget),
      area->x, area->y, &anchor.x, &anchor.y);
    /* Keep the existing full-row viewport anchor for the mapped cell trigger. */
    anchor.x = 0;
    anchor.width = gtk_widget_get_allocated_width (widget);
  }
  if (!native_filter) {
    gtk_popover_set_position (GTK_POPOVER (popover), GTK_POS_RIGHT);
    if (area) gtk_popover_set_pointing_to (GTK_POPOVER (popover), &anchor);
    gtk_container_add (GTK_CONTAINER (popover), box);
  }
  opacity = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, 0, 100, 1);
  gtk_widget_set_name (opacity, "painter-tree-layer-opacity");
  gtk_widget_set_size_request (opacity, 240, -1);
  gtk_box_pack_start (GTK_BOX (box), gtk_label_new (_("Opacity")), FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (box), opacity, FALSE, FALSE, 0);
  mode = gimp_layer_mode_box_new (gimp_viewable_get_children (GIMP_VIEWABLE (target.get ())) ?
                                  GIMP_LAYER_MODE_CONTEXT_GROUP : GIMP_LAYER_MODE_CONTEXT_LAYER);
  gtk_widget_set_name (mode, "painter-tree-layer-mode");
  gtk_box_pack_start (GTK_BOX (box), mode, FALSE, FALSE, 0);
  alpha = gtk_check_button_new_with_label (_("Lock alpha channel"));
  gtk_widget_set_name (alpha, "painter-tree-layer-lock-alpha");
  gtk_widget_set_sensitive (alpha, gimp_layer_can_lock_alpha (GIMP_LAYER (target.get ())));
  gtk_box_pack_start (GTK_BOX (box), alpha, FALSE, FALSE, 0);
  refresh ();
  if (!current (request)) return;
  connect (*this, controls, G_OBJECT (opacity), "value-changed", G_CALLBACK (opacity_changed));
  connect (*this, controls, G_OBJECT (mode), "notify::layer-mode", G_CALLBACK (mode_changed));
  connect (*this, controls, G_OBJECT (alpha), "toggled", G_CALLBACK (alpha_changed));
  connect (*this, controls, target.get (), "opacity-changed", G_CALLBACK (model_changed));
  connect (*this, controls, target.get (), "mode-changed", G_CALLBACK (model_changed));
  connect (*this, controls, target.get (), "lock-alpha-changed", G_CALLBACK (model_changed));
  connect (*this, controls, target.get (), "removed", G_CALLBACK (target_removed));
  if (GIMP_IS_CLONE_LAYER (target.get ())) {
    GtkWidget *edit = gtk_button_new_with_label (_("Clone Source…"));
    gtk_widget_set_name (edit, "painter-tree-edit-clone");
    gtk_box_pack_start (GTK_BOX (box), edit, FALSE, FALSE, 0);
    connect (*this, controls, G_OBJECT (edit), "clicked", G_CALLBACK (edit_layer));
  }
  gtk_widget_show_all (popover);
  if (current (request)) {
    if (native_filter) painter_filter_layer_popup_show_at (popover, widget, area ? &anchor : nullptr);
    else gtk_popover_popup (GTK_POPOVER (popover));
  }
}
enum { PROP_0, PROP_ACTIVE, PROP_POPUP };
void get_property (GObject *object, guint id, GValue *value, GParamSpec *spec) noexcept {
  property_boundary (object, spec, "get", [&] {
    auto& store = BindingStore::require (object);
    store.read<PopupSlot> ([&] (const Popup& p) {
      if (id == PROP_ACTIVE) g_value_set_boolean (value, p.active);
      else if (id == PROP_POPUP) g_value_set_object (value, p.popup.get ());
      else G_OBJECT_WARN_INVALID_PROPERTY_ID (object, id, spec);
    });
  });
}
void set_property (GObject *object, guint id, const GValue *value, GParamSpec *spec) noexcept {
  property_boundary (object, spec, "set", [&] {
    auto& store = BindingStore::require (object);
    auto assign = [&] (Popup& p) {
      if (id == PROP_ACTIVE) p.active = g_value_get_boolean (value);
      else G_OBJECT_WARN_INVALID_PROPERTY_ID (object, id, spec);
    };
    if (store.state () == BindingStore::State::constructing)
      store.initialize<PopupSlot> (assign);
    else store.with<PopupSlot> (assign);
  });
}
void constructed (GObject *object) noexcept {
  G_OBJECT_CLASS (gimp_cell_renderer_popup_parent_class)->constructed (object);
  try { BindingStore::require (object).activate (); }
  catch (...) { g_warning ("Could not activate painter tree popup"); }
}
void dispose (GObject *object) noexcept {
  try { if (auto *store = BindingStore::find (object)) store->close (); }
  catch (...) { g_warning ("Could not dispose painter tree popup"); }
  G_OBJECT_CLASS (gimp_cell_renderer_popup_parent_class)->dispose (object);
}
void parent_clicked (GimpCellRendererPopup *cell, GObject *widget,
                     const gchar *path, GdkRectangle *area) noexcept {
  use (G_OBJECT (cell), [&] (Popup& p) { p.open (GTK_WIDGET (widget), path, area); });
}
gboolean activate (GtkCellRenderer *cell, GdkEvent *event, GtkWidget *widget,
                   const gchar *path, const GdkRectangle *background,
                   const GdkRectangle *, GtkCellRendererState) noexcept {
  if (event && (event->type != GDK_BUTTON_PRESS || event->button.button != 1)) return FALSE;
  GdkRectangle area = *background;
  gimp_cell_renderer_popup_clicked (GIMP_CELL_RENDERER_POPUP (cell), G_OBJECT (widget), path, &area);
  return TRUE;
}
}
static void gimp_cell_renderer_popup_class_init (GimpCellRendererPopupClass *klass) {
  G_OBJECT_CLASS (klass)->constructed = constructed;
  G_OBJECT_CLASS (klass)->dispose = dispose;
  G_OBJECT_CLASS (klass)->get_property = get_property;
  G_OBJECT_CLASS (klass)->set_property = set_property;
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_ACTIVE,
    g_param_spec_boolean ("active", "Active", "Legacy renderer active state", FALSE,
                         GParamFlags (G_PARAM_READWRITE | G_PARAM_CONSTRUCT)));
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_POPUP,
    g_param_spec_object ("popup", "Popup", "Current layer properties popup",
                         GTK_TYPE_WIDGET, G_PARAM_READABLE));
  GTK_CELL_RENDERER_CLASS (klass)->activate = activate;
  klass->parent_clicked = parent_clicked;
  /* The historic popover listener was connected during constructed(), before
   * caller observers. Run the provider closure first to retain that ordering
   * and capture the row before an observer can change its path or reopen. */
  g_signal_new ("parent-clicked", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_FIRST,
    G_STRUCT_OFFSET (GimpCellRendererPopupClass, parent_clicked), nullptr, nullptr, nullptr,
    G_TYPE_NONE, 3, G_TYPE_OBJECT, G_TYPE_STRING, G_TYPE_POINTER);
}
static void gimp_cell_renderer_popup_init (GimpCellRendererPopup *cell) {
  try {
    auto& store = BindingStore::ensure (G_OBJECT (cell));
    store.emplace<PopupSlot> (cell);
    g_object_set (cell, "mode", GTK_CELL_RENDERER_MODE_ACTIVATABLE,
                  "icon-name", GIMP_ICON_MENU_RIGHT, "xpad", 0, "ypad", 0, nullptr);
  } catch (...) { g_warning ("Could not construct painter tree popup"); }
}
GtkCellRenderer *gimp_cell_renderer_popup_new () {
  return GTK_CELL_RENDERER (g_object_new (GIMP_TYPE_CELL_RENDERER_POPUP, nullptr));
}
gboolean gimp_cell_renderer_popup_bind_tree (GimpCellRendererPopup *cell, GtkWidget *tree) {
  g_return_val_if_fail (GIMP_IS_CELL_RENDERER_POPUP (cell), FALSE);
  g_return_val_if_fail (GIMP_IS_LAYER_TREE_VIEW (tree), FALSE);
  bool result = false;
  use (G_OBJECT (cell), [&] (Popup& p) {
    auto previous = p.tree.lock ();
    if (previous) { result = previous.get () == G_OBJECT (tree); return; }
    if (p.closed || gtk_widget_in_destruction (tree)) return;
    p.tree = WeakRef<GObject> (ObjectRef<GObject>::retain (G_OBJECT (tree)));
    connect (p, p.permanent, G_OBJECT (tree), "destroy", G_CALLBACK (owner_destroyed));
    connect (p, p.permanent, G_OBJECT (tree), "set-image", G_CALLBACK (image_changed));
    connect (p, p.permanent, G_OBJECT (tree), "set-image", G_CALLBACK (image_changed_after), G_CONNECT_AFTER);
    result = true;
  });
  return result;
}
void gimp_cell_renderer_popup_clicked (GimpCellRendererPopup *cell, GObject *widget,
                                      const gchar *path, GdkRectangle *area) {
  g_return_if_fail (GIMP_IS_CELL_RENDERER_POPUP (cell));
  g_return_if_fail (GTK_IS_TREE_VIEW (widget));
  g_return_if_fail (path != nullptr);
  boundary_void (nullptr, [&] {
    /* Reject inactive or foreign-thread calls before acquiring callback-capable
     * leases. Lookup never creates a store or reopens a closed renderer. */
    auto& store = BindingStore::require (G_OBJECT (cell));
    if (store.state () != BindingStore::State::active) return;
    auto owner = ObjectRef<GObject>::retain (G_OBJECT (cell));
    auto tree = ObjectRef<GObject>::retain (widget);
    /* Either retain can invoke a toggle callback that closes the binding.
     * Admit through the original typed slot only after both leases exist. */
    if (!store.with<PopupSlot> ([] (Popup& p) { return !p.closed; })) return;
    /* No Impl borrow spans emission: keep RUN_FIRST class closure, observers,
     * overrides and nested emissions in their normal GLib ordering. */
    g_signal_emit_by_name (cell, "parent-clicked", widget, path, area);
  });
}
GtkWidget *gimp_cell_renderer_popup_get_popup (GimpCellRendererPopup *cell) {
  g_return_val_if_fail (GIMP_IS_CELL_RENDERER_POPUP (cell), nullptr);
  GtkWidget *result = nullptr;
  use (G_OBJECT (cell), [&] (Popup& p) { result = p.popup ? GTK_WIDGET (p.popup.get ()) : nullptr; });
  return result;
}

guint64 gimp_cell_renderer_popup_get_generation (GimpCellRendererPopup *cell) {
  g_return_val_if_fail (GIMP_IS_CELL_RENDERER_POPUP (cell), G_MAXUINT64);
  guint64 result = G_MAXUINT64;
  use (G_OBJECT (cell), [&] (Popup& p) { result = p.epoch; });
  return result;
}
