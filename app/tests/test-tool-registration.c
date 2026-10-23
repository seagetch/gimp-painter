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
#include "core/gimptoolinfo.h"
#include "tools/gimp-tools.h"
#include "paint/gimppaintoptions.h"
#include "tools/tools-types.h"
#include "tools/gimptool.h"
#include "tools/gimppaintbrushtool.h"
#include "tools/gimpfillbrushtool.h"
#include "core/gimptooloptions.h"
#include "core/gimpcontext.h"
#include "tests.h"
#include "gimp-app-test-utils.h"
/* Exact legacy signature also permits a real missing-symbol baseline link test. */
void gimp_tools_register (Gimp *gimp, GimpToolInfo *tool_info);
void tool_registration_cpp (Gimp *gimp, GimpToolInfo *tool_info);
static Gimp *application;
static guint callbacks;
static gboolean tools_initialized;
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
  g_object_set (application->config, "painter-canvas-ui", TRUE, NULL);
  { gboolean enabled = FALSE; g_object_get (application->config, "painter-canvas-ui", &enabled, NULL); g_assert_true (enabled); }
  return TRUE;
}
static void finish_case (void)
{
  gimp_test_utils_set_gimp3_directory ("GIMP_TESTING_ABS_TOP_BUILDDIR", "app/tests/gimpdir-output");
  if (tools_initialized) gimp_tools_exit (application);
  gimp_exit (application, TRUE);
}
static GimpToolInfo *new_info (const gchar *name)
{
  return gimp_tool_info_new (application, GIMP_TYPE_TOOL,
                              GIMP_TYPE_TOOL_OPTIONS, 0, name, "Registration test",
                              "Tool registration ownership", "_Registration", NULL,
                              NULL, "gimp-tool-paintbrush", "gimp-paintbrush", "gimp-tool-paintbrush");
}
/* Before publication, tool options have GimpContext::tool and
 * GimpToolOptions::tool_info back-references. Native container removal clears
 * the context reference (gimp_context_tool_removed); only the explicit options
 * tool_info cycle remains. A disposal lease breaks that final cycle safely. */
