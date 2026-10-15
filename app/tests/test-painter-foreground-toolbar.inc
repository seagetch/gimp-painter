/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Current GEGL matting controls in independent legacy-style Details views.
 * This does not establish parity with the old, absent SIOX controls. */
#include "tools/gimpforegroundselectoptions.h"
#include "tools/gimppaintertooltoolbar.h"
#include "widgets/gimpcolorpanel.h"
#include "widgets/gimpcolordialog.h"
#include "widgets/gimptooloptionseditor.h"
#include "widgets/gimpdialogfactory.h"
#include "widgets/gimpdockable.h"
#include "widgets/gimpwindowstrategy.h"
#include "core/gimp-gui.h"

static GtkWidget *
foreground_radio (GtkWidget   *root,
                  const gchar *property)
{
  GtkWidget *bound = find (root, property, 0);
  GtkWidget *radio;

  g_assert_nonnull (bound);
  if (GTK_IS_RADIO_BUTTON (bound))
    return bound;
  radio = g_object_get_data (G_OBJECT (bound), "radio-button");
  if (! radio && GTK_IS_CONTAINER (bound))
    {
      GList *children = gtk_container_get_children (GTK_CONTAINER (bound));
      for (GList *iter = children; iter && ! radio; iter = iter->next)
        if (GTK_IS_RADIO_BUTTON (iter->data))
          radio = iter->data;
      g_list_free (children);
    }
  g_assert_true (GTK_IS_RADIO_BUTTON (radio));
  return radio;
}

static void
foreground_assert_color (GtkWidget *root,
                         gdouble    red,
                         gdouble    green,
                         gdouble    blue,
                         gdouble    alpha)
{
  GtkWidget *button = find (root, "mask-color", GIMP_TYPE_COLOR_BUTTON);
  GeglColor *color;
  gdouble r, g, b, a;

  g_assert_nonnull (button);
  color = gimp_color_button_get_color (GIMP_COLOR_BUTTON (button));
  gegl_color_get_rgba (color, &r, &g, &b, &a);
  g_assert_cmpfloat_with_epsilon (r, red, 1e-6);
  g_assert_cmpfloat_with_epsilon (g, green, 1e-6);
  g_assert_cmpfloat_with_epsilon (b, blue, 1e-6);
  g_assert_cmpfloat_with_epsilon (a, alpha, 1e-6);
  g_object_unref (color);
}

static void
foreground_assert_engine (GtkWidget *root,
                          gint       engine)
{
  GtkWidget *combo = find (root, "engine", GIMP_TYPE_INT_COMBO_BOX);
  gint value = -1;

  g_assert_nonnull (combo);
  g_assert_true (gimp_int_combo_box_get_active (GIMP_INT_COMBO_BOX (combo), &value));
  g_assert_cmpint (value, ==, engine);
  g_assert_cmpint (gtk_widget_get_sensitive (combo), ==,
                   gegl_has_operation ("gegl:matting-levin"));
  g_assert_cmpint (gtk_widget_get_visible (find (root, "levels", GTK_TYPE_SPIN_BUTTON)),
                   ==, engine == GIMP_MATTING_ENGINE_LEVIN);
  g_assert_cmpint (gtk_widget_get_visible (find (root, "active-levels", GTK_TYPE_SPIN_BUTTON)),
                   ==, engine == GIMP_MATTING_ENGINE_LEVIN);
  g_assert_cmpint (gtk_widget_get_visible (find (root, "iterations", GTK_TYPE_SPIN_BUTTON)),
                   ==, engine == GIMP_MATTING_ENGINE_GLOBAL);
}

static void
foreground_assert_native (GimpToolOptions *options,
                          GtkWidget       *native,
                          GtkWidget       *mode_box,
                          GtkWidget       *antialias,
                          GPtrArray       *widgets,
                          GPtrArray       *parents)
{
  g_assert_true (gimp_tools_get_tool_options_gui (options) == native);
  g_assert_true (GIMP_SELECTION_OPTIONS (options)->mode_box == mode_box);
  g_assert_true (GIMP_SELECTION_OPTIONS (options)->antialias_toggle == antialias);
  for (guint i = 0; i < widgets->len; i++)
    g_assert_true (gtk_widget_get_parent (g_ptr_array_index (widgets, i)) ==
                   g_ptr_array_index (parents, i));
}

