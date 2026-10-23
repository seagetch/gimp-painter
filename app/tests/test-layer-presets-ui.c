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
#include "core/gimpclonelayer.h"
#include "core/gimpcontainer.h"
#include "core/gimplist.h"
#include "core/gimpdatafactory.h"
#include "core/gimpundostack.h"
#include "config/gimpcoreconfig.h"
#include "config/gimprc.h"
#include "widgets/gimplayerpresetview.h"
#include "widgets/gimpcontainerview.h"
#include "widgets/gimpcontainertreeview.h"
#include "widgets/gimpcontainertreestore.h"
#include "widgets/gimpviewrenderer.h"
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
#include "display/gimppaintercanvasui.h"
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

/* These controls go through GDK's queue and GTK's current-event stack. A
 * direct selection, signal emission or gtk_widget_event() would not reproduce
 * both native tree notifications with the dispatch identity used by the view. */
typedef struct
{
  GimpImage             *saved_image;
  GimpDisplay           *saved_display;
  GimpDisplay           *empty_display;
  GimpDisplay           *display;
  gchar                 *saved_path;
  GimpImage             *image;
  GimpLayer             *source;
  GimpViewable          *preset;
  GtkWidget            *dockable;
  GimpLayerPresetView   *view;
  GimpContainerView     *container;
  GimpContainerTreeView *tree;
  GdkEvent              *expected;
  GtkWidget             *event_target;
  guint                 delivered;
  guint                 press_selections;
  guint                 release_selections;
  guint                 activations;
  guchar                pixels[64 * 64 * 4];
} GestureFixture;

static void
gesture_event_after (GtkWidget      *widget,
                     GdkEvent       *event,
                     GestureFixture *f)
{
  GdkEvent *current;

  if (! f->expected || widget != f->event_target ||
      event->type != f->expected->type ||
      event->any.window != f->expected->any.window ||
      gdk_event_get_time (event) != gdk_event_get_time (f->expected))
    return;

  g_assert_true (gdk_event_get_device (event) ==
                 gdk_event_get_device (f->expected));
  g_assert_true (gdk_event_get_source_device (event) ==
                 gdk_event_get_source_device (f->expected));
  if (event->type == GDK_BUTTON_PRESS || event->type == GDK_BUTTON_RELEASE ||
      event->type == GDK_2BUTTON_PRESS)
    {
      g_assert_cmpuint (event->button.button, ==, f->expected->button.button);
      g_assert_cmpuint (event->button.state, ==, f->expected->button.state);
    }
  current = gtk_get_current_event ();
  g_assert_nonnull (current);
  g_assert_cmpint (current->type, ==, event->type);
  g_assert_true (current->any.window == event->any.window);
  g_assert_cmpuint (gdk_event_get_time (current), ==,
                    gdk_event_get_time (event));
  gdk_event_free (current);
  f->delivered++;
}

static void
gesture_item_activated (GimpContainerView *container,
                       GimpViewable      *item,
                       GestureFixture    *f)
{
  g_assert_true (item == f->preset);
  f->activations++;
}

static void
gesture_selection_changed (GimpContainerView *container,
                           GestureFixture    *f)
{
  GdkEvent *event = gtk_get_current_event ();

  if (event && event->any.window ==
      gtk_tree_view_get_bin_window (f->tree->view))
    {
      if (event->type == GDK_BUTTON_PRESS) f->press_selections++;
      if (event->type == GDK_BUTTON_RELEASE) f->release_selections++;
    }
  if (event) gdk_event_free (event);
}

static void
gesture_open (GestureFixture *f)
{
  f->dockable = open_native_preset (TRUE);
  g_assert_true (GIMP_IS_DOCKABLE (f->dockable));
  f->view = GIMP_LAYER_PRESET_VIEW (gtk_bin_get_child (GTK_BIN (f->dockable)));
  f->container = GIMP_CONTAINER_EDITOR (f->view)->view;
  g_assert_true (GIMP_IS_CONTAINER_TREE_VIEW (f->container));
  f->tree = GIMP_CONTAINER_TREE_VIEW (f->container);
  wait_native_mapped (f->dockable);
  wait_native_mapped (GTK_WIDGET (f->tree->view));
  g_assert_true (gimp_context_get_image (
    gimp_container_view_get_context (f->container)) == f->image);
  g_signal_connect (f->tree->view, "event-after",
                    G_CALLBACK (gesture_event_after), f);
  g_signal_connect (f->container, "selection-changed",
                    G_CALLBACK (gesture_selection_changed), f);
  g_signal_connect (f->container, "item-activated",
                    G_CALLBACK (gesture_item_activated), f);
}

