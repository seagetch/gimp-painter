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
static void target_controls (void) {
  for (guint direction = 0; direction < 2; ++direction) {
    Fixture f; fixture_init_direction (&f, direction ? GTK_TEXT_DIR_RTL : GTK_TEXT_DIR_LTR);
    GtkCellRenderer *cell = popup_cell (&f);
    g_assert_true (click_row (&f, f.a, 0));
    GtkWidget *popup = current_popup (cell); g_assert_nonnull (popup);
    GdkRectangle anchor; g_assert_true (gtk_popover_get_pointing_to (GTK_POPOVER (popup), &anchor));
    g_assert_cmpint (anchor.width, ==, gtk_widget_get_allocated_width (GTK_WIDGET (f.view->view)));
    g_assert_cmpint (anchor.x, ==, 0);
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
  GtkWidget *button = find (popup, "painter-tree-edit-filter"); g_assert_nonnull (button);
  gimp_container_view_set_1_selected (GIMP_CONTAINER_VIEW (f.tree), GIMP_VIEWABLE (f.b));
  gtk_button_clicked (GTK_BUTTON (button));
  GtkWidget *dialog = filter_dialog (); g_assert_nonnull (dialog);
  g_assert_null (find (dialog, "painter-filter-name"));
  g_assert_nonnull (find (dialog, "painter-filter-definition"));
  g_assert_null (current_popup (popup_cell (&f)));
  gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL);
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
    gtk_popover_popdown (GTK_POPOVER (popup)); drain ();
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
    g_assert_true (gtk_popover_get_pointing_to (GTK_POPOVER (popup), &anchor));
    g_assert_cmpint (anchor.x, ==, 0); g_assert_cmpint (anchor.y, ==, expected_y);
    g_assert_cmpint (anchor.height, ==, cell_area.height);
    g_assert_cmpint (anchor.width, ==, gtk_widget_get_allocated_width (view));
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
  g_application_run (gimp->app, 0, NULL);
  int result = gimp_core_app_get_exit_status (GIMP_CORE_APP (gimp->app));
  g_application_quit (G_APPLICATION (gimp->app)); g_clear_object (&gimp->app);
  return result;
}
