/* SPDX-License-Identifier: GPL-3.0-or-later
 * Legacy plug-in-sobel arithmetic adapted from plug-ins/common/edge-sobel.c
 * at afa43fae3e920210146abed514f136fd49f671b5.
 * Original plug-in by Thorsten Schnier (thorsten@arch.usyd.edu.au), 1997,
 * based on Spencer Kimball and Peter Mattis's Gauss and Blur filters.
 */
#include "filter-sobel.hpp"
#include "filter-native-kernels.hpp"
#include "filter-progress.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace GimpPainter {
namespace {

constexpr std::size_t strip = 1024;
using HaloRow = std::array<std::uint8_t, (strip + 2) * 4>;

struct Geometry
{
  std::uint64_t full_size, region_size;
};

Geometry validate (std::size_t width, std::size_t height,
                   const FilterSelectionRegion& region)
{
  constexpr auto maximum = std::numeric_limits<std::int32_t>::max ();
  constexpr auto offset_maximum = std::uint64_t
    (std::numeric_limits<std::int64_t>::max ());
  if (!width || !height || width > std::size_t (maximum) ||
      height > std::size_t (maximum) ||
      std::uint64_t (width) > offset_maximum / 4 / height)
    throw std::invalid_argument ("Unsupported full Sobel shadow dimensions");

  if (region.x1 < 0 || region.y1 < 0 || region.x2 < region.x1 ||
      region.y2 < region.y1 || std::uint64_t (region.x2) > width ||
      std::uint64_t (region.y2) > height ||
      region.intersects != (region.x1 < region.x2 && region.y1 < region.y2) ||
      (!region.selected && (!region.intersects || region.x1 != 0 ||
                             region.y1 != 0 || std::size_t (region.x2) != width ||
                             std::size_t (region.y2) != height)))
    throw std::invalid_argument ("Invalid captured Sobel selection region");

  // Ordered, nonnegative INT32 endpoints make these subtractions safe.
  const auto region_width = std::size_t (region.x2 - region.x1);
  const auto region_height = std::size_t (region.y2 - region.y1);
  // The original returns before allocation on an empty intersection. For an
  // intersecting RGBA ROI, (width+2)*bytes is signed gint in edge-sobel.c.
  if (region.intersects && region_width > std::size_t (maximum / 4 - 2))
    throw std::invalid_argument ("Sobel row allocation overflows legacy gint");
  return {std::uint64_t (width) * height * 4,
          std::uint64_t (region_width) * region_height * 4};
}

struct Cancelled {};
void check_cancel (const std::atomic<bool>& cancel)
{
  if (cancel.load (std::memory_order_relaxed)) throw Cancelled {};
}

class WorkProgress
{
  const std::shared_ptr<FilterProgress>& progress_;
  const double total_;
  std::uint64_t completed_ = 0;
public:
  WorkProgress (const std::shared_ptr<FilterProgress>& progress,
                const Geometry& geometry)
    : progress_ (progress),
      total_ (double (geometry.full_size) + double (geometry.region_size)) {}
  void wrote (std::size_t count)
  {
    // Two extents bounded by INT64_MAX fit together in this unsigned counter.
    completed_ += count;
    if (progress_) progress_->set_value (0.99 * double (completed_) / total_);
  }
};

/* Buffer layout is always one left halo, count centers, one right halo.
 * Only horizontal ROI endpoints replicate. The caller supplies a Y clamped
 * against the complete drawable, not the selected rectangle. */
void prepare_row (FilterRaster& input, std::size_t width, std::size_t y,
                  std::size_t x, std::size_t count,
                  std::size_t left, std::size_t right,
                  HaloRow& row, const std::atomic<bool>& cancel)
{
  const auto first = x > left ? x - 1 : x;
  const auto end = x + count < right ? x + count + 1 : right;
  check_cancel (cancel);
  input.read ((std::uint64_t (y) * width + first) * 4, (end - first) * 4,
              row.data () + (x == left ? 4 : 0));
  check_cancel (cancel);
  if (x == left) std::copy_n (row.data () + 4, 4, row.data ());
  if (x + count == right)
    std::copy_n (row.data () + count * 4, 4, row.data () + (count + 1) * 4);
}

/* This is ROUND, not SIGNED_ROUND or std::round. The old macro is
 * ((int)((x)+0.5)), including its asymmetric negative-value behavior. */
std::int32_t legacy_round (double value)
{
  return static_cast<std::int32_t> (value + 0.5);
}

std::int32_t gradient (const std::uint8_t *top,
                       const std::uint8_t *middle,
                       const std::uint8_t *bottom,
                       const SobelOptions& options)
{
  const std::int32_t horizontal = options.horizontal ?
    (top[-4] + 2 * top[0] + top[4]) -
    (bottom[-4] + 2 * bottom[0] + bottom[4]) : 0;
  const std::int32_t vertical = options.vertical ?
    (top[-4] + 2 * middle[-4] + bottom[-4]) -
    (top[4] + 2 * middle[4] + bottom[4]) : 0;

  // Each gradient lies in [-1020,1020], so the sum of squares fits INT32.
  // Keep the old ROUND -> divide -> integer assignment order exactly.
  if (options.horizontal && options.vertical)
    return static_cast<std::int32_t> (legacy_round (std::sqrt
      (double (horizontal * horizontal + vertical * vertical))) / 5.66);

  const auto sum = horizontal + vertical;
  return options.keep_sign ? 127 + legacy_round (sum / 8.0) :
                            legacy_round (std::abs (sum) / 4.0);
}

} // namespace

