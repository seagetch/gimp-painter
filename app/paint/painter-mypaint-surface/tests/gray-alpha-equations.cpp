/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Synthetic scalar invariants against the pinned RGBA equations. */
#include "paint/painter-mypaint-surface/gray-alpha-pixels.hpp"
#include <cassert>
#include <initializer_list>
#include <cmath>
#include <cstdio>
using namespace GimpPainter::MyPaint;
using namespace GimpPainter::MyPaint::LegacyPixel;
int main(){
  int comparisons=0;
  for(int a:{0,1,73,180,255})for(int v:{0,37,128,255})for(float amount:{.1f,.55f,1.f})for(int mode=0;mode<3;++mode){
    guchar gray[2]={guchar(v),guchar(a)},rgba[4]={guchar(v),guchar(v),guchar(v),guchar(a)};
    float mask=.8f,color[4]={.72f,.3f,.9f,1.f};
    using It=BrushPixelIteratorForPlainData<ColoredBrushmarkIterator,float,float>;
    It gi(&mask,color,gray,gray,1,1,1,2,2,1,2,2),ri(&mask,color,rgba,rgba,1,1,1,4,4,1,4,4);
    if(mode==0){GrayAlpha::normal(gi,amount);draw_dab_pixels_BlendMode_Normal(ri,amount);}
    else if(mode==1){GrayAlpha::erase(gi,.4f,amount);draw_dab_pixels_BlendMode_Normal_and_Eraser(ri,.4f,amount);}
    else{GrayAlpha::lock_alpha(gi,amount);draw_dab_pixels_BlendMode_LockAlpha(ri,amount);}
    assert(gray[0]==rgba[0]&&gray[1]==rgba[3]);++comparisons;
    float gw=0,gr=0,gg=0,gb=0,ga=0,rw=0,rr=0,rg=0,rb=0,ra=0;
    GrayAlpha::accumulate(gi,&gw,&gr,&gg,&gb,&ga);get_color_pixels_accumulate(ri,&rw,&rr,&rg,&rb,&ra);
    assert(gw==rw&&gr==rr&&ga==ra&&gg==gr&&gb==gr);
  }
  guchar pair[2]={123,231};GrayAlpha::Pixmap mark(pair,nullptr,1,1,2,2);assert(mark.get_brush_alpha()==231);assert(!mark.should_skipped());pair[1]=0;assert(mark.should_skipped());
  std::printf("%d Gray-alpha red/alpha equation and sampling comparisons passed\n",comparisons);
}
