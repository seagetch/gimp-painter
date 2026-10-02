/* SPDX-License-Identifier: GPL-3.0-or-later
 * Native data resource. JSON is retained verbatim semantically, never rebuilt
 * from only the currently understood keys. Schema validation belongs to apply.
 */
#include "config.h"
#include <gegl.h>
#include <json-glib/json-glib.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <string.h>
#include "core-types.h"
#include "gimplayerpreset.h"
#include "gimpcontext.h"

G_DEFINE_TYPE (GimpLayerPreset, gimp_layer_preset, GIMP_TYPE_DATA)
/* json_node_copy() shares JsonObject/JsonArray values. Resource snapshots and
 * duplicates must be independently editable, including unknown nested fields. */
static JsonNode *deep_copy (JsonNode *node)
{
  gchar *text;
  JsonNode *copy;
  if (!node) return NULL;
  text = json_to_string (node, FALSE);
  copy = json_from_string (text, NULL);
  g_free (text);
  return copy;
}

static void finalize (GObject *object)
{
  g_clear_pointer (&GIMP_LAYER_PRESET (object)->root, json_node_unref);
  G_OBJECT_CLASS (gimp_layer_preset_parent_class)->finalize (object);
}
static gboolean save (GimpData *data, GOutputStream *output, GError **error)
{
  JsonGenerator *generator = json_generator_new ();
  JsonNode *snapshot = deep_copy (GIMP_LAYER_PRESET (data)->root);
  gchar *text;
  gsize length;
  gboolean result;
  if (!snapshot)
    {
      g_set_error_literal (error, GIMP_DATA_ERROR, GIMP_DATA_ERROR_WRITE,
                           "Layer preset has no valid JSON document");
      g_object_unref (generator);
      return FALSE;
    }
  if (gimp_object_get_name (data))
    json_object_set_string_member (json_node_get_object (snapshot), "name", gimp_object_get_name (data));
  json_generator_set_root (generator, snapshot);
  json_generator_set_pretty (generator, TRUE);
  text = json_generator_to_data (generator, &length);
  result = g_output_stream_write_all (output, text, length, NULL, NULL, error);
  g_free (text);
  g_object_unref (generator);
  json_node_unref (snapshot);
  return result;
}
static const gchar *extension (GimpData *data) { return ".json"; }
static void copy (GimpData *data, GimpData *source)
{ gimp_layer_preset_set_json (GIMP_LAYER_PRESET (data), GIMP_LAYER_PRESET (source)->root, NULL); }
static void gimp_layer_preset_class_init (GimpLayerPresetClass *klass)
{
  G_OBJECT_CLASS (klass)->finalize = finalize;
  GIMP_DATA_CLASS (klass)->save = save;
  GIMP_DATA_CLASS (klass)->get_extension = extension;
  GIMP_DATA_CLASS (klass)->copy = copy;
  GIMP_VIEWABLE_CLASS (klass)->default_icon_name = "gimp-layer";
}
static void gimp_layer_preset_init (GimpLayerPreset *preset) {}
GimpData *gimp_layer_preset_new (GimpContext *context, const gchar *name)
{
  GimpLayerPreset *preset = g_object_new (GIMP_TYPE_LAYER_PRESET, "name", name,
                                         "mime-type", "application/json", NULL);
  JsonParser *parser = json_parser_new ();
  json_parser_load_from_data (parser, "{\"version\":1,\"source-layer\":{\"type\":\"any\"},\"replacement-layer\":[]}", -1, NULL);
  preset->root = deep_copy (json_parser_get_root (parser));
  g_object_unref (parser);
  return GIMP_DATA (preset);
}
gboolean gimp_layer_preset_set_json (GimpLayerPreset *preset, JsonNode *root, GError **error)
{
  JsonNode *copy;
  g_return_val_if_fail (GIMP_IS_LAYER_PRESET (preset), FALSE);
  if (!root || !JSON_NODE_HOLDS_OBJECT (root))
    {
      g_set_error_literal (error, GIMP_DATA_ERROR, GIMP_DATA_ERROR_READ,
                           "Layer preset root must be an object");
      return FALSE;
    }
  copy = deep_copy (root);
  if (!copy)
    {
      g_set_error_literal (error, GIMP_DATA_ERROR, GIMP_DATA_ERROR_READ,
                           "Layer preset is not valid JSON");
      return FALSE;
    }
  /* Name/dirty observers may release their last reference during notification. */
  g_object_ref (preset);
  g_clear_pointer (&preset->root, json_node_unref);
  preset->root = copy;
  {
    JsonNode *name = json_object_get_member (json_node_get_object (copy), "name");
    if (name && json_node_get_value_type (name) == G_TYPE_STRING)
      gimp_object_set_name (GIMP_OBJECT (preset), json_node_get_string (name));
  }
  gimp_data_dirty (GIMP_DATA (preset));
  g_object_unref (preset);
  return TRUE;
}
JsonNode *gimp_layer_preset_dup_json (GimpLayerPreset *preset)
{
  g_return_val_if_fail (GIMP_IS_LAYER_PRESET (preset), NULL);
  return deep_copy (preset->root);
}
GList *gimp_layer_preset_load (GimpContext *context, GFile *file,
                               GInputStream *input, GError **error)
{
  JsonParser *parser = json_parser_new ();
  GimpData *data = NULL;
  /* A bounded resource read prevents a hostile preset from consuming unlimited
   * parser memory. Unknown valid JSON values remain part of the saved resource. */
  GByteArray *bytes = g_byte_array_new ();
  guchar chunk[8192];
  gssize n;
  while ((n = g_input_stream_read (input, chunk, sizeof chunk, NULL, error)) > 0)
    {
      if (bytes->len + n > 4 * 1024 * 1024)
        {
          g_set_error_literal (error, GIMP_DATA_ERROR, GIMP_DATA_ERROR_READ,
                               "Layer preset exceeds 4 MiB");
          n = -1;
          break;
        }
      g_byte_array_append (bytes, chunk, n);
    }
  if (n >= 0 && json_parser_load_from_data (parser, (const gchar *) (bytes->data ? bytes->data : (const guchar *) ""), bytes->len, error))
    {
      JsonNode *root = json_parser_get_root (parser);
      gchar *basename = g_file_get_basename (file);
      data = gimp_layer_preset_new (context, basename);
      g_free (basename);
      if (!gimp_layer_preset_set_json (GIMP_LAYER_PRESET (data), root, error))
        g_clear_object (&data);
      else
        {
          JsonObject *object = json_node_get_object (root);
          JsonNode *name = json_object_get_member (object, "name");
          if (name && json_node_get_value_type (name) == G_TYPE_STRING)
            gimp_object_set_name (GIMP_OBJECT (data), json_node_get_string (name));
          gimp_data_clean (data);
        }
    }
  g_byte_array_unref (bytes);
  g_object_unref (parser);
  return data ? g_list_prepend (NULL, data) : NULL;
}
