/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Actual registered Bucket route. Modern field layout is deliberately retained;
 * this verifies carrier/input/binding contracts, not full old visual parity. */
#include "tools/gimpbucketfilloptions.h"

static gboolean
bucket_popup_position_hook (GSignalInvocationHint *hint, guint count,
                               const GValue *values, gpointer data)
{
  TransformPopupPosition *record = data;
  (void) hint; (void) count;
  GtkWidget *widget = g_value_get_object (&values[0]);
  if (g_strcmp0 (gtk_widget_get_name (widget), "painter-bucket-details-popup")) return TRUE;
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
bucket_popup_open_button (GtkWidget *button)
{
  transform_popup_reveal_button (button);
  TransformPopupPosition realize = {0}; realize.button = button;
  guint realize_signal = g_signal_lookup ("realize", GTK_TYPE_WIDGET);
  gulong realize_hook = g_signal_add_emission_hook (realize_signal, 0, bucket_popup_position_hook, &realize, NULL);
  GList *before = gtk_window_list_toplevels ();
#ifdef GDK_WINDOWING_X11
  RectanglePopupInput input = transform_popup_input ();
  transform_popup_click_center (&input, button); g_module_close (input.module);
#else
  gtk_button_clicked (GTK_BUTTON (button));
#endif
  drain ();
  g_signal_remove_emission_hook (realize_signal, realize_hook);
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
  g_list_free (before);
  g_assert_nonnull (actual);
  g_test_message ("Actual independent Details: invoker=%s carrier=%s",
                  G_OBJECT_TYPE_NAME (button), G_OBJECT_TYPE_NAME (actual));
  g_assert_true (GTK_IS_WINDOW (actual));
  g_assert_cmpint (gtk_window_get_window_type (GTK_WINDOW (actual)), ==, GTK_WINDOW_POPUP);
  g_assert_true (gtk_widget_has_grab (actual));
  g_assert_cmpint (gtk_button_get_relief (GTK_BUTTON (button)), ==, GTK_RELIEF_NONE);
  GtkWidget *label = find (button, NULL, GTK_TYPE_LABEL);
  g_assert_nonnull (label);
  g_assert_cmpstr (atk_object_get_name (gtk_widget_get_accessible (button)), ==,
                   gtk_label_get_text (GTK_LABEL (label)));
  g_assert_cmpuint (strlen (gtk_label_get_text (GTK_LABEL (label))), >, 0);
  /* Legacy show_over snapshots the button before synchronous show(); it does
   * not promise to track a later scroller allocation. Assert that exact
   * opening rectangle/edge flip without a tolerance or artificial reanchor. */
  g_assert_true (realize.captured); g_assert_false (realize.mapped);
  GdkMonitor *monitor = gdk_display_get_monitor_at_point (gtk_widget_get_display (button), realize.left, realize.top);
  GdkRectangle rect; gdk_monitor_get_geometry (monitor, &rect);
  gint x = realize.left, y = realize.top + realize.anchor_height, actual_x, actual_y;
  if (x + realize.width > rect.x + rect.width) x = realize.left + realize.anchor_width - realize.width;
  if (y + realize.height > rect.y + rect.height) y = realize.top - realize.height;
  gtk_window_get_position (GTK_WINDOW (actual), &actual_x, &actual_y);
  g_assert_cmpint (actual_x, ==, x); g_assert_cmpint (actual_y, ==, y);
  gint later_x, later_y; GtkAllocation later; gtk_widget_get_allocation (button, &later);
  gdk_window_get_origin (gtk_widget_get_window (button), &later_x, &later_y);
  if (!gtk_widget_get_has_window (button)) { later_x += later.x; later_y += later.y; }
  g_test_message ("Bucket opening anchor=%d/%d %dx%d popup=%d/%d %dx%d later-anchor=%d/%d",
    realize.left, realize.top, realize.anchor_width, realize.anchor_height,
    actual_x, actual_y, realize.width, realize.height, later_x, later_y);
  return actual;
}

static GtkWidget *
bucket_popup_canvas_button (GimpDisplayShell *shell, GimpToolOptions *options)
{
  GPtrArray *widgets = g_ptr_array_new (); gather (shell->canvas, widgets);
  GtkWidget *button = NULL;
  for (guint i = 0; i < widgets->len; ++i) {
    GtkWidget *w = g_ptr_array_index (widgets, i);
    GtkWidget *parent = gtk_widget_get_parent (w);
    if (!g_strcmp0 (gtk_widget_get_name (w), "gimp-painter-fill-details") &&
        parent && g_object_get_data (G_OBJECT (parent), "gimp-painter-toolbar-options") == options &&
        gtk_widget_get_ancestor (w, GIMP_TYPE_DISPLAY_SHELL) == GTK_WIDGET (shell)) {
      g_assert_null (button); button = w;
    }
  }
  g_ptr_array_unref (widgets); g_assert_nonnull (button); return button;
}

static GimpDisplayShell *
bucket_popup_canvas (GtkWidget **button)
{
  g_assert_true (gimp->config->painter_canvas_ui);
  gboolean single = FALSE; g_object_get (gimp->config, "single-window-mode", &single, NULL);
  g_assert_true (single);
  GimpDisplayShell *shell = new_shell ();
  GimpToolInfo *tool = info ("gimp-bucket-fill-tool");
  gimp_context_set_tool (gimp_get_user_context (gimp), tool); drain ();
  gimp_painter_canvas_ui_pointer (shell, 0, 0, TRUE); drain ();
  *button = bucket_popup_canvas_button (shell, tool->tool_options);
  if (g_getenv ("GIMP_BUCKET_POPUP_CAPTURE")) {
    gtk_window_maximize (GTK_WINDOW (gtk_widget_get_toplevel (*button)));
    gint64 until = g_get_monotonic_time () + G_TIME_SPAN_SECOND;
    while (g_get_monotonic_time () < until) { g_main_context_iteration (NULL, FALSE); g_usleep (1000); }
  }
  return shell;
}

static void
bucket_popup_capture (GtkWidget *outer)
{
  gint px, py; gdk_window_get_origin (gtk_widget_get_window (outer), &px, &py);
  gint scale = gdk_window_get_scale_factor (gtk_widget_get_window (outer));
  gint width = gtk_widget_get_allocated_width (outer), height = gtk_widget_get_allocated_height (outer);
  GdkMonitor *monitor = gdk_display_get_monitor_at_window (gtk_widget_get_display (outer), gtk_widget_get_window (outer));
  GdkRectangle bounds; gdk_monitor_get_geometry (monitor, &bounds);
  g_assert_cmpint (px, >=, bounds.x); g_assert_cmpint (py, >=, bounds.y);
  g_assert_cmpint (px + width, <=, bounds.x + bounds.width);
  g_assert_cmpint (py + height, <=, bounds.y + bounds.height);
  g_test_message ("Bucket physical popup=%d/%d %dx%d scale=%d", px * scale, py * scale, width * scale, height * scale, scale);
  const gchar *names[] = { "threshold", "fill-area", "fill-criterion" };
  for (guint i = 0; i < G_N_ELEMENTS (names); ++i) {
    GtkWidget *control = find (outer, names[i], GTK_TYPE_WIDGET); g_assert_nonnull (control);
    gint x, y; g_assert_true (gtk_widget_translate_coordinates (control, outer, 0, 0, &x, &y));
    gint w = gtk_widget_get_allocated_width (control), h = gtk_widget_get_allocated_height (control);
    g_assert_true (gtk_widget_get_mapped (control));
    g_assert_cmpint (x, >=, 0); g_assert_cmpint (y, >=, 0);
    g_assert_cmpint (x + w, <=, width); g_assert_cmpint (y + h, <=, height);
    g_test_message ("Bucket physical control %s=%d/%d %dx%d", names[i], (px+x)*scale, (py+y)*scale, w*scale, h*scale);
  }
  const gchar *capture = g_getenv ("GIMP_BUCKET_POPUP_CAPTURE");
  if (!capture) return;
  GdkWindow *root = gdk_screen_get_root_window (gtk_widget_get_screen (outer));
  GdkPixbuf *pixels = gdk_pixbuf_get_from_window (root, 0, 0,
    gdk_window_get_width (root), gdk_window_get_height (root));
  g_assert_nonnull (pixels); GError *error = NULL;
  g_assert_true (gdk_pixbuf_save (pixels, capture, "png", &error, NULL)); g_assert_no_error (error);
  g_object_unref (pixels);
  const gchar *pause = g_getenv ("GIMP_BUCKET_POPUP_PAUSE");
  if (pause) {
    gint64 until = g_get_monotonic_time () + g_ascii_strtoll (pause, NULL, 10) * G_TIME_SPAN_SECOND;
    while (g_get_monotonic_time () < until) { g_main_context_iteration (NULL, FALSE); g_usleep (1000); }
  }
}

static gchar *bucket_popup_load_patterns (void);
static void bucket_popup_restore_patterns (gchar *path);

static void
bucket_popup_actual_entry_baseline (void)
{
  if (!transform_popup_require_x11_input ()) return;
#ifdef GDK_WINDOWING_X11
  gchar *old_path = bucket_popup_load_patterns ();
  GtkWidget *button; GimpDisplayShell *shell = bucket_popup_canvas (&button);
  GimpToolOptions *options = info ("gimp-bucket-fill-tool")->tool_options;
  GimpPattern *old_pattern = gimp_context_get_pattern (GIMP_CONTEXT (options));
  if (old_pattern) g_object_ref (old_pattern);
  GimpContainer *patterns = gimp_data_factory_get_container (gimp->pattern_factory);
  GimpPattern *real_pattern = NULL;
  for (gint i = 0; i < gimp_container_get_n_children (patterns); ++i) {
    GimpPattern *candidate = GIMP_PATTERN (gimp_container_get_child_by_index (patterns, i));
    if (gimp_data_get_file (GIMP_DATA (candidate))) { real_pattern = candidate; break; }
  }
  g_assert_nonnull (real_pattern);
  gimp_context_set_pattern (GIMP_CONTEXT (options), real_pattern);
  GimpBucketFillOptions *bucket = GIMP_BUCKET_FILL_OPTIONS (options);
  GimpTool *active_tool = g_object_ref (tool_manager_get_active (gimp));
  GimpImage *image = gimp_display_get_image (shell->display);
  gint undo_depth = gimp_undo_stack_get_depth (gimp_image_get_undo_stack (image));
  GtkWidget *dock = gimp_tools_get_tool_options_gui (options), *dock_parent = gtk_widget_get_parent (dock);
  GtkWidget *busy = bucket->line_art_busy_box; GimpStrokeOptions *stroke = bucket->stroke_options;
  gdouble original; gint original_area; gboolean original_transparent;
  g_object_get (options, "threshold", &original, "fill-area", &original_area,
                "fill-transparent", &original_transparent, NULL);
  g_object_set (options, "fill-area", GIMP_BUCKET_FILL_SIMILAR_COLORS, NULL); drain ();
  /* Establish the actual on-canvas carrier before auxiliary peer setup, so
   * exact old-source negatives fail this production route, not a fixture. */
  GtkWidget *entry = bucket_popup_open_button (button); g_object_ref (entry);
  g_signal_emit_by_name (entry, "cancel"); drain ();
  g_assert_false (gtk_widget_get_realized (entry)); g_object_unref (entry);
  GtkWidget *peer_gui = gimp_tools_create_painter_toolbar_gui (options); g_object_ref_sink (peer_gui);
  GtkWidget *peer_button = find (peer_gui, "gimp-painter-fill-details", GTK_TYPE_BUTTON);
  GtkWidget *peer_window = gtk_window_new (GTK_WINDOW_TOPLEVEL); g_object_ref_sink (peer_window);
  gtk_container_add (GTK_CONTAINER (peer_window), peer_gui); gtk_widget_show_all (peer_window); drain ();
  gtk_button_clicked (GTK_BUTTON (peer_button)); drain ();
  GtkWidget *peer_body = standard_toggle_visible (GTK_TYPE_WINDOW, "painter-bucket-details-popup");
  g_assert_nonnull (peer_body);
  GtkWidget *peer = find (peer_body, "threshold", GTK_TYPE_SPIN_BUTTON);
  GtkWidget *dock_control = find (dock, "threshold", GTK_TYPE_SPIN_BUTTON);
  g_assert_nonnull (peer); g_assert_nonnull (dock_control);
  g_signal_emit_by_name (peer_body, "cancel"); gtk_widget_hide (peer_window); drain ();
  GtkWidget *outer = bucket_popup_open_button (button); g_object_ref (outer);
  GtkWidget *control = find (outer, "threshold", GTK_TYPE_SPIN_BUTTON);
  GtkWidget *transparent = find (outer, "fill-transparent", GTK_TYPE_TOGGLE_BUTTON);
  g_assert_nonnull (control); g_assert_nonnull (transparent);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (control), 37); drain ();
  gdouble value; g_object_get (options, "threshold", &value, NULL);
  g_assert_cmpfloat_with_epsilon (value, 37, 1e-6);
  g_assert_cmpfloat_with_epsilon (gtk_spin_button_get_value (GTK_SPIN_BUTTON (peer)), 37, 1e-6);
  g_assert_cmpfloat_with_epsilon (gtk_spin_button_get_value (GTK_SPIN_BUTTON (dock_control)), 37, 1e-6);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (peer), 42); drain ();
  g_assert_cmpfloat_with_epsilon (gtk_spin_button_get_value (GTK_SPIN_BUTTON (control)), 42, 1e-6);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (dock_control), 29); drain ();
  g_assert_cmpfloat_with_epsilon (gtk_spin_button_get_value (GTK_SPIN_BUTTON (control)), 29, 1e-6);
  g_assert_cmpfloat_with_epsilon (gtk_spin_button_get_value (GTK_SPIN_BUTTON (peer)), 29, 1e-6);
  const gint areas[] = { GIMP_BUCKET_FILL_SELECTION, GIMP_BUCKET_FILL_LINE_ART, GIMP_BUCKET_FILL_SIMILAR_COLORS };
  for (guint i = 0; i < G_N_ELEMENTS (areas); ++i) {
    g_object_set (options, "fill-area", areas[i], NULL); drain ();
    gboolean sensitive = areas[i] == GIMP_BUCKET_FILL_SIMILAR_COLORS;
    g_assert_cmpint (gtk_widget_is_sensitive (control), ==, sensitive);
    g_assert_cmpint (gtk_widget_is_sensitive (transparent), ==, sensitive);
    g_assert_cmpint (gtk_widget_is_sensitive (peer), ==, sensitive);
    g_assert_true (gtk_widget_is_sensitive (button));
    g_assert_true (gtk_widget_is_sensitive (find (outer, "fill-area", GTK_TYPE_COMBO_BOX)));
    g_assert_true (gtk_widget_is_sensitive (find (outer, NULL, GIMP_TYPE_VIEWABLE_BUTTON)));
  }
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (transparent), !original_transparent); drain ();
  gboolean flag; g_object_get (options, "fill-transparent", &flag, NULL);
  g_assert_cmpint (flag, ==, !original_transparent);
  RectanglePopupInput input = transform_popup_input ();
  transform_popup_click_center (&input, control);
  g_assert_true (gtk_window_get_focus (GTK_WINDOW (outer)) == control);
  const guint keys[] = { GDK_KEY_Return, GDK_KEY_KP_Enter, GDK_KEY_space };
  for (guint i = 0; i < G_N_ELEMENTS (keys); ++i) {
    rectangle_popup_key (&input, outer, keys[i]); g_assert_true (gtk_widget_get_mapped (outer));
    g_assert_true (tool_manager_get_active (gimp) == active_tool);
    g_assert_cmpint (gimp_undo_stack_get_depth (gimp_image_get_undo_stack (image)), ==, undo_depth);
  }
  GdkDisplay *display = gtk_widget_get_display (outer); gpointer native = gdk_x11_display_get_xdisplay (display);
  GdkKeymapKey *entries = NULL; gint count = 0;
  g_assert_true (gdk_keymap_get_entries_for_keyval (gdk_keymap_get_for_display (display), GDK_KEY_Control_L, &entries, &count));
  g_assert_cmpint (count, >, 0);
  input.key (native, entries[0].keycode, TRUE, 0); rectangle_popup_key (&input, outer, GDK_KEY_a);
  input.key (native, entries[0].keycode, FALSE, 0); g_free (entries);
  rectangle_popup_key (&input, outer, GDK_KEY_5); rectangle_popup_key (&input, outer, GDK_KEY_3);
  rectangle_popup_key (&input, outer, GDK_KEY_Return);
  g_object_get (options, "threshold", &value, NULL); g_assert_cmpfloat_with_epsilon (value, 53, 1e-6);
  g_assert_cmpfloat_with_epsilon (gtk_spin_button_get_value (GTK_SPIN_BUTTON (peer)), 53, 1e-6);
  g_assert_cmpfloat_with_epsilon (gtk_spin_button_get_value (GTK_SPIN_BUTTON (dock_control)), 53, 1e-6);
  g_assert_true (tool_manager_get_active (gimp) == active_tool);
  g_assert_cmpint (gimp_undo_stack_get_depth (gimp_image_get_undo_stack (image)), ==, undo_depth);
  bucket_popup_capture (outer);
  rectangle_popup_key (&input, outer, GDK_KEY_Escape); drain ();
  g_assert_false (gtk_widget_get_realized (outer));
  GtkWidget *next = bucket_popup_open_button (button); g_object_ref (next);
  g_assert_true (next != outer); g_assert_true (find (next, "threshold", GTK_TYPE_SPIN_BUTTON) == control);
  g_assert_cmpfloat_with_epsilon (gtk_spin_button_get_value (GTK_SPIN_BUTTON (control)), 53, 1e-6);
  g_assert_false (standard_toggle_event (next, 0)); g_assert_false (gtk_widget_get_realized (next));
  GtkWidget *last = bucket_popup_open_button (button); g_object_ref (last);
  g_assert_true (gtk_widget_get_parent (dock) == dock_parent);
  g_assert_true (bucket->line_art_busy_box == busy); g_assert_true (bucket->stroke_options == stroke);
  g_assert_true (tool_manager_get_active (gimp) == active_tool);
  g_assert_cmpint (gimp_undo_stack_get_depth (gimp_image_get_undo_stack (image)), ==, undo_depth);
  g_object_unref (active_tool);
  gimp_context_set_tool (gimp_get_user_context (gimp), info ("gimp-paintbrush-tool")); drain ();
  g_assert_false (gtk_widget_get_realized (last));
  g_assert_true (gimp_tools_get_tool_options_gui (options) == dock);
  g_object_set (options, "threshold", original, "fill-area", original_area,
                "fill-transparent", original_transparent, NULL);
  gtk_widget_destroy (peer_window); g_object_unref (peer_window); g_object_unref (peer_gui);
  g_object_unref (outer); g_object_unref (next); g_object_unref (last);
  gimp_context_set_pattern (GIMP_CONTEXT (options), old_pattern); g_clear_object (&old_pattern);
  close_shell (shell); g_module_close (input.module); bucket_popup_restore_patterns (old_path);
