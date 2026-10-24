/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gegl.h>
#include <gtk/gtk.h>
extern "C" {
#include "libgimpbase/gimpbase.h"
#include "libgimpwidgets/gimpwidgets.h"
#include "core/core-types.h"
#include "widgets/widgets-types.h"
#include "display/display-types.h"
#include "core/gimp.h"
#include "core/gimpcontext.h"
#include "core/gimpcontainer.h"
#include "core/gimpdatafactory.h"
#include "core/gimpdata.h"
#include "core/gimpimage.h"
#include "core/gimpimage-undo.h"
#include "core/gimpfilterlayer.h"
#include "core/gimplayer.h"
#include "core/gimpdrawable.h"
#include "core/gimpundostack.h"
#include "config/gimpcoreconfig.h"
#include "display/gimpdisplay.h"
#include "display/gimpdisplayshell.h"
#include "widgets/gimpdialogfactory.h"
#include "display/gimppaintercanvasui.h"
#include "widgets/gimppainterlayertiles.h"
#include "widgets/gimplayertreeview.h"
#include "widgets/gimpcontainertreeview.h"
#include "widgets/gimpcontainertreestore.h"
#include "widgets/gimpviewrenderer.h"
#include "widgets/gimpcellrendererpopup.h"
#include "dialogs/painter-layer-dialog.h"
#include "gimp-app-test-utils.h"
#include "tests.h"
void gimp_test_filter_layer_popup_register (Gimp *application);
}
namespace {
Gimp *app;
void drain () {
  // The generic helper quits at DEFAULT_IDLE (200), before the tiles' LOW
  // (300) model rebuild. Drain that rebuild before routing a real row event.
  auto until=g_get_monotonic_time()+2*G_TIME_SPAN_SECOND;
  while(g_main_context_pending(nullptr)&&g_get_monotonic_time()<until)
    g_main_context_iteration(nullptr,FALSE);
}
GtkWidget *find (GtkWidget *w, const char *name, GType type = 0) {
  if ((!name || !g_strcmp0 (gtk_widget_get_name (w), name)) && (!type || G_TYPE_CHECK_INSTANCE_TYPE (w,type))) return w;
  if (!GTK_IS_CONTAINER (w)) return nullptr;
  auto *children=gtk_container_get_children(GTK_CONTAINER(w)); GtkWidget *result=nullptr;
  for(auto*l=children;l&&!result;l=l->next)result=find(GTK_WIDGET(l->data),name,type);
  g_list_free(children);return result;
}
GtkWidget *field(GtkWidget*w,const char*name){auto*r=find(w,name);g_assert_nonnull(r);return r;}
struct Fixture {
  GimpDisplay *display; GimpImage *image; GimpDisplayShell *shell; GimpFilterLayer *filter; GtkWidget *popup=nullptr;
  Fixture () {
    gimp_set_focused_once(app);g_assert_true(app->config->painter_canvas_ui);
    static bool resources=false;
    if(!resources){
      auto*shipped=g_build_filename(g_getenv("GIMP_TESTING_ABS_TOP_SRCDIR"),"gimp-data","brushes",nullptr);
      auto*writable=g_build_filename(gimp_directory(),"brushes",nullptr);g_assert_cmpint(g_mkdir_with_parents(writable,0700),==,0);
      auto*search=g_build_path(G_SEARCHPATH_SEPARATOR_S,shipped,writable,nullptr);
      g_object_set(app->config,"brush-path",search,"brush-path-writable",writable,nullptr);g_free(search);g_free(shipped);g_free(writable);
      gimp_data_factory_data_refresh(app->brush_factory,app->user_context);
      auto*c=gimp_data_factory_get_container(app->brush_factory);guint count=0;
      for(int n=0;n<gimp_container_get_n_children(c);++n)if(gimp_data_get_file(GIMP_DATA(gimp_container_get_child_by_index(c,n))))++count;
      g_assert_cmpuint(count,>,0);g_test_message("Shipped native brushes: %u",count);resources=true;
    }
    gimp_test_utils_create_image(app,320,240);drain();
    display=gimp_context_get_display(app->user_context);g_assert_nonnull(display);shell=gimp_display_get_shell(display);image=gimp_display_get_image(display);
    auto*layers=gimp_image_get_selected_layers(image);g_assert_nonnull(layers);
    auto*source=GIMP_DRAWABLE(layers->data);auto*color=gegl_color_new("rgba(0.25,0.5,0.75,1.0)");
    gegl_buffer_set_color(gimp_drawable_get_buffer(source),nullptr,color);g_object_unref(color);gimp_drawable_update(source,0,0,-1,-1);gimp_image_flush(image);
    g_assert_true(gimp_painter_canvas_ui_get_visible(shell));
    if(g_getenv("GIMP_TEST_FILTER_POPUP_SCREENSHOT")||g_getenv("GIMP_TEST_FILTER_POPUP_AFTER_APPLY_SCREENSHOT")){
      auto*w=gtk_widget_get_toplevel(GTK_WIDGET(shell));gtk_window_resize(GTK_WINDOW(w),1040,1800);gtk_window_move(GTK_WINDOW(w),0,0);
      auto until=g_get_monotonic_time()+300*G_TIME_SPAN_MILLISECOND;do{drain();g_usleep(1000);}while(g_get_monotonic_time()<until);
    }
    filter=GIMP_FILTER_LAYER(gimp_filter_layer_new(image,320,240,"Popup Colorify",1,GIMP_LAYER_MODE_NORMAL_LEGACY));
    auto*tint=g_variant_ref_sink(g_variant_new("(sudddd)","gimp-painter-colorify-rgba",1u,1.,1.,1.,1.));
    auto*args=gimp_value_array_new_from_types(nullptr,G_TYPE_INT,-123,G_TYPE_INT,456,G_TYPE_INT,-789,G_TYPE_VARIANT,tint,G_TYPE_NONE);
    auto*raw=g_bytes_new_static("popup-colorify",14);
    g_assert_true(gimp_filter_layer_set_definition(filter,"plug-in-colorify",raw,args,nullptr));
    g_bytes_unref(raw);gimp_value_array_unref(args);g_variant_unref(tint);
    g_assert_true(gimp_image_add_layer(image,GIMP_LAYER(filter),nullptr,0,FALSE));g_object_ref(filter);
    auto*selected=g_list_prepend(nullptr,filter);gimp_image_set_selected_layers(image,selected);g_list_free(selected);drain();
  }
  GtkWidget *tile (bool pointer=false) {
    auto*tiles=find(shell->canvas,nullptr,GIMP_TYPE_PAINTER_LAYER_TILES);g_assert_nonnull(tiles);
    auto signal=g_signal_lookup("map",GTK_TYPE_WIDGET);
    auto hook=g_signal_add_emission_hook(signal,0,+[](GSignalInvocationHint*,guint,const GValue*values,gpointer data)->gboolean{
      auto*w=GTK_WIDGET(g_value_get_object(&values[0]));
      if(!g_strcmp0(gtk_widget_get_name(w),"painter-layer-properties-popup")) {
        auto**slot=static_cast<GtkWidget**>(data);if(!*slot)*slot=GTK_WIDGET(g_object_ref(w));
      }return TRUE;
    },&popup,nullptr);
    g_assert_cmpuint(hook,>,0);
    auto*trigger=field(tiles,"layer-popup");g_assert_true(gtk_widget_get_mapped(trigger));g_assert_true(gtk_widget_is_sensitive(trigger));
    g_assert_cmpint(gtk_widget_get_allocated_width(trigger),>,0);g_assert_cmpint(gtk_widget_get_allocated_height(trigger),>,0);
    if(pointer){
      auto*canvas=field(tiles,"painter-layer-canvas");g_assert_true(gtk_widget_get_mapped(canvas));auto*window=gtk_widget_get_window(canvas);g_assert_nonnull(window);
      auto until=g_get_monotonic_time()+2*G_TIME_SPAN_SECOND;while(!gdk_window_is_viewable(window)&&g_get_monotonic_time()<until){drain();g_usleep(1000);}g_assert_true(gdk_window_is_viewable(window));
      auto*event=gdk_event_new(GDK_BUTTON_PRESS);event->button.window=GDK_WINDOW(g_object_ref(window));event->button.x=32;event->button.y=32;event->button.button=3;event->button.time=GDK_CURRENT_TIME;
      gint x,y;gdk_window_get_origin(window,&x,&y);event->button.x_root=x+32;event->button.y_root=y+32;
      gdk_event_set_device(event,gdk_seat_get_pointer(gdk_display_get_default_seat(gtk_widget_get_display(canvas))));gtk_main_do_event(event);gdk_event_free(event);
    }else gtk_button_clicked(GTK_BUTTON(trigger));drain();
    g_signal_remove_emission_hook(signal,hook);g_assert_nonnull(popup);return popup;
  }
  GtkWidget *tree () {
    auto*monitor=gdk_display_get_monitor_at_window(gtk_widget_get_display(GTK_WIDGET(shell)),gtk_widget_get_window(GTK_WIDGET(shell)));
    auto*dockable=gimp_dialog_factory_dialog_raise(gimp_dialog_factory_get_singleton(),monitor,GTK_WIDGET(shell),"gimp-layer-list",-1);
    g_assert_nonnull(dockable);drain();auto*w=find(dockable,nullptr,GIMP_TYPE_LAYER_TREE_VIEW);
    g_assert_nonnull(w);g_assert_true(GIMP_IS_LAYER_TREE_VIEW(w));
    auto*v=GIMP_CONTAINER_TREE_VIEW(w);auto*column=gtk_tree_view_get_column(v->view,0);
    auto*cells=gtk_cell_layout_get_cells(GTK_CELL_LAYOUT(column));GimpCellRendererPopup*cell=nullptr;
    for(auto*l=cells;l;l=l->next)if(GIMP_IS_CELL_RENDERER_POPUP(l->data))cell=GIMP_CELL_RENDERER_POPUP(l->data);
    g_list_free(cells);g_assert_nonnull(cell);GtkTreeIter iter;GtkTreePath*path=nullptr;
    for(gboolean valid=gtk_tree_model_get_iter_first(v->model,&iter);valid;valid=gtk_tree_model_iter_next(v->model,&iter)){
      auto*r=gimp_container_tree_store_get_renderer(GIMP_CONTAINER_TREE_STORE(v->model),&iter);
      bool match=r->viewable==GIMP_VIEWABLE(filter);g_object_unref(r);if(match){path=gtk_tree_model_get_path(v->model,&iter);break;}
    }
    g_assert_nonnull(path);auto*text=gtk_tree_path_to_string(path);GdkRectangle area;
    gtk_tree_view_get_cell_area(v->view,path,column,&area);
    gimp_cell_renderer_popup_clicked(cell,G_OBJECT(v->view),text,&area);g_free(text);gtk_tree_path_free(path);drain();
    popup=gimp_cell_renderer_popup_get_popup(cell);g_assert_nonnull(popup);g_object_ref(popup);return popup;
  }
  void close_popup(){if(popup){gtk_widget_destroy(popup);g_object_unref(popup);popup=nullptr;drain();}}
  double red(){auto*a=gimp_filter_layer_dup_args(filter);auto*v=g_value_get_variant(gimp_value_array_index(a,3));double r,g,b,alpha;const char*tag;guint version;g_variant_get(v,"(&sudddd)",&tag,&version,&r,&g,&b,&alpha);gimp_value_array_unref(a);return r;}
  guint depth(){return gimp_undo_stack_get_depth(gimp_image_get_undo_stack(image));}
  void edit(const char*value){gtk_entry_set_text(GTK_ENTRY(field(popup,"painter-colorify-red")),value);}
  void apply(){gtk_button_clicked(GTK_BUTTON(field(popup,"painter-filter-apply")));drain();}
  ~Fixture(){if(popup){gtk_widget_destroy(popup);g_object_unref(popup);}gimp_display_delete(display);g_object_unref(filter);g_object_unref(image);drain();}
};
void screenshot(const char*env){if(auto*path=g_getenv(env)){auto until=g_get_monotonic_time()+500*G_TIME_SPAN_MILLISECOND;do{drain();g_usleep(1000);}while(g_get_monotonic_time()<until);auto*root=gdk_get_default_root_window();g_assert_cmpint(gdk_window_get_width(root),==,1080);g_assert_cmpint(gdk_window_get_height(root),==,1920);auto*p=gdk_pixbuf_get_from_window(root,0,0,1080,1920);GError*error=nullptr;g_assert_nonnull(p);g_assert_true(gdk_pixbuf_save(p,path,"png",&error,nullptr));g_assert_no_error(error);g_object_unref(p);}}
void actual_tile_type () {
 Fixture f;auto*p=f.tile(true);g_assert_true(GTK_IS_WINDOW(p));
 g_assert_cmpint(gtk_window_get_window_type(GTK_WINDOW(p)),==,GTK_WINDOW_POPUP);
 g_assert_true(gtk_widget_get_mapped(p));g_assert_nonnull(field(p,"painter-colorify-red"));g_assert_nonnull(field(p,"painter-filter-apply"));screenshot("GIMP_TEST_FILTER_POPUP_SCREENSHOT");
}
void actual_tree_type(){Fixture f;auto*p=f.tree();g_assert_true(GTK_IS_WINDOW(p));g_assert_cmpint(gtk_window_get_window_type(GTK_WINDOW(p)),==,GTK_WINDOW_POPUP);field(p,"painter-colorify-red");field(p,"painter-filter-apply");}
void key(GtkWidget*w,guint code){auto until=g_get_monotonic_time()+2*G_TIME_SPAN_SECOND;while(!gdk_window_is_viewable(gtk_widget_get_window(w))&&g_get_monotonic_time()<until){drain();g_usleep(1000);}g_assert_true(gdk_window_is_viewable(gtk_widget_get_window(w)));auto*e=gdk_event_new(GDK_KEY_PRESS);e->key.window=GDK_WINDOW(g_object_ref(gtk_widget_get_window(w)));e->key.keyval=code;GdkKeymapKey*keys=nullptr;gint count=0;g_assert_true(gdk_keymap_get_entries_for_keyval(gdk_keymap_get_for_display(gtk_widget_get_display(w)),code,&keys,&count));g_assert_cmpint(count,>,0);e->key.hardware_keycode=keys[0].keycode;e->key.group=keys[0].group;g_free(keys);gdk_event_set_device(e,gdk_seat_get_keyboard(gdk_display_get_default_seat(gtk_widget_get_display(w))));gtk_main_do_event(e);gdk_event_free(e);drain();}
void staged_apply_undo(){Fixture f;f.tile(true);screenshot("GIMP_TEST_FILTER_POPUP_SCREENSHOT");const auto depth=f.depth();f.edit("0.5000");g_assert_cmpfloat(f.red(),==,1.);guint changed=0;auto signal=g_signal_connect(field(f.popup,"painter-colorify-red"),"changed",G_CALLBACK(+[](GtkEditable*,gpointer data){++*static_cast<guint*>(data);}),&changed);f.apply();g_signal_handler_disconnect(field(f.popup,"painter-colorify-red"),signal);g_assert_cmpuint(changed,==,0);g_assert_true(gtk_widget_get_visible(f.popup));g_assert_cmpfloat(f.red(),==,.5);g_assert_cmpuint(f.depth(),==,depth+1);g_assert_cmpstr(gtk_entry_get_text(GTK_ENTRY(field(f.popup,"painter-colorify-red"))),==,"0.5000");screenshot("GIMP_TEST_FILTER_POPUP_AFTER_APPLY_SCREENSHOT");
 f.apply();g_assert_true(gtk_widget_get_visible(f.popup));g_assert_cmpuint(f.depth(),==,depth+1);
 f.edit("0.25");f.apply();g_assert_true(gtk_widget_get_visible(f.popup));g_assert_cmpfloat(f.red(),==,.25);g_assert_cmpuint(f.depth(),==,depth+2);
 f.edit("0.1");key(f.popup,GDK_KEY_Escape);g_assert_false(gtk_widget_get_visible(f.popup));g_assert_cmpfloat(f.red(),==,.25);
 g_assert_true(gimp_image_undo(f.image));g_assert_cmpfloat(f.red(),==,.5);g_assert_true(gimp_image_undo(f.image));g_assert_cmpfloat(f.red(),==,1.);
}
void invalid_and_return(){Fixture f;f.tree();auto depth=f.depth();f.edit("not-a-number");f.apply();g_assert_true(gtk_widget_get_visible(f.popup));g_assert_cmpfloat(f.red(),==,1.);g_assert_cmpuint(f.depth(),==,depth);g_assert_true(gtk_widget_get_visible(field(f.popup,"painter-layer-error")));
 f.edit("0.5");gtk_widget_grab_focus(field(f.popup,"painter-colorify-red"));key(f.popup,GDK_KEY_Return);g_assert_true(gtk_widget_get_visible(f.popup));g_assert_cmpfloat(f.red(),==,1.);g_assert_cmpuint(f.depth(),==,depth);
}
void outside_staged_live_opacity(){Fixture f;f.tile();f.edit("0.25");gtk_range_set_value(GTK_RANGE(field(f.popup,"painter-layer-opacity")),37);g_assert_cmpfloat_with_epsilon(gimp_layer_get_opacity(GIMP_LAYER(f.filter)),.37,1e-6);
 auto*e=gdk_event_new(GDK_BUTTON_PRESS);e->button.window=GDK_WINDOW(g_object_ref(gtk_widget_get_window(f.popup)));e->button.x=-10;e->button.y=-10;e->button.button=1;gdk_event_set_device(e,gdk_seat_get_pointer(gdk_display_get_default_seat(gtk_widget_get_display(f.popup))));gtk_widget_event(f.popup,e);gdk_event_free(e);drain();
 g_assert_false(gtk_widget_get_visible(f.popup));g_assert_cmpfloat(f.red(),==,1.);g_assert_cmpfloat_with_epsilon(gimp_layer_get_opacity(GIMP_LAYER(f.filter)),.37,1e-6);
 f.close_popup();f.tile();g_assert_cmpstr(gtk_entry_get_text(GTK_ENTRY(field(f.popup,"painter-colorify-red"))),==,"1");
}
void operation_reload(){Fixture f;f.tile();f.edit("0.125");auto*choice=GTK_COMBO_BOX(field(f.popup,"painter-filter-choice"));g_assert_true(gtk_combo_box_set_active_id(choice,"gauss"));g_assert_true(gtk_combo_box_set_active_id(choice,"colorify"));g_assert_cmpstr(gtk_entry_get_text(GTK_ENTRY(field(f.popup,"painter-colorify-red"))),==,"1");g_assert_cmpfloat(f.red(),==,1.);}
void stale_and_deleted(){
 {Fixture f;f.tile();f.edit("0.5");auto*a=gimp_filter_layer_dup_args(f.filter);auto*v=g_variant_ref_sink(g_variant_new("(sudddd)","gimp-painter-colorify-rgba",1u,.75,1.,1.,1.));g_value_set_variant(gimp_value_array_index(a,3),v);auto*raw=gimp_filter_layer_ref_definition(f.filter);g_assert_true(gimp_filter_layer_set_definition(f.filter,"plug-in-colorify",raw,a,nullptr));g_bytes_unref(raw);gimp_value_array_unref(a);g_variant_unref(v);auto depth=f.depth();f.apply();g_assert_cmpfloat(f.red(),==,.75);g_assert_cmpuint(f.depth(),==,depth);}
 {Fixture f;f.tree();f.edit("0.5");auto*button=field(f.popup,"painter-filter-apply");g_object_ref(button);gimp_image_remove_layer(f.image,GIMP_LAYER(f.filter),FALSE,nullptr);drain();gtk_button_clicked(GTK_BUTTON(button));drain();g_assert_cmpfloat(f.red(),==,1.);g_object_unref(button);}
}
void apply_reentry(){Fixture f;f.tile();f.edit("0.5");struct State{Fixture*f;bool fired;}state{&f,false};auto depth=f.depth();
 auto hook=g_signal_connect(f.image,"undo-event",G_CALLBACK(+[](GimpImage*,GimpUndoEvent event,GimpUndo*,gpointer data){auto&s=*static_cast<State*>(data);if(event==GIMP_UNDO_EVENT_UNDO_PUSHED&&!s.fired){s.fired=true;s.f->apply();}}),&state);
 f.apply();g_signal_handler_disconnect(f.image,hook);g_assert_true(state.fired);g_assert_true(gtk_widget_get_visible(f.popup));g_assert_cmpfloat(f.red(),==,.5);g_assert_cmpuint(f.depth(),==,depth+1);
}

void gaussian_common_guard(){Fixture f;f.tile();auto*choice=GTK_COMBO_BOX(field(f.popup,"painter-filter-choice"));g_assert_true(gtk_combo_box_set_active_id(choice,"gauss"));auto*h=GTK_SPIN_BUTTON(field(f.popup,"painter-gauss-horizontal"));g_assert_cmpfloat(gtk_spin_button_get_value(h),==,5.);gtk_spin_button_set_value(h,11.);g_assert_true(gtk_combo_box_set_active_id(choice,"gauss-iir2"));g_assert_cmpfloat(gtk_spin_button_get_value(h),==,5.);g_assert_true(gtk_combo_box_set_active_id(choice,"gauss"));gtk_spin_button_set_value(h,7.);f.apply();g_assert_true(gtk_widget_get_visible(f.popup));
 auto depth=f.depth();auto*a=gimp_filter_layer_dup_args(f.filter);g_assert_cmpfloat(g_value_get_double(gimp_value_array_index(a,3)),==,7.);g_value_set_double(gimp_value_array_index(a,3),9.);auto*raw=gimp_filter_layer_ref_definition(f.filter);g_assert_true(gimp_filter_layer_set_definition(f.filter,"plug-in-gauss",raw,a,nullptr));gimp_value_array_unref(a);g_bytes_unref(raw);gtk_spin_button_set_value(h,13.);f.apply();a=gimp_filter_layer_dup_args(f.filter);g_assert_cmpfloat(g_value_get_double(gimp_value_array_index(a,3)),==,9.);gimp_value_array_unref(a);g_assert_cmpuint(f.depth(),==,depth);
}

GtkWidget*internal_type(GtkWidget*w,GType type){if(G_TYPE_CHECK_INSTANCE_TYPE(w,type))return w;if(!GTK_IS_CONTAINER(w))return nullptr;GList*children=nullptr;gtk_container_forall(GTK_CONTAINER(w),+[](GtkWidget*child,gpointer data){auto**list=static_cast<GList**>(data);*list=g_list_prepend(*list,child);},&children);GtkWidget*result=nullptr;for(auto*l=children;l&&!result;l=l->next)result=internal_type(GTK_WIDGET(l->data),type);g_list_free(children);return result;}
void menu_open(GtkComboBox*combo){auto*button=internal_type(GTK_WIDGET(combo),GTK_TYPE_TOGGLE_BUTTON);g_assert_nonnull(button);auto*window=gtk_button_get_event_window(GTK_BUTTON(button));g_assert_nonnull(window);auto*device=gdk_seat_get_pointer(gdk_display_get_default_seat(gtk_widget_get_display(button)));
 auto*enter=gdk_event_new(GDK_ENTER_NOTIFY);enter->crossing.window=GDK_WINDOW(g_object_ref(window));enter->crossing.mode=GDK_CROSSING_NORMAL;enter->crossing.detail=GDK_NOTIFY_NONLINEAR;enter->crossing.x=5;enter->crossing.y=5;gdk_event_set_device(enter,device);gtk_main_do_event(enter);gdk_event_free(enter);
 auto*event=gdk_event_new(GDK_BUTTON_PRESS);event->button.window=GDK_WINDOW(g_object_ref(window));event->button.x=5;event->button.y=5;event->button.button=1;event->button.time=GDK_CURRENT_TIME;gdk_event_set_device(event,device);gtk_main_do_event(event);gdk_event_free(event);drain();
}

void operation_menu_grab(){Fixture f;f.tile();auto*choice=GTK_COMBO_BOX(field(f.popup,"painter-filter-choice"));
 menu_open(choice);drain();gboolean shown=FALSE;g_object_get(choice,"popup-shown",&shown,nullptr);g_assert_true(shown);g_assert_true(gtk_widget_get_visible(f.popup));
 auto*grab=gtk_grab_get_current();g_assert_nonnull(grab);g_assert_true(grab!=f.popup);key(grab,GDK_KEY_Escape);drain();g_object_get(choice,"popup-shown",&shown,nullptr);g_assert_false(shown);g_assert_true(gtk_widget_get_visible(f.popup));g_assert_true(gtk_grab_get_current()==f.popup);g_assert_cmpfloat(f.red(),==,1.);
 menu_open(choice);drain();g_assert_true(gtk_combo_box_set_active_id(choice,"gauss"));gtk_combo_box_popdown(choice);drain();g_assert_true(gtk_grab_get_current()==f.popup);g_assert_true(gtk_widget_get_visible(f.popup));g_assert_cmpfloat(f.red(),==,1.);
}

void postcommit_uncertain(){Fixture f;f.tile();f.edit("0.5");struct State{Fixture*f;guint64 revision;bool fired;}state{&f,gimp_filter_layer_get_definition_revision(f.filter),false};
 auto hook=g_signal_connect(f.filter,"filter-state-changed",G_CALLBACK(+[](GimpFilterLayer*layer,gpointer data){auto&s=*static_cast<State*>(data);if(!s.fired&&gimp_filter_layer_get_definition_revision(layer)>s.revision){s.fired=true;s.f->edit("0.75");}}),&state);
 f.apply();g_signal_handler_disconnect(f.filter,hook);g_assert_true(state.fired);g_assert_cmpfloat(f.red(),==,.5);g_assert_true(gtk_widget_get_visible(f.popup));g_assert_false(gtk_widget_get_sensitive(field(f.popup,"painter-filter-apply")));g_assert_true(gtk_widget_get_visible(field(f.popup,"painter-layer-error")));auto depth=f.depth();f.apply();g_assert_cmpfloat(f.red(),==,.5);g_assert_cmpuint(f.depth(),==,depth);
}
void anchor_teardown(){Fixture f;f.tile();auto*tiles=find(f.shell->canvas,nullptr,GIMP_TYPE_PAINTER_LAYER_TILES);g_assert_nonnull(tiles);gtk_widget_hide(tiles);drain();g_assert_false(gtk_widget_get_visible(f.popup));gtk_widget_show(tiles);f.close_popup();f.tile();g_assert_true(gtk_widget_get_visible(f.popup));}

void operation_switch_reentry(){Fixture f;f.tile();auto*choice=GTK_COMBO_BOX(field(f.popup,"painter-filter-choice"));g_assert_true(gtk_combo_box_set_active_id(choice,"gauss"));auto*h=GTK_SPIN_BUTTON(field(f.popup,"painter-gauss-horizontal"));auto*v=GTK_SPIN_BUTTON(field(f.popup,"painter-gauss-vertical"));gtk_spin_button_set_value(h,11.);
 struct State{GtkSpinButton*other;bool fired;}state{v,false};auto hook=g_signal_connect(h,"value-changed",G_CALLBACK(+[](GtkSpinButton*,gpointer data){auto&s=*static_cast<State*>(data);if(!s.fired){s.fired=true;gtk_spin_button_set_value(s.other,17.);}}),&state);
 g_assert_true(gtk_combo_box_set_active_id(choice,"gauss-iir2"));g_signal_handler_disconnect(h,hook);g_assert_true(state.fired);auto depth=f.depth();f.apply();g_assert_cmpfloat(f.red(),==,1.);g_assert_cmpuint(f.depth(),==,depth);g_assert_false(gtk_widget_get_sensitive(field(f.popup,"painter-filter-apply")));
}

void published_definition_reentry(){Fixture f;f.tile();f.edit("0.5");struct State{guint64 revision;bool fired;}state{gimp_filter_layer_get_definition_revision(f.filter),false};auto hook=g_signal_connect(f.filter,"filter-state-changed",G_CALLBACK(+[](GimpFilterLayer*layer,gpointer data){auto&s=*static_cast<State*>(data);if(s.fired||gimp_filter_layer_get_definition_revision(layer)<=s.revision)return;s.fired=true;auto*a=gimp_filter_layer_dup_args(layer);auto*v=g_variant_ref_sink(g_variant_new("(sudddd)","gimp-painter-colorify-rgba",1u,.75,1.,1.,1.));g_value_set_variant(gimp_value_array_index(a,3),v);auto*raw=gimp_filter_layer_ref_definition(layer);g_assert_true(gimp_filter_layer_set_definition(layer,"plug-in-colorify",raw,a,nullptr));g_bytes_unref(raw);gimp_value_array_unref(a);g_variant_unref(v);}),&state);
 f.apply();g_signal_handler_disconnect(f.filter,hook);g_assert_true(state.fired);g_assert_cmpfloat(f.red(),==,.75);g_assert_false(gtk_widget_get_sensitive(field(f.popup,"painter-filter-apply")));auto depth=f.depth();f.apply();g_assert_cmpfloat(f.red(),==,.75);g_assert_cmpuint(f.depth(),==,depth);
}

void isolated_routes_repeated_apply()
{
  const char*routes[]={"blinds","small-tiles","retinex","convmatrix"};
  for(guint route=0;route<4;++route){
    Fixture f;f.tile();auto*choice=GTK_COMBO_BOX(field(f.popup,"painter-filter-choice"));
    g_assert_true(gtk_combo_box_set_active_id(choice,routes[route]));
    g_assert_true(gtk_widget_get_sensitive(field(f.popup,"painter-filter-apply")));
    const auto revision=gimp_filter_layer_get_definition_revision(f.filter);const auto depth=f.depth();
    f.apply();g_assert_true(gtk_widget_get_visible(f.popup));g_assert_true(gtk_widget_get_sensitive(field(f.popup,"painter-filter-apply")));
    g_assert_cmpuint(gimp_filter_layer_get_definition_revision(f.filter),==,revision+1);g_assert_cmpuint(f.depth(),==,depth+1);
    auto*procedure=gimp_filter_layer_dup_procedure(f.filter);auto*expected=g_strconcat("plug-in-",routes[route],nullptr);g_assert_cmpstr(procedure,==,expected);g_free(procedure);g_free(expected);
    f.apply();g_assert_true(gtk_widget_get_visible(f.popup));g_assert_true(gtk_widget_get_sensitive(field(f.popup,"painter-filter-apply")));g_assert_cmpuint(gimp_filter_layer_get_definition_revision(f.filter),==,revision+1);g_assert_cmpuint(f.depth(),==,depth+1);
    auto text=[&](const char*name,const char*value){gtk_entry_set_text(GTK_ENTRY(field(f.popup,name)),value);};
    if(route==0){text("painter-blinds-angle","23");text("painter-blinds-segments","7");g_assert_true(gtk_combo_box_set_active_id(GTK_COMBO_BOX(field(f.popup,"painter-blinds-orientation")),"vertical"));gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(field(f.popup,"painter-blinds-transparent")),TRUE);}
    if(route==1)text("painter-small-tiles-factor","3");
    if(route==2){text("painter-retinex-scale","64");text("painter-retinex-nscales","2");text("painter-retinex-cvar","1.25");g_assert_true(gtk_combo_box_set_active_id(GTK_COMBO_BOX(field(f.popup,"painter-retinex-mode")),"2"));}
    if(route==3){text("painter-convmatrix-0","0.25");gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(field(f.popup,"painter-convmatrix-channel-0")),FALSE);text("painter-convmatrix-divisor","2");text("painter-convmatrix-offset","1.5");g_assert_true(gtk_combo_box_set_active_id(GTK_COMBO_BOX(field(f.popup,"painter-convmatrix-border")),"2"));}
    f.apply();g_assert_true(gtk_widget_get_visible(f.popup));g_assert_true(gtk_widget_get_sensitive(field(f.popup,"painter-filter-apply")));g_assert_cmpuint(gimp_filter_layer_get_definition_revision(f.filter),==,revision+2);g_assert_cmpuint(f.depth(),==,depth+2);
    auto*a=gimp_filter_layer_dup_args(f.filter);
    auto integer=[&](guint index){auto*v=gimp_value_array_index(a,index);g_assert_true(G_VALUE_HOLDS_INT(v));return g_value_get_int(v);};
    auto real=[&](guint index){auto*v=gimp_value_array_index(a,index);g_assert_true(G_VALUE_HOLDS_DOUBLE(v));return g_value_get_double(v);};
    if(route==0){g_assert_cmpint(integer(3),==,23);g_assert_cmpint(integer(4),==,7);g_assert_cmpint(integer(5),==,1);g_assert_cmpint(integer(6),==,1);}
    if(route==1)g_assert_cmpint(integer(3),==,3);
    if(route==2){g_assert_cmpint(integer(3),==,64);g_assert_cmpint(integer(4),==,2);g_assert_cmpint(integer(5),==,2);g_assert_cmpfloat(real(6),==,1.25);}
    if(route==3){gsize count=0;auto*matrix=gimp_value_get_double_array(gimp_value_array_index(a,4),&count);g_assert_cmpuint(count,==,25);g_assert_cmpfloat(matrix[0],==,.25);auto*channels=gimp_value_get_int32_array(gimp_value_array_index(a,9),&count);g_assert_cmpuint(count,==,5);g_assert_cmpint(channels[0],==,0);g_assert_cmpfloat(real(6),==,2.);g_assert_cmpfloat(real(7),==,1.5);g_assert_cmpint(integer(10),==,2);}
    gimp_value_array_unref(a);
    f.apply();g_assert_true(gtk_widget_get_visible(f.popup));g_assert_true(gtk_widget_get_sensitive(field(f.popup,"painter-filter-apply")));g_assert_cmpuint(gimp_filter_layer_get_definition_revision(f.filter),==,revision+2);g_assert_cmpuint(f.depth(),==,depth+2);
    if(route==0)text("painter-blinds-angle","24");
    if(route==1)text("painter-small-tiles-factor","4");
    if(route==2)text("painter-retinex-cvar","1.5");
    if(route==3)text("painter-convmatrix-offset","2.5");
    f.apply();g_assert_true(gtk_widget_get_visible(f.popup));g_assert_true(gtk_widget_get_sensitive(field(f.popup,"painter-filter-apply")));g_assert_cmpuint(gimp_filter_layer_get_definition_revision(f.filter),==,revision+3);g_assert_cmpuint(f.depth(),==,depth+3);
    a=gimp_filter_layer_dup_args(f.filter);
    if(route<2)g_assert_cmpint(integer(3),==,route==0?24:4);
    else g_assert_cmpfloat(real(route==2?6:7),==,route==2?1.5:2.5);
    gimp_value_array_unref(a);
  }
}

