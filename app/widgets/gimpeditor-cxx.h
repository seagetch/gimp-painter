/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef APP_WIDGETS_GIMPEDITOR_CXX_H_
#define APP_WIDGETS_GIMPEDITOR_CXX_H_

G_BEGIN_DECLS

/* Legacy callable providers. list_id was unused in the original implementation
 * and remains ignored. Callback data is borrowed, with the original gpointer
 * contract; it need not be a GObject. The returned widget is the primary button. */
GtkWidget *gimp_editor_add_dropdown (GimpEditor  *editor,
                                     const gchar *icon_name,
                                     const gchar *tooltip,
                                     const gchar *help_id,
                                     const gchar *list_id,
                                     GCallback    callback,
                                     GCallback    extended_callback,
                                     gpointer     callback_data);
GtkWidget *gimp_editor_add_action_dropdown (GimpEditor  *editor,
                                            const gchar *list_id,
                                            const gchar *group_name,
                                            const gchar *action_name,
                                            ...) G_GNUC_NULL_TERMINATED;

G_END_DECLS
#endif
