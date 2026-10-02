/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_PAINT_GATE_H
#define GIMP_PAINTER_PAINT_GATE_H
#include "paint-types.h"
#include "gimppaintcore.h"
G_BEGIN_DECLS
#define GIMP_TYPE_PAINTER_PAINT_GATE (gimp_painter_paint_gate_get_type ())
typedef struct {GimpPaintCore parent_instance;} GimpPainterPaintGate;
typedef struct {GimpPaintCoreClass parent_class;} GimpPainterPaintGateClass;
GType gimp_painter_paint_gate_get_type (void) G_GNUC_CONST;
void gimp_painter_paint_gate_register (Gimp *gimp,GimpPaintRegisterCallback callback);
G_END_DECLS
#endif
