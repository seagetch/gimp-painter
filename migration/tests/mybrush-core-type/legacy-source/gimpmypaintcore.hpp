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

#ifndef __GIMP_MYPAINT_CORE_H__
#define __GIMP_MYPAINT_CORE_H__

extern "C" {
#include "core/gimpobject.h"
#include "gimpmypaintoptions.h"
#include "core/gimpmypaintbrush.h"
};

extern "C++" {
#include "gimpmypaintcore-surface.hpp"
#include "mypaintbrush-brush.hpp"
#include "mypaintbrush-stroke.hpp"
#include "base/delegators.hpp"

class GimpMypaintCore
{
  gint         ID;               /*  unique instance ID                  */

  GimpMypaintSurface*     surface;
  Brush*       brush;
  Stroke*      stroke;
  GimpMypaintBrush* mypaint_brush;
  GimpMypaintOptions*     options;

  Delegators::Connection* option_changed_handler;
  
  public:
  GimpMypaintCore();
  ~GimpMypaintCore();
  void cleanup();
  virtual void stroke_to (GimpDrawable* drawable, 
                            gdouble dtime, 
                            const GimpCoords* coords,
                            GimpMypaintOptions* options);

  virtual void update_resource (GimpMypaintOptions* options);
  void split_stroke();
  void reset_brush();
  
  void option_changed(GObject* object, GParamSpec *pspec);
  
};

void      gimp_mypaint_core_round_line                (GimpMypaintCore    *core,
                                                     GimpMypaintOptions *options,
                                                     gboolean          constrain_15_degrees);

}; // extern C++
#endif  /*  __GIMP_MYPAINT_CORE_H__  */
