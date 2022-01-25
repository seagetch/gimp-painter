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

#include "core/gimpdashpattern.h"
#include "core/gimpmarshal.h"
#include "core/gimp.h"
#include "core/gimpimage.h"
#include "core/gimplayer.h"
#include "core/gimplayermask.h"
#include "core/gimpcontext.h"
#include "core/gimpcontainer.h"

#include "gimp-intl.h"
#include "gimpwidgets-constructors.h"
#include "gimpwidgets-utils.h"
#include "gimphelp-ids.h"
#include "gimpspinscale.h"
#include "gimpdnd.h"

#include "libgimpcolor/gimpcolor.h"
#include "libgimpwidgets/gimpwidgets.h"

};

#include "base/glib-cxx-def-utils.hpp"

#include "gimplayertileview.h"

using namespace GLib;

//////////////////////////////////////////////////////////////////////////////////////////////
static const int LAYER_MAX_WIDTH    = 64;
static const int LAYER_MAX_HEIGHT   = 64;
static const int LAYER_MIN_WIDTH    = 56;
static const int LAYER_MIN_HEIGHT   = 56;
static const int LAYER_INDENT_WIDTH = 8;
static const int LAYER_BAR_WIDTH    = 4;
static const int CHECKERBORAD_SIZE  = 4;
static const int ICON_SIZE          = 14;
//////////////////////////////////////////////////////////////////////////////////////////////
namespace GLib {
namespace _D = Delegators;

//////////////////////////////////////////////////////////////////////////////////////////////
// Helper classes

template<typename T>
class IContainer : public IObject<GimpContainer> {
public:
  IContainer (GimpContainer* src) : IObject(src) {}
  IContainer (const IObject<GimpContainer>& src) : IObject(src) {}
  void each(std::function<void(T* obj)> f) {
    auto recursive_delegator = _D::delegator(f);
    (*this) [gimp_container_foreach] (GFunc(std::remove_reference<decltype(*recursive_delegator)>::type::callback), &recursive_delegator);
    delete recursive_delegator;
  }
};

//////////////////////////////////////////////////////////////////////////////////////////////
// Class definitions

typedef UseCStructs<GtkBox, GimpLayerTileView> CStructs;

struct LayerTileView : virtual public ImplBase, virtual public LayerTileViewInterface
{
  Object<GtkScrolledWindow> scrolled_window;
  Object<GtkDrawingArea>    content_area;
  IObject<GimpContext>      context;
  IObject<GimpImage>        image;

  List                      layers;
  HashTable                 layer_dict;

  CXXPointer<_D::Connection> added_handler;
  CXXPointer<_D::Connection> removed_handler;
  CXXPointer<_D::Connection> reordered_handler;
  ScopedPointer<cairo_surface_t, void (cairo_surface_t*), cairo_surface_destroy> eye_surface;
  CXXPointer<_D::Delegator<GimpViewable* (GtkWidget*, GimpContext**)> > drag_viewable_holder; 
  CXXPointer<_D::Connection> drag_motion_handler;
  CXXPointer<_D::Connection> drag_drop_handler;
  // Internal class which hold layer information required for display and control.
  struct Layer {
    Layer(GimpViewable* layer, int level) : surface(NULL) {
      this->layer = layer;
      this->level = level;
      dirty_count = 0;
    }

    ~Layer() {}

    CXXPointer<_D::Connection> invalidate_preview_handler;
    CXXPointer<_D::Connection> size_changed_handler;
    CXXPointer<_D::Connection> added_handler;
    CXXPointer<_D::Connection> removed_handler;
    CXXPointer<_D::Connection> reordered_handler;
    ScopedPointer<cairo_surface_t, void (cairo_surface_t*), cairo_surface_destroy> surface;

    IObject<GimpViewable> layer;
    int level;
    int preview_width;
    int preview_height;
    int dirty_count;
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
    GimpViewable* target;
    Action action;
  };

