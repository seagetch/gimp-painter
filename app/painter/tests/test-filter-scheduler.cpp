/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "filter-scheduler.hpp"
#include <glib.h>
#include <glib/gstdio.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <memory>
#include <limits>
#include <stdexcept>
#include <thread>
using namespace GimpPainter;
using State = FilterScheduler::State;
static std::string spool_directory;
struct Harness
{
  Harness () = default;
  explicit Harness (std::shared_ptr<WorkAdmission> admission) : scheduler (std::move (admission)) {}
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
  void configure_spool (std::size_t w = 32, std::size_t h = 32, FilterSpool::Process process = {})
  {
    if (!process) process = [] (FilterRaster& in, FilterRaster& out, std::atomic<bool>& cancel, const FilterRasterFactory&) {
      FilterSpool::Bytes bytes (32768*4);
      for (std::uint64_t offset = 0; offset < in.size ();)
        {
          if (cancel.load ()) return false;
          const auto count = std::size_t (std::min (std::uint64_t (bytes.size ()),in.size ()-offset));
          in.read (offset,count,bytes.data ()); out.write (offset,count,bytes.data ()); offset += count;
        }
      return true;
    };
    FilterScheduler::Request request;
    request.width = w; request.height = h; request.peak_bytes = 1024*1024;
    request.raster_process = std::move (process); request.spool_directory = spool_directory;
    scheduler.set_request (std::move (request));
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
static void adjustable_chunk_budget ()
{
  for (auto budget : {std::size_t (0), std::size_t (1), std::size_t (37), std::size_t (65536)})
    {
      Harness h;
      h.configure (257,3); h.scheduler.set_pixel_budget (budget); h.finish ();
      const auto cap = std::min (std::max (budget,std::size_t (1)),std::size_t (FilterScheduler::pixel_budget));
      g_assert_cmpuint (h.max_chunk, <=, cap);
      g_assert_cmpuint (h.imported.size (), ==, 257 * 3 * 4);
      g_assert_cmpuint (h.scheduler.starts (), ==, 1); g_assert_cmpuint (h.commits, ==, 1);
    }
  FilterScheduler s;
  const std::size_t width = 40001, height = 3;
  std::size_t read_offset = 0, import_offset = 0, expected_cap = 17;
  unsigned reads = 0, imports = 0, commits = 0;
  s.set_request ({width,height,[] (const FilterScheduler::Bytes& input,std::atomic<bool>&,FilterScheduler::Bytes& output) {
    output = input; return true;
  }});
  s.set_pixel_budget (expected_cap);
  const auto generation = s.generation ();
  const auto deadline = g_get_monotonic_time () + G_TIME_SPAN_SECOND * 10;
  do
    {
      s.step (true,
        [&] (std::size_t offset,std::size_t count,FilterScheduler::Bytes& out) {
          g_assert_cmpuint (offset, ==, read_offset); g_assert_cmpuint (count, >, 0);
          g_assert_cmpuint (count, <=, expected_cap);
          g_assert_true (count <= width - offset % width || (offset % width == 0 && count % width == 0));
          out.insert (out.end (),count * 4,42); read_offset += count;
          expected_cap = ++reads % 2 ? 32768 : 17; s.set_pixel_budget (expected_cap);
        },
        [&] (std::size_t offset,std::size_t count,const std::uint8_t *bytes) {
          g_assert_cmpuint (offset, ==, import_offset); g_assert_cmpuint (count, >, 0);
          g_assert_cmpuint (count, <=, expected_cap);
          for (std::size_t i = 0; i < count * 4; ++i) g_assert_cmpuint (bytes[i], ==, 42);
          import_offset += count; expected_cap = ++imports % 2 ? 32768 : 17; s.set_pixel_budget (expected_cap);
        },
        [&] (std::uint64_t) { ++commits; });
      if (s.settled ()) break;
      g_usleep (100);
    }
  while (g_get_monotonic_time () < deadline);
  g_assert_true (s.settled ()); g_assert_cmpuint (commits, ==, 1);
  g_assert_cmpuint (s.generation (), ==, generation);
  g_assert_cmpuint (read_offset, ==, width * height); g_assert_cmpuint (import_offset, ==, width * height);
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


static void rejected_worker_is_not_settled_until_done ()
{
  auto release = std::make_shared<std::atomic<bool>> (false);
  Harness h;
  h.configure (1,1,[release] (const FilterScheduler::Bytes&, std::atomic<bool>&, FilterScheduler::Bytes&) {
    while (!release->load ()) std::this_thread::yield ();
    return false;
  });
  h.step (); h.scheduler.reject ("dependency failed");
  g_assert_true (h.scheduler.state () == State::failed);
  g_assert_true (h.scheduler.has_worker ()); g_assert_false (h.scheduler.settled ());
  for (int i = 0; i < 5; ++i) h.step ();
  g_assert_false (h.scheduler.settled ()); g_assert_cmpuint (h.commits, ==, 0);
  release->store (true); h.finish ();
  g_assert_true (h.scheduler.settled ()); g_assert_false (h.scheduler.has_worker ());
  g_assert_cmpuint (h.scheduler.starts (), ==, 1); g_assert_cmpuint (h.commits, ==, 0);
}


static void saved_cache_generations ()
{
  Harness h; h.configure ();
  h.scheduler.restore_cache ({42,41,true});
  const auto restored_generation = h.scheduler.generation ();
  g_assert_cmpuint (restored_generation, !=, 42);
  g_assert_true (h.scheduler.state () == State::waiting); h.finish ();
  g_assert_cmpuint (h.scheduler.generation (), ==, restored_generation);
  g_assert_cmpuint (h.scheduler.cache_generation (), ==, restored_generation);
  g_assert_cmpuint (h.commits, ==, 1);
  h.scheduler.restore_cache ({100,100,true});
  g_assert_false (h.step ()); g_assert_cmpuint (h.commits, ==, 1);
  h.scheduler.restore_cache ({100,100,false}); h.finish ();
  g_assert_cmpuint (h.commits, ==, 2); g_assert_true (h.scheduler.snapshot ().cache_complete);
  const auto valid_generation = h.scheduler.generation ();
  bool failed = false;
  try { h.scheduler.restore_cache ({2,3,true}); } catch (const std::invalid_argument&) { failed = true; }
  g_assert_true (failed); g_assert_cmpuint (h.scheduler.generation (), ==, valid_generation);
}
static void restored_cache_rejects_colliding_old_job ()
{
  auto release = std::make_shared<std::atomic<bool>> (false);
  Harness h; h.configure (1,1,[release] (const FilterScheduler::Bytes& in, std::atomic<bool>&, FilterScheduler::Bytes& out) {
    while (!release->load ()) std::this_thread::yield ();
    out = in; return true;
  });
  h.step ();
  const auto token = h.scheduler.generation ();
  h.scheduler.restore_cache ({token,token,true});
  g_assert_cmpuint (h.scheduler.generation (), !=, token);
  release->store (true); h.finish ();
  g_assert_cmpuint (h.commits, ==, 0); g_assert_cmpuint (h.scheduler.starts (), ==, 1);
  g_assert_true (h.scheduler.state () == State::clean);
}


static void commit_failure_does_not_certify_cache ()
{
  FilterScheduler s; unsigned attempts = 0;
  s.set_request ({1,1,[] (const FilterScheduler::Bytes& in, std::atomic<bool>&, FilterScheduler::Bytes& out) { out = in; return true; }});
  const auto deadline = g_get_monotonic_time () + G_TIME_SPAN_SECOND * 5;
  while (!s.settled () && g_get_monotonic_time () < deadline)
    {
      s.step (true,[] (std::size_t, std::size_t, FilterScheduler::Bytes& out) { out.resize (4); },
        [] (std::size_t, std::size_t, const std::uint8_t *) {},
        [&] (std::uint64_t) { ++attempts; throw std::runtime_error ("publication failed"); });
      g_usleep (100);
    }
  g_assert_true (s.state () == State::failed); g_assert_cmpuint (attempts, ==, 1);
  g_assert_false (s.snapshot ().cache_complete); g_assert_cmpuint (s.cache_generation (), ==, 0);
  for (int i = 0; i < 5; ++i) g_assert_false (s.step (true, {}, {}, {}));
}
static void obsolete_read_failure_preserves_new_edit ()
{
  Harness h; h.configure (1,1);
  g_assert_true (h.scheduler.step (true,
    [&] (std::size_t, std::size_t, FilterScheduler::Bytes&) { h.scheduler.invalidate (); throw std::runtime_error ("obsolete read failed"); },
    {}, {}));
  g_assert_true (h.scheduler.state () == State::waiting);
  h.value = 97; h.finish ();
  g_assert_cmpuint (h.commits, ==, 1);
  for (auto value : h.imported) g_assert_cmpuint (value, ==, 97);
}


static void saved_generation_boundaries ()
{
  Harness h; h.configure (1,1);
  h.scheduler.restore_cache ({0,0,true});
  g_assert_true (h.scheduler.settled ());
  g_assert_cmpuint (h.scheduler.generation (), ==, h.scheduler.cache_generation ());
  h.scheduler.invalidate ();
  const auto previous = h.scheduler.generation ();
  bool rejected = false;
  try { h.scheduler.restore_cache ({0,1,true}); } catch (const std::invalid_argument&) { rejected = true; }
  g_assert_true (rejected); g_assert_cmpuint (h.scheduler.generation (), ==, previous);
  rejected = false;
  try { h.scheduler.restore_cache ({std::numeric_limits<std::uint64_t>::max (),0,true}); }
  catch (const std::invalid_argument&) { rejected = true; }
  g_assert_true (rejected); g_assert_cmpuint (h.scheduler.generation (), ==, previous);
  h.scheduler.invalidate (); g_assert_cmpuint (h.scheduler.generation (), ==, previous + 1);
  h.finish ();
  const auto maximum = std::uint64_t (std::numeric_limits<std::int64_t>::max ());
  h.scheduler.restore_cache ({maximum,maximum,true});
  g_assert_cmpuint (h.scheduler.generation (), <, maximum);
  h.scheduler.invalidate ();
  const auto saved = h.scheduler.snapshot ();
  g_assert_cmpuint (saved.generation, !=, saved.cache_generation);
  Harness reopened; reopened.configure (1,1); reopened.scheduler.restore_cache (saved);
  g_assert_true (reopened.scheduler.state () == State::waiting);
  reopened.finish ();
  g_assert_cmpuint (reopened.scheduler.generation (), ==, reopened.scheduler.cache_generation ());
}

static void admission_waits_without_reading_and_resumes_fifo ()
{
  auto pool = std::make_shared<WorkAdmission> (WorkAdmission::Limits {1,1024});
  auto release = std::make_shared<std::atomic<bool>> (false);
  Harness first (pool), second (pool), third (pool);
  first.configure (1,1,[release] (const FilterScheduler::Bytes& in,std::atomic<bool>&,FilterScheduler::Bytes& out) {
    while (!release->load ()) std::this_thread::yield ();
    out = in; return true;
  });
  second.configure (1,1); third.configure (1,1);
  first.step (); second.step (); third.step ();
  g_assert_cmpuint (pool->active_jobs (), ==, 1);
  g_assert_cmpuint (second.reads, ==, 0); g_assert_cmpuint (third.reads, ==, 0);
  g_assert_true (second.scheduler.state () == State::waiting);
  g_assert_cmpstr (second.scheduler.error ().c_str (), ==, "");
  release->store (true); first.finish ();
  third.step (); g_assert_cmpuint (third.reads, ==, 0);
  second.finish (); third.finish ();
  g_assert_cmpuint (second.commits, ==, 1); g_assert_cmpuint (third.commits, ==, 1);
  g_assert_cmpuint (pool->active_jobs (), ==, 0); g_assert_cmpuint (pool->active_bytes (), ==, 0);
}
static void admission_cancel_retains_worker_reservation ()
{
  auto pool = std::make_shared<WorkAdmission> (WorkAdmission::Limits {1,1024});
  auto release = std::make_shared<std::atomic<bool>> (false);
  auto ended = std::make_shared<std::atomic<bool>> (false);
  auto first = std::unique_ptr<Harness> (new Harness (pool));
  first->configure (1,1,[release,ended] (const FilterScheduler::Bytes&,std::atomic<bool>&,FilterScheduler::Bytes&) {
    while (!release->load ()) std::this_thread::yield ();
    ended->store (true); return false;
  });
  first->step ();
  Harness second (pool); second.configure (1,1); second.step ();
  first.reset (); // must not wait or release running bytes too soon
  for (unsigned i = 0; i < 100; ++i) second.step ();
  g_assert_cmpuint (second.reads, ==, 0); g_assert_cmpuint (pool->active_jobs (), ==, 1);
  release->store (true); second.finish ();
  g_assert_true (ended->load ()); g_assert_cmpuint (second.commits, ==, 1);
}
static void admission_edit_and_dependency_wait_release_preparation ()
{
  auto pool = std::make_shared<WorkAdmission> (WorkAdmission::Limits {1,32768});
  Harness first (pool), second (pool);
  first.configure (16,16); first.scheduler.set_pixel_budget (1);
  first.step (); g_assert_true (first.scheduler.state () == State::preparing);
  second.configure (1,1); second.step (); g_assert_cmpuint (second.reads, ==, 0);
  first.scheduler.invalidate (); // allocation and lease released on edit
  g_assert_cmpuint (pool->active_jobs (), ==, 0);
  first.step (); g_assert_true (first.scheduler.state () == State::waiting); // second owns FIFO position
  second.finish ();
  first.step (); g_assert_true (first.scheduler.state () == State::preparing);
  first.ready = false; first.step ();
  g_assert_cmpuint (pool->active_jobs (), ==, 0);
  first.ready = true; first.finish (); g_assert_cmpuint (first.commits, ==, 1);
}
static void admission_failure_and_loaded_cache_release ()
{
  auto pool = std::make_shared<WorkAdmission> (WorkAdmission::Limits {1,1024});
  Harness failed (pool), next (pool);
  failed.configure (1,1,[] (const FilterScheduler::Bytes&,std::atomic<bool>&,FilterScheduler::Bytes&) -> bool {
    throw std::runtime_error ("expected worker failure");
  });
  failed.finish (); g_assert_true (failed.scheduler.state () == State::failed);
  g_assert_cmpuint (pool->active_jobs (), ==, 0);
  next.configure (4,4); next.scheduler.set_pixel_budget (1); next.step ();
  g_assert_cmpuint (pool->active_jobs (), ==, 1);
  next.scheduler.mark_loaded (); g_assert_cmpuint (pool->active_jobs (), ==, 0);
  next.scheduler.invalidate (); next.finish (); g_assert_cmpuint (next.commits, ==, 1);
  next.scheduler.set_request ({1,1,{},1025}); next.finish (); // unsupported proc fails before admission
  next.configure (16,16); next.finish (); // minimum working bytes cannot fit
  g_assert_true (next.scheduler.state () == State::failed);
  g_assert_nonnull (strstr (next.scheduler.error ().c_str (),"admission memory limit"));
  g_assert_cmpuint (pool->active_jobs (), ==, 0);
}

static void spool_completed_cache_and_chunk_geometry ()
{
  for (auto budget : {std::size_t (17), std::size_t (32768)})
    {
      Harness h; h.configure_spool (40001,3); h.scheduler.set_pixel_budget (budget); h.finish ();
      g_assert_cmpuint (h.commits, ==, 1); g_assert_cmpuint (h.scheduler.starts (), ==, 1);
      g_assert_cmpuint (h.max_chunk, <=, budget); g_assert_cmpuint (h.imported.size (), ==, 40001*3*4);
      for (auto byte : h.imported) g_assert_cmpuint (byte, ==, 17);
    }
}
static void spool_reentrant_read_cancellation ()
{
  Harness h; h.configure_spool (256,257);
  h.scheduler.step (true,[&] (std::size_t,std::size_t count,FilterScheduler::Bytes& bytes) {
    bytes.resize (count*4,17); auto *owned = bytes.data (); h.scheduler.invalidate ();
    owned[0] = 42; // independent read chunk must stay alive across invalidation
  },{},{ });
  h.value = 93; h.finish ();
  g_assert_cmpuint (h.commits, ==, 1); g_assert_cmpuint (h.scheduler.starts (), ==, 1);
  for (auto byte : h.imported) g_assert_cmpuint (byte, ==, 93);
}
static void spool_cancel_request_is_not_completion ()
{
  auto started = std::make_shared<std::atomic<bool>> (false), release = std::make_shared<std::atomic<bool>> (false);
  Harness h; h.configure_spool (1,1,[started,release] (FilterRaster& in,FilterRaster& out,std::atomic<bool>& cancel,const FilterRasterFactory&) {
    started->store (true);
    while (!release->load ()) std::this_thread::yield ();
    if (cancel.load ()) return false;
    std::uint8_t pixel[4]; in.read (0,4,pixel); out.write (0,4,pixel); return true;
  });
  const auto deadline = g_get_monotonic_time () + 10 * G_TIME_SPAN_SECOND;
  while (h.scheduler.state () != State::running && g_get_monotonic_time () < deadline) { h.step (); g_usleep (100); }
  g_assert_true (started->load ()); g_assert_true (h.scheduler.state () == State::running);
  h.scheduler.invalidate ();
  for (unsigned i = 0; i < 100; ++i) h.step ();
  g_assert_cmpuint (h.scheduler.starts (), ==, 1); g_assert_cmpuint (h.commits, ==, 0);
  h.value = 67; release->store (true); h.finish ();
  g_assert_cmpuint (h.scheduler.starts (), ==, 2); g_assert_cmpuint (h.commits, ==, 1);
  for (auto byte : h.imported) g_assert_cmpuint (byte, ==, 67);
}
static void spool_change_during_import ()
{
  Harness h; h.configure_spool (256,257); h.scheduler.set_pixel_budget (256);
  const auto deadline = g_get_monotonic_time () + 10 * G_TIME_SPAN_SECOND;
  while (!h.imports && g_get_monotonic_time () < deadline) { h.step (); g_usleep (100); }
  g_assert_cmpuint (h.imports, >, 0); g_assert_cmpuint (h.commits, ==, 0);
  h.scheduler.invalidate (); h.value = 123; h.finish ();
  g_assert_cmpuint (h.scheduler.starts (), ==, 2); g_assert_cmpuint (h.commits, ==, 1);
  for (auto byte : h.imported) g_assert_cmpuint (byte, ==, 123);
}
static void spool_read_failure_cancels_unsealed_worker ()
{
  auto admission = std::make_shared<WorkAdmission> (WorkAdmission::Limits {1,1024*1024});
  Harness h (admission); h.configure_spool (2,2);
  bool again = h.scheduler.step (true,[] (std::size_t,std::size_t,FilterScheduler::Bytes&) {},{},{});
  // Follow the actual dispatch contract; do not hide a dropped continuation by
  // independently polling settled(). The retained collecting worker must drain
  // cancellation and release its admission without a new edit or owner close.
  g_assert_true (again);
  const auto deadline = g_get_monotonic_time () + 10 * G_TIME_SPAN_SECOND;
  while (again && g_get_monotonic_time () < deadline)
    { g_usleep (100); again = h.step (); }
  g_assert_false (again); g_assert_true (h.scheduler.settled ());
  g_assert_true (h.scheduler.state () == State::failed);
  g_assert_cmpuint (admission->active_jobs (), ==, 0);
  g_assert_cmpuint (admission->active_bytes (), ==, 0);
  g_assert_cmpuint (h.scheduler.starts (), ==, 0); g_assert_cmpuint (h.commits, ==, 0);
  g_assert_nonnull (strstr (h.scheduler.error ().c_str (),"invalid chunk size"));
  h.configure_spool (); h.finish (); g_assert_cmpuint (h.commits, ==, 1);
}
static void spool_result_failure_never_commits ()
{
  Harness h; h.configure (); h.finish ();
  const auto cache_generation = h.scheduler.cache_generation ();
  h.configure_spool (32768,4,[] (FilterRaster&,FilterRaster& out,std::atomic<bool>&,const FilterRasterFactory&) {
    FilterSpool::Bytes incomplete (32768*4,17); out.write (0,incomplete.size (),incomplete.data ()); return true;
  });
  h.finish (); g_assert_true (h.scheduler.state () == State::failed);
  g_assert_cmpuint (h.commits, ==, 1); g_assert_cmpuint (h.scheduler.cache_generation (), ==, cache_generation);
  g_assert_nonnull (strstr (h.scheduler.error ().c_str (),"read failed"));
  for (unsigned i = 0; i < 100; ++i) g_assert_false (h.step ());
}
static void spool_large_metadata_is_bounded_preparation ()
{
  Harness h; h.configure_spool (8193,8193); h.scheduler.set_pixel_budget (1); h.step ();
  g_assert_true (h.scheduler.state () == State::preparing); g_assert_cmpuint (h.max_chunk, ==, 1);
  g_assert_cmpuint (h.scheduler.starts (), ==, 0);
  h.scheduler.close (); g_assert_true (h.scheduler.state () == State::closed);
}
static void spool_close_during_import_keeps_callback_bytes ()
{
  Harness h; h.configure_spool (32,32);
  const auto deadline = g_get_monotonic_time () + 10 * G_TIME_SPAN_SECOND;
  bool closed = false;
  while (!closed && g_get_monotonic_time () < deadline)
    {
      h.scheduler.step (true,[] (std::size_t,std::size_t count,FilterScheduler::Bytes& bytes) { bytes.assign (count*4,93); },
        [&] (std::size_t,std::size_t count,const std::uint8_t *bytes) {
          h.scheduler.close (); closed = true;
          for (std::size_t i = 0; i < count*4; ++i) g_assert_cmpuint (bytes[i], ==, 93);
        },[] (std::uint64_t) { g_error ("closed spool published"); });
      g_usleep (100);
    }
  g_assert_true (closed);
}

static void recursive_step_does_not_consume_another_chunk ()
{
  for (bool spool : {false,true})
    {
      Harness h;
      if (spool) h.configure_spool (3,1); else h.configure (3,1);
      h.scheduler.set_pixel_budget (1);
      unsigned nested_reads = 0, commits = 0;
      FilterScheduler::Bytes observed;
      const auto deadline = g_get_monotonic_time () + 10 * G_TIME_SPAN_SECOND;
      while (!h.scheduler.settled () && g_get_monotonic_time () < deadline)
        {
          h.scheduler.step (true,[&] (std::size_t,std::size_t count,FilterScheduler::Bytes& bytes) {
            h.scheduler.step (true,[&] (std::size_t,std::size_t nested_count,FilterScheduler::Bytes& nested) {
              ++nested_reads; nested.assign (nested_count*4,99);
            },{},{});
            bytes.assign (count*4,17);
          },[&] (std::size_t,std::size_t count,const std::uint8_t *bytes) {
            observed.insert (observed.end (),bytes,bytes+count*4);
          },[&] (std::uint64_t) { ++commits; });
          g_usleep (100);
        }
      g_assert_true (h.scheduler.settled ()); g_assert_cmpuint (nested_reads, ==, 0);
      g_assert_cmpuint (commits, ==, 1); g_assert_cmpuint (observed.size (), ==, 12);
      for (auto byte : observed) g_assert_cmpuint (byte, ==, 17);
    }
}

static void admission_switch_keeps_restored_cache ()
{
  FilterScheduler scheduler;scheduler.mark_loaded();auto before=scheduler.snapshot();
  scheduler.set_admission(std::make_shared<WorkAdmission>(WorkAdmission::Limits{1,1024,0}));
  auto after=scheduler.snapshot();g_assert_cmpuint(after.generation,==,before.generation);
  g_assert_cmpuint(after.cache_generation,==,before.cache_generation);g_assert_true(after.cache_complete);
  g_assert_true(scheduler.state()==FilterScheduler::State::clean);
}
static void native_stride_vector_and_spool ()
{
  for (bool spill : {false,true}) for (std::size_t stride : {std::size_t (1),std::size_t (8),std::size_t (32)})
    {
      FilterScheduler scheduler;
      FilterScheduler::Request request;
      request.width=37; request.height=11; request.bytes_per_pixel=stride;
      request.process=[] (const FilterScheduler::Bytes& input,std::atomic<bool>&,FilterScheduler::Bytes& output) {output=input;return true;};
      if (spill) request.raster_process=[] (FilterRaster& input,FilterRaster& output,std::atomic<bool>&,const FilterRasterFactory&) {
        FilterScheduler::Bytes bytes(input.size ()); input.read (0,bytes.size (),bytes.data ()); output.write (0,bytes.size (),bytes.data ()); return true;
      };
      request.spool_directory=spool_directory;
      scheduler.set_request (request); scheduler.set_pixel_budget (13);
      FilterScheduler::Bytes actual; unsigned commits=0;
      const auto deadline=g_get_monotonic_time ()+10*G_TIME_SPAN_SECOND;
      while (!scheduler.settled () && g_get_monotonic_time ()<deadline)
        {
          scheduler.step (true,[&] (std::size_t offset,std::size_t count,FilterScheduler::Bytes& bytes) {
            for (std::size_t i=0;i<count*stride;++i) bytes.push_back ((offset*stride+i)%251);
          },[&] (std::size_t offset,std::size_t count,const std::uint8_t *bytes) {
            g_assert_cmpuint (offset*stride,==,actual.size ()); actual.insert (actual.end (),bytes,bytes+count*stride);
          },[&] (std::uint64_t) {++commits;});
          g_usleep (100);
        }
      g_assert_true (scheduler.settled ()); g_assert_cmpuint (commits,==,1);
      g_assert_cmpuint (actual.size (),==,37*11*stride);
      for (std::size_t i=0;i<actual.size ();++i) g_assert_cmpuint (actual[i],==,i%251);
    }
}
static void invalid_stride_never_reads ()
{
  for (std::size_t stride : {std::size_t (0),std::size_t (33),std::numeric_limits<std::size_t>::max ()})
    {
      FilterScheduler scheduler; FilterScheduler::Request request;
      request.width=2;request.height=2;request.bytes_per_pixel=stride;
      request.process=[] (const FilterScheduler::Bytes&,std::atomic<bool>&,FilterScheduler::Bytes&) {g_error ("invalid stride launched");return false;};
      scheduler.set_request (request);
      scheduler.step (true,[] (std::size_t,std::size_t,FilterScheduler::Bytes&) {g_error ("invalid stride read");},{},{});
      g_assert_true (scheduler.state ()==FilterScheduler::State::failed);g_assert_cmpuint (scheduler.starts (),==,0);
    }
}

int main (int argc, char **argv)
{
  g_test_init (&argc, &argv, nullptr);
  gchar *directory = g_dir_make_tmp ("painter-scheduler-spool-XXXXXX",nullptr);
  g_assert_nonnull (directory); spool_directory = directory; g_free (directory);
#define ADD(name) g_test_add_func ("/painter-filter-scheduler/" #name, name)
  ADD (recursive_step_does_not_consume_another_chunk); ADD (spool_completed_cache_and_chunk_geometry); ADD (spool_reentrant_read_cancellation);
  ADD (spool_cancel_request_is_not_completion); ADD (spool_change_during_import);
  ADD (spool_read_failure_cancels_unsealed_worker); ADD (spool_result_failure_never_commits);
  ADD (spool_large_metadata_is_bounded_preparation); ADD (spool_close_during_import_keeps_callback_bytes);
  ADD (admission_waits_without_reading_and_resumes_fifo); ADD (admission_cancel_retains_worker_reservation);
  ADD (admission_edit_and_dependency_wait_release_preparation); ADD (admission_failure_and_loaded_cache_release);
  ADD (saved_generation_boundaries); ADD (commit_failure_does_not_certify_cache); ADD (obsolete_read_failure_preserves_new_edit);
  ADD (saved_cache_generations); ADD (restored_cache_rejects_colliding_old_job);
  ADD (rejected_worker_is_not_settled_until_done); ADD (reject_during_read); ADD (reject_during_import); ADD (closed_request_is_inert);
  ADD (adjustable_chunk_budget); ADD (completed_cache); ADD (bounded_chunks); ADD (dependency_priority);
  ADD (cancel_request_is_not_completion); ADD (changes_during_preparation); ADD (changes_during_import);
  ADD (failures_do_not_retry); ADD (close_does_not_wait); ADD (loaded_cache_not_reexecuted);
  ADD (invalid_request_and_result); ADD (reentry_at_commit);
  ADD (admission_switch_keeps_restored_cache); ADD (native_stride_vector_and_spool); ADD (invalid_stride_never_reads);
  const auto result = g_test_run ();
  g_assert_cmpint (g_rmdir (spool_directory.c_str ()), ==, 0);
  return result;
}
