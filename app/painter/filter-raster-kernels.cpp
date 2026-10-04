/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "filter-raster-kernels.hpp"
#include "filter-edge-kernel-private.hpp"
#include "filter-gauss-kernel-private.hpp"
#include "filter-progress.hpp"
#include <array>

namespace GimpPainter {
namespace {
using Bytes = std::vector<std::uint8_t>;
using FilterGaussDetail::Check;

std::uint64_t checked_extent (FilterRaster& input, FilterRaster& output,
                              std::size_t width, std::size_t height)
{
  const auto maximum = std::uint64_t (std::numeric_limits<std::int64_t>::max ());
  if (!width || !height || std::uint64_t (width) > maximum / 4 / height ||
      &input == &output)
    throw std::invalid_argument ("Invalid filter raster dimensions/alias");
  const auto size = std::uint64_t (width) * height * 4;
  if (input.size () != size || output.size () != size)
    throw std::invalid_argument ("Filter raster is not tightly packed RGBA8");
  return size;
}

/* A strip and its two halo pixels take at most three contiguous reads even
 * across the wrapped edge. There is no per-pixel or per-column disk seek. */
void read_edge_strip (FilterRaster& input, std::size_t width, std::size_t y,
                      std::size_t x, std::size_t count, int wrap,
                      std::uint8_t *bytes)
{
  const auto first = x ? x - 1 : 0;
  const auto end = x + count < width ? x + count + 1 : width;
  const auto offset = x ? 0 : 4;
  const auto row = std::uint64_t (y) * width * 4;
  input.read (row + std::uint64_t (first) * 4, (end - first) * 4, bytes + offset);
  if (!x)
    {
      if (wrap == 1) input.read (row + (std::uint64_t (width) - 1) * 4, 4, bytes);
      else if (wrap == 2) std::copy_n (bytes + 4, 4, bytes);
      else std::fill_n (bytes, 4, 0);
    }
  if (x + count == width)
    {
      auto *last = bytes + (count + 1) * 4;
      if (wrap == 1) input.read (row, 4, last);
      else if (wrap == 2) std::copy_n (last - 4, 4, last);
      else std::fill_n (last, 4, 0);
    }
}

template<class Kernel>
void row_pass (FilterRaster& input, FilterRaster& output,
               std::size_t width, std::size_t height,
               Kernel& kernel, Check& check,
               const std::shared_ptr<FilterProgress>& progress,
               double begin, double span)
{
  Bytes src (width * 4), dest (width * 4);
  for (std::size_t y = 0; y < height; ++y)
    {
      check.now ();
      const auto offset = std::uint64_t (y) * width * 4;
      input.read (offset, src.size (), src.data ());
      FilterGaussDetail::multiply_alpha (src, check);
      kernel.process (src, dest, check);
      FilterGaussDetail::separate_alpha (dest, check);
      check.now ();
      output.write (offset, dest.size (), dest.data ());
      if (progress) progress->set_value (begin + span * double (y + 1) / height);
    }
}

void gaussian_pass (FilterRaster& input, FilterRaster& output,
                    std::size_t width, std::size_t height,
                    double radius, bool iir, Check& check,
                    const std::shared_ptr<FilterProgress>& progress,
                    double begin, double span)
{
  check.now ();
  if (iir)
    {
      FilterGaussDetail::Iir kernel (radius, width);
      row_pass (input, output, width, height, kernel, check, progress, begin, span);
    }
  else
    {
      FilterGaussDetail::Rle kernel (radius, width, check);
      row_pass (input, output, width, height, kernel, check, progress, begin, span);
    }
}

void shadow_merge (FilterRaster& input, FilterRaster& output,
                   std::uint64_t size, std::size_t width,
                   const FilterGaussDetail::Region& region, Check& check,
                   const std::shared_ptr<FilterProgress>& progress,
                   double begin, double span)
{
  const std::size_t chunk = 64 * 1024;
  Bytes original (chunk), result (chunk);
  for (std::uint64_t offset = 0; offset < size; )
    {
      check.now ();
      const auto count = static_cast<std::size_t>
        (std::min<std::uint64_t> (chunk, size - offset));
      input.read (offset, count, original.data ());
      output.read (offset, count, result.data ());
      for (std::size_t i = 0; i < count; i += 4)
        {
          check.step ();
          if (region.empty)
            std::copy_n (original.data () + i, 4, result.data () + i);
          else if (region.cropped && !region.contains ((offset + i) / 4,width))
            { std::copy_n (original.data () + i, 3, result.data () + i); result[i + 3] = 0; }
          else if (!result[i + 3])
            std::copy_n (original.data () + i, 3, result.data () + i);
        }
      check.now ();
      output.write (offset, count, result.data ());
      offset += count;
      if (progress) progress->set_value (begin + span * double (offset) / size);
    }
}
} // namespace

bool filter_edge_raster (FilterRaster& input,
                         std::size_t width, std::size_t height,
                         const EdgeOptions& options,
                         std::atomic<bool>& cancel, FilterRaster& output,
                         const std::shared_ptr<FilterProgress>& progress)
{
  checked_extent (input, output, width, height);
  if (options.wrapmode < 1 || options.wrapmode > 3 ||
      options.edgemode < 0 || options.edgemode > 5 ||
      !std::isfinite (options.amount))
    throw std::invalid_argument ("Invalid legacy edge-filter options");
  if (cancel.load (std::memory_order_relaxed)) return false;
  const double amount = std::max (1.0, options.amount);
  constexpr std::size_t strip = 1024;
  std::array<std::array<std::uint8_t, (strip + 2) * 4>, 3> rows;
  std::array<std::uint8_t, strip * 4> result;
  for (std::size_t y = 0; y < height; ++y)
    for (std::size_t x = 0; x < width; )
      {
        if (cancel.load (std::memory_order_relaxed)) return false;
        const auto count = std::min (strip, width - x);
        for (int dy = -1; dy <= 1; ++dy)
          {
            std::size_t source_y;
            if (FilterEdgeDetail::neighbor (y, height, dy, options.wrapmode, source_y))
              read_edge_strip (input, width, source_y, x, count,
                               options.wrapmode, rows[dy + 1].data ());
            else
              std::fill_n (rows[dy + 1].data (), (count + 2) * 4, 0);
          }
        for (std::size_t i = 0; i < count; ++i)
          {
            if ((i & 255) == 0 && cancel.load (std::memory_order_relaxed)) return false;
            const auto *original = rows[1].data () + (i + 1) * 4;
            auto *destination = result.data () + i * 4;
            if (!original[3]) std::copy_n (original, 4, destination);
            else
              {
                std::uint8_t kernels[3][9];
                for (int dx = 0; dx < 3; ++dx)
                  for (int dy = 0; dy < 3; ++dy)
                    for (int channel = 0; channel < 3; ++channel)
                      kernels[channel][dx * 3 + dy] = rows[dy][(i + dx) * 4 + channel];
                for (int channel = 0; channel < 3; ++channel)
                  destination[channel] = FilterEdgeDetail::detect
                    (kernels[channel], options.edgemode, amount);
                destination[3] = original[3];
              }
          }
        if (cancel.load (std::memory_order_relaxed)) return false;
        output.write ((std::uint64_t (y) * width + x) * 4, count * 4, result.data ());
        x += count;
        if (progress)
          progress->set_value (0.99 * (double (y) * width + x) /
                               (double (width) * height));
      }
  if (cancel.load (std::memory_order_relaxed)) return false;
  output.flush ();
  if (cancel.load (std::memory_order_relaxed)) return false;
  if (progress) progress->set_value (1.0);
  return true;
}

bool filter_gauss_raster (FilterRaster& input,
                          std::size_t width, std::size_t height,
                          const GaussOptions& options,
                          std::atomic<bool>& cancel, FilterRaster& output,
                          const FilterRasterFactory& scratch_factory,
                          const std::shared_ptr<FilterProgress>& progress)
{
  const auto size = checked_extent (input, output, width, height);
  const auto legacy_maximum = static_cast<std::size_t>
    (std::numeric_limits<int>::max () / 4);
  if (width > legacy_maximum || height > legacy_maximum)
    throw std::invalid_argument ("Invalid legacy Gaussian dimensions");
  if (!std::isfinite (options.horizontal) || !std::isfinite (options.vertical) ||
      (options.horizontal <= 0.0 && options.vertical <= 0.0) ||
      options.method < 0 || options.method > 1)
    throw std::invalid_argument ("Invalid legacy Gaussian options");
  const auto region = FilterGaussDetail::legacy_region (width,height,options.horizontal,options.vertical);
  Check check { cancel };
  try
    {
      check.now ();
      const bool iir = options.method == 0 &&
                       options.horizontal > 1.0 && options.vertical > 1.0;
      // Each phase traverses the whole raster. Reserve completion until flush.
      const double span = 0.99 / (3 * (options.vertical > 0.0) +
                                 (options.horizontal > 0.0) + 1);
      double begin = 0.0;
      if (options.vertical > 0)
        {
          if (!scratch_factory)
            throw std::invalid_argument ("Missing Gaussian scratch factory");
          auto scratch = scratch_factory (size);
          if (scratch.get () == &input || scratch.get () == &output)
            {
              /* A broken factory must not delete a borrowed input/output. */
              scratch.release ();
              throw std::invalid_argument ("Aliased Gaussian scratch storage");
            }
          if (!scratch || scratch->size () != size)
            throw std::invalid_argument ("Invalid Gaussian scratch storage");
          check.now ();
          if (!transpose_filter_rgba (input, *scratch, width, height, cancel)) return false;
          begin += span;
          if (progress) progress->set_value (begin);
          gaussian_pass (*scratch, *scratch, height, width, options.vertical, iir,
                         check, progress, begin, span);
          begin += span;
          if (!transpose_filter_rgba (*scratch, output, height, width, cancel)) return false;
          begin += span;
          if (progress) progress->set_value (begin);
        }
      if (options.horizontal > 0)
        {
          gaussian_pass (options.vertical > 0 ? output : input, output,
                         width, height, options.horizontal, iir,
                         check, progress, begin, span);
          begin += span;
        }
      shadow_merge (input, output, size, width, region, check, progress, begin, span);
      check.now ();
      output.flush ();
      check.now ();
      if (progress) progress->set_value (1.0);
      return true;
    }
  catch (const FilterGaussDetail::Cancelled&)
    { return false; }
}
} // namespace GimpPainter
