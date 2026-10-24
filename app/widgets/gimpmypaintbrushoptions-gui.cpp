/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gegl.h>
#include <gtk/gtk.h>
extern "C" {
#include "libgimpbase/gimpbase.h"
#include "libgimpmath/gimpmath.h"
#include "libgimpwidgets/gimpwidgets.h"
#include "core/core-types.h"
#include "widgets-types.h"
#include "core/gimp.h"
#include "core/gimpcontext.h"
#include "core/gimpcontainer.h"
#include "core/gimpdatafactory.h"
#include "core/gimppaintermybrush.h"
#include "gimpcontainereditor.h"
#include "gimpcontainericonview.h"
#include "gimpcontainertreestore.h"
#include "gimpcontainerview.h"
#include "gimpview.h"
#include "popupper.h"
#include "gimpviewrenderer.h"
#include "gimpmypaintbrushoptions-gui.h"
#include "paint/painter-mypaint-surface/gimp-painter-options.h"
#include "painter/gimp-painter-binding.h"
#include "gimp-intl.h"
}
#include "painter/binding-store.hpp"
#include "painter/connection.hpp"
#include "painter/resources.hpp"
#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <vector>

using namespace GimpPainter;
namespace {
enum { HISTORY_INDEX, HISTORY_PREVIEW, HISTORY_NAME, HISTORY_COLUMNS };
using Ref = ObjectRef<GObject>;

struct Selector {
  GtkWidget *owner;                 // native button owns the only binding slot
  Ref context, options, container, render_context;
  Ref thumbnail, popup, body, notebook, grid, history, message;
  std::vector<Connection> connections;
  std::uint64_t epoch = 0, history_epoch = 0;
  guint64 source_seen = 0;
  unsigned updating = 0;
  bool closed = false, refreshing = false, identity_pending = false;
  bool opening = false;
  std::uint64_t presentation = 0;

