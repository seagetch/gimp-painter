/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "legacy-generated-mask.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
using namespace GimpPainter::MyPaint;
int main()
{
  for(int shape=0;shape<3;++shape)for(int spikes : {2,5})for(float base_aspect : {1.f,3.5f}) {
    auto base=generate_legacy_mask(GeneratedShape(shape),7.5f,spikes,.65f,base_aspect,23.f);
    for(int i=0;i<6;++i) {
      float radius=i==1?4.7f:5.3f,hard=i==2?.9f:.55f,aspect=i==0?1.f:2.3f,angle=i==4?-43.f:37.f;
      const float scale=radius*2/std::max(base.width,base.height),ratio=20*(1.0-(1.0/aspect));
      const double transformed_aspect=ratio==0?base_aspect:std::min(std::abs(double(ratio))+1,20.);
      const double turns=-angle/360;
      auto mask=generate_legacy_mask(GeneratedShape(shape),7.5f*scale,spikes,.65f*hard,transformed_aspect,23.f+360*turns);
      std::printf("GENERATED %d %d %a %d %d %d ",shape,spikes,double(base_aspect),i,mask.width,mask.height);
      for(auto pixel:mask.pixels)std::printf("%02x",pixel);std::puts("");
    }
  }
}
