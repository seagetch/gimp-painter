/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The multi-window Painter toolbar is a real image-window host with an owned
 * empty shell and a borrowed action display. Each case runs in a fresh process
 * so startup/session tests exercise the real gui_init()/gimp_restore() route.
 * No test substitutes a toy toolbar, reparents a native dock GUI, or treats a
 * destroyed GtkWidget's cleared weak pointer as proof of finalization. */
#include "config.h"

#include <string.h>
#include <gegl.h>
#include <gtk/gtk.h>

#include "libgimpbase/gimpbase.h"
#include "libgimpconfig/gimpconfig.h"
#include "libgimpwidgets/gimpwidgets.h"
#include "core/core-types.h"
#include "widgets/widgets-types.h"
#include "display/display-types.h"
#include "config/gimpcoreconfig.h"
#include "core/gimp.h"
#include "core/gimp-gui.h"
#include "core/gimpcontainer.h"
#include "core/gimpcontext.h"
#include "core/gimpimage.h"
#include "core/gimpimage-color-profile.h"
#include "core/gimptoolinfo.h"
#include "core/gimptooloptions.h"
#include "display/gimpdisplay.h"
#include "display/gimpdisplay-foreach.h"
#include "display/gimpdisplayshell.h"
#include "display/gimpdisplayshell-appearance.h"
#include "display/gimpimagewindow.h"
#include "display/gimppaintercanvasui.h"
#include "file/file-open.h"
#include "gui/session.h"
#include "tools/tools-enums.h"
#include "tools/gimp-tools.h"
#include "tools/gimptransformoptions.h"
#include "tools/gimptransformgridoptions.h"
#include "widgets/gimpdialogfactory.h"
#include "widgets/gimpdockable.h"
#include "widgets/gimpdock.h"
#include "widgets/gimpdockcolumns.h"
#include "widgets/gimpdockcontainer.h"
#include "widgets/gimpdockwindow.h"
#include "widgets/gimppaintercompactoptions.h"
#include "widgets/gimptooloptionseditor.h"
#include "widgets/gimpuimanager.h"
#include "widgets/gimpactiongroup.h"
#include "widgets/gimptoggleaction.h"
#include "widgets/gimpradioaction.h"
#include "widgets/gimpwidgets-utils.h"
#include "widgets/gimpwindowstrategy.h"
#include "actions/actions-types.h"
#include "actions/actions.h"
#include "actions/view-commands.h"
#include "widgets/gimpsessioninfo.h"
#include "gimpcoreapp.h"
#include "gimp-app-test-utils.h"
#include "tests.h"

#define ENV_PAINTER "GIMP_TEST_TOOLBAR_PAINTER"
#define ENV_SINGLE  "GIMP_TEST_TOOLBAR_SINGLE"
#define ENV_SESSION "GIMP_TEST_TOOLBAR_SESSION_DIRECTORY"
#define ENV_PASS    "GIMP_TEST_TOOLBAR_SESSION_PASS"

static Gimp     *gimp;
static gboolean intercept_exit;
static guint    exit_requests;
static gboolean last_exit_force;

typedef struct
{
  void     (*run) (void);
  gboolean   painter;
  gboolean   single;
} ToolbarCase;

typedef struct
{
  gpointer object;
  guint    destroyed;
  guint    finalized;
} Lifetime;

typedef struct
{
  GimpToolInfo *tool;
  GtkWidget    *gui;
  GPtrArray    *widgets;
  GPtrArray    *parents;
} NativeTree;

static void watch            (GObject *object, Lifetime *witness);
static void assert_finalized (Lifetime *witness, gboolean widget);
static gboolean host_key_dispatch (GimpImageWindow *host,
                                    guint            keyval,
                                    GdkModifierType  state);
static void count_close_activation (GimpAction *action,
                                     GVariant   *parameter,
                                     gpointer    data);

static void
settle (void)
{
  gimp_test_run_mainloop_until_idle ();
  /* A reconcile idle can enqueue another idle behind the loop's quit source. */
  for (guint i = 0; i < 1000 && g_main_context_pending (NULL); i++)
    g_main_context_iteration (NULL, FALSE);
}

static void
gather (GtkWidget *widget,
        GPtrArray *widgets)
{
  if (g_ptr_array_find (widgets, widget, NULL))
    return;
  g_ptr_array_add (widgets, widget);
  if (GTK_IS_MENU_BUTTON (widget))
    {
      GtkPopover *popover = gtk_menu_button_get_popover (GTK_MENU_BUTTON (widget));
      if (popover)
        gather (GTK_WIDGET (popover), widgets);
    }
  if (GTK_IS_FRAME (widget))
    {
      GtkWidget *label = gtk_frame_get_label_widget (GTK_FRAME (widget));
      if (label)
        gather (label, widgets);
    }
  if (GTK_IS_CONTAINER (widget))
    {
      GList *children = gtk_container_get_children (GTK_CONTAINER (widget));
      for (GList *iter = children; iter; iter = iter->next)
        gather (iter->data, widgets);
      g_list_free (children);
    }
}

static GtkWidget *
find_widget (GtkWidget   *root,
             const gchar *name,
             GType        type)
{
  GPtrArray *widgets = g_ptr_array_new ();
  GtkWidget *result = NULL;

  gather (root, widgets);
  for (guint i = 0; i < widgets->len && ! result; i++)
    {
      GtkWidget *widget = g_ptr_array_index (widgets, i);
      if ((! type || G_TYPE_CHECK_INSTANCE_TYPE (widget, type)) &&
          (! g_strcmp0 (gtk_widget_get_name (widget), name) ||
           ! g_strcmp0 (g_object_get_data (G_OBJECT (widget),
                                          "gimp-widget-property-name"), name)))
        result = widget;
    }
  g_ptr_array_unref (widgets);
  return result;
}

static GimpToolInfo *
tool_info (const gchar *name)
{
  GimpToolInfo *tool = GIMP_TOOL_INFO
    (gimp_container_get_child_by_name (gimp->tool_info_list, name));
  g_assert_nonnull (tool);
  return tool;
}

static GimpImageWindow *
find_host (void)
{
  GList *windows = gimp_get_image_windows (gimp);
  GimpImageWindow *host = NULL;

  for (GList *iter = windows; iter; iter = iter->next)
    if (gimp_image_window_is_painter_toolbar (iter->data))
      {
        g_assert_null (host); /* Duplicates must not hide behind first-match. */
        host = iter->data;
      }
  g_list_free (windows);
  return host;
}

static GtkWidget *
host_controls (GimpImageWindow *host)
{
  GtkWidget *options = find_widget (GTK_WIDGET (host),
                                    "painter-toolbar-window-options", 0);
  GtkWidget *controls = find_widget (GTK_WIDGET (host),
                                     "painter-toolbar-window-controls", GTK_TYPE_BOX);
  g_assert_nonnull (options);
  g_assert_nonnull (controls);
  g_assert_true (gtk_widget_is_ancestor (controls, options));
  return controls;
}

static GtkWidget *
host_gui (GimpImageWindow *host,
          gboolean         supported)
{
  GtkWidget *controls = host_controls (host);
  GList *children = gtk_container_get_children (GTK_CONTAINER (controls));
  GtkWidget *gui = children ? children->data : NULL;

  g_assert_cmpuint (g_list_length (children), ==, supported ? 1 : 0);
  g_list_free (children);
  if (gui)
    g_assert_true (gtk_widget_get_visible (gui));
  return gui;
}

static void
assert_graph (guint    images,
              gboolean painter,
              gboolean single)
{
  GList *windows = gimp_get_image_windows (gimp);
  GimpImageWindow *host = find_host ();
  gboolean want_host = painter && ! single;
  guint shells = 0, real = 0, empty = 0;

  if (g_list_length (windows) != (single ? 1 : images + (want_host || ! images ? 1 : 0)))
    for (GList *iter = windows; iter; iter = iter->next)
      {
        GimpImageWindow *window = iter->data;
        gint count = gimp_image_window_get_n_shells (window);
        g_printerr ("TOOLBAR-TEST graph window=%p toolbar=%d shells=%d\n",
                    window, gimp_image_window_is_painter_toolbar (window), count);
        for (gint i = 0; i < count; i++)
          {
            GimpDisplayShell *shell = gimp_image_window_get_shell (window, i);
            g_printerr ("TOOLBAR-TEST graph shell=%p display-id=%d has-image=%d\n",
                        shell, shell->display ? gimp_display_get_id (shell->display) : -1,
                        shell->display && gimp_display_get_image (shell->display) != NULL);
          }
      }
  g_assert_cmpuint (g_list_length (windows), ==,
                    single ? 1 : images + (want_host || ! images ? 1 : 0));
  g_assert_cmpint (host != NULL, ==, want_host);
  for (GList *iter = windows; iter; iter = iter->next)
    {
      GimpImageWindow *window = iter->data;
      gint count = gimp_image_window_get_n_shells (window);
      g_assert_cmpint (count, >, 0);
      for (gint i = 0; i < count; i++)
        {
          GimpDisplayShell *shell = gimp_image_window_get_shell (window, i);
          g_assert_true (gimp_display_shell_get_window (shell) == window);
          g_assert_true (gimp_display_get_shell (shell->display) == shell);
          g_assert_true (gimp_container_have (gimp->displays, GIMP_OBJECT (shell->display)));
          shells++;
          if (gimp_display_get_image (shell->display))
            {
              real++;
              g_assert_true (window != host);
            }
          else
            empty++;
        }
    }
  g_assert_cmpuint (real, ==, images);
  g_assert_cmpuint (empty, ==, want_host || ! images ? 1 : 0);
  g_assert_cmpuint (shells, ==, gimp_container_get_n_children (gimp->displays));
  if (host)
    {
      GimpDisplayShell *placeholder = gimp_image_window_get_active_shell (host);
      GtkWidget *notebook;

      g_assert_nonnull (placeholder);
      g_assert_null (gimp_display_get_image (placeholder->display));
      g_assert_cmpint (gimp_image_window_get_n_shells (host), ==, 1);
      notebook = gtk_widget_get_ancestor (GTK_WIDGET (placeholder), GTK_TYPE_NOTEBOOK);
      g_assert_nonnull (notebook);
      g_assert_false (gtk_widget_get_visible (notebook));
      g_assert_nonnull (host_controls (host));
      {
        GimpSessionInfo *session = gimp_dialog_factory_find_session_info
          (gimp_dialog_factory_get_singleton (), "gimp-empty-image-window");
        g_assert_nonnull (session);
        g_assert_true (gimp_session_info_get_widget (session) == GTK_WIDGET (host));
      }
      g_assert_true (gtk_widget_get_visible (GTK_WIDGET (host)));
      if (! images)
        g_assert_null (gimp_image_window_get_action_display (host));
    }
  g_list_free (windows);
}

static void
set_boolean (const gchar *name,
             gboolean     value)
{
  gboolean previous;
  g_object_get (gimp->config, name, &previous, NULL);
  if (value != previous)
    {
      g_object_set (gimp->config, name, value, NULL);
    }
  settle ();
}

static GimpDisplay *
new_display (void)
{
  GimpDisplay *display;

  gimp_test_utils_create_image (gimp, 64, 64);
  settle ();
  display = gimp_context_get_display (gimp_get_user_context (gimp));
  g_assert_nonnull (display);
  g_assert_nonnull (gimp_display_get_image (display));
  return display;
}

static void
close_display (GimpDisplay *display)
{
  /* create_image() leaves its caller's image reference with the test. */
  GimpImage *image = gimp_display_get_image (display);
  gimp_display_close (display);
  g_object_unref (image);
  settle ();
}

static void
select_display (GimpDisplay *display)
{
  GimpDisplayShell *shell = gimp_display_get_shell (display);
  gimp_context_set_display (gimp_get_user_context (gimp), display);
  gimp_display_shell_present (shell);
  gtk_widget_grab_focus (shell->canvas);
  settle ();
  g_assert_true (gimp_context_get_display (gimp_get_user_context (gimp)) == display);
}

static void
assert_route (GimpImageWindow *host,
              GimpDisplay     *display)
{
  GimpDisplayShell *owned = gimp_image_window_get_active_shell (host);

  g_assert_nonnull (owned);
  g_assert_null (gimp_display_get_image (owned->display));
  g_assert_true (gimp_display_shell_get_window (owned) == host);
  g_assert_true (gimp_image_window_get_action_display (host) == display);
  g_assert_true (action_data_get_display (host) == display);
  g_assert_true (action_data_get_image (host) ==
                 (display ? gimp_display_get_image (display) : NULL));
  g_assert_true (action_data_get_shell (host) ==
                 (display ? gimp_display_get_shell (display) : NULL));
}

static void
startup_and_native_reuse (void)
{
  gboolean painter = g_strcmp0 (g_getenv (ENV_PAINTER), "1") == 0;
  gboolean single = g_strcmp0 (g_getenv (ENV_SINGLE), "1") == 0;
  GimpDisplay *startup = gimp_context_get_display (gimp_get_user_context (gimp));
  GimpImageWindow *host = find_host ();
  GimpDisplay *created;

  assert_graph (0, painter, single);
  if (host)
    {
      startup = gimp_image_window_get_active_shell (host)->display;
      assert_route (host, NULL);
    }
  g_assert_nonnull (startup);
  created = new_display ();
  assert_graph (1, painter, single);
  if (host)
    {
      g_assert_true (find_host () == host);
      g_assert_true (gimp_image_window_get_active_shell (host)->display == startup);
      g_assert_true (created != startup);
      assert_route (host, created);
    }
  else
    g_assert_true (created == startup);
  close_display (created);
  assert_graph (0, painter, single);
}

