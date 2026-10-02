/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gtk/gtk.h>
#include <gegl.h>
extern "C" {
#include "paint-types.h"
#include "core/gimperror.h"
#include "core/gimpdrawable.h"
#include "core/gimpimage.h"
#include "gimppainterpaintgate.h"
#include "gimp-intl.h"
}
#include "gimppainterpaintgate.hpp"
#include "painter-mypaint-surface/gimp-painter-options.hpp"
#include "painter-mypaint-surface/gimp-painter-session.hpp"
#include "painter/binding-store.hpp"
#include "painter/connection.hpp"
#include "painter/gimp-painter-binding.h"
#include <cmath>
using namespace GimpPainter;
namespace GimpPainter {
template<> struct TypeTraits<GimpImage>{static GType type() noexcept{return GIMP_TYPE_IMAGE;}};
template<> struct TypeTraits<GimpDrawable>{static GType type() noexcept{return GIMP_TYPE_DRAWABLE;}};
}
G_DEFINE_TYPE(GimpPainterPaintGate,gimp_painter_paint_gate,GIMP_TYPE_PAINT_CORE)
namespace {
template<class F>void checked(F call)
{
  GError*error=nullptr;const gboolean result=call(&error);
  if(!result){std::string why=error?error->message:"Extended paint operation failed";g_clear_error(&error);throw std::runtime_error(why);}
  g_clear_error(&error);
}
struct Operation {
  ObjectRef<GimpDrawable> drawable;
  ObjectRef<GimpImage> image;
  ObjectRef<GimpPainterSession> session;
  Connection pending;
  bool active=false,canceled=false,batch=false;
  void cancel() noexcept{canceled=true;if(session)gimp_painter_session_cancel(session.get(),nullptr);}
  ~Operation() noexcept{
    if(batch&&session)gimp_painter_session_end_batch(session.get(),FALSE,nullptr);
    active=false;pending.close();
  }
};
struct GateImpl {
  std::shared_ptr<Operation>operation;
  void close() noexcept{auto old=std::move(operation);if(old)old->cancel();}
};
struct GateSlot:SlotSpec<GimpPainterPaintGate,GateImpl>{};
BindingStore&store(GimpPaintCore*core)
{if(!GIMP_IS_PAINTER_PAINT_GATE(core))throw Error(GIMP_PAINTER_ERROR_WRONG_TYPE,"Expected Painter paint adapter");return BindingStore::require(G_OBJECT(core));}
using WeakOperation=std::weak_ptr<Operation>;
void weak_destroy(gpointer data,GClosure*)noexcept{delete static_cast<WeakOperation*>(data);}
gboolean query_pending(GimpImage*,gpointer data)noexcept
{auto current=static_cast<WeakOperation*>(data)->lock();return current&&current->active;}
struct Admission {
  PainterPaintGateRef owner;
  BindingStore&binding;
  std::uint64_t generation;
  std::shared_ptr<Operation>operation;
  GimpCoords saved_start,saved_current,saved_last;
  GimpVector2 saved_paint;
  bool committed=false;
  explicit Admission(GimpPaintCore*core):owner(PainterPaintGateRef::retain(core)),binding(store(core)),generation(binding.generation()),operation(std::make_shared<Operation>())
  {binding.with<GateSlot>([&](GateImpl&i){if(i.operation)throw std::logic_error("Painter adapter is already processing an operation");i.operation=operation;});saved_start=core->start_coords;saved_current=core->cur_coords;saved_last=core->last_coords;saved_paint=core->last_paint;}
  ~Admission()noexcept{
    // Keep the image pending until rollback has actually finished, even if a
    // callback explicitly disposed the adapter during an evaluator sample.
    if(operation->batch&&operation->session){gimp_painter_session_end_batch(operation->session.get(),FALSE,nullptr);operation->batch=false;}
    if(!committed){auto*core=GIMP_PAINT_CORE(owner.get());core->start_coords=saved_start;core->cur_coords=saved_current;core->last_coords=saved_last;core->last_paint=saved_paint;}
    operation->active=false;operation->pending.close();
    boundary_void(nullptr,[&]{if(binding.accepts(generation))binding.with<GateSlot>([&](GateImpl&i){if(i.operation==operation)i.operation.reset();});});
  }
  void validate()const{if(operation->canceled||!binding.accepts(generation))throw Error(GIMP_PAINTER_ERROR_CLOSED,"Painter operation was closed or canceled");}
};
gboolean refuse(GimpPaintCore*,GList*,GimpPaintOptions*,const GimpCoords*,GError**error)
{
  g_set_error_literal(error,GIMP_ERROR,GIMP_FAILED,_("Extended Painter MyPaint requires its atomic Stroke Path/PDB entrypoints; direct native start is not supported."));return FALSE;
}
void dispose(GObject*object)
{gimp_painter_binding_close(object,nullptr);G_OBJECT_CLASS(gimp_painter_paint_gate_parent_class)->dispose(object);}
}
static void gimp_painter_paint_gate_class_init(GimpPainterPaintGateClass*klass)
{
  G_OBJECT_CLASS(klass)->dispose=dispose;
  GIMP_PAINT_CORE_CLASS(klass)->check_start=refuse;
  GIMP_PAINT_CORE_CLASS(klass)->start=refuse;
}
static void gimp_painter_paint_gate_init(GimpPainterPaintGate*self)
{
  boundary_void(nullptr,[&]{auto&binding=BindingStore::ensure(G_OBJECT(self));binding.emplace<GateSlot>();binding.activate();});
}
void gimp_painter_paint_gate_register(Gimp*gimp,GimpPaintRegisterCallback callback)
{callback(gimp,GIMP_TYPE_PAINTER_PAINT_GATE,GIMP_TYPE_PAINTER_MYBRUSH_OPTIONS,"gimp-painter-mypaint",_("Painter MyPaint"),"gimp-tool-mypaint-brush");}
gboolean gimp_painter_paint_gate_stroke(GimpPaintCore*core,GimpDrawable*drawable,GimpPaintOptions*options,const GimpPaintStrokeSegment*segments,gsize n_segments,gboolean push_undo,GError**error)
{
  return boundary<gboolean>(error,FALSE,[&]() -> gboolean{
    Admission admission(core);auto operation=admission.operation;
    if(!GIMP_IS_PAINTER_MYBRUSH_OPTIONS(options))throw std::invalid_argument("Extended stroke requires Painter options");
    auto model=PainterOptionsRef::retain(GIMP_PAINTER_MYBRUSH_OPTIONS(options));model.snapshot();
    operation->drawable=ObjectRef<GimpDrawable>::retain(drawable);
    if(!operation->drawable||!gimp_item_is_attached(GIMP_ITEM(drawable)))throw std::invalid_argument("Expected attached stroke drawable");
    operation->image=ObjectRef<GimpImage>::retain(gimp_item_get_image(GIMP_ITEM(drawable)));
    if(!segments&&n_segments)throw std::invalid_argument("Expected stroke segments");
    if(gimp_image_has_pending_paint(operation->image.get()))throw std::runtime_error("Image is already processing paint");
    std::unique_ptr<WeakOperation>weak(new WeakOperation(operation));
    operation->pending=Connection::connect(ObjectRef<GObject>::retain(G_OBJECT(operation->image.get())),"query-pending-paint",G_CALLBACK(query_pending),weak.get(),weak_destroy);weak.release();operation->active=true;
    GError*create_error=nullptr;operation->session=ObjectRef<GimpPainterSession>::adopt(gimp_painter_session_new(model.get(),&create_error));
    if(!operation->session){std::string why=create_error?create_error->message:"Cannot create extended stroke session";g_clear_error(&create_error);throw std::runtime_error(why);}
    admission.validate();checked([&](GError**e){return gimp_painter_session_begin_named_batch(operation->session.get(),push_undo,core->undo_desc,e);});operation->batch=true;
    core->start_coords=core->last_coords;
    gint off_x,off_y;gimp_item_get_offset(GIMP_ITEM(drawable),&off_x,&off_y);
    for(gsize segment=0;segment<n_segments;++segment){
      const auto&input=segments[segment];if(!input.n_coords)continue;if(!input.coords)throw std::invalid_argument("Expected stroke coordinates");
      admission.validate();checked([&](GError**e){return gimp_painter_session_next_segment(operation->session.get(),e);});
      GimpCoords previous=input.coords[0];previous.x-=off_x;previous.y-=off_y;
      core->cur_coords=core->last_coords=input.coords[0];
      // Current GIMP3 generic MyPaint timing: zero-pressure initialization,
      // 15ms first draw, then 0.5ms per pixel of synthetic interpolation.
      auto hover=previous;hover.pressure=0;
      checked([&](GError**e){return gimp_painter_session_hover_to(operation->session.get(),drawable,1.,&hover,nullptr,e);});
      for(gsize point=0;point<input.n_coords;++point){
        auto coords=input.coords[point];coords.x-=off_x;coords.y-=off_y;
        const double seconds=point?.0005*std::hypot(coords.x-previous.x,coords.y-previous.y):.015;
        core->cur_coords=input.coords[point];core->last_paint.x=core->cur_coords.x;core->last_paint.y=core->cur_coords.y;
        checked([&](GError**e){return gimp_painter_session_stroke_to(operation->session.get(),drawable,seconds,&coords,nullptr,e);});
        admission.validate();previous=coords;core->last_coords=core->cur_coords;
      }
    }
    admission.validate();checked([&](GError**e){return gimp_painter_session_end_batch(operation->session.get(),TRUE,e);});operation->batch=false;admission.committed=true;
    return TRUE;
  });
}
