/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 2009 Martin Nordholts
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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <gegl.h>

#include "core/core-types.h"

#include "core/gimp.h"
#include "core/gimpimage.h"
#include "core/gimplayer.h"

#include "tests.h"

#include "gimp-app-test-utils.h"


#define GIMP_TEST_IMAGE_SIZE 100

#define ADD_TEST(function) \
  g_test_add ("/gimp-layer-grouping/" #function, \
              GimpTestFixture, \
              gimp, \
              gimp_test_image_setup, \
              function, \
              gimp_test_image_teardown);


typedef struct
{
  GimpImage *image;
} GimpTestFixture;


static void gimp_test_image_setup    (GimpTestFixture *fixture,
                                      gconstpointer    data);
static void gimp_test_image_teardown (GimpTestFixture *fixture,
                                      gconstpointer    data);


static Gimp *gimp = NULL;


/**
 * gimp_test_image_setup:
 * @fixture:
 * @data:
 *
 * Test fixture setup for a single image.
 **/
static void
gimp_test_image_setup (GimpTestFixture *fixture,
                       gconstpointer    data)
{
  fixture->image = gimp_image_new (gimp,
                                   GIMP_TEST_IMAGE_SIZE,
                                   GIMP_TEST_IMAGE_SIZE,
                                   GIMP_RGB);
}

/**
 * gimp_test_image_teardown:
 * @fixture:
 * @data:
 *
 * Test fixture teardown for a single image.
 **/
static void
gimp_test_image_teardown (GimpTestFixture *fixture,
                          gconstpointer    data)
{
  g_object_unref (fixture->image);
}

/**
 * add_layer:
 * @fixture:
 * @data:
 *
 * Super basic test that makes sure we can add a layer.
 **/
static void
add_layer (GimpTestFixture *fixture,
           gconstpointer    data)
{
  GimpImage *image = fixture->image;
  GimpLayer *layer = gimp_layer_new (image,
                                     GIMP_TEST_IMAGE_SIZE,
                                     GIMP_TEST_IMAGE_SIZE,
                                     GIMP_RGBA_IMAGE,
                                     "Test Layer",
                                     1.0,
                                     GIMP_NORMAL_MODE);

  g_assert_cmpint (gimp_image_get_n_layers (image), ==, 0);

  gimp_image_add_layer (image,
                        layer,
                        GIMP_IMAGE_ACTIVE_PARENT,
                        0,
                        FALSE);

  g_assert_cmpint (gimp_image_get_n_layers (image), ==, 1);
}

int
main (int    argc,
      char **argv)
{
  g_thread_init (NULL);
  g_type_init ();
  g_test_init (&argc, &argv, NULL);

  gimp_test_utils_set_gimp2_directory ("GIMP_TESTING_ABS_TOP_SRCDIR",
                                       "app/tests/gimpdir");

  /* We share the same application instance across all tests */
  gimp = gimp_init_for_testing (TRUE);

  /* Add tests */
  ADD_TEST (add_layer);

  /* Run the tests and return status */
  return g_test_run ();
}