static void
gesture_close (GestureFixture *f)
{
  GimpDockbook *book = gimp_dockable_get_dockbook (GIMP_DOCKABLE (f->dockable));
  gpointer weak_view = f->view, weak_dockable = f->dockable;

  g_object_add_weak_pointer (G_OBJECT (f->view), &weak_view);
  g_object_add_weak_pointer (G_OBJECT (f->dockable), &weak_dockable);
  gtk_notebook_set_current_page (GTK_NOTEBOOK (book),
    gtk_notebook_page_num (GTK_NOTEBOOK (book), f->dockable));
  dockable_close_tab_cmd_callback (NULL, NULL, book);
  gimp_test_run_mainloop_until_idle ();
  g_assert_null (weak_view);
  g_assert_null (weak_dockable);
  f->view = NULL; f->dockable = NULL; f->tree = NULL; f->container = NULL;
}

static void
gesture_fixture_init (GestureFixture *f)
{
  *f = (GestureFixture) { 0 };
  f->saved_image = gimp_context_get_image (gimp->user_context);
  f->saved_display = gimp_context_get_display (gimp->user_context);
  f->empty_display = gimp_get_empty_display (gimp);
  if (f->saved_image) g_object_ref (f->saved_image);
  if (f->saved_display) g_object_ref (f->saved_display);
  g_object_get (gimp->config, "layer-presets-path", &f->saved_path, NULL);
  factory_prepare ();
  f->image = image_new (&f->source);
  select_source (f->image, f->source);
  for (guint i = 0; i < sizeof (f->pixels); i++)
    f->pixels[i] = i % 4 == 3 ? 255 : (i * 37 + 11) % 256;
  gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (f->source)),
                   GEGL_RECTANGLE (0, 0, 64, 64), 0, babl_format ("R'G'B'A u8"),
                   f->pixels, GEGL_AUTO_ROWSTRIDE);
  gimp_image_clean_all (f->image);
  f->display = gimp_create_display (gimp, f->image, gimp_unit_pixel (), 1.0, NULL);
  g_assert_nonnull (f->display);
  gimp_test_run_mainloop_until_idle ();
  f->preset = GIMP_VIEWABLE (gimp_container_get_child_by_name (
    gimp_data_factory_get_container (gimp->layer_preset_factory), "Clone layer"));
  g_assert_nonnull (f->preset);
  gesture_open (f);
  assert_native_source (f->image, f->source, f->pixels, 0);
  g_assert_null (gimp_container_view_get_1_selected (f->container));
}

static void
gesture_fixture_clear (GestureFixture *f)
{
  gesture_close (f);
  if (f->display == f->empty_display) gimp_display_empty (f->display);
  else gimp_display_delete (f->display);
  gimp_test_run_mainloop_until_idle ();
  gimp_context_set_display (gimp->user_context, f->saved_display);
  gimp_context_set_image (gimp->user_context, f->saved_image);
  g_object_unref (f->image);
  g_clear_object (&f->saved_display);
  g_clear_object (&f->saved_image);
  g_object_set (gimp->config, "layer-presets-path", f->saved_path, NULL);
  g_free (f->saved_path);
  gimp_data_factory_data_refresh (gimp->layer_preset_factory, gimp->user_context);
  gimp_test_run_mainloop_until_idle ();
}

static GtkTreePath *
gesture_preset_path (GestureFixture *f)
{
  GtkTreeIter iter;
  gboolean valid = gtk_tree_model_get_iter_first (f->tree->model, &iter);

  while (valid)
    {
      GimpViewRenderer *renderer = gimp_container_tree_store_get_renderer (
        GIMP_CONTAINER_TREE_STORE (f->tree->model), &iter);
      gboolean match = renderer->viewable == f->preset;
      g_object_unref (renderer);
      if (match) return gtk_tree_model_get_path (f->tree->model, &iter);
      valid = gtk_tree_model_iter_next (f->tree->model, &iter);
    }
  g_assert_not_reached ();
  return NULL;
}

