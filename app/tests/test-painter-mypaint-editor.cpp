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
#include "display/display-types.h"
#include "actions/actions-types.h"
#include "actions/actions.h"
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
  gchar*path=nullptr;g_object_get(gimp->config,"painter-mypaint-brush-path-writable",&path,"painter-mypaint-brush-path",&original_search,nullptr);
  GError*error=nullptr;gchar*expanded=gimp_config_path_expand(path,TRUE,&error);g_assert_no_error(error);g_assert_nonnull(expanded);
  writable_directory=expanded;g_assert_nonnull(strstr(expanded,"painter-mypaint-brushes"));g_assert_cmpint(g_mkdir_with_parents(expanded,0700),==,0);
  gchar*corpus=g_build_filename(g_getenv("GIMP_TESTING_ABS_TOP_SRCDIR"),"data/painter-mypaint-brushes",nullptr);
  gchar*search=g_strconcat(expanded,G_SEARCHPATH_SEPARATOR_S,corpus,nullptr);g_object_set(gimp->config,"painter-mypaint-brush-path",search,nullptr);
  g_free(search);g_free(corpus);g_free(expanded);g_free(path);
  gimp_data_factory_data_refresh(gimp->painter_mybrush_factory,gimp->user_context);
  auto*c=gimp_data_factory_get_container(gimp->painter_mybrush_factory);g_assert_cmpint(gimp_container_get_n_children(c),==,177);
  gimp_context_set_painter_mybrush(gimp->user_context,GIMP_PAINTER_MYBRUSH(gimp_container_get_child_by_index(c,0)));
}
static void constructors_and_shared_model(){
  prepare();GError*e=nullptr;
  g_assert_null(gimp_painter_mybrush_editor_new(nullptr,FALSE,&e));g_assert_nonnull(e);g_clear_error(&e);
  auto*wrong=g_object_new(G_TYPE_OBJECT,nullptr);g_assert_null(gimp_painter_mybrush_editor_new(reinterpret_cast<GimpContext*>(wrong),FALSE,&e));g_assert_nonnull(e);g_clear_error(&e);g_object_unref(wrong);
  auto*context=gimp_context_new(gimp,"unselected editor",nullptr);
  {Editor a(false,context),b(true,GIMP_CONTEXT(a.model.get()));g_assert_true(a.model.get()==b.model.get());g_assert_nonnull(gimp_context_get_painter_mybrush(GIMP_CONTEXT(a.model.get())));
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(a.get("opaque")),.423);g_assert_cmpfloat_with_epsilon(gtk_spin_button_get_value(GTK_SPIN_BUTTON(b.get("opaque"))),.423,1e-6);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(b.get("non-incremental")),TRUE);g_assert_true(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(a.get("non-incremental"))));
    auto selected=PainterMybrushRef::create("ordinary-context-update");auto selected_settings=selected.snapshot();selected_settings.set_base_value(BRUSH_OPAQUE,.612);selected.replace(selected_settings);
    gimp_context_set_painter_mybrush(context,selected.get());g_assert_cmpfloat_with_epsilon(a.snapshot().base_value(BRUSH_OPAQUE),.612,1e-6);
    g_assert_cmpfloat_with_epsilon(gtk_spin_button_get_value(GTK_SPIN_BUTTON(b.get("opaque"))),.612,1e-6);
    gimp_docked_set_context(GIMP_DOCKED(a.widget),nullptr);g_assert_null(find(GTK_WIDGET(a.widget),"opaque"));
    gimp_painter_mybrush_editor_request_preview(a.widget);g_assert_null(gimp_painter_mybrush_editor_ref_preview(a.widget));
    g_assert_false(gimp_painter_mybrush_editor_save(a.widget,nullptr,&e));g_assert_nonnull(e);g_clear_error(&e);
    gimp_docked_set_context(GIMP_DOCKED(a.widget),context);g_assert_nonnull(a.get("opaque"));
    auto*popup=gimp_painter_mybrush_editor_popup(GIMP_CONTEXT(a.model.get()),a.window,&e);g_assert_no_error(e);g_assert_nonnull(popup);gtk_widget_destroy(popup);
  }
  g_object_run_dispose(G_OBJECT(context));g_assert_null(gimp_painter_mybrush_editor_new(context,FALSE,&e));g_assert_nonnull(e);g_clear_error(&e);g_object_unref(context);
}
static void all_controls_and_curves(){
  Editor a;auto r=MyPaint::Resource::decode("{\"version\":3,\"unknown\":{\"preserve\":[1,2]},\"settings\":{},\"texts\":{\"future_reference\":\"retained\"}}");a.set(r);
  for(auto&s:painter_mypaint_settings){std::string key=s.internal_name;std::replace(key.begin(),key.end(),'_','-');auto*w=a.get(key.c_str());double value=s.minimum+(s.maximum-s.minimum)*.37;gtk_spin_button_set_value(GTK_SPIN_BUTTON(w),value);g_assert_cmpfloat_with_epsilon(a.snapshot().base_value(s.index),value,1e-5);}
  for(auto&s:painter_mypaint_switches){std::string key=s.internal_name;std::replace(key.begin(),key.end(),'_','-');gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(a.get(key.c_str())),TRUE);g_assert_true(a.snapshot().switch_value(s.index));}
  for(auto&s:painter_mypaint_texts){std::string key=s.internal_name;std::replace(key.begin(),key.end(),'_','-');gtk_entry_set_text(GTK_ENTRY(a.get(key.c_str())),"missing resource retained");g_signal_emit_by_name(a.get(key.c_str()),"activate");g_assert_true(a.snapshot().text_value(s.index)=="missing resource retained");}
  for(int input=0;input<INPUT_COUNT;++input){
    gtk_combo_box_set_active(GTK_COMBO_BOX(a.get("painter-curve-setting")),BRUSH_TEXTURE_GRAIN);gtk_combo_box_set_active(GTK_COMBO_BOX(a.get("painter-curve-input")),input);
    gtk_button_clicked(GTK_BUTTON(a.get("painter-curve-clear")));gtk_button_clicked(GTK_BUTTON(a.get("painter-curve-add")));
    for(int n=2;n<8;++n)gtk_button_clicked(GTK_BUTTON(a.get("painter-curve-add")));
    g_assert_cmpuint(a.snapshot().curve(BRUSH_TEXTURE_GRAIN,input).size(),==,8);
    gtk_combo_box_set_active(GTK_COMBO_BOX(a.get("painter-curve-point")),0);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(a.get("painter-curve-x")),-.12345678);gtk_spin_button_set_value(GTK_SPIN_BUTTON(a.get("painter-curve-y")),.87654321);gtk_button_clicked(GTK_BUTTON(a.get("painter-curve-apply")));
    auto points=a.snapshot().curve(BRUSH_TEXTURE_GRAIN,input);g_assert_cmpfloat_with_epsilon(points[0].x,-.12345678,1e-8);g_assert_cmpfloat_with_epsilon(points[0].y,.87654321,1e-8);
    gtk_button_clicked(GTK_BUTTON(a.get("painter-curve-remove")));g_assert_cmpuint(a.snapshot().curve(BRUSH_TEXTURE_GRAIN,input).size(),==,7);
  }
  g_assert_nonnull(strstr(a.snapshot().encode().c_str(),"preserve"));g_assert_nonnull(strstr(a.snapshot().encode().c_str(),"future_reference"));
  const auto edited=a.snapshot();GError*error=nullptr;
  g_assert_true(gimp_painter_mybrush_editor_save(a.widget,"Painter All Controls Roundtrip",&error));g_assert_no_error(error);
  auto saved=ObjectRef<GimpPainterMybrush>::retain(gimp_context_get_painter_mybrush(GIMP_CONTEXT(a.model.get())));
  auto*file=gimp_data_get_file(GIMP_DATA(saved.get()));g_assert_nonnull(file);
  auto stream=ObjectRef<GObject>::adopt(G_OBJECT(g_file_read(file,nullptr,&error)));g_assert_no_error(error);
  auto disk=MyPaint::Resource::load(G_INPUT_STREAM(stream.get()));auto expected=edited;expected.set_parent_brush_name("Painter All Controls Roundtrip");
  g_assert_true(disk.encode()==expected.encode());
  gimp_context_set_painter_mybrush(GIMP_CONTEXT(a.model.get()),GIMP_PAINTER_MYBRUSH(gimp_painter_mybrush_get_standard(GIMP_CONTEXT(a.model.get()))));
  gimp_context_set_painter_mybrush(GIMP_CONTEXT(a.model.get()),saved.get());g_assert_true(a.snapshot().encode()==edited.encode());
  g_assert_true(gimp_painter_mybrush_editor_delete(a.widget,&error));g_assert_no_error(error);
}
static void list_search_history(){
  auto*c=gimp_data_factory_get_container(gimp->painter_mybrush_factory);auto*first=GIMP_PAINTER_MYBRUSH(gimp_container_get_child_by_index(c,0));auto*second=GIMP_PAINTER_MYBRUSH(gimp_container_get_child_by_index(c,1));gimp_context_set_painter_mybrush(gimp->user_context,first);
  Editor a;auto r=a.snapshot();r.set_base_value(BRUSH_OPAQUE,.137);r.set_text(BRUSH_TEXTURE_NAME,"missing history texture");r.set_curve(BRUSH_SMUDGE,INPUT_PRESSURE,{{0,.123},{.5,.789},{1,.456}});a.set(r);
  guint before=gimp_painter_mybrush_options_history_size(a.model.get());gimp_context_set_painter_mybrush(GIMP_CONTEXT(a.model.get()),second);g_assert_cmpuint(gimp_painter_mybrush_options_history_size(a.model.get()),==,before+1);
  gtk_combo_box_set_active(GTK_COMBO_BOX(a.get("painter-brush-history")),before);
  g_assert_cmpfloat_with_epsilon(a.snapshot().base_value(BRUSH_OPAQUE),.137,1e-6);g_assert_true(a.snapshot().text_value(BRUSH_TEXTURE_NAME)=="missing history texture");g_assert_cmpuint(a.snapshot().curve(BRUSH_SMUDGE,INPUT_PRESSURE).size(),==,3);
  gtk_entry_set_text(GTK_ENTRY(a.get("painter-brush-search")),"no-such-painter-resource-92");auto*model=gtk_tree_view_get_model(GTK_TREE_VIEW(a.get("painter-brush-list")));g_assert_cmpint(gtk_tree_model_iter_n_children(model,nullptr),==,0);
  gtk_entry_set_text(GTK_ENTRY(a.get("painter-brush-search")),"");g_assert_cmpint(gtk_tree_model_iter_n_children(model,nullptr),==,177);
  auto*tag=gimp_tag_new("editor-test-tag");gimp_tagged_add_tag(GIMP_TAGGED(first),tag);gtk_entry_set_text(GTK_ENTRY(a.get("painter-brush-search")),"editor-test-tag");g_assert_cmpint(gtk_tree_model_iter_n_children(model,nullptr),==,1);gimp_tagged_remove_tag(GIMP_TAGGED(first),tag);g_object_unref(tag);
}
static void save_conflict_default_path(){
  auto*c=gimp_data_factory_get_container(gimp->painter_mybrush_factory);auto*first=GIMP_PAINTER_MYBRUSH(gimp_container_get_child_by_index(c,0));gimp_context_set_painter_mybrush(gimp->user_context,first);
  const int standard_count=gimp_container_get_n_children(gimp_data_factory_get_container(gimp->mybrush_factory));
  Editor a;GError*e=nullptr;g_assert_false(gimp_data_is_writable(GIMP_DATA(first)));g_assert_false(gimp_painter_mybrush_editor_save(a.widget,nullptr,&e));g_assert_nonnull(e);g_clear_error(&e);
  auto r=MyPaint::Resource::decode("{\"version\":3,\"unknown\":{\"save\":true}}");r.set_base_value(BRUSH_STROKE_OPACITY,.351);r.set_switch(BRUSH_NON_INCREMENTAL,true);r.set_text(BRUSH_BRUSHMARK_NAME,"unresolved saved shape");r.set_curve(BRUSH_SMUDGE,INPUT_CUSTOM,{{-.2,.1},{-.2,.2},{0,.3},{.3,.4},{.5,.6},{.8,.7},{1,.8},{2,.9}});a.set(r);
  /* The default configured writable location must also work on an upgrade
   * where the new Painter directory does not exist yet. Only our empty test
   * directory is removed; native factory Save As must recreate it. */
  g_assert_cmpint(g_rmdir(writable_directory.c_str()),==,0);
  g_assert_false(g_file_test(writable_directory.c_str(),G_FILE_TEST_EXISTS));
  g_assert_true(gimp_painter_mybrush_editor_save(a.widget,"Painter Editor Default Save",&e));g_assert_no_error(e);
  auto selected=ObjectRef<GimpPainterMybrush>::retain(gimp_context_get_painter_mybrush(GIMP_CONTEXT(a.model.get())));auto*file=gimp_data_get_file(GIMP_DATA(selected.get()));g_assert_nonnull(file);String path(g_file_get_path(file));g_assert_true(g_str_has_prefix(path.get(),writable_directory.c_str()));g_assert_true(g_file_test(path.get(),G_FILE_TEST_EXISTS));
  auto stream=ObjectRef<GObject>::adopt(G_OBJECT(g_file_read(file,nullptr,&e)));g_assert_no_error(e);auto reloaded=MyPaint::Resource::load(G_INPUT_STREAM(stream.get()));
  g_assert_cmpuint(reloaded.curve(BRUSH_SMUDGE,INPUT_CUSTOM).size(),==,8);g_assert_nonnull(strstr(reloaded.encode().c_str(),"unknown"));g_assert_true(reloaded.text_value(BRUSH_BRUSHMARK_NAME)=="unresolved saved shape");
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(a.get("opaque")),.234);auto external=PainterMybrushRef::retain(selected.get()).snapshot();external.set_base_value(BRUSH_HARDNESS,.345);PainterMybrushRef::retain(selected.get()).replace(external);
  gboolean conflict=FALSE;g_object_get(a.model.get(),"painter-conflict",&conflict,nullptr);g_assert_true(conflict);g_assert_false(gimp_painter_mybrush_editor_save(a.widget,nullptr,&e));g_assert_nonnull(e);g_clear_error(&e);
  g_assert_true(gimp_painter_mybrush_editor_save(a.widget,"Painter Editor Conflict Copy",&e));g_assert_no_error(e);g_assert_cmpfloat_with_epsilon(a.snapshot().base_value(BRUSH_OPAQUE),.234,1e-6);
  Editor peer(true,GIMP_CONTEXT(a.model.get()));
  g_assert_true(gimp_painter_mybrush_editor_rename(a.widget,"Painter Editor Renamed",&e));g_assert_no_error(e);g_assert_cmpstr(gimp_object_get_name(gimp_context_get_painter_mybrush(GIMP_CONTEXT(a.model.get()))),==,"Painter Editor Renamed");
  g_assert_cmpstr(gtk_entry_get_text(GTK_ENTRY(peer.get("painter-brush-name"))),==,"Painter Editor Renamed");
  g_assert_true(gimp_painter_mybrush_editor_delete(a.widget,&e));g_assert_no_error(e);
  gimp_context_set_painter_mybrush(GIMP_CONTEXT(a.model.get()),selected.get());g_assert_true(gimp_painter_mybrush_editor_delete(a.widget,&e));g_assert_no_error(e);g_assert_false(g_file_test(path.get(),G_FILE_TEST_EXISTS));g_assert_cmpint(gimp_container_get_n_children(c),==,177);
  g_assert_cmpint(gimp_container_get_n_children(gimp_data_factory_get_container(gimp->mybrush_factory)),==,standard_count);
}
static void preview_deterministic_extended(){
  Editor a;MyPaint::Resource r;r.set_base_value(BRUSH_RADIUS_LOGARITHMIC,2.2);r.set_base_value(BRUSH_OPAQUE,.5);r.set_base_value(BRUSH_OFFSET_BY_RANDOM,.2);a.set(r);
  auto baseline=a.preview();gimp_painter_mybrush_editor_request_preview(a.widget);g_assert_true(a.preview()==baseline);
  auto first_revision=gimp_painter_mybrush_editor_preview_revision(a.widget);
  r.set_base_value(BRUSH_SMUDGE,1);a.set(r);auto smudge=a.preview();g_assert_true(smudge!=baseline);
  r.set_base_value(BRUSH_SMUDGE,.4);r.set_switch(BRUSH_NON_INCREMENTAL,true);r.set_base_value(BRUSH_STROKE_OPACITY,.21);a.set(r);auto nonincremental=a.preview();g_assert_true(nonincremental!=smudge);
  auto*pattern=GIMP_PATTERN(gimp_pattern_new(GIMP_CONTEXT(a.model.get()),"editor-test-paper"));auto*mask=gimp_pattern_get_mask(pattern);g_assert_nonnull(mask);auto*bytes=gimp_temp_buf_get_data(mask);gsize size=gimp_temp_buf_get_data_size(mask);for(gsize n=0;n<size;++n)bytes[n]=(n%7)*37;
  gimp_context_set_pattern(GIMP_CONTEXT(a.model.get()),pattern);r.set_switch(BRUSH_USE_GIMP_TEXTURE,true);r.set_text(BRUSH_TEXTURE_NAME,nullptr);r.set_base_value(BRUSH_TEXTURE_CONTRAST,.8);r.set_base_value(BRUSH_TEXTURE_GRAIN,.2);a.set(r);auto paper=a.preview();g_assert_true(paper!=nonincremental);gimp_painter_mybrush_editor_request_preview(a.widget);g_assert_true(a.preview()==paper);g_object_unref(pattern);
  r.set_base_value(BRUSH_OPAQUE,.71);a.set(r);g_assert_null(gimp_painter_mybrush_editor_ref_preview(a.widget));r.set_base_value(BRUSH_OPAQUE,.14);a.set(r);auto latest=a.preview();g_assert_true(latest!=paper);g_assert_cmpuint(gimp_painter_mybrush_editor_preview_revision(a.widget),>,first_revision);
}
static void legacy_preview_oracle(){
  const std::string root=g_getenv("GIMP_TESTING_ABS_TOP_SRCDIR");
  std::ifstream hashes(root+"/migration/fixtures/legacy-mypaint-preview/preview-hashes.tsv");g_assert_true(hashes.good());
  gchar*text=nullptr;gsize length=0;GError*error=nullptr;
  g_assert_true(g_file_get_contents((root+"/migration/fixtures/legacy-mypaint-session/session-base.myb").c_str(),&text,&length,&error));g_assert_no_error(error);
  auto base=MyPaint::Resource::decode(text);g_free(text);Editor editor(true);
  auto*brush=GIMP_BRUSH(g_object_new(GIMP_TYPE_BRUSH,"name","preview-bitmap",nullptr));
  brush->priv->mask=gimp_temp_buf_new(5,3,babl_format("Y u8"));const guchar mask[]={0,30,90,160,255,12,60,120,180,240,255,190,140,70,0};std::memcpy(gimp_temp_buf_get_data(brush->priv->mask),mask,sizeof mask);
  auto*paper=GIMP_PATTERN(g_object_new(GIMP_TYPE_PATTERN,"name","preview-paper",nullptr));
  paper->mask=gimp_temp_buf_new(3,2,babl_format("R'G'B' u8"));const guchar texture[]={20,250,10,100,0,50,240,2,180,200,12,33,64,25,70,128,100,5};std::memcpy(gimp_temp_buf_get_data(paper->mask),texture,sizeof texture);
  gimp_context_set_brush(GIMP_CONTEXT(editor.model.get()),brush);gimp_context_set_pattern(GIMP_CONTEXT(editor.model.get()),paper);
  for(int scenario=0;scenario<16;++scenario){
    int expected_index=-1;std::string expected;hashes>>expected_index>>expected;g_assert_cmpint(expected_index,==,scenario);
    auto r=base;r.set_base_value(BRUSH_DABS_PER_SECOND,40);r.set_base_value(BRUSH_RADIUS_LOGARITHMIC,2.3);
    r.set_base_value(BRUSH_OPAQUE,.65);r.set_base_value(BRUSH_COLOR_H,.63);r.set_base_value(BRUSH_COLOR_S,.7);r.set_base_value(BRUSH_COLOR_V,.6);
    r.set_switch(BRUSH_NON_INCREMENTAL,scenario&1);r.set_base_value(BRUSH_STROKE_OPACITY,.37);
    r.set_base_value(BRUSH_SMUDGE,scenario&8?.8:0);r.set_base_value(BRUSH_SMUDGE_LENGTH,.65);
    r.set_switch(BRUSH_USE_GIMP_BRUSHMARK,scenario&2);r.set_switch(BRUSH_USE_GIMP_TEXTURE,scenario&4);
    r.set_base_value(BRUSH_TEXTURE_GRAIN,.14);r.set_base_value(BRUSH_TEXTURE_CONTRAST,.8);
    r.set_text(BRUSH_BRUSHMARK_NAME,nullptr);r.set_text(BRUSH_TEXTURE_NAME,nullptr);editor.set(r);
    auto pixels=editor.preview();String hash(g_compute_checksum_for_data(G_CHECKSUM_SHA256,pixels.data(),pixels.size()));
    g_test_message("old native preview scene %d: %s",scenario,hash.get());
    if(expected!=hash.get()){
      const std::string output=std::string(g_getenv("GIMP_TESTING_ABS_TOP_BUILDDIR"))+"/app/tests/painter-preview-failed-"+std::to_string(scenario)+".rgba";
      g_file_set_contents(output.c_str(),reinterpret_cast<const gchar*>(pixels.data()),pixels.size(),nullptr);
    }
    g_assert_cmpstr(hash.get(),==,expected.c_str());
  }
  g_object_unref(brush);g_object_unref(paper);
}
static void replacement_and_close(){
  Editor a;auto*old=a.get("opaque");g_object_ref(old);auto*context=gimp_context_new(gimp,"replacement",nullptr);GError*e=nullptr;
  g_assert_true(gimp_painter_mybrush_editor_set_context(a.widget,context,&e));g_assert_no_error(e);auto before=a.snapshot().base_value(BRUSH_OPAQUE);gtk_spin_button_set_value(GTK_SPIN_BUTTON(old),.987);g_assert_cmpfloat(a.snapshot().base_value(BRUSH_OPAQUE),==,before);g_object_unref(old);g_object_unref(context);
  auto*widget=G_OBJECT(a.widget);g_object_ref(widget);gimp_painter_mybrush_editor_request_preview(a.widget);gtk_widget_destroy(a.window);g_object_unref(a.window);a.window=nullptr;
  g_assert_false(gimp_painter_mybrush_editor_preview_pending(a.widget));g_assert_null(gimp_painter_mybrush_editor_ref_preview(a.widget));g_object_run_dispose(widget);g_object_run_dispose(widget);g_object_unref(widget);
  auto*w=gimp_painter_mybrush_editor_new(gimp->user_context,TRUE,&e);g_assert_no_error(e);g_object_ref_sink(w);GObject*weak=G_OBJECT(w);g_object_add_weak_pointer(weak,reinterpret_cast<gpointer*>(&weak));
  g_signal_connect(w,"preview-ready",G_CALLBACK(+[](GimpPainterMybrushEditor*e,guint64,gpointer){gtk_widget_destroy(GTK_WIDGET(e));g_object_unref(e);}),nullptr);
  gint64 deadline=g_get_monotonic_time()+20*G_TIME_SPAN_SECOND;while(weak&&g_get_monotonic_time()<deadline)g_main_context_iteration(nullptr,FALSE);g_assert_null(weak);
}
static void close_during_refresh(){
  {
    auto*options=GIMP_PAINTER_MYBRUSH_OPTIONS(g_object_new(GIMP_TYPE_PAINTER_MYBRUSH_OPTIONS,"gimp",gimp,nullptr));
    Editor detached(false,GIMP_CONTEXT(options));gimp_painter_mybrush_editor_request_preview(detached.widget);
    g_assert_true(gimp_painter_binding_close(G_OBJECT(options),nullptr));
    while(gimp_painter_mybrush_editor_preview_pending(detached.widget))g_main_context_iteration(nullptr,FALSE);
    g_assert_null(gimp_painter_mybrush_editor_ref_preview(detached.widget));g_object_unref(options);
  }
  Editor editor;auto*owner=G_OBJECT(editor.widget);g_object_ref(owner);
  auto*adjustment=gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(editor.get("opaque")));
  g_signal_connect(adjustment,"value-changed",G_CALLBACK(+[](GtkAdjustment*,gpointer data){gtk_widget_destroy(GTK_WIDGET(data));}),editor.widget);
  g_object_set(editor.model.get(),"opaque",.261,nullptr);
  g_assert_false(gimp_painter_mybrush_editor_preview_pending(editor.widget));
  g_assert_null(gimp_painter_mybrush_editor_ref_preview(editor.widget));g_object_unref(owner);
}
static void owner_destruction_orders()
{
  for (bool options_first : {true, false})
    {
      auto *options = GIMP_PAINTER_MYBRUSH_OPTIONS (g_object_new (
        GIMP_TYPE_PAINTER_MYBRUSH_OPTIONS, "gimp", gimp, nullptr));
      bool options_finalized = false, editor_finalized = false, control_finalized = false;
      const auto finalized = +[](gpointer data) { *static_cast<bool*> (data) = true; };
      g_object_set_data_full (G_OBJECT (options), "test-08008-finalized", &options_finalized, finalized);
      GError *error = nullptr;
      auto *widget = gimp_painter_mybrush_editor_new (GIMP_CONTEXT (options), FALSE, &error);
      g_assert_no_error (error); g_assert_nonnull (widget); g_object_ref_sink (widget);
      g_object_set_data_full (G_OBJECT (widget), "test-08008-finalized", &editor_finalized, finalized);
      auto *spin = control (widget, "opaque"); g_object_ref (spin);
      g_object_set_data_full (G_OBJECT (spin), "test-08008-finalized", &control_finalized, finalized);
      if (options_first)
        {
          g_object_unref (options);
          g_assert_false (options_finalized);
          gtk_spin_button_set_value (GTK_SPIN_BUTTON (spin), .321);
          double value = 0; g_object_get (options, "opaque", &value, nullptr);
          g_assert_cmpfloat_with_epsilon (value, .321, 1e-6);
        }
      gtk_widget_destroy (widget);
      g_object_run_dispose (G_OBJECT (widget));
      g_object_run_dispose (G_OBJECT (widget));
      g_object_unref (widget);
      g_assert_true (editor_finalized);
      g_assert_false (control_finalized);
      if (options_first)
        g_assert_true (options_finalized);
      else
        {
          g_assert_false (options_finalized);
          g_object_set (options, "opaque", .654, nullptr);
          gtk_spin_button_set_value (GTK_SPIN_BUTTON (spin), .111);
          double value = 0; g_object_get (options, "opaque", &value, nullptr);
          g_assert_cmpfloat_with_epsilon (value, .654, 1e-6);
          g_object_unref (options);
          g_assert_true (options_finalized);
        }
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (spin), .222);
      g_object_unref (spin);
      g_assert_true (control_finalized);
      gimp_test_run_mainloop_until_idle ();
    }
}
static void graph_gestures(){
  Editor editor(true);MyPaint::Resource r;r.set_curve(BRUSH_OPAQUE,INPUT_PRESSURE,{{0,0},{1,1}});editor.set(r);
  auto*graph=editor.get("painter-curve-graph");auto*parent=gtk_widget_get_parent(graph);while(parent&&!GTK_IS_NOTEBOOK(parent))parent=gtk_widget_get_parent(parent);g_assert_nonnull(parent);gtk_notebook_set_current_page(GTK_NOTEBOOK(parent),2);
  while(g_main_context_pending(nullptr))g_main_context_iteration(nullptr,FALSE);
  double w=gtk_widget_get_allocated_width(graph)-16,h=gtk_widget_get_allocated_height(graph)-16;g_assert_cmpfloat(w,>,0);g_assert_cmpfloat(h,>,0);
  GdkEvent*event=gdk_event_new(GDK_BUTTON_PRESS);event->button.button=1;event->button.x=8;event->button.y=8+h/2;gboolean handled=FALSE;g_signal_emit_by_name(graph,"button-press-event",event,&handled);g_assert_true(handled);gdk_event_free(event);
  event=gdk_event_new(GDK_MOTION_NOTIFY);event->motion.state=GDK_BUTTON1_MASK;event->motion.x=8+w/4;event->motion.y=8+h/4;g_signal_emit_by_name(graph,"motion-notify-event",event,&handled);gdk_event_free(event);
  event=gdk_event_new(GDK_BUTTON_RELEASE);event->button.button=1;g_signal_emit_by_name(graph,"button-release-event",event,&handled);gdk_event_free(event);
  auto points=editor.snapshot().curve(BRUSH_OPAQUE,INPUT_PRESSURE);g_assert_cmpfloat_with_epsilon(points[0].x,.25,1e-6);g_assert_cmpfloat_with_epsilon(points[0].y,.5,1e-6);g_assert_false(gtk_widget_has_grab(graph));
  g_assert_true(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(editor.get("painter-curve-enabled"))));
  const char*range_names[]={"painter-curve-x-min","painter-curve-x-max","painter-curve-y-min","painter-curve-y-max"};const double range_values[]={-2,2,-3,3};
  for(int n=0;n<4;++n)gtk_spin_button_set_value(GTK_SPIN_BUTTON(editor.get(range_names[n])),range_values[n]);
  gtk_button_clicked(GTK_BUTTON(editor.get("painter-curve-rescale")));points=editor.snapshot().curve(BRUSH_OPAQUE,INPUT_PRESSURE);
  g_assert_cmpfloat_with_epsilon(points[0].x,-1,1e-6);g_assert_cmpfloat_with_epsilon(points[0].y,1.5,1e-6);g_assert_cmpfloat_with_epsilon(points[1].x,2,1e-6);g_assert_cmpfloat_with_epsilon(points[1].y,3,1e-6);
  event=gdk_event_new(GDK_BUTTON_PRESS);event->button.button=3;event->button.x=8+w/4;event->button.y=8+h/4;g_signal_emit_by_name(graph,"button-press-event",event,&handled);gdk_event_free(event);g_assert_true(editor.snapshot().curve(BRUSH_OPAQUE,INPUT_PRESSURE).empty());
  g_assert_false(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(editor.get("painter-curve-enabled"))));g_assert_cmpfloat(gtk_spin_button_get_value(GTK_SPIN_BUTTON(editor.get("painter-curve-x"))),==,0);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(editor.get("painter-curve-enabled")),TRUE);g_assert_cmpuint(editor.snapshot().curve(BRUSH_OPAQUE,INPUT_PRESSURE).size(),==,2);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(editor.get("painter-curve-enabled")),FALSE);g_assert_true(editor.snapshot().curve(BRUSH_OPAQUE,INPUT_PRESSURE).empty());
}
static void replaced_resource_notifications(){
  Editor editor;auto*ctx=GIMP_CONTEXT(editor.model.get());
  auto*first=GIMP_PATTERN(gimp_pattern_new(ctx,"preview-watch-first"));auto*second=GIMP_PATTERN(gimp_pattern_new(ctx,"preview-watch-second"));
  gimp_context_set_pattern(ctx,first);MyPaint::Resource r;r.set_switch(BRUSH_USE_GIMP_TEXTURE,true);r.set_text(BRUSH_TEXTURE_NAME,nullptr);r.set_base_value(BRUSH_TEXTURE_GRAIN,.1);r.set_base_value(BRUSH_TEXTURE_CONTRAST,.8);editor.set(r);editor.preview();
  auto*mask=gimp_pattern_get_mask(first);std::memset(gimp_temp_buf_get_data(mask),30,gimp_temp_buf_get_data_size(mask));gimp_data_dirty(GIMP_DATA(first));g_assert_true(gimp_painter_mybrush_editor_preview_pending(editor.widget));editor.preview();
  gimp_context_set_pattern(ctx,second);editor.preview();gimp_data_dirty(GIMP_DATA(first));g_assert_false(gimp_painter_mybrush_editor_preview_pending(editor.widget));
  gtk_widget_destroy(editor.window);gimp_data_dirty(GIMP_DATA(second));g_object_unref(first);g_object_unref(second);
}
static void delete_failure_retains_draft(){
  Editor editor;editor.set(MyPaint::Resource());GError*error=nullptr;
  g_assert_true(gimp_painter_mybrush_editor_save(editor.widget,"Painter Delete Failure Test",&error));g_assert_no_error(error);
  auto selected=ObjectRef<GimpPainterMybrush>::retain(gimp_context_get_painter_mybrush(GIMP_CONTEXT(editor.model.get())));
  auto file=ObjectRef<GObject>::retain(G_OBJECT(gimp_data_get_file(GIMP_DATA(selected.get()))));String path(g_file_get_path(G_FILE(file.get())));std::string moved=std::string(path.get())+".temporary";auto backup=ObjectRef<GObject>::adopt(G_OBJECT(g_file_new_for_path(moved.c_str())));
  auto directory=ObjectRef<GObject>::adopt(G_OBJECT(g_file_new_for_path(writable_directory.c_str())));
  gimp_data_set_file(GIMP_DATA(selected.get()),G_FILE(directory.get()),TRUE,TRUE);
  g_object_set(editor.model.get(),"opaque",.476,nullptr);auto unsaved=editor.snapshot().encode();
  g_assert_false(gimp_painter_mybrush_editor_save(editor.widget,nullptr,&error));g_assert_nonnull(error);g_clear_error(&error);
  gboolean dirty=FALSE;g_object_get(editor.model.get(),"painter-dirty",&dirty,nullptr);g_assert_true(dirty);g_assert_true(editor.snapshot().encode()==unsaved);
  gimp_data_set_file(GIMP_DATA(selected.get()),G_FILE(file.get()),TRUE,TRUE);
  g_assert_true(g_file_move(G_FILE(file.get()),G_FILE(backup.get()),G_FILE_COPY_NONE,nullptr,nullptr,nullptr,&error));g_assert_no_error(error);
  g_object_set(editor.model.get(),"opaque",.476,nullptr);const auto draft=editor.snapshot().encode();
  g_assert_false(gimp_painter_mybrush_editor_delete(editor.widget,&error));g_assert_nonnull(error);g_clear_error(&error);
  g_assert_true(gimp_context_get_painter_mybrush(GIMP_CONTEXT(editor.model.get()))==selected.get());g_assert_true(editor.snapshot().encode()==draft);
  g_assert_true(gimp_container_have(gimp_data_factory_get_container(gimp->painter_mybrush_factory),GIMP_OBJECT(selected.get())));
  g_assert_true(g_file_move(G_FILE(backup.get()),G_FILE(file.get()),G_FILE_COPY_NONE,nullptr,nullptr,nullptr,&error));g_assert_no_error(error);
  g_assert_true(gimp_painter_mybrush_editor_delete(editor.widget,&error));g_assert_no_error(error);
}
static void visual_demo(){
  Editor editor(true);auto*c=gimp_data_factory_get_container(gimp->painter_mybrush_factory);
  gimp_context_set_painter_mybrush(GIMP_CONTEXT(editor.model.get()),GIMP_PAINTER_MYBRUSH(gimp_container_get_child_by_index(c,0)));
  gtk_window_set_title(GTK_WINDOW(editor.window),"Painter MyPaint Editor QA");editor.preview();
  auto*loop=g_main_loop_new(nullptr,FALSE);g_signal_connect(editor.window,"destroy",G_CALLBACK(+[](GtkWidget*,gpointer p){g_main_loop_quit(static_cast<GMainLoop*>(p));}),loop);g_main_loop_run(loop);g_main_loop_unref(loop);
}
static void registration(){
  auto*group=gimp_action_factory_get_group(global_action_factory,"dialogs",gimp);g_assert_nonnull(gimp_action_group_get_action(group,"dialogs-painter-mypaint-editor"));
  auto*entry=gimp_dialog_factory_find_entry(gimp_dialog_factory_get_singleton(),"gimp-painter-mypaint-editor");g_assert_nonnull(entry);
  auto*dialog=preferences_dialog_create(gimp);g_object_ref_sink(dialog);gtk_dialog_response(GTK_DIALOG(dialog),GTK_RESPONSE_CANCEL);gtk_widget_destroy(dialog);g_object_unref(dialog);
}
/* Original 08.017: current shared widget controllers use one common store.
 * This is slot/registration evidence, not equivalence for all 93 source duties.
 * Insert before main(); add painter/binding-store.hpp and registration below. */
