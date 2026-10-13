/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Additive real-GIMP mode-conversion coverage. Include after the frozen
 * test-painter-right-docks.inc. Never invokes Painter's attach/reconcile,
 * suspend/resume, or width helpers, and never reads private C++ state. */
#include "widgets/gimpdockcontainer.h"
#include "widgets/gimpdockwindow.h"
#include "widgets/gimpsessionmanaged.h"
#include "widgets/gimpsessioninfo-aux.h"
#include "widgets/gimptoggleaction.h"

typedef struct
{
  GObject *object;
} RightConversionWeak;

typedef struct
{
  GtkWidget *widget;
  guint children;
  gint selected;
  GtkWidget *selected_widget; /* Identity only; also weakly observed in tree. */
} RightConversionNode;

static void
right_conversion_weak_free (gpointer data)
{
  RightConversionWeak *weak = data;

  if (weak->object)
    g_object_remove_weak_pointer (weak->object, (gpointer *) &weak->object);
  g_free (weak);
}

static RightConversionWeak *
right_conversion_observe (GPtrArray *lifetimes,
                          gpointer   object)
{
  RightConversionWeak *weak;

  g_assert_true (G_IS_OBJECT (object));
  for (guint i = 0; i < lifetimes->len; i++)
    {
      weak = g_ptr_array_index (lifetimes, i);
      if (weak->object == object)
        return weak;
    }
  weak = g_new0 (RightConversionWeak, 1);
  weak->object = object;
  g_object_add_weak_pointer (weak->object, (gpointer *) &weak->object);
  g_ptr_array_add (lifetimes, weak);
  return weak;
}

static void
right_conversion_node_free (gpointer data)
{
  RightConversionNode *node = data;

  if (node->widget)
    g_object_remove_weak_pointer (G_OBJECT (node->widget),
                                  (gpointer *) &node->widget);
  g_free (node);
}

static GPtrArray *
right_conversion_snapshot (GPtrArray *contents)
{
  GPtrArray *tree = g_ptr_array_new_with_free_func (right_conversion_node_free);

  for (guint i = 0; i < contents->len; i++)
    {
      RightConversionNode *node = g_new0 (RightConversionNode, 1);

      node->widget = g_ptr_array_index (contents, i);
      node->selected = -1;
      if (GIMP_IS_DOCK (node->widget))
        node->children = g_list_length (gimp_dock_get_dockbooks (GIMP_DOCK (node->widget)));
      else if (GIMP_IS_DOCKBOOK (node->widget))
        {
          GtkNotebook *book = GTK_NOTEBOOK (node->widget);

          node->children = gtk_notebook_get_n_pages (book);
          node->selected = gtk_notebook_get_current_page (book);
          g_assert_cmpint (node->selected, >=, 0);
          node->selected_widget = gtk_notebook_get_nth_page (book, node->selected);
        }
      else
        g_assert_true (GIMP_IS_DOCKABLE (node->widget));
      g_object_add_weak_pointer (G_OBJECT (node->widget),
                                 (gpointer *) &node->widget);
      g_ptr_array_add (tree, node);
    }
  return tree;
}

static GPtrArray *
right_conversion_flatten (GList *docks)
{
  GPtrArray *actual = g_ptr_array_new ();

  for (GList *d = docks; d; d = d->next)
    {
      g_ptr_array_add (actual, d->data);
      for (GList *b = gimp_dock_get_dockbooks (d->data); b; b = b->next)
        {
          GtkNotebook *book = b->data;

          g_ptr_array_add (actual, book);
          for (gint page = 0; page < gtk_notebook_get_n_pages (book); page++)
            g_ptr_array_add (actual, gtk_notebook_get_nth_page (book, page));
        }
    }
  return actual;
}

/* Strict per-origin native sequence. This remains an exact equality check,
 * including every book/tab and selected page, not a subsequence allowance. */
