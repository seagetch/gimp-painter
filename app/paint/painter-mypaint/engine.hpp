/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_MYPAINT_ENGINE_HPP
#define GIMP_PAINTER_MYPAINT_ENGINE_HPP
#include "resource.hpp"
#include "surface.hpp"
#include <memory>
namespace GimpPainter { namespace MyPaint {
/* Owns the extended legacy evaluator, never a standard libmypaint brush. */
class Engine
{
public:
  explicit Engine (const Resource& resource);
  ~Engine ();
  Engine (const Engine&) = delete;
  Engine& operator= (const Engine&) = delete;
  void configure (const Resource& resource);
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
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
} }
#endif
