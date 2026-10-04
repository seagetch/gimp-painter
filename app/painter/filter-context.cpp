/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "filter-context.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace GimpPainter {
namespace {

/* Preserve the old signed arithmetic right shift explicitly. C++ division
 * truncates toward zero, while INT_MULT with a negative difference rounds
 * using arithmetic shifts on the pinned old platform. */
int floor_shift_8 (int value) noexcept
{
  return value >= 0 ? value / 256 : -((-value + 255) / 256);
}

int multiply_255 (int a, int b) noexcept
{
  const int product = a * b + 128;
  return floor_shift_8 (floor_shift_8 (product) + product);
}

unsigned divide_nearest (unsigned numerator, unsigned denominator) noexcept
{
  return numerator / denominator +
         (numerator % denominator > denominator / 2);
}

double unit_coverage (double value) noexcept
{ return std::max (0.0, std::min (1.0, value)); }

double interpolate_native (double original, double shadow, double ratio) noexcept
{
  if (ratio == 0.0) return original;
  if (ratio == 1.0) return shadow;
  // Same-sign subtraction and opposite-sign weighted addition cannot overflow
  // for finite endpoints and a ratio in [0,1]. Preserve exact endpoints above.
  if (std::signbit (original) != std::signbit (shadow))
    return original * (1.0 - ratio) + shadow * ratio;
  return original + (shadow - original) * ratio;
}

} // namespace

bool FilterSelectionScan::reset (std::int32_t width, std::int32_t height) noexcept
{
  if (width <= 0 || height <= 0) return false;
  width_ = width; height_ = height; consumed_ = 0;
  bounds_ = {};
  return true;
}

bool FilterSelectionScan::append (std::uint64_t offset, const std::uint8_t *coverage,
                                 std::size_t count) noexcept
{ return append_samples (offset, coverage, count); }

bool FilterSelectionScan::append_native (std::uint64_t offset, const double *coverage,
                                        std::size_t count) noexcept
{ return append_samples (offset, coverage, count); }

template<class Sample>
bool FilterSelectionScan::append_samples (std::uint64_t offset, const Sample *coverage,
                                         std::size_t count) noexcept
{
  const auto total = static_cast<std::uint64_t> (width_) * height_;
  if (!width_ || offset != consumed_ || consumed_ > total ||
      count > total - consumed_ || count > std::numeric_limits<std::size_t>::max () / sizeof (Sample) ||
      (count && !coverage)) return false;
  for (std::size_t i = 0; i < count; ++i)
    if (!std::isfinite (coverage[i])) return false;
  for (std::size_t i = 0; i < count; ++i)
    if (coverage[i] > 0)
      {
        const auto x = static_cast<std::int32_t> ((offset + i) % width_);
        const auto y = static_cast<std::int32_t> ((offset + i) / width_);
        if (bounds_.empty)
          bounds_ = { false, x, y, x + 1, y + 1 };
        else
          {
            bounds_.x1 = std::min (bounds_.x1, x);
            bounds_.y1 = std::min (bounds_.y1, y);
            bounds_.x2 = std::max (bounds_.x2, x + 1);
            bounds_.y2 = std::max (bounds_.y2, y + 1);
          }
      }
  consumed_ += count;
  return true;
}

bool FilterSelectionScan::finish (FilterSelectionBounds& bounds) const noexcept
{
  if (!width_ || consumed_ != static_cast<std::uint64_t> (width_) * height_)
    return false;
  bounds = bounds_;
  return true;
}

bool
filter_selection_region (const FilterSelectionBounds& selection,
                         std::int32_t offset_x, std::int32_t offset_y,
                         std::int32_t width, std::int32_t height,
                         FilterSelectionRegion& region) noexcept
{
  if (width <= 0 || height <= 0 ||
      (!selection.empty && (selection.x1 >= selection.x2 ||
                            selection.y1 >= selection.y2)))
    return false;
  FilterSelectionRegion next { !selection.empty, true, 0, 0, width, height };
  if (!selection.empty)
    {
      const auto clamp = [] (std::int32_t value, std::int32_t offset,
                             std::int32_t extent) noexcept {
        return static_cast<std::int32_t> (std::max<std::int64_t> (0,
          std::min<std::int64_t> (static_cast<std::int64_t> (value) - offset, extent)));
      };
      next.x1 = clamp (selection.x1, offset_x, width);
      next.y1 = clamp (selection.y1, offset_y, height);
      next.x2 = clamp (selection.x2, offset_x, width);
      next.y2 = clamp (selection.y2, offset_y, height);
      next.intersects = next.x1 < next.x2 && next.y1 < next.y2;
    }
  region = next;
  return true;
}

