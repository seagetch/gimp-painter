/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gegl.h>
#include <gtk/gtk.h>
#include <vector>
#include <cmath>
extern "C" {
#include "core/core-types.h"
#include "widgets/widgets-types.h"
#include "core/gimp.h"
#include "core/gimpcontext.h"
#include "core/gimpimage.h"
#include "core/gimpimage-undo.h"
#include "core/gimpundostack.h"
#include "core/gimplayer.h"
#include "core/gimplayer-new.h"
#include "paint/gimppaintoptions.h"
#include "tests.h"
#include "gimp-app-test-utils.h"
}
#include "paint/painter-mypaint-surface/paint-core.hpp"
using namespace GimpPainter::MyPaint;
static Gimp*gimp;
struct Scene {
  GimpImage*image;GimpDrawable*drawable;GimpPaintOptions*options;
  Scene(){
    image=gimp_image_new(gimp,48,40,GIMP_GRAY,GIMP_PRECISION_U8_NON_LINEAR);
    auto*layer=gimp_layer_new(image,32,24,babl_format("Y' u8"),"Native Gray",1,GIMP_LAYER_MODE_NORMAL);g_assert_true(gimp_image_add_layer(image,layer,nullptr,0,FALSE));drawable=GIMP_DRAWABLE(layer);gimp_item_set_offset(GIMP_ITEM(layer),7,3);
    std::vector<guchar> initial(32*24);for(std::size_t i=0;i<initial.size();++i)initial[i]=i%256;
    gegl_buffer_set(gimp_drawable_get_buffer(drawable),GEGL_RECTANGLE(0,0,32,24),0,babl_format("Y' u8"),initial.data(),GEGL_AUTO_ROWSTRIDE);
    options=GIMP_PAINT_OPTIONS(g_object_new(GIMP_TYPE_PAINT_OPTIONS,"gimp",gimp,nullptr));auto*color=gegl_color_new("red");gimp_context_set_foreground(GIMP_CONTEXT(options),color);g_object_unref(color);gimp_image_undo_free(image);
  }
  ~Scene(){g_object_unref(options);g_object_unref(image);}
  std::vector<guchar> pixels(){auto*buffer=gimp_drawable_get_buffer(drawable);g_assert_true(gegl_buffer_get_format(buffer)==babl_format("Y' u8"));std::vector<guchar> p(32*24);gegl_buffer_get(buffer,GEGL_RECTANGLE(0,0,32,24),1,gegl_buffer_get_format(buffer),p.data(),GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);return p;}
};
static Resource resource(){Resource r;r.set_base_value(BRUSH_DABS_PER_SECOND,40);r.set_base_value(BRUSH_RADIUS_LOGARITHMIC,1.5);return r;}
static void stroke(PaintCore&core,GimpDrawable*d){GimpCoords c=GIMP_COORDS_DEFAULT_VALUES;c.x=12;c.y=10;c.pressure=0;core.stroke_to(d,.01,c);c.pressure=.85;core.stroke_to(d,.05,c);for(int i=0;i<8;++i){c.x+=1;core.stroke_to(d,.012,c);}}
static void native_gray_undo_cancel()
{
  Scene s;const auto before=s.pixels();PaintCore core(s.options,resource());stroke(core,s.drawable);core.finish();const auto after=s.pixels();g_assert_true(after!=before);g_assert_cmpint(gimp_undo_stack_get_depth(gimp_image_get_undo_stack(s.image)),==,1);
  g_assert_true(gimp_image_undo(s.image));g_assert_true(s.pixels()==before);g_assert_true(gimp_image_redo(s.image));g_assert_true(s.pixels()==after);
  stroke(core,s.drawable);core.cancel();g_assert_true(s.pixels()==after);g_assert_cmpint(gimp_undo_stack_get_depth(gimp_image_get_undo_stack(s.image)),==,1);g_assert_false(gimp_viewable_preview_is_frozen(GIMP_VIEWABLE(s.drawable)));
}
static void red_channel_and_gray_sampling()
{
  auto*buffer=gegl_buffer_new(GEGL_RECTANGLE(0,0,1,1),babl_format("Y' u8"));guchar zero=0;gegl_buffer_set(buffer,GEGL_RECTANGLE(0,0,1,1),0,babl_format("Y' u8"),&zero,GEGL_AUTO_ROWSTRIDE);
  GeglSurface surface(buffer);surface.begin_session();g_assert_true(surface.draw_dab(.5,.5,4,1,0,0,1,1,1,1,0,0,0,0,1));guchar result=0;gegl_buffer_get(buffer,GEGL_RECTANGLE(0,0,1,1),1,babl_format("Y' u8"),&result,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);g_assert_cmpint(result,==,255);
  float r,g,b,a;surface.get_color(.5,.5,4,&r,&g,&b,&a,1,1,0,0,1);g_assert_cmpfloat(r,==,1);g_assert_cmpfloat(g,==,1);g_assert_cmpfloat(b,==,1);g_assert_cmpfloat(a,==,1);
  surface.draw_dab(.5,.5,4,0,1,0,1,1,1,1,0,0,0,0,1);gegl_buffer_get(buffer,GEGL_RECTANGLE(0,0,1,1),1,babl_format("Y' u8"),&result,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);g_assert_cmpint(result,==,0);surface.end_session();g_object_unref(buffer);
}
static void unsupported_precision_policy()
{
  // This byte renderer refuses native precision/TRC conversion before painting.
  for(const char*name:{"Y u8","Y' u16","Y float","Y' float","Y' half","RGB u8","RGBA u8","R'G'B' u16","R'G'B'A u16","RGBA float","R'G'B'A float","R'G'B'A half"}){
    auto*buffer=gegl_buffer_new(GEGL_RECTANGLE(0,0,4,4),babl_format(name));bool rejected=false;try{GeglSurface surface(buffer);}catch(const std::invalid_argument&){rejected=true;}g_assert_true(rejected);g_object_unref(buffer);
  }
}
static void native_gray_nonincremental()
{
  Scene s;const auto before=s.pixels();auto r=resource();r.set_switch(BRUSH_NON_INCREMENTAL,true);r.set_base_value(BRUSH_STROKE_OPACITY,.37);
  PaintCore core(s.options,r);stroke(core,s.drawable);core.finish();const auto painted=s.pixels();
  g_assert_true(painted!=before);g_assert_cmpint(gimp_undo_stack_get_depth(gimp_image_get_undo_stack(s.image)),==,1);
  g_assert_true(gimp_image_undo(s.image));g_assert_true(s.pixels()==before);g_assert_true(gimp_image_redo(s.image));g_assert_true(s.pixels()==painted);
  g_assert_false(core.active());g_assert_false(gimp_viewable_preview_is_frozen(GIMP_VIEWABLE(s.drawable)));
}
int main(int argc,char**argv)
{
  g_test_init(&argc,&argv,nullptr);gimp_test_utils_set_gimp3_directory("GIMP_TESTING_ABS_TOP_SRCDIR","app/tests/gimpdir");gimp=gimp_init_for_testing();
  g_test_add_func("/painter-gray/native-undo-cancel",native_gray_undo_cancel);
  g_test_add_func("/painter-gray/red-component-sampling",red_channel_and_gray_sampling);
  g_test_add_func("/painter-gray/precision-policy",unsupported_precision_policy);
  g_test_add_func("/painter-gray/defined-nonincremental",native_gray_nonincremental);
  const int result=g_test_run();gimp_test_utils_set_gimp3_directory("GIMP_TESTING_ABS_TOP_BUILDDIR","app/tests/gimpdir-output");gimp_exit(gimp,TRUE);return result;
}
