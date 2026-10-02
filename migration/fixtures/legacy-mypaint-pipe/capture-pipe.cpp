/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Shared synthetic inputs, compiled separately against pinned legacy archives or
 * the port. The legacy build includes no port evaluator/resource implementation.
 * The native selector wrapper observes only; it forwards unmodified inputs and
 * never reads the old undefined first last_coords on a freshly made Surface. */
#include <cstdio>
#include <vector>
#include <cstring>
#include <memory>
extern "C" {
#include "config.h"
#include <gtk/gtk.h>
#include <gegl.h>
#include "libgimpbase/gimpbase.h"
#include "libgimpmath/gimpmath.h"
#include "core/core-types.h"
#include "core/gimp.h"
#include "core/gimpcontext.h"
#include "core/gimpimage.h"
#include "core/gimpimage-undo.h"
#include "core/gimpundostack.h"
#include "core/gimplayer.h"
#include "core/gimpbrush.h"
#include "core/gimpbrushpipe.h"
#ifdef PIPE_LEGACY
#include "libgimpcolor/gimpcolor.h"
#include "base/base-types.h"
#include "base/temp-buf.h"
#include "base/tile-manager.h"
#include "core/gimpmypaintbrush.h"
#include "core/gimpmypaintbrush-load.h"
#include "paint/gimpmypaintoptions.h"
#else
#include "core/gimplayer-new.h"
#include "core/gimpbrush-private.h"
#include "core/gimptempbuf.h"
#include "paint/gimppaintoptions.h"
#endif
#include "tests.h"
}
#ifdef PIPE_LEGACY
#include "core/gimpmypaintbrush-private.hpp"
#include "paint/gimpmypaintcore.hpp"
#else
#include "paint/painter-mypaint-surface/paint-core.hpp"
using namespace GimpPainter::MyPaint;
#endif
static constexpr int width=64,height=48;
static int scenario,step,selection_count;
static bool coordinates_warmed=false,recording=false;
static GimpBrush*(*native_select)(GimpBrush*,const GimpCoords*,const GimpCoords*);
static GimpBrushPipe*active_a,*active_b;
static void coords_dump(const GimpCoords&c)
{std::printf(" %.9g %.9g %.9g %.9g %.9g %.9g %.9g",c.x,c.y,c.pressure,c.xtilt,c.ytilt,c.direction,c.velocity);}
static GimpBrush*observe_select(GimpBrush*b,const GimpCoords*last,const GimpCoords*current)
{
  const bool valid=coordinates_warmed;auto*p=GIMP_BRUSH_PIPE(b);
  auto*selected=native_select(b,last,current);++selection_count;
  if(recording){std::printf("PIPE_SELECT %d %d %d %c %d %d",scenario,step,selection_count,p==active_a?'A':'B',p->index[0],valid);if(valid)coords_dump(*last);coords_dump(*current);std::puts("");}
  coordinates_warmed=true;return selected;
}
static GimpBrushPipe*pipe_new(const char*name)
{
  auto*p=GIMP_BRUSH_PIPE(g_object_new(GIMP_TYPE_BRUSH_PIPE,"name",name,nullptr));
  p->n_brushes=4;p->brushes=g_new0(GimpBrush*,4);p->dimension=1;
  p->rank=g_new(gint,1);p->rank[0]=4;p->stride=g_new(gint,1);p->stride[0]=1;
  p->select=g_new(PipeSelectModes,1);p->select[0]=PIPE_SELECT_CONSTANT;
  p->index=g_new0(gint,1);
  for(int i=0;i<4;++i){auto*b=GIMP_BRUSH(g_object_new(GIMP_TYPE_BRUSH,"name","Pipe cell",nullptr));
#ifdef PIPE_LEGACY
    b->mask=temp_buf_new(5,3,1,0,0,nullptr);auto*data=temp_buf_get_data(b->mask);
#else
    b->priv->mask=gimp_temp_buf_new(5,3,babl_format("Y u8"));auto*data=gimp_temp_buf_get_data(b->priv->mask);
#endif
    for(int k=0;k<15;++k)data[k]=(k*37+i*53+11)%256;
    p->brushes[i]=b;
  }
  p->current=p->brushes[0];
#ifdef PIPE_LEGACY
  GIMP_BRUSH(p)->mask=p->brushes[0]->mask;
#else
  GIMP_BRUSH(p)->priv->mask=p->brushes[0]->priv->mask;
#endif
  return p;
}
static std::vector<guchar>pixels(GimpDrawable*d)
{
  std::vector<guchar>p(width*height*4);
#ifdef PIPE_LEGACY
  tile_manager_read_pixel_data(gimp_drawable_get_tiles(d),0,0,width-1,height-1,p.data(),width*4);
#else
  gegl_buffer_get(gimp_drawable_get_buffer(d),GEGL_RECTANGLE(0,0,width,height),1,babl_format("R'G'B'A u8"),p.data(),GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
#endif
  return p;
}
static void put_pixels(GimpDrawable*d,const std::vector<guchar>&p)
{
#ifdef PIPE_LEGACY
  tile_manager_write_pixel_data(gimp_drawable_get_tiles(d),0,0,width-1,height-1,p.data(),width*4);
#else
  gegl_buffer_set(gimp_drawable_get_buffer(d),GEGL_RECTANGLE(0,0,width,height),0,babl_format("R'G'B'A u8"),p.data(),GEGL_AUTO_ROWSTRIDE);
#endif
}
static int child(GimpBrushPipe*p){for(int i=0;i<p->n_brushes;++i)if(p->brushes[i]==p->current)return i;return -1;}
static void dump(const char*phase,GimpDrawable*d)
{
  std::printf("PIPE_STATE %d %s %d %d %d %d %d %d\n",scenario,phase,selection_count,active_a->index[0],child(active_a),active_b->index[0],child(active_b),gimp_undo_stack_get_depth(gimp_image_get_undo_stack(gimp_item_get_image(GIMP_ITEM(d)))));
  std::printf("PIPE_PIX %d %s ",scenario,phase);for(auto c:pixels(d))std::printf("%02x",c);std::puts("");
}
static GimpLayer*layer_new(GimpImage*image,const char*name)
{
#ifdef PIPE_LEGACY
  auto*l=gimp_layer_new(image,width,height,GIMP_RGBA_IMAGE,name,1,GIMP_NORMAL_MODE);
#else
  auto*l=gimp_layer_new(image,width,height,babl_format("R'G'B'A u8"),name,1,GIMP_LAYER_MODE_NORMAL);
#endif
  g_assert(gimp_image_add_layer(image,l,nullptr,0,FALSE));return l;
}
static void capture(Gimp*gimp,int number,const char*base)
{
  scenario=number;step=0;selection_count=0;recording=false;coordinates_warmed=false;
  const auto mode=static_cast<PipeSelectModes>(number%8);const bool smudge=(number/8)%2;const bool floating=number>=16;
#ifdef PIPE_LEGACY
  auto*image=gimp_image_new(gimp,width,height,GIMP_RGB);
  auto*options=GIMP_MYPAINT_OPTIONS(g_object_new(GIMP_TYPE_MYPAINT_OPTIONS,"gimp",gimp,nullptr));
  if(options->brush)g_object_unref(options->brush);GError*error=nullptr;
  auto*loaded=gimp_mypaint_brush_load(GIMP_CONTEXT(options),base,&error);g_assert_no_error(error);g_assert(loaded);
  options->brush=GIMP_MYPAINT_BRUSH(loaded->data);g_list_free(loaded);
  auto&settings=*static_cast<GimpMypaintBrushPrivate*>(options->brush->p);
#define SET_SWITCH settings.set_bool_value
#else
  (void)base;auto*image=gimp_image_new(gimp,width,height,GIMP_RGB,GIMP_PRECISION_U8_NON_LINEAR);
  auto*options=GIMP_PAINT_OPTIONS(g_object_new(GIMP_TYPE_PAINT_OPTIONS,"gimp",gimp,nullptr));
  auto settings=Resource::decode("{\"version\":3,\"settings\":{},\"switches\":{},\"texts\":{}}");
#define SET_SWITCH settings.set_switch
#endif
  SET_SWITCH(BRUSH_NON_INCREMENTAL,false);SET_SWITCH(BRUSH_USE_GIMP_BRUSHMARK,true);
#undef SET_SWITCH
  settings.set_base_value(BRUSH_DABS_PER_SECOND,40);settings.set_base_value(BRUSH_RADIUS_LOGARITHMIC,1.7);
  settings.set_base_value(BRUSH_SMUDGE,smudge?.7:0);settings.set_base_value(BRUSH_SMUDGE_LENGTH,.65);
#ifdef PIPE_LEGACY
  GimpRGB fg={.8,.2,.4,1},bg={.1,.2,.3,1};gimp_context_set_foreground(GIMP_CONTEXT(options),&fg);gimp_context_set_background(GIMP_CONTEXT(options),&bg);
#else
  auto*fg=gegl_color_new(nullptr);auto*bg=gegl_color_new(nullptr);double f[4]={.8,.2,.4,1},b[4]={.1,.2,.3,1};
  gegl_color_set_pixel(fg,babl_format("R'G'B'A double"),f);gegl_color_set_pixel(bg,babl_format("R'G'B'A double"),b);
  gimp_context_set_foreground(GIMP_CONTEXT(options),fg);gimp_context_set_background(GIMP_CONTEXT(options),bg);g_object_unref(fg);g_object_unref(bg);
#endif
  active_a=pipe_new("Active pipe A");active_b=pipe_new("Active pipe B");gimp_context_set_brush(GIMP_CONTEXT(options),GIMP_BRUSH(active_a));
  auto*d=GIMP_DRAWABLE(layer_new(image,"Pipe target A"));auto*other=GIMP_DRAWABLE(layer_new(image,"Pipe target B"));
  std::vector<guchar>initial(width*height*4);for(int y=0;y<height;++y)for(int x=0;x<width;++x){int i=(y*width+x)*4;initial[i]=(x*19+y*7)%256;initial[i+1]=(x*3+y*29)%256;initial[i+2]=(x*13+y*11)%256;initial[i+3]=160+(x*17+y*23)%96;}
  put_pixels(d,initial);put_pixels(other,initial);gimp_image_undo_free(image);
  {
#ifdef PIPE_LEGACY
    // Static initialization defines the old core's otherwise uninitialized
    // options pointer. cleanup() discards its previous Surface between cases.
    static GimpMypaintCore core;
    auto finish=[&]{core.split_stroke();};
    auto move=[&](GimpDrawable*target,double dt,GimpCoords c,bool hover=false){if(hover)c.pressure=0;core.stroke_to(target,dt,&c,options);};
    auto configure=[&]{core.option_changed(G_OBJECT(options),nullptr);};
#else
    PaintCore core(options,settings);
    auto finish=[&]{core.finish();};
    auto move=[&](GimpDrawable*target,double dt,GimpCoords c,bool hover=false){if(hover)core.hover_to(target,dt,c);else core.stroke_to(target,dt,c);};
    auto configure=[&]{core.configure(settings);};
#endif
    GimpCoords c=GIMP_COORDS_DEFAULT_VALUES;c.x=18;c.y=18;c.pressure=0;c.direction=.125;c.velocity=.2;c.xtilt=-.25;c.ytilt=.4;
    move(d,.01,c);c.pressure=.85;move(d,.1,c);finish();g_assert(coordinates_warmed);g_assert(gimp_image_undo(image));g_assert(pixels(d)==initial);gimp_image_undo_free(image);
#ifdef PIPE_LEGACY
    settings.set_bool_value(BRUSH_NON_INCREMENTAL,floating);
#else
    settings.set_switch(BRUSH_NON_INCREMENTAL,floating);
#endif
    configure();
    active_a->select[0]=mode;active_b->select[0]=mode;
    // The one seed belongs to the test's controlled external RNG, never to a
    // production segment/resource provider. No reseeding occurs at boundaries.
    g_random_set_seed(7823+number);selection_count=0;recording=true;
    c.pressure=.3;c.x=22;c.y=21;move(d,.08,c);dump("first",d);
    ++step;finish();dump("explicit-finish",d);
    ++step;c.x=25;c.y=20;c.pressure=.95;c.direction=.75;c.velocity=.9;c.xtilt=.75;c.ytilt=-.8;move(d,.06,c);dump("after-finish",d);
    ++step;c.x=27;c.y=24;c.pressure=.5;c.direction=.43;c.velocity=.4;c.xtilt=-.8;c.ytilt=.75;move(d,.2,c);dump("moving",d);
    ++step;c.x=34;c.y=28;move(d,.25,c,true);dump("hover-split",d);
    ++step;c.x=31;c.y=27;c.direction=.1;c.velocity=.1;move(d,.2,c,true);dump("hover-idle",d);
    ++step;finish();dump("idle-finish",d);
    ++step;c.x=29;c.y=22;c.pressure=.65;c.direction=.9;c.velocity=.75;move(d,.1,c);finish();dump("repeated",d);
    ++step;gboolean undo=gimp_image_undo(image);dump("undo",d);gboolean redo=gimp_image_redo(image);dump("redo",d);std::printf("PIPE_UNDO %d %d %d\n",scenario,undo,redo);
    ++step;gimp_context_set_brush(GIMP_CONTEXT(options),GIMP_BRUSH(active_b));configure();c.x=26;c.y=18;c.pressure=.8;move(d,.08,c);finish();dump("resource-B",d);
    ++step;gimp_context_set_brush(GIMP_CONTEXT(options),GIMP_BRUSH(active_a));configure();c.x=24;c.y=17;c.pressure=.45;move(d,.07,c);finish();dump("resource-A",d);
    // A drawable switch destroys the old Surface. Warm a constant selector
    // again before recording defined last_coords, without asserting cold bytes.
    ++step;recording=false;coordinates_warmed=false;active_a->select[0]=PIPE_SELECT_CONSTANT;
#ifdef PIPE_LEGACY
    settings.set_bool_value(BRUSH_NON_INCREMENTAL,false);
#else
    settings.set_switch(BRUSH_NON_INCREMENTAL,false);
#endif
    configure();
    move(other,.01,c);c.x=28;c.y=20;move(other,.1,c);finish();g_assert(coordinates_warmed);
    #ifdef PIPE_LEGACY
    settings.set_bool_value(BRUSH_NON_INCREMENTAL,floating);
#else
    settings.set_switch(BRUSH_NON_INCREMENTAL,floating);
#endif
    configure();active_a->select[0]=mode;recording=true;c.x=30;c.y=23;c.pressure=.6;move(other,.08,c);finish();dump("drawable-B-warmed",other);
    ++step;std::printf("PIPE_RNG %d %u\n",scenario,g_random_int());recording=false;
#ifdef PIPE_LEGACY
    core.cleanup();
#endif
  }
  g_object_unref(active_a);g_object_unref(active_b);g_object_unref(options);g_object_unref(image);
}
int main(int argc,char**argv)
{
#ifdef PIPE_LEGACY
  g_type_init();
#endif
  auto*gimp=gimp_init_for_testing();auto*klass=GIMP_BRUSH_CLASS(g_type_class_ref(GIMP_TYPE_BRUSH_PIPE));native_select=klass->select_brush;klass->select_brush=observe_select;
  for(int i=0;i<32;++i)capture(gimp,i,argc>1?argv[1]:"");
  klass->select_brush=native_select;g_type_class_unref(klass);
  std::puts("PIPE_CAPTURE_COMPLETE");return 0;
}
