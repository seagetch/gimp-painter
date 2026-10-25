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
#include "config/gimpcoreconfig.h"
#include "core/gimpcontext.h"
#include "core/gimpimage.h"
#include "core/gimpimage-color-profile.h"
#include "libgimpconfig/gimpconfig.h"
#include "core/gimp-gui.h"
#include "widgets/gimpwindowstrategy.h"
#include "widgets/gimpdialogfactory.h"
#include "core/gimpimage-undo.h"
#include "core/gimplist.h"
#include "core/gimpcontainer.h"
#include "core/gimptoolinfo.h"
#include "core/gimptoolgroup.h"
#include "core/gimptooloptions.h"
#include "widgets/gimpoverlaybox.h"
#include "widgets/gimpdock.h"
#include "widgets/gimpdockcolumns.h"
#include "widgets/gimpoverlaychild.h"
#include "widgets/gimpoverlayframe.h"
#include "widgets/gimpcoloreditor.h"
#include "widgets/gimpfgbgeditor.h"
#include "widgets/gimppainterlayertiles.h"
#include "widgets/gimppaintermybrusheditor.h"
#include "widgets/gimppaintercompactoptions.h"
#include "widgets/gimpdocked.h"
#include "widgets/gimpwidgets-utils.h"
#include "widgets/gimppropwidgets.h"
#include "gimpdisplay.h"
#include "gimpdisplayshell.h"
#include "gimpcanvas.h"
#include "gimpdisplayshell-actions.h"
#include "gimppaintercanvasui.h"
#include "gimp-intl.h"
}
#include "widgets/gimplayertileview-handle.hpp"
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
/* A record has a stable address while GTK emits parent/destroy signals.  Its
 * two bounded owners are the controller slot and an operation-local lease. */