  explicit Selector (GtkWidget *button) : owner (button) {}
  GimpContext *source () const { return GIMP_CONTEXT (context.get ()); }
  GimpContext *canonical () const { return GIMP_CONTEXT (options.get ()); }
  GimpPainterMybrushOptions *model () const
  { return GIMP_PAINTER_MYBRUSH_OPTIONS (options.get ()); }
  void close () noexcept {
    if (closed) return;
    closed = true; ++epoch; ++history_epoch; ++presentation;
    if (body) {
      auto *content = GTK_WIDGET (body.get ());
      if (auto *parent = gtk_widget_get_parent (content))
        gtk_container_remove (GTK_CONTAINER (parent), content);
    }
    connections.clear ();
    // Each signal owns only a weak button reference. No widget/model retains
    // the controller, and no new model is allocated by this presentation.
    auto old_popup = std::move (popup);
    if (old_popup) gtk_widget_destroy (GTK_WIDGET (old_popup.get ()));
    if (body) gtk_widget_destroy (GTK_WIDGET (body.get ()));
    body.reset (); thumbnail.reset (); notebook.reset (); grid.reset ();
    history.reset (); message.reset ();
    container.reset (); options.reset (); context.reset (); render_context.reset ();
  }
};
struct SelectorSlot : SlotSpec<GObject, Selector> {
  static GType owner_type () { return GTK_TYPE_BUTTON; }
};
struct Updating {
  Selector& selector;
  explicit Updating (Selector& s) : selector (s) { ++selector.updating; }
  ~Updating () { --selector.updating; }
};
struct Event {
  GObject *emitter = nullptr;
  GimpPainterMybrush *brush = nullptr;
  GtkTreePath *path = nullptr;
};
struct Callback {
  WeakRef<GObject> owner;
  std::uint64_t generation;
  std::function<void (Selector&, const Event&)> run;
};

BindingStore& store (GtkWidget *owner)
{ return BindingStore::require (G_OBJECT (owner)); }
bool alive (Selector& i) {
  if (i.closed || !i.options || !i.context ||
      !i.source ()->gimp || !i.canonical ()->gimp) return false;
  auto *binding = BindingStore::find (i.options.get ());
  return binding && binding->state () == BindingStore::State::active;
}
bool current (Selector& i, std::uint64_t epoch)
{ return alive (i) && i.epoch == epoch; }
void status (Selector& i, const char *text) {
  auto label = i.message;
  if (!i.closed && label) gtk_label_set_text (GTK_LABEL (label.get ()), text ? text : "");
}
void dispatch (gpointer data, const Event& event) noexcept {
  // A callback can disconnect itself while executing; retain its callable and
  // button storage before entering any native emission or borrowed slot.
  boundary_void (nullptr, [&] {
    auto *cb = static_cast<Callback *> (data);
    auto owner = cb->owner.lock ();
    if (!owner) return;
    const auto generation = cb->generation;
    auto function = cb->run;
    auto *binding = BindingStore::find (owner.get ());
    if (!binding || !binding->accepts (generation)) return;
    try {
      binding->with<SelectorSlot> ([&] (Selector& i) {
        if (!alive (i)) { binding->close (); return; }
        function (i, event);
        if (!i.closed && !alive (i)) binding->close ();
      });
    } catch (const std::exception& error) {
      if (binding->accepts (generation))
        binding->with<SelectorSlot> ([&] (Selector& i) {
          if (!alive (i)) binding->close ();
          else status (i, error.what ());
        });
    } catch (...) {
      if (binding->accepts (generation))
        binding->with<SelectorSlot> ([&] (Selector& i) {
          if (!alive (i)) binding->close ();
          else status (i, _("The brush selection could not be updated."));
        });
    }
  });
}
void simple_cb (GObject *emitter, gpointer data)
{ dispatch (data, Event{emitter, nullptr, nullptr}); }
void object_cb (GObject *emitter, gpointer, gpointer data)
{ dispatch (data, Event{emitter, nullptr, nullptr}); }
void selection_cb (GimpContext *context, GimpPainterMybrush *brush, gpointer data)
{ dispatch (data, Event{G_OBJECT (context), brush, nullptr}); }
void name_cb (GimpContext *context, GimpContextPropType property, gpointer data) {
  if (property == GIMP_CONTEXT_PROP_PAINTER_MYBRUSH)
    dispatch (data, Event{G_OBJECT (context), nullptr, nullptr});
}
void cursor_cb (GtkTreeView *view, gpointer data) {
  GtkTreeModel *model = nullptr;
  GtkTreeIter row;
  GtkTreePath *path = nullptr;
  if (gtk_tree_selection_get_selected (gtk_tree_view_get_selection (view), &model, &row))
    path = gtk_tree_model_get_path (model, &row);
  dispatch (data, Event{G_OBJECT (view), nullptr, path});
  if (path) gtk_tree_path_free (path);
}
void model_row_cb (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *, gpointer data)
{ dispatch (data, Event{G_OBJECT (model), nullptr, path}); }
void free_callback (gpointer data, GClosure *)
{ delete static_cast<Callback *> (data); }
void connect (Selector& i, GObject *emitter, const char *signal, GCallback callback,
              std::function<void (Selector&, const Event&)> function) {
  std::unique_ptr<Callback> data (new Callback {
    WeakRef<GObject> (Ref::retain (G_OBJECT (i.owner))),
    store (i.owner).generation (), std::move (function)
  });
  auto connection = Connection::connect (Ref::retain (emitter), signal, callback,
                                         data.get (), free_callback);
  data.release ();
  i.connections.push_back (std::move (connection));
}
Ref widget (GtkWidget *value, const char *name) {
  auto owned = Ref::sink (G_OBJECT (value));
  gtk_widget_set_name (value, name);
  return owned;
}
void refresh_identity (Selector& i);
void refresh_history (Selector& i);

void route_selection (Selector& i, GimpContext *origin, GimpPainterMybrush *emitted) {
  if (!alive (i) || (origin != i.source () && origin != i.canonical ())) return;
  // Ignore an outer signal whose resource was superseded by a nested emission.
  if (gimp_context_get_painter_mybrush (origin) != emitted) return;
  const auto revision = origin->painter_mybrush_revision;
  const auto epoch = ++i.epoch;
  auto desired = Ref::retain (G_OBJECT (emitted));
  auto source = i.context, options = i.options;
  auto *target = origin == GIMP_CONTEXT (source.get ())
               ? GIMP_CONTEXT (options.get ()) : GIMP_CONTEXT (source.get ());
  i.source_seen = i.source ()->painter_mybrush_revision;
  if (target != origin && gimp_context_get_painter_mybrush (target) != emitted)
    gimp_context_set_painter_mybrush (target,
      reinterpret_cast<GimpPainterMybrush *> (desired.get ()));
  // Echoes and newer selections increment the epoch. Their own callback owns
  // the resulting view refresh; an older stack must never overwrite them.
  if (!current (i, epoch) || origin->painter_mybrush_revision != revision ||
      gimp_context_get_painter_mybrush (origin) != emitted) return;
  i.source_seen = i.source ()->painter_mybrush_revision;
  refresh_identity (i);
}
bool reconcile_source (Selector& i) {
  if (!alive (i)) return false;
  if (i.source () == i.canonical ()) return true;
  // A deliberate user action can arrive inside a context notify handler,
  // before painter-mybrush-changed. Reconcile that changed source once; never
  // call ref_for_context from a repaint, popup-open or dismissal.
  if (i.source_seen == i.source ()->painter_mybrush_revision) return true;
  const auto revision = i.source ()->painter_mybrush_revision;
  auto desired = Ref::retain (G_OBJECT (gimp_context_get_painter_mybrush (i.source ())));
  route_selection (i, i.source (), reinterpret_cast<GimpPainterMybrush *> (desired.get ()));
  return alive (i) && i.source ()->painter_mybrush_revision == revision &&
    G_OBJECT (gimp_context_get_painter_mybrush (i.source ())) == desired.get () &&
    G_OBJECT (gimp_context_get_painter_mybrush (i.canonical ())) == desired.get ();
}
void refresh_identity_once (Selector& i) {
  if (!alive (i)) return;
  const auto epoch = i.epoch;
  auto selected = Ref::retain (G_OBJECT (gimp_context_get_painter_mybrush (i.canonical ())));
  auto thumbnail = i.thumbnail, grid = i.grid;
  const char *resource_name = selected ? gimp_object_get_name (selected.get ()) : nullptr;
  const std::string name = resource_name ? resource_name : _("No brush selected");
  Updating updating (i);
  // Native GimpViewRenderer observes invalidate-preview and size-changed,
  // including set_icon(), independently of settings-changed.
  gimp_view_set_viewable (GIMP_VIEW (thumbnail.get ()),
                          selected ? GIMP_VIEWABLE (selected.get ()) : nullptr);
  if (!current (i, epoch) || i.identity_pending) return;
  gtk_widget_set_tooltip_text (i.owner, name.c_str ());
  if (!current (i, epoch) || i.identity_pending) return;
  auto native = Ref::retain (G_OBJECT (GIMP_CONTAINER_EDITOR (grid.get ())->view));
  auto *view = GIMP_CONTAINER_ICON_VIEW (native.get ());
  auto icons = Ref::retain (G_OBJECT (view->view));
  auto rows = Ref::retain (G_OBJECT (view->model));
  auto *icon_view = GTK_ICON_VIEW (icons.get ());
  using Path = std::unique_ptr<GtkTreePath, decltype (&gtk_tree_path_free)>;
  Path path (nullptr, gtk_tree_path_free);
  GtkTreeIter row;
  if (selected)
    for (gboolean valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (rows.get ()), &row);
         valid; valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (rows.get ()), &row)) {
      GimpViewRenderer *renderer = nullptr;
      gtk_tree_model_get (GTK_TREE_MODEL (rows.get ()), &row,
                          GIMP_CONTAINER_TREE_STORE_COLUMN_RENDERER, &renderer, -1);
      auto held = Ref::adopt (G_OBJECT (renderer));
      if (renderer && G_OBJECT (renderer->viewable) == selected.get ()) {
        path.reset (gtk_tree_model_get_path (GTK_TREE_MODEL (rows.get ()), &row));
        break;
      }
    }
  auto *selected_paths = gtk_icon_view_get_selected_items (icon_view);
  const bool matches = path ? selected_paths && !selected_paths->next &&
    gtk_tree_path_compare (path.get (), static_cast<GtkTreePath *> (selected_paths->data)) == 0
    : selected_paths == nullptr;
  g_list_free_full (selected_paths, +[] (gpointer value) {
    gtk_tree_path_free (static_cast<GtkTreePath *> (value));
  });
  if (matches) return;
  using Row = std::unique_ptr<GtkTreeRowReference, decltype (&gtk_tree_row_reference_free)>;
  Row target (path ? gtk_tree_row_reference_new (GTK_TREE_MODEL (rows.get ()), path.get ())
                   : nullptr, gtk_tree_row_reference_free);
  // Native set_1_selected is a compound unselect/select/cursor operation with
  // no reentry checks. Keep its real GtkIconView/model, but check after each
  // step: a nested selection or destroy must stop the outer stale operation.
  gtk_icon_view_unselect_all (icon_view);
  if (!current (i, epoch) || i.identity_pending) return;
  if (target) {
    // Row references track insert/reorder by identity while native emissions
    // run. A path copied before unselect can otherwise name a different row.
    path.reset (gtk_tree_row_reference_get_path (target.get ()));
    if (!path) { i.identity_pending = true; return; }
    gtk_icon_view_select_path (icon_view, path.get ());
    if (!current (i, epoch) || i.identity_pending) return;
    path.reset (gtk_tree_row_reference_get_path (target.get ()));
    if (!path) { i.identity_pending = true; return; }
    gtk_icon_view_set_cursor (icon_view, path.get (), nullptr, FALSE);
    if (!current (i, epoch) || i.identity_pending) return;
    path.reset (gtk_tree_row_reference_get_path (target.get ()));
    if (!path) { i.identity_pending = true; return; }
    gtk_icon_view_scroll_to_path (icon_view, path.get (), FALSE, 0.0, 0.0);
  }
}
void refresh_identity (Selector& i) {
  if (!alive (i)) return;
  if (i.refreshing) { i.identity_pending = true; return; }
  struct Refreshing {
    Selector& selector;
    explicit Refreshing (Selector& s) : selector (s) { selector.refreshing = true; }
    ~Refreshing () { selector.refreshing = false; }
  } refreshing (i);
  do {
    i.identity_pending = false;
    const auto epoch = i.epoch;
    refresh_identity_once (i);
    if (!alive (i)) return;
    if (i.epoch != epoch) i.identity_pending = true;
  } while (i.identity_pending);
}
void choose_preset (Selector& i, const Event&) {
  if (i.updating) return;
  auto grid = i.grid, options = i.options, container = i.container;
  GList *items = nullptr;
  gimp_container_view_get_selected (GIMP_CONTAINER_EDITOR (grid.get ())->view, &items);
  Ref selected;
  if (items && !items->next) selected = Ref::retain (G_OBJECT (items->data));
  g_list_free (items);
  if (!selected || !GIMP_IS_PAINTER_MYBRUSH (selected.get ()) ||
      !gimp_container_have (GIMP_CONTAINER (container.get ()), GIMP_OBJECT (selected.get ()))) return;
  if (!reconcile_source (i) ||
      !gimp_container_have (GIMP_CONTAINER (container.get ()), GIMP_OBJECT (selected.get ()))) return;
  if (gimp_context_get_painter_mybrush (GIMP_CONTEXT (options.get ())) !=
      reinterpret_cast<GimpPainterMybrush *> (selected.get ()))
    gimp_context_set_painter_mybrush (GIMP_CONTEXT (options.get ()),
                                    GIMP_PAINTER_MYBRUSH (selected.get ()));
  if (alive (i)) refresh_identity (i);
}
void refresh_history (Selector& i) {
  if (!alive (i)) return;
  const auto revision = ++i.history_epoch;
  auto options = i.options, history = i.history;
  auto *model = GIMP_PAINTER_MYBRUSH_OPTIONS (options.get ());
  // Build an independent view snapshot, then swap once. Existing rows keep
  // append-only history indices, so duplicate names never alias one another.
  auto list = Ref::adopt (G_OBJECT (gtk_list_store_new (HISTORY_COLUMNS,
                                 G_TYPE_UINT, GDK_TYPE_PIXBUF, G_TYPE_STRING)));
  const guint count = gimp_painter_mybrush_options_history_size (model);
  bool damaged_preview = false;
  for (guint index = 0; index < count; ++index) {
    String name (gimp_painter_mybrush_options_history_name (model, index));
    GError *error = nullptr;
    auto preview = Ref::adopt (G_OBJECT (
      gimp_painter_mybrush_options_history_preview (model, index, &error)));
    damaged_preview = damaged_preview || error != nullptr;
    g_clear_error (&error);
    if (!alive (i) || i.history_epoch != revision) return;
    Ref scaled;
    if (preview) {
      auto *pixbuf = GDK_PIXBUF (preview.get ());
      const double scale = std::min (48.0 / gdk_pixbuf_get_width (pixbuf),
                                     48.0 / gdk_pixbuf_get_height (pixbuf));
      scaled = Ref::adopt (G_OBJECT (gdk_pixbuf_scale_simple (pixbuf,
        std::max (1, int (gdk_pixbuf_get_width (pixbuf) * scale)),
        std::max (1, int (gdk_pixbuf_get_height (pixbuf) * scale)), GDK_INTERP_BILINEAR)));
    }
    GtkTreeIter row;
    gtk_list_store_append (GTK_LIST_STORE (list.get ()), &row);
    gtk_list_store_set (GTK_LIST_STORE (list.get ()), &row,
      HISTORY_INDEX, index, HISTORY_PREVIEW, scaled.get (),
      HISTORY_NAME, name ? name.get () : _("Unnamed brush"), -1);
  }
  if (!alive (i) || i.history_epoch != revision) return;
  // A restore can append the outgoing dirty draft and refresh this model.
  // Keep the append-only history identity under the cursor so the next keyboard
  // move continues from that row, without restoring it again during the swap.
  guint cursor_index = count;
  GtkTreePath *cursor = nullptr;
  auto *view = GTK_TREE_VIEW (history.get ());
  gtk_tree_view_get_cursor (view, &cursor, nullptr);
  auto previous = Ref::retain (G_OBJECT (gtk_tree_view_get_model (view)));
  GtkTreeIter old_row;
  if (cursor && previous && gtk_tree_model_get_iter (GTK_TREE_MODEL (previous.get ()), &old_row, cursor))
    gtk_tree_model_get (GTK_TREE_MODEL (previous.get ()), &old_row, HISTORY_INDEX, &cursor_index, -1);
  if (cursor) gtk_tree_path_free (cursor);
  Updating updating (i);
  gtk_tree_view_set_model (view, GTK_TREE_MODEL (list.get ()));
  if (!alive (i) || i.history_epoch != revision ||
      gtk_tree_view_get_model (view) != GTK_TREE_MODEL (list.get ())) return;
  if (cursor_index < count) {
    auto *path = gtk_tree_path_new_from_indices (cursor_index, -1);
    gtk_tree_view_set_cursor (view, path, nullptr, FALSE);
    gtk_tree_path_free (path);
  }
  if (alive (i) && i.history_epoch == revision)
    status (i, damaged_preview ? _("Some saved previews are unavailable.") : "");
}
void choose_history (Selector& i, const Event& event) {
  if (i.updating || !event.path) return;
  auto history = i.history, options = i.options;
  auto list = Ref::retain (G_OBJECT (gtk_tree_view_get_model (GTK_TREE_VIEW (history.get ()))));
  GtkTreeIter row;
  if (!list || !gtk_tree_model_get_iter (GTK_TREE_MODEL (list.get ()), &row, event.path)) return;
  guint index = 0;
  gtk_tree_model_get (GTK_TREE_MODEL (list.get ()), &row, HISTORY_INDEX, &index, -1);
  if (!reconcile_source (i)) return;
  GError *error = nullptr;
  const auto success = gimp_painter_mybrush_options_restore_history (
                        GIMP_PAINTER_MYBRUSH_OPTIONS (options.get ()), index, &error);
  if (!success)
    throw Error (GIMP_PAINTER_ERROR_INVALID_STATE,
                 take_error_message (error, "Saved brush could not be restored").c_str ());
  g_clear_error (&error);
  if (alive (i)) refresh_identity (i);
}
void preview_cell (GtkTreeViewColumn *, GtkCellRenderer *cell, GtkTreeModel *model,
                   GtkTreeIter *row, gpointer) {
  GdkPixbuf *preview = nullptr;
  gtk_tree_model_get (model, row, HISTORY_PREVIEW, &preview, -1);
  // The native neutral brush icon is the only fallback. Never borrow the
  // mutable source icon or generate a fake draft resource for history.
  if (preview) g_object_set (cell, "icon-name", nullptr, "pixbuf", preview, nullptr);
  else g_object_set (cell, "pixbuf", nullptr, "icon-name", "gimp-tool-mypaint-brush", nullptr);
  g_clear_object (&preview);
}
void render_row (Selector& i, const Event& event) {
  if (!event.path || !i.render_context) return;
  GtkTreeIter row;
  auto model = Ref::retain (event.emitter), context = i.render_context;
  if (!gtk_tree_model_get_iter (GTK_TREE_MODEL (model.get ()), &row, event.path)) return;
  GimpViewRenderer *renderer = nullptr;
  gtk_tree_model_get (GTK_TREE_MODEL (model.get ()), &row,
                      GIMP_CONTAINER_TREE_STORE_COLUMN_RENDERER, &renderer, -1);
  auto held = Ref::adopt (G_OBJECT (renderer));
  if (renderer && renderer->context != GIMP_CONTEXT (context.get ()))
    gimp_view_renderer_set_context (renderer, GIMP_CONTEXT (context.get ()));
}
void dismiss (Selector& i, const Event&) {
  ++i.presentation;
  auto popup = i.popup;
  if (popup) gtk_widget_destroy (GTK_WIDGET (popup.get ()));
}
// The retained content belongs to the selector, not to any ephemeral host.
// Detach before the popup's default cancellation/destruction destroys children.
void detach_popup (Selector& i, const Event& event, bool retiring) {
  if (i.popup.get () != event.emitter) return;
  auto popup = i.popup, body = i.body;
  if (retiring) { i.popup.reset (); ++i.presentation; }
  if (body) {
    auto *content = GTK_WIDGET (body.get ());
    if (auto *parent = gtk_widget_get_parent (content))
      gtk_container_remove (GTK_CONTAINER (parent), content);
  }
}
void open_popup (Selector& i, const Event&) {
  if (i.opening || !gtk_widget_get_mapped (i.owner)) return;
  struct Opening {
    Selector& selector;
    explicit Opening (Selector& s) : selector (s) { s.opening = true; }
    ~Opening () { selector.opening = false; }
  } opening (i);
  auto anchor = Ref::retain (G_OBJECT (i.owner));
  dismiss (i, Event{});
  const auto revision = i.presentation;
  auto valid = [&] { return alive (i) && i.presentation == revision &&
    gtk_widget_get_mapped (GTK_WIDGET (anchor.get ())) &&
    !gtk_widget_in_destruction (GTK_WIDGET (anchor.get ())); };
  refresh_identity (i);
  if (!valid ()) return;
  refresh_history (i);
  if (!valid ()) return;
  i.connections.erase (std::remove_if (i.connections.begin (), i.connections.end (),
    [] (const Connection& connection) { return !connection.connected (); }), i.connections.end ());
  auto body = i.body;
  auto carrier = Ref::sink (G_OBJECT (gtk_box_new (GTK_ORIENTATION_VERTICAL, 0)));
  auto *created = gimp_painter_popup_new (GTK_WIDGET (carrier.get ()));
  if (!created) return;
  auto popup = Ref::retain (G_OBJECT (created));
  if (!valid ()) { gtk_widget_destroy (created); return; }
  i.popup = popup;
  connect (i, popup.get (), "cancel", G_CALLBACK (simple_cb),
    [] (Selector& s, const Event& e) { detach_popup (s, e, false); });
  connect (i, popup.get (), "confirm", G_CALLBACK (simple_cb),
    [] (Selector& s, const Event& e) { detach_popup (s, e, false); });
  connect (i, popup.get (), "destroy", G_CALLBACK (simple_cb),
    [] (Selector& s, const Event& e) { detach_popup (s, e, true); });
  gtk_widget_set_name (created, "painter-mypaint-brush-popup");
  if (!valid () || i.popup.get () != popup.get ()) return;
  if (gtk_widget_get_parent (GTK_WIDGET (body.get ()))) { gtk_widget_destroy (created); return; }
  gtk_box_pack_start (GTK_BOX (carrier.get ()), GTK_WIDGET (body.get ()), TRUE, TRUE, 0);
  if (!valid () || i.popup.get () != popup.get ()) return;
  // Showing a tree can establish its initial cursor; this is presentation sync,
  // not a user request to restore a history snapshot.
  Updating updating (i);
  gimp_painter_popup_show_over (created, GTK_WIDGET (anchor.get ()));
}
void disconnect_context (Selector& i, const Event&)
{ store (i.owner).close (); }
void destroy_button (GtkWidget *widget, gpointer)
{ gimp_painter_binding_close (G_OBJECT (widget), nullptr); }