#endif
}

#ifdef GDK_WINDOWING_X11
static void
bucket_popup_native_enum_menu (gconstpointer data)
{
  if (!transform_popup_require_x11_input ()) return;
  const gchar *property = data;
  RectanglePopupInput input = {0};
  input.module = g_module_open ("libXtst.so.6", G_MODULE_BIND_LAZY);
  g_assert_nonnull (input.module);
  g_assert_true (g_module_symbol (input.module, "XTestFakeMotionEvent", (gpointer *) &input.motion));
  g_assert_true (g_module_symbol (input.module, "XTestFakeButtonEvent", (gpointer *) &input.button));
  g_assert_true (g_module_symbol (input.module, "XTestFakeKeyEvent", (gpointer *) &input.key));
  GtkWidget *button; GimpDisplayShell *shell = bucket_popup_canvas (&button);
  GimpToolInfo *tool = info ("gimp-bucket-fill-tool");
  GimpToolOptions *options = tool->tool_options;
  gint original; g_object_get (options, property, &original, NULL);
  gimp_context_set_tool (gimp_get_user_context (gimp), tool); drain ();
  gimp_painter_canvas_ui_pointer (shell, 0, 0, TRUE); drain ();
  gint original_area; g_object_get (options, "fill-area", &original_area, NULL);
  g_object_set (options, "fill-area", GIMP_BUCKET_FILL_SIMILAR_COLORS, NULL); drain ();
  GtkWidget *outer = bucket_popup_open_button (button); g_object_ref (outer);
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
  GtkWidget *next = bucket_popup_open_button (button);
  g_assert_true (next != outer);
  g_assert_true (find (next, property, GTK_TYPE_COMBO_BOX) == combo);
  g_signal_emit_by_name (next, "cancel"); drain ();
  g_object_set (options, property, original, NULL);
  g_object_unref (menu); g_object_unref (combo); g_object_unref (outer);
  g_object_set (options, "fill-area", original_area, NULL);
  close_shell (shell); g_module_close (input.module);
}
#else
static void bucket_popup_native_enum_menu (gconstpointer data) { (void) data; g_test_skip ("Actual native enum menu requires X11/XTest"); }
#endif

