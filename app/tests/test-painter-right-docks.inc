/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Production-caller regressions. Included by the C canvas harness after its
 * helpers. Deliberately never calls the generic attach/detach test helper. */
#include "display/gimpimagewindow.h"
#include "widgets/gimpdockcolumns.h"
#include "widgets/gimpdock.h"
#include "widgets/gimpdockbook.h"
#include "widgets/gimpdockable.h"
#include "widgets/gimpdialogfactory.h"
#include "widgets/gimpitemtreeview.h"
#include "widgets/gimppaintercompactoptions.h"
#include "tools/gimp-tools.h"

typedef struct
{
  GimpImageWindow *image_window;
  GimpDisplayShell *shells[2];
  GtkWidget *columns;
  GtkWidget *home;
  GtkWidget *home_child1;
  GtkNotebook *notebook;
  GtkWidget *books[2];
  GtkWidget *dockables[4];
  GtkWidget *guis[2];
  GimpToolInfo *tools[2];
  GimpToolInfo *saved_tool;
  GimpToolGroup *groups[3];
  gchar *group_active[3];
  gboolean group_expanded[3];
  guint n_groups;
  GPtrArray *contents;
  gboolean saved_single, saved_hide, saved_painter;
  gboolean resize, shrink;
  gint divider;
  guint switches;
  guint parent_sets;
} RightDocksFixture;

/* GObject setters emit notify even for unchanged values. In particular,
 * re-emitting single-window-mode TRUE with no displays enters the native
 * configurer without an active display. Setup/teardown must not do that. */
static void
right_docks_set_boolean (const gchar *property,
                         gboolean     value)
{
  gboolean previous;

  g_object_get (gimp->config, property, &previous, NULL);
  if (previous != value)
    g_object_set (gimp->config, property, value, NULL);
}

static void
right_docks_save_group (RightDocksFixture *f,
                        GimpToolInfo      *tool)
{
  GimpViewable *parent = gimp_viewable_get_parent (GIMP_VIEWABLE (tool));
  guint index = f->n_groups;

  if (!GIMP_IS_TOOL_GROUP (parent))
    return;
  for (guint i = 0; i < index; i++)
    if (f->groups[i] == GIMP_TOOL_GROUP (parent))
      return;
  g_assert_cmpuint (index, <, G_N_ELEMENTS (f->groups));
  f->groups[index] = g_object_ref (GIMP_TOOL_GROUP (parent));
  f->group_active[index] = g_strdup (gimp_tool_group_get_active_tool (f->groups[index]));
  f->group_expanded[index] = gimp_viewable_get_expanded (parent);
  f->n_groups++;
}

static void
right_docks_restore_groups (RightDocksFixture *f)
{
  /* Restore the context first: the native tool manager synchronizes its active
   * group when the selected tool changes. These snapshots precede every test
   * selection, including the paintbrush setup selection. */
  gimp_context_set_tool (gimp_get_user_context (gimp), f->saved_tool);
  for (guint i = 0; i < f->n_groups; i++)
    gimp_tool_group_set_active_tool (f->groups[i], f->group_active[i]);
  for (guint i = 0; i < f->n_groups; i++)
    gimp_viewable_set_expanded (GIMP_VIEWABLE (f->groups[i]), f->group_expanded[i]);
  drain ();
  g_assert_true (gimp_context_get_tool (gimp_get_user_context (gimp)) == f->saved_tool);
  for (guint i = 0; i < f->n_groups; i++)
    {
      g_assert_cmpstr (gimp_tool_group_get_active_tool (f->groups[i]), ==, f->group_active[i]);
      g_assert_cmpint (gimp_viewable_get_expanded (GIMP_VIEWABLE (f->groups[i])), ==,
                       f->group_expanded[i]);
      g_free (f->group_active[i]);
      g_object_unref (f->groups[i]);
    }
  g_object_unref (f->saved_tool);
}

static gboolean
right_docks_drawable (GtkWidget *widget)
{
  GdkWindow *window = gtk_widget_get_window (widget);

  return gtk_widget_get_realized (widget) &&
         gtk_widget_get_mapped (widget) &&
         gtk_widget_is_drawable (widget) &&
         window && gdk_window_is_viewable (window) &&
         gtk_widget_get_allocated_width (widget) > 0 &&
         gtk_widget_get_allocated_height (widget) > 0;
}

