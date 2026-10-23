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
#include "gimpmypaintcoreundo.h"
#include "painter/object-ref.hpp"
namespace GimpPainter GIMP_PAINTER_PRIVATE {
template<> struct TypeTraits<GimpMypaintCoreUndo>
{ static GType type () noexcept { return gimp_mypaint_core_undo_get_type (); } };
/* Typed ownership of the sole native entry; no legacy raw Stroke API. */
class MypaintCoreUndoRef {
public:
 MypaintCoreUndoRef () noexcept = default;
 static MypaintCoreUndoRef adopt (GimpMypaintCoreUndo *p)
 { return MypaintCoreUndoRef (ObjectRef<GimpMypaintCoreUndo>::adopt (p)); }
 static MypaintCoreUndoRef retain (GimpMypaintCoreUndo *p)
 { return MypaintCoreUndoRef (ObjectRef<GimpMypaintCoreUndo>::retain (p)); }
 GimpMypaintCoreUndo *get () const noexcept { return owner_.get (); }
 explicit operator bool () const noexcept { return bool (owner_); }
 GimpMypaintCoreUndo *release () noexcept { return owner_.release (); }
 void reset () noexcept { owner_.reset (); }
private:
 explicit MypaintCoreUndoRef (ObjectRef<GimpMypaintCoreUndo> p) noexcept
 : owner_ (std::move (p)) {}
 ObjectRef<GimpMypaintCoreUndo> owner_;
};
}
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
