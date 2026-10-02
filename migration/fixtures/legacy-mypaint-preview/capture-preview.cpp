/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Calls the unchanged pinned editor preview, not a reimplementation of it. */
#include <cstdio>
#include <vector>
#include <cstring>
extern "C" {
#include "config.h"
#include <gtk/gtk.h>
#include <gegl.h>
#include "libgimpbase/gimpbase.h"
#include "libgimpmath/gimpmath.h"
#include "core/core-types.h"
#include "base/temp-buf.h"
#include "core/gimp.h"
#include "core/gimpcontext.h"
#include "core/gimpbrush.h"
#include "core/gimppattern.h"
#include "core/gimpmypaintbrush.h"
#include "core/gimpmypaintbrush-load.h"
#include "tests.h"
}
#include "core/gimpmypaintbrush-private.hpp"
static void capture(Gimp*gimp,int scenario,const char*base)
{
  auto*context=gimp_context_new(gimp,"preview-oracle",nullptr);
  GError*error=nullptr;auto*loaded=gimp_mypaint_brush_load(context,base,&error);
  g_assert_no_error(error);g_assert(loaded);auto*source=GIMP_MYPAINT_BRUSH(loaded->data);g_list_free(loaded);
  auto*settings=static_cast<GimpMypaintBrushPrivate*>(source->p);
  settings->set_base_value(BRUSH_DABS_PER_SECOND,40);settings->set_base_value(BRUSH_RADIUS_LOGARITHMIC,2.3);
  settings->set_base_value(BRUSH_OPAQUE,.65);settings->set_base_value(BRUSH_COLOR_H,.63);settings->set_base_value(BRUSH_COLOR_S,.7);settings->set_base_value(BRUSH_COLOR_V,.6);
  settings->set_bool_value(BRUSH_NON_INCREMENTAL,scenario&1);settings->set_base_value(BRUSH_STROKE_OPACITY,.37);
  settings->set_base_value(BRUSH_SMUDGE,scenario&8?.8:0);settings->set_base_value(BRUSH_SMUDGE_LENGTH,.65);
  settings->set_bool_value(BRUSH_USE_GIMP_BRUSHMARK,scenario&2);settings->set_bool_value(BRUSH_USE_GIMP_TEXTURE,scenario&4);
  settings->set_base_value(BRUSH_TEXTURE_GRAIN,.14);settings->set_base_value(BRUSH_TEXTURE_CONTRAST,.8);
  settings->set_text_value(BRUSH_BRUSHMARK_NAME,nullptr);settings->set_text_value(BRUSH_TEXTURE_NAME,nullptr);
  guchar clear[4]={0,0,0,0};GimpBrush*brush=nullptr;GimpPattern*pattern=nullptr;
  if(scenario&2){brush=GIMP_BRUSH(g_object_new(GIMP_TYPE_BRUSH,"name","preview-bitmap",nullptr));brush->mask=temp_buf_new(5,3,1,0,0,clear);const guchar mask[]={0,30,90,160,255,12,60,120,180,240,255,190,140,70,0};std::memcpy(temp_buf_get_data(brush->mask),mask,sizeof mask);gimp_context_set_brush(context,brush);}
  if(scenario&4){pattern=GIMP_PATTERN(g_object_new(GIMP_TYPE_PATTERN,"name","preview-paper",nullptr));pattern->mask=temp_buf_new(3,2,3,0,0,clear);const guchar texture[]={20,250,10,100,0,50,240,2,180,200,12,33,64,25,70,128,100,5};std::memcpy(temp_buf_get_data(pattern->mask),texture,sizeof texture);gimp_context_set_pattern(context,pattern);}
  std::vector<guchar>pixels(256*256*4),again(pixels.size());
  settings->get_new_preview(pixels.data(),context,256,256,4,256*4);
  settings->get_new_preview(again.data(),context,256,256,4,256*4);
  std::printf("PREVIEW_PIX %d ",scenario);for(auto byte:pixels)std::printf("%02x",byte);std::puts("");
  std::printf("PREVIEW_REPEAT %d %d\n",scenario,pixels==again);
  if(brush)g_object_unref(brush);if(pattern)g_object_unref(pattern);g_object_unref(source);g_object_unref(context);
}
int main(int argc,char**argv){g_assert(argc==2);g_type_init();auto*gimp=gimp_init_for_testing();for(int n=0;n<16;++n)capture(gimp,n,argv[1]);std::puts("PREVIEW_CAPTURE_COMPLETE");return 0;}
