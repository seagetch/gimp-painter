/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "filter-process.hpp"
#include "filter-wire.hpp"
#include <glib.h>
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstring>
#include <iostream>
#include <thread>
#ifndef G_OS_WIN32
#include <signal.h>
#include <sys/wait.h>
#include <cerrno>
#include <unistd.h>
#endif
using namespace GimpPainter;
using Disposition = FilterProcedureDisposition;
#ifdef FILTER_PROCESS_WRAP_REAPER
static bool force_external_reap = false;
static pid_t externally_reaped = 0;
static unsigned signals_after_reap = 0;
extern "C" int __real_waitid (idtype_t, id_t, siginfo_t *, int);
extern "C" int __real_kill (pid_t, int);
extern "C" int __wrap_waitid (idtype_t, id_t, siginfo_t *, int);
extern "C" int __wrap_kill (pid_t, int);
extern "C" int __wrap_waitid (idtype_t type, id_t id, siginfo_t *info, int flags) {
  if (force_external_reap) {
    const int observed = __real_waitid (type,id,info,flags);
    if (observed < 0 || !info->si_pid) return observed;
    int status; pid_t result;
    do { result = waitpid (pid_t (id), &status, 0); } while (result < 0 && errno == EINTR);
    g_assert (result == pid_t (id)); externally_reaped = result;
    force_external_reap = false; errno = ECHILD; return -1;
  }
  return __real_waitid (type,id,info,flags);
}
extern "C" int __wrap_kill (pid_t pid, int signal) {
  if (externally_reaped && (pid == externally_reaped || pid == -externally_reaped)) ++signals_after_reap;
  return __real_kill (pid,signal);
}
#endif

