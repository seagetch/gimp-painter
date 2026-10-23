/* SPDX-License-Identifier: GPL-3.0-or-later */
#define main checkpoint_fixture_main
#include "test-checkpoint.cpp"
#undef main
#include <type_traits>
static_assert(std::is_nothrow_move_constructible<Engine::Prepared>::value,"prepared move");
static_assert(std::is_nothrow_move_assignable<Engine::Prepared>::value,"prepared assignment");
static_assert(std::is_nothrow_move_constructible<Resource>::value,"resource move");
static_assert(std::is_nothrow_move_assignable<Resource>::value,"resource assignment");
static void historical_pair()
{
 Resource before_resource=resource();before_resource.set_group("before");
 Engine engine(before_resource);run(engine,0,6);auto before=engine.checkpoint();
 Resource after_resource=before_resource;after_resource.set_group("after");after_resource.set_base_value(BRUSH_RADIUS_LOGARITHMIC,2);
 engine.configure(after_resource);run(engine,6,12);auto after=engine.checkpoint();
 engine.restore(before);auto before_trace=run(engine,12,29);
 engine.restore(after);auto after_trace=run(engine,12,29);
 // The live controller was reconfigured AFTER publication; this must never
 // become the historical Redo state.
 Resource live=resource();live.set_group("transient");live.set_base_value(BRUSH_RADIUS_LOGARITHMIC,4);engine.configure(live);run(engine);
 auto prepared_before=Engine::prepare(before);auto alternate=Engine::prepare(after);
 Resource prepared_resource=before_resource;Resource alternate_resource=after_resource;
 fail_allocation=1;
 g_assert_true(engine.try_exchange(prepared_before));std::swap(live,prepared_resource);
 g_assert_cmpuint(fail_allocation,==,1);fail_allocation=0;
 equal(before_resource.encode(),live.encode());equal(before_trace,run(engine,12,29));
 // Rewind observation only; real pop does not run samples between cycles.
 engine.restore(before);
 for(unsigned i=0;i<8;++i){
  fail_allocation=1;g_assert_true(engine.try_exchange(alternate));std::swap(live,alternate_resource);g_assert_cmpuint(fail_allocation,==,1);fail_allocation=0;
  equal((i%2?before_resource:after_resource).encode(),live.encode());
  auto current=engine.checkpoint();equal(i%2?before_trace:after_trace,run(engine,12,29));engine.restore(current);
 }
}
static void empty_prepared()
{
 Engine e(resource());auto saved=e.checkpoint();auto expected=run(e);e.restore(saved);
 Engine::Prepared p;g_assert_true(p.empty());fail_allocation=1;g_assert_false(e.try_exchange(p));g_assert_cmpuint(fail_allocation,==,1);fail_allocation=0;equal(expected,run(e));
 bool rejected=false;try{Engine::prepare(Engine::Checkpoint{});}catch(const std::invalid_argument&){rejected=true;}g_assert_true(rejected);
}
static void preparation_failure()
{
 Engine e(resource());run(e,0,7);auto saved=e.checkpoint();auto expected=run(e,7,26);
 for(unsigned nth:{1u,2u,4u}){e.restore(saved);bool rejected=false;fail_allocation=nth;try{Engine::prepare(saved);}catch(const std::bad_alloc&){rejected=true;}fail_allocation=0;g_assert_true(rejected);equal(expected,run(e,7,26));}
 auto prepared=Engine::prepare(saved);auto moved=std::move(prepared);g_assert_true(prepared.empty());g_assert_false(moved.empty());g_assert_true(e.try_exchange(moved));equal(expected,run(e,7,26));
}
static void busy_exchange()
{
 Engine e(resource());auto saved=e.checkpoint();auto prepared=Engine::prepare(saved);
 struct Busy:Trace{Engine&e;Engine::Prepared&p;unsigned rejected=0;Busy(Engine&e,Engine::Prepared&p):e(e),p(p){}
 bool draw_dab(float x,float y,float radius,float r,float g,float b,float opaque,float hardness,float alpha,float aspect,float angle,float lock,float colorize,float grain,float contrast)override{
  fail_allocation=1;g_assert_false(e.try_exchange(p));g_assert_cmpuint(fail_allocation,==,1);fail_allocation=0;++rejected;
  return Trace::draw_dab(x,y,radius,r,g,b,opaque,hardness,alpha,aspect,angle,lock,colorize,grain,contrast);
 }}surface(e,prepared);
 for(int i=0;i<8;++i)e.stroke_to(surface,20+i*3,30,.8,0,0,.01);
 g_assert_cmpuint(surface.rejected,>,0);g_assert_true(e.try_exchange(prepared));auto expected=run(e);e.restore(saved);equal(expected,run(e));
}
int main(int argc,char**argv){g_test_init(&argc,&argv,nullptr);g_test_add_func("/prepared-state/historical-resource-pair",historical_pair);g_test_add_func("/prepared-state/empty-rejected",empty_prepared);g_test_add_func("/prepared-state/preparation-failure-move",preparation_failure);g_test_add_func("/prepared-state/busy-exchange",busy_exchange);return g_test_run();}
