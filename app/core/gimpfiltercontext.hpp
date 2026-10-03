/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_FILTER_OWNER_CONTEXT_HPP
#define GIMP_PAINTER_FILTER_OWNER_CONTEXT_HPP

#include "painter/filter-context.hpp"
#include "painter/filter-procedure.hpp"
#include "painter/object-ref.hpp"
#include <cstddef>
#include <cstdint>
#include <vector>

typedef struct _GimpDrawable GimpDrawable;

namespace GimpPainter {

/* Owner-only state contained in the FilterLayer's existing typed slot. This is
 * not a GObject implementation/store. No instance or method crosses a worker
 * boundary. GEGL storage follows the host's shared tile cache/swap policy;
 * every pixel read/write is bounded and occurs after scheduler admission. */
class FilterOwnerContext
{
public:
  void reset () noexcept;
  void selection_changed () noexcept { selection_dirty_ = true; }
  void capture_input (GimpDrawable *, std::size_t offset, std::size_t count,
                       const std::uint8_t *rgba);
  bool before_process (GimpDrawable *, FilterProcedureRequest&, std::size_t budget);
  bool before_import (GimpDrawable *, FilterProcedureDisposition, std::size_t budget);
  void merge_chunk (std::size_t offset, std::size_t count,
                     const std::uint8_t *shadow_rgba, std::vector<std::uint8_t>& rgba);
private:
  bool bounds_quantum (GimpDrawable *, std::size_t budget);
  void seal_components (GimpDrawable *);
  ObjectRef<GObject> input_, mask_;
  FilterSelectionScan scan_;
  FilterSelectionBounds bounds_;
  FilterSelectionRegion final_region_;
  std::int32_t width_ = 0, height_ = 0;
  std::size_t input_cursor_ = 0, mask_cursor_ = 0;
  unsigned channels_ = 4, active_ = 15;
  bool selection_dirty_ = true, bounds_valid_ = false, scan_started_ = false;
  bool final_started_ = false, final_ready_ = false, no_merge_ = false;
};

} // namespace GimpPainter
#endif
