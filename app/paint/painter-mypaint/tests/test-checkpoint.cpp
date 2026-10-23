/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "engine.hpp"
#include <glib.h>
#include <cmath>
#include <string>
#include <memory>
#include <new>
#include <cstddef>
using namespace GimpPainter::MyPaint;
static unsigned fail_allocation;
extern "C" void *__real__Znwm(std::size_t);
extern "C" void *__wrap__Znwm(std::size_t size)
{
  if(fail_allocation && --fail_allocation==0) throw std::bad_alloc();
  return __real__Znwm(size);
}
struct Trace : Surface {
  std::string bytes;
  template<class T> void append(const T& v){bytes.append(reinterpret_cast<const char*>(&v),sizeof(v));}
  bool draw_dab(float x,float y,float radius,float r,float g,float b,float opaque,
                float hardness,float alpha,float aspect,float angle,float lock,
                float colorize,float grain,float contrast) override {
    append('D');for(float v:{x,y,radius,r,g,b,opaque,hardness,alpha,aspect,angle,lock,colorize,grain,contrast})append(v);
    return opaque>0 && hardness>0;
  }
  void get_color(float x,float y,float radius,float*r,float*g,float*b,float*a,
                  float hardness,float aspect,float angle,float grain,float contrast) override {
    append('C');for(float v:{x,y,radius,hardness,aspect,angle,grain,contrast})append(v);
    *r=.2f;*g=.5f;*b=.7f;*a=.8f;
  }
  void begin_session()override{} void end_session()override{}
};
static Resource resource(){
 auto r=Resource::decode("{\"version\":3}");
 r.set_base_value(BRUSH_RADIUS_LOGARITHMIC,1.2);r.set_base_value(BRUSH_OPAQUE,1);
 r.set_base_value(BRUSH_DABS_PER_ACTUAL_RADIUS,3);
 r.set_curve(BRUSH_RADIUS_LOGARITHMIC,INPUT_RANDOM,{{0,-.2},{1,.2}});
 r.set_base_value(BRUSH_SMUDGE,.3);return r;
}
static std::string run(Engine&e,int start=0,int end=18){
 Trace t;for(int i=start;i<end;++i){
  const bool split=e.stroke_to(t,20+i*2.5f,30+std::sin(i*.3f)*5.f,i%7?.7f:0.f,.2f,-.3f,.01);
  t.append(split);for(int j=0;j<STATE_COUNT;++j)t.append(e.state(j));
  t.append(e.painting_time());t.append(e.idling_time());t.append(e.stroke_opacity());t.append(e.non_incremental());
 }return t.bytes;
}
static void equal(const std::string&a,const std::string&b){g_assert_cmpuint(a.size(),==,b.size());g_assert_true(a==b);}
static void initial(){Engine e(resource());auto c=e.checkpoint();g_assert_false(c.empty());auto a=run(e);e.restore(c);equal(a,run(e));}
static void midstroke(){Engine e(resource());run(e,0,9);auto c=e.checkpoint();auto a=run(e,9,32);e.restore(c);equal(a,run(e,9,32));e.restore(c);equal(a,run(e,9,32));}
static void changed_settings(){Engine e(resource());run(e,0,7);auto c=e.checkpoint();auto a=run(e,7,24);e.set_base_value(BRUSH_RADIUS_LOGARITHMIC,4);e.set_foreground(.9,.1,.2);e.set_base_value(BRUSH_STROKE_OPACITY,.2);e.reset();run(e);e.restore(c);equal(a,run(e,7,24));}
static void reset_pending(){Engine e(resource());run(e);e.reset();auto c=e.checkpoint();auto a=run(e);e.restore(c);equal(a,run(e));}
static void owner_gone(){Engine::Checkpoint c;std::string a;{Engine e(resource());run(e,0,8);c=e.checkpoint();a=run(e,8,27);}Engine target(Resource::decode("{\"version\":3}"));target.restore(c);equal(a,run(target,8,27));}
static void independent(){Engine e(resource());run(e,0,6);auto c=e.checkpoint();auto copy=c;auto a=run(e,6,21);Engine other(resource());other.restore(c);run(other);e.restore(copy);equal(a,run(e,6,21));}
static void empty(){Engine e(resource());auto c=e.checkpoint();auto a=run(e);e.restore(c);bool failed=false;try{e.restore(Engine::Checkpoint{});}catch(const std::invalid_argument&){failed=true;}g_assert_true(failed);equal(a,run(e));}
static void allocation_failure(){
 Engine e(resource());run(e,0,8);auto saved=e.checkpoint();auto expected=run(e,8,23);
 for(unsigned nth:{1u,2u,4u}) {
  e.restore(saved);bool rejected=false;fail_allocation=nth;
  try{e.checkpoint();}catch(const std::bad_alloc&){rejected=true;}
  fail_allocation=0;g_assert_true(rejected);equal(expected,run(e,8,23));
  e.restore(saved);rejected=false;fail_allocation=nth;
  try{e.restore(saved);}catch(const std::bad_alloc&){rejected=true;}
  fail_allocation=0;g_assert_true(rejected);equal(expected,run(e,8,23));
 }
}
static void reentry(){
 Engine e(resource());auto saved=e.checkpoint();
 struct Reentry : Trace {
  Engine& engine;const Engine::Checkpoint& saved;unsigned rejected=0;
  Reentry(Engine&e,const Engine::Checkpoint&c):engine(e),saved(c){}
  bool draw_dab(float x,float y,float radius,float r,float g,float b,float opaque,float hardness,float alpha,float aspect,float angle,float lock,float colorize,float grain,float contrast) override {
   try{engine.checkpoint();}catch(const std::logic_error&){++rejected;}
   try{engine.restore(saved);}catch(const std::logic_error&){++rejected;}
   return Trace::draw_dab(x,y,radius,r,g,b,opaque,hardness,alpha,aspect,angle,lock,colorize,grain,contrast);
  }
 } surface(e,saved);
 for(int i=0;i<8;++i)e.stroke_to(surface,20+i*3,30,.8,0,0,.01);
 g_assert_cmpuint(surface.rejected,>,0);auto done=e.checkpoint();e.restore(done);
}
int main(int argc,char**argv){g_test_init(&argc,&argv,nullptr);g_test_add_func("/checkpoint/initial",initial);g_test_add_func("/checkpoint/midstroke-random",midstroke);g_test_add_func("/checkpoint/changed-settings",changed_settings);g_test_add_func("/checkpoint/reset-pending",reset_pending);g_test_add_func("/checkpoint/owner-gone",owner_gone);g_test_add_func("/checkpoint/independent",independent);g_test_add_func("/checkpoint/empty-rejected",empty);g_test_add_func("/checkpoint/reentrant-boundary",reentry);g_test_add_func("/checkpoint/allocation-failure",allocation_failure);return g_test_run();}
