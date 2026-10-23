/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "test-registry.hpp"
#include "test-fixture-traits.hpp"
#include "binding-store.hpp"
#include "resources.hpp"
#include "connection.hpp"
#include <cstring>
#include <initializer_list>

using namespace GimpPainter;
namespace GimpPainter {
template<> struct TypeTraits<PainterConstructionFixture>
{ static GType type () { return painter_construction_fixture_get_type (); } };
}
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

struct ConstructionStats
{
  int setters = 0, attempts = 0, rejected = 0, runs = 0, connected = 0;
  int closed = 0, destroyed = 0;
  int observed[2] = {-1, -1};
};
ConstructionStats construction_stats;
struct ConstructionImpl
{
  int values[2] = {-1, -1};
  Connection early_probe, normal;
  ~ConstructionImpl () { ++construction_stats.destroyed; }
  void close () noexcept
  {
    ++construction_stats.closed;
    early_probe.close ();
    normal.close ();
  }
};
struct ConstructionSlot : SlotSpec<PainterConstructionFixture, ConstructionImpl> {};
void construction_probe (GObject *owner, gpointer)
{
  ++construction_stats.attempts;
  GError *error = nullptr;
  boundary_void (&error, [&] {
    BindingStore::require (owner).with<ConstructionSlot> ([] (ConstructionImpl& impl) {
      ++construction_stats.runs;
      for (int i = 0; i != 2; ++i)
        {
          g_assert_cmpint (impl.values[i], >=, 0);
          construction_stats.observed[i] = impl.values[i];
        }
    });
  });
  if (error)
    {
      g_assert_error (error, GIMP_PAINTER_ERROR, GIMP_PAINTER_ERROR_INVALID_STATE);
      ++construction_stats.rejected;
      g_clear_error (&error);
    }
}

void construction_property_callbacks ()
{
  for (bool explicit_values : {false, true})
    {
      construction_stats = {};
      auto owner = ObjectRef<GObject>::adopt (G_OBJECT (explicit_values ?
        g_object_new (painter_construction_fixture_get_type (), "left", 17, "right", 23, nullptr) :
        g_object_new (painter_construction_fixture_get_type (), nullptr)));
      const int left = explicit_values ? 17 : 7, right = explicit_values ? 23 : 11;
      g_assert_cmpint (construction_stats.setters, ==, 2);
      g_assert_cmpint (construction_stats.attempts, ==, 3);
      g_assert_cmpint (construction_stats.rejected, ==, 2);
      g_assert_cmpint (construction_stats.connected, ==, 1);
      g_assert_cmpint (construction_stats.runs, ==, 1);
      g_assert_cmpint (construction_stats.observed[0], ==, left);
      g_assert_cmpint (construction_stats.observed[1], ==, right);
      gint actual_left = -1, actual_right = -1;
      g_object_get (owner.get (), "left", &actual_left, "right", &actual_right, nullptr);
      g_assert_cmpint (actual_left, ==, left);
      g_assert_cmpint (actual_right, ==, right);
      g_object_set (owner.get (), "left", 29, nullptr);
      g_assert_cmpint (construction_stats.attempts, ==, 4);
      g_assert_cmpint (construction_stats.runs, ==, 2);
      g_assert_cmpint (construction_stats.observed[0], ==, 29);
      g_assert_cmpint (construction_stats.observed[1], ==, right);
      g_object_run_dispose (owner.get ());
      g_object_run_dispose (owner.get ());
      g_signal_emit_by_name (owner.get (), "construction-probe");
      g_assert_cmpint (construction_stats.attempts, ==, 4);
      g_assert_cmpint (construction_stats.closed, ==, 1);
      g_assert_cmpint (construction_stats.destroyed, ==, 0);
      owner.reset ();
      g_assert_cmpint (construction_stats.destroyed, ==, 1);
    }
}

