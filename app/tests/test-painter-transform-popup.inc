/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Actual independent Transform Details entry: no artificial carrier factory. */
#include "tools/tool_manager.h"
#include "tools/gimptransformgridtool.h"
#include "display/gimptoolgui.h"
#include "core/gimpimage-undo.h"
#include "core/gimpundostack.h"

typedef struct {
  GimpTool *tool;
  GimpToolWidget *widget;
  GimpToolGui *gui;
  GimpImage *image;
  gboolean dialog_visible;
  TransInfo infos[2];
  gint undo_depth;
} TransformPopupAction;

static TransformPopupAction
transform_popup_action_snapshot (GimpDisplayShell *shell)
{
  TransformPopupAction state = {0};
  state.tool = g_object_ref (tool_manager_get_active (gimp));
  g_assert_true (GIMP_IS_TRANSFORM_GRID_TOOL (state.tool));
  GimpTransformGridTool *grid = GIMP_TRANSFORM_GRID_TOOL (state.tool);
  state.widget = grid->widget ? g_object_ref (grid->widget) : NULL;
  state.gui = grid->gui ? g_object_ref (grid->gui) : NULL;
  state.dialog_visible = state.gui && gimp_tool_gui_get_visible (state.gui);
  state.image = gimp_display_get_image (shell->display);
  state.undo_depth = gimp_undo_stack_get_depth (gimp_image_get_undo_stack (state.image));
  memcpy (state.infos, grid->trans_infos, sizeof state.infos);
  return state;
}

static void
transform_popup_assert_action_unchanged (TransformPopupAction *state)
{
  g_assert_true (tool_manager_get_active (gimp) == state->tool);
  GimpTransformGridTool *grid = GIMP_TRANSFORM_GRID_TOOL (state->tool);
  g_assert_true (grid->widget == state->widget);
  g_assert_true (grid->gui == state->gui);
  g_assert_cmpint (grid->gui && gimp_tool_gui_get_visible (grid->gui), ==, state->dialog_visible);
  g_assert_cmpmem (state->infos, sizeof state->infos, grid->trans_infos, sizeof grid->trans_infos);
  g_assert_cmpint (gimp_undo_stack_get_depth (gimp_image_get_undo_stack (state->image)), ==, state->undo_depth);
}

static void
transform_popup_action_clear (TransformPopupAction *state)
{
  g_clear_object (&state->widget); g_clear_object (&state->gui); g_clear_object (&state->tool);
}
static void transform_popup_wait_stable_geometry (GtkWidget *widget);

static gboolean
transform_popup_require_x11_input (void)
{
#ifdef GDK_WINDOWING_X11
  if (GDK_IS_X11_DISPLAY (gdk_display_get_default ())) return TRUE;
#endif
  g_test_skip ("Actual XTest input case requires the X11 runtime backend");
  return FALSE;
}

#ifdef GDK_WINDOWING_X11
static void
transform_popup_physical_center (GtkWidget *widget, gint *x, gint *y)
{
  GtkWidget *top = gtk_widget_get_toplevel (widget);
  gint root_x, root_y, local_x, local_y;
  g_assert_true (gtk_widget_translate_coordinates (widget, top,
    gtk_widget_get_allocated_width (widget) / 2, gtk_widget_get_allocated_height (widget) / 2,
    &local_x, &local_y));
  gdk_window_get_origin (gtk_widget_get_window (top), &root_x, &root_y);
  /* GTK/GDK positions are logical pixels; XTest consumes physical pixels. */
  gint scale = gdk_window_get_scale_factor (gtk_widget_get_window (top));
  *x = (root_x + local_x) * scale; *y = (root_y + local_y) * scale;
}

