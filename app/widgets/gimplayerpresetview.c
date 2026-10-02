/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gegl.h>
#include <gtk/gtk.h>
#include "libgimpwidgets/gimpwidgets.h"
#include "widgets-types.h"
#include "core/gimp.h"
#include "core/gimpcontext.h"
#include "core/gimpimage.h"
#include "core/gimplayerpreset.h"
#include "gimplayerpresetview.h"
#include "gimpcontainerview.h"
#include "gimpeditor.h"
#include "gimpuimanager.h"
#include "gimpmenufactory.h"

G_DEFINE_TYPE (GimpLayerPresetView, gimp_layer_preset_view, GIMP_TYPE_DATA_FACTORY_VIEW)
static GimpLayerPreset *selected (GimpLayerPresetView *view, GimpContext **context, GimpLayer **source)
{
  GimpContainerView *container = GIMP_CONTAINER_EDITOR (view)->view;
  GList *items = NULL, *layers;
  GimpLayerPreset *preset = NULL;
  GimpImage *image;
  *context = gimp_container_view_get_context (container);
  *source = NULL;
  if (!*context || !(image = gimp_context_get_image (*context))) return NULL;
  layers = gimp_image_get_selected_layers (image);
  if (!layers || layers->next) return NULL;
  *source = layers->data;
  if (gimp_container_view_get_selected (container, &items) == 1 && GIMP_IS_LAYER_PRESET (items->data)) preset = items->data;
  g_list_free (items);
  return preset;
}
gboolean gimp_layer_preset_view_can_apply (GimpLayerPresetView *view)
{
  GimpContext *context; GimpLayer *source; GimpLayerPreset *preset;
  g_return_val_if_fail (GIMP_IS_LAYER_PRESET_VIEW (view), FALSE);
  preset = selected (view, &context, &source);
  return !view->applying && preset && gimp_layer_preset_matches (preset, source);
}
void gimp_layer_preset_view_apply (GimpLayerPresetView *view)
{
  GimpContext *context; GimpLayer *source; GimpLayerPreset *preset; GError *error = NULL;
  g_return_if_fail (GIMP_IS_LAYER_PRESET_VIEW (view));
  if (view->applying) return;
  preset = selected (view, &context, &source);
  if (!preset) return;
  g_object_ref (view);
  view->applying = TRUE;
  if (!gimp_layer_preset_apply (preset, context, source, &error))
    { gimp_message_literal (context->gimp, G_OBJECT (view), GIMP_MESSAGE_ERROR, error ? error->message : "Unable to apply layer preset"); g_clear_error (&error); }
  view->applying = FALSE;
  g_object_unref (view);
}
static void select_item (GimpContainerEditor *editor, GimpViewable *item)
{
  GimpLayerPresetView *view = GIMP_LAYER_PRESET_VIEW (editor);
  GIMP_CONTAINER_EDITOR_CLASS (gimp_layer_preset_view_parent_class)->select_item (editor, item);
  /* Legacy single-click selection applies the resource. Construction and
   * refresh selections are suppressed; explicit activation can apply again. */
  if (view->ready && item && gimp_layer_preset_view_can_apply (view))
    gimp_layer_preset_view_apply (view);
}
static void activate_item (GimpContainerEditor *editor, GimpViewable *item)
{ if (item) gimp_layer_preset_view_apply (GIMP_LAYER_PRESET_VIEW (editor)); }
static void dispose (GObject *object)
{
  GimpLayerPresetView *view = GIMP_LAYER_PRESET_VIEW (object);
  if (!view->menu_closed)
    {
      GimpMenuFactory *factory = NULL;
      view->menu_closed = TRUE; view->ready = FALSE;
      g_object_get (object, "menu-factory", &factory, NULL);
      /* Menu/action caches are keyed by the borrowed callback owner. Remove
       * them before this owner dies, including repeated dispose/close paths. */
      if (factory)
        {
          gimp_menu_factory_delete_manager (factory, "<LayerPresets>", view);
          g_object_unref (factory);
        }
    }
  G_OBJECT_CLASS (gimp_layer_preset_view_parent_class)->dispose (object);
}
static void gimp_layer_preset_view_class_init (GimpLayerPresetViewClass *klass)
{
  G_OBJECT_CLASS (klass)->dispose = dispose;
  GIMP_CONTAINER_EDITOR_CLASS (klass)->select_item = select_item;
  GIMP_CONTAINER_EDITOR_CLASS (klass)->activate_item = activate_item;
}
static void gimp_layer_preset_view_init (GimpLayerPresetView *view) {}
GtkWidget *gimp_layer_preset_view_new (GimpContext *context, gint size, GimpMenuFactory *menu_factory)
{
  GimpLayerPresetView *view = g_object_new (GIMP_TYPE_LAYER_PRESET_VIEW,
    "view-type", GIMP_VIEW_TYPE_LIST, "data-factory", context->gimp->layer_preset_factory,
    "context", context, "view-size", size, "view-border-width", 0,
    "menu-factory", menu_factory, "menu-identifier", "<LayerPresets>",
    "ui-path", "/layer-presets-popup", "action-group", "layer-presets", NULL);
  gimp_editor_add_action_button (GIMP_EDITOR (GIMP_CONTAINER_EDITOR (view)->view),
                                  "layer-presets", "layer-presets-apply", NULL);
  view->ready = TRUE;
  return GTK_WIDGET (view);
}
