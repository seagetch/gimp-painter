/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "core/gimplayer.h"
#include "widgets/gimpcontainertreeview.h"
#include "widgets/gimpcontainerview.h"
#include "widgets/gimpdocked.h"
/* Insert before main() in test-painter-compact-options.c.
 * Required extra headers are in native-test-includes.h.inc.
 * Uses the preceding central widget_store_finalized qdata-destroy helper.
 * This deliberately uses the production GimpDocked interface and property
 * factory, without replacing/interposing gimp_prop_view_new(). */
static void
native_preview_factory_failure (void)
{
  gimp_set_focused_once (gimp);
  GimpContext *context = gimp_context_new (gimp, "Native failed preview fixture", NULL);
  GimpContainer *container = gimp_list_new (GIMP_TYPE_LAYER, FALSE);
  GtkWidget *box = gimp_container_tree_view_new (container, NULL, 24, 0);
  g_assert_nonnull (box);
  g_object_ref_sink (box);
  /* A layer container is valid for ContainerView, but GimpContext has no
   * selected-layer property. This reaches the real factory's NULL return. */
  g_assert_null (gimp_context_type_to_prop_name (GIMP_TYPE_LAYER));
  for (guint repeat = 0; repeat < 2; ++repeat)
    {
      g_test_expect_message ("GLib-GObject", G_LOG_LEVEL_CRITICAL,
                             "*g_object_class_find_property*property_name != NULL*");
      g_test_expect_message ("Gimp-Widgets", G_LOG_LEVEL_WARNING,
                             "gimp_prop_view_new: GimpContext has no property named '(null)'");
      g_test_expect_message ("Gimp-Widgets", G_LOG_LEVEL_CRITICAL,
                             "*gimp_container_box_get_preview*preview != NULL*");
      GtkWidget *preview = gimp_docked_get_preview (GIMP_DOCKED (box), context,
                                                   GTK_ICON_SIZE_MENU);
      g_test_assert_expected_messages ();
      g_assert_null (preview);
    }
  /* Only the exact three diagnostics above are expected, per attempt. No
   * log handler/fatal-mask changes suppress any unrelated warnings. */
  gtk_widget_destroy (box);
  g_object_unref (box);
  g_object_unref (container);
  g_object_unref (context);
  drain ();
}

static void
native_preview_factory_success (void)
{
  gimp_set_focused_once (gimp);
  GimpContext *context = gimp_context_new (gimp, "Native preview fixture", NULL);
  GimpContainer *container = gimp_list_new (GIMP_TYPE_BRUSH, FALSE);
  GimpBrush *brush = GIMP_BRUSH (gimp_brush_generated_new (
    "Native preview brush", GIMP_BRUSH_GENERATED_CIRCLE, 9., 2, .5, 1., 0.));
  GtkWidget *box = gimp_container_tree_view_new (container, NULL, 24, 0);
  g_assert_nonnull (box);
  g_object_ref_sink (box);
  g_assert_cmpstr (gimp_context_type_to_prop_name (GIMP_TYPE_BRUSH), ==, "brush");
  for (guint border = 0; border <= 3; border += 3)
    {
      gimp_container_view_set_view_size (GIMP_CONTAINER_VIEW (box), 24, border);
      gimp_context_set_brush (context, NULL);
      GtkWidget *preview = gimp_docked_get_preview (GIMP_DOCKED (box), context,
                                                   GTK_ICON_SIZE_MENU);
      g_assert_true (GIMP_IS_VIEW (preview));
      g_object_ref_sink (preview);
      GimpViewRenderer *renderer = GIMP_VIEW (preview)->renderer;
      gint width = 0, height = 0;
      g_assert_true (gtk_icon_size_lookup (GTK_ICON_SIZE_MENU, &width, &height));
      g_assert_cmpint (renderer->size, ==, -1);
      g_assert_cmpint (renderer->width, ==, width);
      g_assert_cmpint (renderer->height, ==, height);
      g_assert_cmpint (renderer->border_width, ==, MIN (1, border));
      g_assert_null (renderer->viewable);
      g_assert_true (gtk_widget_get_visible (preview));
      gimp_context_set_brush (context, brush);
      g_assert_true (renderer->viewable == GIMP_VIEWABLE (brush));
      gimp_context_set_brush (context, NULL);
      g_assert_null (renderer->viewable);
      gboolean gone = FALSE;
      g_object_set_data_full (G_OBJECT (preview), "08017-finalized",
                              &gone, widget_store_finalized);
      gtk_widget_destroy (preview);
      g_assert_false (gone); /* Our explicit strong reference still survives. */
      g_object_unref (preview);
      g_assert_true (gone);
      /* Dead preview notify handlers must no longer target its renderer. */
      gimp_context_set_brush (context, brush);
    }
  gtk_widget_destroy (box);
  g_object_unref (box);
  g_object_unref (container);
  g_object_unref (context);
  g_object_unref (brush);
  drain ();
}
/* Native helper coverage is distinct from C++ BindingStore ownership.
 * This fixture binds real production enum/boolean controls to a GObject. */
