/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gtk/gtk.h>
#include <gegl.h>
#include <vector>
#include <limits>
#include <cmath>
extern "C" {
#include "core/core-types.h"
#include "widgets/widgets-types.h"
#include "core/gimp.h"
#include "core/gimpcontext.h"
#include "core/gimpimage.h"
#include "core/gimpimage-undo.h"
#include "core/gimpundostack.h"
#include "core/gimplayer.h"
#include "core/gimplayer-new.h"
#include "core/gimpchannel.h"
#include "core/gimpchannel-select.h"
#include "core/gimpboundary.h"
#include "paint/gimppaintcore-stroke.h"
#include "paint/gimppainterpaintgate.h"
#include "vectors/gimppath.h"
#include "vectors/gimpstroke.h"
#include "vectors/gimpbezierstroke.h"
#include "tests.h"
#include "gimp-app-test-utils.h"
}
#include "paint/painter-mypaint-surface/paint-core.hpp"
#include "paint/painter-mypaint-surface/gimp-painter-options.hpp"
#include "paint/painter-mypaint-surface/gimp-painter-session.hpp"
using namespace GimpPainter;
using namespace GimpPainter::MyPaint;
static Gimp*gimp;
struct Scene {
  GimpImage*image;GimpDrawable*drawable;GimpPainterMybrushOptions*options;
  Scene(bool floating=false,bool selected=false){
    image=gimp_image_new(gimp,96,64,GIMP_RGB,GIMP_PRECISION_U8_NON_LINEAR);
    auto*layer=gimp_layer_new(image,80,48,babl_format("R'G'B'A u8"),"Atomic target",1,GIMP_LAYER_MODE_NORMAL);g_assert_true(gimp_image_add_layer(image,layer,nullptr,0,FALSE));gimp_item_set_offset(GIMP_ITEM(layer),7,3);drawable=GIMP_DRAWABLE(layer);
    std::vector<guchar>initial(80*48*4);for(std::size_t i=0;i<initial.size();i+=4){initial[i]=40;initial[i+1]=90;initial[i+2]=170;initial[i+3]=180;}
    gegl_buffer_set(gimp_drawable_get_buffer(drawable),GEGL_RECTANGLE(0,0,80,48),0,babl_format("R'G'B'A u8"),initial.data(),GEGL_AUTO_ROWSTRIDE);
    if(selected)gimp_channel_select_rectangle(gimp_image_get_mask(image),35,0,60,64,GIMP_CHANNEL_OP_REPLACE,FALSE,0,0,FALSE);
    options=GIMP_PAINTER_MYBRUSH_OPTIONS(g_object_new(GIMP_TYPE_PAINTER_MYBRUSH_OPTIONS,"gimp",gimp,nullptr));
    g_object_set(options,"non-incremental",floating,"dabs-per-second",40.,"radius-logarithmic",1.5,"stroke-opacity",.37,"smudge",.4,nullptr);
    auto*color=gegl_color_new("red");gimp_context_set_foreground(GIMP_CONTEXT(options),color);g_object_unref(color);gimp_image_undo_free(image);
  }
  ~Scene(){g_clear_object(&options);g_object_unref(image);}
  Resource resource(){return PainterOptionsRef::retain(options).snapshot();}
  std::vector<guchar>pixels(){std::vector<guchar>p(80*48*4);gegl_buffer_get(gimp_drawable_get_buffer(drawable),GEGL_RECTANGLE(0,0,80,48),1,babl_format("R'G'B'A u8"),p.data(),GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);return p;}
  int undo(){return gimp_undo_stack_get_depth(gimp_image_get_undo_stack(image));}
  int redo(){return gimp_undo_stack_get_depth(gimp_image_get_redo_stack(image));}
};
static GimpCoords point(double x,double y,double pressure=1)
{GimpCoords c=GIMP_COORDS_DEFAULT_VALUES;c.x=x;c.y=y;c.pressure=pressure;return c;}
static void stroke(PaintCore&core,Scene&s,double x,double y)
{
  auto c=point(x,y,0);core.hover_to(s.drawable,1,c);c.pressure=.9;core.stroke_to(s.drawable,.015,c);
  for(int i=0;i<12;++i){c.x+=1.5;c.y+=.25;core.stroke_to(s.drawable,.012,c);}
}
struct FreezeCount{int freeze=0,thaw=0;};
static void count_freeze(GObject*o,GParamSpec*,gpointer d)
{auto&count=*static_cast<FreezeCount*>(d);if(gimp_viewable_preview_is_frozen(GIMP_VIEWABLE(o)))++count.freeze;else ++count.thaw;}
static void one_transaction_logical_segments()
{
  for(bool floating:{false,true})for(bool selection:{false,true}){
    Scene reference(floating,selection),actual(floating,selection);const auto before=actual.pixels();
    for(int i=0;i<2;++i){PaintCore control(GIMP_PAINT_OPTIONS(reference.options),reference.resource());stroke(control,reference,24+18*i,16+10*i);control.finish();}
    FreezeCount transitions;auto handler=g_signal_connect(actual.drawable,"notify::frozen",G_CALLBACK(count_freeze),&transitions);
    PaintCore core(GIMP_PAINT_OPTIONS(actual.options),actual.resource());core.begin_batch(true);stroke(core,actual,24,16);core.next_segment();stroke(core,actual,42,26);
    g_assert_cmpint(actual.undo(),==,0);g_assert_true(core.active());core.end_batch(true);g_assert_true(actual.pixels()==reference.pixels());g_assert_cmpint(actual.undo(),==,1);
    g_assert_cmpint(transitions.freeze,==,1);g_assert_cmpint(transitions.thaw,==,1);g_signal_handler_disconnect(actual.drawable,handler);
    const auto painted=actual.pixels();g_assert_true(gimp_image_undo(actual.image));g_assert_true(actual.pixels()==before);g_assert_true(gimp_image_redo(actual.image));g_assert_true(actual.pixels()==painted);
  }
}
static void error_preserves_undo_redo()
{
  Scene s(true);{PaintCore c(GIMP_PAINT_OPTIONS(s.options),s.resource());stroke(c,s,15,12);c.finish();}
  {PaintCore c(GIMP_PAINT_OPTIONS(s.options),s.resource());stroke(c,s,48,30);c.finish();}
  const auto redo_pixels=s.pixels();g_assert_true(gimp_image_undo(s.image));const auto before=s.pixels();g_assert_cmpint(s.undo(),==,1);g_assert_cmpint(s.redo(),==,1);
  PaintCore core(GIMP_PAINT_OPTIONS(s.options),s.resource());core.begin_batch(true);stroke(core,s,25,20);g_assert_true(s.pixels()!=before);core.next_segment();
  bool rejected=false;auto invalid=point(std::numeric_limits<double>::quiet_NaN(),20);
  try{core.stroke_to(s.drawable,.01,invalid);}catch(const std::invalid_argument&){rejected=true;}
  g_assert_true(rejected);core.end_batch(false);g_assert_true(s.pixels()==before);g_assert_cmpint(s.undo(),==,1);g_assert_cmpint(s.redo(),==,1);
  g_assert_false(gimp_viewable_preview_is_frozen(GIMP_VIEWABLE(s.drawable)));g_assert_true(gimp_image_redo(s.image));g_assert_true(s.pixels()==redo_pixels);
}
static void no_undo_and_noop_lock()
{
  Scene s;const auto before=s.pixels();PaintCore core(GIMP_PAINT_OPTIONS(s.options),s.resource());core.begin_batch(false);stroke(core,s,20,20);core.end_batch(true);g_assert_true(s.pixels()!=before);g_assert_cmpint(s.undo(),==,0);g_assert_cmpint(s.redo(),==,0);
  for(bool lock:{false,true}){
    Scene empty(lock);auto r=empty.resource();if(lock)gimp_layer_set_lock_alpha(GIMP_LAYER(empty.drawable),TRUE,FALSE);else r.set_base_value(BRUSH_OPAQUE,0);
    const auto initial=empty.pixels();PaintCore c(GIMP_PAINT_OPTIONS(empty.options),r);c.begin_batch(true);stroke(c,empty,25,20);c.end_batch(true);
    g_assert_true(empty.pixels()==initial);g_assert_cmpint(empty.undo(),==,lock?1:0);
  }
}
static void sample_cancel(GimpDrawable*,gint,gint,gint,gint,gpointer data)
{static_cast<PaintCore*>(data)->cancel();}
static void canceled_batch_cannot_commit()
{
  Scene s;const auto initial=s.pixels();PaintCore core(GIMP_PAINT_OPTIONS(s.options),s.resource());core.begin_batch(true);
  auto handler=g_signal_connect(s.drawable,"update",G_CALLBACK(sample_cancel),&core);bool rejected=false;
  try{stroke(core,s,25,20);}catch(const std::exception&){rejected=true;}
  g_signal_handler_disconnect(s.drawable,handler);try{core.end_batch(true);}catch(const std::runtime_error&){rejected=true;}
  g_assert_true(rejected);g_assert_true(s.pixels()==initial);g_assert_cmpint(s.undo(),==,0);g_assert_false(core.active());
  core.begin_batch(true);stroke(core,s,25,20);core.end_batch(true);g_assert_cmpint(s.undo(),==,1);
}
static void session_setting_change_aborts()
{
  Scene s;const auto initial=s.pixels();auto*session=gimp_painter_session_new(s.options,nullptr);GError*error=nullptr;g_assert_true(gimp_painter_session_begin_batch(session,TRUE,&error));
  auto c=point(24,20,0);g_assert_true(gimp_painter_session_hover_to(session,s.drawable,1,&c,nullptr,&error));c.pressure=.9;
  for(int i=0;i<12;++i){c.x+=1;g_assert_true(gimp_painter_session_stroke_to(session,s.drawable,.025,&c,nullptr,&error));}
  g_assert_true(s.pixels()!=initial);g_object_set(s.options,"radius-logarithmic",2.,nullptr);
  g_assert_false(gimp_painter_session_end_batch(session,TRUE,&error));g_assert_nonnull(error);g_clear_error(&error);g_assert_true(s.pixels()==initial);g_assert_cmpint(s.undo(),==,0);
  g_assert_true(gimp_painter_session_begin_batch(session,FALSE,&error));g_assert_true(gimp_painter_session_end_batch(session,FALSE,&error));g_assert_no_error(error);g_object_unref(session);
}
static GimpPaintCore*gate_new()
{return GIMP_PAINT_CORE(g_object_new(GIMP_TYPE_PAINTER_PAINT_GATE,"undo-desc","Atomic test",nullptr));}
static std::vector<GimpCoords>line(double x,double y)
{std::vector<GimpCoords>v;for(int i=0;i<20;++i)v.push_back(point(x+i,y+i*.15));return v;}
static void reference_segment(PaintCore&core,Scene&s,const std::vector<GimpCoords>&v)
{
  core.next_segment();auto previous=v.front();previous.x-=7;previous.y-=3;auto hover=previous;hover.pressure=0;core.hover_to(s.drawable,1,hover);
  for(std::size_t i=0;i<v.size();++i){auto c=v[i];c.x-=7;c.y-=3;const double dt=i?.0005*std::hypot(c.x-previous.x,c.y-previous.y):.015;core.stroke_to(s.drawable,dt,c);previous=c;}
}
static void raw_stroke_dispatch()
{
  for(bool floating:{false,true})for(bool selection:{false,true}){
    Scene s(floating,selection),reference(floating,selection);const auto initial=s.pixels();auto coords=line(33,22);const auto unchanged=coords;
    PaintCore expected(GIMP_PAINT_OPTIONS(reference.options),reference.resource());expected.begin_batch(true);reference_segment(expected,reference,coords);expected.end_batch(true);
    auto*gate=gate_new();GError*error=nullptr;g_assert_true(gimp_paint_core_stroke(gate,s.drawable,GIMP_PAINT_OPTIONS(s.options),coords.data(),coords.size(),TRUE,&error));g_assert_no_error(error);
    g_assert_true(s.pixels()==reference.pixels());g_assert_true(s.pixels()!=initial);g_assert_cmpint(s.undo(),==,1);g_assert_cmpstr(gimp_object_get_name(GIMP_OBJECT(gimp_undo_stack_peek(gimp_image_get_undo_stack(s.image)))),==,"Atomic test");g_assert_cmpfloat(gate->last_coords.x,==,coords.back().x);g_assert_cmpfloat(gate->last_coords.y,==,coords.back().y);g_assert_cmpmem(coords.data(),coords.size()*sizeof(GimpCoords),unchanged.data(),unchanged.size()*sizeof(GimpCoords));
    g_assert_false(gimp_image_has_pending_paint(s.image));g_object_unref(gate);
  }
}
static GimpPath*path_new(Scene&s)
{
  auto*path=gimp_path_new(s.image,"Atomic subpaths");
  for(int i=0;i<2;++i){auto begin=point(24,15+24*i),end=point(65,15+24*i);auto*stroke=gimp_bezier_stroke_new_moveto(&begin);gimp_bezier_stroke_lineto(stroke,&end);gimp_path_stroke_add(path,stroke);g_object_unref(stroke);}
  return path;
}
static void raw_path_and_boundary()
{
  Scene s(true,true);auto*gate=gate_new();auto*path=path_new(s);const auto initial=s.pixels();GError*error=nullptr;
  for(GList*l=path->strokes->head;l;l=l->next){gboolean closed;auto*a=gimp_stroke_interpolate(GIMP_STROKE(l->data),1.,&closed);if(a&&a->len){auto*cs=reinterpret_cast<GimpCoords*>(a->data);g_test_message("path n=%u first=(%g,%g,%g) last=(%g,%g,%g)",a->len,cs[0].x,cs[0].y,cs[0].pressure,cs[a->len-1].x,cs[a->len-1].y,cs[a->len-1].pressure);}if(a)g_array_unref(a);}
  g_assert_true(gimp_paint_core_stroke_path(gate,s.drawable,GIMP_PAINT_OPTIONS(s.options),TRUE,path,TRUE,&error));g_assert_no_error(error);const auto painted=s.pixels();g_assert_true(painted!=initial);g_assert_cmpint(s.undo(),==,1);
  g_assert_true(gimp_image_undo(s.image));g_assert_true(s.pixels()==initial);g_assert_true(gimp_image_redo(s.image));g_assert_true(s.pixels()==painted);g_object_unref(path);
  GimpBoundSeg boundary[]={{30,12,60,12,1,0},{60,12,60,35,1,0},{60,35,30,35,1,0},{30,35,30,12,1,0}};
  g_assert_true(gimp_paint_core_stroke_boundary(gate,s.drawable,GIMP_PAINT_OPTIONS(s.options),FALSE,boundary,4,0,0,TRUE,&error));g_assert_no_error(error);g_assert_cmpint(s.undo(),==,2);g_assert_true(s.pixels()!=painted);
  g_assert_true(gimp_image_undo(s.image));g_assert_true(s.pixels()==painted);g_object_unref(gate);
}
struct LaterFailure{Scene*scene;bool fired=false;};
static void fail_later_path(GimpDrawable*,gint,gint y,gint,gint,gpointer data)
{auto&s=*static_cast<LaterFailure*>(data);if(!s.fired&&y>25){s.fired=true;g_object_set(s.scene->options,"radius-logarithmic",2.,nullptr);}}
static void raw_later_path_failure()
{
  Scene s(true);{PaintCore c(GIMP_PAINT_OPTIONS(s.options),s.resource());stroke(c,s,15,12);c.finish();}{PaintCore c(GIMP_PAINT_OPTIONS(s.options),s.resource());stroke(c,s,48,30);c.finish();}
  const auto redo=s.pixels();g_assert_true(gimp_image_undo(s.image));const auto initial=s.pixels();auto*gate=gate_new();auto*path=path_new(s);LaterFailure state{&s};auto handler=g_signal_connect(s.drawable,"update",G_CALLBACK(fail_later_path),&state);GError*error=nullptr;
  g_assert_false(gimp_paint_core_stroke_path(gate,s.drawable,GIMP_PAINT_OPTIONS(s.options),FALSE,path,TRUE,&error));g_assert_true(state.fired);g_assert_nonnull(error);g_clear_error(&error);g_signal_handler_disconnect(s.drawable,handler);
  g_assert_true(s.pixels()==initial);g_assert_cmpint(s.undo(),==,1);g_assert_cmpint(s.redo(),==,1);g_assert_false(gimp_image_has_pending_paint(s.image));g_assert_true(gimp_image_redo(s.image));g_assert_true(s.pixels()==redo);
  g_object_unref(path);g_object_unref(gate);
}
struct Reentry{Scene*scene;GimpPaintCore*gate;bool fired=false;bool dispose=false;};
static void raw_reentry(GimpDrawable*,gint,gint,gint,gint,gpointer data)
{
  auto&r=*static_cast<Reentry*>(data);if(r.fired)return;r.fired=true;g_assert_true(gimp_image_has_pending_paint(r.scene->image));
  if(r.dispose){g_object_run_dispose(G_OBJECT(r.gate));g_assert_true(gimp_image_has_pending_paint(r.scene->image));return;}
  auto coords=line(20,30);GError*error=nullptr;g_assert_false(gimp_paint_core_stroke(r.gate,r.scene->drawable,GIMP_PAINT_OPTIONS(r.scene->options),coords.data(),coords.size(),TRUE,&error));g_assert_nonnull(error);g_clear_error(&error);
}
static void raw_reentry_and_dispose()
{
  for(bool dispose:{false,true}){Scene s;const auto initial=s.pixels();auto*gate=gate_new();Reentry state{&s,gate,false,dispose};auto handler=g_signal_connect(s.drawable,"update",G_CALLBACK(raw_reentry),&state);auto coords=line(28,20);GError*error=nullptr;
    const bool success=gimp_paint_core_stroke(gate,s.drawable,GIMP_PAINT_OPTIONS(s.options),coords.data(),coords.size(),TRUE,&error);g_assert_true(state.fired);g_assert_cmpint(success,==,!dispose);
    if(dispose){g_assert_nonnull(error);g_clear_error(&error);g_assert_true(s.pixels()==initial);g_assert_cmpint(s.undo(),==,0);}else{g_assert_no_error(error);g_assert_true(s.pixels()!=initial);g_assert_cmpint(s.undo(),==,1);}
    g_assert_false(gimp_image_has_pending_paint(s.image));g_signal_handler_disconnect(s.drawable,handler);g_object_unref(gate);
  }
}
static void raw_no_undo_and_empty_path()
{
  Scene s;{PaintCore c(GIMP_PAINT_OPTIONS(s.options),s.resource());stroke(c,s,15,12);c.finish();}{PaintCore c(GIMP_PAINT_OPTIONS(s.options),s.resource());stroke(c,s,48,30);c.finish();}g_assert_true(gimp_image_undo(s.image));const auto before=s.pixels();
  auto*gate=gate_new();auto coords=line(28,20);GError*error=nullptr;g_assert_true(gimp_paint_core_stroke(gate,s.drawable,GIMP_PAINT_OPTIONS(s.options),coords.data(),coords.size(),FALSE,&error));g_assert_no_error(error);g_assert_true(s.pixels()!=before);g_assert_cmpint(s.undo(),==,1);g_assert_cmpint(s.redo(),==,1);
  auto*empty=gimp_path_new(s.image,"Empty path");g_assert_false(gimp_paint_core_stroke_path(gate,s.drawable,GIMP_PAINT_OPTIONS(s.options),FALSE,empty,TRUE,nullptr));g_assert_cmpint(s.undo(),==,1);g_assert_cmpint(s.redo(),==,1);g_object_unref(empty);g_object_unref(gate);
}
static void invalid_first_sample_no_native_start()
{
  Scene s;PaintCore core(GIMP_PAINT_OPTIONS(s.options),s.resource());const auto initial=s.pixels();FreezeCount count;auto handler=g_signal_connect(s.drawable,"notify::frozen",G_CALLBACK(count_freeze),&count);
  for(double x:{std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity(),1e8,1e100}){bool rejected=false;auto c=point(x,20);try{core.stroke_to(s.drawable,.01,c);}catch(const std::invalid_argument&){rejected=true;}g_assert_true(rejected);}
  g_assert_cmpint(count.freeze,==,0);g_assert_cmpint(count.thaw,==,0);g_assert_true(s.pixels()==initial);g_assert_cmpint(s.undo(),==,0);g_signal_handler_disconnect(s.drawable,handler);
}
/* Real virtual interpolation callback: the public path wrapper must own the
 * Options before invoking it, even before a Session has been constructed. */
