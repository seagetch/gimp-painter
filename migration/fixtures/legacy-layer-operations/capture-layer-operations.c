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
typedef struct {
GimpLayer *p;
const gchar *id;
gboolean alive;
guint updates;
}
 Watch;
static Watch watches[64];
 static gint nw=0;
static const gchar *step="setup";
static guint seq=0;
static const gchar *idof(gpointer p){
gint i;
if(!p)return "NULL";
for(i=0;i<nw;i++)if(watches[i].p==p)return watches[i].id;
return "UNWATCHED";
}
static void dead(gpointer data,GObject *p){
Watch *w=data;
w->alive=FALSE;
g_print("EVENT seq=%u step=%s id=%s event=finalize pointer=%p\n",++seq,step,w->id,p);
}
static void update(GimpDrawable *d,gint x,gint y,gint width,gint height,gpointer data){
Watch *w=data;
w->updates++;
g_print("EVENT seq=%u step=%s id=%s event=update rect=%d,%d,%d,%d\n",++seq,step,w->id,x,y,width,height);
}
static void notify(GObject *o,GParamSpec *ps,gpointer data){
Watch *w=data;
g_print("EVENT seq=%u step=%s id=%s event=notify property=%s\n",++seq,step,w->id,ps->name);
}
static void watch(GimpLayer *p,const gchar *id){
Watch *w=&watches[nw++];
w->p=p;
w->id=id;
w->alive=TRUE;
g_object_weak_ref(G_OBJECT(p),dead,w);
g_signal_connect(p,"update",G_CALLBACK(update),w);
g_signal_connect(p,"notify",G_CALLBACK(notify),w);
g_print("REGISTER id=%s pointer=%p type=%s\n",id,p,G_OBJECT_TYPE_NAME(p));
}
static void pixels(GimpPickable *p,gint w,gint h,gint bpp,const gchar *id){
guchar *buf=g_malloc0(w*h*bpp);
gint x,y;
gchar *f,*hash;
GError *err=NULL;
for(y=0;y<h;y++)for(x=0;x<w;x++)g_assert(gimp_pickable_get_pixel_at(p,x,y,buf+(y*w+x)*bpp));
f=g_strdup_printf("%s/%s.%s.pixels",g_getenv("GIMP_PAINTER_FIXTURE_DIR"),step,id);
g_assert(g_file_set_contents(f,(gchar*)buf,w*h*bpp,&err));
g_assert_no_error(err);
hash=g_compute_checksum_for_data(G_CHECKSUM_SHA256,buf,w*h*bpp);
g_print("PIXELS step=%s id=%s width=%d height=%d bpp=%d sha256=%s\n",step,id,w,h,bpp,hash);
g_free(f);
g_free(hash);
g_free(buf);
}
static void settle(GimpImage *im){
guchar sample[4];
gint64 until=g_get_monotonic_time()+(g_getenv("OPERATIONS_FILTER")?2000000:100000);
do{
while(g_main_context_iteration(NULL,FALSE)){
}
gimp_pickable_flush(GIMP_PICKABLE(gimp_image_get_projection(im)));
gimp_pickable_get_pixel_at(GIMP_PICKABLE(gimp_image_get_projection(im)),20,20,sample);
gimp_pickable_get_pixel_at(GIMP_PICKABLE(gimp_image_get_projection(im)),72,20,sample);
gimp_pickable_get_pixel_at(GIMP_PICKABLE(gimp_image_get_projection(im)),20,72,sample);
gimp_pickable_get_pixel_at(GIMP_PICKABLE(gimp_image_get_projection(im)),72,72,sample);
g_usleep(1000);
}
while(g_get_monotonic_time()<until);
}
static void history(GimpUndo *u,const gchar *stack,const gchar *path){
gint i;
if(!u)return;
g_print("HISTORY step=%s stack=%s path=%s type=%s undo_type=%d item=%s\n",step,stack,path,G_OBJECT_TYPE_NAME(u),u->undo_type,GIMP_IS_ITEM_UNDO(u)?idof(GIMP_ITEM_UNDO(u)->item):"-");
if(GIMP_IS_UNDO_STACK(u))for(i=0;i<gimp_container_get_n_children(GIMP_UNDO_STACK(u)->undos);i++){
gchar *p=g_strdup_printf("%s.%d",path,i);
history(GIMP_UNDO(gimp_container_get_child_by_index(GIMP_UNDO_STACK(u)->undos,i)),stack,p);
g_free(p);
}
}
static void snapshot(Gimp *gimp,GimpImage *im,const gchar *name){
gint i;
gchar *f;
GError *err=NULL;
GimpPlugInProcedure *proc;
step=name;
settle(im);
g_print("STATE step=%s undo=%d redo=%d\n",step,gimp_undo_stack_get_depth(gimp_image_get_undo_stack(im)),gimp_undo_stack_get_depth(gimp_image_get_redo_stack(im)));
for(i=0;i<nw;i++){
Watch *w=&watches[i];
GimpLayer *l=w->p;
g_print("OBJECT step=%s id=%s pointer=%p alive=%d updates=%u",step,w->id,l,w->alive,w->updates);
if(w->alive){
g_print(" core_id=%d attached=%d parent=%s index=%d xy=%d,%d wh=%d,%d",gimp_item_get_ID(GIMP_ITEM(l)),gimp_item_is_attached(GIMP_ITEM(l)),idof(gimp_item_get_parent(GIMP_ITEM(l))),gimp_item_is_attached(GIMP_ITEM(l))?gimp_item_get_index(GIMP_ITEM(l)):-1,gimp_item_get_offset_x(GIMP_ITEM(l)),gimp_item_get_offset_y(GIMP_ITEM(l)),gimp_item_get_width(GIMP_ITEM(l)),gimp_item_get_height(GIMP_ITEM(l)));
if(GIMP_IS_CLONE_LAYER(l)){
GimpLayer *src=gimp_clone_layer_get_source(GIMP_CLONE_LAYER(l));
g_print(" source=%s source_pointer=%p",idof(src),src);
}
if(GIMP_IS_FILTER_LAYER(l))g_print(" procedure=%s pending=%d completions=%d",gimp_filter_layer_get_procedure(GIMP_FILTER_LAYER(l)),gimp_filter_layer_capture_pending(GIMP_FILTER_LAYER(l)),GPOINTER_TO_INT(g_object_get_data(G_OBJECT(l),"migration-capture-completions")));
}
g_print("\n");
if(w->alive && GIMP_IS_FILTER_LAYER(l)){
GArray *args=gimp_filter_layer_get_procedure_args(GIMP_FILTER_LAYER(l));
guint k;
for(k=0;k<args->len;k++){
GValue *v=&g_array_index(args,GValue,k);
gchar *value=G_VALUE_TYPE(v)?g_strdup_value_contents(v):g_strdup("UNSET");
g_print("ARG step=%s id=%s index=%u type=%s value=%s\n",step,w->id,k,G_VALUE_TYPE(v)?G_VALUE_TYPE_NAME(v):"UNSET",value);
g_free(value);
}
g_array_unref(args);
}
if(w->alive)pixels(GIMP_PICKABLE(l),gimp_item_get_width(GIMP_ITEM(l)),gimp_item_get_height(GIMP_ITEM(l)),gimp_drawable_bytes(GIMP_DRAWABLE(l)),w->id);
}
 history(GIMP_UNDO(gimp_image_get_undo_stack(im)),"undo","root");
history(GIMP_UNDO(gimp_image_get_redo_stack(im)),"redo","root");
pixels(GIMP_PICKABLE(gimp_image_get_projection(im)),96,80,4,"projection");
f=g_strdup_printf("%s/%s.xcf",g_getenv("GIMP_PAINTER_FIXTURE_DIR"),step);
proc=file_procedure_find(gimp->plug_in_manager->save_procs,f,&err);
g_assert_no_error(err);
g_assert_cmpint(file_save(gimp,im,NULL,f,proc,GIMP_RUN_NONINTERACTIVE,FALSE,FALSE,FALSE,&err),==,GIMP_PDB_SUCCESS);
g_assert_no_error(err);
g_print("SAVE step=%s success=1\n",step);
g_free(f);
}
static void action(const gchar *name,const gchar *description){
step=name;
g_print("ACTION step=%s api=%s\n",step,description);
}
static void undo_redo(Gimp *gimp,GimpImage *im,const gchar *prefix){
gchar *s=g_strdup_printf("%s-undo",prefix);
action(s,"gimp_image_undo");
g_print("RESULT step=%s success=%d\n",step,gimp_image_undo(im));
snapshot(gimp,im,s);
s=g_strdup_printf("%s-redo",prefix);
action(s,"gimp_image_redo");
g_print("RESULT step=%s success=%d\n",step,gimp_image_redo(im));
snapshot(gimp,im,s);
}
static GimpLayer *rect(GimpImage *im,GimpLayer *parent,const gchar *name,gint x,gint y,GimpRGB color){
GimpLayer *l=gimp_layer_new(im,24,20,GIMP_RGBA_IMAGE,name,1,GIMP_NORMAL_MODE);
g_assert(gimp_image_add_layer(im,l,parent,0,FALSE));
gimp_item_set_offset(GIMP_ITEM(l),x,y);
gimp_drawable_fill(GIMP_DRAWABLE(l),&color,NULL);
gimp_drawable_update(GIMP_DRAWABLE(l),0,0,24,20);
watch(l,name);
return l;
}
static GimpLayer *clone(GimpImage *im,GimpLayer *parent,GimpLayer *source,const gchar *name){
GimpLayer *l=gimp_clone_layer_new(im,source,24,20,name,1,GIMP_NORMAL_MODE);
g_assert(gimp_image_add_layer(im,l,parent,0,FALSE));
watch(l,name);
return l;
}
static void capture_clone(Gimp *gimp){
GimpImage *im=gimp_create_image(gimp,96,80,GIMP_RGB,FALSE);
GimpLayer *group,*source,*c,*external,*ec,*direct,*copy,*copy_source,*copy_clone;
GimpRGB red={
.8,.2,.4,1}
,blue={
.2,.3,.8,1}
,green={
.2,.8,.3,1}
;
 external=rect(im,NULL,"external",60,40,blue);
group=gimp_group_layer_new(im);
gimp_object_set_name(GIMP_OBJECT(group),"group");
g_assert(gimp_image_add_layer(im,group,NULL,0,FALSE));
watch(group,"group");
source=rect(im,group,"source",8,8,red);
c=clone(im,group,source,"clone");
gimp_item_set_offset(GIMP_ITEM(c),36,8);
ec=clone(im,group,external,"external-clone");
gimp_item_set_offset(GIMP_ITEM(ec),36,36);
gimp_drawable_update(GIMP_DRAWABLE(source),0,0,24,20);
gimp_drawable_update(GIMP_DRAWABLE(external),0,0,24,20);
settle(im);
gimp_image_undo_free(im);
snapshot(gimp,im,"baseline");
 action("move-source","gimp_item_translate(source,5,7,TRUE)");
gimp_item_translate(GIMP_ITEM(source),5,7,TRUE);
snapshot(gimp,im,"move-source");
undo_redo(gimp,im,"move-source");
 action("move-clone","gimp_item_translate(clone,-3,4,TRUE)");
gimp_item_translate(GIMP_ITEM(c),-3,4,TRUE);
snapshot(gimp,im,"move-clone");
undo_redo(gimp,im,"move-clone");
 action("reorder-source","gimp_image_reorder_item(source,group,0,TRUE)");
g_assert(gimp_image_reorder_item(im,GIMP_ITEM(source),GIMP_ITEM(group),0,TRUE,"Move source in stack"));
snapshot(gimp,im,"reorder-source");
undo_redo(gimp,im,"reorder-source");
 action("duplicate-clone","gimp_item_duplicate(clone,GIMP_TYPE_CLONE_LAYER)+gimp_image_add_layer(push_undo=TRUE)");
direct=GIMP_LAYER(gimp_item_duplicate(GIMP_ITEM(c),GIMP_TYPE_CLONE_LAYER));
watch(direct,"direct-copy");
gimp_object_set_name(GIMP_OBJECT(direct),"direct-copy");
g_assert(gimp_image_add_layer(im,direct,NULL,0,TRUE));
snapshot(gimp,im,"duplicate-clone");
undo_redo(gimp,im,"duplicate-clone");
 action("duplicate-group","gimp_item_duplicate(group,GIMP_TYPE_GROUP_LAYER)+gimp_image_add_layer(push_undo=TRUE)");
copy=GIMP_LAYER(gimp_item_duplicate(GIMP_ITEM(group),GIMP_TYPE_GROUP_LAYER));
watch(copy,"group-copy");
gimp_object_set_name(GIMP_OBJECT(copy),"group-copy");
{
GimpContainer *children=gimp_viewable_get_children(GIMP_VIEWABLE(copy));
gint i;
for(i=0;i<gimp_container_get_n_children(children);i++){
GimpLayer *l=GIMP_LAYER(gimp_container_get_child_by_index(children,i));
const gchar *name=gimp_object_get_name(l);
watch(l,g_strdup_printf("copy-%s",name));
}
}
 g_assert(gimp_image_add_layer(im,copy,NULL,0,TRUE));
copy_source=watches[nw-3].p;
copy_clone=NULL;
{
gint i;
for(i=0;i<nw;i++){
if(!strcmp(watches[i].id,"copy-source"))copy_source=watches[i].p;
if(!strcmp(watches[i].id,"copy-clone"))copy_clone=watches[i].p;
}
}
g_assert(copy_clone);
snapshot(gimp,im,"duplicate-group");
undo_redo(gimp,im,"duplicate-group");
 action("edit-original","gimp_drawable_fill(source,green)+gimp_drawable_update");
gimp_drawable_fill(GIMP_DRAWABLE(source),&green,NULL);
gimp_drawable_update(GIMP_DRAWABLE(source),0,0,24,20);
snapshot(gimp,im,"edit-original");
 action("edit-copy","gimp_drawable_fill(copy-source,blue)+gimp_drawable_update");
gimp_drawable_fill(GIMP_DRAWABLE(copy_source),&blue,NULL);
gimp_drawable_update(GIMP_DRAWABLE(copy_source),0,0,24,20);
snapshot(gimp,im,"edit-copy");
 action("edit-external","gimp_drawable_fill(external,green)+gimp_drawable_update");
gimp_drawable_fill(GIMP_DRAWABLE(external),&green,NULL);
gimp_drawable_update(GIMP_DRAWABLE(external),0,0,24,20);
snapshot(gimp,im,"edit-external");
 action("delete-source","gimp_image_remove_layer(source,TRUE)");
gimp_image_remove_layer(im,source,TRUE,NULL);
snapshot(gimp,im,"delete-source");
undo_redo(gimp,im,"delete-source");
 action("restore-source","gimp_image_undo");
g_assert(gimp_image_undo(im));
snapshot(gimp,im,"restore-source");
action("edit-restored","gimp_drawable_fill(source,blue)+gimp_drawable_update");
gimp_drawable_fill(GIMP_DRAWABLE(source),&blue,NULL);
gimp_drawable_update(GIMP_DRAWABLE(source),0,0,24,20);
snapshot(gimp,im,"edit-restored");
 action("delete-clone","gimp_image_remove_layer(clone,TRUE)");
gimp_image_remove_layer(im,c,TRUE,NULL);
snapshot(gimp,im,"delete-clone");
undo_redo(gimp,im,"delete-clone");
 action("delete-group","gimp_image_remove_layer(group-copy,TRUE)");
gimp_image_remove_layer(im,copy,TRUE,NULL);
snapshot(gimp,im,"delete-group");
undo_redo(gimp,im,"delete-group");
g_print("OPERATIONS_COMPLETE scene=clone\n");
}
static void capture_filter(Gimp *gimp){
GimpImage *im=gimp_create_image(gimp,96,80,GIMP_RGB,FALSE);
GimpLayer *source,*effect,*copy;
GimpRGB red={
.8,.2,.4,1}
,blue={
.1,.2,.8,1}
;
GimpProcedure *proc;
GValueArray *defaults;
GArray *args;
guint i;
 source=rect(im,NULL,"source",8,8,red);
gimp_context_set_foreground(gimp_get_user_context(gimp),&blue);
gimp_channel_select_rectangle(gimp_image_get_mask(im),14,13,10,8,GIMP_CHANNEL_OP_REPLACE,FALSE,0,0,FALSE);
g_assert(gimp_edit_fill(im,GIMP_DRAWABLE(source),gimp_get_user_context(gimp),GIMP_FOREGROUND_FILL));
gimp_channel_clear(gimp_image_get_mask(im),NULL,FALSE);
gimp_drawable_update(GIMP_DRAWABLE(source),0,0,24,20);
 proc=gimp_pdb_lookup_procedure(gimp->pdb,"plug-in-edge");
g_assert(proc);
defaults=gimp_procedure_get_arguments(proc);
args=g_array_sized_new(FALSE,TRUE,sizeof(GValue),6);
g_array_set_clear_func(args,(GDestroyNotify)g_value_unset);
g_array_set_size(args,6);
for(i=0;i<6;i++){
GValue *v=&g_array_index(args,GValue,i);
g_value_init(v,G_VALUE_TYPE(&defaults->values[i]));
g_value_copy(&defaults->values[i],v);
}
g_value_set_int(&g_array_index(args,GValue,0),GIMP_RUN_NONINTERACTIVE);
g_value_set_double(&g_array_index(args,GValue,3),2);
g_value_set_int(&g_array_index(args,GValue,4),1);
g_value_set_int(&g_array_index(args,GValue,5),0);
 effect=gimp_filter_layer_new(im,24,20,"filter",1,GIMP_NORMAL_MODE);
g_assert(gimp_image_add_layer(im,effect,NULL,0,FALSE));
gimp_item_set_offset(GIMP_ITEM(effect),8,8);
watch(effect,"filter");
gimp_filter_layer_set_procedure(GIMP_FILTER_LAYER(effect),"plug-in-edge",args);
g_array_unref(args);
g_value_array_free(defaults);
settle(im);
gimp_image_undo_free(im);
snapshot(gimp,im,"baseline");
 action("move-source","gimp_item_translate(source,5,7,TRUE)");
gimp_item_translate(GIMP_ITEM(source),5,7,TRUE);
snapshot(gimp,im,"move-source");
undo_redo(gimp,im,"move-source");
 action("move-filter","gimp_item_translate(filter,3,4,TRUE)");
gimp_item_translate(GIMP_ITEM(effect),3,4,TRUE);
snapshot(gimp,im,"move-filter");
undo_redo(gimp,im,"move-filter");
 action("duplicate-filter","gimp_item_duplicate(filter,GIMP_TYPE_FILTER_LAYER)+gimp_image_add_layer(TRUE)");
copy=GIMP_LAYER(gimp_item_duplicate(GIMP_ITEM(effect),GIMP_TYPE_FILTER_LAYER));
watch(copy,"filter-copy");
gimp_object_set_name(GIMP_OBJECT(copy),"filter-copy");
g_assert(gimp_image_add_layer(im,copy,NULL,0,TRUE));
snapshot(gimp,im,"duplicate-filter");
undo_redo(gimp,im,"duplicate-filter");
 action("delete-source","gimp_image_remove_layer(source,TRUE)");
gimp_image_remove_layer(im,source,TRUE,NULL);
snapshot(gimp,im,"delete-source");
undo_redo(gimp,im,"delete-source");
 action("restore-source","gimp_image_undo");
g_assert(gimp_image_undo(im));
snapshot(gimp,im,"restore-source");
 action("delete-filter","gimp_image_remove_layer(filter-copy,TRUE)");
gimp_image_remove_layer(im,copy,TRUE,NULL);
snapshot(gimp,im,"delete-filter");
undo_redo(gimp,im,"delete-filter");
g_print("OPERATIONS_COMPLETE scene=filter\n");
}
int main(int argc,char **argv){
Gimp *gimp;
g_thread_init(NULL);
g_type_init();
gimp=gimp_init_for_testing();
if(g_getenv("OPERATIONS_FILTER"))capture_filter(gimp);
else capture_clone(gimp);
return 0;
}
