/* This file is part of MyPaint.
 * Copyright (C) 2008-2011 by Martin Renold <martinxyz@gmx.ch>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __MYPAINT_BRUSHMODES_HPP__
#define __MYPAINT_BRUSHMODES_HPP__

#include "../../painter/gimp-painter-visibility.h"
#define REAL_CALC
#include "legacy-pixel.hpp"
namespace GimpPainter GIMP_PAINTER_PRIVATE { namespace MyPaint { namespace LegacyPixel {

struct BrushPixelIteratorForRunLength {
  Pixel::real*   mask;
  gint*          offsets;
  Pixel::real*   colors;
  Pixel::data_t* src;
  Pixel::data_t* dest;
  gint           src_bytes;
  gint           dest_bytes;
  
  BrushPixelIteratorForRunLength(Pixel::real* mask_, 
                                 gint* offsets_, 
                                 Pixel::real* colors_,
                                 Pixel::data_t* src_, 
                                 Pixel::data_t* dest_, 
                                 gint src_bytes_,
                                 gint dest_bytes_) {
    mask       = mask_;
    offsets    = offsets_;
    colors     = colors_;
    src        = src_;
    dest       = dest_;
    src_bytes  = src_bytes_;
    dest_bytes = dest_bytes_;
  }
  
  bool is_row_end() { return !mask[0]; };
  bool is_data_end() { return !offsets[0]; };
  void next_pixel() { mask++; src+=src_bytes; dest += dest_bytes; };
  void next_row() {
    src  += offsets[0] * src_bytes;
    dest += offsets[0] * dest_bytes;
    offsets ++;
    mask ++;
  };
  bool should_skipped() { return false; }
  
  Pixel::real  get_brush_alpha() { return mask[0]; }
  Pixel::real* get_brush_color() { return colors; }
};

struct ColoredBrushmarkIterator {
  typedef Pixel::real mask_t;
  typedef Pixel::real color_t;
  mask_t*              mask;
  gint                 width;
  gint                 height;
  gint                 mask_stride;
  mask_t*              row_guard;
  mask_t*              data_guard;
  color_t*             colors;
  int x, y;
  
  ColoredBrushmarkIterator(mask_t* mask_,
                           color_t* colors_,
                           gint width_,
                           gint height_,
                           gint mask_stride_,
                           gint /*unused*/
                          ) 
    : mask(mask_), width(width_), height(height_), mask_stride(mask_stride_),
      colors(colors_)
  {
    row_guard  = mask + width;
    data_guard = mask_ + height * mask_stride;
    colors     = colors_;
    x = y = 0;
  };
  
  void next_pixel() {
    mask++;
    x++;
  };

  void next_row() {
    y ++; x = 0;
    mask += mask_stride - width;
    row_guard = mask + width;
  };

  bool is_row_end() { return mask == row_guard; };
  bool is_data_end() { return (mask + mask_stride - width) == data_guard; };
  
  mask_t   get_brush_alpha() { return mask[0]; };
  color_t* get_brush_color() { return colors; };
  bool should_skipped() { return !mask[0]; }

};

struct PixmapBrushmarkIterator {
  typedef Pixel::data_t mask_t;
  typedef Pixel::data_t color_t;
  mask_t*        mask;
  gint           width;
  gint           height;
  gint           mask_stride;
  gint           mask_bytes;
  mask_t*        row_guard;
  mask_t*        data_guard;
  int x,y;
  
  PixmapBrushmarkIterator(mask_t* mask_,
                          color_t* /*unused*/,
                          gint width_,
                          gint height_,
                          gint mask_stride_,
                          gint mask_bytes_
                          ) 
    : mask(mask_),
      width(width_), height(height_),
      mask_stride(mask_stride_), mask_bytes(mask_bytes_)
  {
    row_guard  = mask + width  * mask_bytes;
    data_guard = mask + height * mask_stride;
    x = y = 0;
  };
  
  bool is_row_end() { return mask == row_guard; };
  bool is_data_end() { return (mask + mask_stride - width) >= data_guard; };
  
  void next_pixel() {
    mask   += mask_bytes;
    x ++;
  };

  void next_row() {
    y++; x = 0;
    mask      += mask_stride - width * mask_bytes;
    row_guard = mask + width * mask_bytes;
  };

