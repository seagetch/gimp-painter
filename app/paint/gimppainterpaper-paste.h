/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_PAPER_PASTE_H
#define GIMP_PAINTER_PAPER_PASTE_H
#include "paint-types.h"
G_BEGIN_DECLS
/* Native byte compatibility dispatch: FALSE means use the normal PaintCore
 * path; TRUE means handled (including a reported failure, never fallback after
 * mutation). Limited to original nonlinear-byte drawables and Painter modes.
 * All other precisions and modern modes retain native PaintCore arithmetic. */
gboolean gimp_painter_paper_paste (GimpPaintCore *core,
                                   const GimpTempBuf *mask,
                                   gint mask_x, gint mask_y,
                                   GimpDrawable *drawable,
                                   gdouble paint_opacity,
                                   gdouble image_opacity,
                                   GimpLayerMode paint_mode,
                                   GimpPaintApplicationMode mode,
                                   GError **error);
G_END_DECLS
#endif
