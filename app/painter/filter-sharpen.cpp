/* SPDX-License-Identifier: GPL-3.0-or-later
 * Legacy plug-in-sharpen arithmetic adapted from plug-ins/common/sharpen.c at
 * afa43fae3e920210146abed514f136fd49f671b5.
 * Copyright 1997-1998 Michael Sweet (mike@easysw.com).
 */
#include "filter-sharpen.hpp"
#include "filter-progress.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>

namespace GimpPainter {
namespace {

/* The pinned legacy implementation uses signed >> 3. Express its arithmetic
 * right shift as floor division, without implementation-defined negative
 * shifts or overflowing a negation. Keep both legacy rounding steps. */
std::int32_t floor_eighth (std::int32_t value)
{
  return value / 8 - (value < 0 && value % 8 != 0);
}

struct SharpenLuts
{
  std::array<std::int32_t, 256> positive, negative;

  explicit SharpenLuts (std::int32_t percent)
  {
    const auto factor = std::max<std::int32_t> (1, 100 - percent);
    for (std::int32_t i = 0; i < 256; ++i)
      {
        positive[i] = 800 * i / factor;
        negative[i] = floor_eighth (4 + positive[i] - 8 * i);
      }
  }

  std::uint8_t channel (const std::uint8_t *top,
                        const std::uint8_t *middle,
                        const std::uint8_t *bottom) const
  {
    auto pixel = positive[*middle] - negative[top[-4]] - negative[top[0]] -
                 negative[top[4]] - negative[middle[-4]] - negative[middle[4]] -
                 negative[bottom[-4]] - negative[bottom[0]] - negative[bottom[4]];
    pixel = floor_eighth (pixel + 4);
    return static_cast<std::uint8_t> (std::max<std::int32_t>
      (0, std::min<std::int32_t> (255, pixel)));
  }
};

} // namespace

bool filter_sharpen_raster (FilterRaster& input,
                            std::size_t width,
                            std::size_t height,
                            const SharpenOptions& options,
                            std::atomic<bool>& cancel,
                            FilterRaster& output,
                            const std::shared_ptr<FilterProgress>& progress)
{
  constexpr auto legacy_maximum = std::numeric_limits<std::int32_t>::max ();
  constexpr auto offset_maximum = std::uint64_t
    (std::numeric_limits<std::int64_t>::max ());
  if (width < 2 || height < 3 || width > std::size_t (legacy_maximum / 4) ||
      height > std::size_t (legacy_maximum) ||
      std::uint64_t (width) > offset_maximum / 4 / height || &input == &output)
    throw std::invalid_argument ("Unsupported legacy sharpen dimensions/alias");

  const auto size = std::uint64_t (width) * height * 4;
  if (input.size () != size || output.size () != size)
    throw std::invalid_argument ("Sharpen raster is not tightly packed RGBA8");
  if (options.percent < 100 - legacy_maximum)
    throw std::invalid_argument ("Sharpen percent overflows legacy subtraction");
  if (cancel.load (std::memory_order_relaxed)) return false;

  const SharpenLuts luts (options.percent);
  constexpr std::size_t strip = 1024;
  std::array<std::array<std::uint8_t, (strip + 2) * 4>, 3> rows;
  std::array<std::uint8_t, strip * 4> result;
  for (std::size_t y = 0; y < height; ++y)
    for (std::size_t x = 0; x < width; )
      {
        if (cancel.load (std::memory_order_relaxed)) return false;
        const auto count = std::min (strip, width - x);
        const auto first = x ? x - 1 : 0;
        const auto end = x + count < width ? x + count + 1 : width;
        const auto offset = (std::uint64_t (y) * width + x) * 4;
        const bool border_row = !y || y == height - 1;

        input.read ((std::uint64_t (y) * width + first) * 4,
                    (end - first) * 4, rows[1].data ());
        if (cancel.load (std::memory_order_relaxed)) return false;
        if (!border_row)
          {
            input.read ((std::uint64_t (y - 1) * width + first) * 4,
                        (end - first) * 4, rows[0].data ());
            if (cancel.load (std::memory_order_relaxed)) return false;
            input.read ((std::uint64_t (y + 1) * width + first) * 4,
                        (end - first) * 4, rows[2].data ());
            if (cancel.load (std::memory_order_relaxed)) return false;
          }

        for (std::size_t i = 0; i < count; ++i)
          {
            if ((i & 255) == 0 && cancel.load (std::memory_order_relaxed))
              return false;
            const auto local = (x + i - first) * 4;
            const auto *original = rows[1].data () + local;
            auto *destination = result.data () + i * 4;
            if (border_row || x + i == 0 || x + i == width - 1)
              std::copy_n (original, 4, destination);
            else
              {
                for (std::size_t channel = 0; channel < 3; ++channel)
                  destination[channel] = luts.channel
                    (rows[0].data () + local + channel, original + channel,
                     rows[2].data () + local + channel);
                destination[3] = original[3];
              }
          }

        if (cancel.load (std::memory_order_relaxed)) return false;
        output.write (offset, count * 4, result.data ());
        x += count;
        if (progress)
          progress->set_value (0.99 * double (offset + count * 4) / size);
      }

  if (cancel.load (std::memory_order_relaxed)) return false;
  output.flush ();
  if (cancel.load (std::memory_order_relaxed)) return false;
  if (progress) progress->set_value (1.0);
  return true;
}

} // namespace GimpPainter
