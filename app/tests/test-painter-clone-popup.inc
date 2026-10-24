/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Real registered Clone-family entries; Heal is deliberately outside this slice. */
#include "gimp-intl.h"
static GtkWidget *
clone_popup_canvas_button (GimpDisplayShell *shell)
{
  GPtrArray *widgets = g_ptr_array_new (); gather (shell->canvas, widgets);
  GtkWidget *button = NULL;
  for (guint i = 0; i < widgets->len; ++i) {
    GtkWidget *w = g_ptr_array_index (widgets, i);
    GtkWidget *row = gtk_widget_get_parent (w);
    GtkWidget *toolbar = row ? gtk_widget_get_parent (row) : NULL;
    if (!g_strcmp0 (gtk_widget_get_name (w), "painter-paint-source") && toolbar &&
        !g_strcmp0 (gtk_widget_get_name (toolbar), "gimp-painter-paint-toolbar") &&
        gtk_widget_get_ancestor (w, GIMP_TYPE_DISPLAY_SHELL) == GTK_WIDGET (shell)) {
      g_assert_null (button); button = w;
    }
  }
  g_ptr_array_unref (widgets); g_assert_nonnull (button); return button;
}

static gboolean
clone_popup_position_hook (GSignalInvocationHint *hint, guint count,
                               const GValue *values, gpointer data)
{
  TransformPopupPosition *record = data;
  (void) hint; (void) count;
  GtkWidget *widget = g_value_get_object (&values[0]);
  if (g_strcmp0 (gtk_widget_get_name (widget), "painter-clone-options-popup")) return TRUE;
  GtkRequisition natural; gtk_widget_get_preferred_size (widget, NULL, &natural);
  GtkAllocation allocation; gtk_widget_get_allocation (record->button, &allocation);
  gdk_window_get_origin (gtk_widget_get_window (record->button), &record->left, &record->top);
  if (!gtk_widget_get_has_window (record->button)) {
    record->left += allocation.x; record->top += allocation.y;
  }
  record->anchor_width = allocation.width; record->anchor_height = allocation.height;
  record->width = natural.width; record->height = natural.height;
  gtk_window_get_position (GTK_WINDOW (widget), &record->x, &record->y);
  record->mapped = gtk_widget_get_mapped (widget); record->captured = TRUE;
  return TRUE;
}
static GtkWidget *
clone_popup_open_button (GtkWidget *button)
{
  transform_popup_reveal_button (button);
  TransformPopupPosition realize = {0}; realize.button = button;
  guint signal = g_signal_lookup ("realize", GTK_TYPE_WIDGET);
  gulong hook = g_signal_add_emission_hook (signal, 0, clone_popup_position_hook, &realize, NULL);
  GList *before = gtk_window_list_toplevels ();
#ifdef GDK_WINDOWING_X11
  if (GDK_IS_X11_DISPLAY (gtk_widget_get_display (button))) {
    RectanglePopupInput input = transform_popup_input ();
    transform_popup_click_center (&input, button); g_module_close (input.module);
  } else
#endif
    gtk_button_clicked (GTK_BUTTON (button));
  drain ();
  g_signal_remove_emission_hook (signal, hook);
  GtkWidget *actual = NULL;
  if (GTK_IS_MENU_BUTTON (button)) {
    actual = GTK_WIDGET (gtk_menu_button_get_popover (GTK_MENU_BUTTON (button)));
    g_assert_nonnull (actual); g_assert_true (gtk_widget_get_visible (actual));
  } else {
    GList *windows = gtk_window_list_toplevels ();
    for (GList *i = windows; i; i = i->next)
      if (!g_list_find (before, i->data) && GTK_IS_WINDOW (i->data) &&
          gtk_widget_get_visible (i->data)) {
        g_assert_null (actual); actual = i->data;
      }
    g_list_free (windows);
  }
  g_list_free (before); g_assert_nonnull (actual);
  g_test_message ("Actual same-shell Clone Options: invoker=%s carrier=%s",
                  G_OBJECT_TYPE_NAME (button), G_OBJECT_TYPE_NAME (actual));
  g_assert_true (GTK_IS_WINDOW (actual));
  g_assert_cmpint (gtk_window_get_window_type (GTK_WINDOW (actual)), ==, GTK_WINDOW_POPUP);
  g_assert_true (gtk_widget_has_grab (actual));
  g_assert_cmpint (gtk_button_get_relief (GTK_BUTTON (button)), ==, GTK_RELIEF_NONE);
  GtkWidget *label = find (button, NULL, GTK_TYPE_LABEL); g_assert_nonnull (label);
  g_assert_cmpstr (gtk_label_get_text (GTK_LABEL (label)), ==, _("Options"));
  g_assert_cmpstr (atk_object_get_name (gtk_widget_get_accessible (button)), ==, _("Options"));
  transform_popup_assert_anchor (actual, button, &realize);
  return actual;
}

