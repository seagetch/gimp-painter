/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Legacy plug-in-edge compatibility arithmetic, adapted from
 * gimp-painter plug-ins/common/edge.c at
 * afa43fae3e920210146abed514f136fd49f671b5.
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis.
 * Based on pgmedge, Copyright (C) 1989 Jef Poskanzer.
 * GIMP 1.0 port by Eiichi Takamori; additional detectors by Dave Neary.
 */
/* Shared arithmetic for the vector and spill-backed implementations.
 * Preserve historical operation order and byte rounding in one place. */
#ifndef GIMP_PAINTER_FILTER_EDGE_KERNEL_PRIVATE_HPP
#define GIMP_PAINTER_FILTER_EDGE_KERNEL_PRIVATE_HPP
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>
namespace GimpPainter { namespace FilterEdgeDetail {

/* These kernels use the historical column-major neighborhood order. Keep
 * multiplication and addition in the same order as the old C implementation:
 * amount is inside the square root for four detectors, outside for two. */
inline std::uint8_t clamp_byte (double value)
{
  /* The old function returned gint (truncating toward zero), then CLAMP0255.
   * Clamping before conversion is identical for defined legacy results, and
   * avoids undefined float-to-int overflow for arbitrarily large amounts. */
  if (value <= 0.0)
    return 0;
  if (value >= 255.0)
    return 255;
  return static_cast<std::uint8_t> (value);
}

inline std::uint8_t detect (const std::uint8_t *data, int mode, double amount)
{
  static const int vertical[6][9] =
  {
    { -1,  0,  1, -2,  0,  2, -1,  0,  1 }, // Sobel
    {  0,  0,  0,  0,  0,  0,  0,  0,  0 }, // Prewitt uses compass masks
    {  0,  0,  0,  0,  4, -4,  0,  0,  0 }, // Gradient
    {  0,  0,  0,  0,  4,  0,  0,  0, -4 }, // Roberts
    {  0,  0,  0,  0,  2, -2,  0,  2, -2 }, // Differential
    {  1,  1,  1,  1, -8,  1,  1,  1,  1 }  // Laplace
  };
  
  static const int horizontal[5][9] =
  {
    { -1, -2, -1,  0,  0,  0,  1,  2,  1 },
    {  0,  0,  0,  0,  0,  0,  0,  0,  0 },
    {  0,  0,  0,  0, -4,  0,  0,  4,  0 },
    {  0,  0,  0,  0,  0,  4,  0, -4,  0 },
    {  0,  0,  0,  0, -2, -2,  0,  2,  2 }
  };
  
  static const int compass[8][9] =
  {
    {  1,  1,  1,  1, -2,  1, -1, -1, -1 },
    {  1,  1,  1,  1, -2, -1,  1, -1, -1 },
    {  1,  1, -1,  1, -2, -1,  1,  1, -1 },
    {  1, -1, -1,  1, -2, -1,  1,  1,  1 },
    { -1, -1, -1,  1, -2,  1,  1,  1,  1 },
    { -1, -1,  1, -1, -2,  1,  1,  1,  1 },
    { -1,  1,  1, -1, -2,  1, -1,  1,  1 },
    {  1,  1,  1, -1, -2,  1, -1, -1,  1 }
  };

  if (mode == 1)
    {
      int maximum = 0;
      for (const auto& mask : compass)
        {
          int value = 0;
          for (int i = 0; i < 9; ++i)
            value += mask[i] * data[i];
          maximum = std::max (maximum, value);
        }
      return clamp_byte (amount * maximum);
    }

  int v_grad = 0;
  for (int i = 0; i < 9; ++i)
    v_grad += vertical[mode][i] * data[i];

  if (mode == 5)
    return clamp_byte (v_grad * amount);

  int h_grad = 0;
  for (int i = 0; i < 9; ++i)
    h_grad += horizontal[mode][i] * data[i];

  return clamp_byte (std::sqrt (v_grad * v_grad * amount +
                               h_grad * h_grad * amount));
}

/* The neighborhood only needs an offset of -1, 0, or 1. Keeping coordinates
 * unsigned avoids narrowing the public size_t dimensions or overflowing a
 * signed coordinate at the drawable edge. */
inline bool neighbor (std::size_t coordinate, std::size_t extent, int offset,
               int wrapmode, std::size_t& result)
{
  if (offset == -1 && coordinate == 0)
    {
      result = wrapmode == 1 ? extent - 1 : 0;
      return wrapmode != 3;
    }
  if (offset == 1 && coordinate == extent - 1)
    {
      result = wrapmode == 1 ? 0 : extent - 1;
      return wrapmode != 3;
    }
  result = offset == -1 ? coordinate - 1 :
           offset == 1 ? coordinate + 1 : coordinate;
  return true;
}

} } // namespace GimpPainter::FilterEdgeDetail
#endif
