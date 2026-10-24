/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Actual registered Ink entries, not standalone test-created carriers. */
#include <math.h>
#include "widgets/gimpblobeditor.h"
#include "paint/gimpinkoptions.h"
static GtkWidget *
ink_popup_canvas_button (GimpDisplayShell *shell, const gchar *id)
{
  GPtrArray *widgets = g_ptr_array_new (); gather (shell->canvas, widgets);
  gchar *name = g_strconcat ("painter-paint-", id, NULL);
  GtkWidget *button = NULL;
  for (guint i = 0; i < widgets->len; ++i) {
    GtkWidget *w = g_ptr_array_index (widgets, i);
    GtkWidget *row = gtk_widget_get_parent (w);
    GtkWidget *toolbar = row ? gtk_widget_get_parent (row) : NULL;
    if (!g_strcmp0 (gtk_widget_get_name (w), name) && toolbar &&
        !g_strcmp0 (gtk_widget_get_name (toolbar), "gimp-painter-paint-toolbar") &&
        gtk_widget_get_ancestor (w, GIMP_TYPE_DISPLAY_SHELL) == GTK_WIDGET (shell)) {
      g_assert_null (button); button = w;
    }
  }
  g_free (name); g_ptr_array_unref (widgets); g_assert_nonnull (button);
  return button;
}

