/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Actual-C11 coverage of original FilterLayer reads and checked Blinds preparation.
 * These are bounded native contracts, not historical runner/default or
 * procedure-resolution parity. In particular, object-array boxes own their
 * containers only; scalar object GValues retain their referenced objects. */
#include "config.h"
#include <gegl.h>
#include <gtk/gtk.h>
#include <string.h>
#include <math.h>
#include "libgimpbase/gimpbase.h"
#include "libgimpmath/gimpmath.h"
#include "libgimpcolor/gimpcolor.h"
#include "core/core-types.h"
#include "widgets/widgets-types.h"
#include "core/gimp.h"
#include "core/gimpfilterlayer.h"
#include "core/gimpimage.h"
#include "core/gimpimage-undo.h"
#include "core/gimplayer-new.h"
#include "core/gimpundostack.h"
#include "painter/gimp-painter-binding.h"
#include "tests.h"
#include "gimp-app-test-utils.h"

void gimp_test_filter_entry_cpp_register (Gimp *application);
void gimp_test_filter_entry_assert_blinds_provider (Gimp *application);
void gimp_test_filter_entry_provider_rejections (Gimp *application, GimpFilterLayer *filter);
void gimp_test_filter_entry_assert_legacy_rejected (GimpFilterLayer *filter);

static Gimp *gimp;

typedef struct
{
  GimpImage       *image;
  GimpFilterLayer *filter;
} Fixture;

typedef struct
{
  guint64              revision, generation, cache, runs;
  GimpFilterLayerState  state;
  gint                 undo_depth, redo_depth, dirty;
  GBytes              *raw, *opaque, *pixels;
  gchar               *procedure;
  GimpFilterArgumentsSnapshot *arguments;
  gpointer             binding;
} ReadCheckpoint;

static Fixture
fixture_new_size (gint width, gint height)
{
  Fixture result;
  result.image = gimp_image_new (gimp, width, height, GIMP_RGB,
                                GIMP_PRECISION_U8_NON_LINEAR);
  result.filter = GIMP_FILTER_LAYER (gimp_filter_layer_new (
    result.image, width, height, "entry contract", 0.75, GIMP_LAYER_MODE_NORMAL));
  g_assert_nonnull (result.filter);
  g_object_ref_sink (result.filter);
  gimp_image_add_layer (result.image, GIMP_LAYER (result.filter), NULL, 0, FALSE);
  return result;
}

static Fixture
fixture_new (void)
{ return fixture_new_size (7, 9); }

static void
fixture_clear (Fixture *fixture)
{
  g_clear_object (&fixture->image);
  g_clear_object (&fixture->filter);
}

static GValue *
argument (GArray *values,
          guint   index)
{
  g_assert_nonnull (values);
  g_assert_cmpuint (index, <, values->len);
  return &g_array_index (values, GValue, index);
}

static void
append_value (GimpValueArray *array,
              GValue         *value)
{
  gimp_value_array_append (array, value);
  g_value_unset (value);
}

static ReadCheckpoint
checkpoint_new (Fixture *fixture)
{
  ReadCheckpoint result;
  result.revision = gimp_filter_layer_get_definition_revision (fixture->filter);
  result.generation = gimp_filter_layer_get_generation (fixture->filter);
  result.cache = gimp_filter_layer_get_cache_generation (fixture->filter);
  result.runs = gimp_filter_layer_get_run_count (fixture->filter);
  result.state = gimp_filter_layer_get_state (fixture->filter);
  result.undo_depth = gimp_undo_stack_get_depth (gimp_image_get_undo_stack (fixture->image));
  result.redo_depth = gimp_undo_stack_get_depth (gimp_image_get_redo_stack (fixture->image));
  result.dirty = gimp_image_is_dirty (fixture->image);
  result.raw = gimp_filter_layer_ref_definition (fixture->filter);
  result.opaque = gimp_filter_layer_ref_opaque_arguments (fixture->filter);
  result.procedure = gimp_filter_layer_dup_procedure (fixture->filter);
  result.arguments = gimp_filter_layer_snapshot_arguments (fixture->filter);
  {
    const gint width = gimp_item_get_width (GIMP_ITEM (fixture->filter));
    const gint height = gimp_item_get_height (GIMP_ITEM (fixture->filter));
    const gsize size = (gsize) width * height * 4;
    guchar *pixels = g_malloc (size);
    GeglRectangle rect = { 0, 0, width, height };
    gegl_buffer_get (gimp_drawable_get_buffer (GIMP_DRAWABLE (fixture->filter)),
                     &rect, 1.0, babl_format ("R'G'B'A u8"), pixels,
                     GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);
    result.pixels = g_bytes_new_take (pixels, size);
  }
  result.binding = g_object_get_qdata (G_OBJECT (fixture->filter),
                                     g_quark_from_static_string ("gimp-painter-binding-store-v1"));
  g_assert_nonnull (result.binding);
  return result;
}

static void
assert_same_bytes (GBytes *before,
                   GBytes *after)
{
  g_assert_true ((before == NULL) == (after == NULL));
  if (before)
    {
      g_assert_true (before == after);
      g_assert_true (g_bytes_equal (before, after));
    }
}

static void
checkpoint_assert_clear (Fixture        *fixture,
                         ReadCheckpoint *before)
{
  ReadCheckpoint after = checkpoint_new (fixture);
  g_assert_cmpuint (after.revision, ==, before->revision);
  g_assert_cmpuint (after.generation, ==, before->generation);
  g_assert_cmpuint (after.cache, ==, before->cache);
  g_assert_cmpuint (after.runs, ==, before->runs);
  g_assert_cmpint (after.state, ==, before->state);
  g_assert_cmpint (after.undo_depth, ==, before->undo_depth);
  g_assert_cmpint (after.redo_depth, ==, before->redo_depth);
  g_assert_cmpint (after.dirty, ==, before->dirty);
  g_assert_true (after.binding == before->binding);
  assert_same_bytes (before->raw, after.raw);
  assert_same_bytes (before->opaque, after.opaque);
  g_assert_cmpstr (after.procedure, ==, before->procedure);
  g_assert_true (g_bytes_equal (after.pixels, before->pixels));
  g_assert_true ((after.arguments == NULL) == (before->arguments == NULL));
  if (before->arguments)
    {
      guint count = gimp_filter_arguments_snapshot_count (before->arguments);
      g_assert_cmpuint (gimp_filter_arguments_snapshot_count (after.arguments), ==, count);
      for (guint i = 0; i < count; ++i)
        {
          g_assert_true (gimp_filter_arguments_snapshot_type (after.arguments, i) ==
                         gimp_filter_arguments_snapshot_type (before->arguments, i));
          g_assert_true (gimp_filter_arguments_snapshot_peek_value (after.arguments, i) ==
                         gimp_filter_arguments_snapshot_peek_value (before->arguments, i));
        }
    }
  g_clear_pointer (&after.arguments, gimp_filter_arguments_snapshot_free);
  g_clear_pointer (&before->arguments, gimp_filter_arguments_snapshot_free);
  g_clear_pointer (&after.procedure, g_free);
  g_clear_pointer (&before->procedure, g_free);
  g_clear_pointer (&after.pixels, g_bytes_unref);
  g_clear_pointer (&before->pixels, g_bytes_unref);
  g_clear_pointer (&after.raw, g_bytes_unref);
  g_clear_pointer (&after.opaque, g_bytes_unref);
  g_clear_pointer (&before->raw, g_bytes_unref);
  g_clear_pointer (&before->opaque, g_bytes_unref);
}

