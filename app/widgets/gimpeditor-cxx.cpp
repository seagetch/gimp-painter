/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * gimpeditor-cxx.cpp
 * Copyright (C) 2017 seagetch
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

extern "C" {
#include "config.h"
#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <stdarg.h>

#include "libgimpwidgets/gimpwidgets.h"

#include "widgets-types.h"
#include "widgets-enums.h"
#include "gimpeditor.h"
#include "gimpuimanager.h"
#include "gimpwidgets-utils.h"
#include "gimpeditor-private.h"
#include "gimpcontainerview.h"
#include "core/gimpcontext.h"
#include "core/gimpcontainer.h"

#include "gimp-intl.h"

};

#include "base/glib-cxx-def-utils.hpp"
#include "gimpeditor-cxx.h"
#include "popupper.h"

#include "presets/layer-preset-gui.h"

using namespace GLib;
namespace GIMP_EDITOR {
class PopupWindow {
  GimpEditor* editor;
public:
  PopupWindow(GimpEditor* e) : editor(e) { };
  void create_view(GtkWidget* w, GtkWidget** _result, gpointer user_data) {
    Nullable<GtkWidget*> result(_result);
    if (!_result)
      return;

    auto view = ref( w );
    GimpViewSize   view_size = GIMP_VIEW_SIZE_SMALL;
    GimpContext*   context   = ref(editor) [gimp_container_view_get_context] ();

    g_return_if_fail (GIMP_IS_CONTEXT (context));

    auto layer_config = LayerPresetGuiConfig::config();

    result = with (gtk_vbox_new (FALSE, 1), [&](auto it){
      it [gtk_widget_show] ();
      GtkRequisition req = { -1, 300 };
      it [gtk_widget_set_size_request] (req.width, req.height);

      it.pack_start (TRUE, TRUE, 0) (layer_config->create_view(G_OBJECT(context), view_size, NULL /*menu_factory*/), [&](auto it){
        it [gtk_widget_show] ();
      });

    });
  }
};

}; // namespace


GtkWidget *
gimp_editor_add_dropdown (GimpEditor  *editor,
                          const gchar *stock_id,
                          const gchar *tooltip,
                          const gchar *help_id,
                          const gchar *list_id,
                          GCallback    callback,
                          GCallback    extended_callback,
                          gpointer     callback_data)
{
  GtkWidget      *button;

  g_return_val_if_fail (GIMP_IS_EDITOR (editor), NULL);
  g_return_val_if_fail (stock_id != NULL, NULL);

  button = with(gtk_button_new(), [&](auto it){
    GtkIconSize     button_icon_size;
    GtkReliefStyle  button_relief;

    it [gtk_widget_show] ();
    button_icon_size = gimp_editor_ensure_button_box (editor, &button_relief);
    it [gtk_button_set_relief] (button_relief);
    if (tooltip || help_id)
      it [gimp_help_set_help_data] (tooltip, help_id);


    auto old_child = ref (it [gtk_bin_get_child] ());
    if (old_child) {
      old_child [gtk_widget_destroy] ();
      old_child = NULL;
    }

    it [gtk_container_add] (with( gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0), [&](auto it){
      it [gtk_widget_show] ();

      it.pack_start(FALSE, FALSE, 0)
        (gtk_image_new_from_stock (stock_id, button_icon_size), [&](auto it){
          it [gtk_widget_show] ();

        } );

    } ));

    if (callback)
      g_signal_connect (it, "clicked",
                        callback,
                        callback_data);
    if (extended_callback)
      g_signal_connect (it, "extended-clicked",
                        extended_callback,
                        callback_data);

  });

  auto arrow = with(gtk_button_new(), [&](auto it){
    GtkIconSize     button_icon_size;
    GtkReliefStyle  button_relief;

    it [gtk_widget_show] ();
    button_icon_size = gimp_editor_ensure_button_box (editor, &button_relief);
    it [gtk_button_set_relief] (button_relief);

    if (tooltip || help_id)
      it [gimp_help_set_help_data] (tooltip, help_id);

    auto old_child = ref (it [gtk_bin_get_child] ());
    if (old_child)
      old_child [gtk_widget_destroy] ();

    it [gtk_container_add] (with( gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0), [&](auto it){
      it [gtk_widget_show] ();

      it.pack_start(FALSE, FALSE, 0) (gtk_arrow_new (GTK_ARROW_DOWN, GTK_SHADOW_ETCHED_IN), [&](auto it){
        it [gtk_widget_show] ();
      });

    } ));

    auto popup = new GIMP_EDITOR::PopupWindow(editor);
    decorator (it.ptr(), popup);
    decorate_popover (it, Delegators::delegator(popup, &GIMP_EDITOR::PopupWindow::create_view) );

  });

  with (editor->priv->button_box, [&](auto it) {
    it.pack_start(button, TRUE, TRUE, 0);
    it.pack_start(arrow.ptr(), TRUE, TRUE, 0);
  });

  return button;
}


