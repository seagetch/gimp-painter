/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Registered independent-toolbar entry points, not test-created popup hosts. */
#include "widgets/gimpcontainerpopup.h"
#include "widgets/gimpcontainertreestore.h"
#include "core/gimpdatafactory.h"

typedef struct {
  GimpToolOptions *options;
  GtkWidget *window, *gui, *button, *body, *row;
  const gchar *enabled;
  gboolean previous;
} StandardToggleFixture;

static GtkWidget *
standard_toggle_visible (GType type, const gchar *name)
{
  GtkWidget *result = NULL;
  GList *windows = gtk_window_list_toplevels ();
  for (GList *i = windows; i; i = i->next)
    if (G_TYPE_CHECK_INSTANCE_TYPE (i->data, type) &&
        gtk_widget_get_visible (i->data) &&
        (!name || !g_strcmp0 (gtk_widget_get_name (i->data), name))) {
      g_assert_null (result); result = i->data;
    }
  g_list_free (windows);
  return result;
}

static StandardToggleFixture
standard_toggle_fixture_for (const gchar *id, GimpToolOptions *options)
{
  StandardToggleFixture f = {0};
  f.options = options;
  f.enabled = !strcmp (id, "jitter") ? "use-jitter" :
              !strcmp (id, "texture") ? "use-texture" : "use-smoothing";
  g_object_get (f.options, f.enabled, &f.previous, NULL);
  GimpToolInfo *tool = info ("gimp-paintbrush-tool");
  if (options == tool->tool_options)
    f.gui = gimp_tools_create_painter_toolbar_gui (f.options);
  else {
    /* The public dispatcher deliberately admits canonical options only.
     * Private model-lifetime tests call its exact registered factory, never
     * overwrite tool_info->tool_options or dispose the shared model. */
    GimpToolOptionsGUIFunc factory = g_object_get_data (G_OBJECT (tool),
                                                       "gimp-tool-options-gui-horizontal-func");
    g_assert_nonnull (factory);
    f.gui = factory (options);
  }
  g_assert_nonnull (f.gui);
  f.window = gtk_window_new (GTK_WINDOW_TOPLEVEL); g_object_ref_sink (f.window);
  gtk_window_move (GTK_WINDOW (f.window), 40, 40);
  gtk_container_add (GTK_CONTAINER (f.window), f.gui);
  gtk_widget_show_all (f.window); drain ();
  gchar *name = g_strconcat ("painter-paint-", id, NULL);
  f.button = find (f.gui, name, GTK_TYPE_BUTTON); g_free (name);
  g_assert_nonnull (f.button); g_assert_false (GTK_IS_MENU_BUTTON (f.button));
  f.row = gtk_widget_get_parent (f.button);
  f.body = g_object_get_data (G_OBJECT (f.button), "gimp-painter-toolbar-popup");
  g_assert_nonnull (f.body);
  /* The independent toolbar is wider than a portrait monitor. Put this
   * actual row, rather than its far-left siblings, safely inside the screen. */
  gint row_x, row_y;
  g_assert_true (gtk_widget_translate_coordinates (f.row, f.window, 0, 0, &row_x, &row_y));
  gtk_window_move (GTK_WINDOW (f.window), 100 - row_x, 40); drain ();
  return f;
}

static StandardToggleFixture
standard_toggle_fixture (const gchar *id)
{
  return standard_toggle_fixture_for (id, info ("gimp-paintbrush-tool")->tool_options);
}

static GtkWidget *
standard_toggle_open (StandardToggleFixture *f)
{
  gtk_button_clicked (GTK_BUTTON (f->button)); drain ();
  GtkWidget *host = standard_toggle_visible (GTK_TYPE_WINDOW, "painter-standard-toggle-popup");
  g_assert_nonnull (host);
  g_assert_cmpint (gtk_window_get_window_type (GTK_WINDOW (host)), ==, GTK_WINDOW_POPUP);
  g_assert_true (gtk_widget_is_ancestor (f->body, host));
  g_assert_true (gtk_widget_get_mapped (host));
  g_assert_true (gtk_widget_has_grab (host));
  return host;
}