static gchar *
bucket_popup_load_patterns (void)
{
  gchar *old_path = NULL; g_object_get (gimp->config, "pattern-path", &old_path, NULL);
  gchar *path = g_build_filename (g_getenv ("GIMP_TESTING_ABS_TOP_SRCDIR"), "gimp-data", "patterns", NULL);
  g_object_set (gimp->config, "pattern-path", path, NULL); g_free (path);
  gimp_data_factory_data_refresh (gimp->pattern_factory, gimp_get_user_context (gimp));
  g_assert_cmpint (gimp_container_get_n_children (gimp_data_factory_get_container (gimp->pattern_factory)), >, 1);
  return old_path;
}

static void
bucket_popup_restore_patterns (gchar *path)
{
  g_object_set (gimp->config, "pattern-path", path, NULL); g_free (path);
  gimp_data_factory_data_refresh (gimp->pattern_factory, gimp_get_user_context (gimp));
}

#ifdef GDK_WINDOWING_X11
static GtkWidget *
bucket_popup_stock_open (GtkWidget *outer)
{
  GtkWidget *button = find (outer, NULL, GIMP_TYPE_VIEWABLE_BUTTON); g_assert_nonnull (button);
  RectanglePopupInput input = transform_popup_input ();
  transform_popup_click_center (&input, button); g_module_close (input.module);
  GtkWidget *child = standard_toggle_visible (GIMP_TYPE_CONTAINER_POPUP, NULL); g_assert_nonnull (child);
  g_object_ref (child);
  gint64 deadline = g_get_monotonic_time () + 3 * G_TIME_SPAN_SECOND;
  while (gtk_grab_get_current () != child && gtk_widget_get_visible (child) && g_get_monotonic_time () < deadline) {
    g_main_context_iteration (NULL, FALSE); g_usleep (1000);
  }
  g_assert_true (gtk_grab_get_current () == child);
  g_assert_true (gtk_window_get_transient_for (GTK_WINDOW (child)) == GTK_WINDOW (outer));
  g_object_unref (child); return child;
}