  mask_t   get_brush_alpha() { return mask[3]; };
  color_t* get_brush_color() { return mask; };
  bool should_skipped() { return !mask[3]; }

};

template<typename ParentClass, typename Mask, typename Color>
struct BrushPixelIteratorForPlainData : ParentClass {
  typedef Mask mask_t;
  typedef Color color_t;
  Pixel::data_t* src;
  Pixel::data_t* dest;
  gint           src_stride;
  gint           dest_stride;
  gint           src_bytes;
  gint           dest_bytes;
  Pixel::data_t* dest_guard;
  
  BrushPixelIteratorForPlainData(mask_t*  mask_,
                                 color_t* colors_,
                                 Pixel::data_t* src_,
                                 Pixel::data_t* dest_,
                                 gint width_, gint height_, 
                                 gint mask_stride_,
                                 gint src_stride_,
                                 gint dest_stride_,
                                 gint mask_bytes_,
                                 gint src_bytes_,
                                 gint dest_bytes_) 
    : ParentClass(mask_, colors_, width_, height_, mask_stride_, mask_bytes_)
  {
    src         = src_;
    dest        = dest_;
    src_stride  = src_stride_;
    dest_stride = dest_stride_;
    src_bytes   = src_bytes_;
    dest_bytes  = dest_bytes_;
    dest_guard  = dest + (((ParentClass*)this)->height - 1) * dest_stride + ((ParentClass*)this)->width * dest_bytes - 1;
  }
  
  void next_pixel() {
    ParentClass::next_pixel();
    src  += src_bytes; 
    dest += dest_bytes;
  };

  void next_row() {
    ParentClass::next_row();
    src  += src_stride - ((ParentClass*)this)->width * src_bytes;
    dest += dest_stride - ((ParentClass*)this)->width * dest_bytes;
  };


};

// parameters to those methods:
//
// rgba: A pointer to 16bit rgba data with premultiplied alpha.
//       The range of each components is limited from 0 to 2^15.
//
// mask: Contains the dab shape, that is, the intensity of the dab at
//       each pixel. Usually rendering is done for one tile at a
//       time. The mask is LRE encoded to jump quickly over regions
//       that are not affected by the dab.
//
// opacity: overall strenght of the blending mode. Has the same
//          influence on the dab as the values inside the mask.


// We are manipulating pixels with premultiplied alpha directly.
// This is an "over" operation (opa = topAlpha).
// In the formula below, topColor is assumed to be premultiplied.
//
//               opa_a      <   opa_b      >
// resultAlpha = topAlpha + (1.0 - topAlpha) * bottomAlpha
// resultColor = topColor + (1.0 - topAlpha) * bottomColor
//
template<typename Iter>
void draw_dab_pixels_BlendMode_Normal (Iter iter,
                                       Pixel::real   opacity) 
{
  switch (iter.src_bytes) {
  case 1:
    while (1) {
      for (; !iter.is_row_end(); iter.next_pixel()) {
        if (iter.should_skipped())
          continue;
        pixel_t opa_a = pix( eval( pix(iter.get_brush_alpha()) * pix(opacity) ) ); // topAlpha
        pixel_t opa_b = pix( eval (pix(1.0f) - opa_a ) ); // bottomAlpha
        iter.dest[0] = r2d(eval( opa_a*pix(iter.get_brush_color()[0]) + opa_b*pix(iter.src[0]) ));
      }
      if (iter.is_data_end()) break;
      iter.next_row();
    }
    break;
  case 3:
    while (1) {
      for (; !iter.is_row_end(); iter.next_pixel()) {
        if (iter.should_skipped())
          continue;
        pixel_t opa_a = pix( eval( pix(iter.get_brush_alpha()) * pix(opacity) ) ); // topAlpha
        pixel_t opa_b = pix( eval (pix(1.0f) - opa_a ) ); // bottomAlpha
	result_t internal[3];
	internal[0] = eval( opa_a*pix(iter.get_brush_color()[0]) + opa_b*pix(iter.src[0]) );
	internal[1] = eval( opa_a*pix(iter.get_brush_color()[1]) + opa_b*pix(iter.src[1]) );
	internal[2] = eval( opa_a*pix(iter.get_brush_color()[2]) + opa_b*pix(iter.src[2]) );
        iter.dest[0] = r2d(internal[0]);
        iter.dest[1] = r2d(internal[1]);
        iter.dest[2] = r2d(internal[2]);
      }
      if (iter.is_data_end()) break;
      iter.next_row();
    }
    break;
  case 4:
    while (1) {
      for (; !iter.is_row_end(); iter.next_pixel()) {
        if (iter.should_skipped())
          continue;

        pixel_t brush_a = pix( eval( pix(iter.get_brush_alpha()) * pix(opacity) ) );
        pixel_t base_a  = pix(iter.src[3]);
	result_t alpha  = eval( brush_a + (pix(1.0f) - brush_a) * base_a );
        iter.dest[3] = r2d(alpha);
        if (alpha) {
          pixel_t dest_a = pix(alpha);
	  result_t internal[3];
	  internal[0] = eval( (brush_a * pix(iter.get_brush_color()[0]) + 
                              (pix(1.0f) - brush_a) * base_a * pix(iter.src[0])) 
                              / dest_a );
	  internal[1] = eval( (brush_a * pix(iter.get_brush_color()[1]) + 
                              (pix(1.0f) - brush_a) * base_a * pix(iter.src[1])) 
                              / dest_a );
	  internal[2] = eval( (brush_a * pix(iter.get_brush_color()[2]) + 
                              (pix(1.0f) - brush_a) * base_a * pix(iter.src[2])) 
                              / dest_a );
          iter.dest[0] = r2d(internal[0]);
          iter.dest[1] = r2d(internal[1]);
          iter.dest[2] = r2d(internal[2]);
        } else {
          iter.dest[0] = iter.get_brush_color()[0];
          iter.dest[1] = iter.get_brush_color()[1];
          iter.dest[2] = iter.get_brush_color()[2];
        }
      }
      if (iter.is_data_end()) break;
      iter.next_row();
    }
    break;
  default:
    g_print("Unsupported layer type: %d.\n", iter.src_bytes);
  }
};

