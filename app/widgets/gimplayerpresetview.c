/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gegl.h>
#include <gtk/gtk.h>
#include "libgimpwidgets/gimpwidgets.h"
#include "widgets-types.h"
#include "core/gimp.h"
#include "core/gimpcontext.h"
#include "core/gimpcontainer.h"
#include "core/gimpdatafactory.h"
#include "core/gimpimage.h"
#include "core/gimplayerpreset.h"
#include "gimplayerpresetview.h"
#include "gimpcontainerview.h"
#include "gimpcontainertreeview.h"
#include "gimpcontainertreestore.h"
#include "gimpviewrenderer.h"
#include "gimpwidgets-utils.h"
#include "gimpdocked.h"
#include "gimpeditor.h"
#include "gimpuimanager.h"
#include "gimpmenufactory.h"

typedef struct
{
  GdkEvent *press;
  GWeakRef preset;
  GWeakRef image;
  GWeakRef source;
  gboolean attempted;
  guint64 generation;
  GdkEvent *release_event; /* borrowed dispatch identity; never dereferenced */
  GimpContext *gesture_context;
  gulong image_changed_handler;
} GimpLayerPresetViewPrivate;

static void docked_iface_init (GimpDockedInterface *iface);
G_DEFINE_TYPE_WITH_CODE (GimpLayerPresetView, gimp_layer_preset_view,
                        GIMP_TYPE_DATA_FACTORY_VIEW,
                        G_ADD_PRIVATE (GimpLayerPresetView)
                        G_IMPLEMENT_INTERFACE (GIMP_TYPE_DOCKED, docked_iface_init))
static void docked_iface_init (GimpDockedInterface *iface)
{
  /* Layer Presets selection belongs to this view, not a GimpContext resource
   * property. Use the registered tab icon instead of a property-bound preview.
   * Keep the inherited context, menu, button-bar and auxiliary-info methods. */
  iface->get_preview = NULL;
}
/* A pointer gesture may notify selection on both press and release. Keep this
 * policy local to the immediately-applying preset view; ordinary container
 * selection notifications and explicit Apply requests retain their meaning. */
static void
gesture_clear (GimpLayerPresetView *view)
{
  GimpLayerPresetViewPrivate *priv = gimp_layer_preset_view_get_instance_private (view);
  priv->generation++;
  priv->release_event = NULL;
  g_clear_pointer (&priv->press, gdk_event_free);
  g_weak_ref_set (&priv->preset, NULL);
  g_weak_ref_set (&priv->image, NULL);
  g_weak_ref_set (&priv->source, NULL);
  priv->attempted = FALSE;
}

static GtkTreeView *
gesture_tree (GimpLayerPresetView *view)
{
  return GIMP_CONTAINER_TREE_VIEW (GIMP_CONTAINER_EDITOR (view)->view)->view;
}

static gboolean
gesture_own_button (GimpLayerPresetView *view,
                    GdkEvent           *event)
{
  return event &&
    (event->type == GDK_BUTTON_PRESS || event->type == GDK_BUTTON_RELEASE ||
     event->type == GDK_2BUTTON_PRESS || event->type == GDK_3BUTTON_PRESS) &&
    event->button.button == 1 &&
    event->button.window == gtk_tree_view_get_bin_window (gesture_tree (view));
}

static gboolean
gesture_same_press (GimpLayerPresetView *view,
                    GdkEvent           *event)
{
  GimpLayerPresetViewPrivate *priv = gimp_layer_preset_view_get_instance_private (view);
  return priv->press && gesture_own_button (view, event) &&
    event->button.window == priv->press->button.window &&
    gdk_event_get_device (event) == gdk_event_get_device (priv->press) &&
    gdk_event_get_source_device (event) == gdk_event_get_source_device (priv->press) &&
    (gint32) (event->button.time - priv->press->button.time) >= 0;
}

