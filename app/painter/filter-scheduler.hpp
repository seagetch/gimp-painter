/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_FILTER_SCHEDULER_HPP
#define GIMP_PAINTER_FILTER_SCHEDULER_HPP
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace GimpPainter {
/* Main-thread state machine. Workers receive only owned bytes and a cancellation
 * flag; never a GObject, graph, drawable, callback or borrowed implementation. */
class FilterScheduler
{
public:
  using Bytes = std::vector<std::uint8_t>;
  using Process = std::function<bool (const Bytes&, std::atomic<bool>&, Bytes&)>;
  enum class State { clean, waiting, preparing, running, cancelling, importing, failed, closed };
  struct Request { std::size_t width = 0, height = 0; Process process; };
  struct Snapshot
  {
    std::uint64_t generation = 0, cache_generation = 0;
    bool cache_complete = true;
  };
  /* Offset and count are pixels, always a contiguous portion of one scanline
   * or a whole number of scanlines. Each call is <= pixel_budget. */
  using Read = std::function<void (std::size_t, std::size_t, Bytes&)>;
  using Import = std::function<void (std::size_t, std::size_t, const std::uint8_t *)>;
  using Commit = std::function<void (std::uint64_t)>;
  static constexpr std::size_t pixel_budget = 256 * 128;
  static constexpr std::size_t maximum_pixels = 64 * 1024 * 1024;

  FilterScheduler () = default;
  ~FilterScheduler () noexcept { close (); }
  FilterScheduler (const FilterScheduler&) = delete;
  FilterScheduler& operator= (const FilterScheduler&) = delete;
  void invalidate () noexcept;
  void set_request (Request request);
  void mark_loaded () noexcept;
  Snapshot snapshot () const noexcept { return {generation_, cache_generation_, cache_complete_}; }
  void restore_cache (const Snapshot&);
  void reject (const char *message) noexcept;
  void close () noexcept;
  /* One bounded main-thread quantum. False means no further quanta are needed.
   * Dependency waiters sleep until their dependency signal schedules a step. */
  bool step (bool dependencies_ready, const Read&, const Import&, const Commit&) noexcept;
  State state () const noexcept { return state_; }
  bool settled () const noexcept { return !job_ && (state_ == State::clean || state_ == State::failed); }
  bool has_worker () const noexcept { return bool (job_); }
  std::uint64_t generation () const noexcept { return generation_; }
  std::uint64_t cache_generation () const noexcept { return cache_generation_; }
  std::uint64_t starts () const noexcept { return starts_; }
  const std::string& error () const noexcept { return error_; }
private:
  struct Job;
  std::size_t next_count (std::size_t offset) const noexcept;
  void advance_generation () noexcept;
  void cancel () noexcept;
  void fail (const char *) noexcept;
  Request request_;
  std::shared_ptr<Job> job_;
  Bytes input_;
  std::size_t cursor_ = 0;
  std::uint64_t generation_ = 0, work_generation_ = 0, cache_generation_ = 0, starts_ = 0;
  State state_ = State::clean;
  bool dirty_ = false, cache_complete_ = true;
  std::string error_;
};
} // namespace GimpPainter
#endif
