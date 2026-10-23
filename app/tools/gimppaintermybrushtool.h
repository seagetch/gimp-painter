/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_MYBRUSH_TOOL_H
#define GIMP_PAINTER_MYBRUSH_TOOL_H
#include "gimpcolortool.h"
G_BEGIN_DECLS
#define GIMP_TYPE_PAINTER_MYBRUSH_TOOL (gimp_painter_mybrush_tool_get_type ())
#define GIMP_PAINTER_MYBRUSH_TOOL(o) (G_TYPE_CHECK_INSTANCE_CAST ((o),GIMP_TYPE_PAINTER_MYBRUSH_TOOL,GimpPainterMybrushTool))
#define GIMP_IS_PAINTER_MYBRUSH_TOOL(o) (G_TYPE_CHECK_INSTANCE_TYPE ((o),GIMP_TYPE_PAINTER_MYBRUSH_TOOL))
#define GIMP_PAINTER_MYBRUSH_TOOL_CLASS(k) (G_TYPE_CHECK_CLASS_CAST ((k),GIMP_TYPE_PAINTER_MYBRUSH_TOOL,GimpPainterMybrushToolClass))
#define GIMP_IS_PAINTER_MYBRUSH_TOOL_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k),GIMP_TYPE_PAINTER_MYBRUSH_TOOL))
#define GIMP_PAINTER_MYBRUSH_TOOL_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o),GIMP_TYPE_PAINTER_MYBRUSH_TOOL,GimpPainterMybrushToolClass))
typedef struct {GimpColorTool parent_instance;gboolean binding_failed;} GimpPainterMybrushTool;
typedef struct {GimpColorToolClass parent_class;} GimpPainterMybrushToolClass;
GType gimp_painter_mybrush_tool_get_type (void) G_GNUC_CONST;
void gimp_painter_mybrush_tool_register (GimpToolRegisterCallback callback,gpointer data);
gboolean gimp_painter_mybrush_tool_has_pending_stroke (GimpPainterMybrushTool *tool);
/* Image-space anchor retained with all original device axes. */
gboolean gimp_painter_mybrush_tool_get_last_press (GimpPainterMybrushTool *tool,GimpCoords *coords);
G_END_DECLS
#endif
