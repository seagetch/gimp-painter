#include <gtk/gtk.h>
int main (int argc, char **argv) {
  if (argc != 2 || !gtk_init_check (&argc, &argv)) return 2;
  GdkWindow *root = gdk_get_default_root_window ();
  gdk_display_sync (gdk_display_get_default ());
  GdkPixbuf *pixbuf = gdk_pixbuf_get_from_window (root, 0, 0, gdk_window_get_width (root), gdk_window_get_height (root));
  if (!pixbuf) return 3;
  gboolean ok = gdk_pixbuf_save (pixbuf, argv[1], "png", NULL, NULL);
  g_object_unref (pixbuf);
  return ok ? 0 : 4;
}
