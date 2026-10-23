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

#ifndef __GIMP_PAINT_H__
#define __GIMP_PAINT_H__

#include <glib-object.h>
typedef struct _Gimp Gimp;
typedef struct _GimpPaintInfo GimpPaintInfo;
G_BEGIN_DECLS


void   gimp_paint_init (Gimp *gimp);
void   gimp_paint_exit (Gimp *gimp);
/* Consumes one paint_info reference after valid application/resource/live
 * registry preconditions. Invalid arguments remain caller-owned. */
void   gimp_paint_register (Gimp *gimp, GimpPaintInfo *paint_info);


G_END_DECLS

#endif  /* __GIMP_PAINT_H__ */
