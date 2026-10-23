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

extern "C" {
#include "config.h"

#include <string.h>
#include <math.h>

#include <gegl.h>

#include "libgimpbase/gimpbase.h"
#include "libgimpmath/gimpmath.h"

#include "paint-types.h"

#include "base/base-types.h"
#include "base/pixel-region.h"
#include "base/temp-buf.h"
#include "base/tile-manager.h"
#include "base/tile.h"
#include "base/pixel-processor.h"

#include "core/gimp.h"
#include "core/gimp-utils.h"
#include "core/gimpdrawable.h"
#include "core/gimpimage.h"
#include "core/gimpimage-undo.h"
#include "core/gimpimage-undo-push.h"
#include "core/gimpchannel.h"
#include "core/gimppattern.h"
#include "core/gimppickable.h"
#include "core/gimpprojection.h"

#include "gimpmypaintcore.hpp"
#include "gimpmypaintcoreundo.h"
#include "gimpmypaintoptions.h"

#include "gimp-intl.h"
#include "paint-funcs/paint-funcs.h"
};

#define REAL_CALC

#include "base/pixel.hpp"
#include "base/scopeguard.hpp"
#include "paint-funcs/mypaint-brushmodes.hpp"
#include "gimpmypaintcore-surface.hpp"
#include "base/delegators.hpp"
#include "base/glib-cxx-utils.hpp"
#include "paint/gimpmypaintcore-brushfeature.hpp"
#include "paint/gimpmypaintcore-drawablefeature.hpp"

////////////////////////////////////////////////////////////////////////////////
template<typename BrushFeature>
struct ParallelProcessor {
  template<typename... Args>
  struct Member {
    typedef void (BrushFeature::*Signature)(Args...);
    template<Signature func>
    class Processor {
    public:

      static void process_member(BrushFeature* impl, Args... args) {
        (impl->*func)(args...);
      }

      void operator()(BrushFeature* impl, Args... args) {
        pixel_regions_process_parallel((PixelProcessorFunc)Processor::process_member,
                                       impl, sizeof...(args), args...);
      };
    };
  };
};

template<typename BrushFeature>
struct Processors {
private:
  typedef ParallelProcessor<BrushFeature> P;
  typedef typename P::template Member<PixelRegion*,PixelRegion*,PixelRegion*,PixelRegion*,PixelRegion*> M5;
  typedef typename P::template Member<PixelRegion*,PixelRegion*,PixelRegion*,PixelRegion*> M4;
  typedef typename M5::Signature M5S;
  typedef typename M4::Signature M4S;
public:
  static typename M5::
    template Processor<reinterpret_cast<M5S>(&BrushFeature::draw_dab)> 
    draw_dab;

  static typename M5::
    template Processor<reinterpret_cast<M5S>(&BrushFeature::copy_stroke)> 
    copy_stroke;

  static typename M4::
    template Processor<reinterpret_cast<M4S>(&BrushFeature::get_color)> 
    get_color;
};

////////////////////////////////////////////////////////////////////////////////
template<class DrawableFeature>
class GimpMypaintSurfaceImpl : public GimpMypaintSurface
{
private:
  DrawableFeature drawable_feature;
  GimpRGB       bg_color;
  GimpBrush*    brushmark;
  GimpPattern*  texture;
  GimpCoords    last_coords;
  GimpCoords    current_coords;
  bool          floating_stroke;
  float         stroke_opacity;
  
  gint          session;          /*  reference counter of atomic scope   */

  void      validate_undo_tiles       (gint              x,
                                       gint              y,
                                       gint              w,
                                       gint              h);

  void      validate_floating_stroke_tiles (gint              x,
                                            gint              y,
                                            gint              w,
                                            gint              h);

  void start_undo_group();
  void stop_undo_group();
  
  void start_floating_stroke();
  void stop_floating_stroke();

  struct Boundary {
    gint offset_x, offset_y;
    int original_x1, original_y1, original_x2, original_y2;
    int rx1, ry1, rx2, ry2;
    int width, height;

