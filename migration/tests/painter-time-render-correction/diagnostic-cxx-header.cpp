/* SPDX-License-Identifier: GPL-3.0-or-later
 * Original23.009: direct registered-tool timestamps, not queued GDK events.
 * The link observer forwards every argument unchanged to the real session.
 */
int frozen_mypaint_main(int argc,char **argv);
#define main frozen_mypaint_main
#include "test-painter-mypaint-tool.cpp"
#undef main
#include "paint/painter-mypaint-surface/gimp-painter-session.h"

extern "C" {
#include "display/gimpimagewindow.h"
}
static GtkWidget *mapped_editor_button(GtkWidget *widget)
{
 if(GTK_IS_BUTTON(widget)&&gtk_widget_get_mapped(widget)&&g_strcmp0(gtk_button_get_label(GTK_BUTTON(widget)),"MyPaint Editor")==0)return widget;
 if(GTK_IS_CONTAINER(widget)){GList *children=gtk_container_get_children(GTK_CONTAINER(widget));GtkWidget *found=nullptr;for(auto *i=children;i&&!found;i=i->next)found=mapped_editor_button(GTK_WIDGET(i->data));g_list_free(children);return found;}return nullptr;
}
static void verified_render(Scene &s)
{
 const char *dir=g_getenv("TIMESTAMP_RENDER_DIRECTORY");g_assert_nonnull(dir);
 auto *shell=gimp_display_get_shell(s.display);auto *window=gtk_widget_get_toplevel(GTK_WIDGET(shell));g_assert_true(GIMP_IS_IMAGE_WINDOW(window));
 gimp_image_window_set_active_shell(GIMP_IMAGE_WINDOW(window),shell);gimp_display_shell_present(shell);
 gtk_window_move(GTK_WINDOW(window),0,0);gtk_window_resize(GTK_WINDOW(window),1080,1920);gtk_window_present(GTK_WINDOW(window));
 gimp_painter_canvas_ui_set_visible(shell,TRUE);gimp_display_shell_scale(shell,GIMP_ZOOM_TO,5.,GIMP_ZOOM_FOCUS_IMAGE_CENTER);
 gimp_pickable_flush(GIMP_PICKABLE(gimp_image_get_projection(s.image)));gimp_display_flush_now(s.display);
 auto until=g_get_monotonic_time()+750000;while(g_get_monotonic_time()<until){g_main_context_iteration(nullptr,FALSE);g_usleep(1000);}
 g_assert_true(gimp_image_window_get_active_shell(GIMP_IMAGE_WINDOW(window))==shell);g_assert_true(gtk_widget_get_mapped(window));g_assert_true(gtk_widget_get_mapped(shell->canvas));g_assert_true(gimp_painter_canvas_ui_get_visible(shell));
 auto *root=gdk_get_default_root_window();g_assert_cmpint(gdk_window_get_width(root),==,1080);g_assert_cmpint(gdk_window_get_height(root),==,1920);g_assert_cmpint(gdk_window_get_width(gtk_widget_get_window(window)),==,1080);g_assert_cmpint(gdk_window_get_height(gtk_widget_get_window(window)),==,1920);
 auto *button=mapped_editor_button(window);g_assert_nonnull(button);gint bx,by;g_assert_true(gtk_widget_translate_coordinates(button,window,0,0,&bx,&by));g_assert_cmpint(bx,>=,0);g_assert_cmpint(by,>=,0);g_assert_cmpint(bx+gtk_widget_get_allocated_width(button),<=,1080);g_assert_cmpint(by+gtk_widget_get_allocated_height(button),<=,1920);
 auto *shot=gdk_pixbuf_get_from_window(root,0,0,1080,1920);g_assert_nonnull(shot);
 double x,y;gimp_display_shell_transform_xy_f(shell,48,32,&x,&y);gint ox,oy;gdk_window_get_origin(gtk_widget_get_window(shell->canvas),&ox,&oy);gint px=int(x)+ox,py=int(y)+oy;g_assert_cmpint(px,>=,0);g_assert_cmpint(px,<,1080);g_assert_cmpint(py,>=,0);g_assert_cmpint(py,<,1920);
 const guchar *pixel=gdk_pixbuf_get_pixels(shot)+py*gdk_pixbuf_get_rowstride(shot)+px*gdk_pixbuf_get_n_channels(shot);g_assert_cmpint(pixel[0],>,pixel[1]+25);g_assert_cmpint(pixel[0],>,pixel[2]+25);
 g_test_message("VERIFIED_RENDER target_active=1 canvas_mapped=1 oncanvas=1 root=1080x1920 window=1080x1920 editor_button=%d,%d sample=%d,%d rgb=%u,%u,%u",bx,by,px,py,pixel[0],pixel[1],pixel[2]);
 auto *path=g_build_filename(dir,"timestamp-wrap-render-verified.png",nullptr);GError *error=nullptr;g_assert_true(gdk_pixbuf_save(shot,path,"png",&error,nullptr));g_assert_no_error(error);g_free(path);g_object_unref(shot);
}

