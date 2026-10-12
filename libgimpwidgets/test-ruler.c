/* LIBGIMP - The GIMP Library
 * Copyright (C) 2026
 *
 * This library is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library.  If not, see
 * <https://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <float.h>
#include <math.h>
#include <string.h>
#include <gtk/gtk.h>

#include "gimpwidgetstypes.h"
#include "gimpruler.h"


typedef struct
{
  GtkWidget      *window;
  GimpRuler      *ruler;
  GtkOrientation orientation;
} RulerFixture;


static void
flush_events (void)
{
  while (g_main_context_iteration (NULL, FALSE))
    ;
}

static RulerFixture
create_ruler (GtkOrientation orientation)
{
  RulerFixture    fixture;
  GtkWidget      *widget;
  GtkCssProvider *css;

  fixture.window      = gtk_offscreen_window_new ();
  fixture.orientation = orientation;
  g_object_ref_sink (fixture.window);

  widget = gimp_ruler_new (orientation);
  fixture.ruler = GIMP_RULER (widget);
  gtk_widget_set_size_request (widget,
                               orientation == GTK_ORIENTATION_HORIZONTAL ? 320 : 32,
                               orientation == GTK_ORIENTATION_HORIZONTAL ? 32 : 320);

  css = gtk_css_provider_new ();
  gtk_css_provider_load_from_data (css,
                                   "* { background-color: white; background-image: none;"
                                   " color: black; border: none; padding: 0;"
                                   " box-shadow: none; font: 12px Sans; }",
                                   -1, NULL);
  gtk_style_context_add_provider (gtk_widget_get_style_context (widget),
                                   GTK_STYLE_PROVIDER (css),
                                   GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_object_unref (css);

  gtk_container_add (GTK_CONTAINER (fixture.window), widget);
  gtk_widget_show_all (fixture.window);
  flush_events ();
  g_assert_true (gtk_widget_is_drawable (widget));

  return fixture;
}

static void
destroy_ruler (RulerFixture *fixture)
{
  gtk_widget_destroy (fixture->window);
  g_object_unref (fixture->window);
  flush_events ();
}

static cairo_surface_t *
snapshot (RulerFixture *fixture,
          gdouble       position)
{
  GtkWidget       *widget = GTK_WIDGET (fixture->ruler);
  cairo_surface_t *surface;
  cairo_t         *cr;

  gimp_ruler_set_position (fixture->ruler, position);
  flush_events ();

  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                         gtk_widget_get_allocated_width (widget),
                                         gtk_widget_get_allocated_height (widget));
  cr = cairo_create (surface);
  cairo_set_source_rgb (cr, 1.0, 1.0, 1.0);
  cairo_paint (cr);
  gtk_widget_draw (widget, cr);
  g_assert_cmpint (cairo_status (cr), ==, CAIRO_STATUS_SUCCESS);
  cairo_destroy (cr);
  cairo_surface_flush (surface);
  g_assert_cmpint (cairo_surface_status (surface), ==, CAIRO_STATUS_SUCCESS);

  return surface;
}

static guint
count_drawn_pixels (cairo_surface_t *surface)
{
  const guint32 *pixels = (const guint32 *) cairo_image_surface_get_data (surface);
  gint           width  = cairo_image_surface_get_width (surface);
  gint           height = cairo_image_surface_get_height (surface);
  gint           stride = cairo_image_surface_get_stride (surface) / 4;
  guint          count  = 0;
  gint           x, y;

  for (y = 0; y < height; y++)
    for (x = 0; x < width; x++)
      if (pixels[y * stride + x] != 0xffffffff)
        count++;

  return count;
}

static void
assert_same_pixels (cairo_surface_t *a,
                    cairo_surface_t *b)
{
  gint stride = cairo_image_surface_get_stride (a);
  gint height = cairo_image_surface_get_height (a);

  g_assert_cmpint (stride, ==, cairo_image_surface_get_stride (b));
  g_assert_cmpint (height, ==, cairo_image_surface_get_height (b));
  g_assert_cmpmem (cairo_image_surface_get_data (a), stride * height,
                  cairo_image_surface_get_data (b), stride * height);
}

/* Assert the marker changes real pixels, only close to its expected location.
 * This prevents the invalid-input tests from passing with drawing disabled.
 */
