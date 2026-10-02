/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "gimp-painter-error.h"

GQuark
gimp_painter_error_quark (void)
{
  return g_quark_from_static_string ("gimp-painter-error");
}
