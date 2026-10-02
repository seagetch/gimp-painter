/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "filter-scheduler.hpp"
#include <algorithm>
#include <stdexcept>
#include <limits>
#include <thread>
#include <utility>

namespace GimpPainter {
namespace {
std::shared_ptr<WorkAdmission> default_admission ()
{
  /* One pool for the application owner thread. Leases keep independent pool
   * state alive when closed layers leave cancelled workers finishing. */
  static auto pool = std::make_shared<WorkAdmission> (WorkAdmission::Limits {2, 1024 * 1024 * 1024});
  return pool;
}
}
FilterScheduler::FilterScheduler () : admission_ (default_admission ()) {}
FilterScheduler::FilterScheduler (std::shared_ptr<WorkAdmission> admission)
  : admission_ (std::move (admission))
{ if (!admission_) throw std::invalid_argument ("Filter scheduler has no work admission pool"); }
struct FilterScheduler::Job
{
  /* Declare the lease before buffers so it releases after their destruction. */
  WorkAdmission::Lease lease;
  std::atomic<bool> cancelled { false }, done { false };
  Bytes input, output;
  Process process;
  std::unique_ptr<FilterSpool> spool;
  std::unique_ptr<FilterSpool::Chunk> chunk;
  std::size_t chunk_used = 0;
  bool counted = false;
  std::string error;
  bool success = false;
  std::uint64_t generation = 0;
};
void FilterScheduler::advance_generation () noexcept
{
  /* Persisted counters never become live job tokens. Keep generated snapshots
   * inside the wire range even if a runtime epoch is eventually exhausted. */
  if (generation_ >= std::uint64_t (std::numeric_limits<std::int64_t>::max ()))
    { generation_ = 1; cache_generation_ = 0; }
  else
    ++generation_;
}
void FilterScheduler::cancel () noexcept
{
  if (job_)
    {
      job_->cancelled.store (true, std::memory_order_relaxed);
      if (job_->spool) job_->spool->cancel ();
    }
}
void FilterScheduler::release_preparation () noexcept
{
  /* clear() alone retained a whole-raster allocation after edits/close, which
   * would escape admission accounting. Release storage before its lease. */
  Bytes ().swap (input_);
  admission_ticket_.close ();
  admission_lease_.close ();
}
void FilterScheduler::invalidate () noexcept
{
  if (state_ == State::closed) return;
  advance_generation ();
  dirty_ = true;
  error_.clear ();
  cancel ();
  /* Worker completion is a separate checkpoint. Never launch a replacement
   * while an old worker is still running, even after a cancellation request. */
  state_ = job_ ? State::cancelling : State::waiting;
  cursor_ = 0;
  release_preparation ();
}
void FilterScheduler::set_admission (std::shared_ptr<WorkAdmission> pool)
{
  if (!pool) throw std::invalid_argument ("Filter scheduler has no admission pool");
  if (state_ == State::closed || pool == admission_) return;
  // Selecting accounting for an inert/restored cache is not a pixel edit.
  // Only an in-flight preparation/job needs a replacement generation.
  if (job_ || admission_ticket_ || admission_lease_) invalidate ();
  admission_ = std::move (pool);
}
void FilterScheduler::set_request (Request request)
{ if (state_ != State::closed) { request_ = std::move (request); invalidate (); } }
void FilterScheduler::mark_loaded () noexcept
{
  if (state_ == State::closed) return;
  advance_generation ();
  cancel ();
  dirty_ = false;
  cache_generation_ = generation_;
  cache_complete_ = true;
  state_ = job_ ? State::cancelling : State::clean;
  cursor_ = 0;
  release_preparation ();
  error_.clear ();
}
void FilterScheduler::restore_cache (const Snapshot& saved)
{
  if (state_ == State::closed)
    throw std::invalid_argument ("Cannot restore a closed filter scheduler");
  if (saved.cache_generation > saved.generation ||
      saved.generation > std::uint64_t (std::numeric_limits<std::int64_t>::max ()))
    throw std::invalid_argument ("Invalid saved filter generations");
  cancel ();
  advance_generation ();
  cache_complete_ = saved.cache_complete;
  dirty_ = !cache_complete_ || saved.generation != saved.cache_generation;
  /* The stored relationship carries meaning; the absolute persisted counter
   * is untrusted diagnostic lineage, not a runtime freshness capability. */
  cache_generation_ = dirty_ ? 0 : generation_;
  cursor_ = 0;
  release_preparation ();
  error_.clear ();
  state_ = job_ ? State::cancelling : dirty_ ? State::waiting : State::clean;
}
void FilterScheduler::reject (const char *message) noexcept
{ if (state_ != State::closed) { advance_generation (); cancel (); fail (message); } }
void FilterScheduler::close () noexcept
{
  if (state_ == State::closed) return;
  advance_generation ();
  cancel ();
  job_.reset (); // worker owns the job independently; no join/wait or UI access
  release_preparation ();
  dirty_ = false;
  state_ = State::closed;
}
void FilterScheduler::fail (const char *message) noexcept
{
  cancel ();
  try { error_ = message; } catch (...) {}
  dirty_ = false; // a new edit may retry; idle time alone never retries failure
  release_preparation ();
  cursor_ = 0;
  state_ = State::failed;
}
std::size_t FilterScheduler::next_count (std::size_t offset) const noexcept
{
  const auto remaining = request_.width * request_.height - offset;
  const auto x = offset % request_.width;
  if (x || request_.width > pixel_budget_)
    return std::min ({remaining, request_.width - x, pixel_budget_});
  return std::min (remaining, (pixel_budget_ / request_.width) * request_.width);
}
bool FilterScheduler::step (bool ready, const Read& read, const Import& import,
                            const Commit& commit) noexcept
{
  if (state_ == State::closed) return false;
  if (stepping_) return dirty_ || bool (job_);
  struct StepGuard
  {
    bool& active;
    explicit StepGuard (bool& value) : active (value) { active = true; }
    ~StepGuard () { active = false; }
  } guard (stepping_);
  const auto operation_generation = generation_;
  try
    {
      if (admission_->closed () && state_ != State::failed) reject ("Filter resource configuration is closed");
      if (job_ && job_->spool)
        {
          auto& spool = *job_->spool;
          if (spool.started () && !job_->counted) { job_->counted = true; ++starts_; }
          if (state_ == State::failed && !dirty_)
            { if (!spool.done ()) return true; job_.reset (); return false; }
          if (job_->generation != generation_ || job_->cancelled.load (std::memory_order_relaxed))
            {
              if (!spool.done ()) return true;
              job_.reset (); state_ = dirty_ ? State::waiting : State::clean;
            }
          else if (spool.done () && !spool.succeeded ())
            {
              const std::string message = spool.error ().empty () ? "Filter spool failed or was cancelled" : spool.error ();
              job_.reset (); fail (message.c_str ()); return false;
            }
          else if (spool.phase () == FilterSpool::Phase::exporting || spool.done ())
            { if (state_ != State::importing) { state_ = State::importing; cursor_ = 0; } }
          else if (spool.started ())
            { state_ = State::running; return true; }
        }
      if (job_ && !job_->spool && state_ != State::importing)
        {
          if (!job_->done.load (std::memory_order_acquire)) return true;
          if (state_ == State::failed && !dirty_) { job_.reset (); return false; }
          if (job_->generation != generation_ || job_->cancelled.load (std::memory_order_relaxed))
            { job_.reset (); state_ = dirty_ ? State::waiting : State::clean; }
          else if (!job_->success)
            { const std::string message = job_->error; job_.reset (); fail (message.c_str ()); return false; }
          else
            {
              if (job_->output.size () != request_.width * request_.height * 4)
                { job_.reset (); fail ("Filter returned an invalid result size"); return false; }
              state_ = State::importing;
              cursor_ = 0;
            }
        }
      if (!dirty_) return false;
      if (!ready)
        {
          /* A spill worker may still be draining output. Cancel it and retain
           * its lease until completion before replacing the generation. */
          if (job_ && job_->spool) { invalidate (); return true; }
          /* A dependency can become dirty even without replacing our request. */
          if (state_ == State::importing) job_.reset ();
          release_preparation (); cursor_ = 0; state_ = State::waiting;
          return false;
        }
      if (state_ == State::waiting)
        {
          if (!request_.width || !request_.height ||
              request_.width > std::numeric_limits<std::size_t>::max () / request_.height ||
              std::uint64_t (request_.width) > std::uint64_t (std::numeric_limits<std::int64_t>::max ()) / 4 / request_.height ||
              (!request_.raster_process && request_.width > maximum_pixels / request_.height))
            { fail ("Filter input exceeds the bounded raster size"); return false; }
          if (!request_.process && !request_.raster_process)
            { fail ("Saved filter procedure or argument mapping is unsupported"); return false; }
          if (!admission_ticket_)
            admission_ticket_ = admission_->request (std::max (request_.peak_bytes,
                                                              request_.raster_process ? pixel_budget * 4 * 6 : request_.width * request_.height * 8),
              request_.raster_process ? std::max (request_.peak_spill_bytes, std::uint64_t (request_.width) * request_.height * 8) : 0);
          admission_lease_ = admission_ticket_.try_acquire ();
          /* Resource contention is waiting, never a failure or a lost dirty
           * generation. The owner's normal paced dispatcher retries fairly. */
          if (!admission_lease_) return true;
          cursor_ = 0;
          work_generation_ = generation_;
          if (request_.raster_process)
            {
              auto job = std::make_shared<Job> ();
              job->generation = work_generation_;
              job->spool.reset (new FilterSpool (request_.width, request_.height, request_.spool_directory,
                                                 request_.raster_process, std::move (admission_lease_)));
              job_ = std::move (job);
            }
          else
            input_.reserve (request_.width * request_.height * 4);
          state_ = State::preparing;
        }
      if (state_ == State::preparing)
        {
          auto collecting = job_; // callback closure cannot destroy an active transport
          if (collecting && collecting->spool &&
              (cursor_ == request_.width * request_.height || !collecting->spool->can_submit ())) return true;
          const auto count = next_count (cursor_);
          const auto token = work_generation_;
          /* A callback can invalidate/close the scheduler. Keep its bounded
           * mutable chunk independent of the admitted aggregate so reentry
           * cannot retain unaccounted storage or invalidate its data pointer. */
          Bytes chunk;
          chunk.reserve (count * 4);
          read (cursor_, count, chunk);
          if (generation_ != token || state_ != State::preparing)
            return state_ != State::closed && (dirty_ || bool (job_));
          if (chunk.size () != count * 4)
            { fail ("Input producer returned an invalid chunk size"); return bool (job_); }
          if (collecting && collecting->spool)
            {
              if (!collecting->spool->submit (cursor_, chunk)) return true;
              cursor_ += count;
              if (cursor_ == request_.width * request_.height) collecting->spool->seal_input ();
              return true;
            }
          input_.insert (input_.end (), chunk.begin (), chunk.end ());
          cursor_ += count;
          if (cursor_ < request_.width * request_.height) return true;
          auto job = std::make_shared<Job> ();
          job->lease = std::move (admission_lease_);
          job->input = std::move (input_);
          job->process = request_.process;
          job->generation = work_generation_;
          std::thread worker ([job] {
            try { job->success = job->process (job->input, job->cancelled, job->output); }
            catch (const std::exception& e) { try { job->error = e.what (); } catch (...) {} }
            catch (...) { try { job->error = "Filter worker threw an unknown exception"; } catch (...) {} }
            if (!job->success && job->error.empty ())
              { try { job->error = "Filter execution failed or was cancelled"; } catch (...) {} }
            job->done.store (true, std::memory_order_release);
          });
          job_ = std::move (job);
          worker.detach ();
          ++starts_;
          state_ = State::running;
          return true;
        }
      if (state_ == State::importing)
        {
          const auto token = generation_;
          /* Keep worker bytes alive if a main-thread callback closes us. */
          auto job = job_;
          if (cursor_ < request_.width * request_.height)
            {
              auto count = next_count (cursor_);
              const std::uint8_t *pixels;
              if (job->spool)
                {
                  if (!job->chunk) { job->chunk = job->spool->take_result (); job->chunk_used = 0; }
                  if (!job->chunk)
                    {
                      if (job->spool->done ()) throw std::runtime_error ("Spool returned an incomplete result");
                      return true;
                    }
                  if (job->chunk->bytes.empty () || job->chunk->bytes.size () % 4 ||
                      job->chunk->offset > request_.width * request_.height ||
                      job->chunk_used >= job->chunk->bytes.size () / 4 ||
                      job->chunk_used > request_.width * request_.height - job->chunk->offset ||
                      job->chunk->offset + job->chunk_used != cursor_ ||
                      job->chunk->bytes.size () / 4 > request_.width * request_.height - job->chunk->offset)
                    throw std::runtime_error ("Spool returned an invalid result chunk");
                  count = std::min (count, job->chunk->bytes.size () / 4 - job->chunk_used);
                  pixels = job->chunk->bytes.data () + job->chunk_used * 4;
                }
              else pixels = job->output.data () + cursor_ * 4;
              import (cursor_, count, pixels);
              if (state_ == State::closed) return false;
              if (token != generation_ || state_ != State::importing) return dirty_ || bool (job_);
              cursor_ += count;
              if (job->spool)
                {
                  job->chunk_used += count;
                  if (job->chunk_used == job->chunk->bytes.size () / 4) job->chunk.reset ();
                }
            }
          if (cursor_ < request_.width * request_.height) return true;
          if (job->spool && !job->spool->done ()) return true;
          if (job->spool && !job->spool->succeeded ())
            throw std::runtime_error ("Spool result transfer failed");
          /* Publish metadata before the atomic buffer swap emits update
           * signals. If an adapter throws, do not certify an unknown cache. */
          const auto previous_cache_generation = cache_generation_;
          dirty_ = false; state_ = State::clean; cache_generation_ = token; cache_complete_ = true;
          job_.reset ();
          try { commit (token); }
          catch (...)
            {
              if (state_ != State::closed)
                {
                  cache_complete_ = false;
                  if (cache_generation_ == token) cache_generation_ = previous_cache_generation;
                  if (generation_ != token && state_ == State::clean)
                    { dirty_ = true; state_ = State::waiting; }
                }
              throw;
            }
          return dirty_;
        }
      return false;
    }
  catch (const std::exception& e)
    {
      if (state_ != State::closed && generation_ == operation_generation) { cancel (); if (!job_ || !job_->spool || job_->spool->done ()) job_.reset (); fail (e.what ()); }
    }
  catch (...)
    {
      if (state_ != State::closed && generation_ == operation_generation) { cancel (); if (!job_ || !job_->spool || job_->spool->done ()) job_.reset (); fail ("Filter scheduling failed"); }
    }
  /* An obsolete callback failure must not erase an edit made by reentry. */
  return state_ != State::closed && (dirty_ || bool (job_));
}
} // namespace GimpPainter
