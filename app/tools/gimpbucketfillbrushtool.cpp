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
#include <functional>

using namespace GLib;

extern "C" {

#include "base/tile.h"
#include "base/tile-manager.h"
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



#include "paint/gimppaintoptions.h"
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
#include "config.h"
#include <glib-object.h>
#include "libgimpconfig/gimpconfig.h"
#include "paint/paint-types.h"

};



////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Paint Option

static const int BUCKET_FILL_BRUSH_DEFAULT_RATE = 50.0;


struct Options : virtual public ImplBase
{
  gdouble rate;
  gboolean eraser_mode;

  Options(GObject* g_object) : ImplBase(g_object) {
    rate = BUCKET_FILL_BRUSH_DEFAULT_RATE;
    eraser_mode = FALSE;
  };

  ~Options() {

  };

  CopyValue get_rate() {
    return rate;
  };

  void set_rate(IValue v) {
    rate = v;
  }

  CopyValue get_eraser_mode() {
    return eraser_mode;
  };

  void set_eraser_mode(IValue v) {
    eraser_mode = v;
  }
};

extern const char gimp_bucket_fill_brush_options_name[] = "GimpBucketFillBrushOptions";
using OptionsClass = NewGClass<gimp_bucket_fill_brush_options_name, GLib::DerivedFrom<GimpPaintOptions>, Options>;

#define _override(method) OptionsClass::__(&klass->method).bind<&Options::method>()
#define _getter(method)   OptionsClass::__((CopyValue (**)(GObject*))NULL).bind<&Options::get_##method >()
#define _setter(method)   OptionsClass::__((void (**)(GObject*, IValue))NULL).bind<&Options::set_##method >()

OptionsClass options_class([](IGClass::IWithClass* with_class){
  g_print("OptionsClass::class_init\n");

  with_class
    ->install_property(
      OptionsClass::g_param_spec_new("rate", OptionsClass::Range<double>(0, 100.), 50.0, (GParamFlags)(GIMP_PARAM_READWRITE|G_PARAM_CONSTRUCT) ),
      _getter(rate), _setter(rate)

    )->install_property(
      OptionsClass::g_param_spec_new("eraser-mode", false, (GParamFlags)(GIMP_PARAM_READWRITE|G_PARAM_CONSTRUCT) ),
      _getter(eraser_mode), _setter(eraser_mode)
    );

});

#undef _override
#undef _getter
#undef _setter
namespace GLib {
template<> class Traits<OptionsClass::Instance> : public OptionsClass::Traits { };
};


////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Paint Core

extern "C" {
#include "base/pixel-region.h"
#include "paint/gimpbrushcore.h"
};

struct Brush : public virtual ImplBase {
  gboolean       initialized;
  guint          max_radius;

  TileManager   *tiles;
  GimpImageType  src_type;
  gboolean       has_alpha;
  gint           bytes;
  gint           off_x;
  gint           off_y;
  guchar         start_col[MAX_CHANNELS];

  Brush(GObject* obj) : ImplBase(obj) {
    initialized = FALSE;
    tiles       = NULL;
  }
  ~Brush() {
    if (tiles) {
      tile_manager_unref (tiles);
      tiles = NULL;
    }  
  }

  void paint (GimpDrawable     *drawable,
              GimpPaintOptions *paint_options,
              const GimpCoords *coords,
              GimpPaintState    paint_state,
              guint32           time);

  gboolean start (GimpDrawable     *_drawable,
                  GimpPaintOptions *_paint_options,
                  const GimpCoords *coords);

  void motion (GimpDrawable     *_drawable,
               GimpPaintOptions *_paint_options,
               const GimpCoords *coords);

};

extern const char gimp_bucket_fill_brush_name[] = "GimpBucketFillBrush";
using BrushClass = NewGClass<gimp_bucket_fill_brush_name, GLib::DerivedFrom<GimpBrushCore>, Brush>;

#define _override(method) BrushClass::__(&klass->method).bind<&Brush::method>()

BrushClass brush_class([](IGClass::IWithClass* with_class){
  g_print("BrushClass::class_init\n");
  with_class->
    as_class<GimpPaintCore>([](GimpPaintCoreClass* klass) {
      _override (paint);
    })->as_class<GimpBrushCore>([](GimpBrushCoreClass* klass) {
      klass->handles_changing_brush             = TRUE;
      klass->handles_transforming_brush         = TRUE;
      klass->handles_dynamic_transforming_brush = TRUE;
    });
});

#undef override

namespace GLib {
template<> class Traits<BrushClass::Instance> : public BrushClass::Traits { };
};



