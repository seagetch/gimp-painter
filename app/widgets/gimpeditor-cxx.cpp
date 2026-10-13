/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gegl.h>
#include <gtk/gtk.h>
extern "C" {
#include "libgimpbase/gimpbase.h"
#include "libgimpwidgets/gimpwidgets.h"
#include "core/core-types.h"
#include "core/gimp.h"
#include "core/gimpcontext.h"
#include "widgets-types.h"
#include "gimpeditor.h"
#include "gimpeditor-cxx.h"
#include "gimpeditor-cxx-private.h"
#include "gimpaction.h"
#include "gimpactiongroup.h"
#include "gimpuimanager.h"
#include "gimpcontainerview.h"
#include "gimplayerpresetview.h"
#include "menus/menus.h"
}
#include "painter/binding-store.hpp"
#include "painter/connection.hpp"
#include "painter/source.hpp"
#include <memory>
#include <vector>

using namespace GimpPainter;
namespace GimpPainter {
template<> struct TypeTraits<GtkMenuButton> {
  static GType type () noexcept { return GTK_TYPE_MENU_BUTTON; }
};
}
namespace {
struct Dropdown;
struct DropdownSlot : SlotSpec<GtkMenuButton, Dropdown> {};

/* Signal userdata contains only a weak native identity and generation. No
 * callback, closure, qdata or C consumer can borrow a Dropdown implementation. */
struct Hook {
  WeakRef<GObject> owner;
  guint64 generation;
  Hook (GObject *object, guint64 value)
    : owner (ObjectRef<GObject>::retain (object)), generation (value) {}
};
void free_hook (gpointer data, GClosure *) noexcept { delete static_cast<Hook *> (data); }

struct Dropdown {
  WeakRef<GObject> owner, editor, primary;
  ObjectRef<GObject> popup, view;
  std::vector<Connection> connections;
  Source pending_reopen;
  GCallback callback, extended_callback;
  gpointer callback_data;
  guint64 generation = 0, presentation = 0;
  bool closed = false, showing = false;
  guint dismissals = 0;

