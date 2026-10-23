/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_CANVAS_UI_H
#define GIMP_PAINTER_CANVAS_UI_H
#include "display-types.h"
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
/* Window-column presentation is independent of canonical Tool Options. */
gboolean gimp_painter_canvas_ui_attach_right_docks (GimpDisplayShell *shell, GtkWidget *columns, guint64 revision);
void gimp_painter_canvas_ui_detach_right_docks (GimpDisplayShell *shell);
gboolean gimp_painter_canvas_ui_has_right_docks (GimpDisplayShell *shell);
gboolean gimp_painter_canvas_ui_get_right_docks_width (GimpDisplayShell *shell, gint *width);
void gimp_painter_canvas_ui_set_right_docks_width (GimpDisplayShell *shell, gint encoded_width);
/* Preserve explicit full/compact/hidden options across native tab transfer. */
gint gimp_painter_canvas_ui_get_options_presentation (GimpDisplayShell *shell);
void gimp_painter_canvas_ui_set_options_presentation (GimpDisplayShell *shell, gint mode);
/* This is the single C/C++ declaration boundary for native window
 * integration. GimpImageWindow remains opaque here; its native C widget
 * hierarchy contains C-only instance fields and must not be included by
 * the Canvas implementation. */
/* Native Painter integration.  The revision predicate grants no ownership;
 * Canvas must revalidate it after each callback-producing hierarchy change. */
/* Native session ownership is released only after topology reconciliation.
 * Hold every participant through the complete sync pass; sync may emit GTK
 * callbacks, whose new ownership requests remain deferred by that hold. */
void               gimp_image_window_session_suspend     (GimpImageWindow   *window);
gboolean           gimp_image_window_session_resume      (GimpImageWindow   *window);
gboolean           gimp_image_window_session_sync        (GimpImageWindow   *window);
void               gimp_image_window_painter_reconcile    (GimpImageWindow   *window);
void               gimp_image_window_painter_suspend      (GimpImageWindow   *window);
void               gimp_image_window_painter_resume       (GimpImageWindow   *window);
gboolean           gimp_image_window_painter_can_attach   (GimpImageWindow   *window,
                                                           GimpDisplayShell  *shell,
                                                           guint64            revision);
gint               gimp_image_window_get_right_docks_width (GimpImageWindow *window);
void               gimp_image_window_get_right_docks_geometry (GimpImageWindow *window,
                                                              gint            *width,
                                                              gint            *position,
                                                              gint            *span,
                                                              gboolean        *position_set);
/* The toolbar's owned shell remains an empty session placeholder.  Actions
 * route separately to a live image in the user context. */
gboolean           gimp_image_window_is_painter_toolbar  (GimpImageWindow  *window);
void               gimp_image_window_set_painter_toolbar (GimpImageWindow  *window,
                                                           gboolean          toolbar);
GimpDisplay      * gimp_image_window_get_action_display   (GimpImageWindow  *window);

/* Suppress synchronous actions while a selected live image has no valid
 * native owner/membership during a Painter toolbar transfer. */
gboolean gimp_image_window_painter_actions_pending (Gimp *gimp);

G_END_DECLS
#endif
