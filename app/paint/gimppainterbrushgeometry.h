/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_BRUSH_GEOMETRY_H
#define GIMP_PAINTER_BRUSH_GEOMETRY_H

#include "paint-types.h"

G_BEGIN_DECLS

/* Explicit serialized provenance. Neither paper nor blend mode implies it. */
gboolean gimp_painter_brush_geometry_options (GimpPaintOptions *options);
gboolean gimp_painter_brush_geometry_enabled (GimpBrushCore *core);
void     gimp_painter_brush_geometry_enable  (GimpBrushCore *core, gboolean enabled);

/* Stateless legacy geometry; caller owns the returned buffer. */
gboolean gimp_painter_brush_geometry_size   (GimpBrush *brush, gint *width,
                                              gint *height, GError **error);
GimpTempBuf *gimp_painter_brush_geometry_transform (GimpBrush *brush,
                                                  gdouble scale,
                                                  gdouble aspect,
                                                  gdouble angle,
                                                  gboolean reflect,
                                                  gdouble hardness,
                                                  gboolean pixmap,
                                                  GError **error);
gboolean gimp_painter_brush_geometry_color (const GimpTempBuf *pixmap,
                                            const GimpTempBuf *mask,
                                            GeglBuffer *area,
                                            const GimpCoords *coords,
                                            gint area_x, gint area_y,
                                            GError **error);
G_END_DECLS
#endif
