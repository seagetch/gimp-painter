/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "filter-native-kernels.hpp"
#include <glib.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iterator>
#include <limits>
#include <sstream>
using namespace GimpPainter;
using Bytes = std::vector<std::uint8_t>;
static std::string manifest;
class Memory final : public FilterRaster {
public:
  Bytes data;
  explicit Memory (std::uint64_t n) : data(n) {}
  explicit Memory (const Bytes& b) : data(b) {}
  std::uint64_t size() const noexcept override { return data.size(); }
  void read(std::uint64_t off,std::size_t n,std::uint8_t *p) override {
    if(off>size() || n>size()-off) throw std::out_of_range("read");
    std::memcpy(p,data.data()+off,n);
  }
  void write(std::uint64_t off,std::size_t n,const std::uint8_t *p) override {
    if(off>size() || n>size()-off) throw std::out_of_range("write");
    std::memcpy(data.data()+off,p,n);
  }
  void flush() override {}
};
static FilterRasterFactory factory = [](std::uint64_t n) {return std::unique_ptr<FilterRaster>(new Memory(n));};
static Bytes bytes(const std::vector<double>& samples)
{ Bytes b(samples.size()*sizeof(double));std::memcpy(b.data(),samples.data(),b.size());return b; }
static std::vector<double> reals(const Bytes& b)
{ std::vector<double> p(b.size()/sizeof(double));std::memcpy(p.data(),b.data(),b.size());return p; }
static Bytes read(const std::string& name)
{ std::ifstream in(name,std::ios::binary);g_assert_true(bool(in));return {std::istreambuf_iterator<char>(in),{}}; }
static void genuine_point_corpus()
{
  const auto directory=manifest.substr(0,manifest.find_last_of('/')+1);
  std::ifstream input(manifest);g_assert_true(bool(input));std::string line;unsigned cases=0;
  while(std::getline(input,line)) {
    if(line.empty() || line[0]=='#') continue;
    std::istringstream row(line);std::string proc,src,dst,extra;int argument;std::size_t w,h,c;
    g_assert_true(bool(row>>proc>>argument>>w>>h>>c>>src>>dst));g_assert_false(bool(row>>extra));
    const auto native=read(directory+src),expected=read(directory+dst);g_assert_cmpuint(native.size(),==,w*h*c);
    Bytes rgba(w*h*4),actual;
    for(std::size_t i=0;i<w*h;++i) {
      if(c==2) std::fill_n(rgba.data()+i*4,3,native[i*2]);
      else std::copy_n(native.data()+i*c,3,rgba.data()+i*4);
      rgba[i*4+3]=c==3?255:native[(i+1)*c-1];
    }
    auto operation=proc=="plug-in-vinvert"?FilterPoint::value_invert:proc=="plug-in-max-rgb"?FilterPoint::max_rgb:FilterPoint::threshold_alpha;
    std::atomic<bool> cancel{false};
    const FilterNativeProcess process=[&](FilterRaster& in,FilterRaster& out,std::atomic<bool>& stop,const FilterRasterFactory&) {
      return filter_point_raster(in,out,w,h,operation,argument,false,stop);
    };
    g_assert_true(filter_native_vector(rgba,cancel,actual,process));
    Memory in(rgba),out(rgba.size());g_assert_true(process(in,out,cancel,factory));g_assert_true(out.data==actual);
    for(std::size_t i=0;i<w*h;++i) for(std::size_t channel=0;channel<c;++channel) {
      const auto index=c==2?(channel?3:0):channel;
      if(actual[i*4+index]!=expected[i*c+channel])
        g_error("Genuine %s pixel %zu channel %zu: got%u expected%u",dst.c_str(),i,channel,actual[i*4+index],expected[i*c+channel]);
    }
    ++cases;
  }
  g_assert_cmpuint(cases,==,40);g_test_message("40 real original PDB point outputs matched byte-for-byte");
}
static void real_point_equations()
{
  const std::vector<double> original={.123456789,.234567891,.765432198,1, .9,.4,.7,0, .4,.4,.2,.500001};
  for(auto op:{FilterPoint::value_invert,FilterPoint::max_rgb,FilterPoint::threshold_alpha}) for(int argument:{-2,0,1,127,256}) {
    auto expected=original;
    for(std::size_t i=0;i<expected.size();i+=4) {
      if(op==FilterPoint::threshold_alpha) expected[i+3]=argument/255.0<original[i+3]?1:0;
      else if(original[i+3]) {
        const double maximum=std::max({original[i],original[i+1],original[i+2]});
        const double value=argument>0?maximum:std::min({original[i],original[i+1],original[i+2]});
        for(int c=0;c<3;++c) expected[i+c]=op==FilterPoint::value_invert?(1-maximum)*original[i+c]/maximum:
          original[i+c]==value?value:0;
      }
    }
    Memory in(bytes(original)),out(in.size());std::atomic<bool> cancel{false};
    g_assert_true(filter_point_raster(in,out,3,1,op,argument,true,cancel));auto actual=reals(out.data);
    for(std::size_t i=0;i<actual.size();++i) g_assert_cmpfloat_with_epsilon(actual[i],expected[i],1e-15);
  }
}
static void real_edge_ramp_and_hidden()
{
  std::vector<double> samples;
  for(int y=0;y<5;++y) for(int x=0;x<5;++x) samples.insert(samples.end(),{.2+x*.0123456789,.7,.6,1});
  samples[3]=0;
  Memory in(bytes(samples)),out(in.size());std::atomic<bool> cancel{false};
  g_assert_true(filter_edge_real_raster(in,out,5,5,{1,2,0},cancel));auto actual=reals(out.data);
  g_assert_cmpfloat_with_epsilon(actual[(2*5+2)*4],8*.0123456789,1e-15);
  for(int c=0;c<4;++c) g_assert_cmpfloat(actual[c],==,samples[c]);
  for(int mode=0;mode<6;++mode) for(int wrap=1;wrap<=3;++wrap) {
    g_assert_true(filter_edge_real_raster(in,out,5,5,{1.75,wrap,mode},cancel));
    for(double v:reals(out.data)) g_assert_true(std::isfinite(v));
  }
}
static void real_gauss_constant_and_impulse()
{
  for(int method:{0,1}) {
    std::vector<double> samples(13*11*4);
    for(std::size_t i=0;i<samples.size();i+=4) {samples[i]=.123456789;samples[i+1]=.345678912;samples[i+2]=.712345678;samples[i+3]=1;}
    Memory in(bytes(samples)),out(in.size());std::atomic<bool> cancel{false};
    g_assert_true(filter_gauss_real_raster(in,out,13,11,{2.5,7.25,method},cancel,factory));auto actual=reals(out.data);
    for(std::size_t i=0;i<actual.size();++i) g_assert_cmpfloat_with_epsilon(actual[i],samples[i],1e-10);
    // A single RLE horizontal pass admits an independent finite convolution.
    for(int x=0;x<13;++x) for(int c=0;c<3;++c) samples[x*4+c]=x==6?.7123456789:0;
    Memory impulse(bytes(std::vector<double>(samples.begin(),samples.begin()+13*4))),blur(13*32);
    g_assert_true(filter_gauss_real_raster(impulse,blur,13,1,{2,0,method},cancel,factory));actual=reals(blur.data);
    const double sigma2=-9/std::log(1.0/255.0);int weights[4],denominator=0;
    for(int i=0;i<=3;++i) weights[i]=int(std::exp(-i*i/sigma2)*255);
    for(int i=-3;i<3;++i) denominator+=weights[std::abs(i)];
    for(int x=0;x<13;++x) {
      // The original RLE chooses encoded accumulation on this mostly-zero row,
      // so its half-open support is intentional, as documented.
      const int delta=6-x;
      const double expected=delta>=-3 && delta<3?.7123456789*weights[std::abs(delta)]/denominator:0;
      g_assert_cmpfloat_with_epsilon(actual[x*4],expected,1e-15);
    }
  }
}
static void native_rejection_and_cancel()
{
  for(double bad:{std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}) {
    auto source=bytes({bad,.2,.3,1});Memory in(source),out(source.size());std::atomic<bool> cancel{false};bool caught=false;
    try {filter_point_raster(in,out,1,1,FilterPoint::value_invert,0,true,cancel);}catch(const std::invalid_argument&){caught=true;}
    g_assert_true(caught);
  }
  auto source=bytes({.123456789,.2,.3,1});Bytes output{17};std::atomic<bool> cancel{true};
  g_assert_false(filter_native_vector(source,cancel,output,{}));g_assert_cmpuint(output.size(),==,1);g_assert_cmpuint(output[0],==,17);
  Memory in(source),out(source.size());
  g_assert_false(filter_gauss_real_raster(in,out,1,1,{2,2,0},cancel,factory));
  g_assert_false(filter_edge_real_raster(in,out,1,1,{1,2,0},cancel));
}
static void native_transpose()
{
  std::vector<double> samples(9*7*4);for(std::size_t i=0;i<samples.size();++i)samples[i]=i*.123456789;
  Memory in(bytes(samples)),transposed(in.size()),out(in.size());std::atomic<bool> cancel{false};
  g_assert_true(transpose_filter_rgba(in,transposed,9,7,cancel,3,32));
  g_assert_true(transpose_filter_rgba(transposed,out,7,9,cancel,4,32));g_assert_true(in.data==out.data);
}
int main(int argc,char **argv)
{
  g_assert_cmpint(argc,>=,2);manifest=argv[1];--argc;++argv;g_test_init(&argc,&argv,nullptr);
#define ADD(n) g_test_add_func("/filter-native/" #n,n)
  ADD(genuine_point_corpus);ADD(real_point_equations);ADD(real_edge_ramp_and_hidden);
  ADD(real_gauss_constant_and_impulse);ADD(native_rejection_and_cancel);ADD(native_transpose);
  return g_test_run();
}