struct LifetimeStroke { GimpStroke parent; GimpPainterMybrushOptions **caller; bool *finalized; gboolean empty; };
struct LifetimeStrokeClass { GimpStrokeClass parent; };
GType lifetime_stroke_get_type(void);
G_DEFINE_TYPE(LifetimeStroke, lifetime_stroke, GIMP_TYPE_STROKE)
static GArray *lifetime_interpolate(GimpStroke *stroke,gdouble,gboolean *closed)
{
  auto*self=reinterpret_cast<LifetimeStroke*>(stroke);if(closed)*closed=FALSE;
  if(self->caller&&*self->caller){g_clear_object(self->caller);g_assert_false(*self->finalized);}
  auto*result=g_array_new(FALSE,FALSE,sizeof(GimpCoords));
  if(!self->empty){auto coords=line(28,20);g_array_append_vals(result,coords.data(),coords.size());}
  return result;
}
static void lifetime_stroke_class_init(LifetimeStrokeClass *klass)
{GIMP_STROKE_CLASS(klass)->interpolate=lifetime_interpolate;}
static void lifetime_stroke_init(LifetimeStroke*){}
static void mark_finalized(gpointer data,GObject*){*static_cast<bool*>(data)=true;}
static void path_options_last_owner_loss()
{
  for(bool empty:{false,true}){
    Scene s;const auto initial=s.pixels();bool finalized=false;
    g_object_weak_ref(G_OBJECT(s.options),mark_finalized,&finalized);
    auto*path=gimp_path_new(s.image,"Options owner loss");
    auto*segment=reinterpret_cast<LifetimeStroke*>(g_object_new(lifetime_stroke_get_type(),nullptr));
    gimp_path_stroke_add(path,GIMP_STROKE(segment));
    segment->caller=&s.options;segment->finalized=&finalized;segment->empty=empty;
    auto*gate=gate_new();GError*error=nullptr;
    const bool result=gimp_paint_core_stroke_path(gate,s.drawable,GIMP_PAINT_OPTIONS(s.options),FALSE,path,TRUE,&error);
    g_assert_cmpint(result,==,!empty);g_assert_null(s.options);g_assert_true(finalized);
    if(empty){g_assert_nonnull(error);g_clear_error(&error);g_assert_true(s.pixels()==initial);g_assert_cmpint(s.undo(),==,0);}
    else{g_assert_no_error(error);g_assert_true(s.pixels()!=initial);g_assert_cmpint(s.undo(),==,1);}
    segment->caller=nullptr;g_object_unref(segment);g_object_unref(path);g_object_unref(gate);
  }
}
static void assert_coords_equal(const GimpCoords&a,const GimpCoords&b)
{
  g_assert_cmpfloat(a.x,==,b.x);g_assert_cmpfloat(a.y,==,b.y);g_assert_cmpfloat(a.pressure,==,b.pressure);
  g_assert_cmpfloat(a.xtilt,==,b.xtilt);g_assert_cmpfloat(a.ytilt,==,b.ytilt);g_assert_cmpfloat(a.wheel,==,b.wheel);
  g_assert_cmpfloat(a.distance,==,b.distance);g_assert_cmpfloat(a.rotation,==,b.rotation);g_assert_cmpfloat(a.slider,==,b.slider);
  g_assert_cmpfloat(a.velocity,==,b.velocity);g_assert_cmpfloat(a.direction,==,b.direction);
  g_assert_cmpfloat(a.xscale,==,b.xscale);g_assert_cmpfloat(a.yscale,==,b.yscale);g_assert_cmpfloat(a.angle,==,b.angle);g_assert_cmpint(a.reflect,==,b.reflect);
}
static void native_start_coordinate_contract()
{
  Scene s;auto*gate=gate_new();auto*native=GIMP_PAINT_CORE(g_object_new(GIMP_TYPE_PAINT_CORE,nullptr));
  for(int reused=0;reused<2;++reused){
    const auto previous=native->last_coords;auto coords=line(24+reused*8,15+reused*12);
    coords.back().pressure=.8;coords.back().xtilt=.2;coords.back().ytilt=-.3;coords.back().rotation=.4;coords.back().slider=.5;
    GError*error=nullptr;
    g_assert_true(gimp_paint_core_stroke(native,s.drawable,GIMP_PAINT_OPTIONS(s.options),coords.data(),coords.size(),FALSE,&error));g_assert_no_error(error);
    g_assert_true(gimp_paint_core_stroke(gate,s.drawable,GIMP_PAINT_OPTIONS(s.options),coords.data(),coords.size(),FALSE,&error));g_assert_no_error(error);
    /* Native start_coords remembers the previous endpoint for Undo, rather
     * than the current stroke's first point. Check both fresh and reused cores. */
    assert_coords_equal(native->start_coords,previous);assert_coords_equal(gate->start_coords,native->start_coords);
    assert_coords_equal(gate->cur_coords,native->cur_coords);assert_coords_equal(gate->last_coords,native->last_coords);
  }
  g_object_unref(native);g_object_unref(gate);
}

