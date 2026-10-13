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
  g_application_run (gimp->app, 0, nullptr);
  const gint result = gimp_core_app_get_exit_status (GIMP_CORE_APP (gimp->app));
  g_application_quit (G_APPLICATION (gimp->app)); g_clear_object (&gimp->app); return result;
}
