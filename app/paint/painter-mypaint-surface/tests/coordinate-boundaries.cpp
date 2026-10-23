/* SPDX-License-Identifier: GPL-3.0-or-later
 * Current half-open GEGL coordinate contract, not unrestricted legacy parity.
 * The old selection upper bound used mask_width-offset as an inclusive end;
 * that extra endpoint is deliberately NOT treated as a safe readable pixel.
 */
#include "paint/painter-mypaint-surface/gegl-surface.hpp"
#include <gegl.h>
#include <cstring>
#include <vector>
#include <utility>
using namespace GimpPainter::MyPaint;
namespace {
const GeglRectangle backing_rect = {-8,-7,16,14};
const GeglRectangle target_rect = {-6,-5,12,10};
struct Fixture {
  bool wide;
  const Babl *format;
  GeglBuffer *backing, *target;
  explicit Fixture(bool w) : wide(w), format(babl_format(w ? "R'G'B'A double" : "R'G'B'A u8")),
    backing(gegl_buffer_new(&backing_rect,format)),
    target(gegl_buffer_create_sub_buffer(backing,&target_rect))
  {
    std::vector<guchar> pixels(std::size_t(backing_rect.width)*backing_rect.height*bpp());
    for(std::size_t i=0;i<pixels.size();i+=bpp()) {
      if(wide) {const double blue[]={0,0,1,1};std::memcpy(pixels.data()+i,blue,sizeof blue);}
      else {const guchar blue[]={0,0,255,255};std::memcpy(pixels.data()+i,blue,sizeof blue);}
    }
    gegl_buffer_set(backing,&backing_rect,0,format,pixels.data(),GEGL_AUTO_ROWSTRIDE);
  }
  ~Fixture(){g_object_unref(target);g_object_unref(backing);}
  int bpp() const{return wide?32:4;}
  std::vector<guchar> read() const {
    std::vector<guchar> p(std::size_t(backing_rect.width)*backing_rect.height*bpp());
    gegl_buffer_get(backing,&backing_rect,1,format,p.data(),GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);return p;
  }
  std::size_t index(int x,int y) const{return std::size_t((y-backing_rect.y)*backing_rect.width+x-backing_rect.x)*bpp();}
};
bool inside(const GeglRectangle&r,int x,int y){return x>=r.x&&y>=r.y&&x<r.x+r.width&&y<r.y+r.height;}
void rect_equal(const GeglRectangle&a,const GeglRectangle&b){g_assert_true(gegl_rectangle_equal(&a,&b));}
void shape(GeglSurface&s,int w,int h){s.set_shape_provider([w,h](float,float,float,float){return ShapeMask{w,h,std::vector<guchar>(std::size_t(w)*h,255)};});}
bool red_dab(GeglSurface&s,float x,float y){return s.draw_dab(x,y,2,1,0,0,1,1);}
void red_pixel(const Fixture&f,const guchar*p){
  if(f.wide){double v[4];std::memcpy(v,p,sizeof v);g_assert_cmpfloat(v[0],==,1);g_assert_cmpfloat(v[1],==,0);g_assert_cmpfloat(v[2],==,0);g_assert_cmpfloat(v[3],==,1);}
  else {const guchar red[]={255,0,0,255};g_assert_cmpmem(p,4,red,4);}
}
void sample_red(GeglSurface&s,float x,float y){float r,g,b,a;s.get_color(x,y,2,&r,&g,&b,&a,1);g_assert_cmpfloat_with_epsilon(r,1,1e-6);g_assert_cmpfloat(g,==,0);g_assert_cmpfloat(b,==,0);g_assert_cmpfloat_with_epsilon(a,1,1e-6);}
void sample_empty(GeglSurface&s,float x,float y){float r,g,b,a;s.get_color(x,y,2,&r,&g,&b,&a,1);g_assert_cmpfloat(r,==,0);g_assert_cmpfloat(g,==,1);g_assert_cmpfloat(b,==,0);g_assert_cmpfloat(a,==,0);}
void selection_edges(){
  for(bool wide:{false,true})for(int sign:{-1,1}) {
    Fixture f(wide);const auto before=f.read();GeglSurface s(f.target);shape(s,7,5);
    // Shape covers [-4,3) x [-3,2), independently of target and mask origins.
    const int ox=sign*9,oy=sign*7;const GeglRectangle local={-3,-2,4,3};
    const GeglRectangle image={local.x+ox,local.y+oy,local.width,local.height};
    auto*mask=gegl_buffer_new(&image,babl_format(wide?"Y double":"Y u8"));
    const double d[]={1,1,1,1, 1,0,0,1, 1,1,1,1};
    const guchar u[]={255,255,255,255, 255,0,0,255, 255,255,255,255};
    gegl_buffer_set(mask,&image,0,babl_format(wide?"Y double":"Y u8"),wide?static_cast<const void*>(d):static_cast<const void*>(u),GEGL_AUTO_ROWSTRIDE);
    s.set_selection(mask,ox,oy);std::vector<GeglRectangle> notifications;
    s.set_dirty_callback([&](const GeglRectangle&r){notifications.push_back(r);});s.begin_session();
    g_assert_true(red_dab(s,0,0));const auto after=f.read();unsigned changed=0;
    for(int y=backing_rect.y;y<backing_rect.y+backing_rect.height;++y)for(int x=backing_rect.x;x<backing_rect.x+backing_rect.width;++x){
      const auto i=f.index(x,y);const bool selected=inside(local,x,y)&&!(y==-1&&(x==-2||x==-1));
      if(selected){red_pixel(f,after.data()+i);++changed;}
      else g_assert_cmpmem(after.data()+i,f.bpp(),before.data()+i,f.bpp());
    }
    g_assert_cmpuint(changed,==,10);rect_equal(s.dirty(),local);g_assert_cmpuint(notifications.size(),==,1);rect_equal(notifications[0],local);
    // Weighted sampling must read the translated mask and ignore the blue holes.
    sample_red(s,0,0);g_assert_true(f.read()==after);g_assert_cmpuint(notifications.size(),==,1);
    s.cancel_session();g_assert_true(f.read()==before);g_assert_cmpuint(notifications.size(),==,2);rect_equal(notifications.back(),local);
    g_test_message("selection edges: %s offset=(%d,%d)",wide?"double":"legacy-u8",ox,oy);g_object_unref(mask);
  }
}
void empty_intersections(){
  // Each touching edge is empty under the current half-open contract. In
  // particular x=-6 and y=-5 belong to target, never to a mask ending there.
  const GeglRectangle locals[]={{6,-5,2,10},{-8,-5,2,10},{-6,5,12,2},{-6,-7,12,2},{20,20,2,2}};
  for(bool wide:{false,true})for(int sign:{-1,1})for(const auto&local:locals){
    Fixture f(wide);const auto before=f.read();GeglSurface s(f.target);shape(s,31,31);
    const int ox=sign*11,oy=-sign*13;GeglRectangle image=local;image.x+=ox;image.y+=oy;
    auto*mask=gegl_buffer_new(&image,babl_format("Y u8"));std::vector<guchar>full(std::size_t(image.width)*image.height,255);gegl_buffer_set(mask,&image,0,babl_format("Y u8"),full.data(),GEGL_AUTO_ROWSTRIDE);
    s.set_selection(mask,ox,oy);unsigned calls=0;s.set_dirty_callback([&](const GeglRectangle&){++calls;});s.begin_session();
    g_assert_false(red_dab(s,0,0));sample_empty(s,0,0);g_assert_cmpuint(s.bytes_read(),==,0);g_assert_cmpuint(s.bytes_written(),==,0);g_assert_cmpint(s.dirty().width,==,0);g_assert_cmpuint(calls,==,0);s.cancel_session();g_assert_true(f.read()==before);g_assert_cmpuint(calls,==,0);g_object_unref(mask);
  }
}
void target_sampling_domain(){
  for(bool wide:{false,true}){
    Fixture f(wide);const auto before=f.read();GeglSurface s(f.target);shape(s,31,31);s.begin_session();g_assert_true(red_dab(s,0,0));rect_equal(s.dirty(),target_rect);
    const auto after=f.read();for(int y=backing_rect.y;y<backing_rect.y+backing_rect.height;++y)for(int x=backing_rect.x;x<backing_rect.x+backing_rect.width;++x){const auto i=f.index(x,y);if(inside(target_rect,x,y))red_pixel(f,after.data()+i);else g_assert_cmpmem(after.data()+i,f.bpp(),before.data()+i,f.bpp());}
    // The blue backing halo is deliberately visible through shared storage;
    // target extent, not storage or zero origin, is the sampling domain.
    sample_red(s,0,0);sample_empty(s,40,40);g_assert_true(f.read()==after);s.cancel_session();g_assert_true(f.read()==before);
  }
}
void overlapping_adjacent_dirty_union(){
  for(bool wide:{false,true})for(bool floating:{false,true}){
    Fixture f(wide);const auto before=f.read();GeglSurface s(f.target);shape(s,3,3);
    s.set_non_incremental(floating);s.set_stroke_opacity(.5f);
    const GeglRectangle local={-5,-4,8,6},image={local.x+9,local.y-7,local.width,local.height};
    auto*mask=gegl_buffer_new(&image,babl_format("Y u8"));
    std::vector<guchar> full(std::size_t(image.width)*image.height,255);
    gegl_buffer_set(mask,&image,0,babl_format("Y u8"),full.data(),GEGL_AUTO_ROWSTRIDE);
    s.set_selection(mask,9,-7);
    const GeglRectangle dabs[]={{-5,-4,3,3},{-3,-3,3,3},{0,-3,3,3}};
    const GeglRectangle unions[]={{-5,-4,3,3},{-5,-4,5,4},{-5,-4,8,4}};
    const int centers[][2]={{-3,-2},{-1,-1},{2,-1}};
    unsigned notifications=0;
    s.set_dirty_callback([&](const GeglRectangle&r){
      g_assert_cmpuint(notifications,<,3);rect_equal(r,dabs[notifications]);
      // The accumulated box is already published when the per-dab observer runs.
      rect_equal(s.dirty(),unions[notifications]);++notifications;
    });
    s.begin_session();
    for(unsigned n=0;n<3;++n){
      const auto previous=f.read();
      g_assert_true(s.draw_dab(centers[n][0],centers[n][1],2,1,0,0,.5f,1));
      rect_equal(s.dirty(),unions[n]);g_assert_cmpuint(notifications,==,n+1);
      const auto current=f.read();unsigned newly_changed=0;
      for(int y=backing_rect.y;y<backing_rect.y+backing_rect.height;++y)
        for(int x=backing_rect.x;x<backing_rect.x+backing_rect.width;++x){
          const auto i=f.index(x,y);bool covered=false,earlier=false;
          for(unsigned j=0;j<=n;++j)covered=covered||inside(dabs[j],x,y);
          for(unsigned j=0;j<n;++j)earlier=earlier||inside(dabs[j],x,y);
          if(!covered)g_assert_cmpmem(current.data()+i,f.bpp(),before.data()+i,f.bpp());
          if(inside(dabs[n],x,y)&&!earlier&&std::memcmp(current.data()+i,previous.data()+i,f.bpp()))++newly_changed;
        }
      g_assert_cmpuint(newly_changed,>,0);
    }
    const auto painted=f.read();const auto reads=s.bytes_read(),writes=s.bytes_written();
    // First touches the target's right edge. Second intersects the target but
    // misses/touches the translated selection. Neither can enlarge dirty state.
    for(const auto&center:{std::pair<int,int>{8,0},{6,4}}){
      g_assert_false(s.draw_dab(center.first,center.second,2,1,0,0,.5f,1));
      rect_equal(s.dirty(),unions[2]);g_assert_cmpuint(notifications,==,3);
      g_assert_cmpuint(s.bytes_read(),==,reads);g_assert_cmpuint(s.bytes_written(),==,writes);
      g_assert_true(f.read()==painted);
    }
    s.end_session();rect_equal(s.dirty(),unions[2]);g_assert_cmpuint(notifications,==,3);
    g_assert_true(f.read()==painted);g_assert_cmpuint(s.bytes_read(),==,reads);g_assert_cmpuint(s.bytes_written(),==,writes);
    s.begin_session();g_assert_cmpint(s.dirty().width,==,0);g_assert_cmpint(s.dirty().height,==,0);
    g_assert_cmpuint(s.bytes_read(),==,0);g_assert_cmpuint(s.bytes_written(),==,0);
    s.cancel_session();g_assert_cmpuint(notifications,==,3);g_assert_true(f.read()==painted);
    g_test_message("dirty overlap/adjacency: %s %s",wide?"double":"legacy-u8",floating?"nonincremental":"incremental");
    g_object_unref(mask);
  }
}
void nonincremental_cancel_dirty_union(){
  for(bool wide:{false,true}){
    Fixture f(wide);const auto before=f.read();GeglSurface s(f.target);shape(s,1,1);s.set_non_incremental(true);s.set_stroke_opacity(.5);
    std::vector<GeglRectangle> notifications;s.set_dirty_callback([&](const GeglRectangle&r){notifications.push_back(r);});s.begin_session();
    // For a 1x1 shape the anchored origin is floor(center-1).
    g_assert_true(s.draw_dab(-4,-3,2,1,0,0,.5,1));g_assert_true(red_dab(s,5,4));g_assert_true(s.draw_dab(-4,-3,2,1,0,0,.5,1));
    const GeglRectangle first={-5,-4,1,1},last={4,3,1,1},both={-5,-4,10,8};
    rect_equal(s.dirty(),both);g_assert_cmpuint(notifications.size(),==,3);rect_equal(notifications[0],first);rect_equal(notifications[1],last);rect_equal(notifications[2],first);
    const auto after=f.read();for(int y=backing_rect.y;y<backing_rect.y+backing_rect.height;++y)for(int x=backing_rect.x;x<backing_rect.x+backing_rect.width;++x){const auto i=f.index(x,y);if(inside(first,x,y)||inside(last,x,y))g_assert_true(std::memcmp(after.data()+i,before.data()+i,f.bpp())!=0);else g_assert_cmpmem(after.data()+i,f.bpp(),before.data()+i,f.bpp());}
    s.cancel_session();g_assert_true(f.read()==before);g_assert_cmpuint(notifications.size(),==,4);rect_equal(notifications.back(),both);
    s.begin_session();g_assert_cmpint(s.dirty().width,==,0);g_assert_cmpuint(s.bytes_read(),==,0);g_assert_cmpuint(s.bytes_written(),==,0);s.cancel_session();g_assert_cmpuint(notifications.size(),==,4);
  }
}
}
#include "write-domains.inc"
int main(int argc,char**argv){g_test_init(&argc,&argv,nullptr);gegl_init(&argc,&argv);g_test_add_func("/coordinates/selection-edges",selection_edges);g_test_add_func("/coordinates/empty-touch-intersections",empty_intersections);g_test_add_func("/coordinates/target-sampling-domain",target_sampling_domain);g_test_add_func("/coordinates/overlapping-adjacent-dirty-union",overlapping_adjacent_dirty_union);g_test_add_func("/coordinates/nonincremental-cancel-dirty-union",nonincremental_cancel_dirty_union);g_test_add_func("/write-domains/144-formats",WriteDomains::format_grid);g_test_add_func("/write-domains/coalesced-dirty-publication",WriteDomains::coalesced_dirty_publication);const int result=g_test_run();gegl_exit();return result;}
