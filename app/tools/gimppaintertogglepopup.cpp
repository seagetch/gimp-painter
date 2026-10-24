/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gegl.h>
#include <gtk/gtk.h>
extern "C" {
#include "libgimpbase/gimpbase.h"
#include "core/core-types.h"
#include "widgets/widgets-types.h"
#include "widgets/gimpcontainerpopup.h"
#include "widgets/popupper.h"
#include "gimppaintertogglepopup.h"
}
#include "painter/binding-store.hpp"
#include "painter/connection.hpp"
#include <vector>
#include <memory>
using namespace GimpPainter;
namespace GimpPainter {
template<> struct TypeTraits<GtkWidget> { static GType type () noexcept { return GTK_TYPE_WIDGET; } };
}
namespace {
using Ref = ObjectRef<GObject>;
struct DeferredContent { Ref content; unsigned pending = 0; bool dispose = false; };
struct TogglePopup;
struct Slot : SlotSpec<GtkWidget, TogglePopup> {};
struct TogglePopup {
  GtkWidget *owner;
  WeakRef<GObject> anchor;
  Ref body, popup;
  std::vector<Connection> connections, host_connections;
  std::shared_ptr<DeferredContent> deferred = std::make_shared<DeferredContent> ();
  bool closed = false, opening = false, retiring = false, disposing = false;
  explicit TogglePopup (GtkWidget *button, GtkWidget *row, GtkWidget *content)
    : owner (button), anchor (Ref::retain (G_OBJECT (row))),
      body (Ref::sink (G_OBJECT (content))) {}
  void detach (GtkWidget *host);
  void popdown ();
  void open ();
  void close () noexcept;
};
struct Hook {
  WeakRef<GObject> owner;
  explicit Hook (GtkWidget *widget) : owner (Ref::retain (G_OBJECT (widget))) {}
};
void free_hook (gpointer data, GClosure*) { delete static_cast<Hook*> (data); }
template<class F> void dispatch (gpointer data, F&& fn) noexcept {
  try {
    auto owner = static_cast<Hook*> (data)->owner.lock ();
    if (!owner) return;
    auto *store = BindingStore::find (owner.get ());
    if (store && store->state () == BindingStore::State::active)
      store->with<Slot> ([&] (TogglePopup& p) { if (!p.closed) fn (p); });
  } catch (const std::exception& e) { g_warning ("Painter toggle popup: %s", e.what ()); }
}
void clicked (GtkButton*, gpointer data) {
  dispatch (data, [] (TogglePopup& p) { p.open (); });
}
void unmapped (GtkWidget*, gpointer data) {
  dispatch (data, [] (TogglePopup& p) { p.popdown (); });
}
void dismiss (GtkWidget *host, gpointer data) {
  auto lease = Ref::retain (G_OBJECT (host));
  dispatch (data, [host] (TogglePopup& p) { p.detach (host); });
}
void destroyed (GtkWidget *host, gpointer data) {
  auto lease = Ref::retain (G_OBJECT (host));
  dispatch (data, [host] (TogglePopup& p) {
    p.detach (host);
    if (p.popup.get () == G_OBJECT (host)) p.popup.reset ();
  });
}
void owner_destroyed (GtkWidget *owner, gpointer) {
  try { if (auto *s = BindingStore::find (G_OBJECT (owner))) s->close (); }
  catch (const std::exception& e) { g_warning ("Painter toggle destroy: %s", e.what ()); }
}
Connection connect (GtkWidget *emitter, const char *signal, GCallback callback,
                    GtkWidget *owner) {
  return Connection::connect (Ref::retain (G_OBJECT (emitter)), signal,
                              callback, new Hook (owner), free_hook);
}
void pending_child_destroyed (GtkWidget*, gpointer data) {
  auto deferred = *static_cast<std::shared_ptr<DeferredContent>*> (data);
  if (deferred->pending) --deferred->pending;
  if (!deferred->pending) {
    auto content = std::move (deferred->content);
    if (deferred->dispose && content) gtk_widget_destroy (GTK_WIDGET (content.get ()));
  }
}
void free_pending_child (gpointer data, GClosure*) {
  delete static_cast<std::shared_ptr<DeferredContent>*> (data);
}
void TogglePopup::detach (GtkWidget *host) {
  if (popup.get () != G_OBJECT (host) || retiring) return;
  retiring = true;
  struct Guard { TogglePopup& p; ~Guard () { p.retiring = false; if (p.closed) p.close (); } } guard {*this};
  auto content = body;
  // GimpViewableButton's stock popup callbacks borrow their invoker. Close only
  // this host's transient stock selectors while that invoker is still alive.
  // Their selection/confirmation/key/grab policy remains entirely stock.
  std::vector<Ref> children;
  GList *windows = gtk_window_list_toplevels ();
  for (auto *node = windows; node; node = node->next) {
    auto *child = GTK_WIDGET (node->data);
    if (GIMP_IS_CONTAINER_POPUP (child) &&
        gtk_window_get_transient_for (GTK_WINDOW (child)) == GTK_WINDOW (host))
      children.push_back (Ref::retain (G_OBJECT (child)));
  }
  g_list_free (windows);
  for (auto& child : children) {
    if (gtk_widget_in_destruction (GTK_WIDGET (child.get ()))) continue;
    auto *invocation = g_signal_get_invocation_hint (child.get ());
    if (invocation &&
        (invocation->signal_id == g_signal_lookup ("cancel", G_OBJECT_TYPE (child.get ())) ||
         invocation->signal_id == g_signal_lookup ("confirm", G_OBJECT_TYPE (child.get ())))) {
      // A stock cancellation observer may destroy our toolbar before the
      // stock default handler runs. Never recursively emit the same signal or
      // destroy its borrowed invoker midway through that emission.
      if (!deferred->content) deferred->content = body;
      ++deferred->pending;
      g_signal_connect_data (child.get (), "destroy", G_CALLBACK (pending_child_destroyed),
                             new std::shared_ptr<DeferredContent> (deferred), free_pending_child, GConnectFlags (0));
    } else {
      g_signal_emit_by_name (child.get (), "cancel");
    }
  }
  if (content) {
    auto *widget = GTK_WIDGET (content.get ());
    if (auto *parent = gtk_widget_get_parent (widget))
      gtk_container_remove (GTK_CONTAINER (parent), widget);
  }
}
void TogglePopup::popdown () {
  if (retiring) return;
  auto host = popup;
  if (!host) return;
  detach (GTK_WIDGET (host.get ()));
  popup.reset ();
  gtk_widget_destroy (GTK_WIDGET (host.get ()));
}
void TogglePopup::open () {
  if (opening || retiring || deferred->pending || !gtk_widget_get_mapped (owner) ||
      !gtk_widget_is_sensitive (owner)) return;
  opening = true;
  struct Guard { bool& value; ~Guard () { value = false; } } guard {opening};
  auto row = anchor.lock (), content = body;
  popdown ();
  if (closed || deferred->pending || !row || !content || gtk_widget_in_destruction (GTK_WIDGET (row.get ())) ||
      !gtk_widget_get_mapped (GTK_WIDGET (row.get ()))) return;
  host_connections.clear ();
  auto carrier = Ref::sink (G_OBJECT (gtk_box_new (GTK_ORIENTATION_VERTICAL, 0)));
  auto *host = gimp_painter_popup_new (GTK_WIDGET (carrier.get ()));
  if (!host) return;
  auto lease = Ref::retain (G_OBJECT (host));
  if (closed) { gtk_widget_destroy (host); return; }
  popup = lease;
  host_connections.push_back (connect (host, "cancel", G_CALLBACK (dismiss), owner));
  host_connections.push_back (connect (host, "confirm", G_CALLBACK (dismiss), owner));
  host_connections.push_back (connect (host, "destroy", G_CALLBACK (destroyed), owner));
  gtk_widget_set_name (host, "painter-standard-toggle-popup");
  if (closed || popup.get () != G_OBJECT (host) || gtk_widget_in_destruction (host) ||
      !gtk_widget_get_mapped (owner) || !gtk_widget_is_sensitive (owner)) {
    if (popup.get () == G_OBJECT (host)) popdown ();
    else gtk_widget_destroy (host);
    return;
  }
  if (gtk_widget_get_parent (GTK_WIDGET (content.get ()))) { popdown (); return; }
  gtk_box_pack_start (GTK_BOX (carrier.get ()), GTK_WIDGET (content.get ()), TRUE, TRUE, 0);
  if (closed || popup.get () != G_OBJECT (host) ||
      !gtk_widget_get_mapped (owner) || !gtk_widget_is_sensitive (owner) ||
      gtk_widget_in_destruction (host)) {
    if (popup.get () == G_OBJECT (host)) popdown ();
    else gtk_widget_destroy (host);
    return;
  }
  gimp_painter_popup_show_over (host, GTK_WIDGET (row.get ()));
}
void TogglePopup::close () noexcept {
  closed = true;
  if (retiring || disposing) return;
  disposing = true;
  popdown ();
  host_connections.clear ();
  connections.clear ();
  auto content = std::move (body);
  deferred->dispose = true;
  if (content && !deferred->pending) gtk_widget_destroy (GTK_WIDGET (content.get ()));
  anchor.reset ();
  disposing = false;
}
}
extern "C" GtkWidget *
gimp_painter_toggle_popup_button_new (GtkWidget *anchor, GtkWidget *body) {
  return boundary<GtkWidget*> (nullptr, nullptr, [&] {
    auto *button = gtk_button_new ();
    auto lease = Ref::sink (G_OBJECT (button));
    auto& store = BindingStore::ensure (G_OBJECT (button));
    store.emplace<Slot> (button, anchor, body);
    store.activate ();
    store.with<Slot> ([&] (TogglePopup& p) {
      p.connections.push_back (connect (button, "clicked", G_CALLBACK (clicked), button));
      p.connections.push_back (connect (button, "unmap", G_CALLBACK (unmapped), button));
      p.connections.push_back (Connection::connect (lease, "destroy", G_CALLBACK (owner_destroyed), nullptr, nullptr));
    });
    // Return the ordinary floating widget ownership expected by GTK factories.
    g_object_force_floating (lease.release ());
    return button;
  });
}
