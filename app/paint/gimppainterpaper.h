/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_PAPER_H
#define GIMP_PAINTER_PAPER_H
#include <glib.h>
#include "core/core-types.h"
G_BEGIN_DECLS
/* Borrowed native byte view. No conversion, alpha multiplication or colorspace
 * transform: old ordinary and MyPaint paper both read the first byte. The
 * caller keeps pattern alive and does not mutate it while the view is used. */
typedef struct {
  const guchar *data;
  gint width, height, channels;
} GimpPainterPaperView;
gboolean gimp_painter_paper_get_view (GimpPattern *pattern,
                                      GimpPainterPaperView *view,
                                      GError **error);
/* Caller owns the result. Byte masks use old INT_MULT; float masks keep their
 * precision. x/y are drawable-local dab centers, not image/view coordinates. */
GimpTempBuf *gimp_painter_paper_texturize (GimpPattern *pattern,
                                         const GimpTempBuf *mask,
                                         gdouble x, gdouble y,
                                         GError **error);
G_END_DECLS
#endif
