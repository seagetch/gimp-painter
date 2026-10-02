/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "fair-dispatcher.hpp"
#include <array>
#include <stdexcept>
#include <vector>
using namespace GimpPainter;
struct Context
{
  GMainContext *get = g_main_context_new ();
  ~Context () { g_main_context_unref (get); }
  void drain () { for (unsigned n = 0; g_main_context_pending (get) && n < 1000; ++n) g_main_context_iteration (get, FALSE); }
};
static void fifo_one_quantum ()
{
  Context context; FairDispatcher dispatcher (context.get,0);
  std::array<unsigned,5> counts {{0,0,0,0,0}};
  std::vector<unsigned> order;
  std::vector<FairDispatcher::Ticket> tickets;
  for (unsigned i = 0; i < counts.size (); ++i)
    {
      tickets.push_back (dispatcher.ticket ([&,i] { order.push_back (i); return ++counts[i] < 3; }));
      tickets.back ().schedule (); tickets.back ().schedule ();
    }
  for (unsigned i = 0; i < 15; ++i)
    {
      g_assert_true (g_main_context_iteration (context.get,FALSE));
      g_assert_cmpuint (order.size (), ==, i+1);
      g_assert_cmpuint (order.back (), ==, i%5);
    }
  g_assert_false (g_main_context_pending (context.get));
}
static void cancel_during_callback ()
{
  Context context; FairDispatcher dispatcher (context.get,0);
  FairDispatcher::Ticket first, second;
  unsigned first_count = 0, second_count = 0;
  first = dispatcher.ticket ([&] { ++first_count; first.close (); second.close (); return true; });
  second = dispatcher.ticket ([&] { ++second_count; return false; });
  first.schedule (); second.schedule (); context.drain ();
  g_assert_cmpuint (first_count, ==, 1); g_assert_cmpuint (second_count, ==, 0);
  g_assert_false (g_main_context_pending (context.get));
}
static void schedule_during_callback ()
{
  Context context; FairDispatcher dispatcher (context.get,0);
  FairDispatcher::Ticket ticket;
  unsigned count = 0;
  ticket = dispatcher.ticket ([&] { if (++count < 3) ticket.schedule (); return count < 3; });
  ticket.schedule (); context.drain ();
  g_assert_cmpuint (count, ==, 3);
}
static void dispatcher_close_invalidates_tickets ()
{
  Context context; FairDispatcher::Ticket ticket;
  unsigned calls = 0;
  {
    FairDispatcher dispatcher (context.get,0);
    ticket = dispatcher.ticket ([&] { ++calls; return true; }); ticket.schedule ();
  }
  g_assert_false (ticket.valid ()); ticket.schedule (); context.drain ();
  g_assert_cmpuint (calls, ==, 0);
}
static void exception_does_not_stall_others ()
{
  Context context; FairDispatcher dispatcher (context.get,0);
  unsigned calls = 0;
  auto broken = dispatcher.ticket ([] () -> bool { throw std::runtime_error ("expected"); });
  auto other = dispatcher.ticket ([&] { ++calls; return false; });
  broken.schedule (); other.schedule ();
  g_test_expect_message (nullptr,G_LOG_LEVEL_WARNING,"*Painter dispatch failed: expected*");
  context.drain (); g_test_assert_expected_messages ();
  g_assert_cmpuint (calls, ==, 1); g_assert_false (broken.valid ());
}

static void input_preempts_next_quantum ()
{
  Context context; FairDispatcher dispatcher (context.get,0);
  std::vector<int> order;
  Source input;
  auto first = dispatcher.ticket ([&] {
    order.push_back (1);
    input = Source::idle (context.get,G_PRIORITY_DEFAULT,[&] { order.push_back (99); return false; });
    return false;
  });
  auto second = dispatcher.ticket ([&] { order.push_back (2); return false; });
  first.schedule (); second.schedule ();
  g_main_context_iteration (context.get,FALSE);
  g_assert_cmpuint (order.size (), ==, 1);
  g_main_context_iteration (context.get,FALSE);
  g_assert_cmpuint (order.size (), ==, 2); g_assert_cmpint (order.back (), ==, 99);
  context.drain (); g_assert_cmpint (order.back (), ==, 2);
}


static void cancel_then_schedule_replaces_source ()
{
  Context context; FairDispatcher dispatcher (context.get,0);
  FairDispatcher::Ticket first, second;
  unsigned count = 0;
  first = dispatcher.ticket ([&] { first.close (); second.schedule (); return true; });
  second = dispatcher.ticket ([&] { ++count; return false; });
  first.schedule (); context.drain ();
  g_assert_cmpuint (count, ==, 1); g_assert_false (g_main_context_pending (context.get));
}


static void capture_destruction_can_replace_ticket ()
{
  Context context; FairDispatcher dispatcher (context.get,0);
  FairDispatcher::Ticket ticket;
  unsigned replacement_calls = 0, outer_calls = 0;
  struct Replace
  {
    Replace (FairDispatcher::Ticket& ticket, FairDispatcher& dispatcher, unsigned& count)
      : ticket (ticket), dispatcher (dispatcher), count (count) {}
    ~Replace ()
    {
      auto *counter = &count;
      ticket = dispatcher.ticket ([counter] { ++*counter; return false; });
    }
    FairDispatcher::Ticket& ticket;
    FairDispatcher& dispatcher;
    unsigned& count;
  };
  auto capture = std::make_shared<Replace> (ticket,dispatcher,replacement_calls);
  ticket = dispatcher.ticket ([capture] { return false; });
  capture.reset ();
  ticket = dispatcher.ticket ([&] { ++outer_calls; return false; });
  ticket.schedule (); context.drain ();
  g_assert_cmpuint (replacement_calls, ==, 1); g_assert_cmpuint (outer_calls, ==, 0);
}

int main (int argc, char **argv)
{
  g_test_init (&argc,&argv,nullptr);
#define ADD(name) g_test_add_func ("/painter-fair-dispatcher/" #name,name)
  ADD (capture_destruction_can_replace_ticket); ADD (cancel_then_schedule_replaces_source); ADD (input_preempts_next_quantum); ADD (fifo_one_quantum); ADD (cancel_during_callback); ADD (schedule_during_callback);
  ADD (dispatcher_close_invalidates_tickets); ADD (exception_does_not_stall_others);
  return g_test_run ();
}
