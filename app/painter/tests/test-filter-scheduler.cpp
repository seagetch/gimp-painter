/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "filter-scheduler.hpp"
#include <glib.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <memory>
#include <stdexcept>
#include <thread>
using namespace GimpPainter;
using State = FilterScheduler::State;
struct Harness
{
  FilterScheduler scheduler;
  FilterScheduler::Bytes imported;
  unsigned reads = 0, imports = 0, commits = 0;
  std::size_t max_chunk = 0;
  std::uint8_t value = 17;
  bool ready = true;
  void configure (std::size_t w = 32, std::size_t h = 32, FilterScheduler::Process process = {})
  {
    if (!process) process = [] (const FilterScheduler::Bytes& in, std::atomic<bool>& cancel, FilterScheduler::Bytes& out) {
      if (cancel.load ()) return false;
      out = in; return true;
    };
    scheduler.set_request ({w, h, std::move (process)});
  }
  bool step ()
  {
    return scheduler.step (ready,
      [&] (std::size_t, std::size_t count, FilterScheduler::Bytes& out) {
        ++reads; max_chunk = std::max (max_chunk, count); out.insert (out.end (), count * 4, value);
      },
      [&] (std::size_t offset, std::size_t count, const std::uint8_t *pixels) {
        ++imports; max_chunk = std::max (max_chunk, count);
        if (!offset) imported.clear ();
        imported.insert (imported.end (), pixels, pixels + count * 4);
      },
      [&] (std::uint64_t token) { ++commits; g_assert_cmpuint (token, ==, scheduler.generation ()); });
  }
  void finish ()
  {
    auto deadline = g_get_monotonic_time () + G_TIME_SPAN_SECOND * 10;
    do { step (); if (scheduler.settled ()) return; g_usleep (100); } while (g_get_monotonic_time () < deadline);
    g_error ("Scheduler did not converge");
  }
};
static void completed_cache ()
{
  Harness h; h.configure (); h.finish ();
  g_assert_cmpuint (h.commits, ==, 1); g_assert_cmpuint (h.scheduler.starts (), ==, 1);
  g_assert_cmpuint (h.scheduler.cache_generation (), ==, h.scheduler.generation ());
  for (auto c : h.imported) g_assert_cmpuint (c, ==, 17);
  for (int i = 0; i < 100; ++i) g_assert_false (h.step ());
  g_assert_cmpuint (h.scheduler.starts (), ==, 1);
}
static void bounded_chunks ()
{
  for (auto width : { std::size_t (256), std::size_t (40001) })
    {
      Harness h; h.configure (width, 257); h.finish ();
      g_assert_cmpuint (h.max_chunk, <=, FilterScheduler::pixel_budget);
      g_assert_cmpuint (h.reads, >, 1); g_assert_cmpuint (h.imports, ==, h.reads);
      g_assert_cmpuint (h.imported.size (), ==, width * 257 * 4);
    }
}
static void dependency_priority ()
{
  Harness low, high; low.configure (); high.configure ();
  high.ready = false;
  g_assert_false (high.step ()); g_assert_cmpuint (high.scheduler.starts (), ==, 0);
  low.finish (); high.ready = low.scheduler.state () == State::clean; high.finish ();
  g_assert_cmpuint (high.scheduler.starts (), ==, 1);
}
static void cancel_request_is_not_completion ()
{
  struct Gate { std::atomic<bool> started {false}, release {false}, ended {false}; };
  auto gate = std::make_shared<Gate> ();
  Harness h; h.configure (32, 32, [gate] (const FilterScheduler::Bytes& in, std::atomic<bool>& cancel, FilterScheduler::Bytes& out) {
    gate->started = true; while (!gate->release.load ()) std::this_thread::yield ();
    out = in; gate->ended = true; return !cancel.load ();
  });
  h.step (); while (!gate->started.load ()) std::this_thread::yield ();
  h.scheduler.invalidate ();
  g_assert_true (h.scheduler.state () == State::cancelling);
  for (int i = 0; i < 100; ++i) h.step ();
  g_assert_cmpuint (h.scheduler.starts (), ==, 1); g_assert_cmpuint (h.commits, ==, 0);
  h.value = 93; gate->release = true; h.finish ();
  g_assert_true (gate->ended); g_assert_cmpuint (h.scheduler.starts (), ==, 2);
  g_assert_cmpuint (h.commits, ==, 1);
  for (auto c : h.imported) g_assert_cmpuint (c, ==, 93);
}
static void changes_during_preparation ()
{
  Harness h; h.configure (256, 257); h.step (); h.value = 45; h.scheduler.invalidate (); h.finish ();
  g_assert_cmpuint (h.scheduler.starts (), ==, 1);
  for (auto c : h.imported) g_assert_cmpuint (c, ==, 45);
}
static void changes_during_import ()
{
  Harness h; h.configure (256, 257);
  while (h.imports == 0) { h.step (); g_usleep (100); }
  g_assert_cmpuint (h.commits, ==, 0);
  h.value = 67; h.scheduler.invalidate (); h.finish ();
  g_assert_cmpuint (h.scheduler.starts (), ==, 2); g_assert_cmpuint (h.commits, ==, 1);
  for (auto c : h.imported) g_assert_cmpuint (c, ==, 67);
}
static void failures_do_not_retry ()
{
  Harness h; h.configure (32, 32, [] (const FilterScheduler::Bytes&, std::atomic<bool>&, FilterScheduler::Bytes&) -> bool {
    throw std::runtime_error ("expected failure");
  });
  h.finish (); g_assert_true (h.scheduler.state () == State::failed);
  for (int i = 0; i < 100; ++i) g_assert_false (h.step ());
  g_assert_cmpuint (h.scheduler.starts (), ==, 1); g_assert_cmpuint (h.commits, ==, 0);
  g_assert_cmpstr (h.scheduler.error ().c_str (), ==, "expected failure");
  h.configure (); h.finish (); g_assert_cmpuint (h.commits, ==, 1);
}
static void close_does_not_wait ()
{
  auto release = std::make_shared<std::atomic<bool>> (false);
  auto ended = std::make_shared<std::atomic<bool>> (false);
  auto h = std::unique_ptr<Harness> (new Harness);
  h->configure (1, 1, [release, ended] (const FilterScheduler::Bytes&, std::atomic<bool>&, FilterScheduler::Bytes&) {
    while (!release->load ()) std::this_thread::yield ();
    ended->store (true); return false;
  });
  h->step ();
  const auto start = g_get_monotonic_time (); h.reset ();
  g_assert_cmpint (g_get_monotonic_time () - start, <, 100000);
  release->store (true);
  while (!ended->load ()) std::this_thread::yield ();
}
static void loaded_cache_not_reexecuted ()
{
  Harness h; h.configure (); h.scheduler.mark_loaded ();
  for (int i = 0; i < 100; ++i) g_assert_false (h.step ());
  g_assert_cmpuint (h.scheduler.starts (), ==, 0);
  g_assert_cmpuint (h.scheduler.cache_generation (), ==, h.scheduler.generation ());
  h.scheduler.invalidate (); h.finish (); g_assert_cmpuint (h.commits, ==, 1);
}
static void invalid_request_and_result ()
{
  Harness h; h.scheduler.set_request ({2, 2, {}}); h.finish ();
  g_assert_true (h.scheduler.state () == State::failed);
  h.configure (FilterScheduler::maximum_pixels, 2); h.finish ();
  g_assert_cmpuint (h.reads, ==, 0);
  h.configure (2, 2, [] (const FilterScheduler::Bytes&, std::atomic<bool>&, FilterScheduler::Bytes&) { return true; });
  h.finish (); g_assert_true (h.scheduler.state () == State::failed); g_assert_cmpuint (h.commits, ==, 0);
}
static void reentry_at_commit ()
{
  FilterScheduler s; unsigned commits = 0;
  s.set_request ({1, 1, [] (const FilterScheduler::Bytes& in, std::atomic<bool>&, FilterScheduler::Bytes& out) { out = in; return true; }});
  auto deadline = g_get_monotonic_time () + G_TIME_SPAN_SECOND * 5;
  while (commits < 2 && g_get_monotonic_time () < deadline)
    {
      s.step (true, [] (std::size_t, std::size_t n, FilterScheduler::Bytes& out) { out.insert (out.end (), n * 4, 0); },
              [] (std::size_t, std::size_t, const std::uint8_t *) {},
              [&] (std::uint64_t) { if (++commits == 1) s.invalidate (); });
      g_usleep (100);
    }
  g_assert_cmpuint (commits, ==, 2); g_assert_cmpuint (s.starts (), ==, 2);
}

