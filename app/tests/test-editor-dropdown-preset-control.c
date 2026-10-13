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
/* Independent of the new dropdown symbol and common-store controller.
 * The exact same source is runnable before and after the native view fix. */
static void retained_closed_view (void)
{
  GtkWidget *view = gimp_layer_preset_view_new (gimp->user_context, 32,
                                               menus_get_global_menu_factory (gimp));
  GtkWidget *window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  g_object_ref_sink (window); g_object_ref_sink (view);
  gtk_container_add (GTK_CONTAINER (window), view); gtk_widget_show_all (window);
  gtk_widget_destroy (window); g_object_unref (window);
  /* No child lease: the view's old child pointer is no longer a valid lookup
   * target. The public retained owner must reject before inspecting children. */
  g_assert_false (gimp_layer_preset_view_can_apply (GIMP_LAYER_PRESET_VIEW (view)));
  gimp_layer_preset_view_apply (GIMP_LAYER_PRESET_VIEW (view));
  g_object_run_dispose (G_OBJECT (view)); g_object_unref (view);
}
static void destroy_from_layer_add (GimpContainer *container, GimpObject *object, gpointer data)
{
  gtk_widget_destroy (GTK_WIDGET (data));
}
static void close_during_apply (void)
{
  GimpLayer *source; GimpImage *image = image_new (&source);
  GtkWidget *view = gimp_layer_preset_view_new (gimp->user_context, 32,
                                               menus_get_global_menu_factory (gimp));
  GtkWidget *window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  g_object_ref_sink (window); gtk_container_add (GTK_CONTAINER (window), view); gtk_widget_show_all (window);
  GimpContainerView *container = GIMP_CONTAINER_EDITOR (view)->view;
  GimpContainer *presets = gimp_data_factory_get_container (gimp->layer_preset_factory);
  GimpViewable *preset = GIMP_VIEWABLE (gimp_container_get_child_by_name (presets, "Clone layer"));
  g_assert_nonnull (preset);
  gulong handler = g_signal_connect (gimp_image_get_layers (image), "add", G_CALLBACK (destroy_from_layer_add), window);
  gimp_container_view_set_1_selected (container, preset);
  g_signal_handler_disconnect (gimp_image_get_layers (image), handler);
  g_assert_cmpint (gimp_image_get_n_layers (image), ==, 2);
  g_assert_cmpuint (gimp_image_get_undo_group_count (image), ==, 0);
  g_object_unref (window); gimp_context_set_image (gimp->user_context, NULL); g_object_unref (image);
}
int main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL); if (!gtk_init_check (&argc, &argv)) return GIMP_EXIT_TEST_SKIPPED;
  gimp_test_utils_setup_menus_path (); gimp = gimp_init_for_gui_testing (TRUE); factory_prepare ();
  g_test_add_func ("/editor-dropdown-preset-control/retained-closed-view", retained_closed_view);
  g_test_add_func ("/editor-dropdown-preset-control/close-during-apply", close_during_apply);
  g_application_run (gimp->app, 0, NULL);
  gint result = gimp_core_app_get_exit_status (GIMP_CORE_APP (gimp->app));
  g_application_quit (G_APPLICATION (gimp->app)); g_clear_object (&gimp->app); return result;
}
