/* SPDX-License-Identifier: GPL-3.0-or-later
 * Dedicated Smudge default size and untouched neutral dynamics only.
 * This does not cover arbitrary transformed accumulator repositioning. */
#include <sstream>
#include <string>
extern "C" {
#include "libgimpconfig/gimpconfig.h"
#include "core/gimpbrushgenerated.h"
#include "core/gimpcontainer.h"
#include "core/gimpcurve.h"
#include "core/gimpcurve-map.h"
#include "core/gimptooloptions.h"
#include "paint/gimppainterbrushgeometry.h"
#include "paint/gimpsmudgeoptions.h"
}

static std::vector<double> smudge_old_default_samples ()
{
  gchar *path = g_build_filename (g_getenv ("GIMP_TESTING_ABS_TOP_SRCDIR"),
    "migration/fixtures/legacy-brush-geometry/default-dynamics.tsv.gz", nullptr);
  gchar *compressed = nullptr; gsize length = 0; GError *error = nullptr;
  g_assert_true (g_file_get_contents (path, &compressed, &length, &error));
  g_assert_no_error (error); g_free (path);
  auto *memory = g_memory_input_stream_new_from_data (compressed, length, g_free);
  auto *decoder = g_zlib_decompressor_new (G_ZLIB_COMPRESSOR_FORMAT_GZIP);
  auto *stream = g_converter_input_stream_new (memory, G_CONVERTER (decoder));
  std::string data; char buffer[8192]; gssize n;
  while ((n = g_input_stream_read (stream, buffer, sizeof buffer, nullptr, &error)) > 0)
    data.append (buffer, n);
  g_assert_no_error (error); g_assert_cmpint (n, ==, 0);
  g_object_unref (stream); g_object_unref (decoder); g_object_unref (memory);
  std::istringstream input (data); std::string tag, value; int identity, points, count, index;
  input >> tag >> identity >> points >> count;
  g_assert_cmpstr (tag.c_str (), ==, "CURVE_DEFAULT");
  g_assert_cmpint (identity, ==, 0); g_assert_cmpint (points, ==, 17);
  g_assert_cmpint (count, ==, 256);
  std::vector<double> samples (count);
  for (int i = 0; i < count; ++i) {
    input >> tag >> index >> value;
    g_assert_false (input.fail ()); g_assert_cmpstr (tag.c_str (), ==, "CURVE_SAMPLE");
    g_assert_cmpint (index, ==, i); samples[i] = g_ascii_strtod (value.c_str (), nullptr);
  }
  return samples;
}
static double smudge_old_default_value (const std::vector<double>& samples, double pressure)
{
  if (pressure <= 0) return 0;
  if (pressure >= 1) return 1;
  const double position = pressure * (samples.size () - 1);
  const int i = int (position); const double fraction = position - i;
  return (1 - fraction) * samples[i] + fraction * samples[i + 1];
}
static void smudge_default_dynamics ()
{
  const auto samples = smudge_old_default_samples ();
  auto *legacy = GIMP_PAINT_OPTIONS (g_object_new (GIMP_TYPE_PAINTER_SMUDGE_OPTIONS, "gimp", gimp, nullptr));
  auto *modern = GIMP_PAINT_OPTIONS (g_object_new (GIMP_TYPE_SMUDGE_OPTIONS, "gimp", gimp, nullptr));
  auto *dynamics = GIMP_DYNAMICS (g_object_new (GIMP_TYPE_DYNAMICS, "name", "Smudge neutral mapping", nullptr));
  g_assert_false (gimp_painter_brush_geometry_options (legacy));
  unsigned differences = 0, checks = 0;
  const GimpDynamicsOutputType types[] = {GIMP_DYNAMICS_OUTPUT_SIZE, GIMP_DYNAMICS_OUTPUT_OPACITY,
    GIMP_DYNAMICS_OUTPUT_HARDNESS, GIMP_DYNAMICS_OUTPUT_FORCE, GIMP_DYNAMICS_OUTPUT_RATE,
    GIMP_DYNAMICS_OUTPUT_BLENDING};
  GimpCoords coords = GIMP_COORDS_DEFAULT_VALUES;
  for (auto type : types) {
    auto *output = gimp_dynamics_get_output (dynamics, type);
    g_object_set (output, "use-pressure", TRUE, nullptr);
    GimpCurve *curve = nullptr; g_object_get (output, "pressure-curve", &curve, nullptr);
    g_assert_cmpint (curve->n_points, ==, 0);
    for (int i = 0; i < 1030; ++i) {
      coords.pressure = i < 1024 ? i / 1023.0 : i == 1024 ? .37 : i == 1025 ? .48 :
        i == 1026 ? .1 : i == 1027 ? .4 : i == 1028 ? -.1 : 1.1;
      const double standard = gimp_curve_map_value (curve, coords.pressure);
      const double expected = smudge_old_default_value (samples, coords.pressure);
      g_assert_cmpfloat (gimp_dynamics_get_linear_value (dynamics, type, &coords, modern, 1), ==, standard);
      g_assert_cmpfloat (gimp_dynamics_get_linear_value (dynamics, type, &coords, legacy, 1), ==, expected);
      g_assert_cmpfloat (gimp_dynamics_get_linear_value (dynamics, type, &coords, modern, 1), ==, standard);
      differences += expected != standard; ++checks;
    }
    /* Neither explicit smooth points nor hand-edited free samples are defaults. */
    gimp_curve_add_point (curve, .5, .27); coords.pressure = .37;
    g_assert_cmpfloat (gimp_dynamics_get_linear_value (dynamics, type, &coords, legacy, 1), ==,
                      gimp_curve_map_value (curve, coords.pressure));
    gimp_curve_set_curve_type (curve, GIMP_CURVE_FREE);
    gimp_curve_set_curve (curve, .37, .73);
    g_assert_cmpfloat (gimp_dynamics_get_linear_value (dynamics, type, &coords, legacy, 1), ==,
                      gimp_curve_map_value (curve, coords.pressure));
    g_object_unref (curve);
  }
  g_assert_cmpuint (checks, ==, 6180); g_assert_cmpuint (differences, >, 0);
  g_assert_false (gimp_painter_brush_geometry_options (legacy));
  g_test_message ("6180 old-sample comparisons; %u differ from modern neutral mapping", differences);
  g_object_unref (dynamics); g_object_unref (modern); g_object_unref (legacy);

  /* Exercise the actual dedicated core, not only a model getter. */
  Scene scene;
  auto *size = gimp_dynamics_get_output (gimp_context_get_dynamics (GIMP_CONTEXT (scene.options)), GIMP_DYNAMICS_OUTPUT_SIZE);
  g_object_set (size, "use-pressure", TRUE, nullptr);
  scene.coords.pressure = .37; scene.begin (); scene.motion ();
  g_assert_cmpfloat (GIMP_BRUSH_CORE (scene.core)->scale, ==, smudge_old_default_value (samples, .37));
  g_assert_false (gimp_painter_brush_geometry_enabled (GIMP_BRUSH_CORE (scene.core)));
  scene.finish (false); g_assert (scene.pixels () == scene.initial);
}

