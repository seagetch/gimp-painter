/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef TEST_PAINTER_LAYER_DISPATCH_H
#define TEST_PAINTER_LAYER_DISPATCH_H
#include <glib-object.h>
G_BEGIN_DECLS
void painter_layer_dispatch_arm (GObject *owner, int kind);
int painter_layer_dispatch_consumed (void);
int painter_layer_dispatch_unconsumed (void);
void painter_layer_dispatch_disarm (void);
G_END_DECLS
#endif
