/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gtk/gtk.h>
#include <algorithm>
#include <vector>
extern "C" {
#include "core/core-types.h"
#include "widgets/widgets-types.h"
#include "core/gimp.h"
#include "core/gimpimage.h"
#include "core/gimpimage-undo.h"
#include "core/gimplayer.h"
#include "core/gimplayer-new.h"
#include "tests.h"
#include "gimp-app-test-utils.h"
}
#include "paint/painter-mypaint-surface/gimp-painter-options.hpp"
#include "paint/painter-mypaint-surface/gimp-painter-session.hpp"
#include "paint/painter-mypaint-surface/gegl-surface.hpp"
#include "painter/tests/binding-store-observation.hpp"
using namespace GimpPainter;
using StoreObservation::Counts;
static Gimp *gimp;

static std::vector<guchar> pixels (GeglBuffer *buffer)
{
  auto *extent = gegl_buffer_get_extent (buffer);
  auto *format = gegl_buffer_get_format (buffer);
  std::vector<guchar> bytes (std::size_t (extent->width) * extent->height * babl_format_get_bytes_per_pixel (format));
  gegl_buffer_get (buffer, extent, 1, format, bytes.data (), GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);
  return bytes;
}
struct StrokeResult { Counts counts; std::size_t changed; };
static StrokeResult session_stroke (int side, bool wide, int samples)
{
  auto *options = GIMP_PAINTER_MYBRUSH_OPTIONS (g_object_new (GIMP_TYPE_PAINTER_MYBRUSH_OPTIONS, "gimp", gimp, nullptr));
  g_object_set (options, "dabs-per-second", 40., "radius-logarithmic", side == 64 ? 2. : 4.25, nullptr);
  GError *error = nullptr;
  auto *session = gimp_painter_session_new (options, &error);
  g_assert_no_error (error);
  auto *image = gimp_image_new (gimp, side, side, GIMP_RGB,
                              wide ? GIMP_PRECISION_DOUBLE_LINEAR : GIMP_PRECISION_U8_NON_LINEAR);
  auto *layer = gimp_layer_new (image, side, side, babl_format (wide ? "RGBA double" : "R'G'B'A u8"),
                               "Qdata operation fixture", 1, GIMP_LAYER_MODE_NORMAL);
  g_assert_true (gimp_image_add_layer (image, layer, nullptr, 0, FALSE));
  auto *drawable = GIMP_DRAWABLE (layer);
  auto *buffer = gimp_drawable_get_buffer (drawable);
  gegl_buffer_clear (buffer, nullptr);
  gimp_image_undo_free (image);
  const auto before = pixels (buffer);
  GimpCoords coords = GIMP_COORDS_DEFAULT_VALUES;
  coords.x = side / 3.; coords.y = side / 3.; coords.pressure = 0;
  g_assert_true (gimp_painter_session_stroke_to (session, drawable, .01, &coords, nullptr, &error));
  g_assert_no_error (error);
  const auto measured_before = pixels (buffer);
  const auto start = StoreObservation::snapshot ();
  for (int sample = 0; sample < samples; ++sample) {
    coords.x += 1; coords.y += 1; coords.pressure = .85;
    g_assert_true (gimp_painter_session_stroke_to (session, drawable, .05, &coords, nullptr, &error));
    g_assert_no_error (error);
  }
  const auto counts = StoreObservation::since (start);
  g_assert_cmpuint (counts.stores, ==, 2 * samples);
  g_assert_cmpuint (counts.slots, ==, 3 * samples);
  g_assert_true (gimp_painter_session_finish (session, &error));
  g_assert_no_error (error);
  const auto after = pixels (buffer);
  const int stride = babl_format_get_bytes_per_pixel (gegl_buffer_get_format (buffer));
  std::size_t changed = 0;
  for (std::size_t at = 0; at < after.size (); at += stride)
    changed += !std::equal (after.begin () + at, after.begin () + at + stride, measured_before.begin () + at);
  g_assert_cmpuint (changed, >, 0);
  g_assert_true (gimp_image_undo (image));
  g_assert_true (pixels (buffer) == before);
  g_test_message ("session side=%d wide=%d samples=%d changed_pixels=%zu stores=%zu slots=%zu",
                  side, wide, samples, changed, counts.stores, counts.slots);
  g_object_unref (session); g_object_unref (options); g_object_unref (image);
  return {counts, changed};
}
static void session_operations ()
{
  for (bool wide : {false, true}) {
    Counts one {};
    for (int samples : {1, 8}) {
      const auto small = session_stroke (64, wide, samples);
      const auto large = session_stroke (256, wide, samples);
      g_assert_cmpuint (large.changed, >, small.changed);
      g_assert_cmpuint (small.counts.stores, ==, large.counts.stores);
      g_assert_cmpuint (small.counts.slots, ==, large.counts.slots);
      if (samples == 1) one = small.counts;
      else {
        g_assert_cmpuint (small.counts.stores, >, one.stores);
        g_assert_cmpuint (small.counts.slots, >, one.slots);
        // Exact per-sample totals are recorded; admission/finish stages can
        // legitimately differ from a pure one-lookup synthetic operation.
        g_assert_cmpuint (small.counts.stores, <=, 8 * one.stores);
        g_assert_cmpuint (small.counts.slots, <=, 8 * one.slots);
      }
    }
  }
}
static void surface_operations ()
{
  for (const char *name : {"R'G'B'A u8", "RGBA double"})
    for (int side : {16, 256}) {
      auto *format = babl_format (name);
      auto *buffer = gegl_buffer_new (GEGL_RECTANGLE (0, 0, side, side), format);
      gegl_buffer_clear (buffer, nullptr);
      const auto before = pixels (buffer);
      MyPaint::GeglSurface surface (buffer);
      surface.set_shape_provider ([side] (float, float, float, float) {
        return MyPaint::ShapeMask {side, side, std::vector<guchar> (std::size_t (side) * side, 255)};
      });
      surface.begin_session ();
      const auto start = StoreObservation::snapshot ();
      for (int dab = 0; dab < 8; ++dab) {
        g_assert_true (surface.draw_dab (side / 2.f, side / 2.f, side / 2.f, .7, .2, .3, .5));
        float r, g, b, a;
        surface.get_color (side / 2.f, side / 2.f, side / 2.f, &r, &g, &b, &a);
        g_assert_cmpfloat (a, >, 0);
      }
      const auto counts = StoreObservation::since (start);
      g_assert_cmpuint (counts.stores, ==, 0);
      g_assert_cmpuint (counts.slots, ==, 0);
      const auto write_bytes = std::size_t (side) * side * babl_format_get_bytes_per_pixel (format) * 8;
      g_assert_cmpuint (surface.bytes_written (), ==, write_bytes);
      g_assert_cmpuint (surface.bytes_read (), >=, write_bytes);
      g_assert_true (pixels (buffer) != before);
      g_test_message ("surface format=%s pixels=%d dabs=8 stores=0 slots=0 bytes_read=%zu bytes_written=%zu",
                      name, side * side, surface.bytes_read (), surface.bytes_written ());
      surface.cancel_session ();
      g_assert_true (pixels (buffer) == before);
      g_object_unref (buffer);
    }
}
int main (int argc, char **argv)
{
  g_test_init (&argc, &argv, nullptr);
  gimp_test_utils_set_gimp3_directory ("GIMP_TESTING_ABS_TOP_SRCDIR", "app/tests/gimpdir");
  gimp = gimp_init_for_testing ();
  g_test_add_func ("/qdata/native-session", session_operations);
  g_test_add_func ("/qdata/native-surface", surface_operations);
  const int result = g_test_run ();
  gimp_test_utils_set_gimp3_directory ("GIMP_TESTING_ABS_TOP_BUILDDIR", "app/tests/gimpdir-output");
  gimp_exit (gimp, TRUE);
  return result;
}
