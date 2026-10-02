/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gegl.h>
#include <gtk/gtk.h>
#include <vector>
#include <cstring>
extern "C" {
#include "libgimpbase/gimpbase.h"
#include "libgimpmath/gimpmath.h"
#include "core/core-types.h"
#include "widgets/widgets-types.h"
#include "core/gimp.h"
#include "core/gimpbrush.h"
#include "core/gimpbrush-private.h"
#include "core/gimppattern.h"
#include "core/gimpcontainer.h"
#include "core/gimpdatafactory.h"
#include "core/gimpcontext.h"
#include "core/gimpimage.h"
#include "core/gimpimage-undo.h"
#include "core/gimpundostack.h"
#include "core/gimpchannel.h"
#include "core/gimpchannel-select.h"
#include "core/gimplayer.h"
#include "core/gimplayer-new.h"
#include "core/gimptempbuf.h"
#include "paint/gimppaintoptions.h"
#include "tests.h"
#include "gimp-app-test-utils.h"
}
#include "paint/painter-mypaint-surface/paint-core.hpp"
#include "paint/painter-mypaint-surface/gimp-resources.hpp"
#include "painter/object-ref.hpp"
#include "paint/painter-mypaint-surface/legacy-mask-transform.hpp"
#include "paint/painter-mypaint-surface/legacy-generated-mask.hpp"
using namespace GimpPainter::MyPaint;
static Gimp *gimp;
static GimpLayer *layer_new(GimpImage **image)
{
  *image=gimp_image_new(gimp,64,48,GIMP_RGB,GIMP_PRECISION_U8_NON_LINEAR);
  auto*layer=gimp_layer_new(*image,64,48,babl_format("R'G'B'A u8"),"Paint target",1,GIMP_LAYER_MODE_NORMAL);
  g_assert_true(gimp_image_add_layer(*image,layer,nullptr,0,FALSE));
  std::vector<guchar> pixels(64*48*4);
  for(unsigned i=0;i<pixels.size();i+=4){pixels[i]=40;pixels[i+1]=90;pixels[i+2]=170;pixels[i+3]=180;}
  gegl_buffer_set(gimp_drawable_get_buffer(GIMP_DRAWABLE(layer)),GEGL_RECTANGLE(0,0,64,48),0,babl_format("R'G'B'A u8"),pixels.data(),GEGL_AUTO_ROWSTRIDE);
  return layer;
}
static std::vector<guchar> pixels(GimpDrawable *drawable)
{
  std::vector<guchar> values(64*48*4);
  gegl_buffer_get(gimp_drawable_get_buffer(drawable),GEGL_RECTANGLE(0,0,64,48),1,babl_format("R'G'B'A u8"),values.data(),GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
  return values;
}
static GimpPaintOptions *options_new()
{
  auto*options=GIMP_PAINT_OPTIONS(g_object_new(GIMP_TYPE_PAINT_OPTIONS,"gimp",gimp,nullptr));
  auto*color=gegl_color_new("rgb(0.8,0.2,0.4)");gimp_context_set_foreground(GIMP_CONTEXT(options),color);g_object_unref(color);
  return options;
}
static Resource settings(bool floating)
{
  Resource r;r.set_base_value(BRUSH_DABS_PER_SECOND,40);r.set_base_value(BRUSH_RADIUS_LOGARITHMIC,1.5);
  r.set_switch(BRUSH_NON_INCREMENTAL,floating);r.set_base_value(BRUSH_STROKE_OPACITY,.37);return r;
}
static void stroke(PaintCore&core,GimpDrawable*drawable)
{
  GimpCoords coords=GIMP_COORDS_DEFAULT_VALUES;coords.x=16;coords.y=16;coords.pressure=0;
  core.stroke_to(drawable,.01,coords);
  coords.pressure=.85;core.stroke_to(drawable,.05,coords); // stationary-pressure event
  for(int i=0;i<8;++i){coords.x+=2;coords.y+=1;core.stroke_to(drawable,.01,coords);}
}
static void session_undo_and_cancel()
{
  for(bool floating:{false,true}) {
    GimpImage*image;auto*layer=layer_new(&image);auto*drawable=GIMP_DRAWABLE(layer);auto*options=options_new();
    gimp_image_undo_free(image);auto before=pixels(drawable);
    {
      PaintCore core(options,settings(floating));stroke(core,drawable);g_assert_true(core.active());core.finish();
      g_assert_false(gimp_viewable_preview_is_frozen(GIMP_VIEWABLE(layer)));
      auto after=pixels(drawable);g_assert_true(before!=after);
      g_assert_cmpint(gimp_undo_stack_get_depth(gimp_image_get_undo_stack(image)),==,1);
      g_assert_true(gimp_image_undo(image));g_assert_true(pixels(drawable)==before);
      g_assert_true(gimp_image_redo(image));g_assert_true(pixels(drawable)==after);
      g_assert_cmpuint(core.bytes_written(),>,0);
    }
    {
      auto before_cancel=pixels(drawable);const int depth=gimp_undo_stack_get_depth(gimp_image_get_undo_stack(image));
      PaintCore core(options,settings(floating));stroke(core,drawable);core.cancel();core.cancel();
      g_assert_true(pixels(drawable)==before_cancel);
      g_assert_false(gimp_viewable_preview_is_frozen(GIMP_VIEWABLE(layer)));
      g_assert_cmpint(gimp_undo_stack_get_depth(gimp_image_get_undo_stack(image)),==,depth);
    }
    {
      auto before_drop=pixels(drawable);{PaintCore core(options,settings(floating));stroke(core,drawable);}
      g_assert_true(pixels(drawable)==before_drop);g_assert_false(gimp_viewable_preview_is_frozen(GIMP_VIEWABLE(layer)));
    }
    g_object_unref(options);g_object_unref(image);
  }
}
static void stationary_and_selection()
{
  GimpImage*image;auto*layer=layer_new(&image);auto*drawable=GIMP_DRAWABLE(layer);auto*options=options_new();
  gimp_channel_select_rectangle(gimp_image_get_mask(image),16,0,48,48,GIMP_CHANNEL_OP_REPLACE,FALSE,0,0,FALSE);
  auto before=pixels(drawable);
  PaintCore core(options,settings(false));
  GimpCoords coords=GIMP_COORDS_DEFAULT_VALUES;coords.x=16;coords.y=16;coords.pressure=0;core.stroke_to(drawable,.01,coords);
  coords.pressure=1;core.stroke_to(drawable,.075,coords);core.finish();auto after=pixels(drawable);
  g_assert_true(before!=after);
  for(int y=0;y<48;++y)for(int x=0;x<16;++x)g_assert_cmpmem(before.data()+(y*64+x)*4,4,after.data()+(y*64+x)*4,4);
  g_object_unref(options);g_object_unref(image);
}
static GimpBrush *bitmap()
{
  auto*brush=GIMP_BRUSH(g_object_new(GIMP_TYPE_BRUSH,"name","Painter test bitmap",nullptr));
  brush->priv->mask=gimp_temp_buf_new(5,3,babl_format("Y u8"));
  std::memset(gimp_temp_buf_get_data(brush->priv->mask),255,15);return brush;
}
static void resources_lifetime_and_missing()
{
  auto*options=options_new();auto*brush=bitmap();
  auto*pattern=GIMP_PATTERN(g_object_new(GIMP_TYPE_PATTERN,"name","Painter test paper",nullptr));
  pattern->mask=gimp_temp_buf_new(3,2,babl_format("R'G'B' u8"));std::memset(gimp_temp_buf_get_data(pattern->mask),20,18);
  gimp_context_set_brush(GIMP_CONTEXT(options),brush);gimp_context_set_pattern(GIMP_CONTEXT(options),pattern);
  Resource r;r.set_switch(BRUSH_USE_GIMP_BRUSHMARK,true);r.set_switch(BRUSH_BRUSHMARK_SPECIFIED,true);r.set_text(BRUSH_BRUSHMARK_NAME,"missing-name-retained");
  r.set_switch(BRUSH_USE_GIMP_TEXTURE,true);r.set_switch(BRUSH_TEXTURE_SPECIFIED,true);r.set_text(BRUSH_TEXTURE_NAME,"missing-paper-retained");
  auto*buffer=gegl_buffer_new(GEGL_RECTANGLE(0,0,24,24),babl_format("R'G'B'A u8"));
  auto surface=std::unique_ptr<GeglSurface>(new GeglSurface(buffer));
  {
    GimpResources resources(GIMP_CONTEXT(options),r);
    g_assert_true(resources.resolution().brush_missing);g_assert_true(resources.resolution().texture_missing);
    g_assert_cmpint(brush->priv->use_count,==,1);resources.set_brush(brush);g_assert_cmpint(brush->priv->use_count,==,1);
    resources.attach(*surface);resources.set_brush(nullptr);g_assert_cmpint(brush->priv->use_count,==,0);
    resources.set_brush(brush);g_assert_cmpint(brush->priv->use_count,==,1);
    surface->begin_session();surface->draw_dab(12,12,3,1,0,0,1,1);surface->end_session();
  }
  g_assert_cmpint(brush->priv->use_count,==,1); // Surface owns its bound resources
  surface.reset();g_assert_cmpint(brush->priv->use_count,==,0);
  {
    GimpResources preview(GIMP_CONTEXT(options),r,GimpResources::Purpose::Preview);
    g_assert_true(preview.resolution().brush_missing);g_assert_true(preview.resolution().effective_brush.empty());
    g_assert_cmpint(brush->priv->use_count,==,0);
  }
  g_assert_true(r.text_value(BRUSH_BRUSHMARK_NAME)=="missing-name-retained");
  g_object_unref(buffer);g_object_unref(brush);g_object_unref(pattern);g_object_unref(options);
}
static void paper_invalidation()
{
  auto*options=options_new();auto*pattern=GIMP_PATTERN(g_object_new(GIMP_TYPE_PATTERN,"name","changing-paper",nullptr));
  pattern->mask=gimp_temp_buf_new(1,1,babl_format("R'G'B' u8"));std::memset(gimp_temp_buf_get_data(pattern->mask),20,3);
  gimp_context_set_pattern(GIMP_CONTEXT(options),pattern);Resource r;r.set_switch(BRUSH_USE_GIMP_TEXTURE,true);
  auto*buffer=gegl_buffer_new(GEGL_RECTANGLE(0,0,16,16),babl_format("R'G'B'A u8"));GeglSurface surface(buffer);
  GimpResources resources(GIMP_CONTEXT(options),r);resources.attach(surface);
  guchar pixel[4];surface.begin_session();surface.draw_dab(8.5,8.5,3,1,0,0,1,1);surface.end_session();
  gegl_buffer_get(buffer,GEGL_RECTANGLE(8,8,1,1),1,babl_format("R'G'B'A u8"),pixel,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
  g_assert_cmpint(pixel[3],==,20);
  gegl_buffer_clear(buffer,nullptr);std::memset(gimp_temp_buf_get_data(pattern->mask),255,3);gimp_data_dirty(GIMP_DATA(pattern));
  surface.begin_session();surface.draw_dab(8.5,8.5,3,1,0,0,1,1);surface.end_session();
  gegl_buffer_get(buffer,GEGL_RECTANGLE(8,8,1,1),1,babl_format("R'G'B'A u8"),pixel,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
  g_assert_cmpint(pixel[3],==,255);g_object_unref(buffer);g_object_unref(pattern);g_object_unref(options);
}
static void rejected_snapshot_and_format()
{
  auto*buffer=gegl_buffer_new(GEGL_RECTANGLE(0,0,16,16),babl_format("R'G'B'A u8"));
  GeglSurface surface(buffer);bool rejected=false;
  try{surface.begin_session_from(buffer);}catch(const std::invalid_argument&){rejected=true;}
  g_assert_true(rejected);
  surface.begin_session();surface.draw_dab(8,8,3,1,0,0,1,1);surface.cancel_session();
  g_object_unref(buffer);
  auto*high=gegl_buffer_new(GEGL_RECTANGLE(0,0,16,16),babl_format("R'G'B'A float"));rejected=false;
  try{GeglSurface unsupported(high);}catch(const std::invalid_argument&){rejected=true;}
  g_assert_true(rejected);g_object_unref(high);
}
static void transparent_lock_and_representable_masks()
{
  for(bool floating:{false,true}) {
    auto*buffer=gegl_buffer_new(GEGL_RECTANGLE(0,0,16,16),babl_format("R'G'B'A u8"));
    std::vector<guchar> hidden(16*16*4);
    for(unsigned i=0;i<hidden.size();i+=4){hidden[i]=33;hidden[i+1]=99;hidden[i+2]=177;hidden[i+3]=0;}
    gegl_buffer_set(buffer,GEGL_RECTANGLE(0,0,16,16),0,babl_format("R'G'B'A u8"),hidden.data(),GEGL_AUTO_ROWSTRIDE);
    GeglSurface surface(buffer);surface.set_non_incremental(floating);surface.begin_session();
    surface.draw_dab(8,8,4,.8,.2,.4,.63,.55,1,1,0,1,0,0,1);surface.end_session();
    guchar value[4];gegl_buffer_get(buffer,GEGL_RECTANGLE(8,8,1,1),1,babl_format("R'G'B'A u8"),value,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
    g_assert_cmpint(value[0],==,floating?33:0);g_assert_cmpint(value[1],==,floating?99:0);
    g_assert_cmpint(value[2],==,floating?177:0);g_assert_cmpint(value[3],==,0);g_object_unref(buffer);
  }
  ShapeMask wide{9001,1,std::vector<guchar>(9001,255)};
  auto same=transform_bitmap_mask(wide,1,0,0,1);g_assert_true(same.pixels==wide.pixels);
  auto size=legacy_generated_dimensions(GeneratedShape::Circle,5000,2,.5,1,0);
  g_assert_cmpint(size.first,>,8192);
}
struct StartReentry {
  PaintCore*core;GimpDrawable*drawable;bool fired=false,motion_rejected=false,configure_rejected=false;
};
static void reenter_start(GObject*object,GParamSpec*,gpointer data)
{
  auto&s=*static_cast<StartReentry*>(data);
  if(s.fired||!gimp_viewable_preview_is_frozen(GIMP_VIEWABLE(object)))return;
  s.fired=true;GimpCoords coords=GIMP_COORDS_DEFAULT_VALUES;
  try{s.core->stroke_to(s.drawable,.01,coords);}catch(const std::logic_error&){s.motion_rejected=true;}
  try{s.core->configure(settings(false));}catch(const std::logic_error&){s.configure_rejected=true;}
  s.core->cancel();s.core->cancel();s.core->finish(); // deferred until start unwinds
}
static void starting_reentry_and_deferred_cancel()
{
  GimpImage*image;auto*layer=layer_new(&image);auto*drawable=GIMP_DRAWABLE(layer);auto*options=options_new();
  gimp_image_undo_free(image);auto before=pixels(drawable);PaintCore core(options,settings(false));
  StartReentry state{&core,drawable};auto id=g_signal_connect(layer,"notify::frozen",G_CALLBACK(reenter_start),&state);
  GimpCoords coords=GIMP_COORDS_DEFAULT_VALUES;coords.x=16;coords.y=16;coords.pressure=1;core.stroke_to(drawable,.1,coords);
  g_assert_true(state.fired&&state.motion_rejected&&state.configure_rejected);
  g_assert_false(core.active());g_assert_false(gimp_viewable_preview_is_frozen(GIMP_VIEWABLE(layer)));
  g_assert_true(before==pixels(drawable));g_assert_cmpint(gimp_undo_stack_get_depth(gimp_image_get_undo_stack(image)),==,0);
  g_signal_handler_disconnect(layer,id);stroke(core,drawable);core.finish();g_assert_true(before!=pixels(drawable));
  g_object_unref(options);g_object_unref(image);
}
struct ImageReentry {
  GimpImage**owner;GimpLayer*layer;int phase;bool fired=false,alive_inside=false,finalized=false;
};
static void image_gone(gpointer data,GObject*){static_cast<ImageReentry*>(data)->finalized=true;}
static void release_image(ImageReentry&s)
{
  if(s.fired)return;
  s.fired=true;auto*image=*s.owner;*s.owner=nullptr;g_object_unref(image);s.alive_inside=!s.finalized;
}
static void image_preview_reentry(GObject*object,GParamSpec*,gpointer data)
{
  auto&s=*static_cast<ImageReentry*>(data);bool frozen=gimp_viewable_preview_is_frozen(GIMP_VIEWABLE(object));
  if((s.phase==0&&frozen)||(s.phase==1&&!frozen))release_image(s);
}
static void image_dirty_reentry(GimpImage*,GimpDirtyMask,gpointer data){release_image(*static_cast<ImageReentry*>(data));}
static void image_lifetime_across_native_callbacks()
{
  for(int phase=0;phase<3;++phase) {
    GimpImage*image;auto*layer=layer_new(&image);g_object_ref(layer);auto*options=options_new();
    gimp_image_undo_free(image);ImageReentry state{&image,layer,phase};g_object_weak_ref(G_OBJECT(image),image_gone,&state);
    gulong id=0;
    {
      PaintCore core(options,settings(false));
      if(phase==0) {
        id=g_signal_connect(layer,"notify::frozen",G_CALLBACK(image_preview_reentry),&state);
        GimpCoords coords=GIMP_COORDS_DEFAULT_VALUES;coords.x=16;coords.y=16;coords.pressure=1;
        core.stroke_to(GIMP_DRAWABLE(layer),.1,coords);
      } else {
        // Arm end hooks after painting, so an automatic empty first-event split
        // cannot close the image before the actual commit under examination.
        stroke(core,GIMP_DRAWABLE(layer));
        if(phase==2)g_signal_connect(image,"dirty",G_CALLBACK(image_dirty_reentry),&state);
        else id=g_signal_connect(layer,"notify::frozen",G_CALLBACK(image_preview_reentry),&state);
      }
      core.finish();g_assert_false(core.active());
    }
    g_assert_true(state.fired&&state.alive_inside&&state.finalized);g_assert_null(image);
    g_assert_null(gimp_item_get_image(GIMP_ITEM(layer)));g_assert_false(gimp_viewable_preview_is_frozen(GIMP_VIEWABLE(layer)));
    if(id)g_signal_handler_disconnect(layer,id);
    g_object_unref(layer);g_object_unref(options);
  }
}
struct RemoveReentry {GimpImage*image;GimpLayer*layer;bool on_freeze,fired=false;};
static void remove_during_preview(GObject*object,GParamSpec*,gpointer data)
{
  auto&s=*static_cast<RemoveReentry*>(data);if(s.fired||gimp_viewable_preview_is_frozen(GIMP_VIEWABLE(object))!=s.on_freeze)return;
  s.fired=true;gimp_image_remove_layer(s.image,s.layer,FALSE,nullptr);
}
static void drawable_removal_during_native_callbacks()
{
  for(bool freeze:{false,true}) {
    GimpImage*image;auto*layer=layer_new(&image);g_object_ref(layer);auto*options=options_new();auto before=pixels(GIMP_DRAWABLE(layer));
    RemoveReentry state{image,layer,freeze};gulong id=0;
    if(freeze)id=g_signal_connect(layer,"notify::frozen",G_CALLBACK(remove_during_preview),&state);
    {PaintCore core(options,settings(false));
      if(freeze){GimpCoords coords=GIMP_COORDS_DEFAULT_VALUES;coords.x=16;coords.y=16;coords.pressure=1;core.stroke_to(GIMP_DRAWABLE(layer),.1,coords);}
      else{stroke(core,GIMP_DRAWABLE(layer));id=g_signal_connect(layer,"notify::frozen",G_CALLBACK(remove_during_preview),&state);core.finish();}
      g_assert_false(core.active());}
    g_assert_true(state.fired);g_assert_false(gimp_item_is_attached(GIMP_ITEM(layer)));
    g_assert_false(gimp_viewable_preview_is_frozen(GIMP_VIEWABLE(layer)));
    if(freeze)g_assert_true(before==pixels(GIMP_DRAWABLE(layer)));
    g_signal_handler_disconnect(layer,id);g_object_unref(layer);g_object_unref(options);g_object_unref(image);
  }
}
static void rgb_native_cancel_undo()
{
  for(bool floating:{false,true})for(bool eraser:{false,true}) {
    auto*image=gimp_image_new(gimp,64,48,GIMP_RGB,GIMP_PRECISION_U8_NON_LINEAR);
    const auto*format=babl_format("R'G'B' u8");auto*layer=gimp_layer_new(image,64,48,format,"RGB paint target",1,GIMP_LAYER_MODE_NORMAL);
    g_assert_true(gimp_image_add_layer(image,layer,nullptr,0,FALSE));auto*drawable=GIMP_DRAWABLE(layer);auto*buffer=gimp_drawable_get_buffer(drawable);
    std::vector<guchar>initial(64*48*3,120);gegl_buffer_set(buffer,GEGL_RECTANGLE(0,0,64,48),0,format,initial.data(),GEGL_AUTO_ROWSTRIDE);
    const auto read=[&]{std::vector<guchar>out(initial.size());gegl_buffer_get(gimp_drawable_get_buffer(drawable),GEGL_RECTANGLE(0,0,64,48),1,format,out.data(),GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);return out;};
    auto*options=options_new();auto resource=settings(floating);resource.set_base_value(BRUSH_ERASER,eraser?.65:0);gimp_image_undo_free(image);
    PaintCore core(options,resource);stroke(core,drawable);g_assert_true(read()!=initial);core.cancel();g_assert_true(read()==initial);
    g_assert_cmpint(gimp_undo_stack_get_depth(gimp_image_get_undo_stack(image)),==,0);g_assert_false(gimp_viewable_preview_is_frozen(GIMP_VIEWABLE(layer)));
    stroke(core,drawable);core.finish();const auto painted=read();g_assert_true(painted!=initial);g_assert_cmpint(gimp_undo_stack_get_depth(gimp_image_get_undo_stack(image)),==,1);
    g_assert_true(gimp_image_undo(image));g_assert_true(read()==initial);g_assert_true(gimp_image_redo(image));g_assert_true(read()==painted);
    g_assert_false(gimp_drawable_has_alpha(drawable));g_assert_cmpint(babl_format_get_bytes_per_pixel(gegl_buffer_get_format(gimp_drawable_get_buffer(drawable))),==,3);
    g_object_unref(options);g_object_unref(image);
  }
}
int main(int argc,char**argv)
{
  g_test_init(&argc,&argv,nullptr);gimp_test_utils_set_gimp3_directory("GIMP_TESTING_ABS_TOP_SRCDIR","app/tests/gimpdir");gimp=gimp_init_for_testing();
  g_test_add_func("/painter-surface/session-undo-cancel",session_undo_and_cancel);
  g_test_add_func("/painter-surface/rgb-cancel-undo",rgb_native_cancel_undo);
  g_test_add_func("/painter-surface/stationary-selection",stationary_and_selection);
  g_test_add_func("/painter-surface/resource-lifetime-missing",resources_lifetime_and_missing);
  g_test_add_func("/painter-surface/paper-invalidation",paper_invalidation);
  g_test_add_func("/painter-surface/rejected-snapshot-format",rejected_snapshot_and_format);
  g_test_add_func("/painter-surface/transparent-lock-representable-masks",transparent_lock_and_representable_masks);
  g_test_add_func("/painter-surface/starting-reentry-deferred-cancel",starting_reentry_and_deferred_cancel);
  g_test_add_func("/painter-surface/image-lifetime-native-callbacks",image_lifetime_across_native_callbacks);
  g_test_add_func("/painter-surface/drawable-removal-native-callbacks",drawable_removal_during_native_callbacks);
  int result=g_test_run();gimp_test_utils_set_gimp3_directory("GIMP_TESTING_ABS_TOP_BUILDDIR","app/tests/gimpdir-output");gimp_exit(gimp,TRUE);return result;
}
