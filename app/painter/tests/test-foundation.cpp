/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "test-registry.hpp"
#include "test-c-api.h"
#include "test-fixture-traits.hpp"
#include "gimp-painter-binding.h"
#include "gimp-painter-binding.h"
#include "binding-store.hpp"
#include "resources.hpp"
#include <functional>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <vector>

using namespace GimpPainter;

namespace {
struct Counters { int close = 0; int destroy = 0; int alive = 0; };
struct Impl
{
  explicit Impl (Counters& counters) : counters (counters) { ++counters.alive; }
  ~Impl () { --counters.alive; ++counters.destroy; }
  void close () noexcept { ++counters.close; }
  int value = 41;
  Counters& counters;
};
struct MainSlot : SlotSpec<GObject, Impl> {};
struct SecondSlot : SlotSpec<GObject, Impl> {};
struct AbsentSlot : SlotSpec<GObject, Impl> {};
struct WrongOwnerSlot : SlotSpec<PainterFixture, Impl> {};
Counters lookup_counters;
int lookup_constructed = 0;
struct LookupImpl
{
  LookupImpl () { ++lookup_constructed; ++lookup_counters.alive; }
  ~LookupImpl () { --lookup_counters.alive; ++lookup_counters.destroy; }
  void close () noexcept { ++lookup_counters.close; }
  int value = 0;
};
struct LookupSlot : SlotSpec<GObject, LookupImpl> {};
struct MissingLookupSlot : SlotSpec<GObject, LookupImpl> {};
struct CloseObserver
{
  CloseObserver (Counters& counts, std::function<void ()> callback)
    : counts (counts), callback (std::move (callback)) { ++counts.alive; }
  ~CloseObserver () { --counts.alive; ++counts.destroy; }
  void close () noexcept { ++counts.close; callback (); }
  Counters& counts;
  std::function<void ()> callback;
};
struct CloseObserverSlot : SlotSpec<GObject, CloseObserver> {};
struct FailingImpl
{
  FailingImpl () { throw std::runtime_error ("injected constructor failure"); }
  void close () noexcept {}
};
struct FailingSlot : SlotSpec<GObject, FailingImpl> {};

ObjectRef<GObject> new_object ()
{ return ObjectRef<GObject>::adopt (G_OBJECT (g_object_new (G_TYPE_OBJECT, nullptr))); }

void weak_notify (gpointer data, GObject *) { ++*static_cast<int *> (data); }

template<class Function> void expect (GimpPainterError code, Function&& function)
{
  bool threw = false;
  try { function (); }
  catch (const Error& error) { threw = true; g_assert_cmpint (error.code (), ==, code); }
  g_assert_true (threw);
}

void references ()
{
  int destroyed = 0;
  auto owner = new_object ();
  g_object_weak_ref (owner.get (), weak_notify, &destroyed);
  {
    auto retained = ObjectRef<GObject>::retain (owner.get ());
    auto copy = retained;
    auto moved = std::move (copy);
    g_assert_false (static_cast<bool> (copy));
    moved = moved;
    auto *same = &moved;
    moved = std::move (*same);
    ObjectRef<GObject> assigned;
    assigned = moved;
    owner.reset ();
    g_assert_cmpint (destroyed, ==, 0);
    g_assert_true (static_cast<bool> (assigned));
  }
  g_assert_cmpint (destroyed, ==, 1);
  auto empty = ObjectRef<GObject>::adopt (nullptr);
  auto empty_copy = empty;
  empty_copy = std::move (empty);
  g_assert_false (static_cast<bool> (empty_copy));
}

void retain_factory ()
{
  auto empty = ObjectRef<GObject>::retain (nullptr);
  g_assert_null (empty.get ());
  g_assert_false (static_cast<bool> (empty));
  empty.reset ();

  for (GType type : {G_TYPE_OBJECT, G_TYPE_INITIALLY_UNOWNED})
    {
      int destroyed = 0;
      auto *raw = G_OBJECT (g_object_new (type, nullptr));
      const bool was_floating = g_object_is_floating (raw);
      g_object_weak_ref (raw, weak_notify, &destroyed);
      g_assert_cmpuint (raw->ref_count, ==, 1);

      auto retained = ObjectRef<GObject>::retain (raw);
      g_assert_true (retained.get () == raw);
      g_assert_cmpuint (raw->ref_count, ==, 2);
      g_assert_cmpint (g_object_is_floating (raw), ==, was_floating);
      // Retain never consumes a floating reference. The original caller can
      // sink its own reference afterward without acquiring a third one.
      if (was_floating)
        {
          g_object_ref_sink (raw);
          g_assert_false (g_object_is_floating (raw));
          g_assert_cmpuint (raw->ref_count, ==, 2);
        }
      g_object_unref (raw);
      g_assert_cmpint (destroyed, ==, 0);
      g_assert_cmpuint (retained.get ()->ref_count, ==, 1);
      retained.reset ();
      g_assert_cmpint (destroyed, ==, 1);
      retained.reset ();
      g_assert_cmpint (destroyed, ==, 1);
    }
}

void adopt_factory ()
{
  auto empty = ObjectRef<GObject>::adopt (nullptr);
  g_assert_null (empty.get ());
  g_assert_false (static_cast<bool> (empty));

  for (GType type : {G_TYPE_OBJECT, G_TYPE_INITIALLY_UNOWNED})
    {
      int destroyed = 0;
      auto *raw = G_OBJECT (g_object_new (type, nullptr));
      const bool was_floating = g_object_is_floating (raw);
      g_object_weak_ref (raw, weak_notify, &destroyed);
      {
        auto adopted = ObjectRef<GObject>::adopt (raw);
        g_assert_true (adopted.get () == raw);
        g_assert_cmpuint (raw->ref_count, ==, 1);
        g_assert_cmpint (g_object_is_floating (raw), ==, was_floating);
        if (was_floating)
          {
            g_object_ref_sink (adopted.get ());
            g_assert_cmpuint (raw->ref_count, ==, 1);
          }
      }
      g_assert_cmpint (destroyed, ==, 1);
    }

  int destroyed = 0;
  auto *caller = G_OBJECT (g_object_new (G_TYPE_OBJECT, nullptr));
  g_object_weak_ref (caller, weak_notify, &destroyed);
  {
    // A native C producer has already supplied a distinct owned reference.
    auto *transferred = G_OBJECT (g_object_ref (caller));
    auto adopted = ObjectRef<GObject>::adopt (transferred);
    g_assert_true (adopted.get () == caller);
    g_assert_cmpuint (caller->ref_count, ==, 2);
  }
  g_assert_cmpuint (caller->ref_count, ==, 1);
  g_assert_cmpint (destroyed, ==, 0);
  g_object_unref (caller);
  g_assert_cmpint (destroyed, ==, 1);
}

void sink_factory ()
{
  auto empty = ObjectRef<GObject>::sink (nullptr);
  g_assert_null (empty.get ());
  g_assert_false (static_cast<bool> (empty));

  for (GType type : {G_TYPE_OBJECT, G_TYPE_INITIALLY_UNOWNED})
    {
      int destroyed = 0;
      auto *raw = G_OBJECT (g_object_new (type, nullptr));
      const bool was_floating = g_object_is_floating (raw);
      g_object_weak_ref (raw, weak_notify, &destroyed);
      g_assert_cmpuint (raw->ref_count, ==, 1);
      auto sunk = ObjectRef<GObject>::sink (raw);
      g_assert_true (sunk.get () == raw);
      g_assert_false (g_object_is_floating (raw));
      g_assert_cmpuint (raw->ref_count, ==, was_floating ? 1 : 2);
      // A floating reference was transferred; a nonfloating caller still
      // owns its separate original reference and must release it itself.
      if (!was_floating) g_object_unref (raw);
      g_assert_cmpint (destroyed, ==, 0);
      g_assert_cmpuint (sunk.get ()->ref_count, ==, 1);
      sunk.reset ();
      g_assert_cmpint (destroyed, ==, 1);
      sunk.reset ();
      g_assert_cmpint (destroyed, ==, 1);
    }
}

void copy_move_contract ()
{
  using Ref = ObjectRef<GObject>;
  static_assert (std::is_nothrow_copy_constructible<Ref>::value, "copy construction");
  static_assert (std::is_nothrow_copy_assignable<Ref>::value, "copy assignment");
  static_assert (std::is_nothrow_move_constructible<Ref>::value, "move construction");
  static_assert (std::is_nothrow_move_assignable<Ref>::value, "move assignment");

  for (GType type : {G_TYPE_OBJECT, G_TYPE_INITIALLY_UNOWNED})
    {
      int destroyed = 0, displaced = 0, moved_over = 0;
      auto owner = Ref::adopt (G_OBJECT (g_object_new (type, nullptr)));
      auto *raw = owner.get ();
      const bool floating = g_object_is_floating (raw);
      g_object_weak_ref (raw, weak_notify, &destroyed);
      g_assert_cmpuint (raw->ref_count, ==, 1);
      Ref copy (owner);
      g_assert_true (copy.get () == raw);
      g_assert_cmpuint (raw->ref_count, ==, 2);
      Ref moved (std::move (copy));
      g_assert_null (copy.get ());
      g_assert_true (moved.get () == raw);
      g_assert_cmpuint (raw->ref_count, ==, 2);
      auto *self = &moved;
      moved = *self;
      g_assert_cmpuint (raw->ref_count, ==, 2);
      moved = std::move (*self);
      g_assert_true (moved.get () == raw);
      g_assert_cmpuint (raw->ref_count, ==, 2);

      auto assigned = new_object ();
      g_object_weak_ref (assigned.get (), weak_notify, &displaced);
      assigned = moved;
      g_assert_cmpint (displaced, ==, 1);
      g_assert_true (assigned.get () == raw);
      g_assert_cmpuint (raw->ref_count, ==, 3);
      assigned = moved; // Distinct wrappers already own the same object.
      g_assert_cmpuint (raw->ref_count, ==, 3);
      Ref duplicate (owner);
      g_assert_cmpuint (raw->ref_count, ==, 4);
      assigned = std::move (duplicate);
      g_assert_null (duplicate.get ());
      g_assert_true (assigned.get () == raw);
      g_assert_cmpuint (raw->ref_count, ==, 3);

      Ref empty;
      Ref empty_copy (empty);
      Ref empty_moved (std::move (empty_copy));
      g_assert_null (empty_copy.get ());
      g_assert_null (empty_moved.get ());
      assigned = empty;
      g_assert_null (assigned.get ());
      g_assert_cmpuint (raw->ref_count, ==, 2);
      moved = std::move (empty_moved);
      g_assert_null (moved.get ());
      g_assert_cmpuint (raw->ref_count, ==, 1);

      auto destination = new_object ();
      g_object_weak_ref (destination.get (), weak_notify, &moved_over);
      destination = std::move (owner);
      g_assert_null (owner.get ());
      g_assert_true (destination.get () == raw);
      g_assert_cmpuint (raw->ref_count, ==, 1);
      g_assert_cmpint (moved_over, ==, 1);
      g_assert_cmpint (g_object_is_floating (raw), ==, floating);
      g_assert_cmpint (destroyed, ==, 0);
      if (floating) g_object_ref_sink (raw);
      destination.reset ();
      g_assert_cmpint (destroyed, ==, 1);
    }
}

void floating ()
{
  int destroyed = 0;
  auto *raw = G_OBJECT (g_object_new (G_TYPE_INITIALLY_UNOWNED, nullptr));
  g_assert_true (g_object_is_floating (raw));
  g_object_weak_ref (raw, weak_notify, &destroyed);
  {
    auto sunk = ObjectRef<GObject>::sink (raw);
    g_assert_false (g_object_is_floating (raw));
    auto ref_nonfloating = ObjectRef<GObject>::sink (raw);
    sunk.reset ();
    g_assert_cmpint (destroyed, ==, 0);
  }
  g_assert_cmpint (destroyed, ==, 1);
}

void weak_handles ()
{
  using Weak = WeakRef<GObject>;
  static_assert (!std::is_copy_constructible<Weak>::value, "weak handle is move-only");
  static_assert (!std::is_copy_assignable<Weak>::value, "weak handle is move-only");
  static_assert (std::is_nothrow_move_constructible<Weak>::value, "weak move construction");
  static_assert (std::is_nothrow_move_assignable<Weak>::value, "weak move assignment");
  Weak empty;
  g_assert_null (empty.lock ().get ());
  ObjectRef<GObject> no_owner;
  Weak null_owner (no_owner);
  g_assert_null (null_owner.lock ().get ());
  null_owner.reset ();
  null_owner.reset ();

  for (GType type : {G_TYPE_OBJECT, G_TYPE_INITIALLY_UNOWNED})
    {
      int finalized = 0;
      auto owner = ObjectRef<GObject>::adopt (G_OBJECT (g_object_new (type, nullptr)));
      auto *raw = owner.get ();
      const bool floating = g_object_is_floating (raw);
      g_object_weak_ref (raw, weak_notify, &finalized);
      Weak weak (owner), independent (owner);
      g_assert_cmpuint (raw->ref_count, ==, 1);
      {
        Weak scoped (owner);
        g_assert_cmpuint (raw->ref_count, ==, 1);
      }
      g_assert_cmpuint (raw->ref_count, ==, 1);
      Weak moved (std::move (weak));
      g_assert_null (weak.lock ().get ());
      g_assert_cmpuint (raw->ref_count, ==, 1);
      auto temporary = moved.lock ();
      g_assert_true (temporary.get () == raw);
      g_assert_cmpuint (raw->ref_count, ==, 2);
      g_assert_cmpint (g_object_is_floating (raw), ==, floating);
      {
        auto second = independent.lock ();
        g_assert_true (second.get () == raw);
        g_assert_cmpuint (raw->ref_count, ==, 3);
      }
      g_assert_cmpuint (raw->ref_count, ==, 2);
      if (floating) g_object_ref_sink (raw);
      owner.reset ();
      g_assert_cmpuint (raw->ref_count, ==, 1);
      g_assert_cmpint (finalized, ==, 0);
      moved.reset (); // Removing a weak observer cannot release the strong lock.
      g_assert_cmpuint (raw->ref_count, ==, 1);
      g_assert_null (moved.lock ().get ());
      temporary.reset ();
      g_assert_cmpint (finalized, ==, 1);
      for (int i = 0; i != 3; ++i) g_assert_null (independent.lock ().get ());
      independent.reset ();
      g_assert_cmpint (finalized, ==, 1);
    }

  int first_finalized = 0, second_finalized = 0;
  auto first = new_object (), second = new_object ();
  g_object_weak_ref (first.get (), weak_notify, &first_finalized);
  g_object_weak_ref (second.get (), weak_notify, &second_finalized);
  Weak destination (first), source (second);
  destination = std::move (source);
  g_assert_null (source.lock ().get ());
  g_assert_cmpuint (first.get ()->ref_count, ==, 1);
  g_assert_cmpuint (second.get ()->ref_count, ==, 1);
  auto *self = &destination;
  destination = std::move (*self);
  {
    auto lock = destination.lock ();
    g_assert_true (lock.get () == second.get ());
    g_assert_cmpuint (second.get ()->ref_count, ==, 2);
  }
  first.reset ();
  g_assert_cmpint (first_finalized, ==, 1);
  second.reset ();
  g_assert_cmpint (second_finalized, ==, 1);
  g_assert_null (destination.lock ().get ());
}

void store_registration ()
{
  const GQuark reserved = g_quark_from_static_string ("gimp-painter-binding-store-v1");
  const GQuark foreign = g_quark_from_static_string ("painter-test-unrelated-owner-data");
  int first_finalized = 0, second_finalized = 0;
  int first_foreign_destroyed = 0, second_foreign_destroyed = 0;
  Counters first_stats, second_stats;
  auto first = new_object (), second = new_object ();
  g_object_weak_ref (first.get (), weak_notify, &first_finalized);
  g_object_weak_ref (second.get (), weak_notify, &second_finalized);
  auto destroy_foreign = [] (gpointer data) { ++*static_cast<int *> (data); };
  g_object_set_qdata_full (first.get (), foreign, &first_foreign_destroyed, destroy_foreign);
  g_object_set_qdata_full (second.get (), foreign, &second_foreign_destroyed, destroy_foreign);
  g_assert_null (BindingStore::find (nullptr));
  g_assert_null (BindingStore::find (first.get ()));
  g_assert_null (g_object_get_qdata (first.get (), reserved));
  expect (GIMP_PAINTER_ERROR_MISSING_SLOT, [&] { BindingStore::require (first.get ()); });
  g_assert_null (g_object_get_qdata (first.get (), reserved));

  auto& first_store = BindingStore::ensure (first.get ());
  auto& second_store = BindingStore::ensure (second.get ());
  g_assert_true (&first_store != &second_store);
  g_assert_true (g_object_get_qdata (first.get (), reserved) == &first_store);
  g_assert_true (g_object_get_qdata (second.get (), reserved) == &second_store);
  for (int i = 0; i != 3; ++i)
    {
      g_assert_true (&BindingStore::ensure (first.get ()) == &first_store);
      g_assert_true (&BindingStore::require (first.get ()) == &first_store);
      g_assert_true (BindingStore::find (first.get ()) == &first_store);
      g_assert_cmpuint (first.get ()->ref_count, ==, 1);
    }
  first_store.emplace<MainSlot> (first_stats);
  second_store.emplace<MainSlot> (second_stats);
  first_store.activate ();
  second_store.activate ();
  const auto generation = first_store.generation ();
  g_assert_true (&BindingStore::ensure (first.get ()) == &first_store);
  g_assert_true (first_store.state () == BindingStore::State::active);
  g_assert_cmpuint (first_store.generation (), ==, generation);
  first_store.close ();
  g_assert_true (&BindingStore::ensure (first.get ()) == &first_store);
  g_assert_true (first_store.state () == BindingStore::State::closed);
  g_assert_cmpuint (first_store.generation (), ==, generation + 1);
  g_assert_true (second_store.state () == BindingStore::State::active);
  g_assert_cmpint (second_store.with<MainSlot> ([] (Impl& value) { return value.value; }), ==, 41);
  g_assert_true (g_object_get_qdata (first.get (), foreign) == &first_foreign_destroyed);
  g_assert_true (g_object_get_qdata (second.get (), foreign) == &second_foreign_destroyed);
  g_assert_cmpint (first_foreign_destroyed, ==, 0);
  g_assert_cmpint (second_foreign_destroyed, ==, 0);
  g_assert_cmpuint (first.get ()->ref_count, ==, 1);
  g_assert_cmpuint (second.get ()->ref_count, ==, 1);
  first.reset ();
  g_assert_cmpint (first_finalized, ==, 1);
  g_assert_cmpint (first_foreign_destroyed, ==, 1);
  g_assert_cmpint (first_stats.destroy, ==, 1);
  g_assert_cmpint (second_finalized, ==, 0);
  g_assert_cmpint (second_foreign_destroyed, ==, 0);
  g_assert_cmpint (second_stats.destroy, ==, 0);
  g_assert_true (BindingStore::find (second.get ()) == &second_store);
  second.reset ();
  g_assert_cmpint (second_finalized, ==, 1);
  g_assert_cmpint (second_foreign_destroyed, ==, 1);
  g_assert_cmpint (second_stats.destroy, ==, 1);
}

void store_lifetime ()
{
  Counters counters;
  auto owner = new_object ();
  auto& store = BindingStore::ensure (owner.get ());
  g_assert_true (&store == &BindingStore::ensure (owner.get ()));
  g_assert_true (&store == &BindingStore::require (owner.get ()));
  store.emplace<MainSlot> (counters);
  expect (GIMP_PAINTER_ERROR_DUPLICATE_SLOT, [&] { store.emplace<MainSlot> (counters); });
  expect (GIMP_PAINTER_ERROR_INVALID_STATE, [&] { store.with<MainSlot> ([] (Impl&) {}); });
  store.activate ();
  g_assert_cmpint (store.with<MainSlot> ([] (Impl& impl) { return ++impl.value; }), ==, 42);
  const auto generation = store.generation ();
  g_assert_true (store.accepts (generation));
  expect (GIMP_PAINTER_ERROR_MISSING_SLOT, [&] { store.with<AbsentSlot> ([] (Impl&) {}); });
  expect (GIMP_PAINTER_ERROR_INVALID_STATE, [&] { store.emplace<SecondSlot> (counters); });
  store.close ();
  store.close ();
  g_assert_false (store.accepts (generation));
  expect (GIMP_PAINTER_ERROR_CLOSED, [&] { store.with<MainSlot> ([] (Impl&) {}); });
  g_assert_cmpint (store.read<MainSlot> ([] (const Impl& impl) { return impl.value; }), ==, 42);
  g_assert_cmpint (counters.close, ==, 1);
  g_assert_cmpint (counters.destroy, ==, 0);
  owner.reset ();
  g_assert_cmpint (counters.destroy, ==, 1);
}

void multiple_slots ()
{
  Counters counters;
  auto owner = new_object ();
  auto& store = BindingStore::ensure (owner.get ());
  store.emplace<MainSlot> (counters);
  store.emplace<SecondSlot> (counters);
  store.activate ();
  store.with<MainSlot> ([] (Impl& impl) { impl.value = 7; });
  g_assert_cmpint (store.with<SecondSlot> ([] (Impl& impl) { return impl.value; }), ==, 41);
  owner.reset ();
  g_assert_cmpint (counters.close, ==, 2);
  g_assert_cmpint (counters.destroy, ==, 2);
}

void slot_registration ()
{
  Counters installed, rejected;
  auto owner = new_object ();
  auto& store = BindingStore::ensure (owner.get ());
  const auto generation = store.generation ();
  auto rejection_has_no_effect = [&] {
    g_assert_cmpint (rejected.alive, ==, 0);
    g_assert_cmpint (rejected.close, ==, 0);
    g_assert_cmpint (rejected.destroy, ==, 0);
    g_assert_cmpuint (owner.get ()->ref_count, ==, 1);
  };
  expect (GIMP_PAINTER_ERROR_WRONG_TYPE, [&] { store.emplace<WrongOwnerSlot> (rejected); });
  rejection_has_no_effect ();
  g_assert_true (store.state () == BindingStore::State::constructing);
  g_assert_cmpuint (store.generation (), ==, generation);
  store.emplace<MainSlot> (installed);
  store.initialize<MainSlot> ([] (Impl& value) { value.value = 73; });
  expect (GIMP_PAINTER_ERROR_DUPLICATE_SLOT, [&] { store.emplace<MainSlot> (rejected); });
  rejection_has_no_effect ();
  g_assert_cmpint (installed.alive, ==, 1);
  g_assert_cmpint (store.read<MainSlot> ([] (const Impl& value) { return value.value; }), ==, 73);
  g_assert_cmpuint (store.generation (), ==, generation);
  store.activate ();
  expect (GIMP_PAINTER_ERROR_INVALID_STATE, [&] { store.emplace<SecondSlot> (rejected); });
  rejection_has_no_effect ();
  g_assert_true (store.state () == BindingStore::State::active);
  g_assert_cmpint (store.with<MainSlot> ([] (Impl& value) { return value.value; }), ==, 73);
  expect (GIMP_PAINTER_ERROR_MISSING_SLOT, [&] { store.read<SecondSlot> ([] (const Impl&) {}); });
  store.close ();
  const auto closed_generation = store.generation ();
  expect (GIMP_PAINTER_ERROR_INVALID_STATE, [&] { store.emplace<SecondSlot> (rejected); });
  expect (GIMP_PAINTER_ERROR_INVALID_STATE, [&] { store.emplace<MainSlot> (rejected); });
  rejection_has_no_effect ();
  g_assert_true (store.state () == BindingStore::State::closed);
  g_assert_cmpuint (store.generation (), ==, closed_generation);
  g_assert_cmpint (store.read<MainSlot> ([] (const Impl& value) { return value.value; }), ==, 73);
  expect (GIMP_PAINTER_ERROR_MISSING_SLOT, [&] { store.read<SecondSlot> ([] (const Impl&) {}); });
  g_assert_cmpint (installed.close, ==, 1);
  g_assert_cmpint (installed.destroy, ==, 0);
  owner.reset ();
  g_assert_cmpint (installed.destroy, ==, 1);
  g_assert_cmpint (rejected.destroy, ==, 0);
}

void slot_lookup ()
{
  lookup_counters = {};
  lookup_constructed = 0;
  int missing_callbacks = 0;
  auto owner = new_object ();
  auto& store = BindingStore::ensure (owner.get ());
  const auto generation = store.generation ();
  expect (GIMP_PAINTER_ERROR_MISSING_SLOT, [&] {
    store.initialize<LookupSlot> ([&] (LookupImpl&) { ++missing_callbacks; });
  });
  expect (GIMP_PAINTER_ERROR_MISSING_SLOT, [&] {
    store.read<LookupSlot> ([&] (const LookupImpl&) { ++missing_callbacks; });
  });
  g_assert_cmpint (lookup_constructed, ==, 0);
  g_assert_cmpint (lookup_counters.alive, ==, 0);
  g_assert_cmpint (lookup_counters.destroy, ==, 0);
  g_assert_cmpint (missing_callbacks, ==, 0);
  g_assert_cmpuint (owner.get ()->ref_count, ==, 1);
  g_assert_cmpuint (store.generation (), ==, generation);
  g_assert_true (store.state () == BindingStore::State::constructing);
  // Only this explicit registration constructs the default-constructible Impl.
  store.emplace<LookupSlot> ();
  g_assert_cmpint (lookup_constructed, ==, 1);
  store.initialize<LookupSlot> ([] (LookupImpl& value) { value.value = 57; });
  store.activate ();
  g_assert_cmpint (store.with<LookupSlot> ([] (LookupImpl& value) { return value.value; }), ==, 57);
  expect (GIMP_PAINTER_ERROR_MISSING_SLOT, [&] {
    store.with<MissingLookupSlot> ([&] (LookupImpl&) { ++missing_callbacks; });
  });
  expect (GIMP_PAINTER_ERROR_MISSING_SLOT, [&] {
    store.read<MissingLookupSlot> ([&] (const LookupImpl&) { ++missing_callbacks; });
  });
  g_assert_cmpint (lookup_constructed, ==, 1);
  g_assert_cmpint (missing_callbacks, ==, 0);
  g_assert_cmpint (lookup_counters.close, ==, 0);
  g_assert_cmpuint (store.generation (), ==, generation);
  g_assert_true (store.state () == BindingStore::State::active);
  g_assert_cmpuint (owner.get ()->ref_count, ==, 1);
  store.close ();
  const auto closed_generation = store.generation ();
  expect (GIMP_PAINTER_ERROR_MISSING_SLOT, [&] {
    store.read<MissingLookupSlot> ([&] (const LookupImpl&) { ++missing_callbacks; });
  });
  g_assert_cmpint (store.read<LookupSlot> ([] (const LookupImpl& value) { return value.value; }), ==, 57);
  g_assert_cmpint (lookup_constructed, ==, 1);
  g_assert_cmpint (lookup_counters.alive, ==, 1);
  g_assert_cmpint (lookup_counters.close, ==, 1);
  g_assert_cmpint (lookup_counters.destroy, ==, 0);
  g_assert_cmpint (missing_callbacks, ==, 0);
  g_assert_cmpuint (owner.get ()->ref_count, ==, 1);
  g_assert_true (store.state () == BindingStore::State::closed);
  g_assert_cmpuint (store.generation (), ==, closed_generation);
  owner.reset ();
  g_assert_cmpint (lookup_constructed, ==, 1);
  g_assert_cmpint (lookup_counters.alive, ==, 0);
  g_assert_cmpint (lookup_counters.destroy, ==, 1);
}

void implementation_ownership ()
{
  // Cover construction teardown, active teardown fallback and explicit close.
  for (int mode = 0; mode != 3; ++mode)
    {
      Counters first, second;
      int finalized = 0;
      auto owner = new_object ();
      g_object_weak_ref (owner.get (), weak_notify, &finalized);
      auto& store = BindingStore::ensure (owner.get ());
      store.emplace<MainSlot> (first);
      store.emplace<SecondSlot> (second);
      if (mode != 0) store.activate ();
      if (mode == 2)
        {
          store.close ();
          store.close ();
        }
      for (const auto *counter : {&first, &second})
        {
          g_assert_cmpint (counter->alive, ==, 1);
          g_assert_cmpint (counter->close, ==, mode == 2 ? 1 : 0);
          g_assert_cmpint (counter->destroy, ==, 0);
        }
      auto final_owner = ObjectRef<GObject>::retain (owner.get ());
      g_assert_cmpuint (owner.get ()->ref_count, ==, 2);
      owner.reset ();
      g_assert_cmpuint (final_owner.get ()->ref_count, ==, 1);
      g_assert_cmpint (finalized, ==, 0);
      for (const auto *counter : {&first, &second})
        {
          g_assert_cmpint (counter->alive, ==, 1);
          g_assert_cmpint (counter->destroy, ==, 0);
        }
      final_owner.reset ();
      g_assert_cmpint (finalized, ==, 1);
      for (const auto *counter : {&first, &second})
        {
          g_assert_cmpint (counter->alive, ==, 0);
          g_assert_cmpint (counter->close, ==, 1);
          g_assert_cmpint (counter->destroy, ==, 1);
        }
    }
}

void close_transitions ()
{
  for (bool activate : {false, true})
    {
      Counters observer, regular;
      auto owner = new_object ();
      auto& store = BindingStore::ensure (owner.get ());
      const auto original_generation = store.generation ();
      store.emplace<MainSlot> (regular);
      store.emplace<CloseObserverSlot> (observer, [&] {
        g_assert_true (store.state () == BindingStore::State::closing);
        g_assert_cmpuint (store.generation (), ==, original_generation + 1);
        g_assert_false (store.accepts (original_generation));
        store.close ();
        store.close ();
        g_assert_true (store.state () == BindingStore::State::closing);
        g_assert_cmpuint (store.generation (), ==, original_generation + 1);
        g_assert_cmpint (observer.close, ==, 1);
        g_assert_cmpint (observer.destroy, ==, 0);
        expect (GIMP_PAINTER_ERROR_CLOSED, [&] { store.with<MainSlot> ([] (Impl&) {}); });
        expect (GIMP_PAINTER_ERROR_INVALID_STATE, [&] { store.activate (); });
      });
      if (activate)
        {
          store.activate ();
          g_assert_true (store.accepts (original_generation));
        }
      else g_assert_false (store.accepts (original_generation));
      store.close ();
      g_assert_true (store.state () == BindingStore::State::closed);
      g_assert_cmpuint (store.generation (), ==, original_generation + 1);
      g_assert_false (store.accepts (store.generation ()));
      store.close ();
      store.close ();
      g_assert_cmpuint (store.generation (), ==, original_generation + 1);
      g_assert_cmpuint (owner.get ()->ref_count, ==, 1);
      g_assert_cmpint (store.read<MainSlot> ([] (const Impl& value) { return value.value; }), ==, 41);
      for (const auto *counter : {&observer, &regular})
        {
          g_assert_cmpint (counter->close, ==, 1);
          g_assert_cmpint (counter->alive, ==, 1);
          g_assert_cmpint (counter->destroy, ==, 0);
        }
      owner.reset ();
      for (const auto *counter : {&observer, &regular})
        {
          g_assert_cmpint (counter->close, ==, 1);
          g_assert_cmpint (counter->alive, ==, 0);
          g_assert_cmpint (counter->destroy, ==, 1);
        }
    }
}

void reentrant_close ()
{
  Counters counters;
  auto owner = new_object ();
  auto& store = BindingStore::ensure (owner.get ());
  store.emplace<MainSlot> (counters);
  store.activate ();
  store.with<MainSlot> ([&] (Impl& impl) {
    store.close ();
    owner.reset ();
    g_assert_cmpint (counters.destroy, ==, 0);
    g_assert_cmpint (impl.value, ==, 41);
  });
  g_assert_cmpint (counters.close, ==, 1);
  g_assert_cmpint (counters.destroy, ==, 1);
}

void call_leases ()
{
  // initialize, with, active read, and closed read; return and unwind paths.
  for (int method = 0; method != 4; ++method)
    for (bool fail : {false, true})
      {
        Counters counters;
        int finalized = 0;
        auto owner = new_object ();
        auto *raw = owner.get ();
        g_object_weak_ref (raw, weak_notify, &finalized);
        auto& store = BindingStore::ensure (raw);
        store.emplace<MainSlot> (counters);
        if (method != 0) store.activate ();
        if (method == 3) store.close ();
        bool called = false, threw = false;
        int result = -1;
        auto call = [&] (auto& impl) {
          called = true;
          g_assert_cmpuint (raw->ref_count, ==, 2);
          store.close ();
          owner.reset ();
          g_assert_null (owner.get ());
          g_assert_cmpuint (raw->ref_count, ==, 1);
          g_assert_cmpint (finalized, ==, 0);
          g_assert_cmpint (counters.alive, ==, 1);
          g_assert_cmpint (counters.close, ==, 1);
          g_assert_cmpint (counters.destroy, ==, 0);
          g_assert_true (store.state () == BindingStore::State::closed);
          g_assert_cmpint (impl.value, ==, 41);
          if (fail) throw std::runtime_error ("injected after close and owner release");
          return impl.value;
        };
        try
          {
            if (method == 0) result = store.initialize<MainSlot> (call);
            else if (method == 1) result = store.with<MainSlot> (call);
            else result = store.read<MainSlot> (call);
          }
        catch (const std::runtime_error& error)
          {
            threw = true;
            g_assert_cmpstr (error.what (), ==, "injected after close and owner release");
          }
        g_assert_true (called);
        g_assert_cmpint (threw, ==, fail);
        g_assert_cmpint (result, ==, fail ? -1 : 41);
        g_assert_null (owner.get ());
        g_assert_cmpint (finalized, ==, 1);
        g_assert_cmpint (counters.alive, ==, 0);
        g_assert_cmpint (counters.close, ==, 1);
        g_assert_cmpint (counters.destroy, ==, 1);
        // The store and Impl have now died; do not access their old borrows.
      }
}

void failed_construction ()
{
  Counters counters;
  auto owner = new_object ();
  auto& store = BindingStore::ensure (owner.get ());
  store.emplace<MainSlot> (counters);
  bool failed = false;
  try { store.emplace<FailingSlot> (); }
  catch (const std::runtime_error&) { failed = true; }
  g_assert_true (failed);
  store.activate ();
  expect (GIMP_PAINTER_ERROR_MISSING_SLOT, [&] { store.with<FailingSlot> ([] (FailingImpl&) {}); });
  owner.reset ();
  g_assert_cmpint (counters.destroy, ==, 1);
}

struct ConstructionCounts
{
  int cpp_freed = 0;
  int object_freed = 0;
  int array_elements_freed = 0;
  int completed = 0;
  int closed = 0;
  int destroyed = 0;
};
struct ConstructionResource
{
  explicit ConstructionResource (ConstructionCounts& counts) : counts (counts) {}
  ~ConstructionResource () { ++counts.cpp_freed; }
  ConstructionCounts& counts;
};
void construction_array_clear (gpointer element)
{ ++**static_cast<int **> (element); }
ArrayRef construction_array (ConstructionCounts& counts)
{
  auto result = ArrayRef::adopt (g_array_new (FALSE, FALSE, sizeof (int *)));
  g_array_set_clear_func (result.get (), construction_array_clear);
  auto *counter = &counts.array_elements_freed;
  g_array_append_val (result.get (), counter);
  g_array_append_val (result.get (), counter);
  return result;
}
ObjectRef<GObject> construction_object (ConstructionCounts& counts)
{
  auto result = new_object ();
  g_object_weak_ref (result.get (), weak_notify, &counts.object_freed);
  return result;
}
struct AcquiredImpl
{
  AcquiredImpl (ConstructionCounts& counts, int failure,
                ObjectRef<GObject> *release_owner)
    : counts (counts), resource (new ConstructionResource (counts)),
      object (construction_object (counts)), text (g_strdup ("owned matcher")),
      patterns (construction_array (counts))
  {
    g_assert_cmpstr (text.get (), ==, "owned matcher");
    if (release_owner) release_owner->reset ();
    if (failure == 1) throw std::runtime_error ("after resource acquisition");
    if (failure == 2) throw std::bad_alloc ();
    ++counts.completed;
  }
  ~AcquiredImpl () { ++counts.destroyed; }
  void close () noexcept { ++counts.closed; }
  ConstructionCounts& counts;
  std::unique_ptr<ConstructionResource> resource;
  ObjectRef<GObject> object;
  String text;
  ArrayRef patterns;
};
struct AcquiredSlot : SlotSpec<GObject, AcquiredImpl> {};

void acquired_construction_failure ()
{
  // Failure must destroy completed members, not the unfinished Impl itself.
  // Both an empty store and a store with an existing slot remain reusable.
  for (bool prior : {false, true})
    for (int failure : {1, 2})
      for (bool abandon : {false, true})
        {
          Counters previous;
          ConstructionCounts counts;
          int finalized = 0;
          auto owner = new_object ();
          g_object_weak_ref (owner.get (), weak_notify, &finalized);
          auto& store = BindingStore::ensure (owner.get ());
          if (prior) store.emplace<MainSlot> (previous);
          const auto generation = store.generation ();
          bool threw = false;
          try { store.emplace<AcquiredSlot> (counts, failure, abandon ? &owner : nullptr); }
          catch (const std::bad_alloc&) { g_assert_cmpint (failure, ==, 2); threw = true; }
          catch (const std::runtime_error&) { g_assert_cmpint (failure, ==, 1); threw = true; }
          g_assert_true (threw);
          g_assert_cmpint (counts.cpp_freed, ==, 1);
          g_assert_cmpint (counts.object_freed, ==, 1);
          g_assert_cmpint (counts.array_elements_freed, ==, 2);
          g_assert_cmpint (counts.completed, ==, 0);
          g_assert_cmpint (counts.closed, ==, 0);
          g_assert_cmpint (counts.destroyed, ==, 0);
          if (!abandon)
            {
              g_assert_cmpint (finalized, ==, 0);
              g_assert_cmpuint (owner.get ()->ref_count, ==, 1);
              g_assert_true (BindingStore::find (owner.get ()) == &store);
              g_assert_true (store.state () == BindingStore::State::constructing);
              g_assert_cmpuint (store.generation (), ==, generation);
              expect (GIMP_PAINTER_ERROR_MISSING_SLOT, [&] {
                store.initialize<AcquiredSlot> ([] (AcquiredImpl&) {});
              });
              if (prior)
                g_assert_cmpint (store.initialize<MainSlot> ([] (Impl& impl) { return impl.value; }), ==, 41);
              // Same identity can be retried: no stale pending reservation.
              store.emplace<AcquiredSlot> (counts, 0, nullptr);
              store.activate ();
              store.with<AcquiredSlot> ([] (AcquiredImpl& impl) {
                g_assert_cmpstr (impl.text.get (), ==, "owned matcher");
                g_assert_cmpuint (impl.patterns.get ()->len, ==, 2);
              });
              store.close ();
              store.close ();
              owner.reset ();
              g_assert_cmpint (counts.cpp_freed, ==, 2);
              g_assert_cmpint (counts.object_freed, ==, 2);
              g_assert_cmpint (counts.array_elements_freed, ==, 4);
              g_assert_cmpint (counts.completed, ==, 1);
              g_assert_cmpint (counts.closed, ==, 1);
              g_assert_cmpint (counts.destroyed, ==, 1);
            }
          // In the abandon case the emplace lease released the last reference
          // during unwind; do not dereference the now-destroyed store.
          g_assert_null (owner.get ());
          g_assert_cmpint (finalized, ==, 1);
          g_assert_cmpint (previous.alive, ==, 0);
          g_assert_cmpint (previous.close, ==, prior ? 1 : 0);
          g_assert_cmpint (previous.destroy, ==, prior ? 1 : 0);
        }
}

void wrong_thread ()
{
  auto owner = new_object ();
  auto& store = BindingStore::ensure (owner.get ());
  bool rejected = false;
  std::thread worker ([&] {
    try { store.activate (); }
    catch (const Error& error) { rejected = error.code () == GIMP_PAINTER_ERROR_WRONG_THREAD; }
  });
  worker.join (); // test only; never an owner close implementation
  g_assert_true (rejected);
  store.activate ();
}

void c_close_boundary ()
{
  GError *error = nullptr;
  g_assert_false (gimp_painter_binding_close (nullptr, &error));
  g_assert_error (error, GIMP_PAINTER_ERROR, GIMP_PAINTER_ERROR_WRONG_TYPE);
  g_clear_error (&error);
  auto owner = new_object ();
  g_assert_true (gimp_painter_binding_close (owner.get (), &error));
  g_assert_null (BindingStore::find (owner.get ()));
  Counters counters;
  auto& store = BindingStore::ensure (owner.get ());
  store.emplace<MainSlot> (counters);
  store.activate ();
  g_assert_true (gimp_painter_binding_close (owner.get (), &error));
  g_assert_cmpint (counters.close, ==, 1);
  g_assert_no_error (error);
  owner.reset ();
}

void exception_conversion ()
{
  GError *error = nullptr;
  int value = boundary<int> (&error, -1, [] () -> int { throw 9; });
  g_assert_cmpint (value, ==, -1);
  g_assert_error (error, GIMP_PAINTER_ERROR, GIMP_PAINTER_ERROR_EXCEPTION);
  g_clear_error (&error);
  boundary_void (&error, [] { throw Error (GIMP_PAINTER_ERROR_CLOSED, "closed"); });
  g_assert_error (error, GIMP_PAINTER_ERROR, GIMP_PAINTER_ERROR_CLOSED);
  g_clear_error (&error);
  g_assert_cmpint (boundary<int> (nullptr, 8, [] () -> int { throw std::bad_alloc (); }), ==, 8);
}
}

