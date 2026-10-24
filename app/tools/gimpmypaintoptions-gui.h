/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_MYPAINT_OPTIONS_GUI_H
#define GIMP_MYPAINT_OPTIONS_GUI_H
#include <gtk/gtk.h>
#include "core/core-types.h"
G_BEGIN_DECLS
/* Original C signatures, modern native editor presentations. Transfer-floating
 * widget; NULL for invalid/closed options or failed construction. The exact
 * supplied native Options model is retained, including independent options
 * without tool-info; no canonical model substitution or extra store is made.
 * Horizontal means the native editor's horizontal presentation, not the old
 * compact toolbar geometry. Native collision/conflict/writability protections
 * and explicit constant curves are preserved, not old destructive-save parity.
 * Gimp remains borrowed: keep the application alive throughout widget use. */
GtkWidget *gimp_mypaint_options_gui (GimpToolOptions *tool_options);
GtkWidget *gimp_mypaint_options_gui_horizontal (GimpToolOptions *tool_options);
G_END_DECLS
#endif