static void common_store_widget_behaviors ()
{
  Editor first, second;
  auto first_owner = ObjectRef<GObject>::retain (G_OBJECT (first.widget));
  auto second_owner = ObjectRef<GObject>::retain (G_OBJECT (second.widget));
  auto *first_store = BindingStore::find (first_owner.get ());
  auto *second_store = BindingStore::find (second_owner.get ());
  g_assert_nonnull (first_store); g_assert_nonnull (second_store);
  g_assert_true (first_store != second_store);
  g_assert_true (first_store->state () == BindingStore::State::active);
  g_assert_true (second_store->state () == BindingStore::State::active);
  g_assert_true (first.model.get () == second.model.get ());
  const auto generation = first_store->generation ();
  const auto second_generation = second_store->generation ();
  auto first_spin = ObjectRef<GObject>::retain (G_OBJECT (first.get ("opaque")));
  auto first_curve = ObjectRef<GObject>::retain (G_OBJECT (first.get ("painter-curve-enabled")));
  auto second_spin = ObjectRef<GObject>::retain (G_OBJECT (second.get ("opaque")));

  gtk_spin_button_set_value (GTK_SPIN_BUTTON (first_spin.get ()), .237);
  g_assert_cmpfloat_with_epsilon (first.snapshot ().base_value (BRUSH_OPAQUE), .237, 1e-6);
  g_assert_cmpfloat_with_epsilon (gtk_spin_button_get_value (GTK_SPIN_BUTTON (second_spin.get ())), .237, 1e-6);
  GError *error = nullptr;
  g_assert_true (gimp_painter_mybrush_editor_set_context (first.widget, gimp->user_context, &error));
  g_assert_no_error (error);
  g_assert_true (BindingStore::find (first_owner.get ()) == first_store);
  g_assert_cmpuint (first_store->generation (), ==, generation);

  g_assert_true (gimp_painter_binding_close (first_owner.get (), &error));
  g_assert_no_error (error);
  g_assert_true (first_store->state () == BindingStore::State::closed);
  g_assert_false (first_store->accepts (generation));
  const auto closed_generation = first_store->generation ();
  g_assert_cmpuint (closed_generation, >, generation);
  const auto before = first.snapshot ().encode ();
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (first_spin.get ()), .873);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (first_curve.get ()),
                               !gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (first_curve.get ())));
  g_assert_true (first.snapshot ().encode () == before);
  g_assert_false (gimp_painter_mybrush_editor_preview_pending (first.widget));
  g_assert_null (gimp_painter_mybrush_editor_ref_preview (first.widget));

  gtk_spin_button_set_value (GTK_SPIN_BUTTON (second_spin.get ()), .419);
  g_assert_cmpfloat_with_epsilon (second.snapshot ().base_value (BRUSH_OPAQUE), .419, 1e-6);
  g_assert_cmpfloat_with_epsilon (gtk_spin_button_get_value (GTK_SPIN_BUTTON (first_spin.get ())), .873, 1e-6);
  g_assert_true (second_store->state () == BindingStore::State::active);
  g_assert_cmpuint (second_store->generation (), ==, second_generation);
  g_assert_false (gimp_painter_mybrush_editor_set_context (first.widget, gimp->user_context, &error));
  g_assert_error (error, GIMP_PAINTER_ERROR, GIMP_PAINTER_ERROR_CLOSED); g_clear_error (&error);
  g_assert_true (gimp_painter_binding_close (first_owner.get (), &error)); g_assert_no_error (error);
  g_assert_true (BindingStore::find (first_owner.get ()) == first_store);
  g_assert_cmpuint (first_store->generation (), ==, closed_generation);
}

