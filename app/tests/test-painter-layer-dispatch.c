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
#include "gimpcoreapp.h"
#include "gimp-app-test-utils.h"
#include "tests.h"

#include "core/gimplayer.h"
#include "painter/gimp-painter-binding.h"

/* Real GTK signals enter production dispatch; only the post-GLib C++
 * continuation is fault injected. No GLib function is claimed to throw. */
static Gimp *gimp;
#include "test-painter-layer-dispatch.h"
static GtkWidget* find(GtkWidget*w,const char*name){
 if(g_strcmp0(gtk_widget_get_name(w),name)==0)return w;
 if(GTK_IS_CONTAINER(w)){GList*l=gtk_container_get_children(GTK_CONTAINER(w));for(GList*p=l;p;p=p->next){GtkWidget*r=find(p->data,name);if(r){g_list_free(l);return r;}}g_list_free(l);}return NULL;
}
static void drain(void){for(int i=0;i<300&&g_main_context_pending(NULL);i++)g_main_context_iteration(NULL,FALSE);}
static void test_dispatch(gconstpointer data){
 int kind=GPOINTER_TO_INT(data);
 GimpImage*image=gimp_image_new(gimp,64,64,GIMP_RGB,GIMP_PRECISION_U8_NON_LINEAR);
 GimpLayer*layer=gimp_layer_new(image,64,64,gimp_image_get_layer_format(image,TRUE),"Dispatch fault",1.0,GIMP_LAYER_MODE_NORMAL);
 gimp_image_add_layer(image,layer,NULL,0,FALSE);
 GtkWidget*window=gtk_window_new(GTK_WINDOW_TOPLEVEL);g_object_ref_sink(window);
 GtkWidget*tiles=gimp_painter_layer_tiles_new(gimp_get_user_context(gimp),image);g_object_ref_sink(tiles);
 gtk_container_add(GTK_CONTAINER(window),tiles);gtk_widget_show_all(window);drain();
 GtkWidget*popup=gimp_painter_layer_tiles_popup(GIMP_PAINTER_LAYER_TILES(tiles),FALSE);g_assert_nonnull(popup);
 GtkWidget*control=find(popup,"painter-layer-opacity");g_assert_nonnull(control);g_object_ref(control);
 g_test_expect_message(NULL,G_LOG_LEVEL_WARNING,"Painter layer callback: *");
 painter_layer_dispatch_arm(G_OBJECT(tiles),kind);gtk_range_set_value(GTK_RANGE(control),23);
 g_test_assert_expected_messages();g_assert_true(painter_layer_dispatch_consumed());g_assert_cmpfloat(gimp_layer_get_opacity(layer),==,1.0);
 gtk_range_set_value(GTK_RANGE(control),24);g_assert_cmpfloat(gimp_layer_get_opacity(layer),==,0.24);
 gtk_container_remove(GTK_CONTAINER(gtk_widget_get_parent(control)),control);
 g_assert_true(gimp_painter_binding_close(G_OBJECT(tiles),NULL));
 painter_layer_dispatch_arm(G_OBJECT(tiles),kind);gtk_range_set_value(GTK_RANGE(control),25);
 g_assert_true(painter_layer_dispatch_unconsumed());painter_layer_dispatch_disarm();g_assert_cmpfloat(gimp_layer_get_opacity(layer),==,0.24);
 gtk_widget_destroy(control);g_object_unref(control);gtk_widget_destroy(window);g_object_unref(window);g_object_unref(tiles);g_object_unref(image);drain();
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

for (int kind=1;kind<=4;kind++) {gchar*path=g_strdup_printf("/dispatch/kind%d",kind);g_test_add_data_func(path,GINT_TO_POINTER(kind),test_dispatch);g_free(path);}


  g_application_run (gimp->app, 0, NULL);
  result = gimp_core_app_get_exit_status (GIMP_CORE_APP (gimp->app));
  g_application_quit (G_APPLICATION (gimp->app));
  g_clear_object (&gimp->app);
  return result;
}