static GdkEvent *
gesture_button_event (GestureFixture *f,
                      GdkEventType    type,
                      guint32         time,
                      gboolean        icon)
{
  GtkTreePath *path = gesture_preset_path (f), *hit = NULL;
  GtkTreeViewColumn *column = f->tree->main_column, *hit_column = NULL;
  GtkCellRenderer *cell = icon ? f->tree->renderer_cell :
    gimp_container_tree_view_get_name_cell (f->tree);
  GdkWindow *window = gtk_tree_view_get_bin_window (f->tree->view);
  GdkSeat *seat = gdk_display_get_default_seat (gdk_window_get_display (window));
  GdkRectangle area;
  gint start, width, root_x, root_y;
  gint click_x = 0, click_y = 0;
  gboolean target_visible = FALSE;
  GdkEvent *event = gdk_event_new (type);
  gint64 deadline = g_get_monotonic_time () + 2 * G_TIME_SPAN_SECOND;

  gtk_tree_view_scroll_to_cell (f->tree->view, path, NULL, TRUE, 0.5, 0.0);
  while (! gdk_window_is_viewable (window) && g_get_monotonic_time () < deadline)
    { g_main_context_iteration (NULL, FALSE); g_usleep (1000); }
  g_assert_true (gdk_window_is_viewable (window));
  /* Mapping/allocation can precede deferred scrolling. A portrait oncanvas
   * layout can also clip a renderer's full width. Wait for an actually visible
   * portion of the target cell and verify its native row/column hit-test before
   * dispatch; never resize the dock or invent coordinates outside its viewport. */
  do
    {
      gtk_tree_view_get_cell_area (f->tree->view, path, column, &area);
      g_assert_true (gtk_tree_view_column_cell_get_position (column, cell, &start, &width));
      if (width > 0 && area.height > 0)
        {
          gint left = MAX (0, area.x + start);
          gint right = MIN (gdk_window_get_width (window), area.x + start + width);
          gint top = MAX (0, area.y);
          gint bottom = MIN (gdk_window_get_height (window), area.y + area.height);

          if (left < right && top < bottom)
            {
              click_x = left + (right - left) / 2;
              click_y = top + (bottom - top) / 2;
              target_visible = gtk_tree_view_get_path_at_pos (f->tree->view,
                click_x, click_y, &hit, &hit_column, NULL, NULL) &&
                gtk_tree_path_compare (path, hit) == 0 && hit_column == column;
              g_clear_pointer (&hit, gtk_tree_path_free);
              if (target_visible) break;
            }
        }
      g_main_context_iteration (NULL, FALSE);
      g_usleep (1000);
    }
  while (g_get_monotonic_time () < deadline);
  g_assert_cmpint (width, >, 0);
  g_assert_cmpint (area.height, >, 0);
  if (!target_visible)
    {
      const gchar *capture = g_getenv ("GIMP_TEST_PRESET_GEOMETRY_SCREENSHOT");
      g_test_message ("target area=%d,%d %dx%d renderer=%d,%d viewport=%dx%d click=%d,%d",
                      area.x, area.y, area.width, area.height, start, width,
                      gdk_window_get_width (window), gdk_window_get_height (window),
                      click_x, click_y);
      if (capture)
        {
          GdkWindow *root_window = gdk_get_default_root_window ();
          GdkPixbuf *pixels = gdk_pixbuf_get_from_window (root_window, 0, 0,
            gdk_window_get_width (root_window), gdk_window_get_height (root_window));
          GError *error = NULL;
          g_assert_nonnull (pixels);
          g_assert_true (gdk_pixbuf_save (pixels, capture, "png", &error, NULL));
          g_assert_no_error (error);
          g_object_unref (pixels);
        }
    }
  g_assert_true (target_visible);
  event->button.window = g_object_ref (window);
  event->button.send_event = TRUE;
  event->button.time = time;
  event->button.button = 1;
  event->button.state = type == GDK_BUTTON_RELEASE ? GDK_BUTTON1_MASK : 0;
  event->button.x = click_x;
  event->button.y = click_y;
  gdk_window_get_origin (window, &root_x, &root_y);
  event->button.x_root = root_x + event->button.x;
  event->button.y_root = root_y + event->button.y;
  gdk_event_set_device (event, gdk_seat_get_pointer (seat));
  gdk_event_set_source_device (event, gdk_seat_get_pointer (seat));
  g_assert_true (gtk_tree_view_get_path_at_pos (f->tree->view,
    event->button.x, event->button.y, &hit, &hit_column, NULL, NULL));
  g_assert_cmpint (gtk_tree_path_compare (path, hit), ==, 0);
  g_assert_true (hit_column == column);
  gtk_tree_path_free (path); gtk_tree_path_free (hit);
  return event;
}

static void
gesture_dispatch_to (GestureFixture *f,
                     GtkWidget      *target,
                     GdkEvent       *event)
{
  gint64 deadline = g_get_monotonic_time () + 2 * G_TIME_SPAN_SECOND;

  g_assert_null (f->expected);
  f->expected = event;
  f->event_target = target;
  f->delivered = 0;
  gdk_event_put (event);
  while (! f->delivered && g_get_monotonic_time () < deadline)
    { g_main_context_iteration (NULL, FALSE); g_usleep (1000); }
  g_assert_cmpuint (f->delivered, ==, 1);
  f->expected = NULL;
  f->event_target = NULL;
  gdk_event_free (event);
  gimp_test_run_mainloop_until_idle ();
}

static void
gesture_dispatch (GestureFixture *f,
                  GdkEvent       *event)
{
  gesture_dispatch_to (f, GTK_WIDGET (f->tree->view), event);
}

static void
gesture_button (GestureFixture *f,
                GdkEventType    type,
                guint32         time,
                gboolean        icon)
{
  gesture_dispatch (f, gesture_button_event (f, type, time, icon));
}

