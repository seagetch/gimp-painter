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
