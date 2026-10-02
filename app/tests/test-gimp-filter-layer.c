/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gegl.h>
#include <gtk/gtk.h>
#include <string.h>
#include <stddef.h>
#include "libgimpbase/gimpbase.h"
#include "libgimpmath/gimpmath.h"
#include "libgimpcolor/gimpcolor.h"
#include "core/core-types.h"
#include "widgets/widgets-types.h"
#include "core/gimp.h"
#include "core/gimpfilterlayer.h"
#include "core/gimpclonelayer.h"
#include "core/gimpimage.h"
#include "core/gimpimage-color-profile.h"
#include "core/gimpimage-undo.h"
#include "core/gimpgrouplayer.h"
#include "core/gimplayer-new.h"
#include "core/gimplayermask.h"
#include "core/gimppickable.h"
#include "core/gimpundostack.h"
#include "painter/gimp-painter-binding.h"
#include "tests.h"
#include "gimp-app-test-utils.h"
void gimp_test_filter_cpp_layout (gsize size, gsize offset, GimpFilterLayer *layer);
static Gimp *gimp;
static gint broken_filter_finalized;
typedef struct { GimpFilterLayer parent; } TestBrokenFilter;
typedef struct { GimpFilterLayerClass parent; } TestBrokenFilterClass;
GType test_broken_filter_get_type (void);
G_DEFINE_TYPE (TestBrokenFilter,test_broken_filter,GIMP_TYPE_FILTER_LAYER)
static void broken_filter_weak (gpointer data, GObject *object) { ++broken_filter_finalized; }
static void test_broken_filter_class_init (TestBrokenFilterClass *klass) {}
static void test_broken_filter_init (TestBrokenFilter *filter)
{
  g_object_weak_ref (G_OBJECT (filter),broken_filter_weak,NULL);
  gimp_painter_binding_close (G_OBJECT (filter),NULL);
}
typedef struct { GimpLayer parent; GimpCloneLayer *pending; gboolean resolved; } TestResolvingLayer;
typedef struct { GimpLayerClass parent; } TestResolvingLayerClass;
GType test_resolving_layer_get_type (void);
G_DEFINE_TYPE (TestResolvingLayer,test_resolving_layer,GIMP_TYPE_LAYER)
static GeglNode *test_resolving_layer_get_node (GimpFilter *filter)
{
  TestResolvingLayer *layer = (TestResolvingLayer *) filter;
  if (layer->pending && !layer->resolved)
    { layer->resolved = TRUE; gimp_clone_layer_get_source (layer->pending); }
  return GIMP_FILTER_CLASS (test_resolving_layer_parent_class)->get_node (filter);
}
static void test_resolving_layer_class_init (TestResolvingLayerClass *klass)
{ GIMP_FILTER_CLASS (klass)->get_node = test_resolving_layer_get_node; }
static void test_resolving_layer_init (TestResolvingLayer *layer) {}
static GimpImage *image_new (gint w, gint h)
{ return gimp_image_new (gimp, w, h, GIMP_RGB, GIMP_PRECISION_U8_NON_LINEAR); }
static GimpLayer *source_new (GimpImage *image, GimpLayer *parent, gint w, gint h)
{
  GimpLayer *layer = gimp_layer_new (image, w, h, babl_format ("R'G'B'A u8"), "source", 1, GIMP_LAYER_MODE_NORMAL_LEGACY);
  gimp_image_add_layer (image, layer, parent, 0, FALSE); return layer;
}
static void fill (GimpLayer *layer, guchar r, guchar g, guchar b, guchar a)
{
  GeglColor *color = gegl_color_new (NULL);
  guchar pixel[] = {r,g,b,a};
  gegl_color_set_pixel (color, babl_format ("R'G'B'A u8"), pixel);
  gegl_buffer_set_color (gimp_drawable_get_buffer (GIMP_DRAWABLE (layer)), NULL, color);
  g_object_unref (color);
  gimp_drawable_update (GIMP_DRAWABLE (layer), 0, 0, gimp_item_get_width (GIMP_ITEM (layer)), gimp_item_get_height (GIMP_ITEM (layer)));
}
static GimpValueArray *edge_args (void)
{
  return gimp_value_array_new_from_types (NULL,
    G_TYPE_INT, 1, G_TYPE_INT, 123, G_TYPE_INT, 456, G_TYPE_DOUBLE, 2.0,
    G_TYPE_INT, 2, G_TYPE_INT, 0, G_TYPE_NONE);
}
static void define_edge (GimpFilterLayer *filter)
{
  GimpValueArray *args = edge_args ();
  GError *error = NULL;
  g_assert_true (gimp_filter_layer_set_definition (filter, "plug-in-edge", NULL, args, &error));
  g_assert_no_error (error); gimp_value_array_unref (args);
}
static GimpFilterLayer *filter_new (GimpImage *image, GimpLayer *parent, gint w, gint h)
{
  GimpLayer *layer = gimp_filter_layer_new (image, w, h, "edge", 1, GIMP_LAYER_MODE_NORMAL_LEGACY);
  g_assert_true (GIMP_IS_FILTER_LAYER (layer));
  gimp_image_add_layer (image, layer, parent, 0, FALSE);
  define_edge (GIMP_FILTER_LAYER (layer)); return GIMP_FILTER_LAYER (layer);
}
static void spin_ms (gint duration)
{
  gint64 deadline = g_get_monotonic_time () + duration * 1000;
  while (g_get_monotonic_time () < deadline)
    { g_main_context_iteration (NULL, FALSE); g_usleep (100); }
}
static void settle (GimpFilterLayer *layer)
{
  gint64 deadline = g_get_monotonic_time () + 20 * G_TIME_SPAN_SECOND;
  while (g_get_monotonic_time () < deadline)
    {
      GimpFilterLayerState state = gimp_filter_layer_get_state (layer);
      if (state == GIMP_FILTER_LAYER_CLEAN) return;
      if (state == GIMP_FILTER_LAYER_FAILED)
        { gchar *message = gimp_filter_layer_dup_error (layer); g_error ("Filter failed: %s", message); }
      g_main_context_iteration (NULL, FALSE); g_usleep (100);
    }
  g_error ("Filter did not converge");
}
static void pixel (GimpLayer *layer, gint r, gint g, gint b, gint a)
{
  guchar actual[4];
  gegl_buffer_get (gimp_drawable_get_buffer (GIMP_DRAWABLE (layer)), GEGL_RECTANGLE (0,0,1,1),
    1.0, babl_format ("R'G'B'A u8"), actual, GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);
  g_assert_cmpint (actual[0], ==, r); g_assert_cmpint (actual[1], ==, g);
  g_assert_cmpint (actual[2], ==, b); g_assert_cmpint (actual[3], ==, a);
}
static void independent_type_and_edge (void)
{
  GimpImage *image = image_new (16,16);
  GimpLayer *source = source_new (image,NULL,16,16);
  GimpFilterLayer *filter;
  fill (source, 55, 123, 240, 255); filter = filter_new (image,NULL,16,16);
  settle (filter); pixel (GIMP_LAYER (filter),0,0,0,255);
  g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 1);
  g_assert_cmpuint (gimp_filter_layer_get_cache_generation (filter), ==, gimp_filter_layer_get_generation (filter));
  g_assert_cmpfloat (gimp_pickable_get_opacity_at (GIMP_PICKABLE (filter),0,0), ==, 0);
  g_assert_true (gimp_item_is_content_locked (GIMP_ITEM (filter),NULL));
  g_object_unref (image);
}
static void automatic_updates_and_self_exclusion (void)
{
  GimpImage *image = image_new (16,16);
  GimpLayer *source = source_new (image,NULL,16,16);
  GimpFilterLayer *filter = filter_new (image,NULL,16,16);
  GimpLayer *above;
  guint64 count;
  fill (source,255,0,0,255); settle (filter);
  above = source_new (image,NULL,16,16); settle (filter); /* topology edit establishes baseline */
  count = gimp_filter_layer_get_run_count (filter);
  fill (above,0,255,0,255); spin_ms (50);
  g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, count);
  gimp_drawable_update (GIMP_DRAWABLE (filter),0,0,16,16); spin_ms (50);
  g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, count);
  fill (source,0,0,255,255); settle (filter);
  g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, count+1);
  g_object_unref (image);
}
static void count_image_flush (GimpImage *image, gboolean invalidate_preview, gpointer data)
{ ++*(guint *) data; }
static void completion_flushes_image_projection (void)
{
  GimpImage *image = image_new (256,1024);
  GimpLayer *source = source_new (image,NULL,256,1024);
  GimpLayer *clone;
  GimpFilterLayer *filter;
  GeglBuffer *projection;
  guchar value[4];
  guint flushes = 0;
  fill (source,255,255,255,255);
  clone = gimp_clone_layer_new (image,source,256,1024,"Background clone",1,GIMP_LAYER_MODE_PAINTER_NORMAL);
  gimp_image_add_layer (image,clone,NULL,0,FALSE);
  filter = GIMP_FILTER_LAYER (gimp_filter_layer_new (image,256,1024,"Replace edge",1,GIMP_LAYER_MODE_PAINTER_REPLACE));
  gimp_image_add_layer (image,GIMP_LAYER (filter),NULL,0,FALSE); define_edge (filter);
  /* A display has already rendered the initially empty completed cache. */
  gimp_pickable_flush (GIMP_PICKABLE (image));
  projection = gimp_pickable_get_buffer (GIMP_PICKABLE (image));
  gegl_buffer_get (projection,GEGL_RECTANGLE (32,32,1,1),1.0,babl_format ("R'G'B'A u8"),value,
                  GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
  g_assert_cmpuint (value[3], ==, 0);
  g_signal_connect (image,"flush",G_CALLBACK (count_image_flush),&flushes);
  settle (filter); spin_ms (30);
  pixel (GIMP_LAYER (filter),0,0,0,255);
  /* No explicit image/pickable flush here: completion must notify the display. */
  g_assert_cmpuint (flushes, >, 0);
  gegl_buffer_get (projection,GEGL_RECTANGLE (32,32,1,1),1.0,babl_format ("R'G'B'A u8"),value,
                  GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
  g_assert_cmpuint (value[0], ==, 0); g_assert_cmpuint (value[1], ==, 0);
  g_assert_cmpuint (value[2], ==, 0); g_assert_cmpuint (value[3], ==, 255);
  g_object_unref (image);
}
static void saved_definition_is_separate (void)
{
  const guchar original[] = {0,0,0,1,0xfe,0x89,0,0xff,0};
  GimpImage *image = image_new (4,4);
  GimpFilterLayer *filter = filter_new (image,NULL,4,4);
  GBytes *raw = g_bytes_new (original,sizeof original), *copy;
  GimpValueArray *args = edge_args (), *saved;
  gchar *name;
  g_assert_true (gimp_filter_layer_set_definition (filter,"future-unknown-procedure",raw,args,NULL));
  g_value_set_int (gimp_value_array_index (args,4),3);
  gimp_value_array_unref (args); g_bytes_unref (raw);
  spin_ms (30);
  g_assert_cmpint (gimp_filter_layer_get_state (filter), ==, GIMP_FILTER_LAYER_FAILED);
  name = gimp_filter_layer_dup_procedure (filter); g_assert_cmpstr (name, ==, "future-unknown-procedure"); g_free (name);
  copy = gimp_filter_layer_ref_definition (filter);
  g_assert_cmpmem (g_bytes_get_data (copy,NULL),g_bytes_get_size (copy),original,sizeof original);
  g_bytes_unref (copy);
  saved = gimp_filter_layer_dup_args (filter);
  g_assert_cmpint (g_value_get_int (gimp_value_array_index (saved,4)), ==, 2); gimp_value_array_unref (saved);
  spin_ms (30); g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 0);
  g_object_unref (image);
}
static void loaded_cache (void)
{
  GimpImage *image = image_new (8,8);
  GimpLayer *source = source_new (image,NULL,8,8);
  GimpFilterLayer *filter = filter_new (image,NULL,8,8);
  fill (GIMP_LAYER (filter),77,88,99,123); gimp_filter_layer_mark_as_loaded (filter);
  spin_ms (40); pixel (GIMP_LAYER (filter),77,88,99,123);
  g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 0);
  fill (source,0,0,255,255); settle (filter); pixel (GIMP_LAYER (filter),0,0,0,255);
  g_object_unref (image);
}
static void dependency_order (void)
{
  GimpImage *image = image_new (256,1024);
  GimpLayer *source = source_new (image,NULL,256,1024);
  GimpFilterLayer *lower, *upper;
  guint64 lower_count, upper_count;
  fill (source,255,0,0,255);
  lower = filter_new (image,NULL,256,1024); upper = filter_new (image,NULL,256,1024);
  spin_ms (8);
  g_assert_cmpuint (gimp_filter_layer_get_run_count (upper), ==, 0);
  settle (upper); g_assert_cmpint (gimp_filter_layer_get_state (lower), ==, GIMP_FILTER_LAYER_CLEAN);
  pixel (GIMP_LAYER (upper),0,0,0,255);
  lower_count = gimp_filter_layer_get_run_count (lower); upper_count = gimp_filter_layer_get_run_count (upper);
  fill (source,0,255,0,255); settle (upper);
  g_assert_cmpuint (gimp_filter_layer_get_run_count (lower), ==, lower_count+1);
  g_assert_cmpuint (gimp_filter_layer_get_run_count (upper), ==, upper_count+1);
  spin_ms (30); g_assert_cmpuint (gimp_filter_layer_get_run_count (upper), ==, upper_count+1);
  g_object_unref (image);
}
static void count_filter_state (GimpFilterLayer *layer, gpointer data)
{ ++*(guint *) data; }
static void long_chain_coalesces_state_notifications (void)
{
  GimpImage *image = image_new (16,16);
  GimpLayer *source = source_new (image,NULL,16,16);
  GimpFilterLayer *layers[20];
  guint notifications = 0;
  fill (source,55,127,240,255);
  for (guint i = 0; i < G_N_ELEMENTS (layers); ++i) layers[i] = filter_new (image,NULL,16,16);
  settle (layers[G_N_ELEMENTS (layers)-1]);
  for (guint i = 0; i < G_N_ELEMENTS (layers); ++i)
    {
      g_assert_cmpuint (gimp_filter_layer_get_run_count (layers[i]), ==, 1);
      g_signal_connect (layers[i],"filter-state-changed",G_CALLBACK (count_filter_state),&notifications);
    }
  fill (source,240,55,127,255); settle (layers[G_N_ELEMENTS (layers)-1]);
  for (guint i = 0; i < G_N_ELEMENTS (layers); ++i)
    g_assert_cmpuint (gimp_filter_layer_get_run_count (layers[i]), ==, 2);
  g_test_message ("20-filter chain state notifications for one source edit: %u",notifications);
  g_assert_cmpuint (notifications, <, G_N_ELEMENTS (layers) * 8);
  pixel (GIMP_LAYER (layers[G_N_ELEMENTS (layers)-1]),0,0,0,255);
  g_object_unref (image);
}
static void group_scope (void)
{
  GimpImage *image = image_new (16,16);
  GimpLayer *group = gimp_group_layer_new (image);
  GimpLayer *source;
  GimpFilterLayer *filter;
  gimp_image_add_layer (image,group,NULL,0,FALSE);
  source = source_new (image,group,16,16); fill (source,0,255,0,255);
  filter = filter_new (image,group,16,16); settle (filter);
  pixel (GIMP_LAYER (filter),0,0,0,255);
  fill (source,255,0,0,255); settle (filter);
  g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 2);
  g_object_unref (image);
}
static void completed_buffer_only (void)
{
  GimpImage *image = image_new (512,1024);
  GimpLayer *source = source_new (image,NULL,512,1024);
  GimpFilterLayer *filter = filter_new (image,NULL,512,1024);
  GeglNode *node;
  guchar rgba[4];
  fill (GIMP_LAYER (filter),11,22,33,255); gimp_filter_layer_mark_as_loaded (filter);
  fill (source,255,0,0,255);
  spin_ms (5); pixel (GIMP_LAYER (filter),11,22,33,255);
  node = gimp_drawable_get_source_node (GIMP_DRAWABLE (filter));
  gegl_node_blit (node,1,GEGL_RECTANGLE (0,0,1,1),babl_format ("R'G'B'A u8"),rgba,GEGL_AUTO_ROWSTRIDE,GEGL_BLIT_DEFAULT);
  g_assert_cmpint (rgba[0], ==, 11);
  settle (filter); pixel (GIMP_LAYER (filter),0,0,0,255);
  g_object_unref (image);
}
static void close_during_preparation (void)
{
  GimpImage *image = image_new (256,1024);
  GimpLayer *source = source_new (image,NULL,256,1024);
  GimpFilterLayer *filter = filter_new (image,NULL,256,1024);
  gint64 time;
  fill (source,255,255,255,255); spin_ms (5);
  time = g_get_monotonic_time ();
  g_assert_true (gimp_painter_binding_close (G_OBJECT (filter),NULL));
  g_assert_cmpint (g_get_monotonic_time () - time, <, 100000);
  fill (source,0,0,0,255); spin_ms (30);
  g_assert_cmpint (gimp_filter_layer_get_state (filter), ==, GIMP_FILTER_LAYER_CLOSED);
  g_object_unref (image);
}
static void duplicate_keeps_definition (void)
{
  GimpImage *image = image_new (8,8);
  GimpLayer *source = source_new (image,NULL,8,8);
  GimpFilterLayer *filter = filter_new (image,NULL,8,8);
  GimpItem *copy;
  gchar *name;
  fill (source,255,255,255,255); settle (filter);
  copy = gimp_item_duplicate (GIMP_ITEM (filter),GIMP_TYPE_FILTER_LAYER);
  g_assert_true (GIMP_IS_FILTER_LAYER (copy));
  name = gimp_filter_layer_dup_procedure (GIMP_FILTER_LAYER (copy));
  g_assert_cmpstr (name, ==, "plug-in-edge"); g_free (name);
  pixel (GIMP_LAYER (copy),0,0,0,255);
  g_assert_cmpuint (gimp_filter_layer_get_run_count (GIMP_FILTER_LAYER (copy)), ==, 0);
  g_object_unref (copy); g_object_unref (image);
}

