/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "filter-native-kernels.hpp"
#include "filter-progress.hpp"
#include <glib.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

using namespace GimpPainter;
using Bytes = std::vector<std::uint8_t>;

namespace {
struct Trace
{
  std::shared_ptr<FilterProgress> progress = std::make_shared<FilterProgress> ();
  std::atomic<bool> cancel {false};
  double last = 0, cancel_at = 2;
  unsigned intermediate = 0;

  void observe ()
  {
    FilterProgress::Snapshot state;
    g_assert_true (progress->try_snapshot (state));
    g_assert_true (std::isfinite (state.value));
    g_assert_cmpfloat (state.value, >=, last);
    g_assert_cmpfloat (state.value, <=, 1);
    if (state.value > last && state.value < 1) ++intermediate;
    last = state.value;
    if (last >= cancel_at && last < 1) cancel.store (true);
  }
};

/* Observe the mailbox from real raster operations, so intermediate and
 * cancellation assertions do not depend on the timing of a polling thread.
 * The production progress channel contains no observation callback. */
class Memory final : public FilterRaster
{
  Trace *trace_;
public:
  Bytes data;
  Memory (std::uint64_t size, Trace *trace) : trace_ (trace), data (size) {}
  Memory (const Bytes& bytes, Trace *trace) : trace_ (trace), data (bytes) {}
  std::uint64_t size () const noexcept override { return data.size (); }
  void read (std::uint64_t offset, std::size_t count, std::uint8_t *bytes) override
  {
    if (trace_) trace_->observe ();
    if (offset > size () || count > size () - offset) throw std::out_of_range ("Read");
    std::memcpy (bytes, data.data () + offset, count);
  }
  void write (std::uint64_t offset, std::size_t count, const std::uint8_t *bytes) override
  {
    if (trace_) trace_->observe ();
    if (offset > size () || count > size () - offset) throw std::out_of_range ("Write");
    std::memcpy (data.data () + offset, bytes, count);
  }
  void flush () override { if (trace_) trace_->observe (); }
};

Bytes fixture (std::size_t width, std::size_t height, bool real)
{
  Bytes bytes (width * height * (real ? 32 : 4));
  for (std::size_t pixel = 0; pixel < width * height; ++pixel)
    if (real)
      {
        const double samples[] = {double (pixel % 127) / 126,
                                  double (pixel % 83) / 82, 0.345678912,
                                  pixel % 7 ? 1.0 : 0.0};
        std::memcpy (bytes.data () + pixel * 32, samples, sizeof samples);
      }
    else
      {
        bytes[pixel * 4] = pixel % 251;
        bytes[pixel * 4 + 1] = pixel % 197;
        bytes[pixel * 4 + 2] = 73;
        bytes[pixel * 4 + 3] = pixel % 7 ? 255 : 0;
      }
  return bytes;
}

enum class Kernel { edge, gauss_iir, gauss_rle, gauss_horizontal, gauss_vertical,
                    value_invert, max_rgb, threshold_alpha };

bool run (Kernel kernel, bool real, Memory& input, Memory& output,
          std::size_t width, std::size_t height, Trace& trace, bool report)
{
  const auto progress = report ? trace.progress : std::shared_ptr<FilterProgress> {};
  const FilterRasterFactory factory = [&trace] (std::uint64_t size) {
    return std::unique_ptr<FilterRaster> (new Memory (size, &trace));
  };
  if (kernel == Kernel::edge)
    return real ? filter_edge_real_raster (input, output, width, height, {}, trace.cancel, progress)
                : filter_edge_raster (input, width, height, {}, trace.cancel, output, progress);
  if (kernel == Kernel::value_invert || kernel == Kernel::max_rgb || kernel == Kernel::threshold_alpha)
    {
      const auto point = kernel == Kernel::value_invert ? FilterPoint::value_invert :
                         kernel == Kernel::max_rgb ? FilterPoint::max_rgb : FilterPoint::threshold_alpha;
      return filter_point_raster (input, output, width, height, point, 127, real, trace.cancel, progress);
    }
  const GaussOptions options {kernel == Kernel::gauss_vertical ? 0.0 : 2.5,
                              kernel == Kernel::gauss_horizontal ? 0.0 : 3.25,
                              kernel == Kernel::gauss_iir ? 0 : 1};
  return real ? filter_gauss_real_raster (input, output, width, height, options, trace.cancel, factory, progress)
              : filter_gauss_raster (input, width, height, options, trace.cancel, output, factory, progress);
}

void raster_progress ()
{
  constexpr std::size_t width = 257, height = 65;
  for (bool real : {false, true})
    for (auto kernel : {Kernel::edge, Kernel::gauss_iir, Kernel::gauss_rle,
                        Kernel::gauss_horizontal, Kernel::gauss_vertical,
                        Kernel::value_invert, Kernel::max_rgb, Kernel::threshold_alpha})
      {
        const auto source = fixture (width, height, real);
        Trace trace;
        Memory input (source, &trace), output (source.size (), &trace);
        g_assert_true (run (kernel, real, input, output, width, height, trace, true));
        trace.observe ();
        g_assert_cmpuint (trace.intermediate, >, 1);
        g_assert_cmpfloat (trace.last, ==, 1);
        g_assert_true (input.data == source);
        Trace quiet;
        Memory reference_input (source, nullptr), reference_output (source.size (), nullptr);
        g_assert_true (run (kernel, real, reference_input, reference_output, width, height, quiet, false));
        g_assert_true (output.data == reference_output.data);
      }
}

void raster_cancellation ()
{
  constexpr std::size_t width = 257, height = 65;
  for (bool real : {false, true})
    for (auto kernel : {Kernel::edge, Kernel::gauss_iir, Kernel::gauss_rle,
                        Kernel::gauss_horizontal, Kernel::gauss_vertical,
                        Kernel::value_invert, Kernel::max_rgb, Kernel::threshold_alpha})
      {
        const auto source = fixture (width, height, real);
        for (double cancel_at : {0.0, 0.4, 0.99})
          {
            Trace trace;
            trace.cancel_at = cancel_at;
            Memory input (source, &trace), output (source.size (), &trace);
            g_assert_false (run (kernel, real, input, output, width, height, trace, true));
            trace.observe ();
            g_assert_true (trace.cancel.load ());
            g_assert_cmpfloat (trace.last, <, 1);
            g_assert_true (input.data == source);
          }
      }
}

void vector_progress ()
{
  constexpr std::size_t width = 257, height = 65;
  const auto source = fixture (width, height, false);
  for (bool edge : {false, true})
    for (const auto& radii : {GaussOptions {2.5, 3.25, 0}, {2.5, 3.25, 1},
                             {0, 3.25, 1}, {2.5, 0, 1}})
      {
        Trace trace;
        Bytes output, expected;
        g_assert_true (edge ? filter_edge (source, width, height, {}, trace.cancel, output, trace.progress)
                            : filter_gauss (source, width, height, radii, trace.cancel, output, trace.progress));
        FilterProgress::Snapshot state;
        g_assert_true (trace.progress->try_snapshot (state));
        g_assert_cmpuint (state.revision, >, 2); // Actual work checkpoints preceded completion.
        g_assert_cmpfloat (state.value, ==, 1);
        g_assert_true (edge ? filter_edge (source, width, height, {}, trace.cancel, expected)
                            : filter_gauss (source, width, height, radii, trace.cancel, expected));
        g_assert_true (output == expected);
        trace.progress->reset ();
        trace.cancel.store (true);
        output = {17, 29};
        g_assert_false (edge ? filter_edge (source, width, height, {}, trace.cancel, output, trace.progress)
                             : filter_gauss (source, width, height, radii, trace.cancel, output, trace.progress));
        g_assert_true (trace.progress->try_snapshot (state));
        g_assert_cmpfloat (state.value, ==, 0);
        g_assert_true (output == (Bytes {17, 29}));
      }
}
} // namespace

int main (int argc, char **argv)
{
  g_test_init (&argc, &argv, nullptr);
  g_test_add_func ("/filter-kernel-progress/raster", raster_progress);
  g_test_add_func ("/filter-kernel-progress/cancellation", raster_cancellation);
  g_test_add_func ("/filter-kernel-progress/vector", vector_progress);
  return g_test_run ();
}
