/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The actual horizontal Brush thumbnail, not the adjacent Brush Details or
 * generic compact compatibility editor. No Brush popup factory is invoked
 * directly; the stock-policy fixture opens its real dock through native UI. */
#include <glib/gstdio.h>
#include "core/gimp-gui.h"
#include "core/gimpbrush-private.h"
#include "core/gimptempbuf.h"
#include "paint/gimppaintoptions.h"
#include "widgets/gimpcontainericonview.h"
#include "widgets/gimpcontainerview.h"
#include "widgets/gimpdialogfactory.h"
#include "widgets/gimpdockable.h"
#include "widgets/gimptooloptionseditor.h"
#include "widgets/gimpwindowstrategy.h"

typedef struct {
  GimpBrush *brush;
  gchar *path, *bytes;
  gsize length;
} BrushPopupResource;

typedef struct {
  GimpDisplayShell *shell;
  GimpToolOptions *options;
  GimpToolInfo *previous_tool;
  GimpBrush *previous_brush;
  GtkWidget *button, *toolbar, *dock, *dock_parent;
  GtkWidget *stock_dockable, *stock_dockable_parent, *stock_editor;
  gboolean created_stock_dockable;
  GimpImage *image;
  gint undo_depth;
  gdouble size, aspect, angle, spacing, hardness;
  gboolean link_size, link_aspect, link_angle, link_spacing, link_hardness;
  GPtrArray *paused_editors;
  gchar *directory;
  BrushPopupResource a, b, readonly;
} BrushPopupFixture;

static void
brush_popup_finalized (gpointer data, GObject *object)
{
  (void) object; ++*(guint *) data;
}

static GtkWidget *
brush_popup_toolbar (GimpDisplayShell *shell, GimpToolOptions *options)
{
  GPtrArray *widgets = g_ptr_array_new ();
  GtkWidget *toolbar = NULL;
  gather (shell->canvas, widgets);
  for (guint i = 0; i < widgets->len; ++i) {
    GtkWidget *widget = g_ptr_array_index (widgets, i);
    if (!g_strcmp0 (gtk_widget_get_name (widget), "gimp-painter-paint-toolbar") &&
        g_object_get_data (G_OBJECT (widget), "gimp-painter-toolbar-options") == options &&
        gtk_widget_get_ancestor (widget, GIMP_TYPE_DISPLAY_SHELL) == GTK_WIDGET (shell)) {
      g_assert_null (toolbar); toolbar = widget;
    }
  }
  g_ptr_array_unref (widgets); g_assert_nonnull (toolbar); return toolbar;
}

static GtkWidget *
brush_popup_thumbnail (GtkWidget *toolbar)
{
  GtkWidget *button = NULL;
  GimpContainer *brushes = gimp_data_factory_get_container (gimp->brush_factory);
  GList *children = gtk_container_get_children (GTK_CONTAINER (toolbar));
  for (GList *i = children; i; i = i->next) {
    GtkWidget *widget = i->data;
    /* The fallback deliberately locates the exact pre-change stock thumbnail.
     * The final-test negative must fail on missing extended content, not on a
     * newly introduced name or unavailable production factory symbol. */
    if (!g_strcmp0 (gtk_widget_get_name (widget), "painter-paint-brush-thumbnail") ||
        (GIMP_IS_VIEWABLE_BUTTON (widget) && GIMP_VIEWABLE_BUTTON (widget)->container == brushes)) {
      g_assert_null (button); button = widget;
    }
  }
  g_list_free (children); g_assert_nonnull (button); return button;
}

static void
brush_popup_pause_dock_editors (BrushPopupFixture *f)
{
  /* Ordinary dock editors intentionally retain their own autosave policy.
   * Keep them on the original resource while these no-save fixtures are live. */
  f->paused_editors = g_ptr_array_new_with_free_func (g_object_unref);
  GPtrArray *widgets = g_ptr_array_new ();
  GList *windows = gtk_window_list_toplevels ();
  for (GList *i = windows; i; i = i->next) gather (i->data, widgets);
  g_list_free (windows);
  for (guint i = 0; i < widgets->len; ++i) {
    GtkWidget *widget = g_ptr_array_index (widgets, i);
    if (GIMP_IS_BRUSH_EDITOR (widget) &&
        gimp_data_editor_get_edit_active (GIMP_DATA_EDITOR (widget))) {
      g_ptr_array_add (f->paused_editors, g_object_ref (widget));
      gimp_data_editor_set_edit_active (GIMP_DATA_EDITOR (widget), FALSE);
    }
  }
  g_ptr_array_unref (widgets);
}

static BrushPopupResource
brush_popup_resource (BrushPopupFixture *f, const gchar *name, gdouble radius,
                      gboolean writable)
{
  BrushPopupResource r = {0}; GError *error = NULL;
  r.brush = GIMP_BRUSH (gimp_brush_generated_new (name, GIMP_BRUSH_GENERATED_CIRCLE,
                                                 radius, 2, 0.5, 1.0, 0.0));
  r.path = g_strdup_printf ("%s/%s.vbr", f->directory, name);
  GFile *file = g_file_new_for_path (r.path);
  gimp_data_set_file (GIMP_DATA (r.brush), file, TRUE, TRUE);
  g_assert_true (gimp_data_is_writable (GIMP_DATA (r.brush)));
  g_assert_true (gimp_data_save (GIMP_DATA (r.brush), &error)); g_assert_no_error (error);
  g_assert_false (gimp_data_is_dirty (GIMP_DATA (r.brush)));
  g_assert_true (g_file_get_contents (r.path, &r.bytes, &r.length, &error)); g_assert_no_error (error);
  if (!writable) gimp_data_set_file (GIMP_DATA (r.brush), file, FALSE, FALSE);
  g_object_unref (file);
  g_assert_cmpint (gimp_data_is_writable (GIMP_DATA (r.brush)), ==, writable);
  g_assert_true (gimp_container_add (gimp_data_factory_get_container (gimp->brush_factory),
                                   GIMP_OBJECT (r.brush)));
  return r;
}

static GtkWidget *brush_popup_dock_stock_button (GtkWidget *dock);

static void
brush_popup_prepare_stock_dock (BrushPopupFixture *f, GdkMonitor *monitor)
{
  GimpDialogFactory *factory = gimp_dialog_factory_get_singleton ();
  f->created_stock_dockable = !gimp_dialog_factory_find_widget (factory, "gimp-tool-options");
  /* The options getter intentionally creates a GUI even with no open dock.
   * Exercise the real Open command, including native book transfer/selection,
   * before taking the parent and resource snapshots used by this case. */
  f->stock_dockable = gimp_window_strategy_show_dockable_dialog
    (GIMP_WINDOW_STRATEGY (gimp_get_window_strategy (gimp)), gimp, factory,
     monitor, "gimp-tool-options");
  g_assert_true (GIMP_IS_DOCKABLE (f->stock_dockable));
  g_object_ref (f->stock_dockable);
  f->stock_editor = gtk_bin_get_child (GTK_BIN (f->stock_dockable));
  g_assert_true (GIMP_IS_TOOL_OPTIONS_EDITOR (f->stock_editor));
  GtkWidget *dock = gimp_tools_get_tool_options_gui (f->options);
  GtkWidget *button = g_object_ref (brush_popup_dock_stock_button (dock));
  gint64 deadline = g_get_monotonic_time () + 3 * G_TIME_SPAN_SECOND;
  while ((!gtk_widget_get_mapped (button) || !gtk_widget_get_window (button) ||
          !gdk_window_is_viewable (gtk_widget_get_window (button))) &&
         g_get_monotonic_time () < deadline) {
    g_main_context_iteration (NULL, FALSE); g_usleep (1000);
  }
  guint depth = 0;
  for (GtkWidget *w = button; w && depth < 16; w = gtk_widget_get_parent (w), ++depth)
    g_test_message ("Stock Brush ancestry[%u] type=%s name=%s visible=%d mapped=%d sensitive=%d",
                    depth, G_OBJECT_TYPE_NAME (w), gtk_widget_get_name (w),
                    gtk_widget_get_visible (w), gtk_widget_get_mapped (w), gtk_widget_is_sensitive (w));
  g_assert_nonnull (gimp_dockable_get_dockbook (GIMP_DOCKABLE (f->stock_dockable)));
  g_assert_true (gimp_tool_options_editor_get_tool_options
                   (GIMP_TOOL_OPTIONS_EDITOR (f->stock_editor)) == f->options);
  g_assert_true (gtk_widget_get_ancestor (dock, GIMP_TYPE_TOOL_OPTIONS_EDITOR) == f->stock_editor);
  g_assert_true (gtk_widget_is_ancestor (dock, f->stock_dockable));
  g_assert_true (gtk_widget_get_mapped (button));
  g_assert_true (gtk_widget_is_sensitive (button));
  g_assert_nonnull (gtk_widget_get_window (button));
  g_assert_true (gdk_window_is_viewable (gtk_widget_get_window (button)));
  f->stock_dockable_parent = gtk_widget_get_parent (f->stock_dockable);
  g_assert_nonnull (f->stock_dockable_parent);
  g_object_unref (button);
}

static BrushPopupFixture
brush_popup_fixture_with_stock_dock (gboolean stock_dock)
{
  BrushPopupFixture f = {0}; GError *error = NULL;
  gboolean single = FALSE;
  g_object_get (gimp->config, "single-window-mode", &single, NULL);
  g_assert_true (single); g_assert_true (gimp->config->painter_canvas_ui);
  f.shell = new_shell ();
  GtkWidget *top = gtk_widget_get_toplevel (GTK_WIDGET (f.shell));
  GdkMonitor *monitor = gdk_display_get_monitor_at_window (gtk_widget_get_display (top),
                                                         gtk_widget_get_window (top));
  GdkRectangle bounds;
  g_assert_nonnull (monitor); gdk_monitor_get_geometry (monitor, &bounds);
  /* Maximize needs a WM. This ordinary fixture placement also works in bare
   * Xvfb, without moving the thumbnail or modifying production panel layout. */
  gtk_window_resize (GTK_WINDOW (top), MAX (1, bounds.width - 40), MAX (1, bounds.height - 80));
  gtk_window_move (GTK_WINDOW (top), bounds.x, bounds.y); rectangle_popup_settle ();
  GimpContext *user = gimp_get_user_context (gimp);
  f.previous_tool = gimp_context_get_tool (user);
  if (f.previous_tool) g_object_ref (f.previous_tool);
  GimpToolInfo *tool = info ("gimp-paintbrush-tool"); f.options = tool->tool_options;
  gimp_context_set_tool (user, tool); drain ();
  if (stock_dock) brush_popup_prepare_stock_dock (&f, monitor);
  f.previous_brush = gimp_context_get_brush (GIMP_CONTEXT (f.options));
  if (f.previous_brush) g_object_ref (f.previous_brush);
  g_object_get (f.options, "brush-size", &f.size, "brush-aspect-ratio", &f.aspect,
                "brush-angle", &f.angle, "brush-spacing", &f.spacing,
                "brush-hardness", &f.hardness, "brush-link-size", &f.link_size,
                "brush-link-aspect-ratio", &f.link_aspect, "brush-link-angle", &f.link_angle,
                "brush-link-spacing", &f.link_spacing, "brush-link-hardness", &f.link_hardness, NULL);
  f.dock = gimp_tools_get_tool_options_gui (f.options); f.dock_parent = gtk_widget_get_parent (f.dock);
  brush_popup_pause_dock_editors (&f);
  f.directory = g_dir_make_tmp ("painter-brush-popup-XXXXXX", &error); g_assert_no_error (error);
  f.a = brush_popup_resource (&f, "00 Popup A", 11, TRUE);
  f.b = brush_popup_resource (&f, "01 Popup B", 19, TRUE);
  f.readonly = brush_popup_resource (&f, "02 Popup Read Only", 7, FALSE);
  g_object_set (f.options, "brush-link-size", FALSE, "brush-link-aspect-ratio", FALSE,
                "brush-link-angle", FALSE, "brush-link-spacing", FALSE,
                "brush-link-hardness", FALSE, NULL);
  gimp_context_set_brush (GIMP_CONTEXT (f.options), f.a.brush);
  gimp_painter_canvas_ui_pointer (f.shell, 0, 0, TRUE); drain ();
  f.toolbar = brush_popup_toolbar (f.shell, f.options); f.button = brush_popup_thumbnail (f.toolbar);
  f.image = gimp_display_get_image (f.shell->display);
  f.undo_depth = gimp_undo_stack_get_depth (gimp_image_get_undo_stack (f.image));
  return f;
}

static BrushPopupFixture
brush_popup_fixture (void)
{
  return brush_popup_fixture_with_stock_dock (FALSE);
}

static void
brush_popup_bytes_unchanged (BrushPopupResource *r)
{
  gchar *bytes = NULL; gsize length = 0; GError *error = NULL;
  g_assert_true (g_file_get_contents (r->path, &bytes, &length, &error)); g_assert_no_error (error);
  g_assert_cmpmem (bytes, length, r->bytes, r->length); g_free (bytes);
}

static void
brush_popup_no_action (BrushPopupFixture *f)
{
  g_assert_cmpint (gimp_undo_stack_get_depth (gimp_image_get_undo_stack (f->image)), ==, f->undo_depth);
  g_assert_true (gimp_tools_get_tool_options_gui (f->options) == f->dock);
  g_assert_true (gtk_widget_get_parent (f->dock) == f->dock_parent);
  if (f->stock_dockable) {
    g_assert_true (gtk_widget_get_parent (f->stock_dockable) == f->stock_dockable_parent);
    g_assert_true (gtk_widget_get_ancestor (f->dock, GIMP_TYPE_TOOL_OPTIONS_EDITOR) == f->stock_editor);
    g_assert_true (gtk_widget_is_ancestor (f->dock, f->stock_dockable));
    g_assert_true (gimp_tool_options_editor_get_tool_options
                     (GIMP_TOOL_OPTIONS_EDITOR (f->stock_editor)) == f->options);
  }
}

static void
brush_popup_resource_clear (BrushPopupResource *r)
{
  g_assert_true (gimp_container_remove (gimp_data_factory_get_container (gimp->brush_factory),
                                      GIMP_OBJECT (r->brush)));
  g_clear_object (&r->brush);
  g_assert_cmpint (g_unlink (r->path), ==, 0); g_free (r->path); g_free (r->bytes);
}

