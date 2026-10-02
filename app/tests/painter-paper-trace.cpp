/* SPDX-License-Identifier: GPL-3.0-or-later
 * Real unchanged pinned BrushCore mask dispatch and ordinary paint transactions. */
#include "config.h"
#include <gtk/gtk.h>
#include <gegl.h>
#include <stdio.h>
extern "C" {
#include "libgimpbase/gimpbase.h"
#include "libgimpcolor/gimpcolor.h"
#include "libgimpmath/gimpmath.h"
#include "core/core-types.h"
#include "core/gimptempbuf.h"
#include "core/gimpbrush-private.h"
#include "core/gimp.h"
#include "core/gimpcontext.h"
#include "core/gimpimage.h"
#include "core/gimpimage-undo.h"
#include "core/gimplayer.h"
#include "core/gimplayer-new.h"
#include "core/gimpbrush.h"
#include "core/gimpbrushgenerated.h"
#include "core/gimppattern.h"
#include "core/gimpdynamics.h"
#include "core/gimpchannel.h"
#include "core/gimpdynamicsoutput.h"
#include "paint/paint-types.h"
#include "paint/gimpbrushcore.h"
#include "paint/gimppaintbrush.h"
#include "operations/layer-modes-legacy/gimpoperationpainterlegacy.h"
#include "paint/gimppaintoptions.h"
#include "tests.h"
}
#include <vector>
static GimpPattern *paper(int channels) {
 GimpPattern*p=GIMP_PATTERN(g_object_new(GIMP_TYPE_PATTERN,"name","paper oracle",NULL));
 if(p->mask)gimp_temp_buf_unref(p->mask);
 const char*formats[]={"Y' u8","Y'A u8","R'G'B' u8","R'G'B'A u8"};
 p->mask=gimp_temp_buf_new(5,3,babl_format(formats[channels-1]));guchar*d=gimp_temp_buf_get_data(p->mask);
 for(int y=0;y<3;++y)for(int x=0;x<5;++x)for(int b=0;b<channels;++b)d[(y*5+x)*channels+b]=(x*57+y*31+b*83)%256;
 return p;
}
static GimpBrush *brush(int shape) {
 if(shape>=3)return GIMP_BRUSH(gimp_brush_generated_new("generated paper brush",(GimpBrushGeneratedShape)(shape-3),8.75,3,.42,1.7,27));
 int w=shape==0?5:shape==1?4:17,h=shape==0?3:shape==1?4:17;
 GimpBrush*b=GIMP_BRUSH(g_object_new(GIMP_TYPE_BRUSH,"name","paper brush",NULL));b->priv->mask=gimp_temp_buf_new(w,h,babl_format("Y u8"));guchar*d=gimp_temp_buf_get_data(b->priv->mask);
 for(int y=0;y<h;++y)for(int x=0;x<w;++x)d[y*w+x]=(x*31+y*67+17)%256;
 return b;
}
static void masks(void) {
 int id=0,extended=g_getenv("PAINTER_PAPER_TRANSFORM_FIXTURE")!=NULL;
 const double xs[]={20,20.25,20.5,20.9,-1,-.25};
 for(int ch=1;ch<=4;++ch)for(int shape=0;shape<(extended?6:3);++shape)for(int transform=0;transform<(extended?3:1);++transform)for(int mode=0;mode<3;++mode)for(int pos=0;pos<6;++pos,++id){
  GimpBrushCore*core=GIMP_BRUSH_CORE(g_object_new(GIMP_TYPE_PAINTBRUSH,NULL));GimpBrush*b=brush(shape);GimpPattern*p=paper(ch);
  gimp_brush_core_set_brush(core,b);core->brush=b;core->scale=transform==1?.65:transform==2?1.4:1;core->hardness=1;core->angle=transform==2?.125:0;core->aspect_ratio=transform==2?3:0;
  if(shape>=3){auto*generated=GIMP_BRUSH_GENERATED(b);core->hardness=gimp_brush_generated_get_hardness(generated);core->angle+=gimp_brush_generated_get_angle(generated)/360.0;core->aspect_ratio=transform==2?3.0*20/19:(gimp_brush_generated_get_aspect_ratio(generated)-1)*20/19;}
  GimpCoords c=GIMP_COORDS_DEFAULT_VALUES;c.x=xs[pos];c.y=20.25;
  for(int textured=0;textured<2;++textured){
   gimp_brush_core_set_texture(core,textured?p:NULL);
   const GimpTempBuf*m=gimp_brush_core_get_brush_mask(core,&c,(GimpBrushApplicationMode)mode,.37);
   g_assert(m);
   printf("PAPER_MASK %d %d %d %d %d %.17g %.17g ",id,textured,ch,gimp_temp_buf_get_width(m),gimp_temp_buf_get_height(m),c.x,c.y);
   std::vector<guchar>bytes(gimp_temp_buf_get_width(m)*gimp_temp_buf_get_height(m));babl_process(babl_fish(gimp_temp_buf_get_format(m),babl_format("Y u8")),gimp_temp_buf_get_data(m),bytes.data(),bytes.size());const guchar*d=bytes.data();for(int i=0;i<gimp_temp_buf_get_width(m)*gimp_temp_buf_get_height(m);++i)printf("%02x",d[i]);puts("");
  }
  g_object_unref(core);g_object_unref(b);g_object_unref(p);
 }
}
#define W 48
#define H 40
static void pixels(int id,const char*phase,GimpDrawable*d) {
 int bytes=(gimp_drawable_is_gray(d)?1:3)+gimp_drawable_has_alpha(d);guchar p[W*H*4];
 gegl_buffer_get(gimp_drawable_get_buffer(d),GEGL_RECTANGLE(0,0,W,H),1,gimp_drawable_get_format(d),p,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
 printf("PAPER_STROKE %d %s %d ",id,phase,bytes);for(int i=0;i<W*H*bytes;++i)printf("%02x",p[i]);puts("");
}
static void strokes(Gimp*gimp) {
 const char*types[]={"Y' u8","Y'A u8","R'G'B' u8","R'G'B'A u8"};
 const int extended=g_getenv("PAINTER_PAPER_MODES_FIXTURE")!=NULL;
 const int mode_values[]={0,3,23,24,25,26,27,28,29};
 for(int id=0;id<(extended?432:24);++id){
  int base=id%24,ch=base/6+1,incremental=(base/3)%2,mode=base%3;
  int raw_mode=extended?mode_values[(id/24)%9]:0;
  if((ch%2) && (raw_mode==27 || raw_mode==29))continue; /* old reversed-source destination channel mismatch */
  GimpImage*image=gimp_image_new(gimp,56,50,ch<3?GIMP_GRAY:GIMP_RGB,GIMP_PRECISION_U8_NON_LINEAR);
  GimpLayer*layer=gimp_layer_new(image,W,H,babl_format(types[ch-1]),"paper oracle",1,GIMP_LAYER_MODE_PAINTER_NORMAL);
  gimp_image_add_layer(image,layer,NULL,0,FALSE);gimp_item_set_offset(GIMP_ITEM(layer),3,5);
  GimpDrawable*d=GIMP_DRAWABLE(layer);guchar initial[W*H*4];
  for(int y=0;y<H;++y)for(int x=0;x<W;++x)for(int b=0;b<ch;++b)initial[(y*W+x)*ch+b]=(b==ch-1&&ch%2==0)?(x<24?255:96):(x*2+y*3+b*51)%256;
  gegl_buffer_set(gimp_drawable_get_buffer(d),GEGL_RECTANGLE(0,0,W,H),0,gimp_drawable_get_format(d),initial,GEGL_AUTO_ROWSTRIDE);

  if(extended && id>=216){
    guchar selection[56*50];for(int y=0;y<50;++y)for(int x=0;x<56;++x)selection[y*56+x]=(x<14||x>40||y<10||y>40)?0:(x+y)%3==0?255:(x+y)%3==1?128:37;
    GimpChannel*channel=gimp_image_get_mask(image);gegl_buffer_set(gimp_drawable_get_buffer(GIMP_DRAWABLE(channel)),GEGL_RECTANGLE(0,0,56,50),0,babl_format("Y u8"),selection,GEGL_AUTO_ROWSTRIDE);channel->bounds_known=FALSE;channel->boundary_known=FALSE;
  }
  GimpPaintOptions*o=GIMP_PAINT_OPTIONS(g_object_new(GIMP_TYPE_PAINT_OPTIONS,"gimp",gimp,"use-texture",TRUE,"brush-size",17.0,"hard",mode==1,"application-mode",incremental?GIMP_PAINT_INCREMENTAL:GIMP_PAINT_CONSTANT,NULL));
  GimpBrush*b=brush(2);GimpPattern*p=paper(ch);GimpDynamics*dyn=GIMP_DYNAMICS(g_object_new(GIMP_TYPE_DYNAMICS,"name","paper dynamics",NULL));
  if(mode==2)g_object_set(gimp_dynamics_get_output(dyn,GIMP_DYNAMICS_OUTPUT_FORCE),"use-pressure",TRUE,NULL);
  gimp_context_set_brush(GIMP_CONTEXT(o),b);g_object_set(o,"brush-spacing",gimp_brush_get_spacing(b)/100.0,nullptr);gimp_context_set_pattern(GIMP_CONTEXT(o),p);gimp_context_set_dynamics(GIMP_CONTEXT(o),dyn);
  double rgba[]={.8,.15,.35,1};GeglColor*fg=gegl_color_new(NULL);gegl_color_set_pixel(fg,babl_format("R'G'B'A double"),rgba);gimp_context_set_foreground(GIMP_CONTEXT(o),fg);g_object_unref(fg);GimpLayerMode paint_mode;g_assert(gimp_painter_layer_mode_from_legacy(raw_mode,&paint_mode));gimp_context_set_paint_mode(GIMP_CONTEXT(o),paint_mode);gimp_context_set_opacity(GIMP_CONTEXT(o),.65);
  GimpPaintCore*core=GIMP_PAINT_CORE(g_object_new(GIMP_TYPE_PAINTBRUSH,"undo-desc","paper oracle",NULL));GList list={d,NULL,NULL};GimpCoords c=GIMP_COORDS_DEFAULT_VALUES;c.x=21;c.y=24;c.pressure=.37;GError*error=NULL;
  gimp_image_undo_free(image);pixels(id,"initial",d);g_assert(gimp_paint_core_start(core,&list,o,&c,&error));g_assert_no_error(error);
  gimp_paint_core_paint(core,&list,o,GIMP_PAINT_STATE_INIT,0);gimp_paint_core_paint(core,&list,o,GIMP_PAINT_STATE_MOTION,0);gimp_paint_core_set_last_coords(core,&c);
  for(int n=1;n<=5;++n){c.x+=2.5;c.y+=(n%2?1:-1);c.pressure=n%2?.7:.37;gimp_paint_core_interpolate(core,&list,o,&c,n*20);}
  gimp_paint_core_paint(core,&list,o,GIMP_PAINT_STATE_FINISH,120);gimp_paint_core_finish(core,&list,TRUE);
  pixels(id,"finish",d);g_assert(gimp_image_undo(image));pixels(id,"undo",d);g_assert(gimp_image_redo(image));pixels(id,"redo",d);
  gimp_paint_core_cleanup(core);g_object_unref(core);g_object_unref(dyn);g_object_unref(p);g_object_unref(b);g_object_unref(o);g_object_unref(image);
 }
}
int main(void){Gimp*gimp=gimp_init_for_testing();if(!g_getenv("PAINTER_PAPER_MODES_FIXTURE"))masks();if(!g_getenv("PAINTER_PAPER_TRANSFORM_FIXTURE"))strokes(gimp);puts("PAPER_CAPTURE_COMPLETE");return 0;}
