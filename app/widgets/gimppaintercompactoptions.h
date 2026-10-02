/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_COMPACT_OPTIONS_H
#define GIMP_PAINTER_COMPACT_OPTIONS_H
#include <gtk/gtk.h>
#include "core/core-types.h"
G_BEGIN_DECLS
/* Reversible presentation of the registered canonical options GUI. No second
 * options instance or second standard GUI is constructed. Controls retain
 * their native resource, sensitivity, reset and action connections. */
gboolean gimp_painter_compact_options_set (GtkWidget       *gui,
                                           GimpToolOptions *options,
                                           gboolean         enabled,
                                           GtkOrientation   orientation,
                                           GError         **error);
gboolean gimp_painter_compact_options_get (GtkWidget *gui);
void     gimp_painter_compact_options_popdown (GtkWidget *gui);
G_END_DECLS
#endif
