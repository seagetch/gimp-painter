/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "filter-point-shadow.hpp"
#include "filter-native-kernels.hpp"
#include "filter-progress.hpp"
#include <glib.h>
#include <glib/gstdio.h>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace GimpPainter;
using Bytes = std::vector<std::uint8_t>;
using Outcome = FilterPointShadowOutcome;
namespace {
constexpr auto int_minimum = std::numeric_limits<std::int32_t>::min ();
constexpr auto int_maximum = std::numeric_limits<std::int32_t>::max ();
std::string directory;
void require (bool value, const char *reason)
{ if (!value) throw std::runtime_error (reason); }
void require (bool value, const std::string& reason)
{ if (!value) throw std::runtime_error (reason); }
template<class Error, class Action> void rejects (Action action, const char *reason)
{
  bool rejected = false;
  try { action (); } catch (const Error&) { rejected = true; }
  require (rejected, reason);
}
FilterSelectionRegion full_region (std::size_t width, std::size_t height, bool selected = false)
{ return {selected, true, 0, 0, std::int32_t (width), std::int32_t (height)}; }
FilterSelectionRegion selected_region (const FilterSelectionBounds& bounds,
                                       std::int32_t offset_x, std::int32_t offset_y,
                                       std::int32_t width, std::int32_t height)
{
  FilterSelectionRegion result;
  require (filter_selection_region (bounds, offset_x, offset_y, width, height, result),
           "Fixture selection region was invalid");
  return result;
}
Bytes fixture (std::size_t width, std::size_t height, bool gray = false)
{
  Bytes result (width * height * 4);
  for (std::size_t pixel = 0; pixel < width * height; ++pixel)
    {
      // Alpha visits all 256 values; alpha-zero pixels retain nonzero colors.
      result[pixel * 4] = std::uint8_t ((pixel * 37 + 83) % 255 + 1);
      result[pixel * 4 + 1] = gray ? result[pixel * 4] : std::uint8_t ((pixel * 17 + 19) % 255 + 1);
      result[pixel * 4 + 2] = gray ? result[pixel * 4] : std::uint8_t ((pixel * 53 + 41) % 255 + 1);
      result[pixel * 4 + 3] = std::uint8_t (pixel);
    }
  return result;
}

/* Independent, generated scalar expectations. These assertions are not fresh
 * old-runtime captures; the genuine archived full-image outputs are separate. */
Bytes expected_shadow (const Bytes& input, std::size_t width, std::size_t height,
                       const FilterSelectionRegion& region, std::int32_t threshold)
{
  Bytes result (width * height * 4, 0);
  for (auto y = region.y1; y < region.y2; ++y)
    for (auto x = region.x1; x < region.x2; ++x)
      {
        const auto offset = (std::size_t (y) * width + x) * 4;
        std::copy_n (input.data () + offset, 3, result.data () + offset);
        result[offset + 3] = threshold < input[offset + 3] ? 255 : 0;
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
{ StorageFailure () : std::runtime_error ("Injected point shadow storage error") {} };
struct Audit
{
  const std::size_t width, height;
  const FilterSelectionRegion region;
  const std::uint64_t full_size, crop_size;
  std::atomic<bool>& cancel;
  std::shared_ptr<FilterProgress> progress;
  std::size_t reads = 0, writes = 0, flushes = 0, zero_writes = 0;
  std::size_t max_read = 0, max_write = 0, zero_updates = 0, crop_updates = 0;
  std::uint64_t zero_written = 0, crop_read = 0, crop_written = 0;
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
      require (count > 0 && count <= 16384 && offset % 4 == 0 && count % 4 == 0 &&
               offset <= full_size && count <= full_size - offset,
               "Invalid or unbounded full-raster transfer");
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
        require (zero_written == full_size && region.intersects,
                 "Kernel I/O preceded zero-fill or used an empty crop");
        auto& transferred = operation == Operation::read ? crop_read : crop_written;
        require (transferred < crop_size && count <= crop_size - transferred, "Crop transfer exceeded extent");
        const auto crop_width = std::size_t (region.x2 - region.x1);
        const auto x = std::size_t (transferred / 4 % crop_width), y = transferred / 4 / crop_width;
        const auto expected = ((y + region.y1) * width + x + region.x1) * 4;
        require (offset == expected && count <= (crop_width - x) * 4,
                 "Crop transfer skipped, repeated, crossed row or escaped captured ROI");
        if (operation == Operation::read)
          { ordinal = ++reads; max_read = std::max (max_read, count); }
        else
          {
            ordinal = ++writes; max_write = std::max (max_write, count);
            require (crop_written + count <= crop_read, "Crop write preceded its source read");
          }
        transferred += count;
      }
    else
      {
        ordinal = ++flushes;
        require (zero_written == full_size && crop_read == crop_size &&
                 crop_written == crop_size && flushes == 1,
                 "Shadow flushed before all bytes or flushed twice");
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
Outcome audited_vector (const Bytes& input, std::int32_t threshold, Bytes& output, Audit& audit)
{
  auto outcome = Outcome::cancelled;
  const auto complete = filter_native_vector (input, audit.cancel, output,
    [&] (FilterRaster& in, FilterRaster& out, std::atomic<bool>& cancel, const FilterRasterFactory&) {
      Observed source (in, audit, true), destination (out, audit, false);
      outcome = filter_point_shadow_raster (source, audit.width, audit.height, audit.region,
                                            threshold, cancel, destination, audit.progress);
      return outcome != Outcome::cancelled;
    });
  return complete ? outcome : Outcome::cancelled;
}
Bytes spill_result (const Bytes& input, std::size_t width, std::size_t height,
                    const FilterSelectionRegion& region, std::int32_t threshold)
{
  TemporaryFilterRaster in (directory, input.size ()), out (directory, input.size ());
  in.write (0, input.size (), input.data ());
  const Bytes poison (input.size (), 0xa7); out.write (0, poison.size (), poison.data ());
  std::atomic<bool> cancel {false}; auto progress = std::make_shared<FilterProgress> ();
  Audit audit (width, height, region, cancel, progress);
  Observed source (in, audit, true), destination (out, audit, false);
  require (filter_point_shadow_raster (source, width, height, region, threshold, cancel, destination, progress) ==
           (region.intersects ? Outcome::shadow : Outcome::no_merge), "Wrong spill outcome");
  audit.observe ();
  require (audit.last_progress == 1 && audit.flushes == 1, "Spill did not complete after flush");
  Bytes result (input.size ()), unchanged (input.size ());
  out.read (0, result.size (), result.data ()); in.read (0, unchanged.size (), unchanged.data ());
  require (unchanged == input, "Spill changed source input");
  return result;
}
void verify_paths (const Bytes& input, std::size_t width, std::size_t height,
                   const FilterSelectionRegion& region, std::int32_t threshold, bool spill = true)
{
  const auto expected = expected_shadow (input, width, height, region, threshold);
  const auto outcome = region.intersects ? Outcome::shadow : Outcome::no_merge;
  std::atomic<bool> cancel {false}; Bytes result {19,31,71};
  require (filter_point_shadow_vector (input, width, height, region, threshold, cancel, result) == outcome &&
           result == expected, "Transactional vector disagrees with generated scalar shadow");
  Memory in (input), out (input.size ()); auto progress = std::make_shared<FilterProgress> ();
  Audit audit (width, height, region, cancel, progress);
  Observed source (in, audit, true), destination (out, audit, false);
  require (filter_point_shadow_raster (source, width, height, region, threshold, cancel, destination, progress) == outcome &&
           out.data == expected && in.data == input, "Memory raster changed source or disagrees with scalar shadow");
  audit.observe ();
  require (audit.last_progress == 1 && audit.flushes == 1, "Memory raster lacked completed flush/progress");
  if (spill) require (spill_result (input, width, height, region, threshold) == expected,
                      "Disk shadow disagrees with scalar shadow");
}

void scalar_and_tiny_geometry ()
{
  std::vector<std::int32_t> thresholds {int_minimum, int_maximum};
  for (std::int32_t value = -1; value <= 256; ++value) thresholds.push_back (value);
  unsigned cases = 0;
  for (bool gray : {false,true})
    {
      const auto input = fixture (256,1,gray);
      for (const auto threshold : thresholds)
        { verify_paths (input,256,1,full_region (256,1),threshold,false); ++cases; }
      for (const auto& shape : {std::pair<std::size_t,std::size_t> {1,1},{1,7},{7,1},{2,2},{17,13},{67,66},{8201,3}})
        {
          const auto width = shape.first, height = shape.second;
          const auto native = fixture (width,height,gray);
          std::vector<FilterSelectionRegion> regions {full_region (width,height),full_region (width,height,true),
            {true,true,0,0,1,1}, {true,true,std::int32_t(width)-1,0,std::int32_t(width),std::int32_t(height)},
            {true,true,0,std::int32_t(height)-1,std::int32_t(width),std::int32_t(height)}};
          if (width > 2 && height > 2)
            regions.push_back ({true,true,1,1,std::int32_t(width)-1,std::int32_t(height)-1});
          if (width == 67) regions.push_back ({true,true,63,63,65,65});
          for (const auto& region : regions)
            for (const auto threshold : {int_minimum,-1,0,1,127,254,255,256,int_maximum})
              { verify_paths (native,width,height,region,threshold); ++cases; }
        }
    }
  std::cout << "Generated scalar/all-alpha/tiny/cropped/row-boundary RGBA and replicated-YA cases: " << cases << '\n';
}
Bytes read_bytes (const std::string& path)
{
  std::ifstream input (path,std::ios::binary); require (bool(input),"Cannot open fixture "+path);
  return {std::istreambuf_iterator<char> (input),{}};
}
void genuine_point_corpus (const std::string& manifest)
{
  const auto base = manifest.substr (0,manifest.find_last_of ('/') + 1);
  std::ifstream input (manifest); require (bool(input),"Cannot read genuine point manifest");
  std::string line; unsigned cases = 0;
  while (std::getline (input,line))
    {
      if (line.empty () || line[0] == '#') continue;
      std::istringstream row (line); std::string proc,src,dst,extra;
      std::int32_t threshold; std::size_t width,height,channels;
      require (bool (row >> proc >> threshold >> width >> height >> channels >> src >> dst) && !(row >> extra),
               "Malformed genuine point fixture row");
      if (proc != "plug-in-threshold-alpha") continue;
      require (channels == 2 || channels == 4,"Original Threshold Alpha fixture lacks alpha");
      const auto native = read_bytes (base+src), expected = read_bytes (base+dst);
      require (native.size () == width*height*channels && expected.size () == native.size (),"Wrong genuine point extent");
      Bytes rgba (width*height*4), result;
      for (std::size_t pixel = 0; pixel < width*height; ++pixel)
        {
          if (channels == 2) std::fill_n (rgba.data ()+pixel*4,3,native[pixel*2]);
          else std::copy_n (native.data ()+pixel*4,3,rgba.data ()+pixel*4);
          rgba[pixel*4+3] = native[(pixel+1)*channels-1];
        }
      std::atomic<bool> cancel {false};
      require (filter_point_shadow_vector (rgba,width,height,full_region (width,height),threshold,cancel,result) == Outcome::shadow,
               "Genuine full-drawable Threshold Alpha run failed");
      require (spill_result (rgba,width,height,full_region (width,height),threshold) == result,
               "Genuine full-drawable Threshold Alpha vector/spill mismatch");
      for (std::size_t pixel = 0; pixel < width*height; ++pixel)
        for (std::size_t channel = 0; channel < channels; ++channel)
          require (result[pixel*4+(channels == 2 ? (channel ? 3 : 0) : channel)] == expected[pixel*channels+channel],
                   "Genuine original PDB Threshold Alpha bytes differ: "+dst);
      ++cases;
    }
  require (cases == 20,"Genuine Threshold Alpha subset lost cases");
  std::cout << "20 archived genuine original PDB Threshold Alpha outputs matched in vector/spill paths\n";
}

void offsets_outside_and_final_colors ()
{
  constexpr std::size_t width = 19,height = 13;
  const auto input = fixture (width,height);
  const std::vector<FilterSelectionRegion> regions {
    selected_region ({},99,-42,width,height),
    selected_region ({false,103,-39,114,-32},99,-42,width,height),
    selected_region ({false,-106,53,-95,60},-110,50,width,height),
    selected_region ({false,int_minimum,int_minimum,int_minimum+9,int_minimum+7},int_minimum,int_minimum,width,height),
    selected_region ({false,int_maximum-8,int_maximum-6,int_maximum,int_maximum},int_maximum-18,int_maximum-12,width,height),
    selected_region ({false,-10,2,-1,9},0,0,width,height),
    selected_region ({false,25,2,30,9},0,0,width,height),
    selected_region ({false,3,-9,12,-1},0,0,width,height),
    selected_region ({false,3,20,12,24},0,0,width,height),
    selected_region ({false,int_minimum,int_minimum,int_minimum+1,int_minimum+1},int_maximum,int_maximum,width,height)};
  unsigned outside = 0;
  for (const auto& region : regions)
    { verify_paths (input,width,height,region,127); if (!region.intersects) ++outside; }
  require (outside == 5,"Outside fixtures lost an empty intersection");
  verify_paths (Bytes {83,17,139,0},1,1,{true,false,0,0,0,1},int_minimum);

  const FilterSelectionRegion region {true,true,3,2,16,11};
  auto changed_outside = input;
  for (std::size_t y = 0; y < height; ++y) for (std::size_t x = 0; x < width; ++x)
    if (x < 3 || x >= 16 || y < 2 || y >= 11)
      std::fill_n (changed_outside.data ()+(y*width+x)*4,4,255);
  std::atomic<bool> cancel {false}; Bytes original_shadow,changed_shadow;
  require (filter_point_shadow_vector (input,width,height,region,127,cancel,original_shadow) == Outcome::shadow &&
           filter_point_shadow_vector (changed_outside,width,height,region,127,cancel,changed_shadow) == Outcome::shadow &&
           original_shadow == changed_shadow,"Outside source pixels influenced captured point ROI");

  // The merge arithmetic already has its own oracle suite. Independently
  // assert this operation's RGB/Y invariant across all final masks/components,
  // including alpha lock (inactive alpha) and masks expanding beyond start ROI.
  for (bool gray : {false,true})
    for (const auto threshold : {-1,0,127,255,256})
      {
        const auto rgba = fixture (width,height,gray);
        const auto channels = gray ? 2U : 4U;
        Bytes shadow; require (filter_point_shadow_vector (rgba,width,height,region,threshold,cancel,shadow) == Outcome::shadow,
                               "Final-color fixture failed");
        Bytes native (width*height*channels),raw (native.size ()),merged (native.size ()),mask (width*height);
        for (std::size_t pixel = 0; pixel < width*height; ++pixel)
          for (unsigned c = 0; c < channels; ++c)
            {
              const auto transport = gray ? (c ? 3U : 0U) : c;
              native[pixel*channels+c] = rgba[pixel*4+transport];
              raw[pixel*channels+c] = shadow[pixel*4+transport];
            }
        for (unsigned active = 0; active < (1U<<channels); ++active)
          for (unsigned coverage = 0; coverage < 256; ++coverage)
            {
              std::fill (mask.begin (),mask.end (),std::uint8_t (coverage));
              require (filter_replace_inten_row (native.data (),raw.data (),mask.data (),merged.data (),
                                                 width*height,channels,active),"Final color merge rejected");
              for (std::size_t pixel = 0; pixel < width*height; ++pixel)
                {
                  for (unsigned c = 0; c < channels-1; ++c)
                    require (merged[pixel*channels+c] == native[pixel*channels+c],"Threshold Alpha final RGB/Y changed");
                  if (!(active & (1U<<(channels-1))) || coverage == 0)
                    require (merged[(pixel+1)*channels-1] == native[(pixel+1)*channels-1],"Inactive/locked/unselected alpha changed");
                  else if (coverage == 255)
                    require (merged[(pixel+1)*channels-1] == raw[(pixel+1)*channels-1],"Cleared/expanded final mask did not import raw alpha");
                }
            }
      }
  std::cout << "Offsets/outside distinction and all-final-mask/component RGB/Y invariance passed\n";
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
  constexpr std::size_t width = 8,height = 7,size = width*height*4;
  Sized input (size),output (size),short_raster (size-1),long_raster (size+1);
  std::atomic<bool> cancel {false}; const auto region = full_region (width,height);
  for (const auto& shape : {std::pair<std::size_t,std::size_t> {0,height},{width,0},
       {std::size_t(int_maximum)+1,height},{width,std::size_t(int_maximum)+1},
       {std::numeric_limits<std::size_t>::max(),height},{width,std::numeric_limits<std::size_t>::max()},
       {std::size_t(int_maximum),std::size_t(int_maximum)}})
    {
      rejects<std::invalid_argument> ([&] { filter_point_shadow_validate (shape.first,shape.second,region); },
                                      "Invalid full extent passed scalar preflight");
      rejects<std::invalid_argument> ([&] { filter_point_shadow_raster (input,shape.first,shape.second,region,0,cancel,output); },
                                      "Invalid full extent accessed/completed raster");
    }
  rejects<std::invalid_argument> ([&] { filter_point_shadow_raster (input,width,height,region,0,cancel,input); },
                                  "Direct raster alias accepted");
  for (auto *bad : {&short_raster,&long_raster})
    {
      rejects<std::invalid_argument> ([&] { filter_point_shadow_raster (*bad,width,height,region,0,cancel,output); },
                                      "Inexact input extent accepted");
      rejects<std::invalid_argument> ([&] { filter_point_shadow_raster (input,width,height,region,0,cancel,*bad); },
                                      "Inexact output extent accepted");
    }
  const std::vector<FilterSelectionRegion> invalid {
    {false,false,0,0,0,0}, {false,true,1,0,8,7}, {false,true,0,1,8,7},
    {false,true,0,0,7,7}, {false,true,0,0,8,6}, {true,false,1,1,7,6},
    {true,true,1,1,1,6}, {true,true,1,1,7,1}, {true,false,-1,0,0,0},
    {true,false,0,-1,0,0}, {true,true,-1,0,8,7}, {true,true,0,-1,8,7},
    {true,true,0,0,9,7}, {true,true,0,0,8,8}, {true,false,7,0,3,7},
    {true,false,0,6,8,3}, {true,false,0,0,int_minimum,7}};
  const Bytes original = fixture (width,height),sentinel {11,23,47};
  for (const auto& bad : invalid)
    {
      rejects<std::invalid_argument> ([&] { filter_point_shadow_validate (width,height,bad); },"Malformed region passed preflight");
      rejects<std::invalid_argument> ([&] { filter_point_shadow_raster (input,width,height,bad,int_minimum,cancel,output); },
                                      "Malformed region accepted by raster");
      Bytes published = sentinel;
      rejects<std::invalid_argument> ([&] { filter_point_shadow_vector (original,width,height,bad,int_maximum,cancel,published); },
                                      "Malformed region accepted by vector");
      require (published == sentinel,"Vector validation changed published output");
    }
  for (auto bad_size : {size-1,size+1})
    {
      Bytes published = sentinel;
      rejects<std::invalid_argument> ([&] { filter_point_shadow_vector (Bytes(bad_size),width,height,region,0,cancel,published); },
                                      "Vector accepted inexact input extent");
      require (published == sentinel,"Invalid vector size changed output");
    }
  require (input.calls == 0 && output.calls == 0 && short_raster.calls == 0 && long_raster.calls == 0,
           "Argument validation accessed backing storage");
  for (const auto& shape : {std::pair<std::size_t,std::size_t> {46341,46341},{32768,65536},
       {std::size_t(int_maximum),2},{2,std::size_t(int_maximum)}})
    {
      const auto full = full_region (shape.first,shape.second);
      rejects<std::invalid_argument> ([&] { filter_point_shadow_validate (shape.first,shape.second,full); },
                                      "Undefined signed legacy total_area accepted");
      Sized in (std::uint64_t(shape.first)*shape.second*4),out (in.size ());
      cancel.store (true);
      rejects<std::invalid_argument> ([&] { filter_point_shadow_raster (in,shape.first,shape.second,full,127,cancel,out); },
                                      "Cancellation bypassed legacy-area validation");
      require (in.calls == 0 && out.calls == 0,"Legacy area rejection performed I/O");
    }
  // Declared extents/pre-cancellation, not a claim of high-offset disk I/O.
  for (const auto& shape : {std::pair<std::size_t,std::size_t> {1,std::size_t(int_maximum)},
       {std::size_t(int_maximum),1},{46340,46340}})
    {
      const auto full = full_region (shape.first,shape.second);
      filter_point_shadow_validate (shape.first,shape.second,full);
      Sized in (std::uint64_t(shape.first)*shape.second*4),out (in.size ()); cancel.store (true);
      require (filter_point_shadow_raster (in,shape.first,shape.second,full,int_minimum,cancel,out) == Outcome::cancelled &&
               in.calls == 0 && out.calls == 0,"Valid signed-area boundary rejected or accessed storage");
    }
  for (const auto& shape : {std::pair<std::size_t,std::size_t> {1025,1048577},
       {std::size_t(int_maximum),3},{std::size_t(int_maximum),std::size_t(int_maximum)/2+1}})
    {
      const auto w = std::int32_t(shape.first),h = std::int32_t(shape.second);
      for (const auto& narrow : {FilterSelectionRegion {true,true,w-1,h-1,w,h},
                                 FilterSelectionRegion {true,false,w,h-1,w,h}})
        {
          filter_point_shadow_validate (shape.first,shape.second,narrow);
          Sized in (std::uint64_t(shape.first)*shape.second*4),out (in.size ()); cancel.store (true);
          require (filter_point_shadow_raster (in,shape.first,shape.second,narrow,int_maximum,cancel,out) == Outcome::cancelled &&
                   in.calls == 0 && out.calls == 0,"Valid large full extent/tiny ROI rejected or accessed storage");
        }
    }
  std::cout << "Malformed/overflow/alias/exact extent validation and INT32 total-area boundaries passed\n";
}

void cancellation_errors_and_progress ()
{
  constexpr std::size_t width = 8205,height = 7;
  const auto input = fixture (width,height); const Bytes sentinel {17,29,43};
  unsigned cases = 0;
  for (const auto& region : {FilterSelectionRegion {true,true,3,2,8202,5},
                             FilterSelectionRegion {true,false,0,2,0,5}})
    {
      std::atomic<bool> baseline_cancel {false}; auto baseline_progress = std::make_shared<FilterProgress> ();
      Audit baseline (width,height,region,baseline_cancel,baseline_progress); Bytes result;
      const auto expected_outcome = region.intersects ? Outcome::shadow : Outcome::no_merge;
      require (audited_vector (input,127,result,baseline) == expected_outcome,"Audited baseline failed");
      baseline.observe ();
      require (result == expected_shadow (input,width,height,region,127),"Audited baseline scalar mismatch");
      require (baseline.zero_writes == (input.size()+16383)/16384 && baseline.zero_updates > 1 &&
               baseline.flushes == 1 && baseline.last_progress == 1 && baseline.max_write == 16384 && baseline.max_read <= 16384,
               "Zero phase lacks bounded transfers/staged progress/flushed completion");
      if (region.intersects)
        require (baseline.reads > 3 && baseline.crop_updates > 3 && baseline.crop_read == baseline.crop_size &&
                 baseline.crop_written == baseline.crop_size,"Crop phase lacks expected strip/row boundaries and progress");
      else require (baseline.reads == 0 && baseline.writes == baseline.zero_writes && baseline.crop_updates == 0,
                    "No-merge unexpectedly read source or executed kernel");
      for (bool spill : {false,true})
        for (auto operation : {Operation::read,Operation::write,Operation::flush})
          for (bool fail : {false,true})
            {
              const auto last = operation == Operation::read ? baseline.reads :
                                operation == Operation::write ? baseline.writes : baseline.flushes;
              for (std::size_t at = 1; at <= last; ++at)
                {
                  std::atomic<bool> cancel {false}; auto progress = std::make_shared<FilterProgress> ();
                  Audit audit (width,height,region,cancel,progress); Bytes published = sentinel;
                  if (fail) { audit.fail_on = operation; audit.fail_at = at; }
                  else { audit.cancel_on = operation; audit.cancel_at = at; }
                  if (spill)
                    {
                      TemporaryFilterRaster in (directory,input.size ()),out (directory,input.size ());
                      in.write (0,input.size (),input.data ());
                      Observed source (in,audit,true),destination (out,audit,false);
                      const auto run = [&] { return filter_point_shadow_raster (source,width,height,region,127,cancel,destination,progress); };
                      if (fail) rejects<StorageFailure> (run,"Disk storage error did not propagate");
                      else require (run () == Outcome::cancelled && cancel.load (),"Disk cancellation completed");
                      Bytes unchanged (input.size ()); in.read (0,unchanged.size (),unchanged.data ());
                      require (unchanged == input,"Cancelled/failed disk shadow changed source");
                    }
                  else
                    {
                      const auto run = [&] { return audited_vector (input,127,published,audit); };
                      if (fail) rejects<StorageFailure> (run,"Vector storage error did not propagate");
                      else require (run () == Outcome::cancelled && cancel.load (),"Vector cancellation completed");
                      require (published == sentinel && input == fixture (width,height),"Cancelled/failed vector published or modified source");
                    }
                  audit.observe ();
                  require (audit.last_progress < 1,"Cancelled/failed shadow reported complete");
                  const auto observed = operation == Operation::read ? audit.reads :
                                        operation == Operation::write ? audit.writes : audit.flushes;
                  require (observed == at,"Additional I/O followed cancellation/storage error"); ++cases;
                }
            }
      // Cancellation at the final crop/zero write and final flush preserves an
      // aliased vector too, including the no-merge result's zero workspace.
      for (auto operation : {Operation::write,Operation::flush})
        for (bool fail : {false,true})
          {
            std::atomic<bool> cancel {false}; Bytes alias = input; Audit audit (width,height,region,cancel);
            const auto at = operation == Operation::write ? baseline.writes : baseline.flushes;
            if (fail) { audit.fail_on = operation; audit.fail_at = at; }
            else { audit.cancel_on = operation; audit.cancel_at = at; }
            const auto run = [&] { return audited_vector (alias,127,alias,audit); };
            if (fail) rejects<StorageFailure> (run,"Aliased vector storage error did not propagate");
            else require (run () == Outcome::cancelled,"Late aliased vector cancellation completed");
            require (alias == input,"Late cancelled/failed aliased vector published partial output");
          }
    }
  const FilterSelectionRegion region {true,true,3,2,8202,5};
  std::atomic<bool> cancel {true}; auto progress = std::make_shared<FilterProgress> ();
  Audit audit (width,height,region,cancel,progress);
  Memory in (input),out (input.size ()); Observed source (in,audit,true),destination (out,audit,false);
  const auto poison = out.data; Bytes result = sentinel;
  require (filter_point_shadow_raster (source,width,height,region,127,cancel,destination,progress) == Outcome::cancelled &&
           filter_point_shadow_vector (input,width,height,region,127,cancel,result,progress) == Outcome::cancelled && result == sentinel,
           "Pre-cancelled shadow completed/published");
  require (in.data == input && out.data == poison && audit.reads == 0 && audit.writes == 0 && audit.flushes == 0,
           "Pre-cancelled shadow accessed input/output");
  cancel.store (false); Bytes alias = input;
  require (filter_point_shadow_vector (alias,width,height,region,127,cancel,alias) == Outcome::shadow &&
           alias == expected_shadow (input,width,height,region,127),"Public vector alias transaction failed");
  alias = input; cancel.store (true);
  require (filter_point_shadow_vector (alias,width,height,region,127,cancel,alias) == Outcome::cancelled && alias == input,
           "Pre-cancelled alias changed input");
  cancel.store (false);
  rejects<std::invalid_argument> ([&] { filter_point_shadow_vector (alias,width,height,{true,true,0,0,0,1},127,cancel,alias); },
                                  "Malformed aliased vector region accepted");
  require (alias == input,"Malformed aliased vector changed input");
  require (filter_point_shadow_vector (alias,width,height,{true,false,0,0,0,7},int_minimum,cancel,alias) == Outcome::no_merge &&
           alias == Bytes (input.size (),0),"Aliased no-merge lost distinct zero-workspace outcome");
  std::cout << "Every read/write/flush cancellation and storage-error ordinal: " << cases
            << " vector/spill cases; monotone progress and transactional aliases passed\n";
}
} // namespace

int main (int argc, char **argv)
{
  if (argc != 2) { std::cerr << "Expected genuine point fixture manifest\n"; return 1; }
  GError *error = nullptr;
  gchar *path = g_dir_make_tmp ("painter-point-shadow-XXXXXX",&error);
  if (!path)
    {
      std::cerr << "Cannot create private spill directory: " << (error ? error->message : "unknown error") << '\n';
      g_clear_error (&error); return 1;
    }
  directory = path; g_free (path); int status = 0;
  try
    {
      scalar_and_tiny_geometry ();
      genuine_point_corpus (argv[1]);
      offsets_outside_and_final_colors ();
      validation ();
      cancellation_errors_and_progress ();
    }
  catch (const std::exception& failure) { std::cerr << failure.what () << '\n'; status = 1; }
  if (g_rmdir (directory.c_str ()) != 0)
    { std::cerr << "Private spill directory could not be removed\n"; status = 1; }
  return status;
}