namespace TimestampSequence {
struct Sample { double seconds; GimpCoords coords; };
static std::vector<Sample> delivered;
static bool capture=false;
extern "C" gboolean __real_gimp_painter_session_stroke_to(GimpPainterSession*,GimpDrawable*,gdouble,const GimpCoords*,gboolean*,GError**);
extern "C" gboolean __wrap_gimp_painter_session_stroke_to(GimpPainterSession*s,GimpDrawable*d,gdouble seconds,const GimpCoords*c,gboolean*split,GError**error)
{
  if(capture)delivered.push_back({seconds,*c});
  return __real_gimp_painter_session_stroke_to(s,d,seconds,c,split,error);
}
struct Sequence { guint32 start;std::array<guint32,3> times;std::array<guint32,3> milliseconds; };
static const Sequence sequences[]={
 {100,{120,140,160},{20,20,20}},
 {100,{100,120,120},{0,20,0}},
 {100,{90,110,130},{0,20,20}},
 {0xfffffff0u,{4,24,44},{20,20,20}},
 {100,{0x80000064u,0x80000078u,0x8000008cu},{0,20,20}},
 {0,{0,20,40},{0,20,20}},
 {100,{0x80000063u,0x80000077u,0x8000008bu},{0x7fffffffu,20,20}}
};
static std::vector<guchar> evaluate(const Sequence &q)
{
 Scene s;s.time=q.start;auto before=s.pixels();
 delivered.clear();capture=true;s.press();g_assert_cmpuint(delivered.size(),==,1);g_assert_cmpfloat(delivered[0].seconds,==,0);
 for(unsigned i=0;i<3;++i){auto prior_pixels=s.pixels();s.time=q.times[i];s.coords.x=40+i*8;s.coords.pressure=.35+i*.2;gimp_tool_motion(s.tool,&s.coords,s.time,GDK_BUTTON1_MASK,s.display);g_assert_cmpuint(delivered.size(),==,i+2);const auto &sample=delivered.back();g_assert_cmpfloat(sample.seconds,==,q.milliseconds[i]/1000.);g_assert_cmpfloat(sample.coords.x,==,s.coords.x);g_assert_cmpfloat(sample.coords.pressure,==,s.coords.pressure);if(q.milliseconds[i]==0x7fffffffu)g_assert_true(s.pixels()==prior_pixels);}
 capture=false;s.release();auto painted=s.pixels();g_assert_true(painted!=before);unsigned alpha=0;for(unsigned i=3;i<painted.size();i+=4)if(painted[i])++alpha;g_assert_cmpuint(alpha,>,0);g_test_message("painted alpha pixels=%u",alpha);if(q.start==0xfffffff0u)verified_render(s);g_assert_cmpint(s.depth(),==,1);g_assert_true(gimp_image_undo(s.image));g_assert_true(s.pixels()==before);g_assert_true(gimp_image_redo(s.image));g_assert_true(s.pixels()==painted);return painted;
}
static void run(gconstpointer value)
{
 const auto &q=sequences[GPOINTER_TO_UINT(value)];auto painted=evaluate(q);Sequence reference=q;reference.start=100;guint32 time=reference.start;for(unsigned i=0;i<3;++i){time+=q.milliseconds[i];reference.times[i]=time;}auto normalized=evaluate(reference);g_assert_true(painted==normalized);
}
}
int main(int argc,char**argv)
{
 g_test_init(&argc,&argv,nullptr);if(!gtk_init_check(&argc,&argv))return GIMP_EXIT_TEST_SKIPPED;gimp_test_utils_setup_menus_path();gimp=gimp_init_for_gui_testing(TRUE);
 g_test_add_data_func("/timestamp-render/wrap",GUINT_TO_POINTER(3),TimestampSequence::run);
 g_application_run(gimp->app,0,nullptr);int result=gimp_core_app_get_exit_status(GIMP_CORE_APP(gimp->app));g_application_quit(G_APPLICATION(gimp->app));g_clear_object(&gimp->app);return result;
}