typedef struct {
  GimpDisplayShell *shell;
  GimpToolOptions *options;
  GtkWidget *button, *dock, *dock_parent, *peer, *mode;
  gpointer dock_merged, dock_label;
  GimpTool *active;
  GimpImage *image;
  gint undo_depth, source, alignment, clone_mode;
  gboolean merged;
} ClonePopupFixture;

static ClonePopupFixture
clone_popup_fixture (const gchar *tool_id)
{
  ClonePopupFixture f = {0};
  g_assert_true (gimp->config->painter_canvas_ui);
  gboolean single = FALSE; g_object_get (gimp->config, "single-window-mode", &single, NULL); g_assert_true (single);
  f.shell = new_shell (); GimpToolInfo *tool = info (tool_id); g_assert_nonnull (tool);
  f.options = tool->tool_options;
  gimp_context_set_tool (gimp_get_user_context (gimp), tool); drain ();
  gimp_painter_canvas_ui_pointer (f.shell, 0, 0, TRUE); drain ();
  f.button = clone_popup_canvas_button (f.shell);
  f.dock = gimp_tools_get_tool_options_gui (f.options); f.dock_parent = gtk_widget_get_parent (f.dock);
  f.dock_merged = g_object_get_data (G_OBJECT (f.options), "sample-merged-checkbox");
  f.dock_label = g_object_get_data (G_OBJECT (f.options), "src-label");
  f.active = g_object_ref (tool_manager_get_active (gimp)); f.image = gimp_display_get_image (f.shell->display);
  f.undo_depth = gimp_undo_stack_get_depth (gimp_image_get_undo_stack (f.image));
  g_object_get (f.options, "clone-type", &f.source, "align-mode", &f.alignment, "sample-merged", &f.merged, NULL);
  GtkWidget *toolbar = gtk_widget_get_parent (gtk_widget_get_parent (f.button));
  if (!g_strcmp0 (tool_id, "gimp-perspective-clone-tool")) {
    f.mode = find (toolbar, "clone-mode", GTK_TYPE_COMBO_BOX); g_assert_nonnull (f.mode);
    g_object_get (f.options, "clone-mode", &f.clone_mode, NULL);
  }
  if (g_getenv ("GIMP_CLONE_POPUP_CAPTURE")) {
    gtk_window_maximize (GTK_WINDOW (gtk_widget_get_toplevel (f.button)));
    gint64 until = g_get_monotonic_time () + G_TIME_SPAN_SECOND;
    while (g_get_monotonic_time () < until) { g_main_context_iteration (NULL, FALSE); g_usleep (1000); }
  }
  return f;
}

static void
clone_popup_assert_no_action (ClonePopupFixture *f)
{
  g_assert_true (tool_manager_get_active (gimp) == f->active);
  g_assert_cmpint (gimp_undo_stack_get_depth (gimp_image_get_undo_stack (f->image)), ==, f->undo_depth);
  g_assert_true (gimp_tools_get_tool_options_gui (f->options) == f->dock);
  g_assert_true (gtk_widget_get_parent (f->dock) == f->dock_parent);
  g_assert_true (g_object_get_data (G_OBJECT (f->options), "sample-merged-checkbox") == f->dock_merged);
  g_assert_true (g_object_get_data (G_OBJECT (f->options), "src-label") == f->dock_label);
  if (f->mode) {
    GtkWidget *toolbar = gtk_widget_get_parent (gtk_widget_get_parent (f->button));
    g_assert_true (find (toolbar, "clone-mode", GTK_TYPE_COMBO_BOX) == f->mode);
    gint mode; g_object_get (f->options, "clone-mode", &mode, NULL); g_assert_cmpint (mode, ==, f->clone_mode);
  }
}

