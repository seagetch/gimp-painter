/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_NATIVE_STROKING_HPP
#define GIMP_PAINTER_NATIVE_STROKING_HPP
#include "../painter/resources.hpp"
#include "../painter/gimp-painter-visibility.h"
#include "painter/connection.hpp"
#include <atomic>
#include <memory>
#include <vector>
#include <utility>
#include <string>

namespace GimpPainter GIMP_PAINTER_PRIVATE {
/* An operation-local watch, not a second GObject implementation. Signal data
 * owns only a weak pointer to this independent cancellation record. */
struct NativeStrokeWatch {
  std::atomic<bool> invalid {false}, sealed {false};
  bool committed = false;
  std::vector<ObjectRef<GObject>> resources;
  std::vector<Connection> connections;
  void invalidate () noexcept { if (!sealed.load ()) invalid.store (true); }
  void check () const {
    if (!sealed.load () && invalid.load ()) throw Error (GIMP_PAINTER_ERROR_CLOSED, "Stroke settings or resources changed during the operation");
  }
  using Weak = std::weak_ptr<NativeStrokeWatch>;
  static void destroy_hook (gpointer data, GClosure *) noexcept { delete static_cast<Weak *> (data); }
  static void changed (GObject *, gpointer data) noexcept {
    if (auto watch = static_cast<Weak *> (data)->lock ()) watch->invalidate ();
  }
  static void notified (GObject *object, GParamSpec *, gpointer data) noexcept { changed (object, data); }
  static void connect (const std::shared_ptr<NativeStrokeWatch>& watch, GObject *emitter,
                       const char *signal, GCallback callback) {
    if (!emitter) return;
    std::unique_ptr<Weak> hook (new Weak (watch));
    auto connection = Connection::connect (ObjectRef<GObject>::retain (emitter), signal,
                                           callback, hook.get (), destroy_hook);
    hook.release ();
    watch->resources.push_back (ObjectRef<GObject>::retain (emitter));
    watch->connections.push_back (std::move (connection));
  }
  static std::shared_ptr<NativeStrokeWatch> create (GimpPaintOptions *options) {
    auto watch = std::make_shared<NativeStrokeWatch> ();
    connect (watch, G_OBJECT (options), "notify", G_CALLBACK (notified));
    auto *context = GIMP_CONTEXT (options);
    connect (watch, G_OBJECT (gimp_context_get_brush (context)), "dirty", G_CALLBACK (changed));
    connect (watch, G_OBJECT (gimp_context_get_dynamics (context)), "dirty", G_CALLBACK (changed));
    connect (watch, G_OBJECT (gimp_context_get_pattern (context)), "dirty", G_CALLBACK (changed));
    connect (watch, G_OBJECT (gimp_context_get_gradient (context)), "dirty", G_CALLBACK (changed));
    return watch;
  }
};

/* The caller holds the existing typed store borrow. There is one native
 * transaction and one undo snapshot, regardless of disconnected subpaths. */
template<class Cancel>
struct NativeStrokeGuard {
  GimpPaintCore *core;
  std::shared_ptr<NativeStrokeWatch>& slot;
  std::shared_ptr<NativeStrokeWatch> watch;
  Cancel cancel;
  GimpCoords start, current, last;
  GimpVector2 paint;
  double distance, pixel_dist;
  NativeStrokeGuard (GimpPaintCore *c, std::shared_ptr<NativeStrokeWatch>& s,
                     std::shared_ptr<NativeStrokeWatch> w, Cancel fn)
    : core (c), slot (s), watch (std::move (w)), cancel (std::move (fn)),
      start (c->start_coords), current (c->cur_coords), last (c->last_coords), paint (c->last_paint),
      distance (c->distance), pixel_dist (c->pixel_dist) {
    if (slot) throw Error (GIMP_PAINTER_ERROR_INVALID_STATE, "Owned stroke operation is already active");
    slot = watch;
  }
  NativeStrokeGuard (const NativeStrokeGuard&) = delete;
  NativeStrokeGuard& operator= (const NativeStrokeGuard&) = delete;
  ~NativeStrokeGuard () noexcept {
    if (!watch->committed) {
      try { cancel (); } catch (...) {}
      core->start_coords = start; core->cur_coords = current;
      core->last_coords = last; core->last_paint = paint;
      core->distance = distance; core->pixel_dist = pixel_dist;
    }
    if (slot == watch) slot.reset ();
  }
};
template<class Cancel>
std::unique_ptr<NativeStrokeGuard<Cancel>> native_stroke_guard (GimpPaintCore *core,
    std::shared_ptr<NativeStrokeWatch>& slot, std::shared_ptr<NativeStrokeWatch> watch, Cancel cancel) {
  return std::unique_ptr<NativeStrokeGuard<Cancel>> (new NativeStrokeGuard<Cancel> (core, slot, std::move (watch), std::move (cancel)));
}

template<class Validate>
const GimpCoords *native_stroke_validate (const GimpPaintStrokeSegment *segments,
                                         gsize n_segments, Validate validate) {
  if (!segments && n_segments) throw std::invalid_argument ("Missing stroke segments");
  const GimpCoords *first = nullptr;
  for (gsize i = 0; i < n_segments; ++i) {
    const auto& segment = segments[i];
    if (!segment.coords && segment.n_coords) throw std::invalid_argument ("Missing stroke coordinates");
    for (gsize n = 0; n < segment.n_coords; ++n) validate (&segment.coords[n]);
    if (!first && segment.n_coords) first = segment.coords;
  }
  if (!first) throw std::invalid_argument ("Not enough points to stroke");
  return first;
}
template<class Function>
void native_stroke_checked (Function function) {
  GError *error = nullptr;
  if (!function (&error)) {
    const std::string message = GimpPainter::take_error_message (error, "Owned stroke operation failed"); throw std::runtime_error (message);
  }
  g_clear_error (&error);
}
template<class Next, class Motion, class Step>
void native_stroke_segments (const GimpPaintStrokeSegment *segments, gsize count,
                             const std::shared_ptr<NativeStrokeWatch>& watch,
                             Next next, Motion motion, Step step) {
  bool first = true;
  for (gsize i = 0; i < count; ++i) {
    const auto& segment = segments[i]; if (!segment.n_coords) continue;
    watch->check ();
    if (!first) next (segment.coords);
    first = false;
    for (gsize n = 0; n < segment.n_coords; ++n) {
      watch->check ();
      native_stroke_checked ([&] (GError **error) { return motion (&segment.coords[n], error); });
      for (;;) {
        GError *error = nullptr;
        const bool done = step (&error);
        if (error) { std::string message = GimpPainter::take_error_message (error, "Unknown error");throw std::runtime_error (message); }
        watch->check ();
        if (done) break;
      }
    }
  }
}
} // namespace GimpPainter
#endif
