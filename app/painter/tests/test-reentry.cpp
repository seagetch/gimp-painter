/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "test-registry.hpp"
#include "binding-store.hpp"
#include "connection.hpp"
#include "resources.hpp"
#include "source.hpp"
#include <functional>

using namespace GimpPainter;
namespace {
auto object () -> ObjectRef<GObject>
{ return ObjectRef<GObject>::adopt (G_OBJECT (g_object_new (G_TYPE_OBJECT, nullptr))); }
void count (gpointer data, GObject *) { ++*static_cast<int *> (data); }
struct ReplaceRef { ObjectRef<GObject> *target; GObject *replacement; };
void replace_ref (gpointer data, GObject *)
{
  auto& state = *static_cast<ReplaceRef *> (data);
  *state.target = ObjectRef<GObject>::retain (state.replacement);
}
void ref_reentry ()
{
  int incoming_destroyed = 0;
  auto value = object ();
  auto replacement = object ();
  auto incoming = object ();
  g_object_weak_ref (incoming.get (), count, &incoming_destroyed);
  ReplaceRef state { &value, replacement.get () };
  g_object_weak_ref (value.get (), replace_ref, &state);
  value = std::move (incoming);
  g_assert_true (value.get () == replacement.get ());
  g_assert_cmpint (incoming_destroyed, ==, 1);
}
void replace_value (gpointer data, GObject *)
{
  auto *target = static_cast<Value *> (data);
  Value next (G_TYPE_STRING);
  g_value_set_string (next.get (), "replacement");
  *target = std::move (next);
}
void value_reentry ()
{
  for (int move = 0; move != 2; ++move)
    {
      Value value (G_TYPE_OBJECT);
      auto old = object ();
      g_object_weak_ref (old.get (), replace_value, &value);
      g_value_take_object (value.get (), old.release ());
      if (move)
        {
          Value incoming (G_TYPE_STRING);
          g_value_set_string (incoming.get (), "incoming");
          value = std::move (incoming);
        }
      else value.reset ();
      g_assert_true (G_VALUE_HOLDS_STRING (value.get ()));
      g_assert_cmpstr (g_value_get_string (value.get ()), ==, "replacement");
    }
}
void replace_array (gpointer data, GObject *)
{
  auto *target = static_cast<ArrayRef *> (data);
  auto array = ArrayRef::adopt (g_array_new (FALSE, FALSE, sizeof (int)));
  int marker = 42;
  g_array_append_val (array.get (), marker);
  *target = std::move (array);
}
void array_reentry ()
{
  auto value = ArrayRef::adopt (g_array_new (FALSE, FALSE, sizeof (GObject *)));
  g_array_set_clear_func (value.get (), [] (gpointer p) { g_object_unref (*static_cast<GObject **> (p)); });
  auto old = object ();
  g_object_weak_ref (old.get (), replace_array, &value);
  auto *raw = old.release ();
  g_array_append_val (value.get (), raw);
  value = ArrayRef::adopt (g_array_new (FALSE, FALSE, sizeof (int)));
  g_assert_cmpuint (value.get ()->len, ==, 1);
  g_assert_cmpint (g_array_index (value.get (), int, 0), ==, 42);
}
struct Reschedule
{
  Reschedule (Source& target, GMainContext *context, int& calls)
    : target (target), context (context), calls (calls) {}
  ~Reschedule ()
  {
    auto *counter = &calls;
    target = Source::idle (context, G_PRIORITY_DEFAULT_IDLE, [counter] { ++*counter; return false; });
  }
  Source& target;
  GMainContext *context;
  int& calls;
};
void source_reentry ()
{
  auto *context = g_main_context_new ();
  int calls = 0;
  Source source;
  auto replace = std::make_shared<Reschedule> (source, context, calls);
  source = Source::idle (context, G_PRIORITY_DEFAULT_IDLE, [replace] { return false; });
  replace.reset ();
  source = Source::idle (context, G_PRIORITY_DEFAULT_IDLE, [&] { calls += 100; return false; });
  while (g_main_context_iteration (context, FALSE)) {}
  g_assert_cmpint (calls, ==, 1);
  source.close ();
  g_main_context_unref (context);
}
void notified (GObject *, GParamSpec *, gpointer data) { ++*static_cast<int *> (data); }
void emit (GObject *owner)
{
  auto *spec = g_param_spec_int ("value", "value", "value", 0, 9, 0, G_PARAM_READWRITE);
  g_param_spec_ref_sink (spec);
  g_signal_emit_by_name (owner, "notify", spec);
  g_param_spec_unref (spec);
}
struct Reconnect { Connection *target; ObjectRef<GObject> *owner; int *calls; };
void reconnect (gpointer data, GClosure *)
{
  auto& state = *static_cast<Reconnect *> (data);
  *state.target = Connection::connect (*state.owner, "notify", G_CALLBACK (notified), state.calls, nullptr);
}
void connection_reentry ()
{
  auto owner = object ();
  int calls = 0;
  Connection connection;
  Reconnect state { &connection, &owner, &calls };
  connection = Connection::connect (owner, "notify", G_CALLBACK (notified), &state, reconnect);
  connection = Connection::connect (owner, "notify", G_CALLBACK (notified), &calls, nullptr);
  emit (owner.get ());
  g_assert_cmpint (calls, ==, 1);
  connection.close ();
  emit (owner.get ());
  g_assert_cmpint (calls, ==, 1);
}
void delete_receiver (gpointer data, GClosure *)
{ static_cast<std::unique_ptr<Connection> *> (data)->reset (); }
void connection_deletion ()
{
  auto owner = object ();
  std::unique_ptr<Connection> connection (new Connection);
  *connection = Connection::connect (owner, "notify", G_CALLBACK (notified), &connection, delete_receiver);
  connection->close ();
  g_assert_null (connection.get ());
}

struct Stats { int closed = 0; int destroyed = 0; };
struct ReentrantImpl
{
  ReentrantImpl (Stats& stats, const std::function<void ()>& during_construction)
    : stats (stats) { during_construction (); }
  ~ReentrantImpl () { ++stats.destroyed; }
  void close () noexcept { ++stats.closed; }
  Stats& stats;
};
struct ReentrantSlot : SlotSpec<GObject, ReentrantImpl> {};
void constructor_close ()
{
  auto owner = object ();
  Stats stats;
  auto& store = BindingStore::ensure (owner.get ());
  bool rejected = false;
  try { store.emplace<ReentrantSlot> (stats, [&] { store.close (); }); }
  catch (const Error& error) { rejected = error.code () == GIMP_PAINTER_ERROR_INVALID_STATE; }
  g_assert_true (rejected);
  g_assert_cmpint (stats.closed, ==, 1);
  g_assert_cmpint (stats.destroyed, ==, 1);
  owner.reset ();
}
void constructor_duplicate ()
{
  Stats stats;
  auto owner = object ();
  auto& store = BindingStore::ensure (owner.get ());
  store.emplace<ReentrantSlot> (stats, [&] {
    bool rejected = false;
    try { store.emplace<ReentrantSlot> (stats, [] {}); }
    catch (const Error& error) { rejected = error.code () == GIMP_PAINTER_ERROR_DUPLICATE_SLOT; }
    g_assert_true (rejected);
    rejected = false;
    try { store.activate (); }
    catch (const Error& error) { rejected = error.code () == GIMP_PAINTER_ERROR_INVALID_STATE; }
    g_assert_true (rejected);
  });
  store.activate ();
  owner.reset ();
  g_assert_cmpint (stats.closed, ==, 1);
  g_assert_cmpint (stats.destroyed, ==, 1);
}
void constructor_owner_loss ()
{
  Stats stats;
  auto owner = object ();
  auto& store = BindingStore::ensure (owner.get ());
  store.emplace<ReentrantSlot> (stats, [&] { owner.reset (); });
  g_assert_cmpint (stats.closed, ==, 1);
  g_assert_cmpint (stats.destroyed, ==, 1);
}
struct FinalizingImpl
{
  explicit FinalizingImpl (std::function<void ()> callback) : callback (std::move (callback)) {}
  void close () noexcept { callback (); }
  std::function<void ()> callback;
};
struct FinalizingSlot : SlotSpec<GObject, FinalizingImpl> {};
void finalizing_reentry ()
{
  bool rejected = false;
  auto owner = object ();
  auto& store = BindingStore::ensure (owner.get ());
  store.emplace<FinalizingSlot> ([&] {
    store.close ();
    try { store.read<FinalizingSlot> ([] (const FinalizingImpl&) {}); }
    catch (const Error& error) { rejected = error.code () == GIMP_PAINTER_ERROR_CLOSED; }
  });
  store.activate ();
  owner.reset ();
  g_assert_true (rejected);
}
}
void painter_test_register_reentry ()
{
  g_test_add_func ("/painter/reentry/ref-move", ref_reentry);
  g_test_add_func ("/painter/reentry/value-reset-move", value_reentry);
  g_test_add_func ("/painter/reentry/array-move", array_reentry);
  g_test_add_func ("/painter/reentry/source-move", source_reentry);
  g_test_add_func ("/painter/reentry/connection-move", connection_reentry);
  g_test_add_func ("/painter/reentry/receiver-deletion", connection_deletion);
  g_test_add_func ("/painter/reentry/constructor-close", constructor_close);
  g_test_add_func ("/painter/reentry/constructor-duplicate", constructor_duplicate);
  g_test_add_func ("/painter/reentry/constructor-owner-loss", constructor_owner_loss);
  g_test_add_func ("/painter/reentry/finalizing-close-read", finalizing_reentry);
}
