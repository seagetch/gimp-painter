/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef __GIMP_PERSPECTIVE_GUIDE_TOOL_H__
#define __GIMP_PERSPECTIVE_GUIDE_TOOL_H__
#include "gimpdrawtool.h"
G_BEGIN_DECLS
#define GIMP_TYPE_PERSPECTIVE_GUIDE_TOOL (gimp_perspective_guide_tool_get_type ())
#define GIMP_IS_PERSPECTIVE_GUIDE_TOOL(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIMP_TYPE_PERSPECTIVE_GUIDE_TOOL))
typedef struct _GimpPerspectiveGuideTool { GimpDrawTool parent; gboolean binding_failed; } GimpPerspectiveGuideTool;
typedef struct _GimpPerspectiveGuideToolClass { GimpDrawToolClass parent; } GimpPerspectiveGuideToolClass;
GType gimp_perspective_guide_tool_get_type (void) G_GNUC_CONST;
void gimp_perspective_guide_tool_register (GimpToolRegisterCallback, gpointer);
G_END_DECLS
#endif