static void
weak_finalized (gpointer data,
                GObject *object)
{
  (void) object;
  ++*(guint *) data;
}

static void
c_factory_native_identity (void)
{
  GimpImage *image = gimp_image_new (gimp, 17, 19, GIMP_RGB,
                                   GIMP_PRECISION_U8_NON_LINEAR);
  GimpLayer *layer;
  GTypeQuery query;
  guint finalized = 0;
  gboolean failed = TRUE;

  g_assert_true (gimp_filter_layer_get_type () == GIMP_TYPE_FILTER_LAYER);
  g_type_query (GIMP_TYPE_FILTER_LAYER, &query);
  g_assert_cmpuint (query.instance_size, ==, sizeof (GimpFilterLayer));
  g_assert_cmpuint (query.class_size, ==, sizeof (GimpFilterLayerClass));
  g_assert_true (g_type_parent (GIMP_TYPE_FILTER_LAYER) == GIMP_TYPE_LAYER);

  layer = gimp_filter_layer_new (image, 7, 9, "C original factory", 0.375,
                                 GIMP_LAYER_MODE_NORMAL_LEGACY);
  g_assert_nonnull (layer);
  g_assert_true (g_object_is_floating (layer));
  g_assert_true (G_OBJECT_TYPE (layer) == GIMP_TYPE_FILTER_LAYER);
  g_assert_cmpstr (gimp_object_get_name (layer), ==, "C original factory");
  g_assert_cmpint (gimp_item_get_width (GIMP_ITEM (layer)), ==, 7);
  g_assert_cmpint (gimp_item_get_height (GIMP_ITEM (layer)), ==, 9);
  g_assert_cmpfloat (gimp_layer_get_opacity (layer), ==, 0.375);
  g_assert_cmpint (gimp_layer_get_mode (layer), ==, GIMP_LAYER_MODE_NORMAL_LEGACY);
  g_object_get (layer, "binding-failed", &failed, NULL);
  g_assert_false (failed);
  g_assert_nonnull (gimp_drawable_get_buffer (GIMP_DRAWABLE (layer)));
  g_object_weak_ref (G_OBJECT (layer), weak_finalized, &finalized);
  g_object_ref_sink (layer);
  g_assert_false (g_object_is_floating (layer));
  g_object_unref (layer);
  g_assert_cmpuint (finalized, ==, 1);

  /* Native nonpositive-size fallback is recorded, not claimed as old parity. */
  layer = gimp_filter_layer_new (image, 0, -1, "native fallback", 1.0,
                                 GIMP_LAYER_MODE_NORMAL);
  g_assert_true (g_object_is_floating (layer));
  g_object_ref_sink (layer);
  g_assert_cmpint (gimp_item_get_width (GIMP_ITEM (layer)), ==, 17);
  g_assert_cmpint (gimp_item_get_height (GIMP_ITEM (layer)), ==, 19);
  g_object_unref (image);
  g_object_unref (layer);
}

static void
c_exact_name_absent_and_empty (void)
{
  static const guint8 payload[] = { 0, 0xff, 0x81, 0, 0x32, 0xfe };
  static const gchar exact_name[] = "  Unsupported.Mixed_CASE/entry\n";
  Fixture fixture = fixture_new ();
  GBytes *raw = g_bytes_new_static (payload, sizeof payload);
  GimpValueArray *empty = gimp_value_array_new (0);
  const gchar *borrow;
  gchar *owned;
  ReadCheckpoint before;
  GArray *first, *second;

  g_assert_cmpstr (gimp_filter_layer_get_procedure (fixture.filter), ==, "");
  g_assert_null (gimp_filter_layer_get_procedure_args (fixture.filter));
  g_assert_true (gimp_filter_layer_set_definition (fixture.filter, exact_name,
                                                  raw, NULL, NULL));
  borrow = gimp_filter_layer_get_procedure (fixture.filter);
  owned = g_strdup (borrow);
  g_assert_cmpstr (borrow, ==, exact_name);
  before = checkpoint_new (&fixture);
  for (guint i = 0; i < 32; i++)
    {
      gchar *modern = gimp_filter_layer_dup_procedure (fixture.filter);
      g_assert_true (gimp_filter_layer_get_procedure (fixture.filter) == borrow);
      g_assert_cmpstr (modern, ==, exact_name);
      g_assert_true (modern != borrow);
      g_free (modern);
      g_assert_null (gimp_filter_layer_get_procedure_args (fixture.filter));
    }
  checkpoint_assert_clear (&fixture, &before);

  g_assert_true (gimp_filter_layer_set_definition (fixture.filter, "explicit-empty",
                                                  raw, empty, NULL));
  gimp_value_array_unref (empty);
  g_assert_cmpstr (owned, ==, exact_name);
  g_assert_cmpstr (gimp_filter_layer_get_procedure (fixture.filter), ==, "explicit-empty");
  before = checkpoint_new (&fixture);
  first = gimp_filter_layer_get_procedure_args (fixture.filter);
  second = gimp_filter_layer_get_procedure_args (fixture.filter);
  g_assert_nonnull (first);
  g_assert_nonnull (second);
  g_assert_true (first != second);
  g_assert_cmpuint (first->len, ==, 0);
  g_assert_cmpuint (second->len, ==, 0);
  g_array_unref (first);
  g_array_unref (second);
  checkpoint_assert_clear (&fixture, &before);

  g_assert_true (gimp_filter_layer_set_definition (fixture.filter, NULL, raw, NULL, NULL));
  g_assert_cmpstr (gimp_filter_layer_get_procedure (fixture.filter), ==, "");
  g_assert_null (gimp_filter_layer_get_procedure_args (fixture.filter));
  fixture_clear (&fixture);
  g_assert_cmpstr (owned, ==, exact_name);
  g_free (owned);
  g_bytes_unref (raw);
}

static GimpValueArray *
rich_arguments (void)
{
  GimpValueArray *args = gimp_value_array_new (12);
  GimpValueArray *nested;
  GValue value = G_VALUE_INIT;
  const gchar *strings[] = { "alpha", "beta", NULL };
  const gint32 integers[] = { -7, 0, 19 };
  const gdouble reals[] = { 0.25, -3.5 };
  const guint8 bytes[] = { 0, 0xff, 0x31, 0 };

  g_value_init (&value, G_TYPE_STRING);
  g_value_set_string (&value, "saved string");
  append_value (args, &value);
  g_value_init (&value, G_TYPE_INT);
  g_value_set_int (&value, -12345);
  append_value (args, &value);
  g_value_init (&value, G_TYPE_BOOLEAN);
  g_value_set_boolean (&value, TRUE);
  append_value (args, &value);
  g_value_init (&value, G_TYPE_DOUBLE);
  g_value_set_double (&value, 7.25);
  append_value (args, &value);
  g_value_init (&value, G_TYPE_STRV);
  g_value_set_boxed (&value, strings);
  append_value (args, &value);
  g_value_init (&value, GIMP_TYPE_INT32_ARRAY);
  gimp_value_set_int32_array (&value, integers, G_N_ELEMENTS (integers));
  append_value (args, &value);
  g_value_init (&value, GIMP_TYPE_DOUBLE_ARRAY);
  gimp_value_set_double_array (&value, reals, G_N_ELEMENTS (reals));
  append_value (args, &value);
  g_value_init (&value, G_TYPE_BYTES);
  g_value_take_boxed (&value, g_bytes_new (bytes, sizeof bytes));
  append_value (args, &value);
  nested = gimp_value_array_new_from_types (NULL, G_TYPE_STRING, "nested string",
                                           G_TYPE_INT, 41, G_TYPE_NONE);
  g_value_init (&value, GIMP_TYPE_VALUE_ARRAY);
  g_value_take_boxed (&value, nested);
  append_value (args, &value);
  g_value_init (&value, G_TYPE_VARIANT);
  g_value_set_variant (&value, g_variant_new_string ("immutable variant"));
  append_value (args, &value);
  g_value_init (&value, G_TYPE_UINT64);
  g_value_set_uint64 (&value, G_GUINT64_CONSTANT (0xfedcba9876543210));
  append_value (args, &value);
  g_value_init (&value, G_TYPE_STRING);
  append_value (args, &value);
  return args;
}

