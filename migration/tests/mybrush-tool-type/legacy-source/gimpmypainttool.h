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

#ifndef __GIMP_MYPAINT_TOOL_H__
#define __GIMP_MYPAINT_TOOL_H__


#include "gimpcolortool.h"
#include "tools-types.h"


#define GIMP_TYPE_MYPAINT_TOOL            (gimp_mypaint_tool_get_type ())
#define GIMP_MYPAINT_TOOL(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIMP_TYPE_MYPAINT_TOOL, GimpMypaintTool))
#define GIMP_MYPAINT_TOOL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GIMP_TYPE_MYPAINT_TOOL, GimpMypaintToolClass))
#define GIMP_IS_MYPAINT_TOOL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIMP_TYPE_MYPAINT_TOOL))
#define GIMP_IS_MYPAINT_TOOL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GIMP_TYPE_MYPAINT_TOOL))
#define GIMP_MYPAINT_TOOL_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GIMP_TYPE_MYPAINT_TOOL, GimpMypaintToolClass))

#define GIMP_MYPAINT_TOOL_GET_OPTIONS(t)  (GIMP_MYPAINT_OPTIONS (gimp_tool_get_options (GIMP_TOOL (t))))


typedef struct _GimpMypaintToolClass GimpMypaintToolClass;

struct _GimpMypaintTool
{
  GimpColorTool  parent_instance;

  gboolean       pick_colors;  /*  pick color if ctrl is pressed   */
  gboolean       draw_line;

  const gchar   *status;       /* status message */
  const gchar   *status_line;  /* status message when drawing a line */
  const gchar   *status_ctrl;  /* additional message for the ctrl modifier */
  
  GimpCoords curr_coords;
  guint32 last_event_time;
  gdouble last_event_x;
  gdouble last_event_y;
  gdouble last_painting_x;
  gdouble last_painting_y;
  
  guint32 last_flush_time;

  gpointer core;
};

struct _GimpMypaintToolClass
{
  GimpColorToolClass  parent_class;
};


GType   gimp_mypaint_tool_get_type            (void) G_GNUC_CONST;

void    gimp_mypaint_tool_enable_color_picker (GimpMypaintTool     *tool,
                                             GimpColorPickMode  mode);

void    gimp_mypaint_tool_register (GimpToolRegisterCallback  callback,
                                     gpointer                  data);


#endif  /*  __GIMP_MYPAINT_TOOL_H__  */