static void
append_dock_contents (GPtrArray *contents,
                      GList     *docks)
{
  for (GList *dock = docks; dock; dock = dock->next)
    {
      g_ptr_array_add (contents, dock->data);
      for (GList *book = gimp_dock_get_dockbooks (dock->data); book; book = book->next)
        {
          GtkNotebook *notebook = GTK_NOTEBOOK (book->data);

          g_ptr_array_add (contents, notebook);
          for (gint page = 0; page < gtk_notebook_get_n_pages (notebook); page++)
            g_ptr_array_add (contents, gtk_notebook_get_nth_page (notebook, page));
        }
    }
}

static GPtrArray *
dock_contents (GtkWidget *root)
{
  GPtrArray *contents = g_ptr_array_new ();

  if (GIMP_IS_DOCK_COLUMNS (root))
    append_dock_contents (contents, gimp_dock_columns_get_docks (GIMP_DOCK_COLUMNS (root)));
  else
    {
      GList *docks = gimp_dock_container_get_docks (GIMP_DOCK_CONTAINER (root));
      append_dock_contents (contents, docks);
      g_list_free (docks);
    }
  return contents;
}

static void
count_dock_root_destroy (GtkWidget *widget,
                         gpointer   data)
{
  (void) widget;
  (*(guint *) data)++;
}

static void
single_mode_painter_preserves_external_docks (void)
{
  GimpDisplay *display = new_display ();
  GimpImage *image = gimp_display_get_image (display);
  GimpDisplayShell *shell = gimp_display_get_shell (display);
  GimpImageWindow *window = gimp_display_shell_get_window (shell);
  GimpDialogFactory *factory = gimp_dialog_factory_get_singleton ();
  GPtrArray *roots = g_ptr_array_new ();
  GPtrArray **contents, **parents;
  gulong *handlers;
  guint destroyed = 0;

  assert_graph (1, FALSE, TRUE);
  select_display (display);
  g_ptr_array_add (roots, gimp_image_window_get_left_docks (window));
  g_ptr_array_add (roots, gimp_image_window_get_right_docks (window));
  for (GList *iter = gimp_dialog_factory_get_open_dialogs (factory); iter; iter = iter->next)
    if (GIMP_IS_DOCK_WINDOW (iter->data))
      g_ptr_array_add (roots, iter->data);
  /* The real session fixture must supply external docks; an empty graph
   * would not catch accidentally running a full single-window merge. */
  g_assert_cmpuint (roots->len, >, 2);
  contents = g_new0 (GPtrArray *, roots->len);
  parents = g_new0 (GPtrArray *, roots->len);
  handlers = g_new0 (gulong, roots->len);
  for (guint i = 0; i < roots->len; i++)
    {
      GtkWidget *root = g_ptr_array_index (roots, i);

      contents[i] = dock_contents (root);
      parents[i] = g_ptr_array_new ();
      if (i >= 2)
        g_assert_cmpuint (contents[i]->len, >, 0);
      for (guint j = 0; j < contents[i]->len; j++)
        g_ptr_array_add (parents[i], gtk_widget_get_parent (g_ptr_array_index (contents[i], j)));
      handlers[i] = g_signal_connect (root, "destroy", G_CALLBACK (count_dock_root_destroy), &destroyed);
    }
  for (guint step = 0; step < 6; step++)
    {
      gboolean painter = step % 2 == 0;
      guint external = 0;

      set_boolean ("painter-canvas-ui", painter);
      g_assert_cmpuint (destroyed, ==, 0);
      assert_graph (1, painter, TRUE);
      g_assert_true (gimp_display_get_image (display) == image);
      g_assert_true (gimp_display_get_shell (display) == shell);
      g_assert_true (gimp_display_shell_get_window (shell) == window);
      g_assert_true (gimp_context_get_display (gimp_get_user_context (gimp)) == display);
      g_assert_true (gimp_image_window_get_left_docks (window) == g_ptr_array_index (roots, 0));
      g_assert_true (gimp_image_window_get_right_docks (window) == g_ptr_array_index (roots, 1));
      for (GList *iter = gimp_dialog_factory_get_open_dialogs (factory); iter; iter = iter->next)
        if (GIMP_IS_DOCK_WINDOW (iter->data))
          {
            external++;
            g_assert_true (g_ptr_array_find (roots, iter->data, NULL));
          }
      g_assert_cmpuint (external, ==, roots->len - 2);
      for (guint i = 0; i < roots->len; i++)
        {
          GPtrArray *actual = dock_contents (g_ptr_array_index (roots, i));

          g_assert_cmpuint (actual->len, ==, contents[i]->len);
          for (guint j = 0; j < actual->len; j++)
            {
              GtkWidget *widget = g_ptr_array_index (actual, j);
              g_assert_true (widget == g_ptr_array_index (contents[i], j));
              g_assert_true (gtk_widget_get_parent (widget) == g_ptr_array_index (parents[i], j));
            }
          g_ptr_array_unref (actual);
        }
    }
  for (guint i = 0; i < roots->len; i++)
    {
      g_signal_handler_disconnect (g_ptr_array_index (roots, i), handlers[i]);
      g_ptr_array_unref (contents[i]);
      g_ptr_array_unref (parents[i]);
    }
  g_free (handlers);
  g_free (parents);
  g_free (contents);
  g_ptr_array_unref (roots);
  close_display (display);
}

static void
new_open_and_focus_routing (void)
{
  GimpImageWindow *host = find_host ();
  GimpDisplayShell *placeholder = gimp_image_window_get_active_shell (host);
  GimpImage *first, *second;
  GimpDisplay *a, *b;
  GFile *file;
  GError *error = NULL;
  GimpPDBStatusType status;
  gchar *path;

  assert_graph (0, TRUE, FALSE);
  first = gimp_test_utils_create_image_from_dialog (gimp);
  settle ();
  a = gimp_context_get_display (gimp_get_user_context (gimp));
  g_assert_true (gimp_display_get_image (a) == first);
  assert_graph (1, TRUE, FALSE);
  path = g_build_filename (g_getenv ("GIMP_TESTING_ABS_TOP_SRCDIR"),
                           "app", "tests", "files", "gimp-2-6-file.xcf", NULL);
  file = g_file_new_for_path (path);
  second = file_open_with_display (gimp, gimp_get_user_context (gimp), NULL,
                                   file, FALSE, NULL, &status, &error);
  g_assert_no_error (error);
  g_assert_cmpint (status, ==, GIMP_PDB_SUCCESS);
  g_assert_nonnull (second);
  settle ();
  b = gimp_context_get_display (gimp_get_user_context (gimp));
  g_assert_true (gimp_display_get_image (b) == second);
  g_assert_true (a != b);
  g_assert_true (gimp_display_shell_get_window (gimp_display_get_shell (a)) !=
                 gimp_display_shell_get_window (gimp_display_get_shell (b)));
  assert_graph (2, TRUE, FALSE);
  for (guint i = 0; i < 4; i++)
    {
      GimpDisplay *active = i % 2 ? b : a;
      GdkEvent *event;

      select_display (active);
      assert_route (host, active);
      gtk_window_present (GTK_WINDOW (host));
      event = gdk_event_new (GDK_FOCUS_CHANGE);
      event->focus_change.window = g_object_ref (gtk_widget_get_window (GTK_WIDGET (host)));
      event->focus_change.in = TRUE;
      event->focus_change.send_event = TRUE;
      gtk_widget_event (GTK_WIDGET (host), event);
      gdk_event_free (event);
      settle ();
      g_assert_true (gimp_context_get_display (gimp_get_user_context (gimp)) == active);
      g_assert_true (gimp_image_window_get_active_shell (host) == placeholder);
      assert_route (host, active);
      {
        GimpDisplayShell *shell = gimp_display_get_shell (active);
        GimpDisplayShell *other = gimp_display_get_shell (active == a ? b : a);
        gboolean previous = gimp_display_shell_get_show_guides (shell);
        gboolean other_previous = gimp_display_shell_get_show_guides (other);
        GVariant *value = g_variant_ref_sink (g_variant_new_boolean (! previous));

        /* Execute a real routed action with the host as action data. */
        view_toggle_guides_cmd_callback (NULL, value, host);
        g_variant_unref (value);
        g_assert_cmpint (gimp_display_shell_get_show_guides (shell), ==, ! previous);
        g_assert_cmpint (gimp_display_shell_get_show_guides (other), ==, other_previous);
      }
    }
  {
    GimpDisplayShell *shell = gimp_display_get_shell (a);
    GimpImageWindow *owner = gimp_display_shell_get_window (shell);
    GimpUIManager *manager = gimp_test_utils_get_ui_manager (gimp);
    GimpAction *close_action = gimp_ui_manager_find_action (manager, "view", "view-close");
    GimpAction *quit_action = gimp_ui_manager_find_action (manager, "file", "file-quit");
    gboolean original_guides = gimp_display_shell_get_show_guides (shell);
    gboolean original_bpc = gimp_image_get_simulation_bpc (first);
    guint activations = 0;
    gulong handler;
    GdkEvent *event;

    select_display (a);
    gtk_window_present (GTK_WINDOW (host));
    gtk_window_set_focus (GTK_WINDOW (host), NULL);
    settle ();
    gimp_display_shell_set_show_guides (shell, TRUE);
    gimp_image_set_simulation_bpc (first, TRUE);
    gimp_image_clean_all (first);
    g_assert_nonnull (close_action);
    g_assert_nonnull (quit_action);
    handler = g_signal_connect (close_action, "activate",
                                 G_CALLBACK (count_close_activation), &activations);
    /* A failed shortcut must produce an assertion, not stale-pointer access. */
    g_object_ref (a);
    g_object_ref (first);
    g_object_ref (owner);
    g_object_ref (shell);
    /* Native conversion transfers a live shell before attaching its new
     * owner. The host must not dispatch to that temporarily parentless shell.
     * Do not drain events or close the display inside this transfer gap. */
    gimp_image_window_remove_shell (owner, shell);
    g_assert_null (gtk_widget_get_parent (GTK_WIDGET (shell)));
    g_assert_null (gimp_display_shell_get_window (shell));
    g_assert_true (gimp_context_get_display (gimp_get_user_context (gimp)) == a);
    g_assert_true (gimp_container_have (gimp->displays, GIMP_OBJECT (a)));
    g_assert_true (gimp_display_get_image (a) == first);
    g_assert_true (gimp_display_get_shell (a) == shell);
    assert_route (host, NULL);
    /* Exercise the production host event/update path synchronously. The
     * transfer-gap barrier defers action synchronization and consumes host
     * input without mutating the still-selected image. Already-open menus
     * and direct global GAction activation during transfer are separate from
     * this window-event contract. */
    event = gdk_event_new (GDK_FOCUS_CHANGE);
    event->focus_change.window = g_object_ref (gtk_widget_get_window (GTK_WIDGET (host)));
    event->focus_change.in = TRUE;
    event->focus_change.send_event = TRUE;
    gtk_widget_event (GTK_WIDGET (host), event);
    gdk_event_free (event);
    g_assert_true (g_action_get_enabled (G_ACTION (quit_action)));
    g_assert_true (gimp_display_shell_get_show_guides (shell));
    g_assert_true (gimp_image_get_simulation_bpc (first));
    g_assert_cmpuint (activations, ==, 0);
    g_assert_true (host_key_dispatch (host, GDK_KEY_w, GDK_CONTROL_MASK));
    g_assert_cmpuint (activations, ==, 0);
    g_assert_true (gimp_display_shell_get_show_guides (shell));
    g_assert_true (gimp_image_get_simulation_bpc (first));
    g_assert_true (gimp_container_have (gimp->displays, GIMP_OBJECT (a)));
    g_assert_true (gimp_display_get_image (a) == first);
    g_assert_true (gimp_display_get_shell (a) == shell);
    gimp_image_window_add_shell (owner, shell);
    g_object_unref (shell);
    select_display (a);
    assert_graph (2, TRUE, FALSE);
    g_assert_true (find_host () == host);
    g_assert_true (gimp_image_window_get_active_shell (host) == placeholder);
    g_assert_true (gimp_display_shell_get_window (gimp_display_get_shell (a)) == owner);
    assert_route (host, a);
    g_assert_true (g_action_get_enabled (G_ACTION (close_action)));
    g_assert_true (gimp_ui_manager_activate_action (manager, "view", "view-show-guides"));
    g_assert_false (gimp_display_shell_get_show_guides (gimp_display_get_shell (a)));
    gimp_display_shell_set_show_guides (gimp_display_get_shell (a), original_guides);
    gimp_image_set_simulation_bpc (first, original_bpc);
    g_signal_handler_disconnect (close_action, handler);
    g_object_unref (owner);
    g_object_unref (first);
    g_object_unref (a);
  }
  g_object_unref (file);
  g_free (path);
  /* New/Open have production ownership conventions; process shutdown releases
   * their images. The explicit helper-created images use close_display(). */
}