static void weak_finalized (gpointer data, GObject *object)
{ gint *count = data; (*count)++; }
static void object_arguments_do_not_cycle (void)
{
  gint finalized = 0;
  GimpImage *image = image_new (8,8);
  GimpLayer *source = source_new (image,NULL,8,8);
  GimpFilterLayer *filter = filter_new (image,NULL,8,8);
  GimpValueArray *args = gimp_value_array_new_from_types (NULL,
    G_TYPE_INT,1, GIMP_TYPE_IMAGE,image, GIMP_TYPE_DRAWABLE,source,
    G_TYPE_DOUBLE,2.0, G_TYPE_INT,2, G_TYPE_INT,0, G_TYPE_NONE);
  GimpValueArray *copy;
  GType type = G_TYPE_INVALID;
  gint64 id = 0;
  gboolean expired = TRUE;
  g_object_weak_ref (G_OBJECT (image),weak_finalized,&finalized);
  g_object_weak_ref (G_OBJECT (source),weak_finalized,&finalized);
  g_object_weak_ref (G_OBJECT (filter),weak_finalized,&finalized);
  g_assert_true (gimp_filter_layer_set_definition (filter,"plug-in-edge",NULL,args,NULL));
  gimp_value_array_unref (args);
  g_assert_true (gimp_filter_layer_get_argument_reference (filter,1,0,&type,&id,&expired));
  g_assert_cmpuint (type, ==, GIMP_TYPE_IMAGE);
  g_assert_cmpint (id, ==, gimp_image_get_id (image)); g_assert_false (expired);
  copy = gimp_filter_layer_dup_args (filter);
  g_assert_true (g_value_get_object (gimp_value_array_index (copy,1)) == image);
  g_assert_true (g_value_get_object (gimp_value_array_index (copy,2)) == source);
  gimp_value_array_unref (copy);
  g_object_unref (image);
  spin_ms (20);
  g_assert_cmpint (finalized, ==, 3);
}
static void expired_object_records_and_reassignment (void)
{
  gint finalized = 0;
  GimpImage *image = image_new (8,8);
  GimpFilterLayer *filter = filter_new (image,NULL,8,8);
  GObject *object = g_object_new (G_TYPE_OBJECT,NULL);
  GimpValueArray *args = gimp_value_array_new_from_types (NULL,G_TYPE_OBJECT,object,G_TYPE_NONE);
  GimpValueArray *copy;
  GType type = G_TYPE_INVALID;
  gint64 id = -1;
  gboolean expired = FALSE;
  g_object_weak_ref (object,weak_finalized,&finalized);
  g_assert_true (gimp_filter_layer_set_definition (filter,"future-object-procedure",NULL,args,NULL));
  gimp_value_array_unref (args); g_object_unref (object);
  g_assert_cmpint (finalized, ==, 1);
  g_assert_true (gimp_filter_layer_get_argument_reference (filter,0,0,&type,&id,&expired));
  g_assert_true (expired); g_assert_cmpuint (type, ==, G_TYPE_OBJECT);
  copy = gimp_filter_layer_dup_args (filter);
  g_assert_true (G_VALUE_HOLDS_OBJECT (gimp_value_array_index (copy,0)));
  g_assert_null (g_value_get_object (gimp_value_array_index (copy,0)));
  gimp_value_array_unref (copy);
  define_edge (filter);
  g_assert_false (gimp_filter_layer_get_argument_reference (filter,0,0,NULL,NULL,NULL));
  g_object_unref (image);
}
static void object_array_arguments_do_not_dangle (void)
{
  gint finalized = 0;
  GimpImage *image = image_new (8,8);
  GimpFilterLayer *filter = filter_new (image,NULL,8,8);
  GObject *object = g_object_new (G_TYPE_OBJECT,NULL);
  GObject *array[] = {G_OBJECT (image), object, NULL};
  GimpValueArray *args = gimp_value_array_new_from_types (NULL,GIMP_TYPE_CORE_OBJECT_ARRAY,array,G_TYPE_NONE);
  gboolean expired = FALSE;
  g_object_weak_ref (object,weak_finalized,&finalized);
  g_object_weak_ref (G_OBJECT (image),weak_finalized,&finalized);
  g_object_weak_ref (G_OBJECT (filter),weak_finalized,&finalized);
  g_assert_true (gimp_filter_layer_set_definition (filter,"future-array-procedure",NULL,args,NULL));
  gimp_value_array_unref (args); g_object_unref (object);
  g_assert_cmpint (finalized, ==, 1);
  g_assert_true (gimp_filter_layer_get_argument_reference (filter,0,1,NULL,NULL,&expired));
  g_assert_true (expired);
  g_assert_null (gimp_filter_layer_dup_args (filter));
  g_object_unref (image); spin_ms (10); g_assert_cmpint (finalized, ==, 3);
}
static void removal_and_undo (void)
{
  GimpImage *image = image_new (256,1024);
  GimpLayer *source = source_new (image,NULL,256,1024);
  GimpFilterLayer *filter = filter_new (image,NULL,256,1024);
  fill (source,255,255,255,255);
  spin_ms (4);
  g_object_ref (filter);
  gimp_image_remove_layer (image,GIMP_LAYER (filter),TRUE,NULL);
  spin_ms (30);
  g_assert_cmpuint (gimp_filter_layer_get_cache_generation (filter), !=, gimp_filter_layer_get_generation (filter));
  gimp_image_undo (image); settle (filter);
  pixel (GIMP_LAYER (filter),0,0,0,255);
  g_object_unref (filter); g_object_unref (image);
}
static void lower_group_failure_and_recovery (void)
{
  GimpImage *image = image_new (16,16);
  GimpLayer *group = gimp_group_layer_new (image);
  GimpLayer *source;
  GimpFilterLayer *inner, *upper;
  gimp_image_add_layer (image,group,NULL,0,FALSE);
  source = source_new (image,group,16,16); fill (source,255,0,0,255);
  inner = filter_new (image,group,16,16);
  upper = filter_new (image,NULL,16,16); settle (upper);
  g_assert_true (gimp_filter_layer_set_definition (inner,"unsupported",NULL,NULL,NULL));
  spin_ms (40);
  g_assert_cmpint (gimp_filter_layer_get_state (inner), ==, GIMP_FILTER_LAYER_FAILED);
  g_assert_cmpint (gimp_filter_layer_get_state (upper), ==, GIMP_FILTER_LAYER_FAILED);
  define_edge (inner); settle (upper);
  g_assert_cmpint (gimp_filter_layer_get_state (inner), ==, GIMP_FILTER_LAYER_CLEAN);
  g_object_unref (image);
}


