/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gegl.h>
#include <gtk/gtk.h>
#include "paint/gimpmypaintoptions.h"
_Static_assert(GIMP_MYPAINT_NORMAL==0 && GIMP_MYPAINT_NORMAL_AND_ERASE==1 &&
               GIMP_MYPAINT_LOCK_ALPHA==2 && GIMP_MYPAINT_COLORIZE==3,
               "Original mode source enum values");
GimpMypaintBrushMode options_mode_c_consumer(GimpMypaintOptions *options);
GimpMypaintBrushMode options_mode_c_consumer(GimpMypaintOptions *options)
{
 GimpMypaintBrushMode (*entry)(GimpMypaintOptions*)=gimp_mypaint_options_get_brush_mode;
 return entry(options);
}