static void
right_docks_wait_drawable (GtkWidget *widget)
{
  GtkWidget *observed = widget;
  gint64 deadline = g_get_monotonic_time () + 2 * G_TIME_SPAN_SECOND;

  /* Weak observation remains safe while pending GTK allocations are drained;
   * a test-held widget ref must not conceal a destroyed/parentless control. */
  g_object_add_weak_pointer (G_OBJECT (observed), (gpointer *) &observed);
  drain ();
  while (observed && !right_docks_drawable (observed) &&
         g_get_monotonic_time () < deadline)
    {
      g_main_context_iteration (NULL, FALSE);
      g_usleep (1000);
    }
  g_assert_nonnull (observed);
  g_assert_true (gtk_widget_get_mapped (observed));
  g_assert_true (gtk_widget_is_drawable (observed));
  g_assert_true (right_docks_drawable (observed));
  g_object_remove_weak_pointer (G_OBJECT (observed), (gpointer *) &observed);
}

static void
right_docks_count_parent_set (GtkWidget *widget,
                              GtkWidget *previous,
                              gpointer   data)
{
  RightDocksFixture *f = data;

  (void) widget;
  (void) previous;
  f->parent_sets++;
}

static void
right_docks_action (RightDocksFixture *f,
                    const gchar       *group,
                    const gchar       *action,
                    gboolean           active)
{
  GimpUIManager *manager = gimp_test_utils_get_ui_manager (gimp);
  GimpDisplayShell *shell = gimp_image_window_get_active_shell (f->image_window);

  g_assert_nonnull (shell);
  gimp_ui_manager_update (manager, shell->display);
  g_assert_true (gimp_ui_manager_toggle_action (manager, group, action, active));
}

static void
right_docks_switched (GtkNotebook *notebook,
                      GtkWidget   *page,
                      guint        page_number,
                      gpointer     data)
{
  RightDocksFixture *f = data;

  (void) notebook;
  (void) page;
  (void) page_number;
  f->switches++;
}

static void
right_docks_select (RightDocksFixture *f,
                    guint              selected)
{
  GimpDisplayShell *shell = f->shells[selected];

  g_assert_nonnull (shell);
  gimp_image_window_set_active_shell (f->image_window, shell);
  drain ();
  g_assert_true (gimp_image_window_get_active_shell (f->image_window) == shell);
  g_assert_true (gtk_notebook_get_nth_page (f->notebook,
                    gtk_notebook_get_current_page (f->notebook)) == GTK_WIDGET (shell));
  g_assert_true (gimp_context_get_display (gimp_get_user_context (gimp)) == shell->display);
}

/* Save the complete public dock/book/tab sequence, including any restored
 * session contents. No additional widget refs can conceal parent loss. */
static GPtrArray *
right_docks_contents (RightDocksFixture *f)
{
  GPtrArray *contents = g_ptr_array_new ();
  GList *docks;

  g_assert_nonnull (f->columns);
  docks = gimp_dock_columns_get_docks (GIMP_DOCK_COLUMNS (f->columns));
  for (GList *d = docks; d; d = d->next)
    {
      g_ptr_array_add (contents, d->data);
      for (GList *b = gimp_dock_get_dockbooks (GIMP_DOCK (d->data)); b; b = b->next)
        {
          GtkNotebook *book = GTK_NOTEBOOK (b->data);

          g_ptr_array_add (contents, book);
          for (gint page = 0; page < gtk_notebook_get_n_pages (book); page++)
            g_ptr_array_add (contents, gtk_notebook_get_nth_page (book, page));
        }
    }
  return contents;
}

static void
right_docks_assert_contents (RightDocksFixture *f)
{
  GPtrArray *actual = right_docks_contents (f);

  g_assert_cmpuint (actual->len, ==, f->contents->len);
  for (guint i = 0; i < actual->len; i++)
    g_assert_true (g_ptr_array_index (actual, i) == g_ptr_array_index (f->contents, i));
  g_ptr_array_unref (actual);
  for (guint i = 0; i < G_N_ELEMENTS (f->dockables); i++)
    {
      g_assert_nonnull (f->dockables[i]);
      g_assert_true (gimp_dockable_get_dockbook (GIMP_DOCKABLE (f->dockables[i])) ==
                     GIMP_DOCKBOOK (f->books[i / 2]));
      g_assert_true (gtk_notebook_get_nth_page (GTK_NOTEBOOK (f->books[i / 2]), i % 2) ==
                     f->dockables[i]);
    }
}

static void
right_docks_assert_parent (RightDocksFixture *f)
{
  gboolean valid;

  g_assert_nonnull (f->columns);
  g_assert_nonnull (gtk_widget_get_parent (f->columns));
  valid = gtk_widget_get_parent (f->columns) == f->home;
  for (guint i = 0; i < G_N_ELEMENTS (f->shells); i++)
    if (f->shells[i])
      valid |= gtk_widget_is_ancestor (f->columns, f->shells[i]->canvas);
  g_assert_true (valid);
  right_docks_assert_contents (f);
}

