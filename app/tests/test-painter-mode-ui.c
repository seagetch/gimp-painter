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
#include "core/gimplayer.h"
#include "core/gimplayer-new.h"
#include "core/gimpdrawable.h"
#include "display/gimpdisplay.h"
#include "display/gimpdisplayshell.h"
#include "display/gimppaintercanvasui.h"
#include "widgets/gimplayermodecombobox.h"
#include "widgets/gimpdialogfactory.h"
#include "widgets/gimpwidgets-utils.h"
#include "operations/layer-modes-legacy/gimpoperationpainterlegacy.h"
#include "gimpcoreapp.h"
#include "gimp-app-test-utils.h"
#include "tests.h"
static Gimp *gimp;
static GtkWidget *find_combo(GtkWidget *widget)
{
  if(GIMP_IS_LAYER_MODE_COMBO_BOX(widget) &&
     gimp_layer_mode_combo_box_get_context(GIMP_LAYER_MODE_COMBO_BOX(widget))==GIMP_LAYER_MODE_CONTEXT_LAYER)
    return widget;
  if(GTK_IS_CONTAINER(widget)) {
    GList *children=gtk_container_get_children(GTK_CONTAINER(widget));
    GtkWidget *found=NULL;
    for(GList *p=children;p&&!found;p=p->next) found=find_combo(p->data);
    g_list_free(children);return found;
  }
  return NULL;
}
static void mode_selection(gconstpointer data)
{
  static const gchar *labels[]={"Erase (Painter legacy)","Replace (Painter legacy)",
    "Anti erase (Painter legacy)","SRC IN (Painter legacy)","DST IN (Painter legacy)",
    "SRC OUT (Painter legacy)","DST OUT (Painter legacy)"};
  const gboolean float_modes=GPOINTER_TO_INT(data);
  static const gchar *float_labels[]={"Erase (Painter float nonlinear)","Replace (Painter float nonlinear)",
    "Anti erase (Painter float nonlinear)","SRC IN (Painter float nonlinear)","DST IN (Painter float nonlinear)",
    "SRC OUT (Painter float nonlinear)","DST OUT (Painter float nonlinear)","Normal (Painter float nonlinear)","Multiply (Painter float nonlinear)"};
  GtkWidget *combo=NULL,*window;
  GimpDisplayShell *shell;
  GimpImage *image;
  GimpLayer *layer;
  GtkWidget *dockable;
  guchar *pixels=g_malloc(256*256*4);
  gboolean enabled=FALSE;
  gimp_set_focused_once(gimp);
  gimp_test_utils_create_image(gimp,256,256);
  gimp_test_run_mainloop_until_idle();
  shell=gimp_display_get_shell(gimp_get_display_iter(gimp)->data);
  image=gimp_display_get_image(shell->display);
  layer=gimp_image_get_selected_layers(image)->data;
  for(guint y=0;y<256;++y) for(guint x=0;x<256;++x) {
    guint p=4*(256*y+x);pixels[p]=x;pixels[p+1]=y;pixels[p+2]=196;pixels[p+3]=255;
  }
  gegl_buffer_set(gimp_drawable_get_buffer(GIMP_DRAWABLE(layer)),GEGL_RECTANGLE(0,0,256,256),0,babl_format("R'G'B'A u8"),pixels,GEGL_AUTO_ROWSTRIDE);
  layer=gimp_layer_new(image,256,256,babl_format("R'G'B'A u8"),float_modes?"Nine float nonlinear modes — native selector":"Seven legacy modes — native selector",1,GIMP_LAYER_MODE_PAINTER_NORMAL);
  for(guint y=0;y<256;++y) for(guint x=0;x<256;++x) {
    guint p=4*(256*y+x);pixels[p]=240;pixels[p+1]=48;pixels[p+2]=x;pixels[p+3]=(x/32+y/32)%2?64:192;
  }
  gegl_buffer_set(gimp_drawable_get_buffer(GIMP_DRAWABLE(layer)),GEGL_RECTANGLE(0,0,256,256),0,babl_format("R'G'B'A u8"),pixels,GEGL_AUTO_ROWSTRIDE);
  g_free(pixels);
  g_assert_true(gimp_image_add_layer(image,layer,NULL,0,FALSE));
  {GList *selection=g_list_prepend(NULL,layer);gimp_image_set_selected_layers(image,selection);g_list_free(selection);}
  gimp_context_set_image(gimp_get_user_context(gimp),image);
  gimp_image_flush(image);gimp_test_run_mainloop_until_idle();
  window=gtk_widget_get_toplevel(GTK_WIDGET(shell));
  dockable=gimp_dialog_factory_dialog_raise(gimp_dialog_factory_get_singleton(),gimp_widget_get_monitor(window),window,"gimp-layer-list",-1);
  g_assert_nonnull(dockable);combo=find_combo(dockable);g_assert_nonnull(combo);
  gimp_test_run_mainloop_until_idle();
  g_assert_true(gtk_widget_get_mapped(combo));
  gimp_layer_mode_combo_box_set_group(GIMP_LAYER_MODE_COMBO_BOX(combo),float_modes?GIMP_LAYER_MODE_GROUP_DEFAULT:GIMP_LAYER_MODE_GROUP_LEGACY);
  for(guint index=0;index<(float_modes?9:7);++index) {
    guint raw=23+index;
    GimpLayerMode mode;GtkTreeIter iter;gchar *label=NULL;gint value;
    if(float_modes)mode=GIMP_LAYER_MODE_PAINTER_FLOAT_ERASE+index;
    else g_assert_true(gimp_painter_layer_mode_from_legacy(raw,&mode));
    /* Native GtkComboBox selection emits changed into the actual layer dock.
     * This is a widget-signal integration test, not pointer-event synthesis. */
    g_assert_true(gimp_int_combo_box_set_active(GIMP_INT_COMBO_BOX(combo),mode));
    gimp_test_run_mainloop_until_idle();
    g_assert_cmpint(gimp_layer_get_mode(layer),==,mode);
    g_assert_cmpint(gimp_layer_mode_combo_box_get_mode(GIMP_LAYER_MODE_COMBO_BOX(combo)),==,mode);
    g_assert_true(gtk_combo_box_get_active_iter(GTK_COMBO_BOX(combo),&iter));
    gtk_tree_model_get(gtk_combo_box_get_model(GTK_COMBO_BOX(combo)),&iter,GIMP_INT_STORE_VALUE,&value,GIMP_INT_STORE_LABEL,&label,-1);
    g_assert_cmpint(value,==,mode);g_assert_cmpstr(label,==,float_modes?float_labels[index]:labels[index]);
    g_test_message("%s index %u enum %d label %s applied to selected layer",float_modes?"float":"legacy",index,value,label);g_free(label);
  }
  g_assert_true(gimp_int_combo_box_set_active(GIMP_INT_COMBO_BOX(combo),float_modes?GIMP_LAYER_MODE_PAINTER_FLOAT_REPLACE:GIMP_LAYER_MODE_PAINTER_REPLACE));
  g_object_get(gimp->config,"painter-canvas-ui",&enabled,NULL);g_assert_true(enabled);
  g_assert_true(gimp_painter_canvas_ui_get_visible(shell));
  window=gtk_widget_get_toplevel(GTK_WIDGET(shell));
  gtk_window_resize(GTK_WINDOW(window),1040,1800);gtk_window_move(GTK_WINDOW(window),0,0);gtk_window_present(GTK_WINDOW(window));
  {GtkWidget *dock_window=gtk_widget_get_toplevel(dockable);
   g_test_message("mode dock toplevel %s separate=%d",G_OBJECT_TYPE_NAME(dock_window),dock_window!=window);
   if(dock_window!=window) {gtk_window_resize(GTK_WINDOW(dock_window),360,640);gtk_window_move(GTK_WINDOW(dock_window),660,550);gtk_window_present(GTK_WINDOW(dock_window));}}
  {gint64 end=g_get_monotonic_time()+G_TIME_SPAN_SECOND;while(g_get_monotonic_time()<end){g_main_context_iteration(NULL,FALSE);g_usleep(10000);}}
  if(g_getenv("GIMP_TEST_MODE_SCREENSHOT")) {
    GdkWindow *root=gdk_get_default_root_window();GError *error=NULL;
    GdkPixbuf *shot;
    g_assert_cmpint(gdk_window_get_width(root),==,1080);g_assert_cmpint(gdk_window_get_height(root),==,1920);
    shot=gdk_pixbuf_get_from_window(root,0,0,1080,1920);g_assert_nonnull(shot);
    g_assert_true(gdk_pixbuf_save(shot,g_getenv("GIMP_TEST_MODE_SCREENSHOT"),"png",&error,NULL));g_assert_no_error(error);g_object_unref(shot);
  }
  g_object_unref(image);gimp_display_close(shell->display);gimp_test_run_mainloop_until_idle();
}
#include "test-painter-mode-anime.inc"
#include "test-filter-flip-portrait.inc"
#include "test-filter-geometry-portrait.inc"
#include "test-filter-red-eye-portrait.inc"

