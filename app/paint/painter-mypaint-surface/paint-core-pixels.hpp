/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_MYPAINT_PAINT_CORE_PIXELS_HPP
#define GIMP_PAINTER_MYPAINT_PAINT_CORE_PIXELS_HPP
#include "../../painter/gimp-painter-visibility.h"
#include <gegl.h>
#include <memory>
typedef struct _GimpPaintCore GimpPaintCore;
typedef struct _GimpDrawable GimpDrawable;
typedef struct _GimpImage GimpImage;
typedef struct _GimpCoords GimpCoords;
namespace GimpPainter GIMP_PAINTER_PRIVATE { namespace MyPaint {
/* Prepare-only payload. No history publication, baseline stealing, pixel writes,
 * preview thaw or evaluator changes. Caller holds its lifecycle transition guard.
 * Borrowed accessors remain valid while this object lives; a consuming native
 * Undo constructor must take its own references. Geometry expansion is rejected. */
class PreparedPixels
{
public:
  ~PreparedPixels ();
  PreparedPixels (const PreparedPixels&) = delete;
  PreparedPixels& operator= (const PreparedPixels&) = delete;
  GeglBuffer *buffer () const noexcept;
  GimpDrawable *drawable () const noexcept;
  GimpImage *image () const noexcept;
  const GeglRectangle& rectangle () const noexcept;
  const GimpCoords& start_coords () const noexcept;
  // Identity/geometry/transaction staleness check, not a lock against arbitrary
  // external pixel writes. Call again immediately before publication.
  bool matches (GimpPaintCore *core, GimpDrawable *drawable) const noexcept;
private:
  struct Impl;
  explicit PreparedPixels (std::unique_ptr<Impl> impl);
  std::unique_ptr<Impl> impl_;
  friend std::unique_ptr<PreparedPixels> prepare_pixels (GimpPaintCore*, GimpDrawable*);
};
// Null means no dirty pixels. Invalid/stale transaction or failed preparation
// throws; the native rollback snapshot and existing history remain untouched.
std::unique_ptr<PreparedPixels> prepare_pixels (GimpPaintCore *core, GimpDrawable *drawable);
} }
#endif
