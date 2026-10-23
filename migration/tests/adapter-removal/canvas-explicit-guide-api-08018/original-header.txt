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

#ifndef __GIMP_CANVAS_PERSPECTIVE_GUIDE_H__
#define __GIMP_CANVAS_PERSPECTIVE_GUIDE_H__


#ifdef __cplusplus
extern "C" {
#endif

#define GIMP_TYPE_CANVAS_PERSPECTIVE_GUIDE            (gimp_canvas_perspective_guide_get_type ())
#define GIMP_CANVAS_PERSPECTIVE_GUIDE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIMP_TYPE_CANVAS_PERSPECTIVE_GUIDE, GimpCanvasPerspectiveGuide))
#define GIMP_CANVAS_PERSPECTIVE_GUIDE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GIMP_TYPE_CANVAS_PERSPECTIVE_GUIDE, GimpCanvasPerspectiveGuideClass))
#define GIMP_IS_CANVAS_PERSPECTIVE_GUIDE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIMP_TYPE_CANVAS_PERSPECTIVE_GUIDE))
#define GIMP_IS_CANVAS_PERSPECTIVE_GUIDE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GIMP_TYPE_CANVAS_PERSPECTIVE_GUIDE))
#define GIMP_CANVAS_PERSPECTIVE_GUIDE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GIMP_TYPE_CANVAS_PERSPECTIVE_GUIDE, GimpCanvasPerspectiveGuideClass))


typedef struct _GimpCanvasPerspectiveGuideClass GimpCanvasPerspectiveGuideClass;
typedef struct _GimpCanvasPerspectiveGuide      GimpCanvasPerspectiveGuide;

struct _GimpCanvasPerspectiveGuide
{
  GimpCanvasItem  parent_instance;
};

struct _GimpCanvasPerspectiveGuideClass
{
  GimpCanvasItemClass         parent_class;

  /*  signals  */
};


GType               gimp_canvas_perspective_guide_get_type        (void) G_GNUC_CONST;

GimpCanvasPerspectiveGuide *         gimp_canvas_perspective_guide_new  (GimpDisplayShell* shell, GimpPerspectiveGuide* guide);


#ifdef __cplusplus
}; // namespace

class CanvasPerspectiveGuideInterface {
public:

  // static methods

  static GimpCanvasPerspectiveGuide*      new_instance   (GimpDisplayShell* shell, GimpPerspectiveGuide* guide);
  static CanvasPerspectiveGuideInterface* cast           (gpointer obj);
  static bool                       is_instance    (gpointer obj);

  // instance methods

  virtual ~CanvasPerspectiveGuideInterface () { };

  virtual void draw(GimpDisplayShell *shell, cairo_t          *cr) = 0;
  virtual cairo_region_t * get_extents(GimpDisplayShell *shell) = 0;
  virtual void stroke (GimpDisplayShell *shell, cairo_t          *cr) = 0;
};
__DECLARE_GTK_CLASS__(GimpCanvasPerspectiveGuide, GIMP_TYPE_CANVAS_PERSPECTIVE_GUIDE);
#endif

#endif /* __GIMP_CANVAS_PERSPECTIVE_GUIDE_H__ */