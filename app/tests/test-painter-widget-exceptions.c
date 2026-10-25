/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"

#include <gegl.h>
#include <gtk/gtk.h>

#include "libgimpbase/gimpbase.h"
#include "libgimpwidgets/gimpwidgets.h"
#include "core/core-types.h"
#include "widgets/widgets-types.h"
#include "display/display-types.h"
#include "core/gimp.h"
#include "core/gimpcontext.h"
#include "core/gimpimage.h"
#include "core/gimplayer-new.h"
#include "display/gimpdisplay.h"
#include "display/gimpdisplayshell.h"
#include "display/gimppaintercanvasui.h"
#include "widgets/gimppainterlayertiles.h"
#include "gimpcoreapp.h"
#include "gimp-app-test-utils.h"
#include "tests.h"

#include "test-painter-widget-exception-fault.h"

/* Keep every call under test in an actual C translation unit. The test-only
 * C++ wrapper throws inside the real use() helper, before its operation starts.
 * This is containment/pre-operation preservation coverage, not a transactional
 * rollback claim or coverage of dispatch(), initialization, or cleanup. */
static Gimp *gimp;

static void
drain (void)
{
  for (gint i = 0; i < 300 && g_main_context_pending (NULL); ++i)
    g_main_context_iteration (NULL, FALSE);
}

static GimpDisplayShell *
new_shell (void)
{
  GimpDisplay *display;

  gimp_set_focused_once (gimp);
  gimp_test_utils_create_image (gimp, 256, 256);
  drain ();
  display = gimp_context_get_display (gimp_get_user_context (gimp));
  g_assert_nonnull (display);
  return gimp_display_get_shell (display);
}

static void
close_shell (GimpDisplayShell *shell)
{
  GimpImage *image = gimp_display_get_image (shell->display);

  gimp_display_delete (shell->display);
  g_object_unref (image);
  drain ();
}

static void
expect_exception (const gchar            *prefix,
                  PainterWidgetException  exception)
{
  gchar *message = g_strdup_printf ("%s: %s", prefix,
                                    exception == PAINTER_WIDGET_EXCEPTION_STANDARD ?
                                    PAINTER_WIDGET_EXCEPTION_MESSAGE :
                                    "Unknown C++ exception");

  /* The C++ widget sources do not receive the C-only per-library log domain. */
  g_test_expect_message (NULL, G_LOG_LEVEL_WARNING, message);
  g_free (message);
}

static void
assert_exception_consumed (void)
{
  g_test_assert_expected_messages ();
  g_assert_true (painter_widget_exception_fault_consumed ());
}

static void
canvas_set_visible (gconstpointer data)
{
  PainterWidgetException exception = GPOINTER_TO_INT (data);
  GimpDisplayShell   *shell = new_shell ();

  /* Settle queued preference reconciliation before establishing the state
   * whose preservation is checked across this synchronous fault boundary. */
  drain ();
  gimp_painter_canvas_ui_set_visible (shell, FALSE);
  g_assert_false (gimp_painter_canvas_ui_get_visible (shell));

  expect_exception ("Painter canvas UI", exception);
  painter_widget_exception_fault_arm (G_OBJECT (shell), exception);
  gimp_painter_canvas_ui_set_visible (shell, TRUE);
  assert_exception_consumed ();
  g_assert_false (gimp_painter_canvas_ui_get_visible (shell));

  /* The failed request neither changed the state nor poisoned normal use. */
  gimp_painter_canvas_ui_set_visible (shell, TRUE);
  g_assert_true (gimp_painter_canvas_ui_get_visible (shell));
  drain ();
  gimp_painter_canvas_ui_set_visible (shell, FALSE);
  g_assert_false (gimp_painter_canvas_ui_get_visible (shell));
  close_shell (shell);
}

static void
canvas_attach_dock (gconstpointer data)
{
  PainterWidgetException exception = GPOINTER_TO_INT (data);
  GimpDisplayShell   *shell = new_shell ();
  GtkWidget         *parent = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  GtkWidget         *child = gtk_label_new ("Exception boundary dock");
  gpointer           weak_child = child;
  gboolean           attached;

  g_object_ref_sink (parent);
  gtk_container_add (GTK_CONTAINER (parent), child);
  g_object_ref (child);
  g_object_add_weak_pointer (G_OBJECT (child), &weak_child);
  gimp_painter_canvas_ui_set_visible (shell, TRUE);
  drain ();
  g_assert_true (gtk_widget_get_parent (child) == parent);

  expect_exception ("Painter canvas UI", exception);
  painter_widget_exception_fault_arm (G_OBJECT (shell), exception);
  attached = gimp_painter_canvas_ui_attach_dock (shell, child);
  assert_exception_consumed ();
  g_assert_false (attached);
  g_assert_true (GTK_IS_LABEL (child));
  g_assert_true (gtk_widget_get_parent (child) == parent);

  g_assert_true (gimp_painter_canvas_ui_attach_dock (shell, child));
  g_assert_true (gtk_widget_get_ancestor (child, GIMP_TYPE_DISPLAY_SHELL) ==
                 GTK_WIDGET (shell));
  gimp_painter_canvas_ui_detach_dock (shell);
  g_assert_true (gtk_widget_get_parent (child) == parent);

  gtk_widget_destroy (parent);
  g_object_unref (child);
  g_object_unref (parent);
  g_assert_null (weak_child);
  close_shell (shell);
}

