/* SPDX-License-Identifier: GPL-3.0-or-later */
/* No new provider symbols/headers are referenced here: this same test source
 * can be linked against the frozen baseline for behavioral red controls. */
#include "config.h"
#include <gegl.h>
#include <gtk/gtk.h>
#include "libgimpbase/gimpbase.h"
#include "libgimpwidgets/gimpwidgets.h"
#include "core/core-types.h"
#include "widgets/widgets-types.h"
#include "core/gimp.h"
#include "core/gimpcontext.h"
#include "core/gimpimage.h"
#include "core/gimpimage-undo.h"
#include "core/gimplayer.h"
#include "core/gimplayer-new.h"
#include "core/gimpfilterlayer.h"
#include "core/gimpgrouplayer.h"
#include "widgets/gimpcontainertreeview.h"
#include "widgets/gimpcontainertreestore.h"
#include "widgets/gimpcontainerview.h"
#include "widgets/gimpcellrendererpopup.h"
#include "widgets/gimpitemtreeview.h"
#include "widgets/gimplayertreeview.h"
#include "widgets/gimplayermodebox.h"
#include "widgets/gimpviewrenderer.h"
#include "widgets/gimpeditor.h"
#include "widgets/gimpdnd.h"
#include "widgets/gimpuimanager.h"
#include "menus/menus.h"
#include "painter/gimp-painter-binding.h"
#include "gimpcoreapp.h"
#include "gimp-app-test-utils.h"
#include "tests.h"
static Gimp *gimp;
typedef struct {
  GimpImage *image;
  GimpLayer *a, *b;
  GimpContext *context;
  GtkWidget *window, *tree;
  GimpContainerTreeView *view;
} Fixture;
static void drain (void) { gimp_test_run_mainloop_until_idle (); }
static GtkWidget *find (GtkWidget *root, const char *name) {
  if (!g_strcmp0 (gtk_widget_get_name (root), name)) return root;
  if (GTK_IS_CONTAINER (root)) {
    GList *children = gtk_container_get_children (GTK_CONTAINER (root));
    GtkWidget *result = NULL;
    for (GList *i = children; i && !result; i = i->next) result = find (i->data, name);
    g_list_free (children); return result;
  }
  return NULL;
}
static GimpLayer *layer (GimpImage *image, const char *name) {
  GimpLayer *result = gimp_layer_new (image, 16, 16, gimp_image_get_layer_format (image, TRUE),
                                     name, 1, GIMP_LAYER_MODE_NORMAL_LEGACY);
  g_assert_true (gimp_image_add_layer (image, result, NULL, 0, FALSE));
  return result;
}
static void fixture_init_direction (Fixture *f, GtkTextDirection direction) {
  gimp_set_focused_once (gimp);
  f->image = gimp_image_new (gimp, 16, 16, GIMP_RGB, GIMP_PRECISION_U8_NON_LINEAR);
  f->a = layer (f->image, "Clicked target A");
  f->b = layer (f->image, "Selected target B");
  f->context = gimp_context_new (gimp, "layer-tree-test", gimp_get_user_context (gimp));
  gimp_context_set_image (f->context, f->image);
  f->tree = gimp_item_tree_view_new (GIMP_TYPE_LAYER_TREE_VIEW, 32, 1, TRUE,
    f->image, menus_get_global_menu_factory (gimp), "<Layers>", "/layers-popup");
  g_assert_nonnull (f->tree); g_object_ref_sink (f->tree);
  f->view = GIMP_CONTAINER_TREE_VIEW (f->tree);
  /* Set direction before the first allocation, not between layout frames. */
  gtk_widget_set_direction (GTK_WIDGET (f->view->view), direction);
  gimp_container_view_set_context (GIMP_CONTAINER_VIEW (f->tree), f->context);
  f->window = gtk_window_new (GTK_WINDOW_TOPLEVEL); g_object_ref_sink (f->window);
  gtk_window_set_default_size (GTK_WINDOW (f->window), 400, 500);
  gtk_container_add (GTK_CONTAINER (f->window), f->tree);
  gtk_widget_show_all (f->window); drain ();
}
static void fixture_init (Fixture *f) {
  fixture_init_direction (f, GTK_TEXT_DIR_LTR);
}
static void fixture_clear (Fixture *f) {
  gtk_widget_destroy (f->window); g_object_unref (f->window); if (f->tree) g_object_unref (f->tree);
  g_object_unref (f->context); g_object_unref (f->image); drain ();
}
static GtkCellRenderer *popup_cell (Fixture *f) {
  GtkTreeViewColumn *column = gtk_tree_view_get_column (f->view->view, 0);
  GList *cells = gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (column));
  GtkCellRenderer *result = NULL;
  for (GList *i = cells; i; i = i->next)
    if (!g_strcmp0 (G_OBJECT_TYPE_NAME (i->data), "GimpCellRendererPopup")) result = i->data;
  g_list_free (cells); g_assert_nonnull (result); return result;
}
static GtkWidget *current_popup (GtkCellRenderer *cell) {
  GtkWidget *popup = NULL;
  g_object_get (cell, "popup", &popup, NULL); return popup;
}
static GtkWidget *retain_live_opacity (GtkWidget *popup) {
  GtkWidget *opacity = find (popup, "painter-tree-layer-opacity");
  g_assert_nonnull (opacity); g_object_ref (opacity);
  /* A retained but destroyed GtkRange has no adjustment. Detach this test
   * control so popup destruction leaves a live late-signal emitter. */
  GtkWidget *parent = gtk_widget_get_parent (opacity);
  g_assert_true (GTK_IS_CONTAINER (parent));
  gtk_container_remove (GTK_CONTAINER (parent), opacity);
  return opacity;
}
static GtkTreePath *path_for (Fixture *f, GimpLayer *layer) {
  GtkTreeIter iter; gboolean valid = gtk_tree_model_get_iter_first (f->view->model, &iter);
  while (valid) {
    GimpViewRenderer *r = gimp_container_tree_store_get_renderer (GIMP_CONTAINER_TREE_STORE (f->view->model), &iter);
    gboolean match = r->viewable == GIMP_VIEWABLE (layer); g_object_unref (r);
    if (match) return gtk_tree_model_get_path (f->view->model, &iter);
    valid = gtk_tree_model_iter_next (f->view->model, &iter);
  }
  g_assert_not_reached (); return NULL;
}
static void open_signal (Fixture *f, GimpLayer *layer) {
  GtkTreePath *path = path_for (f, layer); gchar *text = gtk_tree_path_to_string (path);
  GdkRectangle area;
  gtk_tree_view_get_cell_area (f->view->view, path,
                               gtk_tree_view_get_column (f->view->view, 0), &area);
  area.x = 0; area.width = gtk_widget_get_allocated_width (GTK_WIDGET (f->view->view));
  g_signal_emit_by_name (popup_cell (f), "parent-clicked", f->view->view, text, &area);
  gtk_tree_path_free (path); g_free (text);
}
static gboolean click_row (Fixture *f, GimpLayer *layer, GdkModifierType state) {
  GtkTreePath *path = path_for (f, layer);
  GtkTreeViewColumn *column = gtk_tree_view_get_column (f->view->view, 0);
  GdkRectangle area; gtk_tree_view_get_cell_area (f->view->view, path, column, &area);
  GdkEvent *event = gdk_event_new (GDK_BUTTON_PRESS);
  event->button.window = g_object_ref (gtk_tree_view_get_bin_window (f->view->view));
  event->button.send_event = TRUE; event->button.time = GDK_CURRENT_TIME;
  event->button.button = 1; event->button.state = state;
  event->button.x = area.x + area.width / 2; event->button.y = area.y + area.height / 2;
  /* Reject stale fixture geometry before testing native event dispatch. */
  GtkTreePath *hit_path = NULL; GtkTreeViewColumn *hit_column = NULL;
  g_assert_true (gtk_tree_view_get_path_at_pos (f->view->view,
    event->button.x, event->button.y, &hit_path, &hit_column, NULL, NULL));
  g_assert_true (hit_column == column);
  g_assert_cmpint (gtk_tree_path_compare (hit_path, path), ==, 0);
  gtk_tree_path_free (hit_path);
  gdk_event_set_device (event, gdk_seat_get_pointer (gdk_display_get_default_seat (gtk_widget_get_display (f->tree))));
  gboolean handled = gtk_widget_event (GTK_WIDGET (f->view->view), event);
  gdk_event_free (event); gtk_tree_path_free (path); return handled;
}
static void native_column (void) {
  Fixture f; fixture_init (&f); GtkCellRenderer *cell = popup_cell (&f);
  gint mode; g_object_get (cell, "mode", &mode, NULL);
  g_assert_cmpint (mode, ==, GTK_CELL_RENDERER_MODE_ACTIVATABLE);
  g_assert_cmpuint (g_signal_lookup ("parent-clicked", G_OBJECT_TYPE (cell)), !=, 0);
  g_assert_null (current_popup (cell));
  gboolean active = TRUE; g_object_get (cell, "active", &active, NULL); g_assert_false (active);
  g_object_set (cell, "active", TRUE, NULL);
  g_object_get (cell, "active", &active, NULL); g_assert_true (active);
  GtkCellRenderer *constructed = g_object_new (G_OBJECT_TYPE (cell), "active", TRUE, NULL);
  g_object_ref_sink (constructed); active = FALSE;
  g_object_get (constructed, "active", &active, NULL); g_assert_true (active);
  g_object_unref (constructed);
  fixture_clear (&f);
}
static GtkWidget *filter_dialog (void) {
  GList *windows = gtk_window_list_toplevels (); GtkWidget *result = NULL;
  for (GList *i = windows; i; i = i->next)
    if (find (i->data, "painter-filter-choice")) { result = i->data; break; }
  g_list_free (windows); return result;
}
static void direct_filter_button (void) {
  Fixture f; fixture_init (&f);
  GtkWidget *button = find (f.tree, "painter-tree-new-filter");
  g_assert_nonnull (button); g_assert_true (GTK_IS_BUTTON (button));
  GList *buttons = gtk_container_get_children (GTK_CONTAINER (gimp_editor_get_button_box (GIMP_EDITOR (f.tree))));
  g_assert_true (g_list_nth_data (buttons, 3) == button); g_list_free (buttons);
  g_assert_true (gtk_widget_is_sensitive (button));
  gtk_button_clicked (GTK_BUTTON (button));
  GtkWidget *dialog = filter_dialog (); g_assert_nonnull (dialog);
  g_assert_nonnull (find (dialog, "painter-filter-name"));
  gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL);
  fixture_clear (&f);
}
/* Native window counterpart of the old full-row GtkPopover anchor oracle.
 * Compute expected root placement from the row, not from popup coordinates. */
