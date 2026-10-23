/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_MYPAINT_BRUSH_H
#define GIMP_MYPAINT_BRUSH_H

#include "gimppaintermybrush.h"

G_BEGIN_DECLS

/* Source compatibility for named construction and duplication. This alias
 * does not restore the legacy object/class layout or its runtime GType name. */
typedef GimpPainterMybrush GimpMypaintBrush;
#define GIMP_TYPE_MYPAINT_BRUSH (gimp_mypaint_brush_get_type ())
#define GIMP_MYPAINT_BRUSH(obj) GIMP_PAINTER_MYBRUSH (obj)
#define GIMP_IS_MYPAINT_BRUSH(obj) GIMP_IS_PAINTER_MYBRUSH (obj)

/* Same native type and BrushSlot as GimpPainterMybrush, not a second type. */
GType gimp_mypaint_brush_get_type (void) G_GNUC_CONST;

/* Transfer-full, nonfloating named native resource. Context is unused, as in
 * the original operation. NULL/empty names fail the original GLib precondition.
 * Construction failure returns NULL; no C++ exception crosses this entry. */
GimpData *gimp_mypaint_brush_new (GimpContext *context, const gchar *name);

/* Returns a distinct, transfer-full named resource, with independent settings
 * and live icon pixels and no inherited file association. Invalid, unnamed,
 * inactive or wrong-thread sources and copy failure return NULL. */
GimpData *gimp_mypaint_brush_duplicate (GimpMypaintBrush *brush);

G_END_DECLS

#endif
