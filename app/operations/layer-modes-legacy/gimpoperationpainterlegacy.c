/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * Painter compatibility arithmetic derived from paint-funcs.c and
 * gimp-composite-generic.c at afa43fae3e920210146abed514f136fd49f671b5.
 * Copyright (C) 2003 Helvetix Victorinox and the GIMP contributors.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "config.h"
#include <math.h>
#include <string.h>
#include <gegl-plugin.h>
#include "../operations-types.h"
#include "gimpoperationpainterlegacy.h"

/* Preserve the historical integer roundoff, including INT_MULT3's known
 * approximation and INT_DIV's strictly-greater half tie. */
static gint mult (gint a, gint b)
{ gint t = a * b + 128; return ((t >> 8) + t) >> 8; }
static gint mult3 (gint a, gint b, gint c)
{ gint t = a * b * c + 0x7f5b; return ((t >> 7) + t) >> 16; }
static guint divide (guint a, guint b)
{ return a / b + (a % b > b / 2); }

void
gimp_painter_legacy_composite_u8 (const guint8 backdrop[4], const guint8 source[4],
                                  guint8 out[4], guint opacity, guint mask,
                                  guint raw_mode)
{
  const guint8 *a = backdrop, *b = source;
  guint8 temp[4];
  guint alpha, k;
  g_return_if_fail (raw_mode >= 23 && raw_mode <= 29);
  g_return_if_fail (opacity <= 255 && mask <= 255);
  if (raw_mode == 27 || raw_mode == 29) { a = source; b = backdrop; }
  memcpy (out, a, 4);
  if (raw_mode == 23 || raw_mode == 25)
    {
      alpha = mult3 (b[3], mask, opacity);
      out[3] = raw_mode == 23 ? a[3] - mult (a[3], alpha) :
                               a[3] + mult (255 - a[3], alpha);
      return;
    }
  if (raw_mode >= 26)
    {
      memcpy (temp, b, 4);
      temp[3] = mult (raw_mode >= 28 ? 255 - a[3] : a[3], b[3]);
      b = temp;
    }
  alpha = mult ((gint) b[3] - a[3], mult (mask, opacity)) + a[3];
  out[3] = alpha;
  if (alpha)
    {
      guint ratio = divide ((mask * opacity / 255) * b[3], alpha);
      for (k = 0; k < 3; ++k)
        {
          guint delta = divide (ABS ((gint) b[k] - a[k]) * ratio, 255);
          out[k] = b[k] > a[k] ? a[k] + delta : a[k] - delta;
        }
    }
}

gboolean
gimp_painter_layer_mode_from_legacy (guint32 raw, GimpLayerMode *mode)
{
  G_STATIC_ASSERT (GIMP_LAYER_MODE_COLOR_ERASE_LEGACY == 22);
  g_return_val_if_fail (mode != NULL, FALSE);
  if (raw <= 22) { *mode = (GimpLayerMode) raw; return TRUE; }
  if (raw <= 29)
    { *mode = GIMP_LAYER_MODE_PAINTER_ERASE + (raw - 23); return TRUE; }
  return FALSE;
}
gboolean
gimp_painter_layer_mode_to_legacy (GimpLayerMode mode, guint32 *raw)
{
  g_return_val_if_fail (raw != NULL, FALSE);
  if (mode >= 0 && mode <= GIMP_LAYER_MODE_COLOR_ERASE_LEGACY)
    { *raw = mode; return TRUE; }
  if (mode >= GIMP_LAYER_MODE_PAINTER_ERASE && mode <= GIMP_LAYER_MODE_PAINTER_DST_OUT)
    { *raw = 23 + mode - GIMP_LAYER_MODE_PAINTER_ERASE; return TRUE; }
  return FALSE;
}

static guint byte (gfloat value)
{
  if (! isfinite (value) || value <= 0.0f) return 0;
  if (value >= 1.0f) return 255;
  return (guint) floorf (value * 255.0f + 0.5f);
}
static void prepare (GeglOperation *op);
static gboolean process (GeglOperation *op, void *in_p, void *aux_p, void *mask_p,
                          void *out_p, glong samples, const GeglRectangle *roi,
                          gint level);
