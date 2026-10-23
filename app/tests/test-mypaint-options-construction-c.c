/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gegl.h>
#include <gtk/gtk.h>
#include "paint/gimpmypaintoptions.h"
GimpMypaintOptions *options_original_c_constructor (GimpToolInfo *info);
GimpMypaintOptions *options_original_c_constructor (GimpToolInfo *info)
{
  GType (*type_entry) (void) = gimp_mypaint_options_get_type;
  GimpMypaintOptions *(*new_entry) (GimpToolInfo *) = gimp_mypaint_options_new;
  g_assert_cmpuint (type_entry (), ==, GIMP_TYPE_PAINTER_MYBRUSH_OPTIONS);
  return new_entry (info);
}