static void clone_filter_dependency_cycle (void)
{
  GimpImage *image = image_new (16,16);
  GimpFilterLayer *filter = filter_new (image,NULL,16,16);
  GimpLayer *clone = gimp_clone_layer_new (image,GIMP_LAYER (filter),16,16,"cyclic lower clone",1,GIMP_LAYER_MODE_NORMAL_LEGACY);
  gimp_image_add_layer (image,clone,NULL,1,FALSE);
  spin_ms (50);
  g_assert_cmpint (gimp_filter_layer_get_state (filter), ==, GIMP_FILTER_LAYER_FAILED);
  g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 0);
  spin_ms (30); g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 0);
  gimp_image_remove_layer (image,clone,FALSE,NULL); settle (filter);
  g_object_unref (image);
}
static void clone_filter_dependency_order (void)
{
  GimpImage *source_image = image_new (128,512), *image = image_new (32,32);
  GimpLayer *source = source_new (source_image,NULL,128,512);
  GimpFilterLayer *lower = filter_new (source_image,NULL,128,512), *upper;
  GimpLayer *clone = gimp_clone_layer_new (image,GIMP_LAYER (lower),32,32,"cross-image filter source",1,GIMP_LAYER_MODE_NORMAL_LEGACY);
  fill (source,55,127,240,255);
  gimp_image_add_layer (image,clone,NULL,0,FALSE);
  upper = filter_new (image,NULL,32,32);
  settle (upper);
  g_assert_cmpint (gimp_filter_layer_get_state (lower), ==, GIMP_FILTER_LAYER_CLEAN);
  g_assert_cmpuint (gimp_filter_layer_get_run_count (upper), ==, 1);
  pixel (GIMP_LAYER (upper),0,0,0,255);
  fill (source,240,55,127,255); settle (upper);
  g_assert_cmpint (gimp_filter_layer_get_state (lower), ==, GIMP_FILTER_LAYER_CLEAN);
  g_assert_cmpuint (gimp_filter_layer_get_run_count (upper), ==, 2);
  g_object_unref (image); g_object_unref (source_image);
}
static void cached_dependency_close_before_start (void)
{
  GimpImage *source_image = image_new (16,16), *image = image_new (256,512);
  GimpLayer *source = source_new (source_image,NULL,16,16);
  GimpFilterLayer *lower = filter_new (source_image,NULL,16,16), *upper;
  GimpLayer *clone;
  fill (source,55,127,240,255); settle (lower);
  clone = gimp_clone_layer_new (image,GIMP_LAYER (lower),256,512,"retained dependency",1,GIMP_LAYER_MODE_NORMAL_LEGACY);
  gimp_image_add_layer (image,clone,NULL,0,FALSE);
  upper = filter_new (image,NULL,256,512);
  while (gimp_filter_layer_get_state (upper) == GIMP_FILTER_LAYER_WAITING)
    { g_main_context_iteration (NULL,FALSE); g_usleep (100); }
  g_assert_cmpint (gimp_filter_layer_get_state (upper), ==, GIMP_FILTER_LAYER_PREPARING);
  /* Explicit close emits no Filter state notification; cached validation must
   * still inspect live state before launching or importing any work. */
  gimp_painter_binding_close (G_OBJECT (lower),NULL); spin_ms (30);
  g_assert_cmpint (gimp_filter_layer_get_state (upper), ==, GIMP_FILTER_LAYER_FAILED);
  g_assert_cmpuint (gimp_filter_layer_get_run_count (upper), ==, 0);
  g_object_unref (image); g_object_unref (source_image);
}
static void cross_image_filter_cycle_has_no_signal_loop (void)
{
  GimpImage *a = image_new (16,16), *b = image_new (16,16);
  GimpFilterLayer *fa = filter_new (a,NULL,16,16), *fb = filter_new (b,NULL,16,16);
  GimpLayer *ca = gimp_clone_layer_new (a,GIMP_LAYER (fb),16,16,"cycle a",1,GIMP_LAYER_MODE_NORMAL_LEGACY);
  GimpLayer *cb = gimp_clone_layer_new (b,GIMP_LAYER (fa),16,16,"cycle b",1,GIMP_LAYER_MODE_NORMAL_LEGACY);
  gimp_image_add_layer (a,ca,NULL,1,FALSE); gimp_image_add_layer (b,cb,NULL,1,FALSE);
  spin_ms (30);
  g_assert_cmpint (gimp_filter_layer_get_state (fa), ==, GIMP_FILTER_LAYER_FAILED);
  g_assert_cmpint (gimp_filter_layer_get_state (fb), ==, GIMP_FILTER_LAYER_FAILED);
  define_edge (fa); spin_ms (30);
  g_assert_cmpint (gimp_filter_layer_get_state (fa), ==, GIMP_FILTER_LAYER_FAILED);
  g_assert_cmpuint (gimp_filter_layer_get_run_count (fa), ==, 0);
  g_assert_cmpuint (gimp_filter_layer_get_run_count (fb), ==, 0);
  g_object_unref (a); g_object_unref (b);
}

typedef struct { GimpImage *image; GimpLayer *clone; gboolean close; gboolean called; } PendingResolution;
static void mutate_on_pending_resolution (GimpDrawable *drawable, gint x, gint y, gint w, gint h, gpointer data)
{
  PendingResolution *mutation = data;
  if (mutation->called) return;
  mutation->called = TRUE;
  if (mutation->close)
    {
      GimpImage *image = mutation->image; mutation->image = NULL;
      g_object_unref (image);
    }
  else
    gimp_image_remove_layer (mutation->image,mutation->clone,FALSE,NULL);
}
static void dependency_walk_does_not_resolve_pending_names (void)
{
  for (guint close = 0; close < 2; ++close)
    {
      GimpImage *image = image_new (16,16);
      GimpLayer *source = source_new (image,NULL,16,16);
      GimpLayer *clone = gimp_clone_layer_new (image,NULL,16,16,"pending clone",1,GIMP_LAYER_MODE_NORMAL_LEGACY);
      GimpFilterLayer *filter;
      PendingResolution mutation = {image,clone,close,FALSE};
      gimp_object_set_name (GIMP_OBJECT (source),"pending target"); fill (source,55,127,240,255);
      gimp_image_add_layer (image,clone,NULL,0,FALSE); g_object_ref (clone);
      gimp_clone_layer_set_source_by_name (GIMP_CLONE_LAYER (clone),"pending target");
      g_signal_connect (clone,"update",G_CALLBACK (mutate_on_pending_resolution),&mutation);
      filter = filter_new (image,NULL,16,16); g_object_ref (filter);
      settle (filter);
      g_assert_false (mutation.called);
      g_assert_cmpint (gimp_clone_layer_get_source_state (GIMP_CLONE_LAYER (clone)), ==, GIMP_CLONE_SOURCE_PENDING);
      /* Resolution remains an explicit legacy getter action outside borrowed
       * traversal. Its callback may remove the clone or close the image. */
      gimp_clone_layer_get_source (GIMP_CLONE_LAYER (clone));
      g_assert_true (mutation.called);
      if (close)
        {
          g_assert_cmpint (gimp_filter_layer_get_state (filter), ==, GIMP_FILTER_LAYER_CLOSED);
          g_assert_null (gimp_item_get_image (GIMP_ITEM (filter)));
        }
      else
        { settle (filter); g_object_unref (mutation.image); }
      g_object_unref (clone); g_object_unref (filter); spin_ms (5);
    }
}
static void suppress_clone_update (GimpDrawable *drawable, gint x, gint y, gint w, gint h, gpointer data)
{ if (*(gboolean *) data) g_signal_stop_emission_by_name (drawable,"update"); }
typedef struct { GimpImage *image; gboolean armed; gboolean called; } CloseOnDependency;
static void close_on_dependency_invalidation (GimpFilterLayer *filter, gpointer data)
{
  CloseOnDependency *state = data;
  if (state->armed && !state->called && gimp_filter_layer_get_state (filter) == GIMP_FILTER_LAYER_WAITING)
    {
      GimpImage *image = state->image; state->image = NULL; state->called = TRUE;
      g_object_unref (image);
    }
}
static void changed_dependency_invalidation_can_close_owner (void)
{
  GimpImage *sources = image_new (16,16), *image = image_new (256,512);
  GimpLayer *a = source_new (sources,NULL,16,16), *b = source_new (sources,NULL,16,16);
  GimpLayer *clone = gimp_clone_layer_new (image,a,256,512,"silent source change",1,GIMP_LAYER_MODE_NORMAL_LEGACY);
  GimpFilterLayer *filter;
  gboolean suppress = FALSE;
  CloseOnDependency state = {image,FALSE,FALSE};
  gint64 deadline = g_get_monotonic_time () + 10 * G_TIME_SPAN_SECOND;
  fill (a,55,127,240,255); fill (b,127,55,240,255);
  gimp_image_add_layer (image,clone,NULL,0,FALSE);
  g_signal_connect (clone,"update",G_CALLBACK (suppress_clone_update),&suppress);
  filter = filter_new (image,NULL,256,512); g_object_ref (filter);
  g_signal_connect (filter,"filter-state-changed",G_CALLBACK (close_on_dependency_invalidation),&state);
  while (gimp_filter_layer_get_state (filter) == GIMP_FILTER_LAYER_WAITING && g_get_monotonic_time () < deadline)
    { g_main_context_iteration (NULL,FALSE); g_usleep (100); }
  g_assert_cmpint (gimp_filter_layer_get_state (filter), ==, GIMP_FILTER_LAYER_PREPARING);
  suppress = TRUE; gimp_clone_layer_set_source (GIMP_CLONE_LAYER (clone),b); suppress = FALSE;
  state.armed = TRUE;
  while (!state.called && g_get_monotonic_time () < deadline)
    { g_main_context_iteration (NULL,FALSE); g_usleep (100); }
  g_assert_true (state.called);
  g_assert_cmpint (gimp_filter_layer_get_state (filter), ==, GIMP_FILTER_LAYER_CLOSED);
  g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 0);
  g_assert_null (gimp_item_get_image (GIMP_ITEM (filter)));
  g_object_unref (filter); g_object_unref (sources); spin_ms (10);
}

static void pending_cycle_resolved_during_graph_read_is_discarded (void)
{
  for (guint suppress_signal = 0; suppress_signal < 2; ++suppress_signal)
    {
      gboolean suppress = FALSE;
      GimpImage *image = image_new (16,16);
      TestResolvingLayer *source = (TestResolvingLayer *) gimp_drawable_new (
        test_resolving_layer_get_type (),image,"late resolver",0,0,16,16,gimp_image_get_layer_format (image,TRUE));
      GimpLayer *clone = gimp_clone_layer_new (image,NULL,16,16,"pending cycle",1,GIMP_LAYER_MODE_NORMAL_LEGACY);
      GimpFilterLayer *filter;
      gint64 deadline = g_get_monotonic_time () + G_TIME_SPAN_SECOND * 10;
      gimp_image_add_layer (image,GIMP_LAYER (source),NULL,0,FALSE); fill (GIMP_LAYER (source),55,127,240,255);
      gimp_image_add_layer (image,clone,NULL,0,FALSE);
      gimp_clone_layer_set_source_by_name (GIMP_CLONE_LAYER (clone),"cycle filter");
      g_signal_connect (clone,"update",G_CALLBACK (suppress_clone_update),&suppress);
      filter = filter_new (image,NULL,16,16); gimp_object_set_name (GIMP_OBJECT (filter),"cycle filter");
      source->pending = GIMP_CLONE_LAYER (clone); suppress = suppress_signal;
      g_assert_null (gimp_filter_peek_node (GIMP_FILTER (source)));
      while (gimp_filter_layer_get_state (filter) != GIMP_FILTER_LAYER_FAILED && g_get_monotonic_time () < deadline)
        { g_main_context_iteration (NULL,FALSE); g_usleep (100); }
      g_assert_true (source->resolved);
      g_assert_cmpint (gimp_clone_layer_get_source_state (GIMP_CLONE_LAYER (clone)), ==, GIMP_CLONE_SOURCE_LIVE);
      g_assert_cmpint (gimp_filter_layer_get_state (filter), ==, GIMP_FILTER_LAYER_FAILED);
      g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 0);
      spin_ms (15); g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 0);
      g_object_unref (image);
    }
}
static void definition_revision_separates_cache_updates (void)
{
  GimpImage *image = image_new (8,8);
  GimpLayer *source = source_new (image,NULL,8,8);
  GimpFilterLayer *filter = filter_new (image,NULL,8,8);
  GimpFilterArgumentsSnapshot *empty;
  GimpFilterLayerSnapshot cache;
  GimpValueArray *args;
  guint64 revision = gimp_filter_layer_get_definition_revision (filter);
  GError *error = NULL;
  gint opaque = 0;
  g_assert_cmpuint (revision, ==, 1);
  fill (source,55,127,240,255); settle (filter);
  gimp_filter_layer_invalidate (filter); settle (filter);
  gimp_filter_layer_mark_as_loaded (filter);
  g_assert_true (gimp_filter_layer_get_snapshot_state (filter,&cache));
  g_assert_true (gimp_filter_layer_restore_snapshot_state (filter,&cache,&error)); g_assert_no_error (error);
  g_assert_cmpuint (gimp_filter_layer_get_definition_revision (filter), ==, revision);
  g_assert_true (gimp_filter_layer_set_definition (filter,"plug-in-edge",NULL,NULL,&error)); g_assert_no_error (error);
  g_assert_cmpuint (gimp_filter_layer_get_definition_revision (filter), ==, ++revision);
  args = gimp_value_array_new_from_types (NULL,G_TYPE_POINTER,&opaque,G_TYPE_NONE);
  g_assert_false (gimp_filter_layer_set_definition (filter,"opaque",NULL,args,&error));
  g_assert_nonnull (error); g_clear_error (&error); gimp_value_array_unref (args);
  g_assert_cmpuint (gimp_filter_layer_get_definition_revision (filter), ==, revision);
  empty = gimp_filter_arguments_snapshot_import (0,NULL,&error); g_assert_no_error (error); g_assert_nonnull (empty);
  g_assert_true (gimp_filter_layer_set_definition_with_snapshot (filter,"plug-in-edge",NULL,empty,&error));
  g_assert_no_error (error); gimp_filter_arguments_snapshot_free (empty);
  g_assert_cmpuint (gimp_filter_layer_get_definition_revision (filter), ==, ++revision);
  args = edge_args ();
  g_assert_true (gimp_filter_layer_edit_definition (filter,"plug-in-edge",NULL,args,&error));
  g_assert_no_error (error); gimp_value_array_unref (args);
  g_assert_cmpuint (gimp_filter_layer_get_definition_revision (filter), ==, ++revision);
  g_assert_true (gimp_image_undo (image));
  g_assert_cmpuint (gimp_filter_layer_get_definition_revision (filter), ==, ++revision);
  g_assert_true (gimp_image_redo (image));
  g_assert_cmpuint (gimp_filter_layer_get_definition_revision (filter), ==, ++revision);
  settle (filter); g_assert_cmpuint (gimp_filter_layer_get_definition_revision (filter), ==, revision);
  g_object_unref (image);
}

