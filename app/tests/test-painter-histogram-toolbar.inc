/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The legacy Threshold/Levels/Curves factories share options, never controls. */
#include "tools/gimpcurvestool.h"
#include "tools/gimphistogramoptions.h"
#include "tools/gimplevelstool.h"
#include "tools/gimpthresholdtool.h"
#include "tools/tool_manager.h"
#include "widgets/gimphistogrambox.h"
#include "widgets/gimphistogramview.h"

static const gchar *histogram_toolbar_tools[] = {
  "gimp-threshold-tool", "gimp-levels-tool", "gimp-curves-tool"
};

static GtkWidget *
histogram_toolbar_radio (GtkWidget *toolbar)
{
  GtkWidget *frame = find (toolbar, "gimp-painter-histogram-scale", GTK_TYPE_FRAME);
  GtkWidget *radio;

  g_assert_nonnull (frame);
  radio = g_object_get_data (G_OBJECT (frame), "radio-button");
  g_assert_true (GTK_IS_RADIO_BUTTON (radio));
  g_assert_cmpint (gtk_orientable_get_orientation
                    (GTK_ORIENTABLE (gtk_widget_get_parent (radio))),
                   ==, GTK_ORIENTATION_HORIZONTAL);
  return radio;
}

static void
histogram_toolbar_assert_scale (GtkWidget *toolbar,
                                gint       expected)
{
  native_assert_radio_value (histogram_toolbar_radio (toolbar), expected, 2);
}

static void
histogram_toolbar_assert_native (GimpToolOptions *options,
                                 GtkWidget       *native,
                                 GPtrArray       *widgets,
                                 GPtrArray       *parents)
{
  g_assert_true (gimp_tools_get_tool_options_gui (options) == native);
  for (guint i = 0; i < widgets->len; i++)
    g_assert_true (gtk_widget_get_parent (g_ptr_array_index (widgets, i)) ==
                   g_ptr_array_index (parents, i));
}