static void dispose_removed (GimpToolInfo *info)
{
  g_assert_true (gimp_context_get_tool (GIMP_CONTEXT (info->tool_options)) != info);
  g_assert_true (info->tool_options->tool_info == info);
  g_assert_cmpuint (G_OBJECT (info)->ref_count, ==, 1);
  g_object_ref (info);
  g_object_run_dispose (G_OBJECT (info));
  g_assert_cmpuint (G_OBJECT (info)->ref_count, ==, 1);
  g_object_unref (info);
}
static void transfer (void)
{
  GimpToolInfo *info;
  gpointer weak;
  gint count;
  void (*entry) (Gimp *, GimpToolInfo *) = gimp_tools_register;
  if (!begin_case ()) return;
  count = gimp_container_get_n_children (application->tool_info_list);
  info = new_info ("original-public-registration"); weak = info;
  g_object_add_weak_pointer (G_OBJECT (info), &weak);
  entry (application, info);
  g_assert_nonnull (weak);
  g_assert_true (gimp_container_have (application->tool_info_list, GIMP_OBJECT (info)));
  g_assert_cmpint (gimp_container_get_n_children (application->tool_info_list), ==, count + 1);
  gimp_container_remove (application->tool_info_list, GIMP_OBJECT (info));
  dispose_removed (info);
  g_assert_null (weak);
  finish_case ();
}
static void cpp_transfer (void)
{
  GimpToolInfo *info;
  gpointer weak;
  if (!begin_case ()) return;
  info = new_info ("cpp-transfer-registration"); weak = info;
  g_object_add_weak_pointer (G_OBJECT (info), &weak);
  tool_registration_cpp (application, info);
  gimp_container_remove (application->tool_info_list, GIMP_OBJECT (info));
  dispose_removed (info);
  g_assert_null (weak); finish_case ();
}
static void invalid (void)
{
  GimpToolInfo *info;
  GimpContainer *saved;
  if (!begin_case ()) return;
  info = new_info ("invalid-registration");
  g_test_expect_message ("Gimp-Tools", G_LOG_LEVEL_CRITICAL, "*GIMP_IS_GIMP*failed*");
  gimp_tools_register (NULL, info); g_test_assert_expected_messages ();
  g_test_expect_message ("Gimp-Tools", G_LOG_LEVEL_CRITICAL, "*GIMP_IS_TOOL_INFO*failed*");
  gimp_tools_register (application, NULL); g_test_assert_expected_messages ();
  saved = application->tool_info_list; application->tool_info_list = NULL;
  g_test_expect_message ("Gimp-Tools", G_LOG_LEVEL_CRITICAL, "*GIMP_IS_CONTAINER*failed*");
  gimp_tools_register (application, info); g_test_assert_expected_messages ();
  application->tool_info_list = saved;
  g_assert_cmpuint (G_OBJECT (info)->ref_count, ==, 3);
  g_object_run_dispose (G_OBJECT (info));
  g_object_unref (info); finish_case ();
}
static void duplicate (void)
{
  GimpToolInfo *info;
  gpointer weak;
  guint refs;
  if (!begin_case ()) return;
  info = new_info ("duplicate-registration"); weak = info;
  g_object_add_weak_pointer (G_OBJECT (info), &weak);
  gimp_tools_register (application, info); refs = G_OBJECT (info)->ref_count;
  g_test_expect_message ("Gimp-Core", G_LOG_LEVEL_WARNING, "*already contains object*");
  gimp_tools_register (application, g_object_ref (info)); g_test_assert_expected_messages ();
  g_assert_cmpuint (G_OBJECT (info)->ref_count, ==, refs);
  gimp_container_remove (application->tool_info_list, GIMP_OBJECT (info));
  dispose_removed (info);
  g_assert_null (weak); finish_case ();
}
static void remove_on_items (GListModel *model, guint position, guint removed,
                             guint added, gpointer data)
{
  GimpToolInfo *info = data;
  (void) position; (void) removed;
  if (added)
    {
      ++callbacks;
      gimp_container_remove (GIMP_CONTAINER (model), GIMP_OBJECT (info));
    }
}
static void observer_removes (void)
{
  GimpToolInfo *info;
  gpointer weak;
  gulong id;
  gint count;
  if (!begin_case ()) return;
  count = gimp_container_get_n_children (application->tool_info_list);
  info = new_info ("observer-removes-registration"); weak = info;
  g_object_add_weak_pointer (G_OBJECT (info), &weak);
  id = g_signal_connect (application->tool_info_list, "items-changed", G_CALLBACK (remove_on_items), info);
  gimp_tools_register (application, info);
  g_signal_handler_disconnect (application->tool_info_list, id);
  g_assert_cmpuint (callbacks, ==, 1); dispose_removed (info); g_assert_null (weak);
  g_assert_cmpint (gimp_container_get_n_children (application->tool_info_list), ==, count);
  finish_case ();
}
static void nested_add (GimpContainer *registry, GimpObject *object, gpointer data)
{
  (void) registry; (void) object; (void) data;
  if (callbacks++ == 0) gimp_tools_register (application, new_info ("nested-registration"));
}
static void observer_registers (void)
{
  gulong id;
  gint count;
  if (!begin_case ()) return;
  count = gimp_container_get_n_children (application->tool_info_list);
  id = g_signal_connect (application->tool_info_list, "add", G_CALLBACK (nested_add), NULL);
  gimp_tools_register (application, new_info ("outer-registration"));
  g_signal_handler_disconnect (application->tool_info_list, id);
  g_assert_cmpuint (callbacks, ==, 2);
  g_assert_cmpint (gimp_container_get_n_children (application->tool_info_list), ==, count + 2);
  finish_case ();
}
static void detach_registry (GimpContainer *registry, GimpObject *object, gpointer data)
{
  (void) registry; (void) object; (void) data;
  ++callbacks; g_clear_object (&application->tool_info_list);
}
static void observer_detaches (void)
{
  GimpContainer *original;
  GimpToolInfo *info;
  gpointer weak;
  gulong id;
  if (!begin_case ()) return;
  original = g_object_ref (application->tool_info_list);
  info = new_info ("detached-registry-registration"); weak = info;
  g_object_add_weak_pointer (G_OBJECT (info), &weak);
  id = g_signal_connect (original, "add", G_CALLBACK (detach_registry), NULL);
  gimp_tools_register (application, info);
  g_assert_null (application->tool_info_list); g_assert_nonnull (weak);
  g_signal_handler_disconnect (original, id);
  application->tool_info_list = original;
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
  GimpToolInfo *info;
  gpointer weak_application, weak_info;
  if (!begin_case ()) return;
  info = new_info ("last-owner-registration");
  weak_application = application; weak_info = info;
  g_object_add_weak_pointer (G_OBJECT (application), &weak_application);
  g_object_add_weak_pointer (G_OBJECT (info), &weak_info);
  gimp_contexts_exit (application); /* normal native teardown prerequisite */
  g_signal_connect (application->tool_info_list, "items-changed",
                    G_CALLBACK (release_application), NULL);
  gimp_tools_register (application, info);
  g_assert_cmpuint (callbacks, ==, 1);
  g_assert_null (weak_application); g_assert_null (weak_info);
  application = NULL;
}
static void native_catalog (void)
{
  GimpToolInfo *standard;
  GHashTable *names;
  GList *iter;
  gint count;
  GString *catalog;
  const gchar *path = g_getenv ("GIMP_TEST_TOOL_REGISTRATION_CATALOG");
  if (!begin_case ()) return;
  catalog = g_string_new (NULL);
  g_assert_cmpint (gimp_container_get_n_children (application->tool_info_list), ==, 0);
  gimp_tools_init (application); tools_initialized = TRUE;
  count = gimp_container_get_n_children (application->tool_info_list);
  g_assert_cmpint (count, ==, 50);
  names = g_hash_table_new (g_str_hash, g_str_equal);
  for (iter = gimp_get_tool_info_iter (application); iter; iter = iter->next) {
    GimpToolInfo *info = iter->data;
    const gchar *name = gimp_object_get_name (GIMP_OBJECT (info));
    g_assert_false (g_hash_table_contains (names, name)); g_hash_table_add (names, (gpointer) name);
    g_assert_true (g_type_is_a (info->tool_type, GIMP_TYPE_TOOL));
    g_assert_nonnull (info->tool_options);
    /* These three native registrations deliberately supply no options GUI. */
    if (g_str_equal (name, "gimp-offset-tool") || g_str_equal (name, "gimp-threshold-tool") ||
        g_str_equal (name, "gimp-brightness-contrast-tool"))
      g_assert_null (g_object_get_data (G_OBJECT (info), "gimp-tool-options-gui-func"));
    else
      g_assert_nonnull (g_object_get_data (G_OBJECT (info), "gimp-tool-options-gui-func"));
    g_string_append_printf (catalog, "%s\t%s\t%d\n", name, g_type_name (info->tool_type),
                            g_object_get_data (G_OBJECT (info), "gimp-tool-options-gui-func") != NULL);
  }
  g_assert_cmpuint (g_hash_table_size (names), ==, count); g_hash_table_unref (names);
  standard = gimp_tool_info_get_standard (application); g_assert_nonnull (standard);
  g_assert_cmpuint (standard->tool_type, ==, GIMP_TYPE_PAINTBRUSH_TOOL);
  g_assert_true (gimp_container_have (application->tool_info_list, GIMP_OBJECT (standard)));
  g_assert_nonnull (gimp_container_get_child_by_name (application->tool_info_list, "gimp-bucket-fill-brush-tool"));
  g_assert_nonnull (gimp_container_get_child_by_name (application->tool_info_list, "gimp-perspective-guide-tool"));
  g_test_message ("Native registered tool count: %d", count);
  if (path) { GError *error = NULL; g_assert_true (g_file_set_contents (path, catalog->str, catalog->len, &error)); g_assert_no_error (error); }
  g_string_free (catalog, TRUE);
  finish_case ();
}
static GimpToolInfo *removed_standard;
static void remove_standard_on_add (GimpContainer *registry, GimpObject *object, gpointer data)
{
  (void) data;
  if (g_str_equal (gimp_object_get_name (object), "gimp-paintbrush-tool")) {
    ++callbacks;
    removed_standard = g_object_ref (GIMP_TOOL_INFO (object));
    gimp_container_remove (registry, object);
  }
}
static void native_standard_removed (void)
{
  gulong handler;
  gpointer weak;
  if (!begin_case ()) return;
  handler = g_signal_connect (application->tool_info_list, "add", G_CALLBACK (remove_standard_on_add), NULL);
  gimp_tools_init (application); tools_initialized = TRUE;
  g_signal_handler_disconnect (application->tool_info_list, handler);
  g_assert_cmpuint (callbacks, ==, 1); g_assert_nonnull (removed_standard);
  g_assert_null (gimp_tool_info_get_standard (application));
  g_assert_false (gimp_container_have (application->tool_info_list, GIMP_OBJECT (removed_standard)));
  g_assert_cmpint (gimp_container_get_n_children (application->tool_info_list), ==, 49);
  weak = removed_standard; g_object_add_weak_pointer (G_OBJECT (removed_standard), &weak);
  /* Observer's explicit reference owns this removed resource; retire its
   * native options cycle before releasing that reference. */
  g_object_run_dispose (G_OBJECT (removed_standard)); g_clear_object (&removed_standard);
  g_assert_null (weak); finish_case ();
}
int main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);
  if (!gtk_init_check (&argc, &argv)) return GIMP_EXIT_TEST_SKIPPED;
  g_test_add_func ("/tool-registration/transfer", transfer);
  g_test_add_func ("/tool-registration/cpp-transfer", cpp_transfer);
  g_test_add_func ("/tool-registration/invalid", invalid);
  g_test_add_func ("/tool-registration/duplicate", duplicate);
  g_test_add_func ("/tool-registration/observer-removes", observer_removes);
  g_test_add_func ("/tool-registration/observer-registers", observer_registers);
  g_test_add_func ("/tool-registration/observer-detaches", observer_detaches);
  g_test_add_func ("/tool-registration/observer-last-owner", observer_last_owner);
  g_test_add_func ("/tool-registration/native-catalog", native_catalog);
  g_test_add_func ("/tool-registration/native-standard-removed", native_standard_removed);
  return g_test_run ();
}