static gboolean
ink_popup_position_hook (GSignalInvocationHint *hint, guint count,
                               const GValue *values, gpointer data)
{
  TransformPopupPosition *record = data;
  (void) hint; (void) count;
  GtkWidget *widget = g_value_get_object (&values[0]);
  if (g_strcmp0 (gtk_widget_get_name (widget), "painter-ink-sensitivity-popup") &&
      g_strcmp0 (gtk_widget_get_name (widget), "painter-ink-blob-popup")) return TRUE;
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
ink_popup_open_button (GtkWidget *button)
{
  transform_popup_reveal_button (button);
  TransformPopupPosition realize = {0}; realize.button = button;
  guint realize_signal = g_signal_lookup ("realize", GTK_TYPE_WIDGET);
  gulong realize_hook = g_signal_add_emission_hook (realize_signal, 0, ink_popup_position_hook, &realize, NULL);
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

typedef struct {
  GimpDisplayShell *shell;
  GimpToolOptions *options;
  GtkWidget *button, *dock, *dock_parent, *peer;
  GimpTool *tool;
  GimpImage *image;
  gint undo_depth, type;
  gdouble size, tilt, sensitivity[3], aspect, angle;
} InkPopupFixture;
static const gchar *ink_sensitivity_properties[] = { "size-sensitivity", "tilt-sensitivity", "vel-sensitivity" };

static InkPopupFixture
ink_popup_fixture (const gchar *id)
{
  InkPopupFixture f = {0};
  g_assert_true (gimp->config->painter_canvas_ui);
  gboolean single = FALSE; g_object_get (gimp->config, "single-window-mode", &single, NULL); g_assert_true (single);
  f.shell = new_shell (); GimpToolInfo *info_ink = info ("gimp-ink-tool"); f.options = info_ink->tool_options;
  gimp_context_set_tool (gimp_get_user_context (gimp), info_ink); drain ();
  gimp_painter_canvas_ui_pointer (f.shell, 0, 0, TRUE); drain ();
  f.button = ink_popup_canvas_button (f.shell, id);
  f.dock = gimp_tools_get_tool_options_gui (f.options); f.dock_parent = gtk_widget_get_parent (f.dock);
  f.tool = g_object_ref (tool_manager_get_active (gimp)); f.image = gimp_display_get_image (f.shell->display);
  f.undo_depth = gimp_undo_stack_get_depth (gimp_image_get_undo_stack (f.image));
  g_object_get (f.options, "size", &f.size, "tilt-angle", &f.tilt, "blob-type", &f.type,
                "blob-aspect", &f.aspect, "blob-angle", &f.angle, NULL);
  for (guint i = 0; i < 3; ++i) g_object_get (f.options, ink_sensitivity_properties[i], &f.sensitivity[i], NULL);
  if (g_getenv ("GIMP_INK_POPUP_CAPTURE")) {
    gtk_window_maximize (GTK_WINDOW (gtk_widget_get_toplevel (f.button)));
    gint64 until = g_get_monotonic_time () + G_TIME_SPAN_SECOND;
    while (g_get_monotonic_time () < until) { g_main_context_iteration (NULL, FALSE); g_usleep (1000); }
  }
  return f;
}

static void
ink_popup_assert_no_action (InkPopupFixture *f)
{
  g_assert_true (tool_manager_get_active (gimp) == f->tool);
  g_assert_cmpint (gimp_undo_stack_get_depth (gimp_image_get_undo_stack (f->image)), ==, f->undo_depth);
  gdouble size, tilt; g_object_get (f->options, "size", &size, "tilt-angle", &tilt, NULL);
  g_assert_cmpfloat_with_epsilon (size, f->size, 1e-6); g_assert_cmpfloat_with_epsilon (tilt, f->tilt, 1e-6);
  g_assert_true (gimp_tools_get_tool_options_gui (f->options) == f->dock);
  g_assert_true (gtk_widget_get_parent (f->dock) == f->dock_parent);
}

static GtkWidget *
ink_popup_peer_body (InkPopupFixture *f, const gchar *id)
{
  if (!f->peer) { f->peer = gimp_tools_create_painter_toolbar_gui (f->options); g_assert_nonnull (f->peer); g_object_ref_sink (f->peer); }
  gchar *name = g_strconcat ("painter-paint-", id, NULL);
  GtkWidget *button = find (f->peer, name, GTK_TYPE_BUTTON); g_free (name); g_assert_nonnull (button);
  GtkWidget *body = g_object_get_data (G_OBJECT (button), "gimp-painter-toolbar-popup");
  g_assert_nonnull (body); return body;
}

static void
ink_popup_finish (InkPopupFixture *f)
{
  if (f->peer) { gtk_widget_destroy (f->peer); g_object_unref (f->peer); }
  g_object_set (f->options, "size", f->size, "tilt-angle", f->tilt, "blob-type", f->type,
                "blob-aspect", f->aspect, "blob-angle", f->angle, NULL);
  for (guint i = 0; i < 3; ++i) g_object_set (f->options, ink_sensitivity_properties[i], f->sensitivity[i], NULL);
  g_object_unref (f->tool); if (f->shell) close_shell (f->shell);
}

static void
ink_popup_capture (GtkWidget *outer)
{
  gint x, y; gtk_window_get_position (GTK_WINDOW (outer), &x, &y);
  gint w = gtk_widget_get_allocated_width (outer), h = gtk_widget_get_allocated_height (outer);
  GdkRectangle bounds; GdkMonitor *monitor = gdk_display_get_monitor_at_window (gtk_widget_get_display (outer), gtk_widget_get_window (outer));
  gdk_monitor_get_geometry (monitor, &bounds);
  g_assert_cmpint (x, >=, bounds.x); g_assert_cmpint (y, >=, bounds.y);
  g_assert_cmpint (x+w, <=, bounds.x+bounds.width); g_assert_cmpint (y+h, <=, bounds.y+bounds.height);
  gint scale = gtk_widget_get_scale_factor (outer);
  g_test_message ("Ink native physical bounds=%d/%d %dx%d scale=%d", x*scale, y*scale, w*scale, h*scale, scale);
  const gchar *path = g_getenv ("GIMP_INK_POPUP_CAPTURE"); if (!path) return;
  GdkWindow *root = gdk_screen_get_root_window (gtk_widget_get_screen (outer));
  GdkPixbuf *pixels = gdk_pixbuf_get_from_window (root, 0, 0, gdk_window_get_width (root), gdk_window_get_height (root));
  g_assert_nonnull (pixels); GError *error = NULL;
  g_assert_true (gdk_pixbuf_save (pixels, path, "png", &error, NULL)); g_assert_no_error (error); g_object_unref (pixels);
  const gchar *pause = g_getenv ("GIMP_INK_POPUP_PAUSE");
  if (pause) { gint64 until = g_get_monotonic_time () + g_ascii_strtoll (pause, NULL, 10) * G_TIME_SPAN_SECOND;
    while (g_get_monotonic_time () < until) { g_main_context_iteration (NULL, FALSE); g_usleep (1000); } }
}

#ifdef GDK_WINDOWING_X11
static void
ink_popup_type_sensitivity (GtkWidget *outer, GtkWidget *control)
{
  RectanglePopupInput input = transform_popup_input (); transform_popup_click_center (&input, control);
  g_assert_true (gtk_window_get_focus (GTK_WINDOW (outer)) == control);
  const guint no_close[] = { GDK_KEY_Return, GDK_KEY_KP_Enter, GDK_KEY_space };
  for (guint i = 0; i < G_N_ELEMENTS (no_close); ++i) { rectangle_popup_key (&input, outer, no_close[i]); g_assert_true (gtk_widget_get_mapped (outer)); }
  GdkDisplay *display = gtk_widget_get_display (outer); gpointer native = gdk_x11_display_get_xdisplay (display);
  GdkKeymapKey *entries = NULL; gint count = 0;
  g_assert_true (gdk_keymap_get_entries_for_keyval (gdk_keymap_get_for_display (display), GDK_KEY_Control_L, &entries, &count));
  g_assert_cmpint (count, >, 0); input.key (native, entries[0].keycode, TRUE, 0); rectangle_popup_key (&input, outer, GDK_KEY_a);
  input.key (native, entries[0].keycode, FALSE, 0); g_free (entries);
  const guint text[] = {GDK_KEY_0, GDK_KEY_period, GDK_KEY_6, GDK_KEY_3, GDK_KEY_Return};
  for (guint i = 0; i < G_N_ELEMENTS (text); ++i) rectangle_popup_key (&input, outer, text[i]);
  g_assert_cmpfloat_with_epsilon (gtk_spin_button_get_value (GTK_SPIN_BUTTON (control)), .63, 1e-6);
  g_assert_true (gtk_widget_get_mapped (outer)); g_module_close (input.module);
}

static void
ink_popup_motion (RectanglePopupInput *input, GtkWidget *widget, gint x, gint y)
{
  GtkWidget *top = gtk_widget_get_toplevel (widget); gint local_x, local_y, root_x, root_y;
  g_assert_true (gtk_widget_translate_coordinates (widget, top, x, y, &local_x, &local_y));
  gdk_window_get_origin (gtk_widget_get_window (top), &root_x, &root_y);
  gint scale = gtk_widget_get_scale_factor (widget); GdkDisplay *display = gtk_widget_get_display (widget);
  input->motion (gdk_x11_display_get_xdisplay (display), -1, (root_x+local_x)*scale, (root_y+local_y)*scale, 0);
  gdk_display_flush (display); rectangle_popup_settle ();
}

static void
ink_popup_drag_begin (RectanglePopupInput *input, GtkWidget *widget)
{
  GimpBlobEditor *editor = GIMP_BLOB_EDITOR (widget);
  gint w = gtk_widget_get_allocated_width (widget), h = gtk_widget_get_allocated_height (widget), r = MIN (w,h)/2;
  gint x = w/2 + .85*r*editor->aspect/10.0*cos (editor->angle);
  gint y = h/2 + .85*r*editor->aspect/10.0*sin (editor->angle);
  ink_popup_motion (input, widget, x, y);
  g_assert_true (editor->in_handle); g_assert_false (editor->active);
  GdkDisplay *display = gtk_widget_get_display (widget);
  input->button (gdk_x11_display_get_xdisplay (display), 1, TRUE, 0); gdk_display_flush (display); rectangle_popup_settle ();
  g_assert_true (editor->active);
  ink_popup_motion (input, widget, w/2 + r/2, h/2 + r/3);
}

static void
ink_popup_release (RectanglePopupInput *input, GtkWidget *widget)
{
  GdkDisplay *display = gtk_widget_get_display (widget);
  input->button (gdk_x11_display_get_xdisplay (display), 1, FALSE, 0); gdk_display_flush (display); rectangle_popup_settle ();
}
#endif

static void
ink_popup_actual_entry (gconstpointer data)
{
  if (!transform_popup_require_x11_input ()) return;
#ifdef GDK_WINDOWING_X11
  const gchar *id = data; gboolean blob = !strcmp (id, "ink-blob");
  InkPopupFixture f = ink_popup_fixture (id);
  GtkWidget *outer = ink_popup_open_button (f.button); g_object_ref (outer);
  GtkWidget *peer = ink_popup_peer_body (&f, id);
  GtkWidget *retained;
  if (!blob) {
    for (guint i = 0; i < 3; ++i) {
      GtkWidget *control = find (outer, ink_sensitivity_properties[i], GTK_TYPE_SPIN_BUTTON);
      GtkWidget *peer_control = find (peer, ink_sensitivity_properties[i], GTK_TYPE_SPIN_BUTTON);
      GtkWidget *dock_control = find (f.dock, ink_sensitivity_properties[i], GTK_TYPE_SPIN_BUTTON);
      g_assert_nonnull (control); g_assert_nonnull (peer_control); g_assert_nonnull (dock_control);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (control), .21 + .1*i); drain ();
      gdouble value; g_object_get (f.options, ink_sensitivity_properties[i], &value, NULL);
      g_assert_cmpfloat_with_epsilon (value, .21+.1*i, 1e-6);
      g_assert_cmpfloat_with_epsilon (gtk_spin_button_get_value (GTK_SPIN_BUTTON (peer_control)), value, 1e-6);
      g_assert_cmpfloat_with_epsilon (gtk_spin_button_get_value (GTK_SPIN_BUTTON (dock_control)), value, 1e-6);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (peer_control), .48); drain ();
      g_assert_cmpfloat_with_epsilon (gtk_spin_button_get_value (GTK_SPIN_BUTTON (control)), .48, 1e-6);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (dock_control), .57); drain ();
      g_assert_cmpfloat_with_epsilon (gtk_spin_button_get_value (GTK_SPIN_BUTTON (control)), .57, 1e-6);
      g_object_set (f.options, ink_sensitivity_properties[i], .39, NULL); drain ();
      g_assert_cmpfloat_with_epsilon (gtk_spin_button_get_value (GTK_SPIN_BUTTON (control)), .39, 1e-6);
    }
    retained = find (outer, "size-sensitivity", GTK_TYPE_SPIN_BUTTON);
    ink_popup_type_sensitivity (outer, retained);
    gdouble value; g_object_get (f.options, "size-sensitivity", &value, NULL); g_assert_cmpfloat_with_epsilon (value, .63, 1e-6);
  } else {
    retained = find (outer, NULL, GIMP_TYPE_BLOB_EDITOR); g_assert_nonnull (retained);
    GtkWidget *peer_editor = find (peer, NULL, GIMP_TYPE_BLOB_EDITOR);
    GtkWidget *dock_editor = find (f.dock, NULL, GIMP_TYPE_BLOB_EDITOR);
    g_assert_nonnull (peer_editor); g_assert_nonnull (dock_editor);
    g_assert_true (retained != peer_editor && retained != dock_editor);
    g_object_set (f.options, "blob-aspect", 3.0, "blob-angle", .2, NULL); drain ();
    g_assert_cmpfloat_with_epsilon (GIMP_BLOB_EDITOR (retained)->aspect, 3, 1e-6);
    g_assert_cmpfloat_with_epsilon (GIMP_BLOB_EDITOR (retained)->angle, .2, 1e-6);
    RectanglePopupInput input = transform_popup_input (); ink_popup_drag_begin (&input, retained); ink_popup_release (&input, retained);
    g_assert_false (GIMP_BLOB_EDITOR (retained)->active);
    gint r = MIN (gtk_widget_get_allocated_width (retained), gtk_widget_get_allocated_height (retained))/2;
    gdouble angle, aspect; g_object_get (f.options, "blob-aspect", &aspect, "blob-angle", &angle, NULL);
    gdouble expected = CLAMP (10.0*sqrt ((gdouble)((r/2)*(r/2)+(r/3)*(r/3)))/(r*.85), 1.0, 10.0);
    g_assert_cmpfloat_with_epsilon (angle, atan2 (r/3, r/2), 1e-6);
    g_assert_cmpfloat_with_epsilon (aspect, expected, 1e-6);
    g_assert_cmpfloat_with_epsilon (GIMP_BLOB_EDITOR (peer_editor)->aspect, aspect, 1e-6);
    g_assert_cmpfloat_with_epsilon (GIMP_BLOB_EDITOR (dock_editor)->angle, angle, 1e-6);
    g_object_set (peer_editor, "blob-angle", .7, NULL); drain ();
    g_object_get (f.options, "blob-angle", &angle, NULL); g_assert_cmpfloat_with_epsilon (angle, .7, 1e-6);
    g_assert_cmpfloat_with_epsilon (GIMP_BLOB_EDITOR (retained)->angle, .7, 1e-6);
    g_assert_cmpfloat_with_epsilon (GIMP_BLOB_EDITOR (dock_editor)->angle, .7, 1e-6);
    g_object_set (dock_editor, "blob-aspect", 4.2, NULL); drain ();
    g_object_get (f.options, "blob-aspect", &aspect, NULL); g_assert_cmpfloat_with_epsilon (aspect, 4.2, 1e-6);
    g_assert_cmpfloat_with_epsilon (GIMP_BLOB_EDITOR (retained)->aspect, 4.2, 1e-6);
    g_assert_cmpfloat_with_epsilon (GIMP_BLOB_EDITOR (peer_editor)->aspect, 4.2, 1e-6);
    const guint keys[] = { GDK_KEY_Return, GDK_KEY_KP_Enter, GDK_KEY_space };
    for (guint i = 0; i < G_N_ELEMENTS (keys); ++i) {
      rectangle_popup_key (&input, outer, keys[i]); g_assert_true (gtk_widget_get_mapped (outer));
      ink_popup_assert_no_action (&f);
    }
    GPtrArray *widgets = g_ptr_array_new (); gather (outer, widgets); guint shapes = 0, shape_mask = 0;
    for (guint i = 0; i < widgets->len; ++i) {
      GtkWidget *w = g_ptr_array_index (widgets, i);
      if (GTK_IS_RADIO_BUTTON (w)) {
        gint expected_type = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (w), "gimp-item-data"));
        transform_popup_click_center (&input, w);
        gint type; g_object_get (f.options, "blob-type", &type, NULL);
        g_assert_cmpint (type, ==, expected_type); shape_mask |= 1u << type;
        g_assert_cmpint (GIMP_BLOB_EDITOR (retained)->type, ==, type);
        g_assert_cmpint (GIMP_BLOB_EDITOR (peer_editor)->type, ==, type);
        g_assert_cmpint (GIMP_BLOB_EDITOR (dock_editor)->type, ==, type); ++shapes;
      }
    }
    g_ptr_array_unref (widgets); g_assert_cmpuint (shapes, ==, 3); g_assert_cmpuint (shape_mask, ==, 7); g_module_close (input.module);
  }
  ink_popup_assert_no_action (&f); ink_popup_capture (outer);
  transform_popup_real_key (outer, GDK_KEY_Escape); g_assert_false (gtk_widget_get_realized (outer));
  GtkWidget *next = ink_popup_open_button (f.button); g_object_ref (next); g_assert_true (next != outer);
  g_assert_true (find (next, blob ? NULL : "size-sensitivity", blob ? GIMP_TYPE_BLOB_EDITOR : GTK_TYPE_SPIN_BUTTON) == retained);
  if (blob) {
    g_assert_cmpfloat_with_epsilon (GIMP_BLOB_EDITOR (retained)->aspect, 4.2, 1e-6);
    g_assert_cmpfloat_with_epsilon (GIMP_BLOB_EDITOR (retained)->angle, .7, 1e-6);
  } else g_assert_cmpfloat_with_epsilon (gtk_spin_button_get_value (GTK_SPIN_BUTTON (retained)), .63, 1e-6);
  ink_popup_assert_no_action (&f); g_assert_false (standard_toggle_event (next, 0));
  g_assert_false (gtk_widget_get_realized (next));
  GtkWidget *other_button = ink_popup_canvas_button (f.shell, blob ? "ink-sensitivity" : "ink-blob");
  GtkWidget *other = ink_popup_open_button (other_button); g_object_ref (other);
  g_signal_emit_by_name (other, "cancel"); drain ();
  g_object_unref (other); g_object_unref (next); g_object_unref (outer); ink_popup_finish (&f);