/* WBS 08.017 source-specific preparation: insert before main() in the existing
 * test-painter-mypaint-editor.cpp. These tests use the actual registered dock
 * constructor, GtkTreeSelection callback, container add/remove notifications,
 * and production context/close APIs. No new production seam is required.
 * This does not establish the legacy PDB selector interface or feature parity.
 */
namespace {
struct BoundaryBrushes
{
  GimpContainer *container;
  PainterMybrushRef first = PainterMybrushRef::create ("08017 boundary first");
  PainterMybrushRef second = PainterMybrushRef::create ("08017 boundary second");
  PainterMybrushRef added = PainterMybrushRef::create ("08017 boundary added");
  ObjectRef<GimpPainterMybrushOptions> shared_options;
  ObjectRef<GimpPainterMybrush> saved_selection;
  gchar *saved_json = nullptr;
  gchar *saved_name = nullptr;

  BoundaryBrushes ()
    : container (gimp_data_factory_get_container (gimp->painter_mybrush_factory))
  {
    // Ordinary contexts intentionally share the real registered tool options.
    // Keep their previous selection/draft alive so fixture cleanup cannot
    // leave a transient removed resource selected in that persistent model.
    auto *info = GIMP_TOOL_INFO (gimp_container_get_child_by_name (
      gimp->tool_info_list, "gimp-painter-mypaint-tool"));
    g_assert_nonnull (info);
    g_assert_true (GIMP_IS_PAINTER_MYBRUSH_OPTIONS (info->tool_options));
    shared_options = ObjectRef<GimpPainterMybrushOptions>::retain (
      GIMP_PAINTER_MYBRUSH_OPTIONS (info->tool_options));
    saved_selection = ObjectRef<GimpPainterMybrush>::retain (
      gimp_context_get_painter_mybrush (GIMP_CONTEXT (shared_options.get ())));
    GError *error = nullptr;
    saved_json = gimp_painter_mybrush_options_dup_json (shared_options.get (), &error);
    g_assert_no_error (error); g_assert_nonnull (saved_json);
    saved_name = gimp_painter_mybrush_options_dup_draft_name (shared_options.get (), &error);
    g_assert_no_error (error); g_assert_nonnull (saved_name);
    auto a = first.snapshot (); a.set_base_value (BRUSH_OPAQUE, .217); first.replace (a);
    auto b = second.snapshot (); b.set_base_value (BRUSH_OPAQUE, .683); second.replace (b);
    // The real factory's tag cache requests an identifier on insertion. Give
    // these transient resources a native internal collection before any add;
    // their distinct names make each identifier unique without disk fixtures.
    for (auto *brush : {first.get (), second.get (), added.get ()})
      gimp_data_make_internal (GIMP_DATA (brush),
                               "gimp-painter-mypaint-editor-boundary-tests");
    g_assert_true (gimp_container_add (container, GIMP_OBJECT (first.get ())));
    g_assert_true (gimp_container_add (container, GIMP_OBJECT (second.get ())));
  }
  ~BoundaryBrushes ()
  {
    // Editors and temporary contexts are declared after this fixture and are
    // already closed here. Native selection history remains genuine history;
    // these tests neither clear it nor claim to reset the whole tool model.
    gimp_context_set_painter_mybrush (GIMP_CONTEXT (shared_options.get ()),
                                    saved_selection.get ());
    GError *error = nullptr;
    g_assert_true (gimp_painter_mybrush_options_set_json (shared_options.get (),
                                                       saved_json, &error));
    g_assert_no_error (error);
    g_assert_true (gimp_painter_mybrush_options_set_draft_name (
      shared_options.get (), saved_name, &error));
    g_assert_no_error (error);
    g_free (saved_json);
    g_free (saved_name);
    for (auto *brush : {added.get (), second.get (), first.get ()})
      if (gimp_container_have (container, GIMP_OBJECT (brush)))
        gimp_container_remove (container, GIMP_OBJECT (brush));
  }
};

bool boundary_find_resource (GtkTreeModel *model, GimpPainterMybrush *brush,
                             GtkTreeIter *result)
{
  // This is the canonical editor's published GTK list, with resource column 0.
  g_assert_cmpuint (gtk_tree_model_get_column_type (model, 0), ==,
                    GIMP_TYPE_PAINTER_MYBRUSH);
  GtkTreeIter iter;
  for (gboolean valid = gtk_tree_model_get_iter_first (model, &iter); valid;
       valid = gtk_tree_model_iter_next (model, &iter))
    {
      GimpPainterMybrush *row = nullptr;
      gtk_tree_model_get (model, &iter, 0, &row, -1);
      const bool matches = row == brush;
      if (row) g_object_unref (row);
      if (matches) { if (result) *result = iter; return true; }
    }
  return false;
}

void boundary_select_resource (GtkWidget *tree, GimpPainterMybrush *brush)
{
  GtkTreeIter iter;
  auto *model = gtk_tree_view_get_model (GTK_TREE_VIEW (tree));
  g_assert_true (boundary_find_resource (model, brush, &iter));
  // Use GTK's user-selection path, not the context setter under test.
  gtk_tree_selection_select_iter (gtk_tree_view_get_selection (GTK_TREE_VIEW (tree)), &iter);
}

void boundary_assert_selected (GtkWidget *tree, GimpPainterMybrush *brush)
{
  GtkTreeModel *model = nullptr; GtkTreeIter iter;
  g_assert_true (gtk_tree_selection_get_selected (
    gtk_tree_view_get_selection (GTK_TREE_VIEW (tree)), &model, &iter));
  GimpPainterMybrush *row = nullptr;
  gtk_tree_model_get (model, &iter, 0, &row, -1);
  g_assert_true (row == brush);
  g_object_unref (row);
}
}