static void
right_conversion_assert_tree (GPtrArray *tree,
                              GList     *docks)
{
  GPtrArray *actual = right_conversion_flatten (docks);

  g_assert_cmpuint (actual->len, ==, tree->len);
  for (guint i = 0; i < tree->len; i++)
    {
      RightConversionNode *node = g_ptr_array_index (tree, i);

      g_assert_nonnull (node->widget);
      g_assert_true (g_ptr_array_index (actual, i) == node->widget);
      if (GIMP_IS_DOCK (node->widget))
        g_assert_cmpuint (g_list_length (gimp_dock_get_dockbooks (GIMP_DOCK (node->widget))),
                          ==, node->children);
      else if (GIMP_IS_DOCKBOOK (node->widget))
        {
          GtkNotebook *book = GTK_NOTEBOOK (node->widget);

          g_assert_cmpuint (gtk_notebook_get_n_pages (book), ==, node->children);
          g_assert_cmpint (gtk_notebook_get_current_page (book), ==, node->selected);
          g_assert_true (gtk_notebook_get_nth_page (book, node->selected) ==
                         node->selected_widget);
          for (gint page = 0; page < gtk_notebook_get_n_pages (book); page++)
            g_assert_true (gimp_dockable_get_dockbook (GIMP_DOCKABLE (
                             gtk_notebook_get_nth_page (book, page))) ==
                           GIMP_DOCKBOOK (book));
        }
    }
  g_ptr_array_unref (actual);
}

/* Native mode conversion merges ALL open dock windows. Accept only the
 * exact pre-operation known inventory, never arbitrary newly found extras.
 * Each origin's docks stay on one side and retain their complete sequence.
 * The original fixture's right docks must specifically return to the right. */
static void
right_conversion_assert_merged (GPtrArray *tree,
                                GPtrArray *other_trees,
                                GList     *left,
                                GList     *right)
{
  GList *both = g_list_concat (g_list_copy (left), g_list_copy (right));
  GPtrArray *actual = right_conversion_flatten (both);
  GHashTable *expected = g_hash_table_new (g_direct_hash, g_direct_equal);
  GHashTable *seen = g_hash_table_new (g_direct_hash, g_direct_equal);
  guint expected_nodes = 0;

  for (guint group = 0; group <= other_trees->len; group++)
    {
      GPtrArray *origin = group ? g_ptr_array_index (other_trees, group - 1) : tree;
      GHashTable *origin_docks = g_hash_table_new (g_direct_hash, g_direct_equal);
      GList *filtered = NULL;
      gint side = -1;

      for (guint i = 0; i < origin->len; i++)
        {
          RightConversionNode *node = g_ptr_array_index (origin, i);

          g_assert_nonnull (node->widget);
          g_assert_true (g_hash_table_add (expected, node->widget));
          expected_nodes++;
          if (GIMP_IS_DOCK (node->widget))
            g_hash_table_add (origin_docks, node->widget);
        }
      for (GList *i = both; i; i = i->next)
        if (g_hash_table_contains (origin_docks, i->data))
          {
            gint current_side = g_list_find (right, i->data) ? 1 : 0;

            if (side < 0)
              side = current_side;
            g_assert_cmpint (current_side, ==, side);
            if (! group)
              g_assert_cmpint (current_side, ==, 1);
            filtered = g_list_append (filtered, i->data);
          }
      right_conversion_assert_tree (origin, filtered);
      g_list_free (filtered);
      g_hash_table_unref (origin_docks);
    }
  g_assert_cmpuint (actual->len, ==, expected_nodes);
  for (guint i = 0; i < actual->len; i++)
    {
      gpointer widget = g_ptr_array_index (actual, i);

      g_assert_true (g_hash_table_contains (expected, widget));
      g_assert_true (g_hash_table_add (seen, widget));
    }
  g_assert_cmpuint (g_hash_table_size (seen), ==, g_hash_table_size (expected));
  g_test_message ("Merged native inventory: %u original nodes, %u other "
                  "origins, %u total nodes; all identities unique",
                  tree->len, other_trees->len, actual->len);
  g_hash_table_unref (seen);
  g_hash_table_unref (expected);
  g_ptr_array_unref (actual);
  g_list_free (both);
}

static gint
right_conversion_aux_width (GimpImageWindow *image_window)
{
  GList *aux = gimp_session_managed_get_aux_info (GIMP_SESSION_MANAGED (image_window));
  gint width = 0;
  guint matches = 0;

  for (GList *i = aux; i; i = i->next)
    {
      GimpSessionInfoAux *value = i->data;

      if (! strcmp (value->name, "right-docks-width"))
        {
          gchar *end = NULL;
          gint64 parsed = g_ascii_strtoll (value->value, &end, 10);

          g_assert_true (end != value->value && *end == '\0');
          g_assert_cmpint (parsed, >, 0);
          g_assert_cmpint (parsed, <=, G_MAXINT);
          width = (gint) parsed;
          matches++;
        }
    }
  g_list_free_full (aux, (GDestroyNotify) gimp_session_info_aux_free);
  g_assert_cmpuint (matches, ==, 1);
  return width;
}

