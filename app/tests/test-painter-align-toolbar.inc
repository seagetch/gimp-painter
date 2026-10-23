/* Independent Align views share the native model and native tool actions. */
#include "core/gimpimage.h"
#include "core/gimpimage-guides.h"
#include "core/gimpimage-undo.h"
#include "core/gimpguide.h"
#include "core/gimplayer.h"
#include "core/gimplayer-new.h"
#include "tools/gimpalignoptions.h"
#include "widgets/gimppivotselector.h"
#include "vectors/gimppath.h"

static GtkWidget *
align_action (GtkWidget *root, GimpAlignmentType action)
{
  GPtrArray *widgets = g_ptr_array_new ();
  GtkWidget *result = NULL;

  gather (root, widgets);
  for (guint i = 0; i < widgets->len; i++)
    {
      GtkWidget *widget = g_ptr_array_index (widgets, i);
      gpointer value;
      if (g_object_get_data (G_OBJECT (widget), "align-action") &&
          GPOINTER_TO_INT (g_object_get_data (G_OBJECT (widget), "align-action")) == action)
        result = widget;
      /* ALIGN_LEFT is zero, so its stored data is NULL. Identify that
       * existing native action by its icon rather than translated text. */
      value = g_object_get_data (G_OBJECT (widget), "align-action");
      if (!value && action == GIMP_ALIGN_LEFT && GTK_IS_BUTTON (widget) &&
          GTK_IS_IMAGE (gtk_bin_get_child (GTK_BIN (widget))))
        {
          const gchar *name = NULL;
          gtk_image_get_icon_name (GTK_IMAGE (gtk_bin_get_child (GTK_BIN (widget))),
                                    &name, NULL);
          if (!g_strcmp0 (name, GIMP_ICON_GRAVITY_WEST)) result = widget;
        }
    }
  g_ptr_array_unref (widgets);
  g_assert_nonnull (result);
  return result;
}

static GtkWidget *
align_pivot (GtkWidget *root)
{
  GPtrArray *widgets = g_ptr_array_new ();
  GtkWidget *result = NULL;
  gather (root, widgets);
  for (guint i = 0; i < widgets->len; i++)
    if (GIMP_IS_PIVOT_SELECTOR (g_ptr_array_index (widgets, i)))
      result = g_ptr_array_index (widgets, i);
  g_ptr_array_unref (widgets);
  g_assert_nonnull (result);
  return result;
}

static void
align_assert_pivot (GtkWidget *root, gdouble x, gdouble y)
{
  gdouble actual_x, actual_y;
  gimp_pivot_selector_get_position (GIMP_PIVOT_SELECTOR (align_pivot (root)),
                                    &actual_x, &actual_y);
  g_assert_cmpfloat (actual_x, ==, x);
  g_assert_cmpfloat (actual_y, ==, y);
}