typedef struct { GimpFilterLayer *filter; gboolean replace; gboolean called; guint late_notifies; } DefinitionReentry;
static void definition_reentered (DefinitionReentry *state)
{
  if (state->called) return;
  state->called = TRUE;
  if (state->replace)
    {
      GimpValueArray *args = edge_args ();
      g_value_set_double (gimp_value_array_index (args,3),7.0);
      g_assert_true (gimp_filter_layer_set_definition (state->filter,"plug-in-edge",NULL,args,NULL));
      gimp_value_array_unref (args);
    }
  else
    g_assert_true (gimp_painter_binding_close (G_OBJECT (state->filter),NULL));
}
static void definition_dirty_reentry (GimpImage *image, GimpDirtyMask dirty, gpointer data)
{ definition_reentered (data); }
static void definition_undo_event_reentry (GimpImage *image, GimpUndoEvent event, GimpUndo *undo, gpointer data)
{ if (event == GIMP_UNDO_EVENT_UNDO_PUSHED) definition_reentered (data); }
static void definition_status_reentry (GimpFilterLayer *filter, gpointer data)
{ definition_reentered (data); }
static void definition_notify_reentry (GObject *object, GParamSpec *spec, gpointer data)
{ definition_reentered (data); }
static void definition_notify_count (GObject *object, GParamSpec *spec, gpointer data)
{ DefinitionReentry *state = data; if (state->called) ++state->late_notifies; }
static void observe_definition_notifies (GimpFilterLayer *filter, DefinitionReentry *state)
{
  g_signal_connect (filter,"notify::filter-procedure",G_CALLBACK (definition_notify_count),state);
  g_signal_connect (filter,"notify::filter-arguments",G_CALLBACK (definition_notify_count),state);
  g_signal_connect (filter,"notify::filter-original-definition",G_CALLBACK (definition_notify_count),state);
  g_signal_connect (filter,"notify::filter-opaque-arguments",G_CALLBACK (definition_notify_count),state);
}
static void definition_edit_stops_after_undo_close (void)
{
  for (guint phase = 0; phase < 3; ++phase)
    {
      GimpImage *image = image_new (8,8);
      GimpFilterLayer *filter = filter_new (image,NULL,8,8);
      GimpValueArray *args = edge_args ();
      DefinitionReentry state = {filter,FALSE,FALSE,0};
      GError *error = NULL;
      if (phase == 1) gimp_image_undo_freeze (image);
      observe_definition_notifies (filter,&state);
      if (phase == 2) g_signal_connect (image,"undo-event",G_CALLBACK (definition_undo_event_reentry),&state);
      else g_signal_connect (image,"dirty",G_CALLBACK (definition_dirty_reentry),&state);
      g_assert_false (gimp_filter_layer_edit_definition (filter,"plug-in-edge",NULL,args,&error));
      g_assert_nonnull (error); g_clear_error (&error); gimp_value_array_unref (args);
      g_assert_true (state.called); g_assert_cmpuint (state.late_notifies, ==, 0);
      g_assert_cmpint (gimp_filter_layer_get_state (filter), ==, GIMP_FILTER_LAYER_CLOSED);
      g_object_unref (image);
    }
}
static void definition_edit_preserves_reentered_install (void)
{
  GimpImage *image = image_new (8,8);
  GimpFilterLayer *filter = filter_new (image,NULL,8,8);
  GimpValueArray *args = edge_args (), *saved;
  DefinitionReentry state = {filter,TRUE,FALSE,0};
  GError *error = NULL;
  guint64 revision = gimp_filter_layer_get_definition_revision (filter);
  g_signal_connect (image,"dirty",G_CALLBACK (definition_dirty_reentry),&state);
  g_value_set_double (gimp_value_array_index (args,3),3.0);
  g_assert_false (gimp_filter_layer_edit_definition (filter,"plug-in-edge",NULL,args,&error));
  g_assert_nonnull (error); g_clear_error (&error); gimp_value_array_unref (args);
  g_assert_true (state.called);
  g_assert_cmpuint (gimp_filter_layer_get_definition_revision (filter), ==, revision+1);
  saved = gimp_filter_layer_dup_args (filter);
  g_assert_cmpfloat (g_value_get_double (gimp_value_array_index (saved,3)), ==, 7.0);
  gimp_value_array_unref (saved); g_object_unref (image);
}
static void definition_notifications_stop_after_close (void)
{
  for (guint phase = 0; phase < 2; ++phase)
    {
      GimpImage *image = image_new (8,8);
      GimpFilterLayer *filter = filter_new (image,NULL,8,8);
      GimpValueArray *args = edge_args ();
      DefinitionReentry state = {filter,FALSE,FALSE,0};
      GError *error = NULL;
      observe_definition_notifies (filter,&state);
      if (phase) g_signal_connect (filter,"notify::filter-procedure",G_CALLBACK (definition_notify_reentry),&state);
      else g_signal_connect (filter,"filter-state-changed",G_CALLBACK (definition_status_reentry),&state);
      g_assert_false (gimp_filter_layer_set_definition (filter,"plug-in-edge",NULL,args,&error));
      g_assert_nonnull (error); g_clear_error (&error); gimp_value_array_unref (args);
      g_assert_true (state.called); g_assert_cmpuint (state.late_notifies, ==, 0);
      g_assert_cmpint (gimp_filter_layer_get_state (filter), ==, GIMP_FILTER_LAYER_CLOSED);
      g_object_unref (image);
    }
}

static void assert_opaque (GimpFilterLayer *filter, GBytes *expected)
{
  GBytes *actual = gimp_filter_layer_ref_opaque_arguments (filter);
  g_assert_nonnull (actual); g_assert_true (g_bytes_equal (actual,expected)); g_bytes_unref (actual);
  g_assert_null (gimp_filter_layer_snapshot_arguments (filter));
  g_assert_null (gimp_filter_layer_dup_args (filter));
}
static void opaque_arguments_duplicate_and_undo (void)
{
  static const guchar opaque_data[] = {0xff,0,0x80,3,0,27};
  static const guchar raw_data[] = {32,0,89,0xff};
  GimpImage *image = image_new (8,8);
  GimpFilterLayer *filter = filter_new (image,NULL,8,8);
  GBytes *opaque = g_bytes_new_static (opaque_data,sizeof opaque_data), *raw = g_bytes_new_static (raw_data,sizeof raw_data), *saved;
  GimpItem *copy;
  GimpValueArray *args = edge_args ();
  fill (GIMP_LAYER (filter),31,47,93,255);
  g_assert_true (gimp_filter_layer_set_definition_with_opaque_arguments (filter,"plug-in-edge",raw,opaque,NULL));
  gimp_filter_layer_mark_as_loaded (filter); assert_opaque (filter,opaque);
  saved = gimp_filter_layer_ref_definition (filter); g_assert_true (g_bytes_equal (saved,raw)); g_bytes_unref (saved);
  copy = gimp_item_duplicate (GIMP_ITEM (filter),GIMP_TYPE_FILTER_LAYER);
  g_assert_nonnull (copy); assert_opaque (GIMP_FILTER_LAYER (copy),opaque);
  g_assert_cmpuint (gimp_filter_layer_get_definition_revision (GIMP_FILTER_LAYER (copy)), >, 0);
  g_assert_cmpuint (gimp_filter_layer_get_run_count (GIMP_FILTER_LAYER (copy)), ==, 0);
  g_object_unref (copy);
  g_assert_true (gimp_filter_layer_edit_definition (filter,"plug-in-edge",NULL,args,NULL));
  g_assert_null (gimp_filter_layer_ref_opaque_arguments (filter));
  g_assert_cmpint (gimp_object_get_memsize (GIMP_OBJECT (gimp_undo_stack_peek (gimp_image_get_undo_stack (image))),NULL), >=, 8*8*4);
  settle (filter); pixel (GIMP_LAYER (filter),0,0,0,0);
  g_assert_true (gimp_image_undo (image)); assert_opaque (filter,opaque);
  pixel (GIMP_LAYER (filter),31,47,93,255);
  spin_ms (15); pixel (GIMP_LAYER (filter),31,47,93,255);
  g_assert_cmpint (gimp_filter_layer_get_state (filter), ==, GIMP_FILTER_LAYER_FAILED);
  g_assert_cmpuint (gimp_filter_layer_get_cache_generation (filter), !=, gimp_filter_layer_get_generation (filter));
  saved = gimp_filter_layer_ref_definition (filter); g_assert_true (g_bytes_equal (saved,raw)); g_bytes_unref (saved);
  g_assert_true (gimp_image_redo (image)); g_assert_null (gimp_filter_layer_ref_opaque_arguments (filter));
  g_assert_null (gimp_filter_layer_ref_definition (filter));
  settle (filter); pixel (GIMP_LAYER (filter),0,0,0,0);
  gimp_value_array_unref (args); g_bytes_unref (opaque); g_bytes_unref (raw); g_object_unref (image);
}
static void duplicate_preserves_cache_freshness (void)
{
  for (guint phase = 0; phase < 3; ++phase)
    {
      GimpImage *image = image_new (8,8);
      GimpFilterLayer *filter = filter_new (image,NULL,8,8);
      GBytes *opaque = g_bytes_new_static ("opaque",6);
      GimpFilterLayerSnapshot saved = {1,10,phase == 2 ? 10 : 9,phase != 0,GIMP_FILTER_LAYER_RUNNING}, observed;
      GimpItem *copy;
      fill (GIMP_LAYER (filter),31,47,93,255);
      g_assert_true (gimp_filter_layer_set_definition_with_opaque_arguments (filter,"unknown",NULL,opaque,NULL));
      g_assert_true (gimp_filter_layer_restore_snapshot_state (filter,&saved,NULL));
      copy = gimp_item_duplicate (GIMP_ITEM (filter),GIMP_TYPE_FILTER_LAYER); g_assert_nonnull (copy);
      g_assert_true (gimp_filter_layer_get_snapshot_state (GIMP_FILTER_LAYER (copy),&observed));
      g_assert_cmpint (observed.cache_complete, ==, saved.cache_complete);
      g_assert_cmpint (observed.generation == observed.cache_generation, ==, phase == 2);
      g_assert_cmpuint (gimp_filter_layer_get_run_count (GIMP_FILTER_LAYER (copy)), ==, 0);
      pixel (GIMP_LAYER (copy),31,47,93,255); assert_opaque (GIMP_FILTER_LAYER (copy),opaque);
      g_object_unref (copy); g_bytes_unref (opaque); g_object_unref (image);
    }
}
static void opaque_arguments_never_execute (void)
{
  GimpImage *image = image_new (8,8);
  GimpLayer *source = source_new (image,NULL,8,8);
  GimpFilterLayer *filter = filter_new (image,NULL,8,8);
  GBytes *opaque = g_bytes_new_static ("unavailable canonical model",27);
  guint64 runs;
  fill (source,55,127,240,255); settle (filter); runs = gimp_filter_layer_get_run_count (filter);
  fill (GIMP_LAYER (filter),17,18,19,255);
  g_assert_true (gimp_filter_layer_set_definition_with_opaque_arguments (filter,"plug-in-edge",NULL,opaque,NULL));
  gimp_filter_layer_mark_as_loaded (filter); spin_ms (5);
  g_assert_cmpint (gimp_filter_layer_get_state (filter), ==, GIMP_FILTER_LAYER_CLEAN);
  gimp_filter_layer_invalidate (filter); spin_ms (20);
  g_assert_cmpint (gimp_filter_layer_get_state (filter), ==, GIMP_FILTER_LAYER_FAILED);
  g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, runs);
  assert_opaque (filter,opaque); pixel (GIMP_LAYER (filter),17,18,19,255);
  g_bytes_unref (opaque); g_object_unref (image);
}
static void empty_opaque_arguments_are_distinct (void)
{
  GimpImage *image = image_new (8,8);
  GimpFilterLayer *filter = filter_new (image,NULL,8,8);
  GBytes *empty = g_bytes_new_static ("",0), *property = NULL;
  GimpFilterArgumentsSnapshot *converted = gimp_filter_arguments_snapshot_import (0,NULL,NULL), *snapshot;
  g_assert_true (gimp_filter_layer_set_definition_with_opaque_arguments (filter,"future",NULL,empty,NULL));
  assert_opaque (filter,empty);
  g_object_get (filter,"filter-opaque-arguments",&property,NULL);
  g_assert_nonnull (property); g_assert_cmpuint (g_bytes_get_size (property), ==, 0); g_bytes_unref (property);
  g_assert_true (gimp_filter_layer_set_definition (filter,"future",NULL,NULL,NULL));
  g_assert_null (gimp_filter_layer_ref_opaque_arguments (filter));
  g_assert_true (gimp_filter_layer_set_definition_with_opaque_arguments (filter,"future",NULL,empty,NULL));
  g_assert_true (gimp_filter_layer_set_definition_with_snapshot (filter,"future",NULL,converted,NULL));
  g_assert_null (gimp_filter_layer_ref_opaque_arguments (filter));
  snapshot = gimp_filter_layer_snapshot_arguments (filter); g_assert_nonnull (snapshot);
  g_assert_cmpuint (gimp_filter_arguments_snapshot_count (snapshot), ==, 0);
  gimp_filter_arguments_snapshot_free (snapshot); gimp_filter_arguments_snapshot_free (converted);
  g_bytes_unref (empty); g_object_unref (image);
}
typedef struct { GimpFilterLayer *filter; gboolean called; } PayloadReentry;
static void retired_payload_install_definition (gpointer data)
{
  PayloadReentry *state = data;
  GimpValueArray *args = edge_args ();
  GBytes *raw = g_bytes_new_static ("inner",5);
  state->called = TRUE;
  g_value_set_double (gimp_value_array_index (args,3),7.0);
  g_assert_true (gimp_filter_layer_set_definition (state->filter,"plug-in-edge",raw,args,NULL));
  g_bytes_unref (raw); gimp_value_array_unref (args);
}
static void retired_definition_payload_reentry (void)
{
  GimpImage *image = image_new (8,8);
  GimpFilterLayer *filter = filter_new (image,NULL,8,8);
  PayloadReentry state = {filter,FALSE};
  GBytes *raw = g_bytes_new_with_free_func ("retired",7,retired_payload_install_definition,&state);
  GBytes *opaque = g_bytes_new_static ("opaque",6), *saved;
  GimpValueArray *args = edge_args (), *values;
  g_assert_true (gimp_filter_layer_set_definition_with_opaque_arguments (filter,"old",raw,opaque,NULL));
  g_bytes_unref (raw); g_bytes_unref (opaque); g_assert_false (state.called);
  g_value_set_double (gimp_value_array_index (args,3),3.0);
  g_assert_true (gimp_filter_layer_set_definition (filter,"plug-in-edge",NULL,args,NULL));
  g_assert_true (state.called); g_assert_null (gimp_filter_layer_ref_opaque_arguments (filter));
  values = gimp_filter_layer_dup_args (filter);
  g_assert_cmpfloat (g_value_get_double (gimp_value_array_index (values,3)), ==, 7.0);
  saved = gimp_filter_layer_ref_definition (filter);
  g_assert_cmpmem (g_bytes_get_data (saved,NULL),g_bytes_get_size (saved),"inner",5);
  g_bytes_unref (saved); gimp_value_array_unref (values); gimp_value_array_unref (args); g_object_unref (image);
}
typedef struct { GimpFilterLayer *original; GimpFilterLayer *copy; } DuplicateReentry;
static gboolean replace_duplicate_definition (GSignalInvocationHint *hint, guint n_values, const GValue *values, gpointer data)
{
  DuplicateReentry *state = data;
  GimpFilterLayer *filter = g_value_get_object (values);
  if (filter != state->original && !state->copy)
    {
      GimpValueArray *args = edge_args ();
      state->copy = g_object_ref (filter);
      g_value_set_double (gimp_value_array_index (args,3),7.0);
      g_assert_true (gimp_filter_layer_set_definition (filter,"plug-in-edge",NULL,args,NULL));
      gimp_value_array_unref (args);
    }
  return TRUE;
}
static void duplicate_reentry_cannot_certify_new_definition (void)
{
  GimpImage *image = image_new (8,8);
  GimpFilterLayer *filter = filter_new (image,NULL,8,8);
  DuplicateReentry state = {filter,NULL};
  guint signal = g_signal_lookup ("filter-state-changed",GIMP_TYPE_FILTER_LAYER);
  gulong hook = g_signal_add_emission_hook (signal,0,replace_duplicate_definition,&state,NULL);
  GimpItem *copy = gimp_item_duplicate (GIMP_ITEM (filter),GIMP_TYPE_FILTER_LAYER);
  g_signal_remove_emission_hook (signal,hook);
  g_assert_null (copy); g_assert_nonnull (state.copy);
  g_assert_cmpuint (gimp_filter_layer_get_generation (state.copy), !=, gimp_filter_layer_get_cache_generation (state.copy));
  g_object_unref (state.copy); g_object_unref (image);
}