/* Unlike the frozen initial settler, this only observes current geometry.
 * It cannot reset the saved home width or force a borrowed attachment. */
typedef struct
{
  GtkWidget *widgets[4];
  GtkAllocation allocations[4];
  guint samples;
  guint stable_frames;
} RightConversionGeometry;

static void
right_conversion_frame (GdkFrameClock *clock,
                        gpointer       data)
{
  RightConversionGeometry *geometry = data;
  gboolean same = geometry->samples > 0;

  (void) clock;
  for (guint i = 0; i < G_N_ELEMENTS (geometry->widgets); i++)
    {
      GtkAllocation current;
      GtkAllocation previous = geometry->allocations[i];
      GtkWidget *widget = geometry->widgets[i];

      if (! widget)
        {
          same = FALSE;
          continue;
        }
      gtk_widget_get_allocation (widget, &current);
      same &= right_docks_drawable (widget) &&
              current.x == previous.x && current.y == previous.y &&
              current.width == previous.width && current.height == previous.height;
      geometry->allocations[i] = current;
    }
  geometry->samples++;
  geometry->stable_frames = same ? geometry->stable_frames + 1 : 0;
}

static void
right_conversion_settle (GtkWidget *one,
                         GtkWidget *two,
                         GtkWidget *three,
                         GtkWidget *four)
{
  RightConversionGeometry geometry = { { one, two, three, four }, { { 0 } }, 0, 0 };
  GdkFrameClock *clock;
  gulong handler;
  gint64 deadline;

  for (guint i = 0; i < G_N_ELEMENTS (geometry.widgets); i++)
    {
      g_assert_nonnull (geometry.widgets[i]);
      g_object_add_weak_pointer (G_OBJECT (geometry.widgets[i]),
                                 (gpointer *) &geometry.widgets[i]);
    }
  right_docks_wait_drawable (one);
  clock = gtk_widget_get_frame_clock (one);
  g_assert_nonnull (clock);
  g_object_ref (clock); /* Frame-clock lease, never a tested widget lease. */
  handler = g_signal_connect (clock, "after-paint", G_CALLBACK (right_conversion_frame),
                              &geometry);
  deadline = g_get_monotonic_time () + 5 * G_TIME_SPAN_SECOND;
  while (geometry.stable_frames < 3 && g_get_monotonic_time () < deadline)
    {
      gdk_frame_clock_request_phase (clock, GDK_FRAME_CLOCK_PHASE_PAINT);
      gimp_test_run_temp_mainloop (10);
    }
  g_signal_handler_disconnect (clock, handler);
  g_object_unref (clock);
  for (guint i = 0; i < G_N_ELEMENTS (geometry.widgets); i++)
    {
      g_assert_nonnull (geometry.widgets[i]);
      g_object_remove_weak_pointer (G_OBJECT (geometry.widgets[i]),
                                    (gpointer *) &geometry.widgets[i]);
    }
  g_assert_cmpuint (geometry.stable_frames, >=, 3);
}

static void
right_conversion_assert_selected (RightDocksFixture *f)
{
  GimpImageWindow *current;
  GtkNotebook *notebook;

  g_assert_nonnull (f->shells[0]);
  g_assert_nonnull (f->shells[1]);
  g_assert_true (gimp_context_get_display (gimp_get_user_context (gimp)) ==
                 f->shells[1]->display);
  current = gimp_display_shell_get_window (f->shells[1]);
  g_assert_nonnull (current);
  g_assert_true (gimp_image_window_get_active_shell (current) == f->shells[1]);
  notebook = GTK_NOTEBOOK (gtk_widget_get_parent (GTK_WIDGET (f->shells[1])));
  g_assert_true (GTK_IS_NOTEBOOK (notebook));
  g_assert_true (gtk_notebook_get_nth_page (notebook,
                   gtk_notebook_get_current_page (notebook)) == GTK_WIDGET (f->shells[1]));
}

