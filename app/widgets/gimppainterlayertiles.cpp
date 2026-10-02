/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gegl.h>
#include <gtk/gtk.h>
extern "C" {
#include "libgimpbase/gimpbase.h"
#include "libgimpwidgets/gimpwidgets.h"
#include "widgets-types.h"
#include "core/gimp.h"
#include "core/gimpcontext.h"
#include "core/gimpcontainer.h"
#include "core/gimplist.h"
#include "core/gimpimage.h"
#include "core/gimpimage-undo.h"
#include "core/gimplayer.h"
#include "core/gimplayermask.h"
#include "core/gimplayerpreset.h"
#include "core/gimpdatafactory.h"
#include "core/gimpclonelayer.h"
#include "core/gimpfilterlayer.h"
#include "core/gimpitem.h"
#include "menus/menus.h"
#include "gimpdocked.h"
#include "gimpview.h"
#include "gimpviewrenderer.h"
#include "gimpuimanager.h"
#include "gimpmenufactory.h"
#include "gimpaction.h"
#include "gimplayermodebox.h"
#include "gimppainterlayertiles.h"
#include "actions/layers-commands.h"
#include "display/display-types.h"
#include "display/gimpdisplay.h"
#include "actions/actions-types.h"
#include "actions/actions.h"
#include "dialogs/painter-layer-dialog.h"
#include "gimp-intl.h"
}
#include "painter/binding-store.hpp"
#include "painter/connection.hpp"
#include "painter/source.hpp"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>
using namespace GimpPainter;
G_DEFINE_TYPE (GimpPainterLayerTiles, gimp_painter_layer_tiles, GIMP_TYPE_IMAGE_EDITOR)
namespace GimpPainter {
template<> struct TypeTraits<GimpPainterLayerTiles> {
  static GType type () noexcept { return GIMP_TYPE_PAINTER_LAYER_TILES; }
};
}
namespace {
struct Tiles;
struct Slot : SlotSpec<GimpPainterLayerTiles, Tiles> {};
struct Row {
  ObjectRef<GObject> layer;
  GtkWidget *widget = nullptr, *visible = nullptr, *expand = nullptr, *name = nullptr;
};
struct Hook {
  WeakRef<GObject> owner;
  ObjectRef<GObject> target;
  std::string command;
  Hook (GimpPainterLayerTiles *o, GObject *t, const char *c = "")
    : owner (ObjectRef<GObject>::retain (G_OBJECT (o))),
      target (ObjectRef<GObject>::retain (t)), command (c) {}
};
struct Tiles {
  GimpPainterLayerTiles *owner;
  ObjectRef<GObject> context, image, popup, drag_layer, press_widget, pending_single;
  GtkWidget *list = nullptr, *add = nullptr, *scroll = nullptr, *popup_content = nullptr;
  std::vector<ObjectRef<GObject>> widgets; // root children survive synchronous destroy reentry
  std::vector<Row> rows;
  std::vector<Connection> permanent, model, controls, popup_connections;
  Source rebuild, long_press, autoscroll;
  double scroll_step = 0;
  std::uint64_t image_generation = 0;
  bool syncing = false, closed = false, pressed = false, popup_opened = false, popup_active = false;
  double press_x = 0, press_y = 0;
  explicit Tiles (GimpPainterLayerTiles *o) : owner (o) {}
  GimpImage *img () const { return image ? GIMP_IMAGE (image.get ()) : nullptr; }
  GimpContext *ctx () const { return context ? GIMP_CONTEXT (context.get ()) : nullptr; }
  void cancel_press () noexcept {
    long_press.close (); pressed = false; pending_single.reset ();
    auto old = std::move (press_widget);
    if (old && gtk_widget_has_grab (GTK_WIDGET (old.get ()))) gtk_grab_remove (GTK_WIDGET (old.get ()));
  }
  void restore_focus () noexcept {
    if (!popup_active) return;
    popup_active = false;
    if (!closed) {
      gtk_widget_grab_focus (GTK_WIDGET (owner));
      g_signal_emit_by_name (owner, "popup-closed");
    }
  }
  void close_popup () noexcept {
    popup_connections.clear ();
    auto old = std::move (popup);
    popup_content = nullptr;
    if (old) gtk_widget_destroy (GTK_WIDGET (old.get ()));
    restore_focus ();
  }
  void close () noexcept {
    if (closed) return;
    closed = true; cancel_press (); rebuild.close (); autoscroll.close ();
    close_popup (); permanent.clear (); model.clear (); controls.clear ();
    rows.clear (); drag_layer.reset (); pending_single.reset ();
    gimp_docked_set_context (GIMP_DOCKED (owner), nullptr);
    image.reset (); context.reset ();
  }
  void build ();
  void bind_image (GimpImage *value);
  void refresh ();
  void schedule ();
  void observe (GimpContainer *container);
  void append (GimpContainer *container, int depth);
  void sync_selection ();
  void choose (GimpLayer *layer, GdkModifierType state);
  void action (const std::string& command, GObject *target);
  GtkWidget *show_popup (bool creation);
  bool move (GimpLayer *layer, GimpLayer *target, bool into, bool after);
  void begin_press (GtkWidget *widget, GimpLayer *layer, double x, double y);
  void scroll_drag (GtkWidget *widget, int x, int y);
};
template<class F> void use (GObject *owner, F&& f) noexcept {
  try {
    auto *store = BindingStore::find (owner);
    if (store && store->state () == BindingStore::State::active)
      store->with<Slot> (std::forward<F> (f));
  } catch (const std::exception& e) { g_warning ("Painter layer tiles: %s", e.what ()); }
}
template<class F> void dispatch (gpointer data, F&& f) noexcept {
  try {
  auto *hook = static_cast<Hook *> (data);
  auto owner = hook->owner.lock ();
  auto target = hook->target;
  auto command = hook->command;
  if (owner) use (owner.get (), [&] (Tiles& s) { f (s, target.get (), command); });
  } catch (const std::exception& error) { g_warning ("Painter layer callback: %s", error.what ()); }
}
void destroy_hook (gpointer data, GClosure *) { delete static_cast<Hook *> (data); }
void connect (Tiles& s, std::vector<Connection>& connections, GObject *emitter,
              const char *signal, GCallback callback, GObject *target = nullptr,
              const char *command = "") {
  auto hook = std::unique_ptr<Hook> (new Hook (s.owner, target, command));
  connections.emplace_back (Connection::connect (ObjectRef<GObject>::retain (emitter),
    signal, callback, hook.get (), destroy_hook));
  hook.release ();
}
void clicked (GtkWidget *, gpointer data) {
  dispatch (data, [] (Tiles& s, GObject *target, const std::string& command) {
    if (!s.syncing) s.action (command, target);
  });
}
void changed (GObject *, gpointer data) {
  dispatch (data, [] (Tiles& s, GObject *, const std::string&) { s.schedule (); });
}
void container_changed (GimpContainer *, GimpObject *, gpointer data) { changed (nullptr, data); }
void reordered (GimpContainer *, GimpObject *, gint, gint, gpointer data) { changed (nullptr, data); }
void selected (GimpImage *, gpointer data) {
  dispatch (data, [] (Tiles& s, GObject *, const std::string&) { s.close_popup (); s.sync_selection (); });
}
void list_selected (GtkListBox *, gpointer data) {
  dispatch (data, [] (Tiles& s, GObject *, const std::string&) {
    if (s.syncing || !s.img ()) return;
    GList *layers = nullptr;
    for (auto& row : s.rows)
      if (gtk_list_box_row_is_selected (GTK_LIST_BOX_ROW (row.widget)))
        layers = g_list_append (layers, row.layer.get ());
    /* GIMP requires a non-empty selection while layers exist. */
    if (layers) gimp_image_set_selected_layers (s.img (), layers);
    g_list_free (layers);
  });
}
gboolean press (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  if (event->button != 1 && event->button != 3) return FALSE;
  if (g_getenv ("PAINTER_CANVAS_TRACE")) g_printerr ("TILE_PRESS button=%u state=%u x=%.1f y=%.1f\n", event->button,event->state,event->x,event->y);
  dispatch (data, [&] (Tiles& s, GObject *target, const std::string&) {
    if (target) {
      GList *selection = s.img () ? gimp_image_get_selected_layers (s.img ()) : nullptr;
      bool keep_multiple = !(event->state & (GDK_CONTROL_MASK | GDK_SHIFT_MASK)) &&
                           g_list_length (selection) > 1 && g_list_find (selection, target);
      auto image = s.image;
      if (!keep_multiple) s.choose (GIMP_LAYER (target), GdkModifierType (event->state));
      if (s.closed || s.image.get () != image.get ()) return;
      if (event->button == 3) s.show_popup (false);
      else {
        s.begin_press (widget, GIMP_LAYER (target), event->x, event->y);
        if (keep_multiple) s.pending_single = ObjectRef<GObject>::retain (target);
      }
    } else if (event->button == 3) s.show_popup (true);
    else s.begin_press (widget, nullptr, event->x, event->y);
  });
  return TRUE;
}
gboolean motion (GtkWidget *widget, GdkEventMotion *event, gpointer data) {
  dispatch (data, [&] (Tiles& s, GObject *target, const std::string&) {
    if (!s.pressed || s.popup_opened) return;
    if (g_getenv ("PAINTER_CANVAS_TRACE")) g_printerr ("TILE_MOTION state=%u x=%.1f y=%.1f\n",event->state,event->x,event->y);
    if (!target && event->y - s.press_y > 12) {
      s.cancel_press (); s.popup_opened = true; s.show_popup (true);
    } else if (std::abs (event->x - s.press_x) > 12 || std::abs (event->y - s.press_y) > 12) {
      s.cancel_press ();
      if (target && (event->state & GDK_BUTTON1_MASK))
        gtk_drag_begin_with_coordinates (widget, gtk_drag_source_get_target_list (widget),
                                         GDK_ACTION_MOVE, 1, reinterpret_cast<GdkEvent *> (event), -1, -1);
    }
  });
  return FALSE;
}
gboolean release (GtkWidget *, GdkEventButton *event, gpointer data) {
  if (event->button != 1) return FALSE;
  dispatch (data, [] (Tiles& s, GObject *target, const std::string&) {
    const bool short_press = s.pressed && !s.popup_opened;
    auto pending = std::move (s.pending_single);
    s.cancel_press ();
    if (target && short_press && pending) s.choose (GIMP_LAYER (pending.get ()), GdkModifierType (0));
    if (!target && short_press) s.action ("layers-new-last-values", nullptr);
  });
  return TRUE;
}
gboolean key (GtkWidget *, GdkEventKey *event, gpointer data) {
  bool handled = false;
  dispatch (data, [&] (Tiles& s, GObject *, const std::string&) {
    if (event->keyval == GDK_KEY_Insert) {
      s.action ("layers-new-last-values", nullptr); handled = true;
    } else if (event->keyval == GDK_KEY_Menu || (event->keyval == GDK_KEY_F10 && (event->state & GDK_SHIFT_MASK))) {
      s.show_popup (false); handled = true;
    } else if ((event->state & GDK_MOD1_MASK) && (event->keyval == GDK_KEY_Up || event->keyval == GDK_KEY_Down)) {
      s.action (event->keyval == GDK_KEY_Up ? "layers-raise" : "layers-lower", nullptr); handled = true;
    } else if (event->keyval == GDK_KEY_Delete) {
      s.action ("layers-delete", nullptr); handled = true;
    } else if (event->keyval == GDK_KEY_Escape && s.popup) {
      s.close_popup (); gtk_widget_grab_focus (GTK_WIDGET (s.owner)); handled = true;
    }
  });
  return handled;
}
const GtkTargetEntry drag_targets[] = {{const_cast<gchar *> ("application/x-gimp-painter-layer-tile"), GTK_TARGET_SAME_APP, 0}};
void drag_begin (GtkWidget *, GdkDragContext *, gpointer data) {
  dispatch (data, [] (Tiles& s, GObject *target, const std::string&) {
    s.cancel_press ();
    s.drag_layer = ObjectRef<GObject>::retain (target);
  });
}
void drag_data (GtkWidget *, GdkDragContext *, GtkSelectionData *selection, guint, guint, gpointer data) {
  dispatch (data, [&] (Tiles& s, GObject *target, const std::string&) {
    if (!target || !s.img ()) return;
    gint id = gimp_item_get_id (GIMP_ITEM (target));
    gtk_selection_data_set (selection, gtk_selection_data_get_target (selection), 8,
                            reinterpret_cast<const guchar *> (&id), sizeof (id));
  });
}
void drag_received (GtkWidget *widget, GdkDragContext *drag, gint x, gint y,
                    GtkSelectionData *selection, guint, guint time, gpointer data) {
  bool moved = false;
  dispatch (data, [&] (Tiles& s, GObject *target, const std::string&) {
    s.autoscroll.close ();
    if (gtk_selection_data_get_length (selection) != sizeof (gint) || !target || !s.ctx ()) return;
    gint id = 0; memcpy (&id, gtk_selection_data_get_data (selection), sizeof (id));
    GimpItem *item = gimp_item_get_by_id (s.ctx ()->gimp, id);
    if (!GIMP_IS_LAYER (item)) return;
    const int height = gtk_widget_get_allocated_height (widget);
    bool into = gimp_viewable_get_children (GIMP_VIEWABLE (target)) && y >= height/3 && y < 2*height/3;
    moved = s.move (GIMP_LAYER (item), GIMP_LAYER (target), into, y >= height/2);
  });
  gtk_drag_finish (drag, moved, FALSE, time);
}
void drag_end (GtkWidget *, GdkDragContext *, gpointer data) {
  dispatch (data, [] (Tiles& s, GObject *, const std::string&) { s.autoscroll.close (); s.drag_layer.reset (); });
}
gboolean drag_motion (GtkWidget *widget, GdkDragContext *, gint x, gint y, guint, gpointer data) {
  dispatch (data, [&] (Tiles& s, GObject *, const std::string&) { s.scroll_drag (widget, x, y); });
  return FALSE;
}
void drag_leave (GtkWidget *, GdkDragContext *, guint, gpointer data) {
  dispatch (data, [] (Tiles& s, GObject *, const std::string&) { s.autoscroll.close (); });
}
std::vector<ObjectRef<GObject>> selection_refs (Tiles& s) {
  std::vector<ObjectRef<GObject>> result;
  if (s.img ()) for (GList *i=gimp_image_get_selected_layers (s.img ()); i; i=i->next)
    result.push_back (ObjectRef<GObject>::retain (G_OBJECT (i->data)));
  return result;
}
void mode_changed (GObject *box, GParamSpec *, gpointer data) {
  const auto mode = gimp_layer_mode_box_get_mode (GIMP_LAYER_MODE_BOX (box));
  dispatch (data, [&] (Tiles& s, GObject *, const std::string&) {
    if (s.syncing || !s.img ()) return;
    auto image = s.image;
    auto selection = selection_refs (s);
    gimp_image_undo_group_start (GIMP_IMAGE (image.get ()), GIMP_UNDO_GROUP_LAYER_MODE, _("Layer Mode"));
    for (auto& layer : selection) {
      if (s.closed || s.image.get () != image.get ()) break;
      if (gimp_item_is_attached (GIMP_ITEM (layer.get ())))
        gimp_layer_set_mode (GIMP_LAYER (layer.get ()), mode, TRUE);
    }
    gimp_image_undo_group_end (GIMP_IMAGE (image.get ()));
    gimp_image_flush (GIMP_IMAGE (image.get ()));
  });
}
void flag_toggled (GtkToggleButton *widget, gpointer data) {
  const bool value = gtk_toggle_button_get_active (widget);
  dispatch (data, [&] (Tiles& s, GObject *, const std::string& command) {
    if (s.syncing || !s.img ()) return;
    auto image = s.image;
    auto selection = selection_refs (s);
    gimp_image_undo_group_start (GIMP_IMAGE (image.get ()), GIMP_UNDO_GROUP_ITEM_PROPERTIES, _("Layer Properties"));
    for (auto& item : selection) {
      if (s.closed || s.image.get () != image.get ()) break;
      auto *layer = GIMP_LAYER (item.get ());
      if (!gimp_item_is_attached (GIMP_ITEM (layer))) continue;
      if (command == "painter-layer-lock-alpha" && gimp_layer_can_lock_alpha (layer)) gimp_layer_set_lock_alpha (layer, value, TRUE);
      else if (command == "painter-layer-lock-content" && gimp_item_can_lock_content (GIMP_ITEM (layer))) gimp_item_set_lock_content (GIMP_ITEM (layer), value, TRUE);
      else if (command == "painter-layer-lock-position" && gimp_item_can_lock_position (GIMP_ITEM (layer))) gimp_item_set_lock_position (GIMP_ITEM (layer), value, TRUE);
      else if (gimp_layer_get_mask (layer)) {
        if (command == "painter-layer-mask-edit") gimp_layer_set_edit_mask (layer, value);
        if (command == "painter-layer-mask-show") gimp_layer_set_show_mask (layer, value, TRUE);
        if (command == "painter-layer-mask-apply") gimp_layer_set_apply_mask (layer, value, TRUE);
      }
    }
    gimp_image_undo_group_end (GIMP_IMAGE (image.get ()));
    gimp_image_flush (GIMP_IMAGE (image.get ()));
  });
}
void opacity (GtkRange *range, gpointer data) {
  const double value = gtk_range_get_value (range) / 100.;
  dispatch (data, [&] (Tiles& s, GObject *, const std::string&) {
    if (!s.img ()) return;
    auto image = s.image;
    auto layers = selection_refs (s);
    gimp_image_undo_group_start (GIMP_IMAGE (image.get ()), GIMP_UNDO_GROUP_LAYER_OPACITY, _("Layer Opacity"));
    for (auto& layer : layers) {
      if (s.closed || s.image.get () != image.get ()) break;
      if (gimp_item_is_attached (GIMP_ITEM (layer.get ())))
        gimp_layer_set_opacity (GIMP_LAYER (layer.get ()), value, TRUE);
    }
    gimp_image_undo_group_end (GIMP_IMAGE (image.get ()));
    gimp_image_flush (GIMP_IMAGE (image.get ()));
  });
}
void popup_closed (GtkPopover *, gpointer data) {
  dispatch (data, [] (Tiles& s, GObject *, const std::string&) {
    s.cancel_press ();
    s.restore_focus ();
  });
}
void unmapped (GtkWidget *, gpointer data) {
  dispatch (data, [] (Tiles& s, GObject *, const std::string&) {
    s.cancel_press (); s.autoscroll.close (); s.close_popup ();
  });
}
gboolean grab_broken (GtkWidget *, GdkEventGrabBroken *, gpointer data) {
  dispatch (data, [] (Tiles& s, GObject *, const std::string&) { s.cancel_press (); });
  return FALSE;
}
GtkWidget *button (Tiles& s, GtkWidget *box, const char *label, const char *name,
                   std::vector<Connection>& hooks, GObject *target = nullptr) {
  GtkWidget *result = gtk_button_new_with_label (label);
  gtk_widget_set_name (result, name); gtk_widget_set_can_focus (result, FALSE);
  gtk_box_pack_start (GTK_BOX (box), result, FALSE, FALSE, 0);
  connect (s, hooks, G_OBJECT (result), "clicked", G_CALLBACK (clicked), target, name);
  return result;
}
void Tiles::build () {
  gtk_widget_set_name (GTK_WIDGET (owner), "painter-layer-tiles");
  gimp_editor_set_show_name (GIMP_EDITOR (owner), FALSE);
  gtk_widget_set_can_focus (GTK_WIDGET (owner), TRUE);
  GtkWidget *bar = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_box_pack_start (GTK_BOX (owner), bar, FALSE, FALSE, 0);
  add = gtk_button_new_with_label ("+");
  gtk_widget_set_name (add, "painter-layer-add");
  gtk_widget_set_tooltip_text (add, _("Add a layer. Hold or drag down for layer types and presets."));
  gtk_widget_set_can_focus (add, FALSE);
  gtk_box_pack_start (GTK_BOX (bar), add, TRUE, TRUE, 0);
  gtk_widget_add_events (add, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK);
  connect (*this, permanent, G_OBJECT (add), "button-press-event", G_CALLBACK (press));
  connect (*this, permanent, G_OBJECT (add), "motion-notify-event", G_CALLBACK (motion));
  connect (*this, permanent, G_OBJECT (add), "button-release-event", G_CALLBACK (release));
  button (*this, bar, "⋮", "layer-popup", permanent);
  scroll = gtk_scrolled_window_new (nullptr, nullptr);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_box_pack_start (GTK_BOX (owner), scroll, TRUE, TRUE, 0);
  list = gtk_list_box_new ();
  gtk_list_box_set_selection_mode (GTK_LIST_BOX (list), GTK_SELECTION_MULTIPLE);
  gtk_container_add (GTK_CONTAINER (scroll), list);
  connect (*this, permanent, G_OBJECT (list), "selected-rows-changed", G_CALLBACK (list_selected));
  connect (*this, permanent, G_OBJECT (owner), "key-press-event", G_CALLBACK (key));
  connect (*this, permanent, G_OBJECT (owner), "unmap", G_CALLBACK (unmapped));
  connect (*this, permanent, G_OBJECT (add), "grab-broken-event", G_CALLBACK (grab_broken));
  widgets.push_back (ObjectRef<GObject>::retain (G_OBJECT (list)));
  widgets.push_back (ObjectRef<GObject>::retain (G_OBJECT (add)));
  widgets.push_back (ObjectRef<GObject>::retain (G_OBJECT (scroll)));
  gtk_widget_show_all (GTK_WIDGET (owner));
}
void Tiles::bind_image (GimpImage *value) {
  ++image_generation;
  cancel_press (); rebuild.close (); autoscroll.close (); close_popup ();
  model.clear (); controls.clear (); rows.clear (); drag_layer.reset ();
  image = ObjectRef<GObject>::retain (value ? G_OBJECT (value) : nullptr);
  if (ctx ()) gimp_context_set_image (ctx (), value);
  refresh ();
}
void Tiles::schedule () {
  if (closed || rebuild.active ()) return;
  auto weak = std::make_shared<WeakRef<GObject>> (ObjectRef<GObject>::retain (G_OBJECT (owner)));
  const auto generation = BindingStore::require (G_OBJECT (owner)).generation ();
  rebuild = Source::idle (nullptr, G_PRIORITY_LOW, [weak, generation] () {
    auto owner = weak->lock ();
    if (owner) {
      auto *store = BindingStore::find (owner.get ());
      if (store && store->accepts (generation)) store->with<Slot> ([] (Tiles& s) { s.refresh (); });
    }
    return false;
  });
}
void Tiles::observe (GimpContainer *container) {
  connect (*this, model, G_OBJECT (container), "add", G_CALLBACK (container_changed));
  connect (*this, model, G_OBJECT (container), "remove", G_CALLBACK (container_changed));
  connect (*this, model, G_OBJECT (container), "reorder", G_CALLBACK (reordered));
  for (GList *i = GIMP_LIST (container)->queue->head; i; i=i->next) {
    auto *layer = GIMP_LAYER (i->data);
    for (const char *signal : {"name-changed", "visibility-changed", "mask-changed", "expanded-changed", "lock-content-changed"})
      connect (*this, model, G_OBJECT (layer), signal, G_CALLBACK (changed));
    if (auto *children = gimp_viewable_get_children (GIMP_VIEWABLE (layer))) observe (children);
  }
}
void Tiles::append (GimpContainer *container, int depth) {
  for (GList *i = GIMP_LIST (container)->queue->head; i; i=i->next) {
    auto *layer = GIMP_LAYER (i->data);
    Row row; row.layer = ObjectRef<GObject>::retain (G_OBJECT (layer));
    row.widget = gtk_list_box_row_new ();
    gtk_widget_set_name (row.widget, "painter-layer-tile");
    gtk_widget_set_tooltip_text (row.widget, gimp_object_get_name (layer));
    gtk_widget_set_margin_start (row.widget, std::min (depth, 5) * 8);
    GtkWidget *box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add (GTK_CONTAINER (row.widget), box);
    GtkWidget *top = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start (GTK_BOX (box), top, FALSE, FALSE, 0);
    row.visible = gtk_toggle_button_new ();
    gtk_button_set_image (GTK_BUTTON (row.visible), gtk_image_new_from_icon_name ("gimp-visible", GTK_ICON_SIZE_MENU));
    gtk_widget_set_name (row.visible, "painter-layer-visible");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (row.visible), gimp_item_get_visible (GIMP_ITEM (layer)));
    gtk_widget_set_can_focus (row.visible, FALSE);
    gtk_box_pack_start (GTK_BOX (top), row.visible, FALSE, FALSE, 0);
    connect (*this, controls, G_OBJECT (row.visible), "toggled", G_CALLBACK (clicked), G_OBJECT (layer), "visibility");
    auto *children = gimp_viewable_get_children (GIMP_VIEWABLE (layer));
    if (children) row.expand = button (*this, top,
      gimp_viewable_get_expanded (GIMP_VIEWABLE (layer)) ? "▾" : "▸", "expand", controls, G_OBJECT (layer));
    GtkWidget *event = gtk_event_box_new ();
    gtk_event_box_set_above_child (GTK_EVENT_BOX (event), TRUE);
    gtk_widget_set_name (event, "painter-layer-preview");
    GtkWidget *previews = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 1);
    gtk_container_add (GTK_CONTAINER (event), previews);
    GtkWidget *preview = gimp_view_new_full (ctx (), GIMP_VIEWABLE (layer), 56, 56, 1, FALSE, FALSE, FALSE);
    GIMP_VIEW (preview)->eat_button_events = FALSE;
    gtk_box_pack_start (GTK_BOX (previews), preview, TRUE, FALSE, 0);
    if (auto *mask = gimp_layer_get_mask (layer)) {
      GtkWidget *mask_view = gimp_view_new_full (ctx (), GIMP_VIEWABLE (mask), 24, 40, 1, FALSE, FALSE, FALSE);
      GIMP_VIEW (mask_view)->eat_button_events = FALSE;
      gtk_box_pack_start (GTK_BOX (previews), mask_view, FALSE, FALSE, 0);
    }
    gtk_box_pack_start (GTK_BOX (box), event, FALSE, FALSE, 0);
    row.name = gtk_label_new (gimp_object_get_name (layer));
    gtk_label_set_ellipsize (GTK_LABEL (row.name), PANGO_ELLIPSIZE_END);
    gtk_label_set_max_width_chars (GTK_LABEL (row.name), 11);
    gtk_box_pack_start (GTK_BOX (box), row.name, FALSE, FALSE, 0);
    gtk_widget_add_events (event, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK);
    connect (*this, controls, G_OBJECT (event), "button-press-event", G_CALLBACK (press), G_OBJECT (layer));
    connect (*this, controls, G_OBJECT (event), "button-release-event", G_CALLBACK (release), G_OBJECT (layer));
    connect (*this, controls, G_OBJECT (event), "motion-notify-event", G_CALLBACK (motion), G_OBJECT (layer));
    connect (*this, controls, G_OBJECT (event), "grab-broken-event", G_CALLBACK (grab_broken));
    /* Our press/long-press gesture starts the drag explicitly. A second GTK
     * implicit gesture must not race the 500 ms popup timer. */
    gtk_drag_source_set (event, GdkModifierType (0), drag_targets, 1, GDK_ACTION_MOVE);
    gtk_drag_dest_set (event, GTK_DEST_DEFAULT_ALL, drag_targets, 1, GDK_ACTION_MOVE);
    connect (*this, controls, G_OBJECT (event), "drag-begin", G_CALLBACK (drag_begin), G_OBJECT (layer));
    connect (*this, controls, G_OBJECT (event), "drag-data-get", G_CALLBACK (drag_data), G_OBJECT (layer));
    connect (*this, controls, G_OBJECT (event), "drag-data-received", G_CALLBACK (drag_received), G_OBJECT (layer));
    connect (*this, controls, G_OBJECT (event), "drag-end", G_CALLBACK (drag_end));
    connect (*this, controls, G_OBJECT (event), "drag-motion", G_CALLBACK (drag_motion));
    connect (*this, controls, G_OBJECT (event), "drag-leave", G_CALLBACK (drag_leave));
    gtk_container_add (GTK_CONTAINER (list), row.widget);
    rows.emplace_back (std::move (row));
    if (children && gimp_viewable_get_expanded (GIMP_VIEWABLE (layer))) append (children, depth+1);
  }
}
void Tiles::refresh () {
  if (closed) return;
  cancel_press (); autoscroll.close ();
  rebuild.close (); model.clear (); controls.clear (); rows.clear ();
  syncing = true;
  GList *children = gtk_container_get_children (GTK_CONTAINER (list));
  std::vector<ObjectRef<GObject>> old_children;
  for (GList *i=children; i; i=i->next) old_children.push_back (ObjectRef<GObject>::retain (G_OBJECT (i->data)));
  g_list_free (children);
  for (auto& child : old_children) { gtk_widget_destroy (GTK_WIDGET (child.get ())); if (closed) return; }
  if (img ()) {
    observe (gimp_image_get_layers (img ()));
    connect (*this, model, G_OBJECT (img ()), "selected-layers-changed", G_CALLBACK (selected));
    append (gimp_image_get_layers (img ()), 0);
  }
  syncing = false; sync_selection (); gtk_widget_show_all (list);
}
void Tiles::sync_selection () {
  if (syncing || closed) return;
  syncing = true;
  GList *selected = img () ? gimp_image_get_selected_layers (img ()) : nullptr;
  for (auto& row : rows) {
    if (g_list_find (selected, row.layer.get ())) gtk_list_box_select_row (GTK_LIST_BOX (list), GTK_LIST_BOX_ROW (row.widget));
    else gtk_list_box_unselect_row (GTK_LIST_BOX (list), GTK_LIST_BOX_ROW (row.widget));
  }
  syncing = false;
}
void Tiles::choose (GimpLayer *layer, GdkModifierType state) {
  if (!img () || gimp_item_get_image (GIMP_ITEM (layer)) != img ()) return;
  GList *selection = g_list_copy (gimp_image_get_selected_layers (img ()));
  if (state & GDK_CONTROL_MASK) {
    if (g_list_find (selection, layer)) selection = g_list_remove (selection, layer);
    else selection = g_list_append (selection, layer);
  } else if ((state & GDK_SHIFT_MASK) && selection) {
    int first = -1, last = -1;
    for (std::size_t i=0; i<rows.size (); ++i) {
      if (rows[i].layer.get () == selection->data) first = int (i);
      if (rows[i].layer.get () == G_OBJECT (layer)) last = int (i);
    }
    if (first >= 0 && last >= 0) {
      g_list_free (selection); selection = nullptr;
      for (int i=std::min(first,last); i<=std::max(first,last); ++i)
        selection = g_list_append (selection, rows[i].layer.get ());
    }
  } else { g_list_free (selection); selection = g_list_append (nullptr, layer); }
  if (selection) gimp_image_set_selected_layers (img (), selection);
  g_list_free (selection); sync_selection ();
}
void Tiles::begin_press (GtkWidget *widget, GimpLayer *layer, double x, double y) {
  if (closed || !img ()) return;
  cancel_press (); press_widget = ObjectRef<GObject>::retain (G_OBJECT (widget));
  gtk_grab_add (widget); pressed = true; popup_opened = false; press_x=x; press_y=y;
  auto weak = std::make_shared<WeakRef<GObject>> (ObjectRef<GObject>::retain (G_OBJECT (owner)));
  const auto generation = BindingStore::require (G_OBJECT (owner)).generation ();
  const bool creation = !layer;
  const auto image_epoch = image_generation;
  long_press = Source::timeout (nullptr, 500, G_PRIORITY_DEFAULT, [weak, generation, image_epoch, creation] () {
    auto object=weak->lock ();
    if (object) {
      auto *store=BindingStore::find (object.get ());
      if (store && store->accepts (generation)) store->with<Slot> ([&] (Tiles& s) {
        if (s.image_generation == image_epoch && s.pressed && !s.popup_opened) { s.cancel_press (); s.popup_opened=true; s.show_popup (creation); }
      });
    }
    return false;
  });
}
void Tiles::scroll_drag (GtkWidget *widget, int x, int y) {
  int sx = 0, sy = 0;
  if (!gtk_widget_translate_coordinates (widget, scroll, x, y, &sx, &sy)) return;
  int height = gtk_widget_get_allocated_height (scroll);
  scroll_step = sy < 32 ? -8. : sy > height - 32 ? 8. : 0.;
  if (!scroll_step) { autoscroll.close (); return; }
  if (autoscroll.active ()) return;
  auto weak = std::make_shared<WeakRef<GObject>> (ObjectRef<GObject>::retain (G_OBJECT (owner)));
  auto generation = BindingStore::require (G_OBJECT (owner)).generation ();
  autoscroll = Source::timeout (nullptr, 25, G_PRIORITY_DEFAULT, [weak, generation] () {
    auto owner = weak->lock ();
    bool again = false;
    if (owner) {
      auto *store = BindingStore::find (owner.get ());
      if (store && store->accepts (generation)) store->with<Slot> ([&] (Tiles& s) {
        GtkAdjustment *adjustment = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (s.scroll));
        double value = gtk_adjustment_get_value (adjustment);
        gtk_adjustment_set_value (adjustment, value + s.scroll_step);
        again = !s.closed && s.scroll_step != 0;
      });
    }
    return again;
  });
}
bool Tiles::move (GimpLayer *layer, GimpLayer *target, bool into, bool after) {
  if (!img () || !layer || !target || layer == target ||
      gimp_item_get_image (GIMP_ITEM (layer)) != img () ||
      gimp_item_get_image (GIMP_ITEM (target)) != img () ||
      !gimp_item_is_attached (GIMP_ITEM (layer)) || !gimp_item_is_attached (GIMP_ITEM (target))) return false;
  auto image_lease = image;
  auto target_lease = ObjectRef<GObject>::retain (G_OBJECT (target));
  auto *children = gimp_viewable_get_children (GIMP_VIEWABLE (target));
  if (children && after && !gimp_container_get_n_children (children)) into = true;
  auto *parent = into ? target : gimp_layer_get_parent (target);
  if (into && !children) return false;
  GList *selection = gimp_image_get_selected_layers (img ());
  std::vector<ObjectRef<GObject>> moving;
  if (g_list_find (selection, layer) && g_list_length (selection) > 1) {
    GList *all = gimp_image_get_layer_list (img ());
    for (GList *i=all; i; i=i->next) if (g_list_find (selection, i->data)) {
      bool nested = false;
      for (auto *p=gimp_layer_get_parent (GIMP_LAYER (i->data)); p; p=gimp_layer_get_parent (p))
        if (g_list_find (selection, p)) { nested = true; break; }
      if (!nested) moving.push_back (ObjectRef<GObject>::retain (G_OBJECT (i->data)));
    }
    g_list_free (all);
  } else moving.push_back (ObjectRef<GObject>::retain (G_OBJECT (layer)));
  for (auto& item : moving) {
    if (item.get () == G_OBJECT (target)) return false;
    for (auto *p=parent; p; p=gimp_layer_get_parent (p)) if (G_OBJECT (p) == item.get ()) return false;
  }
  /* Match the native tree view's stable relative order. Repeated inserts after
   * one anchor, and at the start of a group, run in reverse source order. */
  if (into || after) std::reverse (moving.begin (), moving.end ());
  bool result = false;
  gimp_image_undo_group_start (GIMP_IMAGE (image_lease.get ()), GIMP_UNDO_GROUP_IMAGE_ITEM_REORDER, _("Reorder Layers"));
  for (auto& item : moving) {
    if (closed || image.get () != image_lease.get () || !gimp_item_is_attached (GIMP_ITEM (target))) break;
    auto *source = GIMP_LAYER (item.get ());
    if (!gimp_item_is_attached (GIMP_ITEM (source))) continue;
    int index = into ? 0 : gimp_item_get_index (GIMP_ITEM (target)) + (after ? 1 : 0);
    if (gimp_layer_get_parent (source) == parent && gimp_item_get_index (GIMP_ITEM (source)) < index) --index;
    result = gimp_image_reorder_item (GIMP_IMAGE (image_lease.get ()), GIMP_ITEM (source),
                                      parent ? GIMP_ITEM (parent) : nullptr, index, TRUE, nullptr) || result;
  }
  gimp_image_undo_group_end (GIMP_IMAGE (image_lease.get ()));
  if (result) {
    if (!closed && parent) gimp_viewable_set_expanded (GIMP_VIEWABLE (parent), TRUE);
    gimp_image_flush (GIMP_IMAGE (image_lease.get ()));
  }
  return result;
}
void Tiles::action (const std::string& command, GObject *target) {
  if (!img ()) return;
  auto image_lease=image;
  auto context_lease=context;
  if (command == "visibility" && target) {
    auto layer_lease=ObjectRef<GObject>::retain(target);
    auto *item=GIMP_ITEM (target);
    if (gimp_item_is_attached (item) && gimp_item_get_image (item) == img ()) {
      gimp_item_set_visible (item, !gimp_item_get_visible (item), TRUE); gimp_image_flush (GIMP_IMAGE (image_lease.get ()));
    }
  } else if (command == "layer-menu") {
    close_popup ();
    if (!closed) gimp_editor_popup_menu_at_pointer (GIMP_EDITOR (owner), nullptr);
  } else if (command == "expand" && target) {
    gimp_viewable_set_expanded (GIMP_VIEWABLE (target), !gimp_viewable_get_expanded (GIMP_VIEWABLE (target)));
  } else if (command == "layer-popup") show_popup (false);
  else if (command == "preset" && target) {
    GList *selection=gimp_image_get_selected_layers (img ());
    GError *error=nullptr;
    if (!gimp_layer_preset_apply (GIMP_LAYER_PRESET (target), GIMP_CONTEXT (context_lease.get ()), selection ? GIMP_LAYER (selection->data) : nullptr, &error)) {
      if (!closed) gimp_message_literal (GIMP_CONTEXT (context_lease.get ())->gimp, G_OBJECT (owner), GIMP_MESSAGE_ERROR, error ? error->message : _("Cannot apply layer preset"));
      g_clear_error (&error);
    }
    close_popup ();
  } else {
    /* Existing commands own all dialogs/argument copies, action sensitivity,
     * Undo groups and independent Clone/Filter semantics. The editor supplies
     * this canvas's image even when another display is globally active. */
    using Command = void (*) (GimpAction *, GVariant *, gpointer);
    struct Entry { const char *name; Command callback; };
    static const Entry entries[] = {
      {"layers-new", layers_new_cmd_callback}, {"layers-new-last-values", layers_new_last_vals_cmd_callback},
      {"layers-new-group", layers_new_group_cmd_callback}, {"layers-duplicate", layers_duplicate_cmd_callback},
      {"layers-new-clone", layers_new_clone_cmd_callback}, {"layers-new-filter", layers_new_filter_cmd_callback},
      {"layers-edit-clone", layers_edit_clone_cmd_callback}, {"layers-edit-filter", layers_edit_filter_cmd_callback},
      {"layers-edit-attributes", layers_edit_attributes_cmd_callback},
      {"layers-raise", layers_raise_cmd_callback}, {"layers-lower", layers_lower_cmd_callback},
      {"layers-delete", layers_delete_cmd_callback}
    };
    for (const auto& entry : entries) if (command == entry.name) {
      close_popup (); entry.callback (nullptr, nullptr, owner); break;
    }
  }
}
GtkWidget *Tiles::show_popup (bool creation) {
  if (!img ()) return nullptr;
  close_popup ();
  popup = ObjectRef<GObject>::sink (G_OBJECT (gtk_popover_new (creation ? add : GTK_WIDGET (owner))));
  auto *widget=GTK_WIDGET (popup.get ());
  gtk_widget_set_name (widget, creation ? "painter-layer-create-popup" : "painter-layer-properties-popup");
  gtk_popover_set_position (GTK_POPOVER (widget), GTK_POS_LEFT);
  popup_content=gtk_box_new (GTK_ORIENTATION_VERTICAL, 3);
  gtk_container_set_border_width (GTK_CONTAINER (popup_content), 8);
  GtkWidget *scroller = gtk_scrolled_window_new (nullptr, nullptr);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroller), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_min_content_width (GTK_SCROLLED_WINDOW (scroller), 260);
  gtk_scrolled_window_set_max_content_height (GTK_SCROLLED_WINDOW (scroller), 500);
  gtk_scrolled_window_set_propagate_natural_height (GTK_SCROLLED_WINDOW (scroller), TRUE);
  gtk_container_add (GTK_CONTAINER (scroller), popup_content);
  gtk_container_add (GTK_CONTAINER (widget), scroller);
  connect (*this, popup_connections, G_OBJECT (widget), "closed", G_CALLBACK (popup_closed));
  const char *create_labels[] = { N_("New Layer…"), N_("New Group"), N_("Duplicate"), N_("New Clone Layer"), N_("New Filter Layer…") };
  const char *create_commands[] = { "layers-new", "layers-new-group", "layers-duplicate", "layers-new-clone", "layers-new-filter" };
  for (unsigned i=0; i<G_N_ELEMENTS(create_commands); ++i)
    button (*this, popup_content, _(create_labels[i]), create_commands[i], popup_connections);
  if (!creation) {
    GList *selection=gimp_image_get_selected_layers (img ());
    auto *layer=selection ? GIMP_LAYER (selection->data) : nullptr;
    if (layer) {
      GtkWidget *scale=gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, 0, 100, 1);
      gtk_widget_set_name (scale, "painter-layer-opacity");
      gtk_range_set_value (GTK_RANGE (scale), 100*gimp_layer_get_opacity (layer));
      gtk_box_pack_start (GTK_BOX (popup_content), gtk_label_new (_("Opacity")), FALSE, FALSE, 0);
      gtk_box_pack_start (GTK_BOX (popup_content), scale, FALSE, FALSE, 0);
      connect (*this, popup_connections, G_OBJECT (scale), "value-changed", G_CALLBACK (opacity));
      GtkWidget *mode = gimp_layer_mode_box_new (GIMP_LAYER_MODE_CONTEXT_LAYER);
      gtk_widget_set_name (mode, "painter-layer-mode");
      gimp_layer_mode_box_set_mode (GIMP_LAYER_MODE_BOX (mode), gimp_layer_get_mode (layer));
      gtk_box_pack_start (GTK_BOX (popup_content), mode, FALSE, FALSE, 0);
      connect (*this, popup_connections, G_OBJECT (mode), "notify::layer-mode", G_CALLBACK (mode_changed));
      auto flag = [&] (const char *label, const char *name, bool value) {
        GtkWidget *check = gtk_check_button_new_with_label (label);
        gtk_widget_set_name (check, name);
        if (!strcmp (name, "painter-layer-lock-alpha")) gtk_widget_set_sensitive (check, gimp_layer_can_lock_alpha (layer));
        if (!strcmp (name, "painter-layer-lock-content")) gtk_widget_set_sensitive (check, gimp_item_can_lock_content (GIMP_ITEM (layer)));
        if (!strcmp (name, "painter-layer-lock-position")) gtk_widget_set_sensitive (check, gimp_item_can_lock_position (GIMP_ITEM (layer)));
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), value);
        gtk_box_pack_start (GTK_BOX (popup_content), check, FALSE, FALSE, 0);
        connect (*this, popup_connections, G_OBJECT (check), "toggled", G_CALLBACK (flag_toggled), nullptr, name);
      };
      flag (_("Lock Alpha"), "painter-layer-lock-alpha", gimp_layer_get_lock_alpha (layer));
      flag (_("Lock Pixels"), "painter-layer-lock-content", gimp_item_get_lock_content (GIMP_ITEM (layer)));
      flag (_("Lock Position"), "painter-layer-lock-position", gimp_item_get_lock_position (GIMP_ITEM (layer)));
      if (gimp_layer_get_mask (layer)) {
        flag (_("Edit Mask"), "painter-layer-mask-edit", gimp_layer_get_edit_mask (layer));
        flag (_("Show Mask"), "painter-layer-mask-show", gimp_layer_get_show_mask (layer));
        flag (_("Apply Mask"), "painter-layer-mask-apply", gimp_layer_get_apply_mask (layer));
      }
      button (*this, popup_content, _("Edit Layer Attributes…"), "layers-edit-attributes", popup_connections);
      if (GIMP_IS_CLONE_LAYER (layer)) button (*this, popup_content, _("Clone Source…"), "layers-edit-clone", popup_connections);
      if (GIMP_IS_FILTER_LAYER (layer)) button (*this, popup_content, _("Filter Settings…"), "layers-edit-filter", popup_connections);
      button (*this, popup_content, _("Raise Layer"), "layers-raise", popup_connections);
      button (*this, popup_content, _("Lower Layer"), "layers-lower", popup_connections);
      button (*this, popup_content, _("Delete Layer"), "layers-delete", popup_connections);
    }
  }
  if (!creation) button (*this, popup_content, _("All Layer Actions…"), "layer-menu", popup_connections);
  GimpContainer *presets=gimp_data_factory_get_container (ctx ()->gimp->layer_preset_factory);
  for (GList *i=GIMP_LIST (presets)->queue->head; i; i=i->next)
    button (*this, popup_content, gimp_object_get_name (i->data), "preset", popup_connections, G_OBJECT (i->data));
  popup_active = true;
  gtk_widget_show_all (widget); gtk_popover_popup (GTK_POPOVER (widget));
  return widget;
}
void close_widget (GtkWidget *widget) {
  try { if (auto *store=BindingStore::find (G_OBJECT (widget))) store->close (); }
  catch (...) { g_warning ("Could not close painter layer tiles"); }
  GTK_WIDGET_CLASS (gimp_painter_layer_tiles_parent_class)->destroy (widget);
}
void set_image (GimpImageEditor *editor, GimpImage *image) {
  GIMP_IMAGE_EDITOR_CLASS (gimp_painter_layer_tiles_parent_class)->set_image (editor, image);
  use (G_OBJECT (editor), [&] (Tiles& s) { s.bind_image (image); });
}
}
static void gimp_painter_layer_tiles_class_init (GimpPainterLayerTilesClass *klass) {
  g_signal_new ("popup-closed", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST, 0,
                nullptr, nullptr, nullptr, G_TYPE_NONE, 0);
  GTK_WIDGET_CLASS (klass)->destroy=close_widget;
  GIMP_IMAGE_EDITOR_CLASS (klass)->set_image=set_image;
}
static void gimp_painter_layer_tiles_init (GimpPainterLayerTiles *owner) {
  try {
    auto& store=BindingStore::ensure (G_OBJECT (owner)); store.emplace<Slot> (owner); store.activate ();
    store.with<Slot> ([] (Tiles& s) { s.build (); });
  } catch (const std::exception& e) { g_warning ("Cannot construct painter layer tiles: %s", e.what ()); }
}
GtkWidget *gimp_painter_layer_tiles_new (GimpContext *context, GimpImage *image) {
  g_return_val_if_fail (GIMP_IS_CONTEXT (context), nullptr);
  auto *owner=GIMP_PAINTER_LAYER_TILES (g_object_new (GIMP_TYPE_PAINTER_LAYER_TILES, nullptr));
  use (G_OBJECT (owner), [&] (Tiles& s) {
    s.context=ObjectRef<GObject>::adopt (G_OBJECT (gimp_context_new (context->gimp, "painter-layer-tiles", context)));
    gimp_context_define_properties (s.ctx (), GimpContextPropMask (GIMP_CONTEXT_PROP_MASK_ALL & ~(GIMP_CONTEXT_PROP_MASK_IMAGE | GIMP_CONTEXT_PROP_MASK_DISPLAY)), FALSE);
    gimp_context_set_parent (s.ctx (), context);
    gimp_context_set_display (s.ctx (), nullptr);
    gimp_docked_set_context (GIMP_DOCKED (owner), s.ctx ());
    if (global_action_factory)
      if (auto *factory = menus_get_global_menu_factory (context->gimp))
        gimp_editor_create_menu (GIMP_EDITOR (owner), factory, "<Layers>", "/layers-popup", owner);
  });
  gimp_image_editor_set_image (GIMP_IMAGE_EDITOR (owner), image);
  return GTK_WIDGET (owner);
}
void gimp_painter_layer_tiles_set_image (GimpPainterLayerTiles *tiles, GimpImage *image) {
  g_return_if_fail (GIMP_IS_PAINTER_LAYER_TILES (tiles));
  gimp_image_editor_set_image (GIMP_IMAGE_EDITOR (tiles), image);
}
GtkWidget *gimp_painter_layer_tiles_popup (GimpPainterLayerTiles *tiles, gboolean creation) {
  GtkWidget *result=nullptr;
  use (G_OBJECT (tiles), [&] (Tiles& s) { result=s.show_popup (creation); });
  return result;
}
gboolean gimp_painter_layer_tiles_move (GimpPainterLayerTiles *tiles, GimpLayer *layer,
                                       GimpLayer *target, gboolean into, gboolean after) {
  bool result=false; use (G_OBJECT (tiles), [&] (Tiles& s) { result=s.move (layer,target,into,after); }); return result;
}
gboolean gimp_painter_layer_tiles_long_press_pending (GimpPainterLayerTiles *tiles) {
  bool result=false; use (G_OBJECT (tiles), [&] (Tiles& s) { result=s.long_press.active (); }); return result;
}

void gimp_painter_layer_tiles_cancel_interaction (GimpPainterLayerTiles *tiles) {
  use (G_OBJECT (tiles), [] (Tiles& s) { s.cancel_press (); s.autoscroll.close (); s.close_popup (); });
}

void gimp_painter_layer_tiles_set_display (GimpPainterLayerTiles *tiles, GimpDisplay *display) {
  use (G_OBJECT (tiles), [&] (Tiles& s) {
    if (s.ctx ()) {
      gimp_context_set_display (s.ctx (), display);
      if (display) gimp_painter_layer_tiles_set_image (tiles, gimp_display_get_image (display));
    }
  });
}