int property_destroyed = 0;
struct PropertyImpl
{
  explicit PropertyImpl (GObject *owner)
  {
    guint count = 0;
    GParamSpec **specs = g_object_class_list_properties (G_OBJECT_GET_CLASS (owner), &count);
    g_assert_cmpuint (count, ==, PAINTER_PROPERTY_COUNT - 1);
    for (guint i = 0; i < count; ++i)
      values[specs[i]->param_id] = Value::copy (g_param_spec_get_default_value (specs[i]));
    g_free (specs);
  }
  ~PropertyImpl () { ++property_destroyed; }
  void close () noexcept {}
  Value values[PAINTER_PROPERTY_COUNT];
};
struct PropertySlot : SlotSpec<PainterPropertyFixture, PropertyImpl> {};

GParamSpec *property_spec (GObject *owner, const char *name)
{
  auto *spec = g_object_class_find_property (G_OBJECT_GET_CLASS (owner), name);
  g_assert_nonnull (spec);
  return spec;
}
Value property_read (GObject *owner, const char *name)
{
  Value value (G_PARAM_SPEC_VALUE_TYPE (property_spec (owner, name)));
  g_object_get_property (owner, name, value.get ());
  return value;
}
struct Notifications { guint counts[PAINTER_PROPERTY_COUNT] = {}; };
void property_notified (GObject *owner, GParamSpec *spec, gpointer data)
{
  auto& counts = *static_cast<Notifications *> (data);
  ++counts.counts[spec->param_id];
  g_assert_true (spec == property_spec (owner, spec->name));
  auto actual = property_read (owner, spec->name);
  g_assert_cmpuint (G_VALUE_TYPE (actual.get ()), ==, G_PARAM_SPEC_VALUE_TYPE (spec));
}