static void
brush_popup_finish (BrushPopupFixture *f)
{
  GtkWidget *host = standard_toggle_visible (GTK_TYPE_WINDOW, "painter-brush-thumbnail-popup");
  if (host) gtk_widget_destroy (host);
  gimp_context_set_brush (GIMP_CONTEXT (f->options), f->previous_brush);
  g_object_set (f->options, "brush-link-size", f->link_size, "brush-link-aspect-ratio", f->link_aspect,
                "brush-link-angle", f->link_angle, "brush-link-spacing", f->link_spacing,
                "brush-link-hardness", f->link_hardness, "brush-size", f->size,
                "brush-aspect-ratio", f->aspect, "brush-angle", f->angle,
                "brush-spacing", f->spacing, "brush-hardness", f->hardness, NULL);
  for (guint i = 0; i < f->paused_editors->len; ++i)
    gimp_data_editor_set_edit_active (g_ptr_array_index (f->paused_editors, i), TRUE);
  g_ptr_array_unref (f->paused_editors);
  brush_popup_resource_clear (&f->a); brush_popup_resource_clear (&f->b);
  brush_popup_resource_clear (&f->readonly);
  g_assert_cmpint (g_rmdir (f->directory), ==, 0); g_free (f->directory);
  if (f->previous_tool) gimp_context_set_tool (gimp_get_user_context (gimp), f->previous_tool);
  g_clear_object (&f->previous_tool); g_clear_object (&f->previous_brush);
  if (f->stock_dockable) {
    if (f->created_stock_dockable) gtk_widget_destroy (f->stock_dockable);
    g_clear_object (&f->stock_dockable);
  }
  close_shell (f->shell);
}

static gboolean
brush_popup_position_hook (GSignalInvocationHint *hint, guint count,
                           const GValue *values, gpointer data)
{
  TransformPopupPosition *record = data; (void) hint; (void) count;
  GtkWidget *widget = g_value_get_object (&values[0]);
  if (g_strcmp0 (gtk_widget_get_name (widget), "painter-brush-thumbnail-popup")) return TRUE;
  GtkRequisition natural; gtk_widget_get_preferred_size (widget, NULL, &natural);
  GtkAllocation allocation; gtk_widget_get_allocation (record->button, &allocation);
  gdk_window_get_origin (gtk_widget_get_window (record->button), &record->left, &record->top);
  if (!gtk_widget_get_has_window (record->button)) {
    record->left += allocation.x; record->top += allocation.y;
  }
  record->anchor_width = allocation.width; record->anchor_height = allocation.height;
  record->width = natural.width; record->height = natural.height;
  gtk_window_get_position (GTK_WINDOW (widget), &record->x, &record->y);
  record->mapped = gtk_widget_get_mapped (widget); record->captured = TRUE; return TRUE;
}

static gboolean
brush_popup_is_tooltip (GtkWidget *widget)
{
  /* Match accepted common-popup coverage: only GTK's own tooltip carrier
   * with its tooltip hint is incidental. Menus, dialogs, application popup
   * hosts and the stock baseline GimpContainerPopup must still count. */
  return !g_strcmp0 (G_OBJECT_TYPE_NAME (widget), "GtkTooltipWindow") &&
    gtk_window_get_type_hint (GTK_WINDOW (widget)) == GDK_WINDOW_TYPE_HINT_TOOLTIP;
}

static GtkWidget *
brush_popup_new_visible (GList *before)
{
  GtkWidget *host = NULL; GList *windows = gtk_window_list_toplevels ();
  guint candidates = 0;
  /* Diagnose the full candidate set before the strict second-pass assertion.
   * No event dispatch, hiding or activation occurs during either pass. */
  for (GList *i = windows; i; i = i->next)
    if (!g_list_find (before, i->data) && GTK_IS_WINDOW (i->data) && gtk_widget_get_visible (i->data)) {
      GtkWidget *widget = i->data;
      if (++candidates <= 32)
        g_test_message ("Brush new visible toplevel=%p type=%s name=%s hint=%d kind=%d mapped=%d tooltip-excluded=%d",
                        (void *) widget, G_OBJECT_TYPE_NAME (widget), gtk_widget_get_name (widget),
                        gtk_window_get_type_hint (GTK_WINDOW (widget)),
                        gtk_window_get_window_type (GTK_WINDOW (widget)),
                        gtk_widget_get_mapped (widget), brush_popup_is_tooltip (widget));
    }
  if (candidates > 32) g_test_message ("Brush new visible toplevel diagnostics omitted=%u", candidates - 32);
  for (GList *i = windows; i; i = i->next)
    if (!g_list_find (before, i->data) && GTK_IS_WINDOW (i->data) && gtk_widget_get_visible (i->data) &&
        !brush_popup_is_tooltip (i->data)) {
      g_assert_null (host); host = i->data;
    }
  g_list_free (windows); return host;
}

static GtkWidget *
brush_popup_shape_radio (GtkWidget *host, GimpBrushGeneratedShape shape)
{
  GtkWidget *box = find (host, "painter-brush-resource-shape", GTK_TYPE_BOX);
  g_assert_nonnull (box);
  GList *children = gtk_container_get_children (GTK_CONTAINER (box));
  g_assert_cmpuint (g_list_length (children), ==, 3);
  GtkWidget *result = NULL;
  GEnumClass *klass = g_type_class_ref (GIMP_TYPE_BRUSH_GENERATED_SHAPE);
  for (GList *i = children; i; i = i->next) {
    GtkWidget *radio = i->data;
    g_assert_true (GTK_IS_RADIO_BUTTON (radio));
    gint value = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (radio), "gimp-item-data"));
    GEnumValue *entry = g_enum_get_value (klass, value); g_assert_nonnull (entry);
    GtkWidget *image = gtk_bin_get_child (GTK_BIN (radio)); g_assert_true (GTK_IS_IMAGE (image));
    const gchar *icon = NULL; gtk_image_get_icon_name (GTK_IMAGE (image), &icon, NULL);
    gchar *expected = g_strconcat ("gimp-shape-", entry->value_nick, NULL);
    g_assert_cmpstr (icon, ==, expected); g_free (expected);
    g_assert_cmpint (gtk_button_get_relief (GTK_BUTTON (radio)), ==, GTK_RELIEF_NONE);
    g_assert_false (gtk_toggle_button_get_mode (GTK_TOGGLE_BUTTON (radio)));
    const gchar *accessible = atk_object_get_name (gtk_widget_get_accessible (radio));
    g_assert_nonnull (accessible); g_assert_cmpuint (strlen (accessible), >, 0);
    if (value == shape) { g_assert_null (result); result = radio; }
  }
  g_type_class_unref (klass); g_list_free (children); g_assert_nonnull (result); return result;
}

static GtkWidget *
brush_popup_title (GtkWidget *host, GimpBrush *brush)
{
  GtkWidget *selector = find (host, "painter-brush-popup-selector", GIMP_TYPE_CONTAINER_ICON_VIEW);
  GtkWidget *title = NULL;
  GPtrArray *widgets = g_ptr_array_new ();
  gchar *expected = brush ? gimp_viewable_get_description (GIMP_VIEWABLE (brush), NULL) :
                           g_strdup (g_dgettext (GETTEXT_PACKAGE, "(None)"));
  gboolean shown = FALSE;
  g_assert_nonnull (selector);
  g_object_get (selector, "show-name", &shown, NULL); g_assert_true (shown);
  gather (selector, widgets);
  for (guint i = 0; i < widgets->len; ++i) {
    GtkWidget *widget = g_ptr_array_index (widgets, i);
    if (GTK_IS_LABEL (widget) && !g_strcmp0 (gtk_label_get_text (GTK_LABEL (widget)), expected)) {
      g_assert_null (title); title = widget;
    }
  }
  g_ptr_array_unref (widgets); g_free (expected);
  g_assert_nonnull (title); g_assert_true (gtk_widget_get_visible (title));
  return title;
}

static GtkWidget *
brush_popup_assert_body (GtkWidget *host, GimpToolOptions *options)
{
  g_assert_nonnull (host); g_assert_true (GTK_IS_WINDOW (host));
  g_assert_cmpint (gtk_window_get_window_type (GTK_WINDOW (host)), ==, GTK_WINDOW_POPUP);
  /* Baseline stock selectors already satisfy GTK_WINDOW_POPUP. The actual
   * content and live canonical context make this a meaningful negative. */
  GtkWidget *body = find (host, "painter-brush-popup-body", 0);
  GtkWidget *selector = find (host, "painter-brush-popup-selector", GIMP_TYPE_CONTAINER_ICON_VIEW);
  GtkWidget *preview = find (host, "painter-brush-popup-preview", GIMP_TYPE_VIEW);
  g_test_message ("Actual Brush thumbnail carrier=%s extended-body=%d selector=%d preview=%d",
                  G_OBJECT_TYPE_NAME (host), body != NULL, selector != NULL, preview != NULL);
  g_assert_nonnull (body); g_assert_nonnull (selector); g_assert_nonnull (preview);
  brush_popup_title (host, gimp_context_get_brush (GIMP_CONTEXT (options)));
  g_assert_false (GIMP_IS_CONTAINER_POPUP (host));
  g_assert_true (gimp_container_view_get_context (GIMP_CONTAINER_VIEW (selector)) == GIMP_CONTEXT (options));
  g_assert_true (gimp_container_view_get_container (GIMP_CONTAINER_VIEW (selector)) ==
                 gimp_data_factory_get_container (gimp->brush_factory));
  g_assert_true (GIMP_VIEW (preview)->renderer->viewable ==
                 GIMP_VIEWABLE (gimp_context_get_brush (GIMP_CONTEXT (options))));
  g_assert_cmpint (GIMP_VIEW (preview)->renderer->width, ==, 256);
  g_assert_cmpint (GIMP_VIEW (preview)->renderer->height, ==, 256);
  const gchar *tools[] = { "brush-size", "brush-aspect-ratio", "brush-angle" };
  for (guint i = 0; i < G_N_ELEMENTS (tools); ++i)
    g_assert_nonnull (find (host, tools[i], GTK_TYPE_SPIN_BUTTON));
  const gchar *resources[] = { "radius", "spikes", "hardness", "aspect-ratio", "angle", "spacing" };
  for (guint i = 0; i < G_N_ELEMENTS (resources); ++i) {
    gchar *name = g_strconcat ("painter-brush-resource-", resources[i], NULL);
    g_assert_nonnull (find (host, name, GTK_TYPE_SPIN_BUTTON)); g_free (name);
  }
  g_assert_nonnull (brush_popup_shape_radio (host, GIMP_BRUSH_GENERATED_CIRCLE));
  g_assert_null (find (host, NULL, GIMP_TYPE_DATA_EDITOR));
  g_assert_true (gtk_widget_has_grab (host)); return body;
}

static GtkWidget *
brush_popup_open (BrushPopupFixture *f, gboolean physical)
{
  /* Kept in the Brush oracle: the accepted-base RED imports only this include,
   * not any concurrent improvements to the prior Transform fixture. */
  transform_popup_wait_stable_geometry (f->button);
  transform_popup_reveal_button (f->button);
  TransformPopupPosition record = {0}; record.button = f->button;
  guint signal = g_signal_lookup ("realize", GTK_TYPE_WIDGET);
  gulong hook = g_signal_add_emission_hook (signal, 0, brush_popup_position_hook, &record, NULL);
  GList *before = gtk_window_list_toplevels (); GtkWidget *host = NULL;
#ifdef GDK_WINDOWING_X11
  if (physical) {
    RectanglePopupInput input = transform_popup_input ();
    GdkDisplay *display = gtk_widget_get_display (f->button);
    gpointer native = gdk_x11_display_get_xdisplay (display); gint x, y;
    transform_popup_physical_center (f->button, &x, &y);
    input.motion (native, -1, x, y, 0); gdk_display_flush (display); rectangle_popup_settle ();
    transform_popup_wait_stable_geometry (f->button);
    transform_popup_physical_center (f->button, &x, &y);
    g_assert_true (input.motion (native, -1, x, y, 0)); gdk_display_sync (display);
    GdkDevice *pointer = gdk_seat_get_pointer (gdk_display_get_default_seat (display));
    GdkWindow *root = gdk_screen_get_root_window (gtk_widget_get_screen (f->button));
    gint scale = gtk_widget_get_scale_factor (f->button), px, py, wx, wy;
    gpointer hit_owner = NULL;
    gdk_device_get_position (pointer, NULL, &px, &py);
    GdkWindow *hit = gdk_device_get_window_at_position (pointer, &wx, &wy);
    if (hit) gdk_window_get_user_data (hit, &hit_owner);
    g_test_message ("Brush native pointer expected=%d/%d actual=%d/%d hit=%s:%s",
                    x / scale, y / scale, px, py,
                    GTK_IS_WIDGET (hit_owner) ? G_OBJECT_TYPE_NAME (hit_owner) : "none",
                    GTK_IS_WIDGET (hit_owner) ? gtk_widget_get_name (hit_owner) : "none");
    g_assert_cmpint (x, >=, 0); g_assert_cmpint (y, >=, 0);
    g_assert_cmpint (x / scale, <, gdk_window_get_width (root));
    g_assert_cmpint (y / scale, <, gdk_window_get_height (root));
    g_assert_nonnull (hit); g_assert_true (gdk_window_is_viewable (hit));
    g_assert_true (GTK_IS_WIDGET (hit_owner));
    g_assert_true (hit_owner == f->button || gtk_widget_is_ancestor (GTK_WIDGET (hit_owner), f->button));
    g_assert_cmpint (px, ==, x / scale); g_assert_cmpint (py, ==, y / scale);
    input.button (native, 1, TRUE, 0);
    gdk_display_flush (display); rectangle_popup_settle (); drain ();
    /* Record the press boundary before release. Check content first after
     * release so the old stock baseline fails for its missing extended body,
     * not merely its clicked-only phase; then require the same host already
     * existed on press, catching a clicked-only extended-editor port too. */
    GtkWidget *pressed = brush_popup_new_visible (before);
    if (pressed) g_object_ref (pressed);
    input.button (native, 1, FALSE, 0); gdk_display_flush (display); rectangle_popup_settle (); drain ();
    host = brush_popup_new_visible (before); brush_popup_assert_body (host, f->options);
    g_assert_nonnull (pressed); g_assert_true (pressed == host);
    g_assert_true (gtk_widget_get_mapped (host));
    g_assert_true (standard_toggle_visible (GTK_TYPE_WINDOW, "painter-brush-thumbnail-popup") == host);
    g_object_unref (pressed); g_module_close (input.module);
  } else
#else
  (void) physical;
#endif
  {
    gtk_button_clicked (GTK_BUTTON (f->button)); drain (); host = brush_popup_new_visible (before);
  }
  g_signal_remove_emission_hook (signal, hook); g_list_free (before);
  brush_popup_assert_body (host, f->options);
  g_assert_true (record.button == f->button);
  g_assert_true (record.captured); g_assert_false (record.mapped);
  GdkMonitor *monitor = gdk_display_get_monitor_at_point (gtk_widget_get_display (f->button),
                                                        record.left, record.top);
  GdkRectangle bounds; gdk_monitor_get_geometry (monitor, &bounds);
  gint expected_x = record.left, expected_y = record.top + record.anchor_height, actual_x, actual_y;
  if (expected_x + record.width > bounds.x + bounds.width)
    expected_x = record.left + record.anchor_width - record.width;
  if (expected_y + record.height > bounds.y + bounds.height)
    expected_y = record.top - record.height;
  /* Brush's fresh host preserves preferred/flip placement, then constrains a
   * fitting form before mapping. Derive this independently from the anchor
   * and natural-size snapshot, never from the observed popup coordinates. */
  expected_x = CLAMP (expected_x, bounds.x, MAX (bounds.x, bounds.x + bounds.width - record.width));
  expected_y = CLAMP (expected_y, bounds.y, MAX (bounds.y, bounds.y + bounds.height - record.height));
  gtk_window_get_position (GTK_WINDOW (host), &actual_x, &actual_y);
  g_assert_cmpint (actual_x, ==, expected_x); g_assert_cmpint (actual_y, ==, expected_y);
  g_test_message ("Brush native anchor=%d/%d %dx%d natural=%dx%d popup=%d/%d",
                  record.left, record.top, record.anchor_width, record.anchor_height,
                  record.width, record.height, actual_x, actual_y);
  return host;
}

