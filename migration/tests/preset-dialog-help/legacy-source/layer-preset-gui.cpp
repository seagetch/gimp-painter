/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * GimpJsonResource
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

#include "base/delegators.hpp"
#include "base/glib-cxx-types.hpp"
#include "base/glib-cxx-bridge.hpp"
#include "base/glib-cxx-impl.hpp"
#include "base/glib-cxx-utils.hpp"

extern "C" {
#include "config.h"
#include <string.h>
#include <gtk/gtk.h>

#include "libgimpbase/gimpbase.h"
#include "libgimpconfig/gimpconfig.h"
#include "libgimpwidgets/gimpwidgets.h"


#include "widgets/widgets-types.h"
#include "widgets/gimpdatafactoryview.h"
#include "widgets/gimpmenufactory.h"
#include "widgets/gimpactiongroup.h"

#include "presets-enums.h"

#include "gimp-intl.h"
#include "core/gimpcontext.h"

}
#include "presets/gimpjsonresource.h"
#include "layer-preset-gui.h"

using namespace GLib;

////////////////////////////////////////////////////////////////////////////
// LayerPrestFactoryView

class LayerPresetFactoryView : virtual public ImplBase
{
public:
  LayerPresetFactoryView(GObject* o) : ImplBase(o) { }
  ~LayerPresetFactoryView() { }

  void select_item(GimpViewable* viewable) {
    g_print("LayerPresetFactoryView::select: %s\n", ref(viewable)[gimp_object_get_name]());

    auto self = ref(g_object);
    GObject* context = self["context"];
    if (context && GIMP_CONTEXT(context)->image) {
      auto applier = ILayerPresetApplier::new_instance(GIMP_CONTEXT(context), GIMP_JSON_RESOURCE(viewable));
      applier->apply_for_active_layer();
    }
  }
  void activate_item(GimpViewable* viewable) {
    g_print("LayerPresetFactoryView::activate: %s\n", ref(viewable)[gimp_object_get_name]());

    auto self = ref(g_object);
    GObject* context = self["context"];
    if (context && GIMP_CONTEXT(context)->image) {
      auto applier = ILayerPresetApplier::new_instance(GIMP_CONTEXT(context), GIMP_JSON_RESOURCE(viewable));
      applier->apply_for_active_layer();
    }
  }
};

extern const char layer_preset_factory_view[] = "GimpLayerPresetFactoryView";
using GClass = GLib::NewGClass<layer_preset_factory_view, DerivedFrom<GimpDataFactoryView>, LayerPresetFactoryView>;
#define __override(method) GClass::__(&klass->method).bind<&Impl::method>()

GClass class_instance([](IGClass::IWithClass* with_class){
  using Impl = LayerPresetFactoryView;
  g_print("LayerPresetFactoryView:: CLASS INITIALIZATION.\n");
  with_class->
  as_class<GimpContainerEditor>([](GimpContainerEditorClass* klass){
    __override (select_item);
    __override (activate_item);
  });
});

using GimpLayerPresetFactoryView = ::GClass::Instance;

namespace GLib {
template<> class Traits<::GClass::Instance> : public ::GClass::Traits { };
}




////////////////////////////////////////////////////////////////////////////
// LayerPrestGuiConfig

LayerPresetGuiConfig::LayerPresetGuiConfig() :
    LayerPresetConfig()
{
}


LayerPresetGuiConfig::~LayerPresetGuiConfig()
{
}


