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
#include "base/delegators.hpp"
#include "base/scopeguard.hpp"
#include "base/glib-cxx-bridge.hpp"
#include "base/glib-cxx-utils.hpp"
#include "base/glib-cxx-types.hpp"
#include "base/glib-cxx-impl.hpp"
#include "base/selectcase-utils.hpp"
#include "base/tile.h"
#include "base/tile-manager.h"
#include <functional>

using namespace GLib;

extern "C" {

#include <gegl.h>

#include "config.h"

#include <gtk/gtk.h>

#include "libgimpwidgets/gimpwidgets.h"

#include "tools-types.h"
#include "gimp-tools.h"

#include "core/gimptoolinfo.h"
#include "core/gimpchannel.h"
#include "core/gimpimage-contiguous-region.h"
#include "paint/gimp-paint.h"
#include "widgets/gimphelp-ids.h"
#include "widgets/gimppropwidgets.h"

#include "gimptooloptions-gui.h"
#include "gimppaintoptions-gui.h"
#include "gimptoolcontrol.h"
#include "gimp-intl.h"



////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Paint Option

#include "paint/gimppaintoptions.h"

#define GIMP_TYPE_BUCKET_FILL_BRUSH_OPTIONS            (gimp_bucket_fill_brush_options_get_type ())
#define GIMP_BUCKET_FILL_BRUSH_OPTIONS(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIMP_TYPE_BUCKET_FILL_BRUSH_OPTIONS, GimpBucketFillBrushOptions))
#define GIMP_BUCKET_FILL_BRUSH_OPTIONS_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GIMP_TYPE_BUCKET_FILL_BRUSH_OPTIONS, GimpBucketFillBrushOptionsClass))
#define GIMP_IS_BUCKET_FILL_BRUSH_OPTIONS(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIMP_TYPE_BUCKET_FILL_BRUSH_OPTIONS))
#define GIMP_IS_BUCKET_FILL_BRUSH_OPTIONS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GIMP_TYPE_BUCKET_FILL_BRUSH_OPTIONS))
#define GIMP_BUCKET_FILL_BRUSH_OPTIONS_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GIMP_TYPE_BUCKET_FILL_BRUSH_OPTIONS, GimpBucketFillBrushOptionsClass))


typedef struct 
{
  GimpPaintOptions  parent_instance;

  gdouble           rate;
  gboolean          use_color_blending;
} GimpBucketFillBrushOptions;

typedef struct
{
  GimpPaintOptionsClass  parent_class;
} GimpBucketFillBrushOptionsClass;


GType   gimp_bucket_fill_brush_options_get_type (void) G_GNUC_CONST;

};


extern "C" {

#include "config.h"

#include <gegl.h>

#include "libgimpmath/gimpmath.h"

#include "paint/paint-types.h"

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
#include "core/gimppaintinfo.h"

#include "gimp-intl.h"

};



////////////////////////////////////////////////////////////////////////////////////////////////////////////

extern "C" {

#include "config.h"

#include <glib-object.h>

#include "libgimpconfig/gimpconfig.h"

#include "paint/paint-types.h"


#define BUCKET_FILL_BRUSH_DEFAULT_RATE 50.0
#define DEFAULT_USE_COLOR_BLENDING FALSE


enum
{
  PROP_0,
  PROP_RATE,
  PROP_USE_COLOR_BLENDING
};


static void   gimp_bucket_fill_brush_options_set_property (GObject      *object,
                                                guint         property_id,
                                                const GValue *value,
                                                GParamSpec   *pspec);
static void   gimp_bucket_fill_brush_options_get_property (GObject      *object,
                                                guint         property_id,
                                                GValue       *value,
                                                GParamSpec   *pspec);


////////////////////////////////////////////////////////////////////////////////////////////////////////////
G_DEFINE_TYPE (GimpBucketFillBrushOptions, gimp_bucket_fill_brush_options,
               GIMP_TYPE_PAINT_OPTIONS)
};


__DECLARE_GTK_CLASS__(GimpBucketFillBrushOptions, GIMP_TYPE_BUCKET_FILL_BRUSH_OPTIONS);

