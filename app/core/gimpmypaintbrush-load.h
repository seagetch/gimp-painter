/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_MYPAINT_BRUSH_LOAD_H
#define GIMP_MYPAINT_BRUSH_LOAD_H
#include "gimpmypaintbrush.h"
G_BEGIN_DECLS
#define GIMP_MYPAINT_BRUSH_FILE_EXTENSION ".myb"
#define GIMP_MYPAINT_BRUSH_ICON_FILE_EXTENSION "_prev.png"
/* Original absolute-filename C entry, transfer-full list and brush. The
 * filename is borrowed only during this call; the stream is private and closed
 * on return. Uses the native strict parser/name/icon policy, not the historical
 * permissive fallback parser. No file association is attached to the result. */
GList *gimp_mypaint_brush_load (GimpContext *context,
                              const gchar *filename,
                              GError **error);
G_END_DECLS
#endif
