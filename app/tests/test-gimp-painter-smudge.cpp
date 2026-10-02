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
#include "paint/gimppaintersmudge.h"
#include "painter/gimp-painter-binding.h"
#include "tests.h"
}
static Gimp* gimp;
struct Scene {
  GimpImage* image;
  GimpLayer* layer;
  GimpPainterSmudge* core;
  GimpPaintOptions* options;
  GimpCoords coords = GIMP_COORDS_DEFAULT_VALUES;
  std::vector<guchar> initial;
  bool owns_image = true;
  Scene(): initial(64*48*4) {
    image=gimp_image_new(gimp,72,58,GIMP_RGB,GIMP_PRECISION_U8_NON_LINEAR);
    g_object_add_weak_pointer(G_OBJECT(image),reinterpret_cast<gpointer*>(&image));
    layer=gimp_layer_new(image,64,48,babl_format("R'G'B'A u8"),"smudge target",1,GIMP_LAYER_MODE_PAINTER_NORMAL);
    gimp_image_add_layer(image,layer,nullptr,0,FALSE);g_object_ref(layer);gimp_item_set_offset(GIMP_ITEM(layer),3,5);
    for(int y=0;y<48;++y)for(int x=0;x<64;++x){auto i=(y*64+x)*4;initial[i]=x<32?40:220;initial[i+1]=120;initial[i+2]=70;initial[i+3]=255;}
    gegl_buffer_set(gimp_drawable_get_buffer(GIMP_DRAWABLE(layer)),GEGL_RECTANGLE(0,0,64,48),0,babl_format("R'G'B'A u8"),initial.data(),GEGL_AUTO_ROWSTRIDE);
    options=GIMP_PAINT_OPTIONS(g_object_new(GIMP_TYPE_PAINTER_SMUDGE_OPTIONS,"gimp",gimp,"brush-size",17.0,"use-color-blending",TRUE,nullptr));
    auto* brush=GIMP_BRUSH(g_object_new(GIMP_TYPE_BRUSH,"name","smudge test brush",nullptr));brush->priv->mask=gimp_temp_buf_new(17,17,babl_format("Y u8"));std::memset(gimp_temp_buf_get_data(brush->priv->mask),255,17*17);
    gimp_context_set_brush(GIMP_CONTEXT(options),brush);g_object_unref(brush);
    auto* dynamics=GIMP_DYNAMICS(g_object_new(GIMP_TYPE_DYNAMICS,"name","smudge test dynamics",nullptr));gimp_context_set_dynamics(GIMP_CONTEXT(options),dynamics);g_object_unref(dynamics);
    auto* color=gegl_color_new("red");gimp_context_set_foreground(GIMP_CONTEXT(options),color);g_object_unref(color);
    core=GIMP_PAINTER_SMUDGE(g_object_new(GIMP_TYPE_PAINTER_SMUDGE,"undo-desc","smudge test",nullptr));coords.x=32;coords.y=27;coords.pressure=1;gimp_image_undo_free(image);
  }
  ~Scene(){if(core)g_object_unref(core);g_object_unref(options);g_object_unref(layer);if(image){g_object_remove_weak_pointer(G_OBJECT(image),reinterpret_cast<gpointer*>(&image));if(owns_image)g_object_unref(image);}}
  std::vector<guchar> pixels(){std::vector<guchar> result(initial.size());gegl_buffer_get(gimp_drawable_get_buffer(GIMP_DRAWABLE(layer)),GEGL_RECTANGLE(0,0,64,48),1,babl_format("R'G'B'A u8"),result.data(),GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);return result;}
  void begin(){GError* error=nullptr;g_assert_true(gimp_painter_smudge_begin(core,GIMP_DRAWABLE(layer),options,&coords,&error));g_assert_no_error(error);}
  void motion(){GError* error=nullptr;g_assert_true(gimp_painter_smudge_motion(core,&coords,0,&error));g_assert_no_error(error);}
  void finish(bool commit){GError* error=nullptr;g_assert_true(gimp_painter_smudge_finish(core,commit,&error));g_assert_no_error(error);}
};
static void reuse_and_undo(){Scene s;for(int n=0;n<2;++n){s.begin();s.motion();s.coords.x+=4;s.motion();s.finish(true);g_assert(s.pixels()!=s.initial);g_assert_cmpint(gimp_undo_stack_get_depth(gimp_image_get_undo_stack(s.image)),==,1);g_assert_true(gimp_image_undo(s.image));g_assert(s.pixels()==s.initial);g_assert_true(gimp_image_redo(s.image));g_assert_true(gimp_image_undo(s.image));gimp_image_undo_free(s.image);}}
static void cancel(){Scene s;s.begin();s.motion();g_assert(s.pixels()!=s.initial);s.finish(false);g_assert(s.pixels()==s.initial);g_assert_cmpint(gimp_undo_stack_get_depth(gimp_image_get_undo_stack(s.image)),==,0);}
static void dispose(){Scene s;s.begin();s.motion();auto* core=s.core;s.core=nullptr;g_object_unref(core);g_assert(s.pixels()==s.initial);g_assert_false(gimp_viewable_preview_is_frozen(GIMP_VIEWABLE(s.layer)));}
static void cancel_frozen(GObject* object,GParamSpec*,gpointer data){if(gimp_viewable_preview_is_frozen(GIMP_VIEWABLE(object))){auto*s=static_cast<Scene*>(data);GError* error=nullptr;g_assert_false(gimp_painter_smudge_finish(s->core,FALSE,&error));g_assert_no_error(error);}}
static void start_cancel(){Scene s;auto id=g_signal_connect(s.layer,"notify::frozen",G_CALLBACK(cancel_frozen),&s);GError* error=nullptr;g_assert_false(gimp_painter_smudge_begin(s.core,GIMP_DRAWABLE(s.layer),s.options,&s.coords,&error));g_assert_nonnull(error);g_clear_error(&error);g_signal_handler_disconnect(s.layer,id);g_assert_false(gimp_viewable_preview_is_frozen(GIMP_VIEWABLE(s.layer)));s.begin();s.finish(false);}
static void cancel_update(GimpDrawable*,gint,gint,gint,gint,gpointer data){auto*s=static_cast<Scene*>(data);GError* error=nullptr;gimp_painter_smudge_finish(s->core,FALSE,&error);g_assert_no_error(error);}
static void publication_cancel(){Scene s;s.begin();auto id=g_signal_connect(s.layer,"update",G_CALLBACK(cancel_update),&s);GError* error=nullptr;g_assert_false(gimp_painter_smudge_motion(s.core,&s.coords,0,&error));g_assert_nonnull(error);g_clear_error(&error);g_signal_handler_disconnect(s.layer,id);g_assert(s.pixels()==s.initial);g_assert_false(gimp_viewable_preview_is_frozen(GIMP_VIEWABLE(s.layer)));}
static void drop_core(GimpDrawable*,gint,gint,gint,gint,gpointer data){auto*s=static_cast<Scene*>(data);if(s->core){auto*core=s->core;s->core=nullptr;g_object_unref(core);}}
static void caller_core_loss(){Scene s;s.begin();auto id=g_signal_connect(s.layer,"update",G_CALLBACK(drop_core),&s);GError* error=nullptr;gimp_painter_smudge_motion(s.core,&s.coords,0,&error);g_clear_error(&error);g_signal_handler_disconnect(s.layer,id);g_assert_null(s.core);g_assert(s.pixels()==s.initial);}
static void drop_image(GimpImage*,GimpDirtyMask,gpointer data){auto*s=static_cast<Scene*>(data);if(s->owns_image){s->owns_image=false;g_object_unref(s->image);}}
static void caller_image_loss(){Scene s;s.begin();s.motion();g_signal_connect(s.image,"dirty",G_CALLBACK(drop_image),&s);s.finish(true);g_assert_null(s.image);g_assert_null(gimp_item_get_image(GIMP_ITEM(s.layer)));}
static void invalid_axes(){Scene s;double GimpCoords::*axes[]={&GimpCoords::x,&GimpCoords::y,&GimpCoords::pressure,&GimpCoords::xtilt,&GimpCoords::ytilt,&GimpCoords::wheel,&GimpCoords::distance,&GimpCoords::rotation,&GimpCoords::slider,&GimpCoords::velocity,&GimpCoords::direction,&GimpCoords::xscale,&GimpCoords::yscale,&GimpCoords::angle};for(auto axis:axes){auto c=s.coords;c.*axis=std::numeric_limits<double>::quiet_NaN();GError*error=nullptr;g_assert_false(gimp_painter_smudge_begin(s.core,GIMP_DRAWABLE(s.layer),s.options,&c,&error));g_assert_nonnull(error);g_clear_error(&error);}s.begin();s.motion();for(auto axis:axes){auto c=s.coords;c.*axis=std::numeric_limits<double>::infinity();GError*error=nullptr;g_assert_false(gimp_painter_smudge_motion(s.core,&c,1,&error));g_assert_nonnull(error);g_clear_error(&error);}s.finish(false);g_assert(s.pixels()==s.initial);}
static void external_edit(){Scene s;s.begin();s.motion();auto* color=gegl_color_new("blue");gegl_buffer_set_color(gimp_drawable_get_buffer(GIMP_DRAWABLE(s.layer)),GEGL_RECTANGLE(0,0,5,5),color);g_object_unref(color);auto edited=s.pixels();s.coords.x+=3;GError*error=nullptr;g_assert_false(gimp_painter_smudge_motion(s.core,&s.coords,1,&error));g_assert_nonnull(error);g_clear_error(&error);g_assert(s.pixels()==edited);}
static void invalid_vfunc(){Scene s;GIMP_PAINT_CORE_GET_CLASS(s.core)->paint(GIMP_PAINT_CORE(s.core),nullptr,s.options,nullptr,GIMP_PAINT_STATE_MOTION,0);auto*error=gimp_painter_smudge_dup_error(s.core);g_assert_nonnull(error);g_free(error);g_assert(s.pixels()==s.initial);}
static void paused_segment () {
  Scene s;s.begin();g_assert_true(gimp_image_has_pending_paint(s.image));s.motion();
  s.coords.x=1e9;GError* error=nullptr;
  g_assert_true(gimp_painter_smudge_motion_begin(s.core,&s.coords,1,&error));g_assert_no_error(error);
  g_assert_false(gimp_painter_smudge_finish(s.core,TRUE,&error));g_assert_no_error(error);
  for(int n=0;n<8;++n){g_assert_false(gimp_painter_smudge_step(s.core,&error));g_assert_no_error(error);}
  gimp_painter_smudge_cancel_pending(s.core);g_assert(s.pixels()==s.initial);g_assert_false(gimp_image_has_pending_paint(s.image));
}
static void nested_native_start () {
  Scene s;s.begin();s.motion();auto* core=GIMP_PAINT_CORE(s.core);auto* buffer=core->stroke_buffer;auto coords=core->cur_coords;
  GList list={s.layer,nullptr,nullptr};GError* error=nullptr;auto next=coords;next.x+=18;
  g_assert_false(gimp_paint_core_start(core,&list,s.options,&next,&error));g_assert_nonnull(error);g_clear_error(&error);
  g_assert_true(buffer==core->stroke_buffer);g_assert_cmpfloat(core->cur_coords.x,==,coords.x);s.finish(false);g_assert(s.pixels()==s.initial);
}
static void unowned_start_refused () {
  Scene s;GList list={s.layer,nullptr,nullptr};GError*error=nullptr;
  auto*core=GIMP_PAINT_CORE(s.core);
  g_assert_false(gimp_paint_core_start(core,&list,s.options,&s.coords,&error));g_assert_nonnull(error);g_clear_error(&error);
  g_assert_false(GIMP_PAINT_CORE_GET_CLASS(core)->start(core,&list,s.options,&s.coords,&error));g_assert_nonnull(error);g_clear_error(&error);
  g_assert_null(core->stroke_buffer);g_assert(s.pixels()==s.initial);g_assert_false(gimp_viewable_preview_is_frozen(GIMP_VIEWABLE(s.layer)));
}
int main(int argc,char**argv){g_test_init(&argc,&argv,nullptr);gimp=gimp_init_for_testing();g_test_add_func("/painter/smudge/reuse-undo",reuse_and_undo);g_test_add_func("/painter/smudge/cancel",cancel);g_test_add_func("/painter/smudge/dispose",dispose);g_test_add_func("/painter/smudge/start-cancel",start_cancel);g_test_add_func("/painter/smudge/publication-cancel",publication_cancel);g_test_add_func("/painter/smudge/core-loss",caller_core_loss);g_test_add_func("/painter/smudge/image-loss",caller_image_loss);g_test_add_func("/painter/smudge/invalid-axes",invalid_axes);g_test_add_func("/painter/smudge/external-edit",external_edit);g_test_add_func("/painter/smudge/invalid-vfunc",invalid_vfunc);g_test_add_func("/painter/smudge/paused-segment",paused_segment);g_test_add_func("/painter/smudge/nested-native-start",nested_native_start);g_test_add_func("/painter/smudge/unowned-start-refused",unowned_start_refused);return g_test_run();}
