/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_FILTER_PROCEDURE_HPP
#define GIMP_PAINTER_FILTER_PROCEDURE_HPP
#include "filter-context.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
namespace GimpPainter {
/* This is the entire untrusted procedure selector. A frame contains no name,
 * executable, path, script, image identity, or GObject. Extend only after a
 * separate source/runtime compatibility audit of a literal bundled route. */
enum class FilterProcedure : std::uint32_t { blinds = 1, small_tiles = 2, retinex = 3 };
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
  std::int32_t tiles = 2;
  /* Retinex statistics divide by actual native BPP, never carrier BPP. */
  std::uint32_t storage_channels = 4;
  std::int32_t scale = 240, nscales = 3, scales_mode = 0;
  double cvar = 1.2;
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
    const auto region = execution_region ();
    if (storage_channels != 3 && storage_channels != 4)
      throw std::invalid_argument ("Unsupported private Filter native storage");
    if (procedure != FilterProcedure::retinex &&
        (storage_channels != 4 || scale != 240 || nscales != 3 || scales_mode != 0 || cvar != 1.2))
      throw std::invalid_argument ("Noncanonical private Filter route scalars");
    if (procedure == FilterProcedure::blinds)
      {
        if (angle < 0 || angle > 90 || segments < 1 || segments > 100 || tiles != 2)
          throw std::invalid_argument ("Unsupported private Blinds request");
      }
    else if (procedure == FilterProcedure::small_tiles)
      {
        if (tiles < 0 || tiles > 6 || angle || segments != 1 || orientation || transparent ||
            std::uint64_t (region.x2 - region.x1) * std::uint64_t (region.y2 - region.y1) > 2147483647u)
          throw std::invalid_argument ("Unsupported private Small Tiles request");
      }
    else if (procedure == FilterProcedure::retinex)
      {
        const auto pixels = std::uint64_t (region.x2 - region.x1) * (region.y2 - region.y1);
        if (gray || angle || segments != 1 || orientation || transparent || tiles != 2 ||
            scale < 16 || scale > 256 || nscales < 0 || nscales > 8 ||
            scales_mode < 0 || scales_mode > 2 || !std::isfinite (cvar) || cvar < 0 || cvar > 4 ||
            !region.intersects || region.x2 - region.x1 < 16 || region.y2 - region.y1 < 16 ||
            pixels > std::uint64_t (std::numeric_limits<std::int32_t>::max () - (storage_channels - 1)) / storage_channels)
          throw std::invalid_argument ("Unsupported private Retinex request");
      }
    else throw std::invalid_argument ("Unsupported private Filter procedure request");
    return std::uint64_t (width) * height * 4;
  }
  std::uint64_t scratch_bytes () const {
    bytes ();
    /* Full drawable accounting is conservative before operation-start ROI.
     * Source bytes + BPP float output + two float planes + recurrence lines. */
    if (procedure == FilterProcedure::retinex)
      return std::uint64_t (width) * height * (storage_channels * 5 + 8) +
             (std::uint64_t (std::max (width, height)) + 3) * 8;
    if (procedure == FilterProcedure::blinds || procedure == FilterProcedure::small_tiles)
      return std::uint64_t (std::max (width, height)) * 384;
    return 0;
  }
};
}
#endif
