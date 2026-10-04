/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_FILTER_PROCEDURE_POLICY_HPP
#define GIMP_PAINTER_FILTER_PROCEDURE_POLICY_HPP
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace GimpPainter {
/* The complete untrusted wire selector. These identities describe audited
 * adapters, not permission to resolve arbitrary saved names or executables. */
enum class FilterProcedure : std::uint32_t { blinds = 1, small_tiles = 2, retinex = 3, convolution = 4 };

namespace FilterLegacy {
/* Semantic policy for the four accepted isolated routes. Provenance and
 * exceptions: migration/contracts/filter-{process-bridge,small-tiles,retinex,
 * convolution}.md. Runtime GParamSpec constraints are independently checked
 * in core; coincident numbers do not make public metadata a legacy policy. */
struct RouteNames { const char *saved; const char *execution; const char *public_name; };
constexpr RouteNames blinds {"plug-in-blinds", "plug-in-blinds", "plug-in-blinds"};
constexpr RouteNames small_tiles {"plug-in-small-tiles", "plug-in-painter-small-tiles", "plug-in-small-tiles"};
constexpr RouteNames retinex {"plug-in-retinex", "plug-in-painter-retinex", "plug-in-retinex"};
constexpr RouteNames convolution {"plug-in-convmatrix", "plug-in-painter-convmatrix", nullptr};

template<typename T> struct Scalar {
  T minimum, maximum, initial;
  constexpr bool accepts (T value) const noexcept { return value >= minimum && value <= maximum; }
};
/* 'initial' is the private request's inactive/default value, not a replacement
 * for absent saved fields or the public plug-in's UI/config default. */
constexpr Scalar<std::int32_t> angle {0, 90, 0}, segments {1, 100, 1}, tiles {0, 6, 2};
constexpr Scalar<std::int32_t> scale {16, 256, 240}, nscales {0, 8, 3}, scales_mode {0, 2, 0};
constexpr Scalar<double> cvar {0, 4, 1.2};
constexpr Scalar<std::int32_t> border {0, 2, 2};
constexpr std::int32_t orientation_default = 0, transparent_default = 0, alpha_alg_default = 1;
constexpr double divisor_default = 1, offset_default = 0;
constexpr std::size_t matrix_count = 25, channel_count = 5;
using Matrix = std::array<double, matrix_count>;
using Channels = std::array<std::int32_t, channel_count>;
constexpr Matrix identity_matrix {{0,0,0,0,0, 0,0,0,0,0, 0,0,1,0,0, 0,0,0,0,0, 0,0,0,0,0}};
constexpr Channels all_channels {{1,1,1,1,1}};

constexpr bool blinds_scalars (std::int32_t a, std::int32_t s) noexcept
{ return angle.accepts (a) && segments.accepts (s); }
inline bool retinex_scalars (std::int32_t s, std::int32_t n, std::int32_t mode, double v) noexcept
{ return scale.accepts (s) && nscales.accepts (n) && scales_mode.accepts (mode) && std::isfinite (v) && cvar.accepts (v); }
constexpr bool enabled (std::int32_t flag) noexcept { return flag != 0; }
constexpr const char *blinds_orientation (std::int32_t flag) noexcept
{ return flag == 1 ? "vertical" : "horizontal"; }
inline const char *retinex_distribution (std::int32_t mode) noexcept
{
  /* Caller has already admitted the integer domain. */
  constexpr const char *names[] = {"uniform", "low", "high"};
  return names[mode];
}
constexpr bool convolution_saved_count (std::size_t count) noexcept
{ return count == 11 || count == 12; } // arbitrary typed slot 11 stays ignored

/* Owner admission is deliberately conservative over the complete image and
 * RGBA carrier. It is not the execution-region/native-BPP overflow bound. */
constexpr bool retinex_owner_geometry (std::uint32_t width, std::uint32_t height) noexcept
{
  return width >= 16 && height >= 16 &&
    std::uint64_t (width) * height <= std::uint64_t (std::numeric_limits<std::int32_t>::max ()) / 4;
}

inline void validate_convolution_numbers (const Matrix& matrix, double divisor, double offset, bool legacy_float)
{
  for (double v : matrix) {
    if (!std::isfinite (v)) throw std::invalid_argument ("Nonfinite Convolution coefficient");
    if (legacy_float) {
      if (std::abs (v) > std::numeric_limits<float>::max ())
        throw std::invalid_argument ("Convolution coefficient exceeds legacy float range");
      const float f = v;
      if (!std::isfinite (f)) throw std::invalid_argument ("Convolution coefficient exceeds legacy float range");
    }
  }
  if (legacy_float) {
    if (std::abs (divisor) > std::numeric_limits<float>::max () ||
        std::abs (offset) > std::numeric_limits<float>::max ())
      throw std::invalid_argument ("Convolution scalar exceeds legacy float range");
    const float d = divisor, o = offset;
    if (!std::isfinite (d) || !d || !std::isfinite (o))
      throw std::invalid_argument ("Convolution scalar cannot be represented by legacy float arithmetic");
    /* Finite matrix/offset underflow to float zero is intentional. Divisor
     * underflow to zero is invalid. Native-double arithmetic does not narrow.
     * Intermediate and integer-conversion safety remains a per-pixel check. */
  }
}
}
}
#endif
