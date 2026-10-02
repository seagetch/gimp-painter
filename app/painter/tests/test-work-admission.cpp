/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "work-admission.hpp"
#include <glib.h>
#include <atomic>
#include <memory>
#include <thread>
#include <vector>
using namespace GimpPainter;
static void fifo_and_jobs ()
{
  WorkAdmission pool ({2,100});
  auto a = pool.request (20), b = pool.request (20), c = pool.request (20);
  g_assert_false (bool (b.try_acquire ()));
  auto la = a.try_acquire (), lb = b.try_acquire ();
  g_assert_true (bool (la)); g_assert_true (bool (lb));
  g_assert_false (bool (c.try_acquire ()));
  g_assert_cmpuint (pool.active_jobs (), ==, 2); g_assert_cmpuint (pool.active_bytes (), ==, 40);
  la.close (); auto lc = c.try_acquire (); g_assert_true (bool (lc));
  lb.close (); lc.close (); g_assert_cmpuint (pool.active_jobs (), ==, 0); g_assert_cmpuint (pool.active_bytes (), ==, 0);
}
static void byte_limit_and_no_overtaking ()
{
  WorkAdmission pool ({4,100});
  auto a = pool.request (60); auto la = a.try_acquire ();
  auto b = pool.request (80), c = pool.request (10);
  g_assert_false (bool (b.try_acquire ()));
  g_assert_false (bool (c.try_acquire ())); // reserve FIFO priority, no starvation
  la.close (); auto lb = b.try_acquire (), lc = c.try_acquire ();
  g_assert_true (bool (lb)); g_assert_true (bool (lc));
  g_assert_cmpuint (pool.active_bytes (), ==, 90);
}
static void cancelled_requests ()
{
  WorkAdmission pool ({1,100});
  std::vector<WorkAdmission::Ticket> tickets;
  for (unsigned i = 0; i < 100; ++i) tickets.push_back (pool.request (10));
  for (auto& ticket : tickets) ticket.close ();
  auto last = pool.request (100); auto lease = last.try_acquire ();
  g_assert_true (bool (lease)); g_assert_cmpuint (pool.active_bytes (), ==, 100);
}
static void repeated_edits_do_not_accumulate_queue_nodes ()
{
  WorkAdmission pool ({1,100});
  auto first = pool.request (100); auto lease = first.try_acquire ();
  auto blocked = pool.request (100);
  for (unsigned i = 0; i < 100000; ++i)
    { auto edited = pool.request (1); edited.close (); }
  g_assert_cmpuint (pool.pending_requests (), ==, 1);
  lease.close (); auto next = blocked.try_acquire (); g_assert_true (bool (next));
  g_assert_cmpuint (pool.pending_requests (), ==, 0);
}
static void releases_on_worker ()
{
  WorkAdmission pool ({1,100});
  auto a = pool.request (100); auto lease = a.try_acquire ();
  auto b = pool.request (100);
  std::atomic<bool> release {false}, done {false};
  std::thread worker ([lease = std::move (lease), &release, &done] () mutable {
    while (!release.load ()) std::this_thread::yield ();
    lease.close (); done = true;
  });
  g_assert_false (bool (b.try_acquire ()));
  release = true;
  while (!done.load ()) std::this_thread::yield ();
  auto next = b.try_acquire (); g_assert_true (bool (next));
  worker.join ();
}
static void owner_can_close_without_waiting ()
{
  auto pool = std::unique_ptr<WorkAdmission> (new WorkAdmission ({1,100}));
  auto ticket = pool->request (100); auto lease = ticket.try_acquire ();
  auto waiting = pool->request (100);
  pool.reset (); waiting.close ();
  std::thread worker ([lease = std::move (lease)] () mutable { lease.close (); });
  worker.join ();
}
static void moves_release_exactly_once ()
{
  WorkAdmission pool ({2,100});
  auto a = pool.request (30), b = pool.request (40);
  auto la = a.try_acquire (), lb = b.try_acquire ();
  la = std::move (lb);
  g_assert_cmpuint (pool.active_jobs (), ==, 1); g_assert_cmpuint (pool.active_bytes (), ==, 40);
  auto *same = &la; la = std::move (*same); la.close (); la.close ();
  g_assert_cmpuint (pool.active_jobs (), ==, 0);
  auto c = pool.request (10), d = pool.request (20);
  c = std::move (d); auto lc = c.try_acquire ();
  g_assert_true (bool (lc)); g_assert_cmpuint (pool.active_bytes (), ==, 20);
}
static void invalid_limits_and_requests ()
{
  for (auto limits : {WorkAdmission::Limits {0,1}, WorkAdmission::Limits {1,0}})
    { bool caught = false; try { WorkAdmission pool (limits); } catch (const std::invalid_argument&) { caught = true; } g_assert_true (caught); }
  WorkAdmission pool ({1,100});
  for (auto bytes : {0u,101u})
    { bool caught = false; try { pool.request (bytes); } catch (const std::invalid_argument&) { caught = true; } g_assert_true (caught); }
  g_assert_cmpuint (pool.active_bytes (), ==, 0);
}
static void wrong_thread_is_rejected ()
{
  WorkAdmission pool ({1,100}); auto ticket = pool.request (10);
  bool caught_request = false, caught_acquire = false;
  std::thread worker ([&] {
    try { pool.request (10); } catch (const std::logic_error&) { caught_request = true; }
    try { ticket.try_acquire (); } catch (const std::logic_error&) { caught_acquire = true; }
  }); worker.join ();
  g_assert_true (caught_request); g_assert_true (caught_acquire);
  auto lease = ticket.try_acquire (); g_assert_true (bool (lease));
}
static void spill_limits_and_fifo ()
{
  WorkAdmission pool ({4,100,1000});
  auto a = pool.request (10,700); auto lease = a.try_acquire ();
  auto b = pool.request (10,400), c = pool.request (10,100);
  g_assert_false (bool (b.try_acquire ()));g_assert_false (bool (c.try_acquire ()));
  g_assert_cmpuint (pool.active_spill_bytes (), ==, 700);
  lease.close ();auto lb = b.try_acquire (), lc = c.try_acquire ();
  g_assert_true (bool (lb));g_assert_true (bool (lc));g_assert_cmpuint (pool.active_spill_bytes (), ==, 500);
  lb = std::move (lc);g_assert_cmpuint (pool.active_spill_bytes (), ==, 100);
  lb.close ();g_assert_cmpuint (pool.active_spill_bytes (), ==, 0);
}
static void lowered_limits_preserve_live_leases ()
{
  WorkAdmission pool ({2,100,1000});auto a=pool.request(80,800);auto lease=a.try_acquire();
  auto waiting=pool.request(10,100);pool.configure({2,30,300});
  g_assert_false(bool(waiting.try_acquire()));
  g_assert_cmpuint(pool.active_bytes(),==,80);g_assert_cmpuint(pool.active_spill_bytes(),==,800);
  std::thread worker([lease=std::move(lease)]()mutable{lease.close();});worker.join();
  auto next=waiting.try_acquire();g_assert_true(bool(next));
  auto oversized=pool.request(10,200);pool.configure({2,30,100});
  bool rejected=false;try{oversized.try_acquire();}catch(const std::invalid_argument&){rejected=true;}
  g_assert_true(rejected);g_assert_cmpuint(pool.pending_requests(),==,0);
  pool.configure({2,30,0});g_assert_cmpuint(pool.active_spill_bytes(),==,100);next.close();
  auto memory_only=pool.request(10);g_assert_true(bool(memory_only.try_acquire()));
  rejected=false;try{pool.request(1,1);}catch(const std::invalid_argument&){rejected=true;}g_assert_true(rejected);
}
static void spill_uint64_accounting ()
{
  const auto maximum=std::numeric_limits<std::uint64_t>::max();
  WorkAdmission pool({2,100,maximum});auto a=pool.request(10,maximum-1);auto lease=a.try_acquire();
  auto b=pool.request(10,2);g_assert_false(bool(b.try_acquire()));lease.close();
  auto next=b.try_acquire();g_assert_true(bool(next));g_assert_cmpuint(pool.active_spill_bytes(),==,2);
}
static void close_keeps_only_existing_reservations ()
{
  WorkAdmission pool({2,100,1000});auto a=pool.request(10,800);auto lease=a.try_acquire();auto b=pool.request(20,100);
  pool.close();g_assert_true(pool.closed());g_assert_cmpuint(pool.active_spill_bytes(),==,800);
  bool rejected=false;try{b.try_acquire();}catch(const std::runtime_error&){rejected=true;}g_assert_true(rejected);
  g_assert_cmpuint(pool.pending_requests(),==,0);rejected=false;
  try{pool.request(1);}catch(const std::runtime_error&){rejected=true;}g_assert_true(rejected);
  lease.close();g_assert_cmpuint(pool.active_spill_bytes(),==,0);
}
int main (int argc, char **argv)
{
  g_test_init (&argc,&argv,nullptr);
#define ADD(name) g_test_add_func ("/work-admission/" #name,name)
  ADD (fifo_and_jobs); ADD (byte_limit_and_no_overtaking); ADD (cancelled_requests);
  ADD (repeated_edits_do_not_accumulate_queue_nodes); ADD (releases_on_worker); ADD (owner_can_close_without_waiting); ADD (moves_release_exactly_once);
  ADD (invalid_limits_and_requests); ADD (wrong_thread_is_rejected);
  ADD (spill_limits_and_fifo);ADD (lowered_limits_preserve_live_leases);ADD (spill_uint64_accounting);
  ADD (close_keeps_only_existing_reservations);
  return g_test_run ();
}
