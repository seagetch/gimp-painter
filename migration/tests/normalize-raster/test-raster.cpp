/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "app/painter/filter-normalize-raster.hpp"
#include "app/painter/filter-normalize-raster.hpp"
#include "app/painter/filter-progress.hpp"
#include <vector>
#include <cassert>
#include <cstring>
#include <stdexcept>
#include <cstdio>
using namespace GimpPainter;
static unsigned checks;
namespace {
struct Events{int ordinal=0,at=0;bool throws=false;std::atomic<bool> cancel{false};void event(){++ordinal;if(ordinal==at){if(throws)throw std::runtime_error("injected IO");cancel=true;}}};
struct Memory:FilterRaster{std::vector<std::uint8_t> bytes;Events&e;int reads=0,writes=0,flushes=0;Memory(std::size_t n,Events&x):bytes(n,0xa5),e(x){}std::uint64_t size()const noexcept override{return bytes.size();}void read(std::uint64_t o,std::size_t n,std::uint8_t*p)override{assert(n<=32768&&o+n<=bytes.size());reads++;e.event();memcpy(p,bytes.data()+o,n);}void write(std::uint64_t o,std::size_t n,const std::uint8_t*p)override{assert(n<=32768&&o+n<=bytes.size());writes++;e.event();memcpy(bytes.data()+o,p,n);}void flush()override{flushes++;e.event();}};
}
static std::vector<std::uint8_t> scene(std::size_t pixels){std::vector<std::uint8_t>b(pixels*4);for(std::size_t i=0;i<pixels;i++){b[i*4]=20+i%201;b[i*4+1]=40+i%101;b[i*4+2]=80+i%71;b[i*4+3]=i%3?1:0;}b[0]=20;b[1]=220;b[3]=255;return b;}
static std::vector<std::uint8_t> oracle(std::vector<std::uint8_t>b){int lo=255,hi=0;for(std::size_t i=0;i<b.size();i+=4)if(b[i+3])for(int c=0;c<3;c++){lo=std::min(lo,int(b[i+c]));hi=std::max(hi,int(b[i+c]));}for(std::size_t i=0;i<b.size();i+=4)for(int c=0;c<3;c++)b[i+c]=hi==lo?b[i+c]:255*(b[i+c]-lo)/(hi-lo);return b;}
int main(){
 for(std::size_t count:{1u,8191u,8192u,8193u,20000u}){Events e;Memory in(count*4,e),out(count*4,e);in.bytes=scene(count);const auto saved=in.bytes,want=oracle(saved);auto progress=std::make_shared<FilterProgress>();assert(filter_normalize_raw_raster(in,out,count,1,e.cancel,progress)==FilterNormalizeOutcome::complete);FilterProgress::Snapshot state;assert(progress->try_snapshot(state));assert(out.bytes==want&&in.bytes==saved&&state.value==1&&out.flushes==1);int events=e.ordinal;checks++;
  for(bool throwing:{false,true})for(int at=1;at<=events;at++){Events f;f.at=at;f.throws=throwing;Memory a(count*4,f),b(count*4,f);a.bytes=saved;auto progress=std::make_shared<FilterProgress>();bool error=false;FilterNormalizeOutcome result=FilterNormalizeOutcome::complete;try{result=filter_normalize_raw_raster(a,b,count,1,f.cancel,progress);}catch(const std::runtime_error&){error=true;}assert(throwing?error:result==FilterNormalizeOutcome::cancelled);assert(a.bytes==saved);FilterProgress::Snapshot state;assert(progress->try_snapshot(state)&&state.value<1);checks++;}
  TemporaryFilterRaster filein("/tmp",saved.size()),fileout("/tmp",saved.size());filein.write(0,saved.size(),saved.data());filein.flush();std::atomic<bool> cancel{false};assert(filter_normalize_raw_raster(filein,fileout,count,1,cancel)==FilterNormalizeOutcome::complete);std::vector<std::uint8_t> got(saved.size());fileout.read(0,got.size(),got.data());assert(got==want);checks++;
 }
 for(bool no_visible:{false,true}){Events e;Memory in(8,e),out(8,e);in.bytes={20,30,40,1,10,30,40,0};if(no_visible)in.bytes[3]=0;auto saved=in.bytes;bool rejected=false;try{filter_normalize_raw_raster(in,out,2,1,e.cancel);}catch(const std::domain_error&){rejected=true;}assert(rejected&&out.writes==0&&out.flushes==0&&in.bytes==saved);checks++;}
 {Events e;Memory a(0,e),b(0,e);assert(filter_normalize_raw_raster(a,b,0,5,e.cancel)==FilterNormalizeOutcome::empty);assert(e.ordinal==0);checks++;}
 {Events e;Memory a(4,e),b(4,e);for(int type=0;type<3;type++){bool rejected=false;try{filter_normalize_raw_raster(a,type==0?a:b,type==1?46341:2,type==1?46341:1,e.cancel);}catch(const std::invalid_argument&){rejected=true;}assert(rejected&&e.ordinal==0);checks++;}}
 {Events e;e.cancel=true;Memory a(4,e),b(4,e);assert(filter_normalize_raw_raster(a,b,1,1,e.cancel)==FilterNormalizeOutcome::cancelled&&e.ordinal==0);checks++;}
 std::printf("PASS %u raster/fault/empty/unsupported cases\n",checks);
}