// This blend mode is used for smudging and erasing.  Smudging
// allows to "drag" around transparency as if it was a color.  When
// smuding over a region that is 60% opaque the result will stay 60%
// opaque (color_a=0.6).  For normal erasing color_a is set to 0.0
// and color_r/g/b will be ignored. This function can also do normal
// blending (color_a=1.0).
//
template<typename Iter>
void draw_dab_pixels_BlendMode_Normal_and_Eraser (Iter iter,
                                                  Pixel::real   color_a,
                                                  Pixel::real   opacity,
                                                  Pixel::real   background_r = 1.0f,
                                                  Pixel::real   background_g = 1.0f,
                                                  Pixel::real   background_b = 1.0f) {
//  g_print("BlendMode_Normal_and_Eraser(color_a=%d)\n", color_a);
  Pixel::real bg_color[3];
  bg_color[0] = background_r;
  bg_color[1] = background_g;
  bg_color[2] = background_b;

  switch (iter.src_bytes) {
  case 1:
    while (1) {
      for (; !iter.is_row_end(); iter.next_pixel()) {

        if (iter.should_skipped())
          continue;

        pixel_t brush_a     = pix( eval( pix(iter.get_brush_alpha()) * pix(opacity) ) ); // topAlpha
        pixel_t inv_brush_a = pix( eval (f2p(1.0) - brush_a ) ); // bottomAlpha

        iter.dest[0] = r2d(eval( brush_a*((pix(1.0f) - pix(color_a))*pix(bg_color[0]) 
                                          + pix(color_a)*pix(iter.get_brush_color()[0])) 
                                 + inv_brush_a*pix(iter.src[0]) ));
      }
      if (iter.is_data_end()) break;
      iter.next_row();
    }
    break;  
  case 3:
    while (1) {
      for (; !iter.is_row_end(); iter.next_pixel()) {

        if (iter.should_skipped())
          continue;

        pixel_t brush_a     = pix( eval( pix(iter.get_brush_alpha()) * pix(opacity) ) ); // topAlpha
        pixel_t inv_brush_a = pix( eval (f2p(1.0) - brush_a ) ); // bottomAlpha
	result_t internal[3];

	internal[0] = eval( brush_a*((pix(1.0f) - pix(color_a))*pix(bg_color[0]) 
                            + pix(color_a)*pix(iter.get_brush_color()[0])) 
                            + inv_brush_a*pix(iter.src[0]) );
	internal[1] = eval( brush_a*((pix(1.0f) - pix(color_a))*pix(bg_color[1]) 
                            + pix(color_a)*pix(iter.get_brush_color()[1])) 
                            + inv_brush_a*pix(iter.src[1]) );
	internal[2] = eval( brush_a*((pix(1.0f) - pix(color_a))*pix(bg_color[2]) 
                            + pix(color_a)*pix(iter.get_brush_color()[2])) 
                            + inv_brush_a*pix(iter.src[2]) );

        iter.dest[0] = r2d(internal[0]);
        iter.dest[1] = r2d(internal[1]);
        iter.dest[2] = r2d(internal[2]);
      }
      if (iter.is_data_end()) break;
      iter.next_row();
    }
    break;
  case 4:
    while (1) {
      for (; !iter.is_row_end(); iter.next_pixel()) {

        if (iter.should_skipped())
          continue;

        pixel_t brush_a = pix( eval( pix(iter.get_brush_alpha()) * pix(opacity) ) );
        pixel_t base_a  = pix(iter.src[3]);
	      result_t alpha = eval( brush_a * pix(color_a) + (pix(1.0f) - brush_a) * base_a );
        iter.dest[3] = r2d(alpha);
        
        if (alpha) {
          pixel_t inv_brush_a = pix( eval(pix(1.0f) - brush_a));
          pixel_t dest_a = pix(alpha);
	  result_t internal[3];

	  internal[0] = eval( (inv_brush_a*base_a*pix(iter.src[0]) + brush_a*pix(color_a)*pix(iter.get_brush_color()[0])) / dest_a );
	  internal[1] = eval( (inv_brush_a*base_a*pix(iter.src[1]) + brush_a*pix(color_a)*pix(iter.get_brush_color()[1])) / dest_a );
	  internal[2] = eval( (inv_brush_a*base_a*pix(iter.src[2]) + brush_a*pix(color_a)*pix(iter.get_brush_color()[2])) / dest_a );

          iter.dest[0] = r2d(internal[0]);
          iter.dest[1] = r2d(internal[1]);
          iter.dest[2] = r2d(internal[2]);
        } else {
          iter.dest[0] = iter.get_brush_color()[0];
          iter.dest[1] = iter.get_brush_color()[1];
          iter.dest[2] = iter.get_brush_color()[2];
        }
      }
      if (iter.is_data_end()) break;
      iter.next_row();
    }
    break;
  default:
    g_print("Unsupported layer type.\n");
  }
};

