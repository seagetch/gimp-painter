/* GIMP - The GNU Image Manipulation Program
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef GIMP_OPERATION_PAINTER_LEGACY_H
#define GIMP_OPERATION_PAINTER_LEGACY_H
#include "operations/layer-modes/gimpoperationlayermode.h"
G_BEGIN_DECLS
#define GIMP_TYPE_OPERATION_PAINTER_LEGACY (gimp_operation_painter_legacy_get_type ())
typedef struct _GimpOperationPainterLegacy GimpOperationPainterLegacy;
typedef struct _GimpOperationPainterLegacyClass GimpOperationPainterLegacyClass;
struct _GimpOperationPainterLegacy
{
  GimpOperationLayerMode parent_instance;
  GeglRectangle source_extent;
};
struct _GimpOperationPainterLegacyClass { GimpOperationLayerModeClass parent_class; };
GType gimp_operation_painter_legacy_get_type (void) G_GNUC_CONST;
/* Wire numbers are decoded explicitly, never cast into modern colliding enums. */
gboolean gimp_painter_layer_mode_is_compatibility (GimpLayerMode mode);
gboolean gimp_painter_layer_mode_from_legacy (guint32 raw, GimpLayerMode *mode);
gboolean gimp_painter_layer_mode_to_legacy (GimpLayerMode mode, guint32 *raw);
/* Byte-faithful RGBA8 arithmetic. Mask256 means no mask, otherwise0..255.
 * Supports aliasing the output with either input. */
void gimp_painter_legacy_composite_u8 (const guint8 backdrop[4],
                                     const guint8 source[4], guint8 out[4],
                                     guint opacity, guint mask, guint raw_mode);
G_END_DECLS
#endif
