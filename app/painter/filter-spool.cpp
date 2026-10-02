/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "filter-spool.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <condition_variable>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <utility>
namespace GimpPainter {
namespace {
/* SPSC ownership transfer. Each side owns its cursor; neither side touches a
 * handed-over Chunk again. Destruction happens after both sides have stopped. */
class ChunkQueue
{
public:
  ChunkQueue () noexcept { for (auto& slot : slots_) slot.store (nullptr,std::memory_order_relaxed); }
  ~ChunkQueue () noexcept { for (auto& slot : slots_) delete slot.load (); }
  bool available () const noexcept { return slots_[write_ % 2].load (std::memory_order_acquire) == nullptr; }
  bool readable () const noexcept { return slots_[read_ % 2].load (std::memory_order_acquire) != nullptr; }
  bool push (std::unique_ptr<FilterSpool::Chunk>& chunk) noexcept
  {
    auto& slot = slots_[write_ % 2];
    if (slot.load (std::memory_order_acquire)) return false;
    slot.store (chunk.release (), std::memory_order_release); ++write_;
    return true;
  }
  std::unique_ptr<FilterSpool::Chunk> pop () noexcept
  {
    auto& slot = slots_[read_ % 2];
    auto *chunk = slot.exchange (nullptr, std::memory_order_acq_rel);
    if (!chunk) return {};
    ++read_; return std::unique_ptr<FilterSpool::Chunk> (chunk);
  }
private:
  std::array<std::atomic<FilterSpool::Chunk *>,2> slots_;
  std::size_t write_ = 0, read_ = 0;
};
}
struct FilterSpool::State
{
  State (std::size_t w, std::size_t h, std::string path, Process processor, WorkAdmission::Lease admitted)
    : lease (std::move (admitted)), width (w), height (h), directory (std::move (path)), process (std::move (processor)) {}
  // Release admission only after all buffers/closures have been destroyed.
  WorkAdmission::Lease lease;
  std::size_t width, height;
  std::string directory;
  Process process;
  ChunkQueue input, output;
  std::atomic<bool> cancelled {false}, sealed {false}, started {false}, done {false};
  std::atomic<Phase> phase {Phase::collecting};
  std::condition_variable wake;
  std::mutex wait_mutex; // only the worker locks; the owner only notifies
  bool success = false;
  std::string error;
  template<class Predicate> void wait (Predicate ready)
  {
    std::unique_lock<std::mutex> lock (wait_mutex);
    wake.wait_for (lock, std::chrono::milliseconds (10), [&] {
      return cancelled.load (std::memory_order_relaxed) || ready ();
    });
  }
  bool run ()
  {
    if (cancelled.load (std::memory_order_relaxed)) return false;
    const auto pixels = width * height;
    TemporaryFilterRaster snapshot (directory, std::uint64_t (pixels) * 4);
    TemporaryFilterRaster result (directory, std::uint64_t (pixels) * 4);
    std::size_t received = 0;
    for (;;)
      {
        if (cancelled.load (std::memory_order_relaxed)) return false;
        auto chunk = input.pop ();
        if (chunk)
          {
            const auto count = chunk->bytes.size () / 4;
            if (chunk->offset != received || !count || chunk->bytes.size () % 4 ||
                count > pixel_budget || count > pixels - received)
              throw std::invalid_argument ("Invalid input spool chunk");
            snapshot.write (std::uint64_t (received) * 4, chunk->bytes.size (), chunk->bytes.data ());
            received += count;
          }
        else if (sealed.load (std::memory_order_acquire))
          {
            // Acquire the seal before rechecking a producer's last publication.
            if (input.readable ()) continue;
            if (received != pixels) throw std::invalid_argument ("Incomplete input spool");
            break;
          }
        else wait ([&] { return input.readable () || sealed.load (std::memory_order_acquire); });
      }
    snapshot.flush ();
    if (cancelled.load (std::memory_order_relaxed)) return false;
    started.store (true, std::memory_order_release);
    phase.store (Phase::processing, std::memory_order_release);
    FilterRasterFactory factory = [this] (std::uint64_t bytes) {
      return std::unique_ptr<FilterRaster> (new TemporaryFilterRaster (directory,bytes));
    };
    if (!process (snapshot,result,cancelled,factory) || cancelled.load (std::memory_order_relaxed)) return false;
    result.flush (); // never export a deferred write error as a complete result
    phase.store (Phase::exporting, std::memory_order_release);
    for (std::size_t offset = 0; offset < pixels;)
      {
        if (cancelled.load (std::memory_order_relaxed)) return false;
        const auto x = offset % width;
        const auto count = x || width > pixel_budget ? std::min ({pixels - offset, width - x, pixel_budget}) :
                           std::min (pixels - offset, (pixel_budget / width) * width);
        std::unique_ptr<Chunk> chunk (new Chunk {offset,Bytes (count * 4)});
        result.read (std::uint64_t (offset) * 4, count * 4, chunk->bytes.data ());
        while (!output.push (chunk))
          {
            if (cancelled.load (std::memory_order_relaxed)) return false;
            wait ([&] { return output.available (); });
          }
        offset += count;
      }
    return !cancelled.load (std::memory_order_relaxed);
  }
};
FilterSpool::FilterSpool (std::size_t width, std::size_t height, const std::string& directory,
                         Process process, WorkAdmission::Lease lease)
{
  if (!width || !height || width > std::numeric_limits<std::size_t>::max () / height ||
      std::uint64_t (width) > std::uint64_t (std::numeric_limits<std::int64_t>::max ()) / 4 / height || !process)
    throw std::invalid_argument ("Invalid filter spool request");
  auto state = std::make_shared<State> (width,height,directory,std::move (process),std::move (lease));
  std::thread worker ([state] {
    try { state->success = state->run (); }
    catch (const std::exception& e) { try { state->error = e.what (); } catch (...) {} }
    catch (...) { try { state->error = "Unknown filter spool failure"; } catch (...) {} }
    // run() has destroyed all worker-only files before completion is visible.
    state->phase.store (Phase::complete, std::memory_order_release);
    state->done.store (true, std::memory_order_release);
  });
  state_ = std::move (state); worker.detach ();
}
FilterSpool::~FilterSpool () noexcept { cancel (); }
bool FilterSpool::can_submit () const noexcept
{ return owner_ == std::this_thread::get_id () && !sealed_ && !state_->cancelled.load (std::memory_order_relaxed) && !done () && state_->input.available (); }
bool FilterSpool::submit (std::size_t offset, Bytes& bytes)
{
  if (owner_ != std::this_thread::get_id ()) throw std::logic_error ("Filter spool producer used outside owner thread");
  const auto count = bytes.size () / 4;
  if (offset != submitted_ || bytes.size () % 4 || !count || count > pixel_budget ||
      count > state_->width * state_->height - submitted_ || sealed_)
    throw std::invalid_argument ("Invalid owner spool chunk");
  if (!can_submit ()) return false;
  std::unique_ptr<Chunk> chunk (new Chunk {offset,std::move (bytes)});
  if (!state_->input.push (chunk)) throw std::logic_error ("Input spool has multiple producers");
  submitted_ += count; state_->wake.notify_one (); return true;
}
void FilterSpool::seal_input ()
{
  if (owner_ != std::this_thread::get_id ()) throw std::logic_error ("Filter spool sealed outside owner thread");
  if (submitted_ != state_->width * state_->height || sealed_)
    throw std::invalid_argument ("Cannot seal incomplete or already sealed filter input");
  sealed_ = true; state_->sealed.store (true,std::memory_order_release); state_->wake.notify_one ();
}
std::unique_ptr<FilterSpool::Chunk> FilterSpool::take_result () noexcept
{
  if (owner_ != std::this_thread::get_id ()) return {};
  auto result = state_->output.pop (); if (result) state_->wake.notify_one (); return result;
}
void FilterSpool::cancel () noexcept
{ if (state_) { state_->cancelled.store (true,std::memory_order_relaxed); state_->wake.notify_one (); } }
FilterSpool::Phase FilterSpool::phase () const noexcept { return state_->phase.load (std::memory_order_acquire); }
bool FilterSpool::started () const noexcept { return state_->started.load (std::memory_order_acquire); }
bool FilterSpool::done () const noexcept { return state_->done.load (std::memory_order_acquire); }
bool FilterSpool::succeeded () const noexcept { return done () && state_->success; }
const std::string& FilterSpool::error () const
{ if (!done ()) throw std::logic_error ("Filter spool error read before completion"); return state_->error; }
} // namespace GimpPainter
