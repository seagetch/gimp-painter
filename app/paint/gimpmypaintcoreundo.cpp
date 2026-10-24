/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gio/gio.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include "gimpmypaintcoreundo.hpp"
extern "C" {
#include "gimpmypaintcoreundo.h"
#include "core/gimpdrawable.h"
#include "core/gimpimage.h"
#include "core/gimpundopopguard.h"
}
#include "painter/binding-store.hpp"
#include "painter/gimp-painter-binding.h"
#include <limits>
#include <cstdint>
using namespace GimpPainter;
using namespace GimpPainter::MyPaint;
struct _GimpMypaintCoreUndo { GimpDrawableUndo parent; gboolean failed, preparing, guarded, freed, pending_free; GimpUndoMode free_mode; };
struct _GimpMypaintCoreUndoClass { GimpDrawableUndoClass parent; };
typedef struct { GObject parent; gboolean failed; } GimpPainterStrokePayload;
typedef struct { GObjectClass parent; } GimpPainterStrokePayloadClass;
static GType gimp_painter_stroke_payload_get_type(void);
static void initable_iface(GInitableIface*);
static void guard_iface(GimpUndoPopGuardInterface*);
G_DEFINE_TYPE(GimpPainterStrokePayload,gimp_painter_stroke_payload,G_TYPE_OBJECT)
G_DEFINE_TYPE_WITH_CODE(GimpMypaintCoreUndo,gimp_mypaint_core_undo,GIMP_TYPE_DRAWABLE_UNDO,
 G_IMPLEMENT_INTERFACE(G_TYPE_INITABLE,initable_iface)
 G_IMPLEMENT_INTERFACE(GIMP_TYPE_UNDO_POP_GUARD,guard_iface))