static void
gesture_assert_operations (GestureFixture *f,
                           guint           operations)
{
  guint clones = 0;

  g_assert_cmpint (gimp_image_get_n_layers (f->image), ==, 1 + operations);
  g_assert_cmpint (gimp_undo_stack_get_depth (gimp_image_get_undo_stack (f->image)),
                   ==, operations);
  for (guint i = 0; i <= operations; i++)
    {
      GimpLayer *layer = GIMP_LAYER (gimp_container_get_child_by_index (
        gimp_image_get_layers (f->image), i));
      GimpLayer *root = layer;
      guint chain = 0;
      if (layer == f->source) continue;
      g_assert_true (GIMP_IS_CLONE_LAYER (layer));
      /* Adding a layer selects it, so an uninterrupted later operation can
       * clone the previous clone. Its finite source chain must still end at
       * our original source; do not change selection between physical events. */
      while (GIMP_IS_CLONE_LAYER (root))
        {
          g_assert_cmpuint (++chain, <=, operations);
          root = gimp_clone_layer_get_source (GIMP_CLONE_LAYER (root));
          g_assert_nonnull (root);
          g_assert_true (gimp_item_get_image (GIMP_ITEM (root)) == f->image);
        }
      g_assert_true (root == f->source);
      clones++;
    }
  g_assert_cmpuint (clones, ==, operations);
}

static void
gesture_undo_all (GestureFixture *f,
                  guint           operations)
{
  for (guint i = operations; i > 0; i--)
    {
      g_assert_true (gimp_image_undo (f->image));
      gesture_assert_operations (f, i - 1);
    }
  assert_native_source (f->image, f->source, f->pixels, operations);
}

/* Capture only after verified native queued click/release and Undo assertions.
 * Resizing is evidence presentation after the interaction, never hit-test setup. */
static void
capture_native_preset_result (GestureFixture *f)
{
  const gchar *path = g_getenv ("GIMP_TEST_PRESET_SCREENSHOT");
  GimpDisplayShell *shell;
  GtkWidget *window;
  GdkWindow *native;
  GdkPixbuf *pixels;
  GError *error = NULL;
  gint old_width, old_height;
  gint64 deadline;

  if (!path) return;
  shell = gimp_display_get_shell (f->display);
  g_assert_true (gimp_painter_canvas_ui_get_visible (shell));
  gesture_assert_operations (f, 1);
  window = gtk_widget_get_toplevel (GTK_WIDGET (shell));
  gtk_window_get_size (GTK_WINDOW (window), &old_width, &old_height);
  gtk_window_resize (GTK_WINDOW (window), 1040, 1800);
  gtk_window_present (GTK_WINDOW (window));
  deadline = g_get_monotonic_time () + G_TIME_SPAN_SECOND;
  while (g_get_monotonic_time () < deadline)
    { g_main_context_iteration (NULL, FALSE); g_usleep (10000); }
  native = gtk_widget_get_window (window);
  pixels = gdk_pixbuf_get_from_window (native, 0, 0,
    gdk_window_get_width (native), gdk_window_get_height (native));
  g_assert_nonnull (pixels);
  g_assert_true (gdk_pixbuf_save (pixels, path, "png", &error, NULL));
  g_assert_no_error (error);
  g_object_unref (pixels);
  gtk_window_resize (GTK_WINDOW (window), old_width, old_height);
  gimp_test_run_mainloop_until_idle ();
}

static void
native_primary_click (void)
{
  GestureFixture f;

  gesture_fixture_init (&f);
  gesture_button (&f, GDK_BUTTON_PRESS, 1000, FALSE);
  g_assert_cmpuint (f.press_selections, ==, 1);
  gesture_assert_operations (&f, 1);
  gesture_button (&f, GDK_BUTTON_RELEASE, 1010, FALSE);
  g_assert_cmpuint (f.release_selections, ==, 1);
  gesture_assert_operations (&f, 1);
  capture_native_preset_result (&f);
  g_assert_true (gimp_image_undo (f.image));
  assert_native_source (f.image, f.source, f.pixels, 1);
  g_assert_true (gimp_image_redo (f.image));
  gesture_assert_operations (&f, 1);
  g_assert_true (gimp_image_undo (f.image));
  assert_native_source (f.image, f.source, f.pixels, 1);
  gesture_fixture_clear (&f);
}

static void
native_repeated_click (void)
{
  GestureFixture f;

  gesture_fixture_init (&f);
  gesture_button (&f, GDK_BUTTON_PRESS, 2000, FALSE);
  gesture_assert_operations (&f, 1);
  gesture_button (&f, GDK_BUTTON_RELEASE, 2010, FALSE);
  gesture_assert_operations (&f, 1);
  /* Two physically distinct queued pairs can share the same millisecond
   * timestamps. Each new press is a new gesture, not a debounce timeout. */
  gesture_button (&f, GDK_BUTTON_PRESS, 2000, FALSE);
  gesture_assert_operations (&f, 1);
  gesture_button (&f, GDK_BUTTON_RELEASE, 2010, FALSE);
  gesture_assert_operations (&f, 2);
  /* Exercise the native preview/icon path as well as name-cell selection. */
  gesture_button (&f, GDK_BUTTON_PRESS, 4000, TRUE);
  gesture_assert_operations (&f, 2);
  gesture_button (&f, GDK_BUTTON_RELEASE, 4010, TRUE);
  gesture_assert_operations (&f, 3);
  g_assert_cmpuint (f.press_selections, ==, 1);
  g_assert_cmpuint (f.release_selections, ==, 3);
  gesture_undo_all (&f, 3);
  gesture_fixture_clear (&f);
}

