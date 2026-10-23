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
#include "filter-edge-kernel-private.hpp"
#include "filter-progress.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace GimpPainter {
namespace {
using namespace FilterEdgeDetail;

} // namespace

bool filter_edge (const std::vector<std::uint8_t>& input,
                  std::size_t width,
                  std::size_t height,
                  const EdgeOptions& options,
                  std::atomic<bool>& cancel,
                  std::vector<std::uint8_t>& output,
                  const std::shared_ptr<FilterProgress>& progress)
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
          if ((x & 255) == 0)
            {
              if (cancel.load (std::memory_order_relaxed)) return false;
              if (progress)
                progress->set_value (0.99 * (double (y) * width + x) /
                                     (double (width) * height));
            }

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
  if (progress) progress->set_value (1.0);
  return true;
}

} // namespace GimpPainter
