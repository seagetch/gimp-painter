/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_BUCKET_FILL_BRUSH_TOOL_HPP
#define GIMP_BUCKET_FILL_BRUSH_TOOL_HPP
#include <gegl.h>
#include <gtk/gtk.h>
#include "tools-types.h"
#include "gimpbucketfillbrushtool.h"
#include "painter/object-ref.hpp"
namespace GimpPainter GIMP_PAINTER_PRIVATE {
template<> struct TypeTraits<GimpFillBrushTool> {
  static GType type () noexcept { return gimp_bucket_fill_brush_tool_get_type (); }
};
class BucketFillBrushToolRef {
public:
  BucketFillBrushToolRef () noexcept = default;
  static BucketFillBrushToolRef retain (GimpBucketFillBrushTool *tool)
  { return BucketFillBrushToolRef (ObjectRef<GimpBucketFillBrushTool>::retain (tool)); }
  static BucketFillBrushToolRef adopt (GimpBucketFillBrushTool *tool)
  { return BucketFillBrushToolRef (ObjectRef<GimpBucketFillBrushTool>::adopt (tool)); }
  static void register_type (GimpToolRegisterCallback callback, gpointer data)
  { gimp_bucket_fill_brush_tool_register (callback, data); }
  GimpBucketFillBrushTool *get () const noexcept { return owner_.get (); }
  explicit operator bool () const noexcept { return bool (owner_); }
  void reset () noexcept { owner_.reset (); }
private:
  explicit BucketFillBrushToolRef (ObjectRef<GimpBucketFillBrushTool> owner) noexcept : owner_ (std::move (owner)) {}
  ObjectRef<GimpBucketFillBrushTool> owner_;
};
}
#endif
