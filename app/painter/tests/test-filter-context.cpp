/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "filter-context.hpp"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace GimpPainter;
using Bytes = std::vector<std::uint8_t>;

void require (bool condition, const std::string& reason)
{
  if (!condition) throw std::runtime_error (reason);
}

Bytes read (const std::string& path, std::size_t count)
{
  std::ifstream stream (path, std::ios::binary);
  require (stream.good (), "Cannot read " + path);
  Bytes bytes ((std::istreambuf_iterator<char> (stream)), std::istreambuf_iterator<char> ());
  require (bytes.size () == count, "Wrong byte count " + path);
  return bytes;
}

void native_merges (const std::string& directory)
{
  std::ifstream table (directory + "/merges.tsv");
  require (table.good (), "Missing genuine old native merge fixtures");
  std::string id;
  unsigned width, height, channels, active, empty;
  std::size_t cases = 0;
  while (table >> id >> width >> height >> channels >> active >> empty)
    {
      const std::size_t pixels = static_cast<std::size_t> (width) * height;
      const auto input = read (directory + "/" + id + "-input.raw", pixels * channels);
      const auto shadow = read (directory + "/" + id + "-shadow.raw", pixels * channels);
      const auto coverage = read (directory + "/" + id + "-mask.raw", pixels);
      const auto expected = read (directory + "/" + id + "-output.raw", pixels * channels);
      if (id.find ("native-") == 0)
        {
          const auto zero = std::find (coverage.begin (), coverage.end (), 0);
          const auto full = std::find (coverage.begin (), coverage.end (), 255);
          if (id.find ("-s0-") != std::string::npos)
            require (empty && zero == coverage.end (), id + " unrestricted mask fixture");
          else
            require (!empty, id + " selection fixture was incorrectly globally empty");
          if (id.find ("-s1-") != std::string::npos)
            require (zero != coverage.end () && full != coverage.end (), id + " missing hard selection");
          if (id.find ("-s2-") != std::string::npos)
            require (std::any_of (coverage.begin (), coverage.end (), [] (auto v) { return v && v != 255; }),
                     id + " missing soft selection");
          if (id.find ("-s3-") != std::string::npos)
            require (std::all_of (coverage.begin (), coverage.end (), [] (auto v) { return !v; }),
                     id + " outside selection must have zero local coverage");
        }
      Bytes output (input.size (), 173);
      require (filter_replace_inten_row (input.data (), shadow.data (), coverage.data (),
                                        output.data (), pixels, channels, active), id + " rejected");
      require (output == expected, id + " differs from the old native shadow merge");
      auto alias_input = input;
      require (filter_replace_inten_row (alias_input.data (), shadow.data (), coverage.data (),
                                        alias_input.data (), pixels, channels, active), "Input alias rejected");
      require (alias_input == expected, id + " input alias differs");
      auto alias_shadow = shadow;
      require (filter_replace_inten_row (input.data (), alias_shadow.data (), coverage.data (),
                                        alias_shadow.data (), pixels, channels, active), "Shadow alias rejected");
      require (alias_shadow == expected, id + " shadow alias differs");
      if (empty)
        {
          require (filter_replace_inten_row (input.data (), shadow.data (), nullptr,
                                            output.data (), pixels, channels, active), "Null mask rejected");
          require (output == expected, id + " null mask differs");
        }
      for (std::size_t start = 0; start < pixels; )
        {
          const auto count = std::min<std::size_t> (pixels - start, 1 + start % 43);
          require (filter_replace_inten_row (input.data () + start * channels,
                                            shadow.data () + start * channels,
                                            coverage.data () + start,
                                            output.data () + start * channels,
                                            count, channels, active), "Chunk rejected");
          start += count;
        }
      require (output == expected, id + " bounded chunks differ");
      ++cases;
    }
  require (table.eof () && cases >= 256, "Incomplete genuine old native corpus");
  std::cout << "Exact old native shadow merges: " << cases << '\n';
}

void native_bounds (const std::string& directory)
{
  std::ifstream table (directory + "/bounds.tsv");
  require (table.good (), "Missing genuine old bounds fixtures");
  std::string id;
  std::int32_t width, height, ox, oy, sx1, sy1, sx2, sy2, x1, y1, x2, y2;
  int empty, selected;
  std::size_t cases = 0;
  while (table >> id >> width >> height >> ox >> oy >> empty >> sx1 >> sy1 >> sx2 >> sy2
         >> selected >> x1 >> y1 >> x2 >> y2)
    {
      FilterSelectionRegion region;
      require (filter_selection_region ({ empty != 0, sx1, sy1, sx2, sy2 }, ox, oy,
                                        width, height, region), id + " bounds rejected");
      require (region.selected == (selected != 0) && region.x1 == x1 && region.y1 == y1 &&
               region.x2 == x2 && region.y2 == y2, id + " old mask bounds differ");
      require (region.intersects == (x1 < x2 && y1 < y2), id + " intersection differs");
      ++cases;
    }
  require (table.eof () && cases >= 256, "Incomplete old bounds corpus");
  std::cout << "Exact old native selection bounds: " << cases << '\n';
}

