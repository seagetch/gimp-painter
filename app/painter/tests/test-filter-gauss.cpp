/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "filter-gauss.hpp"

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
           GaussOptions options)
{
  std::atomic<bool> cancel { false };
  Bytes output { 201, 202 };
  require (filter_gauss (input, width, height, options, cancel, output),
           "Unexpected cancellation");
  require (output.size () == input.size (), "Wrong output byte count");
  return output;
}

void small_goldens ()
{
  /* Captured using real legacy plug-in-gauss at
   * afa43fae3e920210146abed514f136fd49f671b5. See the committed raw PNG/RGBA
   * outputs and provenance in migration/fixtures/legacy-gauss. These are
   * executable-derived goldens, not expectations regenerated from this port. */
  const double radii[8][2] =
  { { 25, 25 }, { 2, 2 }, { 2.5, 7.25 }, { 0, 25 },
    { 25, 0 }, { 0.25, 0.75 }, { 1, 5 }, { 2, 3 } };
  const std::uint64_t expected[8][2] =
  {
    { 0x29d82e869a58334cULL, 0xe9cd66c9245ccff0ULL },
    { 0x3e77f8ad23c4ff8fULL, 0x99983ab504a35230ULL },
    { 0xa1be6bc61de4bfc4ULL, 0xce00100b62fe0116ULL },
    { 0xde7542f7eed5848fULL, 0xde7542f7eed5848fULL },
    { 0x7713da012798eae2ULL, 0x7713da012798eae2ULL },
    { 0xb223a04fa481402fULL, 0xb223a04fa481402fULL },
    { 0x98c0127e9a25a59eULL, 0x98c0127e9a25a59eULL },
    { 0xca2c6c60487d1adeULL, 0xe4e50339b572adaeULL }
  };
  const auto input = fixture (5, 4);
  for (int radius = 0; radius < 8; ++radius)
    for (int method = 0; method < 2; ++method)
      require (hash_bytes (run (input, 5, 4,
                 { radii[radius][0], radii[radius][1], method })) ==
               expected[radius][method], "Legacy Gaussian small golden mismatch");

  const Bytes constant = [] {
    Bytes b;
    for (int i = 0; i < 72; ++i)
      b.insert (b.end (), { 83, 111, 237, 127 });
    return b;
  } ();
  require (hash_bytes (run (constant, 9, 8, { 2, 2, 1 })) ==
           0xa907b5dd814c39a5ULL, "Legacy encoded-RLE golden mismatch");
}

void axis_fallback_and_alpha ()
{
  const auto input = fixture (9, 8);
  for (const double disabled : { -17.0, -0.5, 0.0 })
    {
      require (run (input, 9, 8, { disabled, 25, 0 }) ==
               run (input, 9, 8, { 0, 25, 1 }),
               "Nonpositive horizontal axis did not select vertical RLE");
      require (run (input, 9, 8, { 25, disabled, 0 }) ==
               run (input, 9, 8, { 25, 0, 1 }),
               "Nonpositive vertical axis did not select horizontal RLE");
    }
  for (double small : { 0.25, 0.75, 1.0 })
    {
      require (run (input, 9, 8, { small, 25, 0 }) ==
               run (input, 9, 8, { small, 25, 1 }), "Missing RLE fallback");
      require (run (input, 9, 8, { 25, small, 0 }) ==
               run (input, 9, 8, { 25, small, 1 }), "Missing RLE fallback");
    }
  require (run (input, 9, 8, { 25, 25, 0 }) !=
           run (input, 9, 8, { 25, 25, 1 }), "IIR was replaced by RLE");

  auto transparent = input;
  for (std::size_t i = 3; i < transparent.size (); i += 4)
    transparent[i] = 0;
  for (int method : { 0, 1 })
    require (run (transparent, 9, 8, { 25, 25, method }) == transparent,
             "Zero-alpha shadow merge lost hidden original RGB");

  /* Unlike edge, Gaussian blurs alpha; it must premultiply source RGB too. */
  const Bytes hidden_red { 255, 0, 0, 0, 0, 0, 255, 255 };
  const auto result = run (hidden_red, 2, 1, { 2, 2, 1 });
  require (result[3] != 0 && result[7] != 255, "Alpha was not blurred");
  require (result[0] == 0 && result[4] == 0,
           "Hidden source red leaked through premultiplication");
}

