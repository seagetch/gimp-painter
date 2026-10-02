/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gegl.h>
#include <gtk/gtk.h>
#include <json-glib/json-glib.h>
#include <string.h>
#include "libgimpbase/gimpbase.h"
#include "core/core-types.h"
#include "widgets/widgets-types.h"
#include "core/gimp.h"
#include "core/gimpcontext.h"
#include "core/gimpimage.h"
#include "core/gimpimage-undo.h"
#include "core/gimpcontainer.h"
#include "core/gimplist.h"
#include "core/gimplayer-new.h"
#include "core/gimplayerpreset.h"
#include "core/gimpclonelayer.h"
#include "core/gimpfilterlayer.h"
#include "core/gimpgrouplayer.h"
#include "core/gimpchannel.h"
#include "core/gimpchannel-select.h"
#include "core/gimpundostack.h"
#include "core/gimpdatafactory.h"
#include "config/gimpcoreconfig.h"
#include "operations/layer-modes-legacy/gimpoperationpainterlegacy.h"
#include "xcf/xcf.h"
#include "tests.h"
#include "gimp-app-test-utils.h"
static Gimp *gimp;
static const gchar *names[] = {"crop-with-selection.json", "duplicate-as-clone.json", "duplicate-as-normal.json", "new-anime-layer.json", "new-cropped-layer.json", "test.json", "test2.json", "watercolor.json"};
static gchar *asset (const char *name)
{ return g_build_filename (g_getenv ("GIMP_TESTING_ABS_TOP_SRCDIR"), "data", "layer-presets", name, NULL); }
static GimpLayerPreset *load_file (GFile *file)
{
  GError *error = NULL; GFileInputStream *stream = g_file_read (file, NULL, &error); GList *list;
  g_assert_no_error (error); g_assert_nonnull (stream);
  list = gimp_layer_preset_load (gimp->user_context, file, G_INPUT_STREAM (stream), &error);
  g_assert_no_error (error); g_assert_nonnull (list); g_object_unref (stream);
  GimpLayerPreset *preset = list->data; g_list_free (list); return preset;
}
static GimpLayerPreset *load (const gchar *name)
{ gchar *path = asset (name); GFile *file = g_file_new_for_path (path); GimpLayerPreset *preset = load_file (file); g_free (path); g_object_unref (file); return preset; }
static GimpImage *setup (guint id, GimpLayer **source)
{
  GimpImage *image = gimp_image_new (gimp, 100, 80, GIMP_RGB, GIMP_PRECISION_U8_NON_LINEAR);
  *source = (id == 5 || id == 6) ? gimp_filter_layer_new (image, 40, 30, "source", .6, GIMP_LAYER_MODE_SCREEN_LEGACY) :
    gimp_layer_new (image, 40, 30, gimp_image_get_layer_format (image, TRUE), "source", .6, GIMP_LAYER_MODE_SCREEN_LEGACY);
  gimp_image_add_layer (image, *source, NULL, 0, FALSE); gimp_item_set_offset (GIMP_ITEM (*source), 7, 9);
  gimp_context_set_image (gimp->user_context, image);
  gimp_channel_select_rectangle (gimp_image_get_mask (image), 12, 15, 20, 17, GIMP_CHANNEL_OP_REPLACE, FALSE, 0, 0, FALSE);
  return image;
}
static void dump (GString *text, const char *name, GimpContainer *container, const char *parent, GimpLayer *source)
{
  guint index = 0;
  for (GList *it = GIMP_LIST (container)->queue->head; it; it = it->next, ++index)
    {
      GimpLayer *layer = it->data; guint32 mode;
      gchar *path = g_strdup_printf ("%s/%u", parent, index);
      const char *type = GIMP_IS_GROUP_LAYER (layer) ? "group" : GIMP_IS_CLONE_LAYER (layer) ? "clone" : GIMP_IS_FILTER_LAYER (layer) ? "filter" : "normal";
      g_assert_true (gimp_painter_layer_mode_to_legacy (gimp_layer_get_mode (layer), &mode));
      g_string_append_printf (text, "PRESET\t%s\t%s\t%s\t%s\t%u\t%.17g\t%d\t%d\t%d\t%d\t%d\t%d", name, path, type, gimp_object_get_name (layer), mode, gimp_layer_get_opacity (layer), gimp_item_get_offset_x (GIMP_ITEM (layer)), gimp_item_get_offset_y (GIMP_ITEM (layer)), gimp_item_get_width (GIMP_ITEM (layer)), gimp_item_get_height (GIMP_ITEM (layer)), layer == source, GIMP_IS_CLONE_LAYER (layer) && gimp_clone_layer_get_source (GIMP_CLONE_LAYER (layer)) == source);
      if (GIMP_IS_FILTER_LAYER (layer))
        {
          gchar *procedure = gimp_filter_layer_dup_procedure (GIMP_FILTER_LAYER (layer));
          GimpValueArray *args = gimp_filter_layer_dup_args (GIMP_FILTER_LAYER (layer));
          g_string_append_printf (text, "\t%s", procedure ? procedure : ""); g_free (procedure);
          if (args)
            {
              for (guint j = 0; j < gimp_value_array_length (args); ++j)
                {
                  const GValue *v = gimp_value_array_index (args, j);
                  const gchar *type_name = G_VALUE_TYPE (v) ? G_VALUE_TYPE_NAME (v) : "null";
                  /* Legacy parameter GTypes were derived int types; map only
                   * the known stable ABI slots, retaining values and order. */
                  if (G_VALUE_HOLDS_INT (v)) type_name = j == 1 ? "GimpImageID" : j == 2 ? "GimpDrawableID" : "GimpInt32";
                  g_string_append_printf (text, "\t%s:", type_name);
                  if (G_VALUE_HOLDS_DOUBLE (v)) g_string_append_printf (text, "%.17g", g_value_get_double (v));
                  else if (G_VALUE_HOLDS_FLOAT (v)) g_string_append_printf (text, "%.17g", (double) g_value_get_float (v));
                  else if (G_VALUE_HOLDS_INT (v)) g_string_append_printf (text, "%d", g_value_get_int (v));
                }
              gimp_value_array_unref (args);
            }
        }
      g_string_append_c (text, '\n');
      if (GIMP_IS_GROUP_LAYER (layer)) dump (text, name, gimp_viewable_get_children (GIMP_VIEWABLE (layer)), path, source);
      g_free (path);
    }
}
static void all_bundled_construction (void)
{
  GString *actual = g_string_new (NULL); gchar *expected = NULL;
  gchar *path = g_build_filename (g_getenv ("GIMP_TESTING_ABS_TOP_SRCDIR"), "migration", "fixtures", "legacy-layer-presets", "construction.tsv", NULL);
  g_assert_true (g_file_get_contents (path, &expected, NULL, NULL)); g_free (path);
  for (guint i = 0; i < G_N_ELEMENTS (names); ++i)
    {
      GimpLayer *source; GimpImage *image = setup (i, &source); GimpLayerPreset *preset = load (names[i]); GError *error = NULL;
      g_assert_true (gimp_layer_preset_matches (preset, source));
      g_assert_true (gimp_layer_preset_apply (preset, gimp->user_context, source, &error)); g_assert_no_error (error);
      dump (actual, names[i], gimp_image_get_layers (image), "", source);
      g_string_append_printf (actual, "PRESET_UNDO\t%s\t%d\n", names[i], gimp_image_get_undo_group_count (image));
      g_assert_cmpint (gimp_undo_stack_get_depth (gimp_image_get_undo_stack (image)), ==, 1);
      g_assert_true (gimp_image_undo (image));
      g_assert_cmpint (gimp_container_get_n_children (gimp_image_get_layers (image)), ==, 1);
      g_assert_true (gimp_container_get_first_child (gimp_image_get_layers (image)) == GIMP_OBJECT (source));
      g_assert_cmpint (gimp_item_get_offset_x (GIMP_ITEM (source)), ==, 7);
      g_assert_cmpint (gimp_item_get_width (GIMP_ITEM (source)), ==, 40);
      g_assert_cmpfloat (gimp_layer_get_opacity (source), ==, .6);
      g_assert_true (gimp_image_redo (image));
      g_object_unref (preset); gimp_context_set_image (gimp->user_context, NULL); g_object_unref (image);
    }
  g_string_append (actual, "LAYER_PRESET_CAPTURE_COMPLETE\n");
  if (strcmp (expected, actual->str)) g_test_message ("Actual construction:\n%s", actual->str);
  g_assert_cmpstr (actual->str, ==, expected); g_free (expected); g_string_free (actual, TRUE);
}
static GimpLayerPreset *from_json (const char *text)
{
  JsonParser *parser = json_parser_new (); GError *error = NULL;
  g_assert_true (json_parser_load_from_data (parser, text, -1, &error)); g_assert_no_error (error);
  GimpLayerPreset *preset = GIMP_LAYER_PRESET (gimp_layer_preset_new (gimp->user_context, "test"));
  g_assert_true (gimp_layer_preset_set_json (preset, json_parser_get_root (parser), &error)); g_assert_no_error (error);
  g_object_unref (parser); return preset;
}
static void resource_roundtrip (void)
{
  const char *json = "{\"version\":1,\"name\":\"保存 🌻\",\"unknown\":{\"array\":[1,null,\"未来\"]},\"source-layer\":{\"type\":\"any\"},\"replacement-layer\":[]}";
  GimpLayerPreset *preset = from_json (json), *reloaded, *copy;
  gchar *directory = g_dir_make_tmp ("gimp-preset-XXXXXX", NULL), *filename = g_build_filename (directory, "preset.json", NULL);
  GFile *file = g_file_new_for_path (filename); GError *error = NULL;
  gimp_data_set_file (GIMP_DATA (preset), file, TRUE, TRUE);
  g_assert_true (gimp_data_save (GIMP_DATA (preset), &error)); g_assert_no_error (error);
  reloaded = load_file (file); copy = GIMP_LAYER_PRESET (gimp_data_duplicate (GIMP_DATA (preset)));
  JsonNode *a = gimp_layer_preset_dup_json (preset), *b = gimp_layer_preset_dup_json (reloaded), *c = gimp_layer_preset_dup_json (copy);
  g_assert_true (json_node_equal (a, b)); g_assert_true (json_node_equal (a, c));
  json_object_set_string_member (json_node_get_object (c), "copy-only", "changed");
  g_assert_false (json_object_has_member (json_node_get_object (a), "copy-only"));
  json_node_unref (a); json_node_unref (b); json_node_unref (c);
  g_object_unref (copy); g_object_unref (preset); g_object_unref (reloaded); g_file_delete (file, NULL, NULL); g_object_unref (file);
  GFile *dir = g_file_new_for_path (directory); g_file_delete (dir, NULL, NULL); g_object_unref (dir); g_free (directory); g_free (filename);
}
static void reject_without_history_loss (void)
{
  const char *cases[] = {
    "{\"version\":2,\"source-layer\":{\"type\":\"any\"},\"replacement-layer\":[]}",
    "{\"version\":1,\"source-layer\":{\"type\":\"filter\"},\"replacement-layer\":[]}",
    "{\"version\":1,\"source-layer\":{\"type\":\"any\"},\"replacement-layer\":[{\"target\":\"new\"},{\"target\":\"new\",\"type\":\"invalid\"}]}",
    "{\"version\":1,\"source-layer\":{\"type\":\"any\"},\"replacement-layer\":[{\"target\":\"source\"},{\"target\":\"invalid\"}]}",
    "{\"version\":1,\"source-layer\":{\"type\":\"any\"},\"replacement-layer\":[{\"target\":\"new\",\"boundary\":[0,0,-1,3]}]}",
    "{\"version\":1,\"source-layer\":{\"type\":\"any\"},\"replacement-layer\":[{\"target\":\"source\",\"boundary\":[-2147483648,0,-2147483647,1]}]}"
  };
  for (guint i = 0; i < G_N_ELEMENTS (cases); ++i)
    {
      GimpLayer *source; GimpImage *image = setup (0, &source); GError *error = NULL;
      gimp_layer_set_opacity (source, .8, TRUE); gimp_layer_set_opacity (source, .9, TRUE); gimp_image_undo (image);
      GimpUndo *undo = gimp_undo_stack_peek (gimp_image_get_undo_stack (image)), *redo = gimp_undo_stack_peek (gimp_image_get_redo_stack (image));
      GimpLayerPreset *preset = from_json (cases[i]);
      g_assert_false (gimp_layer_preset_apply (preset, gimp->user_context, source, &error)); g_assert_nonnull (error); g_clear_error (&error);
      g_assert_true (undo == gimp_undo_stack_peek (gimp_image_get_undo_stack (image))); g_assert_true (redo == gimp_undo_stack_peek (gimp_image_get_redo_stack (image)));
      g_assert_cmpint (gimp_image_get_undo_group_count (image), ==, 0); g_assert_cmpint (gimp_container_get_n_children (gimp_image_get_layers (image)), ==, 1);
      g_assert_cmpfloat (gimp_layer_get_opacity (source), ==, .8);
      g_object_unref (preset); gimp_context_set_image (gimp->user_context, NULL); g_object_unref (image);
    }
}
static void cancel_added (GimpContainer *container, GimpObject *object, GCancellable *cancel)
{ g_cancellable_cancel (cancel); }
static void rollback_after_insert (void)
{
  GimpLayer *source; GimpImage *image = setup (0, &source); GimpLayerPreset *preset = load ("new-anime-layer.json"); GError *error = NULL;
  GCancellable *cancel = g_cancellable_new ();
  gimp_layer_set_opacity (source, .8, TRUE); gimp_layer_set_opacity (source, .9, TRUE); gimp_image_undo (image);
  GimpUndo *undo = gimp_undo_stack_peek (gimp_image_get_undo_stack (image)), *redo = gimp_undo_stack_peek (gimp_image_get_redo_stack (image));
  gulong handler = g_signal_connect (gimp_image_get_layers (image), "add", G_CALLBACK (cancel_added), cancel);
  g_assert_false (gimp_layer_preset_apply_full (preset, gimp->user_context, source, cancel, &error)); g_assert_nonnull (error); g_clear_error (&error);
  g_signal_handler_disconnect (gimp_image_get_layers (image), handler);
  g_assert_true (undo == gimp_undo_stack_peek (gimp_image_get_undo_stack (image))); g_assert_true (redo == gimp_undo_stack_peek (gimp_image_get_redo_stack (image)));
  g_assert_cmpint (gimp_image_get_undo_group_count (image), ==, 0); g_assert_cmpint (gimp_container_get_n_children (gimp_image_get_layers (image)), ==, 1);
  g_assert_true (gimp_container_get_first_child (gimp_image_get_layers (image)) == GIMP_OBJECT (source)); g_assert_cmpfloat (gimp_layer_get_opacity (source), ==, .8);
  g_assert_true (gimp_image_redo (image)); g_assert_cmpfloat (gimp_layer_get_opacity (source), ==, .9);
  g_object_unref (cancel); g_object_unref (preset); gimp_context_set_image (gimp->user_context, NULL); g_object_unref (image);
}
static void rename_preset_added (GimpContainer *container, GimpObject *object, GimpLayerPreset *preset)
{
  gimp_object_set_name (GIMP_OBJECT (preset), "Renamed from insertion callback");
}
static void callback_rename_keeps_undo_name (void)
{
  GimpLayer *source; GimpImage *image = setup (0, &source); GError *error = NULL;
  GimpLayerPreset *preset = load ("duplicate-as-normal.json");
  gchar *original_name = g_strdup (gimp_object_get_name (preset));
  gulong handler = g_signal_connect (gimp_image_get_layers (image), "add", G_CALLBACK (rename_preset_added), preset);
  g_assert_true (gimp_layer_preset_apply (preset, gimp->user_context, source, &error)); g_assert_no_error (error);
  g_signal_handler_disconnect (gimp_image_get_layers (image), handler);
  g_assert_cmpstr (gimp_object_get_name (preset), ==, "Renamed from insertion callback");
  g_assert_cmpstr (gimp_object_get_name (gimp_undo_stack_peek (gimp_image_get_undo_stack (image))), ==, original_name);
  g_assert_cmpint (gimp_undo_stack_get_depth (gimp_image_get_undo_stack (image)), ==, 1);
  g_assert_true (gimp_image_undo (image)); g_assert_true (gimp_image_redo (image));
  g_free (original_name); g_object_unref (preset); gimp_context_set_image (gimp->user_context, NULL); g_object_unref (image);
}
typedef struct
{
  GimpImage *image;
  GimpLayer *source;
  GObject *parent;
  guint mutation;
  gboolean on_cleanup, armed, fired;
} PreflightMutation;
static void mutate_preflight_target (PreflightMutation *mutation)
{
  mutation->fired = TRUE;
  if (mutation->mutation == 0)
    {
      g_assert_true (gimp_image_reorder_item (mutation->image, GIMP_ITEM (mutation->source), NULL, 0, FALSE, NULL));
      gimp_image_remove_layer (mutation->image, GIMP_LAYER (mutation->parent), FALSE, NULL);
      /* The applier's lease must keep the now-unowned former parent alive. */
      g_assert_nonnull (mutation->parent);
    }
  else if (mutation->mutation == 1)
    g_assert_true (gimp_image_reorder_item (mutation->image, GIMP_ITEM (mutation->source), GIMP_ITEM (mutation->parent), 1, FALSE, NULL));
  else
    gimp_image_remove_layer (mutation->image, GIMP_LAYER (mutation->parent), FALSE, NULL);
}
static void mutate_on_probe_cleanup (gpointer data, GObject *object)
{ mutate_preflight_target (data); }
static gboolean mutate_on_probe_name (GSignalInvocationHint *hint, guint n_values, const GValue *values, gpointer data)
{
  PreflightMutation *mutation = data;
  GObject *object = g_value_get_object (&values[0]);
  if (!mutation->armed && g_strcmp0 (gimp_object_get_name (object), "Preflight probe") == 0)
    {
      mutation->armed = TRUE;
      if (mutation->on_cleanup) g_object_weak_ref (object, mutate_on_probe_cleanup, mutation);
      else mutate_preflight_target (mutation);
    }
  return TRUE;
}
static void preflight_rejects_changed_target (void)
{
  for (guint cleanup = 0; cleanup < 2; ++cleanup)
    for (guint operation = 0; operation < 3; ++operation)
      {
        GimpLayer *source; GimpImage *image = setup (0, &source); GError *error = NULL;
        GimpLayer *parent = gimp_group_layer_new (image);
        g_assert_true (gimp_image_add_layer (image, parent, NULL, 0, FALSE));
        g_assert_true (gimp_image_reorder_item (image, GIMP_ITEM (source), GIMP_ITEM (parent), 0, FALSE, NULL));
        if (operation == 1)
          {
            GimpLayer *sibling = gimp_layer_new (image, 10, 10, gimp_image_get_layer_format (image, TRUE), "sibling", 1, GIMP_LAYER_MODE_NORMAL);
            g_assert_true (gimp_image_add_layer (image, sibling, parent, 1, FALSE));
          }
        GimpLayerPreset *preset = from_json ("{\"version\":1,\"source-layer\":{\"type\":\"any\"},\"replacement-layer\":[{\"target\":\"new\",\"name\":\"Preflight probe\"}]}");
        PreflightMutation mutation = {image, source, G_OBJECT (parent), operation, cleanup, FALSE, FALSE};
        g_object_add_weak_pointer (mutation.parent, (gpointer *) &mutation.parent);
        guint signal = g_signal_lookup ("name-changed", GIMP_TYPE_OBJECT);
        gulong hook = g_signal_add_emission_hook (signal, 0, mutate_on_probe_name, &mutation, NULL);
        g_assert_false (gimp_layer_preset_apply (preset, gimp->user_context, source, &error));
        g_signal_remove_emission_hook (signal, hook);
        g_assert_true (mutation.fired); g_assert_nonnull (error);
        g_assert_nonnull (strstr (error->message, "source changed during preflight")); g_clear_error (&error);
        g_assert_cmpint (gimp_undo_stack_get_depth (gimp_image_get_undo_stack (image)), ==, 0);
        g_assert_cmpint (gimp_image_get_undo_group_count (image), ==, 0);
        if (operation == 1)
          {
            g_assert_true (mutation.parent == G_OBJECT (parent));
            g_assert_cmpint (gimp_container_get_child_index (gimp_viewable_get_children (GIMP_VIEWABLE (parent)), GIMP_OBJECT (source)), ==, 1);
            g_object_remove_weak_pointer (mutation.parent, (gpointer *) &mutation.parent);
          }
        else g_assert_null (mutation.parent);
        g_object_unref (preset); gimp_context_set_image (gimp->user_context, NULL); g_object_unref (image);
      }
}
static void ordered_source_edits (void)
{
  GimpLayer *source; GimpImage *image = setup (0, &source); GError *error = NULL;
  GimpLayerPreset *preset = from_json ("{\"version\":1,\"source-layer\":{\"type\":\"any\"},\"replacement-layer\":[{\"target\":\"source\",\"opacity\":0.25,\"boundary\":[2,3,12,13]},{\"target\":\"new\",\"source\":\"source\",\"boundary\":\"source\",\"opacity\":\"source\"},{\"target\":\"source\",\"opacity\":0.75}]}");
  g_assert_true (gimp_layer_preset_apply (preset, gimp->user_context, source, &error)); g_assert_no_error (error);
  GimpLayer *duplicate = GIMP_LAYER (gimp_container_get_first_child (gimp_image_get_layers (image)));
  g_assert_true (duplicate != source); g_assert_cmpfloat (gimp_layer_get_opacity (duplicate), ==, .25);
  g_assert_cmpint (gimp_item_get_width (GIMP_ITEM (duplicate)), ==, 10); g_assert_cmpint (gimp_item_get_offset_x (GIMP_ITEM (duplicate)), ==, 2);
  g_assert_cmpfloat (gimp_layer_get_opacity (source), ==, .75);
  g_assert_true (gimp_image_undo (image)); g_assert_cmpint (gimp_container_get_n_children (gimp_image_get_layers (image)), ==, 1);
  g_assert_cmpint (gimp_item_get_width (GIMP_ITEM (source)), ==, 40); g_assert_cmpfloat (gimp_layer_get_opacity (source), ==, .6);
  g_assert_true (gimp_image_redo (image)); g_assert_cmpfloat (gimp_layer_get_opacity (source), ==, .75);
  g_object_unref (preset); gimp_context_set_image (gimp->user_context, NULL); g_object_unref (image);
}
static void factory_paths (void)
{
  gchar *path = asset (""), *original = NULL;
  g_object_get (gimp->config, "layer-presets-path", &original, NULL);
  g_assert_true (gimp_get_data_factory (gimp, GIMP_TYPE_LAYER_PRESET) == gimp->layer_preset_factory);
  g_object_set (gimp->config, "layer-presets-path", path, NULL);
  gimp_data_factory_data_refresh (gimp->layer_preset_factory, gimp->user_context);
  GimpContainer *container = gimp_data_factory_get_container (gimp->layer_preset_factory);
  g_assert_cmpint (gimp_container_get_n_children (container), ==, 8);
  gimp_data_factory_data_refresh (gimp->layer_preset_factory, gimp->user_context);
  g_assert_cmpint (gimp_container_get_n_children (container), ==, 8); g_free (path);
  g_object_set (gimp->config, "layer-presets-path", original, NULL); g_free (original);
}
static void save_reopen_editable (void)
{
  for (guint id = 3; id <= 7; id += 4)
    {
      GimpLayer *source; GimpImage *image = setup (id, &source), *copy; GError *error = NULL; GimpLayerPreset *preset = load (names[id]);
      g_assert_true (gimp_layer_preset_apply (preset, gimp->user_context, source, &error)); g_assert_no_error (error);
      GOutputStream *output = g_memory_output_stream_new_resizable (); GFile *file = g_file_new_for_path ("/tmp/layer-preset-roundtrip.xcf");
      g_assert_true (xcf_save_stream (gimp, image, output, file, NULL, &error)); g_assert_no_error (error);
      g_output_stream_close (output, NULL, &error); g_assert_no_error (error);
      GBytes *bytes = g_memory_output_stream_steal_as_bytes (G_MEMORY_OUTPUT_STREAM (output)); GInputStream *input = g_memory_input_stream_new_from_bytes (bytes);
      copy = xcf_load_stream (gimp, input, file, NULL, &error); g_assert_no_error (error); g_assert_nonnull (copy);
      GimpLayer *group = GIMP_LAYER (gimp_container_get_first_child (gimp_image_get_layers (copy)));
      g_assert_true (GIMP_IS_GROUP_LAYER (group)); GimpContainer *children = gimp_viewable_get_children (GIMP_VIEWABLE (group));
      GimpLayer *first = GIMP_LAYER (gimp_container_get_first_child (children)), *last = GIMP_LAYER (gimp_container_get_last_child (children));
      if (id == 3) { g_assert_true (GIMP_IS_CLONE_LAYER (first)); g_assert_true (gimp_clone_layer_get_source (GIMP_CLONE_LAYER (first)) == last); }
      else { g_assert_true (GIMP_IS_FILTER_LAYER (first)); gchar *proc = gimp_filter_layer_dup_procedure (GIMP_FILTER_LAYER (first)); g_assert_cmpstr (proc, ==, "plug-in-edge"); g_free (proc); }
      GeglColor *color = gegl_color_new ("rgba(0.2,0.7,0.4,0.8)"); gegl_buffer_set_color (gimp_drawable_get_buffer (GIMP_DRAWABLE (last)), NULL, color); g_object_unref (color);
      gimp_drawable_update (GIMP_DRAWABLE (last), 0, 0, -1, -1);
      if (id == 3) { g_assert_true (gimp_clone_layer_get_source (GIMP_CLONE_LAYER (first)) == last); }
      else {
        gint64 until = g_get_monotonic_time () + 10 * G_USEC_PER_SEC;
        while (gimp_filter_layer_get_state (GIMP_FILTER_LAYER (first)) != GIMP_FILTER_LAYER_CLEAN && g_get_monotonic_time () < until)
          { g_main_context_iteration (NULL, FALSE); g_usleep (1000); }
        g_assert_cmpint (gimp_filter_layer_get_state (GIMP_FILTER_LAYER (first)), ==, GIMP_FILTER_LAYER_CLEAN);
        g_assert_cmpuint (gimp_filter_layer_get_run_count (GIMP_FILTER_LAYER (first)), >, 0);
      }
      g_object_unref (input); g_bytes_unref (bytes); g_object_unref (output); g_object_unref (file); g_object_unref (copy);
      g_object_unref (preset); gimp_context_set_image (gimp->user_context, NULL); g_object_unref (image);
    }
}