static void
independent_align_toolbar (void)
{
  GimpToolInfo *tool = info ("gimp-align-tool");
  GimpAlignOptions *options = GIMP_ALIGN_OPTIONS (tool->tool_options);
  GimpContext *context = gimp_get_user_context (gimp);
  GimpToolInfo *old_tool = gimp_context_get_tool (context);
  GimpDisplayShell *shell = new_shell ();
  GimpImage *image = gimp_display_get_image (shell->display);
  GtkWidget *native = gimp_tools_get_tool_options_gui (tool->tool_options);
  GtkWidget *parent = gtk_widget_get_parent (native);
  GtkWidget *first = gimp_tools_create_painter_toolbar_gui (tool->tool_options);
  GtkWidget *second = gimp_tools_create_painter_toolbar_gui (tool->tool_options);
  GimpLayer *layers[3];
  GList *selected = NULL;
  GimpGuide *guide;
  gint offsets[] = { 20, 80, 180 };

  g_object_ref_sink (first);
  g_object_ref_sink (second);
  g_assert_true (first != second && first != native);
  g_assert_cmpint (gtk_orientable_get_orientation (GTK_ORIENTABLE (first)),
                   ==, GTK_ORIENTATION_HORIZONTAL);
  gimp_context_set_tool (context, tool);
  drain ();
  parent = gtk_widget_get_parent (native);
  g_object_set (options, "align-reference", GIMP_ALIGN_REFERENCE_IMAGE,
                "align-layers", TRUE, "align-vectors", FALSE,
                "align-contents", FALSE, "pivot-x", 0.0, "pivot-y", 0.0, NULL);
  for (guint i = 0; i < 3; i++)
    {
      layers[i] = gimp_layer_new (image, 20 + 20 * i, 20,
                                  gimp_image_get_layer_format (image, TRUE),
                                  "Alignment target", 1.0, GIMP_LAYER_MODE_NORMAL);
      gimp_image_add_layer (image, layers[i], NULL, 0, FALSE);
      gimp_item_set_offset (GIMP_ITEM (layers[i]), offsets[i], 30 + i * 40);
      selected = g_list_append (selected, layers[i]);
    }
  gimp_image_set_selected_layers (image, selected);
  g_list_free (selected);
  drain ();
  g_assert_true (gtk_widget_get_sensitive (align_action (first, GIMP_ALIGN_LEFT)));
  g_assert_true (gtk_widget_get_sensitive (align_action (second, GIMP_ARRANGE_HFILL)));
  gtk_button_clicked (GTK_BUTTON (align_action (first, GIMP_ALIGN_LEFT)));
  for (guint i = 0; i < 3; i++)
    g_assert_cmpint (gimp_item_get_offset_x (GIMP_ITEM (layers[i])), ==, 0);
  g_assert_true (gimp_image_undo (image));
  for (guint i = 0; i < 3; i++)
    g_assert_cmpint (gimp_item_get_offset_x (GIMP_ITEM (layers[i])), ==, offsets[i]);
  gtk_button_clicked (GTK_BUTTON (align_action (second, GIMP_ARRANGE_HFILL)));
  g_assert_cmpint (gimp_item_get_offset_x (GIMP_ITEM (layers[1])), ==, 100);
  g_assert_true (gimp_image_undo (image));
  g_assert_cmpint (gimp_item_get_offset_x (GIMP_ITEM (layers[1])), ==, 80);
  /* Unequal widths distinguish even anchor spacing from even gaps. */
  gtk_button_clicked (GTK_BUTTON (align_action (first, GIMP_DISTRIBUTE_EVEN_HORIZONTAL_GAP)));
  g_assert_cmpint (gimp_item_get_offset_x (GIMP_ITEM (layers[1])), ==, 90);
  g_assert_true (gimp_image_undo (image));
  g_assert_cmpint (gimp_item_get_offset_x (GIMP_ITEM (layers[1])), ==, 80);

  gimp_pivot_selector_set_position (GIMP_PIVOT_SELECTOR (align_pivot (first)), 1.0, 0.5);
  align_assert_pivot (second, 1.0, 0.5);
  align_assert_pivot (native, 1.0, 0.5);
  g_object_set (options, "pivot-x", 0.5, "pivot-y", 1.0, NULL);
  align_assert_pivot (first, 0.5, 1.0);
  align_assert_pivot (native, 0.5, 1.0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (find (first, "align-layers", GTK_TYPE_CHECK_BUTTON)), FALSE);
  g_assert_false (gtk_widget_get_sensitive (align_action (second, GIMP_ALIGN_LEFT)));
  g_assert_false (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (find (native, "align-layers", GTK_TYPE_CHECK_BUTTON))));

  guide = gimp_image_add_vguide (image, 80, FALSE);
  gimp_align_options_pick_guide (options, guide, FALSE);
  g_object_set (options, "align-reference", GIMP_ALIGN_REFERENCE_PICK, NULL);
  g_assert_false (gtk_widget_get_sensitive (align_action (first, GIMP_ALIGN_LEFT)));
  gimp_align_options_pick_reference (options, G_OBJECT (guide));
  g_assert_true (gtk_widget_get_sensitive (align_action (first, GIMP_ALIGN_LEFT)));
  g_assert_false (gtk_widget_get_sensitive (align_action (first, GIMP_ALIGN_TOP)));
  g_assert_false (gtk_widget_get_sensitive (align_action (first, GIMP_ARRANGE_HFILL)));
  gimp_image_remove_guide (image, guide, FALSE);
  g_assert_false (gtk_widget_get_sensitive (align_action (native, GIMP_ALIGN_LEFT)));

  /* Selected path notifications must reach every view, even without a layer
   * selection/property change. */
  {
    GimpPath *path = gimp_path_new (image, "Selected alignment path");
    GList one = { path, NULL, NULL };
    gimp_image_add_path (image, path, NULL, 0, FALSE);
    gimp_image_set_selected_paths (image, NULL);
    g_object_set (options, "align-reference", GIMP_ALIGN_REFERENCE_IMAGE,
                  "align-vectors", TRUE, NULL);
    g_assert_false (gtk_widget_get_sensitive (align_action (second, GIMP_ALIGN_LEFT)));
    gimp_image_set_selected_paths (image, &one);
    g_assert_true (gtk_widget_get_sensitive (align_action (second, GIMP_ALIGN_LEFT)));
    gimp_image_set_selected_paths (image, NULL);
    g_assert_false (gtk_widget_get_sensitive (align_action (native, GIMP_ALIGN_LEFT)));
  }
  gtk_widget_destroy (first);
  g_object_unref (first);
  g_object_set (options, "pivot-x", 0.0, "pivot-y", 0.0, NULL);
  align_assert_pivot (second, 0.0, 0.0);
  gimp_context_set_tool (context, info ("gimp-move-tool"));
  drain ();
  gimp_context_set_tool (context, tool);
  drain ();
  g_assert_true (gimp_tools_get_tool_options_gui (tool->tool_options) == native);
  g_assert_true (gtk_widget_get_parent (native) == parent);
  gtk_widget_destroy (second);
  g_object_unref (second);
  g_object_set (options, "align-layers", TRUE, "align-vectors", FALSE,
                "align-contents", TRUE, "pivot-x", 0.5, "pivot-y", 0.5, NULL);
  gimp_context_set_tool (context, old_tool);
  close_shell (shell);
}

