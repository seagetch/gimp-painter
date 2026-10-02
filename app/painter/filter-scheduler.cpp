/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "filter-scheduler.hpp"
#include <algorithm>
#include <stdexcept>
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
void FilterScheduler::cancel () noexcept
{ if (job_) job_->cancelled.store (true, std::memory_order_relaxed); }
void FilterScheduler::invalidate () noexcept
{
  if (state_ == State::closed) return;
  ++generation_;
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
  ++generation_;
  cancel ();
  dirty_ = false;
  cache_generation_ = generation_;
  state_ = job_ ? State::cancelling : State::clean;
  cursor_ = 0;
  input_.clear ();
  error_.clear ();
}
void FilterScheduler::reject (const char *message) noexcept
{ if (state_ != State::closed) { ++generation_; cancel (); fail (message); } }
void FilterScheduler::close () noexcept
{
  if (state_ == State::closed) return;
  ++generation_;
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
          /* Publish state before update signals can reenter invalidation. */
          dirty_ = false; state_ = State::clean; cache_generation_ = token;
          job_.reset ();
          commit (token);
          return dirty_;
        }
      return false;
    }
  catch (const std::exception& e) { if (state_ != State::closed) { job_.reset (); fail (e.what ()); } }
  catch (...) { if (state_ != State::closed) { job_.reset (); fail ("Filter scheduling failed"); } }
  return false;
}
} // namespace GimpPainter
