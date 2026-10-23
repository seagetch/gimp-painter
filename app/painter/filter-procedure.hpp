/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_FILTER_PROCEDURE_HPP
#define GIMP_PAINTER_FILTER_PROCEDURE_HPP
#include "filter-context.hpp"
#include "filter-procedure-policy.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
namespace GimpPainter {
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
  std::int32_t angle = FilterLegacy::angle.initial, segments = FilterLegacy::segments.initial,
    orientation = FilterLegacy::orientation_default, transparent = FilterLegacy::transparent_default;
  std::int32_t tiles = FilterLegacy::tiles.initial;
  /* Retinex statistics divide by actual native BPP, never carrier BPP. */
  std::uint32_t storage_channels = 4;
  std::int32_t scale = FilterLegacy::scale.initial, nscales = FilterLegacy::nscales.initial,
    scales_mode = FilterLegacy::scales_mode.initial;
  double cvar = FilterLegacy::cvar.initial;
  /* Convolution has a fixed typed shape; saved counts/types are checked at the
   * owner before this descriptor exists. Other routes require defaults here. */
  std::uint32_t sample_mode = 0; // 0: nonlinear U8; 1/2/3: linear/nonlinear/perceptual double
  FilterLegacy::Matrix matrix = FilterLegacy::identity_matrix;
  FilterLegacy::Channels channels = FilterLegacy::all_channels;
  std::int32_t alpha_alg = FilterLegacy::alpha_alg_default, border = FilterLegacy::border.initial;
  double divisor = FilterLegacy::divisor_default, offset = FilterLegacy::offset_default;
  bool gray = false;
  std::size_t bytes_per_pixel () const noexcept { return sample_mode ? 32 : 4; }
  void validate_convolution () const {
    if (sample_mode > 3 || width < 3 || height < 3 || !FilterLegacy::border.accepts (border) ||
        !std::isfinite (divisor) || divisor == 0 || !std::isfinite (offset) ||
        (gray ? (storage_channels != 1 && storage_channels != 2) :
                (storage_channels != 3 && storage_channels != 4)))
      throw std::invalid_argument ("Unsupported private Convolution request");
    FilterLegacy::validate_convolution_numbers (matrix, divisor, offset, !sample_mode);
  }
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
    if (procedure != FilterProcedure::convolution && (storage_channels != 3 && storage_channels != 4))
      throw std::invalid_argument ("Unsupported private Filter native storage");
    if (procedure != FilterProcedure::retinex && procedure != FilterProcedure::convolution &&
        (storage_channels != 4 || scale != FilterLegacy::scale.initial || nscales != FilterLegacy::nscales.initial ||
         scales_mode != FilterLegacy::scales_mode.initial || cvar != FilterLegacy::cvar.initial))
      throw std::invalid_argument ("Noncanonical private Filter route scalars");
    if (procedure != FilterProcedure::convolution) {
      const FilterProcedureRequest defaults;
      if (sample_mode || matrix != defaults.matrix || channels != defaults.channels ||
          alpha_alg != defaults.alpha_alg || border != defaults.border || divisor != defaults.divisor || offset != defaults.offset)
        throw std::invalid_argument ("Noncanonical private Convolution fields");
    }
    if (procedure == FilterProcedure::convolution) {
      if (angle != FilterLegacy::angle.initial || segments != FilterLegacy::segments.initial ||
          orientation != FilterLegacy::orientation_default || transparent != FilterLegacy::transparent_default ||
          tiles != FilterLegacy::tiles.initial ||
          scale != FilterLegacy::scale.initial || nscales != FilterLegacy::nscales.initial ||
          scales_mode != FilterLegacy::scales_mode.initial || cvar != FilterLegacy::cvar.initial)
        throw std::invalid_argument ("Noncanonical private Convolution route scalars");
      validate_convolution ();
    }
    else if (procedure == FilterProcedure::blinds)
      {
        if (!FilterLegacy::blinds_scalars (angle, segments) || tiles != FilterLegacy::tiles.initial)
          throw std::invalid_argument ("Unsupported private Blinds request");
      }
    else if (procedure == FilterProcedure::small_tiles)
      {
        if (!FilterLegacy::tiles.accepts (tiles) || angle != FilterLegacy::angle.initial ||
            segments != FilterLegacy::segments.initial || orientation != FilterLegacy::orientation_default ||
            transparent != FilterLegacy::transparent_default ||
            std::uint64_t (region.x2 - region.x1) * std::uint64_t (region.y2 - region.y1) > 2147483647u)
          throw std::invalid_argument ("Unsupported private Small Tiles request");
      }
    else if (procedure == FilterProcedure::retinex)
      {
        const auto pixels = std::uint64_t (region.x2 - region.x1) * (region.y2 - region.y1);
        if (gray || angle != FilterLegacy::angle.initial || segments != FilterLegacy::segments.initial ||
            orientation != FilterLegacy::orientation_default || transparent != FilterLegacy::transparent_default ||
            tiles != FilterLegacy::tiles.initial ||
            !FilterLegacy::retinex_scalars (scale, nscales, scales_mode, cvar) ||
            !region.intersects || region.x2 - region.x1 < 16 || region.y2 - region.y1 < 16 ||
            pixels > std::uint64_t (std::numeric_limits<std::int32_t>::max () - (storage_channels - 1)) / storage_channels)
          throw std::invalid_argument ("Unsupported private Retinex request");
      }
    else throw std::invalid_argument ("Unsupported private Filter procedure request");
    return std::uint64_t (width) * height * bytes_per_pixel ();
  }
  std::uint64_t scratch_bytes () const {
    bytes ();
    /* Full drawable accounting is conservative before operation-start ROI.
     * Source bytes + BPP float output + two float planes + recurrence lines. */
    if (procedure == FilterProcedure::retinex)
      return std::uint64_t (width) * height * (storage_channels * 5 + 8) +
             (std::uint64_t (std::max (width, height)) + 3) * 8;
    if (procedure == FilterProcedure::convolution)
      return (std::uint64_t (width) + 4) * storage_channels * (sample_mode ? 8 : 1) * 6;
    if (procedure == FilterProcedure::blinds || procedure == FilterProcedure::small_tiles)
      return std::uint64_t (std::max (width, height)) * 384;
    return 0;
  }
};
}
#endif
