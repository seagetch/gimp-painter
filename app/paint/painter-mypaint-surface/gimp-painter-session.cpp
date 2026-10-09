/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gtk/gtk.h>
#include "gimp-painter-session.hpp"
#include "gimp-painter-options.hpp"
#include "paint-core.hpp"
#include "painter/binding-store.hpp"
#include "painter/connection.hpp"
#include "painter/gimp-painter-binding.h"
using namespace GimpPainter;
using MyPaint::PaintCore;
G_DEFINE_TYPE(GimpPainterSession,gimp_painter_session,GIMP_TYPE_OBJECT)
namespace {
enum {PROP_OPTIONS=1,PROP_ACTIVE,PROP_ERROR};
struct SessionImpl {
  ObjectRef<GimpPainterMybrushOptions> options;
  std::shared_ptr<PaintCore> core;
  Connection settings,context;
  std::string error;
  bool busy=false,refreshing=false,pending=false;
  std::uint64_t revision=0;
  void close() noexcept {
    auto old_core=std::move(core);auto old_options=std::move(options);
    settings.close();context.close();
    try{if(old_core)old_core->cancel();}catch(...){}
  }
};
struct SessionSlot:SlotSpec<GimpPainterSession,SessionImpl>{};
BindingStore&store(GimpPainterSession*s)
{if(!GIMP_IS_PAINTER_SESSION(s))throw Error(GIMP_PAINTER_ERROR_WRONG_TYPE,"Expected painter session");return BindingStore::require(G_OBJECT(s));}
void refresh(GimpPainterSession*session);
struct WeakSession {WeakRef<GObject> owner;std::uint64_t generation;};
void weak_destroy(gpointer data,GClosure*){delete static_cast<WeakSession*>(data);}
void changed(GObject*,gpointer data) noexcept
{
  boundary_void(nullptr,[&]{auto&w=*static_cast<WeakSession*>(data);auto object=w.owner.lock();if(!object)return;auto*b=BindingStore::find(object.get());if(b&&b->accepts(w.generation))refresh(GIMP_PAINTER_SESSION(object.get()));});
}
void context_changed(GObject*object,GParamSpec*spec,gpointer data) noexcept
{
  if(spec->owner_type==GIMP_TYPE_CONTEXT&&g_strcmp0(spec->name,"painter-mybrush")&&g_strcmp0(spec->name,"mypaint-brush"))changed(object,data);
}
Connection connect(GimpPainterSession*session,GObject*emitter,const char*signal,GCallback callback)
{
  std::unique_ptr<WeakSession> weak(new WeakSession{WeakRef<GObject>(ObjectRef<GObject>::retain(G_OBJECT(session))),store(session).generation()});
  auto connection=Connection::connect(ObjectRef<GObject>::retain(emitter),signal,callback,weak.get(),weak_destroy);weak.release();return connection;
}
void refresh(GimpPainterSession*session)
{
  auto owner=ObjectRef<GimpPainterSession>::retain(session);auto&binding=store(session);const auto generation=binding.generation();
  const bool begin=binding.with<SessionSlot>([](SessionImpl&i){i.pending=true;++i.revision;if(i.busy||i.refreshing)return false;i.refreshing=true;return true;});
  if(!begin)return;
  for(unsigned iteration=0;iteration<16;++iteration) {
    struct Input {ObjectRef<GimpPainterMybrushOptions> options;std::shared_ptr<PaintCore> core;std::uint64_t revision;};
    auto input=binding.with<SessionSlot>([](SessionImpl&i){i.pending=false;return Input{i.options,i.core,i.revision};});
    std::string message;
    try {
      auto resource=PainterOptionsRef::retain(input.options.get()).snapshot();
      if(input.core)input.core->configure(resource);
      else input.core=std::make_shared<PaintCore>(GIMP_PAINT_OPTIONS(input.options.get()),resource);
    }catch(const std::exception&e){message=e.what();try{if(input.core)input.core->finish();}catch(...) {}}
    if(!binding.accepts(generation))return;
    bool changed_error=false;
    const bool again=binding.with<SessionSlot>([&](SessionImpl&i){
      if(i.revision==input.revision){i.core=input.core;changed_error=i.error!=message;i.error=message;}
      const bool pending=i.pending;if(!pending)i.refreshing=false;return pending;
    });
    if(changed_error)g_object_notify(G_OBJECT(session),"error");
    if(!binding.accepts(generation))return;
    if(!again)return;
  }
  binding.with<SessionSlot>([](SessionImpl&i){i.refreshing=false;i.error="Brush settings kept changing during a synchronous update; retry the pending settings";});
  g_object_notify(G_OBJECT(session),"error");
}
void set_property(GObject*object,guint id,const GValue*value,GParamSpec*spec)
{
  property_boundary(object,spec,"set",[&]{if(id!=PROP_OPTIONS){G_OBJECT_WARN_INVALID_PROPERTY_ID(object,id,spec);return;}store(GIMP_PAINTER_SESSION(object)).initialize<SessionSlot>([&](SessionImpl&i){i.options=ObjectRef<GimpPainterMybrushOptions>::retain(GIMP_PAINTER_MYBRUSH_OPTIONS(g_value_get_object(value)));});});
}
void get_property(GObject*object,guint id,GValue*value,GParamSpec*spec)
{
  property_boundary(object,spec,"get",[&]{store(GIMP_PAINTER_SESSION(object)).read<SessionSlot>([&](const SessionImpl&i){
    if(id==PROP_OPTIONS)g_value_set_object(value,i.options.get());
    else if(id==PROP_ACTIVE)g_value_set_boolean(value,i.core&&i.core->active());
    else if(id==PROP_ERROR)g_value_set_string(value,i.error.empty()?nullptr:i.error.c_str());
    else G_OBJECT_WARN_INVALID_PROPERTY_ID(object,id,spec);
  });});
}
void constructed(GObject*object)
{
  G_OBJECT_CLASS(gimp_painter_session_parent_class)->constructed(object);auto*session=GIMP_PAINTER_SESSION(object);
  if(!session->binding_failed)session->binding_failed=!boundary<bool>(nullptr,false,[&]{
    auto&binding=store(session);binding.initialize<SessionSlot>([&](SessionImpl&i){
      if(!i.options)throw std::invalid_argument("Expected painter options");
      i.settings=connect(session,G_OBJECT(i.options.get()),"settings-changed",G_CALLBACK(changed));
      i.context=connect(session,G_OBJECT(i.options.get()),"notify",G_CALLBACK(context_changed));
    });binding.activate();refresh(session);return true;
  });
  if(session->binding_failed)gimp_painter_binding_close(object,nullptr);
}
void dispose(GObject*object)
{gimp_painter_binding_close(object,nullptr);G_OBJECT_CLASS(gimp_painter_session_parent_class)->dispose(object);}
template<class Function> void operate(GimpPainterSession*session,bool painting,Function operation)
{
  auto owner=ObjectRef<GimpPainterSession>::retain(session);auto&binding=store(session);const auto generation=binding.generation();
  if(painting&&binding.read<SessionSlot>([](const SessionImpl&i){return i.pending&&!i.busy&&!i.refreshing;}))refresh(session);
  struct Input {ObjectRef<GimpPainterMybrushOptions> options;std::shared_ptr<PaintCore> core;};
  auto input=binding.with<SessionSlot>([&](SessionImpl&i){
    if(i.busy||i.refreshing)throw std::logic_error("Painter session is transitioning");
    if(painting&&!i.error.empty())throw std::runtime_error(i.error);
    i.busy=true;return Input{i.options,i.core};
  });
  const auto release=[&]{
    if(!binding.accepts(generation))return;
    const bool pending=binding.with<SessionSlot>([](SessionImpl&i){i.busy=false;return i.pending;});
    if(pending)refresh(session);
    if(binding.accepts(generation))g_object_notify(G_OBJECT(session),"active");
  };
  try {
    if(painting){auto*b=BindingStore::find(G_OBJECT(input.options.get()));if(!b||b->state()!=BindingStore::State::active){if(input.core)input.core->cancel();throw Error(GIMP_PAINTER_ERROR_CLOSED,"Painter options are closed");}}
    if(input.core)operation(*input.core);else if(painting)throw std::runtime_error("Painter session has no usable brush");
  }catch(...){try{release();}catch(...){}throw;}
  release();
}
}
static void gimp_painter_session_class_init(GimpPainterSessionClass*klass)
{
  auto*object=G_OBJECT_CLASS(klass);object->constructed=constructed;object->dispose=dispose;object->set_property=set_property;object->get_property=get_property;
  g_object_class_install_property(object,PROP_OPTIONS,g_param_spec_object("options","Painter options",nullptr,GIMP_TYPE_PAINTER_MYBRUSH_OPTIONS,GParamFlags(G_PARAM_READWRITE|G_PARAM_CONSTRUCT_ONLY)));
  g_object_class_install_property(object,PROP_ACTIVE,g_param_spec_boolean("active","Active stroke",nullptr,FALSE,G_PARAM_READABLE));
  g_object_class_install_property(object,PROP_ERROR,g_param_spec_string("error","Brush error",nullptr,nullptr,G_PARAM_READABLE));
}
static void gimp_painter_session_init(GimpPainterSession*session)
{session->binding_failed=!boundary<bool>(nullptr,false,[&]{BindingStore::ensure(G_OBJECT(session)).emplace<SessionSlot>();return true;});}
GimpPainterSession*gimp_painter_session_new(GimpPainterMybrushOptions*options,GError**error)
{
  return boundary<GimpPainterSession*>(error,nullptr,[&]{
    auto source=PainterOptionsRef::retain(options);source.snapshot();
    auto session=ObjectRef<GimpPainterSession>::adopt(GIMP_PAINTER_SESSION(g_object_new(GIMP_TYPE_PAINTER_SESSION,"options",options,nullptr)));
    if(session.get()->binding_failed)throw std::runtime_error("Unable to initialize painter session");
    return session.release();
  });
}
gboolean gimp_painter_session_stroke_to(GimpPainterSession*session,GimpDrawable*drawable,gdouble seconds,const GimpCoords*coords,gboolean*split,GError**error)
{
  return boundary<gboolean>(error,FALSE,[&]() -> gboolean{if(!coords)throw std::invalid_argument("Expected input coordinates");operate(session,true,[&](PaintCore&core){const bool result=core.stroke_to(drawable,seconds,*coords);if(split)*split=result;});return TRUE;});
}
gboolean gimp_painter_session_hover_to(GimpPainterSession*session,GimpDrawable*drawable,gdouble seconds,const GimpCoords*coords,gboolean*split,GError**error)
{
  return boundary<gboolean>(error,FALSE,[&]() -> gboolean{if(!coords)throw std::invalid_argument("Expected input coordinates");operate(session,true,[&](PaintCore&core){const bool result=core.hover_to(drawable,seconds,*coords);if(split)*split=result;});return TRUE;});
}
namespace {
gboolean stop(GimpPainterSession*session,bool commit,GError**error)
{
  return boundary<gboolean>(error,FALSE,[&]() -> gboolean{
    auto owner=ObjectRef<GimpPainterSession>::retain(session);
    auto pending=store(session).with<SessionSlot>([](SessionImpl&i){return (i.busy||i.refreshing)?i.core:std::shared_ptr<PaintCore>();});
    // PaintCore defers start/sample-time stop requests until the guarded input
    // unwinds. Cancel takes priority over a simultaneous finish request.
    if(pending){if(commit)pending->finish();else pending->cancel();}
    else operate(session,false,[&](PaintCore&core){if(commit)core.finish();else core.cancel();});
    return TRUE;
  });
}
}
gboolean gimp_painter_session_finish(GimpPainterSession*session,GError**error)
{return stop(session,true,error);}
gboolean gimp_painter_session_cancel(GimpPainterSession*session,GError**error)
{return stop(session,false,error);}
gboolean gimp_painter_session_begin_batch(GimpPainterSession*session,gboolean push_undo,GError**error)
{return gimp_painter_session_begin_named_batch(session,push_undo,nullptr,error);}
gboolean gimp_painter_session_begin_named_batch(GimpPainterSession*session,gboolean push_undo,const gchar*description,GError**error)
{return boundary<gboolean>(error,FALSE,[&]() -> gboolean{operate(session,true,[&](PaintCore&core){core.begin_batch(push_undo,description);});return TRUE;});}
gboolean gimp_painter_session_next_segment(GimpPainterSession*session,GError**error)
{return boundary<gboolean>(error,FALSE,[&]() -> gboolean{operate(session,true,[](PaintCore&core){core.next_segment();});return TRUE;});}
gboolean gimp_painter_session_end_batch(GimpPainterSession*session,gboolean commit,GError**error)
{
  return boundary<gboolean>(error,FALSE,[&]() -> gboolean{
    operate(session,false,[&](PaintCore&core){
      // A settings change aborts the immutable operation. Once rollback/end
      // completes, reconcile the current options for a future independent call.
      store(session).with<SessionSlot>([](SessionImpl&i){i.pending=true;});
      core.end_batch(commit);
    });return TRUE;
  });
}
gboolean gimp_painter_session_is_active(GimpPainterSession*session)
{return boundary<gboolean>(nullptr,FALSE,[&]{return store(session).read<SessionSlot>([](const SessionImpl&i)->gboolean{return i.core&&i.core->active();});});}
gchar*gimp_painter_session_dup_error(GimpPainterSession*session)
{return boundary<gchar*>(nullptr,nullptr,[&]{return store(session).read<SessionSlot>([](const SessionImpl&i){return i.error.empty()?nullptr:g_strdup(i.error.c_str());});});}
