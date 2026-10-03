/* SPDX-License-Identifier: GPL-3.0-or-later
 * Prepare complete cached Filter XCFs for actual console tests. No job runs
 * here: the separate console must invalidate the source after readiness.
 */
#include "config.h"
#include <gegl.h>
#include <gio/gio.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <fontconfig/fontconfig.h>
extern "C" {
#include "libgimpbase/gimpbase.h"
#include "core/core-types.h"
#include "core/gimp.h"
#include "core/gimp-contexts.h"
#include "core/gimpdrawable.h"
#include "core/gimpimage.h"
#include "core/gimpimage-undo.h"
#include "core/gimplayer-new.h"
#include "core/gimpfilterlayer.h"
#include "gegl/gimp-gegl.h"
#include "xcf/xcf.h"
#include "tests.h"
}
#include "core/gimp-painter-type-traits.hpp"
#include "painter/gio-type-traits.hpp"
#include <array>
#include <cstdlib>
#include <fstream>
#include <stdexcept>
#include <string>

namespace GimpPainter {
template<> struct TypeTraits<GimpImage>
{ static GType type () noexcept { return GIMP_TYPE_IMAGE; } };
template<> struct TypeTraits<GeglColor>
{ static GType type () noexcept { return GEGL_TYPE_COLOR; } };
}
using namespace GimpPainter;
namespace {
void require (bool condition, const char *message)
{ if (!condition) throw std::runtime_error (message); }
void check_error (GError *error)
{
  if (error)
    {
      const std::string message = error->message;
      g_error_free (error);
      throw std::runtime_error (message);
    }
}
void fill (GimpLayer *layer, const std::array<guint8, 4>& pixel)
{
  auto color = ObjectRef<GeglColor>::adopt (gegl_color_new (nullptr));
  gegl_color_set_pixel (color.get (), babl_format ("R'G'B'A u8"), pixel.data ());
  gegl_buffer_set_color (gimp_drawable_get_buffer (GIMP_DRAWABLE (layer)), nullptr, color.get ());
}
void fixture (Gimp *gimp, const std::string& name, gint side, unsigned index)
{
  auto image = ObjectRef<GimpImage>::adopt (gimp_image_new (
    gimp, side, side, GIMP_RGB, GIMP_PRECISION_U8_NON_LINEAR));
  gimp_image_undo_disable (image.get ());
  auto source = ObjectRef<GimpLayer>::sink (gimp_layer_new (
    image.get (), side, side, babl_format ("R'G'B'A u8"),
    "quit source", 1.0, GIMP_LAYER_MODE_NORMAL_LEGACY));
  auto filter = ObjectRef<GimpLayer>::sink (gimp_filter_layer_new (
    image.get (), side, side, "quit Blinds", 1.0, GIMP_LAYER_MODE_NORMAL_LEGACY));
  require (gimp_image_add_layer (image.get (), source.get (), nullptr, 0, FALSE), "Cannot add source");
  require (gimp_image_add_layer (image.get (), filter.get (), nullptr, 0, FALSE), "Cannot add Blinds");
  fill (source.get (), {{guint8 (117 + index), 71, 199, 255}});
  fill (filter.get (), {{13, guint8 (29 + index), 47, 255}});
  GimpValueArray *arguments = gimp_value_array_new_from_types (nullptr,
    G_TYPE_INT, 1, G_TYPE_INT, 0, G_TYPE_INT, 0,
    G_TYPE_INT, 67, G_TYPE_INT, 7, G_TYPE_INT, 1, G_TYPE_INT, 1, G_TYPE_NONE);
  GError *error = nullptr;
  const bool defined = gimp_filter_layer_set_definition (
    GIMP_FILTER_LAYER (filter.get ()), "plug-in-blinds", nullptr, arguments, &error);
  gimp_value_array_unref (arguments);
  check_error (error);
  require (defined, "Cannot define Blinds");
  const GimpFilterLayerSnapshot complete {1, 41, 41, TRUE, GIMP_FILTER_LAYER_CLEAN};
  require (gimp_filter_layer_restore_snapshot_state (GIMP_FILTER_LAYER (filter.get ()),
                                                    &complete, &error), "Cannot install complete cached snapshot");
  check_error (error);
  require (gimp_filter_layer_get_state (GIMP_FILTER_LAYER (filter.get ())) == GIMP_FILTER_LAYER_CLEAN,
           "Fixture started a filter job before saving");
  require (gimp_filter_layer_get_run_count (GIMP_FILTER_LAYER (filter.get ())) == 0,
           "Fixture preparation must not execute Blinds");
  auto file = ObjectRef<GFile>::adopt (g_file_new_for_path (name.c_str ()));
  auto output = ObjectRef<GFileOutputStream>::adopt (
    g_file_replace (file.get (), nullptr, FALSE, G_FILE_CREATE_NONE, nullptr, &error));
  check_error (error);
  require (xcf_save_stream (gimp, image.get (), G_OUTPUT_STREAM (output.get ()),
                           file.get (), nullptr, &error), "Cannot save Quit fixture");
  check_error (error);
  if (!g_output_stream_is_closed (G_OUTPUT_STREAM (output.get ())))
    require (g_output_stream_close (G_OUTPUT_STREAM (output.get ()), nullptr, &error), "Cannot close fixture");
  check_error (error);
  g_print ("QUIT_FIXTURE_READY index=%u width=%d height=%d runs=0 state=CLEAN file=%s\n",
           index, side, side, name.c_str ());
}
void small_tiles_fixture (Gimp *gimp, const std::string& directory,
                          const std::string& evidence, gint factor)
{
  constexpr gint width = 53, height = 41;
  const std::string scene = factor == 0 ? "tiles-g0-s0-v0" : "tiles-g0-s0-v2";
  const std::string input = evidence + "/live/" + scene + "-source.raw";
  std::array<guint8, width * height * 4> pixels;
  std::ifstream captured (input, std::ios::binary);
  captured.read (reinterpret_cast<char *> (pixels.data ()), pixels.size ());
  require (captured && captured.peek () == std::ifstream::traits_type::eof (),
           "Cannot read exact captured SmallTiles source extent");
  GError *error = nullptr;
  auto image = ObjectRef<GimpImage>::adopt (gimp_image_new (
    gimp, width, height, GIMP_RGB, GIMP_PRECISION_U8_NON_LINEAR));
  gimp_image_undo_disable (image.get ());
  auto source = ObjectRef<GimpLayer>::sink (gimp_layer_new (
    image.get (), width, height, babl_format ("R'G'B'A u8"),
    "small tiles source", 1.0, GIMP_LAYER_MODE_NORMAL_LEGACY));
  auto filter = ObjectRef<GimpLayer>::sink (gimp_filter_layer_new (
    image.get (), width, height, "small tiles Filter", 1.0, GIMP_LAYER_MODE_NORMAL_LEGACY));
  require (gimp_image_add_layer (image.get (), source.get (), nullptr, 0, FALSE), "Cannot add SmallTiles source");
  require (gimp_image_add_layer (image.get (), filter.get (), nullptr, 0, FALSE), "Cannot add SmallTiles Filter");
  const GeglRectangle rectangle {0, 0, width, height};
  gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (source.get ())),
                   &rectangle, 0, babl_format ("R'G'B'A u8"), pixels.data (), GEGL_AUTO_ROWSTRIDE);
  fill (filter.get (), {{13, 29, 47, 255}});
  GimpValueArray *arguments = gimp_value_array_new_from_types (nullptr,
    G_TYPE_INT, 1, G_TYPE_INT, 0, G_TYPE_INT, 0, G_TYPE_INT, factor, G_TYPE_NONE);
  const bool defined = gimp_filter_layer_set_definition (
    GIMP_FILTER_LAYER (filter.get ()), "plug-in-small-tiles", nullptr, arguments, &error);
  gimp_value_array_unref (arguments);
  check_error (error);
  require (defined, "Cannot define SmallTiles");
  const GimpFilterLayerSnapshot complete {1, 41, 41, TRUE, GIMP_FILTER_LAYER_CLEAN};
  require (gimp_filter_layer_restore_snapshot_state (GIMP_FILTER_LAYER (filter.get ()),
                                                    &complete, &error), "Cannot install SmallTiles cached snapshot");
  check_error (error);
  require (gimp_filter_layer_get_state (GIMP_FILTER_LAYER (filter.get ())) == GIMP_FILTER_LAYER_CLEAN,
           "SmallTiles fixture started a filter job before saving");
  require (gimp_filter_layer_get_run_count (GIMP_FILTER_LAYER (filter.get ())) == 0,
           "Fixture preparation must not execute SmallTiles");
  const std::string name = directory + "/small-tiles-" + std::to_string (factor) + ".xcf";
  auto file = ObjectRef<GFile>::adopt (g_file_new_for_path (name.c_str ()));
  auto output = ObjectRef<GFileOutputStream>::adopt (
    g_file_replace (file.get (), nullptr, FALSE, G_FILE_CREATE_NONE, nullptr, &error));
  check_error (error);
  require (xcf_save_stream (gimp, image.get (), G_OUTPUT_STREAM (output.get ()),
                           file.get (), nullptr, &error), "Cannot save SmallTiles fixture");
  check_error (error);
  if (!g_output_stream_is_closed (G_OUTPUT_STREAM (output.get ())))
    require (g_output_stream_close (G_OUTPUT_STREAM (output.get ()), nullptr, &error), "Cannot close SmallTiles fixture");
  check_error (error);
  g_print ("SMALL_TILES_FIXTURE_READY factor=%d width=%d height=%d runs=0 state=CLEAN scene=%s file=%s\n",
           factor, width, height, scene.c_str (), name.c_str ());
}
}
int main (int argc, char **argv)
{
  const bool small_tiles = argc == 4 && std::string (argv[1]) == "--small-tiles";
  if (argc != 3 && !small_tiles)
    { g_printerr ("usage: filter-quit-fixture OUTPUT_DIRECTORY DIMENSION\n"
                  "       filter-quit-fixture --small-tiles EVIDENCE_ROOT OUTPUT_DIRECTORY\n"); return 2; }
  gchar *end = nullptr;
  const auto side = small_tiles ? 0 : g_ascii_strtoll (argv[2], &end, 10);
  if (!small_tiles && (!end || *end || (side != 2048 && side != 4096))) return 2;
  g_set_prgname ("filter-quit-fixture");
  if (FcConfig *fonts = FcConfigCreate ())
    { FcConfigSetCurrent (fonts); FcConfigDestroy (fonts); }
  Gimp *gimp = nullptr;
  int status = 0;
  try
    {
      require (g_file_test (argv[small_tiles ? 3 : 1], G_FILE_TEST_IS_DIR), "Output directory does not exist");
      gimp = gimp_init_for_testing ();
      require (gimp != nullptr, "Cannot initialize fixture application");
      if (small_tiles)
        for (gint factor : {0, 3}) small_tiles_fixture (gimp, argv[3], argv[2], factor);
      else
        for (unsigned index = 1; index <= 2; ++index)
          fixture (gimp, std::string (argv[1]) + "/quit-blinds-" + std::to_string (index) + ".xcf",
                   gint (side), index);
    }
  catch (const std::exception& error)
    { g_printerr ("Quit fixture failed: %s\n", error.what ()); status = 1; }
  if (gimp)
    { gimp_gegl_exit (gimp); gimp_contexts_exit (gimp); g_object_unref (gimp); }
  gegl_exit ();
  return status;
}