static GtkSpinButton *
brush_popup_spin (GtkWidget *host, const gchar *property)
{
  gchar *name = g_strconcat ("painter-brush-resource-", property, NULL);
  GtkWidget *spin = find (host, name, GTK_TYPE_SPIN_BUTTON); g_free (name); g_assert_nonnull (spin);
  return GTK_SPIN_BUTTON (spin);
}

#ifdef GDK_WINDOWING_X11
static void
brush_popup_grid_click (RectanglePopupInput *input, GtkWidget *host, GimpBrush *brush)
{
  GimpContainerIconView *selector = GIMP_CONTAINER_ICON_VIEW (
    find (host, "painter-brush-popup-selector", GIMP_TYPE_CONTAINER_ICON_VIEW));
  GtkTreeIter iter; GtkTreePath *path = NULL;
  gboolean valid = gtk_tree_model_get_iter_first (selector->model, &iter);
  while (valid) {
    GimpViewRenderer *renderer = NULL;
    gtk_tree_model_get (selector->model, &iter, GIMP_CONTAINER_TREE_STORE_COLUMN_RENDERER, &renderer, -1);
    gboolean matches = renderer && renderer->viewable == GIMP_VIEWABLE (brush);
    g_clear_object (&renderer);
    if (matches) { path = gtk_tree_model_get_path (selector->model, &iter); break; }
    valid = gtk_tree_model_iter_next (selector->model, &iter);
  }
  g_assert_nonnull (path);
  gtk_icon_view_scroll_to_path (selector->view, path, TRUE, 0.5, 0.5); drain (); rectangle_popup_settle ();
  GdkRectangle cell; g_assert_true (gtk_icon_view_get_cell_rect (selector->view, path, NULL, &cell));
  gint wx, wy, root_x, root_y, local_x, local_y;
  /* gtk_icon_view_get_cell_rect() returns widget coordinates. */
  wx = cell.x + cell.width / 2; wy = cell.y + cell.height / 2;
  g_assert_true (gtk_widget_translate_coordinates (GTK_WIDGET (selector->view), host, wx, wy, &local_x, &local_y));
  gdk_window_get_origin (gtk_widget_get_window (host), &root_x, &root_y);
  gint scale = gtk_widget_get_scale_factor (host); GdkDisplay *display = gtk_widget_get_display (host);
  gpointer native = gdk_x11_display_get_xdisplay (display);
  input->motion (native, -1, (root_x + local_x) * scale, (root_y + local_y) * scale, 0);
  input->button (native, 1, TRUE, 0); input->button (native, 1, FALSE, 0);
  gdk_display_flush (display); rectangle_popup_settle (); drain (); gtk_tree_path_free (path);
  g_assert_true (gimp_context_get_brush (gimp_container_view_get_context (GIMP_CONTAINER_VIEW (selector))) == brush);
  GtkWidget *preview = find (host, "painter-brush-popup-preview", GIMP_TYPE_VIEW);
  g_assert_true (GIMP_VIEW (preview)->renderer->viewable == GIMP_VIEWABLE (brush));
  g_assert_true (gimp_container_view_is_item_selected (GIMP_CONTAINER_VIEW (selector), GIMP_VIEWABLE (brush)));
  brush_popup_title (host, brush);
  g_assert_true (gtk_widget_get_mapped (host));
}
#endif

static void
brush_popup_capture_to (GtkWidget *host, const gchar *path)
{
  gint x, y; gtk_window_get_position (GTK_WINDOW (host), &x, &y);
  gint width = gtk_widget_get_allocated_width (host), height = gtk_widget_get_allocated_height (host);
  GdkRectangle bounds; GdkMonitor *monitor = gdk_display_get_monitor_at_window (
    gtk_widget_get_display (host), gtk_widget_get_window (host));
  gdk_monitor_get_geometry (monitor, &bounds);
  g_assert_cmpint (x, >=, bounds.x); g_assert_cmpint (y, >=, bounds.y);
  g_assert_cmpint (x + width, <=, bounds.x + bounds.width);
  g_assert_cmpint (y + height, <=, bounds.y + bounds.height);
  g_test_message ("Brush popup logical=%d/%d %dx%d scale=%d", x, y, width, height, gtk_widget_get_scale_factor (host));
  if (!path) return;
  GdkWindow *root = gdk_screen_get_root_window (gtk_widget_get_screen (host));
  GdkPixbuf *pixels = gdk_pixbuf_get_from_window (root, 0, 0, gdk_window_get_width (root), gdk_window_get_height (root));
  GError *error = NULL; g_assert_nonnull (pixels);
  g_assert_true (gdk_pixbuf_save (pixels, path, "png", &error, NULL)); g_assert_no_error (error); g_object_unref (pixels);
}

static void
brush_popup_capture (GtkWidget *host)
{
  brush_popup_capture_to (host, g_getenv ("GIMP_BRUSH_POPUP_CAPTURE"));
}

static void
brush_popup_actual_entry (void)
{
  if (!transform_popup_require_x11_input ()) return;
#ifdef GDK_WINDOWING_X11
  BrushPopupFixture f = brush_popup_fixture ();
  GtkWidget *host = brush_popup_open (&f, TRUE); g_object_ref (host);
  g_assert_cmpint (gtk_button_get_relief (GTK_BUTTON (f.button)), ==, GTK_RELIEF_NONE);
  gchar *tooltip = gtk_widget_get_tooltip_text (f.button); g_assert_nonnull (tooltip);
  g_assert_cmpuint (strlen (tooltip), >, 0); g_free (tooltip);
  /* This property-bound view is tagged "brush". find(..., NULL, ...) matches
   * an unbound widget, rather than treating NULL as a property wildcard. */
  GtkWidget *thumbnail = find (f.button, "brush", GIMP_TYPE_VIEW); g_assert_nonnull (thumbnail);
  g_assert_true (GIMP_VIEW (thumbnail)->renderer->viewable == GIMP_VIEWABLE (f.a.brush));
  GtkWidget *body = brush_popup_assert_body (host, f.options); g_object_ref (body);
  RectanglePopupInput input = transform_popup_input ();
  brush_popup_grid_click (&input, host, f.b.brush);
  g_assert_true (GIMP_VIEW (thumbnail)->renderer->viewable == GIMP_VIEWABLE (f.b.brush));
  g_assert_cmpfloat (gtk_spin_button_get_value (brush_popup_spin (host, "radius")), ==, 19);
  brush_popup_grid_click (&input, host, f.a.brush);
  g_assert_true (GIMP_VIEW (thumbnail)->renderer->viewable == GIMP_VIEWABLE (f.a.brush));
  GtkWidget *diamond = brush_popup_shape_radio (host, GIMP_BRUSH_GENERATED_DIAMOND);
  transform_popup_click_center (&input, diamond); drain ();
  g_assert_true (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (diamond)));
  g_assert_cmpint (gimp_brush_generated_get_shape (GIMP_BRUSH_GENERATED (f.a.brush)), ==, GIMP_BRUSH_GENERATED_DIAMOND);
  g_assert_true (gtk_widget_get_mapped (host));
  /* Actual keyboard input continues through the native grab; it does not
   * confirm/destroy the outer host when Return commits the spin entry. */
  GtkWidget *size = find (host, "brush-size", GTK_TYPE_SPIN_BUTTON); g_assert_nonnull (size);
  g_object_set (f.options, "brush-size", 73.0, NULL);
  transform_popup_click_center (&input, size);
  gdouble before_key; g_object_get (f.options, "brush-size", &before_key, NULL);
  rectangle_popup_key (&input, host, GDK_KEY_Up);
  rectangle_popup_key (&input, host, GDK_KEY_Return);
  g_assert_true (gtk_widget_get_mapped (host));
  gdouble value; g_object_get (f.options, "brush-size", &value, NULL);
  g_assert_cmpfloat (value, >, before_key);
  brush_popup_no_action (&f); brush_popup_capture (host);
  rectangle_popup_key (&input, host, GDK_KEY_Escape); drain ();
  g_assert_false (gtk_widget_get_realized (host));
  g_assert_true (gimp_context_get_brush (GIMP_CONTEXT (f.options)) == f.a.brush);
  /* The old thumbnail consumes wheel input instead of cycling resources. */
  GdkEvent *wheel = gdk_event_new (GDK_SCROLL);
  wheel->scroll.window = g_object_ref (gtk_widget_get_window (f.button));
  wheel->scroll.send_event = TRUE; wheel->scroll.time = GDK_CURRENT_TIME;
  wheel->scroll.direction = GDK_SCROLL_DOWN;
  gdk_event_set_device (wheel, gdk_seat_get_pointer (gdk_display_get_default_seat (gtk_widget_get_display (f.button))));
  g_assert_true (gtk_widget_event (f.button, wheel)); gdk_event_free (wheel); drain ();
  g_assert_true (gimp_context_get_brush (GIMP_CONTEXT (f.options)) == f.a.brush);
  GtkWidget *next = brush_popup_open (&f, FALSE); g_object_ref (next);
  g_assert_true (next != host); g_assert_true (find (next, "painter-brush-popup-body", 0) != body);
  g_assert_cmpfloat (gtk_spin_button_get_value (GTK_SPIN_BUTTON (find (next, "brush-size", GTK_TYPE_SPIN_BUTTON))), ==, value);
  g_assert_false (standard_toggle_event (next, 0));
  g_assert_false (gtk_widget_get_realized (next));
  brush_popup_bytes_unchanged (&f.a); brush_popup_bytes_unchanged (&f.b);
  g_object_unref (next); g_object_unref (body); g_object_unref (host); g_module_close (input.module);
  brush_popup_no_action (&f); brush_popup_finish (&f);
#endif
}

#ifdef GDK_WINDOWING_X11
typedef struct {
  GtkWidget *host, *body, *viewport, *bar, *preview;
  GtkWidget *spins[5];
  GtkAdjustment *adjustment;
  GdkDisplay *display;
  GdkDevice *pointer;
  RectanglePopupInput input;
} BrushLowerInput;

/* Each field above is independently retained for the complete input scope.
 * A host reference alone does not retain children after GTK teardown. */
static void
brush_popup_lower_live (BrushLowerInput *s, GtkWidget *target, gboolean viewable)
{
  GdkWindow *window;
  g_assert_false (gdk_display_is_closed (s->display));
  g_assert_false (gtk_widget_in_destruction (s->host));
  g_assert_true (gtk_widget_get_realized (s->host));
  g_assert_true (gtk_widget_get_mapped (s->host));
  window = gtk_widget_get_window (s->host);
  g_assert_nonnull (window); g_assert_true (gdk_window_is_viewable (window));
  /* Host first, then body/viewport topology, before any dependent getter. */
  g_assert_false (gtk_widget_in_destruction (s->body));
  g_assert_true (gtk_widget_get_realized (s->body));
  g_assert_true (gtk_widget_get_mapped (s->body));
  g_assert_true (gtk_widget_is_ancestor (s->body, s->host));
  g_assert_true (gtk_bin_get_child (GTK_BIN (s->body)) == s->viewport);
  g_assert_false (gtk_widget_in_destruction (s->viewport));
  g_assert_true (gtk_widget_get_realized (s->viewport));
  g_assert_true (gtk_widget_get_mapped (s->viewport));
  g_assert_true (gtk_widget_is_ancestor (s->viewport, s->body));
  g_assert_false (gtk_widget_in_destruction (s->bar));
  g_assert_true (gtk_scrolled_window_get_vscrollbar (GTK_SCROLLED_WINDOW (s->body)) == s->bar);
  g_assert_true (gtk_widget_is_ancestor (s->bar, s->body));
  g_assert_true (gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (s->body)) == s->adjustment);
  if (target) {
    g_assert_false (gtk_widget_in_destruction (target));
    g_assert_true (gtk_widget_get_realized (target));
    g_assert_true (gtk_widget_get_mapped (target));
    g_assert_true (gtk_widget_is_ancestor (target, s->body));
    if (target == s->bar)
      g_assert_true (gtk_range_get_adjustment (GTK_RANGE (target)) == s->adjustment);
    if (viewable) {
      window = gtk_widget_get_window (target);
      g_assert_nonnull (window); g_assert_true (gdk_window_is_viewable (window));
    }
  }
}

/* Geometry/adjustment reads are oracles only, never reveal setters. */
static gint
brush_popup_lower_clip (BrushLowerInput *s, GtkWidget *widget)
{
  GdkWindow *view;
  gint vx, vy, hx, hy, x, y, width, height;
  brush_popup_lower_live (s, widget, TRUE);
  view = gtk_viewport_get_view_window (GTK_VIEWPORT (s->viewport));
  g_assert_nonnull (view); g_assert_true (gdk_window_is_viewable (view));
  gdk_window_get_origin (view, &vx, &vy);
  gdk_window_get_origin (gtk_widget_get_window (s->host), &hx, &hy);
  g_assert_true (gtk_widget_translate_coordinates (widget, s->host, 0, 0, &x, &y));
  x += hx - vx; y += hy - vy;
  width = gtk_widget_get_allocated_width (widget);
  height = gtk_widget_get_allocated_height (widget);
  g_assert_cmpint (width, >, 0); g_assert_cmpint (height, >, 0);
  g_assert_cmpint (x, >=, 0); /* Horizontal scrolling is NEVER. */
  g_assert_cmpint (x + width, <=, gdk_window_get_width (view));
  g_assert_cmpint (height, <=, gdk_window_get_height (view));
  if (y < 0) return -1;
  if (y + height > gdk_window_get_height (view)) return 1;
  return 0;
}

typedef struct { gint x, y, local_x, local_y, width, height, scale; } BrushLowerPoint;

