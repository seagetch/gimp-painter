/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gegl.h>
#include <gtk/gtk.h>
extern "C" {
#include "libgimpbase/gimpbase.h"
#include "libgimpwidgets/gimpwidgets.h"
#include "core/core-types.h"
#include "core/gimptooloptions.h"
#include "core/gimptoolinfo.h"
#include "core/gimpobject.h"
#include "widgets-types.h"
#include "gimpviewablebutton.h"
#include "gimpbrusheditor.h"
#include "gimpdynamicseditor.h"
#include "gimpdataeditor.h"
#include "gimpdocked.h"
#include "menus/menus.h"
#include "display/display-types.h"
#include "display/gimpdisplayshell.h"
#include "display/gimppaintercanvasui.h"
#include "gimppaintermybrusheditor.h"
#include "gimppaintercompactoptions.h"
#include "gimp-intl.h"
}
#include "painter/binding-store.hpp"
#include "painter/connection.hpp"
#include <algorithm>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
using namespace GimpPainter;
namespace GimpPainter {
template<> struct TypeTraits<GtkWidget> { static GType type () noexcept { return GTK_TYPE_WIDGET; } };
}
namespace {
struct Presentation;
struct Slot : SlotSpec<GtkWidget, Presentation> {};
struct Saved {
  ObjectRef<GObject> widget;
  WeakRef<GObject> parent;
  gint position = -1, width = -1, height = -1;
  gint left = 0, top = 0, columns = 1, rows = 1;
  bool grid = false;
  gboolean expand = FALSE, fill = FALSE, no_show_all = FALSE;
  guint padding = 0;
  GtkPackType pack = GTK_PACK_START;
  GtkOrientation orientation = GTK_ORIENTATION_VERTICAL;
  GtkAlign valign = GTK_ALIGN_FILL;
  bool oriented = false, moved = false, label = false;
  std::shared_ptr<bool> alive = std::make_shared<bool> (true);
};
struct Group {
  std::string id;
  ObjectRef<GObject> row, button, popup, box, editor;
};
struct Presentation {
  GtkWidget *owner;
  WeakRef<GObject> options;
  std::vector<std::unique_ptr<Saved>> saved;
  std::vector<std::unique_ptr<Group>> groups;
  std::vector<Connection> connections;
  Connection destroy;
  bool enabled = false, closed = false, changing = false;
  guint64 epoch = 0;
  GtkOrientation layout = GTK_ORIENTATION_HORIZONTAL;
  GtkOrientation original_orientation = GTK_ORIENTATION_VERTICAL;
  explicit Presentation (GtkWidget *widget) : owner (widget) {}
  void close () noexcept;
  void restore () noexcept;
  void popdown () noexcept;
  void populate (GtkWidget *popup);
  Saved& remember (GtkWidget *widget);
  void move (GtkWidget *widget, GtkWidget *box);
  void orient (GtkWidget *widget, GtkOrientation value);
  Group& group (GtkWidget *box, const char *id, const char *label, GtkWidget *toggle = nullptr);
  void build (GtkWidget *box, const std::string& tool);
  bool set (GimpToolOptions *value, bool active, GtkOrientation orientation);
};
struct Hook {
  WeakRef<GObject> owner;
  guint64 epoch;
  std::shared_ptr<bool> alive;
  WeakRef<GObject> target;
  Hook (Presentation& p, Saved *s = nullptr, GObject *t = nullptr)
    : owner (ObjectRef<GObject>::retain (G_OBJECT (p.owner))), epoch (p.epoch),
      alive (s ? s->alive : nullptr), target (ObjectRef<GObject>::retain (t)) {}
};
template<class F> void use (GObject *owner, F&& fn) noexcept {
  try {
    auto *s = BindingStore::find (owner);
    if (s && s->state () == BindingStore::State::active) s->with<Slot> (std::forward<F> (fn));
  } catch (const std::exception& e) { g_warning ("Painter compact options: %s", e.what ()); }
}
template<class F> void dispatch (gpointer data, F&& fn) noexcept {
  auto *hook = static_cast<Hook*> (data);
  auto owner = hook->owner.lock ();
  if (!owner) return;
  const auto epoch = hook->epoch;
  auto target = hook->target.lock ();

  use (owner.get (), [&] (Presentation& p) {
    if (!p.closed && p.epoch == epoch) fn (p, nullptr, target.get ());
  });
}
void free_hook (gpointer data, GClosure*) { delete static_cast<Hook*> (data); }
void widget_destroyed (GtkWidget*, gpointer data) {
  auto alive = static_cast<Hook*> (data)->alive;
  if (alive) *alive = false;
}
void owner_destroyed (GtkWidget *owner, gpointer) {
  try { if (auto *s = BindingStore::find (G_OBJECT (owner))) s->close (); }
  catch (const std::exception& e) { g_warning ("Painter compact destroy: %s", e.what ()); }
}
void toggle_changed (GtkToggleButton *toggle, gpointer data) {
  const bool active = gtk_toggle_button_get_active (toggle);
  dispatch (data, [active] (Presentation&, Saved*, GObject *target) {
    if (target && !gtk_widget_in_destruction (GTK_WIDGET (target))) gtk_widget_set_sensitive (GTK_WIDGET (target), active);
  });
}
void popup_shown (GtkWidget *popup, gpointer data) {
  dispatch (data, [popup] (Presentation& p, Saved*, GObject*) { p.populate (popup); });
}
void popup_closed (GtkPopover*, gpointer data) {
  dispatch (data, [] (Presentation& p, Saved*, GObject*) {
    auto *shell = gtk_widget_get_ancestor (p.owner, GIMP_TYPE_DISPLAY_SHELL);
    if (shell) {
      gimp_painter_canvas_ui_pointer (GIMP_DISPLAY_SHELL (shell), 0, 0, TRUE);
      gtk_widget_grab_focus (GIMP_DISPLAY_SHELL (shell)->canvas);
    }
  });
}
void connect (Presentation& p, GObject *emitter, const char *signal, GCallback callback,
              Saved *saved = nullptr, GObject *target = nullptr) {
  auto hook = std::unique_ptr<Hook> (new Hook (p, saved, target));
  auto c = Connection::connect (ObjectRef<GObject>::retain (emitter), signal, callback, hook.get (), free_hook);
  hook.release (); p.connections.push_back (std::move (c));
}
/* This is the existing native property identity used by widget search/blink;
 * no private model or extra qdata protocol is introduced by this adapter. */
const char *property (GtkWidget *widget) {
  return static_cast<const char*> (g_object_get_data (G_OBJECT (widget), "gimp-widget-property-name"));
}
std::vector<GtkWidget*> children (GtkWidget *widget) {
  std::vector<GtkWidget*> result;
  if (GTK_IS_CONTAINER (widget)) {
    GList *list = gtk_container_get_children (GTK_CONTAINER (widget));
    for (GList *i = list; i; i = i->next) result.push_back (GTK_WIDGET (i->data));
    g_list_free (list);
  }
  return result;
}
bool contains (GtkWidget *widget, const char *name) {
  if (g_strcmp0 (property (widget), name) == 0) return true;
  if (GTK_IS_FRAME (widget)) {
    auto *label = gtk_frame_get_label_widget (GTK_FRAME (widget));
    if (label && contains (label, name)) return true;
  }
  for (auto *child : children (widget)) if (contains (child, name)) return true;
  return false;
}
bool any (GtkWidget *widget, std::initializer_list<const char*> names) {
  for (auto *name : names) if (contains (widget, name)) return true;
  return false;
}
GtkWidget *named_descendant (GtkWidget *widget, const char *name) {
  if (g_strcmp0 (gtk_widget_get_name (widget), name) == 0) return widget;
  for (auto *child : children (widget)) if (auto *found = named_descendant (child, name)) return found;
  return nullptr;
}
GtkWidget *property_descendant (GtkWidget *widget, const char *name) {
  if (g_strcmp0 (property (widget), name) == 0) return widget;
  for (auto *child : children (widget)) if (auto *found = property_descendant (child, name)) return found;
  return nullptr;
}
GtkWidget *resource_button (GtkWidget *widget) {
  for (auto *child : children (widget)) if (GIMP_IS_VIEWABLE_BUTTON (child)) return child;
  return nullptr;
}
Saved& Presentation::remember (GtkWidget *widget) {
  for (auto& item : saved) if (item->widget.get () == G_OBJECT (widget)) return *item;
  auto item = std::unique_ptr<Saved> (new Saved);
  item->widget = ObjectRef<GObject>::retain (G_OBJECT (widget));
  item->no_show_all = gtk_widget_get_no_show_all (widget);
  item->valign = gtk_widget_get_valign (widget);
  gtk_widget_get_size_request (widget, &item->width, &item->height);
  if (GTK_IS_ORIENTABLE (widget)) {
    item->oriented = true;
    item->orientation = gtk_orientable_get_orientation (GTK_ORIENTABLE (widget));
  }
  if (auto *parent = gtk_widget_get_parent (widget)) {
    item->parent = WeakRef<GObject> (ObjectRef<GObject>::retain (G_OBJECT (parent)));
    if (GTK_IS_FRAME (parent) && gtk_frame_get_label_widget (GTK_FRAME (parent)) == widget) item->label = true;
    else if (GTK_IS_GRID (parent)) {
      item->grid = true;
      gtk_container_child_get (GTK_CONTAINER (parent), widget, "left-attach", &item->left, "top-attach", &item->top,
                               "width", &item->columns, "height", &item->rows, nullptr);
    } else if (GTK_IS_BOX (parent)) {
      gtk_container_child_get (GTK_CONTAINER (parent), widget, "position", &item->position, nullptr);
      gtk_box_query_child_packing (GTK_BOX (parent), widget, &item->expand, &item->fill, &item->padding, &item->pack);
    }
  }
  auto *result = item.get (); saved.push_back (std::move (item));
  connect (*this, G_OBJECT (widget), "destroy", G_CALLBACK (widget_destroyed), result);
  return *result;
}
void Presentation::move (GtkWidget *widget, GtkWidget *box) {
  auto& state = remember (widget); state.moved = true;
  if (auto *parent = gtk_widget_get_parent (widget)) {
    if (GTK_IS_FRAME (parent) && gtk_frame_get_label_widget (GTK_FRAME (parent)) == widget)
      gtk_frame_set_label_widget (GTK_FRAME (parent), nullptr);
    else gtk_container_remove (GTK_CONTAINER (parent), widget);
  }
  if (closed || !*state.alive) throw Error (GIMP_PAINTER_ERROR_CLOSED, "Options closed during presentation change");
  gtk_box_pack_start (GTK_BOX (box), widget, FALSE, FALSE, 0);
}
void Presentation::orient (GtkWidget *widget, GtkOrientation value) {
  if (widget != owner) remember (widget);
  gtk_orientable_set_orientation (GTK_ORIENTABLE (widget), value);
}
Group& Presentation::group (GtkWidget *box, const char *id, const char *label, GtkWidget *toggle) {
  for (auto& g : groups) if (g->id == id && gtk_widget_get_parent (GTK_WIDGET (g->row.get ())) == box) return *g;
  auto g = std::unique_ptr<Group> (new Group);
  g->id = id;
  g->row = ObjectRef<GObject>::sink (G_OBJECT (gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0)));
  gtk_widget_set_valign (GTK_WIDGET (g->row.get ()), GTK_ALIGN_CENTER);
  g->button = ObjectRef<GObject>::sink (G_OBJECT (gtk_menu_button_new ()));
  g->popup = ObjectRef<GObject>::sink (G_OBJECT (gtk_popover_new (GTK_WIDGET (g->button.get ()))));
  g->box = ObjectRef<GObject>::sink (G_OBJECT (gtk_box_new (GTK_ORIENTATION_VERTICAL, 4)));
  gtk_widget_set_name (GTK_WIDGET (g->button.get ()), (std::string ("painter-compact-") + id).c_str ());
  gtk_widget_set_tooltip_text (GTK_WIDGET (g->button.get ()), label);
  if (toggle) {
    move (toggle, GTK_WIDGET (g->row.get ()));
    gtk_container_add (GTK_CONTAINER (g->button.get ()), gtk_image_new_from_icon_name ("pan-down-symbolic", GTK_ICON_SIZE_MENU));
    gtk_widget_set_sensitive (GTK_WIDGET (g->button.get ()), gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (toggle)));
    connect (*this, G_OBJECT (toggle), "toggled", G_CALLBACK (toggle_changed), nullptr, g->button.get ());
  } else gtk_button_set_label (GTK_BUTTON (g->button.get ()), label);
  auto *scroll = gtk_scrolled_window_new (nullptr, nullptr);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_min_content_width (GTK_SCROLLED_WINDOW (scroll), 340);
  gtk_scrolled_window_set_max_content_height (GTK_SCROLLED_WINDOW (scroll), 420);
  gtk_scrolled_window_set_propagate_natural_height (GTK_SCROLLED_WINDOW (scroll), TRUE);
  gtk_container_set_border_width (GTK_CONTAINER (g->box.get ()), 8);
  gtk_container_add (GTK_CONTAINER (scroll), GTK_WIDGET (g->box.get ()));
  gtk_container_add (GTK_CONTAINER (g->popup.get ()), scroll);
  gtk_menu_button_set_popover (GTK_MENU_BUTTON (g->button.get ()), GTK_WIDGET (g->popup.get ()));
  connect (*this, g->popup.get (), "show", G_CALLBACK (popup_shown));
  connect (*this, g->popup.get (), "closed", G_CALLBACK (popup_closed));
  gtk_box_pack_start (GTK_BOX (g->row.get ()), GTK_WIDGET (g->button.get ()), FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (box), GTK_WIDGET (g->row.get ()), FALSE, FALSE, 0);
  gtk_widget_show_all (GTK_WIDGET (g->row.get ()));
  gtk_widget_show (GTK_WIDGET (g->box.get ())); gtk_widget_show (scroll);
  auto *result = g.get (); groups.push_back (std::move (g)); return *result;
}
void Presentation::build (GtkWidget *box, const std::string& tool) {
  orient (box, layout);
  if (closed) return;
  auto original = children (box);
  for (auto *widget : original) remember (widget);
  for (auto *widget : original) {
    remember (widget);
    if (closed) return;
    /* Keep native dynamic visibility authoritative under Canvas show_all(). */
    gtk_widget_set_no_show_all (widget, TRUE);
    gtk_widget_set_valign (widget, GTK_ALIGN_CENTER);
    const char *id = nullptr, *title = nullptr;
    GtkWidget *toggle = nullptr;
    const bool paint = contains (box, "opacity");
    if (GIMP_IS_PAINTER_MYBRUSH_EDITOR (widget)) {
      // Pinned Basic toolbar fields. The size-reset handler in the pinned
      // source was empty; do not manufacture a resource-changing reset.
      std::vector<GtkWidget*> primary;
      for (const char *key : {"opaque", "radius-logarithmic", "slow-tracking", "hardness"})
        if (auto *field = named_descendant (widget, key)) primary.push_back (gtk_widget_get_parent (field));
      for (auto *row : primary) remember (row);
      for (auto *row : primary) { gtk_widget_set_valign (row, GTK_ALIGN_CENTER); move (row, box); }
      id = "mypaint"; title = _("MyPaint Editor");
    }
    else if (GTK_IS_BOX (widget) && contains (widget, "fixed-center")) { id = "rectangle"; title = _("Selection details…"); }
    else if (GTK_IS_BOX (widget) && (contains (widget, "opacity") || (tool == "gimp-text-tool" && contains (widget, "font-size")))) {
      build (widget, tool); continue;
    }
    else if (resource_button (widget)) {
      /* Keep the native resource thumbnail popup inline. Text/name/edit and
       * brush geometry remain available in its adjacent details popup. */
      id = paint ? "brush" : tool == "gimp-text-tool" ? "font" : "resource";
      title = paint ? _("Brush") : tool == "gimp-text-tool" ? _("Font") : _("Resource");
      auto *button = resource_button (widget);
      auto& g = group (box, id, title);
      gint at = 0; gtk_container_child_get (GTK_CONTAINER (box), widget, "position", &at, nullptr);
      gtk_box_reorder_child (GTK_BOX (box), GTK_WIDGET (g.row.get ()), at + 1);
      for (auto *part : children (widget)) if (part != button) move (part, GTK_WIDGET (g.box.get ()));
      continue;
    }
    else if (GTK_IS_FRAME (widget) && GTK_IS_TOGGLE_BUTTON (gtk_frame_get_label_widget (GTK_FRAME (widget)))) {
      toggle = gtk_frame_get_label_widget (GTK_FRAME (widget));
      const char *prop = property (toggle);
      if (prop) { id = prop; title = gtk_button_get_label (GTK_BUTTON (toggle)); }
    }
    if (!id && paint && any (widget, {"brush-aspect-ratio", "brush-angle", "brush-spacing", "brush-hardness", "brush-force", "brush-lock-to-view"})) { id = "brush"; title = _("Brush"); }
    if (!id && tool == "gimp-bucket-fill-tool" && !any (widget, {"opacity", "paint-mode"})) { id = "details"; title = _("Details…"); }
    if (!id && (tool == "gimp-clone-tool" || tool == "gimp-perspective-clone-tool") && any (widget, {"clone-type", "sample-merged", "align-mode"})) { id = "clone"; title = _("Options"); }
    if (!id && tool == "gimp-convolve-tool" && any (widget, {"type", "rate"})) { id = "convolve"; title = _("Convolve options…"); }
    if (!id && tool == "gimp-dodge-burn-tool" && any (widget, {"type", "mode", "exposure"})) { id = "details"; title = _("Details…"); }
    if (!id && any (box, {"interpolation", "clip"}) && !any (widget, {"type", "direction", "flip-type"})) { id = "transform"; title = _("Transformation details…"); }
    if (!id && tool == "gimp-ink-tool" && any (widget, {"size-sensitivity", "tilt-sensitivity", "vel-sensitivity"})) { id = "sensitivity"; title = _("Sensitivity"); }
    if (!id && tool == "gimp-ink-tool" && any (widget, {"blob-type", "blob-aspect", "blob-angle"})) { id = "blob"; title = _("Ink Blob"); }
    if (!id && tool == "gimp-text-tool" && !GIMP_IS_BUSY_BOX (widget) && !any (widget, {"font-size", "use-editor", "antialias"}) && !resource_button (widget)) { id = "details"; title = _("Details…"); }
    if (!id && tool == "gimp-foreground-select-tool" && !any (widget, {"operation", "antialias", "feather", "contiguous"})) { id = "details"; title = _("Details…"); }
    if (id) {
      if (tool == "gimp-text-tool" && !std::strcmp (id, "details"))
        if (auto *color = property_descendant (widget, "foreground")) move (color, box);
      auto& g = group (box, id, title ? title : id, toggle);
      if (children (GTK_WIDGET (g.box.get ())).empty ()) {
        gint at = 0; gtk_container_child_get (GTK_CONTAINER (box), widget, "position", &at, nullptr);
        gtk_box_reorder_child (GTK_BOX (box), GTK_WIDGET (g.row.get ()), at);
      }
      move (widget, GTK_WIDGET (g.box.get ()));
    } else {
      if (GTK_IS_FRAME (widget)) {
        auto *child = gtk_bin_get_child (GTK_BIN (widget));
        if (GTK_IS_BOX (child)) orient (child, layout);
      }
      /* Native reset/link buttons stay next to their original spin scale. */
      if (any (widget, {"paint-mode", "opacity", "brush-size", "rate", "flow", "threshold", "size", "tilt-angle", "offset"})) {
        auto& s = remember (widget);
        const int minimum = contains (widget, "paint-mode") ? 220 : contains (widget, "brush-size") ? 210 : 160;
        gtk_widget_set_size_request (widget, std::max (s.width, minimum), s.height);
      }
    }
  }
}
void Presentation::populate (GtkWidget *popup) {
  auto context = options.lock ();
  if (!context) return;
  const guint64 current = epoch;
  std::string id;
  for (auto& g : groups) if (g->popup.get () == G_OBJECT (popup) && !g->editor) id = g->id;
  GtkWidget *widget = id == "brush" ? gimp_brush_editor_new (GIMP_CONTEXT (context.get ()), menus_get_global_menu_factory (GIMP_CONTEXT (context.get ())->gimp)) :
                      id == "dynamics-enabled" ? gimp_dynamics_editor_new (GIMP_CONTEXT (context.get ()), menus_get_global_menu_factory (GIMP_CONTEXT (context.get ())->gimp)) : nullptr;
  if (!widget) return;
  auto editor = ObjectRef<GObject>::sink (G_OBJECT (widget));
  if (closed || !enabled || epoch != current) { gtk_widget_destroy (widget); return; }
  gimp_data_editor_set_edit_active (GIMP_DATA_EDITOR (widget), TRUE);
  gtk_widget_set_name (widget, id == "brush" ? "painter-compact-brush-editor" : "painter-compact-dynamics-editor");
  for (auto& g : groups) if (g->popup.get () == G_OBJECT (popup)) {
    g->editor = editor;
    gtk_box_pack_start (GTK_BOX (g->box.get ()), widget, FALSE, FALSE, 0);
    gtk_widget_show (widget); return;
  }
  gtk_widget_destroy (widget);
}
void Presentation::popdown () noexcept {
  auto snapshot = std::vector<ObjectRef<GObject>> ();
  for (auto& g : groups) snapshot.push_back (g->popup);
  for (auto& popup : snapshot) gtk_popover_popdown (GTK_POPOVER (popup.get ()));
}
void Presentation::restore () noexcept {
  if (!enabled && saved.empty ()) return;
  enabled = false; ++epoch;
  auto old_connections = std::move (connections);
  /* Detach every borrowed control before destroying any popup. Parent refs
   * and child refs keep native action targets alive during synchronous reentry. */
  auto old = std::move (saved);
  for (auto it = old.rbegin (); it != old.rend (); ++it) {
    auto& s = **it; auto *w = GTK_WIDGET (s.widget.get ());
    if (!*s.alive) continue;
    if (s.moved) {
      if (auto *parent = gtk_widget_get_parent (w)) gtk_container_remove (GTK_CONTAINER (parent), w);
      auto parent_ref = s.parent.lock ();
      auto *parent = parent_ref ? GTK_WIDGET (parent_ref.get ()) : nullptr;
      bool parent_alive = parent != owner || !closed;
      for (auto& candidate : old) if (candidate->widget.get () == G_OBJECT (parent)) parent_alive = *candidate->alive;
      if (parent && parent_alive && !gtk_widget_in_destruction (parent)) {
        if (s.label) gtk_frame_set_label_widget (GTK_FRAME (parent), w);
        else if (s.grid) gtk_grid_attach (GTK_GRID (parent), w, s.left, s.top, s.columns, s.rows);
        else if (GTK_IS_BOX (parent)) {
          if (s.pack == GTK_PACK_END) gtk_box_pack_end (GTK_BOX (parent), w, s.expand, s.fill, s.padding);
          else gtk_box_pack_start (GTK_BOX (parent), w, s.expand, s.fill, s.padding);
        }
      }
    }
    if (closed) { gtk_widget_destroy (w); continue; }
    if (s.oriented) gtk_orientable_set_orientation (GTK_ORIENTABLE (w), s.orientation);
    gtk_widget_set_size_request (w, s.width, s.height);
    gtk_widget_set_no_show_all (w, s.no_show_all);
    gtk_widget_set_valign (w, s.valign);
  }
  auto old_groups = std::move (groups);
  for (auto& g : old_groups) { gtk_widget_destroy (GTK_WIDGET (g->popup.get ())); gtk_widget_destroy (GTK_WIDGET (g->row.get ())); }
  /* Temporary buttons must be gone before restoring absolute sibling indices. */
  for (auto& item : old) {
    auto& s = *item;
    auto parent = s.parent.lock ();
    if (*s.alive && s.position >= 0 && parent && GTK_IS_BOX (parent.get ()) &&
        gtk_widget_get_parent (GTK_WIDGET (s.widget.get ())) == GTK_WIDGET (parent.get ()))
      gtk_box_reorder_child (GTK_BOX (parent.get ()), GTK_WIDGET (s.widget.get ()), s.position);
  }
  if (!closed) gtk_orientable_set_orientation (GTK_ORIENTABLE (owner), original_orientation);
  options.reset ();
}
void Presentation::close () noexcept {
  if (closed) return;
  closed = true;
  if (!changing) restore ();
  destroy.close ();
}
bool Presentation::set (GimpToolOptions *value, bool active, GtkOrientation orientation) {
  if (closed || changing) return false;
  if (enabled && active && options.lock ().get () == G_OBJECT (value) && layout == orientation) return true;
  changing = true;
  struct Guard { bool& flag; ~Guard () { flag = false; } } guard {changing};
  restore ();
  if (!active || closed) return !closed;
  options = WeakRef<GObject> (ObjectRef<GObject>::retain (G_OBJECT (value)));
  original_orientation = gtk_orientable_get_orientation (GTK_ORIENTABLE (owner));
  layout = orientation; enabled = true; ++epoch;
  try { build (owner, value->tool_info ? gimp_object_get_name (value->tool_info) : ""); }
  catch (...) { restore (); throw; }
  if (closed) restore ();
  return !closed;
}
}
extern "C" gboolean gimp_painter_compact_options_set (GtkWidget *gui, GimpToolOptions *options,
                                                       gboolean enabled, GtkOrientation orientation, GError **error) {
  return boundary<gboolean> (error, FALSE, [&] {
    if (!GTK_IS_BOX (gui) || !GIMP_IS_TOOL_OPTIONS (options) || gtk_widget_in_destruction (gui))
      throw Error (GIMP_PAINTER_ERROR_WRONG_TYPE, "Compact options require the live registered options box");
    auto *store = BindingStore::find (G_OBJECT (gui));
    if (!store) {
      auto& s = BindingStore::ensure (G_OBJECT (gui)); s.emplace<Slot> (gui); s.activate (); store = &s;
      s.with<Slot> ([&] (Presentation& p) {
        p.destroy = Connection::connect (ObjectRef<GObject>::retain (G_OBJECT (gui)), "destroy", G_CALLBACK (owner_destroyed), nullptr, nullptr);
      });
    }
    return store->with<Slot> ([&] (Presentation& p) { return p.set (options, enabled, orientation); });
  });
}
extern "C" gboolean gimp_painter_compact_options_get (GtkWidget *gui) {
  bool result = false; if (GTK_IS_WIDGET (gui)) use (G_OBJECT (gui), [&] (Presentation& p) { result = p.enabled; }); return result;
}
extern "C" void gimp_painter_compact_options_popdown (GtkWidget *gui) {
  if (GTK_IS_WIDGET (gui)) use (G_OBJECT (gui), [] (Presentation& p) { p.popdown (); });
}
