/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_FILL_BRUSH_TOOL_H
#define GIMP_FILL_BRUSH_TOOL_H
#include "gimpbrushtool.h"
G_BEGIN_DECLS
#define GIMP_TYPE_FILL_BRUSH_TOOL (gimp_fill_brush_tool_get_type ())
#define GIMP_FILL_BRUSH_TOOL(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIMP_TYPE_FILL_BRUSH_TOOL, GimpFillBrushTool))
#define GIMP_IS_FILL_BRUSH_TOOL(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIMP_TYPE_FILL_BRUSH_TOOL))
typedef struct { GimpBrushTool parent_instance; gboolean binding_failed; } GimpFillBrushTool;
typedef struct { GimpBrushToolClass parent_class; } GimpFillBrushToolClass;
GType    gimp_fill_brush_tool_get_type (void) G_GNUC_CONST;
void     gimp_fill_brush_tool_register (GimpToolRegisterCallback callback, gpointer data);
/* Owner-thread status, also useful to native lifecycle/latency tests. */
gboolean gimp_fill_brush_tool_is_pending (GimpFillBrushTool *tool);
G_END_DECLS
#endif