static BrushLowerPoint
brush_popup_lower_point (BrushLowerInput *s, GtkWidget *widget, gboolean number)
{
  BrushLowerPoint p = {0};
  gint hx, hy;
  brush_popup_lower_live (s, widget, TRUE);
  if (widget != s->bar) g_assert_cmpint (brush_popup_lower_clip (s, widget), ==, 0);
  p.width = gtk_widget_get_allocated_width (widget);
  p.height = gtk_widget_get_allocated_height (widget);
  p.local_x = p.width / 2; p.local_y = p.height / 2;
  if (number) {
    GdkRectangle text;
    PangoRectangle logical;
    g_assert_true (GTK_IS_SPIN_BUTTON (widget));
    /* Preserve GimpSpinScale TARGET_NUMBER, not the scale-changing label. */
    gtk_entry_get_text_area (GTK_ENTRY (widget), &text);
    gtk_entry_get_layout_offsets (GTK_ENTRY (widget), &p.local_x, &p.local_y);
    pango_layout_get_pixel_extents (gtk_entry_get_layout (GTK_ENTRY (widget)), NULL, &logical);
    g_assert_cmpint (logical.width, >, 0); g_assert_cmpint (logical.height, >, 0);
    p.local_x += logical.width / 2;
    p.local_y = text.y + (p.local_y - text.y + logical.height) / 2;
  }
  g_assert_cmpint (p.local_x, >=, 0); g_assert_cmpint (p.local_x, <, p.width);
  g_assert_cmpint (p.local_y, >=, 0); g_assert_cmpint (p.local_y, <, p.height);
  g_assert_true (gtk_widget_translate_coordinates (widget, s->host, p.local_x, p.local_y, &p.x, &p.y));
  gdk_window_get_origin (gtk_widget_get_window (s->host), &hx, &hy);
  p.x += hx; p.y += hy;
  p.scale = gdk_window_get_scale_factor (gtk_widget_get_window (s->host));
  return p;
}

static gboolean
brush_popup_lower_same_point (BrushLowerPoint a, BrushLowerPoint b)
{
  return a.x == b.x && a.y == b.y && a.local_x == b.local_x && a.local_y == b.local_y &&
         a.width == b.width && a.height == b.height && a.scale == b.scale;
}

typedef struct {
  BrushLowerInput *scope;
  GtkWidget *target; /* Retained by aim for this complete callback/wait scope. */
  gboolean number;
  guint frames, stable;
  BrushLowerPoint point;
} BrushLowerFrames;

static void
brush_popup_lower_after_paint (GdkFrameClock *clock, gpointer data)
{
  BrushLowerFrames *f = data;
  /* point() checks host and retained child liveness before all geometry reads. */
  BrushLowerPoint current = brush_popup_lower_point (f->scope, f->target, f->number);
  f->stable = f->frames && brush_popup_lower_same_point (current, f->point) ? f->stable + 1 : 0;
  f->point = current; ++f->frames;
  if (f->stable < 3) {
    gtk_widget_queue_draw (f->target);
    gdk_frame_clock_request_phase (clock, GDK_FRAME_CLOCK_PHASE_AFTER_PAINT);
  }
}

static BrushLowerPoint
brush_popup_lower_wait_point (BrushLowerInput *s, GtkWidget *target, gboolean number)
{
  BrushLowerFrames f = {0};
  GdkFrameClock *clock;
  gulong handler;
  gint64 deadline;
  brush_popup_lower_live (s, target, TRUE);
  f.scope = s; f.target = target; f.number = number;
  clock = gtk_widget_get_frame_clock (target); g_assert_nonnull (clock); g_object_ref (clock);
  handler = g_signal_connect (clock, "after-paint", G_CALLBACK (brush_popup_lower_after_paint), &f);
  gdk_frame_clock_begin_updating (clock); gtk_widget_queue_draw (target);
  gdk_frame_clock_request_phase (clock, GDK_FRAME_CLOCK_PHASE_AFTER_PAINT);
  deadline = g_get_monotonic_time () + 3 * G_TIME_SPAN_SECOND;
  while (f.stable < 3 && g_get_monotonic_time () < deadline) {
    g_main_context_iteration (NULL, FALSE);
    brush_popup_lower_live (s, target, TRUE);
    g_usleep (1000);
  }
  gdk_frame_clock_end_updating (clock); g_signal_handler_disconnect (clock, handler); g_object_unref (clock);
  brush_popup_lower_live (s, target, TRUE);
  g_assert_cmpuint (f.stable, >=, 3);
  return f.point;
}

/* Strict point-based aim shared by hover, wheel and numeric-entry presses. */
static void
brush_popup_lower_aim (BrushLowerInput *s, GtkWidget *target, gboolean number)
{
  BrushLowerPoint point, after;
  GdkWindow *hit, *root;
  gpointer owner = NULL;
  gint px, py, wx, wy;
  g_object_ref (target);
  point = brush_popup_lower_wait_point (s, target, number);
  g_assert_true (s->input.motion (gdk_x11_display_get_xdisplay (s->display), -1,
                                 point.x * point.scale, point.y * point.scale, 0));
  gdk_display_flush (s->display); rectangle_popup_settle ();
  brush_popup_lower_live (s, target, TRUE);
  after = brush_popup_lower_wait_point (s, target, number);
  /* Recompute the actual numeric point after hover, not merely widget center. */
  if (!brush_popup_lower_same_point (point, after)) {
    point = after;
    g_assert_true (s->input.motion (gdk_x11_display_get_xdisplay (s->display), -1,
                                   point.x * point.scale, point.y * point.scale, 0));
    gdk_display_flush (s->display); rectangle_popup_settle ();
    brush_popup_lower_live (s, target, TRUE);
    after = brush_popup_lower_wait_point (s, target, number);
    g_assert_true (brush_popup_lower_same_point (point, after));
  }
  brush_popup_lower_live (s, target, TRUE);
  after = brush_popup_lower_point (s, target, number);
  g_assert_true (brush_popup_lower_same_point (point, after));
  root = gdk_screen_get_root_window (gtk_widget_get_screen (s->host));
  g_assert_cmpint (point.x, >=, 0); g_assert_cmpint (point.y, >=, 0);
  g_assert_cmpint (point.x, <, gdk_window_get_width (root));
  g_assert_cmpint (point.y, <, gdk_window_get_height (root));
  gdk_device_get_position (s->pointer, NULL, &px, &py);
  hit = gdk_device_get_window_at_position (s->pointer, &wx, &wy);
  if (hit) gdk_window_get_user_data (hit, &owner);
  g_assert_nonnull (hit); g_assert_true (gdk_window_is_viewable (hit));
  g_assert_true (GTK_IS_WIDGET (owner));
  g_assert_true (owner == target || gtk_widget_is_ancestor (GTK_WIDGET (owner), target));
  g_assert_cmpint (px, ==, point.x); g_assert_cmpint (py, ==, point.y);
  g_object_unref (target);
}

static void
brush_popup_lower_reveal_bar (BrushLowerInput *s)
{
  GdkWindow *window;
  gboolean overlay;
  brush_popup_lower_live (s, s->bar, FALSE);
  overlay = gtk_scrolled_window_get_overlay_scrolling (GTK_SCROLLED_WINDOW (s->body));
  window = gtk_widget_get_window (s->bar);
  /* A mapped GTK overlay bar can have a hidden indicator parent. Reveal it
   * only by actual captured motion on an already-visible content descendant. */
  if (!window || !gdk_window_is_viewable (window)) {
    const gchar *names[] = { "brush-size", "brush-aspect-ratio", "brush-angle",
      "painter-brush-resource-spacing", "painter-brush-resource-radius",
      "painter-brush-resource-spikes", "painter-brush-resource-hardness",
      "painter-brush-resource-aspect-ratio", "painter-brush-resource-angle",
      "painter-brush-popup-preview" };
    GtkWidget *hover = NULL;
    gint64 deadline;
    for (guint i = 0; i < G_N_ELEMENTS (names); ++i) {
      GtkWidget *candidate = find (s->body, names[i], 0);
      g_assert_nonnull (candidate);
      if (brush_popup_lower_clip (s, candidate) == 0) { hover = candidate; break; }
    }
    g_assert_nonnull (hover);
    brush_popup_lower_aim (s, hover, FALSE);
    deadline = g_get_monotonic_time () + 2 * G_TIME_SPAN_SECOND;
    for (;;) {
      brush_popup_lower_live (s, s->bar, FALSE);
      window = gtk_widget_get_window (s->bar);
      if ((window && gdk_window_is_viewable (window)) || g_get_monotonic_time () >= deadline) break;
      g_main_context_iteration (NULL, FALSE); g_usleep (1000);
    }
  }
  brush_popup_lower_live (s, s->bar, TRUE);
  g_test_message ("Brush lower scrollbar after physical-hover readiness mapped=1 viewable=1 overlay=%d", overlay);
  g_assert_cmpint (gtk_scrolled_window_get_overlay_scrolling (GTK_SCROLLED_WINDOW (s->body)), ==, overlay);
}

static gdouble
brush_popup_lower_value (BrushLowerInput *s)
{
  brush_popup_lower_live (s, NULL, FALSE);
  return gtk_adjustment_get_value (s->adjustment);
}

/* Stable frames alone can precede GTK's overlay hover/resize transition.
 * Enter the actual scrollbar first, without sending a button or setting CSS. */
static gboolean
brush_popup_lower_enter_bar (BrushLowerInput *s)
{
  gboolean indicator, intended = FALSE, hovering = FALSE;
  gint64 deadline = g_get_monotonic_time () + 3 * G_TIME_SPAN_SECOND;
  brush_popup_lower_live (s, s->bar, TRUE);
  indicator = gtk_style_context_has_class (gtk_widget_get_style_context (s->bar), "overlay-indicator");
  do {
    BrushLowerPoint point = brush_popup_lower_point (s, s->bar, FALSE);
    GdkWindow *root = gdk_screen_get_root_window (gtk_widget_get_screen (s->host));
    GdkWindow *hit;
    gpointer owner = NULL;
    gint px, py, wx, wy;
    gboolean moved, current_indicator, hover_class;
    g_test_message ("Brush lower scrollbar enter requested-root=%d/%d size=%d/%d scale=%d",
                    point.x, point.y, point.width, point.height, point.scale);
    g_assert_cmpint (point.x, >=, 0); g_assert_cmpint (point.y, >=, 0);
    g_assert_cmpint (point.x, <, gdk_window_get_width (root));
    g_assert_cmpint (point.y, <, gdk_window_get_height (root));
    moved = s->input.motion (gdk_x11_display_get_xdisplay (s->display), -1,
                             point.x * point.scale, point.y * point.scale, 0);
    g_test_message ("Brush lower scrollbar enter motion=%d", moved);
    g_assert_true (moved);
    gdk_display_sync (s->display);
    /* Dispatch pending real input/style events, not an elapsed-time sleep. */
    while (g_main_context_pending (NULL) && g_get_monotonic_time () < deadline) {
      g_main_context_iteration (NULL, FALSE);
      brush_popup_lower_live (s, s->bar, TRUE);
    }
    brush_popup_lower_live (s, s->bar, TRUE);
    gdk_device_get_position (s->pointer, NULL, &px, &py);
    hit = gdk_device_get_window_at_position (s->pointer, &wx, &wy);
    if (hit) gdk_window_get_user_data (hit, &owner);
    intended = hit && gdk_window_is_viewable (hit) && GTK_IS_WIDGET (owner) &&
      (owner == s->bar || gtk_widget_is_ancestor (GTK_WIDGET (owner), s->bar));
    current_indicator = gtk_style_context_has_class (gtk_widget_get_style_context (s->bar), "overlay-indicator");
    hover_class = gtk_style_context_has_class (gtk_widget_get_style_context (s->bar), "hovering");
    hovering = !indicator || hover_class;
    g_test_message ("Brush lower scrollbar enter actual-root=%d/%d hit=%s intended=%d overlay-indicator=%d hovering-class=%d",
                    px, py, GTK_IS_WIDGET (owner) ? G_OBJECT_TYPE_NAME (owner) : "none",
                    intended, current_indicator, hover_class);
    g_assert_cmpint (px, ==, point.x); g_assert_cmpint (py, ==, point.y);
    g_assert_cmpint (current_indicator, ==, indicator);
    /* A revealed indicator can need another real motion to receive hover.
     * Both native hit and semantic entry must precede the unchanged aim. */
  } while ((!intended || !hovering) && g_get_monotonic_time () < deadline);
  g_assert_true (intended); g_assert_true (hovering);
  return indicator;
}

static void
brush_popup_lower_wheel (BrushLowerInput *s, gboolean down)
{
  gdouble before;
  gint64 deadline;
  gboolean indicator;
  brush_popup_lower_live (s, s->bar, FALSE);
  g_assert_true (gtk_widget_is_sensitive (s->bar));
  brush_popup_lower_reveal_bar (s);
  indicator = brush_popup_lower_enter_bar (s);
  brush_popup_lower_aim (s, s->bar, FALSE);
  brush_popup_lower_live (s, s->bar, TRUE);
  g_assert_cmpint (gtk_style_context_has_class (gtk_widget_get_style_context (s->bar), "overlay-indicator"), ==, indicator);
  g_assert_true (!indicator || gtk_style_context_has_class (gtk_widget_get_style_context (s->bar), "hovering"));
  before = brush_popup_lower_value (s);
  g_assert_true (s->input.button (gdk_x11_display_get_xdisplay (s->display), down ? 5 : 4, TRUE, 0));
  g_assert_true (s->input.button (gdk_x11_display_get_xdisplay (s->display), down ? 5 : 4, FALSE, 0));
  gdk_display_flush (s->display);
  deadline = g_get_monotonic_time () + G_TIME_SPAN_SECOND;
  while (brush_popup_lower_value (s) == before && g_get_monotonic_time () < deadline) {
    g_main_context_iteration (NULL, FALSE); g_usleep (1000);
  }
  brush_popup_lower_live (s, s->bar, TRUE);
  if (down) g_assert_cmpfloat (brush_popup_lower_value (s), >, before);
  else      g_assert_cmpfloat (brush_popup_lower_value (s), <, before);
  rectangle_popup_settle (); drain ();
  brush_popup_lower_live (s, s->bar, TRUE);
  g_assert_true (gtk_grab_get_current () == s->host);
}

static guint
brush_popup_lower_bottom (BrushLowerInput *s)
{
  gdouble lower, maximum;
  guint ticks = 0;
  gint64 deadline = g_get_monotonic_time () + 10 * G_TIME_SPAN_SECOND;
  brush_popup_lower_live (s, NULL, FALSE);
  lower = gtk_adjustment_get_lower (s->adjustment);
  maximum = MAX (lower, gtk_adjustment_get_upper (s->adjustment) - gtk_adjustment_get_page_size (s->adjustment));
  if (maximum > lower && brush_popup_lower_value (s) >= maximum - 0.5) {
    brush_popup_lower_wheel (s, FALSE); ++ticks;
  }
  while (brush_popup_lower_value (s) < maximum - 0.5 && g_get_monotonic_time () < deadline) {
    brush_popup_lower_wheel (s, TRUE); ++ticks;
  }
  g_assert_cmpfloat_with_epsilon (brush_popup_lower_value (s), maximum, 0.5);
  g_test_message ("Brush lower scroll overflow=%d wheel-events=%u bottom=%g/%g",
                  maximum > lower, ticks, brush_popup_lower_value (s), maximum);
  return ticks;
}