    void clamp_within(int bx1, int by1, int bx2, int by2) {
      rx1 = CLAMP (rx1, bx1, bx2);
      ry1 = CLAMP (ry1, by1, by2);
      rx2 = CLAMP (rx2, bx1, bx2);
      ry2 = CLAMP (ry2, by1, by2);      
    }
  };

  template<class BrushFeature>
  bool adjust_boundary(Boundary&  b,
                       BrushFeature* brush_impl,
                       TempBuf* dab_mask) 
  {
    /*  get the layer offsets  */
    drawable_feature.get_drawable_offset(b.offset_x, b.offset_y);

    brush_impl->get_boundary(b.original_x1, b.original_y1, 
                             b.original_x2, b.original_y2);

    b.rx1 = b.original_x1;
    b.ry1 = b.original_y1;
    b.rx2 = b.original_x2;
    b.ry2 = b.original_y2;
    
    if (drawable_feature.has_mask_item()) {
      /*  make sure coordinates are in mask bounds ...
       *  we need to add the layer offset to transform coords
       *  into the mask coordinate system
       */
      b.clamp_within(-b.offset_x, -b.offset_y,
                     drawable_feature.get_mask_width()  - b.offset_x,
                     drawable_feature.get_mask_height() - b.offset_y);
    }
    b.clamp_within(0,0,
                   drawable_feature.get_drawable_width()  - 1,
                   drawable_feature.get_drawable_height() - 1);

    if (dab_mask) {
      if (dab_mask->width < b.rx2 - b.rx1 + 1)
        b.rx2 = b.rx1 + dab_mask->width - 1;
      if (dab_mask->height < b.ry2 - b.ry1 + 1)
        b.ry2 = b.ry1 + dab_mask->height - 1;
    }

    b.width    = (b.rx2 - b.rx1 + 1);
    b.height   = (b.ry2 - b.ry1 + 1);

    if (b.rx1 > b.rx2 || b.ry1 > b.ry2)
      return false;

    if (dab_mask &&
        (dab_mask->width < b.rx1 - b.original_x1 || 
         dab_mask->height < b.ry1 - b.original_y1))
      return false;

    return true;
  };

  void configure_pixel_regions(PixelRegion** src1PR, 
                               PixelRegion** destPR, 
                               PixelRegion** brushPR, 
                               PixelRegion** maskPR, 
                               PixelRegion** texturePR,
                               Boundary     b,
                               bool src_use_floating,
                               bool dest_use_floating,
                               TempBuf*     dab_mask)
  {
    if (src1PR) {
      if (src_use_floating)
        *src1PR = drawable_feature.
          get_floating_stroke_region(b.rx1, b.ry1, b.width, b.height, false);
      else
        *src1PR = drawable_feature.
          get_drawable_region(b.rx1, b.ry1, b.width, b.height, false);
    }
    
    if (destPR) {
      if (dest_use_floating)
        *destPR = drawable_feature.
          get_floating_stroke_region(b.rx1, b.ry1, b.width, b.height, true);
      else
        *destPR = drawable_feature.
          get_drawable_region(b.rx1, b.ry1, b.width, b.height, true);
    }
    
    if (brushPR && dab_mask) {
      *brushPR = g_new(PixelRegion, 1);
      pixel_region_init_temp_buf(*brushPR, dab_mask, 
                                 MAX(b.rx1 - b.original_x1, 0), 
                                 MAX(b.ry1 - b.original_y1, 0), 
                                 b.width, b.height);
    }

    if (maskPR)
      *maskPR = drawable_feature.
        get_mask_region(b.rx1 + b.offset_x, b.ry1 + b.offset_y,
                        b.width, b.height, false);

    if (texturePR && texture) {
      *texturePR = g_new(PixelRegion, 1);
      TempBuf* pattern = NULL;
      pattern = gimp_pattern_get_mask (texture);
      pixel_region_init_temp_buf(*texturePR, pattern,
                                 b.rx1 % pattern->width,
                                 b.ry1 % pattern->height,
                                 pattern->width, pattern->height);
      pixel_region_set_closed_loop(*texturePR, TRUE);
    }
  };

