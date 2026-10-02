/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "filter-edge.hpp"

#include <atomic>
#include <chrono>
#include <exception>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using namespace GimpPainter;

namespace {
using Bytes = std::vector<std::uint8_t>;

void require (bool condition, const char *message)
{
  if (!condition)
    throw std::runtime_error (message);
}

std::uint64_t hash_bytes (const Bytes& bytes)
{
  std::uint64_t hash = UINT64_C (14695981039346656037);
  for (auto byte : bytes)
    hash = (hash ^ byte) * UINT64_C (1099511628211);
  return hash;
}

Bytes fixture (std::size_t width, std::size_t height)
{
  const std::uint8_t alpha[] = { 0, 1, 63, 127, 191, 254, 255 };
  Bytes bytes;
  for (std::size_t y = 0; y < height; ++y)
    for (std::size_t x = 0; x < width; ++x)
      {
        bytes.push_back ((x * 7 + y * 13 + 3) % 47);
        bytes.push_back ((x * 3 + y * 11 + 5) % 41);
        bytes.push_back ((x * 17 + y * 5 + 9) % 43);
        bytes.push_back (alpha[(x + 3 * y) % 7]);
      }
  return bytes;
}

Bytes run (const Bytes& input, std::size_t width, std::size_t height,
           EdgeOptions options)
{
  std::atomic<bool> cancel { false };
  Bytes output { 201, 202 };
  require (filter_edge (input, width, height, options, cancel, output),
           "Unexpected cancellation");
  require (output.size () == input.size (), "Wrong output byte count");
  for (std::size_t i = 3; i < input.size (); i += 4)
    require (output[i] == input[i], "Alpha byte was filtered");
  return output;
}

void small_goldens ()
{
  /* Goldens captured from real legacy plug-in-edge execution at
   * afa43fae3e920210146abed514f136fd49f671b5, over fixture(5,4), amount1.75.
   * Raw inputs/outputs and capture provenance are preserved in
   * migration/fixtures/legacy-edge. edge.c SHA256:
   * 38512426b7145167e7e0b220164bd9cecee70f9b754276ad0d006adc1a18234b.
   * The whole-drawable, unselected RGBA8 PDB operation is covered, including
   * hidden RGB retention by the shadow merge, but not FilterLayer scheduling.
   * Optional runtime goldens below compare all captured bytes directly. */
  const std::uint64_t expected[6][3] =
  {
    { 0x069141ec77d88478ULL, 0x6a39a7e4fd555917ULL, 0x24cc69eb01782ceeULL },
    { 0x70b1cc19aec88438ULL, 0x81e1f2471585fac9ULL, 0xc107975aeda0333fULL },
    { 0x70f62b60a8187e62ULL, 0xaf8f889a1e1a65b9ULL, 0xd857b299e8545c2fULL },
    { 0x849d97cd76e52c24ULL, 0xd3013983596698b2ULL, 0x7a3ef06610355249ULL },
    { 0x11803cdfee67c9ebULL, 0x1615cdb52c9cd3e5ULL, 0x49cc700d2c3dc388ULL },
    { 0xcf33bee719f21929ULL, 0x8161f7dd01003242ULL, 0x2275934381be7583ULL }
  };
  const auto input = fixture (5, 4);
  for (int mode = 0; mode < 6; ++mode)
    for (int wrap = 1; wrap <= 3; ++wrap)
      require (hash_bytes (run (input, 5, 4, { 1.75, wrap, mode })) ==
               expected[mode][wrap - 1], "Legacy small golden mismatch");
}

void dimensions_and_borders ()
{
  for (const auto width : { 1U, 5U })
    for (const auto height : { 1U, 5U })
      {
        Bytes constant (width * height * 4, 17);
        for (int mode = 0; mode < 6; ++mode)
          for (int wrap = 1; wrap <= 2; ++wrap)
            {
              auto output = run (constant, width, height, { 2.0, wrap, mode });
              for (std::size_t i = 0; i < output.size (); ++i)
                require (output[i] == (i % 4 == 3 ? 17 : 0),
                         "Constant image has nonzero wrapped/smeared edge");
            }
      }

  const Bytes one { 3, 5, 9, 127 };
  const std::uint8_t expected[6][3] =
  {
    { 0, 0, 0 }, { 0, 0, 0 }, { 16, 28, 50 },
    { 12, 20, 36 }, { 8, 14, 25 }, { 0, 0, 0 }
  };
  for (int mode = 0; mode < 6; ++mode)
    {
      const auto output = run (one, 1, 1, { 1.0, 3, mode });
      for (int channel = 0; channel < 3; ++channel)
        require (output[channel] == expected[mode][channel],
                 "Single-pixel black border mismatch");
    }
}

void amount_and_clipping ()
{
  const auto input = fixture (5, 4);
  for (int mode = 0; mode < 6; ++mode)
    for (int wrap = 1; wrap <= 3; ++wrap)
      {
        const auto expected = run (input, 5, 4, { 1.0, wrap, mode });
        for (double amount : { -17.0, 0.0, 0.5 })
          require (run (input, 5, 4, { amount, wrap, mode }) == expected,
                   "Legacy minimum amount clamp differs");
        /* This must stay defined even when the mathematical intermediate
         * exceeds double or int range; the legacy C cast was undefined. */
        run (input, 5, 4,
             { std::numeric_limits<double>::max (), wrap, mode });
      }

  auto bright_center = Bytes (3 * 3 * 4, 0);
  for (std::size_t i = 3; i < bright_center.size (); i += 4)
    bright_center[i] = 255;
  bright_center[16] = 255;
  bright_center[17] = 1;
  bright_center[18] = 128;
  const auto dark = run (bright_center, 3, 3, { 1.0, 2, 5 });
  require (dark[16] == 0 && dark[17] == 0 && dark[18] == 0,
           "Negative Laplace response was not clipped");
  const auto bright = run (bright_center, 3, 3, { 1.75, 2, 5 });
  require (bright[0] == 255 && bright[1] == 1 && bright[2] == 224,
           "Laplace saturation or truncation differs");
}

void transparent_rgb ()
{
  const Bytes hidden { 33, 71, 109, 0 };
  for (int mode = 0; mode < 6; ++mode)
    for (int wrap = 1; wrap <= 3; ++wrap)
      require (run (hidden, 1, 1, { 1.75, wrap, mode }) == hidden,
               "Legacy shadow merge did not retain hidden RGB");

  /* The transparent pixel is itself preserved, but its hidden red must
   * contribute to the Laplace result at the opaque neighbor. */
  const Bytes input { 17, 0, 0, 0, 0, 0, 0, 255 };
  const auto output = run (input, 2, 1, { 1.0, 2, 5 });
  require (output == Bytes ({ 17, 0, 0, 0, 51, 0, 0, 255 }),
           "Transparent neighbor RGB was premultiplied or ignored");
}

void validation_and_aliasing ()
{
  const Bytes input { 3, 5, 9, 127 };
  const Bytes sentinel { 99, 100, 101 };
  Bytes output = sentinel;
  std::atomic<bool> cancel { false };
  auto invalid = [&] (const Bytes& bytes, std::size_t width,
                     std::size_t height, EdgeOptions options)
    {
      bool caught = false;
      try { filter_edge (bytes, width, height, options, cancel, output); }
      catch (const std::invalid_argument&) { caught = true; }
      require (caught, "Invalid argument was accepted");
      require (output == sentinel, "Invalid call changed output");
    };

  invalid (input, 0, 1, {});
  invalid (input, 1, 0, {});
  invalid (input, std::numeric_limits<std::size_t>::max (), 1, {});
  invalid (input, 1, std::numeric_limits<std::size_t>::max (), {});
  invalid (input, std::numeric_limits<std::size_t>::max () / 4, 2, {});
  invalid ({}, 1, 1, {});
  invalid ({ 0, 0, 0 }, 1, 1, {});
  invalid ({ 0, 0, 0, 0, 0 }, 1, 1, {});
  invalid (input, 1, 1, { 2.0, 0, 0 });
  invalid (input, 1, 1, { 2.0, 4, 0 });
  invalid (input, 1, 1, { 2.0, 2, -1 });
  invalid (input, 1, 1, { 2.0, 2, 6 });
  invalid (input, 1, 1, { std::numeric_limits<double>::quiet_NaN (), 2, 0 });
  invalid (input, 1, 1, { std::numeric_limits<double>::infinity (), 2, 0 });
  invalid (input, 1, 1, { -std::numeric_limits<double>::infinity (), 2, 0 });

  auto alias = fixture (5, 4);
  const auto expected = run (alias, 5, 4, { 1.75, 1, 4 });
  require (filter_edge (alias, 5, 4, { 1.75, 1, 4 }, cancel, alias),
           "Aliased execution was cancelled");
  require (alias == expected, "Aliased input/output changed results");
}

void cancellation ()
{
  const auto input = fixture (5, 4);
  const Bytes sentinel { 99, 100, 101 };
  Bytes output = sentinel;
  std::atomic<bool> cancel { true };
  require (!filter_edge (input, 5, 4, {}, cancel, output),
           "Pre-cancelled execution succeeded");
  require (output == sentinel, "Cancellation published partial output");

  auto alias = input;
  require (!filter_edge (alias, 5, 4, {}, cancel, alias),
           "Pre-cancelled aliased execution succeeded");
  require (alias == input, "Cancellation changed aliased input");

  /* Exercise a worker-thread cancellation race. Either the completed result
   * or the unchanged sentinel is legal if the worker wins the race. No time
   * threshold determines correctness, making this independent of CPU speed. */
  const Bytes large (1024 * 1024 * 4, 17);
  std::atomic<bool> started { false };
  cancel.store (false);
  bool complete = false;
  std::exception_ptr error;
  std::thread worker ([&] {
    started.store (true);
    try { complete = filter_edge (large, 1024, 1024, {}, cancel, output); }
    catch (...) { error = std::current_exception (); }
  });
  while (!started.load ())
    std::this_thread::yield ();
  std::this_thread::sleep_for (std::chrono::milliseconds (1));
  cancel.store (true);
  worker.join ();
  if (error)
    std::rethrow_exception (error);
  if (!complete)
    require (output == sentinel, "Worker cancellation published partial output");
  else
    {
      require (output.size () == large.size (), "Completed worker output truncated");
      for (std::size_t i = 0; i < output.size (); ++i)
        require (output[i] == (i % 4 == 3 ? 17 : 0),
                 "Completed worker output inconsistent");
    }
}

Bytes read_bytes (const std::string& path)
{
  std::ifstream stream (path, std::ios::binary);
  if (!stream)
    throw std::runtime_error ("Cannot open fixture: " + path);
  Bytes bytes ((std::istreambuf_iterator<char> (stream)),
                std::istreambuf_iterator<char> ());
  if (stream.bad ())
    throw std::runtime_error ("Cannot read fixture: " + path);
  return bytes;
}

void runtime_goldens (const std::string& manifest)
{
  /* Whitespace-separated rows: width height amount wrapmode edgemode
   * input.rgba expected.rgba. Paths are relative to the manifest directory.
   * Lines starting with # and empty lines are ignored. No GIMP/PNG library
   * dependency is required to compare every captured byte. */
  std::ifstream stream (manifest);
  if (!stream)
    throw std::runtime_error ("Cannot open legacy fixture manifest: " + manifest);
  const auto slash = manifest.find_last_of ("/\\");
  const auto directory = slash == std::string::npos ? "" : manifest.substr (0, slash + 1);
  std::string line;
  unsigned int count = 0;
  while (std::getline (stream, line))
    {
      std::istringstream row (line);
      row >> std::ws;
      if (row.eof () || row.peek () == '#')
        continue;
      std::size_t width, height;
      EdgeOptions options;
      std::string input_name, expected_name, trailing;
      if (!(row >> width >> height >> options.amount >> options.wrapmode >>
            options.edgemode >> input_name >> expected_name) || row >> trailing)
        throw std::runtime_error ("Invalid legacy fixture row: " + line);
      const auto input = read_bytes (directory + input_name);
      const auto expected = read_bytes (directory + expected_name);
      if (run (input, width, height, options) != expected)
        throw std::runtime_error ("Legacy runtime golden mismatch: " + expected_name);
      ++count;
    }
  require (stream.eof (), "Cannot read legacy fixture manifest");
  require (count > 0, "Legacy runtime manifest had no cases");
  std::cout << "Legacy executable byte comparisons: " << count << " passed\n";
}
} // namespace

int main (int argc, char **argv)
{
  try
    {
      require (argc <= 2, "Usage: painter-filter-edge [legacy-manifest.tsv]");
      small_goldens ();
      dimensions_and_borders ();
      amount_and_clipping ();
      transparent_rgb ();
      validation_and_aliasing ();
      cancellation ();
      if (argc == 2)
        runtime_goldens (argv[1]);
      std::cout << "Edge compatibility unit tests passed\n";
      return 0;
    }
  catch (const std::exception& error)
    {
      std::cerr << error.what () << '\n';
      return 1;
    }
}
