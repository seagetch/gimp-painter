/* SPDX-License-Identifier: GPL-3.0-or-later
 * One native GimpData owner for arbitrary JSON and specialized layer presets. */
#include "config.h"
#include <gegl.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <string.h>
#include <json-glib/json-glib.h>
#include "core-types.h"
#include "gimpjsonresource.h"

typedef struct { JsonNode *root; gboolean disposed; } GimpJsonResourcePrivate;
enum { PROP_0, PROP_ROOT };
G_DEFINE_TYPE_WITH_PRIVATE (GimpJsonResource, gimp_json_resource, GIMP_TYPE_DATA)

static void dispose (GObject *object)
{
  GimpJsonResourcePrivate *private = gimp_json_resource_get_instance_private (GIMP_JSON_RESOURCE (object));
  private->disposed = TRUE;
  g_clear_pointer (&private->root, json_node_unref);
  G_OBJECT_CLASS (gimp_json_resource_parent_class)->dispose (object);
}
gboolean gimp_json_resource_set_json (GimpJsonResource *resource, JsonNode *node)
{
  GimpJsonResourcePrivate *private;
  gboolean live;
  JsonNode *owned = NULL;
  GimpJsonResourceClass *klass;
  g_return_val_if_fail (GIMP_IS_JSON_RESOURCE (resource), FALSE);
  private = gimp_json_resource_get_instance_private (resource);
  if (private->disposed) return FALSE;
  if (private->root == node) return TRUE;
  klass = GIMP_JSON_RESOURCE_GET_CLASS (resource);
  /* Subclass normalization and observers may dispose/release the owner. */
  g_object_ref (resource);
  if (!klass->prepare_root (resource, node, &owned) || private->disposed)
    {
      if (owned) json_node_unref (owned);
      g_object_unref (resource);
      return FALSE;
    }
  g_clear_pointer (&private->root, json_node_unref);
  private->root = owned;
  /* Pin the selected root across metadata notifications that can reenter. */
  if (owned) json_node_ref (owned);
  if (klass->root_changed) klass->root_changed (resource, owned);
  if (owned) json_node_unref (owned);
  if (!private->disposed) g_object_notify (G_OBJECT (resource), "root");
  live = !private->disposed;
  g_object_unref (resource);
  return live;
}
gboolean gimp_json_resource_is_live (GimpJsonResource *resource)
{
  GimpJsonResourcePrivate *private;
  g_return_val_if_fail (GIMP_IS_JSON_RESOURCE (resource), FALSE);
  private = gimp_json_resource_get_instance_private (resource);
  return !private->disposed;
}
JsonNode *gimp_json_resource_ref_json (GimpJsonResource *resource)
{
  GimpJsonResourcePrivate *private;
  g_return_val_if_fail (GIMP_IS_JSON_RESOURCE (resource), NULL);
  private = gimp_json_resource_get_instance_private (resource);
  return private->root ? json_node_ref (private->root) : NULL;
}
static void set_property (GObject *object, guint id, const GValue *value, GParamSpec *pspec)
{
  if (id == PROP_ROOT)
    gimp_json_resource_set_json (GIMP_JSON_RESOURCE (object), g_value_get_boxed (value));
  else G_OBJECT_WARN_INVALID_PROPERTY_ID (object, id, pspec);
}
static void get_property (GObject *object, guint id, GValue *value, GParamSpec *pspec)
{
  if (id == PROP_ROOT)
    g_value_take_boxed (value, gimp_json_resource_ref_json (GIMP_JSON_RESOURCE (object)));
  else G_OBJECT_WARN_INVALID_PROPERTY_ID (object, id, pspec);
}
static gboolean save (GimpData *data, GOutputStream *stream, GError **error)
{
  JsonNode *root = gimp_json_resource_ref_json (GIMP_JSON_RESOURCE (data));
  gchar *text;
  gboolean result;
  if (!root)
    {
      g_set_error_literal (error, GIMP_DATA_ERROR, GIMP_DATA_ERROR_WRITE,
                           "JSON resource has no document");
      return FALSE;
    }
  text = json_to_string (root, TRUE);
  json_node_unref (root);
  result = g_output_stream_write_all (stream, text, strlen (text), NULL, NULL, error);
  g_free (text);
  return result;
}
static void copy (GimpData *data, GimpData *source)
{
  JsonNode *root = gimp_json_resource_ref_json (GIMP_JSON_RESOURCE (source));
  JsonNode *copy = root ? json_node_copy (root) : NULL;
  gimp_json_resource_set_json (GIMP_JSON_RESOURCE (data), copy);
  if (copy) json_node_unref (copy);
  if (root) json_node_unref (root);
}
static gboolean prepare_root (GimpJsonResource *resource, JsonNode *root, JsonNode **owned)
{
  (void) resource;
  *owned = root ? json_node_ref (root) : NULL;
  return TRUE;
}
static const gchar *extension (GimpData *data) { (void) data; return ".json"; }
static void gimp_json_resource_class_init (GimpJsonResourceClass *klass)
{
  GObjectClass *object = G_OBJECT_CLASS (klass);
  klass->prepare_root = prepare_root;
  object->dispose = dispose;
  object->set_property = set_property;
  object->get_property = get_property;
  g_object_class_install_property (object, PROP_ROOT,
    g_param_spec_boxed ("root", NULL, NULL, JSON_TYPE_NODE,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));
  GIMP_DATA_CLASS (klass)->save = save;
  GIMP_DATA_CLASS (klass)->copy = copy;
  GIMP_DATA_CLASS (klass)->get_extension = extension;
}
static void gimp_json_resource_init (GimpJsonResource *resource) { (void) resource; }
GimpData *gimp_json_resource_new (GimpContext *context, const gchar *name)
{
  (void) context;
  return GIMP_DATA (g_object_new (GIMP_TYPE_JSON_RESOURCE,
                                  "name", name, "mime-type", "application/json", NULL));
}
