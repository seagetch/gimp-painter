/* Paths views share native properties and actions, without cached widget pointers. */
#include "config/gimpdialogconfig.h"
#include "core/gimpchannel.h"
#include "core/gimpchannel-select.h"
#include "core/gimpdrawable.h"
#include "core/gimpfilloptions.h"
#include "core/gimpimage.h"
#include "core/gimpimage-undo.h"
#include "core/gimplayer.h"
#include "core/gimpstrokeoptions.h"
#include "core/gimpundostack.h"
#include "tools/gimpvectoroptions.h"
#include "tools/gimpvectortool.h"
#include "tools/tool_manager.h"
#include "vectors/gimpbezierstroke.h"
#include "vectors/gimppath.h"

static const gchar *vector_action_labels[] = {
  "Selection from Path", "Fill Path", "Stroke Path"
};

typedef struct
{
  GimpToolInfo     *previous;
  GimpDisplayShell *shell;
  GimpImage        *image;
  GimpLayer        *layer;
  GimpPath         *path;
  GimpVectorTool   *tool;
  GimpVectorOptions *options;
  gint             edit_mode;
  gboolean         polygonal;
} VectorFixture;

static GtkWidget *
vector_action (GtkWidget *root, guint action)
{
  GPtrArray *widgets = g_ptr_array_new ();
  GtkWidget *result = NULL;
  guint count = 0;

  g_assert_cmpuint (action, <, G_N_ELEMENTS (vector_action_labels));
  gather (root, widgets);
  for (guint i = 0; i < widgets->len; i++)
    {
      GtkWidget *widget = g_ptr_array_index (widgets, i);
      if (GTK_IS_BUTTON (widget) &&
          !g_strcmp0 (gtk_button_get_label (GTK_BUTTON (widget)),
                       g_dgettext (GETTEXT_PACKAGE, vector_action_labels[action])))
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
vector_mode_button (GtkWidget *root, GimpPathMode mode)
{
  GPtrArray *widgets = g_ptr_array_new ();
  GtkWidget *result = NULL;
  guint count = 0;

  gather (root, widgets);
  for (guint i = 0; i < widgets->len; i++)
    {
      GtkWidget *widget = g_ptr_array_index (widgets, i);
      if (GTK_IS_RADIO_BUTTON (widget))
        {
          count++;
          if (GPOINTER_TO_INT (g_object_get_data (G_OBJECT (widget),
                                                 "gimp-item-data")) == mode)
            result = widget;
        }
    }
  g_ptr_array_unref (widgets);
  g_assert_cmpuint (count, ==, 3);
  g_assert_nonnull (result);
  return result;
}

static GimpPath *
vector_rectangle (GimpImage *image, const gchar *name, gdouble x, gdouble y)
{
  GimpPath *path = gimp_path_new (image, name);
  GimpCoords point = GIMP_COORDS_DEFAULT_VALUES;
  GimpStroke *stroke;

  point.x = x; point.y = y;
  stroke = gimp_bezier_stroke_new_moveto (&point);
  point.x = x + 96;
  gimp_bezier_stroke_lineto (stroke, &point);
  point.y = y + 96;
  gimp_bezier_stroke_lineto (stroke, &point);
  point.x = x;
  gimp_bezier_stroke_lineto (stroke, &point);
  gimp_stroke_close (stroke);
  gimp_path_stroke_add (path, stroke);
  g_object_unref (stroke);
  gimp_image_add_path (image, path, NULL, 0, FALSE);
  return path;
}

static void
vector_fixture_init (VectorFixture *fixture)
{
  GimpContext *context = gimp_get_user_context (gimp);
  GimpToolInfo *tool_info = info ("gimp-vector-tool");
  GList selected = { NULL, NULL, NULL };

  fixture->previous = gimp_context_get_tool (context);
  fixture->options = GIMP_VECTOR_OPTIONS (tool_info->tool_options);
  g_object_get (fixture->options, "vectors-edit-mode", &fixture->edit_mode,
                "vectors-polygonal", &fixture->polygonal, NULL);
  gimp_context_set_tool (context, info ("gimp-move-tool"));
  fixture->shell = new_shell ();
  fixture->image = gimp_display_get_image (fixture->shell->display);
  fixture->layer = gimp_layer_new (fixture->image, 256, 256,
                                   gimp_image_get_layer_format (fixture->image, TRUE),
                                   "Paths action target", 1.0, GIMP_LAYER_MODE_NORMAL);
  gimp_image_add_layer (fixture->image, fixture->layer, NULL, 0, FALSE);
  gegl_buffer_clear (gimp_drawable_get_buffer (GIMP_DRAWABLE (fixture->layer)), NULL);
  selected.data = fixture->layer;
  gimp_image_set_selected_layers (fixture->image, &selected);
  fixture->path = vector_rectangle (fixture->image, "Closed Paths fixture", 64, 64);
  selected.data = fixture->path;
  gimp_image_set_selected_paths (fixture->image, &selected);
  gimp_context_set_tool (context, tool_info);
  drain ();
  g_assert_true (GIMP_IS_VECTOR_TOOL (tool_manager_get_active (gimp)));
  fixture->tool = g_object_ref (GIMP_VECTOR_TOOL (tool_manager_get_active (gimp)));
  gimp_vector_tool_set_path (fixture->tool, fixture->path);
  g_assert_true (fixture->tool->path == fixture->path);
  g_assert_nonnull (fixture->tool->widget);
  gimp_channel_clear (gimp_image_get_mask (fixture->image), NULL, FALSE);
}

static void
vector_fixture_clear (VectorFixture *fixture)
{
  GimpContext *context = gimp_get_user_context (gimp);

  gimp_vector_tool_set_path (fixture->tool, NULL);
  gimp_context_set_tool (context, info ("gimp-move-tool"));
  g_object_unref (fixture->tool);
  g_object_set (fixture->options, "vectors-edit-mode", fixture->edit_mode,
                "vectors-polygonal", fixture->polygonal, NULL);
  close_shell (fixture->shell);
  gimp_context_set_tool (context, fixture->previous);
  drain ();
}

static void
vector_assert_available (GtkWidget *root, gboolean available)
{
  for (guint i = 0; i < G_N_ELEMENTS (vector_action_labels); i++)
    g_assert_cmpint (gtk_widget_get_sensitive (vector_action (root, i)), ==, available);
}

static void
vector_assert_properties (GtkWidget *root, gint mode, gboolean polygonal)
{
  GtkWidget *toggle = find (root, "vectors-polygonal", GTK_TYPE_CHECK_BUTTON);
  g_assert_nonnull (toggle);
  g_assert_true (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (vector_mode_button (root, mode))));
  g_assert_cmpint (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (toggle)), ==, polygonal);
}