static GimpPattern *
bucket_popup_select_other (GtkWidget *child, GimpPattern *before)
{
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
  return selected;
}
#endif

static void
bucket_popup_pattern_transaction (void)
{
  if (!transform_popup_require_x11_input ()) return;
#ifdef GDK_WINDOWING_X11
  gchar *old_path = bucket_popup_load_patterns ();
  GtkWidget *button; GimpDisplayShell *shell = bucket_popup_canvas (&button);
  GimpContext *context = GIMP_CONTEXT (info ("gimp-bucket-fill-tool")->tool_options);
  GimpPattern *original = gimp_context_get_pattern (context); if (original) g_object_ref (original);
  GtkWidget *outer = bucket_popup_open_button (button); g_object_ref (outer);
  GtkWidget *invoker = find (outer, NULL, GIMP_TYPE_VIEWABLE_BUTTON); g_assert_nonnull (invoker);
  gint old_type; g_object_get (invoker, "popup-view-type", &old_type, NULL);
  g_object_set (invoker, "popup-view-type", GIMP_VIEW_TYPE_GRID, NULL);
  GtkWidget *child = bucket_popup_stock_open (outer); g_object_ref (child);
  GimpPattern *selected = bucket_popup_select_other (child, original);
  g_assert_true (gimp_context_get_pattern (context) == original);
  transform_popup_real_key (child, GDK_KEY_Escape);
  g_assert_false (gtk_widget_get_realized (child));
  g_assert_true (gimp_context_get_pattern (context) == original);
  g_assert_true (gtk_grab_get_current () == outer);
  g_object_unref (child); g_object_unref (selected);
  child = bucket_popup_stock_open (outer); g_object_ref (child);
  selected = bucket_popup_select_other (child, original);
  g_assert_true (gimp_context_get_pattern (context) == original);
  transform_popup_real_key (child, GDK_KEY_Return);
  g_assert_false (gtk_widget_get_realized (child));
  g_assert_true (gimp_context_get_pattern (context) == selected);
  g_assert_true (gtk_grab_get_current () == outer);
  g_assert_false (standard_toggle_event (outer, 0));
  g_assert_true (gimp_context_get_pattern (context) == selected);
  GtkWidget *next = bucket_popup_open_button (button); g_object_ref (next);
  g_assert_true (next != outer); g_assert_true (find (next, NULL, GIMP_TYPE_VIEWABLE_BUTTON) == invoker);
  g_assert_true (gimp_context_get_pattern (context) == selected);
  g_object_set (invoker, "popup-view-type", old_type, NULL);
  g_signal_emit_by_name (next, "cancel"); drain ();
  gimp_context_set_pattern (context, original); g_clear_object (&original); g_object_unref (selected);
  g_object_unref (child); g_object_unref (next); g_object_unref (outer);
  close_shell (shell); bucket_popup_restore_patterns (old_path);
#endif
}