static gboolean heartbeat (gpointer data)
{ guint *counter = data; ++*counter; return G_SOURCE_CONTINUE; }
static void main_context_remains_responsive (void)
{
  guint beats = 0;
  GimpImage *image = image_new (2048,1536);
  GimpLayer *source = source_new (image,NULL,2048,1536);
  GimpFilterLayer *filter;
  guint heartbeat_source;
  gint64 start;
  fill (source,255,127,42,255); filter = filter_new (image,NULL,2048,1536);
  start = g_get_monotonic_time ();
  heartbeat_source = g_timeout_add (2,heartbeat,&beats);
  settle (filter); g_source_remove (heartbeat_source);
  g_test_message ("2048x1536 evaluation wall=%" G_GINT64_FORMAT "us; largest main-context quantum=%" G_GINT64_FORMAT "us; 2ms heartbeat=%u",
                  g_get_monotonic_time () - start,gimp_filter_layer_get_max_quantum_us (filter),beats);
  g_assert_cmpuint (beats, >, 5);
  pixel (GIMP_LAYER (filter),0,0,0,255);
  g_object_unref (image);
}

typedef struct { gint64 previous; GArray *intervals; } HeartbeatLatency;
static gboolean sample_heartbeat_latency (gpointer data)
{
  HeartbeatLatency *samples = data;
  gint64 now = g_get_monotonic_time (), interval = now - samples->previous;
  samples->previous = now; g_array_append_val (samples->intervals,interval);
  return G_SOURCE_CONTINUE;
}
static gint compare_latency (gconstpointer a, gconstpointer b)
{
  gint64 left = *(const gint64 *) a, right = *(const gint64 *) b;
  return (left > right) - (left < right);
}
static void complex_graph_remains_responsive (void)
{
  GimpImage *image = image_new (1024,1024);
  GimpFilterLayer *filter;
  GimpLayer *top_source = NULL;
  for (guint i = 0; i < 64; ++i)
    {
      top_source = source_new (image,NULL,1024,1024);
      fill (top_source,55+i,127,240,255);
      if (i) gimp_layer_set_opacity (top_source,0.5,FALSE);
    }
  filter = filter_new (image,NULL,1024,1024);
  for (guint phase = 0; phase < 2; ++phase)
    {
      HeartbeatLatency samples = {0,g_array_new (FALSE,FALSE,sizeof (gint64))};
      gint64 start;
      guint timer;
      if (phase) fill (top_source,55,240,127,255);
      start = samples.previous = g_get_monotonic_time ();
      timer = g_timeout_add (2,sample_heartbeat_latency,&samples);
      settle (filter); g_source_remove (timer);
      g_assert_cmpuint (samples.intervals->len, >, 5);
      g_array_sort (samples.intervals,compare_latency);
      g_test_message ("complex-graph phase=%s width=1024 height=1024 layers=64 wall_us=%" G_GINT64_FORMAT
        " max_quantum_us=%" G_GINT64_FORMAT " heartbeat_samples=%u heartbeat_p50_us=%" G_GINT64_FORMAT
        " heartbeat_p95_us=%" G_GINT64_FORMAT " heartbeat_p99_us=%" G_GINT64_FORMAT " heartbeat_max_us=%" G_GINT64_FORMAT,
        phase ? "edit" : "first",g_get_monotonic_time () - start,gimp_filter_layer_get_max_quantum_us (filter),samples.intervals->len,
        g_array_index (samples.intervals,gint64,samples.intervals->len/2),
        g_array_index (samples.intervals,gint64,samples.intervals->len*95/100),
        g_array_index (samples.intervals,gint64,samples.intervals->len*99/100),
        g_array_index (samples.intervals,gint64,samples.intervals->len-1));
      g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, phase+1);
      pixel (GIMP_LAYER (filter),0,0,0,255);
      g_array_unref (samples.intervals);
    }
  g_object_unref (image);
}
static void cached_graph_tracks_clone_reassignment (void)
{
  GimpImage *a = image_new (16,16), *b = image_new (128,512), *image = image_new (256,512);
  GimpLayer *sa = source_new (a,NULL,16,16), *sb = source_new (b,NULL,128,512);
  GimpFilterLayer *fa = filter_new (a,NULL,16,16), *fb = filter_new (b,NULL,128,512), *upper;
  GimpLayer *clone;
  guint64 generation;
  fill (sa,55,127,240,255); settle (fa); fill (sb,127,55,240,255);
  clone = gimp_clone_layer_new (image,GIMP_LAYER (fa),256,512,"changing dependency",1,GIMP_LAYER_MODE_NORMAL_LEGACY);
  gimp_image_add_layer (image,clone,NULL,0,FALSE); upper = filter_new (image,NULL,256,512);
  while (gimp_filter_layer_get_state (upper) == GIMP_FILTER_LAYER_WAITING)
    { g_main_context_iteration (NULL,FALSE); g_usleep (100); }
  g_assert_cmpint (gimp_filter_layer_get_state (upper), ==, GIMP_FILTER_LAYER_PREPARING);
  generation = gimp_filter_layer_get_generation (upper);
  gimp_clone_layer_set_source (GIMP_CLONE_LAYER (clone),GIMP_LAYER (fb));
  g_assert_cmpuint (gimp_filter_layer_get_generation (upper), >, generation);
  settle (upper);
  g_assert_cmpint (gimp_filter_layer_get_state (fb), ==, GIMP_FILTER_LAYER_CLEAN);
  g_assert_cmpuint (gimp_filter_layer_get_run_count (upper), ==, 1);
  g_object_unref (a); g_object_unref (b); spin_ms (10);
  g_assert_null (gimp_clone_layer_get_source (GIMP_CLONE_LAYER (clone)));
  settle (upper);
  g_object_unref (image);
}

static void cpp_header_layout (void)
{
  GimpImage *image = image_new (8,8);
  GimpFilterLayer *filter = filter_new (image,NULL,8,8);
  gimp_test_filter_cpp_layout (sizeof (GimpDrawable),offsetof (GimpDrawable,private),filter);
  g_object_unref (image);
}

static void hidden_filter_and_offset (void)
{
  GimpImage *image = image_new (32,16);
  GimpLayer *source = source_new (image,NULL,16,16);
  GimpFilterLayer *lower = filter_new (image,NULL,8,8);
  GimpFilterLayer *upper;
  fill (source,255,0,0,255); settle (lower);
  gimp_item_set_visible (GIMP_ITEM (lower),FALSE,FALSE);
  fill (source,0,255,0,255);
  upper = filter_new (image,NULL,8,8); settle (upper);
  g_assert_cmpuint (gimp_filter_layer_get_run_count (lower), ==, 1);
  gimp_item_set_visible (GIMP_ITEM (lower),TRUE,FALSE); settle (upper);
  g_assert_cmpuint (gimp_filter_layer_get_run_count (lower), ==, 2);
  gimp_item_set_offset (GIMP_ITEM (lower),20,0); settle (lower);
  pixel (GIMP_LAYER (lower),0,0,0,0);
  gimp_item_set_offset (GIMP_ITEM (lower),0,0); settle (lower);
  pixel (GIMP_LAYER (lower),0,0,0,255);
  g_object_unref (image);
}


static void definition_undo_redo (void)
{
  const guchar before_data[] = {1,2,3,0}, after_data[] = {7,8,9,0};
  gint finalized = 0;
  GimpImage *image = image_new (256,1024);
  GimpLayer *source = source_new (image,NULL,256,1024);
  GimpFilterLayer *filter = filter_new (image,NULL,256,1024);
  GimpValueArray *before_args = gimp_value_array_new_from_types (NULL,
    G_TYPE_INT,1, GIMP_TYPE_IMAGE,image, GIMP_TYPE_DRAWABLE,source,
    G_TYPE_DOUBLE,2.0, G_TYPE_INT,2, G_TYPE_INT,0, G_TYPE_NONE);
  GimpValueArray *after_args = edge_args (), *values;
  GBytes *before = g_bytes_new (before_data,sizeof before_data), *after = g_bytes_new (after_data,sizeof after_data), *raw;
  gchar *procedure;
  g_object_weak_ref (G_OBJECT (image),weak_finalized,&finalized);
  g_object_weak_ref (G_OBJECT (filter),weak_finalized,&finalized);
  fill (source,255,0,0,255);
  g_assert_true (gimp_filter_layer_set_definition (filter,"plug-in-edge",before,before_args,NULL));
  gimp_value_array_unref (before_args); g_bytes_unref (before);
  g_assert_cmpint (gimp_undo_stack_get_depth (gimp_image_get_undo_stack (image)), ==, 0);
  settle (filter);
  g_value_set_double (gimp_value_array_index (after_args,3),3.5);
  g_assert_true (gimp_filter_layer_edit_definition (filter,"plug-in-edge",after,after_args,NULL));
  gimp_value_array_unref (after_args); g_bytes_unref (after);
  spin_ms (6);
  g_assert_cmpint (gimp_undo_stack_get_depth (gimp_image_get_undo_stack (image)), ==, 1);
  g_assert_true (gimp_image_undo (image)); settle (filter);
  g_object_get (filter,"filter-procedure",&procedure,"filter-arguments",&values,"filter-original-definition",&raw,NULL);
  g_assert_cmpstr (procedure, ==, "plug-in-edge"); g_free (procedure);
  g_assert_cmpfloat (g_value_get_double (gimp_value_array_index (values,3)), ==, 2.0);
  g_assert_true (g_value_get_object (gimp_value_array_index (values,1)) == image);
  gimp_value_array_unref (values);
  g_assert_cmpmem (g_bytes_get_data (raw,NULL),g_bytes_get_size (raw),before_data,sizeof before_data);
  g_bytes_unref (raw);
  g_assert_true (gimp_image_redo (image)); settle (filter);
  values = gimp_filter_layer_dup_args (filter);
  g_assert_cmpfloat (g_value_get_double (gimp_value_array_index (values,3)), ==, 3.5); gimp_value_array_unref (values);
  raw = gimp_filter_layer_ref_definition (filter);
  g_assert_cmpmem (g_bytes_get_data (raw,NULL),g_bytes_get_size (raw),after_data,sizeof after_data); g_bytes_unref (raw);
  g_assert_cmpuint (gimp_filter_layer_get_cache_generation (filter), ==, gimp_filter_layer_get_generation (filter));
  g_object_unref (image); spin_ms (10); g_assert_cmpint (finalized, ==, 2);
}