static void
native_double_click (void)
{
  GestureFixture f;

  gesture_fixture_init (&f);
  /* A complete GTK double click includes the second ordinary press. The
   * icon cell takes the native activation path rather than name editing. */
  gesture_button (&f, GDK_BUTTON_PRESS, 5000, TRUE);
  gesture_assert_operations (&f, 1);
  gesture_button (&f, GDK_BUTTON_RELEASE, 5010, TRUE);
  gesture_assert_operations (&f, 1);
  gesture_button (&f, GDK_BUTTON_PRESS, 5050, TRUE);
  gesture_assert_operations (&f, 1);
  gesture_button (&f, GDK_2BUTTON_PRESS, 5050, TRUE);
  g_assert_cmpuint (f.activations, ==, 1);
  gesture_assert_operations (&f, 2);
  gesture_button (&f, GDK_BUTTON_RELEASE, 5060, TRUE);
  gesture_assert_operations (&f, 2);
  g_assert_cmpuint (f.press_selections, ==, 1);
  g_assert_cmpuint (f.release_selections, ==, 2);
  gesture_undo_all (&f, 2);
  gesture_fixture_clear (&f);
}

static GtkWidget *
gesture_apply_button (GestureFixture *f)
{
  GimpActionGroup *group = gimp_action_factory_get_group (
    global_action_factory, "layer-presets", f->view);
  GimpAction *action = gimp_action_group_get_action (group, "layer-presets-apply");
  GList *buttons = gtk_container_get_children (GTK_CONTAINER (
    gimp_editor_get_button_box (GIMP_EDITOR (f->container))));
  GtkWidget *result = NULL;

  for (GList *l = buttons; l; l = l->next)
    if (GTK_IS_BUTTON (l->data) &&
        g_signal_handler_find (l->data, G_SIGNAL_MATCH_ID | G_SIGNAL_MATCH_DATA,
                               g_signal_lookup ("clicked", GTK_TYPE_BUTTON),
                               0, NULL, NULL, action))
      {
        g_assert_null (result);
        result = l->data;
      }
  g_list_free (buttons);
  g_assert_nonnull (result);
  g_assert_true (gtk_widget_is_sensitive (result));
  return result;
}

static void
native_independent_actions (void)
{
  GestureFixture f;
  GtkWidget *button;
  guint operations = 0;

  gesture_fixture_init (&f);
  gesture_button (&f, GDK_BUTTON_PRESS, 6000, FALSE);
  gesture_button (&f, GDK_BUTTON_RELEASE, 6010, FALSE);
  gesture_assert_operations (&f, ++operations);
  for (guint i = 0; i < 2; i++)
    {
      gimp_container_view_set_1_selected (f.container, f.preset);
      gesture_assert_operations (&f, ++operations);
    }
  button = gesture_apply_button (&f);
  for (guint i = 0; i < 2; i++)
    {
      gtk_button_clicked (GTK_BUTTON (button));
      gesture_assert_operations (&f, ++operations);
    }
  for (guint i = 0; i < 2; i++)
    {
      _gimp_container_view_item_activated (f.container, f.preset);
      gesture_assert_operations (&f, ++operations);
    }
  g_assert_cmpuint (f.activations, ==, 2);
  /* Independent API/Apply requests also retire an interrupted pointer press. */
  for (guint i = 0; i < 2; i++)
    {
      gesture_button (&f, GDK_BUTTON_PRESS, 6200 + i * 100, FALSE);
      gesture_assert_operations (&f, operations);
      if (i == 0)
        gimp_container_view_set_1_selected (f.container, f.preset);
      else
        gtk_button_clicked (GTK_BUTTON (button));
      gesture_assert_operations (&f, ++operations);
      gesture_button (&f, GDK_BUTTON_RELEASE, 6210 + i * 100, FALSE);
      gesture_assert_operations (&f, operations);
    }
  gesture_undo_all (&f, operations);
  gesture_fixture_clear (&f);
}

static GdkEvent *
gesture_key_event (GtkWidget    *widget,
                   GdkEventType  type,
                   guint         keyval,
                   guint32       time)
{
  GdkEvent *event = gdk_event_new (type);
  GdkDisplay *display = gtk_widget_get_display (widget);
  GdkSeat *seat = gdk_display_get_default_seat (display);
  GdkKeymapKey *keys = NULL;
  gint n_keys = 0;

  event->key.window = g_object_ref (gtk_widget_get_window (widget));
  event->key.send_event = TRUE;
  event->key.time = time;
  event->key.keyval = keyval;
  g_assert_true (gdk_keymap_get_entries_for_keyval (
    gdk_keymap_get_for_display (display), keyval, &keys, &n_keys));
  g_assert_cmpint (n_keys, >, 0);
  event->key.hardware_keycode = keys[0].keycode;
  event->key.group = keys[0].group;
  g_free (keys);
  gdk_event_set_device (event, gdk_seat_get_keyboard (seat));
  gdk_event_set_source_device (event, gdk_seat_get_keyboard (seat));
  return event;
}

