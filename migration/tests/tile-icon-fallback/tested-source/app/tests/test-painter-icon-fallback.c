/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gegl.h>
#include <gtk/gtk.h>
#include "libgimpbase/gimpbase.h"
#include "libgimpwidgets/gimpwidgets.h"
#include "core/core-types.h"
#include "display/display-types.h"
#include "core/gimp.h"
#include "core/gimpcontext.h"
#include "core/gimpimage.h"
#include "core/gimptoolinfo.h"
#include "display/gimpdisplay.h"
#include "display/gimpdisplayshell.h"
#include "display/gimppaintercanvasui.h"
#include "gimpcoreapp.h"
#include "gimp-app-test-utils.h"
#include "tests.h"

static Gimp *gimp;
static gint layer_fault, tool_fault;
static guint icon_calls, conversion_calls, pixbuf_finalized;
static GdkPixbuf *conversion_target;
static GimpViewable *tool_target;
GdkPixbuf *__real_gtk_icon_theme_load_icon (GtkIconTheme *, const gchar *, gint, GtkIconLookupFlags, GError **);
cairo_surface_t *__real_gimp_cairo_surface_create_from_pixbuf (GdkPixbuf *);
const gchar *__real_gimp_viewable_get_icon_name (GimpViewable *);
static void finalized (gpointer data, GObject *object) { ++*(guint *) data; }
static void storage_finalized (gpointer data) { ++*(guint *) data; }
GdkPixbuf *__wrap_gtk_icon_theme_load_icon (GtkIconTheme *theme, const gchar *name, gint size, GtkIconLookupFlags flags, GError **error)
{
  if (layer_fault && !g_strcmp0 (name, "gimp-visible"))
    {
      GdkPixbuf *original, *unique;
      ++icon_calls;
      if (layer_fault == 1)
        return __real_gtk_icon_theme_load_icon (theme, "painter-test-unregistered-icon-9ef127", size, flags, error);
      if (layer_fault == 2) return NULL;
      original = __real_gtk_icon_theme_load_icon (theme, name, size, flags, error);
      g_assert_nonnull (original);
      unique = gdk_pixbuf_copy (original); g_object_unref (original);
      g_assert_nonnull (unique);
      g_object_weak_ref (G_OBJECT (unique), finalized, &pixbuf_finalized);
      conversion_target = unique;
      return unique;
    }
  return __real_gtk_icon_theme_load_icon (theme, name, size, flags, error);
}
cairo_surface_t *__wrap_gimp_cairo_surface_create_from_pixbuf (GdkPixbuf *pixbuf)
{
  if (pixbuf && pixbuf == conversion_target)
    { conversion_target = NULL; ++conversion_calls; return NULL; }
  return __real_gimp_cairo_surface_create_from_pixbuf (pixbuf);
}
const gchar *__wrap_gimp_viewable_get_icon_name (GimpViewable *viewable)
{
  if (tool_fault && viewable == tool_target)
    return tool_fault == 1 ? "painter-test-unregistered-icon-9ef127" : NULL;
  return __real_gimp_viewable_get_icon_name (viewable);
}
static void drain (void)
{
  gint64 end = g_get_monotonic_time () + 100000;
  do { while (g_main_context_pending (NULL)) g_main_context_iteration (NULL, FALSE); g_usleep (1000); } while (g_get_monotonic_time () < end);
}
static GtkWidget *find (GtkWidget *widget, const gchar *name)
{
  GtkWidget *found = NULL;
  if (!g_strcmp0 (gtk_widget_get_name (widget), name)) return widget;
  if (GTK_IS_CONTAINER (widget))
    {
      GList *children = gtk_container_get_children (GTK_CONTAINER (widget));
      for (GList *l = children; l && !found; l = l->next) found = find (l->data, name);
      g_list_free (children);
    }
  return found;
}
static GimpDisplayShell *open_shell (void)
{
  GimpDisplay *display;
  gimp_set_focused_once (gimp); gimp_test_utils_create_image (gimp, 256, 256); drain ();
  display = gimp_context_get_display (gimp_get_user_context (gimp)); g_assert_nonnull (display);
  g_assert_true (gimp_painter_canvas_ui_get_visible (gimp_display_get_shell (display)));
  return gimp_display_get_shell (display);
}
static void close_shell (GimpDisplayShell *shell)
{
  GimpImage *image = gimp_display_get_image (shell->display);
  gimp_display_delete (shell->display); g_object_unref (image); drain ();
}
static cairo_surface_t *snapshot (GtkWidget *widget)
{
  GtkAllocation a; cairo_surface_t *surface; cairo_t *cr;
  gtk_container_check_resize (GTK_CONTAINER (gtk_widget_get_toplevel (widget)));
  gtk_widget_get_allocation (widget, &a); g_assert_cmpint (a.width, >, 0); g_assert_cmpint (a.height, >, 0);
  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, a.width, a.height);
  cr = cairo_create (surface); gtk_widget_draw (widget, cr);
  g_assert_cmpint (cairo_status (cr), ==, CAIRO_STATUS_SUCCESS); cairo_destroy (cr); cairo_surface_flush (surface);
  return surface;
}
static guint32 pixel (cairo_surface_t *s, gint x, gint y)
{ return *(guint32 *)(cairo_image_surface_get_data (s) + y * cairo_image_surface_get_stride (s) + x * 4); }
static void capture (GimpDisplayShell *shell)
{
  const gchar *path = g_getenv ("GIMP_TEST_ICON_SCREENSHOT");
  GtkWidget *window; GdkWindow *native; GdkPixbuf *pixels; GError *error = NULL;
  if (!path) return;
  window = gtk_widget_get_toplevel (GTK_WIDGET (shell)); gtk_window_resize (GTK_WINDOW (window), 1040, 1800); gtk_window_present (GTK_WINDOW (window)); drain ();
  native = gtk_widget_get_window (window);
  pixels = gdk_pixbuf_get_from_window (native, 0, 0, gdk_window_get_width (native), gdk_window_get_height (native)); g_assert_nonnull (pixels);
  g_assert_true (gdk_pixbuf_save (pixels, path, "png", &error, NULL)); g_assert_no_error (error); g_object_unref (pixels);
}
static void layer_fallback (gconstpointer data)
{
  GimpDisplayShell *shell; GtkWidget *canvas; cairo_surface_t *first, *again; gboolean dark = FALSE;
  layer_fault = GPOINTER_TO_INT (data); icon_calls = conversion_calls = pixbuf_finalized = 0;
  shell = open_shell (); canvas = find (shell->canvas, "painter-layer-canvas"); g_assert_nonnull (canvas);
  first = snapshot (canvas);
  /* Interior of the visibility icon: transparent thumbnail alone is a light
   * checkerboard. A genuine missing-icon glyph must introduce dark pixels. */
  for (gint y = 6; y < 16; y++) for (gint x = 2; x < 12; x++)
    { guint32 p = pixel (first, x, y); dark |= ((p >> 16) & 255) < 64 && ((p >> 8) & 255) < 64 && (p & 255) < 64; }
  g_assert_true (dark); g_assert_cmpuint (icon_calls, ==, 1);
  again = snapshot (canvas); g_assert_cmpuint (icon_calls, ==, 1);
  for (gint y = 6; y < 16; y++) for (gint x = 2; x < 12; x++) g_assert_cmphex (pixel (first,x,y), ==, pixel (again,x,y));
  cairo_surface_destroy (first); cairo_surface_destroy (again);
  if (layer_fault == 3) { g_assert_cmpuint (conversion_calls, ==, 1); g_assert_cmpuint (pixbuf_finalized, ==, 1); }
  else { g_assert_cmpuint (conversion_calls, ==, 0); g_assert_cmpuint (pixbuf_finalized, ==, 0); }
  if (layer_fault == 1) capture (shell);
  close_shell (shell); g_assert_null (conversion_target); layer_fault = 0;
}
static void tool_fallback (gconstpointer data)
{
  GimpDisplayShell *shell; GtkWidget *button, *icon; cairo_surface_t *surface; GtkAllocation a;
  guint released = 0; gboolean visible = FALSE; const gchar *name; GtkIconSize size;
  tool_target = GIMP_VIEWABLE (gimp_context_get_tool (gimp_get_user_context (gimp))); g_assert_nonnull (tool_target);
  tool_fault = GPOINTER_TO_INT (data); shell = open_shell ();
  button = find (shell->canvas, gimp_object_get_name (GIMP_OBJECT (tool_target))); g_assert_true (GTK_IS_BUTTON (button));
  icon = gtk_button_get_image (GTK_BUTTON (button)); g_assert_true (GTK_IS_IMAGE (icon));
  gtk_image_get_icon_name (GTK_IMAGE (icon), &name, &size);
  g_assert_cmpstr (name, ==, tool_fault == 1 ? "painter-test-unregistered-icon-9ef127" : "image-missing");
  g_assert_true (gtk_widget_get_mapped (icon)); surface = snapshot (icon); gtk_widget_get_allocation (icon, &a);
  for (gint y=0;y<a.height;y++) for (gint x=0;x<a.width;x++) visible |= (pixel(surface,x,y)>>24) != 0;
  g_assert_true (visible); cairo_surface_destroy (surface);
  /* GtkWidget destroy clears weak notifications during dispose; qdata tracks
   * final storage release rather than mistaking dispose for finalization. */
  g_object_ref (icon); g_object_set_data_full (G_OBJECT (icon), "icon-storage-finalized", &released, storage_finalized);
  close_shell (shell); g_assert_cmpuint (released, ==, 0); g_object_unref (icon); g_assert_cmpuint (released, ==, 1);
  tool_target = NULL; tool_fault = 0;
}
int main (int argc, char **argv)
{
  gint result; g_test_init (&argc,&argv,NULL); if (!gtk_init_check (&argc,&argv)) return GIMP_EXIT_TEST_SKIPPED;
  gimp_test_utils_setup_menus_path (); gimp = gimp_init_for_gui_testing (TRUE); g_object_set (gimp->config,"painter-canvas-ui",TRUE,NULL);
  g_test_add_data_func ("/painter-icons/layer-missing-name",GINT_TO_POINTER(1),layer_fallback);
  g_test_add_data_func ("/painter-icons/layer-loader-null",GINT_TO_POINTER(2),layer_fallback);
  g_test_add_data_func ("/painter-icons/layer-render-null",GINT_TO_POINTER(3),layer_fallback);
  g_test_add_data_func ("/painter-icons/tool-missing-name",GINT_TO_POINTER(1),tool_fallback);
  g_test_add_data_func ("/painter-icons/tool-null-name",GINT_TO_POINTER(2),tool_fallback);
  g_application_run (gimp->app,0,NULL); result=gimp_core_app_get_exit_status(GIMP_CORE_APP(gimp->app));
  g_application_quit (G_APPLICATION(gimp->app));g_clear_object(&gimp->app);return result;
}
