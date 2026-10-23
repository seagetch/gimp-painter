/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gegl.h>
#include <gtk/gtk.h>
#include <cmath>
#include <limits>
#include <set>
#include <stdexcept>
#include "httpd-resource.hpp"
extern "C"
{
#include "libgimpbase/gimpbase.h"
#include "libgimpcolor/gimpcolor.h"
#include "core/gimp.h"
#include "core/gimp-gui.h"
#include "core/gimpcontext.h"
#include "core/gimpimage.h"
#include "core/gimpitem.h"
#include "core/gimpdrawable.h"
#include "core/gimpdrawablefilter.h"
#include "core/gimpresource.h"
#include "core/gimpdata.h"
#include "pdb/pdb-types.h"
#include "pdb/gimppdb.h"
#include "pdb/gimppdb-query.h"
#include "pdb/gimpprocedure.h"
#include "display/display-types.h"
#include "display/gimpdisplay.h"
}
#include "painter/object-ref.hpp"
namespace GimpPainter GIMP_PAINTER_PRIVATE {
namespace Http
{
using Refs = std::vector<ObjectRef<GObject> >;
struct ValuesFree
{
  void
  operator() (GimpValueArray *v) const
  {
    if (v)
      gimp_value_array_unref (v);
  }
};
using Values = std::unique_ptr<GimpValueArray, ValuesFree>;
static bool
unique_names (GParamSpec **specs, int n)
{
  std::set<std::string> names;
  for (int i = 0; i < n; ++i)
    if (! names.insert (specs[i]->name).second)
      return false;
  return true;
}
static std::string
key (GParamSpec *p, int i, bool names)
{
  return names || std::string (p->name) == "run-mode" ? p->name : std::to_string (i);
}
static gint
checked_id (JsonNode *n)
{
  auto id = integer (n);
  if (id < 0 || id > G_MAXINT)
    throw Failure (400, "Object ID is out of range");
  return id;
}
static GObject *
resolve (Gimp *g, GType t, JsonNode *n)
{
  if (JSON_NODE_HOLDS_NULL (n))
    return nullptr;
  gint     id = checked_id (n);
  GObject *o  = nullptr;
  if (g_type_is_a (t, GIMP_TYPE_IMAGE))
    o = G_OBJECT (gimp_image_get_by_id (g, id));
  else if (g_type_is_a (t, GIMP_TYPE_ITEM))
    o = G_OBJECT (gimp_item_get_by_id (g, id));
  else if (g_type_is_a (t, GIMP_TYPE_DISPLAY))
    o = G_OBJECT (gimp_display_get_by_id (g, id));
  else if (g_type_is_a (t, GIMP_TYPE_DRAWABLE_FILTER))
    o = G_OBJECT (gimp_drawable_filter_get_by_id (g, id));
  else if (g_type_is_a (t, GIMP_TYPE_RESOURCE))
    o = G_OBJECT (gimp_data_get_by_id (id));
  else if (g_type_is_a (t, GIMP_TYPE_UNIT))
    o = G_OBJECT (gimp_unit_get_by_id (id));
  else
    throw Failure (501, std::string ("Unsupported object type: ") + g_type_name (t));
  if (! o || ! g_type_is_a (G_OBJECT_TYPE (o), t))
    throw Failure (400, "Object ID has expired or has the wrong type");
  return o;
}
static Json
object_value (Gimp *g, GObject *o)
{
  if (! o)
    return null ();
  if (GIMP_IS_IMAGE (o))
    return number (gimp_image_get_id (GIMP_IMAGE (o)));
  if (GIMP_IS_ITEM (o))
    return number (gimp_item_get_id (GIMP_ITEM (o)));
  if (GIMP_IS_DISPLAY (o))
    return number (gimp_display_get_id (GIMP_DISPLAY (o)));
  if (GIMP_IS_DATA (o))
    return number (gimp_data_get_id (GIMP_DATA (o)));
  if (GIMP_IS_DRAWABLE_FILTER (o))
    return number (gimp_drawable_filter_get_id (GIMP_DRAWABLE_FILTER (o)));
  if (GIMP_IS_EXPORT_OPTIONS (o))
    {
      guint flags = 0;
      g_object_get (o, "capabilities", &flags, nullptr);
      auto n = object ();
      set (n.get (), "capabilities", number (flags));
      return n;
    }
  if (GIMP_IS_UNIT (o))
    return number (gimp_unit_get_id (GIMP_UNIT (o)));
  if (G_IS_FILE (o))
    {
      auto *s = g_file_get_uri (G_FILE (o));
      auto  n = string (s);
      g_free (s);
      return n;
    }
  if (GEGL_IS_COLOR (o))
    {
      double rgba[4];
      gegl_color_get_rgba (GEGL_COLOR (o), rgba, rgba + 1, rgba + 2, rgba + 3);
      auto n = array ();
      for (auto v : rgba)
        append (n.get (), real (v));
      return n;
    }
  throw Failure (501, std::string ("Unsupported result object: ") + G_OBJECT_TYPE_NAME (o));
}
static Json
serialize (Gimp *g, const GValue *v)
{
  GType t = G_VALUE_TYPE (v);
  if (t == G_TYPE_BOOLEAN)
    return boolean (g_value_get_boolean (v));
  if (t == G_TYPE_INT)
    return number (g_value_get_int (v));
  if (t == G_TYPE_UINT)
    return number (g_value_get_uint (v));
  if (t == G_TYPE_INT64)
    return number (g_value_get_int64 (v));
  if (t == G_TYPE_UINT64)
    {
      auto x = g_value_get_uint64 (v);
      if (x > G_MAXINT64)
        throw Failure (422, "Unsigned return exceeds JSON integer range");
      return number (x);
    }
  if (t == G_TYPE_LONG)
    return number (g_value_get_long (v));
  if (t == G_TYPE_ULONG)
    return number (g_value_get_ulong (v));
  if (t == G_TYPE_UCHAR)
    return number (g_value_get_uchar (v));
  if (t == G_TYPE_CHAR)
    return number (g_value_get_schar (v));
  if (t == G_TYPE_DOUBLE)
    return real (g_value_get_double (v));
  if (t == G_TYPE_FLOAT)
    return real (g_value_get_float (v));
  if (t == G_TYPE_STRING)
    return string (g_value_get_string (v));
  if (t == GIMP_TYPE_LAYER_MODE)
    return layer_mode_json (g_value_get_enum (v));
  if (G_TYPE_IS_ENUM (t))
    return number (g_value_get_enum (v));
  if (G_TYPE_IS_FLAGS (t))
    return number (g_value_get_flags (v));
  if (g_type_is_a (t, G_TYPE_OBJECT))
    return object_value (g, G_OBJECT (g_value_get_object (v)));
  if (t == GIMP_TYPE_BABL_FORMAT)
    {
      auto *format = static_cast<const Babl *> (g_value_get_boxed (v));
      return string (format ? babl_get_name (format) : nullptr);
    }
  if (! G_TYPE_IS_BOXED (t))
    throw Failure (501, std::string ("Unsupported result type: ") + g_type_name (t));
  if (! g_value_get_boxed (v))
    return null ();
  auto n = array ();
  if (t == GIMP_TYPE_INT32_ARRAY)
    {
      gsize size = 0;
      auto *p    = gimp_value_get_int32_array (v, &size);
      for (gsize i = 0; i < size; ++i)
        append (n.get (), number (p[i]));
    }
  else if (t == GIMP_TYPE_DOUBLE_ARRAY)
    {
      gsize size = 0;
      auto *p    = gimp_value_get_double_array (v, &size);
      for (gsize i = 0; i < size; ++i)
        append (n.get (), real (p[i]));
    }
  else if (t == G_TYPE_STRV)
    {
      auto **p = static_cast<gchar **> (g_value_get_boxed (v));
      for (; *p; ++p)
        append (n.get (), string (*p));
    }
  else if (t == GIMP_TYPE_CORE_OBJECT_ARRAY || t == GIMP_TYPE_COLOR_ARRAY)
    {
      auto **p = static_cast<GObject **> (g_value_get_boxed (v));
      for (; *p; ++p)
        append (n.get (), object_value (g, *p));
    }
  else if (t == G_TYPE_BYTES)
    {
      gsize size = 0;
      auto *p    = static_cast<const guint8 *> (
          g_bytes_get_data (static_cast<GBytes *> (g_value_get_boxed (v)), &size));
      for (gsize i = 0; i < size; ++i)
        append (n.get (), number (p[i]));
    }
  else if (t == GIMP_TYPE_PARASITE)
    {
      auto *p = static_cast<GimpParasite *> (g_value_get_boxed (v));
      n       = object ();
      set (n.get (), "name", string (gimp_parasite_get_name (p)));
      set (n.get (), "flags", number (gimp_parasite_get_flags (p)));
      guint32 size = 0;
      auto *d = static_cast<const guchar *> (gimp_parasite_get_data (p, &size));
      auto *s = g_base64_encode (d, size);
      set (n.get (), "data", string (s));
      g_free (s);
    }
  else
    throw Failure (501, std::string ("Unsupported result type: ") + g_type_name (t));
  return n;
}
static void
deserialize (Gimp *g, GParamSpec *p, GValue *v, JsonNode *n, Refs &refs)
{
  GType t        = G_VALUE_TYPE (v);
  auto  integral = [&] (gint64 lo, gint64 hi) {
    auto x = integer (n);
    if (x < lo || x > hi)
      throw Failure (400, std::string ("Out of range: ") + p->name);
    return x;
  };
  if (t == G_TYPE_BOOLEAN)
    g_value_set_boolean (v, truth (n));
  else if (t == G_TYPE_INT)
    g_value_set_int (v, integral (G_MININT, G_MAXINT));
  else if (t == G_TYPE_UINT)
    g_value_set_uint (v, integral (0, G_MAXUINT));
  else if (t == G_TYPE_INT64)
    g_value_set_int64 (v, integer (n));
  else if (t == G_TYPE_UINT64)
    g_value_set_uint64 (v, integral (0, G_MAXINT64));
  else if (t == G_TYPE_LONG)
    g_value_set_long (v, integral (G_MINLONG, G_MAXLONG));
  else if (t == G_TYPE_ULONG)
    g_value_set_ulong (v, integral (0, G_MAXLONG));
  else if (t == G_TYPE_CHAR)
    g_value_set_schar (v, integral (G_MININT8, G_MAXINT8));
  else if (t == G_TYPE_UCHAR)
    g_value_set_uchar (v, integral (0, G_MAXUINT8));
  else if (t == G_TYPE_DOUBLE)
    g_value_set_double (v, numeric (n));
  else if (t == G_TYPE_FLOAT)
    g_value_set_float (v, numeric (n));
  else if (t == G_TYPE_STRING)
    g_value_set_string (v, JSON_NODE_HOLDS_NULL (n) ? nullptr : text (n).c_str ());
  else if (t == GIMP_TYPE_LAYER_MODE)
    g_value_set_enum (v, layer_mode (n));
  else if (G_TYPE_IS_ENUM (t))
    {
      int id;
      if (JSON_NODE_HOLDS_VALUE (n) && json_node_get_value_type (n) == G_TYPE_STRING)
        {
          auto  s = text (n);
          auto *c = static_cast<GEnumClass *> (g_type_class_ref (t));
          auto *e = g_enum_get_value_by_nick (c, s.c_str ());
          if (! e)
            e = g_enum_get_value_by_name (c, s.c_str ());
          id = e ? e->value : 0;
          g_type_class_unref (c);
          if (! e)
            throw Failure (400, "Unknown enum name");
        }
      else
        id = integral (G_MININT, G_MAXINT);
      g_value_set_enum (v, id);
    }
  else if (G_TYPE_IS_FLAGS (t))
    g_value_set_flags (v, integral (0, G_MAXUINT));
  else if (g_type_is_a (t, G_TYPE_OBJECT))
    {
      GObject           *o = nullptr;
      ObjectRef<GObject> owned;
      if (JSON_NODE_HOLDS_NULL (n))
        {
        }
      else if (g_type_is_a (t, G_TYPE_FILE))
        {
          auto s = text (n);
          owned  = ObjectRef<GObject>::adopt (
              G_OBJECT (g_file_new_for_commandline_arg (s.c_str ())));
          o = owned.get ();
        }
      else if (t == GIMP_TYPE_EXPORT_OPTIONS)
        {
          auto *flags = member (n, "capabilities", false);
          auto  mask  = flags ? integer (flags) : 0;
          if (mask < 0 || mask > G_MAXUINT)
            throw Failure (400, "Export capabilities out of range");
          auto *klass = static_cast<GFlagsClass *> (g_type_class_ref (GIMP_TYPE_EXPORT_CAPABILITIES));
          bool valid = (guint (mask) & ~klass->mask) == 0;
          g_type_class_unref (klass);
          if (! valid)
            throw Failure (400, "Unknown export capability bits");
          owned = ObjectRef<GObject>::adopt (
              G_OBJECT (g_object_new (t, "capabilities", guint (mask), nullptr)));
          o = owned.get ();
        }
      else if (g_type_is_a (t, GEGL_TYPE_COLOR))
        {
          if (! JSON_NODE_HOLDS_ARRAY (n) ||
              json_array_get_length (json_node_get_array (n)) != 4)
            throw Failure (400, "Color needs four RGBA numbers");
          double c[4];
          for (int i = 0; i < 4; ++i)
            c[i] = numeric (json_array_get_element (json_node_get_array (n), i));
          auto *color = gegl_color_new (nullptr);
          gegl_color_set_rgba (color, c[0], c[1], c[2], c[3]);
          owned = ObjectRef<GObject>::adopt (G_OBJECT (color));
          o     = owned.get ();
        }
      else
        o = resolve (g, t, n);
      g_value_set_object (v, o);
    }
  else if (JSON_NODE_HOLDS_NULL (n))
    g_value_set_boxed (v, nullptr);
  else if (t == GIMP_TYPE_BABL_FORMAT)
    {
      auto name = text (n);
      if (! babl_format_exists (name.c_str ()))
        throw Failure (400, "Unknown Babl encoding");
      g_value_set_boxed (v, babl_format (name.c_str ()));
    }
  else if (t == GIMP_TYPE_PARASITE)
    {
      auto name  = text (member (n, "name"));
      auto flags = integer (member (n, "flags"));
      if (flags < 0 || flags > G_MAXUINT32)
        throw Failure (400, "Parasite flags out of range");
      auto  encoded  = text (member (n, "data"));
      gsize size     = 0;
      auto *bytes    = g_base64_decode (encoded.c_str (), &size);
      auto *parasite = gimp_parasite_new (name.c_str (), flags, size, bytes);
      g_free (bytes);
      g_value_take_boxed (v, parasite);
    }
  else
    {
      if (! JSON_NODE_HOLDS_ARRAY (n))
        throw Failure (400, "Expected array");
      auto *a    = json_node_get_array (n);
      guint size = json_array_get_length (a);
      if (size > 65536)
        throw Failure (413, "Array exceeds 65536 entries");
      if (t == GIMP_TYPE_INT32_ARRAY)
        {
          std::vector<gint32> x;
          for (guint i = 0; i < size; ++i)
            {
              auto q = integer (json_array_get_element (a, i));
              if (q < G_MININT32 || q > G_MAXINT32)
                throw Failure (400, "Array integer out of range");
              x.push_back (q);
            }
          gimp_value_set_int32_array (v, x.data (), size);
        }
      else if (t == GIMP_TYPE_DOUBLE_ARRAY)
        {
          std::vector<double> x;
          for (guint i = 0; i < size; ++i)
            x.push_back (numeric (json_array_get_element (a, i)));
          gimp_value_set_double_array (v, x.data (), size);
        }
      else if (t == G_TYPE_STRV)
        {
          auto **strv = g_new0 (gchar *, size + 1);
          for (guint i = 0; i < size; ++i)
            {
              try
                {
                  strv[i] = g_strdup (text (json_array_get_element (a, i)).c_str ());
                }
              catch (...)
                {
                  g_strfreev (strv);
                  throw;
                }
            }
          g_value_take_boxed (v, strv);
        }
      else if (t == GIMP_TYPE_CORE_OBJECT_ARRAY)
        {
          auto type = gimp_param_spec_core_object_array_get_object_type (p);
          std::vector<GObject *> objects;
          for (guint i = 0; i < size; ++i)
            {
              auto *o = resolve (g, type, json_array_get_element (a, i));
              if (! o)
                throw Failure (400, "Null object array member");
              objects.push_back (o);
              refs.push_back (ObjectRef<GObject>::retain (o));
            }
          objects.push_back (nullptr);
          g_value_set_boxed (v, objects.data ());
        }
      else if (t == GIMP_TYPE_COLOR_ARRAY)
        {
          std::vector<ObjectRef<GObject> > colors;
          std::vector<GeglColor *>         values;
          for (guint i = 0; i < size; ++i)
            {
              auto *entry = json_array_get_element (a, i);
              if (! JSON_NODE_HOLDS_ARRAY (entry) ||
                  json_array_get_length (json_node_get_array (entry)) != 4)
                throw Failure (400, "Color needs four RGBA numbers");
              double c[4];
              for (int j = 0; j < 4; ++j)
                c[j] = numeric (json_array_get_element (json_node_get_array (entry), j));
              auto *color = gegl_color_new (nullptr);
              gegl_color_set_rgba (color, c[0], c[1], c[2], c[3]);
              colors.push_back (ObjectRef<GObject>::adopt (G_OBJECT (color)));
              values.push_back (color);
            }
          values.push_back (nullptr);
          g_value_set_boxed (v, values.data ());
        }
      else if (t == G_TYPE_BYTES)
        {
          std::vector<guint8> x;
          for (guint i = 0; i < size; ++i)
            {
              auto q = integer (json_array_get_element (a, i));
              if (q < 0 || q > 255)
                throw Failure (400, "Byte out of range");
              x.push_back (q);
            }
          g_value_take_boxed (v, g_bytes_new (x.data (), size));
        }
      else
        throw Failure (501, std::string ("Unsupported argument type: ") + g_type_name (t));
    }
  if (g_param_value_validate (p, v))
    throw Failure (400,
                   std::string ("Argument violates parameter constraints: ") + p->name);
}
static Json
schema (GParamSpec *p)
{
  auto        n     = object ();
  const GType t     = G_PARAM_SPEC_VALUE_TYPE (p);
  const bool  color = g_type_is_a (t, GEGL_TYPE_COLOR);
  const char *type  = (t == G_TYPE_STRING || t == GIMP_TYPE_BABL_FORMAT ||
                      g_type_is_a (t, G_TYPE_FILE)) ?
                          "string" :
                      t == G_TYPE_BOOLEAN                       ? "boolean" :
                      (t == G_TYPE_DOUBLE || t == G_TYPE_FLOAT) ? "number" :
                      (t == G_TYPE_STRV || t == GIMP_TYPE_INT32_ARRAY ||
                      t == GIMP_TYPE_DOUBLE_ARRAY || t == GIMP_TYPE_CORE_OBJECT_ARRAY ||
                      t == GIMP_TYPE_COLOR_ARRAY || t == G_TYPE_BYTES || color) ?
                                                                  "array" :
                      (t == GIMP_TYPE_PARASITE || t == GIMP_TYPE_EXPORT_OPTIONS) ?
                                                                  "object" :
                                                                  "integer";
  // Swagger2 has no oneOf. Leave enum type unrestricted and publish the exact
  // integer/name alternatives as metadata instead of promising a false schema.
  if (! G_TYPE_IS_ENUM (t))
    set (n.get (), "type", string (type));
  set (n.get (), "description",
       string (g_param_spec_get_blurb (p) ? g_param_spec_get_blurb (p) : ""));
  set (n.get (), "x-gimp-type", string (g_type_name (t)));
  if (G_TYPE_IS_ENUM (t))
    {
      auto alternatives = array ();
      append (alternatives.get (), string ("integer"));
      append (alternatives.get (), string ("string"));
      set (n.get (), "x-gimp-value-types", std::move (alternatives));
      if (t == GIMP_TYPE_LAYER_MODE)
        set (n.get (), "x-gimp-v1-legacy-numeric-modes", boolean (true));
    }
  if (std::string (type) == "array")
    {
      auto items = object ();
      set (items.get (), "type",
           string (t == G_TYPE_STRV                       ? "string" :
                   t == GIMP_TYPE_COLOR_ARRAY             ? "array" :
                   (t == GIMP_TYPE_DOUBLE_ARRAY || color) ? "number" :
                                                            "integer"));
      if (t == GIMP_TYPE_COLOR_ARRAY)
        {
          auto component = object ();
          set (component.get (), "type", string ("number"));
          set (items.get (), "items", std::move (component));
          set (items.get (), "minItems", number (4));
          set (items.get (), "maxItems", number (4));
        }
      set (n.get (), "items", std::move (items));
      if (color)
        {
          set (n.get (), "minItems", number (4));
          set (n.get (), "maxItems", number (4));
        }
    }
  if (t == GIMP_TYPE_PARASITE || t == GIMP_TYPE_EXPORT_OPTIONS)
    {
      auto properties = object ();
      if (t == GIMP_TYPE_EXPORT_OPTIONS)
        {
          auto value = object ();
          set (value.get (), "type", string ("integer"));
          set (properties.get (), "capabilities", std::move (value));
        }
      else
        for (const char *name : { "name", "flags", "data" })
          {
            auto value = object ();
            set (value.get (), "type",
                 string (std::string (name) == "flags" ? "integer" : "string"));
            if (std::string (name) == "data")
              set (value.get (), "format", string ("byte"));
            set (properties.get (), name, std::move (value));
          }
      set (n.get (), "properties", std::move (properties));
    }
  return n;
}
static Json
properties (GParamSpec **ps, int count)
{
  auto n     = object ();
  bool names = unique_names (ps, count);
  for (int i = 0; i < count; ++i)
    set (n.get (), key (ps[i], i, names).c_str (), schema (ps[i]));
  return n;
}
static Json
publish (GimpProcedure *p)
{
  auto root = object (), post = object (), responses = object (),
       success = object (), result = object (), props = object ();
  set (post.get (), "operationId", string (gimp_object_get_name (p)));
  auto media_types = array ();
  append (media_types.get (), string ("application/json"));
  set (post.get (), "consumes", Json (json_node_copy (media_types.get ())));
  set (post.get (), "produces", std::move (media_types));
  set (post.get (), "description",
       string (gimp_procedure_get_blurb (p) ? gimp_procedure_get_blurb (p) : ""));
  set (result.get (), "type", string ("object"));
  auto context_schema = object ();
  set (context_schema.get (), "type", string ("object"));
  set (props.get (), "context", std::move (context_schema));
  auto values = object ();
  set (values.get (), "type", string ("object"));
  set (values.get (), "properties", properties (p->values, p->num_values));
  set (props.get (), "values", std::move (values));
  set (result.get (), "properties", std::move (props));
  set (success.get (), "description", string ("Successful call"));
  set (success.get (), "schema", std::move (result));
  set (responses.get (), "200", std::move (success));
  set (post.get (), "responses", std::move (responses));
  auto params = array (), payload = object (), input = object (),
       fields = object (), args = object ();
  set (payload.get (), "name", string ("payload"));
  set (payload.get (), "in", string ("body"));
  set (payload.get (), "required", boolean (true));
  set (input.get (), "type", string ("object"));
  auto ctx = object ();
  set (ctx.get (), "type", string ("object"));
  set (fields.get (), "context", std::move (ctx));
  set (args.get (), "type", string ("object"));
  set (args.get (), "properties", properties (p->args, p->num_args));
  set (fields.get (), "arguments", std::move (args));
  set (input.get (), "properties", std::move (fields));
  set (payload.get (), "schema", std::move (input));
  append (params.get (), std::move (payload));
  set (post.get (), "parameters", std::move (params));
  set (root.get (), "post", std::move (post));
  return root;
}
class Pdb final : public Resource
{
public:
  Response
  handle (Gimp *g, const Request &r) override
  {
    auto parts = segments (r.path.substr (std::string ("/api/v1/pdb").size ()));
    if (parts.size () > 1)
      throw Failure (404, "Unknown PDB route");
    if (r.method != "GET" && r.method != "POST")
      throw Failure (405, "PDB supports GET and POST");
    GimpProcedure *p = parts.empty () ?
                           nullptr :
                           gimp_pdb_lookup_procedure (g->pdb, parts[0].c_str ());
    if (! parts.empty () && (! p || p->is_private))
      throw Failure (404, "Procedure not found");
    if (r.method == "GET")
      {
        if (p)
          return reply (publish (p));
        auto n = object (), info = object (), paths = object ();
        set (n.get (), "swagger", string ("2.0"));
        set (info.get (), "title", string ("GIMP PDB"));
        set (info.get (), "version", string ("3.0"));
        set (n.get (), "info", std::move (info));
        set (n.get (), "basePath", string ("/api/v1/pdb"));
        gchar **names = nullptr;
        GError *e     = nullptr;
        if (! gimp_pdb_query (g->pdb, ".*", ".*", ".*", ".*", ".*", ".*", ".*", &names, &e))
          {
            std::string m = e ? e->message : "PDB query failed";
            g_clear_error (&e);
            throw Failure (500, m);
          }
        try
          {
            for (gchar **s = names; s && *s; ++s)
              {
                auto *proc = gimp_pdb_lookup_procedure (g->pdb, *s);
                if (proc && ! proc->is_private)
                  set (paths.get (), ("/" + std::string (*s)).c_str (), publish (proc));
              }
          }
        catch (...)
          {
            g_strfreev (names);
            throw;
          }
        g_strfreev (names);
        set (n.get (), "paths", std::move (paths));
        return reply (std::move (n));
      }
    if (! p)
      throw Failure (404, "Procedure name required");
    auto    input = parse (r.body);
    Context c     = context (g, member (input.get (), "context"));
    auto   *a     = member (input.get (), "arguments");
    if (! JSON_NODE_HOLDS_OBJECT (a))
      throw Failure (400, "arguments must be an object");
    Refs refs;
    for (auto *o : { G_OBJECT (c.image), G_OBJECT (c.item),
                     G_OBJECT (c.drawable), G_OBJECT (c.display) })
      if (o)
        refs.push_back (ObjectRef<GObject>::retain (o));
    auto                  procedure = ObjectRef<GObject>::retain (G_OBJECT (p));
    Values                args (gimp_procedure_get_arguments (p));
    bool                  names = unique_names (p->args, p->num_args);
    std::set<std::string> known;
    for (int i = 0; i < p->num_args; ++i)
      known.insert (key (p->args[i], i, names));
    auto *members = json_object_get_members (json_node_get_object (a));
    for (auto *l = members; l; l = l->next)
      if (! known.count (static_cast<const char *> (l->data)))
        {
          std::string bad = static_cast<const char *> (l->data);
          g_list_free (members);
          throw Failure (400, "Unknown argument: " + bad);
        }
    g_list_free (members);
    for (int i = 0; i < p->num_args; ++i)
      {
        auto *spec = p->args[i];
        auto *v    = gimp_value_array_index (args.get (), i);
        auto  name = key (spec, i, names);
        auto *j    = member (a, name.c_str (), false);
        auto  t    = G_VALUE_TYPE (v);
        if (std::string (spec->name) == "run-mode" && G_TYPE_IS_ENUM (t))
          g_value_set_enum (v, GIMP_RUN_NONINTERACTIVE);
        else if (j)
          deserialize (g, spec, v, j, refs);
        else if (g_type_is_a (t, GIMP_TYPE_IMAGE) && c.image)
          g_value_set_object (v, c.image);
        else if (g_type_is_a (t, GIMP_TYPE_DRAWABLE) && c.drawable &&
                 g_type_is_a (G_OBJECT_TYPE (c.drawable), t))
          g_value_set_object (v, c.drawable);
        else if (g_type_is_a (t, GIMP_TYPE_ITEM) && c.item &&
                 g_type_is_a (G_OBJECT_TYPE (c.item), t))
          g_value_set_object (v, c.item);
        else if (g_type_is_a (t, GIMP_TYPE_DISPLAY) && c.display)
          g_value_set_object (v, c.display);
        else if (t == GIMP_TYPE_CORE_OBJECT_ARRAY && c.drawable &&
                 g_type_is_a (G_OBJECT_TYPE (c.drawable),
                              gimp_param_spec_core_object_array_get_object_type (spec)))
          {
            GObject *objects[] = { G_OBJECT (c.drawable), nullptr };
            g_value_set_boxed (v, objects);
          }
      }
    GError     *e = nullptr;
    Values      output (gimp_procedure_execute (p, g, gimp_get_user_context (g),
                                                nullptr, args.get (), &e));
    std::string execution_error = e ? e->message : "";
    g_clear_error (&e);
    if (! output || gimp_value_array_length (output.get ()) < 1)
      throw Failure (500, execution_error.empty () ?
                              "Procedure returned no status" :
                              execution_error);
    int status = g_value_get_enum (gimp_value_array_index (output.get (), 0));
    if (status != GIMP_PDB_SUCCESS)
      {
        std::string m = execution_error.empty () ?
                            "Procedure execution failed" :
                            execution_error;
        if (gimp_value_array_length (output.get ()) > 1 &&
            G_VALUE_HOLDS_STRING (gimp_value_array_index (output.get (), 1)))
          {
            auto *s = g_value_get_string (gimp_value_array_index (output.get (), 1));
            if (s)
              m = s;
          }
        throw Failure (status == GIMP_PDB_CALLING_ERROR ? 400 :
                       status == GIMP_PDB_CANCEL        ? 409 :
                                                          500,
                       m);
      }
    if (gimp_value_array_length (output.get ()) < p->num_values + 1)
      throw Failure (500, "Procedure return count mismatch");
    auto result = object (), values = object ();
    names = unique_names (p->values, p->num_values);
    for (int i = 0; i < p->num_values; ++i)
      set (values.get (), key (p->values[i], i, names).c_str (),
           serialize (g, gimp_value_array_index (output.get (), i + 1)));
    set (result.get (), "context", context_json (c));
    set (result.get (), "values", std::move (values));
    return reply (std::move (result));
  }
};
std::unique_ptr<Resource>
pdb_resource ()
{
  return std::unique_ptr<Resource> (new Pdb);
}
}
}
