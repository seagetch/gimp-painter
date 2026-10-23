/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef __GIMP_PERSPECTIVE_GUIDE_TOOL_H__
#define __GIMP_PERSPECTIVE_GUIDE_TOOL_H__
#include "gimpdrawtool.h"
#include "core/gimptooloptions.h"
G_BEGIN_DECLS
/* Original empty options identity, now a native GimpToolOptions subtype. */
#define GIMP_TYPE_PERSPECTIVE_GUIDE_OPTIONS (gimp_perspective_guide_options_get_type ())
#define GIMP_PERSPECTIVE_GUIDE_OPTIONS(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIMP_TYPE_PERSPECTIVE_GUIDE_OPTIONS, GimpPerspectiveGuideOptions))
#define GIMP_IS_PERSPECTIVE_GUIDE_OPTIONS(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIMP_TYPE_PERSPECTIVE_GUIDE_OPTIONS))
#define GIMP_PERSPECTIVE_GUIDE_OPTIONS_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), GIMP_TYPE_PERSPECTIVE_GUIDE_OPTIONS, GimpPerspectiveGuideOptionsClass))
#define GIMP_IS_PERSPECTIVE_GUIDE_OPTIONS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GIMP_TYPE_PERSPECTIVE_GUIDE_OPTIONS))
#define GIMP_PERSPECTIVE_GUIDE_OPTIONS_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GIMP_TYPE_PERSPECTIVE_GUIDE_OPTIONS, GimpPerspectiveGuideOptionsClass))
typedef struct _GimpPerspectiveGuideOptions { GimpToolOptions parent; } GimpPerspectiveGuideOptions;
typedef struct _GimpPerspectiveGuideOptionsClass { GimpToolOptionsClass parent; } GimpPerspectiveGuideOptionsClass;
GType gimp_perspective_guide_options_get_type (void) G_GNUC_CONST;
#define GIMP_TYPE_PERSPECTIVE_GUIDE_TOOL (gimp_perspective_guide_tool_get_type ())
#define GIMP_PERSPECTIVE_GUIDE_TOOL(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIMP_TYPE_PERSPECTIVE_GUIDE_TOOL, GimpPerspectiveGuideTool))
#define GIMP_PERSPECTIVE_GUIDE_TOOL_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), GIMP_TYPE_PERSPECTIVE_GUIDE_TOOL, GimpPerspectiveGuideToolClass))
#define GIMP_IS_PERSPECTIVE_GUIDE_TOOL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GIMP_TYPE_PERSPECTIVE_GUIDE_TOOL))
#define GIMP_PERSPECTIVE_GUIDE_TOOL_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GIMP_TYPE_PERSPECTIVE_GUIDE_TOOL, GimpPerspectiveGuideToolClass))
#define GIMP_IS_PERSPECTIVE_GUIDE_TOOL(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIMP_TYPE_PERSPECTIVE_GUIDE_TOOL))
typedef struct _GimpPerspectiveGuideTool { GimpDrawTool parent; gboolean binding_failed; } GimpPerspectiveGuideTool;
typedef struct _GimpPerspectiveGuideToolClass { GimpDrawToolClass parent; } GimpPerspectiveGuideToolClass;
GType gimp_perspective_guide_tool_get_type (void) G_GNUC_CONST;
void gimp_perspective_guide_tool_register (GimpToolRegisterCallback, gpointer);
G_END_DECLS
#endif
