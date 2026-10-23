/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <math.h>
#include <string.h>
#include <gegl-plugin.h>
#include "../operations-types.h"
#include "gimpoperationpainterfloat.h"
#include "gimppainterfloatkernel.h"
static void prepare (GeglOperation *operation);
static GeglRectangle bounding_box (GeglOperation *operation);
static gboolean parent_process (GeglOperation *operation,
                                GeglOperationContext *context,
                                const gchar *output_property,
                                const GeglRectangle *result, gint level);
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
  operation->get_bounding_box = bounding_box;
  layer->parent_process = parent_process;
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
/* Opacity/mask do not remove source coverage for these explicit semantics:
 * swapped DST modes return source at zero weight, and bottom initialization
 * retains source RGB even when alpha is zero.  Generic layer-mode bounding-box
 * and empty-input shortcuts assume otherwise.  Keep this exception local to
 * the opt-in float operation; historical byte and ordinary modes are untouched. */
static GeglRectangle bounding_box (GeglOperation *operation)
{
  const GeglRectangle *input = gegl_operation_source_get_bounding_box (operation, "input");
  const GeglRectangle *source = gegl_operation_source_get_bounding_box (operation, "aux");
  GeglRectangle result = { 0, 0, 0, 0 };
  if (input) result = *input;
  if (source) gegl_rectangle_bounding_box (&result, &result, source);
  return result;
}
static gboolean parent_process (GeglOperation *operation,
                                GeglOperationContext *context,
                                const gchar *output_property,
                                const GeglRectangle *result, gint level)
{
  GObject *input = gegl_operation_context_get_object (context, "input");
  GObject *aux = gegl_operation_context_get_object (context, "aux");

  /* The inherited outer wrapper already applies effective opacity and the
   * out-of-ROI mask rule.  Go to the actual point composer without the generic
   * layer-mode's invalid empty-input/zero-opacity early return.  Supply empty
   * buffers exactly as the generic path does before its point-composer call. */
  if (!input || !aux)
    {
      GObject *empty = G_OBJECT (gegl_buffer_new (NULL, NULL));
      if (!input) gegl_operation_context_set_object (context, "input", empty);
      if (!aux) gegl_operation_context_set_object (context, "aux", empty);
      if (!input && !aux) gegl_object_set_has_forked (empty);
      g_object_unref (empty);
    }
  return GEGL_OPERATION_CLASS (g_type_class_peek_parent (
      gimp_operation_painter_float_parent_class))->process (
          operation, context, output_property, result, level);
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
