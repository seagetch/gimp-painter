/* Independent rectangle views use the native Crop/Select action and lifecycle. */
#include "core/gimpchannel.h"
#include "core/gimpdrawable.h"
#include "core/gimpimage.h"
#include "core/gimpimage-undo.h"
#include "core/gimplayer.h"
#include "core/gimpundostack.h"
#include "display/gimptoolrectangle.h"
#include "tools/gimpcropoptions.h"
#include "tools/gimpdrawtool.h"
#include "tools/gimprectangleoptions.h"
#include "tools/gimprectangleselectoptions.h"
#include "tools/gimptoolcontrol.h"
#include "tools/tool_manager.h"

static const gchar *rectangle_tool_names[] = {
  "gimp-crop-tool", "gimp-rect-select-tool", "gimp-ellipse-select-tool"
};

typedef struct
{
  GimpToolInfo     *previous;
  GimpToolInfo     *info;
  GimpToolOptions  *options;
  GimpConfig       *saved;
  GimpTool         *tool;
  GimpDisplayShell *shell;
  GimpImage        *image;
  GimpLayer        *selected;
  GimpLayer        *other;
  GimpLayer        *hidden;
} RectangleFixture;

static GtkWidget *
rectangle_action (GtkWidget *root)
{
  GPtrArray *widgets = g_ptr_array_new ();
  GtkWidget *result = NULL;
  guint count = 0;

  gather (root, widgets);
  for (guint i = 0; i < widgets->len; i++)
    {
      GtkWidget *widget = g_ptr_array_index (widgets, i);
      if (GTK_IS_BUTTON (widget) &&
          !g_strcmp0 (gtk_button_get_label (GTK_BUTTON (widget)),
                       g_dgettext (GETTEXT_PACKAGE, "Auto Shrink")))
        {
          result = widget;
          count++;
        }
    }
  g_ptr_array_unref (widgets);
  g_assert_cmpuint (count, ==, 1);
  return result;
}

static GtkWidget *
rectangle_merged (GtkWidget *root)
{
  GtkWidget *toggle = find (root, "shrink-merged", GTK_TYPE_CHECK_BUTTON);
  g_assert_nonnull (toggle);
  return toggle;
}

static GtkWidget *
rectangle_new_view (GimpToolOptions *options)
{
  GtkWidget *view = gimp_tools_create_painter_toolbar_gui (options);
  g_assert_nonnull (view);
  g_assert_true (g_object_is_floating (view));
  g_object_ref_sink (view);
  g_assert_null (gtk_widget_get_parent (view));
  g_assert_cmpint (gtk_orientable_get_orientation (GTK_ORIENTABLE (view)),
                   ==, GTK_ORIENTATION_HORIZONTAL);
  return view;
}

static void
rectangle_destroy_view (GtkWidget *view)
{
  gtk_widget_destroy (view);
  g_object_unref (view);
}

static GimpLayer *
rectangle_content_layer (GimpImage *image, const gchar *name,
                         gint width, gint height, gint offset_x, gint offset_y,
                         gint x, gint y, gint content_width, gint content_height)
{
  GimpLayer *layer = gimp_layer_new (image, width, height,
                                    gimp_image_get_layer_format (image, TRUE),
                                    name, 1.0, GIMP_LAYER_MODE_NORMAL);
  GeglBuffer *buffer = gimp_drawable_get_buffer (GIMP_DRAWABLE (layer));
  GeglColor *red = gegl_color_new ("red");
  GeglColor *green = gegl_color_new ("green");

  gegl_buffer_clear (buffer, NULL);
  gegl_buffer_set_color (buffer, GEGL_RECTANGLE (x, y, content_width, content_height), red);
  /* A duplicate native callback would shrink the red frame a second time,
   * down to the green inset. Exact bounds catch duplicate delivery even
   * when an otherwise uniform content rectangle would make it a no-op. */
  gegl_buffer_set_color (buffer,
                         GEGL_RECTANGLE (x + 8, y + 8,
                                         content_width - 16, content_height - 16), green);
  g_object_unref (red);
  g_object_unref (green);
  gimp_image_add_layer (image, layer, NULL, 0, FALSE);
  gimp_item_set_offset (GIMP_ITEM (layer), offset_x, offset_y);
  return layer;
}

