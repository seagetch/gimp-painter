/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gegl.h>
#include <gtk/gtk.h>
#include "core/core-types.h"
#include "widgets/widgets-types.h"
#include "paint/paint-types.h"
#include "core/gimp.h"
#include "core/gimp-contexts.h"
#include "core/gimpcontainer.h"
#include "core/gimppaintinfo.h"
#include "paint/gimp-paint.h"
#include "paint/gimppaintoptions.h"
#include "paint/gimppaintbrush.h"
#include "tests.h"
#include "gimp-app-test-utils.h"
/* Exact legacy signature also permits a real missing-symbol baseline link test. */
void gimp_paint_register (Gimp *gimp, GimpPaintInfo *paint_info);
void paint_registration_cpp (Gimp *gimp, GimpPaintInfo *paint_info);
static Gimp *application;
static guint callbacks;
static gboolean begin_case (void)
{
  if (!g_test_subprocess ())
    {
      g_test_trap_subprocess (NULL, 30 * G_TIME_SPAN_SECOND, 0);
      g_test_trap_assert_passed ();
      return FALSE;
    }
  gimp_test_utils_set_gimp3_directory ("GIMP_TESTING_ABS_TOP_SRCDIR", "app/tests/gimpdir");
  application = gimp_init_for_testing ();
  return TRUE;
}
static void finish_case (void)
{
  gimp_test_utils_set_gimp3_directory ("GIMP_TESTING_ABS_TOP_BUILDDIR", "app/tests/gimpdir-output");
  gimp_exit (application, TRUE);
}
static GimpPaintInfo *new_info (const gchar *name)
{
  return gimp_paint_info_new (application, GIMP_TYPE_PAINTBRUSH,
                              GIMP_TYPE_PAINT_OPTIONS, name, "Registration test", "gimp-tool-paintbrush");
}
/* Native paint-info/options are a deliberate mutual-reference pair. As native
 * gimp_paint_exit does, dispose the info after registry removal. The single
 * remaining reference must be the options back-reference, not the consumed one. */
