/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gegl.h>
#include <gtk/gtk.h>
extern "C" {
#include "libgimpbase/gimpbase.h"
#include "libgimpwidgets/gimpwidgets.h"
#include "core/core-types.h"
#include "widgets/widgets-types.h"
#include "core/gimp.h"
#include "core/gimpcontext.h"
#include "core/gimpimage.h"
#include "core/gimpimage-undo.h"
#include "core/gimplayer-new.h"
#include "core/gimplayerpreset.h"
#include "core/gimpcontainer.h"
#include "core/gimpdatafactory.h"
#include "core/gimpundostack.h"
#include "config/gimpcoreconfig.h"
#include "widgets/gimpeditor.h"
#include "widgets/gimpeditor-cxx.h"
#include "widgets/gimpcontainertreeview.h"
#include "widgets/gimpcontainerview.h"
#include "widgets/gimplayerpresetview.h"
#include "widgets/gimpaction.h"
#include "widgets/gimpactionimpl.h"
#include "widgets/gimptoggleaction.h"
#include "widgets/gimpactiongroup.h"
#include "widgets/gimpuimanager.h"
#include "widgets/gimpmenufactory.h"
#include "menus/menus.h"
#include "gimpcoreapp.h"
#include "tests.h"
#include "gimp-app-test-utils.h"
}
#include "painter/binding-store.hpp"
#include "widgets/gimpeditor-handle.hpp"
#include <memory>

using namespace GimpPainter;
static Gimp *gimp;
static void drain () { gimp_test_run_mainloop_until_idle (); }
struct Fixture {
  GtkWidget *window, *editor;
  GimpMenuFactory *factory;
  bool manager_deleted = false;
  Fixture () {
    factory = menus_get_global_menu_factory (gimp);
    window = gtk_window_new (GTK_WINDOW_TOPLEVEL); g_object_ref_sink (window);
    editor = gimp_container_tree_view_new (nullptr, gimp->user_context, 32, 0);
    g_object_ref_sink (editor);
    gimp_editor_create_menu (GIMP_EDITOR (editor), factory, "<LayerPresets>", "/layer-presets-popup", editor);
    gtk_container_add (GTK_CONTAINER (window), editor);
    gtk_widget_show_all (window); drain ();
  }
  ~Fixture () {
    gtk_widget_destroy (window);
    if (!manager_deleted) gimp_menu_factory_delete_manager (factory, "<LayerPresets>", editor);
    g_object_unref (editor); g_object_unref (window); drain ();
  }
  GtkWidget *arrow () {
    GList *children = gtk_container_get_children (GTK_CONTAINER (gimp_editor_get_button_box (GIMP_EDITOR (editor))));
    auto *result = GTK_WIDGET (g_list_last (children)->data);
    g_list_free (children); g_assert_true (GTK_IS_MENU_BUTTON (result)); return result;
  }
};
static GtkWidget *generic (Fixture& fixture, GCallback clicked = nullptr, GCallback extended = nullptr, gpointer data = nullptr) {
  auto *button = gimp_editor_add_dropdown (GIMP_EDITOR (fixture.editor), "document-new", "Create a layer", "dropdown-help", "deliberately-ignored", clicked, extended, data);
  g_assert_nonnull (button); return button;
}
static GtkWidget *open (GtkWidget *arrow) {
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (arrow), TRUE); drain ();
  auto *popup = GTK_WIDGET (gtk_menu_button_get_popover (GTK_MENU_BUTTON (arrow)));
  g_assert_nonnull (popup); g_assert_true (gtk_widget_get_visible (popup));
  auto *view = gtk_bin_get_child (GTK_BIN (popup));
  g_assert_true (GIMP_IS_LAYER_PRESET_VIEW (view)); return view;
}
/* Wait for an observed GTK condition, with a failure watchdog. This does not
 * replace popdown with hide or change native animation settings. */