struct Borrow {
  enum class Home { none, box, paned1, paned2, container };
  ObjectRef<GObject> widget, options;
  Panel panel;
  WeakRef<GObject> parent;
  Home home = Home::none;
  bool parent_alive = true, alive = true, compact = false, released = false, right = false;
  gboolean was_visible = FALSE;
  bool compact_restore_pending = false;
  gint position = -1, pane_position = 0, pane_span = 0, pane_width = 0;
  gboolean expand = TRUE, fill = TRUE, resize = FALSE, shrink = FALSE;
  gboolean position_set = FALSE;
  guint padding = 0;
  GtkPackType pack = GTK_PACK_START;
  Connection parent_destroy, widget_destroy;
  std::vector<Connection> signals;
  ~Borrow () noexcept {
    parent_destroy.close();widget_destroy.close();signals.clear();
  }
};
struct Canvas {
  GimpDisplayShell *shell;
  bool visible = false, closed = false, painting = false, positioning = false, syncing_colors = false;
  std::vector<Panel> panels;
  std::vector<Connection> signals, controls, tool_signals, color_image_signals;
  Source refresh_tools, refresh_options, refresh_layout;
  guint64 tool_revision = 0;
  GtkWidget *layers = nullptr, *colors = nullptr, *tools = nullptr, *bar = nullptr, *swatches = nullptr;
  ObjectRef<GObject> popup, canvas, toolbar_gui, toolbar_options;
  Connection popup_connection;
  std::shared_ptr<Borrow> options_dock, right_dock;
  guint64 options_revision = 0, right_revision = 0;
  bool options_busy = false, right_busy = false;
  gint options_presentation = 1;
  bool options_presentation_pending = false;
  guint64 presentation_revision = 0, options_intent = 0;
  std::vector<ObjectRef<GObject>> roots;
  explicit Canvas (GimpDisplayShell *s) : shell (s), canvas (ObjectRef<GObject>::retain (G_OBJECT (s->canvas))) {}
  GimpContext *ctx () const { return gimp_get_user_context (shell->display->gimp); }
  GimpImage *image () const { return gimp_display_get_image (shell->display); }
  GimpOverlayBox *overlay () const { return GIMP_OVERLAY_BOX (shell->canvas); }
  void close () noexcept;
  void create ();
  void show (bool value);
  void layout ();
  void schedule_layout ();
  void reconnect ();
  void sync_color ();
  void sync_color_image ();
  bool update_toolbar ();
  void pointer (double x, double y, bool released);
  void action (const std::string& name);
  void rebuild_tools ();
  void schedule_tools ();
  bool attach (GtkWidget *widget, bool right = false, guint64 revision = 0, bool from_options = false, bool replace = false);
  void schedule_options ();
  std::vector<Panel> panel_snapshot () const;
  void restore (const std::shared_ptr<Borrow>& borrowed) noexcept;
  void finish_compact_restore (const std::shared_ptr<Borrow>& borrowed) noexcept;
  bool attach_options (bool compact, bool preserve_transfer = false);
  void detach (bool right = false) noexcept;
};
template<class F> void use (GObject *owner, F&& f) noexcept {
  try { auto *store=BindingStore::find (owner);
    if (store && store->state () == BindingStore::State::active) store->with<Slot> (std::forward<F> (f));
  } catch (const std::exception& e) { g_warning ("Painter canvas UI: %s",e.what ()); }
  catch (...) { g_warning ("Painter canvas UI: Unknown C++ exception"); }
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
void resized (GtkWidget*,GtkAllocation*,gpointer data) { dispatch(data,[](Canvas&s,GObject*,const std::string&){s.schedule_layout();}); }
void image_changed (GObject*,GParamSpec*,gpointer data) { dispatch(data,[](Canvas&s,GObject*,const std::string&){s.reconnect();}); }
void clicked (GtkWidget*,gpointer data) { dispatch(data,[](Canvas&s,GObject*,const std::string&name){s.action(name);}); }
void tools_changed (GObject*,gpointer data) { dispatch(data,[](Canvas&s,GObject*,const std::string&){s.schedule_tools();}); }
void tools_added (GimpContainer*,GimpObject*,gpointer data) { tools_changed(nullptr,data); }
void tools_reordered (GimpContainer*,GimpObject*,gint,gint,gpointer data) { tools_changed(nullptr,data); }
void active_color (GObject*,GParamSpec*,gpointer data) {
  dispatch(data,[](Canvas&s,GObject*,const std::string&){s.sync_color();});
}
void context_color_changed (GimpContext*,GeglColor*,gpointer data) {
  dispatch(data,[](Canvas&s,GObject*,const std::string&){s.sync_color();});
}
void selector_color_changed (GimpColorSelector*,GeglColor*color,gpointer data) {
  dispatch(data,[&](Canvas&s,GObject*,const std::string&){
    if(s.closed || s.syncing_colors || !s.swatches)return;
    s.syncing_colors=true;
    if(GIMP_FG_BG_EDITOR(s.swatches)->active_color==GIMP_ACTIVE_COLOR_BACKGROUND)
      gimp_context_set_background(s.ctx(),color);
    else gimp_context_set_foreground(s.ctx(),color);
    s.syncing_colors=false;
  });
}
void color_image_changed (GObject*,gpointer data) {
  dispatch(data,[](Canvas&s,GObject*,const std::string&){s.sync_color_image();});
}
void color_mode_changed (GObject*,GParamSpec*,gpointer data) {
  dispatch(data,[](Canvas&s,GObject*,const std::string&){s.sync_color_image();});
}
void tool_changed (GimpContext*,GimpToolInfo *tool,gpointer data) {
  dispatch(data,[&](Canvas&s,GObject*,const std::string&){
    /* Signals can synchronously close this owner or select a newer tool.
     * The binding lease keeps the controller alive, not the operation valid. */
    if(gimp_context_get_tool(s.ctx())!=tool)return;
    const auto revision=++s.tool_revision;
    auto selected=ObjectRef<GObject>::retain(tool?G_OBJECT(tool):nullptr);
    auto parent=ObjectRef<GObject>::retain(tool?G_OBJECT(gimp_viewable_get_parent(GIMP_VIEWABLE(tool))):nullptr);
    if(GIMP_IS_TOOL_GROUP(parent.get())) {
      gimp_tool_group_set_active_tool_info(GIMP_TOOL_GROUP(parent.get()),tool);
      if(s.closed || s.tool_revision!=revision)return;
      gimp_viewable_set_expanded(GIMP_VIEWABLE(parent.get()),TRUE);
      if(s.closed || s.tool_revision!=revision)return;
    }
    s.schedule_tools();
  });
}
void tool_selected (GtkWidget*,gpointer data) { dispatch(data,[](Canvas&s,GObject*target,const std::string&){
  if (GIMP_IS_TOOL_INFO(target)) {
    const auto revision=++s.tool_revision;
    auto focus_selected=[&](){
      if(!s.closed && gimp_context_get_tool(s.ctx())==GIMP_TOOL_INFO(target))
        gtk_widget_grab_focus(s.shell->canvas);
    };
    auto parent=ObjectRef<GObject>::retain(G_OBJECT(gimp_viewable_get_parent(GIMP_VIEWABLE(target))));
    if(GIMP_IS_TOOL_GROUP(parent.get())) {
      gimp_tool_group_set_active_tool_info(GIMP_TOOL_GROUP(parent.get()),GIMP_TOOL_INFO(target));
      /* A same-target tool-manager notification completes this choice;
       * preserve its focus handoff without replaying model changes. */
      if(s.closed || s.tool_revision!=revision){focus_selected();return;}
      gimp_viewable_set_expanded(GIMP_VIEWABLE(parent.get()),TRUE);
      if(s.closed || s.tool_revision!=revision){focus_selected();return;}
    }
    gimp_context_set_tool(s.ctx(),GIMP_TOOL_INFO(target));
    if(s.closed)return;
    s.schedule_tools();focus_selected();
  }
}); }
void group_expanded (GObject*widget,GParamSpec*,gpointer data) { dispatch(data,[&](Canvas&s,GObject*target,const std::string&){
  if(target) {
    const auto revision=++s.tool_revision;
    bool expanded=gtk_expander_get_expanded(GTK_EXPANDER(widget));
    gimp_viewable_set_expanded(GIMP_VIEWABLE(target),expanded);
    if(s.closed || s.tool_revision!=revision)return;
    if(expanded) {
      auto *active=gimp_tool_group_get_active_tool_info(GIMP_TOOL_GROUP(target));
      if(active)gimp_context_set_tool(s.ctx(),active);
      if(s.closed)return;
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
    if(!s.painting && !s.closed && gimp_overlay_child_find(s.overlay(),widget))gimp_overlay_box_set_child_opacity(s.overlay(),widget,1.);
  });return FALSE;
}
gboolean leave (GtkWidget*widget,GdkEventCrossing*,gpointer data) {
  dispatch(data,[&](Canvas&s,GObject*,const std::string&){
    if(!s.painting && !s.closed && gimp_overlay_child_find(s.overlay(),widget)) for(auto&panel:s.panel_snapshot())if(panel.frame.get()==G_OBJECT(widget))gimp_overlay_box_set_child_opacity(s.overlay(),widget,panel.opacity);
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
  auto layer_owner = LayerTileViewRef::create ();
  layers = GTK_WIDGET (layer_owner.get ());
  g_object_set (layers, "context", ctx (), "image", image (), nullptr);
  gimp_painter_layer_tiles_set_display(GIMP_PAINTER_LAYER_TILES(layers),shell->display);
  if(!frame(*this,layers))return;
  connect(*this,signals,G_OBJECT(layers),"popup-closed",G_CALLBACK(popup_closed));
  /* Legacy uses the native notebook directly, not a dock editor with a
   * second foreground/background editor or channel controls around it. */
  colors=gimp_color_selector_new(GIMP_TYPE_COLOR_NOTEBOOK,
                                gimp_context_get_foreground(ctx()),
                                GIMP_COLOR_SELECTOR_HUE);
  gtk_widget_set_name(colors,"painter-color-editor");
  auto *notebook=GIMP_COLOR_NOTEBOOK(colors);
  auto *pages=GTK_NOTEBOOK(gimp_color_notebook_get_notebook(notebook));
  gimp_color_selector_set_show_alpha(GIMP_COLOR_SELECTOR(colors),FALSE);
  gimp_color_selector_set_config(GIMP_COLOR_SELECTOR(colors),ctx()->gimp->config->color_management);
  for(GList *i=gimp_color_notebook_get_selectors(notebook);i;i=i->next)
    if(!strcmp(G_OBJECT_TYPE_NAME(i->data),"ColorselWheel"))
      gtk_notebook_set_current_page(pages,gtk_notebook_page_num(pages,GTK_WIDGET(i->data)));
  gtk_widget_set_size_request(colors,256,256);
  if(!frame(*this,colors,1.))return;
  connect(*this,signals,G_OBJECT(colors),"color-changed",G_CALLBACK(selector_color_changed));
  connect(*this,signals,G_OBJECT(ctx()),"foreground-changed",G_CALLBACK(context_color_changed));
  connect(*this,signals,G_OBJECT(ctx()),"background-changed",G_CALLBACK(context_color_changed));
  connect(*this,signals,G_OBJECT(gimp_display_shell_get_color_config(shell)),"notify::mode",G_CALLBACK(color_mode_changed));
  GtkWidget *toolscroll=gtk_scrolled_window_new(nullptr,nullptr);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(toolscroll),GTK_POLICY_NEVER,GTK_POLICY_AUTOMATIC);
  tools=gtk_box_new(GTK_ORIENTATION_VERTICAL,0);gtk_container_add(GTK_CONTAINER(toolscroll),tools);if(!frame(*this,toolscroll))return;
  bar=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,3);
  GtkWidget *bar_scroll=gtk_scrolled_window_new(nullptr,nullptr);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(bar_scroll),GTK_POLICY_AUTOMATIC,GTK_POLICY_NEVER);
  gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(bar_scroll),48);
  gtk_container_add(GTK_CONTAINER(bar_scroll),bar);if(!frame(*this,bar_scroll))return;
  swatches=gimp_fg_bg_editor_new(ctx());gtk_widget_set_name(swatches,"painter-fg-bg");
  gtk_widget_set_size_request(swatches,80,48);
  if(!frame(*this,swatches,1.))return;
  connect(*this,signals,G_OBJECT(swatches),"notify::active-color",G_CALLBACK(active_color));
  sync_color();
  if(closed)return;
  color_image_signals.clear();
  if(image())for(const char *name:{"simulation-profile-changed","simulation-intent-changed","simulation-bpc-changed","profile-changed","precision-changed"})
    connect(*this,color_image_signals,G_OBJECT(image()),name,G_CALLBACK(color_image_changed));
  if(image())connect(*this,color_image_signals,G_OBJECT(image()),"notify::base-type",G_CALLBACK(color_mode_changed));
  sync_color_image();
  if(closed)return;
  for(auto*w:{layers,colors,tools,bar,swatches}) roots.push_back(ObjectRef<GObject>::retain(G_OBJECT(w)));
  rebuild_tools();
}
void Canvas::close () noexcept {
  if(closed)return;
  closed=true;refresh_tools.close();refresh_options.close();refresh_layout.close();signals.clear();controls.clear();tool_signals.clear();color_image_signals.clear();popup_connection.close();
  if(popup){gtk_widget_destroy(GTK_WIDGET(popup.get()));popup.reset();}
  detach();detach(true);
  toolbar_gui.reset();toolbar_options.reset();
  if (swatches) gimp_fg_bg_editor_set_context(GIMP_FG_BG_EDITOR(swatches),nullptr);
  auto old_panels=std::move(panels);
  for(auto&panel:old_panels)gtk_widget_destroy(GTK_WIDGET(panel.frame.get()));
  layers=colors=tools=bar=swatches=nullptr;
}
void Canvas::show (bool value) {
  if(closed)return;
  const auto presentation=++presentation_revision;
  if(value && !visible && !options_presentation_pending)options_presentation=1;
  options_presentation_pending=false;
  visible=value;
  if(value)create();
  if(closed || presentation_revision!=presentation)return;
  if(value && image())update_toolbar();
  if(closed || presentation_revision!=presentation)return;
  std::vector<ObjectRef<GObject>> snapshot;
  for(auto&panel:panels)snapshot.push_back(panel.frame);
  for(auto&panel:snapshot){if(closed || presentation_revision!=presentation)return;auto*w=GTK_WIDGET(panel.get());if(value&&image())gtk_widget_show_all(w);else gtk_widget_hide(w);}
  /* show_all would otherwise reveal GimpEditor's deliberately hidden name.
   * roots can retain the editor after destroy removed it from its frame;
   * that lease does not preserve the editor's private child widgets. */
  if(layers && gtk_widget_get_parent(layers))gimp_editor_set_show_name(GIMP_EDITOR(layers),FALSE);
  if(bar && panels.size()>3)gtk_widget_set_visible(GTK_WIDGET(panels[3].frame.get()),value && image() && options_presentation && bool(toolbar_gui));
  if(!value) {
    refresh_layout.close();refresh_options.close();
    if(layers)gimp_painter_layer_tiles_cancel_interaction(GIMP_PAINTER_LAYER_TILES(layers));
    if(closed || presentation_revision!=presentation)return;
    painting=false;pointer(0,0,true);
    if(closed || presentation_revision!=presentation)return;
    if(popup){auto old=std::move(popup);gtk_widget_destroy(GTK_WIDGET(old.get()));}
    if(closed || presentation_revision!=presentation)return;
    detach();
    if(closed || presentation_revision!=presentation)return;
    detach(true);
  }
  /* Only the native global preference drives the toggle action. Updating it
   * from transient/inactive-shell visibility can synchronously disable every
   * canvas while switching to a previously hidden tab. */
  if(!closed && presentation_revision==presentation)layout();
}
void Canvas::schedule_layout () {
  if(closed || !visible || refresh_layout.active())return;
  auto weak=std::make_shared<WeakRef<GObject>>(ObjectRef<GObject>::retain(G_OBJECT(shell)));
  auto generation=BindingStore::require(G_OBJECT(shell)).generation();
  refresh_layout=Source::idle(nullptr,G_PRIORITY_DEFAULT_IDLE,[weak,generation](){
    auto owner=weak->lock();if(owner) {
      auto*store=BindingStore::find(owner.get());
      if(store && store->accepts(generation))store->with<Slot>([](Canvas&s){
        s.refresh_layout.close();
        if(s.closed || !s.visible)return;
        /* GTK discards resize requests made inside size-allocate. Compute
         * positions only after that stack has unwound, coalescing its signals. */
        s.layout();
        if(s.closed || !s.visible)return;
        /* A prior request may already hold the desired coordinates, making
         * the position setter a no-op. Requeue allocation only while one of
         * our visible children still has an unapplied position. */
        for(auto&panel:s.panel_snapshot()) {
          auto*widget=GTK_WIDGET(panel.frame.get());
          auto*child=gimp_overlay_child_find(s.overlay(),widget);
          if(child && child->has_position && gtk_widget_get_visible(widget) &&
             (child->matrix.x0!=static_cast<gint>(child->x) ||
              child->matrix.y0!=static_cast<gint>(child->y))) {
            gtk_widget_queue_allocate(s.shell->canvas);
            break;
          }
        }
      });
    }
    return false;
  });
}
void Canvas::layout () {
  if(positioning||!visible||panels.size()<5)return;
  positioning=true;
  auto fixed_panels=panels;
  const int w=gtk_widget_get_allocated_width(shell->canvas),h=gtk_widget_get_allocated_height(shell->canvas);
  const bool portrait=w<h;
  GtkWidget *window=gtk_widget_get_toplevel(GTK_WIDGET(shell));
  const bool horizontal_docks=gtk_widget_get_allocated_width(window)<=gtk_widget_get_allocated_height(window);
  auto place=[&](int index,int x,int y,int width,int height){
    if(closed)return;
    auto*widget=GTK_WIDGET(fixed_panels[index].frame.get());
    gtk_widget_set_size_request(widget,width,height);
    if(!closed)gimp_overlay_box_set_child_position(overlay(),widget,x,y);
  };
  /* Geometry follows afa43fae gimpdisplayshell-overlays.cpp. The notebook,
   * canvas and dock orientation are each based on their own allocation. */
  const int layer_width=88;
  const int offset=portrait?256:0;
  const int layer_height=std::max(1,(h-offset)*8/10);
  place(0,w-layer_width+4,offset+(h-offset-layer_height)/2,layer_width,layer_height);
  const int color_width=std::max(256,gtk_widget_get_allocated_width(GTK_WIDGET(fixed_panels[1].frame.get())));
  const int color_height=std::max(256,gtk_widget_get_allocated_height(GTK_WIDGET(fixed_panels[1].frame.get())));
  const int fg_width=std::max(84,gtk_widget_get_allocated_width(GTK_WIDGET(fixed_panels[4].frame.get())));
  place(1,w-color_width-fg_width,0,256,256);
  place(4,w-84,0,80,48);
  gint tool_min=0,tool_natural=0;
  gtk_widget_get_preferred_height(tools,&tool_min,&tool_natural);
  const int tools_height=std::max(1,std::min(h,tool_natural+8));
  place(2,-4,(h-tools_height)/2,48,tools_height);
  const int bh=std::max(48,gtk_widget_get_allocated_height(GTK_WIDGET(fixed_panels[3].frame.get())));
  int bw=std::max(1,w*98/100),bx=(w-bw)/2;
  auto right=right_dock;
  if(right && right->panel.frame) {
    auto*host=GTK_WIDGET(right->panel.frame.get());
    if(GIMP_IS_DOCK_COLUMNS(right->widget.get())) {
      std::vector<ObjectRef<GObject>> docks;
      for(GList *i=gimp_dock_columns_get_docks(GIMP_DOCK_COLUMNS(right->widget.get()));i;i=i->next)
        docks.push_back(ObjectRef<GObject>::retain(G_OBJECT(i->data)));
      for(auto& dock:docks) {
        if(!gtk_widget_is_ancestor(GTK_WIDGET(dock.get()),GTK_WIDGET(right->widget.get())))continue;
        gtk_orientable_set_orientation(GTK_ORIENTABLE(dock.get()),horizontal_docks?GTK_ORIENTATION_HORIZONTAL:GTK_ORIENTATION_VERTICAL);
        if(closed || right_dock!=right){positioning=false;return;}
      }
    }
    const int rx=portrait?0:w-layer_width-color_width-4;
    const int ry=portrait?0:color_height+4;
    gimp_overlay_box_set_child_position(overlay(),host,rx,ry);
    if(closed || right_dock!=right){positioning=false;return;}
    gtk_widget_set_size_request(host,portrait?std::max(1,w-color_width-fg_width-4):color_width,
                               portrait?color_height:std::max(1,h-ry));
    if(!portrait && rx<bx+bw){bw=std::max(1,rx-bx-4);bx=(rx-bw)/2;}
  }
  place(3,bx,h-bh+4,bw,-1);
  positioning=false;
}
void Canvas::sync_color () {
  if(closed || syncing_colors || !colors || !swatches)return;
  syncing_colors=true;
  GeglColor *color=GIMP_FG_BG_EDITOR(swatches)->active_color==GIMP_ACTIVE_COLOR_BACKGROUND
    ?gimp_context_get_background(ctx()):gimp_context_get_foreground(ctx());
  gimp_color_selector_set_color(GIMP_COLOR_SELECTOR(colors),color);
  syncing_colors=false;
}
void Canvas::sync_color_image () {
  if(closed || !colors)return;
  auto image_ref=ObjectRef<GObject>::retain(image()?G_OBJECT(image()):nullptr);
  auto notebook_ref=ObjectRef<GObject>::retain(G_OBJECT(colors));
  auto *im=image_ref?GIMP_IMAGE(image_ref.get()):nullptr;
  auto *notebook=GIMP_COLOR_NOTEBOOK(notebook_ref.get());
  auto current=[&](){return !closed && image()==im && colors==GTK_WIDGET(notebook);};
  gimp_color_notebook_set_format(notebook,im?gimp_image_get_layer_format(im,FALSE):nullptr);
  if(!current())return;
  gimp_color_notebook_set_simulation(notebook,im?gimp_image_get_simulation_profile(im):nullptr,
    im?gimp_image_get_simulation_intent(im):GIMP_COLOR_RENDERING_INTENT_RELATIVE_COLORIMETRIC,
    im?gimp_image_get_simulation_bpc(im):FALSE);
  if(!current())return;
  gimp_color_notebook_enable_simulation(notebook,im &&
    gimp_color_config_get_mode(gimp_display_shell_get_color_config(shell))==GIMP_COLOR_MANAGEMENT_SOFTPROOF);
}
bool Canvas::update_toolbar () {
  if(closed || !bar)return false;
  auto *tool=gimp_context_get_tool(ctx());
  auto *options=tool?tool->tool_options:nullptr;
  if(toolbar_gui && toolbar_options.get()==G_OBJECT(options)) {
    if(panels.size()>3)gtk_widget_set_visible(GTK_WIDGET(panels[3].frame.get()),visible && image() && options_presentation);
    return true;
  }
  const auto revision=tool_revision;
  auto old=std::move(toolbar_gui);toolbar_options.reset();
  if(old)gtk_widget_destroy(GTK_WIDGET(old.get()));
  if(closed || tool_revision!=revision)return false;
  GtkWidget *created=options?gimp_tools_create_painter_toolbar_gui(options):nullptr;
  if(!created) {
    if(panels.size()>3)gtk_widget_hide(GTK_WIDGET(panels[3].frame.get()));
    return false;
  }
  auto gui=ObjectRef<GObject>::sink(G_OBJECT(created));
  auto current=[&](){return !closed && revision==tool_revision && gimp_context_get_tool(ctx())==tool;};
  // The registered horizontal factory already built the independent toolbar.
  if(!current()){gtk_widget_destroy(created);return false;}
  toolbar_gui=gui;toolbar_options=ObjectRef<GObject>::retain(G_OBJECT(options));
  gtk_box_pack_start(GTK_BOX(bar),created,TRUE,TRUE,0);
  if(!current())return false;
  gtk_widget_show_all(bar);
  if(panels.size()>3)gtk_widget_set_visible(GTK_WIDGET(panels[3].frame.get()),visible && image() && options_presentation);
  schedule_layout();return true;
}
void Canvas::reconnect () {
  color_image_signals.clear();
  if(image())for(const char *name:{"simulation-profile-changed","simulation-intent-changed","simulation-bpc-changed","profile-changed","precision-changed"})
    connect(*this,color_image_signals,G_OBJECT(image()),name,G_CALLBACK(color_image_changed));
  if(image())connect(*this,color_image_signals,G_OBJECT(image()),"notify::base-type",G_CALLBACK(color_mode_changed));
  sync_color_image();
  if(closed)return;
  painting=false;pointer(0,0,true);
  if (!image()) {detach();detach(true);}
  if(layers)gimp_painter_layer_tiles_set_image(GIMP_PAINTER_LAYER_TILES(layers),image());
  show(visible);
}
std::vector<Panel> Canvas::panel_snapshot () const {
  auto result=panels;
  if(options_dock && options_dock->panel.frame)result.push_back(options_dock->panel);
  if(right_dock && right_dock->panel.frame)result.push_back(right_dock->panel);
  return result;
}
void Canvas::pointer (double x,double y,bool released) {
  painting=!released;
  for(auto&panel:panel_snapshot()) {
    if(closed)return;
    auto*w=GTK_WIDGET(panel.frame.get());auto*child=gimp_overlay_child_find(overlay(),w);if(!child)continue;
    if(released) {
      gimp_overlay_box_set_child_opacity(overlay(),w,panel.opacity);
      if(!closed && gimp_overlay_child_find(overlay(),w))gimp_overlay_box_set_child_input_pass_through(overlay(),w,FALSE);
    } else {
      const int width=gtk_widget_get_allocated_width(w),height=gtk_widget_get_allocated_height(w);
      double dx=std::max({child->x-x,0.,x-child->x-width}),dy=std::max({child->y-y,0.,y-child->y-height});
      if(dx*dx+dy*dy<200.*200.) {
        gimp_overlay_box_set_child_opacity(overlay(),w,.1);
        if(!closed && gimp_overlay_child_find(overlay(),w))gimp_overlay_box_set_child_input_pass_through(overlay(),w,TRUE);
      }
    }
  }
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
  if(visible && image())update_toolbar();
  if (options_dock && options_dock->options) {
    auto *tool = gimp_context_get_tool(ctx());
    if (tool && G_OBJECT(tool->tool_options) != options_dock->options.get()) {
      /* The protected options transaction restores the previous GUI itself.
       * Never snapshot a mode, detach with callbacks, then replay that stale
       * mode over a newer full/compact/hidden request. */
      if(!closed && visible && options_presentation && gimp_context_get_display(ctx())==shell->display)
        attach_options(options_presentation==1);
      else detach();
    }
  }
  layout();
}
void borrowed_parent_destroyed (GtkWidget*,gpointer data) {
  static_cast<Borrow*>(data)->parent_alive=false;
}
void borrowed_widget_destroyed (GtkWidget*,gpointer data) {
  static_cast<Borrow*>(data)->alive=false;
}
void borrowed_grab_changed (GtkWidget*,gboolean,gpointer data) {
  dispatch(data,[](Canvas&s,GObject*,const std::string&){s.painting=false;s.pointer(0,0,true);});
}
/* Compact presentation rejects a nested mutation while it reparents native
 * controls. A cancelled borrow may already be home when that outer operation
 * returns; finish its restore then, without overriding a newer Canvas owner. */
void Canvas::finish_compact_restore (const std::shared_ptr<Borrow>& borrowed) noexcept {
  if(!borrowed || !borrowed->compact_restore_pending || !borrowed->alive || !borrowed->options)return;
  auto*widget=GTK_WIDGET(borrowed->widget.get());
  bool claimed=false;
  if(auto*ancestor=gtk_widget_get_ancestor(widget,GIMP_TYPE_DISPLAY_SHELL))
    use(G_OBJECT(ancestor),[&](Canvas&owner){
      if(owner.options_dock && owner.options_dock!=borrowed &&
         owner.options_dock->widget.get()==G_OBJECT(widget)) {
        claimed=true;
        owner.schedule_options();
      }
    });
  if(claimed || gimp_painter_compact_options_set(widget,GIMP_TOOL_OPTIONS(borrowed->options.get()),FALSE,GTK_ORIENTATION_HORIZONTAL,nullptr)) {
    borrowed->compact_restore_pending=false;
    borrowed->compact=false;
  }
}
/* State is removed from its slot before GTK can reenter.  The local record
 * keeps both the widget and lifetime flags valid until restoration finishes. */
void Canvas::restore (const std::shared_ptr<Borrow>& borrowed) noexcept {
  if(!borrowed || borrowed->released)return;
  borrowed->released=true;
  auto parent=borrowed->parent.lock();
  auto*widget=GTK_WIDGET(borrowed->widget.get());
  auto*host=GTK_WIDGET(borrowed->panel.frame.get());
  if(borrowed->compact && borrowed->alive && borrowed->options) {
    borrowed->compact_restore_pending=!gimp_painter_compact_options_set(widget,GIMP_TOOL_OPTIONS(borrowed->options.get()),FALSE,GTK_ORIENTATION_HORIZONTAL,nullptr);
    if(!borrowed->compact_restore_pending)borrowed->compact=false;
  }
  if(borrowed->alive) {
    auto*current=gtk_widget_get_parent(widget);
    /* A newer owner may already have claimed the widget during a callback.
     * Only remove this record's host, never that owner's parent. */
    if(current && host && gtk_widget_is_ancestor(widget,host)) {
      gtk_container_remove(GTK_CONTAINER(current),widget);
      current=gtk_widget_get_parent(widget);
    }
    if(!current && borrowed->alive && borrowed->parent_alive && parent &&
       !gtk_widget_in_destruction(GTK_WIDGET(parent.get()))) {
      auto*p=GTK_WIDGET(parent.get());
      switch(borrowed->home) {
        case Borrow::Home::box:
          if(borrowed->pack==GTK_PACK_END)gtk_box_pack_end(GTK_BOX(p),widget,borrowed->expand,borrowed->fill,borrowed->padding);
          else gtk_box_pack_start(GTK_BOX(p),widget,borrowed->expand,borrowed->fill,borrowed->padding);
          if(borrowed->alive && borrowed->parent_alive && gtk_widget_get_parent(widget)==p && borrowed->position>=0)
            gtk_box_reorder_child(GTK_BOX(p),widget,borrowed->position);
          break;
        case Borrow::Home::paned1:
          if(!gtk_paned_get_child1(GTK_PANED(p)))gtk_paned_pack1(GTK_PANED(p),widget,borrowed->resize,borrowed->shrink);
          break;
        case Borrow::Home::paned2:
          if(!gtk_paned_get_child2(GTK_PANED(p)))gtk_paned_pack2(GTK_PANED(p),widget,borrowed->resize,borrowed->shrink);
          break;
        case Borrow::Home::container:
          if(!GTK_IS_BIN(p) || !gtk_bin_get_child(GTK_BIN(p)))gtk_container_add(GTK_CONTAINER(p),widget);
          break;
        case Borrow::Home::none:break;
      }
    }
    if(borrowed->alive && borrowed->parent_alive && parent && gtk_widget_get_parent(widget)==GTK_WIDGET(parent.get())) {
      if(GTK_IS_PANED(parent.get())) {
        auto*p=GTK_PANED(parent.get());
        const int span=gtk_orientable_get_orientation(GTK_ORIENTABLE(p))==GTK_ORIENTATION_HORIZONTAL?
          gtk_widget_get_allocated_width(GTK_WIDGET(p)):gtk_widget_get_allocated_height(GTK_WIDGET(p));
        const int position=span==borrowed->pane_span?borrowed->pane_position:
          (borrowed->home==Borrow::Home::paned1?borrowed->pane_width:std::max(0,span-borrowed->pane_width));
        gtk_paned_set_position(p,position);
        if(borrowed->parent_alive && gtk_widget_get_parent(widget)==GTK_WIDGET(p))
          g_object_set(p,"position-set",borrowed->position_set,nullptr);
      }
      if(borrowed->alive && borrowed->parent_alive && gtk_widget_get_parent(widget)==GTK_WIDGET(parent.get()))
        gtk_widget_set_visible(widget,borrowed->was_visible);
    }
    if(borrowed->alive && !gtk_widget_get_parent(widget) && borrowed->right &&
       borrowed->parent_alive && parent && !closed && !right_dock) {
      /* A callback filled our home slot. Preserve the original record and
       * keep a hidden, parented recovery host; never remove the occupant. */
      if(!borrowed->panel.frame)
        borrowed->panel.frame=ObjectRef<GObject>::sink(G_OBJECT(gimp_overlay_frame_new()));
      host=GTK_WIDGET(borrowed->panel.frame.get());
      if(borrowed->signals.empty()) {
        gtk_widget_set_name(host,"painter-right-docks");
        gtk_container_set_border_width(GTK_CONTAINER(host),4);
        gtk_widget_add_events(host,GDK_ENTER_NOTIFY_MASK|GDK_LEAVE_NOTIFY_MASK);
        connect(*this,borrowed->signals,G_OBJECT(host),"size-allocate",G_CALLBACK(resized));
        connect(*this,borrowed->signals,G_OBJECT(host),"enter-notify-event",G_CALLBACK(enter));
        connect(*this,borrowed->signals,G_OBJECT(host),"leave-notify-event",G_CALLBACK(leave));
        connect(*this,borrowed->signals,G_OBJECT(host),"grab-notify",G_CALLBACK(borrowed_grab_changed));
      }
      auto*container=gtk_bin_get_child(GTK_BIN(host));
      if(!container) {
        container=gtk_scrolled_window_new(nullptr,nullptr);
        gtk_container_add(GTK_CONTAINER(host),container);
      }
      /* Removing the child leaves GtkScrolledWindow's viewport installed.
       * Restore into that empty viewport rather than adding a second child
       * to GtkScrolledWindow (which GTK correctly rejects). */
      if(GTK_IS_SCROLLED_WINDOW(container)) {
        auto*viewport=gtk_bin_get_child(GTK_BIN(container));
        if(GTK_IS_VIEWPORT(viewport) && !gtk_bin_get_child(GTK_BIN(viewport)))container=viewport;
      }
      borrowed->released=false;
      right_dock=borrowed;
      gtk_container_add(GTK_CONTAINER(container),widget);
      if(!closed && right_dock==borrowed && borrowed->alive && !gtk_widget_get_parent(host))
        gimp_overlay_box_add_child(overlay(),host,0,0);
      if(!closed && right_dock==borrowed && borrowed->alive && gtk_widget_get_parent(widget)) {
        gimp_overlay_box_set_child_opacity(overlay(),host,borrowed->panel.opacity);
        gtk_widget_hide(host);
        return;
      }
      /* A synchronous close/new owner now owns the terminal cleanup. */
      if(right_dock!=borrowed)return;
      right_dock.reset();borrowed->released=true;
    }
    /* Parent destruction is terminal for window columns.  Generic/options
     * callers may have a genuinely parentless model-owned canonical GUI. */
    if(borrowed->alive && !gtk_widget_get_parent(widget)) {
      if(borrowed->right)gtk_widget_destroy(widget);
      else gtk_widget_hide(widget);
    }
  }
  if(host)gtk_widget_destroy(host);
}
void Canvas::detach (bool right) noexcept {
  auto&slot=right?right_dock:options_dock;
  auto&revision=right?right_revision:options_revision;
  ++revision;
  auto old=std::move(slot);
  if(right && old && GIMP_IS_DOCK_COLUMNS(old->widget.get())) {
    std::vector<ObjectRef<GObject>> docks;
    for(GList *i=gimp_dock_columns_get_docks(GIMP_DOCK_COLUMNS(old->widget.get()));i;i=i->next)
      docks.push_back(ObjectRef<GObject>::retain(G_OBJECT(i->data)));
    for(auto& dock:docks) {
      if(right_dock)break; /* Reentrant attachment owns orientation now. */
      if(gtk_widget_is_ancestor(GTK_WIDGET(dock.get()),GTK_WIDGET(old->widget.get())))
        gtk_orientable_set_orientation(GTK_ORIENTABLE(dock.get()),GTK_ORIENTATION_VERTICAL);
    }
  }
  restore(old);
}
bool Canvas::attach (GtkWidget*widget,bool right,guint64 window_revision,bool from_options,bool replace) {
  if(!widget || widget==shell->canvas || closed)return false;
  auto&slot=right?right_dock:options_dock;
  auto&busy=right?right_busy:options_busy;
  if(busy && !from_options)return false;
  const auto intent=options_intent;
  const auto presentation=presentation_revision;
  auto*window=gimp_display_shell_get_window(shell);
  auto window_lease=ObjectRef<GObject>::retain(G_OBJECT(window));
  auto eligible=[&](){return !closed && (right ? (window &&
    gimp_image_window_painter_can_attach(window,shell,window_revision)) :
    (options_intent==intent && presentation_revision==presentation));};
  if(!eligible())return false;
  if(!replace && slot && slot->widget.get()==G_OBJECT(widget) && slot->alive && !slot->released) {
    auto same=slot;
    for(auto*part=widget;part && part!=shell->canvas;part=gtk_widget_get_parent(part)) {
      gtk_widget_show(part);
      if(!eligible() || slot!=same || !same->alive)return false;
    }
    layout();return eligible() && slot==same && same->alive;
  }
  auto widget_lease=ObjectRef<GObject>::retain(G_OBJECT(widget));
  /* Observe the target before restoring any other record: restoration can
   * destroy a retained GtkWidget without finalizing its GObject storage. */
  auto record=std::make_shared<Borrow>();
  record->widget=widget_lease;record->right=right;
  record->widget_destroy=Connection::connect(widget_lease,"destroy",G_CALLBACK(borrowed_widget_destroyed),record.get(),nullptr);
  struct Busy { bool&value;bool previous;explicit Busy(bool&v):value(v),previous(v){value=true;}~Busy(){value=previous;} } guard(busy);
  detach(right);
  if(!eligible() || !record->alive)return false;
  /* Restore through the former slot first; an overlay/viewport is never a
   * new logical home for a shared canonical widget. */
  if(auto*ancestor=gtk_widget_get_ancestor(widget,GIMP_TYPE_DISPLAY_SHELL)) {
    auto*other=GIMP_DISPLAY_SHELL(ancestor);
    use(G_OBJECT(other),[&](Canvas&s){
      auto&previous=right?s.right_dock:s.options_dock;
      if(previous && previous->widget.get()==G_OBJECT(widget))s.detach(right);
    });
    if(!eligible() || !record->alive)return false;
  }
  auto*parent=gtk_widget_get_parent(widget);
  auto parent_lease=ObjectRef<GObject>::retain(G_OBJECT(parent));
  if(parent && gtk_widget_get_ancestor(widget,GIMP_TYPE_CANVAS))return false;
  if(right && (!parent || !GTK_IS_PANED(parent)))return false;
  record->was_visible=gtk_widget_get_visible(widget);
  if(parent) {
    record->parent=WeakRef<GObject>(ObjectRef<GObject>::retain(G_OBJECT(parent)));
    record->parent_destroy=Connection::connect(ObjectRef<GObject>::retain(G_OBJECT(parent)),"destroy",G_CALLBACK(borrowed_parent_destroyed),record.get(),nullptr);
    if(GTK_IS_BOX(parent)) {
      record->home=Borrow::Home::box;
      gtk_container_child_get(GTK_CONTAINER(parent),widget,"position",&record->position,nullptr);
      gtk_box_query_child_packing(GTK_BOX(parent),widget,&record->expand,&record->fill,&record->padding,&record->pack);
    } else if(GTK_IS_PANED(parent)) {
      record->home=gtk_paned_get_child1(GTK_PANED(parent))==widget?Borrow::Home::paned1:Borrow::Home::paned2;
      gtk_container_child_get(GTK_CONTAINER(parent),widget,"resize",&record->resize,"shrink",&record->shrink,nullptr);
      record->pane_position=gtk_paned_get_position(GTK_PANED(parent));
      record->pane_span=gtk_orientable_get_orientation(GTK_ORIENTABLE(parent))==GTK_ORIENTATION_HORIZONTAL?
        gtk_widget_get_allocated_width(parent):gtk_widget_get_allocated_height(parent);
      record->pane_width=record->home==Borrow::Home::paned1?record->pane_position:
        std::max(0,record->pane_span-record->pane_position);
      g_object_get(parent,"position-set",&record->position_set,nullptr);
      if(right && window)gimp_image_window_get_right_docks_geometry(window,
        &record->pane_width,&record->pane_position,&record->pane_span,&record->position_set);
    } else record->home=Borrow::Home::container;
  }
  auto&epoch=right?right_revision:options_revision;
  const auto operation=++epoch;
  slot=record;
  auto valid=[&](){return eligible() && slot==record && epoch==operation && record->alive && record->parent_alive && !record->released;};
  auto abort=[&](){if(slot==record)detach(right);return false;};
  if(parent)gtk_container_remove(GTK_CONTAINER(parent),widget);
  if(!valid())return abort();
  /* Do not steal a widget which a parent-set handler moved elsewhere. */
  if(gtk_widget_get_parent(widget))return abort();
  record->panel.frame=ObjectRef<GObject>::sink(G_OBJECT(gimp_overlay_frame_new()));
  auto*host=GTK_WIDGET(record->panel.frame.get());
  gtk_widget_set_name(host,right?"painter-right-docks":"painter-options-host");
  gtk_container_set_border_width(GTK_CONTAINER(host),4);
  gtk_widget_add_events(host,GDK_ENTER_NOTIFY_MASK|GDK_LEAVE_NOTIFY_MASK);
  connect(*this,record->signals,G_OBJECT(host),"size-allocate",G_CALLBACK(resized));
  connect(*this,record->signals,G_OBJECT(host),"enter-notify-event",G_CALLBACK(enter));
  connect(*this,record->signals,G_OBJECT(host),"leave-notify-event",G_CALLBACK(leave));
  connect(*this,record->signals,G_OBJECT(host),"grab-notify",G_CALLBACK(borrowed_grab_changed));
  auto scroller=ObjectRef<GObject>::sink(G_OBJECT(gtk_scrolled_window_new(nullptr,nullptr)));
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroller.get()),GTK_POLICY_AUTOMATIC,GTK_POLICY_AUTOMATIC);
  gtk_container_add(GTK_CONTAINER(host),GTK_WIDGET(scroller.get()));
  if(!valid())return abort();
  gtk_container_add(GTK_CONTAINER(scroller.get()),widget);
  if(!valid())return abort();
  gimp_overlay_box_add_child(overlay(),host,0,0);
  if(!valid())return abort();
  gimp_overlay_box_set_child_opacity(overlay(),host,.85);
  if(!valid())return abort();
  /* Arbitrary native dockables deliberately hide some of their children. */
  gtk_widget_show(widget);
  if(!valid())return abort();
  if(auto*viewport=gtk_widget_get_parent(widget))gtk_widget_show(viewport);
  if(!valid())return abort();
  gtk_widget_show(GTK_WIDGET(scroller.get()));
  if(!valid())return abort();
  gtk_widget_show(host);
  if(!valid())return abort();
  layout();schedule_layout();return valid();
}
void Canvas::schedule_options () {
  if(closed || refresh_options.active())return;
  auto weak=std::make_shared<WeakRef<GObject>>(ObjectRef<GObject>::retain(G_OBJECT(shell)));
  auto generation=BindingStore::require(G_OBJECT(shell)).generation();
  refresh_options=Source::idle(nullptr,G_PRIORITY_DEFAULT_IDLE,[weak,generation](){
    auto owner=weak->lock();if(owner) {
      auto*store=BindingStore::find(owner.get());
      if(store && store->accepts(generation))store->with<Slot>([](Canvas&s){
        s.refresh_options.close();
        if(!s.visible || !s.options_presentation)s.detach();
        else if(s.image() && gimp_context_get_display(s.ctx())==s.shell->display)
          s.attach_options(s.options_presentation==1);
      });
    }
    return false;
  });
}
bool Canvas::attach_options (bool compact,bool preserve_transfer) {
  (void)preserve_transfer;
  if(closed)return false;
  if(compact){options_presentation=1;return update_toolbar();}
  gimp_window_strategy_show_dockable_dialog(GIMP_WINDOW_STRATEGY(gimp_get_window_strategy(ctx()->gimp)),
    ctx()->gimp,gimp_dialog_factory_get_singleton(),gimp_widget_get_monitor(GTK_WIDGET(shell)),"gimp-tool-options");
  return !closed;
}
void Canvas::action (const std::string&name) {
  if(name=="painter-canvas-hide") {
    g_object_set(ctx()->gimp->config,"painter-canvas-ui",FALSE,nullptr);
    /* Config callbacks may synchronously supersede this request. */
    if(!closed) {
      if(auto*window=gimp_display_shell_get_window(shell))gimp_image_window_painter_reconcile(window);
      /* A transient helper presentation may never have opted its native
       * window in. Hide it only if this is still the current global intent. */
      if(!closed && !ctx()->gimp->config->painter_canvas_ui)show(false);
    }
    return;
  }
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
  }else if(name=="painter-canvas-options" || name=="painter-canvas-compact") {
    const bool compact=name=="painter-canvas-compact";
    if(compact && options_presentation && toolbar_gui) {
      options_presentation=0;++options_intent;
      if(panels.size()>3)gtk_widget_hide(GTK_WIDGET(panels[3].frame.get()));
      return;
    }
    options_presentation=compact?1:2;
    attach_options(compact);
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
      s.show(s.ctx()->gimp->config->painter_canvas_ui);
    });
  }catch(const std::exception&e){g_warning("Cannot initialize painter canvas: %s",e.what());}
}
void gimp_painter_canvas_ui_close (GimpDisplayShell*shell) {try{if(auto*store=BindingStore::find(G_OBJECT(shell)))store->close();}catch(...) {}}
void gimp_painter_canvas_ui_set_visible (GimpDisplayShell*shell,gboolean value) {use(G_OBJECT(shell),[&](Canvas&s){s.show(value);});}
gboolean gimp_painter_canvas_ui_get_visible (GimpDisplayShell*shell) {bool result=false;use(G_OBJECT(shell),[&](Canvas&s){result=s.visible;});return result;}
void gimp_painter_canvas_ui_pointer (GimpDisplayShell*shell,gdouble x,gdouble y,gboolean released) {use(G_OBJECT(shell),[&](Canvas&s){s.pointer(x,y,released);});}
gboolean gimp_painter_canvas_ui_attach_dock (GimpDisplayShell*shell,GtkWidget*dock) {bool result=false;use(G_OBJECT(shell),[&](Canvas&s){++s.options_intent;result=s.attach(dock);});return result;}
void gimp_painter_canvas_ui_detach_dock (GimpDisplayShell*shell) {use(G_OBJECT(shell),[](Canvas&s){++s.options_intent;s.detach();});}

