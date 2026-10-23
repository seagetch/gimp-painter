/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "test-registry.hpp"
#include "connection.hpp"
#include "resources.hpp"
#include "source.hpp"
#include <initializer_list>
#include <vector>

using namespace GimpPainter;
namespace {
void count_destroy (gpointer data, GObject *) { ++*static_cast<int *> (data); }
ObjectRef<GObject> new_object ()
{ return ObjectRef<GObject>::adopt (G_OBJECT (g_object_new (G_TYPE_OBJECT, nullptr))); }

void values ()
{
  int destroyed = 0;
  auto owner = new_object ();
  g_object_weak_ref (owner.get (), count_destroy, &destroyed);
  {
    Value value (G_TYPE_OBJECT);
    g_value_set_object (value.get (), owner.get ());
    Value copy (value);
    Value moved (std::move (copy));
    g_assert_false (G_IS_VALUE (copy.get ()));
    auto *same = &moved;
    moved = *same;
    moved = std::move (*same);
    Value string (G_TYPE_STRING);
    g_value_set_string (string.get (), "retained text");
    string = std::move (moved);
    owner.reset ();
    value.reset ();
    g_assert_cmpint (destroyed, ==, 0);
    Value borrowed_copy = ValueView (string.get ()).copy ();
    string.reset ();
    g_assert_cmpint (destroyed, ==, 0);
    borrowed_copy.reset ();
    g_assert_cmpint (destroyed, ==, 1);
  }
  String text (g_strdup ("text"));
  g_assert_cmpstr (text.get (), ==, "text");
}

struct BoxCounts { int allocated = 0; int copied = 0; int freed = 0; };
struct CountedBox { BoxCounts *counts; int value; };
gpointer copy_box (gpointer data)
{
  auto *box = static_cast<CountedBox *> (data);
  auto *copy = new CountedBox (*box);
  ++box->counts->allocated; ++box->counts->copied;
  return copy;
}
void free_box (gpointer data)
{
  auto *box = static_cast<CountedBox *> (data);
  ++box->counts->freed;
  delete box;
}
void value_ownership ()
{
  Value empty;
  g_assert_false (G_IS_VALUE (empty.get ()));
  auto null_copy = Value::copy (nullptr);
  auto empty_copy = ValueView (empty.get ()).copy ();
  g_assert_false (G_IS_VALUE (null_copy.get ()));
  g_assert_false (G_IS_VALUE (empty_copy.get ()));
  empty.reset (); empty.reset ();
  for (bool heap : {false, true})
    {
      int destroyed = 0;
      auto owner = new_object ();
      auto *raw = owner.get ();
      g_object_weak_ref (raw, count_destroy, &destroyed);
      GValue stack = G_VALUE_INIT;
      GValue *native = heap ? g_new0 (GValue, 1) : &stack;
      g_value_init (native, G_TYPE_OBJECT);
      g_value_take_object (native, owner.release ());
      Value copied;
      {
        ValueView borrow (native);
        g_assert_true (borrow.get () == native);
        g_assert_cmpuint (raw->ref_count, ==, 1);
        copied = borrow.copy ();
        g_assert_true (copied.get () != native);
        g_assert_cmpuint (raw->ref_count, ==, 2);
      }
      g_assert_cmpuint (raw->ref_count, ==, 2); // borrow destructor does not unset
      g_value_unset (native);
      if (heap) g_free (native); // the C producer retains ownership of its shell
      g_assert_cmpuint (raw->ref_count, ==, 1);
      g_assert_true (g_value_get_object (copied.get ()) == raw);
      copied.reset (); copied.reset ();
      g_assert_cmpint (destroyed, ==, 1);
    }
  Value string (G_TYPE_STRING);
  g_value_set_string (string.get (), "copied text");
  auto string_copy = ValueView (string.get ()).copy ();
  g_value_set_string (string.get (), "changed input");
  g_assert_cmpstr (g_value_get_string (string_copy.get ()), ==, "copied text");

  const GType type = g_boxed_type_register_static ("PainterCountedValueBox", copy_box, free_box);
  BoxCounts counts, displaced;
  {
    Value value (type);
    ++counts.allocated;
    g_value_take_boxed (value.get (), new CountedBox { &counts, 37 });
    Value copy (value);
    g_assert_cmpint (counts.copied, ==, 1);
    g_assert_true (g_value_get_boxed (copy.get ()) != g_value_get_boxed (value.get ()));
    Value destination (type);
    ++displaced.allocated;
    g_value_take_boxed (destination.get (), new CountedBox { &displaced, -1 });
    destination = value;
    g_assert_cmpint (displaced.freed, ==, 1);
    g_assert_cmpint (counts.copied, ==, 2);
    g_assert_true (g_value_get_boxed (destination.get ()) != g_value_get_boxed (value.get ()));
    g_assert_cmpint (static_cast<CountedBox *> (g_value_get_boxed (destination.get ()))->value, ==, 37);
    auto *alias = &destination;
    destination = *alias;
    g_assert_cmpint (counts.copied, ==, 3);
    g_assert_cmpint (counts.freed, ==, 1);
    auto *self_payload = g_value_get_boxed (destination.get ());
    g_assert_cmpint (static_cast<CountedBox *> (self_payload)->value, ==, 37);
    destination = std::move (*alias);
    g_assert_true (g_value_get_boxed (destination.get ()) == self_payload);
    g_assert_cmpint (counts.copied, ==, 3);
    g_assert_cmpint (counts.freed, ==, 1);
    destination = std::move (copy);
    g_assert_false (G_IS_VALUE (copy.get ()));
    g_assert_cmpint (counts.freed, ==, 2);
    Value moved (std::move (destination));
    g_assert_false (G_IS_VALUE (destination.get ()));
    g_assert_cmpint (static_cast<CountedBox *> (g_value_get_boxed (moved.get ()))->value, ==, 37);
    value = empty; // copy assignment from empty releases the old boxed payload
    g_assert_false (G_IS_VALUE (value.get ()));
    g_assert_cmpint (counts.freed, ==, 3);
    moved = Value (); // move assignment from empty releases its old payload too
    g_assert_false (G_IS_VALUE (moved.get ()));
    g_assert_cmpint (counts.freed, ==, 4);
  }
  g_assert_cmpint (counts.allocated, ==, 4);
  g_assert_cmpint (counts.freed, ==, counts.allocated);
  g_assert_cmpint (displaced.freed, ==, displaced.allocated);
}

void errors ()
{
  GError *error = g_error_new_literal (GIMP_PAINTER_ERROR,
                                     GIMP_PAINTER_ERROR_INVALID_STATE,
                                     "失敗した処理の所有権");
  const auto message = take_error_message (error, "fallback");
  g_assert_null (error);
  g_assert_cmpstr (message.c_str (), ==, "失敗した処理の所有権");
  const auto fallback = take_error_message (error, "fallback");
  g_assert_cmpstr (fallback.c_str (), ==, "fallback");
  g_assert_null (error);
}

void arrays ()
{
  int destroyed = 0;
  auto *raw = G_OBJECT (g_object_new (G_TYPE_OBJECT, nullptr));
  g_object_weak_ref (raw, count_destroy, &destroyed);
  auto array = ArrayRef::adopt (g_array_new (FALSE, FALSE, sizeof (GObject *)));
  g_array_set_clear_func (array.get (), [] (gpointer element) { g_object_unref (*static_cast<GObject **> (element)); });
  g_array_append_val (array.get (), raw); // array now owns the only reference
  auto copy = array;
  ArrayRef moved = std::move (copy);
  array.reset ();
  auto *same = &moved;
  moved = std::move (*same);
  g_assert_cmpint (destroyed, ==, 0);
  moved = ArrayRef::adopt (g_array_new (FALSE, FALSE, sizeof (int)));
  g_assert_cmpint (destroyed, ==, 1);
  g_assert_cmpuint (moved.get ()->len, ==, 0);
}
ArrayRef counted_array (int& destroyed)
{
  auto result = ArrayRef::adopt (g_array_new (FALSE, FALSE, sizeof (GObject *)));
  g_array_set_clear_func (result.get (), [] (gpointer element) {
    g_object_unref (*static_cast<GObject **> (element));
  });
  auto *object = G_OBJECT (g_object_new (G_TYPE_OBJECT, nullptr));
  g_object_weak_ref (object, count_destroy, &destroyed);
  g_array_append_val (result.get (), object);
  return result;
}
void array_ownership ()
{
  auto adopted_null = ArrayRef::adopt (nullptr);
  auto retained_null = ArrayRef::retain (nullptr);
  auto copied_null = retained_null;
  g_assert_null (adopted_null.get ());
  g_assert_null (copied_null.get ());
  int destroyed = 0, displaced = 0;
  auto source = counted_array (destroyed);
  auto *raw = source.release ();
  g_assert_null (source.get ());
  auto retained = ArrayRef::retain (raw);
  g_array_unref (raw); // release the producer's reference, keeping the retained one
  g_assert_cmpint (destroyed, ==, 0);
  auto copy = retained;
  auto destination = counted_array (displaced);
  destination = retained;
  g_assert_cmpint (displaced, ==, 1);
  g_assert_true (destination.get () == retained.get ());
  auto *alias = &destination;
  destination = *alias;
  destination = std::move (*alias);
  g_assert_true (destination.get () == retained.get ());
  destination = std::move (copy);
  g_assert_null (copy.get ());
  retained.reset ();
  g_assert_cmpint (destroyed, ==, 0);
  g_assert_cmpuint (destination.get ()->len, ==, 1);
  raw = destination.release ();
  destination.reset (); destination.reset ();
  g_assert_cmpint (destroyed, ==, 0);
  g_array_unref (raw);
  g_assert_cmpint (destroyed, ==, 1);
  int empty_copy_destroyed = 0, empty_move_destroyed = 0;
  auto old_copy = counted_array (empty_copy_destroyed);
  old_copy = copied_null;
  g_assert_null (old_copy.get ());
  g_assert_cmpint (empty_copy_destroyed, ==, 1);
  auto old_move = counted_array (empty_move_destroyed);
  old_move = ArrayRef ();
  g_assert_null (old_move.get ());
  g_assert_cmpint (empty_move_destroyed, ==, 1);
  auto empty = ArrayRef::adopt (g_array_new (FALSE, TRUE, sizeof (int)));
  g_assert_cmpuint (empty.get ()->len, ==, 0);
  auto empty_shared = empty;
  const int value = 17;
  g_array_append_val (empty.get (), value);
  g_assert_cmpuint (empty_shared.get ()->len, ==, 1);
  g_assert_cmpint (g_array_index (empty_shared.get (), int, 0), ==, 17);
  g_array_set_size (empty.get (), 0);
  g_assert_cmpuint (empty_shared.get ()->len, ==, 0); // no index-zero access on empty
}
void string_ownership ()
{
  static_assert (!std::is_copy_constructible<String>::value, "GLib string ownership is explicit");
  String value (g_strdup ("owned UTF-8: \u753b\u50cf"));
  String copy (g_strdup (value.get ()));
  g_assert_true (value.get () != copy.get ());
  value.get ()[0] = 'O';
  g_assert_cmpstr (copy.get (), ==, "owned UTF-8: \u753b\u50cf");
  auto *raw = value.get ();
  String moved (std::move (value));
  g_assert_null (value.get ());
  g_assert_true (moved.get () == raw);
  String replaced (g_strdup_printf ("%s-%d", "old", 7));
  replaced = std::move (moved);
  String& alias = replaced;
  replaced = std::move (alias);
  g_assert_true (replaced.get () == raw);
  g_assert_null (moved.get ());
  raw = replaced.release ();
  g_assert_null (replaced.get ());
  g_assert_cmpstr (raw, ==, "Owned UTF-8: \u753b\u50cf");
  g_free (raw);
  copy.reset (g_strdup (""));
  g_assert_cmpstr (copy.get (), ==, "");
  copy.reset (); copy.reset ();
  try
    {
      String temporary (static_cast<gchar *> (g_malloc0 (32)));
      temporary.get ()[0] = 'x';
      throw 1;
    }
  catch (int) {}
}

void mutex_guard ()
{
  GMutex mutex;
  g_mutex_init (&mutex);
  try { MutexGuard guard (mutex); throw 1; }
  catch (int) {}
  g_assert_true (g_mutex_trylock (&mutex));
  g_mutex_unlock (&mutex);
  g_mutex_clear (&mutex);
}

void on_notify (GObject *, GParamSpec *, gpointer data)
{ ++*static_cast<int *> (data); }
void notify (GObject *object)
{
  auto *spec = g_param_spec_int ("value", "value", "value", 0, 99, 0, G_PARAM_READWRITE);
  g_param_spec_ref_sink (spec);
  g_signal_emit_by_name (object, "notify", spec);
  g_param_spec_unref (spec);
}
void connections ()
{
  int count = 0;
  auto owner = new_object ();
  auto connection = Connection::connect (owner, "notify", G_CALLBACK (on_notify), &count, nullptr);
  notify (owner.get ());
  g_assert_cmpint (count, ==, 1);
  connection.block ();
  connection.block ();
  notify (owner.get ());
  connection.unblock ();
  notify (owner.get ());
  g_assert_cmpint (count, ==, 1);
  connection.unblock ();
  connection.unblock (); // unmatched unblock is a no-op
  notify (owner.get ());
  g_assert_cmpint (count, ==, 2);
  auto moved = std::move (connection);
  g_assert_false (connection.connected ());
  g_assert_true (moved.connected ());
  owner.reset ();
  g_assert_false (moved.connected ());
  moved.close ();
  moved.close ();
}
struct ClosureCounts { int calls = 0; int destroyed = 0; };
struct ClosureData { ClosureCounts *counts; };
void owned_notify (GObject *, GParamSpec *, gpointer data)
{ ++static_cast<ClosureData *> (data)->counts->calls; }
void owned_closure_destroy (gpointer data, GClosure *)
{
  auto *owned = static_cast<ClosureData *> (data);
  ++owned->counts->destroyed;
  delete owned;
}
void connection_ownership ()
{
  // Target first, explicit close, wrapper destruction, and external disconnect.
  for (int order = 0; order < 4; ++order)
    {
      int finalized = 0;
      ClosureCounts counts;
      auto emitter = new_object ();
      g_object_weak_ref (emitter.get (), count_destroy, &finalized);
      {
        auto *data = new ClosureData { &counts };
        auto connection = Connection::connect (emitter, "notify", G_CALLBACK (owned_notify),
                                                data, owned_closure_destroy);
        g_assert_cmpuint (emitter.get ()->ref_count, ==, 1);
        notify (emitter.get ());
        g_assert_cmpint (counts.calls, ==, 1);
        g_assert_cmpint (counts.destroyed, ==, 0);
        if (order == 0)
          {
            emitter.reset ();
            g_assert_cmpint (finalized, ==, 1);
            g_assert_cmpint (counts.destroyed, ==, 1);
            g_assert_false (connection.connected ());
            connection.block (); connection.unblock ();
            connection.close (); connection.close ();
          }
        else if (order == 1)
          {
            connection.close (); connection.close ();
            g_assert_false (connection.connected ());
          }
        else if (order == 3)
          {
            g_assert_cmpuint (g_signal_handlers_disconnect_by_data (emitter.get (), data), ==, 1);
            g_assert_false (connection.connected ());
            connection.close ();
          }
      }
      g_assert_cmpint (counts.destroyed, ==, 1);
      if (emitter)
        {
          notify (emitter.get ());
          g_assert_cmpint (counts.calls, ==, 1);
          g_assert_cmpint (finalized, ==, 0);
          emitter.reset ();
        }
      g_assert_cmpint (finalized, ==, 1);
      g_assert_cmpint (counts.destroyed, ==, 1);
    }
  ClosureCounts first, displaced;
  auto emitter = new_object ();
  auto source = Connection::connect (emitter, "notify", G_CALLBACK (owned_notify),
                                     new ClosureData { &first }, owned_closure_destroy);
  auto destination = Connection::connect (emitter, "notify", G_CALLBACK (owned_notify),
                                          new ClosureData { &displaced }, owned_closure_destroy);
  destination = std::move (source);
  g_assert_false (source.connected ());
  g_assert_true (destination.connected ());
  g_assert_cmpint (displaced.destroyed, ==, 1);
  Connection& alias = destination;
  destination = std::move (alias);
  auto moved = std::move (destination);
  g_assert_false (destination.connected ());
  notify (emitter.get ());
  g_assert_cmpint (first.calls, ==, 1);
  g_assert_cmpint (displaced.calls, ==, 0);
  moved.close (); source.close (); destination.close ();
  g_assert_cmpint (first.destroyed, ==, 1);
  g_assert_cmpint (displaced.destroyed, ==, 1);
  g_assert_cmpuint (emitter.get ()->ref_count, ==, 1);
}
struct Reconnect
{
  Connection *connection;
  ObjectRef<GObject> *owner;
  int *count;
};
void reconnect_on_destroy (gpointer data, GClosure *)
{
  auto& state = *static_cast<Reconnect *> (data);
  *state.connection = Connection::connect (*state.owner, "notify", G_CALLBACK (on_notify), state.count, nullptr);
}
void reentrant_disconnect ()
{
  int count = 0;
  auto owner = new_object ();
  Connection connection;
  Reconnect data { &connection, &owner, &count };
  connection = Connection::connect (owner, "notify", G_CALLBACK (on_notify), &data, reconnect_on_destroy);
  connection.close ();
  g_assert_true (connection.connected ());
  notify (owner.get ());
  g_assert_cmpint (count, ==, 1);
  connection.close ();
  notify (owner.get ());
  g_assert_cmpint (count, ==, 1);
}

void ordered_notify (GObject *, GParamSpec *, gpointer data)
{ static_cast<std::vector<int> *> (data)->push_back (1); }
void after_notify (GObject *, GParamSpec *, gpointer data)
{ static_cast<std::vector<int> *> (data)->push_back (2); }
void signal_order ()
{
  std::vector<int> order;
  auto owner = new_object ();
  auto after = Connection::connect (owner, "notify", G_CALLBACK (after_notify), &order, nullptr, G_CONNECT_AFTER);
  auto before = Connection::connect (owner, "notify", G_CALLBACK (ordered_notify), &order, nullptr);
  notify (owner.get ());
  g_assert_cmpuint (order.size (), ==, 2);
  g_assert_cmpint (order[0], ==, 1);
  g_assert_cmpint (order[1], ==, 2);
  before.close ();
  after.close ();
  notify (owner.get ());
  g_assert_cmpuint (order.size (), ==, 2);
}

struct PhaseTrace
{
  int phases[8] = {};
  guint size = 0;
  void add (int phase)
  { g_assert_cmpuint (size, <, G_N_ELEMENTS (phases)); phases[size++] = phase; }
  void expect (std::initializer_list<int> expected)
  {
    g_assert_cmpuint (size, ==, expected.size ());
    guint i = 0;
    for (int phase : expected) g_assert_cmpint (phases[i++], ==, phase);
    size = 0;
  }
};
void default_phase (GObject *, gpointer trace, gpointer)
{ static_cast<PhaseTrace *> (trace)->add (2); }
void handler_phase (GObject *, gpointer trace, gpointer phase)
{ static_cast<PhaseTrace *> (trace)->add (GPOINTER_TO_INT (phase)); }
void signal_block_order ()
{
  const guint signal = g_signal_new_class_handler ("painter-foundation-phase-order", G_TYPE_OBJECT,
    G_SIGNAL_RUN_LAST, G_CALLBACK (default_phase), nullptr, nullptr,
    g_cclosure_marshal_VOID__POINTER, G_TYPE_NONE, 1, G_TYPE_POINTER);
  g_assert_cmpuint (signal, !=, 0);
  auto owner = new_object ();
  PhaseTrace trace;
  auto emit = [&] { g_signal_emit (owner.get (), signal, 0, &trace); };
  // Register AFTER first: phase semantics must still override registration order.
  auto after = Connection::connect (owner, "painter-foundation-phase-order",
    G_CALLBACK (handler_phase), GINT_TO_POINTER (3), nullptr, G_CONNECT_AFTER);
  auto before = Connection::connect (owner, "painter-foundation-phase-order",
    G_CALLBACK (handler_phase), GINT_TO_POINTER (1), nullptr);
  emit (); trace.expect ({1, 2, 3});
  before.block (); before.block ();
  emit (); trace.expect ({2, 3});
  before.unblock ();
  emit (); trace.expect ({2, 3});
  auto moved = std::move (before);
  before.unblock (); // moved-from wrapper cannot consume the remaining block
  emit (); trace.expect ({2, 3});
  moved.unblock (); moved.unblock ();
  emit (); trace.expect ({1, 2, 3});
  after.block ();
  emit (); trace.expect ({1, 2});
  after.unblock ();
  emit (); trace.expect ({1, 2, 3});
  // An unmatched wrapper unblock must not consume an external native block.
  g_assert_cmpuint (g_signal_handlers_block_matched (owner.get (), G_SIGNAL_MATCH_DATA,
    0, 0, nullptr, nullptr, GINT_TO_POINTER (1)), ==, 1);
  moved.unblock ();
  emit (); trace.expect ({2, 3});
  moved.block (); moved.unblock ();
  emit (); trace.expect ({2, 3});
  g_assert_cmpuint (g_signal_handlers_unblock_matched (owner.get (), G_SIGNAL_MATCH_DATA,
    0, 0, nullptr, nullptr, GINT_TO_POINTER (1)), ==, 1);
  emit (); trace.expect ({1, 2, 3});
  moved.block ();
  moved = Connection::connect (owner, "painter-foundation-phase-order",
    G_CALLBACK (handler_phase), GINT_TO_POINTER (1), nullptr);
  emit (); trace.expect ({1, 2, 3});
  moved.unblock (); // replaced handler starts with no wrapper-owned block
  emit (); trace.expect ({1, 2, 3});
  moved.close (); after.close ();
  emit (); trace.expect ({2});
}

void sources ()
{
  auto *context = g_main_context_new ();
  int calls = 0;
  auto canceled = Source::idle (context, G_PRIORITY_DEFAULT_IDLE, [&] { ++calls; return false; });
  canceled.close ();
  g_assert_false (g_main_context_iteration (context, FALSE));
  g_assert_cmpint (calls, ==, 0);
  auto active = Source::idle (context, G_PRIORITY_DEFAULT_IDLE, [&] { ++calls; return calls < 2; });
  g_assert_true (active.active ());
  g_assert_true (g_main_context_iteration (context, FALSE));
  g_assert_true (active.active ());
  g_assert_true (g_main_context_iteration (context, FALSE));
  g_assert_false (active.active ());
  g_assert_cmpint (calls, ==, 2);
  active.close ();
  g_main_context_unref (context);
}
struct Capture
{
  explicit Capture (int& destroyed) : destroyed (destroyed) {}
  ~Capture () { ++destroyed; }
  int& destroyed;
};
void source_ownership ()
{
  for (bool timeout : {false, true})
    {
      auto *context = g_main_context_new ();
      auto create = [context, timeout] (std::function<bool ()> callback) {
        return timeout ? Source::timeout (context, 0, G_PRIORITY_DEFAULT_IDLE, std::move (callback))
                       : Source::idle (context, G_PRIORITY_DEFAULT_IDLE, std::move (callback));
      };
      for (int path = 0; path < 3; ++path)
        {
          int calls = 0, destroyed = 0;
          auto capture = std::make_shared<Capture> (destroyed);
          std::weak_ptr<Capture> weak = capture;
          {
            auto source = create ([capture, &calls, path] { ++calls; return path == 2 && calls == 1; });
            capture.reset ();
            g_assert_false (weak.expired ());
            if (path == 0)
              {
                source.close (); source.close ();
                g_assert_false (source.active ());
                g_assert_true (weak.expired ());
                g_assert_cmpint (destroyed, ==, 1);
                g_assert_false (g_main_context_iteration (context, FALSE));
                g_assert_cmpint (calls, ==, 0);
              }
            if (path == 2)
              {
                g_assert_true (g_main_context_iteration (context, FALSE));
                g_assert_true (source.active ());
                g_assert_false (weak.expired ());
                g_assert_cmpint (destroyed, ==, 0);
                g_assert_true (g_main_context_iteration (context, FALSE));
                g_assert_false (source.active ());
                g_assert_true (weak.expired ());
                g_assert_cmpint (destroyed, ==, 1);
              }
          } // path 1 cancels through the wrapper destructor
          g_assert_true (weak.expired ());
          g_assert_cmpint (destroyed, ==, 1);
          g_assert_false (g_main_context_iteration (context, FALSE));
          g_assert_cmpint (calls, ==, path == 2 ? 2 : 0);
        }
      int first_calls = 0, old_calls = 0, first_destroyed = 0, old_destroyed = 0;
      auto first = std::make_shared<Capture> (first_destroyed);
      auto old = std::make_shared<Capture> (old_destroyed);
      auto source = create ([first, &first_calls] { ++first_calls; return false; });
      auto destination = create ([old, &old_calls] { ++old_calls; return false; });
      first.reset (); old.reset ();
      destination = std::move (source);
      g_assert_false (source.active ());
      g_assert_cmpint (old_destroyed, ==, 1);
      Source& alias = destination;
      destination = std::move (alias);
      auto moved = std::move (destination);
      source.close (); destination.close ();
      g_assert_true (g_main_context_iteration (context, FALSE));
      g_assert_false (g_main_context_iteration (context, FALSE));
      g_assert_cmpint (first_calls, ==, 1);
      g_assert_cmpint (old_calls, ==, 0);
      g_assert_cmpint (first_destroyed, ==, 1);
      g_assert_cmpint (old_destroyed, ==, 1);
      moved.close ();
      g_main_context_unref (context);
    }
}
void source_self_close ()
{
  for (bool timeout : {false, true})
    {
      auto *context = g_main_context_new ();
      int destroyed = 0, calls = 0;
      auto capture = std::make_shared<Capture> (destroyed);
      Source source;
      auto callback = [&, capture] {
        ++calls;
        source.close ();
        g_assert_false (source.active ());
        g_assert_cmpint (destroyed, ==, 0);
        g_assert_true (capture != nullptr);
        return true; // an already destroyed source must not repeat
      };
      source = timeout ? Source::timeout (context, 0, G_PRIORITY_DEFAULT_IDLE, std::move (callback))
                       : Source::idle (context, G_PRIORITY_DEFAULT_IDLE, std::move (callback));
      capture.reset ();
      g_assert_true (g_main_context_iteration (context, FALSE));
      g_assert_cmpint (destroyed, ==, 1);
      g_assert_false (g_main_context_iteration (context, FALSE));
      g_assert_cmpint (calls, ==, 1);
      g_main_context_unref (context);
    }
}
void source_exception ()
{
  auto *context = g_main_context_new ();
  int destroyed = 0;
  auto capture = std::make_shared<Capture> (destroyed);
  auto source = Source::idle (context, G_PRIORITY_DEFAULT_IDLE, [capture] () -> bool { throw std::runtime_error ("injected"); });
  capture.reset ();
  g_test_expect_message (nullptr, G_LOG_LEVEL_WARNING, "*Painter source callback failed: injected*");
  g_assert_true (g_main_context_iteration (context, FALSE));
  g_test_assert_expected_messages ();
  g_assert_false (source.active ());
  g_assert_cmpint (destroyed, ==, 1);
  source.close ();
  g_assert_false (g_main_context_iteration (context, FALSE));
  g_assert_cmpint (destroyed, ==, 1);
  g_main_context_unref (context);
}
}

void painter_test_register_resources ()
{
  g_test_add_func ("/painter/resources/value-assignment", values);
  g_test_add_func ("/painter/resources/value-owned-borrow-copy", value_ownership);
  g_test_add_func ("/painter/resources/array-assignment", arrays);
  g_test_add_func ("/painter/resources/array-owned-ref-release", array_ownership);
  g_test_add_func ("/painter/resources/glib-string-owner", string_ownership);
  g_test_add_func ("/painter/resources/error-message-transfer", errors);
  g_test_add_func ("/painter/resources/mutex-exception", mutex_guard);
  g_test_add_func ("/painter/signal/lifetime-blocks", connections);
  g_test_add_func ("/painter/signal/owned-lifetime", connection_ownership);
  g_test_add_func ("/painter/signal/after-order", signal_order);
  g_test_add_func ("/painter/signal/block-phase-order", signal_block_order);
  g_test_add_func ("/painter/signal/reentrant-disconnect", reentrant_disconnect);
  g_test_add_func ("/painter/source/cancel-repeat", sources);
  g_test_add_func ("/painter/source/owned-lifetime", source_ownership);
  g_test_add_func ("/painter/source/self-close", source_self_close);
  g_test_add_func ("/painter/source/exception", source_exception);
}