static void
rectangle_fixture_init (RectangleFixture *fixture, guint tool_index)
{
  GimpContext *context = gimp_get_user_context (gimp);
  GList selected = { NULL, NULL, NULL };

  fixture->previous = gimp_context_get_tool (context);
  fixture->info = info (rectangle_tool_names[tool_index]);
  fixture->options = fixture->info->tool_options;
  fixture->saved = gimp_config_duplicate (GIMP_CONFIG (fixture->options));
  gimp_context_set_tool (context, info ("gimp-move-tool"));
  fixture->shell = new_shell ();
  fixture->image = gimp_display_get_image (fixture->shell->display);
  for (GList *iter = gimp_image_get_layer_iter (fixture->image); iter; iter = iter->next)
    gimp_item_set_visible (GIMP_ITEM (iter->data), FALSE, FALSE);
  fixture->selected = rectangle_content_layer (fixture->image, "Selected offset content",
                                                144, 144, 31, 23, 12, 18, 70, 56);
  fixture->other = rectangle_content_layer (fixture->image, "Other visible content",
                                             96, 96, 104, 76, 14, 10, 64, 54);
  fixture->hidden = rectangle_content_layer (fixture->image, "Hidden outer content",
                                              224, 224, 0, 0, 8, 9, 212, 208);
  gimp_item_set_visible (GIMP_ITEM (fixture->hidden), FALSE, FALSE);
  selected.data = fixture->selected;
  gimp_image_set_selected_layers (fixture->image, &selected);
  g_object_set (fixture->options, "fixed-rule-active", FALSE,
                "fixed-center", FALSE, "shrink-merged", FALSE,
                "position-unit", gimp_unit_pixel (), "size-unit", gimp_unit_pixel (), NULL);
  if (GIMP_IS_CROP_OPTIONS (fixture->options))
    g_object_set (fixture->options, "layer-only", FALSE, "allow-growing", FALSE,
                  "delete-pixels", FALSE, NULL);
  else
    g_object_set (fixture->options, "operation", GIMP_CHANNEL_OP_REPLACE,
                  "feather", FALSE, "antialias", FALSE, "round-corners", FALSE, NULL);
  gimp_context_set_tool (context, fixture->info);
  drain ();
  fixture->tool = g_object_ref (tool_manager_get_active (gimp));
  g_assert_true (fixture->tool->tool_info == fixture->info);
  g_assert_null (GIMP_DRAW_TOOL (fixture->tool)->widget);
}

static void
rectangle_halt (RectangleFixture *fixture)
{
  gimp_tool_control (fixture->tool, GIMP_TOOL_ACTION_HALT, fixture->tool->display);
  g_assert_null (fixture->tool->display);
  g_assert_null (GIMP_DRAW_TOOL (fixture->tool)->widget);
}

static void
rectangle_fixture_clear (RectangleFixture *fixture)
{
  GimpContext *context = gimp_get_user_context (gimp);

  rectangle_halt (fixture);
  gimp_context_set_tool (context, info ("gimp-move-tool"));
  g_clear_object (&fixture->tool);
  gimp_config_sync (G_OBJECT (fixture->saved), G_OBJECT (fixture->options), 0);
  g_clear_object (&fixture->saved);
  close_shell (fixture->shell);
  gimp_context_set_tool (context, fixture->previous);
  drain ();
}

static void
rectangle_assert_widget_bounds (GimpToolWidget *widget,
                                gint x, gint y, gint width, gint height)
{
  gdouble x1, y1, x2, y2;

  g_assert_true (GIMP_IS_TOOL_RECTANGLE (widget));
  gimp_tool_rectangle_get_public_rect (GIMP_TOOL_RECTANGLE (widget), &x1, &y1, &x2, &y2);
  g_assert_cmpfloat (x1, ==, x);
  g_assert_cmpfloat (y1, ==, y);
  g_assert_cmpfloat (x2, ==, x + width);
  g_assert_cmpfloat (y2, ==, y + height);
}

static void
rectangle_assert_bounds (RectangleFixture *fixture,
                         gint x, gint y, gint width, gint height)
{
  gdouble actual_x, actual_y, actual_width, actual_height;

  rectangle_assert_widget_bounds (GIMP_DRAW_TOOL (fixture->tool)->widget,
                                   x, y, width, height);
  g_object_get (fixture->options, "x", &actual_x, "y", &actual_y,
                "width", &actual_width, "height", &actual_height, NULL);
  g_assert_cmpfloat (actual_x, ==, x);
  g_assert_cmpfloat (actual_y, ==, y);
  g_assert_cmpfloat (actual_width, ==, width);
  g_assert_cmpfloat (actual_height, ==, height);
}