static void
standard_toggle_finish (StandardToggleFixture *f)
{
  gtk_widget_destroy (f->window); g_object_unref (f->window); drain ();
  g_object_set (f->options, f->enabled, f->previous, NULL); drain ();
  g_assert_null (standard_toggle_visible (GTK_TYPE_WINDOW, "painter-standard-toggle-popup"));
}

static gboolean
standard_toggle_event (GtkWidget *host, guint key)
{
  GdkEvent *event = gdk_event_new (key ? GDK_KEY_PRESS : GDK_BUTTON_PRESS);
  event->any.window = g_object_ref (gtk_widget_get_window (host));
  event->any.send_event = TRUE;
  GdkSeat *seat = gdk_display_get_default_seat (gtk_widget_get_display (host));
  gboolean handled;
  if (key) {
    event->key.time = GDK_CURRENT_TIME; event->key.keyval = key;
    GdkKeymapKey *entries = NULL; gint count = 0;
    g_assert_true (gdk_keymap_get_entries_for_keyval (
      gdk_keymap_get_for_display (gtk_widget_get_display (host)), key, &entries, &count));
    g_assert_cmpint (count, >, 0);
    event->key.hardware_keycode = entries[0].keycode; event->key.group = entries[0].group;
    g_free (entries);
    gdk_event_set_device (event, gdk_seat_get_keyboard (seat));
    handled = gtk_widget_event (host, event);
  } else {
    event->button.time = GDK_CURRENT_TIME; event->button.button = 1;
    event->button.x = -10; event->button.y = -10;
    gdk_event_set_device (event, gdk_seat_get_pointer (seat));
    /* GTK dispatch returns TRUE on destruction regardless of handler policy. */
    handled = GTK_WIDGET_GET_CLASS (host)->button_press_event (host, &event->button);
  }
  gdk_event_free (event); drain (); return handled;
}

static void
standard_toggle_focus (GtkWidget *host)
{
  gtk_window_set_focus (GTK_WINDOW (host), NULL);
  gdk_window_focus (gtk_widget_get_window (host), GDK_CURRENT_TIME);
  gint64 deadline = g_get_monotonic_time () + 3 * G_TIME_SPAN_SECOND;
  while (!gtk_window_is_active (GTK_WINDOW (host)) && g_get_monotonic_time () < deadline) {
    g_main_context_iteration (NULL, FALSE); g_usleep (1000);
  }
  g_assert_true (gtk_window_is_active (GTK_WINDOW (host)));
}

