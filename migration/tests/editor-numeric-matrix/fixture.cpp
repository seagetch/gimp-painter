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
};
extern "C" {
#include "display/gimpdisplay.h"
#include "display/gimpdisplayshell.h"
#include "display/gimppaintercanvasui.h"
}
struct NumericExpected {const char*name;int index;double minimum,missing_default,maximum;};
static const NumericExpected numeric_expected[]={
  {"opaque",0,double(float(0.0)),double(float(1.0)),double(float(2.0))},
  {"opaque-multiply",1,double(float(0.0)),double(float(0.0)),double(float(2.0))},
  {"opaque-linearize",2,double(float(0.0)),double(float(0.9)),double(float(2.0))},
  {"radius-logarithmic",3,double(float(-2.0)),double(float(2.0)),double(float(5.0))},
  {"hardness",4,double(float(0.0)),double(float(0.8)),double(float(1.0))},
  {"anti-aliasing",5,double(float(0.0)),double(float(1.0)),double(float(5.0))},
  {"dabs-per-basic-radius",6,double(float(0.0)),double(float(0.0)),double(float(6.0))},
  {"dabs-per-actual-radius",7,double(float(0.0)),double(float(2.0)),double(float(6.0))},
  {"dabs-per-second",8,double(float(0.0)),double(float(0.0)),double(float(80.0))},
  {"radius-by-random",9,double(float(0.0)),double(float(0.0)),double(float(1.5))},
  {"speed1-slowness",10,double(float(0.0)),double(float(0.04)),double(float(0.2))},
  {"speed2-slowness",11,double(float(0.0)),double(float(0.8)),double(float(3.0))},
  {"speed1-gamma",12,double(float(-8.0)),double(float(4.0)),double(float(8.0))},
  {"speed2-gamma",13,double(float(-8.0)),double(float(4.0)),double(float(8.0))},
  {"offset-by-random",14,double(float(0.0)),double(float(0.0)),double(float(25.0))},
  {"offset-by-speed",15,double(float(-3.0)),double(float(0.0)),double(float(3.0))},
  {"offset-by-speed-slowness",16,double(float(0.0)),double(float(1.0)),double(float(15.0))},
  {"slow-tracking",17,double(float(0.0)),double(float(0.0)),double(float(10.0))},
  {"slow-tracking-per-dab",18,double(float(0.0)),double(float(0.0)),double(float(10.0))},
  {"tracking-noise",19,double(float(0.0)),double(float(0.0)),double(float(12.0))},
  {"color-h",20,double(float(0.0)),double(float(0.0)),double(float(1.0))},
  {"color-s",21,double(float(-0.5)),double(float(0.0)),double(float(1.5))},
  {"color-v",22,double(float(-0.5)),double(float(0.0)),double(float(1.5))},
  {"restore-color",23,double(float(0.0)),double(float(0.0)),double(float(1.0))},
  {"change-color-h",24,double(float(-2.0)),double(float(0.0)),double(float(2.0))},
  {"change-color-l",25,double(float(-2.0)),double(float(0.0)),double(float(2.0))},
  {"change-color-hsl-s",26,double(float(-2.0)),double(float(0.0)),double(float(2.0))},
  {"change-color-v",27,double(float(-2.0)),double(float(0.0)),double(float(2.0))},
  {"change-color-hsv-s",28,double(float(-2.0)),double(float(0.0)),double(float(2.0))},
  {"smudge",29,double(float(0.0)),double(float(0.0)),double(float(1.0))},
  {"smudge-length",30,double(float(0.0)),double(float(0.5)),double(float(1.0))},
  {"smudge-radius-log",31,double(float(-1.6)),double(float(0.0)),double(float(1.6))},
  {"eraser",32,double(float(0.0)),double(float(0.0)),double(float(1.0))},
  {"stroke-threshold",33,double(float(0.0)),double(float(0.0)),double(float(0.5))},
  {"stroke-duration-logarithmic",34,double(float(-1.0)),double(float(4.0)),double(float(7.0))},
  {"stroke-holdtime",35,double(float(0.0)),double(float(0.0)),double(float(10.0))},
  {"custom-input",36,double(float(-5.0)),double(float(0.0)),double(float(5.0))},
  {"custom-input-slowness",37,double(float(0.0)),double(float(0.0)),double(float(10.0))},
  {"elliptical-dab-ratio",38,double(float(1.0)),double(float(1.0)),double(float(10.0))},
  {"elliptical-dab-angle",39,double(float(0.0)),double(float(90.0)),double(float(360.0))},
  {"direction-filter",40,double(float(0.0)),double(float(2.0)),double(float(10.0))},
  {"lock-alpha",41,double(float(0.0)),double(float(0.0)),double(float(1.0))},
  {"stroke-opacity",42,double(float(0.0)),double(float(1.0)),double(float(1.0))},
  {"texture-grain",43,double(float(-1.0)),double(float(0.0)),double(float(1.0))},
  {"texture-contrast",44,double(float(0.0)),double(float(1.0)),double(float(1.0))},
};
static void numeric_metadata_matrix() {
 Editor a,b;g_assert_true(a.model.get()==b.model.get());
 auto empty=MyPaint::Resource::decode("{\"version\":3,\"unknown_numeric_matrix\":{\"retained\":true}}");a.set(empty);
 for(const auto&e:numeric_expected){
  auto*spec=g_object_class_find_property(G_OBJECT_GET_CLASS(a.model.get()),e.name);g_assert_true(G_IS_PARAM_SPEC_DOUBLE(spec));
  g_assert_cmpfloat(G_PARAM_SPEC_DOUBLE(spec)->minimum,==,e.minimum);g_assert_cmpfloat(G_PARAM_SPEC_DOUBLE(spec)->maximum,==,e.maximum);g_assert_cmpfloat(G_PARAM_SPEC_DOUBLE(spec)->default_value,==,e.missing_default);
  g_assert_cmpfloat(a.snapshot().base_value(e.index),==,e.missing_default);
 }
 unsigned assignments=0;
 for(const auto&e:numeric_expected){
  auto*x=GTK_SPIN_BUTTON(a.get(e.name));auto*y=GTK_SPIN_BUTTON(b.get(e.name));
  const double values[]={e.minimum,e.minimum+(e.maximum-e.minimum)*.37,e.maximum};
  for(double wanted:values){
   // View A -> shared model -> view B. Explicit double edits are not float rounded.
   gtk_spin_button_set_value(x,wanted);
   g_assert_cmpfloat(a.snapshot().base_value(e.index),==,wanted);g_assert_cmpfloat(gtk_spin_button_get_value(x),==,wanted);g_assert_cmpfloat(gtk_spin_button_get_value(y),==,wanted);
   ++assignments;
  }
  for(double wanted:values){
   auto r=a.snapshot();r.set_base_value(e.index,wanted);a.set(r);
   g_assert_cmpfloat(a.snapshot().base_value(e.index),==,wanted);g_assert_cmpfloat(gtk_spin_button_get_value(x),==,wanted);g_assert_cmpfloat(gtk_spin_button_get_value(y),==,wanted);
   ++assignments;
  }
  for(double wanted:values){
   gtk_spin_button_set_value(y,wanted);
   g_assert_cmpfloat(b.snapshot().base_value(e.index),==,wanted);g_assert_cmpfloat(gtk_spin_button_get_value(x),==,wanted);g_assert_cmpfloat(gtk_spin_button_get_value(y),==,wanted);++assignments;
  }
  g_test_message("%s independent old float metadata + exact-double min/interior/max, A->model->B, model->A+B, B->model->A PASS",e.name);
 }
 g_assert_cmpuint(assignments,==,405);g_assert_nonnull(strstr(a.snapshot().encode().c_str(),"unknown_numeric_matrix"));
  const char*shot=g_getenv("GIMP_TEST_SWITCH_DEFAULT_SCREENSHOT");g_assert_nonnull(shot);
  gimp_test_utils_create_image(gimp,640,480);auto*display=gimp_context_get_display(gimp->user_context);g_assert_nonnull(display);auto*shell=gimp_display_get_shell(display);gimp_painter_canvas_ui_set_visible(shell,TRUE);g_assert_true(gimp_painter_canvas_ui_get_visible(shell));
  auto*main=gtk_widget_get_toplevel(GTK_WIDGET(shell));gtk_window_resize(GTK_WINDOW(main),1040,1850);gtk_window_move(GTK_WINDOW(main),0,0);gtk_widget_hide(b.window);gtk_window_resize(GTK_WINDOW(a.window),1000,1180);gtk_window_move(GTK_WINDOW(a.window),20,650);gtk_window_set_title(GTK_WINDOW(a.window),"45 numeric settings: 405 exact model/two-view checks PASS");
  gimp_test_run_temp_mainloop(500);gtk_window_present(GTK_WINDOW(a.window));gimp_test_run_temp_mainloop(200);gdk_window_raise(gtk_widget_get_window(a.window));gdk_display_sync(gdk_display_get_default());g_assert_true(gtk_widget_is_drawable(a.get("opaque")));
  auto*root=gdk_get_default_root_window();g_assert_cmpint(gdk_window_get_width(root),==,1080);g_assert_cmpint(gdk_window_get_height(root),==,1920);auto pixels=ObjectRef<GObject>::adopt(G_OBJECT(gdk_pixbuf_get_from_window(root,0,0,1080,1920)));g_assert_nonnull(pixels.get());GError*error=nullptr;g_assert_true(gdk_pixbuf_save(GDK_PIXBUF(pixels.get()),shot,"png",&error,nullptr));g_assert_no_error(error);g_test_message("API numeric edits; 45 settings / 405 exact checks, oncanvas enabled 1080x1920; not pointer interaction");
}
int main(int argc,char**argv){g_test_init(&argc,&argv,nullptr);if(!gtk_init_check(&argc,&argv))return GIMP_EXIT_TEST_SKIPPED;gimp_test_utils_setup_menus_path();gimp=gimp_init_for_gui_testing(TRUE);add_editor_case("/painter-numeric/matrix45",numeric_metadata_matrix);g_application_run(gimp->app,0,nullptr);int result=gimp_core_app_get_exit_status(GIMP_CORE_APP(gimp->app));g_application_quit(gimp->app);g_clear_object(&gimp->app);return result;}
