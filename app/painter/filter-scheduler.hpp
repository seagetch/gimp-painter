/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_FILTER_SCHEDULER_HPP
#define GIMP_PAINTER_FILTER_SCHEDULER_HPP
#include "work-admission.hpp"
#include "filter-spool.hpp"
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
  struct Request
  {
    std::size_t width = 0, height = 0;
    Process process;
    /* Includes input/result plus executor scratch and adapter staging. Zero
     * declares the two-raster minimum used by simple independent processors. */
    std::size_t peak_bytes = 0;
    /* Optional bounded, worker-owned spill route. Only this route can exceed
     * the whole-vector raster limit. Location is trusted app configuration. */
    FilterSpool::Process raster_process {};
    std::string spool_directory {};
    std::uint64_t peak_spill_bytes = 0; // includes input, result and transpose scratch
    /* Owned packed bytes, not a Babl/GEGL object. RGBA8 defaults to four;
     * normalized-double RGBA uses32. The scheduler never interprets samples. */
    std::size_t bytes_per_pixel = 4;
  };
  struct Snapshot
  {
    std::uint64_t generation = 0, cache_generation = 0;
    bool cache_complete = true;
  };
  /* Offset and count are pixels, always a contiguous portion of one scanline
   * or a whole number of scanlines. Each call is <= pixel_budget. Read receives
   * an empty, independently owned chunk and must append exactly count*request.bytes_per_pixel bytes. */
  using Read = std::function<void (std::size_t, std::size_t, Bytes&)>;
  using Import = std::function<void (std::size_t, std::size_t, const std::uint8_t *)>;
  using Commit = std::function<void (std::uint64_t)>;
  /* Ephemeral owner-thread gates, never retained in a Request or Job. A false
   * return yields this quantum; source/generation reentry is checked afterward.
   * Gates may prepare bounded context only after admission. */
  using Gate = std::function<bool ()>;
  static constexpr std::size_t pixel_budget = 256 * 128;
  static constexpr std::size_t maximum_pixels = 64 * 1024 * 1024;

  FilterScheduler ();
  explicit FilterScheduler (std::shared_ptr<WorkAdmission>);
  ~FilterScheduler () noexcept { close (); }
  FilterScheduler (const FilterScheduler&) = delete;
  FilterScheduler& operator= (const FilterScheduler&) = delete;
  /* The adapter may tune quantum size between chunks; this changes neither
   * raster geometry nor generation. Zero clamps to one, oversized to the cap. */
  void set_pixel_budget (std::size_t pixels) noexcept
  { pixel_budget_ = pixels == 0 ? 1 : pixels > pixel_budget ? pixel_budget : pixels; }
  void set_admission (std::shared_ptr<WorkAdmission>);
  void invalidate () noexcept;
  void set_request (Request request);
  bool has_processor () const noexcept { return bool (request_.process) || bool (request_.raster_process); }
  bool uses_spool () const noexcept { return bool (request_.raster_process); }
  void set_spool_directory (std::string directory)
  { if (state_ != State::closed) request_.spool_directory = std::move (directory); }
  void mark_loaded () noexcept;
  Snapshot snapshot () const noexcept { return {generation_, cache_generation_, cache_complete_}; }
  void restore_cache (const Snapshot&);
  void reject (const char *message) noexcept;
  void close () noexcept;
  /* One bounded main-thread quantum. False means no further quanta are needed.
   * Dependency waiters sleep until their dependency signal schedules a step. */
  bool step (bool dependencies_ready, const Read&, const Import&, const Commit&,
             const Gate& before_process = {}, const Gate& before_import = {}) noexcept;
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
  void release_preparation () noexcept;
  void fail (const char *) noexcept;
  std::shared_ptr<WorkAdmission> admission_;
  WorkAdmission::Ticket admission_ticket_;
  WorkAdmission::Lease admission_lease_;
  Request request_;
  std::shared_ptr<Job> job_;
  Bytes input_;
  std::size_t cursor_ = 0, pixel_budget_ = pixel_budget;
  std::uint64_t generation_ = 0, work_generation_ = 0, cache_generation_ = 0, starts_ = 0;
  State state_ = State::clean;
  bool dirty_ = false, cache_complete_ = true, stepping_ = false;
  bool input_sealed_ = false, import_ready_ = false;
  std::string error_;
};
} // namespace GimpPainter
#endif