static gboolean
gesture_matches (GimpLayerPresetView *view,
                 GdkEvent           *event,
                 GimpViewable       *item)
{
  GimpLayerPresetViewPrivate *priv = gimp_layer_preset_view_get_instance_private (view);
  GObject *preset, *image, *source;
  GimpContext *context;
  GList *layers;
  gboolean matches;

  if (! gesture_same_press (view, event))
    return FALSE;
  preset = g_weak_ref_get (&priv->preset);
  image = g_weak_ref_get (&priv->image);
  source = g_weak_ref_get (&priv->source);
  context = gimp_container_view_get_context (GIMP_CONTAINER_EDITOR (view)->view);
  layers = image ? gimp_image_get_selected_layers (GIMP_IMAGE (image)) : NULL;
  matches = preset == G_OBJECT (item) && preset != NULL &&
    image != NULL && context != NULL &&
    G_OBJECT (gimp_context_get_image (context)) == image &&
    (priv->attempted || (source && layers && ! layers->next && layers->data == source));
  g_clear_object (&preset);
  g_clear_object (&image);
  g_clear_object (&source);
  return matches;
}

static gboolean
gesture_event (GtkWidget           *widget,
               GdkEvent            *event,
               GimpLayerPresetView *view)
{
  GimpLayerPresetViewPrivate *priv = gimp_layer_preset_view_get_instance_private (view);

  if (event->type == GDK_BUTTON_PRESS)
    {
      GtkTreePath *path = NULL;
      GtkTreeIter iter;
      GtkTreeModel *model = gtk_tree_view_get_model (GTK_TREE_VIEW (widget));
      GimpViewRenderer *renderer = NULL;

      gesture_clear (view);
      if (view->ready && ! view->menu_closed && gesture_own_button (view, event) &&
          ! (event->button.state & gimp_get_all_modifiers_mask ()) &&
          gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (widget),
                                        event->button.x, event->button.y,
                                        &path, NULL, NULL, NULL))
        {
          if (gtk_tree_model_get_iter (model, &iter, path))
            gtk_tree_model_get (model, &iter,
                                GIMP_CONTAINER_TREE_STORE_COLUMN_RENDERER, &renderer, -1);
          if (renderer && GIMP_IS_LAYER_PRESET (renderer->viewable))
            {
              GimpContext *context = gimp_container_view_get_context (GIMP_CONTAINER_EDITOR (view)->view);
              GimpImage *image = context ? gimp_context_get_image (context) : NULL;
              GList *layers = image ? gimp_image_get_selected_layers (image) : NULL;
              priv->press = gdk_event_copy (event);
              g_weak_ref_set (&priv->image, image ? G_OBJECT (image) : NULL);
              g_weak_ref_set (&priv->source, layers && ! layers->next ? G_OBJECT (layers->data) : NULL);
              g_weak_ref_set (&priv->preset, G_OBJECT (renderer->viewable));
            }
          g_clear_object (&renderer);
          gtk_tree_path_free (path);
        }
    }
  else if (event->type == GDK_BUTTON_RELEASE && gesture_same_press (view, event))
    priv->release_event = event;
  else if ((event->type == GDK_FOCUS_CHANGE && ! event->focus_change.in) ||
           (event->type == GDK_KEY_PRESS && event->key.keyval == GDK_KEY_Escape) ||
           (event->type == GDK_GRAB_BROKEN &&
            event->grab_broken.grab_window != gtk_widget_get_window (widget) &&
            event->grab_broken.grab_window != gtk_tree_view_get_bin_window (GTK_TREE_VIEW (widget))))
    gesture_clear (view);

  /* Observe before the existing button handler; never consume a GTK event. */
  return FALSE;
}

static void
gesture_event_after (GtkWidget           *widget,
                     GdkEvent            *event,
                     GimpLayerPresetView *view)
{
  GimpLayerPresetViewPrivate *priv = gimp_layer_preset_view_get_instance_private (view);

  /* Releases on blank space or editable cells need not notify selection.
   * Retire their token too, without clearing a reentrant newer gesture. */
  if (event->type == GDK_BUTTON_RELEASE && priv->release_event == event)
    gesture_clear (view);
}

