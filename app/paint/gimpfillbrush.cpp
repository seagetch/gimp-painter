/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gio/gio.h>
#include <gegl.h>
#include <cairo.h>
extern "C" {
#include "libgimpbase/gimpbase.h"
#include "libgimpconfig/gimpconfig.h"
#include "libgimpcolor/gimpcolor.h"
#include "libgimpmath/gimpmath.h"
#include "paint-types.h"
#include "core/gimp.h"
#include "core/gimpbrush.h"
#include "core/gimpcontext.h"
#include "core/gimpdrawable.h"
#include "core/gimpdynamics.h"
#include "core/gimpimage.h"
#include "core/gimppickable.h"
#include "core/gimpprojection.h"
#include "core/gimpsymmetry.h"
#include "core/gimptempbuf.h"
#include "gimpfillbrush.h"
#include "operations/layer-modes/gimp-layer-modes.h"
}
#include "painter/binding-store.hpp"
#include "painter/connection.hpp"
#include "painter/gimp-painter-binding.h"
#include "painter-bounded-fill/search.hpp"
#include "gimp-intl.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <deque>
#include <string>
using namespace GimpPainter;
G_DEFINE_TYPE(GimpFillBrushOptions,gimp_fill_brush_options,GIMP_TYPE_PAINT_OPTIONS)
G_DEFINE_TYPE(GimpFillBrush,gimp_fill_brush,GIMP_TYPE_BRUSH_CORE)
namespace GimpPainter {
template<> struct TypeTraits<GimpFillBrushOptions>{static GType type(){return GIMP_TYPE_FILL_BRUSH_OPTIONS;}};
template<> struct TypeTraits<GimpFillBrush>{static GType type(){return GIMP_TYPE_FILL_BRUSH;}};
template<> struct TypeTraits<GimpPaintOptions>{static GType type(){return GIMP_TYPE_PAINT_OPTIONS;}};
template<> struct TypeTraits<GimpDrawable>{static GType type(){return GIMP_TYPE_DRAWABLE;}};
template<> struct TypeTraits<GimpImage>{static GType type(){return GIMP_TYPE_IMAGE;}};
template<> struct TypeTraits<GeglBuffer>{static GType type(){return GEGL_TYPE_BUFFER;}};
template<> struct TypeTraits<GeglColor>{static GType type(){return GEGL_TYPE_COLOR;}};
}
namespace {
struct Settings{double rate=50;bool eraser=false;void close()noexcept{}};
struct OptionsSlot:SlotSpec<GimpFillBrushOptions,Settings>{};
void options_set(GObject*o,guint id,const GValue*v,GParamSpec*p){boundary_void(nullptr,[&]{auto&store=BindingStore::require(o);auto set=[&](Settings&s){if(id==1)s.rate=g_value_get_double(v);else if(id==2)s.eraser=g_value_get_boolean(v);else G_OBJECT_WARN_INVALID_PROPERTY_ID(o,id,p);};if(store.state()==BindingStore::State::constructing)store.initialize<OptionsSlot>(set);else store.with<OptionsSlot>(set);});}
void options_get(GObject*o,guint id,GValue*v,GParamSpec*p){boundary_void(nullptr,[&]{BindingStore::require(o).read<OptionsSlot>([&](const Settings&s){if(id==1)g_value_set_double(v,s.rate);else if(id==2)g_value_set_boolean(v,s.eraser);else G_OBJECT_WARN_INVALID_PROPERTY_ID(o,id,p);});});}
void options_constructed(GObject*o){G_OBJECT_CLASS(gimp_fill_brush_options_parent_class)->constructed(o);boundary_void(nullptr,[&]{BindingStore::require(o).activate();});}
void options_dispose(GObject*o){gimp_painter_binding_close(o,nullptr);G_OBJECT_CLASS(gimp_fill_brush_options_parent_class)->dispose(o);}
struct TempDelete{void operator()(GimpTempBuf*b)const{if(b)gimp_temp_buf_unref(b);}};
using Temp=std::unique_ptr<GimpTempBuf,TempDelete>;
struct Dab{
 ObjectRef<GimpDrawable> drawable;ObjectRef<GimpImage> image;ObjectRef<GeglColor> color;
 GeglRectangle rect;int offset_x,offset_y;double opacity,image_opacity;GimpLayerMode mode;
 std::unique_ptr<Fill::Search> search;
};
struct TargetWatch {
 std::atomic<bool> changed{false},owned_write{false},busy{true};
};
void target_changed(GeglBuffer*,const GeglRectangle*,gpointer data) {
 auto&watch=**static_cast<std::shared_ptr<TargetWatch>*>(data);
 if(!watch.owned_write.load(std::memory_order_acquire))watch.changed.store(true,std::memory_order_release);
}
gboolean pending_paint(GimpImage*,gpointer data){return (*static_cast<std::shared_ptr<TargetWatch>*>(data))->busy.load(std::memory_order_acquire);}
void release_watch(gpointer data,GClosure*){delete static_cast<std::shared_ptr<TargetWatch>*>(data);}
void validate_coords(const GimpCoords*coords) {
 if(!coords)throw std::invalid_argument("Missing Fill coordinates");
 const double values[]={coords->x,coords->y,coords->pressure,coords->xtilt,coords->ytilt,coords->wheel,coords->distance,coords->rotation,coords->slider,coords->velocity,coords->direction,coords->xscale,coords->yscale,coords->angle};
 for(double value:values)if(!std::isfinite(value))throw std::invalid_argument("Nonfinite Fill input coordinate or axis");
 if(std::abs(coords->x)>G_MAXINT/2||std::abs(coords->y)>G_MAXINT/2)throw std::invalid_argument("Fill coordinates exceed safe bounds");
}
struct InterpolationDelete { void operator()(GimpBrushCoreInterpolation*s)const {gimp_brush_core_interpolation_free(s);} };
struct Segment {
 GimpCoords coords;guint32 time;bool first;
 std::unique_ptr<GimpBrushCoreInterpolation,InterpolationDelete> interpolation;
};
struct BrushImpl;
void finish_frame (BrushImpl& impl, bool commit);
struct BrushImpl {
 explicit BrushImpl(GimpFillBrush* value):owner(value){}
 GimpFillBrush* owner;
 std::shared_ptr<const Fill::Snapshot> snapshot;
 std::shared_ptr<Segment> segment;
 std::unique_ptr<std::deque<std::shared_ptr<Dab>>> pending;
 ObjectRef<GimpImage> image;ObjectRef<GimpDrawable> drawable;ObjectRef<GimpPaintOptions> options;
 ObjectRef<GeglBuffer> observed_buffer;
 Connection target_connection,paint_query_connection;
 std::shared_ptr<TargetWatch> target_watch;
 const Babl* original_format=nullptr;
 int original_width=0,original_height=0,original_x=0,original_y=0;
 std::string error;
 bool closed=false,preparing=false,draining=false,interpolating=false,starting=false,ending=false;
 bool started=false,first=true,cancel_requested=false;
 std::uint64_t revision=0;
 void clear_dabs(){++revision;auto old_segment=std::move(segment);auto retired=std::move(pending);auto old_snapshot=std::move(snapshot);error.clear();}
 void release_frame(){clear_dabs();auto old_connection=std::move(target_connection);auto old_query=std::move(paint_query_connection);auto old_watch=std::move(target_watch);if(old_watch)old_watch->busy.store(false,std::memory_order_release);old_query.close();old_connection.close();auto old_buffer=std::move(observed_buffer);auto old_options=std::move(options);auto old_drawable=std::move(drawable);auto old_image=std::move(image);}
 void close()noexcept {
  closed=true;cancel_requested=true;
  if(starting||preparing||draining||ending)return;
  try{finish_frame(*this,false);}catch(...){release_frame();}
 }
};
bool target_current(const BrushImpl&impl,bool require_writable=true) {
 auto*d=impl.drawable.get();
 if(!d||gimp_item_get_image(GIMP_ITEM(d))!=impl.image.get()||(require_writable&&(!gimp_item_is_attached(GIMP_ITEM(d))||gimp_item_is_content_locked(GIMP_ITEM(d),nullptr))))return false;
 int x,y;gimp_item_get_offset(GIMP_ITEM(d),&x,&y);
 return x==impl.original_x&&y==impl.original_y&&gimp_item_get_width(GIMP_ITEM(d))==impl.original_width&&gimp_item_get_height(GIMP_ITEM(d))==impl.original_height&&gimp_drawable_get_format(d)==impl.original_format&&gimp_drawable_get_buffer(d)==impl.observed_buffer.get()&&impl.target_watch&&!impl.target_watch->changed.load(std::memory_order_acquire);
}
void observe_target(BrushImpl&impl) {
 auto*d=impl.drawable.get();impl.original_width=gimp_item_get_width(GIMP_ITEM(d));impl.original_height=gimp_item_get_height(GIMP_ITEM(d));gimp_item_get_offset(GIMP_ITEM(d),&impl.original_x,&impl.original_y);impl.original_format=gimp_drawable_get_format(d);
 impl.observed_buffer=ObjectRef<GeglBuffer>::retain(gimp_drawable_get_buffer(d));impl.target_watch=std::make_shared<TargetWatch>();
 auto payload=std::unique_ptr<std::shared_ptr<TargetWatch>>(new std::shared_ptr<TargetWatch>(impl.target_watch));
 impl.target_connection=Connection::connect(ObjectRef<GObject>::retain(G_OBJECT(impl.observed_buffer.get())),"changed",G_CALLBACK(target_changed),payload.get(),release_watch);payload.release();
 auto query_payload=std::unique_ptr<std::shared_ptr<TargetWatch>>(new std::shared_ptr<TargetWatch>(impl.target_watch));
 impl.paint_query_connection=Connection::connect(ObjectRef<GObject>::retain(G_OBJECT(impl.image.get())),"query-pending-paint",G_CALLBACK(pending_paint),query_payload.get(),release_watch);query_payload.release();
}
struct BrushSlot:SlotSpec<GimpFillBrush,BrushImpl>{};
void constructed(GObject*o){G_OBJECT_CLASS(gimp_fill_brush_parent_class)->constructed(o);boundary_void(nullptr,[&]{BindingStore::require(o).activate();});}
void dispose(GObject*o){gimp_painter_binding_close(o,nullptr);G_OBJECT_CLASS(gimp_fill_brush_parent_class)->dispose(o);}
void queue_dab(GimpFillBrush*self,BrushImpl&impl,GimpDrawable*d,GimpPaintOptions*options,GimpSymmetry*sym){
 if(impl.closed)throw std::runtime_error("Fill owner is closed");
 if(impl.preparing||(impl.draining&&!impl.interpolating))throw std::runtime_error("Fill transaction is transitioning");
 struct Preparing{bool&value;Preparing(bool&v):value(v){value=true;}~Preparing(){value=false;}} preparing(impl.preparing);
 const auto revision=impl.revision;
 if(gimp_symmetry_get_size(sym)!=1)throw std::runtime_error("Fill symmetry requires its own compatibility integration");
 if(gimp_drawable_is_indexed(d))throw std::runtime_error("Legacy Fill Brush does not paint indexed drawables");
 auto target=ObjectRef<GimpDrawable>::retain(d);auto owner=ObjectRef<GimpImage>::retain(gimp_item_get_image(GIMP_ITEM(d)));
 if(!owner||!gimp_item_is_attached(GIMP_ITEM(d)))throw std::runtime_error("Fill drawable is detached");
 if(gimp_image_get_precision(owner.get())!=GIMP_PRECISION_U8_NON_LINEAR)throw std::runtime_error("Fill Brush currently requires nonlinear byte precision");
 auto*core=GIMP_PAINT_CORE(self);auto*brush=GIMP_BRUSH_CORE(self);auto*context=GIMP_CONTEXT(options);
 GimpCoords coords=*gimp_symmetry_get_origin(sym);int ox,oy;gimp_item_get_offset(GIMP_ITEM(d),&ox,&oy);coords.x-=ox;coords.y-=oy;
 if(!std::isfinite(coords.x)||!std::isfinite(coords.y)||std::abs(coords.x)>G_MAXINT/2||std::abs(coords.y)>G_MAXINT/2)throw std::invalid_argument("Fill coordinates exceed safe bounds");
 if(!impl.snapshot){
  auto*pickable=GIMP_PICKABLE(gimp_image_get_projection(owner.get()));gimp_pickable_flush(pickable);
  if(impl.closed||revision!=impl.revision||!gimp_item_is_attached(GIMP_ITEM(d)))throw std::runtime_error("Fill changed during projection flush");
  auto*src=gimp_pickable_get_buffer(pickable);const bool gray=gimp_image_get_base_type(owner.get())==GIMP_GRAY;const Babl*format=babl_format_with_space(gray?"Y'A u8":"R'G'B'A u8",babl_format_get_space(gegl_buffer_get_format(src)));
  auto copy=ObjectRef<GeglBuffer>::adopt(gegl_buffer_new(gegl_buffer_get_extent(src),format));gegl_buffer_copy(src,nullptr,GEGL_ABYSS_NONE,copy.get(),nullptr);
  impl.snapshot=std::make_shared<Fill::Snapshot>(copy.get(),gray?2:4,true,int(coords.x)+ox,int(coords.y)+oy);impl.image=owner;impl.drawable=target;
 }
 if(impl.image.get()!=owner.get()||impl.drawable.get()!=d)throw std::runtime_error("Fill stroke cannot switch drawable before finish");
 // Legacy interpolation takes spacing from the brush resource, not the new
 // independent GIMP3 paint-options spacing default.
 brush->spacing=gimp_brush_get_spacing(brush->main_brush)/100.0;
 const double fade=gimp_paint_options_get_fade(options,owner.get(),core->pixel_dist);
 const double opacity=gimp_dynamics_get_linear_value(brush->dynamics,GIMP_DYNAMICS_OUTPUT_OPACITY,&coords,options,fade);
 if(opacity<=0)return;
 gimp_brush_core_eval_transform_dynamics(brush,owner.get(),options,&coords);
 const auto*base=gimp_brush_get_mask(brush->main_brush);if(!base)return;
 brush->scale=std::max(.5/double(std::min(gimp_temp_buf_get_width(base),gimp_temp_buf_get_height(base))),brush->scale);
 int bw,bh;gimp_brush_transform_size(brush->brush,brush->scale,brush->aspect_ratio,brush->angle,brush->reflect,&bw,&bh);if(bw<=0||bh<=0)return;
 const int x=int(std::floor(coords.x))-bw/2,y=int(std::floor(coords.y))-bh/2;
 const int dw=std::min(gimp_item_get_width(GIMP_ITEM(d)),gimp_image_get_width(owner.get())-ox),dh=std::min(gimp_item_get_height(GIMP_ITEM(d)),gimp_image_get_height(owner.get())-oy),minx=std::max(-ox,0),miny=std::max(-oy,0);
 if(dw<=minx||dh<=miny)return;
 const int x1=CLAMP(x,minx,dw),y1=CLAMP(y,miny,dh),x2=CLAMP(std::int64_t(x)+bw,minx,dw),y2=CLAMP(std::int64_t(y)+bh,miny,dh);if(x1>=x2||y1>=y2)return;
 // The old Fill Brush used hardness output as force, including this distinction.
 double force=gimp_dynamics_get_linear_value(brush->dynamics,GIMP_DYNAMICS_OUTPUT_HARDNESS,&coords,options,fade);
 const auto*mask=gimp_brush_core_get_brush_mask(brush,&coords,gimp_paint_options_get_brush_mode(options),force);if(!mask)return;
 auto maskbuf=ObjectRef<GeglBuffer>::adopt(gimp_temp_buf_create_buffer(mask));GeglRectangle search_rect={x1+ox,y1+oy,x2-x1,y2-y1};auto search_mask=ObjectRef<GeglBuffer>::adopt(gegl_buffer_new(&search_rect,babl_format("Y u8")));
 GeglRectangle mask_rect={x1==minx?bw-(x2-minx):0,y1==miny?bh-(y2-miny):0,x2-x1,y2-y1};gegl_buffer_copy(maskbuf.get(),&mask_rect,GEGL_ABYSS_NONE,search_mask.get(),&search_rect);
 const auto settings=BindingStore::require(G_OBJECT(options)).read<OptionsSlot>([](const Settings&s){return s;});
 auto dab=std::make_shared<Dab>();dab->drawable=target;dab->image=owner;dab->rect={x1,y1,x2-x1,y2-y1};dab->offset_x=ox;dab->offset_y=oy;dab->opacity=std::min(opacity,1.0);dab->image_opacity=gimp_context_get_opacity(context);dab->mode=settings.eraser?GIMP_LAYER_MODE_PAINTER_ERASE:gimp_context_get_paint_mode(context);
 dab->color=ObjectRef<GeglColor>::adopt(gegl_color_duplicate(settings.eraser?gimp_context_get_background(context):gimp_context_get_foreground(context)));gimp_color_set_alpha(dab->color.get(),1);
 dab->search.reset(new Fill::Search(impl.snapshot,search_mask.get(),search_rect,int(coords.x)+ox,int(coords.y)+oy,Fill::Search::Options{}));if(!impl.pending)impl.pending.reset(new std::deque<std::shared_ptr<Dab>>);impl.pending->push_back(std::move(dab));
}
void paint(GimpPaintCore*core,GList*drawables,GimpPaintOptions*options,GimpSymmetry*sym,GimpPaintState state,guint32){
 boundary_void(nullptr,[&]{BindingStore::require(G_OBJECT(core)).with<BrushSlot>([&](BrushImpl&impl){
  if(state==GIMP_PAINT_STATE_INIT){impl.clear_dabs();return;}
  if(state==GIMP_PAINT_STATE_FINISH){impl.clear_dabs();return;}
  if(!impl.error.empty())return;
  try{if(!drawables||drawables->next)throw std::runtime_error("Fill Brush requires one drawable per native transaction");queue_dab(GIMP_FILL_BRUSH(core),impl,GIMP_DRAWABLE(drawables->data),options,sym);}
  catch(const std::exception&e){impl.clear_dabs();impl.error=e.what();}
 });});
}
void release_native_scratch (GimpPaintCore* core) {
 g_clear_object(&core->mask_buffer);
 if(core->applicators){g_hash_table_unref(core->applicators);core->applicators=nullptr;}
 if(core->stroke_buffer){g_array_free(core->stroke_buffer,TRUE);core->stroke_buffer=nullptr;}
 core->image_pickable=nullptr;gimp_paint_core_cleanup(core);
}
void finish_frame (BrushImpl& impl, bool commit) {
 if(impl.starting){impl.cancel_requested=true;return;}
 if(impl.ending)return;
 if(!impl.started){impl.release_frame();return;}
 struct Ending{bool&v;Ending(bool&value):v(value){v=true;}~Ending(){v=false;}} ending(impl.ending);
 impl.started=false;
 auto image=impl.image;auto drawable=impl.drawable;auto options=impl.options;
 auto*core=GIMP_PAINT_CORE(impl.owner);GList list={drawable.get(),nullptr,nullptr};
 const bool unchanged=target_current(impl,false);
 impl.target_connection.close();
 impl.clear_dabs();
 if(!impl.closed)gimp_paint_core_paint(core,&list,options.get(),GIMP_PAINT_STATE_FINISH,0);
 if(!unchanged)
  gimp_paint_core_finish(core,&list,FALSE); // preserve independently changed data, never stale rollback
 else if(commit&&!impl.cancel_requested&&gimp_item_is_attached(GIMP_ITEM(drawable.get())))
  gimp_paint_core_finish(core,&list,TRUE);
 else if(core->x1==core->x2||core->y1==core->y2)gimp_paint_core_finish(core,&list,FALSE);
 else gimp_paint_core_cancel(core,&list);
 release_native_scratch(core);impl.release_frame();
}

}
static void gimp_fill_brush_options_class_init(GimpFillBrushOptionsClass*k){auto*o=G_OBJECT_CLASS(k);o->constructed=options_constructed;o->dispose=options_dispose;o->get_property=options_get;o->set_property=options_set;
 GIMP_CONFIG_PROP_DOUBLE(o,1,"rate","Rate","Legacy fill rate (preserved; old output is rate-independent)",0,100,50,GIMP_PARAM_STATIC_STRINGS);
 GIMP_CONFIG_PROP_BOOLEAN(o,2,"eraser-mode","Eraser mode","Erase instead of painting foreground",FALSE,GIMP_PARAM_STATIC_STRINGS);}
