/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include "painter/binding-store.hpp"
#include <gegl.h>
#include <gtk/gtk.h>
#include <glib/gstdio.h>
extern "C" {
#include "libgimpbase/gimpbase.h"
#include "libgimpconfig/gimpconfig.h"
#include "libgimpmath/gimpmath.h"
#include "libgimpwidgets/gimpwidgets.h"
#include "core/core-types.h"
#include "widgets/widgets-types.h"
#include "core/gimp.h"
#include "core/gimpcontext.h"
#include "core/gimpcontainer.h"
#include "core/gimpdatafactory.h"
#include "core/gimpbrush.h"
#include "core/gimpbrush-private.h"
#include "core/gimppattern.h"
#include "core/gimptempbuf.h"
#include "core/gimptag.h"
#include "core/gimptagged.h"
#include "core/gimptoolinfo.h"
#include "config/gimpcoreconfig.h"
#include "config/gimprc.h"
#include "widgets/gimpaction.h"
#include "widgets/gimpactiongroup.h"
#include "widgets/gimpactionfactory.h"
#include "widgets/gimpdialogfactory.h"
#include "widgets/gimpdocked.h"
#include "widgets/gimpdock.h"
#include "widgets/gimpdockable.h"
#include "widgets/gimpdockbook.h"
#include "widgets/gimphelp-ids.h"
#include "widgets/gimpuimanager.h"
#include "display/display-types.h"
#include "actions/actions-types.h"
#include "actions/actions.h"
#include "actions/dockable-commands.h"
#include "dialogs/preferences-dialog.h"
#include "gimpcoreapp.h"
#include "tests.h"
#include "gimp-app-test-utils.h"
}
#include "widgets/gimppaintermybrusheditor.hpp"
#include "core/gimppaintermybrush-handle.hpp"
#include "paint/painter-mypaint-surface/gimp-painter-options.hpp"
#include "mypaintbrush-settings-data.h"
#include "painter/gimp-painter-binding.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <fstream>
#include <vector>
using namespace GimpPainter;
static Gimp *gimp;
struct EditorTestCase { void (*run)(); };
static void run_editor_case(gconstpointer data) {
  // Every filtered case needs the deferred device setup before GUI teardown.
  gimp_set_focused_once(gimp);
  static_cast<const EditorTestCase *>(data)->run();
}
static void add_editor_case(const char *name,void (*run)()) {
  auto *test=new EditorTestCase{run};
  g_test_add_data_func_full(name,test,run_editor_case,
    +[](gpointer data){delete static_cast<EditorTestCase *>(data);});
}
static std::string writable_directory;
static gchar *original_search;
static GtkWidget*find(GtkWidget*w,const char*name){
  if(!g_strcmp0(gtk_widget_get_name(w),name))return w;
  if(GTK_IS_CONTAINER(w)){GList*children=gtk_container_get_children(GTK_CONTAINER(w));GtkWidget*result=nullptr;for(auto*l=children;l&&!result;l=l->next)result=find(GTK_WIDGET(l->data),name);g_list_free(children);return result;}return nullptr;
}
static GtkWidget*control(GtkWidget*w,const char*name){auto*r=find(w,name);g_assert_nonnull(r);return r;}
struct Editor {
  GtkWidget*window;GimpPainterMybrushEditor*widget;ObjectRef<GimpPainterMybrushOptions>model;
  explicit Editor(bool horizontal=false,GimpContext*context=nullptr) {
    GError*error=nullptr;auto*ctx=context?context:gimp->user_context;
    auto*w=gimp_painter_mybrush_editor_new(ctx,horizontal,&error);g_assert_no_error(error);g_assert_nonnull(w);widget=GIMP_PAINTER_MYBRUSH_EDITOR(w);
    model=ObjectRef<GimpPainterMybrushOptions>::adopt(gimp_painter_mybrush_options_ref_for_context(ctx,&error));g_assert_no_error(error);
    window=gtk_window_new(GTK_WINDOW_TOPLEVEL);g_object_ref_sink(window);gtk_window_set_default_size(GTK_WINDOW(window),horizontal?900:440,700);gtk_container_add(GTK_CONTAINER(window),w);gtk_widget_show_all(window);
  }
  ~Editor(){if(window){gtk_widget_destroy(window);g_object_unref(window);}}
  GtkWidget*get(const char*name){return control(GTK_WIDGET(widget),name);}
  MyPaint::Resource snapshot(){return PainterOptionsRef::retain(model.get()).snapshot();}
  void set(const MyPaint::Resource&r){GError*e=nullptr;g_assert_true(gimp_painter_mybrush_options_set_json(model.get(),r.encode().c_str(),&e));g_assert_no_error(e);}
  std::vector<guchar>preview(){
    gint64 deadline=g_get_monotonic_time()+20*G_TIME_SPAN_SECOND;
    while(gimp_painter_mybrush_editor_preview_pending(widget)&&g_get_monotonic_time()<deadline)g_main_context_iteration(nullptr,FALSE);
    g_assert_false(gimp_painter_mybrush_editor_preview_pending(widget));auto*p=gimp_painter_mybrush_editor_ref_preview(widget);g_assert_nonnull(p);
    std::vector<guchar>pixels;for(int y=0;y<gdk_pixbuf_get_height(p);++y){auto*row=gdk_pixbuf_get_pixels(p)+y*gdk_pixbuf_get_rowstride(p);pixels.insert(pixels.end(),row,row+gdk_pixbuf_get_width(p)*4);}g_object_unref(p);return pixels;
  }
};
static void prepare(){
  gimp_set_focused_once(gimp);
  // A filtered case needs the same corpus setup as the full suite. Reuse the
  // first setup without replacing original_search or refreshing live drafts.
  static bool prepared=false;
  if(prepared)return;
  gchar*path=nullptr;g_object_get(gimp->config,"painter-mypaint-brush-path-writable",&path,"painter-mypaint-brush-path",&original_search,nullptr);
  GError*error=nullptr;gchar*expanded=gimp_config_path_expand(path,TRUE,&error);g_assert_no_error(error);g_assert_nonnull(expanded);
  writable_directory=expanded;g_assert_nonnull(strstr(expanded,"painter-mypaint-brushes"));g_assert_cmpint(g_mkdir_with_parents(expanded,0700),==,0);
  gchar*corpus=g_build_filename(g_getenv("GIMP_TESTING_ABS_TOP_SRCDIR"),"data/painter-mypaint-brushes",nullptr);
  gchar*search=g_strconcat(expanded,G_SEARCHPATH_SEPARATOR_S,corpus,nullptr);g_object_set(gimp->config,"painter-mypaint-brush-path",search,nullptr);
  g_free(search);g_free(corpus);g_free(expanded);g_free(path);
  gimp_data_factory_data_refresh(gimp->painter_mybrush_factory,gimp->user_context);
  auto*c=gimp_data_factory_get_container(gimp->painter_mybrush_factory);g_assert_cmpint(gimp_container_get_n_children(c),==,177);
  gimp_context_set_painter_mybrush(gimp->user_context,GIMP_PAINTER_MYBRUSH(gimp_container_get_child_by_index(c,0)));
  prepared=true;
}