gboolean gimp_painter_canvas_ui_attach_right_docks (GimpDisplayShell*shell,GtkWidget*columns,guint64 revision) {
  bool result=false;use(G_OBJECT(shell),[&](Canvas&s){result=s.attach(columns,true,revision);});return result;
}
void gimp_painter_canvas_ui_detach_right_docks (GimpDisplayShell*shell) {
  use(G_OBJECT(shell),[](Canvas&s){s.detach(true);});
}
gboolean gimp_painter_canvas_ui_has_right_docks (GimpDisplayShell*shell) {
  bool result=false;use(G_OBJECT(shell),[&](Canvas&s){result=bool(s.right_dock);});return result;
}
gboolean gimp_painter_canvas_ui_get_right_docks_width (GimpDisplayShell*shell,gint*width) {
  bool result=false;use(G_OBJECT(shell),[&](Canvas&s){if(s.right_dock){if(width)*width=s.right_dock->pane_width;result=true;}});return result;
}
void gimp_painter_canvas_ui_set_right_docks_width (GimpDisplayShell*shell,gint width) {
  use(G_OBJECT(shell),[&](Canvas&s){if(auto borrowed=s.right_dock) {
    auto parent=borrowed->parent.lock();
    const int span=parent?gtk_widget_get_allocated_width(GTK_WIDGET(parent.get())):borrowed->pane_span;
    borrowed->pane_span=span;
    borrowed->pane_position=width>0?std::max(0,span-width):-width;
    borrowed->pane_width=width>0?width:std::max(0,span+width);
    borrowed->position_set=TRUE;
  }});
}

gint gimp_painter_canvas_ui_get_options_presentation (GimpDisplayShell*shell) {
  gint result=1;use(G_OBJECT(shell),[&](Canvas&s){result=s.options_presentation;});return result;
}
void gimp_painter_canvas_ui_set_options_presentation (GimpDisplayShell*shell,gint mode) {
  use(G_OBJECT(shell),[&](Canvas&s){s.options_presentation=CLAMP(mode,0,2);s.options_presentation_pending=true;});
}
