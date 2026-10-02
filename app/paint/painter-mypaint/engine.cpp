/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "engine.hpp"
#include "legacy-brush.hpp"
namespace GimpPainter { namespace MyPaint {
struct Engine::Impl {
  Legacy::Brush brush;
  float opacity = 1;
  bool non_incremental = false;
};
Engine::Engine (const Resource& r) : impl_ (new Impl) { configure (r); }
Engine::~Engine () = default;
void Engine::configure (const Resource& r)
{
  for (const auto& d : r.diagnostics ())
    if (d.kind == Diagnostic::Kind::Unsupported)
      throw std::invalid_argument (d.path+": "+d.message);
  std::array<Mapping, BRUSH_MAPPING_COUNT> values;
  for (int i = 0; i < BRUSH_MAPPING_COUNT; ++i) values[i] = r.mapping (i);
  // The pinned core copies mappings without re-running speed precalculation.
  // Preserve that observable behavior until a separately reviewed correction.
  for (int i = 0; i < BRUSH_MAPPING_COUNT; ++i) impl_->brush.copy_mapping (i, &values[i]);
  impl_->opacity = r.base_value (BRUSH_STROKE_OPACITY);
  impl_->non_incremental = r.switch_value (BRUSH_NON_INCREMENTAL);
}
void Engine::set_base_value (int id, float value)
{
  if (id < 0 || id >= BRUSH_MAPPING_COUNT || !std::isfinite(value))
    throw std::invalid_argument("Invalid runtime brush setting");
  impl_->brush.set_base_value(id,value);
  if (id == BRUSH_STROKE_OPACITY) impl_->opacity=value;
}
void Engine::set_foreground (double r,double g,double b)
{
  if (!std::isfinite(r)||!std::isfinite(g)||!std::isfinite(b)) throw std::invalid_argument("Invalid foreground color");
  Legacy::GimpRGB rgb;rgb.r=r;rgb.g=g;rgb.b=b;Legacy::GimpHSV hsv;
  Legacy::gimp_rgb_to_hsv(&rgb,&hsv);
  set_base_value(BRUSH_COLOR_H,hsv.h);set_base_value(BRUSH_COLOR_S,hsv.s);set_base_value(BRUSH_COLOR_V,hsv.v);
}
void Engine::reset () { impl_->brush.reset (); }
void Engine::new_stroke () { impl_->brush.new_stroke (); }
bool Engine::stroke_to (Surface& surface, float x, float y, float pressure, float xtilt, float ytilt, double dtime)
{
  if (!std::isfinite (x) || !std::isfinite (y) || !std::isfinite (pressure) ||
      !std::isfinite (xtilt) || !std::isfinite (ytilt) || !std::isfinite (dtime) ||
      std::abs (x) >= 1e8 || std::abs (y) >= 1e8)
    throw std::invalid_argument ("Invalid brush input sample");
  // Coordinate equality is intentional: stationary pressure/time events must
  // still reach the integrator. GIMP input compression is a separate adapter.
  return impl_->brush.stroke_to (&surface, x, y, pressure, xtilt, ytilt, dtime);
}
float Engine::state (int i) const
{
  if (i < 0 || i >= STATE_COUNT) throw std::out_of_range ("Brush state");
  return impl_->brush.get_state (i);
}
void Engine::set_state (int i, float v)
{
  if (i < 0 || i >= STATE_COUNT || !std::isfinite (v)) throw std::invalid_argument ("Brush state");
  impl_->brush.set_state (i, v);
}
double Engine::painting_time () const { return impl_->brush.stroke_total_painting_time; }
double Engine::idling_time () const { return impl_->brush.stroke_current_idling_time; }
float Engine::stroke_opacity () const noexcept { return impl_->opacity; }
bool Engine::non_incremental () const noexcept { return impl_->non_incremental; }
} }
