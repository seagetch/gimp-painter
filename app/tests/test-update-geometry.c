/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Pure geometry regression; no application, GEGL graph or display required. */
#include <math.h>
#include <glib.h>

#include "core/gimp-update-geometry.h"
#include "core/gimp-update-geometry.h"

static void
ordinary_grid_equivalence (void)
{
  static const gint grids[] = { 1, 32 };

  /* Independent expression of the previous floating-point grid expansion.
   * All these inputs and rounded endpoints fit in gint, so its original
   * arithmetic is defined.  Include bounds that do not start on a grid line
   * and updates outside the bounds but inside their first/last grid cells.
   */
  for (guint grid = 0; grid < G_N_ELEMENTS (grids); grid++)
    for (gint start = -96; start <= 96; start++)
      for (gint size = 1; size <= 65; size++)
        for (gint bounds = -45; bounds <= 45; bounds += 5)
          for (gint extent = 1; extent <= 70; extent += 7)
            {
              gint first = MAX ((gint) (floor (start / (gdouble) grids[grid]) * grids[grid]), bounds);
              gint last  = MIN ((gint) (ceil ((start + size) / (gdouble) grids[grid]) * grids[grid]),
                                bounds + extent);
              gint actual_start = 123;
              gint actual_size  = 456;
              gboolean result;

              result = gimp_update_interval_align (start, size, bounds, extent,
                                                    grids[grid], &actual_start,
                                                    &actual_size);
              g_assert_cmpint (result, ==, last > first);
              if (result)
                {
                  g_assert_cmpint (actual_start, ==, first);
                  g_assert_cmpint (actual_size, ==, last - first);
                }
            }
}

static void
unrounded_intersection_bounds (void)
{
  gint start, size;

  g_assert_true (gimp_update_interval_align (G_MAXINT - 2, 2049,
                                             G_MAXINT - 100, 100,
                                             1, &start, &size));
  g_assert_cmpint (start, ==, G_MAXINT - 2);
  g_assert_cmpint (size, ==, 2);
  g_assert_true (gimp_update_interval_align ((gint64) G_MININT - 2, 4,
                                             G_MININT, 100,
                                             1, &start, &size));
  g_assert_cmpint (start, ==, G_MININT);
  g_assert_cmpint (size, ==, 2);
  g_assert_true (gimp_update_interval_align (G_MAXINT - 20, 100,
                                             G_MAXINT - 10, G_MAXINT,
                                             1, &start, &size));
  g_assert_cmpint (start, ==, G_MAXINT - 10);
  g_assert_cmpint (size, ==, 10);
  g_assert_false (gimp_update_interval_align (G_MAXINT, 1,
                                              G_MAXINT, G_MAXINT,
                                              1, &start, &size));
  g_assert_false (gimp_update_interval_align (0, 1, 0, 1, 0, &start, &size));
  g_assert_false (gimp_update_interval_align (0, 1, 0, 1, -1, &start, &size));
}

static void
translation_bounds (void)
{
  static const struct
  {
    gint start, size, offset, expected_start, expected_size;
  } cases[] = {
    { -12, 50, -30, -42, 50 },
    { G_MAXINT - 2, 2049, 0, G_MAXINT - 2, 2 },
    { 16, 32, G_MAXINT - 2, G_MAXINT, 0 },
    { -16, 32, G_MININT + 2, G_MININT, 18 },
    { G_MININT, G_MAXINT, G_MININT, G_MININT, 0 },
    { G_MAXINT, G_MAXINT, G_MAXINT, G_MAXINT, 0 },
    { G_MININT, G_MAXINT, G_MAXINT, -1, G_MAXINT },
    { G_MAXINT, G_MAXINT, G_MININT, -1, G_MAXINT },
    { G_MININT, G_MAXINT, 0, G_MININT, G_MAXINT },
    { 17, 0, 9, 26, 0 },
    { 17, -1, 9, 26, 0 },
    { 17, G_MININT, 9, 26, 0 }
  };

  for (guint i = 0; i < G_N_ELEMENTS (cases); i++)
    {
      gint start, size;

      gimp_update_interval_translate (cases[i].start, cases[i].size,
                                       cases[i].offset, &start, &size);
      g_assert_cmpint (start, ==, cases[i].expected_start);
      g_assert_cmpint (size, ==, cases[i].expected_size);
      g_assert_cmpint ((gint64) start + size, <=, G_MAXINT);
      g_assert_cmpint (size, >=, 0);
    }
}