static void
independent_histogram_toolbar (void)
{
  for (guint i = 0; i < G_N_ELEMENTS (histogram_toolbar_tools); i++)
    {
      GimpToolOptions *options = info (histogram_toolbar_tools[i])->tool_options;
      GtkWidget *native = gimp_tools_get_tool_options_gui (options);
      GPtrArray *widgets = g_ptr_array_new ();
      GPtrArray *parents = g_ptr_array_new ();
      GtkWidget *views[2];
      gint original_scale;
      gboolean original_average;
      gboolean original_merged;
      gdouble original_radius;
      guint notifications = 0;

      g_assert_nonnull (native);
      gather (native, widgets);
      for (guint n = 0; n < widgets->len; n++)
        g_ptr_array_add (parents, gtk_widget_get_parent (g_ptr_array_index (widgets, n)));
      g_object_get (options, "histogram-scale", &original_scale,
                    "sample-average", &original_average,
                    "average-radius", &original_radius,
                    "sample-merged", &original_merged, NULL);
      g_object_set (options, "histogram-scale", GIMP_HISTOGRAM_SCALE_LINEAR,
                    "sample-average", TRUE, "average-radius", 3.0, NULL);
      for (guint n = 0; n < G_N_ELEMENTS (views); n++)
        {
          views[n] = gimp_tools_create_painter_toolbar_gui (options);
          g_assert_nonnull (views[n]);
          g_assert_true (g_object_is_floating (views[n]));
          g_object_ref_sink (views[n]);
          g_assert_true (views[n] != native);
          g_assert_cmpint (gtk_orientable_get_orientation (GTK_ORIENTABLE (views[n])),
                           ==, GTK_ORIENTATION_HORIZONTAL);
          histogram_toolbar_assert_scale (views[n], GIMP_HISTOGRAM_SCALE_LINEAR);
        }
      g_assert_true (views[0] != views[1]);
      histogram_toolbar_assert_native (options, native, widgets, parents);
      g_signal_connect (options, "notify::histogram-scale",
                         G_CALLBACK (native_count_notify), &notifications);
      gimp_int_radio_group_set_active (GTK_RADIO_BUTTON (histogram_toolbar_radio (views[0])),
                                       GIMP_HISTOGRAM_SCALE_LOGARITHMIC);
      g_assert_cmpuint (notifications, ==, 1);
      g_assert_cmpint (GIMP_HISTOGRAM_OPTIONS (options)->scale,
                       ==, GIMP_HISTOGRAM_SCALE_LOGARITHMIC);
      histogram_toolbar_assert_scale (views[1], GIMP_HISTOGRAM_SCALE_LOGARITHMIC);

      if (i == 0)
        {
          /* The old Threshold toolbar has no color-sampling controls. */
          for (guint n = 0; n < G_N_ELEMENTS (views); n++)
            {
              g_assert_null (find (views[n], "sample-average", GTK_TYPE_TOGGLE_BUTTON));
              g_assert_null (find (views[n], "average-radius", GTK_TYPE_SPIN_BUTTON));
              g_assert_null (find (views[n], "gimp-painter-histogram-average", GTK_TYPE_MENU_BUTTON));
            }
        }
      else
        {
          GtkWidget *radius = find (views[0], "average-radius", GTK_TYPE_SPIN_BUTTON);
          GtkWidget *peer = find (views[1], "average-radius", GTK_TYPE_SPIN_BUTTON);
          GtkWidget *dock_radius = find (native, "average-radius", GTK_TYPE_SPIN_BUTTON);
          GtkWidget *average = find (views[0], "sample-average", GTK_TYPE_TOGGLE_BUTTON);
          GtkWidget *peer_average = find (views[1], "sample-average", GTK_TYPE_TOGGLE_BUTTON);
          GtkWidget *dock_average = find (native, "sample-average", GTK_TYPE_TOGGLE_BUTTON);
          gdouble actual_radius;
          gboolean actual_average;
          gboolean actual_merged;

          g_assert_nonnull (radius); g_assert_nonnull (peer); g_assert_nonnull (dock_radius);
          g_assert_nonnull (average); g_assert_nonnull (peer_average); g_assert_nonnull (dock_average);
          g_assert_true (radius != peer && radius != dock_radius && peer != dock_radius);
          gtk_spin_button_set_value (GTK_SPIN_BUTTON (radius), 23.0);
          g_object_get (options, "average-radius", &actual_radius, NULL);
          g_assert_cmpfloat (actual_radius, ==, 23.0);
          g_assert_cmpfloat (gtk_spin_button_get_value (GTK_SPIN_BUTTON (peer)), ==, 23.0);
          g_assert_cmpfloat (gtk_spin_button_get_value (GTK_SPIN_BUTTON (dock_radius)), ==, 23.0);
          gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (average), FALSE);
          g_object_get (options, "sample-average", &actual_average, NULL);
          g_assert_false (actual_average);
          g_assert_false (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (peer_average)));
          g_assert_false (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dock_average)));
          g_assert_false (gtk_widget_is_sensitive (radius));
          g_assert_false (gtk_widget_is_sensitive (peer));
          for (guint n = 0; n < G_N_ELEMENTS (views); n++)
            g_assert_false (gtk_widget_get_sensitive
                              (find (views[n], "gimp-painter-histogram-average", GTK_TYPE_MENU_BUTTON)));
          gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dock_average), TRUE);
          gtk_spin_button_set_value (GTK_SPIN_BUTTON (dock_radius), 11.0);
          g_assert_true (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (average)));
          g_assert_true (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (peer_average)));
          g_assert_true (gtk_widget_is_sensitive (radius));
          g_assert_true (gtk_widget_is_sensitive (peer));
          for (guint n = 0; n < G_N_ELEMENTS (views); n++)
            g_assert_true (gtk_widget_get_sensitive
                             (find (views[n], "gimp-painter-histogram-average", GTK_TYPE_MENU_BUTTON)));
          g_assert_cmpfloat (gtk_spin_button_get_value (GTK_SPIN_BUTTON (radius)), ==, 11.0);
          g_assert_cmpfloat (gtk_spin_button_get_value (GTK_SPIN_BUTTON (peer)), ==, 11.0);
          g_object_get (options, "sample-merged", &actual_merged, NULL);
          g_assert_cmpint (actual_merged, ==, original_merged);
        }

      gpointer weak_first = views[0];
      gpointer weak_second = views[1];
      g_object_add_weak_pointer (G_OBJECT (views[0]), &weak_first);
      g_object_add_weak_pointer (G_OBJECT (views[1]), &weak_second);
      gtk_widget_destroy (views[0]);
      g_object_unref (views[0]);
      g_assert_null (weak_first);
      g_object_set (options, "histogram-scale", GIMP_HISTOGRAM_SCALE_LINEAR, NULL);
      g_assert_cmpuint (notifications, ==, 2);
      histogram_toolbar_assert_scale (views[1], GIMP_HISTOGRAM_SCALE_LINEAR);
      histogram_toolbar_assert_native (options, native, widgets, parents);
      gtk_widget_destroy (views[1]);
      g_object_unref (views[1]);
      g_assert_null (weak_second);
      g_signal_handlers_disconnect_by_data (options, &notifications);
      g_object_set (options, "histogram-scale", original_scale,
                    "sample-average", original_average,
                    "average-radius", original_radius, NULL);
      histogram_toolbar_assert_native (options, native, widgets, parents);
      g_ptr_array_unref (widgets);
      g_ptr_array_unref (parents);
    }
}

