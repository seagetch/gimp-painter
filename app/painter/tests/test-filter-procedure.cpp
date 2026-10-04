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
    if (const gchar *fixtures = g_getenv ("GIMP_PAINTER_SMALL_TILES_FIXTURES")) {
      const std::string base = std::string (fixtures) + "/pdb";
      std::ifstream manifest (base + "/fixtures.tsv");
      if (!manifest) throw std::runtime_error ("Cannot read sealed SmallTiles manifest");
      const auto read_bytes = [&] (const std::string& name) {
        std::ifstream file (base + "/" + name,std::ios::binary);
        if (!file) throw std::runtime_error ("Cannot read old SmallTiles raster: " + name);
        return std::vector<std::uint8_t> (std::istreambuf_iterator<char> (file),{});
      };
      std::string line; unsigned compared = 0, failed = 0;
      while (std::getline (manifest,line)) {
        if (line.empty ()) continue;
        std::istringstream columns (line);
        std::string procedure, input_name, output_name, extra;
        unsigned channels;
        FilterProcedureRequest request; request.procedure = FilterProcedure::small_tiles;
        if (!(columns >> procedure >> request.width >> request.height >> channels >> request.tiles >> input_name >> output_name) ||
            columns >> extra || procedure != "plug-in-small-tiles" || channels < 1 || channels > 4)
          throw std::runtime_error ("Invalid sealed SmallTiles manifest row");
        request.gray = channels <= 2;
        const bool alpha = channels == 2 || channels == 4;
        const auto original = read_bytes (input_name), expected = read_bytes (output_name);
        if (original.size () != std::uint64_t (request.width)*request.height*channels || expected.size () != original.size ())
          throw std::runtime_error ("Invalid old SmallTiles byte extent");
        Raster input (request.bytes ()), output (request.bytes ());
        for (std::size_t at = 0,pixel = 0; at < original.size (); at += channels,pixel += 4) {
          input.bytes[pixel] = original[at]; input.bytes[pixel+1] = original[at+(request.gray ? 0 : 1)];
          input.bytes[pixel+2] = original[at+(request.gray ? 0 : 2)];
          input.bytes[pixel+3] = alpha ? original[at+channels-1] : 255;
        }
        std::atomic<bool> cancel {false};
        if (!filter_process (request,input,output,cancel,options)) throw std::runtime_error ("SmallTiles unexpectedly cancelled");
        std::size_t differences = 0;
        for (std::size_t at = 0,pixel = 0; at < expected.size (); at += channels,pixel += 4) {
          for (unsigned c = 0; c < 3; ++c)
            if (output.bytes[pixel+c] != expected[at+(request.gray ? 0 : c)]) ++differences;
          if (output.bytes[pixel+3] != (alpha ? expected[at+channels-1] : 255)) ++differences;
        }
        ++compared;
        if (differences) { ++failed; std::cerr << output_name << ": " << differences << " differing private RGBA bytes\n"; }
      }
      if (compared != 196 || failed)
        throw std::runtime_error (std::to_string (failed)+" of "+std::to_string (compared)+" genuine old SmallTiles cases failed");
      std::cout << compared << " actual legacy SmallTiles oracles byte-exact (RGB/RGBA/Gray/Gray-alpha, factors0..6)\n";
    }
    if (const gchar *fixtures = g_getenv ("GIMP_PAINTER_RETINEX_FIXTURES")) {
      const std::string base = std::string (fixtures) + "/pdb";
      std::ifstream manifest (base + "/fixtures.tsv");
      if (!manifest) throw std::runtime_error ("Cannot read sealed Retinex manifest");
      const auto read_bytes = [&] (const std::string& name) {
        std::ifstream file (base + "/" + name,std::ios::binary);
        if (!file) throw std::runtime_error ("Cannot read old Retinex raster: " + name);
        return std::vector<std::uint8_t> (std::istreambuf_iterator<char> (file),{});
      };
      std::string line;
      unsigned compared = 0, rgb = 0, rgba = 0, failed = 0, selected = 0;
      while (std::getline (manifest,line)) {
        if (line.empty () || line[0] == '#') continue;
        std::istringstream columns (line);
        std::string procedure, input_name, output_name, extra;
        FilterProcedureRequest request; request.procedure = FilterProcedure::retinex;
        if (!(columns >> procedure >> request.width >> request.height >> request.storage_channels >>
              request.scale >> request.nscales >> request.scales_mode >> request.cvar >> input_name >> output_name) ||
            columns >> extra || procedure != "plug-in-retinex" ||
            (request.storage_channels != 3 && request.storage_channels != 4))
          throw std::runtime_error ("Invalid sealed Retinex manifest row");
        const unsigned channels = request.storage_channels;
        const auto original = read_bytes (input_name), expected = read_bytes (output_name);
        if (original.size () != std::uint64_t (request.width)*request.height*channels || expected.size () != original.size ())
          throw std::runtime_error ("Invalid old Retinex byte extent");
        Raster input (request.bytes ()), output (request.bytes ());
        for (std::size_t at = 0,pixel = 0; at < original.size (); at += channels,pixel += 4) {
          std::copy_n (original.data () + at,3,input.bytes.data () + pixel);
          input.bytes[pixel+3] = channels == 4 ? original[at+3] : 255;
        }
        std::atomic<bool> cancel {false};
        auto outcome = std::make_shared<FilterProcedureResult> ();
        if (!filter_process (request,input,output,cancel,options,outcome) ||
            outcome->disposition () != FilterProcedureDisposition::merged)
          throw std::runtime_error ("Retinex did not publish a merged result");
        std::size_t differences = 0;
        for (std::size_t at = 0,pixel = 0; at < expected.size (); at += channels,pixel += 4) {
          for (unsigned c = 0; c < 3; ++c)
            if (output.bytes[pixel+c] != expected[at+c]) ++differences;
          if (output.bytes[pixel+3] != (channels == 4 ? expected[at+3] : 255)) ++differences;
        }
        ++compared; ++(channels == 3 ? rgb : rgba);
        if (differences) { ++failed; std::cerr << output_name << ": " << differences << " differing private Retinex RGBA bytes\n"; }

        if (request.scale == 240 && request.nscales == 3 && request.scales_mode == 0 && request.cvar == 1.2) {
          /* Analytic ROI composition, not another old capture: embed the whole
           * old input in a larger drawable. Retinex processes only the hard ROI,
           * whose dimensions and samples exactly match that original fixture. */
          const auto old_width = request.width, old_height = request.height;
          request.width += 11; request.height += 13;
          request.start_region = {true,true,5,7,std::int32_t (5+old_width),std::int32_t (7+old_height)};
          Raster selected_input (request.bytes ()), selected_output (request.bytes ());
          for (std::size_t pixel = 0; pixel < selected_input.bytes.size (); pixel += 4) {
            selected_input.bytes[pixel] = std::uint8_t (pixel*17+13);
            selected_input.bytes[pixel+1] = std::uint8_t (pixel*31+29);
            selected_input.bytes[pixel+2] = std::uint8_t (pixel*47+67);
            selected_input.bytes[pixel+3] = channels == 4 ? std::uint8_t (pixel/4*13) : 255;
          }
          auto selected_expected = selected_input.bytes;
          for (std::uint32_t y = 0; y < old_height; ++y) for (std::uint32_t x = 0; x < old_width; ++x) {
            const std::size_t source = (std::size_t (y)*old_width+x)*channels;
            const std::size_t target = (std::size_t (y+7)*request.width+x+5)*4;
            std::copy_n (original.data ()+source,3,selected_input.bytes.data ()+target);
            std::copy_n (expected.data ()+source,3,selected_expected.data ()+target);
            selected_input.bytes[target+3] = channels == 4 ? original[source+3] : 255;
            selected_expected[target+3] = channels == 4 ? expected[source+3] : 255;
          }
          if (!filter_process (request,selected_input,selected_output,cancel,options,outcome) ||
              outcome->disposition () != FilterProcedureDisposition::merged || selected_output.bytes != selected_expected)
            throw std::runtime_error ("Analytic padded Retinex ROI differs from old whole-fixture output: " + output_name);
          ++selected;
        }
      }
      if (compared != 174 || rgb != 87 || rgba != 87 || failed || selected != 6)
        throw std::runtime_error (std::to_string (failed)+" of "+std::to_string (compared)+" genuine old Retinex cases failed or coverage changed");
      std::cout << compared << " actual legacy Retinex oracles byte-exact (87 native RGB,87 native RGBA)\n";
      std::cout << selected << " analytic selected Retinex ROI compositions matched old whole-fixture outputs\n";
    }
    unsigned raw_retinex = 0, rejected_carriers = 0;
    for (unsigned channels : {3u,4u}) {
      FilterProcedureRequest request; request.procedure = FilterProcedure::retinex;
      request.width = 39; request.height = 37; request.storage_channels = channels;
      request.nscales = 0; request.raw_shadow = true;
      request.start_region = {true,true,5,7,30,28};
      Raster input (request.bytes ()), output (request.bytes ());
      for (std::size_t pixel = 0; pixel < input.bytes.size (); pixel += 4) {
        input.bytes[pixel] = std::uint8_t (pixel*17+11);
        input.bytes[pixel+1] = std::uint8_t (pixel*31+61);
        input.bytes[pixel+2] = std::uint8_t (pixel*47+137);
        constexpr std::uint8_t alpha[] = {0,1,127,255};
        input.bytes[pixel+3] = channels == 4 ? alpha[pixel/4%4] : 255;
      }
      std::atomic<bool> cancel {false};
      auto outcome = std::make_shared<FilterProcedureResult> ();
      if (!filter_process (request,input,output,cancel,options,outcome) ||
          outcome->disposition () != FilterProcedureDisposition::shadow)
        throw std::runtime_error ("Raw Retinex did not publish a shadow result");
      /* Analytic nscales=0 gives black RGB while retaining input alpha inside
       * the ROI. Cleared native RGB shadow has implicit opaque carrier alpha;
       * cleared RGBA shadow has alpha zero. No merged alpha-zero repair applies. */
      for (std::uint32_t y = 0; y < request.height; ++y) for (std::uint32_t x = 0; x < request.width; ++x) {
        const std::size_t pixel = (std::size_t (y)*request.width+x)*4;
        const bool inside = x >= 5 && x < 30 && y >= 7 && y < 28;
        const std::uint8_t alpha = channels == 3 ? 255 : inside ? input.bytes[pixel+3] : 0;
        if (output.bytes[pixel] || output.bytes[pixel+1] || output.bytes[pixel+2] || output.bytes[pixel+3] != alpha)
          throw std::runtime_error ("Raw Retinex ROI changed native RGB/RGBA shadow bytes or alpha");
      }
      ++raw_retinex;
      if (channels == 3) for (bool raw : {false,true}) for (bool outside_roi : {false,true}) {
        request.raw_shadow = raw;
        const std::size_t pixel = outside_roi ? 0 : (std::size_t (7)*request.width+5)*4;
        input.bytes[pixel+3] = 254;
        std::fill (output.bytes.begin (),output.bytes.end (),0xa5);
        outcome->publish (FilterProcedureDisposition::merged);
        bool rejected = false;
        try { filter_process (request,input,output,cancel,options,outcome); }
        catch (const std::runtime_error&) { rejected = true; }
        input.bytes[pixel+3] = 255;
        if (!rejected || outcome->disposition () != FilterProcedureDisposition::pending ||
            !std::all_of (output.bytes.begin (),output.bytes.end (),[] (std::uint8_t value) { return value == 0xa5; }))
          throw std::runtime_error ("Private Retinex accepted a nonopaque RGB carrier or published failure bytes");
        ++rejected_carriers;
      }
    }
    std::cout << raw_retinex << " analytic native Retinex raw-shadow ROI cases and " << rejected_carriers << " RGB carrier rejection cases passed\n";
    GDir *dir = g_dir_open (directory,0,nullptr); g_assert (dir && !g_dir_read_name (dir)); g_dir_close (dir);
    g_assert (rmdir (directory) == 0); g_free (directory);
    std::cout << passed << " native Blinds identity cases passed\n";
  } catch (const std::exception& error) { std::cerr << error.what () << '\n'; return 1; }
}