static void
transform_popup_click_center (RectanglePopupInput *input, GtkWidget *widget)
{
  GdkDisplay *display = gtk_widget_get_display (widget);
  gpointer native = gdk_x11_display_get_xdisplay (display);
  gint x, y, updated_x, updated_y;
  transform_popup_physical_center (widget, &x, &y);
  input->motion (native, -1, x, y, 0);
  gdk_display_flush (display); rectangle_popup_settle ();
  /* Hover can reflow the toolbar's horizontal viewport. Stabilizing before
   * motion alone is insufficient: observe its actual painted geometry and
   * recompute the physical target before sending the button press. */
  transform_popup_wait_stable_geometry (widget);
  transform_popup_physical_center (widget, &updated_x, &updated_y);
  if (updated_x != x || updated_y != y) {
    x = updated_x; y = updated_y;
    input->motion (native, -1, x, y, 0);
    gdk_display_flush (display); rectangle_popup_settle ();
    transform_popup_wait_stable_geometry (widget);
    transform_popup_physical_center (widget, &updated_x, &updated_y);
    g_assert_cmpint (updated_x, ==, x); g_assert_cmpint (updated_y, ==, y);
  }
  input->button (native, 1, TRUE, 0);
  gdk_display_flush (display); rectangle_popup_settle ();
  input->button (native, 1, FALSE, 0);
  gdk_display_flush (display); rectangle_popup_settle ();
}

static RectanglePopupInput
transform_popup_input (void)
{
  RectanglePopupInput input = {0};
  input.module = g_module_open ("libXtst.so.6", G_MODULE_BIND_LAZY);
  g_assert_nonnull (input.module);
  g_assert_true (g_module_symbol (input.module, "XTestFakeMotionEvent", (gpointer *) &input.motion));
  g_assert_true (g_module_symbol (input.module, "XTestFakeButtonEvent", (gpointer *) &input.button));
  g_assert_true (g_module_symbol (input.module, "XTestFakeKeyEvent", (gpointer *) &input.key));
  return input;
}

static void
transform_popup_real_key (GtkWidget *host, guint key)
{
  RectanglePopupInput input = transform_popup_input ();
  rectangle_popup_key (&input, host, key); g_module_close (input.module); drain ();
}

static void
transform_popup_native_entry_keys (GtkWidget *host, GtkWidget *opacity, TransformPopupAction *action)
{
  RectanglePopupInput input = transform_popup_input ();
  transform_popup_click_center (&input, opacity);
  transform_popup_assert_action_unchanged (action);
  g_assert_true (gtk_window_get_focus (GTK_WINDOW (host)) == opacity);
  const guint keys[] = {GDK_KEY_Return, GDK_KEY_KP_Enter, GDK_KEY_space};
  for (guint i = 0; i < G_N_ELEMENTS (keys); ++i) {
    rectangle_popup_key (&input, host, keys[i]); g_assert_true (gtk_widget_get_mapped (host));
    transform_popup_assert_action_unchanged (action);
  }
  GdkDisplay *display = gtk_widget_get_display (host);
  gpointer native = gdk_x11_display_get_xdisplay (display);
  GdkKeymapKey *entries = NULL; gint count = 0;
  g_assert_true (gdk_keymap_get_entries_for_keyval (gdk_keymap_get_for_display (display),
    GDK_KEY_Control_L, &entries, &count)); g_assert_cmpint (count, >, 0);
  input.key (native, entries[0].keycode, TRUE, 0);
  rectangle_popup_key (&input, host, GDK_KEY_a);
  input.key (native, entries[0].keycode, FALSE, 0); g_free (entries);
  rectangle_popup_key (&input, host, GDK_KEY_7);
  rectangle_popup_key (&input, host, GDK_KEY_3);
  rectangle_popup_key (&input, host, GDK_KEY_Return);
  transform_popup_assert_action_unchanged (action);
  g_assert_true (gtk_widget_get_mapped (host));
  g_assert_cmpfloat_with_epsilon (gtk_spin_button_get_value (GTK_SPIN_BUTTON (opacity)), 73.0, 1e-6);
  /* A WM need not advertise override-redirect popups as its active toplevel.
   * Record X focus separately; actual control/model edits above prove delivery
   * through the production grab, without injecting a focus or seat grab. */
  GModule *x11 = g_module_open ("libX11.so.6", G_MODULE_BIND_LAZY);
  g_assert_nonnull (x11);
  gint (*get_focus) (gpointer, gulong *, gint *) = NULL;
  g_assert_true (g_module_symbol (x11, "XGetInputFocus", (gpointer *) &get_focus));
  gulong focused = 0; gint revert = 0; get_focus (native, &focused, &revert);
  g_test_message ("Actual key delivery: GTK-active=%d GTK-grab-popup=%d X-focus-popup=%d entry-value=73",
    gtk_window_is_active (GTK_WINDOW (host)), gtk_grab_get_current () == host,
    focused == gdk_x11_window_get_xid (gtk_widget_get_window (host)));
  g_module_close (x11); g_module_close (input.module);
}
#else
static void transform_popup_real_key (GtkWidget *host, guint key)
{ standard_toggle_event (host, key); }
static void transform_popup_native_entry_keys (GtkWidget *host, GtkWidget *opacity, TransformPopupAction *action)
{ (void) host; (void) opacity; (void) action; g_test_skip ("Native key input case requires X11/XTest"); }
#endif
typedef struct {
  GtkWidget *button;
  gint width, height, x, y, left, top, anchor_width, anchor_height;
  gboolean captured, mapped;
} TransformPopupPosition;

