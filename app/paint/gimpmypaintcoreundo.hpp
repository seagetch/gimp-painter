/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_MYPAINT_CORE_UNDO_HPP
#define GIMP_MYPAINT_CORE_UNDO_HPP
#include <gegl.h>
#include <memory>
#include <cstddef>
extern "C" {
#include "core/core-types.h"
}
#include "painter/gimp-painter-visibility.h"
namespace GimpPainter GIMP_PAINTER_PRIVATE { namespace MyPaint {
class StrokeState {
public:
 virtual ~StrokeState() noexcept = default;
 virtual bool prepare(GimpUndoMode) noexcept = 0;
 // No allocations, callbacks, or exceptions: install immutable historical state.
 virtual void apply(GimpUndoMode) noexcept = 0;
 virtual void finish(GimpUndoMode) noexcept = 0;
 virtual std::size_t memsize() const noexcept = 0;
};
// Owned, fully initialized, unpublished native entry. State readiness is checked
// by the native pop guard, not during construction of an ending active stroke.
GimpUndo *create_stroke_undo(GimpImage*,GimpDrawable*,GeglBuffer*,int x,int y,
                           const char *description,std::shared_ptr<StrokeState>,GError**) noexcept;
} }
#endif
