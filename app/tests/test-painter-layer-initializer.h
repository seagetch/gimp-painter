/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef TEST_PAINTER_LAYER_INITIALIZER_H
#define TEST_PAINTER_LAYER_INITIALIZER_H
#include <glib-object.h>
G_BEGIN_DECLS
void painter_layer_init_fault_arm (gint kind, gboolean after_activation);
gboolean painter_layer_init_fault_consumed (void);
guint painter_layer_init_observed (void);
guint painter_layer_init_live (void);
gboolean painter_layer_init_store_closed (GObject *owner);
G_END_DECLS
#endif
