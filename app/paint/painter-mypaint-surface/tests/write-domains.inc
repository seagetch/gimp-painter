/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "paint/painter-mypaint-surface/buffer-read.hpp"
#include <string>
namespace WriteDomains {
using namespace GimpPainter::MyPaint;
static const GeglRectangle backing = {-9,-8,17,15}, target_extent = {-6,-5,11,9};
static bool member(const GeglRectangle& r,int x,int y)
{return x>=r.x&&y>=r.y&&std::int64_t(x)<std::int64_t(r.x)+r.width&&std::int64_t(y)<std::int64_t(r.y)+r.height;}
static void changed(GeglBuffer*,const GeglRectangle* rect,gpointer data)
{static_cast<std::vector<GeglRectangle>*>(data)->push_back(*rect);}
static void format_grid()
{
  unsigned configurations=0, operations=0;
  for(const char*space:{"sRGB","Rec2020"})
    for(const char*model:{"RGB","RGBA","R'G'B'","R'G'B'A","R~G~B~","R~G~B~A","Y","YA","Y'","Y'A","Y~","Y~A"})
      for(const char*type:{"u8","u16","u32","half","float","double"})
        for(bool floating:{false,true})
          {
            const auto*format=babl_format_with_space((std::string(model)+" "+type).c_str(),babl_space(space));
            const int bpp=babl_format_get_bytes_per_pixel(format);
            auto*parent=gegl_buffer_new(&backing,format);
            auto*target=gegl_buffer_create_sub_buffer(parent,&target_extent);
            auto*blue=gegl_color_new("blue");
            GeglSurface surface(target);surface.set_non_incremental(floating);surface.set_stroke_opacity(.5f);
            surface.set_shape_provider([](float,float,float,float){return ShapeMask{5,3,std::vector<guchar>(15,255)};});
            std::vector<GeglRectangle> signals,notices;
            const gulong handler=gegl_buffer_signal_connect(target,"changed",G_CALLBACK(changed),&signals);
            surface.set_dirty_callback([&](const GeglRectangle&r){notices.push_back(r);});
            for(bool masked:{false,true})
              for(const auto&center:{std::pair<int,int>{-6,-5},{-4,-3},{0,0},{5,4},{7,4},{0,6},{-20,-20},{20,20}})
                {
                  gegl_buffer_set_color(parent,&backing,blue);
                  const auto before=BufferRead::packed(parent,backing);
                  const GeglRectangle local_mask={-4,-3,6,5},image_mask={7,-10,6,5};
                  auto*mask=masked?gegl_buffer_new(&image_mask,babl_format("Y u8")):nullptr;
                  if(mask){std::vector<guchar>full(30,255);gegl_buffer_set(mask,&image_mask,0,babl_format("Y u8"),full.data(),GEGL_AUTO_ROWSTRIDE);}
                  surface.set_selection(mask,11,-7);if(mask)g_object_unref(mask);
                  surface.begin_session();signals.clear();notices.clear();
                  const GeglRectangle dab={center.first-3,center.second-2,5,3};
                  int left=100,top=100,right=-100,bottom=-100;unsigned expected_pixels=0;
                  // Independent membership oracle, not the production intersection helper.
                  for(int y=backing.y;y<backing.y+backing.height;++y)
                    for(int x=backing.x;x<backing.x+backing.width;++x)
                      if(member(target_extent,x,y)&&member(dab,x,y)&&(!masked||member(local_mask,x,y)))
                        {++expected_pixels;left=std::min(left,x);top=std::min(top,y);right=std::max(right,x+1);bottom=std::max(bottom,y+1);}
                  const bool drew=surface.draw_dab(center.first,center.second,2,1,0,0,1,1);
                  g_assert_cmpint(drew,==,expected_pixels!=0);
                  const auto after=BufferRead::packed(parent,backing);unsigned actual_pixels=0;
                  for(int y=backing.y;y<backing.y+backing.height;++y)
                    for(int x=backing.x;x<backing.x+backing.width;++x)
                      {
                        const auto i=((y-backing.y)*backing.width+x-backing.x)*bpp;
                        const bool allowed=member(target_extent,x,y)&&member(dab,x,y)&&(!masked||member(local_mask,x,y));
                        const bool differs=std::memcmp(before.data()+i,after.data()+i,bpp)!=0;
                        g_assert_cmpint(differs,==,allowed);actual_pixels+=differs;
                      }
                  g_assert_cmpuint(actual_pixels,==,expected_pixels);
                  g_assert_cmpuint(notices.size(),==,expected_pixels?1:0);
                  if(expected_pixels)
                    {
                      const GeglRectangle expected={left,top,right-left,bottom-top};
                      g_assert_true(gegl_rectangle_equal(&surface.dirty(),&expected));
                      g_assert_true(gegl_rectangle_equal(&notices[0],&expected));
                      // Real GEGL write notification, not only the Surface observer.
                      g_assert_false(signals.empty());
                      for(const auto&r:signals)g_assert_true(gegl_rectangle_equal(&r,&expected));
                      g_assert_cmpuint(surface.bytes_written(),>=,expected_pixels*bpp);
                    }
                  else
                    {g_assert_cmpint(surface.dirty().width,==,0);g_assert_cmpuint(surface.bytes_written(),==,0);g_assert_true(signals.empty());}
                  const auto writes=surface.bytes_written();const auto events=signals.size();
                  float red,green,blue_value,alpha;surface.get_color(center.first,center.second,2,&red,&green,&blue_value,&alpha,1);
                  g_assert_true(BufferRead::packed(parent,backing)==after);
                  g_assert_cmpuint(surface.bytes_written(),==,writes);g_assert_cmpuint(signals.size(),==,events);
                  surface.end_session();g_assert_true(BufferRead::packed(parent,backing)==after);
                  ++operations;
                }
            g_signal_handler_disconnect(target,handler);g_object_unref(blue);g_object_unref(target);g_object_unref(parent);++configurations;
          }
  g_assert_cmpuint(configurations,==,288);g_assert_cmpuint(operations,==,4608);
  g_test_message("144 formats x 2 accumulation modes x 16 edge/mask cases = 4608 actual producer writes");
}
static void coalesced_dirty_publication()
{
  for(const char*name:{"R'G'B'A u8","Y'A u8","RGBA double","YA double"})
    for(bool floating:{false,true})
      {
        const auto*format=babl_format(name);const int bpp=babl_format_get_bytes_per_pixel(format);
        auto*parent=gegl_buffer_new(&backing,format);auto*target=gegl_buffer_create_sub_buffer(parent,&target_extent);
        auto*blue=gegl_color_new("blue");gegl_buffer_set_color(parent,&backing,blue);g_object_unref(blue);
        const auto before=BufferRead::packed(parent,backing);
        GeglSurface surface(target);surface.set_non_incremental(floating);surface.set_stroke_opacity(.5f);
        surface.set_shape_provider([](float,float,float,float){return ShapeMask{3,3,std::vector<guchar>(9,255)};});
        std::vector<GeglRectangle> signals,notices;
        const auto id=gegl_buffer_signal_connect(target,"changed",G_CALLBACK(changed),&signals);
        const GeglRectangle dabs[]={{-6,-5,2,2},{-5,-4,3,3},{2,1,3,3}};
        const GeglRectangle unions[]={{-6,-5,2,2},{-6,-5,4,4},{-6,-5,11,9}};
        surface.set_dirty_callback([&](const GeglRectangle&r){
          const auto n=notices.size();g_assert_cmpuint(n,<,3);
          g_assert_true(gegl_rectangle_equal(&r,&dabs[n]));
          g_assert_true(gegl_rectangle_equal(&surface.dirty(),&unions[n]));
          notices.push_back(r);
        });
        surface.begin_session();gegl_buffer_freeze_changed(target);
        const std::pair<int,int> centers[]={{-5,-4},{-3,-2},{4,3}};
        for(const auto&center:centers)g_assert_true(surface.draw_dab(center.first,center.second,2,1,0,0,.7f,1));
        g_assert_true(signals.empty());g_assert_cmpuint(notices.size(),==,3);
        gegl_buffer_thaw_changed(target);g_assert_cmpuint(signals.size(),==,1);
        g_assert_true(gegl_rectangle_equal(&signals[0],&unions[2]));
        const auto after=BufferRead::packed(parent,backing);
        for(int y=backing.y;y<backing.y+backing.height;++y)
          for(int x=backing.x;x<backing.x+backing.width;++x)
            {
              bool written=false;for(const auto&dab:dabs)written=written||member(dab,x,y);
              const auto i=((y-backing.y)*backing.width+x-backing.x)*bpp;
              g_assert_cmpint(std::memcmp(before.data()+i,after.data()+i,bpp)!=0,==,written);
            }
        // The conservative union includes the untouched gap, not a promise
        // that every pixel of dirty() changed. Empty dabs do not enlarge it.
        g_assert_false(surface.draw_dab(50,50,2,1,0,0,1,1));
        g_assert_cmpuint(signals.size(),==,1);g_assert_cmpuint(notices.size(),==,3);
        surface.end_session();g_assert_true(gegl_rectangle_equal(&surface.dirty(),&unions[2]));
        surface.begin_session();g_assert_cmpint(surface.dirty().width,==,0);g_assert_cmpuint(surface.bytes_written(),==,0);
        surface.end_session();g_assert_true(BufferRead::packed(parent,backing)==after);
        g_signal_handler_disconnect(target,id);g_object_unref(target);g_object_unref(parent);
      }
}

}
