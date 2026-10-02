/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Defined Gray extension invariants against current RGB/RGBA equations.
 * These synthetic paired sessions do not claim an old Gray-alpha oracle. */
#include <cstdio>
#include <vector>
#include <cstring>
#include <cmath>
extern "C" {
#include "config.h"
#include <gtk/gtk.h>
#include <gegl.h>
#include "libgimpbase/gimpbase.h"
#include "libgimpmath/gimpmath.h"
#include "core/core-types.h"
#include "widgets/widgets-types.h"
#include "core/gimp.h"
#include "core/gimpcontext.h"
#include "core/gimpimage.h"
#include "core/gimpimage-undo.h"
#include "core/gimpundostack.h"
#include "core/gimplayer.h"
#include "core/gimplayer-new.h"
#include "core/gimpbrush.h"
#include "core/gimpbrush-private.h"
#include "core/gimptempbuf.h"
#include "paint/gimppaintoptions.h"
#include "core/gimppattern.h"
#include "core/gimpchannel.h"
#include "core/gimpchannel-select.h"
#include "tests.h"
#include "gimp-app-test-utils.h"
}
#include "paint/painter-mypaint-surface/paint-core.hpp"
using namespace GimpPainter::MyPaint;
static Gimp*test_gimp;
static constexpr int width=130,height=96;
static std::vector<guchar> pixels(GimpDrawable*d)
{
 auto*buffer=gimp_drawable_get_buffer(d);const auto*format=gegl_buffer_get_format(buffer);
 std::vector<guchar> p(width*height*babl_format_get_bytes_per_pixel(format));
 gegl_buffer_get(buffer,GEGL_RECTANGLE(0,0,width,height),1,format,p.data(),GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);return p;
}
static std::vector<guchar> capture(Gimp*gimp,int scenario,bool gray,bool alpha)
{
  const int blend=scenario/32;const int channels=(gray?1:3)+(alpha?1:0);
  const auto*format=babl_format(gray?(alpha?"Y'A u8":"Y' u8"):(alpha?"R'G'B'A u8":"R'G'B' u8"));
  const bool floating=scenario&1,shape=scenario&2,paper=scenario&4,selection=scenario&8,smudge=scenario&16;
  auto*image=gimp_image_new(gimp,150,110,gray?GIMP_GRAY:GIMP_RGB,GIMP_PRECISION_U8_NON_LINEAR);
  auto*layer=gimp_layer_new(image,width,height,format,"session-target",1,GIMP_LAYER_MODE_NORMAL);
  g_assert(gimp_image_add_layer(image,layer,nullptr,0,FALSE));gimp_layer_set_lock_alpha(layer,blend==2,FALSE);gimp_item_set_offset(GIMP_ITEM(layer),7,3);
  auto*drawable=GIMP_DRAWABLE(layer);std::vector<guchar> initial(width*height*channels),result;
  for(int y=0;y<height;++y)for(int x=0;x<width;++x){int i=(y*width+x)*channels;guchar v=(x*19+y*7)%256;initial[i]=v;if(!gray)initial[i+1]=initial[i+2]=v;if(alpha)initial[i+channels-1]=(x+y)%3?180:0;}
  gegl_buffer_set(gimp_drawable_get_buffer(drawable),GEGL_RECTANGLE(0,0,width,height),0,format,initial.data(),GEGL_AUTO_ROWSTRIDE);
  if(selection)gimp_channel_select_rectangle(gimp_image_get_mask(image),70,0,80,110,GIMP_CHANNEL_OP_REPLACE,FALSE,0,0,FALSE);
  auto*options=GIMP_PAINT_OPTIONS(g_object_new(GIMP_TYPE_PAINT_OPTIONS,"gimp",gimp,nullptr));
  double foreground[4]={.8,.8,.8,1},background[4]={.1,.1,.1,1};
  auto*fg=gegl_color_new(nullptr);auto*bg=gegl_color_new(nullptr);
  gegl_color_set_pixel(fg,babl_format("R'G'B'A double"),foreground);gegl_color_set_pixel(bg,babl_format("R'G'B'A double"),background);
  gimp_context_set_foreground(GIMP_CONTEXT(options),fg);gimp_context_set_background(GIMP_CONTEXT(options),bg);g_object_unref(fg);g_object_unref(bg);
  Resource settings=Resource::decode("{\"version\":3,\"settings\":{},\"switches\":{},\"texts\":{}}");
  settings.set_base_value(BRUSH_DABS_PER_SECOND,40);settings.set_base_value(BRUSH_RADIUS_LOGARITHMIC,2.3);
  settings.set_switch(BRUSH_NON_INCREMENTAL,floating);settings.set_base_value(BRUSH_STROKE_OPACITY,smudge?.37:1.0);
  settings.set_base_value(BRUSH_ERASER,blend==1?.65:0);settings.set_base_value(BRUSH_LOCK_ALPHA,blend==2?.7:0);
  settings.set_base_value(BRUSH_SMUDGE,smudge?.8:0);settings.set_base_value(BRUSH_SMUDGE_LENGTH,.65);
  settings.set_switch(BRUSH_USE_GIMP_BRUSHMARK,shape);settings.set_switch(BRUSH_USE_GIMP_TEXTURE,paper);
  GimpBrush*brush=nullptr;GimpPattern*pattern=nullptr;
  if(shape){brush=GIMP_BRUSH(g_object_new(GIMP_TYPE_BRUSH,"name","session-bitmap",nullptr));brush->priv->mask=gimp_temp_buf_new(5,3,babl_format("Y u8"));const guchar mask[]={0,30,90,160,255,12,60,120,180,240,255,190,140,70,0};std::memcpy(gimp_temp_buf_get_data(brush->priv->mask),mask,sizeof mask);gimp_context_set_brush(GIMP_CONTEXT(options),brush);}
  if(paper){pattern=GIMP_PATTERN(g_object_new(GIMP_TYPE_PATTERN,"name","session-paper",nullptr));pattern->mask=gimp_temp_buf_new(3,2,babl_format("R'G'B' u8"));const guchar texture[]={20,250,10,100,0,50,240,2,180,200,12,33,64,25,70,128,100,5};std::memcpy(gimp_temp_buf_get_data(pattern->mask),texture,sizeof texture);gimp_context_set_pattern(GIMP_CONTEXT(options),pattern);}
  gimp_image_undo_free(image);
  {
    PaintCore core(options,settings);
    GimpCoords coords=GIMP_COORDS_DEFAULT_VALUES;coords.x=60;coords.y=60;coords.pressure=0;
    core.stroke_to(drawable,.01,coords);coords.pressure=.85;core.stroke_to(drawable,.075,coords);
    for(int i=0;i<16;++i){coords.x+=2;coords.y+=.75;coords.pressure=i%3?.85:.45;core.stroke_to(drawable,.012,coords);}
    core.finish();

    auto after=pixels(drawable);
    gboolean undo=gimp_image_undo(image);gboolean before_equal=pixels(drawable)==initial;

    gboolean redo=gimp_image_redo(image);gboolean after_equal=pixels(drawable)==after;

    g_assert_true(undo);g_assert_true(before_equal);g_assert_true(redo);g_assert_true(after_equal);
    g_assert_cmpint(gimp_undo_stack_get_depth(gimp_image_get_undo_stack(image)),==,1);
    for(int i=0;i<width*height;++i){result.push_back(after[i*channels]);result.push_back(alpha?after[i*channels+channels-1]:255);}

  }
  if(brush)g_object_unref(brush);
  if(pattern)g_object_unref(pattern);
  g_object_unref(options);g_object_unref(image);return result;
}
static void paired_native_sessions()
{
  int count=0;
  for(bool alpha:{false,true})for(int i=0;i<96;++i){
    auto expected=capture(test_gimp,i,false,alpha);
    auto actual=capture(test_gimp,i,true,alpha);
    if(expected!=actual)g_error("Gray equation invariant failed: alpha=%d scene=%d",alpha,i);
    ++count;
  }
  g_test_message("%d paired native full-session/Undo/Redo comparisons",count);
}
static void transparent_defined_behavior()
{
  for(bool lock:{false,true}){
    auto*buffer=gegl_buffer_new(GEGL_RECTANGLE(0,0,1,1),babl_format("Y'A u8"));
    guchar pixel[2]={123,guchar(lock?0:180)};
    gegl_buffer_set(buffer,GEGL_RECTANGLE(0,0,1,1),0,babl_format("Y'A u8"),pixel,GEGL_AUTO_ROWSTRIDE);
    GeglSurface surface(buffer);surface.begin_session();
    g_assert_true(surface.draw_dab(.5,.5,4,.8,.2,.5,1,1,lock?1:0,1,0,lock?1:0,0,0,1));
    gegl_buffer_get(buffer,GEGL_RECTANGLE(0,0,1,1),1,babl_format("Y'A u8"),pixel,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
    g_assert_cmpint(pixel[0],==,0);g_assert_cmpint(pixel[1],==,0);
    float r,g,b,a;surface.get_color(.5,.5,4,&r,&g,&b,&a,1,1,0,0,1);
    g_assert_true(std::isfinite(r)&&std::isfinite(g)&&std::isfinite(b)&&std::isfinite(a));g_assert_cmpfloat(a,==,0);
    surface.cancel_session();gegl_buffer_get(buffer,GEGL_RECTANGLE(0,0,1,1),1,babl_format("Y'A u8"),pixel,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
    g_assert_cmpint(pixel[0],==,123);g_assert_cmpint(pixel[1],==,lock?0:180);g_object_unref(buffer);
  }
}
int main(int argc,char**argv)
{
  g_test_init(&argc,&argv,nullptr);gimp_test_utils_set_gimp3_directory("GIMP_TESTING_ABS_TOP_SRCDIR","app/tests/gimpdir");test_gimp=gimp_init_for_testing();
  g_test_add_func("/painter-gray-extension/paired-native-sessions",paired_native_sessions);
  g_test_add_func("/painter-gray-extension/transparent-defined",transparent_defined_behavior);
  const int result=g_test_run();gimp_test_utils_set_gimp3_directory("GIMP_TESTING_ABS_TOP_BUILDDIR","app/tests/gimpdir-output");gimp_exit(test_gimp,TRUE);return result;
}
