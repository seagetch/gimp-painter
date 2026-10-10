/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * GimpFilterLayer
 * Copyright (C) 2017 seagetch <sigetch@gmail.com>
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

#ifndef __GIMP_FILTER_LAYER_H__
#define __GIMP_FILTER_LAYER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "gimplayer.h"

#define GIMP_TYPE_FILTER_LAYER            (gimp_filter_layer_get_type ())
#define GIMP_FILTER_LAYER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIMP_TYPE_FILTER_LAYER, GimpFilterLayer))
#define GIMP_FILTER_LAYER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GIMP_TYPE_FILTER_LAYER, GimpFilterLayerClass))
#define GIMP_IS_FILTER_LAYER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIMP_TYPE_FILTER_LAYER))
#define GIMP_IS_FILTER_LAYER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GIMP_TYPE_FILTER_LAYER))
#define GIMP_FILTER_LAYER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GIMP_TYPE_FILTER_LAYER, GimpFilterLayerClass))

struct _GimpFilterLayer
{
  GimpLayer  parent_instance;
};

struct GimpFilterLayerClass
{
  GimpLayerClass parent_class;
};

GType            gimp_filter_layer_get_type       (void) G_GNUC_CONST;
GimpLayer      * gimp_filter_layer_new            (GimpImage            *image,
                                                   gint                  width,
                                                   gint                  height,
                                                   const gchar          *name,
                                                   gdouble               opacity,
                                                   GimpLayerModeEffects  mode);
const gchar*     gimp_filter_layer_get_procedure  (GimpFilterLayer* layer);
GArray*          gimp_filter_layer_get_procedure_args  (GimpFilterLayer* layer);
void             gimp_filter_layer_set_procedure  (GimpFilterLayer* layer,
                                                   const gchar*     proc_name,
                                                   GArray*          args);
void             gimp_filter_layer_mark_as_loaded (GimpFilterLayer* layer);

#ifdef __cplusplus
}

class FilterLayerInterface {
public:
  static GimpLayer*               new_instance   (GimpImage*            image,
                                                  gint                  width,
                                                  gint                  height,
                                                  const gchar          *name,
                                                  gdouble               opacity,
                                                  GimpLayerModeEffects  mode);
  static FilterLayerInterface*    cast           (gpointer obj);
  static bool                     is_instance    (gpointer obj);

  virtual void                    set_procedure(const char* proc_name, GArray* args = NULL) = 0;
  virtual const char*             get_procedure() = 0;
  virtual GArray*                 get_procedure_args() = 0;
  virtual GValue                  get_procedure_arg(int index) = 0;
  virtual void                    set_procedure_arg(int index, GValue value) = 0;
  virtual void                    mark_as_loaded() = 0;
};

#include "base/glib-cxx-types.hpp"
namespace GLib {
template<> inline GType g_type<GimpFilterLayer>() { return gimp_filter_layer_get_type(); }
template<> inline auto g_class_type<GimpFilterLayer>(const GimpFilterLayer* instance) { return GIMP_FILTER_LAYER_GET_CLASS(instance); }
};
#endif

#endif /* __GIMP_FILTER_LAYER_H__ */