typedef struct
{
  GObject parent;
  gint choice;
  gboolean enabled;
} NativeControlFixture;
typedef struct { GObjectClass parent; } NativeControlFixtureClass;
static GType native_control_fixture_get_type (void);
G_DEFINE_TYPE (NativeControlFixture, native_control_fixture, G_TYPE_OBJECT)

static void
native_control_fixture_set_property (GObject *object, guint id,
                                      const GValue *value, GParamSpec *spec)
{
  NativeControlFixture *fixture = (NativeControlFixture *) object;
  switch (id)
    {
    case 1: fixture->choice = g_value_get_enum (value); break;
    case 2: fixture->enabled = g_value_get_boolean (value); break;
    default: G_OBJECT_WARN_INVALID_PROPERTY_ID (object, id, spec); break;
    }
}

static void
native_control_fixture_get_property (GObject *object, guint id,
                                      GValue *value, GParamSpec *spec)
{
  NativeControlFixture *fixture = (NativeControlFixture *) object;
  switch (id)
    {
    case 1: g_value_set_enum (value, fixture->choice); break;
    case 2: g_value_set_boolean (value, fixture->enabled); break;
    default: G_OBJECT_WARN_INVALID_PROPERTY_ID (object, id, spec); break;
    }
}

static void
native_control_fixture_class_init (NativeControlFixtureClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  object_class->set_property = native_control_fixture_set_property;
  object_class->get_property = native_control_fixture_get_property;
  g_object_class_install_property (object_class, 1,
    g_param_spec_enum ("fixture-choice", "Fixture choice", "Native enum binding",
                       GIMP_TYPE_BRUSH_GENERATED_SHAPE, GIMP_BRUSH_GENERATED_SQUARE,
                       G_PARAM_READWRITE));
  g_object_class_install_property (object_class, 2,
    g_param_spec_boolean ("fixture-enabled", "Fixture enabled", "Native boolean binding",
                          TRUE, G_PARAM_READWRITE));
}

static void
native_control_fixture_init (NativeControlFixture *fixture)
{
  fixture->choice = GIMP_BRUSH_GENERATED_SQUARE;
  fixture->enabled = TRUE;
}

static GtkWidget *
native_radio_for_value (GtkWidget *first, gint value)
{
  g_assert_true (GTK_IS_RADIO_BUTTON (first));
  for (GSList *iter = gtk_radio_button_get_group (GTK_RADIO_BUTTON (first));
       iter; iter = iter->next)
    if (GPOINTER_TO_INT (g_object_get_data (G_OBJECT (iter->data), "gimp-item-data")) == value)
      return GTK_WIDGET (iter->data);
  return NULL;
}

static void
native_assert_radio_value (GtkWidget *first, gint value, guint count)
{
  GtkWidget *button = native_radio_for_value (first, value);
  g_assert_nonnull (button);
  g_assert_true (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)));
  GSList *group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (first));
  g_assert_cmpuint (g_slist_length (group), ==, count);
  guint active = 0;
  for (GSList *iter = group; iter; iter = iter->next)
    active += gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (iter->data));
  g_assert_cmpuint (active, ==, 1);
}

static void
native_count_notify (GObject *object, GParamSpec *spec, gpointer data)
{
  (void) object; (void) spec;
  ++*(guint *) data;
}

