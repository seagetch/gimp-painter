/* The toolbar owns a fresh GUI; the dockable keeps its cached GUI and parent. */
static void
independent_mypaint_toolbar (void)
{
  GimpToolInfo *tool = info ("gimp-painter-mypaint-tool");
  GimpToolOptions *options = tool->tool_options;
  GtkWidget *native = gimp_tools_get_tool_options_gui (options);
  GtkWidget *parent = gtk_widget_get_parent (native);
  GtkWidget *first = gimp_tools_create_painter_toolbar_gui (options);
  GtkWidget *second = gimp_tools_create_painter_toolbar_gui (options);
  GtkWidget *native_radius;
  GtkWidget *first_radius;
  GtkWidget *second_radius;
  gdouble original;
  gdouble actual;

  g_assert_nonnull (first);
  g_assert_nonnull (second);
  g_assert_true (g_object_is_floating (first));
  g_assert_true (g_object_is_floating (second));
  g_object_ref_sink (first);
  g_object_ref_sink (second);
  g_assert_true (first != second && first != native && second != native);
  g_assert_null (gtk_widget_get_parent (first));
  g_assert_null (gtk_widget_get_parent (second));

  compact (first, options, TRUE, GTK_ORIENTATION_HORIZONTAL);
  g_assert_true (gimp_tools_get_tool_options_gui (options) == native);
  g_assert_true (gtk_widget_get_parent (native) == parent);
  native_radius = find (native, "radius-logarithmic", GTK_TYPE_SPIN_BUTTON);
  first_radius = find (first, "radius-logarithmic", GTK_TYPE_SPIN_BUTTON);
  second_radius = find (second, "radius-logarithmic", GTK_TYPE_SPIN_BUTTON);
  g_assert_nonnull (native_radius);
  g_assert_nonnull (first_radius);
  g_assert_nonnull (second_radius);
  g_object_get (options, "radius-logarithmic", &original, NULL);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (first_radius), 1.25);
  drain ();
  g_object_get (options, "radius-logarithmic", &actual, NULL);
  g_assert_cmpfloat_with_epsilon (actual, 1.25, 1e-6);
  g_assert_cmpfloat_with_epsilon (gtk_spin_button_get_value (GTK_SPIN_BUTTON (native_radius)), 1.25, 1e-6);
  g_assert_cmpfloat_with_epsilon (gtk_spin_button_get_value (GTK_SPIN_BUTTON (second_radius)), 1.25, 1e-6);

  compact (first, options, FALSE, GTK_ORIENTATION_HORIZONTAL);
  gtk_widget_destroy (first);
  g_object_unref (first);
  g_object_set (options, "radius-logarithmic", 1.5, NULL);
  drain ();
  g_assert_cmpfloat_with_epsilon (gtk_spin_button_get_value (GTK_SPIN_BUTTON (second_radius)), 1.5, 1e-6);
  gtk_widget_destroy (second);
  g_object_unref (second);
  g_object_set (options, "radius-logarithmic", original, NULL);
  drain ();
  g_assert_true (gimp_tools_get_tool_options_gui (options) == native);
  g_assert_true (gtk_widget_get_parent (native) == parent);
}

/* A second toolbar must neither steal widgets nor overwrite the native GUI.
 * Test every implemented family, including creation/destruction twice. */
static void
independent_horizontal_toolbar_inventory (void)
{
  guint supported = 0;

  for (GList *iter = GIMP_LIST (gimp->tool_info_list)->queue->head;
       iter; iter = iter->next)
    {
      GimpToolInfo *tool = iter->data;
      GtkWidget *native = gimp_tools_get_tool_options_gui (tool->tool_options);
      GPtrArray *widgets = g_ptr_array_new ();
      GPtrArray *parents = g_ptr_array_new ();
      GtkWidget *first;
      GtkWidget *second;

      gather (native, widgets);
      for (guint i = 0; i < widgets->len; i++)
        g_ptr_array_add (parents, gtk_widget_get_parent (g_ptr_array_index (widgets, i)));
      first = gimp_tools_create_painter_toolbar_gui (tool->tool_options);
      if (! first)
        {
          g_test_message ("Horizontal factory not yet ported: %s", gimp_object_get_name (tool));
          g_ptr_array_unref (widgets);
          g_ptr_array_unref (parents);
          continue;
        }
      supported++;
      g_assert_true (g_object_is_floating (first));
      g_object_ref_sink (first);
      second = gimp_tools_create_painter_toolbar_gui (tool->tool_options);
      g_assert_nonnull (second);
      g_object_ref_sink (second);
      g_assert_true (first != second && first != native && second != native);
      g_assert_true (GTK_IS_BOX (first));
      g_assert_cmpint (gtk_orientable_get_orientation (GTK_ORIENTABLE (first)),
                       ==, GTK_ORIENTATION_HORIZONTAL);
      g_assert_null (gtk_widget_get_parent (first));
      g_assert_true (gimp_tools_get_tool_options_gui (tool->tool_options) == native);
      for (guint i = 0; i < widgets->len; i++)
        g_assert_true (gtk_widget_get_parent (g_ptr_array_index (widgets, i)) ==
                       g_ptr_array_index (parents, i));
      gtk_widget_destroy (first);
      g_object_unref (first);
      gtk_widget_destroy (second);
      g_object_unref (second);
      drain ();
      for (guint i = 0; i < widgets->len; i++)
        g_assert_true (gtk_widget_get_parent (g_ptr_array_index (widgets, i)) ==
                       g_ptr_array_index (parents, i));
      g_test_message ("Independent horizontal factory: %s", gimp_object_get_name (tool));
      g_ptr_array_unref (widgets);
      g_ptr_array_unref (parents);
    }
  g_test_message ("Independent source-based toolbar families: %u", supported);
  g_assert_cmpuint (supported, >=, 16);
}

