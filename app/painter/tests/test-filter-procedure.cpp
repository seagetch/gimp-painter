/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "filter-process.hpp"
#include <glib.h>
#include <algorithm>
#include <cassert>
#include <iostream>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <unistd.h>
using namespace GimpPainter;
namespace {
struct Raster : FilterRaster {
  std::vector<std::uint8_t> bytes;
  explicit Raster (std::size_t n) : bytes (n) {}
  std::uint64_t size () const noexcept override { return bytes.size (); }
  void read (std::uint64_t at, std::size_t n, std::uint8_t *p) override { g_assert (at+n <= size ()); std::copy_n (bytes.data () + at,n,p); }
  void write (std::uint64_t at, std::size_t n, const std::uint8_t *p) override { g_assert (at+n <= size ()); std::copy_n (p,n,bytes.data () + at); }
  void flush () override {}
};
}
int main (int argc, char **argv) {
  if (argc != 2 && argc != 3) return 2;
  gchar *directory = g_dir_make_tmp ("filter-procedure-test-XXXXXX", nullptr); g_assert (directory);
  FilterProcessOptions options {argv[1],directory};
  try {
    unsigned passed = 0;
    for (bool gray : {false,true}) for (int direction : {0,1}) for (int transparent : {0,1,-1}) {
      FilterProcedureRequest request; request.width = 36; request.height = 18;
      request.gray = gray; request.orientation = direction; request.transparent = transparent;
      request.angle = 0; request.segments = 3; request.background = {{20,20,20,255}};
      Raster input (request.bytes ()), output (request.bytes ());
      for (std::size_t i = 0; i < input.size (); i += 4) {
        input.bytes[i] = std::uint8_t (i * 17); input.bytes[i+1] = gray ? input.bytes[i] : std::uint8_t (i*31);
        input.bytes[i+2] = gray ? input.bytes[i] : std::uint8_t (i*47); input.bytes[i+3] = std::uint8_t (i/4*13);
      }
      std::atomic<bool> cancel {false};
      if (!filter_process (request,input,output,cancel,options)) throw std::runtime_error ("Native Filter cancelled");
      if (output.bytes != input.bytes) throw std::runtime_error ("Native Blinds identity differs");
      ++passed;
    }
    if (argc == 3) {
      std::ifstream manifest (argv[2]); if (!manifest) throw std::runtime_error ("Cannot read legacy Blinds manifest");
      gchar *parent = g_path_get_dirname (argv[2]); const std::string base (parent); g_free (parent);
      std::string line; unsigned compared = 0, failed = 0;
      const auto read_bytes = [&] (const std::string& name) {
        std::ifstream file (base + "/" + name, std::ios::binary);
        if (!file) throw std::runtime_error ("Cannot read legacy Blinds raster: " + name);
        return std::vector<std::uint8_t> (std::istreambuf_iterator<char> (file), {});
      };
      while (std::getline (manifest,line)) {
        if (line.empty () || line[0] == '#') continue;
        std::istringstream columns (line); std::string name, input_name, output_name, extra;
        FilterProcedureRequest request; unsigned channels; int r, g, b;
        if (!(columns >> name >> request.width >> request.height >> channels >> request.angle >> request.segments >>
              request.orientation >> request.transparent >> r >> g >> b >> input_name >> output_name) || columns >> extra ||
            name != "plug-in-blinds" || (channels != 2 && channels != 4))
          throw std::runtime_error ("Invalid legacy Blinds manifest row");
        request.gray = channels == 2; request.background = {{std::uint8_t (r),std::uint8_t (g),std::uint8_t (b),255}};
        const auto original = read_bytes (input_name), expected = read_bytes (output_name);
        if (original.size () != std::uint64_t (request.width) * request.height * channels || expected.size () != original.size ())
          throw std::runtime_error ("Invalid legacy Blinds oracle byte size");
        Raster input (request.bytes ()), output (request.bytes ());
        for (std::size_t at = 0, pixel = 0; at < original.size (); at += channels, pixel += 4) {
          input.bytes[pixel] = original[at]; input.bytes[pixel+1] = original[at + (request.gray ? 0 : 1)];
          input.bytes[pixel+2] = original[at + (request.gray ? 0 : 2)]; input.bytes[pixel+3] = original[at+channels-1];
        }
        std::atomic<bool> cancel {false};
        if (!filter_process (request,input,output,cancel,options)) throw std::runtime_error ("Native Filter cancelled");
        std::size_t differences = 0;
        for (std::size_t at = 0, pixel = 0; at < expected.size (); at += channels, pixel += 4)
          for (unsigned c = 0; c < 4; ++c)
            if (output.bytes[pixel+c] != expected[at + (c == 3 ? channels-1 : request.gray ? 0 : c)]) ++differences;
        ++compared; if (differences) { ++failed; std::cerr << output_name << ": " << differences << " differing RGBA bytes\n"; }
      }
      if (compared != 320 || failed) throw std::runtime_error (std::to_string (failed) + " of " + std::to_string (compared) + " legacy Blinds cases failed");
      std::cout << compared << " actual legacy Blinds oracles byte-exact\n";
    }
    GDir *dir = g_dir_open (directory,0,nullptr); g_assert (dir && !g_dir_read_name (dir)); g_dir_close (dir);
    g_assert (rmdir (directory) == 0); g_free (directory);
    std::cout << passed << " native Blinds identity cases passed\n";
  } catch (const std::exception& error) { std::cerr << error.what () << '\n'; return 1; }
}