static void
native_property_controls (void)
{
  gimp_set_focused_once (gimp);
  GimpToolInfo *tool = info ("gimp-paintbrush-tool");
  NativeControlFixture *fixture = g_object_new (native_control_fixture_get_type (), NULL);
  GtkWidget *gui = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
  g_object_ref_sink (gui);
  GtkWidget *full = gimp_prop_enum_radio_frame_new (G_OBJECT (fixture), "fixture-choice",
                                                   "Full enum", 0, 0);
  GtkWidget *range = gimp_prop_enum_radio_frame_new (G_OBJECT (fixture), "fixture-choice",
                                                    "Ranged enum", GIMP_BRUSH_GENERATED_SQUARE,
                                                    GIMP_BRUSH_GENERATED_DIAMOND);
  GtkWidget *boolean = gimp_prop_boolean_radio_frame_new (G_OBJECT (fixture), "fixture-enabled",
                                                          "Boolean", "Enabled", "Disabled");
  GtkWidget *icons = gimp_prop_enum_icon_box_new (G_OBJECT (fixture), "fixture-choice",
                                                 "gimp-shape", 0, 0);
  GtkWidget *controls[] = {full, range, boolean, icons};
  GtkWidget *first[4];
  for (guint i = 0; i < G_N_ELEMENTS (controls); ++i)
    {
      g_assert_nonnull (controls[i]);
      gtk_box_pack_start (GTK_BOX (gui), controls[i], FALSE, FALSE, 0);
      if (i < 3)
        first[i] = g_object_get_data (G_OBJECT (controls[i]), "radio-button");
      else
        {
          GList *children = gtk_container_get_children (GTK_CONTAINER (controls[i]));
          g_assert_nonnull (children);
          first[i] = GTK_WIDGET (children->data);
          g_list_free (children);
        }
      g_assert_true (GTK_IS_RADIO_BUTTON (first[i]));
    }
  g_assert_null (native_radio_for_value (first[1], GIMP_BRUSH_GENERATED_CIRCLE));
  native_assert_radio_value (first[0], GIMP_BRUSH_GENERATED_SQUARE, 3);
  native_assert_radio_value (first[1], GIMP_BRUSH_GENERATED_SQUARE, 2);
  native_assert_radio_value (first[2], TRUE, 2);
  native_assert_radio_value (first[3], GIMP_BRUSH_GENERATED_SQUARE, 3);
  guint enum_notifies = 0, boolean_notifies = 0;
  g_signal_connect (fixture, "notify::fixture-choice", G_CALLBACK (native_count_notify), &enum_notifies);
  g_signal_connect (fixture, "notify::fixture-enabled", G_CALLBACK (native_count_notify), &boolean_notifies);

  /* The actual compact presentation consumer adapts native frame children,
   * then restores their original orientation. It does not replace bindings. */
  for (guint pass = 0; pass < 4; ++pass)
    {
      GtkOrientation orientation = pass % 2 ? GTK_ORIENTATION_VERTICAL : GTK_ORIENTATION_HORIZONTAL;
      compact (gui, tool->tool_options, TRUE, orientation);
      for (guint i = 0; i < 3; ++i)
        {
          GtkWidget *box = gtk_bin_get_child (GTK_BIN (controls[i]));
          g_assert_cmpint (gtk_orientable_get_orientation (GTK_ORIENTABLE (box)), ==, orientation);
          g_assert_true (g_object_get_data (G_OBJECT (controls[i]), "radio-button") == first[i]);
        }
      /* The old stock-property orientation argument was ignored. Current
       * native icon-box HORIZONTAL behavior matches that exact limitation. */
      g_assert_cmpint (gtk_orientable_get_orientation (GTK_ORIENTABLE (icons)), ==,
                       GTK_ORIENTATION_HORIZONTAL);
      guint before_enum = enum_notifies, before_boolean = boolean_notifies;
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (
        native_radio_for_value (first[1], GIMP_BRUSH_GENERATED_DIAMOND)), TRUE);
      g_assert_cmpint (fixture->choice, ==, GIMP_BRUSH_GENERATED_DIAMOND);
      g_assert_cmpuint (enum_notifies, ==, before_enum + 1);
      native_assert_radio_value (first[0], GIMP_BRUSH_GENERATED_DIAMOND, 3);
      native_assert_radio_value (first[3], GIMP_BRUSH_GENERATED_DIAMOND, 3);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (native_radio_for_value (first[2], FALSE)), TRUE);
      g_assert_false (fixture->enabled);
      g_assert_cmpuint (boolean_notifies, ==, before_boolean + 1);
      g_object_set (fixture, "fixture-choice", GIMP_BRUSH_GENERATED_SQUARE,
                    "fixture-enabled", TRUE, NULL);
      g_assert_cmpuint (enum_notifies, ==, before_enum + 2);
      g_assert_cmpuint (boolean_notifies, ==, before_boolean + 2);
      native_assert_radio_value (first[0], GIMP_BRUSH_GENERATED_SQUARE, 3);
      native_assert_radio_value (first[1], GIMP_BRUSH_GENERATED_SQUARE, 2);
      native_assert_radio_value (first[2], TRUE, 2);
      native_assert_radio_value (first[3], GIMP_BRUSH_GENERATED_SQUARE, 3);
      gtk_widget_hide (gui);
      gtk_widget_show_all (gui);
      compact (gui, tool->tool_options, FALSE, GTK_ORIENTATION_HORIZONTAL);
      for (guint i = 0; i < 3; ++i)
        g_assert_cmpint (gtk_orientable_get_orientation (
          GTK_ORIENTABLE (gtk_bin_get_child (GTK_BIN (controls[i])))), ==, GTK_ORIENTATION_VERTICAL);
    }
  gboolean retained_gone = FALSE, fixture_gone = FALSE, gui_gone = FALSE;
  gboolean controls_gone[4] = {FALSE, FALSE, FALSE, FALSE};
  g_object_ref (first[0]);
  g_object_set_data_full (G_OBJECT (first[0]), "08017-finalized",
                          &retained_gone, widget_store_finalized);
  g_object_set_data_full (G_OBJECT (fixture), "08017-finalized",
                          &fixture_gone, widget_store_finalized);
  g_object_set_data_full (G_OBJECT (gui), "08017-finalized",
                          &gui_gone, widget_store_finalized);
  for (guint i = 0; i < G_N_ELEMENTS (controls); ++i)
    g_object_set_data_full (G_OBJECT (controls[i]), "08017-finalized",
                            &controls_gone[i], widget_store_finalized);
  gtk_widget_destroy (gui);
  g_assert_false (gui_gone);
  g_assert_false (retained_gone);
  g_object_unref (gui);
  g_assert_true (gui_gone);
  for (guint i = 0; i < G_N_ELEMENTS (controls); ++i)
    g_assert_true (controls_gone[i]);
  g_assert_false (retained_gone);
  g_assert_false (fixture_gone);
  g_object_unref (first[0]);
  g_assert_true (retained_gone);
  /* All widgets are finalized; config notifications remain safe. */
  g_object_set (fixture, "fixture-choice", GIMP_BRUSH_GENERATED_DIAMOND,
                "fixture-enabled", FALSE, NULL);
  g_object_unref (fixture);
  g_assert_true (fixture_gone);
  drain ();
}

