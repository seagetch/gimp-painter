/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Actual C11 callers of the original quick-selector factory. No historical
 * subclass/layout ABI is asserted: the native owner is a GtkMenuButton. */
#include "config.h"
#include <gegl.h>
#include <gtk/gtk.h>
#include "libgimpbase/gimpbase.h"
#include "libgimpwidgets/gimpwidgets.h"
#include "core/core-types.h"
#include "widgets/widgets-types.h"
#include "core/gimp.h"
#include "core/gimpcontainer.h"
#include "core/gimpcontext.h"
#include "paint/painter-mypaint-surface/gimp-painter-options.h"
#include "widgets/gimpmypaintbrushoptions-gui.h"
#include "painter/gimp-painter-binding.h"
#include "gimpcoreapp.h"
#include "gimp-app-test-utils.h"
#include "tests.h"

void gimp_test_mypaint_selector_cpp_register (Gimp *application);
static Gimp *gimp;
static GtkWidget *(* const original_factory) (GObject *) =
  gimp_mypaint_brush_button_with_popup;

static void
storage_released (gpointer data)
{
  ++*(guint *) data;
}

static void
watch_storage (GObject *object, guint *count)
{
  /* GtkWidget weak notifications happen during dispose; qdata instead proves
   * final storage release after all explicit handle/parent refs are gone. */
  g_object_set_data_full (object, "mypaint-selector-test-storage", count,
                          storage_released);
}

static GimpPainterMybrushOptions *
new_options (void)
{
  gimp_set_focused_once (gimp);
  return g_object_new (GIMP_TYPE_PAINTER_MYBRUSH_OPTIONS, "gimp", gimp, NULL);
}

static void
c_original_factory_invalid_and_closed (void)
{
  GObject *wrong = g_object_new (G_TYPE_OBJECT, NULL);
  GimpPainterMybrushOptions *options = new_options ();
  GimpContext *context = gimp_context_new (gimp, "selector disposed context", NULL);
  g_assert_null (original_factory (NULL));
  g_assert_null (original_factory (wrong));
  g_assert_true (gimp_painter_binding_close (G_OBJECT (options), NULL));
  g_assert_null (original_factory (G_OBJECT (options)));
  g_object_run_dispose (G_OBJECT (context));
  g_assert_null (original_factory (G_OBJECT (context)));
  g_object_unref (context);
  g_object_unref (options);
  options = new_options ();
  g_object_run_dispose (G_OBJECT (options));
  g_assert_null (original_factory (G_OBJECT (options)));
  g_object_unref (options);
  g_object_unref (wrong);
}

static void
c_floating_parent_and_final_storage (void)
{
  for (guint model_first = 0; model_first != 2; ++model_first)
    {
      guint model_gone = 0, button_gone = 0;
      GimpPainterMybrushOptions *options = new_options ();
      GtkWidget *button = original_factory (G_OBJECT (options));
      GtkWidget *window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      g_assert_true (GTK_IS_MENU_BUTTON (button));
      g_assert_true (g_object_is_floating (button));
      watch_storage (G_OBJECT (options), &model_gone);
      watch_storage (G_OBJECT (button), &button_gone);
      g_object_ref_sink (window);
      gtk_container_add (GTK_CONTAINER (window), button);
      g_assert_false (g_object_is_floating (button));
      g_object_ref (button);
      if (model_first)
        {
          g_object_unref (options);
          g_assert_cmpuint (model_gone, ==, 0);
        }
      gtk_widget_destroy (window);
      g_assert_cmpuint (button_gone, ==, 0);
      g_object_unref (window);
      g_object_unref (button);
      g_assert_cmpuint (button_gone, ==, 1);
      if (!model_first) g_object_unref (options);
      g_assert_cmpuint (model_gone, ==, 1);
    }
}

static void
c_supplied_context_retained_until_close (void)
{
  guint context_gone = 0;
  GimpContext *context;
  GtkWidget *button;
  gimp_set_focused_once (gimp);
  context = gimp_context_new (gimp, "selector owned source", NULL);
  button = original_factory (G_OBJECT (context));
  g_assert_nonnull (button);
  g_object_ref_sink (button);
  watch_storage (G_OBJECT (context), &context_gone);
  g_object_unref (context);
  g_assert_cmpuint (context_gone, ==, 0);
  gtk_widget_destroy (button);
  g_object_unref (button);
  g_assert_cmpuint (context_gone, ==, 1);
}

static gpointer
reject_worker_close (gpointer data)
{
  /* Only the supported binding admission check runs off-thread. No GTK call
   * or factory operation here implies general GTK thread safety. */
  GError *error = NULL;
  g_assert_false (gimp_painter_binding_close (data, &error));
  g_assert_error (error, GIMP_PAINTER_ERROR, GIMP_PAINTER_ERROR_WRONG_THREAD);
  g_clear_error (&error);
  return NULL;
}