static void
layer_tiles_popup (gconstpointer data)
{
  PainterWidgetException exception = GPOINTER_TO_INT (data);
  GimpImage         *image;
  GimpLayer         *layer;
  GtkWidget         *window;
  GtkWidget         *tiles;
  GtkWidget         *popup;
  gpointer           weak_tiles;
  gpointer           weak_popup;

  gimp_set_focused_once (gimp);
  image = gimp_image_new (gimp, 64, 64, GIMP_RGB, GIMP_PRECISION_U8_NON_LINEAR);
  layer = gimp_layer_new (image, 64, 64,
                          gimp_image_get_layer_format (image, TRUE),
                          "Exception boundary layer", 1.0,
                          GIMP_LAYER_MODE_NORMAL);
  gimp_image_add_layer (image, layer, NULL, 0, FALSE);
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  tiles = gimp_painter_layer_tiles_new (gimp_get_user_context (gimp), image);
  g_object_ref_sink (window);
  g_object_ref_sink (tiles);
  weak_tiles = tiles;
  g_object_add_weak_pointer (G_OBJECT (tiles), &weak_tiles);
  gtk_window_set_default_size (GTK_WINDOW (window), 220, 700);
  gtk_container_add (GTK_CONTAINER (window), tiles);
  gtk_widget_show_all (window);
  drain ();

  expect_exception ("Painter layer tiles", exception);
  painter_widget_exception_fault_arm (G_OBJECT (tiles), exception);
  popup = gimp_painter_layer_tiles_popup (GIMP_PAINTER_LAYER_TILES (tiles), FALSE);
  assert_exception_consumed ();
  g_assert_null (popup);
  g_assert_true (GIMP_IS_PAINTER_LAYER_TILES (tiles));
  g_assert_true (gtk_widget_get_parent (tiles) == window);
  g_assert_true (gimp_image_editor_get_image (GIMP_IMAGE_EDITOR (tiles)) == image);

  popup = gimp_painter_layer_tiles_popup (GIMP_PAINTER_LAYER_TILES (tiles), FALSE);
  g_assert_nonnull (popup);
  g_assert_true (GTK_IS_WINDOW (popup));
  g_assert_cmpint (gtk_window_get_window_type (GTK_WINDOW (popup)), ==, GTK_WINDOW_POPUP);
  g_assert_cmpstr (gtk_widget_get_name (popup), ==, "painter-layer-properties-popup");
  g_assert_true (gtk_widget_get_mapped (tiles));
  g_object_ref (popup);
  weak_popup = popup;
  g_object_add_weak_pointer (G_OBJECT (popup), &weak_popup);
  drain ();
  gimp_painter_layer_tiles_cancel_interaction (GIMP_PAINTER_LAYER_TILES (tiles));
  drain ();
  g_assert_false (gtk_widget_get_visible (popup));
  g_assert_false (gtk_widget_has_grab (popup));
  g_object_unref (popup);
  g_assert_null (weak_popup);

  gtk_widget_destroy (window);
  g_object_unref (window);
  g_object_unref (tiles);
  drain ();
  g_assert_null (weak_tiles);
  g_object_unref (image);
}

int
main (int argc, char **argv)
{
  gint result;

  g_test_init (&argc, &argv, NULL);
  if (! gtk_init_check (&argc, &argv))
    return GIMP_EXIT_TEST_SKIPPED;
  gimp_test_utils_setup_menus_path ();
  gimp = gimp_init_for_gui_testing (TRUE);

#define ADD_CASE(path, function, exception) \
  g_test_add_data_func ("/painter-widget-exceptions/" path, \
                        GINT_TO_POINTER (exception), function)
  ADD_CASE ("standard/canvas-set-visible", canvas_set_visible, PAINTER_WIDGET_EXCEPTION_STANDARD);
  ADD_CASE ("standard/canvas-attach-dock", canvas_attach_dock, PAINTER_WIDGET_EXCEPTION_STANDARD);
  ADD_CASE ("standard/layer-tiles-popup", layer_tiles_popup, PAINTER_WIDGET_EXCEPTION_STANDARD);
  ADD_CASE ("unknown/canvas-set-visible", canvas_set_visible, PAINTER_WIDGET_EXCEPTION_UNKNOWN);
  ADD_CASE ("unknown/canvas-attach-dock", canvas_attach_dock, PAINTER_WIDGET_EXCEPTION_UNKNOWN);
  ADD_CASE ("unknown/layer-tiles-popup", layer_tiles_popup, PAINTER_WIDGET_EXCEPTION_UNKNOWN);
#undef ADD_CASE

  g_application_run (gimp->app, 0, NULL);
  result = gimp_core_app_get_exit_status (GIMP_CORE_APP (gimp->app));
  g_application_quit (G_APPLICATION (gimp->app));
  g_clear_object (&gimp->app);
  return result;
}