static void
assert_rich_arguments (GArray *args)
{
  static const guint8 bytes[] = { 0, 0xff, 0x31, 0 };
  GimpValueArray *nested;
  GBytes *boxed;
  gchar **strings;
  const gint32 *integers;
  const gdouble *reals;
  gsize count;
  g_assert_cmpuint (args->len, ==, 12);
  g_assert_cmpstr (g_value_get_string (argument (args, 0)), ==, "saved string");
  g_assert_cmpint (g_value_get_int (argument (args, 1)), ==, -12345);
  g_assert_true (g_value_get_boolean (argument (args, 2)));
  g_assert_cmpfloat (g_value_get_double (argument (args, 3)), ==, 7.25);
  strings = g_value_get_boxed (argument (args, 4));
  g_assert_cmpstr (strings[0], ==, "alpha");
  g_assert_cmpstr (strings[1], ==, "beta");
  g_assert_null (strings[2]);
  integers = gimp_value_get_int32_array (argument (args, 5), &count);
  g_assert_cmpuint (count, ==, 3);
  g_assert_cmpint (integers[0], ==, -7);
  g_assert_cmpint (integers[2], ==, 19);
  reals = gimp_value_get_double_array (argument (args, 6), &count);
  g_assert_cmpuint (count, ==, 2);
  g_assert_cmpfloat (reals[0], ==, 0.25);
  g_assert_cmpfloat (reals[1], ==, -3.5);
  boxed = g_value_get_boxed (argument (args, 7));
  g_assert_cmpmem (g_bytes_get_data (boxed, NULL), g_bytes_get_size (boxed), bytes, sizeof bytes);
  nested = g_value_get_boxed (argument (args, 8));
  g_assert_cmpint (gimp_value_array_length (nested), ==, 2);
  g_assert_cmpstr (g_value_get_string (gimp_value_array_index (nested, 0)), ==, "nested string");
  g_assert_cmpint (g_value_get_int (gimp_value_array_index (nested, 1)), ==, 41);
  g_assert_cmpstr (g_variant_get_string (g_value_get_variant (argument (args, 9)), NULL),
                   ==, "immutable variant");
  g_assert_true (g_value_get_uint64 (argument (args, 10)) == G_GUINT64_CONSTANT (0xfedcba9876543210));
  g_assert_true (G_VALUE_HOLDS_STRING (argument (args, 11)));
  g_assert_null (g_value_get_string (argument (args, 11)));
}

static void
c_owned_values_and_model_identity (void)
{
  /* Both free orders exercise GArray's clear function, including nested boxes. */
  for (guint reverse = 0; reverse < 2; reverse++)
    {
      Fixture fixture = fixture_new ();
      GimpValueArray *input = rich_arguments ();
      GBytes *raw = g_bytes_new_static ("\0raw\xff", 5);
      GimpFilterArgumentsSnapshot *saved, *current, *saved_nested, *current_nested;
      GArray *first, *second, *third;
      ReadCheckpoint before;
      const gchar *name;
      gchar **strings;
      GimpValueArray *nested;

      g_assert_true (gimp_filter_layer_set_definition (fixture.filter, "future-values",
                                                      raw, input, NULL));
      g_value_set_string (gimp_value_array_index (input, 0), "caller changed");
      g_value_set_int (gimp_value_array_index (input, 1), 777);
      gimp_value_array_unref (input);
      saved = gimp_filter_layer_snapshot_arguments (fixture.filter);
      saved_nested = gimp_filter_arguments_snapshot_nested (saved, 8);
      name = gimp_filter_layer_get_procedure (fixture.filter);
      before = checkpoint_new (&fixture);
      first = gimp_filter_layer_get_procedure_args (fixture.filter);
      second = gimp_filter_layer_get_procedure_args (fixture.filter);
      assert_rich_arguments (first);
      assert_rich_arguments (second);
      g_assert_true (first != second);
      g_assert_true (first->data != second->data);
      g_assert_true (g_value_get_string (argument (first, 0)) !=
                     g_value_get_string (argument (second, 0)));
      g_assert_true (g_value_get_boxed (argument (first, 4)) !=
                     g_value_get_boxed (argument (second, 4)));
      g_assert_true (g_value_get_boxed (argument (first, 5)) !=
                     g_value_get_boxed (argument (second, 5)));
      g_assert_true (g_value_get_boxed (argument (first, 8)) !=
                     g_value_get_boxed (argument (second, 8)));

      g_value_set_string (argument (first, 0), "result changed");
      g_value_set_int (argument (first, 1), 888);
      strings = g_value_get_boxed (argument (first, 4));
      strings[0][0] = 'X';
      ((GimpArray *) g_value_get_boxed (argument (first, 5)))->data[0] ^= 1;
      nested = g_value_get_boxed (argument (first, 8));
      g_value_set_string (gimp_value_array_index (nested, 0), "nested changed");
      g_value_set_int (gimp_value_array_index (nested, 1), -1);
      assert_rich_arguments (second);
      third = gimp_filter_layer_get_procedure_args (fixture.filter);
      assert_rich_arguments (third);
      g_array_unref (third);

      current = gimp_filter_layer_snapshot_arguments (fixture.filter);
      current_nested = gimp_filter_arguments_snapshot_nested (current, 8);
      g_assert_true (gimp_filter_arguments_snapshot_peek_value (saved, 0) ==
                     gimp_filter_arguments_snapshot_peek_value (current, 0));
      g_assert_true (gimp_filter_arguments_snapshot_peek_value (saved_nested, 0) ==
                     gimp_filter_arguments_snapshot_peek_value (current_nested, 0));
      g_assert_true (name == gimp_filter_layer_get_procedure (fixture.filter));
      checkpoint_assert_clear (&fixture, &before);
      gimp_filter_arguments_snapshot_free (current_nested);
      gimp_filter_arguments_snapshot_free (saved_nested);
      gimp_filter_arguments_snapshot_free (current);
      gimp_filter_arguments_snapshot_free (saved);

      if (reverse)
        {
          g_array_unref (second);
          fixture_clear (&fixture);
          g_assert_cmpstr (g_value_get_string (argument (first, 0)), ==, "result changed");
          g_array_unref (first);
        }
      else
        {
          g_array_unref (first);
          fixture_clear (&fixture);
          assert_rich_arguments (second);
          g_array_unref (second);
        }
      g_bytes_unref (raw);
    }
}