static void
vector_assert_canvas_properties (VectorFixture *fixture, gint mode, gboolean polygonal)
{
  gint actual_mode = -1;
  gboolean actual_polygonal = !polygonal;

  g_object_get (fixture->options, "vectors-edit-mode", &actual_mode,
                "vectors-polygonal", &actual_polygonal, NULL);
  g_assert_cmpint (actual_mode, ==, mode);
  g_assert_cmpint (actual_polygonal, ==, polygonal);
  g_object_get (fixture->tool->widget, "edit-mode", &actual_mode,
                "polygonal", &actual_polygonal, NULL);
  g_assert_cmpint (actual_mode, ==, mode);
  g_assert_cmpint (actual_polygonal, ==, polygonal);
}

static void
independent_vector_toolbar (void)
{
  VectorFixture fixture;
  GtkWidget *views[3];
  GtkWidget *parent;
  GParamSpec *active_spec;
  gboolean available = FALSE;

  vector_fixture_init (&fixture);
  /* The live path exists before either independent toolbar is constructed. */
  views[0] = gimp_tools_get_tool_options_gui (GIMP_TOOL_OPTIONS (fixture.options));
  parent = gtk_widget_get_parent (views[0]);
  for (guint i = 1; i < G_N_ELEMENTS (views); i++)
    {
      views[i] = gimp_tools_create_painter_toolbar_gui (GIMP_TOOL_OPTIONS (fixture.options));
      g_assert_nonnull (views[i]);
      g_assert_true (g_object_is_floating (views[i]));
      g_object_ref_sink (views[i]);
      g_assert_true (views[i] != views[0]);
      g_assert_null (gtk_widget_get_parent (views[i]));
      g_assert_cmpint (gtk_orientable_get_orientation (GTK_ORIENTABLE (views[i])),
                       ==, GTK_ORIENTATION_HORIZONTAL);
      g_assert_cmpint (gtk_orientable_get_orientation (GTK_ORIENTABLE (
                       gtk_widget_get_parent (vector_mode_button (views[i], GIMP_PATH_MODE_DESIGN)))),
                       ==, GTK_ORIENTATION_HORIZONTAL);
    }
  g_assert_true (views[1] != views[2]);
  g_assert_cmpint (gtk_orientable_get_orientation (GTK_ORIENTABLE (views[0])),
                   ==, GTK_ORIENTATION_VERTICAL);
  active_spec = g_object_class_find_property (G_OBJECT_GET_CLASS (fixture.options), "path-active");
  g_assert_nonnull (active_spec);
  g_assert_true ((active_spec->flags & G_PARAM_READABLE) != 0);
  g_assert_true ((active_spec->flags & G_PARAM_WRITABLE) == 0);
  g_object_get (fixture.options, "path-active", &available, NULL);
  g_assert_true (available);
  for (guint i = 0; i < G_N_ELEMENTS (views); i++)
    {
      vector_assert_available (views[i], TRUE);
      for (guint action = 0; action < G_N_ELEMENTS (vector_action_labels); action++)
        g_assert_true (vector_action (views[i], action) != vector_action (views[(i + 1) % 3], action));
      g_assert_nonnull (strstr (gtk_button_get_label (GTK_BUTTON (vector_mode_button (views[i], GIMP_PATH_MODE_MOVE))),
                                gimp_get_mod_string (GDK_MOD1_MASK)));
      g_assert_nonnull (strstr (gtk_button_get_label (GTK_BUTTON (vector_mode_button (views[i], GIMP_PATH_MODE_EDIT))),
                                gimp_get_mod_string (gimp_get_toggle_behavior_mask ())));
    }
  for (guint source = 0; source < G_N_ELEMENTS (views); source++)
    for (gint mode = GIMP_PATH_MODE_DESIGN; mode <= GIMP_PATH_MODE_MOVE; mode++)
      {
        gboolean polygonal = (source + mode) % 2;
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (vector_mode_button (views[source], mode)), TRUE);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (find (views[source], "vectors-polygonal", GTK_TYPE_CHECK_BUTTON)), polygonal);
        for (guint target = 0; target < G_N_ELEMENTS (views); target++)
          vector_assert_properties (views[target], mode, polygonal);
        vector_assert_canvas_properties (&fixture, mode, polygonal);
      }
  g_object_set (fixture.options, "vectors-edit-mode", GIMP_PATH_MODE_EDIT,
                "vectors-polygonal", TRUE, NULL);
  for (guint i = 0; i < G_N_ELEMENTS (views); i++)
    vector_assert_properties (views[i], GIMP_PATH_MODE_EDIT, TRUE);
  gimp_config_reset_property (G_OBJECT (fixture.options), "vectors-edit-mode");
  gimp_config_reset_property (G_OBJECT (fixture.options), "vectors-polygonal");
  for (guint i = 0; i < G_N_ELEMENTS (views); i++)
    vector_assert_properties (views[i], GIMP_PATH_MODE_DESIGN, FALSE);
  vector_assert_canvas_properties (&fixture, GIMP_PATH_MODE_DESIGN, FALSE);
  gtk_widget_destroy (views[1]);
  g_object_unref (views[1]);
  for (guint cycle = 0; cycle < 8; cycle++)
    {
      views[1] = gimp_vector_options_gui_horizontal (GIMP_TOOL_OPTIONS (fixture.options));
      g_object_ref_sink (views[1]);
      vector_assert_available (views[1], TRUE);
      g_object_set (fixture.options, "vectors-polygonal", cycle % 2, NULL);
      vector_assert_properties (views[1], GIMP_PATH_MODE_DESIGN, cycle % 2);
      vector_assert_properties (views[2], GIMP_PATH_MODE_DESIGN, cycle % 2);
      gtk_widget_destroy (views[1]);
      g_object_unref (views[1]);
    }
  g_assert_true (gimp_tools_get_tool_options_gui (GIMP_TOOL_OPTIONS (fixture.options)) == views[0]);
  g_assert_true (gtk_widget_get_parent (views[0]) == parent);
  gtk_widget_destroy (views[2]);
  g_object_unref (views[2]);
  vector_fixture_clear (&fixture);
}

