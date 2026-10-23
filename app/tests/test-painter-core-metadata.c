/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"

#include <gegl.h>
#include <gtk/gtk.h>

#include "libgimpconfig/gimpconfig.h"

#include "core/core-types.h"
#include "widgets/widgets-types.h"
#include "core/gimp.h"
#include "core/gimpcurve.h"
#include "core/gimpcurve-map.h"
#include "core/gimpimage.h"
#include "core/gimplayer.h"
#include "core/gimplayer-new.h"
#include "gegl/gimp-gegl-utils.h"

#include "tests.h"
#include "gimp-app-test-utils.h"

static Gimp *gimp;

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

typedef struct
{
  const gchar  *path;
  GeglRectangle extent;
  gint          new_offset_y;
  gboolean      implicit_bounds;
} DrawableBufferCase;

typedef struct
{
  GeglBuffer   *buffer;
  GeglRectangle local_region;
  GeglRectangle image_region;
} DrawableUpdate;

typedef struct
{
  guint          count;
  DrawableUpdate updates[2];
} DrawableUpdateLog;

static void
record_drawable_update (GimpDrawable      *drawable,
                        gint               x,
                        gint               y,
                        gint               width,
                        gint               height,
                        DrawableUpdateLog *log)
{
  DrawableUpdate *update;

  g_assert_cmpuint (log->count, <, G_N_ELEMENTS (log->updates));
  update = &log->updates[log->count++];
  update->buffer = gimp_drawable_get_buffer (drawable);
  update->local_region = *GEGL_RECTANGLE (x, y, width, height);
  update->image_region = update->local_region;
  update->image_region.x += gimp_item_get_offset_x (GIMP_ITEM (drawable));
  update->image_region.y += gimp_item_get_offset_y (GIMP_ITEM (drawable));
}

static void
assert_drawable_update (const DrawableUpdate *update,
                        GeglBuffer           *buffer,
                        const GeglRectangle  *extent,
                        gint                  offset_y)
{
  GeglRectangle image_region = *extent;

  image_region.x += 7;
  image_region.y += offset_y;
  g_assert_true (update->buffer == buffer);
  g_assert_true (gegl_rectangle_equal (&update->local_region, extent));
  g_assert_true (gegl_rectangle_equal (&update->image_region, &image_region));
}

static void
drawable_buffer_update_regions (gconstpointer data)
{
  const DrawableBufferCase *test = data;
  const Babl               *format = babl_format ("R'G'B'A u8");
  GimpImage                *image;
  GimpLayer                *layer;
  GimpDrawable             *drawable;
  GeglBuffer               *old_buffer;
  GeglBuffer               *new_buffer;
  GeglRectangle             old_bounds = { 7, -3, 0, 0 };
  GeglRectangle             new_bounds = { 7, test->new_offset_y, 0, 0 };
  GeglRectangle             bounding_box;
  DrawableUpdateLog         log = { 0 };
  gulong                    handler;
  gboolean                  moved = test->new_offset_y != old_bounds.y;

  /* Keep the existing metadata-only selections lightweight.  These cases
   * need real image/layer ownership and native GEGL operations, but no GUI. */
  if (! gimp)
    {
      gimp_test_utils_set_gimp3_directory ("GIMP_TESTING_ABS_TOP_SRCDIR",
                                           "app/tests/gimpdir");
      gimp = gimp_init_for_testing ();
    }

  image = gimp_image_new (gimp, 32, 32, GIMP_RGB,
                          GIMP_PRECISION_U8_NON_LINEAR);
  layer = gimp_layer_new (image, test->extent.width, test->extent.height,
                          format, "Drawable invalidation", 1.0,
                          GIMP_LAYER_MODE_NORMAL);
  g_assert_true (gimp_image_add_layer (image, layer, NULL, 0, FALSE));
  drawable = GIMP_DRAWABLE (layer);
  old_buffer = gegl_buffer_new (&test->extent, format);
  new_buffer = gegl_buffer_new (&test->extent, format);
  gimp_drawable_set_buffer_full (drawable, FALSE, NULL, old_buffer,
                                 &old_bounds, FALSE);
  bounding_box = gimp_drawable_get_bounding_box (drawable);
  g_assert_true (gegl_rectangle_equal (&bounding_box, &test->extent));

  handler = g_signal_connect (drawable, "update",
                              G_CALLBACK (record_drawable_update), &log);
  gimp_drawable_set_buffer_full (drawable, FALSE, NULL, new_buffer,
                                 test->implicit_bounds ? NULL : &new_bounds,
                                 TRUE);

  /* Equal translated extents need only the post-replacement update.  For
   * a y-only move to old x, the wrong-axis comparison instead drops the
   * required old-region update.  Record the live buffer and offset at each
   * signal to prove the old region is invalidated before replacement. */
  g_assert_cmpuint (log.count, ==, moved ? 2 : 1);
  if (moved)
    assert_drawable_update (&log.updates[0], old_buffer,
                            &test->extent, old_bounds.y);
  assert_drawable_update (&log.updates[log.count - 1], new_buffer,
                          &test->extent, test->new_offset_y);
  g_assert_true (gimp_drawable_get_buffer (drawable) == new_buffer);
  g_assert_cmpint (gimp_item_get_offset_x (GIMP_ITEM (drawable)), ==, 7);
  g_assert_cmpint (gimp_item_get_offset_y (GIMP_ITEM (drawable)), ==,
                   test->new_offset_y);
  g_assert_cmpint (gimp_item_get_width (GIMP_ITEM (drawable)), ==,
                   test->extent.width);
  g_assert_cmpint (gimp_item_get_height (GIMP_ITEM (drawable)), ==,
                   test->extent.height);

  g_signal_handler_disconnect (drawable, handler);
  g_object_unref (new_buffer);
  g_object_unref (old_buffer);
  g_object_unref (image);
}

int
main (int argc, char **argv)
{
  static const DrawableBufferCase drawable_cases[] =
  {
    { "same-size-explicit-bounds", { 0, 0, 4, 3 }, -3, FALSE },
    { "same-size-implicit-bounds", { 0, 0, 4, 3 }, -3, TRUE  },
    { "y-only-move",               { 0, 0, 4, 3 },  7, FALSE },
    { "nonzero-same-size",         {-2, 5, 4, 3 }, -3, FALSE },
    { "nonzero-implicit-bounds",   {-2, 5, 4, 3 }, -3, TRUE  },
    { "nonzero-y-only-move",       {-2, 5, 4, 3 },  7, FALSE }
  };
  gint result;
  guint i;

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
  for (i = 0; i < G_N_ELEMENTS (drawable_cases); i++)
    {
      gchar *path = g_strconcat ("/painter/core-metadata/drawable-buffer/",
                                 drawable_cases[i].path, NULL);

      g_test_add_data_func (path, &drawable_cases[i],
                            drawable_buffer_update_regions);
      g_free (path);
    }

  result = g_test_run ();
  if (gimp)
    {
      gimp_test_utils_set_gimp3_directory ("GIMP_TESTING_ABS_TOP_BUILDDIR",
                                           "app/tests/gimpdir-output");
      gimp_exit (gimp, TRUE);
    }
  else
    gegl_exit ();

  return result;
}
