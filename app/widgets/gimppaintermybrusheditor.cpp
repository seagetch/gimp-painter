/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gegl.h>
#include <gtk/gtk.h>
extern "C" {
#include "libgimpbase/gimpbase.h"
#include "libgimpmath/gimpmath.h"
#include "libgimpwidgets/gimpwidgets.h"
#include "core/core-types.h"
#include "widgets-types.h"
#include "core/gimp.h"
#include "core/gimpcontext.h"
#include "core/gimpcontainer.h"
#include "core/gimpdatafactory.h"
#include "core/gimptag.h"
#include "core/gimptagged.h"
#include "core/gimpbrush.h"
#include "core/gimppattern.h"
#include "core/gimpcoords.h"
#include "gimpdocked.h"
#include "gimpviewablebox.h"
#include "gimp-intl.h"
}
#include "gimppaintermybrusheditor.hpp"
#include "core/gimppaintermybrush-handle.hpp"
#include "paint/painter-mypaint-surface/gimp-painter-options.hpp"
#include "paint/painter-mypaint-surface/gimp-resources.hpp"
#include "paint/painter-mypaint/engine.hpp"
#include "painter/binding-store.hpp"
#include "painter/connection.hpp"
#include "painter/source.hpp"
#include "painter/gimp-painter-binding.h"
#include "mypaintbrush-settings-data.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <string>
#include <vector>
using namespace GimpPainter;
namespace MP = GimpPainter::MyPaint;
static void docked_init (GimpDockedInterface *iface);
G_DEFINE_TYPE_WITH_CODE (GimpPainterMybrushEditor, gimp_painter_mybrush_editor, GIMP_TYPE_EDITOR,
                        G_IMPLEMENT_INTERFACE (GIMP_TYPE_DOCKED, docked_init))