static void assert_native_area_popup (GtkWidget *popup, GtkWidget *widget,
                                      const GdkRectangle *area) {
  g_assert_true (GTK_IS_WINDOW (popup));
  g_assert_cmpint (gtk_window_get_window_type (GTK_WINDOW (popup)), ==, GTK_WINDOW_POPUP);
  g_assert_cmpstr (G_OBJECT_TYPE_NAME (popup), ==, "GimpPopover2");
  GtkAllocation allocation; gtk_widget_get_allocation (widget, &allocation);
  gint left, top; gdk_window_get_origin (gtk_widget_get_window (widget), &left, &top);
  if (!gtk_widget_get_has_window (widget)) { left += allocation.x; top += allocation.y; }
  left += area->x; top += area->y;
  GtkRequisition natural; gtk_widget_get_preferred_size (popup, NULL, &natural);
  GdkMonitor *monitor = gdk_display_get_monitor_at_point (gtk_widget_get_display (widget), left, top);
  GdkRectangle rect; gdk_monitor_get_geometry (monitor, &rect);
  const gint right = left + area->width, bottom = top + area->height;
  gint x = left, y = bottom;
  if (x + natural.width > rect.x + rect.width) x = right - natural.width;
  if (y + natural.height > rect.y + rect.height) y = top - natural.height;
  if (x < rect.x) x = rect.x;
  /* The old source's missing brace makes this above-first branch unconditional. */
  if (top - natural.height >= rect.y) y = top - natural.height;
  else if (bottom + natural.height < rect.y + rect.height) y = bottom;
  if (y < rect.y) {
    y = rect.y;
    if (right + natural.width < rect.x + rect.width) x = right;
    else if (left - natural.width >= rect.x) x = left - natural.width;
  }
  if (natural.width <= rect.width) x = CLAMP (x, rect.x, rect.x + rect.width - natural.width);
  if (natural.height <= rect.height) y = CLAMP (y, rect.y, rect.y + rect.height - natural.height);
  gint actual_x, actual_y; gtk_window_get_position (GTK_WINDOW (popup), &actual_x, &actual_y);
  g_assert_cmpint (actual_x, ==, x); g_assert_cmpint (actual_y, ==, y);
}
static void target_controls (void) {
  for (guint direction = 0; direction < 2; ++direction) {
    Fixture f; fixture_init_direction (&f, direction ? GTK_TEXT_DIR_RTL : GTK_TEXT_DIR_LTR);
    GtkCellRenderer *cell = popup_cell (&f);
    g_assert_true (click_row (&f, f.a, 0));
    GtkWidget *popup = current_popup (cell); g_assert_nonnull (popup);
    GdkRectangle anchor; GtkTreePath *target_path = path_for (&f, f.a);
    gtk_tree_view_get_cell_area (f.view->view, target_path, gtk_tree_view_get_column (f.view->view, 0), &anchor);
    gtk_tree_view_convert_bin_window_to_widget_coords (f.view->view, 0, anchor.y, NULL, &anchor.y);
    anchor.x = 0; anchor.width = gtk_widget_get_allocated_width (GTK_WIDGET (f.view->view));
    assert_native_area_popup (popup, GTK_WIDGET (f.view->view), &anchor); gtk_tree_path_free (target_path);
    GtkWidget *opacity = find (popup, "painter-tree-layer-opacity");
    GtkWidget *mode = find (popup, "painter-tree-layer-mode");
    GtkWidget *alpha = find (popup, "painter-tree-layer-lock-alpha");
    g_assert_nonnull (opacity); g_assert_nonnull (mode); g_assert_nonnull (alpha);
    gimp_container_view_set_1_selected (GIMP_CONTAINER_VIEW (f.tree), GIMP_VIEWABLE (f.b));
    gtk_range_set_value (GTK_RANGE (opacity), 37);
    g_assert_cmpfloat_with_epsilon (gimp_layer_get_opacity (f.a), .37, .000001);
    g_assert_cmpfloat (gimp_layer_get_opacity (f.b), ==, 1.0);
    gimp_layer_set_opacity (f.a, .62, FALSE);
    g_assert_cmpfloat_with_epsilon (gtk_range_get_value (GTK_RANGE (opacity)), 62, .000001);
    gimp_layer_mode_box_set_mode (GIMP_LAYER_MODE_BOX (mode), GIMP_LAYER_MODE_MULTIPLY_LEGACY);
    g_assert_cmpint (gimp_layer_get_mode (f.a), ==, GIMP_LAYER_MODE_MULTIPLY_LEGACY);
    g_assert_cmpint (gimp_layer_get_mode (f.b), ==, GIMP_LAYER_MODE_NORMAL_LEGACY);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (alpha), TRUE);
    g_assert_true (gimp_layer_get_lock_alpha (f.a)); g_assert_false (gimp_layer_get_lock_alpha (f.b));
    g_assert_true (gimp_image_undo (f.image));
    g_assert_false (gimp_layer_get_lock_alpha (f.a));
    g_assert_false (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (alpha)));
    g_assert_true (gimp_image_undo (f.image));
    g_assert_cmpint (gimp_layer_get_mode (f.a), ==, GIMP_LAYER_MODE_NORMAL_LEGACY);
    g_assert_cmpint (gimp_layer_mode_box_get_mode (GIMP_LAYER_MODE_BOX (mode)), ==, GIMP_LAYER_MODE_NORMAL_LEGACY);
    g_object_unref (popup); fixture_clear (&f);
  }
}
static void modifier_target (void) {
  Fixture f; fixture_init (&f);
  gimp_container_view_set_1_selected (GIMP_CONTAINER_VIEW (f.tree), GIMP_VIEWABLE (f.b));
  g_assert_true (click_row (&f, f.a, GDK_SHIFT_MASK));
  GtkWidget *popup = current_popup (popup_cell (&f)); g_assert_nonnull (popup);
  gtk_range_set_value (GTK_RANGE (find (popup, "painter-tree-layer-opacity")), 28);
  g_assert_cmpfloat_with_epsilon (gimp_layer_get_opacity (f.a), .28, .000001);
  g_assert_cmpfloat (gimp_layer_get_opacity (f.b), ==, 1);
  g_object_unref (popup); fixture_clear (&f);
}
static void retained_controls_owner_close (void) {
  Fixture f; fixture_init (&f); GtkCellRenderer *cell = popup_cell (&f); g_object_ref (cell);
  /* Signal marshalling needs a live GObject argument even after tree destroy.
   * Keep only the child widget alive; the owner still closes at destroy. */
  GtkWidget *child_view = g_object_ref (GTK_WIDGET (f.view->view));
  open_signal (&f, f.a); GtkWidget *popup = current_popup (cell); g_assert_nonnull (popup);
  GtkWidget *opacity = retain_live_opacity (popup);
  gpointer weak = cell; g_object_add_weak_pointer (G_OBJECT (cell), &weak);
  gtk_widget_destroy (f.tree);
  g_assert_null (current_popup (cell)); g_assert_false (gtk_widget_has_grab (popup));
  gtk_range_set_value (GTK_RANGE (opacity), 19);
  g_assert_cmpfloat (gimp_layer_get_opacity (f.a), ==, 1);
  GError *error = NULL; g_assert_true (gimp_painter_binding_close (G_OBJECT (cell), &error)); g_assert_no_error (error);
  g_signal_emit_by_name (cell, "parent-clicked", child_view, "0", NULL);
  g_assert_null (current_popup (cell));
  g_object_unref (child_view);
  g_object_unref (opacity); g_object_unref (popup); fixture_clear (&f);
  g_object_unref (cell); g_assert_null (weak);
}
static void image_switch_and_removed_target (void) {
  Fixture f; fixture_init (&f); GtkCellRenderer *cell = popup_cell (&f);
  open_signal (&f, f.a); GtkWidget *popup = current_popup (cell);
  GtkWidget *opacity = retain_live_opacity (popup);
  gimp_item_tree_view_set_image (GIMP_ITEM_TREE_VIEW (f.tree), NULL);
  g_assert_null (current_popup (cell));
  gtk_range_set_value (GTK_RANGE (opacity), 17); g_assert_cmpfloat (gimp_layer_get_opacity (f.a), ==, 1);
  g_object_unref (opacity); g_object_unref (popup);
  gimp_item_tree_view_set_image (GIMP_ITEM_TREE_VIEW (f.tree), f.image);
  open_signal (&f, f.a); popup = current_popup (cell); g_assert_nonnull (popup);
  opacity = retain_live_opacity (popup); g_object_ref (f.a);
  gimp_image_remove_layer (f.image, f.a, FALSE, NULL);
  g_assert_null (current_popup (cell)); gtk_range_set_value (GTK_RANGE (opacity), 12);
  g_assert_cmpfloat (gimp_layer_get_opacity (f.a), ==, 1);
  g_object_unref (f.a); g_object_unref (opacity); g_object_unref (popup); fixture_clear (&f);
}
static void reopen_b (GtkWidget *widget, gpointer data) { (void) widget; Fixture *f = data; open_signal (f, f->b); }
static void newer_popup_wins (void) {
  Fixture f; fixture_init (&f); GtkCellRenderer *cell = popup_cell (&f);
  open_signal (&f, f.a); GtkWidget *old = current_popup (cell);
  g_signal_connect (old, "destroy", G_CALLBACK (reopen_b), &f);
  open_signal (&f, f.a);
  GtkWidget *newer = current_popup (cell); g_assert_nonnull (newer); g_assert_true (newer != old);
  gtk_range_set_value (GTK_RANGE (find (newer, "painter-tree-layer-opacity")), 44);
  g_assert_cmpfloat (gimp_layer_get_opacity (f.a), ==, 1);
  g_assert_cmpfloat_with_epsilon (gimp_layer_get_opacity (f.b), .44, .000001);
  g_object_unref (old); g_object_unref (newer); fixture_clear (&f);
}
static void reorder_selection (GimpImage *image, gpointer data) {
  Fixture *f = data;
  g_signal_handlers_disconnect_by_func (image, reorder_selection, data);
  g_assert_true (gimp_image_reorder_item (image, GIMP_ITEM (f->a), NULL, 0, FALSE, NULL));
}
static void selection_reorders_clicked_row (void) {
  Fixture f; fixture_init (&f);
  gimp_container_view_set_1_selected (GIMP_CONTAINER_VIEW (f.tree), GIMP_VIEWABLE (f.b));
  g_signal_connect (f.image, "selected-layers-changed", G_CALLBACK (reorder_selection), &f);
  click_row (&f, f.a, 0); GtkWidget *popup = current_popup (popup_cell (&f)); g_assert_nonnull (popup);
  gtk_range_set_value (GTK_RANGE (find (popup, "painter-tree-layer-opacity")), 53);
  g_assert_cmpfloat_with_epsilon (gimp_layer_get_opacity (f.a), .53, .000001);
  g_assert_cmpfloat (gimp_layer_get_opacity (f.b), ==, 1);
  g_object_unref (popup); fixture_clear (&f);
}
static void filter_editor_clicked_target (void) {
  Fixture f; fixture_init (&f);
  GimpLayer *filter = gimp_filter_layer_new (f.image, 16, 16, "Filter popup target", 1, GIMP_LAYER_MODE_NORMAL_LEGACY);
  g_assert_true (gimp_image_add_layer (f.image, filter, NULL, 0, FALSE)); drain ();
  open_signal (&f, filter); GtkWidget *popup = current_popup (popup_cell (&f)); g_assert_nonnull (popup);
  g_assert_true (GTK_IS_WINDOW (popup));
  g_assert_cmpint (gtk_window_get_window_type (GTK_WINDOW (popup)), ==, GTK_WINDOW_POPUP);
  g_assert_null (find (popup, "painter-tree-edit-filter"));
  g_assert_null (find (popup, "painter-filter-name"));
  g_assert_nonnull (find (popup, "painter-filter-definition"));
  gimp_container_view_set_1_selected (GIMP_CONTAINER_VIEW (f.tree), GIMP_VIEWABLE (f.b));
  GtkWidget *still_open = current_popup (popup_cell (&f));
  g_assert_true (still_open == popup);
  g_object_unref (still_open);
  gtk_range_set_value (GTK_RANGE (find (popup, "painter-tree-layer-opacity")), 37);
  g_assert_cmpfloat_with_epsilon (gimp_layer_get_opacity (filter), .37, .000001);
  g_assert_cmpfloat (gimp_layer_get_opacity (f.b), ==, 1);
  gtk_dialog_response (GTK_DIALOG (popup), GTK_RESPONSE_CANCEL);
  g_assert_null (current_popup (popup_cell (&f)));
  g_object_unref (popup); fixture_clear (&f);
}
static void switch_during_refresh (GtkRange *range, gpointer data) {
  Fixture *f = data;
  g_signal_handlers_disconnect_by_func (range, switch_during_refresh, data);
  open_signal (f, f->b);
}
static void refresh_reentry_initializes_new_target (void) {
  Fixture f; fixture_init (&f); GtkCellRenderer *cell = popup_cell (&f);
  gimp_layer_set_opacity (f.b, .74, FALSE);
  gimp_layer_set_mode (f.b, GIMP_LAYER_MODE_MULTIPLY_LEGACY, FALSE);
  gimp_layer_set_lock_alpha (f.b, TRUE, FALSE);
  open_signal (&f, f.a); GtkWidget *old = current_popup (cell);
  GtkWidget *opacity = find (old, "painter-tree-layer-opacity");
  g_signal_connect (opacity, "value-changed", G_CALLBACK (switch_during_refresh), &f);
  gimp_layer_set_opacity (f.a, .52, FALSE);
  GtkWidget *newer = current_popup (cell); g_assert_nonnull (newer); g_assert_true (newer != old);
  g_assert_cmpfloat_with_epsilon (gtk_range_get_value (GTK_RANGE (find (newer, "painter-tree-layer-opacity"))), 74, .000001);
  g_assert_cmpint (gimp_layer_mode_box_get_mode (GIMP_LAYER_MODE_BOX (find (newer, "painter-tree-layer-mode"))), ==, GIMP_LAYER_MODE_MULTIPLY_LEGACY);
  g_assert_true (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (find (newer, "painter-tree-layer-lock-alpha"))));
  g_object_unref (old); g_object_unref (newer); fixture_clear (&f);
}
static void newer_selection_popup (GimpImage *image, gpointer data) {
  Fixture *f = data;
  g_signal_handlers_disconnect_by_func (image, newer_selection_popup, data);
  gimp_container_view_set_1_selected (GIMP_CONTAINER_VIEW (f->tree), GIMP_VIEWABLE (f->b));
  open_signal (f, f->b);
}
static void selection_newer_popup_wins (void) {
  Fixture f; fixture_init (&f);
  gimp_container_view_set_1_selected (GIMP_CONTAINER_VIEW (f.tree), GIMP_VIEWABLE (f.b));
  g_signal_connect (f.image, "selected-layers-changed", G_CALLBACK (newer_selection_popup), &f);
  click_row (&f, f.a, 0);
  GtkWidget *popup = current_popup (popup_cell (&f)); g_assert_nonnull (popup);
  gtk_range_set_value (GTK_RANGE (find (popup, "painter-tree-layer-opacity")), 47);
  g_assert_cmpfloat_with_epsilon (gimp_layer_get_opacity (f.b), .47, .000001);
  g_assert_cmpfloat (gimp_layer_get_opacity (f.a), ==, 1);
  g_object_unref (popup); fixture_clear (&f);
}
static void remove_selected_target (GimpImage *image, gpointer data) {
  Fixture *f = data;
  g_signal_handlers_disconnect_by_func (image, remove_selected_target, data);
  gimp_image_remove_layer (image, f->a, FALSE, NULL);
}
static void selection_removes_target (void) {
  Fixture f; fixture_init (&f); GtkCellRenderer *cell = popup_cell (&f);
  gimp_container_view_set_1_selected (GIMP_CONTAINER_VIEW (f.tree), GIMP_VIEWABLE (f.b));
  g_signal_connect (f.image, "selected-layers-changed", G_CALLBACK (remove_selected_target), &f);
  click_row (&f, f.a, 0); g_assert_null (current_popup (cell));
  fixture_clear (&f);
}
static void destroy_selected_owner (GimpImage *image, gpointer data) {
  Fixture *f = data;
  g_signal_handlers_disconnect_by_func (image, destroy_selected_owner, data);
  gtk_widget_destroy (f->tree);
}
static void selection_destroys_owner (void) {
  Fixture f; fixture_init (&f); GtkCellRenderer *cell = popup_cell (&f); g_object_ref (cell);
  gimp_container_view_set_1_selected (GIMP_CONTAINER_VIEW (f.tree), GIMP_VIEWABLE (f.b));
  g_signal_connect (f.image, "selected-layers-changed", G_CALLBACK (destroy_selected_owner), &f);
  click_row (&f, f.a, 0); g_assert_null (current_popup (cell));
  fixture_clear (&f); g_object_unref (cell);
}
static void repeat_popdown_disconnects_old_controls (void) {
  Fixture f; fixture_init (&f); GtkCellRenderer *cell = popup_cell (&f);
  for (guint i = 0; i < 3; ++i) {
    open_signal (&f, f.a); GtkWidget *popup = current_popup (cell); g_assert_nonnull (popup);
    GtkWidget *opacity = retain_live_opacity (popup);
    g_signal_emit_by_name (popup, "cancel"); drain ();
    g_assert_null (current_popup (cell)); g_assert_false (gtk_widget_has_grab (popup));
    gtk_range_set_value (GTK_RANGE (opacity), 31 + i);
    g_assert_cmpfloat (gimp_layer_get_opacity (f.a), ==, 1);
    g_object_unref (opacity); g_object_unref (popup);
  }
  fixture_clear (&f);
}
static void signal_reopen_b (GtkCellRenderer *cell, GObject *widget, const char *path,
                             GdkRectangle *area, gpointer data) {
  Fixture *f = data;
  (void) widget; (void) path; (void) area;
  g_signal_handlers_disconnect_by_func (cell, signal_reopen_b, data);
  GtkWidget *initial = current_popup (cell); g_assert_nonnull (initial); g_object_unref (initial);
  open_signal (f, f->b);
}
static void signal_observer_newer_popup_wins (void) {
  Fixture f; fixture_init (&f); GtkCellRenderer *cell = popup_cell (&f);
  g_signal_connect (cell, "parent-clicked", G_CALLBACK (signal_reopen_b), &f);
  click_row (&f, f.a, 0);
  GtkWidget *popup = current_popup (cell); g_assert_nonnull (popup);
  gtk_range_set_value (GTK_RANGE (find (popup, "painter-tree-layer-opacity")), 23);
  g_assert_cmpfloat_with_epsilon (gimp_layer_get_opacity (f.b), .23, .000001);
  g_assert_cmpfloat (gimp_layer_get_opacity (f.a), ==, 1);
  g_object_unref (popup); fixture_clear (&f);
}
static void signal_reorder_a (GtkCellRenderer *cell, GObject *widget, const char *path,
                              GdkRectangle *area, gpointer data) {
  Fixture *f = data;
  (void) widget; (void) path; (void) area;
  g_signal_handlers_disconnect_by_func (cell, signal_reorder_a, data);
  g_assert_true (gimp_image_reorder_item (f->image, GIMP_ITEM (f->a), NULL, 0, FALSE, NULL));
}
static void signal_observer_preserves_clicked_target (void) {
  Fixture f; fixture_init (&f); GtkCellRenderer *cell = popup_cell (&f);
  g_signal_connect (cell, "parent-clicked", G_CALLBACK (signal_reorder_a), &f);
  click_row (&f, f.a, 0);
  GtkWidget *popup = current_popup (cell); g_assert_nonnull (popup);
  gtk_range_set_value (GTK_RANGE (find (popup, "painter-tree-layer-opacity")), 21);
  g_assert_cmpfloat_with_epsilon (gimp_layer_get_opacity (f.a), .21, .000001);
  g_assert_cmpfloat (gimp_layer_get_opacity (f.b), ==, 1);
  g_object_unref (popup); fixture_clear (&f);
}
static void image_switch_reentrant_old_popup (void) {
  Fixture f; fixture_init (&f); GtkCellRenderer *cell = popup_cell (&f);
  open_signal (&f, f.a); GtkWidget *old = current_popup (cell); g_assert_nonnull (old);
  g_signal_connect (old, "destroy", G_CALLBACK (reopen_b), &f);
  gimp_item_tree_view_set_image (GIMP_ITEM_TREE_VIEW (f.tree), NULL);
  g_assert_null (gimp_item_tree_view_get_image (GIMP_ITEM_TREE_VIEW (f.tree)));
  g_assert_null (current_popup (cell)); g_assert_false (gtk_widget_has_grab (old));
  g_object_unref (old); fixture_clear (&f);
}
static void ensure_horizontal_room (Fixture *f, GtkAdjustment *horizontal, gdouble room) {
  const gint64 deadline = g_get_monotonic_time () + 2 * G_TIME_SPAN_SECOND;
  /* check_resize dispatches pending container allocation synchronously when
   * possible. A toplevel configure can still require the actual main loop. */
  gtk_container_check_resize (GTK_CONTAINER (f->window));
  while (gtk_adjustment_get_upper (horizontal) - gtk_adjustment_get_page_size (horizontal) < room &&
         g_get_monotonic_time () < deadline)
    gimp_test_run_temp_mainloop (10);
  g_assert_cmpfloat (gtk_adjustment_get_upper (horizontal) - gtk_adjustment_get_page_size (horizontal), >=, room);
}
static void scrolled_full_row_geometry (void) {
  for (guint direction = 0; direction < 2; ++direction) {
    Fixture f; fixture_init_direction (&f, direction ? GTK_TEXT_DIR_RTL : GTK_TEXT_DIR_LTR);
    GtkWidget *view = GTK_WIDGET (f.view->view);
    gtk_tree_view_column_set_sizing (f.view->main_column, GTK_TREE_VIEW_COLUMN_FIXED);
    gtk_tree_view_column_set_fixed_width (f.view->main_column, 1200);
    GtkAdjustment *horizontal = gtk_scrollable_get_hadjustment (GTK_SCROLLABLE (view));
    ensure_horizontal_room (&f, horizontal, 80);
    gtk_adjustment_set_value (horizontal, 80); drain ();
    g_assert_cmpfloat (gtk_adjustment_get_value (horizontal), >=, 80);
    open_signal (&f, f.a); GtkWidget *popup = current_popup (popup_cell (&f)); g_assert_nonnull (popup);
    GdkRectangle anchor, cell_area; GtkTreePath *path = path_for (&f, f.a); gint expected_y;
    gtk_tree_view_get_cell_area (f.view->view, path, gtk_tree_view_get_column (f.view->view, 0), &cell_area);
    gtk_tree_view_convert_bin_window_to_widget_coords (f.view->view, 0, cell_area.y, NULL, &expected_y);
    anchor = (GdkRectangle) {0, expected_y, gtk_widget_get_allocated_width (view), cell_area.height};
    assert_native_area_popup (popup, view, &anchor);
    gtk_tree_path_free (path); g_object_unref (popup); fixture_clear (&f);
  }
}
static void group_pass_through_context (void) {
  Fixture f; fixture_init (&f);
  GimpLayer *group = gimp_group_layer_new (f.image);
  g_assert_true (gimp_image_add_layer (f.image, group, NULL, 0, FALSE));
  gimp_layer_set_mode (group, GIMP_LAYER_MODE_PASS_THROUGH, FALSE); drain ();
  open_signal (&f, group); GtkWidget *popup = current_popup (popup_cell (&f)); g_assert_nonnull (popup);
  GtkWidget *mode = find (popup, "painter-tree-layer-mode");
  GtkWidget *alpha = find (popup, "painter-tree-layer-lock-alpha");
  g_assert_cmpint (gimp_layer_mode_box_get_mode (GIMP_LAYER_MODE_BOX (mode)), ==, GIMP_LAYER_MODE_PASS_THROUGH);
  g_assert_cmpint (gtk_widget_get_sensitive (alpha), ==, gimp_layer_can_lock_alpha (group));
  gimp_layer_mode_box_set_mode (GIMP_LAYER_MODE_BOX (mode), GIMP_LAYER_MODE_NORMAL);
  g_assert_cmpint (gimp_layer_get_mode (group), ==, GIMP_LAYER_MODE_NORMAL);
  g_assert_true (gimp_image_undo (f.image));
  g_assert_cmpint (gimp_layer_get_mode (group), ==, GIMP_LAYER_MODE_PASS_THROUGH);
  g_assert_cmpint (gimp_layer_mode_box_get_mode (GIMP_LAYER_MODE_BOX (mode)), ==, GIMP_LAYER_MODE_PASS_THROUGH);
  g_object_unref (popup); fixture_clear (&f);
}
static void edit_again_during_refresh (GtkRange *range, gpointer data) {
  Fixture *f = data;
  g_signal_handlers_disconnect_by_func (range, edit_again_during_refresh, data);
  gimp_layer_set_opacity (f->a, .80, FALSE);
}
static void same_target_refresh_keeps_latest_model (void) {
  Fixture f; fixture_init (&f); GtkCellRenderer *cell = popup_cell (&f);
  open_signal (&f, f.a); GtkWidget *popup = current_popup (cell);
  GtkWidget *opacity = find (popup, "painter-tree-layer-opacity");
  g_signal_connect (opacity, "value-changed", G_CALLBACK (edit_again_during_refresh), &f);
  gimp_layer_set_opacity (f.a, .50, FALSE); drain ();
  g_assert_cmpfloat_with_epsilon (gimp_layer_get_opacity (f.a), .80, .000001);
  g_assert_cmpfloat_with_epsilon (gtk_range_get_value (GTK_RANGE (opacity)), 80, .000001);
  g_object_unref (popup); fixture_clear (&f);
}
static void remove_hit_column (GimpImage *image, gpointer data) {
  Fixture *f = data;
  g_signal_handlers_disconnect_by_func (image, remove_hit_column, data);
  gtk_tree_view_remove_column (f->view->view, gtk_tree_view_get_column (f->view->view, 0));
}
static void selection_removes_column (void) {
  Fixture f; fixture_init (&f); GtkCellRenderer *cell = popup_cell (&f); g_object_ref (cell);
  gimp_container_view_set_1_selected (GIMP_CONTAINER_VIEW (f.tree), GIMP_VIEWABLE (f.b));
  g_signal_connect (f.image, "selected-layers-changed", G_CALLBACK (remove_hit_column), &f);
  click_row (&f, f.a, 0); g_assert_null (current_popup (cell));
  fixture_clear (&f); g_object_unref (cell);
}
static void select_b_only (GimpImage *image, gpointer data) {
  Fixture *f = data;
  g_signal_handlers_disconnect_by_func (image, select_b_only, data);
  gimp_container_view_set_1_selected (GIMP_CONTAINER_VIEW (f->tree), GIMP_VIEWABLE (f->b));
}
static void selection_only_supersedes_activation (void) {
  Fixture f; fixture_init (&f); GtkCellRenderer *cell = popup_cell (&f);
  gimp_container_view_set_1_selected (GIMP_CONTAINER_VIEW (f.tree), GIMP_VIEWABLE (f.b));
  g_signal_connect (f.image, "selected-layers-changed", G_CALLBACK (select_b_only), &f);
  click_row (&f, f.a, 0); g_assert_null (current_popup (cell));
  g_assert_true (gimp_container_view_get_1_selected (GIMP_CONTAINER_VIEW (f.tree)) == GIMP_VIEWABLE (f.b));
  fixture_clear (&f);
}
typedef struct { Fixture *fixture; guint calls; } FocusChange;
static void focus_drops_owner (GtkWidget *view, gpointer data) {
  FocusChange *change = data; Fixture *f = change->fixture;
  g_signal_handlers_disconnect_by_func (view, focus_drops_owner, data); ++change->calls;
  g_assert_nonnull (f->tree);
  GtkWidget *tree = f->tree; f->tree = NULL;
  gtk_widget_destroy (tree); g_object_unref (tree); f->view = NULL;
}
static void focus_reentry_closes_owner (void) {
  Fixture f; fixture_init (&f); GtkCellRenderer *cell = popup_cell (&f); g_object_ref (cell);
  FocusChange change = { &f, 0 };
  gtk_window_set_focus (GTK_WINDOW (f.window), NULL); drain ();
  g_assert_false (gtk_widget_has_focus (GTK_WIDGET (f.view->view)));
  /* Run inside the native grab-focus action after its default closure.
   * Actual has-focus depends on window-manager activation under Xvfb. */
  g_signal_connect_after (f.view->view, "grab-focus", G_CALLBACK (focus_drops_owner), &change);
  click_row (&f, f.a, 0);
  g_assert_cmpuint (change.calls, ==, 1);
  g_assert_null (f.tree); g_assert_null (current_popup (cell));
  fixture_clear (&f); g_object_unref (cell);
}
typedef struct { Fixture *fixture; gboolean remove; guint calls; } ApplyChange;
static void change_after_attributes (GtkCellArea *area, GtkTreeModel *model, GtkTreeIter *iter,
                                     gboolean expander, gboolean expanded, gpointer data) {
  ApplyChange *change = data; Fixture *f = change->fixture;
  (void) model; (void) iter; (void) expander; (void) expanded;
  g_signal_handlers_disconnect_by_func (area, change_after_attributes, data); ++change->calls;
  if (change->remove)
    gtk_tree_view_remove_column (f->view->view, gtk_tree_view_get_column (f->view->view, 0));
  else
    gimp_container_view_set_1_selected (GIMP_CONTAINER_VIEW (f->tree), GIMP_VIEWABLE (f->b));
}
static void arm_attribute_change (GimpImage *image, gpointer data) {
  ApplyChange *change = data; Fixture *f = change->fixture;
  g_signal_handlers_disconnect_by_func (image, arm_attribute_change, data);
  GtkTreeViewColumn *column = gtk_tree_view_get_column (f->view->view, 0);
  GtkCellArea *area = gtk_cell_layout_get_area (GTK_CELL_LAYOUT (column));
  g_signal_connect_after (area, "apply-attributes", G_CALLBACK (change_after_attributes), data);
}
static void attributes_supersede_activation (void) {
  for (guint remove = 0; remove < 2; ++remove) {
    Fixture f; fixture_init (&f); GtkCellRenderer *cell = popup_cell (&f); g_object_ref (cell);
    ApplyChange change = { &f, remove, 0 };
    gimp_container_view_set_1_selected (GIMP_CONTAINER_VIEW (f.tree), GIMP_VIEWABLE (f.b));
    g_signal_connect (f.image, "selected-layers-changed", G_CALLBACK (arm_attribute_change), &change);
    click_row (&f, f.a, 0);
    g_assert_cmpuint (change.calls, ==, 1); g_assert_null (current_popup (cell));
    fixture_clear (&f); g_object_unref (cell);
  }
}
typedef struct { Fixture *fixture; guint mode, calls; } EarlyChange;
static void first_attribute_change (GtkCellArea *area, GtkTreeModel *model, GtkTreeIter *iter,
                                    gboolean expander, gboolean expanded, gpointer data) {
  EarlyChange *change = data; Fixture *f = change->fixture;
  (void) model; (void) iter; (void) expander; (void) expanded;
  g_signal_handlers_disconnect_by_func (area, first_attribute_change, data); ++change->calls;
  if (change->mode == 0)
    g_assert_true (gimp_image_reorder_item (f->image, GIMP_ITEM (f->a), NULL, 0, FALSE, NULL));
  else if (change->mode == 1)
    gimp_image_remove_layer (f->image, f->a, FALSE, NULL);
  else if (change->mode == 2)
    open_signal (f, f->b);
  else if (change->mode == 3)
    gtk_widget_destroy (f->tree);
  else
    gtk_tree_view_remove_column (f->view->view, gtk_tree_view_get_column (f->view->view, 0));
}
static void first_attributes_preserve_request (void) {
  for (guint mode = 0; mode < 5; ++mode) {
    Fixture f; fixture_init (&f); GtkCellRenderer *cell = popup_cell (&f); g_object_ref (cell);
    EarlyChange change = { &f, mode, 0 };
    GtkTreeViewColumn *column = gtk_tree_view_get_column (f.view->view, 0);
    GtkCellArea *area = gtk_cell_layout_get_area (GTK_CELL_LAYOUT (column));
    g_signal_connect_after (area, "apply-attributes", G_CALLBACK (first_attribute_change), &change);
    click_row (&f, f.a, 0); g_assert_cmpuint (change.calls, ==, 1);
    GtkWidget *popup = current_popup (cell);
    if (mode == 0 || mode == 2) {
      g_assert_nonnull (popup);
      gtk_range_set_value (GTK_RANGE (find (popup, "painter-tree-layer-opacity")), 26);
      GimpLayer *target = mode == 0 ? f.a : f.b;
      GimpLayer *other = mode == 0 ? f.b : f.a;
      g_assert_cmpfloat_with_epsilon (gimp_layer_get_opacity (target), .26, .000001);
      g_assert_cmpfloat (gimp_layer_get_opacity (other), ==, 1);
      g_object_unref (popup);
    } else g_assert_null (popup);
    fixture_clear (&f); g_object_unref (cell);
  }
}
static GimpViewable *drag_viewable (GtkWidget *view) {
  /* This tree registers list DnD first, so the generic get_drag_viewable helper
   * selects that list callback. Exercise the actual scalar cache consumer. */
  GimpDndDragViewableFunc get = (GimpDndDragViewableFunc)
    g_object_get_data (G_OBJECT (view), "gimp-dnd-get-layer-func");
  gpointer data = g_object_get_data (G_OBJECT (view), "gimp-dnd-get-layer-data");
  GimpContext *context = NULL;
  g_assert_nonnull (get);
  return get (view, &context, data);
}
static GdkPixbuf *drag_pixbuf (GtkWidget *view) {
  GimpDndDragPixbufFunc get = (GimpDndDragPixbufFunc)
    g_object_get_data (G_OBJECT (view), "gimp-dnd-get-pixbuf-func");
  gpointer data = g_object_get_data (G_OBJECT (view), "gimp-dnd-get-pixbuf-data");
  g_assert_nonnull (get);
  return get (view, data);
}
static void removed_row_invalidates_drag_cache (void) {
  for (guint reentrant = 0; reentrant < 2; ++reentrant) {
    Fixture f; fixture_init (&f);
    GtkWidget *view = GTK_WIDGET (f.view->view);
    if (reentrant) {
      EarlyChange change = { &f, 1, 0 };
      GtkTreeViewColumn *column = gtk_tree_view_get_column (f.view->view, 0);
      GtkCellArea *area = gtk_cell_layout_get_area (GTK_CELL_LAYOUT (column));
      g_signal_connect_after (area, "apply-attributes", G_CALLBACK (first_attribute_change), &change);
      click_row (&f, f.a, 0); g_assert_cmpuint (change.calls, ==, 1);
    } else {
      click_row (&f, f.a, 0);
      g_assert_true (drag_viewable (view) == GIMP_VIEWABLE (f.a));
      GdkPixbuf *preview = drag_pixbuf (view); g_assert_nonnull (preview); g_object_unref (preview);
      gimp_image_remove_layer (f.image, f.a, FALSE, NULL);
    }
    /* Both actual native callback paths must reject the retired row. The
     * old cache became dangling after the activation's renderer lease ended. */
    g_assert_null (drag_viewable (view));
    g_assert_null (drag_pixbuf (view));
    fixture_clear (&f);
  }
}

