/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Opt-in actual-window evidence; does not replace the 24 unconditional cases. */
static void layer_mask_visual_capture()
{
  const char*directory=g_getenv("GIMP_TEST_LAYER_MASK_VISUAL_DIR");
  g_assert_nonnull(directory);
  gboolean saved_canvas=FALSE;
  g_object_get(gimp->config,"painter-canvas-ui",&saved_canvas,nullptr);
  g_object_set(gimp->config,"painter-canvas-ui",TRUE,nullptr);
  {
    Scene s;
    auto*layer=GIMP_LAYER(s.drawable);auto*mask=layer_mask_fixture(s);
    std::vector<guchar> rgba(128*128*4),coverage(128*128);
    for(int y=0;y<128;++y)for(int x=0;x<128;++x) {
      const auto i=(y*128+x)*4;
      const bool band=(y/16)%2;
      rgba[i]=band?240:30;rgba[i+1]=band?170:105;rgba[i+2]=band?30:210;rgba[i+3]=255;
      coverage[y*128+x]=x<64?255:0;
    }
    gegl_buffer_set(gimp_drawable_get_buffer(s.drawable),GEGL_RECTANGLE(0,0,128,128),0,babl_format("R'G'B'A u8"),rgba.data(),GEGL_AUTO_ROWSTRIDE);
    gegl_buffer_set(gimp_drawable_get_buffer(GIMP_DRAWABLE(mask)),GEGL_RECTANGLE(0,0,128,128),0,babl_format("Y u8"),coverage.data(),GEGL_AUTO_ROWSTRIDE);
    gimp_drawable_update(GIMP_DRAWABLE(mask),0,0,128,128);
    const auto raw=native_pixels(s.drawable),mask_raw=native_pixels(GIMP_DRAWABLE(mask));
    auto*shell=gimp_display_get_shell(s.display);
    auto*window=gtk_widget_get_toplevel(GTK_WIDGET(shell));
    g_assert_true(GTK_IS_WINDOW(window));
    gtk_window_move(GTK_WINDOW(window),0,0);gtk_window_resize(GTK_WINDOW(window),1080,1920);
    gtk_window_present(GTK_WINDOW(window));
    gimp_display_shell_scale(shell,GIMP_ZOOM_TO,5.,GIMP_ZOOM_FOCUS_IMAGE_CENTER);
    for(bool apply:{true,false,true}) {
      gimp_layer_set_apply_mask(layer,apply,FALSE);
      gimp_layer_set_show_mask(layer,FALSE,FALSE);
      gimp_drawable_update(s.drawable,0,0,128,128);gimp_image_flush(s.image);
      gimp_pickable_flush(GIMP_PICKABLE(gimp_image_get_projection(s.image)));
      gimp_display_flush_now(s.display);
      const auto deadline=g_get_monotonic_time()+2*G_TIME_SPAN_SECOND;
      while(g_get_monotonic_time()<deadline){g_main_context_iteration(nullptr,FALSE);g_usleep(10000);}
      g_assert_true(gtk_widget_get_mapped(window));
      g_assert_true(gtk_widget_get_mapped(shell->canvas));
      g_assert_cmpint(gdk_window_get_width(gtk_widget_get_window(window)),==,1080);
      g_assert_cmpint(gdk_window_get_height(gtk_widget_get_window(window)),==,1920);
      gboolean canvas=FALSE;g_object_get(gimp->config,"painter-canvas-ui",&canvas,nullptr);g_assert_true(canvas);
      g_assert_cmpint(gimp_layer_get_apply_mask(layer),==,apply);
      g_assert_true(native_pixels(s.drawable)==raw);g_assert_true(native_pixels(GIMP_DRAWABLE(mask))==mask_raw);
      double left=-1,right=-1;
      auto*projection=GIMP_PICKABLE(gimp_image_get_projection(s.image));
      g_assert_true(gimp_pickable_get_pixel_at(projection,32,32,babl_format("A double"),&left));
      g_assert_true(gimp_pickable_get_pixel_at(projection,96,32,babl_format("A double"),&right));
      g_assert_cmpfloat(left,==,1);g_assert_cmpfloat(right,==,apply?0:1);
      auto*root=gdk_get_default_root_window();g_assert_cmpint(gdk_window_get_width(root),==,1080);g_assert_cmpint(gdk_window_get_height(root),==,1920);
      auto*pixbuf=gdk_pixbuf_get_from_window(root,0,0,1080,1920);g_assert_nonnull(pixbuf);
      const auto*name=apply?"mask-applied.png":"mask-disabled.png";
      auto*path=g_build_filename(directory,name,nullptr);GError*error=nullptr;
      g_assert_true(gdk_pixbuf_save(pixbuf,path,"png",&error,nullptr));g_assert_no_error(error);
      g_test_message("RAW_CAPTURE %s width=1080 height=1920 painter-canvas-ui=1 apply=%d left_alpha=%.0f right_alpha=%.0f raw_and_mask_unchanged=1",name,apply,left,right);
      g_free(path);g_object_unref(pixbuf);
    }
  }
  g_object_set(gimp->config,"painter-canvas-ui",saved_canvas,nullptr);
}