static void
assert_marker (RulerFixture    *fixture,
               cairo_surface_t *background,
               gdouble         position,
               gint            expected)
{
  cairo_surface_t *marked = snapshot (fixture, position);
  const guint32   *a = (const guint32 *) cairo_image_surface_get_data (background);
  const guint32   *b = (const guint32 *) cairo_image_surface_get_data (marked);
  gint             width  = cairo_image_surface_get_width (marked);
  gint             height = cairo_image_surface_get_height (marked);
  gint             stride = cairo_image_surface_get_stride (marked) / 4;
  gint             changed = 0;
  gint             x, y;

  for (y = 0; y < height; y++)
    for (x = 0; x < width; x++)
      if (a[y * stride + x] != b[y * stride + x])
        {
          gint coordinate = fixture->orientation == GTK_ORIENTATION_HORIZONTAL ? x : y;

          g_assert_cmpint (ABS (coordinate - expected), <=, 12);
          changed++;
        }

  g_assert_cmpint (changed, >, 0);
  cairo_surface_destroy (marked);
}

static void
test_zero_range (void)
{
  GtkOrientation orientation;
  const gdouble  limits[] = { 0.0, 42.0, -42.0, G_MAXDOUBLE };
  guint          i;

  for (orientation = GTK_ORIENTATION_HORIZONTAL;
       orientation <= GTK_ORIENTATION_VERTICAL;
       orientation++)
    {
      RulerFixture fixture = create_ruler (orientation);

      for (i = 0; i < G_N_ELEMENTS (limits); i++)
        {
          cairo_surface_t *a, *b;

          gimp_ruler_set_range (fixture.ruler, limits[i], limits[i], limits[i]);
          a = snapshot (&fixture, limits[i]);
          b = snapshot (&fixture, 25.0);
          assert_same_pixels (a, b);
          g_assert_cmpuint (count_drawn_pixels (a), ==, 0);
          cairo_surface_destroy (a);
          cairo_surface_destroy (b);
        }

      destroy_ruler (&fixture);
    }
}

static void
test_nonfinite_ranges (void)
{
  GtkOrientation orientation;
  const gdouble  ranges[][3] =
  {
    { NAN, 100.0, 100.0 },
    { 0.0, NAN, 100.0 },
    { -INFINITY, 100.0, 100.0 },
    { 0.0, INFINITY, 100.0 },
    { -G_MAXDOUBLE, G_MAXDOUBLE, G_MAXDOUBLE }
  };
  guint i;

  for (orientation = GTK_ORIENTATION_HORIZONTAL;
       orientation <= GTK_ORIENTATION_VERTICAL;
       orientation++)
    {
      RulerFixture fixture = create_ruler (orientation);

      for (i = 0; i < G_N_ELEMENTS (ranges); i++)
        {
          cairo_surface_t *a, *b;

          gimp_ruler_set_range (fixture.ruler, ranges[i][0], ranges[i][1], ranges[i][2]);
          a = snapshot (&fixture, 25.0);
          b = snapshot (&fixture, 75.0);
          assert_same_pixels (a, b);
          g_assert_cmpuint (count_drawn_pixels (a), ==, 0);
          cairo_surface_destroy (a);
          cairo_surface_destroy (b);
        }

      destroy_ruler (&fixture);
    }
}

static void
test_nonfinite_positions (void)
{
  GtkOrientation orientation;
  const gdouble  positions[] = { NAN, INFINITY, -INFINITY };
  guint          i;

  for (orientation = GTK_ORIENTATION_HORIZONTAL;
       orientation <= GTK_ORIENTATION_VERTICAL;
       orientation++)
    {
      RulerFixture     fixture = create_ruler (orientation);
      cairo_surface_t *background;

      gimp_ruler_set_range (fixture.ruler, 0.0, 100.0, 100.0);
      background = snapshot (&fixture, -1000.0);

      for (i = 0; i < G_N_ELEMENTS (positions); i++)
        {
          cairo_surface_t *invalid = snapshot (&fixture, positions[i]);

          assert_same_pixels (background, invalid);
          cairo_surface_destroy (invalid);
          assert_marker (&fixture, background, 25.0, 80);
        }

      cairo_surface_destroy (background);
      destroy_ruler (&fixture);
    }
}

