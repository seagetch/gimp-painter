/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "filter-sharpen-shadow.hpp"
#include "filter-native-kernels.hpp"
#include "filter-progress.hpp"
#include "legacy-sharpen-oracle.h"
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
using Outcome = FilterSharpenShadowOutcome;
namespace {
constexpr auto int_maximum = std::numeric_limits<std::int32_t>::max ();
constexpr auto minimum_percent = 100 - int_maximum;
std::string directory;
void require (bool value, const std::string& reason)
{ if (!value) throw std::runtime_error (reason); }
template<class Error, class Action> void rejects (Action action, const char *reason)
{
  bool rejected = false;
  try { action (); } catch (const Error&) { rejected = true; }
  require (rejected, reason);
}

Bytes fixture (std::size_t width, std::size_t height, unsigned channels)
{
  Bytes result (width * height * channels);
  std::uint32_t state = 0x1395e63aU;
  const std::uint8_t alphas[] = {0, 1, 63, 127, 128, 191, 254, 255};
  for (std::size_t pixel = 0; pixel < width * height; ++pixel)
    for (unsigned channel = 0; channel < channels; ++channel)
      {
        state = state * 1664525U + 1013904223U;
        result[pixel * channels + channel] = channels % 2 == 0 && channel == channels - 1 ?
          alphas[pixel % 8] : std::uint8_t (state >> 24);
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
FilterSelectionRegion selected_region (const FilterSelectionBounds& bounds,
                                       std::int32_t offset_x, std::int32_t offset_y,
                                       std::int32_t width, std::int32_t height)
{
  FilterSelectionRegion region;
  require (filter_selection_region (bounds, offset_x, offset_y, width, height, region),
           "Fixture selection region was invalid");
  return region;
}

/* Crop and placement only: arithmetic comes from the separately compiled,
 * source-extracted C oracle. These are not old-application runtime captures. */
Bytes expected_shadow (const Bytes& native, std::size_t width, std::size_t height,
                       unsigned channels, const FilterSelectionRegion& region,
                       std::int32_t percent)
{
  Bytes result (width * height * 4, 0);
  if (!region.intersects) return result;
  const auto w = std::size_t (region.x2 - region.x1), h = std::size_t (region.y2 - region.y1);
  Bytes crop (w * h * channels), sharpened (crop.size (), 0xa7);
  for (std::size_t y = 0; y < h; ++y)
    std::copy_n (native.data () + ((y + region.y1) * width + region.x1) * channels,
                 w * channels, crop.data () + y * w * channels);
  require (legacy_sharpen_oracle (crop.data (), w, h, channels, percent, sharpened.data ()) == 1,
           "Source-extracted C oracle rejected supported crop");
  const auto rgba = expand (sharpened, channels);
  for (std::size_t y = 0; y < h; ++y)
    std::copy_n (rgba.data () + y * w * 4, w * 4,
                 result.data () + ((y + region.y1) * width + region.x1) * 4);
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
  unsigned read_rows = 0;
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
        const auto crop_height = std::size_t (region.y2 - region.y1);
        const auto x = std::size_t (crop_written / 4 % crop_width), y = crop_written / 4 / crop_width;
        const auto full_x = std::size_t (offset / 4 % width), full_y = offset / 4 / width;
        require (full_x >= std::size_t (region.x1) && full_y >= std::size_t (region.y1) &&
                 full_x + count / 4 <= std::size_t (region.x2) && full_y < std::size_t (region.y2),
                 "Crop transfer escaped captured ROI or crossed full row");
        if (operation == Operation::read)
          {
            ordinal = ++reads; max_read = std::max (max_read, count);
            const auto first = x ? x - 1 : 0, end = std::min (crop_width, x + 1024 + 1);
            require (count <= 1026 * 4 && full_x == std::size_t (region.x1) + first &&
                     count == (end - first) * 4, "Kernel read wrong strip halo");
            const auto source_y = full_y - region.y1;
            require (source_y == y || (y && source_y == y - 1) || source_y == y + 1,
                     "Kernel read outside three-row neighborhood");
            const auto bit = source_y == y ? 2U : source_y < y ? 1U : 4U;
            require (!(read_rows & bit), "Duplicate read of crop neighborhood row");
            read_rows |= bit;
          }
        else
          {
            ordinal = ++writes; max_write = std::max (max_write, count);
            require (full_x == std::size_t (region.x1) + x && full_y == std::size_t (region.y1) + y &&
                     count == std::min (std::size_t (1024), crop_width - x) * 4,
                     "Crop writes are not exact bounded logical strips");
            require (read_rows == (y == 0 || y == crop_height - 1 ? 2U : 7U),
                     "Crop write lacked exact row neighborhood");
            read_rows = 0; crop_written += count;
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

Outcome audited_vector (const Bytes& input, std::int32_t percent, Bytes& output, Audit& audit)
{
  auto outcome = Outcome::cancelled;
  const auto complete = filter_native_vector (input, audit.cancel, output,
    [&] (FilterRaster& in, FilterRaster& out, std::atomic<bool>& cancel, const FilterRasterFactory&) {
      Observed source (in, audit, true), destination (out, audit, false);
      outcome = filter_sharpen_shadow_raster (source, audit.width, audit.height, audit.region,
                                               {percent}, cancel, destination, audit.progress);
      return outcome != Outcome::cancelled;
    });
  return complete ? outcome : Outcome::cancelled;
}
Bytes spill_result (const Bytes& input, std::size_t width, std::size_t height,
                    const FilterSelectionRegion& region, std::int32_t percent)
{
  TemporaryFilterRaster in (directory, input.size ()), out (directory, input.size ());
  in.write (0, input.size (), input.data ());
  // Poison actual disk storage too: outside-ROI zeros must be written explicitly.
  const Bytes poison (input.size (), 0xa7); out.write (0, poison.size (), poison.data ());
  std::atomic<bool> cancel {false}; auto progress = std::make_shared<FilterProgress> ();
  Audit audit (width, height, region, cancel, progress);
  Observed source (in, audit, true), destination (out, audit, false);
  const auto outcome = filter_sharpen_shadow_raster (source, width, height, region, {percent},
                                                     cancel, destination, progress);
  require (outcome == (region.intersects ? Outcome::shadow : Outcome::no_merge), "Wrong spill outcome");
  audit.observe ();
  require (audit.last_progress == 1 && audit.flushes == 1, "Spill output did not complete after flush");
  Bytes result (input.size ()), unchanged (input.size ());
  out.read (0, result.size (), result.data ()); in.read (0, unchanged.size (), unchanged.data ());
  require (unchanged == input, "Spill changed source input");
  return result;
}
void raw_contract (const Bytes& input, const Bytes& output, std::size_t width,
                   std::size_t height, const FilterSelectionRegion& region)
{
  for (std::size_t y = 0; y < height; ++y)
    for (std::size_t x = 0; x < width; ++x)
      {
        const auto offset = (y * width + x) * 4;
        const bool inside = region.intersects && x >= std::size_t (region.x1) && x < std::size_t (region.x2) &&
                             y >= std::size_t (region.y1) && y < std::size_t (region.y2);
        if (!inside)
          require (std::all_of (output.begin () + offset, output.begin () + offset + 4,
                               [] (std::uint8_t value) { return value == 0; }), "Outside-ROI raw shadow is not zero");
        else
          {
            require (input[offset + 3] == output[offset + 3], "Raw crop alpha changed");
            if (x == std::size_t (region.x1) || y == std::size_t (region.y1) ||
                x + 1 == std::size_t (region.x2) || y + 1 == std::size_t (region.y2))
              require (std::equal (input.begin () + offset, input.begin () + offset + 4, output.begin () + offset),
                       "Captured crop border changed");
          }
      }
}
void source_oracle_parity ()
{
  unsigned cases = 0;
  for (unsigned channels = 1; channels <= 4; ++channels)
    for (const auto& shape : {std::pair<std::size_t, std::size_t> {2,3}, {17,11}, {2051,9}})
      {
        const auto width = shape.first, height = shape.second;
        const auto native = fixture (width, height, channels), input = expand (native, channels);
        std::vector<FilterSelectionRegion> regions {full_region (width, height), full_region (width, height, true)};
        if (width > 2)
          {
            const auto w = std::int32_t (width), h = std::int32_t (height);
            // Interior and each touching edge establish all four ROI borders.
            regions.insert (regions.end (), {{true,true,1,1,w-1,h-1}, {true,true,0,1,w-2,h-1},
              {true,true,2,1,w,h-1}, {true,true,1,0,w-1,h-2}, {true,true,1,2,w-1,h},
              {true,true,w-2,1,w,h-1}});
          }
        for (const auto& region : regions)
          for (auto percent : {minimum_percent, -100, -1, 0, 10, 50, 99, 100, int_maximum})
            {
              const auto expected = expected_shadow (native, width, height, channels, region, percent);
              std::atomic<bool> cancel {false}; Bytes result {19,31,71};
              require (filter_sharpen_shadow_vector (input, width, height, region, {percent}, cancel, result) == Outcome::shadow,
                       "Supported vector crop did not produce a shadow");
              require (result == expected, "Vector shadow/source-extracted C mismatch");
              require (spill_result (input, width, height, region, percent) == expected,
                       "Disk shadow/source-extracted C mismatch");
              require (input == expand (native, channels), "Vector changed source input");
              raw_contract (input, result, width, height, region); ++cases;
            }
      }
  std::cout << "Full/partial/all-edge native Y/YA/RGB/RGBA source-C/vector/spill shadow cases: " << cases << '\n';
}
void offsets_outside_and_raw_semantics ()
{
  constexpr std::size_t width = 19, height = 13;
  const auto input = fixture (width, height, 4);
  const auto minimum = std::numeric_limits<std::int32_t>::min ();
  const std::vector<FilterSelectionRegion> regions {
    selected_region ({}, 99, -42, width, height),
    selected_region ({false,103,-39,114,-32}, 99,-42,width,height),
    selected_region ({false,-106,53,-95,60}, -110,50,width,height),
    selected_region ({false,minimum,minimum,minimum+9,minimum+7}, minimum,minimum,width,height),
    selected_region ({false,int_maximum-8,int_maximum-6,int_maximum,int_maximum},
                     int_maximum-18,int_maximum-12,width,height),
    selected_region ({false,-10,2,-1,9}, 0,0,width,height), // left, empty x only
    selected_region ({false,25,2,30,9}, 0,0,width,height), // right, empty x only
    selected_region ({false,3,-9,12,-1}, 0,0,width,height), // top, empty y only
    selected_region ({false,3,20,12,24}, 0,0,width,height), // bottom, empty y only
    selected_region ({false,minimum,minimum,minimum+1,minimum+1},
                     int_maximum,int_maximum,width,height)};
  unsigned outside = 0;
  for (const auto& region : regions)
    {
      std::atomic<bool> cancel {false}; Bytes output;
      const auto outcome = filter_sharpen_shadow_vector (input,width,height,region,{50},cancel,output);
      require (outcome == (region.intersects ? Outcome::shadow : Outcome::no_merge), "Wrong captured-selection outcome");
      const auto expected = expected_shadow (input,width,height,4,region,50);
      require (output == expected && spill_result (input,width,height,region,50) == expected,
               "Offset/outside-selection shadow mismatch");
      raw_contract (input,output,width,height,region);
      if (!region.intersects) ++outside;
    }
  require (outside == 5, "Outside selection fixtures lost an empty intersection");
  const FilterSelectionRegion crop {true,true,3,2,16,11};
  auto changed_outside = input;
  for (std::size_t y=0;y<height;++y) for (std::size_t x=0;x<width;++x)
    if (x < 3 || x >= 16 || y < 2 || y >= 11)
      std::fill_n (changed_outside.data () + (y*width+x)*4,4,255);
  std::atomic<bool> cancel {false}; Bytes baseline, altered;
  require (filter_sharpen_shadow_vector (input,width,height,crop,{50},cancel,baseline) == Outcome::shadow &&
           filter_sharpen_shadow_vector (changed_outside,width,height,crop,{50},cancel,altered) == Outcome::shadow &&
           baseline == altered, "Pixels outside captured ROI influenced its kernel");
  for (unsigned channel=0;channel<4;++channel)
    {
      auto changed = input;
      for (std::size_t pixel=0;pixel<width*height;++pixel) changed[pixel*4+channel] ^= 255;
      require (filter_sharpen_shadow_vector (changed,width,height,crop,{50},cancel,altered) == Outcome::shadow,
               "Component independence fixture failed");
      for (std::size_t pixel=0;pixel<width*height;++pixel) for (unsigned other=0;other<4;++other)
        if (other != channel) require (baseline[pixel*4+other] == altered[pixel*4+other],
                                       "Changing one raw component affected another");
    }
  Bytes hidden (5*5*4,0); const auto center=(2*5+2)*4;
  hidden[center]=100; hidden[center+1]=50; hidden[center+2]=25;
  require (filter_sharpen_shadow_vector (hidden,5,5,{true,true,1,1,4,4},{50},cancel,altered) == Outcome::shadow &&
           altered[center] == 200 && altered[center+1] == 100 && altered[center+2] == 50 && altered[center+3] == 0,
           "Raw hidden RGB was masked, merged, alpha-weighted or component-gated");
  // Unsupported tiny intersections are different from a valid no-merge result.
  require (filter_sharpen_shadow_vector (Bytes (4,0xa7),1,1,{true,false,0,0,0,1},{10},cancel,altered) == Outcome::no_merge &&
           altered == Bytes (4,0), "Outside selection on tiny full raster invented an identity crop");
  std::cout << "Drawable offsets, extreme coordinates, no-selection/outside distinction and independent raw components passed\n";
}

class Sized final : public FilterRaster
{
  const std::uint64_t size_;
public:
  std::size_t calls = 0;
  explicit Sized (std::uint64_t size) : size_ (size) {}
  std::uint64_t size () const noexcept override { return size_; }
  void read (std::uint64_t, std::size_t, std::uint8_t *) override
  { ++calls; throw std::logic_error ("Unexpected sized-raster read"); }
  void write (std::uint64_t, std::size_t, const std::uint8_t *) override
  { ++calls; throw std::logic_error ("Unexpected sized-raster write"); }
  void flush () override { ++calls; throw std::logic_error ("Unexpected sized-raster flush"); }
};
void validation ()
{
  constexpr std::size_t width=8,height=7,size=width*height*4;
  Sized input (size), output (size), short_raster (size-1), long_raster (size+1);
  std::atomic<bool> cancel {false}; const auto region=full_region(width,height);
  for (const auto& shape : {std::pair<std::size_t,std::size_t> {0,height},{width,0},
       {std::size_t(int_maximum)+1,height},{width,std::size_t(int_maximum)+1},
       {std::numeric_limits<std::size_t>::max(),height},{width,std::numeric_limits<std::size_t>::max()},
       {std::size_t(int_maximum),std::size_t(int_maximum)}})
    {
      rejects<std::invalid_argument> ([&] { filter_sharpen_shadow_validate(shape.first,shape.second,region,{}); },
                                      "Invalid full extent passed scalar preflight");
      rejects<std::invalid_argument> ([&] { filter_sharpen_shadow_raster(input,shape.first,shape.second,region,{},cancel,output); },
                                      "Invalid full extent accessed/completed raster");
    }
  rejects<std::invalid_argument> ([&] { filter_sharpen_shadow_raster(input,width,height,region,{},cancel,input); },
                                  "Direct raster input/output alias accepted");
  for (auto *bad : {&short_raster,&long_raster})
    {
      rejects<std::invalid_argument> ([&] { filter_sharpen_shadow_raster(*bad,width,height,region,{},cancel,output); },
                                      "Inexact input extent accepted");
      rejects<std::invalid_argument> ([&] { filter_sharpen_shadow_raster(input,width,height,region,{},cancel,*bad); },
                                      "Inexact output extent accepted");
    }
  const std::vector<FilterSelectionRegion> invalid_regions {
    {false,false,0,0,0,0}, {false,true,1,0,8,7}, {false,true,0,1,8,7},
    {false,true,0,0,7,7}, {false,true,0,0,8,6}, {true,false,1,1,7,6},
    {true,true,1,1,1,6}, {true,true,1,1,7,1}, {true,false,-1,0,0,0},
    {true,false,0,-1,0,0}, {true,true,-1,0,8,7}, {true,true,0,-1,8,7},
    {true,true,0,0,9,7}, {true,true,0,0,8,8}, {true,false,7,0,3,7},
    {true,false,0,6,8,3}, {true,false,0,0,std::numeric_limits<std::int32_t>::min(),7},
    {true,true,1,1,2,6}, {true,true,1,1,7,2}, {true,true,1,1,7,3}};
  const Bytes original=fixture(width,height,4), sentinel {11,23,47};
  for (const auto& bad : invalid_regions)
    {
      rejects<std::invalid_argument> ([&] { filter_sharpen_shadow_raster(input,width,height,bad,{},cancel,output); },
                                      "Malformed/tiny captured region accepted");
      Bytes published=sentinel;
      rejects<std::invalid_argument> ([&] { filter_sharpen_shadow_vector(original,width,height,bad,{},cancel,published); },
                                      "Vector accepted malformed/tiny region");
      require(published==sentinel,"Vector validation changed previously published output");
    }
  for (auto percent : {std::numeric_limits<std::int32_t>::min(),minimum_percent-1})
    for (const auto& selection : {region,FilterSelectionRegion {true,false,0,0,0,7}})
      rejects<std::invalid_argument> ([&] { filter_sharpen_shadow_raster(input,width,height,selection,{percent},cancel,output); },
                                      "Undefined percent accepted, including no-merge path");
  for (auto bad_size : {size-1,size+1})
    {
      Bytes published=sentinel;
      rejects<std::invalid_argument> ([&] { filter_sharpen_shadow_vector(Bytes(bad_size),width,height,region,{},cancel,published); },
                                      "Vector accepted inexact input extent");
      require(published==sentinel,"Invalid vector size changed output");
    }
  require(input.calls==0 && output.calls==0 && short_raster.calls==0 && long_raster.calls==0,
          "Argument validation accessed backing storage");

  // Full width is not the cropped kernel's narrower signed row-stride limit.
  // These are declared-extent/pre-cancellation checks, not high-offset I/O.
  for (const auto& shape : {std::pair<std::size_t,std::size_t> {1025,1048577},
       {std::size_t(int_maximum),3},{std::size_t(int_maximum),std::size_t(int_maximum)/2+1}})
    {
      const auto bytes=std::uint64_t(shape.first)*shape.second*4;
      const auto w=std::int32_t(shape.first),h=std::int32_t(shape.second);
      const FilterSelectionRegion narrow {true,true,w-2,h-3,w,h};
      Sized large_input(bytes),large_output(bytes); cancel.store(true);
      filter_sharpen_shadow_validate(shape.first,shape.second,narrow,{minimum_percent});
      require(filter_sharpen_shadow_raster(large_input,shape.first,shape.second,narrow,{minimum_percent},cancel,large_output)==Outcome::cancelled,
              "Valid large full extent/narrow ROI rejected or completed");
      require(large_input.calls==0 && large_output.calls==0,"Pre-cancelled large extent accessed storage");
    }
  rejects<std::invalid_argument> ([&] {
    filter_sharpen_shadow_validate(std::size_t(int_maximum),3,full_region(int_maximum,3),{});
  },"Oversized intersecting kernel row accepted");
  std::cout << "Malformed/overflow/tiny/alias/exact backing validation and declared 64-bit full extents passed\n";
}

void cancellation_errors_and_progress ()
{
  constexpr std::size_t width=2055,height=11;
  const auto input=fixture(width,height,4);
  const Bytes sentinel {17,29,43};
  unsigned cases=0;
  // Repeat for intersecting and outside ROI: no-merge still owns zero/flush work.
  for (const auto& region : {FilterSelectionRegion {true,true,3,2,2052,9},
                             FilterSelectionRegion {true,false,0,2,0,9}})
    {
      std::atomic<bool> baseline_cancel {false}; auto baseline_progress=std::make_shared<FilterProgress>();
      Audit baseline(width,height,region,baseline_cancel,baseline_progress); Bytes result;
      const auto expected_outcome=region.intersects?Outcome::shadow:Outcome::no_merge;
      require(audited_vector(input,50,result,baseline)==expected_outcome,"Audited baseline failed");
      baseline.observe();
      require(result==expected_shadow(input,width,height,4,region,50),"Audited baseline oracle mismatch");
      require(baseline.zero_writes==6 && baseline.zero_updates>=5 && baseline.flushes==1 && baseline.last_progress==1,
              "Zero phase lacks bounded writes/monotone intermediate progress/flushed completion");
      require(baseline.max_write==16384 && baseline.max_read<=4104,"Transfer bound exceeded");
      if (region.intersects)
        require(baseline.reads==51 && baseline.writes==27 && baseline.crop_updates>=20,
                "Kernel phase lacks expected strips or staged progress");
      else require(baseline.reads==0 && baseline.writes==6 && baseline.crop_updates==0,
                    "No-merge unexpectedly read source or executed kernel");

      for (bool spill : {false,true})
        for (auto operation : {Operation::read,Operation::write,Operation::flush})
          for (bool fail : {false,true})
            {
              const auto last=operation==Operation::read?baseline.reads:
                              operation==Operation::write?baseline.writes:baseline.flushes;
              for (std::size_t at=1;at<=last;++at)
                {
                  std::atomic<bool> cancel {false}; auto progress=std::make_shared<FilterProgress>();
                  Audit audit(width,height,region,cancel,progress); Bytes published=sentinel;
                  if (fail) { audit.fail_on=operation; audit.fail_at=at; }
                  else { audit.cancel_on=operation; audit.cancel_at=at; }
                  if (spill)
                    {
                      TemporaryFilterRaster in(directory,input.size()),out(directory,input.size());
                      in.write(0,input.size(),input.data());
                      Observed source(in,audit,true),destination(out,audit,false);
                      const auto run=[&] {
                        return filter_sharpen_shadow_raster(source,width,height,region,{50},cancel,destination,progress);
                      };
                      if (fail) rejects<StorageFailure>(run,"Disk shadow storage error did not propagate");
                      else require(run()==Outcome::cancelled && cancel.load(),"Disk shadow cancellation completed");
                      Bytes unchanged(input.size()); in.read(0,unchanged.size(),unchanged.data());
                      require(unchanged==input,"Cancelled/failed disk shadow changed source");
                    }
                  else
                    {
                      const auto run=[&] { return audited_vector(input,50,published,audit); };
                      if (fail) rejects<StorageFailure>(run,"Vector shadow storage error did not propagate");
                      else require(run()==Outcome::cancelled && cancel.load(),"Vector shadow cancellation completed");
                      require(published==sentinel,"Cancelled/failed vector published partial shadow");
                      require(input==fixture(width,height,4),"Cancelled/failed vector modified source");
                    }
                  audit.observe();
                  require(audit.last_progress<1,"Cancelled/failed shadow reported complete");
                  const auto observed=operation==Operation::read?audit.reads:
                                      operation==Operation::write?audit.writes:audit.flushes;
                  require(observed==at,"Shadow performed additional I/O after cancellation/storage error");
                  ++cases;
                }
            }
    }

  const FilterSelectionRegion region {true,true,3,2,2052,9};
  std::atomic<bool> cancel {true}; auto progress=std::make_shared<FilterProgress>();
  Audit audit(width,height,region,cancel,progress);
  Memory in(input),out(input.size()); Observed source(in,audit,true),destination(out,audit,false);
  const auto poison=out.data; Bytes result=sentinel;
  require(filter_sharpen_shadow_raster(source,width,height,region,{},cancel,destination,progress)==Outcome::cancelled,
          "Pre-cancelled raster shadow completed");
  require(filter_sharpen_shadow_vector(input,width,height,region,{},cancel,result,progress)==Outcome::cancelled && result==sentinel,
          "Pre-cancelled vector shadow published");
  require(in.data==input && out.data==poison && audit.reads==0 && audit.writes==0 && audit.flushes==0,
          "Pre-cancelled shadow accessed input/output");

  cancel.store(false); Bytes alias=input;
  const auto expected=expected_shadow(input,width,height,4,region,50);
  require(filter_sharpen_shadow_vector(alias,width,height,region,{50},cancel,alias)==Outcome::shadow && alias==expected,
          "Public vector alias transaction failed");
  alias=input; cancel.store(true);
  require(filter_sharpen_shadow_vector(alias,width,height,region,{50},cancel,alias)==Outcome::cancelled && alias==input,
          "Pre-cancelled vector alias changed input");
  cancel.store(false);
  rejects<std::invalid_argument>([&] {
    filter_sharpen_shadow_vector(alias,width,height,region,{minimum_percent-1},cancel,alias);
  },"Invalid aliased vector percentage accepted");
  require(alias==input,"Invalid aliased vector changed input");

  // Late cancellation of an aliased transactional vector, including after the
  // final crop write and in the real final flush, preserves its original bytes.
  for (auto operation : {Operation::write,Operation::flush})
    {
      alias=input; Audit late(width,height,region,cancel);
      late.cancel_on=operation; late.cancel_at=operation==Operation::write?27:1;
      require(audited_vector(alias,50,alias,late)==Outcome::cancelled && alias==input,
              "Late-cancelled aliased vector published partial shadow");
      cancel.store(false);
    }
  alias=input;
  require(filter_sharpen_shadow_vector(alias,width,height,{true,false,0,0,0,7},{50},cancel,alias)==Outcome::no_merge &&
           alias==Bytes(input.size(),0),"Aliased no-merge lost its distinct zero-workspace outcome");
  std::cout << "Every read/write/flush cancellation and storage-error ordinal: " << cases
            << " vector/spill cases; zero/kernel progress and transactional aliases passed\n";
}

} // namespace
int main ()
{
  GError *error=nullptr;
  gchar *path=g_dir_make_tmp("painter-sharpen-shadow-XXXXXX",&error);
  if (!path)
    {
      std::cerr << "Cannot create private spill directory: " << (error?error->message:"unknown error") << '\n';
      g_clear_error(&error); return 1;
    }
  directory=path; g_free(path);
  int status=0;
  try
    {
      source_oracle_parity();
      offsets_outside_and_raw_semantics();
      validation();
      cancellation_errors_and_progress();
    }
  catch (const std::exception& failure) { std::cerr << failure.what() << '\n'; status=1; }
  if (g_rmdir(directory.c_str())!=0)
    { std::cerr << "Private spill directory could not be removed\n"; status=1; }
  return status;
}
