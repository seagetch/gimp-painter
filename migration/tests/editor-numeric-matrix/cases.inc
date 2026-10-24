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
}