static void
native_keyboard_selection (void)
{
  GestureFixture f;
  GimpViewable *other;
  GtkTreePath *first;

  gesture_fixture_init (&f);
  first = gesture_preset_path (&f);
  g_assert_cmpint (gtk_tree_path_get_depth (first), ==, 1);
  g_assert_cmpint (gtk_tree_path_get_indices (first)[0], ==, 0);
  gtk_tree_path_free (first);
  other = GIMP_VIEWABLE (gimp_container_get_child_by_name (
    gimp_data_factory_get_container (gimp->layer_preset_factory),
    "Duplicate as normal layer"));
  g_assert_nonnull (other);
  gimp_container_view_set_1_selected (f.container, other);
  g_assert_cmpint (gimp_image_get_n_layers (f.image), ==, 2);
  g_assert_true (gimp_image_undo (f.image));
  assert_native_source (f.image, f.source, f.pixels, 1);
  gtk_widget_grab_focus (GTK_WIDGET (f.tree->view));
  g_assert_true (gtk_widget_has_focus (GTK_WIDGET (f.tree->view)));
  gesture_dispatch (&f, gesture_key_event (GTK_WIDGET (f.tree->view),
                                           GDK_KEY_PRESS, GDK_KEY_Home, 7000));
  g_assert_true (gimp_container_view_get_1_selected (f.container) == f.preset);
  gesture_assert_operations (&f, 1);
  gesture_dispatch (&f, gesture_key_event (GTK_WIDGET (f.tree->view),
                                           GDK_KEY_RELEASE, GDK_KEY_Home, 7010));
  gesture_assert_operations (&f, 1);
  gesture_undo_all (&f, 1);
  gesture_fixture_clear (&f);
}

static void
native_cancelled_gestures (void)
{
  GestureFixture f;

  gesture_fixture_init (&f);
  gimp_container_view_set_1_selected (f.container, f.preset);
  gesture_undo_all (&f, 1);
  for (guint kind = 0; kind < 4; kind++)
    {
      guint32 time = 8000 + kind * 1000;
      GdkEvent *cancel;
      GdkSeat *seat = gdk_display_get_default_seat (
        gtk_widget_get_display (GTK_WIDGET (f.tree->view)));

      g_test_message ("Cancel pending selected-row gesture: %s",
                       (const gchar *[]) { "Escape", "focus-out", "unmap", "grab-broken" }[kind]);
      gtk_widget_grab_focus (GTK_WIDGET (f.tree->view));
      gesture_button (&f, GDK_BUTTON_PRESS, time, FALSE);
      gesture_assert_operations (&f, 0);
      switch (kind)
        {
        case 0:
          {
            /* GimpWindow handles Escape before forwarding to the tree: it
             * returns focus to its primary widget. Observe the real window
             * dispatch and let the resulting focus loss cancel the gesture. */
            GtkWidget *window = gtk_widget_get_toplevel (GTK_WIDGET (f.tree->view));
            gulong observer = g_signal_connect (window, "event-after",
              G_CALLBACK (gesture_event_after), &f);
            gesture_dispatch_to (&f, window, gesture_key_event (window,
              GDK_KEY_PRESS, GDK_KEY_Escape, time + 1));
            gesture_dispatch_to (&f, window, gesture_key_event (window,
              GDK_KEY_RELEASE, GDK_KEY_Escape, time + 2));
            g_signal_handler_disconnect (window, observer);
            g_assert_false (gtk_widget_has_focus (GTK_WIDGET (f.tree->view)));
          }
          break;
        case 1:
          cancel = gdk_event_new (GDK_FOCUS_CHANGE);
          cancel->focus_change.window = g_object_ref (gtk_widget_get_window (
            GTK_WIDGET (f.tree->view)));
          cancel->focus_change.send_event = TRUE;
          cancel->focus_change.in = FALSE;
          gdk_event_set_device (cancel, gdk_seat_get_keyboard (seat));
          gdk_event_set_source_device (cancel, gdk_seat_get_keyboard (seat));
          gesture_dispatch (&f, cancel);
          break;
        case 2:
          gtk_widget_hide (GTK_WIDGET (f.tree->view));
          g_assert_false (gtk_widget_get_mapped (GTK_WIDGET (f.tree->view)));
          gtk_widget_show (GTK_WIDGET (f.tree->view));
          wait_native_mapped (GTK_WIDGET (f.tree->view));
          break;
        case 3:
          cancel = gdk_event_new (GDK_GRAB_BROKEN);
          cancel->grab_broken.window = g_object_ref (
            gtk_tree_view_get_bin_window (f.tree->view));
          cancel->grab_broken.send_event = TRUE;
          cancel->grab_broken.keyboard = FALSE;
          cancel->grab_broken.implicit = FALSE;
          cancel->grab_broken.grab_window = NULL;
          gdk_event_set_device (cancel, gdk_seat_get_pointer (seat));
          gdk_event_set_source_device (cancel, gdk_seat_get_pointer (seat));
          gesture_dispatch (&f, cancel);
          break;
        }
      /* A delayed activation cannot resurrect the cancelled second press.
       * The icon route emits native item-activated even though Apply is gated. */
      gesture_button (&f, GDK_2BUTTON_PRESS, time + 3, TRUE);
      g_assert_cmpuint (f.activations, ==, kind + 1);
      gesture_assert_operations (&f, 0);
      gesture_button (&f, GDK_BUTTON_RELEASE, time + 4, FALSE);
      gesture_assert_operations (&f, 0);
      gesture_button (&f, GDK_BUTTON_PRESS, time + 100, FALSE);
      gesture_assert_operations (&f, 0);
      gesture_button (&f, GDK_BUTTON_RELEASE, time + 110, FALSE);
      gesture_assert_operations (&f, 1);
      gimp_container_view_set_1_selected (f.container, f.preset);
      gesture_assert_operations (&f, 2);
      gesture_undo_all (&f, 2);
    }
  gesture_fixture_clear (&f);
}

