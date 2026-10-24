/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PRESET_FACTORY_GUI_H
#define GIMP_PRESET_FACTORY_GUI_H
#include <glib-object.h>
typedef struct _GimpActionGroup GimpActionGroup;
G_BEGIN_DECLS
/* Legacy shallow array: caller owns/may edit the array and pointer fields,
 * but not the pointed-to strings. There is no element clear callback.
 * Dynamic strings remain alive until the last application owner closes the
 * catalog epoch; built-in static strings are permanent. Use the owning snapshot
 * below for an
 * independently retained read-only view across owner/catalog teardown. */
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
typedef struct _Gimp Gimp;
typedef struct _GimpPresetPreferencesSnapshot GimpPresetPreferencesSnapshot;
/* NULL selects the process catalog; otherwise the named native owner snapshot.
 * Transfer full. The read-only view/strings live until the last snapshot unref. */
GimpPresetPreferencesSnapshot *gimp_preset_preferences_snapshot_new (Gimp *gimp, GError **error);
GimpPresetPreferencesSnapshot *gimp_preset_preferences_snapshot_ref (GimpPresetPreferencesSnapshot *snapshot);
void gimp_preset_preferences_snapshot_unref (GimpPresetPreferencesSnapshot *snapshot);
const PrefsDialogEntry *gimp_preset_preferences_snapshot_get_entries (GimpPresetPreferencesSnapshot *snapshot, guint *length);
/* Original registry operation was deliberately empty. This does not install
 * native layer-presets Apply/Refresh actions; those have their own setup route. */
void preset_factory_gui_action_group_entry_point (GimpActionGroup *group);
void preset_factory_gui_dialogs_actions_entry_point (GimpActionGroup *group);
G_END_DECLS
#endif