static void malformed_resource_loads (void)
{
  const gchar *inputs[] = {"", "[1,2]", "null", "{", "{\"name\":}"};
  for (guint i = 0; i < G_N_ELEMENTS (inputs); ++i)
    {
      GInputStream *stream = g_memory_input_stream_new_from_data (inputs[i], -1, NULL);
      GFile *file = g_file_new_for_path ("/tmp/preset-malformed.json"); GError *error = NULL;
      GList *list = gimp_layer_preset_load (gimp->user_context, file, stream, &error);
      g_assert_null (list); g_assert_nonnull (error); g_clear_error (&error); g_object_unref (stream); g_object_unref (file);
    }
}
static void unknown_filter_stays_inert (void)
{
  GimpLayer *source; GimpImage *image = setup (0, &source); GError *error = NULL;
  GimpLayerPreset *preset = from_json ("{\"version\":1,\"source-layer\":{\"type\":\"any\"},\"replacement-layer\":[{\"target\":\"new\",\"type\":\"filter\",\"filter\":{\"name\":\"script-fu-untrusted-preset\",\"unknown\":{\"retain\":true},\"args\":[{\"type\":\"STRING\",\"value\":\"do not execute\"}]}},{\"target\":\"source\"}]}");
  g_assert_true (gimp_layer_preset_apply (preset, gimp->user_context, source, &error)); g_assert_no_error (error);
  GimpFilterLayer *filter = GIMP_FILTER_LAYER (gimp_container_get_first_child (gimp_image_get_layers (image)));
  gint64 until = g_get_monotonic_time () + 10 * G_USEC_PER_SEC;
  while (gimp_filter_layer_get_state (filter) != GIMP_FILTER_LAYER_FAILED && g_get_monotonic_time () < until)
    { g_main_context_iteration (NULL, FALSE); g_usleep (1000); }
  g_assert_cmpint (gimp_filter_layer_get_state (filter), ==, GIMP_FILTER_LAYER_FAILED);
  g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 0);
  GBytes *bytes = gimp_filter_layer_ref_definition (filter); gsize size; const char *data = g_bytes_get_data (bytes, &size);
  gchar *json = g_strndup (data, size); JsonNode *node = json_from_string (json, &error); g_assert_no_error (error);
  g_assert_true (json_object_has_member (json_node_get_object (node), "unknown"));
  g_free (json); json_node_unref (node); g_bytes_unref (bytes);
  g_object_unref (preset); gimp_context_set_image (gimp->user_context, NULL); g_object_unref (image);
}

int main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);
  gimp_test_utils_set_gimp3_directory ("GIMP_TESTING_ABS_TOP_SRCDIR", "app/tests/gimpdir"); gimp = gimp_init_for_testing ();
#define ADD(name) g_test_add_func ("/layer-presets/" #name, name)
  ADD (malformed_resource_loads); ADD (unknown_filter_stays_inert); ADD (all_bundled_construction); ADD (resource_roundtrip); ADD (reject_without_history_loss); ADD (rollback_after_insert); ADD (callback_rename_keeps_undo_name); ADD (preflight_rejects_changed_target); ADD (ordered_source_edits); ADD (factory_paths); ADD (save_reopen_editable);
  int result = g_test_run ();
  gimp_test_utils_set_gimp3_directory ("GIMP_TESTING_ABS_TOP_BUILDDIR", "app/tests/gimpdir-output"); gimp_exit (gimp, TRUE); return result;
}