static void
clone_popup_finish (ClonePopupFixture *f)
{
  if (f->peer) { gtk_widget_destroy (f->peer); g_object_unref (f->peer); }
  g_object_set (f->options, "clone-type", f->source, "align-mode", f->alignment, "sample-merged", f->merged, NULL);
  g_object_unref (f->active); close_shell (f->shell);
}

static gint
clone_popup_enum_value (GtkWidget *widget)
{
  gint value = -1; g_assert_nonnull (widget);
  g_assert_true (gimp_int_combo_box_get_active (GIMP_INT_COMBO_BOX (widget), &value)); return value;
}

static void
clone_popup_capture (GtkWidget *outer)
{
  gint x, y; gtk_window_get_position (GTK_WINDOW (outer), &x, &y);
  gint w = gtk_widget_get_allocated_width (outer), h = gtk_widget_get_allocated_height (outer);
  GdkRectangle bounds; GdkMonitor *monitor = gdk_display_get_monitor_at_window (gtk_widget_get_display (outer), gtk_widget_get_window (outer));
  gdk_monitor_get_geometry (monitor, &bounds);
  g_assert_cmpint (x, >=, bounds.x); g_assert_cmpint (y, >=, bounds.y);
  g_assert_cmpint (x+w, <=, bounds.x+bounds.width); g_assert_cmpint (y+h, <=, bounds.y+bounds.height);
  gint scale = gtk_widget_get_scale_factor (outer);
  g_test_message ("Clone native physical bounds=%d/%d %dx%d scale=%d", x*scale, y*scale, w*scale, h*scale, scale);
  const gchar *path = g_getenv ("GIMP_CLONE_POPUP_CAPTURE"); if (!path) return;
  GdkWindow *root = gdk_screen_get_root_window (gtk_widget_get_screen (outer));
  GdkPixbuf *pixels = gdk_pixbuf_get_from_window (root, 0, 0, gdk_window_get_width (root), gdk_window_get_height (root));
  g_assert_nonnull (pixels); GError *error = NULL;
  g_assert_true (gdk_pixbuf_save (pixels, path, "png", &error, NULL)); g_assert_no_error (error); g_object_unref (pixels);
  const gchar *pause = g_getenv ("GIMP_CLONE_POPUP_PAUSE");
  if (pause) { gint64 until = g_get_monotonic_time () + g_ascii_strtoll (pause, NULL, 10) * G_TIME_SPAN_SECOND;
    while (g_get_monotonic_time () < until) { g_main_context_iteration (NULL, FALSE); g_usleep (1000); } }
}