static void
c_null_and_empty_argument_shapes (void)
{
  Fixture fixture = fixture_new ();
  GimpValueArray *input = gimp_value_array_new (6);
  GValue value = G_VALUE_INIT;
  const gchar *empty_strings[] = { NULL };
  GObject *empty_objects[] = { NULL };
  GArray *result;

  g_value_init (&value, GIMP_TYPE_VALUE_ARRAY);
  append_value (input, &value);
  g_value_init (&value, GIMP_TYPE_VALUE_ARRAY);
  g_value_take_boxed (&value, gimp_value_array_new (0));
  append_value (input, &value);
  g_value_init (&value, G_TYPE_STRV);
  append_value (input, &value);
  g_value_init (&value, G_TYPE_STRV);
  g_value_set_boxed (&value, empty_strings);
  append_value (input, &value);
  g_value_init (&value, GIMP_TYPE_CORE_OBJECT_ARRAY);
  append_value (input, &value);
  g_value_init (&value, GIMP_TYPE_CORE_OBJECT_ARRAY);
  g_value_set_boxed (&value, empty_objects);
  append_value (input, &value);
  g_assert_true (gimp_filter_layer_set_definition (fixture.filter, "nullable-shapes", NULL, input, NULL));
  gimp_value_array_unref (input);
  result = gimp_filter_layer_get_procedure_args (fixture.filter);
  g_assert_cmpuint (result->len, ==, 6);
  for (guint i = 0; i < 6; i += 2)
    {
      g_assert_null (g_value_get_boxed (argument (result, i)));
      g_assert_nonnull (g_value_get_boxed (argument (result, i + 1)));
    }
  g_assert_cmpint (gimp_value_array_length (g_value_get_boxed (argument (result, 1))), ==, 0);
  g_assert_null (((gchar **) g_value_get_boxed (argument (result, 3)))[0]);
  g_assert_null (((GObject **) g_value_get_boxed (argument (result, 5)))[0]);
  fixture_clear (&fixture);
  g_array_unref (result);
}

static void
c_scalar_object_identity_and_ownership (void)
{
  for (guint reverse = 0; reverse < 2; reverse++)
    {
      Fixture fixture = fixture_new ();
      GObject *object = g_object_new (G_TYPE_OBJECT, NULL);
      GimpValueArray *input = gimp_value_array_new_from_types (NULL, G_TYPE_OBJECT,
                                                             object, G_TYPE_NONE);
      GArray *first, *second, *expired;
      GimpFilterArgumentReference descriptor;
      GimpFilterArgumentsSnapshot *snapshot;
      guint finalized = 0;
      g_object_weak_ref (object, weak_finalized, &finalized);
      g_assert_true (gimp_filter_layer_set_definition (fixture.filter, "object-identity", NULL, input, NULL));
      gimp_value_array_unref (input);
      first = gimp_filter_layer_get_procedure_args (fixture.filter);
      second = gimp_filter_layer_get_procedure_args (fixture.filter);
      g_assert_true (g_value_get_object (argument (first, 0)) == object);
      g_assert_true (g_value_get_object (argument (second, 0)) == object);
      g_object_unref (object);
      g_assert_cmpuint (finalized, ==, 0);
      g_array_unref (reverse ? second : first);
      g_assert_cmpuint (finalized, ==, 0);
      g_array_unref (reverse ? first : second);
      g_assert_cmpuint (finalized, ==, 1);
      snapshot = gimp_filter_layer_snapshot_arguments (fixture.filter);
      g_assert_true (gimp_filter_arguments_snapshot_reference (snapshot, 0, 0, &descriptor));
      g_assert_true (descriptor.was_set);
      g_assert_true (descriptor.expired);
      g_assert_true (descriptor.object_type == G_TYPE_OBJECT);
      expired = gimp_filter_layer_get_procedure_args (fixture.filter);
      g_assert_nonnull (expired);
      g_assert_true (G_VALUE_HOLDS_OBJECT (argument (expired, 0)));
      g_assert_null (g_value_get_object (argument (expired, 0)));
      g_array_unref (expired);
      gimp_filter_arguments_snapshot_free (snapshot);
      fixture_clear (&fixture);
    }
}

static void
c_live_object_array_container_copy (void)
{
  Fixture fixture = fixture_new ();
  GObject *object = g_object_new (G_TYPE_OBJECT, NULL);
  GObject *objects[] = { G_OBJECT (fixture.image), object, NULL };
  GimpValueArray *input = gimp_value_array_new_from_types (NULL,
    GIMP_TYPE_CORE_OBJECT_ARRAY, objects, G_TYPE_NONE);
  GArray *first, *second, *third;
  GObject **a, **b;
  guint finalized = 0;
  ReadCheckpoint before;

  g_object_weak_ref (object, weak_finalized, &finalized);
  g_assert_true (gimp_filter_layer_set_definition (fixture.filter, "live-array", NULL, input, NULL));
  gimp_value_array_unref (input);
  before = checkpoint_new (&fixture);
  first = gimp_filter_layer_get_procedure_args (fixture.filter);
  second = gimp_filter_layer_get_procedure_args (fixture.filter);
  a = g_value_get_boxed (argument (first, 0));
  b = g_value_get_boxed (argument (second, 0));
  g_assert_true (a != b);
  g_assert_true (a[0] == G_OBJECT (fixture.image));
  g_assert_true (a[1] == object);
  g_assert_true (b[1] == object);
  a[0] = object;
  g_assert_true (b[0] == G_OBJECT (fixture.image));
  third = gimp_filter_layer_get_procedure_args (fixture.filter);
  g_assert_true (((GObject **) g_value_get_boxed (argument (third, 0)))[0] == G_OBJECT (fixture.image));
  g_array_unref (first);
  g_array_unref (third);
  checkpoint_assert_clear (&fixture, &before);
  g_object_unref (object);
  /* The native boxed array does not own element refs. Never dereference its
   * saved pointers after this point; freeing the container remains safe. */
  g_assert_cmpuint (finalized, ==, 1);
  g_assert_null (gimp_filter_layer_get_procedure_args (fixture.filter));
  g_array_unref (second);
  fixture_clear (&fixture);
}

static void
c_expired_arrays_reject_atomically (void)
{
  /* Failure after a live scalar object must release all temporary prefix
   * copies, including when the rejected array appears inside a nested array. */
  for (guint nested = 0; nested < 2; nested++)
    {
      Fixture fixture = fixture_new ();
      GObject *live = g_object_new (G_TYPE_OBJECT, NULL);
      GObject *dead = g_object_new (G_TYPE_OBJECT, NULL);
      GObject *objects[] = { dead, NULL };
      GimpValueArray *tail = gimp_value_array_new_from_types (NULL,
        GIMP_TYPE_CORE_OBJECT_ARRAY, objects, G_TYPE_NONE);
      GimpValueArray *input = gimp_value_array_new_from_types (NULL,
        G_TYPE_STRING, "prefix", G_TYPE_OBJECT, live, G_TYPE_NONE);
      GValue value = G_VALUE_INIT;
      GimpFilterArgumentsSnapshot *snapshot, *leaf;
      GimpFilterArgumentReference before_reference, after_reference;
      guint finalized = 0;
      ReadCheckpoint before;

      g_object_weak_ref (live, weak_finalized, &finalized);
      g_object_weak_ref (dead, weak_finalized, &finalized);
      if (nested)
        {
          g_value_init (&value, GIMP_TYPE_VALUE_ARRAY);
          g_value_set_boxed (&value, tail);
          append_value (input, &value);
        }
      else
        gimp_value_array_append (input, gimp_value_array_index (tail, 0));
      g_assert_true (gimp_filter_layer_set_definition (fixture.filter, "expired-array", NULL, input, NULL));
      gimp_value_array_unref (input);
      gimp_value_array_unref (tail);
      g_object_unref (dead);
      g_assert_cmpuint (finalized, ==, 1);
      snapshot = gimp_filter_layer_snapshot_arguments (fixture.filter);
      leaf = nested ? gimp_filter_arguments_snapshot_nested (snapshot, 2) : snapshot;
      g_assert_true (gimp_filter_arguments_snapshot_reference (leaf, nested ? 0 : 2,
                                                              0, &before_reference));
      g_assert_true (before_reference.was_set);
      g_assert_true (before_reference.expired);
      before = checkpoint_new (&fixture);
      for (guint i = 0; i < 8; i++)
        g_assert_null (gimp_filter_layer_get_procedure_args (fixture.filter));
      checkpoint_assert_clear (&fixture, &before);
      g_assert_true (gimp_filter_arguments_snapshot_reference (leaf, nested ? 0 : 2,
                                                              0, &after_reference));
      g_assert_true (after_reference.object_type == before_reference.object_type);
      g_assert_true (after_reference.id == before_reference.id);
      g_assert_true (after_reference.was_set == before_reference.was_set);
      g_assert_true (after_reference.expired == before_reference.expired);
      if (nested)
        gimp_filter_arguments_snapshot_free (leaf);
      gimp_filter_arguments_snapshot_free (snapshot);
      g_object_unref (live);
      g_assert_cmpuint (finalized, ==, 2);
      fixture_clear (&fixture);
    }
}