static void registered_dock_selection_context_boundary ()
{
  BoundaryBrushes resources;
  auto first_context = ObjectRef<GObject>::adopt (
    G_OBJECT (gimp_context_new (gimp, "08017 first context", nullptr)));
  auto next_context = ObjectRef<GObject>::adopt (
    G_OBJECT (gimp_context_new (gimp, "08017 next context", nullptr)));
  auto *first = GIMP_CONTEXT (first_context.get ());
  auto *next = GIMP_CONTEXT (next_context.get ());
  gimp_context_set_painter_mybrush (first, resources.first.get ());
  gimp_context_set_painter_mybrush (next, resources.first.get ());

  auto *factory = gimp_dialog_factory_get_singleton ();
  auto *entry = gimp_dialog_factory_find_entry (factory, "gimp-painter-mypaint-editor");
  g_assert_nonnull (entry); g_assert_nonnull (entry->new_func);
  auto *widget = entry->new_func (factory, first, nullptr, entry->view_size);
  g_assert_true (GIMP_IS_PAINTER_MYBRUSH_EDITOR (widget));
  auto owner = ObjectRef<GObject>::sink (G_OBJECT (widget));
  auto *editor = GIMP_PAINTER_MYBRUSH_EDITOR (widget);
  auto *binding = BindingStore::find (owner.get ());
  g_assert_nonnull (binding);
  g_assert_true (binding->state () == BindingStore::State::active);
  const auto generation = binding->generation ();
  GError *error = nullptr;
  auto options = ObjectRef<GimpPainterMybrushOptions>::adopt (
    gimp_painter_mybrush_options_ref_for_context (first, &error));
  g_assert_no_error (error);
  Editor peer (true, GIMP_CONTEXT (options.get ()));

  boundary_select_resource (control (widget, "painter-brush-list"), resources.second.get ());
  g_assert_true (gimp_context_get_painter_mybrush (first) == resources.second.get ());
  g_assert_true (gimp_context_get_painter_mybrush (GIMP_CONTEXT (options.get ())) == resources.second.get ());
  boundary_assert_selected (peer.get ("painter-brush-list"), resources.second.get ());
  g_assert_cmpfloat_with_epsilon (gtk_spin_button_get_value (
    GTK_SPIN_BUTTON (peer.get ("opaque"))), .683, 1e-6);

  g_assert_true (gimp_painter_mybrush_editor_set_context (editor, next, &error));
  g_assert_no_error (error);
  g_assert_true (BindingStore::find (owner.get ()) == binding);
  g_assert_cmpuint (binding->generation (), ==, generation);
  boundary_assert_selected (control (widget, "painter-brush-list"), resources.first.get ());
  boundary_select_resource (control (widget, "painter-brush-list"), resources.second.get ());
  g_assert_true (gimp_context_get_painter_mybrush (next) == resources.second.get ());

  // Detached ordinary contexts must no longer retarget this shared Options.
  gimp_context_set_painter_mybrush (first, resources.first.get ());
  g_assert_true (gimp_context_get_painter_mybrush (GIMP_CONTEXT (options.get ())) == resources.second.get ());
  boundary_assert_selected (control (widget, "painter-brush-list"), resources.second.get ());
  gimp_context_set_painter_mybrush (next, resources.first.get ());
  boundary_assert_selected (control (widget, "painter-brush-list"), resources.first.get ());
  boundary_assert_selected (peer.get ("painter-brush-list"), resources.first.get ());
  g_assert_cmpfloat_with_epsilon (gtk_spin_button_get_value (
    GTK_SPIN_BUTTON (peer.get ("opaque"))), .217, 1e-6);

  gtk_widget_destroy (widget);
  gimp_context_set_painter_mybrush (next, resources.second.get ());
  g_assert_true (gimp_context_get_painter_mybrush (GIMP_CONTEXT (options.get ())) == resources.first.get ());
  g_assert_true (binding->state () == BindingStore::State::closed);
  g_assert_false (binding->accepts (generation));
}