static void
standard_toggle_live_lifecycle (gconstpointer data)
{
  const gchar *id = data;
  StandardToggleFixture f = standard_toggle_fixture (id);
  g_object_set (f.options, f.enabled, FALSE, NULL); drain ();
  g_assert_false (gtk_widget_is_sensitive (f.button));
  g_assert_true (gtk_widget_get_sensitive (f.body));
  gtk_button_clicked (GTK_BUTTON (f.button)); drain ();
  g_assert_null (standard_toggle_visible (GTK_TYPE_WINDOW, "painter-standard-toggle-popup"));
  g_object_set (f.options, f.enabled, TRUE, NULL); drain ();
  g_assert_true (gtk_widget_is_sensitive (f.button));
  GtkWidget *host = standard_toggle_open (&f); g_object_ref (host);
  /* Away from monitor edges, left alignment must use the whole checkbox row,
   * not the arrow at its right edge. */
  gint rx, ry, bx, by, px, py;
  g_assert_true (gtk_widget_translate_coordinates (f.row, f.window, 0, 0, &rx, &ry));
  g_assert_true (gtk_widget_translate_coordinates (f.button, f.window, 0, 0, &bx, &by));
  gint wx, wy; gdk_window_get_origin (gtk_widget_get_window (f.window), &wx, &wy);
  gtk_window_get_position (GTK_WINDOW (host), &px, &py);
  g_assert_cmpint (bx, >, rx);
  g_assert_cmpint (px, ==, wx + rx);
  const gchar *value_property = !strcmp (id, "jitter") ? "jitter-amount" :
                               !strcmp (id, "smoothing") ? "smoothing-factor" : NULL;
  gdouble saved = 0, edited = 0;
  GtkWidget *spin = value_property ? find (host, value_property, GTK_TYPE_SPIN_BUTTON) : NULL;
  if (value_property) {
    g_assert_nonnull (spin); saved = gtk_spin_button_get_value (GTK_SPIN_BUTTON (spin));
    edited = !strcmp (id, "jitter") ? 1.73 : 31;
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (spin), edited); drain ();
    gdouble value; g_object_get (f.options, value_property, &value, NULL);
    g_assert_cmpfloat_with_epsilon (value, edited, 1e-6);
  }
  g_object_set (f.options, f.enabled, FALSE, NULL); drain ();
  g_assert_false (gtk_widget_is_sensitive (f.button));
  g_assert_true (gtk_widget_is_sensitive (f.body));
  g_assert_true (gtk_widget_get_mapped (host));
  standard_toggle_focus (host);
  const guint keys[] = {GDK_KEY_Return, GDK_KEY_KP_Enter, GDK_KEY_space};
  for (guint i = 0; i < G_N_ELEMENTS (keys); ++i) {
    standard_toggle_event (host, keys[i]); g_assert_true (gtk_widget_get_mapped (host));
  }
  standard_toggle_event (host, GDK_KEY_Escape);
  g_assert_false (gtk_widget_get_realized (host));
  g_object_set (f.options, f.enabled, TRUE, NULL); drain ();
  GtkWidget *next = standard_toggle_open (&f); g_object_ref (next);
  g_assert_true (next != host);
  if (spin) g_assert_cmpfloat_with_epsilon (gtk_spin_button_get_value (GTK_SPIN_BUTTON (spin)), edited, 1e-6);
  g_assert_false (standard_toggle_event (next, 0));
  g_assert_false (gtk_widget_get_realized (next));
  GtkWidget *last = standard_toggle_open (&f); g_object_ref (last);
  g_assert_true (last != next && last != host);
  if (spin) gtk_spin_button_set_value (GTK_SPIN_BUTTON (spin), saved);
  standard_toggle_finish (&f);
  g_assert_false (gtk_widget_get_realized (last));
  g_object_unref (last); g_object_unref (next); g_object_unref (host);
}

static GtkWidget *
standard_toggle_stock_open (GtkWidget *outer)
{
  GtkWidget *button = find (outer, NULL, GIMP_TYPE_VIEWABLE_BUTTON);
  g_assert_nonnull (button);
  gtk_button_clicked (GTK_BUTTON (button)); drain ();
  GtkWidget *child = standard_toggle_visible (GIMP_TYPE_CONTAINER_POPUP, NULL);
  g_assert_nonnull (child);
  g_object_ref (child);
  g_assert_true (gtk_window_get_transient_for (GTK_WINDOW (child)) == GTK_WINDOW (outer));
  /* Stock GimpPopup acquires its real seat/GTK grab in map-event, not in
   * show(). Wait for that observable condition; never manufacture a grab. */
  gint64 deadline = g_get_monotonic_time () + 3 * G_TIME_SPAN_SECOND;
  while (gtk_grab_get_current () != child && gtk_widget_get_visible (child) &&
         g_get_monotonic_time () < deadline) {
    g_main_context_iteration (NULL, FALSE); g_usleep (1000);
  }
  if (gtk_grab_get_current () != child) {
    GtkWidget *grab = gtk_grab_get_current ();
    g_test_message ("Stock child grab: current=%s/%s child visible=%d mapped=%d",
      grab ? G_OBJECT_TYPE_NAME (grab) : "none", grab ? gtk_widget_get_name (grab) : "none",
      gtk_widget_get_visible (child), gtk_widget_get_mapped (child));
  }
  g_assert_true (gtk_grab_get_current () == child);
  g_assert_true (gtk_widget_get_mapped (outer));
  g_object_unref (child);
  return child;
}

