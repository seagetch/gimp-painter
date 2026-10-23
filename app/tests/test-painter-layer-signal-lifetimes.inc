/* SPDX-License-Identifier: GPL-3.0-or-later */
/* 07.008/untracked-signals: these are actual native tile controls. Keeping a
 * closed BindingStore's native widgets alive distinguishes disconnection from
 * GtkWidget::destroy's own removal of handlers. */
static guint
tile_signal_handlers (gpointer object, const gchar *signal)
{
  guint id = 0;
  GQuark detail = 0;
  GSignalMatchType match = G_SIGNAL_MATCH_ID;
  guint count;
  g_assert_true (g_signal_parse_name (signal, G_OBJECT_TYPE (object), &id, &detail, TRUE));
  if (detail) match |= G_SIGNAL_MATCH_DETAIL;
  count = g_signal_handlers_block_matched (object, match, id, detail, NULL, NULL, NULL);
  g_assert_cmpuint (g_signal_handlers_unblock_matched (object, match, id, detail, NULL, NULL, NULL), ==, count);
  return count;
}

static void
layer_popup_signal_close (void)
{
  const gchar *names[] = { "painter-layer-opacity", "painter-layer-mode",
    "painter-layer-lock-alpha", "painter-layer-lock-content", "painter-layer-lock-position",
    "layers-new-group", "layers-duplicate", "layers-new-clone", "layers-delete" };
  const gchar *signals[] = { "value-changed", "notify::layer-mode",
    "toggled", "toggled", "toggled", "clicked", "clicked", "clicked", "clicked" };
  GimpImage *image = new_image ();
  GimpLayer *a = layer (image, NULL, "Retained popup target");
  GtkWidget *owner = tiles (image);
  GtkWidget *popup = gimp_painter_layer_tiles_popup (GIMP_PAINTER_LAYER_TILES (owner), FALSE);
  GtkWidget *controls[G_N_ELEMENTS (names)];
  guint before[G_N_ELEMENTS (names)];
  g_assert_nonnull (popup);
  g_object_ref (owner);
  for (guint i = 0; i < G_N_ELEMENTS (names); i++)
    {
      controls[i] = find (popup, names[i]);
      g_assert_nonnull (controls[i]);
      g_object_ref (controls[i]);
      before[i] = tile_signal_handlers (controls[i], signals[i]);
      g_assert_cmpuint (before[i], >, 0);
      /* Keep native controls alive outside the popover. Its destruction must
       * not be what makes the captured owner safe. */
      gtk_container_remove (GTK_CONTAINER (gtk_widget_get_parent (controls[i])), controls[i]);
    }
  g_assert_true (gimp_painter_binding_close (G_OBJECT (owner), NULL));
  g_assert_true (gimp_painter_binding_close (G_OBJECT (owner), NULL));
  for (guint i = 0; i < G_N_ELEMENTS (names); i++)
    {
      g_test_message ("Closed tile control: %s / %s", names[i], signals[i]);
      g_assert_cmpuint (tile_signal_handlers (controls[i], signals[i]), ==, before[i] - 1);
    }
  gtk_range_set_value (GTK_RANGE (controls[0]), 23);
  gimp_layer_mode_box_set_mode (GIMP_LAYER_MODE_BOX (controls[1]), GIMP_LAYER_MODE_MULTIPLY_LEGACY);
  for (guint i = 2; i < 5; i++) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (controls[i]), TRUE);
  for (guint i = 5; i < G_N_ELEMENTS (names); i++) gtk_button_clicked (GTK_BUTTON (controls[i]));
  g_assert_cmpint (gimp_image_get_n_layers (image), ==, 1);
  g_assert_cmpfloat (gimp_layer_get_opacity (a), ==, 1.0);
  g_assert_cmpint (gimp_layer_get_mode (a), ==, GIMP_LAYER_MODE_NORMAL_LEGACY);
  g_assert_false (gimp_layer_get_lock_alpha (a));
  g_assert_false (gimp_item_get_lock_content (GIMP_ITEM (a)));
  g_assert_false (gimp_item_get_lock_position (GIMP_ITEM (a)));
  close_tiles (owner);
  g_object_unref (owner);
  for (guint i = 0; i < G_N_ELEMENTS (names); i++)
    { gtk_widget_destroy (controls[i]); g_object_unref (controls[i]); }
  g_object_unref (image);
}

