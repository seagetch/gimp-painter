/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_FAIR_DISPATCHER_HPP
#define GIMP_PAINTER_FAIR_DISPATCHER_HPP
#include "source.hpp"
#include <algorithm>
#include <deque>
#include <functional>
#include <memory>
#include <utility>

namespace GimpPainter {
/* One Source, one bounded callback per dispatch, FIFO across independent owners.
 * Callbacks must do their own bounded quantum and return true to rotate again.
 * The dispatcher owns no GObject; adapters capture weak generation tickets.
 * Tickets, including their destruction, are confined to the owner thread. */
class FairDispatcher
{
  struct Entry
  {
    explicit Entry (std::function<bool ()> f) : function (std::move (f)) {}
    std::function<bool ()> function;
    bool queued = false, cancelled = false;
  };
  struct State : std::enable_shared_from_this<State>
  {
    State (GMainContext *context, guint interval, int priority)
      : context (g_main_context_ref (context ? context : g_main_context_default ())),
        thread (g_thread_ref (g_thread_self ())), interval (interval), priority (priority) {}
    ~State () noexcept
    { source.close (); g_main_context_unref (context); g_thread_unref (thread); }
    void enqueue (const std::shared_ptr<Entry>& entry)
    {
      if (g_thread_self () != thread)
        throw Error (GIMP_PAINTER_ERROR_WRONG_THREAD, "Dispatcher used outside its owner thread");
      if (entry->queued || entry->cancelled) return;
      queue.push_back (entry);
      entry->queued = true;
      if (!source.active ())
        {
          std::weak_ptr<State> weak = shared_from_this ();
          try
            {
              source = Source::timeout (context, interval, priority, [weak] {
                auto state = weak.lock ();
                return state && state->dispatch ();
              });
            }
          catch (...)
            {
              queue.erase (std::remove (queue.begin (), queue.end (), entry), queue.end ());
              entry->queued = false;
              throw;
            }
        }
    }
    void cancel (const std::shared_ptr<Entry>& entry) noexcept
    {
      entry->cancelled = true;
      entry->queued = false;
      queue.erase (std::remove (queue.begin (), queue.end (), entry), queue.end ());
      if (queue.empty ()) source.close ();
    }
    bool dispatch ()
    {
      if (queue.empty ()) return false;
      auto entry = queue.front ();
      queue.pop_front ();
      entry->queued = false;
      if (!entry->cancelled)
        {
          try { if (entry->function ()) enqueue (entry); }
          catch (const std::exception& error)
            { cancel (entry); g_warning ("Painter dispatch failed: %s", error.what ()); }
          catch (...)
            { cancel (entry); g_warning ("Painter dispatch failed: unknown exception"); }
        }
      return !queue.empty ();
    }
    GMainContext *context;
    GThread *thread;
    guint interval;
    int priority;
    Source source;
    std::deque<std::shared_ptr<Entry>> queue;
  };
public:
  class Ticket
  {
  public:
    Ticket () noexcept = default;
    ~Ticket () noexcept { close (); }
    Ticket (const Ticket&) = delete;
    Ticket& operator= (const Ticket&) = delete;
    Ticket (Ticket&& other) noexcept : state_ (std::move (other.state_)), entry_ (std::move (other.entry_)) {}
    Ticket& operator= (Ticket&& other) noexcept
    {
      if (this != &other) { Ticket moved (std::move (other)); swap (moved); }
      return *this;
    }
    void swap (Ticket& other) noexcept
    { state_.swap (other.state_); entry_.swap (other.entry_); }
    bool valid () const noexcept { return entry_ && !entry_->cancelled && !state_.expired (); }
    void schedule ()
    { auto state = state_.lock (); if (state && entry_) state->enqueue (entry_); }
    void close () noexcept
    {
      auto entry = std::move (entry_);
      auto state = state_.lock ();
      state_.reset ();
      if (state && entry) state->cancel (entry);
    }
  private:
    friend class FairDispatcher;
    Ticket (const std::shared_ptr<State>& state, std::shared_ptr<Entry> entry)
      : state_ (state), entry_ (std::move (entry)) {}
    std::weak_ptr<State> state_;
    std::shared_ptr<Entry> entry_;
  };
  explicit FairDispatcher (GMainContext *context = nullptr, guint interval = 2, int priority = 150)
    : state_ (std::make_shared<State> (context, interval, priority)) {}
  FairDispatcher (const FairDispatcher&) = delete;
  FairDispatcher& operator= (const FairDispatcher&) = delete;
  Ticket ticket (std::function<bool ()> function)
  {
    if (!function) throw Error (GIMP_PAINTER_ERROR_INVALID_STATE, "Dispatcher ticket has no callback");
    return Ticket (state_, std::make_shared<Entry> (std::move (function)));
  }
private:
  std::shared_ptr<State> state_;
};
} // namespace GimpPainter
#endif