static void
brush_popup_lower_click_number (BrushLowerInput *s, GtkWidget *spin)
{
  gint direction;
  gint64 deadline = g_get_monotonic_time () + 10 * G_TIME_SPAN_SECOND;
  gdouble before;
  g_object_ref (spin);
  direction = brush_popup_lower_clip (s, spin);
  while (direction && g_get_monotonic_time () < deadline) {
    brush_popup_lower_wheel (s, direction > 0);
    direction = brush_popup_lower_clip (s, spin);
  }
  g_assert_cmpint (direction, ==, 0);
  brush_popup_lower_live (s, spin, TRUE);
  before = gtk_spin_button_get_value (GTK_SPIN_BUTTON (spin));
  brush_popup_lower_aim (s, spin, TRUE);
  /* No event-loop wait between strict numeric-point hit proof and press. */
  g_assert_true (s->input.button (gdk_x11_display_get_xdisplay (s->display), 1, TRUE, 0));
  g_assert_true (s->input.button (gdk_x11_display_get_xdisplay (s->display), 1, FALSE, 0));
  gdk_display_flush (s->display); rectangle_popup_settle (); drain ();
  brush_popup_lower_live (s, spin, TRUE);
  g_assert_true (gtk_window_get_focus (GTK_WINDOW (s->host)) == spin);
  g_assert_cmpfloat (gtk_spin_button_get_value (GTK_SPIN_BUTTON (spin)), ==, before);
  g_assert_cmpint (brush_popup_lower_clip (s, spin), ==, 0);
  g_object_unref (spin);
}
#endif

static void
brush_popup_lower_controls_actual_input (void)
{
  if (!transform_popup_require_x11_input ()) return;
#ifdef GDK_WINDOWING_X11
  {
    BrushPopupFixture f = brush_popup_fixture ();
    GtkWidget *host = brush_popup_open (&f, TRUE);
    GtkWidget *body, *preview;
    BrushLowerInput input = {0};
    GimpDisplay *held_display = g_object_ref (f.shell->display);
    GimpDisplayShell *held_shell = g_object_ref (f.shell);
    GimpImage *held_image = g_object_ref (f.image);
    GimpToolOptions *held_options = g_object_ref (f.options);
    GtkWidget *held_dock = g_object_ref (f.dock);
    gint image_dirty = gimp_image_is_dirty (f.image);
    gint resource_spacing = gimp_brush_get_spacing (f.a.brush);
    gdouble tool_before[3], tool_after[3];
    guint ticks;
    const gchar *fields[] = { "radius", "spikes", "hardness", "aspect-ratio", "angle" };
    GimpBrushGenerated *brush = GIMP_BRUSH_GENERATED (f.a.brush);
    input.host = g_object_ref (host);
    input.body = g_object_ref (brush_popup_assert_body (host, f.options)); body = input.body;
    input.viewport = g_object_ref (gtk_bin_get_child (GTK_BIN (body)));
    g_assert_true (GTK_IS_VIEWPORT (input.viewport));
    input.bar = g_object_ref (gtk_scrolled_window_get_vscrollbar (GTK_SCROLLED_WINDOW (body)));
    input.adjustment = g_object_ref (gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (body)));
    input.preview = g_object_ref (find (host, "painter-brush-popup-preview", GIMP_TYPE_VIEW)); preview = input.preview;
    input.display = g_object_ref (gtk_widget_get_display (host));
    input.pointer = g_object_ref (gdk_seat_get_pointer (gdk_display_get_default_seat (input.display)));
    input.input = transform_popup_input ();
    for (guint i = 0; i < G_N_ELEMENTS (fields); ++i)
      input.spins[i] = g_object_ref (GTK_WIDGET (brush_popup_spin (host, fields[i])));
    g_object_get (f.options, "brush-size", &tool_before[0],
                  "brush-aspect-ratio", &tool_before[1], "brush-angle", &tool_before[2], NULL);
    ticks = brush_popup_lower_bottom (&input);
    g_assert_cmpint (brush_popup_lower_clip (&input, preview), ==, 0);
    for (guint i = 0; i < G_N_ELEMENTS (fields); ++i) {
      GtkWidget *spin = input.spins[i];
      brush_popup_lower_live (&input, spin, TRUE);
      g_assert_true (gtk_widget_is_sensitive (spin));
      brush_popup_lower_click_number (&input, spin);
      if (i + 1 == G_N_ELEMENTS (fields)) {
        GdkDisplay *display = gtk_widget_get_display (host);
        gpointer native = gdk_x11_display_get_xdisplay (display);
        GdkKeymapKey *keys = NULL; gint count = 0;
        g_assert_true (gdk_keymap_get_entries_for_keyval (gdk_keymap_get_for_display (display),
                                                        GDK_KEY_Control_L, &keys, &count));
        g_assert_cmpint (count, >, 0);
        g_assert_true (input.input.key (native, keys[0].keycode, TRUE, 0));
        rectangle_popup_key (&input.input, host, GDK_KEY_a);
        brush_popup_lower_live (&input, spin, TRUE);
        g_assert_true (input.input.key (native, keys[0].keycode, FALSE, 0)); g_free (keys);
        rectangle_popup_key (&input.input, host, GDK_KEY_4);
        brush_popup_lower_live (&input, spin, TRUE);
        rectangle_popup_key (&input.input, host, GDK_KEY_1);
        brush_popup_lower_live (&input, spin, TRUE);
      } else {
        rectangle_popup_key (&input.input, host, GDK_KEY_Up);
        brush_popup_lower_live (&input, spin, TRUE);
      }
      rectangle_popup_key (&input.input, host, GDK_KEY_Return); drain ();
      brush_popup_lower_live (&input, spin, TRUE);
      g_assert_true (gimp_display_get_image (held_display) == held_image);
      g_assert_true (gimp_display_get_shell (held_display) == held_shell);
      g_assert_true (gtk_widget_get_mapped (host));
      g_assert_true (gtk_grab_get_current () == host);
      g_assert_true (gimp_context_get_brush (GIMP_CONTEXT (f.options)) == f.a.brush);
      brush_popup_bytes_unchanged (&f.a); brush_popup_bytes_unchanged (&f.b);
      brush_popup_no_action (&f);
      g_assert_cmpint (gimp_image_is_dirty (f.image), ==, image_dirty);
      g_test_message ("Brush lower physical control %s focused/edited value=%g",
                      fields[i], gtk_spin_button_get_value (GTK_SPIN_BUTTON (spin)));
    }
    g_assert_cmpfloat_with_epsilon (gimp_brush_generated_get_radius (brush), 12, 1e-6);
    g_assert_cmpint (gimp_brush_generated_get_spikes (brush), ==, 3);
    g_assert_cmpfloat_with_epsilon (gimp_brush_generated_get_hardness (brush), 0.51, 1e-6);
    g_assert_cmpfloat_with_epsilon (gimp_brush_generated_get_aspect_ratio (brush), 1.1, 1e-6);
    g_assert_cmpfloat_with_epsilon (gimp_brush_generated_get_angle (brush), 41, 1e-6);
    g_assert_cmpint (gimp_brush_get_spacing (f.a.brush), ==, resource_spacing);
    g_object_get (f.options, "brush-size", &tool_after[0],
                  "brush-aspect-ratio", &tool_after[1], "brush-angle", &tool_after[2], NULL);
    for (guint i = 0; i < G_N_ELEMENTS (tool_before); ++i)
      g_assert_cmpfloat_with_epsilon (tool_after[i], tool_before[i], 1e-6);
    g_assert_true (gimp_data_is_dirty (GIMP_DATA (f.a.brush)));
    g_assert_false (gimp_data_is_dirty (GIMP_DATA (f.b.brush)));
    ticks += brush_popup_lower_bottom (&input);
    g_assert_cmpint (brush_popup_lower_clip (&input, preview), ==, 0);
    g_assert_true (GIMP_VIEW (preview)->renderer->viewable == GIMP_VIEWABLE (f.a.brush));
    g_test_message ("Brush lower controls reachable=5 numeric-entry=41 preview-visible=1 wheel-events=%u scale=%d",
                    ticks, gtk_widget_get_scale_factor (host));
    brush_popup_capture_to (host, g_getenv ("GIMP_BRUSH_POPUP_BOTTOM_CAPTURE"));
    rectangle_popup_key (&input.input, host, GDK_KEY_Escape); drain ();
    g_assert_false (gtk_widget_get_realized (host));
    g_assert_true (gimp_display_get_image (held_display) == held_image);
    g_assert_true (gimp_display_get_shell (held_display) == held_shell);
    g_assert_cmpfloat_with_epsilon (gimp_brush_generated_get_angle (brush), 41, 1e-6);
    g_assert_true (gimp_data_is_dirty (GIMP_DATA (f.a.brush)));
    brush_popup_bytes_unchanged (&f.a); brush_popup_bytes_unchanged (&f.b);
    brush_popup_bytes_unchanged (&f.readonly);
    brush_popup_no_action (&f); g_assert_cmpint (gimp_image_is_dirty (f.image), ==, image_dirty);
    for (guint i = 0; i < G_N_ELEMENTS (input.spins); ++i) g_object_unref (input.spins[i]);
    g_object_unref (input.preview); g_object_unref (input.bar); g_object_unref (input.adjustment);
    g_object_unref (input.viewport); g_object_unref (input.body); g_object_unref (input.host);
    g_object_unref (input.pointer); g_object_unref (input.display); g_module_close (input.input.module);
    brush_popup_finish (&f);
    g_object_unref (held_dock); g_object_unref (held_options); g_object_unref (held_image);
    g_object_unref (held_shell); g_object_unref (held_display);
  }
#endif
}

static void
brush_popup_no_save (gconstpointer mode_data)
{
  const gchar *mode = mode_data; BrushPopupFixture f = brush_popup_fixture ();
  GtkWidget *host = brush_popup_open (&f, FALSE); g_object_ref (host);
  guint body_dead = 0; GtkWidget *body = brush_popup_assert_body (host, f.options);
  g_object_weak_ref (G_OBJECT (body), brush_popup_finalized, &body_dead);
  gtk_spin_button_set_value (brush_popup_spin (host, "radius"), 23); drain ();
  g_assert_cmpfloat (gimp_brush_generated_get_radius (GIMP_BRUSH_GENERATED (f.a.brush)), ==, 23);
  g_assert_true (gimp_data_is_dirty (GIMP_DATA (f.a.brush))); brush_popup_bytes_unchanged (&f.a);
  /* This boundary independently catches GimpDataEditor::set_data autosave. */
  gimp_context_set_brush (GIMP_CONTEXT (f.options), f.b.brush); drain ();
  g_assert_true (gimp_data_is_dirty (GIMP_DATA (f.a.brush))); brush_popup_bytes_unchanged (&f.a);
  g_assert_cmpfloat (gtk_spin_button_get_value (brush_popup_spin (host, "radius")), ==, 19);
  gtk_spin_button_set_value (brush_popup_spin (host, "radius"), 29); drain ();
  g_assert_true (gimp_data_is_dirty (GIMP_DATA (f.b.brush)));
  if (!strcmp (mode, "escape")) transform_popup_real_key (host, GDK_KEY_Escape);
  else if (!strcmp (mode, "outside")) g_assert_false (standard_toggle_event (host, 0));
  else if (!strcmp (mode, "confirm")) g_signal_emit_by_name (host, "confirm");
  else if (!strcmp (mode, "host-destroy")) gtk_widget_destroy (host);
  else if (!strcmp (mode, "hide")) gtk_widget_hide (host);
  else if (!strcmp (mode, "unmap")) gtk_widget_unmap (host);
  else if (!strcmp (mode, "body-destroy")) gtk_widget_destroy (body);
  else if (!strcmp (mode, "toolbar-destroy")) {
    gimp_context_set_tool (gimp_get_user_context (gimp), info ("gimp-pencil-tool"));
  } else g_assert_not_reached ();
  drain (); g_assert_false (gtk_widget_get_realized (host)); g_assert_cmpuint (body_dead, ==, 1);
  g_assert_true (gimp_data_is_dirty (GIMP_DATA (f.a.brush)));
  g_assert_true (gimp_data_is_dirty (GIMP_DATA (f.b.brush)));
  brush_popup_bytes_unchanged (&f.a); brush_popup_bytes_unchanged (&f.b);
  g_assert_cmpfloat (gimp_brush_generated_get_radius (GIMP_BRUSH_GENERATED (f.a.brush)), ==, 23);
  g_assert_cmpfloat (gimp_brush_generated_get_radius (GIMP_BRUSH_GENERATED (f.b.brush)), ==, 29);
  if (strcmp (mode, "toolbar-destroy")) {
    GtkWidget *next = brush_popup_open (&f, FALSE);
    g_assert_cmpfloat (gtk_spin_button_get_value (brush_popup_spin (next, "radius")), ==, 29);
    gtk_widget_destroy (next); drain ();
    brush_popup_bytes_unchanged (&f.a); brush_popup_bytes_unchanged (&f.b);
  }
  /* Positive control: the ordinary explicit resource save path still writes.
   * No new popup Save UI and no global save-vfunc interception is involved. */
  GError *error = NULL;
  g_assert_true (gimp_data_factory_data_save_single (gimp->brush_factory, GIMP_DATA (f.a.brush), &error));
  g_assert_no_error (error); g_assert_false (gimp_data_is_dirty (GIMP_DATA (f.a.brush)));
  gchar *saved = NULL; gsize length = 0;
  g_assert_true (g_file_get_contents (f.a.path, &saved, &length, &error)); g_assert_no_error (error);
  g_assert_true (length != f.a.length || memcmp (saved, f.a.bytes, length) != 0); g_free (saved);
  g_assert_true (gimp_data_is_dirty (GIMP_DATA (f.b.brush))); brush_popup_bytes_unchanged (&f.b);
  g_object_unref (host); brush_popup_finish (&f);
}

static void
brush_popup_count_notify (GObject *object, GParamSpec *pspec, gpointer data)
{
  (void) object; (void) pspec; ++*(guint *) data;
}

/* Global brush sharing forwards each linked tool's native reset through the
 * options manager. GObject notify includes same-value assignments, so its raw
 * cardinality is not one. Preserve that traffic as an ordered native control,
 * rather than hard-coding the observed number of registered paint tools. */
typedef struct {
  const gchar *property;
  GArray *values;
  gdouble initial, last;
  guint transitions;
  gulong handler;
} BrushPopupNotifyTrace;

static void
brush_popup_trace_notify (GObject *object, GParamSpec *pspec, gpointer data)
{
  BrushPopupNotifyTrace *trace = data;
  gdouble value;
  g_assert_cmpstr (pspec->name, ==, trace->property);
  g_object_get (object, trace->property, &value, NULL);
  g_array_append_val (trace->values, value);
  if (value != trace->last) ++trace->transitions;
  trace->last = value;
}