void filter_sobel_shadow_validate (std::size_t width, std::size_t height,
                                   const FilterSelectionRegion& region,
                                   const SobelOptions&)
{
  validate (width, height, region);
}

FilterSobelShadowOutcome filter_sobel_shadow_raster (
  FilterRaster& input, std::size_t width, std::size_t height,
  const FilterSelectionRegion& region, const SobelOptions& options,
  std::atomic<bool>& cancel, FilterRaster& output,
  const std::shared_ptr<FilterProgress>& progress)
{
  if (&input == &output)
    throw std::invalid_argument ("Sobel shadow input/output alias");
  const auto geometry = validate (width, height, region);
  if (input.size () != geometry.full_size || output.size () != geometry.full_size)
    throw std::invalid_argument ("Sobel shadow is not full tightly packed RGBA8");

  try
    {
      check_cancel (cancel);
      WorkProgress work (progress, geometry);
      const std::array<std::uint8_t, 16384> zeros {};
      for (std::uint64_t offset = 0; offset < geometry.full_size; )
        {
          check_cancel (cancel);
          const auto count = std::size_t (std::min<std::uint64_t>
            (zeros.size (), geometry.full_size - offset));
          output.write (offset, count, zeros.data ());
          check_cancel (cancel);
          work.wrote (count);
          offset += count;
        }

      std::array<HaloRow, 3> rows;
      std::array<std::uint8_t, strip * 4> result;
      const auto left = std::size_t (region.x1), right = std::size_t (region.x2);
      if (region.intersects)
        for (std::size_t y = std::size_t (region.y1);
             y < std::size_t (region.y2); ++y)
          for (std::size_t x = left; x < right; )
            {
              check_cancel (cancel);
              const auto count = std::min (strip, right - x);
              prepare_row (input, width, y ? y - 1 : 0,
                           x, count, left, right, rows[0], cancel);
              prepare_row (input, width, y,
                           x, count, left, right, rows[1], cancel);
              prepare_row (input, width, y + 1 < height ? y + 1 : y,
                           x, count, left, right, rows[2], cancel);

              for (std::size_t pixel = 0; pixel < count; ++pixel)
                {
                  if ((pixel & 255) == 0) check_cancel (cancel);
                  const auto at = (pixel + 1) * 4;
                  auto *destination = result.data () + pixel * 4;
                  bool opaque = false;
                  for (std::size_t channel = 0; channel < 3; ++channel)
                    {
                      const auto value = gradient (rows[0].data () + at + channel,
                                                   rows[1].data () + at + channel,
                                                   rows[2].data () + at + channel,
                                                   options);
                      // Compare the integer gradient before byte conversion,
                      // as the old alpha counter does. Input alpha is unused.
                      opaque = opaque || value > 10;
                      destination[channel] = static_cast<std::uint8_t> (value);
                    }
                  destination[3] = opaque ? 255 : 0;
                }

              check_cancel (cancel);
              output.write ((std::uint64_t (y) * width + x) * 4,
                            count * 4, result.data ());
              check_cancel (cancel);
              work.wrote (count * 4);
              x += count;
            }

      check_cancel (cancel);
      output.flush ();
      check_cancel (cancel);
      if (progress) progress->set_value (1.0);
      return region.intersects ? FilterSobelShadowOutcome::shadow :
                                 FilterSobelShadowOutcome::no_merge;
    }
  catch (const Cancelled&) { return FilterSobelShadowOutcome::cancelled; }
}

FilterSobelShadowOutcome filter_sobel_shadow_vector (
  const std::vector<std::uint8_t>& input, std::size_t width, std::size_t height,
  const FilterSelectionRegion& region, const SobelOptions& options,
  std::atomic<bool>& cancel, std::vector<std::uint8_t>& output,
  const std::shared_ptr<FilterProgress>& progress)
{
  const auto geometry = validate (width, height, region);
  if (input.size () != geometry.full_size)
    throw std::invalid_argument ("Sobel shadow is not full tightly packed RGBA8");
  auto outcome = FilterSobelShadowOutcome::cancelled;
  const auto completed = filter_native_vector (input, cancel, output,
    [&] (FilterRaster& source, FilterRaster& destination,
         std::atomic<bool>& stop, const FilterRasterFactory&) {
      outcome = filter_sobel_shadow_raster (source, width, height, region,
                                            options, stop, destination, progress);
      return outcome != FilterSobelShadowOutcome::cancelled;
    });
  return completed ? outcome : FilterSobelShadowOutcome::cancelled;
}

} // namespace GimpPainter
