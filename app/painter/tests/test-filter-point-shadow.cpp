/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "filter-point-shadow.hpp"
#include "filter-native-kernels.hpp"
#include "filter-progress.hpp"
#include <glib.h>
#include <glib/gstdio.h>
#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

extern "C" void painter_legacy_vinvert_row (const unsigned char *, unsigned char *, int, int);

using namespace GimpPainter;
using Bytes = std::vector<std::uint8_t>;
using Outcome = FilterPointShadowOutcome;
using PointOperation = FilterPointShadowOperation;
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
                       const FilterSelectionRegion& region, std::int32_t threshold,
                       PointOperation operation = PointOperation::threshold_alpha)
{
  Bytes result (width * height * 4, 0);
  for (auto y = region.y1; y < region.y2; ++y)
    for (auto x = region.x1; x < region.x2; ++x)
      {
        const auto offset = (std::size_t (y) * width + x) * 4;
        if (operation == PointOperation::threshold_alpha)
          {
            std::copy_n (input.data () + offset, 3, result.data () + offset);
            result[offset + 3] = threshold < input[offset + 3] ? 255 : 0;
          }
        else if (operation == PointOperation::red_eye)
          {
            std::copy_n(input.data()+offset,4,result.data()+offset);
            const auto r=std::uint64_t(input[offset])*5133333/10000000;
            const auto b=std::uint64_t(input[offset+2])*1933333/10000000;
            const auto g=std::int64_t(input[offset+1]);
            const auto adjustment=(std::int64_t(threshold)-50)*2;
            if(std::int64_t(r)>=g-adjustment&&std::int64_t(r)>=std::int64_t(b)-adjustment)
              result[offset]=std::uint8_t(std::min<std::uint64_t>(255,(g+b)*10000000/10266666));
          }
        else if (operation == PointOperation::value_invert)
          painter_legacy_vinvert_row (input.data () + offset, result.data () + offset, 1, 4);
        else
          {
            // Independent pairwise predicates retain every tied channel. Never
            // consult alpha: raw Max RGB transforms hidden color as well.
            for (unsigned c = 0; c < 3; ++c)
              {
                bool keep = true;
                for (unsigned other = 0; other < 3; ++other)
                  if (threshold > 0 ? input[offset+c] < input[offset+other] :
                                      input[offset+c] > input[offset+other]) keep = false;
                result[offset+c] = keep ? input[offset+c] : 0;
              }
            result[offset+3] = input[offset+3];
          }
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
Outcome run_shadow_raster (FilterRaster& input, std::size_t width, std::size_t height,
                           const FilterSelectionRegion& region, std::int32_t argument,
                           std::atomic<bool>& cancel, FilterRaster& output,
                           const std::shared_ptr<FilterProgress>& progress,
                           PointOperation operation)
{
  return operation == PointOperation::threshold_alpha ?
    filter_point_shadow_raster (input,width,height,region,argument,cancel,output,progress) :
    filter_point_shadow_raster (input,width,height,region,operation,argument,cancel,output,progress);
}
Outcome run_shadow_vector (const Bytes& input, std::size_t width, std::size_t height,
                           const FilterSelectionRegion& region, std::int32_t argument,
                           std::atomic<bool>& cancel, Bytes& output,
                           const std::shared_ptr<FilterProgress>& progress,
                           PointOperation operation)
{
  return operation == PointOperation::threshold_alpha ?
    filter_point_shadow_vector (input,width,height,region,argument,cancel,output,progress) :
    filter_point_shadow_vector (input,width,height,region,operation,argument,cancel,output,progress);
}
Outcome audited_vector (const Bytes& input, std::int32_t threshold, Bytes& output, Audit& audit,
                        PointOperation operation = PointOperation::threshold_alpha)
{
  auto outcome = Outcome::cancelled;
  const auto complete = filter_native_vector (input, audit.cancel, output,
    [&] (FilterRaster& in, FilterRaster& out, std::atomic<bool>& cancel, const FilterRasterFactory&) {
      Observed source (in, audit, true), destination (out, audit, false);
      outcome = run_shadow_raster (source, audit.width, audit.height, audit.region,
                                    threshold, cancel, destination, audit.progress, operation);
      return outcome != Outcome::cancelled;
    });
  return complete ? outcome : Outcome::cancelled;
}
Bytes spill_result (const Bytes& input, std::size_t width, std::size_t height,
                    const FilterSelectionRegion& region, std::int32_t threshold,
                    PointOperation operation = PointOperation::threshold_alpha)
{
  TemporaryFilterRaster in (directory, input.size ()), out (directory, input.size ());
  in.write (0, input.size (), input.data ());
  const Bytes poison (input.size (), 0xa7); out.write (0, poison.size (), poison.data ());
  std::atomic<bool> cancel {false}; auto progress = std::make_shared<FilterProgress> ();
  Audit audit (width, height, region, cancel, progress);
  Observed source (in, audit, true), destination (out, audit, false);
  require (run_shadow_raster (source, width, height, region, threshold, cancel, destination, progress, operation) ==
           (region.intersects ? Outcome::shadow : Outcome::no_merge), "Wrong spill outcome");
  audit.observe ();
  require (audit.last_progress == 1 && audit.flushes == 1, "Spill did not complete after flush");
  Bytes result (input.size ()), unchanged (input.size ());
  out.read (0, result.size (), result.data ()); in.read (0, unchanged.size (), unchanged.data ());
  require (unchanged == input, "Spill changed source input");
  return result;
}
void verify_paths (const Bytes& input, std::size_t width, std::size_t height,
                   const FilterSelectionRegion& region, std::int32_t threshold, bool spill = true,
                   PointOperation operation = PointOperation::threshold_alpha)
{
  const auto expected = expected_shadow (input, width, height, region, threshold, operation);
  const auto outcome = region.intersects ? Outcome::shadow : Outcome::no_merge;
  std::atomic<bool> cancel {false}; Bytes result {19,31,71};
  require (run_shadow_vector (input, width, height, region, threshold, cancel, result, {}, operation) == outcome &&
           result == expected, "Transactional vector disagrees with generated scalar shadow");
  Memory in (input), out (input.size ()); auto progress = std::make_shared<FilterProgress> ();
  Audit audit (width, height, region, cancel, progress);
  Observed source (in, audit, true), destination (out, audit, false);
  require (run_shadow_raster (source, width, height, region, threshold, cancel, destination, progress, operation) == outcome &&
           out.data == expected && in.data == input, "Memory raster changed source or disagrees with scalar shadow");
  audit.observe ();
  require (audit.last_progress == 1 && audit.flushes == 1, "Memory raster lacked completed flush/progress");
  if (spill) require (spill_result (input, width, height, region, threshold, operation) == expected,
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

void cancellation_errors_and_progress (PointOperation point_operation = PointOperation::threshold_alpha,
                                       std::int32_t argument = 127)
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
      require (audited_vector (input,argument,result,baseline,point_operation) == expected_outcome,"Audited baseline failed");
      baseline.observe ();
      require (result == expected_shadow (input,width,height,region,argument,point_operation),"Audited baseline scalar mismatch");
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
                      const auto run = [&] { return run_shadow_raster (source,width,height,region,argument,cancel,destination,progress,point_operation); };
                      if (fail) rejects<StorageFailure> (run,"Disk storage error did not propagate");
                      else require (run () == Outcome::cancelled && cancel.load (),"Disk cancellation completed");
                      Bytes unchanged (input.size ()); in.read (0,unchanged.size (),unchanged.data ());
                      require (unchanged == input,"Cancelled/failed disk shadow changed source");
                    }
                  else
                    {
                      const auto run = [&] { return audited_vector (input,argument,published,audit,point_operation); };
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
            const auto run = [&] { return audited_vector (alias,argument,alias,audit,point_operation); };
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
  require (run_shadow_raster (source,width,height,region,argument,cancel,destination,progress,point_operation) == Outcome::cancelled &&
           run_shadow_vector (input,width,height,region,argument,cancel,result,progress,point_operation) == Outcome::cancelled && result == sentinel,
           "Pre-cancelled shadow completed/published");
  require (in.data == input && out.data == poison && audit.reads == 0 && audit.writes == 0 && audit.flushes == 0,
           "Pre-cancelled shadow accessed input/output");
  cancel.store (false); Bytes alias = input;
  require (run_shadow_vector (alias,width,height,region,argument,cancel,alias,{},point_operation) == Outcome::shadow &&
           alias == expected_shadow (input,width,height,region,argument,point_operation),"Public vector alias transaction failed");
  alias = input; cancel.store (true);
  require (run_shadow_vector (alias,width,height,region,argument,cancel,alias,{},point_operation) == Outcome::cancelled && alias == input,
           "Pre-cancelled alias changed input");
  cancel.store (false);
  rejects<std::invalid_argument> ([&] { run_shadow_vector (alias,width,height,{true,true,0,0,0,1},argument,cancel,alias,{},point_operation); },
                                  "Malformed aliased vector region accepted");
  require (alias == input,"Malformed aliased vector changed input");
  require (run_shadow_vector (alias,width,height,{true,false,0,0,0,7},
                             point_operation == PointOperation::red_eye ? argument : int_minimum,
                             cancel,alias,{},point_operation) == Outcome::no_merge &&
           alias == Bytes (input.size (),0),"Aliased no-merge lost distinct zero-workspace outcome");
  std::cout << (point_operation == PointOperation::max_rgb ? "Max RGB " :
                point_operation == PointOperation::value_invert ? "Value Invert " : "Threshold Alpha ")
            << "Every read/write/flush cancellation and storage-error ordinal: " << cases
            << " vector/spill cases; monotone progress and transactional aliases passed\n";
}
void max_rgb_raw_arithmetic ()
{
  // All distinct-channel permutations, each tied pair, equal channels, and
  // byte extrema; repeat each color at every alpha, including hidden RGB.
  const unsigned colors[][3] = {
    {17,83,139},{17,139,83},{83,17,139},{83,139,17},{139,17,83},{139,83,17},
    {83,83,17},{83,17,83},{17,83,83},{17,17,83},{17,83,17},{83,17,17},
    {0,0,0},{255,255,255},{83,83,83},{0,255,17},{255,0,17},{17,255,0}};
  Bytes input;
  for (unsigned alpha = 0; alpha < 256; ++alpha)
    for (const auto& color : colors)
      input.insert (input.end (), {std::uint8_t(color[0]),std::uint8_t(color[1]),
                                  std::uint8_t(color[2]),std::uint8_t(alpha)});
  const auto width = input.size () / 4;
  for (const auto mode : {int_minimum,-256,-2,-1,0,1,2,3,256,int_maximum})
    {
      const auto expected = expected_shadow (input,width,1,full_region(width,1),mode,PointOperation::max_rgb);
      Memory source (input),raw (input.size ()),legacy (input.size ());
      std::atomic<bool> cancel {false}; auto progress = std::make_shared<FilterProgress> ();
      require (filter_max_rgb_raw_raster (source,raw,width,1,mode,cancel,progress) &&
               raw.data == expected && source.data == input,"Raw Max RGB pairwise/tie/alpha equations differ");
      FilterProgress::Snapshot snapshot;
      require (progress->try_snapshot(snapshot) && snapshot.value == 1,"Raw Max RGB lacked completion progress");
      require (filter_point_raster (source,legacy,width,1,FilterPoint::max_rgb,mode,false,cancel),
               "Legacy merged-semantics Max RGB failed");
      Bytes merged (input.size ());
      require (filter_replace_inten_row (input.data(),raw.data.data(),nullptr,merged.data(),width,4,15),
               "Raw Max RGB generated full-coverage merge failed");
      require (merged == legacy.data,"Raw Max RGB plus full-coverage merge differs from legacy point route");
      unsigned hidden_changed = 0;
      for (std::size_t offset = 0; offset < input.size (); offset += 4)
        {
          require (raw.data[offset+3] == input[offset+3],"Raw Max RGB changed alpha");
          for (unsigned c = 0; c < 3; ++c)
            {
              require (legacy.data[offset+c] == (input[offset+3] ? expected[offset+c] : input[offset+c]),
                       "Existing point_byte alpha-zero behavior changed");
              if (!input[offset+3] && raw.data[offset+c] != input[offset+c]) ++hidden_changed;
            }
        }
      require (hidden_changed > 0,"Raw/merged alpha-zero distinction was not exercised");
      verify_paths (input,width,1,full_region(width,1),mode,false,PointOperation::max_rgb);
    }
  std::cout << "Generated raw Max RGB: signed modes, all ties/all alpha, unchanged legacy hidden RGB, raw-versus-merged distinction passed\n";
}

void legacy_hidden_color_regression ()
{
  const Bytes hidden_bytes {17,83,139,0};
  const double hidden_reals[] = {.9,.4,.7,0};
  Bytes packed_reals (sizeof hidden_reals);
  std::memcpy (packed_reals.data (),hidden_reals,sizeof hidden_reals);
  for (const auto operation : {FilterPoint::value_invert,FilterPoint::max_rgb})
    for (const auto argument : {int_minimum,0,1,int_maximum})
      for (bool real : {false,true})
        {
          const auto& original = real ? packed_reals : hidden_bytes;
          Memory input (original),output (original.size ()); std::atomic<bool> cancel {false};
          require (filter_point_raster (input,output,1,1,operation,argument,real,cancel) &&
                   output.data == original && input.data == original,
                   "Existing byte/real point route no longer preserves alpha-zero hidden color");
        }
  std::cout << "Existing point_byte and point_real Value Invert/Max RGB alpha-zero behavior preserved\n";
}

void max_rgb_geometry_and_isolation ()
{
  unsigned cases = 0;
  for (const auto& shape : {std::pair<std::size_t,std::size_t>{1,1},{1,7},{7,1},{2,2},{17,13},{67,66},{8201,3}})
    {
      const auto width = shape.first,height = shape.second;
      const auto input = fixture (width,height);
      std::vector<FilterSelectionRegion> regions {full_region(width,height),full_region(width,height,true),
        {true,true,0,0,1,1}, {true,true,std::int32_t(width)-1,0,std::int32_t(width),std::int32_t(height)},
        {true,true,0,std::int32_t(height)-1,std::int32_t(width),std::int32_t(height)},
        {true,false,0,0,0,std::int32_t(height)}};
      if (width > 2 && height > 2) regions.push_back ({true,true,1,1,std::int32_t(width)-1,std::int32_t(height)-1});
      if (width == 67) regions.push_back ({true,true,63,63,65,65});
      for (const auto& region : regions)
        for (const auto mode : {int_minimum,-2,0,1,3,int_maximum})
          { verify_paths (input,width,height,region,mode,true,PointOperation::max_rgb); ++cases; }
    }
  constexpr std::size_t width = 19,height = 13;
  const FilterSelectionRegion region {true,true,3,2,16,11};
  const auto input = fixture (width,height); auto changed = input;
  for (std::size_t y = 0; y < height; ++y) for (std::size_t x = 0; x < width; ++x)
    if (x < 3 || x >= 16 || y < 2 || y >= 11) std::fill_n (changed.data()+(y*width+x)*4,4,255);
  for (const auto mode : {int_minimum,1})
    {
      std::atomic<bool> cancel {false}; Bytes completed,other;
      require (filter_point_shadow_vector (input,width,height,region,PointOperation::max_rgb,mode,cancel,completed) == Outcome::shadow &&
               filter_point_shadow_vector (changed,width,height,region,PointOperation::max_rgb,mode,cancel,other) == Outcome::shadow &&
               completed == other,"Outside pixels influenced Max RGB ROI");
      const auto saved = completed;
      // A later successful operation has independent output; cancellation and
      // storage exceptions cannot overwrite the earlier completed buffer.
      require (filter_point_shadow_vector (input,width,height,full_region(width,height),PointOperation::max_rgb,
                                           mode > 0 ? 0 : 1,cancel,other) == Outcome::shadow && completed == saved,
               "Later operation mutated an earlier completed shadow");
      cancel.store (true);
      require (filter_point_shadow_vector (input,width,height,region,PointOperation::max_rgb,mode,cancel,completed) == Outcome::cancelled &&
               completed == saved,"Cancellation overwrote completed Max RGB buffer");
      cancel.store (false); Audit audit (width,height,region,cancel);
      audit.fail_on = Operation::flush; audit.fail_at = 1;
      rejects<StorageFailure> ([&] { audited_vector(input,mode,completed,audit,PointOperation::max_rgb); },
                               "Max RGB flush failure did not propagate");
      require (completed == saved,"Storage failure overwrote completed Max RGB buffer");
    }
  const auto offset_region = selected_region ({false,103,-39,114,-32},99,-42,width,height);
  for (const auto mode : {0,1}) verify_paths(input,width,height,offset_region,mode,true,PointOperation::max_rgb);
  std::cout << "Generated Max RGB vector/memory/spill tiny/ROI/row-boundary/no-merge cases: " << cases
            << "; offset and completed-buffer isolation passed\n";
}

void genuine_max_rgb_corpus (const std::string& manifest)
{
  const auto base = manifest.substr (0,manifest.find_last_of ('/') + 1);
  std::ifstream input(manifest); require(bool(input),"Cannot read genuine Max RGB manifest");
  std::string line; unsigned cases = 0,hidden_differences = 0;
  while (std::getline(input,line))
    {
      if (line.empty() || line[0] == '#') continue;
      std::istringstream row(line); std::string proc,src,dst,extra;
      std::int32_t mode; std::size_t width,height,channels;
      require (bool(row >> proc >> mode >> width >> height >> channels >> src >> dst) && !(row >> extra),
               "Malformed genuine Max RGB fixture row");
      if (proc != "plug-in-max-rgb") continue;
      require (channels == 3 || channels == 4,"Genuine Max RGB fixture is not RGB/RGBA");
      const auto native = read_bytes(base+src),expected = read_bytes(base+dst);
      require (native.size() == width*height*channels && expected.size() == native.size(),"Wrong Max RGB fixture extent");
      Bytes rgba(width*height*4),shadow,merged(rgba.size());
      for (std::size_t pixel = 0; pixel < width*height; ++pixel)
        {
          std::copy_n (native.data()+pixel*channels,3,rgba.data()+pixel*4);
          rgba[pixel*4+3] = channels == 3 ? 255 : native[pixel*channels+3];
        }
      std::atomic<bool> cancel {false};
      require (filter_point_shadow_vector(rgba,width,height,full_region(width,height),PointOperation::max_rgb,mode,cancel,shadow) == Outcome::shadow,
               "Genuine Max RGB raw shadow failed");
      require (shadow == expected_shadow(rgba,width,height,full_region(width,height),mode,PointOperation::max_rgb) &&
               spill_result(rgba,width,height,full_region(width,height),mode,PointOperation::max_rgb) == shadow,
               "Genuine Max RGB raw scalar/vector/spill mismatch");
      // Archived bytes are final PDB output, never raw callback output. The
      // genuine comparison must occur after the known REPLACE_INTEN equation.
      require (filter_replace_inten_row(rgba.data(),shadow.data(),nullptr,merged.data(),width*height,4,15),
               "Genuine Max RGB full-coverage merge rejected");
      for (std::size_t pixel = 0; pixel < width*height; ++pixel)
        for (std::size_t channel = 0; channel < channels; ++channel)
          {
            require (merged[pixel*4+channel] == expected[pixel*channels+channel],
                     "Archived genuine final Max RGB bytes differ: "+dst);
            if (channel < 3 && !rgba[pixel*4+3] && shadow[pixel*4+channel] != merged[pixel*4+channel]) ++hidden_differences;
          }
      ++cases;
    }
  require (cases == 16 && hidden_differences > 0,"Genuine Max RGB cases or raw/final hidden-color distinction lost");
  std::cout << "16 archived original PDB Max RGB outputs matched after explicit merge; no fresh owner-context capture claimed\n";
}

void max_rgb_validation ()
{
  constexpr std::size_t width = 8,height = 7,size = width*height*4;
  Sized input(size),output(size),short_raster(size-1),long_raster(size+1);
  const auto region = full_region(width,height); const Bytes original = fixture(width,height),sentinel{11,23,47};
  for (bool cancelled : {false,true})
    {
      std::atomic<bool> cancel{cancelled};
      for (const auto invalid : {static_cast<PointOperation>(-1),static_cast<PointOperation>(4),static_cast<PointOperation>(99)})
        for (const auto& selected : {region,FilterSelectionRegion{true,false,0,0,0,7}})
          {
            rejects<std::invalid_argument> ([&] { filter_point_shadow_raster(input,width,height,selected,invalid,0,cancel,output); },
                                            "Invalid operation tag passed raster validation");
            Bytes published = sentinel;
            rejects<std::invalid_argument> ([&] { filter_point_shadow_vector(original,width,height,selected,invalid,0,cancel,published); },
                                            "Invalid operation tag passed vector/cancellation validation");
            require(published == sentinel,"Invalid operation tag changed completed output");
          }
      for (auto *bad : {&short_raster,&long_raster})
        {
          rejects<std::invalid_argument> ([&] { filter_max_rgb_raw_raster(*bad,output,width,height,0,cancel); },"Raw Max RGB inexact input extent accepted");
          rejects<std::invalid_argument> ([&] { filter_max_rgb_raw_raster(input,*bad,width,height,0,cancel); },"Raw Max RGB inexact output extent accepted");
          rejects<std::invalid_argument> ([&] { filter_point_shadow_raster(*bad,width,height,region,PointOperation::max_rgb,0,cancel,output); },
                                          "Max RGB shadow inexact extent accepted");
        }
      rejects<std::invalid_argument> ([&] { filter_max_rgb_raw_raster(input,input,width,height,0,cancel); },"Raw Max RGB direct alias accepted");
      rejects<std::invalid_argument> ([&] { filter_max_rgb_raw_raster(input,output,0,height,0,cancel); },"Raw Max RGB zero width accepted");
      rejects<std::invalid_argument> ([&] { filter_max_rgb_raw_raster(input,output,width,0,0,cancel); },"Raw Max RGB zero height accepted");
      rejects<std::invalid_argument> ([&] { filter_point_shadow_raster(input,width,height,region,PointOperation::max_rgb,0,cancel,input); },
                                      "Max RGB shadow direct alias accepted");
    }
  require(input.calls == 0 && output.calls == 0 && short_raster.calls == 0 && long_raster.calls == 0,"Max RGB rejection accessed storage");
  std::atomic<bool> cancel{true};
  require(!filter_max_rgb_raw_raster(input,output,width,height,int_minimum,cancel) && input.calls == 0 && output.calls == 0,
          "Pre-cancelled raw Max RGB accessed storage");
  for (const auto& shape : {std::pair<std::size_t,std::size_t>{46341,46341},{32768,65536},{std::size_t(int_maximum),2}})
    {
      Sized in(std::uint64_t(shape.first)*shape.second*4),out(in.size());
      rejects<std::invalid_argument> ([&] { filter_point_shadow_raster(in,shape.first,shape.second,full_region(shape.first,shape.second),
                                                                     PointOperation::max_rgb,0,cancel,out); },
                                      "Max RGB accepted undefined signed total_area");
      require(in.calls == 0 && out.calls == 0,"Max RGB area rejection performed I/O");
    }
  for (const auto& shape : {std::pair<std::size_t,std::size_t>{1,std::size_t(int_maximum)},{std::size_t(int_maximum),1},{46340,46340}})
    {
      Sized in(std::uint64_t(shape.first)*shape.second*4),out(in.size());
      require(filter_point_shadow_raster(in,shape.first,shape.second,full_region(shape.first,shape.second),
                                         PointOperation::max_rgb,int_maximum,cancel,out) == Outcome::cancelled && in.calls == 0 && out.calls == 0,
              "Max RGB valid signed-area boundary rejected or accessed storage");
    }
  // The explicit Threshold tag is an exact compatibility alias, including
  // unrestricted signed arguments; the original overload is exercised above.
  for (const auto argument : {int_minimum,-1,127,256,int_maximum})
    {
      cancel.store(false); Bytes legacy,tagged;
      require(filter_point_shadow_vector(original,width,height,region,argument,cancel,legacy) == Outcome::shadow &&
              filter_point_shadow_vector(original,width,height,region,PointOperation::threshold_alpha,argument,cancel,tagged) == Outcome::shadow &&
              legacy == tagged,"Tagged Threshold API differs from preserved Threshold overload");
    }
  std::cout << "Max RGB raw/tagged validation, no-I/O rejection, area boundaries and Threshold overload compatibility passed\n";
}

struct RawAudit
{
  std::atomic<bool>& cancel;
  const std::shared_ptr<FilterProgress>& progress;
  Operation trigger = Operation::none;
  std::size_t at = 0,reads = 0,writes = 0,flushes = 0;
  bool fail = false;
  double last = 0;
  RawAudit(std::atomic<bool>& cancel,const std::shared_ptr<FilterProgress>& progress) : cancel(cancel),progress(progress) {}
  void event(Operation operation,std::size_t count = 0)
  {
    require(!cancel.load(),"Raw point kernel performed I/O after cancellation");
    FilterProgress::Snapshot snapshot; require(progress->try_snapshot(snapshot),"Cannot read raw progress");
    require(snapshot.value >= last && snapshot.value < 1,"Raw progress regressed or completed before I/O");
    last = snapshot.value;
    if(operation != Operation::flush) require(count && count <= 32768 && count % 4 == 0,"Raw transfer exceeded fixed chunk bound");
    const auto ordinal = operation == Operation::read ? ++reads : operation == Operation::write ? ++writes : ++flushes;
    if(operation == trigger && ordinal == at)
      { if(fail) throw StorageFailure(); else cancel.store(true); }
  }
};
class RawObserved final : public FilterRaster
{
  FilterRaster& base_; RawAudit& audit_; bool source_;
public:
  RawObserved(FilterRaster& base,RawAudit& audit,bool source) : base_(base),audit_(audit),source_(source) {}
  std::uint64_t size() const noexcept override { return base_.size(); }
  void read(std::uint64_t offset,std::size_t count,std::uint8_t *bytes) override
  { require(source_,"Raw kernel read output"); audit_.event(Operation::read,count); base_.read(offset,count,bytes); }
  void write(std::uint64_t offset,std::size_t count,const std::uint8_t *bytes) override
  { require(!source_,"Raw kernel wrote input"); audit_.event(Operation::write,count); base_.write(offset,count,bytes); }
  void flush() override
  { require(!source_,"Raw kernel flushed input"); audit_.event(Operation::flush); base_.flush(); }
};
void raw_point_faults (PointOperation point_operation)
{
  constexpr std::size_t width = 8205,height = 3;
  const auto input = fixture(width,height); const Bytes sentinel{17,29,43};
  const auto chunks = (input.size()+32767)/32768;
  for (const auto mode : {int_minimum,1})
    for (auto operation : {Operation::read,Operation::write,Operation::flush})
      for (bool fail : {false,true})
        for (std::size_t at = 1; at <= (operation == Operation::flush ? 1 : chunks); ++at)
          {
            std::atomic<bool> cancel{false}; auto progress = std::make_shared<FilterProgress>();
            RawAudit audit(cancel,progress); audit.trigger = operation; audit.at = at; audit.fail = fail;
            Bytes published = sentinel;
            const auto run = [&] {
              return filter_native_vector(input,cancel,published,
                [&](FilterRaster& in,FilterRaster& out,std::atomic<bool>& stop,const FilterRasterFactory&) {
                  RawObserved source(in,audit,true),destination(out,audit,false);
                  return point_operation == PointOperation::value_invert ?
                    filter_value_invert_raw_raster(source,destination,width,height,stop,progress) :
                    filter_max_rgb_raw_raster(source,destination,width,height,mode,stop,progress);
                });
            };
            if(fail) rejects<StorageFailure>(run,"Raw point storage failure did not propagate");
            else require(!run() && cancel.load(),"Raw point cancellation completed");
            require(published == sentinel && input == fixture(width,height),"Raw fault changed source/completed output");
            FilterProgress::Snapshot snapshot;
            require(progress->try_snapshot(snapshot) && snapshot.value < 1,"Raw failure/cancellation reported completion");
            const auto observed = operation == Operation::read ? audit.reads : operation == Operation::write ? audit.writes : audit.flushes;
            require(observed == at,"Raw point continued after injected cancellation/error");
          }
  std::cout << (point_operation == PointOperation::value_invert ? "Value Invert" : "Max RGB")
            << " direct raw bounded read/write/flush cancellation/error ordinals preserve completed output\n";
}


void value_invert_exhaustive_rgb ()
{
  // Every RGB triple, in bounded 65,536-pixel batches. The literal pinned C
  // callback retains its original branch order and runs as separately compiled
  // C. Both bpp=3 and bpp=4 are checked; modern transport is always RGBA8.
  constexpr std::size_t pixels = 256 * 256;
  Bytes rgba (pixels * 4), rgb (pixels * 3), expected (rgba.size ()), rgb_expected (rgb.size ());
  std::atomic<bool> cancel {false};
  for (unsigned red = 0; red < 256; ++red)
    {
      for (unsigned green = 0; green < 256; ++green)
        for (unsigned blue = 0; blue < 256; ++blue)
          {
            const auto pixel = green * 256 + blue;
            rgba[pixel * 4] = rgb[pixel * 3] = red;
            rgba[pixel * 4 + 1] = rgb[pixel * 3 + 1] = green;
            rgba[pixel * 4 + 2] = rgb[pixel * 3 + 2] = blue;
            rgba[pixel * 4 + 3] = (red + 3 * green + 5 * blue) & 255;
          }
      painter_legacy_vinvert_row (rgba.data (), expected.data (), pixels, 4);
      painter_legacy_vinvert_row (rgb.data (), rgb_expected.data (), pixels, 3);
      Memory source (rgba), destination (rgba.size ());
      auto progress = std::make_shared<FilterProgress> ();
      require (filter_value_invert_raw_raster (source, destination, pixels, 1, cancel, progress) &&
               destination.data == expected && source.data == rgba,
               "Raw Value Invert differs from exact pinned C callback across exhaustive RGB triples");
      for (std::size_t pixel = 0; pixel < pixels; ++pixel)
        {
          require (destination.data[pixel * 4 + 3] == rgba[pixel * 4 + 3],
                   "Raw Value Invert changed alpha");
          for (unsigned channel = 0; channel < 3; ++channel)
            require (destination.data[pixel * 4 + channel] == rgb_expected[pixel * 3 + channel],
                     "Pinned RGB and RGBA Value Invert callbacks disagree");
        }
      FilterProgress::Snapshot snapshot;
      require (progress->try_snapshot (snapshot) && snapshot.value == 1,
               "Raw Value Invert did not complete progress");
    }
  std::cout << "Value Invert exact pinned C callback: all 16,777,216 RGB triples, bpp=3/4, varying copied alpha passed\n";
}

void value_invert_all_alpha_and_legacy ()
{
  const unsigned colors[][3] = {
    {0,0,0},{1,1,1},{127,127,127},{254,254,254},{255,255,255},
    {1,0,0},{0,1,0},{0,0,1},{2,1,0},{1,2,0},{1,0,2},
    {3,1,2},{1,3,2},{1,2,3},{7,3,1},{3,7,1},{3,1,7},
    {17,83,139},{139,17,83},{83,139,17},{139,83,17},{83,17,139},{17,139,83},
    {139,139,17},{139,17,139},{17,139,139},{254,127,1},{127,254,1},{127,1,254},
    {255,128,0},{128,255,0},{128,0,255},{0,255,255},{255,0,255},{255,255,0}
  };
  Bytes input;
  for (const auto& color : colors)
    for (unsigned alpha = 0; alpha < 256; ++alpha)
      { for (const auto channel : color) input.push_back (channel); input.push_back (alpha); }
  const auto width = input.size () / 4;
  const auto expected = expected_shadow (input,width,1,full_region(width,1),0,PointOperation::value_invert);
  std::atomic<bool> cancel {false}; Memory source (input), raw (input.size ()), legacy (input.size ());
  require (filter_value_invert_raw_raster (source,raw,width,1,cancel) && raw.data == expected,
           "Value Invert all-alpha representative raw arithmetic differs");
  unsigned hidden_changes = 0;
  for (const auto argument : {int_minimum,-1,0,1,255,int_maximum})
    {
      require (filter_point_raster (source,legacy,width,1,FilterPoint::value_invert,argument,false,cancel),
               "Existing Value Invert byte route failed");
      for (std::size_t offset = 0; offset < input.size (); offset += 4)
        {
          require (legacy.data[offset+3] == input[offset+3],"Legacy Value Invert changed alpha");
          for (unsigned channel = 0; channel < 3; ++channel)
            {
              require (legacy.data[offset+channel] == (input[offset+3] ? expected[offset+channel] : input[offset+channel]),
                       "Legacy Value Invert byte route changed its merged hidden-color contract");
              if (!input[offset+3] && raw.data[offset+channel] != input[offset+channel]) ++hidden_changes;
            }
        }
      verify_paths (input,width,1,full_region(width,1),argument,false,PointOperation::value_invert);
    }
  require (hidden_changes > 0,"Value Invert raw/legacy hidden-color distinction unexercised");
  // Positive alpha, normalized values, extended finite RGB, and achromatic
  // inputs ensure the existing real route retains its non-rounded arithmetic.
  const double samples[][4] = {{.17,.83,.39,.5},{1,1,1,1},{0,0,0,.1},{.5,.5,.5,.8},
                                {1.2,.3,.7,1},{-.2,.4,.1,.4},{.9,.4,.7,0}};
  for (const auto& pixel : samples)
    {
      double wanted[4]; std::copy_n (pixel,4,wanted);
      const double maximum = std::max ({pixel[0],pixel[1],pixel[2]});
      const double minimum = std::min ({pixel[0],pixel[1],pixel[2]});
      if (pixel[3])
        for (unsigned channel = 0; channel < 3; ++channel)
          wanted[channel] = !maximum || maximum == minimum ? 1-maximum : (1-maximum)*pixel[channel]/maximum;
      Bytes bytes (sizeof pixel), answer (sizeof wanted);
      std::memcpy (bytes.data (),pixel,sizeof pixel); std::memcpy (answer.data (),wanted,sizeof wanted);
      Memory real_source(bytes),real_output(bytes.size());
      require (filter_point_raster(real_source,real_output,1,1,FilterPoint::value_invert,0,true,cancel) &&
               real_output.data == answer && real_source.data == bytes,
               "Existing normalized-real Value Invert arithmetic changed");
    }
  std::cout << "Value Invert all-alpha black/achromatic/ties/rounding/hidden-color and unchanged byte/real APIs passed\n";
}

void value_invert_geometry_and_isolation ()
{
  unsigned cases = 0;
  for (const auto& shape : {std::pair<std::size_t,std::size_t>{1,1},{1,7},{7,1},{2,2},{17,13},{67,66},{8201,3}})
    {
      const auto width = shape.first,height = shape.second;
      const auto input = fixture (width,height);
      std::vector<FilterSelectionRegion> regions {full_region(width,height),full_region(width,height,true),
        {true,true,0,0,1,1}, {true,true,std::int32_t(width)-1,0,std::int32_t(width),std::int32_t(height)},
        {true,true,0,std::int32_t(height)-1,std::int32_t(width),std::int32_t(height)},
        {true,false,0,0,0,std::int32_t(height)}};
      if (width > 2 && height > 2) regions.push_back ({true,true,1,1,std::int32_t(width)-1,std::int32_t(height)-1});
      if (width == 67) regions.push_back ({true,true,63,63,65,65});
      for (const auto& region : regions)
        for (const auto argument : {int_minimum,0,int_maximum})
          { verify_paths (input,width,height,region,argument,true,PointOperation::value_invert); ++cases; }
    }
  constexpr std::size_t width = 19,height = 13;
  const FilterSelectionRegion region {true,true,3,2,16,11};
  const auto input = fixture (width,height); auto changed = input;
  for (std::size_t y = 0; y < height; ++y) for (std::size_t x = 0; x < width; ++x)
    if (x < 3 || x >= 16 || y < 2 || y >= 11) std::fill_n (changed.data()+(y*width+x)*4,4,255);
  std::atomic<bool> cancel {false}; Bytes completed,other;
  require (filter_point_shadow_vector(input,width,height,region,PointOperation::value_invert,0,cancel,completed) == Outcome::shadow &&
           filter_point_shadow_vector(changed,width,height,region,PointOperation::value_invert,0,cancel,other) == Outcome::shadow &&
           completed == other,"Outside pixels influenced Value Invert ROI");
  const auto saved = completed;
  require (filter_point_shadow_vector(input,width,height,full_region(width,height),PointOperation::value_invert,0,cancel,other) == Outcome::shadow &&
           completed == saved,"Later Value Invert operation mutated completed shadow");
  cancel.store(true);
  require (filter_point_shadow_vector(input,width,height,region,PointOperation::value_invert,0,cancel,completed) == Outcome::cancelled &&
           completed == saved,"Cancelled Value Invert overwrote completed shadow");
  cancel.store(false); Audit audit(width,height,region,cancel); audit.fail_on = Operation::flush; audit.fail_at = 1;
  rejects<StorageFailure> ([&] { audited_vector(input,0,completed,audit,PointOperation::value_invert); },
                           "Value Invert flush failure did not propagate");
  require (completed == saved,"Value Invert storage failure overwrote completed shadow");
  const auto offset_region = selected_region ({false,103,-39,114,-32},99,-42,width,height);
  verify_paths(input,width,height,offset_region,0,true,PointOperation::value_invert);
  std::cout << "Value Invert vector/memory/spill tiny/ROI/row-boundary/no-merge cases: " << cases
            << "; offset and completed-buffer isolation passed\n";
}

void genuine_value_invert_corpus (const std::string& manifest)
{
  const auto base = manifest.substr (0,manifest.find_last_of ('/') + 1);
  std::ifstream input(manifest); require(bool(input),"Cannot read genuine Value Invert manifest");
  std::string line; unsigned cases = 0,hidden_differences = 0;
  while (std::getline(input,line))
    {
      if (line.empty() || line[0] == '#') continue;
      std::istringstream row(line); std::string proc,src,dst,extra;
      std::int32_t argument; std::size_t width,height,channels;
      require (bool(row >> proc >> argument >> width >> height >> channels >> src >> dst) && !(row >> extra),
               "Malformed genuine Value Invert fixture row");
      if (proc != "plug-in-vinvert") continue;
      require (channels == 3 || channels == 4,"Genuine Value Invert fixture is not RGB/RGBA");
      const auto native = read_bytes(base+src),expected = read_bytes(base+dst);
      require (native.size() == width*height*channels && expected.size() == native.size(),"Wrong Value Invert fixture extent");
      Bytes rgba(width*height*4),shadow,merged(rgba.size());
      for (std::size_t pixel = 0; pixel < width*height; ++pixel)
        {
          std::copy_n (native.data()+pixel*channels,3,rgba.data()+pixel*4);
          rgba[pixel*4+3] = channels == 3 ? 255 : native[pixel*channels+3];
        }
      std::atomic<bool> cancel {false};
      require (filter_point_shadow_vector(rgba,width,height,full_region(width,height),PointOperation::value_invert,argument,cancel,shadow) == Outcome::shadow,
               "Genuine Value Invert raw shadow failed");
      require (shadow == expected_shadow(rgba,width,height,full_region(width,height),0,PointOperation::value_invert) &&
               spill_result(rgba,width,height,full_region(width,height),0,PointOperation::value_invert) == shadow,
               "Genuine Value Invert raw pinned-C/vector/spill mismatch");
      // Archived PDB outputs are observations only of the final merged pixels.
      require (filter_replace_inten_row(rgba.data(),shadow.data(),nullptr,merged.data(),width*height,4,15),
               "Genuine Value Invert full-coverage merge rejected");
      for (std::size_t pixel = 0; pixel < width*height; ++pixel)
        for (std::size_t channel = 0; channel < channels; ++channel)
          {
            require (merged[pixel*4+channel] == expected[pixel*channels+channel],
                     "Archived genuine final Value Invert bytes differ: "+dst);
            if (channel < 3 && !rgba[pixel*4+3] && shadow[pixel*4+channel] != merged[pixel*4+channel]) ++hidden_differences;
          }
      ++cases;
    }
  require (cases == 4 && hidden_differences > 0,"Value Invert archived cases or raw/final distinction lost");
  std::cout << "4 archived original PDB Value Invert outputs matched after explicit merge; no fresh owner-context capture claimed\n";
}

void value_invert_validation ()
{
  constexpr std::size_t width = 8,height = 7,size = width*height*4;
  Sized input(size),output(size),short_raster(size-1),long_raster(size+1);
  const auto region = full_region(width,height); const Bytes original = fixture(width,height),sentinel{11,23,47};
  for (bool cancelled : {false,true})
    {
      std::atomic<bool> cancel{cancelled};
      for (auto *bad : {&short_raster,&long_raster})
        {
          rejects<std::invalid_argument> ([&] { filter_value_invert_raw_raster(*bad,output,width,height,cancel); },"Raw Value Invert inexact input accepted");
          rejects<std::invalid_argument> ([&] { filter_value_invert_raw_raster(input,*bad,width,height,cancel); },"Raw Value Invert inexact output accepted");
          rejects<std::invalid_argument> ([&] { filter_point_shadow_raster(*bad,width,height,region,PointOperation::value_invert,0,cancel,output); },
                                          "Value Invert shadow inexact input accepted");
          rejects<std::invalid_argument> ([&] { filter_point_shadow_raster(input,width,height,region,PointOperation::value_invert,0,cancel,*bad); },
                                          "Value Invert shadow inexact output accepted");
        }
      rejects<std::invalid_argument> ([&] { filter_value_invert_raw_raster(input,input,width,height,cancel); },"Raw Value Invert alias accepted");
      rejects<std::invalid_argument> ([&] { filter_point_shadow_raster(input,width,height,region,PointOperation::value_invert,0,cancel,input); },
                                      "Value Invert shadow alias accepted");
      for (const auto& shape : {std::pair<std::size_t,std::size_t>{0,height},{width,0},
                                 {std::numeric_limits<std::size_t>::max(),height},{width,std::numeric_limits<std::size_t>::max()}})
        rejects<std::invalid_argument> ([&] { filter_value_invert_raw_raster(input,output,shape.first,shape.second,cancel); },
                                        "Raw Value Invert invalid extent accepted");
      for (const auto& bad : {FilterSelectionRegion{false,false,0,0,0,0},{false,true,1,0,8,7},{true,false,1,1,7,6},
                              {true,true,1,1,1,6},{true,true,-1,0,8,7},{true,true,0,0,9,7},{true,false,7,0,3,7}})
        {
          rejects<std::invalid_argument> ([&] { filter_point_shadow_raster(input,width,height,bad,PointOperation::value_invert,0,cancel,output); },
                                          "Value Invert malformed region accepted");
          Bytes published = sentinel;
          rejects<std::invalid_argument> ([&] { filter_point_shadow_vector(original,width,height,bad,PointOperation::value_invert,0,cancel,published); },
                                          "Value Invert malformed vector region accepted");
          require(published == sentinel,"Value Invert validation changed output");
        }
      for (const auto bad_size : {size-1,size+1})
        {
          Bytes published = sentinel;
          rejects<std::invalid_argument> ([&] { filter_point_shadow_vector(Bytes(bad_size),width,height,region,PointOperation::value_invert,0,cancel,published); },
                                          "Value Invert vector inexact extent accepted");
          require(published == sentinel,"Value Invert inexact extent changed output");
        }
    }
  require(input.calls == 0 && output.calls == 0 && short_raster.calls == 0 && long_raster.calls == 0,"Value Invert rejection accessed storage");
  std::atomic<bool> cancel{true};
  require(!filter_value_invert_raw_raster(input,output,width,height,cancel) && input.calls == 0 && output.calls == 0,
          "Pre-cancelled raw Value Invert accessed storage");
  for (const auto& shape : {std::pair<std::size_t,std::size_t>{46341,46341},{32768,65536},{std::size_t(int_maximum),2}})
    {
      Sized in(std::uint64_t(shape.first)*shape.second*4),out(in.size());
      rejects<std::invalid_argument> ([&] { filter_point_shadow_raster(in,shape.first,shape.second,full_region(shape.first,shape.second),
                                                                     PointOperation::value_invert,0,cancel,out); },
                                      "Value Invert accepted undefined signed width*height product");
      require(in.calls == 0 && out.calls == 0,"Value Invert area rejection performed I/O");
    }
  for (const auto& shape : {std::pair<std::size_t,std::size_t>{1,std::size_t(int_maximum)},{std::size_t(int_maximum),1},{46340,46340}})
    {
      Sized in(std::uint64_t(shape.first)*shape.second*4),out(in.size());
      require(filter_point_shadow_raster(in,shape.first,shape.second,full_region(shape.first,shape.second),
                                         PointOperation::value_invert,0,cancel,out) == Outcome::cancelled && in.calls == 0 && out.calls == 0,
              "Value Invert valid signed-area boundary rejected or accessed storage");
    }
  std::cout << "Value Invert raw/tagged no-I/O validation, alias/geometry/extents/signed-area boundaries passed\n";
}

#include "test-filter-red-eye-shadow.inc"

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
      red_eye_raw_and_shadow ();
      scalar_and_tiny_geometry ();
      genuine_point_corpus (argv[1]);
      offsets_outside_and_final_colors ();
      validation ();
      cancellation_errors_and_progress ();
      max_rgb_raw_arithmetic ();
      legacy_hidden_color_regression ();
      max_rgb_geometry_and_isolation ();
      genuine_max_rgb_corpus (argv[1]);
      max_rgb_validation ();
      raw_point_faults (PointOperation::max_rgb);
      cancellation_errors_and_progress (PointOperation::max_rgb,1);
      cancellation_errors_and_progress (PointOperation::max_rgb,int_minimum);
      value_invert_exhaustive_rgb ();
      value_invert_all_alpha_and_legacy ();
      value_invert_geometry_and_isolation ();
      genuine_value_invert_corpus (argv[1]);
      value_invert_validation ();
      raw_point_faults (PointOperation::value_invert);
      cancellation_errors_and_progress (PointOperation::value_invert,0);
    }
  catch (const std::exception& failure) { std::cerr << failure.what () << '\n'; status = 1; }
  if (g_rmdir (directory.c_str ()) != 0)
    { std::cerr << "Private spill directory could not be removed\n"; status = 1; }
  return status;
}
