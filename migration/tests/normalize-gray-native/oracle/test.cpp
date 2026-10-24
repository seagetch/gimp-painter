#include "filter-normalize-kernel.hpp"
#include <array>
#include <cassert>
#include <cstdio>
extern "C" void old_gray(const unsigned char*,unsigned char*,int,int,int,int);
int main(){unsigned long scenes=0;for(int alpha=0;alpha<2;++alpha)for(unsigned lo=0;lo<256;++lo)for(unsigned hi=lo;hi<256;++hi){
 std::array<unsigned char,14> src{},dst{};const unsigned stride=1+alpha;src.fill(213);
 for(unsigned i=1;i<6;++i){src[i*stride]=lo+(i%(hi-lo+1));if(alpha)src[i*stride+1]=i==3?0:i==4?1:255;}
 src[stride]=lo;src[5*stride]=hi;
 old_gray(src.data(),dst.data(),7,alpha,1,6);
 GimpPainter::FilterNormalizeStats stats;for(unsigned i=1;i<6;++i){auto y=src[i*stride];stats.add({{y,y,y,static_cast<unsigned char>(alpha?src[i*stride+1]:255)}});}
 auto kernel=stats.finish();assert(kernel.status()==GimpPainter::FilterNormalizeStatus::ready);
 for(unsigned i=1;i<6;++i){auto y=src[i*stride];auto a=static_cast<unsigned char>(alpha?src[i*stride+1]:255);auto out=kernel.apply({{y,y,y,a}});unsigned want=lo==hi?y:255*(y-lo)/(hi-lo);for(unsigned c=0;c<3;++c){assert(out[c]==want);assert(out[c]==dst[i*stride]);}assert(out[3]==a);if(alpha)assert(dst[i*stride+1]==a);}
 for(unsigned c=0;c<stride;++c){assert(dst[c]==0xa5);assert(dst[6*stride+c]==0xa5);}++scenes;
 }
 std::printf("PASS %lu Gray/GrayA old-C/equal-RGB/integer-oracle scenes\n",scenes);
}