void validation_and_aliasing ()
{
  const Bytes input { 3, 5, 9, 127 };
  const Bytes sentinel { 99, 100, 101 };
  Bytes output = sentinel;
  std::atomic<bool> cancel { false };
  auto invalid = [&] (const Bytes& bytes, std::size_t width,
                     std::size_t height, GaussOptions options)
    {
      bool caught = false;
      try { filter_gauss (bytes, width, height, options, cancel, output); }
      catch (const std::invalid_argument&) { caught = true; }
      require (caught, "Invalid argument was accepted");
      require (output == sentinel, "Invalid call changed output");
    };
  invalid (input, 0, 1, {});
  invalid (input, 1, 0, {});
  invalid (input, std::numeric_limits<std::size_t>::max (), 1, {});
  invalid (input, 1, std::numeric_limits<std::size_t>::max (), {});
  invalid ({}, 1, 1, {});
  invalid ({ 0, 0, 0 }, 1, 1, {});
  invalid ({ 0, 0, 0, 0, 0 }, 1, 1, {});
  invalid (input, 1, 1, { 0, 0, 0 });
  invalid (input, 1, 1, { -1, -2, 0 });
  invalid (input, 1, 1, { 25, 25, -1 });
  invalid (input, 1, 1, { 25, 25, 2 });
  for (double nonfinite : { std::numeric_limits<double>::quiet_NaN (),
                           std::numeric_limits<double>::infinity (),
                           -std::numeric_limits<double>::infinity () })
    {
      invalid (input, 1, 1, { nonfinite, 2, 0 });
      invalid (input, 1, 1, { 2, nonfinite, 1 });
    }
  invalid (input, 1, 1, { std::numeric_limits<double>::max (), 2, 0 });
  invalid (input, 1, 1, { 2, std::numeric_limits<double>::max (), 1 });
  invalid (input, 1, 1, { 50000, 2, 1 }); // legacy i*i and accumulator overflow
  invalid (input, 1, 1, { 45000, 2, 1 }); // defined curve, overflowing accumulator

  auto alias = fixture (5, 4);
  const auto expected = run (alias, 5, 4, { 25, 25, 0 });
  require (filter_gauss (alias, 5, 4, { 25, 25, 0 }, cancel, alias),
           "Aliased execution was cancelled");
  require (alias == expected, "Aliased input/output changed results");
}

void cancellation ()
{
  const auto input = fixture (5, 4);
  const Bytes sentinel { 99, 100, 101 };
  Bytes output = sentinel;
  std::atomic<bool> cancel { true };
  require (!filter_gauss (input, 5, 4, {}, cancel, output),
           "Pre-cancelled execution succeeded");
  require (output == sentinel, "Cancellation published partial output");
  auto alias = input;
  require (!filter_gauss (alias, 5, 4, {}, cancel, alias),
           "Pre-cancelled aliased execution succeeded");
  require (alias == input, "Cancellation changed aliased input");

  /* A scheduling race is allowed to complete first. Partial output is never
   * allowed; correctness is not defined by a machine-dependent time limit. */
  const auto large = fixture (1024, 1024);
  for (int method : { 0, 1 })
    {
      output = sentinel;
      cancel.store (false);
      std::atomic<bool> started { false };
      bool complete = false;
      std::exception_ptr error;
      std::thread worker ([&] {
        started.store (true);
        try { complete = filter_gauss (large, 1024, 1024,
                                        { 25, 25, method }, cancel, output); }
        catch (...) { error = std::current_exception (); }
      });
      while (!started.load ())
        std::this_thread::yield ();
      std::this_thread::sleep_for (std::chrono::milliseconds (5));
      cancel.store (true);
      worker.join ();
      if (error)
        std::rethrow_exception (error);
      if (!complete)
        require (output == sentinel, "Worker cancellation published partial output");
      else
        require (output == run (large, 1024, 1024, { 25, 25, method }),
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
  /* Whitespace-separated rows: width height horizontal vertical method
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
      GaussOptions options;
      std::string input_name, expected_name, trailing;
      if (!(row >> width >> height >> options.horizontal >> options.vertical >>
            options.method >> input_name >> expected_name) || row >> trailing)
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
      require (argc <= 2, "Usage: painter-filter-gauss [legacy-manifest.tsv]");
      small_goldens ();
      axis_fallback_and_alpha ();
      validation_and_aliasing ();
      cancellation ();
      if (argc == 2)
        runtime_goldens (argv[1]);
      std::cout << "Gaussian compatibility unit tests passed\n";
      return 0;
    }
  catch (const std::exception& error)
    {
      std::cerr << error.what () << '\n';
      return 1;
    }
}
