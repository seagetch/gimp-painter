/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_BUCKET_FILL_BRUSH_TOOL_H
#define GIMP_BUCKET_FILL_BRUSH_TOOL_H
#include "gimpfillbrushtool.h"
G_BEGIN_DECLS
/* Source operation aliases to the sole native GimpFillBrushTool. These are not
 * the historical instance/class layout or runtime GimpBucketFillBrushTool name. */
typedef GimpFillBrushTool GimpBucketFillBrushTool;
typedef GimpFillBrushToolClass GimpBucketFillBrushToolClass;
#define GIMP_TYPE_BUCKET_FILL_BRUSH_TOOL (gimp_bucket_fill_brush_tool_get_type ())
#define GIMP_BUCKET_FILL_BRUSH_TOOL(obj) GIMP_FILL_BRUSH_TOOL (obj)
#define GIMP_BUCKET_FILL_BRUSH_TOOL_CLASS(klass) GIMP_FILL_BRUSH_TOOL_CLASS (klass)
#define GIMP_IS_BUCKET_FILL_BRUSH_TOOL(obj) GIMP_IS_FILL_BRUSH_TOOL (obj)
#define GIMP_IS_BUCKET_FILL_BRUSH_TOOL_CLASS(klass) GIMP_IS_FILL_BRUSH_TOOL_CLASS (klass)
#define GIMP_BUCKET_FILL_BRUSH_TOOL_GET_CLASS(obj) GIMP_FILL_BRUSH_TOOL_GET_CLASS (obj)
GType gimp_bucket_fill_brush_tool_get_type (void) G_GNUC_CONST;
/* Native callback provider: opaque borrowed data is forwarded once, not
 * interpreted as Gimp. NULL callback is inert; exceptions are contained.
 * Historical direct registry/paint-core registration is not reproduced. */
void gimp_bucket_fill_brush_tool_register (GimpToolRegisterCallback callback, gpointer data);
G_END_DECLS
#endif
