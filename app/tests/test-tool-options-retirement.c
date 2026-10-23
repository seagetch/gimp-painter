/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gegl.h>
#include <gtk/gtk.h>
#include "core/core-types.h"
#include "widgets/widgets-types.h"
#include "core/gimp.h"
#include "core/gimpcontainer.h"
#include "core/gimpcontext.h"
#include "core/gimptoolinfo.h"
#include "core/gimptooloptions.h"
#include "tools/tools-types.h"
#include "tools/gimptool.h"
#include "tests.h"
#include "gimp-app-test-utils.h"
typedef struct { GimpToolOptions parent; } RetiredOptions;
typedef struct { GimpToolOptionsClass parent; } RetiredOptionsClass;
GType retired_options_get_type (void);
G_DEFINE_TYPE (RetiredOptions, retired_options, GIMP_TYPE_TOOL_OPTIONS)
static void retired_options_class_init (RetiredOptionsClass *c) { (void)c; }
static void retired_options_init (RetiredOptions *o) { (void)o; }
static void run_case (gconstpointer data)
{
  guint mode=GPOINTER_TO_UINT(data); Gimp *g; GimpToolInfo *own,*other; gpointer weak;
  if (!g_test_subprocess ()) { g_test_trap_subprocess(NULL,30*G_TIME_SPAN_SECOND,0);g_test_trap_assert_passed();return; }
  gimp_test_utils_set_gimp3_directory("GIMP_TESTING_ABS_TOP_SRCDIR","app/tests/gimpdir");g=gimp_init_for_testing();
  g_object_set(g->config,"painter-canvas-ui",TRUE,NULL);
  other=gimp_tool_info_new(g,GIMP_TYPE_TOOL,GIMP_TYPE_TOOL_OPTIONS,0,"other-tool","Other","Other","Other",NULL,NULL,"other","gimp-paintbrush","gimp-tool-paintbrush");
  own=gimp_tool_info_new(g,GIMP_TYPE_TOOL,retired_options_get_type(),0,"retiring-tool","Retiring","Retiring","Retiring",NULL,NULL,"retiring","gimp-paintbrush","gimp-tool-paintbrush");
  gimp_container_add(g->tool_info_list,GIMP_OBJECT(other));gimp_container_add(g->tool_info_list,GIMP_OBJECT(own));
  weak=own;g_object_add_weak_pointer(G_OBJECT(own),&weak);
  if(mode==5) {
    GimpToolOptions *device=g_object_new(GIMP_TYPE_PAINTER_DEVICE_OPTIONS,"gimp",g,"tool-info",other,NULL);
    gimp_context_set_tool(GIMP_CONTEXT(device),other);
    g_assert_true(gimp_context_get_tool(GIMP_CONTEXT(device))==other);
    gimp_container_remove(g->tool_info_list,GIMP_OBJECT(other));
    gimp_context_set_tool(GIMP_CONTEXT(device),own);
    g_assert_true(gimp_context_get_tool(GIMP_CONTEXT(device))==own);
    gimp_container_add(g->tool_info_list,GIMP_OBJECT(other));
    g_object_unref(device);
  }
  if(mode==3) {
    g_test_expect_message("Gimp-Core",G_LOG_LEVEL_WARNING,"*was set to bogus value*it MUST be*");
    gimp_context_set_tool(GIMP_CONTEXT(own->tool_options),other);
    g_test_assert_expected_messages();gimp_context_set_tool(GIMP_CONTEXT(own->tool_options),own);
  }
  if(mode==1)gimp_container_freeze(g->tool_info_list);
  g_assert_true(gimp_container_remove(g->tool_info_list,GIMP_OBJECT(own)));
  if(mode==1)gimp_container_thaw(g->tool_info_list);
  g_assert_true(own->tool_options->tool_info==own);
  g_assert_null(gimp_context_get_tool(GIMP_CONTEXT(own->tool_options)));
  if(mode==4) {
    g_test_expect_message("Gimp-Core",G_LOG_LEVEL_CRITICAL,"*gimp_tool_options_check_tool_info*should not be reached*");
    g_object_set(own->tool_options,"tool",other,NULL);
    g_test_assert_expected_messages();
    g_assert_true(own->tool_options->tool_info==own);
    g_assert_null(gimp_context_get_tool(GIMP_CONTEXT(own->tool_options)));
  }
  if(mode==2) {
    gimp_container_add(g->tool_info_list,GIMP_OBJECT(own));
    gimp_context_set_tool(GIMP_CONTEXT(own->tool_options),own);
    g_assert_true(gimp_context_get_tool(GIMP_CONTEXT(own->tool_options))==own);
    gimp_container_remove(g->tool_info_list,GIMP_OBJECT(own));
  }
  g_object_run_dispose(G_OBJECT(own));g_object_unref(own);g_assert_null(weak);
  gimp_container_remove(g->tool_info_list,GIMP_OBJECT(other));g_object_run_dispose(G_OBJECT(other));g_object_unref(other);
  gimp_test_utils_set_gimp3_directory("GIMP_TESTING_ABS_TOP_BUILDDIR","app/tests/gimpdir-output");gimp_exit(g,TRUE);
}
int main(int argc,char**argv) {
 g_test_init(&argc,&argv,NULL);if(!gtk_init_check(&argc,&argv))return GIMP_EXIT_TEST_SKIPPED;
 g_test_add_data_func("/tool-options-retirement/remove",GUINT_TO_POINTER(0),run_case);
 g_test_add_data_func("/tool-options-retirement/thaw",GUINT_TO_POINTER(1),run_case);
 g_test_add_data_func("/tool-options-retirement/readd",GUINT_TO_POINTER(2),run_case);
 g_test_add_data_func("/tool-options-retirement/live-bogus-rejected",GUINT_TO_POINTER(3),run_case);
 g_test_add_data_func("/tool-options-retirement/retired-deserialization-rejected",GUINT_TO_POINTER(4),run_case);
 g_test_add_data_func("/tool-options-retirement/device-snapshot-any-tool",GUINT_TO_POINTER(5),run_case);
 return g_test_run();
}
