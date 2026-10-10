/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * GimpCloneLayer
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

#ifndef __GIMP_CLONE_LAYER_H__
#define __GIMP_CLONE_LAYER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "gimplayer.h"

#define GIMP_TYPE_CLONE_LAYER            (gimp_clone_layer_get_type ())
#define GIMP_CLONE_LAYER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIMP_TYPE_CLONE_LAYER, GimpCloneLayer))
#define GIMP_CLONE_LAYER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GIMP_TYPE_CLONE_LAYER, GimpCloneLayerClass))
#define GIMP_IS_CLONE_LAYER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIMP_TYPE_CLONE_LAYER))
#define GIMP_IS_CLONE_LAYER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GIMP_TYPE_CLONE_LAYER))
#define GIMP_CLONE_LAYER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GIMP_TYPE_CLONE_LAYER, GimpCloneLayerClass))

struct _GimpCloneLayer
{
  GimpLayer  parent_instance;
};

struct GimpCloneLayerClass
{
  GimpLayerClass parent_class;
};

GType            gimp_clone_layer_get_type       (void) G_GNUC_CONST;
GimpLayer*       gimp_clone_layer_new            (GimpImage*           image,
                                                  GimpLayer*           source,
                                                  gint                 width,
                                                  gint                 height,
                                                  const gchar*         name,
                                                  gdouble              opacity,
                                                  GimpLayerModeEffects mode);
void             gimp_clone_layer_set_source     (GimpCloneLayer*      layer,
                                                  GimpLayer*           source);
GimpLayer*       gimp_clone_layer_get_source     (GimpCloneLayer*      layer);
void         gimp_clone_layer_set_source_by_name (GimpCloneLayer* layer,
                                                  const gchar* name);

#ifdef __cplusplus
}

class CloneLayerInterface {
public:
  static GimpLayer*           new_instance   (GimpImage*            image,
                                              GimpLayer*            source,
                                              gint                  width,
                                              gint                  height,
                                              const gchar*          name,
                                              gdouble               opacity,
                                              GimpLayerModeEffects  mode);
  static CloneLayerInterface* cast           (gpointer obj);
  static bool                 is_instance    (gpointer obj);

  virtual void       set_source (GimpLayer* layer) = 0;
  virtual GimpLayer* get_source () = 0;
  virtual void       set_source_by_name (const gchar* ref_name) = 0;
};

#include "base/glib-cxx-types.hpp"
namespace GLib {
template<> inline GType g_type<GimpCloneLayer>() { return gimp_clone_layer_get_type(); }
template<> inline auto g_class_type<GimpCloneLayer>(const GimpCloneLayer* instance) { return GIMP_CLONE_LAYER_GET_CLASS(instance); }
};
#endif

#endif /* __GIMP_CLONE_LAYER_H__ */
