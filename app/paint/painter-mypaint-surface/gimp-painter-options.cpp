/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include "../../painter/resources.hpp"
#include <gtk/gtk.h>
extern "C" {
#include "libgimpconfig/gimpconfig.h"
#include "core/core-types.h"
#include "core/gimp.h"
#include "core/gimpcontext.h"
#include "core/gimpcontainer.h"
#include "core/gimpdatafactory.h"
#include "core/gimpbrush.h"
#include "core/gimppattern.h"
#include "core/gimptoolinfo.h"
#include "core/gimppaintermybrush.h"
}
#include "gimp-painter-options.hpp"
#include "core/gimppaintermybrush-handle.hpp"
#include "painter/binding-store.hpp"
#include "painter/connection.hpp"
#include "painter/source.hpp"
#include "painter/gimp-painter-binding.h"
#include "mypaintbrush-settings-data.h"
#include <algorithm>
namespace GimpPainter {
template<> struct TypeTraits<Gimp>
{ static GType type () noexcept { return GIMP_TYPE_GIMP; } };
}
using namespace GimpPainter;
using MyPaint::Resource;
static void config_iface_init(GimpConfigInterface*iface);
G_DEFINE_TYPE_WITH_CODE (GimpPainterMybrushOptions, gimp_painter_mybrush_options, GIMP_TYPE_PAINT_OPTIONS,
                        G_IMPLEMENT_INTERFACE(GIMP_TYPE_CONFIG,config_iface_init))
