/* The toolbar owns a fresh GUI; the dockable keeps its cached GUI and parent. */
#include "core/gimp-gui.h"
#include "widgets/gimpdialogfactory.h"
#include "widgets/gimpdockable.h"
#include "widgets/gimpoverlayframe.h"
#include "widgets/gimptooloptionseditor.h"
#include "widgets/gimpwindowstrategy.h"

/* These are deliberately unsupported by this legacy restoration slice.
 * afa43fae3e920210146abed514f136fd49f671b5 registered NULL horizontal
 * factories for the first two; the remaining six tools did not exist there.
 * They must not acquire a generic empty toolbar from the Perspective Guide
 * branch, nor be reported as missing legacy horizontal-factory ports. */
static const struct
{
  const gchar *tool;
  const gchar *reason;
} horizontal_toolbar_unsupported[] = {
  { "gimp-brightness-contrast-tool", "explicit NULL legacy factory" },
  { "gimp-gegl-tool",                "explicit NULL legacy factory" },
  { "gimp-unified-transform-tool",   "absent from pinned legacy tools" },
  { "gimp-handle-transform-tool",    "absent from pinned legacy tools" },
  { "gimp-transform-3d-tool",        "absent from pinned legacy tools" },
  { "gimp-warp-tool",                "absent from pinned legacy tools" },
  { "gimp-offset-tool",              "absent from pinned legacy tools" },
  { "gimp-operation-tool",           "absent from pinned legacy tools" }
};

static const gchar *
horizontal_toolbar_unsupported_reason (GimpToolInfo *tool)
{
  for (guint i = 0; i < G_N_ELEMENTS (horizontal_toolbar_unsupported); i++)
    if (! g_strcmp0 (gimp_object_get_name (tool),
                    horizontal_toolbar_unsupported[i].tool))
      return horizontal_toolbar_unsupported[i].reason;

  return NULL;
}

static void
perspective_toolbar_assert_empty (GtkWidget      *gui,
                                  GtkOrientation  orientation)
{
  GList *children;

  g_assert_true (GTK_IS_BOX (gui));
  g_assert_cmpint (gtk_orientable_get_orientation (GTK_ORIENTABLE (gui)),
                   ==, orientation);
  g_assert_cmpint (gtk_box_get_spacing (GTK_BOX (gui)), ==, 4);
  g_assert_false (gtk_box_get_homogeneous (GTK_BOX (gui)));
  children = gtk_container_get_children (GTK_CONTAINER (gui));
  g_assert_null (children);
  g_list_free (children);
}

typedef struct
{
  GimpToolInfo     *previous_tool;
  gboolean         previous_visible;
  GimpDisplayShell *shell;
  GtkWidget        *existing_dockable;
  GtkWidget        *dockable;
  GtkWidget        *dockable_parent;
  GtkWidget        *editor;
  GtkWidget        *native;
  GtkWidget        *native_parent;
} PerspectiveToolbarFixture;

static void
perspective_toolbar_assert_dock (PerspectiveToolbarFixture *fixture)
{
  GimpToolOptions *options = info ("gimp-perspective-guide-tool")->tool_options;

  g_assert_true (gimp_tools_get_tool_options_gui (options) == fixture->native);
  g_assert_true (gtk_widget_get_parent (fixture->native) == fixture->native_parent);
  g_assert_true (gtk_widget_get_parent (fixture->dockable) == fixture->dockable_parent);
  g_assert_true (gtk_widget_get_ancestor (fixture->native, GIMP_TYPE_TOOL_OPTIONS_EDITOR) ==
                 fixture->editor);
  g_assert_true (gtk_widget_is_ancestor (fixture->native, fixture->dockable));
  perspective_toolbar_assert_empty (fixture->native, GTK_ORIENTATION_VERTICAL);
}