static gpointer
query_wrong_thread (gpointer data)
{
  GimpFilterLayer *filter = data;
  g_assert_null (gimp_filter_layer_get_procedure (filter));
  g_assert_null (gimp_filter_layer_get_procedure_args (filter));
  return NULL;
}

static void
c_boundary_checks_and_closed_metadata (void)
{
  Fixture fixture = fixture_new ();
  GObject *wrong = g_object_new (G_TYPE_OBJECT, NULL);
  GimpValueArray *input = rich_arguments ();
  GArray *before_close, *after_close;
  GThread *thread;
  ReadCheckpoint before;
  const gchar *name;
  gchar *owned_name;
  GQuark key = g_quark_from_static_string ("gimp-painter-binding-store-v1");

  g_assert_null (gimp_filter_layer_get_procedure (NULL));
  g_assert_null (gimp_filter_layer_get_procedure_args (NULL));
  g_assert_null (g_object_get_qdata (wrong, key));
  /* A live wrong-type object is safe to reject. No invalid-address probe. */
  g_assert_null (gimp_filter_layer_get_procedure ((GimpFilterLayer *) wrong));
  g_assert_null (gimp_filter_layer_get_procedure_args ((GimpFilterLayer *) wrong));
  g_assert_null (g_object_get_qdata (wrong, key));
  g_object_unref (wrong);
  g_assert_true (gimp_filter_layer_set_definition (fixture.filter, "closed metadata", NULL, input, NULL));
  gimp_value_array_unref (input);
  name = gimp_filter_layer_get_procedure (fixture.filter);
  owned_name = g_strdup (name);
  before_close = gimp_filter_layer_get_procedure_args (fixture.filter);
  before = checkpoint_new (&fixture);
  thread = g_thread_new ("filter-entry-query", query_wrong_thread, fixture.filter);
  g_thread_join (thread);
  checkpoint_assert_clear (&fixture, &before);
  g_assert_true (name == gimp_filter_layer_get_procedure (fixture.filter));

  g_assert_true (gimp_painter_binding_close (G_OBJECT (fixture.filter), NULL));
  g_assert_cmpint (gimp_filter_layer_get_state (fixture.filter), ==, GIMP_FILTER_LAYER_CLOSED);
  before = checkpoint_new (&fixture);
  g_assert_true (name == gimp_filter_layer_get_procedure (fixture.filter));
  g_assert_cmpstr (name, ==, "closed metadata");
  after_close = gimp_filter_layer_get_procedure_args (fixture.filter);
  assert_rich_arguments (after_close);
  thread = g_thread_new ("closed-filter-query", query_wrong_thread, fixture.filter);
  g_thread_join (thread);
  checkpoint_assert_clear (&fixture, &before);
  fixture_clear (&fixture);
  g_assert_cmpstr (owned_name, ==, "closed metadata");
  assert_rich_arguments (before_close);
  assert_rich_arguments (after_close);
  g_free (owned_name);
  g_array_unref (after_close);
  g_array_unref (before_close);
}

static void
c_opaque_model_stays_opaque (void)
{
  static const guint8 bytes[] = { 0xff, 0, 0x80, 0x7f };
  Fixture fixture = fixture_new ();
  GBytes *raw = g_bytes_new_static (bytes, sizeof bytes);
  GBytes *opaque = g_bytes_new_static ("", 0);
  ReadCheckpoint before;
  g_assert_true (gimp_filter_layer_set_definition_with_opaque_arguments (
    fixture.filter, "unsupported-opaque", raw, opaque, NULL));
  before = checkpoint_new (&fixture);
  g_assert_cmpstr (gimp_filter_layer_get_procedure (fixture.filter), ==, "unsupported-opaque");
  g_assert_null (gimp_filter_layer_get_procedure_args (fixture.filter));
  g_assert_null (gimp_filter_layer_snapshot_arguments (fixture.filter));
  checkpoint_assert_clear (&fixture, &before);
  g_bytes_unref (opaque);
  g_bytes_unref (raw);
  fixture_clear (&fixture);
}

/* The checked preparation is deliberately narrower than set_definition(). */
static void
legacy_value_clear (gpointer data)
{
  GValue *value = data;
  if (G_IS_VALUE (value)) g_value_unset (value);
}

static GArray *
legacy_args (void)
{
  GArray *args = g_array_sized_new (FALSE, TRUE, sizeof (GValue), 7);
  g_array_set_clear_func (args, legacy_value_clear);
  g_array_set_size (args, 7);
  for (guint i = 0; i < 7; ++i) g_value_init (argument (args, i), G_TYPE_INT);
  g_value_set_int (argument (args, 0), 42);
  g_value_set_int (argument (args, 1), 123);
  g_value_set_int (argument (args, 2), 456);
  g_value_set_int (argument (args, 4), 3);
  g_value_set_int (argument (args, 5), 2);
  g_value_set_int (argument (args, 6), -7);
  return args;
}

static GValue *
legacy_retype (GArray *args, guint index, GType type)
{
  GValue *value = argument (args, index);
  g_value_unset (value);
  if (type) g_value_init (value, type);
  return value;
}

static void
legacy_assert_values (GArray *args, const gint expected[7])
{
  g_assert_nonnull (args);
  g_assert_cmpuint (g_array_get_element_size (args), ==, sizeof (GValue));
  g_assert_cmpuint (args->len, ==, 7);
  for (guint i = 0; i < 7; ++i)
    {
      g_assert_true (G_VALUE_TYPE (argument (args, i)) == G_TYPE_INT);
      g_assert_cmpint (g_value_get_int (argument (args, i)), ==, expected[i]);
    }
}

static void
legacy_preserved_definition (Fixture *fixture)
{
  static const guint8 payload[] = { 0, 0xff, 0x80, 7, 0 };
  GBytes *raw = g_bytes_new_static (payload, sizeof payload);
  GBytes *opaque = g_bytes_new_static ("\0opaque\xff", 8);
  g_assert_true (gimp_filter_layer_set_definition_with_opaque_arguments (
    fixture->filter, "unsupported saved definition", raw, opaque, NULL));
  gimp_filter_layer_mark_as_loaded (fixture->filter);
  g_bytes_unref (raw);
  g_bytes_unref (opaque);
}

