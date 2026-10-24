/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gegl.h>
#include <gtk/gtk.h>
extern "C" {
#include "core/core-types.h"
#include "widgets/widgets-types.h"
#include "core/gimp.h"
#include "core/gimptoolinfo.h"
#include "paint/gimpmypaintoptions.h"
#include "tests.h"
#include "gimp-app-test-utils.h"
GimpMypaintBrushMode options_mode_c_consumer(GimpMypaintOptions*);
}
#include "paint/painter-mypaint-surface/gimp-painter-options.hpp"
using namespace GimpPainter;
static void notified(GObject*,GParamSpec*,gpointer p){++*static_cast<unsigned*>(p);}
static void changed(GObject*,gpointer p){++*static_cast<unsigned*>(p);}
static void run_case(gconstpointer data){
 unsigned mode=GPOINTER_TO_UINT(data);
 if(!g_test_subprocess()){g_test_trap_subprocess(nullptr,30*G_TIME_SPAN_SECOND,GTestSubprocessFlags(0));g_test_trap_assert_passed();return;}
 gimp_test_utils_set_gimp3_directory("GIMP_TESTING_ABS_TOP_SRCDIR","app/tests/gimpdir");auto*app=gimp_init_for_testing();
 g_object_set(app->config,"painter-canvas-ui",TRUE,nullptr);gboolean enabled=FALSE;g_object_get(app->config,"painter-canvas-ui",&enabled,nullptr);g_assert_true(enabled);
 auto*info=GIMP_TOOL_INFO(g_object_new(GIMP_TYPE_TOOL_INFO,nullptr));info->gimp=app;
 auto*options=gimp_mypaint_options_new(info);g_assert_nonnull(options);g_object_unref(info);
 if(mode<4){
  g_object_set(options,"brush-mode",mode,nullptr);const auto before=MypaintOptionsRef::retain(options).snapshot().encode();
  unsigned notices=0,settings=0;g_signal_connect(options,"notify",G_CALLBACK(notified),&notices);g_signal_connect(options,"settings-changed",G_CALLBACK(changed),&settings);
  for(unsigned i=0;i<16;++i){g_assert_cmpint(options_mode_c_consumer(options),==,GIMP_MYPAINT_NORMAL);g_assert_cmpint(MypaintOptionsRef::retain(options).get_brush_mode(),==,GIMP_MYPAINT_NORMAL);}
  gint stored=-1;g_object_get(options,"brush-mode",&stored,nullptr);g_assert_cmpint(stored,==,mode);g_assert_true(MypaintOptionsRef::retain(options).snapshot().encode()==before);g_assert_cmpuint(notices,==,0);g_assert_cmpuint(settings,==,0);
  g_signal_handlers_disconnect_by_data(options,&notices);g_signal_handlers_disconnect_by_data(options,&settings);
 }else if(mode==4){
  g_object_run_dispose(G_OBJECT(options));g_assert_cmpint(options_mode_c_consumer(options),==,GIMP_MYPAINT_NORMAL);g_assert_cmpint(MypaintOptionsRef::retain(options).get_brush_mode(),==,GIMP_MYPAINT_NORMAL);
 }else{
  GObject*wrong=G_OBJECT(g_object_new(G_TYPE_OBJECT,nullptr));
  for(auto*o:{static_cast<GimpMypaintOptions*>(nullptr),reinterpret_cast<GimpMypaintOptions*>(wrong)}){g_test_expect_message(nullptr,G_LOG_LEVEL_CRITICAL,"*GIMP_IS_MYPAINT_OPTIONS*failed*");g_assert_cmpint(options_mode_c_consumer(o),==,GIMP_MYPAINT_NORMAL);g_test_assert_expected_messages();}g_object_unref(wrong);
 }
 g_object_unref(options);gimp_test_utils_set_gimp3_directory("GIMP_TESTING_ABS_TOP_BUILDDIR","app/tests/gimpdir-output");gimp_exit(app,TRUE);
}
int main(int argc,char**argv){g_test_init(&argc,&argv,nullptr);if(!gtk_init_check(&argc,&argv))return GIMP_EXIT_TEST_SKIPPED;
 const char*names[]={"stored-normal","stored-normal-erase","stored-lock-alpha","stored-colorize","disposed-still-normal","invalid-default-diagnostic"};for(unsigned i=0;i<6;++i){gchar*p=g_strconcat("/original-options-mode/",names[i],nullptr);g_test_add_data_func(p,GUINT_TO_POINTER(i),run_case);g_free(p);}return g_test_run();}