  CXXPointer<DragAction> drag_action;

  LayerTileView(GObject* o);  
  virtual ~LayerTileView();

  static void class_init(CStructs::Class* klass);

  void on_expose(GtkDrawingArea *widget, GdkEventExpose *event);
  void draw(GtkDrawingArea* drawing_area, cairo_t* cr, int width, int height); //gpointer user_data
  void on_layer_added(GimpContainer* container, GimpViewable* layer);
  void on_layer_removed(GimpContainer* container, GimpViewable* layer);
  void on_layer_reordered(GimpContainer* container, GimpViewable* layer, gint index);
  void on_invalidate_preview(GimpViewable* viewable);
  gboolean on_button_press(GtkWidget* widget, GdkEventButton* event);
  gboolean on_drag_motion(GtkWidget* widget, GdkDragContext* context, gint x, gint y, guint time_);
  gboolean on_drag_drop(GtkWidget* widget, GdkDragContext* context, gint x, gint y, guint time_);

  CopyValue get_image();
  void      set_image(IValue v);
  CopyValue get_context();
  void      set_context(IValue v);

  void reset_layers();

  void update_cairo_surface(Layer* layer_info, GimpViewable* viewable);
  cairo_surface_t* build_cairo_surface(TempBuf* temp_buf);
  cairo_surface_t* build_cairo_surface(GdkPixbuf* pixbuf);
  cairo_surface_t* build_cairo_surface(const gchar* stock_id, int width, int height);

  GdkRectangle  get_boundary(GimpViewable* viewable);
  MouseAction get_viewable_at(gint x, gint y);


  GimpViewable* get_drag_viewable(GtkWidget *widget, GimpContext **context);

  // Inherited methods
  virtual void            constructed  ();
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

void LayerTileView::class_init(CStructs::Class *klass)
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
// Constructors / Destructors

LayerTileView::LayerTileView(GObject* o) : 
    ImplBase(o), 
    layers(NULL), 
    layer_dict(g_hash_table_new(g_direct_hash, g_direct_equal)) {
}

LayerTileView::~LayerTileView() {

}

void LayerTileView::constructed ()
{
  ref(g_object) [gtk_orientable_set_orientation] (GTK_ORIENTATION_VERTICAL);

  with (ref(GTK_BOX(g_object)), [&](auto self) {
    
    self.pack_start(false, false, 0) (
      gtk_button_new(), [&] (auto button) {

          button [gtk_button_set_label] ("+");
          button [gtk_widget_show] ();
      }
    ).pack_start(true, true, 0) (
      GTK_SCROLLED_WINDOW (gtk_scrolled_window_new (NULL, NULL)), [&] (auto window) {

        scrolled_window     = window.ptr();
        content_area        = GTK_DRAWING_AREA (gtk_drawing_area_new ());
        auto i_content_area = ref(content_area);

        window [gtk_scrolled_window_add_with_viewport] (GTK_WIDGET(content_area.ptr()));
        
        i_content_area [gtk_widget_set_size_request] (LAYER_MAX_WIDTH, LAYER_MAX_HEIGHT);
        i_content_area [gtk_widget_set_events] (GDK_ALL_EVENTS_MASK);

        i_content_area.connect("expose-event",       _D::delegator(this, &LayerTileView::on_expose));
        i_content_area.connect("button-press-event", _D::delegator(this, &LayerTileView::on_button_press));
        
        i_content_area [gtk_widget_show] ();

        window [gtk_widget_set_size_request] (LAYER_MAX_WIDTH + 16, LAYER_MAX_HEIGHT);
        window [gtk_widget_show] ();

      }
    );
  });

}

//////////////////////////////////////////////////////////////////////////////////////////////
// Event handlers

void LayerTileView::on_expose(GtkDrawingArea *widget, GdkEventExpose *event) {
  auto i_widget = ref(widget);
  cairo_t* cr = gdk_cairo_create (i_widget [gtk_widget_get_window] ());
  int width = event->area.width;
  int height = event->area.height;
  gdk_cairo_rectangle (cr, &event->area);
  draw(widget, cr, width, height);
  cairo_destroy(cr);
}

void LayerTileView::draw(GtkDrawingArea * widget, cairo_t* cr, int width, int height) {
//  g_print("LayerTileView::draw(%d, %d)\n", width, height);
  IList<Layer*> i_layers = layers;
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
    eye_surface = build_cairo_surface(GIMP_STOCK_VISIBLE, ICON_SIZE, ICON_SIZE);
  }

