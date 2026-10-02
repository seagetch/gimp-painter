/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_SMUDGE_TOOL_H
#define GIMP_PAINTER_SMUDGE_TOOL_H
#include "gimpbrushtool.h"
G_BEGIN_DECLS
#define GIMP_TYPE_PAINTER_SMUDGE_TOOL (gimp_painter_smudge_tool_get_type ())
#define GIMP_PAINTER_SMUDGE_TOOL(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIMP_TYPE_PAINTER_SMUDGE_TOOL, GimpPainterSmudgeTool))
#define GIMP_IS_PAINTER_SMUDGE_TOOL(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIMP_TYPE_PAINTER_SMUDGE_TOOL))
typedef struct { GimpBrushTool parent_instance; gboolean binding_failed; } GimpPainterSmudgeTool;
typedef struct { GimpBrushToolClass parent_class; } GimpPainterSmudgeToolClass;
GType    gimp_painter_smudge_tool_get_type (void) G_GNUC_CONST;
void     gimp_painter_smudge_tool_register (GimpToolRegisterCallback callback, gpointer data);
/* Owner-thread status, also useful to native lifecycle/latency tests. */
gboolean gimp_painter_smudge_tool_is_pending (GimpPainterSmudgeTool *tool);
G_END_DECLS
#endif