static void
right_conversion_assert_options (RightDocksFixture *f)
{
  guint owners = 0;

  right_conversion_assert_selected (f);
  g_assert_nonnull (f->guis[0]);
  g_assert_true (gimp_tools_get_tool_options_gui (f->tools[0]->tool_options) == f->guis[0]);
  g_assert_true (gimp_painter_compact_options_get (f->guis[0]));
  for (guint i = 0; i < G_N_ELEMENTS (f->shells); i++)
    if (gtk_widget_is_ancestor (f->guis[0], f->shells[i]->canvas))
      owners++;
  g_assert_cmpuint (owners, ==, 1);
  g_assert_true (gtk_widget_is_ancestor (f->guis[0], f->shells[1]->canvas));
  right_docks_overlay_root (f->guis[0], f->shells[1]->canvas);
}

static void
right_conversion_assert_mode (RightDocksFixture *f,
                              gboolean           single)
{
  GimpUIManager *manager = gimp_test_utils_get_ui_manager (gimp);
  GimpAction *action;
  gboolean actual, painter, hidden;

  right_conversion_assert_selected (f);
  gimp_ui_manager_update (manager, f->shells[1]->display);
  action = gimp_ui_manager_find_action (manager, "windows", "windows-use-single-window-mode");
  g_assert_true (GIMP_IS_TOGGLE_ACTION (action));
  g_assert_cmpint (gimp_toggle_action_get_active (GIMP_TOGGLE_ACTION (action)), ==, single);
  g_object_get (gimp->config, "single-window-mode", &actual,
                "painter-canvas-ui", &painter, "hide-docks", &hidden, NULL);
  g_assert_cmpint (actual, ==, single);
  g_assert_true (painter);
  g_assert_false (hidden);
  action = gimp_ui_manager_find_action (manager, "view", "view-painter-canvas-ui");
  g_assert_true (GIMP_IS_TOGGLE_ACTION (action));
  g_assert_true (gimp_toggle_action_get_active (GIMP_TOGGLE_ACTION (action)));
  g_assert_true (gimp_painter_canvas_ui_get_visible (f->shells[1]));
}

static void
right_conversion_mode_action (RightDocksFixture *f,
                              gboolean           single)
{
  GimpUIManager *manager = gimp_test_utils_get_ui_manager (gimp);
  gboolean previous;

  right_conversion_assert_selected (f);
  g_object_get (gimp->config, "single-window-mode", &previous, NULL);
  g_assert_cmpint (previous, !=, single); /* Never replay unchanged config. */
  gimp_ui_manager_update (manager, f->shells[1]->display);
  g_assert_true (gimp_ui_manager_toggle_action (manager, "windows",
                                               "windows-use-single-window-mode", single));
  drain ();
  right_conversion_assert_mode (f, single);
}

static GtkWidget *
right_conversion_observe_window (GPtrArray      *lifetimes,
                                 GimpImageWindow *image_window)
{
  GtkWidget *columns = GTK_WIDGET (gimp_image_window_get_right_docks (image_window));

  right_conversion_observe (lifetimes, image_window);
  right_conversion_observe (lifetimes, columns);
  if (GTK_IS_PANED (gtk_widget_get_parent (columns)))
    right_conversion_observe (lifetimes, gtk_widget_get_parent (columns));
  return columns;
}

static void
right_conversion_count_style_update (GtkWidget *widget,
                                     gpointer   data)
{
  guint *count = data;

  (void) widget;
  (*count)++;
}