static void
standard_toggle_destroy_observer (GtkWidget *child, gpointer owner)
{
  (void) child; gtk_widget_destroy (owner);
}

static void
standard_toggle_nested_stock (gconstpointer data)
{
  StandardToggleFixture f = standard_toggle_fixture ("texture");
  g_object_set (f.options, f.enabled, TRUE, NULL); drain ();
  GtkWidget *outer = standard_toggle_open (&f); g_object_ref (outer);
  const guint keys[] = {GDK_KEY_Return, GDK_KEY_space, GDK_KEY_Escape, 0};
  for (guint i = 0; i < G_N_ELEMENTS (keys); ++i) {
    GtkWidget *child = standard_toggle_stock_open (outer); g_object_ref (child);
    if (keys[i]) { standard_toggle_focus (child); standard_toggle_event (child, keys[i]); }
    else g_assert_true (standard_toggle_event (child, 0));
    g_assert_false (gtk_widget_get_realized (child));
    g_assert_true (gtk_widget_get_mapped (outer));
    g_assert_true (gtk_grab_get_current () == outer);
    g_object_unref (child);
  }
  GtkWidget *child = standard_toggle_stock_open (outer); g_object_ref (child);
  /* Retirement cancels the real stock child; that observer may synchronously
   * destroy the owner while its raw stock invoker callback is still active. */
  g_signal_connect (child, "cancel", G_CALLBACK (standard_toggle_destroy_observer), f.window);
  g_signal_emit_by_name (data ? child : outer, "cancel"); drain ();
  g_assert_false (gtk_widget_get_realized (child));
  g_assert_false (gtk_widget_get_realized (outer));
  standard_toggle_finish (&f);
  g_object_unref (child); g_object_unref (outer);
}

typedef struct {
  GtkWidget *outer, *button;
  guint calls;
  gboolean cancel_outer;
} StandardToggleReopen;

static void
standard_toggle_reopen_during_stock_cancel (GtkWidget *child, gpointer data)
{
  StandardToggleReopen *request = data;
  (void) child;
  ++request->calls;
  /* Fail promptly rather than allowing an unbounded recursive cancel. */
  g_assert_cmpuint (request->calls, ==, 1);
  if (request->cancel_outer) g_signal_emit_by_name (request->outer, "cancel");
  gtk_button_clicked (GTK_BUTTON (request->button));
  /* A retiring stock child still owns a borrowed invoker and its grab.
   * Do not reopen a parent host until that child's signal has unwound. */
  g_assert_null (standard_toggle_visible (GTK_TYPE_WINDOW, "painter-standard-toggle-popup"));
}

static void
standard_toggle_pending_stock_reopen (gconstpointer data)
{
  StandardToggleFixture f = standard_toggle_fixture ("texture");
  g_object_set (f.options, f.enabled, TRUE, NULL); drain ();
  GtkWidget *outer = standard_toggle_open (&f); g_object_ref (outer);
  GtkWidget *child = standard_toggle_stock_open (outer); g_object_ref (child);
  gboolean direct = !strcmp (data, "cancel-direct");
  StandardToggleReopen request = {outer, f.button, 0, !direct};
  const gchar *signal = direct ? "cancel" : data;
  g_signal_connect (child, signal, G_CALLBACK (standard_toggle_reopen_during_stock_cancel), &request);
  g_signal_emit_by_name (child, signal); drain ();
  g_assert_cmpuint (request.calls, ==, 1);
  g_assert_false (gtk_widget_get_realized (outer));
  g_assert_false (gtk_widget_get_realized (child));
  g_assert_true (gtk_grab_get_current () != outer);
  g_assert_true (gtk_grab_get_current () != child);
  GtkWidget *next = standard_toggle_open (&f);
  g_assert_true (next != outer);
  standard_toggle_finish (&f);
  g_object_unref (child); g_object_unref (outer);
}

typedef struct {
  GtkWidget *window, *button;
  gboolean body_destroyed;
  guint calls;
} StandardToggleNestedRetirement;