namespace {
constexpr int preview_width = 256, preview_height = 256, preview_samples = 257;
enum { COL_RESOURCE, COL_ICON, COL_NAME, COL_TAGS, N_COLUMNS };
struct PreviewJob {
  ObjectRef<GObject> buffer;
  MP::GimpResources resources;
  MP::GeglSurface surface;
  MP::Engine engine;
  int sample = 0;
  PreviewJob (GimpContext *context, const MP::Resource& resource)
    : buffer (ObjectRef<GObject>::adopt (G_OBJECT (gegl_buffer_new (
        GEGL_RECTANGLE (0, 0, preview_width, preview_height), babl_format ("R'G'B'A u8"))))),
      resources (context, resource, MP::GimpResources::Purpose::Preview),
      surface (GEGL_BUFFER (buffer.get ())), engine (resource)
  {
    /* A fixed multicolour underpainting makes smudge, eraser, paper and stroke
     * opacity observable. Neither the image nor a brush pipe is advanced. */
    std::vector<guchar> pixels (preview_width * preview_height * 4);
    for (int y = 0; y < preview_height; ++y)
      for (int x = 0; x < preview_width; ++x) {
        auto *p = &pixels[(y * preview_width + x) * 4];
        p[0] = 255; p[1] = x < y ? 192 : 255;
        p[2] = x < y ? 192 : 255; p[3] = 255;
      }
    gegl_buffer_set (GEGL_BUFFER (buffer.get ()), GEGL_RECTANGLE (0, 0, preview_width, preview_height),
                     0, babl_format ("R'G'B'A u8"), pixels.data (), GEGL_AUTO_ROWSTRIDE);
    resources.attach (surface);
    surface.set_non_incremental (engine.non_incremental ());
    surface.set_stroke_opacity (engine.stroke_opacity ());
    surface.set_background (1, 1, 1);
    /* Match the pinned preview, not active painting's foreground startup.
     * Its temporary blue base is overwritten by all 45 copied mappings; speed
     * caches retain raw Brush-constructor defaults. Engine(resource) performs
     * precisely that copy-only setup. Its RNG state starts at zero. */
    engine.reset ();
    surface.begin_session ();
  }
  bool step () {
    /* Never publish a half-rendered image. Work is split between owner-context
     * iterations, so changing settings or closing cancels the stale generation. */
    for (int end = std::min (sample + 4, preview_samples); sample < end; ++sample) {
      GimpCoords coords = GIMP_COORDS_DEFAULT_VALUES;
      double interval = .001;
      coords.x = coords.y = 128;
      coords.pressure = 0;
      if (sample > 0) {
        const int index = sample - 1;
        const double theta = 2.0 * index * G_PI / 256.0;
        coords.x = 128 + std::cos(theta) * std::exp(.2*theta) * 30;
        coords.y = 128 + std::sin(theta) * std::exp(.2*theta) * 30;
        coords.pressure = (256.0-index)/256.0;
        interval = (index+2)/1000.0;
      }
      coords.xtilt = coords.ytilt = 0;
      resources.set_coords (coords);
      engine.stroke_to (surface, coords.x, coords.y, coords.pressure, 0, 0, interval);
    }
    if (sample < preview_samples) return true;
    surface.end_session (); return false;
  }
  ObjectRef<GObject> pixbuf () {
    auto pix = ObjectRef<GObject>::adopt (G_OBJECT (gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, preview_width, preview_height)));
    gegl_buffer_get (GEGL_BUFFER (buffer.get ()), GEGL_RECTANGLE (0,0,preview_width,preview_height), 1,
                     babl_format ("R'G'B'A u8"), gdk_pixbuf_get_pixels (GDK_PIXBUF (pix.get ())),
                     gdk_pixbuf_get_rowstride (GDK_PIXBUF (pix.get ())), GEGL_ABYSS_NONE);
    return pix;
  }
};
struct EditorImpl {
  GimpPainterMybrushEditor *owner; // owner contains the single slot
  ObjectRef<GObject> context;
  ObjectRef<GimpPainterMybrushOptions> options;
  std::vector<Connection> connections;
  Connection resource_changed[2];
  ObjectRef<GObject> watched_resource[2];
  std::vector<ObjectRef<GObject>> widgets; // keep destroyed controls alive across synchronous reentry
  Source preview_source;
  std::shared_ptr<PreviewJob> job;
  ObjectRef<GObject> preview;
  ObjectRef<GimpPainterMybrush> listed;
  GtkWidget *root = nullptr, *status = nullptr, *preview_image = nullptr, *search = nullptr,
    *tree = nullptr, *history = nullptr, *setting = nullptr, *input = nullptr, *graph = nullptr,
    *point = nullptr, *px = nullptr, *py = nullptr, *name = nullptr, *tags = nullptr,
    *save = nullptr, *rename = nullptr, *remove = nullptr;
  std::vector<GtkWidget *> numeric, switches, texts;
  GtkWidget *curve_enabled=nullptr;
  GtkWidget *ranges[4]{};
  int curve_setting=-1, curve_input=-1;
  bool custom_range=false;
  std::vector<MP::Point> curve;
  std::uint64_t ui_revision = 0, requested = 0, published = 0;
  bool horizontal = false, syncing = false, closed = false, replacing = false;
  int dragged = -1;
  double xmin = 0, xmax = 1, ymin = -1, ymax = 1;
  explicit EditorImpl (GimpPainterMybrushEditor *o) : owner (o) {}
  void detach () noexcept {
    ++ui_revision; ++requested; preview_source.close (); job.reset (); preview.reset ();
    connections.clear ();
    for (auto& connection : resource_changed) connection.close ();
    for (auto& resource : watched_resource) resource.reset ();
    listed.reset (); options.reset (); context.reset ();
  }
  void close () noexcept { if (!closed) { closed = true; detach (); } }
  GimpContext *ctx () const { return options ? GIMP_CONTEXT (options.get ()) : nullptr; }
  MP::Resource snapshot () const { return PainterOptionsRef::retain (options.get ()).snapshot (); }
};
struct SyncScope {
  EditorImpl& impl; std::uint64_t revision; bool previous;
  explicit SyncScope(EditorImpl& i):impl(i),revision(i.ui_revision),previous(i.syncing){i.syncing=true;}
  bool current() const noexcept {return !impl.closed&&impl.ui_revision==revision;}
  ~SyncScope(){if(current())impl.syncing=previous;}
};
struct EditorSlot : SlotSpec<GimpPainterMybrushEditor, EditorImpl> {};
BindingStore& store (GimpPainterMybrushEditor *editor) {
  if (!GIMP_IS_PAINTER_MYBRUSH_EDITOR (editor)) throw Error (GIMP_PAINTER_ERROR_WRONG_TYPE, "Expected painter brush editor");
  return BindingStore::require (G_OBJECT (editor));
}
struct Callback {
  WeakRef<GimpPainterMybrushEditor> owner;
  std::uint64_t generation, ui;
  std::function<void (EditorImpl&)> run;
};
void status (EditorImpl& i, const char *message) {
  if (!i.closed && i.root && i.status) gtk_label_set_text (GTK_LABEL (i.status), message ? message : "");
}
void checked (gboolean success, GError *error) {
  if (!success) { std::string message = error ? error->message : "Painter brush operation failed"; g_clear_error (&error); throw Error (GIMP_PAINTER_ERROR_INVALID_STATE, message.c_str ()); }
  g_clear_error (&error);
}
void dispatch (gpointer data) noexcept {
  auto *cb = static_cast<Callback *> (data);
  auto owner = cb->owner.lock ();
  if (!owner) return;
  auto *binding = BindingStore::find (G_OBJECT (owner.get ()));
  if (!binding || !binding->accepts (cb->generation)) return;
  /* Copy the callable before it can disconnect its own signal. */
  const auto revision = cb->ui, generation = cb->generation; auto function = cb->run;
  try { binding->with<EditorSlot> ([&] (EditorImpl& i) { if (!i.closed && i.ui_revision == revision && !i.syncing) function (i); }); }
  catch (const std::exception& e) { if (binding->accepts(generation)) binding->with<EditorSlot> ([&](EditorImpl& i) { status (i,e.what()); }); }
  catch (...) { if (binding->accepts(generation)) binding->with<EditorSlot> ([&](EditorImpl& i) { status (i,"Unexpected painter editor error"); }); }
}
void simple_cb (gpointer, gpointer data) { dispatch (data); }
void notify_cb (GObject *, GParamSpec *, gpointer data) { dispatch (data); }
void object_cb (gpointer, gpointer, gpointer data) { dispatch (data); }
void context_name_cb (GimpContext *, GimpContextPropType property, gpointer data) {
  if (property == GIMP_CONTEXT_PROP_PAINTER_MYBRUSH) dispatch (data);
}
void destroy_cb (gpointer data, GClosure *) { delete static_cast<Callback *> (data); }
Connection signal_connection (EditorImpl& i, gpointer emitter, const char *signal, GCallback callback, std::function<void(EditorImpl&)> fn) {
  std::unique_ptr<Callback> cb (new Callback { WeakRef<GimpPainterMybrushEditor>(ObjectRef<GimpPainterMybrushEditor>::retain(i.owner)), store(i.owner).generation(), i.ui_revision, std::move(fn) });
  auto connection = Connection::connect (ObjectRef<GObject>::retain (G_OBJECT(emitter)), signal, callback, cb.get(), destroy_cb);
  cb.release(); return connection;
}
void connect (EditorImpl& i, gpointer emitter, const char *signal, GCallback callback, std::function<void(EditorImpl&)> fn) {
  i.connections.push_back(signal_connection(i,emitter,signal,callback,std::move(fn)));
}
GtkWidget *named (GtkWidget *widget, const char *name) { gtk_widget_set_name(widget,name); return widget; }
void pack (GtkWidget *box, GtkWidget *child, bool expand=false) { gtk_box_pack_start (GTK_BOX(box),child,expand,expand,0); }
GtkWidget *row (GtkWidget *box, const char *label, GtkWidget *control) {
  auto *h = gtk_box_new(GTK_ORIENTATION_HORIZONTAL,4); auto *text=gtk_label_new(label);
  gtk_label_set_xalign(GTK_LABEL(text),0); pack(h,text,true); pack(h,control); pack(box,h); return h;
}
GtkWidget *button (EditorImpl& i, GtkWidget *box, const char *label, const char *id, std::function<void(EditorImpl&)> fn) {
  auto *b=named(gtk_button_new_with_label(label),id); pack(box,b);
  connect(i,b,"clicked",G_CALLBACK(simple_cb),std::move(fn)); return b;
}
std::string property (const char *name) { std::string s(name);std::replace(s.begin(),s.end(),'_','-');return s; }
void update_curve (EditorImpl& i);
void refresh (EditorImpl& i);
void refresh_history (EditorImpl& i);
void rebuild_list (EditorImpl& i);
void request_preview (EditorImpl& i);
void apply_curve (EditorImpl& i) {
  int s=gtk_combo_box_get_active(GTK_COMBO_BOX(i.setting)), input=gtk_combo_box_get_active(GTK_COMBO_BOX(i.input));
  std::vector<GimpVector2> points; for(auto p:i.curve) points.push_back({p.x,p.y});
  GError *error=nullptr; auto ok=gimp_painter_mybrush_options_set_curve(i.options.get(),s,input,points.data(),points.size(),&error); checked(ok,error);
}
void curve_bounds (EditorImpl& i) {
  auto in=std::max(0,gtk_combo_box_get_active(GTK_COMBO_BOX(i.input)));
  i.xmin=painter_mypaint_inputs[in].soft_minimum; i.xmax=painter_mypaint_inputs[in].soft_maximum;
  i.ymin=-1; i.ymax=1;
  for(auto p:i.curve) {i.xmin=std::min(i.xmin,p.x);i.xmax=std::max(i.xmax,p.x);i.ymin=std::min(i.ymin,p.y);i.ymax=std::max(i.ymax,p.y);}
  if(i.xmax==i.xmin)i.xmax=i.xmin+1;
  if(i.ymax==i.ymin)i.ymax=i.ymin+1;
}
void update_curve (EditorImpl& i) {
  if(!i.setting||!i.input)return;
  int s=gtk_combo_box_get_active(GTK_COMBO_BOX(i.setting)),in=gtk_combo_box_get_active(GTK_COMBO_BOX(i.input));
  if(s<0||in<0)return;
  if(s!=i.curve_setting||in!=i.curve_input){i.custom_range=false;i.curve_setting=s;i.curve_input=in;}
  i.curve=i.snapshot().curve(s,in);if(!i.custom_range)curve_bounds(i);
  SyncScope sync(i);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(i.curve_enabled),!i.curve.empty());if(!sync.current())return;
  const double bounds[]={i.xmin,i.xmax,i.ymin,i.ymax};
  for(int n=0;n<4;++n){gtk_spin_button_set_value(GTK_SPIN_BUTTON(i.ranges[n]),bounds[n]);if(!sync.current())return;}
  int selected=gtk_combo_box_get_active(GTK_COMBO_BOX(i.point));gtk_combo_box_text_remove_all(GTK_COMBO_BOX_TEXT(i.point));if(!sync.current())return;
  for(unsigned n=0;n<i.curve.size();++n){auto label=std::to_string(n+1);gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(i.point),label.c_str());if(!sync.current())return;}
  selected=i.curve.empty()?-1:std::max(0,std::min(selected,int(i.curve.size())-1));
  gtk_combo_box_set_active(GTK_COMBO_BOX(i.point),selected);if(!sync.current())return;
  if(selected>=0) {gtk_spin_button_set_value(GTK_SPIN_BUTTON(i.px),i.curve[selected].x);if(!sync.current())return;gtk_spin_button_set_value(GTK_SPIN_BUTTON(i.py),i.curve[selected].y);if(!sync.current())return;}
  if(selected<0){gtk_spin_button_set_value(GTK_SPIN_BUTTON(i.px),0);if(!sync.current())return;gtk_spin_button_set_value(GTK_SPIN_BUTTON(i.py),0);if(!sync.current())return;}
  gtk_widget_set_sensitive(i.px,selected>=0);gtk_widget_set_sensitive(i.py,selected>=0);
  if(sync.current())gtk_widget_queue_draw(i.graph);
}
void rescale_curve(EditorImpl&i) {
  double bounds[4];for(int n=0;n<4;++n)bounds[n]=gtk_spin_button_get_value(GTK_SPIN_BUTTON(i.ranges[n]));
  if(!std::all_of(bounds,bounds+4,[](double v){return std::isfinite(v);})||bounds[0]>=bounds[1]||bounds[2]>=bounds[3])
    throw Error(GIMP_PAINTER_ERROR_INVALID_STATE,"Curve ranges need finite increasing bounds");
  auto previous=i.curve;
  for(auto&point:i.curve){point.x=bounds[0]+(point.x-i.xmin)/(i.xmax-i.xmin)*(bounds[1]-bounds[0]);point.y=bounds[2]+(point.y-i.ymin)/(i.ymax-i.ymin)*(bounds[3]-bounds[2]);}
  const double old[]={i.xmin,i.xmax,i.ymin,i.ymax};const bool was_custom=i.custom_range;
  i.xmin=bounds[0];i.xmax=bounds[1];i.ymin=bounds[2];i.ymax=bounds[3];i.custom_range=true;
  try{apply_curve(i);}catch(...){i.curve=std::move(previous);i.xmin=old[0];i.xmax=old[1];i.ymin=old[2];i.ymax=old[3];i.custom_range=was_custom;throw;}
}
void point_values (EditorImpl& i) {
  int n=gtk_combo_box_get_active(GTK_COMBO_BOX(i.point));if(n<0||n>=int(i.curve.size()))return;
  i.syncing=true;gtk_spin_button_set_value(GTK_SPIN_BUTTON(i.px),i.curve[n].x);gtk_spin_button_set_value(GTK_SPIN_BUTTON(i.py),i.curve[n].y);i.syncing=false;
}
void add_point (EditorImpl& i, double x, double y) {
  if(i.curve.size()>=8)throw Error(GIMP_PAINTER_ERROR_INVALID_STATE,"A mapping supports at most eight points");
  if(i.curve.empty()){i.curve={{i.xmin,0},{i.xmax,0}};}
  else {i.curve.push_back({x,y});std::stable_sort(i.curve.begin(),i.curve.end(),[](MP::Point a,MP::Point b){return a.x<b.x;});}
  apply_curve(i);
}
void delete_point (EditorImpl& i) {
  int n=gtk_combo_box_get_active(GTK_COMBO_BOX(i.point));if(n<0||n>=int(i.curve.size()))return;
  if(i.curve.size()<=2)i.curve.clear();else i.curve.erase(i.curve.begin()+n);apply_curve(i);
}
bool graph_owner (gpointer data, const std::function<void(EditorImpl&)>& fn) {
  auto *cb=static_cast<Callback*>(data);auto owner=cb->owner.lock();if(!owner)return false;
  auto *binding=BindingStore::find(G_OBJECT(owner.get()));if(!binding||!binding->accepts(cb->generation))return false;
  return boundary<bool>(nullptr,false,[&]{return binding->with<EditorSlot>([&](EditorImpl&i){if(i.closed||i.ui_revision!=cb->ui)return false;fn(i);return true;});});
}
gboolean draw_cb (GtkWidget *widget, cairo_t *cr, gpointer data) {
  graph_owner(data,[&](EditorImpl&i){
    double w=gtk_widget_get_allocated_width(widget)-16,h=gtk_widget_get_allocated_height(widget)-16;
    cairo_set_source_rgb(cr,.15,.15,.15);cairo_paint(cr);cairo_set_source_rgb(cr,.4,.4,.4);cairo_set_line_width(cr,1);
    for(int n=0;n<=4;++n){cairo_move_to(cr,8+n*w/4,8);cairo_line_to(cr,8+n*w/4,h+8);cairo_move_to(cr,8,8+n*h/4);cairo_line_to(cr,w+8,8+n*h/4);}cairo_stroke(cr);
    cairo_set_source_rgb(cr,.3,.75,1);cairo_set_line_width(cr,2);
    for(unsigned n=0;n<i.curve.size();++n){double x=8+(i.curve[n].x-i.xmin)*w/(i.xmax-i.xmin),y=8+h-(i.curve[n].y-i.ymin)*h/(i.ymax-i.ymin);if(n)cairo_line_to(cr,x,y);else cairo_move_to(cr,x,y);}cairo_stroke(cr);
    for(auto p:i.curve){cairo_arc(cr,8+(p.x-i.xmin)*w/(i.xmax-i.xmin),8+h-(p.y-i.ymin)*h/(i.ymax-i.ymin),4,0,2*G_PI);cairo_fill(cr);}
  });return FALSE;
}
MP::Point graph_point(EditorImpl&i,double x,double y) {
  double w=std::max(1,gtk_widget_get_allocated_width(i.graph)-16),h=std::max(1,gtk_widget_get_allocated_height(i.graph)-16);
  return {i.xmin+std::max(0.,std::min(1.,(x-8)/w))*(i.xmax-i.xmin),i.ymax-std::max(0.,std::min(1.,(y-8)/h))*(i.ymax-i.ymin)};
}
gboolean press_cb(GtkWidget*,GdkEventButton*event,gpointer data) {
  graph_owner(data,[&](EditorImpl&i){
    i.dragged=-1;
    auto p=graph_point(i,event->x,event->y);int nearest=-1;double distance=.06;
    for(unsigned n=0;n<i.curve.size();++n){double d=std::hypot((p.x-i.curve[n].x)/(i.xmax-i.xmin),(p.y-i.curve[n].y)/(i.ymax-i.ymin));if(d<distance){distance=d;nearest=n;}}
    if(nearest>=0){gtk_combo_box_set_active(GTK_COMBO_BOX(i.point),nearest);if(event->button==3)delete_point(i);else {i.dragged=nearest;gtk_grab_add(i.graph);}}
    else if(event->button==1)add_point(i,p.x,p.y);
  });return TRUE;
}
gboolean motion_cb(GtkWidget*,GdkEventMotion*event,gpointer data) {
  graph_owner(data,[&](EditorImpl&i){if(i.dragged<0||i.dragged>=int(i.curve.size())||!(event->state&GDK_BUTTON1_MASK))return;
    auto p=graph_point(i,event->x,event->y);if(i.dragged>0)p.x=std::max(p.x,i.curve[i.dragged-1].x);if(i.dragged+1<int(i.curve.size()))p.x=std::min(p.x,i.curve[i.dragged+1].x);
    i.curve[i.dragged]=p;apply_curve(i);
  });return TRUE;
}
gboolean release_cb(GtkWidget*,GdkEventButton*,gpointer data){graph_owner(data,[](EditorImpl&i){i.dragged=-1;if(gtk_widget_has_grab(i.graph))gtk_grab_remove(i.graph);});return TRUE;}
std::string resource_tags(GimpPainterMybrush*b) {
  auto r=PainterMybrushRef::retain(b).snapshot();std::string result=r.group();
  for(auto*l=gimp_tagged_get_tags(GIMP_TAGGED(b));l;l=l->next){if(!result.empty())result+=", ";result+=gimp_tag_get_name(GIMP_TAG(l->data));}return result;
}
void rebuild_list(EditorImpl&i) {
  if(!i.tree||!i.options||i.closed)return;
  auto options=i.options;auto*ctx=GIMP_CONTEXT(options.get());
  auto model_owner=ObjectRef<GObject>::retain(G_OBJECT(gtk_tree_view_get_model(GTK_TREE_VIEW(i.tree))));
  auto*model=GTK_LIST_STORE(model_owner.get());
  String needle(g_utf8_casefold(gtk_entry_get_text(GTK_ENTRY(i.search)),-1));
  auto*container=gimp_data_factory_get_container(ctx->gimp->painter_mybrush_factory);
  std::vector<ObjectRef<GimpPainterMybrush>> resources;
  for(int n=0;n<gimp_container_get_n_children(container);++n)
    resources.push_back(ObjectRef<GimpPainterMybrush>::retain(GIMP_PAINTER_MYBRUSH(gimp_container_get_child_by_index(container,n))));
  SyncScope sync(i);gtk_list_store_clear(model);if(!sync.current())return;
  for(const auto&owner:resources){
    auto*b=owner.get();const char*name=gimp_object_get_name(b);auto tags=resource_tags(b);
    String haystack(g_utf8_casefold((std::string(name?name:"")+" "+tags).c_str(),-1));
    if(*needle.get()&&!strstr(haystack.get(),needle.get()))continue;
    auto icon=ObjectRef<GObject>::adopt(G_OBJECT(gimp_painter_mybrush_ref_icon(b)));
    ObjectRef<GObject> small;if(icon)small=ObjectRef<GObject>::adopt(G_OBJECT(gdk_pixbuf_scale_simple(GDK_PIXBUF(icon.get()),32,32,GDK_INTERP_BILINEAR)));
    GtkTreeIter iter;gtk_list_store_append(model,&iter);if(!sync.current())return;
    gtk_list_store_set(model,&iter,COL_RESOURCE,b,COL_ICON,small.get(),COL_NAME,name,COL_TAGS,tags.c_str(),-1);if(!sync.current())return;
    if(b==gimp_context_get_painter_mybrush(ctx))gtk_tree_selection_select_iter(gtk_tree_view_get_selection(GTK_TREE_VIEW(i.tree)),&iter);
    if(!sync.current())return;
  }
  i.listed=ObjectRef<GimpPainterMybrush>::retain(gimp_context_get_painter_mybrush(ctx));
}
void select_list(EditorImpl&i) {
  GtkTreeModel*model;GtkTreeIter iter;
  if(!gtk_tree_selection_get_selected(gtk_tree_view_get_selection(GTK_TREE_VIEW(i.tree)),&model,&iter))return;
  GimpPainterMybrush*b=nullptr;gtk_tree_model_get(model,&iter,COL_RESOURCE,&b,-1);auto hold=ObjectRef<GimpPainterMybrush>::adopt(b);
  gimp_context_set_painter_mybrush(i.ctx(),b);
  if(!i.closed&&i.context&&GIMP_CONTEXT(i.context.get())!=i.ctx())gimp_context_set_painter_mybrush(GIMP_CONTEXT(i.context.get()),b);
}
void refresh_history(EditorImpl&i) {
  if(!i.options||i.closed)return;
  auto model=i.options;SyncScope sync(i);
  gtk_combo_box_text_remove_all(GTK_COMBO_BOX_TEXT(i.history));
  if(!sync.current())return;
  for(guint n=0;n<gimp_painter_mybrush_options_history_size(model.get());++n){
    String name(gimp_painter_mybrush_options_history_name(model.get(),n));
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(i.history),name.get());
    if(!sync.current())return;
  }
}
void refresh(EditorImpl&i) {
  if(!i.options||i.closed)return;
  auto model=i.options;auto context=i.context;auto*ctx=GIMP_CONTEXT(model.get());
  const auto resource=i.snapshot();SyncScope sync(i);
  for(int n=0;n<BRUSH_MAPPING_COUNT;++n){
    const double value=resource.base_value(n);auto*adj=gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(i.numeric[n]));
    gtk_adjustment_set_lower(adj,std::min(double(painter_mypaint_settings[n].minimum),value));if(!sync.current())return;
    gtk_adjustment_set_upper(adj,std::max(double(painter_mypaint_settings[n].maximum),value));if(!sync.current())return;
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(i.numeric[n]),value);if(!sync.current())return;
  }
  for(int n=0;n<BRUSH_BOOL_COUNT;++n){gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(i.switches[n]),resource.switch_value(painter_mypaint_switches[n].index));if(!sync.current())return;}
  for(int n=0;n<BRUSH_TEXT_COUNT;++n){gtk_entry_set_text(GTK_ENTRY(i.texts[n]),resource.text_value(painter_mypaint_texts[n].index).c_str());if(!sync.current())return;}
  auto selected=ObjectRef<GimpPainterMybrush>::retain(gimp_context_get_painter_mybrush(ctx));auto*b=selected.get();bool writable=b&&gimp_data_is_writable(GIMP_DATA(b));gboolean conflict=FALSE,dirty=FALSE;
  g_object_get(model.get(),"painter-conflict",&conflict,"painter-dirty",&dirty,nullptr);
  dirty=dirty||(b&&gimp_data_is_dirty(GIMP_DATA(b)));
  gtk_widget_set_sensitive(i.save,writable&&!conflict);if(!sync.current())return;
  gtk_widget_set_sensitive(i.rename,writable&&!conflict);if(!sync.current())return;
  gtk_widget_set_sensitive(i.remove,b&&gimp_data_is_deletable(GIMP_DATA(b)));if(!sync.current())return;
  gtk_entry_set_text(GTK_ENTRY(i.name),b?gimp_object_get_name(b):"");if(!sync.current())return;
  gimp_editor_set_name(GIMP_EDITOR(i.owner),b?gimp_object_get_name(b):_("Painter MyPaint Brushes"));if(!sync.current())return;
  gtk_entry_set_text(GTK_ENTRY(i.tags),b?resource_tags(b).c_str():"");if(!sync.current())return;
  refresh_history(i);if(!sync.current())return;
  std::string text=conflict?_("The source changed externally. Save As preserves this draft."):dirty?_("Unsaved draft"):_("Saved brush");
  if(!writable)text+=_(" · Read-only source: use Save As");
  for(auto d:resource.diagnostics())text+="\n"+d.path+": "+d.message;
  status(i,text.c_str());if(!sync.current())return;
  update_curve(i);if(!sync.current())return;
  if(context&&GIMP_CONTEXT(context.get())!=ctx&&gimp_context_get_painter_mybrush(GIMP_CONTEXT(context.get()))!=b)
    gimp_context_set_painter_mybrush(GIMP_CONTEXT(context.get()),b);
  if(!sync.current())return;
  if(b!=i.listed.get())rebuild_list(i);
  if(sync.current())request_preview(i);
}
void request_preview(EditorImpl&i) {
  if (i.options && !i.closed) {
    auto resource = i.snapshot ();
    for (int n=0;n<2;++n) {
      GObject *selected = n ? G_OBJECT(gimp_context_get_pattern(i.ctx())) : G_OBJECT(gimp_context_get_brush(i.ctx()));
      const int setting=n?BRUSH_TEXTURE_NAME:BRUSH_BRUSHMARK_NAME;
      if(!resource.text_is_null(setting)) {
        auto*factory=n?i.ctx()->gimp->pattern_factory:i.ctx()->gimp->brush_factory;
        selected=G_OBJECT(gimp_container_get_child_by_name(gimp_data_factory_get_container(factory),resource.text_value(setting).c_str()));
      }
      if(selected!=i.watched_resource[n].get()) {
        auto replacement=ObjectRef<GObject>::retain(selected);
        auto connection=selected?signal_connection(i,selected,"dirty",G_CALLBACK(simple_cb),request_preview):Connection();
        i.resource_changed[n]=std::move(connection);
        i.watched_resource[n]=std::move(replacement);
      }
    }
  }
  ++i.requested;i.preview_source.close();i.job.reset();i.preview.reset();
  if(!i.options||i.closed)return;
  if(i.preview_image)gtk_image_clear(GTK_IMAGE(i.preview_image));
  auto weak=std::make_shared<WeakRef<GimpPainterMybrushEditor>>(ObjectRef<GimpPainterMybrushEditor>::retain(i.owner));
  auto generation=store(i.owner).generation(),revision=i.requested;
  /* Capture resources and colour synchronously, before another editor changes
   * the context; the job never consults a later model generation. */
  try{i.job=std::make_shared<PreviewJob>(i.ctx(),i.snapshot());}
  catch(const std::exception&e){status(i,e.what());return;}
  i.preview_source=Source::idle(nullptr,G_PRIORITY_DEFAULT_IDLE,[weak,generation,revision] {
    auto owner=weak->lock();if(!owner)return false;auto*binding=BindingStore::find(G_OBJECT(owner.get()));
    if(!binding||!binding->accepts(generation))return false;
    bool more=false;bool ready=false;
    binding->with<EditorSlot>([&](EditorImpl&i){if(i.closed||i.requested!=revision||!i.job)return;
      auto*model_binding=i.options?BindingStore::find(G_OBJECT(i.options.get())):nullptr;
      if(!model_binding||model_binding->state()!=BindingStore::State::active){
        i.job.reset();i.preview.reset();if(i.root&&i.preview_image)gtk_image_clear(GTK_IMAGE(i.preview_image));
        status(i,_("The brush options were closed"));return;
      }
      try{auto job=i.job;more=job->step();if(!more&&binding->accepts(generation)&&i.requested==revision){i.preview=job->pixbuf();i.published=revision;i.job.reset();gtk_image_set_from_pixbuf(GTK_IMAGE(i.preview_image),GDK_PIXBUF(i.preview.get()));ready=true;}}
      catch(const std::exception&e){i.job.reset();status(i,e.what());}
    });
    if(ready&&binding->accepts(generation))g_signal_emit_by_name(owner.get(),"preview-ready",revision);
    return more;
  });
}
void prompt(EditorImpl&i,int operation) {
  /* Dialog owns only weak, generation-checked callbacks. No nested main loop
   * can retain an editor borrow while context replacement is in progress. */
  auto*dialog=gtk_dialog_new_with_buttons(operation==2?_("Delete Painter Brush"):operation==1?_("Rename Painter Brush"):_("Save Painter Brush As"),
    GTK_IS_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(i.owner)))?GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(i.owner))):nullptr,
    GtkDialogFlags(GTK_DIALOG_DESTROY_WITH_PARENT),_("Cancel"),GTK_RESPONSE_CANCEL,
    operation==2?_("Delete"):_("Save"),GTK_RESPONSE_OK,nullptr);
  auto*entry=gtk_entry_new();gtk_entry_set_text(GTK_ENTRY(entry),gtk_entry_get_text(GTK_ENTRY(i.name)));
  if(operation==0){std::string n=gtk_entry_get_text(GTK_ENTRY(entry));n+=_(" copy");gtk_entry_set_text(GTK_ENTRY(entry),n.c_str());}
  gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(dialog))),operation==2?gtk_label_new(_("Delete the selected brush from disk?")):entry);
  struct Prompt {WeakRef<GimpPainterMybrushEditor> editor;std::uint64_t generation,ui;int operation;GtkWidget*entry;ObjectRef<GimpPainterMybrush> resource;};
  auto*payload=new Prompt{WeakRef<GimpPainterMybrushEditor>(ObjectRef<GimpPainterMybrushEditor>::retain(i.owner)),store(i.owner).generation(),i.ui_revision,operation,entry,ObjectRef<GimpPainterMybrush>::retain(gimp_context_get_painter_mybrush(i.ctx()))};
  g_signal_connect_data(dialog,"response",G_CALLBACK(+[](GtkDialog*d,gint response,gpointer data){
    auto*p=static_cast<Prompt*>(data);auto owner=p->editor.lock();
    if(response==GTK_RESPONSE_OK&&owner){auto*b=BindingStore::find(G_OBJECT(owner.get()));if(b&&b->accepts(p->generation))b->with<EditorSlot>([&](EditorImpl&i){
      if(i.ui_revision!=p->ui||i.closed||gimp_context_get_painter_mybrush(i.ctx())!=p->resource.get()){status(i,_("Selection changed; no brush was modified"));return;}
      GError*error=nullptr;gboolean ok=p->operation==2?gimp_painter_mybrush_editor_delete(i.owner,&error):p->operation==1?gimp_painter_mybrush_editor_rename(i.owner,gtk_entry_get_text(GTK_ENTRY(p->entry)),&error):gimp_painter_mybrush_editor_save(i.owner,gtk_entry_get_text(GTK_ENTRY(p->entry)),&error);
      if(!ok)status(i,error?error->message:_("Unable to save brush"));g_clear_error(&error);
    });}gtk_widget_destroy(GTK_WIDGET(d));
  }),payload,+[](gpointer p,GClosure*){delete static_cast<Prompt*>(p);},GConnectFlags(0));
  if(operation==2)gtk_widget_destroy(entry);
  gtk_widget_show_all(dialog);
}
void build(EditorImpl&i) {
  i.syncing=true;
  auto*root=gtk_box_new(i.horizontal?GTK_ORIENTATION_HORIZONTAL:GTK_ORIENTATION_VERTICAL,6);
  auto*viewport=gtk_scrolled_window_new(nullptr,nullptr);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(viewport),GTK_POLICY_AUTOMATIC,GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(viewport),360);
  gtk_container_add(GTK_CONTAINER(viewport),root);i.root=viewport;pack(GTK_WIDGET(i.owner),viewport,true);
  auto*left=gtk_box_new(GTK_ORIENTATION_VERTICAL,4),*right=gtk_box_new(GTK_ORIENTATION_VERTICAL,4);pack(root,left,true);pack(root,right,true);
  i.search=named(gtk_search_entry_new(),"painter-brush-search");gtk_entry_set_placeholder_text(GTK_ENTRY(i.search),_("Search brushes, groups and tags"));pack(left,i.search);
  auto*list=gtk_list_store_new(N_COLUMNS,GIMP_TYPE_PAINTER_MYBRUSH,GDK_TYPE_PIXBUF,G_TYPE_STRING,G_TYPE_STRING);
  i.tree=named(gtk_tree_view_new_with_model(GTK_TREE_MODEL(list)),"painter-brush-list");g_object_unref(list);
  gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(i.tree),-1,"",gtk_cell_renderer_pixbuf_new(),"pixbuf",COL_ICON,nullptr);
  gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(i.tree),-1,_("Brush"),gtk_cell_renderer_text_new(),"text",COL_NAME,nullptr);
  gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(i.tree),-1,_("Tags"),gtk_cell_renderer_text_new(),"text",COL_TAGS,nullptr);
  auto*scroll=gtk_scrolled_window_new(nullptr,nullptr);gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),GTK_POLICY_AUTOMATIC,GTK_POLICY_AUTOMATIC);gtk_widget_set_size_request(scroll,240,155);gtk_container_add(GTK_CONTAINER(scroll),i.tree);pack(left,scroll,true);
  connect(i,i.search,"changed",G_CALLBACK(simple_cb),rebuild_list);connect(i,gtk_tree_view_get_selection(GTK_TREE_VIEW(i.tree)),"changed",G_CALLBACK(simple_cb),select_list);
  i.name=named(gtk_entry_new(),"painter-brush-name");gtk_editable_set_editable(GTK_EDITABLE(i.name),FALSE);row(left,_("Brush"),i.name);
  i.tags=named(gtk_entry_new(),"painter-brush-tags");row(left,_("Tags (comma separated)"),i.tags);
  connect(i,i.tags,"activate",G_CALLBACK(simple_cb),[](EditorImpl&i){auto*b=gimp_context_get_painter_mybrush(i.ctx());if(!b)return;gchar**names=g_strsplit(gtk_entry_get_text(GTK_ENTRY(i.tags)),",",-1);GList*tags=nullptr;for(int n=0;names[n];++n){g_strstrip(names[n]);if(*names[n]){auto*t=gimp_tag_new(names[n]);if(t)tags=g_list_prepend(tags,t);}}gimp_tagged_set_tags(GIMP_TAGGED(b),tags);g_list_free_full(tags,g_object_unref);g_strfreev(names);rebuild_list(i);});
  auto*actions=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,2);pack(left,actions);
  i.save=button(i,actions,_("Save"),"painter-brush-save",[](EditorImpl&i){GError*e=nullptr;auto ok=gimp_painter_mybrush_editor_save(i.owner,nullptr,&e);checked(ok,e);});
  button(i,actions,_("Save As"),"painter-brush-save-as",[](EditorImpl&i){prompt(i,0);});
  i.rename=button(i,actions,_("Rename"),"painter-brush-rename",[](EditorImpl&i){prompt(i,1);});
  i.remove=button(i,actions,_("Delete"),"painter-brush-delete",[](EditorImpl&i){prompt(i,2);});
  auto*more=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,2);pack(left,more);
  button(i,more,_("Duplicate"),"painter-brush-duplicate",[](EditorImpl&i){prompt(i,0);});
  button(i,more,_("Popup Editor"),"painter-brush-popup",[](EditorImpl&i){GError*e=nullptr;auto*w=gimp_painter_mybrush_editor_popup(i.ctx(),GTK_WIDGET(i.owner),&e);checked(w!=nullptr,e);});
  button(i,more,_("Refresh"),"painter-brush-refresh",[](EditorImpl&i){gimp_data_factory_data_refresh(i.ctx()->gimp->painter_mybrush_factory,i.ctx());rebuild_list(i);});
  i.history=named(gtk_combo_box_text_new(),"painter-brush-history");row(left,_("Draft history"),i.history);
  connect(i,i.history,"changed",G_CALLBACK(simple_cb),[](EditorImpl&i){int n=gtk_combo_box_get_active(GTK_COMBO_BOX(i.history));if(n>=0){GError*e=nullptr;auto ok=gimp_painter_mybrush_options_restore_history(i.options.get(),n,&e);checked(ok,e);}});
  i.preview_image=named(gtk_image_new(),"painter-brush-preview");pack(left,i.preview_image);
  i.status=named(gtk_label_new(""),"painter-brush-status");gtk_label_set_line_wrap(GTK_LABEL(i.status),TRUE);gtk_label_set_xalign(GTK_LABEL(i.status),0);pack(left,i.status);
  auto*notebook=gtk_notebook_new();pack(right,notebook,true);
  auto*settings=gtk_box_new(GTK_ORIENTATION_VERTICAL,3);auto*ss=gtk_scrolled_window_new(nullptr,nullptr);gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(ss),GTK_POLICY_NEVER,GTK_POLICY_AUTOMATIC);gtk_widget_set_size_request(ss,270,250);gtk_container_add(GTK_CONTAINER(ss),settings);gtk_notebook_append_page(GTK_NOTEBOOK(notebook),ss,gtk_label_new(_("Settings")));
  for(int n=0;n<BRUSH_MAPPING_COUNT;++n){auto&m=painter_mypaint_settings[n];auto key=property(m.internal_name);auto*w=named(gtk_spin_button_new_with_range(m.minimum,m.maximum,.01),key.c_str());gtk_spin_button_set_digits(GTK_SPIN_BUTTON(w),6);gtk_widget_set_tooltip_text(w,m.tooltip);row(settings,m.displayed_name,w);i.numeric.push_back(w);
    connect(i,w,"value-changed",G_CALLBACK(simple_cb),[n,key](EditorImpl&i){g_object_set(i.options.get(),key.c_str(),gtk_spin_button_get_value(GTK_SPIN_BUTTON(i.numeric[n])),nullptr);});}
  auto*resources=gtk_box_new(GTK_ORIENTATION_VERTICAL,3);gtk_notebook_append_page(GTK_NOTEBOOK(notebook),resources,gtk_label_new(_("Shape & Paper")));
  for(int n=0;n<BRUSH_BOOL_COUNT;++n){auto&m=painter_mypaint_switches[n];auto key=property(m.internal_name);auto*w=named(gtk_check_button_new_with_label(m.displayed_name),key.c_str());pack(resources,w);i.switches.push_back(w);connect(i,w,"toggled",G_CALLBACK(simple_cb),[n,key](EditorImpl&i){g_object_set(i.options.get(),key.c_str(),gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(i.switches[n])),nullptr);});}
  pack(resources,gimp_brush_box_new(nullptr,i.ctx(),_("Brush shape"),3));pack(resources,gimp_pattern_box_new(nullptr,i.ctx(),_("Paper texture"),3));
  for(int n=0;n<BRUSH_TEXT_COUNT;++n){auto&m=painter_mypaint_texts[n];auto key=property(m.internal_name);auto*w=named(gtk_entry_new(),key.c_str());row(resources,m.displayed_name,w);i.texts.push_back(w);connect(i,w,"activate",G_CALLBACK(simple_cb),[n,key](EditorImpl&i){g_object_set(i.options.get(),key.c_str(),gtk_entry_get_text(GTK_ENTRY(i.texts[n])),nullptr);});}
  auto*curves=gtk_box_new(GTK_ORIENTATION_VERTICAL,3);gtk_notebook_append_page(GTK_NOTEBOOK(notebook),curves,gtk_label_new(_("Input Curves")));
  i.setting=named(gtk_combo_box_text_new(),"painter-curve-setting");for(auto&m:painter_mypaint_settings)gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(i.setting),m.displayed_name);row(curves,_("Setting"),i.setting);gtk_combo_box_set_active(GTK_COMBO_BOX(i.setting),0);
  i.input=named(gtk_combo_box_text_new(),"painter-curve-input");for(auto&m:painter_mypaint_inputs)gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(i.input),m.displayed_name);row(curves,_("Input"),i.input);gtk_combo_box_set_active(GTK_COMBO_BOX(i.input),0);
  connect(i,i.setting,"changed",G_CALLBACK(simple_cb),update_curve);connect(i,i.input,"changed",G_CALLBACK(simple_cb),update_curve);
  i.curve_enabled=named(gtk_check_button_new_with_label(_("Enable input mapping")),"painter-curve-enabled");pack(curves,i.curve_enabled);
  connect(i,i.curve_enabled,"toggled",G_CALLBACK(simple_cb),[](EditorImpl&i){
    if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(i.curve_enabled))){
      if(i.curve.empty()){int n=gtk_combo_box_get_active(GTK_COMBO_BOX(i.input));i.curve={{i.xmin,0},{i.xmax,painter_mypaint_inputs[n].normal}};}
    }else i.curve.clear();
    apply_curve(i);
  });
  i.graph=named(gtk_drawing_area_new(),"painter-curve-graph");gtk_widget_set_size_request(i.graph,240,150);gtk_widget_add_events(i.graph,GDK_BUTTON_PRESS_MASK|GDK_BUTTON_RELEASE_MASK|GDK_POINTER_MOTION_MASK);pack(curves,i.graph,true);
  connect(i,i.graph,"draw",G_CALLBACK(draw_cb),{});connect(i,i.graph,"button-press-event",G_CALLBACK(press_cb),{});connect(i,i.graph,"motion-notify-event",G_CALLBACK(motion_cb),{});connect(i,i.graph,"button-release-event",G_CALLBACK(release_cb),{});
  i.point=named(gtk_combo_box_text_new(),"painter-curve-point");row(curves,_("Point"),i.point);connect(i,i.point,"changed",G_CALLBACK(simple_cb),point_values);
  i.px=named(gtk_spin_button_new_with_range(-G_MAXFLOAT,G_MAXFLOAT,.01),"painter-curve-x");i.py=named(gtk_spin_button_new_with_range(-G_MAXFLOAT,G_MAXFLOAT,.01),"painter-curve-y");
  for(auto*w:{i.px,i.py}){gtk_spin_button_set_digits(GTK_SPIN_BUTTON(w),8);gtk_spin_button_set_value(GTK_SPIN_BUTTON(w),0);gtk_entry_set_width_chars(GTK_ENTRY(w),12);}
  row(curves,_("Input value"),i.px);row(curves,_("Setting offset"),i.py);
  auto*ca=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,2);pack(curves,ca);
  button(i,ca,_("Apply Point"),"painter-curve-apply",[](EditorImpl&i){int n=gtk_combo_box_get_active(GTK_COMBO_BOX(i.point));if(n>=0){auto saved=i.curve;i.curve[n]={gtk_spin_button_get_value(GTK_SPIN_BUTTON(i.px)),gtk_spin_button_get_value(GTK_SPIN_BUTTON(i.py))};try{apply_curve(i);}catch(...){i.curve=std::move(saved);throw;}}});
  button(i,ca,_("Add"),"painter-curve-add",[](EditorImpl&i){add_point(i,(i.xmin+i.xmax)/2,0);});button(i,ca,_("Remove"),"painter-curve-remove",delete_point);
  button(i,ca,_("Clear"),"painter-curve-clear",[](EditorImpl&i){i.curve.clear();apply_curve(i);});
  auto*range_expander=gtk_expander_new(_("Curve range and rescaling"));auto*range_box=gtk_box_new(GTK_ORIENTATION_VERTICAL,3);gtk_container_add(GTK_CONTAINER(range_expander),range_box);pack(curves,range_expander);
  const char*range_names[]={"painter-curve-x-min","painter-curve-x-max","painter-curve-y-min","painter-curve-y-max"};
  const char*range_labels[]={_("Input minimum"),_("Input maximum"),_("Offset minimum"),_("Offset maximum")};
  for(int n=0;n<4;++n){i.ranges[n]=named(gtk_spin_button_new_with_range(-G_MAXFLOAT,G_MAXFLOAT,.01),range_names[n]);gtk_spin_button_set_digits(GTK_SPIN_BUTTON(i.ranges[n]),8);gtk_entry_set_width_chars(GTK_ENTRY(i.ranges[n]),12);row(range_box,range_labels[n],i.ranges[n]);}
  auto*range_actions=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,3);pack(range_box,range_actions);
  button(i,range_actions,_("Rescale Curve"),"painter-curve-rescale",rescale_curve);
  button(i,range_actions,_("Fit View"),"painter-curve-fit",[](EditorImpl&i){i.custom_range=false;update_curve(i);});
  pack(curves,gtk_label_new(_("Click to add, drag to move, right-click to remove.\nExact coordinates above retain up to eight points.")));
  if(i.context.get()!=G_OBJECT(i.ctx()))
    connect(i,i.context.get(),"painter-mybrush-changed",G_CALLBACK(object_cb),[](EditorImpl&i){
      GError*error=nullptr;
      auto model=ObjectRef<GimpPainterMybrushOptions>::adopt(gimp_painter_mybrush_options_ref_for_context(GIMP_CONTEXT(i.context.get()),&error));
      checked(bool(model),error);
    });
  connect(i,i.options.get(),"settings-changed",G_CALLBACK(simple_cb),refresh);
  connect(i,i.options.get(),"history-changed",G_CALLBACK(simple_cb),refresh_history);
  connect(i,i.options.get(),"prop-name-changed",G_CALLBACK(context_name_cb),[](EditorImpl&i){rebuild_list(i);refresh(i);});
  connect(i,i.options.get(),"notify::painter-conflict",G_CALLBACK(notify_cb),refresh);
  connect(i,i.options.get(),"foreground-changed",G_CALLBACK(object_cb),request_preview);
  connect(i,i.options.get(),"background-changed",G_CALLBACK(object_cb),request_preview);
  connect(i,i.options.get(),"brush-changed",G_CALLBACK(object_cb),request_preview);
  connect(i,i.options.get(),"pattern-changed",G_CALLBACK(object_cb),request_preview);
  auto*container=gimp_data_factory_get_container(i.ctx()->gimp->painter_mybrush_factory);
  connect(i,container,"add",G_CALLBACK(object_cb),rebuild_list);connect(i,container,"remove",G_CALLBACK(object_cb),rebuild_list);
  for (auto *w : {i.root, root, i.curve_enabled, i.ranges[0], i.ranges[1], i.ranges[2], i.ranges[3], i.status, i.preview_image, i.search, i.tree, i.history, i.setting, i.input, i.graph, i.point, i.px, i.py, i.name, i.tags, i.save, i.rename, i.remove})
    i.widgets.push_back(ObjectRef<GObject>::retain(G_OBJECT(w)));
  for (const auto& controls : {i.numeric, i.switches, i.texts})
    for (auto *w : controls) i.widgets.push_back(ObjectRef<GObject>::retain(G_OBJECT(w)));
  i.syncing=false;refresh(i);if (!i.closed) gtk_widget_show_all(i.root);
}
void constructed(GObject*object) {
  G_OBJECT_CLASS(gimp_painter_mybrush_editor_parent_class)->constructed(object);
  auto*e=GIMP_PAINTER_MYBRUSH_EDITOR(object);if(!e->binding_failed)e->binding_failed=!boundary<bool>(nullptr,false,[&]{store(e).activate();return true;});
}
void destroy(GtkWidget*widget) {
  gimp_painter_binding_close(G_OBJECT(widget),nullptr);
  GTK_WIDGET_CLASS(gimp_painter_mybrush_editor_parent_class)->destroy(widget);
}
void dispose(GObject*object) {gimp_painter_binding_close(object,nullptr);G_OBJECT_CLASS(gimp_painter_mybrush_editor_parent_class)->dispose(object);}
}
static void gimp_painter_mybrush_editor_class_init(GimpPainterMybrushEditorClass*klass) {
  G_OBJECT_CLASS(klass)->constructed=constructed;G_OBJECT_CLASS(klass)->dispose=dispose;GTK_WIDGET_CLASS(klass)->destroy=destroy;
  g_signal_new("preview-ready",G_TYPE_FROM_CLASS(klass),G_SIGNAL_RUN_LAST,0,nullptr,nullptr,nullptr,G_TYPE_NONE,1,G_TYPE_UINT64);
}
static void gimp_painter_mybrush_editor_init(GimpPainterMybrushEditor*editor) {
  editor->binding_failed=!boundary<bool>(nullptr,false,[&]{BindingStore::ensure(G_OBJECT(editor)).emplace<EditorSlot>(editor);return true;});
}
static void docked_init(GimpDockedInterface*iface) {
  iface->set_context=+[](GimpDocked*d,GimpContext*c){
    auto*editor=GIMP_PAINTER_MYBRUSH_EDITOR(d);
    if(c)gimp_painter_mybrush_editor_set_context(editor,c,nullptr);
    else boundary_void(nullptr,[&]{store(editor).with<EditorSlot>([](EditorImpl&i){
      i.detach();if(i.root)gtk_widget_destroy(i.root);i.root=nullptr;
      i.widgets.clear();i.numeric.clear();i.switches.clear();i.texts.clear();
    });});
  };
  iface->get_title=+[](GimpDocked*)->gchar*{return g_strdup(_("Painter MyPaint Brush Editor"));};
}
gboolean gimp_painter_mybrush_editor_set_context(GimpPainterMybrushEditor*editor,GimpContext*context,GError**error) {
  return boundary<gboolean>(error,FALSE,[&]()->gboolean {
    auto lease=PainterEditorRef::retain(editor);
    return store(editor).with<EditorSlot>([&](EditorImpl&i)->gboolean {
      if(i.replacing)throw Error(GIMP_PAINTER_ERROR_INVALID_STATE,"Editor context replacement is already in progress");
      struct Replacing { bool& value; explicit Replacing(bool& v):value(v){value=true;} ~Replacing(){value=false;} } replacing(i.replacing);
      GError*failure=nullptr;
      auto options=ObjectRef<GimpPainterMybrushOptions>::adopt(gimp_painter_mybrush_options_ref_for_context(context,&failure));checked(bool(options),failure);
      auto owned=ObjectRef<GObject>::retain(G_OBJECT(context));
      if(i.closed)throw Error(GIMP_PAINTER_ERROR_CLOSED,"Editor closed while acquiring context");
      if(i.context.get()==G_OBJECT(context)&&i.options.get()==options.get())return TRUE;
      i.detach();
      auto old_widgets=std::move(i.widgets);
      if(i.root)gtk_widget_destroy(i.root);
      i.root=nullptr;i.numeric.clear();i.switches.clear();i.texts.clear();i.custom_range=false;i.curve_setting=i.curve_input=-1;
      if(i.closed)throw Error(GIMP_PAINTER_ERROR_CLOSED,"Editor closed during context replacement");
      i.context=std::move(owned);i.options=std::move(options);build(i);
      return i.closed?FALSE:TRUE;
    });
  });
}
GtkWidget*gimp_painter_mybrush_editor_new(GimpContext*context,gboolean horizontal,GError**error) {
  return boundary<GtkWidget*>(error,nullptr,[&]()->GtkWidget* {
    auto*e=GIMP_PAINTER_MYBRUSH_EDITOR(g_object_new(GIMP_TYPE_PAINTER_MYBRUSH_EDITOR,nullptr));
    auto hold=ObjectRef<GimpPainterMybrushEditor>::sink(e);store(e).with<EditorSlot>([&](EditorImpl&i){i.horizontal=horizontal;});
    if(!gimp_painter_mybrush_editor_set_context(e,context,error))return nullptr;
    g_object_force_floating(G_OBJECT(e));return GTK_WIDGET(hold.release());
  });
}
GtkWidget*gimp_painter_mybrush_editor_popup(GimpContext*context,GtkWidget*parent,GError**error) {
  auto*editor=gimp_painter_mybrush_editor_new(context,TRUE,error);if(!editor)return nullptr;
  auto*window=gtk_window_new(GTK_WINDOW_TOPLEVEL);gtk_window_set_title(GTK_WINDOW(window),_("Painter MyPaint Brush Editor"));gtk_window_set_default_size(GTK_WINDOW(window),860,630);
  if(parent&&GTK_IS_WINDOW(gtk_widget_get_toplevel(parent)))gtk_window_set_transient_for(GTK_WINDOW(window),GTK_WINDOW(gtk_widget_get_toplevel(parent)));
  gtk_window_set_destroy_with_parent(GTK_WINDOW(window),TRUE);gtk_container_add(GTK_CONTAINER(window),editor);gtk_widget_show_all(window);return window;
}
gboolean gimp_painter_mybrush_editor_save(GimpPainterMybrushEditor*editor,const gchar*name,GError**error) {
  return boundary<gboolean>(error,FALSE,[&]()->gboolean {return store(editor).with<EditorSlot>([&](EditorImpl&i)->gboolean {
    if(!i.options)throw Error(GIMP_PAINTER_ERROR_INVALID_STATE,"The editor has no context");
    auto model=i.options;auto context=ObjectRef<GObject>::retain(G_OBJECT(i.ctx()));auto*ctx=GIMP_CONTEXT(context.get());auto*factory=ctx->gimp->painter_mybrush_factory;
    auto selected=ObjectRef<GimpPainterMybrush>::retain(gimp_context_get_painter_mybrush(ctx));if(!selected)throw Error(GIMP_PAINTER_ERROR_INVALID_STATE,"No painter brush is selected");
    GError*failure=nullptr;
    if(name){
      if(!*name||!g_utf8_validate(name,-1,nullptr))throw Error(GIMP_PAINTER_ERROR_INVALID_STATE,"A valid brush name is required");
      auto draft=PainterOptionsRef::retain(model.get()).snapshot();auto copy=PainterMybrushRef::create(name);copy.replace(draft);
      gimp_tagged_set_tags(GIMP_TAGGED(copy.get()),gimp_tagged_get_tags(GIMP_TAGGED(selected.get())));
      auto*container=gimp_data_factory_get_container(factory);
      if(gimp_container_get_child_by_name(container,name))throw Error(GIMP_PAINTER_ERROR_INVALID_STATE,"A painter brush with that name already exists");
      auto ok=gimp_data_factory_data_save_single(factory,GIMP_DATA(copy.get()),&failure);checked(ok,failure);
      gimp_container_add(container,GIMP_OBJECT(copy.get()));
      if(!i.closed&&i.options.get()==model.get()&&gimp_context_get_painter_mybrush(ctx)==selected.get()){gimp_context_set_painter_mybrush(ctx,copy.get());if(i.context&&GIMP_CONTEXT(i.context.get())!=ctx)gimp_context_set_painter_mybrush(GIMP_CONTEXT(i.context.get()),copy.get());}
    }else{
      if(!gimp_data_is_writable(GIMP_DATA(selected.get())))throw Error(GIMP_PAINTER_ERROR_INVALID_STATE,"This brush is read-only; use Save As");
      const auto draft=PainterOptionsRef::retain(model.get()).snapshot().encode();
      auto ok=gimp_painter_mybrush_options_commit(model.get(),&failure);checked(ok,failure);
      ok=gimp_data_factory_data_save_single(factory,GIMP_DATA(selected.get()),&failure);
      if(!ok){
        /* Memory commit is not filesystem success. Keep an unchanged failed
         * save as a draft so selecting another brush still records it. */
        auto*binding=BindingStore::find(G_OBJECT(model.get()));
        if(binding&&binding->state()==BindingStore::State::active&&gimp_context_get_painter_mybrush(ctx)==selected.get()&&PainterOptionsRef::retain(model.get()).snapshot().encode()==draft)
          gimp_painter_mybrush_options_set_json(model.get(),draft.c_str(),nullptr);
      }
      checked(ok,failure);
    }
    if(!i.closed){rebuild_list(i);refresh(i);}
    return TRUE;
  });});
}
gboolean gimp_painter_mybrush_editor_rename(GimpPainterMybrushEditor*editor,const gchar*name,GError**error) {
  return boundary<gboolean>(error,FALSE,[&]()->gboolean {return store(editor).with<EditorSlot>([&](EditorImpl&i)->gboolean {
    if(!i.options)throw Error(GIMP_PAINTER_ERROR_INVALID_STATE,"The editor has no context");
    if(!name||!*name||!g_utf8_validate(name,-1,nullptr))throw Error(GIMP_PAINTER_ERROR_INVALID_STATE,"A valid brush name is required");
    auto selected=ObjectRef<GimpPainterMybrush>::retain(gimp_context_get_painter_mybrush(i.ctx()));if(!selected||!gimp_data_is_writable(GIMP_DATA(selected.get())))throw Error(GIMP_PAINTER_ERROR_INVALID_STATE,"This brush cannot be renamed");
    auto*factory=i.ctx()->gimp->painter_mybrush_factory;auto*other=gimp_container_get_child_by_name(gimp_data_factory_get_container(factory),name);
    if(other&&other!=GIMP_OBJECT(selected.get()))throw Error(GIMP_PAINTER_ERROR_INVALID_STATE,"A painter brush with that name already exists");
    if(!gimp_painter_mybrush_editor_save(editor,nullptr,error))return FALSE;
    std::string old=gimp_object_get_name(selected.get());gimp_object_set_name(GIMP_OBJECT(selected.get()),name);
    if(!gimp_data_factory_data_save_single(factory,GIMP_DATA(selected.get()),error)){gimp_object_set_name(GIMP_OBJECT(selected.get()),old.c_str());return FALSE;}
    if(!i.closed){rebuild_list(i);refresh(i);}
    return TRUE;
  });});
}
gboolean gimp_painter_mybrush_editor_delete(GimpPainterMybrushEditor*editor,GError**error) {
  return boundary<gboolean>(error,FALSE,[&]()->gboolean {return store(editor).with<EditorSlot>([&](EditorImpl&i)->gboolean {
    if(!i.options)throw Error(GIMP_PAINTER_ERROR_INVALID_STATE,"The editor has no context");
    auto model=i.options;auto*ctx=GIMP_CONTEXT(model.get());auto selected=ObjectRef<GimpPainterMybrush>::retain(gimp_context_get_painter_mybrush(ctx));
    if(!selected||!gimp_data_is_deletable(GIMP_DATA(selected.get())))throw Error(GIMP_PAINTER_ERROR_INVALID_STATE,"This brush cannot be deleted");
    auto*factory=ctx->gimp->painter_mybrush_factory;
    /* Native factory deletion removes its item before trying the filesystem.
     * Delete through GimpData first so an I/O failure leaves selection/draft
     * and factory membership intact, then perform the native memory removal. */
    if(gimp_data_get_file(GIMP_DATA(selected.get()))&&!gimp_data_delete_from_disk(GIMP_DATA(selected.get()),error))return FALSE;
    if(!gimp_data_factory_data_delete(factory,GIMP_DATA(selected.get()),FALSE,error))return FALSE;
    auto*standard=gimp_context_get_painter_mybrush(ctx);
    if(!standard||standard==selected.get()){standard=GIMP_PAINTER_MYBRUSH(gimp_painter_mybrush_get_standard(ctx));gimp_context_set_painter_mybrush(ctx,standard);}
    if(!i.closed&&i.context&&GIMP_CONTEXT(i.context.get())!=ctx)gimp_context_set_painter_mybrush(GIMP_CONTEXT(i.context.get()),standard);
    if(!i.closed){rebuild_list(i);refresh(i);}
    return TRUE;
  });});
}
void gimp_painter_mybrush_editor_request_preview(GimpPainterMybrushEditor*editor){boundary_void(nullptr,[&]{store(editor).with<EditorSlot>(request_preview);});}
GdkPixbuf*gimp_painter_mybrush_editor_ref_preview(GimpPainterMybrushEditor*editor){return boundary<GdkPixbuf*>(nullptr,nullptr,[&]{return store(editor).with<EditorSlot>([](EditorImpl&i){return i.preview?GDK_PIXBUF(g_object_ref(i.preview.get())):nullptr;});});}
guint64 gimp_painter_mybrush_editor_preview_revision(GimpPainterMybrushEditor*editor){return boundary<guint64>(nullptr,0,[&]{return store(editor).with<EditorSlot>([](EditorImpl&i){return i.published;});});}
gboolean gimp_painter_mybrush_editor_preview_pending(GimpPainterMybrushEditor*editor){return boundary<gboolean>(nullptr,FALSE,[&]{return store(editor).with<EditorSlot>([](EditorImpl&i)->gboolean{return i.preview_source.active();});});}