static void
right_conversion_assert_multi (RightDocksFixture *f,
                               GPtrArray         *tree,
                               GPtrArray         *lifetimes,
                               gint               logical_width,
                               gint               overlay_width)
{
  GimpImageWindow *image_windows[2];
  GimpDockWindow *dock_window;
  GtkWidget *dock_columns;
  GList *docks;
  gint default_width, actual_width, minimum_width;
  guint style_updates = 0;
  gulong style_handler;

  right_conversion_assert_mode (f, FALSE);
  for (guint i = 0; i < G_N_ELEMENTS (image_windows); i++)
    {
      GtkWidget *columns, *home;

      image_windows[i] = gimp_display_shell_get_window (f->shells[i]);
      columns = right_conversion_observe_window (lifetimes, image_windows[i]);
      home = gtk_widget_get_parent (columns);
      g_assert_true (GTK_IS_PANED (home));
      g_assert_true (gtk_paned_get_child2 (GTK_PANED (home)) == columns);
      g_assert_false (gtk_widget_get_visible (columns));
      g_assert_null (gimp_dock_columns_get_docks (GIMP_DOCK_COLUMNS (columns)));
      g_assert_cmpint (gimp_image_window_get_n_shells (image_windows[i]), ==, 1);
      for (guint j = 0; j < G_N_ELEMENTS (f->shells); j++)
        g_assert_false (gtk_widget_is_ancestor (columns, f->shells[j]->canvas));
    }
  g_assert_true (image_windows[0] != image_windows[1]);
  /* Resolve the real dock window from a surviving native dock, never from
   * an assumed factory order, widget name, or former columns identity. */
  g_assert_nonnull (((RightConversionNode *) g_ptr_array_index (tree, 0))->widget);
  dock_window = gimp_dock_window_from_dock (GIMP_DOCK (
                 ((RightConversionNode *) g_ptr_array_index (tree, 0))->widget));
  g_assert_nonnull (dock_window);
  right_conversion_observe (lifetimes, dock_window);
  dock_columns = gtk_widget_get_ancestor (GTK_WIDGET (gimp_dockbook_get_dock (
                    GIMP_DOCKBOOK (f->books[0]))), GIMP_TYPE_DOCK_COLUMNS);
  g_assert_nonnull (dock_columns);
  right_conversion_observe (lifetimes, dock_columns);
  docks = gimp_dock_container_get_docks (GIMP_DOCK_CONTAINER (dock_window));
  right_conversion_assert_tree (tree, docks);
  for (GList *i = docks; i; i = i->next)
    {
      g_assert_true (gimp_dock_window_from_dock (i->data) == dock_window);
      g_assert_true (gtk_widget_is_ancestor (i->data, dock_columns));
    }
  g_list_free (docks);
  right_conversion_settle (GTK_WIDGET (image_windows[0]), GTK_WIDGET (image_windows[1]),
                           GTK_WIDGET (dock_window), dock_columns);
  gtk_window_get_default_size (GTK_WINDOW (dock_window), &default_width, NULL);
  gtk_window_get_size (GTK_WINDOW (dock_window), &actual_width, NULL);
  gtk_widget_get_preferred_width (GTK_WIDGET (dock_window), &minimum_width, NULL);
  g_test_message ("Conversion width: saved home=%d, old overlay columns=%d, "
                  "native default=%d, settled=%d, native minimum=%d",
                  logical_width, overlay_width, default_width, actual_width, minimum_width);
  g_assert_cmpint (overlay_width, !=, logical_width); /* Distinguishing precondition. */
  /* GtkWindow's default width is not a retained resize request: native
   * GimpDockWindow style updates legitimately reset it to -1. The actual
   * settled width is the unchanged logical-home oracle; 449 cannot pass
   * when the preborrow width was 640 and the native minimum is only 449.
   */
  g_assert_cmpint (actual_width, ==, MAX (logical_width, minimum_width));
  right_conversion_assert_options (f);

  /* Exercise the native virtual style handler twice on the REAL converted
   * dock window. This emits the same public GTK signal that clears default
   * widths; neither the test nor a helper resizes the window afterward.
   */
  style_handler = g_signal_connect_after (dock_window, "style-updated",
                                          G_CALLBACK (right_conversion_count_style_update),
                                          &style_updates);
  for (guint round = 0; round < 2; round++)
    {
      guint before = style_updates;

      g_signal_emit_by_name (dock_window, "style-updated");
      g_assert_cmpuint (style_updates, >, before);
      right_conversion_settle (GTK_WIDGET (image_windows[0]), GTK_WIDGET (image_windows[1]),
                               GTK_WIDGET (dock_window), dock_columns);
      gtk_window_get_default_size (GTK_WINDOW (dock_window), &default_width, NULL);
      gtk_window_get_size (GTK_WINDOW (dock_window), &actual_width, NULL);
      gtk_widget_get_preferred_width (GTK_WIDGET (dock_window), &minimum_width, NULL);
      g_test_message ("Converted dock after style update %u: callbacks=%u, "
                      "default=%d, settled=%d, saved home=%d, minimum=%d",
                      round + 1, style_updates, default_width, actual_width,
                      logical_width, minimum_width);
      g_assert_cmpint (actual_width, ==, MAX (logical_width, minimum_width));
      right_conversion_assert_options (f);
    }
  g_signal_handler_disconnect (dock_window, style_handler);
}