static GtkWidget *
right_docks_overlay_root (GtkWidget *widget,
                          GtkWidget *canvas)
{
  GtkWidget *root = widget;

  g_assert_true (gtk_widget_is_ancestor (widget, canvas));
  while (gtk_widget_get_parent (root) != canvas)
    {
      root = gtk_widget_get_parent (root);
      g_assert_nonnull (root);
    }
  g_assert_nonnull (gimp_overlay_child_find (GIMP_OVERLAY_BOX (canvas), root));
  g_object_add_weak_pointer (G_OBJECT (root), (gpointer *) &root);
  g_object_add_weak_pointer (G_OBJECT (widget), (gpointer *) &widget);
  right_docks_wait_drawable (canvas);
  g_assert_nonnull (root);
  right_docks_wait_drawable (root);
  g_assert_nonnull (widget);
  right_docks_wait_drawable (widget);
  g_assert_nonnull (root);
  g_assert_true (gtk_widget_is_ancestor (widget, canvas));
  g_assert_true (gtk_widget_get_parent (root) == canvas);
  g_object_remove_weak_pointer (G_OBJECT (widget), (gpointer *) &widget);
  g_object_remove_weak_pointer (G_OBJECT (root), (gpointer *) &root);
  return root;
}

static void
right_docks_assert_canvas (RightDocksFixture *f,
                           guint              selected)
{
  GimpDisplayShell *shell = f->shells[selected];

  right_docks_assert_parent (f);
  g_assert_true (gimp_image_window_get_active_shell (f->image_window) == shell);
  g_test_message ("Right columns parent: %s; selected canvas: %s",
                  G_OBJECT_TYPE_NAME (gtk_widget_get_parent (f->columns)),
                  G_OBJECT_TYPE_NAME (shell->canvas));
  /* Primary red assertion: the real window-owned columns must be on the
   * selected canvas. Current production leaves them in GtkPaned child2. */
  g_assert_true (gtk_widget_is_ancestor (f->columns, shell->canvas));
  right_docks_overlay_root (f->columns, shell->canvas);
  g_assert_true (gimp_image_window_get_right_docks (f->image_window) ==
                 GIMP_DOCK_COLUMNS (f->columns));
  g_assert_true (gtk_widget_get_visible (f->columns));
  g_assert_true (gimp_context_get_display (
                   gimp_dock_columns_get_context (GIMP_DOCK_COLUMNS (f->columns))) ==
                 shell->display);
  for (guint i = 0; i < 2; i++)
    {
      GtkWidget *view = gtk_bin_get_child (GTK_BIN (f->dockables[i]));

      g_assert_true (GIMP_IS_ITEM_TREE_VIEW (view));
      g_assert_true (gimp_item_tree_view_get_image (GIMP_ITEM_TREE_VIEW (view)) ==
                     gimp_display_get_image (shell->display));
    }
}

static void
right_docks_assert_options (RightDocksFixture *f,
                            guint              selected,
                            guint              tool,
                            gboolean           compact)
{
  GtkWidget *canvas = f->shells[selected]->canvas;

  g_assert_nonnull (f->guis[tool]);
  g_assert_true (gimp_tools_get_tool_options_gui (f->tools[tool]->tool_options) ==
                 f->guis[tool]);
  right_docks_overlay_root (f->guis[tool], canvas);
  right_docks_overlay_root (f->columns, canvas);
  g_assert_false (gtk_widget_is_ancestor (f->guis[tool], f->columns));
  g_assert_false (gtk_widget_is_ancestor (f->columns, f->guis[tool]));
  g_assert_cmpint (gimp_painter_compact_options_get (f->guis[tool]), ==, compact);
}

static void
right_docks_assert_home (RightDocksFixture *f)
{
  gboolean resize, shrink;

  right_docks_assert_parent (f);
  g_assert_true (gtk_widget_get_parent (f->columns) == f->home);
  g_assert_true (gtk_paned_get_child1 (GTK_PANED (f->home)) == f->home_child1);
  g_assert_true (gtk_paned_get_child2 (GTK_PANED (f->home)) == f->columns);
  gtk_container_child_get (GTK_CONTAINER (f->home), f->columns,
                           "resize", &resize, "shrink", &shrink, NULL);
  g_assert_cmpint (resize, ==, f->resize);
  g_assert_cmpint (shrink, ==, f->shrink);
  g_assert_cmpint (gtk_paned_get_position (GTK_PANED (f->home)), ==, f->divider);
}

