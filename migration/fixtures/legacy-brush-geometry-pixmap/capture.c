/* SPDX-License-Identifier: GPL-3.0-or-later
 * Real unchanged pinned BrushCore mask dispatch and ordinary paint transactions. */
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
#include "core/gimpbrushgenerated.h"
#include "core/gimppattern.h"
#include "core/gimpdynamics.h"
#include "core/gimpchannel.h"
#include "core/gimpdynamicsoutput.h"
#include "paint/paint-types.h"
#include "paint/gimpbrushcore.h"
#include "paint/gimppaintbrush.h"
#include "paint/gimppaintoptions.h"
#include "tests.h"
static GimpPattern *paper(int channels) {
 GimpPattern*p=g_object_new(GIMP_TYPE_PATTERN,"name","paper oracle",NULL);
 if(p->mask)temp_buf_free(p->mask);
 p->mask=temp_buf_new(5,3,channels,0,0,NULL);guchar*d=temp_buf_get_data(p->mask);
 for(int y=0;y<3;++y)for(int x=0;x<5;++x)for(int b=0;b<channels;++b)d[(y*5+x)*channels+b]=(x*57+y*31+b*83)%256;
 return p;
}
static GimpBrush *brush(int shape) {
 if(shape>=3 && shape<6)return GIMP_BRUSH(gimp_brush_generated_new("generated paper brush",(GimpBrushGeneratedShape)(shape-3),8.75,3,.42,1.7,27));
 int w=shape==0?5:shape==1?4:17,h=shape==0?3:shape==1?4:17;
 GimpBrush*b=g_object_new(GIMP_TYPE_BRUSH,"name","paper brush",NULL);b->mask=temp_buf_new(w,h,1,0,0,NULL);guchar*d=temp_buf_get_data(b->mask);
 for(int y=0;y<h;++y)for(int x=0;x<w;++x)d[y*w+x]=(x*31+y*67+17)%256;
 if(shape==6){b->pixmap=temp_buf_new(w,h,3,0,0,NULL);guchar*p=temp_buf_get_data(b->pixmap);for(int i=0;i<w*h;++i)for(int c=0;c<3;++c)p[3*i+c]=(i*19+c*83)%256;}
 return b;
}
#define W 48
#define H 40
static void pixels(int id,const char*phase,GimpDrawable*d) {
 int bytes=gimp_drawable_bytes(d);guchar p[W*H*4];
 tile_manager_read_pixel_data(gimp_drawable_get_tiles(d),0,0,W-1,H-1,p,W*bytes);
 printf("GEOMETRY_STROKE %d %s %d ",id,phase,bytes);for(int i=0;i<W*H*bytes;++i)printf("%02x",p[i]);puts("");
}
static void strokes(Gimp*gimp) {
 const GimpImageType types[]={GIMP_GRAY_IMAGE,GIMP_GRAYA_IMAGE,GIMP_RGB_IMAGE,GIMP_RGBA_IMAGE};
 const int extended=0;
 const int mode_values[]={0,3,23,24,25,26,27,28,29};
 for(int id=0;id<48;++id){
  int shape=6, transform=id%4, mode=(id/4)%3, dynamic=(id/12)%2, textured=id/24;
  int ch=1+(id%4), incremental=(id/4)%2;
  const double sizes[]={17,10.5,24,17},angles[]={0,31,-45,123},aspects[]={0,0,3,-2};
  int raw_mode=extended?mode_values[(id/24)%9]:0;
  if((ch%2) && (raw_mode==27 || raw_mode==29))continue; /* old reversed-source destination channel mismatch */
  GimpImage*image=gimp_image_new(gimp,56,50,ch<3?GIMP_GRAY:GIMP_RGB);
  GimpLayer*layer=gimp_layer_new(image,W,H,types[ch-1],"paper oracle",1,GIMP_NORMAL_MODE);
  gimp_image_add_layer(image,layer,NULL,0,FALSE);gimp_item_set_offset(GIMP_ITEM(layer),3,5);
  GimpDrawable*d=GIMP_DRAWABLE(layer);guchar initial[W*H*4];
  for(int y=0;y<H;++y)for(int x=0;x<W;++x)for(int b=0;b<ch;++b)initial[(y*W+x)*ch+b]=(b==ch-1&&ch%2==0)?(x<24?255:96):(x*2+y*3+b*51)%256;
  tile_manager_write_pixel_data(gimp_drawable_get_tiles(d),0,0,W-1,H-1,initial,W*ch);

  if(extended && id>=216){
    guchar selection[56*50];for(int y=0;y<50;++y)for(int x=0;x<56;++x)selection[y*56+x]=(x<14||x>40||y<10||y>40)?0:(x+y)%3==0?255:(x+y)%3==1?128:37;
    GimpChannel*channel=gimp_image_get_mask(image);tile_manager_write_pixel_data(gimp_drawable_get_tiles(GIMP_DRAWABLE(channel)),0,0,55,49,selection,56);channel->bounds_known=FALSE;channel->boundary_known=FALSE;
  }
  GimpPaintOptions*o=g_object_new(GIMP_TYPE_PAINT_OPTIONS,"gimp",gimp,"use-texture",textured,"brush-size",sizes[transform],"brush-angle",angles[transform],"brush-aspect-ratio",aspects[transform],"hard",mode==1,"application-mode",incremental?GIMP_PAINT_INCREMENTAL:GIMP_PAINT_CONSTANT,NULL);
  GimpBrush*b=brush(shape);GimpPattern*p=paper(ch);GimpDynamics*dyn=g_object_new(GIMP_TYPE_DYNAMICS,"name","paper dynamics",NULL);
  if(dynamic){
   g_object_set(gimp_dynamics_get_output(dyn,GIMP_DYNAMICS_OUTPUT_SIZE),"use-pressure",TRUE,NULL);
   g_object_set(gimp_dynamics_get_output(dyn,GIMP_DYNAMICS_OUTPUT_HARDNESS),"use-pressure",TRUE,NULL);
   g_object_set(gimp_dynamics_get_output(dyn,GIMP_DYNAMICS_OUTPUT_ANGLE),"use-tilt",TRUE,NULL);
   g_object_set(gimp_dynamics_get_output(dyn,GIMP_DYNAMICS_OUTPUT_ASPECT_RATIO),"use-velocity",TRUE,NULL);
  }
  if(mode==2)g_object_set(gimp_dynamics_get_output(dyn,GIMP_DYNAMICS_OUTPUT_FORCE),"use-pressure",TRUE,NULL);
  gimp_context_set_brush(GIMP_CONTEXT(o),b);gimp_context_set_pattern(GIMP_CONTEXT(o),p);gimp_context_set_dynamics(GIMP_CONTEXT(o),dyn);
  GimpRGB fg={.8,.15,.35,1};gimp_context_set_foreground(GIMP_CONTEXT(o),&fg);gimp_context_set_opacity(GIMP_CONTEXT(o),.65);gimp_context_set_paint_mode(GIMP_CONTEXT(o),(GimpLayerModeEffects)raw_mode);
  GimpPaintCore*core=g_object_new(GIMP_TYPE_PAINTBRUSH,"undo-desc","paper oracle",NULL);GimpCoords c=GIMP_COORDS_DEFAULT_VALUES;c.x=18;c.y=19;c.pressure=.37;c.xtilt=.2;c.ytilt=-.3;c.velocity=.4;GError*error=NULL;
  gimp_image_undo_free(image);pixels(id,"initial",d);g_assert(gimp_paint_core_start(core,d,o,&c,&error));g_assert_no_error(error);
  gimp_paint_core_paint(core,d,o,GIMP_PAINT_STATE_INIT,0);gimp_paint_core_paint(core,d,o,GIMP_PAINT_STATE_MOTION,0);gimp_paint_core_set_last_coords(core,&c);
  for(int n=1;n<=5;++n){c.x+=2.5;c.y+=(n%2?1:-1);c.pressure=n%2?.7:.37;c.xtilt+=.03;c.ytilt+=.05;c.velocity=n*.1;gimp_paint_core_interpolate(core,d,o,&c,n*20);}
  gimp_paint_core_paint(core,d,o,GIMP_PAINT_STATE_FINISH,120);gimp_paint_core_finish(core,d,TRUE);
  pixels(id,"finish",d);g_assert(gimp_image_undo(image));pixels(id,"undo",d);g_assert(gimp_image_redo(image));pixels(id,"redo",d);
  gimp_paint_core_cleanup(core);g_object_unref(core);g_object_unref(dyn);g_object_unref(p);g_object_unref(b);g_object_unref(o);g_object_unref(image);
 }
}
int main(void){Gimp*gimp=gimp_init_for_testing();strokes(gimp);puts("GEOMETRY_CAPTURE_COMPLETE");return 0;}
