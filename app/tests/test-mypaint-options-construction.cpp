/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gegl.h>
#include <gtk/gtk.h>
extern "C" {
#include "core/core-types.h"
#include "widgets/widgets-types.h"
#include "core/gimp.h"
#include "core/gimpcontext.h"
#include "core/gimptoolinfo.h"
#include "core/gimptooloptions.h"
#include "tools/tools-types.h"
#include "tools/gimptool.h"
#include "paint/gimpmypaintoptions.h"
#include "tests.h"
#include "gimp-app-test-utils.h"
}
#include "paint/painter-mypaint-surface/gimp-painter-options.hpp"
#include "painter/binding-store.hpp"
extern "C" GimpMypaintOptions *options_original_c_constructor(GimpToolInfo*);
using namespace GimpPainter;
static bool fail_ensure;
static gpointer failed_owner;
extern "C" BindingStore& __real__ZN11GimpPainter12BindingStore6ensureEP8_GObject(GObject*);
extern "C" BindingStore& __wrap__ZN11GimpPainter12BindingStore6ensureEP8_GObject(GObject*);
extern "C" BindingStore& __wrap__ZN11GimpPainter12BindingStore6ensureEP8_GObject(GObject*owner){
 if(fail_ensure && GIMP_IS_MYPAINT_OPTIONS(owner)){fail_ensure=false;failed_owner=owner;g_object_add_weak_pointer(owner,&failed_owner);throw std::bad_alloc();}
 return __real__ZN11GimpPainter12BindingStore6ensureEP8_GObject(owner);
}
static void run_case(gconstpointer data) {
 const guint mode=GPOINTER_TO_UINT(data);
 if(!g_test_subprocess()){g_test_trap_subprocess(nullptr,30*G_TIME_SPAN_SECOND,GTestSubprocessFlags(0));g_test_trap_assert_passed();return;}
 gimp_test_utils_set_gimp3_directory("GIMP_TESTING_ABS_TOP_SRCDIR","app/tests/gimpdir");
 Gimp*g=gimp_init_for_testing();g_object_set(g->config,"painter-canvas-ui",TRUE,nullptr);
 gboolean enabled=FALSE;g_object_get(g->config,"painter-canvas-ui",&enabled,nullptr);g_assert_true(enabled);
 GimpToolInfo*info=gimp_tool_info_new(g,GIMP_TYPE_TOOL,GIMP_TYPE_TOOL_OPTIONS,GimpContextPropMask(0),"construction-probe","Probe","Probe","Probe",nullptr,nullptr,"probe","gimp-paintbrush","gimp-tool-paintbrush");
 auto*canonical=info->tool_options;
 if(mode==0){
  GType(*original_type)(void)=gimp_mypaint_options_get_type;
  GimpMypaintOptions*(*original_new)(GimpToolInfo*)=gimp_mypaint_options_new;
  g_assert_cmpuint(original_type(),==,gimp_painter_mybrush_options_get_type());
  for(unsigned i=0;i<16;i++)g_assert_cmpuint(original_type(),==,GIMP_TYPE_MYPAINT_OPTIONS);
  g_assert_true(original_new==gimp_mypaint_options_new);auto*o=options_original_c_constructor(info);g_assert_nonnull(o);g_assert_true(GIMP_IS_MYPAINT_OPTIONS(o));
  g_assert_true(GIMP_IS_MYPAINT_OPTIONS_CLASS(GIMP_MYPAINT_OPTIONS_GET_CLASS(o)));
  g_assert_true(GIMP_MYPAINT_OPTIONS_CLASS(G_OBJECT_GET_CLASS(o))==GIMP_MYPAINT_OPTIONS_GET_CLASS(o));
  g_assert_cmpstr(G_OBJECT_TYPE_NAME(o),==,"GimpPainterMybrushOptions");g_object_unref(o);
 }else if(mode==1){
  auto*a=gimp_mypaint_options_new(info);auto*b=gimp_mypaint_options_new(info);
  g_assert_nonnull(a);g_assert_nonnull(b);g_assert_true(a!=b);g_assert_false(g_object_is_floating(a));
  g_assert_cmpstr(gimp_object_get_name(a),==,"Mypaint");g_assert_true(GIMP_CONTEXT(a)->gimp==g);
  g_assert_null(GIMP_TOOL_OPTIONS(a)->tool_info);g_assert_true(info->tool_options==canonical);
  gdouble before=0,after=0;g_object_get(b,"radius-logarithmic",&before,nullptr);
  g_object_set(a,"radius-logarithmic",before+1,nullptr);g_object_get(b,"radius-logarithmic",&after,nullptr);g_assert_cmpfloat(before,==,after);
  gpointer weak=a;g_object_add_weak_pointer(G_OBJECT(a),&weak);g_object_unref(a);g_assert_null(weak);g_object_unref(b);
 }else if(mode==2){
  gpointer weak=nullptr;{auto owner=MypaintOptionsRef::create(info);weak=owner.get();g_object_add_weak_pointer(G_OBJECT(owner.get()),&weak);g_assert_nonnull(weak);auto snapshot=owner.snapshot();g_assert_false(snapshot.encode().empty());}g_assert_null(weak);
 }else if(mode==3){
  auto*saved=info->gimp;info->gimp=nullptr;g_assert_null(gimp_mypaint_options_new(info));info->gimp=saved;
 }else if(mode==4){
  auto*binding=BindingStore::find(G_OBJECT(g));g_assert_nonnull(binding);binding->close();g_assert_null(gimp_mypaint_options_new(info));
 }else if(mode==5){
  auto*saved=g->config;g->config=nullptr;g_assert_null(gimp_mypaint_options_new(info));g->config=saved;
 }else if(mode==6){
  g_test_expect_message(nullptr,G_LOG_LEVEL_CRITICAL,"*GIMP_IS_TOOL_INFO*failed*");g_assert_null(gimp_mypaint_options_new(nullptr));g_test_assert_expected_messages();
 }else if(mode==8){
  fail_ensure=true;g_assert_null(gimp_mypaint_options_new(info));g_assert_false(fail_ensure);g_assert_null(failed_owner);g_assert_true(info->tool_options==canonical);
 }else if(mode==7){
  auto*o=gimp_mypaint_options_new(info);g_assert_nonnull(o);g_object_run_dispose(G_OBJECT(info));g_assert_true(GIMP_CONTEXT(o)->gimp==g);
  GError*error=nullptr;gchar*json=gimp_painter_mybrush_options_dup_json(o,&error);g_assert_no_error(error);g_assert_nonnull(json);g_free(json);g_object_unref(o);
 }
 g_object_run_dispose(G_OBJECT(info));g_object_unref(info);
 gimp_test_utils_set_gimp3_directory("GIMP_TESTING_ABS_TOP_BUILDDIR","app/tests/gimpdir-output");gimp_exit(g,TRUE);
}
int main(int argc,char**argv){g_test_init(&argc,&argv,nullptr);if(!gtk_init_check(&argc,&argv))return GIMP_EXIT_TEST_SKIPPED;
 const char*names[]={"type-and-signatures","independent-owned-original-name","typed-owner","detached-application","closed-application","missing-config","null-info","outlives-tool-info","native-construction-failure"};
 for(guint i=0;i<G_N_ELEMENTS(names);i++){gchar*name=g_strconcat("/mypaint-options-construction/",names[i],nullptr);g_test_add_data_func(name,GUINT_TO_POINTER(i),run_case);g_free(name);}return g_test_run();}
