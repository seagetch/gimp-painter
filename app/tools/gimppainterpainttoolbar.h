/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef __GIMP_PAINTER_PAINT_TOOLBAR_H__
#define __GIMP_PAINTER_PAINT_TOOLBAR_H__

#include <gtk/gtk.h>
#include "core/core-types.h"

G_BEGIN_DECLS

/* A fresh, floating horizontal presentation of the existing options model.
 * No dock GUI is created, reparented, or replaced. Unsupported tools return NULL.
 */
GtkWidget * gimp_painter_paint_toolbar_new (GimpToolOptions *tool_options);

G_END_DECLS

#endif /* __GIMP_PAINTER_PAINT_TOOLBAR_H__ */
