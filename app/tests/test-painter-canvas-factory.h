/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef TEST_PAINTER_CANVAS_FACTORY_H
#define TEST_PAINTER_CANVAS_FACTORY_H
#include <glib-object.h>
G_BEGIN_DECLS
void painter_canvas_fault_arm (gint kind, gint stage);
gboolean painter_canvas_fault_consumed (void);
guint painter_canvas_fault_live (void);
guint painter_canvas_fault_connections (void);
gboolean painter_canvas_fault_disconnected (void);
gboolean painter_canvas_store_closed (GObject *owner);
void painter_canvas_handle_test (gpointer shell);
void painter_canvas_explicit_handle_test (gpointer shell);
G_END_DECLS
#endif