extern "C" {
#include "display/gimpdisplay.h"
#include "display/gimpdisplayshell.h"
#include "display/gimppaintercanvasui.h"
}
static bool reject_next_json=false;
extern "C" gboolean __real_gimp_painter_mybrush_options_set_json(GimpPainterMybrushOptions*,const gchar*,GError**);
extern "C" gboolean __wrap_gimp_painter_mybrush_options_set_json(GimpPainterMybrushOptions*o,const gchar*j,GError**e){
  if(reject_next_json){reject_next_json=false;g_set_error_literal(e,G_IO_ERROR,G_IO_ERROR_FAILED,"Injected prepublication rejection");return FALSE;}
  return __real_gimp_painter_mybrush_options_set_json(o,j,e);
}
static void numeric_loaded_range () {
  Editor a,b;auto r=MyPaint::Resource::decode("{\"version\":3,\"unknown\":{\"lossless\":[true,\"keep\"]}}");
  r.set_switch(BRUSH_USE_GIMP_BRUSHMARK,false);r.set_switch(BRUSH_USE_GIMP_TEXTURE,false);
  r.set_curve(BRUSH_OPAQUE,INPUT_PRESSURE,{{0,.23},{1,.87}});
  r.set_text(BRUSH_BRUSHMARK_NAME,"missing named shape retained");r.set_base_value(BRUSH_RADIUS_LOGARITHMIC,6.0);a.set(r);
  GError*error=nullptr;g_assert_true(gimp_painter_mybrush_options_set_draft_name(a.model.get(),"Pending numeric draft",&error));g_assert_no_error(error);
  auto*selected=gimp_context_get_painter_mybrush(GIMP_CONTEXT(a.model.get()));
  auto*w=GTK_SPIN_BUTTON(a.get("radius-logarithmic"));auto*v=GTK_SPIN_BUTTON(b.get("radius-logarithmic"));
  guint changed=0,known_notify=0,name_notify=0;
  auto ch=g_signal_connect(a.model.get(),"settings-changed",G_CALLBACK(+[](GObject*,gpointer p){++*static_cast<guint*>(p);}),&changed);
  auto kn=g_signal_connect(a.model.get(),"notify",G_CALLBACK(+[](GObject*,GParamSpec*p,gpointer d){
    for(auto&s:painter_mypaint_settings){std::string k=s.internal_name;std::replace(k.begin(),k.end(),'_','-');if(k==p->name){++*static_cast<guint*>(d);return;}}
    for(auto&s:painter_mypaint_switches){std::string k=s.internal_name;std::replace(k.begin(),k.end(),'_','-');if(k==p->name){++*static_cast<guint*>(d);return;}}
    for(auto&s:painter_mypaint_texts){std::string k=s.internal_name;std::replace(k.begin(),k.end(),'_','-');if(k==p->name){++*static_cast<guint*>(d);return;}}
  }),&known_notify);
  auto nn=g_signal_connect(a.model.get(),"notify::painter-draft-name",G_CALLBACK(+[](GObject*,GParamSpec*,gpointer p){++*static_cast<guint*>(p);}),&name_notify);
  for(double value:{5.9,5.7,4.9}){
    changed=known_notify=name_notify=0;gtk_spin_button_set_value(w,value);
    g_assert_cmpfloat_with_epsilon(a.snapshot().base_value(BRUSH_RADIUS_LOGARITHMIC),value,1e-12);
    g_assert_cmpfloat_with_epsilon(gtk_spin_button_get_value(v),value,1e-12);
    g_assert_cmpuint(changed,==,1);g_assert_cmpuint(name_notify,==,0);
    g_assert_cmpuint(known_notify,==,value>5?52:1);
    r.set_base_value(BRUSH_RADIUS_LOGARITHMIC,value);g_assert_true(a.snapshot().encode()==r.encode());
    g_assert_true(gimp_context_get_painter_mybrush(GIMP_CONTEXT(a.model.get()))==selected);
    String name(gimp_painter_mybrush_options_dup_draft_name(a.model.get(),&error));g_assert_no_error(error);g_assert_cmpstr(name.get(),==,"Pending numeric draft");
    const char*shot=g_getenv("GIMP_TEST_CONTROLS_SCREENSHOT");
    if(shot&&value==5.9){
      gimp_test_utils_create_image(gimp,640,480);auto*display=gimp_context_get_display(gimp->user_context);g_assert_nonnull(display);
      auto*shell=gimp_display_get_shell(display);gimp_painter_canvas_ui_set_visible(shell,TRUE);g_assert_true(gimp_painter_canvas_ui_get_visible(shell));
      auto*main=gtk_widget_get_toplevel(GTK_WIDGET(shell));gtk_window_resize(GTK_WINDOW(main),1040,1850);gtk_window_move(GTK_WINDOW(main),0,0);
      gtk_widget_hide(b.window);gtk_window_resize(GTK_WINDOW(a.window),1000,1180);gtk_window_move(GTK_WINDOW(a.window),20,650);
      gtk_window_set_title(GTK_WINDOW(a.window),"Loaded radius 6 -> UI and shared model 5.9; no clamp");
      gimp_test_run_temp_mainloop(500);gtk_window_present(GTK_WINDOW(a.window));gimp_test_run_temp_mainloop(200);gdk_window_raise(gtk_widget_get_window(a.window));gdk_display_sync(gdk_display_get_default());
      auto*root=gdk_get_default_root_window();g_assert_cmpint(gdk_window_get_width(root),==,1080);g_assert_cmpint(gdk_window_get_height(root),==,1920);
      auto pixels=ObjectRef<GObject>::adopt(G_OBJECT(gdk_pixbuf_get_from_window(root,0,0,1080,1920)));g_assert_nonnull(pixels.get());
      g_assert_true(gdk_pixbuf_save(GDK_PIXBUF(pixels.get()),shot,"png",&error,nullptr));g_assert_no_error(error);
      g_test_message("Actual1080x1920 oncanvas visible; API-edited spin5.9 and model verified");
    }

  }
  g_signal_handler_disconnect(a.model.get(),ch);g_signal_handler_disconnect(a.model.get(),kn);g_signal_handler_disconnect(a.model.get(),nn);
  g_test_message("loaded6 ->5.9->5.7->4.9: both views coherent, exact whole Resource/selection/name retained; settings signal1; outside notifies52 / inside1");
}
static void numeric_rejection () {
  Editor a;auto r=a.snapshot();r.set_base_value(BRUSH_RADIUS_LOGARITHMIC,6);a.set(r);auto*w=GTK_SPIN_BUTTON(a.get("radius-logarithmic"));
  reject_next_json=true;gtk_spin_button_set_value(w,5.9);g_assert_false(reject_next_json);
  g_assert_true(a.snapshot().encode()==r.encode());g_assert_cmpfloat(gtk_spin_button_get_value(w),==,6);
  g_test_message("rejected draft update restores displayed6 and unchanged model");
}
static void switches_two_way () {
  Editor a,b;
  g_assert_true(a.model.get()==b.model.get());
  for(auto&s:painter_mypaint_switches){
    std::string key=s.internal_name;std::replace(key.begin(),key.end(),'_','-');
    auto*x=GTK_TOGGLE_BUTTON(a.get(key.c_str()));auto*y=GTK_TOGGLE_BUTTON(b.get(key.c_str()));
    for(bool value:{false,true,false}){
      auto r=a.snapshot();r.set_switch(s.index,value);a.set(r);
      g_assert_cmpint(gtk_toggle_button_get_active(x),==,value);g_assert_cmpint(gtk_toggle_button_get_active(y),==,value);
      gtk_toggle_button_set_active(x,!value);g_assert_cmpint(a.snapshot().switch_value(s.index),==,!value);g_assert_cmpint(gtk_toggle_button_get_active(y),==,!value);
      gtk_toggle_button_set_active(y,value);g_assert_cmpint(b.snapshot().switch_value(s.index),==,value);g_assert_cmpint(gtk_toggle_button_get_active(x),==,value);
    }
    g_test_message("switch %s model/two-view bidirectional PASS",s.internal_name);
  }
}

int main(int argc,char**argv){g_test_init(&argc,&argv,nullptr);if(!gtk_init_check(&argc,&argv))return GIMP_EXIT_TEST_SKIPPED;gimp_test_utils_setup_menus_path();gimp=gimp_init_for_gui_testing(TRUE);add_editor_case("/painter-controls/numeric-loaded-range",numeric_loaded_range);add_editor_case("/painter-controls/numeric-rejection",numeric_rejection);add_editor_case("/painter-controls/switches-two-way",switches_two_way);g_application_run(gimp->app,0,nullptr);int result=gimp_core_app_get_exit_status(GIMP_CORE_APP(gimp->app));g_application_quit(gimp->app);g_clear_object(&gimp->app);g_free(original_search);return result;}