typedef struct { GtkWidget *outer, *button; guint calls; } BucketPopupPending;
static void
bucket_popup_pending_reopen (GtkWidget *child, gpointer data)
{
  BucketPopupPending *request = data; (void) child;
  ++request->calls; g_assert_cmpuint (request->calls, ==, 1);
  /* open() itself retires the old host during an in-flight stock cancel.
   * It must recheck pending children before trying to acquire a new grab. */
  gtk_button_clicked (GTK_BUTTON (request->button)); drain ();
  g_assert_null (standard_toggle_visible (GTK_TYPE_WINDOW, "painter-bucket-details-popup"));
}

static void
bucket_popup_pending_lifetime (void)
{
  if (!transform_popup_require_x11_input ()) return;
#ifdef GDK_WINDOWING_X11
  gchar *old_path = bucket_popup_load_patterns ();
  GtkWidget *button; GimpDisplayShell *shell = bucket_popup_canvas (&button);
  GtkWidget *outer = bucket_popup_open_button (button); g_object_ref (outer);
  GtkWidget *child = bucket_popup_stock_open (outer); g_object_ref (child);
  BucketPopupPending request = { outer, button, 0 };
  g_signal_connect (child, "cancel", G_CALLBACK (bucket_popup_pending_reopen), &request);
  transform_popup_real_key (child, GDK_KEY_Escape);
  g_assert_cmpuint (request.calls, ==, 1); g_assert_false (gtk_widget_get_realized (child));
  g_assert_false (gtk_widget_get_realized (outer));
  g_assert_true (gtk_grab_get_current () != outer && gtk_grab_get_current () != child);
  GtkWidget *next = bucket_popup_open_button (button); g_object_ref (next); g_assert_true (next != outer);
  GtkWidget *second = bucket_popup_stock_open (next); g_object_ref (second);
  gtk_widget_destroy (next); drain ();
  g_assert_false (gtk_widget_get_realized (second)); g_assert_false (gtk_widget_get_realized (next));
  GtkWidget *last = bucket_popup_open_button (button); g_object_ref (last);
  GtkWidget *third = bucket_popup_stock_open (last); g_object_ref (third);
  /* Actual owner unmap on tool switch, with the stock selector still open. */
  gimp_context_set_tool (gimp_get_user_context (gimp), info ("gimp-paintbrush-tool")); drain ();
  g_assert_false (gtk_widget_get_realized (third)); g_assert_false (gtk_widget_get_realized (last));
  g_object_unref (third); g_object_unref (last); g_object_unref (second); g_object_unref (next);
  g_object_unref (child); g_object_unref (outer); close_shell (shell); bucket_popup_restore_patterns (old_path);
#endif
}