static void
histogram_toolbar_native_dialogs (void)
{
  GimpContext *context = gimp_get_user_context (gimp);
  GimpToolInfo *previous = gimp_context_get_tool (context);
  GimpDisplayShell *shell;
  GimpImage *image;
  GimpLayer *layer;
  GList selected = { NULL, NULL, NULL };

  gimp_context_set_tool (context, info ("gimp-move-tool"));
  shell = new_shell ();
  image = gimp_display_get_image (shell->display);
  layer = gimp_layer_new (image, 256, 256,
                          gimp_image_get_layer_format (image, TRUE),
                          "Histogram toolbar dialog target", 1.0, GIMP_LAYER_MODE_NORMAL);
  gimp_image_add_layer (image, layer, NULL, 0, FALSE);
  gegl_buffer_clear (gimp_drawable_get_buffer (GIMP_DRAWABLE (layer)), NULL);
  selected.data = layer;
  gimp_image_set_selected_layers (image, &selected);

  for (guint i = 0; i < G_N_ELEMENTS (histogram_toolbar_tools); i++)
    {
      GimpToolInfo *tool_info = info (histogram_toolbar_tools[i]);
      GimpToolOptions *options = tool_info->tool_options;
      GimpTool *tool;
      GtkWidget *native = gimp_tools_get_tool_options_gui (options);
      GtkWidget *toolbar;
      GtkWidget *dialog_view;
      GtkWidget *dialog_parent;
      gpointer weak_view;
      gint original_scale;
      gint actual_scale;

      g_object_get (options, "histogram-scale", &original_scale, NULL);
      gimp_context_set_tool (context, tool_info);
      g_assert_true (tool_manager_initialize_active (gimp, shell->display));
      drain ();
      tool = tool_manager_get_active (gimp);
      g_assert_true (GIMP_IS_FILTER_TOOL (tool));
      if (GIMP_IS_THRESHOLD_TOOL (tool))
        dialog_view = GTK_WIDGET (GIMP_THRESHOLD_TOOL (tool)->histogram_box->view);
      else if (GIMP_IS_LEVELS_TOOL (tool))
        dialog_view = GIMP_LEVELS_TOOL (tool)->histogram_view;
      else
        {
          g_assert_true (GIMP_IS_CURVES_TOOL (tool));
          dialog_view = GIMP_CURVES_TOOL (tool)->graph;
        }
      g_assert_nonnull (dialog_view);
      dialog_parent = gtk_widget_get_parent (dialog_view);
      weak_view = dialog_view;
      g_object_add_weak_pointer (G_OBJECT (dialog_view), &weak_view);
      toolbar = gimp_tools_create_painter_toolbar_gui (options);
      g_assert_nonnull (toolbar);
      g_object_ref_sink (toolbar);
      gimp_int_radio_group_set_active (GTK_RADIO_BUTTON (histogram_toolbar_radio (toolbar)),
                                       GIMP_HISTOGRAM_SCALE_LOGARITHMIC);
      g_object_get (dialog_view, "histogram-scale", &actual_scale, NULL);
      g_assert_cmpint (actual_scale, ==, GIMP_HISTOGRAM_SCALE_LOGARITHMIC);
      g_object_set (dialog_view, "histogram-scale", GIMP_HISTOGRAM_SCALE_LINEAR, NULL);
      histogram_toolbar_assert_scale (toolbar, GIMP_HISTOGRAM_SCALE_LINEAR);
      g_assert_true (gtk_widget_get_parent (dialog_view) == dialog_parent);
      g_assert_true (gimp_tools_get_tool_options_gui (options) == native);

      /* Halt destroys the real filter dialog. Its toolbar peer must still work. */
      tool_manager_control_active (gimp, GIMP_TOOL_ACTION_HALT, shell->display);
      gimp_context_set_tool (context, info ("gimp-move-tool"));
      drain ();
      g_assert_null (weak_view);
      g_object_set (options, "histogram-scale", GIMP_HISTOGRAM_SCALE_LOGARITHMIC, NULL);
      histogram_toolbar_assert_scale (toolbar, GIMP_HISTOGRAM_SCALE_LOGARITHMIC);
      gtk_widget_destroy (toolbar);
      g_object_unref (toolbar);
      g_object_set (options, "histogram-scale", original_scale, NULL);
      g_assert_true (gimp_tools_get_tool_options_gui (options) == native);
    }

  close_shell (shell);
  gimp_context_set_tool (context, previous);
  drain ();
}