void
Brush::paint (GimpDrawable     *drawable,
              GimpPaintOptions *paint_options,
              const GimpCoords *coords,
              GimpPaintState    paint_state,
              guint32           time)
{
  switch (paint_state)
    {
    case GIMP_PAINT_STATE_MOTION:
      /* initialization fails if the user starts outside the drawable */
      if (! initialized)
        initialized = start (drawable, paint_options, coords);

      if (initialized)
        motion (drawable, paint_options, coords);
      break;

    case GIMP_PAINT_STATE_FINISH:
      if (tiles) {
        tile_manager_unref (tiles);
        tiles = NULL;
      }  

      initialized = FALSE;
      break;

    default:
      break;
    }
}


gboolean
Brush::start (GimpDrawable     *_drawable,
              GimpPaintOptions *_paint_options,
              const GimpCoords *coords)
{
  auto paint_core    = ref(GIMP_PAINT_CORE(g_object));
  auto drawable      = ref(_drawable);
  auto paint_options = ref(_paint_options);
  auto image             = ref(drawable [gimp_item_get_image] ());
  gint x, y;
  GimpPickable  *pickable;
  Tile* tile;


  x = coords->x;
  y = coords->y;
  pickable = GIMP_PICKABLE (gimp_image_get_projection (image));

  gimp_pickable_flush (pickable);
  this->src_type  = gimp_pickable_get_image_type (pickable);
  this->has_alpha = GIMP_IMAGE_TYPE_HAS_ALPHA (this->src_type);
  this->bytes     = GIMP_IMAGE_TYPE_BYTES (this->src_type);

  this->tiles     = tile_manager_duplicate(gimp_pickable_get_tiles (pickable));
  if (GIMP_IS_DRAWABLE(pickable)) {
    gimp_item_get_offset (GIMP_ITEM(pickable), &this->off_x, &this->off_y);
    x += this->off_x;
    y += this->off_y;
  } else {
    this->off_x = 0;
    this->off_y = 0;
  }

  if (GIMP_IS_DRAWABLE(drawable.ptr())) {
    gint off_x, off_y;
    drawable [gimp_item_get_offset] (&off_x, &off_y);
    x += off_x - this->off_x;
    y += off_y - this->off_y;
  }

  tile = tile_manager_get_tile (this->tiles, x, y, TRUE, FALSE);
  if (tile) {
    const guchar *start;
    start = (const guchar*)tile_data_pointer (tile, x, y);
    if (GIMP_IMAGE_TYPE_IS_INDEXED (this->src_type)) {
        gimp_image_get_color (image, this->src_type, start, this->start_col);
    } else {
      for (gint i = 0; i < this->bytes; i++)
        this->start_col[i] = start[i];
    }
    tile_release (tile, FALSE);
  } else {
    for (gint i = 0; i < this->bytes; i++)
      this->start_col[i] = 0;
  }

  return TRUE;
}


