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

#include <string.h>

#include <glib/gstdio.h>

#include <gegl.h>

#include <gtk/gtk.h>

#include "libgimpbase/gimpbase.h"

#include "widgets/widgets-types.h"

#include "widgets/gimpuimanager.h"

#include "core/gimp.h"
#include "core/gimpchannel.h"
#include "core/gimpchannel-select.h"
#include "core/gimpdrawable.h"
#include "core/gimpgrid.h"
#include "core/gimpgrouplayer.h"
#include "core/gimpguide.h"
#include "core/gimpimage.h"
#include "core/gimpimage-grid.h"
#include "core/gimpimage-guides.h"
#include "core/gimpimage-sample-points.h"
#include "core/gimplayer.h"
#include "core/gimpsamplepoint.h"
#include "core/gimpselection.h"

#include "vectors/gimpanchor.h"
#include "vectors/gimpbezierstroke.h"
#include "vectors/gimpvectors.h"

#include "file/file-open.h"
#include "file/file-procedure.h"
#include "file/file-save.h"

#include "plug-in/gimppluginmanager.h"

#include "tests.h"

#include "gimp-app-test-utils.h"


#define GIMP_MAINIMAGE_WIDTH            100
#define GIMP_MAINIMAGE_HEIGHT           90
#define GIMP_MAINIMAGE_TYPE             GIMP_RGB

#define GIMP_MAINIMAGE_LAYER1_NAME      "layer1"
#define GIMP_MAINIMAGE_LAYER1_WIDTH     50
#define GIMP_MAINIMAGE_LAYER1_HEIGHT    51
#define GIMP_MAINIMAGE_LAYER1_TYPE      GIMP_RGBA_IMAGE
#define GIMP_MAINIMAGE_LAYER1_OPACITY   1.0
#define GIMP_MAINIMAGE_LAYER1_MODE      GIMP_NORMAL_MODE

#define GIMP_MAINIMAGE_LAYER2_NAME      "layer2"
#define GIMP_MAINIMAGE_LAYER2_WIDTH     25
#define GIMP_MAINIMAGE_LAYER2_HEIGHT    251
#define GIMP_MAINIMAGE_LAYER2_TYPE      GIMP_RGB_IMAGE
#define GIMP_MAINIMAGE_LAYER2_OPACITY   0.0
#define GIMP_MAINIMAGE_LAYER2_MODE      GIMP_MULTIPLY_MODE

#define GIMP_MAINIMAGE_GROUP1_NAME      "group1"

#define GIMP_MAINIMAGE_LAYER3_NAME      "layer3"

#define GIMP_MAINIMAGE_LAYER4_NAME      "layer4"

#define GIMP_MAINIMAGE_GROUP2_NAME      "group2"

#define GIMP_MAINIMAGE_LAYER5_NAME      "layer5"

#define GIMP_MAINIMAGE_VGUIDE1_POS      42
#define GIMP_MAINIMAGE_VGUIDE2_POS      82
#define GIMP_MAINIMAGE_HGUIDE1_POS      3
#define GIMP_MAINIMAGE_HGUIDE2_POS      4

#define GIMP_MAINIMAGE_SAMPLEPOINT1_X   10
#define GIMP_MAINIMAGE_SAMPLEPOINT1_Y   12
#define GIMP_MAINIMAGE_SAMPLEPOINT2_X   41
#define GIMP_MAINIMAGE_SAMPLEPOINT2_Y   49

#define GIMP_MAINIMAGE_RESOLUTIONX      400
#define GIMP_MAINIMAGE_RESOLUTIONY      410

#define GIMP_MAINIMAGE_PARASITE_NAME    "test-parasite"
#define GIMP_MAINIMAGE_PARASITE_DATA    "foo"
#define GIMP_MAINIMAGE_PARASITE_SIZE    4                /* 'f' 'o' 'o' '\0' */

#define GIMP_MAINIMAGE_COMMENT          "Created with code from "\
                                        "app/tests/test-xcf.c in the GIMP "\
                                        "source tree, i.e. it was not created "\
                                        "manually and may thus look weird if "\
                                        "opened and inspected in GIMP."

#define GIMP_MAINIMAGE_UNIT             GIMP_UNIT_PICA

#define GIMP_MAINIMAGE_GRIDXSPACING     25.0
#define GIMP_MAINIMAGE_GRIDYSPACING     27.0

#define GIMP_MAINIMAGE_CHANNEL1_NAME    "channel1"
#define GIMP_MAINIMAGE_CHANNEL1_WIDTH   GIMP_MAINIMAGE_WIDTH
#define GIMP_MAINIMAGE_CHANNEL1_HEIGHT  GIMP_MAINIMAGE_HEIGHT
#define GIMP_MAINIMAGE_CHANNEL1_COLOR   { 1.0, 0.0, 1.0, 1.0 }

#define GIMP_MAINIMAGE_SELECTION_X      5
#define GIMP_MAINIMAGE_SELECTION_Y      6
#define GIMP_MAINIMAGE_SELECTION_W      7
#define GIMP_MAINIMAGE_SELECTION_H      8

#define GIMP_MAINIMAGE_VECTORS1_NAME    "vectors1"
#define GIMP_MAINIMAGE_VECTORS1_COORDS  { { 11.0, 12.0, /* pad zeroes */ },\
                                          { 21.0, 22.0, /* pad zeroes */ },\
                                          { 31.0, 32.0, /* pad zeroes */ }, }

#define GIMP_MAINIMAGE_VECTORS2_NAME    "vectors2"
#define GIMP_MAINIMAGE_VECTORS2_COORDS  { { 911.0, 912.0, /* pad zeroes */ },\
                                          { 921.0, 922.0, /* pad zeroes */ },\
                                          { 931.0, 932.0, /* pad zeroes */ }, }