static void duplicate_failure_releases_partial (void)
{
  GimpImage *image = image_new (8,8);
  GimpFilterLayer *filter = filter_new (image,NULL,8,8);
  GimpItem *copy;
  broken_filter_finalized = 0;
  copy = gimp_item_duplicate (GIMP_ITEM (filter),test_broken_filter_get_type ());
  g_assert_null (copy);
  g_assert_cmpint (broken_filter_finalized, ==, 1);
  g_object_unref (image);
}


static void gaussian_legacy_fixture (void)
{
  GimpImage *image = image_new (9,8);
  GimpLayer *source = source_new (image,NULL,9,8);
  GimpFilterLayer *filter = filter_new (image,NULL,9,8);
  gchar *path = g_build_filename (g_getenv ("GIMP_TESTING_ABS_TOP_SRCDIR"),
                                 "migration/fixtures/legacy-gauss/input-opaque.rgba",NULL);
  gchar *input = NULL, *expected = NULL;
  gsize input_size, expected_size;
  guchar result[9*8*4];
  GimpValueArray *args;
  g_assert_true (g_file_get_contents (path,&input,&input_size,NULL)); g_free (path);
  g_assert_cmpuint (input_size, ==, sizeof result);
  gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (source)),GEGL_RECTANGLE (0,0,9,8),0,
                  babl_format ("R'G'B'A u8"),input,GEGL_AUTO_ROWSTRIDE); g_free (input);
  gimp_drawable_update (GIMP_DRAWABLE (source),0,0,9,8);
  for (gint method = 0; method < 2; ++method)
    {
      args = gimp_value_array_new_from_types (NULL,G_TYPE_INT,1,G_TYPE_INT,123,G_TYPE_INT,456,
                                             G_TYPE_DOUBLE,25.0,G_TYPE_DOUBLE,25.0,G_TYPE_INT,method,G_TYPE_NONE);
      g_assert_true (gimp_filter_layer_set_definition (filter,"plug-in-gauss",NULL,args,NULL));
      gimp_value_array_unref (args); settle (filter);
      path = g_build_filename (g_getenv ("GIMP_TESTING_ABS_TOP_SRCDIR"),"migration/fixtures/legacy-gauss",
                               method ? "gauss-opaque-h25-v25-m1.rgba" : "gauss-opaque-h25-v25-m0.rgba",NULL);
      g_assert_true (g_file_get_contents (path,&expected,&expected_size,NULL)); g_free (path);
      gegl_buffer_get (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)),GEGL_RECTANGLE (0,0,9,8),1.0,
                      babl_format ("R'G'B'A u8"),result,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
      g_assert_cmpmem (result,sizeof result,expected,expected_size); g_free (expected);
    }
  g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 2);
  g_object_unref (image);
}


/* The captured reference bytes have no ICC conversion. Assigning a profile
 * changes their interpretation, never the encoded arithmetic of the old PDB. */
static void native_gaussian_fixture (gboolean adobe, gboolean reassign_running)
{
  GimpImage *image = image_new (9,8);
  GimpColorProfile *profile = adobe ? gimp_color_profile_new_rgb_adobe () : gimp_color_profile_new_rgb_srgb ();
  GimpLayer *source;
  GimpFilterLayer *filter;
  const Babl *native;
  gchar *path, *input = NULL, *expected = NULL;
  gsize input_size, expected_size;
  guchar actual[9*8*4];
  GError *error = NULL;
  g_assert_true (gimp_image_set_color_profile (image,profile,&error)); g_assert_no_error (error);
  g_object_unref (profile);
  native = gimp_image_get_layer_format (image,TRUE);
  source = gimp_layer_new (image,9,8,native,"native encoded source",1,GIMP_LAYER_MODE_NORMAL_LEGACY);
  gimp_image_add_layer (image,source,NULL,0,FALSE);
  path = g_build_filename (g_getenv ("GIMP_TESTING_ABS_TOP_SRCDIR"),
                           "migration/fixtures/legacy-gauss/input-opaque.rgba",NULL);
  g_assert_true (g_file_get_contents (path,&input,&input_size,NULL)); g_free (path);
  g_assert_cmpuint (input_size, ==, sizeof actual);
  gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (source)),GEGL_RECTANGLE (0,0,9,8),0,
                  native,input,GEGL_AUTO_ROWSTRIDE); g_free (input);
  gimp_drawable_update (GIMP_DRAWABLE (source),0,0,9,8);
  filter = filter_new (image,NULL,9,8);
  for (gint method = 0; method < 2; ++method)
    {
      GimpValueArray *args = gimp_value_array_new_from_types (NULL,G_TYPE_INT,1,G_TYPE_INT,123,G_TYPE_INT,456,
        G_TYPE_DOUBLE,25.0,G_TYPE_DOUBLE,25.0,G_TYPE_INT,method,G_TYPE_NONE);
      g_assert_true (gimp_filter_layer_set_definition (filter,"plug-in-gauss",NULL,args,NULL));
      gimp_value_array_unref (args);
      if (reassign_running)
        {
          guint64 generation;
          gint64 deadline = g_get_monotonic_time () + G_TIME_SPAN_SECOND * 20;
          while (gimp_filter_layer_get_state (filter) != GIMP_FILTER_LAYER_RUNNING && g_get_monotonic_time () < deadline)
            { g_main_context_iteration (NULL,FALSE); g_usleep (100); }
          g_assert_cmpint (gimp_filter_layer_get_state (filter), ==, GIMP_FILTER_LAYER_RUNNING);
          generation = gimp_filter_layer_get_generation (filter);
          profile = method ? gimp_color_profile_new_rgb_srgb () : gimp_color_profile_new_rgb_adobe ();
          g_assert_true (gimp_image_assign_color_profile (image,profile,NULL,&error)); g_assert_no_error (error);
          g_object_unref (profile);
          g_assert_cmpuint (gimp_filter_layer_get_generation (filter), >, generation);
          g_assert_cmpuint (gimp_filter_layer_get_cache_generation (filter), !=, gimp_filter_layer_get_generation (filter));
        }
      settle (filter);
      native = gimp_drawable_get_format (GIMP_DRAWABLE (filter));
      g_assert_true (native == gimp_image_get_layer_format (image,TRUE));
      path = g_build_filename (g_getenv ("GIMP_TESTING_ABS_TOP_SRCDIR"),"migration/fixtures/legacy-gauss",
        method ? "gauss-opaque-h25-v25-m1.rgba" : "gauss-opaque-h25-v25-m0.rgba",NULL);
      g_assert_true (g_file_get_contents (path,&expected,&expected_size,NULL)); g_free (path);
      gegl_buffer_get (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)),GEGL_RECTANGLE (0,0,9,8),1.0,
                      native,actual,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
      g_assert_cmpmem (actual,sizeof actual,expected,expected_size); g_free (expected);
    }
  g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, reassign_running ? 4 : 2);
  g_object_unref (image);
}
static void gaussian_native_srgb (void) { native_gaussian_fixture (FALSE,FALSE); }
static void gaussian_native_adobe (void) { native_gaussian_fixture (TRUE,FALSE); }
static void profile_reassignment_discards_worker (void) { native_gaussian_fixture (FALSE,TRUE); }

static void unsupported_precision_retains_cache (void)
{
  static const struct { GimpImageBaseType base; GimpPrecision precision; } cases[] = {
    {GIMP_RGB,GIMP_PRECISION_FLOAT_LINEAR}, {GIMP_RGB,GIMP_PRECISION_U16_NON_LINEAR},
    {GIMP_RGB,GIMP_PRECISION_U8_LINEAR}, {GIMP_GRAY,GIMP_PRECISION_U8_NON_LINEAR}
  };
  static const guchar original[] = {0,255,12,27,36};
  for (guint i = 0; i < G_N_ELEMENTS (cases); ++i)
    {
      GimpImage *image = gimp_image_new (gimp,8,8,cases[i].base,cases[i].precision);
      GimpFilterLayer *filter = filter_new (image,NULL,8,8);
      GimpValueArray *args = edge_args ();
      GBytes *raw = g_bytes_new_static (original,sizeof original), *saved;
      GeglBuffer *cache = gimp_drawable_get_buffer (GIMP_DRAWABLE (filter));
      gchar *message;
      g_object_ref (cache);
      g_assert_true (gimp_filter_layer_set_definition (filter,"plug-in-edge",raw,args,NULL));
      g_bytes_unref (raw); gimp_value_array_unref (args);
      gimp_filter_layer_mark_as_loaded (filter); spin_ms (5);
      g_assert_cmpint (gimp_filter_layer_get_state (filter), ==, GIMP_FILTER_LAYER_CLEAN);
      gimp_filter_layer_invalidate (filter); spin_ms (10);
      g_assert_cmpint (gimp_filter_layer_get_state (filter), ==, GIMP_FILTER_LAYER_FAILED);
      g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 0);
      g_assert_true (cache == gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)));
      message = gimp_filter_layer_dup_error (filter);
      g_assert_nonnull (strstr (message,"requires non-linear RGB U8")); g_free (message);
      saved = gimp_filter_layer_ref_definition (filter);
      g_assert_cmpmem (g_bytes_get_data (saved,NULL),g_bytes_get_size (saved),original,sizeof original);
      g_bytes_unref (saved); g_object_unref (cache); g_object_unref (image);
    }
}

static void small_image_finishes_during_large_preparation (void)
{
  GimpImage *large = image_new (2048,1536), *small = image_new (16,16);
  GimpLayer *large_source = source_new (large,NULL,2048,1536), *small_source = source_new (small,NULL,16,16);
  GimpFilterLayer *large_filter, *small_filter;
  fill (large_source,255,0,0,255); fill (small_source,0,255,0,255);
  large_filter = filter_new (large,NULL,2048,1536); small_filter = filter_new (small,NULL,16,16);
  settle (small_filter);
  g_assert_cmpuint (gimp_filter_layer_get_run_count (large_filter), ==, 0);
  g_assert_cmpint (gimp_filter_layer_get_state (large_filter), ==, GIMP_FILTER_LAYER_PREPARING);
  pixel (GIMP_LAYER (small_filter),0,0,0,255);
  settle (large_filter); pixel (GIMP_LAYER (large_filter),0,0,0,255);
  g_object_unref (large); g_object_unref (small);
}
static void image_close_during_worker (void)
{
  gint finalized = 0;
  GimpImage *image = image_new (1024,1024);
  GimpLayer *source = source_new (image,NULL,1024,1024);
  GimpFilterLayer *filter = filter_new (image,NULL,1024,1024);
  GimpValueArray *args = gimp_value_array_new_from_types (NULL,G_TYPE_INT,1,G_TYPE_INT,123,G_TYPE_INT,456,
    G_TYPE_DOUBLE,25.0,G_TYPE_DOUBLE,25.0,G_TYPE_INT,0,G_TYPE_NONE);
  gint64 deadline = g_get_monotonic_time () + G_TIME_SPAN_SECOND * 20, start;
  fill (source,50,100,150,255);
  gimp_filter_layer_set_definition (filter,"plug-in-gauss",NULL,args,NULL); gimp_value_array_unref (args);
  g_object_weak_ref (G_OBJECT (image),weak_finalized,&finalized);
  g_object_weak_ref (G_OBJECT (source),weak_finalized,&finalized);
  g_object_weak_ref (G_OBJECT (filter),weak_finalized,&finalized);
  while (gimp_filter_layer_get_state (filter) != GIMP_FILTER_LAYER_RUNNING && g_get_monotonic_time () < deadline)
    { g_main_context_iteration (NULL,FALSE); g_usleep (100); }
  g_assert_cmpint (gimp_filter_layer_get_state (filter), ==, GIMP_FILTER_LAYER_RUNNING);
  start = g_get_monotonic_time ();
  g_object_unref (image);
  g_assert_cmpint (g_get_monotonic_time () - start, <, 100000);
  g_assert_cmpint (finalized, ==, 3);
  spin_ms (30); /* lets the detached, cancelled, UI-free worker release bytes */
}
typedef struct { GimpLayer *source; guint updates; } Editing;
static gboolean edit_lower_repeatedly (gpointer data)
{
  Editing *editing = data;
  GeglColor *color = gegl_color_new (NULL);
  guchar pixel[] = {++editing->updates,0,0,255};
  gegl_color_set_pixel (color,babl_format ("R'G'B'A u8"),pixel);
  gegl_buffer_set_color (gimp_drawable_get_buffer (GIMP_DRAWABLE (editing->source)),GEGL_RECTANGLE (0,0,128,512),color);
  g_object_unref (color); gimp_drawable_update (GIMP_DRAWABLE (editing->source),0,0,128,512);
  return editing->updates < 20;
}
static void sustained_edits_converge (void)
{
  GimpImage *image = image_new (256,512);
  GimpLayer *source = source_new (image,NULL,256,512);
  GimpFilterLayer *filter = filter_new (image,NULL,256,512);
  Editing editing = {source,0};
  guchar value[4];
  guint timer;
  gint64 deadline = g_get_monotonic_time () + G_TIME_SPAN_SECOND * 20;
  fill (source,0,0,0,255);
  timer = g_timeout_add (3,edit_lower_repeatedly,&editing);
  while ((editing.updates < 20 || gimp_filter_layer_get_state (filter) != GIMP_FILTER_LAYER_CLEAN) &&
         g_get_monotonic_time () < deadline)
    { g_main_context_iteration (NULL,FALSE); g_usleep (100); }
  if (editing.updates < 20) g_source_remove (timer);
  g_assert_cmpuint (editing.updates, ==, 20);
  g_assert_cmpint (gimp_filter_layer_get_state (filter), ==, GIMP_FILTER_LAYER_CLEAN);
  gegl_buffer_get (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)),GEGL_RECTANGLE (127,256,1,1),1.0,
                  babl_format ("R'G'B'A u8"),value,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
  /* Legacy Sobel: trunc(sqrt((4*20)^2 * amount2)) = 113. */
  g_assert_cmpint (value[0], ==, 113); g_assert_cmpint (value[3], ==, 255);
  g_assert_cmpuint (gimp_filter_layer_get_cache_generation (filter), ==, gimp_filter_layer_get_generation (filter));
  g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), <, 20);
  g_object_unref (image);
}
static void oversized_execution_preserves_definition (void)
{
  GimpImage *image = image_new (8193,8193);
  GimpFilterLayer *filter = filter_new (image,NULL,8193,8193);
  gchar *error, *name;
  spin_ms (15);
  g_assert_cmpint (gimp_filter_layer_get_state (filter), ==, GIMP_FILTER_LAYER_FAILED);
  g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 0);
  error = gimp_filter_layer_dup_error (filter); g_assert_nonnull (strstr (error,"bounded raster size")); g_free (error);
  name = gimp_filter_layer_dup_procedure (filter); g_assert_cmpstr (name, ==, "plug-in-edge"); g_free (name);
  g_object_unref (image);
}


