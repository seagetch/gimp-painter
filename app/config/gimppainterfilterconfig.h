/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_FILTER_CONFIG_H
#define GIMP_PAINTER_FILTER_CONFIG_H
#include <glib-object.h>
G_BEGIN_DECLS
/* Construction/property paths; no application instance layout change. */
void gimp_painter_filter_config_init (GObject *owner);
void gimp_painter_filter_config_activate (GObject *owner);
void gimp_painter_filter_config_set_spill (GObject *owner, guint64 bytes);
guint64 gimp_painter_filter_config_get_spill (GObject *owner);
G_END_DECLS
#endif