  template<class BrushFeature>
  bool draw_dab_impl (BrushFeature& brush_impl,
                      float x, float y, float radius, 
                      float color_r, float color_g, float color_b,
                      float opaque, float hardness,
                      float color_a,
                      float aspect_ratio, float angle,
                      float lock_alpha,float colorize,
                      float texture_grain = 0.0, float texture_contrast = 1.0)
  {
    PixelRegion     *src1PR, *destPR, *brushPR, *maskPR, *texturePR;
    src1PR = destPR = brushPR = maskPR = texturePR = NULL;

    drawable_feature.refresh();
    
    opaque     = CLAMP(opaque, 0.0, 1.0);
    hardness   = CLAMP(hardness, 0.0, 1.0);
    lock_alpha = CLAMP(lock_alpha, 0.0, 1.0);
    colorize   = CLAMP(colorize, 0.0, 1.0);

    if (radius < 0.1)    return false; // don't bother with dabs smaller than 0.1 pixel
    if (hardness == 0.0) return false; // infintly small center point, fully transparent outside
    if (opaque == 0.0)   return false;

    if (aspect_ratio<1.0) aspect_ratio=1.0;

    // blending mode preparation
    float normal = 1.0;

    normal *= 1.0-lock_alpha;
    normal *= 1.0-colorize;

    Pixel::real fg_color[4] = {color_r, color_g, color_b, color_a };
    Pixel::real bg_color[3] = {Pixel::real(this->bg_color.r),
                               Pixel::real(this->bg_color.g), 
                               Pixel::real(this->bg_color.b) };

    if (!brush_impl.prepare_brush(x, y, radius, 
                                  hardness, aspect_ratio, angle, 
                                  normal, opaque, lock_alpha,
                                  fg_color, color_a, bg_color, stroke_opacity,
                                  texture_grain, texture_contrast,
                                  (void*)brushmark))
      return false;
    TempBuf* dab_mask = (TempBuf*)brush_impl.get_brush_data();

    Boundary b;
    if (!adjust_boundary(b, &brush_impl, dab_mask))
      return false;

    /*  set undo blocks  */
    start_undo_group();
    validate_undo_tiles(b.rx1, b.ry1, b.width, b.height);

    if (floating_stroke)
      validate_floating_stroke_tiles(b.rx1, b.ry1, b.width, b.height);
    configure_pixel_regions(&src1PR, &destPR, &brushPR, &maskPR, &texturePR,
                            b, floating_stroke, floating_stroke,
                            dab_mask);
    
    Processors<BrushFeature>::draw_dab(&brush_impl,
                                       src1PR, destPR, 
                                       brushPR, maskPR, texturePR);

    if (floating_stroke) {
      /* Copy floating stroke buffer into drawable buffer */
      if (src1PR) g_free(src1PR);
      src1PR = drawable_feature.
        get_undo_region(b.rx1, b.ry1, b.width, b.height, false);

      if (destPR) g_free(destPR);
      destPR = drawable_feature.
        get_drawable_region(b.rx1, b.ry1, b.width, b.height, true);

      if (brushPR) g_free(brushPR);
      brushPR = drawable_feature.
        get_floating_stroke_region(b.rx1, b.ry1, b.width, b.height, false);

      Processors<BrushFeature>::copy_stroke(&brush_impl,
                                            src1PR, destPR, brushPR,
                                            (PixelRegion*)NULL, (PixelRegion*)NULL);
    }

    drawable_feature.update_drawable(b.rx1, b.ry1, b.width, b.height);
    if (src1PR) g_free(src1PR);
    if (destPR) g_free(destPR);
    if (brushPR) g_free(brushPR);
    if (maskPR) g_free(maskPR);
    if (texturePR) g_free(texturePR);

    return true;
  }
  
