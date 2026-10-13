/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Original08.017 native APIs. Shared helpers/fixture are defined immediately
 * above in test-widget-store-native-controls.inc; no C++ ownership is faked. */
static GtkWidget *
orientation_box (GtkWidget *widget)
{
  return GTK_IS_FRAME (widget) ? gtk_bin_get_child (GTK_BIN (widget)) : widget;
}

static void
orientation_assert (GtkWidget *widget, GtkOrientation expected, gint spacing)
{
  GtkWidget *box = orientation_box (widget);
  g_assert_true (GTK_IS_BOX (box));
  g_assert_cmpint (gtk_orientable_get_orientation (GTK_ORIENTABLE (box)), ==, expected);
  g_assert_false (gtk_box_get_homogeneous (GTK_BOX (box)));
  g_assert_cmpint (gtk_box_get_spacing (GTK_BOX (box)), ==, spacing);
}

static void
orientation_enum_contract (void)
{
  gimp_set_focused_once (gimp);
  for (guint direction = 0; direction < 2; ++direction)
    for (guint kind = 0; kind < 4; ++kind)
      {
        GtkOrientation orientation = direction ? GTK_ORIENTATION_VERTICAL : GTK_ORIENTATION_HORIZONTAL;
        NativeRadioCallback callback = {0, 0, -1};
        GtkWidget *first = NULL, *control = NULL;
        GtkWidget *label = (kind == 1 || kind == 2) ? gtk_label_new ("Original label") : NULL;
        gboolean label_gone = FALSE, first_gone = FALSE;
        if (label)
          g_object_set_data_full (G_OBJECT (label), "08017-finalized", &label_gone, widget_store_finalized);
        switch (kind)
          {
          case 0:
            control = gimp_enum_radio_box_new_with_orientation (GIMP_TYPE_BRUSH_GENERATED_SHAPE,
              G_CALLBACK (native_radio_toggled), &callback, &first, orientation);
            break;
          case 1:
            control = gimp_enum_radio_frame_new_with_orientation (GIMP_TYPE_BRUSH_GENERATED_SHAPE,
              label, G_CALLBACK (native_radio_toggled), &callback, &first, orientation);
            break;
          case 2:
            control = gimp_enum_radio_frame_new_full (GIMP_TYPE_BRUSH_GENERATED_SHAPE,
              GIMP_BRUSH_GENERATED_SQUARE, GIMP_BRUSH_GENERATED_DIAMOND, label,
              G_CALLBACK (native_radio_toggled), &callback, &first, orientation);
            break;
          case 3:
            control = gimp_enum_stock_box_new_with_orientation (GIMP_TYPE_BRUSH_GENERATED_SHAPE,
              "gimp-shape", GTK_ICON_SIZE_MENU, G_CALLBACK (native_radio_toggled),
              &callback, &first, orientation);
            break;
          }
        g_object_ref_sink (control);
        orientation_assert (control, orientation, kind == 3 ? 0 : 1);
        GtkWidget *box = orientation_box (control);
        GList *children = gtk_container_get_children (GTK_CONTAINER (box));
        g_assert_true (children->data == first);
        g_assert_cmpuint (g_list_length (children), ==, kind == 2 ? 2 : 3);
        g_list_free (children);
        if (label)
          g_assert_true (gtk_frame_get_label_widget (GTK_FRAME (control)) == label);
        if (kind == 2)
          g_assert_null (native_radio_for_value (first, GIMP_BRUSH_GENERATED_CIRCLE));
        if (kind == 3)
          {
            GtkWidget *image = gtk_bin_get_child (GTK_BIN (first));
            const gchar *name = NULL;
            GtkIconSize size;
            gtk_image_get_icon_name (GTK_IMAGE (image), &name, &size);
            g_assert_cmpstr (name, ==, "gimp-shape-circle");
            g_assert_cmpint (size, ==, GTK_ICON_SIZE_MENU);
            g_assert_false (gtk_toggle_button_get_mode (GTK_TOGGLE_BUTTON (first)));
          }
        callback.toggles = 0;
        gimp_int_radio_group_set_active (GTK_RADIO_BUTTON (first), GIMP_BRUSH_GENERATED_DIAMOND);
        native_assert_radio_value (first, GIMP_BRUSH_GENERATED_DIAMOND, kind == 2 ? 2 : 3);
        g_assert_cmpuint (callback.toggles, ==, 1);
        g_assert_cmpint (callback.value, ==, GIMP_BRUSH_GENERATED_DIAMOND);
        gtk_widget_hide (control); gtk_widget_show_all (control);
        orientation_assert (control, orientation, kind == 3 ? 0 : 1);
        g_object_ref (first);
        g_object_set_data_full (G_OBJECT (first), "08017-finalized", &first_gone, widget_store_finalized);
        gtk_widget_destroy (control); g_object_unref (control);
        g_assert_false (first_gone);
        if (label) g_assert_true (label_gone);
        g_object_unref (first);
        g_assert_true (first_gone);
        /* No hidden ownership of the original callback-data ABI. */
        g_assert_cmpuint (callback.released, ==, 0);
      }
  for (guint direction = 0; direction < 2; ++direction)
    {
      GtkWidget *first = (GtkWidget *) 1;
      GtkWidget *empty = gimp_enum_radio_frame_new_full (GIMP_TYPE_BRUSH_GENERATED_SHAPE,
        100, 101, NULL, NULL, NULL, &first, direction ? GTK_ORIENTATION_VERTICAL : GTK_ORIENTATION_HORIZONTAL);
      g_object_ref_sink (empty);
      g_assert_null (first);
      g_assert_null (gtk_container_get_children (GTK_CONTAINER (orientation_box (empty))));
      gtk_widget_destroy (empty); g_object_unref (empty);
    }
}

