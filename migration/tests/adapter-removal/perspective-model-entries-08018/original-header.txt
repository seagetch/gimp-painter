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

#ifndef __GIMP_PERSPECTIVE_GUIDE_H__
#define __GIMP_PERSPECTIVE_GUIDE_H__


#include "gimpobject.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GIMP_TYPE_PERSPECTIVE_GUIDE            (gimp_perspective_guide_get_type ())
#define GIMP_PERSPECTIVE_GUIDE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIMP_TYPE_PERSPECTIVE_GUIDE, GimpPerspectiveGuide))
#define GIMP_PERSPECTIVE_GUIDE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GIMP_TYPE_PERSPECTIVE_GUIDE, GimpPerspectiveGuideClass))
#define GIMP_IS_PERSPECTIVE_GUIDE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIMP_TYPE_PERSPECTIVE_GUIDE))
#define GIMP_IS_PERSPECTIVE_GUIDE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GIMP_TYPE_PERSPECTIVE_GUIDE))
#define GIMP_PERSPECTIVE_GUIDE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GIMP_TYPE_PERSPECTIVE_GUIDE, GimpPerspectiveGuideClass))


typedef struct _GimpPerspectiveGuideClass GimpPerspectiveGuideClass;

struct _GimpPerspectiveGuide
{
  GObject              parent_instance;
};

struct _GimpPerspectiveGuideClass
{
  GObjectClass         parent_class;

  /*  signals  */
  void     (* removed)             (GimpPerspectiveGuide  *guide);

};


GType               gimp_perspective_guide_get_type        (void) G_GNUC_CONST;

GimpPerspectiveGuide *         gimp_perspective_guide_new  (guint32              guide_ID);
gint     gimp_perspective_guide_get_vanish_point_length   (GimpPerspectiveGuide * guide);
gint     gimp_perspective_guide_add_vanish_points         (GimpPerspectiveGuide  *guide, gdouble x, gdouble y);
gboolean gimp_perspective_guide_remove_vanish_points      (GimpPerspectiveGuide  *guide, gint index);
gboolean gimp_perspective_guide_get_vanish_points    (GimpPerspectiveGuide  *guide, gint index, gdouble* x, gdouble* y);
gboolean gimp_perspective_guide_set_vanish_points    (GimpPerspectiveGuide  *guide, gint index, gdouble x, gdouble y);


#ifdef __cplusplus
}; // namespace

class PerspectiveGuideInterface {
public:

  // static methods

  static GimpPerspectiveGuide*      new_instance   (guint id);
  static PerspectiveGuideInterface* cast           (gpointer obj);
  static bool                       is_instance    (gpointer obj);

  // instance methods

  virtual ~PerspectiveGuideInterface () { };

  virtual void removed() = 0;

  virtual gint add_vanish_points(gdouble x, gdouble y) = 0;
  virtual gint get_vanish_point_length() = 0;  
  virtual gboolean remove_vanish_points(gint index) = 0;
  virtual gboolean get_vanish_points(gint index, gdouble* x, gdouble* y) = 0;
  virtual gboolean set_vanish_points(gint index, gdouble x, gdouble y) = 0;

};
__DECLARE_GTK_CLASS__(GimpPerspectiveGuide, GIMP_TYPE_PERSPECTIVE_GUIDE);
#endif

#endif /* __GIMP_PERSPECTIVE_GUIDE_H__ */