static void saved_snapshot_generation_restore (void)
{
  GimpImage *image = image_new (8,8);
  GimpLayer *source = source_new (image,NULL,8,8);
  GimpFilterLayer *filter = filter_new (image,NULL,8,8);
  GimpFilterLayerSnapshot saved = {1,42,41,TRUE,GIMP_FILTER_LAYER_RUNNING}, observed;
  GError *error = NULL;
  guint64 restored_generation, valid_generation;
  fill (source,255,0,0,255); fill (GIMP_LAYER (filter),77,88,99,255);
  g_assert_true (gimp_filter_layer_restore_snapshot_state (filter,&saved,&error)); g_assert_no_error (error);
  g_assert_true (gimp_filter_layer_get_snapshot_state (filter,&observed));
  restored_generation = observed.generation;
  g_assert_cmpuint (observed.generation, !=, observed.cache_generation);
  g_assert_cmpint (observed.state, ==, GIMP_FILTER_LAYER_WAITING); pixel (GIMP_LAYER (filter),77,88,99,255);
  settle (filter); pixel (GIMP_LAYER (filter),0,0,0,255);
  g_assert_cmpuint (gimp_filter_layer_get_cache_generation (filter), ==, restored_generation);
  fill (GIMP_LAYER (filter),11,22,33,255);
  saved.generation = saved.cache_generation = 100;
  g_assert_true (gimp_filter_layer_restore_snapshot_state (filter,&saved,NULL)); spin_ms (15);
  pixel (GIMP_LAYER (filter),11,22,33,255); g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 1);
  saved.cache_complete = FALSE;
  g_assert_true (gimp_filter_layer_restore_snapshot_state (filter,&saved,NULL)); settle (filter);
  pixel (GIMP_LAYER (filter),0,0,0,255); g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, 2);
  g_assert_true (gimp_filter_layer_get_snapshot_state (filter,&observed)); g_assert_true (observed.cache_complete);
  valid_generation = gimp_filter_layer_get_generation (filter);
  saved.cache_generation = 101;
  g_assert_false (gimp_filter_layer_restore_snapshot_state (filter,&saved,&error));
  g_assert_error (error,GIMP_PAINTER_ERROR,GIMP_PAINTER_ERROR_INVALID_STATE); g_clear_error (&error);
  g_assert_cmpuint (gimp_filter_layer_get_generation (filter), ==, valid_generation);
  saved.generation = G_MAXUINT64; saved.cache_generation = 0;
  g_assert_false (gimp_filter_layer_restore_snapshot_state (filter,&saved,&error));
  g_assert_error (error,GIMP_PAINTER_ERROR,GIMP_PAINTER_ERROR_INVALID_STATE); g_clear_error (&error);
  gimp_filter_layer_invalidate (filter); g_assert_cmpuint (gimp_filter_layer_get_generation (filter), ==, valid_generation + 1);
  saved.generation = saved.cache_generation = 0; saved.cache_complete = TRUE;
  g_assert_true (gimp_filter_layer_restore_snapshot_state (filter,&saved,NULL));
  g_assert_cmpuint (gimp_filter_layer_get_generation (filter), ==, gimp_filter_layer_get_cache_generation (filter));
  g_assert_cmpint (gimp_filter_layer_get_state (filter), ==, GIMP_FILTER_LAYER_CLEAN);
  saved.generation = saved.cache_generation = G_MAXINT64;
  g_assert_true (gimp_filter_layer_restore_snapshot_state (filter,&saved,NULL));
  gimp_filter_layer_invalidate (filter);
  g_assert_true (gimp_filter_layer_get_snapshot_state (filter,&observed));
  g_assert_cmpuint (observed.generation, <, G_MAXINT64);
  g_assert_true (gimp_filter_layer_restore_snapshot_state (filter,&observed,NULL));
  settle (filter);
  g_object_unref (image);
}


static void typed_argument_snapshot_survives_expiration (void)
{
  gint finalized = 0;
  GimpImage *image = image_new (8,8);
  GimpLayer *source = source_new (image,NULL,8,8);
  GimpFilterLayer *filter = filter_new (image,NULL,8,8);
  GObject *object = g_object_new (G_TYPE_OBJECT,NULL);
  GObject *objects[] = {G_OBJECT (image),object,NULL}, *empty_objects[] = {NULL};
  GimpValueArray *nested = gimp_value_array_new_from_types (NULL,GIMP_TYPE_DRAWABLE,source,G_TYPE_STRING,"retained",G_TYPE_NONE);
  GimpValueArray *empty = gimp_value_array_new (0);
  GimpValueArray *args = gimp_value_array_new_from_types (NULL,
    G_TYPE_INT,-123, G_TYPE_STRING,NULL, GIMP_TYPE_CORE_OBJECT_ARRAY,objects,
    GIMP_TYPE_VALUE_ARRAY,nested, GIMP_TYPE_CORE_OBJECT_ARRAY,NULL,
    GIMP_TYPE_CORE_OBJECT_ARRAY,empty_objects, GIMP_TYPE_VALUE_ARRAY,empty,
    GIMP_TYPE_VALUE_ARRAY,NULL, G_TYPE_OBJECT,NULL, G_TYPE_NONE);
  GimpFilterArgumentsSnapshot *snapshot, *child, *empty_child;
  GimpFilterArgumentReference reference;
  GValue value = G_VALUE_INIT;
  gint64 source_id = gimp_item_get_id (GIMP_ITEM (source));
  g_object_weak_ref (object,weak_finalized,&finalized);
  g_object_weak_ref (G_OBJECT (image),weak_finalized,&finalized);
  g_object_weak_ref (G_OBJECT (source),weak_finalized,&finalized);
  g_object_weak_ref (G_OBJECT (filter),weak_finalized,&finalized);
  g_assert_true (gimp_filter_layer_set_definition (filter,"future-procedure",NULL,args,NULL));
  gimp_value_array_unref (args); gimp_value_array_unref (nested); gimp_value_array_unref (empty);
  snapshot = gimp_filter_layer_snapshot_arguments (filter);
  g_assert_nonnull (snapshot); g_assert_cmpuint (gimp_filter_arguments_snapshot_count (snapshot), ==, 9);
  g_assert_true (gimp_filter_arguments_snapshot_value (snapshot,0,&value));
  g_assert_cmpint (g_value_get_int (&value), ==, -123); g_value_unset (&value);
  g_assert_true (gimp_filter_arguments_snapshot_is_null (snapshot,1));
  g_assert_cmpuint (gimp_filter_arguments_snapshot_type (snapshot,2), ==, GIMP_TYPE_CORE_OBJECT_ARRAY);
  g_assert_cmpuint (gimp_filter_arguments_snapshot_reference_count (snapshot,2), ==, 2);
  g_assert_false (gimp_filter_arguments_snapshot_value (snapshot,2,&value));
  g_assert_false (G_IS_VALUE (&value));
  g_assert_true (gimp_filter_arguments_snapshot_is_null (snapshot,4));
  g_assert_false (gimp_filter_arguments_snapshot_is_null (snapshot,5));
  child = gimp_filter_arguments_snapshot_nested (snapshot,3); g_assert_nonnull (child);
  empty_child = gimp_filter_arguments_snapshot_nested (snapshot,6); g_assert_nonnull (empty_child);
  g_assert_cmpuint (gimp_filter_arguments_snapshot_count (empty_child), ==, 0);
  gimp_filter_arguments_snapshot_free (empty_child);
  g_assert_null (gimp_filter_arguments_snapshot_nested (snapshot,7));
  g_assert_true (gimp_filter_arguments_snapshot_is_null (snapshot,8));
  define_edge (filter); /* snapshots keep the original immutable argument model */
  g_object_unref (object); g_object_unref (image);
  g_assert_cmpint (finalized, ==, 4);
  g_assert_true (gimp_filter_arguments_snapshot_reference (snapshot,2,1,&reference));
  g_assert_true (reference.was_set); g_assert_true (reference.expired);
  g_assert_cmpuint (reference.object_type, ==, G_TYPE_OBJECT);
  gimp_filter_arguments_snapshot_free (snapshot);
  g_assert_true (gimp_filter_arguments_snapshot_reference (child,0,0,&reference));
  g_assert_true (reference.expired); g_assert_cmpint (reference.id, ==, source_id);
  g_assert_true (gimp_filter_arguments_snapshot_value (child,1,&value));
  g_assert_cmpstr (g_value_get_string (&value), ==, "retained"); g_value_unset (&value);
  gimp_filter_arguments_snapshot_free (child);
}


static void typed_argument_import_preserves_descriptors (void)
{
  gint finalized = 0;
  GimpImage *image = image_new (8,8);
  GimpFilterLayer *filter = filter_new (image,NULL,8,8);
  GValue number = G_VALUE_INIT, text = G_VALUE_INIT, observed = G_VALUE_INIT;
  GimpFilterArgumentReference refs[] = {{GIMP_TYPE_IMAGE,999,TRUE,FALSE},{G_TYPE_OBJECT,123,TRUE,TRUE}};
  GimpFilterArgumentReference nested_ref = {GIMP_TYPE_LAYER,554,TRUE,TRUE};
  GimpFilterArgumentReference null_ref = {G_TYPE_OBJECT,0,FALSE,FALSE}, reference;
  GObject *targets[] = {G_OBJECT (image),NULL};
  GimpFilterArgumentSpec nested[2] = {{0}}, specs[8] = {{0}};
  GimpFilterArgumentsSnapshot *snapshot, *copy, *child;
  GError *error = NULL;
  g_value_init (&number,G_TYPE_INT); g_value_set_int (&number,7);
  g_value_init (&text,G_TYPE_STRING); g_value_set_string (&text,"retained import");
  nested[0].value_type = GIMP_TYPE_DRAWABLE; nested[0].n_references = 1; nested[0].references = &nested_ref;
  nested[1].value_type = G_TYPE_STRING; nested[1].value = &text;
  specs[0].value_type = G_TYPE_INT; specs[0].value = &number;
  specs[1].value_type = GIMP_TYPE_CORE_OBJECT_ARRAY; specs[1].n_references = 2; specs[1].references = refs; specs[1].targets = targets;
  specs[2].value_type = GIMP_TYPE_VALUE_ARRAY; specs[2].n_children = 2; specs[2].children = nested;
  specs[3].value_type = G_TYPE_OBJECT; specs[3].is_null = TRUE; specs[3].n_references = 1; specs[3].references = &null_ref;
  specs[4].value_type = GIMP_TYPE_CORE_OBJECT_ARRAY; specs[4].is_null = TRUE;
  specs[5].value_type = GIMP_TYPE_CORE_OBJECT_ARRAY;
  specs[6].value_type = GIMP_TYPE_VALUE_ARRAY;
  specs[7].value_type = GIMP_TYPE_VALUE_ARRAY; specs[7].is_null = TRUE;
  snapshot = gimp_filter_arguments_snapshot_import (8,specs,&error); g_assert_no_error (error); g_assert_nonnull (snapshot);
  g_value_set_int (&number,99); g_value_unset (&text); refs[0].id = 1000;
  g_assert_true (gimp_filter_layer_set_definition_with_snapshot (filter,"future-imported-procedure",NULL,snapshot,&error));
  g_assert_no_error (error); gimp_filter_arguments_snapshot_free (snapshot);
  copy = gimp_filter_layer_snapshot_arguments (filter); g_assert_nonnull (copy);
  g_assert_true (gimp_filter_arguments_snapshot_value (copy,0,&observed));
  g_assert_cmpint (g_value_get_int (&observed), ==, 7); g_value_unset (&observed);
  g_assert_true (gimp_filter_arguments_snapshot_reference (copy,1,0,&reference));
  g_assert_cmpint (reference.id, ==, 999); g_assert_false (reference.expired);
  g_assert_true (gimp_filter_arguments_snapshot_reference (copy,1,1,&reference));
  g_assert_cmpint (reference.id, ==, 123); g_assert_true (reference.expired);
  g_assert_true (gimp_filter_arguments_snapshot_is_null (copy,3));
  g_assert_true (gimp_filter_arguments_snapshot_is_null (copy,4));
  g_assert_false (gimp_filter_arguments_snapshot_is_null (copy,5));
  child = gimp_filter_arguments_snapshot_nested (copy,2); g_assert_nonnull (child);
  g_assert_true (gimp_filter_arguments_snapshot_reference (child,0,0,&reference));
  g_assert_cmpint (reference.id, ==, 554); g_assert_true (reference.expired);
  g_assert_true (gimp_filter_arguments_snapshot_value (child,1,&observed));
  g_assert_cmpstr (g_value_get_string (&observed), ==, "retained import"); g_value_unset (&observed);
  gimp_filter_arguments_snapshot_free (child);
  g_assert_null (gimp_filter_layer_dup_args (filter)); /* expired array is still represented by copy */
  g_object_weak_ref (G_OBJECT (image),weak_finalized,&finalized);
  g_object_weak_ref (G_OBJECT (filter),weak_finalized,&finalized);
  g_object_unref (image); g_assert_cmpint (finalized, ==, 2);
  g_assert_true (gimp_filter_arguments_snapshot_reference (copy,1,0,&reference)); g_assert_true (reference.expired);
  gimp_filter_arguments_snapshot_free (copy); g_value_unset (&number);
}
static void argument_import_validation (void)
{
  GimpFilterArgumentSpec spec = {0}, chain[34] = {{0}};
  GimpFilterArgumentReference reference = {G_TYPE_OBJECT,123,TRUE,TRUE};
  GObject *target = g_object_new (G_TYPE_OBJECT,NULL), *targets[] = {target};
  GValue number = G_VALUE_INIT;
  GError *error = NULL;
  g_value_init (&number,G_TYPE_INT); g_value_set_int (&number,7);
  spec.value_type = G_TYPE_STRING; spec.value = &number;
  g_assert_null (gimp_filter_arguments_snapshot_import (1,&spec,&error));
  g_assert_error (error,GIMP_PAINTER_ERROR,GIMP_PAINTER_ERROR_WRONG_TYPE); g_clear_error (&error);
  spec.value_type = G_TYPE_OBJECT; spec.value = NULL; spec.n_references = 1; spec.references = &reference; spec.targets = targets;
  g_assert_null (gimp_filter_arguments_snapshot_import (1,&spec,&error));
  g_assert_error (error,GIMP_PAINTER_ERROR,GIMP_PAINTER_ERROR_INVALID_STATE); g_clear_error (&error);
  g_assert_null (gimp_filter_arguments_snapshot_import (65537,&spec,&error)); g_assert_nonnull (error); g_clear_error (&error);
  for (gint i = 0; i < 33; ++i)
    { chain[i].value_type = GIMP_TYPE_VALUE_ARRAY; chain[i].n_children = 1; chain[i].children = &chain[i+1]; }
  chain[33].value_type = G_TYPE_INT; chain[33].value = &number;
  g_assert_null (gimp_filter_arguments_snapshot_import (1,chain,&error)); g_assert_nonnull (error); g_clear_error (&error);
  g_value_unset (&number); g_object_unref (target);
}