static gboolean
transform_popup_position_hook (GSignalInvocationHint *hint, guint count,
                               const GValue *values, gpointer data)
{
  TransformPopupPosition *record = data;
  (void) hint; (void) count;
  GtkWidget *widget = g_value_get_object (&values[0]);
  if (g_strcmp0 (gtk_widget_get_name (widget), "painter-transform-details-popup")) return TRUE;
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
transform_popup_canvas_button (GimpDisplayShell *shell, GimpToolOptions *options)
{
  GPtrArray *widgets = g_ptr_array_new (); gather (shell->canvas, widgets);
  GtkWidget *toolbar = NULL, *button = NULL;
  for (guint i = 0; i < widgets->len; ++i) {
    GtkWidget *widget = g_ptr_array_index (widgets, i);
    if (g_object_get_data (G_OBJECT (widget), "gimp-painter-toolbar-options") == options &&
        gtk_widget_get_ancestor (widget, GIMP_TYPE_DISPLAY_SHELL) == GTK_WIDGET (shell)) {
      g_assert_null (toolbar); toolbar = widget;
    }
  }
  g_ptr_array_unref (widgets); g_assert_nonnull (toolbar);
  /* The exact transform factory has one direct button; target/direction
   * controls live in their own child boxes. Do not search a hidden dock or
   * rely on a translated label to identify the actual Details invoker. */
  GList *children = gtk_container_get_children (GTK_CONTAINER (toolbar));
  for (GList *i = children; i; i = i->next)
    if (GTK_IS_BUTTON (i->data)) { g_assert_null (button); button = i->data; }
  g_list_free (children); g_assert_nonnull (button);
  return button;
}

typedef struct {
  GtkWidget *button;
  GtkAllocation allocation;
  gint left, top;
  guint frames, stable;
} TransformPopupLayout;

static void
transform_popup_after_paint (GdkFrameClock *clock, gpointer data)
{
  TransformPopupLayout *layout = data;
  GtkAllocation allocation; gtk_widget_get_allocation (layout->button, &allocation);
  gint left, top; gdk_window_get_origin (gtk_widget_get_window (layout->button), &left, &top);
  if (layout->frames && left == layout->left && top == layout->top &&
      allocation.x == layout->allocation.x && allocation.y == layout->allocation.y &&
      allocation.width == layout->allocation.width && allocation.height == layout->allocation.height)
    ++layout->stable;
  else layout->stable = 0;
  layout->allocation = allocation; layout->left = left; layout->top = top; ++layout->frames;
  if (layout->stable < 3) {
    gtk_widget_queue_draw (layout->button);
    gdk_frame_clock_request_phase (clock, GDK_FRAME_CLOCK_PHASE_AFTER_PAINT);
  }
}

static void
transform_popup_reveal_button (GtkWidget *button)
{
  GtkWidget *scroll = gtk_widget_get_ancestor (button, GTK_TYPE_SCROLLED_WINDOW);
  if (scroll) {
    GtkWidget *viewport = gtk_bin_get_child (GTK_BIN (scroll));
    GtkAdjustment *adjustment = gtk_scrolled_window_get_hadjustment (GTK_SCROLLED_WINDOW (scroll));
    gint x, y;
    g_assert_true (gtk_widget_translate_coordinates (button, viewport, 0, 0, &x, &y));
    gdouble value = gtk_adjustment_get_value (adjustment) + x - 20;
    gtk_adjustment_set_value (adjustment, CLAMP (value,
      gtk_adjustment_get_lower (adjustment), gtk_adjustment_get_upper (adjustment) - gtk_adjustment_get_page_size (adjustment)));
  }
  drain (); g_assert_true (gtk_widget_get_mapped (button));
  transform_popup_wait_stable_geometry (button);
}

static void
transform_popup_wait_stable_geometry (GtkWidget *button)
{
  /* A drained idle queue is not a completed GTK layout/paint frame. Require
   * three successive post-paint observations with unchanged real geometry
   * before a user-equivalent click; never compensate with pixel tolerance. */
  TransformPopupLayout layout = {0}; layout.button = button;
  GdkFrameClock *clock = gtk_widget_get_frame_clock (button); g_assert_nonnull (clock);
  gulong handler = g_signal_connect (clock, "after-paint", G_CALLBACK (transform_popup_after_paint), &layout);
  gdk_frame_clock_begin_updating (clock);
  gtk_widget_queue_draw (button);
  gdk_frame_clock_request_phase (clock, GDK_FRAME_CLOCK_PHASE_AFTER_PAINT);
  gint64 deadline = g_get_monotonic_time () + 3 * G_TIME_SPAN_SECOND;
  while (layout.stable < 3 && g_get_monotonic_time () < deadline) {
    g_main_context_iteration (NULL, FALSE); g_usleep (1000);
  }
  gdk_frame_clock_end_updating (clock); g_signal_handler_disconnect (clock, handler);
  g_assert_cmpuint (layout.stable, >=, 3);
  g_test_message ("Stable target frames=%u origin=%d/%d allocation=%d/%d %dx%d",
    layout.frames, layout.left, layout.top, layout.allocation.x, layout.allocation.y,
    layout.allocation.width, layout.allocation.height);
}

static void
transform_popup_assert_anchor (GtkWidget *popup, GtkWidget *button,
                               TransformPopupPosition *realize)
{
  GtkAllocation allocation; gtk_widget_get_allocation (button, &allocation);
  gint left, top; gdk_window_get_origin (gtk_widget_get_window (button), &left, &top);
  if (!gtk_widget_get_has_window (button)) { left += allocation.x; top += allocation.y; }
  GtkRequisition natural; gtk_widget_get_preferred_size (popup, NULL, &natural);
  GdkMonitor *monitor = gdk_display_get_monitor_at_point (gtk_widget_get_display (button), left, top);
  GdkRectangle rect; gdk_monitor_get_geometry (monitor, &rect);
  gint x = left, y = top + allocation.height, actual_x, actual_y;
  if (x + natural.width > rect.x + rect.width) x = left + allocation.width - natural.width;
  if (y + natural.height > rect.y + rect.height) y = top - natural.height;
  gtk_window_get_position (GTK_WINDOW (popup), &actual_x, &actual_y);
  g_assert_true (realize->captured); g_assert_false (realize->mapped);
  /* The source button must remain stationary across popup mapping, not merely
   * agree with an obsolete coordinate sampled during unfinished main layout. */
  g_assert_cmpint (realize->left, ==, left); g_assert_cmpint (realize->top, ==, top);
  g_assert_cmpint (realize->anchor_width, ==, allocation.width);
  g_assert_cmpint (realize->anchor_height, ==, allocation.height);
  g_test_message ("Native anchor %d/%d %dx%d: popup %dx%d at %d/%d",
    left, top, allocation.width, allocation.height, natural.width, natural.height, actual_x, actual_y);
  g_assert_cmpint (actual_x, ==, x); g_assert_cmpint (actual_y, ==, y);
}

static GtkWidget *
transform_popup_open_button (GtkWidget *button)
{
  transform_popup_reveal_button (button);
  TransformPopupPosition realize = {0}; realize.button = button;
  guint realize_signal = g_signal_lookup ("realize", GTK_TYPE_WIDGET);
  gulong realize_hook = g_signal_add_emission_hook (realize_signal, 0, transform_popup_position_hook, &realize, NULL);
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
  transform_popup_assert_anchor (actual, button, &realize);
  return actual;
}

static void
transform_popup_actual_entry_baseline (gconstpointer data)
{
  if (!transform_popup_require_x11_input ()) return;
  const gchar *tool_name = data;
  g_assert_true (gimp->config->painter_canvas_ui);
  gboolean single = FALSE;
  g_object_get (gimp->config, "single-window-mode", &single, NULL);
  g_assert_true (single);
  GimpDisplayShell *shell = new_shell ();
  GimpToolInfo *tool = info (tool_name);
  GimpToolOptions *options = tool->tool_options;
  GtkWidget *dock = gimp_tools_get_tool_options_gui (options);
  GtkWidget *dock_parent;
  GimpTransformGridOptions *grid = GIMP_TRANSFORM_GRID_OPTIONS (options);
  GtkWidget *type_box = GIMP_TRANSFORM_OPTIONS (grid)->type_box;
  GtkWidget *direction = GIMP_TRANSFORM_OPTIONS (grid)->direction_frame;
  GtkWidget *chain = grid->direction_chain_button;
  gdouble original_opacity;
  gboolean original_composited, original_preview;
  gint original_guides, original_grid_size;
  g_object_get (options, "preview-opacity", &original_opacity,
                "composited-preview", &original_composited, "show-preview", &original_preview,
                "grid-type", &original_guides, "grid-size", &original_grid_size, NULL);
  gimp_context_set_tool (gimp_get_user_context (gimp), tool); drain ();
  gimp_painter_canvas_ui_pointer (shell, 0, 0, TRUE); drain ();
  /* Modern tool_manager auto-activates transform tools on selection. Its
   * legitimate transform dialog may already be visible; Details input must
   * neither start another operation, commit it, cancel it nor recreate it. */
  TransformPopupAction action = transform_popup_action_snapshot (shell);
  dock_parent = gtk_widget_get_parent (dock);
  GtkWidget *button = transform_popup_canvas_button (shell, options);
  const gchar *capture = g_getenv ("GIMP_TRANSFORM_POPUP_CAPTURE");
  if (capture) {
    gtk_window_maximize (GTK_WINDOW (gtk_widget_get_toplevel (button)));
    gint64 until = g_get_monotonic_time () + G_TIME_SPAN_SECOND;
    while (g_get_monotonic_time () < until) { g_main_context_iteration (NULL, FALSE); g_usleep (1000); }
  }
  g_object_set (options, "composited-preview", FALSE, "show-preview", TRUE, NULL); drain ();
  GtkWidget *actual = transform_popup_open_button (button); g_object_ref (actual);
  transform_popup_assert_action_unchanged (&action);
  GtkWidget *opacity = find (actual, "preview-opacity", GTK_TYPE_SPIN_BUTTON);
  GtkWidget *size = find (actual, "grid-size", GTK_TYPE_SPIN_BUTTON);
  g_assert_nonnull (opacity); g_assert_nonnull (size);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (opacity), 81.0); drain ();
  gdouble value; g_object_get (options, "preview-opacity", &value, NULL);
  g_assert_cmpfloat_with_epsilon (value, .81, 1e-6);
  GtkWidget *dock_opacity = find (dock, "preview-opacity", GTK_TYPE_SPIN_BUTTON);
  g_assert_nonnull (dock_opacity);
  g_assert_cmpfloat_with_epsilon (gtk_spin_button_get_value (GTK_SPIN_BUTTON (dock_opacity)), 81.0, 1e-6);
  g_object_set (options, "grid-type", GIMP_GUIDES_N_LINES, "grid-size", 13, NULL); drain ();
  g_assert_true (gtk_widget_is_sensitive (size));
  g_assert_cmpint (gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (size)), ==, 13);
  g_object_set (options, "grid-type", GIMP_GUIDES_NONE, NULL); drain ();
  g_assert_false (gtk_widget_is_sensitive (size));
  transform_popup_native_entry_keys (actual, opacity, &action);
  g_object_get (options, "preview-opacity", &value, NULL);
  g_assert_cmpfloat_with_epsilon (value, .73, 1e-6);
  g_assert_cmpfloat_with_epsilon (gtk_spin_button_get_value (GTK_SPIN_BUTTON (dock_opacity)), 73.0, 1e-6);
  if (capture) {
    GdkWindow *root = gdk_screen_get_root_window (gtk_widget_get_screen (actual));
    GdkPixbuf *pixels = gdk_pixbuf_get_from_window (root, 0, 0,
      gdk_window_get_width (root), gdk_window_get_height (root));
    g_assert_nonnull (pixels); GError *error = NULL;
    g_assert_true (gdk_pixbuf_save (pixels, capture, "png", &error, NULL)); g_assert_no_error (error);
    g_object_unref (pixels);
    const gchar *pause = g_getenv ("GIMP_TRANSFORM_POPUP_PAUSE");
    if (pause) {
      gint64 until = g_get_monotonic_time () + g_ascii_strtoll (pause, NULL, 10) * G_TIME_SPAN_SECOND;
      while (g_get_monotonic_time () < until) { g_main_context_iteration (NULL, FALSE); g_usleep (1000); }
    }
  }
  transform_popup_real_key (actual, GDK_KEY_Escape);
  g_assert_false (gtk_widget_get_realized (actual));
  transform_popup_assert_action_unchanged (&action);
  GtkWidget *next = transform_popup_open_button (button); g_object_ref (next);
  g_assert_true (next != actual);
  g_assert_true (find (next, "preview-opacity", GTK_TYPE_SPIN_BUTTON) == opacity);
  g_assert_true (gtk_widget_is_ancestor (opacity, next));
  g_assert_cmpfloat_with_epsilon (gtk_spin_button_get_value (GTK_SPIN_BUTTON (opacity)), 73.0, 1e-6);
  g_object_set (options, "show-preview", FALSE, NULL); drain ();
  g_assert_false (gtk_widget_get_mapped (opacity));
  g_assert_false (standard_toggle_event (next, 0));
  g_assert_false (gtk_widget_get_realized (next));
  GtkWidget *last = transform_popup_open_button (button); g_object_ref (last);
  g_assert_false (gtk_widget_get_mapped (opacity));
  g_assert_true (gtk_widget_get_parent (dock) == dock_parent);
  transform_popup_assert_action_unchanged (&action); transform_popup_action_clear (&action);
  gimp_context_set_tool (gimp_get_user_context (gimp), info ("gimp-paintbrush-tool")); drain ();
  g_assert_false (gtk_widget_get_realized (last));
  g_assert_true (gimp_tools_get_tool_options_gui (options) == dock);
  g_assert_true (GIMP_TRANSFORM_OPTIONS (grid)->type_box == type_box);
  g_assert_true (GIMP_TRANSFORM_OPTIONS (grid)->direction_frame == direction);
  g_assert_true (grid->direction_chain_button == chain);
  g_object_set (options, "preview-opacity", original_opacity,
                "composited-preview", original_composited, "show-preview", original_preview,
                "grid-type", original_guides, "grid-size", original_grid_size, NULL);
  g_object_unref (actual); g_object_unref (next); g_object_unref (last);
  close_shell (shell);
}