namespace GimpPainter {
/* Public typed ownership uses the single TypeTraits specialization in the header. */
template<> struct TypeTraits<GimpPainterStrokePayload>{static GType type()noexcept{return gimp_painter_stroke_payload_get_type();}};
}
namespace {
struct Weak { GWeakRef ref; explicit Weak(gpointer p){g_weak_ref_init(&ref,p);} ~Weak(){g_weak_ref_clear(&ref);} ObjectRef<GObject> get(){return ObjectRef<GObject>::adopt(static_cast<GObject*>(g_weak_ref_get(&ref)));} };
struct ChangedPause {
 GeglBuffer*buffer;
 explicit ChangedPause(GeglBuffer*b)noexcept:buffer(b){gegl_buffer_freeze_changed(buffer);}
 ~ChangedPause(){gegl_buffer_thaw_changed(buffer);}
 ChangedPause(const ChangedPause&)=delete;
};
struct Payload {std::shared_ptr<StrokeState> state;void close()noexcept{state.reset();}};
struct PayloadSlot:SlotSpec<GimpPainterStrokePayload,Payload>{};
struct UndoState {
 std::shared_ptr<StrokeState> state;
 mutable std::shared_ptr<StrokeState> prepared_lease;
 void close()noexcept{state.reset();} // Acquired guard lease belongs to planner.
};
struct UndoSlot:SlotSpec<GimpMypaintCoreUndo,UndoState>{};
void payload_dispose(GObject*o){gimp_painter_binding_close(o,nullptr);G_OBJECT_CLASS(gimp_painter_stroke_payload_parent_class)->dispose(o);}
void native_free(GimpMypaintCoreUndo*u,GimpUndoMode mode){
 if(u->freed)return;
 if(u->preparing||u->guarded){u->pending_free=TRUE;u->free_mode=mode;return;}
 u->freed=TRUE;u->pending_free=FALSE;
 GIMP_UNDO_CLASS(gimp_mypaint_core_undo_parent_class)->free(GIMP_UNDO(u),mode);
}
void dispose(GObject*o){auto*u=GIMP_MYPAINT_CORE_UNDO(o);gimp_painter_binding_close(o,nullptr);native_free(u,GIMP_UNDO_MODE_UNDO);G_OBJECT_CLASS(gimp_mypaint_core_undo_parent_class)->dispose(o);}
void free_undo(GimpUndo*undo,GimpUndoMode mode){gimp_painter_binding_close(G_OBJECT(undo),nullptr);native_free(GIMP_MYPAINT_CORE_UNDO(undo),mode);}
void set_property(GObject*o,guint id,const GValue*v,GParamSpec*pspec){
 if(id!=1){G_OBJECT_WARN_INVALID_PROPERTY_ID(o,id,pspec);return;}
 bool ok=property_boundary(o,pspec,"set",[&]{
  auto*holder=g_value_get_object(v);if(!holder)throw Error(GIMP_PAINTER_ERROR_INVALID_STATE,"Missing typed stroke payload");
  auto state=BindingStore::require(G_OBJECT(holder)).read<PayloadSlot>([](const Payload&p){return p.state;});
  if(!state)throw Error(GIMP_PAINTER_ERROR_INVALID_STATE,"Closed typed stroke payload");
  BindingStore::require(o).initialize<UndoSlot>([&](UndoState&p){p.state=std::move(state);});
 });if(!ok)GIMP_MYPAINT_CORE_UNDO(o)->failed=TRUE;
}
void constructed(GObject*o){G_OBJECT_CLASS(gimp_mypaint_core_undo_parent_class)->constructed(o);auto*u=GIMP_MYPAINT_CORE_UNDO(o);if(!u->failed)u->failed=!boundary<bool>(nullptr,false,[&]{BindingStore::require(o).activate();return true;});}
gboolean initialize(GInitable*i,GCancellable*c,GError**e){if(g_cancellable_set_error_if_cancelled(c,e))return FALSE;return boundary<gboolean>(e,FALSE,[&]()->gboolean{
 auto*u=GIMP_MYPAINT_CORE_UNDO(i);if(u->failed||u->freed)throw Error(GIMP_PAINTER_ERROR_INVALID_STATE,"Stroke Undo construction failed");
 auto state=BindingStore::require(G_OBJECT(u)).with<UndoSlot>([](UndoState&p){return p.state;});
 if(!state||!GIMP_IS_DRAWABLE(GIMP_ITEM_UNDO(u)->item)||!GIMP_DRAWABLE_UNDO(u)->buffer)throw Error(GIMP_PAINTER_ERROR_INVALID_STATE,"Incomplete stroke Undo payload");
 return TRUE;});}
gboolean prepare(GimpUndo*undo,GimpUndoMode mode){auto*u=GIMP_MYPAINT_CORE_UNDO(undo);if(u->preparing||u->guarded||u->freed)return FALSE;
 return boundary<gboolean>(nullptr,FALSE,[&]()->gboolean{
  auto&store=BindingStore::require(G_OBJECT(u));if(store.state()!=BindingStore::State::active)return FALSE;
  auto generation=store.generation();auto state=store.with<UndoSlot>([](UndoState&p){return p.state;});
  if(!state||!store.accepts(generation))return FALSE;
  u->preparing=TRUE;bool acquired=state->prepare(mode);u->preparing=FALSE;
  if(!acquired||!store.accepts(generation)||u->pending_free){if(acquired)state->finish(mode);if(u->pending_free)native_free(u,u->free_mode);return FALSE;}
  // No allocating operation or user callback between acquired lease publication
  // and the guard flag. Planner holds the native entry until finish.
  u->preparing=TRUE;
  store.read<UndoSlot>([&](const UndoState&p){p.prepared_lease=state;});
  u->preparing=FALSE;
  if(!store.accepts(generation)||u->pending_free){store.read<UndoSlot>([](const UndoState&p){p.prepared_lease.reset();});state->finish(mode);if(u->pending_free)native_free(u,u->free_mode);return FALSE;}
  u->guarded=TRUE;return TRUE;
 });
}
void finish(GimpUndo*undo,GimpUndoMode mode){auto*u=GIMP_MYPAINT_CORE_UNDO(undo);if(!u->guarded)return;
 boundary_void(nullptr,[&]{auto state=BindingStore::require(G_OBJECT(u)).read<UndoSlot>([](const UndoState&p){return std::move(p.prepared_lease);});u->guarded=FALSE;if(state)state->finish(mode);if(u->pending_free)native_free(u,u->free_mode);});
}
void pop(GimpUndo*undo,GimpUndoMode mode,GimpUndoAccumulator*accum){auto*u=GIMP_MYPAINT_CORE_UNDO(undo);if(!u->guarded||u->freed)return;
 boundary_void(nullptr,[&]{auto state=BindingStore::require(G_OBJECT(u)).read<UndoSlot>([](const UndoState&p){return p.prepared_lease;});if(!state)return;
 auto item=ObjectRef<GObject>::retain(G_OBJECT(GIMP_ITEM_UNDO(u)->item));auto buffer=ObjectRef<GObject>::retain(G_OBJECT(GIMP_DRAWABLE_UNDO(u)->buffer));
 auto live=ObjectRef<GObject>::retain(G_OBJECT(gimp_drawable_get_buffer(GIMP_DRAWABLE(item.get()))));
 // Only the private payload needs batching: live changed must invalidate the
 // graph before native drawable update observers query the projection.
 ChangedPause payload_notifications(GEGL_BUFFER(buffer.get()));
 state->apply(mode);
 GIMP_UNDO_CLASS(gimp_mypaint_core_undo_parent_class)->pop(undo,mode,accum);
 });
}
gint64 memsize(GimpObject*o,gint64*gui){auto parent=GIMP_OBJECT_CLASS(gimp_mypaint_core_undo_parent_class)->get_memsize(o,gui);return boundary<gint64>(nullptr,parent,[&]{auto bytes=BindingStore::require(G_OBJECT(o)).read<UndoSlot>([](const UndoState&p){auto s=p.state?p.state:p.prepared_lease;return s?s->memsize():0;});auto cap=std::numeric_limits<gint64>::max();return bytes>static_cast<std::size_t>(cap-parent)?cap:parent+static_cast<gint64>(bytes);});}
}
static void gimp_painter_stroke_payload_class_init(GimpPainterStrokePayloadClass*k){G_OBJECT_CLASS(k)->dispose=payload_dispose;}
static void gimp_painter_stroke_payload_init(GimpPainterStrokePayload*p){p->failed=!boundary<bool>(nullptr,false,[&]{auto&s=BindingStore::ensure(G_OBJECT(p));s.emplace<PayloadSlot>();s.activate();return true;});}
static void gimp_mypaint_core_undo_class_init(GimpMypaintCoreUndoClass*k){auto*o=G_OBJECT_CLASS(k);o->set_property=set_property;o->constructed=constructed;o->dispose=dispose;GIMP_UNDO_CLASS(k)->free=free_undo;GIMP_UNDO_CLASS(k)->pop=pop;GIMP_OBJECT_CLASS(k)->get_memsize=memsize;g_object_class_install_property(o,1,g_param_spec_object("stroke",nullptr,nullptr,gimp_painter_stroke_payload_get_type(),GParamFlags(G_PARAM_WRITABLE|G_PARAM_CONSTRUCT_ONLY|G_PARAM_STATIC_STRINGS)));}
static void gimp_mypaint_core_undo_init(GimpMypaintCoreUndo*u){u->failed=!boundary<bool>(nullptr,false,[&]{BindingStore::ensure(G_OBJECT(u)).emplace<UndoSlot>();return true;});}
static void initable_iface(GInitableIface*i){i->init=initialize;}
static void guard_iface(GimpUndoPopGuardInterface*i){i->prepare=prepare;i->finish=finish;}
GimpUndo* GimpPainter::MyPaint::create_stroke_undo(GimpImage*image,GimpDrawable*drawable,GeglBuffer*buffer,int x,int y,const char*description,std::shared_ptr<StrokeState>state,GError**error)noexcept{
 return boundary<GimpUndo*>(error,nullptr,[&]()->GimpUndo*{
  if(!GIMP_IS_IMAGE(image)||!GIMP_IS_DRAWABLE(drawable)||!GEGL_IS_BUFFER(buffer)||!state||!gimp_item_is_attached(GIMP_ITEM(drawable))||gimp_item_get_image(GIMP_ITEM(drawable))!=image)throw Error(GIMP_PAINTER_ERROR_INVALID_STATE,"Invalid native stroke payload");
  Weak iw(image),dw(drawable),bw(buffer);
  auto il=iw.get(),dl=dw.get(),bl=bw.get();
  if(!il||!dl||!bl||!gimp_item_is_attached(GIMP_ITEM(dl.get()))||gimp_item_get_image(GIMP_ITEM(dl.get()))!=image)throw Error(GIMP_PAINTER_ERROR_INVALID_STATE,"Stroke endpoint changed during acquisition");
  const auto rectangle=*gegl_buffer_get_extent(buffer);const auto*r=&rectangle;const auto*format=gegl_buffer_get_format(buffer);
  if(gegl_buffer_share_storage(buffer,gimp_drawable_get_buffer(drawable))||r->x!=0||r->y!=0||r->width<=0||r->height<=0||gegl_buffer_get_format(buffer)!=gimp_drawable_get_format(drawable)||std::int64_t(x)+r->width>G_MAXINT||std::int64_t(y)+r->height>G_MAXINT)throw Error(GIMP_PAINTER_ERROR_INVALID_STATE,"Invalid stroke pixel geometry or format");
  auto holder=ObjectRef<GObject>::adopt(G_OBJECT(g_object_new(gimp_painter_stroke_payload_get_type(),nullptr)));
  BindingStore::require(holder.get()).with<PayloadSlot>([&](Payload&p){p.state=std::move(state);});
  auto result=ObjectRef<GObject>::adopt(G_OBJECT(g_initable_new(GIMP_TYPE_MYPAINT_CORE_UNDO,nullptr,error,"image",image,"item",drawable,"buffer",buffer,"x",x,"y",y,"name",description?description:"Paint","undo-type",GIMP_UNDO_MYPAINT,"dirty-mask",GIMP_DIRTY_ITEM|GIMP_DIRTY_DRAWABLE,"stroke",holder.get(),nullptr)));
  if(!result)return nullptr;
  const auto*now=gegl_buffer_get_extent(buffer);
  if(gegl_buffer_share_storage(buffer,gimp_drawable_get_buffer(drawable))||!gimp_item_is_attached(GIMP_ITEM(drawable))||gimp_item_get_image(GIMP_ITEM(drawable))!=image||gimp_drawable_get_format(drawable)!=format||gegl_buffer_get_format(buffer)!=format||now->x!=r->x||now->y!=r->y||now->width!=r->width||now->height!=r->height)
    throw Error(GIMP_PAINTER_ERROR_INVALID_STATE,"Stroke endpoint changed during construction");
  return GIMP_UNDO(result.release());
 });
}
