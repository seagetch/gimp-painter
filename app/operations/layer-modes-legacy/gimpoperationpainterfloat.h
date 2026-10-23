/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_OPERATION_PAINTER_FLOAT_H
#define GIMP_OPERATION_PAINTER_FLOAT_H
#include "operations/layer-modes/gimpoperationlayermode.h"
G_BEGIN_DECLS
#define GIMP_TYPE_OPERATION_PAINTER_FLOAT (gimp_operation_painter_float_get_type ())
typedef struct _GimpOperationPainterFloat GimpOperationPainterFloat;
typedef struct _GimpOperationPainterFloatClass GimpOperationPainterFloatClass;
struct _GimpOperationPainterFloat {
  GimpOperationLayerMode parent_instance;
  GeglRectangle source_extent;
  gboolean direct_paint;
};
struct _GimpOperationPainterFloatClass { GimpOperationLayerModeClass parent_class; };
GType gimp_operation_painter_float_get_type (void) G_GNUC_CONST;
gboolean gimp_painter_layer_mode_is_float (GimpLayerMode mode);
void gimp_painter_float_prepare_direct (GeglOperation *operation);
G_END_DECLS
#endif