static void
extreme_grid_bounds (void)
{
  static const struct
  {
    gint64 start;
    gint size, bounds_start, bounds_size;
    gboolean nonempty;
    gint expected_start, expected_size;
  } cases[] = {
    { G_MAXINT - 2, 2049, 0, 17, FALSE, 0, 0 },
    { G_MININT + 2, 2049, 0, 17, FALSE, 0, 0 },
    { (gint64) G_MAXINT - G_MININT, 2049, 0, 17, FALSE, 0, 0 },
    { (gint64) G_MININT - G_MAXINT, 2049, 0, 17, FALSE, 0, 0 },
    { G_MAXINT64, G_MAXINT, 0, 17, FALSE, 0, 0 },
    { G_MININT64, G_MAXINT, 0, 17, FALSE, 0, 0 },
    { G_MAXINT - 2, 2049, G_MAXINT - 100, 100,
      TRUE, G_MAXINT - 31, 31 },
    { (gint64) G_MININT - 2, 4, G_MININT, 100,
      TRUE, G_MININT, 32 },
    { G_MININT, 1, G_MININT, 100, TRUE, G_MININT, 32 },
    { G_MAXINT - 2, 2049, G_MAXINT - 10, G_MAXINT,
      TRUE, G_MAXINT - 10, 10 },
    { G_MININT, G_MAXINT, G_MININT, G_MAXINT,
      TRUE, G_MININT, G_MAXINT },
    { -17, G_MAXINT, -17, G_MAXINT, TRUE, -17, G_MAXINT },
    { G_MAXINT, 1, G_MAXINT, 1, FALSE, 0, 0 },
    { -1, 1, 1, 10, FALSE, 0, 0 },
    { 0, 1, 1, 10, TRUE, 1, 10 },
    { 31, 1, 1, 10, TRUE, 1, 10 },
    { 32, 1, 1, 10, FALSE, 0, 0 },
    { -33, 1, -31, 10, FALSE, 0, 0 },
    { -32, 1, -31, 10, TRUE, -31, 10 },
    { -1, 1, -31, 10, TRUE, -31, 10 },
    { 0, 1, -31, 10, FALSE, 0, 0 },
    { 1, 0, 0, 100, FALSE, 0, 0 },
    { 1, -1, 0, 100, FALSE, 0, 0 },
    { 1, G_MININT, 0, 100, FALSE, 0, 0 },
    { 1, 100, 0, 0, FALSE, 0, 0 },
    { 1, 100, 0, -1, FALSE, 0, 0 },
    { 1, 100, 0, G_MININT, FALSE, 0, 0 }
  };

  for (guint i = 0; i < G_N_ELEMENTS (cases); i++)
    {
      gint start = 123, size = 456;
      gboolean result;

      result = gimp_update_interval_align (cases[i].start, cases[i].size,
                                            cases[i].bounds_start,
                                            cases[i].bounds_size,
                                            32, &start, &size);
      g_assert_cmpint (result, ==, cases[i].nonempty);
      if (result)
        {
          g_assert_cmpint (start, ==, cases[i].expected_start);
          g_assert_cmpint (size, ==, cases[i].expected_size);
          g_assert_cmpint (start, >=, cases[i].bounds_start);
          g_assert_cmpint ((gint64) start + size, <=, G_MAXINT);
          g_assert_cmpint ((gint64) start + size, <=,
                          (gint64) cases[i].bounds_start + cases[i].bounds_size);
        }
    }
}

static void
grid_rounding (void)
{
  static const gint grids[] = { 1, 2, 3, 31, 32, 33, G_MAXINT };
  static const gint values[] = {
    G_MININT, G_MININT + 1, -65, -33, -32, -31, -1,
    0, 1, 31, 32, 33, 65, G_MAXINT - 1, G_MAXINT
  };

  for (guint i = 0; i < G_N_ELEMENTS (grids); i++)
    for (guint j = 0; j < G_N_ELEMENTS (values); j++)
      {
        gint64 lower = gimp_update_interval_floor (values[j], grids[i]);
        gint64 upper = gimp_update_interval_ceil (values[j], grids[i]);

        g_assert_cmpint (lower, <=, values[j]);
        g_assert_cmpint (upper, >=, values[j]);
        g_assert_cmpint (values[j] - lower, <, grids[i]);
        g_assert_cmpint (upper - values[j], <, grids[i]);
        g_assert_cmpint (lower % grids[i], ==, 0);
        g_assert_cmpint (upper % grids[i], ==, 0);
      }
}

int
main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/update-geometry/ordinary-grid-equivalence", ordinary_grid_equivalence);
  g_test_add_func ("/update-geometry/translation-bounds", translation_bounds);
  g_test_add_func ("/update-geometry/extreme-grid-bounds", extreme_grid_bounds);
  g_test_add_func ("/update-geometry/unrounded-intersection-bounds", unrounded_intersection_bounds);
  g_test_add_func ("/update-geometry/grid-rounding", grid_rounding);
  return g_test_run ();
}