void build (Selector& i) {
  auto *canonical = i.canonical ();
  i.container = Ref::retain (G_OBJECT (gimp_data_factory_get_container (
                                      canonical->gimp->painter_mybrush_factory)));
  if (!i.container) throw Error (GIMP_PAINTER_ERROR_INVALID_STATE, "Painter brush factory is unavailable");
  // The application default context already exists; it is used only for
  // rendering, never for selection or drafts. It also outlives disposal of an
  // individual supplied context/options model.
  i.render_context = Ref::retain (G_OBJECT (gimp_get_default_context (canonical->gimp)));
  if (!i.render_context) throw Error (GIMP_PAINTER_ERROR_INVALID_STATE, "Brush rendering context is unavailable");
  auto *render_context = GIMP_CONTEXT (i.render_context.get ());
  i.source_seen = i.source ()->painter_mybrush_revision;
  i.thumbnail = widget (gimp_view_new_full_by_types (render_context, GIMP_TYPE_VIEW,
    GIMP_TYPE_PAINTER_MYBRUSH, 32, 32, 1, FALSE, FALSE, FALSE),
    "painter-mypaint-brush-thumbnail");
  gtk_container_add (GTK_CONTAINER (i.owner), GTK_WIDGET (i.thumbnail.get ()));
  i.body = widget (gtk_box_new (GTK_ORIENTATION_VERTICAL, 6), "painter-mypaint-selector-body");
  auto body = i.body;
  gtk_container_set_border_width (GTK_CONTAINER (body.get ()), 6);
  i.notebook = widget (gtk_notebook_new (), "painter-mypaint-brush-tabs");
  gtk_box_pack_start (GTK_BOX (body.get ()), GTK_WIDGET (i.notebook.get ()), TRUE, TRUE, 0);
  i.grid = widget (GTK_WIDGET (g_object_new (GIMP_TYPE_CONTAINER_EDITOR,
    "view-type", GIMP_VIEW_TYPE_GRID, "container", i.container.get (),
    "context", render_context, "view-size", 48, "view-border-width", 1, nullptr)),
    "painter-mypaint-preset-grid");
  auto *view = GIMP_CONTAINER_EDITOR (i.grid.get ())->view;
  gtk_widget_set_name (GTK_WIDGET (view), "painter-mypaint-preset-view");
  // Keep the real native container/grid and renderer, but route selections
  // through this epoch-checked controller. Native context_changed otherwise
  // can replay an outer stale emission after a nested selection supersedes it.
  auto *rows = GIMP_CONTAINER_ICON_VIEW (view)->model;
  connect (i, G_OBJECT (rows), "row-changed", G_CALLBACK (model_row_cb), render_row);
  gimp_container_view_set_context (view, nullptr);
  gimp_container_tree_store_set_context (GIMP_CONTAINER_TREE_STORE (rows), render_context);
  gimp_container_editor_set_selection_mode (GIMP_CONTAINER_EDITOR (i.grid.get ()), GTK_SELECTION_SINGLE);
  gtk_widget_set_size_request (GTK_WIDGET (i.grid.get ()), 290, 250);
  gtk_notebook_append_page (GTK_NOTEBOOK (i.notebook.get ()),
                            GTK_WIDGET (i.grid.get ()), gtk_label_new (_("Preset")));
  i.history = widget (gtk_tree_view_new (), "painter-mypaint-custom-list");
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (i.history.get ()), FALSE);
  auto *column = gtk_tree_view_column_new ();
  auto *preview = gtk_cell_renderer_pixbuf_new ();
  g_object_set (preview, "stock-size", GTK_ICON_SIZE_LARGE_TOOLBAR, nullptr);
  gtk_tree_view_column_pack_start (column, preview, FALSE);
  gtk_tree_view_column_set_cell_data_func (column, preview, preview_cell, nullptr, nullptr);
  gtk_tree_view_append_column (GTK_TREE_VIEW (i.history.get ()), column);
  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (i.history.get ()), -1,
    _("Saved brush"), gtk_cell_renderer_text_new (), "text", HISTORY_NAME, nullptr);
  auto scroll = widget (gtk_scrolled_window_new (nullptr, nullptr), "painter-mypaint-custom-scroll");
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll.get ()), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_container_add (GTK_CONTAINER (scroll.get ()), GTK_WIDGET (i.history.get ()));
  gtk_notebook_append_page (GTK_NOTEBOOK (i.notebook.get ()), GTK_WIDGET (scroll.get ()), gtk_label_new (_("Custom")));
  i.message = widget (gtk_label_new (""), "painter-mypaint-selector-status");
  gtk_label_set_line_wrap (GTK_LABEL (i.message.get ()), TRUE);
  gtk_box_pack_start (GTK_BOX (body.get ()), GTK_WIDGET (i.message.get ()), FALSE, FALSE, 0);

  connect (i, i.options.get (), "painter-mybrush-changed", G_CALLBACK (selection_cb),
    [] (Selector& s, const Event& e) { route_selection (s, GIMP_CONTEXT (e.emitter), e.brush); });
  connect (i, i.options.get (), "prop-name-changed", G_CALLBACK (name_cb),
    [] (Selector& s, const Event&) { refresh_identity (s); });
  connect (i, i.options.get (), "history-changed", G_CALLBACK (simple_cb),
    [] (Selector& s, const Event&) { refresh_history (s); });
  connect (i, i.options.get (), "disconnect", G_CALLBACK (simple_cb), disconnect_context);
  if (i.context.get () != i.options.get ()) {
    connect (i, i.context.get (), "painter-mybrush-changed", G_CALLBACK (selection_cb),
      [] (Selector& s, const Event& e) { route_selection (s, GIMP_CONTEXT (e.emitter), e.brush); });
    connect (i, i.context.get (), "disconnect", G_CALLBACK (simple_cb), disconnect_context);
  }
  connect (i, G_OBJECT (view), "selection-changed", G_CALLBACK (simple_cb), choose_preset);
  connect (i, i.history.get (), "cursor-changed", G_CALLBACK (cursor_cb), choose_history);
  connect (i, i.container.get (), "remove", G_CALLBACK (object_cb),
    [] (Selector& s, const Event&) { refresh_identity (s); });
  connect (i, i.container.get (), "thaw", G_CALLBACK (simple_cb),
    [] (Selector& s, const Event&) { refresh_identity (s); });
  // Use GtkButton's accessible clicked action, once per release/keyboard action.
  // No press+clicked double-open; selection never confirms this presentation.
  connect (i, G_OBJECT (i.owner), "clicked", G_CALLBACK (simple_cb), open_popup);
  connect (i, G_OBJECT (i.owner), "unmap", G_CALLBACK (simple_cb), dismiss);
  refresh_identity (i);
  if (!alive (i)) return;
  refresh_history (i);
  if (!alive (i)) return;
  // Native GimpContainerEditor controls its view/busy-box visibility. A
  // recursive show_all would show its busy box and the inverse visibility
  // binding would hide the preset grid. Show this presentation's wrapper,
  // while keeping native descendants under their own visibility policy.
  gtk_widget_set_no_show_all (GTK_WIDGET (i.grid.get ()), TRUE);
  gtk_widget_show (GTK_WIDGET (i.grid.get ()));
  gtk_widget_show_all (GTK_WIDGET (body.get ()));
  gtk_widget_show (GTK_WIDGET (i.thumbnail.get ()));
}
} // namespace

