/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gtk/gtk.h>
#include <gegl.h>
#include <vector>
#include <cstring>
#include <limits>
extern "C" {
#include "libgimpbase/gimpbase.h"
#include "libgimpmath/gimpmath.h"
#include "core/core-types.h"
#include "core/gimp.h"
#include "core/gimpimage.h"
#include "core/gimpimage-undo.h"
#include "core/gimpimage-convert-precision.h"
#include "core/gimpundostack.h"
#include "core/gimplayer.h"
#include "core/gimplayer-new.h"
#include "core/gimpcontext.h"
#include "core/gimpbrush.h"
#include "core/gimpbrush-private.h"
#include "core/gimpdynamics.h"
#include "core/gimptempbuf.h"
#include "paint/gimpfillbrush.h"
#include "tests.h"
}
static Gimp*gimp;
struct Scene {
 GimpImage*image;GimpLayer*layer;GimpFillBrush*brush;GimpPaintOptions*options;
 bool image_owned=true;std::vector<guchar> initial;GimpCoords coords=GIMP_COORDS_DEFAULT_VALUES;
 Scene():initial(64*64*4,80) {
  image=gimp_image_new(gimp,80,80,GIMP_RGB,GIMP_PRECISION_U8_NON_LINEAR);
  g_object_add_weak_pointer(G_OBJECT(image),(gpointer*)&image);
  layer=gimp_layer_new(image,64,64,babl_format("R'G'B'A u8"),"fill-target",1,GIMP_LAYER_MODE_PAINTER_NORMAL);
  gimp_image_add_layer(image,layer,nullptr,0,FALSE);g_object_ref(layer);gimp_item_set_offset(GIMP_ITEM(layer),4,6);
  for(std::size_t i=3;i<initial.size();i+=4)initial[i]=255;
  gegl_buffer_set(gimp_drawable_get_buffer(GIMP_DRAWABLE(layer)),GEGL_RECTANGLE(0,0,64,64),0,babl_format("R'G'B'A u8"),initial.data(),GEGL_AUTO_ROWSTRIDE);
  options=GIMP_PAINT_OPTIONS(g_object_new(GIMP_TYPE_FILL_BRUSH_OPTIONS,"gimp",gimp,"brush-size",15.0,nullptr));
  auto*b=GIMP_BRUSH(g_object_new(GIMP_TYPE_BRUSH,"name","fill-test-brush",nullptr));b->priv->mask=gimp_temp_buf_new(15,15,babl_format("Y u8"));std::memset(gimp_temp_buf_get_data(b->priv->mask),255,15*15);gimp_context_set_brush(GIMP_CONTEXT(options),b);g_object_unref(b);
  auto*d=GIMP_DYNAMICS(g_object_new(GIMP_TYPE_DYNAMICS,"name","no-dynamics",nullptr));gimp_context_set_dynamics(GIMP_CONTEXT(options),d);g_object_unref(d);
  auto*color=gegl_color_new("red");gimp_context_set_foreground(GIMP_CONTEXT(options),color);g_object_unref(color);gimp_context_set_paint_mode(GIMP_CONTEXT(options),GIMP_LAYER_MODE_PAINTER_NORMAL);
  brush=GIMP_FILL_BRUSH(g_object_new(GIMP_TYPE_FILL_BRUSH,"undo-desc","fill test",nullptr));coords.x=34;coords.y=36;coords.pressure=1;gimp_image_undo_free(image);
 }
 ~Scene(){if(brush)g_object_unref(brush);g_object_unref(options);g_object_unref(layer);if(image){g_object_remove_weak_pointer(G_OBJECT(image),(gpointer*)&image);if(image_owned)g_object_unref(image);}}
 std::vector<guchar> pixels(){std::vector<guchar>p(initial.size());gegl_buffer_get(gimp_drawable_get_buffer(GIMP_DRAWABLE(layer)),GEGL_RECTANGLE(0,0,64,64),1,babl_format("R'G'B'A u8"),p.data(),GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);return p;}
 void begin(){GError*error=nullptr;g_assert_true(gimp_fill_brush_begin(brush,GIMP_DRAWABLE(layer),options,&coords,&error));g_assert_no_error(error);g_assert_true(gimp_image_has_pending_paint(image));}
 void motion(){GError*error=nullptr;g_assert_true(gimp_fill_brush_motion(brush,&coords,0,&error));g_assert_no_error(error);}
 void drain(){GError*error=nullptr;while(!gimp_fill_brush_step(brush,17,&error))g_assert_no_error(error);g_assert_no_error(error);}
 void finish(bool commit){GError*error=nullptr;g_assert_true(gimp_fill_brush_finish(brush,commit,&error));g_assert_no_error(error);}
 void drop_image(){g_assert_true(image_owned);image_owned=false;g_object_unref(image);}
};
static void normal_reuse(){Scene s;for(int n=0;n<2;++n){s.begin();s.motion();GError*error=nullptr;g_assert_false(gimp_fill_brush_finish(s.brush,TRUE,&error));g_assert_no_error(error);s.drain();g_assert(s.pixels()!=s.initial);s.finish(true);g_assert_cmpint(gimp_undo_stack_get_depth(gimp_image_get_undo_stack(s.image)),==,1);g_assert_true(gimp_image_undo(s.image));g_assert(s.pixels()==s.initial);g_assert_true(gimp_image_redo(s.image));g_assert_true(gimp_image_undo(s.image));gimp_image_undo_free(s.image);}}
static void cancel_before_after(){Scene s;s.begin();s.motion();s.finish(false);g_assert(s.pixels()==s.initial);s.begin();s.motion();s.drain();g_assert(s.pixels()!=s.initial);s.finish(false);g_assert(s.pixels()==s.initial);g_assert_cmpint(gimp_undo_stack_get_depth(gimp_image_get_undo_stack(s.image)),==,0);}
static void dispose_rolls_back(){Scene s;s.begin();s.motion();s.drain();g_assert(s.pixels()!=s.initial);auto*b=s.brush;s.brush=nullptr;g_object_unref(b);g_assert(s.pixels()==s.initial);g_assert_false(gimp_viewable_preview_is_frozen(GIMP_VIEWABLE(s.layer)));}
static void cancel_on_frozen(GObject*o,GParamSpec*,gpointer data){if(gimp_viewable_preview_is_frozen(GIMP_VIEWABLE(o))){auto*s=static_cast<Scene*>(data);GError*error=nullptr;g_assert_false(gimp_fill_brush_finish(s->brush,FALSE,&error));g_assert_no_error(error);}}
static void starting_cancel(){Scene s;auto id=g_signal_connect(s.layer,"notify::frozen",G_CALLBACK(cancel_on_frozen),&s);GError*error=nullptr;g_assert_false(gimp_fill_brush_begin(s.brush,GIMP_DRAWABLE(s.layer),s.options,&s.coords,&error));g_assert_nonnull(error);g_clear_error(&error);g_signal_handler_disconnect(s.layer,id);g_assert_false(gimp_viewable_preview_is_frozen(GIMP_VIEWABLE(s.layer)));s.begin();s.finish(false);}
static void cancel_on_update(GimpDrawable*,gint,gint,gint,gint,gpointer data){auto*s=static_cast<Scene*>(data);GError*error=nullptr;gimp_fill_brush_finish(s->brush,FALSE,&error);g_assert_no_error(error);}
static void publication_cancel(){Scene s;s.begin();s.motion();auto id=g_signal_connect(s.layer,"update",G_CALLBACK(cancel_on_update),&s);s.drain();g_signal_handler_disconnect(s.layer,id);g_assert(s.pixels()==s.initial);g_assert_false(gimp_viewable_preview_is_frozen(GIMP_VIEWABLE(s.layer)));s.begin();s.finish(false);}
static void release_core_on_update(GimpDrawable*,gint,gint,gint,gint,gpointer data){auto*s=static_cast<Scene*>(data);if(s->brush){auto*b=s->brush;s->brush=nullptr;g_object_unref(b);}}
static void caller_core_loss(){Scene s;s.begin();s.motion();auto id=g_signal_connect(s.layer,"update",G_CALLBACK(release_core_on_update),&s);GError*error=nullptr;auto*b=s.brush;while(s.brush&&!gimp_fill_brush_step(b,4096,&error))g_assert_no_error(error);g_assert_no_error(error);g_signal_handler_disconnect(s.layer,id);g_assert_null(s.brush);g_assert(s.pixels()==s.initial);}
static void release_image_on_dirty(GimpImage*,GimpDirtyMask,gpointer data){auto*s=static_cast<Scene*>(data);if(s->image_owned)s->drop_image();}
static void caller_image_loss(){Scene s;s.begin();s.motion();s.drain();g_signal_connect(s.image,"dirty",G_CALLBACK(release_image_on_dirty),&s);s.finish(true);g_assert_null(s.image);g_assert_null(gimp_item_get_image(GIMP_ITEM(s.layer)));}
static void detached_target(){Scene s;s.begin();s.motion();gimp_image_remove_layer(s.image,s.layer,FALSE,nullptr);GError*error=nullptr;while(!gimp_fill_brush_step(s.brush,4096,&error)&&!error){}g_assert_nonnull(error);g_clear_error(&error);g_assert(s.pixels()==s.initial);g_assert_false(gimp_viewable_preview_is_frozen(GIMP_VIEWABLE(s.layer)));}
static void invalid_coordinates(){
 Scene s;double GimpCoords::*axes[]={&GimpCoords::x,&GimpCoords::y,&GimpCoords::pressure,&GimpCoords::xtilt,&GimpCoords::ytilt,&GimpCoords::wheel,&GimpCoords::distance,&GimpCoords::rotation,&GimpCoords::slider,&GimpCoords::velocity,&GimpCoords::direction,&GimpCoords::xscale,&GimpCoords::yscale,&GimpCoords::angle};
 for(auto axis:axes){auto c=s.coords;c.*axis=std::numeric_limits<double>::quiet_NaN();GError*error=nullptr;g_assert_false(gimp_fill_brush_begin(s.brush,GIMP_DRAWABLE(s.layer),s.options,&c,&error));g_assert_nonnull(error);g_clear_error(&error);g_assert_false(gimp_viewable_preview_is_frozen(GIMP_VIEWABLE(s.layer)));}
 for(double x:{std::numeric_limits<double>::infinity(),-std::numeric_limits<double>::infinity(),1e100,-1e100}){auto c=s.coords;c.x=x;GError*error=nullptr;g_assert_false(gimp_fill_brush_begin(s.brush,GIMP_DRAWABLE(s.layer),s.options,&c,&error));g_assert_nonnull(error);g_clear_error(&error);}
 s.begin();s.motion();s.drain();auto before=s.pixels();for(auto axis:axes){auto c=s.coords;c.*axis=std::numeric_limits<double>::infinity();GError*error=nullptr;g_assert_false(gimp_fill_brush_motion(s.brush,&c,1,&error));g_assert_nonnull(error);g_clear_error(&error);g_assert(s.pixels()==before);}s.finish(false);g_assert(s.pixels()==s.initial);
}
static void stale_external_pixels(){
 Scene s;s.begin();s.motion();s.drain();s.coords.x+=4;s.motion();auto*blue=gegl_color_new("blue");gegl_buffer_set_color(gimp_drawable_get_buffer(GIMP_DRAWABLE(s.layer)),GEGL_RECTANGLE(25,25,9,9),blue);g_object_unref(blue);auto independent=s.pixels();GError*error=nullptr;g_assert_false(gimp_fill_brush_step(s.brush,1,&error));g_assert_nonnull(error);g_clear_error(&error);g_assert(s.pixels()==independent);g_assert_false(gimp_viewable_preview_is_frozen(GIMP_VIEWABLE(s.layer)));s.finish(false);g_assert(s.pixels()==independent);
}
static void stale_geometry_format_lock(){
 for(int kind=0;kind<4;++kind){Scene s;s.begin();s.motion();s.drain();s.coords.x+=4;s.motion();
  if(kind==0)gimp_item_set_offset(GIMP_ITEM(s.layer),9,11);
  else if(kind==1)gimp_item_resize(GIMP_ITEM(s.layer),GIMP_CONTEXT(s.options),GIMP_FILL_TRANSPARENT,61,59,0,0);
  else if(kind==2)gimp_image_convert_precision(s.image,GIMP_PRECISION_U16_NON_LINEAR,GEGL_DITHER_NONE,GEGL_DITHER_NONE,GEGL_DITHER_NONE,nullptr);
  else gimp_item_set_lock_content(GIMP_ITEM(s.layer),TRUE,FALSE);
  auto*buffer=gimp_drawable_get_buffer(GIMP_DRAWABLE(s.layer));auto rect=*gegl_buffer_get_extent(buffer);auto*format=gegl_buffer_get_format(buffer);const auto count=std::size_t(rect.width)*rect.height*babl_format_get_bytes_per_pixel(format);std::vector<guchar> before(count),after(count);gegl_buffer_get(buffer,&rect,1,format,before.data(),GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);int x,y;gimp_item_get_offset(GIMP_ITEM(s.layer),&x,&y);if(kind==3)before=s.initial;
  GError*error=nullptr;g_assert_false(gimp_fill_brush_step(s.brush,1,&error));g_assert_nonnull(error);g_clear_error(&error);g_assert(gegl_buffer_get_format(gimp_drawable_get_buffer(GIMP_DRAWABLE(s.layer)))==format);gegl_buffer_get(gimp_drawable_get_buffer(GIMP_DRAWABLE(s.layer)),&rect,1,format,after.data(),GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);g_assert(before==after);int xx,yy;gimp_item_get_offset(GIMP_ITEM(s.layer),&xx,&yy);g_assert_cmpint(x,==,xx);g_assert_cmpint(y,==,yy);g_assert_cmpint(gimp_item_get_width(GIMP_ITEM(s.layer)),==,rect.width);g_assert_cmpint(gimp_item_get_height(GIMP_ITEM(s.layer)),==,rect.height);if(kind==3)g_assert_true(gimp_item_is_content_locked(GIMP_ITEM(s.layer),nullptr));g_assert_false(gimp_viewable_preview_is_frozen(GIMP_VIEWABLE(s.layer)));
 }
}
static void query_during_start(GObject*o,GParamSpec*,gpointer data){auto*s=static_cast<Scene*>(data);if(gimp_viewable_preview_is_frozen(GIMP_VIEWABLE(o)))g_assert_true(gimp_image_has_pending_paint(s->image));}
static void query_on_update(GimpDrawable*,gint,gint,gint,gint,gpointer data){auto*s=static_cast<Scene*>(data);g_assert_true(gimp_image_has_pending_paint(s->image));}
static void pending_query_lifetime(){Scene s;g_assert_false(gimp_image_has_pending_paint(s.image));auto start=g_signal_connect(s.layer,"notify::frozen",G_CALLBACK(query_during_start),&s);s.begin();g_signal_handler_disconnect(s.layer,start);auto update=g_signal_connect(s.layer,"update",G_CALLBACK(query_on_update),&s);s.motion();s.drain();g_signal_handler_disconnect(s.layer,update);g_assert_true(gimp_image_has_pending_paint(s.image));update=g_signal_connect(s.layer,"update",G_CALLBACK(query_on_update),&s);auto*b=s.brush;s.brush=nullptr;g_object_unref(b);g_signal_handler_disconnect(s.layer,update);g_assert_false(gimp_image_has_pending_paint(s.image));g_assert(s.pixels()==s.initial);}
int main(int argc,char**argv){g_test_init(&argc,&argv,nullptr);gimp=gimp_init_for_testing();g_test_add_func("/fill-core/normal-reuse",normal_reuse);g_test_add_func("/fill-core/cancel-before-after",cancel_before_after);g_test_add_func("/fill-core/dispose-rollback",dispose_rolls_back);g_test_add_func("/fill-core/starting-cancel",starting_cancel);g_test_add_func("/fill-core/publication-cancel",publication_cancel);g_test_add_func("/fill-core/caller-core-loss",caller_core_loss);g_test_add_func("/fill-core/caller-image-loss",caller_image_loss);g_test_add_func("/fill-core/detached-target",detached_target);g_test_add_func("/fill-core/invalid-coordinates",invalid_coordinates);g_test_add_func("/fill-core/stale-external-pixels",stale_external_pixels);g_test_add_func("/fill-core/stale-geometry-format-lock",stale_geometry_format_lock);g_test_add_func("/fill-core/pending-query-lifetime",pending_query_lifetime);return g_test_run();}
