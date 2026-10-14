/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gegl.h>
#include <gtk/gtk.h>
#include "libgimpbase/gimpbase.h"
#include "libgimpwidgets/gimpwidgets.h"
#include "core/core-types.h"
#include "widgets/widgets-types.h"
#include "core/gimp.h"
#include "core/gimp-gui.h"
#include "core/gimpcontext.h"
#include "core/gimpdrawable.h"
#include "core/gimpimage.h"
#include "core/gimpimage-undo.h"
#include "core/gimplayer-new.h"
#include "core/gimplayer.h"
#include "core/gimplayerpreset.h"
#include "core/gimpcontainer.h"
#include "core/gimpdatafactory.h"
#include "core/gimpundostack.h"
#include "config/gimpcoreconfig.h"
#include "config/gimprc.h"
#include "widgets/gimplayerpresetview.h"
#include "widgets/gimpcontainerview.h"
#include "widgets/gimpaction.h"
#include "widgets/gimpactiongroup.h"
#include "widgets/gimpactionfactory.h"
#include "widgets/gimpdialogfactory.h"
#include "widgets/gimpdock.h"
#include "widgets/gimpdockable.h"
#include "widgets/gimpdockbook.h"
#include "widgets/gimpdocked.h"
#include "widgets/gimphelp-ids.h"
#include "widgets/gimpmenufactory.h"
#include "widgets/gimpuimanager.h"
#include "display/display-types.h"
#include "display/gimpdisplay.h"
#include "actions/actions-types.h"
#include "actions/actions.h"
#include "actions/dockable-commands.h"
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
/* Exercise the native wrapper, notebook ownership and visible Close Tab path,
 * rather than just placing the registered constructor's child in a window. */
static GtkWidget *
open_native_preset (gboolean through_action)
{
  GimpDialogFactory *factory = gimp_dialog_factory_get_singleton ();

  if (through_action)
    {
      GimpUIManager *manager = gimp_test_utils_get_ui_manager (gimp);
      GimpAction *action = gimp_ui_manager_find_action (manager, "dialogs",
                                                       "dialogs-layer-presets");
      g_assert_nonnull (action);
      g_assert_true (gimp_action_is_sensitive (action, NULL));
      g_assert_true (gimp_ui_manager_activate_action (manager, "dialogs",
                                                     "dialogs-layer-presets"));
      /* A creating factory call here would conceal a broken action callback. */
      return gimp_dialog_factory_find_widget (factory, "gimp-layer-preset-list");
    }

  GdkMonitor *monitor = gdk_display_get_monitor (gdk_display_get_default (), 0);
  g_assert_nonnull (monitor);
  return gimp_dialog_factory_dialog_raise (factory, monitor, NULL,
                                           "gimp-layer-preset-list", -1);
}

static guint
native_preset_count (GimpDialogFactory *factory,
                     GimpDialogFactoryEntry *entry)
{
  guint count = 0;

  for (GList *l = gimp_dialog_factory_get_open_dialogs (factory); l; l = l->next)
    {
      GimpDialogFactoryEntry *actual = NULL;
      if (gimp_dialog_factory_from_widget (GTK_WIDGET (l->data), &actual) == factory &&
          actual == entry)
        count++;
    }
  return count;
}