void isolated_patch_postcommit_reentry()
{
  Fixture f;f.tile();auto*choice=GTK_COMBO_BOX(field(f.popup,"painter-filter-choice"));
  g_assert_true(gtk_combo_box_set_active_id(choice,"small-tiles"));f.apply();
  g_assert_true(gtk_widget_get_sensitive(field(f.popup,"painter-filter-apply")));
  auto*entry=field(f.popup,"painter-small-tiles-factor");gtk_entry_set_text(GTK_ENTRY(entry),"3");
  struct State{GtkWidget*entry;guint64 revision;bool fired;}state{entry,gimp_filter_layer_get_definition_revision(f.filter),false};
  const auto depth=f.depth();
  auto hook=g_signal_connect(f.filter,"filter-state-changed",G_CALLBACK(+[](GimpFilterLayer*layer,gpointer data){
    auto&s=*static_cast<State*>(data);
    if(!s.fired&&gimp_filter_layer_get_definition_revision(layer)>s.revision){s.fired=true;gtk_entry_set_text(GTK_ENTRY(s.entry),"4");}
  }),&state);
  f.apply();g_signal_handler_disconnect(f.filter,hook);g_assert_true(state.fired);
  auto*a=gimp_filter_layer_dup_args(f.filter);g_assert_cmpint(g_value_get_int(gimp_value_array_index(a,3)),==,3);gimp_value_array_unref(a);
  g_assert_cmpuint(gimp_filter_layer_get_definition_revision(f.filter),==,state.revision+1);g_assert_cmpuint(f.depth(),==,depth+1);
  g_assert_false(gtk_widget_get_sensitive(field(f.popup,"painter-filter-apply")));g_assert_true(gtk_widget_get_visible(field(f.popup,"painter-layer-error")));
  f.apply();g_assert_cmpuint(gimp_filter_layer_get_definition_revision(f.filter),==,state.revision+1);g_assert_cmpuint(f.depth(),==,depth+1);
}