static void
legacy_assert_rejected (Fixture *fixture, const gchar *name, const GArray *args)
{
  ReadCheckpoint before = checkpoint_new (fixture);
  GError *error = NULL;
  g_assert_false (gimp_filter_layer_set_legacy_procedure (fixture->filter, name, args, &error));
  g_assert_error (error, GIMP_PAINTER_ERROR, GIMP_PAINTER_ERROR_INVALID_STATE);
  g_assert_nonnull (error->message);
  g_assert_cmpuint (strlen (error->message), >, 0);
  g_assert_nonnull (strstr (error->message,
    name && !strcmp (name, "plug-in-blinds") ? "Legacy Blinds" : "Unsupported legacy preparation route"));
  g_clear_error (&error);
  checkpoint_assert_clear (fixture, &before);
}

/* The C++ registry fixture invokes the actual C11 rejection/checkpoint path. */
void
gimp_test_filter_entry_assert_legacy_rejected (GimpFilterLayer *filter)
{
  Fixture fixture = { gimp_item_get_image (GIMP_ITEM (filter)), filter };
  legacy_assert_rejected (&fixture, "plug-in-blinds", NULL);
}

static void
legacy_wait (GimpFilterLayer *filter, GimpFilterLayerState state)
{
  const gint64 deadline = g_get_monotonic_time () + 20 * G_TIME_SPAN_SECOND;
  while (gimp_filter_layer_get_state (filter) != state && g_get_monotonic_time () < deadline)
    { g_main_context_iteration (NULL, FALSE); g_usleep (100); }
  g_assert_cmpint (gimp_filter_layer_get_state (filter), ==, state);
}

static GimpLayer *
legacy_source (Fixture *fixture)
{
  GimpLayer *source = gimp_layer_new (fixture->image,
    gimp_image_get_width (fixture->image), gimp_image_get_height (fixture->image),
    babl_format ("R'G'B'A u8"), "checked Blinds source", 1.0, GIMP_LAYER_MODE_NORMAL_LEGACY);
  const guchar rgba[] = { 31, 121, 217, 255 };
  GeglColor *color = gegl_color_new (NULL);
  g_assert_true (gimp_image_add_layer (fixture->image, source, NULL, 1, FALSE));
  gegl_color_set_pixel (color, babl_format ("R'G'B'A u8"), rgba);
  gegl_buffer_set_color (gimp_drawable_get_buffer (GIMP_DRAWABLE (source)), NULL, color);
  g_object_unref (color);
  gimp_drawable_update (GIMP_DRAWABLE (source), 0, 0,
                        gimp_image_get_width (fixture->image), gimp_image_get_height (fixture->image));
  return source;
}

static void
c_legacy_null_typed_zero (void)
{
  const gint zero[7] = { 0 };
  Fixture fixture = fixture_new ();
  GArray *saved;
  GError *error = NULL;
  gimp_test_filter_entry_assert_blinds_provider (gimp);
  legacy_source (&fixture);
  g_assert_true (gimp_filter_layer_set_legacy_procedure (fixture.filter, "plug-in-blinds", NULL, &error));
  g_assert_no_error (error);
  g_assert_cmpstr (gimp_filter_layer_get_procedure (fixture.filter), ==, "plug-in-blinds");
  saved = gimp_filter_layer_get_procedure_args (fixture.filter);
  legacy_assert_values (saved, zero);
  g_array_unref (saved);
  legacy_wait (fixture.filter, GIMP_FILTER_LAYER_FAILED);
  g_assert_cmpuint (gimp_filter_layer_get_run_count (fixture.filter), ==, 0);
  saved = gimp_filter_layer_get_procedure_args (fixture.filter);
  legacy_assert_values (saved, zero);
  g_array_unref (saved);
  fixture_clear (&fixture);
}

static void
c_legacy_array_shapes (void)
{
  Fixture fixture = fixture_new ();
  const guint lengths[] = { 0, 6, 8 };
  legacy_preserved_definition (&fixture);
  for (guint i = 0; i < G_N_ELEMENTS (lengths); ++i)
    {
      GArray *args = g_array_sized_new (FALSE, TRUE, sizeof (GValue), lengths[i]);
      g_array_set_size (args, lengths[i]);
      legacy_assert_rejected (&fixture, "plug-in-blinds", args);
      g_array_unref (args);
    }
  {
    GArray *args = g_array_sized_new (FALSE, TRUE, sizeof (gint), 7);
    g_array_set_size (args, 7);
    legacy_assert_rejected (&fixture, "plug-in-blinds", args);
    g_array_unref (args);
  }
  fixture_clear (&fixture);
}

static void
c_legacy_owned_exact_arguments (void)
{
  const gint expected[7] = { 42, 0, 0, 0, 3, 2, -7 };
  Fixture fixture = fixture_new ();
  GArray *args = legacy_args (), *first, *second;
  g_assert_true (gimp_filter_layer_set_legacy_procedure (fixture.filter, "plug-in-blinds", args, NULL));
  for (guint i = 0; i < 7; ++i) g_value_set_int (argument (args, i), 999);
  g_array_unref (args);
  first = gimp_filter_layer_get_procedure_args (fixture.filter);
  second = gimp_filter_layer_get_procedure_args (fixture.filter);
  legacy_assert_values (first, expected);
  legacy_assert_values (second, expected);
  g_assert_true (first != second);
  g_assert_true (first->data != second->data);
  g_value_set_int (argument (first, 3), -42);
  fixture_clear (&fixture);
  legacy_assert_values (second, expected);
  g_assert_cmpint (g_value_get_int (argument (first, 3)), ==, -42);
  g_array_unref (first);
  g_array_unref (second);
}

static void
c_legacy_scalar_conversions (void)
{
  Fixture fixture = fixture_new ();
  GArray *args = legacy_args (), *saved;
  const gint mixed[7] = { GIMP_RUN_WITH_LAST_VALS, 0, 0, -12, 101, 1, -7 };
  g_value_set_enum (legacy_retype (args, 0, GIMP_TYPE_RUN_MODE), GIMP_RUN_WITH_LAST_VALS);
  g_value_set_float (legacy_retype (args, 3, G_TYPE_FLOAT), -12.75f);
  g_value_set_double (legacy_retype (args, 4, G_TYPE_DOUBLE), 101.75);
  g_value_set_boolean (legacy_retype (args, 5, G_TYPE_BOOLEAN), TRUE);
  g_assert_true (gimp_filter_layer_set_legacy_procedure (fixture.filter, "plug-in-blinds", args, NULL));
  saved = gimp_filter_layer_get_procedure_args (fixture.filter);
  legacy_assert_values (saved, mixed);
  g_array_unref (saved);
  {
    const gdouble inputs[] = { G_MININT, G_MAXINT, -0.75, 0.0, 12.75 };
    const gint expected[] = { G_MININT, G_MAXINT, 0, 0, 12 };
    for (guint i = 0; i < G_N_ELEMENTS (inputs); ++i)
      {
        g_value_set_double (legacy_retype (args, 3, G_TYPE_DOUBLE), inputs[i]);
        g_assert_true (gimp_filter_layer_set_legacy_procedure (fixture.filter, "plug-in-blinds", args, NULL));
        saved = gimp_filter_layer_get_procedure_args (fixture.filter);
        g_assert_cmpint (g_value_get_int (argument (saved, 3)), ==, expected[i]);
        g_array_unref (saved);
      }
  }
  g_value_set_boolean (legacy_retype (args, 3, G_TYPE_BOOLEAN), FALSE);
  g_value_set_int (legacy_retype (args, 4, G_TYPE_INT), 0);
  g_value_set_int (legacy_retype (args, 5, G_TYPE_INT), -19);
  g_assert_true (gimp_filter_layer_set_legacy_procedure (fixture.filter, "plug-in-blinds", args, NULL));
  saved = gimp_filter_layer_get_procedure_args (fixture.filter);
  g_assert_cmpint (g_value_get_int (argument (saved, 3)), ==, 0);
  g_assert_cmpint (g_value_get_int (argument (saved, 4)), ==, 0);
  g_assert_cmpint (g_value_get_int (argument (saved, 5)), ==, -19);
  g_array_unref (saved);
  g_array_unref (args);
  fixture_clear (&fixture);
}