#ifdef GDK_WINDOWING_X11
static void
transform_popup_wait_menu (GtkWidget *combo, GtkWidget *outer, gboolean shown)
{
  gboolean actual = FALSE;
  gint64 deadline = g_get_monotonic_time () + 3 * G_TIME_SPAN_SECOND;
  do {
    g_object_get (combo, "popup-shown", &actual, NULL);
    if (actual == shown && (shown ? GTK_IS_MENU (gtk_grab_get_current ()) :
        gtk_grab_get_current () == outer)) break;
    g_main_context_iteration (NULL, FALSE); g_usleep (1000);
  } while (g_get_monotonic_time () < deadline);
  g_assert_cmpint (actual, ==, shown);
  if (shown) {
    g_assert_true (GTK_IS_MENU (gtk_grab_get_current ()));
    g_assert_true (gtk_widget_get_mapped (gtk_grab_get_current ()));
  } else g_assert_true (gtk_grab_get_current () == outer);
}

static void
transform_popup_native_enum_menu (gconstpointer data)
{
  if (!transform_popup_require_x11_input ()) return;
  const gchar *property = data;
  RectanglePopupInput input = {0};
  input.module = g_module_open ("libXtst.so.6", G_MODULE_BIND_LAZY);
  g_assert_nonnull (input.module);
  g_assert_true (g_module_symbol (input.module, "XTestFakeMotionEvent", (gpointer *) &input.motion));
  g_assert_true (g_module_symbol (input.module, "XTestFakeButtonEvent", (gpointer *) &input.button));
  g_assert_true (g_module_symbol (input.module, "XTestFakeKeyEvent", (gpointer *) &input.key));
  GimpDisplayShell *shell = new_shell ();
  GimpToolInfo *tool = info ("gimp-rotate-tool");
  GimpToolOptions *options = tool->tool_options;
  gint original; g_object_get (options, property, &original, NULL);
  gimp_context_set_tool (gimp_get_user_context (gimp), tool); drain ();
  gimp_painter_canvas_ui_pointer (shell, 0, 0, TRUE); drain ();
  GtkWidget *button = transform_popup_canvas_button (shell, options);
  GtkWidget *outer = transform_popup_open_button (button); g_object_ref (outer);
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
  GtkWidget *next = transform_popup_open_button (button);
  g_assert_true (next != outer);
  g_assert_true (find (next, property, GTK_TYPE_COMBO_BOX) == combo);
  g_signal_emit_by_name (next, "cancel"); drain ();
  g_object_set (options, property, original, NULL);
  g_object_unref (menu); g_object_unref (combo); g_object_unref (outer);
  close_shell (shell); g_module_close (input.module);
}
#else
static void transform_popup_native_enum_menu (gconstpointer data)
{
  (void) data; g_test_skip ("Native menu input case requires X11/XTest");
}
#endif