int main(int argc,char **argv)
{
  gint result;g_test_init(&argc,&argv,NULL);if(!gtk_init_check(&argc,&argv))return GIMP_EXIT_TEST_SKIPPED;
  gimp_test_utils_setup_menus_path();gimp=gimp_init_for_gui_testing(TRUE);
  g_object_set(gimp->config,"painter-canvas-ui",TRUE,NULL);
  g_test_add_func("/painter-mode-ui/filter-red-eye-portrait",filter_red_eye_oncanvas_portrait);
  g_test_add_func("/painter-mode-ui/filter-flip-portrait",filter_flip_oncanvas_portrait);
  g_test_add_func("/painter-mode-ui/filter-geometry-portrait",filter_geometry_oncanvas_portrait);
  g_test_add_data_func("/painter-mode-ui/native-selector",GINT_TO_POINTER(FALSE),mode_selection);
  g_test_add_data_func("/painter-mode-ui/float-selector",GINT_TO_POINTER(TRUE),mode_selection);
  g_test_add_data_func("/painter-mode-ui/anime-initial",GUINT_TO_POINTER(0),anime_composite_reference);
  g_test_add_data_func("/painter-mode-ui/anime-edited",GUINT_TO_POINTER(1),anime_composite_reference);
  g_application_run(gimp->app,0,NULL);result=gimp_core_app_get_exit_status(GIMP_CORE_APP(gimp->app));
  g_application_quit(G_APPLICATION(gimp->app));g_clear_object(&gimp->app);return result;
}
