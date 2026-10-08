/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Compile as both C and C++, with baseline and current production headers. */
#include "config.h"
#include <stddef.h>
#include <stdio.h>
#include <gegl.h>
#include <gio/gio.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include "libgimpbase/gimpbase.h"
#include "libgimpmath/gimpmath.h"
#include "core-types.h"
#include GIMP_FILTER_LAYOUT_HEADER

#ifdef __cplusplus
#include <type_traits>
static_assert (std::is_standard_layout<GimpFilterLayerSnapshot>::value,
               "Snapshot must retain C-compatible layout");
static_assert (std::is_standard_layout<GimpFilterArgumentPatch>::value,
               "Argument patch must retain C-compatible layout");
static_assert (std::is_standard_layout<GimpFilterArgumentSpec>::value,
               "Argument import spec must retain C-compatible layout");
#endif

#define RECORD(T) printf (#T ":size=%zu:align=%zu\n", \
                         sizeof (T), (size_t) __alignof__ (T))
#define FIELD(T, F) printf (#T "." #F "=%zu\n", offsetof (T, F))

int
main (void)
{
  RECORD (GimpFilterLayerSnapshot);
  FIELD (GimpFilterLayerSnapshot, version);
  FIELD (GimpFilterLayerSnapshot, generation);
  FIELD (GimpFilterLayerSnapshot, cache_generation);
  FIELD (GimpFilterLayerSnapshot, cache_complete);
  FIELD (GimpFilterLayerSnapshot, state);
  RECORD (GimpFilterArgumentPatch);
  FIELD (GimpFilterArgumentPatch, index);
  FIELD (GimpFilterArgumentPatch, value);
  RECORD (GimpFilterArgumentSpec);
  FIELD (GimpFilterArgumentSpec, value_type);
  FIELD (GimpFilterArgumentSpec, is_null);
  FIELD (GimpFilterArgumentSpec, value);
  FIELD (GimpFilterArgumentSpec, n_references);
  FIELD (GimpFilterArgumentSpec, references);
  FIELD (GimpFilterArgumentSpec, targets);
  FIELD (GimpFilterArgumentSpec, n_children);
  FIELD (GimpFilterArgumentSpec, children);
  return 0;
}
