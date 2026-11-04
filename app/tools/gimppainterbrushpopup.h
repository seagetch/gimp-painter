/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_BRUSH_POPUP_H
#define GIMP_PAINTER_BRUSH_POPUP_H
#include <gtk/gtk.h>
#include "paint/paint-types.h"
G_BEGIN_DECLS
/* Independent live editor for the horizontal Brush thumbnail. No dock editor
 * is borrowed, and resource switch/dismissal never initiates a data save. */
GtkWidget *gimp_painter_brush_popup_button_new (GimpPaintOptions *options);
G_END_DECLS
#endif