template<class Predicate>
static void wait_for (const gchar *condition, Predicate ready) {
  if (ready ()) return;
  bool timed_out = false;
  const guint timeout = g_timeout_add_seconds (5, [] (gpointer data) -> gboolean {
    *static_cast<bool *> (data) = true; return G_SOURCE_REMOVE;
  }, &timed_out);
  while (!ready () && !timed_out) g_main_context_iteration (nullptr, TRUE);
  if (!timed_out) g_source_remove (timeout);
  else g_test_message ("Timed out waiting for %s", condition);
  g_assert_false (timed_out);
}
struct OpeningTransition {
  bool progressed = false, completed = false;
};
static void opening_shown (GtkWidget *, gpointer data) {
  auto *state = static_cast<OpeningTransition *> (data);
  state->progressed = state->completed = false;
}
static void opening_opacity (GObject *object, GParamSpec *, gpointer data) {
  auto *state = static_cast<OpeningTransition *> (data);
  if (gtk_widget_get_opacity (GTK_WIDGET (object)) < 1.0) state->progressed = true;
  else if (state->progressed) state->completed = true;
}
static void assert_only_child (GtkWidget *popup, GtkWidget *expected) {
  GList *children = gtk_container_get_children (GTK_CONTAINER (popup));
  g_assert_cmpuint (g_list_length (children), ==, 1);
  g_assert_true (children->data == expected); g_list_free (children);
}
static void finish_opening (GtkWidget *popup, OpeningTransition& transition) {
  gboolean animations = FALSE;
  g_object_get (gtk_widget_get_settings (popup), "gtk-enable-animations", &animations, nullptr);
  if (animations) wait_for ("native popup opening animation", [&] { return transition.completed; });
  drain ();
  g_assert_true (gtk_widget_get_visible (popup));
  g_assert_true (gtk_widget_get_mapped (popup));
}
struct Calls { guint normal = 0, extended = 0; GdkModifierType mask = GdkModifierType (0); GtkWidget *destroy = nullptr; };
static void clicked (GtkWidget *, gpointer data) {
  auto *calls = static_cast<Calls *> (data); ++calls->normal;
  if (calls->destroy) gtk_widget_destroy (calls->destroy);
}
static void extended (GtkWidget *, GdkModifierType mask, gpointer data) {
  auto *calls = static_cast<Calls *> (data); ++calls->extended; calls->mask = mask;
}
static void generic_contract () {
  Fixture fixture; Calls calls;
  auto *button = generic (fixture, G_CALLBACK (clicked), G_CALLBACK (extended), &calls);
  auto *arrow = fixture.arrow ();
  g_object_ref (button); g_object_ref (arrow);
  auto *store = BindingStore::find (G_OBJECT (arrow));
  g_assert_nonnull (store); g_assert_true (store->state () == BindingStore::State::active);
  g_assert_null (BindingStore::find (G_OBJECT (button)));
  g_assert_false (gtk_box_get_homogeneous (gimp_editor_get_button_box (GIMP_EDITOR (fixture.editor))));
  auto *box = gimp_editor_get_button_box (GIMP_EDITOR (fixture.editor));
  gint spacing; gtk_widget_style_get (fixture.editor, "button-spacing", &spacing, nullptr);
  g_assert_cmpint (gtk_box_get_spacing (box), ==, spacing);
  /* The legacy/native bar is END-packed at child-property position 0.
   * GtkBox enumerates START children before END children, so this is not
   * the first entry returned by gtk_container_get_children(). */
  g_assert_true (gtk_widget_get_parent (GTK_WIDGET (box)) == fixture.editor);
  gboolean box_expand, box_fill; guint box_padding; GtkPackType box_packing;
  gtk_box_query_child_packing (GTK_BOX (fixture.editor), GTK_WIDGET (box),
                              &box_expand, &box_fill, &box_padding, &box_packing);
  g_assert_false (box_expand); g_assert_false (box_fill);
  g_assert_cmpuint (box_padding, ==, 0); g_assert_cmpint (box_packing, ==, GTK_PACK_END);
  gint box_position = -1;
  gtk_container_child_get (GTK_CONTAINER (fixture.editor), GTK_WIDGET (box),
                           "position", &box_position, nullptr);
  g_assert_cmpint (box_position, ==, 0);
  GList *editor_children = gtk_container_get_children (GTK_CONTAINER (fixture.editor));
  g_assert_nonnull (editor_children);
  g_assert_true (g_list_last (editor_children)->data == box); g_list_free (editor_children);
  GList *buttons = gtk_container_get_children (GTK_CONTAINER (box));
  g_assert_cmpuint (g_list_length (buttons), ==, 2);
  g_assert_true (buttons->data == button); g_assert_true (buttons->next->data == arrow); g_list_free (buttons);
  for (auto *widget : {button, arrow}) {
    gboolean expand, fill; guint padding; GtkPackType packing;
    gtk_box_query_child_packing (box, widget, &expand, &fill, &padding, &packing);
    g_assert_true (expand); g_assert_true (fill); g_assert_cmpuint (padding, ==, 0); g_assert_cmpint (packing, ==, GTK_PACK_START);
  }
  for (auto *widget : {button, arrow}) {
    gchar *tooltip = gtk_widget_get_tooltip_text (widget);
    g_assert_cmpstr (tooltip, ==, "Create a layer"); g_free (tooltip);
    g_assert_cmpstr (static_cast<const gchar *> (g_object_get_qdata (G_OBJECT (widget), GIMP_HELP_ID)), ==, "dropdown-help");
  }
  gtk_button_clicked (GTK_BUTTON (button));
  g_signal_emit_by_name (button, "extended-clicked", GDK_SHIFT_MASK);
  g_assert_cmpuint (calls.normal, ==, 1); g_assert_cmpuint (calls.extended, ==, 1);
  g_assert_cmpint (calls.mask, ==, GDK_SHIFT_MASK);
  GdkEventScroll event = {}; event.type = GDK_SCROLL; gboolean handled = FALSE;
  g_signal_emit_by_name (arrow, "scroll-event", &event, &handled); g_assert_true (handled);
  const auto generation = store->generation ();
  store->close (); store->close ();
  g_assert_true (store->state () == BindingStore::State::closed);
  g_assert_cmpuint (store->generation (), ==, generation + 1);
  gtk_button_clicked (GTK_BUTTON (button));
  g_signal_emit_by_name (button, "extended-clicked", GDK_CONTROL_MASK);
  g_signal_emit_by_name (arrow, "clicked");
  g_assert_cmpuint (calls.normal, ==, 1); g_assert_cmpuint (calls.extended, ==, 1);
  g_object_unref (button); g_object_unref (arrow);
}
static void generic_reentrant_close () {
  Fixture fixture; Calls calls; calls.destroy = fixture.editor;
  auto *button = generic (fixture, G_CALLBACK (clicked), G_CALLBACK (extended), &calls);
  auto *arrow = fixture.arrow (); g_object_ref (button); g_object_ref (arrow);
  gtk_button_clicked (GTK_BUTTON (button));
  g_assert_cmpuint (calls.normal, ==, 1);
  g_assert_true (BindingStore::require (G_OBJECT (arrow)).state () == BindingStore::State::closed);
  gtk_button_clicked (GTK_BUTTON (button)); g_signal_emit_by_name (button, "extended-clicked", GDK_SHIFT_MASK);
  g_assert_cmpuint (calls.normal, ==, 1); g_assert_cmpuint (calls.extended, ==, 0);
  g_object_unref (button); g_object_unref (arrow);
}
static GimpImage *new_image (GimpLayer **source) {
  auto *image = gimp_image_new (gimp, 64, 64, GIMP_RGB, GIMP_PRECISION_U8_NON_LINEAR);
  *source = gimp_layer_new (image, 64, 64, gimp_image_get_layer_format (image, TRUE), "Dropdown source", 1, GIMP_LAYER_MODE_NORMAL_LEGACY);
  gimp_image_add_layer (image, *source, nullptr, 0, FALSE);
  return image;
}
static GimpViewable *clone_preset () {
  auto *presets = gimp_data_factory_get_container (gimp->layer_preset_factory);
  auto *preset = GIMP_VIEWABLE (gimp_container_get_child_by_name (presets, "Clone layer"));
  g_assert_nonnull (preset); return preset;
}
static void popup_cycles_context_and_retained_view () {
  Fixture fixture; generic (fixture); auto *arrow = fixture.arrow ();
  auto *popup = GTK_WIDGET (gtk_menu_button_get_popover (GTK_MENU_BUTTON (arrow)));
  g_object_ref (popup);
  GimpLayer *source; auto *image = new_image (&source);
  auto *context = gimp_context_new (gimp, "Dropdown independent context", nullptr);
  gimp_context_set_image (context, image);
  gimp_container_view_set_context (GIMP_CONTAINER_VIEW (fixture.editor), context);
  for (guint i = 0; i < 3; ++i) {
    auto *view = open (arrow); g_object_ref (view);
    auto *container = GIMP_CONTAINER_EDITOR (view)->view; g_object_ref (container);
    g_assert_true (gimp_container_view_get_context (container) == context);
    gint width, height; gtk_widget_get_size_request (view, &width, &height); g_assert_cmpint (height, ==, 300);
    gimp_container_view_set_1_selected (container, clone_preset ());
    g_assert_cmpint (gimp_image_get_n_layers (image), ==, 2);
    g_assert_cmpint (gimp_undo_stack_get_depth (gimp_image_get_undo_stack (image)), ==, 1);
    gtk_popover_popdown (GTK_POPOVER (popup)); drain ();
    g_assert_null (gtk_bin_get_child (GTK_BIN (popup)));
    g_assert_false (gimp_layer_preset_view_can_apply (GIMP_LAYER_PRESET_VIEW (view)));
    gimp_layer_preset_view_apply (GIMP_LAYER_PRESET_VIEW (view));
    g_signal_emit_by_name (container, "item-activated", clone_preset ());
    g_assert_cmpint (gimp_image_get_n_layers (image), ==, 2);
    g_assert_true (gimp_image_undo (image));
    GList *layers = g_list_prepend (nullptr, source); gimp_image_set_selected_layers (image, layers); g_list_free (layers);
    GObject *weak_view = G_OBJECT (view); g_object_add_weak_pointer (weak_view, reinterpret_cast<gpointer *> (&weak_view));
    g_object_unref (container); g_object_unref (view); g_assert_null (weak_view);
  }
  /* Context is obtained at each opening, rather than frozen by construction. */
  gimp_container_view_set_context (GIMP_CONTAINER_VIEW (fixture.editor), gimp->user_context);
  auto *view = open (arrow);
  g_assert_true (gimp_container_view_get_context (GIMP_CONTAINER_EDITOR (view)->view) == gimp->user_context);
  g_object_ref (arrow);
  BindingStore::require (G_OBJECT (arrow)).close ();
  g_signal_emit_by_name (popup, "show"); g_signal_emit_by_name (popup, "closed");
  g_assert_null (gtk_bin_get_child (GTK_BIN (popup)));
  g_object_unref (popup); g_object_unref (arrow);
  gimp_context_set_image (context, nullptr); g_object_unref (context); g_object_unref (image);
}
struct Reopen {
  GtkWidget *editor, *arrow;
  GimpContext *context;
  guint calls = 0;
};
static void reopen_from_retired_view (GtkWidget *, gpointer data) {
  auto *state = static_cast<Reopen *> (data);
  ++state->calls;
  gimp_container_view_set_context (GIMP_CONTAINER_VIEW (state->editor), state->context);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (state->arrow), TRUE);
}
static void retire_reopens_newer_context () {
  Fixture fixture; generic (fixture); auto *arrow = fixture.arrow ();
  auto *old_view = open (arrow); g_object_ref (old_view);
  auto *popup = GTK_WIDGET (gtk_menu_button_get_popover (GTK_MENU_BUTTON (arrow)));
  auto *context = gimp_context_new (gimp, "Reopened context", nullptr);
  auto& store = BindingStore::require (G_OBJECT (arrow));
  const auto generation = store.generation ();
  Reopen state {fixture.editor, arrow, context};
  g_signal_connect (old_view, "destroy", G_CALLBACK (reopen_from_retired_view), &state);
  /* Exercise native synchronous dismissal. Its closed callback retires the
   * old child, whose destroy observer requests a newer context. The remaining
   * hide emission must not invalidate that activation. The separate closed-
   * retirement case covers animated popdown. Never emit show on a visible
   * GtkPopover: that reruns its class handler outside gtk_widget_show's guard. */
  gtk_widget_hide (popup);
  g_assert_cmpuint (state.calls, ==, 1);
  g_assert_null (gtk_bin_get_child (GTK_BIN (popup)));
  g_assert_false (gimp_layer_preset_view_can_apply (GIMP_LAYER_PRESET_VIEW (old_view)));
  g_assert_true (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (arrow)));
  drain ();
  auto *current = gtk_bin_get_child (GTK_BIN (popup));
  g_assert_true (GIMP_IS_LAYER_PRESET_VIEW (current));
  g_assert_true (current != old_view);
  g_assert_true (gimp_container_view_get_context (GIMP_CONTAINER_EDITOR (current)->view) == context);
  GList *children = gtk_container_get_children (GTK_CONTAINER (popup));
  g_assert_cmpuint (g_list_length (children), ==, 1); g_list_free (children);
  g_assert_true (gtk_widget_get_visible (popup));
  g_assert_true (gtk_widget_get_mapped (popup));
  g_assert_true (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (arrow)));
  g_assert_true (gtk_grab_get_current () == popup);
  g_assert_true (store.state () == BindingStore::State::active);
  g_assert_cmpuint (store.generation (), ==, generation);
  g_assert_cmpuint (state.calls, ==, 1);
  g_object_unref (old_view);
  BindingStore::require (G_OBJECT (arrow)).close ();
  g_object_unref (context);
}
struct Destruction { GtkWidget *target; guint calls = 0; };
static void layer_added (GimpContainer *, GimpObject *, gpointer data) {
  auto *state = static_cast<Destruction *> (data);
  if (state->calls++ == 0) gtk_widget_destroy (state->target);
}
static void selection_owner_loss () {
  for (guint mode = 0; mode < 3; ++mode) {
    Fixture fixture; auto *button = generic (fixture); auto *arrow = fixture.arrow ();
    GimpLayer *source; auto *image = new_image (&source);
    gimp_context_set_image (gimp->user_context, image);
    auto *view = open (arrow); auto *container = GIMP_CONTAINER_EDITOR (view)->view;
    g_object_ref (view); g_object_ref (container); g_object_ref (button); g_object_ref (arrow);
    auto *popup = GTK_WIDGET (gtk_menu_button_get_popover (GTK_MENU_BUTTON (arrow))); g_object_ref (popup);
    Destruction state { mode == 0 ? fixture.editor : mode == 1 ? button : arrow };
    auto handler = g_signal_connect (gimp_image_get_layers (image), "add", G_CALLBACK (layer_added), &state);
    gimp_container_view_set_1_selected (container, clone_preset ());
    g_signal_handler_disconnect (gimp_image_get_layers (image), handler);
    g_assert_cmpuint (state.calls, ==, 1);
    g_assert_true (BindingStore::require (G_OBJECT (arrow)).state () == BindingStore::State::closed);
    g_assert_cmpint (gimp_image_get_n_layers (image), ==, 2);
    g_assert_cmpuint (gimp_image_get_undo_group_count (image), ==, 0);
    g_assert_false (gimp_layer_preset_view_can_apply (GIMP_LAYER_PRESET_VIEW (view)));
    gimp_layer_preset_view_apply (GIMP_LAYER_PRESET_VIEW (view));
    g_signal_emit_by_name (container, "item-activated", clone_preset ());
    g_signal_emit_by_name (popup, "show"); g_signal_emit_by_name (popup, "closed");
    BindingStore::require (G_OBJECT (arrow)).close ();
    g_assert_cmpint (gimp_image_get_n_layers (image), ==, 2);
    g_object_unref (container); g_object_unref (view); g_object_unref (popup); g_object_unref (button); g_object_unref (arrow);
    gimp_context_set_image (gimp->user_context, nullptr); g_object_unref (image);
  }
}
static void action_called (GimpAction *, GVariant *, gpointer data) { ++*static_cast<guint *> (data); }
static GimpAction *action (GimpActionGroup *group, const char *name, const char *tooltip, guint *count) {
  auto *value = gimp_action_impl_new (name, name, nullptr, tooltip, "document-new", "action-help", gimp->user_context);
  g_signal_connect (value, "activate", G_CALLBACK (action_called), count);
  gimp_action_group_add_action (group, value); g_object_unref (value); return value;
}
static void native_action_contract () {
  Fixture fixture; guint normal = 0, shift = 0, control = 0;
  auto *manager = gimp_editor_get_ui_manager (GIMP_EDITOR (fixture.editor));
  auto *group = gimp_action_group_new (gimp, "editor-dropdown-test", "Dropdown test", nullptr, nullptr, nullptr);
  gimp_ui_manager_add_action_group (manager, group); g_object_unref (group);
  auto *main = action (group, "dropdown-main", "Main <tip>", &normal);
  auto *shift_action = action (group, "dropdown-shift", "Shift <tip>", &shift);
  auto *control_action = action (group, "dropdown-control", "Control <tip>", &control);
  (void) shift_action;
  auto *button = gimp_editor_add_action_dropdown (GIMP_EDITOR (fixture.editor), nullptr, "editor-dropdown-test", "dropdown-main",
    "dropdown-shift", GDK_SHIFT_MASK, "missing-action", GDK_MOD1_MASK, "dropdown-control", GDK_CONTROL_MASK, nullptr);
  g_assert_nonnull (button); auto *arrow = fixture.arrow ();
  g_assert_nonnull (g_object_get_data (G_OBJECT (button), "extended-actions"));
  gchar *markup = gtk_widget_get_tooltip_markup (button);
  g_assert_nonnull (strstr (markup, "Main &lt;tip&gt;"));
  g_assert_nonnull (strstr (markup, "Shift &lt;tip&gt;")); g_assert_nonnull (strstr (markup, "Control &lt;tip&gt;")); g_free (markup);
  gchar *arrow_help = gtk_widget_get_tooltip_text (arrow); g_assert_cmpstr (arrow_help, ==, "Main <tip>"); g_free (arrow_help);
  g_assert_cmpstr (static_cast<const gchar *> (g_object_get_qdata (G_OBJECT (arrow), GIMP_HELP_ID)), ==, "action-help");
  gtk_button_clicked (GTK_BUTTON (button)); g_assert_cmpuint (normal, ==, 1);
  g_signal_emit_by_name (button, "extended-clicked", GDK_SHIFT_MASK); g_assert_cmpuint (shift, ==, 1);
  g_signal_emit_by_name (button, "extended-clicked", GDK_SHIFT_MASK | GDK_CONTROL_MASK); g_assert_cmpuint (control, ==, 1);
  gimp_action_set_sensitive (control_action, FALSE, "Test disabled");
  g_signal_emit_by_name (button, "extended-clicked", GDK_SHIFT_MASK | GDK_CONTROL_MASK); g_assert_cmpuint (shift, ==, 2);
  gimp_action_set_sensitive (main, FALSE, "Test disabled"); g_assert_false (gtk_widget_get_sensitive (button));
  g_assert_true (gtk_widget_get_sensitive (arrow)); // Original arrow did not proxy the primary action.
  gimp_action_set_sensitive (main, TRUE, nullptr); g_assert_true (gtk_widget_get_sensitive (button));
  g_object_ref (button); g_object_ref (arrow);
  gtk_widget_destroy (fixture.editor);
  gimp_menu_factory_delete_manager (fixture.factory, "<LayerPresets>", fixture.editor); fixture.manager_deleted = true;
  gtk_button_clicked (GTK_BUTTON (button)); g_signal_emit_by_name (button, "extended-clicked", GDK_SHIFT_MASK);
  g_assert_cmpuint (normal, ==, 1); g_assert_cmpuint (shift, ==, 2); g_assert_cmpuint (control, ==, 1);
  g_object_unref (button); g_object_unref (arrow);
}
/* Inject owner loss at synchronous show boundaries, after native parenting.
 * GTK 3.24.49 dereferences the child's parent immediately after parent-set;
 * destroying that parent in a parent-set hook invalidates GTK's own operation.
 * show is RUN_FIRST, so its native class closure finishes before this hook.
 * Box/primary/arrow shows are still inside gimp_editor_pack_button(), and the
 * primary image show is still inside construction before proxy installation.
 * No idle or post-helper cancellation may replace these boundaries. */
