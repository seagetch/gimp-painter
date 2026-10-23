/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Runtime consumers compiled as C, not C++ with extern "C" declarations. */
#include "config.h"
#include <gtk/gtk.h>
#include <string.h>
#include "libgimpconfig/gimpconfig.h"
#include "core/core-types.h"
#include "widgets/widgets-types.h"
#include "core/gimp.h"
#include "core/gimpimage.h"
#include "core/gimpimage-undo.h"
#include "core/gimpundostack.h"
#include "core/gimplayer.h"
#include "core/gimplayer-new.h"
#include "paint/painter-mypaint-surface/gimp-painter-options.h"
#include "paint/painter-mypaint-surface/gimp-painter-session.h"
#include "painter/gimp-painter-binding.h"
#include "mypaintbrush-enum-settings.h"
#include "tests.h"
#include "gimp-app-test-utils.h"

static Gimp *gimp;

static GimpPainterMybrushOptions *
options_new (void)
{
  GimpPainterMybrushOptions *options = g_object_new (
    GIMP_TYPE_PAINTER_MYBRUSH_OPTIONS, "gimp", gimp, NULL);
  g_assert_false (options->binding_failed);
  return options;
}

static void
options_owned_roundtrip (void)
{
  GimpPainterMybrushOptions *options = options_new ();
  GimpPainterMybrushOptions *copy = options_new ();
  const GimpVector2 points[] = {{0., 0.25}, {1., 0.75}};
  GError *error = NULL;
  GArray *curve;
  gchar *json, *copied;
  guint i;

  g_assert_true (gimp_painter_mybrush_options_set_curve (
    options, BRUSH_OPAQUE, INPUT_PRESSURE, points, G_N_ELEMENTS (points), &error));
  g_assert_no_error (error);
  json = gimp_painter_mybrush_options_dup_json (options, &error);
  g_assert_no_error (error);
  g_assert_nonnull (json);
  g_assert_true (gimp_painter_mybrush_options_set_json (copy, json, &error));
  g_assert_no_error (error);
  copied = gimp_painter_mybrush_options_dup_json (copy, &error);
  g_assert_no_error (error);
  g_assert_cmpstr (copied, ==, json);
  g_free (copied);
  curve = gimp_painter_mybrush_options_get_curve (
    copy, BRUSH_OPAQUE, INPUT_PRESSURE, &error);
  g_assert_no_error (error);
  g_assert_nonnull (curve);
  g_assert_cmpuint (curve->len, ==, G_N_ELEMENTS (points));
  /* Both returned containers remain owned by C after their owners are gone. */
  g_object_unref (copy);
  g_object_unref (options);
  for (i = 0; i < curve->len; i++)
    {
      GimpVector2 actual = g_array_index (curve, GimpVector2, i);
      g_assert_cmpfloat (actual.x, ==, points[i].x);
      g_assert_cmpfloat (actual.y, ==, points[i].y);
    }
  g_assert_nonnull (strstr (json, "opaque"));
  g_array_unref (curve);
  g_free (json);
}

static void
options_reject_and_close (void)
{
  GimpPainterMybrushOptions *options = options_new ();
  const GimpVector2 invalid[] = {{1., 0.}, {0., 1.}};
  GError *error = NULL;
  gchar *before = gimp_painter_mybrush_options_dup_json (options, &error);
  gchar *after;
  g_assert_no_error (error);
  g_assert_false (gimp_painter_mybrush_options_set_curve (
    options, BRUSH_OPAQUE, INPUT_PRESSURE, invalid, 2, &error));
  g_assert_nonnull (error);
  g_clear_error (&error);
  after = gimp_painter_mybrush_options_dup_json (options, &error);
  g_assert_no_error (error);
  g_assert_cmpstr (after, ==, before);
  g_free (after);
  g_assert_false (gimp_painter_mybrush_options_set_json (options, "{", &error));
  g_assert_nonnull (error);
  g_clear_error (&error);
  after = gimp_painter_mybrush_options_dup_json (options, &error);
  g_assert_no_error (error);
  g_assert_cmpstr (after, ==, before);
  g_free (after);
  g_free (before);
  g_assert_true (gimp_painter_binding_close (G_OBJECT (options), &error));
  g_assert_no_error (error);
  g_assert_null (gimp_painter_mybrush_options_dup_json (options, &error));
  g_assert_error (error, GIMP_PAINTER_ERROR, GIMP_PAINTER_ERROR_CLOSED);
  g_clear_error (&error);
  g_assert_false (gimp_painter_mybrush_options_restore_history (options, 0, &error));
  g_assert_error (error, GIMP_PAINTER_ERROR, GIMP_PAINTER_ERROR_CLOSED);
  g_clear_error (&error);
  g_object_unref (options);
}

static void
read_pixels (GimpDrawable *drawable, guchar *pixels)
{
  gegl_buffer_get (gimp_drawable_get_buffer (drawable),
    GEGL_RECTANGLE (0, 0, 64, 48), 1., babl_format ("R'G'B'A u8"), pixels,
    GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);
}

