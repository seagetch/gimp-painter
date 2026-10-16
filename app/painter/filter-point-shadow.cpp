/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "filter-point-shadow.hpp"
#include "filter-native-kernels.hpp"
#include "filter-progress.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>

namespace GimpPainter {
namespace {

static_assert (std::numeric_limits<int>::digits >= 31,
               "Point kernel must preserve the full signed INT32 threshold");

struct Geometry
{
  std::uint64_t full_size, crop_size;
  std::size_t crop_width, crop_height;
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
    throw std::invalid_argument ("Unsupported full point shadow dimensions");
  const auto size = std::uint64_t (width) * height * 4;

  if (region.x1 < 0 || region.y1 < 0 || region.x2 < region.x1 ||
      region.y2 < region.y1 || std::uint64_t (region.x2) > width ||
      std::uint64_t (region.y2) > height ||
      region.intersects != (region.x1 < region.x2 && region.y1 < region.y2) ||
      (!region.selected && (!region.intersects || region.x1 != 0 ||
                             region.y1 != 0 || std::size_t (region.x2) != width ||
                             std::size_t (region.y2) != height)))
    throw std::invalid_argument ("Invalid captured point selection region");

  // Subtraction is safe only after the bounded, ordered INT32 checks above.
  const auto crop_width = std::size_t (region.x2 - region.x1);
  const auto crop_height = std::size_t (region.y2 - region.y1);
  // Threshold/Max RGB's gimp_rgn_iterate2 total_area and Value Invert's own
  // pixel-region loop width*height both multiply signed gint operands. Reject
  // their undefined overflow domain, admitting positive one-dimensional crops.
  if (std::uint64_t (crop_width) * crop_height > std::uint64_t (maximum))
    throw std::invalid_argument ("Point intersection overflows legacy total area");
  return {size, std::uint64_t (crop_width) * crop_height * 4,
          crop_width, crop_height};
}

struct Cancelled {};
void check_cancel (const std::atomic<bool>& cancel)
{
  if (cancel.load (std::memory_order_relaxed)) throw Cancelled {};
}

/* Progress is driven by actual bounded writes, not by an unmapped kernel
 * progress object. Thus zero initialization cannot be followed by a regression
 * to the kernel's first small percentage, and flush never reports completion. */
class WorkProgress
{
  const std::shared_ptr<FilterProgress>& progress_;
  const double total_;
  std::uint64_t completed_ = 0;
public:
  WorkProgress (const std::shared_ptr<FilterProgress>& progress,
                std::uint64_t full_size, std::uint64_t crop_size)
    : progress_ (progress), total_ (double (full_size) + double (crop_size)) {}
  void wrote (std::size_t count)
  {
    // Two signed-64-bit extents together fit an unsigned-64-bit counter.
    completed_ += count;
    if (progress_) progress_->set_value (0.99 * double (completed_) / total_);
  }
};

/* Checked worker-only rectangular views. Even a request spanning multiple
 * crop rows is split before reaching the full raster, never reading a halo
 * outside the captured region. Read and write capabilities stay separate. */
class CropRaster final : public FilterRaster
{
  FilterRaster& full_;
  const std::uint64_t full_stride_, first_, stride_, size_;
  const bool source_;
  std::atomic<bool>& cancel_;
  WorkProgress& progress_;

