/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "core/gimpfilterlayer.h"
#include "core/gimpimage-flip.h"

static void
flip_portrait_settle (GimpFilterLayer *filter)
{
  gint64 deadline=g_get_monotonic_time()+20*G_TIME_SPAN_SECOND;
  while (g_get_monotonic_time()<deadline)
    {
      if (gimp_filter_layer_get_state (filter)==GIMP_FILTER_LAYER_CLEAN) return;
      if (gimp_filter_layer_get_state (filter)==GIMP_FILTER_LAYER_FAILED)
        {
          gchar *error=gimp_filter_layer_dup_error (filter);
          g_error ("Portrait Gaussian failed: %s",error);
        }
      g_main_context_iteration (NULL,FALSE);g_usleep (1000);
    }
  g_error ("Portrait Gaussian did not settle");
}
static void
filter_flip_oncanvas_portrait (void)
{
  GimpImage *image=gimp_image_new (gimp,400,300,GIMP_RGB,GIMP_PRECISION_U8_NON_LINEAR);
  GimpLayer *source=gimp_layer_new (image,400,300,babl_format ("R'G'B'A u8"),
                                  "Ordinary source: red | blue",1.0,GIMP_LAYER_MODE_NORMAL_LEGACY);
  GimpLayer *filter=gimp_filter_layer_new (image,160,160,"Filter: fixed extent, Gaussian",1.0,
                                          GIMP_LAYER_MODE_NORMAL_LEGACY);
  GimpValueArray *args=gimp_value_array_new_from_types (NULL,
    G_TYPE_INT,1,G_TYPE_INT,123,G_TYPE_INT,456,
    G_TYPE_DOUBLE,12.0,G_TYPE_DOUBLE,12.0,G_TYPE_INT,1,G_TYPE_NONE);
  guchar *pixels=g_malloc (400*300*4);
  GimpDisplay *display;GimpDisplayShell *shell;GtkWidget *window;
  gboolean enabled=FALSE;
  for (guint y=0;y<300;y++) for (guint x=0;x<400;x++)
    {
      guint i=(y*400+x)*4;
      pixels[i]=x<200?255:0;pixels[i+1]=0;pixels[i+2]=x<200?0:255;pixels[i+3]=255;
    }
  g_assert_true (gimp_image_add_layer (image,source,NULL,0,FALSE));
  gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (source)),GEGL_RECTANGLE(0,0,400,300),
                   0,babl_format ("R'G'B'A u8"),pixels,GEGL_AUTO_ROWSTRIDE);g_free (pixels);
  g_assert_true (gimp_filter_layer_set_definition (GIMP_FILTER_LAYER(filter),"plug-in-gauss",NULL,args,NULL));
  gimp_value_array_unref (args);
  gimp_item_set_offset (GIMP_ITEM(filter),140,70);
  g_assert_true (gimp_image_add_layer (image,filter,NULL,0,FALSE));
  flip_portrait_settle (GIMP_FILTER_LAYER(filter));
  {GList *selected=g_list_prepend (NULL,filter);gimp_image_set_selected_layers (image,selected);g_list_free(selected);}
  gimp_set_focused_once(gimp);display=gimp_create_display(gimp,image,gimp_unit_pixel(),2.0,NULL);g_assert_nonnull(display);
  shell=gimp_display_get_shell(display);gimp_context_set_image(gimp_get_user_context(gimp),image);
  window=gtk_widget_get_toplevel(GTK_WIDGET(shell));
  gtk_window_resize(GTK_WINDOW(window),1040,1800);gtk_window_move(GTK_WINDOW(window),0,0);gtk_window_present(GTK_WINDOW(window));
  gimp_display_shell_scale(shell,GIMP_ZOOM_TO,2.0,GIMP_ZOOM_FOCUS_IMAGE_CENTER);
  for (guint state=0;state<2;state++)
    {
      guchar left[4],right[4];
      if (state)
        {
          /* Real core image action API, not pointer/gesture synthesis. */
          gimp_image_flip(image,gimp_get_user_context(gimp),GIMP_ORIENTATION_HORIZONTAL,NULL);
          flip_portrait_settle(GIMP_FILTER_LAYER(filter));
        }
      g_assert_cmpint(gimp_item_get_offset_x(GIMP_ITEM(filter)),==,140);
      g_assert_cmpint(gimp_item_get_offset_y(GIMP_ITEM(filter)),==,70);
      g_assert_cmpint(gimp_item_get_width(GIMP_ITEM(filter)),==,160);
      g_assert_cmpint(gimp_item_get_height(GIMP_ITEM(filter)),==,160);
      gegl_buffer_get(gimp_drawable_get_buffer(GIMP_DRAWABLE(filter)),GEGL_RECTANGLE(32,80,1,1),1.0,
                      babl_format("R'G'B'A u8"),left,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
      gegl_buffer_get(gimp_drawable_get_buffer(GIMP_DRAWABLE(filter)),GEGL_RECTANGLE(128,80,1,1),1.0,
                      babl_format("R'G'B'A u8"),right,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
      g_assert_cmpmem(left,4,(state?(const guchar[]){0,0,255,255}:(const guchar[]){255,0,0,255}),4);
      g_assert_cmpmem(right,4,(state?(const guchar[]){255,0,0,255}:(const guchar[]){0,0,255,255}),4);
      gimp_pickable_flush(GIMP_PICKABLE(image));gimp_image_flush(image);
      {gint64 until=g_get_monotonic_time()+G_TIME_SPAN_SECOND;while(g_get_monotonic_time()<until){g_main_context_iteration(NULL,FALSE);g_usleep(10000);}}
      gtk_window_set_title(GTK_WINDOW(window),state?"Filter image flip: AFTER (direct API test)":"Filter image flip: BEFORE (direct API test)");
      g_object_get(gimp->config,"painter-canvas-ui",&enabled,NULL);g_assert_true(enabled);
      g_assert_true(gimp_painter_canvas_ui_get_visible(shell));
      if(g_getenv("GIMP_TEST_FILTER_FLIP_SCREENSHOT_DIR"))
        {
          gchar *path=g_build_filename(g_getenv("GIMP_TEST_FILTER_FLIP_SCREENSHOT_DIR"),
                                       state?"filter-flip-after-oncanvas-portrait.png":"filter-flip-before-oncanvas-portrait.png",NULL);
          GdkWindow *root=gdk_get_default_root_window();GdkPixbuf *shot;GError *error=NULL;
          g_assert_cmpint(gdk_window_get_width(root),==,1080);g_assert_cmpint(gdk_window_get_height(root),==,1920);
          shot=gdk_pixbuf_get_from_window(root,0,0,1080,1920);g_assert_nonnull(shot);
          g_assert_true(gdk_pixbuf_save(shot,path,"png",&error,NULL));g_assert_no_error(error);
          g_object_unref(shot);g_free(path);
        }
      g_test_message("%s direct image-flip API:fixed140,70/160x160 Filter bounds;left/right derived pixels verified;oncanvas enabled;1080x1920 root",state?"after":"before");
    }
  g_object_unref(image);gimp_display_close(display);gimp_test_run_mainloop_until_idle();
}
