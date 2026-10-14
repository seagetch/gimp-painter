/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_MYPAINT_TOOL_H
#define GIMP_MYPAINT_TOOL_H

#include "gimppaintermybrushtool.h"

G_BEGIN_DECLS

/* Operation-level source port only: the legacy layout and runtime GType are
 * not restored. Legacy destination GimpColorPickMode/MODE_* arguments must
 * be explicitly ported to GimpColorPickTarget/TARGET_*. In GIMP 3 the former
 * type describes color display formats, not picking destinations. */
typedef GimpPainterMybrushTool GimpMypaintTool;
#define GIMP_MYPAINT_TOOL(obj) GIMP_PAINTER_MYBRUSH_TOOL (obj)
#define GIMP_IS_MYPAINT_TOOL(obj) GIMP_IS_PAINTER_MYBRUSH_TOOL (obj)

/* Configure the inherited picker destination during instance initialization
 * or active use. This does not activate picking or change context colors.
 * NONE means pick-only. Invalid targets/owners, closed owners and wrong-thread
 * calls leave configuration unchanged. No legacy enum/ABI compatibility. */
void gimp_mypaint_tool_enable_color_picker (GimpMypaintTool   *tool,
                                          GimpColorPickTarget target);

G_END_DECLS

#endif
