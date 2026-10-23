/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_FILL_BRUSH_H
#define GIMP_FILL_BRUSH_H
#include "paint-types.h"
#include "gimpbrushcore.h"
#include "gimppaintcore-stroke.h"
#include "gimppaintoptions.h"
G_BEGIN_DECLS
#define GIMP_TYPE_FILL_BRUSH (gimp_fill_brush_get_type())
#define GIMP_FILL_BRUSH(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj),GIMP_TYPE_FILL_BRUSH,GimpFillBrush))
#define GIMP_FILL_BRUSH_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass),GIMP_TYPE_FILL_BRUSH,GimpFillBrushClass))
#define GIMP_IS_FILL_BRUSH(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj),GIMP_TYPE_FILL_BRUSH))
#define GIMP_IS_FILL_BRUSH_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),GIMP_TYPE_FILL_BRUSH))
#define GIMP_FILL_BRUSH_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj),GIMP_TYPE_FILL_BRUSH,GimpFillBrushClass))
#define GIMP_TYPE_FILL_BRUSH_OPTIONS (gimp_fill_brush_options_get_type())
typedef struct { GimpBrushCore parent; gboolean binding_failed; } GimpFillBrush;
typedef struct { GimpBrushCoreClass parent; } GimpFillBrushClass;
typedef struct { GimpPaintOptions parent; gboolean binding_failed; } GimpFillBrushOptions;
typedef struct { GimpPaintOptionsClass parent; } GimpFillBrushOptionsClass;
GType gimp_fill_brush_get_type(void) G_GNUC_CONST;
GType gimp_fill_brush_options_get_type(void) G_GNUC_CONST;
void gimp_fill_brush_register (Gimp *gimp, GimpPaintRegisterCallback callback);
/* Bare native start remains refused; generic strokes use the owned facade. */
gboolean gimp_fill_brush_begin (GimpFillBrush *brush, GimpDrawable *drawable,
                                GimpPaintOptions *options, const GimpCoords *coords,
                                GError **error);
gboolean gimp_fill_brush_motion (GimpFillBrush *brush, const GimpCoords *coords,
                                 guint32 time, GError **error);
/* Owner-thread resumable admission: only copies the accepted event. The caller
 * must keep the stroke options/resources immutable until step returns TRUE, and
 * must drain the prior event before submitting another. step expands at most one
 * native dab, or searches at most budget candidates, per call. Native snapshot,
 * mask generation and publication are separate (not bounded by this contract).
 * The original motion API above remains synchronous for compatibility callers. */
gboolean gimp_fill_brush_motion_begin (GimpFillBrush *brush, const GimpCoords *coords,
                                      guint32 time, GError **error);
/* FALSE without error means queued work remains. Cancellation may be requested
 * reentrantly; it is applied after the current native callback unwinds. */
gboolean gimp_fill_brush_finish (GimpFillBrush *brush, gboolean commit, GError **error);
/* Native owner-thread drain. Finish the native transaction only after TRUE.
 * FALSE without error means queued work; failures/cancellation publish no dab. */
gboolean gimp_fill_brush_step(GimpFillBrush *brush, gsize budget, GError **error);
/* Cancel the entire transaction, rolling back any already-published dabs. */
void gimp_fill_brush_cancel_pending(GimpFillBrush *brush);
/* One owned native transaction across all prepared subpaths. */
gboolean gimp_fill_brush_stroke (GimpPaintCore*, GimpDrawable*, GimpPaintOptions*,
                                const GimpPaintStrokeSegment*, gsize, gboolean push_undo, GError**);
G_END_DECLS
#endif
