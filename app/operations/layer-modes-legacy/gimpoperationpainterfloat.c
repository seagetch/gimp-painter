/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <math.h>
#include <string.h>
#include <gegl-plugin.h>
#include "../operations-types.h"
#include "gimpoperationpainterfloat.h"
#include "gimppainterfloatkernel.h"
static void prepare (GeglOperation *operation);
static gboolean process (GeglOperation *operation, void *input, void *aux,
                         void *mask, void *output, glong samples,
                         const GeglRectangle *roi, gint level);
G_DEFINE_TYPE (GimpOperationPainterFloat, gimp_operation_painter_float,
               GIMP_TYPE_OPERATION_LAYER_MODE)
gboolean gimp_painter_layer_mode_is_float (GimpLayerMode mode)
{
  return mode >= GIMP_LAYER_MODE_PAINTER_FLOAT_ERASE &&
         mode <= GIMP_LAYER_MODE_PAINTER_FLOAT_MULTIPLY;
}
void gimp_painter_float_prepare_direct (GeglOperation *operation)
{
  g_return_if_fail (G_TYPE_CHECK_INSTANCE_TYPE (operation, GIMP_TYPE_OPERATION_PAINTER_FLOAT));
  ((GimpOperationPainterFloat *) operation)->direct_paint = TRUE;
}
static GimpLayerCompositeRegion affected (GimpOperationLayerMode *operation)
{ return GIMP_LAYER_COMPOSITE_REGION_UNION; }
static void gimp_operation_painter_float_class_init (GimpOperationPainterFloatClass *klass)
{
  GeglOperationClass *operation = GEGL_OPERATION_CLASS (klass);
  GimpOperationLayerModeClass *layer = GIMP_OPERATION_LAYER_MODE_CLASS (klass);
  gegl_operation_class_set_keys (operation, "name", "gimp:painter-float-mode",
      "description", "Explicit Painter continuous nonlinear float modes", NULL);
  operation->prepare = prepare;
  layer->process = process;
  layer->get_affected_region = affected;
}
static void gimp_operation_painter_float_init (GimpOperationPainterFloat *self) {}
static void prepare (GeglOperation *operation)
{
  GimpOperationPainterFloat *self = (gpointer) operation;
  const GeglRectangle *extent;
  self->parent_instance.composite_space = GIMP_LAYER_COLOR_SPACE_RGB_NON_LINEAR;
  self->parent_instance.blend_space = GIMP_LAYER_COLOR_SPACE_AUTO;
  self->parent_instance.prop_composite_mode = GIMP_LAYER_COMPOSITE_UNION;
  GEGL_OPERATION_CLASS (gimp_operation_painter_float_parent_class)->prepare (operation);
  extent = gegl_operation_source_get_bounding_box (operation, "aux");
  self->source_extent = extent ? *extent : *GEGL_RECTANGLE (0, 0, 0, 0);
}
static float coverage (float x)
{ return !isfinite (x) || x <= 0 ? 0 : x >= 1 ? 1 : x; }
static gboolean process (GeglOperation *operation, void *input, void *auxiliary,
                         void *mask_pointer, void *output, glong samples,
                         const GeglRectangle *roi, gint level)
{
  GimpOperationPainterFloat *self = (gpointer) operation;
  GimpOperationLayerMode *layer = (gpointer) operation;
  const float *input_pixels = input, *aux = auxiliary, *mask = mask_pointer;
  float *out = output;
  GimpPainterFloatKernelMode mode;
  if (!gimp_painter_layer_mode_is_float (layer->layer_mode)) return FALSE;
  mode = layer->layer_mode - GIMP_LAYER_MODE_PAINTER_FLOAT_ERASE;
  for (glong n = 0; n < samples; ++n)
    {
      gint x = roi->x + n % roi->width, y = roi->y + n / roi->width;
      float m = mask ? mask[n] : 1;
      if (!self->direct_paint &&
          (x < self->source_extent.x || y < self->source_extent.y ||
           (gint64)x >= (gint64)self->source_extent.x + self->source_extent.width ||
           (gint64)y >= (gint64)self->source_extent.y + self->source_extent.height))
        { memcpy (out + 4*n, input_pixels + 4*n, 4*sizeof(float)); continue; }
      if (!self->direct_paint && layer->is_last_node)
        {
          for (guint k = 0; k < 3; ++k)
            out[4*n+k] = isfinite (aux[4*n+k]) ? aux[4*n+k] : 0;
          out[4*n+3] = (double)coverage (aux[4*n+3]) * coverage (m) * coverage (layer->opacity);
        }
      else if (!gimp_painter_float_composite (input_pixels + 4*n, aux + 4*n,
                 out + 4*n, layer->opacity, m, mode)) return FALSE;
    }
  return TRUE;
}