static gboolean
gesture_allow_apply (GimpLayerPresetView *view,
                     GimpViewable       *item)
{
  GimpLayerPresetViewPrivate *priv = gimp_layer_preset_view_get_instance_private (view);
  GdkEvent *event = gtk_get_current_event ();
  gboolean allow = TRUE;

  if (gesture_own_button (view, event) &&
      ! (event->button.state & gimp_get_all_modifiers_mask ()))
    {
      gboolean matches = gesture_matches (view, event, item);

      if (! gesture_same_press (view, event))
        {
          /* A stale or foreign-device event must not consume a newer press. */
          allow = FALSE;
        }
      else if (event->type == GDK_BUTTON_RELEASE)
        {
          /* A cancelled/mismatched release cannot start another operation.
           * Consume before applying so reentrant close/new input cannot be
           * overwritten by an outer callback's completion. */
          allow = matches && ! priv->attempted;
          gesture_clear (view);
        }
      else if (matches)
        {
          allow = ! priv->attempted;
          /* Includes the second physical click's activation. Its release is
           * just another selection notification, not another application. */
          priv->attempted = TRUE;
        }
      else
        {
          /* Cancelled or retargeted pointer activation is not an independent
           * API request. In particular, a late double-press cannot revive it. */
          allow = FALSE;
          gesture_clear (view);
        }
    }
  else
    {
      /* Keyboard/API selection is independent of any interrupted gesture. */
      gesture_clear (view);
    }
  if (event) gdk_event_free (event);
  return allow;
}

static void
gesture_image_changed (GimpContext         *context,
                       GimpImage           *image,
                       GimpLayerPresetView *view)
{
  /* Even a round trip A -> B -> A invalidates the original gesture. */
  gesture_clear (view);
}

static void
gesture_watch_context (GimpLayerPresetView *view,
                       GimpContext         *context)
{
  GimpLayerPresetViewPrivate *priv = gimp_layer_preset_view_get_instance_private (view);

  gesture_clear (view);
  if (priv->gesture_context && priv->image_changed_handler)
    g_signal_handler_disconnect (priv->gesture_context, priv->image_changed_handler);
  priv->image_changed_handler = 0;
  g_set_object (&priv->gesture_context, context);
  if (context)
    priv->image_changed_handler = g_signal_connect_object (context, "image-changed",
      G_CALLBACK (gesture_image_changed), view, 0);
}

static void
gesture_notify (GObject             *object,
                GParamSpec          *pspec,
                GimpLayerPresetView *view)
{
  gesture_watch_context (view, gimp_container_view_get_context (
    GIMP_CONTAINER_EDITOR (view)->view));
}