int main(int argc,char**argv)
{
  g_test_init(&argc,&argv,nullptr);gimp_test_utils_set_gimp3_directory("GIMP_TESTING_ABS_TOP_SRCDIR","app/tests/gimpdir");gimp=gimp_init_for_testing();
  g_test_add_func("/painter-batch/one-transaction-segments",one_transaction_logical_segments);
  g_test_add_func("/painter-batch/error-preserves-history",error_preserves_undo_redo);
  g_test_add_func("/painter-batch/no-undo-noop-lock",no_undo_and_noop_lock);
  g_test_add_func("/painter-batch/cancel-cannot-commit",canceled_batch_cannot_commit);
  g_test_add_func("/painter-batch/settings-abort",session_setting_change_aborts);
  g_test_add_func("/painter-batch/raw-stroke",raw_stroke_dispatch);
  g_test_add_func("/painter-batch/raw-path-boundary",raw_path_and_boundary);
  g_test_add_func("/painter-batch/raw-later-path-failure",raw_later_path_failure);
  g_test_add_func("/painter-batch/raw-reentry-dispose",raw_reentry_and_dispose);
  g_test_add_func("/painter-batch/raw-no-undo-empty",raw_no_undo_and_empty_path);
  g_test_add_func("/painter-batch/invalid-before-native-start",invalid_first_sample_no_native_start);
  g_test_add_func("/painter-batch/path-options-last-owner",path_options_last_owner_loss);
  g_test_add_func("/painter-batch/native-start-coordinates",native_start_coordinate_contract);
  const int result=g_test_run();gimp_test_utils_set_gimp3_directory("GIMP_TESTING_ABS_TOP_BUILDDIR","app/tests/gimpdir-output");gimp_exit(gimp,TRUE);return result;
}
