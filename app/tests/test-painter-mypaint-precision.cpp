/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gtk/gtk.h>
#include <gegl.h>
#include <vector>
#include <limits>
extern "C" {
#include "libgimpbase/gimpbase.h"
#include "libgimpmath/gimpmath.h"
#include "libgimpcolor/gimpcolor.h"
#include "core/core-types.h"
#include "widgets/widgets-types.h"
#include "core/gimp.h"
#include "core/gimpcontext.h"
#include "core/gimpdrawable.h"
#include "core/gimpimage.h"
#include "core/gimpimage-color-profile.h"
#include "core/gimpimage-undo.h"
#include "core/gimpundostack.h"
#include "core/gimplayer.h"
#include "core/gimplayer-new.h"
#include "core/gimpchannel.h"
#include "core/gimpchannel-select.h"
#include "gegl/gimp-babl.h"
#include "paint/gimppaintoptions.h"
#include "tests.h"
#include "gimp-app-test-utils.h"
}
#include "paint/painter-mypaint-surface/paint-core.hpp"
using namespace GimpPainter::MyPaint;
static Gimp*gimp;
static const GimpPrecision precisions[]={
 GIMP_PRECISION_U8_LINEAR,GIMP_PRECISION_U8_NON_LINEAR,GIMP_PRECISION_U8_PERCEPTUAL,
 GIMP_PRECISION_U16_LINEAR,GIMP_PRECISION_U16_NON_LINEAR,GIMP_PRECISION_U16_PERCEPTUAL,
 GIMP_PRECISION_U32_LINEAR,GIMP_PRECISION_U32_NON_LINEAR,GIMP_PRECISION_U32_PERCEPTUAL,
 GIMP_PRECISION_HALF_LINEAR,GIMP_PRECISION_HALF_NON_LINEAR,GIMP_PRECISION_HALF_PERCEPTUAL,
 GIMP_PRECISION_FLOAT_LINEAR,GIMP_PRECISION_FLOAT_NON_LINEAR,GIMP_PRECISION_FLOAT_PERCEPTUAL,
 GIMP_PRECISION_DOUBLE_LINEAR,GIMP_PRECISION_DOUBLE_NON_LINEAR,GIMP_PRECISION_DOUBLE_PERCEPTUAL};
