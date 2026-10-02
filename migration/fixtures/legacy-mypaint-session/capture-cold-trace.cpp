/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Pinned real GimpMypaintCore + drawable + Undo, synthetic deterministic stimuli.
 * This capture links old application archives; no port evaluator participates. */
#include <cstdio>
#include <execinfo.h>
#include <csignal>
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
  std::vector<guchar> p(width*height*4);
  tile_manager_read_pixel_data(gimp_drawable_get_tiles(d),0,0,width-1,height-1,p.data(),width*4);return p;
}
static void dump(int scenario,const char*phase,GimpDrawable*d)
{
  auto p=pixels(d);std::printf("SESSION_PIX %d %s ",scenario,phase);
  for(auto c:p)std::printf("%02x",c);std::puts("");
}
static void capture(Gimp*gimp,int scenario)
{
  const bool floating=scenario&1,shape=scenario&2,paper=scenario&4,selection=scenario&8,smudge=scenario&16;
  auto*image=gimp_image_new(gimp,150,110,GIMP_RGB);
  auto*layer=gimp_layer_new(image,width,height,GIMP_RGBA_IMAGE,"session-target",1,GIMP_NORMAL_MODE);
  g_assert(gimp_image_add_layer(image,layer,nullptr,0,FALSE));gimp_item_set_offset(GIMP_ITEM(layer),7,3);
  auto*drawable=GIMP_DRAWABLE(layer);std::vector<guchar> initial(width*height*4);
  for(int y=0;y<height;++y)for(int x=0;x<width;++x){int i=(y*width+x)*4;initial[i]=(x*19+y*7)%256;initial[i+1]=(x*3+y*29)%256;initial[i+2]=(x*13+y*11)%256;initial[i+3]=x<35?0:(x*17+y*23)%256;}
  tile_manager_write_pixel_data(gimp_drawable_get_tiles(drawable),0,0,width-1,height-1,initial.data(),width*4);
  if(selection)gimp_channel_select_rectangle(gimp_image_get_mask(image),70,0,80,110,GIMP_CHANNEL_OP_REPLACE,FALSE,0,0,FALSE);
  auto*options=GIMP_MYPAINT_OPTIONS(g_object_new(GIMP_TYPE_MYPAINT_OPTIONS,"gimp",gimp,nullptr));
  GimpRGB fg={.8,.2,.4,1},bg={.1,.2,.3,1};gimp_context_set_foreground(GIMP_CONTEXT(options),&fg);gimp_context_set_background(GIMP_CONTEXT(options),&bg);
  if(options->brush)g_object_unref(options->brush);
  GError*error=nullptr;auto*loaded=gimp_mypaint_brush_load(GIMP_CONTEXT(options),"/workspace/shared/painter-mypaint-session-oracle/session-base.myb",&error);
  g_assert_no_error(error);g_assert(loaded);options->brush=GIMP_MYPAINT_BRUSH(loaded->data);g_list_free(loaded);
  auto*settings=static_cast<GimpMypaintBrushPrivate*>(options->brush->p);
  settings->set_base_value(BRUSH_DABS_PER_SECOND,40);settings->set_base_value(BRUSH_RADIUS_LOGARITHMIC,2.3);
  settings->set_bool_value(BRUSH_NON_INCREMENTAL,floating);settings->set_base_value(BRUSH_STROKE_OPACITY,smudge?.37:1.0);
  settings->set_base_value(BRUSH_SMUDGE,smudge?.8:0);settings->set_base_value(BRUSH_SMUDGE_LENGTH,.65);
  settings->set_bool_value(BRUSH_USE_GIMP_BRUSHMARK,shape);settings->set_bool_value(BRUSH_USE_GIMP_TEXTURE,paper);
  GimpBrush*brush=nullptr;GimpPattern*pattern=nullptr;guchar clear[4]={0,0,0,0};
  if(shape){brush=GIMP_BRUSH(g_object_new(GIMP_TYPE_BRUSH,"name","session-bitmap",nullptr));brush->mask=temp_buf_new(5,3,1,0,0,clear);const guchar mask[]={0,30,90,160,255,12,60,120,180,240,255,190,140,70,0};std::memcpy(temp_buf_get_data(brush->mask),mask,sizeof mask);gimp_context_set_brush(GIMP_CONTEXT(options),brush);}
  if(paper){pattern=GIMP_PATTERN(g_object_new(GIMP_TYPE_PATTERN,"name","session-paper",nullptr));pattern->mask=temp_buf_new(3,2,3,0,0,clear);const guchar texture[]={20,250,10,100,0,50,240,2,180,200,12,33,64,25,70,128,100,5};std::memcpy(temp_buf_get_data(pattern->mask),texture,sizeof texture);gimp_context_set_pattern(GIMP_CONTEXT(options),pattern);}
  gimp_image_undo_free(image);
  {
    GimpMypaintCore core;
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
    std::printf("SESSION_UNDO %d %d %d %d %d %d\n",scenario,undo,before_equal,redo,after_equal,gimp_undo_stack_get_depth(gimp_image_get_undo_stack(image)));
  }
  if(brush)g_object_unref(brush);if(pattern)g_object_unref(pattern);g_object_unref(options);g_object_unref(image);
}
static void fault_trace(int sig){void*frames[32];int n=backtrace(frames,32);backtrace_symbols_fd(frames,n,2);signal(sig,SIG_DFL);raise(sig);}
int main(){signal(SIGSEGV,fault_trace);g_type_init();auto*gimp=gimp_init_for_testing();capture(gimp,1);std::puts("SESSION_CAPTURE_COMPLETE");return 0;}