static void factory_membership_context_lifetime_boundary ()
{
  BoundaryBrushes resources;
  auto context = ObjectRef<GObject>::adopt (
    G_OBJECT (gimp_context_new (gimp, "08017 membership context", nullptr)));
  auto next_context = ObjectRef<GObject>::adopt (
    G_OBJECT (gimp_context_new (gimp, "08017 membership replacement", nullptr)));
  gimp_context_set_painter_mybrush (GIMP_CONTEXT (context.get ()), resources.first.get ());
  gimp_context_set_painter_mybrush (GIMP_CONTEXT (next_context.get ()), resources.second.get ());
  Editor editor (false, GIMP_CONTEXT (context.get ()));
  auto old_model = ObjectRef<GObject>::retain (G_OBJECT (
    gtk_tree_view_get_model (GTK_TREE_VIEW (editor.get ("painter-brush-list")))));
  auto *old_list = GTK_TREE_MODEL (old_model.get ());
  const int initial = gtk_tree_model_iter_n_children (old_list, nullptr);
  g_assert_false (boundary_find_resource (old_list, resources.added.get (), nullptr));
  g_assert_true (gimp_container_add (resources.container, GIMP_OBJECT (resources.added.get ())));
  g_assert_true (boundary_find_resource (old_list, resources.added.get (), nullptr));
  g_assert_cmpint (gtk_tree_model_iter_n_children (old_list, nullptr), ==, initial + 1);
  g_assert_true (gimp_container_remove (resources.container, GIMP_OBJECT (resources.added.get ())));
  g_assert_false (boundary_find_resource (old_list, resources.added.get (), nullptr));
  g_assert_cmpint (gtk_tree_model_iter_n_children (old_list, nullptr), ==, initial);

  GError *error = nullptr;
  g_assert_true (gimp_painter_mybrush_editor_set_context (
    editor.widget, GIMP_CONTEXT (next_context.get ()), &error));
  g_assert_no_error (error);
  auto new_model = ObjectRef<GObject>::retain (G_OBJECT (
    gtk_tree_view_get_model (GTK_TREE_VIEW (editor.get ("painter-brush-list")))));
  auto *new_list = GTK_TREE_MODEL (new_model.get ());
  g_assert_true (old_list != new_list);
  const int old_after_detach = gtk_tree_model_iter_n_children (old_list, nullptr);
  g_assert_true (gimp_container_add (resources.container, GIMP_OBJECT (resources.added.get ())));
  g_assert_true (boundary_find_resource (new_list, resources.added.get (), nullptr));
  g_assert_false (boundary_find_resource (old_list, resources.added.get (), nullptr));
  g_assert_cmpint (gtk_tree_model_iter_n_children (old_list, nullptr), ==, old_after_detach);
  g_assert_true (gimp_container_remove (resources.container, GIMP_OBJECT (resources.added.get ())));
  g_assert_false (boundary_find_resource (new_list, resources.added.get (), nullptr));

  // A closed controller can still have retained native model storage. Factory
  // notifications must not rebuild either the detached or closed list.
  g_assert_true (gimp_painter_binding_close (G_OBJECT (editor.widget), &error));
  g_assert_no_error (error);
  const int closed_count = gtk_tree_model_iter_n_children (new_list, nullptr);
  g_assert_true (gimp_container_add (resources.container, GIMP_OBJECT (resources.added.get ())));
  g_assert_false (boundary_find_resource (new_list, resources.added.get (), nullptr));
  g_assert_cmpint (gtk_tree_model_iter_n_children (new_list, nullptr), ==, closed_count);
  g_assert_cmpint (gtk_tree_model_iter_n_children (old_list, nullptr), ==, old_after_detach);
  g_assert_true (gimp_container_remove (resources.container, GIMP_OBJECT (resources.added.get ())));
  g_assert_cmpint (gtk_tree_model_iter_n_children (new_list, nullptr), ==, closed_count);
  g_assert_false (gimp_painter_mybrush_editor_preview_pending (editor.widget));
}