static void
standard_toggle_body_destroyed (GtkWidget *widget, gpointer data)
{
  (void) widget; ((StandardToggleNestedRetirement *) data)->body_destroyed = TRUE;
}

static void
standard_toggle_second_stock_during_cancel (GtkWidget *first, gpointer data)
{
  StandardToggleNestedRetirement *request = data;
  g_assert_cmpuint (++request->calls, ==, 1);
  /* The real viewable button creates another stock selector. Do not iterate
   * the main loop: both native cancel emissions deliberately overlap. */
  gtk_button_clicked (GTK_BUTTON (request->button));
  GtkWidget *second = NULL;
  GList *windows = gtk_window_list_toplevels ();
  for (GList *i = windows; i; i = i->next)
    if (i->data != first && GIMP_IS_CONTAINER_POPUP (i->data) && gtk_widget_get_visible (i->data)) {
      g_assert_null (second); second = i->data;
    }
  g_list_free (windows); g_assert_nonnull (second); g_object_ref (second);
  g_signal_connect (second, "cancel", G_CALLBACK (standard_toggle_destroy_observer), request->window);
  g_signal_emit_by_name (second, "cancel");
  g_assert_false (gtk_widget_get_realized (second));
  /* First stock signal still borrows the invoker: the first child to retire
   * must not destroy the shared body while another remains in flight. */
  g_assert_false (request->body_destroyed);
  g_object_unref (second);
}

static void
standard_toggle_multiple_pending_children (void)
{
  StandardToggleFixture f = standard_toggle_fixture ("texture");
  g_object_set (f.options, f.enabled, TRUE, NULL); drain ();
  GtkWidget *outer = standard_toggle_open (&f); g_object_ref (outer);
  GtkWidget *first = standard_toggle_stock_open (outer); g_object_ref (first);
  StandardToggleNestedRetirement request = {f.window,
    find (outer, NULL, GIMP_TYPE_VIEWABLE_BUTTON), FALSE, 0};
  g_assert_nonnull (request.button);
  g_signal_connect (f.body, "destroy", G_CALLBACK (standard_toggle_body_destroyed), &request);
  g_signal_connect (first, "cancel", G_CALLBACK (standard_toggle_second_stock_during_cancel), &request);
  g_signal_emit_by_name (first, "cancel"); drain ();
  g_assert_cmpuint (request.calls, ==, 1);
  g_assert_false (gtk_widget_get_realized (first));
  g_assert_false (gtk_widget_get_realized (outer));
  g_assert_true (request.body_destroyed);
  standard_toggle_finish (&f);
  g_object_unref (first); g_object_unref (outer);
}

static void
standard_toggle_model_finalized (gpointer data, GObject *object)
{
  (void) object; *(gboolean *) data = TRUE;
}

static void
standard_toggle_caller_release_lifetime (gconstpointer data)
{
  GimpToolInfo *tool = info ("gimp-paintbrush-tool");
  GimpToolOptions *options = g_object_new (G_OBJECT_TYPE (tool->tool_options),
                                          "gimp", gimp, "tool-info", tool, NULL);
  gimp_context_copy_properties (GIMP_CONTEXT (tool->tool_options), GIMP_CONTEXT (options),
                                GIMP_CONTEXT_PROP_MASK_PAINT | GIMP_CONTEXT_PROP_MASK_PAINT_INFO);
  gboolean finalized = FALSE;
  g_object_weak_ref (G_OBJECT (options), standard_toggle_model_finalized, &finalized);
  StandardToggleFixture f = standard_toggle_fixture_for ("texture", options);
  g_object_set (options, "use-texture", TRUE, NULL); drain ();
  g_object_unref (options); /* Native property views now own the live model. */
  g_assert_false (finalized);
  GtkWidget *outer = standard_toggle_open (&f); g_object_ref (outer);
  GtkWidget *child = standard_toggle_stock_open (outer); g_object_ref (child);
  if (GPOINTER_TO_INT (data)) gtk_widget_hide (f.window);
  else gtk_widget_destroy (outer);
  drain ();
  g_assert_false (gtk_widget_get_realized (outer));
  g_assert_false (gtk_widget_get_realized (child));
  g_assert_true (gtk_grab_get_current () != outer && gtk_grab_get_current () != child);
  g_assert_false (finalized);
  gtk_widget_show_all (f.window); drain ();
  GtkWidget *next = standard_toggle_open (&f);
  g_assert_true (next != outer);
  gboolean enabled = FALSE; g_object_get (f.options, "use-texture", &enabled, NULL);
  g_assert_true (enabled);
  gtk_widget_destroy (f.window); g_object_unref (f.window);
  g_object_unref (child); g_object_unref (outer); drain ();
  g_assert_true (finalized);
}

