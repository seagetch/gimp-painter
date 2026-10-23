/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_MYPAINT_BUFFER_READ_HPP
#define GIMP_PAINTER_MYPAINT_BUFFER_READ_HPP

#include "../../painter/gimp-painter-visibility.h"
#include <gegl.h>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <vector>

namespace GimpPainter GIMP_PAINTER_PRIVATE { namespace MyPaint { namespace BufferRead {

/* All requests are buffer-local and half-open. Image coordinates must first be
 * translated by minus the drawable offset, exactly once. Extents are inspected,
 * never inferred from width/height or the image canvas. */
inline void validate (const GeglRectangle& rect)
{
  if (rect.width < 0 || rect.height < 0 ||
      std::int64_t (rect.x) + rect.width > G_MAXINT ||
      std::int64_t (rect.y) + rect.height > G_MAXINT)
    throw std::invalid_argument ("Invalid buffer read rectangle");
}

inline GeglRectangle translate (const GeglRectangle& rect,
                                 std::int64_t dx, std::int64_t dy)
{
  validate (rect);
  // Translation accepts a signed drawable offset or its negation.
  if (dx < -std::int64_t (G_MAXINT) - 1 || dx > -std::int64_t (G_MININT) ||
      dy < -std::int64_t (G_MAXINT) - 1 || dy > -std::int64_t (G_MININT))
    throw std::invalid_argument ("Invalid buffer coordinate translation");
  const auto x = std::int64_t (rect.x) + dx;
  const auto y = std::int64_t (rect.y) + dy;
  if (x < G_MININT || y < G_MININT ||
      x + rect.width > G_MAXINT || y + rect.height > G_MAXINT)
    throw std::invalid_argument ("Buffer read translation exceeds gint bounds");
  return {gint (x), gint (y), rect.width, rect.height};
}

/* Intersect before narrowing translated extent coordinates. A distant selection
 * at INT_MIN/INT_MAX may simply miss the request; translating its whole extent
 * into a gint rectangle first would incorrectly reject it (or overflow).
 * Only the requested rectangle must obey the admission contract: a GEGL extent
 * is not rejected just because its exclusive endpoint is beyond G_MAXINT. */
inline bool intersect (GeglRectangle& result, const GeglRectangle& request,
                       const GeglRectangle& extent, gint offset_x = 0,
                       gint offset_y = 0)
{
  validate (request);
  if (extent.width < 0 || extent.height < 0)
    throw std::invalid_argument ("Invalid buffer extent");
  const auto ex = std::int64_t (extent.x) - offset_x;
  const auto ey = std::int64_t (extent.y) - offset_y;
  const auto x = std::max (std::int64_t (request.x), ex);
  const auto y = std::max (std::int64_t (request.y), ey);
  const auto right = std::min (std::int64_t (request.x) + request.width, ex + extent.width);
  const auto bottom = std::min (std::int64_t (request.y) + request.height, ey + extent.height);
  if (right <= x || bottom <= y)
    {
      result = {0, 0, 0, 0};
      return false;
    }
  result = {gint (x), gint (y), gint (right - x), gint (bottom - y)};
  return true;
}

inline std::size_t row_bytes (const GeglRectangle& rect, const Babl *format)
{
  validate (rect);
  if (!format || !babl_format_get_bytes_per_pixel (format))
    throw std::invalid_argument ("Expected buffer read format");
  if (!rect.width || !rect.height)
    return 0;
  const auto bytes = std::uint64_t (rect.width) * babl_format_get_bytes_per_pixel (format);
  if (bytes > G_MAXINT)
    throw std::length_error ("Buffer read row exceeds GEGL stride range");
  return std::size_t (bytes);
}

/* Borrow buffer for this synchronous call. The owner must keep it alive and
 * prevent concurrent mutation. No ownership transfer or snapshot is implied.
 * Byte zero is the REQUEST origin; pixels outside its extent are zero and row
 * padding is untouched. Capacity covers the last pixel, not trailing padding.
 * All admission checks precede writes to the destination. */
inline void into (GeglBuffer *buffer, const GeglRectangle& request,
                  const Babl *format, void *destination, std::size_t capacity,
                  std::size_t rowstride)
{
  if (!buffer)
    throw std::invalid_argument ("Expected source GeglBuffer");
  const auto row = row_bytes (request, format);
  GeglRectangle clipped;
  const bool overlaps = intersect (clipped, request, *gegl_buffer_get_extent (buffer));
  if (!request.width || !request.height)
    return;
  if (rowstride < row || rowstride > G_MAXINT)
    throw std::invalid_argument ("Invalid buffer read rowstride");
  const auto rows = std::size_t (request.height - 1);
  if (rows > (std::numeric_limits<std::size_t>::max () - row) / rowstride)
    throw std::length_error ("Buffer read span overflow");
  const auto span = rows * rowstride + row;
  if (!destination || capacity < span)
    throw std::invalid_argument ("Buffer read destination is too small");

  auto *pixels = static_cast<guchar *> (destination);
  for (gint y = 0; y < request.height; ++y)
    std::memset (pixels + std::size_t (y) * rowstride, 0, row);
  if (overlaps)
    {
      const auto offset = std::size_t (std::int64_t (clipped.y) - request.y) * rowstride +
                          std::size_t (std::int64_t (clipped.x) - request.x) *
                            babl_format_get_bytes_per_pixel (format);
      gegl_buffer_get (buffer, &clipped, 1, format, pixels + offset,
                       gint (rowstride), GEGL_ABYSS_NONE);
    }
}

inline std::vector<guchar> packed (GeglBuffer *buffer, const GeglRectangle& request,
                                 const Babl *format = nullptr)
{
  if (!buffer)
    throw std::invalid_argument ("Expected source GeglBuffer");
  if (!format)
    format = gegl_buffer_get_format (buffer);
  const auto row = row_bytes (request, format);
  std::vector<guchar> pixels;
  if (row && std::size_t (request.height) > pixels.max_size () / row)
    throw std::length_error ("Buffer read allocation overflow");
  pixels.resize (row * std::size_t (request.height));
  into (buffer, request, format, pixels.data (), pixels.size (), row);
  return pixels;
}

} } } // namespace GimpPainter::MyPaint::BufferRead
#endif