static gboolean
host_key_dispatch (GimpImageWindow *host,
                   guint            keyval,
                   GdkModifierType  state)
{
  GtkWidget *widget = GTK_WIDGET (host);
  GdkKeymap *keymap = gdk_keymap_get_for_display (gtk_widget_get_display (widget));
  GdkKeymapKey *keys = NULL;
  gint n_keys = 0, chosen = 0;
  GdkEvent *event;
  gboolean handled;
  g_assert_true (gtk_widget_get_mapped (widget));
  g_assert_true (gdk_window_is_viewable (gtk_widget_get_window (widget)));
  g_assert_true (gdk_keymap_get_entries_for_keyval (keymap, keyval, &keys, &n_keys));
  g_assert_cmpint (n_keys, >, 0);
  for (gint i = 0; i < n_keys; i++)
    if (keys[i].group == 0 && keys[i].level == 0)
      {
        chosen = i;
        break;
      }
  event = gdk_event_new (GDK_KEY_PRESS);
  event->key.window = g_object_ref (gtk_widget_get_window (widget));
  event->key.send_event = TRUE;
  event->key.time = GDK_CURRENT_TIME;
  event->key.keyval = keyval;
  event->key.state = state;
  event->key.hardware_keycode = keys[chosen].keycode;
  event->key.group = keys[chosen].group;
  gdk_event_set_device (event, gdk_seat_get_keyboard
                        (gdk_display_get_default_seat (gtk_widget_get_display (widget))));
  handled = gtk_widget_event (widget, event);
  gdk_event_free (event);
  g_free (keys);
  return handled;
}

static gboolean
host_key (GimpImageWindow *host,
          guint            keyval,
          GdkModifierType  state)
{
  GtkWidget *widget = GTK_WIDGET (host);
  gint64 deadline = g_get_monotonic_time () + 2 * G_TIME_SPAN_SECOND;
  gboolean handled;

  while ((! gtk_widget_get_mapped (widget) ||
          ! gdk_window_is_viewable (gtk_widget_get_window (widget))) &&
         g_get_monotonic_time () < deadline)
    {
      g_main_context_iteration (NULL, FALSE);
      g_usleep (1000);
    }
  handled = host_key_dispatch (host, keyval, state);
  settle ();
  return handled;
}

static void
count_close_activation (GimpAction *action,
                        GVariant   *parameter,
                        gpointer    data)
{
  (void) action;
  (void) parameter;
  (*(guint *) data)++;
}

static void
assert_input_ready (GimpDisplay *a,
                    GimpDisplay *b)
{
  gint64 deadline = g_get_monotonic_time () + 2 * G_TIME_SPAN_SECOND;

  while ((! gimp_display_shell_is_drawn (gimp_display_get_shell (a)) ||
          (b && ! gimp_display_shell_is_drawn (gimp_display_get_shell (b)))) &&
         g_get_monotonic_time () < deadline)
    {
      g_main_context_iteration (NULL, FALSE);
      g_usleep (1000);
    }
  g_assert_true (gimp_display_shell_is_drawn (gimp_display_get_shell (a)));
  if (b)
    g_assert_true (gimp_display_shell_is_drawn (gimp_display_get_shell (b)));
  g_assert_true (gimp_displays_accept_focus_events (gimp));
}

static void
host_actions_and_close_accelerator (void)
{
  GimpImageWindow *host = find_host ();
  GimpDisplayShell *placeholder = gimp_image_window_get_active_shell (host);
  GimpDisplay *a = new_display (), *b = new_display ();
  GimpImage *a_image = gimp_display_get_image (a);
  GimpDisplayShell *a_shell = gimp_display_get_shell (a);
  GimpDisplayShell *b_shell = gimp_display_get_shell (b);
  GimpUIManager *manager = gimp_test_utils_get_ui_manager (gimp);
  gboolean previous, other_previous;
  guint activations = 0;
  GimpAction *close_action = gimp_ui_manager_find_action (manager, "view", "view-close");
  gulong activation_handler;

  g_assert_nonnull (close_action);
  activation_handler = g_signal_connect (close_action, "activate",
                                          G_CALLBACK (count_close_activation), &activations);
  assert_input_ready (a, b);
  select_display (a);
  gtk_window_present (GTK_WINDOW (host));
  gtk_window_set_focus (GTK_WINDOW (host), NULL);
  settle ();
  g_assert_null (gimp_window_get_primary_focus_widget (GIMP_WINDOW (host)));
  g_assert_true (gimp_context_get_display (gimp_get_user_context (gimp)) == a);
  gimp_ui_manager_update (manager, host);
  previous = gimp_display_shell_get_show_guides (a_shell);
  other_previous = gimp_display_shell_get_show_guides (b_shell);
  g_assert_true (gimp_ui_manager_activate_action (manager, "view", "view-show-guides"));
  settle ();
  g_assert_cmpint (gimp_display_shell_get_show_guides (a_shell), ==, ! previous);
  g_assert_cmpint (gimp_display_shell_get_show_guides (b_shell), ==, other_previous);
  g_assert_true (gimp_image_window_get_active_shell (host) == placeholder);

  /* Exercise the same application GAction used by a menu proxy too. */
  g_assert_true (g_action_group_has_action (G_ACTION_GROUP (gimp->app), "view-show-guides"));
  g_action_group_activate_action (G_ACTION_GROUP (gimp->app), "view-show-guides", NULL);
  settle ();
  g_assert_cmpint (gimp_display_shell_get_show_guides (a_shell), ==, previous);
  g_assert_cmpint (gimp_display_shell_get_show_guides (b_shell), ==, other_previous);

  gimp_image_clean_all (a_image);
  g_assert_true (host_key (host, GDK_KEY_w, GDK_CONTROL_MASK));
  g_assert_cmpuint (activations, ==, 1);
  g_object_unref (a_image);
  assert_graph (1, TRUE, FALSE);
  g_assert_true (find_host () == host);
  g_assert_true (gimp_image_window_get_active_shell (host) == placeholder);
  select_display (b);
  assert_input_ready (b, NULL);
  assert_route (host, b);
  gtk_window_present (GTK_WINDOW (host));
  gtk_window_set_focus (GTK_WINDOW (host), NULL);
  settle ();
  gimp_image_clean_all (gimp_display_get_image (b));
  {
    GimpImage *image = gimp_display_get_image (b);
    g_assert_true (host_key (host, GDK_KEY_w, GDK_CONTROL_MASK));
    g_assert_cmpuint (activations, ==, 2);
    g_object_unref (image);
  }
  assert_graph (0, TRUE, FALSE);
  assert_route (host, NULL);
  {
    guint previous_requests = exit_requests;
    host_key (host, GDK_KEY_w, GDK_CONTROL_MASK);
    g_assert_cmpuint (exit_requests, ==, previous_requests);
    assert_graph (0, TRUE, FALSE);
    g_assert_true (find_host () == host);
  }
  g_signal_handler_disconnect (close_action, activation_handler);
}

static void
native_snapshot (NativeTree *tree,
                 const gchar *name)
{
  tree->tool = tool_info (name);
  tree->gui = gimp_tools_get_tool_options_gui (tree->tool->tool_options);
  tree->widgets = g_ptr_array_new ();
  tree->parents = g_ptr_array_new ();
  gather (tree->gui, tree->widgets);
  for (guint i = 0; i < tree->widgets->len; i++)
    g_ptr_array_add (tree->parents,
                     gtk_widget_get_parent (g_ptr_array_index (tree->widgets, i)));
}

static void
native_assert (NativeTree *tree,
               GtkWidget  *editor)
{
  GPtrArray *current = g_ptr_array_new ();

  g_assert_true (gimp_tools_get_tool_options_gui (tree->tool->tool_options) == tree->gui);
  g_assert_true (gtk_widget_get_ancestor (tree->gui, GIMP_TYPE_TOOL_OPTIONS_EDITOR) == editor);
  g_assert_false (gimp_painter_compact_options_get (tree->gui));
  g_assert_cmpint (gtk_orientable_get_orientation (GTK_ORIENTABLE (tree->gui)),
                   ==, GTK_ORIENTATION_VERTICAL);
  gather (tree->gui, current);
  g_assert_cmpuint (current->len, ==, tree->widgets->len);
  for (guint i = 0; i < tree->widgets->len; i++)
    {
      GtkWidget *widget = g_ptr_array_index (tree->widgets, i);
      g_assert_true (g_ptr_array_find (current, widget, NULL));
      if (gtk_widget_get_parent (widget) != g_ptr_array_index (tree->parents, i))
        g_printerr ("TOOLBAR-TEST native-parent tool=%s index=%u widget=%s name=%s property=%s previous=%p current=%p current-type=%s\n",
                    gimp_object_get_name (tree->tool), i, G_OBJECT_TYPE_NAME (widget),
                    gtk_widget_get_name (widget),
                    g_object_get_data (G_OBJECT (widget), "gimp-widget-property-name") ?
                    (const gchar *) g_object_get_data (G_OBJECT (widget), "gimp-widget-property-name") : "none",
                    g_ptr_array_index (tree->parents, i), gtk_widget_get_parent (widget),
                    gtk_widget_get_parent (widget) ? G_OBJECT_TYPE_NAME (gtk_widget_get_parent (widget)) : "none");
      g_assert_true (gtk_widget_get_parent (widget) == g_ptr_array_index (tree->parents, i));
    }
  g_ptr_array_unref (current);
}

static GtkWidget *
first_radio (GtkWidget   *root,
             const gchar *property)
{
  GtkWidget *bound = find_widget (root, property, 0);
  GtkWidget *radio;

  g_assert_nonnull (bound);
  if (GTK_IS_RADIO_BUTTON (bound))
    return bound;
  radio = g_object_get_data (G_OBJECT (bound), "radio-button");
  if (! radio && GTK_IS_CONTAINER (bound))
    {
      GList *children = gtk_container_get_children (GTK_CONTAINER (bound));
      for (GList *iter = children; iter && ! radio; iter = iter->next)
        if (GTK_IS_RADIO_BUTTON (iter->data))
          radio = iter->data;
      g_list_free (children);
    }
  g_assert_true (GTK_IS_RADIO_BUTTON (radio));
  return radio;
}

