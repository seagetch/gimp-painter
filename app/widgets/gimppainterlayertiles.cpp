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
#include "core/gimptempbuf.h"
#include "gimpuimanager.h"
#include "gimpmenufactory.h"
#include "gimpaction.h"
#include "gimplayermodebox.h"
#include "gimppainterlayertiles.h"
#include "popupper.h"
#include "gimplayertileview.h"
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
#include "gimplayertileview-handle.hpp"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
using namespace GimpPainter;
G_DEFINE_TYPE (GimpPainterLayerTiles, gimp_painter_layer_tiles, GIMP_TYPE_IMAGE_EDITOR)
namespace {
enum { PROP_0, PROP_IMAGE, PROP_CONTEXT };
struct Tiles;
struct Slot : SlotSpec<GimpPainterLayerTiles, Tiles> {};
/* Original painter tile geometry (gimplayertileview.cpp, 2016).  These
 * are content coordinates, shared by painting, hit testing and drop feedback. */
constexpr int tile_pitch = 64, preview_size = 56, indent_width = 8;
constexpr int bar_width = 4, icon_size = 14, preview_inset = 4;
using Surface = std::unique_ptr<cairo_surface_t, decltype (&cairo_surface_destroy)>;
struct Preview {
  ObjectRef<GObject> viewable;
  Surface surface {nullptr, cairo_surface_destroy};
  unsigned revision = 1;
  bool dirty = true;
  explicit Preview (GimpViewable *v) : viewable (ObjectRef<GObject>::retain (G_OBJECT (v))) {}
};
struct Row {
  ObjectRef<GObject> layer;
  std::shared_ptr<Preview> preview, mask;
  int depth = 0;
};
struct Hook {
  WeakRef<GObject> owner;
  ObjectRef<GObject> target;
  std::string command;
  std::uint64_t popup_epoch, close_epoch;
  Hook (GimpPainterLayerTiles *o, GObject *t, const char *c = "",
        std::uint64_t p = 0, std::uint64_t close = 0)
    : owner (ObjectRef<GObject>::retain (G_OBJECT (o))),
      target (ObjectRef<GObject>::retain (t)), command (c), popup_epoch (p), close_epoch (close) {}
};
struct Tiles {
  GimpPainterLayerTiles *owner;
  ObjectRef<GObject> supplied_context, context, image, popup, drag_layer, press_layer, press_widget, pending_single;
  GtkWidget *list = nullptr, *add = nullptr, *scroll = nullptr, *popup_content = nullptr;
  std::vector<ObjectRef<GObject>> widgets; // root children survive synchronous destroy reentry
  std::vector<Row> rows;
  std::unordered_map<GObject *, std::shared_ptr<Preview>> previews;
  std::unordered_map<std::string, Surface> icons;
  int drop_row = -1;
  bool drop_into = false, drop_after = false;
  std::vector<Connection> permanent, model, controls, popup_connections;
  Source rebuild, preview_update, long_press, autoscroll;
  std::unique_ptr<GdkEvent, decltype (&gdk_event_free)> closing_press {nullptr, gdk_event_free};
  std::unique_ptr<GdkEvent, decltype (&gdk_event_free)> cancelling_press {nullptr, gdk_event_free};
  double scroll_step = 0;
  std::uint64_t image_generation = 0, popup_generation = 0, popup_close_generation = 0, context_generation = 0, refresh_generation = 0;
  bool clearing_context = false;
  bool syncing = false, closed = false, pressed = false, popup_opened = false, popup_active = false;
  double press_x = 0, press_y = 0;
  explicit Tiles (GimpPainterLayerTiles *o) : owner (o) {}
  GimpImage *img () const { return image ? GIMP_IMAGE (image.get ()) : nullptr; }
  GimpContext *ctx () const { return context ? GIMP_CONTEXT (context.get ()) : nullptr; }
  void cancel_press () noexcept {
    long_press.close (); pressed = false; pending_single.reset (); press_layer.reset ();
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
    closing_press.reset (); cancelling_press.reset ();
    ++popup_close_generation;
    popup_connections.clear ();
    auto old = std::move (popup);
    popup_content = nullptr;
    const bool was_active = popup_active;
    popup_active = false;
    if (old) gtk_widget_destroy (GTK_WIDGET (old.get ()));
    /* A destroy observer may already have installed a replacement host. */
    if (was_active && !closed && !popup) {
      gtk_widget_grab_focus (GTK_WIDGET (owner));
      g_signal_emit_by_name (owner, "popup-closed");
    }
  }
  void close () noexcept {
    if (closed) return;
    closed = true; cancel_press (); rebuild.close (); preview_update.close (); autoscroll.close ();
    close_popup (); permanent.clear (); model.clear (); controls.clear ();
    rows.clear (); previews.clear (); icons.clear (); drag_layer.reset (); pending_single.reset ();
    gimp_docked_set_context (GIMP_DOCKED (owner), nullptr);
    image.reset (); context.reset (); supplied_context.reset ();
  }
  void build ();
  void bind_image (GimpImage *value);
  void bind_context (GimpContext *value);
  void refresh ();
  void schedule ();
  void observe (GimpContainer *container);
  void append (GimpContainer *container, int depth);
  void sync_selection ();
  int row_at (double x, double y, bool preview_only = true) const;
  std::shared_ptr<Preview> cache (GimpViewable *viewable);
  void invalidate (GimpViewable *viewable);
  void schedule_previews ();
  void update_previews ();
  void draw (cairo_t *cr);
  void draw_icon (cairo_t *cr, const char *name, int x, int y);
  void clear_drop () { drop_row = -1; gtk_widget_queue_draw (list); }
  bool choose (GimpLayer *layer, GdkModifierType state);
  void action (const std::string& command, GObject *target);
  GtkWidget *show_popup (bool creation);
  GtkWidget *show_common_popup (ObjectRef<GObject> image, ObjectRef<GObject> context,
                               ObjectRef<GObject> layer, std::uint64_t image_epoch,
                               std::uint64_t context_epoch, std::uint64_t popup_epoch);
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
  catch (...) { g_warning ("Painter layer tiles: Unknown C++ exception"); }
}
template<class F> void dispatch (gpointer data, F&& f) noexcept {
  try {
  auto *hook = static_cast<Hook *> (data);
  auto owner = hook->owner.lock ();
  auto target = hook->target;
  auto command = hook->command;
  const auto popup_epoch = hook->popup_epoch, close_epoch = hook->close_epoch;
  if (owner) use (owner.get (), [&] (Tiles& s) {
    if (popup_epoch && (s.popup_generation != popup_epoch ||
                       s.popup_close_generation != close_epoch || !s.popup_active)) return;
    f (s, target.get (), command);
  });
  } catch (const std::exception& error) { g_warning ("Painter layer callback: %s", error.what ()); }
  catch (...) { g_warning ("Painter layer callback: Unknown C++ exception"); }
}
void destroy_hook (gpointer data, GClosure *) { delete static_cast<Hook *> (data); }
void connect (Tiles& s, std::vector<Connection>& connections, GObject *emitter,
              const char *signal, GCallback callback, GObject *target = nullptr,
              const char *command = "", std::uint64_t popup_epoch = 0, std::uint64_t close_epoch = 0) {
  auto hook = std::unique_ptr<Hook> (new Hook (s.owner, target, command, popup_epoch, close_epoch));
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
void preview_invalidated (GimpViewable *viewable, gpointer data) {
  dispatch (data, [&] (Tiles& s, GObject *, const std::string&) { s.invalidate (viewable); });
}
void appearance_changed (GObject *, gpointer data) {
  dispatch (data, [] (Tiles& s, GObject *, const std::string&) { gtk_widget_queue_draw (s.list); });
}
gboolean draw_tiles (GtkWidget *, cairo_t *cr, gpointer data) {
  dispatch (data, [&] (Tiles& s, GObject *, const std::string&) { s.draw (cr); });
  return FALSE;
}
void style_changed (GtkWidget *, gpointer data) {
  dispatch (data, [] (Tiles& s, GObject *, const std::string&) {
    s.icons.clear (); gtk_widget_queue_draw (s.list);
  });
}
gboolean query_tooltip (GtkWidget *, gint x, gint y, gboolean keyboard,
                        GtkTooltip *tooltip, gpointer data) {
  bool found = false;
  dispatch (data, [&] (Tiles& s, GObject *, const std::string&) {
    int index = keyboard ? -1 : s.row_at (x, y);
    if (index < 0) return;
    auto& row = s.rows[index];
    gtk_tooltip_set_text (tooltip, gimp_object_get_name (row.layer.get ()));
    GdkRectangle rect = {row.depth * indent_width, index * tile_pitch + preview_inset,
                         preview_size, preview_size};
    gtk_tooltip_set_tip_area (tooltip, &rect); found = true;
  });
  return found;
}
gboolean press (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  bool companion = false;
  dispatch (data, [&] (Tiles& s, GObject *, const std::string&) {
    /* Only the exact classified counterpart of a consumed common-host close
     * is inert. Take the marker before any row/action lookup; unmatched input
     * retains the original list/add policy, including Filter and left input. */
    s.cancelling_press.reset ();
    auto closing = std::move (s.closing_press);
    if (!closing) return;
    const auto& previous = closing->button;
    companion = (widget == s.list || widget == s.add) &&
      (event->type == GDK_2BUTTON_PRESS || event->type == GDK_3BUTTON_PRESS) &&
      event->time == previous.time && event->button == previous.button &&
      event->window == previous.window &&
      gdk_event_get_device (reinterpret_cast<GdkEvent *> (event)) == gdk_event_get_device (closing.get ()) &&
      event->x_root == previous.x_root && event->y_root == previous.y_root;
  });
  if (companion) return TRUE;
  if (event->button != 1 && event->button != 3) return FALSE;
  if (g_getenv ("PAINTER_CANVAS_TRACE")) g_printerr ("TILE_PRESS button=%u state=%u x=%.1f y=%.1f\n", event->button,event->state,event->x,event->y);
  dispatch (data, [&] (Tiles& s, GObject *target, const std::string&) {
    ObjectRef<GObject> hit;
    if (widget == s.list) {
      int index = s.row_at (event->x, event->y);
      if (index < 0) return;
      hit = s.rows[index].layer;
      target = hit.get ();
      const int x = int (event->x) - s.rows[index].depth * indent_width;
      const int y = int (event->y) - index * tile_pitch - preview_inset;
      if (x < icon_size && y < icon_size) { s.action ("visibility", target); return; }
      if (x >= preview_size - icon_size && y >= preview_size - icon_size &&
          gimp_viewable_get_children (GIMP_VIEWABLE (target))) {
        s.action ("expand", target); return;
      }
      gtk_widget_grab_focus (GTK_WIDGET (s.owner));
    }
    if (target) {
      GList *selection = s.img () ? gimp_image_get_selected_layers (s.img ()) : nullptr;
      bool keep_multiple = !(event->state & (GDK_CONTROL_MASK | GDK_SHIFT_MASK)) &&
                           g_list_length (selection) > 1 && g_list_find (selection, target);
      auto image = s.image;
      if (!keep_multiple && !s.choose (GIMP_LAYER (target), GdkModifierType (event->state))) return;
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
    auto held = s.press_layer;
    if (widget == s.list) target = held.get ();
    if (g_getenv ("PAINTER_CANVAS_TRACE")) g_printerr ("TILE_MOTION state=%u x=%.1f y=%.1f\n",event->state,event->x,event->y);
    if (!target && event->y - s.press_y > 12) {
      s.cancel_press (); s.popup_opened = true; s.show_popup (true);
    } else if (std::abs (event->x - s.press_x) > 12 || std::abs (event->y - s.press_y) > 12) {
      s.cancel_press ();
      if (target && (event->state & GDK_BUTTON1_MASK)) {
        if (target) s.drag_layer = ObjectRef<GObject>::retain (target);
        gtk_drag_begin_with_coordinates (widget, gtk_drag_source_get_target_list (widget),
                                         GDK_ACTION_MOVE, 1, reinterpret_cast<GdkEvent *> (event), -1, -1);
      }
    }
  });
  return FALSE;
}
gboolean release (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  dispatch (data, [] (Tiles& s, GObject *, const std::string&) { s.closing_press.reset (); s.cancelling_press.reset (); });
  if (event->button != 1) return FALSE;
  dispatch (data, [&] (Tiles& s, GObject *target, const std::string&) {
    auto held = s.press_layer;
    if (widget == s.list) target = held.get ();
    const bool short_press = s.pressed && !s.popup_opened;
    auto pending = std::move (s.pending_single);
    s.cancel_press ();
    if (target && short_press && pending) s.choose (GIMP_LAYER (pending.get ()), GdkModifierType (0));
    if (widget == s.add && !target && short_press) s.action ("layers-new-last-values", nullptr);
  });
  return TRUE;
}
gboolean key (GtkWidget *, GdkEventKey *event, gpointer data) {
  bool handled = false;
  dispatch (data, [&] (Tiles& s, GObject *, const std::string&) {
    s.closing_press.reset (); s.cancelling_press.reset ();
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
    if (target) s.drag_layer = ObjectRef<GObject>::retain (target);
  });
}
void drag_data (GtkWidget *, GdkDragContext *, GtkSelectionData *selection, guint, guint, gpointer data) {
  dispatch (data, [&] (Tiles& s, GObject *target, const std::string&) {
    if (!target) target = s.drag_layer.get ();
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
    s.autoscroll.close (); s.clear_drop ();
    int index = s.row_at (x, y, false);
    if (index < 0 || gtk_selection_data_get_length (selection) != sizeof (gint) || !s.ctx ()) return;
    auto target_lease = s.rows[index].layer;
    target = target_lease.get ();
    gint id = 0; memcpy (&id, gtk_selection_data_get_data (selection), sizeof (id));
    GimpItem *item = gimp_item_get_by_id (s.ctx ()->gimp, id);
    if (!GIMP_IS_LAYER (item)) return;
    const int offset = y - index * tile_pitch;
    bool into = gimp_viewable_get_children (GIMP_VIEWABLE (target)) &&
                offset >= tile_pitch/3 && offset < 2*tile_pitch/3;
    moved = s.move (GIMP_LAYER (item), GIMP_LAYER (target), into, offset >= tile_pitch/2);
  });
  gtk_drag_finish (drag, moved, FALSE, time);
}
void drag_end (GtkWidget *, GdkDragContext *, gpointer data) {
  dispatch (data, [] (Tiles& s, GObject *, const std::string&) { s.autoscroll.close (); s.drag_layer.reset (); s.clear_drop (); });
}
gboolean drag_motion (GtkWidget *widget, GdkDragContext *drag, gint x, gint y, guint time, gpointer data) {
  bool valid = false;
  dispatch (data, [&] (Tiles& s, GObject *, const std::string&) {
    s.scroll_drag (widget, x, y);
    s.drop_row = s.row_at (x, y, false);
    if (s.drop_row >= 0) {
      const int offset = y - s.drop_row * tile_pitch;
      s.drop_into = gimp_viewable_get_children (GIMP_VIEWABLE (s.rows[s.drop_row].layer.get ())) &&
                    offset >= tile_pitch/3 && offset < 2*tile_pitch/3;
      s.drop_after = offset >= tile_pitch/2;
      valid = true;
    }
    gtk_widget_queue_draw (s.list);
  });
  gdk_drag_status (drag, valid ? GDK_ACTION_MOVE : GdkDragAction (0), time);
  return TRUE;
}
void drag_leave (GtkWidget *, GdkDragContext *, guint, gpointer data) {
  dispatch (data, [] (Tiles& s, GObject *, const std::string&) { s.autoscroll.close (); s.clear_drop (); });
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
void popup_target_removed (GimpItem *, gpointer data) {
  auto *hook = static_cast<Hook *> (data);
  auto owner = hook->owner.lock ();
  const auto request = hook->popup_epoch, close_request = hook->close_epoch;
  if (owner) use (owner.get (), [request, close_request] (Tiles& s) {
    if (s.popup_generation == request && s.popup_close_generation == close_request) s.close_popup ();
  });
}
void popup_closed (GtkPopover *, gpointer data) {
  dispatch (data, [] (Tiles& s, GObject *, const std::string&) {
    s.cancel_press ();
    s.restore_focus ();
  });
}
/* Stage provenance only while the common host emits outside cancellation.
 * Native destruction below confirms it against the still-current event; a
 * vetoed cancel or later unrelated destruction must not arm a companion. */
void common_popup_cancel (GtkWidget *widget, gpointer data) {
  dispatch (data, [widget] (Tiles& s, GObject *, const std::string&) {
    s.closing_press.reset (); s.cancelling_press.reset ();
    std::unique_ptr<GdkEvent, decltype (&gdk_event_free)> event {gtk_get_current_event (), gdk_event_free};
    if (s.popup.get () != G_OBJECT (widget) || !event || event->type != GDK_BUTTON_PRESS ||
        (event->button.button != 1 && event->button.button != 3) || !event->button.time || !event->button.window ||
        !gdk_event_get_device (event.get ())) return;
    auto *source = gtk_get_event_widget (event.get ());
    if ((source != s.list && source != s.add) || gtk_widget_get_toplevel (source) == widget) return;
    s.cancelling_press = std::move (event);
  });
}
void common_popup_cancel_finished (GtkWidget *, gpointer data) {
  auto *hook = static_cast<Hook *> (data);
  auto owner = hook->owner.lock ();
  const auto request = hook->popup_epoch, close_request = hook->close_epoch;
  if (owner) use (owner.get (), [request, close_request] (Tiles& s) {
    if (s.popup_generation == request && s.popup_close_generation == close_request)
      s.cancelling_press.reset ();
  });
}
/* Native property hosts disappear by destruction, not GtkPopover::closed.
 * Identity guards keep a retiring host from clearing a synchronously reopened one. */
void native_popup_destroyed (GtkWidget *widget, gpointer data) {
  dispatch (data, [widget] (Tiles& s, GObject *, const std::string&) {
    if (s.popup.get () != G_OBJECT (widget)) return;
    auto hold = s.popup;
    auto cancelling = std::move (s.cancelling_press);
    s.closing_press.reset ();
    std::unique_ptr<GdkEvent, decltype (&gdk_event_free)> current {gtk_get_current_event (), gdk_event_free};
    if (cancelling && current && current->type == GDK_BUTTON_PRESS &&
        current->button.time == cancelling->button.time && current->button.button == cancelling->button.button &&
        current->button.window == cancelling->button.window &&
        gdk_event_get_device (current.get ()) == gdk_event_get_device (cancelling.get ()) &&
        current->button.x_root == cancelling->button.x_root && current->button.y_root == cancelling->button.y_root)
      s.closing_press = std::move (cancelling);
    s.popup.reset ();
    s.popup_content = nullptr;
    s.popup_connections.clear ();
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
                   std::vector<Connection>& hooks, GObject *target = nullptr,
                   std::uint64_t popup_epoch = 0, std::uint64_t close_epoch = 0) {
  GtkWidget *result = gtk_button_new_with_label (label);
  gtk_widget_set_name (result, name); gtk_widget_set_can_focus (result, FALSE);
  gtk_box_pack_start (GTK_BOX (box), result, FALSE, FALSE, 0);
  connect (s, hooks, G_OBJECT (result), "clicked", G_CALLBACK (clicked), target, name, popup_epoch, close_epoch);
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
  /* One custom Cairo canvas, as in the original painter.  There are no GTK
   * row widgets or GimpView renderers: layout and hit testing share rows. */
  list = gtk_drawing_area_new ();
  gtk_widget_set_name (list, "painter-layer-canvas");
  gtk_widget_set_size_request (list, tile_pitch, tile_pitch);
  gtk_widget_set_has_tooltip (list, TRUE);
  gtk_widget_add_events (list, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK);
  gtk_container_add (GTK_CONTAINER (scroll), list);
  connect (*this, permanent, G_OBJECT (list), "draw", G_CALLBACK (draw_tiles));
  connect (*this, permanent, G_OBJECT (list), "style-updated", G_CALLBACK (style_changed));
  connect (*this, permanent, G_OBJECT (list), "query-tooltip", G_CALLBACK (query_tooltip));
  connect (*this, permanent, G_OBJECT (list), "button-press-event", G_CALLBACK (press));
  connect (*this, permanent, G_OBJECT (list), "button-release-event", G_CALLBACK (release));
  connect (*this, permanent, G_OBJECT (list), "motion-notify-event", G_CALLBACK (motion));
  connect (*this, permanent, G_OBJECT (list), "grab-broken-event", G_CALLBACK (grab_broken));
  gtk_drag_source_set (list, GdkModifierType (0), drag_targets, 1, GDK_ACTION_MOVE);
  gtk_drag_dest_set (list, GtkDestDefaults (GTK_DEST_DEFAULT_DROP), drag_targets, 1, GDK_ACTION_MOVE);
  connect (*this, permanent, G_OBJECT (list), "drag-begin", G_CALLBACK (drag_begin));
  connect (*this, permanent, G_OBJECT (list), "drag-data-get", G_CALLBACK (drag_data));
  connect (*this, permanent, G_OBJECT (list), "drag-data-received", G_CALLBACK (drag_received));
  connect (*this, permanent, G_OBJECT (list), "drag-end", G_CALLBACK (drag_end));
  connect (*this, permanent, G_OBJECT (list), "drag-motion", G_CALLBACK (drag_motion));
  connect (*this, permanent, G_OBJECT (list), "drag-leave", G_CALLBACK (drag_leave));
  connect (*this, permanent, G_OBJECT (owner), "key-press-event", G_CALLBACK (key));
  connect (*this, permanent, G_OBJECT (owner), "unmap", G_CALLBACK (unmapped));
  connect (*this, permanent, G_OBJECT (add), "grab-broken-event", G_CALLBACK (grab_broken));
  widgets.push_back (ObjectRef<GObject>::retain (G_OBJECT (list)));
  widgets.push_back (ObjectRef<GObject>::retain (G_OBJECT (add)));
  widgets.push_back (ObjectRef<GObject>::retain (G_OBJECT (scroll)));
  gtk_widget_show_all (GTK_WIDGET (owner));
}
void Tiles::bind_context (GimpContext *value) {
  if (closed) return;
  const auto generation = ++context_generation;
  if (supplied_context.get () == (value ? G_OBJECT (value) : nullptr)) return;
  const auto image_epoch = image_generation;
  auto next_source = ObjectRef<GObject>::retain (value ? G_OBJECT (value) : nullptr);
  auto image_lease = image;
  auto old_source = supplied_context;
  auto old_context = context;
  ObjectRef<GObject> next_context;
  if (value) {
    next_context = ObjectRef<GObject>::adopt (G_OBJECT (
      gimp_context_new (value->gimp, "painter-layer-tiles", value)));
    auto *next = GIMP_CONTEXT (next_context.get ());
    gimp_context_define_properties (next, GimpContextPropMask (
      GIMP_CONTEXT_PROP_MASK_ALL & ~(GIMP_CONTEXT_PROP_MASK_IMAGE | GIMP_CONTEXT_PROP_MASK_DISPLAY)), FALSE);
    gimp_context_define_properties (next, GimpContextPropMask (
      GIMP_CONTEXT_PROP_MASK_IMAGE | GIMP_CONTEXT_PROP_MASK_DISPLAY), TRUE);
    gimp_context_set_parent (next, value);
    gimp_context_set_display (next, ctx () ? gimp_context_get_display (ctx ()) : nullptr);
    gimp_context_set_image (next, img ());
  }
  cancel_press (); autoscroll.close (); close_popup ();
  if (closed || context_generation != generation || image_generation != image_epoch) return;
  preview_update.close (); previews.clear ();
  supplied_context = std::move (next_source);
  context = std::move (next_context);
  /* GimpImageEditor normally clears image when its context is removed. These
   * two original properties are independent. Retained old contexts prevent
   * finalizer reentry while the parent swaps its context and signal hooks. */
  clearing_context = !ctx ();
  gimp_docked_set_context (GIMP_DOCKED (owner), ctx ());
  clearing_context = false;
  if (closed || context_generation != generation || image_generation != image_epoch) return;
  if (value && global_action_factory)
    if (auto *factory = menus_get_global_menu_factory (value->gimp))
      gimp_editor_create_menu (GIMP_EDITOR (owner), factory, "<Layers>", "/layers-popup", owner);
  if (closed || context_generation != generation || image_generation != image_epoch) return;
  /* Same-image native assignment is a no-op; rebuild renderer contexts too. */
  refresh ();
}
void Tiles::bind_image (GimpImage *value) {
  if (closed) return;
  const auto generation = ++image_generation;
  auto next_image = ObjectRef<GObject>::retain (value ? G_OBJECT (value) : nullptr);
  cancel_press (); rebuild.close (); autoscroll.close (); close_popup ();
  /* popup-closed may destroy us or synchronously bind a newer image. */
  if (closed || image_generation != generation) return;
  preview_update.close (); previews.clear (); drop_row = -1;
  model.clear (); controls.clear (); rows.clear (); drag_layer.reset ();
  image = std::move (next_image);
  if (ctx ()) gimp_context_set_image (ctx (), value);
  if (closed || image_generation != generation) return;
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
    for (const char *signal : {"mask-changed", "expanded-changed"})
      connect (*this, model, G_OBJECT (layer), signal, G_CALLBACK (changed));
    for (const char *signal : {"name-changed", "visibility-changed", "lock-content-changed",
                              "apply-mask-changed", "edit-mask-changed", "show-mask-changed"})
      connect (*this, model, G_OBJECT (layer), signal, G_CALLBACK (appearance_changed));
    for (const char *signal : {"invalidate-preview", "size-changed"}) {
      connect (*this, model, G_OBJECT (layer), signal, G_CALLBACK (preview_invalidated));
      if (auto *mask = gimp_layer_get_mask (layer))
        connect (*this, model, G_OBJECT (mask), signal, G_CALLBACK (preview_invalidated));
    }
    if (auto *children = gimp_viewable_get_children (GIMP_VIEWABLE (layer))) observe (children);
  }
}
std::shared_ptr<Preview> Tiles::cache (GimpViewable *viewable) {
  auto& result = previews[G_OBJECT (viewable)];
  if (!result) result = std::make_shared<Preview> (viewable);
  return result;
}
void Tiles::append (GimpContainer *container, int depth) {
  for (GList *i = GIMP_LIST (container)->queue->head; i; i=i->next) {
    auto *layer = GIMP_LAYER (i->data);
    Row row; row.layer = ObjectRef<GObject>::retain (G_OBJECT (layer)); row.depth = depth;
    row.preview = cache (GIMP_VIEWABLE (layer));
    if (auto *mask = gimp_layer_get_mask (layer)) row.mask = cache (GIMP_VIEWABLE (mask));
    rows.emplace_back (std::move (row));
    auto *children = gimp_viewable_get_children (GIMP_VIEWABLE (layer));
    if (children && gimp_viewable_get_expanded (GIMP_VIEWABLE (layer))) append (children, depth+1);
  }
}
void Tiles::refresh () {
  if (closed) return;
  ++refresh_generation;
  cancel_press (); autoscroll.close (); preview_update.close (); drop_row = -1;
  rebuild.close (); model.clear (); controls.clear (); rows.clear ();
  if (img ()) {
    observe (gimp_image_get_layers (img ()));
    connect (*this, model, G_OBJECT (img ()), "selected-layers-changed", G_CALLBACK (selected));
    append (gimp_image_get_layers (img ()), 0);
  }
  /* Keep unchanged surfaces on reorder/expand, release removed layers/masks. */
  std::unordered_set<GObject *> live;
  int max_depth = 0;
  for (auto& row : rows) {
    live.insert (row.layer.get ());
    if (row.mask) live.insert (row.mask->viewable.get ());
    max_depth = std::max (max_depth, row.depth);
  }
  for (auto i = previews.begin (); i != previews.end (); )
    if (!live.count (i->first)) i = previews.erase (i); else ++i;
  gtk_widget_set_size_request (list, tile_pitch + max_depth * indent_width,
                               std::max (tile_pitch, int (rows.size ()) * tile_pitch));
  sync_selection (); schedule_previews ();
}
void Tiles::sync_selection () {
  if (!closed) gtk_widget_queue_draw (list);
}
int Tiles::row_at (double x, double y, bool preview_only) const {
  if (y < 0 || x < 0 || y >= rows.size () * tile_pitch) return -1;
  int index = int (y) / tile_pitch;
  if (preview_only && (x < rows[index].depth * indent_width ||
      x >= rows[index].depth * indent_width + preview_size ||
      int (y) % tile_pitch < preview_inset || int (y) % tile_pitch >= preview_inset + preview_size)) return -1;
  return index;
}
void Tiles::invalidate (GimpViewable *viewable) {
  if (closed) return;
  auto dirty = [&] (GimpViewable *v) {
    auto i = previews.find (G_OBJECT (v));
    if (i != previews.end ()) { i->second->dirty = true; ++i->second->revision; }
  };
  dirty (viewable);
  /* A child's pixels and a mask can change a group's composite thumbnail. */
  if (GIMP_IS_LAYER_MASK (viewable))
    viewable = GIMP_VIEWABLE (gimp_layer_mask_get_layer (GIMP_LAYER_MASK (viewable)));
  while (viewable) {
    dirty (viewable);
    viewable = gimp_viewable_get_parent (viewable);
  }
  schedule_previews ();
}
void Tiles::schedule_previews () {
  if (closed || !ctx () || preview_update.active ()) return;
  auto weak = std::make_shared<WeakRef<GObject>> (ObjectRef<GObject>::retain (G_OBJECT (owner)));
  const auto generation = BindingStore::require (G_OBJECT (owner)).generation ();
  preview_update = Source::idle (nullptr, G_PRIORITY_LOW, [weak, generation] () {
    auto object = weak->lock ();
    if (object) {
      auto *store = BindingStore::find (object.get ());
      if (store && store->accepts (generation)) store->with<Slot> ([] (Tiles& s) {
        s.preview_update.close (); s.update_previews ();
      });
    }
    return false;
  });
}
void Tiles::update_previews () {
  if (closed || !ctx ()) return;
  const auto image_epoch = image_generation, context_epoch = context_generation;
  auto context_lease = context;
  std::vector<std::shared_ptr<Preview>> pending;
  for (auto& entry : previews) if (entry.second->dirty) pending.push_back (entry.second);
  for (auto& preview : pending) {
    if (closed || image_generation != image_epoch || context_generation != context_epoch) return;
    auto *viewable = GIMP_VIEWABLE (preview->viewable.get ());
    int width = preview_size, height = preview_size;
    gimp_viewable_get_preview_size (viewable, preview_size, TRUE, TRUE, &width, &height);
    width = std::max (1, std::min (width, preview_size));
    height = std::max (1, std::min (height, preview_size));
    unsigned revision = preview->revision;
    GdkPixbuf *pixbuf = gimp_viewable_get_pixbuf (viewable, GIMP_CONTEXT (context_lease.get ()), width, height, nullptr);
    ObjectRef<GObject> fallback;
    if (!pixbuf) {
      if (auto *buffer = gimp_viewable_get_preview (viewable, GIMP_CONTEXT (context_lease.get ()), width, height, nullptr)) {
        pixbuf = gimp_temp_buf_create_pixbuf (buffer);
        fallback = ObjectRef<GObject>::adopt (G_OBJECT (pixbuf));
      }
    }
    Surface surface (pixbuf ? gimp_cairo_surface_create_from_pixbuf (pixbuf) : nullptr, cairo_surface_destroy);
    /* Preview generation may synchronously switch image/context or destroy us. */
    if (closed || image_generation != image_epoch || context_generation != context_epoch) return;
    auto current = previews.find (preview->viewable.get ());
    if (current == previews.end () || current->second != preview) continue;
    preview->surface = std::move (surface);
    preview->dirty = preview->revision != revision;
    if (preview->dirty) schedule_previews ();
  }
  gtk_widget_queue_draw (list);
}
void Tiles::draw_icon (cairo_t *cr, const char *name, int x, int y) {
  if (!name) return;
  auto entry = icons.find (name);
  if (entry == icons.end ()) {
    GError *error = nullptr;
    GdkPixbuf *pixbuf = gtk_icon_theme_load_icon (gtk_icon_theme_get_for_screen (gtk_widget_get_screen (list)),
                                                name, icon_size, GTK_ICON_LOOKUP_FORCE_SIZE, &error);
    Surface surface (pixbuf ? gimp_cairo_surface_create_from_pixbuf (pixbuf) : nullptr, cairo_surface_destroy);
    if (pixbuf) g_object_unref (pixbuf);
    g_clear_error (&error);
    entry = icons.emplace (name, std::move (surface)).first;
  }
  if (entry->second) { cairo_set_source_surface (cr, entry->second.get (), x, y); cairo_paint (cr); }
  else {
    /* A missing theme asset or failed pixbuf conversion must not silently erase
     * the affordance. Keep this fallback independent of theme/raster allocation;
     * the null cache entry still prevents repeated failed loads on every draw. */
    cairo_save (cr);
    cairo_set_source_rgba (cr, 1., 1., 1., .9);
    cairo_rectangle (cr, x + 1., y + 1., icon_size - 2., icon_size - 2.);
    cairo_fill (cr);
    cairo_set_source_rgb (cr, 0., 0., 0.);
    cairo_set_line_width (cr, 1.5);
    cairo_rectangle (cr, x + 1.5, y + 1.5, icon_size - 3., icon_size - 3.);
    cairo_move_to (cr, x + 3., y + 3.);
    cairo_line_to (cr, x + icon_size - 3., y + icon_size - 3.);
    cairo_move_to (cr, x + icon_size - 3., y + 3.);
    cairo_line_to (cr, x + 3., y + icon_size - 3.);
    cairo_stroke (cr);
    cairo_restore (cr);
  }
}
void Tiles::draw (cairo_t *cr) {
  if (closed) return;
  GtkStyleContext *style = gtk_widget_get_style_context (list);
  GdkRGBA foreground, highlight;
  gtk_style_context_get_color (style, GTK_STATE_FLAG_NORMAL, &foreground);
  if (!gtk_style_context_lookup_color (style, "theme_selected_bg_color", &highlight))
    highlight = GdkRGBA {0.20, 0.40, 0.65, 1.0};
  gtk_render_background (style, cr, 0, 0, gtk_widget_get_allocated_width (list), gtk_widget_get_allocated_height (list));
  GList *selection = img () ? gimp_image_get_selected_layers (img ()) : nullptr;
  double clip_x1, clip_y1, clip_x2, clip_y2;
  cairo_clip_extents (cr, &clip_x1, &clip_y1, &clip_x2, &clip_y2);
  for (std::size_t index = 0; index < rows.size (); ++index) {
    const auto& row = rows[index];
    int x = row.depth * indent_width, top = int (index) * tile_pitch, y = top + preview_inset;
    if (top + tile_pitch < clip_y1 || top > clip_y2) continue;
    auto *layer = GIMP_LAYER (row.layer.get ());
    if (g_list_find (selection, layer)) {
      gdk_cairo_set_source_rgba (cr, &highlight);
      cairo_rectangle (cr, x, top, tile_pitch, tile_pitch); cairo_fill (cr);
    }
    for (int depth = 0; depth < row.depth; ++depth) {
      cairo_set_source_rgb (cr, .75, .75, .75);
      cairo_rectangle (cr, depth * indent_width, top, bar_width, tile_pitch); cairo_fill (cr);
    }
    cairo_save (cr);
    cairo_rectangle (cr, x, y, preview_size, preview_size); cairo_clip (cr);
    for (int cy = 0; cy < preview_size; cy += 4) for (int cx = 0; cx < preview_size; cx += 4) {
      double shade = ((cx / 4 + cy / 4) % 2) ? .9 : .8;
      cairo_set_source_rgb (cr, shade, shade, shade);
      cairo_rectangle (cr, x + cx, y + cy, 4, 4); cairo_fill (cr);
    }
    /* The model can lose or replace its mask before the queued row rebuild.
     * A retained preview is not evidence that the layer still owns that mask. */
    auto *mask = gimp_layer_get_mask (layer);
    const bool current_mask = mask && row.mask &&
      row.mask->viewable.get () == G_OBJECT (mask);
    auto preview = current_mask && gimp_layer_get_edit_mask (layer) ? row.mask : row.preview;
    if (preview->surface) {
      int width = cairo_image_surface_get_width (preview->surface.get ());
      int height = cairo_image_surface_get_height (preview->surface.get ());
      cairo_set_source_surface (cr, preview->surface.get (), x + (preview_size-width)/2, y + (preview_size-height)/2);
      cairo_paint (cr);
    }
    cairo_restore (cr);
    draw_icon (cr, "gimp-visible", x, y);
    if (!gimp_item_get_visible (GIMP_ITEM (layer))) {
      gdk_cairo_set_source_rgba (cr, &foreground); cairo_set_line_width (cr, 1.5);
      cairo_move_to (cr, x + icon_size, y); cairo_line_to (cr, x, y + icon_size); cairo_stroke (cr);
    }
    if (current_mask) draw_icon (cr, "gimp-layer-mask", x + preview_size - icon_size, y);
    if (gimp_item_get_lock_content (GIMP_ITEM (layer))) draw_icon (cr, "gimp-lock", x, y + icon_size);
    const char *icon = gimp_viewable_get_icon_name (GIMP_VIEWABLE (layer));
    if (icon && strcmp (icon, "gimp-layer")) draw_icon (cr, icon, x, y + preview_size - icon_size);
    if (gimp_viewable_get_children (GIMP_VIEWABLE (layer))) {
      gdk_cairo_set_source_rgba (cr, &foreground);
      int tx = x + preview_size - 10, ty = y + preview_size - 10;
      if (gimp_viewable_get_expanded (GIMP_VIEWABLE (layer))) {
        cairo_move_to (cr, tx, ty); cairo_line_to (cr, tx+8, ty); cairo_line_to (cr, tx+4, ty+6);
      } else { cairo_move_to (cr, tx, ty); cairo_line_to (cr, tx+6, ty+4); cairo_line_to (cr, tx, ty+8); }
      cairo_close_path (cr); cairo_fill (cr);
    }
    cairo_set_source_rgb (cr, .7, .7, .7); cairo_set_line_width (cr, 1);
    cairo_rectangle (cr, x+.5, y+.5, preview_size-1, preview_size-1); cairo_stroke (cr);
    if (int (index) == drop_row) {
      gdk_cairo_set_source_rgba (cr, &highlight); cairo_set_line_width (cr, 3);
      if (drop_into) { cairo_rectangle (cr, x+1.5, y+1.5, preview_size-3, preview_size-3); cairo_stroke (cr); }
      else { cairo_rectangle (cr, x, top + (drop_after ? tile_pitch-4 : 0), tile_pitch, 4); cairo_fill (cr); }
    }
  }
}
bool Tiles::choose (GimpLayer *layer, GdkModifierType state) {
  if (!img () || gimp_item_get_image (GIMP_ITEM (layer)) != img ()) return false;
  const auto image_epoch = image_generation;
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
  sync_selection ();
  /* The notification may replace this selection without changing the image.
   * A retained callback target does not keep the old activation current. */
  bool accepted = !closed && image_generation == image_epoch;
  if (accepted && selection) {
    GList *current = gimp_image_get_selected_layers (img ());
    accepted = g_list_length (current) == g_list_length (selection);
    for (GList *i = selection; accepted && i; i = i->next)
      accepted = g_list_find (current, i->data) != nullptr;
  }
  g_list_free (selection);
  return accepted;
}
void Tiles::begin_press (GtkWidget *widget, GimpLayer *layer, double x, double y) {
  if (closed || !img ()) return;
  cancel_press (); press_widget = ObjectRef<GObject>::retain (G_OBJECT (widget));
  press_layer = ObjectRef<GObject>::retain (layer ? G_OBJECT (layer) : nullptr);
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
  if (closed || !img () || !ctx ()) return;
  auto image_lease=image;
  auto context_lease=context;
  const auto image_epoch = image_generation;
  const auto context_epoch = context_generation;
  const auto popup_epoch = popup_generation;
  auto current = [&] {
    return !closed && image_generation == image_epoch &&
           context_generation == context_epoch && popup_generation == popup_epoch;
  };
  if (command == "visibility" && target) {
    auto layer_lease=ObjectRef<GObject>::retain(target);
    auto *item=GIMP_ITEM (target);
    if (gimp_item_is_attached (item) && gimp_item_get_image (item) == img ()) {
      gimp_item_set_visible (item, !gimp_item_get_visible (item), TRUE); gimp_image_flush (GIMP_IMAGE (image_lease.get ()));
    }
  } else if (command == "layer-menu") {
    close_popup ();
    if (current ()) gimp_editor_popup_menu_at_pointer (GIMP_EDITOR (owner), nullptr);
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
      close_popup ();
      /* popup-closed may replace the image/context, open a newer popup, or
       * destroy the owner. Leases keep storage alive, not this action current. */
      if (current ()) entry.callback (nullptr, nullptr, owner);
      break;
    }
  }
}
GtkWidget *Tiles::show_common_popup (ObjectRef<GObject> image_hold,
                                      ObjectRef<GObject> context_hold,
                                      ObjectRef<GObject> layer_hold,
                                      std::uint64_t image_epoch,
                                      std::uint64_t context_epoch,
                                      std::uint64_t popup_epoch) {
  const auto close_epoch = popup_close_generation;
  auto content_hold = ObjectRef<GObject>::sink (G_OBJECT (gtk_box_new (GTK_ORIENTATION_VERTICAL, 3)));
  auto *content = GTK_WIDGET (content_hold.get ());
  gtk_container_set_border_width (GTK_CONTAINER (content), 8);
  GtkWidget *anchor_widget = list;
  auto anchor_hold = ObjectRef<GObject>::retain (G_OBJECT (anchor_widget));
  GdkRectangle anchor_area = {0, 0, gtk_widget_get_allocated_width (anchor_widget), tile_pitch};
  for (unsigned i = 0; i < rows.size (); ++i)
    if (rows[i].layer.get () == layer_hold.get ()) {
      anchor_area.x = rows[i].depth * indent_width;
      anchor_area.y = i * tile_pitch;
      anchor_area.width = preview_size;
      break;
    }
  /* No GTK host owns this content until every common control is complete.
   * parent-set observers may close/unmap the owner or open another request.
   * A strong widget reference alone cannot make a half-destroyed host usable. */
  auto current = [&] {
    if (closed || image_generation != image_epoch || context_generation != context_epoch ||
        popup_generation != popup_epoch || popup_close_generation != close_epoch ||
        image.get () != image_hold.get () || context.get () != context_hold.get () ||
        gtk_widget_in_destruction (anchor_widget) ||
        !gtk_widget_get_mapped (GTK_WIDGET (owner)) ||
        !gtk_widget_get_mapped (anchor_widget)) return false;
    GList *selected = gimp_image_get_selected_layers (GIMP_IMAGE (image_hold.get ()));
    return ((selected ? G_OBJECT (selected->data) : nullptr) == layer_hold.get () &&
      (!layer_hold || (gimp_item_is_attached (GIMP_ITEM (layer_hold.get ())) &&
       gimp_item_get_image (GIMP_ITEM (layer_hold.get ())) == GIMP_IMAGE (image_hold.get ()) )));
  };
  std::vector<Connection> staged_connections;
  if (layer_hold)
    connect (*this, staged_connections, layer_hold.get (), "removed", G_CALLBACK (popup_target_removed),
             nullptr, "", popup_epoch, close_epoch);
  auto content_cleanup = std::unique_ptr<GtkWidget, decltype (&gtk_widget_destroy)> (content, gtk_widget_destroy);
  auto scroller_hold = ObjectRef<GObject> ();
  GtkWidget *view = content;
  {
    scroller_hold = ObjectRef<GObject>::sink (G_OBJECT (gtk_scrolled_window_new (nullptr, nullptr)));
    auto *scroller = GTK_WIDGET (scroller_hold.get ());
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroller), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_min_content_width (GTK_SCROLLED_WINDOW (scroller), 260);
    gtk_scrolled_window_set_max_content_height (GTK_SCROLLED_WINDOW (scroller), 500);
    gtk_scrolled_window_set_propagate_natural_height (GTK_SCROLLED_WINDOW (scroller), TRUE);
    gtk_container_add (GTK_CONTAINER (scroller), content);
    view = scroller;
  }
  const char *create_labels[] = { N_("New Layer…"), N_("New Group"), N_("Duplicate"), N_("New Clone Layer"), N_("New Filter Layer…") };
  const char *create_commands[] = { "layers-new", "layers-new-group", "layers-duplicate", "layers-new-clone", "layers-new-filter" };
  for (unsigned i=0; i<G_N_ELEMENTS(create_commands); ++i)
    button (*this, content, _(create_labels[i]), create_commands[i], staged_connections, nullptr, popup_epoch, close_epoch);
  {
    auto *layer = layer_hold ? GIMP_LAYER (layer_hold.get ()) : nullptr;
    if (layer) {
      GtkWidget *scale=gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, 0, 100, 1);
      gtk_widget_set_name (scale, "painter-layer-opacity");
      gtk_range_set_value (GTK_RANGE (scale), 100*gimp_layer_get_opacity (layer));
      gtk_box_pack_start (GTK_BOX (content), gtk_label_new (_("Opacity")), FALSE, FALSE, 0);
      gtk_box_pack_start (GTK_BOX (content), scale, FALSE, FALSE, 0);
      connect (*this, staged_connections, G_OBJECT (scale), "value-changed", G_CALLBACK (opacity), nullptr, "", popup_epoch, close_epoch);
      GtkWidget *mode = gimp_layer_mode_box_new (GIMP_LAYER_MODE_CONTEXT_LAYER);
      gtk_widget_set_name (mode, "painter-layer-mode");
      gimp_layer_mode_box_set_mode (GIMP_LAYER_MODE_BOX (mode), gimp_layer_get_mode (layer));
      gtk_box_pack_start (GTK_BOX (content), mode, FALSE, FALSE, 0);
      connect (*this, staged_connections, G_OBJECT (mode), "notify::layer-mode", G_CALLBACK (mode_changed), nullptr, "", popup_epoch, close_epoch);
      auto flag = [&] (const char *label, const char *name, bool value) {
        GtkWidget *check = gtk_check_button_new_with_label (label);
        gtk_widget_set_name (check, name);
        if (!strcmp (name, "painter-layer-lock-alpha")) gtk_widget_set_sensitive (check, gimp_layer_can_lock_alpha (layer));
        if (!strcmp (name, "painter-layer-lock-content")) gtk_widget_set_sensitive (check, gimp_item_can_lock_content (GIMP_ITEM (layer)));
        if (!strcmp (name, "painter-layer-lock-position")) gtk_widget_set_sensitive (check, gimp_item_can_lock_position (GIMP_ITEM (layer)));
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), value);
        gtk_box_pack_start (GTK_BOX (content), check, FALSE, FALSE, 0);
        connect (*this, staged_connections, G_OBJECT (check), "toggled", G_CALLBACK (flag_toggled), nullptr, name, popup_epoch, close_epoch);
      };
      flag (_("Lock Alpha"), "painter-layer-lock-alpha", gimp_layer_get_lock_alpha (layer));
      flag (_("Lock Pixels"), "painter-layer-lock-content", gimp_item_get_lock_content (GIMP_ITEM (layer)));
      flag (_("Lock Position"), "painter-layer-lock-position", gimp_item_get_lock_position (GIMP_ITEM (layer)));
      if (gimp_layer_get_mask (layer)) {
        flag (_("Edit Mask"), "painter-layer-mask-edit", gimp_layer_get_edit_mask (layer));
        flag (_("Show Mask"), "painter-layer-mask-show", gimp_layer_get_show_mask (layer));
        flag (_("Apply Mask"), "painter-layer-mask-apply", gimp_layer_get_apply_mask (layer));
      }
      button (*this, content, _("Edit Layer Attributes…"), "layers-edit-attributes", staged_connections, nullptr, popup_epoch, close_epoch);
      if (GIMP_IS_CLONE_LAYER (layer)) button (*this, content, _("Clone Source…"), "layers-edit-clone", staged_connections, nullptr, popup_epoch, close_epoch);

      button (*this, content, _("Raise Layer"), "layers-raise", staged_connections, nullptr, popup_epoch, close_epoch);
      button (*this, content, _("Lower Layer"), "layers-lower", staged_connections, nullptr, popup_epoch, close_epoch);
      button (*this, content, _("Delete Layer"), "layers-delete", staged_connections, nullptr, popup_epoch, close_epoch);
    }
  }
  button (*this, content, _("All Layer Actions…"), "layer-menu", staged_connections, nullptr, popup_epoch, close_epoch);
  GimpContainer *presets=gimp_data_factory_get_container (GIMP_CONTEXT (context_hold.get ())->gimp->layer_preset_factory);
  for (GList *i=GIMP_LIST (presets)->queue->head; i; i=i->next)
    button (*this, content, gimp_object_get_name (i->data), "preset", staged_connections, G_OBJECT (i->data), popup_epoch, close_epoch);
  if (!current ()) return nullptr;
  gtk_widget_show_all (view);
  if (!current ()) return nullptr;
  GtkWidget *created = gimp_popover_new (view);
  if (!created) return nullptr;
  auto hold = ObjectRef<GObject>::retain (G_OBJECT (created));
  auto host_cleanup = std::unique_ptr<GtkWidget, decltype (&gtk_widget_destroy)> (created, gtk_widget_destroy);
  if (!current () || gtk_widget_in_destruction (created)) return nullptr;
  gtk_widget_set_name (created, "painter-layer-properties-popup");
  if (!current () || gtk_widget_in_destruction (created) ||
      !gtk_widget_is_ancestor (view, created)) return nullptr;
  connect (*this, staged_connections, G_OBJECT (created), "cancel",
           G_CALLBACK (common_popup_cancel), nullptr, "", popup_epoch, close_epoch);
  /* The after handler must survive default destruction clearing the normal
   * connection vector. Its weak-owner closure belongs to the host itself. */
  g_signal_connect_data (created, "cancel", G_CALLBACK (common_popup_cancel_finished),
                         new Hook (owner, nullptr, "", popup_epoch, close_epoch), destroy_hook, G_CONNECT_AFTER);
  connect (*this, staged_connections, G_OBJECT (created), "destroy", G_CALLBACK (native_popup_destroyed));
  popup = hold;
  popup_content = content;
  popup_connections = std::move (staged_connections);
  content_cleanup.release ();
  host_cleanup.release ();
  popup_active = true;
  gimp_popover_show_at (created, anchor_widget, &anchor_area);
  if (!current () || popup.get () != hold.get () || gtk_widget_in_destruction (created) ||
      !gtk_widget_is_ancestor (view, created)) {
    if (popup.get () == hold.get ()) close_popup ();
    else gtk_widget_destroy (created);
    return nullptr;
  }
  return created;
}

