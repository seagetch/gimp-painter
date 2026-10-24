/* SPDX-License-Identifier: GPL-3.0-or-later */
#define main original_tool_main
#include "test-painter-mypaint-tool.cpp"
#undef main
#include "paint/painter-mypaint-surface/gegl-surface.hpp"
extern "C" {
#include "display/gimppaintercanvasui.h"
}
namespace NonincrementalDisplay {
static void match_visible(Scene&s,const char*name) {
  // Capture first: reading projection or flushing here would mask missing redraw.
  auto visible=ProjectionNotify::capture(s,name);
  auto bytes=s.pixels();const auto*p=bytes.data()+(32*128+50)*4;
  std::vector<guchar> projected(bytes.size());auto*projection=gimp_image_get_projection(s.image);auto*buffer=gimp_pickable_get_buffer(GIMP_PICKABLE(projection));g_assert_nonnull(buffer);gegl_buffer_get(buffer,GEGL_RECTANGLE(0,0,128,128),1,babl_format("R'G'B'A u8"),projected.data(),GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);g_assert_true(projected==bytes);
  for(int c=0;c<3;++c)g_assert_cmpint(std::abs(int(visible[c])-int(p[c])),<=,2);
  MyPaint::GeglSurface sampler(gimp_drawable_get_buffer(s.drawable));
  sampler.set_shape_provider([](float,float,float,float){return MyPaint::ShapeMask{1,1,{255}};});
  float r,g,b,a;sampler.get_color(51,33,1,&r,&g,&b,&a,1);
  g_assert_cmpfloat_with_epsilon(r,p[0]/255.f,.005f);g_assert_cmpfloat_with_epsilon(b,p[2]/255.f,.005f);g_assert_cmpfloat_with_epsilon(a,1,.005f);
  g_test_message("composed canvas/sample %s pixel=%u,%u,%u,%u",name,p[0],p[1],p[2],p[3]);
}
static void overlap(Scene&s){s.coords.x=50;s.coords.y=32;s.press();for(int i=0;i<8;++i)s.motion(true,20);ProjectionNotify::settle();}
static void lifecycle(){
 Scene s;g_object_set(s.options,"non-incremental",TRUE,"stroke-opacity",.37,nullptr);
 auto*shell=gimp_display_get_shell(s.display);auto*window=gtk_widget_get_toplevel(GTK_WIDGET(shell));gtk_window_move(GTK_WINDOW(window),0,0);gtk_window_resize(GTK_WINDOW(window),1080,1920);gtk_window_present(GTK_WINDOW(window));
 auto*blue=gegl_color_new("blue");gegl_buffer_set_color(gimp_drawable_get_buffer(s.drawable),GEGL_RECTANGLE(0,0,128,128),blue);g_object_unref(blue);
 gimp_drawable_update(s.drawable,0,0,128,128);gimp_image_flush(s.image);gimp_display_shell_scale(shell,GIMP_ZOOM_TO,5.,GIMP_ZOOM_FOCUS_IMAGE_CENTER);ProjectionNotify::settle();gimp_pickable_flush(GIMP_PICKABLE(gimp_image_get_projection(s.image)));gimp_display_flush_now(s.display);ProjectionNotify::settle();
 g_assert_true(gimp_painter_canvas_ui_get_visible(shell));auto before=s.pixels();match_visible(s,"01-before.png");
 ProjectionNotify::Updates changes;auto id=g_signal_connect(gimp_image_get_projection(s.image),"update",G_CALLBACK(ProjectionNotify::updated),&changes);
 overlap(s);match_visible(s,"02-live.png");g_assert_false(changes.rects.empty());g_assert_cmpint(s.depth(),==,0);auto active=s.pixels();g_assert_true(active!=before);auto*p=active.data()+(32*128+50)*4;g_assert_cmpint(p[0],>,60);g_assert_cmpint(p[0],<=,95);g_assert_cmpint(p[2],>=,159);
 gimp_tool_button_release(s.tool,&s.coords,s.time+20,GDK_BUTTON3_MASK,s.display);ProjectionNotify::settle();match_visible(s,"03-cancel.png");g_assert_true(s.pixels()==before);g_assert_cmpint(s.depth(),==,0);
 overlap(s);match_visible(s,"04-live-again.png");s.release();ProjectionNotify::settle();match_visible(s,"05-released.png");auto after=s.pixels();g_assert_cmpint(s.depth(),==,1);g_assert_true(after!=before);
 g_assert_true(gimp_image_undo(s.image));gimp_image_flush(s.image);ProjectionNotify::settle();match_visible(s,"06-undo.png");g_assert_true(s.pixels()==before);
 g_assert_true(gimp_image_redo(s.image));gimp_image_flush(s.image);ProjectionNotify::settle();match_visible(s,"07-redo.png");g_assert_true(s.pixels()==after);g_signal_handler_disconnect(gimp_image_get_projection(s.image),id);
}
}
int main(int argc,char**argv){g_test_init(&argc,&argv,nullptr);gimp_test_utils_set_gimp3_directory("GIMP_TESTING_ABS_TOP_SRCDIR","app/tests/gimpdir");gimp_test_utils_setup_menus_path();gimp=gimp_init_for_gui_testing(TRUE);g_object_set(gimp->config,"painter-canvas-ui",TRUE,nullptr);g_test_add_func("/nonincremental-display/lifecycle",NonincrementalDisplay::lifecycle);g_application_run(gimp->app,0,nullptr);int result=gimp_core_app_get_exit_status(GIMP_CORE_APP(gimp->app));g_application_quit(G_APPLICATION(gimp->app));g_clear_object(&gimp->app);return result;}