static void
tool_models_and_native_ownership (void)
{
  const gchar *names[] = { "gimp-paintbrush-tool", "gimp-move-tool",
                           "gimp-rotate-tool", "gimp-perspective-guide-tool",
                           "gimp-brightness-contrast-tool" };
  NativeTree trees[G_N_ELEMENTS (names)] = { 0 };
  GimpImageWindow *host = find_host ();
  GimpContext *context = gimp_get_user_context (gimp);
  GimpDisplay *display = new_display ();
  GimpDialogFactory *factory = gimp_dialog_factory_get_singleton ();
  GtkWidget *dockable, *dock_parent, *editor;
  GtkWidget *type_box, *direction_frame;

  dockable = gimp_window_strategy_show_dockable_dialog
    (GIMP_WINDOW_STRATEGY (gimp_get_window_strategy (gimp)), gimp, factory,
     gimp_widget_get_monitor (GTK_WIDGET (host)), "gimp-tool-options");
  g_assert_true (GIMP_IS_DOCKABLE (dockable));
  dock_parent = gtk_widget_get_parent (dockable);
  editor = gtk_bin_get_child (GTK_BIN (dockable));
  g_assert_true (GIMP_IS_TOOL_OPTIONS_EDITOR (editor));
  for (guint i = 0; i < G_N_ELEMENTS (names); i++)
    {
      gimp_context_set_tool (context, tool_info (names[i]));
      /* Native Move::notify::move-type deliberately rebuilds its radio
       * labels. Normalize that fixture choice before observing ownership;
       * the measured toolbar edits below only change move-current. */
      if (i == 1)
        g_object_set (tool_info (names[i])->tool_options,
                       "move-type", GIMP_TRANSFORM_TYPE_LAYER, NULL);
      settle ();
      native_snapshot (&trees[i], names[i]);
    }
  type_box = GIMP_TRANSFORM_OPTIONS (trees[2].tool->tool_options)->type_box;
  direction_frame = GIMP_TRANSFORM_OPTIONS (trees[2].tool->tool_options)->direction_frame;
  for (guint cycle = 0; cycle < 3; cycle++)
    for (guint i = 0; i < G_N_ELEMENTS (names); i++)
      {
        GimpToolOptions *options = trees[i].tool->tool_options;
        GtkWidget *gui;

        gimp_context_set_tool (context, trees[i].tool);
        settle ();
        gui = host_gui (host, i != 4);
        g_assert_true (gtk_widget_get_parent (dockable) == dock_parent);
        g_assert_true (gtk_bin_get_child (GTK_BIN (dockable)) == editor);
        g_assert_nonnull (gimp_dockable_get_dockbook (GIMP_DOCKABLE (dockable)));
        g_assert_true (gimp_tool_options_editor_get_tool_options
                       (GIMP_TOOL_OPTIONS_EDITOR (editor)) == options);
        for (guint j = 0; j < G_N_ELEMENTS (names); j++)
          native_assert (&trees[j], editor);
        g_assert_true (GIMP_TRANSFORM_OPTIONS (trees[2].tool->tool_options)->type_box == type_box);
        g_assert_true (GIMP_TRANSFORM_OPTIONS (trees[2].tool->tool_options)->direction_frame == direction_frame);
        if (! gui)
          continue;
        g_assert_true (gui != trees[i].gui);
        g_assert_cmpint (gtk_orientable_get_orientation (GTK_ORIENTABLE (gui)),
                         ==, GTK_ORIENTATION_HORIZONTAL);
        if (i < 3)
          g_assert_true (g_object_get_data (G_OBJECT (gui), "gimp-painter-toolbar-options") == options);
        if (i == 0)
          {
            GtkWidget *spin = find_widget (gui, "brush-size", GTK_TYPE_SPIN_BUTTON);
            GtkWidget *native = find_widget (trees[i].gui, "brush-size", GTK_TYPE_SPIN_BUTTON);
            gdouble value;
            g_assert_nonnull (spin);
            g_assert_nonnull (native);
            gtk_spin_button_set_value (GTK_SPIN_BUTTON (spin), 37.0 + cycle);
            g_object_get (options, "brush-size", &value, NULL);
            g_assert_cmpfloat (value, ==, 37.0 + cycle);
            g_assert_cmpfloat (gtk_spin_button_get_value (GTK_SPIN_BUTTON (native)), ==, value);
            gtk_spin_button_set_value (GTK_SPIN_BUTTON (native), 51.0 + cycle);
            g_assert_cmpfloat (gtk_spin_button_get_value (GTK_SPIN_BUTTON (spin)), ==, 51.0 + cycle);
          }
        else if (i == 1)
          {
            gboolean current;
            GtkWidget *toggle = find_widget (gui, "move-current", GTK_TYPE_TOGGLE_BUTTON);
            g_assert_nonnull (toggle);
            g_object_set (options, "move-current", FALSE, NULL);
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle), TRUE);
            g_object_get (options, "move-current", &current, NULL);
            g_assert_true (current);
            g_object_set (options, "move-current", FALSE, NULL);
            g_assert_false (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (toggle)));
          }
        else if (i == 2)
          {
            gint direction;
            GtkWidget *radio = first_radio (gui, "direction");
            gimp_int_radio_group_set_active (GTK_RADIO_BUTTON (radio), GIMP_TRANSFORM_BACKWARD);
            g_object_get (options, "direction", &direction, NULL);
            g_assert_cmpint (direction, ==, GIMP_TRANSFORM_BACKWARD);
            g_object_set (options, "direction", GIMP_TRANSFORM_FORWARD, NULL);
            {
              GSList *group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (radio));
              guint selected = 0;
              for (GSList *iter = group; iter; iter = iter->next)
                if (gtk_toggle_button_get_active (iter->data))
                  {
                    g_assert_cmpint (GPOINTER_TO_INT (g_object_get_data
                      (G_OBJECT (iter->data), "gimp-item-data")), ==, GIMP_TRANSFORM_FORWARD);
                    selected++;
                  }
              g_assert_cmpuint (selected, ==, 1);
            }
          }
        else
          {
            GList *children = gtk_container_get_children (GTK_CONTAINER (gui));
            g_assert_null (children);
            g_assert_cmpstr (gtk_widget_get_name (gui), ==, "gimp-painter-perspective-guide-toolbar");
            g_list_free (children);
          }
      }
  for (guint i = 0; i < G_N_ELEMENTS (names); i++)
    {
      g_ptr_array_unref (trees[i].widgets);
      g_ptr_array_unref (trees[i].parents);
    }
  close_display (display);
}

typedef struct
{
  GtkWidget *controls;
  GimpDisplay *new_display;
  gboolean fired;
  Lifetime superseded;
} PublicationInterrupt;

static gboolean
interrupt_toolbar_publication (GSignalInvocationHint *hint,
                               guint                  n_values,
                               const GValue          *values,
                               gpointer               data)
{
  PublicationInterrupt *interrupt = data;
  GtkWidget *widget;
  (void) hint;
  g_assert_cmpuint (n_values, >=, 2);
  widget = g_value_get_object (&values[0]);
  if (! interrupt->fired && gtk_widget_get_parent (widget) == interrupt->controls)
    {
      interrupt->fired = TRUE;
      watch (G_OBJECT (widget), &interrupt->superseded);
      /* The GUI is parented but not yet published/shown. Invalidate the
       * reconcile revision without changing its canonical tool options. */
      gimp_context_set_display (gimp_get_user_context (gimp), interrupt->new_display);
    }
  return TRUE;
}

static void
toolbar_publication_superseded (void)
{
  GimpImageWindow *host = find_host ();
  GimpDisplay *a = new_display (), *b = new_display ();
  PublicationInterrupt interrupt = { host_controls (host), b, FALSE, { 0 } };
  guint signal = g_signal_lookup ("parent-set", GTK_TYPE_WIDGET);
  gulong hook;
  GtkWidget *gui;

  gimp_context_set_tool (gimp_get_user_context (gimp), tool_info ("gimp-move-tool"));
  select_display (a);
  g_assert_cmpuint (signal, !=, 0);
  hook = g_signal_add_emission_hook (signal, 0, interrupt_toolbar_publication,
                                    &interrupt, NULL);
  g_assert_cmpuint (hook, !=, 0);
  gimp_context_set_tool (gimp_get_user_context (gimp), tool_info ("gimp-paintbrush-tool"));
  settle ();
  g_signal_remove_emission_hook (signal, hook);
  g_assert_true (interrupt.fired);
  assert_finalized (&interrupt.superseded, TRUE);
  assert_route (host, b);
  gui = host_gui (host, TRUE);
  g_assert_true (gtk_widget_get_visible (gui));
  g_assert_true (g_object_get_data (G_OBJECT (gui), "gimp-painter-toolbar-options") ==
                 tool_info ("gimp-paintbrush-tool")->tool_options);
  g_assert_nonnull (find_widget (gui, "brush-size", GTK_TYPE_SPIN_BUTTON));
  close_display (a);
  close_display (b);
}

static void
close_active_inactive_last (void)
{
  GimpImageWindow *host = find_host ();
  GimpDisplayShell *placeholder = gimp_image_window_get_active_shell (host);
  GimpDisplay *a = new_display ();
  GimpDisplay *b = new_display ();

  select_display (a);
  close_display (b); /* An inactive close must not steal the live target. */
  assert_graph (1, TRUE, FALSE);
  assert_route (host, a);
  b = new_display ();
  select_display (b);
  close_display (b);
  assert_graph (1, TRUE, FALSE);
  /* Native close can leave context NULL until the surviving canvas focuses.
   * An action must never keep routing to the just-removed display meanwhile. */
  g_assert_true (gimp_image_window_get_action_display (host) != b);
  select_display (a);
  assert_route (host, a);
  close_display (a);
  assert_graph (0, TRUE, FALSE);
  g_assert_true (find_host () == host);
  g_assert_true (gimp_image_window_get_active_shell (host) == placeholder);
  assert_route (host, NULL);
  a = new_display ();
  g_assert_true (find_host () == host);
  assert_route (host, a);
  close_display (a);
}

static void
assert_visibility_notifications_preserve_toolbar (guint         count,
                                                  GimpDisplay **displays,
                                                  GimpImage   **images,
                                                  NativeTree   *native,
                                                  GtkWidget    *dockable,
                                                  GtkWidget    *editor)
{
  GimpImageWindow *host = find_host ();
  GimpDisplayShell *placeholder = gimp_image_window_get_active_shell (host);
  GimpDisplay *empty = placeholder->display;
  GimpDisplay *selected = gimp_context_get_display (gimp_get_user_context (gimp));
  GimpDisplay *target = gimp_image_window_get_action_display (host);
  GimpDisplayShell *shells[2];
  GimpImageWindow *owners[2];
  GtkWidget *controls = host_controls (host);
  GtkWidget *dock_parent = gtk_widget_get_parent (dockable);
  gboolean hidden;

  g_assert_cmpuint (count, <=, G_N_ELEMENTS (shells));
  g_object_get (gimp->config, "hide-docks", &hidden, NULL);
  for (guint i = 0; i < count; i++)
    {
      shells[i] = gimp_display_get_shell (displays[i]);
      owners[i] = gimp_display_shell_get_window (shells[i]);
    }

  /* Visibility changes and redundant config notifications are not mode
   * conversions. Exercise both notifications with docks hidden and shown. */
  for (guint step = 0; step < 6; step++)
    {
      if (step % 3 == 0)
        set_boolean ("hide-docks", step == 0 ? ! hidden : hidden);
      else
        {
          g_object_notify (G_OBJECT (gimp->config),
                           step % 3 == 1 ? "painter-canvas-ui" : "single-window-mode");
          settle ();
        }
      assert_graph (count, TRUE, FALSE);
      g_assert_true (find_host () == host);
      g_assert_true (gimp_image_window_get_active_shell (host) == placeholder);
      g_assert_true (placeholder->display == empty);
      g_assert_true (gimp_display_get_shell (empty) == placeholder);
      g_assert_true (host_controls (host) == controls);
      g_assert_true (gimp_context_get_display (gimp_get_user_context (gimp)) == selected);
      assert_route (host, target);
      for (guint i = 0; i < count; i++)
        {
          g_assert_true (gimp_container_have (gimp->displays, GIMP_OBJECT (displays[i])));
          g_assert_true (gimp_display_get_image (displays[i]) == images[i]);
          g_assert_true (gimp_display_get_shell (displays[i]) == shells[i]);
          g_assert_true (gimp_display_shell_get_window (shells[i]) == owners[i]);
        }
      g_assert_true (gtk_widget_get_parent (dockable) == dock_parent);
      g_assert_true (gtk_bin_get_child (GTK_BIN (dockable)) == editor);
      native_assert (native, editor);
    }
}

static void
repeat_mode_and_painter_toggles (void)
{
  GimpDisplay *displays[2] = { NULL, NULL };
  GimpImage *images[2] = { NULL, NULL };
  GimpDialogFactory *factory = gimp_dialog_factory_get_singleton ();
  NativeTree native = { 0 };
  GtkWidget *dockable, *editor;

  dockable = gimp_window_strategy_show_dockable_dialog
    (GIMP_WINDOW_STRATEGY (gimp_get_window_strategy (gimp)), gimp, factory,
     gimp_widget_get_monitor (GTK_WIDGET (find_host ())), "gimp-tool-options");
  g_assert_true (GIMP_IS_DOCKABLE (dockable));
  editor = gtk_bin_get_child (GTK_BIN (dockable));
  gimp_context_set_tool (gimp_get_user_context (gimp), tool_info ("gimp-move-tool"));
  settle ();
  native_snapshot (&native, "gimp-move-tool");

  for (guint count = 0; count <= 2; count++)
    {
      if (count)
        {
          displays[count - 1] = new_display ();
          images[count - 1] = gimp_display_get_image (displays[count - 1]);
        }
      for (guint cycle = 0; cycle < 3; cycle++)
        {
          assert_graph (count, TRUE, FALSE);
          native_assert (&native, editor);
          assert_visibility_notifications_preserve_toolbar
            (count, displays, images, &native, dockable, editor);
          set_boolean ("single-window-mode", TRUE);
          assert_graph (count, TRUE, TRUE);
          native_assert (&native, editor);
          set_boolean ("painter-canvas-ui", FALSE);
          assert_graph (count, FALSE, TRUE);
          native_assert (&native, editor);
          set_boolean ("single-window-mode", FALSE);
          assert_graph (count, FALSE, FALSE);
          native_assert (&native, editor);
          set_boolean ("painter-canvas-ui", TRUE);
          assert_graph (count, TRUE, FALSE);
          native_assert (&native, editor);
          set_boolean ("painter-canvas-ui", FALSE);
          assert_graph (count, FALSE, FALSE);
          native_assert (&native, editor);
          set_boolean ("painter-canvas-ui", TRUE);
          assert_graph (count, TRUE, FALSE);
          native_assert (&native, editor);
          for (guint i = 0; i < count; i++)
            {
              g_assert_true (gimp_container_have (gimp->displays, GIMP_OBJECT (displays[i])));
              g_assert_true (gimp_display_get_image (displays[i]) == images[i]);
              select_display (displays[i]);
              assert_route (find_host (), displays[i]);
            }
        }
    }
  close_display (displays[1]);
  close_display (displays[0]);
  assert_graph (0, TRUE, FALSE);
  g_assert_true (gimp_dialog_factory_find_widget (factory, "gimp-tool-options") == dockable);
  g_assert_true (gtk_bin_get_child (GTK_BIN (dockable)) == editor);
  native_assert (&native, editor);
  g_ptr_array_unref (native.widgets);
  g_ptr_array_unref (native.parents);
}

static gboolean
observe_exit (Gimp     *instance,
              gboolean  force,
              gpointer  data)
{
  const gchar *directory = g_getenv (ENV_SESSION);
  (void) instance;
  (void) data;
  exit_requests++;
  last_exit_force = force;
  /* tests.c redirects shutdown writes; session children must keep writing
   * into the parent's owned profile for the following fresh-process reader. */
  if (directory)
    g_setenv ("GIMP3_DIRECTORY", directory, TRUE);
  return intercept_exit;
}

static void
request_wm_close (GimpImageWindow *host)
{
  GdkEvent *event = gdk_event_new (GDK_DELETE);
  gboolean handled = FALSE;
  event->any.window = g_object_ref (gtk_widget_get_window (GTK_WIDGET (host)));
  event->any.send_event = TRUE;
  g_signal_emit_by_name (host, "delete-event", event, &handled);
  gdk_event_free (event);
  g_assert_true (handled);
  settle ();
}

