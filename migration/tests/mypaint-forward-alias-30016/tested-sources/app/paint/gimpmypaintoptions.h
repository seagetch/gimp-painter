/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_MYPAINT_OPTIONS_H
#define GIMP_MYPAINT_OPTIONS_H

#include "painter-mypaint-surface/gimp-painter-options.h"

G_BEGIN_DECLS

/* Operation-level source compatibility, not legacy layout or runtime GType. */
typedef GimpPainterMybrushOptions GimpMypaintOptions;
#define GIMP_MYPAINT_OPTIONS(obj) GIMP_PAINTER_MYBRUSH_OPTIONS (obj)
#define GIMP_IS_MYPAINT_OPTIONS(obj) GIMP_IS_PAINTER_MYBRUSH_OPTIONS (obj)

/* Replaces one active draft mapping. Borrows name/points only for this call.
 * Zero points explicitly clears the mapping; otherwise 2 through 8 finite,
 * float-representable points are required. Coordinates round through float;
 * x order is nondecreasing after rounding. No sorting or range clamping.
 * Invalid/unavailable requests leave the draft and notifications unchanged.
 * Successful calls use native draft dirty/history and notification behavior,
 * including broader property fanout plus one settings-changed emission when
 * no reentrant callback supersedes it. No extra legacy notification is sent. */
void gimp_mypaint_options_set_mapping_point (GimpMypaintOptions *options,
                                           gchar              *name,
                                           guint               input,
                                           guint               size,
                                           GimpVector2        *points);

/* Copies the active draft curve, never the selected resource. Caller owns
 * *points and must g_free it. Coordinates retain legacy float rounding.
 * Name normalization preserves ASCII letters/digits/underscore and replaces
 * other characters with underscore; name itself is not modified.
 * Empty/unavailable/invalid results initialize supplied outputs to 0/NULL.
 * Both output pointers are required. Nonfinite or float-unrepresentable
 * coordinates are rejected as a whole, with no clamping or partial array.
 * A source-less active native draft is still a draft; no model is created. */
void gimp_mypaint_options_get_mapping_point (GimpMypaintOptions *options,
                                           gchar              *name,
                                           guint               input,
                                           guint              *size,
                                           GimpVector2       **points);

G_END_DECLS

#endif