struct CancelConstruction { GtkWidget *editor; guint mode, calls = 0; GtkWidget *retained_primary = nullptr; };
static gboolean cancel_construction (GSignalInvocationHint *, guint, const GValue *values, gpointer data) {
  auto *state = static_cast<CancelConstruction *> (data);
  auto *widget = GTK_WIDGET (g_value_get_object (&values[0]));
  if (state->calls) return TRUE;
  auto *box = gimp_editor_get_button_box (GIMP_EDITOR (state->editor));
  if (state->mode == 3) {
    auto *parent = gtk_widget_get_parent (widget);
    if (GTK_IS_IMAGE (widget) && parent && GIMP_IS_BUTTON (parent)) {
      ++state->calls; state->retained_primary = GTK_WIDGET (g_object_ref (parent));
      gtk_widget_destroy (parent);
    }
    return TRUE;
  }
  const bool match = state->mode == 0 ? widget == GTK_WIDGET (box) :
    state->mode == 1 ? GIMP_IS_BUTTON (widget) && gtk_widget_get_parent (widget) == GTK_WIDGET (box) :
    GTK_IS_MENU_BUTTON (widget) && gtk_widget_get_parent (widget) == GTK_WIDGET (box);
  if (match) { ++state->calls; gtk_widget_destroy (state->editor); }
  return TRUE;
}
static void construction_owner_loss () {
  for (guint kind = 0; kind < 3; ++kind) {
    for (guint mode = 0; mode < 4; ++mode) {
      if (kind == 2 && mode == 2) continue;
      g_test_message ("Construction owner loss: kind=%u mode=%u", kind, mode);
      Fixture fixture; guint calls = 0;
      auto *group = gimp_action_group_new (gimp, "editor-dropdown-construction", "Construction", nullptr, nullptr, nullptr);
      gimp_ui_manager_add_action_group (gimp_editor_get_ui_manager (GIMP_EDITOR (fixture.editor)), group);
      auto *main = action (group, "construction-main", "Construct", &calls); g_object_unref (group);
      CancelConstruction cancellation {fixture.editor, mode};
      const guint signal = g_signal_lookup ("show", GTK_TYPE_WIDGET);
      const gulong hook = g_signal_add_emission_hook (signal, 0, cancel_construction, &cancellation, nullptr);
      g_assert_cmpuint (hook, !=, 0);
      GtkWidget *button;
      if (kind == 0)
        button = gimp_editor_add_dropdown (GIMP_EDITOR (fixture.editor), "document-new", "Create", nullptr, nullptr, nullptr, nullptr, nullptr);
      else if (kind == 1)
        button = gimp_editor_add_action_dropdown (GIMP_EDITOR (fixture.editor), nullptr, "editor-dropdown-construction", "construction-main", nullptr);
      else
        button = gimp_editor_add_action_button (GIMP_EDITOR (fixture.editor), "editor-dropdown-construction", "construction-main", nullptr);
      g_signal_remove_emission_hook (signal, hook);
      g_assert_cmpuint (cancellation.calls, ==, 1); g_assert_null (button); g_assert_cmpuint (calls, ==, 0);
      if (cancellation.retained_primary) {
        gimp_action_set_sensitive (main, FALSE, "Must not update a cancelled proxy");
        g_assert_true (gtk_widget_get_sensitive (cancellation.retained_primary));
        gtk_button_clicked (GTK_BUTTON (cancellation.retained_primary));
        g_assert_cmpuint (calls, ==, 0); g_object_unref (cancellation.retained_primary);
      }
    }
  }
}
static void ordinary_action_control () {
  Fixture fixture; guint normal = 0, modified = 0;
  auto *group = gimp_action_group_new (gimp, "editor-ordinary-control", "Ordinary", nullptr, nullptr, nullptr);
  gimp_ui_manager_add_action_group (gimp_editor_get_ui_manager (GIMP_EDITOR (fixture.editor)), group);
  auto *main = action (group, "ordinary-main", "Normal <tip>", &normal);
  auto *alternate = action (group, "ordinary-modified", "Modified <tip>", &modified);
  auto *button = gimp_editor_add_action_button (GIMP_EDITOR (fixture.editor), "editor-ordinary-control", "ordinary-main", "ordinary-modified", GDK_SHIFT_MASK, nullptr);
  g_assert_nonnull (button); g_assert_true (gtk_widget_get_visible (button));
  g_assert_true (gtk_widget_get_parent (button) == GTK_WIDGET (gimp_editor_get_button_box (GIMP_EDITOR (fixture.editor))));
  g_assert_null (BindingStore::find (G_OBJECT (button)));
  gtk_button_clicked (GTK_BUTTON (button)); g_assert_cmpuint (normal, ==, 1);
  g_signal_emit_by_name (button, "extended-clicked", GDK_SHIFT_MASK); g_assert_cmpuint (modified, ==, 1);
  gimp_action_set_sensitive (alternate, FALSE, "Disabled");
  g_signal_emit_by_name (button, "extended-clicked", GDK_SHIFT_MASK); g_assert_cmpuint (modified, ==, 1);
  gimp_action_set_sensitive (main, FALSE, "Disabled"); g_assert_false (gtk_widget_get_sensitive (button));
  gimp_action_set_sensitive (main, TRUE, nullptr); g_assert_true (gtk_widget_get_sensitive (button));
  gchar *markup = gtk_widget_get_tooltip_markup (button);
  /* Sensitivity updates refresh native help, as they did before factoring. */
  g_assert_nonnull (strstr (markup, "Normal &lt;tip&gt;")); g_free (markup);
  g_assert_cmpstr (static_cast<const gchar *> (g_object_get_qdata (G_OBJECT (button), GIMP_HELP_ID)), ==, "action-help");
  g_object_ref (button); gtk_widget_destroy (fixture.editor);
  gimp_menu_factory_delete_manager (fixture.factory, "<LayerPresets>", fixture.editor); fixture.manager_deleted = true;
  g_object_unref (group);
  gtk_button_clicked (GTK_BUTTON (button)); g_signal_emit_by_name (button, "extended-clicked", GDK_SHIFT_MASK);
  g_assert_cmpuint (normal, ==, 1); g_assert_cmpuint (modified, ==, 1); g_object_unref (button);
}
/* Keep these C-provider controls independent of the C++ handle.  A missing
 * action tooltip is legitimate metadata, for both ordinary and split buttons.
 * Each case is separately registered so the unmodified providers can reproduce
 * the NULL-to-g_markup_escape_text diagnostic before the shared repair. */
static void nullable_action_tooltip (guint kind, bool primary_null) {
  Fixture fixture; guint normal = 0, modified = 0;
  auto *group = gimp_action_group_new (gimp, "nullable-action-tooltip", "Nullable tooltip", nullptr, nullptr, nullptr);
  gimp_ui_manager_add_action_group (gimp_editor_get_ui_manager (GIMP_EDITOR (fixture.editor)), group);
  action (group, "nullable-main", primary_null ? nullptr : "Main <tip>", &normal);
  action (group, "nullable-alternate", primary_null ? "Alternate <tip>" : nullptr, &modified);
  ObjectRef<GtkWidget> retained;
  if (kind == 2) retained = EditorRef::retain (GIMP_EDITOR (fixture.editor)).add_action_dropdown (
    nullptr, "nullable-action-tooltip", "nullable-main", "nullable-alternate", GDK_SHIFT_MASK);
  auto *button = kind == 2 ? retained.get () : kind == 0 ?
    gimp_editor_add_action_dropdown (GIMP_EDITOR (fixture.editor), nullptr, "nullable-action-tooltip", "nullable-main",
                                    "nullable-alternate", GDK_SHIFT_MASK, static_cast<const gchar *> (nullptr)) :
    gimp_editor_add_action_button (GIMP_EDITOR (fixture.editor), "nullable-action-tooltip", "nullable-main",
                                  "nullable-alternate", GDK_SHIFT_MASK, static_cast<const gchar *> (nullptr));
  g_assert_nonnull (button);
  gchar *markup = gtk_widget_get_tooltip_markup (button);
  if (primary_null) g_assert_null (markup);
  else g_assert_cmpstr (markup, ==, "Main &lt;tip&gt;");
  g_free (markup);
  if (kind != 1) {
    gchar *tooltip = gtk_widget_get_tooltip_text (fixture.arrow ());
    if (primary_null) g_assert_null (tooltip);
    else g_assert_cmpstr (tooltip, ==, "Main <tip>");
    g_free (tooltip);
  }
  g_assert_cmpstr (static_cast<const gchar *> (g_object_get_qdata (G_OBJECT (button), GIMP_HELP_ID)), ==, "action-help");
  gtk_button_clicked (GTK_BUTTON (button));
  g_signal_emit_by_name (button, "extended-clicked", GDK_SHIFT_MASK);
  g_assert_cmpuint (normal, ==, 1); g_assert_cmpuint (modified, ==, 1);
  g_object_unref (group);
}
static void toggle_action_shape () {
  Fixture fixture; guint alternate_calls = 0;
  auto *group = gimp_action_group_new (gimp, "editor-toggle-control", "Toggle", nullptr, nullptr, nullptr);
  gimp_ui_manager_add_action_group (gimp_editor_get_ui_manager (GIMP_EDITOR (fixture.editor)), group);
  auto *toggle = gimp_toggle_action_new ("dropdown-toggle", "Toggle", nullptr, "Toggle help", "document-new", "toggle-help", gimp->user_context);
  gimp_action_group_add_action (group, toggle);
  action (group, "toggle-alternate", "Alternate", &alternate_calls);
  auto *ordinary = gimp_editor_add_action_button (GIMP_EDITOR (fixture.editor), "editor-toggle-control", "dropdown-toggle", nullptr);
  g_assert_true (GTK_IS_TOGGLE_BUTTON (ordinary));
  auto *dropdown = gimp_editor_add_action_dropdown (GIMP_EDITOR (fixture.editor), nullptr, "editor-toggle-control", "dropdown-toggle", "toggle-alternate", GDK_SHIFT_MASK, nullptr);
  g_assert_true (GIMP_IS_BUTTON (dropdown)); g_assert_false (GTK_IS_TOGGLE_BUTTON (dropdown));
  g_assert_false (gimp_toggle_action_get_active (GIMP_TOGGLE_ACTION (toggle)));
  gtk_button_clicked (GTK_BUTTON (ordinary)); g_assert_true (gimp_toggle_action_get_active (GIMP_TOGGLE_ACTION (toggle)));
  gtk_button_clicked (GTK_BUTTON (dropdown)); g_assert_false (gimp_toggle_action_get_active (GIMP_TOGGLE_ACTION (toggle)));
  g_signal_emit_by_name (dropdown, "extended-clicked", GDK_SHIFT_MASK); g_assert_cmpuint (alternate_calls, ==, 1);
  g_object_unref (toggle); g_object_unref (group);
}
struct RemoveAction { GimpActionGroup *group; GimpAction *action; guint calls = 0; GtkWidget *primary = nullptr; };
static gboolean remove_action_during_construction (GSignalInvocationHint *, guint, const GValue *values, gpointer data) {
  auto *state = static_cast<RemoveAction *> (data);
  auto *widget = GTK_WIDGET (g_value_get_object (&values[0]));
  auto *parent = gtk_widget_get_parent (widget);
  if (!state->calls && GTK_IS_IMAGE (widget) && parent && GIMP_IS_BUTTON (parent)) {
    ++state->calls; state->primary = GTK_WIDGET (g_object_ref (parent));
    gimp_action_group_remove_action (state->group, state->action);
  }
  return TRUE;
}
static void action_loss_during_construction () {
  for (guint kind = 0; kind < 2; ++kind) {
    Fixture fixture; guint calls = 0;
    auto *group = gimp_action_group_new (gimp, "editor-action-loss", "Action loss", nullptr, nullptr, nullptr);
    gimp_ui_manager_add_action_group (gimp_editor_get_ui_manager (GIMP_EDITOR (fixture.editor)), group);
    auto *main = action (group, "action-loss-main", "Action loss", &calls);
    RemoveAction state {group, main};
    const guint signal = g_signal_lookup ("parent-set", GTK_TYPE_WIDGET);
    const gulong hook = g_signal_add_emission_hook (signal, 0, remove_action_during_construction, &state, nullptr);
    GtkWidget *result = kind == 0 ?
      gimp_editor_add_action_dropdown (GIMP_EDITOR (fixture.editor), nullptr, "editor-action-loss", "action-loss-main", nullptr) :
      gimp_editor_add_action_button (GIMP_EDITOR (fixture.editor), "editor-action-loss", "action-loss-main", nullptr);
    g_signal_remove_emission_hook (signal, hook);
    g_assert_cmpuint (state.calls, ==, 1); g_assert_null (result);
    gtk_button_clicked (GTK_BUTTON (state.primary)); g_assert_cmpuint (calls, ==, 0);
    g_object_unref (state.primary); g_object_unref (group);
  }
}
/* These tests exercise the existing public helper with real native action
 * ownership. A weak witness proves finalization before a retained live button
 * is used. No action internals, ExtendedAction layout or fake action are used.
 * kind 0 is the migrated dropdown; kind 1 controls its ordinary shared caller. */
