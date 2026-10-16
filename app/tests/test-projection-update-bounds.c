/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Real top-level drawable/stack/projection notifications.  No filter worker,
 * captured fixture, handler suppression, or private projection-state access.
 */
#include "config.h"

#include <gegl.h>
#include <gtk/gtk.h>

#include "core/core-types.h"
#include "widgets/widgets-types.h"
#include "core/gimp.h"
#include "core/gimpdrawable.h"
#include "core/gimpdrawablestack.h"
#include "core/gimpimage.h"
#include "core/gimpitem.h"
#include "core/gimplayer.h"
#include "core/gimplayer-new.h"
#include "core/gimppickable.h"
#include "core/gimpprojectable.h"
#include "core/gimpprojection.h"
#include "core/gimpviewable.h"

#include "tests.h"
#include "gimp-app-test-utils.h"

static Gimp *gimp;

/* A small provider isolates the projection's image/local coordinate boundary.
 * It is additional arithmetic coverage, not a claim about group-layer or
 * huge-buffer allocation behavior.  The real image/layer lifecycle is below.
 */
typedef struct
{
  GimpViewable  parent_instance;
  GeglNode     *graph;
  GeglRectangle bounds;
  gint          offset_x;
  gint          offset_y;
} UpdateProjectable;

typedef GimpViewableClass UpdateProjectableClass;

static GType update_projectable_get_type (void);
static void  update_projectable_iface_init (GimpProjectableInterface *iface);

G_DEFINE_TYPE_WITH_CODE (UpdateProjectable, update_projectable, GIMP_TYPE_VIEWABLE,
                         G_IMPLEMENT_INTERFACE (GIMP_TYPE_PROJECTABLE,
                                                update_projectable_iface_init))

static void
update_projectable_finalize (GObject *object)
{
  g_clear_object (&((UpdateProjectable *) object)->graph);
  G_OBJECT_CLASS (update_projectable_parent_class)->finalize (object);
}

static void
update_projectable_class_init (UpdateProjectableClass *klass)
{
  G_OBJECT_CLASS (klass)->finalize = update_projectable_finalize;
}

static void
update_projectable_init (UpdateProjectable *projectable)
{
  projectable->graph = gegl_node_new ();
  projectable->bounds = *GEGL_RECTANGLE (0, 0, 17, 13);
}

static const Babl *
update_projectable_format (GimpProjectable *projectable)
{
  return babl_format ("R'G'B'A u8");
}

static void
update_projectable_offset (GimpProjectable *projectable,
                            gint            *x,
                            gint            *y)
{
  *x = ((UpdateProjectable *) projectable)->offset_x;
  *y = ((UpdateProjectable *) projectable)->offset_y;
}

static GeglRectangle
update_projectable_bounds (GimpProjectable *projectable)
{
  return ((UpdateProjectable *) projectable)->bounds;
}

static GeglNode *
update_projectable_graph (GimpProjectable *projectable)
{
  return ((UpdateProjectable *) projectable)->graph;
}

static void
update_projectable_iface_init (GimpProjectableInterface *iface)
{
  iface->get_format       = update_projectable_format;
  iface->get_offset       = update_projectable_offset;
  iface->get_bounding_box = update_projectable_bounds;
  iface->get_graph        = update_projectable_graph;
}

static void
record_rectangle (GArray *rectangles,
                   gint    x,
                   gint    y,
                   gint    width,
                   gint    height)
{
  GeglRectangle rect = { x, y, width, height };

  g_assert_cmpint (width, >=, 0);
  g_assert_cmpint (height, >=, 0);
  g_assert_cmpint ((gint64) x + width, <=, G_MAXINT);
  g_assert_cmpint ((gint64) y + height, <=, G_MAXINT);
  g_array_append_val (rectangles, rect);
}

static void
stack_update (GimpDrawableStack *stack,
               gint               x,
               gint               y,
               gint               width,
               gint               height,
               GArray            *rectangles)
{
  record_rectangle (rectangles, x, y, width, height);
}

