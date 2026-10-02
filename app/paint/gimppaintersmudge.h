/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_SMUDGE_H
#define GIMP_PAINTER_SMUDGE_H
#include "paint-types.h"
#include "gimpbrushcore.h"
#include "gimppaintoptions.h"
G_BEGIN_DECLS
#define GIMP_TYPE_PAINTER_SMUDGE (gimp_painter_smudge_get_type ())
#define GIMP_PAINTER_SMUDGE(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIMP_TYPE_PAINTER_SMUDGE, GimpPainterSmudge))
#define GIMP_TYPE_PAINTER_SMUDGE_OPTIONS (gimp_painter_smudge_options_get_type ())
typedef struct { GimpBrushCore parent; gboolean binding_failed; } GimpPainterSmudge;
typedef struct { GimpBrushCoreClass parent; } GimpPainterSmudgeClass;
typedef struct { GimpPaintOptions parent; gboolean binding_failed; } GimpPainterSmudgeOptions;
typedef struct { GimpPaintOptionsClass parent; } GimpPainterSmudgeOptionsClass;
GType gimp_painter_smudge_get_type (void) G_GNUC_CONST;
GType gimp_painter_smudge_options_get_type (void) G_GNUC_CONST;
void gimp_painter_smudge_register (Gimp *gimp, GimpPaintRegisterCallback callback);
gchar* gimp_painter_smudge_dup_error (GimpPainterSmudge *smudge);
/* Owner-context transaction API. Renderer errors cancel rather than commit a
 * partial stroke; external raster changes never receive stale undo pixels. */
gboolean gimp_painter_smudge_begin (GimpPainterSmudge*, GimpDrawable*, GimpPaintOptions*, const GimpCoords*, GError**);
gboolean gimp_painter_smudge_motion (GimpPainterSmudge*, const GimpCoords*, guint32, GError**);
gboolean gimp_painter_smudge_finish (GimpPainterSmudge*, gboolean commit, GError**);
G_END_DECLS
#endif
