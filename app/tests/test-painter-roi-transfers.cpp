/* SPDX-License-Identifier: GPL-3.0-or-later
 * Exact Surface pixel-transfer accounting, NOT an allocation/RSS limit.
 * GEGL tiles, snapshots/cancel copies, resources and masks are not counted.
 * Sparse 2048-square targets exercise ordinary ROI behavior without full reads.
 */
#include "config.h"
#include <gtk/gtk.h>
#include <gegl.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>
extern "C" {
#include "core/core-types.h"
#include "widgets/widgets-types.h"
#include "core/gimp.h"
#include "core/gimpcontext.h"
#include "core/gimpbrush.h"
#include "core/gimpbrush-private.h"
#include "core/gimpbrushgenerated.h"
#include "core/gimpbrushpipe.h"
#include "core/gimppattern.h"
#include "core/gimptempbuf.h"
#include "tests.h"
#include "gimp-app-test-utils.h"
}
#include "paint/painter-mypaint-surface/gimp-resources.hpp"
#include "paint/painter-mypaint-surface/legacy-mask-transform.hpp"
#include "paint/painter-mypaint-surface/legacy-generated-mask.hpp"
using namespace GimpPainter::MyPaint;
static Gimp *gimp;
static std::vector<guchar> read_local(GeglBuffer *buffer, const GeglRectangle& rect)
{
  const auto *format=gegl_buffer_get_format(buffer);
  std::vector<guchar> bytes(std::size_t(rect.width)*rect.height*babl_format_get_bytes_per_pixel(format));
  gegl_buffer_get(buffer,&rect,1,format,bytes.data(),GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
  return bytes;
}
static void rectangle(const GeglRectangle& actual,const GeglRectangle& expected)
{
  g_assert_cmpint(actual.x,==,expected.x);g_assert_cmpint(actual.y,==,expected.y);
  g_assert_cmpint(actual.width,==,expected.width);g_assert_cmpint(actual.height,==,expected.height);
}
static void counters(const GeglSurface& surface,std::size_t read,std::size_t written)
{
  g_assert_cmpuint(surface.bytes_read(),==,read);g_assert_cmpuint(surface.bytes_written(),==,written);
}
// Selection extent is image-space; offsets map it to (30,31,4,3).
// Resource bounds come from the selected small real mask, not a radius cap.
static void run_case(int side,bool wide,bool floating,int selection,
                     GimpResources *resources=nullptr,const ShapeMask *mask=nullptr,GimpBrushPipe *pipe=nullptr)
{
  const auto *format=babl_format(wide?"RGBA double":"R'G'B'A u8");
  auto *buffer=gegl_buffer_new(GEGL_RECTANGLE(0,0,side,side),format);
  const GeglRectangle local={20,20,25,25},sentinel={side-3,side-3,1,1};
  const double blue[]={0,0,1,1};
  gegl_buffer_set(buffer,&sentinel,0,babl_format("RGBA double"),blue,GEGL_AUTO_ROWSTRIDE);
  const auto before=read_local(buffer,local),far_before=read_local(buffer,sentinel);
  g_assert_true(std::all_of(before.begin(),before.end(),[](guchar byte){return byte==0;}));
  GeglRectangle expected={28,28,9,9};
  if(mask) expected={int(std::floor(32-(mask->width+1)/2)),int(std::floor(32-(mask->height+1)/2)),mask->width,mask->height};
  GeglBuffer *selected=nullptr;
  if(selection) {
    const GeglRectangle image_rect={selection==2?107:37,selection==2?96:27,4,3};
    selected=gegl_buffer_new(&image_rect,babl_format(wide?"Y double":"Y u8"));
    // A sub-byte positive double selection would vanish if read as Y u8.
    double ones[12];std::fill(ones,ones+12,selection==3?1./1024:1.);
    gegl_buffer_set(selected,&image_rect,0,babl_format("Y double"),ones,GEGL_AUTO_ROWSTRIDE);
    const GeglRectangle selected_local={image_rect.x-7,image_rect.y+4,4,3};
    if(!gegl_rectangle_intersect(&expected,&expected,&selected_local))expected={0,0,0,0};
  }
  {
    GeglSurface surface(buffer);surface.set_non_incremental(floating);
    if(resources)resources->attach(surface);
    if(selected)surface.set_selection(selected,7,-4);
    unsigned notifications=0;
    surface.set_dirty_callback([&](const GeglRectangle&r){rectangle(r,expected);++notifications;});
    surface.begin_session();counters(surface,0,0);
    const std::size_t n=std::size_t(expected.width)*expected.height,b=wide?32:4,s=selection?(wide?8:1):0;
    const std::size_t draw_read=n*((floating?3:1)*b+s),draw_write=n*(floating?2:1)*b;
    const bool changed=surface.draw_dab(32,32,3,1,0,0,1,.5);
    g_assert_cmpint(changed,==,n!=0);counters(surface,draw_read,draw_write);
    if(n)rectangle(surface.dirty(),expected);else g_assert_cmpint(surface.dirty().width,==,0);
    g_assert_cmpuint(notifications,==,n?1:0);
    const auto after=read_local(buffer,local);
    g_assert_cmpint(after!=before,==,n!=0);
    // Every inspected pixel outside the intersection must remain unchanged.
    for(int y=local.y;y<local.y+local.height;++y)for(int x=local.x;x<local.x+local.width;++x)
      if(x<expected.x||y<expected.y||x>=expected.x+expected.width||y>=expected.y+expected.height) {
        const std::size_t i=((y-local.y)*local.width+x-local.x)*b;
        g_assert_cmpmem(before.data()+i,b,after.data()+i,b);
      }
    g_assert_true(read_local(buffer,sentinel)==far_before);
    float red,green,blue_value,alpha;
    if(pipe)pipe->current=pipe->brushes[0];
    surface.get_color(32,32,3,&red,&green,&blue_value,&alpha,.5);
    counters(surface,draw_read+n*(b+s),draw_write);
    if(n){g_assert_cmpfloat(red,>,.99);g_assert_cmpfloat(alpha,>,0);}
    g_assert_cmpuint(notifications,==,n?1:0);
    surface.cancel_session();counters(surface,draw_read+n*(b+s),draw_write);
    g_assert_true(read_local(buffer,local)==before);g_assert_true(read_local(buffer,sentinel)==far_before);
    g_assert_cmpuint(notifications,==,n?2:0);
    surface.begin_session();counters(surface,0,0);g_assert_cmpint(surface.dirty().width,==,0);
    surface.end_session();
  }
  if(selected)g_object_unref(selected);
  g_object_unref(buffer);
}
static void ellipse_matrix()
{
  for(int side:{128,2048})for(bool wide:{false,true})for(bool floating:{false,true})for(int selection:{0,1,2})
    run_case(side,wide,floating,selection);
}
static void native_selection_precision()
{
  for(int side:{128,2048})for(bool floating:{false,true})run_case(side,true,floating,3);
}
static GimpBrush *bitmap(int width,int height)
{
  auto *brush=GIMP_BRUSH(g_object_new(GIMP_TYPE_BRUSH,"name","ROI bitmap",nullptr));
  brush->priv->mask=gimp_temp_buf_new(width,height,babl_format("Y u8"));
  std::memset(gimp_temp_buf_get_data(brush->priv->mask),255,width*height);return brush;
}
static void resource_matrix()
{
  // Geometry helpers supply the actual small mask dimensions. This is a transfer
  // regression, not an independent geometry oracle or a pipe-order parity test.
  for(int kind:{0,1,2,3}) {
    auto *context=gimp_context_new(gimp,"ROI resources",nullptr);
    GimpBrush *brush=nullptr;ShapeMask expected;
    if(kind==1) {
      brush=GIMP_BRUSH(gimp_brush_generated_new("ROI generated",GIMP_BRUSH_GENERATED_CIRCLE,3,2,.8,1,0));
      const auto original=legacy_generated_dimensions(GeneratedShape::Circle,3,2,.8,1,0);
      const float scale=6.f/std::max(original.first,original.second);
      expected=generate_legacy_mask(GeneratedShape::Circle,3*scale,2,.8*.5,1,0);
    }else {
      int width=5,height=3;
      if(kind>=2) {
        auto *pipe=GIMP_BRUSH_PIPE(g_object_new(GIMP_TYPE_BRUSH_PIPE,"name","ROI mixed pipe",nullptr));
        pipe->n_brushes=2;pipe->brushes=g_new0(GimpBrush*,2);
        pipe->brushes[0]=bitmap(5,3);pipe->brushes[1]=bitmap(9,7);
        g_assert_true(gimp_brush_pipe_set_params(pipe,nullptr));
        pipe->select[0]=PIPE_SELECT_CONSTANT;pipe->index[0]=kind-2;pipe->current=pipe->brushes[0];
        GIMP_BRUSH(pipe)->priv->mask=pipe->brushes[0]->priv->mask;brush=GIMP_BRUSH(pipe);
        if(kind==3){width=9;height=7;}
      }else brush=bitmap(width,height);
      expected=transform_bitmap_mask(ShapeMask{width,height,std::vector<guchar>(width*height,255)},6.f/5,0,0,.5);
    }
    if(kind==3) {
      const auto first=transform_bitmap_mask(ShapeMask{5,3,std::vector<guchar>(15,255)},6.f/5,0,0,.5);
      g_assert_cmpint(expected.width,>,first.width);g_assert_cmpint(expected.height,>,first.height);
    }
    auto *paper=GIMP_PATTERN(g_object_new(GIMP_TYPE_PATTERN,"name","ROI paper",nullptr));
    paper->mask=gimp_temp_buf_new(3,2,babl_format("R'G'B' u8"));
    auto *pixels=gimp_temp_buf_get_data(paper->mask);
    for(int i=0;i<6;++i){pixels[3*i]=192+i*10;pixels[3*i+1]=0;pixels[3*i+2]=255;}
    gimp_context_set_brush(context,brush);gimp_context_set_pattern(context,paper);
    Resource settings;settings.set_switch(BRUSH_USE_GIMP_BRUSHMARK,true);settings.set_switch(BRUSH_USE_GIMP_TEXTURE,true);
    {
      GimpResources resources(context,settings);
      for(int side:{128,2048})for(bool wide:{false,true})for(bool floating:{false,true})for(int selection:{0,1,2}) {
        // Reset root/current before each operation: native pipe mask lookup uses
        // current before child selection. Keep this accounting fixture bounded.
        if(kind>=2)GIMP_BRUSH_PIPE(brush)->current=GIMP_BRUSH_PIPE(brush)->brushes[0];
        run_case(side,wide,floating,selection,&resources,&expected,kind>=2?GIMP_BRUSH_PIPE(brush):nullptr);
      }
    }
    g_object_unref(paper);g_object_unref(brush);g_object_unref(context);
  }
}
int main(int argc,char **argv)
{
  g_test_init(&argc,&argv,nullptr);
  gimp_test_utils_set_gimp3_directory("GIMP_TESTING_ABS_TOP_SRCDIR","app/tests/gimpdir");gimp=gimp_init_for_testing();
  g_test_add_func("/painter-roi-transfers/ellipse",ellipse_matrix);
  g_test_add_func("/painter-roi-transfers/real-resources",resource_matrix);
  g_test_add_func("/painter-roi-transfers/native-selection-precision",native_selection_precision);
  const int result=g_test_run();
  gimp_test_utils_set_gimp3_directory("GIMP_TESTING_ABS_TOP_BUILDDIR","app/tests/gimpdir-output");gimp_exit(gimp,TRUE);return result;
}
