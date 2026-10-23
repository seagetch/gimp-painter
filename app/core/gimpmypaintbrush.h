/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_MYPAINT_BRUSH_H
#define GIMP_MYPAINT_BRUSH_H

#include "gimppaintermybrush.h"

G_BEGIN_DECLS

/* Source compatibility for named construction and duplication. This alias
 * does not restore the legacy object/class layout or its runtime GType name. */
typedef GimpPainterMybrush GimpMypaintBrush;
typedef GimpPainterMybrushClass GimpMypaintBrushClass;
#define GIMP_TYPE_MYPAINT_BRUSH (gimp_mypaint_brush_get_type ())
#define GIMP_MYPAINT_BRUSH(obj) GIMP_PAINTER_MYBRUSH (obj)
#define GIMP_IS_MYPAINT_BRUSH(obj) GIMP_IS_PAINTER_MYBRUSH (obj)
#define GIMP_MYPAINT_BRUSH_CLASS(klass) GIMP_PAINTER_MYBRUSH_CLASS (klass)
#define GIMP_MYPAINT_BRUSH_GET_CLASS(obj) GIMP_PAINTER_MYBRUSH_GET_CLASS (obj)

/* Same native type and BrushSlot as GimpPainterMybrush, not a second type. */
GType gimp_mypaint_brush_get_type (void) G_GNUC_CONST;

/* Transfer-full, nonfloating named native resource. Context is unused, as in
 * the original operation. NULL/empty names fail the original GLib precondition.
 * Construction failure returns NULL; no C++ exception crosses this entry. */
GimpData *gimp_mypaint_brush_new (GimpContext *context, const gchar *name);

/* Transfer-none native standard resource. Repeated calls share the native
 * provider identity; a caller needing ownership must retain, never adopt.
 * Native name/internal identifier replace the legacy identifiers. A retained
 * closed standard remains that same object until actual finalization. */
GimpData *gimp_mypaint_brush_get_standard (GimpContext *context);

/* Returns a distinct, transfer-full named resource, with independent settings
 * and live icon pixels and no inherited file association. Invalid, unnamed,
 * inactive or wrong-thread sources and copy failure return NULL. */
GimpData *gimp_mypaint_brush_duplicate (GimpMypaintBrush *brush);

/* Borrowed owner. Nested uses dispatch class hooks only on 0->1 / 1->0,
 * after updating the slot counter and outside its borrow. Wrong-thread or
 * closed objects cannot mutate the count. Unmatched end is a precondition
 * failure. No implicit end hook is synthesized on disposal. */
void gimp_mypaint_brush_begin_use (GimpMypaintBrush *brush);
void gimp_mypaint_brush_end_use (GimpMypaintBrush *brush);

G_END_DECLS

#endif