static GtkWidget *lifetime_button (Fixture& fixture, guint kind, const char *group,
                                  const char *primary, const char *alternate) {
  return kind == 0 ?
    gimp_editor_add_action_dropdown (GIMP_EDITOR (fixture.editor), nullptr, group, primary,
                                    alternate, GDK_SHIFT_MASK, nullptr) :
    gimp_editor_add_action_button (GIMP_EDITOR (fixture.editor), group, primary,
                                  alternate, GDK_SHIFT_MASK, nullptr);
}
static void alternate_finalization_live_button (guint kind) {
  {
    g_test_message ("Finalized alternate: kind=%u", kind);
    Fixture fixture; guint normal = 0, modified = 0;
    auto *group = gimp_action_group_new (gimp, "alternate-lifetime", "Lifetime", nullptr, nullptr, nullptr);
    gimp_ui_manager_add_action_group (gimp_editor_get_ui_manager (GIMP_EDITOR (fixture.editor)), group);
    action (group, "lifetime-main", "Main", &normal);
    auto *alternate = action (group, "lifetime-alternate", "Alternate", &modified);
    GObject *weak_alternate = G_OBJECT (alternate);
    g_object_add_weak_pointer (weak_alternate, reinterpret_cast<gpointer *> (&weak_alternate));
    auto *button = lifetime_button (fixture, kind, "alternate-lifetime", "lifetime-main", "lifetime-alternate");
    g_assert_nonnull (button); g_object_ref (button);
    gtk_button_clicked (GTK_BUTTON (button));
    g_signal_emit_by_name (button, "extended-clicked", GDK_SHIFT_MASK);
    g_assert_cmpuint (normal, ==, 1); g_assert_cmpuint (modified, ==, 1);
    gimp_action_group_remove_action (group, alternate);
    g_assert_null (weak_alternate);
    g_test_message ("Finalization witness alternate_finalization_live_button:1 kind=%u", kind);
    g_assert_nonnull (gtk_widget_get_parent (button));
    g_assert_true (gtk_widget_get_sensitive (button));
    gtk_button_clicked (GTK_BUTTON (button)); g_assert_cmpuint (normal, ==, 2);
    /* Baseline red boundary: no replacement allocation can disguise this
     * already-proven finalized action. Fatal diagnostics remain enabled. */
    g_signal_emit_by_name (button, "extended-clicked", GDK_SHIFT_MASK);
    g_assert_cmpuint (modified, ==, 1); g_assert_cmpuint (normal, ==, 2);
    g_object_unref (button); g_object_unref (group);
  }
}
static void alternate_replacement_preserves_identity (guint kind) {
  {
    Fixture fixture; guint normal = 0, modified = 0, next_normal = 0, next_modified = 0;
    auto *group = gimp_action_group_new (gimp, "alternate-replacement", "Replacement", nullptr, nullptr, nullptr);
    gimp_ui_manager_add_action_group (gimp_editor_get_ui_manager (GIMP_EDITOR (fixture.editor)), group);
    auto *main = action (group, "replacement-main", "Main", &normal); g_object_ref (main);
    auto *alternate = action (group, "replacement-alternate", "Alternate", &modified); g_object_ref (alternate);
    GObject *weak_main = G_OBJECT (main), *weak_alternate = G_OBJECT (alternate);
    g_object_add_weak_pointer (weak_main, reinterpret_cast<gpointer *> (&weak_main));
    g_object_add_weak_pointer (weak_alternate, reinterpret_cast<gpointer *> (&weak_alternate));
    auto *button = lifetime_button (fixture, kind, "alternate-replacement", "replacement-main", "replacement-alternate");
    g_assert_nonnull (button);
    gimp_action_group_remove_action (group, main);
    gimp_action_group_remove_action (group, alternate);
    auto *next_main = action (group, "replacement-main", "Next main", &next_normal);
    auto *next_alternate = action (group, "replacement-alternate", "Next alternate", &next_modified);
    /* The old externally retained identities cannot share replacement storage.
     * Native primary proxies do not dynamically rebind by action name. */
    g_assert_true (next_main != main); g_assert_true (next_alternate != alternate);
    gimp_action_set_sensitive (main, FALSE, "Old primary disabled");
    g_assert_false (gtk_widget_get_sensitive (button));
    gimp_action_set_sensitive (main, TRUE, nullptr);
    gimp_action_set_sensitive (alternate, FALSE, "Old alternate disabled");
    gtk_button_clicked (GTK_BUTTON (button));
    g_signal_emit_by_name (button, "extended-clicked", GDK_SHIFT_MASK);
    g_assert_cmpuint (normal, ==, 1); g_assert_cmpuint (modified, ==, 0);
    g_assert_cmpuint (next_normal, ==, 0); g_assert_cmpuint (next_modified, ==, 0);
    gimp_action_set_sensitive (alternate, TRUE, nullptr);
    g_signal_emit_by_name (button, "extended-clicked", GDK_SHIFT_MASK);
    g_assert_cmpuint (modified, ==, 1);
    g_object_unref (main); g_object_unref (alternate);
    g_assert_null (weak_main); g_assert_null (weak_alternate);
    g_test_message ("Finalization witness alternate_replacement_preserves_identity:1 kind=%u", kind);
    gtk_button_clicked (GTK_BUTTON (button));
    g_signal_emit_by_name (button, "extended-clicked", GDK_SHIFT_MASK);
    g_assert_cmpuint (normal, ==, 1); g_assert_cmpuint (modified, ==, 1);
    g_assert_cmpuint (next_normal, ==, 0); g_assert_cmpuint (next_modified, ==, 0);
    /* New controls independently bind the newly registered identities. */
    auto *next_button = lifetime_button (fixture, kind, "alternate-replacement", "replacement-main", "replacement-alternate");
    g_assert_nonnull (next_button);
    gtk_button_clicked (GTK_BUTTON (next_button));
    g_signal_emit_by_name (next_button, "extended-clicked", GDK_SHIFT_MASK);
    g_assert_cmpuint (next_normal, ==, 1); g_assert_cmpuint (next_modified, ==, 1);
    g_object_unref (group);
  }
}
static void alternate_loss_preserves_precedence (guint kind) {
  {
    Fixture fixture; guint normal = 0, shift = 0, control = 0;
    auto *group = gimp_action_group_new (gimp, "alternate-precedence", "Precedence", nullptr, nullptr, nullptr);
    gimp_ui_manager_add_action_group (gimp_editor_get_ui_manager (GIMP_EDITOR (fixture.editor)), group);
    action (group, "precedence-main", "Main", &normal);
    auto *shift_action = action (group, "precedence-shift", "Shift", &shift);
    auto *control_action = action (group, "precedence-control", "Control", &control);
    GObject *weak_shift = G_OBJECT (shift_action), *weak_control = G_OBJECT (control_action);
    g_object_add_weak_pointer (weak_shift, reinterpret_cast<gpointer *> (&weak_shift));
    g_object_add_weak_pointer (weak_control, reinterpret_cast<gpointer *> (&weak_control));
    auto *button = kind == 0 ?
      gimp_editor_add_action_dropdown (GIMP_EDITOR (fixture.editor), nullptr, "alternate-precedence", "precedence-main",
                                      "precedence-shift", GDK_SHIFT_MASK, "precedence-control", GDK_CONTROL_MASK, nullptr) :
      gimp_editor_add_action_button (GIMP_EDITOR (fixture.editor), "alternate-precedence", "precedence-main",
                                    "precedence-shift", GDK_SHIFT_MASK, "precedence-control", GDK_CONTROL_MASK, nullptr);
    g_assert_nonnull (button);
    g_signal_emit_by_name (button, "extended-clicked", GDK_SHIFT_MASK | GDK_CONTROL_MASK);
    g_assert_cmpuint (control, ==, 1); g_assert_cmpuint (shift, ==, 0);
    gimp_action_set_sensitive (control_action, FALSE, "Control disabled");
    g_signal_emit_by_name (button, "extended-clicked", GDK_SHIFT_MASK | GDK_CONTROL_MASK);
    g_assert_cmpuint (control, ==, 1); g_assert_cmpuint (shift, ==, 1);
    gimp_action_group_remove_action (group, control_action); g_assert_null (weak_control);
    g_test_message ("Finalization witness alternate_loss_preserves_precedence:1 kind=%u", kind);
    g_signal_emit_by_name (button, "extended-clicked", GDK_SHIFT_MASK | GDK_CONTROL_MASK);
    g_assert_cmpuint (control, ==, 1); g_assert_cmpuint (shift, ==, 2);
    gimp_action_set_sensitive (shift_action, FALSE, "Shift disabled");
    g_signal_emit_by_name (button, "extended-clicked", GDK_SHIFT_MASK | GDK_CONTROL_MASK);
    g_assert_cmpuint (shift, ==, 2);
    gimp_action_group_remove_action (group, shift_action); g_assert_null (weak_shift);
    g_test_message ("Finalization witness alternate_loss_preserves_precedence:2 kind=%u", kind);
    g_signal_emit_by_name (button, "extended-clicked", GDK_SHIFT_MASK | GDK_CONTROL_MASK);
    gtk_button_clicked (GTK_BUTTON (button));
    g_assert_cmpuint (normal, ==, 1); g_assert_cmpuint (control, ==, 1); g_assert_cmpuint (shift, ==, 2);
    g_object_unref (group);
  }
}
struct RemoveActivatingAlternate {
  GimpActionGroup *group;
  GtkWidget *button;
  bool destroy;
  guint calls = 0;
};
static void remove_activating_alternate (GimpAction *action, GVariant *, gpointer data) {
  auto *state = static_cast<RemoveActivatingAlternate *> (data);
  ++state->calls;
  /* Disable before removal so even a temporary native signal-emission lease
   * cannot legitimately re-enter this same action on the nested click. */
  gimp_action_set_sensitive (action, FALSE, "Removing active alternate");
  gimp_action_group_remove_action (state->group, action);
  if (state->destroy) gtk_widget_destroy (state->button);
  g_signal_emit_by_name (state->button, "extended-clicked", GDK_SHIFT_MASK);
}
static void alternate_self_removal_and_owner_loss (guint kind, guint destroy) {
  {
    {
      Fixture fixture; guint normal = 0, modified = 0;
      auto *group = gimp_action_group_new (gimp, "alternate-self-removal", "Self removal", nullptr, nullptr, nullptr);
      gimp_ui_manager_add_action_group (gimp_editor_get_ui_manager (GIMP_EDITOR (fixture.editor)), group);
      action (group, "self-removal-main", "Main", &normal);
      auto *alternate = action (group, "self-removal-alternate", "Alternate", &modified);
      GObject *weak_alternate = G_OBJECT (alternate);
      g_object_add_weak_pointer (weak_alternate, reinterpret_cast<gpointer *> (&weak_alternate));
      auto *button = lifetime_button (fixture, kind, "alternate-self-removal", "self-removal-main", "self-removal-alternate");
      g_assert_nonnull (button); g_object_ref (button);
      RemoveActivatingAlternate state {group, button, destroy != 0};
      g_signal_connect (alternate, "activate", G_CALLBACK (remove_activating_alternate), &state);
      gtk_button_clicked (GTK_BUTTON (button));
      g_signal_emit_by_name (button, "extended-clicked", GDK_SHIFT_MASK);
      g_assert_cmpuint (modified, ==, 1); g_assert_cmpuint (state.calls, ==, 1);
      g_assert_null (weak_alternate);
      g_test_message ("Finalization witness alternate_self_removal_and_owner_loss:1 kind=%u", kind);
      gtk_button_clicked (GTK_BUTTON (button));
      g_signal_emit_by_name (button, "extended-clicked", GDK_SHIFT_MASK);
      g_assert_cmpuint (modified, ==, 1); g_assert_cmpuint (state.calls, ==, 1);
      g_assert_cmpuint (normal, ==, destroy ? 1 : 2);
      g_object_unref (button); g_object_unref (group);
    }
  }
}
static void alternate_manager_loss_live_button (guint kind) {
  {
    Fixture fixture; guint normal = 0, modified = 0, next_normal = 0, next_modified = 0;
    auto *manager = gimp_editor_get_ui_manager (GIMP_EDITOR (fixture.editor));
    GObject *weak_manager = G_OBJECT (manager);
    g_object_add_weak_pointer (weak_manager, reinterpret_cast<gpointer *> (&weak_manager));
    auto *group = gimp_action_group_new (gimp, "alternate-manager-loss", "Manager loss", nullptr, nullptr, nullptr);
    gimp_ui_manager_add_action_group (manager, group);
    GObject *weak_main = G_OBJECT (action (group, "manager-loss-main", "Main", &normal));
    GObject *weak_alternate = G_OBJECT (action (group, "manager-loss-alternate", "Alternate", &modified));
    g_object_add_weak_pointer (weak_main, reinterpret_cast<gpointer *> (&weak_main));
    g_object_add_weak_pointer (weak_alternate, reinterpret_cast<gpointer *> (&weak_alternate));
    auto *button = lifetime_button (fixture, kind, "alternate-manager-loss", "manager-loss-main", "manager-loss-alternate");
    g_assert_nonnull (button); g_object_ref (button); g_object_unref (group);
    gtk_button_clicked (GTK_BUTTON (button));
    g_signal_emit_by_name (button, "extended-clicked", GDK_SHIFT_MASK);
    g_assert_cmpuint (normal, ==, 1); g_assert_cmpuint (modified, ==, 1);
    gimp_menu_factory_delete_manager (fixture.factory, "<LayerPresets>", fixture.editor); fixture.manager_deleted = true;
    g_assert_null (weak_manager); g_assert_null (weak_main); g_assert_null (weak_alternate);
    g_test_message ("Finalization witness alternate_manager_loss_live_button:1 kind=%u", kind);
    g_assert_null (gimp_editor_get_ui_manager (GIMP_EDITOR (fixture.editor)));
    g_assert_nonnull (gtk_widget_get_parent (button));
    gtk_button_clicked (GTK_BUTTON (button));
    g_signal_emit_by_name (button, "extended-clicked", GDK_SHIFT_MASK);
    g_assert_cmpuint (normal, ==, 1); g_assert_cmpuint (modified, ==, 1);
    gimp_editor_create_menu (GIMP_EDITOR (fixture.editor), fixture.factory, "<LayerPresets>", "/layer-presets-popup", fixture.editor);
    fixture.manager_deleted = false;
    auto *next_group = gimp_action_group_new (gimp, "alternate-manager-loss", "Next manager", nullptr, nullptr, nullptr);
    gimp_ui_manager_add_action_group (gimp_editor_get_ui_manager (GIMP_EDITOR (fixture.editor)), next_group);
    action (next_group, "manager-loss-main", "Next main", &next_normal);
    action (next_group, "manager-loss-alternate", "Next alternate", &next_modified);
    gtk_button_clicked (GTK_BUTTON (button));
    g_signal_emit_by_name (button, "extended-clicked", GDK_SHIFT_MASK);
    g_assert_cmpuint (normal, ==, 1); g_assert_cmpuint (modified, ==, 1);
    g_assert_cmpuint (next_normal, ==, 0); g_assert_cmpuint (next_modified, ==, 0);
    g_object_unref (button); g_object_unref (next_group);
  }
}
static void alternate_retained_destroyed_button (guint kind) {
  {
    Fixture fixture; guint normal = 0, modified = 0;
    auto *group = gimp_action_group_new (gimp, "alternate-destroyed", "Destroyed", nullptr, nullptr, nullptr);
    gimp_ui_manager_add_action_group (gimp_editor_get_ui_manager (GIMP_EDITOR (fixture.editor)), group);
    auto *main = action (group, "destroyed-main", "Main", &normal);
    auto *alternate = action (group, "destroyed-alternate", "Alternate", &modified);
    GObject *weak_alternate = G_OBJECT (alternate);
    g_object_add_weak_pointer (weak_alternate, reinterpret_cast<gpointer *> (&weak_alternate));
    auto *button = lifetime_button (fixture, kind, "alternate-destroyed", "destroyed-main", "destroyed-alternate");
    g_assert_nonnull (button); g_object_ref (button);
    gtk_widget_destroy (button);
    gimp_action_set_sensitive (main, FALSE, "Destroyed proxy must be detached");
    g_assert_true (gtk_widget_get_sensitive (button));
    gtk_button_clicked (GTK_BUTTON (button));
    g_signal_emit_by_name (button, "extended-clicked", GDK_SHIFT_MASK);
    g_assert_cmpuint (normal, ==, 0); g_assert_cmpuint (modified, ==, 0);
    gimp_action_group_remove_action (group, alternate); g_assert_null (weak_alternate);
    g_test_message ("Finalization witness alternate_retained_destroyed_button:1 kind=%u", kind);
    gtk_button_clicked (GTK_BUTTON (button));
    g_signal_emit_by_name (button, "extended-clicked", GDK_SHIFT_MASK);
    g_assert_cmpuint (normal, ==, 0); g_assert_cmpuint (modified, ==, 0);
    g_object_unref (button); g_object_unref (group);
  }
}

