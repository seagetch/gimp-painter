/* SPDX-License-Identifier: GPL-3.0-or-later
 * Actual context -> PaintCore -> evaluator -> native drawable conversion.
 * The reference asks GEGL for source-color -> drawable space directly; it does
 * not call Surface's format/color helpers. Byte Gray remains the documented
 * original red-component policy, rather than a new luminance policy.
 */
static void color_placement_ingress()
{
 unsigned cells=0;
 for(auto precision:{GIMP_PRECISION_U8_NON_LINEAR,GIMP_PRECISION_DOUBLE_LINEAR,
                     GIMP_PRECISION_DOUBLE_NON_LINEAR,GIMP_PRECISION_DOUBLE_PERCEPTUAL})
 for(auto base:{GIMP_RGB,GIMP_GRAY})for(bool custom:{false,true}){
  auto *image=gimp_image_new(gimp,32,24,base,precision);GError *error=nullptr;
  if(custom){auto *profile=base==GIMP_RGB?gimp_color_profile_new_rgb_adobe():gimp_color_profile_new_d50_gray_lab_trc();g_assert_true(gimp_image_set_color_profile(image,profile,&error));g_assert_no_error(error);g_object_unref(profile);}
  auto *format=gimp_image_get_layer_format(image,TRUE);auto *layer=gimp_layer_new(image,32,24,format,"Color placement",1,GIMP_LAYER_MODE_NORMAL);g_assert_true(gimp_image_add_layer(image,layer,nullptr,0,FALSE));
  auto *d=GIMP_DRAWABLE(layer);auto *buffer=gimp_drawable_get_buffer(d);gegl_buffer_clear(buffer,nullptr);
  auto *options=GIMP_PAINT_OPTIONS(g_object_new(GIMP_TYPE_PAINT_OPTIONS,"gimp",gimp,nullptr));auto *color=gegl_color_new("rgb(0.8,0.2,0.4)");gimp_context_set_foreground(GIMP_CONTEXT(options),color);
  const bool gray=base==GIMP_GRAY,byte=precision==GIMP_PRECISION_U8_NON_LINEAR;
  const char *model=babl_get_name(babl_format_get_model(format));std::string read_name=std::string(model)+" double";
  auto *read_format=babl_format_with_space(read_name.c_str(),babl_format_get_space(format));
  double direct[4]={},expected[4]={},evaluator[4]={};
  gegl_color_get_pixel(color,read_format,direct);
  auto *evaluation=babl_format_with_space("R'G'B'A double",babl_format_get_space(format));
  gegl_color_get_pixel(color,evaluation,evaluator);
  // Independently spell out the two documented domain boundaries. A direct
  // profile transform is separately bounded, not assumed bit-identical to
  // Babl's RGB evaluator intermediate for custom Gray ICC.
  babl_process(babl_fish(evaluation,read_format),evaluator,expected,1);
  if(gray&&byte)expected[0]=evaluator[0];
  else for(int c=0;c<(gray?1:3);++c)g_assert_cmpfloat_with_epsilon(expected[c],direct[c],3e-4);
  Resource resource;resource.set_base_value(BRUSH_DABS_PER_SECOND,40);resource.set_base_value(BRUSH_RADIUS_LOGARITHMIC,1.5);PaintCore core(options,resource);stroke(core,d);core.finish();
  const int n=gray?2:4;std::vector<double> out(32*24*n);gegl_buffer_get(buffer,GEGL_RECTANGLE(0,0,32,24),1,read_format,out.data(),GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
  unsigned painted=0;for(std::size_t i=0;i<out.size();i+=n)if(out[i+n-1]>.5){++painted;for(int c=0;c<n-1;++c)g_assert_cmpfloat_with_epsilon(out[i+c],expected[c],byte?2./255:2e-6);}
  g_assert_cmpuint(painted,>,0);g_test_message("context placement precision=%d base=%d custom=%d checked=%u",precision,base,custom,painted);
  g_object_unref(color);g_object_unref(options);g_object_unref(image);++cells;
 }
 g_assert_cmpuint(cells,==,16);
}
