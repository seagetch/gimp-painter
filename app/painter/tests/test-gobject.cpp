/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "test-registry.hpp"
#include "test-fixture.h"
#include "binding-store.hpp"
#include <initializer_list>

namespace GimpPainter {
template<> struct TypeTraits<PainterFixture>
{ static GType type () noexcept { return painter_fixture_get_type (); } };
}
using namespace GimpPainter;
namespace {
enum class Event { construct, construct_property, activate, get, set, close, destroy };
struct Stats
{
  int constructed = 0;
  int closed = 0;
  int destroyed = 0;
  Event events[16] = {};
  guint event_count = 0;

  void record (Event event) noexcept
  {
    g_assert_cmpuint (event_count, <, G_N_ELEMENTS (events));
    events[event_count++] = event;
  }
};
Stats stats;
struct FixtureImpl
{
  FixtureImpl () { ++stats.constructed; stats.record (Event::construct); }
  ~FixtureImpl () { ++stats.destroyed; stats.record (Event::destroy); }
  void close () noexcept { ++stats.closed; stats.record (Event::close); }
  int value = -1;
};
struct FixtureSlot : SlotSpec<PainterFixture, FixtureImpl> {};

void expect_events (std::initializer_list<Event> expected)
{
  g_assert_cmpuint (stats.event_count, ==, expected.size ());
  guint index = 0;
  for (Event event : expected)
    g_assert_cmpint (static_cast<int> (stats.events[index++]), ==,
                     static_cast<int> (event));
}

void lifecycle ()
{
  stats = {};
  auto owner = ObjectRef<PainterFixture>::adopt (
    static_cast<PainterFixture *> (g_object_new (painter_fixture_get_type (), "value", 17, nullptr)));
  g_assert_cmpint (stats.constructed, ==, 1);
  expect_events ({Event::construct, Event::construct_property, Event::activate});
  gint value = 0;
  g_object_get (owner.get (), "value", &value, nullptr);
  g_assert_cmpint (value, ==, 17);
  g_object_set (owner.get (), "value", 25, nullptr);
  g_object_get (owner.get (), "value", &value, nullptr);
  g_assert_cmpint (value, ==, 25);
  expect_events ({Event::construct, Event::construct_property, Event::activate,
                  Event::get, Event::set, Event::get});
  g_object_run_dispose (G_OBJECT (owner.get ()));
  g_object_run_dispose (G_OBJECT (owner.get ()));
  g_assert_cmpint (stats.closed, ==, 1);
  g_assert_cmpint (stats.destroyed, ==, 0);
  expect_events ({Event::construct, Event::construct_property, Event::activate,
                  Event::get, Event::set, Event::get, Event::close});
  g_object_get (owner.get (), "value", &value, nullptr);
  g_assert_cmpint (value, ==, 25);
  owner.reset ();
  g_assert_cmpint (stats.destroyed, ==, 1);
  expect_events ({Event::construct, Event::construct_property, Event::activate,
                  Event::get, Event::set, Event::get, Event::close,
                  Event::get, Event::destroy});
  const char *names[] = {"construct", "construct-property", "activate",
                         "get", "set", "close", "destroy"};
  for (guint i = 0; i < stats.event_count; ++i)
    g_test_message ("fixture event %u: %s", i + 1,
                    names[static_cast<int> (stats.events[i])]);
}

void wrong_type ()
{
  using Ref = ObjectRef<PainterFixture>;
  for (auto factory : {&Ref::retain, &Ref::adopt, &Ref::sink})
    {
      auto empty = factory (nullptr);
      g_assert_null (empty.get ());
      for (GType type : {G_TYPE_OBJECT, G_TYPE_INITIALLY_UNOWNED})
        {
          int finalized = 0;
          auto *object = G_OBJECT (g_object_new (type, nullptr));
          const bool floating = g_object_is_floating (object);
          g_object_weak_ref (object, [] (gpointer count, GObject *) {
            ++*static_cast<int *> (count);
          }, &finalized);
          bool rejected = false;
          try { auto bad = factory (reinterpret_cast<PainterFixture *> (object)); }
          catch (const Error& error)
            { rejected = error.code () == GIMP_PAINTER_ERROR_WRONG_TYPE; }
          g_assert_true (rejected);
          g_assert_cmpuint (object->ref_count, ==, 1);
          g_assert_cmpint (g_object_is_floating (object), ==, floating);
          g_assert_cmpint (finalized, ==, 0);

          GError *error = nullptr;
          const bool accepted = boundary<bool> (&error, false, [&] {
            auto bad = factory (reinterpret_cast<PainterFixture *> (object));
            return true;
          });
          g_assert_false (accepted);
          g_assert_error (error, GIMP_PAINTER_ERROR, GIMP_PAINTER_ERROR_WRONG_TYPE);
          g_clear_error (&error);
          g_assert_cmpuint (object->ref_count, ==, 1);
          g_assert_cmpint (g_object_is_floating (object), ==, floating);
          if (floating) g_object_ref_sink (object);
          g_object_unref (object);
          g_assert_cmpint (finalized, ==, 1);
        }
    }
  auto *object = G_OBJECT (g_object_new (G_TYPE_OBJECT, nullptr));
  auto& store = BindingStore::ensure (object);
  bool rejected = false;
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
  boundary_void (&error, [&] {
    BindingStore::require (owner).activate ();
    stats.record (Event::activate);
  });
  g_assert_no_error (error);
}
void painter_fixture_binding_set (GObject *owner, gint value)
{
  GError *error = nullptr;
  boundary_void (&error, [&] {
    auto& store = BindingStore::require (owner);
    auto setter = [value, &store] (FixtureImpl& impl) {
      stats.record (store.state () == BindingStore::State::constructing ?
                    Event::construct_property : Event::set);
      impl.value = value;
    };
    if (store.state () == BindingStore::State::constructing) store.initialize<FixtureSlot> (setter);
    else store.with<FixtureSlot> (setter);
  });
  g_assert_no_error (error);
}
gint painter_fixture_binding_get (GObject *owner)
{
  GError *error = nullptr;
  gint value = boundary<gint> (&error, 0, [&] {
    return BindingStore::require (owner).read<FixtureSlot> ([] (const FixtureImpl& impl) {
      stats.record (Event::get);
      return impl.value;
    });
  });
  g_assert_no_error (error);
  return value;
}
void painter_test_register_gobject ()
{
  g_test_add_func ("/painter/gobject/property-dispose-finalize", lifecycle);
  g_test_add_func ("/painter/gobject/type-rejection", wrong_type);
}
