/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * GimpPreset
 * Copyright (C) 2017 seagetch <sigetch@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __PRESET_FACTORY_GUI_H__
#define __PRESET_FACTORY_GUI_H__

#include "preset-factory.h"

#ifdef __cplusplus
extern "C" {
#endif
#include <gtk/gtk.h>
#include "widgets/widgets-types.h"
#include "widgets/gimpactionfactory.h"
#include "widgets/gimpdialogfactory.h"

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

GArray*    preset_factory_gui_prefs_entry_point (void);
void       preset_factory_gui_action_group_entry_point (GimpActionGroup* group);
void       preset_factory_gui_dialogs_actions_entry_point (GimpActionGroup* group);


#ifdef __cplusplus
};// extern "C"

/////////////////////////////////////////////////////////////////////
class PresetGuiConfig : virtual public PresetConfig
{
protected:

  template<typename T>
  static GtkWidget *
  dialog_new (GimpDialogFactory      *factory,
              GimpContext            *context,
              GimpUIManager          *ui_manager,
              gint                    view_size)
  {
    g_print("DIALOG_NEW:: \n");
    auto config = T::config();
    GObject* result = config->create_view(G_OBJECT(context),
                                          view_size,
                                          G_OBJECT(gimp_dialog_factory_get_menu_factory (factory)));
    return GTK_WIDGET(result);
  }

  struct ActionGroupEntry
  {
    const gchar* identifier;
    const gchar* label;
    const gchar* stock_id;
  };

public:
  PresetGuiConfig() : PresetConfig() { };
  virtual ~PresetGuiConfig() { };

  ////////////////////////////////////////////
  // GUI Interface

  virtual GObject*                create_view             (GObject* context,
                                                           gint view_size,
                                                           GObject* menu_factory) = 0;
  virtual GObject*                create_editor            () = 0;
  virtual PrefsDialogEntry*       prefs_dialog_entry       () = 0;
  virtual GimpDialogFactoryEntry* view_dialog_new_entry    () = 0;
  virtual GimpStringActionEntry*  view_dialog_action_entry () = 0;
  virtual ActionGroupEntry*       get_action_group_entry   () = 0;
};

/////////////////////////////////////////////////////////////////////
class PresetGuiFactory : virtual public PresetFactory
{
public:
  PresetGuiFactory() : PresetFactory() { };

  virtual void     initialize_factory          ();

  virtual void     entry_point                 (Gimp* gimp);

  virtual GArray*  prefs_entry_point           ();

  virtual void     action_group_entry_point    (GimpActionGroup *group);
  virtual void     action_group_update         (GimpActionGroup *group,
                                                gpointer         user_data);

  virtual void     dialogs_actions_entry_point (GimpActionGroup* group);
  virtual void     initialize                  (Gimp               *gimp,
                                                GimpInitStatusFunc  status_callback);
  virtual void     restore                     (Gimp               *gimp,
                                                GimpInitStatusFunc  status_callback);
  virtual gboolean exit                        (Gimp               *gimp,
                                                gboolean            force);

  static GIMP::Feature* get_factory();
};

#endif // __cplusplus

#endif /* __PRESET_FACTORY_GUI_H__ */