  cairo_set_source_rgb (cr, bg_color.r, bg_color.g, bg_color.b);
  cairo_rectangle ( cr, 0, 0, width, height );
  cairo_fill (cr);

  int i = 0;
  i_layers.each([&](Layer* layer) {
    if (drag_action) {
      if (layer->layer == drag_action->target) {

        switch (drag_action->action) {
        case DragAction::InsertBefore: {
            cairo_rectangle (cr, 
              layer->level * LAYER_INDENT_WIDTH, 
              i* LAYER_MAX_HEIGHT - (LAYER_MAX_HEIGHT - LAYER_MIN_HEIGHT) / 2, 
              LAYER_MAX_WIDTH, 
              (LAYER_MAX_HEIGHT - LAYER_MIN_HEIGHT));
            cairo_set_source_rgb (cr, highlight_color.r, highlight_color.g, highlight_color.b);
            cairo_fill (cr);
          }
          break;
        
        case DragAction::InsertAfter: {
            cairo_rectangle (cr, 
              layer->level * LAYER_INDENT_WIDTH, 
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
      if (layer->layer == active_layer) {
        cairo_rectangle (cr, layer->level * LAYER_INDENT_WIDTH, i* LAYER_MAX_HEIGHT, LAYER_MAX_WIDTH, LAYER_MAX_HEIGHT);
        cairo_set_source_rgb (cr, highlight_color.r, highlight_color.g, highlight_color.b);
        cairo_fill (cr);
      }
    }

    for (int j = 0; j < layer->level; j ++) {
      cairo_rectangle (cr, j * LAYER_INDENT_WIDTH, i* LAYER_MAX_HEIGHT, LAYER_BAR_WIDTH, LAYER_MAX_HEIGHT);
      cairo_set_source_rgb (cr, 0.75, 0.75, 0.75);
      cairo_fill (cr);
    }

    cairo_rectangle (cr, 
      layer->level * LAYER_INDENT_WIDTH, 
      i* LAYER_MAX_HEIGHT + (LAYER_MAX_HEIGHT - LAYER_MIN_HEIGHT) / 2, 
      LAYER_MIN_WIDTH, 
      LAYER_MIN_HEIGHT);
    cairo_set_source (cr, pattern);
    cairo_fill_preserve (cr);

    if (context) {
      if (!layer->surface) {
        update_cairo_surface(layer, layer->layer);
      }

      if (layer->surface) {
        cairo_set_source_surface (
          cr, layer->surface, 
          layer->level * LAYER_INDENT_WIDTH + (LAYER_MIN_WIDTH - layer->preview_width) / 2, 
          i* LAYER_MAX_HEIGHT + (LAYER_MAX_HEIGHT - LAYER_MIN_HEIGHT) / 2 + (LAYER_MIN_HEIGHT - layer->preview_height) / 2);
        cairo_paint (cr);
      }

      // Visibility eye-ball icon
      auto i_layer = ref(layer->layer);
      gboolean visible = i_layer [gimp_item_get_visible] ();
      cairo_set_source_surface (
        cr, eye_surface, 
        layer->level * LAYER_INDENT_WIDTH, 
        i* LAYER_MAX_HEIGHT + (LAYER_MAX_HEIGHT - LAYER_MIN_HEIGHT) / 2);
      cairo_paint (cr);

      if (!visible) {
        cairo_set_source_rgb (cr, fg_color.r, fg_color.g, fg_color.b);
        cairo_move_to (
          cr,layer->level * LAYER_INDENT_WIDTH + ICON_SIZE, 
          i* LAYER_MAX_HEIGHT + (LAYER_MAX_HEIGHT - LAYER_MIN_HEIGHT) / 2);
        cairo_line_to (
          cr,layer->level * LAYER_INDENT_WIDTH, 
          i* LAYER_MAX_HEIGHT + (LAYER_MAX_HEIGHT - LAYER_MIN_HEIGHT) / 2 + ICON_SIZE);
        cairo_stroke(cr);
      }
    }

    cairo_set_source_rgb (cr, 0.7, 0.7, 0.7);
    cairo_stroke (cr);
    i ++;
  });

  cairo_pattern_destroy (pattern);
}


void LayerTileView::on_layer_added(GimpContainer* container, GimpViewable* layer) {
  g_print("on_layer_added\n");
  IContainer<GimpViewable*> i_container = container;
  auto i_layer           = ref(layer);
  auto i_layer_dict      = ref<GimpViewable*, GList*>(this->layer_dict);
  IList<Layer*> i_layers = this->layers;

  gint index = i_container [gimp_container_get_child_index] (GIMP_OBJECT (layer));
  GimpViewable* parent = i_layer [gimp_viewable_get_parent] ();

  GList* parent_list = i_layer_dict[parent];
  Layer* parent_info = parent_list? reinterpret_cast<Layer*>(parent_list->data): NULL;

  Layer* layer_info;
  if (parent_info) {
    layer_info = new Layer(layer, parent_info->level + 1);
  } else {
    layer_info = new Layer(layer, 0);
    index --;
  }


  GList* sibling = parent_list? parent_list: layers.ptr();
  for (;index >= 0; index --) {
    if (!sibling)
      break;
    sibling = sibling->next;
    while (sibling && sibling->data && reinterpret_cast<Layer*>(sibling->data)->level > layer_info->level)
      sibling = sibling->next;
  }
  i_layers.insert_before(sibling, layer_info);

  GList* new_item = g_list_find (layers.ptr(), layer_info);
  i_layer_dict.insert(layer, new_item);

  layer_info->size_changed_handler       = i_layer.connect("size-changed",       _D::delegator(this, &LayerTileView::on_invalidate_preview));
  layer_info->invalidate_preview_handler = i_layer.connect("invalidate-preview", _D::delegator(this, &LayerTileView::on_invalidate_preview));

  GimpContainer* children = gimp_viewable_get_children (layer);
  if (children) {
    IContainer<GimpViewable> i_children(children);
    layer_info->added_handler     = i_children.connect("add",     _D::delegator(this, &LayerTileView::on_layer_added));
    layer_info->removed_handler   = i_children.connect("remove",  _D::delegator(this, &LayerTileView::on_layer_removed));
    layer_info->reordered_handler = i_children.connect("reorder", _D::delegator(this, &LayerTileView::on_layer_reordered));
    i_children.each([&](GimpViewable* viewable) {
      on_layer_added(children, viewable);
    });
  } else {
  }

  ref(content_area) [gtk_widget_queue_draw] ();
}

void LayerTileView::on_layer_removed(GimpContainer* container, GimpViewable* layer) {
  IContainer<GimpViewable*> i_container = container;
  auto i_layer           = ref(layer);
  auto i_layer_dict      = ref<GimpViewable*, GList*>(this->layer_dict);
  IList<Layer*> i_layers = this->layers;
  
  gint index = i_container [gimp_container_get_child_index] (GIMP_OBJECT (layer));

  GList* self_list = i_layer_dict[layer];
  g_return_if_fail (self_list != NULL);

  Layer* layer_info = reinterpret_cast<Layer*>(self_list->data);
  i_layer_dict.remove(layer);
  i_layers.remove(layer_info);

  GimpContainer* children = gimp_viewable_get_children (layer);
  if (children) {
    IContainer<GimpViewable> i_children(children);
    i_children.each([&](GimpViewable* viewable) {
      on_layer_removed(children, viewable);
    });
  }
  delete layer_info;

  ref(content_area) [gtk_widget_queue_draw] ();
}

void LayerTileView::on_layer_reordered(GimpContainer* container, GimpViewable* layer, gint index) {
  on_layer_removed(container, layer);
  on_layer_added(container, layer);
}

void LayerTileView::on_invalidate_preview(GimpViewable* viewable) {
  auto i_context = context;
  if (i_context) {
    IHashTable<GimpViewable*, GList*> i_layer_dict(layer_dict);
    GimpContainer* children = gimp_viewable_get_children (viewable);
    if (!children) {
      update_cairo_surface(NULL, viewable);
    } else {
      GList* list = i_layer_dict[viewable];
      if (list && list->data) {
        Layer* layer_info = reinterpret_cast<Layer*>(list->data);
        if (layer_info->dirty_count > 0) {
          layer_info->dirty_count = 0;
          update_cairo_surface(NULL, viewable);
        }
      }
    }
    while (viewable) {
      viewable = gimp_viewable_get_parent (viewable);
      GList* list = i_layer_dict[viewable];
      if (list && list->data) {
        Layer* layer_info = reinterpret_cast<Layer*>(list->data);
        layer_info->dirty_count ++;
      }
      update_cairo_surface(NULL, viewable);
    }
  }
};

gboolean LayerTileView::on_button_press(GtkWidget* widget, GdkEventButton* event) {
  // Debug
  g_print("button=%x, x, y = %d, %d\n", event->button, (int)event->x, (int)event->y);

  // Obtaining viewable for mouse position.
  MouseAction action = get_viewable_at(event->x, event->y);
  
  if (action.target && action.action == MouseAction::Hit) {
    auto i_viewable = ref(action.target);

    if (event->button == 1) {
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

      ref(content_area) [gtk_widget_queue_draw] ();
      image [gimp_image_flush] ();
    }
  } else {

  }
  return false;
}

gboolean LayerTileView::on_drag_motion(GtkWidget* widget, GdkDragContext* drag_context, gint x, gint y, guint time_) {
  if (!drag_action) {
    drag_action = new DragAction();
    drag_action->target = NULL;
    drag_action->action = DragAction::InsertAfter;
  }
  int index    = y / LAYER_MAX_HEIGHT;
  int offset_y = y % LAYER_MAX_HEIGHT;

  IList<Layer*> i_layers = layers;
  Layer* layer_info = i_layers[index];
  drag_action->target = layer_info->layer;

  if (offset_y < LAYER_MAX_HEIGHT / 2) {
    drag_action->action = DragAction::InsertBefore;
  } else {
    drag_action->action = DragAction::InsertAfter;
  }
  g_print("on_drag_motion::action = %d, target:%s\n", drag_action->action, gimp_object_get_name(GIMP_OBJECT(drag_action->target)));
  gdk_drag_status(drag_context, GDK_ACTION_MOVE, time_);
  ref(content_area) [gtk_widget_queue_draw] ();
  return TRUE; 
}


gboolean LayerTileView::on_drag_drop(GtkWidget* widget, GdkDragContext* context, gint x, gint y, guint time_) {
  g_print("drag_drop\n");
  drag_action = NULL;
  ref(content_area) [gtk_widget_queue_draw] ();

}

//////////////////////////////////////////////////////////////////////////////////////////////
// Setter / getter for GObject properties


CopyValue LayerTileView::get_image() {
  return image.ptr();
}

void LayerTileView::set_image(IValue v) {
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

  if (this->image) {
    reset_layers();

    GimpContainer* container = ref(image) [gimp_image_get_layers] ();
    IContainer<GimpViewable> i_container = ref(container);

    added_handler     = i_container.connect("add",     _D::delegator(this, &LayerTileView::on_layer_added));
    removed_handler   = i_container.connect("remove",  _D::delegator(this, &LayerTileView::on_layer_removed));
    reordered_handler = i_container.connect("reorder", _D::delegator(this, &LayerTileView::on_layer_reordered));

    int level = 0;
    int num_layers = 0;

    i_container.each([&] (GimpViewable* viewable) {
      on_layer_added(container, viewable);
    });

    // Debug dump
    IList<Layer*> i_layers     = layers;
    i_layers.each([](Layer* info){
      auto i_viewable = ref(info->layer);
      g_print("%d: %s\n", info->level, i_viewable [gimp_object_get_name] ());
    });

    auto i_content_area = ref(content_area);
    num_layers = g_list_length (layers.ptr());
    i_content_area [gtk_widget_set_size_request] (LAYER_MAX_WIDTH, LAYER_MAX_HEIGHT * num_layers);

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

    drag_motion_handler = i_content_area.connect("drag-motion", _D::delegator(this, &LayerTileView::on_drag_motion));
    drag_drop_handler   = i_content_area.connect("drag-drop",   _D::delegator(this, &LayerTileView::on_drag_motion));
  }
}

GimpViewable *
LayerTileView::get_drag_viewable (GtkWidget    *widget,
                                  GimpContext **context_holder) {
  if (context_holder)
    *context_holder = context.ptr();

  GimpLayer* layer = image [gimp_image_get_active_layer] ();

  return GIMP_VIEWABLE(layer);
}

CopyValue LayerTileView::get_context() {
  return context.ptr();
}

void LayerTileView::set_context(IValue v) {
  const GValue* val = v.ptr();
  GimpContext* context = GIMP_CONTEXT(g_value_get_object(val));
  this->context = context;
}

//////////////////////////////////////////////////////////////////////////////////////////////
// Other functions

void LayerTileView::reset_layers() {
  IList<Layer*> i_layers = layers;
  IHashTable<GimpViewable*, GList*> i_layer_dict(layer_dict);

  i_layer_dict.remove_all();
  i_layers.each([&](Layer* layer_info) {
    delete layer_info;
  });
  layers.free();
}


void LayerTileView::update_cairo_surface(LayerTileView::Layer* layer_info, GimpViewable* viewable) {
  if (!layer_info) {
    auto i_layer_dict = ref<GimpViewable*, GList*>(layer_dict);
    GList* list = i_layer_dict[viewable];
    if (list)
      layer_info = reinterpret_cast<LayerTileView::Layer*>(list->data);
  }

  if (!layer_info)
    return;
  
  auto i_viewable = ref(viewable);

  gint width, height;
  i_viewable [gimp_viewable_get_preview_size] (std::min(LAYER_MIN_WIDTH, LAYER_MIN_HEIGHT), TRUE, TRUE, &width, &height);

  GdkPixbuf* pixbuf = i_viewable [gimp_viewable_get_pixbuf] (context.ptr(), width, height);

  if (pixbuf) {
    cairo_surface_t* surface   = build_cairo_surface(pixbuf);
    layer_info->surface        = surface;
    layer_info->preview_width  = width;
    layer_info->preview_height = height;

    GdkRectangle boundary = get_boundary(viewable);
    ref(content_area) [gtk_widget_queue_draw_area] (boundary.x, boundary.y, boundary.width, boundary.height);

  } else {
    TempBuf* buf = i_viewable [gimp_viewable_get_preview] (context.ptr(), width, height);
    layer_info->surface        = build_cairo_surface(buf);
    layer_info->preview_width  = width;
    layer_info->preview_height = height;
    temp_buf_free (buf);

    GdkRectangle boundary = get_boundary(viewable);
    ref(content_area) [gtk_widget_queue_draw_area] (boundary.x, boundary.y, boundary.width, boundary.height);
  }
}

cairo_surface_t* LayerTileView::build_cairo_surface(TempBuf* temp_buf) {
  g_return_val_if_fail (temp_buf != NULL, NULL);

  cairo_surface_t* surface = cairo_image_surface_create (CAIRO_FORMAT_RGB24,
                                                         temp_buf->width,
                                                         temp_buf->height);

  g_print("gimp_view_render_temp_buf_to_surface\n");
  gimp_view_render_temp_buf_to_surface (temp_buf,
                                        -1,
                                        GIMP_VIEW_BG_CHECKS,
                                        GIMP_VIEW_BG_WHITE,
                                        surface,
                                        temp_buf->width,
                                        temp_buf->height);

  return surface;
}

cairo_surface_t* LayerTileView::build_cairo_surface(GdkPixbuf* pixbuf) {
  g_return_val_if_fail (pixbuf != NULL, NULL);

  cairo_surface_t* surface = gimp_cairo_surface_create_from_pixbuf (pixbuf);
  return surface;
}

cairo_surface_t* LayerTileView::build_cairo_surface(const gchar* stock_id, gint width, gint height) {
  GdkPixbuf   *pixbuf = NULL;
  GtkIconSize  icon_size;

  g_return_val_if_fail (stock_id != NULL, NULL);
  auto i_widget = ref(g_object);

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

          gimp_viewable_calc_preview_size (w, h,
                                           width, height,
                                           TRUE, 1.0, 1.0,
                                           &w, &h,
                                           NULL);

          scaled_pixbuf = gdk_pixbuf_scale_simple (pixbuf,
                                                   w, h,
                                                   GDK_INTERP_BILINEAR);

          g_object_unref (pixbuf);
          pixbuf = scaled_pixbuf;
        }
    }
    cairo_surface_t* result = build_cairo_surface(pixbuf);
    g_object_unref (pixbuf);
    return result;
}


LayerTileView::MouseAction LayerTileView::get_viewable_at(gint x, gint y) {
  GdkRectangle hit_test_boundary = { 0, (LAYER_MAX_HEIGHT - LAYER_MIN_HEIGHT) / 2, LAYER_MIN_WIDTH, LAYER_MIN_HEIGHT };
  IList<Layer*> i_layers     = layers;

  MouseAction result = {NULL, MouseAction::Hit, 0, 0};

  i_layers.each([&](Layer* layer_info){
    hit_test_boundary.x = layer_info->level * LAYER_INDENT_WIDTH;
    if (result.target)
      return;
    if (hit_test_boundary.x <= x && x < hit_test_boundary.x + hit_test_boundary.width &&
        hit_test_boundary.y <= y && y < hit_test_boundary.y + hit_test_boundary.height) {
      result.target   = layer_info->layer;
      result.action   = MouseAction::Hit;
      result.offset_x = x - hit_test_boundary.x;
      result.offset_y = y - hit_test_boundary.y;
    }
    hit_test_boundary.y += LAYER_MAX_HEIGHT;
  });
  return result;
}

GdkRectangle LayerTileView::get_boundary(GimpViewable* viewable) {
  bool found = false;
  IList<Layer*> i_layers     = layers;
  GdkRectangle result = {0, (LAYER_MAX_HEIGHT - LAYER_MIN_HEIGHT) / 2, LAYER_MIN_WIDTH, LAYER_MIN_HEIGHT };
  i_layers.each([&](Layer* layer_info){
    if (found)
      return;
    if (layer_info->layer == viewable) {
      found = true;
      result.x = layer_info->level * LAYER_INDENT_WIDTH;
    } else {
      result.y += LAYER_MAX_HEIGHT;
    }
  });
  if (found)
    return result;
  else
    return {-1, -1, -1, -1};
}

/*  public functions  */

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