// This is BlendMode_Normal with locked alpha channel.
//
template<typename Iter>
void draw_dab_pixels_BlendMode_LockAlpha (Iter          iter,
                                          Pixel::real   opacity)
{
  switch (iter.src_bytes) {
  case 1:
  case 3:
    draw_dab_pixels_BlendMode_Normal(iter, opacity);
    break;
  case 4:

    while (1) {
      for (; !iter.is_row_end(); iter.next_pixel()) {

        if (iter.should_skipped())
          continue;

        // The pinned x86 runtime writes hidden black for zero-alpha input.
        // Express that measured result without its undefined NaN-to-byte cast.
        if (!iter.src[3]) {
          iter.dest[0] = iter.dest[1] = iter.dest[2] = 0;
          continue;
        }
        pixel_t brush_a = pix( eval( pix(iter.get_brush_alpha()) * pix(opacity) ) ); // topAlpha
        pixel_t inv_brush_a = pix( eval( f2p(1.0) - brush_a) ); // bottomAlpha
        pixel_t alpha = pix( iter.src[3] );
        pixel_t dest_a = pix( iter.src[3] );
        pixel_t base_a = dest_a;
	result_t internal[3];
	
	internal[0] = eval( (brush_a*alpha*pix(iter.get_brush_color()[0]) + inv_brush_a*base_a*pix(iter.src[0])) / dest_a);
	internal[1] = eval( (brush_a*alpha*pix(iter.get_brush_color()[1]) + inv_brush_a*base_a*pix(iter.src[1])) / dest_a);
	internal[2] = eval( (brush_a*alpha*pix(iter.get_brush_color()[2]) + inv_brush_a*base_a*pix(iter.src[2])) / dest_a);
        
        iter.dest[0] = r2d(internal[0]);
        iter.dest[1] = r2d(internal[1]);
        iter.dest[2] = r2d(internal[2]);
      }
      if (iter.is_data_end()) break;
      iter.next_row();
    }
  break;
  default:
    g_print("Unsupported layer type.\n");
  }
};


