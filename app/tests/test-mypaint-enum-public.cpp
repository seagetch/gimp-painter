/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gegl.h>
#include <gtk/gtk.h>
extern "C" {
#include "libgimpconfig/gimpconfig.h"
#include "core/core-types.h"
#include "widgets/widgets-types.h"
#include "core/gimp.h"
#include "core/gimptoolinfo.h"
#include "paint/gimpmypaintoptions.h"
#include "tests.h"
#include "gimp-app-test-utils.h"
GType mypaint_enum_public_c_check(void);
}
#include "paint/painter-mypaint-surface/gimp-painter-options.hpp"
using namespace GimpPainter;
static void run_case(gconstpointer data){
 bool library_first=GPOINTER_TO_UINT(data)==0;
 if(!g_test_subprocess()){g_test_trap_subprocess(nullptr,30*G_TIME_SPAN_SECOND,GTestSubprocessFlags(0));g_test_trap_assert_passed();return;}
 gegl_init(nullptr,nullptr);
 g_assert_cmpuint(g_type_from_name("GimpMypaintBrushMode"),==,0);GType first=0;
 if(library_first)first=mypaint_enum_public_c_check();
 auto*klass=G_OBJECT_CLASS(g_type_class_ref(GIMP_TYPE_MYPAINT_OPTIONS));auto*spec=g_object_class_find_property(klass,"brush-mode");g_assert_nonnull(spec);
 if(!library_first)first=G_PARAM_SPEC_VALUE_TYPE(spec);
 g_assert_cmpuint(first,==,mypaint_enum_public_c_check());g_assert_cmpuint(G_PARAM_SPEC_VALUE_TYPE(spec),==,first);g_type_class_unref(klass);
 gimp_test_utils_set_gimp3_directory("GIMP_TESTING_ABS_TOP_SRCDIR","app/tests/gimpdir");auto*app=gimp_init_for_testing();g_object_set(app->config,"painter-canvas-ui",TRUE,nullptr);gboolean enabled=FALSE;g_object_get(app->config,"painter-canvas-ui",&enabled,nullptr);g_assert_true(enabled);
 auto*info=GIMP_TOOL_INFO(g_object_new(GIMP_TYPE_TOOL_INFO,nullptr));info->gimp=app;auto*source=gimp_mypaint_options_new(info);auto*target=gimp_mypaint_options_new(info);g_object_unref(info);g_assert_nonnull(source);g_assert_nonnull(target);
 const char*nicks[]={"normal","normal-and-erase","lock-alpha","colorize"};
 for(gint mode=0;mode<4;++mode){
  g_object_set(source,"brush-mode",mode,nullptr);gchar*serialized=gimp_config_serialize_to_string(GIMP_CONFIG(source),nullptr);g_assert_nonnull(serialized);gchar*needle=g_strdup_printf("(brush-mode %s)",nicks[mode]);if(mode==0)g_assert_null(g_strstr_len(serialized,-1,"(brush-mode "));else g_assert_nonnull(g_strstr_len(serialized,-1,needle));g_free(needle);
  GError*error=nullptr;g_assert_true(gimp_config_deserialize_string(GIMP_CONFIG(target),serialized,-1,nullptr,&error));g_assert_no_error(error);g_free(serialized);gint actual=-1;g_object_get(target,"brush-mode",&actual,nullptr);g_assert_cmpint(actual,==,mode);g_assert_cmpint(MypaintOptionsRef::retain(target).get_brush_mode(),==,GIMP_MYPAINT_NORMAL);
 }
 g_object_unref(source);g_object_unref(target);gimp_test_utils_set_gimp3_directory("GIMP_TESTING_ABS_TOP_BUILDDIR","app/tests/gimpdir-output");gimp_exit(app,TRUE);
}
int main(int argc,char**argv){g_test_init(&argc,&argv,nullptr);if(!gtk_init_check(&argc,&argv))return GIMP_EXIT_TEST_SKIPPED;g_test_add_data_func("/mypaint-public-enum/library-first",GUINT_TO_POINTER(0),run_case);g_test_add_data_func("/mypaint-public-enum/app-first",GUINT_TO_POINTER(1),run_case);return g_test_run();}