static void
projection_update (GimpProjection *projection,
                    gboolean        now,
                    gint            x,
                    gint            y,
                    gint            width,
                    gint            height,
                    GArray         *rectangles)
{
  record_rectangle (rectangles, x, y, width, height);
}

static void
assert_contains (GArray              *rectangles,
                  const GeglRectangle *expected)
{
  for (guint i = 0; i < rectangles->len; i++)
    if (gegl_rectangle_equal (&g_array_index (rectangles, GeglRectangle, i),
                               expected))
      return;

  g_error ("Missing update (%d, %d, %d, %d) in %u notifications",
           expected->x, expected->y, expected->width, expected->height,
           rectangles->len);
}

static GimpLayer *
add_layer (GimpImage   *image,
            gint         width,
            gint         height,
            const gchar *name)
{
  GimpLayer *layer = gimp_layer_new (image, width, height,
                                     babl_format ("R'G'B'A u8"), name, 1.0,
                                     GIMP_LAYER_MODE_NORMAL);

  g_assert_nonnull (layer);
  g_assert_true (gimp_image_add_layer (image, layer, NULL, 0, FALSE));
  return layer;
}

static void
projectable_grid_updates (void)
{
  static const struct
  {
    GeglRectangle input;
    gboolean nonempty;
    GeglRectangle expected;
  } cases[] = {
    { { 3, 35, 2, 2 }, TRUE, { 0, 32, 32, 32 } },
    { { -2, -2, 4, 4 }, TRUE, { 0, 0, 32, 32 } },
    /* Preserve padding from just outside a non-grid-aligned image edge. */
    { { 83, 71, 1, 1 }, TRUE, { 64, 64, 19, 7 } },
    { { 96, 71, 1, 1 }, FALSE, { 0, 0, 0, 0 } },
    { { -17, 0, G_MAXINT, 1 }, TRUE, { 0, 0, 83, 32 } },
    { { G_MAXINT - 2, 0, 2049, 65 }, FALSE, { 0, 0, 0, 0 } },
    { { 0, G_MAXINT - 2, 2049, 65 }, FALSE, { 0, 0, 0, 0 } },
    { { G_MININT + 2, G_MININT + 2, 2049, 65 }, FALSE, { 0, 0, 0, 0 } },
    { { 1, 1, 0, 1 }, FALSE, { 0, 0, 0, 0 } },
    { { 1, 1, 1, 0 }, FALSE, { 0, 0, 0, 0 } },
    { { 1, 1, -1, 1 }, FALSE, { 0, 0, 0, 0 } },
    { { 1, 1, 1, G_MININT }, FALSE, { 0, 0, 0, 0 } }
  };
  GimpImage      *image = gimp_image_new (gimp, 83, 71, GIMP_RGB,
                                          GIMP_PRECISION_U8_NON_LINEAR);
  GimpProjection *projection = gimp_image_get_projection (image);
  GArray         *updates = g_array_new (FALSE, FALSE, sizeof (GeglRectangle));

  /* Drain the initial whole-image invalidation before observing individual
   * calls.  FALSE invalidates tiles without evaluating the entire graph.
   */
  gimp_pickable_flush (GIMP_PICKABLE (projection));
  g_signal_connect (projection, "update", G_CALLBACK (projection_update), updates);

  for (guint i = 0; i < G_N_ELEMENTS (cases); i++)
    {
      const GeglRectangle *rect = &cases[i].input;

      g_array_set_size (updates, 0);
      gimp_projectable_invalidate (GIMP_PROJECTABLE (image),
                                   rect->x, rect->y, rect->width, rect->height);
      gimp_projection_flush_now (projection, FALSE);
      g_assert_cmpuint (updates->len, ==, cases[i].nonempty ? 1 : 0);
      if (cases[i].nonempty)
        assert_contains (updates, &cases[i].expected);
    }

  g_object_unref (image);
  g_array_unref (updates);
}