/* These are calls compiled as C11 against the original native entry points.
 * They complement, rather than replace, the actual pointer-event tests above. */
typedef struct {
  GObject *widget;
  const gchar *path;
  GdkRectangle *area;
  guint calls;
  gboolean expect_popup;
} PopupDelivery;

static void record_popup_delivery (GimpCellRendererPopup *cell, GObject *widget,
                                   const gchar *path, GdkRectangle *area,
                                   gpointer data) {
  PopupDelivery *delivery = data;
  g_assert_true (widget == delivery->widget);
  g_assert_cmpstr (path, ==, delivery->path);
  g_assert_true (area == delivery->area);
  g_assert_cmpint (gimp_cell_renderer_popup_get_popup (cell) != NULL, ==,
                   delivery->expect_popup);
  ++delivery->calls;
}

static gchar *popup_path_text (Fixture *f, GimpLayer *target) {
  GtkTreePath *path = path_for (f, target);
  gchar *text = gtk_tree_path_to_string (path);
  gtk_tree_path_free (path);
  return text;
}

static void public_factory_floating_column_ownership (void) {
  /* A raw factory result is floating and can be transferred straight into
   * the native GtkCellLayout. The factory must not sink this reference. */
  GtkCellRenderer *(*factory) (void) = gimp_cell_renderer_popup_new;
  GtkCellRenderer *cell = factory ();
  gpointer weak = cell;
  g_object_add_weak_pointer (G_OBJECT (cell), &weak);
  g_assert_true (g_object_is_floating (cell));
  g_assert_true (G_OBJECT_TYPE (cell) == gimp_cell_renderer_popup_get_type ());
  g_assert_true (g_type_parent (G_OBJECT_TYPE (cell)) == GTK_TYPE_CELL_RENDERER_PIXBUF);
  GtkTreeViewColumn *column = gtk_tree_view_column_new ();
  g_object_ref_sink (column);
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (column), cell, FALSE);
  g_assert_false (g_object_is_floating (cell));
  g_object_unref (column);
  g_assert_null (weak);

  /* Both a separately sunk factory reference and a retained column-owned
   * reference survive either release order without stealing column ownership. */
  for (guint retain_column_owned = 0; retain_column_owned < 2; ++retain_column_owned)
    for (guint release_column_first = 0; release_column_first < 2; ++release_column_first) {
      cell = factory (); weak = cell;
      g_object_add_weak_pointer (G_OBJECT (cell), &weak);
      column = gtk_tree_view_column_new (); g_object_ref_sink (column);
      if (!retain_column_owned) g_object_ref_sink (cell);
      gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (column), cell, FALSE);
      if (retain_column_owned) g_object_ref (cell);
      g_assert_false (g_object_is_floating (cell));
      if (release_column_first) {
        g_object_unref (column); g_assert_nonnull (weak);
        g_object_unref (cell);
      } else {
        g_object_unref (cell); g_assert_nonnull (weak);
        g_object_unref (column);
      }
      g_assert_null (weak);
    }
}