  template<typename BrushFeature>
  void get_color_impl (BrushFeature& brush_impl,
                       float x, float y, float radius, 
                       float * color_r, float * color_g, float * color_b, 
                       float * color_a,
                       float hardness, float aspect_ratio, float angle,
                       float texture_grain, float texture_contrast)
  {
    hardness   = CLAMP(hardness, 0.0, 1.0);

    // in case we return with an error
    *color_r = *color_g = *color_b = *color_a = 0.0;

    if (radius < 1.0)     radius = 1.0;
    if (hardness == 0.0)  return; // infintly small center point, fully transparent outside
    if (aspect_ratio<1.0) aspect_ratio=1.0;

    drawable_feature.refresh();
    PixelRegion     *src1PR, *brushPR, *maskPR, *texturePR;
    src1PR = brushPR = maskPR = texturePR = NULL;
    
    /*  get the layer offsets  */
    Pixel::real fg_color[] = {0.0, 0.0, 0.0, 1.0};
    Pixel::real bg_color[] = {1.0, 1.0, 1.0};
    if (!brush_impl.prepare_brush(x, y, radius, 
                                  hardness, aspect_ratio, angle, 
                                  1.0, 1.0, 0.0,
                                  fg_color, 1.0, bg_color, stroke_opacity,
                                  texture_grain, texture_contrast,
                                  brushmark))
        return;

    TempBuf* dab_mask = (TempBuf*)brush_impl.get_brush_data();

    Boundary b;
    if (!adjust_boundary(b, &brush_impl, dab_mask))
      return;

    configure_pixel_regions(&src1PR, NULL, &brushPR, &maskPR, &texturePR,
                            b, false, false, dab_mask);
    
    // first, we calculate the mask (opacity for each pixel)
    Processors<BrushFeature>::get_color(&brush_impl,
                                     src1PR, brushPR, maskPR, texturePR); 
    brush_impl.get_accumulator()->summarize(color_r, color_g, color_b, color_a);

    if (src1PR) g_free(src1PR);
    if (brushPR) g_free(brushPR);
    if (maskPR) g_free(maskPR);
    if (texturePR) g_free(texturePR);
  }
public:
  GimpMypaintSurfaceImpl(typename DrawableFeature::Drawable d) 
    : session(0), drawable_feature(d), brushmark(NULL), 
      floating_stroke(false), stroke_opacity(1.0), texture(NULL)
  {
  }

  virtual ~GimpMypaintSurfaceImpl()
  {
    if (brushmark)
      g_object_unref(G_OBJECT(brushmark));

    if (texture)
      g_object_unref(G_OBJECT(texture));
  }

  virtual bool is_surface_for (GimpDrawable* drawable) { 
    return drawable_feature.is_surface_for(drawable); 
  }
  void set_bg_color (GimpRGB* src) { 
    if (src) bg_color = *src; 
  }

  void get_bg_color (GimpRGB* dest) {
    if (dest) *dest = bg_color; 
  }

  void set_brushmark(GimpBrush* brush_)
  {
    if (brushmark) {
      gimp_brush_end_use(brushmark);
      g_object_unref(G_OBJECT(brushmark));
      brushmark = NULL;
    }

    if (brush_) {
      brushmark = brush_;
      gimp_brush_begin_use(brushmark);
      g_object_ref(G_OBJECT(brushmark));
    }
  }

  GimpBrush* get_brushmark() {
    return brushmark;
  }

  void set_texture(GimpPattern* texture_)
  {
    if (texture) {
      g_object_unref(G_OBJECT(texture));
      texture = NULL;
    }

    if (texture_) {
      texture = texture_;
      g_object_ref(G_OBJECT(texture));
    }
  }

  GimpPattern* get_texture() {
    return texture;
  }

  void set_floating_stroke(bool value) {
    floating_stroke = value;
  }

  bool get_floating_stroke () {
    return floating_stroke;
  }

  void set_stroke_opacity(double value) {
    stroke_opacity = (float)CLAMP(value, 0.0, 1.0);
  }

  virtual void set_coords(const GimpCoords* coords) { current_coords = *coords; }
  virtual bool draw_dab (float x, float y, float radius, 
                         float color_r, float color_g, float color_b,
                         float opaque, float hardness = 0.5,
                         float alpha_eraser = 1.0,
                         float aspect_ratio = 1.0, float angle = 0.0,
                         float lock_alpha = 0.0, float colorize = 0.0,
                         float texture_grain = 0.0, float texture_contrast = 1.0
                         );

  virtual void get_color (float x, float y, float radius, 
                          float * color_r, float * color_g, float * color_b, float * color_a,
                          float hardness, float aspect_ratio, float angle, 
                          float texture_grain, float texture_contrast
                          );

