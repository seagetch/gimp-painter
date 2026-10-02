/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_WORK_ADMISSION_HPP
#define GIMP_PAINTER_WORK_ADMISSION_HPP
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <list>
#include <memory>
#include <stdexcept>
#include <thread>
#include <utility>

namespace GimpPainter {
/* FIFO admission for independent worker snapshots. Requests and acquisition
 * belong to one owner thread; a lease may be released on any worker thread.
 * It contains no callbacks, GObjects or alternate implementation storage.
 * Declared bytes must cover the executor's peak working storage. This cannot
 * measure or constrain an arbitrary callable that violates that contract. */
class WorkAdmission
{
public:
  struct Limits { std::size_t jobs, bytes; std::uint64_t spill_bytes = std::numeric_limits<std::uint64_t>::max (); };
private:
  struct Entry;
  using Queue = std::list<std::shared_ptr<Entry>>;
  struct Entry
  {
    Entry (std::size_t bytes, std::uint64_t spill) : bytes (bytes), spill_bytes (spill) {}
    std::size_t bytes;
    std::uint64_t spill_bytes;
    Queue::iterator position;
    bool queued = false; // owner thread only
    std::atomic<bool> cancelled { false };
  };
  struct State
  {
    explicit State (Limits limits) : limits (limits), owner (std::this_thread::get_id ()) {}
    void check_owner () const
    {
      if (owner != std::this_thread::get_id ())
        throw std::logic_error ("Worker admission used outside its owner thread");
    }
    Limits limits;
    std::thread::id owner;
    std::atomic<std::size_t> jobs { 0 }, bytes { 0 };
    std::atomic<std::uint64_t> spill_bytes { 0 };
    std::atomic<bool> closed { false };
    Queue queue;
  };
public:
  class Lease
  {
  public:
    Lease () noexcept = default;
    ~Lease () noexcept { close (); }
    Lease (const Lease&) = delete;
    Lease& operator= (const Lease&) = delete;
    Lease (Lease&& other) noexcept : state_ (std::move (other.state_)), bytes_ (other.bytes_), spill_bytes_ (other.spill_bytes_) {}
    Lease& operator= (Lease&& other) noexcept
    { if (this != &other) { Lease moved (std::move (other)); swap (moved); } return *this; }
    void swap (Lease& other) noexcept
    { state_.swap (other.state_); std::swap (bytes_, other.bytes_); std::swap (spill_bytes_, other.spill_bytes_); }
    explicit operator bool () const noexcept { return bool (state_); }
    std::uint64_t reserved_spill_bytes () const noexcept { return state_ ? spill_bytes_ : 0; }
    void close () noexcept
    {
      auto state = std::move (state_);
      if (state)
        {
          /* Publish free bytes before the free job slot. A concurrent owner
           * may conservatively defer acquisition, but cannot over-admit. */
          state->bytes.fetch_sub (bytes_, std::memory_order_release);
          state->spill_bytes.fetch_sub (spill_bytes_, std::memory_order_release);
          state->jobs.fetch_sub (1, std::memory_order_release);
        }
    }
  private:
    friend class WorkAdmission;
    Lease (std::shared_ptr<State> state, std::size_t bytes, std::uint64_t spill) noexcept
      : state_ (std::move (state)), bytes_ (bytes), spill_bytes_ (spill)
    {
      state_->bytes.fetch_add (bytes_, std::memory_order_relaxed);
      state_->spill_bytes.fetch_add (spill_bytes_, std::memory_order_relaxed);
      state_->jobs.fetch_add (1, std::memory_order_relaxed);
    }
    std::shared_ptr<State> state_;
    std::size_t bytes_ = 0;
    std::uint64_t spill_bytes_ = 0;
  };
  class Ticket
  {
  public:
    Ticket () noexcept = default;
    ~Ticket () noexcept { close (); }
    Ticket (const Ticket&) = delete;
    Ticket& operator= (const Ticket&) = delete;
    Ticket (Ticket&& other) noexcept : state_ (std::move (other.state_)), entry_ (std::move (other.entry_)) {}
    Ticket& operator= (Ticket&& other) noexcept
    { if (this != &other) { Ticket moved (std::move (other)); swap (moved); } return *this; }
    void swap (Ticket& other) noexcept { state_.swap (other.state_); entry_.swap (other.entry_); }
    explicit operator bool () const noexcept
    { return entry_ && !entry_->cancelled.load (std::memory_order_relaxed); }
    void close () noexcept
    {
      auto entry = std::move (entry_);
      auto state = std::move (state_);
      if (entry)
        {
          entry->cancelled.store (true, std::memory_order_release);
          /* Normal owner cancellation unlinks in O(1); repeated edits behind
           * a busy front request cannot accumulate expired queue nodes. */
          if (state && state->owner == std::this_thread::get_id () && entry->queued)
            { entry->queued = false; state->queue.erase (entry->position); }
        }
    }
    Lease try_acquire ()
    {
      if (!entry_) return {};
      state_->check_owner ();
      if (state_->closed.load (std::memory_order_acquire))
        { close (); throw std::runtime_error ("Worker admission owner is closed"); }
      if (entry_->bytes > state_->limits.bytes || entry_->spill_bytes > state_->limits.spill_bytes)
        { close (); throw std::invalid_argument ("Worker request exceeds updated memory or spill limit"); }
      while (!state_->queue.empty ())
        {
          auto front = state_->queue.front ();
          if (!front->cancelled.load (std::memory_order_acquire)) break;
          front->queued = false;
          state_->queue.pop_front ();
        }
      if (state_->queue.empty () || state_->queue.front () != entry_) return {};
      const auto jobs = state_->jobs.load (std::memory_order_acquire);
      const auto bytes = state_->bytes.load (std::memory_order_acquire);
      const auto spill = state_->spill_bytes.load (std::memory_order_acquire);
      // A lowered limit never subtracts a larger live reservation and wraps.
      if (jobs >= state_->limits.jobs || bytes > state_->limits.bytes || spill > state_->limits.spill_bytes ||
          entry_->bytes > state_->limits.bytes - bytes || entry_->spill_bytes > state_->limits.spill_bytes - spill) return {};
      Lease lease (state_, entry_->bytes, entry_->spill_bytes);
      entry_->queued = false;
      state_->queue.pop_front ();
      close ();
      return lease;
    }
  private:
    friend class WorkAdmission;
    Ticket (const std::shared_ptr<State>& state, std::shared_ptr<Entry> entry)
      : state_ (state), entry_ (std::move (entry)) {}
    std::shared_ptr<State> state_;
    std::shared_ptr<Entry> entry_;
  };
  explicit WorkAdmission (Limits limits) : state_ (std::make_shared<State> (limits))
  {
    if (!limits.jobs || !limits.bytes) throw std::invalid_argument ("Worker admission limits must be positive");
  }
  WorkAdmission (const WorkAdmission&) = delete;
  WorkAdmission& operator= (const WorkAdmission&) = delete;
  Ticket request (std::size_t bytes, std::uint64_t spill_bytes = 0)
  {
    state_->check_owner ();
    if (closed ()) throw std::runtime_error ("Worker admission owner is closed");
    if (!bytes || bytes > state_->limits.bytes)
      throw std::invalid_argument ("Worker snapshot exceeds admission memory limit");
    if (spill_bytes > state_->limits.spill_bytes)
      throw std::invalid_argument ("Worker raster exceeds admission spill-space limit");
    auto entry = std::make_shared<Entry> (bytes, spill_bytes);
    entry->position = state_->queue.insert (state_->queue.end (), entry);
    entry->queued = true;
    return Ticket (state_, std::move (entry));
  }
  std::size_t pending_requests () const
  { state_->check_owner (); return state_->queue.size (); }
  // Changes affect queued/new work. Existing leases retain their complete
  // reservations until worker storage is closed; no callback or join occurs.
  void configure (Limits limits)
  {
    state_->check_owner ();
    if (!limits.jobs || !limits.bytes) throw std::invalid_argument ("Worker admission limits must be positive");
    if (closed ()) throw std::runtime_error ("Worker admission owner is closed");
    state_->limits = limits;
  }
  void close () noexcept { state_->closed.store (true, std::memory_order_release); }
  bool closed () const noexcept { return state_->closed.load (std::memory_order_acquire); }
  Limits limits () const { state_->check_owner (); return state_->limits; }
  std::uint64_t active_spill_bytes () const noexcept { return state_->spill_bytes.load (std::memory_order_acquire); }
  std::size_t active_jobs () const noexcept { return state_->jobs.load (std::memory_order_acquire); }
  std::size_t active_bytes () const noexcept { return state_->bytes.load (std::memory_order_acquire); }
private:
  std::shared_ptr<State> state_;
};
} // namespace GimpPainter
#endif
