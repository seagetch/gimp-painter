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
