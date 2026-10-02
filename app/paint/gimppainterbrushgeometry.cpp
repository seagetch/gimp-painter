/* SPDX-License-Identifier: GPL-3.0-or-later
 * Stateless adapter to the already verified pinned Painter mask kernels.
 * All native buffer ownership stays with the calling GObject. */
#include "config.h"
#include <gegl.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
extern "C" {
#include "gimppainterbrushgeometry.h"
#include "core/gimpbrush.h"
#include "core/gimpbrushgenerated.h"
#include "core/gimptempbuf.h"
}
#include "painter/boundary.hpp"
#include "painter-mypaint-surface/legacy-generated-mask.hpp"
#include "painter-mypaint-surface/legacy-mask-transform.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <stdexcept>

namespace {
namespace MyPaint = GimpPainter::MyPaint;
using Temp = std::unique_ptr<GimpTempBuf, decltype (&gimp_temp_buf_unref)>;
void validate (GimpBrush *brush) {
  if (!GIMP_IS_BRUSH (brush)) throw std::invalid_argument ("Legacy geometry requires a brush");
}
void mirror (MyPaint::ShapeMask& mask) {
  for (int y = 0; y < mask.height; ++y)
    std::reverse (mask.pixels.begin () + std::size_t (y) * mask.width,
                  mask.pixels.begin () + std::size_t (y + 1) * mask.width);
}
}

extern "C" gboolean
gimp_painter_brush_geometry_size (GimpBrush *brush, gint *width, gint *height,
                                   GError **error)
{
  return GimpPainter::boundary<gboolean> (error, FALSE, [&] {
    validate (brush);
    if (!width || !height) throw std::invalid_argument ("Missing legacy size output");
    if (GIMP_IS_BRUSH_GENERATED (brush)) {
      auto *generated = GIMP_BRUSH_GENERATED (brush);
      const auto dimensions = MyPaint::legacy_generated_dimensions (
        static_cast<MyPaint::GeneratedShape> (generated->shape), generated->radius,
        generated->spikes, generated->hardness, generated->aspect_ratio, generated->angle);
      *width = dimensions.first; *height = dimensions.second;
    } else {
      const auto *mask = gimp_brush_get_mask (brush);
      if (!mask) throw std::invalid_argument ("Legacy geometry brush has no mask");
      *width = gimp_temp_buf_get_width (mask); *height = gimp_temp_buf_get_height (mask);
    }
    if (*width <= 0 || *height <= 0) throw std::invalid_argument ("Empty legacy brush geometry");
    return TRUE;
  });
}

extern "C" GimpTempBuf *
gimp_painter_brush_geometry_transform (GimpBrush *brush, gdouble scale,
                                        gdouble aspect, gdouble angle,
                                        gboolean reflect, gdouble hardness,
                                        gboolean pixmap, GError **error)
{
  return GimpPainter::boundary<GimpTempBuf *> (error, nullptr, [&] {
    validate (brush);
    if (!std::isfinite (scale) || scale <= 0 || !std::isfinite (aspect) ||
        !std::isfinite (angle) || !std::isfinite (hardness) || hardness < 0 || hardness > 1)
      throw std::invalid_argument ("Invalid legacy brush transform");
    MyPaint::ShapeMask mask;
    if (GIMP_IS_BRUSH_GENERATED (brush) && !pixmap) {
      auto *generated = GIMP_BRUSH_GENERATED (brush);
      const double ratio = aspect == 0 ? generated->aspect_ratio : std::min (std::abs (aspect) + 1, 20.0);
      const double turns = angle + (aspect < 0 ? .25 : 0);
      mask = MyPaint::generate_legacy_mask (static_cast<MyPaint::GeneratedShape> (generated->shape),
        generated->radius * scale, generated->spikes, generated->hardness * hardness,
        ratio, generated->angle + 360 * turns);
      if (reflect) mirror (mask); // GIMP3 extension: reflect the complete legacy stamp.
      Temp result (gimp_temp_buf_new (mask.width, mask.height, babl_format ("Y u8")), gimp_temp_buf_unref);
      std::memcpy (gimp_temp_buf_get_data (result.get ()), mask.pixels.data (), mask.pixels.size ());
      return result.release ();
    }
    const auto *source = pixmap ? gimp_brush_get_pixmap (brush) : gimp_brush_get_mask (brush);
    if (!source) throw std::invalid_argument ("Legacy brush has no requested pixel data");
    const auto *format = babl_format (pixmap ? "R'G'B' u8" : "Y u8");
    const int channels = pixmap ? 3 : 1;
    mask.width = gimp_temp_buf_get_width (source); mask.height = gimp_temp_buf_get_height (source);
    const std::size_t count = std::size_t (mask.width) * mask.height;
    if (mask.width <= 0 || mask.height <= 0 || count > G_MAXINT / channels)
      throw std::invalid_argument ("Legacy brush data is too large");
    std::vector<guchar> bytes (count * channels);
    babl_process (babl_fish (gimp_temp_buf_get_format (source), format),
                  gimp_temp_buf_get_data (source), bytes.data (), count);
    mask.pixels.resize (count);
    Temp result (nullptr, gimp_temp_buf_unref);
    for (int channel = 0; channel < channels; ++channel) {
      for (std::size_t i = 0; i < count; ++i) mask.pixels[i] = bytes[i * channels + channel];
      auto transformed = MyPaint::transform_bitmap_mask (mask, scale, aspect, angle, hardness);
      if (reflect) mirror (transformed);
      if (!result) result.reset (gimp_temp_buf_new (transformed.width, transformed.height, format));
      auto *output = gimp_temp_buf_get_data (result.get ());
      for (std::size_t i = 0; i < transformed.pixels.size (); ++i) output[i * channels + channel] = transformed.pixels[i];
    }
    return result.release ();
  });
}


