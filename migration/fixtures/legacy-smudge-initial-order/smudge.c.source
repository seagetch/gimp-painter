/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <gegl.h>

#include "libgimpmath/gimpmath.h"

#include "paint-types.h"

#include "base/pixel-region.h"
#include "base/temp-buf.h"

#include "paint-funcs/paint-funcs.h"

#include "core/gimp.h"
#include "core/gimpbrush.h"
#include "core/gimpdrawable.h"
#include "core/gimpdynamics.h"
#include "core/gimpdynamicsoutput.h"
#include "core/gimpimage.h"
#include "core/gimppickable.h"

#include "gimpsmudge.h"
#include "gimpsmudgeoptions.h"

#include "gimp-intl.h"


static void       gimp_smudge_finalize     (GObject          *object);

static void       gimp_smudge_paint        (GimpPaintCore    *paint_core,
                                            GimpDrawable     *drawable,
                                            GimpPaintOptions *paint_options,
                                            const GimpCoords *coords,
                                            GimpPaintState    paint_state,
                                            guint32           time);
static gboolean   gimp_smudge_start        (GimpPaintCore    *paint_core,
                                            GimpDrawable     *drawable,
                                            GimpPaintOptions *paint_options,
                                            const GimpCoords *coords);
static void       gimp_smudge_motion       (GimpPaintCore    *paint_core,
                                            GimpDrawable     *drawable,
                                            GimpPaintOptions *paint_options,
                                            const GimpCoords *coords);

static void       gimp_smudge_brush_coords (GimpPaintCore    *paint_core,
                                            GimpPaintOptions  *options,
                                            const GimpCoords *coords,
                                            gint             *x,
                                            gint             *y,
                                            gint             *w,
                                            gint             *h);
#if 0
static void       gimp_smudge_accumulator_coords (GimpPaintCore    *paint_core,
                                                  const GimpCoords *coords,
                                                  gint             *x,
                                                  gint             *y);

static void       gimp_smudge_accumulator_size   (GimpPaintOptions *paint_options,
                                                  gint             *accumulator_size);
#endif


G_DEFINE_TYPE (GimpSmudge, gimp_smudge, GIMP_TYPE_BRUSH_CORE)

#define parent_class gimp_smudge_parent_class


void
gimp_smudge_register (Gimp                      *gimp,
                      GimpPaintRegisterCallback  callback)
{
  (* callback) (gimp,
                GIMP_TYPE_SMUDGE,
                GIMP_TYPE_SMUDGE_OPTIONS,
                "gimp-smudge",
                _("Smudge"),
                "gimp-tool-smudge");
}

static void
gimp_smudge_class_init (GimpSmudgeClass *klass)
{
  GObjectClass       *object_class     = G_OBJECT_CLASS (klass);
  GimpPaintCoreClass *paint_core_class = GIMP_PAINT_CORE_CLASS (klass);
  GimpBrushCoreClass *brush_core_class = GIMP_BRUSH_CORE_CLASS (klass);

  object_class->finalize  = gimp_smudge_finalize;

  paint_core_class->paint = gimp_smudge_paint;

  brush_core_class->handles_changing_brush = TRUE;
  brush_core_class->handles_transforming_brush = TRUE;
  brush_core_class->handles_dynamic_transforming_brush = TRUE;
}

static void
gimp_smudge_init (GimpSmudge *smudge)
{
  smudge->initialized = FALSE;
  smudge->accum_data  = NULL;
  smudge->blending_data = NULL;
#if 0
  smudge->accum_size  = 0;
#endif
}

