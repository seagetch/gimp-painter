/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "filter-scheduler.hpp"
#include <algorithm>
#include <stdexcept>
#include <limits>
#include <thread>
#include <utility>

namespace GimpPainter {
struct FilterScheduler::Job
{
  std::atomic<bool> cancelled { false }, done { false };
  Bytes input, output;
  Process process;
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
{ if (job_) job_->cancelled.store (true, std::memory_order_relaxed); }
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
  input_.clear ();
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
  input_.clear ();
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
  input_.clear ();
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
  input_.clear ();
  dirty_ = false;
  state_ = State::closed;
}
void FilterScheduler::fail (const char *message) noexcept
{
  try { error_ = message; } catch (...) {}
  dirty_ = false; // a new edit may retry; idle time alone never retries failure
  input_.clear ();
  cursor_ = 0;
  state_ = State::failed;
}
std::size_t FilterScheduler::next_count (std::size_t offset) const noexcept
{
  const auto remaining = request_.width * request_.height - offset;
  const auto x = offset % request_.width;
  if (x || request_.width > pixel_budget)
    return std::min ({remaining, request_.width - x, pixel_budget});
  return std::min (remaining, (pixel_budget / request_.width) * request_.width);
}
bool FilterScheduler::step (bool ready, const Read& read, const Import& import,
                            const Commit& commit) noexcept
{
  if (state_ == State::closed) return false;
  const auto operation_generation = generation_;
  try
    {
      if (job_ && state_ != State::importing)
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
          /* A dependency can become dirty even without replacing our request. */
          if (state_ == State::importing) job_.reset ();
          input_.clear (); cursor_ = 0; state_ = State::waiting;
          return false;
        }
      if (state_ == State::waiting)
        {
          if (!request_.width || !request_.height ||
              request_.width > maximum_pixels / request_.height)
            { fail ("Filter input exceeds the bounded raster size"); return false; }
          if (!request_.process)
            { fail ("Saved filter procedure or argument mapping is unsupported"); return false; }
          input_.clear ();
          input_.reserve (request_.width * request_.height * 4);
          cursor_ = 0;
          work_generation_ = generation_;
          state_ = State::preparing;
        }
      if (state_ == State::preparing)
        {
          const auto count = next_count (cursor_);
          const auto before = input_.size ();
          read (cursor_, count, input_);
          if (generation_ != work_generation_ || state_ != State::preparing)
            return state_ != State::closed && (dirty_ || bool (job_));
          if (input_.size () != before + count * 4)
            { fail ("Input producer returned an invalid chunk size"); return false; }
          cursor_ += count;
          if (cursor_ < request_.width * request_.height) return true;
          auto job = std::make_shared<Job> ();
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
          const auto count = next_count (cursor_);
          /* Keep worker bytes alive if a main-thread callback closes us. */
          auto job = job_;
          import (cursor_, count, job->output.data () + cursor_ * 4);
          if (state_ == State::closed) return false;
          if (token != generation_ || state_ != State::importing) return dirty_ || bool (job_);
          cursor_ += count;
          if (cursor_ < request_.width * request_.height) return true;
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
      if (state_ != State::closed && generation_ == operation_generation) { job_.reset (); fail (e.what ()); }
    }
  catch (...)
    {
      if (state_ != State::closed && generation_ == operation_generation) { job_.reset (); fail ("Filter scheduling failed"); }
    }
  /* An obsolete callback failure must not erase an edit made by reentry. */
  return state_ != State::closed && (dirty_ || bool (job_));
}
} // namespace GimpPainter