static void
native_context_change_cancels (void)
{
  GestureFixture f;
  GimpLayer *other_source;
  GimpImage *other;
  GimpContext *context;

  gesture_fixture_init (&f);
  gimp_container_view_set_1_selected (f.container, f.preset);
  gesture_undo_all (&f, 1);
  other = image_new (&other_source);
  select_source (other, other_source);
  gimp_image_clean_all (other);
  gimp_context_set_image (gimp->user_context, f.image);
  context = gimp_container_view_get_context (f.container);
  g_assert_true (gimp_context_get_image (context) == f.image);
  gesture_button (&f, GDK_BUTTON_PRESS, 12000, FALSE);
  gesture_assert_operations (&f, 0);
  gimp_context_set_image (context, other);
  g_assert_true (gimp_container_view_get_context (f.container) == context);
  g_assert_true (gimp_context_get_image (context) == other);
  gesture_button (&f, GDK_BUTTON_RELEASE, 12010, FALSE);
  g_assert_cmpint (gimp_image_get_n_layers (other), ==, 1);
  g_assert_cmpint (gimp_undo_stack_get_depth (gimp_image_get_undo_stack (other)), ==, 0);
  g_assert_cmpint (gimp_image_is_dirty (other), ==, 0);
  gesture_assert_operations (&f, 0);
  gimp_context_set_image (context, f.image);
  /* Returning to the same image/source cannot resurrect a pending token. */
  gesture_button (&f, GDK_BUTTON_PRESS, 12050, FALSE);
  gimp_context_set_image (context, other);
  gimp_context_set_image (context, f.image);
  gesture_button (&f, GDK_2BUTTON_PRESS, 12060, TRUE);
  gesture_button (&f, GDK_BUTTON_RELEASE, 12070, FALSE);
  gesture_assert_operations (&f, 0);
  gesture_button (&f, GDK_BUTTON_PRESS, 12100, FALSE);
  gesture_button (&f, GDK_BUTTON_RELEASE, 12110, FALSE);
  gesture_assert_operations (&f, 1);
  gesture_undo_all (&f, 1);
  g_object_unref (other);
  gesture_fixture_clear (&f);
}

static void
native_pending_close_reopen (void)
{
  GestureFixture f;
  GdkEvent *release;
  GdkWindow *old_window;

  gesture_fixture_init (&f);
  gimp_container_view_set_1_selected (f.container, f.preset);
  gesture_undo_all (&f, 1);
  gesture_button (&f, GDK_BUTTON_PRESS, 13000, FALSE);
  gesture_assert_operations (&f, 0);
  release = gesture_button_event (&f, GDK_BUTTON_RELEASE, 13010, FALSE);
  old_window = g_object_ref (release->any.window);
  /* Queue the old release, destroy the native tab, then let GTK discard the
   * event for its destroyed window. No widget is retained by this control. */
  gdk_event_put (release);
  gdk_event_free (release);
  gesture_close (&f);
  g_assert_true (gdk_window_is_destroyed (old_window));
  g_object_unref (old_window);
  assert_native_source (f.image, f.source, f.pixels, 1);
  gesture_open (&f);
  assert_native_source (f.image, f.source, f.pixels, 1);
  gesture_button (&f, GDK_BUTTON_PRESS, 13100, FALSE);
  gesture_button (&f, GDK_BUTTON_RELEASE, 13110, FALSE);
  gesture_assert_operations (&f, 1);
  gesture_undo_all (&f, 1);
  gesture_fixture_clear (&f);
}