void property_contract ()
{
  property_destroyed = 0;
  auto owner = ObjectRef<GObject>::adopt (G_OBJECT (g_object_new (painter_property_fixture_get_type (), nullptr)));
  const char *names[] = {nullptr, "count", "enabled", "ratio", "label", "peer", "bytes", "mode", "flags", "serial"};
  const GType types[] = {0, G_TYPE_INT, G_TYPE_BOOLEAN, G_TYPE_DOUBLE, G_TYPE_STRING,
    G_TYPE_OBJECT, G_TYPE_BYTES, painter_property_mode_get_type (), painter_property_flags_get_type (), G_TYPE_UINT64};
  for (guint id = 1; id < PAINTER_PROPERTY_COUNT; ++id)
    {
      auto *spec = property_spec (owner.get (), names[id]);
      g_assert_cmpuint (spec->param_id, ==, id);
      g_assert_cmpuint (spec->owner_type, ==, painter_property_fixture_get_type ());
      g_assert_cmpuint (G_PARAM_SPEC_VALUE_TYPE (spec), ==, types[id]);
      GParamFlags expected = static_cast<GParamFlags> (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
        (id == PAINTER_PROPERTY_INT ? G_PARAM_CONSTRUCT : 0) |
        (id == PAINTER_PROPERTY_BOOL ? G_PARAM_EXPLICIT_NOTIFY : 0));
      g_assert_cmpuint (spec->flags, ==, expected);
      auto value = property_read (owner.get (), names[id]);
      g_assert_cmpint (g_param_values_cmp (spec, value.get (), g_param_spec_get_default_value (spec)), ==, 0);
    }
  g_assert_cmpint (g_value_get_int (property_read (owner.get (), "count").get ()), ==, 7);
  g_assert_true (g_value_get_boolean (property_read (owner.get (), "enabled").get ()));
  g_assert_cmpfloat (g_value_get_double (property_read (owner.get (), "ratio").get ()), ==, 0.25);
  auto initial_label = property_read (owner.get (), "label");
  g_assert_cmpstr (g_value_get_string (initial_label.get ()), ==, "initial");
  g_assert_cmpint (G_PARAM_SPEC_INT (property_spec (owner.get (), "count"))->minimum, ==, -9);
  g_assert_cmpint (G_PARAM_SPEC_INT (property_spec (owner.get (), "count"))->maximum, ==, 99);
  g_assert_cmpfloat (G_PARAM_SPEC_DOUBLE (property_spec (owner.get (), "ratio"))->minimum, ==, -2);
  g_assert_cmpfloat (G_PARAM_SPEC_DOUBLE (property_spec (owner.get (), "ratio"))->maximum, ==, 2);
  g_assert_cmpuint (G_PARAM_SPEC_UINT64 (property_spec (owner.get (), "serial"))->maximum, ==, G_MAXUINT64);
  g_assert_null (g_value_get_object (property_read (owner.get (), "peer").get ()));
  g_assert_null (g_value_get_boxed (property_read (owner.get (), "bytes").get ()));
  g_assert_cmpint (g_value_get_enum (property_read (owner.get (), "mode").get ()), ==, 2);
  g_assert_cmpuint (g_value_get_flags (property_read (owner.get (), "flags").get ()), ==, 1);
  g_assert_cmpuint (g_value_get_uint64 (property_read (owner.get (), "serial").get ()), ==, G_GUINT64_CONSTANT (9007199254740993));
  Notifications notified;
  g_signal_connect (owner.get (), "notify", G_CALLBACK (property_notified), &notified);
  guint peer_finalized = 0;
  auto *peer = G_OBJECT (g_object_new (G_TYPE_OBJECT, nullptr));
  g_object_weak_ref (peer, [] (gpointer data, GObject *) { ++*static_cast<guint *> (data); }, &peer_finalized);
  GBytes *bytes = g_bytes_new_static ("a\0b", 3);
  char label[] = "changed";
  g_object_set (owner.get (), "count", -3, "enabled", FALSE, "ratio", 0.123456789012345,
    "label", label, "peer", peer, "bytes", bytes, "mode", 5, "flags", 5u,
    "serial", G_MAXUINT64 - 3, nullptr);
  label[0] = 'X';
  g_object_unref (peer); g_bytes_unref (bytes);
  for (guint id = 1; id < PAINTER_PROPERTY_COUNT; ++id) g_assert_cmpuint (notified.counts[id], ==, 1);
  g_assert_cmpint (g_value_get_int (property_read (owner.get (), "count").get ()), ==, -3);
  g_assert_false (g_value_get_boolean (property_read (owner.get (), "enabled").get ()));
  g_assert_cmpfloat (g_value_get_double (property_read (owner.get (), "ratio").get ()), ==, 0.123456789012345);
  auto changed_label = property_read (owner.get (), "label");
  g_assert_cmpstr (g_value_get_string (changed_label.get ()), ==, "changed");
  g_assert_cmpint (g_value_get_enum (property_read (owner.get (), "mode").get ()), ==, 5);
  g_assert_cmpuint (g_value_get_flags (property_read (owner.get (), "flags").get ()), ==, 5);
  g_assert_cmpuint (g_value_get_uint64 (property_read (owner.get (), "serial").get ()), ==, G_MAXUINT64 - 3);
  auto owned_peer = property_read (owner.get (), "peer");
  auto owned_bytes = property_read (owner.get (), "bytes");
  g_assert_true (g_value_get_object (owned_peer.get ()) == peer);
  gsize size = 0;
  const void *data = g_bytes_get_data (static_cast<GBytes *> (g_value_get_boxed (owned_bytes.get ())), &size);
  g_assert_cmpuint (size, ==, 3); g_assert_cmpmem (data, size, "a\0b", 3);
  // GObject automatic notify reports every accepted set; explicit notify reports changes.
  g_object_set (owner.get (), "count", -3, "enabled", FALSE, nullptr);
  g_assert_cmpuint (notified.counts[PAINTER_PROPERTY_INT], ==, 2);
  g_assert_cmpuint (notified.counts[PAINTER_PROPERTY_BOOL], ==, 1);
  g_object_freeze_notify (owner.get ());
  g_object_set (owner.get (), "count", 10, "enabled", TRUE, nullptr);
  g_object_set (owner.get (), "count", 11, "enabled", FALSE, nullptr);
  g_assert_cmpuint (notified.counts[PAINTER_PROPERTY_INT], ==, 2);
  g_assert_cmpuint (notified.counts[PAINTER_PROPERTY_BOOL], ==, 1);
  g_object_thaw_notify (owner.get ());
  g_assert_cmpuint (notified.counts[PAINTER_PROPERTY_INT], ==, 3);
  g_assert_cmpuint (notified.counts[PAINTER_PROPERTY_BOOL], ==, 2);
  g_assert_cmpint (g_value_get_int (property_read (owner.get (), "count").get ()), ==, 11);
  // Native GValue conversion remains native; no hand-written coercion table.
  Value integer (G_TYPE_INT); g_value_set_int (integer.get (), 1);
  g_object_set_property (owner.get (), "ratio", integer.get ());
  g_assert_cmpfloat (g_value_get_double (property_read (owner.get (), "ratio").get ()), ==, 1.0);
  Value converted (G_TYPE_DOUBLE);
  g_object_get_property (owner.get (), "count", converted.get ());
  g_assert_cmpuint (G_VALUE_TYPE (converted.get ()), ==, G_TYPE_DOUBLE);
  g_assert_cmpfloat (g_value_get_double (converted.get ()), ==, 11.0);
  // NULL and empty strings remain distinct, and repeated getter storage is safe.
  Value text (G_TYPE_STRING); g_value_set_string (text.get (), "old output");
  g_object_set (owner.get (), "label", "", nullptr);
  g_object_get_property (owner.get (), "label", text.get ());
  g_assert_cmpstr (g_value_get_string (text.get ()), ==, "");
  g_object_set (owner.get (), "label", nullptr, "peer", nullptr, "bytes", nullptr, nullptr);
  g_object_get_property (owner.get (), "label", text.get ());
  g_assert_null (g_value_get_string (text.get ()));
  g_assert_cmpuint (peer_finalized, ==, 0);
  owned_peer.reset (); g_assert_cmpuint (peer_finalized, ==, 1);
  data = g_bytes_get_data (static_cast<GBytes *> (g_value_get_boxed (owned_bytes.get ())), &size);
  g_assert_cmpmem (data, size, "a\0b", 3);
  owner.reset (); g_assert_cmpint (property_destroyed, ==, 1);
  auto constructed = ObjectRef<GObject>::adopt (G_OBJECT (g_object_new (painter_property_fixture_get_type (), "count", 19, nullptr)));
  g_assert_cmpint (g_value_get_int (property_read (constructed.get (), "count").get ()), ==, 19);
}

