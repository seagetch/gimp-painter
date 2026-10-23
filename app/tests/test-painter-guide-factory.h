/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef PAINTER_GUIDE_FACTORY_TEST_H
#define PAINTER_GUIDE_FACTORY_TEST_H
#include <glib-object.h>
G_BEGIN_DECLS
void painter_guide_fault_arm (gint kind, gint stage);
gboolean painter_guide_fault_consumed (void);
guint painter_guide_fault_live (void);
gboolean painter_guide_fault_closed (GObject *owner);
void painter_guide_factory_handle_faults (void);
G_END_DECLS
#endif