static void
clone_popup_actual_entry (gconstpointer data)
{
  if (!transform_popup_require_x11_input ()) return;
#ifdef GDK_WINDOWING_X11
  gchar *old_path = bucket_popup_load_patterns ();
  ClonePopupFixture f = clone_popup_fixture (data);
  GimpPattern *old_pattern = gimp_context_get_pattern (GIMP_CONTEXT (f.options));
  if (old_pattern) g_object_ref (old_pattern);
  GimpContainer *patterns = gimp_data_factory_get_container (gimp->pattern_factory);
  GimpPattern *file_pattern = NULL;
  for (gint i = 0; i < gimp_container_get_n_children (patterns); ++i) {
    GimpPattern *candidate = GIMP_PATTERN (gimp_container_get_child_by_index (patterns, i));
    if (gimp_data_get_file (GIMP_DATA (candidate))) { file_pattern = candidate; break; }
  }
  g_assert_nonnull (file_pattern); gimp_context_set_pattern (GIMP_CONTEXT (f.options), file_pattern);
  /* Fail at the real entry before any auxiliary peer setup on the old carrier. */
  GtkWidget *outer = clone_popup_open_button (f.button); g_object_ref (outer);
  GtkWidget *merged = find (outer, "sample-merged", GTK_TYPE_TOGGLE_BUTTON);
  GtkWidget *align = find (outer, "align-mode", GTK_TYPE_COMBO_BOX);
  GtkWidget *source = find (outer, "clone-type", GTK_TYPE_COMBO_BOX);
  GtkWidget *pattern = find (outer, NULL, GIMP_TYPE_VIEWABLE_BUTTON);
  g_assert_nonnull (merged); g_assert_nonnull (align); g_assert_nonnull (source); g_assert_nonnull (pattern);
  f.peer = gimp_tools_create_painter_toolbar_gui (f.options); g_assert_nonnull (f.peer); g_object_ref_sink (f.peer);
  GtkWidget *peer_button = find (f.peer, "painter-paint-source", GTK_TYPE_BUTTON); g_assert_nonnull (peer_button);
  GtkWidget *peer_body = g_object_get_data (G_OBJECT (peer_button), "gimp-painter-toolbar-popup"); g_assert_nonnull (peer_body);
  GtkWidget *peer_merged = find (peer_body, "sample-merged", GTK_TYPE_TOGGLE_BUTTON);
  GtkWidget *peer_align = find (peer_body, "align-mode", GTK_TYPE_COMBO_BOX);
  GtkWidget *dock_merged = find (f.dock, "sample-merged", GTK_TYPE_TOGGLE_BUTTON);
  GtkWidget *dock_align = find (f.dock, "align-mode", GTK_TYPE_COMBO_BOX);
  g_assert_nonnull (peer_merged); g_assert_nonnull (dock_merged);
  for (gint i = 0; i < 2; ++i) {
    gint type = i ? GIMP_CLONE_PATTERN : GIMP_CLONE_IMAGE;
    g_object_set (f.options, "clone-type", type, NULL); drain ();
    g_assert_cmpint (clone_popup_enum_value (source), ==, type);
    g_assert_cmpint (gtk_widget_get_visible (merged), ==, !i);
    g_assert_cmpint (gtk_widget_is_visible (pattern), ==, i);
    g_assert_cmpint (gtk_widget_get_visible (peer_merged), ==, !i);
    g_assert_true (gtk_widget_is_sensitive (align)); g_assert_true (gtk_widget_is_sensitive (f.button));
  }
  g_object_set (f.options, "clone-type", GIMP_CLONE_IMAGE, "sample-merged", FALSE, NULL); drain ();
  RectanglePopupInput input = transform_popup_input ();
  transform_popup_click_center (&input, merged); drain ();
  gboolean value; g_object_get (f.options, "sample-merged", &value, NULL); g_assert_true (value);
  g_assert_true (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (peer_merged)));
  g_assert_true (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dock_merged)));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (peer_merged), FALSE); drain ();
  g_assert_false (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (merged)));
  g_assert_false (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dock_merged)));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dock_merged), TRUE); drain ();
  g_assert_true (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (merged)));
  g_assert_true (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (peer_merged)));
  GtkTreeModel *model = gtk_combo_box_get_model (GTK_COMBO_BOX (align)); GtkTreeIter row;
  g_assert_true (gtk_tree_model_get_iter_first (model, &row)); gint first, second;
  gtk_tree_model_get (model, &row, GIMP_INT_STORE_VALUE, &first, -1);
  g_assert_true (gtk_tree_model_iter_next (model, &row)); gtk_tree_model_get (model, &row, GIMP_INT_STORE_VALUE, &second, -1);
  g_assert_cmpint (first, !=, second);
  g_assert_true (gimp_int_combo_box_set_active (GIMP_INT_COMBO_BOX (align), second)); drain ();
  gint current; g_object_get (f.options, "align-mode", &current, NULL); g_assert_cmpint (current, ==, second);
  g_assert_cmpint (clone_popup_enum_value (peer_align), ==, second); g_assert_cmpint (clone_popup_enum_value (dock_align), ==, second);
  gimp_int_combo_box_set_active (GIMP_INT_COMBO_BOX (peer_align), first); drain ();
  g_assert_cmpint (clone_popup_enum_value (align), ==, first); g_assert_cmpint (clone_popup_enum_value (dock_align), ==, first);
  gimp_int_combo_box_set_active (GIMP_INT_COMBO_BOX (dock_align), second); drain ();
  g_assert_cmpint (clone_popup_enum_value (align), ==, second); g_assert_cmpint (clone_popup_enum_value (peer_align), ==, second);
  const guint keys[] = { GDK_KEY_Return, GDK_KEY_KP_Enter, GDK_KEY_space };
  for (guint i = 0; i < G_N_ELEMENTS (keys); ++i) {
    rectangle_popup_key (&input, outer, keys[i]); g_assert_true (gtk_widget_get_mapped (outer)); clone_popup_assert_no_action (&f);
  }
  g_object_get (f.options, "sample-merged", &value, NULL);
  /* Show the two genuine source modes across the pair of captures. */
  if (f.mode) { g_object_set (f.options, "clone-type", GIMP_CLONE_PATTERN, NULL); drain (); }
  clone_popup_capture (outer);
  rectangle_popup_key (&input, outer, GDK_KEY_Escape); drain (); g_assert_false (gtk_widget_get_realized (outer));
  GtkWidget *next = clone_popup_open_button (f.button); g_object_ref (next);
  g_assert_true (next != outer); g_assert_true (find (next, "sample-merged", GTK_TYPE_TOGGLE_BUTTON) == merged);
  g_assert_cmpint (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (merged)), ==, value);
  g_assert_cmpint (clone_popup_enum_value (align), ==, second);
  g_assert_false (standard_toggle_event (next, 0)); g_assert_false (gtk_widget_get_realized (next));
  GtkWidget *last = clone_popup_open_button (f.button); g_object_ref (last); clone_popup_assert_no_action (&f);
  gimp_context_set_tool (gimp_get_user_context (gimp), info ("gimp-paintbrush-tool")); drain ();
  g_assert_false (gtk_widget_get_realized (last));
  g_object_unref (outer); g_object_unref (next); g_object_unref (last); g_module_close (input.module);
  gimp_context_set_pattern (GIMP_CONTEXT (f.options), old_pattern); g_clear_object (&old_pattern);
  clone_popup_finish (&f); bucket_popup_restore_patterns (old_path);
