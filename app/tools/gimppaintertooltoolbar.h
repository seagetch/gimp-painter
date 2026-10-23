/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 2026 GIMP-Painter contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef __GIMP_PAINTER_TOOL_TOOLBAR_H__
#define __GIMP_PAINTER_TOOL_TOOLBAR_H__

#include <gtk/gtk.h>
#include "core/core-types.h"

G_BEGIN_DECLS

/* A new, floating horizontal view of the existing model, or NULL when this
 * factory does not implement the tool. Never takes ownership of the dock GUI.
 */
GtkWidget * gimp_painter_tool_toolbar_new (GimpToolOptions *tool_options);

G_END_DECLS

#endif /* __GIMP_PAINTER_TOOL_TOOLBAR_H__ */
