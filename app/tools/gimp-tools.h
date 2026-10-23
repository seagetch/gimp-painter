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

#ifndef __GIMP_TOOLS_H__
#define __GIMP_TOOLS_H__

#include <glib-object.h>
typedef struct _Gimp Gimp;
typedef struct _GimpToolInfo GimpToolInfo;
typedef struct _GimpContainer GimpContainer;
typedef struct _GimpConfigWriter GimpConfigWriter;
G_BEGIN_DECLS


/* Consumes one tool_info reference after valid application/resource/live
 * registry preconditions. Invalid arguments remain caller-owned. */
void       gimp_tools_register    (Gimp              *gimp,
                                   GimpToolInfo       *tool_info);

void       gimp_tools_init        (Gimp              *gimp);
void       gimp_tools_exit        (Gimp              *gimp);

void       gimp_tools_restore     (Gimp              *gimp);
void       gimp_tools_save        (Gimp              *gimp,
                                   gboolean           save_tool_options,
                                   gboolean           always_save);

gboolean   gimp_tools_clear       (Gimp              *gimp,
                                   GError           **error);

gboolean   gimp_tools_serialize   (Gimp              *gimp,
                                   GimpContainer     *container,
                                   GimpConfigWriter  *writer);
gboolean   gimp_tools_deserialize (Gimp              *gimp,
                                   GimpContainer     *container,
                                   GScanner          *scanner);

void       gimp_tools_reset       (Gimp              *gimp,
                                   GimpContainer     *container,
                                   gboolean           user_toolrc);


G_END_DECLS

#endif  /* __GIMP_TOOLS_H__ */
