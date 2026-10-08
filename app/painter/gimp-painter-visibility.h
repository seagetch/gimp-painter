/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_VISIBILITY_H
#define GIMP_PAINTER_VISIBILITY_H

/* Private C++ declarations are not an exported application/plug-in ABI.
 * Keep this dependency-free for standalone pixel/geometry code, and empty in
 * C so the same record declarations retain their C layout and entry points.
 * Toolchains without GNU visibility retain their normal explicit-export model.
 */
#if defined(__cplusplus) && defined(__GNUC__)
#define GIMP_PAINTER_PRIVATE __attribute__ ((visibility ("hidden")))
#define GIMP_PAINTER_C_ENTRY __attribute__ ((visibility ("default")))
#else
#define GIMP_PAINTER_PRIVATE
#define GIMP_PAINTER_C_ENTRY
#endif

#endif