  Dropdown (GtkWidget *arrow, GimpEditor *e, GtkWidget *button,
            GCallback clicked, GCallback extended, gpointer data)
    : owner (ObjectRef<GObject>::retain (G_OBJECT (arrow))),
      editor (ObjectRef<GObject>::retain (G_OBJECT (e))),
      primary (ObjectRef<GObject>::retain (G_OBJECT (button))),
      callback (clicked), extended_callback (extended), callback_data (data) {}
  ~Dropdown () noexcept { close (); }
  void initialize ();
  void close () noexcept;
  void retire () noexcept;
  void populate ();
  void dismiss ();
  void reopen ();
  void defer_reopen ();
  void connect (GObject *, const char *, GCallback);
};

template<class Function>
void dispatch (gpointer data, Function&& function) noexcept
{
  try {
    auto *hook = static_cast<Hook *> (data);
    auto owner = hook->owner.lock ();
    const auto generation = hook->generation;
    if (!owner) return;
    auto *store = BindingStore::find (owner.get ());
    if (store && store->accepts (generation))
      store->with<DropdownSlot> (std::forward<Function> (function));
  } catch (...) { /* No C++ exception crosses a GTK callback. */ }
}
void owner_destroyed (GtkWidget *owner, gpointer) noexcept
{
  try { if (auto *store = BindingStore::find (G_OBJECT (owner))) store->close (); }
  catch (...) {}
}
void dependency_destroyed (GtkWidget *, gpointer data) noexcept
{
  try {
    auto *hook = static_cast<Hook *> (data);
    auto owner = hook->owner.lock ();
    const auto generation = hook->generation;
    if (owner) {
      auto *store = BindingStore::find (owner.get ());
      if (store && store->generation () == generation) store->close ();
    }
  } catch (...) {}
}
void popup_shown (GtkWidget *, gpointer data) noexcept
{ dispatch (data, [] (Dropdown& self) { self.populate (); }); }
void popup_hidden (GtkWidget *, gpointer data) noexcept
{ dispatch (data, [] (Dropdown& self) {
    /* closed already retired the presentation; the animation's later hide
     * must not invalidate a queued activation from that dismissal. */
    if (self.showing || self.view) self.retire ();
  }); }
void popup_closed (GtkPopover *, gpointer data) noexcept
{ dispatch (data, [] (Dropdown& self) { self.dismiss (); }); }
void arrow_toggled (GtkToggleButton *arrow, gpointer data) noexcept
{
  if (gtk_toggle_button_get_active (arrow))
    dispatch (data, [] (Dropdown& self) { self.reopen (); });
}
gboolean scroll_event (GtkWidget *, GdkEventScroll *, gpointer) noexcept
{ return TRUE; } // The original decorator consumes scrolling over the arrow.
void primary_clicked (GtkWidget *button, gpointer data) noexcept
{
  dispatch (data, [button] (Dropdown& self) {
    auto callback = self.callback;
    auto callback_data = self.callback_data;
    if (callback) reinterpret_cast<void (*) (GtkWidget *, gpointer)> (callback) (button, callback_data);
  });
}
void primary_extended (GtkWidget *button, GdkModifierType mask, gpointer data) noexcept
{
  dispatch (data, [button, mask] (Dropdown& self) {
    auto callback = self.extended_callback;
    auto callback_data = self.callback_data;
    if (callback) reinterpret_cast<void (*) (GtkWidget *, GdkModifierType, gpointer)> (callback) (button, mask, callback_data);
  });
}
void Dropdown::connect (GObject *emitter, const char *signal, GCallback callback)
{
  auto arrow = owner.lock ();
  auto hook = std::unique_ptr<Hook> (new Hook (arrow.get (), generation));
  auto connection = Connection::connect (ObjectRef<GObject>::retain (emitter), signal,
                                          callback, hook.get (), free_hook);
  hook.release ();
  connections.push_back (std::move (connection));
}
void Dropdown::initialize ()
{
  auto arrow = owner.lock ();
  auto e = editor.lock ();
  auto button = primary.lock ();
  generation = BindingStore::require (arrow.get ()).generation ();
  connections.push_back (Connection::connect (arrow, "destroy", G_CALLBACK (owner_destroyed), nullptr, nullptr));
  connect (e.get (), "destroy", G_CALLBACK (dependency_destroyed));
  connect (button.get (), "destroy", G_CALLBACK (dependency_destroyed));
  if (callback) connect (button.get (), "clicked", G_CALLBACK (primary_clicked));
  if (extended_callback) connect (button.get (), "extended-clicked", G_CALLBACK (primary_extended));
  connect (arrow.get (), "scroll-event", G_CALLBACK (scroll_event));
  popup = ObjectRef<GObject>::sink (G_OBJECT (gtk_popover_new (GTK_WIDGET (arrow.get ()))));
  /* Let the native closed handler clear active before retiring our view:
   * destruction observers may already request the next activation. */
  gtk_menu_button_set_popover (GTK_MENU_BUTTON (arrow.get ()), GTK_WIDGET (popup.get ()));
  connect (popup.get (), "show", G_CALLBACK (popup_shown));
  connect (popup.get (), "closed", G_CALLBACK (popup_closed));
  connect (popup.get (), "hide", G_CALLBACK (popup_hidden));
  connect (arrow.get (), "toggled", G_CALLBACK (arrow_toggled));
}
void Dropdown::dismiss ()
{
  ++dismissals;
  retire ();
  --dismissals;
}
void Dropdown::defer_reopen ()
{
  auto arrow = owner.lock ();
  if (closed || !arrow || !gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (arrow.get ()))) return;
  auto hook = std::make_shared<Hook> (arrow.get (), generation);
  const auto revision = presentation;
  pending_reopen = Source::idle (nullptr, G_PRIORITY_DEFAULT_IDLE, [hook, revision] {
    dispatch (hook.get (), [revision] (Dropdown& self) {
      auto previous = std::move (self.pending_reopen);
      if (self.presentation == revision) self.reopen ();
    });
    return false;
  });
}
void Dropdown::reopen ()
{
  auto arrow = owner.lock ();
  if (closed || !arrow || !gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (arrow.get ()))) return;
  /* GTK releases modality after emitting closed. Reentrant activation must
   * wait for that emission to unwind, not restore a grab that GTK will remove. */
  if (dismissals) { defer_reopen (); return; }
  if (showing || !popup) return;
  auto popup_lease = popup;
  /* GtkMenuButton's RUN_FIRST toggled handler has already attempted popup().
   * While popdown is animating, GtkWidget remains visible and emits no show.
   * Finish only that retired transition, then start a fresh native animation. */
  const auto revision = presentation;
  if (gtk_widget_get_visible (GTK_WIDGET (popup_lease.get ())))
    gtk_widget_hide (GTK_WIDGET (popup_lease.get ()));
  /* Hide observers may close the owner, cancel activation, or open a newer
   * presentation. None of those choices belongs to this older invocation. */
  if (closed || showing || presentation != revision ||
      !gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (arrow.get ()))) return;
  gtk_popover_popup (GTK_POPOVER (popup_lease.get ()));
}
void Dropdown::retire () noexcept
{
  showing = false;
  ++presentation;
  auto old = std::move (view);
  if (old) {
    /* Leave the GtkBin empty before destroy observers can reopen a newer
     * presentation. The local lease keeps the old view alive while unparenting. */
    auto *parent = gtk_widget_get_parent (GTK_WIDGET (old.get ()));
    if (parent) gtk_container_remove (GTK_CONTAINER (parent), GTK_WIDGET (old.get ()));
    gtk_widget_destroy (GTK_WIDGET (old.get ()));
  }
}
void Dropdown::close () noexcept
{
  if (closed) return;
  closed = true;
  pending_reopen.close ();
  /* Detach all hooks before destroying either sibling or the native preset
   * view. Destruction can synchronously invoke arbitrary application code. */
  auto old_connections = std::move (connections);
  for (auto& connection : old_connections) connection.close ();
  callback = extended_callback = nullptr;
  callback_data = nullptr;
  retire ();
  auto old_popup = std::move (popup);
  if (old_popup) gtk_widget_destroy (GTK_WIDGET (old_popup.get ()));
  auto button = primary.lock ();
  auto arrow = owner.lock ();
  primary.reset (); editor.reset (); owner.reset ();
  if (button && !gtk_widget_in_destruction (GTK_WIDGET (button.get ())))
    gtk_widget_destroy (GTK_WIDGET (button.get ()));
  if (arrow && !gtk_widget_in_destruction (GTK_WIDGET (arrow.get ())))
    gtk_widget_destroy (GTK_WIDGET (arrow.get ()));
}
void Dropdown::populate ()
{
  if (dismissals) { defer_reopen (); return; }
  const auto revision = presentation + 1;
  retire ();
  /* Retiring the old child may synchronously reopen a newer presentation. */
  if (closed || presentation != revision) return;
  showing = true;
  auto e = editor.lock ();
  auto popup_lease = popup;
  if (!e || !popup_lease || !GIMP_IS_CONTAINER_VIEW (e.get ())) return;
  auto context = ObjectRef<GObject>::retain (G_OBJECT (gimp_container_view_get_context (GIMP_CONTAINER_VIEW (e.get ()))));
  if (!context) return;
  auto *factory = gimp_editor_get_menu_factory (GIMP_EDITOR (e.get ()));
  if (!factory) factory = menus_get_global_menu_factory (GIMP_CONTEXT (context.get ())->gimp);
  if (!factory) return;
  auto factory_lease = ObjectRef<GObject>::retain (G_OBJECT (factory));
  auto next = ObjectRef<GObject>::sink (G_OBJECT (gimp_layer_preset_view_new (
    GIMP_CONTEXT (context.get ()), GIMP_VIEW_SIZE_SMALL, factory)));
  if (!next) return;
  /* View construction may emit source/model notifications which close us. */
  if (closed || !showing || presentation != revision) {
    gtk_widget_destroy (GTK_WIDGET (next.get ()));
    return;
  }
  view = next;
  gtk_widget_set_size_request (GTK_WIDGET (next.get ()), -1, 300);
  if (closed || !showing || presentation != revision) return;
  gtk_container_add (GTK_CONTAINER (popup_lease.get ()), GTK_WIDGET (next.get ()));
  if (closed || !showing || presentation != revision) return;
  gtk_widget_show_all (GTK_WIDGET (next.get ()));
}

