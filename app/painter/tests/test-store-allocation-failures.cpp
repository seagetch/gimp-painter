/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Standalone Linux/GNU-linker test. Only this executable wraps C++ allocation;
 * GLib's fatal-OOM allocators are never faulted. Tracking itself cannot allocate. */
#include "binding-store.hpp"
#include "resources.hpp"
#include <cstdlib>
#include <new>

namespace Fault {
struct Pointer { void *value = nullptr; unsigned freed = 0; };
struct State
{
  bool tracking = false;
  unsigned remaining = 0, injected = 0, attempts = 0;
  unsigned allocations = 0, strings = 0;
  Pointer allocated[64] {}, string[16] {};
} state;

void start () { state = {}; state.tracking = true; }
void arm (unsigned position)
{ state.remaining = position; state.injected = state.attempts = 0; }
void remember (Pointer *items, unsigned& count, unsigned limit, void *value)
{
  if (count == limit) std::abort ();
  items[count++] = {value, 0};
}
void released (Pointer *items, unsigned count, void *value) noexcept
{
  if (!state.tracking || !value) return;
  // Search backwards so allocator address reuse is recorded independently.
  for (unsigned i = count; i != 0; --i)
    if (items[i - 1].value == value && !items[i - 1].freed)
      { ++items[i - 1].freed; return; }
}
gchar *string ()
{
  auto *value = g_strdup ("owned constructor input");
  remember (state.string, state.strings, 16, value);
  return value;
}
void finish ()
{
  state.tracking = false;
  for (unsigned i = 0; i != state.allocations; ++i)
    g_assert_cmpuint (state.allocated[i].freed, ==, 1);
  for (unsigned i = 0; i != state.strings; ++i)
    g_assert_cmpuint (state.string[i].freed, ==, 1);
  g_test_message ("tracked C++ allocations/releases=%u; fixture strings/g_free=%u",
                  state.allocations, state.strings);
}
}

extern "C" void *__real__Znwm (std::size_t);
extern "C" void __real__ZdlPv (void *) noexcept;
extern "C" void __real__ZdlPvm (void *, std::size_t) noexcept;
extern "C" void __real_g_free (gpointer);
extern "C" void *__wrap__Znwm (std::size_t size)
{
  if (Fault::state.tracking)
    {
      ++Fault::state.attempts;
      if (Fault::state.remaining && !--Fault::state.remaining)
        { ++Fault::state.injected; throw std::bad_alloc (); }
    }
  auto *value = __real__Znwm (size);
  if (Fault::state.tracking)
    Fault::remember (Fault::state.allocated, Fault::state.allocations, 64, value);
  return value;
}
extern "C" void __wrap__ZdlPv (void *value) noexcept
{
  Fault::released (Fault::state.allocated, Fault::state.allocations, value);
  __real__ZdlPv (value);
}
extern "C" void __wrap__ZdlPvm (void *value, std::size_t size) noexcept
{
  Fault::released (Fault::state.allocated, Fault::state.allocations, value);
  __real__ZdlPvm (value, size);
}
extern "C" __attribute__((visibility("default"))) void __wrap_g_free (gpointer value)
{
  Fault::released (Fault::state.string, Fault::state.strings, value);
  __real_g_free (value);
}