static void
wait_native_mapped (GtkWidget *widget)
{
  GtkWidget *observed = widget;
  gint64 deadline = g_get_monotonic_time () + 2 * G_TIME_SPAN_SECOND;

  /* One idle drain need not reach the next GTK allocation/frame after a
   * reused display is emptied and filled. Observe natural mapping without
   * retaining the widget or concealing a hidden/destroyed ancestor. */
  g_object_add_weak_pointer (G_OBJECT (observed), (gpointer *) &observed);
  while (observed && ! gtk_widget_get_mapped (observed) &&
         g_get_monotonic_time () < deadline)
    {
      g_main_context_iteration (NULL, FALSE);
      g_usleep (1000);
    }
  g_assert_nonnull (observed);
  if (! gtk_widget_get_mapped (observed))
    {
      gboolean single_window, hide_docks, painter_canvas;

      g_object_get (gimp->config,
                    "single-window-mode", &single_window,
                    "hide-docks", &hide_docks,
                    "painter-canvas-ui", &painter_canvas, NULL);
      g_test_message ("Native dock mapping: single-window=%d hide-docks=%d painter-canvas=%d",
                      single_window, hide_docks, painter_canvas);
      for (GtkWidget *ancestor = observed; ancestor;
           ancestor = gtk_widget_get_parent (ancestor))
        g_test_message ("%s (%s): visible=%d child-visible=%d mapped=%d realized=%d size=%dx%d",
                        G_OBJECT_TYPE_NAME (ancestor), gtk_widget_get_name (ancestor),
                        gtk_widget_get_visible (ancestor),
                        gtk_widget_get_child_visible (ancestor),
                        gtk_widget_get_mapped (ancestor),
                        gtk_widget_get_realized (ancestor),
                        gtk_widget_get_allocated_width (ancestor),
                        gtk_widget_get_allocated_height (ancestor));
    }
  g_assert_true (gtk_widget_get_mapped (observed));
  g_object_remove_weak_pointer (G_OBJECT (observed), (gpointer *) &observed);
}

static void
assert_native_source (GimpImage    *image,
                      GimpLayer    *source,
                      const guchar *pixels,
                      gint          redo_depth)
{
  guchar actual[64 * 64 * 4];
  GList *selected = gimp_image_get_selected_layers (image);

  g_assert_cmpint (gimp_image_get_n_layers (image), ==, 1);
  g_assert_true (gimp_container_get_child_by_index (gimp_image_get_layers (image), 0) ==
                 GIMP_OBJECT (source));
  g_assert_nonnull (selected);
  g_assert_true (selected->data == source);
  g_assert_null (selected->next);
  g_assert_cmpint (gimp_image_is_dirty (image), ==, 0);
  g_assert_cmpint (gimp_undo_stack_get_depth (gimp_image_get_undo_stack (image)), ==, 0);
  g_assert_cmpint (gimp_undo_stack_get_depth (gimp_image_get_redo_stack (image)), ==,
                   redo_depth);
  g_assert_true (gimp_item_get_image (GIMP_ITEM (source)) == image);
  g_assert_cmpstr (gimp_object_get_name (source), ==, "UI source");
  g_assert_cmpint (gimp_item_get_width (GIMP_ITEM (source)), ==, 64);
  g_assert_cmpint (gimp_item_get_height (GIMP_ITEM (source)), ==, 64);
  g_assert_cmpint (gimp_item_get_offset_x (GIMP_ITEM (source)), ==, 0);
  g_assert_cmpint (gimp_item_get_offset_y (GIMP_ITEM (source)), ==, 0);
  g_assert_cmpfloat (gimp_layer_get_opacity (source), ==, 1.0);
  g_assert_cmpint (gimp_layer_get_mode (source), ==, GIMP_LAYER_MODE_NORMAL_LEGACY);
  gegl_buffer_get (gimp_drawable_get_buffer (GIMP_DRAWABLE (source)),
                   GEGL_RECTANGLE (0, 0, 64, 64), 1.0, babl_format ("R'G'B'A u8"),
                   actual, GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);
  g_assert_cmpmem (actual, sizeof (actual), pixels, sizeof (actual));
}