void
Brush::motion (GimpDrawable     *_drawable,
               GimpPaintOptions *_paint_options,
               const GimpCoords *coords)
{
  auto paint_core    = ref(GIMP_PAINT_CORE(g_object));
  auto drawable      = ref(_drawable);
  auto paint_options = ref(_paint_options);
  auto options           = OptionsClass::get_private (paint_options.ptr());
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
  if (options->eraser_mode) {
    gimp_image_get_background (image, context, gimp_drawable_type (drawable), col);

  } else {
    gimp_image_get_foreground (image, context, gimp_drawable_type (drawable), col);
    
  }

  col[area->bytes - 1] = OPAQUE_OPACITY;
  color_pixels (temp_buf_get_data (area), col,
              area->width * area->height,
              area->bytes);

  gimp_brush_core_eval_transform_dynamics (brush_core, drawable, paint_options, coords);


  // Adjust the brush scaling.

  TempBuf *mask = brush_core->main_brush->mask;
  brush_core->scale = MAX (0.5 / (gfloat) MIN (mask->width, mask->height), brush_core->scale);

  gint brush_width,brush_height;
  ref(brush_core->brush) [gimp_brush_transform_size] (brush_core->scale, brush_core->aspect_ratio, brush_core->angle, &brush_width, &brush_height);


  //  adjust the x and y coordinates to the upper left corner of the brush
  gint offx, offy;
  drawable [gimp_item_get_offset] (&offx, &offy);
  offx -= this->off_x;
  offy -= this->off_y;

  gint x = (gint) floor (coords->x) - (brush_width  / 2);
  gint y = (gint) floor (coords->y) - (brush_height / 2);

  gint drawable_width  = MIN(drawable [gimp_item_get_width]  (), tile_manager_width (tiles) - offx);
  gint drawable_height = MIN(drawable [gimp_item_get_height] (), tile_manager_height (tiles) - offy);
  gint drawable_minx   = MAX(-offx, 0);
  gint drawable_miny   = MAX(-offy, 0);

  gint x1 = CLAMP (x, drawable_minx, drawable_width);
  gint y1 = CLAMP (y, drawable_miny, drawable_height);
  gint x2 = CLAMP (x + brush_width, drawable_minx, drawable_width);
  gint y2 = CLAMP (y + brush_height, drawable_miny, drawable_height);


  hardness = hardness_output [gimp_dynamics_output_get_linear_value] (coords, paint_options, fade_point);
  force    = hardness_output [gimp_dynamics_output_get_linear_value] (coords, paint_options, fade_point);
  
  Object<GimpChannel> channel     = hold(gimp_channel_new_mask (image, 
                                                                tile_manager_width  (this->tiles), 
                                                                tile_manager_height (this->tiles)));
  TileManager*        channel_buf = ref(channel) [gimp_drawable_get_tiles] ();
  TempBuf*            brush_mask  = (TempBuf*)brush_core [gimp_brush_core_get_brush_mask] (coords, paint_options [gimp_paint_options_get_brush_mode] (), force);
  PixelRegion         pr_ch, pr_br;

  pixel_region_init          (&pr_ch, channel_buf, x1 + offx, y1 + offy, x2 - x1, y2 - y1, TRUE);
  pixel_region_init_temp_buf (&pr_br, brush_mask, 
                              x1 == drawable_minx ? brush_width - (x2 - drawable_minx): 0, 
                              y1 == drawable_miny ? brush_height - (y2 - drawable_miny): 0, x2 - x1, y2 - y1);
  copy_region (&pr_br, &pr_ch);
  
  PixelRegion    srcPR, src_mask_PR;

  channel->x1 = x1 + offx;
  channel->x2 = x2 + offx;
  channel->y1 = y1 + offy;
  channel->y2 = y2 + offy;
  
  pixel_region_init (&src_mask_PR, gimp_drawable_get_tiles (GIMP_DRAWABLE (channel.ptr())), x1 + offx, y1 + offy, x2 - x1, y2 - y1, TRUE);
  pixel_region_init (&srcPR, this->tiles, x1 + offx, y1 + offy, x2 - x1, y2 - y1, FALSE);

  Object<GimpChannel> ch_mask = hold(gimp_image_contiguous_region_by_seed_full (image, 
      &srcPR, this->off_x, this->off_y, &src_mask_PR, this->off_x, this->off_y, 
      x1 + offx, y1 + offy, x2 + offx, y2 + offy, 
      this->src_type, this->has_alpha, this->bytes,
      TRUE, 30, TRUE, GIMP_SELECT_CRITERION_COMPOSITE, coords->x + offx, coords->y + offy, this->start_col));

  ref(ch_mask) [gimp_channel_grow] (1, 1, FALSE);

  pixel_region_init (&pr_ch, ref(ch_mask) [gimp_drawable_get_tiles](), x1 + offx, y1 + offy, x2 - x1, y2 - y1, FALSE);

  TempBuf* old_canvas_buf;
  if ((x2 - x1) && (y2 - y1)) {
        old_canvas_buf         = paint_core->canvas_buf;
        paint_core->canvas_buf = temp_buf_subwindow (paint_core->canvas_buf,
                                                     x1, y1,
                                                     (x2 - x1), (y2 - y1));
  }

  paint_core [gimp_paint_core_paste] (&pr_ch, drawable, MIN (opacity, GIMP_OPACITY_OPAQUE), 
      gimp_context_get_opacity (context), options->eraser_mode? GIMP_ERASE_MODE:gimp_context_get_paint_mode (context), GIMP_PAINT_CONSTANT);

  if (old_canvas_buf)
     paint_core->canvas_buf = old_canvas_buf;
}



static void
gimp_bucket_fill_brush_register (Gimp *gimp)
{
  g_print("BucketFillBrush::register\n");
  GimpPaintInfo* paint_info;
  paint_info = gimp_paint_info_new (gimp,
                                    brush_class.type(),
                                    options_class.type(),
                                    "gimp-bucket-fill-brush",
                                    _("BucketFillBrush"),
                                    "gimp-tool-bucket-fill-brush");
  gimp_paint_register (gimp, paint_info);
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

  gimp_bucket_fill_brush_register(gimp);

  tool_info = gimp_tool_info_new (gimp,
                                  GIMP_TYPE_BUCKET_FILL_BRUSH_TOOL,
                                  options_class.type(),
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

  gimp_paint_tool_enable_color_picker (paint_tool,
                                       GIMP_COLOR_PICK_MODE_FOREGROUND);
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
  GtkWidget *button;
  GList     *children;

  /*  the rate scale  */
  scale = gimp_prop_spin_scale_new (config, "rate",
                                    _("Rate"),
                                    1.0, 10.0, 1);
  gtk_box_pack_start (GTK_BOX (vbox), scale, FALSE, FALSE, 0);
  gtk_widget_show (scale);
  button = gimp_prop_check_button_new (config, "eraser-mode", _("Eraser mode"));
  gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
  gtk_widget_show (button);

  children = gtk_container_get_children (GTK_CONTAINER (vbox));  
  gimp_tool_options_setup_popup_layout (children, FALSE);

  return vbox;
}
