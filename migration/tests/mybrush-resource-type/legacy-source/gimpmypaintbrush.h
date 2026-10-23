/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
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

#ifndef __GIMP_MYPAINT_BRUSH_H__
#define __GIMP_MYPAINT_BRUSH_H__

typedef struct _GimpMypaintBrushClass GimpMypaintBrushClass;

#include "gimpdata.h"
#include "mypaintbrush-enum-settings.h"


#define GIMP_TYPE_MYPAINT_BRUSH            (gimp_mypaint_brush_get_type ())
#define GIMP_MYPAINT_BRUSH(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIMP_TYPE_MYPAINT_BRUSH, GimpMypaintBrush))
#define GIMP_MYPAINT_BRUSH_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GIMP_TYPE_MYPAINT_BRUSH, GimpMypaintBrushClass))
#define GIMP_IS_MYPAINT_BRUSH(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIMP_TYPE_MYPAINT_BRUSH))
#define GIMP_IS_MYPAINT_BRUSH_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GIMP_TYPE_MYPAINT_BRUSH))
#define GIMP_MYPAINT_BRUSH_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GIMP_TYPE_MYPAINT_BRUSH, GimpMypaintBrushClass))



struct _GimpMypaintBrush
{
  GimpData        parent_instance;
  gint            use_count;  /*  for keeping the caches alive   */
  gpointer        p;
};

struct _GimpMypaintBrushClass
{
  GimpDataClass  parent_class;

  /*  virtual functions  */
  void              (* begin_use)          (GimpMypaintBrush        *mypaint_brush);
  void              (* end_use)            (GimpMypaintBrush        *mypaint_brush);
  GimpMypaintBrush* (* select_mypaint_brush)(GimpMypaintBrush        *mypaint_brush,
                                             const GimpCoords *last_coords,
                                             const GimpCoords *current_coords);
  gboolean          (* want_null_motion)    (GimpMypaintBrush        *mypaint_brush,
                                             const GimpCoords *last_coords,
                                             const GimpCoords *current_coords);
};


GType                  gimp_mypaint_brush_get_type            (void) G_GNUC_CONST;

GimpData             * gimp_mypaint_brush_new                 (GimpContext      *context,
                                                               const gchar      *name);
GimpData             * gimp_mypaint_brush_get_standard        (GimpContext      *context);

void                   gimp_mypaint_brush_begin_use           (GimpMypaintBrush        *mypaint_brush);
void                   gimp_mypaint_brush_end_use             (GimpMypaintBrush        *mypaint_brush);

GimpMypaintBrush     * gimp_mypaint_brush_select_mypaint_brush(GimpMypaintBrush        *mypaint_brush,
                                                               const GimpCoords *last_coords,
                                                               const GimpCoords *current_coords);
gboolean               gimp_mypaint_brush_want_null_motion    (GimpMypaintBrush        *mypaint_brush,
                                                               const GimpCoords *last_coords,
                                                               const GimpCoords *current_coords);
GimpData              * gimp_mypaint_brush_duplicate         (GimpMypaintBrush *mypaint_brush);

#ifdef __cplusplus
extern "C++" {
#include "base/glib-cxx-types.hpp"
__DECLARE_GTK_CAST__(GimpMypaintBrush, GIMP_MYPAINT_BRUSH, gimp_mypaint_brush);
};
#endif

#endif /* __GIMP_MYPAINT_BRUSH_H__ */
