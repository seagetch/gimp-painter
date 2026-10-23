/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_MYPAINT_BRUSH_OPTIONS_GUI_H
#define GIMP_MYPAINT_BRUSH_OPTIONS_GUI_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

/* GUI-thread operation. Returns a floating, direct GtkMenuButton, or NULL for
 * NULL/wrong/disposed/unsupported contexts and closed canonical options. The
 * button retains the supplied context and its single canonical options model.
 * Preset selects a resource; Custom restores native snapshot history (including
 * its dirty draft semantics), not historical brush-object identity. */
GtkWidget *gimp_mypaint_brush_button_with_popup (GObject *config);

G_END_DECLS
#endif