static void public_clicked_active_property_independent (void) {
  Fixture f; fixture_init (&f);
  GimpCellRendererPopup *cell = GIMP_CELL_RENDERER_POPUP (popup_cell (&f));
  gchar *path = popup_path_text (&f, f.a);
  GdkRectangle area = { 0, 0, 100, 24 };
  PopupDelivery delivery = { G_OBJECT (f.view->view), path, &area, 0, TRUE };
  g_signal_connect (cell, "parent-clicked", G_CALLBACK (record_popup_delivery), &delivery);
  gboolean active = TRUE;
  g_object_get (cell, "active", &active, NULL); g_assert_false (active);
  for (guint value = 0; value < 2; ++value) {
    g_object_set (cell, "active", (gboolean) value, NULL);
    const guint64 generation = gimp_cell_renderer_popup_get_generation (cell);
    gimp_cell_renderer_popup_clicked (cell, delivery.widget, path, &area);
    g_assert_cmpuint (delivery.calls, ==, value + 1);
    g_assert_cmpuint (gimp_cell_renderer_popup_get_generation (cell), >, generation);
    GtkWidget *popup = current_popup (GTK_CELL_RENDERER (cell));
    g_assert_nonnull (popup);
    gtk_range_set_value (GTK_RANGE (find (popup, "painter-tree-layer-opacity")), 35 + 20 * value);
    g_assert_cmpfloat_with_epsilon (gimp_layer_get_opacity (f.a), .35 + .20 * value, .000001);
    g_assert_cmpfloat (gimp_layer_get_opacity (f.b), ==, 1);
    g_object_unref (popup);
  }
  g_signal_handlers_disconnect_by_data (cell, &delivery);
  g_free (path); fixture_clear (&f);
}

