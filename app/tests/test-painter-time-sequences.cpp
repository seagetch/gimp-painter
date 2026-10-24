/* SPDX-License-Identifier: GPL-3.0-or-later
 * Original23.009: direct registered-tool timestamps, not queued GDK events.
 * The link observer forwards every argument unchanged to the real session.
 */
int frozen_mypaint_main(int argc,char **argv);
#define main frozen_mypaint_main
#include "test-painter-mypaint-tool.cpp"
#undef main
#include "paint/painter-mypaint-surface/gimp-painter-session.h"
namespace TimestampSequence {
struct Sample { double seconds; GimpCoords coords; };
static std::vector<Sample> delivered;
static bool capture=false;
extern "C" gboolean __real_gimp_painter_session_stroke_to(GimpPainterSession*,GimpDrawable*,gdouble,const GimpCoords*,gboolean*,GError**);
extern "C" gboolean __wrap_gimp_painter_session_stroke_to(GimpPainterSession*s,GimpDrawable*d,gdouble seconds,const GimpCoords*c,gboolean*split,GError**error)
{
  if(capture)delivered.push_back({seconds,*c});
  return __real_gimp_painter_session_stroke_to(s,d,seconds,c,split,error);
}
struct Sequence { guint32 start;std::array<guint32,3> times;std::array<guint32,3> milliseconds; };
static const Sequence sequences[]={
 {100,{120,140,160},{20,20,20}},
 {100,{100,120,120},{0,20,0}},
 {100,{90,110,130},{0,20,20}},
 {0xfffffff0u,{4,24,44},{20,20,20}},
 {100,{0x80000064u,0x80000078u,0x8000008cu},{0,20,20}},
 {0,{0,20,40},{0,20,20}},
 {100,{0x80000063u,0x80000077u,0x8000008bu},{0x7fffffffu,20,20}}
};
static std::vector<guchar> evaluate(const Sequence &q)
{
 Scene s;s.time=q.start;auto before=s.pixels();
 delivered.clear();capture=true;s.press();g_assert_cmpuint(delivered.size(),==,1);g_assert_cmpfloat(delivered[0].seconds,==,0);
 for(unsigned i=0;i<3;++i){auto prior_pixels=s.pixels();s.time=q.times[i];s.coords.x=40+i*8;s.coords.pressure=.35+i*.2;gimp_tool_motion(s.tool,&s.coords,s.time,GDK_BUTTON1_MASK,s.display);g_assert_cmpuint(delivered.size(),==,i+2);const auto &sample=delivered.back();g_assert_cmpfloat(sample.seconds,==,q.milliseconds[i]/1000.);g_assert_cmpfloat(sample.coords.x,==,s.coords.x);g_assert_cmpfloat(sample.coords.pressure,==,s.coords.pressure);if(q.milliseconds[i]==0x7fffffffu)g_assert_true(s.pixels()==prior_pixels);}
 capture=false;s.release();auto painted=s.pixels();g_assert_true(painted!=before);unsigned alpha=0;for(unsigned i=3;i<painted.size();i+=4)if(painted[i])++alpha;g_assert_cmpuint(alpha,>,0);g_test_message("painted alpha pixels=%u",alpha);if(q.start==0xfffffff0u){auto *sh=gimp_display_get_shell(s.display);gimp_display_shell_scale(sh,GIMP_ZOOM_TO,5.,GIMP_ZOOM_FOCUS_IMAGE_CENTER);gimp_pickable_flush(GIMP_PICKABLE(gimp_image_get_projection(s.image)));gimp_display_flush_now(s.display);s.capture_oncanvas("timestamp-wrap-oncanvas-portrait.png");}g_assert_cmpint(s.depth(),==,1);g_assert_true(gimp_image_undo(s.image));g_assert_true(s.pixels()==before);g_assert_true(gimp_image_redo(s.image));g_assert_true(s.pixels()==painted);return painted;
}
static void run(gconstpointer value)
{
 const auto &q=sequences[GPOINTER_TO_UINT(value)];auto painted=evaluate(q);Sequence reference=q;reference.start=100;guint32 time=reference.start;for(unsigned i=0;i<3;++i){time+=q.milliseconds[i];reference.times[i]=time;}auto normalized=evaluate(reference);g_assert_true(painted==normalized);
}
}
int main(int argc,char**argv)
{
 g_test_init(&argc,&argv,nullptr);if(!gtk_init_check(&argc,&argv))return GIMP_EXIT_TEST_SKIPPED;gimp_test_utils_setup_menus_path();gimp=gimp_init_for_gui_testing(TRUE);
 const char*names[]={"ordinary","same-time","reverse-resume","wrap","half-range-reject","zero-start","half-range-accepted"};for(unsigned i=0;i<7;++i){auto *name=g_strdup_printf("/timestamp-sequence/%s",names[i]);g_test_add_data_func(name,GUINT_TO_POINTER(i),TimestampSequence::run);g_free(name);}
 g_application_run(gimp->app,0,nullptr);int result=gimp_core_app_get_exit_status(GIMP_CORE_APP(gimp->app));g_application_quit(G_APPLICATION(gimp->app));g_clear_object(&gimp->app);return result;
}
