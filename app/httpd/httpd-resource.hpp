/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_HTTP_RESOURCE_HPP
#define GIMP_PAINTER_HTTP_RESOURCE_HPP
#include <json-glib/json-glib.h>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
extern "C"
{
#include "core/core-types.h"
}
namespace GimpPainter
{
namespace Http
{
struct JsonFree
{
  void
  operator() (JsonNode *p) const noexcept
  {
    if (p)
      json_node_unref (p);
  }
};
using Json = std::unique_ptr<JsonNode, JsonFree>;
struct Request
{
  std::string method, path, content_type, body;
};
struct Response
{
  unsigned    status       = 200;
  std::string content_type = "application/json; charset=utf-8", body;
};
class Failure : public std::runtime_error
{
public:
  unsigned status;
  Failure (unsigned s, const std::string &message)
      : std::runtime_error (message), status (s)
  {
  }
};
Json        object ();
Json        array ();
Json        number (gint64);
Json        real (double);
Json        string (const char *);
Json        boolean (bool);
Json        null ();
void        set (JsonNode *, const char *, Json);
void        append (JsonNode *, Json);
JsonNode   *member (JsonNode *, const char *, bool required = true);
std::string text (JsonNode *);
gint64      integer (JsonNode *);
double      numeric (JsonNode *);
bool        truth (JsonNode *);
int         layer_mode (JsonNode *);
Json        layer_mode_json (int);
Json        layer_mode_name (int);
Json        parse (const std::string &);
Response    reply (Json, unsigned status = 200);
Response    error (unsigned, const std::string &);
std::vector<std::string> segments (const std::string &);
struct Context
{
  GimpImage    *image    = nullptr;
  GimpItem     *item     = nullptr;
  GimpDrawable *drawable = nullptr;
  GimpDisplay  *display  = nullptr;
};
Context context (Gimp *, JsonNode *);
Json    context_json (const Context &);
using Navigation = std::function<Response (Gimp *, const Request &)>;
/* Resources and route rules are values/unique owners with virtual destruction.
 * No route aliases own or double-delete a shared raw factory. */
class Resource
{
public:
  virtual ~Resource ()                              = default;
  virtual Response handle (Gimp *, const Request &) = 0;
};
class Router
{
public:
  explicit Router (Navigation navigation = {});
  ~Router ();
  Response dispatch (Gimp *, const Request &);

private:
  struct Rule;
  std::vector<std::unique_ptr<Rule> > rules_;
};
std::unique_ptr<Resource> images_resource ();
std::unique_ptr<Resource> pdb_resource ();
}
}
#endif