static void
extreme_layer_lifecycle (void)
{
  static const struct
  {
    gint offset_x, offset_y;
    GeglRectangle expected;
  } cases[] = {
    { G_MAXINT - 2, 0, { G_MAXINT - 2, 0, 2, 65 } },
    { 0, G_MAXINT - 2, { 0, G_MAXINT - 2, 2049, 2 } },
    { G_MAXINT - 2, G_MAXINT - 2, { G_MAXINT - 2, G_MAXINT - 2, 2, 2 } },
    { G_MININT + 2, G_MININT + 2, { G_MININT + 2, G_MININT + 2, 2049, 65 } },
    { G_MAXINT, G_MAXINT, { G_MAXINT, G_MAXINT, 0, 0 } },
    { G_MININT, G_MININT, { G_MININT, G_MININT, 2049, 65 } }
  };

  for (guint i = 0; i < G_N_ELEMENTS (cases); i++)
    {
      GimpImage      *image = gimp_image_new (gimp, 17, 13, GIMP_RGB,
                                              GIMP_PRECISION_U8_NON_LINEAR);
      GimpLayer      *layer = add_layer (image, 2049, 65, "extreme update");
      GimpLayer      *upper = add_layer (image, 17, 13, "clip backdrop");
      GimpProjection *projection = gimp_image_get_projection (image);
      GArray         *stack_updates = g_array_new (FALSE, FALSE, sizeof (GeglRectangle));
      GArray         *projection_updates = g_array_new (FALSE, FALSE, sizeof (GeglRectangle));
      GeglRectangle   local_expected;
      gint            local_x = cases[i].offset_x < 0 ? -16 : 16;
      gint            local_y = cases[i].offset_y < 0 ? -16 : 16;

      gimp_pickable_flush (GIMP_PICKABLE (projection));
      g_signal_connect (gimp_image_get_layers (image), "update",
                         G_CALLBACK (stack_update), stack_updates);
      g_signal_connect (projection, "update",
                         G_CALLBACK (projection_update), projection_updates);

      gimp_item_set_offset (GIMP_ITEM (layer), cases[i].offset_x, cases[i].offset_y);
      gimp_projection_flush_now (projection, FALSE);
      g_array_set_size (stack_updates, 0);
      g_array_set_size (projection_updates, 0);

      gimp_drawable_update (GIMP_DRAWABLE (layer), 0, 0, 2049, 65);
      assert_contains (stack_updates, &cases[i].expected);
      gimp_projection_flush_now (projection, FALSE);
      g_assert_cmpuint (projection_updates->len, ==, 0);

      /* A nonzero local origin crosses either signed-int edge.  The empty
       * upper-edge update must still deliver its normal stack notification.
       */
      local_expected.x = cases[i].offset_x == 0 ? 16 :
                         cases[i].offset_x > 0 ? G_MAXINT : G_MININT;
      local_expected.y = cases[i].offset_y == 0 ? 16 :
                         cases[i].offset_y > 0 ? G_MAXINT : G_MININT;
      local_expected.width = cases[i].offset_x == 0 ? 32 :
                             cases[i].offset_x > 0 ? 0 :
                             cases[i].offset_x == G_MININT ? 16 : 18;
      local_expected.height = cases[i].offset_y == 0 ? 32 :
                              cases[i].offset_y > 0 ? 0 :
                              cases[i].offset_y == G_MININT ? 16 : 18;
      g_array_set_size (stack_updates, 0);
      gimp_drawable_update (GIMP_DRAWABLE (layer), local_x, local_y, 32, 32);
      assert_contains (stack_updates, &local_expected);

      /* Active-change uses the full bounding box, including deactivation. */
      g_array_set_size (stack_updates, 0);
      gimp_item_set_visible (GIMP_ITEM (layer), FALSE, FALSE);
      assert_contains (stack_updates, &cases[i].expected);
      g_array_set_size (stack_updates, 0);
      gimp_item_set_visible (GIMP_ITEM (layer), TRUE, FALSE);
      assert_contains (stack_updates, &cases[i].expected);

      /* A real excluding-backdrop layer causes update_range to visit the
       * extreme lower layer.  Removing that upper layer visits it again.
       */
      g_array_set_size (stack_updates, 0);
      gimp_layer_set_composite_mode (upper, GIMP_LAYER_COMPOSITE_CLIP_TO_LAYER, FALSE);
      g_assert_true (gimp_layer_get_excludes_backdrop (upper));
      assert_contains (stack_updates, &cases[i].expected);
      g_array_set_size (stack_updates, 0);
      gimp_image_remove_layer (image, upper, FALSE, NULL);
      assert_contains (stack_updates, &cases[i].expected);

      if (i % 2 == 0)
        {
          g_array_set_size (stack_updates, 0);
          gimp_image_remove_layer (image, layer, FALSE, NULL);
          assert_contains (stack_updates, &cases[i].expected);
        }

      /* Alternate explicit layer removal and image-owned layer destruction.
       * Observers remain connected and alive throughout both cleanup paths.
       */
      g_object_unref (image);
      g_array_unref (projection_updates);
      g_array_unref (stack_updates);
    }
}

