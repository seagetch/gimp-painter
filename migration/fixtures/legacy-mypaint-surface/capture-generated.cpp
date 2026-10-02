/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <cstdio>
#include <initializer_list>
extern "C" {
#include "config.h"
#include <gegl.h>
#include <cairo.h>
#include "libgimpbase/gimpbase.h"
#include "app/core/core-types.h"
#include "app/base/base-types.h"
#include "app/base/temp-buf.h"
#include "app/core/gimpbrushgenerated.h"
}
int main()
{
  for(int shape=0;shape<3;++shape)for(int spikes : {2,5})for(float base_aspect : {1.f,3.5f}) {
    auto *brush=GIMP_BRUSH(gimp_brush_generated_new("generated",GimpBrushGeneratedShape(shape),7.5f,spikes,.65f,base_aspect,23.f));
    gimp_brush_begin_use(brush);
    for(int i=0;i<6;++i) {
      float radius=i==1?4.7f:5.3f,hard=i==2?.9f:.55f,aspect=i==0?1.f:2.3f,angle=i==4?-43.f:37.f;
      const float scale=radius*2/MAX(brush->mask->width,brush->mask->height),ratio=20*(1.0-(1.0/aspect));
      auto *mask=gimp_brush_transform_mask(brush,scale,ratio,-angle/360,hard);
      std::printf("GENERATED %d %d %a %d %d %d ",shape,spikes,double(base_aspect),i,mask->width,mask->height);
      auto *pixels=temp_buf_get_data(mask);for(int j=0;j<mask->width*mask->height;++j)std::printf("%02x",pixels[j]);std::puts("");
    }
    gimp_brush_end_use(brush);g_object_unref(brush);
  }
}