GObject*
LayerPresetGuiConfig::create_view (GObject* context,
                                   gint view_size,
                                   GObject* menu_factory)
{ 
  GimpDataFactory*            factory = get_data_factory();
  GimpLayerPresetFactoryView* factory_view;

  g_return_val_if_fail (GIMP_IS_CONTEXT (context), NULL);
  g_return_val_if_fail (view_size > 0 &&
                        view_size <= GIMP_VIEWABLE_MAX_PREVIEW_SIZE, NULL);
  g_return_val_if_fail (menu_factory == NULL ||
                        GIMP_IS_MENU_FACTORY (menu_factory), NULL);

  factory_view = Traits<GimpLayerPresetFactoryView>::cast(
      g_object_new (Traits<GimpLayerPresetFactoryView>::get_type(),
                    "view-type",         GIMP_VIEW_TYPE_LIST,
                    "data-factory",      factory,
                    "context",           context,
                    "view-size",         view_size,
                    "view-border-width", 0,
                    "menu-factory",      menu_factory,
                    "menu-identifier",   "<LayerPresets>",
                    "ui-path",           "/layer-presets-popup",
                    "action-group",      "layer-presets",
                    NULL));

  gtk_widget_hide (gimp_data_factory_view_get_duplicate_button (GIMP_DATA_FACTORY_VIEW (factory_view)));
  return G_OBJECT(factory_view);
}


GObject*
LayerPresetGuiConfig::create_editor ()
{ 
  return NULL; 
}


///////////////////////////////////////////////////
/// LayerPresetGuiConfig#view_dialog_new_entry
///
///  Returns information to create notebook page
///  for path in preference dialog.
///
///  @see dialogs/preferences-dialog.c
///////////////////////////////////////////////////
PrefsDialogEntry*
LayerPresetGuiConfig::prefs_dialog_entry ()
{
  static PrefsDialogEntry entry = {
    N_("Layer Presets"), N_("Layer presets Folders"), "folders-layer-presets",
    "",
    N_("Select Layer Presets Folders"),
    get_readonly_path_prop_name(), get_writable_path_prop_name()
  };
  return &entry;
}


///////////////////////////////////////////////////
/// LayerPresetGuiConfig#view_dialog_new_entry
///
///  Returns information to define new dialogs
///  to list up all presets and select one.
///
///  @see dialogs/dialogs.c
///////////////////////////////////////////////////
GimpDialogFactoryEntry*
LayerPresetGuiConfig::view_dialog_new_entry()
{
  static GimpDialogFactoryEntry entry = {
    "gimp-layer-preset-list", //*identifier,
    N_("Layer Presets"),      //*name,
    NULL,                     //*blurb,
    GIMP_STOCK_LAYER,         //*stock_id,
    "",                       //*help_id,
    NULL,                     //new_func,
    NULL,                     // restore_func
    GIMP_VIEW_SIZE_SMALL,     //view_size,
    FALSE,                    //singleton,
    FALSE,                    //session_managed,
    FALSE,                    //remember_size,
    TRUE,                     //remember_if_open,
    TRUE,                     //hideable,
    FALSE,                    //image_window,
    TRUE                      //dockable);
  };
  entry.new_func = PresetGuiConfig::dialog_new<LayerPresetGuiConfig>;
  return &entry;
}


///////////////////////////////////////////////////
/// LayerPresetGuiConfig#view_dialog_action_entry
///
///  returns information to define mapping from
///  menu (actions) to command to open dialog.
///
///  @see actions/dialogs-actions.c
///  @see actions/dialogs-commands.c
///////////////////////////////////////////////////
GimpStringActionEntry*
LayerPresetGuiConfig::view_dialog_action_entry()
{
  static GimpStringActionEntry entry =
      { "dialogs-layer-presets", GIMP_STOCK_LAYER,
        NC_("dialogs-action", "Layer presets"), NULL,
        NC_("dialogs-action", "Open layer presets dialog"),
        "gimp-layer-preset-list",
        "gimp-layer-preset-dialog" };
  return &entry;
}


LayerPresetGuiConfig::ActionGroupEntry*
LayerPresetGuiConfig::get_action_group_entry()
{
  static LayerPresetGuiConfig::ActionGroupEntry entry = {
      "layer-presets",
      "Layer presets",
      GIMP_STOCK_LAYER
  };
  return &entry;
}

////////////////////////////////////////////////////////////////////////////
// LayerPrestGuiConfig::startup code
static LayerPresetGuiConfig* instance = NULL;

PresetGuiConfig*
LayerPresetGuiConfig::config()
{
  if (!instance)
    instance = new LayerPresetGuiConfig();
  return instance;
}