extern "C" gboolean
gimp_painter_brush_geometry_color (const GimpTempBuf *pixmap,
                                    const GimpTempBuf *mask, GeglBuffer *area,
                                    const GimpCoords *coords,
                                    gint area_x, gint area_y, GError **error)
{
  return GimpPainter::boundary<gboolean> (error, FALSE, [&] {
    if (!pixmap || !GEGL_IS_BUFFER (area) || !coords ||
        !std::isfinite (coords->x) || !std::isfinite (coords->y) ||
        gimp_temp_buf_get_format (pixmap) != babl_format ("R'G'B' u8"))
      throw std::invalid_argument ("Invalid legacy pixmap paint area");
    const int pw = gimp_temp_buf_get_width (pixmap), ph = gimp_temp_buf_get_height (pixmap);
    const int width = gegl_buffer_get_width (area), height = gegl_buffer_get_height (area);
    if (pw <= 0 || ph <= 0 || width <= 0 || height <= 0 ||
        (mask && (gimp_temp_buf_get_width (mask) != pw || gimp_temp_buf_get_height (mask) != ph ||
                  gimp_temp_buf_get_format (mask) != babl_format ("Y u8"))))
      throw std::invalid_argument ("Mismatched legacy pixmap and mask dimensions");
    double left = std::floor (coords->x) - pw / 2;
    double top = std::floor (coords->y) - ph / 2;
    if (!(pw & 1)) left += std::floor (coords->x + .5) - std::floor (coords->x);
    if (!(ph & 1)) top += std::floor (coords->y + .5) - std::floor (coords->y);
    if (left < G_MININT || left > G_MAXINT || top < G_MININT || top > G_MAXINT)
      throw std::invalid_argument ("Legacy pixmap origin exceeds integer coordinates");
    const auto wrap = [] (gint64 value, int size) { value %= size; return static_cast<int> (value < 0 ? value + size : value); };
    const int offset_x = wrap (gint64 (area_x) - static_cast<int> (left), pw);
    const int offset_y = wrap (gint64 (area_y) - static_cast<int> (top), ph);
    const auto *colors = gimp_temp_buf_get_data (pixmap);
    const auto *coverage = mask ? gimp_temp_buf_get_data (mask) : nullptr;
    std::vector<guchar> pixels (std::size_t (width) * height * 4);
    for (int y = 0; y < height; ++y)
      for (int x = 0; x < width; ++x) {
        const std::size_t source = std::size_t ((y + offset_y) % ph) * pw + (x + offset_x) % pw;
        auto *dest = pixels.data () + (std::size_t (y) * width + x) * 4;
        std::copy_n (colors + source * 3, 3, dest);
        dest[3] = coverage ? coverage[source] : 255;
      }
    gegl_buffer_set (area, GEGL_RECTANGLE (0, 0, width, height), 0,
                     babl_format ("R'G'B'A u8"), pixels.data (), GEGL_AUTO_ROWSTRIDE);
    return TRUE;
  });
}
