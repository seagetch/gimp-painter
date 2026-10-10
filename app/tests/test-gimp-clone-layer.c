/* SPDX-License-Identifier: GPL-3.0-or-later
 * Legacy expectations measured with afa43fae, not inferred from this port.
 */
#include "config.h"
#include <gegl.h>
#include <stddef.h>
#include <string.h>
#include <gtk/gtk.h>
#include "libgimpbase/gimpbase.h"
#include "libgimpmath/gimpmath.h"
#include "core/core-types.h"
#include "widgets/widgets-types.h"
#include "core/gimp.h"
#include "core/gimpclonelayer.h"
#include "core/gimpclonelayerundo.h"
#include "core/gimpcontext.h"
#include "core/gimpcontainer.h"
#include "core/gimpimage.h"
#include "core/gimpimage-undo.h"
#include "core/gimpimage-convert-precision.h"
#include "core/gimpundostack.h"
#include "core/gimpitemundo.h"
#include "core/gimpdrawablemodundo.h"
#include "core/gimpgrouplayer.h"
#include "core/gimplayer-new.h"
#include "core/gimplayermask.h"
#include "core/gimppickable.h"
#include "painter/gimp-painter-binding.h"
#include "tests.h"
#include "gimp-app-test-utils.h"

void gimp_test_clone_retained_image (GimpImage *image, GimpCloneLayer *clone);
void gimp_test_clone_cpp_layout (gsize size, gsize offset, GimpCloneLayer *layer);
typedef struct { GimpCloneLayer parent; } TestBrokenClone;
typedef struct { GimpCloneLayerClass parent; } TestBrokenCloneClass;
GType test_broken_clone_get_type (void);
G_DEFINE_TYPE (TestBrokenClone, test_broken_clone, GIMP_TYPE_CLONE_LAYER)
static void test_broken_clone_class_init (TestBrokenCloneClass *klass) {}
static void test_broken_clone_init (TestBrokenClone *clone)
{ gimp_painter_binding_close (G_OBJECT (clone), NULL); }
typedef struct { GimpCloneLayer parent; } TestLifecycleClone;
typedef struct { GimpCloneLayerClass parent; } TestLifecycleCloneClass;
static guint lifecycle_finalizes;
GType test_lifecycle_clone_get_type (void);
G_DEFINE_TYPE (TestLifecycleClone, test_lifecycle_clone, GIMP_TYPE_CLONE_LAYER)
static void test_lifecycle_clone_finalize (GObject *object)
{
  lifecycle_finalizes++;
  G_OBJECT_CLASS (test_lifecycle_clone_parent_class)->finalize (object);
}
static void test_lifecycle_clone_class_init (TestLifecycleCloneClass *klass)
{ G_OBJECT_CLASS (klass)->finalize = test_lifecycle_clone_finalize; }
static void test_lifecycle_clone_init (TestLifecycleClone *clone) {}
static Gimp *gimp;
static void cpp_header_layout (void);
static GimpImage *new_image (void)
{ return gimp_image_new (gimp, 100, 100, GIMP_RGB, GIMP_PRECISION_U8_NON_LINEAR); }
static GimpLayer *new_layer (GimpImage *image, const gchar *name, GimpLayer *parent, gint position)
{
  GimpLayer *layer = gimp_layer_new (image, 16, 16, babl_format ("R'G'B'A u8"), name,
                                     1.0, GIMP_LAYER_MODE_NORMAL_LEGACY);
  g_assert_true (gimp_image_add_layer (image, layer, parent, position, FALSE));
  return layer;
}
static GimpCloneLayer *new_clone (GimpImage *image, GimpLayer *source)
{
  GimpLayer *layer = gimp_clone_layer_new (image, source, 16, 16, "clone", 1.0,
                                           GIMP_LAYER_MODE_NORMAL_LEGACY);
  g_assert_true (GIMP_IS_CLONE_LAYER (layer));
  g_assert_true (gimp_image_add_layer (image, layer, NULL, 0, FALSE));
  return GIMP_CLONE_LAYER (layer);
}
static void fill (GimpLayer *layer, guchar r, guchar g, guchar b, guchar a)
{
  guchar pixels[16 * 16 * 4];
  for (gint i = 0; i < 16 * 16; i++)
    { pixels[4*i] = r; pixels[4*i+1] = g; pixels[4*i+2] = b; pixels[4*i+3] = a; }
  gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (layer)), GEGL_RECTANGLE (0, 0, 16, 16),
                    0, babl_format ("R'G'B'A u8"), pixels, GEGL_AUTO_ROWSTRIDE);
  gimp_drawable_update (GIMP_DRAWABLE (layer), 0, 0, 16, 16);
}
static void pixel (GimpLayer *layer, gint r, gint g, gint b, gint a)
{
  guchar value[4];
  gegl_buffer_get (gimp_drawable_get_buffer (GIMP_DRAWABLE (layer)), GEGL_RECTANGLE (0, 0, 1, 1),
                    1.0, babl_format ("R'G'B'A u8"), value, GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);
  g_assert_cmpint (value[0], ==, r); g_assert_cmpint (value[1], ==, g);
  g_assert_cmpint (value[2], ==, b); g_assert_cmpint (value[3], ==, a);
}
static void projected_pixels (void)
{
  GimpImage *image = new_image ();
  GimpLayer *source = new_layer (image, "source", NULL, 0);
  GimpCloneLayer *clone;
  GimpLayerMask *mask;
  GError *error = NULL;
  guchar mask_pixels[16 * 16];
  fill (source, 204, 51, 102, 128);
  gimp_layer_set_opacity (source, 0.5, FALSE);
  clone = new_clone (image, source);
  pixel (GIMP_LAYER (clone), 204, 51, 102, 64);
  mask = gimp_layer_create_mask (source, GIMP_ADD_MASK_WHITE, NULL);
  memset (mask_pixels, 128, sizeof mask_pixels);
  gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (mask)), GEGL_RECTANGLE (0, 0, 16, 16),
                    0, babl_format ("Y u8"), mask_pixels, GEGL_AUTO_ROWSTRIDE);
  g_assert_nonnull (gimp_layer_add_mask (source, mask, FALSE, &error));
  g_assert_no_error (error);
  pixel (GIMP_LAYER (clone), 204, 51, 102, 32);
  gimp_layer_set_show_mask (source, TRUE, FALSE);
  pixel (GIMP_LAYER (clone), 128, 128, 128, 255);
  gimp_layer_set_show_mask (source, FALSE, FALSE);
  gimp_item_set_visible (GIMP_ITEM (source), FALSE, FALSE);
  pixel (GIMP_LAYER (clone), 204, 51, 102, 32);
  gimp_layer_set_mode (source, GIMP_LAYER_MODE_MULTIPLY_LEGACY, FALSE);
  pixel (GIMP_LAYER (clone), 204, 51, 102, 32);
  g_assert_cmpfloat (gimp_pickable_get_opacity_at (GIMP_PICKABLE (clone), 0, 0), ==, 0.0);
  g_object_unref (image);
}
static void dissolve_legacy_fixture (void)
{
  GimpImage *image = new_image ();
  GimpLayer *source = new_layer (image, "dissolve", NULL, 0);
  GimpLayerMask *mask = gimp_layer_create_mask (source, GIMP_ADD_MASK_WHITE, NULL);
  GimpCloneLayer *clone;
  guchar mask_pixels[256], pixels[1024], alpha[256];
  gchar *digest;
  gint nonzero = 0;
  memset (mask_pixels, 128, sizeof mask_pixels);
  fill (source, 204, 51, 102, 128);
  gimp_layer_set_opacity (source, 0.5, FALSE);
  gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (mask)), GEGL_RECTANGLE (0, 0, 16, 16),
                    0, babl_format ("Y u8"), mask_pixels, GEGL_AUTO_ROWSTRIDE);
  g_assert_nonnull (gimp_layer_add_mask (source, mask, FALSE, NULL));
  gimp_layer_set_mode (source, GIMP_LAYER_MODE_DISSOLVE, FALSE);
  clone = new_clone (image, source);
  gegl_buffer_get (gimp_drawable_get_buffer (GIMP_DRAWABLE (clone)), GEGL_RECTANGLE (0, 0, 16, 16),
                    1, babl_format ("R'G'B'A u8"), pixels, GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);
  for (gint i = 0; i < 256; ++i)
    { alpha[i] = pixels[4*i+3]; nonzero += alpha[i] != 0; }
  digest = g_compute_checksum_for_data (G_CHECKSUM_SHA256, alpha, 256);
  /* Genuine afa43fae clone-capture.log measurement, 2026-10-01. */
  g_assert_cmpint (nonzero, ==, 35);
  g_assert_cmpstr (digest, ==, "e391af9278f0852484c254d50220eca234a3b257f98ddc489ba402ac9182f9eb");
  g_free (digest); g_object_unref (image);
}
static void gray_and_high_precision (void)
{
  GimpImage *gray = gimp_image_new (gimp, 4, 4, GIMP_GRAY, GIMP_PRECISION_U8_NON_LINEAR);
  GimpLayer *source = gimp_layer_new (gray, 1, 1, babl_format ("Y'A u8"), "gray", 0.5,
                                     GIMP_LAYER_MODE_NORMAL_LEGACY);
  GimpLayer *clone;
  guchar input[2] = { 129, 128 }, output[2];
  GimpImage *linear;
  gfloat input_float[4] = { 0.2, 0.4, 0.6, 0.5 }, output_float[4];
  gimp_image_add_layer (gray, source, NULL, 0, FALSE);
  gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (source)), GEGL_RECTANGLE (0, 0, 1, 1),
                    0, babl_format ("Y'A u8"), input, GEGL_AUTO_ROWSTRIDE);
  clone = gimp_clone_layer_new (gray, source, 1, 1, "clone", 1.0, GIMP_LAYER_MODE_NORMAL_LEGACY);
  gimp_image_add_layer (gray, clone, NULL, 0, FALSE);
  gegl_buffer_get (gimp_drawable_get_buffer (GIMP_DRAWABLE (clone)), GEGL_RECTANGLE (0, 0, 1, 1),
                    1, babl_format ("Y'A u8"), output, GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);
  g_assert_cmpint (output[0], ==, 129); g_assert_cmpint (output[1], ==, 64);
  g_object_unref (gray);
  linear = gimp_image_new (gimp, 4, 4, GIMP_RGB, GIMP_PRECISION_FLOAT_LINEAR);
  source = gimp_layer_new (linear, 1, 1, babl_format ("RGBA float"), "float", 0.5,
                            GIMP_LAYER_MODE_NORMAL);
  gimp_image_add_layer (linear, source, NULL, 0, FALSE);
  gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (source)), GEGL_RECTANGLE (0, 0, 1, 1),
                    0, babl_format ("RGBA float"), input_float, GEGL_AUTO_ROWSTRIDE);
  clone = gimp_clone_layer_new (linear, source, 1, 1, "clone", 1.0, GIMP_LAYER_MODE_NORMAL);
  gimp_image_add_layer (linear, clone, NULL, 0, FALSE);
  gegl_buffer_get (gimp_drawable_get_buffer (GIMP_DRAWABLE (clone)), GEGL_RECTANGLE (0, 0, 1, 1),
                    1, babl_format ("RGBA float"), output_float, GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);
  for (gint i = 0; i < 3; ++i)
    g_assert_cmpfloat_with_epsilon (output_float[i], input_float[i], 0.00001);
  g_assert_cmpfloat (output_float[3], ==, 0.25f);
  g_object_unref (linear);
}
static void double_precision_and_trc (void)
{
  /* 2e-14 allows <100 machine eps at this sample scale, but rejects float
   * truncation (about 1e-8) and nonlinear round-trip/clipping changes. */
  const gdouble tolerance = 2e-14;
  const gdouble opacity = 0.33333333333333331;
  for (gint gray = 0; gray < 2; ++gray)
    for (gint linear = 0; linear < 2; ++linear)
      {
        GimpImage *image = gimp_image_new (gimp, 4, 4, gray ? GIMP_GRAY : GIMP_RGB,
          linear ? GIMP_PRECISION_DOUBLE_LINEAR : GIMP_PRECISION_DOUBLE_NON_LINEAR);
        const Babl *format = babl_format (gray ? (linear ? "YA double" : "Y'A double") :
                                                (linear ? "RGBA double" : "R'G'B'A double"));
        GimpLayer *source = gimp_layer_new (image, 1, 1, format, "double", opacity, GIMP_LAYER_MODE_NORMAL);
        GimpLayer *clone;
        gdouble input[4] = { 0.1234567890123456, 1.2345678901234567, -0.0123456789012345, 0.8765432109876543 };
        gdouble output[4];
        gint channels = gray ? 2 : 4;
        if (gray) input[1] = input[3];
        gimp_image_add_layer (image, source, NULL, 0, FALSE);
        gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (source)), GEGL_RECTANGLE (0, 0, 1, 1),
                          0, format, input, GEGL_AUTO_ROWSTRIDE);
        clone = gimp_clone_layer_new (image, source, 1, 1, "clone", 1.0, GIMP_LAYER_MODE_NORMAL);
        gimp_image_add_layer (image, clone, NULL, 0, FALSE);
        gegl_buffer_get (gimp_drawable_get_buffer (GIMP_DRAWABLE (clone)), GEGL_RECTANGLE (0, 0, 1, 1),
                          1, format, output, GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);
        for (gint i = 0; i < channels - 1; ++i)
          g_assert_cmpfloat_with_epsilon (output[i], input[i], tolerance);
        g_assert_cmpfloat_with_epsilon (output[channels - 1], input[channels - 1] * opacity, tolerance);
        g_object_unref (image);
      }
}
static void partial_update_and_graph (void)
{
  GeglNode *node;

  GimpImage *image = new_image ();
  GimpLayer *source = new_layer (image, "source", NULL, 0);
  GimpCloneLayer *clone = new_clone (image, source);
  guchar red[] = { 255, 0, 0, 255 }, value[4];
  fill (source, 0, 0, 255, 255);
  node = gimp_drawable_get_source_node (GIMP_DRAWABLE (clone));
  gegl_node_blit (node, 1, GEGL_RECTANGLE (0, 0, 1, 1), babl_format ("R'G'B'A u8"), value,
                  GEGL_AUTO_ROWSTRIDE, GEGL_BLIT_DEFAULT);
  g_assert_cmpint (value[2], ==, 255);
  gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (source)), GEGL_RECTANGLE (0, 0, 1, 1),
                    0, babl_format ("R'G'B'A u8"), red, GEGL_AUTO_ROWSTRIDE);
  gimp_drawable_update (GIMP_DRAWABLE (source), 0, 0, 1, 1);
  gegl_node_blit (node, 1, GEGL_RECTANGLE (0, 0, 1, 1), babl_format ("R'G'B'A u8"), value,
                  GEGL_AUTO_ROWSTRIDE, GEGL_BLIT_DEFAULT);
  g_assert_cmpmem (value, 4, red, 4);
  gegl_buffer_get (gimp_drawable_get_buffer (GIMP_DRAWABLE (clone)), GEGL_RECTANGLE (1, 0, 1, 1),
                    1, babl_format ("R'G'B'A u8"), value, GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);
  g_assert_cmpint (value[2], ==, 255);
  gimp_drawable_update (GIMP_DRAWABLE (source), -50, -50, 51, 51);
  gimp_drawable_update (GIMP_DRAWABLE (source), 90, 90, 1, 1);
  g_object_unref (image);
}
static void deferred_name (void)
{
  GimpLayer *later;

  GimpImage *image = new_image ();
  GimpLayer *other = new_layer (image, "other source", NULL, 0);
  GimpCloneLayer *clone = new_clone (image, NULL);
  gchar *name;
  gimp_clone_layer_set_source_by_name (clone, "later source");
  gimp_clone_layer_set_source (clone, other);
  g_assert_true (gimp_clone_layer_get_source (clone) == other);
  name = gimp_clone_layer_dup_source_name (clone);
  g_assert_cmpstr (name, ==, "later source"); g_free (name);
  later = new_layer (image, "later source", NULL, 1);
  g_assert_true (gimp_clone_layer_get_source (clone) == later);
  gimp_clone_layer_set_source (clone, other);
  g_assert_true (gimp_clone_layer_get_source (clone) == other);
  gimp_clone_layer_set_source_by_name (clone, "later source");
  gimp_clone_layer_set_source (clone, other);
  /* Direct setter emits update; legacy update_size resolves the existing name. */
  g_assert_cmpint (gimp_clone_layer_get_source_state (clone), ==, GIMP_CLONE_SOURCE_LIVE);
  g_assert_true (gimp_clone_layer_get_source (clone) == later);
  gimp_clone_layer_set_source (clone, other);
  gimp_object_set_name (GIMP_OBJECT (other), "renamed");
  name = gimp_clone_layer_dup_source_name (clone);
  g_assert_cmpstr (name, ==, "renamed"); g_free (name);
  g_object_unref (image);
}
static void recursive_first_match (void)
{
  GimpLayer *inside;
  GimpCloneLayer *clone;

  GimpImage *image = new_image ();
  GimpLayer *group = gimp_group_layer_new (image);
  gimp_object_set_name (GIMP_OBJECT (group), "group");
  gimp_image_add_layer (image, group, NULL, 0, FALSE);
  inside = new_layer (image, "match", group, 0);
  new_layer (image, "match", NULL, 1);
  clone = new_clone (image, NULL);
  gimp_clone_layer_set_source_by_name (clone, "match");
  g_assert_true (gimp_clone_layer_get_source (clone) == inside);
  gimp_object_set_name (GIMP_OBJECT (group), "match");
  gimp_clone_layer_set_source_by_name (clone, "match");
  g_assert_true (gimp_clone_layer_get_source (clone) == group);
  g_object_unref (image);
}
static void offset_size_and_noop_transforms (void)
{
  GimpImage *image = new_image ();
  GimpLayer *source = new_layer (image, "source", NULL, 0);
  GimpCloneLayer *clone = new_clone (image, source);
  GimpContext *context = gimp_context_new (gimp, "Test", NULL);
  GimpMatrix3 matrix;
  gimp_item_set_offset (GIMP_ITEM (clone), 32, 30);
  gimp_item_set_offset (GIMP_ITEM (source), 5, 7);
  gimp_drawable_update (GIMP_DRAWABLE (source), 0, 0, 16, 16);
  g_assert_cmpint (gimp_item_get_offset_x (GIMP_ITEM (clone)), ==, 32);
  g_assert_cmpint (gimp_item_get_offset_y (GIMP_ITEM (clone)), ==, 30);
  gimp_item_resize (GIMP_ITEM (source), context, GIMP_FILL_TRANSPARENT, 20, 18, -2, -3);
  g_assert_cmpint (gimp_item_get_width (GIMP_ITEM (clone)), ==, 20);
  g_assert_cmpint (gimp_item_get_height (GIMP_ITEM (clone)), ==, 18);
  g_assert_cmpint (gimp_item_get_offset_x (GIMP_ITEM (clone)), ==, 34);
  g_assert_cmpint (gimp_item_get_offset_y (GIMP_ITEM (clone)), ==, 33);
  gimp_item_scale (GIMP_ITEM (clone), 13, 9, -5, 4, GIMP_INTERPOLATION_NONE, NULL);
  gimp_item_flip (GIMP_ITEM (clone), context, GIMP_ORIENTATION_HORIZONTAL, 0, FALSE);
  gimp_item_rotate (GIMP_ITEM (clone), context, GIMP_ROTATE_DEGREES90, 0, 0, FALSE);
  gimp_matrix3_identity (&matrix);
  gimp_matrix3_translate (&matrix, 50, 20);
  gimp_item_transform (GIMP_ITEM (clone), context, &matrix, GIMP_TRANSFORM_FORWARD,
                       GIMP_INTERPOLATION_NONE, GIMP_TRANSFORM_RESIZE_ADJUST, NULL);
  g_assert_cmpint (gimp_item_get_width (GIMP_ITEM (clone)), ==, 20);
  g_assert_cmpint (gimp_item_get_height (GIMP_ITEM (clone)), ==, 18);
  g_assert_cmpint (gimp_item_get_offset_x (GIMP_ITEM (clone)), ==, 34);
  g_assert_cmpint (gimp_item_get_offset_y (GIMP_ITEM (clone)), ==, 33);
  g_assert_true (gimp_item_is_content_locked (GIMP_ITEM (clone), NULL));
  g_assert_false (gimp_item_is_position_locked (GIMP_ITEM (clone), NULL));
  g_object_unref (context); g_object_unref (image);
}
static void source_lifetime_and_detach (void)
{
  gchar *name;

  GimpImage *image = new_image ();
  GimpLayer *source = new_layer (image, "source", NULL, 0);
  GimpLayer *other = new_layer (image, "other", NULL, 1);
  GimpCloneLayer *clone = new_clone (image, source);
  fill (source, 204, 51, 102, 128);
  gimp_clone_layer_set_source (clone, other);
  fill (other, 255, 0, 0, 255);
  fill (source, 0, 255, 0, 255);
  pixel (GIMP_LAYER (clone), 255, 0, 0, 255);
  gimp_clone_layer_set_source (clone, NULL);
  fill (other, 0, 0, 255, 255);
  pixel (GIMP_LAYER (clone), 255, 0, 0, 255);
  gimp_clone_layer_set_source (clone, other);
  gimp_image_remove_layer (image, other, FALSE, NULL);
  g_assert_null (gimp_clone_layer_get_source (clone));
  g_assert_cmpint (gimp_clone_layer_get_source_state (clone), ==, GIMP_CLONE_SOURCE_EXPIRED);
  name = gimp_clone_layer_dup_source_name (clone);
  g_assert_cmpstr (name, ==, "other"); g_free (name);
  g_object_unref (image);
}
static void frozen_source_disposal (void)
{
  GimpImage *image = new_image ();
  GimpLayer *source = new_layer (image, "source", NULL, 0);
  GimpCloneLayer *clone = new_clone (image, source);
  gimp_viewable_preview_freeze (GIMP_VIEWABLE (source));
  g_assert_true (gimp_viewable_preview_is_frozen (GIMP_VIEWABLE (clone)));
  gimp_image_remove_layer (image, source, FALSE, NULL);
  g_assert_false (gimp_viewable_preview_is_frozen (GIMP_VIEWABLE (clone)));
  g_assert_null (gimp_clone_layer_get_source (clone));
  g_object_unref (image);
}
static void group_duplicate_internal_reference (void)
{
  GimpImage *image = new_image ();
  GimpLayer *group = gimp_group_layer_new (image);
  GimpLayer *source, *clone;
  GimpItem *copy;
  GimpContainer *children;
  GimpCloneLayer *copy_clone;
  gimp_image_add_layer (image, group, NULL, 0, FALSE);
  source = new_layer (image, "original source", group, 0);
  clone = gimp_clone_layer_new (image, source, 16, 16, "child clone", 1.0, GIMP_LAYER_MODE_NORMAL_LEGACY);
  gimp_image_add_layer (image, clone, group, 0, FALSE);
  copy = gimp_item_duplicate (GIMP_ITEM (group), GIMP_TYPE_GROUP_LAYER);
  g_object_ref_sink (copy);
  children = gimp_viewable_get_children (GIMP_VIEWABLE (copy));
  copy_clone = GIMP_CLONE_LAYER (gimp_container_get_child_by_index (children, 0));
  g_assert_true (gimp_clone_layer_get_source (copy_clone) ==
                 GIMP_LAYER (gimp_container_get_child_by_index (children, 1)));
  g_assert_true (GIMP_LAYER (gimp_container_get_child_by_index (children, 1)) != source);
  g_object_unref (copy);
  /* An explicitly detached clone must not acquire a lazy reference on copy. */
  gimp_clone_layer_set_source (GIMP_CLONE_LAYER (clone), NULL);
  copy = gimp_item_duplicate (GIMP_ITEM (clone), GIMP_TYPE_CLONE_LAYER);
  g_object_ref_sink (copy);
  g_assert_cmpint (gimp_clone_layer_get_source_state (GIMP_CLONE_LAYER (copy)), ==, GIMP_CLONE_SOURCE_NONE);
  g_assert_null (gimp_clone_layer_get_source (GIMP_CLONE_LAYER (copy)));
  g_object_unref (copy); g_object_unref (image);
}
static GimpLayer *child_named (GimpLayer *group, const gchar *name)
{
  return GIMP_LAYER (gimp_container_get_child_by_name (
    gimp_viewable_get_children (GIMP_VIEWABLE (group)), name));
}
static GimpLayer *add_clone_child (GimpImage *image, GimpLayer *parent,
                                   const gchar *name, GimpLayer *source)
{
  GimpLayer *clone = gimp_clone_layer_new (image, source, 16, 16, name, 1.0, GIMP_LAYER_MODE_NORMAL_LEGACY);
  gimp_image_add_layer (image, clone, parent, 0, FALSE);
  return clone;
}
static void group_duplicate_complete_hierarchy (void)
{
  GimpImage *image = new_image ();
  GimpLayer *external = new_layer (image, "external", NULL, 0);
  GimpLayer *root = gimp_group_layer_new (image);
  GimpLayer *left = gimp_group_layer_new (image);
  GimpLayer *right = gimp_group_layer_new (image);
  GimpLayer *left_source, *right_source, *direct, *pending, *pending_live;
  GimpLayer *copy, *copy_left, *copy_right;
  GimpItem *direct_copy;
  gchar *name;
  gimp_object_set_name (GIMP_OBJECT (left), "left");
  gimp_object_set_name (GIMP_OBJECT (right), "right");
  gimp_image_add_layer (image, root, NULL, 0, FALSE);
  gimp_image_add_layer (image, left, root, 0, FALSE);
  gimp_image_add_layer (image, right, root, 1, FALSE);
  left_source = new_layer (image, "left source", left, 0);
  right_source = new_layer (image, "right source", right, 0);
  fill (left_source, 204, 51, 102, 128);
  fill (right_source, 0, 0, 255, 255);
  direct = add_clone_child (image, left, "internal", left_source);
  add_clone_child (image, left, "external clone", external);
  add_clone_child (image, left, "cross right", right_source);
  add_clone_child (image, right, "cross left", left_source);
  add_clone_child (image, left, "group source", right);
  add_clone_child (image, left, "root source", root);
  pending = add_clone_child (image, right, "pending", NULL);
  gimp_clone_layer_set_source_by_name (GIMP_CLONE_LAYER (pending), "missing saved source");
  pending_live = add_clone_child (image, right, "pending live", left_source);
  gimp_clone_layer_set_source_by_name (GIMP_CLONE_LAYER (pending_live), "missing future source");
  /* Direct CloneLayer duplication still retains the original identity. */
  direct_copy = gimp_item_duplicate (GIMP_ITEM (direct), GIMP_TYPE_CLONE_LAYER);
  g_object_ref_sink (direct_copy);
  g_assert_true (gimp_clone_layer_get_source (GIMP_CLONE_LAYER (direct_copy)) == left_source);
  g_object_unref (direct_copy);
  copy = GIMP_LAYER (gimp_item_duplicate (GIMP_ITEM (root), GIMP_TYPE_GROUP_LAYER));
  g_object_ref_sink (copy);
  copy_left = child_named (copy, "left");
  copy_right = child_named (copy, "right");
#define SOURCE(group, name) gimp_clone_layer_get_source (GIMP_CLONE_LAYER (child_named ((group), (name))))
  g_assert_true (SOURCE (copy_left, "internal") == child_named (copy_left, "left source"));
  g_assert_true (SOURCE (copy_left, "external clone") == external);
  g_assert_true (SOURCE (copy_left, "cross right") == child_named (copy_right, "right source"));
  g_assert_true (SOURCE (copy_right, "cross left") == child_named (copy_left, "left source"));
  g_assert_true (SOURCE (copy_left, "group source") == copy_right);
  g_assert_true (SOURCE (copy_left, "root source") == copy);
  g_assert_null (SOURCE (copy_right, "pending"));
  name = gimp_clone_layer_dup_source_name (GIMP_CLONE_LAYER (child_named (copy_right, "pending")));
  g_assert_cmpstr (name, ==, "missing saved source"); g_free (name);
  g_assert_true (SOURCE (copy_right, "pending live") == child_named (copy_left, "left source"));
  name = gimp_clone_layer_dup_source_name (GIMP_CLONE_LAYER (child_named (copy_right, "pending live")));
  g_assert_cmpstr (name, ==, "missing future source"); g_free (name);
  fill (child_named (copy_left, "left source"), 255, 0, 0, 255);
  pixel (child_named (copy_left, "internal"), 255, 0, 0, 255);
  pixel (direct, 204, 51, 102, 128);
#undef SOURCE
  g_object_unref (copy); g_object_unref (image);
}
static void source_reference_undo_live (void)
{
  GimpImage *image = new_image ();
  GimpLayer *first = new_layer (image, "first", NULL, 0);
  GimpLayer *second = new_layer (image, "second", NULL, 1);
  GimpCloneLayer *clone = new_clone (image, first);
  GimpContext *context = gimp_context_new (gimp, "Test", NULL);
  GError *error = NULL;
  fill (first, 255, 0, 0, 255); fill (second, 0, 0, 255, 255);
  gimp_item_resize (GIMP_ITEM (second), context, GIMP_FILL_TRANSPARENT, 20, 18, 0, 0);
  gimp_item_set_offset (GIMP_ITEM (clone), 32, 30);
  gimp_image_undo_free (image);
  g_assert_true (gimp_clone_layer_set_source_with_undo (clone, second, "Change source", &error));
  g_assert_no_error (error);
  {
    GimpUndo *group = gimp_undo_stack_peek (gimp_image_get_undo_stack (image));
    g_assert_true (GIMP_IS_UNDO_STACK (group));
    g_assert_true (GIMP_IS_CLONE_LAYER_UNDO (gimp_container_get_child_by_index (GIMP_UNDO_STACK (group)->undos, 0)));
  }
  g_assert_cmpint (gimp_undo_stack_get_depth (gimp_image_get_undo_stack (image)), ==, 1);
  g_assert_cmpint (gimp_item_get_width (GIMP_ITEM (clone)), ==, 20);
  pixel (GIMP_LAYER (clone), 0, 0, 255, 255);
  /* Restoring a live source projects its current pixels, not stale cached paint. */
  fill (first, 0, 255, 0, 255);
  for (gint cycle = 0; cycle < 2; ++cycle)
    {
      g_assert_true (gimp_image_undo (image));
      g_assert_true (gimp_clone_layer_get_source (clone) == first);
      g_assert_cmpint (gimp_clone_layer_get_source_state (clone), ==, GIMP_CLONE_SOURCE_LIVE);
      g_assert_cmpint (gimp_item_get_width (GIMP_ITEM (clone)), ==, 16);
      g_assert_cmpint (gimp_item_get_offset_x (GIMP_ITEM (clone)), ==, 32);
      pixel (GIMP_LAYER (clone), 0, 255, 0, 255);
      g_assert_cmpint (gimp_undo_stack_get_depth (gimp_image_get_undo_stack (image)), ==, 0);
      g_assert_true (gimp_image_redo (image));
      g_assert_true (gimp_clone_layer_get_source (clone) == second);
      g_assert_cmpint (gimp_item_get_width (GIMP_ITEM (clone)), ==, 20);
      pixel (GIMP_LAYER (clone), 0, 0, 255, 255);
      g_assert_cmpint (gimp_undo_stack_get_depth (gimp_image_get_undo_stack (image)), ==, 1);
    }
  g_object_unref (context); g_object_unref (image);
}
static void source_reference_undo_dependent_clone (void)
{
  GimpImage *image = new_image ();
  GimpLayer *first = new_layer (image, "first", NULL, 0);
  GimpLayer *second = new_layer (image, "second", NULL, 1);
  GimpCloneLayer *clone = new_clone (image, first);
  GimpCloneLayer *dependent = new_clone (image, GIMP_LAYER (clone));
  GimpContext *context = gimp_context_new (gimp, "Test", NULL);
  GError *error = NULL;
  fill (first, 255, 0, 0, 255); fill (second, 0, 0, 255, 255);
  gimp_item_resize (GIMP_ITEM (second), context, GIMP_FILL_TRANSPARENT, 20, 18, 0, 0);
  gimp_image_undo_free (image);
  g_assert_true (gimp_clone_layer_set_source_with_undo (clone, second, NULL, &error));
  g_assert_cmpint (gimp_undo_stack_get_depth (gimp_image_get_undo_stack (image)), ==, 1);
  g_assert_cmpint (gimp_item_get_width (GIMP_ITEM (dependent)), ==, 20);
  g_assert_true (gimp_image_undo (image));
  g_assert_true (gimp_clone_layer_get_source (clone) == first);
  g_assert_cmpint (gimp_item_get_width (GIMP_ITEM (dependent)), ==, 16);
  pixel (GIMP_LAYER (dependent), 255, 0, 0, 255);
  g_assert_true (gimp_image_redo (image));
  g_assert_true (gimp_clone_layer_get_source (clone) == second);
  g_assert_cmpint (gimp_item_get_width (GIMP_ITEM (dependent)), ==, 20);
  pixel (GIMP_LAYER (dependent), 0, 0, 255, 255);
  g_assert_no_error (error);
  g_object_unref (context); g_object_unref (image);
}
static void close_clone_on_dirty (GimpImage *image, GimpDirtyMask dirty, gpointer data)
{ gimp_painter_binding_close (G_OBJECT (data), NULL); }
static void source_reference_undo_close_reentry (void)
{
  GimpImage *image = new_image ();
  GimpLayer *source = new_layer (image, "source", NULL, 0);
  GimpCloneLayer *clone = new_clone (image, NULL);
  GError *error = NULL;
  gulong id;
  fill (GIMP_LAYER (clone), 0, 0, 255, 255);
  fill (source, 255, 0, 0, 255);
  id = g_signal_connect (image, "dirty", G_CALLBACK (close_clone_on_dirty), clone);
  g_assert_false (gimp_clone_layer_set_source_with_undo (clone, source, NULL, &error));
  g_assert_error (error, GIMP_PAINTER_ERROR, GIMP_PAINTER_ERROR_CLOSED);
  g_clear_error (&error);
  g_signal_handler_disconnect (image, id);
  g_assert_cmpint (gimp_image_get_undo_group_count (image), ==, 0);
  pixel (GIMP_LAYER (clone), 0, 0, 255, 255);
  g_assert_null (gimp_clone_layer_get_source (clone));
  g_object_unref (image);
}
static void source_reference_undo_all_states (void)
{
  GimpImage *image = new_image ();
  GimpLayer *first = new_layer (image, "first", NULL, 0);
  GimpLayer *second = new_layer (image, "second", NULL, 1);
  GimpCloneLayer *clone = new_clone (image, NULL);
  GError *error = NULL;
  gchar *name;
  fill (first, 255, 0, 0, 255); fill (second, 0, 255, 0, 255);
  fill (GIMP_LAYER (clone), 0, 0, 255, 255);
  gimp_image_undo_free (image);
  g_assert_true (gimp_clone_layer_set_source_with_undo (clone, first, NULL, &error));
  g_assert_true (gimp_image_undo (image));
  g_assert_cmpint (gimp_clone_layer_get_source_state (clone), ==, GIMP_CLONE_SOURCE_NONE);
  g_assert_null (gimp_clone_layer_get_source (clone));
  pixel (GIMP_LAYER (clone), 0, 0, 255, 255);
  g_assert_true (gimp_image_redo (image));
  gimp_image_undo_free (image);
  gimp_clone_layer_set_source_by_name (clone, "still missing");
  g_assert_true (gimp_clone_layer_set_source_with_undo (clone, second, NULL, &error));
  g_assert_true (gimp_image_undo (image));
  g_assert_cmpint (gimp_clone_layer_get_source_state (clone), ==, GIMP_CLONE_SOURCE_PENDING);
  name = gimp_clone_layer_dup_source_name (clone);
  g_assert_cmpstr (name, ==, "still missing"); g_free (name);
  g_assert_true (gimp_clone_layer_get_source (clone) == first);
  g_assert_true (gimp_image_redo (image));
  g_assert_true (gimp_clone_layer_get_source (clone) == second);
  gimp_image_undo_free (image);
  gimp_clone_layer_set_source (clone, NULL);
  g_assert_true (gimp_clone_layer_set_source_name_with_undo (clone, "new pending", NULL, &error));
  g_assert_true (gimp_image_undo (image));
  name = gimp_clone_layer_dup_source_name (clone);
  g_assert_cmpstr (name, ==, "still missing"); g_free (name);
  g_assert_null (gimp_clone_layer_get_source (clone));
  g_assert_true (gimp_image_redo (image));
  name = gimp_clone_layer_dup_source_name (clone);
  g_assert_cmpstr (name, ==, "new pending"); g_free (name);
  gimp_clone_layer_set_source_by_name (clone, NULL);
  gimp_clone_layer_set_source (clone, second);
  gimp_image_undo_free (image);
  gimp_image_remove_layer (image, second, FALSE, NULL);
  g_assert_cmpint (gimp_clone_layer_get_source_state (clone), ==, GIMP_CLONE_SOURCE_EXPIRED);
  g_assert_true (gimp_clone_layer_set_source_with_undo (clone, first, NULL, &error));
  g_assert_true (gimp_image_undo (image));
  g_assert_cmpint (gimp_clone_layer_get_source_state (clone), ==, GIMP_CLONE_SOURCE_EXPIRED);
  g_assert_null (gimp_clone_layer_get_source (clone));
  name = gimp_clone_layer_dup_source_name (clone);
  g_assert_cmpstr (name, ==, "second"); g_free (name);
  pixel (GIMP_LAYER (clone), 0, 255, 0, 255);
  g_assert_true (gimp_image_redo (image));
  g_assert_true (gimp_clone_layer_get_source (clone) == first);
  g_assert_no_error (error);
  g_object_unref (image);
}
static void source_reference_undo_preserves_pending_live (void)
{
  GimpImage *image = new_image ();
  GimpLayer *first = new_layer (image, "first", NULL, 0);
  GimpLayer *second = new_layer (image, "second", NULL, 1);
  GimpLayer *future;
  GimpCloneLayer *clone = new_clone (image, first);
  GError *error = NULL;
  fill (first, 255, 0, 0, 255); fill (second, 0, 255, 0, 255);
  gimp_clone_layer_set_source_by_name (clone, "future source");
  gimp_image_undo_free (image);
  g_assert_true (gimp_clone_layer_set_source_with_undo (clone, second, NULL, &error));
  future = new_layer (image, "future source", NULL, 0);
  fill (future, 0, 0, 255, 255);
  g_assert_true (gimp_image_undo (image));
  while (g_main_context_iteration (NULL, FALSE));
  /* Replay itself must not resolve an originally pending name. */
  g_assert_cmpint (gimp_clone_layer_get_source_state (clone), ==, GIMP_CLONE_SOURCE_PENDING);
  pixel (GIMP_LAYER (clone), 255, 0, 0, 255);
  g_assert_true (gimp_clone_layer_get_source (clone) == future);
  pixel (GIMP_LAYER (clone), 0, 0, 255, 255);
  g_assert_no_error (error);
  g_object_unref (image);
}
static void source_reference_undo_renamed_then_expired (void)
{
  GimpImage *image = new_image ();
  GimpLayer *first = new_layer (image, "first", NULL, 0);
  GimpLayer *second = new_layer (image, "second", NULL, 1);
  GimpCloneLayer *clone = new_clone (image, first);
  GError *error = NULL;
  gchar *name;
  fill (first, 255, 0, 0, 255); fill (second, 0, 255, 0, 255);
  gimp_image_undo_free (image);
  g_assert_true (gimp_clone_layer_set_source_with_undo (clone, second, NULL, &error));
  gimp_object_set_name (GIMP_OBJECT (first), "renamed source");
  g_assert_true (gimp_image_undo (image));
  g_assert_true (gimp_clone_layer_get_source (clone) == first);
  gimp_image_remove_layer (image, first, FALSE, NULL);
  name = gimp_clone_layer_dup_source_name (clone);
  g_assert_cmpstr (name, ==, "renamed source"); g_free (name);
  g_assert_cmpint (gimp_clone_layer_get_source_state (clone), ==, GIMP_CLONE_SOURCE_EXPIRED);
  g_assert_true (gimp_image_redo (image));
  g_assert_true (gimp_clone_layer_get_source (clone) == second);
  g_assert_true (gimp_image_undo (image));
  new_layer (image, "renamed source", NULL, 0);
  g_assert_cmpint (gimp_clone_layer_get_source_state (clone), ==, GIMP_CLONE_SOURCE_EXPIRED);
  g_assert_null (gimp_clone_layer_get_source (clone));
  pixel (GIMP_LAYER (clone), 255, 0, 0, 255);
  g_assert_no_error (error);
  g_object_unref (image);
}
static void source_reference_undo_expired_cross_image (void)
{
  GimpImage *source_image = new_image ();
  GimpImage *image = new_image ();
  GimpLayer *source = new_layer (source_image, "remote source", NULL, 0);
  GimpLayer *replacement = new_layer (image, "replacement", NULL, 0);
  GimpCloneLayer *clone = new_clone (image, source);
  GError *error = NULL;
  gchar *name;
  fill (source, 255, 0, 0, 255); fill (replacement, 0, 255, 0, 255);
  gimp_image_undo_free (image);
  g_assert_true (gimp_clone_layer_set_source_with_undo (clone, replacement, NULL, &error));
  /* The snapshot must not prolong a foreign layer beyond its image lifetime. */
  g_object_unref (source_image);
  g_assert_true (gimp_image_undo (image));
  g_assert_cmpint (gimp_clone_layer_get_source_state (clone), ==, GIMP_CLONE_SOURCE_EXPIRED);
  g_assert_null (gimp_clone_layer_get_source (clone));
  name = gimp_clone_layer_dup_source_name (clone);
  g_assert_cmpstr (name, ==, "remote source"); g_free (name);
  pixel (GIMP_LAYER (clone), 255, 0, 0, 255);
  g_assert_true (gimp_image_redo (image));
  g_assert_true (gimp_clone_layer_get_source (clone) == replacement);
  pixel (GIMP_LAYER (clone), 0, 255, 0, 255);
  g_assert_no_error (error);
  g_object_unref (image);
}
static void count_source_update (GimpDrawable *drawable, gint x, gint y, gint w, gint h, gpointer data)
{ ++*((gint *) data); }
static void serialization_reference_snapshot_and_restore (void)
{
  GimpImage *image = new_image ();
  GimpLayer *source = new_layer (image, "target", NULL, 0);
  GimpCloneLayer *clone = new_clone (image, NULL);
  GimpCloneLayerReference *snapshot;
  GimpCloneLayerReference metadata = { 0 };
  GimpItem *copy;
  gint updates = 0;
  gulong update_handler;
  GError *error = NULL;
  fill (source, 255, 0, 0, 255);
  fill (GIMP_LAYER (clone), 0, 0, 255, 255);
  update_handler = g_signal_connect (source, "update", G_CALLBACK (count_source_update), &updates);
  gimp_clone_layer_set_source_by_name (clone, "target");
  snapshot = gimp_clone_layer_dup_reference (clone, &error);
  g_assert_no_error (error);
  g_assert_null (snapshot->source);
  g_assert_cmpstr (snapshot->pending_name, ==, "target");
  g_assert_cmpint (snapshot->state, ==, GIMP_CLONE_SOURCE_PENDING);
  g_assert_true (snapshot->allow_name_lookup);
  g_assert_cmpint (updates, ==, 0);
  pixel (GIMP_LAYER (clone), 0, 0, 255, 255);
  gimp_clone_layer_reference_free (snapshot);
  metadata.source = source;
  metadata.source_name = "target";
  metadata.pending_name = "unresolved pending";
  metadata.state = GIMP_CLONE_SOURCE_PENDING;
  metadata.allow_name_lookup = FALSE;
  g_assert_true (gimp_clone_layer_restore_reference (clone, &metadata, &error));
  /* Restore is metadata-only and must preserve cache and source callbacks. */
  g_assert_cmpint (updates, ==, 0);
  pixel (GIMP_LAYER (clone), 0, 0, 255, 255);
  snapshot = gimp_clone_layer_dup_reference (clone, &error);
  g_assert_true (snapshot->source == source);
  g_assert_cmpstr (snapshot->pending_name, ==, "unresolved pending");
  g_assert_false (snapshot->allow_name_lookup);
  gimp_clone_layer_reference_free (snapshot);
  gimp_drawable_update (GIMP_DRAWABLE (source), 0, 0, 16, 16);
  pixel (GIMP_LAYER (clone), 255, 0, 0, 255);
  metadata.source = NULL; metadata.pending_name = NULL;
  metadata.source_expired = TRUE; metadata.state = GIMP_CLONE_SOURCE_EXPIRED;
  g_assert_true (gimp_clone_layer_restore_reference (clone, &metadata, &error));
  g_assert_null (gimp_clone_layer_get_source (clone));
  copy = gimp_item_duplicate (GIMP_ITEM (clone), GIMP_TYPE_CLONE_LAYER);
  g_object_ref_sink (copy);
  snapshot = gimp_clone_layer_dup_reference (GIMP_CLONE_LAYER (copy), &error);
  g_assert_cmpint (snapshot->state, ==, GIMP_CLONE_SOURCE_EXPIRED);
  g_assert_cmpstr (snapshot->source_name, ==, "target");
  g_assert_false (snapshot->allow_name_lookup);
  gimp_clone_layer_reference_free (snapshot); g_object_unref (copy);
  metadata.pending_name = "target"; metadata.state = GIMP_CLONE_SOURCE_PENDING;
  g_assert_true (gimp_clone_layer_restore_reference (clone, &metadata, &error));
  g_assert_null (gimp_clone_layer_get_source (clone));
  copy = gimp_item_duplicate (GIMP_ITEM (clone), GIMP_TYPE_CLONE_LAYER);
  g_object_ref_sink (copy);
  g_assert_null (gimp_clone_layer_get_source (GIMP_CLONE_LAYER (copy)));
  snapshot = gimp_clone_layer_dup_reference (GIMP_CLONE_LAYER (copy), &error);
  g_assert_cmpint (snapshot->state, ==, GIMP_CLONE_SOURCE_PENDING);
  g_assert_false (snapshot->allow_name_lookup);
  gimp_clone_layer_reference_free (snapshot); g_object_unref (copy);
  gimp_image_undo_free (image);
  g_assert_true (gimp_clone_layer_set_source_name_with_undo (clone, "target", NULL, &error));
  g_assert_true (gimp_clone_layer_get_source (clone) == source);
  g_assert_true (gimp_image_undo (image));
  g_assert_null (gimp_clone_layer_get_source (clone));
  snapshot = gimp_clone_layer_dup_reference (clone, &error);
  g_assert_false (snapshot->allow_name_lookup);
  gimp_clone_layer_reference_free (snapshot);
  fill (source, 0, 255, 0, 255);
  pixel (GIMP_LAYER (clone), 255, 0, 0, 255);
  /* An explicit legacy/user name assignment re-enables the old lookup rule. */
  gimp_clone_layer_set_source_by_name (clone, "target");
  g_assert_true (gimp_clone_layer_get_source (clone) == source);
  pixel (GIMP_LAYER (clone), 0, 255, 0, 255);
  metadata.state = GIMP_CLONE_SOURCE_LIVE;
  g_assert_false (gimp_clone_layer_restore_reference (clone, &metadata, &error));
  g_assert_error (error, GIMP_PAINTER_ERROR, GIMP_PAINTER_ERROR_INVALID_STATE);
  g_clear_error (&error);
  g_assert_true (gimp_clone_layer_get_source (clone) == source);
  g_signal_handler_disconnect (source, update_handler);
  g_object_unref (image);
}
static void undo_source_image_relocation (void)
{
  GimpImage *original = new_image ();
  GimpImage *destination = new_image ();
  GimpLayer *source = new_layer (original, "source", NULL, 0);
  GimpLayer *replacement = new_layer (destination, "replacement", NULL, 0);
  GimpCloneLayer *clone = new_clone (destination, source);
  GError *error = NULL;
  fill (source, 255, 0, 0, 255); fill (replacement, 0, 255, 0, 255);
  gimp_image_undo_free (destination);
  g_assert_true (gimp_clone_layer_set_source_with_undo (clone, replacement, NULL, &error));
  g_object_ref (source);
  gimp_image_remove_layer (original, source, FALSE, NULL);
  gimp_item_unset_removed (GIMP_ITEM (source));
  GIMP_ITEM_GET_CLASS (source)->convert (GIMP_ITEM (source), destination, GIMP_TYPE_LAYER);
  gimp_image_add_layer (destination, source, NULL, 0, FALSE);
  g_object_unref (source);
  g_object_unref (original);
  g_assert_true (gimp_image_undo (destination));
  g_assert_true (gimp_clone_layer_get_source (clone) == source);
  pixel (GIMP_LAYER (clone), 255, 0, 0, 255);
  g_assert_true (gimp_image_redo (destination));
  g_assert_true (gimp_clone_layer_get_source (clone) == replacement);
  g_assert_no_error (error);
  g_object_unref (destination);
}
static void clone_owner_image_relocation (void)
{
  GimpImage *original = new_image ();
  GimpImage *destination = new_image ();
  GimpLayer *source = new_layer (original, "source", NULL, 0);
  GimpLayer *replacement = new_layer (destination, "replacement", NULL, 0);
  GimpCloneLayer *clone = new_clone (original, source);
  GError *error = NULL;
  fill (source, 255, 0, 0, 255); fill (replacement, 0, 255, 0, 255);
  g_object_ref (clone);
  gimp_image_remove_layer (original, GIMP_LAYER (clone), FALSE, NULL);
  gimp_item_unset_removed (GIMP_ITEM (clone));
  GIMP_ITEM_GET_CLASS (clone)->convert (GIMP_ITEM (clone), destination, GIMP_TYPE_CLONE_LAYER);
  gimp_image_add_layer (destination, GIMP_LAYER (clone), NULL, 0, FALSE);
  g_object_unref (clone);
  g_object_unref (original);
  g_assert_cmpint (gimp_clone_layer_get_source_state (clone), ==, GIMP_CLONE_SOURCE_EXPIRED);
  g_assert_true (gimp_clone_layer_set_source_full (clone, replacement, &error));
  g_assert_no_error (error);
  pixel (GIMP_LAYER (clone), 0, 255, 0, 255);
  g_object_unref (destination);
}
static void cross_trace_pixel (const gchar *step, const gchar *target, GimpLayer *layer,
                               gint r, gint g, gint b, gint a)
{
  guchar value[4];
  gegl_buffer_get (gimp_drawable_get_buffer (GIMP_DRAWABLE (layer)), GEGL_RECTANGLE (0, 0, 1, 1),
                    1.0, babl_format ("R'G'B'A u8"), value, GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);
  g_assert_cmpint (value[0], ==, r); g_assert_cmpint (value[1], ==, g);
  g_assert_cmpint (value[2], ==, b); g_assert_cmpint (value[3], ==, a);
  g_test_message ("CROSS_PIXEL step=%s target=%s rgba=%d,%d,%d,%d wh=%d,%d xy=%d,%d",
                  step, target, value[0], value[1], value[2], value[3],
                  gimp_item_get_width (GIMP_ITEM (layer)), gimp_item_get_height (GIMP_ITEM (layer)),
                  gimp_item_get_offset_x (GIMP_ITEM (layer)), gimp_item_get_offset_y (GIMP_ITEM (layer)));
}
static void cross_image_copy_and_move (void)
{
  GimpImage *original = gimp_image_new (gimp, 64, 64, GIMP_RGB, GIMP_PRECISION_U8_NON_LINEAR);
  GimpImage *destination = gimp_image_new (gimp, 64, 64, GIMP_RGB, GIMP_PRECISION_U8_NON_LINEAR);
  GimpLayer *source = new_layer (original, "source", NULL, 0);
  GimpCloneLayer *clone = new_clone (original, source);
  GimpLayer *copy, *group, *inside, *internal_clone, *group_copy;
  GimpLayer *copied_inside, *copied_internal, *copied_external;
  fill (source, 204, 51, 102, 128);
  gimp_item_set_offset (GIMP_ITEM (clone), 32, 30);
  copy = GIMP_LAYER (gimp_item_convert (GIMP_ITEM (clone), destination, GIMP_TYPE_CLONE_LAYER));
  g_assert_true (gimp_image_add_layer (destination, copy, NULL, 0, FALSE));
  g_assert_true (gimp_item_get_image (GIMP_ITEM (copy)) == destination);
  g_assert_true (gimp_item_get_image (GIMP_ITEM (source)) == original);
  g_assert_true (gimp_clone_layer_get_source (GIMP_CLONE_LAYER (copy)) == source);
  g_test_message ("CROSS_REF step=direct-copy source_original=1 clone_destination=1 same_source=1");
  cross_trace_pixel ("direct-copy", "copy", copy, 204, 51, 102, 128);
  fill (source, 255, 0, 0, 255);
  cross_trace_pixel ("source-red", "original", GIMP_LAYER (clone), 255, 0, 0, 255);
  cross_trace_pixel ("source-red", "copy", copy, 255, 0, 0, 255);
  group = gimp_group_layer_new (original);
  g_assert_true (gimp_image_add_layer (original, group, NULL, 0, FALSE));
  inside = new_layer (original, "inside", group, 0);
  fill (inside, 0, 0, 255, 255);
  internal_clone = add_clone_child (original, group, "internal", inside);
  add_clone_child (original, group, "external", source);
  group_copy = GIMP_LAYER (gimp_item_convert (GIMP_ITEM (group), destination, GIMP_TYPE_GROUP_LAYER));
  g_assert_true (gimp_image_add_layer (destination, group_copy, NULL, 0, FALSE));
  copied_inside = child_named (group_copy, "inside");
  copied_internal = child_named (group_copy, "internal");
  copied_external = child_named (group_copy, "external");
  g_assert_true (gimp_clone_layer_get_source (GIMP_CLONE_LAYER (copied_internal)) == copied_inside);
  g_assert_true (gimp_clone_layer_get_source (GIMP_CLONE_LAYER (copied_external)) == source);
  g_assert_true (gimp_item_get_image (GIMP_ITEM (copied_inside)) == destination);
  g_assert_true (gimp_clone_layer_get_source (GIMP_CLONE_LAYER (internal_clone)) == inside);
  g_test_message ("CROSS_REF step=group-copy inside_destination=1 internal_remapped=1 external_original=1 original_unchanged=1");
  cross_trace_pixel ("group-copy", "internal", copied_internal, 0, 0, 255, 255);
  cross_trace_pixel ("group-copy", "external", copied_external, 255, 0, 0, 255);
  fill (inside, 255, 255, 0, 255);
  cross_trace_pixel ("original-inside-yellow", "original-internal", internal_clone, 255, 255, 0, 255);
  cross_trace_pixel ("original-inside-yellow", "copied-internal", copied_internal, 0, 0, 255, 255);
  fill (copied_inside, 0, 255, 255, 255);
  cross_trace_pixel ("copied-inside-cyan", "original-internal", internal_clone, 255, 255, 0, 255);
  cross_trace_pixel ("copied-inside-cyan", "copied-internal", copied_internal, 0, 255, 255, 255);
  /* Ordinary source relocation keeps the same live identity. */
  g_object_ref (source);
  gimp_image_remove_layer (original, source, FALSE, NULL);
  gimp_item_unset_removed (GIMP_ITEM (source));
  GIMP_ITEM_GET_CLASS (source)->convert (GIMP_ITEM (source), destination, GIMP_TYPE_LAYER);
  g_assert_true (gimp_image_add_layer (destination, source, NULL, 0, FALSE));
  g_object_unref (source);
  g_assert_true (gimp_item_get_image (GIMP_ITEM (source)) == destination);
  g_assert_true (gimp_clone_layer_get_source (clone) == source);
  g_assert_true (gimp_clone_layer_get_source (GIMP_CLONE_LAYER (copy)) == source);
  g_assert_true (gimp_clone_layer_get_source (GIMP_CLONE_LAYER (copied_external)) == source);
  g_test_message ("CROSS_REF step=source-relocated source_destination=1 original_same_source=1 copy_same_source=1 group_external_same_source=1");
  fill (source, 0, 255, 0, 255);
  cross_trace_pixel ("source-relocated-green", "original", GIMP_LAYER (clone), 0, 255, 0, 255);
  cross_trace_pixel ("source-relocated-green", "copy", copy, 0, 255, 0, 255);
  cross_trace_pixel ("source-relocated-green", "group-external", copied_external, 0, 255, 0, 255);
  g_object_unref (original);
  g_assert_true (gimp_clone_layer_get_source (GIMP_CLONE_LAYER (copy)) == source);
  g_assert_true (gimp_clone_layer_get_source (GIMP_CLONE_LAYER (copied_external)) == source);
  g_assert_true (gimp_clone_layer_get_source (GIMP_CLONE_LAYER (copied_internal)) == copied_inside);
  g_test_message ("CROSS_REF step=original-closed copy_same_source=1 group_external_same_source=1 internal_remapped=1");
  fill (source, 255, 0, 255, 255);
  cross_trace_pixel ("original-closed-magenta", "copy", copy, 255, 0, 255, 255);
  cross_trace_pixel ("original-closed-magenta", "group-external", copied_external, 255, 0, 255, 255);
  cross_trace_pixel ("original-closed-magenta", "group-internal", copied_internal, 0, 255, 255, 255);
  g_object_unref (destination);
}
static void inherited_precision_conversion_undo (void)
{
  GimpImage *image = new_image ();
  GimpLayer *source = new_layer (image, "source", NULL, 0);
  GimpCloneLayer *clone = new_clone (image, source);
  fill (source, 204, 51, 102, 128);
  gimp_image_undo_free (image);
  gimp_image_convert_precision (image, GIMP_PRECISION_FLOAT_LINEAR,
    GEGL_DITHER_NONE, GEGL_DITHER_NONE, GEGL_DITHER_NONE, NULL);
  g_assert_cmpint (gimp_drawable_get_precision (GIMP_DRAWABLE (clone)), ==, GIMP_PRECISION_FLOAT_LINEAR);
  g_assert_true (gimp_clone_layer_get_source (clone) == source);
  g_assert_true (gimp_image_undo (image));
  g_assert_cmpint (gimp_drawable_get_precision (GIMP_DRAWABLE (clone)), ==, GIMP_PRECISION_U8_NON_LINEAR);
  pixel (GIMP_LAYER (clone), 204, 51, 102, 128);
  g_assert_true (gimp_image_redo (image));
  g_assert_cmpint (gimp_drawable_get_precision (GIMP_DRAWABLE (clone)), ==, GIMP_PRECISION_FLOAT_LINEAR);
  fill (source, 255, 0, 0, 255);
  pixel (GIMP_LAYER (clone), 255, 0, 0, 255);
  g_object_unref (image);
}
static void trace_undo_tree (GimpUndo *undo, gint depth)
{
  if (!undo) return;
  g_test_message ("UNDO_NODE depth=%d type=%s undo_type=%d item=%s", depth,
    G_OBJECT_TYPE_NAME (undo), undo->undo_type,
    GIMP_IS_ITEM_UNDO (undo) ? gimp_object_get_name (GIMP_ITEM_UNDO (undo)->item) : "-");
  if (GIMP_IS_DRAWABLE_MOD_UNDO (undo))
    {
      GimpDrawableModUndo *mod = GIMP_DRAWABLE_MOD_UNDO (undo);
      g_test_message ("UNDO_BUFFER depth=%d wh=%d,%d xy=%d,%d", depth,
        gegl_buffer_get_width (mod->buffer), gegl_buffer_get_height (mod->buffer), mod->offset_x, mod->offset_y);
    }
  if (GIMP_IS_UNDO_STACK (undo))
    for (gint i = 0; i < gimp_container_get_n_children (GIMP_UNDO_STACK (undo)->undos); ++i)
      trace_undo_tree (GIMP_UNDO (gimp_container_get_child_by_index (GIMP_UNDO_STACK (undo)->undos, i)), depth + 1);
}
static void trace_resize_snapshot (GimpImage *image, GimpLayer *source, GimpLayer *clone,
                                    const gchar *stage, gboolean with_mask)
{
  GimpLayerMask *mask = gimp_layer_get_mask (clone);
  g_test_message ("RESIZE_TRACE mask=%d stage=%s source_wh=%d,%d source_xy=%d,%d clone_wh=%d,%d clone_xy=%d,%d mask_wh=%d,%d undo=%d redo=%d",
    with_mask, stage, gimp_item_get_width (GIMP_ITEM (source)), gimp_item_get_height (GIMP_ITEM (source)),
    gimp_item_get_offset_x (GIMP_ITEM (source)), gimp_item_get_offset_y (GIMP_ITEM (source)),
    gimp_item_get_width (GIMP_ITEM (clone)), gimp_item_get_height (GIMP_ITEM (clone)),
    gimp_item_get_offset_x (GIMP_ITEM (clone)), gimp_item_get_offset_y (GIMP_ITEM (clone)),
    mask ? gimp_item_get_width (GIMP_ITEM (mask)) : 0, mask ? gimp_item_get_height (GIMP_ITEM (mask)) : 0,
    gimp_undo_stack_get_depth (gimp_image_get_undo_stack (image)),
    gimp_undo_stack_get_depth (gimp_image_get_redo_stack (image)));
  trace_undo_tree (gimp_undo_stack_peek (gimp_image_get_undo_stack (image)), 0);
  trace_undo_tree (gimp_undo_stack_peek (gimp_image_get_redo_stack (image)), 0);
}
static void source_resize_undo_legacy_fixture (void)
{
  for (gint with_mask = 0; with_mask < 2; ++with_mask)
    {
      GimpImage *image = new_image ();
      GimpLayer *source = new_layer (image, "resize source", NULL, 0);
      GimpLayer *clone = GIMP_LAYER (new_clone (image, source));
      GimpContext *context = gimp_context_new (gimp, "Test", NULL);
      fill (source, 204, 51, 102, 255);
      gimp_item_set_offset (GIMP_ITEM (clone), 32, 30);
      gimp_item_set_offset (GIMP_ITEM (source), 5, 7);
      gimp_drawable_update (GIMP_DRAWABLE (source), 0, 0, 16, 16);
      if (with_mask)
        gimp_layer_add_mask (clone, gimp_layer_create_mask (clone, GIMP_ADD_MASK_WHITE, NULL), FALSE, NULL);
      gimp_image_undo_free (image);
      trace_resize_snapshot (image, source, clone, "baseline", with_mask);
      gimp_item_resize (GIMP_ITEM (source), context, GIMP_FILL_TRANSPARENT, 20, 18, -2, -3);
      trace_resize_snapshot (image, source, clone, "resized", with_mask);
      g_assert_cmpint (gimp_undo_stack_get_depth (gimp_image_get_undo_stack (image)), ==, 1);
      {
        GimpUndo *top = gimp_undo_stack_peek (gimp_image_get_undo_stack (image));
        GimpContainer *children;
        GimpDrawableModUndo *clone_undo;
        g_assert_true (GIMP_IS_UNDO_STACK (top));
        children = GIMP_UNDO_STACK (top)->undos;
        g_assert_cmpint (gimp_container_get_n_children (children), ==, 2);
        clone_undo = GIMP_DRAWABLE_MOD_UNDO (gimp_container_get_child_by_index (children, 0));
        g_assert_true (GIMP_ITEM_UNDO (clone_undo)->item == GIMP_ITEM (clone));
        g_assert_cmpint (gegl_buffer_get_width (clone_undo->buffer), ==, 16);
        g_assert_cmpint (gegl_buffer_get_height (clone_undo->buffer), ==, 16);
        g_assert_cmpint (clone_undo->offset_x, ==, 32);
        g_assert_cmpint (clone_undo->offset_y, ==, 30);
        g_assert_true (GIMP_ITEM_UNDO (gimp_container_get_child_by_index (children, 1))->item == GIMP_ITEM (source));
      }
      if (with_mask)
        {
          GimpItem *mask = GIMP_ITEM (gimp_layer_get_mask (clone));
          g_assert_cmpint (gimp_item_get_width (mask), ==, 16);
          g_assert_cmpint (gimp_item_get_height (mask), ==, 16);
          g_assert_cmpint (gimp_item_get_offset_x (mask), ==, 32);
          g_assert_cmpint (gimp_item_get_offset_y (mask), ==, 30);
        }
      for (gint repeat = 0; repeat < 2; ++repeat)
        {
          g_assert_true (gimp_image_undo (image));
          trace_resize_snapshot (image, source, clone, repeat ? "undo2" : "undo1", with_mask);
          g_assert_cmpint (gimp_item_get_width (GIMP_ITEM (clone)), ==, 16);
          g_assert_cmpint (gimp_item_get_height (GIMP_ITEM (clone)), ==, 16);
          g_assert_cmpint (gimp_item_get_offset_x (GIMP_ITEM (clone)), ==, 30);
          g_assert_cmpint (gimp_item_get_offset_y (GIMP_ITEM (clone)), ==, 27);
          g_assert_cmpint (gimp_undo_stack_get_depth (gimp_image_get_undo_stack (image)), ==, repeat ? 4 : 2);
          g_assert_cmpint (gimp_undo_stack_get_depth (gimp_image_get_redo_stack (image)), ==, 1);
          g_assert_true (gimp_image_redo (image));
          trace_resize_snapshot (image, source, clone, repeat ? "redo2" : "redo1", with_mask);
          g_assert_cmpint (gimp_item_get_offset_x (GIMP_ITEM (clone)), ==, 34);
          g_assert_cmpint (gimp_item_get_offset_y (GIMP_ITEM (clone)), ==, 33);
          g_assert_cmpint (gimp_undo_stack_get_depth (gimp_image_get_undo_stack (image)), ==, repeat ? 6 : 4);
          g_assert_cmpint (gimp_undo_stack_get_depth (gimp_image_get_redo_stack (image)), ==, 0);
          if (with_mask)
            {
              GimpItem *mask = GIMP_ITEM (gimp_layer_get_mask (clone));
              g_assert_cmpint (gimp_item_get_width (mask), ==, 16);
              g_assert_cmpint (gimp_item_get_height (mask), ==, 16);
              g_assert_cmpint (gimp_item_get_offset_x (mask), ==, 32);
              g_assert_cmpint (gimp_item_get_offset_y (mask), ==, 30);
            }
        }
      /* legacy-clone-undo/capture.log, including non-ideal offset/history behavior. */
      g_assert_cmpint (gimp_item_get_width (GIMP_ITEM (source)), ==, 20);
      g_assert_cmpint (gimp_item_get_width (GIMP_ITEM (clone)), ==, 20);
      g_object_unref (context); g_object_unref (image);
    }
}
static void source_delete_undo (void)
{
  GimpImage *image = new_image ();
  GimpLayer *source = new_layer (image, "source", NULL, 0);
  GimpCloneLayer *clone = new_clone (image, source);
  fill (source, 204, 51, 102, 128);
  gimp_image_remove_layer (image, source, TRUE, NULL);
  /* The Undo owns the removed source; weak identity stays valid. */
  g_assert_true (gimp_clone_layer_get_source (clone) == source);
  g_assert_true (gimp_image_undo (image));
  g_assert_true (gimp_clone_layer_get_source (clone) == source);
  fill (source, 255, 0, 0, 255);
  pixel (GIMP_LAYER (clone), 255, 0, 0, 255);
  g_assert_true (gimp_image_redo (image));
  g_assert_true (gimp_image_undo (image));
  fill (source, 0, 255, 0, 255);
  pixel (GIMP_LAYER (clone), 0, 255, 0, 255);
  g_object_unref (image);
}
static void freeze_and_close (void)
{
  GError *error;

  GimpImage *image = new_image ();
  GimpLayer *source = new_layer (image, "source", NULL, 0);
  GimpCloneLayer *clone = new_clone (image, source);
  gimp_viewable_preview_freeze (GIMP_VIEWABLE (source));
  g_assert_true (gimp_viewable_preview_is_frozen (GIMP_VIEWABLE (clone)));
  gimp_clone_layer_set_source (clone, NULL);
  g_assert_false (gimp_viewable_preview_is_frozen (GIMP_VIEWABLE (clone)));
  gimp_viewable_preview_thaw (GIMP_VIEWABLE (source));
  gimp_clone_layer_set_source (clone, source);
  g_assert_true (gimp_painter_binding_close (G_OBJECT (clone), NULL));
  g_assert_true (gimp_painter_binding_close (G_OBJECT (clone), NULL));
  fill (source, 255, 0, 0, 255);
  error = NULL;
  g_assert_false (gimp_clone_layer_set_source_full (clone, source, &error));
  g_assert_error (error, GIMP_PAINTER_ERROR, GIMP_PAINTER_ERROR_CLOSED);
  g_clear_error (&error);
  g_object_unref (image);
}
static void cycles (void)
{
  GimpLayer *group;
  GimpLayer *inside;

  GimpImage *image = new_image ();
  GimpCloneLayer *first = new_clone (image, NULL);
  GimpCloneLayer *second = new_clone (image, GIMP_LAYER (first));
  gimp_clone_layer_set_source (first, GIMP_LAYER (second));
  gimp_drawable_update (GIMP_DRAWABLE (first), 0, 0, 16, 16);
  g_assert_true (gimp_clone_layer_get_source (first) == GIMP_LAYER (second));
  gimp_viewable_preview_freeze (GIMP_VIEWABLE (first));
  gimp_viewable_preview_thaw (GIMP_VIEWABLE (first));
  g_assert_false (gimp_viewable_preview_is_frozen (GIMP_VIEWABLE (first)));
  g_assert_false (gimp_viewable_preview_is_frozen (GIMP_VIEWABLE (second)));
  gimp_clone_layer_set_source (first, GIMP_LAYER (first));
  gimp_viewable_preview_freeze (GIMP_VIEWABLE (first));
  gimp_viewable_preview_thaw (GIMP_VIEWABLE (first));
  g_assert_false (gimp_viewable_preview_is_frozen (GIMP_VIEWABLE (first)));
  gimp_drawable_update (GIMP_DRAWABLE (first), 0, 0, 16, 16);
  group = gimp_group_layer_new (image);
  gimp_image_add_layer (image, group, NULL, 0, FALSE);
  inside = gimp_clone_layer_new (image, NULL, 16, 16, "inside", 1.0, GIMP_LAYER_MODE_NORMAL_LEGACY);
  gimp_image_add_layer (image, inside, group, 0, FALSE);
  gimp_clone_layer_set_source (GIMP_CLONE_LAYER (inside), group);
  gimp_drawable_update (GIMP_DRAWABLE (group), 0, 0, 16, 16);
  g_object_unref (image);
}
static void duplicate_and_group_update (void)
{
  GimpLayer *source;
  GimpCloneLayer *clone;
  GimpItem *copy;
  gchar *name;

  GimpImage *image = new_image ();
  GimpLayer *group = gimp_group_layer_new (image);
  gimp_image_add_layer (image, group, NULL, 0, FALSE);
  source = new_layer (image, "inside", group, 0);
  fill (source, 204, 51, 102, 128);
  clone = new_clone (image, group);
  pixel (GIMP_LAYER (clone), 204, 51, 102, 128);
  fill (source, 255, 0, 0, 255);
  gimp_pickable_flush (GIMP_PICKABLE (group));
  pixel (GIMP_LAYER (clone), 255, 0, 0, 255);
  copy = gimp_item_duplicate (GIMP_ITEM (clone), GIMP_TYPE_CLONE_LAYER);
  g_object_ref_sink (copy);
  g_assert_true (gimp_clone_layer_get_source (GIMP_CLONE_LAYER (copy)) == group);
  g_object_unref (copy);
  gimp_clone_layer_set_source (clone, NULL);
  gimp_clone_layer_set_source_by_name (clone, "unresolved");
  copy = gimp_item_duplicate (GIMP_ITEM (clone), GIMP_TYPE_CLONE_LAYER);
  g_object_ref_sink (copy);
  name = gimp_clone_layer_dup_source_name (GIMP_CLONE_LAYER (copy));
  g_assert_cmpstr (name, ==, "unresolved"); g_free (name);
  g_object_unref (copy); g_object_unref (image);
}
static void inert_construction (void)
{
  GimpImage *image = new_image ();
  GimpCloneLayer *clone = g_object_new (test_broken_clone_get_type (), "image", image, NULL);
  gboolean failed = FALSE;
  GError *error = NULL;
  g_object_ref_sink (clone);
  g_object_get (clone, "binding-failed", &failed, NULL);
  g_assert_true (failed);
  g_assert_false (gimp_clone_layer_set_source_full (clone, NULL, &error));
  g_assert_error (error, GIMP_PAINTER_ERROR, GIMP_PAINTER_ERROR_CLOSED);
  g_clear_error (&error);
  g_object_unref (clone);
  g_object_unref (image);
}
static void native_type_lifecycle (void)
{
  GTypeQuery query;
  GimpImage *image = new_image ();
  GimpLayer *layer = gimp_clone_layer_new (image, NULL, 7, 9, "native clone", 0.75,
                                           GIMP_LAYER_MODE_NORMAL);
  GimpLayer *weak_layer = layer;
  gboolean failed = TRUE;
  gint item_id;
  g_type_query (GIMP_TYPE_CLONE_LAYER, &query);
  g_assert_cmpuint (g_type_parent (GIMP_TYPE_CLONE_LAYER), ==, GIMP_TYPE_LAYER);
  g_assert_cmpuint (query.instance_size, ==, sizeof (GimpCloneLayer));
  g_assert_cmpuint (query.class_size, ==, sizeof (GimpCloneLayerClass));
  g_assert_true (g_type_is_a (GIMP_TYPE_CLONE_LAYER, GIMP_TYPE_PICKABLE));
  g_assert_nonnull (layer);
  g_object_ref_sink (layer);
  g_object_get (layer, "binding-failed", &failed, NULL);
  g_assert_false (failed);
  g_assert_cmpuint (G_OBJECT_TYPE (layer), ==, GIMP_TYPE_CLONE_LAYER);
  g_assert_cmpstr (gimp_object_get_name (layer), ==, "native clone");
  g_assert_cmpint (gimp_item_get_width (GIMP_ITEM (layer)), ==, 7);
  g_assert_cmpint (gimp_item_get_height (GIMP_ITEM (layer)), ==, 9);
  g_assert_cmpfloat (gimp_layer_get_opacity (layer), ==, 0.75);
  g_assert_nonnull (gimp_drawable_get_buffer (GIMP_DRAWABLE (layer)));
  g_assert_cmpint (gimp_clone_layer_get_source_state (GIMP_CLONE_LAYER (layer)), ==,
                    GIMP_CLONE_SOURCE_NONE);
  item_id = gimp_item_get_id (GIMP_ITEM (layer));
  g_object_add_weak_pointer (G_OBJECT (layer), (gpointer *) &weak_layer);
  g_object_unref (layer);
  g_assert_null (weak_layer);
  g_assert_null (gimp_item_get_by_id (gimp, item_id));

  /* A separately defined native C descendant observes the real finalize chain,
   * rather than confusing weak notification during dispose with finalization. */
  for (gint explicit_dispose = 0; explicit_dispose < 2; explicit_dispose++)
    {
      GError *error = NULL;
      lifecycle_finalizes = 0;
      layer = g_object_new (test_lifecycle_clone_get_type (), "image", image, NULL);
      g_object_ref_sink (layer);
      item_id = gimp_item_get_id (GIMP_ITEM (layer));
      g_object_get (layer, "binding-failed", &failed, NULL);
      g_assert_false (failed);
      g_assert_true (gimp_clone_layer_set_source_full (GIMP_CLONE_LAYER (layer), NULL, &error));
      g_assert_no_error (error);
      if (explicit_dispose)
        {
          g_object_run_dispose (G_OBJECT (layer));
          g_object_run_dispose (G_OBJECT (layer));
          g_assert_cmpuint (lifecycle_finalizes, ==, 0);
          g_assert_false (gimp_clone_layer_set_source_full (GIMP_CLONE_LAYER (layer), NULL, &error));
          g_assert_error (error, GIMP_PAINTER_ERROR, GIMP_PAINTER_ERROR_CLOSED);
          g_clear_error (&error);
        }
      g_object_unref (layer);
      g_assert_cmpuint (lifecycle_finalizes, ==, 1);
      g_assert_null (gimp_item_get_by_id (gimp, item_id));
    }
  g_object_unref (image);
}
static void replace_on_update (GimpDrawable *drawable, gint x, gint y, gint w, gint h, gpointer data)
{
  GimpLayer **replacement = data;
  if (*replacement)
    {
      GimpLayer *source = *replacement;
      *replacement = NULL;
      gimp_clone_layer_set_source (GIMP_CLONE_LAYER (drawable), source);
    }
}
static void reentrant_source_replace (void)
{
  GimpImage *image = new_image ();
  GimpLayer *source = new_layer (image, "source", NULL, 0);
  GimpLayer *other = new_layer (image, "other", NULL, 1);
  GimpLayer *replacement = other;
  GimpCloneLayer *clone = new_clone (image, source);
  GimpContext *context = gimp_context_new (gimp, "Test", NULL);
  gulong id = g_signal_connect (clone, "update", G_CALLBACK (replace_on_update), &replacement);
  fill (other, 255, 0, 0, 255);
  gimp_item_resize (GIMP_ITEM (source), context, GIMP_FILL_TRANSPARENT, 20, 18, 0, 0);
  while (g_main_context_iteration (NULL, FALSE));
  g_signal_handler_disconnect (clone, id);
  g_assert_true (gimp_clone_layer_get_source (clone) == other);
  pixel (GIMP_LAYER (clone), 255, 0, 0, 255);
  g_assert_cmpint (gimp_item_get_width (GIMP_ITEM (clone)), ==, 16);
  g_object_unref (context); g_object_unref (image);
}
typedef struct
{
  GimpLayer *replacement;
  gboolean close;
  gboolean fired;
} ThawAction;
static void on_clone_thaw (GObject *object, GParamSpec *spec, gpointer data)
{
  ThawAction *action = data;
  if (action->fired || gimp_viewable_preview_is_frozen (GIMP_VIEWABLE (object))) return;
  action->fired = TRUE;
  if (action->close) gimp_painter_binding_close (object, NULL);
  else gimp_clone_layer_set_source (GIMP_CLONE_LAYER (object), action->replacement);
}
static void thaw_reentry_close_and_replace (void)
{
  for (gint close = 0; close < 2; ++close)
    {
      GimpImage *image = new_image ();
      GimpLayer *old_source = new_layer (image, "old", NULL, 0);
      GimpLayer *outer = new_layer (image, "outer", NULL, 1);
      GimpLayer *inner = new_layer (image, "inner", NULL, 2);
      GimpCloneLayer *clone = new_clone (image, old_source);
      ThawAction action = { inner, close, FALSE };
      gulong id;
      GError *error = NULL;
      fill (old_source, 0, 0, 255, 255);
      fill (outer, 255, 0, 0, 255);
      fill (inner, 0, 255, 0, 255);
      gimp_viewable_preview_freeze (GIMP_VIEWABLE (old_source));
      id = g_signal_connect (clone, "notify::frozen", G_CALLBACK (on_clone_thaw), &action);
      gimp_clone_layer_set_source (clone, outer);
      g_assert_true (action.fired);
      g_signal_handler_disconnect (clone, id);
      if (close)
        {
          g_assert_null (gimp_clone_layer_get_source (clone));
          pixel (GIMP_LAYER (clone), 0, 0, 255, 255);
          g_assert_false (gimp_clone_layer_set_source_full (clone, inner, &error));
          g_assert_error (error, GIMP_PAINTER_ERROR, GIMP_PAINTER_ERROR_CLOSED);
          g_clear_error (&error);
        }
      else
        {
          g_assert_true (gimp_clone_layer_get_source (clone) == inner);
          pixel (GIMP_LAYER (clone), 0, 255, 0, 255);
          fill (outer, 255, 255, 0, 255);
          pixel (GIMP_LAYER (clone), 0, 255, 0, 255);
        }
      gimp_viewable_preview_thaw (GIMP_VIEWABLE (old_source));
      g_object_unref (image);
    }
}
static void cpp_header_layout (void)
{
  GimpImage *image = new_image ();
  GimpCloneLayer *clone = new_clone (image, NULL);
  gimp_test_clone_cpp_layout (sizeof (GimpDrawable), offsetof (GimpDrawable, private), clone);
  g_object_unref (image);
}
/* Genuine retained GObjects: no test-side image retention or synthetic weak
 * notification. The image reaches finalization while these handles stay live. */