void argument_boundaries ()
{
  std::uint8_t pixel[] = { 1, 2, 3, 4 }, output[] = { 91, 92, 93, 94 };
  require (!filter_replace_inten_row (pixel, pixel, nullptr, output, 1, 0, 15), "Zero channels");
  require (!filter_replace_inten_row (pixel, pixel, nullptr, output, 1, 5, 15), "Five channels");
  require (!filter_replace_inten_row (pixel, pixel, nullptr, output,
                                      std::numeric_limits<std::size_t>::max (), 4, 15), "Size overflow");
  require (!filter_replace_inten_row (nullptr, pixel, nullptr, output, 1, 4, 15), "Null input");
  require (!filter_replace_inten_row (pixel, nullptr, nullptr, output, 1, 4, 15), "Null shadow");
  require (!filter_replace_inten_row (pixel, pixel, nullptr, nullptr, 1, 4, 15), "Null output");
  require (output[0] == 91 && output[1] == 92 && output[2] == 93 && output[3] == 94,
           "Invalid arguments changed output");
  require (filter_replace_inten_row (nullptr, nullptr, nullptr, nullptr, 0, 4, 15), "Empty row");

  FilterSelectionRegion region { true, true, 1, 2, 3, 4 };
  require (!filter_selection_region ({}, 0, 0, 0, 7, region), "Zero width");
  require (!filter_selection_region ({false, 4, 3, 4, 7}, 0, 0, 8, 8, region), "Invalid bounds");
  require (region.x1 == 1 && region.y1 == 2 && region.x2 == 3 && region.y2 == 4,
           "Invalid bounds changed output");
  require (filter_selection_region ({false, INT32_MIN, INT32_MIN, -1, -1},
                                    INT32_MAX, INT32_MAX, 8, 9, region), "Wide offsets rejected");
  require (region.selected && !region.intersects && region.x1 == 0 && region.x2 == 0,
           "Wide offset overflow");
  require (filter_selection_region ({}, INT32_MAX, INT32_MIN, 8, 9, region), "Global empty rejected");
  require (!region.selected && region.intersects && region.x2 == 8 && region.y2 == 9,
           "Global empty selection must allow full drawable");
}

void bounded_mask_discovery ()
{
  FilterSelectionScan scan;
  FilterSelectionBounds result { false, 5, 6, 7, 8 };
  const std::uint8_t first[] = { 0, 0, 0, 1, 0, 0, 0 };
  const std::uint8_t second[] = { 0, 127, 0, 0, 0, 0, 0, 0 };
  require (!scan.finish (result) && !scan.append (0, first, 1), "Uninitialized scan");
  require (!scan.reset (0, 5), "Zero scan width");
  require (scan.reset (5, 3), "Valid scan rejected");
  require (!scan.append (1, first, 7) && scan.consumed () == 0, "Out-of-order scan mutated state");
  require (scan.append (0, first, 7), "First scan chunk");
  require (!scan.finish (result) && result.x1 == 5, "Incomplete scan changed result");
  require (!scan.append (7, nullptr, 8) && scan.consumed () == 7, "Null scan data");
  require (!scan.append (7, second, 9) && scan.consumed () == 7, "Oversized scan");
  require (scan.append (7, second, 8) && scan.finish (result), "Final scan chunk");
  require (!result.empty && result.x1 == 3 && result.y1 == 0 && result.x2 == 4 && result.y2 == 2,
           "Chunk boundary coverage bounds");
  require (scan.reset (5, 3), "Reset selection revision");
  const std::uint8_t zero = 0;
  for (std::uint64_t i = 0; i < 15; ++i)
    require (scan.append (i, &zero, 1), "One-byte mask quantum");
  require (scan.finish (result) && result.empty, "Global empty scan");
}

} // namespace

int main (int argc, char **argv)
{
  try
    {
      require (argc == 2, "Pass the genuine legacy-filter-context fixture directory");
      argument_boundaries ();
      bounded_mask_discovery ();
      native_merges (argv[1]);
      native_bounds (argv[1]);
      std::cout << "Filter context scalar tests passed\n";
      return 0;
    }
  catch (const std::exception& error)
    {
      std::cerr << error.what () << '\n';
      return 1;
    }
}