#endif
}

static void
ink_popup_interrupted_drag (gconstpointer data)
{
  if (!transform_popup_require_x11_input ()) return;
#ifdef GDK_WINDOWING_X11
  guint mode = GPOINTER_TO_UINT (data);
  InkPopupFixture f = ink_popup_fixture ("ink-blob");
  g_object_set (f.options, "blob-aspect", 3.0, "blob-angle", .2, NULL);
  GtkWidget *outer = ink_popup_open_button (f.button); g_object_ref (outer);
  GtkWidget *editor = find (outer, NULL, GIMP_TYPE_BLOB_EDITOR); g_assert_nonnull (editor); g_object_ref (editor);
  RectanglePopupInput input = transform_popup_input (); ink_popup_drag_begin (&input, editor);
  gdouble aspect, angle; g_object_get (f.options, "blob-aspect", &aspect, "blob-angle", &angle, NULL);
  if (mode == 0) transform_popup_real_key (outer, GDK_KEY_Escape);
  else if (mode == 1) { gimp_context_set_tool (gimp_get_user_context (gimp), info ("gimp-paintbrush-tool")); drain (); }
  else { close_shell (f.shell); f.shell = NULL; drain (); }
  g_assert_false (gtk_widget_get_realized (outer));
  GdkDisplay *display = gdk_display_get_default ();
  input.motion (gdk_x11_display_get_xdisplay (display), -1, 10, 10, 0); gdk_display_flush (display);
  ink_popup_release (&input, editor);
  if (!f.shell) f.shell = new_shell ();
  gimp_context_set_tool (gimp_get_user_context (gimp), info ("gimp-ink-tool")); drain ();
  gimp_painter_canvas_ui_pointer (f.shell, 0, 0, TRUE); drain ();
  f.button = ink_popup_canvas_button (f.shell, "ink-blob");
  GtkWidget *next = ink_popup_open_button (f.button); g_object_ref (next);
  GtkWidget *next_editor = find (next, NULL, GIMP_TYPE_BLOB_EDITOR); g_assert_nonnull (next_editor);
  gdouble before_aspect, before_angle; g_object_get (f.options, "blob-aspect", &before_aspect, "blob-angle", &before_angle, NULL);
  g_assert_cmpfloat_with_epsilon (before_aspect, aspect, 1e-6); g_assert_cmpfloat_with_epsilon (before_angle, angle, 1e-6);
  ink_popup_motion (&input, next_editor, gtk_widget_get_allocated_width (next_editor)/2+10, gtk_widget_get_allocated_height (next_editor)/2+10);
  gdouble after_aspect, after_angle; g_object_get (f.options, "blob-aspect", &after_aspect, "blob-angle", &after_angle, NULL);
  g_test_message ("Interrupted Ink mode%u: active=%d before=%g/%g after-motion-without-press=%g/%g", mode,
                  GIMP_BLOB_EDITOR (next_editor)->active, aspect, angle, after_aspect, after_angle);
  g_assert_cmpfloat_with_epsilon (after_aspect, aspect, 1e-6); g_assert_cmpfloat_with_epsilon (after_angle, angle, 1e-6);
  g_assert_false (GIMP_BLOB_EDITOR (next_editor)->active);
  g_signal_emit_by_name (next, "cancel"); drain ();
  g_object_unref (next); g_object_unref (outer); g_object_unref (editor); g_module_close (input.module); ink_popup_finish (&f);
#endif
}