static void
wm_close_requests_quit (void)
{
  GimpImageWindow *host = find_host ();
  guint previous = exit_requests;

  intercept_exit = TRUE;
  request_wm_close (host);
  g_assert_cmpuint (exit_requests, ==, previous + 1);
  g_assert_false (last_exit_force);
  g_assert_true (find_host () == host);
  assert_graph (0, TRUE, FALSE);
  intercept_exit = FALSE;
}

static void
wm_close_dirty_cancel (void)
{
  GimpImageWindow *host = find_host ();
  GimpDisplay *a = new_display ();
  GimpDisplay *b = new_display ();
  GimpImage *image = gimp_display_get_image (a);
  GtkWidget *dialog;
  guint previous = exit_requests;

  gimp_image_dirty (image, GIMP_DIRTY_IMAGE);
  select_display (a);
  request_wm_close (host);
  g_assert_cmpuint (exit_requests, ==, previous + 1);
  g_assert_false (last_exit_force);
  dialog = gimp_dialog_factory_find_widget (gimp_dialog_factory_get_singleton (),
                                            "gimp-quit-dialog");
  g_assert_true (GTK_IS_DIALOG (dialog));
  g_assert_true (gtk_widget_get_visible (dialog));
  gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL);
  settle ();
  g_assert_true (find_host () == host);
  assert_graph (2, TRUE, FALSE);
  assert_route (host, a);
  g_assert_cmpint (gimp_image_is_dirty (image), !=, 0);
  close_display (b);
  close_display (a);
}

static void
witness_destroy (GtkWidget *widget,
                 gpointer   data)
{
  (void) widget;
  ((Lifetime *) data)->destroyed++;
}

static void
witness_finalize (gpointer data)
{
  ((Lifetime *) data)->finalized++;
}

static void
watch (GObject  *object,
       Lifetime *witness)
{
  GQuark key = g_quark_from_static_string ("gimp-toolbar-window-test-finalization");
  g_assert_null (g_object_get_qdata (object, key));
  witness->object = object;
  g_object_add_weak_pointer (object, &witness->object);
  if (GTK_IS_WIDGET (object))
    g_signal_connect (object, "destroy", G_CALLBACK (witness_destroy), witness);
  /* Never cleared/replaced by the test: the notifier witnesses finalize,
   * while GtkWidget weak pointers can clear in dispose with refs outstanding. */
  g_object_set_qdata_full (object, key, witness, witness_finalize);
}

static void
assert_finalized (Lifetime *witness,
                  gboolean  widget)
{
  g_assert_null (witness->object);
  g_assert_cmpuint (witness->finalized, ==, 1);
  g_assert_cmpuint (witness->destroyed, ==, widget ? 1 : 0);
}

typedef struct
{
  gboolean fired;
  gboolean single;
  gboolean disable_painter;
  GimpDisplay *close;
} ConversionInterrupt;

static void
interrupt_parent_set (GtkWidget           *widget,
                      GtkWidget           *previous,
                      ConversionInterrupt *interrupt)
{
  if (interrupt->fired || ! previous || gtk_widget_get_parent (widget))
    return;
  interrupt->fired = TRUE;
  if (interrupt->close)
    {
      /* This callback belongs to a dock transfer. The source display still
       * owns its shell in its real image window; never delete a temporarily
       * parentless shell from inside that shell's own parent-set emission. */
      GimpImage *image = gimp_display_get_image (interrupt->close);
      gimp_display_close (interrupt->close);
      g_object_unref (image);
    }
  else
    {
      if (interrupt->disable_painter)
        g_object_set (gimp->config, "painter-canvas-ui", FALSE, NULL);
      g_object_set (gimp->config, "single-window-mode", interrupt->single, NULL);
    }
}

static void
nested_conversion_supersession (void)
{
  GimpDisplay *a = new_display (), *b = new_display ();
  GimpDisplayShell *shells[] = { gimp_display_get_shell (a), gimp_display_get_shell (b) };

  for (guint pass = 0; pass < 3; pass++)
    {
      gboolean start_single = pass == 1;
      gboolean final_painter = pass != 2;
      gboolean final_single = pass == 1;
      ConversionInterrupt interrupt = { FALSE, final_single, ! final_painter, NULL };
      gulong handlers[2];
      gboolean actual_single, actual_painter;

      set_boolean ("painter-canvas-ui", TRUE);
      set_boolean ("single-window-mode", start_single);
      select_display (a);
      for (guint i = 0; i < G_N_ELEMENTS (shells); i++)
        handlers[i] = g_signal_connect (shells[i], "parent-set",
                                        G_CALLBACK (interrupt_parent_set), &interrupt);
      set_boolean ("single-window-mode", ! start_single);
      for (guint i = 0; i < G_N_ELEMENTS (shells); i++)
        g_signal_handler_disconnect (shells[i], handlers[i]);
      g_assert_true (interrupt.fired);
      g_object_get (gimp->config, "single-window-mode", &actual_single,
                     "painter-canvas-ui", &actual_painter, NULL);
      g_assert_cmpint (actual_single, ==, final_single);
      g_assert_cmpint (actual_painter, ==, final_painter);
      assert_graph (2, final_painter, final_single);
      g_assert_true (gimp_context_get_display (gimp_get_user_context (gimp)) == a);
      g_assert_true (gimp_display_get_shell (a) == shells[0]);
      g_assert_true (gimp_display_get_shell (b) == shells[1]);
      if (find_host ())
        assert_route (find_host (), a);
    }
  close_display (b);
  close_display (a);
}

static void
source_window_closes_during_dock_transfer (void)
{
  GimpImageWindow *host = find_host ();
  GimpDisplay *a = new_display (), *b = new_display ();
  GimpImageWindow *source = gimp_display_shell_get_window (gimp_display_get_shell (a));
  GimpDialogFactory *factory = gimp_dialog_factory_get_singleton ();
  GtkWidget *dockable, *dock, *editor;
  NativeTree native = { 0 };
  Lifetime source_life = { 0 };
  ConversionInterrupt interrupt = { FALSE, FALSE, FALSE, a };
  gulong handler;

  select_display (b);
  dockable = gimp_window_strategy_show_dockable_dialog
    (GIMP_WINDOW_STRATEGY (gimp_get_window_strategy (gimp)), gimp, factory,
     gimp_widget_get_monitor (GTK_WIDGET (host)), "gimp-tool-options");
  g_assert_true (GIMP_IS_DOCKABLE (dockable));
  dock = gtk_widget_get_ancestor (dockable, GIMP_TYPE_DOCK);
  g_assert_nonnull (dock);
  editor = gtk_bin_get_child (GTK_BIN (dockable));
  gimp_context_set_tool (gimp_get_user_context (gimp), tool_info ("gimp-move-tool"));
  settle ();
  native_snapshot (&native, "gimp-move-tool");
  watch (G_OBJECT (source), &source_life);
  handler = g_signal_connect (dock, "parent-set", G_CALLBACK (interrupt_parent_set), &interrupt);
  set_boolean ("single-window-mode", TRUE);
  g_signal_handler_disconnect (dock, handler);
  g_assert_true (interrupt.fired);
  assert_graph (1, TRUE, TRUE);
  assert_finalized (&source_life, TRUE);
  g_assert_true (gimp_context_get_display (gimp_get_user_context (gimp)) == b);
  native_assert (&native, editor);
  set_boolean ("single-window-mode", FALSE);
  assert_graph (1, TRUE, FALSE);
  assert_route (find_host (), b);
  native_assert (&native, editor);
  g_ptr_array_unref (native.widgets);
  g_ptr_array_unref (native.parents);
  close_display (b);
}

static void
display_closes_during_dock_separation (void)
{
  GimpDisplay *a = new_display (), *b = new_display ();
  GimpDisplayShell *a_shell = gimp_display_get_shell (a);
  GimpDialogFactory *factory = gimp_dialog_factory_get_singleton ();
  GtkWidget *dockable, *dock, *editor;
  NativeTree native = { 0 };
  Lifetime shell_life = { 0 };
  ConversionInterrupt interrupt = { FALSE, FALSE, FALSE, a };
  gulong handler;

  set_boolean ("single-window-mode", TRUE);
  select_display (b);
  dockable = gimp_window_strategy_show_dockable_dialog
    (GIMP_WINDOW_STRATEGY (gimp_get_window_strategy (gimp)), gimp, factory,
     gimp_widget_get_monitor (GTK_WIDGET (gimp_display_get_shell (b))), "gimp-tool-options");
  g_assert_true (GIMP_IS_DOCKABLE (dockable));
  dock = gtk_widget_get_ancestor (dockable, GIMP_TYPE_DOCK);
  g_assert_nonnull (dock);
  editor = gtk_bin_get_child (GTK_BIN (dockable));
  gimp_context_set_tool (gimp_get_user_context (gimp), tool_info ("gimp-move-tool"));
  settle ();
  native_snapshot (&native, "gimp-move-tool");
  watch (G_OBJECT (a_shell), &shell_life);
  handler = g_signal_connect (dock, "parent-set", G_CALLBACK (interrupt_parent_set), &interrupt);
  set_boolean ("single-window-mode", FALSE);
  g_signal_handler_disconnect (dock, handler);
  g_assert_true (interrupt.fired);
  assert_graph (1, TRUE, FALSE);
  assert_finalized (&shell_life, TRUE);
  g_assert_true (gimp_context_get_display (gimp_get_user_context (gimp)) == b);
  assert_route (find_host (), b);
  native_assert (&native, editor);
  g_ptr_array_unref (native.widgets);
  g_ptr_array_unref (native.parents);
  close_display (b);
}

static void
assert_menu_model_identity (GimpImageWindow *window,
                            GtkWidget       *menubar,
                            GimpMenuModel   *model)
{
  GObject *actual = NULL;

  g_assert_true (gimp_image_window_get_menubar_model (window) == model);
  g_object_get (menubar, "model", &actual, NULL);
  g_assert_true ((gpointer) actual == model);
  g_object_unref (actual);
}

static void
scroll_reveal_last_menu (GtkWidget *scroll,
                         GtkWidget *menubar)
{
  GtkWidget *viewport = gtk_bin_get_child (GTK_BIN (scroll));
  GtkWidget *last = NULL, *submenu;
  GtkAdjustment *adjustment = gtk_scrolled_window_get_hadjustment (GTK_SCROLLED_WINDOW (scroll));
  GList *items = gtk_container_get_children (GTK_CONTAINER (menubar));
  GdkEvent *event;
  gint x, y, minimum, natural;
  gint64 deadline;

  g_assert_true (GTK_IS_VIEWPORT (viewport));
  g_assert_true (gtk_bin_get_child (GTK_BIN (viewport)) == menubar);
  for (GList *iter = items; iter; iter = iter->next)
    if (GTK_IS_MENU_ITEM (iter->data) && gtk_widget_get_visible (iter->data))
      last = iter->data;
  g_list_free (items);
  g_assert_nonnull (last);
  g_assert_cmpfloat (gtk_adjustment_get_page_size (adjustment), >, 1);
  gtk_widget_get_preferred_width (menubar, &minimum, &natural);
  if (minimum > gtk_adjustment_get_page_size (adjustment))
    g_assert_cmpfloat (gtk_adjustment_get_upper (adjustment) -
                       gtk_adjustment_get_lower (adjustment), >,
                       gtk_adjustment_get_page_size (adjustment));
  gtk_adjustment_set_value (adjustment, gtk_adjustment_get_upper (adjustment) -
                                       gtk_adjustment_get_page_size (adjustment));
  deadline = g_get_monotonic_time () + 2 * G_TIME_SPAN_SECOND;
  do
    {
      g_main_context_iteration (NULL, FALSE);
      g_assert_true (gtk_widget_translate_coordinates (last, viewport, 0, 0, &x, &y));
      if (x >= 0 && x + gtk_widget_get_allocated_width (last) <=
                    gtk_adjustment_get_page_size (adjustment))
        break;
      g_usleep (1000);
    }
  while (g_get_monotonic_time () < deadline);
  g_assert_true (gtk_widget_get_mapped (last));
  g_assert_cmpint (gtk_widget_get_allocated_width (last), >, 1);
  g_assert_cmpint (x, >=, 0);
  g_assert_cmpfloat (x + gtk_widget_get_allocated_width (last), <=,
                     gtk_adjustment_get_page_size (adjustment));

  /* Open the native item's actual submenu at the revealed anchor, with an
   * explicit pointer event rather than a toy menu or an eventless popup. */
  submenu = gtk_menu_item_get_submenu (GTK_MENU_ITEM (last));
  g_assert_true (GTK_IS_MENU (submenu));
  event = gdk_event_new (GDK_BUTTON_PRESS);
  event->button.window = g_object_ref (gtk_widget_get_window (last));
  event->button.send_event = TRUE;
  event->button.time = GDK_CURRENT_TIME;
  event->button.button = 1;
  gdk_event_set_device (event, gdk_seat_get_pointer
                        (gdk_display_get_default_seat (gtk_widget_get_display (last))));
  gtk_menu_popup_at_widget (GTK_MENU (submenu), last, GDK_GRAVITY_SOUTH_WEST,
                            GDK_GRAVITY_NORTH_WEST, event);
  gdk_event_free (event);
  deadline = g_get_monotonic_time () + 2 * G_TIME_SPAN_SECOND;
  while (! gtk_widget_get_mapped (submenu) && g_get_monotonic_time () < deadline)
    {
      g_main_context_iteration (NULL, FALSE);
      g_usleep (1000);
    }
  g_assert_true (gtk_widget_get_visible (submenu));
  g_assert_true (gtk_widget_get_mapped (submenu));
  gtk_menu_popdown (GTK_MENU (submenu));
  gtk_menu_shell_deactivate (GTK_MENU_SHELL (menubar));
  settle ();
  g_assert_false (gtk_widget_get_visible (submenu));
}

