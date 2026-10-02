/* SPDX-License-Identifier: GPL-3.0-or-later
 * The old paper path's byte mask/canvas ordering around native PaintCore Undo.
 * Derived from pinned gimp-painter paint-funcs and gimppaintcore.c. */
#include "config.h"
#include <gegl.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
extern "C" {
#include "gimppainterpaper-paste.h"
#include "gimpairbrush.h"
#include "gimpbrushcore.h"
#include "gimppainterbrushgeometry.h"
#include "core/gimpdrawable.h"
#include "core/gimpimage.h"
#include "core/gimptempbuf.h"
#include "operations/layer-modes-legacy/gimpoperationpainterlegacy.h"
}
#include "painter/boundary.hpp"
#include "painter-smudge/legacy-pixels.hpp"
#include <algorithm>
#include <cmath>
#include <vector>

extern "C" gboolean
gimp_painter_paper_paste (GimpPaintCore *core, const GimpTempBuf *mask,
                          gint mask_x, gint mask_y, GimpDrawable *drawable,
                          gdouble paint_opacity, gdouble image_opacity,
                          GimpLayerMode paint_mode, GimpPaintApplicationMode mode,
                          GError **error)
{
  if (!GIMP_IS_BRUSH_CORE (core) ||
      (!GIMP_BRUSH_CORE (core)->texture && !gimp_painter_brush_geometry_enabled (GIMP_BRUSH_CORE (core))) ||
      !gimp_painter_layer_mode_is_compatibility (paint_mode) ||
      gimp_image_get_precision (gimp_item_get_image (GIMP_ITEM (drawable))) !=
        GIMP_PRECISION_U8_NON_LINEAR)
    return FALSE;
  return GimpPainter::boundary<gboolean> (error, TRUE, [&] {
    using GimpPainter::Smudge::multiply;
    using GimpPainter::Smudge::multiply3;
    if (!mask || !core->paint_buffer || !core->canvas_buffer ||
        !std::isfinite (paint_opacity) || !std::isfinite (image_opacity) ||
        paint_opacity < 0 || paint_opacity > 1 || image_opacity < 0 || image_opacity > 1)
      throw std::invalid_argument ("Invalid paper paint transaction");
    const auto affect = gimp_drawable_get_active_mask (drawable);
    if (!affect) return TRUE;
    guint32 raw = 0;
    if (!gimp_painter_layer_mode_to_legacy (paint_mode, &raw))
      throw std::invalid_argument ("Unsupported paper paint mode");
    const bool gray = gimp_drawable_is_gray (drawable);
    const bool alpha = gimp_drawable_has_alpha (drawable);
    const unsigned colors = gray ? 1 : 3, channels = colors + alpha;
    /* Pinned generic composite dispatch falls back when the destination has
     * no alpha. Src-Out also returns B unchanged and selects Replace. */
    if (!alpha && (raw == 23 || raw == 25)) raw = 0;
    if (!alpha && raw == 28) raw = 24;
    /* Opaque DST-IN/OUT in the old reversed-source path wrote2/4channels into
     * a1/3channel destination. Use the safe opaque-as-RGBA extension below;
     * that channel-mismatched old path is not a byte-equivalence oracle. */

    const auto *format = gimp_drawable_get_format (drawable);
    if (babl_format_get_bytes_per_pixel (format) != static_cast<int> (channels))
      throw std::invalid_argument ("Paper compatibility requires original byte channels");
    const int width = gegl_buffer_get_width (core->paint_buffer);
    const int height = gegl_buffer_get_height (core->paint_buffer);
    const int mask_width = gimp_temp_buf_get_width (mask);
    const int mask_height = gimp_temp_buf_get_height (mask);
    if (width < 1 || height < 1 || mask_x < 0 || mask_y < 0 ||
        mask_x > mask_width - width || mask_y > mask_height - height)
      throw std::invalid_argument ("Paper paint exceeds its mask bounds");
    const auto count = static_cast<std::size_t> (width) * height;
    std::vector<guchar> coverage (count), canvas (count), paint (count * 4),
                        source (count * channels), result (count * channels),
                        selection (core->mask_buffer ? count : 0);
    /* The intermediate mask has already received paper. Quantize once at the
     * old byte boundary; never quantize a high-precision destination here. */
    auto *mask_bytes = static_cast<guchar *> (gimp_temp_buf_lock (mask, babl_format ("Y u8"), GEGL_ACCESS_READ));
    if (!mask_bytes) throw std::runtime_error ("Paper mask conversion failed");
    for (int row = 0; row < height; ++row)
      std::copy_n (mask_bytes + static_cast<std::size_t> (mask_y + row) * mask_width + mask_x,
                   width, coverage.data () + static_cast<std::size_t> (row) * width);
    gimp_temp_buf_unlock (mask, mask_bytes);
    const GeglRectangle area {core->paint_buffer_x, core->paint_buffer_y, width, height};
    const GeglRectangle paint_area {0, 0, width, height};
    auto *destination = gimp_drawable_get_buffer (drawable);
    auto *backdrop = mode == GIMP_PAINT_CONSTANT ?
      gimp_paint_core_get_orig_image (core, drawable) : destination;
    if (!backdrop) throw std::runtime_error ("Paper paint has no original image");
    gegl_buffer_get (backdrop, &area, 1, format, source.data (), GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);
    gegl_buffer_get (core->paint_buffer, &paint_area, 1, babl_format ("R'G'B'A u8"),
                     paint.data (), GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);
    if (mode == GIMP_PAINT_CONSTANT)
      gegl_buffer_get (core->canvas_buffer, &area, 1, babl_format ("Y u8"),
                       canvas.data (), GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);
    if (core->mask_buffer) {
      int offset_x, offset_y;gimp_item_get_offset (GIMP_ITEM (drawable), &offset_x, &offset_y);
      const GeglRectangle selected {area.x + offset_x, area.y + offset_y, width, height};
      gegl_buffer_get (core->mask_buffer, &selected, 1, babl_format ("Y u8"),
                       selection.data (), GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);
    }
    const unsigned dab_opacity = paint_opacity * 255.999;
    const unsigned opacity = image_opacity * 255.999;
    const bool stipple = GIMP_IS_AIRBRUSH (core);
    for (std::size_t i = 0; i < count; ++i) {
      unsigned char a[4] = {}, b[4], d[4];
      std::copy_n (paint.data () + 4 * i, 4, b);
      if (gray) b[0] = b[1] = b[2] = static_cast<guchar> (b[0] * .2126 + b[1] * .7152 + b[2] * .0722 + .5);
      if (mode == GIMP_PAINT_CONSTANT) {
        if (dab_opacity == 255)
          canvas[i] += multiply (255 - canvas[i], coverage[i]);
        else if (stipple || dab_opacity > canvas[i])
          canvas[i] += multiply ((stipple ? 255 : dab_opacity) - canvas[i],
                                 multiply (coverage[i], dab_opacity));
        b[3] = multiply (b[3], canvas[i]);
      } else {
        b[3] = dab_opacity == 255 ? multiply (b[3], coverage[i]) :
                                   multiply3 (b[3], coverage[i], dab_opacity);
      }
      if (gray) a[0] = a[1] = a[2] = source[i * channels];
      else std::copy_n (source.data () + i * channels, 3, a);
      a[3] = alpha ? source[i * channels + colors] : 255;
      const unsigned selected = selection.empty () ? 256 : selection[i];
      gimp_painter_legacy_composite_u8 (a, b, d, opacity, selected, raw);
      /* The old no-alpha normal compositor uses INT_BLEND, not the RGBA
       * floating alpha ratio (even when the latter's backdrop alpha is255). */
      if (!alpha && (raw == 0 || raw == 3)) {
        if (raw == 3) for (unsigned c = 0; c < 3; ++c) b[c] = multiply (a[c], b[c]);
        const unsigned effective = selection.empty () ? multiply (b[3], opacity) :
                                                       multiply3 (b[3], selection[i], opacity);
        for (unsigned c = 0; c < 3; ++c) d[c] = multiply (static_cast<int> (b[c]) - a[c], effective) + a[c];
      }
      for (unsigned c = 0; c < colors; ++c)
        result[i * channels + c] = (affect & (GIMP_COMPONENT_MASK_RED << c)) ? d[c] : source[i * channels + c];
      if (alpha) result[i * channels + colors] = (affect & GIMP_COMPONENT_MASK_ALPHA) ? d[3] : a[3];
    }
    if (mode == GIMP_PAINT_CONSTANT)
      gegl_buffer_set (core->canvas_buffer, &area, 0, babl_format ("Y u8"), canvas.data (), GEGL_AUTO_ROWSTRIDE);
    gegl_buffer_set (destination, &area, 0, format, result.data (), GEGL_AUTO_ROWSTRIDE);
    core->x1 = std::min (core->x1, area.x);core->y1 = std::min (core->y1, area.y);
    core->x2 = std::max (core->x2, area.x + width);core->y2 = std::max (core->y2, area.y + height);
    gimp_drawable_update (drawable, area.x, area.y, width, height);
    return TRUE;
  });
}
