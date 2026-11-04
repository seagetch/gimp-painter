#include "config.h"
#include <gegl.h>
#include <gtk/gtk.h>
#include "libgimpbase/gimpbase.h"
#include "libgimpconfig/gimpconfig.h"
#include "config/config-types.h"
#include "config/gimpdisplayconfig.h"
#include "display/display-types.h"
#include "display/gimpcanvas.h"

static void
weak_notified (gpointer data, GObject *object)
{
  (void) object;
  (*(guint *) data)++;
}

static void
padding_lifetime (gconstpointer data)
{
  gboolean realized = GPOINTER_TO_INT (data);
  GimpDisplayConfig *config = g_object_new (GIMP_TYPE_DISPLAY_CONFIG, NULL);
  GtkWidget *widget = gimp_canvas_new (config);
  GimpCanvas *canvas = GIMP_CANVAS (widget);
  GtkWidget *window = NULL;
  GeglColor *input = gegl_color_new ("white");
  GeglColor *copy;
  guint old_gone = 0, last_gone = 0, input_gone = 0, canvas_disposed = 0;
  gdouble r, g, b, a;

  g_object_ref_sink (widget);
  g_object_weak_ref (G_OBJECT (widget), weak_notified, &canvas_disposed);
  g_object_weak_ref (G_OBJECT (input), weak_notified, &input_gone);
  gimp_canvas_set_padding (canvas, GIMP_CANVAS_PADDING_MODE_CUSTOM, input);
  g_assert_true (canvas->padding_color != input);
  g_object_weak_ref (G_OBJECT (canvas->padding_color), weak_notified, &old_gone);
  gegl_color_set_rgba (input, .2, .4, .6, 1);
  gimp_canvas_set_padding (canvas, GIMP_CANVAS_PADDING_MODE_CUSTOM, input);
  g_assert_cmpuint (old_gone, ==, 1);
  copy = canvas->padding_color; /* borrowed, so this does not hide the leak */
  g_object_weak_ref (G_OBJECT (copy), weak_notified, &last_gone);
  g_assert_true (copy != input);
  gegl_color_get_rgba (copy, &r, &g, &b, &a);
  g_assert_cmpfloat_with_epsilon (r, .2, 1e-6);
  g_assert_cmpfloat_with_epsilon (g, .4, 1e-6);
  g_assert_cmpfloat_with_epsilon (b, .6, 1e-6);
  g_assert_cmpfloat_with_epsilon (a, 1, 1e-6);

  if (realized)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      g_object_ref_sink (window);
      gtk_container_add (GTK_CONTAINER (window), widget);
      gtk_widget_show_all (window);
      g_assert_true (gtk_widget_get_realized (widget));
      gtk_widget_hide (widget);
      g_assert_false (gtk_widget_get_mapped (widget));
      gtk_widget_unrealize (widget);
      g_assert_false (gtk_widget_get_realized (widget));
      g_assert_cmpuint (last_gone, ==, 0);
      g_assert_true (canvas->padding_color == copy);
      gtk_widget_realize (widget);
      gtk_widget_show (widget);
      g_assert_true (gtk_widget_get_realized (widget));
      g_assert_true (canvas->padding_color == copy);
      gegl_color_get_rgba (copy, &r, &g, &b, &a);
      g_assert_cmpfloat_with_epsilon (r, .2, 1e-6);
      g_assert_cmpfloat_with_epsilon (g, .4, 1e-6);
      g_assert_cmpfloat_with_epsilon (b, .6, 1e-6);
      g_assert_cmpfloat_with_epsilon (a, 1, 1e-6);
      gtk_widget_destroy (window);
      g_object_unref (window);
    }
  else
    {
      g_assert_false (gtk_widget_get_realized (widget));
      gtk_widget_destroy (widget);
    }

  /* GtkWidget destruction runs disposal and notifies weak observers even
   * while our strong reference prevents finalization. The color has not been
   * explicitly disposed: its observer tracks release of the owned copy. */
  g_assert_cmpuint (canvas_disposed, ==, 1);
  g_assert_cmpuint (last_gone, ==, 0);
  g_object_unref (widget);
  g_assert_cmpuint (canvas_disposed, ==, 1);
  g_assert_cmpuint (last_gone, ==, 1);
  g_assert_cmpuint (input_gone, ==, 0);
  g_object_unref (input);
  g_assert_cmpuint (input_gone, ==, 1);
  g_object_unref (config); /* canvas config is borrowed */
}

int
main (int argc, char **argv)
{
  int result;
  g_test_init (&argc, &argv, NULL);
  if (! gtk_init_check (&argc, &argv))
    return 77;
  gegl_init (&argc, &argv);
  g_test_add_data_func ("/ownership/canvas/unrealized-padding", GINT_TO_POINTER (FALSE), padding_lifetime);
  g_test_add_data_func ("/ownership/canvas/realized-padding", GINT_TO_POINTER (TRUE), padding_lifetime);
  result = g_test_run ();
  gegl_exit ();
  return result;
}
