/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Native startup acceptance: no artificial dock creation/reparenting. */
#include "config.h"
#include <gegl.h>
#include <gtk/gtk.h>
#include "libgimpbase/gimpbase.h"
#include "core/core-types.h"
#include "widgets/widgets-types.h"
#include "display/display-types.h"
#include "config/gimpguiconfig.h"
#include "core/gimp.h"
#include "core/gimpcontainer.h"
#include "core/gimpcontext.h"
#include "core/gimpdatafactory.h"
#include "core/gimpdata.h"
#include "core/gimpimage.h"
#include "core/gimptoolinfo.h"
#include "display/gimpdisplay.h"
#include "display/gimpdisplayshell.h"
#include "display/gimpimagewindow.h"
#include "display/gimppaintercanvasui.h"
#include "widgets/gimpdock.h"
#include "widgets/gimpdockbook.h"
#include "widgets/gimpdockcolumns.h"
#include "widgets/gimpdockwindow.h"
#include "widgets/gimpdialogfactory.h"
#include "widgets/gimpcontainereditor.h"
#include "widgets/gimpcontainertreeview.h"
#include "widgets/gimpcontainericonview.h"
#include "widgets/gimpcontainertreestore.h"
#include "widgets/gimpviewrenderer.h"
#include "gui/session.h"
#include "gimpcoreapp.h"
#include "gimp-app-test-utils.h"
#include "tests.h"

static Gimp *gimp;

static void
settle (gint milliseconds)
{
  gint64 end = g_get_monotonic_time () + milliseconds * G_TIME_SPAN_MILLISECOND;
  while (g_get_monotonic_time () < end)
    {
      g_main_context_iteration (NULL, FALSE);
      g_usleep (1000);
    }
}

static GtkWidget *
find_native_dockable (GimpDockColumns *columns, const gchar *identifier)
{
  for (GList *d = gimp_dock_columns_get_docks (columns); d; d = d->next)
    for (GList *b = gimp_dock_get_dockbooks (d->data); b; b = b->next)
      for (gint page = 0; page < gtk_notebook_get_n_pages (b->data); page++)
        {
          GtkWidget *dockable = gtk_notebook_get_nth_page (b->data, page);
          if (g_strcmp0 (g_object_get_data (G_OBJECT (dockable), "gimp-dialog-identifier"),
                         identifier) == 0)
            {
              gtk_notebook_set_current_page (b->data, page);
              return dockable;
            }
        }
  return NULL;
}

static void
assert_drawable (GtkWidget *widget)
{
  g_assert_nonnull (widget);
  g_assert_true (gtk_widget_get_mapped (widget));
  g_assert_true (gtk_widget_is_drawable (widget));
  g_assert_cmpint (gtk_widget_get_allocated_width (widget), >, 0);
  g_assert_cmpint (gtk_widget_get_allocated_height (widget), >, 0);
}