/* A synchronous construction lease is not published or retained by callbacks.
 * Its destroy observer expires before this stack frame returns. */
struct Construction {
  ObjectRef<GObject> editor;
  bool closed = false;
  Connection destroyed;
  static void destroy (GtkWidget *, gpointer data) noexcept {
    *static_cast<bool *> (data) = true;
  }
  explicit Construction (GimpEditor *e) : editor (ObjectRef<GObject>::retain (G_OBJECT (e))) {
    destroyed = Connection::connect (editor, "destroy", G_CALLBACK (destroy), &closed, nullptr);
  }
};

GtkWidget *attach (Construction& construction, GtkWidget *button,
                   const gchar *tooltip, const gchar *help_id,
                   GCallback callback, GCallback extended, gpointer data)
{
  auto *editor = GIMP_EDITOR (construction.editor.get ());
  auto primary = ObjectRef<GObject>::retain (G_OBJECT (button));
  auto arrow = ObjectRef<GObject>::sink (G_OBJECT (gtk_menu_button_new ()));
  try {
    GtkIconSize size;
    GtkReliefStyle relief;
    gtk_widget_style_get (GTK_WIDGET (editor), "button-icon-size", &size,
                          "button-relief", &relief, nullptr);
    gtk_button_set_relief (GTK_BUTTON (arrow.get ()), relief);
    if (auto *child = gtk_bin_get_child (GTK_BIN (arrow.get ()))) gtk_widget_destroy (child);
    auto image = ObjectRef<GObject>::sink (G_OBJECT (gtk_image_new_from_icon_name ("pan-down-symbolic", size)));
    gtk_container_add (GTK_CONTAINER (arrow.get ()), GTK_WIDGET (image.get ()));
    gtk_widget_show (GTK_WIDGET (image.get ()));
    if (tooltip || help_id) {
      gimp_help_set_help_data (GTK_WIDGET (arrow.get ()), tooltip, help_id);
      /* Native help stores a borrowed pointer. The action adapter's help copy
       * expires on return, so give the arrow its own native string lifetime. */
      g_object_set_qdata_full (arrow.get (), GIMP_HELP_ID, g_strdup (help_id), g_free);
    }
    if (construction.closed) {
      gtk_widget_destroy (button); gtk_widget_destroy (GTK_WIDGET (arrow.get ()));
      return nullptr;
    }
    auto& store = BindingStore::ensure (arrow.get ());
    store.emplace<DropdownSlot> (GTK_WIDGET (arrow.get ()), editor, button, callback, extended, data);
    store.initialize<DropdownSlot> ([] (Dropdown& self) { self.initialize (); });
    store.activate ();
    if (!gimp_editor_pack_button (editor, button) ||
        store.state () != BindingStore::State::active) { store.close (); return nullptr; }
    if (!gimp_editor_pack_button (editor, GTK_WIDGET (arrow.get ())) ||
        store.state () != BindingStore::State::active) { store.close (); return nullptr; }
    return button;
  } catch (...) {
    if (auto *store = BindingStore::find (arrow.get ())) store->close ();
    gtk_widget_destroy (GTK_WIDGET (arrow.get ()));
    gtk_widget_destroy (button);
    throw;
  }
}
}

