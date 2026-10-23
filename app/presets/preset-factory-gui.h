/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PRESET_FACTORY_GUI_H
#define GIMP_PRESET_FACTORY_GUI_H
#include <glib-object.h>
typedef struct _GimpActionGroup GimpActionGroup;
G_BEGIN_DECLS
/* Original registry operation was deliberately empty. This does not install
 * native layer-presets Apply/Refresh actions; those have their own setup route. */
void preset_factory_gui_action_group_entry_point (GimpActionGroup *group);
void preset_factory_gui_dialogs_actions_entry_point (GimpActionGroup *group);
G_END_DECLS
#endif
