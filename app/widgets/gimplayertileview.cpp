/* GIMP-painter
 * GimpLayerTileView
 * Copyright (C) 2016  seagetch
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "base/delegators.hpp"
#include "base/scopeguard.hpp"
#include "base/glib-cxx-bridge.hpp"
#include "base/glib-cxx-utils.hpp"
#include "base/glib-cxx-impl.hpp"
#include "base/selectcase-utils.hpp"
#include <functional>

extern "C" {
#include "base/temp-buf.h"

#include <config.h>
#include <gegl.h>
#include <gtk/gtk.h>
#include <gdk/gdkpixbuf.h>

#include "widgets-types.h"
#include "widgets/gimpviewrenderer.h"

#include "menus/menus.h"

#include "core/gimpdashpattern.h"
#include "core/gimpmarshal.h"
#include "core/gimp.h"
#include "core/gimpimage.h"
#include "core/gimplayer.h"
#include "core/gimplayermask.h"
#include "core/gimpcontext.h"
#include "core/gimpcontainer.h"
#include "core/gimpdatafactory.h"
#include "core/gimpdata.h"

#include "gimp-intl.h"
#include "gimpwidgets-constructors.h"
#include "gimpwidgets-utils.h"
#include "gimphelp-ids.h"
#include "gimpspinscale.h"
#include "gimpdnd.h"
#include "gimplayerpopup.h"
#include "popupper.h"
#include "gimpmenufactory.h"
#include "gimpuimanager.h"

#include "libgimpcolor/gimpcolor.h"
#include "libgimpwidgets/gimpwidgets.h"

};
#include "presets/gimpjsonresource.h"
#include "presets/layer-preset.h"

#include "base/glib-cxx-def-utils.hpp"

#include "gimplayertileview.h"

using namespace GLib;
using namespace GIMP;

//////////////////////////////////////////////////////////////////////////////////////////////
static const int LAYER_MAX_WIDTH    = 64;
static const int LAYER_MAX_HEIGHT   = 64;
static const int LAYER_MIN_WIDTH    = 56;
static const int LAYER_MIN_HEIGHT   = 56;
static const int LAYER_INDENT_WIDTH = 8;
static const int LAYER_BAR_WIDTH    = 4;
static const int CHECKERBORAD_SIZE  = 4;
static const int ICON_SIZE          = 14;
static const int SCROLL_THRESHOLD   = LAYER_MAX_WIDTH * 2 / 3;
static const int SCROLL_STEP        = LAYER_MAX_WIDTH / 3;
static const int SCROLL_INTERVAL    = 2;
//////////////////////////////////////////////////////////////////////////////////////////////
namespace GLib {
namespace _D = Delegators;


//////////////////////////////////////////////////////////////////////////////////////////////
// Surface creation helpers

static cairo_surface_t* 
build_cairo_surface(TempBuf* temp_buf)
{
  g_return_val_if_fail (temp_buf != NULL, NULL);

  cairo_surface_t* surface = cairo_image_surface_create (CAIRO_FORMAT_RGB24, temp_buf->width, temp_buf->height);

  g_print("gimp_view_render_temp_buf_to_surface\n");
  gimp_view_render_temp_buf_to_surface (
    temp_buf, -1, 
    GIMP_VIEW_BG_CHECKS, GIMP_VIEW_BG_WHITE, 
    surface, temp_buf->width, temp_buf->height);

  return surface;
}


static cairo_surface_t* 
build_cairo_surface(GdkPixbuf* pixbuf)
{
  g_return_val_if_fail (pixbuf != NULL, NULL);

  cairo_surface_t* surface = gimp_cairo_surface_create_from_pixbuf (pixbuf);
  return surface;
}


static cairo_surface_t* 
build_cairo_surface(GtkWidget* widget, const gchar* stock_id, gint width, gint height)
{
  GdkPixbuf   *pixbuf = NULL;
  GtkIconSize  icon_size;

  g_return_val_if_fail (stock_id != NULL, NULL);
  auto i_widget = ref(widget);

  icon_size = i_widget [gimp_get_icon_size] (stock_id, GTK_ICON_SIZE_INVALID,
                                             width, height);

  if (icon_size)
    pixbuf = i_widget [gtk_widget_render_icon] (stock_id, icon_size, NULL);

  if (pixbuf)
    {
      gint  w  = gdk_pixbuf_get_width (pixbuf);
      gint  h = gdk_pixbuf_get_height (pixbuf);

      if (w > width || h > height)
        {
          GdkPixbuf *scaled_pixbuf;

          gimp_viewable_calc_preview_size (w, h, width, height, TRUE, 1.0, 1.0, &w, &h, NULL);
          scaled_pixbuf = gdk_pixbuf_scale_simple (pixbuf, w, h, GDK_INTERP_BILINEAR);

          g_object_unref (pixbuf);
          pixbuf = scaled_pixbuf;
        }
    }
    cairo_surface_t* result = build_cairo_surface(pixbuf);
    g_object_unref (pixbuf);
    return result;
}

//////////////////////////////////////////////////////////////////////////////////////////////
// Class definitions

typedef UseCStructs<GtkBox, GimpLayerTileView> CStructs;

struct LayerTileView : virtual public ImplBase, virtual public LayerTileViewInterface
{
  // Internal class which hold layer information required for display and control.
  struct LayerInfo {
    LayerInfo(LayerTileView* view, GimpViewable* layer, int level);
    ~LayerInfo();

    LayerTileView* view;

    CXXPointer<_D::Connection> added_handler;
    CXXPointer<_D::Connection> removed_handler;
    CXXPointer<_D::Connection> reordered_handler;
    CXXPointer<_D::Connection> mask_changed_handler;

    CXXPointer<_D::Connection> visible_changed_handler;
    CXXPointer<_D::Connection> linked_changed_handler;
    CXXPointer<_D::Connection> lock_content_changed_handler;

    CXXPointer<_D::Connection> apply_changed_handler;
    CXXPointer<_D::Connection> edit_changed_handler;
    CXXPointer<_D::Connection> show_changed_handler;

    IObject<GimpViewable> layer;
    int level;
    int dirty_count;


    struct LayerPreview {
      ScopedPointer<cairo_surface_t, void (cairo_surface_t*), cairo_surface_destroy> surface;
      int width;
      int height;
      LayerInfo* info;
      CXXPointer<_D::Connection> invalidate_preview_handler;
      CXXPointer<_D::Connection> size_changed_handler;
      CXXPointer<Idle>           update_idle;

      LayerPreview(LayerInfo* _info);
      ~LayerPreview();
      void decorate(GimpViewable* layer);
      void update_cairo_surface(GimpViewable* viewable);
      void on_layer_invalidate_preview(GimpViewable* viewable);
      void draw(cairo_t* cr, gint x, gint y, GimpRGB* fg_color, GimpRGB* bg_color);
    };


    LayerPreview layer_preview;
    LayerPreview mask_preview;

    void decorate(GimpViewable* layer);
    void on_changed(GtkWidget* widget);
    void on_mask_changed(GimpLayer* layer);
    void draw(cairo_t* cr, gint x, gint y, GimpRGB* fg_color, GimpRGB* bg_color);
  };


  struct MouseAction {
    enum Action { Hit, Collapse };
    GimpViewable* target;
    Action action;
    gdouble offset_x;
    gdouble offset_y;
  };

  struct DragAction {
    enum Action { InsertBefore, InsertAfter };
    DragAction() : target(NULL), action(InsertBefore) {};
    DragAction(GimpViewable* target, Action action): target(target), action(action) {};
    GimpViewable* source;
    GimpViewable* target;
    Action action;
  };


  Object<GtkScrolledWindow> scrolled_window;
  Object<GtkDrawingArea>    content_area;
  Object<GtkButton>         add_button;
  Object<GtkButton>         anchor_button;
  Object<GtkButton>         delete_button;
  Object<GimpUIManager>     ui_manager;
  IObject<GimpContext>      context;
  IObject<GimpImage>        image;

  List                      layers;
  HashTable                 layer_dict;
  int                       freezed_count;

  CXXPointer<_D::Connection> added_handler;
  CXXPointer<_D::Connection> removed_handler;
  CXXPointer<_D::Connection> reordered_handler;
  CXXPointer<_D::Connection> active_layer_changed_handler;
  ScopedPointer<cairo_surface_t, void (cairo_surface_t*), cairo_surface_destroy> eye_surface;
  CXXPointer<LayerPopupWindow> layer_popup_decorator;
  CXXPointer<_D::Delegator<GimpViewable* (GtkWidget*, GimpContext**)> > drag_viewable_holder; 
  CXXPointer<_D::Connection> drag_motion_handler;
  CXXPointer<_D::Connection> drag_failed_handler;
  CXXPointer<_D::Connection> drag_leave_handler;
  CXXPointer<_D::Connection> drag_drop_handler;
  CXXPointer<_D::Connection> drag_data_received_handler;
  CXXPointer<Timeout>        scroll_timeout_handler;
  
  CXXPointer<DragAction>     drag_action;
  CXXPointer<GdkPoint>       add_cursor;
  CXXPointer<Timeout>        add_timeout_handler;

  LayerTileView(GObject* o);  
  virtual ~LayerTileView();

  static void class_init(CStructs::Class* klass);

  CopyValue get_image();
  void      set_image(IValue v);
  CopyValue get_context();
  void      set_context(IValue v);

  void on_expose(GtkDrawingArea *widget, GdkEventExpose *event);
  void draw(GtkDrawingArea* drawing_area, cairo_t* cr, int width, int height); //gpointer user_data
  void on_layer_added(GimpContainer* container, GimpViewable* layer);
  void on_layer_removed(GimpContainer* container, GimpViewable* layer);
  void on_layer_reordered(GimpContainer* container, GimpViewable* layer, gint index);
  gboolean on_button_press(GtkWidget* widget, GdkEventButton* event);
  gboolean on_drag_motion(GtkWidget* widget, GdkDragContext* context, gint x, gint y, guint time_);
  gboolean on_drag_drop(GtkWidget* widget, GdkDragContext* context, gint x, gint y, guint time_);
  void on_drag_leave(GtkWidget* widget, GdkDragContext* context, guint time_);
  void on_drag_data_received(GtkWidget* widget, GdkDragContext* context, gint x, gint y, GtkSelectionData* selection_data, guint info, guint time);
  gboolean on_add_button_press(GtkWidget* widget, GdkEventButton* event);
  gboolean on_add_button_motion(GtkWidget* widget, GdkEventMotion* event);
  gboolean on_add_button_release(GtkWidget* widget, GdkEventButton* event);
  void on_changed(GtkWidget* widget);

  void reset_layers();
  void invalidate_dirty();
  GdkRectangle  get_boundary(GimpViewable* viewable);
  MouseAction get_viewable_at(gint x, gint y);

  DragAction* get_drag_action(GtkWidget* widget, GdkDragContext* context, gint x, gint y, guint time_);

  GimpViewable* get_drag_viewable(GtkWidget *widget, GimpContext **context);
  void popup_layer_operation();

  // Inherited methods
  virtual void constructed  ();
  virtual void dummy() {};
};

//////////////////////////////////////////////////////////////////////////////////////////////
// Bridge between C++ class and Gtk-compatible class information

extern const char gimp_layer_tile_view_name[] = "GimpLayerTileView";
using Class = NewGClass<gimp_layer_tile_view_name, CStructs, LayerTileView>;
static Class class_instance;

#define _override(method) Class::__(&klass->method).bind<&LayerTileView::method>()
#define _getter(method)   Class::__((CopyValue (**)(GObject*))NULL).bind<&LayerTileView::get_##method >()
#define _setter(method)   Class::__((void (**)(GObject*, IValue))NULL).bind<&LayerTileView::set_##method >()

void 
LayerTileView::class_init(CStructs::Class *klass)
{
  class_instance.with_class(klass)->
      as_class<GObject>([&](GObjectClass* klass){
        _override (constructed);

      })->install_property(
          Class::g_param_spec_object("image", GIMP_TYPE_IMAGE, (GParamFlags)(GIMP_PARAM_READWRITE|G_PARAM_CONSTRUCT) ),
          _getter(image), _setter(image)
      )->install_property(
          Class::g_param_spec_object("context", GIMP_TYPE_CONTEXT, (GParamFlags)(GIMP_PARAM_READWRITE|G_PARAM_CONSTRUCT) ),
          _getter(context), _setter(context)
      );
}

}; // namespace

//////////////////////////////////////////////////////////////////////////////////////////////
// Implementation of LayerTileView class
//////////////////////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////////////////////
// Class LayerTileView
//////////////////////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////////////////////
// Constructors / Destructors

LayerTileView::LayerTileView(GObject* o) : 
    ImplBase(o), 
    layers(NULL), 
    layer_dict(g_hash_table_new(g_direct_hash, g_direct_equal))
{
  freezed_count = 0;
}


LayerTileView::~LayerTileView()
{
  reset_layers();
}


void 
LayerTileView::constructed ()
{
  ui_manager = gimp_menu_factory_manager_new (global_menu_factory, "<Dock>", g_object, false);//last is tear-off.
  GimpActionGroup* group = gimp_ui_manager_get_action_group (ui_manager, "layers");


  auto setup_action_button = [] (GimpActionGroup* group, const char* action_name, IObject<GtkWidget> button, GtkIconSize button_icon_size) {

    GtkAction* action = gtk_action_group_get_action (GTK_ACTION_GROUP (group), action_name);
    gtk_activatable_set_related_action (GTK_ACTIVATABLE(button.ptr()), action);

    button [gtk_button_set_relief] (GTK_RELIEF_NONE);    
    button [gtk_widget_show] ();

    const gchar* stock_id = gtk_action_get_stock_id (action);
    gchar*       tooltip  = g_strdup (gtk_action_get_tooltip (action));
    const gchar* help_id  = (const char*)g_object_get_qdata (G_OBJECT(action), GIMP_HELP_ID);

    GtkWidget* old_child = button [gtk_bin_get_child] ();

    if (old_child)
      gtk_widget_destroy (old_child);

    GtkWidget* button_image = gtk_image_new_from_stock (stock_id, button_icon_size);
    button [gtk_container_add] (button_image);
    ref(button_image) [gtk_widget_show] ();
  };


  ref(g_object) [gtk_orientable_set_orientation] (GTK_ORIENTATION_VERTICAL);

  with (ref(GTK_BOX(g_object)), [&](auto self) {
    
    self.pack_start(false, false, 0) (gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0), [&](auto box) {

      box.pack_start(false, false, 0) (gtk_button_new(), [&] (auto button) {

        anchor_button = GTK_BUTTON (button.ptr());
        setup_action_button(group, "layers-anchor", button, GTK_ICON_SIZE_MENU);

      }).pack_end(false, false, 0) (gtk_button_new(), [&] (auto button) {
        
        delete_button = GTK_BUTTON (button.ptr());
        setup_action_button(group, "layers-delete", button, GTK_ICON_SIZE_MENU);

      });
      box [gtk_widget_show] ();

    }).pack_start(false, false, 0) (gimp_button_new(), [&] (auto button) {

      add_button = GTK_BUTTON (button.ptr());
      setup_action_button(group, "layers-new-last-values", button, GTK_ICON_SIZE_LARGE_TOOLBAR);
      button [gtk_widget_set_events] (GDK_ALL_EVENTS_MASK);

      button.connect_noret("button-press-event",   _D::delegator(this, &LayerTileView::on_add_button_press));
      button.connect_noret("motion-notify-event",  _D::delegator(this, &LayerTileView::on_add_button_motion));
      button.connect_noret("button-release-event", _D::delegator(this, &LayerTileView::on_add_button_release));

    }).pack_start(true, true, 0) (GTK_SCROLLED_WINDOW (gtk_scrolled_window_new (NULL, NULL)), [&] (auto window) {

      scrolled_window     = with(window, [this](auto window) {

        content_area = window.add_with_viewport(GTK_DRAWING_AREA (gtk_drawing_area_new ()), [this] (auto i_content_area){
          i_content_area [gtk_widget_set_size_request] (LAYER_MAX_WIDTH, LAYER_MAX_HEIGHT);
          i_content_area [gtk_widget_set_events] (GDK_ALL_EVENTS_MASK);

          i_content_area.connect_noret("expose-event",       _D::delegator(this, &LayerTileView::on_expose));
          i_content_area.connect_noret("button-press-event", _D::delegator(this, &LayerTileView::on_button_press));
          
          i_content_area [gtk_widget_show] ();
        }).ptr();
        
        window [gtk_widget_set_size_request] (LAYER_MAX_WIDTH + 16, LAYER_MAX_HEIGHT);
        window [gtk_widget_show] ();
      }).ptr();

    });
  });
}


//////////////////////////////////////////////////////////////////////////////////////////////
// Setter / getter for GObject properties
//////////////////////////////////////////////////////////////////////////////////////////////


CopyValue 
LayerTileView::get_image()
{
  CopyValue result = G_OBJECT(image.ptr());
  GObject* object = result;
  return result;
}


void 
LayerTileView::set_image(IValue v)
{
  const GValue* val = v.ptr();
  GimpImage* image = GIMP_IMAGE(g_value_get_object(val));

  if (this->image.ptr() == image)
    return;

  if (this->image) {
    added_handler     = NULL;
    removed_handler   = NULL;
    reordered_handler = NULL;
  }
  this->image = image;

  reset_layers();
  if (this->image) {

    GimpContainer* container = ref(image) [gimp_image_get_layers] ();
    IGimpContainer<GimpViewable> i_container = ref(container);

    added_handler     = i_container.connect("add",     _D::delegator(this, &LayerTileView::on_layer_added));
    removed_handler   = i_container.connect("remove",  _D::delegator(this, &LayerTileView::on_layer_removed));
    reordered_handler = i_container.connect("reorder", _D::delegator(this, &LayerTileView::on_layer_reordered));

    active_layer_changed_handler = this->image.connect("active-layer-changed", _D::delegator(this, &LayerTileView::on_changed));

    int level = 0;
    int num_layers = 0;

    i_container.each([&] (GimpViewable* viewable) {
      on_layer_added(container, viewable);
    });

    IList<LayerInfo*> i_layers     = layers;
    int max_level = 0;
    for (LayerInfo* info: i_layers){
      max_level = std::max(info->level, max_level);
    };

    auto i_content_area = ref(content_area);
    num_layers = g_list_length (layers.ptr());
    i_content_area [gtk_widget_set_size_request] (max_level * LAYER_INDENT_WIDTH + LAYER_MAX_WIDTH, LAYER_MAX_HEIGHT * num_layers);

    drag_viewable_holder = _D::delegator(this, &LayerTileView::get_drag_viewable);
    i_content_area [gimp_dnd_viewable_source_add] (
      i_container [gimp_container_get_children_type] (),
      std::remove_reference<decltype(*drag_viewable_holder.ptr())>::type::callback,
      drag_viewable_holder.ptr());
    
    i_content_area [gimp_dnd_drag_dest_set_by_type] (
      GtkDestDefaults(0),
      GIMP_TYPE_LAYER,
      GdkDragAction(GDK_ACTION_MOVE | GDK_ACTION_COPY));    
    i_content_area [gimp_dnd_viewable_dest_add] (
                                GIMP_TYPE_CHANNEL,
                                NULL,this);
    i_content_area [gimp_dnd_viewable_dest_add] (
                                GIMP_TYPE_LAYER_MASK,
                                NULL,this);

    drag_motion_handler        = i_content_area.connect("drag-motion",        _D::delegator(this, &LayerTileView::on_drag_motion));
    drag_failed_handler        = i_content_area.connect("drag-failed",        _D::delegator(this, &LayerTileView::on_drag_leave));
    drag_leave_handler         = i_content_area.connect("drag-leave",         _D::delegator(this, &LayerTileView::on_drag_leave));
    drag_drop_handler          = i_content_area.connect("drag-drop",          _D::delegator(this, &LayerTileView::on_drag_drop));
    drag_data_received_handler = i_content_area.connect("drag-data-received", _D::delegator(this, &LayerTileView::on_drag_data_received));

  } else {
    ref(content_area) [gtk_widget_set_size_request] (LAYER_MAX_WIDTH, LAYER_MAX_HEIGHT);
    ref(g_object) [gtk_widget_queue_draw] ();
  }
}


GimpViewable *
LayerTileView::get_drag_viewable (GtkWidget    *widget,
                                  GimpContext **context_holder)
{
  if (context_holder)
    *context_holder = context.ptr();

  GimpLayer* layer = image [gimp_image_get_active_layer] ();

  return GIMP_VIEWABLE(layer);
}


CopyValue 
LayerTileView::get_context()
{
  CopyValue result = G_OBJECT(context.ptr());
  GObject* object = result;
  return result;
}


void 
LayerTileView::set_context(IValue v) 
{
  const GValue* val = v.ptr();
  GimpContext* context = GIMP_CONTEXT(g_value_get_object(val));
  this->context = context;
}

//////////////////////////////////////////////////////////////////////////////////////////////
// Event handlers
//////////////////////////////////////////////////////////////////////////////////////////////

void 
LayerTileView::on_expose(GtkDrawingArea *widget, GdkEventExpose *event)
{
  auto i_widget = ref(widget);
  cairo_t* cr = gdk_cairo_create (i_widget [gtk_widget_get_window] ());
  int width = event->area.width;
  int height = event->area.height;
  gdk_cairo_rectangle (cr, &event->area);
  draw(widget, cr, width, height);
  cairo_destroy(cr);
}


void 
LayerTileView::on_layer_added(GimpContainer* container, GimpViewable* layer)
{
  IGimpContainer<GimpViewable*> i_container = container;
  auto i_layer           = ref(layer);
  auto i_layer_dict      = ref<GimpViewable*, GList*>(this->layer_dict);
  IList<LayerInfo*> i_layers = this->layers;

  gint index = i_container [gimp_container_get_child_index] (GIMP_OBJECT (layer));
  GimpViewable* parent = i_layer [gimp_viewable_get_parent] ();

  GList* parent_list = i_layer_dict[parent];
  LayerInfo* parent_info = parent_list? reinterpret_cast<LayerInfo*>(parent_list->data): NULL;

  LayerInfo* layer_info;
  if (parent_info) {
    layer_info = new LayerInfo(this, layer, parent_info->level + 1);
    // FIXME: ancestor's layer preview must be updated recursively here.
    parent_info->dirty_count ++;
    layer_info->dirty_count ++;
  } else {
    layer_info = new LayerInfo(this, layer, 0);
    layer_info->dirty_count ++;
    index --;
  }


  GList* sibling = parent_list? parent_list: layers.ptr();
  for (;index >= 0; index --) {
    if (!sibling)
      break;
    sibling = sibling->next;
    while (sibling && sibling->data && reinterpret_cast<LayerInfo*>(sibling->data)->level > layer_info->level)
      sibling = sibling->next;
  }
  i_layers.insert_before(sibling, layer_info);

  GList* new_item = g_list_find (layers.ptr(), layer_info);
  i_layer_dict.insert(layer, new_item);

  layer_info->decorate(layer);

  GimpContainer* children = gimp_viewable_get_children (layer);
  if (children) {
    IGimpContainer<GimpViewable> i_children(children);
    layer_info->added_handler     = i_children.connect("add",     _D::delegator(this, &LayerTileView::on_layer_added));
    layer_info->removed_handler   = i_children.connect("remove",  _D::delegator(this, &LayerTileView::on_layer_removed));
    layer_info->reordered_handler = i_children.connect("reorder", _D::delegator(this, &LayerTileView::on_layer_reordered));
    freezed_count ++;
    i_children.each([&](GimpViewable* viewable) {
      on_layer_added(children, viewable);
    });
    freezed_count --;
  } else {
  }

  if (freezed_count == 0) {
    invalidate_dirty();
  }
//    ref(content_area) [gtk_widget_queue_draw] ();
}


void 
LayerTileView::on_layer_removed(GimpContainer* container, GimpViewable* layer)
{
  IGimpContainer<GimpViewable*> i_container = container;
  auto i_layer           = ref(layer);
  auto i_layer_dict      = ref<GimpViewable*, GList*>(this->layer_dict);
  IList<LayerInfo*> i_layers = this->layers;
  
  gint index = i_container [gimp_container_get_child_index] (GIMP_OBJECT (layer));

  GList* self_list = i_layer_dict[layer];
  g_return_if_fail (self_list != NULL);

  LayerInfo* layer_info = reinterpret_cast<LayerInfo*>(self_list->data);
  i_layer_dict.remove(layer);
  i_layers.remove(layer_info);

  GimpContainer* children = gimp_viewable_get_children (layer);
  if (children) {
    IGimpContainer<GimpViewable> i_children(children);
    freezed_count ++;
    i_children.each([&](GimpViewable* viewable) {
      on_layer_removed(children, viewable);
    });
    freezed_count --;
  }
  delete layer_info;

  if (freezed_count == 0) {
    invalidate_dirty();    
  }
//    ref(content_area) [gtk_widget_queue_draw] ();
}


void 
LayerTileView::on_layer_reordered(GimpContainer* container, GimpViewable* layer, gint index)
{
  freezed_count ++;
  on_layer_removed(container, layer);
  on_layer_added(container, layer);
  freezed_count --;

  if (freezed_count == 0) {
    invalidate_dirty();    
  }
//    ref(content_area) [gtk_widget_queue_draw] ();
}


void 
LayerTileView::on_changed(GtkWidget* widget)
{
  ref(scrolled_window) [gtk_widget_queue_draw] ();
}


gboolean 
LayerTileView::on_button_press(GtkWidget* widget, GdkEventButton* event)
{
  // Debug
  g_print("button=%x, x, y = %d, %d\n", event->button, (int)event->x, (int)event->y);

  // Obtaining viewable for mouse position.
  MouseAction action = get_viewable_at(event->x, event->y);
  
  if (action.target && action.action == MouseAction::Hit) {
    auto i_viewable = ref(action.target);

    if (event->button == 1 || event->button == 3) {
      GdkRectangle visibility_area = {0, 0, ICON_SIZE, ICON_SIZE};

      if (visibility_area.x <= action.offset_x && action.offset_x < visibility_area.x + visibility_area.width &&
          visibility_area.x <= action.offset_y && action.offset_y < visibility_area.y + visibility_area.height) {

        // Check visibility
        gboolean active = i_viewable [gimp_item_get_visible] ();
        i_viewable [gimp_item_set_visible] (!active, TRUE);

      } else {
        // Activate item
        image [gimp_image_set_active_layer] (GIMP_LAYER(action.target));
      }

      ref(scrolled_window) [gtk_widget_queue_draw] ();
      image [gimp_image_flush] ();
    }
    
    if (event->button == 3) {
      GdkRectangle area = {event->x - action.offset_x, event->y - action.offset_y, LAYER_MIN_WIDTH, LAYER_MIN_HEIGHT };
      layer_popup_decorator = new LayerPopupWindow;
      GtkWidget* view = NULL;
      layer_popup_decorator->create_view(GTK_WIDGET(g_object), &view, action.target);
      GtkWidget* popover = PopoverInterface::new_instance(view);
      auto i_popover     = PopoverInterface::cast(popover);
      i_popover->show_over(GTK_WIDGET(content_area.ptr()), &area);

      //decorator(g_object, window_decor);
      //decorate_popover(g_object, Delegators::delegator(window_decor, &LayerPopupWindow::create_view));

    }
  } else {

  }
  return false;
}


gboolean 
LayerTileView::on_drag_motion(GtkWidget* widget, GdkDragContext* drag_context, gint x, gint y, guint time_) 
{
  auto i_content_area = ref(content_area);
  auto i_window       = ref(scrolled_window);
  DragAction* action  = get_drag_action(widget, drag_context, x, y, time_);
  GtkAdjustment* adj  = i_window [gtk_scrolled_window_get_vadjustment] ();

  drag_action = action;

  GtkAllocation alloc;
  i_content_area [gtk_widget_get_allocation] (&alloc);

  gint offset_y = y - gtk_adjustment_get_value (adj);
  gint alloc_h  = gtk_adjustment_get_page_size (adj);

  if (offset_y < SCROLL_THRESHOLD || alloc_h - offset_y < SCROLL_THRESHOLD) {
    int distance = std::max(std::min(std::abs(SCROLL_THRESHOLD - offset_y), std::abs(SCROLL_THRESHOLD + offset_y - alloc_h)), 1);
    int sign     = offset_y < SCROLL_THRESHOLD ? -1: 1;
    int interval = SCROLL_INTERVAL * std::max(1, SCROLL_THRESHOLD - distance);

    std::function<gboolean()> on_timeout = [&, distance, sign] () -> gboolean 
    {
      auto           i_window  = ref(scrolled_window);
      GtkAdjustment* adj       = i_window [gtk_scrolled_window_get_vadjustment] ();
      gdouble        new_value = gtk_adjustment_get_value (adj) + sign * SCROLL_STEP;

      new_value = CLAMP (new_value,
                        gtk_adjustment_get_lower (adj),
                        gtk_adjustment_get_upper (adj) -
                        gtk_adjustment_get_page_size (adj));

      gtk_adjustment_set_value (adj, new_value);
      return true;
    };

    if (scroll_timeout_handler) {
      scroll_timeout_handler = NULL;
    }
    scroll_timeout_handler = new Timeout(interval, _D::delegator(on_timeout));
  } else {
    if (scroll_timeout_handler) {
      scroll_timeout_handler = NULL;
    }
  }


  if (!action) {
    return false;
  }

  gdk_drag_status(drag_context, GDK_ACTION_MOVE, time_);
  ref(scrolled_window) [gtk_widget_queue_draw] ();

  return true; 
}


void 
LayerTileView::on_drag_leave(GtkWidget* widget, GdkDragContext* drag_context, guint time_)
{
  g_print("drag_leave\n");
  drag_action = NULL;
  if (scroll_timeout_handler) {
    scroll_timeout_handler = NULL;
  }
  ref(scrolled_window) [gtk_widget_queue_draw] ();
}


gboolean 
LayerTileView::on_drag_drop(GtkWidget* widget, GdkDragContext* drag_context, gint x, gint y, guint time_)
{
  g_print("drag_drop\n");
  bool success = true;

  CXXPointer<DragAction> action = get_drag_action(widget, drag_context, x, y, time_);

  if (!action) {
    g_print("drag_drop: no action\n");
    success = false;

  } else {

    if (true) { 
      // case when dropped viewable(layer).
      // Do actual reordering and / or insertion.
      g_print("drag_drop: execute reorder\n");

      gint dest_index = -1;

      if (image != ref(action->source) [gimp_item_get_image] () ||
          ! g_type_is_a (G_TYPE_FROM_INSTANCE (action->source), GIMP_TYPE_VIEWABLE)) {
        // TBD: dropped from another image or another format. data source must be converted into Viewable.
#if 0
        GType     item_type = item_view_class->item_type;
        GimpItem *new_item;
        GimpItem *parent;

        if (g_type_is_a (G_TYPE_FROM_INSTANCE (src_viewable), item_type))
          item_type = G_TYPE_FROM_INSTANCE (src_viewable);

        dest_index = gimp_item_tree_view_get_drop_index (item_view, dest_viewable,
                                                        drop_pos,
                                                        (GimpViewable **) &parent);

        new_item = gimp_item_convert (GIMP_ITEM (src_viewable),
                                      item_view->priv->image, item_type);

        gimp_item_set_linked (new_item, FALSE, FALSE);

        item_view_class->add_item (item_view->priv->image, new_item,
                                  parent, dest_index, TRUE);
#endif
      } else if (action->target) {
        // Reorder layers within the same image.
        auto      source      = ref(action->source);
        auto      target      = ref(action->target);
        GimpItem* src_parent  = GIMP_ITEM(source [gimp_viewable_get_parent] ());
        int       src_index   = source [gimp_item_get_index] ();

        GimpItem* dest_parent = GIMP_ITEM(target [gimp_viewable_get_parent] ());
        int       dest_index  = target [gimp_item_get_index] ();

        if (action->action == DragAction::InsertAfter) {
          IGimpContainer<GimpViewable*> i_container = target [gimp_viewable_get_children] ();
          if (i_container && i_container [gimp_container_get_n_children] () == 0) {
            dest_parent = GIMP_ITEM(target.ptr());
            dest_index = 0;
          } else {
            dest_index ++;
          }
        }

        if (src_parent == dest_parent) {
          if (src_index < dest_index)
            dest_index--;
        }

        image [gimp_image_reorder_item] (
          GIMP_ITEM (action->source),
          dest_parent, dest_index, TRUE, NULL);
      }

      image [gimp_image_flush] ();

    } else { // TBD: case when dropped various data source.
      // TBD: required target (GdkAtom)
      //gtk_drag_get_data (widget, context, target, time);
      g_print("drag_drop: drop from data source\n");
    }
  }

  gtk_drag_finish (drag_context, success, FALSE, time_);
  ref(scrolled_window) [gtk_widget_queue_draw] ();
  return success;
}


void 
LayerTileView::on_drag_data_received(
    GtkWidget* widget, GdkDragContext* context, gint x, gint y, 
    GtkSelectionData* selection_data, guint info, guint time)
{
  g_print("on_drag_data_received\n");

}


gboolean 
LayerTileView::on_add_button_press(GtkWidget* widget, GdkEventButton* event)
{
  if (image) {

    if (event->button == 1) {
      add_cursor = new GdkPoint;
      add_cursor->x = event->x;
      add_cursor->y = event->y;
      add_timeout_handler = new Timeout(500, _D::delegator(std::function<gboolean()>([this]() -> gboolean {
        popup_layer_operation();
        add_timeout_handler = NULL;
        return false;
      })));
    } else if (event->button == 3) {
      popup_layer_operation();
    }
  }
  return false;
}


gboolean 
LayerTileView::on_add_button_motion(GtkWidget* widget, GdkEventMotion* event)
{
  if (image) {
    if (add_cursor) {
      if (event->y - add_cursor->y > 12) {
        popup_layer_operation();
        add_timeout_handler = NULL;
      }
    }
  }
  return false;
}


gboolean 
LayerTileView::on_add_button_release(GtkWidget* widget, GdkEventButton* event)
{
  if (image) {
    if (add_cursor)
      add_cursor = NULL;
    if (add_timeout_handler) {
      add_timeout_handler = NULL;
      return false;
    }
  }
  return true;
}


//////////////////////////////////////////////////////////////////////////////////////////////
// Other functions
//////////////////////////////////////////////////////////////////////////////////////////////


void 
LayerTileView::draw(GtkDrawingArea * widget, cairo_t* cr, int width, int height)
{
//  g_print("LayerTileView::draw(%d, %d)\n", width, height);
  IList<LayerInfo*> i_layers = layers;
  auto i_layer_dict      = ref<GimpViewable*, GList*>(this->layer_dict);
  
  GimpRGB color1 = { 0.8, 0.8, 0.8, 1};
  GimpRGB color2 = { 0.9, 0.9, 0.9, 1};
  GtkStyle* style = ref(g_object) [gtk_widget_get_style] ();
  GimpRGB highlight_color;
  GimpRGB bg_color;
  GimpRGB fg_color;
  gimp_rgb_set_gdk_color (&highlight_color, &style->bg[GTK_STATE_SELECTED]);
  gimp_rgb_set_gdk_color (&bg_color,        &style->light[GTK_STATE_NORMAL]);
  gimp_rgb_set_gdk_color (&fg_color,        &style->fg[GTK_STATE_NORMAL]);
  
  GimpViewable* active_layer = GIMP_VIEWABLE(image [gimp_image_get_active_layer]());

  cairo_pattern_t* pattern =  gimp_cairo_checkerboard_create (cr, CHECKERBORAD_SIZE, &color1, &color2);

  if (!eye_surface) {
    eye_surface = build_cairo_surface(GTK_WIDGET(g_object), GIMP_STOCK_VISIBLE, ICON_SIZE, ICON_SIZE);
  }

  cairo_set_source_rgb (cr, bg_color.r, bg_color.g, bg_color.b);
  cairo_rectangle ( cr, 0, 0, width, height );
  cairo_fill (cr);

  int i = 0;
  int max_level = 0;
  for (auto layer_info: i_layers) {
    max_level = MAX(max_level, layer_info->level);
    if (drag_action) {
      if (layer_info->layer == drag_action->target) {

        // Drawing insertion point of layers if layer is dragged.
        
        switch (drag_action->action) {
        case DragAction::InsertBefore: {
            cairo_rectangle (cr, 
              layer_info->level * LAYER_INDENT_WIDTH, 
              i* LAYER_MAX_HEIGHT - (LAYER_MAX_HEIGHT - LAYER_MIN_HEIGHT) / 2, 
              LAYER_MAX_WIDTH, 
              (LAYER_MAX_HEIGHT - LAYER_MIN_HEIGHT));
            cairo_set_source_rgb (cr, highlight_color.r, highlight_color.g, highlight_color.b);
            cairo_fill (cr);
          }
          break;
        
        case DragAction::InsertAfter: {
            cairo_rectangle (cr, 
              layer_info->level * LAYER_INDENT_WIDTH, 
              (i + 1) * LAYER_MAX_HEIGHT - (LAYER_MAX_HEIGHT - LAYER_MIN_HEIGHT) / 2, 
              LAYER_MAX_WIDTH, 
              (LAYER_MAX_HEIGHT - LAYER_MIN_HEIGHT));
            cairo_set_source_rgb (cr, highlight_color.r, highlight_color.g, highlight_color.b);
            cairo_fill (cr);
          }
          break;
        }

      }

    } else {
      
      // Drawing background highlight for active layer.
      
      if (layer_info->layer == active_layer) {
        cairo_rectangle (cr, layer_info->level * LAYER_INDENT_WIDTH, i* LAYER_MAX_HEIGHT, LAYER_MAX_WIDTH, LAYER_MAX_HEIGHT);
        cairo_set_source_rgb (cr, highlight_color.r, highlight_color.g, highlight_color.b);
        cairo_fill (cr);
      }
    }

    // Drawing indent level indicator.

    for (int j = 0; j < layer_info->level; j ++) {
      cairo_rectangle (cr, j * LAYER_INDENT_WIDTH, i* LAYER_MAX_HEIGHT, LAYER_BAR_WIDTH, LAYER_MAX_HEIGHT);
      cairo_set_source_rgb (cr, 0.75, 0.75, 0.75);
      cairo_fill (cr);
    }

    // Drawing checkerboard background.

    cairo_rectangle (cr, 
      layer_info->level * LAYER_INDENT_WIDTH, 
      i* LAYER_MAX_HEIGHT + (LAYER_MAX_HEIGHT - LAYER_MIN_HEIGHT) / 2, 
      LAYER_MIN_WIDTH, 
      LAYER_MIN_HEIGHT);
    cairo_set_source (cr, pattern);
    cairo_fill_preserve (cr);

    if (context) {
      gint x = layer_info->level * LAYER_INDENT_WIDTH;
      gint y = i* LAYER_MAX_HEIGHT + (LAYER_MAX_HEIGHT - LAYER_MIN_HEIGHT) / 2;
      layer_info->draw(cr, x, y, &fg_color, &bg_color);
    }

    i ++;
  };
  ref(content_area) [gtk_widget_set_size_request] (max_level * LAYER_INDENT_WIDTH + LAYER_MAX_WIDTH, LAYER_MAX_HEIGHT * i);
  cairo_pattern_destroy (pattern);
}


void 
LayerTileView::reset_layers() 
{
  IList<LayerInfo*> i_layers = layers;
  IHashTable<GimpViewable*, GList*> i_layer_dict(layer_dict);

  i_layer_dict.remove_all();
  for (auto layer_info: i_layers) {
    delete layer_info;
  };
  layers.free();
}


void
LayerTileView::invalidate_dirty()
{
  IList<LayerInfo*> i_layers = layers;
  for (LayerInfo* layer_info: i_layers) {
    if (layer_info->dirty_count > 0) {
      layer_info->layer_preview.on_layer_invalidate_preview (GIMP_VIEWABLE(layer_info->layer.ptr()));
    }
  };
}

//////////////////////////////////////////////////////////////////////////////////////////////
// Conversion between mouse cursor position and target viewable

LayerTileView::MouseAction 
LayerTileView::get_viewable_at(gint x, gint y) 
{
  GdkRectangle hit_test_boundary = { 0, (LAYER_MAX_HEIGHT - LAYER_MIN_HEIGHT) / 2, LAYER_MIN_WIDTH, LAYER_MIN_HEIGHT };
  IList<LayerInfo*> i_layers     = layers;

  MouseAction result = {NULL, MouseAction::Hit, 0, 0};

  for (auto layer_info: i_layers) {
    hit_test_boundary.x = layer_info->level * LAYER_INDENT_WIDTH;
    if (hit_test_boundary.x <= x && x < hit_test_boundary.x + hit_test_boundary.width &&
        hit_test_boundary.y <= y && y < hit_test_boundary.y + hit_test_boundary.height) {
      result.target   = layer_info->layer;
      result.action   = MouseAction::Hit;
      result.offset_x = x - hit_test_boundary.x;
      result.offset_y = y - hit_test_boundary.y;
      return result;
    }
    hit_test_boundary.y += LAYER_MAX_HEIGHT;
  };
  return result;
}


GdkRectangle 
LayerTileView::get_boundary(GimpViewable* viewable)
{
  if (GIMP_IS_LAYER_MASK(viewable))
    viewable = GIMP_VIEWABLE(gimp_layer_mask_get_layer (GIMP_LAYER_MASK(viewable)));

  IList<LayerInfo*> i_layers     = layers;
  GdkRectangle result = {0, (LAYER_MAX_HEIGHT - LAYER_MIN_HEIGHT) / 2, LAYER_MIN_WIDTH, LAYER_MIN_HEIGHT };
  for (auto layer_info: i_layers){
    if (layer_info->layer == viewable) {
      result.x = layer_info->level * LAYER_INDENT_WIDTH;
      return result;

    } else {
      result.y += LAYER_MAX_HEIGHT;
    }
  };
  return {-1, -1, -1, -1};
}

//////////////////////////////////////////////////////////////////////////////////////////////
// Handling drag&drop

LayerTileView::DragAction* 
LayerTileView::get_drag_action(GtkWidget* widget, GdkDragContext* drag_context, gint x, gint y, guint time_)
{
  auto i_content_area = ref(content_area);

  // Getting dragged source.

  GtkTargetList* target_list;
  GdkAtom        target_atom;
  GimpDndType    src_type;
  GimpViewable*  source = NULL;

  target_list = i_content_area [gtk_drag_dest_get_target_list] ();
  target_atom = i_content_area [gtk_drag_dest_find_target] (drag_context, target_list);

  if (! gtk_target_list_find (target_list, target_atom, (guint*)&src_type)) {
    return NULL;
  }

  switch (src_type) {
  case GIMP_DND_TYPE_URI_LIST:
  case GIMP_DND_TYPE_TEXT_PLAIN:
  case GIMP_DND_TYPE_NETSCAPE_URL:
  case GIMP_DND_TYPE_COLOR:
  case GIMP_DND_TYPE_SVG:
  case GIMP_DND_TYPE_SVG_XML:
  case GIMP_DND_TYPE_COMPONENT:
  case GIMP_DND_TYPE_PIXBUF:
    break;

  default: 
    {
      GtkWidget *src_widget = gtk_drag_get_source_widget (drag_context);
      if (src_widget)
        source = GIMP_VIEWABLE(gimp_dnd_get_drag_data (src_widget));
    }
    break;
  }

  if (!source)
    return NULL;

  // Getting dragged target

  int index    = y / LAYER_MAX_HEIGHT;
  int offset_y = y % LAYER_MAX_HEIGHT;

  IList<LayerInfo*> i_layers = layers;
  LayerInfo* layer_info = i_layers[index];

  if (!layer_info)
    return NULL;

  // Checking whether source can be dropped to target.
  if (ref(source) [gimp_viewable_is_ancestor] (layer_info->layer))
    return NULL;
  

  // returning DragAction

  DragAction* drag_action = new DragAction();
  drag_action->action = DragAction::InsertAfter;
  drag_action->source = source;
  drag_action->target = layer_info->layer;

  if (offset_y < LAYER_MAX_HEIGHT / 2) {
    drag_action->action = DragAction::InsertBefore;
  } else {
    drag_action->action = DragAction::InsertAfter;
  }

  return drag_action; 
}

//////////////////////////////////////////////////////////////////////////////////////////////
// Layer operation popup

void
LayerTileView::popup_layer_operation()
{
  if (image) {

    GimpActionGroup* group = gimp_ui_manager_get_action_group (ui_manager, "layers");

    auto action_item_builder = [group] (GtkWidget* menu, const char* command) {

      GtkWidget* item = with(gtk_image_menu_item_new(), [group, command](auto menu_item) {
        GtkAction* action = gtk_action_group_get_action (GTK_ACTION_GROUP (group), command);
        gtk_activatable_set_related_action (GTK_ACTIVATABLE(menu_item.ptr()), action);

        const gchar* stock_id   = gtk_action_get_stock_id (action);
        GtkWidget* button_image = gtk_image_new_from_stock (stock_id, GTK_ICON_SIZE_MENU);

        menu_item [gtk_image_menu_item_set_image] (button_image);
        menu_item [gtk_widget_show] ();
        menu_item [gtk_image_menu_item_set_always_show_image] (TRUE);
      }).ptr();

      gtk_menu_shell_append (GTK_MENU_SHELL(menu), item);
    };

    auto menu = with(GTK_MENU(gtk_menu_new()), [this, group, &action_item_builder](auto menu) {
      action_item_builder(menu, "layers-new");
      action_item_builder(menu, "layers-new-group");
      action_item_builder(menu, "layers-duplicate");
      action_item_builder(menu, "layers-new-filter");
      action_item_builder(menu, "layers-new-clone");

      auto separator = ref(gtk_separator_menu_item_new());
      separator [gtk_widget_show] ();
      gtk_menu_shell_append (GTK_MENU_SHELL(menu.ptr()), separator.ptr());

      GimpDataFactory* factory;
      if (context)
        factory = gimp_get_data_factory (context->gimp, "layer-preset");
      IGimpContainer<GimpData> i_container = ref(gimp_data_factory_get_container(factory));

      i_container.each([&](GimpData* preset) {
        auto menu_item = ref(gtk_menu_item_new());
        menu_item [gtk_menu_item_set_label] (ref(preset) [gimp_object_get_name] ());
        menu_item [gtk_widget_show] ();
        gtk_menu_shell_append (GTK_MENU_SHELL(menu.ptr()), menu_item.ptr());

        menu_item.connect_noret("activate", std::function<void(GtkWidget*)>([this, preset](GtkWidget* widget) {
          auto applier = hold(ILayerPresetApplier::new_instance(context.ptr(), GIMP_JSON_RESOURCE(preset)));
          applier->apply_for_active_layer();
        }));
      });

    });

    GtkButton* add_button = this->add_button.ptr();
    std::function<void(GtkMenu*,gint*,gint*,gboolean*)> set_position = [add_button] (GtkMenu* menu, gint *x, gint *y, gboolean* push_in) {
      IObject<GtkButton> button = add_button;
      GtkAllocation alloc;
      button [gtk_widget_get_allocation] (&alloc);
      gdk_window_get_origin (button [gtk_widget_get_window] (), x, y);
      *x += alloc.x;
      *y += alloc.y + alloc.height;
      *push_in = TRUE;
    };
    auto p_set_position = guard(_D::delegator(set_position));

    menu [gtk_menu_popup] (NULL, NULL, std::remove_reference<decltype(*p_set_position.ptr())>::type::callback, p_set_position.ptr(), 0, gtk_get_current_event_time());
    add_cursor = NULL;

  }
}



//////////////////////////////////////////////////////////////////////////////////////////////
// Class LayerInfo
//////////////////////////////////////////////////////////////////////////////////////////////



//////////////////////////////////////////////////////////////////////////////////////////////
// constructors / destructors

LayerTileView::LayerInfo::LayerInfo(LayerTileView* view, GimpViewable* layer, int level) : 
    layer_preview(this), mask_preview(this) 
{
  this->view  = view;
  this->layer = layer;
  this->level = level;
  dirty_count = 0;
}

LayerTileView::LayerInfo::~LayerInfo() {
}

void 
LayerTileView::LayerInfo::decorate(GimpViewable* layer)
{
  auto i_layer = ref(layer);
  visible_changed_handler      = i_layer.connect("visibility-changed",   _D::delegator(this, &LayerTileView::LayerInfo::on_changed));
  linked_changed_handler       = i_layer.connect("linked-changed",       _D::delegator(this, &LayerTileView::LayerInfo::on_changed));
  lock_content_changed_handler = i_layer.connect("lock-content-changed", _D::delegator(this, &LayerTileView::LayerInfo::on_changed));
  mask_changed_handler         = i_layer.connect("mask-changed",         _D::delegator(this, &LayerTileView::LayerInfo::on_mask_changed));

  layer_preview.decorate(GIMP_VIEWABLE(layer));

  GimpLayerMask* mask = i_layer [gimp_layer_get_mask] ();
  if (mask)
    mask_preview.decorate(GIMP_VIEWABLE(mask));
}

//////////////////////////////////////////////////////////////////////////////////////////////
// Event handlers

void 
LayerTileView::LayerInfo::on_changed(GtkWidget* widget)
{
  ref(view->scrolled_window) [gtk_widget_queue_draw] ();
}


void
LayerTileView::LayerInfo::on_mask_changed(GimpLayer* layer)
{
  auto mask = ref ( ref(layer) [gimp_layer_get_mask] ());
  mask_preview.decorate(GIMP_VIEWABLE(mask.ptr()));
  ref(view->scrolled_window) [gtk_widget_queue_draw] ();

  apply_changed_handler = mask.connect("apply-changed", _D::delegator(this, &LayerTileView::LayerInfo::on_changed));
  edit_changed_handler  = mask.connect("edit-changed",  _D::delegator(this, &LayerTileView::LayerInfo::on_changed));
  show_changed_handler  = mask.connect("show-changed",  _D::delegator(this, &LayerTileView::LayerInfo::on_changed));
}

//////////////////////////////////////////////////////////////////////////////////////////////
// Other functions

void
LayerTileView::LayerInfo::draw(cairo_t* cr, gint x, gint y, GimpRGB* fg_color, GimpRGB* bg_color) 
{
  auto i_layer = ref(layer);
  auto i_layer_mask = ref(i_layer [gimp_layer_get_mask]());

  if (i_layer_mask && i_layer_mask [gimp_layer_mask_get_edit] ())
    mask_preview.draw(cr, x, y, fg_color, bg_color);
  else
    layer_preview.draw(cr, x, y, fg_color, bg_color);

  gboolean visible = i_layer [gimp_item_get_visible] ();
  cairo_set_source_surface (cr, view->eye_surface, x, y);
  cairo_paint (cr);

  if (!visible) {
    cairo_set_source_rgb (cr, fg_color->r, fg_color->g, fg_color->b);
    cairo_move_to (cr, x + ICON_SIZE, y);
    cairo_line_to (cr, x, y + ICON_SIZE);
    cairo_stroke(cr);
  }

  gboolean linked = i_layer [gimp_item_get_linked] ();
  if (linked) {
    ScopedPointer<cairo_surface_t, void (cairo_surface_t*), cairo_surface_destroy> surface = build_cairo_surface(GTK_WIDGET(view->g_object), GIMP_STOCK_LINKED, ICON_SIZE, ICON_SIZE);
    cairo_set_source_surface (cr, surface.ptr(), x, y + ICON_SIZE);
    cairo_paint (cr);
  }

  gboolean has_mask = i_layer [gimp_layer_get_mask] () != NULL;
  if (has_mask) {
    ScopedPointer<cairo_surface_t, void (cairo_surface_t*), cairo_surface_destroy> surface = build_cairo_surface(GTK_WIDGET(view->g_object), GIMP_STOCK_LAYER_MASK, ICON_SIZE, ICON_SIZE);
    cairo_set_source_surface (cr, surface.ptr(), x + LAYER_MIN_WIDTH - ICON_SIZE, y);
    cairo_paint (cr);
  }

  const gchar* stock_id = i_layer [gimp_viewable_get_stock_id] ();
  if (stock_id && strcmp(stock_id, GIMP_STOCK_LAYER) != 0) {
    ScopedPointer<cairo_surface_t, void (cairo_surface_t*), cairo_surface_destroy> surface = build_cairo_surface(GTK_WIDGET(view->g_object), stock_id, ICON_SIZE, ICON_SIZE);
    cairo_set_source_surface (cr, surface.ptr(), x, y + LAYER_MIN_HEIGHT - ICON_SIZE);
    cairo_paint (cr);
  }

  cairo_set_source_rgb (cr, 0.7, 0.7, 0.7);
  cairo_rectangle (cr, x, y, LAYER_MIN_WIDTH, LAYER_MIN_HEIGHT);
  cairo_stroke (cr);

}



//////////////////////////////////////////////////////////////////////////////////////////////
// Class LayerPreview
//////////////////////////////////////////////////////////////////////////////////////////////



//////////////////////////////////////////////////////////////////////////////////////////////
// constructors / destructors

LayerTileView::LayerInfo::LayerPreview::LayerPreview(LayerInfo* _info) : surface(NULL)
{
  info = _info;
  width  = 0;
  height = 0;
};
LayerTileView::LayerInfo::LayerPreview::~LayerPreview() 
{
};

void
LayerTileView::LayerInfo::LayerPreview::decorate(GimpViewable* viewable) 
{
  if (viewable) {
    auto i_layer = ref(viewable);
    size_changed_handler         = i_layer.connect("size-changed",         _D::delegator(this, &LayerTileView::LayerInfo::LayerPreview::on_layer_invalidate_preview));
    invalidate_preview_handler   = i_layer.connect("invalidate-preview",   _D::delegator(this, &LayerTileView::LayerInfo::LayerPreview::on_layer_invalidate_preview));

  } else {
    surface                    = NULL;
    size_changed_handler       = NULL;
    invalidate_preview_handler = NULL;

  }
}

//////////////////////////////////////////////////////////////////////////////////////////////
// Event handler

void 
LayerTileView::LayerInfo::LayerPreview::on_layer_invalidate_preview(GimpViewable* viewable)
{
  bool is_mask = GIMP_IS_LAYER_MASK(viewable);
  if (!is_mask) {
    auto layer_mask = gimp_layer_get_mask (GIMP_LAYER(viewable));
    if (layer_mask && gimp_layer_mask_get_edit(layer_mask))
      return;
  }

  auto i_context = info->view->context;
  if (i_context) {
    IHashTable<GimpViewable*, GList*> i_layer_dict(info->view->layer_dict);
    GimpContainer* children = gimp_viewable_get_children (viewable);
    bool updated = false;

    if (!children || is_mask) {
      updated = true;
      info->dirty_count = 0;
      update_cairo_surface(viewable);
 
    } else {
      GList* list = i_layer_dict[viewable];
      if (list && list->data) {
        LayerInfo* layer_info = reinterpret_cast<LayerInfo*>(list->data);
        if (layer_info->dirty_count > 0 || gimp_container_get_n_children(children) == 0) {
          layer_info->dirty_count = 0;
          updated = true;
          layer_info->layer_preview.update_cairo_surface(viewable);
        }
 
      }
    }

    // Porpagating update to the ancestors if updated surface.
    if (updated) {
 
      if (is_mask)
        viewable = GIMP_VIEWABLE(gimp_layer_mask_get_layer (GIMP_LAYER_MASK(viewable)));
 
      while (viewable) {
        viewable = gimp_viewable_get_parent (viewable);
        GList* list = i_layer_dict[viewable];
 
        if (list && list->data) {
          LayerInfo* layer_info = reinterpret_cast<LayerInfo*>(list->data);
          layer_info->dirty_count ++;
          layer_info->layer_preview.update_cairo_surface(viewable);
        }

      }
    }

  }
}

//////////////////////////////////////////////////////////////////////////////////////////////
// Other functions

void
LayerTileView::LayerInfo::LayerPreview::draw(cairo_t* cr, gint x, gint y, GimpRGB* fg_color, GimpRGB* bg_color)
{
  if (!surface) {
    update_cairo_surface(info->layer);

  } else {
    cairo_set_source_surface (
      cr, surface, 
      x + (LAYER_MIN_WIDTH - width) / 2, 
      y + (LAYER_MIN_HEIGHT - height) / 2);
    cairo_paint (cr);
  }

}


void 
LayerTileView::LayerInfo::LayerPreview::update_cairo_surface(GimpViewable* viewable)
{
  update_idle = new Idle(
    _D::delegator(std::function<gboolean()>([this, viewable]()->gboolean {

      auto i_viewable = ref(viewable);
      gint width, height;

      i_viewable [gimp_viewable_get_preview_size] (std::min(LAYER_MIN_WIDTH, LAYER_MIN_HEIGHT), TRUE, TRUE, &width, &height);
      GdkPixbuf* pixbuf = i_viewable [gimp_viewable_get_pixbuf] (info->view->context.ptr(), width, height);

      if (pixbuf) {
        cairo_surface_t* surface   = build_cairo_surface(pixbuf);
        this->surface  = surface;
        this->width    = width;
        this->height   = height;

        GdkRectangle boundary = info->view->get_boundary(viewable);
        ref(info->view->content_area) [gtk_widget_queue_draw_area] (boundary.x, boundary.y, boundary.width, boundary.height);
        ref(info->view->scrolled_window) [gtk_widget_queue_draw] ();

      } else {
        TempBuf* buf = i_viewable [gimp_viewable_get_preview] (info->view->context.ptr(), width, height);
        this->surface  = build_cairo_surface(buf);
        this->width  = width;
        this->height = height;

        GdkRectangle boundary = info->view->get_boundary(viewable);
        ref(info->view->content_area) [gtk_widget_queue_draw_area] (boundary.x, boundary.y, boundary.width, boundary.height);
        ref(info->view->scrolled_window) [gtk_widget_queue_draw] ();
      }
      return  false;
    }))
  );
}


//////////////////////////////////////////////////////////////////////////////////////////////
// Public functions
//////////////////////////////////////////////////////////////////////////////////////////////



GimpLayerTileView *
LayerTileViewInterface::new_instance () {
  return GIMP_LAYER_TILE_VIEW(g_object_new (GLib::Class::Traits::get_type(), NULL));
}

LayerTileViewInterface*
LayerTileViewInterface::cast(gpointer obj) {
  return dynamic_cast<LayerTileViewInterface*>(GLib::Class::get_private(obj));
}

bool LayerTileViewInterface::is_instance(gpointer obj) {
  return GLib::Class::Traits::is_instance(obj);
}

GType gimp_layer_tile_view_get_type() { return GLib::Class::Traits::get_type(); }
GimpLayerTileView* gimp_layer_tile_view_new() { return LayerTileViewInterface::new_instance(); }