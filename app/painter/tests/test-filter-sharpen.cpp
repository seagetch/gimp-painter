/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "filter-sharpen.hpp"
#include "filter-native-kernels.hpp"
#include "filter-progress.hpp"
#include "legacy-sharpen-oracle.h"
#include <glib.h>
#include <glib/gstdio.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace GimpPainter;
using Bytes = std::vector<std::uint8_t>;
namespace {
constexpr std::int32_t minimum_percent = 100 - std::numeric_limits<std::int32_t>::max ();
std::string directory;
void require (bool value, const std::string& reason)
{ if (!value) throw std::runtime_error (reason); }
template<class Error, class Action> void rejects (Action action, const char *reason)
{
  bool rejected = false;
  try { action (); } catch (const Error&) { rejected = true; }
  require (rejected, reason);
}

Bytes native_fixture (std::size_t w, std::size_t h, unsigned channels, unsigned pattern)
{
  Bytes result (w * h * channels);
  const std::uint8_t alpha[] = {0, 1, 63, 127, 128, 191, 254, 255};
  std::uint32_t random = 0x72b591e3U;
  for (std::size_t y = 0; y < h; ++y)
    for (std::size_t x = 0; x < w; ++x)
      for (unsigned c = 0; c < channels; ++c)
        {
          random = random * 1664525U + 1013904223U;
          const bool is_alpha = (channels == 2 || channels == 4) && c == channels - 1;
          result[(y * w + x) * channels + c] = is_alpha ? alpha[(x + y * 3) % 8] :
            pattern ? (((x + y + c) % 2) ? 255 : 0) : std::uint8_t (random >> 24);
        }
  return result;
}
Bytes expand (const Bytes& native, unsigned channels)
{
  require (channels >= 1 && channels <= 4 && native.size () % channels == 0, "Bad native fixture");
  Bytes result (native.size () / channels * 4);
  for (std::size_t i = 0; i < native.size () / channels; ++i)
    {
      const auto *pixel = native.data () + i * channels;
      result[i * 4] = pixel[0];
      result[i * 4 + 1] = channels <= 2 ? pixel[0] : pixel[1];
      result[i * 4 + 2] = channels <= 2 ? pixel[0] : pixel[2];
      result[i * 4 + 3] = channels == 2 || channels == 4 ? pixel[channels - 1] : 255;
    }
  return result;
}
Bytes oracle (const Bytes& input, std::size_t w, std::size_t h, unsigned channels, std::int32_t percent)
{
  Bytes result (input.size (), 0xa7);
  require (legacy_sharpen_oracle (input.data (), w, h, channels, percent, result.data ()) == 1,
           "Source-extracted C oracle rejected a supported fixture");
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
{ StorageFailure () : std::runtime_error ("Injected sharpen storage failure") {} };
struct Audit
{
  std::size_t width, reads = 0, writes = 0, flushes = 0, max_read = 0, max_write = 0;
  std::uint64_t next_write = 0;
  unsigned read_rows = 0;
  std::atomic<bool>& cancel;
  std::shared_ptr<FilterProgress> progress;
  double last_progress = 0;
  std::size_t intermediate_progress = 0;
  Operation cancel_on = Operation::none, fail_on = Operation::none;
  std::size_t cancel_at = 0, fail_at = 0;

  Audit (std::size_t width, std::atomic<bool>& cancel,
         std::shared_ptr<FilterProgress> progress = {})
    : width (width), cancel (cancel), progress (std::move (progress)) {}
  void observe ()
  {
    if (!progress) return;
    FilterProgress::Snapshot snapshot;
    require (progress->try_snapshot (snapshot), "Could not observe single-threaded progress");
    require (snapshot.value >= last_progress && snapshot.value <= 1, "Non-monotone progress");
    if (snapshot.value > last_progress && snapshot.value < 1) ++intermediate_progress;
    last_progress = snapshot.value;
  }
  void event (Operation operation, std::uint64_t offset = 0, std::size_t count = 0)
  {
    observe ();
    require (!progress || last_progress < 1, "Completion was reported before final I/O");
    std::size_t ordinal;
    if (operation == Operation::read)
      {
        ordinal = ++reads; max_read = std::max (max_read, count);
        require (count > 0 && count <= 1026 * 4 && offset % 4 == 0 && count % 4 == 0,
                 "Read exceeds fixed strip plus halo");
        require ((offset / 4) % width + count / 4 <= width, "Halo read crosses a scanline");
        const auto x = std::size_t ((next_write / 4) % width);
        const auto y = next_write / 4 / width, source_y = offset / 4 / width;
        const auto first = x ? x - 1 : 0;
        const auto end = std::min (width, x + 1024 + 1);
        require ((offset / 4) % width == first && count == (end - first) * 4,
                 "Read has wrong strip halo geometry");
        require (source_y == y || (y && source_y == y - 1) || source_y == y + 1,
                 "Read uses a row outside the 3x3 neighborhood");
        const unsigned bit = source_y == y ? 2 : source_y < y ? 1 : 4;
        require (!(read_rows & bit), "Duplicate source row for one strip");
        read_rows |= bit;
      }
    else if (operation == Operation::write)
      {
        ordinal = ++writes; max_write = std::max (max_write, count);
        require (offset == next_write && offset % 4 == 0 && count % 4 == 0,
                 "Writes are not contiguous pixel-aligned output");
        const auto x = std::size_t ((offset / 4) % width);
        require (x % 1024 == 0 && count == std::min (std::size_t (1024), width - x) * 4,
                 "Write has wrong strip geometry");
        require (read_rows == 2 || read_rows == 7, "Incomplete row neighborhood before write");
        read_rows = 0;
        next_write += count;
      }
    else
      ordinal = ++flushes;
    if (operation == fail_on && ordinal == fail_at)
      throw StorageFailure ();
    if (operation == cancel_on && ordinal == cancel_at) cancel.store (true);
  }
};
class Observed final : public FilterRaster
{
  FilterRaster& base;
  Audit& audit;
  bool source;
public:
  Observed (FilterRaster& base, Audit& audit, bool source) : base (base), audit (audit), source (source) {}
  std::uint64_t size () const noexcept override { return base.size (); }
  void read (std::uint64_t offset, std::size_t count, std::uint8_t *bytes) override
  {
    require (source, "Kernel read private output");
    audit.event (Operation::read, offset, count); base.read (offset, count, bytes);
  }
  void write (std::uint64_t offset, std::size_t count, const std::uint8_t *bytes) override
  {
    require (!source, "Kernel wrote source input");
    audit.event (Operation::write, offset, count); base.write (offset, count, bytes);
  }
  void flush () override
  { require (!source, "Kernel flushed input"); audit.event (Operation::flush); base.flush (); }
};

bool vector_run (const Bytes& input, std::size_t w, std::size_t h, std::int32_t percent,
                 std::atomic<bool>& cancel, Bytes& output, Audit& audit)
{
  return filter_native_vector (input, cancel, output,
    [&] (FilterRaster& in, FilterRaster& out, std::atomic<bool>& stop, const FilterRasterFactory&) {
      Observed source (in, audit, true), destination (out, audit, false);
      return filter_sharpen_raster (source, w, h, {percent}, stop, destination, audit.progress);
    });
}
Bytes vector_run (const Bytes& input, std::size_t w, std::size_t h, std::int32_t percent)
{
  std::atomic<bool> cancel {false}; Audit audit (w, cancel); Bytes output;
  require (vector_run (input, w, h, percent, cancel, output, audit), "Unexpected vector cancellation");
  require (audit.flushes == 1 && audit.next_write == input.size (), "Incomplete vector publication");
  return output;
}
Bytes spill_run (const Bytes& input, std::size_t w, std::size_t h, std::int32_t percent)
{
  TemporaryFilterRaster in (directory, input.size ()), out (directory, input.size ());
  in.write (0, input.size (), input.data ());
  std::atomic<bool> cancel {false}; Audit audit (w, cancel);
  Observed source (in, audit, true), destination (out, audit, false);
  require (filter_sharpen_raster (source, w, h, {percent}, cancel, destination), "Unexpected spill cancellation");
  require (audit.flushes == 1 && audit.next_write == input.size (), "Incomplete spill publication");
  Bytes output (input.size ()), unchanged (input.size ());
  out.read (0, output.size (), output.data ()); in.read (0, unchanged.size (), unchanged.data ());
  require (unchanged == input, "Spill source changed");
  return output;
}
void border_and_alpha (const Bytes& input, const Bytes& output, std::size_t w, std::size_t h)
{
  for (std::size_t y = 0; y < h; ++y)
    for (std::size_t x = 0; x < w; ++x)
      {
        const auto i = (y * w + x) * 4;
        require (input[i + 3] == output[i + 3], "Alpha was filtered");
        if (!x || !y || x == w - 1 || y == h - 1)
          require (std::equal (input.begin () + i, input.begin () + i + 4, output.begin () + i),
                   "Cropped ROI border changed");
      }
}
void source_oracle_parity ()
{
  unsigned cases = 0;
  const std::int32_t percentages[] = {minimum_percent, -1000000, -101, -100, -1, 0, 1, 10, 50, 98, 99, 100, 101,
                                     std::numeric_limits<std::int32_t>::max ()};
  for (unsigned channels = 1; channels <= 4; ++channels)
    for (const auto& shape : {std::pair<std::size_t, std::size_t> {2,3}, {2,7}, {3,3}, {3,4}, {4,3},
                             {7,13}, {19,17}, {1023,5}, {1024,7}, {1025,6}, {2049,5}})
      for (unsigned pattern = 0; pattern < 2; ++pattern)
        {
          const auto w = shape.first, h = shape.second;
          const auto native = native_fixture (w, h, channels, pattern), input = expand (native, channels);
          for (auto percent : percentages)
            {
              const auto expected = expand (oracle (native, w, h, channels, percent), channels);
              const auto vector = vector_run (input, w, h, percent);
              const auto spill = spill_run (input, w, h, percent);
              const auto label = " channels=" + std::to_string (channels) + " width=" + std::to_string (w) +
                " height=" + std::to_string (h) + " percent=" + std::to_string (percent);
              require (vector == expected, "Vector/source-extracted C mismatch" + label);
              require (spill == expected, "Spill/source-extracted C mismatch" + label);
              border_and_alpha (input, vector, w, h); ++cases;
            }
        }
  std::cout << "source-extracted C / native Gray, Gray-alpha, RGB, RGBA / vector / spill: " << cases << " cases passed\n";
}
void arithmetic_and_raw_shadow ()
{
  // This also checks every LUT entry at several non-UI percentages.
  for (auto percent : {minimum_percent, -100, -1, 0, 10, 50, 99, std::numeric_limits<std::int32_t>::max ()})
    for (unsigned value = 0; value < 256; ++value)
      {
        Bytes input (3 * 3 * 4, std::uint8_t (value));
        require (vector_run (input, 3, 3, percent) == input, "Uniform field changed");
      }
  Bytes rounding (3 * 3 * 4, 2);
  std::fill_n (rounding.data () + 4 * 4, 3, 0);
  const auto rounded = vector_run (rounding, 3, 3, -100);
  require (rounded[16] == 1 && rounded[17] == 1 && rounded[18] == 1,
           "Negative LUT used truncation instead of arithmetic shift");
  Bytes hidden (3 * 3 * 4, 0);
  for (std::size_t i = 0; i < 9; ++i) hidden[i * 4 + 3] = 255;
  hidden[16] = hidden[17] = hidden[18] = 100; hidden[19] = 0;
  const auto hidden_result = vector_run (hidden, 3, 3, 50);
  require (hidden_result[16] == 200 && hidden_result[17] == 200 && hidden_result[18] == 200 &&
           hidden_result[19] == 0, "Raw hidden RGB was merged or alpha-weighted");
  Bytes neighbors (3 * 3 * 4, 255);
  for (std::size_t i = 0; i < 9; ++i) neighbors[i * 4 + 3] = 0;
  neighbors[16] = neighbors[17] = neighbors[18] = 100; neighbors[19] = 255;
  require (vector_run (neighbors, 3, 3, 50)[16] == 0, "Transparent neighbors were ignored");
  auto opaque = neighbors;
  for (std::size_t i = 0; i < 9; ++i) opaque[i * 4 + 3] = 255;
  const auto transparent_result = vector_run (neighbors, 3, 3, 50);
  const auto opaque_result = vector_run (opaque, 3, 3, 50);
  for (std::size_t i = 0; i < 9; ++i)
    for (unsigned c = 0; c < 3; ++c)
      require (transparent_result[i * 4 + c] == opaque_result[i * 4 + c], "Alpha changed RGB arithmetic");
  const auto input = native_fixture (19, 7, 4, 0), maximum = vector_run (input, 19, 7, 99);
  for (auto percent : {100, 101, std::numeric_limits<std::int32_t>::max ()})
    require (vector_run (input, 19, 7, percent) == maximum, "Upper intensity clamp changed");
  require (vector_run (input, 19, 7, 0) == input, "Zero percent is not identity");
  // An already-cropped ROI has its own border, independent of outside pixels.
  const auto full = native_fixture (17, 11, 4, 0);
  Bytes cropped (7 * 5 * 4);
  for (std::size_t y = 0; y < 5; ++y)
    std::copy_n (full.data () + ((y + 3) * 17 + 4) * 4, 7 * 4, cropped.data () + y * 7 * 4);
  const auto crop_result = vector_run (cropped, 7, 5, 50);
  require (crop_result == oracle (cropped, 7, 5, 4, 50), "Cropped ROI oracle mismatch");
  border_and_alpha (cropped, crop_result, 7, 5);
  std::cout << "LUT entries, negative rounding, clamp, alpha and raw cropped shadow checks passed\n";
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
  std::atomic<bool> cancel {false}; Sized input (24), output (24), short_raster (23), long_raster (25);
  for (const auto& shape : {std::pair<std::size_t, std::size_t> {0,3}, {1,3}, {2,0}, {2,1}, {2,2},
       {std::size_t (std::numeric_limits<std::int32_t>::max ()) / 4 + 1, 3},
       {2, std::size_t (std::numeric_limits<std::int32_t>::max ()) + 1},
       {std::numeric_limits<std::size_t>::max (), 3}, {2, std::numeric_limits<std::size_t>::max ()}})
    rejects<std::invalid_argument> ([&] { filter_sharpen_raster (input, shape.first, shape.second, {}, cancel, output); },
                                    "Unsupported legacy dimensions accepted");
  rejects<std::invalid_argument> ([&] { filter_sharpen_raster (input, 2, 3, {}, cancel, input); }, "Direct raster alias accepted");
  for (auto *bad : {&short_raster, &long_raster})
    {
      rejects<std::invalid_argument> ([&] { filter_sharpen_raster (*bad, 2, 3, {}, cancel, output); }, "Wrong input size accepted");
      rejects<std::invalid_argument> ([&] { filter_sharpen_raster (input, 2, 3, {}, cancel, *bad); }, "Wrong output size accepted");
    }
  for (auto percent : {std::numeric_limits<std::int32_t>::min (), minimum_percent - 1})
    rejects<std::invalid_argument> ([&] { filter_sharpen_raster (input, 2, 3, {percent}, cancel, output); },
                                    "Undefined legacy percent subtraction accepted");
  require (input.calls == 0 && output.calls == 0 && short_raster.calls == 0 && long_raster.calls == 0,
           "Validation accessed storage");
  // Verify full 64-bit declared extents without traversing or allocating them.
  // This is NOT runtime evidence of I/O at a >4 GiB offset.
  const auto max_width = std::size_t (std::numeric_limits<std::int32_t>::max ()) / 4;
  const auto max_height = std::size_t (std::numeric_limits<std::int32_t>::max ());
  for (auto shape : {std::pair<std::size_t, std::size_t> {1025,1048577}, {max_width,max_height}})
    {
      const auto bytes = std::uint64_t (shape.first) * shape.second * 4;
      Sized large_input (bytes), large_output (bytes); cancel.store (true);
      require (!filter_sharpen_raster (large_input, shape.first, shape.second, {minimum_percent}, cancel, large_output),
               "Cancelled valid 64-bit extent was rejected or completed");
      require (large_input.calls == 0 && large_output.calls == 0, "Pre-cancelled large extent accessed storage");
    }
  std::cout << "Unsupported tiny/overflow/storage cases and pre-cancelled 64-bit extents passed\n";
}
void cancellation_errors_and_progress ()
{
  constexpr std::size_t w = 2049, h = 7;
  const auto input = native_fixture (w, h, 4, 0);
  const Bytes sentinel {17, 29, 43};
  std::atomic<bool> baseline_cancel {false};
  auto baseline_progress = std::make_shared<FilterProgress> ();
  Audit baseline (w, baseline_cancel, baseline_progress); Bytes expected;
  require (vector_run (input, w, h, 50, baseline_cancel, expected, baseline), "Baseline execution cancelled");
  baseline.observe ();
  require (baseline.last_progress == 1 && baseline.intermediate_progress > 2 && baseline.flushes == 1,
           "Progress lacks intermediate/final flushed states");
  require (expected == oracle (input, w, h, 4, 50), "Progress changed output");
  require (baseline.reads == 51 && baseline.writes == 21, "Unexpected strip I/O count");

  unsigned checked = 0;
  for (bool spill : {false, true})
    for (auto operation : {Operation::read, Operation::write, Operation::flush})
      for (bool fail : {false, true})
        {
          const auto last = operation == Operation::read ? baseline.reads :
                            operation == Operation::write ? baseline.writes : baseline.flushes;
          for (std::size_t at = 1; at <= last; ++at)
            {
              std::atomic<bool> cancel {false}; auto progress = std::make_shared<FilterProgress> ();
              Audit audit (w, cancel, progress); Bytes result = sentinel;
              if (fail) { audit.fail_on = operation; audit.fail_at = at; }
              else { audit.cancel_on = operation; audit.cancel_at = at; }
              if (spill)
                {
                  TemporaryFilterRaster in (directory, input.size ()), out (directory, input.size ());
                  in.write (0, input.size (), input.data ());
                  Observed source (in, audit, true), destination (out, audit, false);
                  const auto execute = [&] {
                    return filter_sharpen_raster (source, w, h, {50}, cancel, destination, progress);
                  };
                  if (fail) rejects<StorageFailure> (execute, "Spill storage failure did not propagate");
                  else require (!execute () && cancel.load (), "Spill cancellation did not stop publication");
                  Bytes unchanged (input.size ()); in.read (0, unchanged.size (), unchanged.data ());
                  require (unchanged == input, "Cancelled/failed spill changed source");
                }
              else
                {
                  const auto execute = [&] { return vector_run (input, w, h, 50, cancel, result, audit); };
                  if (fail) rejects<StorageFailure> (execute, "Vector storage failure did not propagate");
                  else require (!execute () && cancel.load (), "Vector cancellation did not stop publication");
                  require (result == sentinel, "Cancelled/failed vector published private output");
                  require (input == native_fixture (w, h, 4, 0), "Cancelled/failed vector changed source");
                }
              audit.observe ();
              require (audit.last_progress < 1, "Cancelled/failed work reported complete");
              const auto observed = operation == Operation::read ? audit.reads :
                                    operation == Operation::write ? audit.writes : audit.flushes;
              require (observed == at, "Kernel performed further I/O after selected cancellation/failure");
              ++checked;
            }
        }
  std::atomic<bool> cancel {true}; auto progress = std::make_shared<FilterProgress> ();
  Audit audit (w, cancel, progress); Bytes result = sentinel;
  require (!vector_run (input, w, h, 10, cancel, result, audit) && result == sentinel,
           "Pre-cancelled vector changed output");
  require (audit.reads == 0 && audit.writes == 0 && audit.flushes == 0, "Pre-cancel did I/O");
  Memory in (input), out (input.size ());
  const auto original_output = out.data;
  Observed source (in, audit, true), destination (out, audit, false);
  require (!filter_sharpen_raster (source, w, h, {10}, cancel, destination, progress),
           "Pre-cancelled direct raster completed");
  require (in.data == input && out.data == original_output && audit.reads == 0 && audit.writes == 0 && audit.flushes == 0,
           "Pre-cancelled direct raster accessed storage");
  cancel.store (false);
  Bytes alias = input; Audit alias_audit (w, cancel);
  require (vector_run (alias, w, h, 50, cancel, alias, alias_audit) && alias == expected,
           "Vector input/output alias publication failed");
  alias = input; Audit cancelled_alias (w, cancel);
  cancelled_alias.cancel_on = Operation::write; cancelled_alias.cancel_at = baseline.writes;
  require (!vector_run (alias, w, h, 50, cancel, alias, cancelled_alias) && alias == input,
           "Cancelled aliased vector published output");
  cancel.store (false); Audit invalid (w, cancel); result = sentinel;
  rejects<std::invalid_argument> ([&] { vector_run (input, w, h, minimum_percent - 1, cancel, result, invalid); },
                                  "Vector accepted undefined percent");
  require (result == sentinel, "Vector validation error changed output");
  std::cout << "Every read/write/flush cancellation and failure ordinal: " << checked
            << " vector/spill cases; alias publication and progress checks passed\n";
}

} // namespace

int main ()
{
  GError *error = nullptr;
  gchar *path = g_dir_make_tmp ("painter-sharpen-XXXXXX", &error);
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
      arithmetic_and_raw_shadow ();
      validation ();
      cancellation_errors_and_progress ();
    }
  catch (const std::exception& failure) { std::cerr << failure.what () << '\n'; status = 1; }
  if (g_rmdir (directory.c_str ()) != 0)
    { std::cerr << "Private spill directory was not empty or could not be removed\n"; status = 1; }
  return status;
}
