/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "filter-scheduler.hpp"
#include "filter-lifetime.hpp"
#include <glib.h>
#include <glib/gstdio.h>
#include <atomic>
#include <thread>
using namespace GimpPainter;
static std::atomic<unsigned> observed {0}, release_stage {0};
extern "C" void filter_spool_observation (int stage);
extern "C" void filter_spool_observation (int stage)
{
  observed.store (stage, std::memory_order_release);
  while (release_stage.load (std::memory_order_acquire) < unsigned (stage))
    std::this_thread::yield ();
}
static void wait_stage (unsigned stage)
{
  const auto deadline = g_get_monotonic_time () + 5 * G_TIME_SPAN_SECOND;
  while (observed.load (std::memory_order_acquire) < stage && g_get_monotonic_time () < deadline)
    g_usleep (100);
  g_assert_cmpuint (observed.load (), ==, stage);
}
int main ()
{
  gchar *directory = g_dir_make_tmp ("painter-completion-gap-XXXXXX", nullptr);
  g_assert_nonnull (directory);
  FilterScheduler scheduler;
  FilterScheduler::Request request;
  request.width = 19; request.height = 17; request.spool_directory = directory;
  request.raster_process = [] (FilterRaster& input, FilterRaster& output, std::atomic<bool>&,
                               const FilterRasterFactory&) {
    FilterScheduler::Bytes data (input.size ());
    input.read (0, data.size (), data.data ()); output.write (0, data.size (), data.data ());
    return true;
  };
  scheduler.set_request (std::move (request));
  unsigned captures = 0, imports = 0, commits = 0;
  auto step = [&] {
    return scheduler.step (true,
      [] (std::size_t, std::size_t count, FilterScheduler::Bytes& data) { data.assign (count * 4, 17); },
      [&] (std::size_t offset, std::size_t count, const std::uint8_t *data) {
        g_assert_cmpuint (offset, ==, 0); g_assert_cmpuint (count, ==, 19 * 17);
        for (unsigned i = 0; i < count * 4; ++i) g_assert_cmpuint (data[i], ==, 17);
        ++imports;
      }, [&] (std::uint64_t) { ++commits; }, {}, [&] { ++captures; return true; });
  };
  g_assert_true (step ());
  wait_stage (1);
  g_assert_true (step ()); // Capture context while real spool is exporting.
  g_assert_cmpuint (captures, ==, 1);
  g_assert_true (scheduler.state () == FilterScheduler::State::importing);
  release_stage.store (1, std::memory_order_release);
  wait_stage (2); // Worker paused between phase=complete and done=true.
  g_assert_true (step ());
  g_assert_true (scheduler.state () == FilterScheduler::State::importing);
  for (unsigned i = 0; i < 8; ++i) g_assert_true (step ());
  g_assert_cmpuint (captures, ==, 1); g_assert_cmpuint (imports, ==, 1);
  g_assert_cmpuint (commits, ==, 0);
  release_stage.store (2, std::memory_order_release);
  const auto deadline = g_get_monotonic_time () + 5 * G_TIME_SPAN_SECOND;
  bool continuation = true;
  while (continuation && g_get_monotonic_time () < deadline)
    { continuation = step (); if (continuation) g_usleep (100); }
  g_assert_false (continuation); g_assert_true (scheduler.settled ());
  g_assert_cmpuint (captures, ==, 1); g_assert_cmpuint (imports, ==, 1);
  g_assert_cmpuint (commits, ==, 1);
  while (FilterLifetime::pending () && g_get_monotonic_time () < deadline) g_usleep (100);
  g_assert_cmpuint (FilterLifetime::pending (), ==, 0);
  g_assert_cmpint (g_rmdir (directory), ==, 0); g_free (directory);
  g_print ("Completion-gap import remains monotonic; context captured once\n");
}