static void
projection_offset_boundaries (void)
{
  static const struct
  {
    gint offset_x, offset_y;
    GeglRectangle bounds;
    GeglRectangle expected;
  } cases[] = {
    { G_MAXINT - 2, 0, { 0, 0, 17, 13 }, { G_MAXINT - 2, 0, 2, 13 } },
    { 0, G_MAXINT - 2, { 0, 0, 17, 13 }, { 0, G_MAXINT - 2, 17, 2 } },
    { G_MAXINT, G_MAXINT, { 0, 0, 17, 13 }, { G_MAXINT, G_MAXINT, 0, 0 } },
    { G_MININT + 2, G_MININT + 2, { -16, -16, 17, 13 },
      { G_MININT, G_MININT, 3, 0 } },
    { G_MININT, G_MININT, { 0, 0, 17, 13 }, { G_MININT, G_MININT, 17, 13 } }
  };

  for (guint i = 0; i < G_N_ELEMENTS (cases); i++)
    {
      UpdateProjectable *provider = g_object_new (update_projectable_get_type (), NULL);
      GimpProjection    *projection;
      GArray            *updates = g_array_new (FALSE, FALSE, sizeof (GeglRectangle));

      provider->offset_x = cases[i].offset_x;
      provider->offset_y = cases[i].offset_y;
      provider->bounds   = cases[i].bounds;
      projection = gimp_projection_new (GIMP_PROJECTABLE (provider));
      g_signal_connect (projection, "update", G_CALLBACK (projection_update), updates);
      gimp_pickable_flush (GIMP_PICKABLE (projection));
      g_assert_cmpuint (updates->len, ==, 1);
      assert_contains (updates, &cases[i].expected);

      /* Opposite extreme image coordinates must be rejected after widening
       * subtraction, before narrowing or putting any area into cairo.
       */
      g_array_set_size (updates, 0);
      gimp_projectable_invalidate (GIMP_PROJECTABLE (provider),
                                   cases[i].offset_x > 0 ? G_MININT : G_MAXINT,
                                   cases[i].offset_y > 0 ? G_MININT : G_MAXINT,
                                   17, 13);
      gimp_projection_flush_now (projection, FALSE);
      g_assert_cmpuint (updates->len, ==, 0);

      g_object_unref (projection);
      g_object_unref (provider);
      g_array_unref (updates);
    }
}

int
main (int argc, char **argv)
{
  gint result;

  g_test_init (&argc, &argv, NULL);
  gimp_test_utils_set_gimp3_directory ("GIMP_TESTING_ABS_TOP_SRCDIR", "app/tests/gimpdir");
  gimp = gimp_init_for_testing ();
  g_test_add_func ("/projection-update-bounds/projectable-grid-updates", projectable_grid_updates);
  g_test_add_func ("/projection-update-bounds/extreme-layer-lifecycle", extreme_layer_lifecycle);
  g_test_add_func ("/projection-update-bounds/projection-offset-boundaries", projection_offset_boundaries);
  result = g_test_run ();
  gimp_test_utils_set_gimp3_directory ("GIMP_TESTING_ABS_TOP_BUILDDIR", "app/tests/gimpdir-output");
  gimp_exit (gimp, TRUE);
  return result;
}
