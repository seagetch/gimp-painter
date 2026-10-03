/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_FILTER_EXIT_H
#define GIMP_FILTER_EXIT_H
#include <gio/gio.h>
G_BEGIN_DECLS
void gimp_filter_exit_init (GApplication *app);
void gimp_filter_exit_quit (GApplication *app);
gboolean gimp_filter_exit_is_requested (GApplication *app);
G_END_DECLS
#endif
