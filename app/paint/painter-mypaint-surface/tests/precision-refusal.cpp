/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "paint/painter-mypaint-surface/gegl-surface.hpp"
#include <gegl.h>
#include <cstring>
#include <stdexcept>
#include <vector>
using namespace GimpPainter::MyPaint;
static std::vector<guchar> bytes(GeglBuffer *b){const auto *r=gegl_buffer_get_extent(b);auto *f=gegl_buffer_get_format(b);std::vector<guchar> v(r->width*r->height*babl_format_get_bytes_per_pixel(f));gegl_buffer_get(b,r,1,f,v.data(),GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);return v;}
static void refused_formats()
{
 unsigned count=0;
 for(const char *name:{"RaGaBaA float","R'aG'aB'aA float","YaA float","CIE Lab float","CIE XYZ float","CMYK float"}){
  g_assert_true(babl_format_exists(name));auto *f=babl_format(name);auto *b=gegl_buffer_new(GEGL_RECTANGLE(-2,-1,3,2),f);
  std::vector<guchar> seed(6*babl_format_get_bytes_per_pixel(f),0);for(unsigned i=0;i<seed.size();++i)seed[i]=(i*37+11)%256;gegl_buffer_set(b,gegl_buffer_get_extent(b),0,f,seed.data(),GEGL_AUTO_ROWSTRIDE);auto before=bytes(b);
  for(int attempt=0;attempt<3;++attempt){bool rejected=false;try{GeglSurface s(b);}catch(const std::invalid_argument &e){rejected=true;g_assert_nonnull(std::strstr(e.what(),"native RGB/RGBA/Gray/Gray-alpha"));}g_assert_true(rejected);g_assert_true(bytes(b)==before);g_assert_true(gegl_buffer_get_format(b)==f);}
  g_test_message("explicit refusal, unchanged native storage: %s",name);g_object_unref(b);++count;
 }
 g_assert_cmpuint(count,==,6);
}
static void noncanonical()
{
 // Same RGB model and scalar kind, but noncanonical BGR component order.
 const auto *f=babl_format_new("name","painter-refusal-BGR-double",babl_model("RGB"),babl_type("double"),babl_component("B"),babl_component("G"),babl_component("R"),nullptr);
 auto *b=gegl_buffer_new(GEGL_RECTANGLE(0,0,1,1),f);double seed[]={.2,.4,.6};gegl_buffer_set(b,gegl_buffer_get_extent(b),0,f,seed,GEGL_AUTO_ROWSTRIDE);auto before=bytes(b);bool rejected=false;
 try{GeglSurface s(b);}catch(const std::invalid_argument &e){rejected=true;g_assert_nonnull(std::strstr(e.what(),"packed homogeneous"));}
 g_assert_true(rejected);g_assert_true(bytes(b)==before);g_assert_true(gegl_buffer_get_format(b)==f);g_object_unref(b);
}
int main(int argc,char **argv){g_test_init(&argc,&argv,nullptr);gegl_init(&argc,&argv);g_test_add_func("/precision-refusal/native-models",refused_formats);g_test_add_func("/precision-refusal/noncanonical-order",noncanonical);int status=g_test_run();gegl_exit();return status;}