  void check_range (std::uint64_t offset, std::size_t count, const void *bytes) const
  {
    if (offset > size_ || count > size_ - offset || (count && !bytes))
      throw std::invalid_argument ("Point crop transfer outside extent");
  }
public:
  CropRaster (FilterRaster& full, std::size_t full_width,
              const FilterSelectionRegion& region, const Geometry& geometry,
              bool source, std::atomic<bool>& cancel, WorkProgress& progress)
    : full_ (full), full_stride_ (std::uint64_t (full_width) * 4),
      first_ ((std::uint64_t (region.y1) * full_width + region.x1) * 4),
      stride_ (std::uint64_t (geometry.crop_width) * 4),
      size_ (geometry.crop_size), source_ (source), cancel_ (cancel), progress_ (progress) {}
  std::uint64_t size () const noexcept override { return size_; }
  void read (std::uint64_t offset, std::size_t count, std::uint8_t *bytes) override
  {
    if (!source_) throw std::logic_error ("Point read private shadow");
    check_range (offset, count, bytes);
    while (count)
      {
        check_cancel (cancel_);
        const auto row = offset / stride_, column = offset % stride_;
        const auto part = std::size_t (std::min<std::uint64_t> (16384,
          std::min<std::uint64_t> (count, stride_ - column)));
        full_.read (first_ + row * full_stride_ + column, part, bytes);
        check_cancel (cancel_);
        offset += part; count -= part; bytes += part;
      }
  }
  void write (std::uint64_t offset, std::size_t count, const std::uint8_t *bytes) override
  {
    if (source_) throw std::logic_error ("Point wrote source input");
    check_range (offset, count, bytes);
    while (count)
      {
        check_cancel (cancel_);
        const auto row = offset / stride_, column = offset % stride_;
        const auto part = std::size_t (std::min<std::uint64_t> (16384,
          std::min<std::uint64_t> (count, stride_ - column)));
        full_.write (first_ + row * full_stride_ + column, part, bytes);
        check_cancel (cancel_);
        progress_.wrote (part);
        offset += part; count -= part; bytes += part;
      }
  }
  void flush () override
  {
    if (source_) throw std::logic_error ("Point flushed source input");
    check_cancel (cancel_);
    full_.flush ();
    check_cancel (cancel_);
  }
};

} // namespace

void filter_point_shadow_validate (std::size_t width, std::size_t height,
                                   const FilterSelectionRegion& region)
{
  validate (width, height, region);
}

FilterPointShadowOutcome filter_point_shadow_raster (
  FilterRaster& input, std::size_t width, std::size_t height,
  const FilterSelectionRegion& region, FilterPointShadowOperation operation,
  std::int32_t argument,
  std::atomic<bool>& cancel, FilterRaster& output,
  const std::shared_ptr<FilterProgress>& progress)
{
  if (operation != FilterPointShadowOperation::threshold_alpha &&
      operation != FilterPointShadowOperation::max_rgb &&
      operation != FilterPointShadowOperation::value_invert)
    throw std::invalid_argument ("Unknown point shadow operation");
  if (&input == &output)
    throw std::invalid_argument ("Point shadow input/output alias");
  const auto geometry = validate (width, height, region);
  if (input.size () != geometry.full_size || output.size () != geometry.full_size)
    throw std::invalid_argument ("Point shadow is not full tightly packed RGBA8");
  try
    {
      check_cancel (cancel);
      WorkProgress work (progress, geometry.full_size, geometry.crop_size);
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
      if (region.intersects)
        {
          CropRaster source (input, width, region, geometry, true, cancel, work);
          CropRaster destination (output, width, region, geometry, false, cancel, work);
          bool completed = false;
          switch (operation)
            {
            case FilterPointShadowOperation::max_rgb:
              completed = filter_max_rgb_raw_raster (source, destination,
                geometry.crop_width, geometry.crop_height, argument, cancel);
              break;
            case FilterPointShadowOperation::value_invert:
              completed = filter_value_invert_raw_raster (source, destination,
                geometry.crop_width, geometry.crop_height, cancel);
              break;
            case FilterPointShadowOperation::threshold_alpha:
              completed = filter_point_raster (source, destination,
                geometry.crop_width, geometry.crop_height,
                FilterPoint::threshold_alpha, argument, false, cancel);
              break;
            }
          if (!completed)
            return FilterPointShadowOutcome::cancelled;
        }
      else
        {
          check_cancel (cancel);
          output.flush ();
        }
      check_cancel (cancel);
      if (progress) progress->set_value (1.0);
      return region.intersects ? FilterPointShadowOutcome::shadow :
                                 FilterPointShadowOutcome::no_merge;
    }
  catch (const Cancelled&) { return FilterPointShadowOutcome::cancelled; }
}

FilterPointShadowOutcome filter_point_shadow_vector (
  const std::vector<std::uint8_t>& input, std::size_t width, std::size_t height,
  const FilterSelectionRegion& region, FilterPointShadowOperation operation,
  std::int32_t argument,
  std::atomic<bool>& cancel, std::vector<std::uint8_t>& output,
  const std::shared_ptr<FilterProgress>& progress)
{
  if (operation != FilterPointShadowOperation::threshold_alpha &&
      operation != FilterPointShadowOperation::max_rgb &&
      operation != FilterPointShadowOperation::value_invert)
    throw std::invalid_argument ("Unknown point shadow operation");
  const auto geometry = validate (width, height, region);
  if (input.size () != geometry.full_size)
    throw std::invalid_argument ("Point shadow is not full tightly packed RGBA8");
  auto outcome = FilterPointShadowOutcome::cancelled;
  const auto completed = filter_native_vector (input, cancel, output,
    [&] (FilterRaster& source, FilterRaster& destination,
         std::atomic<bool>& stop, const FilterRasterFactory&) {
      outcome = filter_point_shadow_raster (source, width, height, region,
                                             operation, argument, stop, destination, progress);
      return outcome != FilterPointShadowOutcome::cancelled;
    });
  return completed ? outcome : FilterPointShadowOutcome::cancelled;
}

FilterPointShadowOutcome filter_point_shadow_raster (
  FilterRaster& input, std::size_t width, std::size_t height,
  const FilterSelectionRegion& region, std::int32_t threshold,
  std::atomic<bool>& cancel, FilterRaster& output,
  const std::shared_ptr<FilterProgress>& progress)
{
  return filter_point_shadow_raster (input, width, height, region,
    FilterPointShadowOperation::threshold_alpha, threshold, cancel, output, progress);
}

FilterPointShadowOutcome filter_point_shadow_vector (
  const std::vector<std::uint8_t>& input, std::size_t width, std::size_t height,
  const FilterSelectionRegion& region, std::int32_t threshold,
  std::atomic<bool>& cancel, std::vector<std::uint8_t>& output,
  const std::shared_ptr<FilterProgress>& progress)
{
  return filter_point_shadow_vector (input, width, height, region,
    FilterPointShadowOperation::threshold_alpha, threshold, cancel, output, progress);
}

} // namespace GimpPainter