static void
native_blank_release_cancels (void)
{
  GestureFixture f;
  GdkEvent *release;
  GtkTreePath *hit = NULL;
  GdkWindow *window;
  GimpContainer *original_container, *one_row;
  gint root_x, root_y;

  gesture_fixture_init (&f);
  /* A compact portrait dock can fit no blank pixel below the full factory.
   * Use a one-row view-only fixture to establish the blank-release condition.
   * The application factory/container is never mutated, even on assertion abort. */
  original_container = g_object_ref (gimp_container_view_get_container (f.container));
  one_row = gimp_list_new (GIMP_TYPE_LAYER_PRESET, FALSE);
  gimp_container_add (one_row, GIMP_OBJECT (f.preset));
  gimp_container_view_set_container (f.container, one_row);
  gimp_test_run_mainloop_until_idle ();
  g_assert_cmpint (gimp_container_get_n_children (one_row), ==, 1);
  gesture_assert_operations (&f, 0);
  gimp_container_view_set_1_selected (f.container, f.preset);
  gesture_undo_all (&f, 1);
  gesture_button (&f, GDK_BUTTON_PRESS, 14000, FALSE);
  release = gesture_button_event (&f, GDK_BUTTON_RELEASE, 14010, FALSE);
  window = gtk_tree_view_get_bin_window (f.tree->view);
  release->button.y = gdk_window_get_height (window) - 2;
  g_assert_false (gtk_tree_view_get_path_at_pos (f.tree->view,
    release->button.x, release->button.y, &hit, NULL, NULL, NULL));
  g_assert_null (hit);
  gdk_window_get_origin (window, &root_x, &root_y);
  release->button.x_root = root_x + release->button.x;
  release->button.y_root = root_y + release->button.y;
  gesture_dispatch (&f, release);
  gesture_assert_operations (&f, 0);
  gesture_button (&f, GDK_2BUTTON_PRESS, 14020, TRUE);
  gesture_button (&f, GDK_BUTTON_RELEASE, 14030, FALSE);
  gesture_assert_operations (&f, 0);
  gesture_button (&f, GDK_BUTTON_PRESS, 14100, FALSE);
  gesture_button (&f, GDK_BUTTON_RELEASE, 14110, FALSE);
  gesture_assert_operations (&f, 1);
  gesture_undo_all (&f, 1);
  gimp_container_view_set_container (f.container, original_container);
  g_object_unref (one_row);
  g_object_unref (original_container);
  gesture_fixture_clear (&f);
}

static void
native_stale_release (void)
{
  GestureFixture f;

  gesture_fixture_init (&f);
  gimp_container_view_set_1_selected (f.container, f.preset);
  gesture_undo_all (&f, 1);
  gesture_button (&f, GDK_BUTTON_PRESS, 15000, FALSE);
  gesture_assert_operations (&f, 0);
  /* A late event from an older click neither applies nor consumes the live
   * newer press. The subsequent matching release must still work once. */
  gesture_button (&f, GDK_BUTTON_RELEASE, 14990, FALSE);
  gesture_assert_operations (&f, 0);
  gesture_button (&f, GDK_BUTTON_RELEASE, 15010, FALSE);
  gesture_assert_operations (&f, 1);
  gesture_undo_all (&f, 1);
  gesture_fixture_clear (&f);
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
  g_object_set (gimp->config, "painter-canvas-ui", TRUE, NULL);
  g_test_add_func ("/layer-presets-ui/selection-actions-lifetime", selection_actions_lifetime);
  g_test_add_func ("/layer-presets-ui/preferences-registration", preferences_and_registration);
  g_test_add_func ("/layer-presets-ui/dialog-factory-dockable-lifetime", dialog_factory_dockable_lifetime);
  g_test_add_func ("/layer-presets-ui/action-dockable-close-reopen", action_dockable_close_reopen);
  g_test_add_func ("/layer-presets-ui/native-primary-click", native_primary_click);
  g_test_add_func ("/layer-presets-ui/native-repeated-click", native_repeated_click);
  g_test_add_func ("/layer-presets-ui/native-double-click", native_double_click);
  g_test_add_func ("/layer-presets-ui/native-independent-actions", native_independent_actions);
  g_test_add_func ("/layer-presets-ui/native-keyboard-selection", native_keyboard_selection);
  g_test_add_func ("/layer-presets-ui/native-cancelled-gestures", native_cancelled_gestures);
  g_test_add_func ("/layer-presets-ui/native-context-change-cancels", native_context_change_cancels);
  g_test_add_func ("/layer-presets-ui/native-pending-close-reopen", native_pending_close_reopen);
  g_test_add_func ("/layer-presets-ui/native-blank-release-cancels", native_blank_release_cancels);
  g_test_add_func ("/layer-presets-ui/native-stale-release", native_stale_release);
  g_test_add_func ("/layer-presets-ui/filter-storage-preferences", filter_storage_preferences);
  g_test_add_func ("/layer-presets-ui/filter-storage-reentrant-close", filter_storage_reentrant_close);
  g_application_run (gimp->app, 0, NULL);
  gint result = gimp_core_app_get_exit_status (GIMP_CORE_APP (gimp->app));
  g_application_quit (G_APPLICATION (gimp->app)); g_clear_object (&gimp->app); return result;
}
