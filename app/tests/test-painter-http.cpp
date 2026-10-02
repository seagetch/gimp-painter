/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gegl.h>
#include <gtk/gtk.h>
#include <libsoup/soup.h>
#include <string>
extern "C"
{
#include "libgimpbase/gimpbase.h"
#include "libgimpcolor/gimpcolor.h"
#include "core/core-types.h"
#include "widgets/widgets-types.h"
#include "core/gimp.h"
#include "core/gimpclonelayer.h"
#include "core/gimpcontainer.h"
#include "core/gimpcontext.h"
#include "core/gimpdrawable.h"
#include "core/gimpdrawablefilter.h"
#include "core/gimpresource.h"
#include "core/gimpbrush.h"
#include "core/gimpdata.h"
#include "core/gimpfilterlayer.h"
#include "core/gimpgrouplayer.h"
#include "core/gimpimage.h"
#include "core/gimpitem.h"
#include "core/gimplayer-new.h"
#include "core/gimplayer.h"
#include "gimp-app-test-utils.h"
#include "httpd/httpd.h"
#include "pdb/gimppdb.h"
#include "pdb/gimpprocedure.h"
#include "pdb/pdb-types.h"
#include "tests.h"
}
#include "httpd/httpd-private.hpp"
#include "httpd/httpd-resource.hpp"
#include "painter/object-ref.hpp"
using namespace GimpPainter;
using namespace GimpPainter::Http;
static Gimp       *gimp;
static const char *token = "test-only-random-token-0123456789abcdef";
static Response
call (Router &r, const char *method, const std::string &path,
      const std::string &body = "", const char *type = "application/json")
{
  return r.dispatch (gimp, { method, path, type, body });
}
static Json
successful (Response r, unsigned code = 200)
{
  if (r.status != code)
    g_error ("HTTP %u (expected %u): %s", r.status, code, r.body.c_str ());
  return parse (r.body);
}
static ObjectRef<GObject>
image ()
{
  return ObjectRef<GObject>::adopt (G_OBJECT (
      gimp_image_new (gimp, 32, 24, GIMP_RGB, GIMP_PRECISION_U8_NON_LINEAR)));
}
static std::string
path (GObject *o)
{
  return "/api/v1/images/" + std::to_string (gimp_image_get_id (GIMP_IMAGE (o)));
}
static GimpLayer *
layer (GimpImage *i, const char *name, GimpLayer *parent = nullptr)
{
  auto *l = gimp_layer_new (i, 8, 9, babl_format ("R'G'B'A u8"), name, .7,
                            GIMP_LAYER_MODE_PAINTER_NORMAL);
  g_assert_true (gimp_image_add_layer (i, l, parent, 0, FALSE));
  return l;
}
static void
routing ()
{
  Router r;
  g_assert_true (call (r, "GET", "/Painter").body == "Hello, Painter");
  g_assert_cmpuint (call (r, "GET", "/not/a/route").status, ==, 404);
  for (const char *method : { "POST", "DELETE", "PATCH", "OPTIONS" })
    g_assert_cmpuint (call (r, method, "/api/v1/images/").status, ==, 405);
  g_assert_cmpuint (call (r, "PUT", "/api/v1/pdb/").status, ==, 405);
  g_assert_cmpuint (call (r, "GET", "/api/v1/navigation").status, ==, 405);
  g_assert_cmpuint (call (r, "POST", "/api/v1/navigation").status, ==, 503);
}
static void
hierarchy ()
{
  Router r;
  auto   i     = image ();
  auto  *im    = GIMP_IMAGE (i.get ());
  auto  *group = gimp_group_layer_new (im);
  gimp_object_set_name (GIMP_OBJECT (group), "Group");
  g_assert_true (gimp_image_add_layer (im, group, nullptr, 0, FALSE));
  auto *src = layer (im, "Ink", group);
  auto *clone = gimp_clone_layer_new (im, src, 8, 9, "Clone", 1, GIMP_LAYER_MODE_PAINTER_NORMAL);
  g_assert_true (gimp_image_add_layer (im, clone, group, 0, FALSE));
  auto *filter = gimp_filter_layer_new (im, 8, 9, "Filter", 1, GIMP_LAYER_MODE_PAINTER_NORMAL);
  g_assert_true (gimp_image_add_layer (im, filter, group, 0, FALSE));
  auto root = successful (call (r, "GET", "/api/v1/images/"));
  g_assert_nonnull (member (root.get (), std::to_string (gimp_image_get_id (im)).c_str ()));
  auto info = successful (call (r, "GET", path (i.get ()) + "/Group/#info"));
  g_assert_true (text (member (info.get (), "type")) == "group");
  g_assert_cmpuint (json_array_get_length (json_node_get_array (member (info.get (), "children"))),
                    ==, 3);
  for (const char *name : { "Clone", "Filter", "Ink" })
    {
      auto out = successful (call (r, "GET", path (i.get ()) + "/Group/" + name));
      g_assert_true (text (member (out.get (), "type")) ==
                     (std::string (name) == "Clone"  ? "clone" :
                      std::string (name) == "Filter" ? "filter" :
                                                       "normal"));
    }
  auto out = successful (call (
      r, "GET",
      path (i.get ()) + "/" + std::to_string (gimp_item_get_id (GIMP_ITEM (group))) +
          "/" + std::to_string (gimp_item_get_id (GIMP_ITEM (clone)))));
  g_assert_cmpint (integer (member (out.get (), "source")), ==,
                   gimp_item_get_id (GIMP_ITEM (src)));
  g_assert_cmpuint (call (r, "GET", path (i.get ()) + "/Group/Ink/absent").status, ==, 404);
  g_assert_cmpuint (call (r, "GET", path (i.get ()) + "/#unknown").status, ==, 404);
}
static void
create_and_pixels ()
{
  Router r;
  auto result = successful (call (r, "PUT", "/api/v1/images/", R"({"boundary":[0,0,20,14]})"),
                            201);
  auto id   = integer (member (result.get (), "image"));
  auto base = "/api/v1/images/" + std::to_string (id);
  auto created = successful (call (r, "PUT", base, R"({"name":"Remote ink","type":"normal","boundary":[3,4,11,13],"opacity":0.4,"mode":"multiply-mode","visible":false})"),
                             201);
  auto  lid = integer (member (created.get (), "layer"));
  auto *l   = GIMP_LAYER (gimp_item_get_by_id (gimp, lid));
  g_assert_cmpint (gimp_item_get_offset_x (GIMP_ITEM (l)), ==, 3);
  g_assert_cmpstr (gimp_object_get_name (l), ==, "Remote ink");
  g_assert_false (gimp_item_get_visible (GIMP_ITEM (l)));
  g_assert_cmpfloat (gimp_layer_get_opacity (l), ==, .4);
  g_assert_cmpint (gimp_layer_get_mode (l), ==, GIMP_LAYER_MODE_PAINTER_MULTIPLY);
  auto png = call (r, "GET", base + "/Remote ink/#data");
  g_assert_cmpuint (png.status, ==, 200);
  g_assert_cmpstr (png.content_type.c_str (), ==, "image/png");
  g_assert_cmpuint (png.body.size (), >, 8);
  g_assert_true (png.body.compare (1, 3, "PNG") == 0);
  auto jpeg = call (r, "GET", base + "/Remote ink/#preview");
  g_assert_cmpuint (jpeg.status, ==, 200);
  g_assert_cmpstr (jpeg.content_type.c_str (), ==, "image/jpeg");
  successful (call (r, "PUT", base + "/#data", png.body, "image/png"), 201);
  g_assert_cmpuint (call (r, "PUT", base, R"({"name":"bad","type":"normal","boundary":[0,0,100000,100000]})")
                        .status,
                    ==, 413);
  g_assert_cmpuint (call (r, "PUT", base, "{").status, ==, 400);
  g_assert_cmpuint (call (r, "PUT", base + "/#data", "bad", "image/png").status, ==, 400);
  g_assert_cmpuint (call (r, "PUT", base + "/#preview", "{}").status, ==, 405);
}
static void
pdb_catalogue ()
{
  Router r;
  auto   out = call (r, "GET", "/api/v1/pdb/");
  g_assert_cmpuint (out.status, ==, 200);
  g_assert_true (out.body.find ("\"swagger\":\"2.0\"") != std::string::npos);
  g_assert_true (out.body.find ("/gimp-image-get-width") != std::string::npos);
  g_assert_true (out.body.find ("x-gimp-type") != std::string::npos);
}
static void
expired_image ()
{
  Router r;
  auto   i = image ();
  auto   p = path (i.get ());
  i.reset ();
  g_assert_cmpuint (call (r, "GET", p).status, ==, 404);
}
static gint received_mode = -1;
static GimpValueArray *
echo (GimpProcedure        *p, Gimp *, GimpContext *, GimpProgress *,
      const GimpValueArray *a, GError **)
{
  auto *r = gimp_procedure_get_return_values (p, TRUE, nullptr);
  for (int i = 0; i < p->num_args; ++i)
    g_value_copy (gimp_value_array_index (a, i), gimp_value_array_index (r, i + 1));
  received_mode = g_value_get_enum (gimp_value_array_index (a, 0));
  return r;
}
static void
pdb_values ()
{
  auto *p = gimp_procedure_new (echo, FALSE);
  gimp_object_set_static_name (GIMP_OBJECT (p), "painter-http-test-echo");
  auto add = [&] (GParamSpec *a, GParamSpec *b) {
    gimp_procedure_add_argument (p, a);
    gimp_procedure_add_return_value (p, b);
  };
  add (g_param_spec_enum ("mode", "mode", "mode", GIMP_TYPE_LAYER_MODE,
                          GIMP_LAYER_MODE_NORMAL, G_PARAM_READWRITE),
       g_param_spec_enum ("mode", "mode", "mode", GIMP_TYPE_LAYER_MODE,
                          GIMP_LAYER_MODE_NORMAL, G_PARAM_READWRITE));
  add (g_param_spec_uint ("unsigned", "unsigned", "unsigned", 0, G_MAXUINT, 0, G_PARAM_READWRITE),
       g_param_spec_uint ("unsigned", "unsigned", "unsigned", 0, G_MAXUINT, 0, G_PARAM_READWRITE));
  add (g_param_spec_boolean ("flag", "flag", "flag", FALSE, G_PARAM_READWRITE),
       g_param_spec_boolean ("flag", "flag", "flag", FALSE, G_PARAM_READWRITE));
  add (gimp_param_spec_int32_array ("ints", "ints", "ints", G_PARAM_READWRITE),
       gimp_param_spec_int32_array ("ints", "ints", "ints", G_PARAM_READWRITE));
  add (gimp_param_spec_double_array ("doubles", "doubles", "doubles", G_PARAM_READWRITE),
       gimp_param_spec_double_array ("doubles", "doubles", "doubles", G_PARAM_READWRITE));
  add (g_param_spec_boxed ("strings", "strings", "strings", G_TYPE_STRV, G_PARAM_READWRITE),
       g_param_spec_boxed ("strings", "strings", "strings", G_TYPE_STRV, G_PARAM_READWRITE));
  gimp_pdb_register_procedure (gimp->pdb, p);
  Router r;
  const char *input = R"({"context":{},"arguments":{"mode":23,"unsigned":4294967295,"flag":true,"ints":[-4,8],"doubles":[0.25,-1.5],"strings":["one","two"]}})";
  auto result = successful (call (r, "POST", "/api/v1/pdb/painter-http-test-echo", input));
  auto *v = member (result.get (), "values");
  g_assert_cmpint (received_mode, ==, GIMP_LAYER_MODE_PAINTER_ERASE);
  g_assert_cmpint (integer (member (v, "mode")), ==, 23);
  g_assert_cmpint (integer (member (v, "unsigned")), ==, 4294967295);
  g_assert_true (truth (member (v, "flag")));
  g_assert_cmpuint (
      json_array_get_length (json_node_get_array (member (v, "ints"))), ==, 2);
  successful (call (r, "POST", "/api/v1/pdb/painter-http-test-echo",
                    R"({"context":{},"arguments":{"mode":"normal"}})"));
  g_assert_cmpint (received_mode, ==, GIMP_LAYER_MODE_NORMAL);
  g_assert_cmpuint (call (r, "POST", "/api/v1/pdb/painter-http-test-echo",
                          R"({"context":{},"arguments":{"unsigned":-1}})")
                        .status,
                    ==, 400);
  g_assert_cmpuint (call (r, "POST", "/api/v1/pdb/painter-http-test-echo",
                          R"({"context":{},"arguments":{"mode":64}})")
                        .status,
                    ==, 400);
  auto spec = successful (call (r, "GET", "/api/v1/pdb/painter-http-test-echo"));
  g_assert_nonnull (member (spec.get (), "post"));
  auto *post_schema = member (spec.get (), "post");
  g_assert_true (text (member (post_schema, "description")).empty ());
  g_assert_cmpstr (json_array_get_string_element (
                       json_node_get_array (member (post_schema, "consumes")), 0),
                   ==, "application/json");
  g_assert_cmpstr (json_array_get_string_element (
                       json_node_get_array (member (post_schema, "produces")), 0),
                   ==, "application/json");
  gimp_pdb_unregister_procedure (gimp->pdb, p);
  g_object_unref (p);
}
static GimpValueArray *
typed_echo (GimpProcedure *procedure, Gimp *, GimpContext *, GimpProgress *,
            const GimpValueArray *arguments, GError **)
{
  auto *result = gimp_procedure_get_return_values (procedure, TRUE, nullptr);
  for (gint i = 0; i < procedure->num_args; ++i)
    g_value_copy (gimp_value_array_index (arguments, i),
                  gimp_value_array_index (result, i + 1));
  return result;
}
static void
pdb_native_objects ()
{
  auto  image_owner  = image ();
  auto *native_image = GIMP_IMAGE (image_owner.get ());
  auto *drawable     = layer (native_image, "Typed object");
  auto  node         = ObjectRef<GObject>::adopt (G_OBJECT (gegl_node_new ()));
  gegl_node_set (GEGL_NODE (node.get ()), "operation", "gegl:brightness-contrast", nullptr);
  auto  filter = ObjectRef<GObject>::adopt (G_OBJECT (gimp_drawable_filter_new (
      GIMP_DRAWABLE (drawable), "HTTP codec", GEGL_NODE (node.get ()), nullptr)));
  auto *resource  = gimp_brush_get_standard (gimp_get_user_context (gimp));
  auto *procedure = gimp_procedure_new (typed_echo, FALSE);
  gimp_object_set_static_name (GIMP_OBJECT (procedure),
                               "painter-http-test-native-types");
  for (bool returned : { false, true })
    {
      auto add = [&] (GParamSpec *spec) {
        if (returned)
          gimp_procedure_add_return_value (procedure, spec);
        else
          gimp_procedure_add_argument (procedure, spec);
      };
      add (g_param_spec_object ("image", "image", "image", GIMP_TYPE_IMAGE, G_PARAM_READWRITE));
      add (gimp_param_spec_core_object_array ("drawables", "drawables", "drawables",
                                              GIMP_TYPE_DRAWABLE, G_PARAM_READWRITE));
      add (g_param_spec_object ("color", "color", "color", GEGL_TYPE_COLOR, G_PARAM_READWRITE));
      add (g_param_spec_boxed ("colors", "colors", "colors",
                               GIMP_TYPE_COLOR_ARRAY, G_PARAM_READWRITE));
      add (gimp_param_spec_file ("file", "file", "file", GIMP_FILE_CHOOSER_ACTION_ANY,
                                 TRUE, nullptr, G_PARAM_READWRITE));
      add (g_param_spec_boxed ("parasite", "parasite", "parasite",
                               GIMP_TYPE_PARASITE, G_PARAM_READWRITE));
      add (g_param_spec_boxed ("bytes", "bytes", "bytes", G_TYPE_BYTES, G_PARAM_READWRITE));
      add (g_param_spec_object ("resource", "resource", "resource",
                                GIMP_TYPE_RESOURCE, G_PARAM_READWRITE));
      add (g_param_spec_object ("filter", "filter", "filter",
                                GIMP_TYPE_DRAWABLE_FILTER, G_PARAM_READWRITE));
      add (gimp_param_spec_export_options ("export-options", "export-options",
                                           "export-options", G_PARAM_READWRITE));
      add (g_param_spec_boxed ("encoding", "encoding", "encoding",
                               GIMP_TYPE_BABL_FORMAT, G_PARAM_READWRITE));
    }
  gimp_pdb_register_procedure (gimp->pdb, procedure);
  Router router;
  auto   body = "{\"context\":{},\"arguments\":{\"image\":" +
              std::to_string (gimp_image_get_id (native_image)) + ",\"drawables\":[" +
              std::to_string (gimp_item_get_id (GIMP_ITEM (drawable))) +
              "],\"color\":[0.2,0.4,0.6,1],\"colors\":[[0,0,0,1],[1,1,1,0.5]],"
              "\"file\":\"file:///tmp/"
              "http-test-not-opened\",\"parasite\":{\"name\":\"test\","
              "\"flags\":1,\"data\":\"AAEC\"},\"bytes\":[0,127,255]}}";
  auto  input     = parse (body);
  auto *arguments = member (input.get (), "arguments");
  set (arguments, "resource", number (gimp_data_get_id (resource)));
  set (arguments, "filter",
       number (gimp_drawable_filter_get_id (GIMP_DRAWABLE_FILTER (filter.get ()))));
  auto options = object ();
  set (options.get (), "capabilities", number (0));
  set (arguments, "export-options", std::move (options));
  set (arguments, "encoding", string ("R'G'B'A u8"));
  body        = reply (std::move (input)).body;
  auto result = successful (
      call (router, "POST", "/api/v1/pdb/painter-http-test-native-types", body));
  auto *values = member (result.get (), "values");
  g_assert_cmpint (integer (member (values, "resource")), ==, gimp_data_get_id (resource));
  g_assert_cmpint (integer (member (values, "filter")), ==,
                   gimp_drawable_filter_get_id (GIMP_DRAWABLE_FILTER (filter.get ())));
  g_assert_cmpint (
      integer (member (member (values, "export-options"), "capabilities")), ==, 0);
  g_assert_true (text (member (values, "encoding")) == "R'G'B'A u8");
  g_assert_cmpint (integer (member (values, "image")), ==, gimp_image_get_id (native_image));
  g_assert_cmpint (json_array_get_int_element (
                       json_node_get_array (member (values, "drawables")), 0),
                   ==, gimp_item_get_id (GIMP_ITEM (drawable)));
  g_assert_cmpfloat_with_epsilon (
      json_array_get_double_element (
          json_node_get_array (member (values, "color")), 0),
      .2, 1e-6);
  g_assert_cmpuint (json_array_get_length (json_node_get_array (member (values, "colors"))),
                    ==, 2);
  g_assert_true (text (member (values, "file")) == "file:///tmp/"
                                                   "http-test-not-opened");
  g_assert_true (text (member (member (values, "parasite"), "data")) == "AAEC");
  g_assert_cmpint (json_array_get_int_element (json_node_get_array (member (values, "bytes")), 2),
                   ==, 255);
  g_assert_cmpuint (call (router, "POST", "/api/v1/pdb/painter-http-test-native-types",
                          R"({"context":{},"arguments":{"drawables":[2147483647]}})")
                        .status,
                    ==, 400);
  gimp_pdb_unregister_procedure (gimp->pdb, procedure);
  g_object_unref (procedure);
}
static void
pdb_context ()
{
  Router r;
  auto   i  = image ();
  auto   id = gimp_image_get_id (GIMP_IMAGE (i.get ()));
  auto payload = "{\"context\":{\"image\":" + std::to_string (id) + "},\"arguments\":{}}";
  auto result = successful (call (r, "POST", "/api/v1/pdb/gimp-image-get-width", payload));
  g_assert_cmpint (integer (member (member (result.get (), "values"), "width")), ==, 32);
  g_assert_cmpint (
      integer (member (member (result.get (), "context"), "image")), ==, id);
  i.reset ();
  g_assert_cmpuint (
      call (r, "POST", "/api/v1/pdb/gimp-image-get-width", payload).status, ==, 404);
  g_assert_cmpuint (call (r, "POST", "/api/v1/pdb/does-not-exist", "{}").status, ==, 404);
}
struct HttpResult
{
  bool        done   = false;
  unsigned    status = 0;
  std::string body;
  GError     *error = nullptr;
};
static HttpResult
request (guint port, const char *method, const std::string &path,
         const std::string &body = "", const char *auth = token,
         const char *origin = nullptr, const char *type = "application/json")
{
  auto *session = soup_session_new_with_options ("proxy-resolver", nullptr,
                                                 "timeout", 5, nullptr);
  auto  uri     = "http://127.0.0.1:" + std::to_string (port) + path;
  auto *message = soup_message_new (method, uri.c_str ());
  if (auth)
    {
      std::string header = "Bearer " + std::string (auth);
      soup_message_headers_append (soup_message_get_request_headers (message),
                                   "Authorization", header.c_str ());
    }
  if (origin)
    soup_message_headers_append (soup_message_get_request_headers (message), "Origin", origin);
  if (std::string (method) == "POST" || std::string (method) == "PUT")
    {
      auto *b = g_bytes_new (body.data (), body.size ());
      soup_message_set_request_body_from_bytes (message, type, b);
      g_bytes_unref (b);
    }
  HttpResult result;
  soup_session_send_and_read_async (
      session, message, G_PRIORITY_DEFAULT, nullptr,
      [] (GObject *s, GAsyncResult *r, gpointer data) {
        auto *out = static_cast<HttpResult *> (data);
        auto *b = soup_session_send_and_read_finish (SOUP_SESSION (s), r, &out->error);
        if (b)
          {
            gsize n;
            auto *p = g_bytes_get_data (b, &n);
            out->body.assign (static_cast<const char *> (p), n);
            g_bytes_unref (b);
          }
        out->done = true;
      },
      &result);
  while (! result.done)
    g_main_context_iteration (nullptr, TRUE);
  result.status = soup_message_get_status (message);
  g_object_unref (message);
  g_object_unref (session);
  return result;
}
static ObjectRef<GObject>
start ()
{
  GError *e = nullptr;
  auto   *r = gimp_painter_httpd_start (gimp, 0, token, nullptr, &e);
  g_assert_no_error (e);
  g_assert_nonnull (r);
  return ObjectRef<GObject>::adopt (r);
}
static void
transport_security ()
{
  auto s    = start ();
  auto port = gimp_painter_httpd_port (s.get ());
  g_assert_cmpuint (port, >, 0);
  auto a = request (port, "GET", "/hello");
  g_assert_no_error (a.error);
  g_assert_cmpuint (a.status, ==, 200);
  g_assert_cmpstr (a.body.c_str (), ==, "Hello, hello");
  auto no = request (port, "GET", "/api/v1/images/", "", nullptr);
  g_assert_cmpuint (no.status, ==, 401);
  auto wrong = request (port, "GET", "/api/v1/images/", "", "wrong");
  g_assert_cmpuint (wrong.status, ==, 401);
  auto origin = request (port, "GET", "/api/v1/images/", "", token, "https://example.test");
  g_assert_cmpuint (origin.status, ==, 403);
  auto malformed = request (port, "PUT", "/api/v1/images/", "{");
  g_assert_cmpuint (malformed.status, ==, 400);
  gimp_painter_httpd_stop (s.get ());
  gimp_painter_httpd_stop (s.get ());
}
static void
transport_multipart ()
{
  auto   s    = start ();
  auto   port = gimp_painter_httpd_port (s.get ());
  Router r;
  auto   i = image ();
  layer (GIMP_IMAGE (i.get ()), "Ink");
  auto png = call (r, "GET", path (i.get ()) + "/Ink/#data");
  g_assert_cmpuint (png.status, ==, 200);
  std::string body = "--sample\r\nContent-Disposition: form-data; "
                     "name=\"image\"; filename=\"image.png\"\r\nContent-Type: "
                     "image/png\r\n\r\n" +
                     png.body + "\r\n--sample--\r\n";
  auto response = request (port, "PUT", path (i.get ()) + "/%23data", body, token,
                           nullptr, "multipart/form-data; boundary=sample");
  g_assert_no_error (response.error);
  if (response.status != 201)
    g_error ("multipart %u: %s", response.status, response.body.c_str ());
  gimp_painter_httpd_stop (s.get ());
}
struct Deferred
{
  SoupSession *session = soup_session_new_with_options ("proxy-resolver", nullptr,
                                                        "timeout", 5, nullptr);
  SoupMessage *message = nullptr;
  HttpResult   result;
  Deferred (guint port, const char *method, const std::string &path,
            const std::string &body = "")
  {
    auto uri         = "http://127.0.0.1:" + std::to_string (port) + path;
    message          = soup_message_new (method, uri.c_str ());
    std::string auth = "Bearer " + std::string (token);
    soup_message_headers_append (soup_message_get_request_headers (message),
                                 "Authorization", auth.c_str ());
    if (std::string (method) == "PUT" || std::string (method) == "POST")
      {
        auto *b = g_bytes_new (body.data (), body.size ());
        soup_message_set_request_body_from_bytes (message, "application/json", b);
        g_bytes_unref (b);
      }
    soup_session_send_and_read_async (
        session, message, G_PRIORITY_DEFAULT, nullptr,
        [] (GObject *s, GAsyncResult *r, gpointer p) {
          auto *out = static_cast<HttpResult *> (p);
          auto *b = soup_session_send_and_read_finish (SOUP_SESSION (s), r, &out->error);
          if (b)
            {
              gsize size;
              auto *d = g_bytes_get_data (b, &size);
              out->body.assign (static_cast<const char *> (d), size);
              g_bytes_unref (b);
            }
          out->done = true;
        },
        &result);
  }
  void
  queued (GObject *owner)
  {
    auto limit = g_get_monotonic_time () + 5 * G_USEC_PER_SEC;
    while (! pending_count (owner) && ! result.done && g_get_monotonic_time () < limit)
      {
        g_main_context_iteration (nullptr, FALSE);
        g_usleep (100);
      }
    g_assert_cmpuint (pending_count (owner), ==, 1);
  }
  void
  finish ()
  {
    while (! result.done)
      g_main_context_iteration (nullptr, TRUE);
    result.status = soup_message_get_status (message);
  }
  ~Deferred ()
  {
    finish ();
    g_clear_error (&result.error);
    g_object_unref (message);
    g_object_unref (session);
  }
};
static void
queued_image_close ()
{
  auto service = start ();
  auto i       = image ();
  Deferred request (gimp_painter_httpd_port (service.get ()), "GET", path (i.get ()));
  request.queued (service.get ());
  i.reset ();
  request.finish ();
  g_assert_no_error (request.result.error);
  g_assert_cmpuint (request.result.status, ==, 404);
  gimp_painter_httpd_stop (service.get ());
}
static void
queued_service_dispose ()
{
  auto     service = start ();
  auto     before  = gimp_container_get_n_children (gimp->images);
  Deferred request (gimp_painter_httpd_port (service.get ()), "PUT",
                    "/api/v1/images/", R"({"boundary":[0,0,4,4]})");
  request.queued (service.get ());
  g_object_run_dispose (service.get ());
  request.finish ();
  g_assert_cmpint (gimp_container_get_n_children (gimp->images), ==, before);
  g_assert_true (request.result.status == 503 || request.result.error != nullptr);
}
static void
headless_creation_delete ()
{
  Router r;
  auto created = successful (call (r, "PUT", "/api/v1/images/", R"({"boundary":[0,0,4,4]})"), 201);
  auto id = integer (member (created.get (), "image"));
  std::string body = "{\"context\":{},\"arguments\":{\"image\":" + std::to_string (id) + "}}";
  successful (call (r, "POST", "/api/v1/pdb/gimp-image-delete", body));
  g_assert_null (gimp_image_get_by_id (gimp, id));
}
static void
pdb_calling_error ()
{
  Router r;
  auto  *context  = gimp_get_user_context (gimp);
  auto  *previous = gimp_context_get_image (context);
  if (previous)
    g_object_ref (previous);
  gimp_context_set_image (context, nullptr);
  auto response = call (r, "POST", "/api/v1/pdb/gimp-image-get-width",
                        R"({"context":{},"arguments":{}})");
  g_assert_cmpuint (response.status, ==, 400);
  gimp_context_set_image (context, previous);
  if (previous)
    g_object_unref (previous);
}
static std::string
raw_request (guint port, const std::string &headers)
{
  auto *client = g_socket_client_new ();
  g_socket_client_set_enable_proxy (client, FALSE);
  g_socket_client_set_timeout (client, 5);
  GError *error    = nullptr;
  auto *connection = g_socket_client_connect_to_host (client, "127.0.0.1", port,
                                                      nullptr, &error);
  g_assert_no_error (error);
  g_assert_nonnull (connection);
  g_test_message ("Raw header case: %s", headers.c_str ());
  std::string bytes = headers + "Authorization: Bearer " + token + "\r\n\r\n";
  if (headers.find ("Transfer-Encoding: chunked") != std::string::npos)
    bytes += "0\r\n\r\n";
  g_assert_true (g_output_stream_write_all (
      g_io_stream_get_output_stream (G_IO_STREAM (connection)), bytes.data (),
      bytes.size (), nullptr, nullptr, &error));
  g_assert_no_error (error);
  auto *input = g_data_input_stream_new (g_io_stream_get_input_stream (G_IO_STREAM (connection)));
  HttpResult result;
  g_data_input_stream_read_line_async (
      input, G_PRIORITY_DEFAULT, nullptr,
      [] (GObject *object, GAsyncResult *res, gpointer data) {
        auto *out  = static_cast<HttpResult *> (data);
        gsize size = 0;
        auto *line = g_data_input_stream_read_line_finish (G_DATA_INPUT_STREAM (object),
                                                           res, &size, &out->error);
        if (line)
          {
            out->body.assign (line, size);
            g_free (line);
          }
        out->done = true;
      },
      &result);
  while (! result.done)
    g_main_context_iteration (nullptr, TRUE);
  g_assert_no_error (result.error);
  g_io_stream_close (G_IO_STREAM (connection), nullptr, nullptr);
  g_object_unref (input);
  g_object_unref (connection);
  g_object_unref (client);
  return result.body;
}
static void
transport_framing_and_host ()
{
  auto service = start ();
  auto port    = gimp_painter_httpd_port (service.get ());
  auto host    = "Host: 127.0.0.1:" + std::to_string (port) + "\r\n";
  g_assert_true (
      raw_request (port, "GET /hello HTTP/1.1\r\nHost: example.invalid\r\n").find (" 403 ") !=
      std::string::npos);
  g_assert_true (raw_request (port, "POST /api/v1/images/ HTTP/1.1\r\n" + host).find (" 411 ") !=
                 std::string::npos);
  g_assert_true (raw_request (port, "GET /hello HTTP/1.1\r\n" + host + "Transfer-Encoding: chunked\r\n")
                     .find (" 411 ") != std::string::npos);
  g_assert_true (raw_request (port, "PUT /api/v1/images/ HTTP/1.1\r\n" + host + "Content-Length: 16777217\r\nExpect: 100-continue\r\n")
                     .find (" 413 ") != std::string::npos);
  GError *error = nullptr;
  g_assert_null (gimp_painter_httpd_start (gimp, port, token, nullptr, &error));
  g_assert_nonnull (error);
  g_clear_error (&error);
  gimp_painter_httpd_stop (service.get ());
}
struct HttpDroppingLayer
{
  GimpLayer parent;
};
struct HttpDroppingLayerClass
{
  GimpLayerClass parent;
};
GType http_dropping_layer_get_type (void);
G_DEFINE_TYPE (HttpDroppingLayer, http_dropping_layer, GIMP_TYPE_LAYER)
static GObject *drop_image              = nullptr;
static bool     dropped_image_finalized = false;
static GdkPixbuf *
drop_last_external_image_owner (GimpViewable *, GimpContext *, gint width,
                                gint height, GeglColor *)
{
  g_clear_object (&drop_image);
  g_assert_false (dropped_image_finalized);
  auto *pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, width, height);
  gdk_pixbuf_fill (pixbuf, 0xffffffff);
  return pixbuf;
}
static void
http_dropping_layer_class_init (HttpDroppingLayerClass *klass)
{
  GIMP_VIEWABLE_CLASS (klass)->get_pixbuf     = nullptr;
  GIMP_VIEWABLE_CLASS (klass)->get_new_pixbuf = drop_last_external_image_owner;
}
static void
http_dropping_layer_init (HttpDroppingLayer *)
{
}
static void
render_reentrant_image_owner_loss ()
{
  Router router;
  drop_image              = image ().release ();
  dropped_image_finalized = false;
  g_object_weak_ref (
      drop_image, [] (gpointer, GObject *) { dropped_image_finalized = true; }, nullptr);
  auto  endpoint = path (drop_image) + "/Dropping/#data";
  auto *drawable = gimp_drawable_new (http_dropping_layer_get_type (),
                                      GIMP_IMAGE (drop_image), "Dropping", 0, 0,
                                      8, 8, babl_format ("R'G'B'A u8"));
  g_assert_true (gimp_image_add_layer (GIMP_IMAGE (drop_image),
                                       GIMP_LAYER (drawable), nullptr, 0, FALSE));
  auto response = call (router, "GET", endpoint);
  g_assert_cmpuint (response.status, ==, 200);
  g_assert_null (drop_image);
  g_assert_true (dropped_image_finalized);
}
static void
disabled_runtime ()
{
  g_unsetenv ("GIMP_PAINTER_HTTP_ENABLE");
  GError *e = nullptr;
  g_assert_null (gimp_painter_httpd_from_environment (gimp, &e));
  g_assert_no_error (e);
  g_assert_null (gimp_painter_httpd_start (gimp, 0, "short", nullptr, &e));
  g_assert_error (e, GIMP_PAINTER_ERROR, GIMP_PAINTER_ERROR_INVALID_STATE);
  g_clear_error (&e);
}
int
main (int argc, char **argv)
{
  g_test_init (&argc, &argv, nullptr);
  gimp_test_utils_set_gimp3_directory ("GIMP_TESTING_ABS_TOP_SRCDIR",
                                       "app/tests/gimpdir");
  gimp = gimp_init_for_testing ();
#define ADD(f) g_test_add_func ("/painter-http/" #f, f)
  ADD (routing);
  ADD (hierarchy);
  ADD (create_and_pixels);
  ADD (expired_image);
  ADD (pdb_values);
  ADD (pdb_context);
  ADD (pdb_native_objects);
  ADD (pdb_catalogue);
  ADD (transport_security);
  ADD (transport_multipart);
  ADD (transport_framing_and_host);
  ADD (disabled_runtime);
  ADD (render_reentrant_image_owner_loss);
  ADD (queued_image_close);
  ADD (queued_service_dispose);
  ADD (headless_creation_delete);
  ADD (pdb_calling_error);
  int result = g_test_run ();
  gimp_test_utils_set_gimp3_directory ("GIMP_TESTING_ABS_TOP_BUILDDIR",
                                       "app/tests/gimpdir-output");
  gimp_exit (gimp, TRUE);
  return result;
}
