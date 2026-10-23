/* brushlib - The MyPaint Brush Library
 * Copyright (C) 2008 Martin Renold <martinxyz@gmx.ch>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
 
#ifndef __MYPAINTBRUSH_SURFACE_HPP__
#define __MYPAINTBRUSH_SURFACE_HPP__

#include "../../painter/gimp-painter-visibility.h"
namespace GimpPainter GIMP_PAINTER_PRIVATE { namespace MyPaint {

// Extended surface contract required by the pinned painter engine
class Surface {
public:

  virtual ~Surface() {}

  virtual bool draw_dab (float x, float y, 
                         float radius, 
                         float color_r, float color_g, float color_b,
                         float opaque, float hardness = 0.5,
                         float alpha_eraser = 1.0,
                         float aspect_ratio = 1.0, float angle = 0.0,
                         float lock_alpha = 0.0, float colorize = 0.0,
                         float texture_grain = 0.0, float texture_contrast = 1.0
                         ) = 0;

  virtual void get_color (float x, float y, 
                          float radius,
                          float * color_r, float * color_g, float * color_b, float * color_a,
                          float hardness = 0.5, float aspect_ratio = 1.0, float angle = 0.0,
                          float texture_grain = 0.0, float texture_contrast = 1.0
                          ) = 0;

  virtual void begin_session() = 0;

  virtual void end_session() = 0;

};

} }
#endif
