/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "filter-spool.hpp"
#include <glib.h>
#include <glib/gstdio.h>
#include <algorithm>
#include <chrono>
#include <memory>
#include <stdexcept>
#include <thread>
using namespace GimpPainter;
static std::string directory;
using Bytes = FilterSpool::Bytes;
static FilterSpool::Process copy_process ()
{
  return [] (FilterRaster& in,FilterRaster& out,std::atomic<bool>& cancel,const FilterRasterFactory&) {
    Bytes bytes (32768 * 4);
    for (std::uint64_t offset = 0; offset < in.size ();)
      {
        if (cancel.load ()) return false;
        const auto count = std::size_t (std::min (std::uint64_t (bytes.size ()),in.size ()-offset));
        in.read (offset,count,bytes.data ()); out.write (offset,count,bytes.data ()); offset += count;
      }
    return true;
  };
}
static void wait_done (FilterSpool& spool)
{
  const auto deadline = g_get_monotonic_time () + 10 * G_TIME_SPAN_SECOND;
  while (!spool.done () && g_get_monotonic_time () < deadline) g_usleep (100);
  g_assert_true (spool.done ());
}
static void roundtrip_and_output_geometry ()
{
  for (auto width : {std::size_t (17),std::size_t (40001)})
    {
      const std::size_t height = 7, pixels = width * height;
      FilterSpool spool (width,height,directory,copy_process (),{});
      std::size_t submitted = 0, received = 0;
      bool sealed = false;
      const auto deadline = g_get_monotonic_time () + 10 * G_TIME_SPAN_SECOND;
      while ((!spool.done () || received < pixels) && g_get_monotonic_time () < deadline)
        {
          if (!sealed && spool.can_submit ())
            {
              const auto count = std::min (pixels-submitted,std::size_t (919));
              Bytes bytes (count*4);
              for (std::size_t i = 0; i < bytes.size (); ++i) bytes[i] = ((submitted*4+i)*137) % 256;
              g_assert_true (spool.submit (submitted,bytes)); g_assert_true (bytes.empty ()); submitted += count;
              if (submitted == pixels) { spool.seal_input (); sealed = true; }
            }
          auto chunk = spool.take_result ();
          if (chunk)
            {
              g_assert_cmpuint (chunk->offset, ==, received);
              const auto count = chunk->bytes.size ()/4;
              g_assert_cmpuint (count, <=, FilterSpool::pixel_budget);
              g_assert_true (count <= width-received%width || (received%width == 0 && count%width == 0));
              for (std::size_t i = 0; i < chunk->bytes.size (); ++i)
                g_assert_cmpuint (chunk->bytes[i], ==, ((received*4+i)*137)%256);
              received += count;
            }
          g_usleep (100);
        }
      g_assert_true (spool.done ()); g_assert_true (spool.succeeded ()); g_assert_true (spool.started ());
      g_assert_cmpuint (received, ==, pixels); g_assert_cmpstr (spool.error ().c_str (), ==, "");
    }
}
static void missing_directory_fails_without_start ()
{
  FilterSpool spool (1,1,directory+"/missing",copy_process (),{});
  wait_done (spool); g_assert_false (spool.started ()); g_assert_false (spool.succeeded ());
  g_assert_false (spool.error ().empty ());
}
static void cancellation_while_collecting ()
{
  FilterSpool spool (65536,1,directory,copy_process (),{});
  Bytes first (4,9); g_assert_true (spool.submit (0,first));
  spool.cancel (); wait_done (spool); g_assert_false (spool.started ()); g_assert_false (spool.succeeded ());
}
static void cancellation_while_output_is_full ()
{
  FilterSpool spool (32768,8,directory,copy_process (),{});
  std::size_t submitted = 0;
  const auto deadline = g_get_monotonic_time () + 10 * G_TIME_SPAN_SECOND;
  while (submitted < 32768*8 && g_get_monotonic_time () < deadline)
    {
      if (spool.can_submit ())
        { Bytes bytes (32768*4,17); g_assert_true (spool.submit (submitted,bytes)); submitted += 32768; }
      g_usleep (100);
    }
  g_assert_cmpuint (submitted, ==, 32768*8); spool.seal_input ();
  while (spool.phase () != FilterSpool::Phase::exporting && g_get_monotonic_time () < deadline) g_usleep (100);
  g_assert_true (spool.phase () == FilterSpool::Phase::exporting);
  g_usleep (10000); // two output chunks fill, worker waits without owner calls
  g_assert_false (spool.done ());
  spool.cancel (); wait_done (spool); g_assert_false (spool.succeeded ());
}
static void close_keeps_worker_lease_until_completion ()
{
  auto pool = std::make_shared<WorkAdmission> (WorkAdmission::Limits {1,1024});
  auto ticket = pool->request (1024); auto lease = ticket.try_acquire ();
  auto release = std::make_shared<std::atomic<bool>> (false), started = std::make_shared<std::atomic<bool>> (false);
  auto spool = std::unique_ptr<FilterSpool> (new FilterSpool (1,1,directory,
    [release,started] (FilterRaster&,FilterRaster&,std::atomic<bool>&,const FilterRasterFactory&) {
      started->store (true);
      while (!release->load ()) std::this_thread::yield ();
      return false;
    },std::move (lease)));
  Bytes bytes (4,0); g_assert_true (spool->submit (0,bytes)); spool->seal_input ();
  while (!started->load ()) std::this_thread::yield ();
  const auto before = g_get_monotonic_time (); spool.reset ();
  g_assert_cmpint (g_get_monotonic_time ()-before, <, 100000);
  g_assert_cmpuint (pool->active_jobs (), ==, 1);
  release->store (true);
  const auto deadline = g_get_monotonic_time () + 10 * G_TIME_SPAN_SECOND;
  while (pool->active_jobs () && g_get_monotonic_time () < deadline) g_usleep (100);
  g_assert_cmpuint (pool->active_jobs (), ==, 0);
}
static void processing_exception_is_reported ()
{
  FilterSpool spool (1,1,directory,[] (FilterRaster&,FilterRaster&,std::atomic<bool>&,const FilterRasterFactory&) -> bool {
    throw std::runtime_error ("deliberate process failure");
  },{});
  Bytes bytes (4,0); g_assert_true (spool.submit (0,bytes)); spool.seal_input (); wait_done (spool);
  g_assert_true (spool.started ()); g_assert_false (spool.succeeded ());
  g_assert_cmpstr (spool.error ().c_str (), ==, "deliberate process failure");
  g_assert_false (bool (spool.take_result ()));
}
static void invalid_input_is_transactional ()
{
  FilterSpool spool (2,1,directory,copy_process (),{});
  Bytes bad (3,99), good (8,77);
  unsigned caught = 0;
  try { spool.submit (0,bad); } catch (const std::invalid_argument&) { ++caught; }
  try { spool.submit (1,good); } catch (const std::invalid_argument&) { ++caught; }
  try { spool.seal_input (); } catch (const std::invalid_argument&) { ++caught; }
  try { spool.error (); } catch (const std::logic_error&) { ++caught; }
  g_assert_cmpuint (caught, ==, 4); g_assert_cmpuint (bad.size (), ==, 3); g_assert_cmpuint (good.size (), ==, 8);
  g_assert_true (spool.submit (0,good)); spool.seal_input (); wait_done (spool);
  auto result = spool.take_result (); g_assert_nonnull (result.get ());
  g_assert_cmpuint (result->bytes.size (), ==, 8); g_assert_true (spool.succeeded ());
}
static void result_read_failure_never_certifies_partial_output ()
{
  FilterSpool spool (32768,4,directory,[] (FilterRaster&,FilterRaster& out,std::atomic<bool>&,const FilterRasterFactory&) {
    Bytes only_first_chunk (32768*4,17); out.write (0,only_first_chunk.size (),only_first_chunk.data ());
    return true; // deliberately incomplete processor, read must diagnose EOF
  },{});
  std::size_t offset = 0;
  const auto deadline = g_get_monotonic_time () + 10 * G_TIME_SPAN_SECOND;
  while (offset < 32768*4 && g_get_monotonic_time () < deadline)
    {
      if (spool.can_submit ())
        { Bytes bytes (32768*4,0); g_assert_true (spool.submit (offset,bytes)); offset += 32768; }
      g_usleep (100);
    }
  g_assert_cmpuint (offset, ==, 32768*4); spool.seal_input (); wait_done (spool);
  g_assert_false (spool.succeeded ()); g_assert_nonnull (strstr (spool.error ().c_str (),"read failed"));
  auto partial = spool.take_result (); g_assert_nonnull (partial.get ());
  g_assert_cmpuint (partial->offset, ==, 0); g_assert_false (bool (spool.take_result ()));
}
static void owner_api_thread_guards ()
{
  FilterSpool spool (1,1,directory,copy_process (),{});
  unsigned caught = 0;
  std::thread other ([&] {
    Bytes bytes (4,0); g_assert_false (spool.can_submit ()); g_assert_false (bool (spool.take_result ()));
    try { spool.submit (0,bytes); } catch (const std::logic_error&) { ++caught; }
    try { spool.seal_input (); } catch (const std::logic_error&) { ++caught; }
  }); other.join (); g_assert_cmpuint (caught, ==, 2);
  spool.cancel (); wait_done (spool);
}
int main (int argc, char **argv)
{
  g_test_init (&argc,&argv,nullptr);
  gchar *path = g_dir_make_tmp ("painter-spool-test-XXXXXX",nullptr);
  g_assert_nonnull (path); directory = path; g_free (path);
#define ADD(name) g_test_add_func ("/filter-spool/" #name,name)
  ADD (roundtrip_and_output_geometry); ADD (missing_directory_fails_without_start); ADD (cancellation_while_collecting);
  ADD (cancellation_while_output_is_full); ADD (close_keeps_worker_lease_until_completion);
  ADD (result_read_failure_never_certifies_partial_output); ADD (owner_api_thread_guards);
  ADD (processing_exception_is_reported); ADD (invalid_input_is_transactional);
  const auto result = g_test_run ();
  g_assert_cmpint (g_rmdir (directory.c_str ()), ==, 0);
  return result;
}
