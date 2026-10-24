/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "app/painter/filter-colorify-kernel.hpp"
#include <cassert>
#include <limits>
#include <type_traits>
#include <cstdio>
using namespace GimpPainter;
static_assert(!std::is_copy_assignable<FilterColorifyKernel>::value,"immutable kernel");
int main(){
 const double bad[]={-0.001,1.001,std::numeric_limits<double>::infinity(),-std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()};
 unsigned rejected=0;
 for(unsigned component=0;component<4;component++)for(double value:bad){FilterColorifyColor c{.1,.2,.3,.4};double *fields[]={&c.r,&c.g,&c.b,&c.a};*fields[component]=value;try{FilterColorifyKernel k(c);assert(false);}catch(const std::invalid_argument&){rejected++;}}
 FilterColorifyColor c{.5,.25,.75,-0.0};const FilterColorifyKernel k(c);c.r=1;
 assert(k.color().r==.5&&k.color().g==.25&&k.color().b==.75&&std::signbit(k.color().a));
 unsigned char in[4]={201,97,53,0},expected[4];k.pixel(in,expected,4);
 for(unsigned channels:{3u,4u})for(unsigned offset:{0u,3u,4u,5u}){unsigned char bytes[12]={0};std::memcpy(bytes+4,in,4);k.pixel(bytes+4,bytes+offset,channels);assert(std::memcmp(bytes+offset,expected,channels)==0);}
 unsigned invalid=0;unsigned char out[4]={0x11,0x22,0x33,0x44},sentinel[4];std::memcpy(sentinel,out,4);
 for(unsigned channels:{0u,1u,2u,5u,255u}){try{k.pixel(in,out,channels);assert(false);}catch(const std::invalid_argument&){invalid++;}assert(std::memcmp(out,sentinel,4)==0);}
 try{k.pixel(nullptr,out,4);assert(false);}catch(const std::invalid_argument&){invalid++;}assert(std::memcmp(out,sentinel,4)==0);
 try{k.pixel(in,nullptr,4);assert(false);}catch(const std::invalid_argument&){invalid++;}
 try{k.tint_component(3,0);assert(false);}catch(const std::out_of_range&){invalid++;}
 try{k.tint_component(0,256);assert(false);}catch(const std::out_of_range&){invalid++;}
 int original=std::fegetround();assert(original==FE_TONEAREST);
 for(int mode:{FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO}){assert(std::fesetround(mode)==0);try{FilterColorifyKernel bad_mode({1,1,1,1});assert(false);}catch(const std::invalid_argument&){rejected++;}assert(std::fegetround()==mode);}
 assert(std::fesetround(original)==0);
 printf("PASS Colorify immutable metadata;20 invalidRGBA+3 rounding rejections;9 invalid pixel/index calls;8 separate/exact/partial aliases;no rounding-mode mutation\n");assert(rejected==23&&invalid==9);
}