/* Initial GTK geometry is not settled merely because no main-context idle
 * is pending: canvas size allocation schedules a native tick callback. Use
 * the real frame clock's after-paint phase, also exercised by the existing
 * close_with_pending_resize test, before taking the restore snapshot. */
typedef struct
{
  RightDocksFixture *fixture;
  GtkAllocation allocations[4];
  gint divider;
  guint samples;
  guint stable_frames;
} RightDocksInitialGeometry;

static void
right_docks_initial_frame_painted (GdkFrameClock *clock,
                                   gpointer       data)
{
  RightDocksInitialGeometry *g = data;
  RightDocksFixture *f = g->fixture;
  GtkWidget *widgets[4];
  gint divider;
  gboolean same = g->samples > 0;

  (void) clock;
  g_assert_nonnull (f->image_window);
  g_assert_nonnull (f->home);
  g_assert_nonnull (f->columns);
  g_assert_nonnull (f->shells[0]);
  widgets[0] = GTK_WIDGET (f->image_window);
  widgets[1] = f->home;
  widgets[2] = f->columns;
  widgets[3] = f->shells[0]->canvas;
  for (guint i = 0; i < G_N_ELEMENTS (widgets); i++)
    {
      GtkAllocation current;
      GtkAllocation previous = g->allocations[i];

      gtk_widget_get_allocation (widgets[i], &current);
      same &= right_docks_drawable (widgets[i]) &&
              current.x == previous.x && current.y == previous.y &&
              current.width == previous.width && current.height == previous.height;
      g->allocations[i] = current;
    }
  divider = gtk_paned_get_position (GTK_PANED (f->home));
  same &= divider == g->divider;
  g->divider = divider;
  g->samples++;
  g->stable_frames = same ? g->stable_frames + 1 : 0;
}

static void
right_docks_settle_initial_geometry (RightDocksFixture *f)
{
  RightDocksInitialGeometry geometry = { 0 };
  GdkFrameClock *clock;
  gulong handler;
  gint64 deadline;

  /* Setup-only: never resnapshot/rebaseline after an exercised transfer. */
  g_assert_false (gimp_painter_canvas_ui_get_visible (f->shells[0]));
  g_assert_false (gimp_painter_canvas_ui_get_visible (f->shells[1]));
  g_assert_true (gtk_widget_get_parent (f->columns) == f->home);
  g_assert_true (gimp_image_window_get_active_shell (f->image_window) == f->shells[0]);
  right_docks_wait_drawable (GTK_WIDGET (f->image_window));
  right_docks_wait_drawable (f->home);
  right_docks_wait_drawable (f->columns);
  right_docks_wait_drawable (f->shells[0]->canvas);
  geometry.fixture = f;
  clock = gtk_widget_get_frame_clock (f->shells[0]->canvas);
  g_assert_nonnull (clock);
  g_object_ref (clock);
  handler = g_signal_connect (clock, "after-paint",
                              G_CALLBACK (right_docks_initial_frame_painted), &geometry);
  deadline = g_get_monotonic_time () + 5 * G_TIME_SPAN_SECOND;
  while (geometry.stable_frames < 3 && g_get_monotonic_time () < deadline)
    {
      gdk_frame_clock_request_phase (clock, GDK_FRAME_CLOCK_PHASE_PAINT);
      gimp_test_run_temp_mainloop (10);
    }
  g_signal_handler_disconnect (clock, handler);
  g_object_unref (clock);
  g_assert_cmpuint (geometry.stable_frames, >=, 3);
  g_test_message ("Initial right-dock geometry settled after %u painted frames: "
                  "home width %d, divider %d",
                  geometry.samples, geometry.allocations[1].width, geometry.divider);
}