static void
rectangle_drag_on_display (RectangleFixture *fixture, GimpDisplay *display)
{
  GimpCoords coords = GIMP_COORDS_DEFAULT_VALUES;
  guint32 time = (guint32) (g_get_monotonic_time () / 1000);

  coords.x = 0.0;
  coords.y = 0.0;
  g_assert_true (gimp_tool_initialize (fixture->tool, display));
  gimp_tool_set_focus_display (fixture->tool, display);
  gimp_tool_oper_update (fixture->tool, &coords, 0, TRUE, display);
  gimp_tool_button_press (fixture->tool, &coords, time, GDK_BUTTON1_MASK,
                          GIMP_BUTTON_PRESS_NORMAL, display);
  g_assert_true (gimp_tool_control_is_active (fixture->tool->control));
  /* The first native motion leaves CREATING and chooses a resize corner;
   * the following motion actually expands that corner. */
  coords.x = 16.0;
  coords.y = 16.0;
  gimp_tool_motion (fixture->tool, &coords, time + 50, GDK_BUTTON1_MASK, display);
  coords.x = 240.0;
  coords.y = 240.0;
  gimp_tool_motion (fixture->tool, &coords, time + 100, GDK_BUTTON1_MASK, display);
  gimp_tool_button_release (fixture->tool, &coords, time + 200, 0, display);
  g_assert_false (gimp_tool_control_is_active (fixture->tool->control));
  g_assert_true (fixture->tool->display == display);
  rectangle_assert_bounds (fixture, 0, 0, 240, 240);
}

static void
rectangle_start (RectangleFixture *fixture)
{
  rectangle_halt (fixture);
  gimp_channel_clear (gimp_image_get_mask (fixture->image), NULL, FALSE);
  rectangle_drag_on_display (fixture, fixture->shell->display);
}

static void
rectangle_change_complete (GimpToolRectangle *rectangle, guint *calls)
{
  (void) rectangle;
  (*calls)++;
}

static void
rectangle_click_and_assert (RectangleFixture *fixture, GtkWidget *root,
                             gint x, gint y, gint width, gint height)
{
  GimpToolWidget *widget = GIMP_DRAW_TOOL (fixture->tool)->widget;
  GimpImage *image = gimp_display_get_image (fixture->tool->display);
  gint image_width = gimp_image_get_width (image);
  gint image_height = gimp_image_get_height (image);
  gint undo_depth = gimp_undo_stack_get_depth (gimp_image_get_undo_stack (image));
  guint calls = 0;
  gulong handler;

  g_assert_true (gtk_widget_get_sensitive (rectangle_action (root)));
  handler = g_signal_connect (widget, "change-complete",
                              G_CALLBACK (rectangle_change_complete), &calls);
  gtk_button_clicked (GTK_BUTTON (rectangle_action (root)));
  g_signal_handler_disconnect (widget, handler);
  g_assert_cmpuint (calls, ==, 1);
  rectangle_assert_bounds (fixture, x, y, width, height);
  g_assert_cmpint (gimp_image_get_width (image), ==, image_width);
  g_assert_cmpint (gimp_image_get_height (image), ==, image_height);
  if (GIMP_IS_CROP_OPTIONS (fixture->options))
    g_assert_cmpint (gimp_undo_stack_get_depth (gimp_image_get_undo_stack (image)), ==, undo_depth);
  else
    {
      gint mask_x, mask_y, mask_width, mask_height;
      /* Selection tools deliberately update the native selection on shrink. */
      g_assert_true (gimp_item_bounds (GIMP_ITEM (gimp_image_get_mask (image)),
                                       &mask_x, &mask_y, &mask_width, &mask_height));
      g_assert_cmpint (mask_x, ==, x);
      g_assert_cmpint (mask_y, ==, y);
      g_assert_cmpint (mask_width, ==, width);
      g_assert_cmpint (mask_height, ==, height);
    }
}

