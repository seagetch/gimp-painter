/* GIMP-painter - The GNU Image Manipulation Program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * GimpCanvasPerspectiveGuide
 * Copyright (C) 2003  Henrik Brix Andersen <brix@gimp.org>
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
#include "base/glib-cxx-types.hpp"
__DECLARE_GTK_CLASS__(GObject, G_TYPE_OBJECT);
#include "base/glib-cxx-impl.hpp"
#include "base/glib-cxx-utils.hpp"
#include <functional>


extern "C" {
#include <math.h>
#include <gegl.h>
#include <gtk/gtk.h>

#include "config.h"

#include "libgimpbase/gimpbase.h"
#include "libgimpconfig/gimpconfig.h"

#include "core/core-types.h"
#include "core/gimpperspectiveguide.h"

#include "display-types.h"
#include "gimpcanvas.h"
#include "gimpcanvasitem.h"
#include "gimpcanvasperspectiveguide.h"
#include "gimpdisplayshell.h"
#include "gimpdisplayshell-rotate.h"
    
};

using namespace GLib;

class CanvasPerspectiveGuide : virtual public ImplBase, virtual public CanvasPerspectiveGuideInterface
{
protected:
  IObject<GimpPerspectiveGuide> guide;
public:

  CanvasPerspectiveGuide(GObject* o);
  virtual ~CanvasPerspectiveGuide();

  //////////////////////////////////////////////
  // Implementation
  virtual void draw(GimpDisplayShell *shell, cairo_t *cr);
  virtual cairo_region_t * get_extents(GimpDisplayShell *shell);
  virtual void stroke (GimpDisplayShell *shell, cairo_t *cr);

  //////////////////////////////////////////////
  // Public interface

  CopyValue get_guide();
  void      set_guide(IValue v);

};


extern const char gimp_canvas_perspective_guide_name[] = "GimpCanvasPerspectiveGuide";
using CanvasPerspectiveGuideClass = NewGClass<gimp_canvas_perspective_guide_name,
                                 UseCStructs<GimpCanvasItem, GimpCanvasPerspectiveGuide>,
                                 CanvasPerspectiveGuide>;
#define _override(method) CanvasPerspectiveGuideClass::__(&klass->method).bind<&CanvasPerspectiveGuide::method>()
#define _getter(method)   CanvasPerspectiveGuideClass::__((CopyValue (**)(GObject*))NULL).bind<&CanvasPerspectiveGuide::get_##method >()
#define _setter(method)   CanvasPerspectiveGuideClass::__((void (**)(GObject*, IValue))NULL).bind<&CanvasPerspectiveGuide::set_##method >()

static CanvasPerspectiveGuideClass class_instance([](IGClass::IWithClass*  klass) {
  klass->
    as_class<GimpCanvasItem>([](GimpCanvasItemClass * klass) {
      _override (draw);
      _override (get_extents);
//      _override (stroke);
    })->
    as_class<GimpCanvasPerspectiveGuide>([](GimpCanvasPerspectiveGuideClass* klass) {
    })->
    install_property(
        CanvasPerspectiveGuideClass::g_param_spec_object("guide", GIMP_TYPE_PERSPECTIVE_GUIDE, GParamFlags(GIMP_PARAM_READWRITE|G_PARAM_CONSTRUCT)),
        _getter(guide), _setter(guide)
    );
});

CanvasPerspectiveGuide::CanvasPerspectiveGuide(GObject* o) : ImplBase(o), guide(NULL)
{ 

}


CanvasPerspectiveGuide::~CanvasPerspectiveGuide() 
{

}


CopyValue 
CanvasPerspectiveGuide::get_guide() {
  return guide.ptr();
};


void 
CanvasPerspectiveGuide::set_guide(IValue v) {
  const GValue* val = v.ptr();
  guide = GIMP_PERSPECTIVE_GUIDE(g_value_get_object(val));
}


void 
CanvasPerspectiveGuide::draw(GimpDisplayShell *shell, cairo_t *cr)
{
  if (guide) {
    auto cairo_draw = [&](std::function<void(void)> callback) {
      cairo_save (cr);
      gimp_display_shell_set_cairo_rotate(shell, cr);
      cairo_translate (cr, -shell->offset_x, -shell->offset_y);
      cairo_scale (cr, shell->scale_x, shell->scale_y);
      callback();
  //    cairo_translate (cr, private->x, private->y);

  //    cairo_append_path (cr, private->path);
      cairo_restore (cr);
    };

    auto i_guide = PerspectiveGuideInterface::cast(guide);
    gint length = i_guide->get_vanish_point_length();
    switch (length) {
    case 1:
//      g_print("Display one vanishment point.\n");
      cairo_draw(std::function<void(void)>([&]() {
        gdouble x, y;
        i_guide->get_vanish_points(0, &x, &y);
        cairo_move_to(cr, x, y);
        cairo_arc(cr, x, y, 8 / shell->scale_x, 0, 2*M_PI);
      }));
      break;
    case 2:
//      g_print("Display two vanishment points.\n");
      cairo_draw(std::function<void(void)>([&]() {
        for (int i = 0; i < 2; i ++) {
          gdouble x, y;
          i_guide->get_vanish_points(i, &x, &y);
          if (i > 0)
            cairo_line_to(cr, x, y);
          cairo_arc(cr, x, y, 8 / shell->scale_x, 0, 2*M_PI);
          cairo_move_to(cr, x, y);
        }
      }));
      break;
    case 3:
//      g_print("Display three vanishment points.\n");
      cairo_draw(std::function<void(void)>([&]() {
        for (int i = 0; i < 3; i ++) {
          gdouble x, y;
          i_guide->get_vanish_points(i, &x, &y);
          cairo_move_to(cr, x, y);
          cairo_arc(cr, x, y, 8 / shell->scale_x, 0, 2*M_PI);
        }
      }));
      break;
    default:
      break;
    }

    ref(g_object) [_gimp_canvas_item_stroke] (cr);
  }
}


cairo_region_t* 
CanvasPerspectiveGuide::get_extents(GimpDisplayShell *shell)
{
  if (!guide || ! gtk_widget_get_realized(shell->canvas))
    return NULL;

  cairo_rectangle_int_t  rectangle;
  gdouble                x1, y1, x2, y2;

  auto i_guide = PerspectiveGuideInterface::cast(guide);
  gint length = i_guide->get_vanish_point_length();
  switch (length) {
  case 1:
//    g_print("Display one vanishment point.\n");
    gdouble x, y;
    i_guide->get_vanish_points(0, &x, &y);
    rectangle.x      = floor (x - 8 / shell->scale_x - 1.0);
    rectangle.y      = floor (y - 8 / shell->scale_x - 1.0);
    rectangle.width  = ceil (x + 8 / shell->scale_x + 1.0) - rectangle.x;
    rectangle.height = ceil (y + 8 / shell->scale_x + 1.0) - rectangle.y;
    break;
  case 2:
//    g_print("Display two vanishment points.\n");
    for (int i = 0; i < 2; i ++) {
      gdouble x, y;
      i_guide->get_vanish_points(0, &x, &y);
      x1 = floor (x - 8 / shell->scale_x - 1.0);
      y1 = floor (y - 8 / shell->scale_x - 1.0);
      x2 = ceil (x + 8 / shell->scale_x + 1.0);
      y2 = ceil (y + 8 / shell->scale_x + 1.0);
      if (i == 0) {
        rectangle.x = x1;
        rectangle.y = y1;
        rectangle.width = x2;
        rectangle.height = y2;
      } else {
        rectangle.x = MIN(rectangle.x, x1);
        rectangle.y = MIN(rectangle.y, y1);
        rectangle.width = MAX(rectangle.x, x2);
        rectangle.height = MAX(rectangle.y, y2);
      }
    }
    rectangle.width -= rectangle.x;
    rectangle.height -= rectangle.y;
    break;
  case 3:
//    g_print("Display three vanishment points.\n");
    for (int i = 0; i < 3; i ++) {
      gdouble x, y;
      i_guide->get_vanish_points(0, &x, &y);
      x1 = floor (x - 8 / shell->scale_x - 1.0);
      y1 = floor (y - 8 / shell->scale_x - 1.0);
      x2 = ceil (x + 8 / shell->scale_x + 1.0);
      y2 = ceil (y + 8 / shell->scale_x + 1.0);
      if (i == 0) {
        rectangle.x = x1;
        rectangle.y = y1;
        rectangle.width = x2;
        rectangle.height = y2;
      } else {
        rectangle.x = MIN(rectangle.x, x1);
        rectangle.y = MIN(rectangle.y, y1);
        rectangle.width = MAX(rectangle.x, x2);
        rectangle.height = MAX(rectangle.y, y2);
      }
    }
    rectangle.width -= rectangle.x;
    rectangle.height -= rectangle.y;
    break;
  default:
    break;
  }
//  g_print("rectangle:%d,%d-%d,%d\n", rectangle.x, rectangle.y, rectangle.width, rectangle.height);

  return cairo_region_create_rectangle (&rectangle);
}


void 
CanvasPerspectiveGuide::stroke (GimpDisplayShell *shell, cairo_t *cr)
{

}



////////////////////////////////////////////////////////////////////////////
// C++ interfaces
GimpCanvasPerspectiveGuide*
CanvasPerspectiveGuideInterface::new_instance (GimpDisplayShell* shell, GimpPerspectiveGuide* guide) {
  return GIMP_CANVAS_PERSPECTIVE_GUIDE( g_object_new(CanvasPerspectiveGuideClass::Traits::get_type(),
                                 "shell", shell,
                                 "guide", guide,
                                 NULL) );
}

CanvasPerspectiveGuideInterface*
CanvasPerspectiveGuideInterface::cast(gpointer obj) {
  return dynamic_cast<CanvasPerspectiveGuideInterface*>(CanvasPerspectiveGuideClass::get_private(obj));
}

bool CanvasPerspectiveGuideInterface::is_instance(gpointer obj) {
  return CanvasPerspectiveGuideClass::Traits::is_instance(obj);
}

////////////////////////////////////////////////////////////////////////////
// C compatibility functions
extern "C" {
GType gimp_canvas_perspective_guide_get_type()
{
  return CanvasPerspectiveGuideClass::get_type();
}

GimpCanvasPerspectiveGuide* gimp_canvas_perspective_guide_new (GimpDisplayShell* shell, GimpPerspectiveGuide* guide)
{
  return CanvasPerspectiveGuideInterface::new_instance(shell, guide);
}

};