GtkWidget *Tiles::show_popup (bool creation) {
  if (closed || !img () || !ctx ()) return nullptr;
  const auto image_epoch = image_generation;
  const auto context_epoch = context_generation;
  const auto popup_epoch = ++popup_generation;
  close_popup ();
  /* Closing may synchronously open a newer popup even on the same image.
   * A live owner and unchanged image do not keep this request current. */
  if (closed || !img () || !ctx () || image_generation != image_epoch ||
      popup_generation != popup_epoch) return nullptr;
  auto image_hold = image;
  auto context_hold = context;
  GList *selected_layers = gimp_image_get_selected_layers (img ());
  auto layer_hold = ObjectRef<GObject>::retain (selected_layers ? G_OBJECT (selected_layers->data) : nullptr);
  const bool native_filter = !creation && layer_hold && GIMP_IS_FILTER_LAYER (layer_hold.get ());
  if (!creation && !native_filter)
    return show_common_popup (image_hold, context_hold, layer_hold,
                              image_epoch, context_epoch, popup_epoch);
  auto content_hold = ObjectRef<GObject>::sink (G_OBJECT (gtk_box_new (GTK_ORIENTATION_VERTICAL, 3)));
  auto *content = GTK_WIDGET (content_hold.get ());
  gtk_container_set_border_width (GTK_CONTAINER (content), 8);
  GtkWidget *anchor_widget = native_filter ? list : (creation ? add : GTK_WIDGET (owner));
  auto anchor_hold = ObjectRef<GObject>::retain (G_OBJECT (anchor_widget));
  GdkRectangle anchor_area = {0, 0, gtk_widget_get_allocated_width (anchor_widget), tile_pitch};
  if (native_filter) for (unsigned i = 0; i < rows.size (); ++i)
    if (rows[i].layer.get () == layer_hold.get ()) {
      anchor_area.x = rows[i].depth * indent_width;
      anchor_area.y = i * tile_pitch;
      anchor_area.width = preview_size;
      break;
    }
  GtkWidget *created = native_filter ?
    painter_filter_layer_popup_new (GIMP_IMAGE (image_hold.get ()), GIMP_LAYER (layer_hold.get ()),
                                    GIMP_CONTEXT (context_hold.get ()), anchor_widget, content) :
    gtk_popover_new (anchor_widget);
  if (!created) return nullptr;
  auto hold = ObjectRef<GObject>::sink (G_OBJECT (created));
  auto current = [&] {
    if (closed || image_generation != image_epoch || context_generation != context_epoch ||
        popup_generation != popup_epoch || image.get () != image_hold.get () ||
        context.get () != context_hold.get () || gtk_widget_in_destruction (anchor_widget) ||
        gtk_widget_in_destruction (created)) return false;
    GList *selected = gimp_image_get_selected_layers (GIMP_IMAGE (image_hold.get ()));
    return !native_filter || (selected && selected->data == layer_hold.get () &&
      gimp_item_is_attached (GIMP_ITEM (layer_hold.get ())) &&
      gimp_item_get_image (GIMP_ITEM (layer_hold.get ())) == GIMP_IMAGE (image_hold.get ()));
  };
  if (!current ()) { gtk_widget_destroy (created); return nullptr; }
  popup = hold;
  auto *widget = created;
  popup_content = content;
  connect (*this, popup_connections, G_OBJECT (widget), native_filter ? "destroy" : "closed",
           native_filter ? G_CALLBACK (native_popup_destroyed) : G_CALLBACK (popup_closed));
  gtk_widget_set_name (widget, creation ? "painter-layer-create-popup" : "painter-layer-properties-popup");
  if (!current () || popup.get () != hold.get ()) return nullptr;
  if (!native_filter) {
    gtk_popover_set_position (GTK_POPOVER (widget), GTK_POS_LEFT);
    GtkWidget *scroller = gtk_scrolled_window_new (nullptr, nullptr);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroller), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_min_content_width (GTK_SCROLLED_WINDOW (scroller), 260);
    gtk_scrolled_window_set_max_content_height (GTK_SCROLLED_WINDOW (scroller), 500);
    gtk_scrolled_window_set_propagate_natural_height (GTK_SCROLLED_WINDOW (scroller), TRUE);
    gtk_container_add (GTK_CONTAINER (scroller), popup_content);
    gtk_container_add (GTK_CONTAINER (widget), scroller);
  }
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
  gtk_widget_show_all (widget);
  if (!current () || popup.get () != hold.get ()) return nullptr;
  if (native_filter) painter_filter_layer_popup_show_at (widget, anchor_widget, &anchor_area);
  else gtk_popover_popup (GTK_POPOVER (widget));
  return current () && popup.get () == hold.get () ? widget : nullptr;
}
void close_widget (GtkWidget *widget) {
  try { if (auto *store=BindingStore::find (G_OBJECT (widget))) store->close (); }
  catch (...) { g_warning ("Could not close painter layer tiles"); }
  GTK_WIDGET_CLASS (gimp_painter_layer_tiles_parent_class)->destroy (widget);
}
void set_image (GimpImageEditor *editor, GimpImage *image) {
  bool entered = false;
  use (G_OBJECT (editor), [&] (Tiles& s) {
    entered = true;
    if (s.clearing_context && !image) {
      s.clearing_context = false; // only the parent's implicit clear, not nested intent
      return;
    }
    const auto image_epoch = s.image_generation;
    auto image_lease = ObjectRef<GObject>::retain (image ? G_OBJECT (image) : nullptr);
    GIMP_IMAGE_EDITOR_CLASS (gimp_painter_layer_tiles_parent_class)->set_image (editor, image);
    /* The parent changes sensitivity, which can synchronously close the widget
     * or route it to another image. Keep both parent and binding in agreement. */
    if (!s.closed && s.image_generation == image_epoch &&
        gimp_image_editor_get_image (editor) == image)
      s.bind_image (image);
  });
  /* Teardown must still disconnect the parent's image/flush handler, but a
   * retained, destroyed widget must never attach a new image to that parent. */
  if (!entered && !image)
    GIMP_IMAGE_EDITOR_CLASS (gimp_painter_layer_tiles_parent_class)->set_image (editor, nullptr);
}
void set_property (GObject *object, guint id, const GValue *value, GParamSpec *pspec) {
  if (pspec->owner_type != GIMP_TYPE_PAINTER_LAYER_TILES) {
    G_OBJECT_CLASS (gimp_painter_layer_tiles_parent_class)->set_property (object, id, value, pspec);
    return;
  }
  boundary_void (nullptr, [&] {
    auto *binding = BindingStore::find (object);
    if (!binding || binding->state () != BindingStore::State::active) return;
    binding->with<Slot> ([&] (Tiles& s) {
      if (id == PROP_CONTEXT) s.bind_context (static_cast<GimpContext *> (g_value_get_object (value)));
      else if (id == PROP_IMAGE) {
        ++s.image_generation; // even a same-image request supersedes older configuration
        if (!s.closed) {
          auto *next = static_cast<GimpImage *> (g_value_get_object (value));
          auto *editor = GIMP_IMAGE_EDITOR (object);
          /* The parent publishes its image before popup-close callbacks. A
           * nested request for that same image must still finish Tiles binding. */
          if (gimp_image_editor_get_image (editor) == next && s.img () != next)
            s.bind_image (next);
          else gimp_image_editor_set_image (editor, next);
        }
      } else G_OBJECT_WARN_INVALID_PROPERTY_ID (object, id, pspec);
    });
  });
}
void get_property (GObject *object, guint id, GValue *value, GParamSpec *pspec) {
  if (pspec->owner_type != GIMP_TYPE_PAINTER_LAYER_TILES) {
    G_OBJECT_CLASS (gimp_painter_layer_tiles_parent_class)->get_property (object, id, value, pspec);
    return;
  }
  boundary_void (nullptr, [&] {
    auto *binding = BindingStore::find (object);
    if (!binding || binding->state () != BindingStore::State::active) return;
    binding->with<Slot> ([&] (Tiles& s) {
      if (id == PROP_CONTEXT) g_value_set_object (value, s.supplied_context.get ());
      else if (id == PROP_IMAGE) g_value_set_object (value, s.image.get ());
      else G_OBJECT_WARN_INVALID_PROPERTY_ID (object, id, pspec);
    });
  });
}
}
static void gimp_painter_layer_tiles_class_init (GimpPainterLayerTilesClass *klass) {
  auto *object_class = G_OBJECT_CLASS (klass);
  object_class->set_property = set_property;
  object_class->get_property = get_property;
  g_object_class_install_property (object_class, PROP_IMAGE,
    g_param_spec_object ("image", "Image", "Independently bound image", GIMP_TYPE_IMAGE,
      GParamFlags (G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (object_class, PROP_CONTEXT,
    g_param_spec_object ("context", "Context", "Supplied context (owned separately from action context)", GIMP_TYPE_CONTEXT,
      GParamFlags (G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS)));
  g_signal_new ("popup-closed", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST, 0,
                nullptr, nullptr, nullptr, G_TYPE_NONE, 0);
  GTK_WIDGET_CLASS (klass)->destroy=close_widget;
  GIMP_IMAGE_EDITOR_CLASS (klass)->set_image=set_image;
}
static void gimp_painter_layer_tiles_init (GimpPainterLayerTiles *owner) {
  owner->binding_failed = TRUE;
  GError *error = nullptr;
  const bool initialized = boundary<bool> (&error, false, [&] {
    auto& store = BindingStore::ensure (G_OBJECT (owner));
    store.emplace<Slot> (owner);
    /* Native signals may fire while widgets are assembled. Keep callbacks
     * disabled until the complete tree and its owned connections are ready. */
    store.initialize<Slot> ([] (Tiles& s) { s.build (); });
    store.activate ();
    return true;
  });
  if (initialized) owner->binding_failed = FALSE;
  else {
    /* This also retires a partially built tree's sources and connections. The
     * native container owns child widgets and disposes them normally. */
    boundary_void (nullptr, [&] {
      if (auto *store = BindingStore::find (G_OBJECT (owner))) store->close ();
    });
    g_warning ("Cannot construct painter layer tiles: %s", error->message);
    g_clear_error (&error);
  }
}
namespace {
GimpPainterLayerTiles *checked_tiles (GimpPainterLayerTiles *tiles) noexcept {
  if (!tiles->binding_failed) return tiles;
  /* GtkWidget factories return a floating reference. Sink that reference before
   * disposing a failed instance so neither a leak nor a floating-finalize
   * diagnostic can escape this failure path. */
  g_object_ref_sink (tiles);
  gtk_widget_destroy (GTK_WIDGET (tiles));
  g_object_unref (tiles);
  return nullptr;
}
}
GType gimp_layer_tile_view_get_type (void) {
  return gimp_painter_layer_tiles_get_type ();
}
GimpLayerTileView *gimp_layer_tile_view_new (void) {
  return boundary<GimpLayerTileView *> (nullptr, nullptr, [] {
    return static_cast<GimpLayerTileView *> (checked_tiles (GIMP_PAINTER_LAYER_TILES (
      g_object_new (GIMP_TYPE_PAINTER_LAYER_TILES, nullptr))));
  });
}
GtkWidget *gimp_painter_layer_tiles_new (GimpContext *context, GimpImage *image) {
  g_return_val_if_fail (GIMP_IS_CONTEXT (context), nullptr);
  g_return_val_if_fail (!image || GIMP_IS_IMAGE (image), nullptr);
  return boundary<GtkWidget *> (nullptr, nullptr, [&] {
    return GTK_WIDGET (checked_tiles (GIMP_PAINTER_LAYER_TILES (
      g_object_new (GIMP_TYPE_PAINTER_LAYER_TILES, "context", context, "image", image, nullptr))));
  });
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