static guint8 *
vector_pixels (GimpDrawable *drawable, const Babl *format, gsize *length)
{
  gint width = gimp_item_get_width (GIMP_ITEM (drawable));
  gint height = gimp_item_get_height (GIMP_ITEM (drawable));
  guint8 *pixels;

  *length = (gsize) width * height * babl_format_get_bytes_per_pixel (format);
  pixels = g_malloc (*length);
  gegl_buffer_get (gimp_drawable_get_buffer (drawable), GEGL_RECTANGLE (0, 0, width, height),
                   1.0, format, pixels, GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);
  return pixels;
}

static void
vector_assert_mask (GimpImage *image, gboolean seed_only, gboolean overlap, gboolean path_only)
{
  gsize length;
  guint8 *mask = vector_pixels (GIMP_DRAWABLE (gimp_image_get_mask (image)), babl_format ("Y u8"), &length);

  g_assert_cmpuint (length, ==, 256 * 256);
  /* Entire interiors, including all four logical regions; edge antialiasing
   * is native and is deliberately excluded from these Boolean assertions. */
  for (guint y = 1; y < 255; y++)
    for (guint x = 1; x < 255; x++)
      {
        gboolean seed = x > 16 && x < 95 && y > 16 && y < 95;
        gboolean path = x > 64 && x < 159 && y > 64 && y < 159;
        gboolean expected;
        if (x == 16 || x == 95 || x == 64 || x == 159 ||
            y == 16 || y == 95 || y == 64 || y == 159)
          continue;
        expected = seed && path ? overlap : seed ? seed_only : path ? path_only : FALSE;
        g_assert_cmpuint (mask[y * 256 + x], ==, expected ? 255 : 0);
      }
  g_free (mask);
}

