/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gegl.h>
#include <gtk/gtk.h>
#include <libsoup/soup.h>
#include <string>
extern "C"
{
#include "libgimpbase/gimpbase.h"
#include "core/core-types.h"
#include "widgets/widgets-types.h"
#include "core/gimp-gui.h"
#include "core/gimp.h"
#include "core/gimpcontext.h"
#include "core/gimpimage.h"
#include "core/gimpitem.h"
#include "core/gimplayer-new.h"
#include "core/gimplayer.h"
#include "display/display-types.h"
#include "display/gimpdisplay.h"
#include "gimp-app-test-utils.h"
#include "gimpcoreapp.h"
#include "httpd/httpd.h"
#include "tests.h"
}
#include "httpd/httpd-resource.hpp"
#include "painter/object-ref.hpp"
using namespace GimpPainter;
using namespace GimpPainter::Http;
static Gimp       *gimp;
static const char *token = "test-only-random-token-0123456789abcdef";
static void
drain ()
{
  for (int i = 0; i < 50; ++i)
    {
      while (g_main_context_pending (nullptr))
        g_main_context_iteration (nullptr, FALSE);
      g_usleep (1000);
    }
}
struct HttpResult
{
  bool        done   = false;
  unsigned    status = 0;
  std::string body;
  GError     *error = nullptr;
};
static HttpResult
post (guint port, const std::string &body)
{
  auto *s = soup_session_new_with_options ("proxy-resolver", nullptr, "timeout", 5, nullptr);
  auto uri = "http://127.0.0.1:" + std::to_string (port) + "/api/v1/navigation";
  auto       *m    = soup_message_new ("POST", uri.c_str ());
  std::string auth = "Bearer " + std::string (token);
  soup_message_headers_append (soup_message_get_request_headers (m),
                               "Authorization", auth.c_str ());
  auto *b = g_bytes_new (body.data (), body.size ());
  soup_message_set_request_body_from_bytes (m, "application/json", b);
  g_bytes_unref (b);
  HttpResult out;
  soup_session_send_and_read_async (
      s, m, G_PRIORITY_DEFAULT, nullptr,
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
      &out);
  while (! out.done)
    g_main_context_iteration (nullptr, TRUE);
  out.status = soup_message_get_status (m);
  g_object_unref (m);
  g_object_unref (s);
  g_assert_no_error (out.error);
  return out;
}
static GtkWidget *
guide ()
{
  auto      *list  = gtk_window_list_toplevels ();
  GtkWidget *found = nullptr;
  for (auto *l = list; l; l = l->next)
    if (g_strcmp0 (gtk_widget_get_name (GTK_WIDGET (l->data)),
                   "gimp-painter-http-navigation") == 0)
      found = GTK_WIDGET (l->data);
  g_list_free (list);
  return found;
}
struct Fixture
{
  ObjectRef<GObject> image, service;
  GimpDisplay       *display = nullptr;
  GimpLayer         *layer   = nullptr;
  Fixture (const char *origin = nullptr)
  {
    gimp_set_focused_once (gimp);
    image   = ObjectRef<GObject>::adopt (G_OBJECT (
        gimp_image_new (gimp, 32, 24, GIMP_RGB, GIMP_PRECISION_U8_NON_LINEAR)));
    auto *i = GIMP_IMAGE (image.get ());
    layer   = gimp_layer_new (i, 32, 24, babl_format ("R'G'B'A u8"),
                              "Initial selection", 1., GIMP_LAYER_MODE_NORMAL);
    g_assert_true (gimp_image_add_layer (i, layer, nullptr, 0, FALSE));
    display = gimp_create_display (gimp, i, gimp_unit_pixel (), 1., nullptr);
    g_assert_nonnull (display);
    GError *e = nullptr;
    service = ObjectRef<GObject>::adopt (gimp_painter_httpd_start (gimp, 0, token,
                                                                   origin, &e));
    g_assert_no_error (e);
    g_assert_nonnull (service.get ());
    gimp_painter_httpd_enable_navigation (service.get ());
    drain ();
  }
  ~Fixture ()
  {
    if (service)
      gimp_painter_httpd_stop (service.get ());
    if (display)
      gimp_display_delete (display);
    drain ();
  }
  std::string
  payload (const std::string &hook = "")
  {
    return "{\"context\":{\"image\":" +
           std::to_string (gimp_image_get_id (GIMP_IMAGE (image.get ()))) +
           ",\"display\":" + std::to_string (gimp_display_get_id (display)) +
           "},\"arguments\":{\"message\":\"Keep painting, then press "
           "OK\",\"message_id\":\"guide-1\",\"webhook_uri\":\"" +
           hook + "\"}}";
  }
  HttpResult
  show (const std::string &hook = "")
  {
    auto out = post (gimp_painter_httpd_port (service.get ()), payload (hook));
    if (out.status != 202)
      g_error ("Navigation %u: %s", out.status, out.body.c_str ());
    drain ();
    return out;
  }
};
static void
nonmodal_replace_cancel ()
{
  Fixture f;
  auto    out = f.show ();
  g_assert_nonnull (guide ());
  g_assert_false (gtk_window_get_modal (GTK_WINDOW (guide ())));
  auto parsed = parse (out.body);
  g_assert_true (text (member (parsed.get (), "message_id")) == "guide-1");
  bool destroyed = false;
  g_signal_connect (guide (), "destroy", G_CALLBACK (+[] (GtkWidget *, gpointer p) {
                      *static_cast<bool *> (p) = true;
                    }),
                    &destroyed);
  f.show ();
  g_assert_true (destroyed);
  g_assert_nonnull (guide ());
  gtk_dialog_response (GTK_DIALOG (guide ()), GTK_RESPONSE_DELETE_EVENT);
  drain ();
  g_assert_null (guide ());
  f.show ();
  gtk_dialog_response (GTK_DIALOG (guide ()), GTK_RESPONSE_OK);
  drain ();
  g_assert_null (guide ());
}
struct Receiver
{
  guint       count = 0;
  std::string body;
};
static void
webhook_context_once ()
{
  Receiver receiver;
  auto    *server = soup_server_new (nullptr, nullptr);
  soup_server_add_handler (
      server, nullptr,
      [] (SoupServer *, SoupServerMessage *m, const char *, GHashTable *, gpointer p) {
        auto *r = static_cast<Receiver *> (p);
        ++r->count;
        auto *b = soup_server_message_get_request_body (m);
        r->body.assign (b->data, b->length);
        soup_server_message_set_status (m, 204, nullptr);
      },
      &receiver, nullptr);
  GError *e = nullptr;
  g_assert_true (soup_server_listen_local (server, 0, SOUP_SERVER_LISTEN_IPV4_ONLY, &e));
  g_assert_no_error (e);
  auto       *uris   = soup_server_get_uris (server);
  std::string origin = "http://127.0.0.1:" +
                       std::to_string (g_uri_get_port (static_cast<GUri *> (uris->data)));
  g_slist_free_full (uris, reinterpret_cast<GDestroyNotify> (g_uri_unref));
  {
    Fixture f (origin.c_str ());
    f.show (origin + "/completed");
    auto  initial = gimp_item_get_id (GIMP_ITEM (f.layer));
    auto *i       = GIMP_IMAGE (f.image.get ());
    auto *other   = gimp_layer_new (i, 8, 8, babl_format ("R'G'B'A u8"),
                                    "Second selection", 1, GIMP_LAYER_MODE_NORMAL);
    gimp_image_add_layer (i, other, nullptr, 0, FALSE);
    gimp_image_flush (i);
    gtk_dialog_response (GTK_DIALOG (guide ()), GTK_RESPONSE_OK);
    for (int j = 0; j < 100 && receiver.count == 0; ++j)
      drain ();
    g_assert_cmpuint (receiver.count, ==, 1);
    auto body = parse (receiver.body);
    g_assert_cmpint (
        integer (member (member (body.get (), "context"), "drawable")), ==, initial);
    drain ();
    g_assert_cmpuint (receiver.count, ==, 1);
    auto denied = post (gimp_painter_httpd_port (f.service.get ()),
                        f.payload ("http://example.invalid/callback"));
    g_assert_cmpuint (denied.status, ==, 403);
    g_assert_null (guide ());
    f.show (origin + "/cancelled");
    gtk_dialog_response (GTK_DIALOG (guide ()), GTK_RESPONSE_CANCEL);
    drain ();
    g_assert_cmpuint (receiver.count, ==, 1);
    f.show (origin + "/shutdown");
    gimp_painter_httpd_stop (f.service.get ());
    drain ();
    g_assert_null (guide ());
    g_assert_cmpuint (receiver.count, ==, 1);
  }
  soup_server_disconnect (server);
  g_object_unref (server);
}
static void
inflight_hook_shutdown ()
{
  struct Paused
  {
    guint              calls = 0;
    ObjectRef<GObject> message;
  } paused;
  auto *server = soup_server_new (nullptr, nullptr);
  soup_server_add_handler (
      server, nullptr,
      [] (SoupServer *, SoupServerMessage *message, const char *, GHashTable *, gpointer data) {
        auto *state = static_cast<Paused *> (data);
        ++state->calls;
        state->message = ObjectRef<GObject>::retain (G_OBJECT (message));
        soup_server_message_pause (message);
      },
      &paused, nullptr);
  GError *error = nullptr;
  g_assert_true (soup_server_listen_local (server, 0, SOUP_SERVER_LISTEN_IPV4_ONLY, &error));
  g_assert_no_error (error);
  auto *uris   = soup_server_get_uris (server);
  auto  origin = "http://127.0.0.1:" +
                std::to_string (g_uri_get_port (static_cast<GUri *> (uris->data)));
  g_slist_free_full (uris, reinterpret_cast<GDestroyNotify> (g_uri_unref));
  {
    Fixture fixture (origin.c_str ());
    fixture.show (origin + "/pending");
    gtk_dialog_response (GTK_DIALOG (guide ()), GTK_RESPONSE_OK);
    for (int i = 0; i < 40 && paused.calls == 0; ++i)
      drain ();
    g_assert_cmpuint (paused.calls, ==, 1);
    bool owner_gone = false;
    g_object_weak_ref (
        fixture.service.get (),
        [] (gpointer data, GObject *) { *static_cast<bool *> (data) = true; }, &owner_gone);
    gimp_painter_httpd_stop (fixture.service.get ());
    fixture.service.reset ();
    g_assert_true (owner_gone);
    drain (); // Dispatch the cancelled async completion after owner destruction.
    g_assert_null (guide ());
    g_assert_cmpuint (paused.calls, ==, 1);
  }
  soup_server_disconnect (server);
  paused.message.reset ();
  g_object_unref (server);
  drain ();
}
static void
display_close ()
{
  Fixture f;
  f.show ();
  gimp_display_delete (f.display);
  f.display = nullptr;
  drain ();
  g_assert_null (guide ());
  gimp_painter_httpd_stop (f.service.get ());
  drain ();
}
static guint
guide_count ()
{
  guint count = 0;
  auto *list  = gtk_window_list_toplevels ();
  for (auto *l = list; l; l = l->next)
    if (g_strcmp0 (gtk_widget_get_name (GTK_WIDGET (l->data)),
                   "gimp-painter-http-navigation") == 0)
      ++count;
  g_list_free (list);
  return count;
}
static void
distinct_windows_keep_guides ()
{
  gboolean original = FALSE;
  g_object_get (gimp->config, "single-window-mode", &original, nullptr);
  g_object_set (gimp->config, "single-window-mode", FALSE, nullptr);
  {
    Fixture first, second;
    first.show ();
    auto *first_guide = guide ();
    auto response = post (gimp_painter_httpd_port (first.service.get ()), second.payload ());
    g_assert_cmpuint (response.status, ==, 202);
    g_assert_cmpuint (guide_count (), ==, 2);
    gtk_dialog_response (GTK_DIALOG (first_guide), GTK_RESPONSE_OK);
    drain ();
    g_assert_cmpuint (guide_count (), ==, 1);
    gimp_display_delete (second.display);
    second.display = nullptr;
    drain ();
    g_assert_cmpuint (guide_count (), ==, 0);
    // Native single-window reconfiguration requires an active display. Restore
    // while the first fixture is alive, before its destructor closes the last.
    gimp_context_set_display (gimp_get_user_context (gimp), first.display);
    g_object_set (gimp->config, "single-window-mode", original, nullptr);
  }
}
static void
empty_display_cancels_guide ()
{
  Fixture f;
  f.show ();
  gimp_display_empty (f.display);
  drain ();
  g_assert_null (guide ());
}
int
main (int argc, char **argv)
{
  g_test_init (&argc, &argv, nullptr);
  if (! gtk_init_check (&argc, &argv))
    return GIMP_EXIT_TEST_SKIPPED;
  gimp_test_utils_setup_menus_path ();
  gimp = gimp_init_for_gui_testing (TRUE);
#define ADD(f) g_test_add_func ("/painter-http-ui/" #f, f)
  ADD (nonmodal_replace_cancel);
  ADD (webhook_context_once);
  ADD (inflight_hook_shutdown);
  ADD (display_close);
  ADD (distinct_windows_keep_guides);
  ADD (empty_display_cancels_guide);
  g_application_run (gimp->app, 0, nullptr);
  int result = gimp_core_app_get_exit_status (GIMP_CORE_APP (gimp->app));
  g_application_quit (gimp->app);
  g_clear_object (&gimp->app);
  return result;
}
