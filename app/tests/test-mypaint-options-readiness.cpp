/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gegl.h>
#include <gtk/gtk.h>
extern "C" {
#include "libgimpconfig/gimpconfig.h"
#include "core/core-types.h"
#include "widgets/widgets-types.h"
#include "core/gimp.h"
#include "core/gimpcontext.h"
#include "core/gimptoolinfo.h"
#include "core/gimppaintermybrush.h"
#include "paint/gimpmypaintoptions.h"
#include "tests.h"
#include "gimp-app-test-utils.h"
gboolean options_readiness_c_consumer(GimpMypaintOptions*);
}
#include "paint/painter-mypaint-surface/gimp-painter-options.hpp"
using namespace GimpPainter;
static void count_notify(GObject*,GParamSpec*,gpointer p){++*static_cast<unsigned*>(p);}
static void count_changed(GObject*,GObject*,gpointer p){++*static_cast<unsigned*>(p);}
static void dead(gpointer p,GObject*){*static_cast<bool*>(p)=true;}
static void assert_ready(GimpMypaintOptions*o,bool expected){g_assert_cmpint(options_readiness_c_consumer(o),==,expected);g_assert_cmpint(MypaintOptionsRef::retain(o).is_ready(),==,expected);}
static void recurse(GimpContext*c,GimpPainterMybrush*b,gpointer p){++*static_cast<unsigned*>(p);gimp_context_set_painter_mybrush(c,b);}
static void dispose_request(GimpContext*c,GimpPainterMybrush*,gpointer){g_object_run_dispose(G_OBJECT(c));}
static void drop_request(GimpContext*,GimpPainterMybrush*,gpointer p){g_clear_object(static_cast<GimpMypaintOptions**>(p));}
static void ready_observer(GObject*o,gpointer p){assert_ready(GIMP_MYPAINT_OPTIONS(o),true);++*static_cast<unsigned*>(p);}
struct Toggle {bool armed=false;unsigned calls=0;bool dispose=false;};
static void toggled(gpointer p,GObject*o,gboolean last){auto&t=*static_cast<Toggle*>(p);if(t.armed){++t.calls;if(!last&&t.dispose)g_object_run_dispose(o);}}
struct ConstructionProbeState {unsigned action=0,calls=0,pre_calls=0;GimpPainterMybrush*other=nullptr;};
static ConstructionProbeState*construction_probe=nullptr;
struct ReadyConstructionProbe {GimpMypaintOptions parent_instance;};
struct ReadyConstructionProbeClass {GimpMypaintOptionsClass parent_class;};
G_DEFINE_TYPE(ReadyConstructionProbe,ready_construction_probe,GIMP_TYPE_MYPAINT_OPTIONS)
static void construction_changed(GimpContext*object,GimpPainterMybrush*,gpointer p){
 auto&probe=*static_cast<ConstructionProbeState*>(p);if(probe.calls++||probe.action==2)return;
 auto*c=GIMP_CONTEXT(object);gimp_context_set_painter_mybrush(c,probe.action?gimp_context_get_painter_mybrush(c):probe.other);
}
static void probe_constructed(GObject*object){
 g_signal_connect(object,"painter-mybrush-changed",G_CALLBACK(construction_changed),construction_probe);
 G_OBJECT_CLASS(ready_construction_probe_parent_class)->constructed(object);
}
static void probe_brush_changed(GimpContext*context,GimpPainterMybrush*brush){
 if(construction_probe&&construction_probe->action==2&&!construction_probe->pre_calls++)gimp_context_set_painter_mybrush(context,brush);
 auto*parent=GIMP_CONTEXT_CLASS(ready_construction_probe_parent_class);if(parent->painter_mybrush_changed)parent->painter_mybrush_changed(context,brush);
}
static void ready_construction_probe_class_init(ReadyConstructionProbeClass*klass){G_OBJECT_CLASS(klass)->constructed=probe_constructed;GIMP_CONTEXT_CLASS(klass)->painter_mybrush_changed=probe_brush_changed;}
static void ready_construction_probe_init(ReadyConstructionProbe*){}
static void run_case(gconstpointer data){
 unsigned which=GPOINTER_TO_UINT(data);
 if(!g_test_subprocess()){g_test_trap_subprocess(nullptr,30*G_TIME_SPAN_SECOND,GTestSubprocessFlags(0));g_test_trap_assert_passed();return;}
 gimp_test_utils_set_gimp3_directory("GIMP_TESTING_ABS_TOP_SRCDIR","app/tests/gimpdir");auto*app=gimp_init_for_testing();
 g_object_set(app->config,"painter-canvas-ui",TRUE,nullptr);gboolean enabled=FALSE;g_object_get(app->config,"painter-canvas-ui",&enabled,nullptr);g_assert_true(enabled);
 auto*info=GIMP_TOOL_INFO(g_object_new(GIMP_TYPE_TOOL_INFO,nullptr));info->gimp=app;
 auto*options=gimp_mypaint_options_new(info);g_object_unref(info);g_assert_nonnull(options);auto*context=GIMP_CONTEXT(options);
 auto*seed=gimp_context_get_painter_mybrush(context);g_assert_nonnull(seed);assert_ready(options,false);
 const auto before=MypaintOptionsRef::retain(options).snapshot().encode();
 unsigned notices=0,changes=0;g_signal_connect(options,"notify",G_CALLBACK(count_notify),&notices);g_signal_connect(options,"painter-mybrush-changed",G_CALLBACK(count_changed),&changes);
 if(which==0){assert_ready(options,false);GTypeQuery query={};g_type_query(GIMP_TYPE_CONTEXT,&query);g_assert_cmpuint(query.instance_size,==,sizeof(GimpContext));g_assert_cmpuint(query.class_size,==,sizeof(GimpContextClass));g_assert_true(g_type_is_a(GIMP_TYPE_CONTEXT,GIMP_TYPE_CONFIG));}
 else if(which==1||which==12){
  if(which==12)g_object_set(options,"opaque",.321,nullptr);
  auto draft=MypaintOptionsRef::retain(options).snapshot().encode();auto history=gimp_painter_mybrush_options_history_size(options);notices=changes=0;
  gimp_context_set_painter_mybrush(context,seed);assert_ready(options,true);
  g_assert_cmpuint(notices,==,0);g_assert_cmpuint(changes,==,0);g_assert_true(MypaintOptionsRef::retain(options).snapshot().encode()==draft);g_assert_cmpuint(gimp_painter_mybrush_options_history_size(options),==,history);
 }else if(which==2){
  auto*brush=GIMP_PAINTER_MYBRUSH(gimp_painter_mybrush_new(context,"readiness changed"));unsigned observed=0;g_signal_connect(options,"settings-changed",G_CALLBACK(ready_observer),&observed);gimp_context_set_painter_mybrush(context,brush);assert_ready(options,true);g_assert_cmpuint(observed,>,0);g_signal_handlers_disconnect_by_data(options,&observed);g_object_unref(brush);
 }else if(which==3){
  auto*held=GIMP_PAINTER_MYBRUSH(g_object_ref(seed));gimp_context_set_painter_mybrush(context,nullptr);assert_ready(options,false);gimp_context_set_painter_mybrush(context,held);assert_ready(options,true);gimp_context_set_painter_mybrush(context,nullptr);assert_ready(options,true);g_object_unref(held);
 }else if(which==4){
  gimp_context_set_painter_mybrush(context,seed);g_object_run_dispose(G_OBJECT(options));assert_ready(options,true);
 }else if(which==5||which==6){
  auto*parent=gimp_context_new(app,"readiness source",nullptr);gimp_context_set_painter_mybrush(parent,seed);
  if(which==5)gimp_context_copy_property(parent,context,GIMP_CONTEXT_PROP_PAINTER_MYBRUSH);
  else{gimp_context_define_property(context,GIMP_CONTEXT_PROP_PAINTER_MYBRUSH,FALSE);gimp_context_set_parent(context,parent);}
  assert_ready(options,true);g_assert_cmpuint(changes,==,0);g_assert_true(MypaintOptionsRef::retain(options).snapshot().encode()==before);gimp_context_set_parent(context,nullptr);g_object_unref(parent);
 }else if(which==7){
  unsigned calls=0;g_signal_connect(context,"painter-mybrush-reaffirmed",G_CALLBACK(recurse),&calls);gimp_context_set_painter_mybrush(context,seed);g_assert_cmpuint(calls,==,1);assert_ready(options,true);g_assert_cmpuint(changes,==,0);g_signal_handlers_disconnect_by_data(context,&calls);
 }else if(which==8){
  g_signal_connect(context,"painter-mybrush-reaffirmed",G_CALLBACK(dispose_request),nullptr);gimp_context_set_painter_mybrush(context,seed);assert_ready(options,true);g_assert_null(context->gimp);
 }else if(which==9||which==18){
  auto*plain=gimp_context_new(app,"unobserved context",nullptr);gimp_context_set_painter_mybrush(plain,seed);unsigned blocked_calls=0;gulong handler=0;if(which==18){handler=g_signal_connect(plain,"painter-mybrush-reaffirmed",G_CALLBACK(count_changed),&blocked_calls);g_signal_handler_block(plain,handler);}Toggle toggle;g_object_add_toggle_ref(G_OBJECT(plain),toggled,&toggle);g_object_unref(plain);toggle.armed=true;gimp_context_set_painter_mybrush(plain,seed);g_assert_cmpuint(toggle.calls,==,0);g_assert_cmpuint(blocked_calls,==,0);toggle.armed=false;g_object_ref(plain);g_object_remove_toggle_ref(G_OBJECT(plain),toggled,&toggle);if(handler)g_signal_handler_disconnect(plain,handler);g_object_unref(plain);
 }else if(which==10){
  auto*other=GIMP_PAINTER_MYBRUSH(gimp_painter_mybrush_new(context,"stale request"));g_signal_emit_by_name(context,"painter-mybrush-reaffirmed",other);assert_ready(options,false);g_signal_emit_by_name(context,"painter-mybrush-reaffirmed",nullptr);assert_ready(options,false);g_object_run_dispose(G_OBJECT(options));g_signal_emit_by_name(context,"painter-mybrush-reaffirmed",other);assert_ready(options,false);g_object_unref(other);
 }else if(which==11){
  auto*explicit_options=GIMP_MYPAINT_OPTIONS(g_object_new(GIMP_TYPE_MYPAINT_OPTIONS,"gimp",app,"painter-mybrush",seed,nullptr));assert_ready(explicit_options,true);g_object_unref(explicit_options);
 }else if(which==13){
  bool destroyed=false;g_object_weak_ref(G_OBJECT(options),dead,&destroyed);g_signal_connect(context,"painter-mybrush-reaffirmed",G_CALLBACK(drop_request),&options);gimp_context_set_painter_mybrush(context,seed);g_assert_null(options);g_assert_true(destroyed);
 }else if(which==14){
  // Only the context and its Options slot own this brush. Context lease
  // acquisition disposes both owners; a direct subsequent brush ref is stale.
  auto*brush=GIMP_PAINTER_MYBRUSH(gimp_painter_mybrush_new(context,"toggle-only source"));gimp_context_set_painter_mybrush(context,brush);bool brush_dead=false;g_object_weak_ref(G_OBJECT(brush),dead,&brush_dead);g_object_unref(brush);
  Toggle toggle;toggle.dispose=true;g_object_add_toggle_ref(G_OBJECT(options),toggled,&toggle);g_object_unref(options);toggle.armed=true;gimp_context_set_painter_mybrush(context,brush);g_assert_true(brush_dead);g_assert_null(context->gimp);toggle.armed=false;g_object_ref(options);g_object_remove_toggle_ref(G_OBJECT(options),toggled,&toggle);assert_ready(options,true);
 }else if(which==15){g_object_run_dispose(G_OBJECT(options));assert_ready(options,false);}
 else if(which==16||which==17||which==19){
  auto*other=GIMP_PAINTER_MYBRUSH(gimp_painter_mybrush_new(context,"construction replacement"));ConstructionProbeState probe;probe.action=which==19?2:which==17;probe.other=other;construction_probe=&probe;
  auto*child=GIMP_MYPAINT_OPTIONS(g_object_new(ready_construction_probe_get_type(),"gimp",app,"name","readiness construction probe",nullptr));
  g_assert_cmpuint(probe.calls,>,0);assert_ready(child,true);if(which==16)g_assert_true(gimp_context_get_painter_mybrush(GIMP_CONTEXT(child))==other);
  g_signal_handlers_disconnect_by_data(child,&probe);g_object_unref(child);construction_probe=nullptr;g_object_unref(other);
 }
 if(options){g_signal_handlers_disconnect_by_data(options,&notices);g_signal_handlers_disconnect_by_data(options,&changes);g_object_unref(options);}
 gimp_test_utils_set_gimp3_directory("GIMP_TESTING_ABS_TOP_BUILDDIR","app/tests/gimpdir-output");gimp_exit(app,TRUE);
}
int main(int argc,char**argv){g_test_init(&argc,&argv,nullptr);if(!gtk_init_check(&argc,&argv))return GIMP_EXIT_TEST_SKIPPED;
 const char*names[]={"initial-implicit-false","same-source-adopted","changed-adopted-before-observer","null-sticky","dispose-sticky","context-copy","context-inheritance","same-request-reentry","request-dispose","no-observer-no-toggle","stale-manual-signal","explicit-construction-property","same-source-dirty-preserved","request-last-owner","context-lease-disposes-brush","initial-dispose-false","constructor-nested-different","constructor-nested-same","blocked-observer-no-toggle","constructor-before-adoption-same"};for(unsigned i=0;i<20;++i){gchar*path=g_strconcat("/original-options-readiness/",names[i],nullptr);g_test_add_data_func(path,GUINT_TO_POINTER(i),run_case);g_free(path);}return g_test_run();}
