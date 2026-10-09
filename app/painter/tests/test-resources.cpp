/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "test-registry.hpp"
#include "connection.hpp"
#include "resources.hpp"
#include "source.hpp"
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
void source_self_close ()
{
  auto *context = g_main_context_new ();
  int destroyed = 0;
  auto capture = std::make_shared<Capture> (destroyed);
  Source source;
  source = Source::idle (context, G_PRIORITY_DEFAULT_IDLE, [&, capture] {
    source.close ();
    g_assert_cmpint (destroyed, ==, 0);
    g_assert_true (capture != nullptr);
    return false;
  });
  capture.reset ();
  g_assert_true (g_main_context_iteration (context, FALSE));
  g_assert_cmpint (destroyed, ==, 1);
  g_assert_false (g_main_context_iteration (context, FALSE));
  g_main_context_unref (context);
}
void source_exception ()
{
  auto *context = g_main_context_new ();
  auto source = Source::idle (context, G_PRIORITY_DEFAULT_IDLE, [] () -> bool { throw std::runtime_error ("injected"); });
  g_test_expect_message (nullptr, G_LOG_LEVEL_WARNING, "*Painter source callback failed: injected*");
  g_assert_true (g_main_context_iteration (context, FALSE));
  g_test_assert_expected_messages ();
  g_assert_false (source.active ());
  g_main_context_unref (context);
}
}

void painter_test_register_resources ()
{
  g_test_add_func ("/painter/resources/value-assignment", values);
  g_test_add_func ("/painter/resources/array-assignment", arrays);
  g_test_add_func ("/painter/resources/error-message-transfer", errors);
  g_test_add_func ("/painter/resources/mutex-exception", mutex_guard);
  g_test_add_func ("/painter/signal/lifetime-blocks", connections);
  g_test_add_func ("/painter/signal/after-order", signal_order);
  g_test_add_func ("/painter/signal/reentrant-disconnect", reentrant_disconnect);
  g_test_add_func ("/painter/source/cancel-repeat", sources);
  g_test_add_func ("/painter/source/self-close", source_self_close);
  g_test_add_func ("/painter/source/exception", source_exception);
}
