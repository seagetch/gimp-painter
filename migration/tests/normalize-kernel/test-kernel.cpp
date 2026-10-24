/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "app/painter/filter-normalize-kernel.hpp"
#include "app/painter/filter-normalize-kernel.hpp"
#include <cassert>
#include <cstdio>
using namespace GimpPainter;
using Pixel=std::array<std::uint8_t,4>;
static unsigned long checks;
int main(){
 for(unsigned low=0;low<256;++low)for(unsigned high=low;high<256;++high){
  FilterNormalizeStats s;s.add(Pixel{{(std::uint8_t)low,(std::uint8_t)high,(std::uint8_t)low,1}});
  const auto k=s.finish();assert(k.status()==FilterNormalizeStatus::ready);
  for(unsigned v=low;v<=high;++v)for(unsigned a: {0u,1u,127u,255u}){
   Pixel p{{(std::uint8_t)v,(std::uint8_t)high,(std::uint8_t)low,(std::uint8_t)a}};s.add(p);
   auto out=k.apply(p);unsigned want=high==low?v:255*(v-low)/(high-low);
   assert(out[0]==want&&out[1]==(high==low?high:255)&&out[2]==(high==low?low:0)&&out[3]==a);checks++;
  }
  assert(s.finish().status()==FilterNormalizeStatus::ready);
 }
 FilterNormalizeStats empty;auto e=empty.finish();assert(e.status()==FilterNormalizeStatus::empty);
 bool threw=false;try{e.apply(Pixel{{0,0,0,0}});}catch(const std::logic_error&){threw=true;}assert(threw);
 FilterNormalizeStats transparent;transparent.add(Pixel{{10,20,30,0}});assert(transparent.finish().status()==FilterNormalizeStatus::no_visible_samples);
 for(unsigned outside:{9u,31u}){FilterNormalizeStats s;s.add(Pixel{{10,20,30,1}});const auto stable=s.finish();s.add(Pixel{{(std::uint8_t)outside,20,30,0}});assert(s.finish().status()==FilterNormalizeStatus::hidden_outside_range);assert(stable.status()==FilterNormalizeStatus::ready);assert(stable.apply(Pixel{{10,20,30,255}})[0]==0);bool rejected=false;try{stable.apply(Pixel{{(std::uint8_t)outside,20,30,0}});}catch(const std::invalid_argument&){rejected=true;}assert(rejected);}
 std::printf("PASS %lu defined pixel/alpha checks; empty/all-transparent/hidden-outside rejected; immutable snapshot\n",checks);
}
