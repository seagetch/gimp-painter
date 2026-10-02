/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef __GIMP_IMAGE_PERSPECTIVE_GUIDE_H__
#define __GIMP_IMAGE_PERSPECTIVE_GUIDE_H__
#include "gimpperspectiveguide.h"
G_BEGIN_DECLS
/* Getter borrows; setter retains incoming before releasing previous, including self assignment. */
GimpPerspectiveGuide *gimp_image_get_perspective_guide (GimpImage *image);
void gimp_image_set_perspective_guide (GimpImage *, GimpPerspectiveGuide *);
void gimp_image_perspective_guide_dispose (GimpImage *);
/* Capture the pre-edit guide (nullable); this operation does not edit the image. */
void gimp_image_perspective_guide_push_undo (GimpImage *, GimpPerspectiveGuide *before, const gchar *name);
G_END_DECLS
#endif
