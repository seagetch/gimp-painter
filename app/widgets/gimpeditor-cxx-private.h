/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef APP_WIDGETS_GIMPEDITOR_CXX_PRIVATE_H_
#define APP_WIDGETS_GIMPEDITOR_CXX_PRIVATE_H_
#include <stdarg.h>
G_BEGIN_DECLS
/* Native app-widget construction helpers, with no GimpEditor private layout.
 * create returns a full reference to an unparented, fully configured proxy;
 * NULL means construction was cancelled. force_button retains the legacy
 * dropdown primary shape even for toggle actions. It consumes args, not va_end().
 * pack borrows both arguments and returns FALSE if a destroy callback cancels
 * publication. The caller keeps its own full reference across the operation. */
GtkWidget *gimp_editor_create_action_button_valist (GimpEditor *, const gchar *, const gchar *, gboolean, va_list);
gboolean   gimp_editor_pack_button (GimpEditor *, GtkWidget *);
G_END_DECLS
#endif
