/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Direct Surface comparison against actual pinned GimpDrawable execution.
 * Oracle origin: seagetch/gimp-painter afa43fae3e920210146abed514f136fd49f671b5.
 * Only the legacy capture's transformed bitmap is injected. No current bitmap
 * transform is used, so this comparison isolates selection/Surface semantics.
 */
#include "paint/painter-mypaint-surface/gegl-surface.hpp"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace GimpPainter::MyPaint;
namespace {
void hex(const char*tag,const std::vector<guchar>&values)
{
  std::printf("SEL_%s ",tag);
  for(auto value:values)std::printf("%02x",value);
  std::puts("");
}
std::vector<guchar> read(GeglBuffer*buffer,const GeglRectangle&rect,const Babl*format)
{
  std::vector<guchar> values(std::size_t(rect.width)*rect.height*babl_format_get_bytes_per_pixel(format));
  gegl_buffer_get(buffer,&rect,1,format,values.data(),GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
  return values;
}
ShapeMask captured_mask(const char*path)
{
  std::ifstream input(path);
  if(!input)throw std::runtime_error("Cannot open old transformed-mask input");
  ShapeMask mask;std::string line;
  while(std::getline(input,line)) {
    std::istringstream row(line);std::string tag,bytes;
    row>>tag;if(tag!="SEL_MASK")continue;
    if(mask.width)throw std::runtime_error("Duplicate transformed-mask record");
    row>>mask.width>>mask.height>>bytes;
    if(!row||mask.width<=0||mask.height<=0||bytes.size()!=std::size_t(mask.width)*mask.height*2)
      throw std::runtime_error("Malformed old transformed-mask input");
    for(std::size_t i=0;i<bytes.size();i+=2) {
      std::size_t used=0;auto value=std::stoul(bytes.substr(i,2),&used,16);
      if(used!=2||value>255)throw std::runtime_error("Invalid mask byte");
      mask.pixels.push_back(guchar(value));
    }
  }
  if(!mask.width)throw std::runtime_error("Missing old transformed-mask record");
  return mask;
}
}
int main(int argc,char**argv)
{
  if(argc!=3){std::fprintf(stderr,"Usage: %s CASE OLD_CAPTURE\n",argv[0]);return 2;}
  const int id=std::atoi(argv[1]);if(id<0||id>=10)return 2;
  const bool outside=id>=8,shape=!outside&&(id&1),soft=!outside&&(id&2);
  const int w=outside?64:130,h=outside?48:96,mw=outside?150:96,mh=outside?110:80;
  const int sign=(outside?(id==9):(id&4))?-1:1,ox=7*sign,oy=3*sign;
  const float x=40.25f,y=35.75f,radius=5.3f,hard=.55f;
  const GeglRectangle target_rect={0,0,w,h},mask_rect={0,0,mw,mh};
  gegl_init(nullptr,nullptr);
  const Babl*rgba=babl_format("R'G'B'A u8");const Babl*scalar=babl_format("Y u8");
  auto*target=gegl_buffer_new(&target_rect,rgba);
  auto*selection=gegl_buffer_new(&mask_rect,scalar);
  std::vector<guchar> initial(w*h*4);
  for(int py=0;py<h;++py)for(int px=0;px<w;++px){
    const int i=(py*w+px)*4;
    initial[i]=(px*19+py*7)%256;initial[i+1]=(px*3+py*29)%256;
    initial[i+2]=(px*13+py*11)%256;initial[i+3]=px<35?0:(px*17+py*23)%256;
  }
  gegl_buffer_set(target,&target_rect,0,rgba,initial.data(),GEGL_AUTO_ROWSTRIDE);
  std::vector<guchar> selected(mw*mh);const guchar levels[]={0,1,64,128,254,255};
  std::size_t nonzero=0;
  for(int py=0;py<mh;++py)for(int px=0;px<mw;++px){
    const int index=(px*3+py*5+(px/4)+(py/3))%6;
    const guchar value=outside?(px>=130&&px<140&&py>=90&&py<100?255:0):(soft?levels[index]:(index<3?0:255));
    selected[py*mw+px]=value;nonzero+=value!=0;
  }
  g_assert_cmpuint(nonzero,>,0);
  gegl_buffer_set(selection,&mask_rect,0,scalar,selected.data(),GEGL_AUTO_ROWSTRIDE);
  g_assert_true(read(target,target_rect,rgba)==initial);
  g_assert_true(read(selection,mask_rect,scalar)==selected);
  std::printf("SEL_CASE %d %d %d %d %d %d %d %d %d %d %a %a\n",id,ox,oy,shape,soft,outside,mw,mh,w,h,double(x),double(y));
  hex("INITIAL",initial);hex("SELECTION",read(selection,mask_rect,scalar));
  {
    GeglSurface surface(target);surface.set_non_incremental(false);
    surface.set_stroke_opacity(1);surface.set_background(.1,.2,.3);
    // Application policy: this fixture is a real nonempty selection. An empty
    // application channel would mean no mask, not an explicitly zero buffer.
    surface.set_selection(selection,ox,oy);
    if(shape){
      const auto mask=captured_mask(argv[2]);
      std::printf("SEL_MASK %d %d ",mask.width,mask.height);
      for(auto value:mask.pixels)std::printf("%02x",value);
      std::puts("");
      surface.set_shape_provider([mask](float r,float h,float a,float angle){
        g_assert_cmpfloat(r,==,5.3f);g_assert_cmpfloat(h,==,.55f);
        g_assert_cmpfloat(a,==,1.f);g_assert_cmpfloat(angle,==,0.f);return mask;
      });
    }
    float rr,gg,bb,aa;
    surface.get_color(x,y,radius,&rr,&gg,&bb,&aa,hard,1,0,0,1);
    std::printf("SEL_BEFORE %a %a %a %a\n",double(rr),double(gg),double(bb),double(aa));
    std::vector<GeglRectangle> updates;
    surface.set_dirty_callback([&updates](const GeglRectangle&rect){
      updates.push_back(rect);
      std::printf("SEL_UPDATE %d %d %d %d\n",rect.x,rect.y,rect.width,rect.height);
    });
    surface.begin_session();
    const bool result=surface.draw_dab(x,y,radius,.8,.2,.4,.63,hard,1,1,0,0,0,0,1);
    const auto painted=read(target,target_rect,rgba);
    std::printf("SEL_RETURN %d\n",result);hex("PIXELS",painted);
    surface.get_color(x,y,radius,&rr,&gg,&bb,&aa,hard,1,0,0,1);
    std::printf("SEL_AFTER %a %a %a %a\n",double(rr),double(gg),double(bb),double(aa));
    // Valid legacy-byte regions notify even when all selected coverage is zero.
    // Record comparison checks that ROI independently of complete pixel bytes.
    g_assert_cmpuint(updates.size(),==,1);
    g_assert_true(gegl_rectangle_equal(&surface.dirty(),&updates.front()));
    const auto after=read(selection,mask_rect,scalar);g_assert_true(after==selected);
    g_assert_true(read(target,target_rect,rgba)==painted);
    if(outside){
      const GeglRectangle expected={33,29,14,14};
      g_assert_true(result);
      g_assert_true(gegl_rectangle_equal(&updates.front(),&expected));
      g_assert_true(painted==initial);
    }
    hex("MASK_AFTER",after);surface.end_session();
  }
  g_object_unref(selection);g_object_unref(target);gegl_exit();
  std::puts("SEL_COMPLETE");return 0;
}
