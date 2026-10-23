/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_MYPAINT_ENGINE_HPP
#define GIMP_PAINTER_MYPAINT_ENGINE_HPP
#include "../../painter/gimp-painter-visibility.h"
#include "resource.hpp"
#include "surface.hpp"
#include <memory>
#include <utility>
namespace GimpPainter GIMP_PAINTER_PRIVATE { namespace MyPaint {
/* Owns the extended legacy evaluator, never a standard libmypaint brush. */
class Engine
{
  struct Impl;
public:
  // Immutable value-owned evaluator snapshot. Copies share only frozen data;
  // neither the live source engine nor surfaces/resources are retained.
  class Checkpoint
  {
    friend class Engine;
    std::shared_ptr<const Impl> value_;
    explicit Checkpoint (std::shared_ptr<const Impl> value) : value_(std::move(value)) {}
  public:
    Checkpoint () = default;
    bool empty () const noexcept { return !value_; }
  };
  // Mutable deep copy prepared before history publication. No live Engine or
  // Resource owner is retained; exchange itself never allocates.
  class Prepared
  {
    friend class Engine;
    std::unique_ptr<Impl> value_;
    explicit Prepared (std::unique_ptr<Impl> value);
  public:
    Prepared () noexcept;
    ~Prepared ();
    Prepared (Prepared&&) noexcept;
    Prepared& operator= (Prepared&&) noexcept;
    Prepared (const Prepared&) = delete;
    Prepared& operator= (const Prepared&) = delete;
    bool empty () const noexcept;
  };
  static Prepared prepare (const Checkpoint& checkpoint);
  // Rejects empty or an active sample without modifying either side. Successful
  // exchange leaves the former live evaluator in prepared for later Redo.
  bool try_exchange (Prepared& prepared) noexcept;
  // Only complete input-sample boundaries; reentrant capture/restore during a
  // Surface callback is rejected instead of exposing a half-evaluated state.
  Checkpoint checkpoint () const;
  // Empty snapshots are rejected; allocation/copy failure leaves this engine
  // unchanged. Full settings/cache/random/timing state is restored atomically.
  void restore (const Checkpoint& checkpoint);
  explicit Engine (const Resource& resource);
  ~Engine ();
  Engine (const Engine&) = delete;
  Engine& operator= (const Engine&) = delete;
  void configure (const Resource& resource);
  void set_base_value (int index, float value);
  void set_foreground (double r, double g, double b);
  void reset ();
  void new_stroke ();
  bool stroke_to (Surface& surface, float x, float y, float pressure,
                  float xtilt, float ytilt, double dtime);
  float state (int index) const;
  void set_state (int index, float value);
  double painting_time () const;
  double idling_time () const;
  // These are stroke/surface properties, deliberately separate from dab alpha.
  float stroke_opacity () const noexcept;
  bool non_incremental () const noexcept;
private:
  std::unique_ptr<Impl> impl_;
};
} }
#endif
