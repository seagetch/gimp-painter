/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gegl.h>
#include <gio/gio.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include "core-types.h"
#include "gimpimage.h"
#include "gimpimage-private.h"
#include "gimpimage-perspective-guide.h"

GimpPerspectiveGuide *
gimp_image_get_perspective_guide (GimpImage *image)
{
  g_return_val_if_fail (GIMP_IS_IMAGE (image), NULL);
  return GIMP_IMAGE_GET_PRIVATE (image)->perspective_guide;
}
void
gimp_image_set_perspective_guide (GimpImage *image, GimpPerspectiveGuide *guide)
{
  GimpImagePrivate *private;
  GimpPerspectiveGuide *previous;
  g_return_if_fail (GIMP_IS_IMAGE (image));
  g_return_if_fail (guide == NULL || GIMP_IS_PERSPECTIVE_GUIDE (guide));
  private = GIMP_IMAGE_GET_PRIVATE (image);
  if (private->perspective_guide_disposing || private->perspective_guide == guide) return;
  g_object_ref (image);
  if (guide) g_object_ref (guide);
  previous = private->perspective_guide;
  private->perspective_guide = guide;
  /* Publish first: observers may synchronously replace the new guide. */
  if (previous)
    {
      g_signal_emit_by_name (previous, "removed");
      g_object_unref (previous);
    }
  g_signal_emit_by_name (image, "perspective-guide-changed");
  g_object_unref (image);
}
void
gimp_image_perspective_guide_dispose (GimpImage *image)
{
  GimpImagePrivate *private = GIMP_IMAGE_GET_PRIVATE (image);
  GimpPerspectiveGuide *previous = private->perspective_guide;
  private->perspective_guide_disposing = TRUE;
  private->perspective_guide = NULL;
  if (previous)
    {
      g_signal_emit_by_name (previous, "removed");
      g_signal_emit_by_name (image, "perspective-guide-changed");
      g_object_unref (previous);
    }
}
