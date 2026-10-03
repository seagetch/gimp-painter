/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "filter-scheduler.hpp"
#include "filter-lifetime.hpp"
#include <glib.h>
#include <glib/gstdio.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <thread>
using namespace GimpPainter;
using State = FilterScheduler::State;
static std::string directory;

struct GateHarness
{
  FilterScheduler scheduler;
  std::thread::id owner = std::this_thread::get_id ();
  std::atomic<unsigned> executions {0};
  unsigned plain_context = 0;
  std::atomic<unsigned> observed_context {0};
  std::uint8_t input_value = 17;
  unsigned reads = 0, imports = 0, commits = 0;
  std::size_t prepared = 0;
  FilterScheduler::Bytes output;
  FilterScheduler::Gate before_process, before_import;
  explicit GateHarness (bool spill, unsigned width = 19, unsigned height = 17,
                        std::shared_ptr<WorkAdmission> pool = {})
    : scheduler (pool ? pool : std::make_shared<WorkAdmission> (WorkAdmission::Limits {1, 4 * 1024 * 1024}))
  {
    FilterScheduler::Request request;
    request.width = width; request.height = height; request.peak_bytes = 1024 * 1024;
    request.process = [&] (const FilterScheduler::Bytes& in, std::atomic<bool>& cancel,
                           FilterScheduler::Bytes& out) {
      g_assert_true (std::this_thread::get_id () != owner); ++executions;
      observed_context.store (plain_context);
      out = in; return !cancel.load ();
    };
    if (spill)
      request.raster_process = [&] (FilterRaster& in, FilterRaster& out, std::atomic<bool>& cancel,
                                    const FilterRasterFactory&) {
        g_assert_true (std::this_thread::get_id () != owner); ++executions;
        observed_context.store (plain_context);
        FilterScheduler::Bytes bytes (in.size ());
        in.read (0, bytes.size (), bytes.data ()); out.write (0, bytes.size (), bytes.data ());
        return !cancel.load ();
      };
    request.spool_directory = directory;
    scheduler.set_request (std::move (request));
    scheduler.set_pixel_budget (11);
  }
  bool step ()
  {
    return scheduler.step (true,
      [&] (std::size_t offset, std::size_t count, FilterScheduler::Bytes& bytes) {
        if (!offset) prepared = 0;
        g_assert_cmpuint (offset, ==, prepared); prepared += count;
        ++reads; bytes.assign (count * 4, input_value);
      },
      [&] (std::size_t offset, std::size_t count, const std::uint8_t *bytes) {
        if (!offset) output.clear ();
        ++imports; output.insert (output.end (), bytes, bytes + count * 4);
      }, [&] (std::uint64_t) { ++commits; }, before_process, before_import);
  }
  void finish ()
  {
    const auto deadline = g_get_monotonic_time () + 5 * G_TIME_SPAN_SECOND;
    bool continuation = true;
    while (continuation && g_get_monotonic_time () < deadline)
      {
        continuation = step ();
        if (continuation) g_usleep (100);
      }
    g_assert_false (continuation);
    g_assert_true (scheduler.settled () || scheduler.state () == State::closed);
    // Independent workers finish their normal lifetime after an owner close.
    while (FilterLifetime::pending () && g_get_monotonic_time () < deadline) g_usleep (100);
    g_assert_cmpuint (FilterLifetime::pending (), ==, 0);
  }
};

static void bounded_gates ()
{
  for (bool spill : {false, true})
    {
      GateHarness h (spill);
      unsigned pre = 0, post = 0;
      h.before_process = [&] {
        g_assert_true (std::this_thread::get_id () == h.owner);
        g_assert_cmpuint (h.prepared, ==, 19 * 17);
        g_assert_cmpuint (h.executions.load (), ==, 0);
        g_assert_cmpuint (h.scheduler.starts (), ==, 0);
        return ++pre == 4;
      };
      h.before_import = [&] {
        g_assert_true (std::this_thread::get_id () == h.owner);
        g_assert_cmpuint (h.executions.load (), ==, 1);
        g_assert_cmpuint (h.imports, ==, 0);
        return ++post == 3;
      };
      h.finish ();
      g_assert_cmpuint (pre, ==, 4); g_assert_cmpuint (post, ==, 3);
      g_assert_cmpuint (h.commits, ==, 1); g_assert_cmpuint (h.executions.load (), ==, 1);
      g_assert_cmpuint (h.output.size (), ==, 19 * 17 * 4);
      for (auto byte : h.output) g_assert_cmpuint (byte, ==, 17);
    }
}

