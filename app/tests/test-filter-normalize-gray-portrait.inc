/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Actual rendered committed native routes. Visibility changes and definitions
 * use direct core APIs, not parameter-editor or mouse-gesture claims. */
static void
filter_normalize_gray_oncanvas_portrait (void)
{
 const guint width=400,height=300;const gsize size=(gsize)width*height*2;
 GimpImage *image=gimp_image_new(gimp,width,height,GIMP_GRAY,GIMP_PRECISION_U8_NON_LINEAR);
 GimpLayer *source=gimp_layer_new(image,width,height,babl_format("Y'A u8"),"Gray source:40/80/120 controls",1,GIMP_LAYER_MODE_NORMAL_LEGACY);
 GimpLayer *layer=gimp_filter_layer_new(image,width,height,"Gray Normalize: extrema40..120",1,GIMP_LAYER_MODE_NORMAL_LEGACY);
 GimpFilterLayer *filter=GIMP_FILTER_LAYER(layer);guchar *input=g_malloc(size),*expected=g_malloc(size),*actual=g_malloc(size);GimpValueArray *args;
 GimpDisplay *display;GimpDisplayShell *shell;GtkWidget *window;GError *error=NULL;gchar *directory;
 for(guint y=0;y<height;y++)for(guint x=0;x<width;x++){
  guint p=(y*width+x)*2;gboolean warm=x>=50&&x<150&&y>=80&&y<220,cool=x>=250&&x<350&&y>=80&&y<220;
  input[p]=warm?120:(cool?40:80);input[p+1]=255;
  expected[p]=255*(input[p]-40)/80;expected[p+1]=255;
 }
 g_assert_true(gimp_image_add_layer(image,source,NULL,0,FALSE));gegl_buffer_set(gimp_drawable_get_buffer(GIMP_DRAWABLE(source)),NULL,0,babl_format("Y'A u8"),input,GEGL_AUTO_ROWSTRIDE);
 args=gimp_value_array_new_from_types(NULL,G_TYPE_INT,-123,G_TYPE_INT,456,G_TYPE_INT,-789,G_TYPE_NONE);
 g_assert_true(gimp_filter_layer_set_definition(filter,"plug-in-normalize",NULL,args,NULL));gimp_value_array_unref(args);
 g_assert_true(gimp_image_add_layer(image,layer,NULL,0,FALSE));flip_portrait_settle(filter);
 gegl_buffer_get(gimp_drawable_get_buffer(GIMP_DRAWABLE(filter)),NULL,1,babl_format("Y'A u8"),actual,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);g_assert_cmpmem(actual,size,expected,size);
 {GList *selected=g_list_prepend(NULL,layer);gimp_image_set_selected_layers(image,selected);g_list_free(selected);}
 gimp_set_focused_once(gimp);display=gimp_create_display(gimp,image,gimp_unit_pixel(),2,NULL);g_assert_nonnull(display);shell=gimp_display_get_shell(display);gimp_context_set_image(gimp_get_user_context(gimp),image);window=gtk_widget_get_toplevel(GTK_WIDGET(shell));
 gtk_window_resize(GTK_WINDOW(window),1040,1800);gtk_window_move(GTK_WINDOW(window),0,0);gtk_window_present(GTK_WINDOW(window));gimp_display_shell_scale(shell,GIMP_ZOOM_TO,2,GIMP_ZOOM_FOCUS_IMAGE_CENTER);
 directory=g_getenv("GIMP_TEST_COMPAT_PORTRAIT_DIR")?g_strdup(g_getenv("GIMP_TEST_COMPAT_PORTRAIT_DIR")):g_dir_make_tmp("gimp-compat-portrait-XXXXXX",&error);g_assert_no_error(error);g_assert_nonnull(directory);
 for(guint state=0;state<2;state++){
  gboolean enabled=FALSE;gchar *title,*name,*filename,*path;GdkWindow *root;GdkPixbuf *shot;
  gimp_item_set_visible(GIMP_ITEM(layer),state,FALSE);gimp_pickable_flush(GIMP_PICKABLE(image));gimp_image_flush(image);
  title=g_strdup_printf("%s %s: %s (direct API, not editor)","Gray Normalize",state?"AFTER":"BEFORE",state?"native Filter visible":"unchanged source, Filter hidden");gtk_window_set_title(GTK_WINDOW(window),title);g_free(title);
  {gint64 end=g_get_monotonic_time()+G_TIME_SPAN_SECOND;while(g_get_monotonic_time()<end){g_main_context_iteration(NULL,FALSE);g_usleep(10000);}}
  gegl_buffer_get(gimp_pickable_get_buffer(GIMP_PICKABLE(image)),GEGL_RECTANGLE(0,0,width,height),1,babl_format("Y'A u8"),actual,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);g_assert_cmpmem(actual,size,state?expected:input,size);
  gegl_buffer_get(gimp_drawable_get_buffer(GIMP_DRAWABLE(source)),NULL,1,babl_format("Y'A u8"),actual,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);g_assert_cmpmem(actual,size,input,size);
  args=gimp_filter_layer_dup_args(filter);g_assert_cmpuint(gimp_value_array_length(args),==,3);for(guint i=0;i<3;i++)g_assert_true(G_VALUE_HOLDS_INT(gimp_value_array_index(args,i)));g_assert_cmpint(g_value_get_int(gimp_value_array_index(args,0)),==,-123);g_assert_cmpint(g_value_get_int(gimp_value_array_index(args,1)),==,456);g_assert_cmpint(g_value_get_int(gimp_value_array_index(args,2)),==,-789);
  gimp_value_array_unref(args);name=gimp_filter_layer_dup_procedure(filter);g_assert_cmpstr(name,==,"plug-in-normalize");g_free(name);
  g_object_get(gimp->config,"painter-canvas-ui",&enabled,NULL);g_assert_true(enabled);g_assert_true(gimp_painter_canvas_ui_get_visible(shell));root=gdk_get_default_root_window();g_assert_cmpint(gdk_window_get_width(root),==,1080);g_assert_cmpint(gdk_window_get_height(root),==,1920);
  {gint ww,wh;gtk_window_get_size(GTK_WINDOW(window),&ww,&wh);g_assert_cmpint(ww,<=,1080);g_assert_cmpint(wh,>,ww);g_test_message("Actual window %dx%d, root1080x1920,oncanvas visible",ww,wh);}
  shot=gdk_pixbuf_get_from_window(root,0,0,1080,1920);g_assert_nonnull(shot);filename=g_strdup_printf("%s-%s-oncanvas-portrait.png","normalize-gray",state?"after":"before");path=g_build_filename(directory,filename,NULL);g_assert_true(gdk_pixbuf_save(shot,path,"png",&error,NULL));g_assert_no_error(error);g_test_message("Captured %s; full120000 projection/source/native pixels match independent byte oracle; direct API visibility only",path);g_object_unref(shot);g_free(filename);g_free(path);
 }
 g_free(directory);g_free(input);g_free(expected);g_free(actual);g_object_unref(image);gimp_display_close(display);gimp_test_run_mainloop_until_idle();
}
