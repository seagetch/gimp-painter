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


static void numeric_loaded_range () {
  Editor a;
  auto r=a.snapshot();r.set_base_value(BRUSH_RADIUS_LOGARITHMIC,6.0);a.set(r);
  auto*w=GTK_SPIN_BUTTON(a.get("radius-logarithmic"));
  g_assert_cmpfloat(gtk_spin_button_get_value(w),==,6.0);
  g_assert_cmpfloat(gtk_adjustment_get_upper(gtk_spin_button_get_adjustment(w)),==,6.0);
  g_test_expect_message("GLib-GObject",G_LOG_LEVEL_CRITICAL,"*invalid or out of range*");
  gtk_spin_button_set_value(w,5.9);
  g_test_assert_expected_messages();
  const double shown=gtk_spin_button_get_value(w),stored=a.snapshot().base_value(BRUSH_RADIUS_LOGARITHMIC);
  g_test_message("loaded6 -> edit5.9: displayed=%.17g model=%.17g",shown,stored);
  gtk_spin_button_set_value(w,4.9);
  g_assert_cmpfloat_with_epsilon(a.snapshot().base_value(BRUSH_RADIUS_LOGARITHMIC),4.9,1e-12);
  g_test_message("in-range4.9 recovery PASS");
  g_assert_cmpfloat_with_epsilon(shown,stored,1e-12);
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

int main(int argc,char**argv){g_test_init(&argc,&argv,nullptr);if(!gtk_init_check(&argc,&argv))return GIMP_EXIT_TEST_SKIPPED;gimp_test_utils_setup_menus_path();gimp=gimp_init_for_gui_testing(TRUE);add_editor_case("/painter-controls/numeric-loaded-range",numeric_loaded_range);add_editor_case("/painter-controls/switches-two-way",switches_two_way);g_application_run(gimp->app,0,nullptr);int result=gimp_core_app_get_exit_status(GIMP_CORE_APP(gimp->app));g_application_quit(gimp->app);g_clear_object(&gimp->app);g_free(original_search);return result;}
