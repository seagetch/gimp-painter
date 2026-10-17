/* SPDX-License-Identifier: GPL-3.0-or-later
 * Legacy plug-in-edge raw shadow, adapted from plug-ins/common/edge.c at
 * afa43fae3e920210146abed514f136fd49f671b5. Copyright (C) 1995 Spencer
 * Kimball and Peter Mattis; based on pgmedge by Jef Poskanzer (1989).
 * GIMP 1.0 port by Eiichi Takamori; additional detectors by Dave Neary.
 */
#include "filter-edge-shadow.hpp"
#include "filter-native-kernels.hpp"
#include "filter-edge-kernel-private.hpp"
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
                   const FilterSelectionRegion& region, const EdgeOptions& options)
{
  constexpr auto maximum = std::numeric_limits<std::int32_t>::max ();
  constexpr auto offset_maximum = std::uint64_t
    (std::numeric_limits<std::int64_t>::max ());
  if (!width || !height || width > std::size_t (maximum) ||
      height > std::size_t (maximum) ||
      std::uint64_t (width) > offset_maximum / 4 / height)
    throw std::invalid_argument ("Unsupported full Edge shadow dimensions");

  if (region.x1 < 0 || region.y1 < 0 || region.x2 < region.x1 ||
      region.y2 < region.y1 || std::uint64_t (region.x2) > width ||
      std::uint64_t (region.y2) > height ||
      region.intersects != (region.x1 < region.x2 && region.y1 < region.y2) ||
      (!region.selected && (!region.intersects || region.x1 != 0 ||
                             region.y1 != 0 || std::size_t (region.x2) != width ||
                             std::size_t (region.y2) != height)))
    throw std::invalid_argument ("Invalid captured Edge selection region");

  // Ordered, nonnegative INT32 endpoints make these subtractions safe.
  const auto region_width = std::size_t (region.x2 - region.x1);
  const auto region_height = std::size_t (region.y2 - region.y1);
  // edge.c computes (x2-x1)*(y2-y1) in signed gint before /100.
  // The tile-backed original does not allocate a complete scanline.
  if (region_width && region_height > std::size_t (maximum) / region_width)
    throw std::invalid_argument ("Edge ROI area overflows legacy gint");
  if (options.wrapmode < 1 || options.wrapmode > 3 ||
      options.edgemode < 0 || options.edgemode > 5 ||
      !std::isfinite (options.amount))
    throw std::invalid_argument ("Invalid legacy Edge shadow options");
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

/* Full-drawable halo in BOTH axes. A strip can end at an ROI edge
 * without ending at a drawable edge. Wrap can add a separate four-byte read;
 * no full scanline or cropped-image boundary is substituted. */
void read (FilterRaster& input, std::uint64_t offset, std::size_t count,
           std::uint8_t *destination, const std::atomic<bool>& cancel)
{
  check_cancel (cancel);
  input.read (offset, count, destination);
  check_cancel (cancel);
}

void prepare_row (FilterRaster& input, std::size_t width, std::size_t height,
                  std::size_t y, int dy, std::size_t x, std::size_t count,
                  int wrapmode, HaloRow& row, const std::atomic<bool>& cancel)
{
  std::size_t source_y;
  if (!FilterEdgeDetail::neighbor (y, height, dy, wrapmode, source_y))
    {
      std::fill_n (row.data (), (count + 2) * 4, 0);
      return;
    }
  const auto first = x ? x - 1 : x;
  const auto end = std::min (width, x + count + 1);
  read (input, (std::uint64_t (source_y) * width + first) * 4,
        (end - first) * 4, row.data () + (x ? 0 : 4), cancel);
  if (!x)
    {
      if (wrapmode == 1)
        read (input, (std::uint64_t (source_y) * width + width - 1) * 4,
              4, row.data (), cancel);
      else if (wrapmode == 2)
        std::copy_n (row.data () + 4, 4, row.data ());
      else
        std::fill_n (row.data (), 4, 0);
    }
  if (x + count == width)
    {
      auto *last = row.data () + (count + 1) * 4;
      if (wrapmode == 1)
        read (input, std::uint64_t (source_y) * width * 4, 4, last, cancel);
      else if (wrapmode == 2)
        std::copy_n (row.data () + count * 4, 4, last);
      else
        std::fill_n (last, 4, 0);
    }
}

} // namespace

void filter_edge_shadow_validate (std::size_t width, std::size_t height,
                                  const FilterSelectionRegion& region,
                                  const EdgeOptions& options)
{
  validate (width, height, region, options);
}

FilterEdgeShadowOutcome filter_edge_shadow_raster (
  FilterRaster& input, std::size_t width, std::size_t height,
  const FilterSelectionRegion& region, const EdgeOptions& options,
  std::atomic<bool>& cancel, FilterRaster& output,
  const std::shared_ptr<FilterProgress>& progress)
{
  if (&input == &output)
    throw std::invalid_argument ("Edge shadow input/output alias");
  const auto geometry = validate (width, height, region, options);
  if (input.size () != geometry.full_size || output.size () != geometry.full_size)
    throw std::invalid_argument ("Edge shadow is not full tightly packed RGBA8");

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

      const double amount = std::max (1.0, options.amount);
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
              for (int dy = -1; dy <= 1; ++dy)
                prepare_row (input, width, height, y, dy, x, count,
                             options.wrapmode, rows[dy + 1], cancel);

              for (std::size_t pixel = 0; pixel < count; ++pixel)
                {
                  if ((pixel & 255) == 0) check_cancel (cancel);
                  auto *destination = result.data () + pixel * 4;
                  for (std::size_t channel = 0; channel < 3; ++channel)
                    {
                      std::uint8_t kernel[9];
                      for (std::size_t dx = 0; dx < 3; ++dx)
                        for (std::size_t dy = 0; dy < 3; ++dy)
                          kernel[dx * 3 + dy] = rows[dy][(pixel + dx) * 4 + channel];
                      destination[channel] = FilterEdgeDetail::detect
                        (kernel, options.edgemode, amount);
                    }
                  // Raw shadow transforms hidden RGB as well. The historical
                  // complete-output entry points retain their separate merge.
                  destination[3] = rows[1][(pixel + 1) * 4 + 3];
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
      return region.intersects ? FilterEdgeShadowOutcome::writes :
                                 FilterEdgeShadowOutcome::no_writes;
    }
  catch (const Cancelled&) { return FilterEdgeShadowOutcome::cancelled; }
}

FilterEdgeShadowOutcome filter_edge_shadow_vector (
  const std::vector<std::uint8_t>& input, std::size_t width, std::size_t height,
  const FilterSelectionRegion& region, const EdgeOptions& options,
  std::atomic<bool>& cancel, std::vector<std::uint8_t>& output,
  const std::shared_ptr<FilterProgress>& progress)
{
  const auto geometry = validate (width, height, region, options);
  if (input.size () != geometry.full_size)
    throw std::invalid_argument ("Edge shadow is not full tightly packed RGBA8");
  auto outcome = FilterEdgeShadowOutcome::cancelled;
  const auto completed = filter_native_vector (input, cancel, output,
    [&] (FilterRaster& source, FilterRaster& destination,
         std::atomic<bool>& stop, const FilterRasterFactory&) {
      outcome = filter_edge_shadow_raster (source, width, height, region,
                                           options, stop, destination, progress);
      return outcome != FilterEdgeShadowOutcome::cancelled;
    });
  return completed ? outcome : FilterEdgeShadowOutcome::cancelled;
}

} // namespace GimpPainter