bool
filter_replace_inten_row (const std::uint8_t *original,
                         const std::uint8_t *shadow,
                         const std::uint8_t *selection,
                         std::uint8_t       *output,
                         std::size_t        pixels,
                         unsigned           channels,
                         unsigned           active_components,
                         std::uint8_t       opacity) noexcept
{
  if (channels < 1 || channels > 4 ||
      pixels > std::numeric_limits<std::size_t>::max () / channels ||
      (pixels && (!original || !shadow || !output)))
    return false;
  const bool has_alpha = channels % 2 == 0;
  const unsigned color_channels = channels - has_alpha;
  for (std::size_t pixel = 0; pixel < pixels; ++pixel)
    {
      const unsigned mask = selection ? selection[pixel] : 255;
      const unsigned old_alpha = has_alpha ? original[color_channels] : 255;
      const unsigned new_alpha = has_alpha ? shadow[color_channels] : 255;
      const unsigned coverage = multiply_255 (mask, opacity);
      const unsigned mixed_alpha = old_alpha + multiply_255 (
        static_cast<int> (new_alpha) - static_cast<int> (old_alpha), coverage);
      unsigned ratio = 0;
      if (mixed_alpha)
        ratio = divide_nearest ((mask * opacity / 255) * new_alpha,
                                mixed_alpha);
      for (unsigned channel = 0; channel < color_channels; ++channel)
        {
          const unsigned old_value = original[channel];
          const unsigned new_value = shadow[channel];
          unsigned result = old_value;
          if (mixed_alpha && (active_components & (1U << channel)))
            {
              if (new_value > old_value)
                result += divide_nearest ((new_value - old_value) * ratio, 255);
              else
                result -= divide_nearest ((old_value - new_value) * ratio, 255);
            }
          output[channel] = static_cast<std::uint8_t> (result);
        }
      if (has_alpha)
        output[color_channels] = (active_components & (1U << color_channels)) ?
                                 mixed_alpha : old_alpha;
      original += channels;
      shadow += channels;
      output += channels;
    }
  return true;
}

bool
filter_replace_native_row (const double *original,
                           const double *shadow,
                           const double *selection,
                           double       *output,
                           std::size_t   pixels,
                           unsigned      channels,
                           unsigned      active_components,
                           double        opacity) noexcept
{
  if (channels < 1 || channels > 4 ||
      pixels > std::numeric_limits<std::size_t>::max () / channels / sizeof (double) ||
      (pixels && (!original || !shadow || !output)) ||
      !std::isfinite (opacity) || opacity < 0.0 || opacity > 1.0)
    return false;
  const bool has_alpha = channels % 2 == 0;
  const unsigned color_channels = channels - has_alpha;
  // Validate the entire row before writing, including inactive and masked
  // samples. The owner can then discard a failed shadow atomically.
  for (std::size_t i = 0; i < pixels * channels; ++i)
    if (!std::isfinite (original[i]) || !std::isfinite (shadow[i])) return false;
  if (selection)
    for (std::size_t i = 0; i < pixels; ++i)
      if (!std::isfinite (selection[i])) return false;
  for (std::size_t pixel = 0; pixel < pixels; ++pixel)
    {
      const double coverage = (selection ? unit_coverage (selection[pixel]) : 1.0) * opacity;
      const double old_alpha = has_alpha ? unit_coverage (original[color_channels]) : 1.0;
      const double new_alpha = has_alpha ? unit_coverage (shadow[color_channels]) : 1.0;
      const double mixed_alpha = coverage == 0.0 ? old_alpha :
                                 coverage == 1.0 ? new_alpha :
                                 old_alpha + (new_alpha - old_alpha) * coverage;
      const double ratio = mixed_alpha != 0.0 ? unit_coverage (coverage * new_alpha / mixed_alpha) : 0.0;
      for (unsigned channel = 0; channel < color_channels; ++channel)
        {
          double value = original[channel];
          if (mixed_alpha != 0.0 && (active_components & (1U << channel)))
            {
              value = interpolate_native (value, shadow[channel], ratio);
            }
          output[channel] = value;
        }
      if (has_alpha)
        output[color_channels] = (active_components & (1U << color_channels)) ?
                                 mixed_alpha : original[color_channels];
      original += channels;
      shadow += channels;
      output += channels;
    }
  return true;
}

} // namespace GimpPainter
