/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "binding-store.hpp"

namespace GimpPainter {
namespace {
GQuark store_key () noexcept
{
  return g_quark_from_static_string ("gimp-painter-binding-store-v1");
}
void destroy_store (gpointer data) noexcept
{
  delete static_cast<BindingStore *> (data);
}
}

BindingStore::BindingStore (GObject *owner) noexcept
  : owner_ (owner), thread_ (g_thread_ref (g_thread_self ())) {}

BindingStore::~BindingStore () noexcept
{
  finalizing_ = true;
  /* A GIMP/GTK owner must be finalized on its owning thread. Report misuse;
   * never throw from GLib's destroy notify or intentionally leak slot memory. */
  if (thread_ != g_thread_self ())
    g_critical ("Painter object finalized outside its owning thread");
  close_unchecked ();
  entries_.clear ();
  g_thread_unref (thread_);
}

BindingStore& BindingStore::ensure (GObject *owner)
{
  if (!G_IS_OBJECT (owner))
    throw Error (GIMP_PAINTER_ERROR_WRONG_TYPE, "Store requires a GObject owner");
  if (auto *existing = find (owner))
    { existing->check_thread (); return *existing; }
  std::unique_ptr<BindingStore> store (new BindingStore (owner));
  auto *result = store.get ();
  g_object_set_qdata_full (owner, store_key (), store.release (), destroy_store);
  return *result;
}

BindingStore *BindingStore::find (GObject *owner) noexcept
{
  return owner ? static_cast<BindingStore *> (g_object_get_qdata (owner, store_key ())) : nullptr;
}

BindingStore& BindingStore::require (GObject *owner)
{
  if (!G_IS_OBJECT (owner))
    throw Error (GIMP_PAINTER_ERROR_WRONG_TYPE, "Store requires a GObject owner");
  auto *store = find (owner);
  if (!store)
    throw Error (GIMP_PAINTER_ERROR_MISSING_SLOT, "Owner has no binding store");
  store->check_thread ();
  return *store;
}

void BindingStore::check_thread () const
{
  if (thread_ != g_thread_self ())
    throw Error (GIMP_PAINTER_ERROR_WRONG_THREAD, "Binding accessed outside its owner thread");
}

BindingStore::EntryBase *BindingStore::entry (const void *identity) const noexcept
{
  for (const auto& value : entries_)
    if (value->id == identity) return value.get ();
  return nullptr;
}

void BindingStore::activate ()
{
  check_thread ();
  if (state_ != State::constructing || constructing_slot_)
    throw Error (GIMP_PAINTER_ERROR_INVALID_STATE, "Binding cannot activate in this state");
  state_ = State::active;
}

void BindingStore::close_unchecked () noexcept
{
  if (state_ == State::closing || state_ == State::closed) return;
  state_ = State::closing;
  ++generation_;
  for (auto it = entries_.rbegin (); it != entries_.rend (); ++it)
    (*it)->close ();
  state_ = State::closed;
}

void BindingStore::close ()
{
  check_thread ();
  if (finalizing_) return;
  /* close hooks may synchronously release the caller's last reference. */
  auto owner_lease = ObjectRef<GObject>::retain (owner_);
  close_unchecked ();
}

} // namespace GimpPainter
