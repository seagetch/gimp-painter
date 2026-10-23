/* SPDX-License-Identifier: GPL-3.0-or-later
 * Current stroke-start isolation contract, not historical lazy first-touch
 * parity under intervening external writes. Native transaction timing is tested
 * separately; this fixture exercises the standalone Surface session boundary.
 */
#include "paint/painter-mypaint-surface/gegl-surface.hpp"
using namespace GimpPainter::MyPaint;
namespace {
void fill(GeglBuffer* buffer, const GeglRectangle& rect, const char* value) {
  auto* color=gegl_color_new(value);
  gegl_buffer_set_color(buffer,&rect,color); g_object_unref(color);
}
std::vector<guchar> bytes(GeglBuffer* buffer, const GeglRectangle& rect) {
  const auto* format=gegl_buffer_get_format(buffer);
  std::vector<guchar> result(std::size_t(rect.width)*rect.height*babl_format_get_bytes_per_pixel(format));
  gegl_buffer_get(buffer,&rect,1,format,result.data(),GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
  return result;
}
void equal(GeglBuffer* left, GeglBuffer* right, const GeglRectangle& rect) {
  g_assert_true(bytes(left,rect)==bytes(right,rect));
}
void configure(GeglSurface& surface, bool nonincremental) {
  surface.set_non_incremental(nonincremental);
  surface.set_shape_provider([](float,float,float,float){return ShapeMask{3,3,std::vector<guchar>(9,255)};});
}
void sample(GeglSurface& surface, int x, int y, float red, float green, float blue) {
  float r,g,b,a; surface.get_color(x,y,2,&r,&g,&b,&a,1);
  g_assert_cmpfloat_with_epsilon(r,red,1e-6);
  g_assert_cmpfloat_with_epsilon(g,green,1e-6);
  g_assert_cmpfloat_with_epsilon(b,blue,1e-6);
  g_assert_cmpfloat_with_epsilon(a,1,1e-6);
}
void isolation(bool supplied) {
  for(bool wide:{false,true}) for(bool nonincremental:{false,true}) {
    g_test_message("supplied=%d wide=%d nonincremental=%d",supplied,wide,nonincremental);
    const auto* format=babl_format(wide?"R'G'B'A double":"R'G'B'A u8");
    GeglRectangle extent={0,0,1,1};
    auto* target=gegl_buffer_new(&extent,format);
    int tw=0,th=0; g_object_get(target,"tile-width",&tw,"tile-height",&th,nullptr);
    g_assert_cmpint(tw,>,8); g_assert_cmpint(th,>,8);
    extent={0,0,3*tw,2*th}; gegl_buffer_set_extent(target,&extent);
    fill(target,extent,"blue");
    auto* baseline=gegl_buffer_dup(target);
    auto* snapshot=supplied?gegl_buffer_dup(target):nullptr;
    const auto original=bytes(baseline,extent);
    auto* control=gegl_buffer_dup(target);
    {
      GeglSurface surface(target), reference(control);
      configure(surface,nonincremental); configure(reference,nonincremental);
      if(supplied) surface.begin_session_from(snapshot); else surface.begin_session();
      // A write covering all tiles precedes even the first dab. Sampling must
      // observe live pixels, while nonincremental composition uses start blue.
      fill(target,extent,"lime");
      if(!nonincremental) fill(control,extent,"lime");
      reference.begin_session();
      const int first=tw/2, later=2*tw+tw/2, y=th/2;
      sample(surface,first,y,0,1,0); sample(surface,later,y,0,1,0);
      for(int x:{first,later}) {
        if(x==later) {
          // This whole tile has never been dabbed. Changing it again must not
          // defer the baseline to first touch, even after another tile was used.
          const GeglRectangle tile={2*tw,0,tw,th};
          fill(target,tile,"yellow");
          if(!nonincremental) fill(control,tile,"yellow");
          sample(surface,x,y,1,1,0);
        }
        for(int dab=0;dab<2;++dab) {
          g_assert_true(surface.draw_dab(x,y,2,1,0,0,.5f,1));
          g_assert_true(reference.draw_dab(x,y,2,1,0,0,.5f,1));
          // Compare only the dab ROI: edits outside it deliberately differ.
          const GeglRectangle probe={x-2,y-2,3,3};
          equal(target,control,probe);
          g_assert_false(bytes(target,probe)==bytes(baseline,probe));
        }
      }
      if(snapshot) g_assert_true(bytes(snapshot,extent)==original);
      // Cancellation restores the bounding rectangle, including the gap
      // between dabs; all external changes outside that rectangle survive.
      auto* expected=gegl_buffer_dup(target);
      const auto dirty=surface.dirty();
      g_assert_cmpint(dirty.width,>,tw); g_assert_cmpint(dirty.height,<,th);
      gegl_buffer_copy(baseline,&dirty,GEGL_ABYSS_NONE,expected,&dirty);
      surface.cancel_session(); equal(target,expected,extent);
      sample(surface,first,y,0,0,1); sample(surface,later,y,0,0,1);
      sample(surface,first,th+th/2,0,1,0);
      if(snapshot) g_assert_true(bytes(snapshot,extent)==original);
      g_object_unref(expected); reference.end_session();
    }
    if(snapshot) g_object_unref(snapshot);
    g_object_unref(control); g_object_unref(baseline); g_object_unref(target);
  }
}
void default_snapshot() { isolation(false); }
void supplied_snapshot() { isolation(true); }
}
int main(int argc,char** argv) {
  g_test_init(&argc,&argv,nullptr); gegl_init(&argc,&argv);
  g_test_add_func("/snapshot-isolation/default",default_snapshot);
  g_test_add_func("/snapshot-isolation/supplied",supplied_snapshot);
  const int result=g_test_run(); gegl_exit(); return result;
}