static void
standard_toggle_reveal_row (GtkWidget *row)
{
  GtkWidget *scroll = gtk_widget_get_ancestor (row, GTK_TYPE_SCROLLED_WINDOW);
  g_assert_nonnull (scroll);
  GtkWidget *viewport = gtk_bin_get_child (GTK_BIN (scroll));
  GtkAdjustment *adjustment = gtk_scrolled_window_get_hadjustment (GTK_SCROLLED_WINDOW (scroll));
  gint x, y;
  g_assert_true (gtk_widget_translate_coordinates (row, viewport, 0, 0, &x, &y));
  gdouble value = gtk_adjustment_get_value (adjustment) + x - 20;
  gtk_adjustment_set_value (adjustment, CLAMP (value,
    gtk_adjustment_get_lower (adjustment), gtk_adjustment_get_upper (adjustment) - gtk_adjustment_get_page_size (adjustment)));
  drain ();
  GtkWidget *top = gtk_widget_get_toplevel (row);
  g_assert_true (gtk_widget_translate_coordinates (row, top, 0, 0, &x, &y));
  gint left, upper; gdk_window_get_origin (gtk_widget_get_window (top), &left, &upper);
  GdkMonitor *monitor = gdk_display_get_monitor_at_point (gtk_widget_get_display (row), left + x, upper + y);
  GdkRectangle rect; gdk_monitor_get_geometry (monitor, &rect);
  g_assert_cmpint (left + x, >=, rect.x);
  g_assert_cmpint (left + x + gtk_widget_get_allocated_width (row), <=, rect.x + rect.width);
}

