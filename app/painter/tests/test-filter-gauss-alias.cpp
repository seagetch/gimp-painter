/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "filter-gauss.hpp"
#include "filter-raster-kernels.hpp"
#include <glib.h>
#include <glib/gstdio.h>
#include <algorithm>
#include <atomic>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace GimpPainter;
using Bytes = std::vector<std::uint8_t>;
namespace {
void require (bool value, const std::string& message)
{ if (!value) throw std::runtime_error (message); }
Bytes read_file (const std::string& path)
{
  std::ifstream input (path,std::ios::binary);
  require (bool (input),"Cannot read fixture: " + path);
  return Bytes (std::istreambuf_iterator<char> (input),{});
}
void compare (const Bytes& rgba, const Bytes& expected, unsigned channels, const std::string& label)
{
  Bytes native (expected.size ());
  for (std::size_t i = 0; i < rgba.size () / 4; ++i)
    {
      if (channels == 2)
        require (rgba[i*4] == rgba[i*4+1] && rgba[i*4] == rgba[i*4+2],"Unequal Gray channels");
      for (unsigned j = 0; j < channels-1; ++j) native[i*channels+j] = rgba[i*4+j];
      native[i*channels+channels-1] = rgba[i*4+3];
    }
  require (native == expected,"Genuine old Gaussian mismatch: " + label);
}
unsigned corpus (const char *manifest_path, const std::string& directory, unsigned& identities)
{
  std::string path (manifest_path), line;
  const auto root = path.substr (0,path.find_last_of ("/\\"));
  std::ifstream manifest (path);
  require (bool (manifest),"Cannot read manifest: " + path);
  unsigned cases = 0;
  while (std::getline (manifest,line))
    {
      if (line.empty () || line[0] == '#') continue;
      std::istringstream row (line);
      std::string procedure, input_path, expected_path;
      std::size_t width, height;
      unsigned channels, options;
      double a, b, c;
      require (bool (row >> procedure >> width >> height >> channels >> options >> a >> b >> c >> input_path >> expected_path),"Invalid fixture row");
      require (width && height && width <= 1024 && height <= 1024 && (channels == 2 || channels == 4),"Invalid fixture geometry");
      const bool flags = procedure == "plug-in-gauss-iir" || procedure == "plug-in-gauss-rle";
      const bool radii = procedure == "plug-in-gauss-iir2" || procedure == "plug-in-gauss-rle2";
      const bool canonical = procedure == "plug-in-gauss";
      require ((flags || radii || canonical) && options == (radii ? 2u : 3u),"Unexpected procedure/options");
      require (!canonical || c == 0 || c == 1,"Invalid canonical method");
      GaussOptions settings {flags ? (b != 0 ? a : 0) : a, flags ? (c != 0 ? a : 0) : b,
        canonical ? int (c) : procedure == "plug-in-gauss-rle" || procedure == "plug-in-gauss-rle2" ? 1 : 0};
      const auto input = read_file (root + "/" + input_path), expected = read_file (root + "/" + expected_path);
      require (input.size () == width*height*channels && input.size () == expected.size (),"Invalid fixture byte count");
      Bytes rgba (width*height*4), result;
      for (std::size_t i = 0; i < width*height; ++i)
        {
          for (unsigned j = 0; j < 3; ++j) rgba[i*4+j] = input[i*channels+(channels == 2 ? 0 : j)];
          rgba[i*4+3] = input[i*channels+channels-1];
        }
      std::atomic<bool> cancel {false};
      const bool identity = flags && settings.horizontal == 0 && settings.vertical == 0;
      if (identity) { require (a > 0,"Invalid single-radius identity"); result = rgba; ++identities; }
      else require (filter_gauss (rgba,width,height,settings,cancel,result),"Unexpected vector cancellation");
      compare (result,expected,channels,expected_path + " vector");
      TemporaryFilterRaster source (directory,rgba.size ()), destination (directory,rgba.size ());
      source.write (0,rgba.size (),rgba.data ()); source.flush ();
      FilterRasterFactory scratch = [&] (std::uint64_t size) {
        return std::unique_ptr<FilterRaster> (new TemporaryFilterRaster (directory,size));
      };
      if (identity) destination.write (0,rgba.size (),rgba.data ());
      else require (filter_gauss_raster (source,width,height,settings,cancel,destination,scratch),"Unexpected raster cancellation");
      destination.flush (); destination.read (0,result.size (),result.data ());
      compare (result,expected,channels,expected_path + " file-backed");
      ++cases;
    }
  return cases;
}
void multi_chunk_regions (const std::string& directory)
{
  const std::size_t width = 257, height = 129;
  Bytes input (width*height*4), expected, result (input.size ());
  for (std::size_t i = 0; i < input.size (); ++i) input[i] = std::uint8_t ((i*71+i/13) & 255);
  for (const auto& radii : {std::pair<double,double> {-2.25,3}, {3,-2.25}, {-200,3}, {3,-200}})
    for (int method : {0,1})
      {
        std::atomic<bool> cancel {false};
        GaussOptions options {radii.first,radii.second,method};
        require (filter_gauss (input,width,height,options,cancel,expected),"Generated region vector failed");
        TemporaryFilterRaster source (directory,input.size ()), output (directory,input.size ());
        source.write (0,input.size (),input.data ()); source.flush ();
        FilterRasterFactory scratch = [&] (std::uint64_t size) {
          return std::unique_ptr<FilterRaster> (new TemporaryFilterRaster (directory,size));
        };
        require (filter_gauss_raster (source,width,height,options,cancel,output,scratch),"Generated region raster failed");
        output.read (0,result.size (),result.data ());
        require (result == expected,"Multi-chunk negative-region parity failed");
      }
  std::cout << "Generated negative-region parity across 64-KiB merge boundaries: 8 passed\n";
}
}
int main (int argc, char **argv)
{
  if (argc != 3) { std::cerr << "Expected alias and negative-region manifests\n"; return 2; }
  gchar *temporary = g_dir_make_tmp ("painter-gauss-alias-XXXXXX",nullptr);
  if (!temporary) return 3;
  const std::string directory (temporary); g_free (temporary);
  int status = 0;
  try
    {
      unsigned identities = 0;
      const auto aliases = corpus (argv[1],directory,identities);
      const auto negative = corpus (argv[2],directory,identities);
      require (aliases == 88 && negative == 112 && identities == 12,"Wrong corpus coverage");
      multi_chunk_regions (directory);
      std::cout << "Matched 200 genuine old native buffers (88 alias + 112 canonical/alias negative regions), vector and file-backed; 12 disabled-axis identities\n";
    }
  catch (const std::exception& error) { std::cerr << error.what () << '\n'; status = 1; }
  if (g_rmdir (directory.c_str ()) != 0) status = 1;
  return status;
}
