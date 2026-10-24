/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Actual rendered direct-core-API test; not an emulated click/gesture. */
static void
red_eye_portrait_define (GimpFilterLayer *filter,gint threshold)
{
  GimpValueArray *args=gimp_value_array_new_from_types(NULL,G_TYPE_INT,0,G_TYPE_INT,-123,G_TYPE_INT,456,G_TYPE_INT,threshold,G_TYPE_NONE);
  g_assert_true(gimp_filter_layer_set_definition(filter,"plug-in-red-eye-removal",NULL,args,NULL));gimp_value_array_unref(args);
  flip_portrait_settle(filter);
}
static void
filter_red_eye_oncanvas_portrait (void)
{
  GimpImage *image=gimp_image_new(gimp,400,300,GIMP_RGB,GIMP_PRECISION_U8_NON_LINEAR);
  GimpLayer *source=gimp_layer_new(image,400,300,babl_format("R'G'B'A u8"),"Source: red test patch and blue control",1,GIMP_LAYER_MODE_NORMAL_LEGACY);
  GimpLayer *layer=gimp_filter_layer_new(image,400,300,"Red Eye: literal old byte formula",1,GIMP_LAYER_MODE_NORMAL_LEGACY);
  GimpFilterLayer *filter=GIMP_FILTER_LAYER(layer);guchar *pixels=g_malloc(400*300*4);
  GimpDisplay *display;GimpDisplayShell *shell;GtkWidget *window;
  for(guint y=0;y<300;y++)for(guint x=0;x<400;x++){
    guint p=(y*400+x)*4;gboolean red=x>=50&&x<150&&y>=80&&y<220,blue=x>=250&&x<350&&y>=80&&y<220;
    pixels[p]=red?200:(blue?10:80);pixels[p+1]=red?20:(blue?30:120);pixels[p+2]=red?10:(blue?200:180);pixels[p+3]=255;
  }
  g_assert_true(gimp_image_add_layer(image,source,NULL,0,FALSE));
  gegl_buffer_set(gimp_drawable_get_buffer(GIMP_DRAWABLE(source)),NULL,0,babl_format("R'G'B'A u8"),pixels,GEGL_AUTO_ROWSTRIDE);g_free(pixels);
  {GimpValueArray *args=gimp_value_array_new_from_types(NULL,G_TYPE_INT,0,G_TYPE_INT,-123,G_TYPE_INT,456,G_TYPE_INT,-1073741646,G_TYPE_NONE);
   g_assert_true(gimp_filter_layer_set_definition(filter,"plug-in-red-eye-removal",NULL,args,NULL));gimp_value_array_unref(args);}
  g_assert_true(gimp_image_add_layer(image,layer,NULL,0,FALSE));flip_portrait_settle(filter);
  {GList *selected=g_list_prepend(NULL,layer);gimp_image_set_selected_layers(image,selected);g_list_free(selected);}
  gimp_set_focused_once(gimp);display=gimp_create_display(gimp,image,gimp_unit_pixel(),2,NULL);g_assert_nonnull(display);
  shell=gimp_display_get_shell(display);gimp_context_set_image(gimp_get_user_context(gimp),image);window=gtk_widget_get_toplevel(GTK_WIDGET(shell));
  gtk_window_resize(GTK_WINDOW(window),1040,1800);gtk_window_move(GTK_WINDOW(window),0,0);gtk_window_present(GTK_WINDOW(window));
  gimp_display_shell_scale(shell,GIMP_ZOOM_TO,2,GIMP_ZOOM_FOCUS_IMAGE_CENTER);
  for(guint state=0;state<2;state++){
    gboolean enabled=FALSE;guchar actual[4];GimpValueArray *args;gchar *name;
    if(state)red_eye_portrait_define(filter,50);
    gegl_buffer_get(gimp_drawable_get_buffer(GIMP_DRAWABLE(filter)),GEGL_RECTANGLE(100,150,1,1),1,babl_format("R'G'B'A u8"),actual,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
    g_assert_cmpmem(actual,4,(state?(const guchar[]){20,20,10,255}:(const guchar[]){200,20,10,255}),4);
    gegl_buffer_get(gimp_drawable_get_buffer(GIMP_DRAWABLE(filter)),GEGL_RECTANGLE(300,150,1,1),1,babl_format("R'G'B'A u8"),actual,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
    g_assert_cmpmem(actual,4,((const guchar[]){10,30,200,255}),4);
    gegl_buffer_get(gimp_drawable_get_buffer(GIMP_DRAWABLE(source)),GEGL_RECTANGLE(100,150,1,1),1,babl_format("R'G'B'A u8"),actual,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
    g_assert_cmpmem(actual,4,((const guchar[]){200,20,10,255}),4);
    args=gimp_filter_layer_dup_args(filter);g_assert_cmpuint(gimp_value_array_length(args),==,4);
    g_assert_cmpint(g_value_get_int(gimp_value_array_index(args,0)),==,0);g_assert_cmpint(g_value_get_int(gimp_value_array_index(args,1)),==,-123);g_assert_cmpint(g_value_get_int(gimp_value_array_index(args,2)),==,456);g_assert_cmpint(g_value_get_int(gimp_value_array_index(args,3)),==,state?50:-1073741646);gimp_value_array_unref(args);
    name=gimp_filter_layer_dup_procedure(filter);g_assert_cmpstr(name,==,"plug-in-red-eye-removal");g_free(name);
    gimp_pickable_flush(GIMP_PICKABLE(image));gimp_image_flush(image);
    gtk_window_set_title(GTK_WINDOW(window),state?"Red Eye AFTER: threshold50 (direct API)":"Red Eye BEFORE: no correction threshold (direct API)");
    {gint64 end=g_get_monotonic_time()+G_TIME_SPAN_SECOND;while(g_get_monotonic_time()<end){g_main_context_iteration(NULL,FALSE);g_usleep(10000);}}
    g_object_get(gimp->config,"painter-canvas-ui",&enabled,NULL);g_assert_true(enabled);g_assert_true(gimp_painter_canvas_ui_get_visible(shell));
    {GdkWindow *root=gdk_get_default_root_window();g_assert_cmpint(gdk_window_get_width(root),==,1080);g_assert_cmpint(gdk_window_get_height(root),==,1920);
     if(g_getenv("GIMP_TEST_RED_EYE_SCREENSHOT_DIR")){GError *error=NULL;GdkPixbuf *shot=gdk_pixbuf_get_from_window(root,0,0,1080,1920);gchar *path=g_build_filename(g_getenv("GIMP_TEST_RED_EYE_SCREENSHOT_DIR"),state?"red-eye-after-oncanvas-portrait.png":"red-eye-before-oncanvas-portrait.png",NULL);g_assert_nonnull(shot);g_assert_true(gdk_pixbuf_save(shot,path,"png",&error,NULL));g_assert_no_error(error);g_object_unref(shot);g_free(path);}}
    g_test_message("Red Eye %s:direct core API;red R200->20 only after;G20/B10/A255 and blue control/source unchanged;metadata exact;oncanvas true1080x1920",state?"after":"before");
  }
  g_object_unref(image);gimp_display_close(display);gimp_test_run_mainloop_until_idle();
}