static void dispose_removed (GimpPaintInfo *info)
{
  g_assert_cmpuint (G_OBJECT (info)->ref_count, ==, 1);
  g_object_run_dispose (G_OBJECT (info));
}
static void transfer (void)
{
  GimpPaintInfo *info;
  gpointer weak;
  gint count;
  void (*entry) (Gimp *, GimpPaintInfo *) = gimp_paint_register;
  if (!begin_case ()) return;
  count = gimp_container_get_n_children (application->paint_info_list);
  info = new_info ("original-public-registration"); weak = info;
  g_object_add_weak_pointer (G_OBJECT (info), &weak);
  entry (application, info);
  g_assert_nonnull (weak);
  g_assert_true (gimp_container_have (application->paint_info_list, GIMP_OBJECT (info)));
  g_assert_cmpint (gimp_container_get_n_children (application->paint_info_list), ==, count + 1);
  gimp_container_remove (application->paint_info_list, GIMP_OBJECT (info));
  dispose_removed (info);
  g_assert_null (weak);
  finish_case ();
}
static void cpp_transfer (void)
{
  GimpPaintInfo *info;
  gpointer weak;
  if (!begin_case ()) return;
  info = new_info ("cpp-transfer-registration"); weak = info;
  g_object_add_weak_pointer (G_OBJECT (info), &weak);
  paint_registration_cpp (application, info);
  gimp_container_remove (application->paint_info_list, GIMP_OBJECT (info));
  dispose_removed (info);
  g_assert_null (weak); finish_case ();
}
static void invalid (void)
{
  GimpPaintInfo *info;
  GimpContainer *saved;
  if (!begin_case ()) return;
  info = new_info ("invalid-registration");
  g_test_expect_message ("Gimp-Paint", G_LOG_LEVEL_CRITICAL, "*GIMP_IS_GIMP*failed*");
  gimp_paint_register (NULL, info); g_test_assert_expected_messages ();
  g_test_expect_message ("Gimp-Paint", G_LOG_LEVEL_CRITICAL, "*GIMP_IS_PAINT_INFO*failed*");
  gimp_paint_register (application, NULL); g_test_assert_expected_messages ();
  saved = application->paint_info_list; application->paint_info_list = NULL;
  g_test_expect_message ("Gimp-Paint", G_LOG_LEVEL_CRITICAL, "*GIMP_IS_CONTAINER*failed*");
  gimp_paint_register (application, info); g_test_assert_expected_messages ();
  application->paint_info_list = saved;
  g_assert_cmpuint (G_OBJECT (info)->ref_count, ==, 2);
  g_object_run_dispose (G_OBJECT (info));
  g_object_unref (info); finish_case ();
}
static void duplicate (void)
{
  GimpPaintInfo *info;
  gpointer weak;
  guint refs;
  if (!begin_case ()) return;
  info = new_info ("duplicate-registration"); weak = info;
  g_object_add_weak_pointer (G_OBJECT (info), &weak);
  gimp_paint_register (application, info); refs = G_OBJECT (info)->ref_count;
  g_test_expect_message ("Gimp-Core", G_LOG_LEVEL_WARNING, "*already contains object*");
  gimp_paint_register (application, g_object_ref (info)); g_test_assert_expected_messages ();
  g_assert_cmpuint (G_OBJECT (info)->ref_count, ==, refs);
  gimp_container_remove (application->paint_info_list, GIMP_OBJECT (info));
  dispose_removed (info);
  g_assert_null (weak); finish_case ();
}
static void remove_on_items (GListModel *model, guint position, guint removed,
                             guint added, gpointer data)
{
  GimpPaintInfo *info = data;
  (void) position; (void) removed;
  if (added)
    {
      ++callbacks;
      gimp_container_remove (GIMP_CONTAINER (model), GIMP_OBJECT (info));
    }
}
static void observer_removes (void)
{
  GimpPaintInfo *info;
  gpointer weak;
  gulong id;
  gint count;
  if (!begin_case ()) return;
  count = gimp_container_get_n_children (application->paint_info_list);
  info = new_info ("observer-removes-registration"); weak = info;
  g_object_add_weak_pointer (G_OBJECT (info), &weak);
  id = g_signal_connect (application->paint_info_list, "items-changed", G_CALLBACK (remove_on_items), info);
  gimp_paint_register (application, info);
  g_signal_handler_disconnect (application->paint_info_list, id);
  g_assert_cmpuint (callbacks, ==, 1); dispose_removed (info); g_assert_null (weak);
  g_assert_cmpint (gimp_container_get_n_children (application->paint_info_list), ==, count);
  finish_case ();
}
static void nested_add (GimpContainer *registry, GimpObject *object, gpointer data)
{
  (void) registry; (void) object; (void) data;
  if (callbacks++ == 0) gimp_paint_register (application, new_info ("nested-registration"));
}
static void observer_registers (void)
{
  gulong id;
  gint count;
  if (!begin_case ()) return;
  count = gimp_container_get_n_children (application->paint_info_list);
  id = g_signal_connect (application->paint_info_list, "add", G_CALLBACK (nested_add), NULL);
  gimp_paint_register (application, new_info ("outer-registration"));
  g_signal_handler_disconnect (application->paint_info_list, id);
  g_assert_cmpuint (callbacks, ==, 2);
  g_assert_cmpint (gimp_container_get_n_children (application->paint_info_list), ==, count + 2);
  finish_case ();
}
static void detach_registry (GimpContainer *registry, GimpObject *object, gpointer data)
{
  (void) registry; (void) object; (void) data;
  ++callbacks; g_clear_object (&application->paint_info_list);
}
static void observer_detaches (void)
{
  GimpContainer *original;
  GimpPaintInfo *info;
  gpointer weak;
  gulong id;
  if (!begin_case ()) return;
  original = g_object_ref (application->paint_info_list);
  info = new_info ("detached-registry-registration"); weak = info;
  g_object_add_weak_pointer (G_OBJECT (info), &weak);
  id = g_signal_connect (original, "add", G_CALLBACK (detach_registry), NULL);
  gimp_paint_register (application, info);
  g_assert_null (application->paint_info_list); g_assert_nonnull (weak);
  g_signal_handler_disconnect (original, id);
  application->paint_info_list = original;
  gimp_container_remove (original, GIMP_OBJECT (info)); dispose_removed (info); g_assert_null (weak);
  g_assert_cmpuint (callbacks, ==, 1); finish_case ();
}
static void release_application (GListModel *model, guint position, guint removed,
                                 guint added, gpointer data)
{
  (void) model; (void) position; (void) removed; (void) data;
  if (added)
    {
      ++callbacks;
      g_object_unref (application);
      g_assert_true (GIMP_IS_GIMP (application)); /* entry's lease */
    }
}
static void observer_last_owner (void)
{
  GimpPaintInfo *info;
  gpointer weak_application, weak_info;
  if (!begin_case ()) return;
  info = new_info ("last-owner-registration");
  weak_application = application; weak_info = info;
  g_object_add_weak_pointer (G_OBJECT (application), &weak_application);
  g_object_add_weak_pointer (G_OBJECT (info), &weak_info);
  gimp_contexts_exit (application); /* normal native teardown prerequisite */
  g_signal_connect (application->paint_info_list, "items-changed",
                    G_CALLBACK (release_application), NULL);
  gimp_paint_register (application, info);
  g_assert_cmpuint (callbacks, ==, 1);
  g_assert_null (weak_application); g_assert_null (weak_info);
  application = NULL;
}
static void native_catalog (void)
{
  GimpPaintInfo *standard;
  if (!begin_case ()) return;
  g_assert_cmpint (gimp_container_get_n_children (application->paint_info_list), ==, 15);
  standard = gimp_paint_info_get_standard (application);
  g_assert_nonnull (standard);
  g_assert_cmpuint (standard->paint_type, ==, GIMP_TYPE_PAINTBRUSH);
  g_assert_true (gimp_container_have (application->paint_info_list, GIMP_OBJECT (standard)));
  finish_case ();
}
int main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/paint-registration/transfer", transfer);
  g_test_add_func ("/paint-registration/cpp-transfer", cpp_transfer);
  g_test_add_func ("/paint-registration/invalid", invalid);
  g_test_add_func ("/paint-registration/duplicate", duplicate);
  g_test_add_func ("/paint-registration/observer-removes", observer_removes);
  g_test_add_func ("/paint-registration/observer-registers", observer_registers);
  g_test_add_func ("/paint-registration/observer-detaches", observer_detaches);
  g_test_add_func ("/paint-registration/observer-last-owner", observer_last_owner);
  g_test_add_func ("/paint-registration/native-catalog", native_catalog);
  return g_test_run ();
}