static void
c_terminal_close_and_thread_admission (void)
{
  GimpPainterMybrushOptions *options = new_options ();
  GtkWidget *button = original_factory (G_OBJECT (options));
  GQuark key = g_quark_from_static_string ("gimp-painter-binding-store-v1");
  gpointer store;
  GThread *worker;
  g_assert_nonnull (button);
  g_object_ref_sink (button);
  store = g_object_get_qdata (G_OBJECT (button), key);
  g_assert_nonnull (store);
  worker = g_thread_new ("selector-close-admission", reject_worker_close, button);
  g_thread_join (worker);
  g_assert_true (gimp_painter_binding_close (G_OBJECT (button), NULL));
  g_assert_true (gimp_painter_binding_close (G_OBJECT (button), NULL));
  g_assert_true (g_object_get_qdata (G_OBJECT (button), key) == store);
  gtk_widget_destroy (button);
  worker = g_thread_new ("selector-closed-admission", reject_worker_close, button);
  g_thread_join (worker);
  g_object_unref (button);
  g_object_unref (options);
}

static void
c_unsupported_normal_context (void)
{
  GimpContext *context;
  GimpObject *tool;
  GtkWidget *restored;
  gchar *name;
  gint index, count;
  gimp_set_focused_once (gimp);
  if (!g_test_subprocess ())
    {
      g_test_trap_subprocess (NULL, 20 * G_TIME_SPAN_SECOND, G_TEST_SUBPROCESS_DEFAULT);
      g_test_trap_assert_passed ();
      return;
    }
  /* Gimp has process-global unit/standard-resource initialization. Reuse the
   * harness application in this isolated process instead of creating another
   * Gimp. Keep the registered object/type present for native tool invariants;
   * rename only the fallback lookup key, then restore its exact name. */
  tool = gimp_container_get_child_by_name (gimp->tool_info_list, "gimp-painter-mypaint-tool");
  g_assert_nonnull (tool);
  g_object_ref (tool);
  name = g_strdup (gimp_object_get_name (tool));
  index = gimp_container_get_child_index (gimp->tool_info_list, tool);
  count = gimp_container_get_n_children (gimp->tool_info_list);
  context = gimp_context_new (gimp, "unregistered painter tool context", NULL);
  gimp_context_set_tool (context, NULL);
  gimp_object_set_name (tool, "selector-test-unavailable-painter-tool");
  g_assert_null (gimp_container_get_child_by_name (gimp->tool_info_list, name));
  g_assert_cmpint (gimp_container_get_n_children (gimp->tool_info_list), ==, count);
  g_assert_null (original_factory (G_OBJECT (context)));
  gimp_object_set_name (tool, name);
  g_assert_true (gimp_container_get_child_by_name (gimp->tool_info_list, name) == tool);
  g_assert_cmpint (gimp_container_get_n_children (gimp->tool_info_list), ==, count);
  g_assert_cmpint (gimp_container_get_child_index (gimp->tool_info_list, tool), ==, index);
  restored = original_factory (G_OBJECT (context));
  g_assert_nonnull (restored);
  g_object_ref_sink (restored);
  gtk_widget_destroy (restored);
  g_object_unref (restored);
  g_object_unref (context);
  g_object_unref (tool);
  g_free (name);
}

int
main (int argc, char **argv)
{
  int result;
  g_test_init (&argc, &argv, NULL);
  if (!gtk_init_check (&argc, &argv)) return GIMP_EXIT_TEST_SKIPPED;
  gimp_test_utils_setup_menus_path ();
  gimp = gimp_init_for_gui_testing (TRUE);
  g_test_add_func ("/mypaint-selector/c/01-invalid-closed", c_original_factory_invalid_and_closed);
  g_test_add_func ("/mypaint-selector/c/02-floating-parent-storage", c_floating_parent_and_final_storage);
  g_test_add_func ("/mypaint-selector/c/03-context-retention", c_supplied_context_retained_until_close);
  g_test_add_func ("/mypaint-selector/c/04-terminal-thread-admission", c_terminal_close_and_thread_admission);
  g_test_add_func ("/mypaint-selector/c/05-unsupported-context", c_unsupported_normal_context);
  gimp_test_mypaint_selector_cpp_register (gimp);
  g_application_run (gimp->app, 0, NULL);
  result = gimp_core_app_get_exit_status (GIMP_CORE_APP (gimp->app));
  g_application_quit (G_APPLICATION (gimp->app));
  g_clear_object (&gimp->app);
  return result;
}
