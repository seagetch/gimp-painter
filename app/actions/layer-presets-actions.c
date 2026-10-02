/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gegl.h>
#include <gtk/gtk.h>
#include "libgimpwidgets/gimpwidgets.h"
#include "actions-types.h"
#include "core/gimpcontext.h"
#include "core/gimpdatafactory.h"
#include "widgets/gimpactiongroup.h"
#include "widgets/gimpcontainerview.h"
#include "widgets/gimplayerpresetview.h"
#include "actions.h"
#include "layer-presets-actions.h"
#include "gimp-intl.h"
static void apply (GimpAction *action, GVariant *value, gpointer data)
{ if (GIMP_IS_LAYER_PRESET_VIEW (data)) gimp_layer_preset_view_apply (GIMP_LAYER_PRESET_VIEW (data)); }
static void refresh (GimpAction *action, GVariant *value, gpointer data)
{
  if (GIMP_IS_LAYER_PRESET_VIEW (data))
    {
      GimpLayerPresetView *view = GIMP_LAYER_PRESET_VIEW (data);
      GimpContext *context = gimp_container_view_get_context (GIMP_CONTAINER_EDITOR (view)->view);
      view->ready = FALSE;
      gimp_data_factory_data_refresh (gimp_data_factory_view_get_data_factory (GIMP_DATA_FACTORY_VIEW (view)), context);
      view->ready = TRUE;
    }
}
static const GimpActionEntry entries[] = {
  { "layer-presets-apply", GIMP_ICON_LAYER, NC_("layer-presets-action", "_Apply Layer Preset"), NULL, {NULL}, NC_("layer-presets-action", "Apply the selected preset to one selected layer"), apply, NULL },
  { "layer-presets-refresh", GIMP_ICON_VIEW_REFRESH, NC_("layer-presets-action", "_Refresh Layer Presets"), NULL, {NULL}, NC_("layer-presets-action", "Reload layer presets from their folders"), refresh, NULL }
};
void layer_presets_actions_setup (GimpActionGroup *group)
{ gimp_action_group_add_actions (group, "layer-presets-action", entries, G_N_ELEMENTS (entries)); }
void layer_presets_actions_update (GimpActionGroup *group, gpointer data)
{
  gimp_action_group_set_action_sensitive (group, "layer-presets-apply",
    GIMP_IS_LAYER_PRESET_VIEW (data) && gimp_layer_preset_view_can_apply (GIMP_LAYER_PRESET_VIEW (data)), NULL);
}
