/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "binding-store.hpp"
#include "binding-store-observation.hpp"
#include <thread>
using namespace GimpPainter;
using StoreObservation::Counts;
namespace {
struct Impl {
  explicit Impl (unsigned *destroyed = nullptr) : destroyed (destroyed) {}
  ~Impl () noexcept { if (destroyed) ++*destroyed; }
  void close () noexcept { closed = true; }
  std::uint64_t sum = 0;
  unsigned *destroyed;
  bool closed = false;
};
struct Slot : SlotSpec<GObject, Impl> {};
ObjectRef<GObject> owner (unsigned *destroyed = nullptr)
{
  auto result = ObjectRef<GObject>::adopt (G_OBJECT (g_object_new (G_TYPE_OBJECT, nullptr)));
  auto& store = BindingStore::ensure (result.get ());
  store.emplace<Slot> (destroyed);
  store.activate ();
  return result;
}
void expect (Counts start, std::size_t stores, std::size_t slots)
{
  const auto count = StoreObservation::since (start);
  g_assert_cmpuint (count.stores, ==, stores);
  g_assert_cmpuint (count.slots, ==, slots);
}
void scoped_regions ()
{
  for (std::size_t pixels : {1u, 257u, 1048576u})
    for (std::size_t regions : {1u, 7u}) {
      auto object = owner ();
      const auto start = StoreObservation::snapshot ();
      for (std::size_t region = 0; region < regions; ++region)
        BindingStore::require (object.get ()).with<Slot> ([&] (Impl& impl) {
          for (std::size_t pixel = region * pixels / regions;
               pixel < (region + 1) * pixels / regions; ++pixel)
            impl.sum += pixel + 1;
        });
      expect (start, regions, regions);
      const auto sum = BindingStore::require (object.get ()).read<Slot> (
        [] (const Impl& impl) { return impl.sum; });
      g_assert_cmpuint (sum, ==, pixels * (pixels + 1) / 2);
      g_test_message ("pixels=%zu regions=%zu stores=%zu slots=%zu", pixels, regions, regions, regions);
    }
}
void nested_operation ()
{
  auto outer = owner (), inner = owner ();
  const auto start = StoreObservation::snapshot ();
  BindingStore::require (outer.get ()).with<Slot> ([&] (Impl& impl) {
    impl.sum = 17;
    BindingStore::require (inner.get ()).with<Slot> ([&] (Impl& other) {
      for (unsigned i = 0; i < 4096; ++i) other.sum += impl.sum;
      g_assert_cmpuint (other.sum, ==, 17 * 4096);
    });
    g_assert_cmpuint (impl.sum, ==, 17);
  });
  expect (start, 2, 2);
}
void owner_lease ()
{
  unsigned destroyed = 0;
  auto object = owner (&destroyed);
  const auto start = StoreObservation::snapshot ();
  BindingStore::require (object.get ()).with<Slot> ([&] (Impl& impl) {
    object.reset ();
    g_assert_cmpuint (destroyed, ==, 0);
    for (unsigned i = 0; i < 4096; ++i) ++impl.sum;
    g_assert_cmpuint (impl.sum, ==, 4096);
  });
  expect (start, 1, 1);
  g_assert_cmpuint (destroyed, ==, 1);
}
void close_generation ()
{
  auto object = owner ();
  const auto start = StoreObservation::snapshot ();
  auto& store = BindingStore::require (object.get ());
  const auto generation = store.generation ();
  store.with<Slot> ([&] (Impl& impl) {
    for (unsigned i = 0; i < 4096; ++i) {
      if (i == 17) store.close (); // synchronous callback closes the owner
      if (!store.accepts (generation)) break;
      g_assert_false (impl.closed);
      ++impl.sum;
    }
    g_assert_cmpuint (impl.sum, ==, 17);
  });
  expect (start, 1, 1); // generation checks are not store/slot resolutions
  bool rejected = false;
  try { BindingStore::require (object.get ()).with<Slot> ([] (Impl&) {}); }
  catch (const Error& error) { rejected = error.code () == GIMP_PAINTER_ERROR_CLOSED; }
  g_assert_true (rejected);
  expect (start, 2, 1);
}
void counter_sensitivity ()
{
  auto object = owner ();
  auto start = StoreObservation::snapshot ();
  // Deliberately incorrect controls prove that both bad patterns are visible.
  for (unsigned i = 0; i < 64; ++i)
    BindingStore::require (object.get ()).with<Slot> ([] (Impl& impl) { ++impl.sum; });
  expect (start, 64, 64);
  start = StoreObservation::snapshot ();
  auto& cached_store = BindingStore::require (object.get ());
  for (unsigned i = 0; i < 64; ++i)
    cached_store.read<Slot> ([] (const Impl& impl) { g_assert_cmpuint (impl.sum, ==, 64); });
  expect (start, 1, 64);
}
void wrong_thread ()
{
  auto object = owner ();
  const auto start = StoreObservation::snapshot ();
  bool rejected = false;
  std::thread other ([&] {
    try { BindingStore::require (object.get ()).with<Slot> ([] (Impl&) {}); }
    catch (const Error& error) { rejected = error.code () == GIMP_PAINTER_ERROR_WRONG_THREAD; }
  });
  other.join ();
  g_assert_true (rejected);
  expect (start, 1, 0);
}
}
int main (int argc, char **argv)
{
  g_test_init (&argc, &argv, nullptr);
  g_test_add_func ("/qdata/scoped-regions", scoped_regions);
  g_test_add_func ("/qdata/nested-operation", nested_operation);
  g_test_add_func ("/qdata/owner-lease", owner_lease);
  g_test_add_func ("/qdata/close-generation", close_generation);
  g_test_add_func ("/qdata/counter-sensitivity", counter_sensitivity);
  g_test_add_func ("/qdata/wrong-thread", wrong_thread);
  return g_test_run ();
}