static void
vector_selection_actions (void)
{
  VectorFixture fixture;
  GtkWidget *views[3];
  const gboolean expected[4][3] = {
    { FALSE, TRUE, TRUE }, { TRUE, TRUE, TRUE },
    { TRUE, FALSE, FALSE }, { FALSE, TRUE, FALSE }
  };
  GdkModifierType modifiers[4] = {
    0, gimp_get_extend_selection_mask (), gimp_get_modify_selection_mask (),
    gimp_get_extend_selection_mask () | gimp_get_modify_selection_mask ()
  };

  vector_fixture_init (&fixture);
  views[0] = gimp_vector_options_gui (GIMP_TOOL_OPTIONS (fixture.options));
  views[1] = gimp_vector_options_gui_horizontal (GIMP_TOOL_OPTIONS (fixture.options));
  views[2] = gimp_vector_options_gui_horizontal (GIMP_TOOL_OPTIONS (fixture.options));
  for (guint view = 0; view < G_N_ELEMENTS (views); view++)
    g_object_ref_sink (views[view]);
  for (guint view = 0; view < G_N_ELEMENTS (views); view++)
    for (guint action = 0; action < G_N_ELEMENTS (modifiers); action++)
      {
        GimpChannel *mask = gimp_image_get_mask (fixture.image);
        gsize before_length, after_length;
        guint8 *before, *after;
        gint depth;

        gimp_channel_select_rectangle (mask, 16, 16, 80, 80, GIMP_CHANNEL_OP_REPLACE,
                                        FALSE, 0, 0, FALSE);
        before = vector_pixels (GIMP_DRAWABLE (mask), babl_format ("Y u8"), &before_length);
        depth = gimp_undo_stack_get_depth (gimp_image_get_undo_stack (fixture.image));
        if (action == 0)
          gtk_button_clicked (GTK_BUTTON (vector_action (views[view], 0)));
        else
          g_signal_emit_by_name (vector_action (views[view], 0), "extended-clicked", modifiers[action]);
        g_assert_cmpint (gimp_undo_stack_get_depth (gimp_image_get_undo_stack (fixture.image)), ==, depth + 1);
        vector_assert_mask (fixture.image, expected[action][0], expected[action][1], expected[action][2]);
        g_assert_true (gimp_image_undo (fixture.image));
        after = vector_pixels (GIMP_DRAWABLE (mask), babl_format ("Y u8"), &after_length);
        g_assert_cmpmem (before, before_length, after, after_length);
        g_free (before); g_free (after);
      }
  /* Destroying either kind of view leaves the surviving toolbar operational. */
  for (guint view = 0; view < 2; view++)
    {
      GtkWidget *retained = g_object_ref (vector_action (views[view], 0));
      gint depth = gimp_undo_stack_get_depth (gimp_image_get_undo_stack (fixture.image));
      gtk_widget_destroy (views[view]);
      g_object_unref (views[view]);
      gtk_button_clicked (GTK_BUTTON (retained));
      g_signal_emit_by_name (retained, "extended-clicked", gimp_get_extend_selection_mask ());
      g_assert_cmpint (gimp_undo_stack_get_depth (gimp_image_get_undo_stack (fixture.image)), ==, depth);
      g_object_unref (retained);
      gtk_button_clicked (GTK_BUTTON (vector_action (views[2], 0)));
      vector_assert_mask (fixture.image, FALSE, TRUE, TRUE);
      g_assert_true (gimp_image_undo (fixture.image));
    }
  gtk_widget_destroy (views[2]);
  g_object_unref (views[2]);
  vector_fixture_clear (&fixture);
}

