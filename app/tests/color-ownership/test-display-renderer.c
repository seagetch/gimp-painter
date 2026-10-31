#include "config.h"
#include <gegl.h>
#include <gtk/gtk.h>
#include "libgimpbase/gimpbase.h"
#include "libgimpcolor/gimpcolor.h"
#include "libgimpconfig/gimpconfig.h"
#include "config/config-types.h"
#include "config/gimpdisplayoptions.h"
#include "core/core-types.h"
#include "widgets/widgets-types.h"
#include "widgets/gimpviewrenderer.h"

static void
finalized (gpointer data, GObject *object)
{
  (void) object;
  (*(guint *) data)++;
}

static void
display_parity (void)
{
  GType types[] = {GIMP_TYPE_DISPLAY_OPTIONS, GIMP_TYPE_DISPLAY_OPTIONS_FULLSCREEN,
                   GIMP_TYPE_DISPLAY_OPTIONS_NO_IMAGE};
  guint ti;
  for (ti = 0; ti < G_N_ELEMENTS (types); ti++)
    {
      GimpDisplayOptions *options = g_object_new (types[ti], NULL);
      GParamSpec **specs;
      guint n, i, gone = 0;
      gdouble r, g, b, a, expected = ti == 1 ? 0 : 1;
      gegl_color_get_rgba (options->padding_color, &r, &g, &b, &a);
      g_assert_cmpfloat_with_epsilon (r, expected, 1e-6);
      g_assert_cmpfloat_with_epsilon (g, expected, 1e-6);
      g_assert_cmpfloat_with_epsilon (b, expected, 1e-6);
      g_assert_cmpfloat_with_epsilon (a, 1, 1e-6);
      g_assert_false (options->padding_mode_set);
      specs = g_object_class_list_properties (G_OBJECT_GET_CLASS (options), &n);
      for (i = 0; i < n; i++)
        {
          GValue value = G_VALUE_INIT;
          if (! (specs[i]->flags & G_PARAM_READABLE)) continue;
          g_value_init (&value, G_PARAM_SPEC_VALUE_TYPE (specs[i]));
          g_object_get_property (G_OBJECT (options), specs[i]->name, &value);
          if (G_VALUE_HOLDS_OBJECT (&value))
            {
              GeglColor *color = g_value_get_object (&value);
              g_assert_true (GEGL_IS_COLOR (color));
              gegl_color_get_rgba (color, &r, &g, &b, &a);
              g_print ("PARITY %s %s color %.17g %.17g %.17g %.17g\n",
                       g_type_name (types[ti]), specs[i]->name, r, g, b, a);
            }
          else
            {
              gchar *text = g_strdup_value_contents (&value);
              g_print ("PARITY %s %s %s\n", g_type_name (types[ti]), specs[i]->name, text);
              g_free (text);
            }
          g_value_unset (&value);
        }
      g_free (specs);
      g_object_weak_ref (G_OBJECT (options->padding_color), finalized, &gone);
      g_object_unref (options);
      g_assert_cmpuint (gone, ==, 1);
    }
}

static void
renderer_smoke (void)
{
  GimpViewRenderer *renderer = g_object_new (GIMP_TYPE_VIEW_RENDERER, NULL);
  GimpViewBorderType types[] = {GIMP_VIEW_BORDER_BLACK, GIMP_VIEW_BORDER_WHITE,
    GIMP_VIEW_BORDER_GREEN, GIMP_VIEW_BORDER_RED, GIMP_VIEW_BORDER_STYLE_FG,
    GIMP_VIEW_BORDER_STYLE_BG};
  guint round, i, gone = 0;
  g_assert_cmpint (gimp_view_renderer_get_border_type (renderer), ==, GIMP_VIEW_BORDER_STYLE_FG);
  g_object_weak_ref (G_OBJECT (renderer), finalized, &gone);
  for (round = 0; round < 3; round++)
    for (i = 0; i < G_N_ELEMENTS (types); i++)
      {
        gimp_view_renderer_set_border_type (renderer, types[i]);
        g_assert_cmpint (gimp_view_renderer_get_border_type (renderer), ==, types[i]);
        gimp_view_renderer_remove_idle (renderer);
      }
  g_object_unref (renderer);
  g_assert_cmpuint (gone, ==, 1); /* Renderer lifetime only, NOT color lifetime. */
}

int
main (int argc, char **argv)
{
  int result;
  g_test_init (&argc, &argv, NULL);
  gegl_init (&argc, &argv);
  g_test_add_func ("/ownership/display/parity", display_parity);
  g_test_add_func ("/ownership/renderer/public-smoke", renderer_smoke);
  result = g_test_run ();
  gegl_exit ();
  return result;
}
