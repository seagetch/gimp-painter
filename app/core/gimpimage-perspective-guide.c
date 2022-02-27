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

#include "config.h"

#include <gegl.h>

#include "core-types.h"

#include "gimp.h"
#include "gimpimage.h"
#include "gimpguide.h"
#include "gimpimage-perspective-guide.h"
#include "gimpimage-private.h"
#include "gimpimage-undo-push.h"

#include "gimp-intl.h"


/*  public functions  */

GimpPerspectiveGuide *
gimp_image_get_perspective_guide (GimpImage *image)
{
  g_return_val_if_fail (GIMP_IS_IMAGE (image), NULL);

  return GIMP_IMAGE_GET_PRIVATE (image)->perspective_guides;
}


void
gimp_image_set_perspective_guide (GimpImage *image, 
                                  GimpPerspectiveGuide *perspective_guide)
{
  GimpImagePrivate* private;
  g_return_if_fail (GIMP_IS_IMAGE (image));
  private = GIMP_IMAGE_GET_PRIVATE (image);

  if (private->perspective_guides) {
    g_object_unref (private->perspective_guides);
  }
  private->perspective_guides = perspective_guide;
}