static GtkWidget *
vector_find_dialog (const gchar *title, guint expected)
{
  GList *windows = gtk_window_list_toplevels ();
  GtkWidget *result = NULL;
  guint count = 0;

  for (GList *iter = windows; iter; iter = iter->next)
    if (GTK_IS_DIALOG (iter->data) && gtk_widget_get_visible (iter->data) &&
        !g_strcmp0 (gtk_window_get_title (GTK_WINDOW (iter->data)), g_dgettext (GETTEXT_PACKAGE, title)))
      {
        result = iter->data;
        count++;
      }
  g_list_free (windows);
  g_assert_cmpuint (count, ==, expected);
  return result;
}

static GtkWidget *
vector_open_dialog (GtkWidget *root, guint action, GtkWidget *host)
{
  GtkWidget *dialog;

  vector_find_dialog (vector_action_labels[action], 0);
  gtk_button_clicked (GTK_BUTTON (vector_action (root, action)));
  drain ();
  dialog = vector_find_dialog (vector_action_labels[action], 1);
  g_assert_true (gtk_window_get_transient_for (GTK_WINDOW (dialog)) == GTK_WINDOW (host));
  return dialog;
}

static void
vector_native_dialog_actions (void)
{
  VectorFixture fixture;
  GimpDialogConfig *config = GIMP_DIALOG_CONFIG (gimp->config);
  GimpConfig *saved_fill = gimp_config_duplicate (GIMP_CONFIG (config->fill_options));
  GimpConfig *saved_stroke = gimp_config_duplicate (GIMP_CONFIG (config->stroke_options));
  GtkWidget *views[3], *hosts[3];
  guint8 *before;
  gsize before_length;

  vector_fixture_init (&fixture);
  g_object_set (config->fill_options, "style", GIMP_FILL_STYLE_FG_COLOR,
                "antialias", FALSE, "feather", FALSE, NULL);
  g_object_set (config->stroke_options, "style", GIMP_FILL_STYLE_FG_COLOR,
                "method", GIMP_STROKE_LINE, "width", 8.0,
                "unit", gimp_unit_pixel (), "antialias", FALSE, NULL);
  views[0] = gimp_vector_options_gui (GIMP_TOOL_OPTIONS (fixture.options));
  views[1] = gimp_vector_options_gui_horizontal (GIMP_TOOL_OPTIONS (fixture.options));
  views[2] = gimp_vector_options_gui_horizontal (GIMP_TOOL_OPTIONS (fixture.options));
  for (guint view = 0; view < G_N_ELEMENTS (views); view++)
    {
      hosts[view] = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      g_object_ref_sink (hosts[view]);
      gtk_container_add (GTK_CONTAINER (hosts[view]), views[view]);
      gtk_widget_show_all (hosts[view]);
    }
  drain ();
  before = vector_pixels (GIMP_DRAWABLE (fixture.layer), babl_format ("R'G'B'A u8"), &before_length);
  for (guint view = 0; view < G_N_ELEMENTS (views); view++)
    for (guint action = 1; action <= 2; action++)
      {
        GtkWidget *dialog;
        guint8 *after;
        gsize after_length;
        gint depth = gimp_undo_stack_get_depth (gimp_image_get_undo_stack (fixture.image));

        dialog = vector_open_dialog (views[view], action, hosts[view]);
        gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL);
        drain ();
        vector_find_dialog (vector_action_labels[action], 0);
        after = vector_pixels (GIMP_DRAWABLE (fixture.layer), babl_format ("R'G'B'A u8"), &after_length);
        g_assert_cmpmem (before, before_length, after, after_length);
        g_free (after);
        g_assert_cmpint (gimp_undo_stack_get_depth (gimp_image_get_undo_stack (fixture.image)), ==, depth);

        dialog = vector_open_dialog (views[view], action, hosts[view]);
        gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
        drain ();
        vector_find_dialog (vector_action_labels[action], 0);
        g_assert_cmpint (gimp_undo_stack_get_depth (gimp_image_get_undo_stack (fixture.image)), ==, depth + 1);
        after = vector_pixels (GIMP_DRAWABLE (fixture.layer), babl_format ("R'G'B'A u8"), &after_length);
        g_assert_cmpuint (after_length, ==, before_length);
        g_assert_cmpint (memcmp (before, after, before_length), !=, 0);
        /* Fill changes the interior; line stroke changes its border only. */
        g_assert_cmpuint (after[(100 * 256 + 100) * 4 + 3], ==, action == 1 ? 255 : 0);
        g_assert_cmpuint (after[(100 * 256 + 65) * 4 + 3], ==, 255);
        g_assert_cmpuint (after[(220 * 256 + 220) * 4 + 3], ==, 0);
        g_free (after);
        g_assert_true (gimp_image_undo (fixture.image));
        after = vector_pixels (GIMP_DRAWABLE (fixture.layer), babl_format ("R'G'B'A u8"), &after_length);
        g_assert_cmpmem (before, before_length, after, after_length);
        g_free (after);
      }
  /* Preserve native no-drawable/content-lock refusal, with no empty dialog,
   * pixel change, or undo entry. Diagnostics go to the normal console. */
  {
    GimpMessageHandlerType handler = gimp->message_handler;
    GList selected = { fixture.layer, NULL, NULL };
    gint depth = gimp_undo_stack_get_depth (gimp_image_get_undo_stack (fixture.image));
    gimp->message_handler = GIMP_CONSOLE;
    gimp_image_set_selected_layers (fixture.image, NULL);
    for (guint action = 1; action <= 2; action++)
      {
        gtk_button_clicked (GTK_BUTTON (vector_action (views[2], action)));
        vector_find_dialog (vector_action_labels[action], 0);
      }
    gimp_image_set_selected_layers (fixture.image, &selected);
    gimp_item_set_lock_content (GIMP_ITEM (fixture.layer), TRUE, FALSE);
    for (guint action = 1; action <= 2; action++)
      {
        gtk_button_clicked (GTK_BUTTON (vector_action (views[2], action)));
        vector_find_dialog (vector_action_labels[action], 0);
      }
    gimp_item_set_lock_content (GIMP_ITEM (fixture.layer), FALSE, FALSE);
    gimp->message_handler = handler;
    g_assert_cmpint (gimp_undo_stack_get_depth (gimp_image_get_undo_stack (fixture.image)), ==, depth);
  }
  /* Open from a toolbar, then remove its entire view and halt its tool. The
   * dialog's original target remains valid and cancellation stays harmless. */
  {
    GtkWidget *dialog = vector_open_dialog (views[1], 2, hosts[1]);
    g_object_ref (dialog);
    gtk_widget_destroy (hosts[1]);
    gimp_context_set_tool (gimp_get_user_context (gimp), info ("gimp-move-tool"));
    gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL);
    g_object_unref (dialog);
    drain ();
    vector_find_dialog ("Stroke Path", 0);
  }
  /* Re-enter using the native path setter, with only the surviving views. */
  gimp_context_set_tool (gimp_get_user_context (gimp), info ("gimp-vector-tool"));
  g_object_unref (fixture.tool);
  fixture.tool = g_object_ref (GIMP_VECTOR_TOOL (tool_manager_get_active (gimp)));
  gimp_vector_tool_set_path (fixture.tool, fixture.path);
  /* The captured drawable can disappear independently of the path. Both
   * native dialogs must close before their borrowed target becomes stale. */
  for (guint action = 1; action <= 2; action++)
    {
      GtkWidget *dialog = vector_open_dialog (views[2], action, hosts[2]);
      GList selected = { NULL, NULL, NULL };
      g_signal_connect_swapped (dialog, "destroy", G_CALLBACK (g_nullify_pointer), &dialog);
      gimp_image_remove_layer (fixture.image, fixture.layer, FALSE, NULL);
      drain ();
      g_assert_null (dialog);
      fixture.layer = gimp_layer_new (fixture.image, 256, 256,
                                      gimp_image_get_layer_format (fixture.image, TRUE),
                                      "Replacement Paths target", 1.0, GIMP_LAYER_MODE_NORMAL);
      gimp_image_add_layer (fixture.image, fixture.layer, NULL, 0, FALSE);
      selected.data = fixture.layer;
      gimp_image_set_selected_layers (fixture.image, &selected);
    }
  {
    GtkWidget *dialog = vector_open_dialog (views[2], 2, hosts[2]);
    g_signal_connect_swapped (dialog, "destroy", G_CALLBACK (g_nullify_pointer), &dialog);
    gimp_image_remove_path (fixture.image, fixture.path, FALSE, NULL);
    drain ();
    g_assert_null (dialog);
    vector_assert_available (views[0], FALSE);
    vector_assert_available (views[2], FALSE);
  }
  /* Closing the original image closes both still-open native dialogs, even
   * though the options and their independent views continue to exist. */
  {
    GtkWidget *fill_dialog;
    GtkWidget *stroke_dialog;
    fixture.path = vector_rectangle (fixture.image, "Image closure path", 64, 64);
    gimp_vector_tool_set_path (fixture.tool, fixture.path);
    fill_dialog = vector_open_dialog (views[0], 1, hosts[0]);
    stroke_dialog = vector_open_dialog (views[2], 2, hosts[2]);
    g_signal_connect_swapped (fill_dialog, "destroy", G_CALLBACK (g_nullify_pointer), &fill_dialog);
    g_signal_connect_swapped (stroke_dialog, "destroy", G_CALLBACK (g_nullify_pointer), &stroke_dialog);
    vector_fixture_clear (&fixture);
    g_assert_null (fill_dialog);
    g_assert_null (stroke_dialog);
    vector_assert_available (views[0], FALSE);
    vector_assert_available (views[2], FALSE);
  }
  for (guint view = 0; view < G_N_ELEMENTS (hosts); view++)
    {
      gtk_widget_destroy (hosts[view]);
      g_object_unref (hosts[view]);
    }
  g_free (before);
  gimp_config_sync (G_OBJECT (saved_fill), G_OBJECT (config->fill_options), 0);
  gimp_config_sync (G_OBJECT (saved_stroke), G_OBJECT (config->stroke_options), 0);
  g_object_unref (saved_fill); g_object_unref (saved_stroke);
}