static void
orientation_integer_contract (void)
{
  gimp_set_focused_once (gimp);
  for (guint direction = 0; direction < 3; ++direction)
    for (guint framed = 0; framed < 2; ++framed)
      {
        GtkOrientation requested = direction == 2 ? (GtkOrientation) 42 : (GtkOrientation) direction;
        GtkOrientation expected = direction == 0 ? GTK_ORIENTATION_HORIZONTAL : GTK_ORIENTATION_VERTICAL;
        NativeRadioCallback callback = {0, 0, -1};
        GtkWidget *zero = NULL, *seven = NULL, *negative = NULL;
        GtkWidget *group = gimp_int_radio_group_new_with_orientation (framed, "Values",
          G_CALLBACK (native_radio_toggled), &callback, requested, 7,
          "Zero", 0, &zero, "Seven", 7, &seven, GINT_TO_POINTER (1), -4, &negative, NULL);
        g_object_ref_sink (group);
        orientation_assert (group, expected, 2);
        native_assert_radio_value (zero, 7, 3);
        g_assert_null (gtk_button_get_label (GTK_BUTTON (negative)));
        g_assert_null (g_object_get_data (G_OBJECT (zero), "gimp-item-data"));
        g_assert_cmpint (GPOINTER_TO_INT (g_object_get_data (G_OBJECT (seven), "user_data")), ==, 7);
        g_assert_cmpint (GPOINTER_TO_INT (g_object_get_data (G_OBJECT (negative), "user_data")), ==, -4);
        callback.toggles = 0;
        gimp_int_radio_group_set_active (GTK_RADIO_BUTTON (zero), -4);
        gimp_int_radio_group_set_active (GTK_RADIO_BUTTON (zero), 0);
        native_assert_radio_value (zero, 0, 3);
        g_assert_cmpuint (callback.toggles, ==, 2);
        g_assert_cmpint (callback.value, ==, 0);
        gtk_widget_destroy (group); g_object_unref (group);
        g_assert_cmpuint (callback.released, ==, 0);
      }
  GtkWidget *empty = gimp_int_radio_group_new_with_orientation (FALSE, NULL,
    G_CALLBACK (native_radio_toggled), NULL, GTK_ORIENTATION_HORIZONTAL, 0, NULL);
  g_object_ref_sink (empty);
  g_assert_null (gtk_container_get_children (GTK_CONTAINER (empty)));
  gtk_widget_destroy (empty); g_object_unref (empty);
}

