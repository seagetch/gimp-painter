/* SPDX-License-Identifier: GPL-3.0-or-later
 * Test-only GNU ELF seams observe real returned buffers, never fabricate OOM.
 * No production ABI, layout, or buffer ownership is changed. */
#include <limits>
namespace InternalRegionAudit {
struct Record { GeglBuffer* pointer; bool alive=true; unsigned finalized=0; };
static bool recording=false;
static std::vector<std::unique_ptr<Record>> records;
static void retired(gpointer data,GObject*) {
  auto* record=static_cast<Record*>(data);record->alive=false;++record->finalized;
}
static void observe(GeglBuffer* buffer) {
  if(!recording || !buffer)return;
  auto record=std::unique_ptr<Record>(new Record{buffer,true,0});
  g_object_weak_ref(G_OBJECT(buffer),retired,record.get());records.push_back(std::move(record));
}
static unsigned live() { unsigned n=0;for(const auto& r:records)n+=r->alive;return n; }
struct Capture {
  Capture(){g_assert_false(recording);g_assert_true(records.empty());recording=true;}
  ~Capture(){recording=false;g_assert_cmpuint(live(),==,0);for(const auto& r:records)g_assert_cmpuint(r->finalized,==,1);records.clear();}
};
}
extern "C" GeglBuffer* __real_gegl_buffer_new(const GeglRectangle*,const Babl*);
extern "C" GeglBuffer* __real_gegl_buffer_dup(GeglBuffer*);
extern "C" GeglBuffer* __wrap_gegl_buffer_new(const GeglRectangle* rect,const Babl* format) {
 auto* result=__real_gegl_buffer_new(rect,format);InternalRegionAudit::observe(result);return result;
}
extern "C" GeglBuffer* __wrap_gegl_buffer_dup(GeglBuffer* buffer) {
 auto* result=__real_gegl_buffer_dup(buffer);InternalRegionAudit::observe(result);return result;
}
namespace InternalRegionAudit {
static void owned_session_buffers() {
 for(bool wide:{false,true})for(bool floating:{false,true})for(int ending:{0,1,2}) {
  Capture capture;
  auto* target=buffer(wide);auto* mask=buffer(wide,true);
  {
   GeglSurface surface(target);shape(surface);surface.set_selection(mask,0,0);surface.set_non_incremental(floating);
   g_object_unref(target);g_object_unref(mask);g_assert_cmpuint(live(),==,2);
   surface.begin_session();g_assert_cmpuint(records.size(),==,floating?4:3);g_assert_cmpuint(live(),==,floating?4:3);
   g_assert_true(draw(surface));sample(surface,true);
   if(ending==0)surface.end_session();else if(ending==1)surface.cancel_session();
   if(ending!=2){g_assert_cmpuint(live(),==,2);surface.end_session();surface.cancel_session();g_assert_cmpuint(live(),==,2);}
  }
  g_assert_cmpuint(live(),==,0);
 }
}
static void rejected_or_throwing_paths() {
 for(bool wide:{false,true})for(bool floating:{false,true})for(int route:{0,1,2,3,4,5,6,7,8}) {
  Capture capture;auto* target=buffer(wide);
  {
   GeglSurface surface(target);g_object_unref(target);surface.set_non_incremental(floating);shape(surface);
   throws<std::logic_error>([&]{draw(surface);});g_assert_cmpuint(live(),==,1);
   surface.begin_session();const auto expected=floating?3:2;g_assert_cmpuint(live(),==,expected);
   if(route==0)g_assert_false(surface.draw_dab(4,4,2,1,0,0,0,1));
   if(route==1)g_assert_false(surface.draw_dab(400,400,2,1,0,0,1,1));
   if(route==2){surface.set_shape_provider([](float,float,float,float)->ShapeMask{throw std::runtime_error("shape failure");});throws<std::runtime_error>([&]{draw(surface);});}
   if(route==3){surface.set_texture_provider([]()->std::shared_ptr<const Texture>{throw std::runtime_error("texture failure");});throws<std::runtime_error>([&]{float r,g,b,a;surface.get_color(4,4,2,&r,&g,&b,&a);});}
   if(route==4){surface.set_shape_provider([](float,float,float,float){return ShapeMask{2,2,{255}};});throws<std::invalid_argument>([&]{draw(surface);});}
   if(route==5){const float nan=std::numeric_limits<float>::quiet_NaN();throws<std::invalid_argument>([&]{surface.draw_dab(4,4,2,nan,0,0,1,1);});float r,g,b,a;throws<std::invalid_argument>([&]{surface.get_color(nan,4,2,&r,&g,&b,&a);});}
   if(route==6){float r,g,b;throws<std::invalid_argument>([&]{surface.get_color(4,4,2,&r,&g,&b,nullptr);});}
   if(route==7){auto* mask=buffer(wide,true);surface.set_selection(mask,100,100);g_object_unref(mask);g_assert_false(draw(surface));float r,g,b,a;surface.get_color(4,4,2,&r,&g,&b,&a);g_assert_cmpfloat(a,==,0);surface.set_selection(nullptr,0,0);}
   if(route==8 && wide)g_assert_false(surface.draw_dab(4,4,2,1,0,0,1,1,1,1,0,0,1));
   g_assert_cmpuint(live(),==,expected);surface.end_session();g_assert_cmpuint(live(),==,1);
  }
 }
}
static void snapshot_replace_and_reject() {
 for(bool wide:{false,true})for(bool floating:{false,true}) {
  Capture capture;auto* target=buffer(wide);auto* before=gegl_buffer_dup(target);
  {
   GeglSurface surface(target);surface.set_non_incremental(floating);shape(surface);
   throws<std::invalid_argument>([&]{surface.begin_session_from(target);});g_assert_cmpuint(live(),==,2);
   surface.begin_session_from(before);g_object_unref(before);g_object_unref(target);
   g_assert_cmpuint(records.size(),==,floating?3:2);g_assert_cmpuint(live(),==,floating?3:2);
   throws<std::logic_error>([&]{surface.begin_session();});g_assert_cmpuint(records.size(),==,floating?3:2);
   g_assert_true(draw(surface));surface.cancel_session();g_assert_cmpuint(live(),==,1);
   surface.begin_session();g_assert_cmpuint(live(),==,floating?3:2);surface.end_session();g_assert_cmpuint(live(),==,1);
  }
 }
}
static void register_tests() {
 g_test_add_func("/region-lifetime/internal-owned-session-buffers",owned_session_buffers);
 g_test_add_func("/region-lifetime/internal-rejected-throwing-paths",rejected_or_throwing_paths);
 g_test_add_func("/region-lifetime/internal-snapshot-transitions",snapshot_replace_and_reject);
}
}