  virtual void begin_session();
  virtual void end_session();
};



template<class DrawableFeature> bool
GimpMypaintSurfaceImpl<DrawableFeature>::
draw_dab (float x, float y, float radius, 
          float color_r, float color_g, float color_b,
          float opaque, float hardness, float color_a,
          float aspect_ratio, float angle,
          float lock_alpha, float colorize,
          float texture_grain, float texture_contrast)
{
  if (brushmark) {
    GimpBrushFeature brush_impl(&current_coords, &last_coords);
    return draw_dab_impl(brush_impl,
                          x, y, radius, color_r, color_g, color_b, opaque,
                          hardness, color_a, aspect_ratio, angle, lock_alpha,
                          colorize, texture_grain, texture_contrast);
  } else {
    MypaintBrushFeature brush_impl;
    return draw_dab_impl(brush_impl,
                          x, y, radius, color_r, color_g, color_b, opaque,
                          hardness, color_a, aspect_ratio, angle, lock_alpha,
                          colorize, texture_grain, texture_contrast);
  }
}

template<class DrawableFeature> void 
GimpMypaintSurfaceImpl<DrawableFeature>::
get_color (float x, float y, float radius, 
           float * color_r, float * color_g, float * color_b, float * color_a,
           float hardness, float aspect_ratio, float angle, 
           float texture_grain, float texture_contrast)
{
  if (brushmark) {
    GimpBrushFeature brush_impl(&current_coords, &last_coords);
    return get_color_impl(brush_impl,
                          x, y, radius, color_r, color_g, color_b, color_a, 
                          hardness, aspect_ratio, angle, 
                          texture_grain, texture_contrast);
  } else { // DEFAULT_CASE
    MypaintBrushFeature brush_impl;
    return get_color_impl(brush_impl,
                          x, y, radius, color_r, color_g, color_b, color_a, 
                          hardness, aspect_ratio, angle, 
                          texture_grain, texture_contrast);
  }
}

template<class DrawableFeature> void 
GimpMypaintSurfaceImpl<DrawableFeature>::begin_session()
{
  session = 0;
  if (floating_stroke)
    start_floating_stroke();
}

template<class DrawableFeature> void 
GimpMypaintSurfaceImpl<DrawableFeature>::end_session()
{
  if (session <= 0)
    return;
    
  stop_undo_group();
  if (floating_stroke)
    stop_floating_stroke();
  session = 0;
}

template<class DrawableFeature> void 
GimpMypaintSurfaceImpl<DrawableFeature>::start_undo_group()
{
  if (session > 0)
    return;
  g_print("Stroke::start_undo_group...\n");
  session = 1;

  drawable_feature.start_undo_group();
}

template<class DrawableFeature> void 
GimpMypaintSurfaceImpl<DrawableFeature>::stop_undo_group()
{
  g_print("Stroke::stop_undo_group...\n");
  drawable_feature.stop_undo_group();
}

template<class DrawableFeature> void 
GimpMypaintSurfaceImpl<DrawableFeature>::start_floating_stroke()
{
  drawable_feature.start_floating_stroke();
}

template<class DrawableFeature> void 
GimpMypaintSurfaceImpl<DrawableFeature>::stop_floating_stroke()
{
  drawable_feature.stop_floating_stroke();
}

template<class DrawableFeature> void
GimpMypaintSurfaceImpl<DrawableFeature>::
validate_floating_stroke_tiles(gint x, gint y, gint w, gint h)
{
  drawable_feature.validate_floating_stroke_tiles(x, y, w, h);
}

template<class DrawableFeature> void
GimpMypaintSurfaceImpl<DrawableFeature>::
validate_undo_tiles (gint x, gint y, gint w, gint h)
{
  drawable_feature.validate_undo_tiles(x, y, w, h);  
}


GimpMypaintSurface* GimpMypaintSurface_new(GimpDrawable* drawable) {
  return new GimpMypaintSurfaceImpl<GimpImageFeature>(drawable);
}

GimpMypaintSurface* GimpMypaintSurface_TempBuf_new(TempBuf* drawable) {
  return new GimpMypaintSurfaceImpl<GimpTempBufFeature>(drawable);
}