static void
independent_horizontal_toolbar_numeric_binding (void)
{
  const struct { const gchar *tool; const gchar *property; gdouble value; } cases[] = {
    { "gimp-paintbrush-tool", "brush-size", 37.0 },
    { "gimp-eraser-tool", "opacity", 63.0 },
    { "gimp-airbrush-tool", "rate", 48.0 },
    { "gimp-fuzzy-select-tool", "threshold", 21.0 },
    { "gimp-rect-select-tool", "feather-radius", 7.0 },
    { "gimp-gradient-tool", "offset", 19.0 },
    { "gimp-rotate-tool", "preview-opacity", 73.0 }
  };

  for (guint i = 0; i < G_N_ELEMENTS (cases); i++)
    {
      GimpToolOptions *options = info (cases[i].tool)->tool_options;
      GtkWidget *native = gimp_tools_get_tool_options_gui (options);
      GtkWidget *first = gimp_tools_create_painter_toolbar_gui (options);
      GtkWidget *second = gimp_tools_create_painter_toolbar_gui (options);
      GtkWidget *control;
      GtkWidget *peer;
      GtkWidget *dock_control;
      gdouble original;
      gdouble actual;

      g_assert_nonnull (first);
      g_assert_nonnull (second);
      g_object_ref_sink (first);
      g_object_ref_sink (second);
      control = find (first, cases[i].property, GTK_TYPE_SPIN_BUTTON);
      peer = find (second, cases[i].property, GTK_TYPE_SPIN_BUTTON);
      dock_control = find (native, cases[i].property, GTK_TYPE_SPIN_BUTTON);
      g_assert_nonnull (control);
      g_assert_nonnull (peer);
      g_assert_nonnull (dock_control);
      g_object_get (options, cases[i].property, &original, NULL);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (control), cases[i].value);
      drain ();
      g_object_get (options, cases[i].property, &actual, NULL);
      /* Opacity widgets display percent, as in the legacy toolbar. */
      if (! g_strcmp0 (cases[i].property, "opacity") ||
          ! g_strcmp0 (cases[i].property, "preview-opacity"))
        g_assert_cmpfloat_with_epsilon (actual, cases[i].value / 100.0, 1e-6);
      else
        g_assert_cmpfloat_with_epsilon (actual, cases[i].value, 1e-6);
      g_assert_cmpfloat_with_epsilon (gtk_spin_button_get_value (GTK_SPIN_BUTTON (peer)),
                                     cases[i].value, 1e-6);
      g_assert_cmpfloat_with_epsilon (gtk_spin_button_get_value (GTK_SPIN_BUTTON (dock_control)),
                                     cases[i].value, 1e-6);
      gtk_widget_destroy (first);
      g_object_unref (first);
      g_object_set (options, cases[i].property, original, NULL);
      drain ();
      g_assert_cmpfloat_with_epsilon (gtk_spin_button_get_value (GTK_SPIN_BUTTON (peer)),
                                     gtk_spin_button_get_value (GTK_SPIN_BUTTON (dock_control)), 1e-6);
      gtk_widget_destroy (second);
      g_object_unref (second);
    }
}

static void
independent_horizontal_toolbar_native_handles (void)
{
  GimpMoveOptions *move = GIMP_MOVE_OPTIONS (info ("gimp-move-tool")->tool_options);
  GimpSelectionOptions *selection = GIMP_SELECTION_OPTIONS (info ("gimp-rect-select-tool")->tool_options);
  GimpGradientOptions *gradient = GIMP_GRADIENT_OPTIONS (info ("gimp-gradient-tool")->tool_options);
  GimpTransformGridOptions *transform = GIMP_TRANSFORM_GRID_OPTIONS (info ("gimp-rotate-tool")->tool_options);
  GimpToolOptions *options[] = { GIMP_TOOL_OPTIONS (move), GIMP_TOOL_OPTIONS (selection),
                               GIMP_TOOL_OPTIONS (gradient), GIMP_TOOL_OPTIONS (transform) };
  GtkWidget *before[9];

  for (guint i = 0; i < G_N_ELEMENTS (options); i++)
    gimp_tools_get_tool_options_gui (options[i]);
  before[0] = move->type_box;
  before[1] = selection->mode_box;
  before[2] = selection->antialias_toggle;
  before[3] = gradient->instant_toggle;
  before[4] = gradient->modify_active_frame;
  before[5] = gradient->modify_active_hint;
  before[6] = GIMP_TRANSFORM_OPTIONS (transform)->type_box;
  before[7] = GIMP_TRANSFORM_OPTIONS (transform)->direction_frame;
  before[8] = transform->direction_chain_button;
  for (guint i = 0; i < G_N_ELEMENTS (options); i++)
    {
      GtkWidget *gui = gimp_tools_create_painter_toolbar_gui (options[i]);
      g_assert_nonnull (gui);
      g_object_ref_sink (gui);
      gtk_widget_destroy (gui);
      g_object_unref (gui);
    }
  g_assert_true (before[0] == move->type_box);
  g_assert_true (before[1] == selection->mode_box);
  g_assert_true (before[2] == selection->antialias_toggle);
  g_assert_true (before[3] == gradient->instant_toggle);
  g_assert_true (before[4] == gradient->modify_active_frame);
  g_assert_true (before[5] == gradient->modify_active_hint);
  g_assert_true (before[6] == GIMP_TRANSFORM_OPTIONS (transform)->type_box);
  g_assert_true (before[7] == GIMP_TRANSFORM_OPTIONS (transform)->direction_frame);
  g_assert_true (before[8] == transform->direction_chain_button);
}
