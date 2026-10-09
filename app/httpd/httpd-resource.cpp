/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gegl.h>
#include <gtk/gtk.h>
#include <cmath>
#include <stdexcept>
#include "httpd-resource.hpp"
#include "painter/object-ref.hpp"
#include "painter/resources.hpp"
extern "C"
{
#include "core/gimp.h"
#include "core/gimp-gui.h"
#include "core/gimpcontext.h"
#include "core/gimpimage.h"
#include "core/gimpitem.h"
#include "core/gimpdrawable.h"
#include "operations/layer-modes-legacy/gimpoperationpainterlegacy.h"
#include "display/display-types.h"
#include "display/gimpdisplay.h"
}
namespace GimpPainter GIMP_PAINTER_PRIVATE {
namespace Http
{
Json
object ()
{
  Json n (json_node_new (JSON_NODE_OBJECT));
  json_node_take_object (n.get (), json_object_new ());
  return n;
}
Json
array ()
{
  Json n (json_node_new (JSON_NODE_ARRAY));
  json_node_take_array (n.get (), json_array_new ());
  return n;
}
Json
number (gint64 v)
{
  Json n (json_node_new (JSON_NODE_VALUE));
  json_node_set_int (n.get (), v);
  return n;
}
Json
real (double v)
{
  if (! std::isfinite (v))
    throw Failure (400, "Non-finite number");
  Json n (json_node_new (JSON_NODE_VALUE));
  json_node_set_double (n.get (), v);
  return n;
}
Json
string (const char *v)
{
  if (! v)
    return null ();
  Json n (json_node_new (JSON_NODE_VALUE));
  json_node_set_string (n.get (), v);
  return n;
}
Json
boolean (bool v)
{
  Json n (json_node_new (JSON_NODE_VALUE));
  json_node_set_boolean (n.get (), v);
  return n;
}
Json
null ()
{
  return Json (json_node_new (JSON_NODE_NULL));
}
void
set (JsonNode *n, const char *k, Json v)
{
  json_object_set_member (json_node_get_object (n), k, v.release ());
}
void
append (JsonNode *n, Json v)
{
  json_array_add_element (json_node_get_array (n), v.release ());
}
JsonNode *
member (JsonNode *n, const char *k, bool required)
{
  if (! n || ! JSON_NODE_HOLDS_OBJECT (n))
    throw Failure (400, "Expected JSON object");
  auto *o = json_node_get_object (n);
  if (! json_object_has_member (o, k))
    {
      if (required)
        throw Failure (400, std::string ("Missing field: ") + k);
      return nullptr;
    }
  return json_object_get_member (o, k);
}
std::string
text (JsonNode *n)
{
  if (! n || ! JSON_NODE_HOLDS_VALUE (n) || json_node_get_value_type (n) != G_TYPE_STRING)
    throw Failure (400, "Expected string");
  return json_node_get_string (n);
}
gint64
integer (JsonNode *n)
{
  if (! n || ! JSON_NODE_HOLDS_VALUE (n) || json_node_get_value_type (n) != G_TYPE_INT64)
    throw Failure (400, "Expected integer");
  return json_node_get_int (n);
}
double
numeric (JsonNode *n)
{
  if (! n || ! JSON_NODE_HOLDS_VALUE (n) ||
      (json_node_get_value_type (n) != G_TYPE_DOUBLE && json_node_get_value_type (n) != G_TYPE_INT64))
    throw Failure (400, "Expected number");
  auto v = json_node_get_double (n);
  if (! std::isfinite (v))
    throw Failure (400, "Non-finite number");
  return v;
}
bool
truth (JsonNode *n)
{
  if (! n || ! JSON_NODE_HOLDS_VALUE (n) || json_node_get_value_type (n) != G_TYPE_BOOLEAN)
    throw Failure (400, "Expected boolean");
  return json_node_get_boolean (n);
}
// v1 numbers and old nicknames belong to the pinned Painter enum, never
// modern colliding ordinals. Modern modes remain available by current nickname.
static const char *legacy_modes[] = {
  "normal-mode",      "dissolve-mode",     "behind-mode",
  "multiply-mode",    "screen-mode",       "overlay-mode",
  "difference-mode",  "addition-mode",     "subtract-mode",
  "darken-only-mode", "lighten-only-mode", "hue-mode",
  "saturation-mode",  "color-mode",        "value-mode",
  "divide-mode",      "dodge-mode",        "burn-mode",
  "hardlight-mode",   "softlight-mode",    "grain-extract-mode",
  "grain-merge-mode", "color-erase-mode",  "erase-mode",
  "replace-mode",     "anti-erase-mode",   "src-in-mode",
  "dst-in-mode",      "src-out-mode",      "dst-out-mode"
};
int
layer_mode (JsonNode *n)
{
  if (! n)
    return GIMP_LAYER_MODE_PAINTER_NORMAL;
  GimpLayerMode mode;
  if (JSON_NODE_HOLDS_VALUE (n) && json_node_get_value_type (n) == G_TYPE_INT64)
    {
      auto value = integer (n);
      if (value < 0 || value > 29 || ! gimp_painter_layer_mode_from_legacy (value, &mode))
        throw Failure (400, "Invalid v1 layer mode number");
      return mode;
    }
  auto name = text (n);
  for (guint i = 0; i < G_N_ELEMENTS (legacy_modes); ++i)
    if (name == legacy_modes[i])
      {
        gimp_painter_layer_mode_from_legacy (i, &mode);
        return mode;
      }
  auto *c = static_cast<GEnumClass *> (g_type_class_ref (GIMP_TYPE_LAYER_MODE));
  auto *v = g_enum_get_value_by_nick (c, name.c_str ());
  if (! v)
    v = g_enum_get_value_by_name (c, name.c_str ());
  int out = v ? v->value : 0;
  g_type_class_unref (c);
  if (! v)
    throw Failure (400, "Unknown layer mode nickname");
  return out;
}
Json
layer_mode_json (int mode)
{
  guint32 raw = 0;
  if (gimp_painter_layer_mode_to_legacy (static_cast<GimpLayerMode> (mode), &raw))
    return number (raw);
  auto *c = static_cast<GEnumClass *> (g_type_class_ref (GIMP_TYPE_LAYER_MODE));
  auto *v = g_enum_get_value (c, mode);
  auto  result = string (v ? v->value_nick : "unknown");
  g_type_class_unref (c);
  return result;
}
Json
layer_mode_name (int mode)
{
  guint32 raw = 0;
  if (gimp_painter_layer_mode_to_legacy (static_cast<GimpLayerMode> (mode), &raw))
    return string (legacy_modes[raw]);
  return layer_mode_json (mode);
}
Json
parse (const std::string &s)
{
  if (s.size () > 1024 * 1024)
    throw Failure (413, "JSON body exceeds 1 MiB");
  auto parser = ObjectRef<GObject>::adopt (G_OBJECT (json_parser_new ()));
  auto       *p  = JSON_PARSER (parser.get ());
  GError     *e  = nullptr;
  bool        ok = json_parser_load_from_data (p, s.data (), s.size (), &e);
  std::unique_ptr<GError, decltype (&g_error_free)> error (e, g_error_free);
  JsonNode   *root = ok ? json_parser_get_root (p) : nullptr;
  Json        n (root ? json_node_copy (root) : nullptr);
  std::string message = error ? error->message : "Invalid JSON";
  if (! n)
    throw Failure (400, message);
  return n;
}
Response
reply (Json n, unsigned status)
{
  String s (json_to_string (n.get (), FALSE));
  Response r;
  r.status = status;
  r.body   = s.get ();
  return r;
}
Response
error (unsigned status, const std::string &s)
{
  auto n = object ();
  set (n.get (), "error", string (s.c_str ()));
  return reply (std::move (n), status);
}
std::vector<std::string>
segments (const std::string &p)
{
  std::vector<std::string> r;
  size_t                   begin = 0;
  while (begin < p.size ())
    {
      auto end = p.find ('/', begin);
      if (end == std::string::npos)
        end = p.size ();
      if (end > begin)
        r.push_back (p.substr (begin, end - begin));
      begin = end + 1;
    }
  return r;
}
static gint
id (JsonNode *n)
{
  auto v = integer (n);
  if (v < 1 || v > G_MAXINT)
    throw Failure (400, "Invalid object ID");
  return v;
}
Context
context (Gimp *g, JsonNode *n)
{
  auto   *user = gimp_get_user_context (g);
  Context c;
  c.image   = gimp_context_get_image (user);
  c.display = gimp_context_get_display (user);
  if (n)
    {
      if (auto *v = member (n, "image", false))
        {
          c.image = gimp_image_get_by_id (g, id (v));
          if (! c.image)
            throw Failure (404, "Context image is gone");
        }
    }
  if (c.image)
    {
      auto *drawables = gimp_image_get_selected_drawables (c.image);
      if (drawables)
        c.drawable = GIMP_DRAWABLE (drawables->data);
      g_list_free (drawables);
      c.item = c.drawable ? GIMP_ITEM (c.drawable) : nullptr;
    }
  if (n)
    {
      if (auto *v = member (n, "item", false))
        {
          c.item = gimp_item_get_by_id (g, id (v));
          if (! c.item)
            throw Failure (404, "Context item is gone");
        }
      if (auto *v = member (n, "drawable", false))
        {
          auto *i = gimp_item_get_by_id (g, id (v));
          if (! i || ! GIMP_IS_DRAWABLE (i))
            throw Failure (404, "Context drawable is gone");
          c.drawable = GIMP_DRAWABLE (i);
        }
      if (auto *v = member (n, "display", false))
        {
          c.display = gimp_display_get_by_id (g, id (v));
          if (! c.display)
            throw Failure (404, "Context display is gone");
        }
    }
  if (c.image && ((c.item && gimp_item_get_image (c.item) != c.image) ||
                  (c.drawable && gimp_item_get_image (GIMP_ITEM (c.drawable)) != c.image)))
    throw Failure (400, "Context objects belong to different images");
  return c;
}
Json
context_json (const Context &c)
{
  auto n = object ();
  set (n.get (), "image", c.image ? number (gimp_image_get_id (c.image)) : null ());
  set (n.get (), "item", c.item ? number (gimp_item_get_id (c.item)) : null ());
  set (n.get (), "drawable",
       c.drawable ? number (gimp_item_get_id (GIMP_ITEM (c.drawable))) : null ());
  return n;
}
struct Router::Rule
{
  std::string               prefix;
  std::unique_ptr<Resource> resource;
  bool                      exact;
  virtual ~Rule () = default;
  Rule (std::string p, std::unique_ptr<Resource> r, bool e)
      : prefix (std::move (p)), resource (std::move (r)), exact (e)
  {
  }
};
class NavigationResource final : public Resource
{
  Navigation action_;

public:
  explicit NavigationResource (Navigation f) : action_ (std::move (f)) {}
  Response
  handle (Gimp *g, const Request &r) override
  {
    if (r.method != "POST")
      throw Failure (405, "Navigation supports POST");
    if (! action_)
      throw Failure (503, "Navigation needs a GUI display");
    return action_ (g, r);
  }
};
Router::Router (Navigation n)
{
  rules_.push_back (std::unique_ptr<Rule> (new Rule ("/api/v1/pdb", pdb_resource (), false)));
  rules_.push_back (std::unique_ptr<Rule> (new Rule ("/api/v1/images", images_resource (), false)));
  rules_.push_back (std::unique_ptr<Rule> (new Rule (
      "/api/v1/navigation",
      std::unique_ptr<Resource> (new NavigationResource (std::move (n))), true)));
}
Router::~Router () = default;
Response
Router::dispatch (Gimp *g, const Request &r)
{
  try
    {
      for (auto &rule : rules_)
        if (r.path == rule->prefix ||
            (! rule->exact &&
             r.path.compare (0, rule->prefix.size () + 1, rule->prefix + "/") == 0))
          return rule->resource->handle (g, r);
      auto parts = segments (r.path);
      if (parts.size () == 1 && r.method == "GET")
        {
          Response out;
          out.content_type = "text/plain; charset=utf-8";
          out.body         = "Hello, " + parts[0];
          return out;
        }
      return error (404, "Route not found");
    }
  catch (const Failure &e)
    {
      return error (e.status, e.what ());
    }
  catch (const std::exception &e)
    {
      return error (500, e.what ());
    }
  catch (...)
    {
      return error (500, "Unknown request failure");
    }
}
}
}