static void
independent_foreground_toolbar (void)
{
  GimpToolOptions *options = info ("gimp-foreground-select-tool")->tool_options;
  GimpConfig *saved = gimp_config_duplicate (GIMP_CONFIG (options));
  GtkWidget *native = gimp_tools_get_tool_options_gui (options);
  GtkWidget *native_mode = GIMP_SELECTION_OPTIONS (options)->mode_box;
  GtkWidget *native_antialias = GIMP_SELECTION_OPTIONS (options)->antialias_toggle;
  GPtrArray *native_widgets = g_ptr_array_new ();
  GPtrArray *native_parents = g_ptr_array_new ();
  GPtrArray *trees[2] = { g_ptr_array_new (), g_ptr_array_new () };
  GtkWidget *views[2];
  gpointer weak[2];
  const struct { const gchar *property; gint initial; gint value; guint count; } enums[] = {
    { "operation", GIMP_CHANNEL_OP_REPLACE, GIMP_CHANNEL_OP_INTERSECT, 4 },
    { "draw-mode", GIMP_MATTING_DRAW_MODE_FOREGROUND, GIMP_MATTING_DRAW_MODE_BACKGROUND, 3 },
    { "preview-mode", GIMP_MATTING_PREVIEW_MODE_ON_COLOR, GIMP_MATTING_PREVIEW_MODE_GRAYSCALE, 2 }
  };
  const struct { const gchar *property; gint value; } numbers[] = {
    { "stroke-width", 43 }, { "levels", 6 }, { "active-levels", 5 }, { "iterations", 4 }
  };
  const gchar *absent_siox[] = {
    "contiguous", "smoothness", "expanded",
    "sensitivity-l", "sensitivity-a", "sensitivity-b"
  };
  guint notifications;
  guint background_notifications = 0;
  GParamSpec *background_spec;
  GeglColor *background;
  gdouble original_background[4];
  gdouble actual_background[4];

  /* The old Foreground Boolean named background no longer exists. A
   * same-named inherited context COLOR is a separate property, not a
   * marking-mode substitute. Assert its ownership/type and leave it alone. */
  background_spec = g_object_class_find_property (G_OBJECT_GET_CLASS (options), "background");
  g_assert_nonnull (background_spec);
  g_assert_true (background_spec->owner_type == GIMP_TYPE_CONTEXT);
  g_assert_true (G_PARAM_SPEC_VALUE_TYPE (background_spec) == GEGL_TYPE_COLOR);
  g_object_get (options, "background", &background, NULL);
  gegl_color_get_rgba (background, &original_background[0], &original_background[1],
                       &original_background[2], &original_background[3]);
  g_object_unref (background);
  g_signal_connect (options, "notify::background",
                     G_CALLBACK (native_count_notify), &background_notifications);

  g_assert_nonnull (native_mode);
  g_assert_nonnull (native_antialias);
  gather (native, native_widgets);
  for (guint i = 0; i < native_widgets->len; i++)
    g_ptr_array_add (native_parents, gtk_widget_get_parent (g_ptr_array_index (native_widgets, i)));
  g_object_set (options, "feather", FALSE, "feather-radius", 3.0,
                "stroke-width", 10, "levels", 2, "active-levels", 2,
                "iterations", 2, "engine", GIMP_MATTING_ENGINE_GLOBAL, NULL);
  for (guint i = 0; i < G_N_ELEMENTS (enums); i++)
    g_object_set (options, enums[i].property, enums[i].initial, NULL);
  for (guint i = 0; i < G_N_ELEMENTS (views); i++)
    {
      views[i] = gimp_tools_create_painter_toolbar_gui (options);
      g_assert_nonnull (views[i]);
      g_assert_true (g_object_is_floating (views[i]));
      g_object_ref_sink (views[i]);
      g_assert_cmpint (gtk_orientable_get_orientation (GTK_ORIENTABLE (views[i])),
                       ==, GTK_ORIENTATION_HORIZONTAL);
      gather (views[i], trees[i]);
      for (guint n = 0; n < trees[i]->len; n++)
        g_assert_false (g_ptr_array_find (native_widgets, g_ptr_array_index (trees[i], n), NULL));
      gtk_widget_show_all (views[i]);
      g_assert_false (gtk_widget_get_visible (find (views[i], "antialias", GTK_TYPE_CHECK_BUTTON)));
      foreground_assert_engine (views[i], GIMP_MATTING_ENGINE_GLOBAL);
      weak[i] = views[i];
      g_object_add_weak_pointer (G_OBJECT (views[i]), &weak[i]);
    }
  for (guint i = 0; i < trees[0]->len; i++)
    g_assert_false (g_ptr_array_find (trees[1], g_ptr_array_index (trees[0], i), NULL));
  foreground_assert_native (options, native, native_mode, native_antialias,
                            native_widgets, native_parents);

  for (guint i = 0; i < G_N_ELEMENTS (enums); i++)
    {
      gchar *signal = g_strdup_printf ("notify::%s", enums[i].property);
      gint actual;
      notifications = 0;
      g_signal_connect (options, signal, G_CALLBACK (native_count_notify), &notifications);
      gimp_int_radio_group_set_active (GTK_RADIO_BUTTON (foreground_radio (views[0], enums[i].property)),
                                       enums[i].value);
      g_assert_cmpuint (notifications, ==, 1);
      g_object_get (options, enums[i].property, &actual, NULL);
      g_assert_cmpint (actual, ==, enums[i].value);
      native_assert_radio_value (foreground_radio (views[1], enums[i].property), enums[i].value, enums[i].count);
      native_assert_radio_value (foreground_radio (native, enums[i].property), enums[i].value, enums[i].count);
      gimp_int_radio_group_set_active (GTK_RADIO_BUTTON (foreground_radio (native, enums[i].property)),
                                       enums[i].initial);
      g_assert_cmpuint (notifications, ==, 2);
      native_assert_radio_value (foreground_radio (views[0], enums[i].property), enums[i].initial, enums[i].count);
      native_assert_radio_value (foreground_radio (views[1], enums[i].property), enums[i].initial, enums[i].count);
      g_signal_handlers_disconnect_by_data (options, &notifications);
      g_free (signal);
    }
  /* The modern third draw mode must also work in all independent trees. */
  gimp_int_radio_group_set_active (GTK_RADIO_BUTTON (foreground_radio (views[1], "draw-mode")),
                                   GIMP_MATTING_DRAW_MODE_UNKNOWN);
  native_assert_radio_value (foreground_radio (views[0], "draw-mode"), GIMP_MATTING_DRAW_MODE_UNKNOWN, 3);
  native_assert_radio_value (foreground_radio (native, "draw-mode"), GIMP_MATTING_DRAW_MODE_UNKNOWN, 3);

  for (guint i = 0; i < G_N_ELEMENTS (numbers); i++)
    {
      gchar *signal = g_strdup_printf ("notify::%s", numbers[i].property);
      GtkWidget *spin = find (views[0], numbers[i].property, GTK_TYPE_SPIN_BUTTON);
      GtkWidget *peer = find (views[1], numbers[i].property, GTK_TYPE_SPIN_BUTTON);
      GtkWidget *dock = find (native, numbers[i].property, GTK_TYPE_SPIN_BUTTON);
      gint actual;
      gdouble low, high, step, page;
      g_assert_nonnull (spin); g_assert_nonnull (peer); g_assert_nonnull (dock);
      gtk_spin_button_get_range (GTK_SPIN_BUTTON (spin), &low, &high);
      g_assert_cmpfloat (low, ==, 1.0);
      g_assert_cmpfloat (high, ==, i == 0 ? 6000.0 : 10.0);
      g_assert_cmpuint (gtk_spin_button_get_digits (GTK_SPIN_BUTTON (spin)), ==, 0);
      gtk_spin_button_get_increments (GTK_SPIN_BUTTON (spin), &step, &page);
      g_assert_cmpfloat (step, ==, 1.0);
      g_assert_cmpfloat (page, ==, i == 0 ? 10.0 : 1.0);
      notifications = 0;
      g_signal_connect (options, signal, G_CALLBACK (native_count_notify), &notifications);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (spin), numbers[i].value);
      g_assert_cmpuint (notifications, ==, 1);
      g_object_get (options, numbers[i].property, &actual, NULL);
      g_assert_cmpint (actual, ==, numbers[i].value);
      g_assert_cmpfloat (gtk_spin_button_get_value (GTK_SPIN_BUTTON (peer)), ==, numbers[i].value);
      g_assert_cmpfloat (gtk_spin_button_get_value (GTK_SPIN_BUTTON (dock)), ==, numbers[i].value);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (dock), numbers[i].value + 1);
      g_assert_cmpuint (notifications, ==, 2);
      g_assert_cmpfloat (gtk_spin_button_get_value (GTK_SPIN_BUTTON (spin)), ==, numbers[i].value + 1);
      g_assert_cmpfloat (gtk_spin_button_get_value (GTK_SPIN_BUTTON (peer)), ==, numbers[i].value + 1);
      g_signal_handlers_disconnect_by_data (options, &notifications);
      g_free (signal);
    }
  notifications = 0;
  g_signal_connect (options, "notify::stroke-width", G_CALLBACK (native_count_notify), &notifications);
  gtk_button_clicked (GTK_BUTTON (find (views[0], "gimp-painter-foreground-reset-width", GTK_TYPE_BUTTON)));
  g_assert_cmpuint (notifications, ==, 1);
  g_assert_cmpfloat (gtk_spin_button_get_value (GTK_SPIN_BUTTON (find (views[1], "stroke-width", GTK_TYPE_SPIN_BUTTON))), ==, 10.0);
  g_assert_cmpfloat (gtk_spin_button_get_value (GTK_SPIN_BUTTON (find (native, "stroke-width", GTK_TYPE_SPIN_BUTTON))), ==, 10.0);
  g_signal_handlers_disconnect_by_data (options, &notifications);

  for (guint enabled = 0; enabled < 2; enabled++)
    {
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (find (native, "feather", GTK_TYPE_TOGGLE_BUTTON)), enabled);
      for (guint i = 0; i < G_N_ELEMENTS (views); i++)
        {
          g_assert_cmpint (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (find (views[i], "feather", GTK_TYPE_TOGGLE_BUTTON))), ==, enabled);
          g_assert_cmpint (gtk_widget_is_sensitive (find (views[i], "feather-radius", GTK_TYPE_SPIN_BUTTON)), ==, enabled);
          g_assert_cmpint (gtk_widget_get_sensitive (find (views[i], "gimp-painter-selection-feather", GTK_TYPE_MENU_BUTTON)), ==, enabled);
        }
    }
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (find (views[1], "feather-radius", GTK_TYPE_SPIN_BUTTON)), 19.5);
  g_assert_cmpfloat (GIMP_SELECTION_OPTIONS (options)->feather_radius, ==, 19.5);
  g_assert_cmpfloat (gtk_spin_button_get_value (GTK_SPIN_BUTTON (find (native, "feather-radius", GTK_TYPE_SPIN_BUTTON))), ==, 19.5);
  g_assert_cmpfloat (gtk_spin_button_get_value (GTK_SPIN_BUTTON (find (views[0], "feather-radius", GTK_TYPE_SPIN_BUTTON))), ==, 19.5);

  GeglColor *color = gegl_color_new ("black");
  gegl_color_set_rgba (color, 0.2, 0.4, 0.8, 0.3);
  notifications = 0;
  g_signal_connect (options, "notify::mask-color", G_CALLBACK (native_count_notify), &notifications);
  gimp_color_button_set_color (GIMP_COLOR_BUTTON (find (views[0], "mask-color", GIMP_TYPE_COLOR_BUTTON)), color);
  g_assert_cmpuint (notifications, ==, 1);
  foreground_assert_color (views[1], 0.2, 0.4, 0.8, 0.3);
  foreground_assert_color (native, 0.2, 0.4, 0.8, 0.3);
  gegl_color_set_rgba (color, 0.8, 0.1, 0.3, 0.7);
  gimp_color_button_set_color (GIMP_COLOR_BUTTON (find (native, "mask-color", GIMP_TYPE_COLOR_BUTTON)), color);
  g_assert_cmpuint (notifications, ==, 2);
  foreground_assert_color (views[0], 0.8, 0.1, 0.3, 0.7);
  foreground_assert_color (views[1], 0.8, 0.1, 0.3, 0.7);
  g_object_unref (color);
  g_signal_handlers_disconnect_by_data (options, &notifications);

  for (guint cycle = 0; cycle < 3; cycle++)
    {
      gint expected = gegl_has_operation ("gegl:matting-levin") ?
                      GIMP_MATTING_ENGINE_LEVIN : GIMP_MATTING_ENGINE_GLOBAL;
      /* Unsupported Levin is rejected by the model rather than exposed as
       * a falsely functioning control. Both branches are asserted, no skip. */
      g_object_set (options, "engine", GIMP_MATTING_ENGINE_LEVIN, NULL);
      for (guint i = 0; i < G_N_ELEMENTS (views); i++)
        {
          gtk_widget_show_all (views[i]);
          GtkWidget *button = find (views[i], "gimp-painter-foreground-details", GTK_TYPE_MENU_BUTTON);
          gtk_widget_show_all (gtk_bin_get_child
                               (GTK_BIN (gtk_menu_button_get_popover (GTK_MENU_BUTTON (button)))));
          foreground_assert_engine (views[i], expected);
        }
      foreground_assert_engine (native, expected);
      gimp_int_combo_box_set_active (GIMP_INT_COMBO_BOX (find (views[1], "engine", GIMP_TYPE_INT_COMBO_BOX)),
                                     GIMP_MATTING_ENGINE_GLOBAL);
      foreground_assert_engine (views[0], GIMP_MATTING_ENGINE_GLOBAL);
      foreground_assert_engine (views[1], GIMP_MATTING_ENGINE_GLOBAL);
      foreground_assert_engine (native, GIMP_MATTING_ENGINE_GLOBAL);
    }
  for (guint i = 0; i < G_N_ELEMENTS (absent_siox); i++)
    {
      g_assert_null (g_object_class_find_property (G_OBJECT_GET_CLASS (options), absent_siox[i]));
      g_assert_null (find (views[0], absent_siox[i], 0));
    }
  g_assert_null (find (views[0], "background", 0));
  g_assert_null (find (views[1], "background", 0));
  gtk_widget_destroy (views[0]);
  g_object_unref (views[0]);
  g_assert_null (weak[0]);
  g_object_set (options, "stroke-width", 71, "iterations", 9, NULL);
  g_assert_cmpfloat (gtk_spin_button_get_value (GTK_SPIN_BUTTON (find (views[1], "stroke-width", GTK_TYPE_SPIN_BUTTON))), ==, 71.0);
  g_assert_cmpfloat (gtk_spin_button_get_value (GTK_SPIN_BUTTON (find (native, "iterations", GTK_TYPE_SPIN_BUTTON))), ==, 9.0);
  foreground_assert_native (options, native, native_mode, native_antialias,
                            native_widgets, native_parents);
  gtk_widget_destroy (views[1]);
  g_object_unref (views[1]);
  g_assert_null (weak[1]);
  g_assert_cmpuint (background_notifications, ==, 0);
  g_object_get (options, "background", &background, NULL);
  gegl_color_get_rgba (background, &actual_background[0], &actual_background[1],
                       &actual_background[2], &actual_background[3]);
  g_object_unref (background);
  for (guint i = 0; i < G_N_ELEMENTS (original_background); i++)
    g_assert_cmpfloat (actual_background[i], ==, original_background[i]);
  g_signal_handlers_disconnect_by_data (options, &background_notifications);
  gimp_config_sync (G_OBJECT (saved), G_OBJECT (options), 0);
  g_object_unref (saved);
  foreground_assert_native (options, native, native_mode, native_antialias,
                            native_widgets, native_parents);
  g_ptr_array_unref (native_widgets);
  g_ptr_array_unref (native_parents);
  g_ptr_array_unref (trees[0]);
  g_ptr_array_unref (trees[1]);
}