#define ADD_TEST(function) \
  g_test_add_data_func ("/gimp-xcf/" #function, gimp, function);


GimpImage        * gimp_test_load_image                        (Gimp            *gimp,
                                                                const gchar     *uri);
static void        gimp_write_and_read_file                    (Gimp            *gimp,
                                                                gboolean         with_unusual_stuff,
                                                                gboolean         compat_paths,
                                                                gboolean         use_gimp_2_8_features);
static GimpImage * gimp_create_mainimage                       (Gimp            *gimp,
                                                                gboolean         with_unusual_stuff,
                                                                gboolean         compat_paths,
                                                                gboolean         use_gimp_2_8_features);
static void        gimp_assert_mainimage                       (GimpImage       *image,
                                                                gboolean         with_unusual_stuff,
                                                                gboolean         compat_paths,
                                                                gboolean         use_gimp_2_8_features);


/**
 * write_and_read_gimp_2_6_format:
 * @data:
 *
 * Do a write and read test on a file that could as well be
 * constructed with GIMP 2.6.
 **/
static void
write_and_read_gimp_2_6_format (gconstpointer data)
{
  Gimp *gimp = GIMP (data);

  gimp_write_and_read_file (gimp,
                            FALSE /*with_unusual_stuff*/,
                            FALSE /*compat_paths*/,
                            FALSE /*use_gimp_2_8_features*/);
}

/**
 * write_and_read_gimp_2_6_format_unusual:
 * @data:
 *
 * Do a write and read test on a file that could as well be
 * constructed with GIMP 2.6, and make it unusual, like compatible
 * vectors and with a floating selection.
 **/
static void
write_and_read_gimp_2_6_format_unusual (gconstpointer data)
{
  Gimp *gimp = GIMP (data);

  gimp_write_and_read_file (gimp,
                            TRUE /*with_unusual_stuff*/,
                            TRUE /*compat_paths*/,
                            FALSE /*use_gimp_2_8_features*/);
}

/**
 * load_gimp_2_6_file:
 * @data:
 *
 * Loads a file created with GIMP 2.6 and makes sure it loaded as
 * expected.
 **/
static void
load_gimp_2_6_file (gconstpointer data)
{
  Gimp      *gimp  = GIMP (data);
  GimpImage *image = NULL;
  gchar     *uri   = NULL;

  uri = g_build_filename (g_getenv ("GIMP_TESTING_ABS_TOP_SRCDIR"),
                          "app/tests/files/gimp-2-6-file.xcf",
                          NULL);

  image = gimp_test_load_image (gimp, uri);

  /* The image file was constructed by running
   * gimp_write_and_read_file (FALSE, FALSE) in GIMP 2.6 by
   * copy-pasting the code to GIMP 2.6 and adapting it to changes in
   * the core API, so we can use gimp_assert_mainimage() to make sure
   * the file was loaded successfully.
   */
  gimp_assert_mainimage (image,
                         FALSE /*with_unusual_stuff*/,
                         FALSE /*compat_paths*/,
                         FALSE /*use_gimp_2_8_features*/);
}

/**
 * write_and_read_gimp_2_8_format:
 * @data:
 *
 * Writes an XCF file that uses GIMP 2.8 features such as layer
 * groups, then reads the file and make sure no relevant information
 * was lost.
 **/
static void
write_and_read_gimp_2_8_format (gconstpointer data)
{
  Gimp *gimp = GIMP (data);

  gimp_write_and_read_file (gimp,
                            FALSE /*with_unusual_stuff*/,
                            FALSE /*compat_paths*/,
                            TRUE /*use_gimp_2_8_features*/);
}

GimpImage *
gimp_test_load_image (Gimp        *gimp,
                      const gchar *uri)
{
  GimpPlugInProcedure *proc     = NULL;
  GimpImage           *image    = NULL;
  GimpPDBStatusType    not_used = 0;

  proc = file_procedure_find (gimp->plug_in_manager->load_procs,
                              uri,
                              NULL /*error*/);
  image = file_open_image (gimp,
                           gimp_get_user_context (gimp),
                           NULL /*progress*/,
                           uri,
                           "irrelevant" /*entered_filename*/,
                           FALSE /*as_new*/,
                           proc,
                           GIMP_RUN_NONINTERACTIVE,
                           &not_used /*status*/,
                           NULL /*mime_type*/,
                           NULL /*error*/);

  return image;
}

/**
 * gimp_write_and_read_file:
 * @gimp:                  #Gimp instance
 * @with_unusual_stuff:    toggles whether to create the image with unusual
 *                         stuff, currently only a floating selection
 * @compat_paths:          toggles whether to use old style paths
 *                         (before GIMP 1.3)
 * @use_gimp_2_8_features: toggles whether to use GIMP 2.8 feature,
 *                         currently layer groups
 *
 * Constructs the main test image and asserts its state, writes it to
 * a file, reads the image from the file, and asserts the state of the
 * loaded file. The function takes various parameters so the same
 * function can be used for different formats.
 **/
static void
gimp_write_and_read_file (Gimp     *gimp,
                          gboolean  with_unusual_stuff,
                          gboolean  compat_paths,
                          gboolean  use_gimp_2_8_features)
{
  GimpImage           *image        = NULL;
  GimpImage           *loaded_image = NULL;
  GimpPlugInProcedure *proc         = NULL;
  gchar               *uri          = NULL;

  /* Create the image */
  image = gimp_create_mainimage (gimp,
                                 with_unusual_stuff,
                                 compat_paths,
                                 use_gimp_2_8_features);

  /* Assert valid state */
  gimp_assert_mainimage (image,
                         with_unusual_stuff,
                         compat_paths,
                         use_gimp_2_8_features);

  /* Write to file */
  uri  = g_build_filename (g_get_tmp_dir (), "gimp-test.xcf", NULL);
  proc = file_procedure_find (image->gimp->plug_in_manager->save_procs,
                              uri,
                              NULL /*error*/);
  file_save (gimp,
             image,
             NULL /*progress*/,
             uri,
             proc,
             GIMP_RUN_NONINTERACTIVE,
             FALSE /*change_saved_state*/,
             FALSE /*export_backward*/,
             FALSE /*export_forward*/,
             NULL /*error*/);

  /* Load from file */
  loaded_image = gimp_test_load_image (image->gimp, uri);

  /* Assert on the loaded file. If success, it means that there is no
   * significant information loss when we wrote the image to a file
   * and loaded it again
   */
  gimp_assert_mainimage (loaded_image,
                         with_unusual_stuff,
                         compat_paths,
                         use_gimp_2_8_features);

  g_unlink (uri);
  g_free (uri);
}

/**
 * gimp_create_mainimage:
 * gimp_write_and_read_file:
 * @gimp:                  #Gimp instance
 * @with_unusual_stuff:    toggles whether to create the image with unusual
 *                         stuff, currently only a floating selection
 * @compat_paths:          toggles whether to use old style paths
 *                         (before GIMP 1.3)
 * @use_gimp_2_8_features: toggles whether to use GIMP 2.8 feature,
 *                         currently layer groups
 *
 * Creates the main test image, i.e. the image that we use for most of
 * our XCF testing purposes.
 *
 * Returns: The #GimpImage
 **/
static GimpImage *
gimp_create_mainimage (Gimp     *gimp,
                       gboolean  with_unusual_stuff,
                       gboolean  compat_paths,
                       gboolean  use_gimp_2_8_features)
{
  GimpImage     *image             = NULL;
  GimpLayer     *layer             = NULL;
  GimpParasite  *parasite          = NULL;
  GimpGrid      *grid              = NULL;
  GimpChannel   *channel           = NULL;
  GimpRGB        channel_color     = GIMP_MAINIMAGE_CHANNEL1_COLOR;
  GimpChannel   *selection         = NULL;
  GimpVectors   *vectors           = NULL;
  GimpCoords     vectors1_coords[] = GIMP_MAINIMAGE_VECTORS1_COORDS;
  GimpCoords     vectors2_coords[] = GIMP_MAINIMAGE_VECTORS2_COORDS;
  GimpStroke    *stroke            = NULL;
  GimpLayerMask *layer_mask        = NULL;

  /* Image size and type */
  image = gimp_image_new (gimp,
                          GIMP_MAINIMAGE_WIDTH,
                          GIMP_MAINIMAGE_HEIGHT,
                          GIMP_MAINIMAGE_TYPE);

  /* Layers */
  layer = gimp_layer_new (image,
                          GIMP_MAINIMAGE_LAYER1_WIDTH,
                          GIMP_MAINIMAGE_LAYER1_HEIGHT,
                          GIMP_MAINIMAGE_LAYER1_TYPE,
                          GIMP_MAINIMAGE_LAYER1_NAME,
                          GIMP_MAINIMAGE_LAYER1_OPACITY,
                          GIMP_MAINIMAGE_LAYER1_MODE);
  gimp_image_add_layer (image,
                        layer,
                        NULL,
                        0,
                        FALSE/*push_undo*/);
  layer = gimp_layer_new (image,
                          GIMP_MAINIMAGE_LAYER2_WIDTH,
                          GIMP_MAINIMAGE_LAYER2_HEIGHT,
                          GIMP_MAINIMAGE_LAYER2_TYPE,
                          GIMP_MAINIMAGE_LAYER2_NAME,
                          GIMP_MAINIMAGE_LAYER2_OPACITY,
                          GIMP_MAINIMAGE_LAYER2_MODE);
  gimp_image_add_layer (image,
                        layer,
                        NULL,
                        0,
                        FALSE /*push_undo*/);

  /* Layer mask */
  layer_mask = gimp_layer_create_mask (layer,
                                       GIMP_ADD_BLACK_MASK,
                                       NULL /*channel*/);
  gimp_layer_add_mask (layer,
                       layer_mask,
                       FALSE /*push_undo*/,
                       NULL /*error*/);

  /* Image compression type
   *
   * We don't do any explicit test, only implicit when we read tile
   * data in other tests
   */

  /* Guides, note we add them in reversed order */
  gimp_image_add_hguide (image,
                         GIMP_MAINIMAGE_HGUIDE2_POS,
                         FALSE /*push_undo*/);
  gimp_image_add_hguide (image,
                         GIMP_MAINIMAGE_HGUIDE1_POS,
                         FALSE /*push_undo*/);
  gimp_image_add_vguide (image,
                         GIMP_MAINIMAGE_VGUIDE2_POS,
                         FALSE /*push_undo*/);
  gimp_image_add_vguide (image,
                         GIMP_MAINIMAGE_VGUIDE1_POS,
                         FALSE /*push_undo*/);


  /* Sample points */
  gimp_image_add_sample_point_at_pos (image,
                                      GIMP_MAINIMAGE_SAMPLEPOINT1_X,
                                      GIMP_MAINIMAGE_SAMPLEPOINT1_Y,
                                      FALSE /*push_undo*/);
  gimp_image_add_sample_point_at_pos (image,
                                      GIMP_MAINIMAGE_SAMPLEPOINT2_X,
                                      GIMP_MAINIMAGE_SAMPLEPOINT2_Y,
                                      FALSE /*push_undo*/);

  /* Tattoo
   * We don't bother testing this, not yet at least
   */

  /* Resolution */
  gimp_image_set_resolution (image,
                             GIMP_MAINIMAGE_RESOLUTIONX,
                             GIMP_MAINIMAGE_RESOLUTIONY);


  /* Parasites */
  parasite = gimp_parasite_new (GIMP_MAINIMAGE_PARASITE_NAME,
                                GIMP_PARASITE_PERSISTENT,
                                GIMP_MAINIMAGE_PARASITE_SIZE,
                                GIMP_MAINIMAGE_PARASITE_DATA);
  gimp_image_parasite_attach (image,
                              parasite);
  gimp_parasite_free (parasite);
  parasite = gimp_parasite_new ("gimp-comment",
                                GIMP_PARASITE_PERSISTENT,
                                strlen (GIMP_MAINIMAGE_COMMENT) + 1,
                                GIMP_MAINIMAGE_COMMENT);
  gimp_image_parasite_attach (image, parasite);
  gimp_parasite_free (parasite);


  /* Unit */
  gimp_image_set_unit (image,
                       GIMP_MAINIMAGE_UNIT);

  /* Grid */
  grid = g_object_new (GIMP_TYPE_GRID,
                       "xspacing", GIMP_MAINIMAGE_GRIDXSPACING,
                       "yspacing", GIMP_MAINIMAGE_GRIDYSPACING,
                       NULL);
  gimp_image_set_grid (image,
                       grid,
                       FALSE /*push_undo*/);
  g_object_unref (grid);

  /* Channel */
  channel = gimp_channel_new (image,
                              GIMP_MAINIMAGE_CHANNEL1_WIDTH,
                              GIMP_MAINIMAGE_CHANNEL1_HEIGHT,
                              GIMP_MAINIMAGE_CHANNEL1_NAME,
                              &channel_color);
  gimp_image_add_channel (image,
                          channel,
                          NULL,
                          -1,
                          FALSE /*push_undo*/);

  /* Selection */
  selection = gimp_image_get_mask (image);
  gimp_channel_select_rectangle (selection,
                                 GIMP_MAINIMAGE_SELECTION_X,
                                 GIMP_MAINIMAGE_SELECTION_Y,
                                 GIMP_MAINIMAGE_SELECTION_W,
                                 GIMP_MAINIMAGE_SELECTION_H,
                                 GIMP_CHANNEL_OP_REPLACE,
                                 FALSE /*feather*/,
                                 0.0 /*feather_radius_x*/,
                                 0.0 /*feather_radius_y*/,
                                 FALSE /*push_undo*/);

  /* Vectors 1 */
  vectors = gimp_vectors_new (image,
                              GIMP_MAINIMAGE_VECTORS1_NAME);
  /* The XCF file can save vectors in two kind of ways, one old way
   * and a new way. Parameterize the way so we can test both variants,
   * i.e. gimp_vectors_compat_is_compatible() must return both TRUE
   * and FALSE.
   */
  if (! compat_paths)
    {
      gimp_item_set_visible (GIMP_ITEM (vectors),
                             TRUE,
                             FALSE /*push_undo*/);
    }
  /* TODO: Add test for non-closed stroke. The order of the anchor
   * points changes for open strokes, so it's boring to test
   */
  stroke = gimp_bezier_stroke_new_from_coords (vectors1_coords,
                                               G_N_ELEMENTS (vectors1_coords),
                                               TRUE /*closed*/);
  gimp_vectors_stroke_add (vectors, stroke);
  gimp_image_add_vectors (image,
                          vectors,
                          NULL /*parent*/,
                          -1 /*position*/,
                          FALSE /*push_undo*/);

  /* Vectors 2 */
  vectors = gimp_vectors_new (image,
                              GIMP_MAINIMAGE_VECTORS2_NAME);

  stroke = gimp_bezier_stroke_new_from_coords (vectors2_coords,
                                               G_N_ELEMENTS (vectors2_coords),
                                               TRUE /*closed*/);
  gimp_vectors_stroke_add (vectors, stroke);
  gimp_image_add_vectors (image,
                          vectors,
                          NULL /*parent*/,
                          -1 /*position*/,
                          FALSE /*push_undo*/);

  /* Some of these things are pretty unusual, parameterize the
   * inclusion of this in the written file so we can do our test both
   * with and without
   */
  if (with_unusual_stuff)
    {
      /* Floating selection */
      gimp_selection_float (GIMP_SELECTION (gimp_image_get_mask (image)),
                            gimp_image_get_active_drawable (image),
                            gimp_get_user_context (gimp),
                            TRUE /*cut_image*/,
                            0 /*off_x*/,
                            0 /*off_y*/,
                            NULL /*error*/);
    }

  /* Adds stuff like layer groups */
  if (use_gimp_2_8_features)
    {
      GimpLayer *parent;

      /* Add a layer group and some layers:
       *
       *  group1
       *    layer3
       *    layer4
       *    group2
       *      layer5
       */

      /* group1 */
      layer = gimp_group_layer_new (image);
      gimp_object_set_name (GIMP_OBJECT (layer), GIMP_MAINIMAGE_GROUP1_NAME);
      gimp_image_add_layer (image,
                            layer,
                            NULL /*parent*/,
                            -1 /*position*/,
                            FALSE /*push_undo*/);
      parent = layer;

      /* layer3 */
      layer = gimp_layer_new (image,
                              GIMP_MAINIMAGE_LAYER1_WIDTH,
                              GIMP_MAINIMAGE_LAYER1_HEIGHT,
                              GIMP_MAINIMAGE_LAYER1_TYPE,
                              GIMP_MAINIMAGE_LAYER3_NAME,
                              GIMP_MAINIMAGE_LAYER1_OPACITY,
                              GIMP_MAINIMAGE_LAYER1_MODE);
      gimp_image_add_layer (image,
                            layer,
                            parent,
                            -1 /*position*/,
                            FALSE /*push_undo*/);

      /* layer4 */
      layer = gimp_layer_new (image,
                              GIMP_MAINIMAGE_LAYER1_WIDTH,
                              GIMP_MAINIMAGE_LAYER1_HEIGHT,
                              GIMP_MAINIMAGE_LAYER1_TYPE,
                              GIMP_MAINIMAGE_LAYER4_NAME,
                              GIMP_MAINIMAGE_LAYER1_OPACITY,
                              GIMP_MAINIMAGE_LAYER1_MODE);
      gimp_image_add_layer (image,
                            layer,
                            parent,
                            -1 /*position*/,
                            FALSE /*push_undo*/);

      /* group2 */
      layer = gimp_group_layer_new (image);
      gimp_object_set_name (GIMP_OBJECT (layer), GIMP_MAINIMAGE_GROUP2_NAME);
      gimp_image_add_layer (image,
                            layer,
                            parent,
                            -1 /*position*/,
                            FALSE /*push_undo*/);
      parent = layer;

      /* layer5 */
      layer = gimp_layer_new (image,
                              GIMP_MAINIMAGE_LAYER1_WIDTH,
                              GIMP_MAINIMAGE_LAYER1_HEIGHT,
                              GIMP_MAINIMAGE_LAYER1_TYPE,
                              GIMP_MAINIMAGE_LAYER5_NAME,
                              GIMP_MAINIMAGE_LAYER1_OPACITY,
                              GIMP_MAINIMAGE_LAYER1_MODE);
      gimp_image_add_layer (image,
                            layer,
                            parent,
                            -1 /*position*/,
                            FALSE /*push_undo*/);
    }

  /* Todo, should be tested somehow:
   *
   * - Color maps
   * - Custom user units
   * - Text layers
   * - Layer parasites
   * - Channel parasites
   * - Different tile compression methods
   */

  return image;
}

static void
gimp_assert_vectors (GimpImage   *image,
                     const gchar *name,
                     GimpCoords   coords[],
                     gsize        coords_size,
                     gboolean     visible)
{
  GimpVectors *vectors        = NULL;
  GimpStroke  *stroke         = NULL;
  GArray      *control_points = NULL;
  gboolean     closed         = FALSE;
  gint         i              = 0;

  vectors = gimp_image_get_vectors_by_name (image, name);
  stroke = gimp_vectors_stroke_get_next (vectors, NULL);
  g_assert (stroke != NULL);
  control_points = gimp_stroke_control_points_get (stroke,
                                                   &closed);
  g_assert (closed);
  g_assert_cmpint (control_points->len,
                   ==,
                   coords_size);
  for (i = 0; i < control_points->len; i++)
    {
      g_assert_cmpint (coords[i].x,
                       ==,
                       g_array_index (control_points,
                                      GimpAnchor,
                                      i).position.x);
      g_assert_cmpint (coords[i].y,
                       ==,
                       g_array_index (control_points,
                                      GimpAnchor,
                                      i).position.y);
    }

  g_assert (gimp_item_get_visible (GIMP_ITEM (vectors)) ? TRUE : FALSE ==
            visible ? TRUE : FALSE);
}

/**
 * gimp_assert_mainimage:
 * @image:
 *
 * Verifies that the passed #GimpImage contains all the information
 * that was put in it by gimp_create_mainimage().
 **/
static void
gimp_assert_mainimage (GimpImage *image,
                       gboolean   with_unusual_stuff,
                       gboolean   compat_paths,
                       gboolean   use_gimp_2_8_features)
{
  const GimpParasite *parasite               = NULL;
  GimpLayer          *layer                  = NULL;
  GList              *iter                   = NULL;
  GimpGuide          *guide                  = NULL;
  GimpSamplePoint    *sample_point           = NULL;
  gdouble             xres                   = 0.0;
  gdouble             yres                   = 0.0;
  GimpGrid           *grid                   = NULL;
  gdouble             xspacing               = 0.0;
  gdouble             yspacing               = 0.0;
  GimpChannel        *channel                = NULL;
  GimpRGB             expected_channel_color = GIMP_MAINIMAGE_CHANNEL1_COLOR;
  GimpRGB             actual_channel_color   = { 0, };
  GimpChannel        *selection              = NULL;
  gint                x1                     = -1;
  gint                y1                     = -1;
  gint                x2                     = -1;
  gint                y2                     = -1;
  gint                w                      = -1;
  gint                h                      = -1;
  GimpCoords          vectors1_coords[]      = GIMP_MAINIMAGE_VECTORS1_COORDS;
  GimpCoords          vectors2_coords[]      = GIMP_MAINIMAGE_VECTORS2_COORDS;

  /* Image size and type */
  g_assert_cmpint (gimp_image_get_width (image),
                   ==,
                   GIMP_MAINIMAGE_WIDTH);
  g_assert_cmpint (gimp_image_get_height (image),
                   ==,
                   GIMP_MAINIMAGE_HEIGHT);
  g_assert_cmpint (gimp_image_base_type (image),
                   ==,
                   GIMP_MAINIMAGE_TYPE);

  /* Layers */
  layer = gimp_image_get_layer_by_name (image,
                                        GIMP_MAINIMAGE_LAYER1_NAME);
  g_assert_cmpint (gimp_item_get_width (GIMP_ITEM (layer)),
                   ==,
                   GIMP_MAINIMAGE_LAYER1_WIDTH);
  g_assert_cmpint (gimp_item_get_height (GIMP_ITEM (layer)),
                   ==,
                   GIMP_MAINIMAGE_LAYER1_HEIGHT);
  g_assert_cmpint (gimp_drawable_type (GIMP_DRAWABLE (layer)),
                   ==,
                   GIMP_MAINIMAGE_LAYER1_TYPE);
  g_assert_cmpstr (gimp_object_get_name (GIMP_DRAWABLE (layer)),
                   ==,
                   GIMP_MAINIMAGE_LAYER1_NAME);
  g_assert_cmpfloat (gimp_layer_get_opacity (layer),
                     ==,
                     GIMP_MAINIMAGE_LAYER1_OPACITY);
  g_assert_cmpint (gimp_layer_get_mode (layer),
                   ==,
                   GIMP_MAINIMAGE_LAYER1_MODE);
  layer = gimp_image_get_layer_by_name (image,
                                        GIMP_MAINIMAGE_LAYER2_NAME);
  g_assert_cmpint (gimp_item_get_width (GIMP_ITEM (layer)),
                   ==,
                   GIMP_MAINIMAGE_LAYER2_WIDTH);
  g_assert_cmpint (gimp_item_get_height (GIMP_ITEM (layer)),
                   ==,
                   GIMP_MAINIMAGE_LAYER2_HEIGHT);
  g_assert_cmpint (gimp_drawable_type (GIMP_DRAWABLE (layer)),
                   ==,
                   GIMP_MAINIMAGE_LAYER2_TYPE);
  g_assert_cmpstr (gimp_object_get_name (GIMP_DRAWABLE (layer)),
                   ==,
                   GIMP_MAINIMAGE_LAYER2_NAME);
  g_assert_cmpfloat (gimp_layer_get_opacity (layer),
                     ==,
                     GIMP_MAINIMAGE_LAYER2_OPACITY);
  g_assert_cmpint (gimp_layer_get_mode (layer),
                   ==,
                   GIMP_MAINIMAGE_LAYER2_MODE);

  /* Guides, note that we rely on internal ordering */
  iter = gimp_image_get_guides (image);
  g_assert (iter != NULL);
  guide = GIMP_GUIDE (iter->data);
  g_assert_cmpint (gimp_guide_get_position (guide),
                   ==,
                   GIMP_MAINIMAGE_VGUIDE1_POS);
  iter = g_list_next (iter);
  g_assert (iter != NULL);
  guide = GIMP_GUIDE (iter->data);
  g_assert_cmpint (gimp_guide_get_position (guide),
                   ==,
                   GIMP_MAINIMAGE_VGUIDE2_POS);
  iter = g_list_next (iter);
  g_assert (iter != NULL);
  guide = GIMP_GUIDE (iter->data);
  g_assert_cmpint (gimp_guide_get_position (guide),
                   ==,
                   GIMP_MAINIMAGE_HGUIDE1_POS);
  iter = g_list_next (iter);
  g_assert (iter != NULL);
  guide = GIMP_GUIDE (iter->data);
  g_assert_cmpint (gimp_guide_get_position (guide),
                   ==,
                   GIMP_MAINIMAGE_HGUIDE2_POS);
  iter = g_list_next (iter);
  g_assert (iter == NULL);

  /* Sample points, we rely on the same ordering as when we added
   * them, although this ordering is not a necessaity
   */
  iter = gimp_image_get_sample_points (image);
  g_assert (iter != NULL);
  sample_point = (GimpSamplePoint *) iter->data;
  g_assert_cmpint (sample_point->x,
                   ==,
                   GIMP_MAINIMAGE_SAMPLEPOINT1_X);
  g_assert_cmpint (sample_point->y,
                   ==,
                   GIMP_MAINIMAGE_SAMPLEPOINT1_Y);
  iter = g_list_next (iter);
  g_assert (iter != NULL);
  sample_point = (GimpSamplePoint *) iter->data;
  g_assert_cmpint (sample_point->x,
                   ==,
                   GIMP_MAINIMAGE_SAMPLEPOINT2_X);
  g_assert_cmpint (sample_point->y,
                   ==,
                   GIMP_MAINIMAGE_SAMPLEPOINT2_Y);
  iter = g_list_next (iter);
  g_assert (iter == NULL);

  /* Resolution */
  gimp_image_get_resolution (image, &xres, &yres);
  g_assert_cmpint (xres,
                   ==,
                   GIMP_MAINIMAGE_RESOLUTIONX);
  g_assert_cmpint (yres,
                   ==,
                   GIMP_MAINIMAGE_RESOLUTIONY);

  /* Parasites */
  parasite = gimp_image_parasite_find (image,
                                       GIMP_MAINIMAGE_PARASITE_NAME);
  g_assert_cmpint (gimp_parasite_data_size (parasite),
                   ==,
                   GIMP_MAINIMAGE_PARASITE_SIZE);
  g_assert_cmpstr (gimp_parasite_data (parasite),
                   ==,
                   GIMP_MAINIMAGE_PARASITE_DATA);
  parasite = gimp_image_parasite_find (image,
                                       "gimp-comment");
  g_assert_cmpint (gimp_parasite_data_size (parasite),
                   ==,
                   strlen (GIMP_MAINIMAGE_COMMENT) + 1);
  g_assert_cmpstr (gimp_parasite_data (parasite),
                   ==,
                   GIMP_MAINIMAGE_COMMENT);

  /* Unit */
  g_assert_cmpint (gimp_image_get_unit (image),
                   ==,
                   GIMP_MAINIMAGE_UNIT);

  /* Grid */
  grid = gimp_image_get_grid (image);
  g_object_get (grid,
                "xspacing", &xspacing,
                "yspacing", &yspacing,
                NULL);
  g_assert_cmpint (xspacing,
                   ==,
                   GIMP_MAINIMAGE_GRIDXSPACING);
  g_assert_cmpint (yspacing,
                   ==,
                   GIMP_MAINIMAGE_GRIDYSPACING);


  /* Channel */
  channel = gimp_image_get_channel_by_name (image,
                                            GIMP_MAINIMAGE_CHANNEL1_NAME);
  gimp_channel_get_color (channel, &actual_channel_color);
  g_assert_cmpint (gimp_item_get_width (GIMP_ITEM (channel)),
                   ==,
                   GIMP_MAINIMAGE_CHANNEL1_WIDTH);
  g_assert_cmpint (gimp_item_get_height (GIMP_ITEM (channel)),
                   ==,
                   GIMP_MAINIMAGE_CHANNEL1_HEIGHT);
  g_assert (memcmp (&expected_channel_color,
                    &actual_channel_color,
                    sizeof (GimpRGB)) == 0);

  /* Selection, if the image contains unusual stuff it contains a
   * floating select, and when floating a selection, the selection
   * mask is cleared, so don't test for the presence of the selection
   * mask in that case
   */
  if (! with_unusual_stuff)
    {
      selection = gimp_image_get_mask (image);
      gimp_channel_bounds (selection, &x1, &y1, &x2, &y2);
      w = x2 - x1;
      h = y2 - y1;
      g_assert_cmpint (x1,
                       ==,
                       GIMP_MAINIMAGE_SELECTION_X);
      g_assert_cmpint (y1,
                       ==,
                       GIMP_MAINIMAGE_SELECTION_Y);
      g_assert_cmpint (w,
                       ==,
                       GIMP_MAINIMAGE_SELECTION_W);
      g_assert_cmpint (h,
                       ==,
                       GIMP_MAINIMAGE_SELECTION_H);
    }

  /* Vectors 1 */
  gimp_assert_vectors (image,
                       GIMP_MAINIMAGE_VECTORS1_NAME,
                       vectors1_coords,
                       G_N_ELEMENTS (vectors1_coords),
                       ! compat_paths /*visible*/);

  /* Vectors 2 (always visible FALSE) */
  gimp_assert_vectors (image,
                       GIMP_MAINIMAGE_VECTORS2_NAME,
                       vectors2_coords,
                       G_N_ELEMENTS (vectors2_coords),
                       FALSE /*visible*/);

  if (with_unusual_stuff)
    g_assert (gimp_image_get_floating_selection (image) != NULL);
  else /* if (! with_unusual_stuff) */
    g_assert (gimp_image_get_floating_selection (image) == NULL);

  if (use_gimp_2_8_features)
    {
      /* Only verify the parent relationships, the layer attributes
       * are tested above
       */
      GimpItem *group1 = GIMP_ITEM (gimp_image_get_layer_by_name (image, GIMP_MAINIMAGE_GROUP1_NAME));
      GimpItem *layer3 = GIMP_ITEM (gimp_image_get_layer_by_name (image, GIMP_MAINIMAGE_LAYER3_NAME));
      GimpItem *layer4 = GIMP_ITEM (gimp_image_get_layer_by_name (image, GIMP_MAINIMAGE_LAYER4_NAME));
      GimpItem *group2 = GIMP_ITEM (gimp_image_get_layer_by_name (image, GIMP_MAINIMAGE_GROUP2_NAME));
      GimpItem *layer5 = GIMP_ITEM (gimp_image_get_layer_by_name (image, GIMP_MAINIMAGE_LAYER5_NAME));

      g_assert (gimp_item_get_parent (group1) == NULL);
      g_assert (gimp_item_get_parent (layer3) == group1);
      g_assert (gimp_item_get_parent (layer4) == group1);
      g_assert (gimp_item_get_parent (group2) == group1);
      g_assert (gimp_item_get_parent (layer5) == group2);
    }
}


/**
 * main:
 * @argc:
 * @argv:
 *
 * These tests intend to
 *
 *  - Make sure that we are backwards compatible with files created by
 *    older version of GIMP, i.e. that we can load files from earlier
 *    version of GIMP
 *
 *  - Make sure that the information put into a #GimpImage is not lost
 *    when the #GimpImage is written to a file and then read again
 **/



#include "core/gimpclonelayer.h"
#include "core/gimpfilterlayer.h"
#include "core/gimpcontainer.h"
#include "core/gimppickable.h"
#include "core/gimpprojection.h"
#include "core/gimpimage-undo.h"
#include "core/gimpundostack.h"
#include "core/gimpitemundo.h"
#include "core/gimpcontext.h"
#include "core/gimp-edit.h"
#include "pdb/gimppdb.h"
#include "pdb/gimpprocedure.h"
extern gint gimp_filter_layer_capture_pending(GimpFilterLayer*);
#include "core/gimpprogress.h"
#include "core/legacy-timeline.h"
static void updated(GimpDrawable *d,gint x,gint y,gint w,gint h,gpointer unused) {legacy_timeline(G_OBJECT(d),"drawable-update-signal");}
static GimpLayer *effect(Gimp *g,GimpImage *im,const char *name,gint position,gboolean builtin) {
 const char *pn=builtin?"gimp-brightness-contrast":"plug-in-edge";GimpProcedure *p=gimp_pdb_lookup_procedure(g->pdb,pn);GValueArray *v;GArray *a;guint i;GimpLayer *l;
 g_assert(p);v=gimp_procedure_get_arguments(p);a=g_array_sized_new(FALSE,TRUE,sizeof(GValue),v->n_values);g_array_set_clear_func(a,(GDestroyNotify)g_value_unset);g_array_set_size(a,v->n_values);
 for(i=0;i<v->n_values;i++){GValue *dest=&g_array_index(a,GValue,i);g_value_init(dest,G_VALUE_TYPE(&v->values[i]));g_value_copy(&v->values[i],dest);}
 if(builtin){g_value_set_int(&g_array_index(a,GValue,1),25);g_value_set_int(&g_array_index(a,GValue,2),-10);}else{g_value_set_int(&g_array_index(a,GValue,0),GIMP_RUN_NONINTERACTIVE);g_value_set_double(&g_array_index(a,GValue,3),2);g_value_set_int(&g_array_index(a,GValue,4),1);g_value_set_int(&g_array_index(a,GValue,5),0);}
 l=gimp_filter_layer_new(im,256,256,name,1,GIMP_NORMAL_MODE);g_assert(gimp_image_add_layer(im,l,NULL,position,FALSE));g_signal_connect(l,"update",G_CALLBACK(updated),NULL);gimp_filter_layer_set_procedure(GIMP_FILTER_LAYER(l),pn,a);g_array_unref(a);g_value_array_free(v);return l;
}
static void render(GimpImage *im){guchar px[4];gint x,y;gimp_pickable_flush(GIMP_PICKABLE(gimp_image_get_projection(im)));for(y=0;y<256;y+=64)for(x=0;x<256;x+=64)gimp_pickable_get_pixel_at(GIMP_PICKABLE(gimp_image_get_projection(im)),x,y,px);}
static GimpLayer *late_cancel_target;
static void pump(GimpImage *im,gint milliseconds){gint64 until=g_get_monotonic_time()+milliseconds*1000;while(g_get_monotonic_time()<until){while(g_main_context_iteration(NULL,FALSE));if(late_cancel_target && g_object_get_data(G_OBJECT(late_cancel_target),"timeline-progress-seen")){GimpLayer *l=late_cancel_target;late_cancel_target=NULL;legacy_timeline(G_OBJECT(l),"late-cancel-request");gimp_progress_cancel(GIMP_PROGRESS(l));}render(im);g_usleep(1000);}}
static void cachehash(GimpLayer *l){GChecksum *sum=g_checksum_new(G_CHECKSUM_SHA256);gint x,y;guchar px[4];for(y=0;y<256;y++)for(x=0;x<256;x++){gimp_pickable_get_pixel_at(GIMP_PICKABLE(l),x,y,px);g_checksum_update(sum,px,4);}g_print("CACHE name=%s sha256=%s\n",gimp_object_get_name(l),g_checksum_get_string(sum));g_checksum_free(sum);}
static void state(GimpLayer *l,const char *event){legacy_timeline(G_OBJECT(l),event);g_print("STATE time_us=%" G_GINT64_FORMAT " name=%s pending=%d active=%d attached=%d ordinal=%d\n",g_get_monotonic_time(),gimp_object_get_name(l),gimp_filter_layer_capture_pending(GIMP_FILTER_LAYER(l)),gimp_progress_is_active(GIMP_PROGRESS(l)),gimp_item_is_attached(GIMP_ITEM(l)),GPOINTER_TO_INT(g_object_get_data(G_OBJECT(l),"timeline-ordinal")));}
int main(int argc,char **argv){Gimp *g;GimpImage *im;GimpLayer *src,*top,*bottom=NULL;GimpRGB red={.8,.2,.4,1},blue={.1,.2,.8,1};const char *scene=g_getenv("TIMELINE_SCENE");
 g_thread_init(NULL);g_type_init();gtk_init(&argc,&argv);gimp_test_utils_setup_menus_dir();g=gimp_init_for_gui_testing(TRUE);im=gimp_create_image(g,256,256,GIMP_RGB,FALSE);src=gimp_layer_new(im,256,256,GIMP_RGBA_IMAGE,"source",1,GIMP_NORMAL_MODE);g_assert(gimp_image_add_layer(im,src,NULL,0,FALSE));gimp_drawable_fill(GIMP_DRAWABLE(src),&red,NULL);gimp_drawable_update(GIMP_DRAWABLE(src),0,0,256,256);
 top=effect(g,im,"top",0,!strcmp(scene,"builtin"));g_object_ref(top);render(im);state(top,"after-initial-flush");
 if(!strcmp(scene,"stack")){bottom=effect(g,im,"bottom",1,FALSE);g_object_ref(bottom);gimp_drawable_update(GIMP_DRAWABLE(src),0,0,256,256);state(top,"after-stack-insert");}
 if(!strcmp(scene,"cancel")){legacy_timeline(G_OBJECT(top),"explicit-cancel-request");gimp_progress_cancel(GIMP_PROGRESS(top));state(top,"after-explicit-cancel");}
 if(!strcmp(scene,"detach")){legacy_timeline(G_OBJECT(top),"detach-request");gimp_image_remove_layer(im,top,TRUE,NULL);state(top,"after-detach");}
 if(!strcmp(scene,"latecancel"))late_cancel_target=top;
 pump(im,1500);state(top,"first-window-end");if(bottom)state(bottom,"first-window-end");cachehash(top);
 if(strcmp(scene,"builtin")&&strcmp(scene,"detach")){legacy_timeline(G_OBJECT(src),"edit-source-request");gimp_drawable_fill(GIMP_DRAWABLE(src),&blue,NULL);gimp_drawable_update(GIMP_DRAWABLE(src),0,0,256,256);pump(im,1500);state(top,"second-window-end");if(bottom)state(bottom,"second-window-end");cachehash(top);}
 g_print("TIMELINE_COMPLETE scene=%s\n",scene);return 0;}