static GtkWidget *
right_conversion_assert_single (RightDocksFixture *f,
                                GPtrArray         *tree,
                                GPtrArray         *lifetimes,
                                GPtrArray         *other_trees)
{
  GimpImageWindow *image_window;
  GtkWidget *columns;
  GList *windows;

  right_conversion_assert_mode (f, TRUE);
  image_window = gimp_display_shell_get_window (f->shells[1]);
  g_assert_true (gimp_display_shell_get_window (f->shells[0]) == image_window);
  g_assert_cmpint (gimp_image_window_get_n_shells (image_window), ==, 2);
  windows = gimp_get_image_windows (gimp);
  g_assert_cmpuint (g_list_length (windows), ==, 1);
  g_assert_true (windows->data == image_window);
  g_list_free (windows);
  columns = right_conversion_observe_window (lifetimes, image_window);
  if (other_trees)
    right_conversion_assert_merged (tree, other_trees,
                                    gimp_dock_columns_get_docks (
                                      gimp_image_window_get_left_docks (image_window)),
                                    gimp_dock_columns_get_docks (GIMP_DOCK_COLUMNS (columns)));
  else
    right_conversion_assert_tree (tree, gimp_dock_columns_get_docks (GIMP_DOCK_COLUMNS (columns)));
  g_assert_true (gtk_widget_is_ancestor (columns, f->shells[1]->canvas));
  g_assert_false (gtk_widget_is_ancestor (columns, f->shells[0]->canvas));
  right_docks_overlay_root (columns, f->shells[1]->canvas);
  right_conversion_assert_options (f);
  g_assert_false (gtk_widget_is_ancestor (f->guis[0], columns));
  g_assert_false (gtk_widget_is_ancestor (columns, f->guis[0]));
  right_conversion_settle (GTK_WIDGET (image_window), f->shells[1]->canvas,
                           columns, f->guis[0]);
  return columns;
}

static gboolean
right_conversion_all_finalized (GPtrArray *lifetimes,
                                GPtrArray *tree)
{
  for (guint i = 0; i < lifetimes->len; i++)
    if (((RightConversionWeak *) g_ptr_array_index (lifetimes, i))->object)
      return FALSE;
  for (guint i = 0; i < tree->len; i++)
    if (((RightConversionNode *) g_ptr_array_index (tree, i))->widget)
      return FALSE;
  return TRUE;
}

/* Topology-aware cleanup: f's original window/columns may have legitimately
 * finalized during merging. Only the surviving shells are closure targets;
 * every original and reconstructed observed owner must ultimately finalize. */
