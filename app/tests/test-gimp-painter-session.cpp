/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gtk/gtk.h>
#include <vector>
extern "C" {
#include "core/core-types.h"
#include "widgets/widgets-types.h"
#include "core/gimp.h"
#include "core/gimpimage.h"
#include "core/gimpimage-undo.h"
#include "core/gimpundostack.h"
#include "core/gimplayer.h"
#include "core/gimplayer-new.h"
#include "core/gimpcontext.h"
#include "tests.h"
#include "gimp-app-test-utils.h"
}
#include "paint/painter-mypaint-surface/gimp-painter-options.hpp"
#include "paint/painter-mypaint-surface/gimp-painter-session.hpp"
#include "paint/painter-mypaint-surface/paint-core.hpp"
#include "painter/gimp-painter-binding.h"
using namespace GimpPainter;
static Gimp*gimp;
static GimpPainterMybrushOptions*options_new()
{
  auto*options=GIMP_PAINTER_MYBRUSH_OPTIONS(g_object_new(GIMP_TYPE_PAINTER_MYBRUSH_OPTIONS,"gimp",gimp,nullptr));
  g_object_set(options,"dabs-per-second",40.,"radius-logarithmic",1.5,nullptr);return options;
}
static GimpLayer*layer_new(GimpImage**image)
{
  *image=gimp_image_new(gimp,64,48,GIMP_RGB,GIMP_PRECISION_U8_NON_LINEAR);
  auto*layer=gimp_layer_new(*image,64,48,babl_format("R'G'B'A u8"),"Session target",1,GIMP_LAYER_MODE_NORMAL);
  g_assert_true(gimp_image_add_layer(*image,layer,nullptr,0,FALSE));
  gegl_buffer_clear(gimp_drawable_get_buffer(GIMP_DRAWABLE(layer)),nullptr);gimp_image_undo_free(*image);return layer;
}
static std::vector<guchar>pixels(GimpDrawable*drawable)
{
  std::vector<guchar>v(64*48*4);gegl_buffer_get(gimp_drawable_get_buffer(drawable),GEGL_RECTANGLE(0,0,64,48),1,babl_format("R'G'B'A u8"),v.data(),GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);return v;
}
static void stroke(GimpPainterSession*session,GimpDrawable*drawable)
{
  GError*error=nullptr;GimpCoords c=GIMP_COORDS_DEFAULT_VALUES;c.x=16;c.y=16;c.pressure=0;
  g_assert_true(gimp_painter_session_stroke_to(session,drawable,.01,&c,nullptr,&error));g_assert_no_error(error);
  c.pressure=.85;g_assert_true(gimp_painter_session_stroke_to(session,drawable,.05,&c,nullptr,&error));g_assert_no_error(error);
  for(int i=0;i<8;++i){c.x+=2;c.y+=1;g_assert_true(gimp_painter_session_stroke_to(session,drawable,.01,&c,nullptr,&error));g_assert_no_error(error);}
}
static void exact_controller_pixels()
{
  for(bool floating:{false,true}) {
    auto*options=options_new();g_object_set(options,"non-incremental",floating,"stroke-opacity",.37,nullptr);GError*error=nullptr;
    auto*session=gimp_painter_session_new(options,&error);g_assert_no_error(error);g_assert_nonnull(session);
    GimpImage*image;auto*layer=layer_new(&image);auto*drawable=GIMP_DRAWABLE(layer);const auto before=pixels(drawable);
    stroke(session,drawable);g_assert_true(gimp_painter_session_is_active(session));g_assert_true(gimp_painter_session_finish(session,&error));g_assert_no_error(error);
    const auto actual=pixels(drawable);g_assert_true(actual!=before);g_assert_cmpint(gimp_undo_stack_get_depth(gimp_image_get_undo_stack(image)),==,1);
    g_assert_true(gimp_image_undo(image));g_assert_true(pixels(drawable)==before);
    MyPaint::PaintCore core(GIMP_PAINT_OPTIONS(options),PainterOptionsRef::retain(options).snapshot());GimpCoords c=GIMP_COORDS_DEFAULT_VALUES;c.x=16;c.y=16;c.pressure=0;core.stroke_to(drawable,.01,c);
    c.pressure=.85;core.stroke_to(drawable,.05,c);for(int i=0;i<8;++i){c.x+=2;c.y+=1;core.stroke_to(drawable,.01,c);}core.finish();
    g_assert_true(pixels(drawable)==actual);stroke(session,drawable);g_assert_true(gimp_painter_session_cancel(session,&error));g_assert_no_error(error);g_assert_true(pixels(drawable)==actual);
    g_object_unref(session);g_object_unref(options);g_object_unref(image);
  }
}
static void setting_splits_and_blocks()
{
  auto*options=options_new();auto*session=gimp_painter_session_new(options,nullptr);GimpImage*image;auto*layer=layer_new(&image);auto*drawable=GIMP_DRAWABLE(layer);
  stroke(session,drawable);g_object_set(options,"radius-logarithmic",2.,nullptr);g_assert_false(gimp_painter_session_is_active(session));
  const auto first=pixels(drawable);g_assert_cmpint(gimp_undo_stack_get_depth(gimp_image_get_undo_stack(image)),==,1);
  stroke(session,drawable);const auto painted=pixels(drawable);g_assert_true(first!=painted);
  GError*error=nullptr;g_assert_true(gimp_painter_mybrush_options_set_json(options,"{\"version\":3,\"settings\":{\"future_extension\":{\"base_value\":1}}}",&error));g_assert_no_error(error);
  String reason(gimp_painter_session_dup_error(session));g_assert_nonnull(reason.get());g_assert_false(gimp_painter_session_is_active(session));
  GimpCoords c=GIMP_COORDS_DEFAULT_VALUES;c.x=10;c.y=10;g_assert_false(gimp_painter_session_stroke_to(session,drawable,.1,&c,nullptr,&error));g_assert_nonnull(error);g_clear_error(&error);g_assert_true(pixels(drawable)==painted);
  g_assert_cmpint(gimp_undo_stack_get_depth(gimp_image_get_undo_stack(image)),==,2);
  g_assert_true(gimp_painter_mybrush_options_set_json(options,"{\"version\":3,\"settings\":{}}",&error));g_assert_no_error(error);g_assert_null(gimp_painter_session_dup_error(session));
  g_object_unref(session);g_object_unref(options);g_object_unref(image);
}
struct Closing {GimpPainterSession*session;bool fired=false;};
static void close_on_freeze(GObject*object,GParamSpec*,gpointer data)
{
  auto&s=*static_cast<Closing*>(data);if(s.fired||!gimp_viewable_preview_is_frozen(GIMP_VIEWABLE(object)))return;s.fired=true;
  GError*error=nullptr;g_assert_true(gimp_painter_binding_close(G_OBJECT(s.session),&error));g_assert_no_error(error);
}
static void close_during_native_start()
{
  auto*options=options_new();auto*session=gimp_painter_session_new(options,nullptr);GimpImage*image;auto*layer=layer_new(&image);const auto before=pixels(GIMP_DRAWABLE(layer));
  Closing state{session};auto id=g_signal_connect(layer,"notify::frozen",G_CALLBACK(close_on_freeze),&state);GimpCoords c=GIMP_COORDS_DEFAULT_VALUES;c.x=16;c.y=16;
  GError*error=nullptr;g_assert_true(gimp_painter_session_stroke_to(session,GIMP_DRAWABLE(layer),.1,&c,nullptr,&error));g_assert_no_error(error);g_assert_true(state.fired);
  g_assert_true(pixels(GIMP_DRAWABLE(layer))==before);g_assert_false(gimp_viewable_preview_is_frozen(GIMP_VIEWABLE(layer)));
  g_assert_false(gimp_painter_session_stroke_to(session,GIMP_DRAWABLE(layer),.1,&c,nullptr,&error));g_assert_nonnull(error);g_clear_error(&error);
  g_signal_handler_disconnect(layer,id);g_object_unref(session);g_object_unref(options);g_object_unref(image);
}
struct Change {GimpPainterMybrushOptions*options;bool fired=false;};
static void change_on_update(GimpDrawable*,gint,gint,gint,gint,gpointer data)
{auto&s=*static_cast<Change*>(data);if(s.fired)return;s.fired=true;g_object_set(s.options,"radius-logarithmic",2.1,nullptr);}
static void options_change_during_motion()
{
  auto*options=options_new();auto*session=gimp_painter_session_new(options,nullptr);GimpImage*image;auto*layer=layer_new(&image);Change state{options};
  auto id=g_signal_connect(layer,"update",G_CALLBACK(change_on_update),&state);stroke(session,GIMP_DRAWABLE(layer));g_assert_true(state.fired);g_assert_null(gimp_painter_session_dup_error(session));
  g_assert_true(gimp_painter_session_finish(session,nullptr));g_assert_cmpint(gimp_undo_stack_get_depth(gimp_image_get_undo_stack(image)),==,2);
  g_signal_handler_disconnect(layer,id);g_object_unref(session);g_object_unref(options);g_object_unref(image);
}
static void closed_options_cancel()
{
  auto*options=options_new();auto*session=gimp_painter_session_new(options,nullptr);GimpImage*image;auto*layer=layer_new(&image);const auto before=pixels(GIMP_DRAWABLE(layer));stroke(session,GIMP_DRAWABLE(layer));
  g_assert_true(gimp_painter_binding_close(G_OBJECT(options),nullptr));GimpCoords c=GIMP_COORDS_DEFAULT_VALUES;c.x=22;c.y=22;GError*error=nullptr;
  g_assert_false(gimp_painter_session_stroke_to(session,GIMP_DRAWABLE(layer),.1,&c,nullptr,&error));g_assert_error(error,GIMP_PAINTER_ERROR,GIMP_PAINTER_ERROR_CLOSED);g_clear_error(&error);
  g_assert_true(pixels(GIMP_DRAWABLE(layer))==before);g_assert_false(gimp_viewable_preview_is_frozen(GIMP_VIEWABLE(layer)));g_object_unref(session);g_object_unref(options);g_object_unref(image);
}
static void finalize_mark(gpointer data,GObject*){*static_cast<bool*>(data)=true;}
static void drop_session(GObject*,GParamSpec*,gpointer data){g_clear_object(static_cast<GimpPainterSession**>(data));}
static void notification_drops_last_ref()
{
  auto*options=options_new();auto*session=gimp_painter_session_new(options,nullptr);GimpImage*image;auto*layer=layer_new(&image);stroke(session,GIMP_DRAWABLE(layer));
  bool finalized=false;g_object_weak_ref(G_OBJECT(session),finalize_mark,&finalized);g_signal_connect(session,"notify::active",G_CALLBACK(drop_session),&session);
  g_assert_true(gimp_painter_session_finish(session,nullptr));g_assert_null(session);g_assert_true(finalized);g_assert_cmpint(gimp_undo_stack_get_depth(gimp_image_get_undo_stack(image)),==,1);g_object_unref(options);g_object_unref(image);
}
struct StopDuringSample {GimpPainterSession*session;bool cancel;bool fired=false;};
static void stop_on_update(GimpDrawable*,gint,gint,gint,gint,gpointer data)
{
  auto&s=*static_cast<StopDuringSample*>(data);if(s.fired)return;s.fired=true;GError*error=nullptr;
  g_assert_true(s.cancel?gimp_painter_session_cancel(s.session,&error):gimp_painter_session_finish(s.session,&error));g_assert_no_error(error);
  // A later HALT/finish must not override an earlier cancellation.
  if(s.cancel){g_assert_true(gimp_painter_session_finish(s.session,&error));g_assert_no_error(error);}
}
static void stop_during_sample()
{
  for(bool cancel:{false,true}) {
    auto*options=options_new();auto*session=gimp_painter_session_new(options,nullptr);GimpImage*image;auto*layer=layer_new(&image);auto*drawable=GIMP_DRAWABLE(layer);const auto before=pixels(drawable);
    GimpCoords c=GIMP_COORDS_DEFAULT_VALUES;c.x=16;c.y=16;c.pressure=0;g_assert_true(gimp_painter_session_stroke_to(session,drawable,.01,&c,nullptr,nullptr));
    StopDuringSample state{session,cancel};auto id=g_signal_connect(layer,"update",G_CALLBACK(stop_on_update),&state);c.pressure=1;c.x+=20;GError*error=nullptr;
    g_assert_true(gimp_painter_session_stroke_to(session,drawable,.1,&c,nullptr,&error));g_assert_no_error(error);g_assert_true(state.fired);g_assert_false(gimp_painter_session_is_active(session));
    g_assert_cmpint(gimp_undo_stack_get_depth(gimp_image_get_undo_stack(image)),==,cancel?0:1);g_assert_true((pixels(drawable)==before)==cancel);g_assert_false(gimp_viewable_preview_is_frozen(GIMP_VIEWABLE(layer)));
    g_signal_handler_disconnect(layer,id);g_object_unref(session);g_object_unref(options);g_object_unref(image);
  }
}
static void request_cancel_on_freeze(GObject*object,GParamSpec*,gpointer data)
{
  auto&s=*static_cast<Closing*>(data);if(s.fired||!gimp_viewable_preview_is_frozen(GIMP_VIEWABLE(object)))return;s.fired=true;GError*error=nullptr;
  g_assert_true(gimp_painter_session_cancel(s.session,&error));g_assert_no_error(error);
}
static void cancel_during_native_start()
{
  auto*options=options_new();auto*session=gimp_painter_session_new(options,nullptr);GimpImage*image;auto*layer=layer_new(&image);const auto before=pixels(GIMP_DRAWABLE(layer));Closing state{session};
  auto id=g_signal_connect(layer,"notify::frozen",G_CALLBACK(request_cancel_on_freeze),&state);GimpCoords c=GIMP_COORDS_DEFAULT_VALUES;c.x=16;c.y=16;
  g_assert_true(gimp_painter_session_stroke_to(session,GIMP_DRAWABLE(layer),.1,&c,nullptr,nullptr));g_assert_true(state.fired);g_assert_false(gimp_painter_session_is_active(session));g_assert_true(pixels(GIMP_DRAWABLE(layer))==before);g_assert_cmpint(gimp_undo_stack_get_depth(gimp_image_get_undo_stack(image)),==,0);
  g_signal_handler_disconnect(layer,id);g_object_unref(session);g_object_unref(options);g_object_unref(image);
}
int main(int argc,char**argv)
{
  g_test_init(&argc,&argv,nullptr);gimp_test_utils_set_gimp3_directory("GIMP_TESTING_ABS_TOP_SRCDIR","app/tests/gimpdir");gimp=gimp_init_for_testing();
  g_test_add_func("/painter-session/controller-pixels",exact_controller_pixels);
  g_test_add_func("/painter-session/settings-split-block",setting_splits_and_blocks);
  g_test_add_func("/painter-session/close-start",close_during_native_start);
  g_test_add_func("/painter-session/settings-during-motion",options_change_during_motion);
  g_test_add_func("/painter-session/closed-options-cancel",closed_options_cancel);
  g_test_add_func("/painter-session/notification-last-ref",notification_drops_last_ref);
  g_test_add_func("/painter-session/deferred-sample-stop",stop_during_sample);
  g_test_add_func("/painter-session/cancel-native-start",cancel_during_native_start);
  int result=g_test_run();gimp_test_utils_set_gimp3_directory("GIMP_TESTING_ABS_TOP_BUILDDIR","app/tests/gimpdir-output");gimp_exit(gimp,TRUE);return result;
}