#endif
}

#ifdef GDK_WINDOWING_X11
static void
clone_popup_native_enum_menu (gconstpointer data)
{
  if (!transform_popup_require_x11_input ()) return;
  gboolean perspective = GPOINTER_TO_INT (data);
  const gchar *property = perspective ? "align-mode" : "clone-type";
  RectanglePopupInput input = transform_popup_input ();
  ClonePopupFixture f = clone_popup_fixture (perspective ? "gimp-perspective-clone-tool" : "gimp-clone-tool");
  GimpToolOptions *options = f.options; GtkWidget *button = f.button;
  gint original; g_object_get (options, property, &original, NULL);
  GtkWidget *outer = clone_popup_open_button (button); g_object_ref (outer);
  GtkWidget *combo = find (outer, property, GTK_TYPE_COMBO_BOX); g_assert_nonnull (combo);
  g_object_ref (combo);
  GtkTreeModel *model = gtk_combo_box_get_model (GTK_COMBO_BOX (combo));
  GtkTreeIter row; g_assert_true (gtk_tree_model_get_iter_first (model, &row));
  gint first, second;
  gtk_tree_model_get (model, &row, GIMP_INT_STORE_VALUE, &first, -1);
  g_assert_true (gtk_tree_model_iter_next (model, &row));
  gtk_tree_model_get (model, &row, GIMP_INT_STORE_VALUE, &second, -1);
  g_assert_cmpint (first, !=, second);
  g_object_set (options, property, first, NULL); drain ();
  /* Open the actual enum menu with X-server pointer input, then select via
   * native keyboard. No combo_set_active substitution for the tested action. */
  transform_popup_click_center (&input, combo);
  transform_popup_wait_menu (combo, outer, TRUE);
  rectangle_popup_key (&input, combo, GDK_KEY_Home);
  rectangle_popup_key (&input, combo, GDK_KEY_Down);
  rectangle_popup_key (&input, combo, GDK_KEY_Return);
  transform_popup_wait_menu (combo, outer, FALSE);
  gint value; g_object_get (options, property, &value, NULL);
  g_assert_cmpint (value, ==, second);
  g_assert_true (gtk_widget_get_mapped (outer));
  transform_popup_click_center (&input, combo);
  transform_popup_wait_menu (combo, outer, TRUE);
  rectangle_popup_key (&input, combo, GDK_KEY_Escape);
  transform_popup_wait_menu (combo, outer, FALSE);
  g_object_get (options, property, &value, NULL); g_assert_cmpint (value, ==, second);
  g_assert_true (gtk_widget_get_mapped (outer));
  transform_popup_click_center (&input, combo);
  transform_popup_wait_menu (combo, outer, TRUE);
  GtkWidget *menu = gtk_grab_get_current (); g_object_ref (menu);
  gtk_widget_destroy (outer); drain ();
  gboolean shown = TRUE;
  gint64 deadline = g_get_monotonic_time () + 3 * G_TIME_SPAN_SECOND;
  do {
    g_object_get (combo, "popup-shown", &shown, NULL);
    if (!shown && gtk_grab_get_current () != menu) break;
    g_main_context_iteration (NULL, FALSE); g_usleep (1000);
  } while (g_get_monotonic_time () < deadline);
  g_assert_false (shown);
  g_assert_true (gtk_grab_get_current () != menu && gtk_grab_get_current () != outer);
  g_assert_false (gtk_widget_get_mapped (menu));
  g_assert_false (gtk_widget_get_realized (outer));
  GtkWidget *next = clone_popup_open_button (button);
  g_assert_true (next != outer);
  g_assert_true (find (next, property, GTK_TYPE_COMBO_BOX) == combo);
  g_signal_emit_by_name (next, "cancel"); drain ();
  g_object_set (options, property, original, NULL);
  g_object_unref (menu); g_object_unref (combo); g_object_unref (outer);
  clone_popup_assert_no_action (&f); clone_popup_finish (&f); g_module_close (input.module);
}
#else
static void clone_popup_native_enum_menu (gconstpointer data) { (void) data; g_test_skip ("Actual native enum menu requires X11/XTest"); }
#endif