void property_notify_reentry ()
{
  property_destroyed = 0;
  auto owner = ObjectRef<GObject>::adopt (G_OBJECT (g_object_new (painter_property_fixture_get_type (), nullptr)));
  guint calls = 0;
  g_signal_connect (owner.get (), "notify::count", G_CALLBACK (+[] (GObject *object, GParamSpec *, gpointer data) {
    auto& calls = *static_cast<guint *> (data);
    ++calls;
    const gint value = g_value_get_int (property_read (object, "count").get ());
    g_assert_cmpint (value, ==, calls == 1 ? 20 : 21);
    if (calls == 1) g_object_set (object, "count", 21, nullptr);
  }), &calls);
  g_object_set (owner.get (), "count", 20, nullptr);
  g_assert_cmpuint (calls, ==, 2);
  g_signal_handlers_disconnect_by_data (owner.get (), &calls);
  g_signal_connect (owner.get (), "notify::enabled", G_CALLBACK (+[] (GObject *object, GParamSpec *, gpointer data) {
    g_assert_false (g_value_get_boolean (property_read (object, "enabled").get ()));
    g_object_run_dispose (object);
    static_cast<ObjectRef<GObject> *> (data)->reset ();
    g_assert_cmpint (property_destroyed, ==, 0);
  }), &owner);
  g_object_set (owner.get (), "enabled", FALSE, nullptr);
  g_assert_false (static_cast<bool> (owner));
  g_assert_cmpint (property_destroyed, ==, 1);
}

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