extern "C" GtkWidget *
gimp_editor_add_dropdown (GimpEditor *editor, const gchar *icon_name,
                          const gchar *tooltip, const gchar *help_id,
                          const gchar *list_id, GCallback callback,
                          GCallback extended_callback, gpointer callback_data)
{
  g_return_val_if_fail (GIMP_IS_EDITOR (editor), nullptr);
  g_return_val_if_fail (icon_name != nullptr, nullptr);
  (void) list_id; // Deliberately unused, matching the original callable provider.
  try {
    Construction construction (editor);
    auto primary = ObjectRef<GObject>::sink (G_OBJECT (gimp_button_new ()));
    auto primary_destroyed = Connection::connect (primary, "destroy", G_CALLBACK (Construction::destroy), &construction.closed, nullptr);
    GtkIconSize size;
    gtk_widget_style_get (GTK_WIDGET (editor), "button-icon-size", &size, nullptr);
    if (construction.closed) return nullptr;
    if (tooltip || help_id) gimp_help_set_help_data (GTK_WIDGET (primary.get ()), tooltip, help_id);
    auto image = ObjectRef<GObject>::sink (G_OBJECT (gtk_image_new_from_icon_name (icon_name, size)));
    gtk_container_add (GTK_CONTAINER (primary.get ()), GTK_WIDGET (image.get ()));
    if (construction.closed) return nullptr;
    gtk_widget_show (GTK_WIDGET (image.get ()));
    if (construction.closed) return nullptr;
    return attach (construction, GTK_WIDGET (primary.get ()), tooltip, help_id, callback, extended_callback, callback_data);
  } catch (...) { return nullptr; }
}

extern "C" GtkWidget *
gimp_editor_add_action_dropdown (GimpEditor *editor, const gchar *list_id,
                                 const gchar *group_name, const gchar *action_name, ...)
{
  g_return_val_if_fail (GIMP_IS_EDITOR (editor), nullptr);
  g_return_val_if_fail (action_name != nullptr, nullptr);
  (void) list_id;
  try {
    Construction construction (editor);
    /* Snapshot native action help before callback-producing construction.
     * The editor's manager/group/action may legitimately disappear during it. */
    auto *manager = gimp_editor_get_ui_manager (editor);
    if (!manager) return nullptr;
    auto *group = gimp_ui_manager_get_action_group (manager, group_name);
    if (!group) return nullptr;
    auto *action = gimp_action_group_get_action (group, action_name);
    if (!action) return nullptr;
    struct Text { gchar *value; ~Text () { g_free (value); } } tooltip {g_strdup (gimp_action_get_tooltip (action))},
      help {g_strdup (gimp_action_get_help_id (action))};
    va_list args;
    va_start (args, action_name);
    auto *button = gimp_editor_create_action_button_valist (editor, group_name, action_name, TRUE, args);
    va_end (args);
    auto primary = ObjectRef<GObject>::adopt (G_OBJECT (button));
    if (!primary || construction.closed) return nullptr;
    return attach (construction, button, tooltip.value, help.value, nullptr, nullptr, nullptr);
  } catch (...) { return nullptr; }
}
