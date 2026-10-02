/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_CANVAS_UI_H
#define GIMP_PAINTER_CANVAS_UI_H
G_BEGIN_DECLS
void gimp_painter_canvas_ui_init (GimpDisplayShell *shell);
void gimp_painter_canvas_ui_close (GimpDisplayShell *shell);
void gimp_painter_canvas_ui_set_visible (GimpDisplayShell *shell, gboolean visible);
gboolean gimp_painter_canvas_ui_get_visible (GimpDisplayShell *shell);
/* Coordinates are GTK logical canvas pixels, never image/buffer pixels. */
void gimp_painter_canvas_ui_pointer (GimpDisplayShell *shell, gdouble x, gdouble y, gboolean released);
/* A dock is temporarily reparented with one balanced owner reference. */
gboolean gimp_painter_canvas_ui_attach_dock (GimpDisplayShell *shell, GtkWidget *dock);
void gimp_painter_canvas_ui_detach_dock (GimpDisplayShell *shell);
G_END_DECLS
#endif
