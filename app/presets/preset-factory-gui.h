/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PRESET_FACTORY_GUI_H
#define GIMP_PRESET_FACTORY_GUI_H
#include <glib-object.h>
typedef struct _GimpActionGroup GimpActionGroup;
G_BEGIN_DECLS
/* Caller owns the returned array; descriptor strings are borrowed static data. */
typedef struct
{
  const gchar *tree_label;
  const gchar *label;
  const gchar *icon;
  const gchar *help_data;
  const gchar *fs_label;
  const gchar *path_property_name;
  const gchar *writable_property_name;
} PrefsDialogEntry;
GArray *preset_factory_gui_prefs_entry_point (void);
/* Original registry operation was deliberately empty. This does not install
 * native layer-presets Apply/Refresh actions; those have their own setup route. */
void preset_factory_gui_action_group_entry_point (GimpActionGroup *group);
void preset_factory_gui_dialogs_actions_entry_point (GimpActionGroup *group);
G_END_DECLS
#endif
