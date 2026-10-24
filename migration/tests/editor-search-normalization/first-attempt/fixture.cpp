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

static GtkTreeModel*list(Editor&e){return gtk_tree_view_get_model(GTK_TREE_VIEW(e.get("painter-brush-list")));}
static void search(Editor&e,const char*query,const std::vector<GimpPainterMybrush*>&expected){
 gtk_entry_set_text(GTK_ENTRY(e.get("painter-brush-search")),query);auto*m=list(e);g_assert_cmpint(gtk_tree_model_iter_n_children(m,nullptr),==,int(expected.size()));
 std::vector<GimpPainterMybrush*>seen;GtkTreeIter iter;for(gboolean valid=gtk_tree_model_get_iter_first(m,&iter);valid;valid=gtk_tree_model_iter_next(m,&iter)){
  GimpPainterMybrush*b=nullptr;gchar*name=nullptr;gtk_tree_model_get(m,&iter,0,&b,2,&name,-1);g_assert_nonnull(b);g_assert_cmpstr(name,==,gimp_object_get_name(b));seen.push_back(b);g_free(name);g_object_unref(b);
 }for(auto*b:expected)g_assert_cmpuint(std::count(seen.begin(),seen.end(),b),==,1);
}
static void select(Editor&e,GimpPainterMybrush*desired){
 auto*m=list(e);GtkTreeIter iter;bool found=false;for(gboolean valid=gtk_tree_model_get_iter_first(m,&iter);valid;valid=gtk_tree_model_iter_next(m,&iter)){
  GimpPainterMybrush*b=nullptr;gtk_tree_model_get(m,&iter,0,&b,-1);bool match=b==desired;g_object_unref(b);if(match){gtk_tree_selection_select_iter(gtk_tree_view_get_selection(GTK_TREE_VIEW(e.get("painter-brush-list"))),&iter);found=true;break;}
 }g_assert_true(found);g_assert_true(gimp_context_get_painter_mybrush(GIMP_CONTEXT(e.model.get()))==desired);
}
static void assert_controls(Editor&e,const MyPaint::Resource&r){
 for(auto&setting:painter_mypaint_settings){std::string name=setting.internal_name;std::replace(name.begin(),name.end(),'_','-');g_assert_cmpfloat(gtk_spin_button_get_value(GTK_SPIN_BUTTON(e.get(name.c_str()))),==,r.base_value(setting.index));}
 for(auto&setting:painter_mypaint_switches){std::string name=setting.internal_name;std::replace(name.begin(),name.end(),'_','-');g_assert_cmpint(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(e.get(name.c_str()))),==,r.switch_value(setting.index));}
 for(auto&setting:painter_mypaint_texts){std::string name=setting.internal_name;std::replace(name.begin(),name.end(),'_','-');const auto value=r.text_value(setting.index);g_assert_cmpstr(gtk_entry_get_text(GTK_ENTRY(e.get(name.c_str()))),==,value.c_str());}
 for(int input=0;input<INPUT_COUNT;++input){gtk_combo_box_set_active(GTK_COMBO_BOX(e.get("painter-curve-setting")),BRUSH_TEXTURE_GRAIN);gtk_combo_box_set_active(GTK_COMBO_BOX(e.get("painter-curve-input")),input);auto pts=r.curve(BRUSH_TEXTURE_GRAIN,input);auto*combo=GTK_COMBO_BOX(e.get("painter-curve-point"));g_assert_cmpint(gtk_tree_model_iter_n_children(gtk_combo_box_get_model(combo),nullptr),==,int(pts.size()));for(unsigned n=0;n<pts.size();++n){gtk_combo_box_set_active(combo,n);g_assert_cmpfloat(gtk_spin_button_get_value(GTK_SPIN_BUTTON(e.get("painter-curve-x"))),==,pts[n].x);g_assert_cmpfloat(gtk_spin_button_get_value(GTK_SPIN_BUTTON(e.get("painter-curve-y"))),==,pts[n].y);}}
 g_assert_true(e.snapshot().encode()==r.encode());
}
static void list_history(){
 auto*factory=gimp_data_factory_get_container(gimp->painter_mybrush_factory);const auto count=gimp_container_get_n_children(factory);
 auto a=PainterMybrushRef::create("List Straße 日本語"),b=PainterMybrushRef::create("List Second"),c=PainterMybrushRef::create("List Third");
 MyPaint::Resource ar;ar.set_group("Group Ω");ar.set_base_value(BRUSH_OPAQUE,.25);a.replace(ar);MyPaint::Resource br;br.set_group("Group other");br.set_base_value(BRUSH_OPAQUE,.75);b.replace(br);
 auto*tag=gimp_tag_new("InkTag");gimp_tagged_add_tag(GIMP_TAGGED(a.get()),tag);g_object_unref(tag);tag=gimp_tag_new("OtherTag");gimp_tagged_add_tag(GIMP_TAGGED(b.get()),tag);g_object_unref(tag);
 for(auto*p:{a.get(),b.get(),c.get()}){gimp_data_make_internal(GIMP_DATA(p),gimp_object_get_name(p));gimp_container_add(factory,GIMP_OBJECT(p));}
 const auto a_json=a.snapshot().encode(),b_json=b.snapshot().encode(),c_json=c.snapshot().encode();
 {
  auto options=ObjectRef<GimpPainterMybrushOptions>::adopt(GIMP_PAINTER_MYBRUSH_OPTIONS(g_object_new(GIMP_TYPE_PAINTER_MYBRUSH_OPTIONS,"gimp",gimp,nullptr)));gimp_context_set_painter_mybrush(GIMP_CONTEXT(options.get()),a.get());Editor first(true,GIMP_CONTEXT(options.get())),peer(false,GIMP_CONTEXT(options.get()));g_assert_true(first.model.get()==peer.model.get());
  search(first,"STRASSE",{a.get()});select(first,a.get());search(first,"日本語",{a.get()});search(first,"group ω",{a.get()});search(first,"inktag",{a.get()});search(first,"missing-list-proof",{});g_assert_true(gimp_context_get_painter_mybrush(GIMP_CONTEXT(options.get()))==a.get());search(first,"List",{a.get(),b.get(),c.get()});
  // Ordinary local tag editing; no invented asynchronous peer-refresh requirement.
  gtk_entry_set_text(GTK_ENTRY(first.get("painter-brush-tags")),"FreshTag, 日本タグ");g_signal_emit_by_name(first.get("painter-brush-tags"),"activate");search(first,"freshtag",{a.get()});search(first,"日本タグ",{a.get()});search(first,"inktag",{});search(peer,"freshtag",{a.get()});search(first,"List",{a.get(),b.get(),c.get()});
  auto draft=MyPaint::Resource::decode("{\"version\":3,\"unknown_history\":{\"nested\":[null,9007199254740993,\"日本語\"]},\"settings\":{},\"texts\":{\"future_history_reference\":\"retained\"}}");
  for(auto&s:painter_mypaint_settings)draft.set_base_value(s.index,double(s.minimum)+(double(s.maximum)-double(s.minimum))*.37);
  for(auto&s:painter_mypaint_switches)draft.set_switch(s.index,true);
  draft.set_text(BRUSH_BRUSHMARK_NAME,"missing history shape 日本語");draft.set_text(BRUSH_TEXTURE_NAME,"missing history paper 日本語");for(int n=0;n<INPUT_COUNT;++n)draft.set_curve(BRUSH_TEXTURE_GRAIN,n,{{-1,.123456789012+n*.01},{0,.654321098765},{1,-.234567890123}});
  first.set(draft);gtk_entry_set_text(GTK_ENTRY(first.get("painter-brush-name")),"Unsaved draft 日本語");const guint history=gimp_painter_mybrush_options_history_size(options.get());assert_controls(first,draft);assert_controls(peer,draft);
  select(first,b.get());g_assert_cmpuint(gimp_painter_mybrush_options_history_size(options.get()),==,history+1);g_assert_true(first.snapshot().encode()==b_json);g_assert_true(peer.snapshot().encode()==b_json);g_assert_true(a.snapshot().encode()==a_json);g_assert_true(b.snapshot().encode()==b_json);
  gimp_test_run_mainloop_until_idle();auto*combo=GTK_COMBO_BOX(peer.get("painter-brush-history"));g_assert_cmpint(gtk_tree_model_iter_n_children(gtk_combo_box_get_model(combo),nullptr),>=,int(history+1));gtk_combo_box_set_active(combo,history);
  g_assert_true(gimp_context_get_painter_mybrush(GIMP_CONTEXT(options.get()))==a.get());assert_controls(first,draft);assert_controls(peer,draft);
  for(auto*e:{&first,&peer})g_assert_cmpstr(gtk_entry_get_text(GTK_ENTRY(e->get("painter-brush-name"))),==,"Unsaved draft 日本語");gboolean dirty=FALSE,name_dirty=FALSE;g_object_get(options.get(),"painter-dirty",&dirty,"painter-name-dirty",&name_dirty,nullptr);g_assert_true(dirty);g_assert_true(name_dirty);
  g_assert_true(a.snapshot().encode()==a_json);g_assert_true(b.snapshot().encode()==b_json);g_assert_true(c.snapshot().encode()==c_json);g_assert_cmpstr(gimp_object_get_name(a.get()),==,"List Straße 日本語");
  g_test_message("Unicode/casefold/group/tag row identity and selection PASS; full45 scalar/5switch/2reference/9input curves + unknown/name draft restored through peer combo; saved resources unchanged");
 }
 for(auto*p:{a.get(),b.get(),c.get()})gimp_container_remove(factory,GIMP_OBJECT(p));g_assert_cmpint(gimp_container_get_n_children(factory),==,count);
}
int main(int argc,char**argv){g_test_init(&argc,&argv,nullptr);if(!gtk_init_check(&argc,&argv))return GIMP_EXIT_TEST_SKIPPED;gimp_test_utils_setup_menus_path();gimp=gimp_init_for_gui_testing(TRUE);add_editor_case("/painter-list/history-full-state",list_history);g_application_run(gimp->app,0,nullptr);int result=gimp_core_app_get_exit_status(GIMP_CORE_APP(gimp->app));g_application_quit(gimp->app);g_clear_object(&gimp->app);return result;}
