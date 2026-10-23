/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"

#include <gegl.h>
#include <gtk/gtk.h>

#include "libgimpbase/gimpbase.h"
#include "libgimpwidgets/gimpwidgets.h"
#include "core/core-types.h"
#include "widgets/widgets-types.h"
#include "display/display-types.h"
#include "core/gimp.h"
#include "core/gimpcontext.h"
#include "core/gimpimage.h"
#include "core/gimplayer-new.h"
#include "display/gimpdisplay.h"
#include "display/gimpdisplayshell.h"
#include "display/gimppaintercanvasui.h"
#include "widgets/gimppainterlayertiles.h"
#include "widgets/gimplayertileview.h"
#include "gimpcoreapp.h"
#include "gimp-app-test-utils.h"
#include "tests.h"

#include "test-painter-widget-exception-fault.h"

static Gimp *gimp;
extern void arm_fault(int);
extern int consumed_fault(void);
static void test_init(gconstpointer data) {
 int raw=GPOINTER_TO_INT(data)>10;
 int kind=GPOINTER_TO_INT(data)%10;
 gpointer result;
 if(kind<4)g_test_expect_message(NULL,G_LOG_LEVEL_WARNING,"Cannot construct painter layer tiles: *");
 arm_fault(kind);
 result=raw ? g_object_new(GIMP_TYPE_PAINTER_LAYER_TILES,NULL) : gimp_layer_tile_view_new();
 if(kind<4)g_test_assert_expected_messages();
 g_assert_cmpint(consumed_fault(),==,1);
 g_test_message("factory result after injected failure: %s",result?"NONNULL":"NULL");
 g_assert_null(result);
}
int
main (int argc, char **argv)
{
  gint result;

  g_test_init (&argc, &argv, NULL);
  if (! gtk_init_check (&argc, &argv))
    return GIMP_EXIT_TEST_SKIPPED;
  gimp_test_utils_setup_menus_path ();
  gimp = gimp_init_for_gui_testing (TRUE);

g_test_add_data_func("/vfunc-init/typed",GINT_TO_POINTER(1),test_init);
  g_test_add_data_func("/vfunc-init/standard",GINT_TO_POINTER(2),test_init);
  g_test_add_data_func("/vfunc-init/allocation",GINT_TO_POINTER(3),test_init);
  g_test_add_data_func("/vfunc-init/unknown",GINT_TO_POINTER(4),test_init);
  g_test_add_data_func("/vfunc-init/raw-unknown",GINT_TO_POINTER(14),test_init);

  g_application_run (gimp->app, 0, NULL);
  result = gimp_core_app_get_exit_status (GIMP_CORE_APP (gimp->app));
  g_application_quit (G_APPLICATION (gimp->app));
  g_clear_object (&gimp->app);
  return result;
}
