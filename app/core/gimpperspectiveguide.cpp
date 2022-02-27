/* GIMP-painter - The GNU Image Manipulation Program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * GimpPerspectiveGuide
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

extern "C" {
#include <math.h>
#include "config.h"

#include <glib-object.h>

#include "libgimpbase/gimpbase.h"
#include "libgimpconfig/gimpconfig.h"

#include "core-types.h"

#include "gimpperspectiveguide.h"
#include "gimpmarshal.h"
    
};

using namespace GLib;

struct Point {
  gdouble x;
  gdouble y;
  Point(gdouble x, gdouble y): x(x), y(y) {};
};

class PerspectiveGuide : virtual public ImplBase, virtual public PerspectiveGuideInterface
{protected:
  guint id;
  List vanish_points;
  gdouble angle;
public:

  PerspectiveGuide(GObject* o);
  virtual ~PerspectiveGuide();

  //////////////////////////////////////////////
  // Implementation
  virtual void removed();

  //////////////////////////////////////////////
  // Public interface

  CopyValue get_id();
  void      set_id(IValue v);

  CopyValue get_angle();
  void      set_angle(IValue v);

  virtual gint add_vanish_points(gdouble x, gdouble y);
  virtual gint get_vanish_point_length();
  virtual gboolean remove_vanish_points(gint index);
  virtual gboolean get_vanish_points(gint index, gdouble* x, gdouble* y);
  virtual gboolean set_vanish_points(gint index, gdouble x, gdouble y);
};


extern const char gimp_perspective_guide_name[] = "GimpPerspectiveGuide";
using PerspectiveGuideClass = NewGClass<gimp_perspective_guide_name,
                                 UseCStructs<GObject, GimpPerspectiveGuide>,
                                 PerspectiveGuide>;
#define _override(method) PerspectiveGuideClass::__(&klass->method).bind<&PerspectiveGuide::method>()
#define _getter(method)   PerspectiveGuideClass::__((CopyValue (**)(GObject*))NULL).bind<&PerspectiveGuide::get_##method >()
#define _setter(method)   PerspectiveGuideClass::__((void (**)(GObject*, IValue))NULL).bind<&PerspectiveGuide::set_##method >()

static PerspectiveGuideClass class_instance([](IGClass::IWithClass*  klass) {
  klass->
    as_class<GimpPerspectiveGuide>([](GimpPerspectiveGuideClass* klass) {
      _override (removed);
    })->
    install_property(
        PerspectiveGuideClass::g_param_spec_new("id", PerspectiveGuideClass::Range<guint>(0, G_MAXUINT32), (guint)0, GParamFlags(GIMP_PARAM_READWRITE|G_PARAM_CONSTRUCT)),
        _getter(id), _setter(id)
    )->
    install_property(
        PerspectiveGuideClass::g_param_spec_new("angle", PerspectiveGuideClass::Range<gdouble>(0, M_PI), 0., GParamFlags(GIMP_PARAM_READWRITE|G_PARAM_CONSTRUCT)),
        _getter(id), _setter(id)
    )-> 
    install_signal("removed", G_TYPE_NONE, G_TYPE_NONE);
});

PerspectiveGuide::PerspectiveGuide(GObject* o) : ImplBase(o), vanish_points(NULL) 
{ 

}


PerspectiveGuide::~PerspectiveGuide() 
{ 
  IList<Point*> i_vanish_points = vanish_points;
  for (auto point: i_vanish_points) {
    delete point;
  }
}


void
PerspectiveGuide::removed() 
{ 
  g_signal_emit_by_name (g_object, "removed");
}


CopyValue 
PerspectiveGuide::get_id() {
  return id;
};


void 
PerspectiveGuide::set_id(IValue v) {
  id = v;
}


CopyValue 
PerspectiveGuide::get_angle() {
  return angle;
};


void 
PerspectiveGuide::set_angle(IValue v) {
  angle = v;
}


gint 
PerspectiveGuide::add_vanish_points(gdouble x, gdouble y)
{
  IList<Point*> i_vanish_points = vanish_points;
  gint length = g_list_length (vanish_points.ptr());
  if (length < 3) {
    i_vanish_points.append(new Point(x, y));
    return length;
  }
  return -1;
}


gint 
PerspectiveGuide::get_vanish_point_length()
{
  IList<Point*> i_vanish_points = vanish_points;
  gint length = g_list_length (vanish_points.ptr());
  return length;
}


gboolean 
PerspectiveGuide::remove_vanish_points(gint index)
{
  IList<Point*> i_vanish_points = vanish_points;
  if (index < g_list_length(vanish_points.ptr())) {
    i_vanish_points.remove ((Point*)g_list_nth_data(vanish_points.ptr(), index));
    return true;
  }
  return false;
}


gboolean 
PerspectiveGuide::get_vanish_points(gint index, gdouble* x, gdouble* y)
{
  IList<Point*> i_vanish_points = vanish_points;
  if (index < g_list_length(vanish_points.ptr())) {
    Point* point = i_vanish_points[index];
    *x = point->x;
    *y = point->y;
    return true;
  }
  return false;
}


gboolean 
PerspectiveGuide::set_vanish_points(gint index, gdouble x, gdouble y)
{
  IList<Point*> i_vanish_points = vanish_points;
  if (index < g_list_length(vanish_points.ptr())) {
    Point* point = i_vanish_points[index];
    point->x = x;
    point->y = y;
    return true;
  }
  return false;
}


//  g_object_notify (G_OBJECT (perspective_guide), "position");



////////////////////////////////////////////////////////////////////////////
// C++ interfaces
GimpPerspectiveGuide*
PerspectiveGuideInterface::new_instance (guint id) {
  return GIMP_PERSPECTIVE_GUIDE( g_object_new(PerspectiveGuideClass::Traits::get_type(),
                                 "id", id,
                                 NULL) );
}

PerspectiveGuideInterface*
PerspectiveGuideInterface::cast(gpointer obj) {
  return dynamic_cast<PerspectiveGuideInterface*>(PerspectiveGuideClass::get_private(obj));
}

bool PerspectiveGuideInterface::is_instance(gpointer obj) {
  return PerspectiveGuideClass::Traits::is_instance(obj);
}

////////////////////////////////////////////////////////////////////////////
// C compatibility functions
extern "C" {
GType gimp_perspective_guide_get_type()
{
  return PerspectiveGuideClass::get_type();
}

GimpPerspectiveGuide* gimp_perspective_guide_new (guint id)
{
  return PerspectiveGuideInterface::new_instance(id);
}

gint     gimp_perspective_guide_get_vanish_point_length   (GimpPerspectiveGuide * guide)
{
  return PerspectiveGuideInterface::cast(guide)->get_vanish_point_length();
}

gint     gimp_perspective_guide_add_vanish_points         (GimpPerspectiveGuide  *guide, gdouble x, gdouble y)
{
  return PerspectiveGuideInterface::cast(guide)->add_vanish_points(x, y);
}

gboolean gimp_perspective_guide_remove_vanish_points      (GimpPerspectiveGuide  *guide, gint index)
{
  return PerspectiveGuideInterface::cast(guide)->remove_vanish_points(index);
}

gboolean gimp_perspective_guide_get_vanish_points    (GimpPerspectiveGuide  *guide, gint index, gdouble* x, gdouble* y)
{
  return PerspectiveGuideInterface::cast(guide)->get_vanish_points(index, x, y);
}

gboolean gimp_perspective_guide_set_vanish_points    (GimpPerspectiveGuide  *guide, gint index, gdouble x, gdouble y) 
{
  return PerspectiveGuideInterface::cast(guide)->set_vanish_points(index, x, y);
}

};
