/* SPDX-License-Identifier: GPL-3.0-or-later
 * Compares port native transactions with separately captured old output. */
#include "config.h"
#include <gtk/gtk.h>
#include <vector>
extern "C" {
#include <gegl.h>
#include <stdio.h>
#include "libgimpbase/gimpbase.h"
#include "libgimpcolor/gimpcolor.h"
#include "libgimpmath/gimpmath.h"
#include "core/core-types.h"
#include "core/gimptempbuf.h"
#include "core/gimpbrush-private.h"
#include "paint/gimpfillbrush.h"
#include "core/gimp.h"
#include "core/gimpcontext.h"
#include "core/gimpimage.h"
#include "core/gimpimage-undo.h"
#include "core/gimplayer.h"
#include "core/gimplayer-new.h"
#include "core/gimpbrush.h"
#include "core/gimpdynamics.h"
#include "core/gimppattern.h"
#include "core/gimpchannel.h"
#include "core/gimpchannel-select.h"
#include "paint/paint-types.h"
#include "paint/gimppaintcore.h"
#include "paint/gimppaintoptions.h"
#include "tests.h"
#include "painter-owned-stroke-trace.h"
}

#define WIDTH 90
#define HEIGHT 70
static void dump(int id,const char*phase,GimpDrawable*d){guchar p[WIDTH*HEIGHT*4];gegl_buffer_get(gimp_drawable_get_buffer(d),GEGL_RECTANGLE(0,0,WIDTH,HEIGHT),1,babl_format("R'G'B'A u8"),p,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);printf("BRUSH %d %s ",id,phase);for(unsigned i=0;i<sizeof p;++i)printf("%02x",p[i]);puts("");}
int main(int argc,char**argv){
 const char*generic_route=g_getenv("PAINTER_OWNED_GENERIC_ROUTE");
 const bool use_paper=g_getenv("PAINTER_FILL_PAPER_FIXTURE")!=nullptr;
 const bool async=argc>1&&g_str_equal(argv[1],"async");
 const bool queued=argc>1&&!async;
 auto motion=async?gimp_fill_brush_motion_begin:gimp_fill_brush_motion;
 Gimp*gimp=gimp_init_for_testing();
 GType type=GIMP_TYPE_FILL_BRUSH,options_type=GIMP_TYPE_FILL_BRUSH_OPTIONS;g_assert(type);g_assert(options_type);
 for(int id=0;id<12;++id){
  GimpImage*image=gimp_image_new(gimp,100,80,GIMP_RGB,GIMP_PRECISION_U8_NON_LINEAR);GimpLayer*layer=gimp_layer_new(image,WIDTH,HEIGHT,babl_format("R'G'B'A u8"),"fill-oracle",1,GIMP_LAYER_MODE_PAINTER_NORMAL);gimp_image_add_layer(image,layer,NULL,0,FALSE);gimp_item_set_offset(GIMP_ITEM(layer),3,5);GimpDrawable*d=GIMP_DRAWABLE(layer);
  guchar p[WIDTH*HEIGHT*4];for(int y=0;y<HEIGHT;++y)for(int x=0;x<WIDTH;++x){int i=(y*WIDTH+x)*4;guchar v=(x==47&&y>4&&y<65)?200:70;p[i]=v;p[i+1]=v+15;p[i+2]=v+30;p[i+3]=255;}gegl_buffer_set(gimp_drawable_get_buffer(d),GEGL_RECTANGLE(0,0,WIDTH,HEIGHT),0,babl_format("R'G'B'A u8"),p,GEGL_AUTO_ROWSTRIDE);
  if(id/6)gimp_channel_select_rectangle(gimp_image_get_mask(image),40,0,60,80,GIMP_CHANNEL_OP_REPLACE,FALSE,0,0,FALSE);
  GimpPaintOptions*options=GIMP_PAINT_OPTIONS(g_object_new(options_type,"gimp",gimp,"rate",(double)((id%3)*50),"eraser-mode",(id/3)%2,"brush-size",23.0,NULL));
  double fgv[]={.8,.15,.35,1},bgv[]={.1,.2,.3,1};GeglColor*fg=gegl_color_new(NULL),*bg=gegl_color_new(NULL);gegl_color_set_pixel(fg,babl_format("R'G'B'A double"),fgv);gegl_color_set_pixel(bg,babl_format("R'G'B'A double"),bgv);gimp_context_set_foreground(GIMP_CONTEXT(options),fg);gimp_context_set_background(GIMP_CONTEXT(options),bg);g_object_unref(fg);g_object_unref(bg);gimp_context_set_paint_mode(GIMP_CONTEXT(options),GIMP_LAYER_MODE_PAINTER_NORMAL);gimp_context_set_opacity(GIMP_CONTEXT(options),.65);
  GimpBrush*brush=GIMP_BRUSH(g_object_new(GIMP_TYPE_BRUSH,"name","fill-mask",NULL));brush->priv->mask=gimp_temp_buf_new(23,23,babl_format("Y u8"));guchar*bm=gimp_temp_buf_get_data(brush->priv->mask);for(int y=0;y<23;++y)for(int x=0;x<23;++x){int dx=x-11,dy=y-11;bm[y*23+x]=dx*dx+dy*dy<90?255:0;}

  if(use_paper){
    g_object_set(options,"use-texture",TRUE,nullptr);
    auto*paper=GIMP_PATTERN(g_object_new(GIMP_TYPE_PATTERN,"name","fill paper",nullptr));
    g_clear_pointer(&paper->mask,gimp_temp_buf_unref);paper->mask=gimp_temp_buf_new(5,3,babl_format("R'G'B' u8"));
    auto*pm=gimp_temp_buf_get_data(paper->mask);for(int y=0;y<3;++y)for(int x=0;x<5;++x)for(int b=0;b<3;++b)pm[(y*5+x)*3+b]=(x*57+y*31+b*83)%256;
    gimp_context_set_pattern(GIMP_CONTEXT(options),paper);g_object_unref(paper);
  }
  gimp_context_set_brush(GIMP_CONTEXT(options),brush);GimpDynamics*dyn=GIMP_DYNAMICS(g_object_new(GIMP_TYPE_DYNAMICS,"name","fill-no-dynamics",NULL));gimp_context_set_dynamics(GIMP_CONTEXT(options),dyn);
  GimpPaintCore*core=GIMP_PAINT_CORE(g_object_new(type,"undo-desc","fill oracle",NULL));GimpCoords c=GIMP_COORDS_DEFAULT_VALUES;c.x=43;c.y=40;c.pressure=1;GError*error=NULL;
  gimp_image_undo_free(image);
  if(generic_route){g_assert(owned_trace_stroke(generic_route,core,d,options,c,FALSE,FALSE,&error));g_assert_no_error(error);}
  else {g_assert(gimp_fill_brush_begin(GIMP_FILL_BRUSH(core),d,options,&c,&error));g_assert_no_error(error);
  g_assert(motion(GIMP_FILL_BRUSH(core),&c,0,&error));g_assert_no_error(error);
  if(!queued)while(!gimp_fill_brush_step(GIMP_FILL_BRUSH(core),1024,&error))g_assert_no_error(error);
  for(int n=1;n<=5;++n){c.x+=2;c.y+=.5;g_assert(motion(GIMP_FILL_BRUSH(core),&c,n*20,&error));g_assert_no_error(error);if(!queued)while(!gimp_fill_brush_step(GIMP_FILL_BRUSH(core),1024,&error))g_assert_no_error(error);}
  while(!gimp_fill_brush_step(GIMP_FILL_BRUSH(core),37,&error))g_assert_no_error(error);
  g_assert(gimp_fill_brush_finish(GIMP_FILL_BRUSH(core),TRUE,&error));g_assert_no_error(error);
  }
  dump(id,"finish",d);g_assert(gimp_image_undo(image));dump(id,"undo",d);g_assert(gimp_image_redo(image));dump(id,"redo",d);
  gimp_paint_core_cleanup(core);g_object_unref(core);g_object_unref(dyn);g_object_unref(brush);g_object_unref(options);g_object_unref(image);
 }puts("FILL_BRUSH_CAPTURE_COMPLETE");return 0;
}