static void
brush_popup_trace_start (GObject *options, const gchar *property,
                         BrushPopupNotifyTrace *trace)
{
  gchar *signal = g_strconcat ("notify::", property, NULL);
  *trace = (BrushPopupNotifyTrace) {0};
  trace->property = property;
  trace->values = g_array_new (FALSE, FALSE, sizeof (gdouble));
  g_object_get (options, property, &trace->initial, NULL);
  trace->last = trace->initial;
  trace->handler = g_signal_connect (options, signal, G_CALLBACK (brush_popup_trace_notify), trace);
  g_free (signal);
}

static void
brush_popup_trace_stop (GObject *options, BrushPopupNotifyTrace *trace)
{
  g_signal_handler_disconnect (options, trace->handler);
  trace->handler = 0;
}

static void
brush_popup_trace_equal (const gchar *action, BrushPopupNotifyTrace *native,
                         BrushPopupNotifyTrace *popup)
{
  GString *native_values = g_string_new (NULL), *popup_values = g_string_new (NULL);
  for (guint i = 0; i < native->values->len; ++i)
    g_string_append_printf (native_values, "%s%.17g", i ? "," : "",
                            g_array_index (native->values, gdouble, i));
  for (guint i = 0; i < popup->values->len; ++i)
    g_string_append_printf (popup_values, "%s%.17g", i ? "," : "",
                            g_array_index (popup->values, gdouble, i));
  g_test_message ("Brush linked %s %s native-raw=%u popup-raw=%u native-transitions=%u popup-transitions=%u native=[%s] popup=[%s]",
                  action, popup->property, native->values->len, popup->values->len,
                  native->transitions, popup->transitions, native_values->str, popup_values->str);
  g_string_free (native_values, TRUE); g_string_free (popup_values, TRUE);
  g_assert_cmpstr (popup->property, ==, native->property);
  g_assert_cmpfloat (popup->initial, ==, native->initial);
  g_assert_cmpuint (native->values->len, >, 0);
  g_assert_cmpuint (popup->values->len, ==, native->values->len);
  for (guint i = 0; i < native->values->len; ++i)
    g_assert_cmpfloat (g_array_index (popup->values, gdouble, i), ==,
                       g_array_index (native->values, gdouble, i));
  /* Chosen initial sentinels differ from the linked canonical value. A single
   * effective transition is required as well as the complete raw trace. */
  g_assert_cmpuint (native->transitions, ==, 1);
  g_assert_cmpuint (popup->transitions, ==, 1);
  g_array_unref (native->values); g_array_unref (popup->values);
  native->values = popup->values = NULL;
}

static void
brush_popup_native_resource_number (GimpBrush *brush, const gchar *property,
                                    gdouble value)
{
  /* Use the identical native setter called by BrushView, not g_object_set(),
   * whose automatic property notification would be a different control. */
  GimpBrushGenerated *generated = GIMP_BRUSH_GENERATED (brush);
  if (!strcmp (property, "radius")) gimp_brush_generated_set_radius (generated, value);
  else if (!strcmp (property, "aspect-ratio")) gimp_brush_generated_set_aspect_ratio (generated, value);
  else if (!strcmp (property, "angle")) gimp_brush_generated_set_angle (generated, value);
  else if (!strcmp (property, "hardness")) gimp_brush_generated_set_hardness (generated, value);
  else if (!strcmp (property, "spacing")) gimp_brush_set_spacing (brush, (gint) value);
  else g_assert_not_reached ();
}

static void
brush_popup_geometry_and_retarget (void)
{
  BrushPopupFixture f = brush_popup_fixture ();
  GtkWidget *host = brush_popup_open (&f, FALSE); g_object_ref (host);
  GtkSpinButton *radius = brush_popup_spin (host, "radius");
  guint a_notify = 0, b_notify = 0;
  gulong a_id = g_signal_connect (f.a.brush, "notify::radius", G_CALLBACK (brush_popup_count_notify), &a_notify);
  gulong b_id = g_signal_connect (f.b.brush, "notify::radius", G_CALLBACK (brush_popup_count_notify), &b_notify);
  g_object_set (f.options, "brush-size", 61.0, "brush-spacing", 0.43, NULL);
  gtk_spin_button_set_value (radius, 17); drain ();
  g_assert_cmpuint (a_notify, ==, 1); g_assert_cmpuint (b_notify, ==, 0);
  gdouble value; g_object_get (f.options, "brush-size", &value, NULL); g_assert_cmpfloat (value, ==, 61.0);
  gtk_spin_button_set_value (radius, 17); drain (); g_assert_cmpuint (a_notify, ==, 1);
  gtk_spin_button_set_value (brush_popup_spin (host, "spacing"), 87); drain ();
  g_assert_cmpint (gimp_brush_get_spacing (f.a.brush), ==, 87);
  g_object_get (f.options, "brush-spacing", &value, NULL); g_assert_cmpfloat (value, ==, 0.43);
  gimp_context_set_brush (GIMP_CONTEXT (f.options), f.b.brush); drain ();
  g_assert_cmpuint (b_notify, ==, 0); g_assert_cmpfloat (gtk_spin_button_get_value (radius), ==, 19);
  gimp_brush_generated_set_radius (GIMP_BRUSH_GENERATED (f.a.brush), 31); drain ();
  g_assert_cmpuint (a_notify, ==, 2); g_assert_cmpfloat (gtk_spin_button_get_value (radius), ==, 19);
  gtk_spin_button_set_value (radius, 21); drain ();
  g_assert_cmpuint (b_notify, ==, 1); g_assert_cmpuint (a_notify, ==, 2);
  gimp_context_set_brush (GIMP_CONTEXT (f.options), f.a.brush); drain ();
  g_assert_cmpfloat (gtk_spin_button_get_value (radius), ==, 31);
  gimp_context_set_brush (GIMP_CONTEXT (f.options), f.a.brush); drain ();
  g_assert_cmpuint (a_notify, ==, 2); g_assert_cmpuint (b_notify, ==, 1);
  g_object_set (f.options, "brush-link-size", TRUE, NULL);
  g_assert_true (gimp->config->global_brush);
  /* Establish native resource-edit and retarget traces without a popup, then
   * restore the exact A/radius31/size61 state before the actual popup edit. */
  gtk_widget_destroy (host); drain (); g_assert_false (gtk_widget_get_realized (host));
  g_object_unref (host); host = NULL; radius = NULL;
  BrushPopupNotifyTrace native_size, native_retarget, popup_size, popup_retarget;
  guint resource_before = a_notify;
  brush_popup_trace_start (G_OBJECT (f.options), "brush-size", &native_size);
  brush_popup_native_resource_number (f.a.brush, "radius", 13); drain ();
  brush_popup_trace_stop (G_OBJECT (f.options), &native_size);
  g_assert_cmpuint (a_notify - resource_before, ==, 1);
  brush_popup_trace_start (G_OBJECT (f.options), "brush-size", &native_retarget);
  gimp_context_set_brush (GIMP_CONTEXT (f.options), f.b.brush); drain ();
  brush_popup_trace_stop (G_OBJECT (f.options), &native_retarget);
  g_assert_cmpuint (a_notify - resource_before, ==, 1); g_assert_cmpuint (b_notify, ==, 1);
  gimp_context_set_brush (GIMP_CONTEXT (f.options), f.a.brush);
  brush_popup_native_resource_number (f.a.brush, "radius", 31);
  g_object_set (f.options, "brush-size", 61.0, NULL); drain ();
  host = brush_popup_open (&f, FALSE); g_object_ref (host);
  radius = brush_popup_spin (host, "radius");
  g_assert_cmpfloat (gtk_spin_button_get_value (radius), ==, 31);
  g_assert_true (gimp_context_get_brush (GIMP_CONTEXT (f.options)) == f.a.brush);
  resource_before = a_notify;
  brush_popup_trace_start (G_OBJECT (f.options), "brush-size", &popup_size);
  gtk_spin_button_set_value (radius, 13); drain ();
  brush_popup_trace_stop (G_OBJECT (f.options), &popup_size);
  g_assert_cmpuint (a_notify - resource_before, ==, 1);
  brush_popup_trace_equal ("resource-edit", &native_size, &popup_size);
  g_object_get (f.options, "brush-size", &value, NULL);
  g_assert_cmpfloat (value, ==, MAX (gimp_brush_get_width (f.a.brush), gimp_brush_get_height (f.a.brush)));
  brush_popup_trace_start (G_OBJECT (f.options), "brush-size", &popup_retarget);
  gimp_context_set_brush (GIMP_CONTEXT (f.options), f.b.brush); drain ();
  brush_popup_trace_stop (G_OBJECT (f.options), &popup_retarget);
  g_assert_cmpuint (a_notify - resource_before, ==, 1); g_assert_cmpuint (b_notify, ==, 1);
  brush_popup_trace_equal ("retarget", &native_retarget, &popup_retarget);
  g_object_get (f.options, "brush-size", &value, NULL);
  g_assert_cmpfloat (value, ==, MAX (gimp_brush_get_width (f.b.brush), gimp_brush_get_height (f.b.brush)));
  GtkWidget *shape = brush_popup_shape_radio (host, GIMP_BRUSH_GENERATED_DIAMOND);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (shape), TRUE);
  gtk_spin_button_set_value (brush_popup_spin (host, "spikes"), 5);
  gtk_spin_button_set_value (brush_popup_spin (host, "hardness"), 0.75);
  gtk_spin_button_set_value (brush_popup_spin (host, "aspect-ratio"), 2.5);
  gtk_spin_button_set_value (brush_popup_spin (host, "angle"), 41); drain ();
  GimpBrushGenerated *b = GIMP_BRUSH_GENERATED (f.b.brush);
  g_assert_cmpint (gimp_brush_generated_get_shape (b), ==, GIMP_BRUSH_GENERATED_DIAMOND);
  g_assert_cmpint (gimp_brush_generated_get_spikes (b), ==, 5);
  g_assert_cmpfloat (gimp_brush_generated_get_hardness (b), ==, 0.75);
  g_assert_cmpfloat (gimp_brush_generated_get_aspect_ratio (b), ==, 2.5);
  g_assert_cmpfloat (gimp_brush_generated_get_angle (b), ==, 41);
  g_signal_handler_disconnect (f.a.brush, a_id); g_signal_handler_disconnect (f.b.brush, b_id);
  brush_popup_no_action (&f); brush_popup_bytes_unchanged (&f.a); brush_popup_bytes_unchanged (&f.b);
  gtk_widget_destroy (host); g_object_unref (host); brush_popup_finish (&f);
}

static void
brush_popup_noneditable_and_null (void)
{
  BrushPopupFixture f = brush_popup_fixture ();
  GtkWidget *host = brush_popup_open (&f, FALSE); g_object_ref (host);
  gimp_context_set_brush (GIMP_CONTEXT (f.options), f.readonly.brush); drain ();
  GtkSpinButton *radius = brush_popup_spin (host, "radius"), *spacing = brush_popup_spin (host, "spacing");
  g_assert_false (gtk_widget_is_sensitive (GTK_WIDGET (radius)));
  gtk_spin_button_set_value (radius, 57); drain ();
  g_assert_cmpfloat (gimp_brush_generated_get_radius (GIMP_BRUSH_GENERATED (f.readonly.brush)), ==, 7);
  g_assert_false (gimp_data_is_dirty (GIMP_DATA (f.readonly.brush)));
  /* Legacy resource spacing was outside the writable generated-geometry
   * frame. Disk-read-only brushes still allow this live-memory adjustment;
   * the native spacing setter intentionally does not mark data dirty. */
  g_assert_true (gtk_widget_is_sensitive (GTK_WIDGET (spacing)));
  gtk_spin_button_set_value (spacing, 63); drain ();
  g_assert_cmpint (gimp_brush_get_spacing (f.readonly.brush), ==, 63);
  g_assert_false (gimp_data_is_dirty (GIMP_DATA (f.readonly.brush)));
  brush_popup_bytes_unchanged (&f.readonly);
  GimpBrush *raster = g_object_new (GIMP_TYPE_BRUSH, "name", "03 Popup Raster", "writable", FALSE, NULL);
  raster->priv->mask = gimp_temp_buf_new (8, 8, babl_format ("Y u8"));
  memset (gimp_temp_buf_get_data (raster->priv->mask), 255, 64);
  /* Registered data must have a persistent identity for the tag cache.
   * This synthetic, non-writable raster has no backing file or image. */
  gimp_data_make_internal (GIMP_DATA (raster),
                           "gimp-test-brush-thumbnail-popup-raster");
  gimp_data_clean (GIMP_DATA (raster));
  gimp_container_add (gimp_data_factory_get_container (gimp->brush_factory), GIMP_OBJECT (raster));
  gimp_context_set_brush (GIMP_CONTEXT (f.options), raster); drain ();
  g_assert_false (gtk_widget_is_sensitive (GTK_WIDGET (radius)));
  g_assert_true (gtk_widget_is_sensitive (GTK_WIDGET (spacing)));
  gtk_spin_button_set_value (spacing, 44); drain (); g_assert_cmpint (gimp_brush_get_spacing (raster), ==, 44);
  gimp_context_set_brush (GIMP_CONTEXT (f.options), NULL); drain ();
  g_assert_null (gimp_context_get_brush (GIMP_CONTEXT (f.options)));
  brush_popup_title (host, NULL);
  g_assert_false (gtk_widget_is_sensitive (GTK_WIDGET (radius)));
  g_assert_false (gtk_widget_is_sensitive (GTK_WIDGET (spacing)));
  gtk_spin_button_set_value (radius, 91); gtk_spin_button_set_value (spacing, 92); drain ();
  g_assert_cmpint (gimp_brush_get_spacing (raster), ==, 44);
  gimp_context_set_brush (GIMP_CONTEXT (f.options), f.a.brush); drain ();
  brush_popup_title (host, f.a.brush);
  g_assert_true (gtk_widget_is_sensitive (GTK_WIDGET (radius)));
  g_assert_cmpfloat (gtk_spin_button_get_value (radius), ==, 11);
  gimp_container_remove (gimp_data_factory_get_container (gimp->brush_factory), GIMP_OBJECT (raster));
  g_object_unref (raster);
  gtk_widget_destroy (host); g_object_unref (host);
  brush_popup_bytes_unchanged (&f.readonly); brush_popup_no_action (&f); brush_popup_finish (&f);
}

typedef struct { BrushPopupFixture *fixture; guint calls; gboolean destroy; } BrushPopupReentry;
static void
brush_popup_radius_reentry (GObject *object, GParamSpec *pspec, gpointer data)
{
  (void) object; (void) pspec; BrushPopupReentry *request = data;
  g_assert_cmpuint (++request->calls, ==, 1);
  if (request->destroy) gtk_widget_destroy (request->fixture->toolbar);
  else gimp_context_set_brush (GIMP_CONTEXT (request->fixture->options), request->fixture->b.brush);
}