static void
right_docks_begin (RightDocksFixture *f)
{
  const gchar *identifiers[] = { "gimp-layer-list", "gimp-channel-list",
                                "gimp-undo-history", "gimp-brush-grid" };
  const gchar *tool_names[] = { "gimp-paintbrush-tool", "gimp-rect-select-tool" };

  memset (f, 0, sizeof (*f));
  g_object_get (gimp->config, "single-window-mode", &f->saved_single,
                "hide-docks", &f->saved_hide, "painter-canvas-ui", &f->saved_painter, NULL);
  f->saved_tool = g_object_ref (gimp_context_get_tool (gimp_get_user_context (gimp)));
  right_docks_set_boolean ("painter-canvas-ui", FALSE);
  right_docks_set_boolean ("hide-docks", FALSE);
  drain ();
  for (guint i = 0; i < G_N_ELEMENTS (f->tools); i++)
    {
      f->tools[i] = GIMP_TOOL_INFO (gimp_container_get_child_by_name (gimp->tool_info_list,
                                                                   tool_names[i]));
      g_assert_nonnull (f->tools[i]);
    }
  right_docks_save_group (f, f->saved_tool);
  for (guint i = 0; i < G_N_ELEMENTS (f->tools); i++)
    right_docks_save_group (f, f->tools[i]);
  gimp_context_set_tool (gimp_get_user_context (gimp), f->tools[0]);
  /* There must be a live active display before FALSE -> TRUE conversion.
   * If already TRUE, skip the setter entirely, including its notify signal. */
  f->shells[0] = new_shell ();
  right_docks_set_boolean ("single-window-mode", TRUE);
  drain ();
  f->image_window = gimp_display_shell_get_window (f->shells[0]);
  /* The startup display may predate the test's config change. Normalize it
   * through the same action used by the user, never the borrow helper. */
  right_docks_action (f, "view", "view-painter-canvas-ui", FALSE);
  drain ();
  f->shells[1] = new_shell ();
  g_assert_true (gimp_display_shell_get_window (f->shells[1]) == f->image_window);
  g_assert_cmpint (gimp_image_window_get_n_shells (f->image_window), ==, 2);
  f->notebook = GTK_NOTEBOOK (gtk_widget_get_parent (GTK_WIDGET (f->shells[0])));
  g_assert_true (GTK_IS_NOTEBOOK (f->notebook));
  g_assert_true (gtk_widget_get_parent (GTK_WIDGET (f->shells[1])) == GTK_WIDGET (f->notebook));
  f->columns = GTK_WIDGET (gimp_image_window_get_right_docks (f->image_window));
  f->home = gtk_widget_get_parent (f->columns);
  g_assert_true (GTK_IS_PANED (f->home));
  f->home_child1 = gtk_paned_get_child1 (GTK_PANED (f->home));
  g_assert_true (f->home_child1 == GTK_WIDGET (f->notebook));
  g_assert_true (gtk_paned_get_child2 (GTK_PANED (f->home)) == f->columns);
  gtk_container_child_get (GTK_CONTAINER (f->home), f->columns,
                           "resize", &f->resize, "shrink", &f->shrink, NULL);
  g_assert_false (f->resize);
  g_assert_false (f->shrink);
  for (guint i = 0; i < G_N_ELEMENTS (f->books); i++)
    gimp_dock_columns_prepare_dockbook (GIMP_DOCK_COLUMNS (f->columns), -1, &f->books[i]);
  for (guint i = 0; i < G_N_ELEMENTS (identifiers); i++)
    {
      f->dockables[i] = gimp_dockbook_add_from_dialog_factory (GIMP_DOCKBOOK (f->books[i / 2]),
                                                              identifiers[i]);
      g_assert_true (GIMP_IS_DOCKABLE (f->dockables[i]));
    }
  gtk_widget_show_all (f->columns);
  /* Canonical GUI creation is lazy and can change native minimum sizes.
   * Complete it before selecting A and taking any restoration snapshot. */
  for (guint i = 0; i < G_N_ELEMENTS (f->guis); i++)
    {
      f->guis[i] = gimp_tools_get_tool_options_gui (f->tools[i]->tool_options);
      g_assert_nonnull (f->guis[i]);
      g_object_add_weak_pointer (G_OBJECT (f->guis[i]), (gpointer *) &f->guis[i]);
      g_object_add_weak_pointer (G_OBJECT (f->shells[i]), (gpointer *) &f->shells[i]);
    }
  g_object_add_weak_pointer (G_OBJECT (f->image_window), (gpointer *) &f->image_window);
  g_object_add_weak_pointer (G_OBJECT (f->columns), (gpointer *) &f->columns);
  g_object_add_weak_pointer (G_OBJECT (f->home), (gpointer *) &f->home);
  g_signal_connect_after (f->notebook, "switch-page", G_CALLBACK (right_docks_switched), f);
  g_signal_connect (f->columns, "parent-set", G_CALLBACK (right_docks_count_parent_set), f);
  right_docks_select (f, 0);
  gtk_window_resize (GTK_WINDOW (f->image_window), 1200, 800);
  right_docks_settle_initial_geometry (f);
  gtk_paned_set_position (GTK_PANED (f->home),
                          MAX (1, gtk_widget_get_allocated_width (f->home) - 320));
  /* GTK may clamp the requested divider to the populated docks' minima.
   * Save the actual stable pre-operation position, not the pending request. */
  right_docks_settle_initial_geometry (f);
  f->divider = gtk_paned_get_position (GTK_PANED (f->home));
  f->contents = right_docks_contents (f);
  right_docks_assert_home (f);
}

