/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include "popupper.hpp"
#include "painter/binding-store.hpp"
#include "painter/gimp-painter-binding.h"

/* Preserve the historical runtime name, without the former Interface store. */
using GimpPopover2 = GimpPopover;
using GimpPopover2Class = GimpPopoverClass;
G_DEFINE_TYPE (GimpPopover2, gimp_popover, GTK_TYPE_WINDOW)
using namespace GimpPainter;
namespace {
struct Popup {
  WeakRef<GObject> frame;
  void close () noexcept { frame.reset (); }
};
struct PopupSlot : SlotSpec<GimpPopover, Popup> {};
guint cancel_signal, confirm_signal;
bool active (GObject *owner)
{
  auto *store = BindingStore::find (owner);
  return store && store->state () == BindingStore::State::active;
}
void constructed (GObject *owner)
{
  const bool ready = boundary<bool> (nullptr, false, [&] {
    G_OBJECT_CLASS (gimp_popover_parent_class)->constructed (owner);
    auto& store = BindingStore::require (owner);
    if (store.state () != BindingStore::State::constructing) return false;
    auto frame = ObjectRef<GObject>::sink (G_OBJECT (gtk_frame_new (nullptr)));
    gtk_frame_set_shadow_type (GTK_FRAME (frame.get ()), GTK_SHADOW_OUT);
    gtk_container_add (GTK_CONTAINER (owner), GTK_WIDGET (frame.get ()));
    gtk_widget_show (GTK_WIDGET (frame.get ()));
    store.initialize<PopupSlot> ([&] (Popup& popup) { popup.frame = WeakRef<GObject> (frame); });
    store.activate ();
    return true;
  });
  if (!ready) gimp_painter_binding_close (owner, nullptr);
}
void dispose (GObject *owner)
{
  gimp_painter_binding_close (owner, nullptr);
  boundary_void (nullptr, [&] {
    if (gtk_grab_get_current () == GTK_WIDGET (owner)) gtk_grab_remove (GTK_WIDGET (owner));
  });
  G_OBJECT_CLASS (gimp_popover_parent_class)->dispose (owner);
}
void close_window (GimpPopover *popover)
{
  boundary_void (nullptr, [&] {
    auto *owner = G_OBJECT (popover);
    if (!active (owner)) return;
    auto lease = ObjectRef<GObject>::retain (owner);
    gimp_painter_binding_close (owner, nullptr);
    if (gtk_grab_get_current () == GTK_WIDGET (owner)) gtk_grab_remove (GTK_WIDGET (owner));
    gtk_widget_destroy (GTK_WIDGET (owner));
  });
}
void map (GtkWidget *widget)
{
  boundary_void (nullptr, [&] {
    auto *owner = G_OBJECT (widget);
    if (!active (owner)) return;
    auto lease = ObjectRef<GObject>::retain (owner);
    GTK_WIDGET_CLASS (gimp_popover_parent_class)->map (widget);
    if (active (owner) && !gtk_widget_in_destruction (widget)) gtk_grab_add (widget);
  });
}
void unmap (GtkWidget *widget)
{
  boundary_void (nullptr, [&] {
    auto lease = ObjectRef<GObject>::retain (G_OBJECT (widget));
    if (gtk_grab_get_current () == widget) gtk_grab_remove (widget);
    GTK_WIDGET_CLASS (gimp_popover_parent_class)->unmap (widget);
  });
}
gboolean button_press (GtkWidget *widget, GdkEventButton *event)
{
  return boundary<gboolean> (nullptr, FALSE, [&] () -> gboolean {
    auto *owner = G_OBJECT (widget);
    if (!event || !active (owner)) return FALSE;
    auto lease = ObjectRef<GObject>::retain (owner);
    auto *target = gtk_get_event_widget (reinterpret_cast<GdkEvent *> (event));
    if (!target) return FALSE;
    bool outside = false;
    if (target == widget) {
      GtkAllocation allocation;
      gtk_widget_get_allocation (widget, &allocation);
      outside = event->x < 0 || event->y < 0 || event->x > allocation.width || event->y > allocation.height;
    } else outside = gtk_widget_get_toplevel (target) != widget;
    if (outside) g_signal_emit (widget, cancel_signal, 0);
    return outside;
  });
}
gboolean key_press (GtkWidget *widget, GdkEventKey *event)
{
  return boundary<gboolean> (nullptr, FALSE, [&] () -> gboolean {
    if (!event || !active (G_OBJECT (widget))) return FALSE;
    auto lease = ObjectRef<GObject>::retain (G_OBJECT (widget));
    auto *bindings = gtk_binding_set_by_class (G_OBJECT_GET_CLASS (widget));
    if (gtk_binding_set_activate (bindings, event->keyval, GdkModifierType (event->state), G_OBJECT (widget))) return TRUE;
    auto parent = GTK_WIDGET_CLASS (gimp_popover_parent_class)->key_press_event;
    return parent ? parent (widget, event) : FALSE;
  });
}
}
static void gimp_popover_class_init (GimpPopover2Class *klass)
{
  auto *object = G_OBJECT_CLASS (klass);
  object->constructed = constructed; object->dispose = dispose;
  auto *widget = GTK_WIDGET_CLASS (klass);
  widget->map = map; widget->unmap = unmap;
  widget->button_press_event = button_press; widget->key_press_event = key_press;
  klass->cancel = close_window; klass->confirm = close_window;
  cancel_signal = g_signal_new ("cancel", G_OBJECT_CLASS_TYPE (klass),
    GSignalFlags (G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION), G_STRUCT_OFFSET (GimpPopoverClass, cancel),
    nullptr, nullptr, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
  confirm_signal = g_signal_new ("confirm", G_OBJECT_CLASS_TYPE (klass),
    GSignalFlags (G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION), G_STRUCT_OFFSET (GimpPopoverClass, confirm),
    nullptr, nullptr, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
  gtk_binding_entry_add_signal (gtk_binding_set_by_class (klass), GDK_KEY_Escape, GdkModifierType (0), "cancel", 0);
  /* Historical Return/space confirmation bindings were disabled. */
}
static void gimp_popover_init (GimpPopover2 *popover)
{
  if (!boundary<bool> (nullptr, false, [&] {
    BindingStore::ensure (G_OBJECT (popover)).emplace<PopupSlot> (); return true;
  })) gimp_painter_binding_close (G_OBJECT (popover), nullptr);
}
GtkWidget *gimp_popover_new (GtkWidget *view)
{
  return boundary<GtkWidget *> (nullptr, nullptr, [&] () -> GtkWidget * {
    if (!GTK_IS_WIDGET (view) || GTK_IS_WINDOW (view) || gtk_widget_get_parent (view) || gtk_widget_in_destruction (view)) return nullptr;
    auto view_lease = ObjectRef<GObject>::retain (G_OBJECT (view));
    auto *widget = GTK_WIDGET (g_object_new (GIMP_TYPE_POPOVER, "type", GTK_WINDOW_POPUP, nullptr));
    /* GtkWindow owns its nonfloating top-level reference. This is an extra
     * factory lease, not adoption of GTK's reference. */
    auto owner_lease = ObjectRef<GObject>::retain (G_OBJECT (widget));
    const bool ready = boundary<bool> (nullptr, false, [&] {
      if (!active (G_OBJECT (widget))) return false;
      auto frame = BindingStore::require (G_OBJECT (widget)).with<PopupSlot> ([] (Popup& popup) { return popup.frame.lock (); });
      if (!frame || gtk_widget_get_parent (GTK_WIDGET (frame.get ())) != widget) return false;
      gtk_window_set_resizable (GTK_WINDOW (widget), FALSE);
      if (!active (G_OBJECT (widget))) return false;
      gtk_container_add (GTK_CONTAINER (frame.get ()), view);
      if (!active (G_OBJECT (widget)) || gtk_widget_get_parent (view) != GTK_WIDGET (frame.get ())) return false;
      gtk_widget_show (view);
      if (!active (G_OBJECT (widget)) || gtk_widget_get_parent (view) != GTK_WIDGET (frame.get ())) return false;
      gtk_widget_grab_focus (view);
      return active (G_OBJECT (widget));
    });
    if (!ready) { gtk_widget_destroy (widget); return nullptr; }
    return widget;
  });
}