static void
standard_toggle_actual_canvas (gconstpointer data)
{
  const gchar *id = data;
  g_assert_true (gimp->config->painter_canvas_ui);
  gboolean single = FALSE;
  g_object_get (gimp->config, "single-window-mode", &single, NULL);
  g_assert_true (single);
  gchar *old_path = NULL;
  g_object_get (gimp->config, "pattern-path", &old_path, NULL);
  gchar *path = g_build_filename (g_getenv ("GIMP_TESTING_ABS_TOP_SRCDIR"), "gimp-data", "patterns", NULL);
  g_object_set (gimp->config, "pattern-path", path, NULL); g_free (path);
  gimp_data_factory_data_refresh (gimp->pattern_factory, gimp_get_user_context (gimp));
  GimpContainer *patterns = gimp_data_factory_get_container (gimp->pattern_factory);
  g_assert_cmpint (gimp_container_get_n_children (patterns), >, 1);
  GimpDisplayShell *shell = new_shell ();
  GimpToolInfo *paint = info ("gimp-paintbrush-tool");
  gimp_context_set_tool (gimp_get_user_context (gimp), paint); drain ();
  gimp_painter_canvas_ui_pointer (shell, 0, 0, TRUE); drain ();
  StandardToggleFixture f = {0};
  f.options = paint->tool_options;
  f.enabled = !strcmp (id, "jitter") ? "use-jitter" :
              !strcmp (id, "texture") ? "use-texture" : "use-smoothing";
  g_object_get (f.options, f.enabled, &f.previous, NULL);
  gchar *name = g_strconcat ("painter-paint-", id, NULL);
  GPtrArray *widgets = g_ptr_array_new (); gather (shell->canvas, widgets);
  for (guint i = 0; i < widgets->len; ++i) {
    GtkWidget *w = g_ptr_array_index (widgets, i);
    if (g_strcmp0 (gtk_widget_get_name (w), name)) continue;
    GtkWidget *row = gtk_widget_get_parent (w), *gui = gtk_widget_get_parent (row);
    if (gui && !g_strcmp0 (gtk_widget_get_name (gui), "gimp-painter-paint-toolbar") &&
        gtk_widget_get_ancestor (w, GIMP_TYPE_DISPLAY_SHELL) == GTK_WIDGET (shell)) {
      g_assert_null (f.button); f.button = w; f.row = row; f.gui = gui;
    }
  }
  g_free (name); g_ptr_array_unref (widgets); g_assert_nonnull (f.button);
  g_assert_false (GTK_IS_MENU_BUTTON (f.button));
  f.body = g_object_get_data (G_OBJECT (f.button), "gimp-painter-toolbar-popup");
  g_object_set (f.options, f.enabled, TRUE, NULL); drain ();
  GtkWidget *top = gtk_widget_get_toplevel (f.button);
  if (g_getenv ("GIMP_STANDARD_TOGGLE_CAPTURE")) {
    gtk_window_maximize (GTK_WINDOW (top));
    gint64 deadline = g_get_monotonic_time () + G_TIME_SPAN_SECOND;
    while (g_get_monotonic_time () < deadline) { g_main_context_iteration (NULL, FALSE); g_usleep (1000); }
  }
  standard_toggle_reveal_row (f.row);
  GtkWidget *host = standard_toggle_open (&f); g_object_ref (host);
  if (!strcmp (id, "texture")) {
    GtkWidget *pattern_button = find (host, NULL, GIMP_TYPE_VIEWABLE_BUTTON);
    g_assert_nonnull (pattern_button);
    GimpPattern *before = gimp_context_get_pattern (GIMP_CONTEXT (f.options));
    if (before) g_object_ref (before);
    gint old_type; g_object_get (pattern_button, "popup-view-type", &old_type, NULL);
    g_object_set (pattern_button, "popup-view-type", GIMP_VIEW_TYPE_GRID, NULL);
    GtkWidget *child = standard_toggle_stock_open (host); g_object_ref (child);
    GtkWidget *icons = find (child, NULL, GTK_TYPE_ICON_VIEW); g_assert_nonnull (icons);
    GtkTreeModel *model = gtk_icon_view_get_model (GTK_ICON_VIEW (icons));
    GtkTreeIter iter; gboolean valid = gtk_tree_model_get_iter_first (model, &iter);
    GimpPattern *selected = NULL;
    while (valid) {
      GimpViewRenderer *renderer = NULL;
      gtk_tree_model_get (model, &iter, GIMP_CONTAINER_TREE_STORE_COLUMN_RENDERER, &renderer, -1);
      if (renderer && renderer->viewable != GIMP_VIEWABLE (before)) {
        selected = GIMP_PATTERN (g_object_ref (renderer->viewable));
        GtkTreePath *row = gtk_tree_model_get_path (model, &iter);
        gtk_icon_view_select_path (GTK_ICON_VIEW (icons), row); gtk_tree_path_free (row);
        g_object_unref (renderer); break;
      }
      g_clear_object (&renderer); valid = gtk_tree_model_iter_next (model, &iter);
    }
    g_assert_nonnull (selected); drain ();
    g_assert_true (gimp_context_get_pattern (GIMP_CONTAINER_POPUP (child)->context) == selected);
    g_assert_true (gimp_context_get_pattern (GIMP_CONTEXT (f.options)) == before);
    standard_toggle_focus (child); standard_toggle_event (child, GDK_KEY_Return);
    g_assert_false (gtk_widget_get_realized (child)); g_object_unref (child);
    g_assert_true (gimp_context_get_pattern (GIMP_CONTEXT (f.options)) == selected);
    g_assert_true (gtk_grab_get_current () == host);
    g_assert_false (standard_toggle_event (host, 0));
    g_assert_true (gimp_context_get_pattern (GIMP_CONTEXT (f.options)) == selected);
    g_object_unref (host); host = standard_toggle_open (&f); g_object_ref (host);
    g_assert_true (gimp_context_get_pattern (GIMP_CONTEXT (f.options)) == selected);
    g_object_set (pattern_button, "popup-view-type", old_type, NULL);
    /* Selection remains live until explicit fixture cleanup, not outer close. */
    gimp_context_set_pattern (GIMP_CONTEXT (f.options), before);
    g_clear_object (&before); g_object_unref (selected);
  }
  const gchar *prefix = g_getenv ("GIMP_STANDARD_TOGGLE_CAPTURE");
  if (prefix) {
    GdkWindow *root = gdk_screen_get_root_window (gtk_widget_get_screen (host));
    GdkPixbuf *pixels = gdk_pixbuf_get_from_window (root, 0, 0,
      gdk_window_get_width (root), gdk_window_get_height (root));
    g_assert_nonnull (pixels);
    gchar *out = g_strdup_printf ("%s-%s.png", prefix, id); GError *error = NULL;
    g_assert_true (gdk_pixbuf_save (pixels, out, "png", &error, NULL)); g_assert_no_error (error);
    g_free (out); g_object_unref (pixels);
    const gchar *pause = g_getenv ("GIMP_STANDARD_TOGGLE_PAUSE");
    if (pause) {
      gint64 until = g_get_monotonic_time () + g_ascii_strtoll (pause, NULL, 10) * G_TIME_SPAN_SECOND;
      while (g_get_monotonic_time () < until) { g_main_context_iteration (NULL, FALSE); g_usleep (1000); }
    }
  }
  g_signal_emit_by_name (host, "cancel"); drain (); g_object_unref (host);
  g_object_set (f.options, f.enabled, f.previous, NULL);
  close_shell (shell);
  g_object_set (gimp->config, "pattern-path", old_path, NULL); g_free (old_path);
  gimp_data_factory_data_refresh (gimp->pattern_factory, gimp_get_user_context (gimp));
}

