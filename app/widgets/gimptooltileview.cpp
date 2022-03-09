/* GIMP-painter
 * GimpToolTileView
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
#include "base/glib-cxx-def-utils.hpp"
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

#include "core/gimpmarshal.h"
#include "core/gimp.h"
#include "core/gimpimage.h"
#include "core/gimpcontext.h"
#include "core/gimpcontainer.h"
#include "core/gimplist.h"
#include "core/gimptoolinfo.h"
#include "core/gimptoolitem.h"
#include "core/gimptoolgroup.h"

#include "gimp-intl.h"
#include "gimpwidgets-constructors.h"
#include "gimpwidgets-utils.h"
#include "gimphelp-ids.h"
#include "gimpspinscale.h"
#include "gimpmenufactory.h"
#include "gimpuimanager.h"

#include "libgimpcolor/gimpcolor.h"
#include "libgimpwidgets/gimpwidgets.h"

};

#include "gimptooltileview.h"

using namespace GLib;
using namespace GIMP;

//////////////////////////////////////////////////////////////////////////////////////////////
static const int ICON_SIZE = 20;
static const int ICON_MARGIN = 6;
static const int ICON_KNOB_WIDTH = 8;
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

typedef UseCStructs<GtkBox, GimpToolTileView> CStructs;

struct ToolTileView : virtual public ImplBase, virtual public ToolTileViewInterface
{
  bool vertical;
  GimpToolItem* hover_tool;
  GimpToolGroup* active_group;

  Object<GtkScrolledWindow> event_box;
  Object<GtkDrawingArea> content_area;

  List tools;

  Object<GimpUIManager>     ui_manager;
  IObject<GimpContext>      context;
  CXXPointer<_D::Connection> tool_changed_handler;

  ToolTileView(GObject* o);  
  virtual ~ToolTileView();

  static void class_init(CStructs::Class* klass);

  CopyValue get_context();
  void set_context(IValue v);

  void on_expose(GtkDrawingArea *widget, GdkEventExpose *event);
  gboolean on_button_press(GtkWidget* widget, GdkEventButton* event);
  gboolean on_motion_notify(GtkWidget* widget, GdkEventMotion* event);
  gboolean on_enter_notify(GtkWidget* widget, GdkEventCrossing* event);
  gboolean on_leave_notify(GtkWidget* widget, GdkEventCrossing* event);
  void     on_tool_changed (GtkWidget* widget, GimpToolInfo* tool_info);


  void draw(GtkDrawingArea* drawing_area, cairo_t* cr, int width, int height);
  void get_tool_item_at_position(gint x, gint y, GimpToolGroup** group, GimpToolInfo** item);
  void configure_size();

  // Inherited methods
  virtual void constructed  ();
  virtual void dummy() {};
};

//////////////////////////////////////////////////////////////////////////////////////////////
// Bridge between C++ class and Gtk-compatible class information

extern const char gimp_tool_tile_view_name[] = "GimpToolTileView";
using Class = NewGClass<gimp_tool_tile_view_name, CStructs, ToolTileView>;
static Class class_instance;

#define _override(method) Class::__(&klass->method).bind<&ToolTileView::method>()
#define _getter(method)   Class::__((CopyValue (**)(GObject*))NULL).bind<&ToolTileView::get_##method >()
#define _setter(method)   Class::__((void (**)(GObject*, IValue))NULL).bind<&ToolTileView::set_##method >()

void 
ToolTileView::class_init(CStructs::Class *klass)
{
  class_instance.with_class(klass)->
      as_class<GObject>([](GObjectClass* klass){
        _override (constructed);

      })->install_property(
          Class::g_param_spec_object("context", GIMP_TYPE_CONTEXT, (GParamFlags)(GIMP_PARAM_READWRITE|G_PARAM_CONSTRUCT) ),
          _getter(context), _setter(context)
      );
}

}; // namespace

//////////////////////////////////////////////////////////////////////////////////////////////
// Implementation of ToolTileView class
//////////////////////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////////////////////
// Class ToolTileView
//////////////////////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////////////////////
// Constructors / Destructors

ToolTileView::ToolTileView(GObject* o) : 
    ImplBase(o), 
    event_box(NULL), 
    content_area(NULL), 
    ui_manager(NULL), 
    context(NULL), 
    tools(NULL)
{
  vertical = true;
  hover_tool = NULL;
  active_group = NULL;
}


ToolTileView::~ToolTileView()
{
}


void 
ToolTileView::constructed ()
{
  ui_manager = gimp_menu_factory_manager_new (global_menu_factory, "<Dock>", g_object, false);//last is tear-off.
  GimpActionGroup* group = gimp_ui_manager_get_action_group (ui_manager, "layers");

  ref(g_object) [gtk_orientable_set_orientation] (GTK_ORIENTATION_VERTICAL);

  with (ref(GTK_BOX(g_object)), [&](auto self) {
    self.pack_start(false, false, 0) (GTK_SCROLLED_WINDOW(gtk_scrolled_window_new(NULL, NULL)), [&](auto box) {
      event_box = box.ptr();
      box [gtk_scrolled_window_set_policy] (GTK_POLICY_NEVER, GTK_POLICY_NEVER);
      content_area = box.add_with_viewport (GTK_DRAWING_AREA (gtk_drawing_area_new ()), [this] (auto i_content_area){
        i_content_area [gtk_widget_set_size_request] (ICON_SIZE, ICON_SIZE);
        i_content_area [gtk_widget_set_events] (GDK_ALL_EVENTS_MASK);

        i_content_area.connect_noret("expose-event",       _D::delegator(this, &ToolTileView::on_expose));
        i_content_area.connect_noret("button-press-event", _D::delegator(this, &ToolTileView::on_button_press));
        i_content_area.connect_noret("motion-notify-event", _D::delegator(this, &ToolTileView::on_motion_notify));
        i_content_area.connect_noret("enter-notify-event", _D::delegator(this, &ToolTileView::on_enter_notify));
        i_content_area.connect_noret("leave-notify-event", _D::delegator(this, &ToolTileView::on_leave_notify));
        
        i_content_area [gtk_widget_show] ();
      }).ptr();

      box [gtk_widget_show] ();
    });
  });

}


//////////////////////////////////////////////////////////////////////////////////////////////
// Setter / getter for GObject properties
//////////////////////////////////////////////////////////////////////////////////////////////


CopyValue 
ToolTileView::get_context()
{
  CopyValue result = G_OBJECT(context.ptr());
  GObject* object = result;
  return result;
}


void 
ToolTileView::set_context(IValue v) 
{
  const GValue* val = v.ptr();
  GimpContext* context = GIMP_CONTEXT(g_value_get_object(val));
  this->context = context;

  if (this->context) {
    g_print("CONTEXT:%p\n", this->context.ptr());

    IList<GimpToolItem*> tool_iter = gimp_get_tool_item_iter (this->context->gimp);
    IList<GimpToolItem*> i_tools = tools;

    for (auto tool_item : tool_iter) {
        if (gimp_tool_item_get_visible (tool_item)) {
          i_tools.append(tool_item);
        }
    }

    tool_changed_handler = ref(context).connect("tool-changed", _D::delegator(this, &ToolTileView::on_tool_changed));
    configure_size();
  }
}


//////////////////////////////////////////////////////////////////////////////////////////////
// Event handlers
//////////////////////////////////////////////////////////////////////////////////////////////

void 
ToolTileView::on_expose(GtkDrawingArea *widget, GdkEventExpose *event)
{
  auto i_widget = ref(widget);
  cairo_t* cr = gdk_cairo_create (i_widget [gtk_widget_get_window] ());
  int width = event->area.width;
  int height = event->area.height;
  gdk_cairo_rectangle (cr, &event->area);
  draw(widget, cr, width, height);
  cairo_destroy(cr);
//  GtkAllocation child_allocation;
//  gtk_widget_get_allocation (GTK_WIDGET(event_box.ptr()), &child_allocation);
//  gdk_window_invalidate_rect (gtk_widget_get_window (GTK_WIDGET(event_box.ptr())), &child_allocation, FALSE);
}

gboolean
ToolTileView::on_button_press(GtkWidget* widget, GdkEventButton* event)
{
  if (event->button == 1) {
    GimpToolGroup* group;
    GimpToolInfo* active_tool = NULL;
    get_tool_item_at_position(event->x, event->y, &group, &active_tool);
    g_print("active_tool=%s, group=%p\n", gimp_object_get_name(G_OBJECT(active_tool)), group);
    if (active_tool) {
      if (group) {
        gimp_viewable_set_expanded (GIMP_VIEWABLE(group), TRUE);
        gimp_tool_group_set_active_tool_info (group, active_tool);
        if (active_group != group)
          gimp_viewable_set_expanded (GIMP_VIEWABLE(active_group), FALSE);
        active_group = group;
        configure_size();
      } else {
        gimp_viewable_set_expanded (GIMP_VIEWABLE(active_group), FALSE);
        active_group = NULL;
        configure_size();
      }
      ref(context) [gimp_context_set_tool] (active_tool);
      ref(event_box) [gtk_widget_queue_draw] ();
    }

  }

  return TRUE;
}


gboolean
ToolTileView::on_motion_notify(GtkWidget* widget, GdkEventMotion* event)
{
  GimpToolGroup* group;
  GimpToolInfo*  tool;
  get_tool_item_at_position(event->x, event->y, &group, &tool);
  if (group && !gimp_viewable_get_expanded(GIMP_VIEWABLE(group)))
    hover_tool = GIMP_TOOL_ITEM(group);
  else
    hover_tool = GIMP_TOOL_ITEM(tool);
  ref(event_box) [gtk_widget_queue_draw] ();

  return TRUE;
}


gboolean
ToolTileView::on_enter_notify(GtkWidget* widget, GdkEventCrossing* event)
{
  return TRUE;
}


gboolean
ToolTileView::on_leave_notify(GtkWidget* widget, GdkEventCrossing* event)
{
  hover_tool = NULL;
  ref(event_box) [gtk_widget_queue_draw] ();

  return TRUE;
}

void
ToolTileView::on_tool_changed (GtkWidget* widget, GimpToolInfo* tool_info)
{
  ref(event_box) [gtk_widget_queue_draw] ();  
}


//////////////////////////////////////////////////////////////////////////////////////////////
// Other functions
//////////////////////////////////////////////////////////////////////////////////////////////


void 
ToolTileView::draw(GtkDrawingArea * widget, cairo_t* cr, int width, int height)
{
//  g_print("LayerTileView::draw(%d, %d)\n", width, height);
  GimpToolInfo* active_tool;
  GimpRGB color1 = { 1.0, 1.0, 1.0, 1};
  GimpRGB color2 = { 0.7, 0.7, 0.7, 1};
  GimpRGB color3 = { 0.25, 0.5, 1.0, 1};
  GimpRGB color4;
  GtkStyle* style = ref(g_object) [gtk_widget_get_style] ();
  gimp_rgb_set_gdk_color (&color1, &style->bg[GTK_STATE_NORMAL]);
  gimp_rgb_set_gdk_color (&color2, &style->dark[GTK_STATE_SELECTED]);
  gimp_rgb_set_gdk_color (&color3, &style->bg[GTK_STATE_SELECTED]);
  gimp_rgb_set_gdk_color (&color4, &style->light[GTK_STATE_SELECTED]);
  
  cairo_set_source_rgb (cr, color1.r, color1.g, color1.b);
  cairo_rectangle ( cr, 0, 0, width, height );
  cairo_fill (cr);


  active_tool = ref(context) [gimp_context_get_tool] ();

  IList<GimpToolItem*> i_tools = tools;
  int i = 0;

  std::function<void(IList<GimpToolItem*>&)> draw_tools = [&](IList<GimpToolItem*>& i_tools) {
    for (auto tool : i_tools) {
        GtkToolItem   *item;
        const gchar   *stock_id;
        GimpUIManager *ui_manager;
        gint x, y, w, h;
        GimpRGB* knob_color = NULL;
        GimpToolItem* tool_item = tool;

        if (GIMP_IS_TOOL_GROUP(tool)) {
          tool_item = GIMP_TOOL_ITEM(gimp_tool_group_get_active_tool_info (GIMP_TOOL_GROUP(tool)));
          if (tool_item == GIMP_TOOL_ITEM(active_tool)) {
            if (gimp_viewable_get_expanded (GIMP_VIEWABLE (tool))) {
              GimpList* container = GIMP_LIST(gimp_viewable_get_children (GIMP_VIEWABLE (tool)));
              IList<GimpToolItem*> sub_tools = container->list;

              x =  vertical? 0: (ICON_SIZE + ICON_MARGIN * 2) * i;
              y = !vertical? 0: (ICON_SIZE + ICON_MARGIN * 2) * i;
              w =  vertical? ICON_KNOB_WIDTH + ICON_SIZE + ICON_MARGIN * 2: (ICON_SIZE+ICON_MARGIN*2)*sub_tools.length();
              h = !vertical? ICON_KNOB_WIDTH + ICON_SIZE + ICON_MARGIN * 2: (ICON_SIZE+ICON_MARGIN*2)*sub_tools.length();
              cairo_set_source_rgb (cr, color4.r, color4.g, color4.b);
              cairo_rectangle ( cr, x, y, w, h );
              cairo_stroke (cr);

              draw_tools(sub_tools);
              continue;
            }
          }
        }


        if (tool_item == GIMP_TOOL_ITEM(active_tool)) {
          knob_color = &color3;
        } else if (tool == GIMP_TOOL_ITEM(hover_tool)) {
          knob_color = &color2;
        }

        if (knob_color) {
          x =  vertical? 0 : (ICON_SIZE + ICON_MARGIN * 2) * i;
          y = !vertical? 0 : (ICON_SIZE + ICON_MARGIN * 2) * i;
          w =  vertical? ICON_KNOB_WIDTH: ICON_SIZE + ICON_MARGIN * 2;
          h = !vertical? ICON_KNOB_WIDTH: ICON_SIZE + ICON_MARGIN * 2;
          cairo_set_source_rgb (cr, knob_color->r, knob_color->g, knob_color->b);
          cairo_rectangle ( cr, x, y, w, h);
          cairo_fill (cr);
        }

        x =  vertical? ICON_MARGIN + ICON_KNOB_WIDTH: (ICON_SIZE + ICON_MARGIN * 2) * i + ICON_MARGIN;
        y = !vertical? ICON_MARGIN + ICON_KNOB_WIDTH: (ICON_SIZE + ICON_MARGIN * 2) * i + ICON_MARGIN;

        stock_id = gimp_viewable_get_stock_id (GIMP_VIEWABLE (tool_item));
        cairo_surface_t* surface = build_cairo_surface (GTK_WIDGET (g_object), stock_id, ICON_SIZE, ICON_SIZE);
        if (surface) {
          cairo_set_source_surface (cr, surface, x, y);
          cairo_paint (cr);
          cairo_surface_destroy (surface);
        }

      i ++;
    }
  };
  draw_tools(i_tools);


}


void 
ToolTileView::get_tool_item_at_position(gint x, gint y, GimpToolGroup** group, GimpToolInfo** item)
{
    gint ix, iy;
    ix =  vertical ? 0: x / (ICON_SIZE + ICON_MARGIN * 2);
    iy = !vertical ? 0: y / (ICON_SIZE + ICON_MARGIN * 2);
    gint index = ix + iy;

    if (group)
      *group = NULL;

    if (item)
      *item = NULL;

    int i = 0;
    IList<GimpToolItem*> i_tools = tools;
    for (auto tool_item: i_tools) {
      if (GIMP_IS_TOOL_GROUP(tool_item) && gimp_viewable_get_expanded(GIMP_VIEWABLE(tool_item))) {
        IList<GimpToolItem*> children = GIMP_LIST(gimp_viewable_get_children (GIMP_VIEWABLE(tool_item)))->list;
        gint length =  children.length();
        if (i + length <= index ) {
          i += length;
          continue;
        } else {
          if (group)
            *group = GIMP_TOOL_GROUP(tool_item);
          if (item)
            *item = GIMP_TOOL_INFO(children[index - i]);
          return;
        }
      } else {
        if (index == i) {
          if (GIMP_IS_TOOL_GROUP(tool_item)) {
            if (item) 
              *item = gimp_tool_group_get_active_tool_info (GIMP_TOOL_GROUP(tool_item));
            if (group)
              *group = GIMP_TOOL_GROUP(tool_item);
          } else if (item)
            *item = GIMP_TOOL_INFO(tool_item);
          return;
        }
      }
      i ++;
    }
}


void 
ToolTileView::configure_size()
{
  IList<GimpToolItem*> i_tools = tools;
  gint length = i_tools.length();

  if (active_group) {
    IList<GimpToolItem*> children = GIMP_LIST(gimp_viewable_get_children (GIMP_VIEWABLE(active_group)))->list;
    length += children.length() - 1;
  }

  ref(content_area) [gtk_widget_set_size_request] ( vertical? ICON_SIZE + ICON_MARGIN * 2 + ICON_KNOB_WIDTH: 16 + (ICON_SIZE + ICON_MARGIN * 2) * length,
                                                  !vertical? ICON_SIZE + ICON_MARGIN * 2 + ICON_KNOB_WIDTH: 16 + (ICON_SIZE + ICON_MARGIN * 2) * length); 
  ref(event_box) [gtk_widget_set_size_request] ( vertical? ICON_SIZE + ICON_MARGIN * 2 + ICON_KNOB_WIDTH: 16 + (ICON_SIZE + ICON_MARGIN * 2) * length,
                                                  !vertical? ICON_SIZE + ICON_MARGIN * 2 + ICON_KNOB_WIDTH: 16 + (ICON_SIZE + ICON_MARGIN * 2) * length); 
}

//////////////////////////////////////////////////////////////////////////////////////////////
// Public functions
//////////////////////////////////////////////////////////////////////////////////////////////



GimpToolTileView *
ToolTileViewInterface::new_instance () {
  return GIMP_TOOL_TILE_VIEW(g_object_new (GLib::Class::Traits::get_type(), NULL));
}

ToolTileViewInterface*
ToolTileViewInterface::cast(gpointer obj) {
  return dynamic_cast<ToolTileViewInterface*>(GLib::Class::get_private(obj));
}

bool ToolTileViewInterface::is_instance(gpointer obj) {
  return GLib::Class::Traits::is_instance(obj);
}

GType gimp_tool_tile_view_get_type() { return GLib::Class::Traits::get_type(); }
GimpToolTileView* gimp_tool_tile_view_new() { return ToolTileViewInterface::new_instance(); }