static void
transform_popup_model_finalized (gpointer data, GObject *object)
{
  (void) object; *(gboolean *) data = TRUE;
}

static void
transform_popup_factory_caller_release (void)
{
  GimpToolInfo *tool = info ("gimp-rotate-tool");
  GimpToolOptions *options = g_object_new (G_OBJECT_TYPE (tool->tool_options),
                                          "gimp", gimp, "tool-info", tool, NULL);
  GimpToolOptionsGUIFunc factory = g_object_get_data (G_OBJECT (tool),
                                                     "gimp-tool-options-gui-horizontal-func");
  g_assert_nonnull (factory);
  /* A private lifetime model uses the exact registered function, not the
   * canonical-options-only dispatcher and never replaces shared tool state. */
  GtkWidget *gui = factory (options); g_assert_nonnull (gui); g_object_ref_sink (gui);
  gboolean finalized = FALSE;
  g_object_weak_ref (G_OBJECT (options), transform_popup_model_finalized, &finalized);
  GtkWidget *host = NULL;
  GtkWidget *outer = independent_transform_present (gui, &host); g_object_ref (outer);
  GtkWidget *button = find (gui, "painter-transform-details", GTK_TYPE_BUTTON);
  g_assert_nonnull (button); g_object_ref (button);
  g_object_unref (options); g_assert_false (finalized);
  GtkWidget *opacity = find (outer, "preview-opacity", GTK_TYPE_SPIN_BUTTON); g_assert_nonnull (opacity);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (opacity), 67.0); drain ();
  gdouble value; g_object_get (options, "preview-opacity", &value, NULL);
  g_assert_cmpfloat_with_epsilon (value, .67, 1e-6);
  gtk_widget_hide (host); drain ();
  g_assert_false (gtk_widget_get_realized (outer)); g_assert_false (finalized);
  gtk_widget_show_all (host); drain ();
  GtkWidget *next;