// Sum up the color/alpha components inside the masked region.
// Called by get_color().
//
template<typename Iter>
void get_color_pixels_accumulate (/*Pixel::real  * mask,
                                  gint          *offsets,
                                  Pixel::data_t * rgba,*/
                                  Iter iter,
                                  float * sum_weight,
                                  float * sum_r,
                                  float * sum_g,
                                  float * sum_b,
                                  float * sum_a/*,
                                  gint bytes*/
                                  ) {


  // The sum of a 64x64 tile fits into a 32 bit integer, but the sum
  // of an arbitrary number of tiles may not fit. We assume that we
  // are processing a single tile at a time, so we can use integers.
  // But for the result we need floats.

  internal_t weight = 0;
  internal_t r = 0;
  internal_t g = 0;
  internal_t b = 0;
  internal_t a = 0;

  switch (iter.src_bytes) {
  case 1:
    while (1) {
      for (; !iter.is_row_end(); iter.next_pixel()) {

        if (iter.should_skipped())
          continue;

        pixel_t opa = pix (iter.get_brush_alpha());
        weight += r2i(eval(opa));
        r      += r2i(eval (opa * pix(iter.src[0]) * pix(1.0f)));
        a      += r2i(eval(opa));
      }
      if (iter.is_data_end()) break;
      iter.next_row();
    }
      g = b = r;
    break;
  case 3:
    while (1) {
      for (; !iter.is_row_end(); iter.next_pixel()) {

        if (iter.should_skipped())
          continue;

        pixel_t opa = pix (iter.get_brush_alpha());
	result_t internal[3];

	internal[0] = eval (opa * pix(iter.src[0]) * pix(1.0f));
	internal[1] = eval (opa * pix(iter.src[1]) * pix(1.0f));
	internal[2] = eval (opa * pix(iter.src[2]) * pix(1.0f));

        weight += r2i(eval(opa));
        r      += r2i(internal[0]);
        g      += r2i(internal[1]);
        b      += r2i(internal[2]);
        a      += r2i(eval(opa));
      }
      if (iter.is_data_end()) break;
      iter.next_row();
    }
    break;
  case 4:
    while (1) {
      for (; !iter.is_row_end(); iter.next_pixel()) {

        if (iter.should_skipped())
          continue;

        pixel_t opa = pix (iter.get_brush_alpha());
	pixel_t alpha = pix(iter.src[3]);
	result_t internal[3];

	internal[0] = eval (opa * pix(iter.src[0]) * alpha);
	internal[1] = eval (opa * pix(iter.src[1]) * alpha);
	internal[2] = eval (opa * pix(iter.src[2]) * alpha);

        weight += r2i(eval(opa));
        r      += r2i(internal[0]);
        g      += r2i(internal[1]);
        b      += r2i(internal[2]);
        a      += r2i(eval (opa * alpha));
      }
      if (iter.is_data_end()) break;
      iter.next_row();
    }
    break;
  default:
    g_print("Unsupported layer type.\n");
  }

  // convert integer to float outside the performance critical loop
  *sum_weight += weight;
  *sum_r += r;
  *sum_g += g;
  *sum_b += b;
  *sum_a += a;
};

#if 0
// Colorize: apply the source hue and saturation, retaining the target
// brightness. Same thing as in the PDF spec addendum but with different Luma
// coefficients. Colorize should be used at either 1.0 or 0.0, values in
// between probably aren't very useful. This blend mode retains the target
// alpha, and any pure whites and blacks in the target layer.

#define MAX3(a, b, c) ((a)>(b)?MAX((a),(c)):MAX((b),(c)))
#define MIN3(a, b, c) ((a)<(b)?MIN((a),(c)):MIN((b),(c)))

/*
// From ITU Rec. BT.601 (SDTV)
static const float LUMA_RED_COEFF   = 0.3;
static const float LUMA_GREEN_COEFF = 0.59;
static const float LUMA_BLUE_COEFF  = 0.11;
*/

// From ITU Rec. BT.709 (HDTV and sRGB)
static const uint16_t LUMA_RED_COEFF = 0.2126 * (1<<15);
static const uint16_t LUMA_GREEN_COEFF = 0.7152 * (1<<15);
static const uint16_t LUMA_BLUE_COEFF = 0.0722 * (1<<15);

