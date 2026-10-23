/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "filter-edge-shadow.hpp"
#include "filter-native-kernels.hpp"
#include "filter-progress.hpp"
#include "legacy-edge-shadow-oracle.h"
#include <glib.h>
#include <glib/gstdio.h>
#include <algorithm>
#include <array>
#include <iostream>
#include <limits>
#include <cmath>
#include <cstring>
#include <fstream>
#include <sstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace GimpPainter;
using Bytes = std::vector<std::uint8_t>;
using Outcome = FilterEdgeShadowOutcome;
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

/* Transport only: independent native-format literal C does all arithmetic.
 * Source alpha is copied; no-alpha sources are independently promoted to 255. */
Bytes expected_shadow (const Bytes& native, std::size_t width, std::size_t height,
                       unsigned channels, const FilterSelectionRegion& region,
                       const EdgeOptions& options)
{
  Bytes result (width * height * 4, 0), calculated (native.size (), 0xa7);
  const auto w = std::size_t (region.x2 - region.x1), h = std::size_t (region.y2 - region.y1);
  require (legacy_edge_shadow_oracle (native.data (), width, height, channels,
          region.x1, region.y1, w, h, options.edgemode, options.wrapmode,
          options.amount, calculated.data ()) == 1, "Source-C oracle rejected supported case");
  unsigned flushes = 0, merges = 0, updates = 0;
  legacy_edge_shadow_oracle_get_counters (&flushes, &merges, &updates);
  require (flushes == 1 && merges == 1 && updates == 1,
           "Literal Edge body omitted final hooks, including empty ROI");
  for (std::size_t y = 0; y < height; ++y)
    for (std::size_t x = 0; x < width; ++x)
      {
        const auto pixel = y * width + x;
        const bool inside = region.intersects && x >= std::size_t (region.x1) &&
          x < std::size_t (region.x2) && y >= std::size_t (region.y1) && y < std::size_t (region.y2);
        if (!inside)
          {
            for (unsigned c = 0; c < channels; ++c)
              require (calculated[pixel * channels + c] == 0xa7, "Oracle wrote outside ROI");
            continue;
          }
        result[pixel * 4] = calculated[pixel * channels];
        result[pixel * 4 + 1] = calculated[pixel * channels + (channels <= 2 ? 0 : 1)];
        result[pixel * 4 + 2] = calculated[pixel * channels + (channels <= 2 ? 0 : 2)];
        result[pixel * 4 + 3] = channels % 2 == 0 ? calculated[pixel * channels + channels - 1] : 255;
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
  int wrapmode = 2;
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
        require (full_x + count / 4 <= width && full_y < height,
                 "Edge halo crossed full row or drawable extent");
        if (operation == Operation::read)
          {
            ordinal = ++reads; max_read = std::max (max_read, count);
            require (count <= 4104, "Unbounded halo read");
            const auto center_x = std::size_t (region.x1) + x;
            const auto n = std::min (std::size_t (1024), crop_width - x);
            const auto first = center_x ? center_x - 1 : 0;
            const auto end = std::min (width, center_x + n + 1);
            require ((full_x == first && count == (end - first) * 4) ||
                     (wrapmode == 1 && count == 4 &&
                       ((!center_x && full_x == width - 1) ||
                        (center_x + n == width && !full_x))), "Wrong horizontal full-drawable halo");
            const auto center_y = std::size_t (region.y1) + y;
            require (full_y == center_y || (center_y && full_y == center_y - 1) ||
                     (center_y + 1 < height && full_y == center_y + 1) ||
                     (wrapmode == 1 && ((!center_y && full_y == height - 1) ||
                       (center_y + 1 == height && !full_y))), "Wrong vertical full-drawable halo");
          }
        else
          {
            ordinal = ++writes; max_write = std::max (max_write, count);
            require (full_x == std::size_t (region.x1) + x && full_y == std::size_t (region.y1) + y &&
                     count == std::min (std::size_t (1024), crop_width - x) * 4,
                     "Crop writes are not exact bounded logical strips");
            crop_written += count;
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

Outcome audited_vector (const Bytes& input, const EdgeOptions& options, Bytes& output, Audit& audit)
{
  audit.wrapmode = options.wrapmode;
  auto outcome = Outcome::cancelled;
  const auto complete = filter_native_vector (input, audit.cancel, output,
    [&] (FilterRaster& in, FilterRaster& out, std::atomic<bool>& cancel, const FilterRasterFactory&) {
      Observed source (in, audit, true), destination (out, audit, false);
      outcome = filter_edge_shadow_raster (source, audit.width, audit.height, audit.region,
                                            options, cancel, destination, audit.progress);
      return outcome != Outcome::cancelled;
    });
  return complete ? outcome : Outcome::cancelled;
}
Bytes spill_result (const Bytes& input, std::size_t width, std::size_t height,
                    const FilterSelectionRegion& region, const EdgeOptions& options)
{
  TemporaryFilterRaster in (directory, input.size ()), out (directory, input.size ());
  in.write (0, input.size (), input.data ());
  // Poison actual disk storage too: outside-ROI zeros must be written explicitly.
  const Bytes poison (input.size (), 0xa7); out.write (0, poison.size (), poison.data ());
  std::atomic<bool> cancel {false}; auto progress = std::make_shared<FilterProgress> ();
  Audit audit (width, height, region, cancel, progress);
  audit.wrapmode = options.wrapmode;
  Observed source (in, audit, true), destination (out, audit, false);
  const auto outcome = filter_edge_shadow_raster (source, width, height, region, options,
                                                  cancel, destination, progress);
  require (outcome == (region.intersects ? Outcome::writes : Outcome::no_writes), "Wrong spill outcome");
  audit.observe ();
  require (audit.last_progress == 1 && audit.flushes == 1, "Spill output did not complete after flush");
  Bytes result (input.size ()), unchanged (input.size ());
  out.read (0, result.size (), result.data ()); in.read (0, unchanged.size (), unchanged.data ());
  require (unchanged == input, "Spill changed source input");
  return result;
}

Bytes vector_result (const Bytes& input, std::size_t width, std::size_t height,
                     const FilterSelectionRegion& region, const EdgeOptions& options)
{
  std::atomic<bool> cancel {false}; Bytes result;
  const auto outcome = filter_edge_shadow_vector (input, width, height, region, options, cancel, result);
  require (outcome == (region.intersects ? Outcome::writes : Outcome::no_writes), "Wrong vector outcome");
  return result;
}
void raw_contract (const Bytes& input, const Bytes& output, std::size_t width, std::size_t height,
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
          require (output[at + 3] == input[at + 3], "Raw Edge changed source alpha");
      }
}
void source_oracle_parity ()
{
  unsigned cases = 0;
  for (unsigned channels = 1; channels <= 4; ++channels)
    for (auto shape : {std::pair<std::size_t,std::size_t> {1,1}, {1,9}, {9,1}, {2,2}, {3,3},
                       {7,13}, {63,3}, {64,5}, {65,4}, {67,65}, {1023,3}, {1024,5}, {1025,4}, {2063,9}})
      {
        const auto width = shape.first, height = shape.second;
        std::vector<FilterSelectionRegion> regions {full_region (width, height),
          {true, true, std::int32_t (width / 2), std::int32_t (height / 2),
                       std::int32_t (width / 2 + 1), std::int32_t (height / 2 + 1)},
          {true, false, std::int32_t (width), 0, std::int32_t (width), std::int32_t (height)},
          {true, false, std::int32_t (width), std::int32_t (height),
                        std::int32_t (width), std::int32_t (height)}};
        if (width > 2 && height > 2)
          {
            regions.push_back ({true, true, 1, 1, std::int32_t (width - 1), std::int32_t (height - 1)});
            // Each partial ROI touches exactly one drawable edge. These mix
            // drawable-border handling with full-image halos beyond ROI edges.
            regions.push_back ({true, true, 0, 1, std::int32_t (width - 1), std::int32_t (height - 1)});
            regions.push_back ({true, true, 1, 1, std::int32_t (width), std::int32_t (height - 1)});
            regions.push_back ({true, true, 1, 0, std::int32_t (width - 1), std::int32_t (height - 1)});
            regions.push_back ({true, true, 1, 1, std::int32_t (width - 1), std::int32_t (height)});
          }
        if (width == 2063) regions.push_back ({true, true, 7, 2, 2056, 7});
        for (unsigned pattern = 0; pattern < 2; ++pattern)
          {
            const auto native = fixture (width, height, channels, pattern), input = expand (native, channels);
            for (const auto& region : regions)
              for (int mode = 0; mode <= 5; ++mode)
                for (int wrap = 1; wrap <= 3; ++wrap)
                  {
                    const EdgeOptions options {pattern ? -7.0 : 1.75, wrap, mode};
                    const auto expected = expected_shadow (native, width, height, channels, region, options);
                    const auto result = vector_result (input, width, height, region, options);
                    const auto spill = spill_result (input, width, height, region, options);
                    const auto label = " channels=" + std::to_string (channels) + " width=" + std::to_string (width) +
                      " height=" + std::to_string (height) + " mode=" + std::to_string (mode) + " wrap=" + std::to_string (wrap);
                    require (result == expected, "Vector/source-C mismatch" + label);
                    require (spill == expected, "Spill/source-C mismatch" + label);
                    raw_contract (input, result, width, height, region);
                    if (channels % 2 != 0)
                      require (expected == expected_shadow (input, width, height, 4, region, options),
                               "Native no-alpha colors and promoted RGBA source oracle disagree");
                    ++cases;
                  }
          }
      }
  std::cout << "Source-C Gray/Gray-alpha/RGB/RGBA, all modes/borders, ROI and tile/strip vector/spill: "
            << cases << " cases passed\n";
}

/* Independent full-coverage REPLACE_INTEN specialization: source and shadow
 * alpha are equal. Original paint-funcs.c computes new_alpha=a; when a>0 its
 * source2 ratio is one, otherwise original hidden RGB is retained. No production
 * detector or merge helper supplies expectations. This is full coverage only. */
Bytes merge_full_coverage (const Bytes& input, const Bytes& raw)
{
  require (raw.size () == input.size (), "Bad independent merge extent");
  auto result = raw;
  for (std::size_t at = 0; at < input.size (); at += 4)
    {
      require (input[at + 3] == raw[at + 3], "Independent merge requires equal alpha");
      if (!raw[at + 3]) std::copy_n (input.data () + at, 3, result.data () + at);
    }
  return result;
}
void arithmetic_and_halos ()
{
  const double limit = double (int_maximum) / 2040.0;
  unsigned checked = 0;
  for (double amount : {-std::numeric_limits<double>::max (), -1.0, -0.0, 0.5, 1.0, 1.0001, 1.75, 2.5, limit})
    for (int mode = 0; mode <= 5; ++mode)
      for (int wrap = 1; wrap <= 3; ++wrap)
        {
          const auto native = fixture (5, 7, 4); const auto roi = full_region (5, 7);
          const EdgeOptions options {amount,wrap,mode};
          require (vector_result (native,5,7,roi,options) == expected_shadow (native,5,7,4,roi,options),
                   "Clamped/fractional/bounded-max amount differs from literal C");
          ++checked;
        }
  for (unsigned alpha = 0; alpha < 256; ++alpha)
    {
      auto input = fixture (3,3,4);
      for (std::size_t at = 3; at < input.size (); at += 4) input[at] = std::uint8_t (alpha);
      for (int mode = 0; mode <= 5; ++mode)
        {
          const EdgeOptions options {2.5,3,mode};
          const auto output = vector_result (input,3,3,full_region (3,3),options);
          require (output == expected_shadow (input,3,3,4,full_region (3,3),options), "All-alpha oracle mismatch");
          raw_contract (input,output,3,3,full_region (3,3));
        }
    }
  // All six detectors have an explicitly bounded C conversion. Check both
  // extrema and amount just outside the oracle domain without invoking old UB.
  std::uint8_t kernel[9] = {}, byte = 0;
  for (int mode = 0; mode <= 5; ++mode)
    require (!legacy_edge_shadow_detect (kernel,mode,std::nextafter (limit,std::numeric_limits<double>::infinity ()),&byte),
             "Oracle accepted amount beyond its defined conservative domain");
  Bytes laplace (9,255); laplace[4] = 0;
  auto input = expand (laplace,1);
  require (vector_result (input,3,3,full_region (3,3),{limit,2,5}) ==
           expected_shadow (laplace,3,3,1,full_region (3,3),{limit,2,5}), "Positive Laplace bound differs from literal C");
  require (vector_result (input,3,3,full_region (3,3),{limit,2,5})[16] == 255, "Laplace positive saturation changed");
  laplace.assign (9,0); laplace[4] = 255; input = expand (laplace,1);
  require (vector_result (input,3,3,full_region (3,3),{limit,2,5}) ==
           expected_shadow (laplace,3,3,1,full_region (3,3),{limit,2,5}), "Negative Laplace bound differs from literal C");
  require (vector_result (input,3,3,full_region (3,3),{limit,2,5})[16] == 0, "Laplace negative saturation changed");
  // Safe finite-amount extension remains separate from literal-C parity.
  for (int mode = 0; mode <= 5; ++mode)
    {
      const EdgeOptions options {std::numeric_limits<double>::max (),2,mode};
      const auto original = fixture (5,3,4), raw = vector_result (original,5,3,full_region (5,3),options);
      Bytes merged; std::atomic<bool> cancel {false};
      require (filter_edge (original,5,3,options,cancel,merged) && merged == merge_full_coverage (original,raw),
               "Safe huge finite amount extension differs from existing Edge");
    }
  // Interior ROI must use surrounding full-drawable pixels on BOTH axes.
  const FilterSelectionRegion roi {true,true,2,2,3,3};
  Bytes neighborhood (25,0); neighborhood[2*5+1] = 100;
  const auto horizontal = vector_result (expand (neighborhood,1),5,5,roi,{1,2,0});
  require (horizontal[(2*5+2)*4] == 200, "Horizontal halo incorrectly replicated ROI edge");
  neighborhood.assign (25,0); neighborhood[1*5+2] = 100;
  require (vector_result (expand (neighborhood,1),5,5,roi,{1,2,0})[(2*5+2)*4] == 200,
           "Vertical halo incorrectly replicated ROI edge");
  auto hidden = fixture (5,5,4);
  for (std::size_t at = 3; at < hidden.size (); at += 4) hidden[at] = 0;
  const auto raw = vector_result (hidden,5,5,full_region (5,5),{});
  require (raw != hidden, "Raw Edge preserved hidden RGB instead of transforming it");
  Bytes merged; std::atomic<bool> cancel {false};
  require (filter_edge (hidden,5,5,{},cancel,merged) && merged == hidden &&
           merge_full_coverage (hidden,raw) == merged, "Existing merged hidden RGB contract changed");
  Memory source (hidden), destination (hidden.size ());
  require (filter_edge_raster (source,5,5,{},cancel,destination) && destination.data == hidden,
           "Existing raster hidden RGB contract changed");
  std::vector<double> real (25*4);
  for (std::size_t i=0;i<hidden.size ();++i) real[i] = hidden[i]/255.0;
  Bytes real_bytes (real.size ()*sizeof (double)); std::memcpy (real_bytes.data (),real.data (),real_bytes.size ());
  Memory real_in (real_bytes), real_out (real_bytes.size ());
  require (filter_edge_real_raster (real_in,real_out,5,5,{},cancel) && real_out.data == real_bytes,
           "Existing normalized-real hidden RGB contract changed");
  std::cout << "Amounts: " << checked << "; all alpha bytes, Laplace signs, hidden RGB, full halos and existing APIs passed\n";
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
    rejects<std::invalid_argument> ([&] { filter_edge_shadow_raster (input, shape.first, shape.second,
                                                                    full, {}, cancel, output); },
                                    "Unsupported full Edge geometry accepted");
  for (const FilterSelectionRegion region : {
    FilterSelectionRegion {true,true,-1,0,1,1}, {true,true,0,-1,1,1}, {true,true,1,0,0,1},
    {true,true,0,1,1,0}, {true,true,0,0,2,1}, {true,true,0,0,1,2}, {true,false,0,0,1,1},
    {true,true,0,0,0,1}, {false,false,0,0,0,0}, {false,true,1,0,1,1}})
    rejects<std::invalid_argument> ([&] { filter_edge_shadow_raster (input, 1, 1, region, {}, cancel, output); },
                                    "Inconsistent captured ROI accepted");
  rejects<std::invalid_argument> ([&] { filter_edge_shadow_raster (input, 1, 1, full, {}, cancel, input); },
                                  "Direct raster alias accepted");
  for (auto *wrong : {&short_raster, &long_raster})
    {
      rejects<std::invalid_argument> ([&] { filter_edge_shadow_raster (*wrong, 1, 1, full, {}, cancel, output); },
                                      "Wrong input extent accepted");
      rejects<std::invalid_argument> ([&] { filter_edge_shadow_raster (input, 1, 1, full, {}, cancel, *wrong); },
                                      "Wrong output extent accepted");
    }
  require (input.calls == 0 && output.calls == 0 && short_raster.calls == 0 && long_raster.calls == 0,
           "Invalid input reached raster I/O");
  filter_edge_shadow_validate (int_maximum,1,full_region (int_maximum,1),{});
  filter_edge_shadow_validate (46340,46340,full_region (46340,46340),{});
  rejects<std::invalid_argument> ([&] {
    filter_edge_shadow_validate (46341,46341,full_region (46341,46341),{});
  }, "Legacy signed ROI area overflow accepted");
  for (const EdgeOptions options : {EdgeOptions {1,0,0}, {1,4,0}, {1,1,-1}, {1,1,6},
       {std::numeric_limits<double>::infinity (),1,0}, {std::numeric_limits<double>::quiet_NaN (),1,0}})
    rejects<std::invalid_argument> ([&] { filter_edge_shadow_raster (input,1,1,full,options,cancel,output); },
                                    "Invalid options reached storage");
  for (auto shape : {std::pair<std::size_t,std::size_t> {std::size_t (int_maximum),1},
                    {1025,1048577}, {1,std::size_t (int_maximum)}, {100000,100000}})
    {
      const auto width = shape.first, height = shape.second;
      const FilterSelectionRegion region {true,true,1,0,2,1};
      auto valid = width > 1 ? region : FilterSelectionRegion {true,true,0,0,1,1};
      filter_edge_shadow_validate (width,height,valid,{});
      const auto size = std::uint64_t (width) * height * 4;
      Sized large_input (size), large_output (size); cancel.store (true);
      require (filter_edge_shadow_raster (large_input,width,height,valid,{},cancel,large_output) == Outcome::cancelled,
               "Pre-cancelled large declared extent failed");
      require (!large_input.calls && !large_output.calls, "Pre-cancelled large extent accessed storage");
    }

  rejects<std::invalid_argument> ([&] {
    filter_edge_shadow_validate (3, 3, {false,true,1,1,2,2}, {});
  }, "Unselected partial region accepted");
  filter_edge_shadow_validate (int_maximum,1,{true,false,0,0,int_maximum,0},{});
  std::uint8_t dummy_in = 0, dummy_out = 0;
  require (!legacy_edge_shadow_oracle (&dummy_in,50000,50000,4,0,0,50000,50000,0,2,1,&dummy_out),
           "Oracle admitted signed ROI area overflow");
  require (!legacy_edge_shadow_oracle (&dummy_in,1,1,4,1,0,1,1,0,2,1,&dummy_out),
           "Oracle accepted out-of-bounds ROI");
  require (!legacy_edge_shadow_oracle (&dummy_in,1,1,4,0,0,1,1,0,2,1,&dummy_in),
           "Oracle accepted direct alias");
  require (!input.calls && !output.calls, "Invalid options reached storage");
  std::cout << "Pre-access geometry, area/extent/options rejection and declared large pre-cancellation passed\n";
}

void cancellation_errors_and_progress ()
{
  constexpr std::size_t width = 2063, height = 9;
  for (int border = 1; border <= 3; ++border)
  {
  const FilterSelectionRegion region = border == 2 ? FilterSelectionRegion {true,true,7,2,2056,7} :
                                                       full_region (width,height,true);
  const EdgeOptions options {1.75,border,5};
  const auto input = fixture (width, height, 4);
  const Bytes sentinel {17,29,43};
  std::atomic<bool> baseline_cancel {false}; auto baseline_progress = std::make_shared<FilterProgress> ();
  Audit baseline (width, height, region, baseline_cancel, baseline_progress); Bytes expected;
  require (audited_vector (input, options, expected, baseline) == Outcome::writes, "Baseline was cancelled");
  baseline.observe ();
  require (baseline.last_progress == 1 && baseline.zero_updates > 0 && baseline.crop_updates > 2 &&
           baseline.flushes == 1 && baseline.max_read <= 4104 && baseline.max_write <= 16384,
           "Progress or bounded I/O contract failed");
  require (baseline.zero_writes == 5 &&
           (border != 2 || (baseline.reads == 45 && baseline.writes == 20)),
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
              audit.wrapmode = border;
              if (fail) { audit.fail_on = operation; audit.fail_at = at; }
              else { audit.cancel_on = operation; audit.cancel_at = at; }
              if (spill)
                {
                  TemporaryFilterRaster in (directory, input.size ()), out (directory, input.size ());
                  in.write (0, input.size (), input.data ());
                  Observed source (in, audit, true), destination (out, audit, false);
                  const auto execute = [&] {
                    return filter_edge_shadow_raster (source, width, height, region, options, cancel, destination, progress);
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
  require (filter_edge_shadow_raster (source, width, height, region, options, cancel, destination, progress) ==
           Outcome::cancelled && in.data == input && out.data == poison,
           "Pre-cancelled direct raster accessed storage");
  require (!audit.reads && !audit.writes && !audit.flushes, "Pre-cancel performed I/O");
  require (filter_edge_shadow_vector (input, width, height, region, options, cancel, result) == Outcome::cancelled &&
           result == sentinel, "Public vector cancellation published output");
  cancel.store (false); Bytes alias = input;
  require (filter_edge_shadow_vector (alias, width, height, region, options, cancel, alias) == Outcome::writes &&
           alias == expected, "Public aliased vector failed transactional publication");
  alias = input; Audit alias_audit (width, height, region, cancel);
  alias_audit.cancel_on = Operation::write; alias_audit.cancel_at = baseline.writes;
  require (audited_vector (alias, options, alias, alias_audit) == Outcome::cancelled && alias == input,
           "Cancelled aliased vector published private output");
  cancel.store (false); result = sentinel;
  rejects<std::invalid_argument> ([&] {
    filter_edge_shadow_vector (input, width, height, {true,true,-1,0,1,1}, options, cancel, result);
  }, "Public vector accepted invalid geometry");
  require (result == sentinel, "Public vector validation error changed output");

  const FilterSelectionRegion outside {true,false,0,0,0,0};
  for (auto operation : {Operation::write, Operation::flush})
    for (bool fail : {false,true})
      {
        cancel.store (false); result = sentinel;
        auto no_progress = std::make_shared<FilterProgress> ();
        Audit empty_work (width, height, outside, cancel, no_progress);
        if (fail) { empty_work.fail_on = operation; empty_work.fail_at = 1; }
        else { empty_work.cancel_on = operation; empty_work.cancel_at = 1; }
        const auto execute = [&] { return audited_vector (input, options, result, empty_work); };
        if (fail) rejects<StorageFailure> (execute, "Empty-ROI failure did not propagate");
        else require (execute () == Outcome::cancelled, "Empty-ROI cancellation completed");
        empty_work.observe ();
        require (result == sentinel && empty_work.reads == 0 && empty_work.last_progress < 1,
                 "Failed/cancelled empty-ROI work published output or read source");
      }
  std::cout << "Every read/write/flush cancellation and failure ordinal: " << checked
            << " vector/spill cases, border " << border << "; empty-ROI, source immutability, transactional aliases and progress passed\n";
  }
}
Bytes read_bytes (const std::string& path)
{
  std::ifstream stream (path,std::ios::binary);
  require (bool (stream),"Cannot open fixture " + path);
  Bytes bytes ((std::istreambuf_iterator<char> (stream)),std::istreambuf_iterator<char> ());
  require (!stream.bad (),"Cannot read fixture " + path); return bytes;
}
/* Tiny drawable boundaries exercise coincident left/right and top/bottom
 * halos; full 5x4 also covers the smear edge-copy paths omitted by the large
 * interior smear case. Enumerate actual observed I/O ordinals for each path. */
void tiny_border_cancellation_errors ()
{
  unsigned checked = 0;
  for (const auto& shape : {std::pair<std::size_t,std::size_t> {1,1}, {1,5}, {5,1}, {5,4}})
    for (int border = 1; border <= 3; ++border)
      {
        const auto width = shape.first, height = shape.second;
        const auto region = full_region (width, height);
        const EdgeOptions options {1.75,border,5};
        const auto input = fixture (width, height, 4);
        const Bytes sentinel {17,29,43};
        std::atomic<bool> baseline_cancel {false};
        auto baseline_progress = std::make_shared<FilterProgress> ();
        Audit baseline (width, height, region, baseline_cancel, baseline_progress);
        Bytes expected;
        require (audited_vector (input, options, expected, baseline) == Outcome::writes,
                 "Tiny boundary baseline did not complete");
        baseline.observe ();
        require (baseline.last_progress == 1 && baseline.flushes == 1 &&
                 expected == expected_shadow (input, width, height, 4, region, options),
                 "Tiny boundary baseline differs from literal C or missed final flush");

        for (bool spill : {false,true})
          for (auto operation : {Operation::read, Operation::write, Operation::flush})
            for (bool fail : {false,true})
              {
                const auto last = operation == Operation::read ? baseline.reads :
                                  operation == Operation::write ? baseline.writes : baseline.flushes;
                for (std::size_t at = 1; at <= last; ++at)
                  {
                    std::atomic<bool> cancel {false};
                    auto progress = std::make_shared<FilterProgress> ();
                    Audit audit (width, height, region, cancel, progress);
                    audit.wrapmode = border;
                    if (fail) { audit.fail_on = operation; audit.fail_at = at; }
                    else { audit.cancel_on = operation; audit.cancel_at = at; }
                    Bytes result = sentinel;
                    if (spill)
                      {
                        TemporaryFilterRaster in (directory, input.size ());
                        TemporaryFilterRaster out (directory, input.size ());
                        in.write (0, input.size (), input.data ());
                        Observed source (in, audit, true), destination (out, audit, false);
                        const auto execute = [&] {
                          return filter_edge_shadow_raster (source, width, height, region,
                                                            options, cancel, destination, progress);
                        };
                        if (fail)
                          rejects<StorageFailure> (execute, "Tiny boundary spill failure did not propagate");
                        else
                          require (execute () == Outcome::cancelled && cancel.load (),
                                   "Tiny boundary spill cancellation completed");
                        Bytes unchanged (input.size ());
                        in.read (0, unchanged.size (), unchanged.data ());
                        require (unchanged == input, "Tiny boundary failure/cancellation changed spill source");
                      }
                    else
                      {
                        const auto execute = [&] { return audited_vector (input, options, result, audit); };
                        if (fail)
                          rejects<StorageFailure> (execute, "Tiny boundary vector failure did not propagate");
                        else
                          require (execute () == Outcome::cancelled && cancel.load (),
                                   "Tiny boundary vector cancellation completed");
                        require (result == sentinel,
                                 "Tiny boundary failure/cancellation published private vector output");
                        require (input == fixture (width, height, 4),
                                 "Tiny boundary failure/cancellation changed vector source");
                      }
                    audit.observe ();
                    const auto observed = operation == Operation::read ? audit.reads :
                                          operation == Operation::write ? audit.writes : audit.flushes;
                    require (observed == at && audit.last_progress < 1,
                             "Tiny boundary I/O continued or completed after selected fault/cancellation");
                    ++checked;
                  }
              }
      }
  std::cout << "Every tiny/full-smear border read/write/flush cancellation and failure ordinal: "
            << checked << " vector/spill cases passed\n";
}

void runtime_goldens (const std::string& edge_directory, const std::string& gray_directory)
{
  unsigned rgba_count = 0, gray_count = 0;
  for (bool gray : {false,true})
    {
      const auto& fixture_directory = gray ? gray_directory : edge_directory;
      std::ifstream manifest (fixture_directory + "/fixtures.tsv");
      require (bool (manifest),"Cannot open fixture manifest");
      std::string line;
      while (std::getline (manifest,line))
        {
          std::istringstream row (line); row >> std::ws;
          if (row.eof () || row.peek () == '#') continue;
          std::size_t width, height; unsigned channels = 4;
          EdgeOptions options; std::string input_name, output_name, procedure, trailing;
          if (gray)
            {
              double wrap;
              require (bool (row >> procedure >> width >> height >> channels >> options.amount >> wrap >>
                              options.edgemode >> input_name >> output_name) && !(row >> trailing),"Malformed Gray fixture");
              if (procedure != "plug-in-edge") continue;
              require ((channels == 1 || channels == 2) && wrap >= 1 && wrap <= 3 && wrap == int (wrap),
                       "Invalid Gray Edge fixture"); options.wrapmode = int (wrap);
            }
          else
            require (bool (row >> width >> height >> options.amount >> options.wrapmode >> options.edgemode >>
                            input_name >> output_name) && !(row >> trailing),"Malformed RGBA fixture");
          const auto native = read_bytes (fixture_directory + "/" + input_name);
          const auto captured = read_bytes (fixture_directory + "/" + output_name);
          require (native.size () == width * height * channels && captured.size () == native.size (),"Fixture extent mismatch");
          const auto input = expand (native,channels), expected = expand (captured,channels);
          const auto region = full_region (width,height);
          const auto oracle = expected_shadow (native,width,height,channels,region,options);
          const auto raw = vector_result (input,width,height,region,options);
          require (raw == oracle && spill_result (input,width,height,region,options) == oracle,
                   "Genuine fixture raw/source-C comparison failed: " + output_name);
          require (merge_full_coverage (input,oracle) == expected && merge_full_coverage (input,raw) == expected,
                   "Independent raw+full-coverage merge differs from genuine output: " + output_name);
          Bytes existing; std::atomic<bool> cancel {false};
          require (filter_edge (input,width,height,options,cancel,existing) && existing == expected,
                   "Existing byte Edge differs from genuine fixture");
          Memory source (input), destination (input.size ());
          require (filter_edge_raster (source,width,height,options,cancel,destination) && destination.data == expected,
                   "Existing raster Edge differs from genuine fixture");
          gray ? ++gray_count : ++rgba_count;
        }
      require (manifest.eof (),"Fixture manifest read failed");
    }
  require (rgba_count == 76 && gray_count == 45,"Incomplete genuine Edge corpus");
  std::cout << "Genuine old Edge final outputs after independent full-coverage merge: "
            << rgba_count << " RGBA and " << gray_count << " Gray passed\n";
}
} // namespace

int main (int argc, char **argv)
{
  GError *error = nullptr;
  gchar *path = g_dir_make_tmp ("painter-edge-shadow-XXXXXX", &error);
  if (!path)
    {
      std::cerr << "Cannot create private spill directory: " << (error ? error->message : "unknown error") << '\n';
      g_clear_error (&error); return 1;
    }
  directory = path; g_free (path);
  int status = 0;
  try
    {
      require (argc == 3, "Expected legacy-edge and legacy-gray-filter fixture directories");
      source_oracle_parity ();
      arithmetic_and_halos ();
      validation ();
      cancellation_errors_and_progress ();
      tiny_border_cancellation_errors ();
      runtime_goldens (argv[1],argv[2]);
    }
  catch (const std::exception& failure) { std::cerr << failure.what () << '\n'; status = 1; }
  if (g_rmdir (directory.c_str ()) != 0)
    { std::cerr << "Private spill directory was not empty or could not be removed\n"; status = 1; }
  if (!status) std::cout << "Edge raw-shadow foundation: all checks passed (76 RGBA, 45 Gray fixtures)\n";
  return status;
}