static void public_clicked_unbound_observers (void) {
  Fixture f; fixture_init (&f);
  GimpCellRendererPopup *cell = GIMP_CELL_RENDERER_POPUP (gimp_cell_renderer_popup_new ());
  g_object_ref_sink (cell);
  gchar *path = popup_path_text (&f, f.a);
  GdkRectangle area = { 3, 5, 100, 24 };
  PopupDelivery delivery = { G_OBJECT (f.view->view), path, &area, 0, FALSE };
  g_signal_connect (cell, "parent-clicked", G_CALLBACK (record_popup_delivery), &delivery);
  g_signal_connect_after (cell, "parent-clicked", G_CALLBACK (record_popup_delivery), &delivery);
  const guint64 generation = gimp_cell_renderer_popup_get_generation (cell);
  gimp_cell_renderer_popup_clicked (cell, delivery.widget, path, &area);
  g_assert_cmpuint (delivery.calls, ==, 2);
  g_assert_cmpuint (gimp_cell_renderer_popup_get_generation (cell), ==, generation);
  g_object_unref (cell); g_free (path); fixture_clear (&f);
}

static void public_clicked_invalid_row_observers (void) {
  Fixture f; fixture_init (&f);
  GimpCellRendererPopup *cell = GIMP_CELL_RENDERER_POPUP (popup_cell (&f));
  GdkRectangle area = { 3, 5, 100, 24 };
  PopupDelivery delivery = { G_OBJECT (f.view->view), "999999", &area, 0, FALSE };
  g_signal_connect (cell, "parent-clicked", G_CALLBACK (record_popup_delivery), &delivery);
  const guint64 generation = gimp_cell_renderer_popup_get_generation (cell);
  gimp_cell_renderer_popup_clicked (cell, delivery.widget, delivery.path, &area);
  g_assert_cmpuint (delivery.calls, ==, 1);
  g_assert_cmpuint (gimp_cell_renderer_popup_get_generation (cell), ==, generation);
  g_signal_handlers_disconnect_by_data (cell, &delivery);
  open_signal (&f, f.a);
  GtkWidget *popup = current_popup (GTK_CELL_RENDERER (cell)); g_assert_nonnull (popup);
  delivery.expect_popup = TRUE;
  g_signal_connect (cell, "parent-clicked", G_CALLBACK (record_popup_delivery), &delivery);
  const guint64 open_generation = gimp_cell_renderer_popup_get_generation (cell);
  gimp_cell_renderer_popup_clicked (cell, delivery.widget, delivery.path, &area);
  g_assert_cmpuint (delivery.calls, ==, 2);
  g_assert_true (gimp_cell_renderer_popup_get_popup (cell) == popup);
  g_assert_cmpuint (gimp_cell_renderer_popup_get_generation (cell), ==, open_generation);
  g_signal_handlers_disconnect_by_data (cell, &delivery);
  g_object_unref (popup); fixture_clear (&f);
}

