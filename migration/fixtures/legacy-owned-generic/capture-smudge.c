/* SPDX-License-Identifier: GPL-3.0-or-later
 * Executes unchanged pinned brush vfuncs through native old paint transactions. */
#include "config.h"
#include <gtk/gtk.h>
#include <gegl.h>
#include <stdio.h>
#include "libgimpbase/gimpbase.h"
#include "libgimpcolor/gimpcolor.h"
#include "libgimpmath/gimpmath.h"
#include "core/core-types.h"
#include "base/base-types.h"
#include "base/tile-manager.h"
#include "base/temp-buf.h"
#include "core/gimp.h"
#include "core/gimpcontext.h"
#include "core/gimpimage.h"
#include "core/gimpimage-undo.h"
#include "core/gimplayer.h"
#include "core/gimpbrush.h"
#include "core/gimpdynamics.h"
#include "core/gimpchannel.h"
#include "core/gimpchannel-select.h"
#include "paint/paint-types.h"
#include "paint/gimppaintcore.h"
#include "paint/gimppaintoptions.h"
#include "tests.h"
#define OWNED_GENERIC_OLD
#include "painter-owned-stroke-trace.h"
#include "core/gimpdynamicsoutput.h"
#include "paint/gimpsmudge.h"
#include "paint/gimpsmudgeoptions.h"
#define WIDTH 64
#define HEIGHT 48
static void dump(int id,const char*phase,GimpDrawable*d){
 int bytes=gimp_drawable_bytes(d);guchar p[WIDTH*HEIGHT*4];
 tile_manager_read_pixel_data(gimp_drawable_get_tiles(d),0,0,WIDTH-1,HEIGHT-1,p,WIDTH*bytes);
 printf("SMUDGE %d %s %d ",id,phase,bytes);
 for(unsigned i=0;i<WIDTH*HEIGHT*bytes;++i)printf("%02x",p[i]);puts("");
}
int main(void){
 const char*route=g_getenv("PAINTER_OWNED_GENERIC_ROUTE");g_assert(route);
 Gimp*gimp=gimp_init_for_testing();
 const GimpImageType types[]={GIMP_GRAY_IMAGE,GIMP_GRAYA_IMAGE,GIMP_RGB_IMAGE,GIMP_RGBA_IMAGE};
 for(int id=0;id<48;++id){
  const int shape=id/12,channels=shape+1,dynamic=id%2,blend=(id/2)%2,rate=((id/4)%3)*50;
  GimpImage*image=gimp_image_new(gimp,72,58,shape<2?GIMP_GRAY:GIMP_RGB);
  GimpLayer*layer=gimp_layer_new(image,WIDTH,HEIGHT,types[shape],"smudge-oracle",1,GIMP_NORMAL_MODE);
  gimp_image_add_layer(image,layer,NULL,0,FALSE);gimp_item_set_offset(GIMP_ITEM(layer),3,5);
  GimpDrawable*d=GIMP_DRAWABLE(layer);g_assert_cmpint(gimp_drawable_bytes(d),==,channels);
  guchar p[WIDTH*HEIGHT*4];
  for(int y=0;y<HEIGHT;++y)for(int x=0;x<WIDTH;++x){
   int i=(y*WIDTH+x)*channels;guchar v=x<32?45:205;
   if(channels<3)p[i]=v;else{p[i]=v;p[i+1]=255-v;p[i+2]=(x*3+y*5)%256;}
   if(channels%2==0)p[i+channels-1]=x<29?255:x<37?96:0;
  }
  tile_manager_write_pixel_data(gimp_drawable_get_tiles(d),0,0,WIDTH-1,HEIGHT-1,p,WIDTH*channels);
  GimpPaintOptions*options=g_object_new(GIMP_TYPE_SMUDGE_OPTIONS,"gimp",gimp,"rate",(double)rate,"use-color-blending",blend,"brush-size",17.0,NULL);
  GimpRGB fg={.8,.15,.35,1};gimp_context_set_foreground(GIMP_CONTEXT(options),&fg);gimp_context_set_opacity(GIMP_CONTEXT(options),.65);
  GimpBrush*brush=g_object_new(GIMP_TYPE_BRUSH,"name","smudge-mask",NULL);guchar zero=0;
  brush->mask=temp_buf_new(17,17,1,0,0,&zero);guchar*bm=temp_buf_get_data(brush->mask);
  for(int y=0;y<17;++y)for(int x=0;x<17;++x){int dx=x-8,dy=y-8;bm[y*17+x]=dx*dx+dy*dy<49?255:0;}
  gimp_context_set_brush(GIMP_CONTEXT(options),brush);
  GimpDynamics*dyn=g_object_new(GIMP_TYPE_DYNAMICS,"name","smudge-dynamics",NULL);
  g_object_set(gimp_dynamics_get_output(dyn,GIMP_DYNAMICS_OUTPUT_BLENDING),"use-pressure",TRUE,NULL);
  if(dynamic)g_object_set(gimp_dynamics_get_output(dyn,GIMP_DYNAMICS_OUTPUT_SIZE),"use-pressure",TRUE,NULL);
  gimp_context_set_dynamics(GIMP_CONTEXT(options),dyn);
  GimpPaintCore*core=g_object_new(GIMP_TYPE_SMUDGE,"undo-desc","smudge oracle",NULL);
  GimpCoords c=GIMP_COORDS_DEFAULT_VALUES;c.x=23;c.y=22;c.pressure=.7;GError*error=NULL;
  gimp_image_undo_free(image);dump(id,"initial",d);
  g_assert(owned_trace_stroke(route,core,d,options,c,TRUE,dynamic,&error));g_assert_no_error(error);
  dump(id,"finish",d);g_assert(gimp_image_undo(image));dump(id,"undo",d);g_assert(gimp_image_redo(image));dump(id,"redo",d);
  gimp_paint_core_cleanup(core);g_object_unref(core);g_object_unref(dyn);g_object_unref(brush);g_object_unref(options);g_object_unref(image);
 }
 puts("SMUDGE_CAPTURE_COMPLETE");return 0;
}