#ifdef GDK_WINDOWING_X11
  if (GDK_IS_X11_DISPLAY (gdk_display_get_default ()))
    next = transform_popup_open_button (button);
  else
#endif
    {
      /* Caller-release ownership is portable and needs no XTest/absolute
       * screen-coordinate oracle. Keep this case active on other backends. */
      gtk_button_clicked (GTK_BUTTON (button)); drain ();
      next = standard_toggle_visible (GTK_TYPE_WINDOW, "painter-transform-details-popup");
      g_assert_nonnull (next);
      g_assert_cmpint (gtk_window_get_window_type (GTK_WINDOW (next)), ==, GTK_WINDOW_POPUP);
      g_assert_true (gtk_widget_has_grab (next));
    }
  g_object_ref (next);
  g_assert_true (next != outer);
  g_assert_true (find (next, "preview-opacity", GTK_TYPE_SPIN_BUTTON) == opacity);
  g_assert_cmpfloat_with_epsilon (gtk_spin_button_get_value (GTK_SPIN_BUTTON (opacity)), 67.0, 1e-6);
  gtk_widget_destroy (gui); g_object_unref (gui);
  gtk_widget_destroy (host); g_object_unref (host);
  g_assert_false (gtk_widget_get_realized (next));
  g_object_unref (outer); g_object_unref (next); drain ();
  g_assert_true (finalized);
  gtk_button_clicked (GTK_BUTTON (button)); drain ();
  g_assert_true (finalized);
  g_assert_null (standard_toggle_visible (GTK_TYPE_WINDOW, "painter-transform-details-popup"));
  g_object_unref (button);
}

static void
register_transform_popup_tests (void)
{
  g_test_add_data_func ("/transform-native-popup/baseline-rotate", "gimp-rotate-tool", transform_popup_actual_entry_baseline);
  g_test_add_data_func ("/transform-native-popup/baseline-scale", "gimp-scale-tool", transform_popup_actual_entry_baseline);
  g_test_add_data_func ("/transform-native-popup/baseline-shear", "gimp-shear-tool", transform_popup_actual_entry_baseline);
  g_test_add_data_func ("/transform-native-popup/baseline-perspective", "gimp-perspective-tool", transform_popup_actual_entry_baseline);
  g_test_add_data_func ("/transform-native-popup/interpolation-native-menu", "interpolation", transform_popup_native_enum_menu);
  g_test_add_data_func ("/transform-native-popup/clipping-native-menu", "clip", transform_popup_native_enum_menu);
  g_test_add_data_func ("/transform-native-popup/guides-native-menu", "grid-type", transform_popup_native_enum_menu);
  g_test_add_func ("/transform-native-popup/factory-caller-release", transform_popup_factory_caller_release);
}
