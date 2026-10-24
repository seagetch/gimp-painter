from pathlib import Path
import json,subprocess
R=Path('/workspace/scratch/0015fc5d1a03/gimp-painter');W=Path('/workspace/shared/editor-curve-matrix')
base=Path('/workspace/shared/editor-numeric-matrix/fixture.cpp').read_text().split('struct NumericExpected')[0]
meta=json.loads(subprocess.check_output(['git','show','HEAD:app/paint/painter-brush-settings/brush-settings.json'],cwd=R))
inputs='\n'.join(' {"%s",%d,double(float(%s)),double(float(%s))},'%(x['name'],x['index'],x['soft_minimum'],x['soft_maximum']) for x in meta['inputs'])
(W/'input-oracle.json').write_text(json.dumps(meta['inputs'],indent=2)+'\n')
body=r'''
struct InputExpected {const char*name;int index;double minimum,maximum;};
static const InputExpected inputs[]={
INPUT_ROWS
};
static void select_point(Editor&e,int n){gtk_combo_box_set_active(GTK_COMBO_BOX(e.get("painter-curve-point")),n);}
static void assert_curve(Editor&e,const std::vector<MyPaint::Point>&points,int input){
 auto actual=e.snapshot().curve(BRUSH_TEXTURE_GRAIN,input);g_assert_cmpuint(actual.size(),==,points.size());
 auto*combo=GTK_COMBO_BOX(e.get("painter-curve-point"));auto*m=gtk_combo_box_get_model(combo);g_assert_cmpint(gtk_tree_model_iter_n_children(m,nullptr),==,int(points.size()));
 for(unsigned n=0;n<points.size();++n){g_assert_cmpfloat(actual[n].x,==,points[n].x);g_assert_cmpfloat(actual[n].y,==,points[n].y);select_point(e,n);g_assert_cmpfloat(gtk_spin_button_get_value(GTK_SPIN_BUTTON(e.get("painter-curve-x"))),==,points[n].x);g_assert_cmpfloat(gtk_spin_button_get_value(GTK_SPIN_BUTTON(e.get("painter-curve-y"))),==,points[n].y);}
 g_assert_cmpint(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(e.get("painter-curve-enabled"))),==,!points.empty());
}
static void click(Editor&e,const char*name){gtk_button_clicked(GTK_BUTTON(e.get(name)));}
static void coordinates(Editor&e,double x,double y){gtk_spin_button_set_value(GTK_SPIN_BUTTON(e.get("painter-curve-x")),x);gtk_spin_button_set_value(GTK_SPIN_BUTTON(e.get("painter-curve-y")),y);}
static void curve_matrix(){
 auto options=ObjectRef<GimpPainterMybrushOptions>::adopt(GIMP_PAINTER_MYBRUSH_OPTIONS(g_object_new(GIMP_TYPE_PAINTER_MYBRUSH_OPTIONS,"gimp",gimp,nullptr)));
 Editor a(true,GIMP_CONTEXT(options.get())),b(false,GIMP_CONTEXT(options.get()));g_assert_true(a.model.get()==b.model.get());
 auto expected=MyPaint::Resource::decode("{\"version\":3,\"curve_matrix_unknown\":{\"nested\":[null,9007199254740993,\"日本語\"]},\"settings\":{},\"texts\":{\"future_curve_reference\":\"unresolved\"}}");expected.set_base_value(BRUSH_TEXTURE_GRAIN,.123456789012);expected.set_base_value(BRUSH_RADIUS_LOGARITHMIC,6);expected.set_curve(BRUSH_SMUDGE,INPUT_CUSTOM,{{-1,.125},{1,.875}});a.set(expected);
 guint signals=0;gulong handler=g_signal_connect(options.get(),"settings-changed",G_CALLBACK(+[](GObject*,gpointer data){++*static_cast<guint*>(data);}),&signals);
 guint success=0,rejected=0;
 for(const auto&in:inputs){
  for(auto*e:{&a,&b}){gtk_combo_box_set_active(GTK_COMBO_BOX(e->get("painter-curve-setting")),BRUSH_TEXTURE_GRAIN);gtk_combo_box_set_active(GTK_COMBO_BOX(e->get("painter-curve-input")),in.index);}
  std::vector<MyPaint::Point> points;
  auto check=[&]{assert_curve(a,points,in.index);assert_curve(b,points,in.index);g_assert_true(a.snapshot().encode()==expected.encode());g_assert_cmpuint(signals,==,success);};
  check();
  for(unsigned n=2;n<=8;++n){click(a,"painter-curve-add");if(n==2)points={{in.minimum,0},{in.maximum,0}};else{points.push_back({(in.minimum+in.maximum)/2,0});std::stable_sort(points.begin(),points.end(),[](MyPaint::Point x,MyPaint::Point y){return x.x<y.x;});}expected.set_curve(BRUSH_TEXTURE_GRAIN,in.index,points);++success;check();}
  click(a,"painter-curve-add");++rejected;g_assert_nonnull(strstr(gtk_label_get_text(GTK_LABEL(a.get("painter-brush-status"))),"eight points"));check();
  // Invalid entered draft is not required to disappear: canonical model and peer must stay intact.
  select_point(a,0);coordinates(a,in.maximum+1,.75);click(a,"painter-curve-apply");++rejected;g_assert_nonnull(strstr(gtk_label_get_text(GTK_LABEL(a.get("painter-brush-status"))),"descending"));g_assert_true(a.snapshot().encode()==expected.encode());g_assert_cmpuint(signals,==,success);assert_curve(b,points,in.index);
  // Replace the rejected draft with a valid exact-double edit, then mutate from the other view.
  select_point(a,0);coordinates(a,in.minimum+.123456789012*(in.maximum-in.minimum),.234567890123);click(a,"painter-curve-apply");points[0]={in.minimum+.123456789012*(in.maximum-in.minimum),.234567890123};expected.set_curve(BRUSH_TEXTURE_GRAIN,in.index,points);++success;check();
  select_point(b,points.size()-1);coordinates(b,in.maximum-.123456789012*(in.maximum-in.minimum),-.345678901234);click(b,"painter-curve-apply");points.back()={in.maximum-.123456789012*(in.maximum-in.minimum),-.345678901234};expected.set_curve(BRUSH_TEXTURE_GRAIN,in.index,points);++success;check();
  if(in.index==INPUT_CUSTOM){
   const char*shot=g_getenv("GIMP_TEST_CURVE_SCREENSHOT");g_assert_nonnull(shot);
   gimp_test_utils_create_image(gimp,640,480);auto*display=gimp_context_get_display(gimp->user_context);g_assert_nonnull(display);auto*shell=gimp_display_get_shell(display);gimp_painter_canvas_ui_set_visible(shell,TRUE);g_assert_true(gimp_painter_canvas_ui_get_visible(shell));
   auto*main=gtk_widget_get_toplevel(GTK_WIDGET(shell));gtk_window_resize(GTK_WINDOW(main),1040,1850);gtk_window_move(GTK_WINDOW(main),0,0);gtk_widget_hide(b.window);gtk_window_resize(GTK_WINDOW(a.window),1000,1180);gtk_window_move(GTK_WINDOW(a.window),20,650);gtk_window_set_title(GTK_WINDOW(a.window),"Nine input curves: ordinary add, move, delete and rejected-draft recovery");
   auto*tab=gtk_widget_get_parent(a.get("painter-curve-graph"));while(tab&&!GTK_IS_NOTEBOOK(tab))tab=gtk_widget_get_parent(tab);g_assert_nonnull(tab);gtk_notebook_set_current_page(GTK_NOTEBOOK(tab),2);
   gimp_test_run_temp_mainloop(500);gtk_window_present(GTK_WINDOW(a.window));gimp_test_run_temp_mainloop(200);gdk_window_raise(gtk_widget_get_window(a.window));gdk_display_sync(gdk_display_get_default());g_assert_true(gtk_widget_is_drawable(a.get("painter-curve-graph")));
   auto*root=gdk_get_default_root_window();g_assert_cmpint(gdk_window_get_width(root),==,1080);g_assert_cmpint(gdk_window_get_height(root),==,1920);auto pixels=ObjectRef<GObject>::adopt(G_OBJECT(gdk_pixbuf_get_from_window(root,0,0,1080,1920)));g_assert_nonnull(pixels.get());GError*error=nullptr;g_assert_true(gdk_pixbuf_save(GDK_PIXBUF(pixels.get()),shot,"png",&error,nullptr));g_assert_no_error(error);g_test_message("API/button-driven curve edits; actual oncanvas enabled1080x1920; not physical pointer gestures");
  }
  while(!points.empty()){select_point(a,0);click(a,"painter-curve-remove");if(points.size()<=2)points.clear();else points.erase(points.begin());expected.set_curve(BRUSH_TEXTURE_GRAIN,in.index,points);++success;check();}
  g_test_message("%s: add2..8, ninth rejected, descending rejected, exact move A/B, remove8..2..0, resource preservation and exact notifications PASS",in.name);
 }
 g_assert_cmpuint(success,==,144);g_assert_cmpuint(rejected,==,18);g_signal_handler_disconnect(options.get(),handler);
}
int main(int argc,char**argv){g_test_init(&argc,&argv,nullptr);if(!gtk_init_check(&argc,&argv))return GIMP_EXIT_TEST_SKIPPED;gimp_test_utils_setup_menus_path();gimp=gimp_init_for_gui_testing(TRUE);add_editor_case("/painter-curve/nine-inputs",curve_matrix);g_application_run(gimp->app,0,nullptr);int result=gimp_core_app_get_exit_status(GIMP_CORE_APP(gimp->app));g_application_quit(gimp->app);g_clear_object(&gimp->app);return result;}
'''.replace('INPUT_ROWS',inputs)
(W/'fixture.cpp').write_text(base+body)
