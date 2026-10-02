/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gegl.h>
#include <libsoup/soup.h>
#include <algorithm>
#include <cstring>
#include <map>
#include "httpd.h"
#include "httpd-private.hpp"
#include "painter/binding-store.hpp"
#include "painter/source.hpp"
extern "C"
{
#include "core/gimp.h"
}
struct GimpPainterHttpd
{
  GObject  parent_instance;
  gboolean binding_failed;
};
struct GimpPainterHttpdClass
{
  GObjectClass parent_class;
};
G_DEFINE_TYPE (GimpPainterHttpd, gimp_painter_httpd, G_TYPE_OBJECT)
namespace GimpPainter
{
template <> struct TypeTraits<GimpPainterHttpd>
{
  static GType
  type () noexcept
  {
    return GIMP_TYPE_PAINTER_HTTPD;
  }
};
}
using namespace GimpPainter;
using namespace GimpPainter::Http;
namespace
{
constexpr goffset max_body = 16 * 1024 * 1024;
struct Hook
{
  ObjectRef<GObject> cancel = ObjectRef<GObject>::adopt (G_OBJECT (g_cancellable_new ()));
  Source deadline;
};
struct Pending
{
  ObjectRef<GObject> message;
  Source             source;
};
struct State : std::enable_shared_from_this<State>
{
  WeakRef<GObject>        application;
  ObjectRef<GObject>      server, session, cancellable;
  GMainContext           *main_context = g_main_context_ref_thread_default ();
  std::shared_ptr<Router> router;
  std::map<guint64, std::shared_ptr<Pending> > pending;
  struct ActiveGuide
  {
    guint64                generation;
    std::unique_ptr<Guide> guide;
  };
  std::vector<ActiveGuide>            guides;
  std::vector<std::shared_ptr<Hook> > hooks;
  GuideFactory                        guide_factory;
  std::string                         token, webhook_origin;
  guint                               port = 0;
  guint64                             next = 0, guide_generation = 0;
  bool                                closed = false;
  ~State ()
  {
    close ();
    g_main_context_unref (main_context);
  }
  void
  close () noexcept
  {
    if (closed)
      return;
    closed = true;
    ++guide_generation;
    auto old_guides = std::move (guides);
    for (auto &entry : old_guides)
      entry.guide->close ();
    old_guides.clear ();
    guide_factory = {};
    if (cancellable)
      g_cancellable_cancel (G_CANCELLABLE (cancellable.get ()));
    for (auto &hook : hooks)
      {
        g_cancellable_cancel (G_CANCELLABLE (hook->cancel.get ()));
        hook->deadline.close ();
      }
    hooks.clear ();
    auto requests = std::move (pending);
    for (auto &entry : requests)
      {
        entry.second->source.close ();
        auto *m = SOUP_SERVER_MESSAGE (entry.second->message.get ());
        soup_server_message_set_status (m, 503, nullptr);
        soup_server_message_unpause (m);
      }
    if (server)
      soup_server_disconnect (SOUP_SERVER (server.get ()));
    server.reset ();
    if (session)
      soup_session_abort (SOUP_SESSION (session.get ()));
    session.reset ();
    router.reset ();
    token.clear ();
  }
  static void
  respond (SoupServerMessage *m, const Response &r)
  {
    soup_server_message_set_status (m, r.status, nullptr);
    soup_server_message_set_response (m, r.content_type.c_str (), SOUP_MEMORY_COPY,
                                      r.body.data (), r.body.size ());
    auto *h = soup_server_message_get_response_headers (m);
    soup_message_headers_replace (h, "Cache-Control", "no-store");
    soup_message_headers_replace (h, "X-Content-Type-Options", "nosniff");
  }
  bool
  authorize (SoupServerMessage *m)
  {
    auto       *h      = soup_server_message_get_request_headers (m);
    auto       *uri    = soup_server_message_get_uri (m);
    const char *host   = g_uri_get_host (uri);
    auto       *origin = soup_message_headers_get_one (h, "Origin");
    if (! host || std::strcmp (host, "127.0.0.1") != 0 || origin)
      {
        respond (m, error (403, "Only non-browser numeric loopback clients are "
                                "allowed"));
        return false;
      }
    const char *auth       = soup_message_headers_get_one (h, "Authorization");
    std::string expected   = "Bearer " + token;
    unsigned    difference = auth ? std::strlen (auth) ^ expected.size () : 1;
    if (auth && std::strlen (auth) == expected.size ())
      for (size_t i = 0; i < expected.size (); ++i)
        difference |= static_cast<unsigned char> (auth[i]) ^
                      static_cast<unsigned char> (expected[i]);
    if (difference)
      {
        respond (m, error (401, "Bearer token required"));
        return false;
      }
    const char *method = soup_server_message_get_method (m);
    bool mutation = std::strcmp (method, "POST") == 0 || std::strcmp (method, "PUT") == 0;
    if (soup_message_headers_get_one (h, "Transfer-Encoding") ||
        (mutation && ! soup_message_headers_get_one (h, "Content-Length")))
      {
        respond (m, error (411, "A bounded Content-Length is required"));
        return false;
      }
    if (soup_message_headers_get_content_length (h) > max_body)
      {
        respond (m, error (413, "Request body exceeds 16 MiB"));
        return false;
      }
    return true;
  }
  static void
  early (SoupServer *, SoupServerMessage *m, const char *, GHashTable *, gpointer data) noexcept
  {
    try
      {
        auto s = static_cast<std::weak_ptr<State> *> (data)->lock ();
        if (! s || s->closed)
          {
            respond (m, error (503, "Service stopped"));
            return;
          }
        if (! s->authorize (m))
          // Soup may drain a rejected non-Expect request before its response.
          // Never accumulate that untrusted/rejected body while it does so.
          soup_message_body_set_accumulate (soup_server_message_get_request_body (m), FALSE);
      }
    catch (...)
      {
        soup_server_message_set_status (m, 500, nullptr);
      }
  }
  static void
  handle (SoupServer *, SoupServerMessage *m, const char *path, GHashTable *, gpointer data) noexcept
  {
    try
      {
        auto s = static_cast<std::weak_ptr<State> *> (data)->lock ();
        if (! s || s->closed)
          {
            respond (m, error (503, "Service stopped"));
            return;
          }
        if (! s->authorize (m))
          return;
        if (s->pending.size () >= 64)
          {
            respond (m, error (503, "Request queue full"));
            return;
          }
        Request r;
        r.method       = soup_server_message_get_method (m);
        r.path         = path;
        auto *h        = soup_server_message_get_request_headers (m);
        auto *type     = soup_message_headers_get_content_type (h, nullptr);
        r.content_type = type ? type : "";
        auto *b        = soup_server_message_get_request_body (m);
        if (b->length > max_body)
          {
            respond (m, error (413, "Request body exceeds limit"));
            return;
          }
        if (r.content_type.compare (0, 10, "multipart/") == 0)
          {
            auto *bytes = soup_message_body_flatten (b);
            std::unique_ptr<SoupMultipart, decltype (&soup_multipart_free)> multipart (
                soup_multipart_new_from_message (h, bytes), soup_multipart_free);
            g_bytes_unref (bytes);
            if (! multipart)
              {
                respond (m, error (400, "Invalid multipart body"));
                return;
              }
            bool found = false;
            for (int i = 0; i < soup_multipart_get_length (multipart.get ()); ++i)
              {
                SoupMessageHeaders *ph = nullptr;
                GBytes             *pb = nullptr;
                if (! soup_multipart_get_part (multipart.get (), i, &ph, &pb))
                  continue;
                auto *mt = soup_message_headers_get_content_type (ph, nullptr);
                if (mt && g_str_has_prefix (mt, "image/"))
                  {
                    gsize size     = 0;
                    auto *data     = g_bytes_get_data (pb, &size);
                    r.content_type = mt;
                    r.body.assign (static_cast<const char *> (data), size);
                    found = true;
                    break;
                  }
              }
            if (! found)
              {
                respond (m, error (415, "Multipart needs an image part"));
                return;
              }
          }
        else
          r.body.assign (b->data ? b->data : "", b->length);
        auto p                    = std::make_shared<Pending> ();
        p->message                = ObjectRef<GObject>::retain (G_OBJECT (m));
        auto                 id   = ++s->next;
        std::weak_ptr<State> weak = s;
        s->pending.emplace (id, p);
        soup_server_message_pause (m);
        try
          {
            p->source = Source::idle (s->main_context, G_PRIORITY_DEFAULT_IDLE, [weak, id, r] {
              auto state = weak.lock ();
              if (! state || state->closed)
                return false;
              auto it = state->pending.find (id);
              if (it == state->pending.end ())
                return false;
              auto request = it->second;
              auto app     = state->application.lock ();
              auto router  = state->router;
              Response response = app ? router->dispatch (GIMP (app.get ()), r) :
                                        error (503, "Application closed");
              if (! state->closed)
                {
                  respond (SOUP_SERVER_MESSAGE (request->message.get ()), response);
                  soup_server_message_unpause (
                      SOUP_SERVER_MESSAGE (request->message.get ()));
                  state->pending.erase (id);
                }
              return false;
            });
          }
        catch (...)
          {
            s->pending.erase (id);
            soup_server_message_set_status (m, 500, nullptr);
            soup_server_message_unpause (m);
            throw;
          }
      }
    catch (...)
      {
        soup_server_message_set_status (m, 500, nullptr);
      }
  }
  Response
  navigation (Gimp *g, const Request &r)
  {
    if (! guide_factory || g->no_interface)
      throw Failure (503, "Navigation requires GUI");
    auto n = parse (r.body);
    auto c = context (g, member (n.get (), "context"));
    if (! c.display)
      throw Failure (404, "No active display");
    auto *a       = member (n.get (), "arguments");
    auto  message = text (member (a, "message"));
    if (message.size () > 65536)
      throw Failure (413, "Navigation message too long");
    std::string hook;
    auto       *hook_node = member (a, "webhook_uri", false);
    if (hook_node && ! JSON_NODE_HOLDS_NULL (hook_node))
      hook = text (hook_node);
    if (! hook.empty ())
      {
        GError *e       = nullptr;
        auto   *u       = g_uri_parse (hook.c_str (), G_URI_FLAGS_NONE, &e);
        bool    allowed = u && g_strcmp0 (g_uri_get_scheme (u), "http") == 0 &&
                       g_strcmp0 (g_uri_get_host (u), "127.0.0.1") == 0 &&
                       ! g_uri_get_userinfo (u) && ! g_uri_get_fragment (u);
        std::string origin = u ? "http://127.0.0.1:" +
                                     std::to_string (g_uri_get_port (u) > 0 ?
                                                         g_uri_get_port (u) :
                                                         80) :
                                 "";
        if (u)
          g_uri_unref (u);
        g_clear_error (&e);
        if (! allowed || webhook_origin.empty () || origin != webhook_origin)
          throw Failure (403, "Webhook origin is not explicitly allowed");
      }
    auto                 generation = ++guide_generation;
    std::weak_ptr<State> weak       = shared_from_this ();
    auto guide = guide_factory (g, c, message, [weak, generation, hook] (std::string body) {
      auto s = weak.lock ();
      if (! s || s->closed ||
          std::none_of (s->guides.begin (), s->guides.end (),
                        [generation] (const ActiveGuide &entry) {
                          return entry.generation == generation;
                        }) ||
          hook.empty ())
        return;
      auto *m = soup_message_new ("POST", hook.c_str ());
      if (! m)
        return;
      soup_message_set_flags (m, SOUP_MESSAGE_NO_REDIRECT);
      auto *bytes = g_bytes_new (body.data (), body.size ());
      soup_message_set_request_body_from_bytes (m, "application/json", bytes);
      g_bytes_unref (bytes);
      auto hook = std::make_shared<Hook> ();
      s->hooks.push_back (hook);
      std::weak_ptr<Hook> weak_hook = hook;
      hook->deadline = Source::timeout (s->main_context, 10000, G_PRIORITY_DEFAULT, [weak_hook] {
        if (auto h = weak_hook.lock ())
          g_cancellable_cancel (G_CANCELLABLE (h->cancel.get ()));
        return false;
      });
      using Callback = std::pair<std::weak_ptr<State>, std::shared_ptr<Hook> >;
      soup_session_send_async (
          SOUP_SESSION (s->session.get ()), m, G_PRIORITY_DEFAULT,
          G_CANCELLABLE (hook->cancel.get ()),
          [] (GObject *o, GAsyncResult *r, gpointer data) {
            std::unique_ptr<Callback> callback (static_cast<Callback *> (data));
            callback->second->deadline.close ();
            GError *e      = nullptr;
            auto   *stream = soup_session_send_finish (SOUP_SESSION (o), r, &e);
            if (stream)
              {
                g_input_stream_close (stream, nullptr, nullptr);
                g_object_unref (stream);
              }
            g_clear_error (&e);
            if (auto s = callback->first.lock ())
              {
                auto &list = s->hooks;
                list.erase (std::remove (list.begin (), list.end (), callback->second),
                            list.end ());
              }
          },
          new Callback (s, hook));
      g_object_unref (m);
    });
    for (auto it = guides.begin (); it != guides.end ();)
      {
        if (! it->guide->active () || it->guide->scope () == guide->scope ())
          {
            it->guide->close ();
            it = guides.erase (it);
          }
        else
          ++it;
      }
    guides.push_back ({ generation, std::move (guide) });
    auto out = object ();
    set (out.get (), "result", boolean (true));
    set (out.get (), "context", context_json (c));
    if (auto *id = member (a, "message_id", false))
      set (out.get (), "message_id", Json (json_node_copy (id)));
    return reply (std::move (out), 202);
  }
};
struct Service
{
  std::shared_ptr<State> state = std::make_shared<State> ();
  void
  close () noexcept
  {
    state->close ();
  }
};
struct ServiceSlot : SlotSpec<GimpPainterHttpd, Service>
{
};
std::shared_ptr<State>
state (GObject *o)
{
  if (! G_TYPE_CHECK_INSTANCE_TYPE (o, GIMP_TYPE_PAINTER_HTTPD))
    throw Error (GIMP_PAINTER_ERROR_WRONG_TYPE, "Not an HTTP service");
  return BindingStore::require (o).with<ServiceSlot> ([] (Service &s) {
    return s.state;
  });
}
void
dispose (GObject *o)
{
  boundary_void (nullptr, [&] {
    if (auto *b = BindingStore::find (o))
      b->close ();
  });
  G_OBJECT_CLASS (gimp_painter_httpd_parent_class)->dispose (o);
}
}
static void
gimp_painter_httpd_class_init (GimpPainterHttpdClass *c)
{
  G_OBJECT_CLASS (c)->dispose = dispose;
}
static void
gimp_painter_httpd_init (GimpPainterHttpd *o)
{
  o->binding_failed = ! boundary<bool> (nullptr, false, [&] {
    auto &b = BindingStore::ensure (G_OBJECT (o));
    b.emplace<ServiceSlot> ();
    b.activate ();
    return true;
  });
}
GObject *
gimp_painter_httpd_start (Gimp *g, guint port, const gchar *token,
                          const gchar *origin, GError **e)
{
  return boundary<GObject *> (e, nullptr, [&] {
    if (! GIMP_IS_GIMP (g) || port > 65535 || ! token || strlen (token) < 32 ||
        strlen (token) > 256)
      throw Error (GIMP_PAINTER_ERROR_INVALID_STATE,
                   "HTTP requires an application, a port, and a 32..256 "
                   "character token");
    for (auto *p = token; *p; ++p)
      if (! g_ascii_isalnum (*p) && *p != '-' && *p != '_')
        throw Error (GIMP_PAINTER_ERROR_INVALID_STATE,
                     "HTTP token must be URL-safe");
    auto owner = ObjectRef<GObject>::adopt (
        G_OBJECT (g_object_new (GIMP_TYPE_PAINTER_HTTPD, nullptr)));
    auto s = state (owner.get ());
    s->application = WeakRef<GObject> (ObjectRef<GObject>::retain (G_OBJECT (g)));
    s->token          = token;
    s->webhook_origin = origin ? origin : "";
    s->server         = ObjectRef<GObject>::adopt (
        G_OBJECT (soup_server_new ("server-header", "GIMP Painter", nullptr)));
    s->session = ObjectRef<GObject>::adopt (
        G_OBJECT (soup_session_new_with_options ("timeout", 10, nullptr)));
    soup_session_set_proxy_resolver (SOUP_SESSION (s->session.get ()), nullptr);
    s->cancellable = ObjectRef<GObject>::adopt (G_OBJECT (g_cancellable_new ()));
    std::weak_ptr<State> weak = s;
    s->router = std::make_shared<Router> ([weak] (Gimp *g, const Request &r) {
      auto s = weak.lock ();
      if (! s || s->closed)
        throw Failure (503, "Service stopped");
      return s->navigation (g, r);
    });
    auto destroy = [] (gpointer p) {
      delete static_cast<std::weak_ptr<State> *> (p);
    };
    soup_server_add_early_handler (SOUP_SERVER (s->server.get ()), nullptr,
                                   State::early, new std::weak_ptr<State> (s), destroy);
    soup_server_add_handler (SOUP_SERVER (s->server.get ()), nullptr,
                             State::handle, new std::weak_ptr<State> (s), destroy);
    if (! soup_server_listen_local (SOUP_SERVER (s->server.get ()), port,
                                    SOUP_SERVER_LISTEN_IPV4_ONLY, e))
      return static_cast<GObject *> (nullptr);
    auto *uris = soup_server_get_uris (SOUP_SERVER (s->server.get ()));
    s->port    = uris ? g_uri_get_port (static_cast<GUri *> (uris->data)) : 0;
    g_slist_free_full (uris, reinterpret_cast<GDestroyNotify> (g_uri_unref));
    return owner.release ();
  });
}
void
gimp_painter_httpd_stop (GObject *o)
{
  boundary_void (nullptr, [&] {
    if (auto *b = BindingStore::find (o))
      b->close ();
  });
}
guint
gimp_painter_httpd_port (GObject *o)
{
  return boundary<guint> (nullptr, 0, [&] { return state (o)->port; });
}
namespace GimpPainter
{
namespace Http
{
guint
pending_count (GObject *o)
{
  return boundary<guint> (nullptr, 0, [&] { return state (o)->pending.size (); });
}
void
set_guide_factory (GObject *o, GuideFactory f)
{
  state (o)->guide_factory = std::move (f);
}
}
}
GObject *
gimp_painter_httpd_from_environment (Gimp *g, GError **e)
{
  if (g_strcmp0 (g_getenv ("GIMP_PAINTER_HTTP_ENABLE"), "1") != 0)
    return nullptr;
  const char *port = g_getenv ("GIMP_PAINTER_HTTP_PORT");
  guint64     n    = 8920;
  if (port)
    {
      char *end = nullptr;
      n         = g_ascii_strtoull (port, &end, 10);
      if (! *port || *end || n > 65535 || ! n)
        {
          g_set_error_literal (e, GIMP_PAINTER_ERROR, GIMP_PAINTER_ERROR_INVALID_STATE,
                               "Invalid HTTP port");
          return nullptr;
        }
    }
  return gimp_painter_httpd_start (
      g, n, g_getenv ("GIMP_PAINTER_HTTP_TOKEN"),
      g_getenv ("GIMP_PAINTER_HTTP_WEBHOOK_ORIGIN"), e);
}