GtkWidget *gimp_mypaint_brush_button_with_popup (GObject *config) {
  return boundary<GtkWidget *> (nullptr, nullptr, [&] () -> GtkWidget * {
    if (!GIMP_IS_CONTEXT (config))
      throw Error (GIMP_PAINTER_ERROR_WRONG_TYPE, "Expected a GimpContext");
    auto context = Ref::retain (config);
    GError *error = nullptr;
    // Resolve/admit the existing options binding before touching GTK. This is
    // the supported thread check, not a claim that GTK is worker-thread safe.
    auto options = Ref::adopt (G_OBJECT (gimp_painter_mybrush_options_ref_for_context (
                                         GIMP_CONTEXT (config), &error)));
    if (!options)
      throw Error (GIMP_PAINTER_ERROR_INVALID_STATE,
                   take_error_message (error, "Painter options are unavailable").c_str ());
    g_clear_error (&error);
    auto button = widget (gtk_button_new (), "painter-mypaint-brush-selector");
    auto *result = GTK_WIDGET (button.get ());
    auto& binding = BindingStore::ensure (button.get ());
    binding.emplace<SelectorSlot> (result);
    g_signal_connect (result, "destroy", G_CALLBACK (destroy_button), nullptr);
    binding.activate ();
    binding.with<SelectorSlot> ([&] (Selector& i) {
      i.context = context; i.options = options; build (i);
      if (!alive (i)) throw Error (GIMP_PAINTER_ERROR_CLOSED, "Brush selector closed during construction");
    });
    g_object_force_floating (button.get ());
    return GTK_WIDGET (button.release ());
  });
}
