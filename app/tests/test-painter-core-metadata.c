/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"

#include <gegl.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "libgimpconfig/gimpconfig.h"

#include "core/core-types.h"
#include "core/gimpcurve.h"
#include "core/gimpcurve-map.h"
#include "gegl/gimp-gegl-utils.h"

static void
assert_single_point (GimpCurve *curve,
                     gdouble    expected_y)
{
  gdouble x;
  gdouble y;

  g_assert_cmpint (gimp_curve_get_n_points (curve), ==, 1);
  g_assert_cmpint (gimp_curve_get_curve_type (curve), ==, GIMP_CURVE_SMOOTH);
  gimp_curve_get_point (curve, 0, &x, &y);
  g_assert_cmpfloat (x, ==, 0.375);
  g_assert_cmpfloat (y, ==, expected_y);
  g_assert_cmpint (gimp_curve_get_point_type (curve, 0), ==,
                   GIMP_CURVE_POINT_SMOOTH);
}

static void
assert_constant_mapping (GimpCurve *curve,
                         gdouble    expected_y)
{
  gint i;
  gint n_samples = gimp_curve_get_n_samples (curve);

  g_assert_cmpint (n_samples, ==, 256);
  g_assert_false (gimp_curve_is_identity (curve));
  /* Cover both boundaries, every stored sample, and interpolated positions. */
  for (i = 0; i <= 2 * (n_samples - 1); i++)
    g_assert_cmpfloat_with_epsilon (
      gimp_curve_map_value (curve, i / (2.0 * (n_samples - 1))),
      expected_y, 1e-12);
}

static void
one_point_serialized_curve (void)
{
  GimpCurve *curve = GIMP_CURVE (gimp_curve_new ("one-point legacy payload"));
  GimpCurve *reopened;
  GError    *error = NULL;
  gchar     *serialized;

  /* The old count property is compatibility-only.  The coordinate array,
   * not a writable legacy allocation size, must retain this single point. */
  g_assert_true (gimp_config_deserialize_string (
    GIMP_CONFIG (curve),
    "(curve-type smooth) (n-points 1) (points 2 0.375 0.625)",
    -1, NULL, &error));
  g_assert_no_error (error);
  assert_single_point (curve, 0.625);

  /* Deserialize preserves supplied samples; a native edit recalculates them.
   * With one control point the entire mapping is its y coordinate. */
  gimp_curve_set_point (curve, 0, 0.375, 0.75);
  assert_single_point (curve, 0.75);
  assert_constant_mapping (curve, 0.75);

  serialized = gimp_config_serialize_to_string (GIMP_CONFIG (curve), NULL);
  g_assert_nonnull (serialized);
  reopened = GIMP_CURVE (gimp_curve_new ("reopened one-point curve"));
  g_assert_true (gimp_config_deserialize_string (GIMP_CONFIG (reopened),
                                                serialized, -1, NULL, &error));
  g_assert_no_error (error);
  assert_single_point (reopened, 0.75);
  assert_constant_mapping (reopened, 0.75);
  g_assert_true (gimp_config_is_equal_to (GIMP_CONFIG (curve),
                                         GIMP_CONFIG (reopened)));

  g_free (serialized);
  g_object_unref (reopened);
  g_object_unref (curve);
}

static void
mark_spec_released (gpointer data)
{
  gboolean *released = data;

  *released = TRUE;
}

static void
assert_string_metadata (GParamSpec  *pspec,
                        const gchar *multiline)
{
  g_assert_true (G_IS_PARAM_SPEC_STRING (pspec));
  g_assert_cmpstr (pspec->name, ==, "painter-text");
  g_assert_cmpstr (g_param_spec_get_nick (pspec), ==, "Painter text");
  g_assert_cmpstr (g_param_spec_get_blurb (pspec), ==, "Text metadata fixture");
  g_assert_cmpstr (G_PARAM_SPEC_STRING (pspec)->default_value, ==,
                   "first line\nsecond line");
  g_assert_cmpstr (gegl_param_spec_get_property_key (pspec, "multiline"), ==,
                   multiline);
  /* This is the same value-sensitive predicate used by the native prop GUI.
   * A present key with value "false" must not select multiline behavior. */
  g_assert_cmpint (gimp_gegl_param_spec_has_key (pspec, "multiline", "true"),
                   ==, g_strcmp0 (multiline, "true") == 0);
}

static void
multiline_metadata_copy (gconstpointer data)
{
  const gchar *multiline = data;
  GParamSpec  *original;
  GParamSpec  *copy;
  gboolean     original_released = FALSE;

  original = g_param_spec_ref_sink (
    g_param_spec_string ("painter-text", "Painter text", "Text metadata fixture",
                         "first line\nsecond line", G_PARAM_READWRITE));
  g_param_spec_set_qdata_full (
    original, g_quark_from_static_string ("painter-test-spec-released"),
    &original_released, mark_spec_released);
  if (multiline)
    gegl_param_spec_set_property_key (original, "multiline", multiline);

  assert_string_metadata (original, multiline);
  copy = gimp_config_param_spec_duplicate (original);
  g_assert_nonnull (copy);
  g_param_spec_ref_sink (copy);
  g_assert_true (copy != original);
  g_assert_cmpuint (copy->flags, ==,
                    original->flags | GIMP_CONFIG_PARAM_SERIALIZE);
  assert_string_metadata (copy, multiline);

  g_param_spec_unref (original);
  g_assert_true (original_released);
  /* No source-spec reference or borrowed metadata pointer may be required. */
  assert_string_metadata (copy, multiline);
  g_param_spec_unref (copy);
}

int
main (int argc, char **argv)
{
  gint result;

  g_test_init (&argc, &argv, NULL);
  gegl_init (NULL, NULL);

  g_test_add_func ("/painter/core-metadata/curve/one-point-serialized",
                   one_point_serialized_curve);
  g_test_add_data_func ("/painter/core-metadata/multiline/true", "true",
                        multiline_metadata_copy);
  g_test_add_data_func ("/painter/core-metadata/multiline/false", "false",
                        multiline_metadata_copy);
  g_test_add_data_func ("/painter/core-metadata/multiline/absent", NULL,
                        multiline_metadata_copy);

  result = g_test_run ();
  gegl_exit ();

  return result;
}
