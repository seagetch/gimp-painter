/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "core/gimpclonelayer.h"
#include "core/gimppickable.h"
#include "pdb/gimppdb.h"
#include "plug-in/gimppluginprocedure.h"
#include "file/file-open.h"
#include "display/gimpdisplayshell-scale.h"
static GimpLayer *anime_layer(GimpImage *image,const gchar *name)
{
  GList *layers=gimp_image_get_layer_list(image);GimpLayer *found=NULL;
  for(GList *p=layers;p;p=p->next)
    if(!g_strcmp0(gimp_object_get_name(p->data),name))found=p->data;
  g_list_free(layers);g_assert_nonnull(found);return found;
}
static void anime_composite_reference(gconstpointer data)
{
  const gchar *states[]={"initial","edited"};
  GimpPlugInProcedure *proc=GIMP_PLUG_IN_PROCEDURE(gimp_pdb_lookup_procedure(gimp->pdb,"gimp-xcf-load"));
  guint first=GPOINTER_TO_UINT(data);
  for(guint state=first;state<first+1;++state) {
    gchar *filename=g_strdup_printf("anime-%s.xcf",states[state]);
    gchar *path=g_build_filename(g_getenv("GIMP_TESTING_ABS_TOP_SRCDIR"),"migration/fixtures/legacy-composite/anime",filename,NULL);
    GFile *file=g_file_new_for_path(path);GError *error=NULL;GimpPDBStatusType status;
    GimpImage *image=file_open_image(gimp,gimp_get_user_context(gimp),NULL,file,0,0,FALSE,proc,GIMP_RUN_NONINTERACTIVE,&status,NULL,&error);
    GimpLayer *clone,*shadow;GimpDisplay *display;GimpDisplayShell *shell;GtkWidget *window,*dockable,*dock_window;
    gchar *reference_name=g_strdup_printf("anime-%s.projection.rgba",state?"edited-reopened":"initial");
    gchar *reference_path=g_build_filename(g_getenv("GIMP_TESTING_ABS_TOP_SRCDIR"),"migration/fixtures/legacy-composite/anime",reference_name,NULL);
    gchar *expected=NULL;gsize size=0;guchar actual[96*80*4];gboolean enabled=FALSE;
    g_assert_no_error(error);g_assert_cmpint(status,==,GIMP_PDB_SUCCESS);g_assert_nonnull(image);
    g_assert_cmpint(gimp_image_get_width(image),==,96);g_assert_cmpint(gimp_image_get_height(image),==,80);
    clone=anime_layer(image,"mask");shadow=anime_layer(image,"shadow");
    g_assert_true(GIMP_IS_CLONE_LAYER(clone));
    g_assert_true(gimp_clone_layer_get_source(GIMP_CLONE_LAYER(clone))==anime_layer(image,"paint source"));
    g_assert_cmpint(gimp_layer_get_mode(clone),==,GIMP_LAYER_MODE_PAINTER_DST_IN);
    g_assert_cmpint(gimp_layer_get_mode(shadow),==,GIMP_LAYER_MODE_PAINTER_MULTIPLY);
    g_assert_cmpfloat_with_epsilon(gimp_layer_get_opacity(shadow),state?.6:1.0,1e-7);
    g_assert_cmpstr(gimp_object_get_name(gimp_item_get_parent(GIMP_ITEM(clone))),==,"New group");
    g_assert_true(g_file_get_contents(reference_path,&expected,&size,&error));g_assert_no_error(error);g_assert_cmpuint(size,==,sizeof actual);
    gimp_pickable_flush(GIMP_PICKABLE(image));
    gegl_buffer_get(gimp_pickable_get_buffer(GIMP_PICKABLE(image)),GEGL_RECTANGLE(0,0,96,80),1,
                    babl_format("R'G'B'A u8"),actual,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
    for(guint p=0;p<sizeof actual;++p)
      if(actual[p]!=(guchar)expected[p])g_error("anime %s projection byte %u expected %u got %u",states[state],p,(guchar)expected[p],actual[p]);
    g_test_message("genuine old anime %s: all7680 RGBA pixels, CloneLayer reference/type, hierarchy and DST-IN/Multiply match",states[state]);
    gimp_set_focused_once(gimp);display=gimp_create_display(gimp,image,gimp_unit_pixel(),4.0,NULL);g_assert_nonnull(display);
    shell=gimp_display_get_shell(display);gimp_context_set_image(gimp_get_user_context(gimp),image);
    {GList *selection=g_list_prepend(NULL,clone);gimp_image_set_selected_layers(image,selection);g_list_free(selection);}
    window=gtk_widget_get_toplevel(GTK_WIDGET(shell));
    gtk_window_resize(GTK_WINDOW(window),1040,1800);gtk_window_move(GTK_WINDOW(window),0,0);gtk_window_present(GTK_WINDOW(window));
    gimp_display_shell_scale(shell,GIMP_ZOOM_TO,4.0,GIMP_ZOOM_FOCUS_IMAGE_CENTER);
    dockable=gimp_dialog_factory_dialog_raise(gimp_dialog_factory_get_singleton(),gimp_widget_get_monitor(window),window,"gimp-layer-list",-1);
    g_assert_nonnull(dockable);dock_window=gtk_widget_get_toplevel(dockable);
    if(dock_window!=window){gtk_window_resize(GTK_WINDOW(dock_window),360,640);gtk_window_move(GTK_WINDOW(dock_window),660,550);gtk_window_present(GTK_WINDOW(dock_window));}
    {gint64 end=g_get_monotonic_time()+G_TIME_SPAN_SECOND;while(g_get_monotonic_time()<end){g_main_context_iteration(NULL,FALSE);g_usleep(10000);}}
    g_object_get(gimp->config,"painter-canvas-ui",&enabled,NULL);g_assert_true(enabled);g_assert_true(gimp_painter_canvas_ui_get_visible(shell));
    if(g_getenv("GIMP_TEST_ANIME_SCREENSHOT_DIR")) {
      gchar *name=g_strdup_printf("anime-%s-oncanvas-portrait.png",states[state]);
      gchar *output=g_build_filename(g_getenv("GIMP_TEST_ANIME_SCREENSHOT_DIR"),name,NULL);
      GdkWindow *root=gdk_get_default_root_window();GdkPixbuf *shot;
      g_assert_cmpint(gdk_window_get_width(root),==,1080);g_assert_cmpint(gdk_window_get_height(root),==,1920);
      shot=gdk_pixbuf_get_from_window(root,0,0,1080,1920);g_assert_nonnull(shot);
      g_assert_true(gdk_pixbuf_save(shot,output,"png",&error,NULL));g_assert_no_error(error);g_object_unref(shot);g_free(output);g_free(name);
    }
    g_object_unref(image);gimp_display_close(display);gimp_test_run_mainloop_until_idle();
    g_free(expected);g_free(reference_name);g_free(reference_path);g_object_unref(file);g_free(filename);g_free(path);
  }
}
