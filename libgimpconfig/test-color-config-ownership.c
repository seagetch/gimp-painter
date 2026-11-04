#include "config.h"
#include <gegl.h>
#include <gio/gio.h>
#include "libgimpconfig/gimpconfig.h"

static void
finalized (gpointer data, GObject *object)
{
  guint *count = data;
  (void) object;
  (*count)++;
}

static void
check_rgba (GeglColor *color, double r, double g, double b, double a)
{
  double cr, cg, cb, ca;
  gegl_color_get_rgba (color, &cr, &cg, &cb, &ca);
  g_assert_cmpfloat_with_epsilon (cr, r, 1e-6);
  g_assert_cmpfloat_with_epsilon (cg, g, 1e-6);
  g_assert_cmpfloat_with_epsilon (cb, b, 1e-6);
  g_assert_cmpfloat_with_epsilon (ca, a, 1e-6);
}

static void
initial_lifetime (void)
{
  GimpColorConfig *config = g_object_new (GIMP_TYPE_COLOR_CONFIG, NULL);
  GeglColor *color = NULL, *same = NULL;
  guint count = 0;
  g_object_get (config, "out-of-gamut-color", &color, NULL);
  g_object_get (config, "out-of-gamut-color", &same, NULL);
  g_assert_true (color == same);
  g_object_unref (same);
  check_rgba (color, 1, 0, 1, 1);
  g_object_weak_ref (G_OBJECT (color), finalized, &count);
  g_object_unref (color);
  g_assert_cmpuint (count, ==, 0);
  g_object_unref (config);
  g_assert_cmpuint (count, ==, 1);
}

static void
replacement_lifetime (void)
{
  GimpColorConfig *config = g_object_new (GIMP_TYPE_COLOR_CONFIG, NULL);
  GeglColor *input = gegl_color_new ("black");
  GeglColor *color = NULL;
  guint old_count = 0, new_count = 0, input_count = 0;
  gegl_color_set_rgba (input, .2, .4, .6, 1);
  g_object_weak_ref (G_OBJECT (input), finalized, &input_count);
  g_object_get (config, "out-of-gamut-color", &color, NULL);
  g_object_weak_ref (G_OBJECT (color), finalized, &old_count);
  g_object_unref (color);
  g_object_set (config, "out-of-gamut-color", input, NULL);
  g_assert_cmpuint (old_count, ==, 1);
  g_object_get (config, "out-of-gamut-color", &color, NULL);
  g_assert_true (color != input);
  check_rgba (color, .2, .4, .6, 1);
  g_object_weak_ref (G_OBJECT (color), finalized, &new_count);
  g_object_unref (color);
  g_object_unref (config);
  g_assert_cmpuint (new_count, ==, 1);
  g_assert_cmpuint (input_count, ==, 0);
  g_object_unref (input);
  g_assert_cmpuint (input_count, ==, 1);
}

int
main (int argc, char **argv)
{
  int result;
  g_test_init (&argc, &argv, NULL);
  gegl_init (&argc, &argv);
  g_test_add_func ("/ownership/config/initial", initial_lifetime);
  g_test_add_func ("/ownership/config/replacement", replacement_lifetime);
  result = g_test_run ();
  gegl_exit ();
  return result;
}
