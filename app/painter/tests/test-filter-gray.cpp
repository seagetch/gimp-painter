/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "filter-edge.hpp"
#include "filter-gauss.hpp"
#include <atomic>
#include <cmath>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace GimpPainter;
namespace {
using Bytes = std::vector<std::uint8_t>;
void require (bool value, const std::string& message)
{ if (!value) throw std::runtime_error (message); }
Bytes read (const std::string& path)
{
  std::ifstream stream (path, std::ios::binary);
  require (bool (stream), "Cannot open " + path);
  Bytes bytes { std::istreambuf_iterator<char> (stream), std::istreambuf_iterator<char> () };
  require (!stream.bad (), "Cannot read " + path);
  return bytes;
}
void compare (const std::string& manifest)
{
  std::ifstream stream (manifest);
  require (bool (stream), "Cannot open manifest " + manifest);
  const auto slash = manifest.find_last_of ("/\\");
  const auto directory = slash == std::string::npos ? "" : manifest.substr (0, slash + 1);
  std::string line;
  unsigned count = 0;
  while (std::getline (stream, line))
    {
      std::istringstream row (line);
      row >> std::ws;
      if (row.eof () || row.peek () == '#') continue;
      std::string procedure, input_name, output_name, trailing;
      std::size_t width, height, channels;
      double a, b;
      int c;
      require (bool (row >> procedure >> width >> height >> channels >> a >> b >> c >> input_name >> output_name)
               && !(row >> trailing), "Invalid fixture row " + line);
      require (width && width <= 65536 && height && height <= 65536 &&
               height <= 64 * 1024 * 1024 / width && (channels == 1 || channels == 2), "Invalid dimensions " + line);
      require (std::isfinite (a) && std::isfinite (b), "Invalid options " + line);
      const auto input = read (directory + input_name), expected = read (directory + output_name);
      const auto pixels = width * height;
      require (input.size () == pixels * channels && expected.size () == input.size (), "Invalid byte count " + line);
      Bytes rgba (pixels * 4), output;
      /* Pure channel replication, never a color/luminance conversion. This
       * checks every native byte captured from the actual 2.8 executables. */
      for (std::size_t i = 0; i < pixels; ++i)
        {
          rgba[i*4] = rgba[i*4+1] = rgba[i*4+2] = input[i*channels];
          rgba[i*4+3] = channels == 2 ? input[i*2+1] : 255;
        }
      std::atomic<bool> cancel { false };
      bool done;
      if (procedure == "plug-in-edge")
        {
          require (b >= 1 && b <= 3 && b == int (b), "Invalid wrap mode " + line);
          done = filter_edge (rgba, width, height, { a, int (b), c }, cancel, output);
        }
      else
        {
          require (procedure == "plug-in-gauss", "Unknown procedure " + line);
          done = filter_gauss (rgba, width, height, { a, b, c }, cancel, output);
        }
      require (done && output.size () == rgba.size (), "Incomplete output " + output_name);
      for (std::size_t i = 0; i < pixels; ++i)
        {
          require (output[i*4] == output[i*4+1] && output[i*4] == output[i*4+2], "Unequal Gray channels " + output_name);
          require (output[i*4] == expected[i*channels] &&
                   output[i*4+3] == (channels == 2 ? expected[i*2+1] : 255),
                   "Legacy native Gray byte mismatch " + output_name + " at pixel " + std::to_string (i));
        }
      ++count;
    }
  require (stream.eof () && count == 77, "Incomplete Gray corpus");
  std::cout << "Legacy executable native Gray/Gray-alpha byte comparisons: " << count << " passed\n";
}
}
int main (int argc, char **argv)
{
  try { require (argc == 2, "Usage: painter-filter-gray fixtures.tsv"); compare (argv[1]); return 0; }
  catch (const std::exception& e) { std::cerr << e.what () << '\n'; return 1; }
}