typedef struct { guint toggles, released; gint value; } NativeRadioCallback;

static void
native_radio_toggled (GtkToggleButton *button, gpointer data)
{
  NativeRadioCallback *callback = data;
  if (gtk_toggle_button_get_active (button))
    {
      ++callback->toggles;
      callback->value = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (button), "gimp-item-data"));
    }
}

static void
native_radio_released (gpointer data)
{
  ++((NativeRadioCallback *) data)->released;
}

static void
native_enum_integer_helpers (void)
{
  gimp_set_focused_once (gimp);
  /* First-button output, full/range metadata, callback payload and native
   * callback-data release are separate from C++ binding-store slots. */
  for (guint icon = 0; icon < 2; ++icon)
    {
      NativeRadioCallback callback = {0, 0, -1};
      GtkWidget *first = NULL;
      GtkWidget *box = icon ?
        gimp_enum_icon_box_new_with_range (GIMP_TYPE_BRUSH_GENERATED_SHAPE,
          GIMP_BRUSH_GENERATED_SQUARE, GIMP_BRUSH_GENERATED_DIAMOND,
          "gimp-shape", GTK_ICON_SIZE_MENU, G_CALLBACK (native_radio_toggled),
          &callback, native_radio_released, &first) :
        gimp_enum_radio_box_new_with_range (GIMP_TYPE_BRUSH_GENERATED_SHAPE,
          GIMP_BRUSH_GENERATED_SQUARE, GIMP_BRUSH_GENERATED_DIAMOND,
          G_CALLBACK (native_radio_toggled), &callback, native_radio_released, &first);
      g_object_ref_sink (box);
      g_assert_nonnull (first);
      g_assert_cmpint (gtk_orientable_get_orientation (GTK_ORIENTABLE (box)), ==,
                       icon ? GTK_ORIENTATION_HORIZONTAL : GTK_ORIENTATION_VERTICAL);
      GList *children = gtk_container_get_children (GTK_CONTAINER (box));
      g_assert_cmpuint (g_list_length (children), ==, 2);
      g_assert_true (children->data == first);
      g_list_free (children);
      g_assert_cmpint (GPOINTER_TO_INT (g_object_get_data (G_OBJECT (first), "gimp-item-data")), ==,
                       GIMP_BRUSH_GENERATED_SQUARE);
      g_assert_null (native_radio_for_value (first, GIMP_BRUSH_GENERATED_CIRCLE));
      callback.toggles = 0;
      gimp_int_radio_group_set_active (GTK_RADIO_BUTTON (first), GIMP_BRUSH_GENERATED_DIAMOND);
      native_assert_radio_value (first, GIMP_BRUSH_GENERATED_DIAMOND, 2);
      g_assert_cmpuint (callback.toggles, ==, 1);
      g_assert_cmpint (callback.value, ==, GIMP_BRUSH_GENERATED_DIAMOND);
      gboolean gone = FALSE;
      g_object_set_data_full (G_OBJECT (box), "08017-finalized", &gone, widget_store_finalized);
      gtk_widget_destroy (box);
      g_assert_false (gone);
      g_object_unref (box);
      g_assert_true (gone);
      /* The provider's weak-notify callback is a release event; qdata
       * destruction above separately establishes actual finalization. */
      g_assert_cmpuint (callback.released, ==, 1);
    }
  for (guint framed = 0; framed < 2; ++framed)
    {
      NativeRadioCallback callback = {0, 0, -1};
      GtkWidget *zero = NULL, *seven = NULL, *negative = NULL;
      GtkWidget *group = gimp_int_radio_group_new (framed, "Integer values",
        G_CALLBACK (native_radio_toggled), &callback, native_radio_released, 7,
        "Zero", 0, &zero, "Seven", 7, &seven, GINT_TO_POINTER (1), -4, &negative, NULL);
      g_object_ref_sink (group);
      GtkWidget *box = framed ? gtk_bin_get_child (GTK_BIN (group)) : group;
      g_assert_cmpint (gtk_orientable_get_orientation (GTK_ORIENTABLE (box)), ==, GTK_ORIENTATION_VERTICAL);
      g_assert_nonnull (zero); g_assert_nonnull (seven); g_assert_nonnull (negative);
      native_assert_radio_value (zero, 7, 3);
      g_assert_null (gtk_button_get_label (GTK_BUTTON (negative)));
      g_assert_null (g_object_get_data (G_OBJECT (zero), "gimp-item-data"));
      g_assert_cmpint (GPOINTER_TO_INT (g_object_get_data (G_OBJECT (seven), "user_data")), ==, 7);
      g_assert_cmpint (GPOINTER_TO_INT (g_object_get_data (G_OBJECT (negative), "user_data")), ==, -4);
      callback.toggles = 0;
      gimp_int_radio_group_set_active (GTK_RADIO_BUTTON (zero), -4);
      native_assert_radio_value (zero, -4, 3);
      g_assert_cmpuint (callback.toggles, ==, 1);
      g_assert_cmpint (callback.value, ==, -4);
      gimp_int_radio_group_set_active (GTK_RADIO_BUTTON (zero), 0);
      native_assert_radio_value (zero, 0, 3);
      g_assert_cmpuint (callback.toggles, ==, 2);
      g_assert_cmpint (callback.value, ==, 0);
      gboolean gone = FALSE;
      g_object_set_data_full (G_OBJECT (group), "08017-finalized", &gone, widget_store_finalized);
      gtk_widget_destroy (group);
      g_assert_false (gone);
      g_object_unref (group);
      g_assert_true (gone);
      g_assert_cmpuint (callback.released, ==, 1);
    }
  /* Empty-range helper still returns a valid empty native box and clears
   * its output. Property wrappers with an empty range are not exercised. */
  GtkWidget *empty_first = (GtkWidget *) 1;
  GtkWidget *empty = gimp_enum_radio_box_new_with_range (GIMP_TYPE_BRUSH_GENERATED_SHAPE,
    100, 101, NULL, NULL, NULL, &empty_first);
  g_object_ref_sink (empty);
  g_assert_null (empty_first);
  GList *children = gtk_container_get_children (GTK_CONTAINER (empty));
  g_assert_null (children);
  gtk_widget_destroy (empty);
  g_object_unref (empty);
  drain ();
}