static void gimp_fill_brush_options_init(GimpFillBrushOptions*o){o->binding_failed=!boundary<bool>(nullptr,false,[&]{BindingStore::ensure(G_OBJECT(o)).emplace<OptionsSlot>();return true;});}
static void gimp_fill_brush_class_init(GimpFillBrushClass*k){G_OBJECT_CLASS(k)->constructed=constructed;G_OBJECT_CLASS(k)->dispose=dispose;GIMP_PAINT_CORE_CLASS(k)->paint=paint;auto*b=GIMP_BRUSH_CORE_CLASS(k);b->handles_changing_brush=TRUE;b->handles_transforming_brush=TRUE;b->handles_dynamic_transforming_brush=TRUE;}
static void gimp_fill_brush_init(GimpFillBrush*b){b->binding_failed=!boundary<bool>(nullptr,false,[&]{BindingStore::ensure(G_OBJECT(b)).emplace<BrushSlot>(b);return true;});}
gboolean gimp_fill_brush_step(GimpFillBrush*self,gsize budget,GError**error){return boundary<gboolean>(error,FALSE,[&]()->gboolean{
 return BindingStore::require(G_OBJECT(self)).with<BrushSlot>([&](BrushImpl&impl)->gboolean{
  if(!impl.error.empty())throw std::runtime_error(impl.error);
  if(impl.draining||impl.preparing)throw std::runtime_error("Fill transaction is transitioning");
  struct Draining{bool&value;Draining(bool&v):value(v){value=true;}~Draining(){value=false;}} draining(impl.draining);
  try {
  if(impl.started&&!target_current(impl))throw std::runtime_error("Fill target changed; current pixels and geometry preserved");
  const auto revision=impl.revision;
  if(!budget)return (!impl.segment&&(!impl.pending||impl.pending->empty()));
  if((!impl.pending||impl.pending->empty())){
   if(!impl.segment)return TRUE;
   // A segment owns only numerical continuation state. Keep a local lease
   // because paint preparation may close/invalidate the owner reentrantly.
   auto segment=impl.segment;auto drawable=impl.drawable;auto options=impl.options;
   auto*core=GIMP_PAINT_CORE(self);GList list={drawable.get(),nullptr,nullptr};
   struct Interpolating{bool&v;Interpolating(bool&b):v(b){v=true;}~Interpolating(){v=false;}} interpolating(impl.interpolating);
   bool done=false;
   if(segment->first){
    gimp_paint_core_set_current_coords(core,&segment->coords);
    gimp_paint_core_paint(core,&list,options.get(),GIMP_PAINT_STATE_MOTION,segment->time);
    gimp_paint_core_set_last_coords(core,&segment->coords);done=true;
   }else{
    if(!segment->interpolation){
     segment->interpolation.reset(gimp_brush_core_interpolation_begin(GIMP_BRUSH_CORE(self),&list,options.get(),&segment->coords,segment->time));
     if(!segment->interpolation)throw std::runtime_error("Fill interpolation count is not representable");
    }
    done=gimp_brush_core_interpolation_step(GIMP_BRUSH_CORE(self),&list,options.get(),segment->interpolation.get(),1);
   }
   if(impl.cancel_requested||impl.closed){finish_frame(impl,false);return TRUE;}
   if(!impl.error.empty())throw std::runtime_error(impl.error);
   if(revision!=impl.revision)throw std::runtime_error("Fill invalidated during interpolation");
   if(done)impl.segment.reset();
   return (!impl.segment&&(!impl.pending||impl.pending->empty()));
  }
  auto dab=impl.pending->front();
  if(dab->search->step(budget)!=Fill::Search::State::Complete)return FALSE;
  impl.pending->pop_front();auto*core=GIMP_PAINT_CORE(self);auto*d=dab->drawable.get();
  if(!gimp_item_is_attached(GIMP_ITEM(d))||gimp_item_get_image(GIMP_ITEM(d))!=dab->image.get())throw std::runtime_error("Fill target changed while queued");
  int ox,oy;gimp_item_get_offset(GIMP_ITEM(d),&ox,&oy);if(ox!=dab->offset_x||oy!=dab->offset_y)throw std::runtime_error("Fill target moved while queued");
  GeglRectangle origin={0,0,dab->rect.width,dab->rect.height};const Babl*format=gimp_layer_mode_get_format(dab->mode,GIMP_LAYER_COLOR_SPACE_AUTO,GIMP_LAYER_COLOR_SPACE_AUTO,gimp_layer_mode_get_paint_composite_mode(dab->mode),gimp_drawable_get_format(d));Temp paint_temp(gimp_temp_buf_new(origin.width,origin.height,format));auto pixels=ObjectRef<GeglBuffer>::adopt(gimp_temp_buf_create_buffer(paint_temp.get()));gegl_buffer_set_color(pixels.get(),&origin,dab->color.get());
  Temp mask(gimp_temp_buf_new(origin.width,origin.height,babl_format("Y u8")));GeglRectangle source={dab->rect.x+ox,dab->rect.y+oy,origin.width,origin.height};gegl_buffer_get(dab->search->result(),&source,1,babl_format("Y u8"),gimp_temp_buf_get_data(mask.get()),GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
  g_set_object(&core->paint_buffer,pixels.get());core->paint_buffer_x=dab->rect.x;core->paint_buffer_y=dab->rect.y;
  auto watch=impl.target_watch;watch->owned_write.store(true,std::memory_order_release);
  gimp_paint_core_paste(core,mask.get(),0,0,d,dab->opacity,dab->image_opacity,dab->mode,GIMP_PAINT_CONSTANT);
  watch->owned_write.store(false,std::memory_order_release);
  if(impl.cancel_requested||impl.closed){finish_frame(impl,false);return TRUE;}
  if(revision!=impl.revision)throw std::runtime_error("Fill invalidated during publication");
  return (!impl.segment&&(!impl.pending||impl.pending->empty()));
  } catch (...) {finish_frame(impl,false);throw;}
 });});}
void gimp_fill_brush_cancel_pending(GimpFillBrush*self){boundary_void(nullptr,[&]{BindingStore::require(G_OBJECT(self)).with<BrushSlot>([](BrushImpl&impl){impl.cancel_requested=true;if(!impl.starting&&!impl.preparing&&!impl.draining&&!impl.ending)finish_frame(impl,false);});});}

gboolean gimp_fill_brush_begin(GimpFillBrush*self,GimpDrawable*d,GimpPaintOptions*options,const GimpCoords*coords,GError**error) {
 return boundary<gboolean>(error,FALSE,[&]()->gboolean{validate_coords(coords);return BindingStore::require(G_OBJECT(self)).with<BrushSlot>([&](BrushImpl&impl)->gboolean{
  if(!options||!G_TYPE_CHECK_INSTANCE_TYPE(options,GIMP_TYPE_FILL_BRUSH_OPTIONS))throw std::invalid_argument("Invalid Fill Brush start options");
  if(impl.closed||impl.started||impl.starting||impl.ending||impl.preparing||impl.draining)throw std::runtime_error("Fill transaction is already active or transitioning");
  auto target=ObjectRef<GimpDrawable>::retain(d);if(!target||!gimp_item_is_attached(GIMP_ITEM(d)))throw std::invalid_argument("Detached Fill drawable");
  auto image=ObjectRef<GimpImage>::retain(gimp_item_get_image(GIMP_ITEM(d)));
  if(gimp_item_is_content_locked(GIMP_ITEM(d),nullptr))throw std::invalid_argument("Fill drawable is locked");
  impl.release_frame();impl.drawable=target;impl.image=image;impl.options=ObjectRef<GimpPaintOptions>::retain(options);impl.starting=true;impl.cancel_requested=false;impl.first=true;
  try{observe_target(impl);}catch(...){impl.starting=false;impl.release_frame();throw;}
  auto*core=GIMP_PAINT_CORE(self);GList list={d,nullptr,nullptr};GError*native_error=nullptr;
  bool ok=gimp_paint_core_start(core,&list,options,coords,&native_error);impl.starting=false;
  if(!ok){std::string message=native_error?native_error->message:"Fill native start failed";g_clear_error(&native_error);release_native_scratch(core);impl.release_frame();throw std::runtime_error(message);}
  impl.started=true;
  if(impl.closed||impl.cancel_requested||!target_current(impl)){finish_frame(impl,false);throw std::runtime_error("Fill start was cancelled");}
  gimp_paint_core_paint(core,&list,options,GIMP_PAINT_STATE_INIT,0);
  if(impl.closed||impl.cancel_requested){finish_frame(impl,false);throw std::runtime_error("Fill initialization was cancelled");}
  return TRUE;
 });});
}
gboolean gimp_fill_brush_motion(GimpFillBrush*self,const GimpCoords*coords,guint32 time,GError**error) {
 return boundary<gboolean>(error,FALSE,[&]()->gboolean{validate_coords(coords);return BindingStore::require(G_OBJECT(self)).with<BrushSlot>([&](BrushImpl&impl)->gboolean{
  if(!impl.started||impl.closed||impl.starting||impl.ending||impl.preparing||impl.draining||impl.segment)throw std::runtime_error("Fill motion outside active transaction or while a segment is pending");
  if(!target_current(impl)){finish_frame(impl,false);throw std::runtime_error("Fill target changed; current pixels and geometry preserved");}
  auto image=impl.image;auto drawable=impl.drawable;auto options=impl.options;auto*core=GIMP_PAINT_CORE(self);GList list={drawable.get(),nullptr,nullptr};
  if(impl.first){impl.first=false;gimp_paint_core_set_current_coords(core,coords);gimp_paint_core_paint(core,&list,options.get(),GIMP_PAINT_STATE_MOTION,time);gimp_paint_core_set_last_coords(core,coords);}
  else gimp_paint_core_interpolate(core,&list,options.get(),coords,time);
  if(impl.closed||impl.cancel_requested){finish_frame(impl,false);throw std::runtime_error("Fill motion cancelled");}
  if(!impl.error.empty()){auto message=impl.error;finish_frame(impl,false);throw std::runtime_error(message);}
  return TRUE;
 });});
}
gboolean gimp_fill_brush_motion_begin(GimpFillBrush*self,const GimpCoords*coords,guint32 time,GError**error) {
 return boundary<gboolean>(error,FALSE,[&]()->gboolean{validate_coords(coords);return BindingStore::require(G_OBJECT(self)).with<BrushSlot>([&](BrushImpl&impl)->gboolean{
  if(!impl.started||impl.closed||impl.starting||impl.ending||impl.preparing||impl.draining||impl.segment||(impl.pending&&!impl.pending->empty()))throw std::runtime_error("Fill asynchronous motion requires a drained active transaction");
  if(!target_current(impl)){finish_frame(impl,false);throw std::runtime_error("Fill target changed; current pixels and geometry preserved");}
  auto segment=std::make_shared<Segment>();segment->coords=*coords;segment->time=time;segment->first=impl.first;
  impl.segment=std::move(segment);impl.first=false;return TRUE;
 });});
}
gboolean gimp_fill_brush_finish(GimpFillBrush*self,gboolean commit,GError**error) {
 return boundary<gboolean>(error,FALSE,[&]()->gboolean{return BindingStore::require(G_OBJECT(self)).with<BrushSlot>([&](BrushImpl&impl)->gboolean{
  if(!commit)impl.cancel_requested=true;
  if(impl.starting||impl.ending||impl.preparing||impl.draining)return FALSE;
  if(impl.started&&!target_current(impl)){finish_frame(impl,false);throw std::runtime_error("Fill target changed; current pixels and geometry preserved");}
  if(commit&&(impl.segment||(impl.pending&&!impl.pending->empty())))return FALSE;
  finish_frame(impl,commit);return TRUE;
 });});
}

void gimp_fill_brush_register (Gimp *gimp, GimpPaintRegisterCallback callback)
{
 callback(gimp,GIMP_TYPE_FILL_BRUSH,GIMP_TYPE_FILL_BRUSH_OPTIONS,
          "gimp-bucket-fill-brush",_("Fill Brush"),"gimp-tool-bucket-fill");
}