static void public_clicked_null_area (void) {
  Fixture f; fixture_init (&f);
  GimpCellRendererPopup *cell = GIMP_CELL_RENDERER_POPUP (popup_cell (&f));
  gchar *path = popup_path_text (&f, f.a);
  PopupDelivery delivery = { G_OBJECT (f.view->view), path, NULL, 0, TRUE };
  g_signal_connect (cell, "parent-clicked", G_CALLBACK (record_popup_delivery), &delivery);
  gimp_cell_renderer_popup_clicked (cell, delivery.widget, path, NULL);
  g_assert_cmpuint (delivery.calls, ==, 1);
  GtkWidget *popup = current_popup (GTK_CELL_RENDERER (cell)); g_assert_nonnull (popup);
  gtk_range_set_value (GTK_RANGE (find (popup, "painter-tree-layer-opacity")), 43);
  g_assert_cmpfloat_with_epsilon (gimp_layer_get_opacity (f.a), .43, .000001);
  g_assert_cmpfloat (gimp_layer_get_opacity (f.b), ==, 1);
  g_signal_handlers_disconnect_by_data (cell, &delivery);
  g_object_unref (popup); g_free (path); fixture_clear (&f);
}

/* Use a genuine native subclass and its installed class closure. This tests
 * the current RUN_FIRST contract, not the removed C++ interface or old ABI. */
