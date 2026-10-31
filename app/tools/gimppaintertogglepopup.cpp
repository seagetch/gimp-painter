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
#include "painter/gimp-painter-binding.h"
#include <string>
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
  const char *host_name;
  WeakRef<GObject> anchor;
  Ref body, popup;
  std::vector<Connection> connections, host_connections;
  std::shared_ptr<DeferredContent> deferred = std::make_shared<DeferredContent> ();
  bool closed = false, opening = false, retiring = false, disposing = false;
  explicit TogglePopup (GtkWidget *button, GtkWidget *row, GtkWidget *content, const char *name)
    : owner (button), host_name (name), anchor (Ref::retain (G_OBJECT (row))),
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
  gtk_widget_set_name (host, host_name);
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
static GtkWidget *
popup_button_new (GtkWidget *anchor, GtkWidget *body, const char *host_name,
                  bool self_anchor) {
  return boundary<GtkWidget*> (nullptr, nullptr, [&] {
    auto *button = gtk_button_new ();
    auto lease = Ref::sink (G_OBJECT (button));
    auto& store = BindingStore::ensure (G_OBJECT (button));
    store.emplace<Slot> (button, self_anchor ? button : anchor, body, host_name);
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

extern "C" GtkWidget *
gimp_painter_toggle_popup_button_new (GtkWidget *anchor, GtkWidget *body) {
  return popup_button_new (anchor, body, "painter-standard-toggle-popup", false);
}

extern "C" GtkWidget *
gimp_painter_transform_details_button_new (GtkWidget *body) {
  // Legacy frame-with-popup anchors to its own label/arrow button, unlike the
  // toggle helper which explicitly anchors to the whole checkbox/arrow row.
  return popup_button_new (nullptr, body, "painter-transform-details-popup", true);
}

extern "C" GtkWidget *
gimp_painter_bucket_details_button_new (GtkWidget *body) {
  return popup_button_new (nullptr, body, "painter-bucket-details-popup", true);
}

extern "C" GtkWidget *
gimp_painter_ink_popup_button_new (GtkWidget *body, gboolean blob) {
  return popup_button_new (nullptr, body,
                           blob ? "painter-ink-blob-popup" :
                                  "painter-ink-sensitivity-popup", true);
}

extern "C" GtkWidget *
gimp_painter_clone_options_button_new (GtkWidget *body) {
  return popup_button_new (nullptr, body, "painter-clone-options-popup", true);
}


/* Fresh resource views have a separate controller: none of the retained-body
 * ownership or nested stock-selector handling above changes. Brush uses a
 * directly embedded live grid, never a staged GimpContainerPopup child. */
namespace {
struct FreshPopup;
struct FreshSlot : SlotSpec<GtkWidget, FreshPopup> {};
struct FreshPopup {
  GtkWidget *owner;
  WeakRef<GObject> model;
  GimpPainterPopupCreateView create_view;
  std::string host_name;
  Ref body, popup;
  std::vector<Connection> connections, host_connections;
  bool closed = false, opening = false, retiring = false;
  FreshPopup (GtkWidget *button, GObject *value,
              GimpPainterPopupCreateView factory, const char *name)
    : owner (button), model (Ref::retain (value)), create_view (factory), host_name (name) {}
  void retire (GtkWidget *host);
  void popdown ();
  void open ();
  void close () noexcept;
};
template<class F> void fresh_dispatch (gpointer data, F&& fn) noexcept {
  try {
    auto owner = static_cast<Hook*> (data)->owner.lock ();
    if (!owner) return;
    auto *store = BindingStore::find (owner.get ());
    if (store && store->state () == BindingStore::State::active)
      store->with<FreshSlot> ([&] (FreshPopup& p) { if (!p.closed) fn (p); });
  } catch (const std::exception& e) { g_warning ("Painter resource popup: %s", e.what ()); }
}
void fresh_clicked (GtkButton*, gpointer data) {
  fresh_dispatch (data, [] (FreshPopup& p) {
    // Keyboard/programmatic activation remains available; never reopen on a
    // clicked companion after the legacy pointer-press opening.
    if (!p.popup) p.open ();
  });
}
void fresh_unmapped (GtkWidget*, gpointer data) {
  fresh_dispatch (data, [] (FreshPopup& p) { p.popdown (); });
}
void fresh_dismissed (GtkWidget *host, gpointer data) {
  auto lease = Ref::retain (G_OBJECT (host));
  fresh_dispatch (data, [host] (FreshPopup& p) { p.retire (host); });
}
void fresh_host_unmapped (GtkWidget *host, gpointer data) {
  auto lease = Ref::retain (G_OBJECT (host));
  fresh_dispatch (data, [host] (FreshPopup& p) {
    if (p.popup.get () == G_OBJECT (host)) p.popdown ();
  });
}
void fresh_body_destroyed (GtkWidget *body, gpointer data) {
  auto lease = Ref::retain (G_OBJECT (body));
  fresh_dispatch (data, [body] (FreshPopup& p) {
    if (p.body.get () == G_OBJECT (body)) p.popdown ();
  });
}
void FreshPopup::retire (GtkWidget *host) {
  if (popup.get () != G_OBJECT (host) || retiring) return;
  retiring = true;
  struct Guard { bool& flag; ~Guard () { flag = false; } } guard {retiring};
  auto content = std::move (body);
  popup.reset ();
  host_connections.clear ();
  // Close observations before any child destroy signals can reenter them.
  if (content) {
    gimp_painter_binding_close (content.get (), nullptr);
    gtk_widget_destroy (GTK_WIDGET (content.get ()));
  }
}
void FreshPopup::popdown () {
  if (retiring) return;
  auto host = popup;
  if (!host) return;
  retire (GTK_WIDGET (host.get ()));
  gtk_widget_destroy (GTK_WIDGET (host.get ()));
}
void FreshPopup::open () {
  if (closed || opening || retiring || !gtk_widget_get_mapped (owner) ||
      !gtk_widget_is_sensitive (owner)) return;
  opening = true;
  struct Guard { bool& flag; ~Guard () { flag = false; } } guard {opening};
  popdown ();
  auto config = model.lock ();
  if (closed || !config || gtk_widget_in_destruction (owner)) return;
  auto *created = create_view (config.get ());
  if (!created) return;
  auto content = Ref::sink (G_OBJECT (created));
  auto discard = [&] {
    gimp_painter_binding_close (content.get (), nullptr);
    gtk_widget_destroy (created);
  };
  if (closed || !gtk_widget_get_mapped (owner) || gtk_widget_in_destruction (created)) {
    discard (); return;
  }
  auto carrier = Ref::sink (G_OBJECT (gtk_box_new (GTK_ORIENTATION_VERTICAL, 0)));
  auto *host = gimp_painter_popup_new (GTK_WIDGET (carrier.get ()));
  if (!host) { discard (); return; }
  auto lease = Ref::retain (G_OBJECT (host));
  if (closed) { discard (); gtk_widget_destroy (host); return; }
  popup = lease;
  body = content;
  host_connections.push_back (connect (host, "cancel", G_CALLBACK (fresh_dismissed), owner));
  host_connections.push_back (connect (host, "confirm", G_CALLBACK (fresh_dismissed), owner));
  host_connections.push_back (connect (host, "destroy", G_CALLBACK (fresh_dismissed), owner));
  host_connections.push_back (connect (host, "unmap", G_CALLBACK (fresh_host_unmapped), owner));
  host_connections.push_back (connect (created, "destroy", G_CALLBACK (fresh_body_destroyed), owner));
  auto current = [&] {
    auto *host_store = BindingStore::find (G_OBJECT (host));
    return host_store && host_store->state () == BindingStore::State::active &&
           !closed && popup.get () == G_OBJECT (host) &&
           !gtk_widget_in_destruction (host) && !gtk_widget_in_destruction (created) &&
           gtk_widget_get_mapped (owner) && gtk_widget_is_sensitive (owner);
  };
  gtk_widget_set_name (host, host_name.c_str ());
  if (!current ()) { popdown (); return; }
  gtk_box_pack_start (GTK_BOX (carrier.get ()), created, TRUE, TRUE, 0);
  if (!current ()) { popdown (); return; }
  gtk_widget_show_all (created);
  if (!current ()) { popdown (); return; }
  // Only the new fresh Brush host constrains an otherwise fitting form. The
  // retained native callers above keep their exact old edge-flipping policy.
  GtkRequisition natural;
  gtk_widget_get_preferred_size (host, nullptr, &natural);
  if (!current ()) { popdown (); return; }
  GtkAllocation allocation; gtk_widget_get_allocation (owner, &allocation);
  gint left, top; gdk_window_get_origin (gtk_widget_get_window (owner), &left, &top);
  if (!gtk_widget_get_has_window (owner)) { left += allocation.x; top += allocation.y; }
  auto *monitor = gdk_display_get_monitor_at_point (gtk_widget_get_display (owner), left, top);
  if (!monitor) { popdown (); return; }
  GdkRectangle bounds; gdk_monitor_get_geometry (monitor, &bounds);
  gint x = left, y = top + allocation.height;
  if (x + natural.width > bounds.x + bounds.width) x = left + allocation.width - natural.width;
  if (y + natural.height > bounds.y + bounds.height) y = top - natural.height;
  x = CLAMP (x, bounds.x, MAX (bounds.x, bounds.x + bounds.width - natural.width));
  y = CLAMP (y, bounds.y, MAX (bounds.y, bounds.y + bounds.height - natural.height));
  if (!current ()) { popdown (); return; }
  gtk_window_move (GTK_WINDOW (host), x, y);
  if (!current ()) { popdown (); return; }
  gtk_widget_show (host);
}
void FreshPopup::close () noexcept {
  closed = true;
  connections.clear ();
  popdown ();
  host_connections.clear ();
  model.reset ();
}
}

/* Override only this private subclass's press phase. Connecting a FALSE
 * button-press handler to ordinary GtkButton would still run its default
 * press handler and steal the newly opened popup's grab. The old 2.8 class
 * replaced that vfunc and returned FALSE without chaining, as done here. */
struct GimpPainterResourceButton { GtkButton parent_instance; };
struct GimpPainterResourceButtonClass { GtkButtonClass parent_class; };
static GType gimp_painter_resource_button_get_type (void);
G_DEFINE_TYPE (GimpPainterResourceButton, gimp_painter_resource_button, GTK_TYPE_BUTTON)
static gboolean resource_button_press (GtkWidget *widget, GdkEventButton*) {
  return boundary<gboolean> (nullptr, FALSE, [&] {
    Hook hook (widget);
    fresh_dispatch (&hook, [] (FreshPopup& p) { p.open (); });
    return FALSE;
  });
}
static gboolean resource_button_scroll (GtkWidget*, GdkEventScroll*) { return TRUE; }
static void gimp_painter_resource_button_class_init (GimpPainterResourceButtonClass *klass) {
  GTK_WIDGET_CLASS (klass)->button_press_event = resource_button_press;
  GTK_WIDGET_CLASS (klass)->scroll_event = resource_button_scroll;
}
static void gimp_painter_resource_button_init (GimpPainterResourceButton *button) {
  gtk_widget_add_events (GTK_WIDGET (button), GDK_SCROLL_MASK);
}
extern "C" GtkWidget *
gimp_painter_resource_popup_button_new (GObject *model,
                                         GimpPainterPopupCreateView create_view,
                                         const gchar *host_name) {
  return boundary<GtkWidget*> (nullptr, nullptr, [&] {
    if (!G_IS_OBJECT (model) || !create_view || !host_name) return static_cast<GtkWidget*> (nullptr);
    auto *button = GTK_WIDGET (g_object_new (gimp_painter_resource_button_get_type (), nullptr));
    auto lease = Ref::sink (G_OBJECT (button));
    auto& store = BindingStore::ensure (G_OBJECT (button));
    store.emplace<FreshSlot> (button, model, create_view, host_name);
    store.activate ();
    store.with<FreshSlot> ([&] (FreshPopup& p) {
      p.connections.push_back (connect (button, "clicked", G_CALLBACK (fresh_clicked), button));
      p.connections.push_back (connect (button, "unmap", G_CALLBACK (fresh_unmapped), button));
      p.connections.push_back (Connection::connect (lease, "destroy", G_CALLBACK (owner_destroyed), nullptr, nullptr));
    });
    g_object_force_floating (lease.release ());
    return button;
  });
}