static void
c_legacy_rejected_scalar_types (void)
{
  Fixture fixture = fixture_new ();
  const GType types[] = { G_TYPE_STRING, G_TYPE_OBJECT, G_TYPE_BYTES, G_TYPE_INVALID,
                         GIMP_TYPE_ORIENTATION_TYPE, G_TYPE_POINTER, G_TYPE_UINT };
  legacy_preserved_definition (&fixture);
  for (guint i = 0; i < G_N_ELEMENTS (types); ++i)
    {
      GArray *args = legacy_args ();
      GValue *value = legacy_retype (args, 3, types[i]);
      guint finalized = 0;
      if (types[i] == G_TYPE_STRING) g_value_set_string (value, "12");
      if (types[i] == G_TYPE_OBJECT)
        {
          GObject *object = g_object_new (G_TYPE_OBJECT, NULL);
          g_object_weak_ref (object, weak_finalized, &finalized);
          g_value_take_object (value, object);
        }
      if (types[i] == G_TYPE_BYTES) g_value_take_boxed (value, g_bytes_new_static ("12", 2));
      legacy_assert_rejected (&fixture, "plug-in-blinds", args);
      g_array_unref (args);
      if (types[i] == G_TYPE_OBJECT) g_assert_cmpuint (finalized, ==, 1);
    }
  {
    const gdouble numbers[] = { NAN, INFINITY, -INFINITY, (gdouble) G_MAXINT + 1, (gdouble) G_MININT - 1 };
    for (guint i = 0; i < G_N_ELEMENTS (numbers); ++i)
      {
        GArray *args = legacy_args ();
        g_value_set_double (legacy_retype (args, 6, G_TYPE_DOUBLE), numbers[i]);
        legacy_assert_rejected (&fixture, "plug-in-blinds", args);
        g_value_set_float (legacy_retype (args, 6, G_TYPE_FLOAT), (gfloat) numbers[i]);
        /* G_MININT-1 rounds back into range as float; use a finite lower overflow. */
        if (i == 4) g_value_set_float (argument (args, 6), (gfloat) G_MININT * 2.0f);
        legacy_assert_rejected (&fixture, "plug-in-blinds", args);
        g_array_unref (args);
      }
  }
  {
    GArray *args = legacy_args ();
    g_value_set_enum (legacy_retype (args, 3, GIMP_TYPE_RUN_MODE), GIMP_RUN_NONINTERACTIVE);
    legacy_assert_rejected (&fixture, "plug-in-blinds", args);
    g_array_unref (args);
  }
  fixture_clear (&fixture);
}

static void
legacy_bytes_finalized (gpointer data)
{ ++*(guint *) data; }

static void
c_legacy_ignored_context (void)
{
  const gint expected[7] = { 42, 0, 0, 0, 3, 2, -7 };
  for (guint empty = 0; empty < 2; ++empty)
    {
      Fixture fixture = fixture_new ();
      GArray *args = legacy_args (), *saved;
      guint finalized = 0, bytes_finalized = 0;
      if (empty)
        { legacy_retype (args, 1, G_TYPE_INVALID); legacy_retype (args, 2, G_TYPE_INVALID); }
      else
        {
          GObject *object = g_object_new (G_TYPE_OBJECT, NULL);
          g_object_weak_ref (object, weak_finalized, &finalized);
          g_value_take_object (legacy_retype (args, 1, G_TYPE_OBJECT), object);
          g_value_take_boxed (legacy_retype (args, 2, G_TYPE_BYTES),
            g_bytes_new_with_free_func ("context", 7, legacy_bytes_finalized, &bytes_finalized));
        }
      g_assert_true (gimp_filter_layer_set_legacy_procedure (fixture.filter, "plug-in-blinds", args, NULL));
      g_array_unref (args);
      if (!empty)
        { g_assert_cmpuint (finalized, ==, 1); g_assert_cmpuint (bytes_finalized, ==, 1); }
      saved = gimp_filter_layer_get_procedure_args (fixture.filter);
      legacy_assert_values (saved, expected);
      g_array_unref (saved);
      fixture_clear (&fixture);
    }
}

static void
c_legacy_unsupported_names_preserve (void)
{
  const gchar *names[] = { NULL, "", "unknown checked route", "plug-in-edge",
                           "plug-in-gauss", "plug-in-convmatrix", "Plug-in-blinds" };
  Fixture fixture = fixture_new ();
  legacy_preserved_definition (&fixture);
  for (guint i = 0; i < G_N_ELEMENTS (names); ++i)
    legacy_assert_rejected (&fixture, names[i], NULL);
  fixture_clear (&fixture);
}

static void
c_legacy_provider_rejections (void)
{
  Fixture fixture = fixture_new ();
  legacy_preserved_definition (&fixture);
  gimp_test_filter_entry_provider_rejections (gimp, fixture.filter);
  fixture_clear (&fixture);
}

static gpointer
legacy_wrong_thread (gpointer data)
{
  GError *error = NULL;
  g_assert_false (gimp_filter_layer_set_legacy_procedure (data, "plug-in-blinds", NULL, &error));
  g_assert_error (error, GIMP_PAINTER_ERROR, GIMP_PAINTER_ERROR_WRONG_THREAD);
  g_clear_error (&error);
  return NULL;
}

static void
c_legacy_owner_boundaries (void)
{
  Fixture fixture = fixture_new ();
  GObject *wrong = g_object_new (G_TYPE_OBJECT, NULL);
  GError *error = NULL;
  GQuark key = g_quark_from_static_string ("gimp-painter-binding-store-v1");
  ReadCheckpoint before;
  GThread *thread;
  GArray *invalid_shape = g_array_new (FALSE, FALSE, sizeof (gint));
  g_assert_false (gimp_filter_layer_set_legacy_procedure (NULL, "plug-in-blinds", invalid_shape, &error));
  g_assert_error (error, GIMP_PAINTER_ERROR, GIMP_PAINTER_ERROR_WRONG_TYPE);
  g_clear_error (&error);
  g_assert_null (g_object_get_qdata (wrong, key));
  g_assert_false (gimp_filter_layer_set_legacy_procedure ((GimpFilterLayer *) wrong, "plug-in-blinds", invalid_shape, &error));
  g_assert_error (error, GIMP_PAINTER_ERROR, GIMP_PAINTER_ERROR_WRONG_TYPE);
  g_clear_error (&error);
  g_assert_null (g_object_get_qdata (wrong, key));
  g_object_unref (wrong);
  legacy_preserved_definition (&fixture);
  before = checkpoint_new (&fixture);
  thread = g_thread_new ("checked-Blinds-thread", legacy_wrong_thread, fixture.filter);
  g_thread_join (thread);
  checkpoint_assert_clear (&fixture, &before);
  g_assert_true (gimp_painter_binding_close (G_OBJECT (fixture.filter), NULL));
  before = checkpoint_new (&fixture);
  g_assert_false (gimp_filter_layer_set_legacy_procedure (fixture.filter, "plug-in-blinds", invalid_shape, &error));
  g_assert_error (error, GIMP_PAINTER_ERROR, GIMP_PAINTER_ERROR_CLOSED);
  g_clear_error (&error);
  checkpoint_assert_clear (&fixture, &before);
  g_array_unref (invalid_shape);
  fixture_clear (&fixture);
}

