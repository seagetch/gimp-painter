/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_MYPAINT_BRUSH_SAVE_H
#define GIMP_MYPAINT_BRUSH_SAVE_H
#include "gimpmypaintbrush.h"
G_BEGIN_DECLS
/* Borrowed native resource; save to its existing GFile through the native data
 * transaction and BrushSlot serializer. No file returns FALSE without error.
 * Success follows native clean/mtime policy and embeds the preview; it does not
 * recreate the old unchecked sidecar writer or historical binary ABI. */
gboolean gimp_mypaint_brush_save (GimpData *brush, GError **error);
G_END_DECLS
#endif
