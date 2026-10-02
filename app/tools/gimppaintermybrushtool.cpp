/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gtk/gtk.h>
#include <gegl.h>
extern "C" {
#include "libgimpwidgets/gimpwidgets.h"
#include "tools-types.h"
#include "core/gimp.h"
#include "core/gimpcontext.h"
#include "core/gimpcontainer.h"
#include "core/gimpdatafactory.h"
#include "core/gimpdrawable.h"
#include "core/gimpimage.h"
#include "core/gimpprojection.h"
#include "core/gimptoolinfo.h"
#include "display/gimpdisplay.h"
#include "widgets/gimppaintermybrusheditor.h"
#include "widgets/gimphelp-ids.h"
#include "widgets/gimpwidgets-utils.h"
#include "gimpcoloroptions.h"
#include "gimppaintermybrushtool.h"
#include "gimptoolcontrol.h"
#include "gimptooloptions-gui.h"
#include "gimp-intl.h"
}
#include "gimppaintermybrushtool.hpp"
#include "paint/painter-mypaint-surface/gimp-painter-options.hpp"
#include "paint/painter-mypaint-surface/gimp-painter-session.hpp"
#include "painter/binding-store.hpp"
#include "painter/connection.hpp"
#include "painter/gimp-painter-binding.h"
#include "mypaintbrush-settings-data.h"
#include <algorithm>
#include <cmath>
#include <limits>
using namespace GimpPainter;
namespace GimpPainter {
template<> struct TypeTraits<GimpDrawable>{static GType type(){return GIMP_TYPE_DRAWABLE;}};
template<> struct TypeTraits<GimpDisplay>{static GType type(){return GIMP_TYPE_DISPLAY;}};
template<> struct TypeTraits<GimpImage>{static GType type(){return GIMP_TYPE_IMAGE;}};
}
G_DEFINE_TYPE(GimpPainterMybrushTool,gimp_painter_mybrush_tool,GIMP_TYPE_COLOR_TOOL)
namespace {
struct ToolImpl {
  explicit ToolImpl(GimpTool*value):owner(value){}
  GimpTool*owner; // owner owns this slot; never a strong cycle
  ObjectRef<GimpPainterSession> session;
  WeakRef<GimpDisplay> display;
  WeakRef<GimpImage> observed_image;
  WeakRef<GimpDrawable> observed_drawable;
  std::vector<Connection> connections;
  bool hover_pending=false;
  GimpCoords pending_coords=GIMP_COORDS_DEFAULT_VALUES;
  guint32 pending_time=0;
  WeakRef<GimpDisplay> pending_display;
  GimpCoords cursor=GIMP_COORDS_DEFAULT_VALUES,last_press=GIMP_COORDS_DEFAULT_VALUES;
  guint32 last_time=0;
  bool have_time=false,pressed=false,busy=false,stopping=false;
  std::string error;
  void close() noexcept {auto old=std::move(session);auto retired=std::move(connections);pressed=false;hover_pending=false;pending_display.reset();display.reset();observed_image.reset();observed_drawable.reset();for(auto&c:retired)c.close();if(gimp_tool_control_is_active(owner->control))gimp_tool_control_halt(owner->control);if(gimp_draw_tool_is_active(GIMP_DRAW_TOOL(owner)))gimp_draw_tool_stop(GIMP_DRAW_TOOL(owner));g_clear_pointer(&owner->drawables,g_list_free);owner->display=nullptr;if(old)gimp_painter_session_cancel(old.get(),nullptr);}
};
struct ToolSlot:SlotSpec<GimpPainterMybrushTool,ToolImpl>{};
BindingStore&store(GimpTool*tool){return BindingStore::require(G_OBJECT(tool));}
bool usable(GimpTool*tool){auto*b=BindingStore::find(G_OBJECT(tool));return b&&b->state()==BindingStore::State::active;}
void input(GimpTool*,const GimpCoords*,guint32,GimpDisplay*,bool);
struct Operation {
  PainterMybrushToolRef owner;
  BindingStore&binding;
  std::uint64_t generation;
  explicit Operation(GimpTool*tool):owner(PainterMybrushToolRef::retain(GIMP_PAINTER_MYBRUSH_TOOL(tool))),binding(store(tool)),generation(binding.generation())
  {binding.with<ToolSlot>([](ToolImpl&i){if(i.busy)throw std::logic_error("Painter tool is processing input");i.busy=true;});}
  ~Operation() noexcept {
    boundary_void(nullptr,[&]{if(!binding.accepts(generation))return;
      struct Pending {bool ready;GimpCoords coords;guint32 time;ObjectRef<GimpDisplay> display;};
      auto pending=binding.with<ToolSlot>([](ToolImpl&i){i.busy=false;Pending p{i.hover_pending,i.pending_coords,i.pending_time,i.pending_display.lock()};i.hover_pending=false;i.pending_display.reset();return p;});
      if(pending.ready&&pending.display)input(GIMP_TOOL(owner.get()),&pending.coords,pending.time,pending.display.get(),false);
    });
  }
};
struct DrawPause {
  GimpDrawTool*tool;
  explicit DrawPause(GimpTool*value):tool(GIMP_DRAW_TOOL(value)){gimp_draw_tool_pause(tool);}
  ~DrawPause(){gimp_draw_tool_resume(tool);}
};
ObjectRef<GimpPainterSession> session(GimpTool*tool)
{return store(tool).with<ToolSlot>([](ToolImpl&i){return i.session;});}
void report(GimpTool*tool,GimpDisplay*display,GError*error)
{
  if(!error)return;
  const std::string message=error->message;g_clear_error(&error);
  auto*binding=BindingStore::find(G_OBJECT(tool));if(!binding||binding->state()!=BindingStore::State::active)return;
  bool changed=binding->with<ToolSlot>([&](ToolImpl&i){if(i.error==message)return false;i.error=message;return true;});
  if(changed&&display)gimp_tool_message_literal(tool,display,message.c_str());
}
void clear_input(GimpTool*tool)
{
  auto*b=BindingStore::find(G_OBJECT(tool));
  if(b&&b->state()==BindingStore::State::active) {
    auto retired=b->with<ToolSlot>([](ToolImpl&i){i.pressed=false;i.have_time=false;i.hover_pending=false;i.pending_display.reset();i.display.reset();i.observed_image.reset();i.observed_drawable.reset();return std::move(i.connections);});
    for(auto&c:retired)c.close();
  }
  if(gimp_tool_control_is_active(tool->control))gimp_tool_control_halt(tool->control);
  g_clear_pointer(&tool->drawables,g_list_free);tool->display=nullptr;
}
struct WeakTool {WeakRef<GObject> owner;std::uint64_t generation;};
void weak_tool_destroy(gpointer data,GClosure*){delete static_cast<WeakTool*>(data);}
ObjectRef<GObject> lock_tool(gpointer data)
{
  auto&w=*static_cast<WeakTool*>(data);auto object=w.owner.lock();if(!object)return {};
  auto*b=BindingStore::find(object.get());return b&&b->accepts(w.generation)?std::move(object):ObjectRef<GObject>();
}
void cancel_observed(GObject*,gpointer data) noexcept
{
  boundary_void(nullptr,[&]{auto owner=lock_tool(data);if(!owner)return;auto*tool=GIMP_TOOL(owner.get());
    auto current=store(tool).with<ToolSlot>([](ToolImpl&i){if(i.stopping)return ObjectRef<GimpPainterSession>();i.stopping=true;return i.session;});
    if(!current)return;
    clear_input(tool);gimp_painter_session_cancel(current.get(),nullptr);
    auto*b=BindingStore::find(owner.get());if(b&&b->state()==BindingStore::State::active)b->with<ToolSlot>([](ToolImpl&i){i.stopping=false;});
  });
}
void target_notify(GObject*object,GParamSpec*,gpointer data) noexcept {cancel_observed(object,data);}
void image_dirty(GimpImage*image,GimpDirtyMask,gpointer data) noexcept
{
  boundary_void(nullptr,[&]{auto owner=lock_tool(data);if(!owner)return;auto*tool=GIMP_TOOL(owner.get());
    const bool own=store(tool).with<ToolSlot>([](const ToolImpl&i){return i.busy||i.stopping||!gimp_painter_session_is_active(i.session.get());});
    if(!own)cancel_observed(G_OBJECT(image),data);
  });
}
gboolean query_pending(GimpImage*,gpointer data) noexcept
{return boundary<gboolean>(nullptr,FALSE,[&]() -> gboolean{auto owner=lock_tool(data);return owner?store(GIMP_TOOL(owner.get())).with<ToolSlot>([](const ToolImpl&i)->gboolean{return i.busy;}):FALSE;});}
void image_saving(GimpImage*,gpointer data) noexcept
{
  boundary_void(nullptr,[&]{auto owner=lock_tool(data);if(!owner)return;auto current=session(GIMP_TOOL(owner.get()));gimp_painter_session_finish(current.get(),nullptr);});
}
void observe_targets(GimpTool*tool,GimpDisplay*display,GimpImage*image,GimpDrawable*drawable)
{
  auto&binding=store(tool);
  const bool same=binding.with<ToolSlot>([&](const ToolImpl&i){auto old_image=i.observed_image.lock();auto old_drawable=i.observed_drawable.lock();auto old_display=i.display.lock();return old_image.get()==image&&old_drawable.get()==drawable&&old_display.get()==display;});
  if(same)return;
  std::vector<Connection> connections;
  const auto connect=[&](GObject*object,const char*signal,GCallback callback){
    std::unique_ptr<WeakTool> weak(new WeakTool{WeakRef<GObject>(ObjectRef<GObject>::retain(G_OBJECT(tool))),binding.generation()});
    auto connection=Connection::connect(ObjectRef<GObject>::retain(object),signal,callback,weak.get(),weak_tool_destroy);weak.release();connections.push_back(std::move(connection));
  };
  connect(G_OBJECT(display),"notify::image",G_CALLBACK(target_notify));
  connect(G_OBJECT(drawable),"removed",G_CALLBACK(cancel_observed));
  for(auto*name:{"notify::lock-content","notify::offset-x","notify::offset-y"})connect(G_OBJECT(drawable),name,G_CALLBACK(target_notify));
  for(auto*name:{"selected-layers-changed","selected-channels-changed","mask-changed","precision-changed"})connect(G_OBJECT(image),name,G_CALLBACK(cancel_observed));
  connect(G_OBJECT(image),"dirty",G_CALLBACK(image_dirty));connect(G_OBJECT(image),"clean",G_CALLBACK(image_dirty));
  connect(G_OBJECT(image),"saving",G_CALLBACK(image_saving));connect(G_OBJECT(image),"query-pending-paint",G_CALLBACK(query_pending));
  auto retired=binding.with<ToolSlot>([&](ToolImpl&i){auto previous=std::move(i.connections);i.connections=std::move(connections);i.observed_image=WeakRef<GimpImage>(ObjectRef<GimpImage>::retain(image));i.observed_drawable=WeakRef<GimpDrawable>(ObjectRef<GimpDrawable>::retain(drawable));return previous;});
  for(auto&c:retired)c.close();
}
void input(GimpTool*tool,const GimpCoords*coords,guint32 time,GimpDisplay*display,bool paint)
{
  Operation operation(tool);DrawPause draw_pause(tool);auto display_owner=ObjectRef<GimpDisplay>::retain(display);
  auto image=ObjectRef<GimpImage>::retain(gimp_display_get_image(display));if(!image)return;
  GList*selected=gimp_image_get_selected_drawables(image.get());
  auto drawable=selected&&!selected->next?ObjectRef<GimpDrawable>::retain(GIMP_DRAWABLE(selected->data)):ObjectRef<GimpDrawable>();g_list_free(selected);
  if(!drawable) {auto current=session(tool);gimp_painter_session_cancel(current.get(),nullptr);clear_input(tool);return;}
  observe_targets(tool,display,image.get(),drawable.get());
  struct Input {ObjectRef<GimpPainterSession> session;bool display_changed;double seconds;};
  auto state=operation.binding.with<ToolSlot>([&](ToolImpl&i){
    auto previous=i.display.lock();const guint32 elapsed=time-i.last_time;const bool display_changed=i.have_time&&previous.get()!=display;
    const double seconds=i.have_time&&previous.get()==display&&elapsed<=std::numeric_limits<gint32>::max()?elapsed/1000.:0.;
    i.display=WeakRef<GimpDisplay>(display_owner);i.have_time=true;i.last_time=time;i.cursor=*coords;
    return Input{i.session,display_changed,seconds};
  });
  GError*error=nullptr;
  if(state.display_changed)gimp_painter_session_cancel(state.session.get(),&error);
  if(error){report(tool,display,error);return;}
  GimpCoords local=*coords;gint x,y;gimp_item_get_offset(GIMP_ITEM(drawable.get()),&x,&y);local.x-=x;local.y-=y;
  const gboolean success=paint?gimp_painter_session_stroke_to(state.session.get(),drawable.get(),state.seconds,&local,nullptr,&error):
                               gimp_painter_session_hover_to(state.session.get(),drawable.get(),state.seconds,&local,nullptr,&error);
  if(!success)report(tool,display,error);
  if(success&&gimp_display_get_image(display)==image.get()) {
    gimp_projection_finish_draw(gimp_image_get_projection(image.get()));
    gimp_projection_flush(gimp_image_get_projection(image.get()));
    gimp_display_flush_now(display);
  }
  if(operation.binding.accepts(operation.generation)&&success)operation.binding.with<ToolSlot>([](ToolImpl&i){i.error.clear();});
}
void constructed(GObject*object)
{
  G_OBJECT_CLASS(gimp_painter_mybrush_tool_parent_class)->constructed(object);auto*self=GIMP_PAINTER_MYBRUSH_TOOL(object);auto*tool=GIMP_TOOL(object);
  if(!self->binding_failed)self->binding_failed=!boundary<bool>(nullptr,false,[&]{
    auto*options=gimp_tool_get_options(tool);if(!GIMP_IS_PAINTER_MYBRUSH_OPTIONS(options))throw std::invalid_argument("Extended painter tool requires painter options");
    GError*error=nullptr;auto value=ObjectRef<GimpPainterSession>::adopt(gimp_painter_session_new(GIMP_PAINTER_MYBRUSH_OPTIONS(options),&error));
    if(!value){std::string why=error?error->message:"Unable to initialize painter session";g_clear_error(&error);throw std::runtime_error(why);}
    auto&binding=store(tool);binding.initialize<ToolSlot>([&](ToolImpl&i){i.session=std::move(value);});binding.activate();return true;
  });
  if(self->binding_failed)gimp_painter_binding_close(object,nullptr);
}
void dispose(GObject*object)
{
  gimp_painter_binding_close(object,nullptr);clear_input(GIMP_TOOL(object));G_OBJECT_CLASS(gimp_painter_mybrush_tool_parent_class)->dispose(object);
}
void control(GimpTool*tool,GimpToolAction action,GimpDisplay*display)
{
  boundary_void(nullptr,[&]{auto owner=PainterMybrushToolRef::retain(GIMP_PAINTER_MYBRUSH_TOOL(tool));
    if(usable(tool)&&(action==GIMP_TOOL_ACTION_HALT||action==GIMP_TOOL_ACTION_COMMIT)){auto current=session(tool);GError*error=nullptr;gimp_painter_session_finish(current.get(),&error);report(tool,display,error);clear_input(tool);}
    GIMP_TOOL_CLASS(gimp_painter_mybrush_tool_parent_class)->control(tool,action,display);
  });
}
void press(GimpTool*tool,const GimpCoords*coords,guint32 time,GdkModifierType state,GimpButtonPressType type,GimpDisplay*display)
{
  if(!usable(tool))return;
  if(gimp_color_tool_is_enabled(GIMP_COLOR_TOOL(tool))){GIMP_TOOL_CLASS(gimp_painter_mybrush_tool_parent_class)->button_press(tool,coords,time,state,type,display);return;}
  if(type!=GIMP_BUTTON_PRESS_NORMAL)return;
  boundary_void(nullptr,[&]{auto owner=PainterMybrushToolRef::retain(GIMP_PAINTER_MYBRUSH_TOOL(tool));
    const bool accepted=store(tool).with<ToolSlot>([&](ToolImpl&i){if(i.busy)return false;i.pressed=true;i.last_press=*coords;return true;});
    if(!accepted)return;
    tool->display=display;gimp_tool_control_activate(tool->control);input(tool,coords,time,display,true);
  });
}
void release(GimpTool*tool,const GimpCoords*coords,guint32 time,GdkModifierType state,GimpButtonReleaseType type,GimpDisplay*display)
{
  if(!usable(tool))return;
  if(gimp_color_tool_is_enabled(GIMP_COLOR_TOOL(tool))){GIMP_TOOL_CLASS(gimp_painter_mybrush_tool_parent_class)->button_release(tool,coords,time,state,type,display);return;}
  boundary_void(nullptr,[&]{auto owner=PainterMybrushToolRef::retain(GIMP_PAINTER_MYBRUSH_TOOL(tool));
    const bool busy=store(tool).with<ToolSlot>([&](ToolImpl&i){i.pressed=false;
      if(i.busy&&type!=GIMP_BUTTON_RELEASE_CANCEL){i.hover_pending=true;i.pending_coords=*coords;i.pending_time=time;i.pending_display=WeakRef<GimpDisplay>(ObjectRef<GimpDisplay>::retain(display));}
      return i.busy;
    });
    if(gimp_tool_control_is_active(tool->control))gimp_tool_control_halt(tool->control);
    if(type==GIMP_BUTTON_RELEASE_CANCEL){auto current=session(tool);gimp_painter_session_cancel(current.get(),nullptr);}
    else if(!busy)input(tool,coords,time,display,false); // engine decides the idle split
    g_clear_pointer(&tool->drawables,g_list_free);
  });
}
void motion(GimpTool*tool,const GimpCoords*coords,guint32 time,GdkModifierType state,GimpDisplay*display)
{
  if(!usable(tool))return;
  if(gimp_color_tool_is_enabled(GIMP_COLOR_TOOL(tool))){GIMP_TOOL_CLASS(gimp_painter_mybrush_tool_parent_class)->motion(tool,coords,time,state,display);return;}
  boundary_void(nullptr,[&]{const bool pressed=store(tool).with<ToolSlot>([](ToolImpl&i){return i.pressed;});input(tool,coords,time,display,pressed&&gimp_tool_control_is_active(tool->control));});
}
void modifier(GimpTool*tool,GdkModifierType key,gboolean press,GdkModifierType,GimpDisplay*display)
{
  if(!usable(tool)||key!=gimp_get_constrain_behavior_mask())return;
  boundary_void(nullptr,[&]{auto owner=PainterMybrushToolRef::retain(GIMP_PAINTER_MYBRUSH_TOOL(tool));
    if(press){auto*info=gimp_get_tool_info(display->gimp,"gimp-color-picker-tool");if(info)gimp_color_tool_enable(GIMP_COLOR_TOOL(tool),GIMP_COLOR_OPTIONS(info->tool_options));}
    else if(gimp_color_tool_is_enabled(GIMP_COLOR_TOOL(tool)))gimp_color_tool_disable(GIMP_COLOR_TOOL(tool));
  });
}
void oper_update(GimpTool*tool,const GimpCoords*coords,GdkModifierType state,gboolean proximity,GimpDisplay*display)
{
  if(!usable(tool))return;
  if(gimp_color_tool_is_enabled(GIMP_COLOR_TOOL(tool))){GIMP_TOOL_CLASS(gimp_painter_mybrush_tool_parent_class)->oper_update(tool,coords,state,proximity,display);return;}
  boundary_void(nullptr,[&]{auto owner=PainterMybrushToolRef::retain(GIMP_PAINTER_MYBRUSH_TOOL(tool));
    store(tool).with<ToolSlot>([&](ToolImpl&i){i.cursor=*coords;});auto*draw=GIMP_DRAW_TOOL(tool);
    if(!proximity||!gimp_display_get_image(display)){if(gimp_draw_tool_is_active(draw))gimp_draw_tool_stop(draw);return;}
    if(gimp_draw_tool_is_active(draw)&&draw->display!=display)gimp_draw_tool_stop(draw);
    if(!gimp_draw_tool_is_active(draw))gimp_draw_tool_start(draw,display);
    else{gimp_draw_tool_pause(draw);gimp_draw_tool_resume(draw);}
  });
}
void draw(GimpDrawTool*draw_tool)
{
  if(!usable(GIMP_TOOL(draw_tool))||!draw_tool->display||!gimp_display_get_image(draw_tool->display))return;
  if(gimp_color_tool_is_enabled(GIMP_COLOR_TOOL(draw_tool))){GIMP_DRAW_TOOL_CLASS(gimp_painter_mybrush_tool_parent_class)->draw(draw_tool);return;}
  boundary_void(nullptr,[&]{auto*tool=GIMP_TOOL(draw_tool);auto*b=BindingStore::find(G_OBJECT(tool));if(!b||b->state()!=BindingStore::State::active)return;auto coords=store(tool).with<ToolSlot>([](const ToolImpl&i){return i.cursor;});gdouble radius_log;
    g_object_get(gimp_tool_get_options(tool),"radius-logarithmic",&radius_log,nullptr);const double radius=std::exp(radius_log);
    gimp_draw_tool_add_arc(draw_tool,FALSE,coords.x-radius,coords.y-radius,2*radius,2*radius,0,2*G_PI);
  });
}
GtkWidget*options_gui(GimpToolOptions*options)
{
  auto*box=gimp_tool_options_gui(options);GError*error=nullptr;
  auto*editor=gimp_painter_mybrush_editor_new(GIMP_CONTEXT(options),FALSE,&error);
  if(editor){gtk_box_pack_start(GTK_BOX(box),editor,FALSE,FALSE,0);gtk_widget_show(editor);}
  else{auto*label=gtk_label_new(error?error->message:_("Unable to open the painter brush editor"));gtk_box_pack_start(GTK_BOX(box),label,FALSE,FALSE,0);gtk_widget_show(label);g_clear_error(&error);}
  return box;
}
}
static void gimp_painter_mybrush_tool_class_init(GimpPainterMybrushToolClass*klass)
{
  auto*object=G_OBJECT_CLASS(klass);object->constructed=constructed;object->dispose=dispose;
  auto*tool=GIMP_TOOL_CLASS(klass);tool->control=control;tool->button_press=press;tool->button_release=release;tool->motion=motion;tool->modifier_key=modifier;tool->oper_update=oper_update;
  GIMP_DRAW_TOOL_CLASS(klass)->draw=draw;
}
static void gimp_painter_mybrush_tool_init(GimpPainterMybrushTool*self)
{
  self->binding_failed=!boundary<bool>(nullptr,false,[&]{BindingStore::ensure(G_OBJECT(self)).emplace<ToolSlot>(GIMP_TOOL(self));return true;});
  auto*tool=GIMP_TOOL(self);tool->want_full_motion_tracking=TRUE;
  gimp_tool_control_set_motion_mode(tool->control,GIMP_MOTION_MODE_EXACT);
  gimp_tool_control_set_tool_cursor(tool->control,GIMP_TOOL_CURSOR_INK);
  gimp_tool_control_set_dirty_mask(tool->control,GimpDirtyMask(GIMP_DIRTY_IMAGE|GIMP_DIRTY_IMAGE_STRUCTURE|GIMP_DIRTY_DRAWABLE|GIMP_DIRTY_ACTIVE_DRAWABLE));
  GIMP_COLOR_TOOL(self)->pick_target=GIMP_COLOR_PICK_TARGET_FOREGROUND;
}
void gimp_painter_mybrush_tool_register(GimpToolRegisterCallback callback,gpointer data)
{
  callback(GIMP_TYPE_PAINTER_MYBRUSH_TOOL,GIMP_TYPE_PAINTER_MYBRUSH_OPTIONS,options_gui,
    GimpContextPropMask(GIMP_CONTEXT_PROP_MASK_FOREGROUND|GIMP_CONTEXT_PROP_MASK_BACKGROUND|GIMP_CONTEXT_PROP_MASK_BRUSH|GIMP_CONTEXT_PROP_MASK_PATTERN|GIMP_CONTEXT_PROP_MASK_PAINTER_MYBRUSH),
    "gimp-painter-mypaint-tool",_("Painter MyPaint"),_("Paint with extended legacy MyPaint brushes"),N_("Painter MyPaint"),nullptr,nullptr,GIMP_HELP_TOOL_MYPAINT_BRUSH,GIMP_ICON_TOOL_MYPAINT_BRUSH,data);
}
gboolean gimp_painter_mybrush_tool_has_pending_stroke(GimpPainterMybrushTool*self)
{return boundary<gboolean>(nullptr,FALSE,[&]() -> gboolean{auto current=session(GIMP_TOOL(self));return gimp_painter_session_is_active(current.get());});}

gboolean gimp_painter_mybrush_tool_get_last_press(GimpPainterMybrushTool*self,GimpCoords*coords)
{return boundary<gboolean>(nullptr,FALSE,[&]() -> gboolean{if(!coords)return FALSE;*coords=store(GIMP_TOOL(self)).with<ToolSlot>([](const ToolImpl&i){return i.last_press;});return TRUE;});}
