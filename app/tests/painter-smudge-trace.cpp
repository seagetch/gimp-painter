/* SPDX-License-Identifier: GPL-3.0-or-later
 * Executes unchanged pinned brush vfuncs through native old paint transactions. */
#include "config.h"
#include <gtk/gtk.h>
#include <gegl.h>
#include <stdio.h>
#include <cstring>
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
#include "core/gimpdynamics.h"
#include "core/gimppattern.h"
#include "core/gimpchannel.h"
#include "core/gimpchannel-select.h"
#include "paint/paint-types.h"
#include "paint/gimppaintcore.h"
#include "paint/gimppaintoptions.h"
#include "tests.h"
#include "core/gimpdynamicsoutput.h"
#include "paint/gimppaintersmudge.h"
#include "paint/gimpsmudgeoptions.h"
#include "painter-owned-stroke-trace.h"
}
#define WIDTH 64
#define HEIGHT 48
static void dump(int id,const char*phase,GimpDrawable*d){
 int bytes=((gimp_drawable_is_gray(d)?1:3)+gimp_drawable_has_alpha(d));guchar p[WIDTH*HEIGHT*4];
 gegl_buffer_get(gimp_drawable_get_buffer(d),GEGL_RECTANGLE(0,0,WIDTH,HEIGHT),1,gimp_drawable_get_format(d),p,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
 printf("SMUDGE %d %s %d ",id,phase,bytes);
 for(int i=0;i<WIDTH*HEIGHT*bytes;++i)printf("%02x",p[i]);
 puts("");
}
int main(int argc,char**argv){
 const char*generic_route=g_getenv("PAINTER_OWNED_GENERIC_ROUTE");
 const bool use_paper=g_getenv("PAINTER_SMUDGE_PAPER_FIXTURE")!=nullptr;
 const bool queued=argc>1&&std::strcmp(argv[1],"owned")==0;
 Gimp*gimp=gimp_init_for_testing();
 const char* formats[]={"Y' u8","Y'A u8","R'G'B' u8","R'G'B'A u8"};
 for(int id=0;id<48;++id){
  const int shape=id/12,channels=shape+1,dynamic=id%2,blend=(id/2)%2,rate=((id/4)%3)*50;
  GimpImage*image=gimp_image_new(gimp,72,58,shape<2?GIMP_GRAY:GIMP_RGB,GIMP_PRECISION_U8_NON_LINEAR);
  GimpLayer*layer=gimp_layer_new(image,WIDTH,HEIGHT,babl_format(formats[shape]),"smudge-oracle",1,GIMP_LAYER_MODE_PAINTER_NORMAL);
  gimp_image_add_layer(image,layer,NULL,0,FALSE);gimp_item_set_offset(GIMP_ITEM(layer),3,5);
  GimpDrawable*d=GIMP_DRAWABLE(layer);g_assert_cmpint(((gimp_drawable_is_gray(d)?1:3)+gimp_drawable_has_alpha(d)),==,channels);
  guchar p[WIDTH*HEIGHT*4];
  for(int y=0;y<HEIGHT;++y)for(int x=0;x<WIDTH;++x){
   int i=(y*WIDTH+x)*channels;guchar v=x<32?45:205;
   if(channels<3)p[i]=v;else{p[i]=v;p[i+1]=255-v;p[i+2]=(x*3+y*5)%256;}
   if(channels%2==0)p[i+channels-1]=x<29?255:x<37?96:0;
  }
  gegl_buffer_set(gimp_drawable_get_buffer(d),GEGL_RECTANGLE(0,0,WIDTH,HEIGHT),0,gimp_drawable_get_format(d),p,GEGL_AUTO_ROWSTRIDE);
  GimpPaintOptions*options=GIMP_PAINT_OPTIONS(g_object_new(GIMP_TYPE_PAINTER_SMUDGE_OPTIONS,"gimp",gimp,"rate",(double)rate,"use-color-blending",blend,"brush-size",17.0,NULL));
  double rgba[4]={.8,.15,.35,1};GeglColor*fg=gegl_color_new(NULL);gegl_color_set_pixel(fg,babl_format("R'G'B'A double"),rgba);gimp_context_set_foreground(GIMP_CONTEXT(options),fg);g_object_unref(fg);gimp_context_set_opacity(GIMP_CONTEXT(options),.65);
  GimpBrush*brush=GIMP_BRUSH(g_object_new(GIMP_TYPE_BRUSH,"name","smudge-mask",NULL));
  brush->priv->mask=gimp_temp_buf_new(17,17,babl_format("Y u8"));guchar*bm=gimp_temp_buf_get_data(brush->priv->mask);
  for(int y=0;y<17;++y)for(int x=0;x<17;++x){int dx=x-8,dy=y-8;bm[y*17+x]=dx*dx+dy*dy<49?255:0;}
  gimp_context_set_brush(GIMP_CONTEXT(options),brush);
  if(use_paper){
    g_object_set(options,"use-texture",TRUE,nullptr);
    auto*paper=GIMP_PATTERN(g_object_new(GIMP_TYPE_PATTERN,"name","smudge paper",nullptr));
    g_clear_pointer(&paper->mask,gimp_temp_buf_unref);paper->mask=gimp_temp_buf_new(5,3,babl_format(formats[shape]));
    auto*pm=gimp_temp_buf_get_data(paper->mask);for(int y=0;y<3;++y)for(int x=0;x<5;++x)for(int b=0;b<channels;++b)pm[(y*5+x)*channels+b]=(x*57+y*31+b*83)%256;
    gimp_context_set_pattern(GIMP_CONTEXT(options),paper);g_object_unref(paper);
  }

  GimpDynamics*dyn=GIMP_DYNAMICS(g_object_new(GIMP_TYPE_DYNAMICS,"name","smudge-dynamics",NULL));
  g_object_set(gimp_dynamics_get_output(dyn,GIMP_DYNAMICS_OUTPUT_BLENDING),"use-pressure",TRUE,NULL);
  if(dynamic)g_object_set(gimp_dynamics_get_output(dyn,GIMP_DYNAMICS_OUTPUT_SIZE),"use-pressure",TRUE,NULL);
  gimp_context_set_dynamics(GIMP_CONTEXT(options),dyn);
  GimpPaintCore*core=GIMP_PAINT_CORE(g_object_new(GIMP_TYPE_PAINTER_SMUDGE,"undo-desc","smudge oracle",NULL));GList list={d,NULL,NULL};
  GimpCoords c=GIMP_COORDS_DEFAULT_VALUES;c.x=26;c.y=27;c.pressure=.7;GError*error=NULL;
  gimp_image_undo_free(image);dump(id,"initial",d);
  if(generic_route){gboolean result=owned_trace_stroke(generic_route,core,d,options,c,TRUE,dynamic,&error);if(error)g_printerr("GENERIC_FAILURE scene %d: %s\n",id,error->message);g_assert_no_error(error);g_assert(result);}
  else {
  g_assert(gimp_painter_smudge_begin(GIMP_PAINTER_SMUDGE(core),d,options,&c,&error));g_assert_no_error(error);
  auto move=[&](guint32 time){
    if(queued){g_assert(gimp_painter_smudge_motion_begin(GIMP_PAINTER_SMUDGE(core),&c,time,&error));g_assert_no_error(error);while(!gimp_painter_smudge_step(GIMP_PAINTER_SMUDGE(core),&error)){g_assert_no_error(error);}g_assert_no_error(error);}
    else{g_assert(gimp_painter_smudge_motion(GIMP_PAINTER_SMUDGE(core),&c,time,&error));g_assert_no_error(error);}
  };
  move(0);
  for(int n=1;n<=7;++n){c.x+=2.5;c.y+=(n%2?1:-1);c.pressure=dynamic?(n%3==0?.25:n%3==1?1:.6):.7;move(n*20);}
  g_assert(gimp_painter_smudge_finish(GIMP_PAINTER_SMUDGE(core),TRUE,&error));g_assert_no_error(error);
  gchar*message=gimp_painter_smudge_dup_error(GIMP_PAINTER_SMUDGE(core));if(message)g_error("Smudge error: %s",message);
  }
  dump(id,"finish",d);g_assert(gimp_image_undo(image));dump(id,"undo",d);g_assert(gimp_image_redo(image));dump(id,"redo",d);
  gimp_paint_core_cleanup(core);g_object_unref(core);g_object_unref(dyn);g_object_unref(brush);g_object_unref(options);g_object_unref(image);
 }
 puts("SMUDGE_CAPTURE_COMPLETE");return 0;
}
