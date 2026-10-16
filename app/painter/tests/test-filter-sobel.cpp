/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "filter-sobel.hpp"
#include "filter-native-kernels.hpp"
#include "filter-progress.hpp"
#include "legacy-sobel-oracle.h"
#include <glib.h>
#include <glib/gstdio.h>
#include <algorithm>
#include <array>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace GimpPainter;
using Bytes = std::vector<std::uint8_t>;
using Outcome = FilterSobelShadowOutcome;
namespace {
constexpr auto int_maximum = std::numeric_limits<std::int32_t>::max ();
std::string directory;
void require (bool value, const std::string& reason)
{ if (!value) throw std::runtime_error (reason); }
template<class Error, class Action> void rejects (Action action, const char *reason)
{
  bool rejected = false;
  try { action (); } catch (const Error&) { rejected = true; }
  require (rejected, reason);
}

Bytes fixture (std::size_t width, std::size_t height, unsigned channels, unsigned pattern = 0)
{
  Bytes result (width * height * channels);
  std::uint32_t state = 0x1395e63aU;
  const std::uint8_t alphas[] = {0, 1, 63, 127, 128, 191, 254, 255};
  for (std::size_t y = 0; y < height; ++y)
    for (std::size_t x = 0; x < width; ++x)
      for (unsigned channel = 0; channel < channels; ++channel)
        {
          state = state * 1664525U + 1013904223U;
          result[(y * width + x) * channels + channel] = channels % 2 == 0 && channel == channels - 1 ?
            alphas[(x + y * 3) % 8] : pattern ?
              std::uint8_t (((x / 2 + y / 3 + channel) % 2) ? 255 : 0) : std::uint8_t (state >> 24);
        }
  return result;
}
Bytes expand (const Bytes& native, unsigned channels)
{
  Bytes result (native.size () / channels * 4);
  for (std::size_t pixel = 0; pixel < native.size () / channels; ++pixel)
    {
      result[pixel * 4] = native[pixel * channels];
      result[pixel * 4 + 1] = native[pixel * channels + (channels <= 2 ? 0 : 1)];
      result[pixel * 4 + 2] = native[pixel * channels + (channels <= 2 ? 0 : 2)];
      result[pixel * 4 + 3] = channels % 2 == 0 ? native[pixel * channels + channels - 1] : 255;
    }
  return result;
}
FilterSelectionRegion full_region (std::size_t width, std::size_t height, bool selected = false)
{ return {selected, true, 0, 0, std::int32_t (width), std::int32_t (height)}; }

/* Only transport/placement lives here. All row preparation and gradient
 * arithmetic is in the separately compiled unchanged C source. Native Y/RGB
 * has no alpha result. The promoted RGBA transport synthesizes it by the old
 * >10 color-gradient rule. Native Gray-alpha/RGBA keeps the oracle alpha. */
Bytes expected_shadow (const Bytes& native, std::size_t width, std::size_t height,
                       unsigned channels, const FilterSelectionRegion& region,
                       const SobelOptions& options)
{
  Bytes result (width * height * 4, 0), calculated (native.size (), 0xa7);
  const auto w = std::size_t (region.x2 - region.x1), h = std::size_t (region.y2 - region.y1);
  require (legacy_sobel_oracle (native.data (), width, height, channels,
                                region.x1, region.y1, w, h,
                                options.horizontal, options.vertical, options.keep_sign,
                                calculated.data ()) == 1,
           "Source-extracted C oracle rejected supported geometry");
  if (!region.intersects)
    {
      require (std::all_of (calculated.begin (), calculated.end (),
                           [] (std::uint8_t value) { return value == 0; }),
               "Source-C no-intersection branch wrote shadow pixels");
      return result;
    }
  for (std::size_t y = std::size_t (region.y1); y < std::size_t (region.y2); ++y)
    for (std::size_t x = std::size_t (region.x1); x < std::size_t (region.x2); ++x)
      {
        const auto pixel = y * width + x;
        result[pixel * 4] = calculated[pixel * channels];
        result[pixel * 4 + 1] = calculated[pixel * channels + (channels <= 2 ? 0 : 1)];
        result[pixel * 4 + 2] = calculated[pixel * channels + (channels <= 2 ? 0 : 2)];
        result[pixel * 4 + 3] = channels % 2 == 0 ? calculated[pixel * channels + channels - 1] :
          (result[pixel * 4] > 10 || result[pixel * 4 + 1] > 10 || result[pixel * 4 + 2] > 10 ? 255 : 0);
      }
  return result;
}

class Memory final : public FilterRaster
{
public:
  Bytes data;
  explicit Memory (const Bytes& bytes) : data (bytes) {}
  explicit Memory (std::size_t size) : data (size, 0xa7) {}
  std::uint64_t size () const noexcept override { return data.size (); }
  void read (std::uint64_t offset, std::size_t count, std::uint8_t *bytes) override
  {
    require (offset <= size () && count <= size () - offset, "Memory read outside extent");
    std::copy_n (data.data () + offset, count, bytes);
  }
  void write (std::uint64_t offset, std::size_t count, const std::uint8_t *bytes) override
  {
    require (offset <= size () && count <= size () - offset, "Memory write outside extent");
    std::copy_n (bytes, count, data.data () + offset);
  }
  void flush () override {}
};

enum class Operation { none, read, write, flush };
struct StorageFailure : std::runtime_error
{ StorageFailure () : std::runtime_error ("Injected shadow storage error") {} };
struct Audit
{
  const std::size_t width, height;
  const FilterSelectionRegion region;
  const std::uint64_t full_size, crop_size;
  std::atomic<bool>& cancel;
  std::shared_ptr<FilterProgress> progress;
  std::size_t reads = 0, writes = 0, flushes = 0, zero_writes = 0;
  std::size_t max_read = 0, max_write = 0, zero_updates = 0, crop_updates = 0;
  std::uint64_t zero_written = 0, crop_written = 0;
  unsigned read_rows = 0, strip_reads = 0;
  double last_progress = 0;
  Operation cancel_on = Operation::none, fail_on = Operation::none;
  std::size_t cancel_at = 0, fail_at = 0;

