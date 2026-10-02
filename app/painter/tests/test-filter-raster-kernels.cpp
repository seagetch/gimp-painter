/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "filter-raster-kernels.hpp"
#include <glib.h>
#include <glib/gstdio.h>
#include <algorithm>
#include <array>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace GimpPainter;
namespace {
using Bytes = std::vector<std::uint8_t>;
std::string directory;
void require (bool value, const std::string& why)
{ if (!value) throw std::runtime_error (why); }
template<class Error, class Action> void rejects (Action action, const char *why)
{
  bool rejected = false;
  try { action (); } catch (const Error&) { rejected = true; }
  require (rejected, why);
}
struct Stats
{
  std::size_t events = 0, cancel_at = 0, fail_at = 0, reads = 0, writes = 0;
  std::size_t max_io = 0, created = 0, live = 0, peak_live = 0;
  std::uint64_t disk_bytes = 0, peak_disk_bytes = 0, max_offset = 0;
  std::atomic<bool> *cancel = nullptr;
  void event ()
  {
    ++events;
    if (fail_at && events == fail_at) throw std::runtime_error ("Injected raster I/O/factory failure");
    if (cancel && cancel_at && events == cancel_at) cancel->store (true);
  }
};
class ObservedRaster final : public FilterRaster
{
  TemporaryFilterRaster raster;
  Stats& stats;
  bool scratch;
public:
  ObservedRaster (std::uint64_t size, Stats& stats, bool scratch = false)
    : raster (directory,size), stats (stats), scratch (scratch)
  {
    if (scratch)
      {
        ++stats.created; ++stats.live;
        stats.peak_live = std::max (stats.peak_live, stats.live);
        stats.disk_bytes += size;
        stats.peak_disk_bytes = std::max (stats.peak_disk_bytes, stats.disk_bytes);
      }
  }
  ~ObservedRaster () override
  { if (scratch) { --stats.live; stats.disk_bytes -= size (); } }
  std::uint64_t size () const noexcept override { return raster.size (); }
  void read (std::uint64_t offset, std::size_t count, std::uint8_t *bytes) override
  {
    stats.event (); ++stats.reads; stats.max_io = std::max (stats.max_io,count);
    stats.max_offset = std::max (stats.max_offset,offset);
    raster.read (offset,count,bytes);
  }
  void write (std::uint64_t offset, std::size_t count, const std::uint8_t *bytes) override
  {
    stats.event (); ++stats.writes; stats.max_io = std::max (stats.max_io,count);
    stats.max_offset = std::max (stats.max_offset,offset);
    raster.write (offset,count,bytes);
  }
  void flush () override { stats.event (); raster.flush (); }
};
FilterRasterFactory factory (Stats& stats)
{
  return [&stats] (std::uint64_t size) {
    stats.event ();
    return std::unique_ptr<FilterRaster> (new ObservedRaster (size,stats,true));
  };
}
Bytes read_file (const std::string& path)
{
  std::ifstream stream (path,std::ios::binary);
  require (bool (stream), "Cannot open " + path);
  Bytes bytes {std::istreambuf_iterator<char> (stream),std::istreambuf_iterator<char> ()};
  require (!stream.bad (), "Cannot read " + path);
  return bytes;
}
Bytes fixture (std::size_t width, std::size_t height)
{
  Bytes bytes (width*height*4);
  const std::uint8_t alpha[] = {0,1,63,127,191,254,255};
  for (std::size_t y = 0; y < height; ++y)
    for (std::size_t x = 0; x < width; ++x)
      {
        const auto i = (y*width+x)*4;
        bytes[i] = (x*7+y*13+3)%256; bytes[i+1] = (x*3+y*11+5)%256;
        bytes[i+2] = (x*17+y*5+9)%256; bytes[i+3] = alpha[(x+3*y)%7];
      }
  return bytes;
}
Bytes run (const Bytes& bytes, std::size_t w, std::size_t h,
           const EdgeOptions *edge, const GaussOptions *gauss)
{
  Stats stats;
  ObservedRaster input (bytes.size (),stats), output (bytes.size (),stats);
  input.write (0,bytes.size (),bytes.data ());
  std::atomic<bool> cancel {false};
  const bool done = edge ? filter_edge_raster (input,w,h,*edge,cancel,output)
                         : filter_gauss_raster (input,w,h,*gauss,cancel,output,factory (stats));
  require (done, "Unexpected raster cancellation");
  require (!stats.live && stats.created == ((!edge && gauss->vertical > 0) ? 1U : 0U),
           "Wrong scratch lifetime/count");
  require (stats.peak_disk_bytes <= bytes.size (), "More than one scratch raster");
  Bytes result (bytes.size ()), unchanged (bytes.size ());
  output.read (0,result.size (),result.data ()); input.read (0,unchanged.size (),unchanged.data ());
  require (unchanged == bytes,"Raster kernel changed input");
  return result;
}
void rgb_corpus (const std::string& manifest, bool edge, unsigned expected_count)
{
  std::ifstream stream (manifest);
  require (bool (stream),"Cannot open " + manifest);
  const auto directory = manifest.substr (0,manifest.find_last_of ("/\\")+1);
  std::string line;
  unsigned count = 0;
  while (std::getline (stream,line))
    {
      std::istringstream row (line); row >> std::ws;
      if (row.eof () || row.peek () == '#') continue;
      std::size_t w,h; double a,b; int c; std::string in,out,trailing;
      require (bool (row >> w >> h >> a >> b >> c >> in >> out) && !(row >> trailing),"Invalid corpus row");
      const auto input = read_file (directory+in), expected = read_file (directory+out);
      EdgeOptions eo {a,int (b),c}; GaussOptions go {a,b,c};
      const auto result = run (input,w,h,edge ? &eo : nullptr,edge ? nullptr : &go);
      require (result == expected,"Disk-backed old runtime byte mismatch: " + out);
      Bytes vector; std::atomic<bool> cancel {false};
      require (edge ? filter_edge (input,w,h,eo,cancel,vector) : filter_gauss (input,w,h,go,cancel,vector),
               "Vector unexpectedly cancelled");
      require (result == vector,"Raster/vector mismatch: " + out); ++count;
    }
  require (stream.eof () && count == expected_count,"Incomplete RGB corpus");
  std::cout << (edge ? "Edge" : "Gaussian") << " genuine old runtime + vector + disk byte comparisons: " << count << " passed\n";
}
void gray_corpus (const std::string& manifest)
{
  std::ifstream stream (manifest); require (bool (stream),"Cannot open " + manifest);
  const auto directory = manifest.substr (0,manifest.find_last_of ("/\\")+1);
  std::string line; unsigned count = 0;
  while (std::getline (stream,line))
    {
      std::istringstream row (line); row >> std::ws;
      if (row.eof () || row.peek () == '#') continue;
      std::string procedure,in,out,trailing; std::size_t w,h,channels; double a,b; int c;
      require (bool (row >> procedure >> w >> h >> channels >> a >> b >> c >> in >> out) && !(row >> trailing),"Invalid Gray row");
      require (channels == 1 || channels == 2,"Invalid Gray channels");
      const auto native = read_file (directory+in), expected_native = read_file (directory+out);
      require (native.size () == w*h*channels && expected_native.size () == native.size (),"Invalid Gray corpus size");
      Bytes input (w*h*4), expected (input.size ());
      for (std::size_t i = 0; i < w*h; ++i)
        {
          input[i*4] = input[i*4+1] = input[i*4+2] = native[i*channels];
          input[i*4+3] = channels == 2 ? native[i*channels+1] : 255;
          expected[i*4] = expected[i*4+1] = expected[i*4+2] = expected_native[i*channels];
          expected[i*4+3] = channels == 2 ? expected_native[i*channels+1] : 255;
        }
      const bool edge = procedure == "plug-in-edge";
      require (edge || procedure == "plug-in-gauss","Invalid Gray procedure");
      EdgeOptions eo {a,int (b),c}; GaussOptions go {a,b,c};
      const auto result = run (input,w,h,edge ? &eo : nullptr,edge ? nullptr : &go);
      require (result == expected,"Disk-backed old native Gray byte mismatch: " + out);
      Bytes vector; std::atomic<bool> cancel {false};
      require (edge ? filter_edge (input,w,h,eo,cancel,vector) : filter_gauss (input,w,h,go,cancel,vector),"Vector cancellation");
      require (result == vector,"Gray raster/vector mismatch"); ++count;
    }
  require (stream.eof () && count == 77,"Incomplete native Gray corpus");
  std::cout << "Native Gray/Gray-alpha old runtime + vector + disk byte comparisons: " << count << " passed\n";
}
void boundaries_and_fractional_axes ()
{
  unsigned edge_count = 0, gauss_count = 0;
  for (const auto& shape : {std::pair<std::size_t,std::size_t> {1,1}, {1,19}, {19,1}, {7,13}, {1023,2}, {1024,3}, {1025,5}})
    {
      const auto w = shape.first,h = shape.second; const auto input = fixture (w,h);
      for (int mode = 0; mode < 6; ++mode)
        for (int wrap = 1; wrap <= 3; ++wrap)
          {
            EdgeOptions options {1.75,wrap,mode}; std::atomic<bool> cancel {false}; Bytes expected;
            require (filter_edge (input,w,h,options,cancel,expected),"Vector failed");
            require (run (input,w,h,&options,nullptr) == expected,"Halo boundary mismatch"); ++edge_count;
          }
      for (int method : {0,1})
        for (const auto& radii : {std::pair<double,double> {0.25,0.75},{2.5,7.25},{0,25},{25,0},{1,5},{-1,3.25}})
          {
            GaussOptions options {radii.first,radii.second,method}; std::atomic<bool> cancel {false}; Bytes expected;
            require (filter_gauss (input,w,h,options,cancel,expected),"Vector failed");
            require (run (input,w,h,nullptr,&options) == expected,"Fractional/one-axis mismatch"); ++gauss_count;
          }
    }
  const std::size_t w = 1031,h = 1027; const auto input = fixture (w,h);
  for (int method : {0,1})
    {
      GaussOptions options {2.5,7.25,method}; Bytes expected; std::atomic<bool> cancel {false};
      require (filter_gauss (input,w,h,options,cancel,expected),"Vector failed");
      require (run (input,w,h,nullptr,&options) == expected,"Transpose boundary mismatch"); ++gauss_count;
    }
  std::cout << "Generated parity: " << edge_count << " Edge halo/border cases, " << gauss_count << " Gaussian fractional/axis/tile cases passed\n";
}
class SizedRaster final : public FilterRaster
{
  std::uint64_t size_;
public:
  std::size_t calls = 0;
  std::atomic<bool> *cancel = nullptr;
  std::uint64_t cancel_offset = std::numeric_limits<std::uint64_t>::max (), observed = 0;
  explicit SizedRaster (std::uint64_t size) : size_ (size) {}
  std::uint64_t size () const noexcept override { return size_; }
  void access (std::uint64_t offset, std::size_t count)
  {
    require (offset <= size_ && count <= size_-offset,"Wrapped raster offset");
    ++calls; observed = std::max (observed,offset);
    if (cancel && offset >= cancel_offset) cancel->store (true);
  }
  void read (std::uint64_t offset, std::size_t count, std::uint8_t *bytes) override
  { access (offset,count); std::fill_n (bytes,count,0); }
  void write (std::uint64_t offset, std::size_t count, const std::uint8_t *) override
  { access (offset,count); }
  void flush () override { ++calls; }
};
void validation_and_large_addresses ()
{
  SizedRaster input (16),output (16),bad (15); std::atomic<bool> cancel {false};
  const auto make = [] (std::uint64_t size) { return std::unique_ptr<FilterRaster> (new SizedRaster (size)); };
  rejects<std::invalid_argument> ([&] { filter_edge_raster (input,2,2,{},cancel,input); },"Edge alias accepted");
  rejects<std::invalid_argument> ([&] { filter_gauss_raster (input,2,2,{},cancel,input,make); },"Gaussian alias accepted");
  for (auto dims : {std::pair<std::size_t,std::size_t> {0,2},{2,0},{std::numeric_limits<std::size_t>::max (),2}})
    {
      rejects<std::invalid_argument> ([&] { filter_edge_raster (input,dims.first,dims.second,{},cancel,output); },"Invalid Edge extent accepted");
      rejects<std::invalid_argument> ([&] { filter_gauss_raster (input,dims.first,dims.second,{},cancel,output,make); },"Invalid Gaussian extent accepted");
    }
  rejects<std::invalid_argument> ([&] { filter_edge_raster (input,2,2,{},cancel,bad); },"Wrong Edge output size accepted");
  rejects<std::invalid_argument> ([&] { filter_gauss_raster (bad,2,2,{},cancel,output,make); },"Wrong Gaussian input size accepted");
  const double nan = std::numeric_limits<double>::quiet_NaN (), inf = std::numeric_limits<double>::infinity ();
  for (auto opts : {EdgeOptions {nan,2,0},{inf,2,0},{2,0,0},{2,4,0},{2,2,-1},{2,2,6}})
    rejects<std::invalid_argument> ([&] { filter_edge_raster (input,2,2,opts,cancel,output); },"Bad Edge options accepted");
  for (auto opts : {GaussOptions {nan,2,0},{2,inf,0},{0,0,0},{2,2,-1},{2,2,2},{1e100,1e100,0},{1e100,1e100,1}})
    rejects<std::invalid_argument> ([&] { filter_gauss_raster (input,2,2,opts,cancel,output,make); },"Bad Gaussian options accepted");
  rejects<std::invalid_argument> ([&] { filter_gauss_raster (input,2,2,{},cancel,output,{}); },"Missing factory accepted");
  rejects<std::invalid_argument> ([&] { filter_gauss_raster (input,2,2,{},cancel,output,[] (std::uint64_t) { return std::unique_ptr<FilterRaster> {}; }); },"Null scratch accepted");
  rejects<std::invalid_argument> ([&] { filter_gauss_raster (input,2,2,{},cancel,output,[] (std::uint64_t) { return std::unique_ptr<FilterRaster> (new SizedRaster (1)); }); },"Wrong scratch size accepted");
  for (auto *alias : {&input,&output})
    rejects<std::invalid_argument> ([&] { filter_gauss_raster (input,2,2,{},cancel,output,[&] (std::uint64_t) { return std::unique_ptr<FilterRaster> (alias); }); },"Aliased scratch accepted");
  const std::size_t w = 1025,h = 1048577;
  const auto size = std::uint64_t (w)*h*4, threshold = UINT64_C (1)<<32;
  SizedRaster large_input (size),large_output (size);
  large_input.cancel = &cancel; large_input.cancel_offset = threshold;
  require (!filter_edge_raster (large_input,w,h,{1.75,1,0},cancel,large_output),"High-offset Edge did not cancel");
  require (large_input.observed >= threshold && !large_output.calls,"Edge did not preserve >4GiB offset");
  cancel.store (false); large_input.cancel = nullptr;
  std::uint64_t scratch_observed = 0;
  struct HighScratch final : FilterRaster
  {
    SizedRaster bytes; std::uint64_t& observed;
    HighScratch (std::uint64_t size,std::atomic<bool>& cancel,std::uint64_t& observed)
      : bytes (size),observed (observed) { bytes.cancel = &cancel; bytes.cancel_offset = UINT64_C (1)<<32; }
    ~HighScratch () override { observed = bytes.observed; }
    std::uint64_t size () const noexcept override { return bytes.size (); }
    void read (std::uint64_t o,std::size_t n,std::uint8_t *p) override { bytes.read (o,n,p); }
    void write (std::uint64_t o,std::size_t n,const std::uint8_t *p) override { bytes.write (o,n,p); }
    void flush () override { bytes.flush (); }
  };
  require (!filter_gauss_raster (large_input,w,h,{},cancel,large_output,[&] (std::uint64_t n) {
    return std::unique_ptr<FilterRaster> (new HighScratch (n,cancel,scratch_observed));
  }),"High-offset Gaussian did not cancel");
  require (scratch_observed >= threshold && !large_output.calls,"Gaussian transpose narrowed >4GiB offset");
  std::cout << "Validation, direct/scratch alias rejection and synthetic >4GiB kernel addressing passed\n";
}
void cancellation_and_failures ()
{
  const auto bytes = fixture (5,4); unsigned injected = 0;
  for (int mode : {-1,0,1,2})
    {
      const GaussOptions options = mode == 2 ? GaussOptions {2.5,0,1} : GaussOptions {2.5,7.25,mode};
      std::size_t events = 0;
      for (int kind = 0; kind < 3; ++kind)
        for (std::size_t at = 0; at <= (kind == 0 ? 0 : events); ++at)
          {
            Stats stats; ObservedRaster input (bytes.size (),stats),output (bytes.size (),stats);
            input.write (0,bytes.size (),bytes.data ()); stats.events = 0;
            std::atomic<bool> cancel {kind == 1 && !at}; stats.cancel = &cancel;
            if (kind == 1) stats.cancel_at = at;
            if (kind == 2) { if (!at) continue; stats.fail_at = at; }
            bool done = false, failed = false;
            try { done = mode == -1 ? filter_edge_raster (input,5,4,{},cancel,output)
              : filter_gauss_raster (input,5,4,options,cancel,output,factory (stats)); }
            catch (const std::runtime_error&) { failed = true; }
            require (!stats.live,"Interrupted kernel leaked scratch");
            if (!kind) { require (done && !failed,"Control run failed"); events = stats.events; }
            else if (kind == 1) require (!done && !failed,"Cancellation reported success or exception");
            else require (!done && failed,"I/O failure reported success");
            if (kind) ++injected;
          }
    }
  std::cout << "Deterministic cancellation/error injection at every read/write/flush/factory boundary: " << injected << " passed\n";
}
void bounded_disk_workspace ()
{
  /* No complete raster vector in this case. Large on-disk input/output and
   * scratch are constructed/read in fixed chunks. A 20 MiB image exceeds the
   * transpose tile and every scanline allocation by a wide margin. */
  const std::size_t w = 4099,h = 1281;
  const auto size = std::uint64_t (w)*h*4;
  Stats stats; ObservedRaster input (size,stats),output (size,stats);
  std::array<std::uint8_t,65536> chunk; chunk.fill (0);
  for (std::size_t i = 3; i < chunk.size (); i += 4) chunk[i] = 255;
  for (std::uint64_t offset = 0; offset < size; )
    {
      const auto n = std::size_t (std::min<std::uint64_t> (chunk.size (),size-offset));
      input.write (offset,n,chunk.data ()); offset += n;
    }
  stats.events = stats.reads = stats.writes = stats.max_io = 0;
  std::atomic<bool> cancel {false};
  require (filter_gauss_raster (input,w,h,{2.5,7.25,1},cancel,output,factory (stats)),"Bounded Gaussian failed");
  require (stats.peak_live == 1 && stats.peak_disk_bytes == size && !stats.live,"Unbounded scratch raster count");
  require (stats.max_io <= chunk.size (),"Unbounded Gaussian I/O");
  for (std::uint64_t offset = 0; offset < size; )
    {
      const auto n = std::size_t (std::min<std::uint64_t> (chunk.size (),size-offset));
      output.read (offset,n,chunk.data ());
      for (std::size_t i = 0; i < n; ++i) require (chunk[i] == (i%4 == 3 ? 255 : 0),"Large constant changed");
      offset += n;
    }
  stats.events = stats.reads = stats.writes = stats.max_io = 0;
  require (filter_edge_raster (input,w,h,{},cancel,output),"Bounded Edge failed");
  require (stats.max_io <= 4104,"Edge I/O exceeded fixed halo strip");
  require (stats.reads <= 3*h*(w/1024+2) && stats.writes == h*((w+1023)/1024),"Per-pixel Edge I/O regression");
  std::cout << "Bounded disk case: dimensions=" << w << 'x' << h << " raster_bytes=" << size
            << " gaussian_scratch_peak_bytes=" << stats.peak_disk_bytes
            << " gaussian_scratch_peak_count=" << stats.peak_live
            << " edge_max_io=" << stats.max_io << " edge_reads=" << stats.reads
            << " edge_writes=" << stats.writes << '\n';
}
} // namespace
int main (int argc,char **argv)
{
  gchar *path = g_dir_make_tmp ("painter-raster-kernels-XXXXXX",nullptr);
  if (!path) return 1;
  directory = path; g_free (path);
  int result = 0;
  try
    {
      const bool bounded_only = argc == 2 && std::string (argv[1]) == "--bounded-only";
      require (argc == 4 || bounded_only,"Usage: painter-filter-raster-kernels edge.tsv gauss.tsv gray.tsv | --bounded-only");
      if (!bounded_only)
        {
          rgb_corpus (argv[1],true,76); rgb_corpus (argv[2],false,104); gray_corpus (argv[3]);
          boundaries_and_fractional_axes (); validation_and_large_addresses ();
          cancellation_and_failures ();
        }
      bounded_disk_workspace ();
      std::cout << "Spill-backed filter kernel tests passed\n";
    }
  catch (const std::exception& e) { std::cerr << e.what () << '\n'; result = 1; }
  if (g_rmdir (directory.c_str ()) != 0) { std::cerr << "Temporary raster cleanup failed\n"; result = 1; }
  return result;
}