static void
layer_popup_emitter_first (void)
{
  const gchar *names[] = { "painter-layer-opacity", "painter-layer-mode",
    "painter-layer-lock-alpha", "layers-new-group" };
  for (guint i = 0; i < G_N_ELEMENTS (names); i++)
    {
      GimpImage *image = new_image ();
      GtkWidget *owner, *popup, *control;
      gpointer weak;
      layer (image, NULL, "Emitter first");
      owner = tiles (image);
      popup = gimp_painter_layer_tiles_popup (GIMP_PAINTER_LAYER_TILES (owner), FALSE);
      g_assert_nonnull (popup);
      control = find (popup, names[i]);
      g_assert_nonnull (control);
      weak = control;
      g_object_add_weak_pointer (G_OBJECT (control), &weak);
      gtk_widget_destroy (control);
      g_assert_null (weak);
      /* Each Connection now has an expired emitter, not just an emitter whose
       * destroy signal ran while a test reference kept the object alive. */
      g_assert_true (gimp_painter_binding_close (G_OBJECT (owner), NULL));
      g_assert_true (gimp_painter_binding_close (G_OBJECT (owner), NULL));
      close_tiles (owner);
      g_object_unref (image);
    }
}

typedef struct {
  GtkWidget *owner;
  gboolean destroy;
  guint calls;
} TileSignalClose;

static void
close_tiles_during_property (GimpLayer *layer, gpointer data)
{
  TileSignalClose *state = data;
  state->calls++;
  if (state->destroy) gtk_widget_destroy (state->owner);
  else g_assert_true (gimp_painter_binding_close (G_OBJECT (state->owner), NULL));
}

static void
layer_popup_signal_reentry (void)
{
  const gchar *names[] = { "painter-layer-opacity", "painter-layer-mode", "painter-layer-lock-alpha" };
  const gchar *signals[] = { "opacity-changed", "mode-changed", "lock-alpha-changed" };
  for (guint kind = 0; kind < G_N_ELEMENTS (names); kind++)
    for (guint destroy = 0; destroy < 2; destroy++)
      {
        GimpImage *image = new_image ();
        GimpLayer *a = layer (image, NULL, "First selected");
        GimpLayer *b = layer (image, NULL, "Must stay unchanged");
        GList *selection = g_list_append (g_list_append (NULL, a), b);
        GtkWidget *owner = tiles (image);
        GtkWidget *popup, *control;
        GtkAdjustment *adjustment = NULL;
        TileSignalClose state = { owner, destroy, 0 };
        gulong handler;
        g_test_message ("Property reentry: %s, destroy=%u", names[kind], destroy);
        g_object_ref (owner);
        gimp_image_set_selected_layers (image, selection);
        g_list_free (selection);
        selection = gimp_image_get_selected_layers (image);
        g_assert_cmpuint (g_list_length (selection), ==, 2);
        a = selection->data;
        b = selection->next->data;
        popup = gimp_painter_layer_tiles_popup (GIMP_PAINTER_LAYER_TILES (owner), FALSE);
        g_assert_nonnull (popup);
        control = find (popup, names[kind]);
        g_assert_nonnull (control);
        g_object_ref (control);
        /* gtk_range_set_value delegates to the adjustment. Retain that actual
         * emitter across a synchronous destroy of its owning GtkRange. */
        if (kind == 0) adjustment = g_object_ref (gtk_range_get_adjustment (GTK_RANGE (control)));
        handler = g_signal_connect (a, signals[kind], G_CALLBACK (close_tiles_during_property), &state);
        if (kind == 0) gtk_range_set_value (GTK_RANGE (control), 31);
        else if (kind == 1) gimp_layer_mode_box_set_mode (GIMP_LAYER_MODE_BOX (control), GIMP_LAYER_MODE_MULTIPLY_LEGACY);
        else gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (control), TRUE);
        g_assert_cmpuint (state.calls, ==, 1);
        g_signal_handler_disconnect (a, handler);
        g_assert_cmpfloat (gimp_layer_get_opacity (b), ==, 1.0);
        g_assert_cmpint (gimp_layer_get_mode (b), ==, GIMP_LAYER_MODE_NORMAL_LEGACY);
        g_assert_false (gimp_layer_get_lock_alpha (b));
        if (kind == 0) g_assert_cmpfloat_with_epsilon (gimp_layer_get_opacity (a), .31, 1e-10);
        else if (kind == 1) g_assert_cmpint (gimp_layer_get_mode (a), ==, GIMP_LAYER_MODE_MULTIPLY_LEGACY);
        else g_assert_true (gimp_layer_get_lock_alpha (a));
        /* Mid-callback close must still balance the native undo group. */
        g_assert_true (gimp_image_undo (image));
        g_assert_cmpfloat (gimp_layer_get_opacity (a), ==, 1.0);
        g_assert_cmpint (gimp_layer_get_mode (a), ==, GIMP_LAYER_MODE_NORMAL_LEGACY);
        g_assert_false (gimp_layer_get_lock_alpha (a));
        g_clear_object (&adjustment);
        gtk_widget_destroy (control); g_object_unref (control);
        close_tiles (owner); g_object_unref (owner);
        g_object_unref (image);
      }
}