#ifndef G_OS_WIN32
namespace {
void read_all (int fd, std::uint8_t *p, std::size_t size) {
  while (size) { const auto n = read (fd, p, size); if (n <= 0) _exit (42); size -= n; p += n; }
}
void write_all (int fd, const std::uint8_t *p, std::size_t size) {
  while (size) { const auto n = write (fd, p, size); if (n <= 0) _exit (43); size -= n; p += n; }
}
FilterWire::Frame frame () {
  std::vector<std::uint8_t> b (FilterWire::header_size); read_all (0, b.data (), b.size ());
  const auto n = FilterWire::payload_size (b.data ()); b.resize (b.size () + n);
  read_all (0, b.data () + FilterWire::header_size, n); return FilterWire::decode (b.data (), b.size ());
}
void send (const FilterWire::Frame& f) { const auto b = FilterWire::encode (f); write_all (1, b.data (), b.size ()); }
void child () {
  const auto r = FilterWire::request (frame ());
  const auto mode = r.procedure == FilterProcedure::convolution ? r.alpha_alg : r.angle;
  const auto disposition = !r.execution_region ().intersects ? Disposition::no_merge :
                           r.raw_shadow ? Disposition::shadow : Disposition::merged;
  if (mode == 1) _exit (0); // no result or terminal
  if (mode == 2) { signal (SIGTERM, SIG_IGN); for (;;) pause (); }
  std::vector<std::uint8_t> data;
  for (;;) { const auto f = frame (); if (f.type == FilterWire::Type::input_end) break; data.insert (data.end (), f.payload.begin (), f.payload.end ()); }
  char byte; g_assert (read (0, &byte, 1) == 0);
  if (mode == 3) { write_all (1, reinterpret_cast<const std::uint8_t*> ("bad"), 3); _exit (0); }
  if (mode == 4) { auto b = FilterWire::encode ({FilterWire::Type::output, 0, {1,2,3,4}}); b[8] = 0xff; b[9] = 0xff; b[10] = 0xff; write_all (1,b.data (),b.size ()); _exit (0); }
  if (mode == 5) { send ({FilterWire::Type::output, 4, {1,2,3,4}}); _exit (0); }
  if (mode == 6) { send (FilterWire::success (r.bytes (),disposition)); _exit (0); }
  if (mode == 7) { send ({FilterWire::Type::failure, 0, {}}); _exit (1); }
  for (std::size_t at = 0; at < data.size (); at += FilterWire::pixel_limit) {
    const auto n = std::min (FilterWire::pixel_limit, data.size () - at);
    send ({FilterWire::Type::output, at, {data.data () + at, data.data () + at + n}});
  }
  if (mode == 13) {
    send (FilterWire::success (r.bytes (),r.raw_shadow ? Disposition::merged : Disposition::shadow)); _exit (0);
  }
  if (mode == 14) { send (FilterWire::success (r.bytes (),Disposition::no_merge)); _exit (0); }
  if (mode == 15) {
    auto terminal = FilterWire::success (r.bytes (),disposition); terminal.payload[0] = 255;
    send (terminal); _exit (0);
  }
  if (mode == 17) { send (FilterWire::success (r.bytes (),Disposition::merged)); _exit (0); }
  if (mode != 8) send (FilterWire::success (r.bytes (),disposition));
  if (mode == 9) send (FilterWire::success (r.bytes (),disposition));
  if (mode == 10) write_all (1, reinterpret_cast<const std::uint8_t*> ("x"), 1);
  if (mode == 11) _exit (2);
  if (mode == 16) { signal (SIGTERM, SIG_IGN); for (;;) pause (); }
  if (mode == 12 && fork () == 0) { signal (SIGTERM, SIG_IGN); for (;;) pause (); }
  _exit (0);
}
struct Raster : FilterRaster {
  std::vector<std::uint8_t> bytes;
  bool fail_flush = false;
  explicit Raster (std::size_t n) : bytes (n) {}
  std::uint64_t size () const noexcept override { return bytes.size (); }
  void read (std::uint64_t at, std::size_t n, std::uint8_t *p) override { g_assert (at+n <= size ()); std::copy_n (bytes.data () + at,n,p); }
  void write (std::uint64_t at, std::size_t n, const std::uint8_t *p) override { g_assert (at+n <= size ()); std::copy_n (p,n,bytes.data () + at); }
  void flush () override { if (fail_flush) throw std::runtime_error ("Injected output flush failure"); }
};
}
#endif
int main (int argc, char **argv) {
#ifndef G_OS_WIN32
  if (argc == 3 && !std::strcmp (argv[1], "--filter-worker-v5")) child ();
  gchar *absolute = g_canonicalize_filename (argv[0], nullptr);
  gchar *directory = g_dir_make_tmp ("filter-process-test-XXXXXX", nullptr);
  g_assert (directory);
  FilterProcessOptions options {absolute,directory}; g_free (absolute);
  FilterProcedureRequest request; request.width = 257; request.height = 139;
  Raster input (request.bytes ()), output (request.bytes ());
  for (std::size_t i = 0; i < input.bytes.size (); ++i) input.bytes[i] = std::uint8_t (i * 17);
  unsigned passed = 0;
  auto outcome = std::make_shared<FilterProcedureResult> ();
  for (bool raw : {false,true}) for (int mode = 0; mode <= 16; ++mode) {
    request.raw_shadow = raw;
    request.start_region = raw ? FilterSelectionRegion {true,true,3,4,250,130} : FilterSelectionRegion {};
    outcome->publish (Disposition::shadow);
    request.angle = mode; std::atomic<bool> cancel {false};
    std::thread cancellation;
    if (mode == 2 || mode == 16) cancellation = std::thread ([&] { std::this_thread::sleep_for (std::chrono::milliseconds (mode == 16 ? 250 : 100)); cancel = true; });
    bool success = false, threw = false;
    const auto start = std::chrono::steady_clock::now ();
    try { success = filter_process (request,input,output,cancel,options,outcome); } catch (const std::exception&) { threw = true; }
    if (cancellation.joinable ()) cancellation.join ();
    if (mode == 0 || mode == 12) {
      g_assert (success && !threw && output.bytes == input.bytes);
      g_assert (outcome->disposition () == (raw ? Disposition::shadow : Disposition::merged));
    }
    else if (mode == 2 || mode == 16) { g_assert (!success && !threw); }
    else g_assert (!success && threw);
    if (!success) g_assert (outcome->disposition () == Disposition::pending);
    g_assert (std::chrono::steady_clock::now () - start < std::chrono::seconds (5));
    GDir *dir = g_dir_open (directory,0,nullptr); g_assert (dir); g_assert (!g_dir_read_name (dir)); g_dir_close (dir); ++passed;
  }
  for (bool raw : {false,true}) {
    request.raw_shadow = raw; request.start_region = {true,false,257,4,257,130};
    request.angle = 0; std::atomic<bool> cancel {false};
    g_assert (filter_process (request,input,output,cancel,options,outcome));
    g_assert (outcome->disposition () == Disposition::no_merge && output.bytes == input.bytes); ++passed;
    request.angle = 17; bool threw = false;
    try { filter_process (request,input,output,cancel,options,outcome); } catch (const std::exception&) { threw = true; }
    g_assert (threw && outcome->disposition () == Disposition::pending); ++passed;
  }
  request.angle = 0; request.start_region = {}; request.raw_shadow = true;
  { std::atomic<bool> cancel {false}; bool threw = false;
    try { filter_process (request,input,output,cancel,options); } catch (const std::invalid_argument&) { threw = true; }
    g_assert (threw); ++passed;
  }
  { std::atomic<bool> cancel {false}; bool threw = false;
    try { filter_process (request,input,input,cancel,options,outcome); } catch (const std::invalid_argument&) { threw = true; }
    g_assert (threw && outcome->disposition () == Disposition::pending); ++passed;
  }
  { std::atomic<bool> cancel {true}; outcome->publish (Disposition::shadow);
    g_assert (!filter_process (request,input,output,cancel,options,outcome));
    g_assert (outcome->disposition () == Disposition::pending); ++passed;
  }
  { std::atomic<bool> cancel {false}; bool threw = false; output.fail_flush = true;
    try { filter_process (request,input,output,cancel,options,outcome); } catch (const std::runtime_error&) { threw = true; }
    output.fail_flush = false;
    g_assert (threw && outcome->disposition () == Disposition::pending); ++passed;
  }
  request.raw_shadow = false;
#ifdef FILTER_PROCESS_WRAP_REAPER
  request.angle = 1; force_external_reap = true;
  { std::atomic<bool> cancel {false}; bool lost = false;
    try { filter_process (request,input,output,cancel,options); }
    catch (const std::exception& e) { lost = std::string (e.what ()).find ("ownership was lost") != std::string::npos; }
    g_assert (lost && externally_reaped && !signals_after_reap); ++passed;
  }
#endif
  request.angle = 0; request.procedure = FilterProcedure::small_tiles;
  for (int factor = 0; factor <= 6; ++factor) for (bool raw : {false,true}) for (bool outside : {false,true}) {
    request.tiles = factor; request.raw_shadow = raw;
    request.start_region = outside ? FilterSelectionRegion {true,false,0,0,0,0} : FilterSelectionRegion {};
    std::atomic<bool> cancel {false};
    g_assert (filter_process (request,input,output,cancel,options,outcome));
    g_assert (output.bytes == input.bytes && outcome->disposition () ==
              (outside ? Disposition::no_merge : raw ? Disposition::shadow : Disposition::merged));
    ++passed;
  }
  request.procedure = FilterProcedure::retinex; request.tiles = 2; request.start_region = {};
  for (unsigned storage : {3u,4u}) for (bool raw : {false,true}) {
    request.storage_channels = storage; request.raw_shadow = raw; request.cvar = 0.123456789;
    std::atomic<bool> cancel {false};
    g_assert (filter_process (request,input,output,cancel,options,outcome));
    g_assert (output.bytes == input.bytes && outcome->disposition () ==
              (raw ? Disposition::shadow : Disposition::merged));
    ++passed;
  }
  for (unsigned mode : {0u,1u,2u,3u}) for (bool raw : {false,true}) for (bool outside : {false,true}) {
    FilterProcedureRequest convolution; convolution.procedure = FilterProcedure::convolution;
    convolution.width = 33; convolution.height = 17; convolution.sample_mode = mode; convolution.alpha_alg = 0;
    convolution.raw_shadow = raw;
    convolution.start_region = outside ? FilterSelectionRegion {true,false,0,0,0,0} : FilterSelectionRegion {};
    Raster a (convolution.bytes ()), b (convolution.bytes ());
    for (std::size_t i = 0; i < a.bytes.size (); ++i) a.bytes[i] = std::uint8_t (i*17);
    std::atomic<bool> cancelled {false};
    g_assert (filter_process (convolution,a,b,cancelled,options,outcome));
    g_assert (a.bytes == b.bytes && outcome->disposition () ==
      (outside ? Disposition::no_merge : raw ? Disposition::shadow : Disposition::merged)); ++passed;
  }
  for (unsigned sample : {0u,1u}) for (int mode : {1,2,5,7,11,12,16}) {
    FilterProcedureRequest convolution; convolution.procedure = FilterProcedure::convolution;
    convolution.width = 33; convolution.height = 17; convolution.sample_mode = sample;
    convolution.raw_shadow = true; convolution.alpha_alg = mode;
    Raster a (convolution.bytes ()),b (convolution.bytes ()); std::atomic<bool> cancelled {false};
    std::thread stop;
    if (mode == 2 || mode == 16) stop = std::thread ([&] {
      std::this_thread::sleep_for (std::chrono::milliseconds (mode == 16 ? 250 : 100)); cancelled = true;
    });
    bool success = false, failed = false;
    try { success = filter_process (convolution,a,b,cancelled,options,outcome); }
    catch (const std::exception&) { failed = true; }
    if (stop.joinable ()) stop.join ();
    if (mode == 12) g_assert (success && !failed && outcome->disposition () == Disposition::shadow);
    else g_assert (!success && outcome->disposition () == Disposition::pending && (cancelled || failed));
    ++passed;
  }
  request.start_region = {}; request.raw_shadow = false;
  request.angle = 0; options.executable += "-missing"; std::atomic<bool> cancel {false};
  bool threw = false; try { filter_process (request,input,output,cancel,options,outcome); } catch (const std::exception&) { threw = true; } g_assert (threw); ++passed;
  GDir *dir = g_dir_open (directory,0,nullptr); g_assert (dir && !g_dir_read_name (dir)); g_dir_close (dir); g_assert (rmdir (directory) == 0); g_free (directory);
  std::cout << passed << " Filter process cases passed\n";
#endif
}
