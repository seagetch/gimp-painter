/* SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef __GIMP_UPDATE_GEOMETRY_H__
#define __GIMP_UPDATE_GEOMETRY_H__

#include <glib.h>


/* Update signals and cairo regions use signed-int coordinates.  Keep both
 * endpoints representable, not just the origin.  Clipping a translated
 * interval cannot increase its (already gint-sized) length.  Empty intervals
 * remain empty; callers still emit their normal update notifications.
 */
static inline void
gimp_update_interval_translate (gint  start,
                                gint  size,
                                gint  offset,
                                gint *result_start,
                                gint *result_size)
{
  gint64 first = (gint64) start + offset;
  gint64 last  = first + MAX (size, 0);

  first = CLAMP (first, G_MININT, G_MAXINT);
  last  = CLAMP (last,  G_MININT, G_MAXINT);

  *result_start = (gint) first;
  *result_size  = (gint) (last - first);
}

/* These round mathematically, including for negative coordinates.  Their
 * callers bound the values to a few gint ranges before rounding, and require
 * a strictly positive gint grid, so all intermediate values fit in gint64.
 */
static inline gint64
gimp_update_interval_floor (gint64 value,
                            gint   grid)
{
  gint64 remainder = value % grid;

  return value - remainder - (remainder < 0 ? grid : 0);
}

static inline gint64
gimp_update_interval_ceil (gint64 value,
                           gint   grid)
{
  gint64 remainder = value % grid;

  return value - remainder + (remainder > 0 ? grid : 0);
}

/* Return the grid-expanded update intersected with the projectable bounds
 * and with the representable endpoint range.  First clip to the bounds'
 * covering grid cells, rather than the bounds themselves: a positive update
 * just outside the bounds can still touch their boundary cell.  This preserves
 * the original align-before-intersect result for ordinary positive extents.
 *
 * Nonpositive update/bounds extents are empty, including off-grid zero-sized
 * updates that the old rounding code could accidentally turn into an area.
 * The bounds' gint size limits the final width to G_MAXINT.  No narrowing or
 * endpoint arithmetic in GEGL/cairo occurs until these limits are established.
 */
static inline gboolean
gimp_update_interval_align (gint64 start,
                            gint   size,
                            gint   bounds_start,
                            gint   bounds_size,
                            gint   grid,
                            gint  *result_start,
                            gint  *result_size)
{
  gint64 bounds_last;
  gint64 grid_first;
  gint64 grid_last;
  gint64 first;
  gint64 last;

  if (size <= 0 || bounds_size <= 0 || grid <= 0)
    return FALSE;

  bounds_last = MIN ((gint64) bounds_start + bounds_size, G_MAXINT);

  if (bounds_last <= bounds_start)
    return FALSE;

  grid_first = gimp_update_interval_floor (bounds_start, grid);
  grid_last  = gimp_update_interval_ceil  (bounds_last, grid);

  /* Reject before adding size, even if an arbitrary gint64 start was passed.
   * A surviving start and its endpoint are then bounded by a few gint ranges.
   */
  if (start >= grid_last || start <= grid_first - size)
    return FALSE;

  first = MAX (start,        grid_first);
  last  = MIN (start + size, grid_last);
  first = MAX (gimp_update_interval_floor (first, grid), bounds_start);
  last  = MIN (gimp_update_interval_ceil  (last,  grid), bounds_last);

  if (last <= first)
    return FALSE;

  *result_start = (gint) first;
  *result_size  = (gint) (last - first);

  return TRUE;
}


#endif /* __GIMP_UPDATE_GEOMETRY_H__ */
