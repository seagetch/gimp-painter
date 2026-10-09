/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "app/painter/binding-store.hpp"
#include "app/painter/gimp-painter-binding.h"
#include "app/painter/source.hpp"
#include <thread>

using namespace GimpPainter;
namespace {
struct Counts { int close = 0, destroy = 0; GThread *final_thread = nullptr; };
struct Impl {
  explicit Impl (Counts& counts) : counts (counts) {}
  ~Impl () { ++counts.destroy; counts.final_thread = g_thread_self (); }
  void close () noexcept { ++counts.close; }
  Counts& counts;
  int value = 17;
};
struct Slot : SlotSpec<GObject, Impl> {};
template<class F> void wrong_thread (F f) {
  bool rejected = false;
  try { f (); }
  catch (const Error& e) { rejected = e.code () == GIMP_PAINTER_ERROR_WRONG_THREAD; }
  g_assert_true (rejected);
}
void thread_is_not_context () {
  Counts counts;
  GThread *owner_thread = g_thread_self ();
  auto owner = ObjectRef<GObject>::adopt (G_OBJECT (g_object_new (G_TYPE_OBJECT, nullptr)));
  auto& store = BindingStore::ensure (owner.get ());
  store.emplace<Slot> (counts); store.activate ();
  auto *context = g_main_context_new ();
  g_assert_false (g_main_context_is_owner (context));
  // Creator-thread access does not require owning or iterating this context.
  store.with<Slot> ([] (Impl& value) { value.value = 23; });
  bool called = false;
  auto source = Source::idle (context, G_PRIORITY_DEFAULT, [&] {
    called = true;
    g_assert_true (g_main_context_is_owner (context));
    g_assert_true (g_thread_self () != owner_thread);
    wrong_thread ([&] { store.read<Slot> ([] (const Impl& value) { return value.value; }); });
    wrong_thread ([&] { store.with<Slot> ([] (Impl& value) { value.value = 99; }); });
    GError *error = nullptr;
    g_assert_false (gimp_painter_binding_close (owner.get (), &error));
    g_assert_error (error, GIMP_PAINTER_ERROR, GIMP_PAINTER_ERROR_WRONG_THREAD);
    g_clear_error (&error);
    return false;
  });
  // Borrowed references remain valid because the owner thread retains every
  // resource and waits only in this test. No UI/GObject-owning capture moves.
  std::thread worker ([&] { g_assert_true (g_main_context_iteration (context, FALSE)); });
  worker.join ();
  g_assert_true (called);
  g_assert_cmpint (store.read<Slot> ([] (const Impl& value) { return value.value; }), ==, 23);
  g_assert_cmpint (counts.close, ==, 0);
  source.close ();
  store.close ();
  g_assert_cmpint (counts.close, ==, 1);
  owner.reset ();
  g_assert_cmpint (counts.destroy, ==, 1);
  g_assert_true (counts.final_thread == owner_thread);
  g_main_context_unref (context);
}
}
int main (int argc, char **argv) {
  g_test_init (&argc, &argv, nullptr);
  g_test_add_func ("/painter/context/thread-versus-context", thread_is_not_context);
  return g_test_run ();
}