/* The lower toolbar is deliberately horizontally scrollable. A popup whose
 * anchor is outside the viewport is not mapped by GTK, even with an active
 * grab. Reveal its real button through the native horizontal adjustment. */
static void
histogram_toolbar_reveal (GtkWidget *button)
{
  GtkWidget *scroll = gtk_widget_get_ancestor (button, GTK_TYPE_SCROLLED_WINDOW);
  GtkWidget *viewport;
  GtkAdjustment *adjustment;
  gdouble value;
  gint x, y;
  gint64 deadline;

  g_assert_nonnull (scroll);
  viewport = gtk_bin_get_child (GTK_BIN (scroll));
  adjustment = gtk_scrolled_window_get_hadjustment (GTK_SCROLLED_WINDOW (scroll));
  deadline = g_get_monotonic_time () + 2 * G_TIME_SPAN_SECOND;
  while ((gtk_widget_get_allocated_width (button) <= 1 ||
          gtk_adjustment_get_page_size (adjustment) <= 1) &&
         g_get_monotonic_time () < deadline)
    {
      g_main_context_iteration (NULL, FALSE);
      g_usleep (1000);
    }
  g_assert_cmpint (gtk_widget_get_allocated_width (button), >, 1);
  g_assert_cmpfloat (gtk_adjustment_get_page_size (adjustment), >, 1);
  g_assert_true (gtk_widget_translate_coordinates (button, viewport, 0, 0, &x, &y));
  g_test_message ("Histogram popup anchor before scroll x=%d width=%d viewport=%.0f offset=%.0f",
                  x, gtk_widget_get_allocated_width (button),
                  gtk_adjustment_get_page_size (adjustment),
                  gtk_adjustment_get_value (adjustment));
  value = gtk_adjustment_get_value (adjustment) + x +
          gtk_widget_get_allocated_width (button) / 2.0 -
          gtk_adjustment_get_page_size (adjustment) / 2.0;
  gtk_adjustment_set_value (adjustment,
                            CLAMP (value, gtk_adjustment_get_lower (adjustment),
                                   gtk_adjustment_get_upper (adjustment) -
                                   gtk_adjustment_get_page_size (adjustment)));
  deadline = g_get_monotonic_time () + 2 * G_TIME_SPAN_SECOND;
  do
    {
      g_main_context_iteration (NULL, FALSE);
      g_assert_true (gtk_widget_translate_coordinates (button, viewport, 0, 0, &x, &y));
      if (x >= 0 && x + gtk_widget_get_allocated_width (button) <=
                    gtk_adjustment_get_page_size (adjustment))
        break;
      g_usleep (1000);
    }
  while (g_get_monotonic_time () < deadline);
  g_test_message ("Histogram popup anchor after scroll x=%d width=%d viewport=%.0f offset=%.0f",
                  x, gtk_widget_get_allocated_width (button),
                  gtk_adjustment_get_page_size (adjustment),
                  gtk_adjustment_get_value (adjustment));
  g_assert_cmpint (x, >=, 0);
  g_assert_cmpfloat (x + gtk_widget_get_allocated_width (button), <=,
                     gtk_adjustment_get_page_size (adjustment));
}