/* Source-only oracle from afa43fae3e920210146abed514f136fd49f671b5:
 * app/core/gimpbrushgenerated.c:gimp_brush_generated_get_half_size,
 * with the final 2 * half + 1 from transform_size. Deliberately does not call
 * the production legacy geometry helper or use current mask dimensions. */
static double smudge_old_default_size (int shape, float radius, int spikes, float ratio, double angle)
{
  angle = ROUND (angle * 1000.0) / 1000.0;
  const double s = std::sin (gimp_deg_to_rad (angle)), c = std::cos (gimp_deg_to_rad (angle));
  const double short_radius = radius / ratio;
  const double xx = c * radius, xy = -s * radius, yx = s * short_radius, yy = c * short_radius;
  double width, height;
  if (shape == GIMP_BRUSH_GENERATED_CIRCLE) {
    width = std::ceil (std::sqrt (xx * xx + yx * yx)); height = std::ceil (std::sqrt (xy * xy + yy * yy));
  } else if (shape == GIMP_BRUSH_GENERATED_SQUARE) {
    width = std::ceil (std::abs (xx) + std::abs (yx)); height = std::ceil (std::abs (xy) + std::abs (yy));
  } else {
    width = std::ceil (std::max (std::abs (xx), std::abs (yx))); height = std::ceil (std::max (std::abs (xy), std::abs (yy)));
  }
  if (spikes > 2) width = height = std::ceil (std::sqrt (radius * radius + short_radius * short_radius));
  return 2 * std::max (width, height) + 1;
}
static void smudge_native_default_size ()
{
  auto *info = gimp_container_get_child_by_name (gimp->paint_info_list, "gimp-painter-smudge");
  g_assert_nonnull (info);
  auto *legacy = GIMP_PAINT_OPTIONS (g_object_new (GIMP_TYPE_PAINTER_SMUDGE_OPTIONS,
    "gimp", gimp, "paint-info", info, nullptr));
  auto *modern = GIMP_PAINT_OPTIONS (g_object_new (GIMP_TYPE_SMUDGE_OPTIONS, "gimp", gimp, nullptr));
  unsigned cases = 0, differences = 0;
  for (int shape = 0; shape < 3; ++shape) for (float radius : {8.75f, 13.0f, 19.0f})
    for (int spikes : {2, 3}) for (double angle : {0.0, 27.0, 90.0, 123.0}) {
      auto *brush = GIMP_BRUSH (gimp_brush_generated_new ("default size oracle",
        static_cast<GimpBrushGeneratedShape> (shape), radius, spikes, .42, 1.7, angle));
      const double expected = smudge_old_default_size (shape, radius, spikes, 1.7f, angle);
      gint width, height; gimp_brush_transform_size (brush, 1, 0, 0, FALSE, &width, &height);
      gimp_context_set_brush (GIMP_CONTEXT (legacy), brush);
      gimp_paint_options_set_default_brush_size (legacy, nullptr);
      g_assert_cmpfloat (legacy->brush_size, ==, expected);
      gimp_paint_options_set_default_brush_size (modern, brush);
      g_assert_cmpfloat (modern->brush_size, ==, MAX (width, height));
      differences += legacy->brush_size != modern->brush_size; ++cases;
      g_assert_false (gimp_painter_brush_geometry_options (legacy));
      g_object_unref (brush);
    }
  g_assert_cmpuint (cases, ==, 72); g_assert_cmpuint (differences, >, 0);
  /* Linked selection and radius updates use the same native default-size path. */
  g_object_set (legacy, "brush-link-angle", FALSE, "brush-link-aspect-ratio", FALSE,
                "brush-link-hardness", FALSE, "brush-link-spacing", FALSE, nullptr);
  gimp_tool_options_set_gui_mode (GIMP_TOOL_OPTIONS (legacy), TRUE);
  auto *brush = GIMP_BRUSH (gimp_brush_generated_new ("linked Smudge default", GIMP_BRUSH_GENERATED_CIRCLE, 8.75, 3, .42, 1.7, 27));
  gimp_context_set_brush (GIMP_CONTEXT (legacy), brush);
  g_assert_cmpfloat (legacy->brush_size, ==, smudge_old_default_size (0, 8.75, 3, 1.7f, 27));
  gimp_brush_generated_set_radius (GIMP_BRUSH_GENERATED (brush), 13);
  g_assert_cmpfloat (legacy->brush_size, ==, smudge_old_default_size (0, 13, 3, 1.7f, 27));
  g_object_set (legacy, "brush-link-size", FALSE, "brush-size", 61.0, nullptr);
  gimp_brush_generated_set_radius (GIMP_BRUSH_GENERATED (brush), 19);
  g_assert_cmpfloat (legacy->brush_size, ==, 61);
  g_object_unref (brush);
  g_object_unref (modern); g_object_unref (legacy);
  g_test_message ("72 generated default sizes; %u differ from modern defaults", differences);

  for (bool generated : {false, true}) {
    Scene scene; /* Bitmap and generated defaults leave the real core at unit scale. */
    if (generated) {
      auto *selected = GIMP_BRUSH (gimp_brush_generated_new ("native Smudge generated",
        GIMP_BRUSH_GENERATED_CIRCLE, 8.75, 3, .42, 1.7, 27));
      gimp_context_set_brush (GIMP_CONTEXT (scene.options), selected); g_object_unref (selected);
    }
    gimp_paint_options_set_default_brush_size (scene.options, nullptr);
    g_assert_cmpfloat (scene.options->brush_size, ==,
      generated ? smudge_old_default_size (0, 8.75, 3, 1.7f, 27) : 17);
    scene.begin (); scene.motion ();
    g_assert_cmpfloat (GIMP_BRUSH_CORE (scene.core)->scale, ==, 1);
    g_assert_false (gimp_painter_brush_geometry_enabled (GIMP_BRUSH_CORE (scene.core)));
    scene.finish (false); g_assert (scene.pixels () == scene.initial);
  }
}