typedef struct { GtkWidget *button; guint calls; } ClonePopupPending;
static void
clone_popup_pending_reopen (GtkWidget *child, gpointer data)
{
  ClonePopupPending *state = data; (void) child;
  ++state->calls; g_assert_cmpuint (state->calls, ==, 1);
  gtk_button_clicked (GTK_BUTTON (state->button)); drain ();
  g_assert_null (standard_toggle_visible (GTK_TYPE_WINDOW, "painter-clone-options-popup"));
}

static void
clone_popup_pattern_transaction (gconstpointer data)
{
  if (!transform_popup_require_x11_input ()) return;
#ifdef GDK_WINDOWING_X11
  gchar *old_path = bucket_popup_load_patterns ();
  ClonePopupFixture f = clone_popup_fixture (data);
  GimpContext *context = GIMP_CONTEXT (f.options);
  GimpPattern *original = gimp_context_get_pattern (context); if (original) g_object_ref (original);
  g_object_set (f.options, "clone-type", GIMP_CLONE_PATTERN, NULL); drain ();
  GtkWidget *outer = clone_popup_open_button (f.button); g_object_ref (outer);
  GtkWidget *invoker = find (outer, NULL, GIMP_TYPE_VIEWABLE_BUTTON); g_assert_nonnull (invoker);
  gint old_type; g_object_get (f.options, "pattern-view-type", &old_type, NULL);
  g_object_set (invoker, "popup-view-type", GIMP_VIEW_TYPE_GRID, NULL);
  /* The actual stock selector stages its context. Native GTK API row selection
   * is deliberately distinguished from the physical open/Escape/Return keys. */
  GtkWidget *child = bucket_popup_stock_open (outer); g_object_ref (child);
  GimpPattern *selected = bucket_popup_select_other (child, original);
  g_assert_nonnull (gimp_data_get_file (GIMP_DATA (selected)));
  g_assert_true (gimp_context_get_pattern (context) == original);
  transform_popup_real_key (child, GDK_KEY_Escape);
  g_assert_false (gtk_widget_get_realized (child));
  g_assert_true (gimp_context_get_pattern (context) == original); g_assert_true (gtk_grab_get_current () == outer);
  g_object_unref (child); g_object_unref (selected);
  child = bucket_popup_stock_open (outer); g_object_ref (child);
  selected = bucket_popup_select_other (child, original);
  g_assert_true (gimp_context_get_pattern (context) == original);
  transform_popup_real_key (child, GDK_KEY_Return);
  g_assert_false (gtk_widget_get_realized (child)); g_assert_true (gimp_context_get_pattern (context) == selected);
  g_assert_true (gtk_grab_get_current () == outer); clone_popup_assert_no_action (&f);
  g_assert_false (standard_toggle_event (outer, 0));
  g_assert_true (gimp_context_get_pattern (context) == selected); g_object_unref (child);
  GtkWidget *next = clone_popup_open_button (f.button); g_object_ref (next);
  g_assert_true (next != outer); g_assert_true (find (next, NULL, GIMP_TYPE_VIEWABLE_BUTTON) == invoker);
  g_assert_true (gimp_context_get_pattern (context) == selected);
  child = bucket_popup_stock_open (next); g_object_ref (child);
  ClonePopupPending pending = { f.button, 0 };
  g_signal_connect (child, "cancel", G_CALLBACK (clone_popup_pending_reopen), &pending);
  transform_popup_real_key (child, GDK_KEY_Escape);
  g_assert_cmpuint (pending.calls, ==, 1);
  g_assert_false (gtk_widget_get_realized (child)); g_assert_false (gtk_widget_get_realized (next));
  g_assert_true (gtk_grab_get_current () != child && gtk_grab_get_current () != next);
  GtkWidget *last = clone_popup_open_button (f.button); g_object_ref (last);
  GtkWidget *nested = bucket_popup_stock_open (last); g_object_ref (nested);
  gtk_widget_destroy (last); drain ();
  g_assert_false (gtk_widget_get_realized (nested)); g_assert_false (gtk_widget_get_realized (last));
  GtkWidget *again = clone_popup_open_button (f.button); g_object_ref (again);
  GtkWidget *final_child = bucket_popup_stock_open (again); g_object_ref (final_child);
  clone_popup_assert_no_action (&f);
  gimp_context_set_tool (gimp_get_user_context (gimp), info ("gimp-paintbrush-tool")); drain ();
  g_assert_false (gtk_widget_get_realized (final_child)); g_assert_false (gtk_widget_get_realized (again));
  g_assert_true (gimp_context_get_pattern (context) == selected);
  /* The tool switch destroys the old invoker. Restore the surviving model
   * preference after stock-child closure, never dereference that widget. */
  g_object_set (f.options, "pattern-view-type", old_type, NULL);
  gimp_context_set_pattern (context, original); g_clear_object (&original); g_object_unref (selected);
  g_object_unref (child); g_object_unref (nested); g_object_unref (final_child);
  g_object_unref (outer); g_object_unref (next); g_object_unref (last); g_object_unref (again);
  clone_popup_finish (&f); bucket_popup_restore_patterns (old_path);
#endif
}

