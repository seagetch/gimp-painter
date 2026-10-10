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
struct ReplaceRef
{
  ObjectRef<GObject> *target;
  GObject *replacement;
  GObject *published;
  int calls;
};
void replace_ref (gpointer data, GObject *)
{
  auto& state = *static_cast<ReplaceRef *> (data);
  g_assert_true (state.target->get () == state.published);
  ++state.calls;
  *state.target = ObjectRef<GObject>::retain (state.replacement);
}
void ref_reentry ()
{
  for (bool move : {false, true})
    {
      int incoming_destroyed = 0, old_destroyed = 0, replacement_destroyed = 0;
      auto value = object ();
      auto replacement = object ();
      auto incoming = object ();
      g_object_weak_ref (value.get (), count, &old_destroyed);
      g_object_weak_ref (incoming.get (), count, &incoming_destroyed);
      g_object_weak_ref (replacement.get (), count, &replacement_destroyed);
      ReplaceRef state { &value, replacement.get (), incoming.get (), 0 };
      g_object_weak_ref (value.get (), replace_ref, &state);
      if (move) value = std::move (incoming);
      else value = incoming;
      g_assert_cmpint (state.calls, ==, 1);
      g_assert_cmpint (old_destroyed, ==, 1);
      g_assert_true (value.get () == replacement.get ());
      g_assert_cmpuint (replacement.get ()->ref_count, ==, 2);
      g_assert_cmpint (incoming_destroyed, ==, move ? 1 : 0);
      if (move) g_assert_null (incoming.get ());
      else g_assert_cmpuint (incoming.get ()->ref_count, ==, 1);
      incoming.reset ();
      g_assert_cmpint (incoming_destroyed, ==, 1);
      value.reset ();
      g_assert_cmpuint (replacement.get ()->ref_count, ==, 1);
      g_assert_cmpint (replacement_destroyed, ==, 0);
      replacement.reset ();
      g_assert_cmpint (replacement_destroyed, ==, 1);
    }
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
  for (int operation = 0; operation != 3; ++operation)
    {
      Value value (G_TYPE_OBJECT);
      auto old = object ();
      g_object_weak_ref (old.get (), replace_value, &value);
      g_value_take_object (value.get (), old.release ());
      if (operation)
        {
          Value incoming (G_TYPE_STRING);
          g_value_set_string (incoming.get (), "incoming");
          if (operation == 1) value = std::move (incoming);
          else
            {
              value = incoming;
              g_assert_cmpstr (g_value_get_string (incoming.get ()), ==, "incoming");
            }
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
  for (int operation = 0; operation < 3; ++operation)
    {
      auto value = ArrayRef::adopt (g_array_new (FALSE, FALSE, sizeof (GObject *)));
      g_array_set_clear_func (value.get (), [] (gpointer p) { g_object_unref (*static_cast<GObject **> (p)); });
      auto old = object ();
      g_object_weak_ref (old.get (), replace_array, &value);
      auto *raw = old.release ();
      g_array_append_val (value.get (), raw);
      auto incoming = ArrayRef::adopt (g_array_new (FALSE, FALSE, sizeof (int)));
      if (operation == 0) value.reset ();
      else if (operation == 1) value = std::move (incoming);
      else
        {
          value = incoming;
          g_assert_cmpuint (incoming.get ()->len, ==, 0);
        }
      g_assert_cmpuint (value.get ()->len, ==, 1);
      g_assert_cmpint (g_array_index (value.get (), int, 0), ==, 42);
    }
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
struct SignalCloseStats
{
  int calls = 0, closed = 0, destroyed = 0, finalized = 0, scope_exits = 0;
};
struct SignalCloseImpl
{
  explicit SignalCloseImpl (SignalCloseStats& stats) : stats (stats) {}
  ~SignalCloseImpl () { ++stats.destroyed; }
  void close () noexcept { ++stats.closed; connection.close (); }
  SignalCloseStats& stats;
  Connection connection;
  int value = 41;
};
struct SignalCloseSlot : SlotSpec<GObject, SignalCloseImpl> {};
struct SignalCloseContext
{
  ObjectRef<GObject> *external_owner;
  GObject *receiver;
  SignalCloseStats *stats;
  bool fail;
};
void signal_close_callback (GObject *, GParamSpec *, gpointer data)
{
  auto& context = *static_cast<SignalCloseContext *> (data);
  auto& stats = *context.stats;
  ++stats.calls;
  GError *error = nullptr;
  boundary_void (&error, [&] {
    auto& store = BindingStore::require (context.receiver);
    store.with<SignalCloseSlot> ([&] (SignalCloseImpl& impl) {
      // The emitter is a different object, so emission cannot retain receiver.
      g_assert_cmpuint (context.receiver->ref_count, ==, 2);
      store.close ();
      context.external_owner->reset ();
      g_assert_cmpuint (context.receiver->ref_count, ==, 1);
      g_assert_cmpint (stats.closed, ==, 1);
      g_assert_cmpint (stats.destroyed, ==, 0);
      g_assert_cmpint (stats.finalized, ==, 0);
      g_assert_false (impl.connection.connected ());
      struct CheckExit
      {
        SignalCloseImpl& impl;
        SignalCloseContext& context;
        ~CheckExit ()
        {
          g_assert_cmpint (impl.value, ==, 41);
          g_assert_cmpint (context.stats->destroyed, ==, 0);
          g_assert_cmpint (context.stats->finalized, ==, 0);
          g_assert_cmpuint (context.receiver->ref_count, ==, 1);
          ++context.stats->scope_exits;
        }
      } check_exit {impl, context};
      if (context.fail) throw std::runtime_error ("signal close unwind");
    });
  });
  if (context.fail)
    {
      g_assert_error (error, GIMP_PAINTER_ERROR, GIMP_PAINTER_ERROR_EXCEPTION);
      g_assert_cmpstr (error->message, ==, "signal close unwind");
      g_clear_error (&error);
    }
  else g_assert_no_error (error);
  g_assert_cmpint (stats.scope_exits, ==, 1);
  g_assert_cmpint (stats.destroyed, ==, 1);
  g_assert_cmpint (stats.finalized, ==, 1);
}
void signal_close_lifetime ()
{
  for (bool fail : {false, true})
    {
      SignalCloseStats stats;
      auto emitter = object ();
      auto owner = object ();
      g_object_weak_ref (owner.get (), count, &stats.finalized);
      SignalCloseContext context {&owner, owner.get (), &stats, fail};
      auto& store = BindingStore::ensure (owner.get ());
      store.emplace<SignalCloseSlot> (stats);
      store.activate ();
      store.with<SignalCloseSlot> ([&] (SignalCloseImpl& impl) {
        impl.connection = Connection::connect (emitter, "notify",
          G_CALLBACK (signal_close_callback), &context, nullptr);
      });
      g_assert_cmpuint (owner.get ()->ref_count, ==, 1);
      emit (emitter.get ());
      g_assert_null (owner.get ());
      g_assert_cmpint (stats.calls, ==, 1);
      g_assert_cmpint (stats.closed, ==, 1);
      g_assert_cmpint (stats.destroyed, ==, 1);
      g_assert_cmpint (stats.finalized, ==, 1);
      emit (emitter.get ());
      g_assert_cmpint (stats.calls, ==, 1);
    }
}
struct QueuedStats
{
  int queued = 0, delivered = 0, mutated = 0, rejected = 0, expired = 0;
  int tickets_destroyed = 0, closed = 0, destroyed = 0, finalized = 0;
  int closing_deliveries = 0;
};
struct QueuedImpl
{
  explicit QueuedImpl (QueuedStats& stats) : stats (stats) {}
  ~QueuedImpl () noexcept { ++stats.destroyed; }
  void close () noexcept
  {
    ++stats.closed;
    connection.close ();
    if (on_close) on_close ();
  }
  QueuedStats& stats;
  Connection connection;
  std::function<void ()> on_close;
  int value = 41;
};
struct QueuedSlot : SlotSpec<GObject, QueuedImpl> {};
struct QueuedTicket
{
  QueuedTicket (const ObjectRef<GObject>& owner, std::uint64_t generation,
                QueuedStats& stats) : owner (owner), generation (generation), stats (stats) {}
  ~QueuedTicket () { ++stats.tickets_destroyed; }
  WeakRef<GObject> owner;
  std::uint64_t generation;
  QueuedStats& stats;
};
struct QueueContext
{
  WeakRef<GObject> owner;
  GMainContext *main;
  Source *source;
  QueuedStats *stats;
  bool timeout, mismatch;
};
void enqueue_callback (GObject *, GParamSpec *, gpointer data) noexcept
{
  auto& context = *static_cast<QueueContext *> (data);
  GError *error = nullptr;
  boundary_void (&error, [&] {
    auto owner = context.owner.lock ();
    g_assert_nonnull (owner.get ());
    auto& store = BindingStore::require (owner.get ());
    auto ticket = std::make_shared<QueuedTicket> (
      owner, store.generation () + (context.mismatch ? 1 : 0), *context.stats);
    auto deliver = [ticket] {
      auto& stats = ticket->stats;
      ++stats.delivered; // Prove real dispatch, including rejected deliveries.
      auto owner = ticket->owner.lock ();
      if (!owner) { ++stats.expired; return false; }
      auto& store = BindingStore::require (owner.get ());
      if (store.state () == BindingStore::State::closing) ++stats.closing_deliveries;
      if (!store.accepts (ticket->generation)) { ++stats.rejected; return false; }
      store.with<QueuedSlot> ([&] (QueuedImpl& impl) { ++impl.value; ++stats.mutated; });
      return false;
    };
    *context.source = context.timeout
      ? Source::timeout (context.main, 0, G_PRIORITY_DEFAULT, deliver)
      : Source::idle (context.main, G_PRIORITY_DEFAULT_IDLE, deliver);
    ++context.stats->queued;
  });
  g_assert_no_error (error);
}
void queued_callback_invalidation ()
{
  // The queued Source intentionally survives disconnect. Admission belongs to
  // the weak owner + lifecycle generation adapter, not Connection alone.
  enum { active, closed, finalized, mismatch, during_close };
  for (bool timeout : {false, true})
    for (int scenario = active; scenario <= during_close; ++scenario)
      {
        auto *main = g_main_context_new ();
        QueuedStats stats;
        auto emitter = object ();
        auto owner = object ();
        Source source;
        QueueContext context {WeakRef<GObject> (owner), main, &source, &stats,
                              timeout, scenario == mismatch};
        g_object_weak_ref (owner.get (), count, &stats.finalized);
        auto& store = BindingStore::ensure (owner.get ());
        store.emplace<QueuedSlot> (stats);
        store.activate ();
        const auto generation = store.generation ();
        store.with<QueuedSlot> ([&] (QueuedImpl& impl) {
          impl.connection = Connection::connect (emitter, "notify",
            G_CALLBACK (enqueue_callback), &context, nullptr);
          if (scenario == during_close)
            impl.on_close = [&] {
              g_assert_true (store.state () == BindingStore::State::closing);
              g_assert_true (source.active ());
              g_assert_true (g_main_context_iteration (main, FALSE));
              g_assert_cmpint (stats.closing_deliveries, ==, 1);
              g_assert_cmpint (stats.mutated, ==, 0);
            };
        });
        emit (emitter.get ());
        g_assert_cmpint (stats.queued, ==, 1);
        g_assert_cmpint (stats.delivered, ==, 0);
        g_assert_cmpint (stats.tickets_destroyed, ==, 0);
        g_assert_cmpuint (owner.get ()->ref_count, ==, 1); // No queue/owner cycle.
        g_assert_true (source.active ());
        if (scenario == closed || scenario == finalized || scenario == during_close)
          {
            store.close ();
            g_assert_cmpuint (store.generation (), ==, generation + 1);
            g_assert_cmpint (stats.closed, ==, 1);
            store.read<QueuedSlot> ([] (const QueuedImpl& impl) {
              g_assert_false (impl.connection.connected ());
              g_assert_cmpint (impl.value, ==, 41);
            });
            emit (emitter.get ());
            g_assert_cmpint (stats.queued, ==, 1); // Disconnect prevents new work.
          }
        if (scenario == finalized)
          {
            owner.reset ();
            g_assert_cmpint (stats.finalized, ==, 1);
            g_assert_cmpint (stats.destroyed, ==, 1);
          }
        if (scenario != during_close)
          {
            g_assert_true (source.active ()); // Not canceled before delivery.
            g_assert_true (g_main_context_iteration (main, FALSE));
          }
        g_assert_cmpint (stats.delivered, ==, 1);
        g_assert_cmpint (stats.mutated, ==, scenario == active ? 1 : 0);
        g_assert_cmpint (stats.expired, ==, scenario == finalized ? 1 : 0);
        g_assert_cmpint (stats.rejected, ==,
                        scenario == closed || scenario == mismatch || scenario == during_close ? 1 : 0);
        g_assert_cmpint (stats.tickets_destroyed, ==, 1);
        g_assert_false (source.active ());
        g_assert_false (g_main_context_iteration (main, FALSE));
        if (owner)
          {
            store.read<QueuedSlot> ([&] (const QueuedImpl& impl) {
              g_assert_cmpint (impl.value, ==, scenario == active ? 42 : 41);
            });
            if (scenario == mismatch)
              {
                g_assert_true (store.state () == BindingStore::State::active);
                g_assert_cmpuint (store.generation (), ==, generation);
              }
            store.close ();
            owner.reset ();
          }
        g_assert_cmpint (stats.closed, ==, 1);
        g_assert_cmpint (stats.destroyed, ==, 1);
        g_assert_cmpint (stats.finalized, ==, 1);
        source.close ();
        g_main_context_unref (main);
      }
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
  g_test_add_func ("/painter/signal/close-receiver-lifetime", signal_close_lifetime);
  g_test_add_func ("/painter/source/queued-callback-invalidation", queued_callback_invalidation);
  g_test_add_func ("/painter/reentry/finalizing-close-read", finalizing_reentry);
}
