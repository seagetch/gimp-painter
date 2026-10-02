/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "search.hpp"
#include <vector>
#include <stdexcept>
using namespace GimpPainter::Fill;
namespace {
GeglBuffer* buffer(const GeglRectangle&r,const char*f,const std::vector<guchar>&p){auto*b=gegl_buffer_new(&r,babl_format(f));gegl_buffer_set(b,&r,0,babl_format(f),p.data(),GEGL_AUTO_ROWSTRIDE);return b;}
std::vector<guchar> read(GeglBuffer*b){auto r=*gegl_buffer_get_extent(b);std::vector<guchar>p(std::size_t(r.width)*r.height);gegl_buffer_get(b,&r,1,babl_format("Y u8"),p.data(),GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);return p;}
void finish(Search&job,std::size_t budget=1){while(job.state()!=Search::State::Complete){auto n=job.visited();job.step(budget);g_assert_cmpuint(job.visited()-n,<=,budget);}}
void barriers(){
  GeglRectangle r={0,0,7,5};std::vector<guchar>p(35,40),m(35,255);for(int y=0;y<5;++y)m[y*7+3]=0;
  auto*b=buffer(r,"Y' u8",p);auto*mask=buffer(r,"Y u8",m);auto s=std::make_shared<Snapshot>(b,1,false,1,2);Search::Options o;o.grow=false;
  Search bounded(s,mask,r,1,2,o);finish(bounded);auto out=read(bounded.result());for(int y=0;y<5;++y)for(int x=0;x<7;++x)g_assert_cmpint(out[y*7+x],==,x<3?255:0);
  // The old penalty at threshold255 intentionally allows zero-mask traversal.
  o.threshold=255;Search high(s,mask,r,1,2,o);finish(high);for(auto v:read(high.result()))g_assert_cmpint(v,==,255);
  // A zero-mask wall can be bypassed in the full image, but not bounded ROI.
  m[3]=255;gegl_buffer_set(mask,&r,0,babl_format("Y u8"),m.data(),GEGL_AUTO_ROWSTRIDE);o.threshold=30;GeglRectangle roi={0,1,7,4};Search limited(s,mask,roi,1,2,o);finish(limited);out=read(limited.result());g_assert_cmpint(out[2*7+5],==,0);
  Search global(s,mask,r,1,2,o);finish(global);out=read(global.result());g_assert_cmpint(out[2*7+5],==,255);g_object_unref(b);g_object_unref(mask);
}
void immutable(){
  GeglRectangle r={-4,-3,8,6};std::vector<guchar>p(48,73),m(48,255);auto*b=buffer(r,"Y' u8",p);auto*mask=buffer(r,"Y u8",m);auto s=std::make_shared<Snapshot>(b,1,false,-1,-1);Search::Options o;o.grow=false;Search first(s,mask,r,-1,-1,o);
  std::fill(p.begin(),p.end(),0);gegl_buffer_set(b,&r,0,babl_format("Y' u8"),p.data(),GEGL_AUTO_ROWSTRIDE);gegl_buffer_clear(mask,&r);g_object_unref(mask);g_object_unref(b);
  finish(first,7);for(auto v:read(first.result()))g_assert_cmpint(v,==,255);
  Search second(s,nullptr,r,0,0,o);finish(second,16);g_assert(read(first.result())==read(second.result()));
}
void connectivity_grow(){
  GeglRectangle r={0,0,5,5};std::vector<guchar>p(25,255);p[6]=p[12]=20;auto*b=buffer(r,"Y' u8",p);auto s=std::make_shared<Snapshot>(b,1,false,1,1);Search::Options o;o.grow=false;
  Search search(s,nullptr,r,1,1,o);finish(search);auto out=read(search.result());g_assert_cmpint(out[6],==,255);g_assert_cmpint(out[12],==,0);
  o.grow=true;GeglRectangle roi={1,1,1,1};Search grow(s,nullptr,roi,1,1,o);finish(grow);out=read(grow.result());for(int y=0;y<5;++y)for(int x=0;x<5;++x)g_assert_cmpint(out[y*5+x],==,(x<3&&y<3)?255:0);g_object_unref(b);
}
void cancellation(){
  GeglRectangle r={0,0,512,512};std::vector<guchar>p(512*512,10);auto*b=buffer(r,"Y' u8",p);auto s=std::make_shared<Snapshot>(b,1,false,250,250);Search::Options o;Search search(s,nullptr,r,250,250,o);g_assert_null(search.result());search.step(0);g_assert_cmpuint(search.visited(),==,0);search.step(13);g_assert_cmpuint(search.visited(),==,13);g_assert_null(search.result());search.cancel();g_assert(search.state()==Search::State::Cancelled);search.step(10000);g_assert_cmpuint(search.visited(),==,13);g_assert_null(search.result());g_object_unref(b);
}
void validation(){
  GeglRectangle r={0,0,2,2};std::vector<guchar>p(4,0);auto*b=buffer(r,"Y' u8",p);auto s=std::make_shared<Snapshot>(b,1,false,0,0);Search::Options o;o.threshold=-1;bool caught=false;try{Search bad(s,nullptr,r,0,0,o);}catch(const std::invalid_argument&){caught=true;}g_assert_true(caught);
  o.threshold=30;Search outside(s,nullptr,r,3,0,o);g_assert(outside.state()==Search::State::Complete);for(auto v:read(outside.result()))g_assert_cmpint(v,==,0);
  GeglRectangle overflow={G_MAXINT,0,2,2};caught=false;try{Search bad(s,nullptr,overflow,0,0,o);}catch(const std::invalid_argument&){caught=true;}g_assert_true(caught);
  auto*linear=buffer(r,"Y u8",p);caught=false;try{Snapshot bad(linear,1,false,0,0);}catch(const std::invalid_argument&){caught=true;}g_assert_true(caught);g_object_unref(linear);g_object_unref(b);
}
}
int main(int argc,char**argv){gegl_init(&argc,&argv);g_test_init(&argc,&argv,nullptr);g_test_add_func("/fill/barriers",barriers);g_test_add_func("/fill/immutable",immutable);g_test_add_func("/fill/connectivity-grow",connectivity_grow);g_test_add_func("/fill/cancellation",cancellation);g_test_add_func("/fill/validation",validation);return g_test_run();}