static void
rectangle_assert_inactive (RectangleFixture *fixture, GtkWidget **views, guint count)
{
  gdouble x, y, width, height, after_x, after_y, after_width, after_height;

  g_assert_null (GIMP_DRAW_TOOL (fixture->tool)->widget);
  g_object_get (fixture->options, "x", &x, "y", &y, "width", &width, "height", &height, NULL);
  for (guint i = 0; i < count; i++)
    {
      g_assert_false (gtk_widget_get_sensitive (rectangle_action (views[i])));
      /* Programmatic emission must be inert, independently of GTK gating. */
      gtk_button_clicked (GTK_BUTTON (rectangle_action (views[i])));
    }
  g_object_get (fixture->options, "x", &after_x, "y", &after_y,
                "width", &after_width, "height", &after_height, NULL);
  g_assert_cmpfloat (after_x, ==, x);
  g_assert_cmpfloat (after_y, ==, y);
  g_assert_cmpfloat (after_width, ==, width);
  g_assert_cmpfloat (after_height, ==, height);
  g_assert_null (GIMP_DRAW_TOOL (fixture->tool)->widget);
}

static void
independent_rectangle_toolbar (void)
{
  for (guint tool = 0; tool < G_N_ELEMENTS (rectangle_tool_names); tool++)
    {
      RectangleFixture fixture;
      GimpRectangleOptionsPrivate *private;
      GtkWidget *views[3], *parent;
      gpointer native_handles[11];

      g_test_message ("Rectangle tool: %s", rectangle_tool_names[tool]);
      rectangle_fixture_init (&fixture, tool);
      views[0] = gimp_tools_get_tool_options_gui (fixture.options);
      parent = gtk_widget_get_parent (views[0]);
      private = GIMP_RECTANGLE_OPTIONS_GET_PRIVATE (fixture.options);
      native_handles[0] = private->auto_shrink_button;
      native_handles[1] = private->fixed_width_entry;
      native_handles[2] = private->fixed_height_entry;
      native_handles[3] = private->fixed_aspect_hbox;
      native_handles[4] = private->aspect_button_box;
      native_handles[5] = private->fixed_size_hbox;
      native_handles[6] = private->size_button_box;
      native_handles[7] = private->position_entry;
      native_handles[8] = private->size_entry;
      native_handles[9] = private->aspect_history;
      native_handles[10] = private->size_history;
      views[1] = rectangle_new_view (fixture.options);
      views[2] = rectangle_new_view (fixture.options);
      g_assert_true (views[0] != views[1] && views[1] != views[2]);
      for (guint view = 0; view < G_N_ELEMENTS (views); view++)
        {
          g_assert_true (rectangle_action (views[view]) != rectangle_action (views[(view + 1) % 3]));
          g_assert_true (rectangle_merged (views[view]) != rectangle_merged (views[(view + 1) % 3]));
        }
      rectangle_assert_inactive (&fixture, views, G_N_ELEMENTS (views));
      for (guint source = 0; source < G_N_ELEMENTS (views); source++)
        for (guint value = 0; value < 2; value++)
          {
            gboolean actual = !value;
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rectangle_merged (views[source])), value);
            g_object_get (fixture.options, "shrink-merged", &actual, NULL);
            g_assert_cmpint (actual, ==, value);
            for (guint target = 0; target < G_N_ELEMENTS (views); target++)
              g_assert_cmpint (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (rectangle_merged (views[target]))), ==, value);
          }
      g_object_set (fixture.options, "shrink-merged", FALSE, NULL);
      for (guint view = 0; view < G_N_ELEMENTS (views); view++)
        g_assert_false (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (rectangle_merged (views[view]))));
      rectangle_destroy_view (views[1]);
      g_object_set (fixture.options, "shrink-merged", TRUE, NULL);
      views[1] = rectangle_new_view (fixture.options);
      g_assert_true (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (rectangle_merged (views[1]))));
      rectangle_destroy_view (views[1]);
      rectangle_destroy_view (views[2]);
      g_assert_true (gimp_tools_get_tool_options_gui (fixture.options) == views[0]);
      g_assert_true (gtk_widget_get_parent (views[0]) == parent);
      g_assert_true (private->auto_shrink_button == native_handles[0]);
      g_assert_true (rectangle_action (views[0]) == native_handles[0]);
      g_assert_true (private->fixed_width_entry == native_handles[1]);
      g_assert_true (private->fixed_height_entry == native_handles[2]);
      g_assert_true (private->fixed_aspect_hbox == native_handles[3]);
      g_assert_true (private->aspect_button_box == native_handles[4]);
      g_assert_true (private->fixed_size_hbox == native_handles[5]);
      g_assert_true (private->size_button_box == native_handles[6]);
      g_assert_true (private->position_entry == native_handles[7]);
      g_assert_true (private->size_entry == native_handles[8]);
      g_assert_true (private->aspect_history == native_handles[9]);
      g_assert_true (private->size_history == native_handles[10]);
      rectangle_fixture_clear (&fixture);
    }
}