void ordinary_dialog_unchanged(){Fixture f;auto*p=painter_filter_layer_dialog_new(f.image,GIMP_LAYER(f.filter),app->user_context,GTK_WIDGET(f.shell));g_assert_true(GTK_IS_DIALOG(p));g_assert_cmpint(gtk_window_get_window_type(GTK_WINDOW(p)),==,GTK_WINDOW_TOPLEVEL);gtk_widget_destroy(p);
 p=painter_filter_layer_dialog_new(f.image,nullptr,app->user_context,GTK_WIDGET(f.shell));g_assert_true(GTK_IS_DIALOG(p));g_assert_cmpint(gtk_window_get_window_type(GTK_WINDOW(p)),==,GTK_WINDOW_TOPLEVEL);gtk_widget_destroy(p);
}

}
extern "C" void gimp_test_filter_layer_popup_register(Gimp*application){app=application;g_test_add_func("/filter-popup/actual-tile-type",actual_tile_type);g_test_add_func("/filter-popup/actual-tree-type",actual_tree_type);g_test_add_func("/filter-popup/staged-apply-undo",staged_apply_undo);g_test_add_func("/filter-popup/invalid-return",invalid_and_return);g_test_add_func("/filter-popup/ordinary-dialog-unchanged",ordinary_dialog_unchanged);g_test_add_func("/filter-popup/outside-staged-live-opacity",outside_staged_live_opacity);g_test_add_func("/filter-popup/operation-reload",operation_reload);g_test_add_func("/filter-popup/stale-deleted",stale_and_deleted);g_test_add_func("/filter-popup/apply-reentry",apply_reentry);g_test_add_func("/filter-popup/gaussian-common-guard",gaussian_common_guard);g_test_add_func("/filter-popup/operation-menu-grab",operation_menu_grab);g_test_add_func("/filter-popup/postcommit-uncertain",postcommit_uncertain);g_test_add_func("/filter-popup/anchor-teardown",anchor_teardown);g_test_add_func("/filter-popup/operation-switch-reentry",operation_switch_reentry);g_test_add_func("/filter-popup/published-definition-reentry",published_definition_reentry);g_test_add_func("/filter-popup/isolated-routes-repeated-apply",isolated_routes_repeated_apply);g_test_add_func("/filter-popup/isolated-patch-postcommit-reentry",isolated_patch_postcommit_reentry);}
