/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef __GIMP_CANVAS_PERSPECTIVE_GUIDE_H__
#define __GIMP_CANVAS_PERSPECTIVE_GUIDE_H__
#include "gimpcanvasitem.h"
G_BEGIN_DECLS
#define GIMP_TYPE_CANVAS_PERSPECTIVE_GUIDE (gimp_canvas_perspective_guide_get_type ())
typedef struct _GimpCanvasPerspectiveGuide { GimpCanvasItem parent; gboolean binding_failed; } GimpCanvasPerspectiveGuide;
typedef struct _GimpCanvasPerspectiveGuideClass { GimpCanvasItemClass parent; } GimpCanvasPerspectiveGuideClass;
GType gimp_canvas_perspective_guide_get_type (void) G_GNUC_CONST;
/* The overlay follows shell->display's current image and its session ruler.
 * Returns one nonfloating reference, or NULL if its native binding cannot be
 * initialized. The shell is borrowed, as for every GimpCanvasItem. */
GimpCanvasItem *gimp_canvas_perspective_guide_new (GimpDisplayShell *shell);
G_END_DECLS
#endif
