/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_TOGGLE_POPUP_H
#define GIMP_PAINTER_TOGGLE_POPUP_H
#include <gtk/gtk.h>
G_BEGIN_DECLS
/* Retains body independently of each ephemeral native host. Anchor is weak. */
GtkWidget *gimp_painter_toggle_popup_button_new (GtkWidget *anchor,
                                                 GtkWidget *body);
/* Self-anchored Details entry; shares retained-body/lifetime policy above. */
GtkWidget *gimp_painter_transform_details_button_new (GtkWidget *body);
GtkWidget *gimp_painter_bucket_details_button_new (GtkWidget *body);
/* Ink frame entries use the same self-anchor policy, not toggle-row policy. */
GtkWidget *gimp_painter_ink_popup_button_new (GtkWidget *body, gboolean blob);
/* Clone and Perspective Clone Options use the frame self-anchor policy. */
GtkWidget *gimp_painter_clone_options_button_new (GtkWidget *body);
G_END_DECLS
#endif
