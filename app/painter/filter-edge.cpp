/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Legacy plug-in-edge compatibility arithmetic, adapted from
 * gimp-painter plug-ins/common/edge.c at
 * afa43fae3e920210146abed514f136fd49f671b5.
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis.
 * Based on pgmedge, Copyright (C) 1989 Jef Poskanzer.
 * GIMP 1.0 port by Eiichi Takamori; additional detectors by Dave Neary.
 */
#include "filter-edge.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace GimpPainter {
namespace {

/* These kernels use the historical column-major neighborhood order. Keep
 * multiplication and addition in the same order as the old C implementation:
 * amount is inside the square root for four detectors, outside for two. */
const int vertical[6][9] =
{
  { -1,  0,  1, -2,  0,  2, -1,  0,  1 }, // Sobel
  {  0,  0,  0,  0,  0,  0,  0,  0,  0 }, // Prewitt uses compass masks
  {  0,  0,  0,  0,  4, -4,  0,  0,  0 }, // Gradient
  {  0,  0,  0,  0,  4,  0,  0,  0, -4 }, // Roberts
  {  0,  0,  0,  0,  2, -2,  0,  2, -2 }, // Differential
  {  1,  1,  1,  1, -8,  1,  1,  1,  1 }  // Laplace
};

const int horizontal[5][9] =
{
  { -1, -2, -1,  0,  0,  0,  1,  2,  1 },
  {  0,  0,  0,  0,  0,  0,  0,  0,  0 },
  {  0,  0,  0,  0, -4,  0,  0,  4,  0 },
  {  0,  0,  0,  0,  0,  4,  0, -4,  0 },
  {  0,  0,  0,  0, -2, -2,  0,  2,  2 }
};

const int compass[8][9] =
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

std::uint8_t clamp_byte (double value)
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

std::uint8_t detect (const std::uint8_t *data, int mode, double amount)
{
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
bool neighbor (std::size_t coordinate, std::size_t extent, int offset,
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

} // namespace

bool filter_edge (const std::vector<std::uint8_t>& input,
                  std::size_t width,
                  std::size_t height,
                  const EdgeOptions& options,
                  std::atomic<bool>& cancel,
                  std::vector<std::uint8_t>& output)
{
  const auto maximum = std::numeric_limits<std::size_t>::max ();
  if (width == 0 || height == 0 || width > maximum / 4 ||
      height > maximum / (width * 4))
    throw std::invalid_argument ("Invalid edge-filter dimensions");
  if (input.size () != width * height * 4)
    throw std::invalid_argument ("Edge-filter input is not tightly packed RGBA8");
  if (options.wrapmode < 1 || options.wrapmode > 3 ||
      options.edgemode < 0 || options.edgemode > 5 ||
      !std::isfinite (options.amount))
    throw std::invalid_argument ("Invalid legacy edge-filter options");
  if (cancel.load (std::memory_order_relaxed))
    return false;

  const double amount = std::max (1.0, options.amount);
  std::vector<std::uint8_t> result (input.size ());
  for (std::size_t y = 0; y < height; ++y)
    {
      for (std::size_t x = 0; x < width; ++x)
        {
          /* Check within wide rows as well; no cancellation publication
           * depends on acquire/release because the flag carries no payload. */
          if ((x & 255) == 0 && cancel.load (std::memory_order_relaxed))
            return false;

          const auto destination = (y * width + x) * 4;
          if (input[destination + 3] == 0)
            {
              /* edge.c writes a shadow, then merges with REPLACE_INTEN.
               * paint-funcs.c:replace_inten_pixels keeps src1 RGB when
               * new_alpha is zero. The complete plug-in output therefore
               * preserves hidden RGB here, although the detector itself
               * calculated a value. Neighbor sampling below stays straight
               * RGBA, so hidden RGB still contributes to adjacent edges. */
              std::copy_n (input.data () + destination, 4,
                           result.data () + destination);
              continue;
            }

          std::uint8_t kernels[3][9];
          for (int dx = -1; dx <= 1; ++dx)
            for (int dy = -1; dy <= 1; ++dy)
              {
                std::size_t source_x, source_y;
                const int k = (dx + 1) * 3 + dy + 1;
                if (neighbor (x, width, dx, options.wrapmode, source_x) &&
                    neighbor (y, height, dy, options.wrapmode, source_y))
                  {
                    const auto source = (source_y * width + source_x) * 4;
                    for (int channel = 0; channel < 3; ++channel)
                      kernels[channel][k] = input[source + channel];
                  }
                else
                  {
                    for (int channel = 0; channel < 3; ++channel)
                      kernels[channel][k] = 0;
                  }
              }
          for (int channel = 0; channel < 3; ++channel)
            result[destination + channel] =
              detect (kernels[channel], options.edgemode, amount);
          result[destination + 3] = input[destination + 3];
        }
    }

  if (cancel.load (std::memory_order_relaxed))
    return false;
  output.swap (result);
  return true;
}

} // namespace GimpPainter