static void
register_standard_toggle_popup_tests (void)
{
  g_test_add_data_func ("/standard-toggle-popup/jitter-live-lifecycle", "jitter", standard_toggle_live_lifecycle);
  g_test_add_data_func ("/standard-toggle-popup/texture-live-lifecycle", "texture", standard_toggle_live_lifecycle);
  g_test_add_data_func ("/standard-toggle-popup/smoothing-live-lifecycle", "smoothing", standard_toggle_live_lifecycle);
  g_test_add_data_func ("/standard-toggle-popup/texture-nested-stock", NULL, standard_toggle_nested_stock);
  g_test_add_data_func ("/standard-toggle-popup/texture-stock-cancel-owner-reentry", GINT_TO_POINTER (1), standard_toggle_nested_stock);
  g_test_add_data_func ("/standard-toggle-popup/texture-pending-stock-reopen", "cancel", standard_toggle_pending_stock_reopen);
  g_test_add_data_func ("/standard-toggle-popup/texture-pending-stock-confirm-reopen", "confirm", standard_toggle_pending_stock_reopen);
  g_test_add_data_func ("/standard-toggle-popup/texture-pending-stock-direct-reopen", "cancel-direct", standard_toggle_pending_stock_reopen);
  g_test_add_func ("/standard-toggle-popup/texture-multiple-pending-stock-children", standard_toggle_multiple_pending_children);
  g_test_add_data_func ("/standard-toggle-popup/texture-caller-release-host-destroy", NULL, standard_toggle_caller_release_lifetime);
  g_test_add_data_func ("/standard-toggle-popup/texture-caller-release-owner-unmap", GINT_TO_POINTER (1), standard_toggle_caller_release_lifetime);
  g_test_add_data_func ("/standard-toggle-popup/actual-canvas-jitter", "jitter", standard_toggle_actual_canvas);
  g_test_add_data_func ("/standard-toggle-popup/actual-canvas-texture", "texture", standard_toggle_actual_canvas);
  g_test_add_data_func ("/standard-toggle-popup/actual-canvas-smoothing", "smoothing", standard_toggle_actual_canvas);
}