static void
session_native_transaction (void)
{
  GimpPainterMybrushOptions *options = options_new ();
  GError *error = NULL;
  GimpPainterSession *session;
  GimpImage *image = gimp_image_new (gimp, 64, 48, GIMP_RGB,
                                     GIMP_PRECISION_U8_NON_LINEAR);
  GimpLayer *layer = gimp_layer_new (image, 64, 48, babl_format ("R'G'B'A u8"),
                                    "Actual C session", 1., GIMP_LAYER_MODE_NORMAL);
  GimpDrawable *drawable = GIMP_DRAWABLE (layer);
  GimpCoords coords = GIMP_COORDS_DEFAULT_VALUES;
  guchar before[64 * 48 * 4], painted[sizeof before], actual[sizeof before];
  gboolean split = 123;
  guint i;
  g_assert_true (gimp_image_add_layer (image, layer, NULL, 0, FALSE));
  gegl_buffer_clear (gimp_drawable_get_buffer (drawable), NULL);
  gimp_image_undo_free (image);
  read_pixels (drawable, before);
  g_object_set (options, "dabs-per-second", 40., "radius-logarithmic", 1.5, NULL);
  session = gimp_painter_session_new (options, &error);
  g_assert_no_error (error);
  g_assert_nonnull (session);
  coords.x = 16.; coords.y = 16.; coords.pressure = 0.;
  g_assert_true (gimp_painter_session_hover_to (
    session, drawable, .01, &coords, &split, &error));
  g_assert_no_error (error);
  /* The output is consumed only after the successful return. */
  g_assert_true (split == FALSE || split == TRUE);
  for (i = 0; i < 9; i++)
    {
      coords.pressure = .85; coords.x += 2.; coords.y += 1.; split = 123;
      g_assert_true (gimp_painter_session_stroke_to (
        session, drawable, .05, &coords, &split, &error));
      g_assert_no_error (error);
      g_assert_true (split == FALSE || split == TRUE);
    }
  g_assert_true (gimp_painter_session_finish (session, &error));
  g_assert_no_error (error);
  read_pixels (drawable, painted);
  g_assert_cmpint (memcmp (painted, before, sizeof before), !=, 0);
  g_assert_cmpint (gimp_undo_stack_get_depth (gimp_image_get_undo_stack (image)), ==, 1);
  g_assert_true (gimp_image_undo (image));
  read_pixels (drawable, actual);
  g_assert_cmpmem (actual, sizeof actual, before, sizeof before);
  g_assert_true (gimp_image_redo (image));
  read_pixels (drawable, actual);
  g_assert_cmpmem (actual, sizeof actual, painted, sizeof painted);
  coords.x = 40.; coords.y = 32.;
  g_assert_true (gimp_painter_session_stroke_to (
    session, drawable, .1, &coords, NULL, &error));
  g_assert_no_error (error);
  g_assert_true (gimp_painter_session_cancel (session, &error));
  g_assert_no_error (error);
  read_pixels (drawable, actual);
  g_assert_cmpmem (actual, sizeof actual, painted, sizeof painted);
  g_assert_false (gimp_painter_session_is_active (session));
  g_object_unref (session); g_object_unref (options); g_object_unref (image);
}

static void
session_errors_and_close (void)
{
  GimpPainterMybrushOptions *options = options_new ();
  GError *error = NULL;
  GimpPainterSession *session = gimp_painter_session_new (options, &error);
  GimpCoords coords = GIMP_COORDS_DEFAULT_VALUES;
  gboolean split;
  g_assert_no_error (error);
  g_assert_nonnull (session);
  g_assert_false (gimp_painter_session_stroke_to (
    session, NULL, .01, NULL, &split, &error));
  g_assert_nonnull (error);
  g_clear_error (&error);
  /* On FALSE the optional output is unspecified: do not inspect it. */
  g_assert_true (gimp_painter_binding_close (G_OBJECT (session), &error));
  g_assert_no_error (error);
  g_assert_false (gimp_painter_session_hover_to (
    session, NULL, .01, &coords, &split, &error));
  g_assert_error (error, GIMP_PAINTER_ERROR, GIMP_PAINTER_ERROR_CLOSED);
  g_clear_error (&error);
  g_assert_false (gimp_painter_session_finish (session, &error));
  g_assert_error (error, GIMP_PAINTER_ERROR, GIMP_PAINTER_ERROR_CLOSED);
  g_clear_error (&error);
  g_assert_true (gimp_painter_binding_close (G_OBJECT (session), &error));
  g_assert_no_error (error);
  g_object_unref (session); g_object_unref (options);
}

int
main (int argc, char **argv)
{
  gint result;
  g_test_init (&argc, &argv, NULL);
  gimp_test_utils_set_gimp3_directory ("GIMP_TESTING_ABS_TOP_SRCDIR", "app/tests/gimpdir");
  gimp = gimp_init_for_testing ();
  g_test_add_func ("/painter-c/options-owned-roundtrip", options_owned_roundtrip);
  g_test_add_func ("/painter-c/options-reject-close", options_reject_and_close);
  g_test_add_func ("/painter-c/session-native-transaction", session_native_transaction);
  g_test_add_func ("/painter-c/session-errors-close", session_errors_and_close);
  result = g_test_run ();
  gimp_test_utils_set_gimp3_directory ("GIMP_TESTING_ABS_TOP_BUILDDIR", "app/tests/gimpdir-output");
  gimp_exit (gimp, TRUE);
  return result;
}