void painter_construction_binding_init (GObject *owner)
{
  GError *error = nullptr;
  boundary_void (&error, [&] {
    auto& store = BindingStore::ensure (owner);
    store.emplace<ConstructionSlot> ();
    // Deliberately premature test observer proves the callback gate rejects
    // real emissions. Normal owner callbacks are connected only after activate.
    store.initialize<ConstructionSlot> ([&] (ConstructionImpl& impl) {
      impl.early_probe = Connection::connect (ObjectRef<GObject>::retain (owner),
        "construction-probe", G_CALLBACK (construction_probe), nullptr, nullptr);
    });
  });
  g_assert_no_error (error);
}
void painter_construction_binding_constructed (GObject *owner)
{
  GError *error = nullptr;
  boundary_void (&error, [&] {
    auto& store = BindingStore::require (owner);
    store.initialize<ConstructionSlot> ([] (ConstructionImpl& impl) {
      g_assert_cmpint (construction_stats.setters, ==, 2);
      g_assert_cmpint (construction_stats.rejected, ==, 2);
      g_assert_cmpint (construction_stats.runs, ==, 0);
      g_assert_false (impl.normal.connected ());
      g_assert_cmpint (impl.values[0], >=, 0);
      g_assert_cmpint (impl.values[1], >=, 0);
      impl.early_probe.close ();
    });
    store.activate ();
    store.with<ConstructionSlot> ([&] (ConstructionImpl& impl) {
      impl.normal = Connection::connect (ObjectRef<GObject>::retain (owner),
        "construction-probe", G_CALLBACK (construction_probe), nullptr, nullptr);
      ++construction_stats.connected;
    });
    g_signal_emit_by_name (owner, "construction-probe");
  });
  g_assert_no_error (error);
}
void painter_construction_binding_set (GObject *owner, guint id, gint value)
{
  GError *error = nullptr;
  boundary_void (&error, [&] {
    auto& store = BindingStore::require (owner);
    const bool constructing = store.state () == BindingStore::State::constructing;
    auto set = [=] (ConstructionImpl& impl) {
      if (constructing) g_assert_false (impl.normal.connected ());
      impl.values[id - 1] = value;
      ++construction_stats.setters;
    };
    if (constructing) store.initialize<ConstructionSlot> (set);
    else store.with<ConstructionSlot> (set);
    g_signal_emit_by_name (owner, "construction-probe");
  });
  g_assert_no_error (error);
}
gint painter_construction_binding_get (GObject *owner, guint id)
{
  GError *error = nullptr;
  gint value = boundary<gint> (&error, -1, [&] {
    return BindingStore::require (owner).read<ConstructionSlot> (
      [id] (const ConstructionImpl& impl) { return impl.values[id - 1]; });
  });
  g_assert_no_error (error);
  return value;
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
void painter_property_binding_init (GObject *owner)
{
  GError *error = nullptr;
  boundary_void (&error, [&] { BindingStore::ensure (owner).emplace<PropertySlot> (owner); });
  g_assert_no_error (error);
}
void painter_property_binding_constructed (GObject *owner)
{
  GError *error = nullptr;
  boundary_void (&error, [&] { BindingStore::require (owner).activate (); });
  g_assert_no_error (error);
}
void painter_property_binding_set (GObject *owner, guint id, const GValue *value, GParamSpec *spec)
{
  property_boundary (owner, spec, "set", [&] {
    auto& store = BindingStore::require (owner);
    auto set = [&] (PropertyImpl& impl) {
      g_assert_cmpuint (G_VALUE_TYPE (value), ==, G_VALUE_TYPE (impl.values[id].get ()));
      const bool changed = g_param_values_cmp (spec, impl.values[id].get (), value) != 0;
      impl.values[id] = Value::copy (value);
      if (changed && (spec->flags & G_PARAM_EXPLICIT_NOTIFY)) g_object_notify_by_pspec (owner, spec);
    };
    if (store.state () == BindingStore::State::constructing) store.initialize<PropertySlot> (set);
    else store.with<PropertySlot> (set);
  });
}
void painter_property_binding_get (GObject *owner, guint id, GValue *value, GParamSpec *spec)
{
  property_boundary (owner, spec, "get", [&] {
    BindingStore::require (owner).read<PropertySlot> ([&] (const PropertyImpl& impl) {
      g_assert_cmpuint (G_VALUE_TYPE (value), ==, G_VALUE_TYPE (impl.values[id].get ()));
      g_value_copy (impl.values[id].get (), value);
    });
  });
}

void painter_test_register_gobject ()
{
  g_test_add_func ("/painter/gobject/property-dispose-finalize", lifecycle);
  g_test_add_func ("/painter/gobject/construction-property-callbacks", construction_property_callbacks);
  g_test_add_func ("/painter/gobject/type-rejection", wrong_type);
  g_test_add_func ("/painter/gobject/property-types-defaults-notify", property_contract);
  g_test_add_func ("/painter/gobject/property-notify-reentry", property_notify_reentry);
}
