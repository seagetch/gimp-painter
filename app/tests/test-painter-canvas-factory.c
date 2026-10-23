/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gegl.h>
#include <gtk/gtk.h>
#include "libgimpbase/gimpbase.h"
#include "core/core-types.h"
#include "display/display-types.h"
#include "core/gimp.h"
#include "core/gimpcontext.h"
#include "core/gimpimage.h"
#include "core/gimpimage-perspective-guide.h"
#include "core/gimpperspectiveguide.h"
#include "display/gimpdisplay.h"
#include "display/gimpdisplayshell.h"
#include "display/gimpdisplayshell-items.h"
#include "display/gimpcanvasperspectiveguide.h"
#include "display/gimppaintercanvasui.h"
#include "gimpcoreapp.h"
#include "gimp-app-test-utils.h"
#include "tests.h"
#include "test-painter-canvas-factory.h"
static Gimp *gimp;
static GimpDisplayShell *shell;
static void setup_shell (void) {
  if (shell) return;
  gimp_set_focused_once (gimp);
  gimp_test_utils_create_image (gimp, 256, 256);
  gimp_test_run_mainloop_until_idle ();
  shell = gimp_display_get_shell (gimp_context_get_display (gimp_get_user_context (gimp)));
  g_assert_nonnull (shell);
  g_assert_true (gimp_painter_canvas_ui_get_visible (shell));
}
static void test_fault (gconstpointer data) {
  gint spec = GPOINTER_TO_INT (data), stage = spec / 100;
  gint caller = (spec / 10) % 10, kind = spec % 10;
  GimpCanvasItem *result;
  gpointer weak;
  setup_shell ();
  painter_canvas_fault_arm (kind, stage);
  if (caller == 0) result = gimp_canvas_perspective_guide_new_for_image (shell);
  else result = g_object_new (GIMP_TYPE_CANVAS_PERSPECTIVE_GUIDE, "shell", shell, "follow-image", TRUE, NULL);
  g_assert_true (painter_canvas_fault_consumed ());
  if (stage) g_assert_cmpuint (painter_canvas_fault_connections (), >=, stage);
  g_assert_true (painter_canvas_fault_disconnected ());
  if (caller == 0) g_assert_null (result);
  else {
    g_assert_nonnull (result);
    g_assert_false (g_object_is_floating (result));
    g_assert_true (((GimpCanvasPerspectiveGuide *) result)->binding_failed);
    g_assert_true (painter_canvas_store_closed (G_OBJECT (result)));
    g_assert_null (gimp_canvas_item_get_extents (result));
    /* Connection callbacks must be inert before and after repeated dispose. */
    g_object_notify (G_OBJECT (shell), "display");
    g_object_notify (G_OBJECT (shell->display), "image");
    weak = result; g_object_add_weak_pointer (G_OBJECT (result), &weak);
    g_object_run_dispose (G_OBJECT (result));
    g_object_run_dispose (G_OBJECT (result));
    g_object_unref (result); g_assert_null (weak);
  }
  g_assert_cmpuint (painter_canvas_fault_live (), ==, 0);
  result = gimp_canvas_perspective_guide_new_for_image (shell);
  g_assert_nonnull (result); g_assert_false (g_object_is_floating (result));
  g_assert_false (((GimpCanvasPerspectiveGuide *) result)->binding_failed);
  weak = result; g_object_add_weak_pointer (G_OBJECT (result), &weak);
  g_object_unref (result); g_assert_null (weak);
}
static void test_handle (void) { setup_shell (); painter_canvas_handle_test (shell); }
static void test_shell_init_failure (void) {
  GimpDisplayShell *previous, *failed_shell;
  GimpDisplay *display;
  GimpImage *image;
  GimpPerspectiveGuide *guide;
  setup_shell ();
  previous = shell;
  painter_canvas_fault_arm (3, 0);
  gimp_test_utils_create_image (gimp, 64, 64);
  gimp_test_run_mainloop_until_idle ();
  display = gimp_context_get_display (gimp_get_user_context (gimp));
  failed_shell = gimp_display_get_shell (display);
  g_assert_true (failed_shell != previous);
  g_assert_true (painter_canvas_fault_consumed ());
  g_assert_cmpuint (painter_canvas_fault_live (), ==, 0);
  g_assert_null (failed_shell->perspective_guide);
  image = gimp_display_get_image (display);
  /* A missing optional overlay must not poison shell render/resize/teardown. */
  guide = gimp_perspective_guide_new (77);
  gimp_image_set_perspective_guide (image, guide);
  gimp_perspective_guide_add_vanish_points (guide, 20, 30);
  gimp_perspective_guide_set_vanish_points (guide, 0, 60, 80);
  g_object_notify (G_OBJECT (failed_shell), "display");
  g_object_notify (G_OBJECT (display), "image");
  gimp_image_set_perspective_guide (image, NULL); g_object_unref (guide);
  gtk_widget_queue_draw (GTK_WIDGET (failed_shell));
  gimp_test_run_mainloop_until_idle ();
  gimp_display_delete (display); g_object_unref (image);
  gimp_test_run_mainloop_until_idle ();
}
static void test_c_signature_and_rejection (void) {
  GimpCanvasItem *(*factory) (GimpDisplayShell *) = gimp_canvas_perspective_guide_new_for_image;
  GType (*type) (void) = gimp_canvas_perspective_guide_get_type;
  GObject *wrong;
  g_assert_true (g_type_is_a (type (), GIMP_TYPE_CANVAS_ITEM));
  g_test_expect_message (NULL, G_LOG_LEVEL_CRITICAL, "*GIMP_IS_DISPLAY_SHELL*failed*");
  g_assert_null (factory (NULL));
  g_test_assert_expected_messages ();
  wrong = g_object_new (G_TYPE_OBJECT, NULL);
  g_test_expect_message (NULL, G_LOG_LEVEL_CRITICAL, "*GIMP_IS_DISPLAY_SHELL*failed*");
  g_assert_null (factory ((GimpDisplayShell *) wrong));
  g_test_assert_expected_messages ();
  g_assert_cmpuint (wrong->ref_count, ==, 1); g_object_unref (wrong);
}
#include "test-painter-canvas-explicit-guide.inc"
int main (int argc, char **argv) {
  gint result;
  g_test_init (&argc, &argv, NULL);
  if (!gtk_init_check (&argc, &argv)) return GIMP_EXIT_TEST_SKIPPED;
  gimp_test_utils_setup_menus_path (); gimp = gimp_init_for_gui_testing (TRUE);
  g_object_set (gimp->config, "painter-canvas-ui", TRUE, NULL);
  for (gint stage = 0; stage < 3; ++stage)
    for (gint caller = 0; caller < 2; ++caller)
      for (gint kind = 1; kind <= 4; ++kind) {
        gchar *path = g_strdup_printf ("/painter-canvas-factory/stage%d/caller%d/kind%d", stage, caller, kind);
        g_test_add_data_func (path, GINT_TO_POINTER (stage * 100 + caller * 10 + kind), test_fault);
        g_free (path);
      }
  g_test_add_func ("/painter-canvas-factory/typed-handle", test_handle);
  g_test_add_func ("/painter-canvas-factory/c-signature-and-rejection", test_c_signature_and_rejection);
  g_test_add_func ("/painter-canvas-factory/shell-init-failure", test_shell_init_failure);
  register_explicit_tests ();
  g_application_run (gimp->app, 0, NULL);
  result = gimp_core_app_get_exit_status (GIMP_CORE_APP (gimp->app));
  g_application_quit (G_APPLICATION (gimp->app)); g_clear_object (&gimp->app);
  return result;
}