static void argument_value_dag_is_bounded (void)
{
  GimpImage *image = image_new (8,8);
  GimpFilterLayer *filter = filter_new (image,NULL,8,8);
  GimpValueArray *values = gimp_value_array_new_from_types (NULL,G_TYPE_INT,1,G_TYPE_NONE);
  GError *error = NULL;
  gchar *procedure;
  for (gint i = 0; i < 20; ++i)
    {
      GimpValueArray *parent = gimp_value_array_new_from_types (NULL,GIMP_TYPE_VALUE_ARRAY,values,GIMP_TYPE_VALUE_ARRAY,values,G_TYPE_NONE);
      gimp_value_array_unref (values); values = parent;
    }
  g_assert_false (gimp_filter_layer_set_definition (filter,"malicious-dag",NULL,values,&error));
  g_assert_error (error,GIMP_PAINTER_ERROR,GIMP_PAINTER_ERROR_INVALID_STATE); g_clear_error (&error);
  procedure = gimp_filter_layer_dup_procedure (filter); g_assert_cmpstr (procedure, ==, "plug-in-edge"); g_free (procedure);
  gimp_value_array_unref (values); g_object_unref (image);
}


static void count_update (GimpDrawable *drawable, gint x, gint y, gint width, gint height, gpointer data)
{ ++*(guint *) data; }
static void retained_handle_after_image_close (void)
{
  for (gint running = 0; running < 2; ++running)
    {
      gint finalized = 0;
      guint updates = 0, after_close;
      GimpImage *image = image_new (1024,1024);
      GimpLayer *source = source_new (image,NULL,1024,1024);
      GimpFilterLayer *filter = filter_new (image,NULL,1024,1024);
      guint64 cache_generation, starts;
      gint64 deadline = g_get_monotonic_time () + G_TIME_SPAN_SECOND * 20;
      g_object_ref (filter); /* independent public owning handle outlives image */
      g_object_weak_ref (G_OBJECT (image),weak_finalized,&finalized);
      g_object_weak_ref (G_OBJECT (source),weak_finalized,&finalized);
      g_object_weak_ref (G_OBJECT (filter),weak_finalized,&finalized);
      fill (GIMP_LAYER (filter),11,22,33,255); gimp_filter_layer_mark_as_loaded (filter);
      fill (source,255,0,0,255);
      if (running)
        {
          while (gimp_filter_layer_get_state (filter) != GIMP_FILTER_LAYER_RUNNING && g_get_monotonic_time () < deadline)
            { g_main_context_iteration (NULL,FALSE); g_usleep (100); }
          g_assert_cmpint (gimp_filter_layer_get_state (filter), ==, GIMP_FILTER_LAYER_RUNNING);
        }
      cache_generation = gimp_filter_layer_get_cache_generation (filter);
      starts = gimp_filter_layer_get_run_count (filter);
      g_signal_connect (filter,"update",G_CALLBACK (count_update),&updates);
      g_object_unref (image);
      g_assert_cmpint (finalized, ==, 2);
      g_assert_null (gimp_item_get_image (GIMP_ITEM (filter)));
      g_assert_cmpint (gimp_filter_layer_get_state (filter), ==, GIMP_FILTER_LAYER_CLOSED);
      after_close = updates;
      spin_ms (50);
      g_assert_cmpuint (updates, ==, after_close);
      g_assert_cmpuint (gimp_filter_layer_get_cache_generation (filter), ==, cache_generation);
      g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, starts);
      pixel (GIMP_LAYER (filter),11,22,33,255);
      g_object_unref (filter); g_assert_cmpint (finalized, ==, 3);
    }
}


typedef struct { GimpImage *image; gboolean closed; guint late_updates; } CloseDuringCommit;
static void close_image_on_buffer_notify (GObject *object, GParamSpec *spec, gpointer data)
{
  CloseDuringCommit *state = data;
  if (state->image)
    {
      GimpImage *image = state->image; state->image = NULL;
      g_object_unref (image); state->closed = TRUE;
    }
}
static void count_late_update (GimpDrawable *drawable, gint x, gint y, gint width, gint height, gpointer data)
{ CloseDuringCommit *state = data; if (state->closed) ++state->late_updates; }
static void image_close_reentry_during_commit (void)
{
  GimpImage *image = image_new (16,16);
  GimpLayer *source = source_new (image,NULL,16,16);
  GimpFilterLayer *filter = filter_new (image,NULL,16,16);
  CloseDuringCommit state = {image,FALSE,0};
  gint64 deadline = g_get_monotonic_time () + G_TIME_SPAN_SECOND * 10;
  g_object_ref (filter);
  fill (source,255,0,0,255);
  g_signal_connect (filter,"notify::buffer",G_CALLBACK (close_image_on_buffer_notify),&state);
  g_signal_connect (filter,"update",G_CALLBACK (count_late_update),&state);
  while (!state.closed && g_get_monotonic_time () < deadline)
    { g_main_context_iteration (NULL,FALSE); g_usleep (100); }
  g_assert_true (state.closed); g_assert_null (gimp_item_get_image (GIMP_ITEM (filter)));
  g_assert_cmpint (gimp_filter_layer_get_state (filter), ==, GIMP_FILTER_LAYER_CLOSED);
  spin_ms (30); g_assert_cmpuint (state.late_updates, ==, 0);
  pixel (GIMP_LAYER (filter),0,0,0,255); /* complete swap preceded the close */
  g_object_unref (filter);
}

static void close_image_on_flush (GimpImage *image, gboolean invalidate_preview, gpointer data)
{
  CloseDuringCommit *state = data;
  if (state->image)
    {
      GimpImage *image = state->image; state->image = NULL; g_object_unref (image);
      /* Completion holds the image only through this signal call. Observe
       * actual disconnect below, not merely this last outside-ref release. */
    }
}
static void image_disconnected_during_flush (GimpImage *image, gpointer data)
{ ((CloseDuringCommit *) data)->closed = TRUE; }
static void count_late_filter_status (GimpFilterLayer *filter, gpointer data)
{ CloseDuringCommit *state = data; if (state->closed) ++state->late_updates; }
static void image_close_reentry_during_completion_flush (void)
{
  GimpImage *image = image_new (16,16);
  GimpLayer *source = source_new (image,NULL,16,16);
  GimpFilterLayer *filter = filter_new (image,NULL,16,16);
  CloseDuringCommit state = {image,FALSE,0};
  gint64 deadline = g_get_monotonic_time () + G_TIME_SPAN_SECOND * 10;
  g_object_ref (filter); fill (source,255,0,0,255);
  g_signal_connect (image,"flush",G_CALLBACK (close_image_on_flush),&state);
  g_signal_connect (image,"disconnect",G_CALLBACK (image_disconnected_during_flush),&state);
  g_signal_connect (filter,"update",G_CALLBACK (count_late_update),&state);
  g_signal_connect (filter,"filter-state-changed",G_CALLBACK (count_late_filter_status),&state);
  while (!state.closed && g_get_monotonic_time () < deadline)
    { g_main_context_iteration (NULL,FALSE); g_usleep (100); }
  g_assert_true (state.closed); g_assert_null (gimp_item_get_image (GIMP_ITEM (filter)));
  g_assert_cmpint (gimp_filter_layer_get_state (filter), ==, GIMP_FILTER_LAYER_CLOSED);
  spin_ms (30); g_assert_cmpuint (state.late_updates, ==, 0);
  pixel (GIMP_LAYER (filter),0,0,0,255); g_object_unref (filter);
}

int main (int argc, char **argv)
{
  int result;
  g_test_init (&argc,&argv,NULL);
  gimp_test_utils_set_gimp3_directory ("GIMP_TESTING_ABS_TOP_SRCDIR","app/tests/gimpdir");
  gimp = gimp_init_for_testing ();
#define ADD(name) g_test_add_func ("/gimp-filter-layer/" #name,name)
  ADD (image_close_reentry_during_completion_flush); ADD (image_close_reentry_during_commit); ADD (retained_handle_after_image_close); ADD (argument_value_dag_is_bounded); ADD (typed_argument_import_preserves_descriptors); ADD (argument_import_validation);
  ADD (typed_argument_snapshot_survives_expiration); ADD (saved_snapshot_generation_restore); ADD (small_image_finishes_during_large_preparation); ADD (image_close_during_worker);
  ADD (sustained_edits_converge); ADD (oversized_execution_preserves_definition);
  ADD (duplicate_preserves_cache_freshness); ADD (opaque_arguments_duplicate_and_undo); ADD (opaque_arguments_never_execute); ADD (empty_opaque_arguments_are_distinct);
  ADD (retired_definition_payload_reentry); ADD (duplicate_reentry_cannot_certify_new_definition);
  ADD (definition_edit_stops_after_undo_close); ADD (definition_edit_preserves_reentered_install); ADD (definition_notifications_stop_after_close);
  ADD (pending_cycle_resolved_during_graph_read_is_discarded); ADD (definition_revision_separates_cache_updates);
  ADD (dependency_walk_does_not_resolve_pending_names); ADD (changed_dependency_invalidation_can_close_owner);
  ADD (completion_flushes_image_projection); ADD (long_chain_coalesces_state_notifications); ADD (complex_graph_remains_responsive); ADD (cached_graph_tracks_clone_reassignment);
  ADD (clone_filter_dependency_order); ADD (cached_dependency_close_before_start); ADD (cross_image_filter_cycle_has_no_signal_loop);
  ADD (gaussian_native_srgb); ADD (gaussian_native_adobe); ADD (profile_reassignment_discards_worker); ADD (unsupported_precision_retains_cache);
  ADD (gaussian_legacy_fixture); ADD (duplicate_failure_releases_partial); ADD (definition_undo_redo); ADD (hidden_filter_and_offset); ADD (cpp_header_layout); ADD (clone_filter_dependency_cycle); ADD (main_context_remains_responsive);
  ADD (object_arguments_do_not_cycle); ADD (expired_object_records_and_reassignment);
  ADD (object_array_arguments_do_not_dangle); ADD (removal_and_undo); ADD (lower_group_failure_and_recovery);
  ADD (independent_type_and_edge); ADD (automatic_updates_and_self_exclusion);
  ADD (saved_definition_is_separate); ADD (loaded_cache); ADD (dependency_order);
  ADD (group_scope); ADD (completed_buffer_only); ADD (close_during_preparation); ADD (duplicate_keeps_definition);
  result = g_test_run ();
  gimp_test_utils_set_gimp3_directory ("GIMP_TESTING_ABS_TOP_BUILDDIR","app/tests/gimpdir-output");
  gimp_exit (gimp,TRUE); return result;
}
