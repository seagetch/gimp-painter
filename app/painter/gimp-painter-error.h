/* GIMP Painter common C boundary. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_ERROR_H
#define GIMP_PAINTER_ERROR_H

#include <glib-object.h>

G_BEGIN_DECLS

typedef enum
{
  GIMP_PAINTER_ERROR_WRONG_TYPE,
  GIMP_PAINTER_ERROR_MISSING_SLOT,
  GIMP_PAINTER_ERROR_DUPLICATE_SLOT,
  GIMP_PAINTER_ERROR_INVALID_STATE,
  GIMP_PAINTER_ERROR_CLOSED,
  GIMP_PAINTER_ERROR_WRONG_THREAD,
  GIMP_PAINTER_ERROR_EXCEPTION
} GimpPainterError;

#define GIMP_PAINTER_ERROR (gimp_painter_error_quark ())
GQuark gimp_painter_error_quark (void);

G_END_DECLS
#endif
