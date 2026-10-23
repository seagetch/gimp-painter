/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995-2001 Spencer Kimball, Peter Mattis and others
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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gegl.h>

#include "paint-types.h"

#include "core/gimp.h"
#include "core/gimplist.h"
#include "core/gimppaintinfo.h"

#include "gimp-paint.h"
#include "gimpairbrush.h"
#include "gimpclone.h"
#include "gimpconvolve.h"
#include "gimpdodgeburn.h"
#include "gimperaser.h"
#include "gimpfillbrush.h"
#include "gimppaintersmudge.h"
#include "gimpheal.h"
#include "gimpink.h"
#include "gimpmybrushcore.h"
#include "gimppainterpaintgate.h"
#include "gimppaintoptions.h"
#include "gimppaintbrush.h"
#include "gimppencil.h"
#include "gimpperspectiveclone.h"
#include "gimpsmudge.h"


/*  local function prototypes  */

static void   gimp_paint_register_internal (Gimp        *gimp,
                                   GType        paint_type,
                                   GType        paint_options_type,
                                   const gchar *identifier,
                                   const gchar *blurb,
                                   const gchar *icon_name);


/*  public functions  */

void
gimp_paint_init (Gimp *gimp)
{
  GimpPaintRegisterFunc register_funcs[] =
  {
    gimp_dodge_burn_register,
    gimp_smudge_register,
    gimp_convolve_register,
    gimp_perspective_clone_register,
    gimp_heal_register,
    gimp_clone_register,
    gimp_mybrush_core_register,
    gimp_painter_paint_gate_register,
    gimp_ink_register,
    gimp_airbrush_register,
    gimp_eraser_register,
    gimp_fill_brush_register,
    gimp_painter_smudge_register,
    gimp_paintbrush_register,
    gimp_pencil_register
  };

  gint i;

  g_return_if_fail (GIMP_IS_GIMP (gimp));

  gimp->paint_info_list = gimp_list_new (GIMP_TYPE_PAINT_INFO, FALSE);
  gimp_object_set_static_name (GIMP_OBJECT (gimp->paint_info_list),
                               "paint infos");

  gimp_container_freeze (gimp->paint_info_list);

  for (i = 0; i < G_N_ELEMENTS (register_funcs); i++)
    {
      register_funcs[i] (gimp, gimp_paint_register_internal);
    }

  gimp_container_thaw (gimp->paint_info_list);
}

void
gimp_paint_exit (Gimp *gimp)
{
  g_return_if_fail (GIMP_IS_GIMP (gimp));

  gimp_paint_info_set_standard (gimp, NULL);

  if (gimp->paint_info_list)
    {
      gimp_container_foreach (gimp->paint_info_list,
                              (GFunc) g_object_run_dispose, NULL);
      g_clear_object (&gimp->paint_info_list);
    }
}


/* Transfer-full after the public preconditions have been validated. Keep both
 * registry and application alive across native container observers. */
void
gimp_paint_register (Gimp          *gimp,
                     GimpPaintInfo *paint_info)
{
  GimpContainer *registry;

  g_return_if_fail (GIMP_IS_GIMP (gimp));
  g_return_if_fail (GIMP_IS_PAINT_INFO (paint_info));
  g_return_if_fail (GIMP_IS_CONTAINER (gimp->paint_info_list));

  g_object_ref (gimp);
  registry = g_object_ref (gimp->paint_info_list);
  gimp_container_add (registry, GIMP_OBJECT (paint_info));
  g_object_unref (paint_info);
  g_object_unref (registry);
  g_object_unref (gimp);
}


/*  private functions  */

static void
gimp_paint_register_internal (Gimp        *gimp,
                     GType        paint_type,
                     GType        paint_options_type,
                     const gchar *identifier,
                     const gchar *blurb,
                     const gchar *icon_name)
{
  GimpPaintInfo *paint_info;
  GimpContainer *registry;

  g_return_if_fail (GIMP_IS_GIMP (gimp));
  g_return_if_fail (g_type_is_a (paint_type, GIMP_TYPE_PAINT_CORE));
  g_return_if_fail (g_type_is_a (paint_options_type, GIMP_TYPE_PAINT_OPTIONS));
  g_return_if_fail (identifier != NULL);
  g_return_if_fail (blurb != NULL);
  g_return_if_fail (icon_name != NULL);

  g_object_ref (gimp);
  registry = g_object_ref (gimp->paint_info_list);
  paint_info = gimp_paint_info_new (gimp,
                                    paint_type,
                                    paint_options_type,
                                    identifier,
                                    blurb,
                                    icon_name);

  if (!paint_info)
    {
      g_object_unref (registry);
      g_object_unref (gimp);
      return;
    }

  /* Retain one local lease: the public entry consumes the transferred one,
   * and observers may remove the resource or replace the application registry. */
  gimp_paint_register (gimp, g_object_ref (paint_info));

  if (paint_type == GIMP_TYPE_PAINTBRUSH &&
      gimp->paint_info_list == registry &&
      gimp_container_have (registry, GIMP_OBJECT (paint_info)))
    gimp_paint_info_set_standard (gimp, paint_info);

  g_object_unref (paint_info);
  g_object_unref (registry);
  g_object_unref (gimp);
}
