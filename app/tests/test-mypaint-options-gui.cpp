/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gegl.h>
#include <gtk/gtk.h>
#include <algorithm>
#include <string>
extern "C" {
#include "core/core-types.h"
#include "widgets/widgets-types.h"
#include "core/gimp.h"
#include "core/gimpcontext.h"
#include "core/gimpcontainer.h"
#include "core/gimpdatafactory.h"
#include "core/gimptoolinfo.h"
#include "core/gimptooloptions.h"
#include "core/gimppaintermybrush.h"
#include "widgets/gimppaintermybrusheditor.h"
#include "tools/gimpmypaintoptions-gui.h"
#include "tests.h"
#include "gimpcoreapp.h"
#include "gimp-app-test-utils.h"
GtkWidget *options_gui_c_consumer(GimpToolOptions*,gboolean);
}
#include "paint/painter-mypaint-surface/gimp-painter-options.hpp"
#include "painter/binding-store.hpp"
#include "mypaintbrush-settings-data.h"
using namespace GimpPainter;
static Gimp *app;
static GtkWidget*find(GtkWidget*w,const char*name){
 if(!g_strcmp0(gtk_widget_get_name(w),name))return w;
 if(GTK_IS_CONTAINER(w)){GList*c=gtk_container_get_children(GTK_CONTAINER(w));GtkWidget*r=nullptr;for(auto*l=c;l&&!r;l=l->next)r=find(GTK_WIDGET(l->data),name);g_list_free(c);return r;}return nullptr;
}
static GtkWidget*control(GtkWidget*w,const char*name){auto*r=find(w,name);g_assert_nonnull(r);return r;}
struct View {
 GtkWidget*window;GtkWidget*widget;
 View(GimpMypaintOptions*options,bool horizontal,bool typed=false){
  if(typed){auto model=MypaintOptionsRef::retain(options);auto owned=horizontal?model.gui_horizontal():model.gui();g_assert_true(bool(owned));widget=GTK_WIDGET(owned.release());}
  else{widget=options_gui_c_consumer(GIMP_TOOL_OPTIONS(options),horizontal);g_assert_nonnull(widget);g_assert_true(g_object_is_floating(widget));g_object_ref_sink(widget);}
  window=gtk_window_new(GTK_WINDOW_TOPLEVEL);g_object_ref_sink(window);gtk_window_set_default_size(GTK_WINDOW(window),1000,1760);gtk_window_move(GTK_WINDOW(window),20,40);
  gtk_container_add(GTK_CONTAINER(window),widget);g_object_unref(widget);gtk_widget_show_all(window);gimp_test_run_mainloop_until_idle();
 }
 ~View(){gtk_widget_destroy(window);g_object_unref(window);}
 GtkWidget*get(const char*name){return control(widget,name);}
};
static void set_curve(GimpMypaintOptions*o){GError*error=nullptr;GimpVector2 pts[]={{0,0},{1,.5}};g_assert_true(gimp_painter_mybrush_options_set_curve(o,BRUSH_TEXTURE_GRAIN,INPUT_PRESSURE,pts,2,&error));g_assert_no_error(error);}
static void curve_controls(View&a,View&b,GimpMypaintOptions*o){
 for(auto*v:{&a,&b}){gtk_combo_box_set_active(GTK_COMBO_BOX(v->get("painter-curve-setting")),BRUSH_TEXTURE_GRAIN);gtk_combo_box_set_active(GTK_COMBO_BOX(v->get("painter-curve-input")),INPUT_PRESSURE);}
 set_curve(o);gtk_combo_box_set_active(GTK_COMBO_BOX(a.get("painter-curve-point")),0);gtk_spin_button_set_value(GTK_SPIN_BUTTON(a.get("painter-curve-x")),0);gtk_spin_button_set_value(GTK_SPIN_BUTTON(a.get("painter-curve-y")),.375);gtk_button_clicked(GTK_BUTTON(a.get("painter-curve-apply")));
 auto snapshot=MypaintOptionsRef::retain(o).snapshot();g_assert_cmpuint(snapshot.curve(BRUSH_TEXTURE_GRAIN,INPUT_PRESSURE).size(),==,2);g_assert_cmpfloat_with_epsilon(snapshot.curve(BRUSH_TEXTURE_GRAIN,INPUT_PRESSURE)[0].y,.375,1e-8);
 gtk_combo_box_set_active(GTK_COMBO_BOX(b.get("painter-curve-point")),0);g_assert_cmpfloat_with_epsilon(gtk_spin_button_get_value(GTK_SPIN_BUTTON(b.get("painter-curve-y"))),.375,1e-8);
 gtk_button_clicked(GTK_BUTTON(b.get("painter-curve-clear")));g_assert_true(MypaintOptionsRef::retain(o).snapshot().curve(BRUSH_TEXTURE_GRAIN,INPUT_PRESSURE).empty());g_assert_false(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(a.get("painter-curve-enabled"))));
}
static void snapshot(View&view,bool horizontal){
 auto*directory=g_getenv("GIMP_OPTIONS_GUI_SCREENSHOT_DIR");g_assert_nonnull(directory);
 gtk_window_set_title(GTK_WINDOW(view.window),horizontal?"Original horizontal C GUI | independent native Options | GTK-control edits":"Original vertical C GUI | independent native Options | GTK-control edits");
 gtk_spin_button_set_value(GTK_SPIN_BUTTON(view.get("opaque")),.42);
 gtk_notebook_set_current_page(GTK_NOTEBOOK(view.get("painter-editor-notebook")),2);
 gtk_combo_box_set_active(GTK_COMBO_BOX(view.get("painter-curve-setting")),BRUSH_TEXTURE_GRAIN);
 gtk_combo_box_set_active(GTK_COMBO_BOX(view.get("painter-curve-input")),INPUT_PRESSURE);
 auto*editor=GIMP_PAINTER_MYBRUSH_EDITOR(view.widget);auto deadline=g_get_monotonic_time()+20*G_TIME_SPAN_SECOND;
 while(gimp_painter_mybrush_editor_preview_pending(editor)&&g_get_monotonic_time()<deadline)g_main_context_iteration(nullptr,FALSE);
 g_assert_false(gimp_painter_mybrush_editor_preview_pending(editor));auto*preview=gimp_painter_mybrush_editor_ref_preview(editor);g_assert_nonnull(preview);g_object_unref(preview);gimp_test_run_mainloop_until_idle();
 auto*root=gdk_get_default_root_window();g_assert_cmpint(gdk_window_get_width(root),==,1080);g_assert_cmpint(gdk_window_get_height(root),==,1920);
 g_assert_true(gtk_widget_get_mapped(view.get("painter-curve-graph")));
 auto*pixbuf=gdk_pixbuf_get_from_window(root,0,0,1080,1920);g_assert_nonnull(pixbuf);
 gchar*path=g_build_filename(directory,horizontal?"original-options-horizontal-portrait.png":"original-options-vertical-portrait.png",nullptr);GError*error=nullptr;
 g_assert_true(gdk_pixbuf_save(pixbuf,path,"png",&error,nullptr));g_assert_no_error(error);g_test_message("Actual rendered portrait screenshot: %s; original C entry; GTK-control API edits",path);g_free(path);g_object_unref(pixbuf);
}
struct Acquisition {bool armed=false;unsigned calls=0;};
static void dispose_on_acquisition(gpointer data,GObject*owner,gboolean last){auto*a=static_cast<Acquisition*>(data);if(!last&&a->armed){a->armed=false;++a->calls;g_object_run_dispose(owner);}}
struct Construction {Gimp*app;unsigned calls=0;gpointer weak_editor=nullptr;};
static gboolean close_app_on_show(GSignalInvocationHint*,guint n,const GValue*v,gpointer data){
 auto*c=static_cast<Construction*>(data);if(c->calls||!n)return TRUE;
 auto*w=GTK_WIDGET(g_value_get_object(v));if(g_strcmp0(gtk_widget_get_name(w),"painter-brush-search"))return TRUE;
 auto*editor=gtk_widget_get_ancestor(w,GIMP_TYPE_PAINTER_MYBRUSH_EDITOR);g_assert_nonnull(editor);c->weak_editor=editor;g_object_add_weak_pointer(G_OBJECT(editor),&c->weak_editor);++c->calls;BindingStore::require(G_OBJECT(c->app)).close();return TRUE;
}
static void run_case(gconstpointer data){
 guint mode=GPOINTER_TO_UINT(data);
 if(!g_test_subprocess()){g_test_trap_subprocess(nullptr,60*G_TIME_SPAN_SECOND,GTestSubprocessFlags(0));g_test_trap_assert_passed();return;}
 gimp_set_focused_once(app);g_object_set(app->config,"painter-canvas-ui",TRUE,nullptr);gboolean enabled=FALSE;g_object_get(app->config,"painter-canvas-ui",&enabled,nullptr);g_assert_true(enabled);
 gchar*corpus=g_build_filename(g_getenv("GIMP_TESTING_ABS_TOP_SRCDIR"),"data/painter-mypaint-brushes",nullptr);g_object_set(app->config,"painter-mypaint-brush-path",corpus,nullptr);g_free(corpus);gimp_data_factory_data_refresh(app->painter_mybrush_factory,app->user_context);
 auto*info=GIMP_TOOL_INFO(gimp_container_get_child_by_name(app->tool_info_list,"gimp-painter-mypaint-tool"));g_assert_nonnull(info);auto*canonical=GIMP_PAINTER_MYBRUSH_OPTIONS(info->tool_options);auto*options=gimp_mypaint_options_new(info);g_assert_nonnull(options);g_assert_null(GIMP_TOOL_OPTIONS(options)->tool_info);g_assert_true(options!=canonical);
 const auto canonical_before=MypaintOptionsRef::retain(canonical).snapshot().encode();
 if(mode==0){
  View a(options,false),b(options,true,true);
  for(auto&s:painter_mypaint_settings){std::string key=s.internal_name;std::replace(key.begin(),key.end(),'_','-');g_assert_true(GTK_IS_SPIN_BUTTON(a.get(key.c_str())));g_assert_true(GTK_IS_SPIN_BUTTON(b.get(key.c_str())));}
  for(auto&s:painter_mypaint_switches){std::string key=s.internal_name;std::replace(key.begin(),key.end(),'_','-');g_assert_true(GTK_IS_TOGGLE_BUTTON(a.get(key.c_str())));}
  for(auto&s:painter_mypaint_texts){std::string key=s.internal_name;std::replace(key.begin(),key.end(),'_','-');g_assert_true(GTK_IS_ENTRY(a.get(key.c_str())));}
  const char*names[]={"opaque","radius-logarithmic","slow-tracking","hardness"};const double values[]={.321,1.75,2.25,.654};
  for(unsigned n=0;n<G_N_ELEMENTS(names);n++){gtk_spin_button_set_value(GTK_SPIN_BUTTON(a.get(names[n])),values[n]);double actual=0;g_object_get(options,names[n],&actual,nullptr);g_assert_cmpfloat_with_epsilon(actual,values[n],1e-6);g_assert_cmpfloat_with_epsilon(gtk_spin_button_get_value(GTK_SPIN_BUTTON(b.get(names[n]))),values[n],1e-6);}
 }else if(mode==1){View a(options,false,true),b(options,true);curve_controls(a,b,options);
 }else if(mode==2){
  View a(canonical,false),b(canonical,true);gtk_spin_button_set_value(GTK_SPIN_BUTTON(a.get("opaque")),.427);g_assert_cmpfloat_with_epsilon(gtk_spin_button_get_value(GTK_SPIN_BUTTON(b.get("opaque"))),.427,1e-6);g_assert_true(info->tool_options==GIMP_TOOL_OPTIONS(canonical));
 }else if(mode==3){
  gpointer weak=options;g_object_add_weak_pointer(G_OBJECT(options),&weak);
  {View a(options,false),b(options,true);g_object_unref(options);options=nullptr;g_assert_nonnull(weak);gtk_spin_button_set_value(GTK_SPIN_BUTTON(a.get("opaque")),.287);g_assert_cmpfloat_with_epsilon(gtk_spin_button_get_value(GTK_SPIN_BUTTON(b.get("opaque"))),.287,1e-6);}g_assert_null(weak);
 }else if(mode==4){
  g_assert_null(gimp_mypaint_options_gui(nullptr));g_assert_null(gimp_mypaint_options_gui_horizontal(nullptr));auto*wrong=g_object_new(G_TYPE_OBJECT,nullptr);g_assert_null(gimp_mypaint_options_gui(reinterpret_cast<GimpToolOptions*>(wrong)));g_object_unref(wrong);
  g_object_run_dispose(G_OBJECT(options));g_assert_null(gimp_mypaint_options_gui(GIMP_TOOL_OPTIONS(options)));g_assert_null(gimp_mypaint_options_gui_horizontal(GIMP_TOOL_OPTIONS(options)));auto typed=MypaintOptionsRef::retain(options);g_assert_false(bool(typed.gui()));g_assert_false(bool(typed.gui_horizontal()));
 }else if(mode==5){
  BindingStore::require(G_OBJECT(app)).close();g_assert_null(gimp_mypaint_options_gui(GIMP_TOOL_OPTIONS(options)));g_assert_null(gimp_mypaint_options_gui_horizontal(GIMP_TOOL_OPTIONS(options)));
 }else if(mode==6){
  Acquisition a;g_object_add_toggle_ref(G_OBJECT(options),dispose_on_acquisition,&a);g_object_unref(options);a.armed=true;
  g_assert_null(gimp_mypaint_options_gui(GIMP_TOOL_OPTIONS(options)));g_assert_cmpuint(a.calls,==,1);g_object_remove_toggle_ref(G_OBJECT(options),dispose_on_acquisition,&a);options=nullptr;
 }else if(mode==7){
  Construction c{app};guint signal=g_signal_lookup("show",GTK_TYPE_WIDGET);g_assert_cmpuint(signal,>,0);gulong hook=g_signal_add_emission_hook(signal,0,close_app_on_show,&c,nullptr);g_assert_cmpuint(hook,>,0);
  auto*widget=gimp_mypaint_options_gui_horizontal(GIMP_TOOL_OPTIONS(options));g_signal_remove_emission_hook(signal,hook);g_assert_cmpuint(c.calls,==,1);g_assert_null(widget);g_assert_null(c.weak_editor);
 }else{View view(options,mode==9);set_curve(options);snapshot(view,mode==9);}
 if(mode!=2)g_assert_true(MypaintOptionsRef::retain(canonical).snapshot().encode()==canonical_before);
 if(options)g_object_unref(options);
 gimp_test_utils_set_gimp3_directory("GIMP_TESTING_ABS_TOP_BUILDDIR","app/tests/gimpdir-output");gimp_exit(app,TRUE);
}
int main(int argc,char**argv){g_test_init(&argc,&argv,nullptr);if(!gtk_init_check(&argc,&argv))return GIMP_EXIT_TEST_SKIPPED;
 const char*names[]={"independent-controls","shared-curve-controls","canonical-controls","owner-first-teardown","invalid-closed-options","closed-application","dispose-during-acquisition","close-app-during-construction","vertical-portrait","horizontal-portrait"};
 guint count=g_getenv("GIMP_OPTIONS_GUI_SCREENSHOT_DIR")?10:8;
 for(guint i=0;i<count;i++){gchar*name=g_strconcat("/original-options-gui/",names[i],nullptr);g_test_add_data_func(name,GUINT_TO_POINTER(i),run_case);g_free(name);}
 gimp_test_utils_setup_menus_path();app=gimp_init_for_gui_testing(TRUE);
 g_application_run(app->app,0,nullptr);int result=gimp_core_app_get_exit_status(GIMP_CORE_APP(app->app));g_application_quit(G_APPLICATION(app->app));g_clear_object(&app->app);return result;}