typedef struct {
  GimpCellRendererPopup parent_instance;
  GString *trace;
  gboolean chain;
  guint closures;
} TestPopupRenderer;
typedef struct { GimpCellRendererPopupClass parent_class; } TestPopupRendererClass;
G_DEFINE_TYPE (TestPopupRenderer, test_popup_renderer, GIMP_TYPE_CELL_RENDERER_POPUP)

static void test_popup_parent_clicked (GimpCellRendererPopup *cell, GObject *widget,
                                      const gchar *path, GdkRectangle *area) {
  TestPopupRenderer *test = (TestPopupRenderer *) cell;
  ++test->closures;
  g_string_append_c (test->trace, 'C');
  if (test->chain) {
    GimpCellRendererPopupClass *parent = test_popup_renderer_parent_class;
    parent->parent_clicked (cell, widget, path, area);
    g_assert_nonnull (gimp_cell_renderer_popup_get_popup (cell));
    g_string_append_c (test->trace, 'P');
  }
}
static void test_popup_renderer_class_init (TestPopupRendererClass *klass) {
  ((GimpCellRendererPopupClass *) klass)->parent_clicked = test_popup_parent_clicked;
}
static void test_popup_renderer_init (TestPopupRenderer *test) {
  test->trace = NULL; test->chain = FALSE; test->closures = 0;
}
static void subclass_normal_observer (GimpCellRendererPopup *cell, GObject *widget,
                                     const gchar *path, GdkRectangle *area,
                                     gpointer data) {
  TestPopupRenderer *test = (TestPopupRenderer *) cell;
  record_popup_delivery (cell, widget, path, area, data);
  g_string_append_c (test->trace, 'N');
}
static void subclass_after_observer (GimpCellRendererPopup *cell, GObject *widget,
                                    const gchar *path, GdkRectangle *area,
                                    gpointer data) {
  TestPopupRenderer *test = (TestPopupRenderer *) cell;
  record_popup_delivery (cell, widget, path, area, data);
  g_string_append_c (test->trace, 'A');
}
static void public_clicked_subclass_order (void) {
  Fixture f; fixture_init (&f);
  gchar *path = popup_path_text (&f, f.a);
  GdkRectangle area = { 0, 0, 100, 24 };
  for (guint chain = 0; chain < 2; ++chain) {
    TestPopupRenderer *test = g_object_new (test_popup_renderer_get_type (), NULL);
    g_object_ref_sink (test);
    test->trace = g_string_new (NULL); test->chain = chain;
    GimpCellRendererPopup *cell = GIMP_CELL_RENDERER_POPUP (test);
    g_assert_true (gimp_cell_renderer_popup_bind_tree (cell, f.tree));
    GSignalQuery query;
    g_signal_query (g_signal_lookup ("parent-clicked", G_OBJECT_TYPE (cell)), &query);
    g_assert_cmpuint (query.signal_flags & (G_SIGNAL_RUN_FIRST | G_SIGNAL_RUN_LAST | G_SIGNAL_RUN_CLEANUP),
                      ==, G_SIGNAL_RUN_FIRST);
    g_assert_cmpuint (query.n_params, ==, 3);
    g_assert_true (query.return_type == G_TYPE_NONE);
    PopupDelivery delivery = { G_OBJECT (f.view->view), path, &area, 0, chain };
    g_signal_connect (cell, "parent-clicked", G_CALLBACK (subclass_normal_observer), &delivery);
    g_signal_connect_after (cell, "parent-clicked", G_CALLBACK (subclass_after_observer), &delivery);
    gimp_cell_renderer_popup_clicked (cell, delivery.widget, path, &area);
    g_assert_cmpstr (test->trace->str, ==, chain ? "CPNA" : "CNA");
    g_assert_cmpuint (test->closures, ==, 1);
    g_assert_cmpuint (delivery.calls, ==, 2);
    if (chain) {
      GtkWidget *popup = current_popup (GTK_CELL_RENDERER (cell)); g_assert_nonnull (popup);
      gimp_container_view_set_1_selected (GIMP_CONTAINER_VIEW (f.tree), GIMP_VIEWABLE (f.b));
      gtk_range_set_value (GTK_RANGE (find (popup, "painter-tree-layer-opacity")), 39);
      g_assert_cmpfloat_with_epsilon (gimp_layer_get_opacity (f.a), .39, .000001);
      g_assert_cmpfloat (gimp_layer_get_opacity (f.b), ==, 1);
      g_object_unref (popup);
    } else {
      g_assert_cmpuint (gimp_cell_renderer_popup_get_generation (cell), ==, 0);
    }
    GString *trace = test->trace;
    g_object_unref (test); g_string_free (trace, TRUE);
  }
  g_free (path); fixture_clear (&f);
}

