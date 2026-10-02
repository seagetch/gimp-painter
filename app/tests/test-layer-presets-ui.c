/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gegl.h>
#include <gtk/gtk.h>
#include "libgimpbase/gimpbase.h"
#include "libgimpwidgets/gimpwidgets.h"
#include "core/core-types.h"
#include "widgets/widgets-types.h"
#include "core/gimp.h"
#include "core/gimpcontext.h"
#include "core/gimpimage.h"
#include "core/gimpimage-undo.h"
#include "core/gimplayer-new.h"
#include "core/gimplayerpreset.h"
#include "core/gimpcontainer.h"
#include "core/gimpdatafactory.h"
#include "core/gimpundostack.h"
#include "config/gimpcoreconfig.h"
#include "widgets/gimplayerpresetview.h"
#include "widgets/gimpcontainerview.h"
#include "widgets/gimpaction.h"
#include "widgets/gimpactiongroup.h"
#include "widgets/gimpactionfactory.h"
#include "widgets/gimpdialogfactory.h"
#include "display/display-types.h"
#include "actions/actions-types.h"
#include "actions/actions.h"
#include "actions/layer-presets-actions.h"
#include "menus/menus.h"
#include "dialogs/preferences-dialog.h"
#include "gimpcoreapp.h"
#include "tests.h"
#include "gimp-app-test-utils.h"
static Gimp *gimp;
static void factory_prepare (void)
{
  /* The synthetic dock is not a GimpImageWindow. Initialize the same deferred
   * device lifecycle a real first app focus performs, before preferences/save. */
  gimp_set_focused_once (gimp);
  gchar *path = g_build_filename (g_getenv ("GIMP_TESTING_ABS_TOP_SRCDIR"), "data", "layer-presets", NULL);
  g_object_set (gimp->config, "layer-presets-path", path, NULL); g_free (path);
  gimp_data_factory_data_refresh (gimp->layer_preset_factory, gimp->user_context);
}
static GimpImage *image_new (GimpLayer **source)
{
  GimpImage *image = gimp_image_new (gimp, 64, 64, GIMP_RGB, GIMP_PRECISION_U8_NON_LINEAR);
  *source = gimp_layer_new (image, 64, 64, gimp_image_get_layer_format (image, TRUE), "UI source", 1, GIMP_LAYER_MODE_NORMAL_LEGACY);
  gimp_image_add_layer (image, *source, NULL, 0, FALSE); gimp_context_set_image (gimp->user_context, image);
  return image;
}
static void select_source (GimpImage *image, GimpLayer *source)
{ GList *layers = g_list_prepend (NULL, source); gimp_image_set_selected_layers (image, layers); g_list_free (layers); }
static void selection_actions_lifetime (void)
{
  gchar *original = NULL;
  g_object_get (gimp->config, "layer-presets-path", &original, NULL);
  factory_prepare ();
  for (guint i = 0; i < 3; ++i)
    {
      GimpLayer *source; GimpImage *image = image_new (&source);
      GtkWidget *widget = gimp_layer_preset_view_new (gimp->user_context, 32, menus_get_global_menu_factory (gimp));
      GimpLayerPresetView *view = GIMP_LAYER_PRESET_VIEW (widget);
      GimpContainerView *container = GIMP_CONTAINER_EDITOR (view)->view;
      GimpContainer *presets = gimp_data_factory_get_container (gimp->layer_preset_factory);
      GimpViewable *preset = GIMP_VIEWABLE (gimp_container_get_child_by_name (presets, "Clone layer"));
      GtkWidget *window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      g_object_ref_sink (window); gtk_container_add (GTK_CONTAINER (window), widget); gtk_widget_show_all (window);
      g_assert_cmpint (gimp_image_get_n_layers (image), ==, 1);
      g_assert_nonnull (preset);
      gimp_container_view_set_1_selected (container, preset);
      /* GTK programmatic selection invokes the actual editor callback. */
      g_assert_cmpint (gimp_image_get_n_layers (image), ==, 2);
      g_assert_cmpint (gimp_undo_stack_get_depth (gimp_image_get_undo_stack (image)), ==, 1);
      g_assert_true (gimp_image_undo (image)); select_source (image, source);
      gimp_layer_preset_view_apply (view);
      g_assert_cmpint (gimp_image_get_n_layers (image), ==, 2);
      g_assert_true (gimp_image_undo (image)); select_source (image, source);
      GimpActionGroup *group = gimp_action_factory_get_group (global_action_factory, "layer-presets", view);
      g_assert_true (group == gimp_action_factory_get_group (global_action_factory, "layer-presets", view));
      gimp_action_group_update (group, view);
      gimp_action_activate (gimp_action_group_get_action (group, "layer-presets-refresh"));
      g_assert_cmpint (gimp_image_get_n_layers (image), ==, 1);
      gimp_action_group_add_actions (group, NULL, NULL, 0);
      gimp_action_group_add_string_actions (group, NULL, NULL, 0, NULL);
      gimp_context_set_image (gimp->user_context, NULL);
      g_assert_false (gimp_layer_preset_view_can_apply (view));
      GObject *weak_group = G_OBJECT (group), *weak_view = G_OBJECT (view);
      g_object_add_weak_pointer (weak_group, (gpointer *) &weak_group);
      g_object_add_weak_pointer (weak_view, (gpointer *) &weak_view);
      gtk_widget_destroy (window); g_object_unref (window); g_object_unref (image);
      g_assert_null (weak_group); g_assert_null (weak_view);
    }
  g_object_set (gimp->config, "layer-presets-path", original, NULL); g_free (original);
}
typedef struct { const gchar *text; gboolean found; } LabelSearch;
static gboolean tree_label (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data)
{
  LabelSearch *search = data;
  for (gint column = 0; column < gtk_tree_model_get_n_columns (model); ++column)
    if (gtk_tree_model_get_column_type (model, column) == G_TYPE_STRING)
      {
        gchar *text = NULL; gtk_tree_model_get (model, iter, column, &text, -1);
        if (g_strcmp0 (text, search->text) == 0) search->found = TRUE;
        g_free (text);
      }
  return search->found;
}
static gboolean has_label (GtkWidget *widget, const char *text)
{
  if (GTK_IS_TREE_VIEW (widget))
    {
      LabelSearch search = {text, FALSE};
      GtkTreeModel *model = gtk_tree_view_get_model (GTK_TREE_VIEW (widget));
      if (model) gtk_tree_model_foreach (model, tree_label, &search);
      if (search.found) return TRUE;
    }
  if (GTK_IS_LABEL (widget) && g_strcmp0 (gtk_label_get_text (GTK_LABEL (widget)), text) == 0) return TRUE;
  if (GTK_IS_CONTAINER (widget))
    {
      GList *children = gtk_container_get_children (GTK_CONTAINER (widget)); gboolean found = FALSE;
      for (GList *l = children; l && !found; l = l->next) found = has_label (l->data, text);
      g_list_free (children); return found;
    }
  return FALSE;
}
static void preferences_and_registration (void)
{
  GimpActionGroup *group = gimp_action_factory_get_group (global_action_factory, "dialogs", gimp);
  GimpDialogFactoryEntry *entry = gimp_dialog_factory_find_entry (gimp_dialog_factory_get_singleton (), "gimp-layer-preset-list");
  g_assert_nonnull (gimp_action_group_get_action (group, "dialogs-layer-presets")); g_assert_nonnull (entry);
  for (guint i = 0; i < 2; ++i)
    {
      GtkWidget *dialog = preferences_dialog_create (gimp); g_object_ref_sink (dialog);
      g_assert_true (has_label (dialog, "Layer Presets"));
      gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL); gtk_widget_destroy (dialog); g_object_unref (dialog);
    }
}
int main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL); if (!gtk_init_check (&argc, &argv)) return GIMP_EXIT_TEST_SKIPPED;
  gimp_test_utils_setup_menus_path (); gimp = gimp_init_for_gui_testing (TRUE);
  g_test_add_func ("/layer-presets-ui/selection-actions-lifetime", selection_actions_lifetime);
  g_test_add_func ("/layer-presets-ui/preferences-registration", preferences_and_registration);
  g_application_run (gimp->app, 0, NULL);
  gint result = gimp_core_app_get_exit_status (GIMP_CORE_APP (gimp->app));
  g_application_quit (G_APPLICATION (gimp->app)); g_clear_object (&gimp->app); return result;
}