static GimpLayerCompositeRegion affected (GimpOperationLayerMode *op)
{ return GIMP_LAYER_COMPOSITE_REGION_UNION; }
G_DEFINE_TYPE (GimpOperationPainterLegacy, gimp_operation_painter_legacy,
               GIMP_TYPE_OPERATION_LAYER_MODE)

static void
gimp_operation_painter_legacy_class_init (GimpOperationPainterLegacyClass *klass)
{
  GeglOperationClass *operation = GEGL_OPERATION_CLASS (klass);
  GimpOperationLayerModeClass *layer = GIMP_OPERATION_LAYER_MODE_CLASS (klass);
  gegl_operation_class_set_keys (operation, "name", "gimp:painter-legacy-mode",
                                 "description", "GIMP Painter byte-compatible layer modes", NULL);
  operation->prepare = prepare;
  layer->process = process;
  layer->get_affected_region = affected;
}
static void gimp_operation_painter_legacy_init (GimpOperationPainterLegacy *self) {}
static void prepare (GeglOperation *op)
{
  GimpOperationPainterLegacy *self = (gpointer) op;
  const GeglRectangle *r;
  /* Immutable legacy arithmetic is defined over nonlinear byte channels. */
  self->parent_instance.composite_space = GIMP_LAYER_COLOR_SPACE_RGB_NON_LINEAR;
  self->parent_instance.blend_space = GIMP_LAYER_COLOR_SPACE_AUTO;
  self->parent_instance.prop_composite_mode = GIMP_LAYER_COMPOSITE_UNION;
  GEGL_OPERATION_CLASS (gimp_operation_painter_legacy_parent_class)->prepare (op);
  r = gegl_operation_source_get_bounding_box (op, "aux");
  self->source_extent = r ? *r : *GEGL_RECTANGLE (0, 0, 0, 0);
}
static gboolean
process (GeglOperation *op, void *in_p, void *aux_p, void *mask_p, void *out_p,
         glong samples, const GeglRectangle *roi, gint level)
{
  GimpOperationPainterLegacy *self = (gpointer) op;
  GimpOperationLayerMode *layer_mode = (gpointer) op;
  const gfloat *in = in_p, *aux = aux_p, *mask = mask_p;
  gfloat *out = out_p;
  guint raw = 0, opacity = (guint) CLAMP (layer_mode->prop_opacity * 255.0, 0.0, 255.0);
  glong n;
  if (!gimp_painter_layer_mode_to_legacy (layer_mode->layer_mode, &raw) || raw < 23)
    return FALSE;
  for (n = 0; n < samples; ++n)
    {
      guint8 a[4], b[4], d[4];
      guint k, m = mask ? byte (mask[n]) : 255;
      gint x = roi->x + n % roi->width, y = roi->y + n / roi->width;
      /* Old combine_regions only visits the layer rectangle. Transparent
       * pixels inside that rectangle are still meaningful for replace/IN. */
      if (x < self->source_extent.x || y < self->source_extent.y ||
          (gint64) x >= (gint64) self->source_extent.x + self->source_extent.width ||
          (gint64) y >= (gint64) self->source_extent.y + self->source_extent.height)
        { memcpy (out + 4*n, in + 4*n, 4 * sizeof (gfloat)); continue; }
      for (k = 0; k < 4; ++k) { a[k] = byte (in[4*n+k]); b[k] = byte (aux[4*n+k]); }
      if (layer_mode->is_last_node)
        {
          memcpy (d, b, 4);
          d[3] = mask ? mult3 (opacity, b[3], m) : mult (opacity, b[3]);
        }
      else
        gimp_painter_legacy_composite_u8 (a, b, d, opacity, m, raw);
      for (k = 0; k < 4; ++k) out[4*n+k] = d[k] / 255.0f;
    }
  return TRUE;
}
