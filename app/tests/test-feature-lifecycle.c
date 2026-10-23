/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gegl.h>
#include <gtk/gtk.h>
#include "core/core-types.h"
#include "widgets/widgets-types.h"
#include "core/gimp.h"
#include "core/gimp-contexts.h"
#include "core/gimpdatafactory.h"
#include "core/gimp-data-factories.h"
#include "core/gimpasync.h"
#include "core/gimpasyncset.h"
#include "core/gimplayerpreset.h"
#include "presets/preset-factory-gui.h"
#include "gimp-features.h"
#include "httpd/httpd.h"
#include "painter/gimp-painter-error.h"
#include "tests.h"
#include "gimp-app-test-utils.h"
static Gimp *application;
static guint provider_calls, mode, releases;
static gpointer weak_service;
void feature_handle_test (Gimp *application);
GObject *__real_gimp_painter_httpd_from_environment (Gimp *, GError **);
GObject *__wrap_gimp_painter_httpd_from_environment (Gimp *, GError **);
GObject *__wrap_gimp_painter_httpd_from_environment (Gimp *gimp, GError **error)
{
  ++provider_calls;
  if (mode == 1)
    {
      GError *nested = NULL;
      features_entry_point (gimp);
      g_assert_false (gimp_features_restored (gimp, &nested));
      g_assert_error (nested, GIMP_PAINTER_ERROR, GIMP_PAINTER_ERROR_INVALID_STATE);
      g_clear_error (&nested);
    }
  if (mode == 2 || mode == 3)
    {
      GObject *service = g_object_new (G_TYPE_OBJECT, NULL);
      weak_service = service;
      g_object_add_weak_pointer (service, &weak_service);
      if (mode == 2) gimp_features_close (gimp);
      else g_set_error_literal (error, GIMP_PAINTER_ERROR, GIMP_PAINTER_ERROR_EXCEPTION,
                                "Injected partial provider failure");
      return service;
    }
  return __real_gimp_painter_httpd_from_environment (gimp, error);
}
static gboolean begin_case (void)
{
  if (!g_test_subprocess ())
    {
      g_test_trap_subprocess (NULL, 30 * G_TIME_SPAN_SECOND, 0);
      g_test_trap_assert_passed ();
      return FALSE;
    }
  g_setenv ("GIMP_PAINTER_HTTP_ENABLE", "0", TRUE);
  gimp_test_utils_set_gimp3_directory ("GIMP_TESTING_ABS_TOP_SRCDIR", "app/tests/gimpdir");
  application = gimp_init_for_testing ();
  return TRUE;
}
static void finish_case (void)
{
  gimp_test_utils_set_gimp3_directory ("GIMP_TESTING_ABS_TOP_BUILDDIR", "app/tests/gimpdir-output");
  gimp_exit (application, TRUE);
}
static void registration (void)
{
  GimpDataFactory *factory;
  void (*entry) (Gimp *) = features_entry_point;
  if (!begin_case ()) return;
  factory = application->layer_preset_factory;
  g_assert_nonnull (factory);
  g_assert_nonnull (gimp_data_factory_get_container (factory));
  entry (application); entry (application); feature_handle_test (application);
  g_assert_true (factory == application->layer_preset_factory);
  g_test_expect_message ("Gimp", G_LOG_LEVEL_CRITICAL, "*GIMP_IS_GIMP*failed*");
  entry (NULL); g_test_assert_expected_messages ();
  finish_case ();
}
static void restore_disabled (void)
{
  GError *error = NULL;
  if (!begin_case ()) return;
  g_assert_true (gimp_features_restored (application, &error)); g_assert_no_error (error);
  g_assert_null (application->painter_httpd);
  g_assert_true (gimp_features_restored (application, &error));
  g_assert_cmpuint (provider_calls, ==, 1);
  finish_case ();
}
static void restore_opt_in (void)
{
  GError *error = NULL;
  GSocket *probe;
  GInetAddress *loopback;
  GSocketAddress *address, *bound;
  GObject *service;
  guint port;
  gchar *port_text;
  if (!begin_case ()) return;
  probe = g_socket_new (G_SOCKET_FAMILY_IPV4, G_SOCKET_TYPE_STREAM, G_SOCKET_PROTOCOL_TCP, &error);
  g_assert_no_error (error);
  loopback = g_inet_address_new_loopback (G_SOCKET_FAMILY_IPV4);
  address = g_inet_socket_address_new (loopback, 0); g_object_unref (loopback);
  g_assert_true (g_socket_bind (probe, address, FALSE, &error)); g_assert_no_error (error);
  bound = g_socket_get_local_address (probe, &error); g_assert_no_error (error);
  port = g_inet_socket_address_get_port (G_INET_SOCKET_ADDRESS (bound));
  g_object_unref (bound); g_object_unref (address);
  g_assert_true (g_socket_close (probe, &error)); g_assert_no_error (error); g_object_unref (probe);
  port_text = g_strdup_printf ("%u", port);
  g_setenv ("GIMP_PAINTER_HTTP_ENABLE", "1", TRUE);
  g_setenv ("GIMP_PAINTER_HTTP_PORT", port_text, TRUE); g_free (port_text);
  /* Public, disposable test fixture value; never a user/application credential. */
  g_setenv ("GIMP_PAINTER_HTTP_TOKEN", "feature-lifecycle-test-token-000000", TRUE);
  g_assert_true (gimp_features_restored (application, &error)); g_assert_no_error (error);
  g_assert_nonnull (application->painter_httpd);
  service = g_object_ref (application->painter_httpd);
  g_assert_cmpuint (gimp_painter_httpd_port (service), ==, port);
  g_assert_true (gimp_features_restored (application, &error)); g_assert_no_error (error);
  g_assert_true (application->painter_httpd == service);
  g_assert_cmpuint (provider_calls, ==, 1);
  gimp_features_close (application);
  g_assert_null (application->painter_httpd);
  g_assert_cmpuint (gimp_painter_httpd_port (service), ==, 0);
  g_object_unref (service);
  finish_case ();
}
static void restore_failure (void)
{
  GError *error = NULL;
  if (!begin_case ()) return;
  g_setenv ("GIMP_PAINTER_HTTP_ENABLE", "1", TRUE);
  g_setenv ("GIMP_PAINTER_HTTP_PORT", "0", TRUE);
  g_assert_false (gimp_features_restored (application, &error));
  g_assert_error (error, GIMP_PAINTER_ERROR, GIMP_PAINTER_ERROR_INVALID_STATE); g_clear_error (&error);
  g_assert_null (application->painter_httpd);
  g_assert_false (gimp_features_restored (application, &error)); g_clear_error (&error);
  g_assert_cmpuint (provider_calls, ==, 1);
  gimp_features_close (application); features_entry_point (application);
  g_assert_false (gimp_features_restored (application, &error));
  g_assert_error (error, GIMP_PAINTER_ERROR, GIMP_PAINTER_ERROR_CLOSED); g_clear_error (&error);
  finish_case ();
}
static void restore_reentry (void)
{
  GError *error = NULL;
  if (!begin_case ()) return;
  mode = 1;
  g_assert_true (gimp_features_restored (application, &error)); g_assert_no_error (error);
  g_assert_cmpuint (provider_calls, ==, 1);
  finish_case ();
}
static void restore_close (void)
{
  GError *error = NULL;
  if (!begin_case ()) return;
  mode = 2;
  g_assert_false (gimp_features_restored (application, &error));
  g_assert_error (error, GIMP_PAINTER_ERROR, GIMP_PAINTER_ERROR_CLOSED); g_clear_error (&error);
  g_assert_null (weak_service); g_assert_null (application->painter_httpd);
  features_entry_point (application); gimp_features_initialized (application);
  g_assert_false (gimp_features_restored (application, &error)); g_clear_error (&error);
  g_assert_cmpuint (provider_calls, ==, 1);
  finish_case ();
}
static void partial_provider_failure (void)
{
  GError *error = NULL;
  if (!begin_case ()) return;
  mode = 3;
  g_assert_false (gimp_features_restored (application, &error));
  g_assert_error (error, GIMP_PAINTER_ERROR, GIMP_PAINTER_ERROR_EXCEPTION); g_clear_error (&error);
  g_assert_null (weak_service); g_assert_null (application->painter_httpd);
  g_assert_false (gimp_features_restored (application, &error)); g_clear_error (&error);
  g_assert_cmpuint (provider_calls, ==, 1);
  finish_case ();
}
static void restore_order (void)
{
  GError *error = NULL;
  if (!begin_case ()) return;
  application->restored = FALSE;
  g_assert_false (gimp_features_restored (application, &error));
  g_assert_error (error, GIMP_PAINTER_ERROR, GIMP_PAINTER_ERROR_INVALID_STATE); g_clear_error (&error);
  g_assert_cmpuint (provider_calls, ==, 0);
  application->restored = TRUE;
  g_assert_true (gimp_features_restored (application, &error)); g_assert_no_error (error);
  g_assert_cmpuint (provider_calls, ==, 1);
  finish_case ();
}
static void close_observer (gpointer data, GObject *dead)
{
  Gimp *gimp = data;
  GError *error = NULL;
  (void) dead;
  ++releases;
  g_assert_null (gimp->painter_httpd);
  features_entry_point (gimp); gimp_features_close (gimp);
  g_assert_false (gimp_features_restored (gimp, &error));
  g_assert_error (error, GIMP_PAINTER_ERROR, GIMP_PAINTER_ERROR_CLOSED); g_clear_error (&error);
  g_object_unref (gimp); /* callback consumes its deliberately transferred lease */
}
static void close_reentry (void)
{
  if (!begin_case ()) return;
  application->painter_httpd = g_object_new (G_TYPE_OBJECT, NULL);
  g_object_weak_ref (application->painter_httpd, close_observer, g_object_ref (application));
  gimp_features_close (application); gimp_features_close (application);
  g_assert_cmpuint (releases, ==, 1); g_assert_null (application->painter_httpd);
  finish_case ();
}
static void close_last_owner (void)
{
  gpointer weak_application;
  if (!begin_case ()) return;
  weak_application = application;
  g_object_add_weak_pointer (G_OBJECT (application), &weak_application);
  application->painter_httpd = g_object_new (G_TYPE_OBJECT, NULL);
  /* Native finalization requires context shutdown, normally done by gimp_exit. */
  gimp_contexts_exit (application);
  /* Transfer the caller's only application reference to the callback. */
  g_object_weak_ref (application->painter_httpd, close_observer, application);
  gimp_features_close (application);
  g_assert_cmpuint (releases, ==, 1); g_assert_null (weak_application);
  application = NULL;
}
static void dispose_fallback (void)
{
  if (!begin_case ()) return;
  application->painter_httpd = g_object_new (G_TYPE_OBJECT, NULL);
  g_object_weak_ref (application->painter_httpd, close_observer, g_object_ref (application));
  /* Isolate dispose fallback, with ordinary native context teardown prepared. */
  gimp_contexts_exit (application);
  g_object_run_dispose (G_OBJECT (application));
  g_assert_cmpuint (releases, ==, 1); g_assert_null (application->painter_httpd);
  features_entry_point (application); gimp_features_initialized (application);
  g_object_run_dispose (G_OBJECT (application));
  g_object_unref (application);
}
typedef struct { guint count; GThread *worker; } PresetWait;
static gpointer complete_preset_load (gpointer data)
{ gimp_async_finish (GIMP_ASYNC (data), NULL); return NULL; }
static void finish_preset_load (GimpAsync *async, gpointer data)
{
  PresetWait *waited = data;
  ++waited->count;
  /* waiting is emitted while the async mutex is held; completion runs on a
   * worker only AFTER actual wait admission, without a timer race. */
  waited->worker = g_thread_new ("preset-load-test", complete_preset_load, async);
}
static void resource_factory_wait (void)
{
  GimpAsync *async;
  PresetWait waited = { 0, NULL };
  GimpAsyncSet *set;
  if (!begin_case ()) return;
  /* Actual app activation sets this flag; the small helper deliberately does
   * not. Exercise the public readiness query rather than reading the field. */
  application->initialized = TRUE;
  g_assert_true (gimp_is_restored (application));
  set = gimp_data_factory_get_async_set (application->layer_preset_factory);
  async = gimp_async_new ();
  gimp_async_set_add (set, async);
  g_assert_false (gimp_async_set_is_empty (set));
  g_signal_connect (async, "waiting", G_CALLBACK (finish_preset_load), &waited);
  g_assert_true (gimp_data_factories_wait (application));
  /* Excluding the whole JSON subtree would return before this real work. */
  g_assert_true (gimp_async_is_finished (async));
  g_assert_cmpuint (waited.count, ==, 1);
  g_thread_join (waited.worker);
  g_object_unref (async);
  finish_case ();
}
/* Explicit native replacement contract, not old singleton ABI parity. */
static void preentry_preferences (void)
{
  GArray *first, *second;
  if (!g_test_subprocess ())
    { g_test_trap_subprocess (NULL, 30 * G_TIME_SPAN_SECOND, 0);
      g_test_trap_assert_passed (); return; }
  /* No Gimp construction or features_entry_point has occurred in this process. */
  first = preset_factory_gui_prefs_entry_point ();
  second = preset_factory_gui_prefs_entry_point ();
  g_assert_cmpuint (first->len, ==, 1);
  g_assert_true (first != second);
  g_assert_cmpstr (g_array_index (first, PrefsDialogEntry, 0).path_property_name,
                   ==, "layer-presets-path");
  g_array_unref (first); g_array_unref (second);
}
static guint observed_phase;
static GimpDataFactory *observed_factory;
static gboolean startup_hook (GSignalInvocationHint *hint, guint n_values,
                              const GValue *values, gpointer data)
{
  Gimp *gimp = g_value_get_object (&values[0]);
  guint phase = GPOINTER_TO_UINT (data);
  GError *error = NULL;
  (void) hint; (void) n_values;
  g_assert_cmpuint (phase, ==, ++observed_phase);
  g_assert_nonnull (gimp->layer_preset_factory);
  if (phase == 1) observed_factory = gimp->layer_preset_factory;
  g_assert_true (observed_factory == gimp->layer_preset_factory);
  g_assert_true (gimp_get_data_factory (gimp, GIMP_TYPE_LAYER_PRESET) == observed_factory);
  g_assert_cmpuint (provider_calls, ==, phase == 3 ? 1 : 0);
  if (phase < 3)
    {
      g_assert_false (gimp_features_restored (gimp, &error));
      g_assert_error (error, GIMP_PAINTER_ERROR, GIMP_PAINTER_ERROR_INVALID_STATE);
      g_clear_error (&error);
    }
  return TRUE;
}
static void supported_startup_order (void)
{
  const gchar *names[] = { "initialize", "restore", "exit" };
  guint signals[3], i; gulong hooks[3]; gpointer klass;
  GError *error = NULL;
  if (!g_test_subprocess ())
    { g_test_trap_subprocess (NULL, 30 * G_TIME_SPAN_SECOND, 0);
      g_test_trap_assert_passed (); return; }
  g_setenv ("GIMP_PAINTER_HTTP_ENABLE", "0", TRUE);
  gimp_test_utils_set_gimp3_directory ("GIMP_TESTING_ABS_TOP_SRCDIR", "app/tests/gimpdir");
  klass = g_type_class_ref (GIMP_TYPE_GIMP);
  for (i = 0; i < 3; i++)
    { signals[i] = g_signal_lookup (names[i], GIMP_TYPE_GIMP);
      hooks[i] = g_signal_add_emission_hook (signals[i], 0, startup_hook,
                                            GUINT_TO_POINTER (i + 1), NULL); }
  application = gimp_init_for_testing ();
  g_assert_cmpuint (observed_phase, ==, 2);
  g_assert_true (gimp_features_restored (application, &error));
  g_assert_no_error (error); g_assert_cmpuint (provider_calls, ==, 1);
  /* Close is explicit at accepted application exit, not an extra signal handler. */
  gimp_features_close (application);
  finish_case ();
  g_assert_cmpuint (observed_phase, ==, 3);
  for (i = 0; i < 3; i++) g_signal_remove_emission_hook (signals[i], hooks[i]);
  g_type_class_unref (klass);
}
int main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/feature-lifecycle/resource-factory-wait", resource_factory_wait);
  g_test_add_func ("/feature-lifecycle/preentry-preferences", preentry_preferences);
  g_test_add_func ("/feature-lifecycle/supported-startup-order", supported_startup_order);
  g_test_add_func ("/feature-lifecycle/registration", registration);
  g_test_add_func ("/feature-lifecycle/restore-order", restore_order);
  g_test_add_func ("/feature-lifecycle/partial-provider-failure", partial_provider_failure);
  g_test_add_func ("/feature-lifecycle/restore-disabled", restore_disabled);
  g_test_add_func ("/feature-lifecycle/restore-opt-in", restore_opt_in);
  g_test_add_func ("/feature-lifecycle/restore-failure", restore_failure);
  g_test_add_func ("/feature-lifecycle/restore-reentry", restore_reentry);
  g_test_add_func ("/feature-lifecycle/restore-close", restore_close);
  g_test_add_func ("/feature-lifecycle/close-reentry", close_reentry);
  g_test_add_func ("/feature-lifecycle/close-last-owner", close_last_owner);
  g_test_add_func ("/feature-lifecycle/dispose-fallback", dispose_fallback);
  return g_test_run ();
}
