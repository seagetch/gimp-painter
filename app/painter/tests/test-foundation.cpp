/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "test-registry.hpp"
#include "test-c-api.h"
#include "gimp-painter-binding.h"
#include "gimp-painter-binding.h"
#include "binding-store.hpp"
#include <stdexcept>
#include <thread>
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
  auto owner = new_object ();
  WeakRef<GObject> weak (owner);
  WeakRef<GObject> moved (std::move (weak));
  g_assert_false (static_cast<bool> (weak.lock ()));
  auto temporary = moved.lock ();
  g_assert_true (temporary.get () == owner.get ());
  owner.reset ();
  g_assert_true (static_cast<bool> (moved.lock ()));
  temporary.reset ();
  g_assert_false (static_cast<bool> (moved.lock ()));
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
  g_test_add_func ("/painter/ref/floating-sink", floating);
  g_test_add_func ("/painter/ref/weak", weak_handles);
  g_test_add_func ("/painter/store/lifecycle", store_lifetime);
  g_test_add_func ("/painter/store/multiple-slots", multiple_slots);
  g_test_add_func ("/painter/store/reentrant-close", reentrant_close);
  g_test_add_func ("/painter/store/construction-failure", failed_construction);
  g_test_add_func ("/painter/store/thread-rejection", wrong_thread);
  g_test_add_func ("/painter/boundary/c-close", c_close_boundary);
  g_test_add_func ("/painter/boundary/exception", exception_conversion);
}
