/* SPDX-License-Identifier: GPL-3.0-or-later */
#define PAIRED_MODEL_NO_MAIN
#include "test-paired-stroke-undo.cpp"
#undef PAIRED_MODEL_NO_MAIN
extern "C" {
#include "gimpcoreapp.h"
#include "display/display-types.h"
#include "display/gimpdisplay.h"
#include "display/gimpdisplayshell.h"
#include "display/gimpdisplayshell-transform.h"
#include "display/gimppaintercanvasui.h"
}
static std::vector<guchar> capture_pair(GimpDisplayShell*shell,const char*name)
{
  const char*directory=g_getenv("GIMP_TEST_PAIRED_SCREENSHOTS");
  g_assert_nonnull(directory);
  gimp_image_flush(gimp_display_get_image(shell->display));
  auto*window=gtk_widget_get_toplevel(GTK_WIDGET(shell));
  gtk_window_resize(GTK_WINDOW(window),1040,1800);
  gtk_window_present(GTK_WINDOW(window));
  const auto deadline=g_get_monotonic_time()+G_TIME_SPAN_SECOND;
  while(g_get_monotonic_time()<deadline){g_main_context_iteration(nullptr,FALSE);g_usleep(10000);}
  g_assert_true(gimp_painter_canvas_ui_get_visible(shell));
  auto*native=gtk_widget_get_window(window);
  auto*shot=gdk_pixbuf_get_from_window(native,0,0,gdk_window_get_width(native),gdk_window_get_height(native));
  g_assert_nonnull(shot);g_assert_cmpint(gdk_pixbuf_get_height(shot),>,gdk_pixbuf_get_width(shot));
  gchar*path=g_build_filename(directory,name,nullptr);GError*error=nullptr;
  g_assert_true(gdk_pixbuf_save(shot,path,"png",&error,nullptr));g_assert_no_error(error);
  gint x0,y0,x1,y1,wx,wy;gimp_display_shell_transform_xy(shell,0,0,&x0,&y0);gimp_display_shell_transform_xy(shell,64,48,&x1,&y1);
  g_assert_true(gtk_widget_translate_coordinates(shell->canvas,window,x0,y0,&wx,&wy));
  ++wx;++wy;const gint width=x1-x0-2,height=y1-y0-2,channels=gdk_pixbuf_get_n_channels(shot),stride=gdk_pixbuf_get_rowstride(shot);
  g_assert_cmpint(wx,>=,0);g_assert_cmpint(wy,>=,0);g_assert_cmpint(wx+width,<=,gdk_pixbuf_get_width(shot));g_assert_cmpint(wy+height,<=,gdk_pixbuf_get_height(shot));
  g_test_message("rendered image rectangle %s: %d,%d %dx%d",name,wx,wy,width,height);
  std::vector<guchar> visible;const auto*data=gdk_pixbuf_read_pixels(shot);for(int row=0;row<height;++row)visible.insert(visible.end(),data+(wy+row)*stride+wx*channels,data+(wy+row)*stride+(wx+width)*channels);
  g_free(path);g_object_unref(shot);return visible;
}
static void paired_visual()
{
  PairFixture f;gimp_set_focused_once(gimp);
  gimp_object_set_name(GIMP_OBJECT(f.layer),"Paired history / historical original Resource");
  auto*display=static_cast<GimpDisplay*>(gimp_create_display(gimp,f.image,gimp_unit_pixel(),8.0,nullptr));
  g_assert_nonnull(display);gimp_test_run_mainloop_until_idle();auto*shell=gimp_display_get_shell(display);
  auto before=pixels(f.d);auto sb=identity(*f.core);auto visible_before=capture_pair(shell,"00-baseline.png");
  batch_stroke(*f.core,f.d);auto after=pixels(f.d);auto sa=identity(*f.core);g_assert_true(after!=before);auto visible_after=capture_pair(shell,"01-painted.png");g_assert_true(visible_after!=visible_before);
  g_assert_true(gimp_image_undo(f.image));g_assert_true(pixels(f.d)==before);g_assert_true(identity(*f.core)==sb);g_assert_true(capture_pair(shell,"02-undo.png")==visible_before);
  g_assert_true(gimp_image_redo(f.image));g_assert_true(pixels(f.d)==after);g_assert_true(identity(*f.core)==sa);g_assert_true(capture_pair(shell,"03-redo.png")==visible_after);
  gimp_display_close(display);gimp_test_run_mainloop_until_idle();
}
int main(int argc,char**argv)
{
 g_test_init(&argc,&argv,nullptr);g_assert_true(gtk_init_check(&argc,&argv));gimp_test_utils_set_gimp3_directory("GIMP_TESTING_ABS_TOP_SRCDIR","app/tests/gimpdir");
 gimp_test_utils_setup_menus_path();gimp=gimp_init_for_gui_testing(TRUE);
 g_object_set(gimp->config,"painter-canvas-ui",TRUE,nullptr);gboolean enabled=FALSE;g_object_get(gimp->config,"painter-canvas-ui",&enabled,nullptr);g_assert_true(enabled);
 g_test_add_func("/paired-ui/visible-roundtrip",paired_visual);g_application_run(gimp->app,0,nullptr);int result=gimp_core_app_get_exit_status(GIMP_CORE_APP(gimp->app));
 g_application_quit(G_APPLICATION(gimp->app));g_clear_object(&gimp->app);return result;
}