static void missing_context_can_reopen () {
  Fixture fixture; generic (fixture); auto *arrow = fixture.arrow ();
  auto *popup = GTK_WIDGET (gtk_menu_button_get_popover (GTK_MENU_BUTTON (arrow)));
  gimp_container_view_set_context (GIMP_CONTAINER_VIEW (fixture.editor), nullptr);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (arrow), TRUE); drain ();
  g_assert_null (gtk_bin_get_child (GTK_BIN (popup)));
  gtk_popover_popdown (GTK_POPOVER (popup)); drain ();
  gimp_container_view_set_context (GIMP_CONTAINER_VIEW (fixture.editor), gimp->user_context);
  auto *view = open (arrow);
  g_assert_true (gimp_container_view_get_context (GIMP_CONTAINER_EDITOR (view)->view) == gimp->user_context);
}
static void completed_popup_cycles () {
  Fixture fixture; generic (fixture); auto *arrow = fixture.arrow ();
  auto *popup = GTK_WIDGET (gtk_menu_button_get_popover (GTK_MENU_BUTTON (arrow)));
  for (guint i = 0; i < 3; ++i) {
    auto *view = open (arrow); g_object_ref (view);
    gtk_popover_popdown (GTK_POPOVER (popup));
    /* Retirement is synchronous even when native visual hiding is not. */
    g_assert_false (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (arrow)));
    g_assert_null (gtk_bin_get_child (GTK_BIN (popup)));
    g_assert_false (gimp_layer_preset_view_can_apply (GIMP_LAYER_PRESET_VIEW (view)));
    g_object_unref (view);
    wait_for ("native popup hidden", [&] { return !gtk_widget_get_visible (popup); });
    g_assert_false (gtk_widget_get_mapped (popup));
  }
}
static void rapid_reopen_changes_context () {
  Fixture fixture; generic (fixture); auto *arrow = fixture.arrow ();
  auto *popup = GTK_WIDGET (gtk_menu_button_get_popover (GTK_MENU_BUTTON (arrow)));
  auto *context = gimp_context_new (gimp, "Rapid reopen context", nullptr);
  for (guint i = 0; i < 3; ++i) {
    auto *old_view = open (arrow); g_object_ref (old_view);
    gtk_popover_popdown (GTK_POPOVER (popup));
    g_assert_false (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (arrow)));
    g_assert_null (gtk_bin_get_child (GTK_BIN (popup)));
    g_assert_false (gimp_layer_preset_view_can_apply (GIMP_LAYER_PRESET_VIEW (old_view)));
    auto *next_context = i % 2 ? gimp->user_context : context;
    gimp_container_view_set_context (GIMP_CONTAINER_VIEW (fixture.editor), next_context);
    OpeningTransition transition;
    const auto shown = g_signal_connect (popup, "show", G_CALLBACK (opening_shown), &transition);
    const auto handler = g_signal_connect (popup, "notify::opacity", G_CALLBACK (opening_opacity), &transition);
    /* No main-loop iteration between popdown and the new user activation. */
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (arrow), TRUE);
    auto *current = gtk_bin_get_child (GTK_BIN (popup));
    g_assert_true (GIMP_IS_LAYER_PRESET_VIEW (current));
    g_assert_true (current != old_view);
    g_assert_true (gimp_container_view_get_context (GIMP_CONTAINER_EDITOR (current)->view) == next_context);
    g_assert_true (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (arrow)));
    finish_opening (popup, transition);
    g_signal_handler_disconnect (popup, handler);
    g_signal_handler_disconnect (popup, shown);
    assert_only_child (popup, current);
    g_assert_true (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (arrow)));
    g_assert_false (gimp_layer_preset_view_can_apply (GIMP_LAYER_PRESET_VIEW (old_view)));
    g_object_unref (old_view);
  }
  BindingStore::require (G_OBJECT (arrow)).close ();
  g_object_unref (context);
}
struct ReopenFromHide {
  GtkWidget *editor;
  GimpContext *context;
  GtkWidget *created = nullptr;
  guint calls = 0;
};
static void reopen_from_native_hide (GtkWidget *popup, gpointer data) {
  auto *state = static_cast<ReopenFromHide *> (data);
  if (state->calls++) return;
  gimp_container_view_set_context (GIMP_CONTAINER_VIEW (state->editor), state->context);
  gtk_popover_popup (GTK_POPOVER (popup));
  state->created = gtk_bin_get_child (GTK_BIN (popup));
}
static void interrupted_hide_preserves_newer_presentation () {
  Fixture fixture; generic (fixture); auto *arrow = fixture.arrow ();
  auto *popup = GTK_WIDGET (gtk_menu_button_get_popover (GTK_MENU_BUTTON (arrow)));
  open (arrow);
  gtk_popover_popdown (GTK_POPOVER (popup));
  auto *context = gimp_context_new (gimp, "Reentrant native hide context", nullptr);
  const bool interrupted = gtk_widget_get_visible (popup);
  ReopenFromHide state {fixture.editor, context};
  const auto hidden = g_signal_connect (popup, "hide", G_CALLBACK (reopen_from_native_hide), &state);
  OpeningTransition transition;
  const auto shown = g_signal_connect (popup, "show", G_CALLBACK (opening_shown), &transition);
  const auto opacity = g_signal_connect (popup, "notify::opacity", G_CALLBACK (opening_opacity), &transition);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (arrow), TRUE);
  /* Native animations may be disabled by the platform, in which case the
   * earlier popdown already finished and the corrective hide is unnecessary. */
  auto *current = gtk_bin_get_child (GTK_BIN (popup));
  g_assert_true (GIMP_IS_LAYER_PRESET_VIEW (current));
  g_assert_cmpuint (state.calls, ==, interrupted ? 1 : 0);
  if (state.calls) {
    g_assert_cmpuint (state.calls, ==, 1);
    g_assert_true (current == state.created);
    g_assert_true (gimp_container_view_get_context (GIMP_CONTAINER_EDITOR (current)->view) == context);
  }
  finish_opening (popup, transition);
  g_signal_handler_disconnect (popup, opacity);
  g_signal_handler_disconnect (popup, shown);
  g_signal_handler_disconnect (popup, hidden);
  assert_only_child (popup, current);
  g_assert_true (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (arrow)));
  BindingStore::require (G_OBJECT (arrow)).close ();
  g_object_unref (context);
}
static void destroy_from_native_hide (GtkWidget *, gpointer data) {
  auto *state = static_cast<Destruction *> (data);
  if (state->calls++ == 0) gtk_widget_destroy (state->target);
}
static void interrupted_hide_owner_loss () {
  for (guint mode = 0; mode < 3; ++mode) {
    Fixture fixture; auto *button = generic (fixture); auto *arrow = fixture.arrow ();
    auto *popup = GTK_WIDGET (gtk_menu_button_get_popover (GTK_MENU_BUTTON (arrow)));
    g_object_ref (button); g_object_ref (arrow); g_object_ref (popup);
    open (arrow); gtk_popover_popdown (GTK_POPOVER (popup));
    const bool interrupted = gtk_widget_get_visible (popup);
    Destruction state {mode == 0 ? fixture.editor : mode == 1 ? button : arrow};
    const auto handler = g_signal_connect (popup, "hide", G_CALLBACK (destroy_from_native_hide), &state);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (arrow), TRUE);
    if (interrupted) {
      g_assert_cmpuint (state.calls, ==, 1);
      g_assert_true (BindingStore::require (G_OBJECT (arrow)).state () == BindingStore::State::closed);
      g_assert_null (gtk_bin_get_child (GTK_BIN (popup)));
      g_signal_emit_by_name (popup, "show"); g_signal_emit_by_name (popup, "closed");
      g_assert_null (gtk_bin_get_child (GTK_BIN (popup)));
    }
    if (g_signal_handler_is_connected (popup, handler)) g_signal_handler_disconnect (popup, handler);
    g_object_unref (popup); g_object_unref (arrow); g_object_unref (button);
  }
}
struct CancelReopen {
  GtkWidget *arrow;
  guint calls = 0;
};
static void cancel_from_native_hide (GtkWidget *, gpointer data) {
  auto *state = static_cast<CancelReopen *> (data);
  ++state->calls;
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (state->arrow), FALSE);
}
static void interrupted_hide_respects_cancel () {
  Fixture fixture; generic (fixture); auto *arrow = fixture.arrow ();
  auto *popup = GTK_WIDGET (gtk_menu_button_get_popover (GTK_MENU_BUTTON (arrow)));
  open (arrow); gtk_popover_popdown (GTK_POPOVER (popup));
  const bool interrupted = gtk_widget_get_visible (popup);
  CancelReopen state {arrow};
  const auto handler = g_signal_connect (popup, "hide", G_CALLBACK (cancel_from_native_hide), &state);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (arrow), TRUE);
  g_assert_cmpuint (state.calls, ==, interrupted ? 1 : 0);
  if (interrupted) {
    g_assert_false (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (arrow)));
    g_assert_false (gtk_widget_get_visible (popup));
    g_assert_false (gtk_widget_get_mapped (popup));
    g_assert_null (gtk_bin_get_child (GTK_BIN (popup)));
  }
  g_signal_handler_disconnect (popup, handler);
  /* Cancellation does not poison the next independent activation. */
  auto *current = open (arrow); assert_only_child (popup, current);
}
struct ReopenFromClosed {
  GtkWidget *editor, *arrow;
  GimpContext *context;
  guint calls = 0;
};
static void activate_from_retired_view (GtkWidget *, gpointer data) {
  auto *state = static_cast<ReopenFromClosed *> (data);
  ++state->calls;
  gimp_container_view_set_context (GIMP_CONTAINER_VIEW (state->editor), state->context);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (state->arrow), TRUE);
}
static void closed_retirement_reopens_newer_context () {
  Fixture fixture; generic (fixture); auto *arrow = fixture.arrow ();
  auto *popup = GTK_WIDGET (gtk_menu_button_get_popover (GTK_MENU_BUTTON (arrow)));
  auto *old_view = open (arrow); g_object_ref (old_view);
  auto *context = gimp_context_new (gimp, "Closed retirement context", nullptr);
  ReopenFromClosed state {fixture.editor, arrow, context};
  g_signal_connect (old_view, "destroy", G_CALLBACK (activate_from_retired_view), &state);
  OpeningTransition transition;
  const auto shown = g_signal_connect (popup, "show", G_CALLBACK (opening_shown), &transition);
  const auto opacity = g_signal_connect (popup, "notify::opacity", G_CALLBACK (opening_opacity), &transition);
  gtk_popover_popdown (GTK_POPOVER (popup));
  g_assert_cmpuint (state.calls, ==, 1);
  g_assert_true (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (arrow)));
  g_assert_null (gtk_bin_get_child (GTK_BIN (popup)));
  g_assert_false (gimp_layer_preset_view_can_apply (GIMP_LAYER_PRESET_VIEW (old_view)));
  drain ();
  auto *current = gtk_bin_get_child (GTK_BIN (popup));
  g_assert_true (GIMP_IS_LAYER_PRESET_VIEW (current));
  g_assert_true (current != old_view);
  g_assert_true (gimp_container_view_get_context (GIMP_CONTAINER_EDITOR (current)->view) == context);
  finish_opening (popup, transition);
  g_signal_handler_disconnect (popup, opacity);
  g_signal_handler_disconnect (popup, shown);
  assert_only_child (popup, current);
  g_assert_true (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (arrow)));
  g_assert_true (gtk_grab_get_current () == popup);
  g_object_unref (old_view);
  BindingStore::require (G_OBJECT (arrow)).close ();
  g_object_unref (context);
}
static void closed_retirement_respects_cancel_and_owner_loss () {
  for (guint mode = 0; mode < 4; ++mode) {
    Fixture fixture; auto *button = generic (fixture); auto *arrow = fixture.arrow ();
    auto *popup = GTK_WIDGET (gtk_menu_button_get_popover (GTK_MENU_BUTTON (arrow)));
    auto *old_view = open (arrow); g_object_ref (old_view);
    g_object_ref (button); g_object_ref (arrow); g_object_ref (popup);
    ReopenFromClosed state {fixture.editor, arrow, gimp->user_context};
    g_signal_connect (old_view, "destroy", G_CALLBACK (activate_from_retired_view), &state);
    gtk_popover_popdown (GTK_POPOVER (popup));
    g_assert_cmpuint (state.calls, ==, 1);
    g_assert_true (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (arrow)));
    if (mode == 0) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (arrow), FALSE);
    else gtk_widget_destroy (mode == 1 ? fixture.editor : mode == 2 ? button : arrow);
    drain ();
    g_assert_null (gtk_bin_get_child (GTK_BIN (popup)));
    g_assert_false (gimp_layer_preset_view_can_apply (GIMP_LAYER_PRESET_VIEW (old_view)));
    if (mode == 0) {
      wait_for ("cancelled popup hidden", [&] { return !gtk_widget_get_visible (popup); });
      g_assert_false (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (arrow)));
      open (arrow);
    } else {
      g_assert_true (BindingStore::require (G_OBJECT (arrow)).state () == BindingStore::State::closed);
    }
    g_object_unref (old_view); g_object_unref (popup); g_object_unref (arrow); g_object_unref (button);
  }
}
static void closed_retirement_newer_open_wins () {
  Fixture fixture; generic (fixture); auto *arrow = fixture.arrow ();
  auto *popup = GTK_WIDGET (gtk_menu_button_get_popover (GTK_MENU_BUTTON (arrow)));
  auto *old_view = open (arrow);
  ReopenFromClosed state {fixture.editor, arrow, gimp->user_context};
  g_signal_connect (old_view, "destroy", G_CALLBACK (activate_from_retired_view), &state);
  gtk_popover_popdown (GTK_POPOVER (popup));
  g_assert_cmpuint (state.calls, ==, 1);
  gtk_widget_hide (popup);
  auto *context = gimp_context_new (gimp, "Newer than deferred activation", nullptr);
  gimp_container_view_set_context (GIMP_CONTAINER_VIEW (fixture.editor), context);
  OpeningTransition transition;
  const auto shown = g_signal_connect (popup, "show", G_CALLBACK (opening_shown), &transition);
  const auto opacity = g_signal_connect (popup, "notify::opacity", G_CALLBACK (opening_opacity), &transition);
  gtk_popover_popup (GTK_POPOVER (popup));
  auto *current = gtk_bin_get_child (GTK_BIN (popup));
  g_assert_true (GIMP_IS_LAYER_PRESET_VIEW (current));
  g_assert_true (gimp_container_view_get_context (GIMP_CONTAINER_EDITOR (current)->view) == context);
  drain ();
  assert_only_child (popup, current);
  finish_opening (popup, transition);
  g_signal_handler_disconnect (popup, opacity);
  g_signal_handler_disconnect (popup, shown);
  assert_only_child (popup, current);
  g_assert_true (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (arrow)));
  g_assert_true (gtk_grab_get_current () == popup);
  BindingStore::require (G_OBJECT (arrow)).close ();
  g_object_unref (context);
}
/* A C control takes its own explicit lease; the handle must return that same
 * ownership without transferring the GTK parent's existing reference. */