static void
ink_popup_factory_caller_release (void)
{
  /* Heal Source remains unconverted; Clone now has its own native Options popup.
   * Preserve the unchanged non-Ink carrier control and all Ink lifetime checks. */
  GtkWidget *other_gui = gimp_tools_create_painter_toolbar_gui (info ("gimp-heal-tool")->tool_options);
  g_assert_nonnull (other_gui); g_object_ref_sink (other_gui);
  GtkWidget *source = find (other_gui, "painter-paint-source", GTK_TYPE_MENU_BUTTON); g_assert_nonnull (source);
  g_assert_true (GTK_IS_POPOVER (gtk_menu_button_get_popover (GTK_MENU_BUTTON (source))));
  gtk_widget_destroy (other_gui); g_object_unref (other_gui);
  GimpToolInfo *tool = info ("gimp-ink-tool");
  GimpToolOptions *options = g_object_new (G_OBJECT_TYPE (tool->tool_options), "gimp", gimp, "tool-info", tool, NULL);
  gimp_context_copy_properties (GIMP_CONTEXT (tool->tool_options), GIMP_CONTEXT (options), GIMP_CONTEXT_PROP_MASK_ALL);
  GimpToolOptionsGUIFunc factory = g_object_get_data (G_OBJECT (tool), "gimp-tool-options-gui-horizontal-func");
  g_assert_nonnull (factory); GtkWidget *gui = factory (options); g_assert_nonnull (gui);
  GtkWidget *host = gtk_window_new (GTK_WINDOW_TOPLEVEL); g_object_ref_sink (host);
  gtk_container_add (GTK_CONTAINER (host), gui); gtk_widget_show_all (host); drain ();
  GtkWidget *button = find (gui, "painter-paint-ink-blob", GTK_TYPE_BUTTON); g_assert_nonnull (button); g_object_ref (button);
  gboolean finalized = FALSE; g_object_weak_ref (G_OBJECT (options), transform_popup_model_finalized, &finalized);
  gtk_button_clicked (GTK_BUTTON (button)); drain ();
  GtkWidget *outer = standard_toggle_visible (GTK_TYPE_WINDOW, "painter-ink-blob-popup"); g_assert_nonnull (outer); g_object_ref (outer);
  g_assert_cmpint (gtk_window_get_window_type (GTK_WINDOW (outer)), ==, GTK_WINDOW_POPUP);
  g_object_unref (options); g_assert_false (finalized);
  GtkWidget *editor = find (outer, NULL, GIMP_TYPE_BLOB_EDITOR); g_assert_nonnull (editor);
  g_object_set (editor, "blob-aspect", 4.3, "blob-angle", .8, NULL); drain ();
  gdouble aspect; g_object_get (options, "blob-aspect", &aspect, NULL); g_assert_cmpfloat_with_epsilon (aspect, 4.3, 1e-6);
  gtk_widget_hide (host); drain (); g_assert_false (gtk_widget_get_realized (outer)); g_assert_false (finalized);
  gtk_widget_show_all (host); drain (); gtk_button_clicked (GTK_BUTTON (button)); drain ();
  GtkWidget *next = standard_toggle_visible (GTK_TYPE_WINDOW, "painter-ink-blob-popup"); g_assert_nonnull (next); g_object_ref (next);
  g_assert_true (next != outer); g_assert_true (find (next, NULL, GIMP_TYPE_BLOB_EDITOR) == editor);
  g_assert_cmpfloat_with_epsilon (GIMP_BLOB_EDITOR (editor)->aspect, 4.3, 1e-6);
  g_assert_cmpfloat_with_epsilon (GIMP_BLOB_EDITOR (editor)->angle, .8, 1e-6);
  gtk_widget_destroy (host); g_object_unref (host); drain ();
  g_assert_false (gtk_widget_get_realized (next)); g_object_unref (outer); g_object_unref (next); drain ();
  g_assert_true (finalized); gtk_button_clicked (GTK_BUTTON (button)); drain ();
  g_assert_null (standard_toggle_visible (GTK_TYPE_WINDOW, "painter-ink-blob-popup")); g_object_unref (button);
}

static void
register_ink_popup_tests (void)
{
  g_test_add_data_func ("/ink-native-popup/actual-sensitivity", "ink-sensitivity", ink_popup_actual_entry);
  g_test_add_data_func ("/ink-native-popup/actual-blob", "ink-blob", ink_popup_actual_entry);
  g_test_add_data_func ("/ink-native-popup/interrupted-escape", GUINT_TO_POINTER (0), ink_popup_interrupted_drag);
  g_test_add_data_func ("/ink-native-popup/interrupted-tool-switch", GUINT_TO_POINTER (1), ink_popup_interrupted_drag);
  g_test_add_data_func ("/ink-native-popup/interrupted-owner-close", GUINT_TO_POINTER (2), ink_popup_interrupted_drag);
  g_test_add_func ("/ink-native-popup/factory-caller-release", ink_popup_factory_caller_release);
}