static void retained_items_image_destroy (void)
{
  GimpImage *image = new_image ();
  GimpLayer *source = new_layer (image, "retained source", NULL, 0);
  GimpCloneLayer *clone = new_clone (image, source);
  GimpCloneLayerReference *snapshot = gimp_clone_layer_dup_reference (clone, NULL);
  gint source_id = gimp_item_get_id (GIMP_ITEM (source));
  gint clone_id = gimp_item_get_id (GIMP_ITEM (clone));
  g_object_ref (source);
  gimp_test_clone_retained_image (image, clone); /* consumes image */
  g_assert_null (gimp_item_get_image (GIMP_ITEM (source)));
  g_assert_false (gimp_item_is_attached (GIMP_ITEM (source)));
  g_assert_true (gimp_item_get_by_id (gimp, source_id) == GIMP_ITEM (source));
  g_assert_null (gimp_item_get_by_id (gimp, clone_id));
  gimp_clone_layer_reference_free (snapshot);
  g_object_unref (source);
  g_assert_null (gimp_item_get_by_id (gimp, source_id));
}
static void retained_item_same_gimp_relocation (void)
{
  GimpImage *first = new_image ();
  GimpImage *second = new_image ();
  GimpLayer *layer = gimp_layer_new (first, 16, 16, babl_format ("R'G'B'A u8"),
                                     "unattached retained", 1.0, GIMP_LAYER_MODE_NORMAL_LEGACY);
  gint id = gimp_item_get_id (GIMP_ITEM (layer));
  g_object_ref_sink (layer);
  gimp_item_set_image (GIMP_ITEM (layer), first);
  g_assert_cmpint (gimp_item_get_id (GIMP_ITEM (layer)), ==, id);
  gimp_item_set_image (GIMP_ITEM (layer), second);
  g_assert_cmpint (gimp_item_get_id (GIMP_ITEM (layer)), ==, id);
  g_object_unref (first);
  g_assert_true (gimp_item_get_image (GIMP_ITEM (layer)) == second);
  g_assert_true (gimp_item_get_by_id (gimp, id) == GIMP_ITEM (layer));
  g_object_unref (second);
  g_assert_null (gimp_item_get_image (GIMP_ITEM (layer)));
  g_object_unref (layer);
  g_assert_null (gimp_item_get_by_id (gimp, id));
}
static void replaced_item_weak_identity_transfer (void)
{
  for (gint different = 0; different < 2; different++)
    {
      GimpImage *first = new_image ();
      GimpImage *second = different ? new_image () : g_object_ref (first);
      GimpItem *old = gimp_item_new (GIMP_TYPE_LAYER, first, "old", 3, 4, 16, 16);
      GimpItem *replacement = gimp_item_new (GIMP_TYPE_LAYER, second, "new", 0, 0, 16, 16);
      gint old_id = gimp_item_get_id (old);
      gint spare_id = gimp_item_get_id (replacement);
      g_object_ref_sink (old); g_object_ref_sink (replacement);
      gimp_item_replace_item (replacement, old);
      g_assert_true (gimp_item_get_image (replacement) == first);
      g_assert_null (gimp_item_get_image (old));
      g_assert_cmpint (gimp_item_get_id (old), ==, 0);
      g_assert_cmpint (gimp_item_get_id (replacement), ==, old_id);
      g_assert_null (gimp_item_get_by_id (gimp, spare_id));
      g_object_unref (old);
      g_assert_true (gimp_item_get_by_id (gimp, old_id) == replacement);
      g_object_unref (second);
      g_assert_true (gimp_item_get_image (replacement) == first);
      g_object_unref (first);
      g_assert_null (gimp_item_get_image (replacement));
      g_object_unref (replacement);
      g_assert_null (gimp_item_get_by_id (gimp, old_id));
    }
}
static void retained_item_gimp_lifetime (void)
{
  if (g_test_subprocess ())
    {
      Gimp *weak_gimp = gimp;
      GimpImage *image = new_image ();
      GimpLayer *source = new_layer (image, "survive Gimp", NULL, 0);
      GimpCloneLayer *clone = new_clone (image, source);
      g_object_ref (source); g_object_ref (clone);
      g_object_unref (image);
      g_assert_null (gimp_item_get_image (GIMP_ITEM (source)));
      g_assert_null (gimp_item_get_image (GIMP_ITEM (clone)));
      gimp_test_utils_set_gimp3_directory ("GIMP_TESTING_ABS_TOP_BUILDDIR", "app/tests/gimpdir-output");
      gimp_exit (gimp, TRUE);
      while (g_main_context_pending (NULL)) g_main_context_iteration (NULL, FALSE);
      g_object_add_weak_pointer (G_OBJECT (gimp), (gpointer *) &weak_gimp);
      g_object_unref (gimp);
      g_assert_null (weak_gimp); /* no strong item-to-Gimp cycle */
      g_assert_null (gimp_clone_layer_get_source (clone));
      g_object_unref (clone); g_object_unref (source);
      gimp = NULL;
      return;
    }
  g_test_trap_subprocess (NULL, 30 * G_USEC_PER_SEC, (GTestSubprocessFlags) 0);
  g_test_trap_assert_passed ();
}
int main (int argc, char **argv)
{
  int result;
  g_test_init (&argc, &argv, NULL);
  gimp_test_utils_set_gimp3_directory ("GIMP_TESTING_ABS_TOP_SRCDIR", "app/tests/gimpdir");
  gimp = gimp_init_for_testing ();
#define ADD(name) g_test_add_func ("/gimp-clone-layer/" #name, name)
  ADD (retained_items_image_destroy); ADD (retained_item_same_gimp_relocation);
  ADD (replaced_item_weak_identity_transfer); ADD (retained_item_gimp_lifetime);
  ADD (serialization_reference_snapshot_and_restore); ADD (undo_source_image_relocation);
  ADD (clone_owner_image_relocation);
  ADD (source_reference_undo_dependent_clone); ADD (source_reference_undo_close_reentry);
  ADD (source_reference_undo_live); ADD (source_reference_undo_all_states);
  ADD (source_reference_undo_preserves_pending_live); ADD (source_reference_undo_renamed_then_expired);
  ADD (source_reference_undo_expired_cross_image); ADD (cross_image_copy_and_move);
  ADD (inherited_precision_conversion_undo);
  ADD (source_resize_undo_legacy_fixture); ADD (group_duplicate_complete_hierarchy);
  ADD (double_precision_and_trc); ADD (thaw_reentry_close_and_replace);
  ADD (dissolve_legacy_fixture); ADD (gray_and_high_precision);
  ADD (frozen_source_disposal); ADD (group_duplicate_internal_reference);
  ADD (inert_construction); ADD (reentrant_source_replace); ADD (cpp_header_layout); ADD (projected_pixels); ADD (partial_update_and_graph); ADD (deferred_name);
  ADD (native_type_lifecycle);
  ADD (recursive_first_match); ADD (offset_size_and_noop_transforms);
  ADD (source_delete_undo); ADD (source_lifetime_and_detach); ADD (freeze_and_close); ADD (cycles);
  ADD (duplicate_and_group_update);
  result = g_test_run ();
  gimp_test_utils_set_gimp3_directory ("GIMP_TESTING_ABS_TOP_BUILDDIR", "app/tests/gimpdir-output");
  if (gimp) gimp_exit (gimp, TRUE);
  return result;
}