static void
rectangle_native_shrink_bounds (void)
{
  for (guint tool = 0; tool < G_N_ELEMENTS (rectangle_tool_names); tool++)
    {
      RectangleFixture fixture;
      GtkWidget *views[3];
      GList selected = { NULL, NULL, NULL };

      g_test_message ("Rectangle tool: %s", rectangle_tool_names[tool]);
      rectangle_fixture_init (&fixture, tool);
      views[0] = gimp_tools_get_tool_options_gui (fixture.options);
      views[1] = rectangle_new_view (fixture.options);
      views[2] = rectangle_new_view (fixture.options);
      for (guint view = 0; view < G_N_ELEMENTS (views); view++)
        {
          g_object_set (fixture.options, "shrink-merged", FALSE, NULL);
          rectangle_start (&fixture);
          rectangle_click_and_assert (&fixture, views[view], 43, 41, 70, 56);
          g_assert_cmpint (gimp_item_get_offset_x (GIMP_ITEM (fixture.selected)), ==, 31);
          g_assert_cmpint (gimp_item_get_offset_y (GIMP_ITEM (fixture.selected)), ==, 23);
          g_assert_cmpint (gimp_item_get_width (GIMP_ITEM (fixture.selected)), ==, 144);
          g_assert_cmpint (gimp_item_get_height (GIMP_ITEM (fixture.selected)), ==, 144);
          gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rectangle_merged (views[view])), TRUE);
          rectangle_start (&fixture);
          /* Visible projection includes the second layer, excludes hidden. */
          rectangle_click_and_assert (&fixture, views[view], 43, 41, 139, 99);
        }
      rectangle_halt (&fixture);
      gimp_item_set_visible (GIMP_ITEM (fixture.hidden), TRUE, FALSE);
      rectangle_start (&fixture);
      rectangle_click_and_assert (&fixture, views[2], 8, 9, 212, 208);
      rectangle_halt (&fixture);
      gimp_item_set_visible (GIMP_ITEM (fixture.hidden), FALSE, FALSE);
      selected.data = fixture.other;
      gimp_image_set_selected_layers (fixture.image, &selected);
      g_object_set (fixture.options, "shrink-merged", FALSE, NULL);
      rectangle_start (&fixture);
      rectangle_click_and_assert (&fixture, views[1], 118, 86, 64, 54);
      if (GIMP_IS_CROP_OPTIONS (fixture.options))
        {
          GdkEventKey enter = { 0 };
          enter.type = GDK_KEY_PRESS;
          enter.keyval = GDK_KEY_Return;
          g_assert_cmpint (gimp_image_get_width (fixture.image), ==, 256);
          g_assert_cmpint (gimp_image_get_height (fixture.image), ==, 256);
          g_assert_true (gimp_tool_key_press (fixture.tool, &enter, fixture.shell->display));
          g_assert_cmpint (gimp_image_get_width (fixture.image), ==, 64);
          g_assert_cmpint (gimp_image_get_height (fixture.image), ==, 54);
          rectangle_assert_inactive (&fixture, views, G_N_ELEMENTS (views));
        }
      rectangle_destroy_view (views[1]);
      rectangle_destroy_view (views[2]);
      rectangle_fixture_clear (&fixture);
    }
}