static void
vector_model_before_view (void)
{
  GimpVectorOptions *options = g_object_new (GIMP_TYPE_VECTOR_OPTIONS,
                                             "gimp", gimp,
                                             "tool-info", info ("gimp-vector-tool"), NULL);
  GtkWidget *first, *dock, *second, *retained;
  gboolean gone = FALSE;

  g_object_set (options, "vectors-edit-mode", GIMP_PATH_MODE_MOVE,
                "vectors-polygonal", TRUE, NULL);
  /* Reverse the usual creation order: toolbar, ordinary dock, toolbar. */
  first = gimp_vector_options_gui_horizontal (GIMP_TOOL_OPTIONS (options));
  dock = gimp_vector_options_gui (GIMP_TOOL_OPTIONS (options));
  second = gimp_vector_options_gui_horizontal (GIMP_TOOL_OPTIONS (options));
  g_object_ref_sink (first); g_object_ref_sink (dock); g_object_ref_sink (second);
  vector_assert_properties (first, GIMP_PATH_MODE_MOVE, TRUE);
  vector_assert_properties (dock, GIMP_PATH_MODE_MOVE, TRUE);
  vector_assert_properties (second, GIMP_PATH_MODE_MOVE, TRUE);
  vector_assert_available (first, FALSE);
  vector_assert_available (dock, FALSE);
  retained = g_object_ref (vector_action (first, 0));
  g_object_weak_ref (G_OBJECT (options), finalized, &gone);
  g_object_unref (options);
  g_assert_false (gone);
  gtk_widget_destroy (first);
  gtk_widget_destroy (dock);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (vector_mode_button (second, GIMP_PATH_MODE_EDIT)), TRUE);
  vector_assert_properties (second, GIMP_PATH_MODE_EDIT, TRUE);
  gtk_widget_destroy (second);
  g_assert_true (gone);
  gtk_button_clicked (GTK_BUTTON (retained));
  g_signal_emit_by_name (retained, "extended-clicked", gimp_get_extend_selection_mask ());
  g_object_unref (retained);
  g_object_unref (first); g_object_unref (dock); g_object_unref (second);
}