static void
portrait_toolbar_scroll_reaches_end (void)
{
  GimpContext *context = gimp_get_user_context (gimp);
  GimpDisplay *display = new_display ();
  GimpImageWindow *host = find_host ();
  GimpToolOptions *options = tool_info ("gimp-paintbrush-tool")->tool_options;
  GimpDialogFactory *factory = gimp_dialog_factory_get_singleton ();
  GtkWidget *dockable, *dock_parent, *editor, *gui, *scroll, *viewport;
  GtkWidget *toggle, *native_toggle, *end;
  GtkWidget *menu_scroll, *menubar;
  GimpMenuModel *menu_model = gimp_image_window_get_menubar_model (host);
  GtkAdjustment *adjustment;
  GtkPolicyType horizontal, vertical;
  GdkRectangle workarea;
  NativeTree native = { 0 };
  Lifetime old_gui = { 0 }, next_gui = { 0 }, scroll_life = { 0 };
  Lifetime viewport_life = { 0 }, host_life = { 0 };
  gboolean original, value;
  gint bound, x, y;
  gint64 deadline;

  dockable = gimp_window_strategy_show_dockable_dialog
    (GIMP_WINDOW_STRATEGY (gimp_get_window_strategy (gimp)), gimp, factory,
     gimp_widget_get_monitor (GTK_WIDGET (host)), "gimp-tool-options");
  g_assert_true (GIMP_IS_DOCKABLE (dockable));
  dock_parent = gtk_widget_get_parent (dockable);
  editor = gtk_bin_get_child (GTK_BIN (dockable));
  gimp_context_set_tool (context, tool_info ("gimp-paintbrush-tool"));
  settle ();
  native_snapshot (&native, "gimp-paintbrush-tool");
  gui = host_gui (host, TRUE);
  menu_scroll = find_widget (GTK_WIDGET (host), "painter-toolbar-window-menu-scroll",
                             GTK_TYPE_SCROLLED_WINDOW);
  g_assert_nonnull (menu_scroll);
  menubar = find_widget (menu_scroll, NULL, GTK_TYPE_MENU_BAR);
  g_assert_nonnull (menubar);
  assert_menu_model_identity (host, menubar, menu_model);
  scroll = find_widget (GTK_WIDGET (host), "painter-toolbar-window-scroll",
                        GTK_TYPE_SCROLLED_WINDOW);
  g_assert_nonnull (scroll);
  viewport = gtk_bin_get_child (GTK_BIN (scroll));
  g_assert_true (GTK_IS_VIEWPORT (viewport));
  g_assert_true (gtk_bin_get_child (GTK_BIN (viewport)) == host_controls (host));
  g_assert_true (gtk_widget_get_ancestor (gui, GTK_TYPE_SCROLLED_WINDOW) == scroll);
  gtk_scrolled_window_get_policy (GTK_SCROLLED_WINDOW (scroll), &horizontal, &vertical);
  g_assert_cmpint (horizontal, ==, GTK_POLICY_AUTOMATIC);
  g_assert_cmpint (vertical, ==, GTK_POLICY_NEVER);
  toggle = find_widget (gui, "expand-use", GTK_TYPE_TOGGLE_BUTTON);
  native_toggle = find_widget (native.gui, "expand-use", GTK_TYPE_TOGGLE_BUTTON);
  end = find_widget (gui, "painter-paint-expand", GTK_TYPE_MENU_BUTTON);
  g_assert_nonnull (toggle);
  g_assert_nonnull (native_toggle);
  g_assert_nonnull (end);
  adjustment = gtk_scrolled_window_get_hadjustment (GTK_SCROLLED_WINDOW (scroll));
  gdk_monitor_get_workarea (gimp_widget_get_monitor (GTK_WIDGET (host)), &workarea);
  bound = MIN (1080, workarea.width);
  g_assert_cmpint (bound, >, 0);
  gtk_window_resize (GTK_WINDOW (host), bound, 160);
  gtk_window_present (GTK_WINDOW (host));
  settle ();
  deadline = g_get_monotonic_time () + 2 * G_TIME_SPAN_SECOND;
  while ((gtk_widget_get_allocated_width (GTK_WIDGET (host)) > bound ||
          ! gtk_widget_get_mapped (end) || gtk_widget_get_allocated_width (end) <= 1 ||
          gtk_adjustment_get_page_size (adjustment) <= 1) &&
         g_get_monotonic_time () < deadline)
    {
      g_main_context_iteration (NULL, FALSE);
      g_usleep (1000);
    }
  g_assert_cmpint (gtk_widget_get_allocated_width (GTK_WIDGET (host)), <=, bound);
  g_assert_true (gtk_widget_get_mapped (end));
  g_assert_cmpint (gtk_widget_get_allocated_width (end), >, 1);
  g_assert_cmpfloat (gtk_adjustment_get_page_size (adjustment), >, 1);
  g_assert_cmpfloat (gtk_adjustment_get_upper (adjustment) -
                     gtk_adjustment_get_lower (adjustment), >,
                     gtk_adjustment_get_page_size (adjustment));
  gtk_adjustment_set_value (adjustment, gtk_adjustment_get_lower (adjustment));
  settle ();
  g_assert_true (gtk_widget_translate_coordinates (end, viewport, 0, 0, &x, &y));
  g_assert_cmpfloat (x + gtk_widget_get_allocated_width (end), >,
                     gtk_adjustment_get_page_size (adjustment));

  /* Reach the actual last Paintbrush controls through GTK's own adjustment,
   * rather than resizing the window beyond the portrait workarea. */
  gtk_adjustment_set_value (adjustment, gtk_adjustment_get_upper (adjustment) -
                                       gtk_adjustment_get_page_size (adjustment));
  deadline = g_get_monotonic_time () + 2 * G_TIME_SPAN_SECOND;
  do
    {
      g_main_context_iteration (NULL, FALSE);
      g_assert_true (gtk_widget_translate_coordinates (end, viewport, 0, 0, &x, &y));
      if (x >= 0 && x + gtk_widget_get_allocated_width (end) <=
                    gtk_adjustment_get_page_size (adjustment))
        break;
      g_usleep (1000);
    }
  while (g_get_monotonic_time () < deadline);
  g_assert_cmpint (gtk_widget_get_allocated_width (GTK_WIDGET (host)), <=, bound);
  for (guint i = 0; i < 2; i++)
    {
      GtkWidget *widget = i ? end : toggle;
      g_assert_true (gtk_widget_get_mapped (widget));
      g_assert_true (gtk_widget_translate_coordinates (widget, viewport, 0, 0, &x, &y));
      g_assert_cmpint (x, >=, 0);
      g_assert_cmpfloat (x + gtk_widget_get_allocated_width (widget), <=,
                         gtk_adjustment_get_page_size (adjustment));
    }
  scroll_reveal_last_menu (menu_scroll, menubar);
  g_assert_true (find_widget (GTK_WIDGET (host), NULL, GTK_TYPE_MENU_BAR) == menubar);
  assert_menu_model_identity (host, menubar, menu_model);
  g_assert_cmpint (gtk_widget_get_allocated_width (GTK_WIDGET (host)), <=, bound);
  g_object_get (options, "expand-use", &original, NULL);
  gtk_button_clicked (GTK_BUTTON (toggle));
  g_object_get (options, "expand-use", &value, NULL);
  g_assert_cmpint (value, ==, ! original);
  g_assert_cmpint (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (native_toggle)), ==, value);
  gtk_button_clicked (GTK_BUTTON (toggle));
  g_object_get (options, "expand-use", &value, NULL);
  g_assert_cmpint (value, ==, original);
  native_assert (&native, editor);
  watch (G_OBJECT (gui), &old_gui);
  watch (G_OBJECT (scroll), &scroll_life);
  watch (G_OBJECT (viewport), &viewport_life);
  watch (G_OBJECT (host), &host_life);
  gimp_context_set_tool (context, tool_info ("gimp-move-tool"));
  settle ();
  assert_finalized (&old_gui, TRUE);
  g_assert_cmpuint (scroll_life.destroyed, ==, 0);
  g_assert_cmpuint (scroll_life.finalized, ==, 0);
  g_assert_cmpuint (viewport_life.destroyed, ==, 0);
  g_assert_cmpuint (viewport_life.finalized, ==, 0);
  g_assert_true (find_widget (GTK_WIDGET (host), "painter-toolbar-window-scroll",
                             GTK_TYPE_SCROLLED_WINDOW) == scroll);
  g_assert_true (gtk_bin_get_child (GTK_BIN (scroll)) == viewport);
  watch (G_OBJECT (host_gui (host, TRUE)), &next_gui);
  set_boolean ("painter-canvas-ui", FALSE);
  assert_graph (1, FALSE, FALSE);
  assert_finalized (&next_gui, TRUE);
  assert_finalized (&scroll_life, TRUE);
  assert_finalized (&viewport_life, TRUE);
  assert_finalized (&host_life, TRUE);
  g_assert_true (gimp_dialog_factory_find_widget (factory, "gimp-tool-options") == dockable);
  g_assert_true (gtk_widget_get_parent (dockable) == dock_parent);
  g_assert_true (gtk_bin_get_child (GTK_BIN (dockable)) == editor);
  native_assert (&native, editor);
  g_ptr_array_unref (native.widgets);
  g_ptr_array_unref (native.parents);
  close_display (display);
}

static void
dispatch_fullscreen_state (GimpImageWindow *window,
                           gboolean         fullscreen)
{
  GdkEvent *event = gdk_event_new (GDK_WINDOW_STATE);

  /* Deliver the production WM event path without requiring a WM in Xvfb. */
  event->window_state.window = g_object_ref (gtk_widget_get_window (GTK_WIDGET (window)));
  event->window_state.send_event = TRUE;
  event->window_state.changed_mask = GDK_WINDOW_STATE_FULLSCREEN;
  event->window_state.new_window_state = fullscreen ? GDK_WINDOW_STATE_FULLSCREEN : 0;
  gtk_widget_event (GTK_WIDGET (window), event);
  gdk_event_free (event);
  g_assert_cmpint (gimp_image_window_get_fullscreen (window), ==, fullscreen);
}

static void
deliver_fullscreen_state (GimpImageWindow *window,
                          gboolean         fullscreen)
{
  dispatch_fullscreen_state (window, fullscreen);
  settle ();
  g_assert_cmpint (gimp_image_window_get_fullscreen (window), ==, fullscreen);
}

typedef struct
{
  GimpImageWindow *window;
  GtkWidget       *viewport;
  gboolean         fired;
} MenuWrapInterrupt;

static void
interrupt_first_menu_wrap (GtkWidget *menu,
                            GtkWidget *previous,
                            gpointer   data)
{
  MenuWrapInterrupt *interrupt = data;
  GtkWidget *parent = gtk_widget_get_parent (menu);
  (void) previous;

  if (! interrupt->fired && GTK_IS_VIEWPORT (parent))
    {
      /* GtkScrolledWindow can add the menu to its automatic viewport before
       * parenting that viewport, so only the exact menu is scoped here. */
      interrupt->fired = TRUE;
      interrupt->viewport = parent;
      /* Interrupt first publication without draining a partially built tree.
       * The fullscreen handler must move the row, never steal its bare menu. */
      dispatch_fullscreen_state (interrupt->window, TRUE);
      g_assert_true (gtk_widget_get_parent (menu) == parent);
      /* GtkBin publishes its child slot after parent-set returns. The exact
       * slot identity is checked after the outer promotion completes. */
    }
}

static void
scroll_menu_before_demotion (GimpImageWindow *window,
                             GtkWidget       *scroll)
{
  GtkAdjustment *adjustment = gtk_scrolled_window_get_hadjustment (GTK_SCROLLED_WINDOW (scroll));
  GdkRectangle workarea;
  gint64 deadline = g_get_monotonic_time () + 2 * G_TIME_SPAN_SECOND;

  gdk_monitor_get_workarea (gimp_widget_get_monitor (GTK_WIDGET (window)), &workarea);
  gtk_window_resize (GTK_WINDOW (window), MIN (400, workarea.width), 160);
  do
    {
      g_main_context_iteration (NULL, FALSE);
      if (gtk_adjustment_get_page_size (adjustment) > 1 &&
          gtk_adjustment_get_upper (adjustment) - gtk_adjustment_get_lower (adjustment) >
          gtk_adjustment_get_page_size (adjustment))
        break;
      g_usleep (1000);
    }
  while (g_get_monotonic_time () < deadline);
  g_assert_cmpfloat (gtk_adjustment_get_page_size (adjustment), >, 1);
  g_assert_cmpfloat (gtk_adjustment_get_upper (adjustment) -
                     gtk_adjustment_get_lower (adjustment), >,
                     gtk_adjustment_get_page_size (adjustment));
  gtk_adjustment_set_value (adjustment, gtk_adjustment_get_upper (adjustment) -
                                       gtk_adjustment_get_page_size (adjustment));
  settle ();
  g_assert_cmpfloat (gtk_adjustment_get_value (adjustment), >,
                     gtk_adjustment_get_lower (adjustment));
}

