/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef __GIMP_CANVAS_PERSPECTIVE_GUIDE_H__
#define __GIMP_CANVAS_PERSPECTIVE_GUIDE_H__
#include "gimpcanvasitem.h"
G_BEGIN_DECLS
#define GIMP_TYPE_CANVAS_PERSPECTIVE_GUIDE (gimp_canvas_perspective_guide_get_type ())
#define GIMP_CANVAS_PERSPECTIVE_GUIDE(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIMP_TYPE_CANVAS_PERSPECTIVE_GUIDE, GimpCanvasPerspectiveGuide))
#define GIMP_IS_CANVAS_PERSPECTIVE_GUIDE(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIMP_TYPE_CANVAS_PERSPECTIVE_GUIDE))
#define GIMP_CANVAS_PERSPECTIVE_GUIDE_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), GIMP_TYPE_CANVAS_PERSPECTIVE_GUIDE, GimpCanvasPerspectiveGuideClass))
#define GIMP_IS_CANVAS_PERSPECTIVE_GUIDE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GIMP_TYPE_CANVAS_PERSPECTIVE_GUIDE))
#define GIMP_CANVAS_PERSPECTIVE_GUIDE_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GIMP_TYPE_CANVAS_PERSPECTIVE_GUIDE, GimpCanvasPerspectiveGuideClass))
typedef struct _GimpCanvasPerspectiveGuide { GimpCanvasItem parent; gboolean binding_failed; } GimpCanvasPerspectiveGuide;
typedef struct _GimpCanvasPerspectiveGuideClass { GimpCanvasItemClass parent; } GimpCanvasPerspectiveGuideClass;
GType gimp_canvas_perspective_guide_get_type (void) G_GNUC_CONST;
/* The legacy source API owns the supplied model, independently of image state.
 * The read/write construct property "guide" has the same explicit identity.
 * Returns one nonfloating reference, or NULL if its native binding cannot be
 * initialized. The shell is borrowed, as for every GimpCanvasItem. */
GimpCanvasPerspectiveGuide *gimp_canvas_perspective_guide_new (GimpDisplayShell *shell,
                                                             GimpPerspectiveGuide *guide);
/* Native extension: follow shell->display's current image/session ruler until
 * an explicit "guide" property assignment selects a fixed model. */
GimpCanvasItem *gimp_canvas_perspective_guide_new_for_image (GimpDisplayShell *shell);
G_END_DECLS
#endif
