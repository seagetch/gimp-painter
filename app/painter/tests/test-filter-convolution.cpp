/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "filter-process.hpp"
#include "filter-wire.hpp"
#include <glib.h>
#include <glib/gstdio.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>
using namespace GimpPainter;
namespace {
struct Raster : FilterRaster {
  std::vector<std::uint8_t> data;
  explicit Raster (std::size_t bytes) : data (bytes) {}
  std::uint64_t size () const noexcept override { return data.size (); }
  void read (std::uint64_t at,std::size_t n,std::uint8_t *p) override {
    if (at > size () || n > size ()-at) throw std::out_of_range ("test read");
    std::memcpy (p,data.data ()+at,n);
  }
  void write (std::uint64_t at,std::size_t n,const std::uint8_t *p) override {
    if (at > size () || n > size ()-at) throw std::out_of_range ("test write");
    std::memcpy (data.data ()+at,p,n);
  }
  void flush () override {}
};
void require (bool condition,const std::string& why) { if (!condition) throw std::runtime_error (why); }
void run (const FilterProcedureRequest& r,Raster& in,Raster& out,const FilterProcessOptions& opts) {
  std::atomic<bool> cancel {false};
  auto result = std::make_shared<FilterProcedureResult> ();
  require (filter_process (r,in,out,cancel,opts,result),"Convolution unexpectedly cancelled");
  require (result->disposition () == (!r.execution_region ().intersects ? FilterProcedureDisposition::no_merge :
    r.raw_shadow ? FilterProcedureDisposition::shadow : FilterProcedureDisposition::merged),"Convolution wrong disposition");
}
}
int main (int argc,char **argv) {
  const bool boundaries_only = argc == 3 && !std::strcmp (argv[2],"--boundaries-only");
  if (argc != 2 && !boundaries_only) return 2;
  gchar *directory = g_dir_make_tmp ("filter-convolution-XXXXXX",nullptr);
  FilterProcessOptions opts {argv[1],directory};
  try {
    const char *path = g_getenv ("GIMP_PAINTER_CONVOLUTION_FIXTURES"); require (path,"Missing sealed Convolution evidence");
    std::ifstream table (std::string (path)+"/cases.tsv"), pixels (std::string (path)+"/pixels.bin",std::ios::binary);
    require (bool (table) && bool (pixels),"Cannot read sealed Convolution evidence");
    const auto buffer = [&] (std::uint64_t at,std::size_t n) {
      std::vector<std::uint8_t> bytes (n); pixels.clear (); pixels.seekg (at);
      if (n) pixels.read (reinterpret_cast<char *> (bytes.data ()),n);
      require (bool (pixels),"Truncated Convolution evidence"); return bytes;
    };
    std::string line; unsigned compared = 0,rejected = 0,shadow_cases = 0;
    while (!boundaries_only && std::getline (table,line)) {
      std::istringstream row (line); std::string id; int status,live,selected;
      FilterProcedureRequest r; r.procedure = FilterProcedure::convolution;
      row >> id >> r.width >> r.height >> r.storage_channels >> status >> live >> selected >>
        r.start_region.x1 >> r.start_region.y1 >> r.start_region.x2 >> r.start_region.y2 >>
        r.alpha_alg >> r.divisor >> r.offset >> r.border;
      r.start_region.selected = selected; r.start_region.intersects =
        r.start_region.x1 < r.start_region.x2 && r.start_region.y1 < r.start_region.y2;
      r.gray = r.storage_channels <= 2;
      for (auto& c : r.channels) row >> c;
      for (auto& c : r.matrix) row >> c;
      std::uint64_t offsets[7]; std::size_t lengths[7];
      for (unsigned i = 0; i < 7; ++i) row >> offsets[i] >> lengths[i];
      std::string extra; require (bool (row) && !(row >> extra),"Malformed Convolution table: "+id);
      if (status != 3) {
        bool failed = false; try { r.bytes (); } catch (const std::invalid_argument&) { failed = true; }
        require (failed,"Old failed geometry admitted: "+id); ++rejected; continue;
      }
      const auto original = buffer (offsets[2],lengths[2]), wanted = buffer (offsets[5],lengths[5]);
      const auto shadow = buffer (offsets[3],lengths[3]), mask = buffer (offsets[4],lengths[4]);
      const auto n = std::size_t (r.width)*r.height; const auto channels = r.storage_channels;
      require (original.size () == n*channels && wanted.size () == original.size () &&
               shadow.size () == original.size () && mask.size () == n,"Bad old Convolution buffers: "+id);
      Raster in (r.bytes ()),out (r.bytes ());
      const bool alpha = channels%2 == 0;
      for (std::size_t p = 0; p < n; ++p) {
        for (unsigned c = 0; c < 3; ++c) in.data[p*4+c] = original[p*channels+(r.gray ? 0 : c)];
        in.data[p*4+3] = alpha ? original[p*channels+channels-1] : 255;
      }
      r.raw_shadow = true; run (r,in,out,opts);
      std::vector<std::uint8_t> actual (original.size ());
      const auto roi = r.execution_region ();
      for (std::size_t p = 0; p < n; ++p) for (unsigned c = 0; c < channels; ++c) {
        const auto value = out.data[p*4+(alpha && c == channels-1 ? 3 : r.gray ? 0 : c)];
        actual[p*channels+c] = value;
        if (p%r.width >= unsigned (roi.x1) && p%r.width < unsigned (roi.x2) &&
            p/r.width >= unsigned (roi.y1) && p/r.width < unsigned (roi.y2))
          require (value == shadow[p*channels+c],"Old raw shadow differs: "+id+" byte="+std::to_string(p*channels+c));
      }
      ++shadow_cases;
      require (filter_replace_inten_row (original.data (),actual.data (),mask.data (),actual.data (),n,channels,(1u<<channels)-1),"Merge rejected");
      require (actual == wanted,"Old final merge differs: "+id);
      ++compared;
    }
    if (!boundaries_only) require (compared == 296 && rejected == 8,"Incomplete old Convolution corpus");
    unsigned native = 0;
    for (unsigned mode : {1u,2u,3u}) for (bool gray : {false,true}) for (bool alpha : {false,true}) {
      if (boundaries_only) continue;
      FilterProcedureRequest r; r.procedure = FilterProcedure::convolution;
      r.width = 9; r.height = 8; r.sample_mode = mode; r.gray = gray;
      r.storage_channels = gray ? (alpha ? 2 : 1) : (alpha ? 4 : 3); r.raw_shadow = true;
      Raster in (r.bytes ()),out (r.bytes ());
      for (std::size_t p = 0; p < 72; ++p) {
        double v[4] = {0.125+std::ldexp(double(p+1),-45),-0.25-double(p)/16,1.5+double(p)/32,alpha ? .25 : 1};
        if (gray) v[1] = v[2] = v[0];
        std::memcpy (in.data.data ()+p*32,v,32);
      }
      run (r,in,out,opts); require (in.data == out.data,"Native identity lost double/HDR components"); ++native;
      r.matrix[12] = 2; r.channels[4] = 0;
      run (r,in,out,opts);
      for (std::size_t p = 0; p < 72; ++p) {
        double before[4],after[4]; std::memcpy (before,in.data.data ()+p*32,32); std::memcpy (after,out.data.data ()+p*32,32);
        for (unsigned c=0;c<4;++c) require (after[c] == before[c]*(c==3 ? 1 : 2),"Native double kernel differs");
      }
      ++native;
      r.matrix[12] = 1; r.start_region = {true,true,5,7,9,8}; r.border = 2;
      run (r,in,out,opts); // safely defined narrow/right-edge extension
      double after[4],before[4]; std::memcpy (before,in.data.data ()+68*32,32); std::memcpy (after,out.data.data ()+68*32,32);
      require (!std::memcmp (before,after,32),"One-row ROI lost native identity"); ++native;
    }
    {
      FilterProcedureRequest r; r.procedure = FilterProcedure::convolution;
      r.width = 9; r.height = 8; r.raw_shadow = true; r.border = 2;
      r.start_region = {true,true,5,7,9,8}; r.matrix.fill (0); r.matrix[24] = 1;
      Raster in (r.bytes ()),out (r.bytes ()); std::fill (in.data.begin (),in.data.end (),255);
      run (r,in,out,opts);
      require (std::all_of (out.data.begin (),out.data.end (),[] (std::uint8_t v) { return v == 0; }),
        "Safe byte CLEAR padding/one-row ROI differs"); ++native;
    }
    for (unsigned storage : {1u,3u}) {
      // Identical 3x3 inputs also captured in the separate genuine-old
      // extreme-coefficient fixture, not an inferred old failure domain.
      FilterProcedureRequest r; r.procedure = FilterProcedure::convolution;
      r.width = 3; r.height = 3; r.gray = storage == 1; r.storage_channels = storage;
      r.raw_shadow = true; r.alpha_alg = 0; r.border = 0;
      r.matrix.fill (0); r.matrix[12] = r.divisor = 3e38;
      Raster in (r.bytes ()),out (r.bytes ());
      for (std::size_t p=0;p<9;++p) {
        for (unsigned c=0;c<3;++c) in.data[p*4+c]=(p+(r.gray ? 0 : c))%2;
        in.data[p*4+3]=255;
      }
      run (r,in,out,opts); require (in.data==out.data,"Defined large-coefficient byte request rejected or changed"); ++native;
      r.offset = 2147483500.0; r.matrix.fill (0); r.divisor = 1;
      run (r,in,out,opts); require (std::all_of (out.data.begin (),out.data.end (),[] (std::uint8_t v) {return v==255;}),
        "Defined near-INT_MAX byte result rejected"); ++native;
    }
    std::cout << compared << " genuine old Convolution final merges, " << shadow_cases << " raw ROI comparisons, "
              << rejected << " old geometry rejections, " << native << " native analytic requests passed\n";
    g_rmdir (directory); g_free (directory); return 0;
  } catch (const std::exception& e) { std::cerr << e.what () << '\n'; g_rmdir (directory); g_free (directory); return 1; }
}