GtkWidget *
gimp_editor_add_action_dropdown (GimpEditor  *editor,
                                 const gchar *list_id,
                                 const gchar *group_name,
                                 const gchar *action_name,
                                 ...)
{
  GimpActionGroup *group;
  GtkAction       *action;
  GList           *extended = NULL;
  va_list          args;

  g_return_val_if_fail (GIMP_IS_EDITOR (editor), NULL);
  g_return_val_if_fail (action_name != NULL, NULL);
  g_return_val_if_fail (editor->priv->ui_manager != NULL, NULL);

  group = gimp_ui_manager_get_action_group (editor->priv->ui_manager, group_name);
  g_return_val_if_fail (group != NULL, NULL);

  action = gtk_action_group_get_action (GTK_ACTION_GROUP (group), action_name);
  g_return_val_if_fail (action != NULL, NULL);

//  if (GTK_IS_TOGGLE_ACTION (action))
//    button = gtk_toggle_button_new ();
//  else
  const gchar* stock_id = gtk_action_get_stock_id (action);
  CString      tooltip  = g_strdup (gtk_action_get_tooltip (action));
  const gchar* help_id  = (const gchar*) g_object_get_qdata (G_OBJECT (action), GIMP_HELP_ID);

  GtkWidget* button = gimp_editor_add_dropdown (editor, stock_id, tooltip,
                                                help_id, list_id, NULL, NULL, NULL);

  gtk_activatable_set_related_action (GTK_ACTIVATABLE (button), action);

  va_start (args, action_name);

  action_name = va_arg (args, const gchar *);

  while (action_name)
    {
      GdkModifierType mod_mask;

      mod_mask = (GdkModifierType)va_arg (args, int);

      action = gtk_action_group_get_action (GTK_ACTION_GROUP (group),
                                            action_name);

      if (action && mod_mask)
        {
          ExtendedAction *ext = g_slice_new (ExtendedAction);

          ext->mod_mask = mod_mask;
          ext->action   = action;

          extended = g_list_prepend (extended, ext);

          if (tooltip)
            {
              const gchar *ext_tooltip = gtk_action_get_tooltip (action);

              if (ext_tooltip)
                {
                  tooltip = g_strconcat (tooltip, "\n<b>",
                                            gimp_get_mod_string (ext->mod_mask),
                                            "</b>  ", ext_tooltip, NULL);
                }
            }
        }

      action_name = va_arg (args, const gchar *);
    }

  va_end (args);

  if (extended)
    {
      g_object_set_data_full (G_OBJECT (button), "extended-actions", extended,
                              (GDestroyNotify) gimp_editor_button_extended_actions_free);

      g_signal_connect (button, "extended-clicked",
                        G_CALLBACK (gimp_editor_button_extended_clicked),
                        NULL);
    }

  if (tooltip || help_id)
    gimp_help_set_help_data_with_markup (button, tooltip, help_id);

  return button;
}