static void
rectangle_shrink_lifecycle (void)
{
  for (guint tool = 0; tool < G_N_ELEMENTS (rectangle_tool_names); tool++)
    {
      RectangleFixture fixture;
      GimpContext *context = gimp_get_user_context (gimp);
      GtkWidget *views[4], *retained;
      GimpDisplayShell *other_shell;
      GimpImage *other_image;
      GimpLayer *other_layer;
      GimpToolWidget *old_rectangle;
      GList selected = { NULL, NULL, NULL };

      g_test_message ("Rectangle tool: %s", rectangle_tool_names[tool]);
      rectangle_fixture_init (&fixture, tool);
      views[0] = gimp_tools_get_tool_options_gui (fixture.options);
      views[1] = rectangle_new_view (fixture.options);
      views[2] = rectangle_new_view (fixture.options);
      rectangle_assert_inactive (&fixture, views, 3);
      rectangle_start (&fixture);
      views[3] = rectangle_new_view (fixture.options);
      for (guint view = 0; view < G_N_ELEMENTS (views); view++)
        g_assert_true (gtk_widget_get_sensitive (rectangle_action (views[view])));
      retained = g_object_ref (rectangle_action (views[1]));
      rectangle_destroy_view (views[1]);
      gtk_button_clicked (GTK_BUTTON (retained));
      rectangle_assert_bounds (&fixture, 0, 0, 240, 240);
      rectangle_click_and_assert (&fixture, views[2], 43, 41, 70, 56);
      rectangle_start (&fixture);
      rectangle_click_and_assert (&fixture, views[0], 43, 41, 70, 56);
      views[1] = rectangle_new_view (fixture.options);
      for (guint cycle = 0; cycle < 3; cycle++)
        {
          GdkEventKey escape = { 0 };
          rectangle_start (&fixture);
          gtk_button_clicked (GTK_BUTTON (retained));
          rectangle_assert_bounds (&fixture, 0, 0, 240, 240);
          rectangle_click_and_assert (&fixture, views[cycle + 1], 43, 41, 70, 56);
          escape.type = GDK_KEY_PRESS;
          escape.keyval = GDK_KEY_Escape;
          g_assert_true (gimp_tool_key_press (fixture.tool, &escape, fixture.shell->display));
          rectangle_assert_inactive (&fixture, views, G_N_ELEMENTS (views));
        }
      g_object_unref (retained);

      /* A real gesture in a second display replaces the old native endpoint. */
      other_shell = new_shell ();
      other_image = gimp_display_get_image (other_shell->display);
      other_layer = rectangle_content_layer (other_image, "Second display offset content",
                                               144, 144, 71, 53, 12, 18, 70, 56);
      selected.data = other_layer;
      gimp_image_set_selected_layers (other_image, &selected);
      gimp_context_set_display (context, fixture.shell->display);
      rectangle_start (&fixture);
      old_rectangle = g_object_ref (GIMP_DRAW_TOOL (fixture.tool)->widget);
      gimp_context_set_display (context, other_shell->display);
      rectangle_drag_on_display (&fixture, other_shell->display);
      g_assert_true (GIMP_DRAW_TOOL (fixture.tool)->widget != old_rectangle);
      rectangle_click_and_assert (&fixture, views[3], 83, 71, 70, 56);
      rectangle_assert_widget_bounds (old_rectangle, 0, 0, 240, 240);
      g_object_unref (old_rectangle);
      close_shell (other_shell);
      rectangle_assert_inactive (&fixture, views, G_N_ELEMENTS (views));

      gimp_context_set_display (context, fixture.shell->display);
      rectangle_start (&fixture);
      gimp_context_set_tool (context, info ("gimp-move-tool"));
      drain ();
      rectangle_assert_inactive (&fixture, views, G_N_ELEMENTS (views));
      gimp_context_set_tool (context, fixture.info);
      drain ();
      g_clear_object (&fixture.tool);
      fixture.tool = g_object_ref (tool_manager_get_active (gimp));
      rectangle_assert_inactive (&fixture, views, G_N_ELEMENTS (views));
      rectangle_start (&fixture);
      rectangle_click_and_assert (&fixture, views[1], 43, 41, 70, 56);
      for (guint view = 1; view < G_N_ELEMENTS (views); view++)
        rectangle_destroy_view (views[view]);
      rectangle_fixture_clear (&fixture);
    }
}

/* Endpoint ownership/reentry is tested separately from authoritative native
 * image behavior above. These counters do not stand in for shrink results. */
typedef struct
{
  GimpRectangleOptions *options;
  GimpImage            *image;
  GObject              *replacement;
  gboolean              armed;
  guint                 changes;
} RectangleReentry;

static void
rectangle_owner_action (GObject *owner)
{
  guint *calls = g_object_get_data (owner, "rectangle-test-calls");
  (*calls)++;
}

static void
rectangle_replace_during_notify (GimpRectangleOptions *options, RectangleReentry *state)
{
  state->changes++;
  if (state->armed)
    {
      state->armed = FALSE;
      gimp_rectangle_options_connect (options, state->image,
                                       G_CALLBACK (rectangle_owner_action), state->replacement);
    }
}