namespace {
using namespace GimpPainter;
struct Counts { unsigned constructed = 0, closed = 0, destroyed = 0; };
struct Impl
{
  Impl (Counts& counts, bool fail = false) : counts (counts), text (Fault::string ())
  {
    if (fail) throw std::bad_alloc ();
    ++counts.constructed;
  }
  ~Impl () { ++counts.destroyed; }
  void close () noexcept { ++counts.closed; }
  Counts& counts;
  String text;
  int value = 37;
};
struct ExistingSlot : SlotSpec<GObject, Impl> {};
struct CandidateSlot : SlotSpec<GObject, Impl> {};
auto object () -> ObjectRef<GObject>
{ return ObjectRef<GObject>::adopt (G_OBJECT (g_object_new (G_TYPE_OBJECT, nullptr))); }
void finalized (gpointer data, GObject *) { ++*static_cast<unsigned *> (data); }

void store_allocation ()
{
  auto owner = object ();
  unsigned finalizations = 0;
  g_object_weak_ref (owner.get (), finalized, &finalizations);
  Fault::start ();
  Fault::arm (1);
  bool failed = false;
  try { BindingStore::ensure (owner.get ()); }
  catch (const std::bad_alloc&) { failed = true; }
  g_assert_true (failed);
  g_assert_cmpuint (Fault::state.injected, ==, 1);
  g_assert_cmpuint (Fault::state.attempts, ==, 1);
  g_assert_cmpuint (Fault::state.allocations, ==, 0);
  g_assert_null (BindingStore::find (owner.get ()));
  g_assert_cmpuint (owner.get ()->ref_count, ==, 1);
  auto& store = BindingStore::ensure (owner.get ());
  g_assert_true (BindingStore::find (owner.get ()) == &store);
  g_assert_true (store.state () == BindingStore::State::constructing);
  g_assert_cmpuint (Fault::state.allocations, ==, 1);
  owner.reset ();
  g_assert_cmpuint (finalizations, ==, 1);
  Fault::finish ();
}

void candidate_failure (unsigned position, bool constructor_failure = false)
{
  Counts existing, candidate;
  auto owner = object ();
  unsigned finalizations = 0;
  g_object_weak_ref (owner.get (), finalized, &finalizations);
  Fault::start ();
  auto& store = BindingStore::ensure (owner.get ());
  store.emplace<ExistingSlot> (existing);
  store.initialize<ExistingSlot> ([] (Impl& impl) { impl.value = 91; });
  const auto generation = store.generation ();
  const auto before = Fault::state.allocations;
  const auto strings_before = Fault::state.strings;
  Fault::arm (position);
  bool failed = false;
  try { store.emplace<CandidateSlot> (candidate, constructor_failure); }
  catch (const std::bad_alloc&) { failed = true; }
  g_assert_true (failed);
  g_assert_cmpuint (Fault::state.remaining, ==, 0);
  g_assert_cmpuint (Fault::state.injected, ==, constructor_failure ? 0 : 1);
  g_assert_cmpuint (Fault::state.attempts, ==, constructor_failure ? 2 : position);
  const unsigned completed = !constructor_failure && position == 3 ? 1 : 0;
  g_assert_cmpuint (candidate.constructed, ==, completed);
  g_assert_cmpuint (candidate.closed, ==, completed);
  g_assert_cmpuint (candidate.destroyed, ==, completed);
  g_assert_cmpuint (Fault::state.allocations - before, ==,
                    constructor_failure ? 2 : position - 1);
  for (unsigned i = 0; i != Fault::state.allocations; ++i)
    g_assert_cmpuint (Fault::state.allocated[i].freed, ==, i < before ? 0 : 1);
  g_assert_cmpuint (Fault::state.strings - strings_before, ==,
                    constructor_failure || completed ? 1 : 0);
  for (unsigned i = 0; i != Fault::state.strings; ++i)
    g_assert_cmpuint (Fault::state.string[i].freed, ==, i < strings_before ? 0 : 1);
  g_assert_cmpuint (existing.constructed, ==, 1);
  g_assert_cmpuint (existing.closed, ==, 0);
  g_assert_cmpuint (existing.destroyed, ==, 0);
  g_assert_true (store.state () == BindingStore::State::constructing);
  g_assert_cmpuint (store.generation (), ==, generation);
  g_assert_cmpuint (owner.get ()->ref_count, ==, 1);
  g_assert_cmpuint (finalizations, ==, 0);
  g_assert_cmpint (store.read<ExistingSlot> ([] (const Impl& i) { return i.value; }), ==, 91);
  bool missing = false;
  try { store.read<CandidateSlot> ([] (const Impl&) {}); }
  catch (const Error& error) { missing = error.code () == GIMP_PAINTER_ERROR_MISSING_SLOT; }
  g_assert_true (missing);

  // Retrying the same identity demonstrates that the pending reservation cleared.
  store.emplace<CandidateSlot> (candidate);
  store.activate ();
  g_assert_cmpint (store.with<CandidateSlot> ([] (Impl& i) { return i.value; }), ==, 37);
  g_assert_cmpint (store.with<ExistingSlot> ([] (Impl& i) { return i.value; }), ==, 91);
  owner.reset ();
  g_assert_cmpuint (finalizations, ==, 1);
  g_assert_cmpuint (existing.closed, ==, 1);
  g_assert_cmpuint (existing.destroyed, ==, 1);
  g_assert_cmpuint (candidate.constructed, ==, completed + 1);
  g_assert_cmpuint (candidate.closed, ==, completed + 1);
  g_assert_cmpuint (candidate.destroyed, ==, completed + 1);
  Fault::finish ();
}
void entry_allocation () { candidate_failure (1); }
void impl_allocation () { candidate_failure (2); }
void publication_allocation () { candidate_failure (3); }
void string_constructor_unwind () { candidate_failure (0, true); }
}

int main (int argc, char **argv)
{
  g_test_init (&argc, &argv, nullptr);
  g_test_add_func ("/painter/store-allocation/store", store_allocation);
  g_test_add_func ("/painter/store-allocation/entry", entry_allocation);
  g_test_add_func ("/painter/store-allocation/impl", impl_allocation);
  g_test_add_func ("/painter/store-allocation/publication", publication_allocation);
  g_test_add_func ("/painter/store-allocation/string-constructor-unwind", string_constructor_unwind);
  return g_test_run ();
}