static void
right_docks_enable (RightDocksFixture *f,
                    guint              selected)
{
  gboolean configured;

  right_docks_select (f, selected);
  /* Set an explicit TRUE action state, never flip the global preference twice.
   * The second enable is idempotent if production already propagated it. */
  right_docks_action (f, "view", "view-painter-canvas-ui", TRUE);
  drain ();
  g_object_get (gimp->config, "painter-canvas-ui", &configured, NULL);
  g_assert_true (configured);
  g_assert_true (gimp_painter_canvas_ui_get_visible (f->shells[selected]));
}

static void
right_docks_end (RightDocksFixture *f)
{
  gboolean single, hide, painter;

  /* Leave any active borrow in place: normal display deletion must restore or
   * retire it, and the last-window path must release every borrowed column. */
  for (guint i = 0; i < G_N_ELEMENTS (f->shells); i++)
    if (f->shells[i])
      close_shell (f->shells[i]);
  drain ();
  g_assert_null (f->shells[0]);
  g_assert_null (f->shells[1]);
  g_assert_null (f->image_window);
  g_assert_null (f->columns);
  g_assert_null (f->home);
  for (guint i = 0; i < G_N_ELEMENTS (f->guis); i++)
    if (f->guis[i])
      g_object_remove_weak_pointer (G_OBJECT (f->guis[i]), (gpointer *) &f->guis[i]);
  g_ptr_array_unref (f->contents);
  right_docks_restore_groups (f);
  right_docks_set_boolean ("painter-canvas-ui", f->saved_painter);
  right_docks_set_boolean ("hide-docks", f->saved_hide);
  g_object_get (gimp->config, "single-window-mode", &single, NULL);
  if (single != f->saved_single)
    {
      /* With the last display gone only restoration to FALSE is valid here.
       * Multi-window conversion traverses an empty window list safely. */
      g_assert_false (f->saved_single);
      right_docks_set_boolean ("single-window-mode", FALSE);
    }
  drain ();
  g_object_get (gimp->config, "single-window-mode", &single,
                "hide-docks", &hide, "painter-canvas-ui", &painter, NULL);
  g_assert_cmpint (single, ==, f->saved_single);
  g_assert_cmpint (hide, ==, f->saved_hide);
  g_assert_cmpint (painter, ==, f->saved_painter);
}

static void
right_docks_native_notebook_transfer (void)
{
  RightDocksFixture f;
  guint switches;

  right_docks_begin (&f);
  right_docks_enable (&f, 1);
  right_docks_enable (&f, 0);
  switches = f.switches;
  right_docks_select (&f, 1);
  g_assert_cmpuint (f.switches, ==, switches + 1);
  /* Keep this first production expectation before the options/lifetime gates,
   * so the baseline establishes the missing columns caller causally. */
  right_docks_assert_canvas (&f, 1);
  right_docks_assert_options (&f, 1, 0, TRUE);
  right_docks_end (&f);
}

static void
right_docks_switch_repeat_and_restore (void)
{
  RightDocksFixture f;

  right_docks_begin (&f);
  right_docks_enable (&f, 1);
  right_docks_enable (&f, 0);
  for (guint round = 0; round < 3; round++)
    {
      for (guint selected = 0; selected < 2; selected++)
        {
          guint switches;
          guint parent_sets;
          GtkWidget *parent;

          right_docks_select (&f, selected);
          right_docks_assert_canvas (&f, selected);
          right_docks_assert_options (&f, selected, 0, TRUE);
          switches = f.switches;
          parent_sets = f.parent_sets;
          parent = gtk_widget_get_parent (f.columns);
          right_docks_select (&f, selected);
          g_assert_cmpuint (f.switches, ==, switches);
          g_assert_cmpuint (f.parent_sets, ==, parent_sets);
          g_assert_true (gtk_widget_get_parent (f.columns) == parent);
          right_docks_assert_canvas (&f, selected);
        }
      right_docks_select (&f, 0);
      right_docks_action (&f, "view", "view-painter-canvas-ui", FALSE);
      drain ();
      right_docks_assert_home (&f);
      right_docks_enable (&f, 0);
    }
  right_docks_end (&f);
}