static void
rectangle_shrink_owner_lifetime_and_reentry (void)
{
  GimpDisplayShell *shell = new_shell ();
  GimpImage *image = gimp_display_get_image (shell->display);

  for (guint type = 0; type < 2; type++)
    {
      GimpToolInfo *tool_info = info (rectangle_tool_names[type]);
      GimpRectangleOptions *options = g_object_new (G_OBJECT_TYPE (tool_info->tool_options),
                                                    "gimp", gimp, "tool-info", tool_info, NULL);
      GtkWidget *first = gimp_rectangle_options_auto_shrink_button_new (options);
      GtkWidget *late;
      GObject *owners[3];
      guint calls[3] = { 0, 0, 0 };
      gboolean gone[3] = { FALSE, FALSE, FALSE };
      gboolean options_gone = FALSE;
      RectangleReentry reentry = { options, image, NULL, FALSE, 0 };
      gulong handler;

      g_object_ref_sink (first);
      g_assert_false (gtk_widget_get_sensitive (first));
      for (guint i = 0; i < G_N_ELEMENTS (owners); i++)
        {
          owners[i] = g_object_new (G_TYPE_OBJECT, NULL);
          g_object_set_data (owners[i], "rectangle-test-calls", &calls[i]);
          g_object_weak_ref (owners[i], finalized, &gone[i]);
        }
      handler = g_signal_connect (options, "auto-shrink-changed",
                                   G_CALLBACK (rectangle_replace_during_notify), &reentry);
      gimp_rectangle_options_connect (options, image, G_CALLBACK (rectangle_owner_action), owners[0]);
      g_assert_cmpuint (reentry.changes, ==, 1);
      /* Reconnecting an identical endpoint is a no-op. */
      gimp_rectangle_options_connect (options, image, G_CALLBACK (rectangle_owner_action), owners[0]);
      g_assert_cmpuint (reentry.changes, ==, 1);
      reentry.replacement = owners[2];
      reentry.armed = TRUE;
      gimp_rectangle_options_connect (options, image, G_CALLBACK (rectangle_owner_action), owners[1]);
      g_assert_cmpuint (reentry.changes, ==, 3);
      late = gimp_rectangle_options_auto_shrink_button_new (options);
      g_object_ref_sink (late);
      g_assert_true (gtk_widget_get_sensitive (first));
      g_assert_true (gtk_widget_get_sensitive (late));
      gtk_button_clicked (GTK_BUTTON (first));
      g_assert_cmpuint (calls[0], ==, 0);
      g_assert_cmpuint (calls[1], ==, 0);
      g_assert_cmpuint (calls[2], ==, 1);
      /* Stale disconnect and replaced-owner finalization cannot affect latest. */
      gimp_rectangle_options_disconnect (options, G_CALLBACK (rectangle_owner_action), owners[1]);
      g_object_unref (owners[0]);
      g_object_unref (owners[1]);
      g_assert_true (gone[0]);
      g_assert_true (gone[1]);
      gtk_button_clicked (GTK_BUTTON (late));
      g_assert_cmpuint (calls[2], ==, 2);
      /* Active owner dies without an explicit disconnect. */
      g_object_unref (owners[2]);
      g_assert_true (gone[2]);
      g_assert_false (gtk_widget_get_sensitive (first));
      g_assert_false (gtk_widget_get_sensitive (late));
      gtk_button_clicked (GTK_BUTTON (first));
      gtk_button_clicked (GTK_BUTTON (late));
      g_assert_cmpuint (calls[2], ==, 2);
      g_signal_handler_disconnect (options, handler);

      /* Conversely, model finalization must remove its weak owner observer
       * and invalidate retained buttons before the live owner is released. */
      owners[0] = g_object_new (G_TYPE_OBJECT, NULL);
      gone[0] = FALSE;
      g_object_set_data (owners[0], "rectangle-test-calls", &calls[0]);
      g_object_weak_ref (owners[0], finalized, &gone[0]);
      gimp_rectangle_options_connect (options, image, G_CALLBACK (rectangle_owner_action), owners[0]);
      g_object_weak_ref (G_OBJECT (options), finalized, &options_gone);
      g_object_unref (options);
      g_assert_true (options_gone);
      gtk_button_clicked (GTK_BUTTON (first));
      gtk_button_clicked (GTK_BUTTON (late));
      g_assert_cmpuint (calls[0], ==, 0);
      g_object_unref (owners[0]);
      g_assert_true (gone[0]);
      rectangle_destroy_view (first);
      rectangle_destroy_view (late);
    }
  close_shell (shell);
}
