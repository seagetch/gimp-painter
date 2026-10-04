/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Analytic native-extension tests, not legacy byte-oracle evidence. */
#include "filter-context.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
using namespace GimpPainter;
using Samples = std::vector<double>;
void require (bool value, const char *message)
{ if (!value) throw std::runtime_error (message); }
void equal (const Samples& actual, const Samples& expected)
{
  require (actual.size () == expected.size (), "Wrong native result length");
  for (std::size_t i = 0; i < actual.size (); ++i)
    require (std::abs (actual[i] - expected[i]) < 1e-14, "Native Replace differs from analytic result");
}
void replace_cases ()
{
  const double tiny = std::ldexp (1.0, -40);
  const Samples selection {0, tiny, .25, 1, .5, 1, .25, 0};
  const Samples ratio {0, tiny, .5, 0, 1, 1, .1, 0};
  const Samples old_alpha {.5, .5, .25, 0, 0, .25, .75, .5};
  const Samples new_alpha {.75, .5, .75, 0, .75, .75, .25, .25};
  const Samples mixed_alpha {.5, .5, .375, 0, .375, .75, .625, .5};
  for (unsigned channels : {2U, 4U})
    {
      Samples original (8 * channels), shadow (original.size ());
      for (unsigned i = 0; i < 8; ++i)
        {
          for (unsigned c = 0; c < channels - 1; ++c)
            {
              original[i * channels + c] = c == 0 ? .125 : c == 1 ? .75 : -.25;
              shadow[i * channels + c] = c == 0 ? .875 : c == 1 ? .25 : 1.75;
            }
          original[(i + 1) * channels - 1] = old_alpha[i];
          shadow[(i + 1) * channels - 1] = new_alpha[i];
        }
      for (unsigned active = 0; active < (1U << channels); ++active)
        {
          Samples expected (original), output (original.size (), -99.0);
          for (unsigned i = 0; i < 8; ++i)
            {
              for (unsigned c = 0; c < channels - 1; ++c)
                if (active & (1U << c))
                  expected[i * channels + c] +=
                    (shadow[i * channels + c] - original[i * channels + c]) * ratio[i];
              if (active & (1U << (channels - 1))) expected[(i + 1) * channels - 1] = mixed_alpha[i];
            }
          require (filter_replace_native_row (original.data (), shadow.data (), selection.data (),
                                               output.data (), 8, channels, active), "Native merge rejected");
          equal (output, expected);
          auto alias_original = original, alias_shadow = shadow;
          require (filter_replace_native_row (alias_original.data (), shadow.data (), selection.data (),
                                               alias_original.data (), 8, channels, active), "Original alias rejected");
          require (filter_replace_native_row (original.data (), alias_shadow.data (), selection.data (),
                                               alias_shadow.data (), 8, channels, active), "Shadow alias rejected");
          equal (alias_original, expected); equal (alias_shadow, expected);
          for (unsigned i = 0; i < 8; ++i)
            require (filter_replace_native_row (original.data () + i * channels, shadow.data () + i * channels,
                                                 selection.data () + i, output.data () + i * channels,
                                                 1, channels, active), "Single pixel quantum rejected");
          equal (output, expected);
        }
    }
  // Full coverage and zero coverage are exact even with very different finite
  // color magnitudes and signed zero. Hidden source colors survive zero alpha.
  const Samples original {1e100, -0.0, .123456789012345, 1.0};
  const Samples shadow {1e-100, .987654321098765, -.75, .5};
  Samples output (4);
  require (filter_replace_native_row (original.data (), shadow.data (), nullptr,
                                       output.data (), 1, 4, 15), "Unrestricted merge rejected");
  require (!std::memcmp (output.data (), shadow.data (), 4 * sizeof (double)), "Full-coverage endpoint changed");
  require (filter_replace_native_row (original.data (), shadow.data (), nullptr,
                                       output.data (), 1, 4, 15, 0.0), "Zero opacity rejected");
  require (!std::memcmp (output.data (), original.data (), 4 * sizeof (double)), "Zero-coverage endpoint changed");

  const double maximum = std::numeric_limits<double>::max ();
  const double hdr_original[] = {maximum, -maximum, 1.0};
  const double hdr_shadow[] = {-maximum, maximum, 1.0};
  double hdr_output[3];
  require (filter_replace_native_row (hdr_original, hdr_shadow, nullptr, hdr_output,
                                       1, 3, 7, .5), "Finite HDR blend rejected");
  require (hdr_output[0] == 0.0 && hdr_output[1] == 0.0 && hdr_output[2] == 1.0,
           "Opposite-sign HDR interpolation overflowed");
  const double unbounded_selection[] = {-10, 3};
  const double mask_original[] = {.125, .5, .25, .5}, mask_shadow[] = {.875, .75, .75, .75};
  double mask_output[4];
  require (filter_replace_native_row (mask_original, mask_shadow, unbounded_selection,
                                       mask_output, 2, 2, 3), "Finite out-of-range coverage rejected");
  require (mask_output[0] == .125 && mask_output[1] == .5 &&
           mask_output[2] == .75 && mask_output[3] == .75, "Coverage was not clamped");
}
void boundaries ()
{
  const double pixel[] = {.1, .2, .3, .4}; double output[] = {91, 92, 93, 94};
  require (!filter_replace_native_row (pixel, pixel, nullptr, output, 1, 0, 15), "Zero channels accepted");
  require (!filter_replace_native_row (pixel, pixel, nullptr, output, 1, 5, 15), "Five channels accepted");
  require (!filter_replace_native_row (pixel, pixel, nullptr, output,
                                       std::numeric_limits<std::size_t>::max (), 4, 15), "Size overflow accepted");
  require (!filter_replace_native_row (pixel, pixel, nullptr, output,
                                       std::numeric_limits<std::size_t>::max () / 4, 4, 15),
           "Native byte-size overflow accepted");
  require (!filter_replace_native_row (nullptr, pixel, nullptr, output, 1, 4, 15), "Null original accepted");
  require (!filter_replace_native_row (pixel, nullptr, nullptr, output, 1, 4, 15), "Null shadow accepted");
  require (!filter_replace_native_row (pixel, pixel, nullptr, nullptr, 1, 4, 15), "Null output accepted");
  for (double opacity : {-1.0, 1.1, std::numeric_limits<double>::infinity (),
                          std::numeric_limits<double>::quiet_NaN ()})
    require (!filter_replace_native_row (pixel, pixel, nullptr, output, 1, 4, 15, opacity), "Invalid opacity accepted");
  require (output[0] == 91 && output[1] == 92 && output[2] == 93 && output[3] == 94,
           "Invalid arguments changed output");
  require (filter_replace_native_row (nullptr, nullptr, nullptr, nullptr, 0, 4, 15), "Empty row rejected");
  for (double invalid : {std::numeric_limits<double>::infinity (),
                          -std::numeric_limits<double>::infinity (),
                          std::numeric_limits<double>::quiet_NaN ()})
    {
      const double coverage[] = {1, invalid};
      const double valid[] = {.1, .2, .3, .4}, bad[] = {.1, .2, .3, invalid};
      require (!filter_replace_native_row (valid, valid, coverage, output, 2, 2, 3), "Nonfinite mask accepted");
      require (!filter_replace_native_row (bad, valid, nullptr, output, 2, 2, 3), "Nonfinite original accepted");
      require (!filter_replace_native_row (valid, bad, nullptr, output, 2, 2, 0), "Nonfinite inactive shadow accepted");
      require (output[0] == 91 && output[1] == 92 && output[2] == 93 && output[3] == 94,
               "Nonfinite sample rejection wrote partial output");
    }
  FilterSelectionScan scan; FilterSelectionBounds bounds;
  const double tiny = std::ldexp (1.0, -100);
  const double first[] = {0, 0, 0, tiny}, last[] = {0, .5, 0, 0};
  require (scan.reset (4, 2), "Native bounds reset rejected");
  require (!scan.append_native (1, first, 4) && scan.consumed () == 0, "Invalid bounds cursor accepted");
  const double bad_mask[] = {0, 0, tiny, std::numeric_limits<double>::quiet_NaN ()};
  require (!scan.append_native (0, bad_mask, 4) && scan.consumed () == 0,
           "Nonfinite bounds scan mutated state");
  require (scan.append_native (0, first, 4) && !scan.finish (bounds), "Native first bounds quantum failed");
  require (!scan.append_native (4, nullptr, 4) && scan.consumed () == 4, "Null native mask accepted");
  require (scan.append_native (4, last, 4) && scan.finish (bounds), "Native final bounds quantum failed");
  require (!bounds.empty && bounds.x1 == 1 && bounds.y1 == 0 && bounds.x2 == 4 && bounds.y2 == 2,
           "Sub-byte native selection coverage disappeared");
}
}
int main ()
{
  try { replace_cases (); boundaries (); std::cout << "Native Filter context tests passed\n"; return 0; }
  catch (const std::exception& error) { std::cerr << error.what () << '\n'; return 1; }
}
