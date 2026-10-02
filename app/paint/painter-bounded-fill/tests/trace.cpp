/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "search.hpp"
#include "migration/fixtures/legacy-bounded-fill/stimuli.h"
#include <cstdio>
#include <vector>
using namespace GimpPainter::Fill;
int main(int argc,char**argv){gegl_init(&argc,&argv);
  for(int id=0;id<FILL_CASES;++id){
    std::vector<guchar> pixels(FILL_WIDTH*FILL_HEIGHT*4),mask(FILL_WIDTH*FILL_HEIGHT),result(mask.size());
    auto s=fill_stimulus(id,pixels.data(),mask.data());
    const char*formats[]={"Y' u8","Y'A u8","R'G'B' u8","R'G'B'A u8"};const Babl*format=babl_format(formats[s.bytes-1]);
    GeglRectangle extent={0,0,FILL_WIDTH,FILL_HEIGHT},bounds={s.x1,s.y1,s.x2-s.x1,s.y2-s.y1};
    auto*src=gegl_buffer_new(&extent,format);auto*m=gegl_buffer_new(&extent,babl_format("Y u8"));
    gegl_buffer_set(src,&extent,0,format,pixels.data(),GEGL_AUTO_ROWSTRIDE);gegl_buffer_set(m,&extent,0,babl_format("Y u8"),mask.data(),GEGL_AUTO_ROWSTRIDE);
    std::array<guchar,4> color{{s.color[0],s.color[1],s.color[2],s.color[3]}};
    auto snapshot=std::make_shared<Snapshot>(src,s.bytes,s.alpha,color);
    for(int grow=0;grow<2;++grow){Search::Options o;o.threshold=s.threshold;o.antialias=s.antialias;o.select_transparent=s.transparent;o.grow=grow;
      Search search(snapshot,s.use_mask?m:nullptr,bounds,s.seed_x,s.seed_y,o);
      while(search.step(137)!=Search::State::Complete){}
      gegl_buffer_get(search.result(),&extent,1,babl_format("Y u8"),result.data(),GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
      std::printf("MASK %d %s ",id,grow?"grow":"search");for(auto v:result)std::printf("%02x",v);std::puts("");}
    g_object_unref(src);g_object_unref(m);
  }std::puts("FILL_CAPTURE_COMPLETE");return 0;
}
