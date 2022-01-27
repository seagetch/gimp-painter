/* GIMP-painter
 * GimpCellRendererPopup
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
#include <config.h>
#include <gegl.h>
#include <gtk/gtk.h>
#include <gdk/gdkpixbuf.h>

#include "widgets-types.h"
#include "core/gimpdashpattern.h"
#include "core/gimpmarshal.h"
#include "core/gimp.h"
#include "core/gimpimage.h"
#include "core/gimplayer.h"

#include "pdb/gimppdb-query.h"
#include "pdb/gimppdb.h"
#include "pdb/gimpprocedure.h"
#include "plug-in/gimppluginprocedure.h"

#include "libgimpcolor/gimpcolor.h"
#include "libgimpwidgets/gimpwidgets.h"
#include "libgimpbase/gimpbase.h"

#include "gimp-intl.h"
#include "gimpwidgets-constructors.h"
#include "gimphelp-ids.h"
#include "gimpspinscale.h"
#include "gimpcolorpanel.h"

};

#include "base/glib-cxx-def-utils.hpp"

#include "gimpcellrendererpopup.h"
#include "popupper.h"
#include "gimplayerpopup.h"

#include "core/gimpfilterlayer.h"

using namespace GLib;


//////////////////////////////////////////////////////////////////////////////////////////////
namespace GLib {
typedef UseCStructs<GtkCellRenderer, GimpCellRendererPopup> CStructs;

struct CellRendererPopup : virtual public ImplBase, virtual public CellRendererPopupInterface
{
  GdkPixbuf*  pixbuf;
  gchar*      stock_id;
  GtkIconSize stock_size;
  bool        active;


  CellRendererPopup(GObject* o) : ImplBase(o) {
    stock_id   = g_strdup(GIMP_STOCK_LAYER_MENU);
    stock_size = GTK_ICON_SIZE_MENU;
  }

  virtual ~CellRendererPopup() {
    if (stock_id) {
      g_free (stock_id);
      stock_id = NULL;
    }

    if (pixbuf) {
      g_object_unref (pixbuf);
      pixbuf = NULL;
    }
  };

  static void class_init(CStructs::Class* klass);

  // Inherited methods
  virtual void            constructed  ();
  virtual void            get_size     (GtkWidget       *widget,
                                        GdkRectangle    *cell_area,
                                        gint            *x_offset,
                                        gint            *y_offset,
                                        gint            *width,
                                        gint            *height);
  void                    create_pixbuf (GtkWidget           *widget);
  virtual void            render       (GdkWindow            *window,
                                        GtkWidget            *widget,
                                        GdkRectangle         *background_area,
                                        GdkRectangle         *cell_area,
                                        GdkRectangle         *expose_area,
                                        GtkCellRendererState  flags);
  virtual gboolean        activate     (GdkEvent             *event,
                                        GtkWidget            *widget,
                                        const gchar          *path,
                                        GdkRectangle         *background_area,
                                        GdkRectangle         *cell_area,
                                        GtkCellRendererState  flags);
  virtual void            clicked      (GObject* widget, const gchar* path, GdkRectangle* cell_area);
};

extern const char gimp_cell_renderer_popup_name[] = "GimpCellRendererPopup";
using Class = NewGClass<gimp_cell_renderer_popup_name, CStructs, CellRendererPopup>;
static Class class_instance;

#define _override(method) Class::__(&klass->method).bind<&CellRendererPopup::method>()
void CellRendererPopup::class_init(CStructs::Class *klass)
{
  class_instance.with_class(klass)->
      as_class<GObject>([&](GObjectClass* klass){
        _override (constructed);

      })->
      as_class<GtkCellRenderer>([&](GtkCellRendererClass* klass) {
        _override (activate);
        _override (get_size);
        _override (render);

      })->
      install_property(
          Class::g_param_spec_new("active", false, (GParamFlags)(GIMP_PARAM_READWRITE|G_PARAM_CONSTRUCT) ),
          // getter
          [](GObject* obj)->GLib::CopyValue {
            g_print("acive::getter\n");
            CellRendererPopup* popup = dynamic_cast<CellRendererPopup*>(CellRendererPopupInterface::cast(obj));
            return CopyValue(popup->active);
          },
          // setter
          [](GObject* obj, GLib::IValue v)->void {
            g_print("acive::setter\n");
            CellRendererPopup* popup = dynamic_cast<CellRendererPopup*>(CellRendererPopupInterface::cast(obj));
            popup->active = v;
          }
      )->
      install_signal("parent-clicked", G_TYPE_NONE, G_TYPE_OBJECT, G_TYPE_STRING, G_TYPE_POINTER);
}

}; // namespace

void GLib::CellRendererPopup::constructed ()
{
  LayerPopupWindow* window_decor = new LayerPopupWindow;
  decorator(g_object, window_decor);
  decorate_popover(g_object, Delegators::delegator(window_decor, &LayerPopupWindow::create_view));
}

void
GLib::CellRendererPopup::get_size (GtkWidget       *widget,
                                     GdkRectangle    *cell_area,
                                     gint            *x_offset,
                                     gint            *y_offset,
                                     gint            *width,
                                     gint            *height)
{
  gfloat xalign, yalign;
  gint   xpad, ypad;
  GtkStyle *style  = gtk_widget_get_style (widget);
  auto self = ref(g_object);
  auto _x_offset = nullable(x_offset);
  auto _y_offset = nullable(y_offset);
  gint pixbuf_width, pixbuf_height;
  gint internal_width, internal_height;

  if (!pixbuf)
    create_pixbuf (widget);

  pixbuf_width  = gdk_pixbuf_get_width  (pixbuf);
  pixbuf_height = gdk_pixbuf_get_height (pixbuf);

  self[gtk_cell_renderer_get_alignment](&xalign, &yalign);
  self[gtk_cell_renderer_get_padding](&xpad, &ypad);

  internal_width  = (pixbuf_width  + (gint) xpad * 2 + style->xthickness * 2);
  internal_height = (pixbuf_height + (gint) ypad * 2 + style->ythickness * 2);

  if (cell_area) {
    gdouble align = ((ref(widget)[gtk_widget_get_direction]() == GTK_TEXT_DIR_RTL) ?
                    1.0 - xalign : xalign);

    _x_offset = MAX(align  * (cell_area->width  - internal_width),  0) + xpad;
    _y_offset = MAX(yalign * (cell_area->height - internal_height), 0) + ypad;
  } else {
    _x_offset = 0;
    _y_offset = 0;
  }

  *width  = internal_width  + 2 * xpad;
  *height = internal_height + 2 * ypad;
}

void
GLib::CellRendererPopup::create_pixbuf (GtkWidget              *widget)
{
  g_print("CellRendererPopup::create_pixbuf\n");
  if (pixbuf)
    g_object_unref (pixbuf);

  pixbuf = gtk_widget_render_icon (widget,
                                   stock_id,
                                   stock_size, NULL);
  g_return_if_fail(pixbuf != NULL);
}

void
GLib::CellRendererPopup::render (GdkWindow            *window,
                                   GtkWidget            *widget,
                                   GdkRectangle         *background_area,
                                   GdkRectangle         *cell_area,
                                   GdkRectangle         *expose_area,
                                   GtkCellRendererState  flags)
{
  auto          cell   = ref(GTK_CELL_RENDERER(g_object));
  auto          dashes = ref(Class::Traits::cast(g_object));
  GtkStyle*     style  = gtk_widget_get_style (widget);
  GdkRectangle  toggle_rect;
  GdkRectangle  draw_rect;
  GtkStateType  state;
  gint          xpad, ypad;
  cairo_t*      cr;
  gint          width;
  gint          x, y;

  get_size (widget, cell_area,
            &toggle_rect.x, &toggle_rect.y, &toggle_rect.width, &toggle_rect.height);
  cell[gtk_cell_renderer_get_padding] (&xpad, &ypad);

  toggle_rect.x      += cell_area->x + xpad;
  toggle_rect.y      += cell_area->y + ypad;
  toggle_rect.width  -= xpad * 2;
  toggle_rect.height -= ypad * 2;

  if (toggle_rect.width <= 0 || toggle_rect.height <= 0)
    return;
  if (! gtk_cell_renderer_get_sensitive (cell)) {
    state = GTK_STATE_INSENSITIVE;
  } else if ((flags & GTK_CELL_RENDERER_SELECTED) == GTK_CELL_RENDERER_SELECTED) {
    if (gtk_widget_has_focus (widget))
      state = GTK_STATE_SELECTED;
    else
      state = GTK_STATE_ACTIVE;
  } else if ((flags & GTK_CELL_RENDERER_PRELIT) == GTK_CELL_RENDERER_PRELIT &&
             gtk_widget_get_state (widget) == GTK_STATE_PRELIGHT) {
    state = GTK_STATE_PRELIGHT;
  } else {
    if (gtk_widget_is_sensitive (widget))
      state = GTK_STATE_NORMAL;
    else
      state = GTK_STATE_INSENSITIVE;
  }

  if (gdk_rectangle_intersect (expose_area, cell_area, &draw_rect) &&
      (flags & GTK_CELL_RENDERER_PRELIT))
    gtk_paint_shadow (style, window,
                      state, active ? GTK_SHADOW_IN : GTK_SHADOW_OUT,
                      &draw_rect, widget, NULL,
                      toggle_rect.x,     toggle_rect.y,
                      toggle_rect.width, toggle_rect.height);

  toggle_rect.x      += style->xthickness;
  toggle_rect.y      += style->ythickness;
  toggle_rect.width  -= style->xthickness * 2;
  toggle_rect.height -= style->ythickness * 2;

  if (gdk_rectangle_intersect (&draw_rect, &toggle_rect, &draw_rect)) {
    cairo_t  *cr = gdk_cairo_create (window);
    gboolean  inconsistent;

    gdk_cairo_rectangle (cr, &draw_rect);
    cairo_clip (cr);

    gdk_cairo_set_source_pixbuf (cr, pixbuf, toggle_rect.x, toggle_rect.y);
    cairo_paint (cr);
    cairo_destroy (cr);
  }
}

gboolean GLib::CellRendererPopup::activate (GdkEvent             *event,
                                              GtkWidget            *widget,
                                              const gchar          *path,
                                              GdkRectangle         *background_area,
                                              GdkRectangle         *cell_area,
                                              GtkCellRendererState  flags)
{
  g_print("CellRendererPopup::activate\n");
  GdkModifierType state = (GdkModifierType)0;

  if (event && ((GdkEventAny *) event)->type == GDK_BUTTON_PRESS)
    state = (GdkModifierType)(((GdkEventButton *) event)->state);

  clicked (G_OBJECT(widget), path, background_area);

  return TRUE;
}

void GLib::CellRendererPopup::clicked (GObject* widget, const gchar* path, GdkRectangle* cell_area)
{
  g_print("CellRendererPopup::clicked\n");
//  auto active = ref(g_object)["active"];
//  g_print("  property active = %s\n", active?"T":"F");
  g_signal_emit_by_name (g_object, "parent-clicked", widget, path, cell_area);
}
/*  public functions  */

GtkCellRenderer *
CellRendererPopupInterface::new_instance () {
  return GTK_CELL_RENDERER(g_object_new (GLib::Class::Traits::get_type(),
      "mode", GTK_CELL_RENDERER_MODE_ACTIVATABLE,
      NULL));
}

CellRendererPopupInterface*
CellRendererPopupInterface::cast(gpointer obj) {
  return dynamic_cast<CellRendererPopupInterface*>(GLib::Class::get_private(obj));
}

bool CellRendererPopupInterface::is_instance(gpointer obj) {
  return GLib::Class::Traits::is_instance(obj);
}

GType gimp_cell_renderer_popup_get_type() { return GLib::Class::Traits::get_type(); }
GtkCellRenderer* gimp_cell_renderer_popup_new() { return CellRendererPopupInterface::new_instance(); }
void
gimp_cell_renderer_popup_clicked(GimpCellRendererPopup* popup, GObject *widget, const gchar* path, GdkRectangle* cell_area)
{
  CellRendererPopupInterface::cast(popup)->clicked(widget, path, cell_area);
}
