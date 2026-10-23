/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Only existing native APIs: can be staged separately for a genuine baseline
 * behavioral red check before the missing legacy symbols are restored. */
static void
orientation_active_callers (void)
{
  static const struct { const gchar *tool, *property; } callers[] = {
    {"gimp-cage-tool", "cage-mode"},
    {"gimp-color-picker-tool", "pick-target"},
    {"gimp-flip-tool", "flip-type"},
    {"gimp-zoom-tool", "zoom-type"},
    {"gimp-move-tool", "move-current"},
    {"gimp-perspective-clone-tool", "clone-mode"},
    {"gimp-rotate-tool", "direction"},
    {"gimp-scale-tool", "direction"},
    {"gimp-shear-tool", "direction"},
    {"gimp-perspective-tool", "direction"},
    {"gimp-unified-transform-tool", "direction"},
    {"gimp-handle-transform-tool", "direction"},
    {"gimp-transform-3d-tool", "direction"},
    {"gimp-vector-tool", "vectors-edit-mode"},
  };
  gimp_set_focused_once (gimp);
  for (guint i = 0; i < G_N_ELEMENTS (callers); ++i)
    {
      GimpToolInfo *tool = info (callers[i].tool);
      GtkWidget *gui = gimp_tools_get_tool_options_gui (tool->tool_options);
      compact (gui, tool->tool_options, FALSE, GTK_ORIENTATION_HORIZONTAL);
      GtkWidget *control = find (gui, callers[i].property, 0);
      g_assert_nonnull (control);
      GtkWidget *first = g_object_get_data (G_OBJECT (control), "radio-button");
      g_assert_true (GTK_IS_RADIO_BUTTON (first));
      GtkWidget *box = gtk_widget_get_parent (first);
      g_assert_true (GTK_IS_BOX (box));
      GtkWidget *parent = gtk_widget_get_parent (box);
      GtkOrientation original = gtk_orientable_get_orientation (GTK_ORIENTABLE (box));
      gint saved_value = 0;
      g_object_get (tool->tool_options, callers[i].property, &saved_value, NULL);
      GtkWidget *alternate = NULL;
      for (GSList *item = gtk_radio_button_get_group (GTK_RADIO_BUTTON (first)); item; item = item->next)
        if (GPOINTER_TO_INT (g_object_get_data (G_OBJECT (item->data), "gimp-item-data")) != saved_value)
          { alternate = item->data; break; }
      g_assert_nonnull (alternate);
      guint notifications = 0;
      gchar *signal = g_strconcat ("notify::", callers[i].property, NULL);
      gulong handler = g_signal_connect (tool->tool_options, signal,
                                          G_CALLBACK (native_count_notify), &notifications);
      g_free (signal);
      for (guint pass = 0; pass < 4; ++pass)
        {
          GtkOrientation layout = pass % 2 ? GTK_ORIENTATION_VERTICAL : GTK_ORIENTATION_HORIZONTAL;
          compact (gui, tool->tool_options, TRUE, layout);
          g_assert_cmpint (gtk_orientable_get_orientation (GTK_ORIENTABLE (box)), ==, layout);
          g_assert_true (gtk_widget_get_parent (box) == parent);
          g_assert_true (find (gui, callers[i].property, 0) == control);
          guint before = notifications;
          gint selected = 0;
          gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (alternate), TRUE);
          g_object_get (tool->tool_options, callers[i].property, &selected, NULL);
          g_assert_cmpint (selected, ==, GPOINTER_TO_INT (g_object_get_data (G_OBJECT (alternate), "gimp-item-data")));
          g_assert_cmpuint (notifications, ==, before + 1);
          g_object_set (tool->tool_options, callers[i].property, saved_value, NULL);
          g_assert_true (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (native_radio_for_value (first, saved_value))));
          g_assert_cmpuint (notifications, ==, before + 2);
          gtk_widget_hide (gui); gtk_widget_show_all (gui);
          compact (gui, tool->tool_options, FALSE, GTK_ORIENTATION_HORIZONTAL);
          g_assert_cmpint (gtk_orientable_get_orientation (GTK_ORIENTABLE (box)), ==, original);
          g_assert_true (gtk_widget_get_parent (box) == parent);
        }
      g_signal_handler_disconnect (tool->tool_options, handler);
      g_test_message ("Original orientation caller preserved: %s / %s", callers[i].tool, callers[i].property);
    }
}