static void
test_offscreen_positions (void)
{
  GtkOrientation orientation;
  const gdouble  positions[] =
  {
    G_MAXDOUBLE, -G_MAXDOUBLE, G_MAXINT, G_MININT, 1000.0, -1000.0
  };
  guint i;

  for (orientation = GTK_ORIENTATION_HORIZONTAL;
       orientation <= GTK_ORIENTATION_VERTICAL;
       orientation++)
    {
      RulerFixture     fixture = create_ruler (orientation);
      cairo_surface_t *background;

      gimp_ruler_set_range (fixture.ruler, 0.0, 100.0, 100.0);
      background = snapshot (&fixture, -1000.0);

      for (i = 0; i < G_N_ELEMENTS (positions); i++)
        {
          cairo_surface_t *offscreen;

          assert_marker (&fixture, background, 75.0, 240);
          offscreen = snapshot (&fixture, positions[i]);
          assert_same_pixels (background, offscreen);
          cairo_surface_destroy (offscreen);
        }

      /* A marker overlapping either edge must still be drawn. */
      assert_marker (&fixture, background, 0.0, 0);
      assert_marker (&fixture, background, 100.0, 320);
      cairo_surface_destroy (background);
      destroy_ruler (&fixture);
    }
}

static void
test_extreme_ranges (void)
{
  GtkOrientation orientation;
  const gdouble  ranges[][3] =
  {
    { 0.0, 100.0, G_MAXDOUBLE },
    { 0.0, 100.0, -G_MAXDOUBLE },
    { 0.0, 100.0, NAN },
    { 0.0, 100.0, INFINITY },
    { 0.0, DBL_MIN, DBL_MIN },
    { 1e20, nextafter (1e20, INFINITY), 100.0 },
    { -1e20, nextafter (-1e20, INFINITY), 100.0 },
    { (gdouble) G_MAXINT + 100.0, (gdouble) G_MAXINT + 200.0, G_MAXDOUBLE }
  };
  guint i;

  for (orientation = GTK_ORIENTATION_HORIZONTAL;
       orientation <= GTK_ORIENTATION_VERTICAL;
       orientation++)
    {
      RulerFixture fixture = create_ruler (orientation);

      for (i = 0; i < G_N_ELEMENTS (ranges); i++)
        {
          cairo_surface_t *rendered;

          gimp_ruler_set_range (fixture.ruler, ranges[i][0], ranges[i][1], ranges[i][2]);
          rendered = snapshot (&fixture, ranges[i][0]);
          cairo_surface_destroy (rendered);
        }

      /* Extreme input must not leave the backing store unusable. */
      gimp_ruler_set_range (fixture.ruler, 0.0, 100.0, 100.0);
      {
        cairo_surface_t *background = snapshot (&fixture, -1000.0);

        assert_marker (&fixture, background, 50.0, 160);
        cairo_surface_destroy (background);
      }
      destroy_ruler (&fixture);
    }
}

static void
test_normal_and_reversed_ranges (void)
{
  GtkOrientation orientation;
  const gdouble  ranges[][2] = { { 0.0, 100.0 }, { 100.0, 0.0 }, { -50.0, 50.0 } };
  guint          i;

  for (orientation = GTK_ORIENTATION_HORIZONTAL;
       orientation <= GTK_ORIENTATION_VERTICAL;
       orientation++)
    {
      RulerFixture fixture = create_ruler (orientation);

      for (i = 0; i < G_N_ELEMENTS (ranges); i++)
        {
          cairo_surface_t *background;
          gdouble          lower = ranges[i][0];
          gdouble          span  = ranges[i][1] - lower;

          gimp_ruler_set_range (fixture.ruler, lower, ranges[i][1], 100.0);
          background = snapshot (&fixture, -1000.0);
          g_assert_cmpuint (count_drawn_pixels (background), >, 50);
          assert_marker (&fixture, background, lower + 0.25 * span, 80);
          assert_marker (&fixture, background, lower + 0.75 * span, 240);
          cairo_surface_destroy (background);
        }

      destroy_ruler (&fixture);
    }
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  if (! gtk_init_check (&argc, &argv))
    return 77;

  g_test_add_func ("/gimp-ruler/zero-range", test_zero_range);
  g_test_add_func ("/gimp-ruler/nonfinite-ranges", test_nonfinite_ranges);
  g_test_add_func ("/gimp-ruler/nonfinite-positions", test_nonfinite_positions);
  g_test_add_func ("/gimp-ruler/offscreen-positions", test_offscreen_positions);
  g_test_add_func ("/gimp-ruler/extreme-ranges", test_extreme_ranges);
  g_test_add_func ("/gimp-ruler/normal-and-reversed-ranges", test_normal_and_reversed_ranges);

  return g_test_run ();
}
