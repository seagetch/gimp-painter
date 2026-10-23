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

#include "test-painter-layer-initializer.h"
#include "widgets/gimplayertileview.h"

/* Every construction call is compiled as C11. The separate C++ wrapper
 * throws only inside the real production initializer's C++ operation. */
static Gimp *gimp;
static void test_init (gconstpointer data) {
  gint spec = GPOINTER_TO_INT (data), stage = spec / 100;
  gint caller = (spec / 10) % 10, kind = spec % 10;
  GtkWidget *result;
  gpointer weak;
  g_test_expect_message (NULL, G_LOG_LEVEL_WARNING, "Cannot construct painter layer tiles: *");
  painter_layer_init_fault_arm (kind, stage != 0);
  if (caller == 0) result = GTK_WIDGET (gimp_layer_tile_view_new ());
  else if (caller == 1) result = gimp_painter_layer_tiles_new (gimp_get_user_context (gimp), NULL);
  else result = g_object_new (GIMP_TYPE_PAINTER_LAYER_TILES, NULL);
  g_test_assert_expected_messages ();
  g_assert_true (painter_layer_init_fault_consumed ());
  g_assert_cmpuint (painter_layer_init_observed (), >, stage ? 1 : 0);
  if (caller < 2) g_assert_null (result);
  else {
    g_assert_nonnull (result);
    g_object_ref_sink (result);
    g_assert_true (GIMP_PAINTER_LAYER_TILES (result)->binding_failed);
    g_assert_true (painter_layer_init_store_closed (G_OBJECT (result)));
    g_assert_null (gimp_painter_layer_tiles_popup (GIMP_PAINTER_LAYER_TILES (result), FALSE));
    g_assert_false (gimp_painter_layer_tiles_long_press_pending (GIMP_PAINTER_LAYER_TILES (result)));
    weak = result; g_object_add_weak_pointer (G_OBJECT (result), &weak);
    gtk_widget_destroy (result); g_object_unref (result);
    g_assert_null (weak);
  }
  g_assert_cmpuint (painter_layer_init_live (), ==, 0);
  /* A failed GObject instance never poisons later construction. */
  result = gimp_painter_layer_tiles_new (gimp_get_user_context (gimp), NULL);
  g_assert_nonnull (result); g_object_ref_sink (result);
  g_assert_false (GIMP_PAINTER_LAYER_TILES (result)->binding_failed);
  weak = result; g_object_add_weak_pointer (G_OBJECT (result), &weak);
  gtk_widget_destroy (result); g_object_unref (result);
  g_assert_null (weak);
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

for (gint stage = 0; stage < 2; ++stage)
    for (gint caller = 0; caller < 3; ++caller)
      for (gint kind = 1; kind <= 4; ++kind) {
        gchar *path = g_strdup_printf ("/painter-layer-initializer/stage%d/caller%d/kind%d", stage, caller, kind);
        g_test_add_data_func (path, GINT_TO_POINTER (stage * 100 + caller * 10 + kind), test_init);
        g_free (path);
      }


  g_application_run (gimp->app, 0, NULL);
  result = gimp_core_app_get_exit_status (GIMP_CORE_APP (gimp->app));
  g_application_quit (G_APPLICATION (gimp->app));
  g_clear_object (&gimp->app);
  return result;
}
