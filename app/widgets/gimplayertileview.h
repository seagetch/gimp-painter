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

#ifndef __GIMP_LAYER_TILE_VIEW_H__
#define __GIMP_LAYER_TILE_VIEW_H__

#ifdef __cplusplus
extern "C" {
#endif

#define GIMP_TYPE_LAYER_TILE_VIEW            (gimp_layer_tile_view_get_type ())
#define GIMP_LAYER_TILE_VIEW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIMP_TYPE_LAYER_TILE_VIEW, GimpLayerTileView))
#define GIMP_LAYER_TILE_VIEW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GIMP_TYPE_LAYER_TILE_VIEW, GimpLayerTileViewClass))
#define GIMP_IS_LAYER_TILE_VIEW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIMP_TYPE_LAYER_TILE_VIEW))
#define GIMP_IS_LAYER_TILE_VIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GIMP_TYPE_LAYER_TILE_VIEW))
#define GIMP_LAYER_TILE_VIEW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GIMP_TYPE_LAYER_TILE_VIEW, GimpLayerTileViewClass))


typedef struct _GimpLayerTileView      GimpLayerTileView;
typedef struct _GimpLayerTileViewClass GimpLayerTileViewClass;

struct _GimpLayerTileView
{
  GtkBox   parent_instance;
};

struct _GimpLayerTileViewClass
{
  GtkBoxClass  parent_class;
};


GType             gimp_layer_tile_view_get_type (void) G_GNUC_CONST;

GimpLayerTileView * gimp_layer_tile_view_new      (void);
#ifdef __cplusplus
}

class LayerTileViewInterface {
public:
  static GimpLayerTileView*          new_instance   ();
  static LayerTileViewInterface*     cast           (gpointer obj);
  static bool                        is_instance    (gpointer obj);
  virtual void                       dummy          () = 0;
};

#include "base/glib-cxx-types.hpp"
__DECLARE_GTK_CLASS__(GimpLayerTileView, GIMP_TYPE_LAYER_TILE_VIEW);
#endif

#endif /* __GIMP_LAYER_TILE_VIEW_H__ */