static void
native_dockable_close_reopen (gboolean through_action)
{
  GimpDialogFactory *factory = gimp_dialog_factory_get_singleton ();
  GimpDialogFactoryEntry *entry = gimp_dialog_factory_find_entry (factory,
                                                                "gimp-layer-preset-list");
  GimpMenuFactory *menus = menus_get_global_menu_factory (gimp);
  GimpMenuFactoryEntry *menu_entry = NULL;
  GimpActionFactoryEntry *action_entry = NULL;
  GimpImage *saved_image = gimp_context_get_image (gimp->user_context);
  GimpDisplay *saved_display = gimp_context_get_display (gimp->user_context);
  GimpDisplay *empty_display = gimp_get_empty_display (gimp);
  GimpDisplay *display;
  gchar *saved_path = NULL;
  GimpLayer *source;
  GimpImage *image;
  GimpViewable *preset;
  guchar pixels[64 * 64 * 4];

  g_assert_nonnull (entry);
  g_assert_null (gimp_dialog_factory_find_widget (factory, entry->identifier));
  if (saved_image) g_object_ref (saved_image);
  if (saved_display) g_object_ref (saved_display);
  g_object_get (gimp->config, "layer-presets-path", &saved_path, NULL);
  factory_prepare ();
  image = image_new (&source);
  select_source (image, source);
  for (guint i = 0; i < sizeof (pixels); i++)
    pixels[i] = i % 4 == 3 ? 255 : (i * 37 + 11) % 256;
  gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (source)),
                   GEGL_RECTANGLE (0, 0, 64, 64), 0, babl_format ("R'G'B'A u8"),
                   pixels, GEGL_AUTO_ROWSTRIDE);
  gimp_image_clean_all (image);
  display = gimp_create_display (gimp, image, gimp_unit_pixel (), 1.0, NULL);
  g_assert_nonnull (display);
  gimp_test_run_mainloop_until_idle ();
  g_assert_true (gimp_context_get_display (gimp->user_context) == display);
  g_assert_true (gimp_context_get_image (gimp->user_context) == image);
  assert_native_source (image, source, pixels, 0);
  preset = GIMP_VIEWABLE (gimp_container_get_child_by_name (
    gimp_data_factory_get_container (gimp->layer_preset_factory), "Clone layer"));
  g_assert_nonnull (preset);
  for (GList *l = gimp_menu_factory_get_registered_menus (menus); l; l = l->next)
    if (g_strcmp0 (((GimpMenuFactoryEntry *) l->data)->identifier, "<LayerPresets>") == 0)
      menu_entry = l->data;
  for (GList *l = global_action_factory->registered_groups; l; l = l->next)
    if (g_strcmp0 (((GimpActionFactoryEntry *) l->data)->identifier, "layer-presets") == 0)
      action_entry = l->data;
  g_assert_nonnull (menu_entry);
  g_assert_nonnull (action_entry);

  for (guint cycle = 0; cycle < 2; cycle++)
    {
      GtkWidget *dockable;
      GtkWidget *child;
      GimpDockbook *book;
      GimpLayerPresetView *view;
      GimpContainerView *container;
      GimpContext *native_context;
      GimpUIManager *manager;
      GimpActionGroup *group;
      GimpDialogFactoryEntry *actual = NULL;
      const gchar *ui_path = NULL;
      gpointer popup_data = NULL;
      gpointer weak_dockable, weak_view, weak_manager, weak_group, owner;
      gint page;

      assert_native_source (image, source, pixels, cycle ? 1 : 0);
      /* Reach gimp_dockable_new() before help assertions, so the old NULL
       * registration fails at the production precondition, not a proxy. */
      dockable = open_native_preset (through_action);
      g_assert_nonnull (dockable);
      g_assert_true (GIMP_IS_DOCKABLE (dockable));
      assert_native_source (image, source, pixels, cycle ? 1 : 0);
      gimp_test_run_mainloop_until_idle ();
      assert_native_source (image, source, pixels, cycle ? 1 : 0);
      g_assert_true (gimp_dialog_factory_from_widget (dockable, &actual) == factory);
      g_assert_true (actual == entry);
      g_assert_true (entry->dockable);
      g_assert_cmpstr (entry->help_id, ==, GIMP_HELP_DOCK);
      g_assert_cmpstr (gimp_dockable_get_help_id (GIMP_DOCKABLE (dockable)), ==,
                       GIMP_HELP_DOCK);
      g_assert_cmpstr (gimp_action_get_help_id (gimp_ui_manager_find_action (
        gimp_test_utils_get_ui_manager (gimp), "dialogs", "dialogs-layer-presets")), ==,
        GIMP_HELP_DOCK);
      book = gimp_dockable_get_dockbook (GIMP_DOCKABLE (dockable));
      g_assert_true (GIMP_IS_DOCKBOOK (book));
      g_assert_true (GIMP_IS_DOCK (gimp_dockbook_get_dock (book)));
      g_assert_true (gtk_widget_get_parent (dockable) == GTK_WIDGET (book));
      page = gtk_notebook_page_num (GTK_NOTEBOOK (book), dockable);
      g_assert_cmpint (page, >=, 0);
      child = gtk_bin_get_child (GTK_BIN (dockable));
      g_assert_true (GIMP_IS_LAYER_PRESET_VIEW (child));
      g_assert_true (GIMP_IS_DOCKED (child));
      g_assert_true (gtk_widget_get_parent (child) == dockable);
      wait_native_mapped (dockable);
      wait_native_mapped (child);
      g_assert_true (gtk_widget_get_mapped (dockable));
      g_assert_true (gtk_widget_get_mapped (child));
      view = GIMP_LAYER_PRESET_VIEW (child);
      container = GIMP_CONTAINER_EDITOR (view)->view;
      native_context = gimp_container_view_get_context (container);
      g_assert_nonnull (native_context);
      g_object_ref (native_context);
      /* Construction must bind the live image before any test-side context
       * assignment, making the no-implicit-Apply checks meaningful. */
      g_assert_true (gimp_context_get_image (native_context) == image);
      g_assert_true (gimp_context_get_display (native_context) == display);
      /* A Layer Preset has no context resource property. Preview-style tabs
       * must use the registered icon without asking for a NULL property. */
      g_assert_null (gimp_context_type_to_prop_name (GIMP_TYPE_LAYER_PRESET));
      g_assert_null (GIMP_DOCKED_GET_IFACE (child)->get_preview);
      g_assert_cmpstr (gimp_dockable_get_icon_name (GIMP_DOCKABLE (dockable)), ==,
                       GIMP_ICON_LAYER);
      for (guint style = GIMP_TAB_STYLE_PREVIEW;
           style <= GIMP_TAB_STYLE_PREVIEW_BLURB; style++)
        {
          if (style != GIMP_TAB_STYLE_PREVIEW &&
              style != GIMP_TAB_STYLE_PREVIEW_NAME &&
              style != GIMP_TAB_STYLE_PREVIEW_BLURB)
            continue;
          GtkWidget *tab = gimp_dockable_create_tab_widget (
            GIMP_DOCKABLE (dockable), native_context, style, GTK_ICON_SIZE_MENU);
          g_assert_nonnull (tab);
          g_object_ref_sink (tab);
          gtk_widget_destroy (tab);
          g_object_unref (tab);
        }
      gimp_dockable_set_tab_style (GIMP_DOCKABLE (dockable),
                                   GIMP_TAB_STYLE_PREVIEW_NAME);
      g_assert_cmpint (gimp_dockable_get_tab_style (GIMP_DOCKABLE (dockable)), ==,
                       GIMP_TAB_STYLE_ICON_NAME);
      assert_native_source (image, source, pixels, cycle ? 1 : 0);
      manager = gimp_dockable_get_menu (GIMP_DOCKABLE (dockable), &ui_path, &popup_data);
      g_assert_nonnull (manager);
      g_assert_cmpstr (ui_path, ==, "/layer-presets-popup");
      g_assert_true (popup_data == view);
      g_assert_true (g_hash_table_lookup (menu_entry->managers, view) == manager);
      group = gimp_action_factory_get_group (global_action_factory, "layer-presets", view);
      g_assert_true (g_hash_table_lookup (action_entry->groups, view) == group);

      g_assert_true (open_native_preset (through_action) == dockable);
      gimp_test_run_mainloop_until_idle ();
      g_assert_true (gtk_bin_get_child (GTK_BIN (dockable)) == child);
      g_assert_cmpuint (native_preset_count (factory, entry), ==, 1);
      assert_native_source (image, source, pixels, cycle ? 1 : 0);
      /* Selection and the actual Apply action must still work after reopen. */
      gimp_container_view_set_1_selected (container, preset);
      g_assert_cmpint (gimp_image_get_n_layers (image), ==, 2);
      g_assert_cmpint (gimp_undo_stack_get_depth (gimp_image_get_undo_stack (image)), ==, 1);
      g_assert_true (gimp_image_undo (image));
      select_source (image, source);
      assert_native_source (image, source, pixels, 1);
      gimp_ui_manager_update (manager, popup_data);
      g_assert_true (gimp_action_is_sensitive (gimp_action_group_get_action (
        group, "layer-presets-apply"), NULL));
      g_assert_true (gimp_ui_manager_activate_action (manager, "layer-presets",
                                                     "layer-presets-apply"));
      g_assert_cmpint (gimp_image_get_n_layers (image), ==, 2);
      g_assert_cmpint (gimp_undo_stack_get_depth (gimp_image_get_undo_stack (image)), ==, 1);
      g_assert_true (gimp_image_undo (image));
      select_source (image, source);
      assert_native_source (image, source, pixels, 1);

      /* Even with a preset selected, context attachment must not apply it. */
      gimp_dockable_set_context (GIMP_DOCKABLE (dockable), NULL);
      g_assert_null (gimp_container_view_get_context (container));
      g_assert_false (gimp_layer_preset_view_can_apply (view));
      assert_native_source (image, source, pixels, 1);
      gimp_dockable_set_context (GIMP_DOCKABLE (dockable), native_context);
      g_assert_true (gimp_container_view_get_context (container) == native_context);
      g_assert_true (gimp_context_get_image (native_context) == image);
      g_assert_true (gimp_context_get_display (native_context) == display);
      g_assert_true (gimp_layer_preset_view_can_apply (view));
      assert_native_source (image, source, pixels, 1);
      g_object_unref (native_context);
      gimp_test_run_mainloop_until_idle ();
      assert_native_source (image, source, pixels, 1);

      owner = weak_view = view;
      weak_dockable = dockable;
      weak_manager = manager;
      weak_group = group;
      g_object_add_weak_pointer (G_OBJECT (dockable), &weak_dockable);
      g_object_add_weak_pointer (G_OBJECT (view), &weak_view);
      g_object_add_weak_pointer (G_OBJECT (manager), &weak_manager);
      g_object_add_weak_pointer (G_OBJECT (group), &weak_group);
      gtk_notebook_set_current_page (GTK_NOTEBOOK (book), page);
      dockable_close_tab_cmd_callback (NULL, NULL, book);
      gimp_test_run_mainloop_until_idle ();
      g_assert_null (weak_dockable);
      g_assert_null (weak_view);
      g_assert_null (weak_manager);
      g_assert_null (weak_group);
      g_assert_false (g_hash_table_contains (menu_entry->managers, owner));
      g_assert_false (g_hash_table_contains (action_entry->groups, owner));
      g_assert_null (gimp_dialog_factory_find_widget (factory, entry->identifier));
      g_assert_cmpuint (native_preset_count (factory, entry), ==, 0);
      assert_native_source (image, source, pixels, 1);
    }

  /* gimp_create_display() can fill the harness's existing empty display.
   * Restore that display rather than destroying an original image window. */
  if (display == empty_display)
    {
      gimp_display_empty (display);
      g_assert_null (gimp_display_get_image (display));
    }
  else
    {
      gimp_display_delete (display);
    }
  gimp_test_run_mainloop_until_idle ();
  gimp_context_set_display (gimp->user_context, saved_display);
  gimp_context_set_image (gimp->user_context, saved_image);
  g_assert_true (gimp_context_get_display (gimp->user_context) == saved_display);
  g_assert_true (gimp_context_get_image (gimp->user_context) == saved_image);
  g_object_unref (image);
  g_clear_object (&saved_display);
  g_clear_object (&saved_image);
  g_object_set (gimp->config, "layer-presets-path", saved_path, NULL);
  g_free (saved_path);
  gimp_data_factory_data_refresh (gimp->layer_preset_factory, gimp->user_context);
  /* Flush the normal config autosave while its owner and contexts are alive. */
  gimp_test_run_mainloop_until_idle ();
  g_assert_cmpuint (GIMP_RC (gimp->config)->save_idle_id, ==, 0);
  g_assert_cmpuint (GIMP_RC (gimp->edit_config)->save_idle_id, ==, 0);
}