// See also http://en.wikipedia.org/wiki/YCbCr


/* Returns the sRGB luminance of an RGB triple, expressed as scaled ints. */

#define LUMA(r,g,b) \
   ((r)*LUMA_RED_COEFF + (g)*LUMA_GREEN_COEFF + (b)*LUMA_BLUE_COEFF)


/*
 * Sets the output RGB triple's luminance to that of the input, retaining its
 * colour. Inputs and outputs are scaled ints having factor 2**-15, and must
 * not store premultiplied alpha.
 */

inline static void
set_rgb16_lum_from_rgb16(const uint16_t topr,
                         const uint16_t topg,
                         const uint16_t topb,
                         uint16_t *botr,
                         uint16_t *botg,
                         uint16_t *botb)
{
    // Spec: SetLum()
    // Colours potentially can go out of band to both sides, hence the
    // temporary representation inflation.
    const uint16_t botlum = LUMA(*botr, *botg, *botb) / (1<<15);
    const uint16_t toplum = LUMA(topr, topg, topb) / (1<<15);
    const int16_t diff = botlum - toplum;
    int32_t r = topr + diff;
    int32_t g = topg + diff;
    int32_t b = topb + diff;

    // Spec: ClipColor()
    // Clip out of band values
    int32_t lum = LUMA(r, g, b) / (1<<15);
    int32_t cmin = MIN3(r, g, b);
    int32_t cmax = MAX3(r, g, b);
    if (cmin < 0) {
        r = lum + (((r - lum) * lum) / (lum - cmin));
        g = lum + (((g - lum) * lum) / (lum - cmin));
        b = lum + (((b - lum) * lum) / (lum - cmin));
    }
    if (cmax > (1<<15)) {
        r = lum + (((r - lum) * ((1<<15)-lum)) / (cmax - lum));
        g = lum + (((g - lum) * ((1<<15)-lum)) / (cmax - lum));
        b = lum + (((b - lum) * ((1<<15)-lum)) / (cmax - lum));
    }
#ifdef HEAVY_DEBUG
    assert((0 <= r) && (r <= (1<<15)));
    assert((0 <= g) && (g <= (1<<15)));
    assert((0 <= b) && (b <= (1<<15)));
#endif

    *botr = r;
    *botg = g;
    *botb = b;
}


// The method is an implementation of that described in the official Adobe "PDF
// Blend Modes: Addendum" document, dated January 23, 2006; specifically it's
// the "Color" nonseparable blend mode. We do however use different
// coefficients for the Luma value.

void
draw_dab_pixels_BlendMode_Color (uint16_t *mask,
                                 uint16_t *rgba, // b=bottom, premult
                                 uint16_t color_r,  // }
                                 uint16_t color_g,  // }-- a=top, !premult
                                 uint16_t color_b,  // }
                                 uint16_t opacity)
{
  while (1) {
    for (; mask[0]; mask++, rgba+=4) {
      // De-premult
      uint16_t r, g, b;
      const uint16_t a = rgba[3];
      r = g = b = 0;
      if (rgba[3] != 0) {
        r = ((1<<15)*((uint32_t)rgba[0])) / a;
        g = ((1<<15)*((uint32_t)rgba[1])) / a;
        b = ((1<<15)*((uint32_t)rgba[2])) / a;
      }

      // Apply luminance
      set_rgb16_lum_from_rgb16(color_r, color_g, color_b, &r, &g, &b);

      // Re-premult
      r = ((uint32_t) r) * a / (1<<15);
      g = ((uint32_t) g) * a / (1<<15);
      b = ((uint32_t) b) * a / (1<<15);

      // And combine as normal.
      uint32_t opa_a = mask[0] * opacity / (1<<15); // topAlpha
      uint32_t opa_b = (1<<15) - opa_a; // bottomAlpha
      rgba[0] = (opa_a*r + opa_b*rgba[0])/(1<<15);
      rgba[1] = (opa_a*g + opa_b*rgba[1])/(1<<15);
      rgba[2] = (opa_a*b + opa_b*rgba[2])/(1<<15);
    }
    if (!mask[1]) break;
    rgba += mask[1];
    mask += 2;
  }
};
#endif

} } }
#undef REAL_CALC
#endif