static void
perspective_toolbar_fixture_begin (PerspectiveToolbarFixture *fixture)
{
  GimpContext       *context = gimp_get_user_context (gimp);
  GimpToolInfo      *tool = info ("gimp-perspective-guide-tool");
  GimpDialogFactory *factory = gimp_dialog_factory_get_singleton ();

  fixture->previous_tool = gimp_context_get_tool (context);
  fixture->previous_visible = gimp->config->painter_canvas_ui;
  g_object_set (gimp->config, "painter-canvas-ui", FALSE, NULL);
  fixture->shell = new_shell ();
  fixture->existing_dockable = gimp_dialog_factory_find_widget (factory, "gimp-tool-options");
  fixture->dockable = gimp_window_strategy_show_dockable_dialog
    (GIMP_WINDOW_STRATEGY (gimp_get_window_strategy (gimp)), gimp, factory,
     gimp_widget_get_monitor (GTK_WIDGET (fixture->shell)), "gimp-tool-options");
  g_assert_true (GIMP_IS_DOCKABLE (fixture->dockable));
  g_assert_nonnull (gimp_dockable_get_dockbook (GIMP_DOCKABLE (fixture->dockable)));
  fixture->editor = gtk_bin_get_child (GTK_BIN (fixture->dockable));
  g_assert_true (GIMP_IS_TOOL_OPTIONS_EDITOR (fixture->editor));
  drain ();
  /* Real shell/device initialization may restore the profile's tool. */
  gimp_context_set_tool (context, tool);
  drain ();
  g_assert_true (gimp_context_get_tool (context) == tool);
  g_assert_true (G_OBJECT_TYPE (tool->tool_options) == GIMP_TYPE_TOOL_OPTIONS);
  g_assert_true (gimp_tool_options_editor_get_tool_options
                  (GIMP_TOOL_OPTIONS_EDITOR (fixture->editor)) == tool->tool_options);
  fixture->native = gimp_tools_get_tool_options_gui (tool->tool_options);
  fixture->native_parent = gtk_widget_get_parent (fixture->native);
  fixture->dockable_parent = gtk_widget_get_parent (fixture->dockable);
  g_assert_nonnull (fixture->native_parent);
  g_assert_nonnull (fixture->dockable_parent);
  perspective_toolbar_assert_dock (fixture);
}

static void
perspective_toolbar_fixture_end (PerspectiveToolbarFixture *fixture)
{
  perspective_toolbar_assert_dock (fixture);
  if (! fixture->existing_dockable)
    gtk_widget_destroy (fixture->dockable);
  close_shell (fixture->shell);
  g_object_set (gimp->config, "painter-canvas-ui", fixture->previous_visible, NULL);
  gimp_context_set_tool (gimp_get_user_context (gimp), fixture->previous_tool);
  drain ();
}

static void
independent_perspective_guide_toolbar (void)
{
  PerspectiveToolbarFixture fixture = { 0 };
  GimpToolOptions *options = info ("gimp-perspective-guide-tool")->tool_options;

  perspective_toolbar_fixture_begin (&fixture);
  for (guint reverse = 0; reverse < 2; reverse++)
    {
      GtkWidget *views[2];
      gpointer weak[2];

      for (guint i = 0; i < 2; i++)
        {
          views[i] = gimp_tools_create_painter_toolbar_gui (options);
          g_assert_nonnull (views[i]);
          g_assert_true (g_object_is_floating (views[i]));
          g_object_ref_sink (views[i]);
          weak[i] = views[i];
          g_object_add_weak_pointer (G_OBJECT (views[i]), &weak[i]);
          g_assert_true (views[i] != fixture.native);
          g_assert_null (gtk_widget_get_parent (views[i]));
          perspective_toolbar_assert_empty (views[i], GTK_ORIENTATION_HORIZONTAL);
        }
      g_assert_true (views[0] != views[1]);
      perspective_toolbar_assert_dock (&fixture);
      for (guint i = 0; i < 2; i++)
        {
          guint index = reverse ? 1 - i : i;

          gtk_widget_destroy (views[index]);
          g_object_unref (views[index]);
          drain ();
          g_assert_null (weak[index]);
          if (i == 0)
            {
              g_assert_nonnull (weak[1 - index]);
              perspective_toolbar_assert_empty (views[1 - index], GTK_ORIENTATION_HORIZONTAL);
              g_assert_null (gtk_widget_get_parent (views[1 - index]));
            }
          perspective_toolbar_assert_dock (&fixture);
        }
    }
  perspective_toolbar_fixture_end (&fixture);
}

static void
perspective_toolbar_assert_child (GtkWidget *bar,
                                  GtkWidget *expected)
{
  GList *children = gtk_container_get_children (GTK_CONTAINER (bar));

  g_assert_cmpuint (g_list_length (children), ==, expected ? 1 : 0);
  if (expected)
    g_assert_true (children->data == expected);
  g_list_free (children);
}