static void
right_conversion_end (RightDocksFixture *f,
                      GPtrArray         *lifetimes,
                      GPtrArray         *tree)
{
  gboolean single, hide, painter;
  gint64 deadline;

  for (guint i = 0; i < G_N_ELEMENTS (f->shells); i++)
    if (f->shells[i])
      close_shell (f->shells[i]);
  deadline = g_get_monotonic_time () + 3 * G_TIME_SPAN_SECOND;
  while (! right_conversion_all_finalized (lifetimes, tree) &&
         g_get_monotonic_time () < deadline)
    gimp_test_run_temp_mainloop (10);
  g_assert_true (right_conversion_all_finalized (lifetimes, tree));
  g_assert_null (f->shells[0]);
  g_assert_null (f->shells[1]);
  g_assert_null (f->image_window);
  g_assert_null (f->columns);
  g_assert_null (f->home);
  for (guint i = 0; i < G_N_ELEMENTS (f->guis); i++)
    if (f->guis[i])
      g_object_remove_weak_pointer (G_OBJECT (f->guis[i]), (gpointer *) &f->guis[i]);
  g_ptr_array_unref (tree);
  g_ptr_array_unref (lifetimes);
  g_ptr_array_unref (f->contents);
  right_docks_restore_groups (f);
  right_docks_set_boolean ("painter-canvas-ui", f->saved_painter);
  right_docks_set_boolean ("hide-docks", f->saved_hide);
  g_object_get (gimp->config, "single-window-mode", &single, NULL);
  if (single != f->saved_single)
    {
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
right_docks_window_mode_roundtrip (void)
{
  RightDocksFixture f;
  GPtrArray *tree, *lifetimes, *split_dock_windows, *other_trees;
  RightConversionWeak *default_dock = NULL;
  GList *left_docks;
  GtkWidget *columns;
  gint logical_width, overlay_width;
  guint native_default_windows = 0;

  right_docks_begin (&f);
  /* Establish a deliberately wide HOME before Painter is enabled. The
   * runtime must demonstrate the later overlay allocation differs; no
   * expected width is resnapshotted from that temporary allocation. */
  gtk_paned_set_position (GTK_PANED (f.home),
                          MAX (1, gtk_widget_get_allocated_width (f.home) - 640));
  right_docks_settle_initial_geometry (&f);
  f.divider = gtk_paned_get_position (GTK_PANED (f.home));
  logical_width = gtk_widget_get_allocated_width (f.home) - f.divider;
  g_assert_cmpint (right_conversion_aux_width (f.image_window), ==, logical_width);
  gtk_notebook_set_current_page (GTK_NOTEBOOK (f.books[0]), 1);
  gtk_notebook_set_current_page (GTK_NOTEBOOK (f.books[1]), 0);
  drain ();
  tree = right_conversion_snapshot (f.contents);
  lifetimes = g_ptr_array_new_with_free_func (right_conversion_weak_free);
  right_conversion_observe_window (lifetimes, f.image_window);
  left_docks = gimp_dock_columns_get_docks (gimp_image_window_get_left_docks (f.image_window));
  g_test_message ("Native no-explicit-width conversion input: %u left docks",
                  g_list_length (left_docks));
  other_trees = g_ptr_array_new_with_free_func ((GDestroyNotify) g_ptr_array_unref);
  if (left_docks)
    {
      GPtrArray *contents = right_conversion_flatten (left_docks);

      default_dock = right_conversion_observe (lifetimes, left_docks->data);
      g_ptr_array_add (other_trees, right_conversion_snapshot (contents));
      for (guint node = 0; node < contents->len; node++)
        right_conversion_observe (lifetimes, g_ptr_array_index (contents, node));
      g_ptr_array_unref (contents);
    }
  for (guint i = 0; i < G_N_ELEMENTS (f.shells); i++)
    {
      right_conversion_observe (lifetimes, f.shells[i]);
      right_conversion_observe (lifetimes, f.shells[i]->display);
    }

  right_docks_enable (&f, 1);
  columns = right_conversion_assert_single (&f, tree, lifetimes, NULL);
  overlay_width = gtk_widget_get_allocated_width (columns);
  g_assert_cmpint (overlay_width, !=, logical_width);
  g_assert_cmpint (right_conversion_aux_width (gimp_display_shell_get_window (f.shells[1])),
                   ==, logical_width);

  /* Startup/session-restored separate windows are legitimate inputs to
   * the later native merge. Capture them BEFORE conversion, including all
   * selections, instead of discovering/allowing extra nodes after failure. */
  for (GList *i = gimp_dialog_factory_get_open_dialogs (gimp_dialog_factory_get_singleton ());
       i; i = i->next)
    if (GIMP_IS_DOCK_WINDOW (i->data))
      {
        GimpDockWindow *native = GIMP_DOCK_WINDOW (i->data);
        GList *docks = gimp_dock_container_get_docks (GIMP_DOCK_CONTAINER (native));
        GPtrArray *contents = right_conversion_flatten (docks);
        gint before_width, actual_width, actual_height, minimum_width, default_width;
        guint style_updates = 0;
        gulong handler;

        g_test_message ("Existing native dock-window input: %u docks, %u tree nodes",
                        g_list_length (docks), contents->len);
        right_conversion_observe (lifetimes, i->data);
        g_ptr_array_add (other_trees, right_conversion_snapshot (contents));
        for (guint node = 0; node < contents->len; node++)
          right_conversion_observe (lifetimes, g_ptr_array_index (contents, node));
        /* Show only this ALREADY existing real native window. Native
         * startup/session restoration supplies its size; no width is set
         * by the test, and no additional window or dock is constructed. */
        gtk_widget_show (GTK_WIDGET (native));
        right_conversion_settle (GTK_WIDGET (native),
                                 docks ? GTK_WIDGET (docks->data) : GTK_WIDGET (native),
                                 f.shells[1]->canvas,
                                 GTK_WIDGET (gimp_display_shell_get_window (f.shells[1])));
        gtk_window_get_default_size (GTK_WINDOW (native), &default_width, NULL);
        gtk_window_get_size (GTK_WINDOW (native), &before_width, &actual_height);
        g_assert_cmpint (default_width, ==, -1);
        g_assert_cmpint (before_width, >, 0);
        g_assert_cmpint (actual_height, >, 0);
        handler = g_signal_connect_after (native, "style-updated",
                                          G_CALLBACK (right_conversion_count_style_update),
                                          &style_updates);
        for (guint round = 0; round < 2; round++)
          {
            guint before = style_updates;

            g_signal_emit_by_name (native, "style-updated");
            g_assert_cmpuint (style_updates, >, before);
            right_conversion_settle (GTK_WIDGET (native),
                                     docks ? GTK_WIDGET (docks->data) : GTK_WIDGET (native),
                                     f.shells[1]->canvas,
                                     GTK_WIDGET (gimp_display_shell_get_window (f.shells[1])));
            gtk_window_get_default_size (GTK_WINDOW (native), &default_width, NULL);
            gtk_window_get_size (GTK_WINDOW (native), &actual_width, &actual_height);
            gtk_widget_get_preferred_width (GTK_WIDGET (native), &minimum_width, NULL);
            g_test_message ("Existing native default window after style %u: "
                            "default=%d, original settled=%d, current=%dx%d, minimum=%d",
                            round + 1, default_width, before_width, actual_width,
                            actual_height, minimum_width);
            g_assert_cmpint (default_width, ==, -1);
            g_assert_cmpint (actual_width, ==, before_width);
            g_assert_cmpint (actual_width, >=, minimum_width);
            g_assert_cmpint (actual_height, >, 0);
          }
        g_signal_handler_disconnect (native, handler);
        right_conversion_assert_tree (g_ptr_array_index (other_trees, other_trees->len - 1),
                                      docks);
        native_default_windows++;
        g_ptr_array_unref (contents);
        g_list_free (docks);
      }
  g_test_message ("Existing native no-explicit-default windows checked: %u",
                  native_default_windows);
  if (! native_default_windows)
    g_test_message ("No existing separate dock window: native-default window "
                    "runtime proof is absent in this fixture");
  right_conversion_assert_options (&f);

  right_conversion_mode_action (&f, FALSE);
  right_conversion_assert_multi (&f, tree, lifetimes, logical_width, overlay_width);
  if (default_dock)
    {
      GimpDockWindow *native;
      gint default_width, actual_width, actual_height, minimum_width;

      g_assert_nonnull (default_dock->object);
      native = gimp_dock_window_from_dock (GIMP_DOCK (default_dock->object));
      g_assert_nonnull (native);
      right_conversion_observe (lifetimes, native);
      /* This is an already-present native left dock, supplied with logical
       * width -1 by the ordinary configurer. No fixture window is created. */
      g_signal_emit_by_name (native, "style-updated");
      right_conversion_settle (GTK_WIDGET (native), GTK_WIDGET (default_dock->object),
                               f.shells[1]->canvas,
                               GTK_WIDGET (gimp_display_shell_get_window (f.shells[1])));
      gtk_window_get_default_size (GTK_WINDOW (native), &default_width, NULL);
      gtk_window_get_size (GTK_WINDOW (native), &actual_width, &actual_height);
      gtk_widget_get_preferred_width (GTK_WIDGET (native), &minimum_width, NULL);
      g_test_message ("Native no-explicit-width window: style default=%d, "
                      "settled=%dx%d, minimum=%d", default_width,
                      actual_width, actual_height, minimum_width);
      g_assert_cmpint (default_width, ==, -1); /* Unmodified native style handler. */
      g_assert_cmpint (actual_width, >, 0);
      g_assert_cmpint (actual_width, >=, minimum_width);
      g_assert_cmpint (actual_height, >, 0);
      right_conversion_assert_options (&f);
    }
  else
    g_test_message ("No left docks exist: no-explicit-width branch has no "
                    "runtime coverage in this fixture (source-preservation check only)");
  split_dock_windows = g_ptr_array_new ();
  for (GList *i = gimp_dialog_factory_get_open_dialogs (gimp_dialog_factory_get_singleton ());
       i; i = i->next)
    if (GIMP_IS_DOCK_WINDOW (i->data))
      g_ptr_array_add (split_dock_windows, right_conversion_observe (lifetimes, i->data));
  g_assert_cmpuint (split_dock_windows->len, >, 0);

  right_conversion_mode_action (&f, TRUE);
  right_conversion_assert_single (&f, tree, lifetimes, other_trees);
  for (guint i = 0; i < split_dock_windows->len; i++)
    g_assert_null (((RightConversionWeak *) g_ptr_array_index (split_dock_windows, i))->object);
  g_ptr_array_unref (split_dock_windows);
  right_conversion_end (&f, lifetimes, tree);
  g_ptr_array_unref (other_trees);
}