static void
finalize (GObject *object)
{
  GimpLayerPresetViewPrivate *priv = gimp_layer_preset_view_get_instance_private (GIMP_LAYER_PRESET_VIEW (object));
  g_weak_ref_clear (&priv->preset);
  g_weak_ref_clear (&priv->image);
  g_weak_ref_clear (&priv->source);
  G_OBJECT_CLASS (gimp_layer_preset_view_parent_class)->finalize (object);
}

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
  if (!view->ready || view->menu_closed) return FALSE;
  preset = selected (view, &context, &source);
  return !view->applying && preset && gimp_layer_preset_matches (preset, source);
}
static void apply_selected (GimpLayerPresetView *view)
{
  GimpContext *context; GimpLayer *source; GimpLayerPreset *preset; GError *error = NULL;
  g_return_if_fail (GIMP_IS_LAYER_PRESET_VIEW (view));
  if (!view->ready || view->menu_closed || view->applying) return;
  preset = selected (view, &context, &source);
  if (!preset) return;
  /* Selection/application can synchronously close the containing popup.
   * Hold inputs through error reporting, after the backend releases its leases. */
  g_object_ref (view); g_object_ref (context); g_object_ref (source); g_object_ref (preset);
  view->applying = TRUE;
  if (!gimp_layer_preset_apply (preset, context, source, &error))
    { gimp_message_literal (context->gimp, G_OBJECT (view), GIMP_MESSAGE_ERROR, error ? error->message : "Unable to apply layer preset"); g_clear_error (&error); }
  view->applying = FALSE;
  g_object_unref (preset); g_object_unref (source); g_object_unref (context);
  g_object_unref (view);
}
void gimp_layer_preset_view_apply (GimpLayerPresetView *view)
{
  g_return_if_fail (GIMP_IS_LAYER_PRESET_VIEW (view));
  gesture_clear (view);
  apply_selected (view);
}
static void select_item (GimpContainerEditor *editor, GimpViewable *item)
{
  GimpLayerPresetView *view = GIMP_LAYER_PRESET_VIEW (editor);
  GimpLayerPresetViewPrivate *priv = gimp_layer_preset_view_get_instance_private (view);
  guint64 generation = priv->generation;
  g_object_ref (view);
  GIMP_CONTAINER_EDITOR_CLASS (gimp_layer_preset_view_parent_class)->select_item (editor, item);
  /* Legacy single-click selection applies the resource. Construction and
   * refresh selections are suppressed; explicit activation can apply again. */
  if (view->ready && ! view->applying && item && generation == priv->generation &&
      gimp_container_view_get_1_selected (GIMP_CONTAINER_EDITOR (view)->view) == item &&
      gesture_allow_apply (view, item) &&
      gimp_layer_preset_view_can_apply (view))
    apply_selected (view);
  g_object_unref (view);
}
static void activate_item (GimpContainerEditor *editor, GimpViewable *item)
{
  GimpLayerPresetView *view = GIMP_LAYER_PRESET_VIEW (editor);
  g_object_ref (view);
  if (item && view->ready && ! view->menu_closed && ! view->applying &&
      gimp_container_view_get_1_selected (GIMP_CONTAINER_EDITOR (view)->view) == item &&
      gesture_allow_apply (view, item))
    apply_selected (view);
  g_object_unref (view);
}
static void dispose (GObject *object)
{
  GimpLayerPresetView *view = GIMP_LAYER_PRESET_VIEW (object);
  if (!view->menu_closed)
    {
      GimpMenuFactory *factory = NULL;
      view->menu_closed = TRUE; view->ready = FALSE;
      gesture_watch_context (view, NULL);
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
  G_OBJECT_CLASS (klass)->finalize = finalize;
  GIMP_CONTAINER_EDITOR_CLASS (klass)->select_item = select_item;
  GIMP_CONTAINER_EDITOR_CLASS (klass)->activate_item = activate_item;
}
static void gimp_layer_preset_view_init (GimpLayerPresetView *view)
{
  GimpLayerPresetViewPrivate *priv = gimp_layer_preset_view_get_instance_private (view);
  g_weak_ref_init (&priv->preset, NULL);
  g_weak_ref_init (&priv->image, NULL);
  g_weak_ref_init (&priv->source, NULL);
}
GtkWidget *gimp_layer_preset_view_new (GimpContext *context, gint size, GimpMenuFactory *menu_factory)
{
  GimpLayerPresetView *view = g_object_new (GIMP_TYPE_LAYER_PRESET_VIEW,
    "view-type", GIMP_VIEW_TYPE_LIST, "data-factory", context->gimp->layer_preset_factory,
    "context", context, "view-size", size, "view-border-width", 0,
    "menu-factory", menu_factory, "menu-identifier", "<LayerPresets>",
    "ui-path", "/layer-presets-popup", "action-group", "layer-presets", NULL);
  gimp_editor_add_action_button (GIMP_EDITOR (GIMP_CONTAINER_EDITOR (view)->view),
                                  "layer-presets", "layer-presets-apply", NULL);
  GtkWidget *tree = GTK_WIDGET (gesture_tree (view));
  g_signal_connect_object (tree, "event", G_CALLBACK (gesture_event), view, 0);
  g_signal_connect_object (tree, "event-after", G_CALLBACK (gesture_event_after), view, 0);
  g_signal_connect_object (tree, "unmap", G_CALLBACK (gesture_clear), view, G_CONNECT_SWAPPED);
  g_signal_connect_object (tree, "drag-begin", G_CALLBACK (gesture_clear), view, G_CONNECT_SWAPPED);
  g_signal_connect_object (tree, "drag-end", G_CALLBACK (gesture_clear), view, G_CONNECT_SWAPPED);
  g_signal_connect_object (GIMP_CONTAINER_EDITOR (view)->view, "notify::context",
                           G_CALLBACK (gesture_notify), view, 0);
  g_signal_connect_object (gimp_data_factory_get_container (context->gimp->layer_preset_factory),
                           "freeze", G_CALLBACK (gesture_clear), view, G_CONNECT_SWAPPED);
  gesture_watch_context (view, gimp_container_view_get_context (
    GIMP_CONTAINER_EDITOR (view)->view));
  view->ready = TRUE;
  return GTK_WIDGET (view);
}
