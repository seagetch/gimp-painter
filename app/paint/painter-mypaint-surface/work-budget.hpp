/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_MYPAINT_WORK_BUDGET_HPP
#define GIMP_PAINTER_MYPAINT_WORK_BUDGET_HPP
#include <cstddef>
#include <stdexcept>
namespace GimpPainter { namespace MyPaint { namespace WorkBudget {
// Per-operation owned scratch, not an image extent or GEGL cache limit.
// Worst wide nonincremental draw: masks 12 + floating 32 + original 32 +
// result 32 = 108 bytes/pixel. Selection decode peaks below this. Reserve
// 128 for every precision/mode so admission is independent of brush state.
// Radius 800: even a rotated 1600-square bitmap fits inside 2266-square;
// 2266^2 * (128 + 1 mask byte) < 1 GiB. No permitted ordinary radius is cut.
constexpr std::size_t limit = std::size_t(1) << 30;
constexpr std::size_t bytes_per_pixel = 128;
inline std::size_t require(std::size_t width, std::size_t height,
                           std::size_t provider_bytes = 0,
                           std::size_t budget = limit)
{
  if (provider_bytes > budget ||
      (height && width > (budget - provider_bytes) / bytes_per_pixel / height))
    throw std::length_error("Painter operation exceeds 1 GiB scratch budget; reduce brush footprint");
  return width * height * bytes_per_pixel + provider_bytes;
}
} } }
#endif
