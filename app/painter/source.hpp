/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_SOURCE_HPP
#define GIMP_PAINTER_SOURCE_HPP
#include "gimp-painter-visibility.h"
#include "boundary.hpp"
#include <functional>
#include <memory>
#include <utility>

namespace GimpPainter GIMP_PAINTER_PRIVATE {

class Source
{
  struct State { explicit State (std::function<bool ()> f) : function (std::move (f)) {}
                 std::function<bool ()> function; };
  using Shared = std::shared_ptr<State>;
public:
  Source () noexcept = default;
  ~Source () { close (); }
  Source (const Source&) = delete;
  Source& operator= (const Source&) = delete;
  Source (Source&& other) noexcept : source_ (other.release ()) {}
  Source& operator= (Source&& other) noexcept
  { if (this != &other) { Source moved (std::move (other)); swap (moved); } return *this; }
  void swap (Source& other) noexcept { std::swap (source_, other.source_); }

  static Source idle (GMainContext *context, int priority, std::function<bool ()> function)
  { return attach (g_idle_source_new (), context, priority, std::move (function)); }
  static Source timeout (GMainContext *context, guint milliseconds, int priority,
                         std::function<bool ()> function)
  { return attach (g_timeout_source_new (milliseconds), context, priority, std::move (function)); }

  bool active () const noexcept { return source_ && !g_source_is_destroyed (source_); }
  void close () noexcept
  {
    auto *old = release ();
    if (old) { g_source_destroy (old); g_source_unref (old); }
  }
private:
  explicit Source (GSource *source) noexcept : source_ (source) {}
  GSource *release () noexcept { auto *old = source_; source_ = nullptr; return old; }
  static Source attach (GSource *source, GMainContext *context, int priority,
                        std::function<bool ()> function)
  {
    Source result (source); // guards the GSource if a C++ allocation throws
    if (!function) throw Error (GIMP_PAINTER_ERROR_INVALID_STATE, "Source has no callback");
    std::unique_ptr<Shared> payload (new Shared (std::make_shared<State> (std::move (function))));
    g_source_set_priority (source, priority);
    g_source_set_callback (source, dispatch, payload.release (), destroy);
    if (!g_source_attach (source, context))
      throw Error (GIMP_PAINTER_ERROR_INVALID_STATE, "Cannot attach source");
    return result;
  }
  static gboolean dispatch (gpointer data) noexcept
  {
    /* Source destruction, even from the callback itself, cannot delete the
     * function or its captures until this invocation returns. */
    auto state = *static_cast<Shared *> (data);
    try { return state->function () ? G_SOURCE_CONTINUE : G_SOURCE_REMOVE; }
    catch (const std::exception& error)
      { g_warning ("Painter source callback failed: %s", error.what ()); }
    catch (...) { g_warning ("Painter source callback failed: unknown C++ exception"); }
    return G_SOURCE_REMOVE;
  }
  static void destroy (gpointer data) noexcept { delete static_cast<Shared *> (data); }
  GSource *source_ = nullptr;
};
} // namespace GimpPainter
#endif
