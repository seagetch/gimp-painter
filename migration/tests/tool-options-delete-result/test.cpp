/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gegl.h>
#include <gtk/gtk.h>
#include <glib/gstdio.h>
#include <cstring>
#include <unistd.h>
extern "C" {
#include "libgimpbase/gimpbase.h"
#include "core/core-types.h"
#include "widgets/widgets-types.h"
#include "core/gimp.h"
#include "core/gimpcontext.h"
#include "core/gimpcontainer.h"
#include "core/gimperror.h"
#include "core/gimptoolinfo.h"
#include "core/gimptooloptions.h"
#include "tools/tools-types.h"
#include "tools/gimptool.h"
#include "tools/gimp-tools.h"
#include "tests.h"
}
static void notified(GObject*,GParamSpec*,gpointer data){++*static_cast<unsigned*>(data);}
static void write_file(const gchar*path,const char*text){GError*e=nullptr;g_assert_true(g_file_set_contents(path,text,-1,&e));g_assert_no_error(e);}
static void expect_file(const gchar*path,const char*text){GError*e=nullptr;gchar*data=nullptr;g_assert_true(g_file_get_contents(path,&data,nullptr,&e));g_assert_no_error(e);g_assert_cmpstr(data,==,text);g_free(data);}
static void run_case(gconstpointer data)
{
 const int mode=GPOINTER_TO_INT(data);
 if(!g_test_subprocess()){g_test_trap_subprocess(nullptr,60*G_TIME_SPAN_SECOND,G_TEST_SUBPROCESS_DEFAULT);g_test_trap_assert_passed();return;}
 GError*error=nullptr;gchar*profile=g_dir_make_tmp("painter-delete-proof-XXXXXX",&error);g_assert_no_error(error);g_assert_nonnull(profile);
 g_setenv("GIMP3_DIRECTORY",profile,TRUE);
 auto*g=gimp_init_for_testing();g_assert_cmpstr(gimp_directory(),==,profile);
 g_assert_cmpint(gimp_container_get_n_children(g->tool_info_list),==,0);
 auto*info=gimp_tool_info_new(g,GIMP_TYPE_TOOL,GIMP_TYPE_TOOL_OPTIONS,static_cast<GimpContextPropMask>(0),"delete-proof-tool","Delete proof","Delete proof","Delete proof",nullptr,nullptr,"delete-proof","gimp-paintbrush","gimp-tool-paintbrush");g_assert_nonnull(info);
 gimp_container_add(g->tool_info_list,GIMP_OBJECT(info));auto*options=info->tool_options;g_assert_nonnull(options);
 auto*file=gimp_tool_info_get_options_file(info,nullptr);gchar*path=g_file_get_path(file);gchar*parent=g_path_get_dirname(path);
 g_assert_true(g_str_has_prefix(path,profile));g_assert_cmpint(g_mkdir_with_parents(parent,0700),==,0);
 gchar*sentinel=g_build_filename(profile,"unrelated-state",nullptr);write_file(sentinel,"unchanged user state");
 const bool caller=mode>=5;const int failure=mode%5;const bool good=failure<2;
 gchar*child=g_build_filename(path,"do-not-remove",nullptr);
 if(failure==0)write_file(path,"saved options bytes");
 if(failure==2){g_assert_cmpint(g_mkdir(path,0700),==,0);write_file(child,"preserved child");}
 if(failure==3){g_assert_cmpint(g_rmdir(parent),==,0);write_file(parent,"not a directory");}
 if(failure==4){g_assert_cmpint(geteuid(),!=,0);write_file(path,"permission-protected options");g_assert_cmpint(g_chmod(parent,0500),==,0);}
 gimp_context_set_opacity(GIMP_CONTEXT(options),0.375);unsigned notifications=0;g_signal_connect(options,"notify",G_CALLBACK(notified),&notifications);
 gboolean result=caller?gimp_tools_clear(g,&error):gimp_tool_options_delete(options,&error);
 g_assert_cmpint(result,==,good);
 if(good){g_assert_no_error(error);g_assert_false(g_file_query_exists(file,nullptr));}
 else {g_assert_nonnull(error);g_assert_error(error,GIMP_ERROR,GIMP_FAILED);g_assert_nonnull(strstr(error->message,"delete-proof-tool"));if(failure==4)g_assert_nonnull(strstr(error->message,"Permission denied"));g_clear_error(&error);}
 g_assert_cmpfloat(gimp_context_get_opacity(GIMP_CONTEXT(options)),==,0.375);g_assert_cmpuint(notifications,==,0);expect_file(sentinel,"unchanged user state");
 if(failure==2){expect_file(child,"preserved child");g_assert_cmpint(g_unlink(child),==,0);g_assert_cmpint(g_rmdir(path),==,0);}
 if(failure==3){expect_file(parent,"not a directory");g_assert_cmpint(g_unlink(parent),==,0);g_assert_cmpint(g_mkdir(parent,0700),==,0);}
 if(failure==4){expect_file(path,"permission-protected options");g_assert_cmpint(g_chmod(parent,0700),==,0);g_assert_cmpint(g_unlink(path),==,0);}
 if(caller){
   // Clear success suppresses normal exit save; failure must not set that flag.
   gimp_tools_save(g,TRUE,FALSE);g_assert_cmpint(g_file_query_exists(file,nullptr),==,!good);
   // Explicit always-save is the existing exception, not a rollback policy.
   if(good){gimp_tools_save(g,TRUE,TRUE);g_assert_true(g_file_query_exists(file,nullptr));}
   expect_file(sentinel,"unchanged user state");
 }
 g_object_unref(file);g_free(child);g_free(parent);g_free(path);g_free(sentinel);g_free(profile);
}
int main(int argc,char**argv){g_test_init(&argc,&argv,nullptr);
 const char*names[]={"/tool-options-delete/direct-success","/tool-options-delete/direct-absent","/tool-options-delete/direct-notempty","/tool-options-delete/direct-notdir","/tool-options-delete/direct-permission","/tool-options-delete/caller-success","/tool-options-delete/caller-absent","/tool-options-delete/caller-notempty","/tool-options-delete/caller-notdir","/tool-options-delete/caller-permission"};
 for(int i=0;i<10;++i)g_test_add_data_func(names[i],GINT_TO_POINTER(i),run_case);
 return g_test_run();}
