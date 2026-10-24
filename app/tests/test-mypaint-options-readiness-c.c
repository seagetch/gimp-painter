/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gegl.h>
#include "paint/gimpmypaintoptions.h"
gboolean options_readiness_c_consumer (GimpMypaintOptions *options);
gboolean options_readiness_c_consumer (GimpMypaintOptions *options)
{
  gboolean (*entry)(GimpMypaintOptions *) = gimp_mypaint_options_is_ready;
  return entry(options);
}
