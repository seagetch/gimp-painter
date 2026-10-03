/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "filter-process.hpp"
#include "filter-wire.hpp"
#include "filter-lifetime.hpp"
#include <glib.h>
#include <glib/gstdio.h>
#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <stdexcept>
#include <vector>
#ifndef G_OS_WIN32
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#endif
namespace GimpPainter {
#ifndef G_OS_WIN32
namespace {
using Clock = std::chrono::steady_clock;
struct Fd {
  int value = -1;
  ~Fd () { reset (); }
  void reset () noexcept { if (value >= 0) { close (value); value = -1; } }
};
void nonblocking (int fd) {
  const int flags = fcntl (fd, F_GETFL);
  if (flags < 0 || fcntl (fd, F_SETFL, flags | O_NONBLOCK) < 0)
    throw std::runtime_error ("Cannot configure Filter process pipe");
}
void group_setup (gpointer) { if (setpgid (0, 0) < 0) _exit (126); }
struct Child {
  GPid pid = 0;
  bool reaped = false;
  Fd input, output, lifeline;
  int status = -1;
  void signal_group (int signal) noexcept {
    if (!pid || reaped) return;
    /* The unreaped leader pins its PID/PGID. Never use the group after reap. */
    if (kill (-pid, signal) < 0 && errno == ESRCH) kill (pid, signal);
  }
  bool observe_exit () {
    if (reaped) return true;
    siginfo_t info {};
    if (waitid (P_PID, pid, &info, WEXITED | WNOHANG | WNOWAIT) < 0) {
      if (errno == EINTR) return false;
      if (errno == ECHILD) {
        /* An external reaper consumed the identity. Retire it before unwind;
         * even a destructor must never signal a potentially reused PID/PGID. */
        reaped = true; g_spawn_close_pid (pid); pid = 0; lifeline.reset ();
      }
      throw std::runtime_error ("Filter child ownership was lost before reap");
    }
    if (!info.si_pid) return false;
    /* DO_NOT_REAP_CHILD plus no GLib child watch gives this worker exclusive
     * reaper ownership. The app must not install a competing waitpid(-1)
     * reaper: WNOWAIT pins identity only while that ownership is respected.
     * Clear descendants while the leader still owns this group identity. */
    signal_group (SIGKILL);
    reap (); return true;
  }
  void reap () noexcept {
    if (!pid || reaped) return;
    pid_t result; do { result = waitpid (pid, &status, 0); } while (result < 0 && errno == EINTR);
    reaped = true; g_spawn_close_pid (pid); lifeline.reset ();
  }
  ~Child () { if (pid && !reaped) { signal_group (SIGKILL); reap (); } }
};
bool remove_tree (const std::string& path, unsigned depth = 0) noexcept {
  if (depth > 32) return false;
  GDir *dir = g_dir_open (path.c_str (), 0, nullptr);
  if (!dir) return g_rmdir (path.c_str ()) == 0 || errno == ENOENT;
  bool okay = true;
  while (const char *name = g_dir_read_name (dir)) {
    const std::string child = path + "/" + name; GStatBuf st {};
    if (g_lstat (child.c_str (), &st) < 0) { okay = false; continue; }
    if (S_ISDIR (st.st_mode)) okay = remove_tree (child, depth + 1) && okay;
    else if (g_unlink (child.c_str ()) < 0) okay = false;
  }
  g_dir_close (dir); return g_rmdir (path.c_str ()) == 0 && okay;
}
struct Profile {
  std::string path;
  explicit Profile (const std::string& root) {
    std::string pattern = root + "/gimp-filter-XXXXXX";
    std::vector<char> bytes (pattern.begin (), pattern.end ()); bytes.push_back (0);
    if (!g_mkdtemp_full (bytes.data (), 0700)) throw std::runtime_error ("Cannot create private Filter profile");
    path = bytes.data ();
  }
  bool clear () { if (path.empty ()) return true; if (!remove_tree (path)) return false; path.clear (); return true; }
  ~Profile () { if (!path.empty ()) remove_tree (path); }
};
}
#endif
bool filter_process (const FilterProcedureRequest& request, FilterRaster& input,
                     FilterRaster& output, std::atomic<bool>& cancel, const FilterProcessOptions& options,
                     std::shared_ptr<FilterProcedureResult> outcome)
{
  if (outcome) outcome->reset ();
  const auto total = request.bytes ();
  if (request.raw_shadow && !outcome) throw std::invalid_argument ("Raw Filter shadow needs an owned result channel");
  if (&input == &output || input.size () != total || output.size () != total)
    throw std::invalid_argument ("Filter process requires separate exact-sized rasters");
#ifdef G_OS_WIN32
  throw std::runtime_error ("Bundled PDB Filter process execution is not supported on this platform yet");
#else
  if (options.executable.empty () || options.executable[0] != '/' || options.temporary_directory.empty ())
    throw std::invalid_argument ("Filter process requires trusted absolute executable and temporary directory");
  if (cancel.load () || FilterLifetime::stopping ()) return false;
  Profile profile (options.temporary_directory);
  Child child;
  int life[2];
  if (pipe (life) < 0) throw std::runtime_error ("Cannot create Filter parent lifeline");
  Fd life_read; life_read.value = life[0]; child.lifeline.value = life[1];
  fcntl (life_read.value, F_SETFD, FD_CLOEXEC); fcntl (child.lifeline.value, F_SETFD, FD_CLOEXEC);
  const int source_fds[] = {life_read.value}, target_fds[] = {3};
  const gchar *argv[] = {options.executable.c_str (), "--filter-worker-v2", profile.path.c_str (), nullptr};
  GError *error = nullptr;
  if (!g_spawn_async_with_pipes_and_fds (nullptr, argv, nullptr,
      GSpawnFlags (G_SPAWN_DO_NOT_REAP_CHILD | G_SPAWN_CLOEXEC_PIPES), group_setup, nullptr,
      -1, -1, -1, source_fds, target_fds, 1, &child.pid,
      &child.input.value, &child.output.value, nullptr, &error)) {
    const std::string message = error ? error->message : "Cannot spawn Filter helper";
    g_clear_error (&error); throw std::runtime_error (message);
  }
  life_read.reset (); nonblocking (child.input.value); nonblocking (child.output.value);
  std::vector<std::uint8_t> outgoing = FilterWire::encode (FilterWire::request (request));
  std::size_t sent = 0;
  std::uint64_t input_offset = 0;
  bool sealed = false, eof = false, stopping = false, killed = false;
  Clock::time_point stop_time;
  std::vector<std::uint8_t> incoming;
  incoming.reserve (FilterWire::header_size + FilterWire::pixel_limit);
  std::size_t wanted = FilterWire::header_size;
  FilterWire::Result result (total, request.raw_shadow, !request.execution_region ().intersects);
  /* SIGPIPE belongs to this independent worker. Block it locally instead of
   * mutating the UI process's global signal disposition. Consume any new
   * pending SIGPIPE before restoring the original thread mask. */
  struct PipeSignal {
    sigset_t old {}, set {}; bool had = false;
    PipeSignal () { sigemptyset (&set); sigaddset (&set, SIGPIPE); sigset_t pending; sigpending (&pending); had = sigismember (&pending, SIGPIPE); pthread_sigmask (SIG_BLOCK, &set, &old); }
    ~PipeSignal () { if (!had) { timespec zero {}; while (sigtimedwait (&set, nullptr, &zero) < 0 && errno == EINTR) {} } pthread_sigmask (SIG_SETMASK, &old, nullptr); }
  } pipe_signal;
  while (!eof || !child.reaped) {
    if ((cancel.load (std::memory_order_relaxed) || FilterLifetime::stopping ()) && !stopping) {
      stopping = true; stop_time = Clock::now (); child.input.reset (); child.signal_group (SIGTERM);
    }
    if (stopping && !killed && Clock::now () - stop_time >= std::chrono::milliseconds (250)) {
      child.signal_group (SIGKILL); killed = true;
    }
    child.observe_exit ();
    if (!stopping && !child.reaped && child.input.value >= 0 && sent == outgoing.size ()) {
      if (input_offset < total) {
        const auto count = std::size_t (std::min<std::uint64_t> (FilterWire::pixel_limit, total - input_offset));
        FilterWire::Frame frame {FilterWire::Type::input, input_offset, std::vector<std::uint8_t> (count)};
        input.read (input_offset, count, frame.payload.data ()); outgoing = FilterWire::encode (frame);
        input_offset += count; sent = 0;
      } else if (!sealed) {
        outgoing = FilterWire::encode ({FilterWire::Type::input_end, total, {}}); sent = 0; sealed = true;
      } else child.input.reset ();
    }
    pollfd descriptors[2] {{child.input.value, short (POLLOUT), 0}, {child.output.value, short (POLLIN), 0}};
    int ready; do { ready = poll (descriptors, 2, 10); } while (ready < 0 && errno == EINTR);
    if (ready < 0) throw std::runtime_error ("Filter pipe poll failed");
    if (child.input.value >= 0 && descriptors[0].revents) {
      const auto count = write (child.input.value, outgoing.data () + sent, outgoing.size () - sent);
      if (count > 0) sent += std::size_t (count);
      else if (count < 0 && errno != EINTR && errno != EAGAIN) child.input.reset ();
    }
    if (child.output.value >= 0 && descriptors[1].revents) {
      std::array<std::uint8_t, FilterWire::pixel_limit> chunk;
      const auto count = read (child.output.value, chunk.data (), std::min (chunk.size (), wanted - incoming.size ()));
      if (count > 0) {
        incoming.insert (incoming.end (), chunk.begin (), chunk.begin () + count);
        if (incoming.size () == FilterWire::header_size) wanted = FilterWire::header_size + FilterWire::payload_size (incoming.data ());
        if (incoming.size () == wanted) {
          const auto frame = FilterWire::decode (incoming.data (), incoming.size ());
          result.accept (frame);
          if (frame.type == FilterWire::Type::output) output.write (frame.offset, frame.payload.size (), frame.payload.data ());
          incoming.clear (); wanted = FilterWire::header_size;
        }
      } else if (count == 0) { eof = true; child.output.reset (); }
      else if (errno != EINTR && errno != EAGAIN) throw std::runtime_error ("Filter output read failed");
    }
  }
  if (stopping) { if (!profile.clear ()) throw std::runtime_error ("Private Filter profile cleanup failed"); return false; }
  if (!incoming.empty ()) throw std::runtime_error ("Truncated Filter process result");
  result.finish ();
  if (!sealed || sent != outgoing.size () || input_offset != total)
    throw std::runtime_error ("Filter child succeeded before exact input transfer completed");
  if (!WIFEXITED (child.status) || WEXITSTATUS (child.status) != 0)
    throw std::runtime_error ("Filter child did not exit successfully");
  output.flush ();
  if (!profile.clear ()) throw std::runtime_error ("Private Filter profile cleanup failed");
  if (cancel.load (std::memory_order_relaxed) || FilterLifetime::stopping ()) return false;
  if (outcome) outcome->publish (result.disposition ());
  return true;
#endif
}
}