static void
gimp_bucket_fill_brush_options_class_init (GimpBucketFillBrushOptionsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = gimp_bucket_fill_brush_options_set_property;
  object_class->get_property = gimp_bucket_fill_brush_options_get_property;

  GIMP_CONFIG_INSTALL_PROP_DOUBLE (object_class, PROP_RATE,
                                   "rate", NULL,
                                   0.0, 100.0, BUCKET_FILL_BRUSH_DEFAULT_RATE,
                                   GIMP_PARAM_STATIC_STRINGS);
                                   
  GIMP_CONFIG_INSTALL_PROP_BOOLEAN (object_class, PROP_USE_COLOR_BLENDING,
                                    "use-color-blending", NULL,
                                    DEFAULT_USE_COLOR_BLENDING,
                                    GIMP_PARAM_STATIC_STRINGS);
}

static void
gimp_bucket_fill_brush_options_init (GimpBucketFillBrushOptions *options)
{
}

static void
gimp_bucket_fill_brush_options_set_property (GObject      *object,
                                  guint         property_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  GimpBucketFillBrushOptions *options = GIMP_BUCKET_FILL_BRUSH_OPTIONS (object);

  switch (property_id)
    {
    case PROP_RATE:
      options->rate = g_value_get_double (value);
      break;
      
    case PROP_USE_COLOR_BLENDING:
      options->use_color_blending = g_value_get_boolean (value);
      break;
    
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gimp_bucket_fill_brush_options_get_property (GObject    *object,
                                    guint       property_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  GimpBucketFillBrushOptions *options = GIMP_BUCKET_FILL_BRUSH_OPTIONS (object);

  switch (property_id)
    {
    case PROP_RATE:
      g_value_set_double (value, options->rate);
      break;
 
    case PROP_USE_COLOR_BLENDING:
      g_value_set_boolean (value, options->use_color_blending);
      break;
 
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Paint Core

extern "C" {

#include "base/pixel-region.h"

#include "paint/gimpbrushcore.h"

#define GIMP_TYPE_BUCKET_FILL_BRUSH            (gimp_bucket_fill_brush_get_type ())
#define GIMP_BUCKET_FILL_BRUSH(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIMP_TYPE_BUCKET_FILL_BRUSH, GimpBucketFillBrush))
#define GIMP_BUCKET_FILL_BRUSH_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GIMP_TYPE_BUCKET_FILL_BRUSH, GimpBucketFillBrushClass))
#define GIMP_IS_BUCKET_FILL_BRUSH(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIMP_TYPE_BUCKET_FILL_BRUSH))
#define GIMP_IS_BUCKET_FILL_BRUSH_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GIMP_TYPE_BUCKET_FILL_BRUSH))
#define GIMP_BUCKET_FILL_BRUSH_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GIMP_TYPE_BUCKET_FILL_BRUSH, GimpBucketFillBrushClass))


struct _GimpBucketFillBrush
{
  GimpBrushCore  parent_instance;

  gboolean       initialized;
  guchar        *blending_data;
  guint          max_radius;
};
typedef struct _GimpBucketFillBrush GimpBucketFillBrush;

struct _GimpBucketFillBrushClass
{
  GimpBrushCoreClass  parent_class;
};
typedef struct _GimpBucketFillBrushClass GimpBucketFillBrushClass;

static void       gimp_bucket_fill_brush_finalize     (GObject          *object);

static void       gimp_bucket_fill_brush_paint        (GimpPaintCore    *paint_core,
                                            GimpDrawable     *drawable,
                                            GimpPaintOptions *paint_options,
                                            const GimpCoords *coords,
                                            GimpPaintState    paint_state,
                                            guint32           time);
static gboolean   gimp_bucket_fill_brush_start        (GimpPaintCore    *paint_core,
                                            GimpDrawable     *drawable,
                                            GimpPaintOptions *paint_options,
                                            const GimpCoords *coords);
static void       gimp_bucket_fill_brush_motion       (GimpPaintCore    *paint_core,
                                            GimpDrawable     *drawable,
                                            GimpPaintOptions *paint_options,
                                            const GimpCoords *coords);

static void       gimp_bucket_fill_brush_brush_coords (GimpPaintCore    *paint_core,
                                            GimpPaintOptions  *options,
                                            const GimpCoords *coords,
                                            gint             *x,
                                            gint             *y,
                                            gint             *w,
                                            gint             *h);

GType   gimp_bucket_fill_brush_get_type (void) G_GNUC_CONST;

////////////////////////////////////////////////////////////////////////////////////////////////////////////
G_DEFINE_TYPE (GimpBucketFillBrush, gimp_bucket_fill_brush, GIMP_TYPE_BRUSH_CORE)
#define parent_class gimp_bucket_fill_brush_parent_class
};

__DECLARE_GTK_CLASS__(GimpBucketFillBrush, GIMP_TYPE_BUCKET_FILL_BRUSH);


static void
gimp_bucket_fill_brush_register (Gimp *gimp)
{
  GimpPaintInfo* paint_info;
  paint_info = gimp_paint_info_new (gimp,
                                    GIMP_TYPE_BUCKET_FILL_BRUSH,
                                    GIMP_TYPE_BUCKET_FILL_BRUSH_OPTIONS,
                                    "gimp-bucket-fill-brush",
                                    _("BucketFillBrush"),
                                    "gimp-tool-bucket-fill-brush");
  gimp_paint_register (gimp, paint_info);
}

static void
gimp_bucket_fill_brush_class_init (GimpBucketFillBrushClass *klass)
{
  GObjectClass       *object_class     = G_OBJECT_CLASS (klass);
  GimpPaintCoreClass *paint_core_class = GIMP_PAINT_CORE_CLASS (klass);
  GimpBrushCoreClass *brush_core_class = GIMP_BRUSH_CORE_CLASS (klass);

  object_class->finalize  = gimp_bucket_fill_brush_finalize;

  paint_core_class->paint = gimp_bucket_fill_brush_paint;

  brush_core_class->handles_changing_brush = TRUE;
  brush_core_class->handles_transforming_brush = TRUE;
  brush_core_class->handles_dynamic_transforming_brush = TRUE;
}

static void
gimp_bucket_fill_brush_init (GimpBucketFillBrush *bucket_fill_brush)
{
  bucket_fill_brush->initialized = FALSE;
  bucket_fill_brush->blending_data = NULL;
}

static void
gimp_bucket_fill_brush_finalize (GObject *object)
{
  GimpBucketFillBrush *bucket_fill_brush = GIMP_BUCKET_FILL_BRUSH (object);
  
  if (bucket_fill_brush->blending_data)
    {
      g_free (bucket_fill_brush->blending_data);
      bucket_fill_brush->blending_data = NULL;
    }
  

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gimp_bucket_fill_brush_paint (GimpPaintCore    *paint_core,
                   GimpDrawable     *drawable,
                   GimpPaintOptions *paint_options,
                   const GimpCoords *coords,
                   GimpPaintState    paint_state,
                   guint32           time)
{
  GimpBucketFillBrush *bucket_fill_brush = GIMP_BUCKET_FILL_BRUSH (paint_core);

  switch (paint_state)
    {
    case GIMP_PAINT_STATE_MOTION:
      /* initialization fails if the user starts outside the drawable */
      if (! bucket_fill_brush->initialized)
        bucket_fill_brush->initialized = gimp_bucket_fill_brush_start (paint_core, drawable,
                                                 paint_options, coords);

      if (bucket_fill_brush->initialized)
        gimp_bucket_fill_brush_motion (paint_core, drawable, paint_options, coords);
      break;

    case GIMP_PAINT_STATE_FINISH:
      if (bucket_fill_brush->blending_data)
        {
          g_free (bucket_fill_brush->blending_data);
          bucket_fill_brush->blending_data = NULL;
        }
      bucket_fill_brush->initialized = FALSE;
      break;

    default:
      break;
    }
}

static gboolean
gimp_bucket_fill_brush_start (GimpPaintCore    *paint_core,
                   GimpDrawable     *drawable,
                   GimpPaintOptions *paint_options,
                   const GimpCoords *coords)
{
#if 0
  GimpBucketFillBrush  *bucket_fill_brush = GIMP_BUCKET_FILL_BRUSH (paint_core);
  TempBuf     *area;
  PixelRegion  srcPR;
  gint         bytes;
  gint         x, y, w, h;
  GimpBrushCore *brush_core  = GIMP_BRUSH_CORE (paint_core);        /* gimp-painter-2.7 */
  GimpBucketFillBrushOptions *options = GIMP_BUCKET_FILL_BRUSH_OPTIONS (paint_options); /* gimp-painter-2.7 */

  if (gimp_drawable_is_indexed (drawable))
    return FALSE;

  brush_core->ignore_scale = TRUE;

  area  = gimp_paint_core_get_paint_area (paint_core, drawable, paint_options,
                                         coords);
  if (! area)
    return FALSE;

  /*  adjust the x and y coordinates to the upper left corner of the brush  */
  bucket_fill_brush->max_radius = 0;
  w = h = 0;
  gimp_bucket_fill_brush_brush_coords (paint_core, paint_options, coords, &x, &y, &w, &h);
//  g_print ("bucket_fill_brush:start: (x,y,w,h)=%d,%d,%d,%d\n", x, y, w, h);

  /*  Allocate the accumulation buffer */
  bytes = gimp_drawable_bytes (drawable);
  if (options->use_color_blending)
    bucket_fill_brush->blending_data = (guchar*)g_malloc (w * h * bytes);

  /*  If clipped, prefill the bucket_fill_brush buffer with the color at the
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

      color_region (&srcPR, fill);
    }

  pixel_region_init (&srcPR, gimp_drawable_get_tiles (drawable),
                     area->x, area->y, area->width, area->height, FALSE);

  /* copy the region under the original painthit. */
//  copy_region (&srcPR, &bucket_fill_brush->accumPR);

//  pixel_region_init_data (&bucket_fill_brush->accumPR, bucket_fill_brush->accum_data,
//                          bytes, bytes * w,
//                          area->x - x,
//                          area->y - y,
//                          area->width,
//                          area->height);

  brush_core->ignore_scale = FALSE;
#endif
  return TRUE;
}

static void
gimp_bucket_fill_brush_motion (GimpPaintCore    *_paint_core,
                               GimpDrawable     *_drawable,
                               GimpPaintOptions *_paint_options,
                               const GimpCoords *coords)
{
  auto paint_core    = ref(_paint_core);
  auto drawable      = ref(_drawable);
  auto paint_options = ref(_paint_options);
  auto options           = GIMP_BUCKET_FILL_BRUSH_OPTIONS (paint_options.ptr());
  auto bucket_fill_brush = GIMP_BUCKET_FILL_BRUSH (paint_core.ptr());
  auto context           = ref(GIMP_CONTEXT (paint_options.ptr()));
  auto dynamics          = ref(GIMP_BRUSH_CORE (paint_core.ptr())->dynamics);
  auto opacity_output    = ref(dynamics [gimp_dynamics_get_output] (GIMP_DYNAMICS_OUTPUT_OPACITY));
  auto force_output      = ref(dynamics [gimp_dynamics_get_output] (GIMP_DYNAMICS_OUTPUT_FORCE));
  auto image             = ref(drawable [gimp_item_get_image] ());
  TempBuf* area          = paint_core [gimp_paint_core_get_paint_area] (drawable, paint_options, coords);

  // FIXME: indexed should be covered in the future.
  if (gimp_drawable_is_indexed (drawable))
    return;

  IObject<GimpDynamicsOutput> rate_output      = ref(dynamics [gimp_dynamics_get_output] (GIMP_DYNAMICS_OUTPUT_RATE));
  IObject<GimpDynamicsOutput> hardness_output  = ref(dynamics [gimp_dynamics_get_output] (GIMP_DYNAMICS_OUTPUT_HARDNESS));
  gdouble                     fade_point       = paint_options [gimp_paint_options_get_fade] (image, paint_core->pixel_dist);
  gdouble                     opacity          = opacity_output [gimp_dynamics_output_get_linear_value] (coords, paint_options, fade_point);
  gdouble                     dynamic_rate     = rate_output [gimp_dynamics_output_get_linear_value] (coords, paint_options, fade_point);
  gdouble                     rate             = (options->rate / 100.0) * dynamic_rate;
  gdouble                     hardness;
  gdouble                     force;
  auto                        brush_core       = ref(GIMP_BRUSH_CORE (paint_core.ptr()));

  if (opacity == 0.0)
    return;

  if (! area)
    return;

  // Fill canvas_buf with foreground color.
  PixelRegion pr_area;
  pixel_region_init_temp_buf (&pr_area, area, area->x, area->y, area->width, area->height);

  guchar col[MAX_CHANNELS];
  gimp_image_get_foreground (image, context, gimp_drawable_type (drawable), col);

  col[area->bytes - 1] = OPAQUE_OPACITY;
  color_pixels (temp_buf_get_data (area), col,
              area->width * area->height,
              area->bytes);

  gimp_brush_core_eval_transform_dynamics (brush_core, drawable, paint_options, coords);


  /* FIXME: following code is simply copied from gimp_brush_core_clamp_scale */
  TempBuf *mask = brush_core->main_brush->mask;
  brush_core->scale = MAX (0.5 / (gfloat) MIN (mask->width, mask->height), brush_core->scale);

  gint brush_width,brush_height;
  ref(brush_core->brush) [gimp_brush_transform_size] (brush_core->scale, brush_core->aspect_ratio, brush_core->angle, &brush_width, &brush_height);


  /*  adjust the x and y coordinates to the upper left corner of the brush  */
  gint x = (gint) floor (coords->x) - (brush_width  / 2);
  gint y = (gint) floor (coords->y) - (brush_height / 2);

  gint drawable_width  = drawable [gimp_item_get_width]  ();
  gint drawable_height = drawable [gimp_item_get_height] ();

  gint x1 = CLAMP (x - 1, 0, drawable_width);
  gint y1 = CLAMP (y - 1, 0, drawable_height);
  gint x2 = CLAMP (x + brush_width  + 1, 0, drawable_width);
  gint y2 = CLAMP (y + brush_height + 1, 0, drawable_height);

  gint offx, offy;
  drawable [gimp_item_get_offset] (&offx, &offy);

  hardness = hardness_output [gimp_dynamics_output_get_linear_value] (coords, paint_options, fade_point);
  force    = hardness_output [gimp_dynamics_output_get_linear_value] (coords, paint_options, fade_point);
  
//  g_print("x1=%d, y1=%d, x2=%d, y2=%d, opacity=%lf, force=%lf\n", x1, y1, x2, y2, opacity, force);

  Object<GimpChannel> channel     = hold(gimp_channel_new_mask (image, image[gimp_image_get_width](), image[gimp_image_get_height]()));
  TileManager*        channel_buf = ref(channel) [gimp_drawable_get_tiles] ();
  TempBuf*            brush_mask  = (TempBuf*)brush_core [gimp_brush_core_get_brush_mask] (coords, paint_options [gimp_paint_options_get_brush_mode] (), force);
  PixelRegion         pr_ch, pr_br;

  pixel_region_init          (&pr_ch, channel_buf, x1 + 1 + offx, y1 + 1 + offy, brush_width, brush_height, TRUE);
  pixel_region_init_temp_buf (&pr_br, brush_mask, 0, 0, brush_width, brush_height);
  copy_region (&pr_br, &pr_ch);
  channel->x1 = x1;
  channel->x2 = x2;
  channel->y1 = y1;
  channel->y2 = y2;

  Object<GimpChannel> ch_mask = hold(gimp_image_contiguous_region_by_seed_full (image, drawable, channel.ptr(),
      TRUE, TRUE, 30, TRUE, GIMP_SELECT_CRITERION_COMPOSITE, coords->x, coords->y));
  
  pixel_region_init (&pr_ch, ref(ch_mask) [gimp_drawable_get_tiles](), x1 + 1 + offx, y1 + 1 + offy, brush_width, brush_height, FALSE);
//  pixel_region_init (&pr_ch, ref(channel) [gimp_drawable_get_tiles](), x1 + 1 + offx, y1 + 1 + offy, brush_width, brush_height, FALSE);
//  pixel_region_init_temp_buf (&pr_br, brush_mask, 0, 0, brush_width, brush_height);

  paint_core [gimp_paint_core_paste] (&pr_ch, drawable, MIN (opacity, GIMP_OPACITY_OPAQUE), 
//  paint_core [gimp_paint_core_paste] (&pr_br, drawable, MIN (opacity, GIMP_OPACITY_OPAQUE), 
      gimp_context_get_opacity (context), gimp_context_get_paint_mode (context), GIMP_PAINT_CONSTANT);
}

static void
gimp_bucket_fill_brush_brush_coords (GimpPaintCore    *paint_core,
                          GimpPaintOptions *paint_options,
                          const GimpCoords *coords,
                          gint             *x,
                          gint             *y,
                          gint             *w,
                          gint             *h)
{
  GimpBrushCore *brush_core = GIMP_BRUSH_CORE (paint_core);
  GimpBucketFillBrush    *bucket_fill_brush     = GIMP_BUCKET_FILL_BRUSH (paint_core);
  gint           width = 0;
  gint           height = 0;

  if (bucket_fill_brush->max_radius == 0)
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
      bucket_fill_brush->max_radius = ceil(sqrt(width * width + height * height));
    }

  width = height = (gint)bucket_fill_brush->max_radius;

  /* Note: these are the brush mask size plus a border of 1 pixel */
  *x = (gint) coords->x - width  / 2 - 1;
  *y = (gint) coords->y - height / 2 - 1;
  *w = width  + 2;
  *h = height + 2;
}



////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Tool definition
extern "C" {
#include "gimpbucketfillbrushtool.h"


static GtkWidget * gimp_bucket_fill_brush_options_gui (GimpToolOptions *tool_options);
static GtkWidget * gimp_bucket_fill_brush_options_gui_horizontal (GimpToolOptions *tool_options);
static GtkWidget * gimp_bucket_fill_brush_options_gui_full (GimpToolOptions *tool_options, gboolean horizontal);


G_DEFINE_TYPE (GimpBucketFillBrushTool, gimp_bucket_fill_brush_tool, GIMP_TYPE_BRUSH_TOOL)
};


void
gimp_bucket_fill_brush_tool_register (GimpToolRegisterCallback  callback,
                           gpointer                  data)
{
  g_return_if_fail (GIMP_IS_GIMP(data));

  Gimp* gimp = (Gimp*) (data);
  GimpToolInfo* tool_info;

  g_print("GimpBuckerFillBrushTool::register\n");
  gimp_bucket_fill_brush_register(gimp);

  tool_info = gimp_tool_info_new (gimp,
                                  GIMP_TYPE_BUCKET_FILL_BRUSH_TOOL,
                                  GIMP_TYPE_BUCKET_FILL_BRUSH_OPTIONS,
                                   GimpContextPropMask(GIMP_PAINT_OPTIONS_CONTEXT_MASK),
                                  "gimp-bucket-fill-brush-tool",
                                  _("BucketFillBrush"),
                                  _("Bucket-fill brush Tool: Bucket-fill brush"),
                                  N_("_BucketFillBrush"),
                                  "]",
                                  NULL,
                                  "gimp-bucket-fill-brush",
                                  "gimp-bucket-fill-brush",
                                  GIMP_STOCK_TOOL_BUCKET_FILL);

  g_object_set (tool_info, "visible", TRUE, NULL);
  g_object_set_data (G_OBJECT (tool_info), "gimp-tool-default-visible",
                     GINT_TO_POINTER (TRUE));

  g_object_set_data (G_OBJECT (tool_info), "gimp-tool-options-gui-func",
                     (gpointer)gimp_bucket_fill_brush_options_gui);

  g_object_set_data (G_OBJECT (tool_info), "gimp-tool-options-gui-horizontal-func",
                     (gpointer)gimp_bucket_fill_brush_options_gui_horizontal);

  gimp_tools_register (gimp, tool_info);
}

static void
gimp_bucket_fill_brush_tool_class_init (GimpBucketFillBrushToolClass *klass)
{
}

static void
gimp_bucket_fill_brush_tool_init (GimpBucketFillBrushTool *bucket_fill_brush)
{
  GimpTool      *tool       = GIMP_TOOL (bucket_fill_brush);
  GimpPaintTool *paint_tool = GIMP_PAINT_TOOL (bucket_fill_brush);

  gimp_tool_control_set_tool_cursor (tool->control, GIMP_TOOL_CURSOR_PAINTBRUSH);

  paint_tool->status      = _("Click to bucket fill brush");
  paint_tool->status_line = _("Click to bucket fill brush the line");
  paint_tool->status_ctrl = NULL;
}


/*  tool options stuff  */

static GtkWidget *
gimp_bucket_fill_brush_options_gui (GimpToolOptions *tool_options)
{
  return gimp_bucket_fill_brush_options_gui_full (tool_options, FALSE);
}

static GtkWidget *
gimp_bucket_fill_brush_options_gui_horizontal (GimpToolOptions *tool_options)
{
  return gimp_bucket_fill_brush_options_gui_full (tool_options, TRUE);
}

static GtkWidget *
gimp_bucket_fill_brush_options_gui_full (GimpToolOptions *tool_options, gboolean horizontal)
{
  GObject   *config = G_OBJECT (tool_options);
  GtkWidget *vbox   = gimp_paint_options_gui_full (tool_options, horizontal);
  GtkWidget *scale;
  GList     *children;

  /*  the rate scale  */
  scale = gimp_prop_spin_scale_new (config, "rate",
                                    _("Rate"),
                                    1.0, 10.0, 1);
  gtk_box_pack_start (GTK_BOX (vbox), scale, FALSE, FALSE, 0);
  gtk_widget_show (scale);

  children = gtk_container_get_children (GTK_CONTAINER (vbox));  
  gimp_tool_options_setup_popup_layout (children, FALSE);

  return vbox;
}