static std::vector<guchar> pixels(GimpDrawable*d){auto*b=gimp_drawable_get_buffer(d);auto*f=gegl_buffer_get_format(b);const auto*r=gegl_buffer_get_extent(b);std::vector<guchar>out(std::size_t(r->width)*r->height*babl_format_get_bytes_per_pixel(f));gegl_buffer_get(b,r,1,f,out.data(),GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);return out;}
static void stroke(PaintCore&core,GimpDrawable*d,int x=10){GimpCoords c=GIMP_COORDS_DEFAULT_VALUES;c.x=x;c.y=10;c.pressure=0;core.stroke_to(d,.01,c);c.pressure=.85;core.stroke_to(d,.05,c);for(int i=0;i<4;++i){c.x+=2;core.stroke_to(d,.01,c);}}
struct CancelDuring {PaintCore*core;bool fired=false;};
static void cancel_during_update(GimpDrawable*,gint,gint,gint,gint,gpointer data){auto*state=static_cast<CancelDuring*>(data);if(!state->fired){state->fired=true;state->core->cancel();}}
static void native_matrix()
{
 unsigned cells=0;
 for(auto precision:precisions)for(auto base:{GIMP_RGB,GIMP_GRAY})for(bool alpha:{false,true})for(bool custom:{false,true}){
  auto*image=gimp_image_new(gimp,32,24,base,precision);GError*error=nullptr;
  if(custom){auto*profile=base==GIMP_RGB?gimp_color_profile_new_rgb_adobe():gimp_color_profile_new_d50_gray_lab_trc();g_assert_true(gimp_image_set_color_profile(image,profile,&error));g_assert_no_error(error);g_object_unref(profile);}
  auto*format=gimp_image_get_layer_format(image,alpha);auto*layer=gimp_layer_new(image,32,24,format,"Native precision target",1,GIMP_LAYER_MODE_NORMAL);g_assert_true(gimp_image_add_layer(image,layer,nullptr,0,FALSE));auto*d=GIMP_DRAWABLE(layer);auto*buffer=gimp_drawable_get_buffer(d);
  g_assert_true(format==gegl_buffer_get_format(buffer));g_assert_cmpint(gimp_babl_format_get_precision(format),==,precision);g_assert_cmpint(gimp_babl_format_get_base_type(format),==,base);g_assert_cmpint(babl_format_has_alpha(format),==,alpha);
  auto*color=gegl_color_new("rgba(0.2,0.35,0.5,0.7)");gegl_buffer_set_color(buffer,nullptr,color);g_object_unref(color);auto before=pixels(d);gimp_image_undo_free(image);
  auto*options=GIMP_PAINT_OPTIONS(g_object_new(GIMP_TYPE_PAINT_OPTIONS,"gimp",gimp,nullptr));color=gegl_color_new("rgb(0.8,0.2,0.4)");gimp_context_set_foreground(GIMP_CONTEXT(options),color);g_object_unref(color);
  for(bool floating:{false,true}){
   Resource r;r.set_base_value(BRUSH_DABS_PER_SECOND,40);r.set_base_value(BRUSH_RADIUS_LOGARITHMIC,1.5);r.set_switch(BRUSH_NON_INCREMENTAL,floating);r.set_base_value(BRUSH_STROKE_OPACITY,.37);PaintCore core(options,r);
   stroke(core,d);g_assert_true(pixels(d)!=before);core.cancel();g_assert_true(pixels(d)==before);g_assert_cmpint(gimp_undo_stack_get_depth(gimp_image_get_undo_stack(image)),==,0);
   core.begin_batch(true,"Precision atomic stroke");stroke(core,d);core.next_segment();stroke(core,d,18);core.end_batch(true);const auto after=pixels(d);g_assert_true(after!=before);g_assert_cmpint(gimp_undo_stack_get_depth(gimp_image_get_undo_stack(image)),==,1);g_assert_true(gimp_image_undo(image));g_assert_true(pixels(d)==before);g_assert_true(gimp_image_redo(image));g_assert_true(pixels(d)==after);g_assert_true(gimp_image_undo(image));g_assert_true(pixels(d)==before);gimp_image_undo_free(image);
   core.begin_batch(true);stroke(core,d);core.next_segment();stroke(core,d,18);GimpCoords invalid=GIMP_COORDS_DEFAULT_VALUES;invalid.x=std::numeric_limits<double>::quiet_NaN();bool rejected=false;try{core.stroke_to(d,.01,invalid);}catch(const std::invalid_argument&){rejected=true;}g_assert_true(rejected);core.end_batch(false);g_assert_true(pixels(d)==before);g_assert_cmpint(gimp_undo_stack_get_depth(gimp_image_get_undo_stack(image)),==,0);g_assert_false(gimp_viewable_preview_is_frozen(GIMP_VIEWABLE(layer)));g_assert_true(gegl_buffer_get_format(gimp_drawable_get_buffer(d))==format);
   PaintCore callback_core(options,r);GimpCoords c=GIMP_COORDS_DEFAULT_VALUES;c.x=10;c.y=10;c.pressure=0;callback_core.stroke_to(d,.01,c);CancelDuring state{&callback_core};auto handler=g_signal_connect(d,"update",G_CALLBACK(cancel_during_update),&state);c.pressure=1;for(int step=0;step<4&&!state.fired;++step){c.x+=2;callback_core.stroke_to(d,.05,c);}g_signal_handler_disconnect(d,handler);g_assert_true(state.fired);g_assert_false(callback_core.active());g_assert_true(pixels(d)==before);g_assert_cmpint(gimp_undo_stack_get_depth(gimp_image_get_undo_stack(image)),==,0);
  }
  g_test_message("precision=%d base=%d alpha=%d profile=%s native=%s component=%d trc=%d",precision,base,alpha,custom?"custom":"builtin",babl_get_name(format),gimp_babl_format_get_component_type(format),gimp_babl_format_get_trc(format));g_object_unref(options);g_object_unref(image);++cells;
 }
 g_assert_cmpuint(cells,==,144);
}
static void profile_change_after_hover()
{
 auto*image=gimp_image_new(gimp,32,24,GIMP_RGB,GIMP_PRECISION_DOUBLE_NON_LINEAR);auto*layer=gimp_layer_new(image,32,24,gimp_image_get_layer_format(image,TRUE),"Profile boundary",1,GIMP_LAYER_MODE_NORMAL);g_assert_true(gimp_image_add_layer(image,layer,nullptr,0,FALSE));auto*d=GIMP_DRAWABLE(layer);gegl_buffer_clear(gimp_drawable_get_buffer(d),nullptr);
 auto*options=GIMP_PAINT_OPTIONS(g_object_new(GIMP_TYPE_PAINT_OPTIONS,"gimp",gimp,nullptr));auto*red=gegl_color_new("red");gimp_context_set_foreground(GIMP_CONTEXT(options),red);Resource r;r.set_base_value(BRUSH_DABS_PER_SECOND,40);r.set_base_value(BRUSH_RADIUS_LOGARITHMIC,1.5);PaintCore core(options,r);GimpCoords c=GIMP_COORDS_DEFAULT_VALUES;c.x=10;c.y=10;c.pressure=0;core.hover_to(d,.01,c);g_assert_false(core.active());
 auto*profile=gimp_color_profile_new_rgb_adobe();GError*error=nullptr;g_assert_true(gimp_image_assign_color_profile(image,profile,nullptr,&error));g_assert_no_error(error);auto*format=gegl_buffer_get_format(gimp_drawable_get_buffer(d));double expected[4];gegl_color_get_pixel(red,GeglSurface::evaluation_format(gimp_drawable_get_buffer(d)),expected);
 stroke(core,d);core.finish();std::vector<double>out(32*24*4);gegl_buffer_get(gimp_drawable_get_buffer(d),GEGL_RECTANGLE(0,0,32,24),1,format,out.data(),GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);bool painted=false;for(std::size_t i=0;i<out.size();i+=4)if(out[i+3]>.1){painted=true;g_assert_cmpfloat_with_epsilon(out[i],expected[0],2e-6);g_assert_cmpfloat_with_epsilon(out[i+1],expected[1],2e-6);g_assert_cmpfloat_with_epsilon(out[i+2],expected[2],2e-6);}g_assert_true(painted);g_object_unref(profile);g_object_unref(red);g_object_unref(options);g_object_unref(image);
}
int main(int argc,char**argv){g_test_init(&argc,&argv,nullptr);gimp_test_utils_set_gimp3_directory("GIMP_TESTING_ABS_TOP_SRCDIR","app/tests/gimpdir");gimp=gimp_init_for_testing();g_test_add_func("/painter-precision/native-144-matrix",native_matrix);g_test_add_func("/painter-precision/profile-change-after-hover",profile_change_after_hover);int result=g_test_run();gimp_test_utils_set_gimp3_directory("GIMP_TESTING_ABS_TOP_BUILDDIR","app/tests/gimpdir-output");gimp_exit(gimp,TRUE);return result;}
