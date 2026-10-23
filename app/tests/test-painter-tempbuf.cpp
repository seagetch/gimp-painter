/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gegl.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <cstring>
#include <string>
#include <vector>
extern "C" {
#include "core/core-types.h"
#include "core/gimptempbuf.h"
}
#include "paint/painter-mypaint-surface/gegl-surface.hpp"
using namespace GimpPainter::MyPaint;
namespace {
void native_format_copy_and_borrow()
{
 const auto baseline=gimp_temp_buf_get_total_memsize();unsigned formats=0;
 for(const char*space:{"sRGB","Rec2020"})for(const char*model:{"RGB","RGBA","R'G'B'","R'G'B'A","R~G~B~","R~G~B~A","Y","YA","Y'","Y'A","Y~","Y~A"})for(const char*type:{"u8","u16","u32","half","float","double"})
 {
  const auto*format=babl_format_with_space((std::string(model)+" "+type).c_str(),babl_space(space));
  auto*temp=gimp_temp_buf_new(7,3,format);const auto size=gimp_temp_buf_get_data_size(temp);
  g_assert_cmpuint(size,==,21*babl_format_get_bytes_per_pixel(format));
  std::vector<guchar>expected(size);for(std::size_t i=0;i<size;++i)expected[i]=1+i%251;
  std::memcpy(gimp_temp_buf_get_data(temp),expected.data(),size);
  for(const Babl*requested:{format,static_cast<const Babl*>(nullptr)})
   {auto*data=gimp_temp_buf_lock(temp,requested,GEGL_ACCESS_READ);g_assert_true(data==gimp_temp_buf_get_data(temp));g_assert_cmpmem(data,size,expected.data(),size);gimp_temp_buf_unlock(temp,data);}
  auto*copy=gimp_temp_buf_copy(temp);g_assert_true(gimp_temp_buf_get_format(copy)==format);
  g_assert_true(gimp_temp_buf_get_data(copy)!=gimp_temp_buf_get_data(temp));
  auto*alias=gimp_temp_buf_create_buffer(temp);g_assert_true(gimp_gegl_buffer_get_temp_buf(alias)==temp);g_assert_true(gegl_buffer_get_format(alias)==format);
  auto*held=gimp_temp_buf_ref(temp);gimp_temp_buf_unref(temp);gimp_temp_buf_data_clear(held);gimp_temp_buf_unref(held);
  std::vector<guchar>zeros(size,0),read(size,0xff);gegl_buffer_get(alias,GEGL_RECTANGLE(0,0,7,3),1,format,read.data(),GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);g_assert_true(read==zeros);
  g_assert_cmpmem(gimp_temp_buf_get_data(copy),size,expected.data(),size);g_object_unref(alias);
  g_assert_cmpmem(gimp_temp_buf_get_data(copy),size,expected.data(),size);gimp_temp_buf_unref(copy);
  g_assert_cmpuint(gimp_temp_buf_get_total_memsize(),==,baseline);++formats;
 }
 g_assert_cmpuint(formats,==,144);
}
void converted_lock_access_modes()
{
 const auto baseline=gimp_temp_buf_get_total_memsize();
 for(const char*space:{"sRGB","Rec2020"})for(const char*model:{"Y'","Y'A","R'G'B'","R'G'B'A"})
 {
  const auto*u8=babl_format_with_space((std::string(model)+" u8").c_str(),babl_space(space));
  const auto*u16=babl_format_with_space((std::string(model)+" u16").c_str(),babl_space(space));
  auto*temp=gimp_temp_buf_new(7,3,u8);const auto size=gimp_temp_buf_get_data_size(temp);
  std::vector<guchar>original(size);for(std::size_t i=0;i<size;++i)original[i]=(i*37+19)%256;
  std::memcpy(gimp_temp_buf_get_data(temp),original.data(),size);
  auto*read=static_cast<guint16*>(gimp_temp_buf_lock(temp,u16,GEGL_ACCESS_READ));g_assert_true(static_cast<void*>(read)!=gimp_temp_buf_get_data(temp));
  for(std::size_t i=0;i<size;++i)g_assert_cmpuint(read[i],==,unsigned(original[i])*257);
  std::memset(read,0,size*2);gimp_temp_buf_unlock(temp,read);
  g_assert_cmpmem(gimp_temp_buf_get_data(temp),size,original.data(),size); // READ scratch never writes back.
  auto*both=static_cast<guint16*>(gimp_temp_buf_lock(temp,u16,GEGL_ACCESS_READWRITE));
  for(std::size_t i=0;i<size;++i){g_assert_cmpuint(both[i],==,unsigned(original[i])*257);both[i]=(255-original[i])*257;}
  gimp_temp_buf_unlock(temp,both);
  for(std::size_t i=0;i<size;++i)g_assert_cmpuint(gimp_temp_buf_get_data(temp)[i],==,255-original[i]);
  auto*write=static_cast<guint16*>(gimp_temp_buf_lock(temp,u16,GEGL_ACCESS_WRITE));
  // WRITE-only scratch is unspecified before initialization; never read it.
  for(std::size_t i=0;i<size;++i)write[i]=(i%2)*65535;
  gimp_temp_buf_unlock(temp,write);
  for(std::size_t i=0;i<size;++i)g_assert_cmpuint(gimp_temp_buf_get_data(temp)[i],==,(i%2)*255);
  g_assert_true(gimp_temp_buf_get_format(temp)==u8);gimp_temp_buf_unref(temp);
 }
 g_assert_cmpuint(gimp_temp_buf_get_total_memsize(),==,baseline);
}
void pixbuf_gone(guchar*,gpointer data){++*static_cast<unsigned*>(data);}
void padded_pixbuf_import_export()
{
 const auto baseline=gimp_temp_buf_get_total_memsize();
 for(bool alpha:{false,true})
 {
  const int channels=alpha?4:3,width=13,height=7,stride=width*channels+11;
  std::vector<guchar>pixels(stride*height,0xa5);
  for(int y=0;y<height;++y)for(int x=0;x<width;++x)for(int c=0;c<channels;++c)pixels[y*stride+x*channels+c]=1+(y*47+x*13+c*19)%251;
  unsigned destroyed=0;auto*parent=gdk_pixbuf_new_from_data(pixels.data(),GDK_COLORSPACE_RGB,alpha,8,width,height,stride,pixbuf_gone,&destroyed);
  auto*child=gdk_pixbuf_new_subpixbuf(parent,2,1,5,4);g_object_unref(parent);g_assert_cmpuint(destroyed,==,0);
  auto*temp=gimp_temp_buf_new_from_pixbuf(child,nullptr);g_assert_cmpint(gimp_temp_buf_get_width(temp),==,5);g_assert_cmpint(gimp_temp_buf_get_height(temp),==,4);
  std::vector<guchar>expected(5*4*channels);
  for(int y=0;y<4;++y)std::memcpy(expected.data()+y*5*channels,pixels.data()+(y+1)*stride+2*channels,5*channels);
  g_assert_cmpmem(gimp_temp_buf_get_data(temp),expected.size(),expected.data(),expected.size());
  g_object_unref(child);g_assert_cmpuint(destroyed,==,1);
  // Import is an owned packed copy, not an alias to the padded pixbuf.
  std::fill(pixels.begin(),pixels.end(),0);auto*exported=gimp_temp_buf_create_pixbuf(temp);
  const int output_stride=gdk_pixbuf_get_rowstride(exported);g_assert_cmpint(output_stride,>=,5*channels);if(!alpha)g_assert_cmpint(output_stride,>,5*channels);
  gimp_temp_buf_data_clear(temp);gimp_temp_buf_unref(temp);
  for(int y=0;y<4;++y)g_assert_cmpmem(gdk_pixbuf_get_pixels(exported)+y*output_stride,5*channels,expected.data()+y*5*channels,5*channels);
  g_object_unref(exported);g_assert_cmpuint(gimp_temp_buf_get_total_memsize(),==,baseline);
 }
}
void explicit_pixbuf_conversion()
{
 const auto baseline=gimp_temp_buf_get_total_memsize();
 for(bool alpha:{false,true})
 {
  const int channels=alpha?2:1;const auto*format=babl_format(alpha?"Y'A u16":"Y' u16");
  auto*temp=gimp_temp_buf_new(5,3,format);auto*data=reinterpret_cast<guint16*>(gimp_temp_buf_get_data(temp));
  for(int y=0;y<3;++y)for(int x=0;x<5;++x){data[(y*5+x)*channels]=((x+y)%2)*65535;if(alpha)data[(y*5+x)*channels+1]=(x%2)*65535;}
  auto*pixbuf=gimp_temp_buf_create_pixbuf(temp);gimp_temp_buf_unref(temp);
  const int out_channels=alpha?4:3,stride=gdk_pixbuf_get_rowstride(pixbuf);const auto*p=gdk_pixbuf_get_pixels(pixbuf);
  for(int y=0;y<3;++y)for(int x=0;x<5;++x){for(int c=0;c<3;++c)g_assert_cmpuint(p[y*stride+x*out_channels+c],==,((x+y)%2)*255);if(alpha)g_assert_cmpuint(p[y*stride+x*out_channels+3],==,(x%2)*255);}
  auto*roundtrip=gimp_temp_buf_new_from_pixbuf(pixbuf,babl_format("R'G'B'A u16"));g_object_unref(pixbuf);
  const auto*rgba=reinterpret_cast<const guint16*>(gimp_temp_buf_get_data(roundtrip));
  for(int y=0;y<3;++y)for(int x=0;x<5;++x){for(int c=0;c<3;++c)g_assert_cmpuint(rgba[(y*5+x)*4+c],==,((x+y)%2)*65535);g_assert_cmpuint(rgba[(y*5+x)*4+3],==,alpha?(x%2)*65535:65535);}
  g_assert_true(gimp_temp_buf_get_format(roundtrip)==babl_format("R'G'B'A u16"));gimp_temp_buf_unref(roundtrip);
 }
 g_assert_cmpuint(gimp_temp_buf_get_total_memsize(),==,baseline);
}
void nested_tempbuf_surface_writes()
{
 const auto baseline=gimp_temp_buf_get_total_memsize();
 for(const char*name:{"Y' u8","Y'A u8","R'G'B' u8","R'G'B'A u8","Y u16","YA double","RGBA half","R'G'B'A double"})
  for(bool cancel:{false,true})
  {
   const auto*format=babl_format(name);const int bpp=babl_format_get_bytes_per_pixel(format);auto*temp=gimp_temp_buf_new(13,9,format);
   auto*root=gimp_temp_buf_create_buffer(temp);auto*blue=gegl_color_new("blue");gegl_buffer_set_color(root,GEGL_RECTANGLE(0,0,13,9),blue);g_object_unref(blue);
   auto*child=gegl_buffer_create_sub_buffer(root,GEGL_RECTANGLE(3,2,7,5));auto*nested=gegl_buffer_create_sub_buffer(child,GEGL_RECTANGLE(4,3,4,3));
   auto*data=gimp_temp_buf_get_data(temp);std::vector<guchar>before(data,data+13*9*bpp);
   gimp_temp_buf_unref(temp);g_object_unref(root);g_object_unref(child);
   {
    GeglSurface surface(nested);surface.set_non_incremental(true);surface.set_stroke_opacity(.5f);
    surface.set_shape_provider([](float,float,float,float){return ShapeMask{9,9,std::vector<guchar>(81,255)};});surface.begin_session();
    g_assert_true(surface.draw_dab(6,5,4,1,0,0,1,1));
    for(int y=0;y<9;++y)for(int x=0;x<13;++x){const auto i=(y*13+x)*bpp;const bool inside=x>=4&&x<8&&y>=3&&y<6;g_assert_cmpint(std::memcmp(data+i,before.data()+i,bpp)!=0,==,inside);}
    const GeglRectangle dirty={4,3,4,3};g_assert_true(gegl_rectangle_equal(&surface.dirty(),&dirty));
    if(cancel){surface.cancel_session();g_assert_cmpmem(data,before.size(),before.data(),before.size());}else surface.end_session();
   }
   g_object_unref(nested);g_assert_cmpuint(gimp_temp_buf_get_total_memsize(),==,baseline);
  }
}
void scale_owned_storage()
{
 const auto baseline=gimp_temp_buf_get_total_memsize();
 for(const char*name:{"Y' u8","Y'A u8","R'G'B' u8","R'G'B'A u8"})
 {
  const auto*format=babl_format(name);const int bpp=babl_format_get_bytes_per_pixel(format);auto*temp=gimp_temp_buf_new(7,5,format);auto*p=gimp_temp_buf_get_data(temp);
  for(int y=0;y<5;++y)for(int x=0;x<7;++x)for(int c=0;c<bpp;++c)p[(y*7+x)*bpp+c]=y*31+x*7+c;
  const std::vector<guchar>original(p,p+7*5*bpp);auto*scaled=gimp_temp_buf_scale(temp,3,2);auto*same=gimp_temp_buf_scale(temp,7,5);
  gimp_temp_buf_data_clear(temp);gimp_temp_buf_unref(temp);g_assert_true(gimp_temp_buf_get_format(scaled)==format);
  for(int y=0;y<2;++y)for(int x=0;x<3;++x)g_assert_cmpmem(gimp_temp_buf_get_data(scaled)+(y*3+x)*bpp,bpp,original.data()+(int(y*2.5)*7+int(x*(7./3.)))*bpp,bpp);
  g_assert_cmpmem(gimp_temp_buf_get_data(same),original.size(),original.data(),original.size());gimp_temp_buf_unref(scaled);gimp_temp_buf_unref(same);
 }
 g_assert_cmpuint(gimp_temp_buf_get_total_memsize(),==,baseline);
}
}
int main(int argc,char**argv)
{
 gegl_init(&argc,&argv);g_test_init(&argc,&argv,nullptr);
 g_test_add_func("/tempbuf/144-native-format-copy-borrow",native_format_copy_and_borrow);
 g_test_add_func("/tempbuf/converted-lock-access-modes",converted_lock_access_modes);
 g_test_add_func("/tempbuf/padded-pixbuf-import-export",padded_pixbuf_import_export);
 g_test_add_func("/tempbuf/explicit-pixbuf-conversion",explicit_pixbuf_conversion);
 g_test_add_func("/tempbuf/nested-surface-writes",nested_tempbuf_surface_writes);
 g_test_add_func("/tempbuf/scale-owned-storage",scale_owned_storage);
 const int result=g_test_run();gegl_exit();return result;
}
