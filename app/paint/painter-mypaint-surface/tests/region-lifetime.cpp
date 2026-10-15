/* SPDX-License-Identifier: GPL-3.0-or-later
 * Observable Surface ownership, not a whole-process leak assertion. Private
 * ROI vectors/floating buffers are exercised, not exposed for instrumentation.
 */
#include "paint/painter-mypaint-surface/gegl-surface.hpp"
#include <memory>
#include <stdexcept>
using namespace GimpPainter::MyPaint;
namespace {
const GeglRectangle extent = {0,0,8,8};
struct Finalization {
  unsigned count = 0;
  static void notify(gpointer data, GObject*) { ++static_cast<Finalization*>(data)->count; }
  void watch(GeglBuffer* buffer) { g_object_weak_ref(G_OBJECT(buffer), notify, this); }
  void expect(unsigned expected) const { g_assert_cmpuint(count, ==, expected); }
};
GeglBuffer* buffer(bool wide, bool mask = false) {
  auto* result = gegl_buffer_new(&extent, babl_format(mask ? "Y u8" : wide ? "R'G'B'A double" : "R'G'B'A u8"));
  auto* color = gegl_color_new(mask ? "white" : "blue");
  gegl_buffer_set_color(result, &extent, color); g_object_unref(color); return result;
}
void shape(GeglSurface& surface) {
  surface.set_shape_provider([](float,float,float,float){ return ShapeMask{3,3,std::vector<guchar>(9,255)}; });
}
bool draw(GeglSurface& surface) { return surface.draw_dab(4,4,2,1,0,0,1,1); }
void sample(GeglSurface& surface, bool red) {
  float r,g,b,a; surface.get_color(4,4,2,&r,&g,&b,&a,1);
  g_assert_cmpfloat_with_epsilon(r,red?1:0,1e-6);
  g_assert_cmpfloat_with_epsilon(g,0,1e-6);
  g_assert_cmpfloat_with_epsilon(b,red?0:1,1e-6);
  g_assert_cmpfloat_with_epsilon(a,1,1e-6);
}
template<class Exception, class Function> void throws(Function action) {
  bool caught=false; try { action(); } catch(const Exception&) { caught=true; }
  g_assert_true(caught);
}
void retained_buffers() {
  for(bool wide:{false,true}) for(bool nonincremental:{false,true}) {
    Finalization target_final, mask_final, snapshot_final;
    auto* target=buffer(wide); auto* mask=buffer(wide,true); auto* snapshot=gegl_buffer_dup(target);
    target_final.watch(target); mask_final.watch(mask); snapshot_final.watch(snapshot);
    {
      GeglSurface surface(target); shape(surface); surface.set_selection(mask,0,0);
      surface.set_non_incremental(nonincremental); surface.begin_session_from(snapshot);
      g_object_unref(target); g_object_unref(mask); g_object_unref(snapshot);
      target_final.expect(0); mask_final.expect(0); snapshot_final.expect(0);
      sample(surface,false); g_assert_true(draw(surface)); sample(surface,true);
      surface.cancel_session(); snapshot_final.expect(1); sample(surface,false);
      target_final.expect(0); mask_final.expect(0);
      surface.cancel_session(); surface.end_session(); snapshot_final.expect(1);
      surface.begin_session(); g_assert_true(draw(surface)); surface.end_session(); sample(surface,true);
    }
    target_final.expect(1); mask_final.expect(1); snapshot_final.expect(1);
  }
}
void selection_replacement() {
  Finalization target_final, first_final, second_final;
  auto* target=buffer(false); auto* first=buffer(false,true); auto* second=buffer(false,true);
  target_final.watch(target); first_final.watch(first); second_final.watch(second);
  {
    GeglSurface surface(target); g_object_unref(target);
    surface.set_selection(first,0,0); g_object_unref(first); first_final.expect(0);
    // Reusing the borrowed pointer is valid while Surface owns it. Retain must
    // precede release even when the new selection is the old selection.
    surface.set_selection(first,1,1); first_final.expect(0);
    surface.set_selection(second,0,0); first_final.expect(1);
    g_object_unref(second); second_final.expect(0);
    surface.set_selection(nullptr,0,0); second_final.expect(1);
    surface.set_selection(nullptr,0,0); first_final.expect(1); second_final.expect(1);
  }
  target_final.expect(1);
}
void snapshot_transitions() {
  for(bool wide:{false,true}) for(bool nonincremental:{false,true}) for(int ending=0;ending<3;++ending) {
    Finalization target_final, first_final, second_final, rejected_final;
    auto* target=buffer(wide); auto* first=gegl_buffer_dup(target); auto* second=gegl_buffer_dup(target);
    auto* rejected=buffer(wide,true);
    target_final.watch(target); first_final.watch(first); second_final.watch(second); rejected_final.watch(rejected);
    {
      GeglSurface surface(target); shape(surface); surface.set_non_incremental(nonincremental);
      throws<std::invalid_argument>([&]{surface.begin_session_from(target);});
      throws<std::invalid_argument>([&]{surface.begin_session_from(rejected);});
      g_object_unref(rejected); rejected_final.expect(1);
      surface.begin_session_from(first); g_object_unref(first); first_final.expect(0);
      throws<std::logic_error>([&]{surface.begin_session_from(second);});
      first_final.expect(0); second_final.expect(0); g_assert_true(draw(surface));
      surface.end_session(); first_final.expect(1); sample(surface,true);
      surface.begin_session_from(second); g_object_unref(second); g_object_unref(target);
      second_final.expect(0); target_final.expect(0);
      g_assert_true(surface.draw_dab(4,4,2,0,0,1,1,1));
      if(ending==0) { surface.end_session(); second_final.expect(1); sample(surface,false); }
      if(ending==1) { surface.cancel_session(); second_final.expect(1); sample(surface,false); }
      // Active raw Surface destruction releases resources; pixel rollback is
      // a controller responsibility and deliberately not asserted here.
    }
    target_final.expect(1); first_final.expect(1); second_final.expect(1); rejected_final.expect(1);
  }
}
struct Lifetime {
  unsigned& count;
  explicit Lifetime(unsigned& value):count(value){}
  ~Lifetime(){++count;}
};
void provider_replacement() {
  unsigned shape_destroyed=0, texture_destroyed=0, callback_destroyed=0;
  Finalization target_final; auto* target=buffer(false); target_final.watch(target);
  {
    GeglSurface surface(target); g_object_unref(target);
    auto shape_owner=std::make_shared<Lifetime>(shape_destroyed);
    auto texture_owner=std::make_shared<Lifetime>(texture_destroyed);
    auto callback_owner=std::make_shared<Lifetime>(callback_destroyed);
    surface.set_shape_provider([shape_owner](float,float,float,float){return ShapeMask{};});
    surface.set_texture_provider([texture_owner]{return std::make_shared<const Texture>(Texture{1,1,{255}});});
    surface.set_dirty_callback([callback_owner](const GeglRectangle&){});
    shape_owner.reset(); texture_owner.reset(); callback_owner.reset();
    g_assert_cmpuint(shape_destroyed,==,0); g_assert_cmpuint(texture_destroyed,==,0); g_assert_cmpuint(callback_destroyed,==,0);
    surface.begin_session(); g_assert_true(draw(surface)); surface.end_session();
    shape(surface); g_assert_cmpuint(shape_destroyed,==,1);
    surface.set_texture(Texture{1,1,{255}}); g_assert_cmpuint(texture_destroyed,==,1);
    surface.set_dirty_callback({}); g_assert_cmpuint(callback_destroyed,==,1);
    surface.set_shape_provider({}); surface.set_texture_provider({});
  }
  target_final.expect(1); g_assert_cmpuint(shape_destroyed,==,1); g_assert_cmpuint(texture_destroyed,==,1); g_assert_cmpuint(callback_destroyed,==,1);
}
void coverage_unwind() {
  for(bool wide:{false,true}) for(bool nonincremental:{false,true}) for(int failure=0;failure<6;++failure) {
    Finalization target_final, mask_final, snapshot_final;
    auto* target=buffer(wide); auto* mask=buffer(wide,true); auto* snapshot=gegl_buffer_dup(target);
    target_final.watch(target); mask_final.watch(mask); snapshot_final.watch(snapshot);
    unsigned textures_destroyed=0, texture_calls=0;
    std::weak_ptr<const Texture> temporary;
    {
      GeglSurface surface(target); surface.set_selection(mask,0,0); surface.set_non_incremental(nonincremental);
      surface.begin_session_from(snapshot); g_object_unref(target); g_object_unref(mask); g_object_unref(snapshot);
      surface.set_texture_provider([&]() -> std::shared_ptr<const Texture> {
        ++texture_calls;
        if(failure==3) throw std::runtime_error("texture failure");
        auto owned=std::shared_ptr<const Texture>(new Texture{failure==2?2:1,1,{255}},[&](const Texture* p){++textures_destroyed;delete p;});
        temporary=owned; return owned;
      });
      surface.set_shape_provider([failure](float,float,float,float) {
        if(failure==0) throw std::runtime_error("shape failure");
        if(failure==1) return ShapeMask{2,2,{255}};
        return ShapeMask{3,3,std::vector<guchar>(9,255)};
      });
      auto action=[&](bool sampling) {
        float r,g,b,a; const float x=failure==5?100:4, radius=failure==4?.01f:2;
        if(sampling) surface.get_color(x,4,radius,&r,&g,&b,&a,1);
        else g_assert_false(surface.draw_dab(x,4,radius,1,0,0,1,1));
      };
      for(bool sampling:{false,true}) {
        if(failure==0||failure==3) throws<std::runtime_error>([&]{action(sampling);});
        else if(failure==1||failure==2) throws<std::invalid_argument>([&]{action(sampling);});
        else action(sampling);
        g_assert_true(temporary.expired()); target_final.expect(0); mask_final.expect(0); snapshot_final.expect(0);
        g_assert_cmpuint(surface.bytes_written(),==,0); g_assert_cmpint(surface.dirty().width,==,0);
      }
      g_assert_cmpuint(texture_calls,==,2); g_assert_cmpuint(textures_destroyed,==,failure==3?0:2);
      // Zero opacity exits before acquiring coverage resources.
      g_assert_false(surface.draw_dab(4,4,2,1,0,0,0,1)); g_assert_cmpuint(texture_calls,==,2);
      surface.set_texture_provider({}); shape(surface); sample(surface,false);
      g_assert_true(draw(surface)); sample(surface,true); surface.cancel_session(); sample(surface,false);
      snapshot_final.expect(1); target_final.expect(0); mask_final.expect(0);
      surface.begin_session(); g_assert_true(draw(surface)); surface.end_session();
    }
    target_final.expect(1); mask_final.expect(1); snapshot_final.expect(1);
  }
}
void callback_self_clear(bool texture) {
  for(bool wide:{false,true}) for(bool sampling:{false,true}) {
    if(!texture && sampling) continue;
    auto* target=buffer(wide); GeglSurface surface(target); g_object_unref(target);
    shape(surface); unsigned destroyed=0, calls=0;
    auto owner=std::make_shared<Lifetime>(destroyed); std::weak_ptr<Lifetime> weak=owner;
    auto* s=&surface; auto* call_count=&calls;
    if(texture) surface.set_texture_provider([owner,weak,s,call_count] {
      // All values needed after replacement are stack locals: do not read the
      // stored closure after a setter could have destroyed it.
      auto lifetime=weak; auto* current=s; auto* count=call_count;
      ++*count; current->set_texture_provider({});
      g_assert_false(lifetime.expired()); return std::shared_ptr<const Texture>{};
    });
    else surface.set_dirty_callback([owner,weak,s,call_count](const GeglRectangle&) {
      auto lifetime=weak; auto* current=s; auto* count=call_count;
      ++*count; current->set_dirty_callback({}); g_assert_false(lifetime.expired());
    });
    owner.reset(); surface.begin_session();
    if(sampling) sample(surface,false); else g_assert_true(draw(surface));
    g_assert_cmpuint(calls,==,1); g_assert_true(weak.expired()); g_assert_cmpuint(destroyed,==,1);
    surface.cancel_session(); g_assert_cmpuint(calls,==,1); sample(surface,false);
  }
}
void texture_self_clear() { callback_self_clear(true); }
void notify_self_clear() { callback_self_clear(false); }
void notify_exception_recovery() {
  for(bool wide:{false,true}) for(bool nonincremental:{false,true}) {
    Finalization target_final, snapshot_final;
    auto* target=buffer(wide); auto* snapshot=gegl_buffer_dup(target);
    target_final.watch(target); snapshot_final.watch(snapshot);
    {
      GeglSurface surface(target); shape(surface); surface.set_non_incremental(nonincremental);
      surface.begin_session_from(snapshot); g_object_unref(snapshot); g_object_unref(target);
      surface.set_dirty_callback([](const GeglRectangle&){throw std::runtime_error("notify failure");});
      throws<std::runtime_error>([&]{draw(surface);});
      // Writes and dirty publication precede notification. The still-active
      // transaction must remain cancellable after its observer throws.
      sample(surface,true); g_assert_cmpint(surface.dirty().width,>,0);
      snapshot_final.expect(0); target_final.expect(0); surface.set_dirty_callback({});
      surface.cancel_session(); snapshot_final.expect(1); sample(surface,false);
      surface.begin_session(); g_assert_true(draw(surface)); surface.end_session();
    }
    target_final.expect(1); snapshot_final.expect(1);
  }
}
struct CopyObserverState {
  bool armed = false;
  unsigned copies = 0, calls = 0;
};
struct CopyObserver {
  std::shared_ptr<CopyObserverState> state;
  explicit CopyObserver(std::shared_ptr<CopyObserverState> value):state(std::move(value)){}
  CopyObserver(const CopyObserver& other):state(other.state) {
    ++state->copies;
    if(state->armed) throw std::runtime_error("observer copy refused");
  }
  void operator()(const GeglRectangle&) const { ++state->calls; }
};
std::vector<guchar> pixels(GeglBuffer* target) {
  const auto* format=gegl_buffer_get_format(target);
  std::vector<guchar> result(64*babl_format_get_bytes_per_pixel(format));
  gegl_buffer_get(target,&extent,1,format,result.data(),GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
  return result;
}
void notify_copy_before_write() {
  for(bool wide:{false,true}) for(bool nonincremental:{false,true}) for(bool prior:{false,true}) {
    auto* target=buffer(wide);
    {
      GeglSurface surface(target); shape(surface); surface.set_non_incremental(nonincremental);
      const auto initial=pixels(target); surface.begin_session();
      if(prior) g_assert_true(surface.draw_dab(2,2,2,0,1,0,1,1));
      const auto before=pixels(target); const auto dirty=surface.dirty();
      const auto written=surface.bytes_written();
      auto state=std::make_shared<CopyObserverState>();
      surface.set_dirty_callback(CopyObserver(state)); state->armed=true;
      const auto copies=state->copies;
      throws<std::runtime_error>([&]{draw(surface);});
      g_assert_cmpuint(state->copies,==,copies+1); g_assert_cmpuint(state->calls,==,0);
      // Explicit ordinary callable-copy refusal, never failed allocation or OOM.
      // Keep the red test recoverable so every bounded case releases its owners.
      if(pixels(target)!=before || surface.bytes_written()!=written ||
         !gegl_rectangle_equal(&surface.dirty(),&dirty)) {
        g_test_message("observer copy changed pixels/write count/dirty: wide=%d floating=%d prior=%d",wide,nonincremental,prior);
        g_test_fail();
      }
      state->armed=false; surface.set_dirty_callback({}); surface.cancel_session();
      g_assert_true(pixels(target)==initial);
      surface.begin_session(); g_assert_true(draw(surface)); surface.cancel_session();
      g_assert_true(pixels(target)==initial);
    }
    g_object_unref(target);
  }
}
void coverage_replaces_notify() {
  for(bool wide:{false,true}) for(bool nonincremental:{false,true}) {
    auto* target=buffer(wide); GeglSurface surface(target); g_object_unref(target);
    unsigned old_calls=0,new_calls=0;
    surface.set_dirty_callback([&](const GeglRectangle&){++old_calls;});
    surface.set_shape_provider([&](float,float,float,float) {
      surface.set_dirty_callback([&](const GeglRectangle&){++new_calls;});
      return ShapeMask{3,3,std::vector<guchar>(9,255)};
    });
    surface.set_non_incremental(nonincremental); surface.begin_session();
    g_assert_true(draw(surface)); g_assert_cmpuint(old_calls,==,0); g_assert_cmpuint(new_calls,==,1);
    surface.set_dirty_callback({}); surface.cancel_session(); sample(surface,false);
  }
}
void active_destructor_providers() {
  unsigned destroyed=0; Finalization target_final, snapshot_final;
  auto* target=buffer(false); auto* snapshot=gegl_buffer_dup(target);
  target_final.watch(target); snapshot_final.watch(snapshot);
  {
    GeglSurface surface(target); auto owner=std::make_shared<Lifetime>(destroyed);
    surface.set_shape_provider([owner](float,float,float,float){return ShapeMask{};});
    surface.set_texture_provider([owner]{return std::shared_ptr<const Texture>{};});
    surface.set_dirty_callback([owner](const GeglRectangle&){}); owner.reset();
    surface.begin_session_from(snapshot); g_object_unref(snapshot); g_object_unref(target);
    g_assert_true(draw(surface)); g_assert_cmpuint(destroyed,==,0);
  }
  target_final.expect(1); snapshot_final.expect(1); g_assert_cmpuint(destroyed,==,1);
}
}
int main(int argc,char**argv) {
  g_test_init(&argc,&argv,nullptr); gegl_init(&argc,&argv);
  g_test_add_func("/region-lifetime/retained-buffers",retained_buffers);
  g_test_add_func("/region-lifetime/selection-replacement",selection_replacement);
  g_test_add_func("/region-lifetime/snapshot-transitions",snapshot_transitions);
  g_test_add_func("/region-lifetime/provider-replacement",provider_replacement);
  g_test_add_func("/region-lifetime/coverage-unwind",coverage_unwind);
  g_test_add_func("/region-lifetime/active-destructor-providers",active_destructor_providers);
  g_test_add_func("/region-lifetime/texture-self-clear",texture_self_clear);
  g_test_add_func("/region-lifetime/notify-self-clear",notify_self_clear);
  g_test_add_func("/region-lifetime/notify-exception-recovery",notify_exception_recovery);
  g_test_add_func("/region-lifetime/notify-copy-before-write",notify_copy_before_write);
  g_test_add_func("/region-lifetime/coverage-replaces-notify",coverage_replaces_notify);
  const int result=g_test_run(); gegl_exit(); return result;
}