static void
right_docks_hide_restore (void)
{
  RightDocksFixture f;

  right_docks_begin (&f);
  right_docks_enable (&f, 1);
  right_docks_enable (&f, 0);
  for (guint round = 0; round < 3; round++)
    {
      right_docks_assert_canvas (&f, 0);
      right_docks_action (&f, "windows", "windows-hide-docks", TRUE);
      drain ();
      right_docks_assert_home (&f);
      g_assert_false (gtk_widget_get_visible (f.columns));
      right_docks_select (&f, 1);
      right_docks_assert_home (&f);
      g_assert_false (gtk_widget_get_visible (f.columns));
      right_docks_action (&f, "windows", "windows-hide-docks", FALSE);
      drain ();
      right_docks_assert_canvas (&f, 1);
      right_docks_select (&f, 0);
    }
  right_docks_end (&f);
}

typedef struct
{
  RightDocksFixture *fixture;
  gboolean supersede;
  guint calls;
} RightDocksReentry;

static void
right_docks_reenter_parent (GtkWidget *widget,
                            GtkWidget *previous,
                            gpointer   data)
{
  RightDocksReentry *r = data;

  if (r->calls || !previous || gtk_widget_get_parent (widget))
    return;
  r->calls++;
  /* Deliberately synchronous. No nested main loop and no direct borrow calls:
   * this interrupts the production removal from the real GtkPaned home. */
  if (r->supersede)
    gimp_image_window_set_active_shell (r->fixture->image_window, r->fixture->shells[1]);
  else
    right_docks_action (r->fixture, "view", "view-painter-canvas-ui", FALSE);
}

static void
right_docks_reentry (gboolean supersede)
{
  RightDocksFixture f;
  RightDocksReentry r = { &f, supersede, 0 };
  gulong handler;
  gboolean configured;

  right_docks_begin (&f);
  if (supersede)
    {
      right_docks_enable (&f, 1);
      right_docks_enable (&f, 0);
      right_docks_action (&f, "windows", "windows-hide-docks", TRUE);
      drain ();
    }
  right_docks_assert_home (&f);
  handler = g_signal_connect (f.columns, "parent-set",
                              G_CALLBACK (right_docks_reenter_parent), &r);
  if (supersede)
    right_docks_action (&f, "windows", "windows-hide-docks", FALSE);
  else
    right_docks_action (&f, "view", "view-painter-canvas-ui", TRUE);
  drain ();
  g_assert_cmpuint (r.calls, ==, 1);
  g_assert_nonnull (f.columns);
  g_signal_handler_disconnect (f.columns, handler);
  right_docks_assert_parent (&f);
  g_object_get (gimp->config, "painter-canvas-ui", &configured, NULL);
  g_assert_cmpint (configured, ==, supersede);
  if (supersede)
    {
      g_assert_true (gimp_image_window_get_active_shell (f.image_window) == f.shells[1]);
      right_docks_assert_canvas (&f, 1);
      right_docks_assert_options (&f, 1, 0, TRUE);
    }
  else
    {
      g_assert_false (gimp_painter_canvas_ui_get_visible (f.shells[0]));
      right_docks_assert_home (&f);
    }
  right_docks_end (&f);
}

static void right_docks_cancel_on_remove (void) { right_docks_reentry (FALSE); }
static void right_docks_newer_selection_on_remove (void) { right_docks_reentry (TRUE); }

static void
right_docks_close_tab (gboolean active)
{
  RightDocksFixture f;
  guint closed = active ? 1 : 0;
  guint remaining = 1 - closed;

  right_docks_begin (&f);
  right_docks_enable (&f, 0);
  right_docks_enable (&f, 1);
  right_docks_assert_canvas (&f, 1);
  close_shell (f.shells[closed]);
  g_assert_null (f.shells[closed]);
  g_assert_cmpint (gimp_image_window_get_n_shells (f.image_window), ==, 1);
  right_docks_assert_canvas (&f, remaining);
  right_docks_assert_options (&f, remaining, 0, TRUE);
  right_docks_end (&f);
}

static void right_docks_close_active_tab (void) { right_docks_close_tab (TRUE); }
static void right_docks_close_inactive_tab (void) { right_docks_close_tab (FALSE); }