gboolean painter_test_cpp_roundtrip (int value, int *result, GError **error)
{
  *result = 0;
  return boundary<gboolean> (error, FALSE, [&] {
    if (value < 0) throw std::runtime_error ("injected C++ failure");
    *result = painter_test_c_callback (value) + 1;
    return TRUE;
  });
}

gboolean painter_test_cpp_exception (PainterTestExceptionKind kind,
                                     gboolean use_void_boundary,
                                     int *destroyed, GError **error)
{
  struct Cleanup
  {
    int& destroyed;
    ~Cleanup () noexcept { ++destroyed; }
  };
  *destroyed = 0;
  auto fail = [&] () -> gboolean {
    Cleanup cleanup { *destroyed };
    switch (kind)
      {
      case PAINTER_TEST_EXCEPTION_TYPED:
        throw Error (GIMP_PAINTER_ERROR_CLOSED, "injected typed failure");
      case PAINTER_TEST_EXCEPTION_STANDARD:
        throw std::runtime_error ("injected standard failure");
      case PAINTER_TEST_EXCEPTION_ALLOCATION:
        throw std::bad_alloc ();
      case PAINTER_TEST_EXCEPTION_UNKNOWN:
        throw 23;
      }
    g_assert_not_reached ();
  };
  if (use_void_boundary)
    {
      gboolean result = FALSE;
      boundary_void (error, [&] { result = fail (); });
      return result;
    }
  return boundary<gboolean> (error, FALSE, fail);
}

