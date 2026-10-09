/* SPDX-License-Identifier: GPL-3.0-or-later
 * Ordinary painter paper. Arithmetic from pinned gimp-painter BrushCore and
 * paint-funcs, under GPL-3.0-or-later. Native owners remain GimpBrushCore fields. */
#include "config.h"
#include "../painter/resources.hpp"
#include <gegl.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
extern "C" {
#include "gimppainterpaper.h"
#include "core/gimppattern.h"
#include "core/gimptempbuf.h"
}
#include "painter/boundary.hpp"
#include <cmath>
#include <memory>
#include <stdexcept>

extern "C" gboolean
gimp_painter_paper_get_view (GimpPattern *pattern, GimpPainterPaperView *view,
                            GError **error)
{
  return GimpPainter::boundary<gboolean> (error, FALSE, [&] {
    if (!GIMP_IS_PATTERN (pattern) || !view)
      throw std::invalid_argument ("Painter paper requires a pattern and a view");
    const auto *mask = gimp_pattern_get_mask (pattern);
    if (!mask) throw std::invalid_argument ("Painter paper has no pixel data");
    const auto *format = gimp_temp_buf_get_format (mask);
    const int channels = babl_format_get_bytes_per_pixel (format);
    if (babl_format_get_type (format, 0) != babl_type ("u8") ||
        channels < 1 || channels > 4 ||
        babl_format_get_n_components (format) != channels)
      throw std::invalid_argument ("Painter paper requires original byte channels");
    for (int i = 1; i < channels; ++i)
      if (babl_format_get_type (format, i) != babl_type ("u8"))
        throw std::invalid_argument ("Painter paper requires original byte channels");
    GimpPainterPaperView fresh {gimp_temp_buf_get_data (mask),
                               gimp_temp_buf_get_width (mask),
                               gimp_temp_buf_get_height (mask), channels};
    if (fresh.width <= 0 || fresh.height <= 0 || !fresh.data)
      throw std::invalid_argument ("Painter paper dimensions are empty");
    *view = fresh;
    return TRUE;
  });
}

extern "C" GimpTempBuf *
gimp_painter_paper_texturize (GimpPattern *pattern, const GimpTempBuf *mask,
                             gdouble x, gdouble y, GError **error)
{
  return GimpPainter::boundary<GimpTempBuf *> (error, nullptr, [&] {
    if (!mask || !std::isfinite (x) || !std::isfinite (y))
      throw std::invalid_argument ("Painter paper requires a mask and finite coordinates");
    GimpPainterPaperView paper {};
    GError *local = nullptr;
    if (!gimp_painter_paper_get_view (pattern, &paper, &local)) {
      std::string message = GimpPainter::take_error_message (local, "Invalid painter paper");
      throw std::invalid_argument (message);
    }
    const int width = gimp_temp_buf_get_width (mask);
    const int height = gimp_temp_buf_get_height (mask);
    const auto *format = gimp_temp_buf_get_format (mask);
    if (format != babl_format ("Y u8") && format != babl_format ("Y float"))
      throw std::invalid_argument ("Painter paper requires a byte or float brush mask");
    /* The old (gint)floor cast and subsequent half-width subtraction must be
     * representable. Reject undefined casts instead of invoking them. */
    const double left = std::floor (x) - (width >> 1);
    const double top = std::floor (y) - (height >> 1);
    if (left < G_MININT || left > G_MAXINT || top < G_MININT || top > G_MAXINT)
      throw std::invalid_argument ("Painter paper coordinates exceed the legacy integer domain");
    const int offset_x = static_cast<int> (left) % paper.width;
    int offset_y = static_cast<int> (top) % paper.height;
    /* Old negative y indexed before the pattern allocation (undefined). The
     * safe extension repeats upwards; the defined nonnegative path is exact.
     * Old x was mixed with guint i and is intentionally unsigned modulo. */
    if (offset_y < 0) offset_y += paper.height;
    using Temp = std::unique_ptr<GimpTempBuf, decltype (&gimp_temp_buf_unref)>;
    Temp output (gimp_temp_buf_new (width, height, format), gimp_temp_buf_unref);
    if (!output) throw std::bad_alloc ();
    const auto *input = gimp_temp_buf_get_data (mask);
    auto *dest = gimp_temp_buf_get_data (output.get ());
    for (int row = 0; row < height; ++row) {
      const auto *paper_row = paper.data +
        ((static_cast<gsize> (row) + offset_y) % paper.height) * paper.width * paper.channels;
      for (int column = 0; column < width; ++column) {
        const auto value = paper_row[((static_cast<guint> (column) + offset_x) %
                                       static_cast<guint> (paper.width)) * paper.channels];
        const gsize index = static_cast<gsize> (row) * width + column;
        if (format == babl_format ("Y u8")) {
          const guint product = input[index] * value + 128;
          dest[index] = ((product >> 8) + product) >> 8;
        } else {
          reinterpret_cast<float *> (dest)[index] =
            reinterpret_cast<const float *> (input)[index] * (value / 255.0f);
        }
      }
    }
    return output.release ();
  });
}
