/* Independent X11 root capture after a diagnostic GDB run stops.
 * Never linked into GIMP or used as acceptance evidence. No inferior calls. */
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

int
main (int argc, char **argv)
{
  GdkWindow *root;
  GdkPixbuf *pixels;
  GError    *error = NULL;
  gboolean  saved;

  if (argc != 2 || !gdk_init_check (NULL, NULL) ||
      !GDK_IS_X11_DISPLAY (gdk_display_get_default ()))
    {
      g_printerr ("Diagnostic root capture: expected output path and usable X11 display\n");
      return 1;
    }
  root = gdk_get_default_root_window ();
  if (!root || gdk_window_get_width (root) != 1080 ||
      gdk_window_get_height (root) != 1920)
    {
      g_printerr ("Diagnostic root capture: expected physical 1080x1920 root\n");
      return 1;
    }
  pixels = gdk_pixbuf_get_from_window (root, 0, 0, 1080, 1920);
  if (!pixels)
    {
      g_printerr ("Diagnostic root capture: pixel acquisition failed\n");
      return 1;
    }
  saved = gdk_pixbuf_save (pixels, argv[1], "png", &error, NULL);
  if (!saved)
    g_printerr ("Diagnostic root capture: PNG save failed\n");
  g_clear_error (&error);
  g_object_unref (pixels);
  return saved ? 0 : 1;
}
