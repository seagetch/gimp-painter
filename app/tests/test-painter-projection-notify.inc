/* SPDX-License-Identifier: GPL-3.0-or-later */
namespace ProjectionNotify {
struct Updates {std::vector<GeglRectangle> rects;};
static void updated(GimpProjection*,gboolean,gint x,gint y,gint w,gint h,gpointer data)
{static_cast<Updates*>(data)->rects.push_back({x,y,w,h});}
static void settle()
{
 const auto until=g_get_monotonic_time()+500000;
 while(g_get_monotonic_time()<until){g_main_context_iteration(nullptr,FALSE);g_usleep(2000);}
}
static std::array<guchar,3> capture(Scene&s,const char*name)
{
 auto*shell=gimp_display_get_shell(s.display);auto*window=gtk_widget_get_toplevel(GTK_WIDGET(shell));
 g_assert_true(gtk_widget_get_mapped(window));g_assert_true(gtk_widget_get_mapped(shell->canvas));
 gboolean enabled=FALSE;g_object_get(gimp->config,"painter-canvas-ui",&enabled,nullptr);g_assert_true(enabled);
 auto*root=gdk_get_default_root_window();g_assert_cmpint(gdk_window_get_width(root),==,1080);g_assert_cmpint(gdk_window_get_height(root),==,1920);
 g_assert_cmpint(gdk_window_get_width(gtk_widget_get_window(window)),==,1080);g_assert_cmpint(gdk_window_get_height(gtk_widget_get_window(window)),==,1920);
 double x,y;gimp_display_shell_transform_xy_f(shell,50,32,&x,&y);int ox,oy;gdk_window_get_origin(gtk_widget_get_window(shell->canvas),&ox,&oy);
 auto*pixbuf=gdk_pixbuf_get_from_window(root,0,0,1080,1920);g_assert_nonnull(pixbuf);
 const int px=int(x)+ox,py=int(y)+oy;g_assert_cmpint(px,>=,0);g_assert_cmpint(px,<,1080);g_assert_cmpint(py,>=,0);g_assert_cmpint(py,<,1920);
 const auto*pixel=gdk_pixbuf_get_pixels(pixbuf)+py*gdk_pixbuf_get_rowstride(pixbuf)+px*gdk_pixbuf_get_n_channels(pixbuf);
 const std::array<guchar,3> result={pixel[0],pixel[1],pixel[2]};
 if(const auto*dir=g_getenv("GIMP_TEST_PROJECTION_VISUAL_DIR")){
  auto*path=g_build_filename(dir,name,nullptr);GError*error=nullptr;g_assert_true(gdk_pixbuf_save(pixbuf,path,"png",&error,nullptr));g_assert_no_error(error);g_free(path);
 }
 g_test_message("RAW_CANVAS %s portrait=1080x1920 oncanvas=1 sample=%d,%d rgb=%d,%d,%d",name,px,py,result[0],result[1],result[2]);
 g_object_unref(pixbuf);return result;
}
static void visible_update()
{
 gboolean saved=FALSE;g_object_get(gimp->config,"painter-canvas-ui",&saved,nullptr);g_object_set(gimp->config,"painter-canvas-ui",TRUE,nullptr);
 {
  Scene s;auto*shell=gimp_display_get_shell(s.display);auto*window=gtk_widget_get_toplevel(GTK_WIDGET(shell));
  gtk_window_move(GTK_WINDOW(window),0,0);gtk_window_resize(GTK_WINDOW(window),1080,1920);gtk_window_present(GTK_WINDOW(window));
  auto*blue=gegl_color_new("blue");gegl_buffer_set_color(gimp_drawable_get_buffer(s.drawable),GEGL_RECTANGLE(0,0,128,128),blue);g_object_unref(blue);
  gimp_drawable_update(s.drawable,0,0,128,128);gimp_image_flush(s.image);gimp_display_shell_scale(shell,GIMP_ZOOM_TO,5.,GIMP_ZOOM_FOCUS_IMAGE_CENTER);settle();gimp_pickable_flush(GIMP_PICKABLE(gimp_image_get_projection(s.image)));gimp_display_flush_now(s.display);settle();
  const auto before=s.pixels();g_assert_cmpint(before[(32*128+50)*4+2],==,255);g_assert_cmpint(before[(32*128+50)*4+3],==,255);const auto baseline=capture(s,"projection-before.png");g_assert_cmpint(baseline[2],>,baseline[0]+100);
  Updates changes;auto*projection=gimp_image_get_projection(s.image);auto id=g_signal_connect(projection,"update",G_CALLBACK(updated),&changes);
  g_object_set_data(G_OBJECT(projection),"test-notify-active",GINT_TO_POINTER(1));
  s.press();s.coords.x=62;s.motion(true,100);settle();
  // No fixture-side flush, invalidation or pixel-read-triggered projection
  // evaluation occurs between actual tool motion and the raw window capture.
  const auto live=capture(s,"projection-live.png");g_assert_cmpint(live[0],>,live[2]+100);g_assert_true(live!=baseline);
  g_assert_true(s.pixels()!=before);g_assert_false(changes.rects.empty());
  bool covers=false;for(const auto&r:changes.rects)if(50>=r.x&&32>=r.y&&50<r.x+r.width&&32<r.y+r.height)covers=true;g_assert_true(covers);
  guchar pixel[4]={};g_assert_true(gimp_pickable_get_pixel_at(GIMP_PICKABLE(projection),50,32,babl_format("R'G'B'A u8"),pixel));g_assert_cmpint(pixel[0],>,pixel[2]+100);
  gimp_tool_button_release(s.tool,&s.coords,s.time+20,GDK_BUTTON3_MASK,s.display);settle();
  const auto canceled=capture(s,"projection-canceled.png");g_assert_true(s.pixels()==before);g_assert_true(canceled==baseline);
  changes.rects.clear();s.coords.x=32;s.press();s.coords.x=62;s.motion(true,100);s.release();gimp_tool_control(s.tool,GIMP_TOOL_ACTION_COMMIT,s.display);settle();
  const auto committed=capture(s,"projection-committed.png");g_assert_cmpint(committed[0],>,committed[2]+100);g_assert_cmpint(s.depth(),==,1);g_assert_false(changes.rects.empty());
  g_signal_handler_disconnect(projection,id);
 }
 g_object_set(gimp->config,"painter-canvas-ui",saved,nullptr);
}
}