static void
perspective_guide_toolbar_canvas_switching (void)
{
  PerspectiveToolbarFixture fixture = { 0 };
  GimpContext *context = gimp_get_user_context (gimp);
  GtkWidget *guide;
  GtkWidget *bar;
  GtkWidget *panel;
  gpointer weak_guide;

  perspective_toolbar_fixture_begin (&fixture);
  gimp_painter_canvas_ui_set_visible (fixture.shell, TRUE);
  drain ();
  guide = find (fixture.shell->canvas, "gimp-painter-perspective-guide-toolbar", GTK_TYPE_BOX);
  g_assert_nonnull (guide);
  bar = gtk_widget_get_parent (guide);
  g_assert_true (GTK_IS_BOX (bar));
  panel = gtk_widget_get_ancestor (bar, GIMP_TYPE_OVERLAY_FRAME);
  g_assert_nonnull (panel);
  for (guint i = 0; i < G_N_ELEMENTS (horizontal_toolbar_unsupported); i++)
    {
      GtkWidget *populated;
      gpointer weak_populated;

      perspective_toolbar_assert_empty (guide, GTK_ORIENTATION_HORIZONTAL);
      g_assert_true (guide != fixture.native);
      perspective_toolbar_assert_child (bar, guide);
      g_assert_true (gtk_widget_get_visible (panel));
      perspective_toolbar_assert_dock (&fixture);
      weak_guide = guide;
      g_object_add_weak_pointer (G_OBJECT (guide), &weak_guide);
      gimp_context_set_tool (context, info ("gimp-move-tool"));
      drain ();
      g_assert_null (weak_guide);
      g_assert_null (find (fixture.shell->canvas, "gimp-painter-perspective-guide-toolbar", GTK_TYPE_BOX));
      populated = find (fixture.shell->canvas, "gimp-painter-tool-toolbar", GTK_TYPE_BOX);
      g_assert_nonnull (populated);
      g_assert_nonnull (find (populated, "move-type", 0));
      perspective_toolbar_assert_child (bar, populated);
      g_assert_true (gtk_widget_get_visible (panel));
      perspective_toolbar_assert_dock (&fixture);
      weak_populated = populated;
      g_object_add_weak_pointer (G_OBJECT (populated), &weak_populated);
      gimp_context_set_tool (context, info (horizontal_toolbar_unsupported[i].tool));
      drain ();
      g_assert_null (weak_populated);
      perspective_toolbar_assert_child (bar, NULL);
      g_assert_false (gtk_widget_get_visible (panel));
      g_assert_null (find (fixture.shell->canvas, "gimp-painter-tool-toolbar", GTK_TYPE_BOX));
      g_assert_null (find (fixture.shell->canvas, "gimp-painter-perspective-guide-toolbar", GTK_TYPE_BOX));
      perspective_toolbar_assert_dock (&fixture);
      gimp_context_set_tool (context, info ("gimp-perspective-guide-tool"));
      drain ();
      guide = find (fixture.shell->canvas, "gimp-painter-perspective-guide-toolbar", GTK_TYPE_BOX);
      g_assert_nonnull (guide);
      g_assert_true (gtk_widget_get_parent (guide) == bar);
    }
  perspective_toolbar_assert_empty (guide, GTK_ORIENTATION_HORIZONTAL);
  perspective_toolbar_assert_child (bar, guide);
  g_assert_true (gtk_widget_get_visible (panel));
  weak_guide = guide;
  g_object_add_weak_pointer (G_OBJECT (guide), &weak_guide);
  perspective_toolbar_fixture_end (&fixture);
  g_assert_null (weak_guide);
}

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
  guint unsupported = 0;

  for (GList *iter = GIMP_LIST (gimp->tool_info_list)->queue->head;
       iter; iter = iter->next)
    {
      GimpToolInfo *tool = iter->data;
      GtkWidget *native = gimp_tools_get_tool_options_gui (tool->tool_options);
      GPtrArray *widgets = g_ptr_array_new ();
      GPtrArray *parents = g_ptr_array_new ();
      GtkWidget *first;
      GtkWidget *second;
      const gchar *reason = horizontal_toolbar_unsupported_reason (tool);

      gather (native, widgets);
      for (guint i = 0; i < widgets->len; i++)
        g_ptr_array_add (parents, gtk_widget_get_parent (g_ptr_array_index (widgets, i)));
      first = gimp_tools_create_painter_toolbar_gui (tool->tool_options);
      if (reason)
        {
          g_assert_null (first);
          g_assert_null (gimp_tools_create_painter_toolbar_gui (tool->tool_options));
          g_assert_true (gimp_tools_get_tool_options_gui (tool->tool_options) == native);
          for (guint i = 0; i < widgets->len; i++)
            g_assert_true (gtk_widget_get_parent (g_ptr_array_index (widgets, i)) ==
                           g_ptr_array_index (parents, i));
          g_test_message ("Deliberately unsupported horizontal toolbar: %s (%s)",
                          gimp_object_get_name (tool), reason);
          unsupported++;
          g_ptr_array_unref (widgets);
          g_ptr_array_unref (parents);
          continue;
        }
      g_test_message ("Expected horizontal factory: %s", gimp_object_get_name (tool));
      g_assert_nonnull (first);
      supported++;
      g_assert_true (g_object_is_floating (first));
      g_object_ref_sink (first);
      second = gimp_tools_create_painter_toolbar_gui (tool->tool_options);
      g_assert_nonnull (second);
      g_assert_true (g_object_is_floating (second));
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
  g_assert_cmpuint (unsupported, ==, G_N_ELEMENTS (horizontal_toolbar_unsupported));
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
