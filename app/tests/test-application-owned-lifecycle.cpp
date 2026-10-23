/* SPDX-License-Identifier: GPL-3.0-or-later
 * Each registered case runs in a fresh GLib subprocess, never disposing a
 * shared Gimp and continuing unrelated tests. Existing preset/HTTP suites
 * own resource semantics, action behavior, transport and GUI coverage.
 */
#include "config.h"
#include <gegl.h>
#include <gtk/gtk.h>
#include <libsoup/soup.h>
#include <glib/gstdio.h>
#include <string>
#include <memory>
#include <cstring>
#include <functional>
extern "C" {
#include "libgimpbase/gimpbase.h"
#include "libgimpconfig/gimpconfig.h"
#include "core/core-types.h"
#include "config/gimprc.h"
#include "core/gimp.h"
#include "core/gimpcontainer.h"
#include "core/gimpcontext.h"
#include "core/gimpdatafactory.h"
#include "core/gimplayerpreset.h"
#include "core/gimpfilterexit.h"
#include "core/gimp-gui.h"
#include "core/gimpimage.h"
#include "core/gimplayer-new.h"
#include "core/gimplayer.h"
#include "display/display-types.h"
#include "display/gimpdisplay.h"
#include "gimp-app-test-utils.h"
#include "httpd/httpd.h"
Gimp *gimp_test_application_owned_start (const gchar *, GApplication **, gboolean);
void gimp_test_application_owned_release (Gimp *, GApplication *);
}
#include "httpd/httpd-private.hpp"
using GimpPainter::Http::pending_count;
static const char *token = "test-only-random-token-0123456789abcdef";

