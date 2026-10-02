/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gegl.h>
#include <gtk/gtk.h>
extern "C" {
#include "libgimpbase/gimpbase.h"
#include "libgimpmath/gimpmath.h"
#include "libgimpwidgets/gimpwidgets.h"
#include "display-types.h"
#include "core/gimp.h"
#include "core/gimpcontext.h"
#include "core/gimpimage.h"
#include "core/gimpimage-undo.h"
#include "core/gimplist.h"
#include "core/gimpcontainer.h"
#include "core/gimptoolinfo.h"
#include "core/gimptoolgroup.h"
#include "core/gimptooloptions.h"
#include "widgets/gimpoverlaybox.h"
#include "widgets/gimpoverlaychild.h"
#include "widgets/gimpoverlayframe.h"
#include "widgets/gimpcoloreditor.h"
#include "widgets/gimpfgbgeditor.h"
#include "widgets/gimppainterlayertiles.h"
#include "widgets/gimppaintermybrusheditor.h"
#include "widgets/gimpdocked.h"
#include "widgets/gimpwidgets-utils.h"
#include "widgets/gimppropwidgets.h"
#include "gimpdisplay.h"
#include "gimpdisplayshell.h"
#include "gimpdisplayshell-actions.h"
#include "gimppaintercanvasui.h"
#include "gimp-intl.h"
}
#include "painter/binding-store.hpp"
#include "painter/connection.hpp"
#include "painter/source.hpp"
#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <vector>
using namespace GimpPainter;
namespace GimpPainter {
template<> struct TypeTraits<GimpDisplayShell> { static GType type () noexcept { return GIMP_TYPE_DISPLAY_SHELL; } };
}
namespace {
struct Canvas;
struct Slot : SlotSpec<GimpDisplayShell, Canvas> {};
struct Panel {
  ObjectRef<GObject> frame;
  double opacity = .85;
  bool retired = false;
};
struct Canvas {
  GimpDisplayShell *shell;
  bool visible = false, closed = false, painting = false, positioning = false, syncing_colors = false;
  std::vector<Panel> panels;
  std::vector<Connection> signals, controls, tool_signals;
  Source refresh_tools;
  GtkWidget *layers = nullptr, *colors = nullptr, *tools = nullptr, *bar = nullptr, *swatches = nullptr;
  ObjectRef<GObject> popup, dock, dock_host, dock_options, canvas;
  Connection popup_connection;
  Connection dock_parent_destroy, dock_destroy;
  bool dock_parent_alive = true, dock_alive = true;
  WeakRef<GObject> dock_parent;
  gint dock_position = -1;
  gboolean dock_expand = TRUE, dock_fill = TRUE;
  guint dock_padding = 0;
  GtkPackType dock_pack = GTK_PACK_START;
  std::vector<ObjectRef<GObject>> roots;
  explicit Canvas (GimpDisplayShell *s) : shell (s), canvas (ObjectRef<GObject>::retain (G_OBJECT (s->canvas))) {}
  GimpContext *ctx () const { return gimp_get_user_context (shell->display->gimp); }
  GimpImage *image () const { return gimp_display_get_image (shell->display); }
  GimpOverlayBox *overlay () const { return GIMP_OVERLAY_BOX (shell->canvas); }
  void close () noexcept;
  void create ();
  void show (bool value);
  void layout ();
  void reconnect ();
  void pointer (double x, double y, bool released);
  void action (const std::string& name);
  void rebuild_tools ();
  void schedule_tools ();
  bool attach (GtkWidget *widget);
  void detach () noexcept;
};
template<class F> void use (GObject *owner, F&& f) noexcept {
  try { auto *store=BindingStore::find (owner);
    if (store && store->state () == BindingStore::State::active) store->with<Slot> (std::forward<F> (f));
  } catch (const std::exception& e) { g_warning ("Painter canvas UI: %s",e.what ()); }
}
struct Hook {
  WeakRef<GObject> owner;
  ObjectRef<GObject> target;
  std::string command;
  Hook (Canvas& s, GObject *t=nullptr, const char *c="")
    : owner(ObjectRef<GObject>::retain(G_OBJECT(s.shell))), target(ObjectRef<GObject>::retain(t)),command(c) {}
};
template<class F> void dispatch (gpointer data, F&& f) noexcept {
  try {
  auto *hook=static_cast<Hook*>(data);auto owner=hook->owner.lock();auto target=hook->target;auto command=hook->command;
  if(owner)use(owner.get(),[&](Canvas& s){ f(s,target.get(),command); });
  } catch (const std::exception& error) { g_warning ("Painter canvas callback: %s", error.what ()); }
}
void destroy_hook (gpointer p,GClosure*) { delete static_cast<Hook*>(p); }
void connect (Canvas& s,std::vector<Connection>& connections,GObject *emitter,const char *signal,GCallback callback,GObject *target=nullptr,const char *command="") {
  std::unique_ptr<Hook> hook(new Hook(s,target,command));
  connections.emplace_back(Connection::connect(ObjectRef<GObject>::retain(emitter),signal,callback,hook.get(),destroy_hook));hook.release();
}
void resized (GtkWidget*,GtkAllocation*,gpointer data) { dispatch(data,[](Canvas&s,GObject*,const std::string&){s.layout();}); }
void image_changed (GObject*,GParamSpec*,gpointer data) { dispatch(data,[](Canvas&s,GObject*,const std::string&){s.reconnect();}); }
void clicked (GtkWidget*,gpointer data) { dispatch(data,[](Canvas&s,GObject*,const std::string&name){s.action(name);}); }
void tools_changed (GObject*,gpointer data) { dispatch(data,[](Canvas&s,GObject*,const std::string&){s.schedule_tools();}); }
void tools_added (GimpContainer*,GimpObject*,gpointer data) { tools_changed(nullptr,data); }
void tools_reordered (GimpContainer*,GimpObject*,gint,gint,gpointer data) { tools_changed(nullptr,data); }
void active_color (GObject *widget,GParamSpec*,gpointer data) {
  dispatch(data,[&](Canvas&s,GObject*,const std::string&){
    if(s.syncing_colors || !s.colors || !s.swatches)return;
    s.syncing_colors=true;
    auto active=GIMP_FG_BG_EDITOR(widget)->active_color;
    GtkWidget *other=widget==G_OBJECT(s.swatches)?GIMP_COLOR_EDITOR(s.colors)->fg_bg:s.swatches;
    gimp_fg_bg_editor_set_active(GIMP_FG_BG_EDITOR(other),active);
    s.syncing_colors=false;
  });
}
void tool_changed (GimpContext*,GimpToolInfo *tool,gpointer data) {
  dispatch(data,[&](Canvas&s,GObject*,const std::string&){
    auto *parent=tool?gimp_viewable_get_parent(GIMP_VIEWABLE(tool)):nullptr;
    if(GIMP_IS_TOOL_GROUP(parent)) {
      gimp_tool_group_set_active_tool_info(GIMP_TOOL_GROUP(parent),tool);
      gimp_viewable_set_expanded(parent,TRUE);
    }
    s.schedule_tools();
  });
}
void tool_selected (GtkWidget*,gpointer data) { dispatch(data,[](Canvas&s,GObject*target,const std::string&){
  if (GIMP_IS_TOOL_INFO(target)) {
    auto *parent=gimp_viewable_get_parent(GIMP_VIEWABLE(target));
    if(GIMP_IS_TOOL_GROUP(parent)) {gimp_tool_group_set_active_tool_info(GIMP_TOOL_GROUP(parent),GIMP_TOOL_INFO(target));gimp_viewable_set_expanded(parent,TRUE);}
    gimp_context_set_tool(s.ctx(),GIMP_TOOL_INFO(target));s.schedule_tools();gtk_widget_grab_focus(s.shell->canvas);
  }
}); }
void group_expanded (GObject*widget,GParamSpec*,gpointer data) { dispatch(data,[&](Canvas&s,GObject*target,const std::string&){
  if(target) {
    bool expanded=gtk_expander_get_expanded(GTK_EXPANDER(widget));
    gimp_viewable_set_expanded(GIMP_VIEWABLE(target),expanded);
    if(expanded) {
      auto *active=gimp_tool_group_get_active_tool_info(GIMP_TOOL_GROUP(target));
      if(active)gimp_context_set_tool(s.ctx(),active);
      s.schedule_tools();
    }
  }
}); }
gboolean event (GtkWidget*,GdkEvent*event,gpointer data) {
  dispatch(data,[&](Canvas&s,GObject*,const std::string&){
    if(!s.visible)return;
    switch(event->type) {
      case GDK_BUTTON_PRESS: if(event->button.button==1){s.painting=true;s.pointer(event->button.x,event->button.y,false);}break;
      case GDK_MOTION_NOTIFY: if(s.painting || (event->motion.state&GDK_BUTTON1_MASK)) s.pointer(event->motion.x,event->motion.y,false);break;
      case GDK_BUTTON_RELEASE: if(event->button.button==1){s.painting=false;s.pointer(0,0,true);}break;
      case GDK_PROXIMITY_OUT: case GDK_GRAB_BROKEN: s.painting=false;s.pointer(0,0,true);break;
      case GDK_FOCUS_CHANGE: if(!event->focus_change.in){s.painting=false;s.pointer(0,0,true);}break;
      default:break;
    }
  });return FALSE;
}
gboolean enter (GtkWidget*widget,GdkEventCrossing*,gpointer data) {
  dispatch(data,[&](Canvas&s,GObject*,const std::string&){
    if(!s.painting && !s.closed)gimp_overlay_box_set_child_opacity(s.overlay(),widget,1.);
  });return FALSE;
}
gboolean leave (GtkWidget*widget,GdkEventCrossing*,gpointer data) {
  dispatch(data,[&](Canvas&s,GObject*,const std::string&){
    if(!s.painting) for(auto&panel:s.panels)if(panel.frame.get()==G_OBJECT(widget))gimp_overlay_box_set_child_opacity(s.overlay(),widget,panel.opacity);
  });return FALSE;
}
void shell_destroy (GtkWidget*,gpointer data) {
  auto owner=static_cast<Hook*>(data)->owner.lock();
  if(owner)gimp_painter_canvas_ui_close(GIMP_DISPLAY_SHELL(owner.get()));
}
void popup_closed (GtkWidget*,gpointer data) { dispatch(data,[](Canvas&s,GObject*,const std::string&){s.painting=false;s.pointer(0,0,true);gtk_widget_grab_focus(s.shell->canvas);}); }
void editor_popup_destroyed (GtkWidget*,gpointer data) {
  dispatch(data,[](Canvas&s,GObject*,const std::string&){
    auto popup=std::move(s.popup);s.popup_connection.close();s.painting=false;s.pointer(0,0,true);gtk_widget_grab_focus(s.shell->canvas);
  });
}
GtkWidget *frame (Canvas&s,GtkWidget*child,double opacity=.85) {
  auto content=ObjectRef<GObject>::sink(G_OBJECT(child));
  if(s.closed){gtk_widget_destroy(child);return nullptr;}
  auto ref=ObjectRef<GObject>::sink(G_OBJECT(gimp_overlay_frame_new()));auto*w=GTK_WIDGET(ref.get());
  gtk_container_set_border_width(GTK_CONTAINER(w),4);gtk_container_add(GTK_CONTAINER(w),child);
  gtk_widget_add_events(w,GDK_ENTER_NOTIFY_MASK|GDK_LEAVE_NOTIFY_MASK);
  connect(s,s.signals,G_OBJECT(w),"size-allocate",G_CALLBACK(resized));
  connect(s,s.signals,G_OBJECT(w),"enter-notify-event",G_CALLBACK(enter));
  connect(s,s.signals,G_OBJECT(w),"leave-notify-event",G_CALLBACK(leave));
  s.panels.push_back({ref,opacity,false});
  gimp_overlay_box_add_child(s.overlay(),w,0,0);
  if(s.closed)return nullptr;
  gimp_overlay_box_set_child_opacity(s.overlay(),w,opacity);
  return w;
}
GtkWidget *action_button(Canvas&s,GtkWidget*box,const char*label,const char*name) {
  GtkWidget*w=gtk_button_new_with_label(label);gtk_widget_set_name(w,name);gtk_widget_set_can_focus(w,FALSE);
  gtk_box_pack_start(GTK_BOX(box),w,FALSE,FALSE,0);connect(s,s.signals,G_OBJECT(w),"clicked",G_CALLBACK(clicked),nullptr,name);return w;
}
void Canvas::create () {
  if(layers)return;
  layers=gimp_painter_layer_tiles_new(ctx(),image());
  gimp_painter_layer_tiles_set_display(GIMP_PAINTER_LAYER_TILES(layers),shell->display);
  if(!frame(*this,layers))return;
  connect(*this,signals,G_OBJECT(layers),"popup-closed",G_CALLBACK(popup_closed));
  colors=gimp_color_editor_new(ctx());
  gtk_widget_set_name(colors,"painter-color-editor");
  gimp_editor_set_show_name(GIMP_EDITOR(colors),FALSE);
  auto *notebook=GIMP_COLOR_NOTEBOOK(GIMP_COLOR_EDITOR(colors)->notebook);
  auto *pages=GTK_NOTEBOOK(gimp_color_notebook_get_notebook(notebook));
  for(GList *i=gimp_color_notebook_get_selectors(notebook);i;i=i->next) {
    /* GtkNotebook requests the largest page. Top-align each native selector
     * so the compact wheel stays visible above the taller channel editors;
     * those complete editors remain reachable through the same scroller. */
    gtk_widget_set_valign(GTK_WIDGET(i->data),GTK_ALIGN_START);
    if(!strcmp(G_OBJECT_TYPE_NAME(i->data),"ColorselWheel")) {
      auto *button=GTK_WIDGET(g_object_get_data(G_OBJECT(i->data),"button"));
      if(button)gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button),TRUE);
      else gtk_notebook_set_current_page(pages,gtk_notebook_page_num(pages,GTK_WIDGET(i->data)));
    }
  }
  GtkWidget *color_scroll=gtk_scrolled_window_new(nullptr,nullptr);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(color_scroll),GTK_POLICY_AUTOMATIC,GTK_POLICY_AUTOMATIC);
  gtk_container_add(GTK_CONTAINER(color_scroll),colors);if(!frame(*this,color_scroll,1.))return;
  GtkWidget *toolscroll=gtk_scrolled_window_new(nullptr,nullptr);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(toolscroll),GTK_POLICY_NEVER,GTK_POLICY_AUTOMATIC);
  tools=gtk_box_new(GTK_ORIENTATION_VERTICAL,0);gtk_container_add(GTK_CONTAINER(toolscroll),tools);if(!frame(*this,toolscroll))return;
  bar=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,3);
  action_button(*this,bar,_("Undo"),"painter-canvas-undo");action_button(*this,bar,_("Redo"),"painter-canvas-redo");
  action_button(*this,bar,_("Tool Options"),"painter-canvas-options");action_button(*this,bar,_("MyPaint Editor"),"painter-canvas-mypaint");
  action_button(*this,bar,_("Hide"),"painter-canvas-hide");
  GtkWidget *bar_scroll=gtk_scrolled_window_new(nullptr,nullptr);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(bar_scroll),GTK_POLICY_AUTOMATIC,GTK_POLICY_NEVER);
  gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(bar_scroll),48);
  gtk_container_add(GTK_CONTAINER(bar_scroll),bar);if(!frame(*this,bar_scroll))return;
  swatches=gimp_fg_bg_editor_new(ctx());gtk_widget_set_name(swatches,"painter-fg-bg");
  gtk_widget_set_size_request(swatches,80,48);
  if(!frame(*this,swatches,1.))return;
  connect(*this,signals,G_OBJECT(swatches),"notify::active-color",G_CALLBACK(active_color));
  connect(*this,signals,G_OBJECT(GIMP_COLOR_EDITOR(colors)->fg_bg),"notify::active-color",G_CALLBACK(active_color));
  for(auto*w:{layers,colors,tools,bar,swatches}) roots.push_back(ObjectRef<GObject>::retain(G_OBJECT(w)));
  rebuild_tools();
}
void Canvas::close () noexcept {
  if(closed)return;
  closed=true;refresh_tools.close();signals.clear();controls.clear();tool_signals.clear();popup_connection.close();
  if(popup){gtk_widget_destroy(GTK_WIDGET(popup.get()));popup.reset();}
  detach();
  if (colors) gimp_docked_set_context(GIMP_DOCKED(colors),nullptr);
  if (swatches) gimp_fg_bg_editor_set_context(GIMP_FG_BG_EDITOR(swatches),nullptr);
  auto old_panels=std::move(panels);
  for(auto&panel:old_panels)gtk_widget_destroy(GTK_WIDGET(panel.frame.get()));
  layers=colors=tools=bar=swatches=nullptr;
}
void Canvas::show (bool value) {
  if(closed)return;
  visible=value;
  if(value)create();
  if(closed)return;
  std::vector<ObjectRef<GObject>> snapshot;
  for(auto&panel:panels)snapshot.push_back(panel.frame);
  for(auto&panel:snapshot){if(closed)return;auto*w=GTK_WIDGET(panel.get());if(value&&image())gtk_widget_show_all(w);else gtk_widget_hide(w);}
  /* show_all would otherwise reveal GimpEditor's deliberately hidden name. */
  if(layers)gimp_editor_set_show_name(GIMP_EDITOR(layers),FALSE);
  if(colors)gimp_editor_set_show_name(GIMP_EDITOR(colors),FALSE);
  if(!value){if(layers)gimp_painter_layer_tiles_cancel_interaction(GIMP_PAINTER_LAYER_TILES(layers));painting=false;pointer(0,0,true);if(popup){gtk_widget_destroy(GTK_WIDGET(popup.get()));popup.reset();}detach();}
  layout();gimp_display_shell_set_action_active(shell,"view-painter-canvas-ui",visible);
}
void Canvas::layout () {
  if(positioning||!visible||panels.size()<5)return;
  positioning=true;
  const int w=gtk_widget_get_allocated_width(shell->canvas),h=gtk_widget_get_allocated_height(shell->canvas);
  const bool portrait=w<h;const int color_width=std::max(180,std::min(256,w-110));
  const int color_height=std::max(180,std::min(256,h/3));
  auto place=[&](int index,int x,int y,int width,int height){
    auto*widget=GTK_WIDGET(panels[index].frame.get());
    gtk_widget_set_size_request(widget,width,height);gimp_overlay_box_set_child_position(overlay(),widget,std::max(0,x),std::max(0,y));
  };
  int layer_y=portrait?color_height+8:80;
  int layer_width=std::max(96,gtk_widget_get_allocated_width(GTK_WIDGET(panels[0].frame.get())));
  place(0,w-layer_width,layer_y,96,std::max(100,h-layer_y-60));
  place(1,w-color_width-layer_width-4,0,color_width,color_height);
  place(4,w-layer_width,0,layer_width,64);
  place(2,0,portrait?color_height+8:60,60,std::max(100,h-(portrait?color_height:60)-80));
  int bw=gtk_widget_get_allocated_width(GTK_WIDGET(panels[3].frame.get()));
  int bh=gtk_widget_get_allocated_height(GTK_WIDGET(panels[3].frame.get()));
  place(3,std::max(60,(w-bw)/2),h-bh,std::max(1,std::min(w-164,650)),-1);
  if(dock_host){int dw=std::max(180,std::min(color_width,w-180));
    gimp_overlay_box_set_child_position(overlay(),GTK_WIDGET(dock_host.get()),portrait?64:std::max(64,w-104-dw),portrait?0:color_height+8);
    gtk_widget_set_size_request(GTK_WIDGET(dock_host.get()),dw,std::max(160,(portrait?color_height:h-color_height-70)));
  }
  positioning=false;
}
void Canvas::reconnect () {
  painting=false;pointer(0,0,true);
  if (!image()) detach();
  if(layers)gimp_painter_layer_tiles_set_image(GIMP_PAINTER_LAYER_TILES(layers),image());
  show(visible);
}
void Canvas::pointer (double x,double y,bool released) {
  painting=!released;
  for(auto&panel:panels){auto*w=GTK_WIDGET(panel.frame.get());auto*child=gimp_overlay_child_find(overlay(),w);if(!child)continue;
    if(released){panel.retired=false;gimp_overlay_box_set_child_opacity(overlay(),w,panel.opacity);gimp_overlay_box_set_child_input_pass_through(overlay(),w,FALSE);}
    else {const int width=gtk_widget_get_allocated_width(w),height=gtk_widget_get_allocated_height(w);
      double dx=std::max({child->x-x,0.,x-child->x-width}),dy=std::max({child->y-y,0.,y-child->y-height});
      if(dx*dx+dy*dy<200.*200.){panel.retired=true;gimp_overlay_box_set_child_opacity(overlay(),w,.1);gimp_overlay_box_set_child_input_pass_through(overlay(),w,TRUE);}
    }
  }
  if(dock_host){auto*w=GTK_WIDGET(dock_host.get());gimp_overlay_box_set_child_opacity(overlay(),w,released?.85:.1);gimp_overlay_box_set_child_input_pass_through(overlay(),w,!released);}
}
void Canvas::schedule_tools () {
  if(closed||refresh_tools.active())return;
  auto weak=std::make_shared<WeakRef<GObject>>(ObjectRef<GObject>::retain(G_OBJECT(shell)));
  auto generation=BindingStore::require(G_OBJECT(shell)).generation();
  refresh_tools=Source::idle(nullptr,G_PRIORITY_DEFAULT_IDLE,[weak,generation](){auto owner=weak->lock();if(owner){auto*store=BindingStore::find(owner.get());
    if(store&&store->accepts(generation))store->with<Slot>([](Canvas&s){s.rebuild_tools();});}return false;});
}
void Canvas::rebuild_tools () {
  if(!tools||closed)return;
  refresh_tools.close();controls.clear();tool_signals.clear();
  GList*children=gtk_container_get_children(GTK_CONTAINER(tools));
  std::vector<ObjectRef<GObject>> old_children;
  for(GList*i=children;i;i=i->next)old_children.push_back(ObjectRef<GObject>::retain(G_OBJECT(i->data)));
  g_list_free(children);
  for(auto&child:old_children){gtk_widget_destroy(GTK_WIDGET(child.get()));if(closed)return;}
  auto watch_container=[&](GimpContainer *container){
    connect(*this,tool_signals,G_OBJECT(container),"add",G_CALLBACK(tools_added));
    connect(*this,tool_signals,G_OBJECT(container),"remove",G_CALLBACK(tools_added));
    connect(*this,tool_signals,G_OBJECT(container),"reorder",G_CALLBACK(tools_reordered));
  };
  watch_container(ctx()->gimp->tool_item_list);
  auto add_tool=[&](GtkWidget*box,GimpToolInfo*tool){
    if(gimp_viewable_get_parent(GIMP_VIEWABLE(tool)))
      connect(*this,tool_signals,G_OBJECT(tool),"visible-changed",G_CALLBACK(tools_changed));
    GtkWidget*button=gtk_toggle_button_new();gtk_widget_set_name(button,gimp_object_get_name(tool));
    const char*icon=gimp_viewable_get_icon_name(GIMP_VIEWABLE(tool));
    gtk_button_set_image(GTK_BUTTON(button),gtk_image_new_from_icon_name(icon?icon:"image-missing",GTK_ICON_SIZE_SMALL_TOOLBAR));
    gtk_widget_set_tooltip_text(button,tool->label);gtk_widget_set_can_focus(button,FALSE);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button),gimp_context_get_tool(ctx())==tool);
    gtk_box_pack_start(GTK_BOX(box),button,FALSE,FALSE,0);connect(*this,controls,G_OBJECT(button),"clicked",G_CALLBACK(tool_selected),G_OBJECT(tool));
  };
  for(GList*i=gimp_get_tool_item_iter(ctx()->gimp);i;i=i->next){auto*item=GIMP_TOOL_ITEM(i->data);connect(*this,tool_signals,G_OBJECT(item),"visible-changed",G_CALLBACK(tools_changed));
    if(!gimp_tool_item_get_visible(item))continue;
    if(GIMP_IS_TOOL_GROUP(item)){
      auto*group=GIMP_TOOL_GROUP(item);
      connect(*this,tool_signals,G_OBJECT(group),"active-tool-changed",G_CALLBACK(tools_changed));
      connect(*this,tool_signals,G_OBJECT(group),"expanded-changed",G_CALLBACK(tools_changed));
      watch_container(gimp_viewable_get_children(GIMP_VIEWABLE(group)));
      auto*active=gimp_tool_group_get_active_tool_info(group);
      GtkWidget*expander=gtk_expander_new(nullptr);gtk_widget_set_name(expander,"painter-tool-group");gtk_box_pack_start(GTK_BOX(tools),expander,FALSE,FALSE,0);
      GtkWidget*heading=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,0);if(active)add_tool(heading,active);gtk_expander_set_label_widget(GTK_EXPANDER(expander),heading);
      GtkWidget*box=gtk_box_new(GTK_ORIENTATION_VERTICAL,0);gtk_container_add(GTK_CONTAINER(expander),box);
      for(GList*j=GIMP_LIST(gimp_viewable_get_children(GIMP_VIEWABLE(group)))->queue->head;j;j=j->next)
        if(gimp_tool_item_get_visible(GIMP_TOOL_ITEM(j->data)))add_tool(box,GIMP_TOOL_INFO(j->data));
      /* Legacy tiles expand the current tool's group, not every group in
       * the toolrc. The shared expanded flag additionally remembers explicit
       * collapse and is restored by choosing a tool in that group. */
      auto *current=gimp_context_get_tool(ctx());
      bool active_group=current&&gimp_viewable_get_parent(GIMP_VIEWABLE(current))==GIMP_VIEWABLE(group);
      gtk_expander_set_expanded(GTK_EXPANDER(expander),active_group&&gimp_viewable_get_expanded(GIMP_VIEWABLE(group)));
      connect(*this,controls,G_OBJECT(expander),"notify::expanded",G_CALLBACK(group_expanded),G_OBJECT(group));
    }else if(GIMP_IS_TOOL_INFO(item))add_tool(tools,GIMP_TOOL_INFO(item));
  }
  gtk_widget_show_all(tools);
  if (dock_options) {
    auto *tool = gimp_context_get_tool(ctx());
    if (tool && G_OBJECT(tool->tool_options) != dock_options.get()) {
      detach();
      if (auto *gui = gimp_tools_get_tool_options_gui(tool->tool_options)) {
        attach(gui);dock_options=ObjectRef<GObject>::retain(G_OBJECT(tool->tool_options));
      }
    }
  }
  layout();
}
void dock_parent_destroyed (GtkWidget*,gpointer data) {
  dispatch(data,[](Canvas&s,GObject*,const std::string&){s.dock_parent_alive=false;s.dock_parent.reset();});
}
void dock_widget_destroyed (GtkWidget*,gpointer data) {
  dispatch(data,[](Canvas&s,GObject*,const std::string&){s.dock_alive=false;});
}
bool Canvas::attach (GtkWidget*widget) {
  if(!widget || widget==shell->canvas || closed)return false;
  detach();
  /* A canonical options GUI can be shown in only one presentation. Transfer
   * it back through the old shell before borrowing it here, never leave two
   * controllers believing they own a reparented dock. */
  if (auto *ancestor=gtk_widget_get_ancestor(widget,GIMP_TYPE_DISPLAY_SHELL)) {
    auto *other=GIMP_DISPLAY_SHELL(ancestor);
    if(other!=shell)gimp_painter_canvas_ui_detach_dock(other);
  }
  dock=ObjectRef<GObject>::retain(G_OBJECT(widget));
  dock_parent_alive=true;dock_alive=true;
  if(auto*parent=gtk_widget_get_parent(widget)){
    dock_parent=WeakRef<GObject>(ObjectRef<GObject>::retain(G_OBJECT(parent)));
    if(GTK_IS_BOX(parent)){
      gtk_container_child_get(GTK_CONTAINER(parent),widget,"position",&dock_position,nullptr);
      gtk_box_query_child_packing(GTK_BOX(parent),widget,&dock_expand,&dock_fill,&dock_padding,&dock_pack);
    }
    std::vector<Connection> temporary;
    connect(*this,temporary,G_OBJECT(parent),"destroy",G_CALLBACK(dock_parent_destroyed));
    dock_parent_destroy=std::move(temporary.front());
    gtk_container_remove(GTK_CONTAINER(parent),widget);
  }
  {
    std::vector<Connection> temporary;
    connect(*this,temporary,G_OBJECT(widget),"destroy",G_CALLBACK(dock_widget_destroyed));
    dock_destroy=std::move(temporary.front());
  }
  dock_host=ObjectRef<GObject>::sink(G_OBJECT(gimp_overlay_frame_new()));
  GtkWidget*scroller=gtk_scrolled_window_new(nullptr,nullptr);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroller),GTK_POLICY_AUTOMATIC,GTK_POLICY_AUTOMATIC);
  gtk_container_add(GTK_CONTAINER(scroller),widget);
  gtk_container_add(GTK_CONTAINER(dock_host.get()),scroller);
  gimp_overlay_box_add_child(overlay(),GTK_WIDGET(dock_host.get()),0,0);
  gimp_overlay_box_set_child_opacity(overlay(),GTK_WIDGET(dock_host.get()),.85);
  gtk_widget_show_all(GTK_WIDGET(dock_host.get()));layout();return true;
}
void Canvas::detach () noexcept {
  auto old=std::move(dock);auto host=std::move(dock_host);auto parent=dock_parent.lock();
  dock_parent.reset();dock_options.reset();dock_parent_destroy.close();dock_destroy.close();
  if(!old)return;
  auto*widget=GTK_WIDGET(old.get());
  if(auto*current=gtk_widget_get_parent(widget))gtk_container_remove(GTK_CONTAINER(current),widget);
  if(dock_alive && dock_parent_alive && parent && !gtk_widget_in_destruction(GTK_WIDGET(parent.get())) &&
     (!GTK_IS_BIN(parent.get()) || !gtk_bin_get_child(GTK_BIN(parent.get())))) {
    if(GTK_IS_BOX(parent.get())) {
      if(dock_pack==GTK_PACK_END)gtk_box_pack_end(GTK_BOX(parent.get()),widget,dock_expand,dock_fill,dock_padding);
      else gtk_box_pack_start(GTK_BOX(parent.get()),widget,dock_expand,dock_fill,dock_padding);
      if(dock_position>=0)gtk_box_reorder_child(GTK_BOX(parent.get()),widget,dock_position);
    }else gtk_container_add(GTK_CONTAINER(parent.get()),widget);
  }else if(dock_alive)gtk_widget_hide(widget);
  if(host)gtk_widget_destroy(GTK_WIDGET(host.get()));
  dock_position=-1;
}
void Canvas::action (const std::string&name) {
  if(name=="painter-canvas-hide"){show(false);return;}
  if(!image())return;
  if(name=="painter-canvas-undo"){gimp_image_undo(image());gimp_image_flush(image());}
  else if(name=="painter-canvas-redo"){gimp_image_redo(image());gimp_image_flush(image());}
  else if(name=="painter-canvas-mypaint") {
    if(popup){gtk_widget_destroy(GTK_WIDGET(popup.get()));popup.reset();}
    GError*error=nullptr;GtkWidget*widget=gimp_painter_mybrush_editor_popup(ctx(),GTK_WIDGET(shell),&error);
    if(widget){
      popup=ObjectRef<GObject>::retain(G_OBJECT(widget));
      std::vector<Connection> temporary;
      connect(*this,temporary,G_OBJECT(widget),"destroy",G_CALLBACK(editor_popup_destroyed));
      popup_connection=std::move(temporary.front());
    }
    if(error){gimp_message_literal(ctx()->gimp,G_OBJECT(shell),GIMP_MESSAGE_ERROR,error->message);g_clear_error(&error);}
  }else if(name=="painter-canvas-options") {
    if(dock){detach();return;}
    auto*tool=gimp_context_get_tool(ctx());
    if(tool&&tool->tool_options){auto*gui=gimp_tools_get_tool_options_gui(tool->tool_options);if(gui&&attach(gui))dock_options=ObjectRef<GObject>::retain(G_OBJECT(tool->tool_options));}
  }
  gtk_widget_grab_focus(shell->canvas);
}
}
void gimp_painter_canvas_ui_init (GimpDisplayShell *shell) {
  g_return_if_fail(GIMP_IS_DISPLAY_SHELL(shell));
  try {auto&store=BindingStore::ensure(G_OBJECT(shell));store.emplace<Slot>(shell);store.activate();
    store.with<Slot>([](Canvas&s){
      connect(s,s.signals,G_OBJECT(s.shell),"destroy",G_CALLBACK(shell_destroy));
      connect(s,s.signals,G_OBJECT(s.shell->canvas),"size-allocate",G_CALLBACK(resized));
      connect(s,s.signals,G_OBJECT(s.shell->canvas),"event",G_CALLBACK(event));
      connect(s,s.signals,G_OBJECT(s.shell->display),"notify::image",G_CALLBACK(image_changed));
      connect(s,s.signals,G_OBJECT(s.ctx()),"tool-changed",G_CALLBACK(tool_changed));
    });
  }catch(const std::exception&e){g_warning("Cannot initialize painter canvas: %s",e.what());}
}
void gimp_painter_canvas_ui_close (GimpDisplayShell*shell) {try{if(auto*store=BindingStore::find(G_OBJECT(shell)))store->close();}catch(...) {}}
void gimp_painter_canvas_ui_set_visible (GimpDisplayShell*shell,gboolean value) {use(G_OBJECT(shell),[&](Canvas&s){s.show(value);});}
gboolean gimp_painter_canvas_ui_get_visible (GimpDisplayShell*shell) {bool result=false;use(G_OBJECT(shell),[&](Canvas&s){result=s.visible;});return result;}
void gimp_painter_canvas_ui_pointer (GimpDisplayShell*shell,gdouble x,gdouble y,gboolean released) {use(G_OBJECT(shell),[&](Canvas&s){s.pointer(x,y,released);});}
gboolean gimp_painter_canvas_ui_attach_dock (GimpDisplayShell*shell,GtkWidget*dock) {bool result=false;use(G_OBJECT(shell),[&](Canvas&s){result=s.attach(dock);});return result;}
void gimp_painter_canvas_ui_detach_dock (GimpDisplayShell*shell) {use(G_OBJECT(shell),[](Canvas&s){s.detach();});}
