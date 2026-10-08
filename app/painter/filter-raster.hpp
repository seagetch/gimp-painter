/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_FILTER_RASTER_HPP
#define GIMP_PAINTER_FILTER_RASTER_HPP
#include "gimp-painter-visibility.h"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <memory>
#include <string>
#include <thread>

namespace GimpPainter GIMP_PAINTER_PRIVATE {
/* Independent worker-owned byte storage. No GObject, GEGL or owner callback.
 * Operations may block on local storage and must never be called by the UI
 * adapter. Offsets are 64-bit even where individual bounded chunks are not. */
class FilterRaster
{
public:
  virtual ~FilterRaster () = default;
  virtual std::uint64_t size () const noexcept = 0;
  virtual void read (std::uint64_t offset, std::size_t count, std::uint8_t *bytes) = 0;
  virtual void write (std::uint64_t offset, std::size_t count, const std::uint8_t *bytes) = 0;
  virtual void flush () = 0;
};

/* Worker-only advisory capacity query. Other processes may race it; actual
 * writes/flush failures remain authoritative. Never call on the UI thread. */
std::uint64_t filter_available_space (const std::string& directory);

using FilterRasterFactory = std::function<std::unique_ptr<FilterRaster> (std::uint64_t)>;

/* An exclusive mode-0600 temporary file, unlinked immediately on POSIX and
 * delete-on-close on Windows. Creation, access and destruction belong to the
 * independent worker; the configured directory is copied before launch. No
 * preallocation or raster-sized memory allocation occurs in the constructor. */
class TemporaryFilterRaster final : public FilterRaster
{
public:
  TemporaryFilterRaster (const std::string& directory, std::uint64_t size);
  ~TemporaryFilterRaster () noexcept override;
  TemporaryFilterRaster (const TemporaryFilterRaster&) = delete;
  TemporaryFilterRaster& operator= (const TemporaryFilterRaster&) = delete;
  std::uint64_t size () const noexcept override { return size_; }
  void read (std::uint64_t, std::size_t, std::uint8_t *) override;
  void write (std::uint64_t, std::size_t, const std::uint8_t *) override;
  void flush () override;
private:
  void position (std::uint64_t, std::size_t, const void *);
  std::FILE *file_ = nullptr;
  std::uint64_t size_;
  std::thread::id thread_;
};

/* Exact byte transpose, not a pixel/color conversion. Production tiles are
 * bounded to1024x1024 pixels with1..32 bytes per pixel (at most32MiB
 * plus one32KiB line). The live RGBA8 route uses4MiB; the double route
 * selects256-square tiles (2MiB). A smaller tile
 * side supports boundary regression tests. Input and output must not alias.
 * Cancellation/failure can leave this private output partial, never published. */
bool transpose_filter_rgba (FilterRaster& input, FilterRaster& output,
                            std::size_t width, std::size_t height,
                            std::atomic<bool>& cancel, std::size_t tile_side = 1024,
                            std::size_t bytes_per_pixel = 4);
} // namespace GimpPainter
#endif
