/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_BOUNDED_FILL_HPP
#define GIMP_PAINTER_BOUNDED_FILL_HPP
#include "../../painter/gimp-painter-visibility.h"
#include <gegl.h>
#include <array>
#include <cstddef>
#include <memory>
namespace GimpPainter GIMP_PAINTER_PRIVATE { namespace Fill {
/* Immutable projection state shared by all dabs of a stroke. Input must have
 * the native nonlinear byte format; no implicit precision/profile conversion.
 * Coordinates are in the buffer's image coordinate system, including offsets. */
class Snapshot {
public:
  Snapshot (GeglBuffer *source, int channels, bool alpha, int origin_x, int origin_y);
  Snapshot (GeglBuffer *source, int channels, bool alpha, const std::array<guchar,4>& fixed_color);
  ~Snapshot ();
  Snapshot (const Snapshot&) = delete;
  Snapshot& operator= (const Snapshot&) = delete;
private:
  struct Impl; std::unique_ptr<Impl> impl_;
  friend class Search;
};
class Search {
public:
  enum class State { Searching, Growing, Complete, Cancelled };
  struct Options { int threshold = 30; bool antialias = true; bool select_transparent = true; bool grow = true; };
  Search (std::shared_ptr<const Snapshot> snapshot, GeglBuffer *search_mask,
          const GeglRectangle& bounds, int seed_x, int seed_y, Options options);
  ~Search ();
  Search (const Search&) = delete;
  Search& operator= (const Search&) = delete;
  /* A bounded number of candidate/grow pixels. No GObject callback is fired;
   * scheduling and generation validation belong to the native owner. */
  State step (std::size_t pixel_budget);
  State state () const noexcept;
  /* Cancellation is O(1); raster/frontier reclamation occurs on destruction. */
  void cancel () noexcept;
  /* Borrowed, immutable result only when Complete; no partial mask publication. */
  GeglBuffer *result () const noexcept;
  std::size_t visited () const noexcept;
private:
  struct Impl; std::unique_ptr<Impl> impl_;
};
} }
#endif