typedef struct {
  BrushPopupFixture *fixture;
  gboolean destroy;
  gulong handler;
  guint calls;
} BrushPopupTitleReentry;

static void
brush_popup_title_reentry (GObject *label, GParamSpec *pspec, gpointer data)
{
  BrushPopupTitleReentry *request = data; (void) pspec;
  g_signal_handler_disconnect (label, request->handler);
  ++request->calls;
  if (request->destroy) gtk_widget_destroy (request->fixture->toolbar);
  else gimp_context_set_brush (GIMP_CONTEXT (request->fixture->options), request->fixture->b.brush);
}

static void
brush_popup_reentry_and_stale_control (gconstpointer destroy_data)
{
  BrushPopupFixture f = brush_popup_fixture ();
  GtkWidget *host = brush_popup_open (&f, FALSE); g_object_ref (host);
  GtkSpinButton *radius = brush_popup_spin (host, "radius"); g_object_ref (radius);
  guint body_dead = 0;
  g_object_weak_ref (G_OBJECT (find (host, "painter-brush-popup-body", 0)), brush_popup_finalized, &body_dead);
  BrushPopupReentry request = { &f, 0, GPOINTER_TO_INT (destroy_data) };
  gulong id = g_signal_connect (f.a.brush, "notify::radius", G_CALLBACK (brush_popup_radius_reentry), &request);
  gtk_spin_button_set_value (radius, 15); drain ();
  g_assert_cmpuint (request.calls, ==, 1);
  g_assert_cmpfloat (gimp_brush_generated_get_radius (GIMP_BRUSH_GENERATED (f.a.brush)), ==, 15);
  g_assert_cmpfloat (gimp_brush_generated_get_radius (GIMP_BRUSH_GENERATED (f.b.brush)), ==, 19);
  g_signal_handler_disconnect (f.a.brush, id);
  if (!request.destroy) {
    g_assert_true (gimp_context_get_brush (GIMP_CONTEXT (f.options)) == f.b.brush);
    g_assert_cmpfloat (gtk_spin_button_get_value (radius), ==, 19);
    gtk_widget_destroy (host); drain ();
  }
  g_assert_cmpuint (body_dead, ==, 1); g_assert_false (gtk_widget_get_realized (host));
  /* Holding a destroyed control must not preserve a live mutation closure.
   * Change the resources after retirement, then emit the exact old signal;
   * a stale writeback must not replace these newer canonical values. Do not
   * invoke setters whose GTK disposal preconditions may no longer hold. */
  gimp_brush_generated_set_radius (GIMP_BRUSH_GENERATED (f.a.brush), 33);
  gimp_brush_generated_set_radius (GIMP_BRUSH_GENERATED (f.b.brush), 35);
  g_signal_emit_by_name (radius, "value-changed"); drain ();
  g_assert_cmpfloat (gimp_brush_generated_get_radius (GIMP_BRUSH_GENERATED (f.a.brush)), ==, 33);
  g_assert_cmpfloat (gimp_brush_generated_get_radius (GIMP_BRUSH_GENERATED (f.b.brush)), ==, 35);
  brush_popup_bytes_unchanged (&f.a); brush_popup_bytes_unchanged (&f.b);
  g_object_unref (radius); g_object_unref (host); brush_popup_finish (&f);

  /* The new title write can also synchronously switch context or destroy the
   * toolbar. Exercise that distinct emission after preserving every original
   * resource-notify/stale-control assertion above. */
  BrushPopupFixture title_f = brush_popup_fixture ();
  GtkWidget *title_host = brush_popup_open (&title_f, FALSE); g_object_ref (title_host);
  GtkWidget *title = brush_popup_title (title_host, title_f.a.brush); g_object_ref (title);
  BrushPopupTitleReentry title_request = { &title_f, GPOINTER_TO_INT (destroy_data), 0, 0 };
  title_request.handler = g_signal_connect (title, "notify::label", G_CALLBACK (brush_popup_title_reentry), &title_request);
  gimp_object_set_name (GIMP_OBJECT (title_f.a.brush), "00 Popup renamed during title sync"); drain ();
  g_assert_cmpuint (title_request.calls, ==, 1);
  if (title_request.destroy) g_assert_false (gtk_widget_get_realized (title_host));
  else {
    g_assert_true (gimp_context_get_brush (GIMP_CONTEXT (title_f.options)) == title_f.b.brush);
    brush_popup_title (title_host, title_f.b.brush);
    g_assert_cmpfloat (gtk_spin_button_get_value (brush_popup_spin (title_host, "radius")), ==, 19);
    gimp_object_set_name (GIMP_OBJECT (title_f.a.brush), "00 Stale Popup A"); drain ();
    brush_popup_title (title_host, title_f.b.brush);
    gtk_widget_destroy (title_host);
  }
  brush_popup_bytes_unchanged (&title_f.a); brush_popup_bytes_unchanged (&title_f.b);
  g_object_unref (title); g_object_unref (title_host); brush_popup_no_action (&title_f); brush_popup_finish (&title_f);
}

static void
brush_popup_peers_and_repeat (void)
{
  BrushPopupFixture f = brush_popup_fixture ();
  GtkWidget *host = brush_popup_open (&f, FALSE); g_object_ref (host);
  GtkWidget *peer = gimp_tools_create_painter_toolbar_gui (f.options);
  GtkWidget *peer_window = gtk_window_new (GTK_WINDOW_TOPLEVEL); g_object_ref_sink (peer_window);
  gtk_container_add (GTK_CONTAINER (peer_window), peer); gtk_widget_show_all (peer_window); drain ();
  BrushPopupFixture peer_f = f; peer_f.toolbar = peer; peer_f.button = brush_popup_thumbnail (peer);
  GtkWidget *peer_host = brush_popup_open (&peer_f, FALSE); g_object_ref (peer_host);
  GtkWidget *body = find (host, "painter-brush-popup-body", 0);
  GtkWidget *peer_body = find (peer_host, "painter-brush-popup-body", 0);
  g_assert_true (body != peer_body); g_assert_true (host != peer_host);
  GtkSpinButton *radius = brush_popup_spin (host, "radius");
  GtkSpinButton *peer_radius = brush_popup_spin (peer_host, "radius");
  g_assert_true (radius != peer_radius);
  guint notifications = 0;
  gulong id = g_signal_connect (f.a.brush, "notify::radius", G_CALLBACK (brush_popup_count_notify), &notifications);
  gtk_spin_button_set_value (radius, 16); drain ();
  g_assert_cmpuint (notifications, ==, 1);
  g_assert_cmpfloat (gtk_spin_button_get_value (peer_radius), ==, 16);
  gtk_spin_button_set_value (peer_radius, 18); drain ();
  g_assert_cmpuint (notifications, ==, 2);
  g_assert_cmpfloat (gtk_spin_button_get_value (radius), ==, 18);
  GtkWidget *dock_size = find (f.dock, "brush-size", GTK_TYPE_SPIN_BUTTON);
  g_assert_nonnull (dock_size);
  GtkWidget *size = find (host, "brush-size", GTK_TYPE_SPIN_BUTTON);
  GtkWidget *peer_size = find (peer_host, "brush-size", GTK_TYPE_SPIN_BUTTON);
  g_assert_true (dock_size != size && dock_size != peer_size && size != peer_size);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (dock_size), 47); drain ();
  g_assert_cmpfloat (gtk_spin_button_get_value (GTK_SPIN_BUTTON (size)), ==, 47);
  g_assert_cmpfloat (gtk_spin_button_get_value (GTK_SPIN_BUTTON (peer_size)), ==, 47);
  brush_popup_no_action (&f);
  gtk_widget_destroy (peer_window); g_object_unref (peer_window); drain ();
  g_assert_false (gtk_widget_get_realized (peer_host));
  g_assert_true (gtk_widget_get_mapped (host));
  gtk_spin_button_set_value (radius, 20); drain (); g_assert_cmpuint (notifications, ==, 3);
  g_signal_handler_disconnect (f.a.brush, id);
  gtk_widget_destroy (host); g_object_unref (host); g_object_unref (peer_host); drain ();
  for (guint i = 0; i < 8; ++i) {
    GtkWidget *current = brush_popup_open (&f, FALSE); g_object_ref (current);
    GtkWidget *current_body = find (current, "painter-brush-popup-body", 0); g_object_ref (current_body);
    guint host_dead = 0, body_dead = 0;
    g_object_weak_ref (G_OBJECT (current), brush_popup_finalized, &host_dead);
    g_object_weak_ref (G_OBJECT (current_body), brush_popup_finalized, &body_dead);
    GimpBrush *chosen = i % 2 ? f.b.brush : f.a.brush;
    gimp_context_set_brush (GIMP_CONTEXT (f.options), chosen); drain ();
    g_assert_cmpfloat (gtk_spin_button_get_value (brush_popup_spin (current, "radius")), ==,
                       gimp_brush_generated_get_radius (GIMP_BRUSH_GENERATED (chosen)));
    transform_popup_real_key (current, GDK_KEY_Escape); drain ();
    g_assert_false (gtk_widget_get_realized (current));
    GtkWidget *next = brush_popup_open (&f, FALSE);
    g_assert_true (next != current);
    g_assert_true (find (next, "painter-brush-popup-body", 0) != current_body);
    g_assert_false (standard_toggle_event (next, 0));
    g_object_unref (current_body); g_object_unref (current); drain ();
    g_assert_cmpuint (host_dead, ==, 1); g_assert_cmpuint (body_dead, ==, 1);
  }
  brush_popup_bytes_unchanged (&f.a); brush_popup_bytes_unchanged (&f.b);
  brush_popup_no_action (&f); brush_popup_finish (&f);
}

static void
brush_popup_other_link_flags (void)
{
  BrushPopupFixture f = brush_popup_fixture ();
  GtkWidget *host = brush_popup_open (&f, FALSE); g_object_ref (host);
  const struct {
    const gchar *resource, *property, *link;
    gdouble sentinel, unlinked, linked;
    void (*reset) (GimpPaintOptions *, GimpBrush *);
  } cases[] = {
    { "aspect-ratio", "brush-aspect-ratio", "brush-link-aspect-ratio", 4, 2, 3,
      gimp_paint_options_set_default_brush_aspect_ratio },
    { "angle", "brush-angle", "brush-link-angle", 17, 31, 53,
      gimp_paint_options_set_default_brush_angle },
    { "spacing", "brush-spacing", "brush-link-spacing", 0.44, 81, 93,
      gimp_paint_options_set_default_brush_spacing },
    { "hardness", "brush-hardness", "brush-link-hardness", 0.3, 0.61, 0.72,
      gimp_paint_options_set_default_brush_hardness }
  };
  for (guint i = 0; i < G_N_ELEMENTS (cases); ++i) {
    GtkSpinButton *spin = brush_popup_spin (host, cases[i].resource);
    g_object_set (f.options, cases[i].link, FALSE, cases[i].property, cases[i].sentinel, NULL);
    gtk_spin_button_set_value (spin, cases[i].unlinked); drain ();
    gdouble actual, expected;
    g_object_get (f.options, cases[i].property, &actual, NULL);
    g_assert_cmpfloat_with_epsilon (actual, cases[i].sentinel, 1e-6);
    g_object_set (f.options, cases[i].link, TRUE, NULL);
    g_assert_true (gimp->config->global_brush);
    gtk_widget_destroy (host); drain (); g_assert_false (gtk_widget_get_realized (host));
    g_object_unref (host); host = NULL; spin = NULL;
    gchar *signal = g_strconcat ("notify::", cases[i].resource, NULL);
    guint resource_notifications = 0;
    gulong id = g_signal_connect (f.a.brush, signal, G_CALLBACK (brush_popup_count_notify), &resource_notifications);
    BrushPopupNotifyTrace native, popup;
    brush_popup_trace_start (G_OBJECT (f.options), cases[i].property, &native);
    brush_popup_native_resource_number (f.a.brush, cases[i].resource, cases[i].linked); drain ();
    brush_popup_trace_stop (G_OBJECT (f.options), &native);
    g_assert_cmpuint (resource_notifications, ==, 1);
    brush_popup_native_resource_number (f.a.brush, cases[i].resource, cases[i].unlinked);
    g_object_set (f.options, cases[i].property, cases[i].sentinel, NULL); drain ();
    host = brush_popup_open (&f, FALSE); g_object_ref (host);
    spin = brush_popup_spin (host, cases[i].resource);
    g_assert_cmpfloat_with_epsilon (gtk_spin_button_get_value (spin), cases[i].unlinked, 1e-6);
    g_assert_true (gimp_context_get_brush (GIMP_CONTEXT (f.options)) == f.a.brush);
    resource_notifications = 0;
    brush_popup_trace_start (G_OBJECT (f.options), cases[i].property, &popup);
    gtk_spin_button_set_value (spin, cases[i].linked); drain ();
    brush_popup_trace_stop (G_OBJECT (f.options), &popup);
    g_assert_cmpuint (resource_notifications, ==, 1);
    g_signal_handler_disconnect (f.a.brush, id); g_free (signal);
    brush_popup_trace_equal ("resource-edit", &native, &popup);
    g_object_get (f.options, cases[i].property, &actual, NULL);
    /* The current model's explicit reset is the authoritative linked policy,
     * including painter geometry's intentional angle/aspect defaults. The
     * popup must not invent a legacy unconditional reset or second callback. */
    cases[i].reset (GIMP_PAINT_OPTIONS (f.options), f.a.brush);
    g_object_get (f.options, cases[i].property, &expected, NULL);
    g_assert_cmpfloat_with_epsilon (actual, expected, 1e-6);
    g_object_set (f.options, cases[i].link, FALSE, NULL);
  }
  brush_popup_bytes_unchanged (&f.a); brush_popup_no_action (&f);
  gtk_widget_destroy (host); g_object_unref (host); brush_popup_finish (&f);
}

typedef struct { GimpBrushGenerated *brush; guint calls; } BrushPopupSyncReentry;
static void
brush_popup_mutate_during_sync (GtkSpinButton *spin, gpointer data)
{
  (void) spin; BrushPopupSyncReentry *r = data;
  if (r->calls++) return;
  gimp_brush_generated_set_angle (r->brush, 41);
}