static void
bucket_popup_factory_caller_release (void)
{
  /* The generic factory still serves other, differently grouped routes.
   * Bucket's named dispatch must not silently convert Gradient Details. */
  GtkWidget *gradient = gimp_tools_create_painter_toolbar_gui (info ("gimp-gradient-tool")->tool_options);
  g_assert_nonnull (gradient); g_object_ref_sink (gradient);
  GtkWidget *other = find (gradient, "gimp-painter-gradient-details", GTK_TYPE_MENU_BUTTON);
  g_assert_nonnull (other);
  g_assert_true (GTK_IS_POPOVER (gtk_menu_button_get_popover (GTK_MENU_BUTTON (other))));
  gtk_widget_destroy (gradient); g_object_unref (gradient);
  GimpToolInfo *tool = info ("gimp-bucket-fill-tool");
  GimpToolOptions *options = g_object_new (G_OBJECT_TYPE (tool->tool_options), "gimp", gimp, "tool-info", tool, NULL);
  gimp_context_copy_properties (GIMP_CONTEXT (tool->tool_options), GIMP_CONTEXT (options), GIMP_CONTEXT_PROP_MASK_ALL);
  GimpToolOptionsGUIFunc factory = g_object_get_data (G_OBJECT (tool), "gimp-tool-options-gui-horizontal-func");
  g_assert_nonnull (factory);
  GtkWidget *gui = factory (options); g_assert_nonnull (gui);
  GtkWidget *window = gtk_window_new (GTK_WINDOW_TOPLEVEL); g_object_ref_sink (window);
  gtk_container_add (GTK_CONTAINER (window), gui); gtk_widget_show_all (window); drain ();
  GtkWidget *button = find (gui, "gimp-painter-fill-details", GTK_TYPE_BUTTON); g_assert_nonnull (button); g_object_ref (button);
  gboolean finalized = FALSE;
  g_object_weak_ref (G_OBJECT (options), transform_popup_model_finalized, &finalized);
  gtk_button_clicked (GTK_BUTTON (button)); drain ();
  GtkWidget *outer = standard_toggle_visible (GTK_TYPE_WINDOW, "painter-bucket-details-popup"); g_assert_nonnull (outer); g_object_ref (outer);
  g_assert_cmpint (gtk_window_get_window_type (GTK_WINDOW (outer)), ==, GTK_WINDOW_POPUP);
  g_object_unref (options); g_assert_false (finalized);
  GtkWidget *control = find (outer, "threshold", GTK_TYPE_SPIN_BUTTON); g_assert_nonnull (control);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (control), 67); drain ();
  gdouble value; g_object_get (options, "threshold", &value, NULL); g_assert_cmpfloat_with_epsilon (value, 67, 1e-6);
  gtk_widget_hide (window); drain (); g_assert_false (gtk_widget_get_realized (outer)); g_assert_false (finalized);
  gtk_widget_show_all (window); drain (); gtk_button_clicked (GTK_BUTTON (button)); drain ();
  GtkWidget *next = standard_toggle_visible (GTK_TYPE_WINDOW, "painter-bucket-details-popup"); g_assert_nonnull (next); g_object_ref (next);
  g_assert_true (next != outer); g_assert_true (find (next, "threshold", GTK_TYPE_SPIN_BUTTON) == control);
  g_assert_cmpfloat_with_epsilon (gtk_spin_button_get_value (GTK_SPIN_BUTTON (control)), 67, 1e-6);
  gtk_widget_destroy (window); g_object_unref (window); drain ();
  g_assert_false (gtk_widget_get_realized (next)); g_object_unref (next); g_object_unref (outer); drain ();
  g_assert_true (finalized);
  gtk_button_clicked (GTK_BUTTON (button)); drain ();
  g_assert_null (standard_toggle_visible (GTK_TYPE_WINDOW, "painter-bucket-details-popup")); g_object_unref (button);
}

static void
register_bucket_popup_tests (void)
{
  g_test_add_func ("/bucket-native-popup/actual-canvas", bucket_popup_actual_entry_baseline);
  g_test_add_data_func ("/bucket-native-popup/area-native-menu", "fill-area", bucket_popup_native_enum_menu);
  g_test_add_data_func ("/bucket-native-popup/criterion-native-menu", "fill-criterion", bucket_popup_native_enum_menu);
  g_test_add_func ("/bucket-native-popup/pattern-transaction", bucket_popup_pattern_transaction);
  g_test_add_func ("/bucket-native-popup/pending-stock-lifetime", bucket_popup_pending_lifetime);
  g_test_add_func ("/bucket-native-popup/factory-caller-release", bucket_popup_factory_caller_release);
}
