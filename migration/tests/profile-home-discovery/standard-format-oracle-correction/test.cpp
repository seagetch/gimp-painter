/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gegl.h>
#include <gio/gio.h>
#include <glib/gstdio.h>
#include <cstring>
#include <string>
extern "C" {
#include "libgimpbase/gimpbase.h"
#include "core/core-types.h"
#include "core/gimp.h"
#include "core/gimp-user-install.h"
#include "tests.h"
gboolean painter_test_install_origin(GimpUserInstall*);
void painter_test_install_source(GimpUserInstall*,const gchar**,gint*,gint*,gboolean*);
}
static std::string read(const std::string&path){gchar*b=nullptr;GError*e=nullptr;g_assert_true(g_file_get_contents(path.c_str(),&b,nullptr,&e));g_assert_no_error(e);std::string out(b);g_free(b);return out;}
static void write(const std::string&path,const char*bytes){GError*e=nullptr;g_assert_true(g_file_set_contents(path.c_str(),bytes,-1,&e));g_assert_no_error(e);}
static void run_case(){
 const char*expected=g_getenv("EXPECTED_SOURCE");const char*target=g_getenv("PROFILE_TARGET");g_assert_nonnull(expected);g_assert_nonnull(target);
 g_assert_cmpstr(g_get_home_dir(),==,g_getenv("HOME"));
 const bool painter=g_strcmp0(g_getenv("PROFILE_CASE"),"migrate-retry")==0;
 const bool migrate=painter||g_strcmp0(g_getenv("PROFILE_CASE"),"migrate-standard")==0;
 GObject*owner=nullptr;
 if(migrate){
   std::string helper=std::string(g_getenv("HOME"))+"/helper-native-profile";
   g_setenv("GIMP3_DIRECTORY",helper.c_str(),TRUE);
   owner=G_OBJECT(gimp_init_for_testing());
 }else owner=G_OBJECT(g_object_new(G_TYPE_OBJECT,nullptr));
 g_setenv("GIMP3_DIRECTORY",target,TRUE);g_assert_cmpstr(gimp_directory(),==,target);
 auto*install=gimp_user_install_new(owner,FALSE);const gchar*selected=nullptr;gint major=-1,minor=-1;gboolean selected_migrate=FALSE;
 painter_test_install_source(install,&selected,&major,&minor,&selected_migrate);
 if(*expected){g_assert_cmpstr(selected,==,expected);g_assert_true(selected_migrate);g_assert_cmpint(major,==,2);g_assert_cmpint(minor,==,g_ascii_strtoll(g_getenv("EXPECTED_MINOR"),nullptr,10));}
 else{g_assert_null(selected);g_assert_false(selected_migrate);}
 if(migrate){
   std::string source=std::string(expected)+"/gimprc";auto source_bytes=read(source);
   g_assert_true(gimp_user_install_run(install,1));
   auto destination=std::string(target)+"/gimprc";auto bytes=read(destination);
   g_assert_cmpint(painter_test_install_origin(install),==,painter);
   if(painter){
   g_assert_nonnull(strstr(bytes.c_str(),"default-painter-mypaint-brush"));
   g_assert_nonnull(strstr(bytes.c_str(),"legacy preserved brush"));
   g_assert_null(strstr(bytes.c_str(),"(default-mypaint-brush"));
   }else g_assert_true(bytes==source_bytes);
   const char*edit="(default-brush \"User destination edit\")\n";write(destination,edit);
   g_assert_true(gimp_user_install_run(install,1));g_assert_true(read(destination)==edit);g_assert_true(read(source)==source_bytes);
 }
 gimp_user_install_free(install);
 if(!migrate)g_object_unref(owner);
}
int main(int argc,char**argv){g_test_init(&argc,&argv,nullptr);g_test_add_func("/profile-discovery/isolated-native",run_case);return g_test_run();}