static void
assert_native_menu_row (GtkWidget *scroll,
                        GtkWidget *menubar)
{
  GtkWidget *viewport = gtk_bin_get_child (GTK_BIN (scroll));
  GtkAdjustment *adjustment = gtk_scrolled_window_get_hadjustment (GTK_SCROLLED_WINDOW (scroll));
  GList *items;
  gint minimum, natural, x, y;
  gint64 deadline = g_get_monotonic_time () + 2 * G_TIME_SPAN_SECOND;

  gtk_widget_get_preferred_width (menubar, &minimum, &natural);
  do
    {
      g_main_context_iteration (NULL, FALSE);
      if (gtk_adjustment_get_page_size (adjustment) >= minimum &&
          gtk_adjustment_get_value (adjustment) == gtk_adjustment_get_lower (adjustment))
        break;
      g_usleep (1000);
    }
  while (g_get_monotonic_time () < deadline);
  g_assert_cmpfloat (gtk_adjustment_get_value (adjustment), ==,
                     gtk_adjustment_get_lower (adjustment));
  g_assert_cmpfloat (gtk_adjustment_get_page_size (adjustment), >=, minimum);
  g_assert_cmpint (gtk_widget_get_allocated_width (viewport), >=, minimum);
  g_assert_cmpint (gtk_widget_get_allocated_width (menubar), >=, minimum);
  items = gtk_container_get_children (GTK_CONTAINER (menubar));
  for (GList *iter = items; iter; iter = iter->next)
    if (GTK_IS_MENU_ITEM (iter->data) && gtk_widget_get_visible (iter->data))
      {
        GtkWidget *item = iter->data;

        g_assert_true (gtk_widget_get_mapped (item));
        g_assert_true (gtk_widget_translate_coordinates (item, viewport, 0, 0, &x, &y));
        g_assert_cmpint (x, >=, 0);
        g_assert_cmpfloat (x + gtk_widget_get_allocated_width (item), <=,
                           gtk_adjustment_get_page_size (adjustment));
      }
  g_list_free (items);
}

static void
menu_scroll_role_and_titlebar_lifecycle (void)
{
  GimpDisplay *display = new_display (), *custom_display;
  GimpImageWindow *host;
  GimpDisplayShell *placeholder;
  GimpMenuModel *model;
  GtkWidget *headerbar, *scroll, *viewport, *menubar, *main_vbox;
  GtkPolicyType horizontal, vertical;
  MenuWrapInterrupt interrupt = { 0 };
  gulong handler;
  Lifetime scroll_life = { 0 }, viewport_life = { 0 }, menu_life = { 0 }, host_life = { 0 };
  Lifetime *survivors[] = { &scroll_life, &viewport_life, &menu_life, &host_life };

  /* Construct a never-host custom-titlebar window through the native path,
   * then leave that same realized window empty before its first promotion. */
  set_boolean ("custom-title-bar", TRUE);
  custom_display = new_display ();
  host = gimp_display_shell_get_window (gimp_display_get_shell (custom_display));
  headerbar = gtk_window_get_titlebar (GTK_WINDOW (host));
  g_assert_true (GTK_IS_HEADER_BAR (headerbar));
  menubar = find_widget (headerbar, NULL, GTK_TYPE_MENU_BAR);
  g_assert_nonnull (menubar);
  g_assert_null (find_widget (headerbar, "painter-toolbar-window-menu-scroll",
                              GTK_TYPE_SCROLLED_WINDOW));
  model = gimp_image_window_get_menubar_model (host);
  main_vbox = gtk_bin_get_child (GTK_BIN (host));
  close_display (display);
  close_display (custom_display);
  assert_graph (0, FALSE, FALSE);
  placeholder = gimp_image_window_get_active_shell (host);
  g_assert_nonnull (placeholder);
  g_assert_true (gtk_widget_get_parent (menubar) == headerbar);
  interrupt.window = host;
  handler = g_signal_connect (menubar, "parent-set",
                              G_CALLBACK (interrupt_first_menu_wrap), &interrupt);
  set_boolean ("painter-canvas-ui", TRUE);
  g_signal_handler_disconnect (menubar, handler);
  g_assert_true (interrupt.fired);
  g_assert_true (find_host () == host);
  g_assert_true (gimp_image_window_get_fullscreen (host));
  scroll = find_widget (main_vbox, "painter-toolbar-window-menu-scroll",
                        GTK_TYPE_SCROLLED_WINDOW);
  g_assert_nonnull (scroll);
  viewport = gtk_bin_get_child (GTK_BIN (scroll));
  g_assert_true (viewport == interrupt.viewport);
  g_assert_true (gtk_bin_get_child (GTK_BIN (viewport)) == menubar);
  g_assert_true (gtk_widget_get_parent (scroll) == main_vbox);
  assert_menu_model_identity (host, menubar, model);
  deliver_fullscreen_state (host, FALSE);
  g_assert_true (gtk_widget_get_parent (scroll) == headerbar);
  assert_graph (0, TRUE, FALSE);
  watch (G_OBJECT (scroll), &scroll_life);
  watch (G_OBJECT (viewport), &viewport_life);
  watch (G_OBJECT (menubar), &menu_life);
  watch (G_OBJECT (host), &host_life);

  for (guint step = 0; step < 6; step++)
    {
      gboolean painter = step % 2 != 0;

      if (! painter)
        scroll_menu_before_demotion (host, scroll);
      set_boolean ("painter-canvas-ui", painter);
      assert_graph (0, painter, FALSE);
      g_assert_cmpint (gimp_image_window_is_painter_toolbar (host), ==, painter);
      g_assert_true (gimp_image_window_get_active_shell (host) == placeholder);
      g_assert_true (gtk_widget_get_parent (scroll) == headerbar);
      g_assert_true (gtk_bin_get_child (GTK_BIN (scroll)) == viewport);
      g_assert_true (gtk_bin_get_child (GTK_BIN (viewport)) == menubar);
      assert_menu_model_identity (host, menubar, model);
      gtk_scrolled_window_get_policy (GTK_SCROLLED_WINDOW (scroll), &horizontal, &vertical);
      g_assert_cmpint (horizontal, ==, painter ? GTK_POLICY_AUTOMATIC : GTK_POLICY_NEVER);
      g_assert_cmpint (vertical, ==, GTK_POLICY_NEVER);
      if (! painter)
        assert_native_menu_row (scroll, menubar);
      gimp_image_window_set_show_menubar (host, FALSE);
      g_assert_false (gtk_widget_get_visible (scroll));
      g_assert_false (gtk_widget_get_visible (menubar));
      gimp_image_window_set_show_menubar (host, TRUE);
      g_assert_true (gtk_widget_get_visible (scroll));
      g_assert_true (gtk_widget_get_visible (menubar));
      deliver_fullscreen_state (host, TRUE);
      g_assert_true (gtk_widget_get_parent (scroll) == main_vbox);
      g_assert_true (gtk_bin_get_child (GTK_BIN (viewport)) == menubar);
      assert_menu_model_identity (host, menubar, model);
      deliver_fullscreen_state (host, FALSE);
      g_assert_true (gtk_widget_get_parent (scroll) == headerbar);
      g_assert_true (gtk_bin_get_child (GTK_BIN (viewport)) == menubar);
      assert_menu_model_identity (host, menubar, model);
      for (guint i = 0; i < G_N_ELEMENTS (survivors); i++)
        {
          g_assert_nonnull (survivors[i]->object);
          g_assert_cmpuint (survivors[i]->destroyed, ==, 0);
          g_assert_cmpuint (survivors[i]->finalized, ==, 0);
        }
    }
  display = new_display ();
  set_boolean ("painter-canvas-ui", FALSE);
  assert_graph (1, FALSE, FALSE);
  assert_finalized (&scroll_life, TRUE);
  assert_finalized (&viewport_life, TRUE);
  assert_finalized (&menu_life, TRUE);
  assert_finalized (&host_life, TRUE);
  close_display (display);
}

static void
retained_destroyed_pending_updates (void)
{
  GimpContext *context = gimp_get_user_context (gimp);
  GimpDisplay *image_display = new_display ();
  GimpImageWindow *host = find_host ();
  GimpDisplayShell *placeholder = gimp_image_window_get_active_shell (host);
  GimpDisplay *empty = placeholder->display;
  GtkWidget *gui, *controls = host_controls (host);
  Lifetime host_life = { 0 }, shell_life = { 0 }, display_life = { 0 };
  Lifetime gui_life = { 0 }, controls_life = { 0 };

  gimp_context_set_tool (context, tool_info ("gimp-move-tool"));
  settle ();
  gui = host_gui (host, TRUE);
  watch (G_OBJECT (host), &host_life);
  watch (G_OBJECT (placeholder), &shell_life);
  watch (G_OBJECT (empty), &display_life);
  watch (G_OBJECT (gui), &gui_life);
  watch (G_OBJECT (controls), &controls_life);
  g_object_ref (host);
  g_object_ref (placeholder);
  g_object_ref (empty);
  g_object_ref (gui);

  /* Queue/replace tool work and remove its owner without first draining it. */
  gimp_context_set_tool (context, tool_info ("gimp-paintbrush-tool"));
  gimp_context_set_tool (context, tool_info ("gimp-rotate-tool"));
  g_object_set (gimp->config, "painter-canvas-ui", FALSE, NULL);
  settle ();
  assert_graph (1, FALSE, FALSE);
  g_assert_cmpuint (host_life.destroyed, ==, 1);
  g_assert_cmpuint (shell_life.destroyed, ==, 1);
  g_assert_cmpuint (gui_life.destroyed, ==, 1);
  g_assert_cmpuint (host_life.finalized, ==, 0);
  g_assert_cmpuint (shell_life.finalized, ==, 0);
  g_assert_cmpuint (display_life.finalized, ==, 0);
  g_assert_cmpuint (gui_life.finalized, ==, 0);
  assert_finalized (&controls_life, TRUE);

  /* Retained destroyed emitters/owners must not reconnect on later updates. */
  gimp_context_set_tool (context, tool_info ("gimp-move-tool"));
  g_object_set (tool_info ("gimp-move-tool")->tool_options,
                 "move-current", TRUE, NULL);
  gimp_context_set_tool (context, tool_info ("gimp-perspective-guide-tool"));
  set_boolean ("painter-canvas-ui", TRUE);
  g_assert_true (find_host () != host);
  assert_route (find_host (), image_display);
  g_object_unref (gui);
  g_object_unref (placeholder);
  g_object_unref (empty);
  g_object_unref (host);
  settle ();
  assert_finalized (&gui_life, TRUE);
  assert_finalized (&shell_life, TRUE);
  assert_finalized (&display_life, FALSE);
  assert_finalized (&host_life, TRUE);
  {
    GimpUIManager *manager = gimp_test_utils_get_ui_manager (gimp);
    g_signal_emit_by_name (manager, "show-tooltip", "Live manager after toolbar destruction");
    g_signal_emit_by_name (manager, "hide-tooltip");
    settle ();
    assert_route (find_host (), image_display);
  }
  close_display (image_display);
}

static void
borrowed_display_closes_and_finalizes (void)
{
  GimpImageWindow *host = find_host ();
  GimpDisplay *display = new_display ();
  GimpDisplayShell *shell = gimp_display_get_shell (display);
  GimpImage *image = gimp_display_get_image (display);
  Lifetime display_life = { 0 }, shell_life = { 0 };

  watch (G_OBJECT (display), &display_life);
  watch (G_OBJECT (shell), &shell_life);
  g_object_ref (display);
  g_object_ref (shell);
  select_display (display);
  assert_route (host, display);
  gimp_image_clean_all (image);
  /* File Close/Ctrl+W's production callback must close the routed image,
   * leaving the host, rather than trying to close its owned placeholder. */
  view_close_cmd_callback (NULL, NULL, host);
  g_object_unref (image);
  settle ();
  g_assert_true (find_host () == host);
  assert_graph (0, TRUE, FALSE);
  assert_route (host, NULL);
  g_assert_cmpuint (shell_life.destroyed, ==, 1);
  g_assert_cmpuint (shell_life.finalized, ==, 0);
  g_assert_cmpuint (display_life.finalized, ==, 0);
  g_object_unref (shell);
  g_object_unref (display);
  settle ();
  assert_finalized (&shell_life, TRUE);
  assert_finalized (&display_life, FALSE);
  /* The same command with no image is a harmless no-op, not Quit. */
  {
    guint previous = exit_requests;
    view_close_cmd_callback (NULL, NULL, host);
    settle ();
    g_assert_cmpuint (exit_requests, ==, previous);
    assert_graph (0, TRUE, FALSE);
  }
}

