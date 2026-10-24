#include "filter-video-kernel.hpp"
#include <cstdio>
#include <cstring>
#include <climits>
#include <exception>
using namespace GimpPainter;
static void check(bool x,const char *why){if(!x)throw std::runtime_error(why);}
template<class F>static void rejected(F f,const char *why){try{f();}catch(const std::invalid_argument&){return;}throw std::runtime_error(why);}
int main(){try{
 unsigned invalid=0;
 for(int p:{-1,9,INT_MIN,INT_MAX}){rejected([&]{FilterVideoKernel k({p,0,0},0,0);},"Video bad pattern accepted");invalid++;}
 const std::int64_t bad[]={-1,INT64_MIN,std::int64_t(INT_MAX)+1,INT64_MAX};
 for(auto n:bad)for(bool x:{false,true}){rejected([&]{FilterVideoKernel k({0,0,0},x?n:0,x?0:n);},"Video bad origin accepted");invalid++;}
 FilterVideoOptions options{8,INT_MIN,INT_MAX};FilterVideoKernel snapshot(options,17,29);options={0,0,0};
 check(snapshot.options().pattern==8&&snapshot.options().additive==INT_MIN&&snapshot.options().rotated==INT_MAX&&snapshot.origin_x()==17&&snapshot.origin_y()==29,"Video mutable option alias");
 std::uint8_t input[4]={17,129,253,0},output[4]={5,7,13,17};const std::uint8_t old[4]={5,7,13,17};
 for(auto n:bad)for(bool x:{false,true}){rejected([&]{snapshot.pixel(x?n:0,x?0:n,input,output,4);},"Video bad relative accepted");check(!std::memcmp(output,old,4),"Video invalid wrote output");invalid++;}
 FilterVideoKernel edge({0,1,-1},INT_MAX,INT_MAX);
 for(bool x:{false,true}){rejected([&]{edge.pixel(x?1:0,x?0:1,input,output,4);},"Video origin addition overflow accepted");check(!std::memcmp(output,old,4),"Video overflow wrote output");invalid++;}
 for(unsigned c:{0u,1u,2u,5u}){rejected([&]{snapshot.pixel(0,0,input,output,c);},"Video bad channels accepted");check(!std::memcmp(output,old,4),"Video channel rejection wrote output");invalid++;}
 rejected([&]{snapshot.pixel(0,0,nullptr,output,4);},"Video null input accepted");check(!std::memcmp(output,old,4),"Video null wrote output");invalid++;
 rejected([&]{snapshot.pixel(0,0,input,nullptr,4);},"Video null output accepted");invalid++;
 unsigned aliases=0;
 for(int p=0;p<9;p++)for(unsigned channels:{3u,4u})for(int shift:{-1,0,1}){
  FilterVideoKernel k({p,1,1},17,29);std::uint8_t expected[4]={0,0,0,0};k.pixel(3,5,input,expected,channels);
  std::uint8_t data[12]={};std::memcpy(data+4,input,4);k.pixel(3,5,data+4,data+4+shift,channels);
  check(!std::memcmp(data+4+shift,expected,channels),"Video partial alias mismatch");aliases++;
 }
 check(invalid==28&&aliases==54,"Video contract count");
 std::printf("Video product28 invalid-input no-write rejections,54 exact/partial aliases,immutable original flags/origin PASS\n");
 return 0;
}catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}}
