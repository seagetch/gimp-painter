/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Bitmap transform + Surface comparison against actual old runtime capture.
 * GIMP resource selection and generated-brush transforms are separate tests. */
#include "gegl-surface.hpp"
#include "legacy-mask-transform.hpp"
#include <cstdio>
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <sstream>
using namespace GimpPainter::MyPaint;
static std::map<std::string,ShapeMask> masks;
static void dump (const std::string& name, GeglBuffer *buffer)
{
  std::vector<guchar> p(23*17*4);
  gegl_buffer_get(buffer,GEGL_RECTANGLE(0,0,23,17),1,babl_format("R'G'B'A u8"),p.data(),GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
  std::printf("PIX %s ",name.c_str());for(auto v:p)std::printf("%02x",v);std::puts("");
}
int main (int argc,char **argv)
{
  if(argc!=2)return 2;
  std::ifstream input(argv[1]);std::string line;
  while(std::getline(input,line)) {
    std::istringstream row(line);std::string tag,name,hex;int i;ShapeMask mask;
    row>>tag>>name>>i>>mask.width>>mask.height>>hex;
    for(unsigned j=0;j<hex.size();j+=2)mask.pixels.push_back(std::stoul(hex.substr(j,2),nullptr,16));
    masks[name+"-"+std::to_string(i)]=std::move(mask);
  }
  gegl_init(nullptr,nullptr);
  for(int shape=0;shape<2;++shape)for(int texture=0;texture<2;++texture)for(int floating=0;floating<2;++floating) {
    const std::string name=std::to_string(shape)+"-"+std::to_string(texture)+"-"+std::to_string(floating);
    auto *buffer=gegl_buffer_new(GEGL_RECTANGLE(0,0,23,17),babl_format("R'G'B'A u8"));
    std::vector<guchar> p(23*17*4);
    for(int y=0;y<17;++y)for(int x=0;x<23;++x) {
      const int i=(y*23+x)*4;p[i]=(x*19+y*7)%256;p[i+1]=(x*3+y*29)%256;p[i+2]=(x*13+y*11)%256;p[i+3]=x<4?0:(x*17+y*23)%256;
    }
    gegl_buffer_set(buffer,GEGL_RECTANGLE(0,0,23,17),0,babl_format("R'G'B'A u8"),p.data(),GEGL_AUTO_ROWSTRIDE);
    GeglSurface surface(buffer);surface.set_background(.1,.2,.3);surface.set_non_incremental(floating);surface.set_stroke_opacity(.37);
    int index=0;
    if(shape)surface.set_shape_provider([&](float radius,float hardness,float aspect,float angle){
      ShapeMask source{5,3,{0,30,90,160,255,12,60,120,180,240,255,190,140,70,0}};
      const float scale=radius*2/5, ratio=20*(1.0-(1.0/aspect));
      auto transformed=transform_bitmap_mask(source,scale,ratio,-angle/360,hardness);
      const auto& expected=masks.at(name+"-"+std::to_string(index));
      if(transformed.width!=expected.width||transformed.height!=expected.height||transformed.pixels!=expected.pixels)
        throw std::runtime_error("Legacy bitmap mask mismatch at "+name+"-"+std::to_string(index));
      return transformed;
    });
    if(texture)surface.set_texture({3,2,{20,100,240,200,64,128}});
    surface.begin_session();
    for(int i=0;i<6;++i) {
      index=i;
      const float x=i<3?7.25:i==3?-2.5:i==4?22.2:11.2,y=i<3?8.75:i==3?1.75:i==4?16.1:5.5;
      const float radius=i==1?4.7f:5.3f,hard=i==2?.9f:.55f,aspect=i==0?1.f:2.3f,angle=i==4?-43.f:37.f;
      float cr,cg,cb,ca;
      surface.get_color(x,y,radius,&cr,&cg,&cb,&ca,hard,aspect,angle,-.15,.8);
      std::printf("SAMPLE %s %d %a %a %a %a\n",name.c_str(),i,double(cr),double(cg),double(cb),double(ca));
      bool changed=surface.draw_dab(x,y,radius,.8,.2,.4,.63,hard,i==2?.2f:1.f,aspect,angle,i==5?.45f:0.f,0.f,-.15,.8);
      std::printf("DAB %s %d %d\n",name.c_str(),i,changed);dump(name+"-"+std::to_string(i),buffer);
    }
    surface.end_session();g_object_unref(buffer);
  }
  for(int floating=0;floating<2;++floating) {
    auto*buffer=gegl_buffer_new(GEGL_RECTANGLE(0,0,23,17),babl_format("R'G'B'A u8"));
    std::vector<guchar> hidden(23*17*4);
    for(unsigned i=0;i<hidden.size();i+=4){hidden[i]=33;hidden[i+1]=99;hidden[i+2]=177;hidden[i+3]=0;}
    gegl_buffer_set(buffer,GEGL_RECTANGLE(0,0,23,17),0,babl_format("R'G'B'A u8"),hidden.data(),GEGL_AUTO_ROWSTRIDE);
    GeglSurface surface(buffer);surface.set_non_incremental(floating);surface.begin_session();
    bool changed=surface.draw_dab(11,8,4,.8,.2,.4,.63,.55,1,1,0,1,0,0,1);
    const auto name=std::string("transparent-lock-")+std::to_string(floating);
    std::printf("DAB %s 0 %d\n",name.c_str(),changed);dump(name,buffer);
    surface.end_session();g_object_unref(buffer);
  }
  gegl_exit();
}
