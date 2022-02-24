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

#ifndef __GIMP_TOOL_TILE_VIEW_H__
#define __GIMP_TOOL_TILE_VIEW_H__

#ifdef __cplusplus
extern "C" {
#endif

#define GIMP_TYPE_TOOL_TILE_VIEW            (gimp_tool_tile_view_get_type ())
#define GIMP_TOOL_TILE_VIEW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIMP_TYPE_TOOL_TILE_VIEW, GimpToolTileView))
#define GIMP_TOOL_TILE_VIEW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GIMP_TYPE_TOOL_TILE_VIEW, GimpToolTileViewClass))
#define GIMP_IS_TOOL_TILE_VIEW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIMP_TYPE_TOOL_TILE_VIEW))
#define GIMP_IS_TOOL_TILE_VIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GIMP_TYPE_TOOL_TILE_VIEW))
#define GIMP_TOOL_TILE_VIEW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GIMP_TYPE_TOOL_TILE_VIEW, GimpToolTileViewClass))


typedef struct _GimpToolTileView      GimpToolTileView;
typedef struct _GimpToolTileViewClass GimpToolTileViewClass;

struct _GimpToolTileView
{
  GtkBox   parent_instance;
};

struct _GimpToolTileViewClass
{
  GtkBoxClass  parent_class;
};


GType             gimp_tool_tile_view_get_type (void) G_GNUC_CONST;

GimpToolTileView * gimp_tool_tile_view_new      (void);
#ifdef __cplusplus
}

class ToolTileViewInterface {
public:
  static GimpToolTileView*          new_instance   ();
  static ToolTileViewInterface*     cast           (gpointer obj);
  static bool                        is_instance    (gpointer obj);
  virtual void                       dummy          () = 0;
};

#include "base/glib-cxx-types.hpp"
__DECLARE_GTK_CLASS__(GimpToolTileView, GIMP_TYPE_TOOL_TILE_VIEW);
#endif

#endif /* __GIMP_TOOL_TILE_VIEW_H__ */