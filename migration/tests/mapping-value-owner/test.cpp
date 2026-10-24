// SPDX-License-Identifier: GPL-3.0-or-later
// Value-ownership contract; does not execute the invalid legacy allocator.
#include "app/paint/painter-mypaint/mapping.hpp"
#include <cassert>
#include <iostream>
#include <utility>
#include <vector>
using GimpPainter::MyPaint::Mapping;
static Mapping make(int inputs,int count) {
  Mapping m(inputs); m.base_value=0.25f;
  for(int i=0;i<inputs;++i) {
    m.set_n(i,count);
    for(int p=0;p<count;++p) m.set_point(i,p,float(p),float(i*16+p));
  }
  return m;
}
static void check(const Mapping& m,int inputs,int count) {
  assert(m.base_value==0.25f); assert(m.is_constant()==(inputs==0 || count==0));
  for(int i=0;i<inputs;++i) {
    assert(m.get_n(i)==count);
    for(int p=0;p<count;++p) {float x=-1,y=-1;m.get_point(i,p,&x,&y);assert(x==p);assert(y==i*16+p);}
  }
  // Independent linear oracle; positive/negative extrapolation included.
  for(float x : {-2.f,0.f,0.5f,9.f}) {
    float data[9];for(auto& v:data)v=x;float expected=0.25f;
    if(count)for(int i=0;i<inputs;++i)expected+=i*16+x;
    assert(m.calculate(data)==expected);
  }
}
int main() {
  unsigned cases=0;
  for(int inputs=0;inputs<=9;++inputs)for(int count : {0,2,3,4,5,6,7,8}) {
    Mapping original=make(inputs,count);check(original,inputs,count);
    Mapping copied(original);check(copied,inputs,count);
    if(inputs && count)copied.set_point(0,0,0,999);
    check(original,inputs,count); // no alias through copy construction
    Mapping assigned=make(9,8);assigned=original;check(assigned,inputs,count);
    if(inputs && count)assigned.set_point(0,0,0,777);
    check(original,inputs,count); // no alias through copy assignment
    original=original;check(original,inputs,count);
    Mapping moved(make(inputs,count));check(moved,inputs,count);
    Mapping source=make(inputs,count);Mapping move_constructed(std::move(source));check(move_constructed,inputs,count);
    source=make(9,2);check(source,9,2); // reuse moved-from by assignment
    Mapping move_assigned=make(9,8);move_assigned=std::move(move_constructed);check(move_assigned,inputs,count);
    move_constructed=make(0,0);check(move_constructed,0,0);
    std::vector<Mapping> values;
    for(int k=0;k<37;++k)values.push_back(original); // relocation and independent destruction
    for(const auto& v:values)check(v,inputs,count);
    values.clear();check(original,inputs,count);
    for(int repeat=0;repeat<50;++repeat){Mapping temp=original;move_assigned=temp;check(move_assigned,inputs,count);}
    ++cases;
  }
  // Clearing a curve updates the active-input counter on independent values.
  Mapping a=make(9,8), b=a;
  for(int i=0;i<9;++i)a.set_n(i,0);
  check(a,9,0);check(b,9,8);
  std::cout << "PASS ownership scenarios=" << cases << " clear-counter=1\n";
}
