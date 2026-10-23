/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gegl.h>
#include <gtk/gtk.h>
extern "C" {
#include "core/core-types.h"
#include "widgets/widgets-types.h"
#include "core/gimp.h"
#include "core/gimpcontext.h"
#include "core/gimp-contexts.h"
#include "core/gimptoolinfo.h"
#include "paint/gimpmypaintoptions.h"
#include "tests.h"
#include "gimp-app-test-utils.h"
}
#include "painter/binding-store.hpp"
using namespace GimpPainter;
struct Toggle { GObject*release;unsigned calls=0;bool armed=false; };
static void toggle(gpointer data,GObject*,gboolean last){auto*t=static_cast<Toggle*>(data);if(!last&&t->armed){t->armed=false;t->calls++;g_object_unref(t->release);}}
struct Hook {Gimp*app;unsigned calls=0;};
static gboolean close_application(GSignalInvocationHint*,guint n,const GValue*v,gpointer data){auto*h=static_cast<Hook*>(data);if(n && GIMP_IS_MYPAINT_OPTIONS(g_value_get_object(v))){++h->calls;BindingStore::require(G_OBJECT(h->app)).close();}return TRUE;}
struct RetireOptions {Gimp*app;bool armed=false;unsigned calls=0;gpointer weak=nullptr;};
static void retire_options(gpointer data,GObject*,gboolean last){
 auto*r=static_cast<RetireOptions*>(data);if(!last||!r->armed)return;r->armed=false;
 GObject*found=nullptr;
 for(GList*l=r->app->context_list;l;l=l->next)
  if(GIMP_IS_MYPAINT_OPTIONS(l->data)&&g_strcmp0(gimp_object_get_name(l->data),"Mypaint")==0){found=G_OBJECT(l->data);break;}
 g_assert_nonnull(found);++r->calls;r->weak=found;g_object_add_weak_pointer(found,&r->weak);g_object_run_dispose(found);
}
static void run_case(gconstpointer data){
 guint mode=GPOINTER_TO_UINT(data);
 if(!g_test_subprocess()){g_test_trap_subprocess(nullptr,30*G_TIME_SPAN_SECOND,GTestSubprocessFlags(0));g_test_trap_assert_passed();return;}
 if(mode<2){
  gegl_init(nullptr,nullptr);
  // Minimal valid endpoints isolate acquisition before native construction.
  auto*app=GIMP(g_object_new(GIMP_TYPE_GIMP,nullptr));gimp_contexts_exit(app);auto*info=GIMP_TOOL_INFO(g_object_new(GIMP_TYPE_TOOL_INFO,nullptr));info->gimp=app;
  gpointer weak_app=app,weak_info=info;g_object_add_weak_pointer(G_OBJECT(app),&weak_app);g_object_add_weak_pointer(G_OBJECT(info),&weak_info);
  if(mode==0){
   Toggle t{G_OBJECT(app)};g_object_add_toggle_ref(G_OBJECT(info),toggle,&t);g_object_unref(info);t.armed=true;
   g_assert_null(gimp_mypaint_options_new(info));g_assert_cmpuint(t.calls,==,1);g_assert_null(weak_app);
   info->gimp=nullptr;g_object_remove_toggle_ref(G_OBJECT(info),toggle,&t);g_assert_null(weak_info);
  }else{
   Toggle t{G_OBJECT(info)};g_object_add_toggle_ref(G_OBJECT(app),toggle,&t);g_object_unref(app);t.armed=true;
   g_assert_null(gimp_mypaint_options_new(info));g_assert_cmpuint(t.calls,==,1);g_assert_null(weak_info);
   g_object_remove_toggle_ref(G_OBJECT(app),toggle,&t);g_assert_null(weak_app);
  }
  return;
 }
 gimp_test_utils_set_gimp3_directory("GIMP_TESTING_ABS_TOP_SRCDIR","app/tests/gimpdir");auto*app=gimp_init_for_testing();
 g_object_set(app->config,"painter-canvas-ui",TRUE,nullptr);gboolean enabled=FALSE;g_object_get(app->config,"painter-canvas-ui",&enabled,nullptr);g_assert_true(enabled);
 auto*info=GIMP_TOOL_INFO(g_object_new(GIMP_TYPE_TOOL_INFO,nullptr));info->gimp=app;
 if(mode==3){
  RetireOptions retired{app};g_object_add_toggle_ref(G_OBJECT(info),retire_options,&retired);g_object_unref(info);retired.armed=true;
  auto*options=gimp_mypaint_options_new(info);g_assert_cmpuint(retired.calls,==,1);g_assert_null(options);g_assert_null(retired.weak);
  g_assert_true(BindingStore::require(G_OBJECT(app)).state()==BindingStore::State::active);
  g_object_remove_toggle_ref(G_OBJECT(info),retire_options,&retired);
 }else{
 Hook hook{app};const guint signal=g_signal_lookup("painter-mybrush-changed",GIMP_TYPE_CONTEXT);g_assert_cmpuint(signal,>,0);
 gulong id=g_signal_add_emission_hook(signal,0,close_application,&hook,nullptr);g_assert_cmpuint(id,>,0);
 auto*options=gimp_mypaint_options_new(info);g_signal_remove_emission_hook(signal,id);
 g_assert_cmpuint(hook.calls,==,1);g_assert_null(options);g_assert_true(BindingStore::require(G_OBJECT(app)).state()==BindingStore::State::closed);
 g_object_unref(info);}
 gimp_test_utils_set_gimp3_directory("GIMP_TESTING_ABS_TOP_BUILDDIR","app/tests/gimpdir-output");gimp_exit(app,TRUE);
}
int main(int argc,char**argv){g_test_init(&argc,&argv,nullptr);if(!gtk_init_check(&argc,&argv))return GIMP_EXIT_TEST_SKIPPED;
 g_test_add_data_func("/options-construction-reentry/info-lease-finalizes-app",GUINT_TO_POINTER(0),run_case);
 g_test_add_data_func("/options-construction-reentry/app-lease-releases-info",GUINT_TO_POINTER(1),run_case);
 g_test_add_data_func("/options-construction-reentry/native-selection-closes-app",GUINT_TO_POINTER(2),run_case);
 g_test_add_data_func("/options-construction-reentry/last-info-lease-disposes-options",GUINT_TO_POINTER(3),run_case);
 return g_test_run();}