static void invalidation_reentry ()
{
  for (bool spill : {false, true}) for (bool final_gate : {false, true})
    {
      GateHarness h (spill);
      bool changed = false;
      auto gate = [&] {
        if (!changed)
          { changed = true; h.input_value = 22; h.scheduler.invalidate (); }
        return true;
      };
      if (final_gate) h.before_import = gate; else h.before_process = gate;
      h.finish ();
      g_assert_true (changed); g_assert_cmpuint (h.commits, ==, 1);
      g_assert_cmpuint (h.executions.load (), ==, final_gate ? 2 : 1);
      for (auto byte : h.output) g_assert_cmpuint (byte, ==, 22);
    }
}

static void close_and_failure ()
{
  for (bool spill : {false, true}) for (bool final_gate : {false, true}) for (bool close : {false, true})
    {
      GateHarness h (spill);
      auto gate = [&] () -> bool {
        if (close) h.scheduler.close ();
        else throw std::runtime_error ("Context capture failed");
        return true;
      };
      if (final_gate) h.before_import = gate; else h.before_process = gate;
      h.finish ();
      g_assert_cmpuint (h.commits, ==, 0); g_assert_cmpuint (h.imports, ==, 0);
      g_assert_cmpuint (h.executions.load (), ==, final_gate ? 1 : 0);
      g_assert_true (h.scheduler.state () == (close ? State::closed : State::failed));
      for (unsigned i = 0; i < 10; ++i) g_assert_false (h.step ());
    }
}

static void blocked_exporter_and_admission ()
{
  for (bool close : {false, true})
    {
      auto pool = std::make_shared<WorkAdmission> (WorkAdmission::Limits {1, 4 * 1024 * 1024});
      GateHarness h (true, 512, 257, pool);
      h.scheduler.set_pixel_budget (32768);
      unsigned captures = 0;
      bool release = false;
      h.before_import = [&] { ++captures; return release; };
      const auto deadline = g_get_monotonic_time () + 5 * G_TIME_SPAN_SECOND;
      while (!captures && g_get_monotonic_time () < deadline)
        { g_assert_true (h.step ()); g_usleep (100); }
      g_assert_cmpuint (captures, >, 0);
      // Five result chunks cannot fit in the two-slot spool queue. No import
      // callback runs while the owner captures its coherent final context.
      GateHarness next (false, 1, 1, pool);
      for (unsigned i = 0; i < 25; ++i)
        {
          g_assert_true (h.step ()); g_assert_true (next.step ());
          g_assert_cmpuint (next.reads, ==, 0); g_assert_cmpuint (h.imports, ==, 0);
          g_assert_cmpuint (h.executions.load (), ==, 1); g_usleep (100);
        }
      if (close) h.scheduler.close ();
      else h.scheduler.reject ("Cancelled during final context capture");
      h.finish ();
      next.finish ();
      g_assert_cmpuint (h.commits, ==, 0); g_assert_cmpuint (h.imports, ==, 0);
      g_assert_cmpuint (next.commits, ==, 1);
    }
}

static void stable_capture_and_plain_context ()
{
  for (bool spill : {false, true})
    {
      GateHarness h (spill);
      unsigned captures = 0, context = 0, frozen = 0;
      h.before_process = [&] { context = 73; h.plain_context = 73; return true; };
      h.before_import = [&] { ++captures; frozen = context; return true; };
      const auto deadline = g_get_monotonic_time () + 5 * G_TIME_SPAN_SECOND;
      bool continuation = true;
      while (continuation && g_get_monotonic_time () < deadline)
        {
          continuation = h.step ();
          if (h.imports) context = 129;
          if (continuation) g_usleep (100);
        }
      g_assert_false (continuation); h.finish ();
      g_assert_cmpuint (captures, ==, 1); g_assert_cmpuint (frozen, ==, 73);
      g_assert_cmpuint (context, ==, 129); g_assert_cmpuint (h.commits, ==, 1);
      g_assert_cmpuint (h.executions.load (), ==, 1);
      g_assert_cmpuint (h.observed_context.load (), ==, 73);
    }
}

int main (int argc, char **argv)
{
  g_test_init (&argc, &argv, nullptr);
  auto *path = g_dir_make_tmp ("painter-owner-gates-XXXXXX", nullptr);
  g_assert_nonnull (path); directory = path; g_free (path);
  g_test_add_func ("/owner-gates/bounded", bounded_gates);
  g_test_add_func ("/owner-gates/invalidation-reentry", invalidation_reentry);
  g_test_add_func ("/owner-gates/close-and-failure", close_and_failure);
  g_test_add_func ("/owner-gates/blocked-exporter-and-admission", blocked_exporter_and_admission);
  g_test_add_func ("/owner-gates/stable-capture-and-plain-context", stable_capture_and_plain_context);
  const auto result = g_test_run ();
  g_assert_cmpint (g_rmdir (directory.c_str ()), ==, 0);
  return result;
}