typedef struct {
  Fixture *fixture;
  const gchar *outer_path, *inner_path;
  GString *trace;
  guint normal_calls, after_calls;
  gboolean nested;
} NestedPopupClick;
static void public_click_nested_observer (GimpCellRendererPopup *cell, GObject *widget,
                                         const gchar *path, GdkRectangle *area,
                                         gpointer data) {
  NestedPopupClick *click = data;
  ++click->normal_calls;
  g_assert_true (widget == G_OBJECT (click->fixture->view->view));
  g_assert_nonnull (gimp_cell_renderer_popup_get_popup (cell));
  if (click->nested) {
    g_assert_cmpstr (path, ==, click->inner_path);
    g_string_append_c (click->trace, 'I');
    return;
  }
  g_assert_cmpstr (path, ==, click->outer_path);
  g_string_append_c (click->trace, 'O');
  click->nested = TRUE;
  gimp_cell_renderer_popup_clicked (cell, widget, click->inner_path, area);
  click->nested = FALSE;
  g_string_append_c (click->trace, 'R');
}
static void public_click_nested_after (GimpCellRendererPopup *cell, GObject *widget,
                                      const gchar *path, GdkRectangle *area,
                                      gpointer data) {
  NestedPopupClick *click = data;
  (void) cell; (void) widget; (void) area;
  g_assert_cmpstr (path, ==, click->nested ? click->inner_path : click->outer_path);
  ++click->after_calls;
  g_string_append_c (click->trace, click->nested ? 'i' : 'o');
}
static void public_clicked_nested_reopen (void) {
  Fixture f; fixture_init (&f);
  GimpCellRendererPopup *cell = GIMP_CELL_RENDERER_POPUP (popup_cell (&f));
  gchar *outer_path = popup_path_text (&f, f.a), *inner_path = popup_path_text (&f, f.b);
  NestedPopupClick click = { &f, outer_path, inner_path, g_string_new (NULL), 0, 0, FALSE };
  g_signal_connect (cell, "parent-clicked", G_CALLBACK (public_click_nested_observer), &click);
  g_signal_connect_after (cell, "parent-clicked", G_CALLBACK (public_click_nested_after), &click);
  gimp_cell_renderer_popup_clicked (cell, G_OBJECT (f.view->view), outer_path, NULL);
  g_assert_cmpstr (click.trace->str, ==, "OIiRo");
  g_assert_cmpuint (click.normal_calls, ==, 2); g_assert_cmpuint (click.after_calls, ==, 2);
  GtkWidget *popup = current_popup (GTK_CELL_RENDERER (cell)); g_assert_nonnull (popup);
  gtk_range_set_value (GTK_RANGE (find (popup, "painter-tree-layer-opacity")), 29);
  g_assert_cmpfloat_with_epsilon (gimp_layer_get_opacity (f.b), .29, .000001);
  g_assert_cmpfloat (gimp_layer_get_opacity (f.a), ==, 1);
  g_signal_handlers_disconnect_by_data (cell, &click);
  g_object_unref (popup); g_string_free (click.trace, TRUE);
  g_free (outer_path); g_free (inner_path); fixture_clear (&f);
}

typedef struct { GString *trace; gboolean closed; } ClosePopupClick;
static void public_click_close_observer (GimpCellRendererPopup *cell, GObject *widget,
                                        const gchar *path, GdkRectangle *area,
                                        gpointer data) {
  ClosePopupClick *click = data;
  g_string_append_c (click->trace, 'C');
  if (!click->closed) {
    GError *error = NULL;
    g_assert_nonnull (gimp_cell_renderer_popup_get_popup (cell));
    click->closed = TRUE;
    g_assert_true (gimp_painter_binding_close (G_OBJECT (cell), &error));
    g_assert_no_error (error);
    /* This fresh entry is rejected, while the already admitted outer
     * emission must still reach its remaining normal and after observers. */
    gimp_cell_renderer_popup_clicked (cell, widget, path, area);
  }
}
static void public_click_close_later (GimpCellRendererPopup *cell, GObject *widget,
                                     const gchar *path, GdkRectangle *area,
                                     gpointer data) {
  ClosePopupClick *click = data;
  (void) widget; (void) path; (void) area;
  g_assert_null (gimp_cell_renderer_popup_get_popup (cell));
  g_string_append_c (click->trace, 'L');
}
static void public_click_close_after (GimpCellRendererPopup *cell, GObject *widget,
                                     const gchar *path, GdkRectangle *area,
                                     gpointer data) {
  ClosePopupClick *click = data;
  (void) cell; (void) widget; (void) path; (void) area;
  g_string_append_c (click->trace, 'A');
}
static void public_clicked_close_keeps_observer_order (void) {
  Fixture f; fixture_init (&f);
  GimpCellRendererPopup *cell = GIMP_CELL_RENDERER_POPUP (popup_cell (&f));
  gchar *path = popup_path_text (&f, f.a);
  ClosePopupClick click = { g_string_new (NULL), FALSE };
  g_signal_connect (cell, "parent-clicked", G_CALLBACK (public_click_close_observer), &click);
  g_signal_connect (cell, "parent-clicked", G_CALLBACK (public_click_close_later), &click);
  g_signal_connect_after (cell, "parent-clicked", G_CALLBACK (public_click_close_after), &click);
  gimp_cell_renderer_popup_clicked (cell, G_OBJECT (f.view->view), path, NULL);
  g_assert_true (click.closed);
  g_assert_cmpstr (click.trace->str, ==, "CLA");
  gimp_cell_renderer_popup_clicked (cell, G_OBJECT (f.view->view), path, NULL);
  g_assert_cmpstr (click.trace->str, ==, "CLA");
  g_signal_handlers_disconnect_by_data (cell, &click);
  g_string_free (click.trace, TRUE); g_free (path); fixture_clear (&f);
}

typedef struct {
  gpointer renderer_weak, tree_weak;
  guint normal_calls, after_calls;
} DropPopupReferences;
static void public_click_drop_references (GimpCellRendererPopup *cell, GObject *widget,
                                         const gchar *path, GdkRectangle *area,
                                         gpointer data) {
  DropPopupReferences *drop = data;
  (void) path; (void) area;
  ++drop->normal_calls;
  g_object_unref (cell); g_object_unref (widget);
  g_assert_nonnull (drop->renderer_weak); g_assert_nonnull (drop->tree_weak);
}
static void public_click_after_drop (GimpCellRendererPopup *cell, GObject *widget,
                                    const gchar *path, GdkRectangle *area,
                                    gpointer data) {
  DropPopupReferences *drop = data;
  (void) path; (void) area;
  ++drop->after_calls;
  g_assert_true (drop->renderer_weak == cell);
  g_assert_true (drop->tree_weak == widget);
  g_assert_true (GIMP_IS_CELL_RENDERER_POPUP (cell));
  g_assert_true (GTK_IS_TREE_VIEW (widget));
}
static void public_clicked_last_reference_leases (void) {
  GimpCellRendererPopup *cell = GIMP_CELL_RENDERER_POPUP (gimp_cell_renderer_popup_new ());
  GtkWidget *tree = gtk_tree_view_new ();
  g_object_ref_sink (cell); g_object_ref_sink (tree);
  DropPopupReferences drop = { cell, tree, 0, 0 };
  g_object_add_weak_pointer (G_OBJECT (cell), &drop.renderer_weak);
  g_object_add_weak_pointer (G_OBJECT (tree), &drop.tree_weak);
  g_signal_connect (cell, "parent-clicked", G_CALLBACK (public_click_drop_references), &drop);
  g_signal_connect_after (cell, "parent-clicked", G_CALLBACK (public_click_after_drop), &drop);
  gimp_cell_renderer_popup_clicked (cell, G_OBJECT (tree), "0", NULL);
  g_assert_cmpuint (drop.normal_calls, ==, 1); g_assert_cmpuint (drop.after_calls, ==, 1);
  g_assert_null (drop.renderer_weak); g_assert_null (drop.tree_weak);
}

int main (int argc, char **argv) {
  g_test_init (&argc, &argv, NULL);
  if (!gtk_init_check (&argc, &argv)) return GIMP_EXIT_TEST_SKIPPED;
  gimp_test_utils_setup_menus_path (); gimp = gimp_init_for_gui_testing (TRUE);
#define ADD(name) g_test_add_func ("/painter-layer-tree/" #name, name)
  ADD (native_column); ADD (direct_filter_button); ADD (target_controls);
  ADD (modifier_target); ADD (retained_controls_owner_close);
  ADD (image_switch_and_removed_target); ADD (newer_popup_wins);
  ADD (selection_reorders_clicked_row); ADD (filter_editor_clicked_target);
  ADD (refresh_reentry_initializes_new_target); ADD (selection_newer_popup_wins);
  ADD (selection_removes_target); ADD (selection_destroys_owner);
  ADD (repeat_popdown_disconnects_old_controls);
  ADD (signal_observer_newer_popup_wins); ADD (signal_observer_preserves_clicked_target);
  ADD (image_switch_reentrant_old_popup); ADD (scrolled_full_row_geometry);
  ADD (group_pass_through_context);
  ADD (same_target_refresh_keeps_latest_model); ADD (selection_removes_column);
  ADD (selection_only_supersedes_activation); ADD (focus_reentry_closes_owner);
  ADD (attributes_supersede_activation); ADD (first_attributes_preserve_request);
  ADD (removed_row_invalidates_drag_cache);
  ADD (public_factory_floating_column_ownership);
  ADD (public_clicked_active_property_independent); ADD (public_clicked_unbound_observers);
  ADD (public_clicked_invalid_row_observers); ADD (public_clicked_null_area);
  ADD (public_clicked_subclass_order); ADD (public_clicked_nested_reopen);
  ADD (public_clicked_close_keeps_observer_order); ADD (public_clicked_last_reference_leases);
  g_application_run (gimp->app, 0, NULL);
  int result = gimp_core_app_get_exit_status (GIMP_CORE_APP (gimp->app));
  g_application_quit (G_APPLICATION (gimp->app)); g_clear_object (&gimp->app);
  return result;
}