static ObjectRef<GtkWidget> owned_generic (Fixture& fixture, bool handle,
                                          GCallback callback = nullptr,
                                          GCallback extended_callback = nullptr,
                                          gpointer data = nullptr) {
  if (handle)
    return EditorRef::retain (GIMP_EDITOR (fixture.editor)).add_dropdown (
      "document-new", "Create a layer", "dropdown-help", "deliberately-ignored",
      callback, extended_callback, data);
  return ObjectRef<GtkWidget>::retain (generic (fixture, callback, extended_callback, data));
}
template<class... Alternates>
static ObjectRef<GtkWidget> owned_action_dropdown (Fixture& fixture, bool handle,
                                                   const gchar *group, const gchar *primary,
                                                   Alternates... alternates) {
  if (handle)
    return EditorRef::retain (GIMP_EDITOR (fixture.editor)).add_action_dropdown (
      "deliberately-ignored", group, primary, alternates...);
  return ObjectRef<GtkWidget>::retain (gimp_editor_add_action_dropdown (
    GIMP_EDITOR (fixture.editor), "deliberately-ignored", group, primary,
    alternates..., static_cast<const gchar *> (nullptr)));
}
static void assert_dropdown_shape (Fixture& fixture, GtkWidget *primary) {
  g_assert_nonnull (primary); g_assert_true (GIMP_IS_BUTTON (primary));
  auto *box = gimp_editor_get_button_box (GIMP_EDITOR (fixture.editor));
  auto *arrow = fixture.arrow ();
  g_assert_true (primary != arrow); g_assert_true (gtk_widget_get_visible (primary));
  g_assert_true (gtk_widget_get_parent (primary) == GTK_WIDGET (box));
  GList *children = gtk_container_get_children (GTK_CONTAINER (box));
  g_assert_cmpuint (g_list_length (children), ==, 2);
  g_assert_true (children->data == primary); g_assert_true (children->next->data == arrow);
  g_list_free (children);
  g_assert_null (BindingStore::find (G_OBJECT (fixture.editor)));
  g_assert_null (BindingStore::find (G_OBJECT (primary)));
  g_assert_true (BindingStore::require (G_OBJECT (arrow)).state () == BindingStore::State::active);
}
static void handle_generic_parity () {
  for (bool handle : {false, true}) {
    Fixture fixture; Calls calls;
    auto primary = owned_generic (fixture, handle, G_CALLBACK (clicked), G_CALLBACK (extended), &calls);
    assert_dropdown_shape (fixture, primary.get ());
    for (auto *widget : {primary.get (), fixture.arrow ()}) {
      gchar *tooltip = gtk_widget_get_tooltip_text (widget);
      g_assert_cmpstr (tooltip, ==, "Create a layer"); g_free (tooltip);
      g_assert_cmpstr (static_cast<const gchar *> (g_object_get_qdata (G_OBJECT (widget), GIMP_HELP_ID)), ==, "dropdown-help");
    }
    gtk_button_clicked (GTK_BUTTON (primary.get ()));
    g_signal_emit_by_name (primary.get (), "extended-clicked", GDK_CONTROL_MASK);
    g_assert_cmpuint (calls.normal, ==, 1); g_assert_cmpuint (calls.extended, ==, 1);
    g_assert_cmpint (calls.mask, ==, GDK_CONTROL_MASK);
    gtk_widget_destroy (fixture.editor);
    gtk_button_clicked (GTK_BUTTON (primary.get ()));
    g_signal_emit_by_name (primary.get (), "extended-clicked", GDK_SHIFT_MASK);
    g_assert_cmpuint (calls.normal, ==, 1); g_assert_cmpuint (calls.extended, ==, 1);
  }
}
static void handle_editor_ref_ownership () {
  Fixture fixture;
  auto *editor = GIMP_EDITOR (fixture.editor);
  const guint before = G_OBJECT (editor)->ref_count;
  {
    auto retained = EditorRef::retain (editor);
    g_assert_true (retained.get () == editor);
    g_assert_cmpuint (G_OBJECT (editor)->ref_count, ==, before + 1);
    {
      auto copied = retained;
      g_assert_cmpuint (G_OBJECT (editor)->ref_count, ==, before + 2);
      auto moved = std::move (copied);
      g_assert_true (moved.get () == editor); g_assert_null (copied.get ());
      g_assert_cmpuint (G_OBJECT (editor)->ref_count, ==, before + 2);
    }
    g_assert_cmpuint (G_OBJECT (editor)->ref_count, ==, before + 1);
    g_assert_null (BindingStore::find (G_OBJECT (editor)));
  }
  g_assert_cmpuint (G_OBJECT (editor)->ref_count, ==, before);
}
static void handle_primary_ref_ownership (bool action_backed, bool release_first) {
  Fixture fixture; guint normal = 0;
  auto *group = gimp_action_group_new (gimp, "handle-ownership", "Ownership", nullptr, nullptr, nullptr);
  gimp_ui_manager_add_action_group (gimp_editor_get_ui_manager (GIMP_EDITOR (fixture.editor)), group);
  action (group, "ownership-main", "Ownership help", &normal); g_object_unref (group);
  auto *control = action_backed ?
    gimp_editor_add_action_dropdown (GIMP_EDITOR (fixture.editor), nullptr, "handle-ownership", "ownership-main", static_cast<const gchar *> (nullptr)) :
    generic (fixture);
  g_assert_nonnull (control);
  const guint native_refs = G_OBJECT (control)->ref_count;
  auto editor = EditorRef::retain (GIMP_EDITOR (fixture.editor));
  auto primary = action_backed ? editor.add_action_dropdown (nullptr, "handle-ownership", "ownership-main") :
    editor.add_dropdown ("document-new", "Ownership help", nullptr, nullptr, nullptr, nullptr, nullptr);
  g_assert_nonnull (primary.get ());
  g_assert_cmpuint (G_OBJECT (primary.get ())->ref_count, ==, native_refs + 1);
  GObject *weak_primary = G_OBJECT (primary.get ()), *weak_arrow = G_OBJECT (fixture.arrow ());
  g_object_add_weak_pointer (weak_primary, reinterpret_cast<gpointer *> (&weak_primary));
  g_object_add_weak_pointer (weak_arrow, reinterpret_cast<gpointer *> (&weak_arrow));
  {
    auto copied = primary;
    g_assert_cmpuint (G_OBJECT (primary.get ())->ref_count, ==, native_refs + 2);
    auto moved = std::move (copied);
    g_assert_null (copied.get ()); g_assert_true (moved.get () == primary.get ());
    g_assert_cmpuint (G_OBJECT (primary.get ())->ref_count, ==, native_refs + 2);
  }
  g_assert_cmpuint (G_OBJECT (primary.get ())->ref_count, ==, native_refs + 1);
  if (release_first) {
    primary.reset ();
    g_assert_nonnull (weak_primary);
    g_assert_cmpuint (weak_primary->ref_count, ==, native_refs);
    g_assert_nonnull (gtk_widget_get_parent (GTK_WIDGET (weak_primary)));
    gtk_widget_destroy (fixture.editor);
    g_assert_null (weak_primary); g_assert_null (weak_arrow);
  } else {
    /* GTK disposal sends weak notifications even while strong storage leases
     * survive. Use the owned pointers and exact counts as the storage witness,
     * and compare with an explicitly retained C-provider control. */
    auto control_lease = ObjectRef<GtkWidget>::retain (control);
    GObject *weak_control = G_OBJECT (control);
    g_object_add_weak_pointer (weak_control, reinterpret_cast<gpointer *> (&weak_control));
    gtk_widget_destroy (fixture.editor);
    g_assert_null (weak_primary); g_assert_null (weak_control); g_assert_null (weak_arrow);
    g_assert_true (GTK_IS_WIDGET (primary.get ())); g_assert_true (GTK_IS_WIDGET (control_lease.get ()));
    g_assert_null (gtk_widget_get_parent (primary.get ()));
    g_assert_null (gtk_widget_get_parent (control_lease.get ()));
    g_assert_cmpuint (G_OBJECT (primary.get ())->ref_count, ==, 1);
    g_assert_cmpuint (G_OBJECT (control_lease.get ())->ref_count, ==, 1);
    /* Register fresh notifications after disposal, then drop the sole refs. */
    weak_primary = G_OBJECT (primary.get ()); weak_control = G_OBJECT (control_lease.get ());
    g_object_add_weak_pointer (weak_primary, reinterpret_cast<gpointer *> (&weak_primary));
    g_object_add_weak_pointer (weak_control, reinterpret_cast<gpointer *> (&weak_control));
    control_lease.reset (); g_assert_null (weak_control); g_assert_nonnull (weak_primary);
    primary.reset (); g_assert_null (weak_primary);
  }
}
static void handle_outlives_fixture (bool editor_first) {
  ObjectRef<GtkWidget> primary;
  std::unique_ptr<EditorRef> editor;
  GObject *weak_editor = nullptr, *weak_primary = nullptr, *weak_arrow = nullptr;
  {
    Fixture fixture;
    editor.reset (new EditorRef (EditorRef::retain (GIMP_EDITOR (fixture.editor))));
    primary = editor->add_dropdown ("document-new", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    weak_editor = G_OBJECT (editor->get ()); weak_primary = G_OBJECT (primary.get ());
    weak_arrow = G_OBJECT (fixture.arrow ());
    g_object_add_weak_pointer (weak_editor, reinterpret_cast<gpointer *> (&weak_editor));
    g_object_add_weak_pointer (weak_primary, reinterpret_cast<gpointer *> (&weak_primary));
    g_object_add_weak_pointer (weak_arrow, reinterpret_cast<gpointer *> (&weak_arrow));
  }
  g_assert_null (weak_editor); g_assert_null (weak_primary); g_assert_null (weak_arrow);
  g_assert_true (GIMP_IS_EDITOR (editor->get ())); g_assert_true (GTK_IS_WIDGET (primary.get ()));
  g_assert_cmpuint (G_OBJECT (editor->get ())->ref_count, ==, 1);
  g_assert_cmpuint (G_OBJECT (primary.get ())->ref_count, ==, 1);
  /* The first weak notifications witnessed disposal, not storage loss. */
  weak_editor = G_OBJECT (editor->get ()); weak_primary = G_OBJECT (primary.get ());
  g_object_add_weak_pointer (weak_editor, reinterpret_cast<gpointer *> (&weak_editor));
  g_object_add_weak_pointer (weak_primary, reinterpret_cast<gpointer *> (&weak_primary));
  g_assert_null (gtk_widget_get_parent (primary.get ()));
  /* Retention keeps identity alive, not GTK liveness. Disposal has removed the
   * manager, so this read-before-construction action path safely returns empty.
   * Do not call the generic constructor on the destroyed container: native GTK
   * preconditions and the old button-box pointer are outside this API contract. */
  g_assert_null (gimp_editor_get_ui_manager (editor->get ()));
  auto unavailable = editor->add_action_dropdown (nullptr, "missing-group", "missing-action");
  g_assert_false (static_cast<bool> (unavailable));
  g_assert_null (gimp_editor_add_action_dropdown (editor->get (), nullptr, "missing-group", "missing-action", static_cast<const gchar *> (nullptr)));
  if (editor_first) {
    editor.reset (); g_assert_null (weak_editor); g_assert_nonnull (weak_primary);
    primary.reset (); g_assert_null (weak_primary);
  } else {
    primary.reset (); g_assert_null (weak_primary); g_assert_nonnull (weak_editor);
    editor.reset (); g_assert_null (weak_editor);
  }
}
static void handle_action_alternates (guint alternate_count) {
  for (bool handle : {false, true}) {
    Fixture fixture; guint normal = 0, shift = 0, control = 0, zero = 0;
    auto *group = gimp_action_group_new (gimp, "handle-alternates", "Alternates", nullptr, nullptr, nullptr);
    gimp_ui_manager_add_action_group (gimp_editor_get_ui_manager (GIMP_EDITOR (fixture.editor)), group);
    auto *main = action (group, "handle-main", "Main <tip>", &normal);
    auto *shift_action = action (group, "handle-shift", "Shift <tip>", &shift);
    auto *control_action = action (group, "handle-control", "Control <tip>", &control);
    action (group, "handle-zero", "Zero mask must be omitted", &zero);
    ObjectRef<GtkWidget> primary;
    if (alternate_count == 0) primary = owned_action_dropdown (fixture, handle, "handle-alternates", "handle-main");
    else if (alternate_count == 1) primary = owned_action_dropdown (fixture, handle, "handle-alternates", "handle-main",
                                                                  "handle-shift", GDK_SHIFT_MASK);
    else primary = owned_action_dropdown (fixture, handle, "handle-alternates", "handle-main",
                                         "handle-shift", GDK_SHIFT_MASK, "missing-alternate", GDK_MOD1_MASK,
                                         "handle-control", GDK_CONTROL_MASK, "handle-zero", GdkModifierType (0));
    assert_dropdown_shape (fixture, primary.get ());
    g_assert_cmpint (g_object_get_data (G_OBJECT (primary.get ()), "extended-actions") != nullptr, ==, alternate_count != 0);
    gchar *markup = gtk_widget_get_tooltip_markup (primary.get ());
    g_assert_nonnull (markup); g_assert_nonnull (strstr (markup, "Main &lt;tip&gt;"));
    if (alternate_count) g_assert_nonnull (strstr (markup, "Shift &lt;tip&gt;"));
    if (alternate_count > 1) g_assert_nonnull (strstr (markup, "Control &lt;tip&gt;"));
    g_assert_null (strstr (markup, "Zero mask")); g_free (markup);
    gtk_button_clicked (GTK_BUTTON (primary.get ())); g_assert_cmpuint (normal, ==, 1);
    g_signal_emit_by_name (primary.get (), "extended-clicked", GDK_MOD1_MASK);
    g_signal_emit_by_name (primary.get (), "extended-clicked", GdkModifierType (0));
    g_assert_cmpuint (shift, ==, 0); g_assert_cmpuint (control, ==, 0); g_assert_cmpuint (zero, ==, 0);
    g_signal_emit_by_name (primary.get (), "extended-clicked", GDK_SHIFT_MASK);
    g_assert_cmpuint (shift, ==, alternate_count ? 1 : 0);
    g_signal_emit_by_name (primary.get (), "extended-clicked", GdkModifierType (GDK_SHIFT_MASK | GDK_CONTROL_MASK));
    g_assert_cmpuint (control, ==, alternate_count > 1 ? 1 : 0);
    g_assert_cmpuint (shift, ==, alternate_count == 1 ? 2 : alternate_count > 1 ? 1 : 0);
    if (alternate_count > 1) {
      gimp_action_set_sensitive (control_action, FALSE, "Last alternate disabled");
      g_signal_emit_by_name (primary.get (), "extended-clicked", GdkModifierType (GDK_SHIFT_MASK | GDK_CONTROL_MASK));
      g_assert_cmpuint (control, ==, 1); g_assert_cmpuint (shift, ==, 2);
    }
    const guint before = shift;
    gimp_action_set_sensitive (shift_action, FALSE, "Shift disabled");
    g_signal_emit_by_name (primary.get (), "extended-clicked", GdkModifierType (GDK_SHIFT_MASK | GDK_CONTROL_MASK));
    g_assert_cmpuint (shift, ==, before); g_assert_cmpuint (zero, ==, 0);
    gimp_action_set_sensitive (main, FALSE, "Primary disabled");
    g_assert_false (gtk_widget_get_sensitive (primary.get ()));
    g_assert_true (gtk_widget_get_sensitive (fixture.arrow ()));
    gimp_action_set_sensitive (main, TRUE, nullptr); g_assert_true (gtk_widget_get_sensitive (primary.get ()));
    g_object_unref (group);
  }
}
static void handle_toggle_action_parity () {
  for (bool handle : {false, true}) {
    Fixture fixture; guint modified = 0;
    auto *group = gimp_action_group_new (gimp, "handle-toggle", "Toggle", nullptr, nullptr, nullptr);
    gimp_ui_manager_add_action_group (gimp_editor_get_ui_manager (GIMP_EDITOR (fixture.editor)), group);
    auto *toggle = gimp_toggle_action_new ("handle-toggle-main", "Toggle", nullptr, "Toggle help", "document-new", "toggle-help", gimp->user_context);
    gimp_action_group_add_action (group, toggle);
    action (group, "handle-toggle-alternate", "Alternate", &modified);
    auto primary = owned_action_dropdown (fixture, handle, "handle-toggle", "handle-toggle-main", "handle-toggle-alternate", GDK_SHIFT_MASK);
    assert_dropdown_shape (fixture, primary.get ());
    g_assert_false (GTK_IS_TOGGLE_BUTTON (primary.get ()));
    g_assert_false (gimp_toggle_action_get_active (GIMP_TOGGLE_ACTION (toggle)));
    gtk_button_clicked (GTK_BUTTON (primary.get ()));
    g_assert_true (gimp_toggle_action_get_active (GIMP_TOGGLE_ACTION (toggle)));
    gtk_button_clicked (GTK_BUTTON (primary.get ()));
    g_assert_false (gimp_toggle_action_get_active (GIMP_TOGGLE_ACTION (toggle)));
    g_signal_emit_by_name (primary.get (), "extended-clicked", GDK_SHIFT_MASK);
    g_assert_cmpuint (modified, ==, 1);
    g_object_unref (toggle); g_object_unref (group);
  }
}
static void handle_missing_lookup_parity () {
  for (bool handle : {false, true}) {
    Fixture fixture; guint normal = 0;
    auto *group = gimp_action_group_new (gimp, "handle-missing", "Missing", nullptr, nullptr, nullptr);
    gimp_ui_manager_add_action_group (gimp_editor_get_ui_manager (GIMP_EDITOR (fixture.editor)), group);
    action (group, "handle-existing", "Existing", &normal); g_object_unref (group);
    g_assert_false (static_cast<bool> (owned_action_dropdown (fixture, handle, "absent-group", "handle-existing")));
    g_assert_false (static_cast<bool> (owned_action_dropdown (fixture, handle, "handle-missing", "absent-primary")));
    gimp_menu_factory_delete_manager (fixture.factory, "<LayerPresets>", fixture.editor); fixture.manager_deleted = true;
    g_assert_false (static_cast<bool> (owned_action_dropdown (fixture, handle, "handle-missing", "handle-existing")));
    g_assert_null (BindingStore::find (G_OBJECT (fixture.editor))); g_assert_cmpuint (normal, ==, 0);
  }
}
struct HandleReentry { ObjectRef<GtkWidget> *result; GtkWidget *editor; guint normal = 0, extended = 0; };
static void handle_reentry_extended (GtkWidget *, GdkModifierType mask, gpointer data) {
  auto *state = static_cast<HandleReentry *> (data);
  ++state->extended; g_assert_cmpint (mask, ==, GDK_SHIFT_MASK);
}
static void handle_reentry_clicked (GtkWidget *button, gpointer data) {
  auto *state = static_cast<HandleReentry *> (data); ++state->normal;
  g_signal_emit_by_name (button, "extended-clicked", GDK_SHIFT_MASK);
  state->result->reset ();
  gtk_widget_destroy (state->editor);
  gtk_button_clicked (GTK_BUTTON (button));
  g_signal_emit_by_name (button, "extended-clicked", GDK_SHIFT_MASK);
}
static void handle_generic_reentry_parity () {
  for (bool handle : {false, true}) {
    Fixture fixture; ObjectRef<GtkWidget> primary;
    HandleReentry state {&primary, fixture.editor};
    primary = owned_generic (fixture, handle, G_CALLBACK (handle_reentry_clicked), G_CALLBACK (handle_reentry_extended), &state);
    auto retained = primary;
    auto arrow = ObjectRef<GtkWidget>::retain (fixture.arrow ());
    gtk_button_clicked (GTK_BUTTON (primary.get ()));
    g_assert_false (static_cast<bool> (primary));
    g_assert_cmpuint (state.normal, ==, 1); g_assert_cmpuint (state.extended, ==, 1);
    g_assert_true (BindingStore::require (G_OBJECT (arrow.get ())).state () == BindingStore::State::closed);
    gtk_button_clicked (GTK_BUTTON (retained.get ()));
    g_signal_emit_by_name (retained.get (), "extended-clicked", GDK_SHIFT_MASK);
    g_assert_cmpuint (state.normal, ==, 1); g_assert_cmpuint (state.extended, ==, 1);
  }
}
struct HandleManagerLoss { Fixture *fixture; GtkWidget *primary; guint calls = 0; };
static void handle_manager_loss_called (GimpAction *main, GVariant *, gpointer data) {
  auto *state = static_cast<HandleManagerLoss *> (data); ++state->calls;
  gimp_action_set_sensitive (main, FALSE, "Losing manager during callback");
  gimp_menu_factory_delete_manager (state->fixture->factory, "<LayerPresets>", state->fixture->editor);
  state->fixture->manager_deleted = true;
  g_signal_emit_by_name (state->primary, "extended-clicked", GDK_SHIFT_MASK);
}
static void handle_action_manager_reentry_parity () {
  for (bool handle : {false, true}) {
    Fixture fixture; guint normal = 0, modified = 0;
    auto *manager = gimp_editor_get_ui_manager (GIMP_EDITOR (fixture.editor));
    auto *group = gimp_action_group_new (gimp, "handle-manager-reentry", "Manager reentry", nullptr, nullptr, nullptr);
    gimp_ui_manager_add_action_group (manager, group);
    auto *main = action (group, "handle-manager-main", "Main", &normal);
    auto *alternate = action (group, "handle-manager-alternate", "Alternate", &modified);
    GObject *weak_manager = G_OBJECT (manager), *weak_main = G_OBJECT (main), *weak_alternate = G_OBJECT (alternate);
    g_object_add_weak_pointer (weak_manager, reinterpret_cast<gpointer *> (&weak_manager));
    g_object_add_weak_pointer (weak_main, reinterpret_cast<gpointer *> (&weak_main));
    g_object_add_weak_pointer (weak_alternate, reinterpret_cast<gpointer *> (&weak_alternate));
    auto primary = owned_action_dropdown (fixture, handle, "handle-manager-reentry", "handle-manager-main", "handle-manager-alternate", GDK_SHIFT_MASK);
    HandleManagerLoss state {&fixture, primary.get ()};
    g_signal_connect (main, "activate", G_CALLBACK (handle_manager_loss_called), &state); g_object_unref (group);
    gtk_button_clicked (GTK_BUTTON (primary.get ()));
    g_assert_cmpuint (normal, ==, 1); g_assert_cmpuint (modified, ==, 0); g_assert_cmpuint (state.calls, ==, 1);
    g_assert_null (weak_manager); g_assert_null (weak_main); g_assert_null (weak_alternate);
    g_assert_nonnull (gtk_widget_get_parent (primary.get ()));
    gtk_button_clicked (GTK_BUTTON (primary.get ()));
    g_signal_emit_by_name (primary.get (), "extended-clicked", GDK_SHIFT_MASK);
    g_assert_cmpuint (normal, ==, 1); g_assert_cmpuint (modified, ==, 0); g_assert_cmpuint (state.calls, ==, 1);
  }
}
static void handle_retained_destroyed_primary_parity () {
  for (bool handle : {false, true}) {
    Fixture fixture; guint normal = 0, modified = 0;
    auto *group = gimp_action_group_new (gimp, "handle-destroyed-primary", "Destroyed", nullptr, nullptr, nullptr);
    gimp_ui_manager_add_action_group (gimp_editor_get_ui_manager (GIMP_EDITOR (fixture.editor)), group);
    auto *main = action (group, "handle-destroyed-main", "Main", &normal);
    auto *alternate = action (group, "handle-destroyed-alternate", "Alternate", &modified);
    auto primary = owned_action_dropdown (fixture, handle, "handle-destroyed-primary", "handle-destroyed-main", "handle-destroyed-alternate", GDK_SHIFT_MASK);
    auto arrow = ObjectRef<GtkWidget>::retain (fixture.arrow ());
    gtk_widget_destroy (primary.get ());
    g_assert_cmpuint (G_OBJECT (primary.get ())->ref_count, ==, 1);
    g_assert_null (gtk_widget_get_parent (primary.get ()));
    g_assert_true (BindingStore::require (G_OBJECT (arrow.get ())).state () == BindingStore::State::closed);
    gimp_action_set_sensitive (main, FALSE, "Detached proxy");
    g_assert_true (gtk_widget_get_sensitive (primary.get ()));
    gtk_button_clicked (GTK_BUTTON (primary.get ()));
    g_signal_emit_by_name (primary.get (), "extended-clicked", GDK_SHIFT_MASK);
    gimp_action_group_remove_action (group, alternate);
    g_signal_emit_by_name (primary.get (), "extended-clicked", GDK_SHIFT_MASK);
    g_assert_cmpuint (normal, ==, 0); g_assert_cmpuint (modified, ==, 0);
    g_object_unref (group);
  }
}
static void handle_construction_owner_loss () {
  for (bool action_backed : {false, true}) {
    for (guint mode = 0; mode < 4; ++mode) {
      Fixture fixture; guint normal = 0;
      auto *group = gimp_action_group_new (gimp, "handle-construction", "Construction", nullptr, nullptr, nullptr);
      gimp_ui_manager_add_action_group (gimp_editor_get_ui_manager (GIMP_EDITOR (fixture.editor)), group);
      action (group, "handle-construction-main", "Main", &normal); g_object_unref (group);
      auto editor = EditorRef::retain (GIMP_EDITOR (fixture.editor));
      CancelConstruction cancellation {fixture.editor, mode};
      const guint signal = g_signal_lookup ("show", GTK_TYPE_WIDGET);
      const gulong hook = g_signal_add_emission_hook (signal, 0, cancel_construction, &cancellation, nullptr);
      g_assert_cmpuint (hook, !=, 0);
      auto primary = action_backed ? editor.add_action_dropdown (nullptr, "handle-construction", "handle-construction-main") :
        editor.add_dropdown ("document-new", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
      g_signal_remove_emission_hook (signal, hook);
      g_assert_cmpuint (cancellation.calls, ==, 1); g_assert_false (static_cast<bool> (primary));
      g_assert_cmpuint (normal, ==, 0);
      if (cancellation.retained_primary) g_object_unref (cancellation.retained_primary);
    }
  }
}
int main (int argc, char **argv) {
  g_test_init (&argc, &argv, nullptr); if (!gtk_init_check (&argc, &argv)) return GIMP_EXIT_TEST_SKIPPED;
  gimp_test_utils_setup_menus_path (); gimp = gimp_init_for_gui_testing (TRUE);
  gimp_set_focused_once (gimp);
  gchar *path = g_build_filename (g_getenv ("GIMP_TESTING_ABS_TOP_SRCDIR"), "data", "layer-presets", nullptr);
  g_object_set (gimp->config, "layer-presets-path", path, nullptr); g_free (path);
  gimp_data_factory_data_refresh (gimp->layer_preset_factory, gimp->user_context);
  g_test_add_func ("/editor-dropdown/generic-contract", generic_contract);
  g_test_add_func ("/editor-dropdown/generic-reentrant-close", generic_reentrant_close);
  g_test_add_func ("/editor-dropdown/popup-cycles-context-retained-view", popup_cycles_context_and_retained_view);
  g_test_add_func ("/editor-dropdown/selection-owner-loss", selection_owner_loss);
  g_test_add_func ("/editor-dropdown/retire-reopens-newer-context", retire_reopens_newer_context);
  g_test_add_func ("/editor-dropdown/native-action-contract", native_action_contract);
  g_test_add_func ("/editor-dropdown/ordinary-action-control", ordinary_action_control);
  g_test_add_func ("/editor-dropdown/construction-owner-loss", construction_owner_loss);
  g_test_add_func ("/editor-dropdown/null-primary-tooltip-dropdown", [] { nullable_action_tooltip (0, true); });
  g_test_add_func ("/editor-dropdown/null-primary-tooltip-ordinary", [] { nullable_action_tooltip (1, true); });
  g_test_add_func ("/editor-dropdown/null-alternate-tooltip-dropdown", [] { nullable_action_tooltip (0, false); });
  g_test_add_func ("/editor-dropdown/null-alternate-tooltip-ordinary", [] { nullable_action_tooltip (1, false); });
  g_test_add_func ("/editor-dropdown/toggle-action-shape", toggle_action_shape);
  g_test_add_func ("/editor-dropdown/action-loss-during-construction", action_loss_during_construction);
  g_test_add_func ("/editor-dropdown/missing-context-can-reopen", missing_context_can_reopen);
  g_test_add_func ("/editor-dropdown/completed-popup-cycles", completed_popup_cycles);
  g_test_add_func ("/editor-dropdown/rapid-reopen-changes-context", rapid_reopen_changes_context);
  g_test_add_func ("/editor-dropdown/interrupted-hide-preserves-newer-presentation", interrupted_hide_preserves_newer_presentation);
  g_test_add_func ("/editor-dropdown/interrupted-hide-owner-loss", interrupted_hide_owner_loss);
  g_test_add_func ("/editor-dropdown/interrupted-hide-respects-cancel", interrupted_hide_respects_cancel);
  g_test_add_func ("/editor-dropdown/closed-retirement-reopens-newer-context", closed_retirement_reopens_newer_context);
  g_test_add_func ("/editor-dropdown/closed-retirement-respects-cancel-and-owner-loss", closed_retirement_respects_cancel_and_owner_loss);
  g_test_add_func ("/editor-dropdown/closed-retirement-newer-open-wins", closed_retirement_newer_open_wins);
  g_test_add_func ("/editor-dropdown/alternate-finalization-live-button-dropdown", [] { alternate_finalization_live_button (0); });
  g_test_add_func ("/editor-dropdown/alternate-finalization-live-button-ordinary", [] { alternate_finalization_live_button (1); });
  g_test_add_func ("/editor-dropdown/alternate-replacement-preserves-identity-dropdown", [] { alternate_replacement_preserves_identity (0); });
  g_test_add_func ("/editor-dropdown/alternate-replacement-preserves-identity-ordinary", [] { alternate_replacement_preserves_identity (1); });
  g_test_add_func ("/editor-dropdown/alternate-loss-preserves-precedence-dropdown", [] { alternate_loss_preserves_precedence (0); });
  g_test_add_func ("/editor-dropdown/alternate-loss-preserves-precedence-ordinary", [] { alternate_loss_preserves_precedence (1); });
  g_test_add_func ("/editor-dropdown/alternate-self-removal-and-owner-loss-dropdown-surviving-owner", [] { alternate_self_removal_and_owner_loss (0, 0); });
  g_test_add_func ("/editor-dropdown/alternate-self-removal-and-owner-loss-dropdown-destroyed-owner", [] { alternate_self_removal_and_owner_loss (0, 1); });
  g_test_add_func ("/editor-dropdown/alternate-self-removal-and-owner-loss-ordinary-surviving-owner", [] { alternate_self_removal_and_owner_loss (1, 0); });
  g_test_add_func ("/editor-dropdown/alternate-self-removal-and-owner-loss-ordinary-destroyed-owner", [] { alternate_self_removal_and_owner_loss (1, 1); });
  g_test_add_func ("/editor-dropdown/alternate-manager-loss-live-button-dropdown", [] { alternate_manager_loss_live_button (0); });
  g_test_add_func ("/editor-dropdown/alternate-manager-loss-live-button-ordinary", [] { alternate_manager_loss_live_button (1); });
  g_test_add_func ("/editor-dropdown/alternate-retained-destroyed-button-dropdown", [] { alternate_retained_destroyed_button (0); });
  g_test_add_func ("/editor-dropdown/alternate-retained-destroyed-button-ordinary", [] { alternate_retained_destroyed_button (1); });
  g_test_add_func ("/editor-dropdown/null-primary-tooltip-handle", [] { nullable_action_tooltip (2, true); });
  g_test_add_func ("/editor-dropdown/null-alternate-tooltip-handle", [] { nullable_action_tooltip (2, false); });
  g_test_add_func ("/editor-dropdown/handle-generic-parity", handle_generic_parity);
  g_test_add_func ("/editor-dropdown/handle-editor-ref-ownership", handle_editor_ref_ownership);
  g_test_add_func ("/editor-dropdown/handle-generic-ref-release-first", [] { handle_primary_ref_ownership (false, true); });
  g_test_add_func ("/editor-dropdown/handle-generic-ref-destroy-first", [] { handle_primary_ref_ownership (false, false); });
  g_test_add_func ("/editor-dropdown/handle-action-ref-release-first", [] { handle_primary_ref_ownership (true, true); });
  g_test_add_func ("/editor-dropdown/handle-action-ref-destroy-first", [] { handle_primary_ref_ownership (true, false); });
  g_test_add_func ("/editor-dropdown/handle-outlives-fixture-editor-first", [] { handle_outlives_fixture (true); });
  g_test_add_func ("/editor-dropdown/handle-outlives-fixture-primary-first", [] { handle_outlives_fixture (false); });
  g_test_add_func ("/editor-dropdown/handle-action-zero-alternates", [] { handle_action_alternates (0); });
  g_test_add_func ("/editor-dropdown/handle-action-one-alternate", [] { handle_action_alternates (1); });
  g_test_add_func ("/editor-dropdown/handle-action-multiple-alternates", [] { handle_action_alternates (2); });
  g_test_add_func ("/editor-dropdown/handle-toggle-action-parity", handle_toggle_action_parity);
  g_test_add_func ("/editor-dropdown/handle-missing-lookup-parity", handle_missing_lookup_parity);
  g_test_add_func ("/editor-dropdown/handle-generic-reentry-parity", handle_generic_reentry_parity);
  g_test_add_func ("/editor-dropdown/handle-action-manager-reentry-parity", handle_action_manager_reentry_parity);
  g_test_add_func ("/editor-dropdown/handle-retained-destroyed-primary-parity", handle_retained_destroyed_primary_parity);
  g_test_add_func ("/editor-dropdown/handle-construction-owner-loss", handle_construction_owner_loss);
  g_application_run (gimp->app, 0, nullptr);
  const gint result = gimp_core_app_get_exit_status (GIMP_CORE_APP (gimp->app));
  g_application_quit (G_APPLICATION (gimp->app)); g_clear_object (&gimp->app); return result;
}
