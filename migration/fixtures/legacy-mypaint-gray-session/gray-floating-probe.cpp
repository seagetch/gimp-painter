/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Independent native Gray-u8: 48 supported incremental scenes, three blend states.
 * Pinned real GimpMypaintCore + drawable + Undo, synthetic deterministic stimuli.
 * This capture links old application archives; no port evaluator participates. */
#include <cstdio>
#include <vector>
#include <cstring>
extern "C" {
#include "config.h"
#include <gtk/gtk.h>
#include <gegl.h>
#include "libgimpbase/gimpbase.h"
#include "libgimpcolor/gimpcolor.h"
#include "libgimpmath/gimpmath.h"
#include "core/core-types.h"
#include "base/base-types.h"
#include "base/temp-buf.h"
#include "base/tile-manager.h"
#include "core/gimp.h"
#include "core/gimpcontext.h"
#include "core/gimpimage.h"
#include "core/gimpimage-undo.h"
#include "core/gimpundostack.h"
#include "core/gimplayer.h"
#include "core/gimpbrush.h"
#include "core/gimppattern.h"
#include "core/gimpchannel.h"
#include "core/gimpchannel-select.h"
#include "core/gimpmypaintbrush.h"
#include "core/gimpmypaintbrush-load.h"
#include "paint/gimpmypaintoptions.h"
#include "tests.h"
}
#include "core/gimpmypaintbrush-private.hpp"
#include "paint/gimpmypaintcore.hpp"
static constexpr int width=130,height=96;
static std::vector<guchar> pixels(GimpDrawable*d)
{
  std::vector<guchar> p(width*height);
  tile_manager_read_pixel_data(gimp_drawable_get_tiles(d),0,0,width-1,height-1,p.data(),width);return p;
}
static void dump(int scenario,const char*phase,GimpDrawable*d)
{
  auto p=pixels(d);std::printf("GRAY_SESSION_PIX %d %s ",scenario,phase);
  for(auto c:p)std::printf("%02x",c);std::puts("");
}
static void capture(Gimp*gimp,int scenario)
{
  const int blend=scenario/16;
  const bool floating=true,shape=scenario&1,paper=scenario&2,selection=scenario&4,smudge=scenario&8;
  auto*image=gimp_image_new(gimp,150,110,GIMP_GRAY);
  auto*layer=gimp_layer_new(image,width,height,GIMP_GRAY_IMAGE,"session-target",1,GIMP_NORMAL_MODE);
  g_assert(gimp_image_add_layer(image,layer,nullptr,0,FALSE));gimp_layer_set_lock_alpha(layer,blend==2,FALSE);gimp_item_set_offset(GIMP_ITEM(layer),7,3);
  auto*drawable=GIMP_DRAWABLE(layer);std::vector<guchar> initial(width*height);
  for(int y=0;y<height;++y)for(int x=0;x<width;++x){int i=(y*width+x);initial[i]=(x*19+y*7)%256;}
  tile_manager_write_pixel_data(gimp_drawable_get_tiles(drawable),0,0,width-1,height-1,initial.data(),width);
  if(selection)gimp_channel_select_rectangle(gimp_image_get_mask(image),70,0,80,110,GIMP_CHANNEL_OP_REPLACE,FALSE,0,0,FALSE);
  auto*options=GIMP_MYPAINT_OPTIONS(g_object_new(GIMP_TYPE_MYPAINT_OPTIONS,"gimp",gimp,nullptr));
  GimpRGB fg={.8,.2,.4,1},bg={.1,.2,.3,1};gimp_context_set_foreground(GIMP_CONTEXT(options),&fg);gimp_context_set_background(GIMP_CONTEXT(options),&bg);
  if(options->brush)g_object_unref(options->brush);
  GError*error=nullptr;auto*loaded=gimp_mypaint_brush_load(GIMP_CONTEXT(options),"/workspace/shared/painter-mypaint-gray-oracle/session-base.myb",&error);
  g_assert_no_error(error);g_assert(loaded);options->brush=GIMP_MYPAINT_BRUSH(loaded->data);g_list_free(loaded);
  auto*settings=static_cast<GimpMypaintBrushPrivate*>(options->brush->p);
  settings->set_base_value(BRUSH_DABS_PER_SECOND,40);settings->set_base_value(BRUSH_RADIUS_LOGARITHMIC,2.3);
  settings->set_bool_value(BRUSH_NON_INCREMENTAL,false);settings->set_base_value(BRUSH_STROKE_OPACITY,smudge?.37:1.0);
  settings->set_base_value(BRUSH_ERASER,blend==1?.65:0);settings->set_base_value(BRUSH_LOCK_ALPHA,blend==2?.7:0);
  settings->set_base_value(BRUSH_SMUDGE,smudge?.8:0);settings->set_base_value(BRUSH_SMUDGE_LENGTH,.65);
  settings->set_bool_value(BRUSH_USE_GIMP_BRUSHMARK,shape);settings->set_bool_value(BRUSH_USE_GIMP_TEXTURE,paper);
  GimpBrush*brush=nullptr;GimpPattern*pattern=nullptr;guchar clear[4]={0,0,0,0};
  if(shape){brush=GIMP_BRUSH(g_object_new(GIMP_TYPE_BRUSH,"name","session-bitmap",nullptr));brush->mask=temp_buf_new(5,3,1,0,0,clear);const guchar mask[]={0,30,90,160,255,12,60,120,180,240,255,190,140,70,0};std::memcpy(temp_buf_get_data(brush->mask),mask,sizeof mask);gimp_context_set_brush(GIMP_CONTEXT(options),brush);}
  if(paper){pattern=GIMP_PATTERN(g_object_new(GIMP_TYPE_PATTERN,"name","session-paper",nullptr));pattern->mask=temp_buf_new(3,2,3,0,0,clear);const guchar texture[]={20,250,10,100,0,50,240,2,180,200,12,33,64,25,70,128,100,5};std::memcpy(temp_buf_get_data(pattern->mask),texture,sizeof texture);gimp_context_set_pattern(GIMP_CONTEXT(options),pattern);}
  gimp_image_undo_free(image);
  {
    GimpMypaintCore core;
    // The real old nonincremental begin path expects a previously refreshed
    // drawable feature. Establish it through actual painting + Undo, no patch.
    GimpCoords warm=GIMP_COORDS_DEFAULT_VALUES;warm.x=50;warm.y=35;warm.pressure=0;
    core.stroke_to(drawable,.01,&warm,options);warm.pressure=.85;core.stroke_to(drawable,.1,&warm,options);core.split_stroke();
    g_assert(gimp_image_undo(image));g_assert(pixels(drawable)==initial);gimp_image_undo_free(image);
    core.reset_brush();settings->set_bool_value(BRUSH_NON_INCREMENTAL,floating);g_object_notify(G_OBJECT(options),"non-incremental");
    GimpCoords coords=GIMP_COORDS_DEFAULT_VALUES;coords.x=60;coords.y=60;coords.pressure=0;
    core.stroke_to(drawable,.01,&coords,options);coords.pressure=.85;core.stroke_to(drawable,.075,&coords,options);
    for(int i=0;i<16;++i){coords.x+=2;coords.y+=.75;coords.pressure=i%3?.85:.45;core.stroke_to(drawable,.012,&coords,options);}
    core.split_stroke();
    dump(scenario,"finish",drawable);
    auto after=pixels(drawable);
    gboolean undo=gimp_image_undo(image);gboolean before_equal=pixels(drawable)==initial;
    dump(scenario,"undo",drawable);
    gboolean redo=gimp_image_redo(image);gboolean after_equal=pixels(drawable)==after;
    dump(scenario,"redo",drawable);
    std::printf("GRAY_SESSION_UNDO %d %d %d %d %d %d\n",scenario,undo,before_equal,redo,after_equal,gimp_undo_stack_get_depth(gimp_image_get_undo_stack(image)));
  }
  if(brush)g_object_unref(brush);if(pattern)g_object_unref(pattern);g_object_unref(options);g_object_unref(image);
}
int main(){g_type_init();auto*gimp=gimp_init_for_testing();for(int i=0;i<1;++i)capture(gimp,i);std::puts("GRAY_SESSION_CAPTURE_COMPLETE");return 0;}