void painter_test_register ()
{
  painter_test_register_resources ();
  painter_test_register_gobject ();
  painter_test_register_reentry ();
  painter_test_register_hierarchy ();
  g_test_add_func ("/painter/ref/copy-move-adopt-retain", references);
  g_test_add_func ("/painter/ref/retain-factory", retain_factory);
  g_test_add_func ("/painter/ref/adopt-factory", adopt_factory);
  g_test_add_func ("/painter/ref/sink-factory", sink_factory);
  g_test_add_func ("/painter/ref/copy-move-contract", copy_move_contract);
  g_test_add_func ("/painter/ref/floating-sink", floating);
  g_test_add_func ("/painter/ref/weak", weak_handles);
  g_test_add_func ("/painter/store/lifecycle", store_lifetime);
  g_test_add_func ("/painter/store/registration", store_registration);
  g_test_add_func ("/painter/store/multiple-slots", multiple_slots);
  g_test_add_func ("/painter/store/slot-registration", slot_registration);
  g_test_add_func ("/painter/store/slot-lookup", slot_lookup);
  g_test_add_func ("/painter/store/implementation-ownership", implementation_ownership);
  g_test_add_func ("/painter/store/close-transitions", close_transitions);
  g_test_add_func ("/painter/store/reentrant-close", reentrant_close);
  g_test_add_func ("/painter/store/call-leases", call_leases);
  g_test_add_func ("/painter/store/construction-failure", failed_construction);
  g_test_add_func ("/painter/store/acquired-construction-failure", acquired_construction_failure);
  g_test_add_func ("/painter/store/thread-rejection", wrong_thread);
  g_test_add_func ("/painter/boundary/c-close", c_close_boundary);
  g_test_add_func ("/painter/boundary/exception", exception_conversion);
}