struct Witness {
  bool finalized = false;
  explicit Witness (gpointer object) {
    g_object_weak_ref (G_OBJECT (object), [](gpointer p, GObject *) {
      static_cast<Witness *> (p)->finalized = true;
    }, this);
  }
};
static void drain () {
  const auto until = g_get_monotonic_time () + 100000;
  while (g_get_monotonic_time () < until) {
    while (g_main_context_pending (nullptr))
      g_main_context_iteration (nullptr, FALSE);
    g_usleep (1000);
  }
}
struct Pending {
  SoupSession *session;
  SoupMessage *message;
  bool done = false;
  GError *error = nullptr;
  explicit Pending (guint port, const char *method = "PUT",
                    const std::string &path = "/api/v1/images/",
                    const std::string &body = "{\"boundary\":[0,0,4,4]}") {
    session = soup_session_new_with_options ("proxy-resolver", nullptr,
                                             "timeout", 5, nullptr);
    const auto uri = "http://127.0.0.1:" + std::to_string (port) + path;
    message = soup_message_new (method, uri.c_str ());
    const auto auth = std::string ("Bearer ") + token;
    soup_message_headers_append (soup_message_get_request_headers (message),
                                 "Authorization", auth.c_str ());
    auto *bytes = g_bytes_new (body.data (), body.size ());
    soup_message_set_request_body_from_bytes (message, "application/json", bytes);
    g_bytes_unref (bytes);
    soup_session_send_and_read_async (session, message, G_PRIORITY_DEFAULT, nullptr,
      [](GObject *source, GAsyncResult *result, gpointer data) {
        auto *self = static_cast<Pending *> (data);
        auto *bytes = soup_session_send_and_read_finish (SOUP_SESSION (source), result,
                                                        &self->error);
        if (bytes) g_bytes_unref (bytes);
        self->done = true;
      }, this);
  }
  void queued (GObject *service) {
    const auto until = g_get_monotonic_time () + 5 * G_USEC_PER_SEC;
    while (!pending_count (service) && !done && g_get_monotonic_time () < until) {
      g_main_context_iteration (nullptr, FALSE); g_usleep (100);
    }
    g_assert_cmpuint (pending_count (service), ==, 1);
  }
  void finish (guint expected = 0) {
    const auto until = g_get_monotonic_time () + 6 * G_USEC_PER_SEC;
    while (!done && g_get_monotonic_time () < until) {
      g_main_context_iteration (nullptr, FALSE); g_usleep (100);
    }
    g_assert_true (done);
    if (expected) { g_assert_no_error (error); g_assert_cmpuint (soup_message_get_status (message), ==, expected); }
    else g_assert_true (error || soup_message_get_status (message) == 503);
  }
  ~Pending () {
    g_assert_true (done); g_clear_error (&error);
    g_object_unref (message); g_object_unref (session);
  }
};
static guint available_port () {
  GError *error = nullptr;
  auto *listener = g_socket_listener_new ();
  auto *address = g_inet_socket_address_new_from_string ("127.0.0.1", 0);
  GSocketAddress *effective = nullptr;
  g_assert_true (g_socket_listener_add_address (listener, address,
    G_SOCKET_TYPE_STREAM, G_SOCKET_PROTOCOL_TCP, nullptr, &effective, &error));
  g_assert_no_error (error);
  const auto port = g_inet_socket_address_get_port (G_INET_SOCKET_ADDRESS (effective));
  g_object_unref (effective); g_object_unref (address); g_object_unref (listener);
  /* The normal environment API requires a nonzero port. A competing bind
   * causes a real test failure, never a fallback to a local test service. */
  return port;
}
struct Navigation {
  SoupServer *server = soup_server_new (nullptr, nullptr);
  SoupServerMessage *paused = nullptr;
  GtkWidget *retained_guide = nullptr;
  guint deliveries = 0;
  bool destroyed = false;
  bool response_terminal = false;
  std::string origin;
  Navigation () {
    soup_server_add_handler (server, nullptr,
      [](SoupServer *, SoupServerMessage *message, const char *, GHashTable *, gpointer data) {
        auto *self = static_cast<Navigation *> (data);
        ++self->deliveries;
        g_assert_null (self->paused);
        self->paused = SOUP_SERVER_MESSAGE (g_object_ref (message));
        g_signal_connect (message, "finished", G_CALLBACK (+[](SoupServerMessage *, gpointer p) {
          static_cast<Navigation *> (p)->response_terminal = true;
        }), self);
        g_signal_connect (message, "disconnected", G_CALLBACK (+[](SoupServerMessage *, gpointer p) {
          static_cast<Navigation *> (p)->response_terminal = true;
        }), self);
        soup_server_message_pause (message);
      }, this, nullptr);
    GError *error = nullptr;
    g_assert_true (soup_server_listen_local (server, 0, SOUP_SERVER_LISTEN_IPV4_ONLY, &error));
    g_assert_no_error (error);
    auto *uris = soup_server_get_uris (server);
    origin = "http://127.0.0.1:" + std::to_string (g_uri_get_port (static_cast<GUri *> (uris->data)));
    g_slist_free_full (uris, reinterpret_cast<GDestroyNotify> (g_uri_unref));
    g_setenv ("GIMP_PAINTER_HTTP_WEBHOOK_ORIGIN", origin.c_str (), TRUE);
  }
  static GtkWidget *guide () {
    auto *list = gtk_window_list_toplevels ();
    GtkWidget *found = nullptr;
    for (auto *it = list; it; it = it->next)
      if (g_strcmp0 (gtk_widget_get_name (GTK_WIDGET (it->data)),
                     "gimp-painter-http-navigation") == 0) found = GTK_WIDGET (it->data);
    g_list_free (list);
    return found;
  }
  void start (Gimp *gimp, guint port) {
    gimp_set_focused_once (gimp);
    auto *image = gimp_image_new (gimp, 16, 16, GIMP_RGB, GIMP_PRECISION_U8_NON_LINEAR);
    auto *layer = gimp_layer_new (image, 16, 16, babl_format ("R'G'B'A u8"),
                                  "Lifecycle image", 1., GIMP_LAYER_MODE_NORMAL);
    g_assert_true (gimp_image_add_layer (image, layer, nullptr, 0, FALSE));
    auto *display = gimp_create_display (gimp, image, gimp_unit_pixel (), 1., nullptr);
    g_assert_nonnull (display);
    gimp_image_clean_all (image);
    const auto body = "{\"context\":{\"image\":" + std::to_string (gimp_image_get_id (image)) +
      ",\"display\":" + std::to_string (gimp_display_get_id (display)) +
      "},\"arguments\":{\"message\":\"Application-owned shutdown\",\"webhook_uri\":\"" + origin + "/completed\"}}";
    { Pending post (port, "POST", "/api/v1/navigation", body); post.finish (202); }
    g_assert_nonnull (guide ());
    gtk_dialog_response (GTK_DIALOG (guide ()), GTK_RESPONSE_OK);
    const auto until = g_get_monotonic_time () + 5 * G_USEC_PER_SEC;
    while (!deliveries && g_get_monotonic_time () < until) {
      g_main_context_iteration (nullptr, FALSE); g_usleep (100);
    }
    g_assert_cmpuint (deliveries, ==, 1);
    /* Keep a second actual native guide open while the first hook is in flight. */
    { Pending post (port, "POST", "/api/v1/navigation", body); post.finish (202); }
    g_assert_nonnull (guide ());
    retained_guide = GTK_WIDGET (g_object_ref (guide ()));
    g_signal_connect (retained_guide, "destroy", G_CALLBACK (+[](GtkWidget *, gpointer data) {
      static_cast<Navigation *> (data)->destroyed = true;
    }), this);
    g_object_unref (image); /* The display now owns the image. */
  }
  void after_exit () {
    g_assert_true (destroyed);
    gtk_dialog_response (GTK_DIALOG (retained_guide), GTK_RESPONSE_OK);
    /* Release the ordinary server response after client owner shutdown.
     * No private callback injection or forced completion. */
    g_assert_nonnull (paused);
    soup_server_message_set_status (paused, 204, nullptr);
    soup_server_message_unpause (paused);
    const auto until = g_get_monotonic_time () + 5 * G_USEC_PER_SEC;
    while (!response_terminal && g_get_monotonic_time () < until) {
      g_main_context_iteration (nullptr, FALSE); g_usleep (100);
    }
    g_assert_true (response_terminal);
    drain ();
    g_assert_cmpuint (deliveries, ==, 1);
  }
  ~Navigation () {
    if (retained_guide) g_object_unref (retained_guide);
    soup_server_disconnect (server);
    g_clear_object (&paused);
    g_object_unref (server);
    drain ();
  }
};
static void rc_queued_save_dispose (Gimp *gimp) {
  auto *rc = GIMP_RC (gimp_config_duplicate (GIMP_CONFIG (gimp->edit_config)));
  Witness witness (rc);
  gimp_rc_set_autosave (rc, TRUE);
  gint undo_levels;
  g_object_get (rc, "undo-levels", &undo_levels, nullptr);
  g_object_set (rc, "undo-levels", undo_levels + 1, nullptr);
  guint source = rc->save_idle_id;
  g_assert_cmpuint (source, !=, 0);
  gimp_rc_save (rc);
  g_assert_cmpuint (rc->save_idle_id, ==, 0);
  g_assert_null (g_main_context_find_source_by_id (nullptr, source));
  g_object_set (rc, "undo-levels", undo_levels + 2, nullptr);
  source = rc->save_idle_id;
  g_assert_cmpuint (source, !=, 0);
  g_object_run_dispose (G_OBJECT (rc));
  g_assert_false (rc->autosave);
  g_assert_cmpuint (rc->save_idle_id, ==, 0);
  g_assert_null (g_main_context_find_source_by_id (nullptr, source));
  g_object_run_dispose (G_OBJECT (rc));
  g_object_unref (rc);
  g_assert_true (witness.finalized);

  /* An existing directory is a deterministic invalid RC destination, even
   * when the test runner has elevated filesystem permissions. */
  GError *error = nullptr;
  gchar *directory = g_dir_make_tmp ("application-rc-write-failure-XXXXXX", &error);
  g_assert_no_error (error);
  auto *target = g_file_new_for_path (directory);
  rc = GIMP_RC (gimp_config_duplicate (GIMP_CONFIG (gimp->edit_config)));
  Witness failed_witness (rc);
  g_object_set (rc, "user-gimprc", target, nullptr);
  gimp_rc_set_autosave (rc, TRUE);
  g_object_set (rc, "undo-levels", undo_levels + 3, nullptr);
  source = rc->save_idle_id;
  g_assert_cmpuint (source, !=, 0);
  g_test_expect_message ("Gimp-Config", G_LOG_LEVEL_MESSAGE, "*");
  gimp_rc_save (rc);
  g_test_assert_expected_messages ();
  g_assert_cmpuint (rc->save_idle_id, ==, source);
  g_assert_nonnull (g_main_context_find_source_by_id (nullptr, source));
  g_test_expect_message ("Gimp-Config", G_LOG_LEVEL_MESSAGE, "*");
  g_object_run_dispose (G_OBJECT (rc));
  g_test_assert_expected_messages ();
  g_assert_cmpuint (rc->save_idle_id, ==, 0);
  g_assert_null (g_main_context_find_source_by_id (nullptr, source));
  g_object_run_dispose (G_OBJECT (rc));
  g_object_unref (rc);
  g_assert_true (failed_witness.finalized);
  g_assert_true (g_file_query_file_type (target, G_FILE_QUERY_INFO_NONE, nullptr) == G_FILE_TYPE_DIRECTORY);
  g_object_unref (target); g_assert_cmpint (g_rmdir (directory), ==, 0); g_free (directory);
}
static void remove_fixture_tree (GFile *file) {
  GError *error = nullptr;
  if (g_file_query_file_type (file, G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, nullptr) == G_FILE_TYPE_DIRECTORY) {
    auto *children = g_file_enumerate_children (file, G_FILE_ATTRIBUTE_STANDARD_NAME,
      G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, nullptr, &error);
    g_assert_no_error (error);
    while (auto *info = g_file_enumerator_next_file (children, nullptr, &error)) {
      auto *child = g_file_get_child (file, g_file_info_get_name (info));
      remove_fixture_tree (child); g_object_unref (child); g_object_unref (info);
    }
    g_assert_no_error (error);
    g_assert_true (g_file_enumerator_close (children, nullptr, &error));
    g_assert_no_error (error); g_object_unref (children);
  }
  g_assert_true (g_file_delete (file, nullptr, &error)); g_assert_no_error (error);
}
static void lifecycle (bool enabled, bool navigation = false) {
  if (!g_test_subprocess ()) {
    g_test_trap_subprocess (nullptr, 60 * G_USEC_PER_SEC, G_TEST_SUBPROCESS_DEFAULT);
    g_test_trap_assert_passed ();
    return;
  }
  GError *error = nullptr;
  gchar *dir = g_dir_make_tmp ("application-owned-lifecycle-XXXXXX", &error);
  g_assert_no_error (error);
  gchar *path = g_build_filename (dir, "loaded.json", nullptr);
  const char fixture[] = "{\"name\":\"Loaded lifecycle preset\",\"version\":1,\"source-layer\":{\"type\":\"any\"},\"replacement-layer\":[]}";
  g_assert_true (g_file_set_contents (path, fixture, -1, &error));
  g_assert_no_error (error);
  if (enabled) {
    const auto port = std::to_string (available_port ());
    g_setenv ("GIMP_PAINTER_HTTP_ENABLE", "1", TRUE);
    g_setenv ("GIMP_PAINTER_HTTP_PORT", port.c_str (), TRUE);
    g_setenv ("GIMP_PAINTER_HTTP_TOKEN", token, TRUE);
  } else g_unsetenv ("GIMP_PAINTER_HTTP_ENABLE");
  g_unsetenv ("GIMP_PAINTER_HTTP_WEBHOOK_ORIGIN");
  /* A private profile prevents other suites' saved session/tool state from
   * changing what this fresh application loads or writes. */
  g_setenv ("GIMP3_DIRECTORY", dir, TRUE);
  std::shared_ptr<Navigation> gui;
  if (navigation) gui.reset (new Navigation ());
  GApplication *app = nullptr;
  Gimp *gimp = gimp_test_application_owned_start (dir, &app, navigation);
  std::function<void ()> teardown;
  std::function<void ()> active = [&] {
    g_assert_true (gimp_is_restored (gimp));
    GError *error = nullptr;
    auto owner = std::make_shared<Witness> (gimp);
    auto application = std::make_shared<Witness> (app);
    auto context = std::make_shared<Witness> (gimp->user_context);
    rc_queued_save_dispose (gimp);
    auto *factory = GIMP_DATA_FACTORY (g_object_ref (gimp->layer_preset_factory));
    auto *container = GIMP_CONTAINER (g_object_ref (gimp_data_factory_get_container (factory)));
    g_assert_true (gimp_get_data_factory (gimp, GIMP_TYPE_LAYER_PRESET) == factory);
    g_assert_cmpint (gimp_container_get_n_children (container), ==, 1);
    auto *preset = GIMP_DATA (g_object_ref (gimp_container_get_first_child (container)));
    auto factory_witness = std::make_shared<Witness> (factory);
    auto container_witness = std::make_shared<Witness> (container);
    auto resource = std::make_shared<Witness> (preset);
    g_assert_cmpstr (gimp_object_get_name (preset), ==, "Loaded lifecycle preset");
    gimp_object_set_name (GIMP_OBJECT (preset), "Saved through accepted exit");
    gimp_data_dirty (preset);
    GObject *service = gimp->painter_httpd ? G_OBJECT (g_object_ref (gimp->painter_httpd)) : nullptr;
    std::shared_ptr<Witness> service_witness;
    std::shared_ptr<Pending> request;
    auto *images = GIMP_CONTAINER (g_object_ref (gimp->images));
    auto images_witness = std::make_shared<Witness> (images);
    auto late_image_adds = std::make_shared<guint> (0);
    gulong image_add_handler = 0;
    guint service_port = service ? gimp_painter_httpd_port (service) : 0;
    const auto image_count = gimp_container_get_n_children (gimp->images);
    if (enabled) {
      g_assert_nonnull (service);
      service_witness.reset (new Witness (service));
      g_assert_cmpuint (gimp_painter_httpd_port (service), >, 0);
      if (gui) gui->start (gimp, service_port);
      image_add_handler = g_signal_connect (images, "add",
        G_CALLBACK (+[](GimpContainer *, GimpObject *, gpointer data) {
          ++*static_cast<guint *> (data);
        }), late_image_adds.get ());
      request.reset (new Pending (gimp_painter_httpd_port (service)));
      request->queued (service);
    } else g_assert_null (service);
    GFile *rc_file = nullptr;
    gint undo_levels;
    g_object_get (gimp->edit_config, "undo-levels", &undo_levels,
                  "user-gimprc", &rc_file, nullptr);
    g_object_set (gimp->edit_config, "undo-levels", undo_levels + 7, nullptr);
    g_assert_true (GIMP_RC (gimp->edit_config)->autosave);
    g_assert_cmpuint (GIMP_RC (gimp->edit_config)->save_idle_id, !=, 0);
    if (!enabled) {
      auto cancel = g_signal_connect (gimp, "exit",
        G_CALLBACK (+[](Gimp *, gboolean, gpointer) -> gboolean { return TRUE; }), nullptr);
      gimp_exit (gimp, FALSE);
      g_signal_handler_disconnect (gimp, cancel);
      g_assert_nonnull (gimp->user_context);
      g_assert_false (gimp_filter_exit_is_requested (app));
      g_assert_true (GIMP_RC (gimp->edit_config)->autosave);
      g_assert_cmpuint (GIMP_RC (gimp->edit_config)->save_idle_id, !=, 0);
    }
    {
      if (service) g_assert_cmpuint (pending_count (service), ==, 1);
      gimp_exit (gimp, FALSE);
      g_assert_true (gimp_filter_exit_is_requested (app));
      g_assert_null (gimp->painter_httpd);
      g_assert_null (gimp->user_context);
      g_assert_false (GIMP_RC (gimp->edit_config)->autosave);
      g_assert_cmpuint (GIMP_RC (gimp->edit_config)->save_idle_id, ==, 0);
      g_assert_false (GIMP_RC (gimp->config)->autosave);
      g_assert_cmpuint (GIMP_RC (gimp->config)->save_idle_id, ==, 0);
      gchar *rc_text = nullptr;
      g_assert_true (g_file_load_contents (rc_file, nullptr, &rc_text, nullptr, nullptr, &error));
      g_assert_no_error (error);
      const auto saved_undo = "(undo-levels " + std::to_string (undo_levels + 7) + ")";
      g_assert_nonnull (strstr (rc_text, saved_undo.c_str ()));
      g_free (rc_text); g_object_unref (rc_file);
      g_assert_false (gimp_data_is_dirty (preset));
      gchar *saved = nullptr;
      g_assert_true (g_file_get_contents (path, &saved, nullptr, &error));
      g_assert_no_error (error);
      g_assert_nonnull (strstr (saved, "Saved through accepted exit"));
      g_free (saved);
    }
    if (request) request->finish ();
    if (gui) gui->after_exit ();
    drain ();
    g_assert_cmpint (gimp_container_get_n_children (gimp->images), ==, image_count);
    g_assert_cmpuint (*late_image_adds, ==, 0);
    if (service) {
      g_assert_cmpuint (pending_count (service), ==, 0);
      g_assert_cmpuint (gimp_painter_httpd_port (service), ==, 0);
      auto *client = g_socket_client_new ();
      g_socket_client_set_enable_proxy (client, FALSE);
      g_socket_client_set_timeout (client, 2);
      auto *connection = g_socket_client_connect_to_host (client, "127.0.0.1",
                                                         service_port, nullptr, &error);
      g_assert_null (connection);
      g_assert_error (error, G_IO_ERROR, G_IO_ERROR_CONNECTION_REFUSED);
      g_clear_error (&error); g_object_unref (client);
    }
    teardown = [=] () mutable {
      gimp_test_application_owned_release (gimp, app);
      g_assert_true (application->finalized);
      g_object_run_dispose (G_OBJECT (gimp));
      g_assert_cmpint (gimp_container_get_n_children (container), ==, 0);
      g_assert_true (gimp->layer_preset_factory == factory);
      g_object_run_dispose (G_OBJECT (gimp));
      g_assert_false (factory_witness->finalized);
      g_object_unref (gimp);
      g_assert_true (owner->finalized);
      g_assert_true (context->finalized);
      /* Retained resource and container emissions must not reach a dead owner. */
      gimp_data_dirty (preset);
      gimp_container_freeze (container); gimp_container_thaw (container);
      if (service) { g_object_run_dispose (service); g_object_run_dispose (service); }
      drain ();
      g_assert_cmpint (gimp_container_get_n_children (container), ==, 0);
      request.reset ();
      gui.reset ();
      g_assert_cmpuint (*late_image_adds, ==, 0);
      if (image_add_handler) g_signal_handler_disconnect (images, image_add_handler);
      g_object_unref (images); g_assert_true (images_witness->finalized);
      if (service) { g_object_unref (service); g_assert_true (service_witness->finalized); }
      g_object_unref (factory); g_assert_true (factory_witness->finalized);
      g_object_unref (container); g_assert_true (container_witness->finalized);
      g_object_unref (preset); g_assert_true (resource->finalized);
      auto *profile = g_file_new_for_path (dir);
      remove_fixture_tree (profile); g_object_unref (profile);
      g_free (path); g_free (dir);
    };
  };
  g_signal_connect_after (app, "activate",
    G_CALLBACK (+[](GApplication *, gpointer data) {
      (*static_cast<std::function<void ()> *> (data)) ();
    }), &active);
  g_assert_cmpint (g_application_run (app, 0, nullptr), ==, 0);
  g_assert_true (static_cast<bool> (teardown));
  gui.reset ();
  teardown ();
}
static void disabled () { lifecycle (false); }
static void enabled_pending () { lifecycle (true); }
static void enabled_navigation () { lifecycle (true, true); }
int main (int argc, char **argv) {
  g_test_init (&argc, &argv, nullptr);
  g_test_add_func ("/application-owned/disabled-accepted-exit", disabled);
  g_test_add_func ("/application-owned/enabled-pending-accepted-exit", enabled_pending);
  g_test_add_func ("/application-owned/enabled-navigation-accepted-exit", enabled_navigation);
  return g_test_run ();
}