static void
orientation_property_contract (void)
{
  gimp_set_focused_once (gimp);
  for (guint direction = 0; direction < 2; ++direction)
    for (guint kind = 0; kind < 5; ++kind)
      {
        GtkOrientation orientation = direction ? GTK_ORIENTATION_VERTICAL : GTK_ORIENTATION_HORIZONTAL;
        NativeControlFixture *fixture = g_object_new (native_control_fixture_get_type (), NULL);
        GtkWidget *control;
        if (kind < 2)
          control = gimp_prop_enum_radio_frame_new_with_orientation (G_OBJECT (fixture), "fixture-choice",
            NULL, kind ? GIMP_BRUSH_GENERATED_SQUARE : 0,
            kind ? GIMP_BRUSH_GENERATED_DIAMOND : 0, orientation);
        else if (kind == 2)
          control = gimp_prop_boolean_radio_frame_new_with_orientation (G_OBJECT (fixture), "fixture-enabled",
            "Boolean", "Enabled", "Disabled", orientation);
        else
          control = gimp_prop_enum_stock_box_new_with_orientation (G_OBJECT (fixture), "fixture-choice",
            "gimp-shape", kind == 4 ? GIMP_BRUSH_GENERATED_SQUARE : 0,
            kind == 4 ? GIMP_BRUSH_GENERATED_DIAMOND : 0, orientation);
        g_object_ref_sink (control);
        orientation_assert (control, kind >= 3 ? GTK_ORIENTATION_HORIZONTAL : orientation,
                             kind >= 3 ? 0 : kind == 2 ? 2 : 1);
        GList *children = gtk_container_get_children (GTK_CONTAINER (orientation_box (control)));
        GtkWidget *first = children->data;
        g_list_free (children);
        const gchar *property_name = kind == 2 ? "fixture-enabled" : "fixture-choice";
        const gint initial = kind == 2 ? TRUE : GIMP_BRUSH_GENERATED_SQUARE;
        const gint changed = kind == 2 ? FALSE : GIMP_BRUSH_GENERATED_DIAMOND;
        const guint count = kind == 1 || kind == 2 || kind == 4 ? 2 : 3;
        native_assert_radio_value (first, initial, count);
        g_assert_true (g_object_get_data (G_OBJECT (control), "gimp-widget-property-config") == fixture);
        g_assert_cmpstr (g_object_get_data (G_OBJECT (control), "gimp-widget-property-name"), ==, property_name);
        guint notifications = 0;
        gchar *signal = g_strconcat ("notify::", property_name, NULL);
        g_signal_connect (fixture, signal, G_CALLBACK (native_count_notify), &notifications);
        g_free (signal);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (native_radio_for_value (first, changed)), TRUE);
        g_assert_cmpuint (notifications, ==, 1);
        g_assert_cmpint (kind == 2 ? fixture->enabled : fixture->choice, ==, changed);
        g_object_set (fixture, property_name, initial, NULL);
        g_assert_cmpuint (notifications, ==, 2);
        native_assert_radio_value (first, initial, count);
        gtk_widget_hide (control); gtk_widget_show_all (control);
        orientation_assert (control, kind >= 3 ? GTK_ORIENTATION_HORIZONTAL : orientation,
                             kind >= 3 ? 0 : kind == 2 ? 2 : 1);
        gboolean first_gone = FALSE, fixture_gone = FALSE;
        g_object_ref (first);
        g_object_set_data_full (G_OBJECT (first), "08017-finalized", &first_gone, widget_store_finalized);
        g_object_set_data_full (G_OBJECT (fixture), "08017-finalized", &fixture_gone, widget_store_finalized);
        gtk_widget_destroy (control); g_object_unref (control);
        g_assert_false (first_gone);
        /* Notify with the destroyed first child retained, then after release. */
        g_object_set (fixture, property_name, changed, NULL);
        g_object_unref (first);
        g_assert_true (first_gone);
        g_object_set (fixture, property_name, initial, NULL);
        g_assert_cmpuint (notifications, ==, 4);
        g_object_unref (fixture);
        g_assert_true (fixture_gone);
      }
}


/* The current histogram callers use native icon factories in filter dialogs.
 * Exercise the registered option types and actual production icon/view binding
 * contract. This is not a claim that old toolbar placement is unchanged. */