static void draft_name_history_save_workflow ()
{
  /* Configure the isolated save location before creating test resources or
   * editors: path changes refresh the entire native factory synchronously. */
  GError *error = nullptr;
  String directory (g_dir_make_tmp ("painter-draft-name-XXXXXX", &error));
  g_assert_no_error (error); g_assert_nonnull (directory.get ());
  gchar *old_search = nullptr, *old_writable = nullptr;
  g_object_get (gimp->config, "painter-mypaint-brush-path", &old_search,
                "painter-mypaint-brush-path-writable", &old_writable, nullptr);
  String search (g_strconcat (directory.get (), G_SEARCHPATH_SEPARATOR_S, old_search, nullptr));
  g_object_set (gimp->config, "painter-mypaint-brush-path", search.get (),
                "painter-mypaint-brush-path-writable", directory.get (), nullptr);
  BoundaryBrushes resources;
  auto context = ObjectRef<GObject>::adopt (
    G_OBJECT (gimp_context_new (gimp, "Draft name workflow", nullptr)));
  gimp_context_set_painter_mybrush (GIMP_CONTEXT (context.get ()), resources.first.get ());
  Editor editor (false, GIMP_CONTEXT (context.get ()));
  Editor peer (true, GIMP_CONTEXT (editor.model.get ()));
  const std::string source_name = gimp_object_get_name (resources.first.get ());
  const auto source_json = resources.first.snapshot ().encode ();
  const auto second_json = resources.second.snapshot ().encode ();
  const int count = gimp_container_get_n_children (resources.container);
  const char *pending_name = "Painter Pending Name \316\261";
  auto *name = GTK_ENTRY (editor.get ("painter-brush-name"));
  g_assert_true (gtk_editable_get_editable (GTK_EDITABLE (name)));
  editor.preview (); peer.preview ();
  const auto preview_revision = gimp_painter_mybrush_editor_preview_revision (editor.widget);
  const auto peer_revision = gimp_painter_mybrush_editor_preview_revision (peer.widget);
  guint settings_changed = 0;
  auto handler = g_signal_connect (editor.model.get (), "settings-changed",
    G_CALLBACK (+[](GimpPainterMybrushOptions *, gpointer p) { ++*static_cast<guint *> (p); }),
    &settings_changed);
  gtk_entry_set_text (name, pending_name);
  g_assert_cmpuint (settings_changed, ==, 0);
  g_assert_false (gimp_painter_mybrush_editor_preview_pending (editor.widget));
  g_assert_false (gimp_painter_mybrush_editor_preview_pending (peer.widget));
  g_assert_cmpuint (gimp_painter_mybrush_editor_preview_revision (editor.widget), ==, preview_revision);
  g_assert_cmpuint (gimp_painter_mybrush_editor_preview_revision (peer.widget), ==, peer_revision);
  g_signal_handler_disconnect (editor.model.get (), handler);
  gboolean dirty = TRUE, name_dirty = FALSE;
  g_object_get (editor.model.get (), "painter-dirty", &dirty,
                "painter-name-dirty", &name_dirty, nullptr);
  g_assert_false (dirty); g_assert_true (name_dirty);
  g_assert_true (editor.snapshot ().encode () == source_json);
  g_assert_cmpstr (gtk_entry_get_text (GTK_ENTRY (peer.get ("painter-brush-name"))), ==, pending_name);

  /* Restoring only a name on this same source must not send an unchanged
   * conflict notification through the full settings/preview refresh path. */
  const guint name_history = gimp_painter_mybrush_options_history_size (editor.model.get ());
  boundary_select_resource (editor.get ("painter-brush-list"), resources.second.get ());
  boundary_select_resource (editor.get ("painter-brush-list"), resources.first.get ());
  editor.preview (); peer.preview ();
  const auto restore_revision = gimp_painter_mybrush_editor_preview_revision (editor.widget);
  const auto peer_restore_revision = gimp_painter_mybrush_editor_preview_revision (peer.widget);
  settings_changed = 0;
  handler = g_signal_connect (editor.model.get (), "settings-changed",
    G_CALLBACK (+[](GimpPainterMybrushOptions *, gpointer p) { ++*static_cast<guint *> (p); }),
    &settings_changed);
  gtk_combo_box_set_active (GTK_COMBO_BOX (editor.get ("painter-brush-history")), name_history);
  g_assert_cmpuint (settings_changed, ==, 0);
  g_signal_handler_disconnect (editor.model.get (), handler);
  g_assert_false (gimp_painter_mybrush_editor_preview_pending (editor.widget));
  g_assert_false (gimp_painter_mybrush_editor_preview_pending (peer.widget));
  g_assert_cmpuint (gimp_painter_mybrush_editor_preview_revision (editor.widget), ==, restore_revision);
  g_assert_cmpuint (gimp_painter_mybrush_editor_preview_revision (peer.widget), ==, peer_restore_revision);
  g_assert_cmpstr (gtk_entry_get_text (name), ==, pending_name);
  g_assert_cmpstr (gtk_entry_get_text (GTK_ENTRY (peer.get ("painter-brush-name"))), ==, pending_name);
  g_assert_true (editor.snapshot ().encode () == source_json);

  gtk_spin_button_set_value (GTK_SPIN_BUTTON (editor.get ("opaque")), .413);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (editor.get ("non-incremental")), TRUE);
  const auto draft = editor.snapshot ();
  const auto draft_json = draft.encode ();
  const guint history = gimp_painter_mybrush_options_history_size (editor.model.get ());
  boundary_select_resource (editor.get ("painter-brush-list"), resources.second.get ());
  g_assert_cmpstr (gtk_entry_get_text (name), ==, gimp_object_get_name (resources.second.get ()));
  g_assert_cmpuint (gimp_painter_mybrush_options_history_size (editor.model.get ()), ==, history + 1);
  gtk_combo_box_set_active (GTK_COMBO_BOX (editor.get ("painter-brush-history")), history);

  const auto assert_source_stable = [&] {
    g_assert_cmpstr (gimp_object_get_name (resources.first.get ()), ==, source_name.c_str ());
    g_assert_true (resources.first.snapshot ().encode () == source_json);
    g_assert_true (resources.second.snapshot ().encode () == second_json);
  };
  const auto assert_pending = [&] (const char *expected) {
    g_assert_true (gimp_context_get_painter_mybrush (GIMP_CONTEXT (editor.model.get ())) == resources.first.get ());
    boundary_assert_selected (peer.get ("painter-brush-list"), resources.first.get ());
    g_assert_cmpstr (gtk_entry_get_text (name), ==, expected);
    g_assert_cmpstr (gtk_entry_get_text (GTK_ENTRY (peer.get ("painter-brush-name"))), ==, expected);
    g_assert_true (editor.snapshot ().encode () == draft_json);
    g_assert_true (peer.snapshot ().encode () == draft_json);
    g_object_get (editor.model.get (), "painter-dirty", &dirty,
                  "painter-name-dirty", &name_dirty, nullptr);
    g_assert_true (dirty); g_assert_true (name_dirty);
    g_assert_cmpint (gimp_container_get_n_children (resources.container), ==, count);
    assert_source_stable ();
  };
  assert_pending (pending_name);

  /* Save As begins with the working name. Canceling edited dialog text must
   * not apply it to the shared draft or mutate either source resource. */
  gtk_button_clicked (GTK_BUTTON (editor.get ("painter-brush-save-as")));
  GtkDialog *dialog = nullptr;
  GList *windows = gtk_window_list_toplevels ();
  for (auto *item = windows; item; item = item->next)
    if (GTK_IS_DIALOG (item->data) &&
        gtk_window_get_transient_for (GTK_WINDOW (item->data)) == GTK_WINDOW (editor.window))
      { g_assert_null (dialog); dialog = GTK_DIALOG (item->data); }
  g_list_free (windows);
  g_assert_nonnull (dialog);
  GtkEntry *dialog_name = nullptr;
  GList *children = gtk_container_get_children (
    GTK_CONTAINER (gtk_dialog_get_content_area (dialog)));
  for (auto *item = children; item; item = item->next)
    if (GTK_IS_ENTRY (item->data)) dialog_name = GTK_ENTRY (item->data);
  g_list_free (children);
  g_assert_nonnull (dialog_name);
  g_assert_cmpstr (gtk_entry_get_text (dialog_name), ==, pending_name);
  gtk_entry_set_text (dialog_name, "Canceled dialog name");
  gtk_dialog_response (dialog, GTK_RESPONSE_CANCEL);
  assert_pending (pending_name);

  for (const char *rejected : {"", gimp_object_get_name (resources.second.get ())})
    {
      gtk_entry_set_text (name, rejected);
      g_assert_false (gimp_painter_mybrush_editor_save (editor.widget, nullptr, &error));
      g_assert_nonnull (error); g_clear_error (&error);
      assert_pending (rejected);
    }
  gtk_entry_set_text (name, pending_name);
  const char invalid_utf8[] = {char (0xff), 0};
  g_assert_false (gimp_painter_mybrush_editor_save (editor.widget, invalid_utf8, &error));
  g_assert_nonnull (error); g_clear_error (&error);
  assert_pending (pending_name);

  /* A regular file in place of our temporary writable directory makes the
   * named-copy save fail on every user ID, including root. No chmod oracle. */
  g_assert_cmpint (g_rmdir (directory.get ()), ==, 0);
  g_assert_true (g_file_set_contents (directory.get (), "not a directory", -1, &error));
  g_assert_no_error (error);
  g_assert_false (gimp_painter_mybrush_editor_save (editor.widget, nullptr, &error));
  g_assert_nonnull (error); g_clear_error (&error);
  assert_pending (pending_name);
  g_assert_cmpint (g_unlink (directory.get ()), ==, 0);
  g_assert_cmpint (g_mkdir (directory.get (), 0700), ==, 0);

  /* The actual Save button must use the restored pending name and create a
   * new persisted resource, even though the selected source is read-only. */
  g_assert_true (gtk_widget_get_sensitive (editor.get ("painter-brush-save")));
  gtk_button_clicked (GTK_BUTTON (editor.get ("painter-brush-save")));
  auto saved = ObjectRef<GimpPainterMybrush>::retain (
    gimp_context_get_painter_mybrush (GIMP_CONTEXT (editor.model.get ())));
  g_assert_true (saved.get () != resources.first.get ());
  g_assert_cmpstr (gimp_object_get_name (saved.get ()), ==, pending_name);
  g_assert_true (PainterMybrushRef::retain (saved.get ()).snapshot ().encode () == draft_json);
  g_assert_cmpint (gimp_container_get_n_children (resources.container), ==, count + 1);
  boundary_assert_selected (peer.get ("painter-brush-list"), saved.get ());
  assert_source_stable ();
  g_object_get (editor.model.get (), "painter-dirty", &dirty,
                "painter-name-dirty", &name_dirty, nullptr);
  g_assert_false (dirty); g_assert_false (name_dirty);
  auto *file = gimp_data_get_file (GIMP_DATA (saved.get ()));
  g_assert_nonnull (file);
  auto stream = ObjectRef<GObject>::adopt (G_OBJECT (g_file_read (file, nullptr, &error)));
  g_assert_no_error (error);
  auto expected_disk = draft; expected_disk.set_parent_brush_name (pending_name);
  g_assert_true (MyPaint::Resource::load (G_INPUT_STREAM (stream.get ())).encode () == expected_disk.encode ());

  /* Explicit Rename still acts on the selected persisted object. It must
   * not route through pending-name Save and accidentally create a third one. */
  const char *later_name = "Still unsaved after Rename";
  gtk_entry_set_text (name, later_name);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (editor.get ("opaque")), .625);
  const auto renamed_settings = editor.snapshot ().encode ();
  g_assert_true (gimp_painter_mybrush_editor_rename (
    editor.widget, "Painter Explicit Source Rename", &error));
  g_assert_no_error (error);
  g_assert_true (gimp_context_get_painter_mybrush (GIMP_CONTEXT (editor.model.get ())) == saved.get ());
  g_assert_cmpstr (gimp_object_get_name (saved.get ()), ==, "Painter Explicit Source Rename");
  g_assert_cmpstr (gtk_entry_get_text (name), ==, later_name);
  g_assert_cmpstr (gtk_entry_get_text (GTK_ENTRY (peer.get ("painter-brush-name"))), ==, later_name);
  g_assert_true (editor.snapshot ().encode () == renamed_settings);
  g_assert_true (PainterMybrushRef::retain (saved.get ()).snapshot ().encode () == renamed_settings);
  g_assert_cmpint (gimp_container_get_n_children (resources.container), ==, count + 1);
  assert_source_stable ();

  /* The initial selected-settings save succeeds. Only the subsequent save
   * of the tentative resource name fails, exercising actual Rename rollback.
   * In the second pass a callback supplies a still newer pending name. */
  auto rename_file = ObjectRef<GObject>::retain (G_OBJECT (gimp_data_get_file (GIMP_DATA (saved.get ()))));
  auto blocked_file = ObjectRef<GObject>::adopt (G_OBJECT (g_file_new_for_path (directory.get ())));
  for (bool newer_name : {false, true})
    {
      const char *requested = "Painter Failed Rename";
      const char *newer = "Newer draft during failed Rename";
      gtk_entry_set_text (name, requested);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (editor.get ("opaque")), newer_name ? .657 : .656);
      const auto retained_settings = editor.snapshot ().encode ();
      struct RenameFailure {
        GimpPainterMybrushOptions *options; GFile *blocked;
        const char *requested; const char *newer; bool fired = false;
      } failure {editor.model.get (), G_FILE (blocked_file.get ()), requested,
                 newer_name ? newer : nullptr};
      handler = g_signal_connect (saved.get (), "name-changed",
        G_CALLBACK (+[](GimpObject *resource, gpointer data) {
          auto &state = *static_cast<RenameFailure *> (data);
          if (state.fired || g_strcmp0 (gimp_object_get_name (resource), state.requested)) return;
          state.fired = true;
          gimp_data_set_file (GIMP_DATA (resource), state.blocked, TRUE, TRUE);
          if (state.newer)
            {
              GError *error = nullptr;
              g_assert_true (gimp_painter_mybrush_options_set_draft_name (state.options, state.newer, &error));
              g_assert_no_error (error);
            }
        }), &failure);
      g_assert_false (gimp_painter_mybrush_editor_rename (editor.widget, requested, &error));
      g_assert_nonnull (error); g_clear_error (&error);
      g_signal_handler_disconnect (saved.get (), handler);
      g_assert_true (failure.fired);
      gimp_data_set_file (GIMP_DATA (saved.get ()), G_FILE (rename_file.get ()), TRUE, TRUE);
      g_assert_true (gimp_context_get_painter_mybrush (GIMP_CONTEXT (editor.model.get ())) == saved.get ());
      g_assert_cmpstr (gimp_object_get_name (saved.get ()), ==, "Painter Explicit Source Rename");
      g_assert_cmpstr (gtk_entry_get_text (name), ==, newer_name ? newer : requested);
      g_assert_cmpstr (gtk_entry_get_text (GTK_ENTRY (peer.get ("painter-brush-name"))), ==,
                       newer_name ? newer : requested);
      g_object_get (editor.model.get (), "painter-name-dirty", &name_dirty, nullptr);
      g_assert_true (name_dirty);
      g_assert_true (editor.snapshot ().encode () == retained_settings);
      g_assert_true (PainterMybrushRef::retain (saved.get ()).snapshot ().encode () == retained_settings);
      auto retained_stream = ObjectRef<GObject>::adopt (G_OBJECT (g_file_read (
        G_FILE (rename_file.get ()), nullptr, &error)));
      g_assert_no_error (error);
      auto retained_disk = editor.snapshot ();
      retained_disk.set_parent_brush_name ("Painter Explicit Source Rename");
      g_assert_true (MyPaint::Resource::load (G_INPUT_STREAM (retained_stream.get ())).encode () == retained_disk.encode ());
      g_assert_cmpint (gimp_container_get_n_children (resources.container), ==, count + 1);
      assert_source_stable ();
    }
  g_assert_true (gimp_painter_mybrush_editor_delete (editor.widget, &error));
  g_assert_no_error (error);
  g_assert_cmpint (gimp_container_get_n_children (resources.container), ==, count);

  /* Factory add is a real callback boundary. A newer active draft created
   * there must survive completion of a save of the older snapshot. */
  boundary_select_resource (editor.get ("painter-brush-list"), resources.first.get ());
  gtk_entry_set_text (name, "Painter Reentry Saved Snapshot");
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (editor.get ("opaque")), .714);
  const auto reentry_snapshot = editor.snapshot ();
  struct SaveReentry { GimpPainterMybrushOptions *options; bool fired = false; };
  SaveReentry reentry {editor.model.get ()};
  handler = g_signal_connect (resources.container, "add",
    G_CALLBACK (+[](GimpContainer *, GimpObject *added, gpointer data) {
      auto &state = *static_cast<SaveReentry *> (data);
      g_assert_false (state.fired); state.fired = true;
      g_assert_cmpstr (gimp_object_get_name (added), ==, "Painter Reentry Saved Snapshot");
      GError *failure = nullptr;
      g_assert_true (gimp_painter_mybrush_options_set_draft_name (
        state.options, "Newer pending draft from factory callback", &failure));
      g_assert_no_error (failure);
      g_object_set (state.options, "opaque", .842, nullptr);
    }), &reentry);
  g_assert_true (gimp_painter_mybrush_editor_save (editor.widget, nullptr, &error));
  g_assert_no_error (error);
  g_signal_handler_disconnect (resources.container, handler);
  g_assert_true (reentry.fired);
  g_assert_true (gimp_context_get_painter_mybrush (GIMP_CONTEXT (editor.model.get ())) == resources.first.get ());
  g_assert_true (gimp_context_get_painter_mybrush (GIMP_CONTEXT (context.get ())) == resources.first.get ());
  g_assert_cmpstr (gtk_entry_get_text (name), ==, "Newer pending draft from factory callback");
  g_assert_cmpstr (gtk_entry_get_text (GTK_ENTRY (peer.get ("painter-brush-name"))), ==,
                   "Newer pending draft from factory callback");
  auto newer_draft = reentry_snapshot; newer_draft.set_base_value (BRUSH_OPAQUE, .842);
  g_assert_true (editor.snapshot ().encode () == newer_draft.encode ());
  g_assert_cmpint (gimp_container_get_n_children (resources.container), ==, count + 1);
  auto *saved_snapshot = GIMP_PAINTER_MYBRUSH (gimp_container_get_child_by_name (
    resources.container, "Painter Reentry Saved Snapshot"));
  g_assert_nonnull (saved_snapshot);
  g_assert_true (PainterMybrushRef::retain (saved_snapshot).snapshot ().encode () == reentry_snapshot.encode ());
  auto reentry_stream = ObjectRef<GObject>::adopt (G_OBJECT (g_file_read (
    gimp_data_get_file (GIMP_DATA (saved_snapshot)), nullptr, &error)));
  g_assert_no_error (error);
  auto reentry_disk = reentry_snapshot;
  reentry_disk.set_parent_brush_name ("Painter Reentry Saved Snapshot");
  g_assert_true (MyPaint::Resource::load (G_INPUT_STREAM (reentry_stream.get ())).encode () == reentry_disk.encode ());
  assert_source_stable ();
  boundary_select_resource (editor.get ("painter-brush-list"), saved_snapshot);
  g_assert_true (gimp_painter_mybrush_editor_delete (editor.widget, &error));
  g_assert_no_error (error);
  g_assert_cmpint (gimp_container_get_n_children (resources.container), ==, count);
  for (auto *view : {&peer, &editor})
    {
      gtk_widget_destroy (view->window);
      g_object_unref (view->window);
      view->window = nullptr;
    }
  g_object_set (gimp->config, "painter-mypaint-brush-path", old_search,
                "painter-mypaint-brush-path-writable", old_writable, nullptr);
  g_free (old_search); g_free (old_writable);
  g_assert_cmpint (g_rmdir (directory.get ()), ==, 0);
  /* Restoring preferences queues normal RC autosave. Complete it while the
   * user context is alive; gimp_exit() clears that context before disposal. */
  gimp_test_run_mainloop_until_idle ();
  g_assert_cmpuint (GIMP_RC (gimp->config)->save_idle_id, ==, 0);
  g_assert_cmpuint (GIMP_RC (gimp->edit_config)->save_idle_id, ==, 0);
}

