/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_FILTER_PROCEDURE_HPP
#define GIMP_PAINTER_FILTER_PROCEDURE_HPP
#include "filter-context.hpp"
#include <array>
#include <atomic>
#include <cstdint>
#include <stdexcept>
namespace GimpPainter {
/* This is the entire untrusted procedure selector. A frame contains no name,
 * executable, path, script, image identity, or GObject. Extend only after a
 * separate source/runtime compatibility audit of a literal bundled route. */
enum class FilterProcedure : std::uint32_t { blinds = 1 };
enum class FilterProcedureDisposition : std::uint32_t {
  pending = 0, merged = 1, shadow = 2, no_merge = 3
};
/* A scalar outcome owned independently of the application. Publish only after
 * exact transfer, terminal, EOF, successful reap and private-profile cleanup. */
class FilterProcedureResult {
public:
  FilterProcedureDisposition disposition () const noexcept { return value_.load (std::memory_order_acquire); }
  void reset () noexcept { value_.store (FilterProcedureDisposition::pending, std::memory_order_release); }
  void publish (FilterProcedureDisposition value) noexcept { value_.store (value, std::memory_order_release); }
private:
  std::atomic<FilterProcedureDisposition> value_ {FilterProcedureDisposition::pending};
};
struct FilterProcedureRequest {
  FilterProcedure procedure = FilterProcedure::blinds;
  std::uint32_t width = 0, height = 0;
  std::int32_t angle = 0, segments = 1, orientation = 0, transparent = 0;
  bool gray = false;
  std::array<std::uint8_t, 4> background {{0, 0, 0, 255}};
  FilterSelectionRegion start_region {};
  bool raw_shadow = false;
  FilterSelectionRegion execution_region () const {
    if (!width || !height || width > 524288 || height > 524288)
      throw std::invalid_argument ("Unsupported private Filter geometry");
    if (!start_region.selected) return {false, true, 0, 0, std::int32_t (width), std::int32_t (height)};
    const auto& r = start_region;
    if (r.x1 < 0 || r.y1 < 0 || r.x2 < r.x1 || r.y2 < r.y1 ||
        std::uint32_t (r.x2) > width || std::uint32_t (r.y2) > height ||
        r.intersects != (r.x1 < r.x2 && r.y1 < r.y2))
      throw std::invalid_argument ("Invalid private Filter start region");
    return r;
  }
  std::uint64_t bytes () const {
    execution_region ();
    if (procedure != FilterProcedure::blinds || !width || !height ||
        width > 524288 || height > 524288 || angle < 0 || angle > 90 ||
        segments < 1 || segments > 100)
      throw std::invalid_argument ("Unsupported private Filter procedure request");
    return std::uint64_t (width) * height * 4;
  }
};
}
#endif