static void
foreground_toolbar_model_before_view (void)
{
  GimpToolOptions *options = g_object_new (GIMP_TYPE_FOREGROUND_SELECT_OPTIONS,
                                          "gimp", gimp,
                                          "tool-info", info ("gimp-foreground-select-tool"), NULL);
  GtkWidget *first;
  GtkWidget *second;
  GtkWidget *reset;
  gpointer weak_options = options;

  /* The canvas dispatcher intentionally admits only the tool-info's
   * canonical options object. A fresh lifetime fixture uses the same real
   * production builder directly, never substitutes itself into tool_info. */
  g_assert_true (options->tool_info->tool_options != options);
  g_assert_null (gimp_tools_create_painter_toolbar_gui (options));
  first = gimp_painter_tool_toolbar_new (options);
  second = gimp_painter_tool_toolbar_new (options);
  g_assert_nonnull (first); g_assert_nonnull (second);
  g_object_ref_sink (first); g_object_ref_sink (second);
  g_assert_null (GIMP_SELECTION_OPTIONS (options)->mode_box);
  g_assert_null (GIMP_SELECTION_OPTIONS (options)->antialias_toggle);
  reset = g_object_ref (find (first, "gimp-painter-foreground-reset-width", GTK_TYPE_BUTTON));
  g_object_add_weak_pointer (G_OBJECT (options), &weak_options);
  g_object_unref (options);
  g_assert_nonnull (weak_options);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (find (first, "stroke-width", GTK_TYPE_SPIN_BUTTON)), 81.0);
  g_assert_cmpfloat (gtk_spin_button_get_value (GTK_SPIN_BUTTON (find (second, "stroke-width", GTK_TYPE_SPIN_BUTTON))), ==, 81.0);
  gtk_widget_destroy (first); g_object_unref (first);
  g_assert_nonnull (weak_options);
  gtk_widget_destroy (second); g_object_unref (second);
  g_assert_null (weak_options);
  /* A retained destroyed reset widget must not dereference the dead model. */
  gtk_button_clicked (GTK_BUTTON (reset));
  g_object_unref (reset);
}

