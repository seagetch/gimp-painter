/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "paint/painter-mypaint-surface/gegl-surface.hpp"
#include "paint/painter-mypaint-surface/work-budget.hpp"
#include <limits>
#include <stdexcept>
using namespace GimpPainter::MyPaint;
namespace {
template<class F> void rejected(F f){bool caught=false;try{f();}catch(const std::length_error&){caught=true;}g_assert_true(caught);}
void arithmetic(){
  g_assert_cmpuint(WorkBudget::require(1,1),==,128);
  g_assert_cmpuint(WorkBudget::require(8192,1024),==,WorkBudget::limit);
  rejected([]{WorkBudget::require(8192,1024,1);});
  rejected([]{WorkBudget::require(8193,1024);});
  rejected([]{WorkBudget::require(std::numeric_limits<std::size_t>::max(),2);});
  rejected([]{WorkBudget::require(1,std::numeric_limits<std::size_t>::max());});
  rejected([]{WorkBudget::require(0,0,WorkBudget::limit+1);});
  g_assert_cmpuint(WorkBudget::require(2266,2266,2266*2266),<,WorkBudget::limit);
  // Tiny independent budget makes admission boundaries executable without
  // requesting a giant allocation; production always uses the default cap.
  g_assert_cmpuint(WorkBudget::require(2,2,7,519),==,519);
  rejected([]{WorkBudget::require(2,2,8,519);});
}
guint64 tile_bytes(){guint64 n=0;g_object_get(gegl_stats(),"tile-alloc-total",&n,nullptr);return n;}
void shape(GeglSurface&s,int n){s.set_shape_provider([n](float,float,float,float){return ShapeMask{n,n,std::vector<guchar>(std::size_t(n)*n,255)};});}
void sparse(){
 for(bool wide:{false,true})for(bool noninc:{false,true}){
  const GeglRectangle extent={-500000,-500000,1000000,1000000};
  const auto*fmt=babl_format(wide?"R'G'B'A double":"R'G'B'A u8");
  const auto before=tile_bytes();auto*target=gegl_buffer_new(&extent,fmt);auto*snapshot=gegl_buffer_new(&extent,fmt);
  {GeglSurface s(target);s.set_non_incremental(noninc);s.begin_session_from(snapshot);
   // Actual pixel tile allocation, not a claim about GEGL metadata or cache.
   g_assert_cmpuint(tile_bytes()-before,<,1024*1024);
   shape(s,3);g_assert_true(s.draw_dab(0,0,800,1,0,0,1,1));
   const std::size_t bpp=wide?32:4;
   g_assert_cmpuint(s.bytes_read(),==,9*bpp*(noninc?3:1));
   g_assert_cmpuint(s.bytes_written(),==,9*bpp*(noninc?2:1));
   g_assert_cmpint(s.dirty().width,==,3);g_assert_cmpint(s.dirty().height,==,3);
   g_assert_cmpuint(tile_bytes()-before,<,16*1024*1024);
   g_test_message("million-square sparse extent: wide=%d nonincremental=%d tile bytes=%" G_GUINT64_FORMAT,wide,noninc,tile_bytes()-before);
   s.end_session();}
  g_object_unref(snapshot);g_object_unref(target);
 }
}
void reject_retry(){
 for(bool wide:{false,true}){
  const GeglRectangle extent={-3000,-3000,6000,6000};const auto*fmt=babl_format(wide?"R'G'B'A double":"R'G'B'A u8");
  auto*t=gegl_buffer_new(&extent,fmt);auto*b=gegl_buffer_new(&extent,fmt);
  {GeglSurface s(t);s.set_non_incremental(true);s.begin_session_from(b);unsigned notifications=0;s.set_dirty_callback([&](const GeglRectangle&){++notifications;});shape(s,4096);
   const auto before=tile_bytes();rejected([&]{s.draw_dab(0,0,800,1,0,0,1,1);});
   g_assert_cmpuint(tile_bytes(),==,before);g_assert_cmpuint(s.bytes_read(),==,0);g_assert_cmpuint(s.bytes_written(),==,0);g_assert_cmpint(s.dirty().width,==,0);g_assert_cmpuint(notifications,==,0);
   float r,g,bl,a;rejected([&]{s.get_color(0,0,800,&r,&g,&bl,&a,1);});g_assert_cmpuint(s.bytes_read(),==,0);
   shape(s,3);g_assert_true(s.draw_dab(0,0,800,1,0,0,1,1));g_assert_cmpuint(notifications,==,1);s.cancel_session();g_assert_cmpuint(notifications,==,2);
  }g_object_unref(b);g_object_unref(t);
 }
}
void selection_clip(){
 const GeglRectangle extent={-3000,-3000,6000,6000}, selected={10,20,3,3};
 auto*t=gegl_buffer_new(&extent,babl_format("R'G'B'A double"));auto*b=gegl_buffer_new(&extent,babl_format("R'G'B'A double"));
 auto*m=gegl_buffer_new(&selected,babl_format("Y double"));const double values[]={1,1,1,1,1,1,1,1,1};gegl_buffer_set(m,&selected,0,babl_format("Y double"),values,GEGL_AUTO_ROWSTRIDE);
 {GeglSurface s(t);s.set_selection(m,11,21);s.begin_session_from(b);shape(s,4096);
  g_assert_true(s.draw_dab(0,0,800,1,0,0,1,1));g_assert_cmpint(s.dirty().x,==,-1);g_assert_cmpint(s.dirty().y,==,-1);g_assert_cmpint(s.dirty().width,==,3);g_assert_cmpint(s.dirty().height,==,3);
  const auto bytes=s.bytes_read();g_assert_false(s.draw_dab(10000,10000,800,1,0,0,1,1));g_assert_cmpuint(s.bytes_read(),==,bytes);s.cancel_session();}
 g_object_unref(m);g_object_unref(b);g_object_unref(t);
}
void clipping(){
 for(bool wide:{false,true}){
  const GeglRectangle rect={-1,-1,3,3};auto*t=gegl_buffer_new(&rect,babl_format(wide?"R'G'B'A double":"R'G'B'A u8"));
  {GeglSurface s(t);s.begin_session();shape(s,4096);g_assert_true(s.draw_dab(0,0,800,1,0,0,1,1));g_assert_cmpint(s.dirty().width,==,3);s.cancel_session();
   s.set_shape_provider({});s.begin_session();g_assert_true(s.draw_dab(0,0,800,1,0,0,1,1,1,1,45));g_assert_cmpint(s.dirty().width,==,3);s.cancel_session();}
  g_object_unref(t);
 }
}
}
int main(int argc,char**argv){gegl_init(&argc,&argv);g_test_init(&argc,&argv,nullptr);g_test_add_func("/work-buffer/arithmetic",arithmetic);g_test_add_func("/work-buffer/sparse",sparse);g_test_add_func("/work-buffer/reject-retry",reject_retry);g_test_add_func("/work-buffer/clipping",clipping);g_test_add_func("/work-buffer/selection-clip",selection_clip);int rc=g_test_run();gegl_exit();return rc;}
