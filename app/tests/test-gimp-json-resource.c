/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gegl.h>
#include <gtk/gtk.h>
#include "widgets/widgets-types.h"
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <json-glib/json-glib.h>
#include "core/core-types.h"
#include "core/gimp.h"
#include "core/gimpcontext.h"
#include "core/gimplayerpreset.h"
#include "presets/gimpjsonresource.h"
#include "tests.h"
#include "gimp-app-test-utils.h"
typedef struct { GimpJsonResource parent; } TestJsonResource;
typedef struct { GimpJsonResourceClass parent; } TestJsonResourceClass;
static guint prepare_mode;
G_DEFINE_TYPE (TestJsonResource, test_json_resource, GIMP_TYPE_JSON_RESOURCE)
static gboolean test_prepare (GimpJsonResource *resource, JsonNode *root, JsonNode **owned)
{
  *owned = root ? json_node_ref (root) : NULL;
  if (prepare_mode == 2) g_object_run_dispose (G_OBJECT (resource));
  if (prepare_mode == 3) g_object_unref (resource);
  return prepare_mode != 1 && prepare_mode != 3;
}
static void test_json_resource_class_init (TestJsonResourceClass *klass)
{ GIMP_JSON_RESOURCE_CLASS (klass)->prepare_root = test_prepare; }
static void test_json_resource_init (TestJsonResource *resource) { (void) resource; }
static Gimp *gimp;
void json_resource_handle_test (void);
static void identity (void)
{
  GimpData *(*factory)(GimpContext*,const gchar*)=gimp_json_resource_new;
  GType (*type)(void)=gimp_json_resource_get_type;
  GimpData *data=factory(gimp_get_user_context(gimp),"generic");
  GParamSpec *spec=g_object_class_find_property(G_OBJECT_GET_CLASS(data),"root");
  g_assert_cmpstr(g_type_name(type()),==,"GimpJsonResource");
  g_assert_cmpuint(g_type_parent(type()),==,GIMP_TYPE_DATA);
  g_assert_true(GIMP_IS_JSON_RESOURCE(data));g_assert_false(GIMP_IS_LAYER_PRESET(data));
  g_assert_false(g_object_is_floating(data));g_assert_cmpstr(gimp_data_get_mime_type(data),==,"application/json");
  g_assert_cmpstr(gimp_data_get_extension(data),==,".json");
  g_assert_cmpuint(spec->value_type,==,JSON_TYPE_NODE);
  g_assert_true((spec->flags&(G_PARAM_READWRITE|G_PARAM_CONSTRUCT))==(G_PARAM_READWRITE|G_PARAM_CONSTRUCT));
  g_assert_null(gimp_json_resource_ref_json(GIMP_JSON_RESOURCE(data)));
  g_object_unref(data);
}
static void arbitrary (void)
{
  const gchar *documents[]={"[1,\"two\",null]","\"text\"","42","true","null","{\"unknown\":{\"nested\":[3]}}"};
  for(guint i=0;i<G_N_ELEMENTS(documents);i++) {
    JsonNode *root=json_from_string(documents[i],NULL),*returned=NULL;
    GimpJsonResource *r=g_object_new(GIMP_TYPE_JSON_RESOURCE,"root",root,NULL);
    gchar *expected=json_to_string(root,FALSE),*actual;
    json_node_unref(root);g_object_get(r,"root",&returned,NULL);g_assert_nonnull(returned);
    actual=json_to_string(returned,FALSE);g_assert_cmpstr(actual,==,expected);g_free(actual);g_free(expected);
    g_object_unref(r);actual=json_to_string(returned,FALSE);g_assert_nonnull(actual);g_free(actual);json_node_unref(returned);
  }
}
static void save_and_copy (void)
{
  GimpData *data=gimp_json_resource_new(NULL,"not-in-json");
  GOutputStream *out=g_memory_output_stream_new_resizable();GError *error=NULL;
  g_assert_false(GIMP_DATA_GET_CLASS(data)->save(data,out,&error));g_assert_error(error,GIMP_DATA_ERROR,GIMP_DATA_ERROR_WRITE);g_clear_error(&error);
  JsonNode *node=json_from_string("{\"unknown\":[1,2],\"name\":\"kept\"}",NULL);
  g_assert_true(gimp_json_resource_set_json(GIMP_JSON_RESOURCE(data),node));
  GimpData *copy=gimp_data_duplicate(data);g_assert_nonnull(copy);g_assert_true(GIMP_IS_JSON_RESOURCE(copy));
  JsonNode *cloned=gimp_json_resource_ref_json(GIMP_JSON_RESOURCE(copy));
  g_assert_true(json_node_get_object(node)==json_node_get_object(cloned)); /* Original shallow node duplicate. */
  g_assert_true(GIMP_DATA_GET_CLASS(data)->save(data,out,&error));g_assert_no_error(error);
  g_assert_true(g_output_stream_close(out,NULL,&error));g_assert_no_error(error);
  JsonParser *parser=json_parser_new();g_assert_true(json_parser_load_from_data(parser,g_memory_output_stream_get_data(G_MEMORY_OUTPUT_STREAM(out)),g_memory_output_stream_get_data_size(G_MEMORY_OUTPUT_STREAM(out)),&error));g_assert_no_error(error);
  g_assert_cmpstr(json_object_get_string_member(json_node_get_object(json_parser_get_root(parser)),"name"),==,"kept");
  json_node_unref(cloned);json_node_unref(node);g_object_unref(parser);g_object_unref(copy);g_object_unref(data);g_object_unref(out);
}
static void count_finalized(gpointer value,GObject *object){(void)object;(*(guint*)value)++;}
static void release_on_root(GObject *object,GParamSpec *spec,gpointer data){(void)spec;g_signal_handlers_disconnect_by_func(object,release_on_root,data);g_object_unref(object);}
static void dispose_on_root(GObject *object,GParamSpec *spec,gpointer data){(void)spec;(void)data;g_object_run_dispose(object);}
static void owner_reentry (void)
{
  guint finalized=0;JsonNode *node=json_from_string("[1]",NULL);
  GimpData *data=gimp_json_resource_new(NULL,"leased");g_object_weak_ref(G_OBJECT(data),count_finalized,&finalized);
  g_signal_connect(data,"notify::root",G_CALLBACK(release_on_root),NULL);
  g_assert_true(gimp_json_resource_set_json(GIMP_JSON_RESOURCE(data),node));g_assert_cmpuint(finalized,==,1);
  data=gimp_json_resource_new(NULL,"disposed");g_signal_connect(data,"notify::root",G_CALLBACK(dispose_on_root),NULL);
  g_assert_false(gimp_json_resource_set_json(GIMP_JSON_RESOURCE(data),node));
  g_assert_false(gimp_json_resource_is_live(GIMP_JSON_RESOURCE(data)));g_assert_null(gimp_json_resource_ref_json(GIMP_JSON_RESOURCE(data)));
  g_assert_false(gimp_json_resource_set_json(GIMP_JSON_RESOURCE(data),node));g_object_run_dispose(G_OBJECT(data));g_object_unref(data);json_node_unref(node);
}
static void subclass_failures (void)
{
  JsonNode *node = json_from_string ("[1]", NULL);
  for (guint mode = 1; mode <= 3; mode++)
    {
      guint finalized = 0;
      prepare_mode = 0;
      GimpJsonResource *resource = g_object_new (test_json_resource_get_type (), NULL);
      g_object_weak_ref (G_OBJECT (resource), count_finalized, &finalized);
      prepare_mode = mode;
      g_assert_false (gimp_json_resource_set_json (resource, node));
      if (mode != 3)
        {
          g_assert_null (gimp_json_resource_ref_json (resource));
          g_assert_cmpint (gimp_json_resource_is_live (resource), ==, mode != 2);
          g_object_unref (resource);
        }
      g_assert_cmpuint (finalized, ==, 1);
    }
  prepare_mode = 0;
  json_node_unref (node);
}
static void replace_on_root(GObject*object,GParamSpec*spec,gpointer data){(void)spec;g_signal_handlers_disconnect_by_func(object,replace_on_root,data);g_assert_true(gimp_layer_preset_set_json(GIMP_LAYER_PRESET(object),data,NULL));}
static void specialized (void)
{
  GimpData *data=gimp_layer_preset_new(NULL,"preset");GimpLayerPreset *preset=GIMP_LAYER_PRESET(data);
  g_assert_true(GIMP_IS_JSON_RESOURCE(data));JsonNode *root=gimp_layer_preset_dup_json(preset);g_assert_true(JSON_NODE_HOLDS_OBJECT(root));json_node_unref(root);
  g_assert_false(gimp_json_resource_set_json(GIMP_JSON_RESOURCE(data),NULL));
  root=gimp_layer_preset_dup_json(preset);g_assert_true(JSON_NODE_HOLDS_OBJECT(root));json_node_unref(root);
  JsonNode *array=json_from_string("[1]",NULL);g_assert_false(gimp_json_resource_set_json(GIMP_JSON_RESOURCE(data),array));json_node_unref(array);
  JsonNode *first=json_from_string("{\"name\":\"first\",\"nested\":{\"value\":1}}",NULL),*second=json_from_string("{\"name\":\"second\",\"nested\":{\"value\":2}}",NULL);
  g_signal_connect(data,"notify::root",G_CALLBACK(replace_on_root),second);
  g_assert_true(gimp_layer_preset_set_json(preset,first,NULL));g_assert_cmpstr(gimp_object_get_name(data),==,"second");
  json_object_set_int_member(json_object_get_object_member(json_node_get_object(second),"nested"),"value",99);
  root=gimp_layer_preset_dup_json(preset);g_assert_cmpint(json_object_get_int_member(json_object_get_object_member(json_node_get_object(root),"nested"),"value"),==,2);
  json_node_unref(root);json_node_unref(first);json_node_unref(second);g_object_unref(data);
}
int main(int argc,char**argv)
{
  int result;g_test_init(&argc,&argv,NULL);gimp_test_utils_set_gimp3_directory("GIMP_TESTING_ABS_TOP_SRCDIR","app/tests/gimpdir");gimp=gimp_init_for_testing();
  g_test_add_func("/json-resource/identity",identity);g_test_add_func("/json-resource/arbitrary",arbitrary);g_test_add_func("/json-resource/save-copy",save_and_copy);g_test_add_func("/json-resource/owner-reentry",owner_reentry);g_test_add_func("/json-resource/specialized",specialized);g_test_add_func("/json-resource/handle",json_resource_handle_test);
  g_test_add_func("/json-resource/subclass-failures",subclass_failures);
  result=g_test_run();gimp_test_utils_set_gimp3_directory("GIMP_TESTING_ABS_TOP_BUILDDIR","app/tests/gimpdir-output");gimp_exit(gimp,TRUE);return result;
}