  Audit (std::size_t width, std::size_t height, const FilterSelectionRegion& region,
         std::atomic<bool>& cancel, std::shared_ptr<FilterProgress> progress = {})
    : width (width), height (height), region (region), full_size (std::uint64_t (width) * height * 4),
      crop_size (std::uint64_t (region.x2 - region.x1) * (region.y2 - region.y1) * 4),
      cancel (cancel), progress (std::move (progress)) {}
  void observe ()
  {
    if (!progress) return;
    FilterProgress::Snapshot snapshot;
    require (progress->try_snapshot (snapshot), "Cannot inspect single-threaded progress");
    require (snapshot.value >= last_progress && snapshot.value <= 1, "Shadow progress regressed");
    if (snapshot.value > last_progress && snapshot.value < 1)
      {
        if (crop_written) ++crop_updates;
        else ++zero_updates;
      }
    last_progress = snapshot.value;
  }
  void event (Operation operation, std::uint64_t offset = 0, std::size_t count = 0,
              const std::uint8_t *bytes = nullptr)
  {
    observe ();
    require (!progress || last_progress < 1, "Shadow completed before final I/O");
    require (!cancel.load (), "Wrapper performed I/O after cancellation");
    std::size_t ordinal = 0;
    if (operation == Operation::read || operation == Operation::write)
      require (count > 0 && offset % 4 == 0 && count % 4 == 0 && offset <= full_size &&
               count <= full_size - offset, "Invalid full-raster transfer");
    if (operation == Operation::write && zero_written < full_size)
      {
        ordinal = ++writes; ++zero_writes; max_write = std::max (max_write, count);
        require (offset == zero_written && count == std::min<std::uint64_t> (16384, full_size - offset),
                 "Shadow zero-fill is not contiguous and bounded");
        require (std::all_of (bytes, bytes + count, [] (std::uint8_t value) { return value == 0; }),
                 "Shadow initialization wrote nonzero bytes");
        zero_written += count;
      }
    else if (operation == Operation::read || operation == Operation::write)
      {
        require (zero_written == full_size && region.intersects && crop_written < crop_size,
                 "Kernel I/O preceded zero-fill or used an empty/finished crop");
        const auto crop_width = std::size_t (region.x2 - region.x1);

        const auto x = std::size_t (crop_written / 4 % crop_width), y = crop_written / 4 / crop_width;
        const auto full_x = std::size_t (offset / 4 % width), full_y = offset / 4 / width;
        require (full_x >= std::size_t (region.x1) &&
                 full_x + count / 4 <= std::size_t (region.x2) && full_y < height,
                 "Sobel horizontal halo escaped ROI or crossed full row");
        if (operation == Operation::read)
          {
            ordinal = ++reads; max_read = std::max (max_read, count);
            const auto first = x ? x - 1 : 0, end = std::min (crop_width, x + 1024 + 1);
            require (count <= 1026 * 4 && full_x == std::size_t (region.x1) + first &&
                     count == (end - first) * 4, "Kernel read wrong strip halo");
            const auto center_y = std::size_t (region.y1) + y;
            require (full_y == center_y || (center_y && full_y == center_y - 1) ||
                     (center_y + 1 < height && full_y == center_y + 1),
                     "Kernel vertical halo did not clamp against full drawable");
            require (strip_reads < 3, "More than three reads per Sobel strip");
            const auto expected_y = strip_reads == 0 ? (center_y ? center_y - 1 : 0) :
                                    strip_reads == 1 ? center_y : std::min (height - 1, center_y + 1);
            require (full_y == expected_y, "Sobel source rows are not top/current/bottom");
            ++strip_reads;
            read_rows |= full_y == center_y ? 2U : full_y < center_y ? 1U : 4U;
          }
        else
          {
            ordinal = ++writes; max_write = std::max (max_write, count);
            require (full_x == std::size_t (region.x1) + x && full_y == std::size_t (region.y1) + y &&
                     count == std::min (std::size_t (1024), crop_width - x) * 4,
                     "Crop writes are not exact bounded logical strips");
            const auto center_y = std::size_t (region.y1) + y;
            const auto required_rows = 2U | (center_y ? 1U : 0U) |
                                       (center_y + 1 < height ? 4U : 0U);
            require (read_rows == required_rows && strip_reads == 3,
                     "Sobel write lacked full-image vertical neighborhood");
            read_rows = strip_reads = 0; crop_written += count;
          }
      }
    else
      {
        ordinal = ++flushes;
        require (zero_written == full_size && crop_written == crop_size && flushes == 1,
                 "Shadow flushed before all bytes were initialized/written or flushed twice");
      }
    if (operation == fail_on && ordinal == fail_at) throw StorageFailure ();
    if (operation == cancel_on && ordinal == cancel_at) cancel.store (true);
  }
};
class Observed final : public FilterRaster
{
  FilterRaster& base_;
  Audit& audit_;
  const bool source_;
public:
  Observed (FilterRaster& base, Audit& audit, bool source) : base_ (base), audit_ (audit), source_ (source) {}
  std::uint64_t size () const noexcept override { return base_.size (); }
  void read (std::uint64_t offset, std::size_t count, std::uint8_t *bytes) override
  {
    require (source_, "Wrapper read shadow storage");
    audit_.event (Operation::read, offset, count); base_.read (offset, count, bytes);
  }
  void write (std::uint64_t offset, std::size_t count, const std::uint8_t *bytes) override
  {
    require (!source_, "Wrapper wrote source input");
    audit_.event (Operation::write, offset, count, bytes); base_.write (offset, count, bytes);
  }
  void flush () override
  { require (!source_, "Wrapper flushed source input"); audit_.event (Operation::flush); base_.flush (); }
};

Outcome audited_vector (const Bytes& input, const SobelOptions& options, Bytes& output, Audit& audit)
{
  auto outcome = Outcome::cancelled;
  const auto complete = filter_native_vector (input, audit.cancel, output,
    [&] (FilterRaster& in, FilterRaster& out, std::atomic<bool>& cancel, const FilterRasterFactory&) {
      Observed source (in, audit, true), destination (out, audit, false);
      outcome = filter_sobel_shadow_raster (source, audit.width, audit.height, audit.region,
                                            options, cancel, destination, audit.progress);
      return outcome != Outcome::cancelled;
    });
  return complete ? outcome : Outcome::cancelled;
}
Bytes spill_result (const Bytes& input, std::size_t width, std::size_t height,
                    const FilterSelectionRegion& region, const SobelOptions& options)
{
  TemporaryFilterRaster in (directory, input.size ()), out (directory, input.size ());
  in.write (0, input.size (), input.data ());
  // Poison actual disk storage too: outside-ROI zeros must be written explicitly.
  const Bytes poison (input.size (), 0xa7); out.write (0, poison.size (), poison.data ());
  std::atomic<bool> cancel {false}; auto progress = std::make_shared<FilterProgress> ();
  Audit audit (width, height, region, cancel, progress);
  Observed source (in, audit, true), destination (out, audit, false);
  const auto outcome = filter_sobel_shadow_raster (source, width, height, region, options,
                                                  cancel, destination, progress);
  require (outcome == (region.intersects ? Outcome::shadow : Outcome::no_merge), "Wrong spill outcome");
  audit.observe ();
  require (audit.last_progress == 1 && audit.flushes == 1, "Spill output did not complete after flush");
  Bytes result (input.size ()), unchanged (input.size ());
  out.read (0, result.size (), result.data ()); in.read (0, unchanged.size (), unchanged.data ());
  require (unchanged == input, "Spill changed source input");
  return result;
}

Bytes vector_result (const Bytes& input, std::size_t width, std::size_t height,
                     const FilterSelectionRegion& region, const SobelOptions& options)
{
  std::atomic<bool> cancel {false}; Bytes result;
  const auto outcome = filter_sobel_shadow_vector (input, width, height, region, options, cancel, result);
  require (outcome == (region.intersects ? Outcome::shadow : Outcome::no_merge), "Wrong vector outcome");
  return result;
}
void raw_contract (const Bytes& output, std::size_t width, std::size_t height,
                   const FilterSelectionRegion& region)
{
  for (std::size_t y = 0; y < height; ++y)
    for (std::size_t x = 0; x < width; ++x)
      {
        const auto at = (y * width + x) * 4;
        const bool inside = region.intersects && x >= std::size_t (region.x1) && x < std::size_t (region.x2) &&
                             y >= std::size_t (region.y1) && y < std::size_t (region.y2);
        if (!inside)
          require (std::all_of (output.begin () + at, output.begin () + at + 4,
                               [] (std::uint8_t value) { return value == 0; }), "Outside-ROI shadow was not zero");
        else
          require (output[at + 3] == (output[at] > 10 || output[at + 1] > 10 || output[at + 2] > 10 ? 255 : 0),
                   "Alpha was not synthesized from the color-gradient threshold");
      }
}
void source_oracle_parity ()
{
  unsigned cases = 0;
  const SobelOptions flags[] = {
    {0,0,0}, {0,0,1}, {0,1,0}, {0,1,1}, {1,0,0}, {1,0,1}, {1,1,0}, {1,1,1},
    {-1,0,int_maximum}, {0,std::numeric_limits<std::int32_t>::min (),0},
    {int_maximum,-7,std::numeric_limits<std::int32_t>::min ()}, {0,0,-7}
  };
  for (unsigned channels = 1; channels <= 4; ++channels)
    for (auto shape : {std::pair<std::size_t,std::size_t> {1,1}, {1,9}, {9,1}, {2,2}, {3,3},
                       {7,13}, {67,65}, {1023,3}, {1024,5}, {1025,4}, {2063,9}})
      {
        const auto width = shape.first, height = shape.second;
        std::vector<FilterSelectionRegion> regions {full_region (width, height),
          {true, true, std::int32_t (width / 2), std::int32_t (height / 2),
                       std::int32_t (width / 2 + 1), std::int32_t (height / 2 + 1)}};
        if (width > 2 && height > 2)
          regions.push_back ({true, true, 1, 1, std::int32_t (width - 1), std::int32_t (height - 1)});
        if (width == 2063)
          regions.push_back ({true, true, 7, 2, 2056, 7}); // ROI crosses two fixed strip boundaries.
        for (unsigned pattern = 0; pattern < 2; ++pattern)
          {
            const auto native = fixture (width, height, channels, pattern), input = expand (native, channels);
            for (const auto& region : regions)
              for (const auto& options : flags)
                {
                  const auto expected = expected_shadow (native, width, height, channels, region, options);
                  const auto result = vector_result (input, width, height, region, options);
                  const auto spill = spill_result (input, width, height, region, options);
                  const auto label = " channels=" + std::to_string (channels) + " width=" + std::to_string (width) +
                    " height=" + std::to_string (height) + " flags=" + std::to_string (options.horizontal) + "," +
                    std::to_string (options.vertical) + "," + std::to_string (options.keep_sign) +
                    " ROI=" + std::to_string (region.x1) + "," + std::to_string (region.y1) + "," +
                    std::to_string (region.x2) + "," + std::to_string (region.y2);
                  require (result == expected, "Vector/source-C mismatch" + label);
                  require (spill == expected, "Spill/source-C mismatch" + label);
                  raw_contract (result, width, height, region);
                  if (channels % 2 != 0)
                    require (expected == expected_shadow (input, width, height, 4, region, options),
                             "Native no-alpha colors and promoted RGBA source oracle disagree");
                  ++cases;
                }
          }
      }
  std::cout << "Source-C Gray/Gray-alpha/RGB/RGBA transport, flags, ROI, tiny/tile/strip boundaries, vector/spill: "
            << cases << " cases passed\n";
}

void arithmetic_and_halos ()
{
  const auto region = full_region (3, 3);
  unsigned negative_checks = 0;
  // Bottom-left alone gives horizontal gradient -value at the center. This
  // sweeps every representable byte around the asymmetric negative ROUND.
  for (unsigned value = 0; value < 256; ++value)
    {
      Bytes native (9, 0); native[6] = std::uint8_t (value);
      const auto input = expand (native, 1);
      for (const SobelOptions options : {SobelOptions {1,0,1}, {1,0,0}})
        {
          const auto output = vector_result (input, 3, 3, region, options);
          require (output == expected_shadow (native, 3, 3, 1, region, options),
                   "One-direction negative/absolute rounding differs from unchanged C");
          ++negative_checks;
        }
    }
  Bytes rounding (9, 0); rounding[6] = 5;
  require (vector_result (expand (rounding, 1), 3, 3, region, {1,0,1})[16] == 127,
           "Negative ROUND was replaced by floor or std::round");
  rounding[6] = 12;
  require (vector_result (expand (rounding, 1), 3, 3, region, {1,0,1})[16] == 126,
           "Negative half-step ROUND changed");
  rounding[6] = 2;
  require (vector_result (expand (rounding, 1), 3, 3, region, {1,0,0})[16] == 1,
           "Absolute half-step did not round upward");
  Bytes rms (9, 0); rms[0] = 3; rms[1] = 4;
  const auto rms_result = vector_result (expand (rms, 1), 3, 3, region, {1,1,0});
  require (rms_result[16] == 1 && rms_result == expected_shadow (rms, 3, 3, 1, region, {1,1,0}),
           "RMS must be rounded before division by 5.66 and truncation");

  for (unsigned value : {40U, 42U})
    {
      Bytes alpha_fixture (9, 0); alpha_fixture[0] = std::uint8_t (value);
      auto input = expand (alpha_fixture, 1);
      for (std::size_t i = 0; i < 9; ++i) input[i * 4 + 3] = 0;
      const auto output = vector_result (input, 3, 3, region, {1,0,0});
      require (output[16] == (value == 40 ? 10 : 11) && output[19] == (value == 40 ? 0 : 255),
               "Strict alpha gradient >10 threshold or hidden RGB changed");
    }
  auto transparent = fixture (7, 5, 4), opaque = transparent;
  for (std::size_t i = 0; i < 35; ++i) { transparent[i * 4 + 3] = 0; opaque[i * 4 + 3] = 255; }
  for (const SobelOptions options : {SobelOptions {1,1,1}, {1,0,1}, {0,1,0}, {0,0,0}, {0,0,-1}})
    require (vector_result (transparent, 7, 5, full_region (7, 5), options) ==
             vector_result (opaque, 7, 5, full_region (7, 5), options),
             "Input alpha influenced raw Sobel color or synthesized alpha");
  for (unsigned value = 0; value < 256; ++value)
    {
      const Bytes input (4, std::uint8_t (value));
      require (vector_result (input, 1, 1, full_region (1, 1), {1,1,0}) == Bytes ({0,0,0,0}),
               "Uniform 1x1 dual-axis field was not zero");
      require (vector_result (input, 1, 1, full_region (1, 1), {0,0,-7}) == Bytes ({127,127,127,255}),
               "Both-disabled signed field changed");
      require (vector_result (input, 1, 1, full_region (1, 1), {0,0,0}) == Bytes ({0,0,0,0}),
               "Both-disabled absolute field changed");
    }

  const FilterSelectionRegion roi {true, true, 1, 2, 4, 3};
  Bytes vertical (25, 0);
  for (std::size_t x = 1; x < 4; ++x) vertical[5 + x] = 100;
  const auto vertical_result = vector_result (expand (vertical, 1), 5, 5, roi, {1,0,0});
  for (std::size_t x = 1; x < 4; ++x)
    require (vertical_result[(2 * 5 + x) * 4] == 100,
             "Vertical source halo was clamped to ROI rather than complete drawable");
  require (vertical_result == expected_shadow (vertical, 5, 5, 1, roi, {1,0,0}),
           "Full-image vertical halo source-C mismatch");
  Bytes horizontal (25, 0);
  for (std::size_t y = 0; y < 5; ++y) { horizontal[y * 5] = 255; horizontal[y * 5 + 4] = 255; }
  require (vector_result (expand (horizontal, 1), 5, 5, roi, {0,1,0}) == Bytes (100, 0),
           "Horizontal halo used columns outside captured ROI");

  const auto input = fixture (11, 7, 4);
  require (vector_result (input, 11, 7, full_region (11, 7), {-7,0,int_maximum}) ==
           vector_result (input, 11, 7, full_region (11, 7), {1,0,1}), "Noncanonical flags were not truthy");
  require (vector_result (input, 11, 7, full_region (11, 7), {1,1,0}) ==
           vector_result (input, 11, 7, full_region (11, 7), {1,1,-7}), "Dual-axis mode used keep_sign");
  require (vector_result (input, 11, 7, full_region (11, 7, true), {}) ==
           vector_result (input, 11, 7, full_region (11, 7), {}), "Full selected ROI changed arithmetic");
  const FilterSelectionRegion outside {true, false, 11, 0, 11, 7};
  const auto outside_oracle = expected_shadow (input, 11, 7, 4, outside, {});
  require (outside_oracle == Bytes (input.size (), 0) &&
           vector_result (input, 11, 7, outside, {}) == outside_oracle &&
           spill_result (input, 11, 7, outside, {}) == outside_oracle,
           "Outside selection did not take source-C early return and leave no-merge workspace");
  std::cout << "Negative/absolute ROUND: " << negative_checks
            << "; RMS order, alpha threshold, hidden colors, flags, mixed halo and no-merge checks passed\n";
}

class Sized final : public FilterRaster
{
  std::uint64_t size_;
public:
  std::size_t calls = 0;
  explicit Sized (std::uint64_t size) : size_ (size) {}
  std::uint64_t size () const noexcept override { return size_; }
  void read (std::uint64_t, std::size_t, std::uint8_t *) override { ++calls; throw std::logic_error ("Unexpected read"); }
  void write (std::uint64_t, std::size_t, const std::uint8_t *) override { ++calls; throw std::logic_error ("Unexpected write"); }
  void flush () override { ++calls; throw std::logic_error ("Unexpected flush"); }
};
void validation ()
{
  std::atomic<bool> cancel {false}; Sized input (4), output (4), short_raster (3), long_raster (5);
  const auto full = full_region (1, 1);
  for (auto shape : {std::pair<std::size_t,std::size_t> {0,1}, {1,0},
                    {std::size_t (int_maximum) + 1,1}, {1,std::size_t (int_maximum) + 1},
                    {std::size_t (int_maximum),std::size_t (int_maximum)},
                    {std::numeric_limits<std::size_t>::max (),1}, {1,std::numeric_limits<std::size_t>::max ()}})
    rejects<std::invalid_argument> ([&] { filter_sobel_shadow_raster (input, shape.first, shape.second,
                                                                    full, {}, cancel, output); },
                                    "Unsupported full Sobel geometry accepted");
  for (const FilterSelectionRegion region : {
    FilterSelectionRegion {true,true,-1,0,1,1}, {true,true,0,-1,1,1}, {true,true,1,0,0,1},
    {true,true,0,1,1,0}, {true,true,0,0,2,1}, {true,true,0,0,1,2}, {true,false,0,0,1,1},
    {true,true,0,0,0,1}, {false,false,0,0,0,0}, {false,true,1,0,1,1}})
    rejects<std::invalid_argument> ([&] { filter_sobel_shadow_raster (input, 1, 1, region, {}, cancel, output); },
                                    "Inconsistent captured ROI accepted");
  rejects<std::invalid_argument> ([&] { filter_sobel_shadow_raster (input, 1, 1, full, {}, cancel, input); },
                                  "Direct raster alias accepted");
  for (auto *wrong : {&short_raster, &long_raster})
    {
      rejects<std::invalid_argument> ([&] { filter_sobel_shadow_raster (*wrong, 1, 1, full, {}, cancel, output); },
                                      "Wrong input extent accepted");
      rejects<std::invalid_argument> ([&] { filter_sobel_shadow_raster (input, 1, 1, full, {}, cancel, *wrong); },
                                      "Wrong output extent accepted");
    }
  require (input.calls == 0 && output.calls == 0 && short_raster.calls == 0 && long_raster.calls == 0,
           "Invalid input reached raster I/O");
  constexpr auto max_roi = std::size_t (int_maximum / 4 - 2);
  rejects<std::invalid_argument> ([&] {
    filter_sobel_shadow_validate (max_roi + 1, 1, full_region (max_roi + 1, 1), {});
  }, "Legacy signed row allocation overflow accepted");
  for (auto shape : {std::pair<std::size_t,std::size_t> {max_roi,1}, {1025,1048577},
                    {std::size_t (int_maximum),1}, {1,std::size_t (int_maximum)}})
    {
      const auto width = shape.first, height = shape.second;
      auto region = full_region (width, height);
      if (width > max_roi) region = {true,true,int_maximum - 1,0,int_maximum,1};
      filter_sobel_shadow_validate (width, height, region,
                                    {std::numeric_limits<std::int32_t>::min (),int_maximum,-7});
      // Checked kernel transport only, not old native drawable/tile admission
      // or actual large/high-offset I/O.
      const auto size = std::uint64_t (width) * height * 4;
      Sized large_input (size), large_output (size); cancel.store (true);
      require (filter_sobel_shadow_raster (large_input, width, height, region, {}, cancel, large_output) ==
               Outcome::cancelled, "Pre-cancelled valid wide/64-bit extent was not admitted");
      require (!large_input.calls && !large_output.calls, "Pre-cancelled large extent accessed storage");
    }
  rejects<std::invalid_argument> ([&] {
    filter_sobel_shadow_validate (3, 3, {false,true,1,1,2,2}, {});
  }, "Unselected partial region accepted");
  // Empty intersections return before the original row allocation expression.
  filter_sobel_shadow_validate (int_maximum, 1, {true,false,0,0,int_maximum,0}, {});
  // Oracle guards run before touching pointed-to storage. Native no-alpha
  // counter has an additional conservative area bound absent in RGBA.
  std::uint8_t dummy_in = 0, dummy_out = 0;
  require (!legacy_sobel_oracle (&dummy_in, int_maximum, 1, 4, 0, 0, int_maximum, 1, 1, 1, 1, &dummy_out),
           "Oracle admitted overflowing legacy row allocation");
  require (!legacy_sobel_oracle (&dummy_in, 50000, 50000, 1, 0, 0, 50000, 50000, 1, 1, 1, &dummy_out),
           "Oracle admitted potentially overflowing no-alpha counter");
  require (!legacy_sobel_oracle (&dummy_in, 1, 1, 4, 1, 0, 1, 1, 1, 1, 1, &dummy_out),
           "Oracle accepted out-of-bounds ROI");
  require (!legacy_sobel_oracle (&dummy_in, 1, 1, 4, 0, 0, 1, 1, 1, 1, 1, &dummy_in),
           "Oracle accepted direct buffer alias");
  std::cout << "Tiny-defined versus signed-overflow geometry, exact storage, aliases, flags, pre-cancelled 64-bit extents passed\n";
}

void cancellation_errors_and_progress ()
{
  constexpr std::size_t width = 2063, height = 9;
  const FilterSelectionRegion region {true,true,7,2,2056,7};
  const SobelOptions options {-7,0,-1};
  const auto input = fixture (width, height, 4);
  const Bytes sentinel {17,29,43};
  std::atomic<bool> baseline_cancel {false}; auto baseline_progress = std::make_shared<FilterProgress> ();
  Audit baseline (width, height, region, baseline_cancel, baseline_progress); Bytes expected;
  require (audited_vector (input, options, expected, baseline) == Outcome::shadow, "Baseline was cancelled");
  baseline.observe ();
  require (baseline.last_progress == 1 && baseline.zero_updates > 0 && baseline.crop_updates > 2 &&
           baseline.flushes == 1 && baseline.max_read <= 4104 && baseline.max_write <= 16384,
           "Progress or bounded I/O contract failed");
  require (baseline.reads == 45 && baseline.zero_writes == 5 && baseline.writes == 20,
           "Unexpected bounded-strip I/O count");
  require (expected == expected_shadow (input, width, height, 4, region, options), "Progress changed output");

  unsigned checked = 0;
  for (bool spill : {false,true})
    for (auto operation : {Operation::read, Operation::write, Operation::flush})
      for (bool fail : {false,true})
        {
          const auto last = operation == Operation::read ? baseline.reads :
                            operation == Operation::write ? baseline.writes : baseline.flushes;
          for (std::size_t at = 1; at <= last; ++at)
            {
              std::atomic<bool> cancel {false}; auto progress = std::make_shared<FilterProgress> ();
              Audit audit (width, height, region, cancel, progress); Bytes result = sentinel;
              if (fail) { audit.fail_on = operation; audit.fail_at = at; }
              else { audit.cancel_on = operation; audit.cancel_at = at; }
              if (spill)
                {
                  TemporaryFilterRaster in (directory, input.size ()), out (directory, input.size ());
                  in.write (0, input.size (), input.data ());
                  Observed source (in, audit, true), destination (out, audit, false);
                  const auto execute = [&] {
                    return filter_sobel_shadow_raster (source, width, height, region, options, cancel, destination, progress);
                  };
                  if (fail) rejects<StorageFailure> (execute, "Spill storage failure did not propagate");
                  else require (execute () == Outcome::cancelled && cancel.load (), "Spill cancellation completed output");
                  Bytes unchanged (input.size ()); in.read (0, unchanged.size (), unchanged.data ());
                  require (unchanged == input, "Failed/cancelled spill changed source");
                }
              else
                {
                  const auto execute = [&] { return audited_vector (input, options, result, audit); };
                  if (fail) rejects<StorageFailure> (execute, "Vector storage failure did not propagate");
                  else require (execute () == Outcome::cancelled && cancel.load (), "Vector cancellation completed output");
                  require (result == sentinel, "Failed/cancelled vector published private output");
                  require (input == fixture (width, height, 4), "Failed/cancelled vector changed source");
                }
              audit.observe ();
              require (audit.last_progress < 1, "Failed/cancelled execution reported complete");
              const auto observed = operation == Operation::read ? audit.reads :
                                    operation == Operation::write ? audit.writes : audit.flushes;
              require (observed == at, "I/O continued after selected cancellation/failure");
              ++checked;
            }
        }
  std::atomic<bool> cancel {true}; auto progress = std::make_shared<FilterProgress> ();
  Audit audit (width, height, region, cancel, progress); Bytes result = sentinel;
  require (audited_vector (input, options, result, audit) == Outcome::cancelled && result == sentinel,
           "Pre-cancelled vector changed output");
  Memory in (input), out (input.size ()); const auto poison = out.data;
  Observed source (in, audit, true), destination (out, audit, false);
  require (filter_sobel_shadow_raster (source, width, height, region, options, cancel, destination, progress) ==
           Outcome::cancelled && in.data == input && out.data == poison,
           "Pre-cancelled direct raster accessed storage");
  require (!audit.reads && !audit.writes && !audit.flushes, "Pre-cancel performed I/O");
  require (filter_sobel_shadow_vector (input, width, height, region, options, cancel, result) == Outcome::cancelled &&
           result == sentinel, "Public vector cancellation published output");
  cancel.store (false); Bytes alias = input;
  require (filter_sobel_shadow_vector (alias, width, height, region, options, cancel, alias) == Outcome::shadow &&
           alias == expected, "Public aliased vector failed transactional publication");
  alias = input; Audit alias_audit (width, height, region, cancel);
  alias_audit.cancel_on = Operation::write; alias_audit.cancel_at = baseline.writes;
  require (audited_vector (alias, options, alias, alias_audit) == Outcome::cancelled && alias == input,
           "Cancelled aliased vector published private output");
  cancel.store (false); result = sentinel;
  rejects<std::invalid_argument> ([&] {
    filter_sobel_shadow_vector (input, width, height, {true,true,-1,0,1,1}, options, cancel, result);
  }, "Public vector accepted invalid geometry");
  require (result == sentinel, "Public vector validation error changed output");

  const FilterSelectionRegion outside {true,false,0,0,0,0};
  for (auto operation : {Operation::write, Operation::flush})
    for (bool fail : {false,true})
      {
        cancel.store (false); result = sentinel;
        auto no_progress = std::make_shared<FilterProgress> ();
        Audit no_merge (width, height, outside, cancel, no_progress);
        if (fail) { no_merge.fail_on = operation; no_merge.fail_at = 1; }
        else { no_merge.cancel_on = operation; no_merge.cancel_at = 1; }
        const auto execute = [&] { return audited_vector (input, options, result, no_merge); };
        if (fail) rejects<StorageFailure> (execute, "No-merge failure did not propagate");
        else require (execute () == Outcome::cancelled, "No-merge cancellation completed");
        no_merge.observe ();
        require (result == sentinel && no_merge.reads == 0 && no_merge.last_progress < 1,
                 "Failed/cancelled no-merge work published output or read source");
      }
  std::cout << "Every read/write/flush cancellation and failure ordinal: " << checked
            << " vector/spill cases; no-merge, source immutability, transactional aliases and progress passed\n";
}
} // namespace

int main ()
{
  GError *error = nullptr;
  gchar *path = g_dir_make_tmp ("painter-sobel-XXXXXX", &error);
  if (!path)
    {
      std::cerr << "Cannot create private spill directory: " << (error ? error->message : "unknown error") << '\n';
      g_clear_error (&error); return 1;
    }
  directory = path; g_free (path);
  int status = 0;
  try
    {
      source_oracle_parity ();
      arithmetic_and_halos ();
      validation ();
      cancellation_errors_and_progress ();
    }
  catch (const std::exception& failure) { std::cerr << failure.what () << '\n'; status = 1; }
  if (g_rmdir (directory.c_str ()) != 0)
    { std::cerr << "Private spill directory was not empty or could not be removed\n"; status = 1; }
  return status;
}
