/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gegl.h>
#include <gtk/gtk.h>
#include "libgimpbase/gimpbase.h"
#include "core/core-types.h"
#include "tools/tools-types.h"
#include "core/gimp.h"
#include "tools/gimpbucketfillbrushtool.h"
#include "tools/gimpbucketfillbrushtool.h"
#include "core/gimpcontainer.h"
#include "core/gimpcontext.h"
#include "display/display-types.h"
#include "display/gimpdisplay.h"
#include "display/gimpdisplayshell.h"
#include "display/gimppaintercanvasui.h"
#include "tools/tool_manager.h"
#include "core/gimptoolinfo.h"
#include "paint/gimpfillbrush.h"
#include "painter/gimp-painter-binding.h"
#include "gimpcoreapp.h"
#include "gimp-app-test-utils.h"
#include "tests.h"

void gimp_test_bucket_handles (GimpToolInfo *info);
void gimp_test_bucket_cpp_register (GimpToolRegisterCallback callback, gpointer data);
void gimp_test_bucket_cpp_exceptions (void);
static Gimp *gimp;
static void original_registration_entry (void)
{
  GType (*entry) (void) = gimp_bucket_fill_brush_tool_get_type;
  g_assert_cmpuint (entry (), ==, entry ());
  g_assert_cmpstr (g_type_name (entry ()), ==, "GimpFillBrushTool");
  g_assert_cmpuint (g_type_parent (entry ()), ==, GIMP_TYPE_BRUSH_TOOL);
  g_assert_cmpuint (gimp_bucket_fill_brush_tool_get_type (), ==, GIMP_TYPE_FILL_BRUSH_TOOL);
}
static GimpToolInfo *registered_info (void)
{
  GimpToolInfo *info = GIMP_TOOL_INFO (gimp_container_get_child_by_name (gimp->tool_info_list, "gimp-bucket-fill-brush-tool"));
  g_assert_nonnull (info); return info;
}
static void registered_tool_lifetime (void)
{
  GimpToolInfo *info = registered_info ();
  GimpBucketFillBrushTool *tool, *weak;
  g_assert_cmpuint (info->tool_type, ==, GIMP_TYPE_BUCKET_FILL_BRUSH_TOOL);
  g_assert_cmpuint (info->tool_options_type, ==, GIMP_TYPE_FILL_BRUSH_OPTIONS);
  tool = g_object_new (GIMP_TYPE_BUCKET_FILL_BRUSH_TOOL, "tool-info", info, NULL);
  weak = tool; g_object_add_weak_pointer (G_OBJECT (tool), (gpointer *) &weak);
  g_assert_true (GIMP_IS_BUCKET_FILL_BRUSH_TOOL (tool));
  g_assert_true (GIMP_IS_BUCKET_FILL_BRUSH_TOOL_CLASS (GIMP_BUCKET_FILL_BRUSH_TOOL_GET_CLASS (tool)));
  g_assert_false (g_object_is_floating (tool));
  g_assert_false (gimp_fill_brush_tool_is_pending (tool));
  g_object_run_dispose (G_OBJECT (tool));
  g_assert_false (gimp_fill_brush_tool_is_pending (tool));
  g_object_unref (tool); g_assert_null (weak);
}
typedef struct { guint calls; gboolean recurse; GObject *borrowed; } Registration;
static void registration_callback (GType tool_type, GType options_type, GimpToolOptionsGUIFunc gui,
  GimpContextPropMask mask, const gchar *id, const gchar *label, const gchar *tooltip,
  const gchar *menu, const gchar *accel, const gchar *help_domain, const gchar *help, const gchar *icon, gpointer data)
{
  Registration *state = data;
  ++state->calls;
  g_assert_cmpuint (tool_type, ==, GIMP_TYPE_BUCKET_FILL_BRUSH_TOOL);
  g_assert_cmpuint (options_type, ==, GIMP_TYPE_FILL_BRUSH_OPTIONS);
  g_assert_nonnull (gui); g_assert_cmpuint (mask, ==, GIMP_PAINT_OPTIONS_CONTEXT_MASK);
  g_assert_cmpstr (id, ==, "gimp-bucket-fill-brush-tool");
  g_assert_nonnull (label); g_assert_nonnull (tooltip); g_assert_nonnull (menu);
  g_assert_cmpstr (accel, ==, "bracketright"); g_assert_null (help_domain);
  g_assert_nonnull (help); g_assert_nonnull (icon);
  g_assert_cmpuint (state->borrowed->ref_count, ==, 1);
  if (state->recurse && state->calls == 1) gimp_bucket_fill_brush_tool_register (registration_callback, state);
}
static void callback_forwarding (void)
{
  Registration state = { 0, FALSE, g_object_new (G_TYPE_OBJECT, NULL) };
  gimp_bucket_fill_brush_tool_register (registration_callback, &state); g_assert_cmpuint (state.calls, ==, 1);
  gimp_fill_brush_tool_register (registration_callback, &state); g_assert_cmpuint (state.calls, ==, 2);
  gimp_test_bucket_cpp_register (registration_callback, &state); g_assert_cmpuint (state.calls, ==, 3);
  g_object_unref (state.borrowed);
}
static void callback_reentry (void)
{
  Registration state = { 0, TRUE, g_object_new (G_TYPE_OBJECT, NULL) };
  gimp_bucket_fill_brush_tool_register (registration_callback, &state); g_assert_cmpuint (state.calls, ==, 2);
  g_object_unref (state.borrowed);
}
static void null_callback (void)
{
  gimp_bucket_fill_brush_tool_register (NULL, NULL);
  gimp_fill_brush_tool_register (NULL, (gpointer) 1);
}
static void typed_handles (void) { gimp_test_bucket_handles (registered_info ()); }
static void actual_registered_ui (void)
{
  GimpToolInfo *info = registered_info ();
  GimpDisplay *display;
  GimpDisplayShell *shell;
  GtkWidget *window;
  const gchar *path = g_getenv ("GIMP_TEST_BUCKET_FILL_SCREENSHOT");
  gboolean enabled = FALSE;
  gimp_test_utils_create_image (gimp, 128, 128);
  gimp_test_run_mainloop_until_idle ();
  gimp_context_set_tool (gimp_get_user_context (gimp), info);
  g_assert_true (GIMP_IS_BUCKET_FILL_BRUSH_TOOL (tool_manager_get_active (gimp)));
  display = gimp_context_get_display (gimp_get_user_context (gimp)); g_assert_nonnull (display);
  shell = gimp_display_get_shell (display); g_assert_nonnull (shell);
  g_object_get (gimp->config, "painter-canvas-ui", &enabled, NULL); g_assert_true (enabled);
  g_assert_true (gimp_painter_canvas_ui_get_visible (shell));
  window = gtk_widget_get_toplevel (GTK_WIDGET (shell));
  gtk_window_resize (GTK_WINDOW (window), 1040, 1800);
  for (guint i = 0; i < 20; ++i) { gimp_test_run_mainloop_until_idle (); g_usleep (10000); }
  if (path) {
    GdkWindow *native = gtk_widget_get_window (window);
    GdkPixbuf *pixels;
    GError *error = NULL;
    g_assert_nonnull (native);
    g_assert_cmpint (gdk_screen_get_width (gtk_widget_get_screen (window)), ==, 1080);
    g_assert_cmpint (gdk_screen_get_height (gtk_widget_get_screen (window)), ==, 1920);
    pixels = gdk_pixbuf_get_from_window (native, 0, 0, gdk_window_get_width (native), gdk_window_get_height (native));
    g_assert_nonnull (pixels); g_assert_true (gdk_pixbuf_save (pixels, path, "png", &error, NULL));
    g_assert_no_error (error); g_object_unref (pixels);
  }
}
int main (int argc, char **argv)
{
  gint result;
  g_test_init (&argc, &argv, NULL);
  if (!gtk_init_check (&argc, &argv)) return GIMP_EXIT_TEST_SKIPPED;
  gimp_test_utils_setup_menus_path ();
  gimp = gimp_init_for_gui_testing (TRUE);
  g_object_set (gimp->config, "painter-canvas-ui", TRUE, NULL);
  g_test_add_func ("/bucket-fill-entry/original-symbols", original_registration_entry);
  g_test_add_func ("/bucket-fill-entry/registered-lifetime", registered_tool_lifetime);
  g_test_add_func ("/bucket-fill-entry/callback-forwarding", callback_forwarding);
  g_test_add_func ("/bucket-fill-entry/callback-reentry", callback_reentry);
  g_test_add_func ("/bucket-fill-entry/null-callback", null_callback);
  g_test_add_func ("/bucket-fill-entry/typed-handles", typed_handles);
  g_test_add_func ("/bucket-fill-entry/exception-containment", gimp_test_bucket_cpp_exceptions);
  g_test_add_func ("/bucket-fill-entry/actual-registered-ui", actual_registered_ui);
  g_application_run (gimp->app, 0, NULL);
  result = gimp_core_app_get_exit_status (GIMP_CORE_APP (gimp->app));
  g_application_quit (G_APPLICATION (gimp->app)); g_clear_object (&gimp->app); return result;
}
