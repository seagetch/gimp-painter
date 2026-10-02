/* SPDX-License-Identifier: GPL-3.0-or-later
 * Equations adapted from MyPaint, Copyright (C) 2008-2011 Martin Renold.
 * Defined native two-byte extension of the pinned one-channel/RGBA equations.
 * The old code has no two-byte blend cases; this is not an old-runtime oracle.
 */
#ifndef GIMP_PAINTER_GRAY_ALPHA_PIXELS_HPP
#define GIMP_PAINTER_GRAY_ALPHA_PIXELS_HPP
#include "legacy-pixel-modes.hpp"
namespace GimpPainter { namespace MyPaint { namespace GrayAlpha {
using namespace LegacyPixel;

// Same byte-stride iterator, but alpha is at index 1 for a native Y'A brushmark.
struct Pixmap : PixmapBrushmarkIterator {
  using Base=PixmapBrushmarkIterator;
  using Base::Base;
  mask_t get_brush_alpha(){return this->mask[1];}
  bool should_skipped(){return !this->mask[1];}
};

template<class Iter>void normal(Iter iter,float opacity)
{
  for(;;){
    for(;!iter.is_row_end();iter.next_pixel()){
      if(iter.should_skipped())continue;
      pixel_t brush_a=pix(eval(pix(iter.get_brush_alpha())*pix(opacity)));
      pixel_t base_a=pix(iter.src[1]);
      result_t alpha=eval(brush_a+(pix(1.f)-brush_a)*base_a);
      iter.dest[1]=r2d(alpha);
      if(alpha){pixel_t dest_a=pix(alpha);iter.dest[0]=r2d(eval((brush_a*pix(iter.get_brush_color()[0])+(pix(1.f)-brush_a)*base_a*pix(iter.src[0]))/dest_a));}
      else iter.dest[0]=0; // Defined hidden black; no division/cast of NaN.
    }
    if(iter.is_data_end())break;
    iter.next_row();
  }
}
template<class Iter>void erase(Iter iter,float color_a,float opacity)
{
  for(;;){
    for(;!iter.is_row_end();iter.next_pixel()){
      if(iter.should_skipped())continue;
      pixel_t brush_a=pix(eval(pix(iter.get_brush_alpha())*pix(opacity)));
      pixel_t base_a=pix(iter.src[1]);
      result_t alpha=eval(brush_a*pix(color_a)+(pix(1.f)-brush_a)*base_a);
      iter.dest[1]=r2d(alpha);
      if(alpha){pixel_t inverse=pix(eval(pix(1.f)-brush_a)),dest_a=pix(alpha);iter.dest[0]=r2d(eval((inverse*base_a*pix(iter.src[0])+brush_a*pix(color_a)*pix(iter.get_brush_color()[0]))/dest_a));}
      else iter.dest[0]=0;
    }
    if(iter.is_data_end())break;
    iter.next_row();
  }
}
template<class Iter>void lock_alpha(Iter iter,float opacity)
{
  for(;;){
    for(;!iter.is_row_end();iter.next_pixel()){
      if(iter.should_skipped())continue;
      if(!iter.src[1]){iter.dest[0]=0;continue;}
      pixel_t brush_a=pix(eval(pix(iter.get_brush_alpha())*pix(opacity)));
      pixel_t inverse=pix(eval(f2p(1.f)-brush_a));
      pixel_t alpha=pix(iter.src[1]),dest_a=pix(iter.src[1]),base_a=dest_a;
      iter.dest[0]=r2d(eval((brush_a*alpha*pix(iter.get_brush_color()[0])+inverse*base_a*pix(iter.src[0]))/dest_a));
    }
    if(iter.is_data_end())break;
    iter.next_row();
  }
}
template<class Iter>void accumulate(Iter iter,float*sum_weight,float*sum_r,float*sum_g,float*sum_b,float*sum_a)
{
  internal_t weight=0,gray=0,alpha=0;
  for(;;){
    for(;!iter.is_row_end();iter.next_pixel()){
      if(iter.should_skipped())continue;
      pixel_t opa=pix(iter.get_brush_alpha()),a=pix(iter.src[1]);
      weight+=r2i(eval(opa));gray+=r2i(eval(opa*pix(iter.src[0])*a));alpha+=r2i(eval(opa*a));
    }
    if(iter.is_data_end())break;
    iter.next_row();
  }
  *sum_weight+=weight;*sum_r+=gray;*sum_g+=gray;*sum_b+=gray;*sum_a+=alpha;
}
} } }
#endif
