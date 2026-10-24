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

extern "C" {
#include "widgets/gimpcontainerentry.h"
#include "widgets/gimpcontainerview.h"
}
#include "paint/painter-mypaint-surface/gimp-resources.hpp"
#include "paint/painter-mypaint-surface/gegl-surface.hpp"
namespace GimpPainter { template<> struct TypeTraits<GimpBrush>{static GType type(){return GIMP_TYPE_BRUSH;}}; template<> struct TypeTraits<GimpPattern>{static GType type(){return GIMP_TYPE_PATTERN;}}; template<> struct TypeTraits<GeglBuffer>{static GType type(){return GEGL_TYPE_BUFFER;}}; }
static GtkWidget*selector(GtkWidget*w,GimpContainer*c){
 if(GIMP_IS_CONTAINER_ENTRY(w)&&gimp_container_view_get_container(GIMP_CONTAINER_VIEW(w))==c)return w;
 if(GTK_IS_CONTAINER(w)){auto*children=gtk_container_get_children(GTK_CONTAINER(w));GtkWidget*out=nullptr;for(auto*l=children;l&&!out;l=l->next)out=selector(GTK_WIDGET(l->data),c);g_list_free(children);return out;}return nullptr;
}
static GimpBrush*make_mask(const char*name,int point){
 auto*b=GIMP_BRUSH(g_object_new(GIMP_TYPE_BRUSH,"name",name,nullptr));b->priv->mask=gimp_temp_buf_new(3,3,babl_format("Y u8"));auto*p=gimp_temp_buf_get_data(b->priv->mask);std::memset(p,0,9);p[point]=255;gimp_data_make_internal(GIMP_DATA(b),name);return b;
}
static GimpPattern*make_paper(const char*name,guchar value){
 auto*p=GIMP_PATTERN(g_object_new(GIMP_TYPE_PATTERN,"name",name,nullptr));p->mask=gimp_temp_buf_new(3,3,babl_format("R'G'B' u8"));std::memset(gimp_temp_buf_get_data(p->mask),value,27);gimp_data_make_internal(GIMP_DATA(p),name);return p;
}
static void surface_oracle(Editor&e,GimpBrush*brush,GimpPattern*paper,int mask_point,bool covered){
 using namespace GimpPainter::MyPaint;
 auto*ctx=GIMP_CONTEXT(e.model.get());g_assert_true(gimp_context_get_brush(ctx)==brush);g_assert_true(gimp_context_get_pattern(ctx)==paper);
 const auto resource=e.snapshot();g_assert_true(resource.text_value(BRUSH_BRUSHMARK_NAME)==gimp_object_get_name(brush));g_assert_true(resource.text_value(BRUSH_TEXTURE_NAME)==gimp_object_get_name(paper));
 const float grain=resource.base_value(BRUSH_TEXTURE_GRAIN),contrast=resource.base_value(BRUSH_TEXTURE_CONTRAST);
 // Identity transform: radius*2/max(3,3)==1, aspect1, angle0. At(4,4)3x3 top-left is(2,2).
 const int px=2+mask_point%3,py=2+mask_point/3,index=(py*16+px)*4;
 auto buffer=ObjectRef<GeglBuffer>::adopt(gegl_buffer_new(GEGL_RECTANGLE(0,0,16,16),babl_format("R'G'B'A u8")));
 std::vector<guchar>blank(16*16*4,0),expected=blank,actual=blank;
 gegl_buffer_set(buffer.get(),GEGL_RECTANGLE(0,0,16,16),0,babl_format("R'G'B'A u8"),blank.data(),GEGL_AUTO_ROWSTRIDE);
 {
  GimpResources resolved(ctx,resource,GimpResources::Purpose::Stroke);const auto&resolution=resolved.resolution();
  g_assert_true(resolution.effective_brush==gimp_object_get_name(brush));g_assert_true(resolution.effective_texture==gimp_object_get_name(paper));
  GeglSurface surface(buffer.get());resolved.attach(surface);surface.begin_session();surface.draw_dab(4,4,1.5,1,0,0,1,1,1,1,0,0,0,grain,contrast);surface.end_session();
 }
 if(covered){expected[index]=255;expected[index+3]=255;}
 gegl_buffer_get(buffer.get(),GEGL_RECTANGLE(0,0,16,16),1,babl_format("R'G'B'A u8"),actual.data(),GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);g_assert_true(actual==expected);
 // Independent target has red and blue at the two mask positions; all other pixels are opaque green.
 std::vector<guchar>source(16*16*4,0);for(unsigned n=0;n<source.size();n+=4){source[n+1]=255;source[n+3]=255;}
 const int left=(3*16+2)*4,right=(3*16+4)*4;source[left]=255;source[left+1]=0;source[right+1]=0;source[right+2]=255;
 gegl_buffer_set(buffer.get(),GEGL_RECTANGLE(0,0,16,16),0,babl_format("R'G'B'A u8"),source.data(),GEGL_AUTO_ROWSTRIDE);
 float r=-1,g=-1,b=-1,a=-1;
 {GimpResources resolved(ctx,resource,GimpResources::Purpose::Stroke);GeglSurface surface(buffer.get());resolved.attach(surface);surface.get_color(4,4,1.5,&r,&g,&b,&a,1,1,0,grain,contrast);}
 g_assert_cmpfloat(a,==,covered?1:0);g_assert_cmpfloat(r,==,covered&&mask_point==3?1:0);g_assert_cmpfloat(g,==,covered?0:1);g_assert_cmpfloat(b,==,covered&&mask_point==5?1:0);
 g_test_message("selector %s / %s grain=%g contrast=%g: exact1024 stamp bytes and4 sample components PASS",gimp_object_get_name(brush),gimp_object_get_name(paper),grain,contrast);
}
static void selector_surface(){
 auto*brushes=gimp_data_factory_get_container(gimp->brush_factory);auto*papers=gimp_data_factory_get_container(gimp->pattern_factory);const auto nb=gimp_container_get_n_children(brushes),np=gimp_container_get_n_children(papers);
 auto a=ObjectRef<GimpBrush>::adopt(make_mask("Selector shape A 日本語",3));auto b=ObjectRef<GimpBrush>::adopt(make_mask("Selector shape B",5));auto white=ObjectRef<GimpPattern>::adopt(make_paper("Selector white paper",255));auto black=ObjectRef<GimpPattern>::adopt(make_paper("Selector black paper 日本語",0));
 for(auto*x:{a.get(),b.get()})gimp_container_add(brushes,GIMP_OBJECT(x));for(auto*x:{white.get(),black.get()})gimp_container_add(papers,GIMP_OBJECT(x));
 {
  auto options=ObjectRef<GimpPainterMybrushOptions>::adopt(GIMP_PAINTER_MYBRUSH_OPTIONS(g_object_new(GIMP_TYPE_PAINTER_MYBRUSH_OPTIONS,"gimp",gimp,nullptr)));Editor first(true,GIMP_CONTEXT(options.get())),peer(false,GIMP_CONTEXT(options.get()));g_assert_true(first.model.get()==peer.model.get());
  auto expected=MyPaint::Resource::decode("{\"version\":3,\"unknown_selector\":{\"nested\":[null,9007199254740993,\"日本語\"]},\"settings\":{},\"texts\":{\"future_selector\":\"retained\"}}");expected.set_switch(BRUSH_USE_GIMP_BRUSHMARK,true);expected.set_switch(BRUSH_BRUSHMARK_SPECIFIED,true);expected.set_switch(BRUSH_USE_GIMP_TEXTURE,true);expected.set_switch(BRUSH_TEXTURE_SPECIFIED,true);expected.set_text(BRUSH_BRUSHMARK_NAME,gimp_object_get_name(b.get()));expected.set_text(BRUSH_TEXTURE_NAME,gimp_object_get_name(white.get()));expected.set_base_value(BRUSH_TEXTURE_GRAIN,0);expected.set_base_value(BRUSH_TEXTURE_CONTRAST,1);expected.set_curve(BRUSH_TEXTURE_GRAIN,INPUT_PRESSURE,{{0,.125},{1,.875}});first.set(expected);
  auto*shape=selector(GTK_WIDGET(first.widget),brushes);auto*paper=selector(GTK_WIDGET(first.widget),papers);auto*other_shape=selector(GTK_WIDGET(peer.widget),brushes);auto*other_paper=selector(GTK_WIDGET(peer.widget),papers);g_assert_nonnull(shape);g_assert_nonnull(paper);g_assert_nonnull(other_shape);g_assert_nonnull(other_paper);
  auto check=[&]{g_assert_true(first.snapshot().encode()==expected.encode());g_assert_true(peer.snapshot().encode()==expected.encode());g_assert_cmpstr(gtk_entry_get_text(GTK_ENTRY(other_shape)),==,expected.text_value(BRUSH_BRUSHMARK_NAME).c_str());g_assert_cmpstr(gtk_entry_get_text(GTK_ENTRY(other_paper)),==,expected.text_value(BRUSH_TEXTURE_NAME).c_str());};
  check();
  gtk_entry_set_text(GTK_ENTRY(shape),gimp_object_get_name(a.get()));expected.set_text(BRUSH_BRUSHMARK_NAME,gimp_object_get_name(a.get()));check();surface_oracle(first,a.get(),white.get(),3,true);
  gtk_entry_set_text(GTK_ENTRY(shape),gimp_object_get_name(b.get()));expected.set_text(BRUSH_BRUSHMARK_NAME,gimp_object_get_name(b.get()));check();surface_oracle(first,b.get(),white.get(),5,true);
  gtk_entry_set_text(GTK_ENTRY(paper),gimp_object_get_name(black.get()));expected.set_text(BRUSH_TEXTURE_NAME,gimp_object_get_name(black.get()));check();surface_oracle(first,b.get(),black.get(),5,false);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(first.get("texture-grain")),1);expected.set_base_value(BRUSH_TEXTURE_GRAIN,1);check();surface_oracle(first,b.get(),black.get(),5,true);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(first.get("texture-contrast")),0);expected.set_base_value(BRUSH_TEXTURE_CONTRAST,0);check();surface_oracle(first,b.get(),black.get(),5,false);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(first.get("texture-grain")),0);expected.set_base_value(BRUSH_TEXTURE_GRAIN,0);gtk_spin_button_set_value(GTK_SPIN_BUTTON(first.get("texture-contrast")),1);expected.set_base_value(BRUSH_TEXTURE_CONTRAST,1);gtk_entry_set_text(GTK_ENTRY(other_paper),gimp_object_get_name(white.get()));expected.set_text(BRUSH_TEXTURE_NAME,gimp_object_get_name(white.get()));check();surface_oracle(first,b.get(),white.get(),5,true);
 }
 for(auto*x:{a.get(),b.get()})gimp_container_remove(brushes,GIMP_OBJECT(x));for(auto*x:{white.get(),black.get()})gimp_container_remove(papers,GIMP_OBJECT(x));g_assert_cmpint(gimp_container_get_n_children(brushes),==,nb);g_assert_cmpint(gimp_container_get_n_children(papers),==,np);
}
int main(int argc,char**argv){g_test_init(&argc,&argv,nullptr);if(!gtk_init_check(&argc,&argv))return GIMP_EXIT_TEST_SKIPPED;gimp_test_utils_setup_menus_path();gimp=gimp_init_for_gui_testing(TRUE);add_editor_case("/painter-selector/stroke-surface",selector_surface);g_application_run(gimp->app,0,nullptr);int result=gimp_core_app_get_exit_status(GIMP_CORE_APP(gimp->app));g_application_quit(gimp->app);g_clear_object(&gimp->app);return result;}