static GimpConfigInterface*parent_config_iface=nullptr;
namespace {
enum { PROP_JSON=BRUSH_SETTINGS_COUNT+1, PROP_DIRTY, PROP_CONFLICT };
GParamSpec *properties[PROP_CONFLICT+1]{};
struct History {
  ObjectRef<GimpPainterMybrush> source;
  Resource resource;
  std::string name;
};
/* Legacy history was a process singleton. Share it across every Options in
 * one application while keeping test/application instances and teardown apart.
 * The application's existing BindingStore is the only ownership bridge. */
struct HistoryState : std::enable_shared_from_this<HistoryState> {
  struct Observer { WeakRef<GimpPainterMybrushOptions> owner; std::uint64_t generation; };
  std::vector<History> entries;
  std::vector<Observer> observers;
  Source notification;
  bool closed = false;
  void observe (GimpPainterMybrushOptions *options) {
    observers.erase (std::remove_if (observers.begin (), observers.end (),
      [](const Observer& observer) { return !observer.owner.lock (); }), observers.end ());
    observers.push_back ({WeakRef<GimpPainterMybrushOptions> (ObjectRef<GimpPainterMybrushOptions>::retain (options)),
                          BindingStore::require (G_OBJECT (options)).generation ()});
  }
  void push (History value) {
    if (closed) return;
    entries.push_back (std::move (value));
    if (notification.active ()) return;
    std::weak_ptr<HistoryState> weak = shared_from_this ();
    notification = Source::idle (nullptr, G_PRIORITY_DEFAULT_IDLE, [weak] {
      auto state = weak.lock (); if (!state || state->closed) return false;
      /* Callbacks can create/close Options and grow the observer vector. Never
       * iterate that vector through a signal emission. */
      struct Target { ObjectRef<GimpPainterMybrushOptions> owner; std::uint64_t generation; };
      const auto revision = state->entries.size ();
      std::vector<Target> targets;
      for (const auto& observer : state->observers)
        if (auto owner = observer.owner.lock ()) targets.push_back ({std::move (owner), observer.generation});
      for (const auto& target : targets) {
        if (state->closed) break;
        auto *binding = BindingStore::find (G_OBJECT (target.owner.get ()));
        if (binding && binding->accepts (target.generation))
          g_signal_emit_by_name (target.owner.get (), "history-changed");
      }
      return !state->closed && state->entries.size () != revision;
    });
  }
  void close () noexcept {
    if (closed) return;
    closed = true; notification.close (); observers.clear ();
    auto old = std::move (entries); // publish empty before resource finalizers
  }
};
struct ApplicationHistory {
  std::shared_ptr<HistoryState> state = std::make_shared<HistoryState> ();
  void close () noexcept { state->close (); }
};
struct ApplicationHistorySlot : SlotSpec<Gimp, ApplicationHistory> {};
std::shared_ptr<HistoryState> application_history (Gimp *gimp) {
  if (!GIMP_IS_GIMP (gimp)) throw Error (GIMP_PAINTER_ERROR_WRONG_TYPE, "Painter history requires an application");
  auto *binding = BindingStore::find (G_OBJECT (gimp));
  if (!binding) {
    binding = &BindingStore::ensure (G_OBJECT (gimp));
    binding->emplace<ApplicationHistorySlot> ();
    binding->activate ();
  }
  return binding->with<ApplicationHistorySlot> ([] (ApplicationHistory& history) { return history.state; });
}
struct OptionsImpl {
  Resource draft;
  ObjectRef<GimpPainterMybrush> selected;
  std::string baseline;
  Connection changed;
  std::shared_ptr<HistoryState> history = std::make_shared<HistoryState> ();
  std::uint64_t revision=0;
  bool dirty=false,conflict=false,committing=false;
  void remember () {
    if(dirty && history)history->push({selected,draft,selected&&gimp_object_get_name(selected.get())?gimp_object_get_name(selected.get()):"Unsaved painter brush"});
  }
  void close () noexcept {changed.close();selected.reset();history.reset();}
};
struct OptionsSlot : SlotSpec<GimpPainterMybrushOptions,OptionsImpl> {};
BindingStore& store(GimpPainterMybrushOptions*o)
{
  if(!GIMP_IS_PAINTER_MYBRUSH_OPTIONS(o))throw Error(GIMP_PAINTER_ERROR_WRONG_TYPE,"Expected painter options");
  return BindingStore::require(G_OBJECT(o));
}
void sync_resources(GimpPainterMybrushOptions*options,std::uint64_t revision);
void notify_changed(GimpPainterMybrushOptions*options,std::uint64_t revision,bool all=true,bool settings_changed=true)
{
  auto owner=ObjectRef<GimpPainterMybrushOptions>::retain(options);
  auto*binding=BindingStore::find(G_OBJECT(options));if(!binding)return;
  const auto generation=binding->generation();
  const auto current=[&](){return binding->accepts(generation)&&binding->read<OptionsSlot>([&](const OptionsImpl&i){return i.revision==revision;});};
  if(settings_changed&&current())sync_resources(options,revision);
  for(unsigned id=1;id<=PROP_CONFLICT;++id) {
    if(!all&&id<PROP_JSON)continue;
    if(!current())return;
    g_object_notify_by_pspec(G_OBJECT(options),properties[id]);
  }
  if(settings_changed&&current())g_signal_emit_by_name(options,"settings-changed");
}
// Match old bidirectional named shape/paper selection. Missing names remain
// editable and keep the current context fallback; callbacks never borrow Impl.
void sync_resources(GimpPainterMybrushOptions*options,std::uint64_t revision)
{
  auto owner=ObjectRef<GimpPainterMybrushOptions>::retain(options);
  auto&binding=store(options);const auto generation=binding.generation();
  auto*context=GIMP_CONTEXT(options);
  for(bool paper:{false,true}) {
    if(!binding.accepts(generation))return;
    auto draft=binding.read<OptionsSlot>([&](const OptionsImpl&i){return i.draft;});
    if(!binding.read<OptionsSlot>([&](const OptionsImpl&i){return i.revision==revision;}))return;
    const int use=paper?BRUSH_USE_GIMP_TEXTURE:BRUSH_USE_GIMP_BRUSHMARK;
    const int specified=paper?BRUSH_TEXTURE_SPECIFIED:BRUSH_BRUSHMARK_SPECIFIED;
    const int text=paper?BRUSH_TEXTURE_NAME:BRUSH_BRUSHMARK_NAME;
    if(!draft.switch_value(use)||!draft.switch_value(specified))continue;
    auto current=ObjectRef<GObject>::retain(paper?G_OBJECT(gimp_context_get_pattern(context)):G_OBJECT(gimp_context_get_brush(context)));
    const auto requested=draft.text_value(text);
    const char*name=current?gimp_object_get_name(current.get()):nullptr;
    if(requested.empty()&&name&&*name) {
      g_object_set(options,paper?"texture-name":"brushmark-name",name,nullptr);
      return; // nested notification owns the new revision
    }
    if(requested.empty()||requested==(name?name:""))continue;
    auto*factory=paper?context->gimp->pattern_factory:context->gimp->brush_factory;
    if(!factory)continue;
    auto*matched=gimp_container_get_child_by_name(gimp_data_factory_get_container(factory),requested.c_str());
    auto replacement=ObjectRef<GObject>::retain(matched?G_OBJECT(matched):nullptr);
    if(replacement) {
      if(paper)gimp_context_set_pattern(context,GIMP_PATTERN(replacement.get()));
      else gimp_context_set_brush(context,GIMP_BRUSH(replacement.get()));
    }
  }
}
void resource_changed(GimpContext*context,bool paper)
{
  auto owner=ObjectRef<GimpPainterMybrushOptions>::retain(GIMP_PAINTER_MYBRUSH_OPTIONS(context));
  auto*binding=BindingStore::find(G_OBJECT(context));if(!binding||binding->state()!=BindingStore::State::active)return;
  auto draft=binding->read<OptionsSlot>([](const OptionsImpl&i){return i.draft;});
  if(!draft.switch_value(paper?BRUSH_USE_GIMP_TEXTURE:BRUSH_USE_GIMP_BRUSHMARK)||
     !draft.switch_value(paper?BRUSH_TEXTURE_SPECIFIED:BRUSH_BRUSHMARK_SPECIFIED))return;
  auto*resource=paper?G_OBJECT(gimp_context_get_pattern(context)):G_OBJECT(gimp_context_get_brush(context));
  const char*name=resource?gimp_object_get_name(resource):nullptr;
  const int text=paper?BRUSH_TEXTURE_NAME:BRUSH_BRUSHMARK_NAME;
  if((name==nullptr)!=draft.text_is_null(text)||draft.text_value(text)!=(name?name:""))
    g_object_set(owner.get(),paper?"texture-name":"brushmark-name",name,nullptr);
}
void brush_changed(GimpContext*context,GimpBrush*brush) noexcept
{
  boundary_void(nullptr,[&]{auto owner=ObjectRef<GimpPainterMybrushOptions>::retain(GIMP_PAINTER_MYBRUSH_OPTIONS(context));
    auto*parent=GIMP_CONTEXT_CLASS(gimp_painter_mybrush_options_parent_class);
    if(parent->brush_changed)parent->brush_changed(context,brush);
    resource_changed(context,false);
  });
}
void pattern_changed(GimpContext*context,GimpPattern*pattern) noexcept
{
  boundary_void(nullptr,[&]{auto owner=ObjectRef<GimpPainterMybrushOptions>::retain(GIMP_PAINTER_MYBRUSH_OPTIONS(context));
    auto*parent=GIMP_CONTEXT_CLASS(gimp_painter_mybrush_options_parent_class);
    if(parent->pattern_changed)parent->pattern_changed(context,pattern);
    resource_changed(context,true);
  });
}
struct WeakOptions {WeakRef<GObject> owner;std::uint64_t generation;};
void weak_destroy(gpointer data,GClosure*){delete static_cast<WeakOptions*>(data);}
void source_changed(GimpPainterMybrush*source,gpointer data) noexcept
{
  boundary_void(nullptr,[&]{
    auto&weak=*static_cast<WeakOptions*>(data);auto object=weak.owner.lock();if(!object)return;
    auto*binding=BindingStore::find(object.get());if(!binding||!binding->accepts(weak.generation))return;
    auto snapshot=PainterMybrushRef::retain(source).snapshot();
    bool adopted=false;
    const auto revision=binding->with<OptionsSlot>([&](OptionsImpl&i){
      if(i.selected.get()!=source||i.committing)return std::uint64_t(0);
      if(i.dirty)i.conflict=snapshot.encode()!=i.baseline;
      else{i.draft=std::move(snapshot);i.baseline=i.draft.encode();i.conflict=false;adopted=true;}
      return ++i.revision;
    });
    if(revision)notify_changed(GIMP_PAINTER_MYBRUSH_OPTIONS(object.get()),revision,adopted,adopted);
  });
}
void select_source(GimpPainterMybrushOptions*options,GimpPainterMybrush*brush)
{
  auto owner=ObjectRef<GimpPainterMybrushOptions>::retain(options);
  auto source=ObjectRef<GimpPainterMybrush>::retain(brush);
  Resource next=source?PainterMybrushRef::retain(source.get()).snapshot():Resource();
  auto&binding=store(options);
  const bool constructing=binding.state()==BindingStore::State::constructing;
  Connection connection;
  if(source) {
    std::unique_ptr<WeakOptions> weak(new WeakOptions{WeakRef<GObject>(ObjectRef<GObject>::retain(G_OBJECT(options))),binding.generation()});
    connection=Connection::connect(ObjectRef<GObject>::retain(G_OBJECT(source.get())),"settings-changed",G_CALLBACK(source_changed),weak.get(),weak_destroy);weak.release();
  }
  struct SelectionChange {std::uint64_t revision;ObjectRef<GimpPainterMybrush> old_source;Connection old_connection;};
  const auto update=[&](OptionsImpl&i){
    i.remember();auto old_source=std::move(i.selected);auto old_connection=std::move(i.changed);
    i.selected=source;i.changed=std::move(connection);i.draft=std::move(next);i.baseline=i.draft.encode();i.dirty=false;i.conflict=false;i.committing=false;
    return SelectionChange{++i.revision,std::move(old_source),std::move(old_connection)};
  };
  auto change=constructing?binding.initialize<OptionsSlot>(update):binding.with<OptionsSlot>(update);
  // Finalizers may select another brush. Release them only after publishing the
  // complete replacement and leaving the implementation borrow.
  change.old_connection.close();change.old_source.reset();
  if(!constructing)notify_changed(options,change.revision);
}
void selected_changed(GimpContext*context,GimpPainterMybrush*brush) noexcept
{boundary_void(nullptr,[&]{select_source(GIMP_PAINTER_MYBRUSH_OPTIONS(context),brush);});}
void constructed(GObject*object)
{
  G_OBJECT_CLASS(gimp_painter_mybrush_options_parent_class)->constructed(object);
  auto*options=GIMP_PAINTER_MYBRUSH_OPTIONS(object);
  if(!options->binding_failed)options->binding_failed=!boundary<bool>(nullptr,false,[&]{
    store(options).activate();
    auto shared_history=application_history(GIMP_CONTEXT(options)->gimp);
    shared_history->observe(options);
    store(options).with<OptionsSlot>([&](OptionsImpl&i){
      for(auto& entry:i.history->entries)shared_history->push(std::move(entry));
      i.history=std::move(shared_history);
    });
    auto*context=GIMP_CONTEXT(options);auto*selected=gimp_context_get_painter_mybrush(context);
    if(!selected&&gimp_get_user_context(context->gimp))selected=gimp_context_get_painter_mybrush(gimp_get_user_context(context->gimp));
    if(!selected)selected=GIMP_PAINTER_MYBRUSH(gimp_painter_mybrush_get_standard(context));
    if(gimp_context_get_painter_mybrush(context)!=selected)gimp_context_set_painter_mybrush(context,selected);
    else select_source(options,selected);
    return true;
  });
  if(options->binding_failed)gimp_painter_binding_close(object,nullptr);
}
void dispose(GObject*object)
{gimp_painter_binding_close(object,nullptr);G_OBJECT_CLASS(gimp_painter_mybrush_options_parent_class)->dispose(object);}
void set_property(GObject*object,guint id,const GValue*value,GParamSpec*pspec)
{
  boundary_void(nullptr,[&]{
    auto owner=ObjectRef<GimpPainterMybrushOptions>::retain(GIMP_PAINTER_MYBRUSH_OPTIONS(object));
    if(id==PROP_JSON){GError*error=nullptr;if(!gimp_painter_mybrush_options_set_json(owner.get(),g_value_get_string(value),&error)){std::string why = GimpPainter::take_error_message (error, "Invalid draft");throw std::runtime_error(why);}return;}
    if(id<1||id>BRUSH_SETTINGS_COUNT){G_OBJECT_WARN_INVALID_PROPERTY_ID(object,id,pspec);return;}
    auto&binding=store(owner.get());const bool constructing=binding.state()==BindingStore::State::constructing;
    const auto update=[&](OptionsImpl&i){
      Resource next=i.draft;
      if(id<=BRUSH_MAPPING_END)next.set_base_value(id-1,g_value_get_double(value));
      else if(id<=BRUSH_BOOL_END)next.set_switch(id-1,g_value_get_boolean(value));
      else next.set_text(id-1,g_value_get_string(value));
      i.draft=std::move(next);i.dirty=true;return ++i.revision;
    };
    auto revision=constructing?binding.initialize<OptionsSlot>(update):binding.with<OptionsSlot>(update);
    if(!constructing)notify_changed(owner.get(),revision,false);
  });
}
void get_property(GObject*object,guint id,GValue*value,GParamSpec*pspec)
{
  boundary_void(nullptr,[&]{
    auto&binding=store(GIMP_PAINTER_MYBRUSH_OPTIONS(object));
    const auto read=[&](const OptionsImpl&i){
      if(id==PROP_JSON)g_value_set_string(value,i.draft.encode().c_str());
      else if(id==PROP_DIRTY)g_value_set_boolean(value,i.dirty);
      else if(id==PROP_CONFLICT)g_value_set_boolean(value,i.conflict);
      else if(id>=1&&id<=BRUSH_MAPPING_END)g_value_set_double(value,i.draft.base_value(id-1));
      else if(id<=BRUSH_BOOL_END&&id>BRUSH_MAPPING_END)g_value_set_boolean(value,i.draft.switch_value(id-1));
      else if(id<=BRUSH_TEXT_END&&id>BRUSH_BOOL_END)g_value_set_string(value,i.draft.text_is_null(id-1)?nullptr:i.draft.text_value(id-1).c_str());
      else G_OBJECT_WARN_INVALID_PROPERTY_ID(object,id,pspec);
    };
    if(binding.state()==BindingStore::State::constructing)binding.initialize<OptionsSlot>(read);else binding.read<OptionsSlot>(read);
  });
}
struct ModelCopy {Resource draft;std::string baseline;ObjectRef<GimpPainterMybrush> selected;bool dirty,conflict;};
ModelCopy model_copy(GimpPainterMybrushOptions*o)
{return store(o).read<OptionsSlot>([](const OptionsImpl&i){return ModelCopy{i.draft,i.baseline,i.selected,i.dirty,i.conflict};});}
void restore_copy(GimpPainterMybrushOptions*dest,const ModelCopy&copy)
{
  auto revision=store(dest).with<OptionsSlot>([&](OptionsImpl&i){
    if(i.selected.get()!=copy.selected.get())throw std::runtime_error("Selection changed while copying painter options");
    i.draft=copy.draft;i.baseline=copy.baseline;i.dirty=copy.dirty;i.conflict=copy.conflict;return ++i.revision;
  });
  notify_changed(dest,revision);
}
gboolean config_copy(GimpConfig*src,GimpConfig*dest,GParamFlags flags)
{
  return boundary<gboolean>(nullptr,FALSE,[&]() -> gboolean{
    auto source=ObjectRef<GimpPainterMybrushOptions>::retain(GIMP_PAINTER_MYBRUSH_OPTIONS(src));
    auto target=ObjectRef<GimpPainterMybrushOptions>::retain(GIMP_PAINTER_MYBRUSH_OPTIONS(dest));auto snapshot=model_copy(source.get());
    if(!parent_config_iface->copy(src,dest,flags))return FALSE;
    if((properties[PROP_JSON]->flags&flags)==flags)restore_copy(target.get(),snapshot);
    return TRUE;
  });
}
GimpConfig*config_duplicate(GimpConfig*config)
{
  return boundary<GimpConfig*>(nullptr,nullptr,[&]{
    auto source=ObjectRef<GimpPainterMybrushOptions>::retain(GIMP_PAINTER_MYBRUSH_OPTIONS(config));auto snapshot=model_copy(source.get());
    auto target=ObjectRef<GimpPainterMybrushOptions>::adopt(GIMP_PAINTER_MYBRUSH_OPTIONS(parent_config_iface->duplicate(config)));
    if(!target)throw std::runtime_error("Unable to duplicate painter options");
    restore_copy(target.get(),snapshot);return GIMP_CONFIG(target.release());
  });
}

}
static void config_iface_init(GimpConfigInterface*iface)
{
  parent_config_iface=static_cast<GimpConfigInterface *>(g_type_interface_peek_parent(iface));
  iface->copy=config_copy;iface->duplicate=config_duplicate;
  // These inherited handlers understand GimpContext's property IDs only;
  // our own numeric/JSON properties use the normal config value serializer.
  iface->serialize_property=nullptr;iface->deserialize_property=nullptr;
}
static void gimp_painter_mybrush_options_class_init(GimpPainterMybrushOptionsClass*klass)
{
  auto*object=G_OBJECT_CLASS(klass);object->constructed=constructed;object->dispose=dispose;object->set_property=set_property;object->get_property=get_property;
  GIMP_CONTEXT_CLASS(klass)->painter_mybrush_changed=selected_changed;
  GIMP_CONTEXT_CLASS(klass)->brush_changed=brush_changed;
  GIMP_CONTEXT_CLASS(klass)->pattern_changed=pattern_changed;
  const auto flags=GParamFlags(G_PARAM_READWRITE|GIMP_CONFIG_PARAM_SERIALIZE);
  // GParamSpec copies and canonicalizes '_' to '-'. Do not allocate C++
  // strings inside this native class initializer (no G_PARAM_STATIC_NAME).
  for(const auto&s:painter_mypaint_settings){properties[s.index+1]=g_param_spec_double(s.internal_name,s.displayed_name,s.tooltip,s.minimum,s.maximum,s.default_value,flags);}
  for(const auto&s:painter_mypaint_switches){properties[s.index+1]=g_param_spec_boolean(s.internal_name,s.displayed_name,nullptr,FALSE,flags);}
  for(const auto&s:painter_mypaint_texts){properties[s.index+1]=g_param_spec_string(s.internal_name,s.displayed_name,nullptr,nullptr,flags);}
  properties[PROP_JSON]=g_param_spec_string("painter-settings","Full painter brush","Lossless settings, curves and unknown data",nullptr,flags);
  properties[PROP_DIRTY]=g_param_spec_boolean("painter-dirty","Edited painter brush",nullptr,FALSE,G_PARAM_READABLE);
  properties[PROP_CONFLICT]=g_param_spec_boolean("painter-conflict","Saved brush changed",nullptr,FALSE,G_PARAM_READABLE);
  for(unsigned i=1;i<=PROP_CONFLICT;++i)g_object_class_install_property(object,i,properties[i]);
  g_signal_new("history-changed",G_TYPE_FROM_CLASS(klass),G_SIGNAL_RUN_LAST,0,nullptr,nullptr,nullptr,G_TYPE_NONE,0);
  g_signal_new("settings-changed",G_TYPE_FROM_CLASS(klass),G_SIGNAL_RUN_LAST,0,nullptr,nullptr,nullptr,G_TYPE_NONE,0);
}
static void gimp_painter_mybrush_options_init(GimpPainterMybrushOptions*options)
{options->binding_failed=!boundary<bool>(nullptr,false,[&]{BindingStore::ensure(G_OBJECT(options)).emplace<OptionsSlot>();return true;});}
gchar*gimp_painter_mybrush_options_dup_json(GimpPainterMybrushOptions*options,GError**error)
{return boundary<gchar*>(error,nullptr,[&]{return store(options).with<OptionsSlot>([](const OptionsImpl&i){return g_strdup(i.draft.encode().c_str());});});}
gboolean gimp_painter_mybrush_options_set_json(GimpPainterMybrushOptions*options,const gchar*json,GError**error)
{
  return boundary<gboolean>(error,FALSE,[&]() -> gboolean{
    auto owner=ObjectRef<GimpPainterMybrushOptions>::retain(options);if(!json)throw std::invalid_argument("Expected painter settings JSON");auto next=Resource::decode(json);
    auto revision=store(options).with<OptionsSlot>([&](OptionsImpl&i){i.draft=std::move(next);i.dirty=true;return ++i.revision;});notify_changed(options,revision);return TRUE;
  });
}
gboolean gimp_painter_mybrush_options_set_curve(GimpPainterMybrushOptions*options,gint setting,gint input,const GimpVector2*points,guint count,GError**error)
{
  return boundary<gboolean>(error,FALSE,[&]() -> gboolean{
    auto owner=ObjectRef<GimpPainterMybrushOptions>::retain(options);if(count&&!points)throw std::invalid_argument("Expected mapping points");
    MyPaint::Mapping validate;validate.set_n(input,count);
    std::vector<MyPaint::Point> curve;
    for(unsigned i=0;i<count;++i){validate.set_point(input,i,points[i].x,points[i].y);curve.push_back({points[i].x,points[i].y});}
    auto revision=store(options).with<OptionsSlot>([&](OptionsImpl&i){Resource next=i.draft;next.set_curve(setting,input,curve);i.draft=std::move(next);i.dirty=true;return ++i.revision;});notify_changed(options,revision);return TRUE;
  });
}
GArray*gimp_painter_mybrush_options_get_curve(GimpPainterMybrushOptions*options,gint setting,gint input,GError**error)
{
  return boundary<GArray*>(error,nullptr,[&]{auto curve=store(options).with<OptionsSlot>([&](const OptionsImpl&i){return i.draft.curve(setting,input);});auto*array=g_array_new(FALSE,FALSE,sizeof(GimpVector2));for(const auto&p:curve){GimpVector2 v={p.x,p.y};g_array_append_val(array,v);}return array;});
}
gboolean gimp_painter_mybrush_options_commit(GimpPainterMybrushOptions*options,GError**error)
{
  return boundary<gboolean>(error,FALSE,[&]() -> gboolean{
    auto owner=ObjectRef<GimpPainterMybrushOptions>::retain(options);
    struct Save {ObjectRef<GimpPainterMybrush> source;Resource value;std::string baseline;std::uint64_t revision;};
    auto save=store(options).read<OptionsSlot>([](const OptionsImpl&i){return Save{i.selected,i.draft,i.baseline,i.revision};});
    if(!save.source)throw std::invalid_argument("No selected painter brush");
    auto source=PainterMybrushRef::retain(save.source.get());if(source.snapshot().encode()!=save.baseline)throw std::runtime_error("The saved brush changed; the unsaved draft is retained");
    store(options).with<OptionsSlot>([](OptionsImpl&i){i.committing=true;});
    try{source.replace(save.value);}catch(...){auto*b=BindingStore::find(G_OBJECT(options));if(b&&b->state()==BindingStore::State::active)b->with<OptionsSlot>([](OptionsImpl&i){i.committing=false;});throw;}
    const auto stored=source.snapshot().encode();
    auto*binding=BindingStore::find(G_OBJECT(options));if(!binding||binding->state()!=BindingStore::State::active)return TRUE;
    auto revision=binding->with<OptionsSlot>([&](OptionsImpl&i){
      i.committing=false;
      if(i.selected.get()!=save.source.get()||i.revision!=save.revision)return std::uint64_t(0);
      i.conflict=stored!=save.value.encode();
      if(!i.conflict){i.baseline=stored;i.dirty=false;}
      return ++i.revision;
    });
    if(revision)notify_changed(options,revision,false,false);
    if(stored!=save.value.encode())throw std::runtime_error("The saved brush changed during commit; the unsaved draft is retained");
    return TRUE;
  });
}
guint gimp_painter_mybrush_options_history_size(GimpPainterMybrushOptions*options)
{return boundary<guint>(nullptr,0,[&]{return store(options).read<OptionsSlot>([](const OptionsImpl&i){return i.history ? guint(i.history->entries.size()) : 0;});});}
gchar*gimp_painter_mybrush_options_history_name(GimpPainterMybrushOptions*options,guint index)
{return boundary<gchar*>(nullptr,nullptr,[&]{return store(options).with<OptionsSlot>([&](const OptionsImpl&i){return g_strdup(i.history->entries.at(index).name.c_str());});});}
gboolean gimp_painter_mybrush_options_restore_history(GimpPainterMybrushOptions*options,guint index,GError**error)
{
  return boundary<gboolean>(error,FALSE,[&]() -> gboolean{
    auto owner=ObjectRef<GimpPainterMybrushOptions>::retain(options);auto saved=store(options).with<OptionsSlot>([&](const OptionsImpl&i){return i.history->entries.at(index);});
    if(gimp_context_get_painter_mybrush(GIMP_CONTEXT(options))==saved.source.get())store(options).with<OptionsSlot>([](OptionsImpl&i){i.remember();});
    else gimp_context_set_painter_mybrush(GIMP_CONTEXT(options),saved.source.get());
    if(gimp_context_get_painter_mybrush(GIMP_CONTEXT(options))!=saved.source.get())throw std::runtime_error("Brush selection changed while restoring history");
    return gimp_painter_mybrush_options_set_json(options,saved.resource.encode().c_str(),error);
  });
}
GimpPainterMybrushOptions*gimp_painter_mybrush_options_ref_for_context(GimpContext*context,GError**error)
{
  return boundary<GimpPainterMybrushOptions*>(error,nullptr,[&]{
    if(!GIMP_IS_CONTEXT(context))throw Error(GIMP_PAINTER_ERROR_WRONG_TYPE,"Expected a valid GimpContext");
    if(!context->gimp)throw Error(GIMP_PAINTER_ERROR_CLOSED,"Painter editor context is disposed");
    auto context_owner=ObjectRef<GObject>::retain(G_OBJECT(context));
    if(GIMP_IS_PAINTER_MYBRUSH_OPTIONS(context)) {
      auto*options=GIMP_PAINTER_MYBRUSH_OPTIONS(context);store(options).with<OptionsSlot>([](const OptionsImpl&){});
      return GIMP_PAINTER_MYBRUSH_OPTIONS(g_object_ref(options));
    }
    auto*info=gimp_context_get_tool(context);
    if(!info||!GIMP_IS_PAINTER_MYBRUSH_OPTIONS(info->tool_options))info=GIMP_TOOL_INFO(gimp_container_get_child_by_name(context->gimp->tool_info_list,"gimp-painter-mypaint-tool"));
    if(!info||!GIMP_IS_PAINTER_MYBRUSH_OPTIONS(info->tool_options))throw std::runtime_error("Painter brush options are not registered in this context");
    auto target=ObjectRef<GimpPainterMybrushOptions>::retain(GIMP_PAINTER_MYBRUSH_OPTIONS(info->tool_options));
    store(target.get()).with<OptionsSlot>([](const OptionsImpl&){});
    auto*selected=gimp_context_get_painter_mybrush(context);
    if(!selected)selected=GIMP_PAINTER_MYBRUSH(gimp_painter_mybrush_get_standard(context));
    auto resource=ObjectRef<GimpPainterMybrush>::retain(selected);
    if(gimp_context_get_painter_mybrush(GIMP_CONTEXT(target.get()))!=selected)
      gimp_context_set_painter_mybrush(GIMP_CONTEXT(target.get()),selected);
    if(gimp_context_get_painter_mybrush(GIMP_CONTEXT(target.get()))!=selected)
      throw std::runtime_error("Brush selection changed while opening the shared painter editor");
    store(target.get()).with<OptionsSlot>([](const OptionsImpl&){});
    return target.release();
  });
}