static void
unretained_toolbar_finalizes_on_switch (void)
{
  GimpImageWindow *host = find_host ();
  GimpContext *context = gimp_get_user_context (gimp);
  const gchar *names[] = { "gimp-move-tool", "gimp-paintbrush-tool",
                           "gimp-rotate-tool", "gimp-perspective-guide-tool" };

  for (guint i = 0; i < 8; i++)
    {
      Lifetime lifetime = { 0 };
      gimp_context_set_tool (context, tool_info (names[i % G_N_ELEMENTS (names)]));
      settle ();
      watch (G_OBJECT (host_gui (host, TRUE)), &lifetime);
      gimp_context_set_tool (context, tool_info ("gimp-brightness-contrast-tool"));
      settle ();
      host_gui (host, FALSE);
      assert_finalized (&lifetime, TRUE);
    }
}

static void
remove_profile (GFile *file)
{
  GError *error = NULL;
  if (g_file_query_file_type (file, G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, NULL) ==
      G_FILE_TYPE_DIRECTORY)
    {
      GFileEnumerator *children = g_file_enumerate_children
        (file, G_FILE_ATTRIBUTE_STANDARD_NAME, G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
         NULL, &error);
      GFileInfo *info;
      g_assert_no_error (error);
      g_assert_nonnull (children);
      while ((info = g_file_enumerator_next_file (children, NULL, &error)))
        {
          GFile *child = g_file_get_child (file, g_file_info_get_name (info));
          remove_profile (child);
          g_object_unref (child);
          g_object_unref (info);
        }
      g_assert_no_error (error);
      g_assert_true (g_file_enumerator_close (children, NULL, &error));
      g_assert_no_error (error);
      g_object_unref (children);
    }
  g_assert_true (g_file_delete (file, NULL, &error));
  g_assert_no_error (error);
}

static void
session_geometry (GimpImageWindow *host)
{
  GFile *marker = gimp_directory_file ("toolbar-window-geometry", NULL);
  GError *error = NULL;
  gint width, height;

  /* Keep both passes on the same modest independent factory, so changing a
   * tool's minimum requisition cannot masquerade as session geometry drift. */
  gimp_context_set_tool (gimp_get_user_context (gimp), tool_info ("gimp-move-tool"));
  settle ();
  if (g_strcmp0 (g_getenv (ENV_PASS), "write") == 0)
    {
      GdkRectangle workarea;
      gchar *contents;
      gdk_monitor_get_workarea (gimp_widget_get_monitor (GTK_WIDGET (host)), &workarea);
      gtk_window_unmaximize (GTK_WINDOW (host));
      gtk_window_resize (GTK_WINDOW (host), MIN (1010, workarea.width - 80),
                         MIN (260, workarea.height - 80));
      gimp_test_run_temp_mainloop (300);
      settle ();
      gtk_window_get_size (GTK_WINDOW (host), &width, &height);
      g_assert_cmpint (width, >, 0);
      g_assert_cmpint (height, >, 0);
      contents = g_strdup_printf ("%d %d", width, height);
      g_assert_true (g_file_replace_contents (marker, contents, strlen (contents),
                                              NULL, FALSE, G_FILE_CREATE_NONE,
                                              NULL, NULL, &error));
      g_assert_no_error (error);
      g_free (contents);
    }
  else
    {
      gchar *contents = NULL;
      gchar **fields;
      gint expected_width, expected_height;
      g_assert_cmpstr (g_getenv (ENV_PASS), ==, "read");
      g_assert_true (g_file_load_contents (marker, NULL, &contents, NULL, NULL, &error));
      g_assert_no_error (error);
      fields = g_strsplit (contents, " ", -1);
      g_assert_cmpuint (g_strv_length (fields), ==, 2);
      expected_width = (gint) g_ascii_strtoll (fields[0], NULL, 10);
      expected_height = (gint) g_ascii_strtoll (fields[1], NULL, 10);
      gimp_test_run_temp_mainloop (300);
      settle ();
      gtk_window_get_size (GTK_WINDOW (host), &width, &height);
      /* Check before this process saves anything. Allow the existing session
       * position-accuracy and a small GTK/WM geometry rounding margin. */
      g_assert_cmpint (ABS (width - expected_width), <=, 12);
      g_assert_cmpint (ABS (height - expected_height), <=, 12);
      g_strfreev (fields);
      g_free (contents);
    }
  g_object_unref (marker);
}

static void
assert_child_no_diagnostics (void)
{
  const gchar *patterns[] = {
    "*WARNING*", "*CRITICAL*", "*ERROR*",
    "*Warning:*", "*Critical:*", "*Error:*",
    "*warning:*", "*critical:*", "*error:*"
  };

  /* stderr is inherited for the outer strict runtime classifier. Keep stdout
   * captured to avoid nested TAP, but do not let a passing child silently hide
   * diagnostic output written to that stream. A mismatch prints the capture. */
  for (guint i = 0; i < G_N_ELEMENTS (patterns); i++)
    g_test_trap_assert_stdout_unmatched (patterns[i]);
}

static void
session_save_fresh_restore (void)
{
  if (g_test_subprocess ())
    {
      GimpImageWindow *host = find_host ();
      GimpDialogFactory *factory = gimp_dialog_factory_get_singleton ();
      GtkWidget *dockable;
      GimpDisplay *display;
      gchar *saved = NULL;
      GFile *session = gimp_directory_file ("sessionrc", NULL);
      GError *error = NULL;
      gboolean single;

      settle ();
      g_object_get (gimp->config, "single-window-mode", &single, NULL);
      g_assert_false (single);
      assert_graph (0, TRUE, FALSE);
      session_geometry (host);
      /* This fixture restores a native tool-options dock on both passes. */
      dockable = gimp_dialog_factory_find_widget (factory, "gimp-tool-options");
      g_assert_true (GIMP_IS_DOCKABLE (dockable));
      g_assert_nonnull (gimp_dockable_get_dockbook (GIMP_DOCKABLE (dockable)));
      display = new_display ();
      g_assert_true (find_host () == host);
      assert_graph (1, TRUE, FALSE);
      session_save (gimp, TRUE);
      g_assert_true (g_file_load_contents (session, NULL, &saved, NULL, NULL, &error));
      g_assert_no_error (error);
      g_assert_nonnull (strstr (saved, "(single-window-mode no)"));
      g_assert_nonnull (strstr (saved, "gimp-tool-options"));
      g_assert_nonnull (strstr (saved, "gimp-empty-image-window"));
      g_assert_null (strstr (saved, "painter-toolbar-window-controls"));
      g_free (saved);
      g_object_unref (session);
      close_display (display);
      return;
    }

  {
    GError *error = NULL;
    gchar *directory = g_dir_make_tmp ("gimp-toolbar-session-XXXXXX", &error);
    gchar *source_path, *destination_path;
    GFile *source, *destination, *profile;

    g_assert_no_error (error);
    g_assert_nonnull (directory);
    profile = g_file_new_for_path (directory);
    source_path = g_build_filename (g_getenv ("GIMP_TESTING_ABS_TOP_SRCDIR"),
                                    "app", "tests", "gimpdir", "sessionrc", NULL);
    destination_path = g_build_filename (directory, "sessionrc", NULL);
    source = g_file_new_for_path (source_path);
    destination = g_file_new_for_path (destination_path);
    g_assert_true (g_file_copy (source, destination, G_FILE_COPY_NONE,
                                NULL, NULL, NULL, &error));
    g_assert_no_error (error);
    g_setenv (ENV_SESSION, directory, TRUE);
    g_setenv (ENV_PAINTER, "1", TRUE);
    for (guint pass = 0; pass < 2; pass++)
      {
        /* The reader starts with the opposite mode before session_init();
         * only the writer's actual persisted session can switch it back. */
        g_setenv (ENV_SINGLE, pass ? "1" : "0", TRUE);
        g_setenv (ENV_PASS, pass ? "read" : "write", TRUE);
        g_test_trap_subprocess (NULL, 60 * G_USEC_PER_SEC,
                                G_TEST_SUBPROCESS_INHERIT_STDERR);
        g_test_trap_assert_passed ();
        assert_child_no_diagnostics ();
      }
    g_unsetenv (ENV_SESSION);
    g_unsetenv (ENV_PASS);
    g_unsetenv (ENV_SINGLE);
    g_unsetenv (ENV_PAINTER);
    remove_profile (profile);
    g_object_unref (profile);
    g_object_unref (source);
    g_object_unref (destination);
    g_free (source_path);
    g_free (destination_path);
    g_free (directory);
  }
}

// Task 29.010/toolbar-transfer-menu-transaction.
#include "test-painter-toolbar-transfer.inc"

static void
run_case (gconstpointer data)
{
  const ToolbarCase *test = data;
  if (! g_test_subprocess ())
    {
      g_setenv (ENV_PAINTER, test->painter ? "1" : "0", TRUE);
      g_setenv (ENV_SINGLE, test->single ? "1" : "0", TRUE);
      g_test_trap_subprocess (NULL, 60 * G_USEC_PER_SEC,
                                G_TEST_SUBPROCESS_INHERIT_STDERR);
      g_test_trap_assert_passed ();
      assert_child_no_diagnostics ();
      g_unsetenv (ENV_PAINTER);
      g_unsetenv (ENV_SINGLE);
      return;
    }
  gimp_set_focused_once (gimp);
  settle ();
  test->run ();
}


int
main (int argc, char **argv)
{
  gint result;

  g_test_init (&argc, &argv, NULL);
  if (! gtk_init_check (&argc, &argv))
    return GIMP_EXIT_TEST_SKIPPED;
#define ADD(name, function, painter, single) G_STMT_START { \
  static const ToolbarCase test = { function, painter, single }; \
  g_test_add_data_func ("/painter-toolbar-window/" name, &test, run_case); \
} G_STMT_END
  ADD ("startup/native-multi", startup_and_native_reuse, FALSE, FALSE);
  ADD ("startup/native-single", startup_and_native_reuse, FALSE, TRUE);
  ADD ("startup/painter-single", startup_and_native_reuse, TRUE, TRUE);
  ADD ("startup/painter-multi", startup_and_native_reuse, TRUE, FALSE);
  ADD ("single-mode-painter-preserves-external-docks", single_mode_painter_preserves_external_docks, FALSE, TRUE);
  ADD ("transfer-radio-command-supersession", transfer_radio_command_supersession, TRUE, FALSE);
  ADD ("transfer-radio-notification", transfer_radio_notification, TRUE, FALSE);
  ADD ("transfer-action-publication", transfer_action_publication, TRUE, FALSE);
  ADD ("transfer-global-actions", transfer_global_actions, TRUE, FALSE);
  ADD ("new-open-focus-routing", new_open_and_focus_routing, TRUE, FALSE);
  ADD ("host-actions-close-accelerator", host_actions_and_close_accelerator, TRUE, FALSE);
  ADD ("nested-conversion-supersession", nested_conversion_supersession, TRUE, FALSE);
  ADD ("display-closes-during-dock-separation", display_closes_during_dock_separation, TRUE, FALSE);
  ADD ("source-closes-during-dock-transfer", source_window_closes_during_dock_transfer, TRUE, FALSE);
  ADD ("toolbar-publication-superseded", toolbar_publication_superseded, TRUE, FALSE);
  ADD ("tool-models-native-ownership", tool_models_and_native_ownership, TRUE, FALSE);
  ADD ("close-active-inactive-last", close_active_inactive_last, TRUE, FALSE);
  ADD ("repeat-mode-painter-toggles", repeat_mode_and_painter_toggles, TRUE, FALSE);
  ADD ("wm-close-requests-quit", wm_close_requests_quit, TRUE, FALSE);
  ADD ("wm-close-dirty-cancel", wm_close_dirty_cancel, TRUE, FALSE);
  ADD ("portrait-toolbar-scroll-reaches-end", portrait_toolbar_scroll_reaches_end, TRUE, FALSE);
  ADD ("menu-scroll-role-titlebar-lifecycle", menu_scroll_role_and_titlebar_lifecycle, FALSE, FALSE);
  ADD ("retained-destroyed-pending-updates", retained_destroyed_pending_updates, TRUE, FALSE);
  ADD ("borrowed-display-closes-finalizes", borrowed_display_closes_and_finalizes, TRUE, FALSE);
  ADD ("unretained-toolbar-finalizes", unretained_toolbar_finalizes_on_switch, TRUE, FALSE);
#undef ADD
  g_test_add_func ("/painter-toolbar-window/session-save-fresh-restore",
                   session_save_fresh_restore);

  if (! g_test_subprocess ())
    return g_test_run ();

  gimp_test_utils_setup_menus_path ();
  gimp = gimp_init_for_gui_testing (TRUE);
  g_object_set (gimp->config,
                 "painter-canvas-ui", g_strcmp0 (g_getenv (ENV_PAINTER), "1") == 0,
                 "single-window-mode", g_strcmp0 (g_getenv (ENV_SINGLE), "1") == 0,
                 NULL);
  if (g_getenv (ENV_SESSION))
    g_setenv ("GIMP3_DIRECTORY", g_getenv (ENV_SESSION), TRUE);
  /* Connected before gui_init's exit observer, allowing the clean-WM case to
   * witness the actual Quit request without shutting down the test harness. */
  g_signal_connect (gimp, "exit", G_CALLBACK (observe_exit), NULL);
  g_application_run (gimp->app, 0, NULL);
  result = gimp_core_app_get_exit_status (GIMP_CORE_APP (gimp->app));
  g_application_quit (G_APPLICATION (gimp->app));
  g_clear_object (&gimp->app);
  return result;
}