static void
clone_popup_factory_caller_release (void)
{
  /* Old Heal was inline, not an Options popup. Its modern grouping remains
   * an explicit residual; this task must not silently convert it. */
  GtkWidget *heal = gimp_tools_create_painter_toolbar_gui (info ("gimp-heal-tool")->tool_options);
  g_assert_nonnull (heal); g_object_ref_sink (heal);
  GtkWidget *untouched = find (heal, "painter-paint-source", GTK_TYPE_MENU_BUTTON); g_assert_nonnull (untouched);
  g_assert_true (GTK_IS_POPOVER (gtk_menu_button_get_popover (GTK_MENU_BUTTON (untouched))));
  g_assert_cmpstr (gtk_button_get_label (GTK_BUTTON (untouched)), ==, _("Source"));
  gtk_widget_destroy (heal); g_object_unref (heal);
  const gchar *ids[] = { "gimp-clone-tool", "gimp-perspective-clone-tool" };
  for (guint i = 0; i < G_N_ELEMENTS (ids); ++i) {
    GimpToolInfo *tool = info (ids[i]);
    GimpToolOptions *options = g_object_new (G_OBJECT_TYPE (tool->tool_options), "gimp", gimp, "tool-info", tool, NULL);
    gimp_context_copy_properties (GIMP_CONTEXT (tool->tool_options), GIMP_CONTEXT (options), GIMP_CONTEXT_PROP_MASK_ALL);
    GimpToolOptionsGUIFunc factory = g_object_get_data (G_OBJECT (tool), "gimp-tool-options-gui-horizontal-func"); g_assert_nonnull (factory);
    GtkWidget *gui = factory (options); g_assert_nonnull (gui);
    GtkWidget *window = gtk_window_new (GTK_WINDOW_TOPLEVEL); g_object_ref_sink (window);
    gtk_container_add (GTK_CONTAINER (window), gui); gtk_widget_show_all (window); drain ();
    GtkWidget *button = find (gui, "painter-paint-source", GTK_TYPE_BUTTON); g_assert_nonnull (button); g_object_ref (button);
    gboolean finalized = FALSE; g_object_weak_ref (G_OBJECT (options), transform_popup_model_finalized, &finalized);
    gtk_button_clicked (GTK_BUTTON (button)); drain ();
    GtkWidget *outer = standard_toggle_visible (GTK_TYPE_WINDOW, "painter-clone-options-popup"); g_assert_nonnull (outer); g_object_ref (outer);
    g_assert_cmpint (gtk_window_get_window_type (GTK_WINDOW (outer)), ==, GTK_WINDOW_POPUP);
    g_object_unref (options); g_assert_false (finalized);
    GtkWidget *control = find (outer, "sample-merged", GTK_TYPE_TOGGLE_BUTTON); g_assert_nonnull (control);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (control), TRUE); drain ();
    gboolean value; g_object_get (options, "sample-merged", &value, NULL); g_assert_true (value);
    gtk_widget_hide (window); drain (); g_assert_false (gtk_widget_get_realized (outer)); g_assert_false (finalized);
    gtk_widget_show_all (window); drain (); gtk_button_clicked (GTK_BUTTON (button)); drain ();
    GtkWidget *next = standard_toggle_visible (GTK_TYPE_WINDOW, "painter-clone-options-popup"); g_assert_nonnull (next); g_object_ref (next);
    g_assert_true (next != outer); g_assert_true (find (next, "sample-merged", GTK_TYPE_TOGGLE_BUTTON) == control);
    g_assert_true (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (control)));
    gtk_widget_destroy (window); g_object_unref (window); drain ();
    g_assert_false (gtk_widget_get_realized (next)); g_object_unref (next); g_object_unref (outer); drain ();
    g_assert_true (finalized); gtk_button_clicked (GTK_BUTTON (button)); drain ();
    g_assert_null (standard_toggle_visible (GTK_TYPE_WINDOW, "painter-clone-options-popup")); g_object_unref (button);
  }
}

static void
register_clone_popup_tests (void)
{
  g_test_add_data_func ("/clone-native-popup/actual-clone", "gimp-clone-tool", clone_popup_actual_entry);
  g_test_add_data_func ("/clone-native-popup/actual-perspective-clone", "gimp-perspective-clone-tool", clone_popup_actual_entry);
  g_test_add_data_func ("/clone-native-popup/clone-source-native-menu", GINT_TO_POINTER (0), clone_popup_native_enum_menu);
  g_test_add_data_func ("/clone-native-popup/perspective-alignment-native-menu", GINT_TO_POINTER (1), clone_popup_native_enum_menu);
  g_test_add_data_func ("/clone-native-popup/clone-pattern-transaction-lifetime", "gimp-clone-tool", clone_popup_pattern_transaction);
  g_test_add_data_func ("/clone-native-popup/perspective-pattern-transaction-lifetime", "gimp-perspective-clone-tool", clone_popup_pattern_transaction);
  g_test_add_func ("/clone-native-popup/factory-caller-release-heal-control", clone_popup_factory_caller_release);
}
