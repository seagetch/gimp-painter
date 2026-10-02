/* SPDX-License-Identifier: GPL-3.0-or-later
 * Capture the unmodified native interpolation before introducing pause points.
 * This spy has only native C instance fields; it owns no C++ implementation.
 */
#include "config.h"
#include <gegl.h>
#include <gtk/gtk.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
extern "C" {
#include "libgimpbase/gimpbase.h"
#include "libgimpmath/gimpmath.h"
#include "core/core-types.h"
#include "core/gimp.h"
#include "core/gimpbrush.h"
#include "core/gimpbrush-private.h"
#include "core/gimpdynamics.h"
#include "core/gimpdynamicsoutput.h"
#include "core/gimpimage.h"
#include "core/gimplayer.h"
#include "core/gimplayer-new.h"
#include "core/gimptempbuf.h"
#include "paint/gimpbrushcore.h"
#include "paint/gimppaintoptions.h"
#include "tests.h"
}
struct TraceBrush { GimpBrushCore parent; };
struct TraceBrushClass { GimpBrushCoreClass parent; };
static guint scenario, motion_number, dab_number;
GType trace_brush_get_type (void);
G_DEFINE_TYPE (TraceBrush, trace_brush, GIMP_TYPE_BRUSH_CORE)
static void print_coords (const GimpCoords& c) {
  std::printf(" %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %d",
    c.x,c.y,c.pressure,c.xtilt,c.ytilt,c.wheel,c.distance,c.rotation,c.slider,c.velocity,c.direction,c.xscale,c.yscale,c.angle,c.reflect);
}
static void paint (GimpPaintCore *core,GList *drawables,GimpPaintOptions *options,GimpSymmetry*,GimpPaintState state,guint32 time) {
  if(state!=GIMP_PAINT_STATE_MOTION)return;
  auto *brush=GIMP_BRUSH_CORE(core);auto *image=gimp_item_get_image(GIMP_ITEM(drawables->data));
  brush->spacing=gimp_brush_get_spacing(brush->main_brush)/100.0;
  gimp_brush_core_eval_transform_dynamics(brush,image,options,&core->cur_coords);
  std::printf("INTERP D %u %u %u %u",scenario,motion_number,dab_number++,time);
  print_coords(core->cur_coords);print_coords(core->last_coords);
  std::printf(" %.17g %.17g %.17g %.17g %.17g %.17g %.17g %d\n",core->last_paint.x,core->last_paint.y,core->distance,core->pixel_dist,brush->scale,brush->angle,brush->aspect_ratio,brush->reflect);
}
static void trace_brush_class_init(TraceBrushClass*k) {
  GIMP_PAINT_CORE_CLASS(k)->paint=paint;auto *b=GIMP_BRUSH_CORE_CLASS(k);
  b->handles_changing_brush=TRUE;b->handles_transforming_brush=TRUE;b->handles_dynamic_transforming_brush=TRUE;
}
static void trace_brush_init(TraceBrush*) {}
static void run_case(Gimp*gimp,int pattern,int spacing,int size,int flags,gsize budget) {
  auto *image=gimp_image_new(gimp,256,256,GIMP_RGB,GIMP_PRECISION_U8_NON_LINEAR);
  auto *layer=gimp_layer_new(image,256,256,babl_format("R'G'B'A u8"),"trace",1,GIMP_LAYER_MODE_NORMAL);
  gimp_image_add_layer(image,layer,nullptr,0,FALSE);GList drawables={layer,nullptr,nullptr};
  auto *options=GIMP_PAINT_OPTIONS(g_object_new(GIMP_TYPE_PAINT_OPTIONS,"gimp",gimp,"brush-size",double(size),"brush-angle",17.0,"brush-aspect-ratio",flags&1?7.0:0.0,"use-jitter",bool(flags&2),"jitter-amount",0.7,nullptr));
  auto *brush=GIMP_BRUSH(g_object_new(GIMP_TYPE_BRUSH,"name","native-interpolation",nullptr));
  brush->priv->mask=gimp_temp_buf_new(15,15,babl_format("Y u8"));std::memset(gimp_temp_buf_get_data(brush->priv->mask),255,225);
  brush->priv->x_axis={7.5,0};brush->priv->y_axis={0,7.5};gimp_brush_set_spacing(brush,spacing);
  gimp_context_set_brush(GIMP_CONTEXT(options),brush);g_object_unref(brush);
  auto *dynamics=GIMP_DYNAMICS(g_object_new(GIMP_TYPE_DYNAMICS,"name","trace-dynamics",nullptr));
  if(flags&1){g_object_set(gimp_dynamics_get_output(dynamics,GIMP_DYNAMICS_OUTPUT_SIZE),"use-pressure",TRUE,nullptr);g_object_set(gimp_dynamics_get_output(dynamics,GIMP_DYNAMICS_OUTPUT_SPACING),"use-pressure",TRUE,nullptr);}
  gimp_context_set_dynamics(GIMP_CONTEXT(options),dynamics);g_object_unref(dynamics);
  auto *core=GIMP_PAINT_CORE(g_object_new(trace_brush_get_type(),"undo-desc","trace",nullptr));
  g_rand_set_seed(GIMP_BRUSH_CORE(core)->rand,101+scenario);g_random_set_seed(901+scenario);
  GimpCoords coords=GIMP_COORDS_DEFAULT_VALUES;coords.x=32;coords.y=32;coords.pressure=.2;coords.xscale=1.25;coords.yscale=.75;coords.angle=.13;coords.reflect=flags&2;
  GError *error=nullptr;g_assert_true(gimp_paint_core_start(core,&drawables,options,&coords,&error));g_assert_no_error(error);
  motion_number=0;dab_number=0;gimp_paint_core_paint(core,&drawables,options,GIMP_PAINT_STATE_INIT,0);gimp_paint_core_paint(core,&drawables,options,GIMP_PAINT_STATE_MOTION,0);gimp_paint_core_set_last_coords(core,&coords);
  const double points[][4][2]={{{42,32},{77.25,32},{70.3,45.2},{80.8,80.1}},{{32,42},{32,77.25},{45.2,70.3},{80.1,80.8}},{{42,33},{43,54},{78.25,54.3},{78.1,79.6}},{{32.00000001,31.99999999},{32.99999999,33.00000001},{30.2,28.8},{19.7,17.2}},{{32,32},{32,32},{33,32},{33,33}},{{32.01,32.01},{32.2,32.4},{32.9,32.7},{33.4,33.1}}};
  std::printf("INTERP C %u %d %d %d %d\n",scenario,pattern,spacing,size,flags);
  for(motion_number=1;motion_number<=4;++motion_number){
    coords.x=points[pattern][motion_number-1][0];coords.y=points[pattern][motion_number-1][1];
    coords.pressure=.2+.175*motion_number;coords.xtilt=.13*motion_number;coords.ytilt=-.11*motion_number;coords.wheel=.07*motion_number;coords.velocity=.08*motion_number;coords.direction=.09*motion_number;coords.rotation=.12*motion_number;coords.distance=.03*motion_number;coords.slider=.02*motion_number;
    if(flags&4)GIMP_BRUSH_CORE(core)->scale=0;
#ifdef GIMP_BRUSH_INTERPOLATION_RESUMABLE
    if(budget){auto *job=gimp_brush_core_interpolation_begin(GIMP_BRUSH_CORE(core),&drawables,options,&coords,137*motion_number);
      while(!gimp_brush_core_interpolation_step(GIMP_BRUSH_CORE(core),&drawables,options,job,budget)){}
      gimp_brush_core_interpolation_free(job);
    }else
#else
    g_assert_cmpuint(budget,==,0);
#endif
      gimp_paint_core_interpolate(core,&drawables,options,&coords,137*motion_number);
    std::printf("INTERP E %u %u %u",scenario,motion_number,dab_number);print_coords(core->cur_coords);print_coords(core->last_coords);
    std::printf(" %.17g %.17g %.17g %.17g\n",core->last_paint.x,core->last_paint.y,core->distance,core->pixel_dist);
  }
  gimp_paint_core_paint(core,&drawables,options,GIMP_PAINT_STATE_FINISH,999);gimp_paint_core_finish(core,&drawables,FALSE);
  g_object_unref(core);g_object_unref(options);g_object_unref(image);
}
int main(int argc,char**argv){gsize budget=argc>1?std::strtoull(argv[1],nullptr,10):0;Gimp*gimp=gimp_init_for_testing();
  for(int pattern=0;pattern<6;++pattern)for(int spacing:{5,20,250})for(int size:{1,17})for(int flags:{0,1,2,3,4})run_case(gimp,pattern,spacing,size,flags,budget),++scenario;
  std::printf("INTERP COMPLETE %u\n",scenario);return 0;}