static void
legacy_count_notify (GObject *object, GParamSpec *spec, gpointer data)
{
  (void) object; (void) spec;
  ++*(guint *) data;
}

static void
c_legacy_render_no_undo (void)
{
  const gint expected[7] = { 42, 0, 0, 0, 3, 2, -7 };
  const gchar *properties[] = { "notify::filter-procedure", "notify::filter-arguments",
                                "notify::filter-original-definition", "notify::filter-opaque-arguments" };
  Fixture fixture = fixture_new_size (12, 12);
  GArray *args = legacy_args ();
  gint undo, redo;
  guint notifications = 0;
  legacy_source (&fixture);
  legacy_preserved_definition (&fixture);
  undo = gimp_undo_stack_get_depth (gimp_image_get_undo_stack (fixture.image));
  redo = gimp_undo_stack_get_depth (gimp_image_get_redo_stack (fixture.image));
  for (guint i = 0; i < G_N_ELEMENTS (properties); ++i)
    g_signal_connect (fixture.filter, properties[i], G_CALLBACK (legacy_count_notify), &notifications);
  for (guint repeat = 0; repeat < 2; ++repeat)
    {
      const guint64 revision = gimp_filter_layer_get_definition_revision (fixture.filter);
      const guint64 generation = gimp_filter_layer_get_generation (fixture.filter);
      const guint64 runs = gimp_filter_layer_get_run_count (fixture.filter);
      GArray *saved;
      guchar pixels[12 * 12 * 4];
      GeglRectangle rect = { 0, 0, 12, 12 };
      g_assert_true (gimp_filter_layer_set_legacy_procedure (fixture.filter, "plug-in-blinds", args, NULL));
      g_assert_cmpuint (gimp_filter_layer_get_definition_revision (fixture.filter), ==, revision + 1);
      g_assert_cmpuint (gimp_filter_layer_get_generation (fixture.filter), ==, generation + 1);
      g_assert_cmpuint (notifications, ==, (repeat + 1) * 4);
      g_assert_null (gimp_filter_layer_ref_definition (fixture.filter));
      g_assert_null (gimp_filter_layer_ref_opaque_arguments (fixture.filter));
      legacy_wait (fixture.filter, GIMP_FILTER_LAYER_CLEAN);
      g_assert_cmpuint (gimp_filter_layer_get_run_count (fixture.filter), ==, runs + 1);
      g_assert_cmpuint (gimp_filter_layer_get_cache_generation (fixture.filter), ==,
                        gimp_filter_layer_get_generation (fixture.filter));
      gegl_buffer_get (gimp_drawable_get_buffer (GIMP_DRAWABLE (fixture.filter)), &rect,
                       1.0, babl_format ("R'G'B'A u8"), pixels, GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);
      for (guint i = 0; i < G_N_ELEMENTS (pixels); i += 4)
        {
          g_assert_cmpuint (pixels[i], ==, 31); g_assert_cmpuint (pixels[i + 1], ==, 121);
          g_assert_cmpuint (pixels[i + 2], ==, 217); g_assert_cmpuint (pixels[i + 3], ==, 255);
        }
      saved = gimp_filter_layer_get_procedure_args (fixture.filter);
      legacy_assert_values (saved, expected);
      g_array_unref (saved);
      g_assert_cmpint (gimp_undo_stack_get_depth (gimp_image_get_undo_stack (fixture.image)), ==, undo);
      g_assert_cmpint (gimp_undo_stack_get_depth (gimp_image_get_redo_stack (fixture.image)), ==, redo);
    }
  g_signal_handlers_disconnect_by_data (fixture.filter, &notifications);
  g_array_unref (args);
  fixture_clear (&fixture);
}

#include "test-filter-small-tiles-entry.inc"
#include "test-filter-retinex-entry.inc"

int
main (int argc, char **argv)
{
  int result;
  g_test_init (&argc, &argv, NULL);
  gimp_test_utils_set_gimp3_directory ("GIMP_TESTING_ABS_TOP_SRCDIR", "app/tests/gimpdir");
  gimp = gimp_init_for_testing ();
  g_test_add_func ("/filter-entry-contracts/c/c_factory_native_identity", c_factory_native_identity);
  g_test_add_func ("/filter-entry-contracts/c/c_exact_name_absent_and_empty", c_exact_name_absent_and_empty);
  g_test_add_func ("/filter-entry-contracts/c/c_owned_values_and_model_identity", c_owned_values_and_model_identity);
  g_test_add_func ("/filter-entry-contracts/c/c_null_and_empty_argument_shapes", c_null_and_empty_argument_shapes);
  g_test_add_func ("/filter-entry-contracts/c/c_scalar_object_identity_and_ownership", c_scalar_object_identity_and_ownership);
  g_test_add_func ("/filter-entry-contracts/c/c_live_object_array_container_copy", c_live_object_array_container_copy);
  g_test_add_func ("/filter-entry-contracts/c/c_expired_arrays_reject_atomically", c_expired_arrays_reject_atomically);
  g_test_add_func ("/filter-entry-contracts/c/c_boundary_checks_and_closed_metadata", c_boundary_checks_and_closed_metadata);
  g_test_add_func ("/filter-entry-contracts/c/c_opaque_model_stays_opaque", c_opaque_model_stays_opaque);
  g_test_add_func ("/filter-entry-contracts/c/c_legacy_null_typed_zero", c_legacy_null_typed_zero);
  g_test_add_func ("/filter-entry-contracts/c/c_legacy_array_shapes", c_legacy_array_shapes);
  g_test_add_func ("/filter-entry-contracts/c/c_legacy_owned_exact_arguments", c_legacy_owned_exact_arguments);
  g_test_add_func ("/filter-entry-contracts/c/c_legacy_scalar_conversions", c_legacy_scalar_conversions);
  g_test_add_func ("/filter-entry-contracts/c/c_legacy_rejected_scalar_types", c_legacy_rejected_scalar_types);
  g_test_add_func ("/filter-entry-contracts/c/c_legacy_ignored_context", c_legacy_ignored_context);
  g_test_add_func ("/filter-entry-contracts/c/c_legacy_unsupported_names_preserve", c_legacy_unsupported_names_preserve);
  g_test_add_func ("/filter-entry-contracts/c/c_legacy_provider_rejections", c_legacy_provider_rejections);
  g_test_add_func ("/filter-entry-contracts/c/c_legacy_owner_boundaries", c_legacy_owner_boundaries);
  g_test_add_func ("/filter-entry-contracts/c/c_legacy_render_no_undo", c_legacy_render_no_undo);
  gimp_test_filter_entry_cpp_register (gimp);
  register_small_tiles_entry_tests ();
  register_retinex_entry_tests ();
  result = g_test_run ();
  gimp_test_utils_set_gimp3_directory ("GIMP_TESTING_ABS_TOP_BUILDDIR", "app/tests/gimpdir-output");
  gimp_exit (gimp, TRUE);
  return result;
}
