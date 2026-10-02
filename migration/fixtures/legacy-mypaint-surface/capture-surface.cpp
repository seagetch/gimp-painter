/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Actual pinned TempBuf Surface implementation; explicit synthetic pixels/dabs.
 * No port renderer/evaluator participates in this capture. */
#include <cstdio>
#include <string>
#include <memory>
extern "C" {
#include "config.h"
#include <gegl.h>
#include <cairo.h>
#include "libgimpbase/gimpbase.h"
#include "libgimpcolor/gimpcolor.h"
#include "app/core/core-types.h"
#include "app/base/base-types.h"
#include "app/base/temp-buf.h"
#include "app/core/gimppattern.h"
#include "app/core/gimpbrush.h"
}
#include "app/paint/gimpmypaintcore-surface.hpp"
static void dump (const std::string& name, TempBuf *buffer)
{
  std::printf ("PIX %s ", name.c_str ());
  auto *p = temp_buf_get_data (buffer);
  for (int i = 0; i < buffer->width*buffer->height*buffer->bytes; ++i) std::printf ("%02x",p[i]);
  std::puts ("");
}
int main ()
{
  GimpRGB bg = { .1, .2, .3, 1 };
  for (int shape = 0; shape < 2; ++shape)
    for (int texture = 0; texture < 2; ++texture)
      for (int floating = 0; floating < 2; ++floating) {
        const std::string name = std::to_string(shape)+"-"+std::to_string(texture)+"-"+std::to_string(floating);
        guchar clear[4] = {0,0,0,0};
        auto *buf = temp_buf_new (23,17,4,0,0,clear);
        auto *p = temp_buf_get_data (buf);
        for (int y = 0; y < 17; ++y) for (int x = 0; x < 23; ++x) {
          const int i = (y*23+x)*4; p[i]=(x*19+y*7)%256; p[i+1]=(x*3+y*29)%256; p[i+2]=(x*13+y*11)%256; p[i+3]=(x<4)?0:(x*17+y*23)%256;
        }
        auto *surface = GimpMypaintSurface_TempBuf_new (buf);
        surface->set_bg_color (&bg); surface->set_floating_stroke (floating); surface->set_stroke_opacity (.37);
        GimpBrush *brush = nullptr;
        GimpPattern *pattern = nullptr;
        if (shape) {
          brush = GIMP_BRUSH (g_object_new (GIMP_TYPE_BRUSH,"name","asymmetric-test-mask",nullptr));
          brush->mask = temp_buf_new (5,3,1,0,0,clear);
          const guchar mask[] = {0,30,90,160,255,12,60,120,180,240,255,190,140,70,0};
          memcpy (temp_buf_get_data (brush->mask),mask,sizeof mask);
          surface->set_brushmark (brush);
        }
        if (texture) {
          pattern = GIMP_PATTERN (g_object_new (GIMP_TYPE_PATTERN,"name","test-paper",nullptr));
          pattern->mask = temp_buf_new (3,2,3,0,0,clear);
          const guchar paper[] = {20,250,10,100,0,50,240,2,180,200,12,33,64,25,70,128,100,5};
          memcpy (temp_buf_get_data (pattern->mask),paper,sizeof paper);
          surface->set_texture (pattern);
        }
        surface->begin_session ();
        for (int i = 0; i < 6; ++i) {
          GimpCoords coords = GIMP_COORDS_DEFAULT_VALUES;
          coords.x = i < 3 ? 7.25 : i == 3 ? -2.5 : i == 4 ? 22.2 : 11.2;
          coords.y = i < 3 ? 8.75 : i == 3 ? 1.75 : i == 4 ? 16.1 : 5.5;
          surface->set_coords (&coords);
          const float r = i == 1 ? 4.7f : 5.3f, hard = i == 2 ? .9f : .55f;
          const float aspect = i == 0 ? 1.f : 2.3f, angle = i == 4 ? -43.f : 37.f;
          if (brush) {
            const TempBuf *mask = gimp_brush_transform_mask (brush, r*2/5.f, 20*(1.f-1.f/aspect), -angle/360, hard);
            std::printf ("MASK %s %d %d %d ",name.c_str(),i,mask->width,mask->height);
            auto *pixels = temp_buf_get_data (mask);
            for (int j=0;j<mask->width*mask->height;++j) std::printf ("%02x",pixels[j]);
            std::puts ("");
          }
          float cr=0,cg=0,cb=0,ca=0;
          surface->get_color (coords.x, coords.y, r,&cr,&cg,&cb,&ca,hard,aspect,angle,-.15,.8);
          std::printf ("SAMPLE %s %d %a %a %a %a\n",name.c_str(),i,double(cr),double(cg),double(cb),double(ca));
          const bool changed = surface->draw_dab (coords.x,coords.y,r,.8,.2,.4,.63,hard,i==2?.2f:1.f,aspect,angle,i==5?.45f:0.f,0.f,-.15,.8);
          std::printf ("DAB %s %d %d\n",name.c_str(),i,changed);
          dump (name+"-"+std::to_string(i),buf);
        }
        surface->end_session ();
        surface->set_brushmark (nullptr); surface->set_texture (nullptr);
        delete surface;
        if (brush) g_object_unref (brush);
        if (pattern) g_object_unref (pattern);
        temp_buf_free (buf);
      }
  // Old zero-alpha lock path performs 0/0 before converting hidden RGB bytes.
  // Capture the actual pinned runtime result, rather than inventing a value.
  for (int floating=0;floating<2;++floating) {
    guchar hidden[4]={33,99,177,0};auto*buf=temp_buf_new(23,17,4,0,0,hidden);
    auto*surface=GimpMypaintSurface_TempBuf_new(buf);surface->set_floating_stroke(floating);
    surface->begin_session();
    bool changed=surface->draw_dab(11,8,4,.8,.2,.4,.63,.55,1,1,0,1,0,0,1);
    const auto name=std::string("transparent-lock-")+std::to_string(floating);
    std::printf("DAB %s 0 %d\n",name.c_str(),changed);dump(name,buf);
    surface->end_session();delete surface;temp_buf_free(buf);
  }

}
