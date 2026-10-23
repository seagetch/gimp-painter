/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Runtime consumers compiled as C, not C++ with extern "C" declarations. */
#include "config.h"
#include <gtk/gtk.h>
#include <string.h>
#include "libgimpconfig/gimpconfig.h"
#include "core/core-types.h"
#include "widgets/widgets-types.h"
#include "core/gimp.h"
#include "core/gimpcontext.h"
#include "core/gimppaintermybrush.h"
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
count_settings_changed (GimpPainterMybrushOptions *options,
                        gpointer                   data)
{
  guint *count = data;
  (void) options;
  ++*count;
}

static void
assert_draft_name (GimpPainterMybrushOptions *options,
                   const gchar               *expected)
{
  GError *error = NULL;
  gchar *name = gimp_painter_mybrush_options_dup_draft_name (options, &error);
  gchar *property = NULL;
  g_assert_no_error (error);
  g_assert_cmpstr (name, ==, expected);
  g_object_get (options, "painter-draft-name", &property, NULL);
  g_assert_cmpstr (property, ==, expected);
  g_free (property);
  g_free (name);
}

static void
options_draft_name_history_copy (void)
{
  GimpPainterMybrushOptions *options = options_new ();
  GimpPainterMybrushOptions *peer = options_new ();
  GimpPainterMybrushOptions *duplicate;
  GimpPainterMybrush *source = GIMP_PAINTER_MYBRUSH (
    gimp_painter_mybrush_new (NULL, "Actual C source"));
  GimpPainterMybrush *next = GIMP_PAINTER_MYBRUSH (
    gimp_painter_mybrush_new (NULL, "Actual C next"));
  const gchar *pending_name = "Actual C unsaved \316\261 variant";
  const gchar *edited_json =
    "{\"version\":3,\"parent_brush_name\":\"independent parent\","
    "\"settings\":{\"opaque\":{\"base_value\":0.37,"
    "\"inputs\":{\"pressure\":[[0,0.2],[1,0.8]]}}},"
    "\"unknown\":{\"retain\":23}}";
  const gchar invalid_utf8[] = { (gchar) 0xff, 0 };
  GError *error = NULL;
  gchar *source_json, *draft_json, *actual, *history_name, *owned_name, *config;
  gboolean dirty = TRUE, name_dirty = FALSE;
  guint settings_changed = 0, history;
  GParamSpec *spec;

  gimp_context_set_painter_mybrush (GIMP_CONTEXT (options), source);
  source_json = gimp_painter_mybrush_dup_json (source, &error);
  g_assert_no_error (error);
  assert_draft_name (options, "Actual C source");
  spec = g_object_class_find_property (G_OBJECT_GET_CLASS (options), "painter-draft-name");
  g_assert_nonnull (spec);
  g_assert_true ((spec->flags & G_PARAM_READABLE) != 0);
  g_assert_false ((spec->flags & G_PARAM_WRITABLE) != 0);
  g_assert_false ((spec->flags & GIMP_CONFIG_PARAM_SERIALIZE) != 0);
  spec = g_object_class_find_property (G_OBJECT_GET_CLASS (options), "painter-name-dirty");
  g_assert_nonnull (spec);
  g_assert_false ((spec->flags & G_PARAM_WRITABLE) != 0);
  g_signal_connect (options, "settings-changed", G_CALLBACK (count_settings_changed),
                    &settings_changed);

  g_assert_true (gimp_painter_mybrush_options_set_draft_name (options, pending_name, &error));
  g_assert_no_error (error);
  g_assert_cmpuint (settings_changed, ==, 0);
  g_object_get (options, "painter-dirty", &dirty, "painter-name-dirty", &name_dirty, NULL);
  g_assert_false (dirty);
  g_assert_true (name_dirty);
  g_assert_true (gimp_context_get_painter_mybrush (GIMP_CONTEXT (options)) == source);
  g_assert_cmpstr (gimp_object_get_name (source), ==, "Actual C source");
  actual = gimp_painter_mybrush_options_dup_json (options, &error);
  g_assert_no_error (error);
  g_assert_cmpstr (actual, ==, source_json);
  g_free (actual);

  /* Name-only history must not turn into a settings edit on restoration. */
  history = gimp_painter_mybrush_options_history_size (options);
  gimp_context_set_painter_mybrush (GIMP_CONTEXT (options), next);
  g_assert_cmpuint (gimp_painter_mybrush_options_history_size (peer), ==, history + 1);
  g_assert_true (gimp_painter_mybrush_options_restore_history (peer, history, &error));
  g_assert_no_error (error);
  assert_draft_name (peer, pending_name);
  g_object_get (peer, "painter-dirty", &dirty, "painter-name-dirty", &name_dirty, NULL);
  g_assert_false (dirty);
  g_assert_true (name_dirty);

  g_assert_true (gimp_painter_mybrush_options_restore_history (options, history, &error));
  g_assert_no_error (error);
  g_assert_true (gimp_painter_mybrush_options_set_json (options, edited_json, &error));
  g_assert_no_error (error);
  draft_json = gimp_painter_mybrush_options_dup_json (options, &error);
  g_assert_no_error (error);
  assert_draft_name (options, pending_name);
  history = gimp_painter_mybrush_options_history_size (options);
  gimp_context_set_painter_mybrush (GIMP_CONTEXT (options), next);
  assert_draft_name (options, "Actual C next");
  history_name = gimp_painter_mybrush_options_history_name (options, history);
  g_assert_cmpstr (history_name, ==, pending_name);
  g_free (history_name);
  g_assert_true (gimp_painter_mybrush_options_restore_history (peer, history, &error));
  g_assert_no_error (error);
  g_assert_true (gimp_context_get_painter_mybrush (GIMP_CONTEXT (peer)) == source);
  assert_draft_name (peer, pending_name);
  actual = gimp_painter_mybrush_options_dup_json (peer, &error);
  g_assert_no_error (error);
  g_assert_cmpstr (actual, ==, draft_json);
  g_free (actual);
  actual = gimp_painter_mybrush_dup_json (source, &error);
  g_assert_no_error (error);
  g_assert_cmpstr (actual, ==, source_json);
  g_assert_cmpstr (gimp_object_get_name (source), ==, "Actual C source");
  g_free (actual);

  duplicate = GIMP_PAINTER_MYBRUSH_OPTIONS (gimp_config_duplicate (GIMP_CONFIG (peer)));
  g_assert_nonnull (duplicate);
  assert_draft_name (duplicate, pending_name);
  g_assert_true (gimp_config_copy (GIMP_CONFIG (peer), GIMP_CONFIG (options), 0));
  assert_draft_name (options, pending_name);
  actual = gimp_painter_mybrush_options_dup_json (options, &error);
  g_assert_no_error (error);
  g_assert_cmpstr (actual, ==, draft_json);
  g_free (actual);
  config = gimp_config_serialize_to_string (GIMP_CONFIG (peer), NULL);
  g_assert_nonnull (config);
  g_assert_null (strstr (config, "painter-draft-name"));
  g_assert_null (strstr (config, "painter-name-dirty"));
  g_assert_null (strstr (config, pending_name));
  g_free (config);

  /* Reset is a settings operation: keep the independent unsaved name. */
  gimp_config_reset (GIMP_CONFIG (duplicate));
  assert_draft_name (duplicate, pending_name);
  g_object_get (duplicate, "painter-name-dirty", &name_dirty, NULL);
  g_assert_true (name_dirty);
  actual = gimp_painter_mybrush_options_dup_json (peer, &error);
  g_assert_no_error (error);
  g_assert_cmpstr (actual, ==, draft_json);
  g_free (actual);
  actual = gimp_painter_mybrush_dup_json (source, &error);
  g_assert_no_error (error);
  g_assert_cmpstr (actual, ==, source_json);
  g_assert_cmpstr (gimp_object_get_name (source), ==, "Actual C source");
  g_free (actual);

  /* Invalid API input leaves both independently owned draft values intact. */
  g_assert_false (gimp_painter_mybrush_options_set_draft_name (peer, NULL, &error));
  g_assert_nonnull (error);
  g_clear_error (&error);
  g_assert_false (gimp_painter_mybrush_options_set_draft_name (peer, invalid_utf8, &error));
  g_assert_nonnull (error);
  g_clear_error (&error);
  assert_draft_name (peer, pending_name);
  owned_name = gimp_painter_mybrush_options_dup_draft_name (peer, &error);
  g_assert_no_error (error);
  g_assert_true (gimp_painter_mybrush_options_set_draft_name (peer, "Later draft", &error));
  g_assert_no_error (error);
  assert_draft_name (peer, "Later draft");
  g_assert_cmpstr (owned_name, ==, pending_name);
  g_object_unref (duplicate);
  g_object_unref (peer);
  g_object_unref (options);
  g_object_unref (source);
  g_object_unref (next);
  /* Transfer-full C text remains valid after its options owners are gone. */
  g_assert_cmpstr (owned_name, ==, pending_name);
  g_free (owned_name);
  g_free (draft_json);
  g_free (source_json);
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
  g_assert_null (gimp_painter_mybrush_options_dup_draft_name (options, &error));
  g_assert_error (error, GIMP_PAINTER_ERROR, GIMP_PAINTER_ERROR_CLOSED);
  g_clear_error (&error);
  g_assert_false (gimp_painter_mybrush_options_set_draft_name (options, "Closed", &error));
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
  GimpPainterMybrush *source = GIMP_PAINTER_MYBRUSH (
    gimp_painter_mybrush_new (NULL, "Actual C transparent source"));
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
  /* A source lookup in place of the draft snapshot would paint no pixels. */
  g_assert_true (gimp_painter_mybrush_set_json (source,
    "{\"version\":3,\"settings\":{\"opaque\":{\"base_value\":0}}}", &error));
  g_assert_no_error (error);
  gimp_context_set_painter_mybrush (GIMP_CONTEXT (options), source);
  g_object_set (options, "opaque", 1., "dabs-per-second", 40.,
                "radius-logarithmic", 1.5, NULL);
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
      if (i == 3)
        {
          g_assert_true (gimp_painter_session_is_active (session));
          g_assert_cmpint (gimp_undo_stack_get_depth (gimp_image_get_undo_stack (image)), ==, 0);
          g_assert_true (gimp_painter_mybrush_options_set_draft_name (
            options, "Named during the same stroke", &error));
          g_assert_no_error (error);
          /* A spurious settings-changed would finish/split this live stroke. */
          g_assert_true (gimp_painter_session_is_active (session));
          g_assert_cmpint (gimp_undo_stack_get_depth (gimp_image_get_undo_stack (image)), ==, 0);
          g_assert_cmpstr (gimp_object_get_name (source), ==, "Actual C transparent source");
        }
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
  g_object_unref (session); g_object_unref (options); g_object_unref (source); g_object_unref (image);
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
  g_test_add_func ("/painter-c/options-draft-name-history-copy", options_draft_name_history_copy);
  g_test_add_func ("/painter-c/options-reject-close", options_reject_and_close);
  g_test_add_func ("/painter-c/session-native-transaction", session_native_transaction);
  g_test_add_func ("/painter-c/session-errors-close", session_errors_and_close);
  result = g_test_run ();
  gimp_test_utils_set_gimp3_directory ("GIMP_TESTING_ABS_TOP_BUILDDIR", "app/tests/gimpdir-output");
  gimp_exit (gimp, TRUE);
  return result;
}