static void
right_docks_options_coexist (void)
{
  RightDocksFixture f;
  GtkWidget *button;

  right_docks_begin (&f);
  right_docks_enable (&f, 1);
  right_docks_enable (&f, 0);
  right_docks_assert_canvas (&f, 0);
  right_docks_assert_options (&f, 0, 0, TRUE);
  button = find (f.shells[0]->canvas, "painter-canvas-options");
  g_assert_true (GTK_IS_BUTTON (button));
  gtk_button_clicked (GTK_BUTTON (button));
  drain ();
  right_docks_assert_canvas (&f, 0);
  right_docks_assert_options (&f, 0, 0, FALSE);
  button = find (f.shells[0]->canvas, "painter-canvas-compact");
  g_assert_true (GTK_IS_BUTTON (button));
  gtk_button_clicked (GTK_BUTTON (button));
  drain ();
  right_docks_assert_canvas (&f, 0);
  right_docks_assert_options (&f, 0, 0, TRUE);
  button = find (f.shells[0]->canvas, "gimp-rect-select-tool");
  g_assert_true (GTK_IS_BUTTON (button));
  gtk_button_clicked (GTK_BUTTON (button));
  drain ();
  g_assert_true (gimp_context_get_tool (gimp_get_user_context (gimp)) == f.tools[1]);
  right_docks_assert_canvas (&f, 0);
  right_docks_assert_options (&f, 0, 1, TRUE);
  right_docks_select (&f, 1);
  right_docks_assert_canvas (&f, 1);
  right_docks_assert_options (&f, 1, 1, TRUE);
  right_docks_end (&f);
}

/* A global first opt-in must work when the next tab existed before that
 * action. Pre-enabling both shells would mask this production initialization
 * defect, so this case sends exactly one TRUE action, while A is selected. */
static void
right_docks_first_opt_in_switch (void)
{
  RightDocksFixture f;
  GtkWidget *options_home;
  gboolean options_had_home;
  gboolean configured;

  right_docks_begin (&f);
  g_object_get (gimp->config, "painter-canvas-ui", &configured, NULL);
  g_assert_false (configured);
  g_assert_false (gimp_painter_canvas_ui_get_visible (f.shells[0]));
  g_assert_false (gimp_painter_canvas_ui_get_visible (f.shells[1]));
  options_home = gtk_widget_get_parent (f.guis[0]);
  options_had_home = options_home != NULL;
  if (options_home)
    g_object_add_weak_pointer (G_OBJECT (options_home), (gpointer *) &options_home);

  right_docks_action (&f, "view", "view-painter-canvas-ui", TRUE);
  drain ();
  g_object_get (gimp->config, "painter-canvas-ui", &configured, NULL);
  g_assert_true (configured);
  g_assert_true (gimp_painter_canvas_ui_get_visible (f.shells[0]));

  /* No second TRUE action, per-shell visibility setter, or borrow helper.
   * Check initialization first, before the separately covered column caller. */
  right_docks_select (&f, 1);
  g_object_get (gimp->config, "painter-canvas-ui", &configured, NULL);
  g_assert_true (configured);
  g_assert_true (gimp_painter_canvas_ui_get_visible (f.shells[1]));
  right_docks_assert_canvas (&f, 1);
  right_docks_assert_options (&f, 1, 0, TRUE);
  g_assert_false (gtk_widget_is_ancestor (f.columns, f.shells[0]->canvas));
  g_assert_false (gtk_widget_is_ancestor (f.guis[0], f.shells[0]->canvas));

  right_docks_select (&f, 0);
  g_assert_true (gimp_painter_canvas_ui_get_visible (f.shells[0]));
  right_docks_assert_canvas (&f, 0);
  right_docks_assert_options (&f, 0, 0, TRUE);
  g_assert_false (gtk_widget_is_ancestor (f.columns, f.shells[1]->canvas));
  g_assert_false (gtk_widget_is_ancestor (f.guis[0], f.shells[1]->canvas));

  right_docks_action (&f, "view", "view-painter-canvas-ui", FALSE);
  drain ();
  g_object_get (gimp->config, "painter-canvas-ui", &configured, NULL);
  g_assert_false (configured);
  g_assert_false (gimp_painter_canvas_ui_get_visible (f.shells[0]));
  right_docks_assert_home (&f);
  g_assert_nonnull (f.guis[0]);
  g_assert_true (gimp_tools_get_tool_options_gui (f.tools[0]->tool_options) == f.guis[0]);
  g_assert_false (gimp_painter_compact_options_get (f.guis[0]));
  for (guint i = 0; i < G_N_ELEMENTS (f.shells); i++)
    g_assert_false (gtk_widget_is_ancestor (f.guis[0], f.shells[i]->canvas));
  if (options_had_home)
    g_assert_nonnull (options_home);
  g_assert_true (gtk_widget_get_parent (f.guis[0]) == options_home);
  if (options_home)
    g_object_remove_weak_pointer (G_OBJECT (options_home), (gpointer *) &options_home);
  right_docks_end (&f);
}
