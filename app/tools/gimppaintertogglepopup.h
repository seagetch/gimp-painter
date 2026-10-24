/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_TOGGLE_POPUP_H
#define GIMP_PAINTER_TOGGLE_POPUP_H
#include <gtk/gtk.h>
G_BEGIN_DECLS
/* Retains body independently of each ephemeral native host. Anchor is weak. */
GtkWidget *gimp_painter_toggle_popup_button_new (GtkWidget *anchor,
                                                 GtkWidget *body);
G_END_DECLS
#endif