static void
layer_permanent_signal_close (void)
{
  const gchar *names[] = { "painter-layer-add", "painter-layer-add", "painter-layer-add",
    "painter-layer-canvas", "painter-layer-canvas", "painter-layer-canvas", "painter-layer-canvas" };
  const gchar *signals[] = { "button-press-event", "motion-notify-event", "button-release-event",
    "button-press-event", "motion-notify-event", "button-release-event", "draw" };
  GimpImage *image = new_image ();
  GimpLayer *a = layer (image, NULL, "Disconnected event target");
  GtkWidget *owner = tiles (image);
  guint before[G_N_ELEMENTS (names)];
  for (guint i = 0; i < G_N_ELEMENTS (names); i++)
    {
      g_assert_nonnull (find (owner, names[i]));
      before[i] = tile_signal_handlers (find (owner, names[i]), signals[i]);
      g_assert_cmpuint (before[i], >, 0);
    }
  g_assert_true (gimp_painter_binding_close (G_OBJECT (owner), NULL));
  for (guint i = 0; i < G_N_ELEMENTS (names); i++)
    g_assert_cmpuint (tile_signal_handlers (find (owner, names[i]), signals[i]), ==, before[i] - 1);
  canvas_button (find (owner, "painter-layer-canvas"), GDK_BUTTON_PRESS, 1, 0, 8, 8);
  canvas_button (find (owner, "painter-layer-canvas"), GDK_BUTTON_RELEASE, 1, 0, 8, 8);
  g_assert_true (gimp_item_get_visible (GIMP_ITEM (a)));
  g_assert_false (gimp_painter_layer_tiles_long_press_pending (GIMP_PAINTER_LAYER_TILES (owner)));
  close_tiles (owner);
  g_object_unref (image);
}

#include "core/gimpdatafactory.h"
#include "core/gimplayerpreset.h"

static GtkWidget *
tile_preset_button (GtkWidget *widget, const gchar *label)
{
  if (GTK_IS_BUTTON (widget) && !g_strcmp0 (gtk_button_get_label (GTK_BUTTON (widget)), label))
    return widget;
  if (GTK_IS_CONTAINER (widget))
    {
      GList *children = gtk_container_get_children (GTK_CONTAINER (widget));
      GtkWidget *result = NULL;
      for (GList *i = children; i && !result; i = i->next)
        result = tile_preset_button (i->data, label);
      g_list_free (children);
      return result;
    }
  return NULL;
}

static void
layer_preset_signal_owner (void)
{
  GimpImage *image = new_image ();
  GimpContainer *presets = gimp_data_factory_get_container (gimp->layer_preset_factory);
  GimpData *preset = gimp_layer_preset_new (gimp_get_user_context (gimp), "Signal lifetime preset");
  gpointer weak_preset = preset;
  GtkWidget *owner, *popup, *button;
  guint before;
  layer (image, NULL, "Unchanged after stale preset click");
  gimp_data_make_internal (preset, "gimp-painter-signal-lifetime-preset");
  g_object_add_weak_pointer (G_OBJECT (preset), &weak_preset);
  g_assert_true (gimp_container_add (presets, GIMP_OBJECT (preset)));
  owner = tiles (image);
  popup = gimp_painter_layer_tiles_popup (GIMP_PAINTER_LAYER_TILES (owner), TRUE);
  g_assert_nonnull (popup);
  button = tile_preset_button (popup, "Signal lifetime preset");
  g_assert_nonnull (button);
  g_object_ref (button);
  before = tile_signal_handlers (button, "clicked");
  g_assert_cmpuint (before, >, 0);
  gtk_container_remove (GTK_CONTAINER (gtk_widget_get_parent (button)), button);
  g_assert_true (gimp_container_remove (presets, GIMP_OBJECT (preset)));
  g_object_unref (preset);
  /* The callback's target lease, not the factory or this test, owns the preset. */
  g_assert_nonnull (weak_preset);
  g_assert_true (gimp_painter_binding_close (G_OBJECT (owner), NULL));
  g_assert_null (weak_preset);
  g_assert_cmpuint (tile_signal_handlers (button, "clicked"), ==, before - 1);
  gtk_button_clicked (GTK_BUTTON (button));
  g_assert_cmpint (gimp_image_get_n_layers (image), ==, 1);
  gtk_widget_destroy (button); g_object_unref (button);
  close_tiles (owner);
  g_object_unref (image);
}