typedef struct
{
  GtkWidget *peer;
  GimpVectorTool *tool;
  guint calls;
} VectorReentry;

static void
vector_destroy_peer_on_sensitive (GtkWidget *button, GParamSpec *pspec, VectorReentry *state)
{
  if (!state->calls && gtk_widget_get_sensitive (button))
    {
      state->calls++;
      gtk_widget_destroy (state->peer);
      gimp_tool_control (GIMP_TOOL (state->tool), GIMP_TOOL_ACTION_HALT,
                          GIMP_TOOL (state->tool)->display);
    }
}

static void
vector_path_lifecycle_and_reentry (void)
{
  VectorFixture fixture;
  GtkWidget *first, *second, *dock;
  GimpPath *other;
  GList *selected;
  VectorReentry state;
  gboolean active = TRUE;

  vector_fixture_init (&fixture);
  first = gimp_vector_options_gui_horizontal (GIMP_TOOL_OPTIONS (fixture.options));
  second = gimp_vector_options_gui_horizontal (GIMP_TOOL_OPTIONS (fixture.options));
  dock = gimp_vector_options_gui (GIMP_TOOL_OPTIONS (fixture.options));
  g_object_ref_sink (first); g_object_ref_sink (second); g_object_ref_sink (dock);
  other = vector_rectangle (fixture.image, "Second selected path", 112, 112);
  gimp_vector_tool_set_path (fixture.tool, fixture.path);
  selected = g_list_append (NULL, fixture.path);
  selected = g_list_append (selected, other);
  gimp_image_set_selected_paths (fixture.image, selected);
  g_list_free (selected);
  vector_assert_available (first, FALSE);
  vector_assert_available (second, FALSE);
  vector_assert_available (dock, FALSE);
  g_object_get (fixture.options, "path-active", &active, NULL);
  g_assert_false (active);

  gimp_vector_tool_set_path (fixture.tool, fixture.path);
  vector_assert_available (first, TRUE);
  gimp_image_set_selected_paths (fixture.image, NULL);
  vector_assert_available (first, FALSE);
  vector_assert_available (dock, FALSE);
  state.peer = second; state.tool = fixture.tool; state.calls = 0;
  g_signal_connect (vector_action (first, 0), "notify::sensitive",
                    G_CALLBACK (vector_destroy_peer_on_sensitive), &state);
  gimp_vector_tool_set_path (fixture.tool, fixture.path);
  g_assert_cmpuint (state.calls, ==, 1);
  g_assert_null (fixture.tool->path);
  vector_assert_available (first, FALSE);
  vector_assert_available (dock, FALSE);
  g_object_unref (second);
  gimp_vector_tool_set_path (fixture.tool, fixture.path);
  vector_assert_available (first, TRUE);
  vector_assert_available (dock, TRUE);
  g_object_set (fixture.options, "vectors-polygonal", TRUE, NULL);
  vector_assert_properties (dock, GIMP_PATH_MODE_DESIGN, TRUE);

  /* Switching the same native tool across displayed images must disconnect
   * the old path and route every action to the new owning image. */
  {
    GimpDisplayShell *other_shell = new_shell ();
    GimpImage *other_image = gimp_display_get_image (other_shell->display);
    GimpPath *other_path = vector_rectangle (other_image, "Other image path", 64, 64);
    gimp_vector_tool_set_path (fixture.tool, other_path);
    g_assert_true (GIMP_TOOL (fixture.tool)->display == other_shell->display);
    g_assert_true (fixture.tool->path == other_path);
    vector_assert_available (first, TRUE);
    gtk_button_clicked (GTK_BUTTON (vector_action (first, 0)));
    vector_assert_mask (other_image, FALSE, TRUE, TRUE);
    g_assert_true (gimp_channel_is_empty (gimp_image_get_mask (fixture.image)));
    g_assert_true (gimp_image_undo (other_image));
    gimp_vector_tool_set_path (fixture.tool, fixture.path);
    g_assert_true (GIMP_TOOL (fixture.tool)->display == fixture.shell->display);
    g_assert_true (fixture.tool->path == fixture.path);
    gtk_button_clicked (GTK_BUTTON (vector_action (dock, 0)));
    vector_assert_mask (fixture.image, FALSE, TRUE, TRUE);
    g_assert_true (gimp_channel_is_empty (gimp_image_get_mask (other_image)));
    g_assert_true (gimp_image_undo (fixture.image));
    close_shell (other_shell);
  }
  gimp_tool_control (GIMP_TOOL (fixture.tool), GIMP_TOOL_ACTION_HALT, fixture.shell->display);
  vector_assert_available (first, FALSE);
  vector_assert_available (dock, FALSE);
  gimp_vector_tool_set_path (fixture.tool, fixture.path);
  vector_assert_available (dock, TRUE);
  gimp_context_set_tool (gimp_get_user_context (gimp), info ("gimp-move-tool"));
  vector_assert_available (first, FALSE);
  vector_assert_available (dock, FALSE);
  gtk_button_clicked (GTK_BUTTON (vector_action (first, 0)));
  g_assert_true (gimp_channel_is_empty (gimp_image_get_mask (fixture.image)));
  gtk_widget_destroy (first); gtk_widget_destroy (dock);
  g_object_unref (first); g_object_unref (dock);
  vector_fixture_clear (&fixture);
}