static void
histogram_toolbar_canvas_popup_lifetime (void)
{
  GimpContext *context = gimp_get_user_context (gimp);
  GimpToolInfo *previous = gimp_context_get_tool (context);
  GimpDisplayShell *shell;
  gboolean old_visible = gimp->config->painter_canvas_ui;
  gboolean animations;
  GtkSettings *settings = gtk_settings_get_default ();

  g_object_get (settings, "gtk-enable-animations", &animations, NULL);
  g_object_set (settings, "gtk-enable-animations", FALSE, NULL);
  gimp_context_set_tool (context, info ("gimp-move-tool"));
  shell = new_shell ();
  g_object_set (gimp->config, "painter-canvas-ui", TRUE, NULL);
  gimp_painter_canvas_ui_set_visible (shell, TRUE);

  for (guint i = 1; i < G_N_ELEMENTS (histogram_toolbar_tools); i++)
    {
      GimpToolOptions *options = info (histogram_toolbar_tools[i])->tool_options;
      GtkWidget *native = gimp_tools_get_tool_options_gui (options);
      gboolean original_average;
      g_object_get (options, "sample-average", &original_average, NULL);
      g_object_set (options, "sample-average", TRUE, NULL);
      for (guint cycle = 0; cycle < 3; cycle++)
        {
          GtkWidget *toolbar;
          GtkWidget *button;
          GtkWidget *popup;
          gpointer weak_toolbar;
          gpointer weak_popup;

          gimp_context_set_tool (context, info (histogram_toolbar_tools[i]));
          drain ();
          toolbar = find (shell->canvas, "gimp-painter-tool-toolbar", GTK_TYPE_BOX);
          g_assert_nonnull (toolbar);
          histogram_toolbar_radio (toolbar);
          button = find (toolbar, "gimp-painter-histogram-average", GTK_TYPE_MENU_BUTTON);
          g_assert_nonnull (button);
          histogram_toolbar_reveal (button);
          popup = GTK_WIDGET (gtk_menu_button_get_popover (GTK_MENU_BUTTON (button)));
          g_assert_nonnull (popup);
          weak_toolbar = toolbar;
          weak_popup = popup;
          g_object_add_weak_pointer (G_OBJECT (toolbar), &weak_toolbar);
          g_object_add_weak_pointer (G_OBJECT (popup), &weak_popup);
          gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
          drain ();
          /* An idle drain can finish before the frame-clock allocates the new
           * toolbar. Wait for actual popup mapping, retaining a strict bound. */
          gint64 deadline = g_get_monotonic_time () + 2 * G_TIME_SPAN_SECOND;
          while (! gtk_widget_get_mapped (popup) && g_get_monotonic_time () < deadline)
            {
              g_main_context_iteration (NULL, FALSE);
              g_usleep (1000);
            }
          g_test_message ("Histogram popup active=%d visible=%d mapped=%d realized=%d grab=%d",
                          gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)),
                          gtk_widget_get_visible (popup), gtk_widget_get_mapped (popup),
                          gtk_widget_get_realized (popup), gtk_widget_has_grab (popup));
          g_assert_true (gtk_widget_get_mapped (popup));
          g_assert_true (gtk_widget_has_grab (popup));
          compact_popup_event (popup, GDK_KEY_PRESS, 0, 0);
          drain ();
          g_assert_false (gtk_widget_get_visible (popup));
          g_assert_false (gtk_widget_has_grab (popup));
          gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
          drain ();
          g_assert_true (gtk_widget_get_mapped (popup));
          /* Active-tool switching must destroy its open popup and control tree. */
          gimp_context_set_tool (context, info ("gimp-move-tool"));
          drain ();
          g_assert_null (weak_toolbar);
          g_assert_null (weak_popup);
          g_assert_null (gtk_grab_get_current ());
          g_assert_true (gimp_tools_get_tool_options_gui (options) == native);
          g_object_set (options, "sample-average", FALSE, NULL);
          g_object_set (options, "sample-average", TRUE, NULL);
        }
      g_object_set (options, "sample-average", original_average, NULL);
    }

  close_shell (shell);
  g_object_set (gimp->config, "painter-canvas-ui", old_visible, NULL);
  gimp_context_set_tool (context, previous);
  g_object_set (settings, "gtk-enable-animations", animations, NULL);
  drain ();
}