static void
gimp_smudge_finalize (GObject *object)
{
  GimpSmudge *smudge = GIMP_SMUDGE (object);

  if (smudge->accum_data)
    {
      g_free (smudge->accum_data);
      smudge->accum_data = NULL;
    }
  
  if (smudge->blending_data)
    {
      g_free (smudge->blending_data);
      smudge->blending_data = NULL;
    }
  

#if 0
  smudge->accum_size = 0;
#endif

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gimp_smudge_paint (GimpPaintCore    *paint_core,
                   GimpDrawable     *drawable,
                   GimpPaintOptions *paint_options,
                   const GimpCoords *coords,
                   GimpPaintState    paint_state,
                   guint32           time)
{
  GimpSmudge *smudge = GIMP_SMUDGE (paint_core);

  switch (paint_state)
    {
    case GIMP_PAINT_STATE_MOTION:
      /* initialization fails if the user starts outside the drawable */
      if (! smudge->initialized)
        smudge->initialized = gimp_smudge_start (paint_core, drawable,
                                                 paint_options, coords);

      if (smudge->initialized)
        gimp_smudge_motion (paint_core, drawable, paint_options, coords);
      break;

    case GIMP_PAINT_STATE_FINISH:
      if (smudge->accum_data)
        {
          g_free (smudge->accum_data);
          smudge->accum_data = NULL;
        }
      if (smudge->blending_data)
        {
          g_free (smudge->blending_data);
          smudge->blending_data = NULL;
        }
      smudge->initialized = FALSE;
      break;

    default:
      break;
    }
}

static gboolean
gimp_smudge_start (GimpPaintCore    *paint_core,
                   GimpDrawable     *drawable,
                   GimpPaintOptions *paint_options,
                   const GimpCoords *coords)
{
#if 0
  GimpSmudge        *smudge     = GIMP_SMUDGE (paint_core);
  GimpImage         *image;
  TempBuf           *area;
  PixelRegion        srcPR;
  gint               bytes;
  gint               x, y, w, h;
#endif
  GimpSmudge  *smudge = GIMP_SMUDGE (paint_core);
  TempBuf     *area;
  PixelRegion  srcPR;
  gint         bytes;
  gint         x, y, w, h;
  GimpBrushCore *brush_core  = GIMP_BRUSH_CORE (paint_core);        /* gimp-painter-2.7 */
  GimpSmudgeOptions *options = GIMP_SMUDGE_OPTIONS (paint_options); /* gimp-painter-2.7 */
#if 0
  gint         x, y;
#endif

  if (gimp_drawable_is_indexed (drawable))
    return FALSE;

  brush_core->ignore_scale = TRUE;

  area  = gimp_paint_core_get_paint_area (paint_core, drawable, paint_options,
                                         coords);
  if (! area)
    return FALSE;

  /*  adjust the x and y coordinates to the upper left corner of the brush  */
  smudge->max_radius = 0;
  w = h = 0;
  gimp_smudge_brush_coords (paint_core, paint_options, coords, &x, &y, &w, &h);
//  g_print ("smudge:start: (x,y,w,h)=%d,%d,%d,%d\n", x, y, w, h);

  /*  Allocate the accumulation buffer */
  bytes = gimp_drawable_bytes (drawable);
  smudge->accum_data = g_malloc (w * h * bytes);
  if (options->use_color_blending)
    smudge->blending_data = g_malloc (w * h * bytes);
#if 0
  gimp_smudge_accumulator_size(paint_options, &smudge->accum_size);

  /*  adjust the x and y coordinates to the upper left corner of the
   *  accumulator
   */
  gimp_smudge_accumulator_coords (paint_core, coords, &x, &y);

  /*  Allocate the accumulation buffer */
  bytes = gimp_drawable_bytes (drawable);
  smudge->accum_data = g_malloc (SQR (smudge->accum_size) * bytes);
#endif

  /*  If clipped, prefill the smudge buffer with the color at the
   *  brush position.
   */
  if (x != area->x ||
      y != area->y ||
      w != area->width ||
      h != area->height)
    {
      guchar fill[4];

      gimp_pickable_get_pixel_at (GIMP_PICKABLE (drawable),
                                  CLAMP ((gint) coords->x,
                                         0,
                                         gimp_item_get_width (GIMP_ITEM (drawable)) - 1),
                                  CLAMP ((gint) coords->y,
                                         0,
                                         gimp_item_get_height (GIMP_ITEM (drawable)) - 1),
                                  fill);

      pixel_region_init_data (&srcPR, smudge->accum_data,
                              bytes, bytes * w,
                              0, 0, w, h);

      color_region (&srcPR, fill);
    }

  pixel_region_init (&srcPR, gimp_drawable_get_tiles (drawable),
                     area->x, area->y, area->width, area->height, FALSE);

  pixel_region_init_data (&smudge->accumPR, smudge->accum_data,
                          bytes, bytes * w,
                          area->x - x,
                          area->y - y,
                          area->width,
                          area->height);

  /* copy the region under the original painthit. */
  copy_region (&srcPR, &smudge->accumPR);

  pixel_region_init_data (&smudge->accumPR, smudge->accum_data,
                          bytes, bytes * w,
                          area->x - x,
                          area->y - y,
                          area->width,
                          area->height);

  brush_core->ignore_scale = FALSE;

  return TRUE;
}

static void
gimp_smudge_motion (GimpPaintCore    *paint_core,
                    GimpDrawable     *drawable,
                    GimpPaintOptions *paint_options,
                    const GimpCoords *coords)
{
  GimpSmudge         *smudge   = GIMP_SMUDGE (paint_core);
  GimpSmudgeOptions  *options  = GIMP_SMUDGE_OPTIONS (paint_options);
  GimpContext        *context  = GIMP_CONTEXT (paint_options);
  GimpDynamics       *dynamics = GIMP_BRUSH_CORE (paint_core)->dynamics;
  GimpDynamicsOutput *opacity_output;
  GimpDynamicsOutput *rate_output;
  GimpDynamicsOutput *hardness_output;
  GimpDynamicsOutput *blending_output;
  GimpImage          *image;
  TempBuf            *area;
  PixelRegion         srcPR, destPR, tempPR;
  gdouble             fade_point;
  gdouble             opacity;
  gdouble             rate;
  gdouble             dynamic_rate;
  gint                x, y, w, h;
  gdouble             hardness;
  TempBuf            *old_canvas_buf = NULL; /* gimp-painter-2.7 */
  GimpBrushCore      *brush_core = GIMP_BRUSH_CORE (paint_core); /* gimp-painter-2.7 */

  if (gimp_drawable_is_indexed (drawable))
    return;

  image = gimp_item_get_image (GIMP_ITEM (drawable));

  opacity_output = gimp_dynamics_get_output (dynamics,
                                             GIMP_DYNAMICS_OUTPUT_OPACITY);

  fade_point = gimp_paint_options_get_fade (paint_options, image,
                                            paint_core->pixel_dist);

  opacity = gimp_dynamics_output_get_linear_value (opacity_output,
                                                   coords,
                                                   paint_options,
                                                   fade_point);
  if (opacity == 0.0)
    return;

  brush_core->ignore_scale = TRUE;

  area = gimp_paint_core_get_paint_area (paint_core, drawable, paint_options,
                                         coords);
  if (! area)
    return;

#if 1
  /*  Get the unclipped brush coordinates  */
  gimp_smudge_brush_coords (paint_core, paint_options, coords, &x, &y, &w, &h);
//  g_print ("smudge:motion: (x,y,w,h)=%d,%d,%d,%d\n", x, y, w, h);
#else
  /*  Get the unclipped acumulator coordinates  */
  gimp_smudge_accumulator_coords (paint_core, coords, &x, &y);
#endif

  /* srcPR will be the pixels under the current painthit from the drawable */
  pixel_region_init (&srcPR, gimp_drawable_get_tiles (drawable),
                     area->x, area->y, area->width, area->height, FALSE);

  /* Enable dynamic rate */
  rate_output = gimp_dynamics_get_output (dynamics,
                                          GIMP_DYNAMICS_OUTPUT_RATE);

  dynamic_rate = gimp_dynamics_output_get_linear_value (rate_output,
                                                        coords,
                                                        paint_options,
                                                        fade_point);

  rate = (options->rate / 100.0) * dynamic_rate;

  /* The tempPR will be the built up buffer (for smudge) */
  pixel_region_init_data (&tempPR, smudge->accum_data,
                          smudge->accumPR.bytes,
                          smudge->accumPR.rowstride,
                          area->x - x,
                          area->y - y,
                          area->width,
                          area->height);

  /* The dest will be the paint area we got above (= canvas_buf) */
  pixel_region_init_temp_buf (&destPR, area,
                              0, 0, area->width, area->height);
 
  /*  Smudge uses the buffer Accum.
   *  For each successive painthit Accum is built like this
   *    Accum =  rate*Accum  + (1-rate)*I.
   *  where I is the pixels under the current painthit.
   *  Then the paint area (canvas_buf) is built as
   *    (Accum,1) (if no alpha),
   */

  blend_region (&srcPR, &tempPR, &tempPR, ROUND (rate * 255.0));

  /* re-init the tempPR */
  pixel_region_init_data (&tempPR, smudge->accum_data,
                          smudge->accumPR.bytes,
                          smudge->accumPR.rowstride,
                          area->x - x,
                          area->y - y,
                          area->width,
                          area->height);

  if (options->use_color_blending)
    {
      guchar col[MAX_CHANNELS];
      gdouble blending_rate = 1.0;
      PixelRegion        blendPR;
      
      pixel_region_init_data (&blendPR, smudge->blending_data,
                              smudge->accumPR.bytes,
                              smudge->accumPR.rowstride,
                              area->x - x,
                              area->y - y,
                              area->width,
                              area->height);

      blending_output = gimp_dynamics_get_output (dynamics,
                                                  GIMP_DYNAMICS_OUTPUT_BLENDING);

      blending_rate *= gimp_dynamics_output_get_linear_value (blending_output,
                                                              coords,
                                                              paint_options,
                                                              fade_point);

      gimp_image_get_foreground (image, context, gimp_drawable_type (drawable), col);
      col[area->bytes - 1] = OPAQUE_OPACITY;
      shade_region (&tempPR, &blendPR, col, ROUND (blending_rate * 255.0));

      /* re-initialize the tempPR */
      pixel_region_init_data (&tempPR, smudge->blending_data,
                              smudge->accumPR.bytes,
                              smudge->accumPR.rowstride,
                              area->x - x,
                              area->y - y,
                              area->width,
                              area->height);
    }

  if (! gimp_drawable_has_alpha (drawable))
    add_alpha_region (&tempPR, &destPR);
  else
    copy_region (&tempPR, &destPR);

  brush_core->ignore_scale = FALSE;

  {
    gint brush_width,brush_height;
    gint x, y;
    gint x1, y1, x2, y2;
    gint drawable_width, drawable_height;
    
    gimp_brush_core_eval_transform_dynamics (brush_core,
                                             drawable,
                                             paint_options,
                                             coords);
    /* FIXME: following code is simply copied from gimp_brush_core_clamp_scale */
    {
      TempBuf *mask = brush_core->main_brush->mask;

      /* ensure that the final brush mask remains >= 0.5 pixel along both axes */
      brush_core->scale = MAX (0.5 / (gfloat) MIN (mask->width, mask->height), brush_core->scale);
    }

    gimp_brush_transform_size (brush_core->brush,
                               brush_core->scale, brush_core->aspect_ratio, brush_core->angle,
                               &brush_width, &brush_height);

    /*  adjust the x and y coordinates to the upper left corner of the brush  */
    x = (gint) floor (coords->x) - (brush_width  / 2);
    y = (gint) floor (coords->y) - (brush_height / 2);

    drawable_width  = gimp_item_get_width  (GIMP_ITEM (drawable));
    drawable_height = gimp_item_get_height (GIMP_ITEM (drawable));

    x1 = CLAMP (x - 1, 0, drawable_width);
    y1 = CLAMP (y - 1, 0, drawable_height);
    x2 = CLAMP (x + brush_width  + 1, 0, drawable_width);
    y2 = CLAMP (y + brush_height + 1, 0, drawable_height);

    /*  configure the canvas buffer  */
    if ((x2 - x1) && (y2 - y1))
      {
        gint bytes;

        bytes = gimp_drawable_bytes_with_alpha (drawable);

        old_canvas_buf         = paint_core->canvas_buf;
        paint_core->canvas_buf = temp_buf_subwindow (paint_core->canvas_buf,
                                                     x1, y1,
                                                     (x2 - x1), (y2 - y1));
      }
  }
  
  hardness_output = gimp_dynamics_get_output (dynamics,
                                              GIMP_DYNAMICS_OUTPUT_HARDNESS);

  hardness = gimp_dynamics_output_get_linear_value (hardness_output,
                                                    coords,
                                                    paint_options,
                                                    fade_point);

  gimp_brush_core_replace_canvas (GIMP_BRUSH_CORE (paint_core), drawable,
                                  coords,
                                  MIN (opacity, GIMP_OPACITY_OPAQUE),
                                  gimp_context_get_opacity (context),
                                  gimp_paint_options_get_brush_mode (paint_options),
                                  hardness,
                                  GIMP_PAINT_INCREMENTAL);
  if (old_canvas_buf)
     paint_core->canvas_buf = old_canvas_buf;
}

#if 1
static void
gimp_smudge_brush_coords (GimpPaintCore    *paint_core,
                          GimpPaintOptions *paint_options,
                          const GimpCoords *coords,
                          gint             *x,
                          gint             *y,
                          gint             *w,
                          gint             *h)
{
  GimpBrushCore *brush_core = GIMP_BRUSH_CORE (paint_core);
  GimpSmudge    *smudge     = GIMP_SMUDGE (paint_core);
  gint           width = 0;
  gint           height = 0;

  if (smudge->max_radius == 0)
    {
      if (brush_core->main_brush)
        brush_core->scale = paint_options->brush_size /
                            MAX (brush_core->main_brush->mask->width,
                                 brush_core->main_brush->mask->height);
      else
        brush_core->scale = -1;

      gimp_brush_transform_size (brush_core->brush,
                                 brush_core->scale,
                                 brush_core->aspect_ratio,
                                 brush_core->angle,
                                 &width, &height);
      smudge->max_radius = ceil(sqrt(width * width + height * height));
    }

  width = height = (gint)smudge->max_radius;

  /* Note: these are the brush mask size plus a border of 1 pixel */
  *x = (gint) coords->x - width  / 2 - 1;
  *y = (gint) coords->y - height / 2 - 1;
  *w = width  + 2;
  *h = height + 2;
}
#else
static void
gimp_smudge_accumulator_coords (GimpPaintCore    *paint_core,
                                const GimpCoords *coords,
                                gint             *x,
                                gint             *y)
{
  GimpSmudge *smudge = GIMP_SMUDGE (paint_core);

  *x = (gint) coords->x - smudge->accum_size / 2;
  *y = (gint) coords->y - smudge->accum_size / 2;
}

static void
gimp_smudge_accumulator_size (GimpPaintOptions *paint_options,
                              gint             *accumulator_size)
{

  /* Note: the max brush mask size plus a border of 1 pixel and a little
   * headroom */
  *accumulator_size = ceil (sqrt (2 * SQR (paint_options->brush_size + 1)) + 2);
}
#endif