int main(int argc,char**argv){
  g_test_init(&argc,&argv,nullptr);if(!gtk_init_check(&argc,&argv))return GIMP_EXIT_TEST_SKIPPED;
  gimp_test_utils_setup_menus_path();gimp=gimp_init_for_gui_testing(TRUE);
  add_editor_case("/painter-editor/01-constructors-shared",constructors_and_shared_model);
  add_editor_case("/painter-editor/14-common-store-widget-behaviors",common_store_widget_behaviors);
  add_editor_case("/painter-editor/02-all-controls-curves",all_controls_and_curves);
  add_editor_case("/painter-editor/03-list-search-history",list_search_history);
  add_editor_case("/painter-editor/04-save-conflict-default-path",save_conflict_default_path);
  add_editor_case("/painter-editor/05-preview-extended",preview_deterministic_extended);
  add_editor_case("/painter-editor/06-replace-close",replacement_and_close);
  add_editor_case("/painter-editor/07-registration",registration);
  add_editor_case("/painter-editor/08-legacy-preview-oracle",legacy_preview_oracle);
  add_editor_case("/painter-editor/09-close-during-refresh",close_during_refresh);
  add_editor_case("/painter-editor/13-owner-destruction-orders",owner_destruction_orders);
  add_editor_case("/painter-editor/10-graph-gestures",graph_gestures);
  add_editor_case("/painter-editor/11-resource-notifications",replaced_resource_notifications);
  add_editor_case("/painter-editor/12-delete-failure",delete_failure_retains_draft);
  add_editor_case("/painter-editor/15-registered-dock-selection-context",registered_dock_selection_context_boundary);
  add_editor_case("/painter-editor/16-factory-membership-context-lifetime",factory_membership_context_lifetime_boundary);
  add_editor_case("/painter-editor/17-draft-name-history-save",draft_name_history_save_workflow);
  if(g_getenv("PAINTER_EDITOR_DEMO"))add_editor_case("/painter-editor/visual-demo",visual_demo);
  g_application_run(gimp->app,0,nullptr);int result=gimp_core_app_get_exit_status(GIMP_CORE_APP(gimp->app));g_application_quit(gimp->app);g_clear_object(&gimp->app);g_free(original_search);return result;
}
