/* SPDX-License-Identifier: GPL-3.0-or-later
 * Value inversion and maximum RGB arithmetic adapted from legacy
 * plug-ins/common/{value-invert,max-rgb,threshold-alpha}.c at afa43fae.
 * Original authors: Spencer Kimball, Peter Mattis, Hirotsuna Mizuno,
 * Shuji Narazaki and Sven Neumann. Gaussian/Edge equations share the audited
 * legacy kernels; normalized-double calculations are a modern extension. */
#include "filter-native-kernels.hpp"
#include "filter-edge-kernel-private.hpp"
#include "filter-gauss-kernel-private.hpp"
#include <array>
#include <cstring>
#include <climits>

namespace GimpPainter {
namespace {
using Bytes = std::vector<std::uint8_t>;
using Reals = std::vector<double>;
using FilterGaussDetail::Check;
constexpr std::size_t stride = 4 * sizeof (double);
static_assert (stride == 32, "Native filter transport needs IEEE-width double storage");

std::uint64_t extent (FilterRaster& in, FilterRaster& out, std::size_t w, std::size_t h, std::size_t bpp)
{
  if (!w || !h || &in == &out || std::uint64_t (w) > std::uint64_t (INT64_MAX) / bpp / h ||
      in.size () != std::uint64_t (w) * h * bpp || out.size () != in.size ())
    throw std::invalid_argument ("Invalid native filter raster extent/storage");
  return in.size ();
}
void validate (const double *samples, std::size_t count)
{
  for (std::size_t i = 0; i < count; ++i)
    if (!std::isfinite (samples[i]) || (i % 4 == 3 && (samples[i] < 0 || samples[i] > 1)))
      throw std::invalid_argument ("Native filter requires finite samples and alpha in [0,1]");
}
void validate_raster (FilterRaster& in, Check& check)
{
  std::array<double,4096> samples;
  for (std::uint64_t offset = 0; offset < in.size ();)
    {
      check.now ();
      const auto bytes = std::size_t (std::min (std::uint64_t (sizeof samples), in.size () - offset));
      in.read (offset, bytes, reinterpret_cast<std::uint8_t *> (samples.data ()));
      validate (samples.data (), bytes / sizeof (double)); offset += bytes;
    }
}
void point_byte (std::uint8_t *p, FilterPoint operation, int argument)
{
  if (operation == FilterPoint::threshold_alpha) { p[3] = argument < p[3] ? 255 : 0; return; }
  if (!p[3]) return; // old REPLACE_INTEN shadow merge retains hidden RGB
  const int maximum = std::max ({p[0],p[1],p[2]});
  const int minimum = std::min ({p[0],p[1],p[2]});
  if (operation == FilterPoint::max_rgb)
    {
      const int value = argument > 0 ? maximum : minimum;
      for (int c = 0; c < 3; ++c) if (p[c] != value) p[c] = 0;
    }
  else if (!maximum || maximum == minimum)
    std::fill_n (p,3,255-maximum);
  else
    for (int c = 0; c < 3; ++c) p[c] = ((255-maximum) * p[c] + maximum/2) / maximum;
}
void point_real (double *p, FilterPoint operation, int argument)
{
  if (operation == FilterPoint::threshold_alpha) { p[3] = argument / 255.0 < p[3] ? 1 : 0; return; }
  if (!p[3]) return;
  const double maximum = std::max ({p[0],p[1],p[2]});
  const double minimum = std::min ({p[0],p[1],p[2]});
  if (operation == FilterPoint::max_rgb)
    {
      const double value = argument > 0 ? maximum : minimum;
      for (int c = 0; c < 3; ++c) if (p[c] != value) p[c] = 0;
    }
  else if (!maximum || maximum == minimum)
    std::fill_n (p,3,1-maximum);
  else
    for (int c = 0; c < 3; ++c) p[c] = (1-maximum) * p[c] / maximum;
  validate (p,4);
}
void read_strip (FilterRaster& in, std::size_t w, std::size_t y,
                 std::size_t x, std::size_t n, int wrap, double *samples)
{
  const auto first = x ? x-1 : 0, end = x+n < w ? x+n+1 : w;
  auto *bytes = reinterpret_cast<std::uint8_t *> (samples);
  const auto row = std::uint64_t (y) * w * stride;
  in.read (row+std::uint64_t (first)*stride,(end-first)*stride,bytes+(x ? 0 : stride));
  if (!x) {
    if (wrap == 1) in.read (row+(std::uint64_t(w)-1)*stride,stride,bytes);
    else if (wrap == 2) std::copy_n (samples+4,4,samples);
    else std::fill_n (samples,4,0.0);
  }
  if (x+n == w) {
    auto *last = samples+(n+1)*4;
    if (wrap == 1) in.read (row,stride,reinterpret_cast<std::uint8_t *> (last));
    else if (wrap == 2) std::copy_n (last-4,4,last);
    else std::fill_n (last,4,0.0);
  }
}
template<class Kernel>
void row_pass (FilterRaster& in, FilterRaster& out, std::size_t w, std::size_t h,
               Kernel& kernel, Check& check)
{
  Reals source (w*4), result (w*4);
  for (std::size_t y=0; y<h; ++y) {
    check.now ();
    const auto offset = std::uint64_t(y)*w*stride;
    in.read (offset,w*stride,reinterpret_cast<std::uint8_t *> (source.data ()));
    for (std::size_t i=0;i<source.size ();i+=4) {
      check.step ();
      for (int c=0;c<3;++c) source[i+c] *= source[i+3];
    }
    kernel.process (source,result,check);
    for (std::size_t i=0;i<result.size ();i+=4) {
      check.step ();
      if (result[i+3] > 0 && result[i+3] != 1)
        for (int c=0;c<3;++c) result[i+c] = std::min (1.0,result[i+c]/result[i+3]);
    }
    validate (result.data (),result.size ());
    out.write (offset,w*stride,reinterpret_cast<const std::uint8_t *> (result.data ()));
  }
}
void gaussian_pass (FilterRaster& in, FilterRaster& out, std::size_t w, std::size_t h,
                    double radius, bool iir, Check& check)
{
  if (iir) { FilterGaussDetail::Iir kernel (radius,w); row_pass (in,out,w,h,kernel,check); }
  else { FilterGaussDetail::RleReal kernel (radius,w,check); row_pass (in,out,w,h,kernel,check); }
}
class VectorRaster final : public FilterRaster {
  const Bytes *input_;
  Bytes *output_;
  Bytes owned_;
public:
  explicit VectorRaster (const Bytes& input) : input_ (&input), output_ (nullptr) {}
  explicit VectorRaster (Bytes& output) : input_ (&output), output_ (&output) {}
  explicit VectorRaster (std::uint64_t size) : input_ (&owned_), output_ (&owned_), owned_ (size) {}
  std::uint64_t size () const noexcept override { return input_->size (); }
  void read (std::uint64_t offset,std::size_t count,std::uint8_t *data) override {
    if (offset>size () || count>size ()-offset) throw std::out_of_range ("Vector raster read");
    std::memcpy (data,input_->data ()+offset,count);
  }
  void write (std::uint64_t offset,std::size_t count,const std::uint8_t *data) override {
    if (!output_ || offset>size () || count>size ()-offset) throw std::out_of_range ("Vector raster write");
    std::memcpy (output_->data ()+offset,data,count);
  }
  void flush () override {}
};
}

bool filter_native_vector (const Bytes& input, std::atomic<bool>& cancel, Bytes& output,
                            const FilterNativeProcess& process)
{
  if (cancel.load (std::memory_order_relaxed)) return false;
  Bytes result (input.size ()); VectorRaster in (input), out (result);
  FilterRasterFactory factory = [] (std::uint64_t n) {
    if (n > std::numeric_limits<std::size_t>::max ()) throw std::bad_alloc ();
    return std::unique_ptr<FilterRaster> (new VectorRaster (n));
  };
  if (!process (in,out,cancel,factory) || cancel.load (std::memory_order_relaxed)) return false;
  output.swap (result); return true;
}
bool filter_point_raster (FilterRaster& in, FilterRaster& out, std::size_t w, std::size_t h,
                           FilterPoint operation, int argument, bool real, std::atomic<bool>& cancel)
{
  const auto bpp = real ? stride : 4;
  const auto size = extent (in,out,w,h,bpp);
  if (operation != FilterPoint::value_invert && operation != FilterPoint::max_rgb &&
      operation != FilterPoint::threshold_alpha) throw std::invalid_argument ("Unknown point operation");
  std::array<double,4096> aligned;
  auto *bytes = reinterpret_cast<std::uint8_t *> (aligned.data ());
  for (std::uint64_t offset=0;offset<size;) {
    if (cancel.load (std::memory_order_relaxed)) return false;
    const auto count = std::size_t (std::min (std::uint64_t (sizeof aligned),size-offset));
    in.read (offset,count,bytes);
    if (real) validate (aligned.data (),count/sizeof(double));
    for (std::size_t i=0;i<count/bpp;++i) {
      if ((i & 255)==0 && cancel.load (std::memory_order_relaxed)) return false;
      if (real) point_real (aligned.data ()+i*4,operation,argument);
      else point_byte (bytes+i*4,operation,argument);
    }
    out.write (offset,count,bytes); offset+=count;
  }
  out.flush (); return !cancel.load (std::memory_order_relaxed);
}
bool filter_edge_real_raster (FilterRaster& in, FilterRaster& out, std::size_t w, std::size_t h,
                               const EdgeOptions& options, std::atomic<bool>& cancel)
{
  extent (in,out,w,h,stride);
  if (!std::isfinite (options.amount) || options.wrapmode<1 || options.wrapmode>3 || options.edgemode<0 || options.edgemode>5)
    throw std::invalid_argument ("Invalid native edge options");
  Check check {cancel};
  try {
    validate_raster (in,check);
    constexpr std::size_t strip=1024;
    std::array<std::array<double,(strip+2)*4>,3> rows;
    std::array<double,strip*4> result;
    for (std::size_t y=0;y<h;++y) for (std::size_t x=0;x<w;) {
      check.now (); const auto count=std::min(strip,w-x);
      for (int dy=-1;dy<=1;++dy) {
        std::size_t source_y;
        if (FilterEdgeDetail::neighbor(y,h,dy,options.wrapmode,source_y))
          read_strip(in,w,source_y,x,count,options.wrapmode,rows[dy+1].data());
        else std::fill_n(rows[dy+1].data(),(count+2)*4,0.0);
      }
      for (std::size_t i=0;i<count;++i) {
        check.step (); const auto *original=rows[1].data()+(i+1)*4; auto *dest=result.data()+i*4;
        std::copy_n(original,4,dest);
        if (original[3]) for (int c=0;c<3;++c) {
          double kernel[9];
          for(int dx=0;dx<3;++dx) for(int dy=0;dy<3;++dy) kernel[dx*3+dy]=rows[dy][(i+dx)*4+c];
          dest[c]=FilterEdgeDetail::detect_real(kernel,options.edgemode,std::max(1.0,options.amount));
        }
      }
      out.write((std::uint64_t(y)*w+x)*stride,count*stride,reinterpret_cast<const std::uint8_t*>(result.data())); x+=count;
    }
    out.flush (); check.now (); return true;
  } catch (const FilterGaussDetail::Cancelled&) { return false; }
}
bool filter_gauss_real_raster (FilterRaster& in, FilterRaster& out, std::size_t w, std::size_t h,
                                const GaussOptions& options, std::atomic<bool>& cancel, const FilterRasterFactory& factory)
{
  const auto size=extent(in,out,w,h,stride);
  if (!std::isfinite(options.horizontal) || !std::isfinite(options.vertical) ||
      (options.horizontal<=0 && options.vertical<=0) || options.method<0 || options.method>1 ||
      w>std::size_t(INT_MAX/4) || h>std::size_t(INT_MAX/4)) throw std::invalid_argument("Invalid native Gaussian options");
  const auto region=FilterGaussDetail::legacy_region(w,h,options.horizontal,options.vertical);
  Check check {cancel};
  try {
    validate_raster(in,check);
    const bool iir=options.method==0 && options.horizontal>1 && options.vertical>1;
    if(options.vertical>0) {
      if(!factory) throw std::invalid_argument("Missing native Gaussian scratch factory");
      auto scratch=factory(size);
      if(scratch.get()==&in || scratch.get()==&out) { scratch.release(); throw std::invalid_argument("Aliased native Gaussian scratch"); }
      if(!scratch || scratch->size()!=size) throw std::invalid_argument("Invalid native Gaussian scratch");
      if(!transpose_filter_rgba(in,*scratch,w,h,cancel,256,stride)) return false;
      gaussian_pass(*scratch,*scratch,h,w,options.vertical,iir,check);
      if(!transpose_filter_rgba(*scratch,out,h,w,cancel,256,stride)) return false;
    }
    if(options.horizontal>0) gaussian_pass(options.vertical>0?out:in,out,w,h,options.horizontal,iir,check);
    std::array<double,4096> original,result;
    for(std::uint64_t offset=0;offset<size;) {
      check.now(); const auto bytes=std::size_t(std::min(std::uint64_t(sizeof original),size-offset));
      in.read(offset,bytes,reinterpret_cast<std::uint8_t*>(original.data()));
      out.read(offset,bytes,reinterpret_cast<std::uint8_t*>(result.data()));
      for(std::size_t i=0;i<bytes/sizeof(double);i+=4) {
        check.step();
        if(region.empty) std::copy_n(original.data()+i,4,result.data()+i);
        else if(region.cropped && !region.contains(offset/stride+i/4,w)) { std::copy_n(original.data()+i,3,result.data()+i); result[i+3]=0; }
        else if(result[i+3]==0) std::copy_n(original.data()+i,3,result.data()+i);
      }
      out.write(offset,bytes,reinterpret_cast<const std::uint8_t*>(result.data())); offset+=bytes;
    }
    out.flush(); check.now(); return true;
  } catch (const FilterGaussDetail::Cancelled&) { return false; }
}
}