static void
brush_popup_same_target_sync_reentry (void)
{
  BrushPopupFixture f = brush_popup_fixture ();
  GtkWidget *host = brush_popup_open (&f, FALSE); g_object_ref (host);
  GtkSpinButton *radius = brush_popup_spin (host, "radius");
  GtkSpinButton *angle = brush_popup_spin (host, "angle");
  BrushPopupSyncReentry request = { GIMP_BRUSH_GENERATED (f.a.brush), 0 };
  gulong id = g_signal_connect (radius, "value-changed", G_CALLBACK (brush_popup_mutate_during_sync), &request);
  /* Model notify synchronizes radius first. Its observer edits the SAME
   * brush's later angle field, so the outer snapshot must not overwrite the
   * nested update despite target identity and selection epoch being equal. */
  gimp_brush_generated_set_radius (request.brush, 27); drain ();
  brush_popup_title (host, f.a.brush);
  g_assert_cmpuint (request.calls, >=, 1);
  g_assert_cmpfloat (gimp_brush_generated_get_angle (request.brush), ==, 41);
  g_assert_cmpfloat (gtk_spin_button_get_value (angle), ==, 41);
  g_signal_handler_disconnect (radius, id);
  brush_popup_bytes_unchanged (&f.a); brush_popup_no_action (&f);
  gtk_widget_destroy (host); g_object_unref (host); brush_popup_finish (&f);
}

static GtkWidget *
brush_popup_dock_stock_button (GtkWidget *dock)
{
  GPtrArray *widgets = g_ptr_array_new (); gather (dock, widgets);
  GimpContainer *brushes = gimp_data_factory_get_container (gimp->brush_factory);
  GtkWidget *button = NULL;
  for (guint i = 0; i < widgets->len; ++i) {
    GtkWidget *w = g_ptr_array_index (widgets, i);
    if (GIMP_IS_VIEWABLE_BUTTON (w) && GIMP_VIEWABLE_BUTTON (w)->container == brushes) {
      g_assert_null (button); button = w;
    }
  }
  g_ptr_array_unref (widgets); g_assert_nonnull (button); return button;
}

static GtkWidget *
brush_popup_dock_stock_open (GtkWidget *button)
{
  gtk_button_clicked (GTK_BUTTON (button)); drain ();
  GtkWidget *host = standard_toggle_visible (GIMP_TYPE_CONTAINER_POPUP, NULL);
  g_assert_nonnull (host); g_object_ref (host);
  gint64 deadline = g_get_monotonic_time () + 3 * G_TIME_SPAN_SECOND;
  while (gtk_grab_get_current () != host && gtk_widget_get_visible (host) &&
         g_get_monotonic_time () < deadline) {
    g_main_context_iteration (NULL, FALSE); g_usleep (1000);
  }
  g_assert_true (gtk_grab_get_current () == host); return host;
}

static void
brush_popup_stock_dock_policy (void)
{
  BrushPopupFixture f = brush_popup_fixture_with_stock_dock (TRUE);
  GtkWidget *button = brush_popup_dock_stock_button (f.dock);
  g_assert_true (gtk_widget_get_mapped (button));
  GtkWidget *stock = brush_popup_dock_stock_open (button);
  GimpContainerPopup *selector = GIMP_CONTAINER_POPUP (stock);
  g_assert_true (selector->orig_context == GIMP_CONTEXT (f.options));
  g_assert_true (selector->context != selector->orig_context);
  /* This is explicitly a programmatic staging check, not claimed as a row
   * pointer-input oracle. Actual on-canvas grid clicks are tested separately. */
  gimp_context_set_brush (selector->context, f.b.brush); drain ();
  g_assert_true (gimp_context_get_brush (GIMP_CONTEXT (f.options)) == f.a.brush);
  g_assert_true (standard_toggle_event (stock, 0));
  g_assert_false (gtk_widget_get_realized (stock));
  g_assert_true (gimp_context_get_brush (GIMP_CONTEXT (f.options)) == f.a.brush);
  g_object_unref (stock);
  stock = brush_popup_dock_stock_open (button); selector = GIMP_CONTAINER_POPUP (stock);
  gimp_context_set_brush (selector->context, f.b.brush); drain ();
  g_assert_true (gimp_context_get_brush (GIMP_CONTEXT (f.options)) == f.a.brush);
  g_signal_emit_by_name (stock, "confirm"); drain ();
  g_assert_false (gtk_widget_get_realized (stock));
  g_assert_true (gimp_context_get_brush (GIMP_CONTEXT (f.options)) == f.b.brush);
  g_object_unref (stock);
  GtkWidget *outer = brush_popup_open (&f, FALSE); g_object_ref (outer);
  g_assert_true (gimp_container_view_get_context (GIMP_CONTAINER_VIEW (
    find (outer, "painter-brush-popup-selector", GIMP_TYPE_CONTAINER_ICON_VIEW))) == GIMP_CONTEXT (f.options));
  g_assert_false (standard_toggle_event (outer, 0)); g_object_unref (outer);
  brush_popup_bytes_unchanged (&f.a); brush_popup_bytes_unchanged (&f.b);
  brush_popup_no_action (&f); brush_popup_finish (&f);
}

static void
brush_popup_private_target_release (void)
{
  GimpToolInfo *tool = info ("gimp-paintbrush-tool");
  GimpToolOptions *options = g_object_new (G_OBJECT_TYPE (tool->tool_options),
                                          "gimp", gimp, "tool-info", tool, NULL);
  gimp_context_copy_properties (GIMP_CONTEXT (tool->tool_options), GIMP_CONTEXT (options), GIMP_CONTEXT_PROP_MASK_ALL);
  g_assert_null (GIMP_CONTEXT (options)->parent);
  GimpBrush *a = GIMP_BRUSH (gimp_brush_generated_new ("Private unregistered A", GIMP_BRUSH_GENERATED_CIRCLE, 12, 2, 0.5, 1, 0));
  GimpBrush *b = GIMP_BRUSH (gimp_brush_generated_new ("Private unregistered B", GIMP_BRUSH_GENERATED_CIRCLE, 18, 2, 0.5, 1, 0));
  guint a_dead = 0, b_dead = 0, model_dead = 0;
  g_object_weak_ref (G_OBJECT (a), brush_popup_finalized, &a_dead);
  g_object_weak_ref (G_OBJECT (b), brush_popup_finalized, &b_dead);
  g_object_weak_ref (G_OBJECT (options), brush_popup_finalized, &model_dead);
  gimp_context_set_brush (GIMP_CONTEXT (options), a);
  GimpToolOptionsGUIFunc factory = g_object_get_data (G_OBJECT (tool), "gimp-tool-options-gui-horizontal-func");
  g_assert_nonnull (factory);
  /* Canonical dispatcher intentionally rejects private models. Use its exact
   * registered factory solely for owner/source lifetime, never substitute it
   * for the actual on-canvas entry oracle or overwrite tool_info->options. */
  GtkWidget *toolbar = factory (options); g_assert_nonnull (toolbar);
  GtkWidget *window = gtk_window_new (GTK_WINDOW_TOPLEVEL); g_object_ref_sink (window);
  gtk_container_add (GTK_CONTAINER (window), toolbar); gtk_widget_show_all (window); drain ();
  BrushPopupFixture f = {0}; f.options = options; f.toolbar = toolbar; f.button = brush_popup_thumbnail (toolbar);
  GtkWidget *host = brush_popup_open (&f, FALSE); g_object_ref (host);
  /* These resources are deliberately absent from the factory/container.
   * Retargeting every view and releasing our caller ref must finalize old A,
   * not merely hide an observer leak behind permanent fixture ownership. */
  gimp_context_set_brush (GIMP_CONTEXT (options), b); drain ();
  g_object_unref (a); drain (); g_assert_cmpuint (a_dead, ==, 1);
  guint body_dead = 0;
  g_object_weak_ref (G_OBJECT (find (host, "painter-brush-popup-body", 0)), brush_popup_finalized, &body_dead);
  g_object_unref (b); g_object_unref (options);
  g_assert_cmpuint (b_dead, ==, 0); g_assert_cmpuint (model_dead, ==, 0);
  gtk_widget_hide (window); drain ();
  g_assert_false (gtk_widget_get_realized (host)); g_assert_cmpuint (body_dead, ==, 1);
  g_assert_cmpuint (b_dead, ==, 0); g_assert_cmpuint (model_dead, ==, 0);
  gtk_widget_show_all (window); drain ();
  GtkWidget *next = brush_popup_open (&f, FALSE); g_object_ref (next);
  g_assert_true (next != host);
  g_assert_cmpfloat (gtk_spin_button_get_value (brush_popup_spin (next, "radius")), ==, 18);
  gtk_widget_destroy (window); g_object_unref (window); drain ();
  g_assert_false (gtk_widget_get_realized (next));
  g_object_unref (next); g_object_unref (host); drain ();
  g_assert_cmpuint (model_dead, ==, 1); g_assert_cmpuint (b_dead, ==, 1);
}

typedef struct {
  GtkWidget *toolbar;
  gboolean destroy_body;
  guint calls, body_dead;
} BrushPopupConstructionCancel;

static gboolean
brush_popup_cancel_construction (GSignalInvocationHint *hint, guint count,
                                const GValue *values, gpointer data)
{
  BrushPopupConstructionCancel *request = data; (void) hint;
  if (request->calls || count < 2) return TRUE;
  GtkWidget *parent = g_value_get_object (&values[0]);
  GtkWidget *child = g_value_get_object (&values[1]);
  if (g_strcmp0 (gtk_widget_get_name (parent), "painter-brush-popup-body") || !GTK_IS_BOX (child)) return TRUE;
  /* GtkContainer::add is RUN_FIRST: the initial columns have been attached
   * when the emission hook runs. Ignore an internal viewport add, so this
   * interruption occurs at the caller's completed root-add boundary. */
  g_assert_true (gtk_widget_is_ancestor (child, parent));
  ++request->calls;
  g_object_weak_ref (G_OBJECT (parent), brush_popup_finalized, &request->body_dead);
  gtk_widget_destroy (request->destroy_body ? parent : request->toolbar);
  return TRUE;
}

static void
brush_popup_construction_cancel (gconstpointer body_data)
{
  BrushPopupFixture f = brush_popup_fixture ();
  BrushPopupConstructionCancel request = { f.toolbar, GPOINTER_TO_INT (body_data), 0, 0 };
  guint signal = g_signal_lookup ("add", GTK_TYPE_CONTAINER); g_assert_cmpuint (signal, !=, 0);
  gulong hook = g_signal_add_emission_hook (signal, 0, brush_popup_cancel_construction, &request, NULL);
  g_assert_cmpuint (hook, !=, 0);
  gtk_button_clicked (GTK_BUTTON (f.button)); drain ();
  g_signal_remove_emission_hook (signal, hook);
  g_assert_cmpuint (request.calls, ==, 1); g_assert_cmpuint (request.body_dead, ==, 1);
  g_assert_null (standard_toggle_visible (GTK_TYPE_WINDOW, "painter-brush-thumbnail-popup"));
  g_assert_false (gimp_data_is_dirty (GIMP_DATA (f.a.brush)));
  brush_popup_bytes_unchanged (&f.a); brush_popup_bytes_unchanged (&f.b);
  if (request.destroy_body) {
    /* Body-only cancellation leaves the actual toolbar alive. It must not
     * strand opening=true, retain the rejected body, or poison fresh retry. */
    g_assert_true (gtk_widget_get_mapped (f.button));
    GtkWidget *host = brush_popup_open (&f, FALSE); g_object_ref (host);
    gtk_spin_button_set_value (brush_popup_spin (host, "radius"), 24); drain ();
    g_assert_cmpfloat (gimp_brush_generated_get_radius (GIMP_BRUSH_GENERATED (f.a.brush)), ==, 24);
    g_assert_false (standard_toggle_event (host, 0)); g_object_unref (host);
    brush_popup_bytes_unchanged (&f.a);
  }
  brush_popup_no_action (&f); brush_popup_finish (&f);
}

/* Causality baselines only: the executable's ordinary GUI startup and exit
 * are deliberately unchanged. Neither diagnostic opens the Brush popup. */
static void
brush_exit_diagnostic_harness_only (void)
{
  g_test_message ("Brush exit diagnostic: shared GUI harness only; no Brush fixture or popup");
}

static void
brush_exit_diagnostic_fixture_no_popup (void)
{
  g_assert_null (standard_toggle_visible (GTK_TYPE_WINDOW, "painter-brush-thumbnail-popup"));
  BrushPopupFixture f = brush_popup_fixture ();
  g_assert_null (standard_toggle_visible (GTK_TYPE_WINDOW, "painter-brush-thumbnail-popup"));
  brush_popup_finish (&f);
  g_assert_null (standard_toggle_visible (GTK_TYPE_WINDOW, "painter-brush-thumbnail-popup"));
  g_test_message ("Brush exit diagnostic: regular Brush fixture finished; no popup opened");
}

static void
register_brush_thumbnail_popup_tests (void)
{
  g_test_add_func ("/brush-exit-diagnostic/harness-only", brush_exit_diagnostic_harness_only);
  g_test_add_func ("/brush-exit-diagnostic/fixture-no-popup", brush_exit_diagnostic_fixture_no_popup);
  g_test_add_func ("/brush-thumbnail-native-popup/actual-entry", brush_popup_actual_entry);
  const gchar *modes[] = { "escape", "outside", "confirm", "host-destroy", "hide", "unmap", "body-destroy", "toolbar-destroy" };
  for (guint i = 0; i < G_N_ELEMENTS (modes); ++i) {
    gchar *path = g_strconcat ("/brush-thumbnail-native-popup/no-save-", modes[i], NULL);
    g_test_add_data_func (path, modes[i], brush_popup_no_save); g_free (path);
  }
  g_test_add_func ("/brush-thumbnail-native-popup/geometry-retarget-links", brush_popup_geometry_and_retarget);
  g_test_add_func ("/brush-thumbnail-native-popup/other-link-flags", brush_popup_other_link_flags);
  g_test_add_func ("/brush-thumbnail-native-popup/same-target-sync-reentry", brush_popup_same_target_sync_reentry);
  g_test_add_func ("/brush-thumbnail-native-popup/stock-dock-staging-policy", brush_popup_stock_dock_policy);
  g_test_add_func ("/brush-thumbnail-native-popup/private-target-release", brush_popup_private_target_release);
  g_test_add_func ("/brush-thumbnail-native-popup/noneditable-null", brush_popup_noneditable_and_null);
  g_test_add_func ("/brush-thumbnail-native-popup/independent-peers-repeat", brush_popup_peers_and_repeat);
  g_test_add_data_func ("/brush-thumbnail-native-popup/construction-cancel-toolbar", GINT_TO_POINTER (FALSE), brush_popup_construction_cancel);
  g_test_add_data_func ("/brush-thumbnail-native-popup/construction-cancel-body", GINT_TO_POINTER (TRUE), brush_popup_construction_cancel);
  g_test_add_data_func ("/brush-thumbnail-native-popup/reentry-retarget", GINT_TO_POINTER (FALSE), brush_popup_reentry_and_stale_control);
  g_test_add_data_func ("/brush-thumbnail-native-popup/reentry-destroy-toolbar", GINT_TO_POINTER (TRUE), brush_popup_reentry_and_stale_control);
  g_test_add_func ("/brush-thumbnail-native-popup/lower-controls-actual-input", brush_popup_lower_controls_actual_input);
}