static void reject_during_read ()
{
  FilterScheduler s;
  unsigned starts = 0, commits = 0;
  s.set_request ({1,1,[&] (const FilterScheduler::Bytes&, std::atomic<bool>&, FilterScheduler::Bytes&) {
    ++starts; return true;
  }});
  g_assert_false (s.step (true,
    [&] (std::size_t, std::size_t, FilterScheduler::Bytes& out) { out.resize (4); s.reject ("read rejected"); },
    [] (std::size_t, std::size_t, const std::uint8_t *) {},
    [&] (std::uint64_t) { ++commits; }));
  for (int i = 0; i < 10; ++i) g_assert_false (s.step (true, {}, {}, {}));
  g_assert_true (s.state () == State::failed);
  g_assert_cmpuint (starts, ==, 0); g_assert_cmpuint (commits, ==, 0);
  g_assert_cmpstr (s.error ().c_str (), ==, "read rejected");
}
static void reject_during_import ()
{
  FilterScheduler s;
  unsigned imports = 0, commits = 0;
  s.set_request ({1,1,[] (const FilterScheduler::Bytes& in, std::atomic<bool>&, FilterScheduler::Bytes& out) { out = in; return true; }});
  const auto deadline = g_get_monotonic_time () + G_TIME_SPAN_SECOND * 5;
  while (s.state () != State::failed && g_get_monotonic_time () < deadline)
    {
      s.step (true,
        [] (std::size_t, std::size_t, FilterScheduler::Bytes& out) { out.resize (4); },
        [&] (std::size_t, std::size_t, const std::uint8_t *) { ++imports; s.reject ("import rejected"); },
        [&] (std::uint64_t) { ++commits; });
      g_usleep (100);
    }
  for (int i = 0; i < 10; ++i) s.step (true, {}, {}, {});
  g_assert_true (s.state () == State::failed); g_assert_cmpuint (imports, ==, 1);
  g_assert_cmpuint (commits, ==, 0); g_assert_false (s.has_worker ());
}
static void closed_request_is_inert ()
{
  FilterScheduler s;
  g_assert_cmpuint (s.generation (), ==, s.cache_generation ());
  s.close ();
  const auto generation = s.generation ();
  s.set_request ({1,1,[] (const FilterScheduler::Bytes&, std::atomic<bool>&, FilterScheduler::Bytes&) { g_error ("closed worker ran"); return true; }});
  s.invalidate (); s.mark_loaded (); s.reject ("ignored");
  g_assert_false (s.step (true, {}, {}, {}));
  g_assert_true (s.state () == State::closed); g_assert_cmpuint (s.generation (), ==, generation);
  g_assert_cmpuint (s.starts (), ==, 0);
}

int main (int argc, char **argv)
{
  g_test_init (&argc, &argv, nullptr);
#define ADD(name) g_test_add_func ("/painter-filter-scheduler/" #name, name)
  ADD (reject_during_read); ADD (reject_during_import); ADD (closed_request_is_inert);
  ADD (completed_cache); ADD (bounded_chunks); ADD (dependency_priority);
  ADD (cancel_request_is_not_completion); ADD (changes_during_preparation); ADD (changes_during_import);
  ADD (failures_do_not_retry); ADD (close_does_not_wait); ADD (loaded_cache_not_reexecuted);
  ADD (invalid_request_and_result); ADD (reentry_at_commit);
  return g_test_run ();
}
