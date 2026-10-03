/* SPDX-License-Identifier: GPL-3.0-or-later
 * Private one-request subprocess, not a general command/procedure runner. */
#include "config.h"
#include "painter/filter-wire.hpp"
#include "painter/filter-raster.hpp"
#include "core/gimpfilterprocedure.hpp"
#include <glib.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <thread>
#ifndef G_OS_WIN32
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
using namespace GimpPainter;
#ifndef G_OS_WIN32
namespace {
void read_exact (int fd, std::uint8_t *bytes, std::size_t size) {
  while (size) { const auto n = read (fd, bytes, size); if (n < 0 && errno == EINTR) continue;
    if (n <= 0) throw std::runtime_error ("Truncated private Filter request");
    bytes += n; size -= std::size_t (n); }
}
void write_exact (int fd, const std::uint8_t *bytes, std::size_t size) {
  while (size) { const auto n = write (fd, bytes, size); if (n < 0 && errno == EINTR) continue;
    if (n <= 0) throw std::runtime_error ("Cannot write private Filter result");
    bytes += n; size -= std::size_t (n); }
}
FilterWire::Frame read_frame () {
  std::array<std::uint8_t, FilterWire::header_size> header;
  read_exact (STDIN_FILENO, header.data (), header.size ());
  const auto size = FilterWire::payload_size (header.data ());
  std::vector<std::uint8_t> bytes (header.begin (), header.end ()); bytes.resize (header.size () + size);
  read_exact (STDIN_FILENO, bytes.data () + header.size (), size);
  return FilterWire::decode (bytes.data (), bytes.size ());
}
void write_frame (int fd, const FilterWire::Frame& frame) {
  const auto bytes = FilterWire::encode (frame); write_exact (fd, bytes.data (), bytes.size ());
}
class Output final : public FilterRaster {
  int fd_; std::uint64_t size_, offset_ = 0;
public:
  Output (int fd, std::uint64_t size) : fd_ (fd), size_ (size) {}
  std::uint64_t size () const noexcept override { return size_; }
  void read (std::uint64_t, std::size_t, std::uint8_t *) override { throw std::logic_error ("Private Filter result is write-only"); }
  void write (std::uint64_t offset, std::size_t count, const std::uint8_t *bytes) override {
    if (offset != offset_ || !count || count % 4 || count > size_ - offset_) throw std::invalid_argument ("Invalid private Filter output extent");
    while (count) { const auto n = std::min (count, FilterWire::pixel_limit);
      write_frame (fd_, {FilterWire::Type::output, offset_, {bytes, bytes + n}});
      count -= n; bytes += n; offset_ += n; }
  }
  void flush () override {}
  void finish () { if (offset_ != size_) throw std::runtime_error ("Private Filter output is incomplete"); }
};
}
#endif
int main (int argc, char **argv)
{
#ifdef G_OS_WIN32
  return 125;
#else
  if (argc != 3 || std::strcmp (argv[1], "--filter-worker-v3")) return 125;
  /* Duplicate before GIMP/GEGL/plugin initialization. All ordinary diagnostics
   * (including library writes to stdout) go to stderr; descendants cannot keep
   * this protocol descriptor alive across exec. */
  const int protocol = fcntl (STDOUT_FILENO, F_DUPFD_CLOEXEC, 10);
  if (protocol < 0 || dup2 (STDERR_FILENO, STDOUT_FILENO) < 0) return 125;
  if (getpgrp () != getpid () || fcntl (3, F_SETFD, FD_CLOEXEC) < 0) return 125;
  struct stat st {};
  if (lstat (argv[2], &st) < 0 || !S_ISDIR (st.st_mode) || st.st_uid != geteuid () || (st.st_mode & 0777) != 0700) return 125;
  std::thread ([] {
    std::uint8_t ignored;
    for (;;) { const auto n = read (3, &ignored, 1); if (n < 0 && errno == EINTR) continue;
      if (n <= 0) { kill (0, SIGKILL); _exit (124); } }
  }).detach ();
  g_setenv ("GIMP3_DIRECTORY", argv[2], TRUE);
  g_setenv ("GIMP3_CACHEDIR", argv[2], TRUE);
  g_setenv ("GIMP3_TEMPDIR", argv[2], TRUE);
  g_setenv ("GIMP3_DATADIR", argv[2], TRUE);
  g_setenv ("GIMP3_PLUGINDIR", argv[2], TRUE);
  g_setenv ("GEGL_SWAP", argv[2], TRUE);
  g_setenv ("GIMP_PAINTER_FILTER_WORKER", "1", TRUE);
  int status = 1;
  try {
    const auto request = FilterWire::request (read_frame ());
    const auto total = request.bytes ();
    auto disposition = FilterProcedureDisposition::pending;
    {
      TemporaryFilterRaster input (argv[2], total);
      std::uint64_t offset = 0;
      for (;;) {
        const auto frame = read_frame ();
        if (frame.type == FilterWire::Type::input_end) {
          if (frame.offset != total || offset != total) throw std::invalid_argument ("Incomplete private Filter input");
          break;
        }
        if (frame.type != FilterWire::Type::input || frame.offset != offset || frame.payload.size () > total - offset)
          throw std::invalid_argument ("Invalid private Filter input sequence");
        input.write (offset, frame.payload.size (), frame.payload.data ()); offset += frame.payload.size ();
      }
      std::uint8_t trailing; ssize_t n; do { n = read (STDIN_FILENO, &trailing, 1); } while (n < 0 && errno == EINTR);
      if (n != 0) throw std::invalid_argument ("Trailing private Filter input bytes");
      input.flush (); Output output (protocol, total); std::atomic<bool> cancel {false};
      disposition = run_filter_procedure (request, input, output, cancel);
      if (disposition == FilterProcedureDisposition::pending) throw std::runtime_error ("Private Filter procedure cancelled");
      output.finish ();
    }
    /* run_filter_procedure and input destruction must finish before terminal.
     * The owner still requires this terminal, EOF, successful reap and cleanup. */
    write_frame (protocol, FilterWire::success (total, disposition)); status = 0;
  } catch (const std::exception& error) {
    std::fprintf (stderr, "Private Filter worker: %s\n", error.what ());
    try { write_frame (protocol, {FilterWire::Type::failure, 0, {}}); } catch (...) {}
  } catch (...) {
    std::fprintf (stderr, "Private Filter worker: unknown exception\n");
    try { write_frame (protocol, {FilterWire::Type::failure, 0, {}}); } catch (...) {}
  }
  close (protocol); return status;
#endif
}