static void dialog_factory_dockable_lifetime (void)
{ native_dockable_close_reopen (FALSE); }
static void action_dockable_close_reopen (void)
{ native_dockable_close_reopen (TRUE); }

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
static GtkWidget *filter_storage_entry (GtkWidget *widget)
{
  if (GTK_IS_ENTRY (widget) &&
      g_strcmp0 (gtk_widget_get_name (widget), "painter-filter-spill-size") == 0)
    return widget;
  if (GTK_IS_CONTAINER (widget))
    {
      GList *children = gtk_container_get_children (GTK_CONTAINER (widget));
      GtkWidget *found = NULL;
      for (GList *l = children; l && !found; l = l->next)
        found = filter_storage_entry (l->data);
      g_list_free (children);
      return found;
    }
  return NULL;
}
static void assert_storage_text (GtkWidget *entry, guint64 value)
{
  gchar *expected = g_strdup_printf ("%" G_GUINT64_FORMAT, value);
  g_assert_cmpstr (gtk_entry_get_text (GTK_ENTRY (entry)), ==, expected);
  g_free (expected);
}
static void filter_storage_preferences (void)
{
  const guint64 initial[] = {G_GUINT64_CONSTANT (8)*1024*1024*1024,
                             GIMP_MAX_MEMSIZE+1, G_MAXUINT64};
  const guint64 values[] = {0, G_GUINT64_CONSTANT (9)*1024*1024*1024+123,
                            GIMP_MAX_MEMSIZE, GIMP_MAX_MEMSIZE+1,
                            (G_GUINT64_CONSTANT (1)<<53)+1, G_MAXUINT64};
  const gchar *invalid[] = {"", "-1", "+1", "1.5", " 1", "1 ", "8G",
                            "18446744073709551616", "999999999999999999999999"};
  guint64 original;

  gimp_set_focused_once (gimp);
  g_object_get (gimp->edit_config, "painter-filter-spill-size", &original, NULL);
  for (guint i = 0; i < G_N_ELEMENTS (initial); ++i)
    {
      GtkWidget *dialog;
      GtkWidget *entry;
      GtkWidget *ok;
      GObject   *copy;
      guint64    value;

      g_object_set (gimp->edit_config, "painter-filter-spill-size", initial[i], NULL);
      dialog = preferences_dialog_create (gimp); g_object_ref_sink (dialog);
      entry = filter_storage_entry (dialog);
      g_assert_nonnull (entry); g_assert_true (gtk_widget_get_visible (entry));
      assert_storage_text (entry, initial[i]);
      copy = g_object_get_data (G_OBJECT (dialog), "config-copy");
      ok = gtk_dialog_get_widget_for_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
      for (guint j = 0; j < G_N_ELEMENTS (values); ++j)
        {
          gchar *text = g_strdup_printf ("%" G_GUINT64_FORMAT, values[j]);
          gtk_entry_set_text (GTK_ENTRY (entry), text); g_free (text);
          g_object_get (copy, "painter-filter-spill-size", &value, NULL);
          g_assert_cmpuint (value, ==, values[j]);
          g_object_get (gimp->edit_config, "painter-filter-spill-size", &value, NULL);
          g_assert_cmpuint (value, ==, values[j]);
          g_assert_null (gtk_entry_get_icon_name (GTK_ENTRY (entry), GTK_ENTRY_ICON_SECONDARY));
          g_assert_true (gtk_widget_get_sensitive (ok));
        }
      for (guint j = 0; j < G_N_ELEMENTS (invalid); ++j)
        {
          gtk_entry_set_text (GTK_ENTRY (entry), invalid[j]);
          g_object_get (copy, "painter-filter-spill-size", &value, NULL);
          g_assert_cmpuint (value, ==, G_MAXUINT64);
          g_object_get (gimp->edit_config, "painter-filter-spill-size", &value, NULL);
          g_assert_cmpuint (value, ==, G_MAXUINT64);
          g_assert_nonnull (gtk_entry_get_icon_name (GTK_ENTRY (entry), GTK_ENTRY_ICON_SECONDARY));
          g_assert_true (gtk_style_context_has_class (gtk_widget_get_style_context (entry), GTK_STYLE_CLASS_ERROR));
          g_assert_false (gtk_widget_get_sensitive (ok));
        }
      /* External/reset notification clears invalid feedback and preserves every
       * bit. Cancel then restores the original value, including oversized ones. */
      g_object_set (copy, "painter-filter-spill-size", initial[i], NULL);
      assert_storage_text (entry, initial[i]);
      g_assert_null (gtk_entry_get_icon_name (GTK_ENTRY (entry), GTK_ENTRY_ICON_SECONDARY));
      g_assert_true (gtk_widget_get_sensitive (ok));
      gtk_entry_set_text (GTK_ENTRY (entry), "0");
      gtk_entry_set_text (GTK_ENTRY (entry), "invalid before cancel");
      g_object_add_weak_pointer (G_OBJECT (entry), (gpointer *) &entry);
      gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL);
      gtk_widget_destroy (dialog); g_object_unref (dialog); g_assert_null (entry);
      g_object_get (gimp->edit_config, "painter-filter-spill-size", &value, NULL);
      g_assert_cmpuint (value, ==, initial[i]);
      dialog = preferences_dialog_create (gimp); g_object_ref_sink (dialog);
      entry = filter_storage_entry (dialog); g_assert_nonnull (entry);
      assert_storage_text (entry, initial[i]);
      gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL);
      gtk_widget_destroy (dialog); g_object_unref (dialog);
    }
  g_object_set (gimp->edit_config, "painter-filter-spill-size", original, NULL);
  /* Restore via the real autosave lifecycle while its Gimp owner/context is
   * still alive, rather than leaving the idle save for application disposal. */
  gimp_test_run_mainloop_until_idle ();
}
static void close_preferences_from_icon (GObject    *entry,
                                        GParamSpec *pspec,
                                        gpointer    data)
{
  GtkWidget *dialog = data;
  g_signal_handlers_disconnect_by_func (entry, close_preferences_from_icon, data);
  gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL);
  gtk_widget_destroy (dialog);
  g_object_unref (dialog);
}
static void filter_storage_reentrant_close (void)
{
  GtkWidget *dialog;
  GtkWidget *entry;
  GObject   *weak_entry;

  gimp_set_focused_once (gimp);
  dialog = preferences_dialog_create (gimp); g_object_ref_sink (dialog);
  entry = filter_storage_entry (dialog); g_assert_nonnull (entry);
  g_object_ref (entry);
  weak_entry = G_OBJECT (entry);
  g_object_add_weak_pointer (weak_entry, (gpointer *) &weak_entry);
  g_object_add_weak_pointer (G_OBJECT (dialog), (gpointer *) &dialog);
  g_signal_connect (entry, "notify::secondary-icon-name",
                    G_CALLBACK (close_preferences_from_icon), dialog);
  gtk_entry_set_text (GTK_ENTRY (entry), "overflow during notification");
  g_assert_null (dialog);
  g_object_unref (entry); g_assert_null (weak_entry);
  gimp_test_run_mainloop_until_idle ();
}
int main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL); if (!gtk_init_check (&argc, &argv)) return GIMP_EXIT_TEST_SKIPPED;
  gimp_test_utils_setup_menus_path (); gimp = gimp_init_for_gui_testing (TRUE);
  g_test_add_func ("/layer-presets-ui/selection-actions-lifetime", selection_actions_lifetime);
  g_test_add_func ("/layer-presets-ui/preferences-registration", preferences_and_registration);
  g_test_add_func ("/layer-presets-ui/dialog-factory-dockable-lifetime", dialog_factory_dockable_lifetime);
  g_test_add_func ("/layer-presets-ui/action-dockable-close-reopen", action_dockable_close_reopen);
  g_test_add_func ("/layer-presets-ui/filter-storage-preferences", filter_storage_preferences);
  g_test_add_func ("/layer-presets-ui/filter-storage-reentrant-close", filter_storage_reentrant_close);
  g_application_run (gimp->app, 0, NULL);
  gint result = gimp_core_app_get_exit_status (GIMP_CORE_APP (gimp->app));
  g_application_quit (G_APPLICATION (gimp->app)); g_clear_object (&gimp->app); return result;
}