static void
native_session_content (void)
{
  GimpDisplayShell *shell;
  GimpImageWindow *window;
  GimpDockColumns *columns;
  GtkWidget *layers, *brushes;
  GimpContainerIconView *icons;
  GimpToolInfo *paintbrush;
  guint floating = 0;
  const gchar *screenshot = g_getenv ("GIMP_TEST_NATIVE_SESSION_SCREENSHOT");

  gimp_set_focused_once (gimp);
  settle (1000);
  g_assert_true (session_is_ready (gimp));
  g_assert_true (GIMP_GUI_CONFIG (gimp->config)->single_window_mode);
  gimp_test_utils_create_image (gimp, 256, 256);
  settle (500);
  g_assert_cmpint (g_list_length (gimp_get_display_iter (gimp)), ==, 1);
  shell = gimp_display_get_shell (gimp_get_display_iter (gimp)->data);
  window = gimp_display_shell_get_window (shell);
  columns = gimp_image_window_get_right_docks (window);
  for (GList *i = gimp_dialog_factory_get_open_dialogs (gimp_dialog_factory_get_singleton ());
       i; i = i->next)
    if (GIMP_IS_DOCK_WINDOW (i->data)) floating++;
  g_test_message ("Restored native session: right docks=%u, floating windows=%u",
                  g_list_length (gimp_dock_columns_get_docks (columns)), floating);
  /* Causal red with the unchanged generic fixture stops here. */
  g_assert_nonnull (gimp_dock_columns_get_docks (columns));
  g_assert_cmpuint (floating, ==, 0);
  g_assert_true (gimp_painter_canvas_ui_get_visible (shell));
  g_assert_true (gtk_widget_is_ancestor (GTK_WIDGET (columns), shell->canvas));

  /* Load actual shipped resources, not a generated thumbnail stand-in. */
  gimp_data_factory_data_refresh (gimp->brush_factory, gimp_get_user_context (gimp));
  paintbrush = GIMP_TOOL_INFO (gimp_container_get_child_by_name (gimp->tool_info_list,
                                                               "gimp-paintbrush-tool"));
  g_assert_nonnull (paintbrush);
  gimp_context_set_tool (gimp_get_user_context (gimp), paintbrush);
  layers = find_native_dockable (columns, "gimp-layer-list");
  brushes = find_native_dockable (columns, "gimp-brush-grid");
  g_assert_nonnull (layers);
  g_assert_nonnull (brushes);
  gtk_window_resize (GTK_WINDOW (window), 1040, 1800);
  gtk_window_present (GTK_WINDOW (window));
  settle (1000);
  assert_drawable (layers);
  assert_drawable (brushes);
  {
    GtkWidget *view = gtk_bin_get_child (GTK_BIN (layers));
    GtkTreeView *tree;
    g_assert_true (GIMP_IS_CONTAINER_TREE_VIEW (view));
    tree = GIMP_CONTAINER_TREE_VIEW (view)->view;
    assert_drawable (GTK_WIDGET (tree));
    g_assert_cmpint (gtk_tree_model_iter_n_children (gtk_tree_view_get_model (tree), NULL), >, 0);
  }
  {
    GtkWidget *editor = gtk_bin_get_child (GTK_BIN (brushes));
    GimpContainerView *view;
    g_assert_true (GIMP_IS_CONTAINER_EDITOR (editor));
    view = GIMP_CONTAINER_EDITOR (editor)->view;
    g_assert_true (GIMP_IS_CONTAINER_ICON_VIEW (view));
    icons = GIMP_CONTAINER_ICON_VIEW (view);
    assert_drawable (GTK_WIDGET (icons->view));
    g_assert_cmpint (gtk_tree_model_iter_n_children (icons->model, NULL), >, 0);
    {
      gchar *path = g_build_filename (g_getenv ("GIMP_TESTING_ABS_TOP_SRCDIR"),
                                      "gimp-data", "brushes", NULL);
      GFile *directory = g_file_new_for_path (path);
      GtkTreeIter iter;
      gboolean valid = gtk_tree_model_get_iter_first (icons->model, &iter);
      guint shipped = 0;
      while (valid)
        {
          GimpViewRenderer *renderer = NULL;
          gtk_tree_model_get (icons->model, &iter,
                              GIMP_CONTAINER_TREE_STORE_COLUMN_RENDERER, &renderer, -1);
          if (renderer && GIMP_IS_DATA (renderer->viewable))
            {
              GFile *file = gimp_data_get_file (GIMP_DATA (renderer->viewable));
              if (file && g_file_has_prefix (file, directory)) shipped++;
            }
          g_clear_object (&renderer);
          valid = gtk_tree_model_iter_next (icons->model, &iter);
        }
      /* Internal Clipboard/default placeholders cannot satisfy this check. */
      g_assert_cmpuint (shipped, >, 0);
      g_test_message ("Mapped brush grid contains %u shipped file-backed brushes", shipped);
      g_object_unref (directory);
      g_free (path);
    }
  }
  g_assert_cmpint (gtk_widget_get_allocated_width (GTK_WIDGET (window)), ==, 1040);
  g_assert_cmpint (gtk_widget_get_allocated_height (GTK_WIDGET (window)), ==, 1800);
  if (screenshot)
    {
      GdkWindow *root = gdk_get_default_root_window ();
      GdkPixbuf *pixels;
      GError *error = NULL;
      g_assert_cmpint (gdk_window_get_width (root), ==, 1080);
      g_assert_cmpint (gdk_window_get_height (root), ==, 1920);
      pixels = gdk_pixbuf_get_from_window (root, 0, 0, 1080, 1920);
      g_assert_nonnull (pixels);
      g_assert_true (gdk_pixbuf_save (pixels, screenshot, "png", &error, NULL));
      g_assert_no_error (error);
      g_object_unref (pixels);
    }
  g_object_unref (gimp_display_get_image (shell->display));
  gimp_display_close (shell->display);
  settle (100);
}

int
main (int argc, char **argv)
{
  gchar *source_brushes, *private_brushes, *search;
  gint result;
  g_test_init (&argc, &argv, NULL);
  if (!gtk_init_check (&argc, &argv)) return GIMP_EXIT_TEST_SKIPPED;
  gimp_test_utils_setup_menus_path ();
  gimp = gimp_init_for_gui_testing (TRUE);
  source_brushes = g_build_filename (g_getenv ("GIMP_TESTING_ABS_TOP_SRCDIR"),
                                     "gimp-data", "brushes", NULL);
  private_brushes = g_build_filename (gimp_directory (), "brushes", NULL);
  g_assert_cmpint (g_mkdir_with_parents (private_brushes, 0700), ==, 0);
  search = g_build_path (G_SEARCHPATH_SEPARATOR_S, source_brushes, private_brushes, NULL);
  /* Exercise the production default: no explicit Painter UI override. */
  g_assert_true (gimp->config->painter_canvas_ui);
  g_object_set (gimp->config, "brush-path", search,
                "brush-path-writable", private_brushes, NULL);
  g_free (search); g_free (source_brushes); g_free (private_brushes);
  g_test_add_func ("/native-session-ui/populated-portrait", native_session_content);
  g_application_run (gimp->app, 0, NULL);
  result = gimp_core_app_get_exit_status (GIMP_CORE_APP (gimp->app));
  g_application_quit (G_APPLICATION (gimp->app));
  g_clear_object (&gimp->app);
  return result;
}
