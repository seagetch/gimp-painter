/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_MYPAINT_BRUSH_EDITOR_COMPAT_H
#define GIMP_MYPAINT_BRUSH_EDITOR_COMPAT_H
#include <gtk/gtk.h>
#include "core/core-types.h"
#include "widgets-types.h"
G_BEGIN_DECLS
/* Source-compatible opaque owner, not the old GimpDataEditor instance/class
 * layout or subclass ABI. This names the sole native GimpEditor-derived type. */
typedef struct _GimpPainterMybrushEditor GimpMypaintBrushEditor;
GType gimp_mypaint_brush_editor_get_type (void) G_GNUC_CONST;
GtkWidget *gimp_mypaint_brush_editor_new (GimpContext *context, GimpMenuFactory *menu_factory);
G_END_DECLS
#endif