static void
foreground_toolbar_canvas_popup_lifetime (void)
{
  GimpContext *context = gimp_get_user_context (gimp);
  GimpToolInfo *previous = gimp_context_get_tool (context);
  GimpToolOptions *options = info ("gimp-foreground-select-tool")->tool_options;
  GtkWidget *native = gimp_tools_get_tool_options_gui (options);
  GtkWidget *native_parent;
  GtkWidget *native_editor;
  GtkWidget *native_dockable;
  GtkWidget *existing_dockable;
  GimpDialogFactory *factory = gimp_dialog_factory_get_singleton ();
  GtkWidget *native_mode = GIMP_SELECTION_OPTIONS (options)->mode_box;
  GtkWidget *native_antialias = GIMP_SELECTION_OPTIONS (options)->antialias_toggle;
  GPtrArray *native_widgets = g_ptr_array_new ();
  GPtrArray *native_parents = g_ptr_array_new ();
  GimpDisplayShell *shell;
  gboolean old_visible = gimp->config->painter_canvas_ui;
  GeglColor *original_color;
  GeglColor *test_color = gegl_color_new ("black");
  gboolean animations;
  GtkSettings *settings = gtk_settings_get_default ();

  gather (native, native_widgets);
  for (guint i = 0; i < native_widgets->len; i++)
    g_ptr_array_add (native_parents, gtk_widget_get_parent (g_ptr_array_index (native_widgets, i)));
  g_object_get (options, "mask-color", &original_color, NULL);
  gegl_color_set_rgba (test_color, 0.1, 0.2, 0.3, 0.4);
  g_object_set (options, "mask-color", test_color, NULL);
  g_object_get (settings, "gtk-enable-animations", &animations, NULL);
  g_object_set (settings, "gtk-enable-animations", FALSE, NULL);
  gimp_context_set_tool (context, info ("gimp-move-tool"));
  shell = new_shell ();
  g_test_message ("Foreground fixture tool after new shell: %s",
                  gimp_object_get_name (gimp_context_get_tool (context)));
  g_object_set (gimp->config, "painter-canvas-ui", TRUE, NULL);
  gimp_painter_canvas_ui_set_visible (shell, TRUE);
  drain ();
  /* A selected tool does not imply that a Tool Options editor exists.
   * Open the actual registered native Dockable, with its ordinary editor
   * and dockbook, before deriving ownership from its populated Move GUI. */
  existing_dockable = gimp_dialog_factory_find_widget (factory, "gimp-tool-options");
  native_dockable = gimp_window_strategy_show_dockable_dialog
    (GIMP_WINDOW_STRATEGY (gimp_get_window_strategy (gimp)), gimp, factory,
     gimp_widget_get_monitor (GTK_WIDGET (shell)), "gimp-tool-options");
  g_assert_true (GIMP_IS_DOCKABLE (native_dockable));
  g_assert_nonnull (gimp_dockable_get_dockbook (GIMP_DOCKABLE (native_dockable)));
  native_editor = gtk_bin_get_child (GTK_BIN (native_dockable));
  g_assert_true (GIMP_IS_TOOL_OPTIONS_EDITOR (native_editor));
  drain ();
  g_test_message ("Foreground fixture tool after native Dockable initialization: %s",
                  gimp_object_get_name (gimp_context_get_tool (context)));
  /* Shell/device initialization can restore the profile's tool after the
   * earlier selection. Establish Move only after that real setup is done. */
  gimp_context_set_tool (context, info ("gimp-move-tool"));
  drain ();
  g_assert_true (gimp_context_get_tool (context) == info ("gimp-move-tool"));
  GtkWidget *move_native = gimp_tools_get_tool_options_gui
    (info ("gimp-move-tool")->tool_options);
  native_parent = gtk_widget_get_parent (move_native);
  g_assert_nonnull (native_parent);
  g_assert_true (gtk_widget_get_ancestor (move_native, GIMP_TYPE_TOOL_OPTIONS_EDITOR) == native_editor);
  g_assert_true (gtk_widget_is_ancestor (move_native, native_dockable));
  g_assert_true (gimp_tool_options_editor_get_tool_options (GIMP_TOOL_OPTIONS_EDITOR (native_editor)) ==
                 info ("gimp-move-tool")->tool_options);
  if (gtk_widget_get_parent (native))
    g_assert_true (gtk_widget_get_parent (native) == native_parent);
  g_assert_true (g_ptr_array_index (native_widgets, 0) == native);
  g_ptr_array_index (native_parents, 0) = native_parent;
  for (guint cycle = 0; cycle < 3; cycle++)
    {
      GtkWidget *toolbar, *button, *popup;
      gpointer weak_toolbar, weak_popup, weak_color_dialog;
      gint64 deadline;

      gimp_context_set_tool (context, info ("gimp-foreground-select-tool"));
      drain ();
      toolbar = find (shell->canvas, "gimp-painter-tool-toolbar", GTK_TYPE_BOX);
      g_assert_nonnull (toolbar);
      foreground_assert_native (options, native, native_mode, native_antialias,
                                native_widgets, native_parents);
      g_assert_true (gtk_widget_get_ancestor (native, GIMP_TYPE_TOOL_OPTIONS_EDITOR) == native_editor);
      g_assert_true (gtk_widget_is_ancestor (native, native_dockable));
      g_assert_true (gimp_tool_options_editor_get_tool_options (GIMP_TOOL_OPTIONS_EDITOR (native_editor)) == options);
      g_assert_false (gtk_widget_is_ancestor (native, toolbar));
      g_assert_true (gtk_widget_get_visible (native));
      button = find (toolbar, "gimp-painter-foreground-details", GTK_TYPE_MENU_BUTTON);
      g_assert_nonnull (button);
      /* This utility verifies real horizontal scrolling and anchor bounds. */
      histogram_toolbar_reveal (button);
      popup = GTK_WIDGET (gtk_menu_button_get_popover (GTK_MENU_BUTTON (button)));
      g_assert_nonnull (popup);
      g_assert_cmpint (gtk_popover_get_position (GTK_POPOVER (popup)), ==, GTK_POS_TOP);
      weak_toolbar = toolbar; weak_popup = popup;
      g_object_add_weak_pointer (G_OBJECT (toolbar), &weak_toolbar);
      g_object_add_weak_pointer (G_OBJECT (popup), &weak_popup);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
      deadline = g_get_monotonic_time () + 2 * G_TIME_SPAN_SECOND;
      while (! gtk_widget_get_mapped (popup) && g_get_monotonic_time () < deadline)
        {
          g_main_context_iteration (NULL, FALSE);
          g_usleep (1000);
        }
      g_assert_true (gtk_widget_get_mapped (popup));
      g_assert_true (gtk_widget_has_grab (popup));
      compact_popup_event (popup, GDK_KEY_PRESS, 0, 0);
      drain ();
      g_assert_false (gtk_widget_get_visible (popup));
      g_assert_false (gtk_widget_has_grab (popup));
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
      drain ();
      g_assert_true (gtk_widget_get_mapped (popup));
      weak_color_dialog = NULL;
      if (cycle != 1)
        {
          GtkWidget *color = find (toolbar, "mask-color", GIMP_TYPE_COLOR_PANEL);
          g_assert_nonnull (color);
          gtk_button_clicked (GTK_BUTTON (color));
          drain ();
          weak_color_dialog = GIMP_COLOR_PANEL (color)->color_dialog;
          g_assert_nonnull (weak_color_dialog);
          g_assert_true (gtk_widget_get_visible (weak_color_dialog));
          g_object_add_weak_pointer (G_OBJECT (weak_color_dialog), &weak_color_dialog);
          if (cycle == 0)
            {
              GtkWidget *dialog = GIMP_COLOR_PANEL (color)->color_dialog;
              GtkWidget *cancel = gtk_dialog_get_widget_for_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL);
              GtkWidget *accept = gtk_dialog_get_widget_for_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
              g_assert_nonnull (cancel); g_assert_nonnull (accept);
              gegl_color_set_rgba (test_color, 0.9, 0.6, 0.2, 0.8);
              gimp_color_selection_set_color (GIMP_COLOR_SELECTION (GIMP_COLOR_DIALOG (dialog)->selection), test_color);
              gtk_button_clicked (GTK_BUTTON (cancel));
              drain ();
              g_assert_false (gtk_widget_get_visible (dialog));
              foreground_assert_color (toolbar, 0.1, 0.2, 0.3, 0.4);
              foreground_assert_color (native, 0.1, 0.2, 0.3, 0.4);
              gtk_button_clicked (GTK_BUTTON (color));
              drain ();
              g_assert_true (gtk_widget_get_visible (dialog));
              gimp_color_selection_set_color (GIMP_COLOR_SELECTION (GIMP_COLOR_DIALOG (dialog)->selection), test_color);
              gtk_button_clicked (GTK_BUTTON (accept));
              drain ();
              g_assert_false (gtk_widget_get_visible (dialog));
              foreground_assert_color (toolbar, 0.9, 0.6, 0.2, 0.8);
              foreground_assert_color (native, 0.9, 0.6, 0.2, 0.8);
              gtk_button_clicked (GTK_BUTTON (color));
              drain ();
              g_assert_true (gtk_widget_get_visible (dialog));
            }
        }
      else
        {
          /* Independently cover tool switching with the popup's actual grab,
           * before a color dialog can change GTK focus/grab state. */
          g_assert_true (gtk_widget_get_mapped (popup));
          g_assert_true (gtk_widget_has_grab (popup));
        }
      gimp_context_set_tool (context, info ("gimp-move-tool"));
      drain ();
      g_assert_null (weak_toolbar);
      g_assert_null (weak_popup);
      g_assert_null (weak_color_dialog);
      g_assert_null (gtk_grab_get_current ());
      foreground_assert_native (options, native, native_mode, native_antialias,
                                native_widgets, native_parents);
      g_assert_true (gtk_widget_get_ancestor (native, GIMP_TYPE_TOOL_OPTIONS_EDITOR) == native_editor);
      g_assert_true (gtk_widget_is_ancestor (native, native_dockable));
      g_assert_true (gimp_tool_options_editor_get_tool_options (GIMP_TOOL_OPTIONS_EDITOR (native_editor)) ==
                     info ("gimp-move-tool")->tool_options);
      g_assert_false (gtk_widget_get_visible (native));
    }
  g_ptr_array_unref (native_widgets);
  g_ptr_array_unref (native_parents);
  if (! existing_dockable)
    gtk_widget_destroy (native_dockable);
  close_shell (shell);
  g_object_set (options, "mask-color", original_color, NULL);
  g_object_unref (original_color);
  g_object_unref (test_color);
  g_object_set (gimp->config, "painter-canvas-ui", old_visible, NULL);
  gimp_context_set_tool (context, previous);
  g_object_set (settings, "gtk-enable-animations", animations, NULL);
  drain ();
}
