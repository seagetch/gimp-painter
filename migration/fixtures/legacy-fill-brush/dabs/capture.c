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
#include "paint/gimpbrushcore.h"
#include "paint/gimppaintoptions.h"
#include "tests.h"
void gimp_bucket_fill_brush_tool_register (gpointer unused, gpointer gimp);
#define WIDTH 90
#define HEIGHT 70
static void dump(int id,const char*phase,GimpDrawable*d){guchar p[WIDTH*HEIGHT*4];tile_manager_read_pixel_data(gimp_drawable_get_tiles(d),0,0,WIDTH-1,HEIGHT-1,p,WIDTH*4);printf("BRUSH %d %s ",id,phase);for(unsigned i=0;i<sizeof p;++i)printf("%02x",p[i]);puts("");}
static void (*original_paint)(GimpPaintCore*,GimpDrawable*,GimpPaintOptions*,const GimpCoords*,GimpPaintState,guint32);
static int current_case;
static void observe_paint(GimpPaintCore*c,GimpDrawable*d,GimpPaintOptions*o,const GimpCoords*xy,GimpPaintState state,guint32 time){
 if(state==GIMP_PAINT_STATE_MOTION)printf("DAB %d %.17g %.17g %.17g %.17g\n",current_case,xy->x,xy->y,GIMP_BRUSH_CORE(c)->spacing,GIMP_BRUSH_CORE(c)->scale);
 original_paint(c,d,o,xy,state,time);
}
int main(void){
 Gimp*gimp=gimp_init_for_testing();gimp_bucket_fill_brush_tool_register(NULL,gimp);
 GType type=g_type_from_name("GimpBucketFillBrush"),options_type=g_type_from_name("GimpBucketFillBrushOptions");g_assert(type);g_assert(options_type);GimpPaintCoreClass*klass=g_type_class_ref(type);original_paint=klass->paint;klass->paint=observe_paint;
 for(int id=0;id<12;++id){current_case=id;
  GimpImage*image=gimp_image_new(gimp,100,80,GIMP_RGB);GimpLayer*layer=gimp_layer_new(image,WIDTH,HEIGHT,GIMP_RGBA_IMAGE,"fill-oracle",1,GIMP_NORMAL_MODE);gimp_image_add_layer(image,layer,NULL,0,FALSE);gimp_item_set_offset(GIMP_ITEM(layer),3,5);GimpDrawable*d=GIMP_DRAWABLE(layer);
  guchar p[WIDTH*HEIGHT*4];for(int y=0;y<HEIGHT;++y)for(int x=0;x<WIDTH;++x){int i=(y*WIDTH+x)*4;guchar v=(x==47&&y>4&&y<65)?200:70;p[i]=v;p[i+1]=v+15;p[i+2]=v+30;p[i+3]=255;}tile_manager_write_pixel_data(gimp_drawable_get_tiles(d),0,0,WIDTH-1,HEIGHT-1,p,WIDTH*4);
  if(id/6)gimp_channel_select_rectangle(gimp_image_get_mask(image),40,0,60,80,GIMP_CHANNEL_OP_REPLACE,FALSE,0,0,FALSE);
  GimpPaintOptions*options=g_object_new(options_type,"gimp",gimp,"rate",(double)((id%3)*50),"eraser-mode",(id/3)%2,"brush-size",23.0,NULL);
  GimpRGB fg={.8,.15,.35,1},bg={.1,.2,.3,1};gimp_context_set_foreground(GIMP_CONTEXT(options),&fg);gimp_context_set_background(GIMP_CONTEXT(options),&bg);gimp_context_set_opacity(GIMP_CONTEXT(options),.65);
  GimpBrush*brush=g_object_new(GIMP_TYPE_BRUSH,"name","fill-mask",NULL);guchar zero=0;brush->mask=temp_buf_new(23,23,1,0,0,&zero);guchar*bm=temp_buf_get_data(brush->mask);for(int y=0;y<23;++y)for(int x=0;x<23;++x){int dx=x-11,dy=y-11;bm[y*23+x]=dx*dx+dy*dy<90?255:0;}
  gimp_context_set_brush(GIMP_CONTEXT(options),brush);GimpDynamics*dyn=g_object_new(GIMP_TYPE_DYNAMICS,"name","fill-no-dynamics",NULL);gimp_context_set_dynamics(GIMP_CONTEXT(options),dyn);
  GimpPaintCore*core=g_object_new(type,"undo-desc","fill oracle",NULL);GimpCoords c=GIMP_COORDS_DEFAULT_VALUES;c.x=40;c.y=35;c.pressure=1;GError*error=NULL;
  gimp_image_undo_free(image);g_assert(gimp_paint_core_start(core,d,options,&c,&error));g_assert_no_error(error);gimp_paint_core_paint(core,d,options,GIMP_PAINT_STATE_INIT,0);gimp_paint_core_paint(core,d,options,GIMP_PAINT_STATE_MOTION,0);gimp_paint_core_set_last_coords(core,&c);
  for(int n=1;n<=5;++n){c.x+=2;c.y+=.5;gimp_paint_core_interpolate(core,d,options,&c,n*20);}
  gimp_paint_core_paint(core,d,options,GIMP_PAINT_STATE_FINISH,120);gimp_paint_core_finish(core,d,TRUE);dump(id,"finish",d);g_assert(gimp_image_undo(image));dump(id,"undo",d);g_assert(gimp_image_redo(image));dump(id,"redo",d);
  gimp_paint_core_cleanup(core);g_object_unref(core);g_object_unref(dyn);g_object_unref(brush);g_object_unref(options);g_object_unref(image);
 }puts("FILL_BRUSH_CAPTURE_COMPLETE");return 0;
}