static void
align_model_before_view (void)
{
  GimpAlignOptions *options = g_object_new (GIMP_TYPE_ALIGN_OPTIONS,
                                           "gimp", gimp,
                                           "tool-info", info ("gimp-align-tool"), NULL);
  GtkWidget *dock = gimp_align_options_gui (GIMP_TOOL_OPTIONS (options));
  GtkWidget *toolbar = gimp_align_options_gui_horizontal (GIMP_TOOL_OPTIONS (options));
  GtkWidget *button = g_object_ref (align_action (toolbar, GIMP_ALIGN_LEFT));
  GtkWidget *pivot = g_object_ref (align_pivot (dock));
  gboolean gone = FALSE;

  g_object_ref_sink (dock);
  g_object_ref_sink (toolbar);
  g_object_weak_ref (G_OBJECT (options), finalized, &gone);
  g_object_unref (options);
  /* Native property widgets retain the model until their own destruction. */
  g_assert_false (gone);
  gtk_widget_destroy (toolbar);
  gtk_widget_destroy (dock);
  g_assert_true (gone);
  /* Destroyed roots and action/pivot widgets can outlive the model. */
  gtk_button_clicked (GTK_BUTTON (button));
  g_signal_emit_by_name (pivot, "changed");
  g_object_unref (button);
  g_object_unref (pivot);
  g_object_unref (toolbar);
  g_object_unref (dock);
}

static void
align_record_action (GimpAlignOptions *options, GimpAlignmentType action,
                      guint *counts)
{
  g_assert_cmpint (action, >=, GIMP_ALIGN_LEFT);
  g_assert_cmpint (action, <=, GIMP_DISTRIBUTE_EVEN_VERTICAL_GAP);
  counts[action]++;
}

typedef struct
{
  GimpAlignOptions *options;
  GtkWidget       *peer;
  guint            calls;
} AlignReentry;

static void
align_reenter_update (GtkWidget *button, GParamSpec *pspec, AlignReentry *state)
{
  if (!state->calls && gtk_widget_get_sensitive (button))
    {
      state->calls++;
      gtk_widget_destroy (state->peer);
      g_object_set (state->options, "align-layers", FALSE, NULL);
    }
}

static void
align_actions_and_reentry (void)
{
  GimpDisplayShell *shell = new_shell ();
  GimpAlignOptions *options = g_object_new (GIMP_TYPE_ALIGN_OPTIONS,
                                           "gimp", gimp,
                                           "tool-info", info ("gimp-align-tool"), NULL);
  GtkWidget *first = gimp_align_options_gui_horizontal (GIMP_TOOL_OPTIONS (options));
  GtkWidget *second = gimp_align_options_gui_horizontal (GIMP_TOOL_OPTIONS (options));
  GtkWidget *dock = gimp_align_options_gui (GIMP_TOOL_OPTIONS (options));
  GtkWidget *retained_pivot = g_object_ref (align_pivot (second));
  guint counts[10] = { 0 };
  AlignReentry state = { options, second, 0 };

  g_object_ref_sink (first);
  g_object_ref_sink (second);
  g_object_ref_sink (dock);
  g_signal_connect (options, "align-button-clicked", G_CALLBACK (align_record_action), counts);
  for (guint i = 0; i < G_N_ELEMENTS (counts); i++)
    {
      gtk_button_clicked (GTK_BUTTON (align_action (first, i)));
      g_assert_cmpuint (counts[i], ==, 1);
    }
  g_object_set (options, "align-layers", FALSE, NULL);
  g_signal_connect (align_action (first, GIMP_ALIGN_LEFT), "notify::sensitive",
                    G_CALLBACK (align_reenter_update), &state);
  g_object_set (options, "align-layers", TRUE, NULL);
  g_assert_cmpuint (state.calls, ==, 1);
  for (guint i = 0; i < G_N_ELEMENTS (counts); i++)
    {
      g_assert_false (gtk_widget_get_sensitive (align_action (first, i)));
      g_assert_false (gtk_widget_get_sensitive (align_action (dock, i)));
    }
  /* The destroyed peer's retained selector must not receive model updates. */
  g_object_set (options, "pivot-x", 1.0, "pivot-y", 0.0, NULL);
  align_assert_pivot (first, 1.0, 0.0);
  g_object_unref (retained_pivot);
  g_object_unref (second);
  gtk_widget_destroy (first);
  gtk_widget_destroy (dock);
  g_object_unref (first);
  g_object_unref (dock);
  g_object_unref (options);
  close_shell (shell);
}