#include "widgets/gimphistogramview.h"
static void
orientation_histogram_current_controls (void)
{
  gimp_set_focused_once (gimp);
  static const gchar *callers[] = {
    "gimp-threshold-tool", "gimp-levels-tool", "gimp-curves-tool"
  };
  for (guint i = 0; i < G_N_ELEMENTS (callers); ++i)
    for (guint direction = 0; direction < 2; ++direction)
      {
        GimpToolInfo *tool = info (callers[i]);
        GObject *options = g_object_new (G_OBJECT_TYPE (tool->tool_options),
                                          "gimp", gimp, "tool-info", tool, NULL);
        GtkOrientation orientation = direction ? GTK_ORIENTATION_VERTICAL : GTK_ORIENTATION_HORIZONTAL;
        GtkWidget *icons = gimp_prop_enum_icon_box_new (options, "histogram-scale",
                                                         "gimp-histogram", 0, 0);
        GtkWidget *frame = gimp_prop_enum_radio_frame_new_with_orientation (options, "histogram-scale",
                                                                            "Histogram Scale", 0, 0, orientation);
        GtkWidget *view = gimp_histogram_view_new (TRUE);
        g_object_ref_sink (icons); g_object_ref_sink (frame); g_object_ref_sink (view);
        g_object_bind_property (options, "histogram-scale", view, "histogram-scale",
                                  G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
        orientation_assert (icons, GTK_ORIENTATION_HORIZONTAL, 0);
        orientation_assert (frame, orientation, 1);
        GList *children = gtk_container_get_children (GTK_CONTAINER (icons));
        GtkWidget *first = children->data;
        g_list_free (children);
        GtkWidget *radio = g_object_get_data (G_OBJECT (frame), "radio-button");
        native_assert_radio_value (first, GIMP_HISTOGRAM_SCALE_LINEAR, 2);
        native_assert_radio_value (radio, GIMP_HISTOGRAM_SCALE_LINEAR, 2);
        guint notifications = 0;
        g_signal_connect (options, "notify::histogram-scale", G_CALLBACK (native_count_notify), &notifications);
        gimp_int_radio_group_set_active (GTK_RADIO_BUTTON (first), GIMP_HISTOGRAM_SCALE_LOGARITHMIC);
        g_assert_cmpuint (notifications, ==, 1);
        g_assert_cmpint (GIMP_HISTOGRAM_VIEW (view)->scale, ==, GIMP_HISTOGRAM_SCALE_LOGARITHMIC);
        native_assert_radio_value (radio, GIMP_HISTOGRAM_SCALE_LOGARITHMIC, 2);
        g_object_set (view, "histogram-scale", GIMP_HISTOGRAM_SCALE_LINEAR, NULL);
        g_assert_cmpuint (notifications, ==, 2);
        native_assert_radio_value (first, GIMP_HISTOGRAM_SCALE_LINEAR, 2);
        native_assert_radio_value (radio, GIMP_HISTOGRAM_SCALE_LINEAR, 2);
        gboolean options_gone = FALSE, first_gone = FALSE, view_gone = FALSE;
        g_object_set_data_full (options, "08017-finalized", &options_gone, widget_store_finalized);
        g_object_set_data_full (G_OBJECT (first), "08017-finalized", &first_gone, widget_store_finalized);
        g_object_set_data_full (G_OBJECT (view), "08017-finalized", &view_gone, widget_store_finalized);
        g_object_ref (first);
        g_object_unref (options);
        g_assert_false (options_gone); /* Property widgets own their bound config. */
        gtk_widget_destroy (icons); g_object_unref (icons);
        g_assert_false (first_gone);
        g_object_set (options, "histogram-scale", GIMP_HISTOGRAM_SCALE_LOGARITHMIC, NULL);
        g_assert_cmpuint (notifications, ==, 3);
        g_assert_cmpint (GIMP_HISTOGRAM_VIEW (view)->scale, ==, GIMP_HISTOGRAM_SCALE_LOGARITHMIC);
        g_object_unref (first); g_assert_true (first_gone);
        gtk_widget_destroy (view); g_object_unref (view); g_assert_true (view_gone);
        g_object_set (options, "histogram-scale", GIMP_HISTOGRAM_SCALE_LINEAR, NULL);
        g_assert_cmpuint (notifications, ==, 4);
        native_assert_radio_value (radio, GIMP_HISTOGRAM_SCALE_LINEAR, 2);
        gtk_widget_destroy (frame); g_object_unref (frame);
        g_assert_true (options_gone);
      }
}
