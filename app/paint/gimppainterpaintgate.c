/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <cairo.h>
#include <gegl.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include "paint-types.h"
#include "core/gimperror.h"
#include "gimppainterpaintgate.h"
#include "painter-mypaint-surface/gimp-painter-options.h"
#include "gimp-intl.h"

/* The interactive ColorTool owns a GimpPainterSession. Until generic path/PDB
 * stroking shares that transaction explicitly, refuse instead of silently
 * selecting the ordinary paintbrush PaintInfo for extended settings. No second
 * implementation or bridge owner lives in this adapter. */
G_DEFINE_TYPE (GimpPainterPaintGate, gimp_painter_paint_gate, GIMP_TYPE_PAINT_CORE)
static gboolean
start (GimpPaintCore *core, GList *drawables, GimpPaintOptions *options,
       const GimpCoords *coords, GError **error)
{
  g_set_error_literal (error, GIMP_ERROR, GIMP_FAILED,
                       _("Extended Painter MyPaint currently requires its interactive tool; generic Stroke Path/PDB transaction sharing is not implemented."));
  return FALSE;
}
static void gimp_painter_paint_gate_class_init (GimpPainterPaintGateClass *klass)
{ GIMP_PAINT_CORE_CLASS (klass)->start = start; }
static void gimp_painter_paint_gate_init (GimpPainterPaintGate *self) {}
void gimp_painter_paint_gate_register (Gimp *gimp, GimpPaintRegisterCallback callback)
{
  callback (gimp, GIMP_TYPE_PAINTER_PAINT_GATE, GIMP_TYPE_PAINTER_MYBRUSH_OPTIONS,
            "gimp-painter-mypaint", _("Painter MyPaint"), "gimp-tool-mypaint-brush");
}
