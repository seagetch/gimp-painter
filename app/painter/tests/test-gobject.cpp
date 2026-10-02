/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "test-registry.hpp"
#include "test-fixture.h"
#include "binding-store.hpp"

namespace GimpPainter {
template<> struct TypeTraits<PainterFixture>
{ static GType type () noexcept { return painter_fixture_get_type (); } };
}
using namespace GimpPainter;
namespace {
struct Stats { int constructed = 0; int closed = 0; int destroyed = 0; };
Stats stats;
struct FixtureImpl
{
  FixtureImpl () { ++stats.constructed; }
  ~FixtureImpl () { ++stats.destroyed; }
  void close () noexcept { ++stats.closed; }
  int value = -1;
};
struct FixtureSlot : SlotSpec<PainterFixture, FixtureImpl> {};

void lifecycle ()
{
  stats = {};
  auto owner = ObjectRef<PainterFixture>::adopt (
    static_cast<PainterFixture *> (g_object_new (painter_fixture_get_type (), "value", 17, nullptr)));
  g_assert_cmpint (stats.constructed, ==, 1);
  gint value = 0;
  g_object_get (owner.get (), "value", &value, nullptr);
  g_assert_cmpint (value, ==, 17);
  g_object_set (owner.get (), "value", 25, nullptr);
  g_object_get (owner.get (), "value", &value, nullptr);
  g_assert_cmpint (value, ==, 25);
  g_object_run_dispose (G_OBJECT (owner.get ()));
  g_object_run_dispose (G_OBJECT (owner.get ()));
  g_assert_cmpint (stats.closed, ==, 1);
  g_assert_cmpint (stats.destroyed, ==, 0);
  g_object_get (owner.get (), "value", &value, nullptr);
  g_assert_cmpint (value, ==, 25);
  owner.reset ();
  g_assert_cmpint (stats.destroyed, ==, 1);
}

void wrong_type ()
{
  auto *object = G_OBJECT (g_object_new (G_TYPE_OBJECT, nullptr));
  bool rejected = false;
  try { auto bad = ObjectRef<PainterFixture>::adopt (reinterpret_cast<PainterFixture *> (object)); }
  catch (const Error& error) { rejected = error.code () == GIMP_PAINTER_ERROR_WRONG_TYPE; }
  g_assert_true (rejected);
  // adopt failure left the original reference untouched
  g_assert_true (G_IS_OBJECT (object));
  auto& store = BindingStore::ensure (object);
  rejected = false;
  try { store.emplace<FixtureSlot> (); }
  catch (const Error& error) { rejected = error.code () == GIMP_PAINTER_ERROR_WRONG_TYPE; }
  g_assert_true (rejected);
  g_object_unref (object);
}
}

void painter_fixture_binding_init (GObject *owner)
{
  GError *error = nullptr;
  boundary_void (&error, [&] { BindingStore::ensure (owner).emplace<FixtureSlot> (); });
  g_assert_no_error (error);
}
void painter_fixture_binding_constructed (GObject *owner)
{
  GError *error = nullptr;
  boundary_void (&error, [&] { BindingStore::require (owner).activate (); });
  g_assert_no_error (error);
}
void painter_fixture_binding_set (GObject *owner, gint value)
{
  GError *error = nullptr;
  boundary_void (&error, [&] {
    auto& store = BindingStore::require (owner);
    auto setter = [value] (FixtureImpl& impl) { impl.value = value; };
    if (store.state () == BindingStore::State::constructing) store.initialize<FixtureSlot> (setter);
    else store.with<FixtureSlot> (setter);
  });
  g_assert_no_error (error);
}
gint painter_fixture_binding_get (GObject *owner)
{
  GError *error = nullptr;
  gint value = boundary<gint> (&error, 0, [&] {
    return BindingStore::require (owner).read<FixtureSlot> ([] (const FixtureImpl& impl) { return impl.value; });
  });
  g_assert_no_error (error);
  return value;
}
void painter_test_register_gobject ()
{
  g_test_add_func ("/painter/gobject/property-dispose-finalize", lifecycle);
  g_test_add_func ("/painter/gobject/type-rejection", wrong_type);
}
