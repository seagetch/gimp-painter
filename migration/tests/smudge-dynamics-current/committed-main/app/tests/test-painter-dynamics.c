/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gegl.h>
#include <gio/gio.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <string.h>
#include "libgimpconfig/gimpconfig.h"
#include "libgimpmath/gimpmath.h"
#include "core/core-types.h"
#include "core/gimpdynamics.h"
#include "core/gimpdynamicsoutput.h"
#include "core/gimpcurve.h"

static void defaults_and_enum (void)
{
  GimpDynamics *dynamics = g_object_new (GIMP_TYPE_DYNAMICS, NULL);
  GEnumClass *types = g_type_class_ref (GIMP_TYPE_DYNAMICS_OUTPUT_TYPE);
  GimpCoords coords = GIMP_COORDS_DEFAULT_VALUES;
  GimpDynamicsOutput *blend = gimp_dynamics_get_output (dynamics, GIMP_DYNAMICS_OUTPUT_BLENDING);
  g_assert_cmpint (GIMP_DYNAMICS_OUTPUT_JITTER, ==, 10);
  g_assert_cmpint (GIMP_DYNAMICS_OUTPUT_BLENDING, ==, 11);
  g_assert_cmpint (g_enum_get_value_by_nick (types, "blending")->value, ==, 11);
  g_assert_nonnull (blend);
  g_assert_false (gimp_dynamics_output_is_enabled (blend));
  g_assert_cmpfloat (gimp_dynamics_output_get_linear_value (blend, &coords, NULL, 0), ==, 1.0);
  g_type_class_unref (types);
  g_object_unref (dynamics);
}

static void blending_is_independent (void)
{
  GimpDynamics *dynamics = g_object_new (GIMP_TYPE_DYNAMICS, NULL);
  GimpCoords coords = GIMP_COORDS_DEFAULT_VALUES;
  GimpDynamicsOutput *blend = gimp_dynamics_get_output (dynamics, GIMP_DYNAMICS_OUTPUT_BLENDING);
  GimpDynamicsOutput *flow = gimp_dynamics_get_output (dynamics, GIMP_DYNAMICS_OUTPUT_FLOW);
  g_object_set (blend, "use-pressure", TRUE, NULL);
  coords.pressure = 0.37;
  g_assert_true (gimp_dynamics_output_is_enabled (blend));
  g_assert_false (gimp_dynamics_output_is_enabled (flow));
  g_assert_cmpfloat_with_epsilon (gimp_dynamics_output_get_linear_value (blend, &coords, NULL, 0), .37, 1e-6);
  g_assert_cmpfloat (gimp_dynamics_output_get_linear_value (flow, &coords, NULL, 0), ==, 1);
  g_object_unref (dynamics);
}

static void roundtrip_and_copy (void)
{
  GimpDynamics *dynamics = g_object_new (GIMP_TYPE_DYNAMICS, "name", "Painter blending", NULL);
  GimpDynamics *reopened = g_object_new (GIMP_TYPE_DYNAMICS, NULL);
  GimpDynamicsOutput *blend = gimp_dynamics_get_output (dynamics, GIMP_DYNAMICS_OUTPUT_BLENDING);
  GimpCurve *curve = NULL;
  GError *error = NULL;
  gchar *text, *roundtrip;
  GimpDynamics *copy;
  gboolean enabled = FALSE;
  g_object_set (blend, "use-pressure", TRUE, "use-fade", TRUE, NULL);
  g_object_get (blend, "pressure-curve", &curve, NULL);
  gimp_curve_add_point (curve, .5, .2);
  text = gimp_config_serialize_to_string (GIMP_CONFIG (dynamics), NULL);
  g_assert_nonnull (strstr (text, "blending-output"));
  g_assert_true (gimp_config_deserialize_string (GIMP_CONFIG (reopened), text, -1, NULL, &error));
  g_assert_no_error (error);
  roundtrip = gimp_config_serialize_to_string (GIMP_CONFIG (reopened), NULL);
  g_assert_cmpstr (text, ==, roundtrip);
  g_free (roundtrip);
  g_assert_true (gimp_config_is_equal_to (GIMP_CONFIG (blend), GIMP_CONFIG (gimp_dynamics_get_output (reopened, GIMP_DYNAMICS_OUTPUT_BLENDING))));
  copy = GIMP_DYNAMICS (gimp_config_duplicate (GIMP_CONFIG (dynamics)));
  g_assert_true (gimp_config_is_equal_to (GIMP_CONFIG (blend), GIMP_CONFIG (gimp_dynamics_get_output (copy, GIMP_DYNAMICS_OUTPUT_BLENDING))));
  g_object_set (gimp_dynamics_get_output (copy, GIMP_DYNAMICS_OUTPUT_BLENDING), "use-pressure", FALSE, NULL);
  g_assert_false (gimp_config_is_equal_to (GIMP_CONFIG (blend), GIMP_CONFIG (gimp_dynamics_get_output (copy, GIMP_DYNAMICS_OUTPUT_BLENDING))));
  g_object_get (blend, "use-pressure", &enabled, NULL);
  g_assert_true (enabled);
  g_free (text);g_object_unref (curve);g_object_unref (copy);g_object_unref (reopened);g_object_unref (dynamics);
}

static void legacy_property_name (void)
{
  GimpDynamics *dynamics = g_object_new (GIMP_TYPE_DYNAMICS, NULL);
  GError *error = NULL;
  gboolean pressure = FALSE, velocity = FALSE;
  g_assert_true (gimp_config_deserialize_string (GIMP_CONFIG (dynamics),
    "(blending-output (use-pressure yes) (use-velocity yes))", -1, NULL, &error));
  g_assert_no_error (error);
  g_object_get (gimp_dynamics_get_output (dynamics, GIMP_DYNAMICS_OUTPUT_BLENDING),
                "use-pressure", &pressure, "use-velocity", &velocity, NULL);
  g_assert_true (pressure);g_assert_true (velocity);
  g_object_unref (dynamics);
}

static void empty_curve_edit (void)
{
  GimpCurve *curve = g_object_new (GIMP_TYPE_CURVE, "name", "empty-curve", NULL);
  GimpCurve *copy;
  gimp_curve_clear_points (curve);
  g_assert_cmpint (gimp_curve_get_n_points (curve), ==, 0);
  g_assert_cmpint (gimp_curve_add_point (curve, .5, .3), ==, 0);
  gimp_curve_delete_point (curve, 0);
  g_assert_cmpint (gimp_curve_get_n_points (curve), ==, 0);
  copy = GIMP_CURVE (gimp_config_duplicate (GIMP_CONFIG (curve)));
  g_assert_true (gimp_config_is_equal_to (GIMP_CONFIG (curve), GIMP_CONFIG (copy)));
  g_object_unref (copy);g_object_unref (curve);
}

int main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);gegl_init (NULL, NULL);
  g_test_add_func ("/painter/dynamics/defaults-enum", defaults_and_enum);
  g_test_add_func ("/painter/dynamics/independent-flow", blending_is_independent);
  g_test_add_func ("/painter/dynamics/roundtrip-copy", roundtrip_and_copy);
  g_test_add_func ("/painter/dynamics/legacy-key", legacy_property_name);
  g_test_add_func ("/painter/dynamics/empty-curve", empty_curve_edit);
  return g_test_run ();
}
