/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Snapshot edits retain untouched immutable slots; they never resolve saved
 * reference provenance merely to change an unrelated scalar or array. */
#include "config.h"
#include <gegl.h>
#include <gio/gio.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
extern "C" {
#include "libgimpbase/gimpbase.h"
#include "core/core-types.h"
#include "core/gimp.h"
#include "core/gimpfilterlayer.h"
#include "core/gimpimage.h"
#include "core/gimpimage-undo.h"
#include "core/gimpitem.h"
#include "core/gimpundostack.h"
void gimp_test_filter_argument_patch (Gimp *application);
}
#include "core/gimpfilterlayer-arguments.hpp"
#include <cstring>
#include <memory>
#include <vector>

namespace {
using namespace GimpPainter;
struct SnapshotFree
{ void operator() (GimpFilterArgumentsSnapshot *p) const { gimp_filter_arguments_snapshot_free (p); } };
using Snapshot = std::unique_ptr<GimpFilterArgumentsSnapshot, SnapshotFree>;
struct BytesFree { void operator() (GBytes *p) const { if (p) g_bytes_unref (p); } };
using Bytes = std::unique_ptr<GBytes, BytesFree>;

struct Fixture
{
  Value number {G_TYPE_INT}, real {G_TYPE_DOUBLE}, text {G_TYPE_STRING}, array {GIMP_TYPE_INT32_ARRAY};
  GimpFilterArgumentReference reference {G_TYPE_OBJECT, 0x12345678, TRUE, TRUE};
  GimpFilterArgumentSpec nested[1] {}, specs[6] {};
  const guint64 nan_bits = G_GUINT64_CONSTANT (0x7ff8000000001234);
  Fixture ()
  {
    g_value_set_int (number.get (), 7);
    gdouble nan; std::memcpy (&nan, &nan_bits, sizeof nan);
    g_value_set_double (real.get (), nan);
    g_value_set_string (text.get (), "original");
    const gint32 elements[] = {-7, 0, 2};
    gimp_value_set_int32_array (array.get (), elements, G_N_ELEMENTS (elements));
    nested[0].value_type = G_TYPE_INT; nested[0].value = number.get ();
    specs[0].value_type = G_TYPE_INT; specs[0].value = number.get ();
    specs[1].value_type = G_TYPE_DOUBLE; specs[1].value = real.get ();
    specs[2].value_type = GIMP_TYPE_VALUE_ARRAY;
    specs[2].n_children = 1; specs[2].children = nested;
    specs[3].value_type = G_TYPE_OBJECT;
    specs[3].n_references = 1; specs[3].references = &reference;
    specs[4].value_type = G_TYPE_STRING; specs[4].value = text.get ();
    specs[5].value_type = GIMP_TYPE_INT32_ARRAY; specs[5].value = array.get ();
  }
  Snapshot snapshot () const
  {
    GError *error = nullptr;
    Snapshot result (gimp_filter_arguments_snapshot_import (G_N_ELEMENTS (specs), specs, &error));
    g_assert_no_error (error); g_assert_nonnull (result.get ()); return result;
  }
};

gint integer (const GimpFilterArgumentsSnapshot *snapshot)
{ return g_value_get_int (gimp_filter_arguments_snapshot_peek_value (snapshot, 0)); }

void assert_retained (const GimpFilterArgumentsSnapshot *original,
                      const GimpFilterArgumentsSnapshot *current,
                      guint64 expected_bits)
{
  g_assert_true (gimp_filter_arguments_snapshot_peek_value (current, 1) ==
                 gimp_filter_arguments_snapshot_peek_value (original, 1));
  const gdouble real = g_value_get_double (gimp_filter_arguments_snapshot_peek_value (current, 1));
  guint64 bits; std::memcpy (&bits, &real, sizeof bits);
  g_assert_true (bits == expected_bits);
  Snapshot before (gimp_filter_arguments_snapshot_nested (original, 2));
  Snapshot after (gimp_filter_arguments_snapshot_nested (current, 2));
  g_assert_true (gimp_filter_arguments_snapshot_peek_value (before.get (), 0) ==
                 gimp_filter_arguments_snapshot_peek_value (after.get (), 0));
  GimpFilterArgumentReference reference {};
  g_assert_true (gimp_filter_arguments_snapshot_reference (current, 3, 0, &reference));
  g_assert_true (reference.object_type == G_TYPE_OBJECT);
  g_assert_cmpint (reference.id, ==, 0x12345678);
  g_assert_true (reference.was_set); g_assert_true (reference.expired);
}

void assert_flattened_ownership (Fixture& fixture)
{
  std::size_t remaining = 65536;
  auto original = std::make_shared<FilterArguments> (G_N_ELEMENTS (fixture.specs), fixture.specs, 0, remaining);
  Value replacement (G_TYPE_INT); g_value_set_int (replacement.get (), 9);
  GimpFilterArgumentPatch first_patch {0, replacement.get ()};
  auto first = std::make_shared<FilterArguments> (original, 1, &first_patch);
  const auto *first_changed = first->at (0);
  g_assert_true (first->at (1) == original->at (1));
  g_assert_true (first->reference (3, 0) == original->reference (3, 0));
  g_assert_true (first->nested (2) == original->nested (2));
  Value text (G_TYPE_STRING); g_value_set_string (text.get (), "second edit");
  GimpFilterArgumentPatch second_patch {4, text.get ()};
  auto second = std::make_shared<FilterArguments> (first, 1, &second_patch);
  std::weak_ptr<const FilterArguments> retired = first;
  first.reset ();
  /* The changed slot has its own owner; preserving it must not keep the prior
   * patch model alive. The original owner is retained only by original slots. */
  g_assert_true (retired.expired ());
  g_assert_true (second->at (0) == first_changed);
  g_assert_cmpint (g_value_get_int (second->at (0)), ==, 9);
  g_assert_cmpint (g_value_get_int (original->at (0)), ==, 7);
  for (gint i = 10; i < 26; ++i)
    {
      g_value_set_int (replacement.get (), i);
      auto next = std::make_shared<FilterArguments> (second, 1, &first_patch);
      retired = second; second = std::move (next);
      g_assert_true (retired.expired ());
      g_assert_true (second->reference (3, 0) == original->reference (3, 0));
    }
}
}

extern "C" void
gimp_test_filter_argument_patch (Gimp *application)
{
  Fixture fixture;
  assert_flattened_ownership (fixture);
  auto image = ObjectRef<GObject>::adopt (G_OBJECT (gimp_image_new (application, 4, 4,
                                            GIMP_RGB, GIMP_PRECISION_U8_NON_LINEAR)));
  auto *owner = GIMP_IMAGE (image.get ());
  auto *filter = GIMP_FILTER_LAYER (gimp_filter_layer_new (owner, 4, 4, "Snapshot patch", 1,
                                                         GIMP_LAYER_MODE_NORMAL_LEGACY));
  g_assert_nonnull (filter);
  g_assert_true (gimp_image_add_layer (owner, GIMP_LAYER (filter), nullptr, 0, FALSE));
  const guint8 raw_data[] = {0xff, 0, 0x80, 7};
  Bytes raw (g_bytes_new_static (raw_data, sizeof raw_data));
  Snapshot original = fixture.snapshot ();
  GError *error = nullptr;
  g_assert_true (gimp_filter_layer_set_definition_with_snapshot (filter, "future-patch-fixture", raw.get (),
                                                                original.get (), &error));
  g_assert_no_error (error);
  gimp_image_undo_free (owner);
  const auto revision = gimp_filter_layer_get_definition_revision (filter);
  const auto generation = gimp_filter_layer_get_generation (filter);
  const auto runs = gimp_filter_layer_get_run_count (filter);
  const auto depth = [&] { return gimp_undo_stack_get_depth (gimp_image_get_undo_stack (owner)); };
  const auto unchanged = [&] {
    g_assert_true (gimp_filter_layer_get_definition_revision (filter) == revision);
    g_assert_true (gimp_filter_layer_get_generation (filter) == generation);
    g_assert_true (gimp_filter_layer_get_run_count (filter) == runs);
    g_assert_cmpint (depth (), ==, 0);
    Snapshot actual (gimp_filter_layer_snapshot_arguments (filter));
    g_assert_true (gimp_filter_arguments_snapshot_peek_value (actual.get (), 0) ==
                   gimp_filter_arguments_snapshot_peek_value (original.get (), 0));
  };
  g_assert_true (gimp_filter_layer_edit_argument_patch (filter, revision, original.get (), 0, nullptr, nullptr, nullptr, &error));
  g_assert_no_error (error); unchanged ();

  Value changed (G_TYPE_INT); g_value_set_int (changed.get (), 9);
  const GimpFilterArgumentPatch patch {0, changed.get ()};
  const auto rejects = [&] (guint64 expected, const GimpFilterArgumentsSnapshot *base,
                            guint count, const GimpFilterArgumentPatch *changes) {
    g_assert_false (gimp_filter_layer_edit_argument_patch (filter, expected, base, count, changes, nullptr, nullptr, &error));
    g_assert_nonnull (error); g_clear_error (&error); unchanged ();
  };
  const GimpFilterArgumentPatch duplicate[] = {patch, patch};
  rejects (revision, original.get (), G_N_ELEMENTS (duplicate), duplicate);
  const GimpFilterArgumentPatch outside {6, changed.get ()};
  rejects (revision, original.get (), 1, &outside);
  const GimpFilterArgumentPatch wrong_type {0, fixture.real.get ()};
  rejects (revision, original.get (), 1, &wrong_type);
  Snapshot other = fixture.snapshot ();
  rejects (revision, other.get (), 1, &patch); // Equal values are not the captured model.
  rejects (revision + 1, original.get (), 1, &patch);
  rejects (revision, original.get (), 1, nullptr);
  std::vector<gint32> oversized (129 * 1024, 1);
  Value large (GIMP_TYPE_INT32_ARRAY);
  gimp_value_set_int32_array (large.get (), oversized.data (), oversized.size ());
  const GimpFilterArgumentPatch too_large {5, large.get ()};
  rejects (revision, original.get (), 1, &too_large);

  guint guard_calls = 0;
  const auto guard = [] (gpointer data) -> gboolean {
    return ++*static_cast<guint *> (data) < 2;
  };
  g_assert_false (gimp_filter_layer_edit_argument_patch (filter, revision, original.get (), 1, &patch,
                                                        guard, &guard_calls, &error));
  g_assert_nonnull (error); g_clear_error (&error);
  g_assert_cmpuint (guard_calls, ==, 2);
  unchanged (); // Failed post-Undo guard removed only its own unchanged-definition item.
  auto *replacement_args = gimp_value_array_new_from_types (nullptr, G_TYPE_INT, 11, G_TYPE_NONE);
  for (gboolean push_undo = FALSE; push_undo <= TRUE; ++push_undo)
    {
      guard_calls = 0;
      g_assert_false (gimp_filter_layer_set_definition_checked (filter, "replacement-fixture", nullptr,
                        replacement_args, push_undo, guard, &guard_calls, &error));
      g_assert_nonnull (error); g_clear_error (&error);
      g_assert_cmpuint (guard_calls, ==, 2); unchanged ();
      Bytes saved (gimp_filter_layer_ref_definition (filter));
      g_assert_true (g_bytes_equal (saved.get (), raw.get ()));
    }
  gimp_value_array_unref (replacement_args);

  const gint32 elements[] = {-7, 1, 2};
  Value array (GIMP_TYPE_INT32_ARRAY);
  gimp_value_set_int32_array (array.get (), elements, G_N_ELEMENTS (elements));
  const GimpFilterArgumentPatch edits[] = {patch, {5, array.get ()}};
  g_assert_true (gimp_filter_layer_edit_argument_patch (filter, revision, original.get (),
                                                       G_N_ELEMENTS (edits), edits, nullptr, nullptr, &error));
  g_assert_no_error (error); g_assert_cmpint (depth (), ==, 1);
  Snapshot edited (gimp_filter_layer_snapshot_arguments (filter));
  g_assert_cmpint (integer (original.get ()), ==, 7);
  g_assert_cmpint (integer (edited.get ()), ==, 9);
  assert_retained (original.get (), edited.get (), fixture.nan_bits);
  const auto *saved_array = static_cast<const GimpArray *> (g_value_get_boxed (
    gimp_filter_arguments_snapshot_peek_value (edited.get (), 5)));
  g_assert_cmpmem (saved_array->data, saved_array->length, elements, sizeof elements);
  Bytes retained_raw (gimp_filter_layer_ref_definition (filter));
  g_assert_true (g_bytes_equal (retained_raw.get (), raw.get ()));
  g_assert_null (gimp_filter_layer_ref_opaque_arguments (filter));
  g_assert_true (gimp_image_undo (owner));
  Snapshot undone (gimp_filter_layer_snapshot_arguments (filter));
  g_assert_cmpint (integer (undone.get ()), ==, 7);
  assert_retained (original.get (), undone.get (), fixture.nan_bits);
  g_assert_true (gimp_image_redo (owner));
  Snapshot redone (gimp_filter_layer_snapshot_arguments (filter));
  g_assert_cmpint (integer (redone.get ()), ==, 9);
  assert_retained (original.get (), redone.get (), fixture.nan_bits);

  /* Opaque-only models have no editable snapshot. Rejection must retain their
   * separate opaque/raw payloads and must not create an Undo entry. */
  const guint8 opaque_data[] = {0x80, 0, 0xff};
  Bytes opaque (g_bytes_new_static (opaque_data, sizeof opaque_data));
  g_assert_true (gimp_filter_layer_set_definition_with_opaque_arguments (
    filter, "future-opaque-fixture", raw.get (), opaque.get (), &error));
  g_assert_no_error (error); gimp_image_undo_free (owner);
  const auto opaque_revision = gimp_filter_layer_get_definition_revision (filter);
  g_assert_false (gimp_filter_layer_edit_argument_patch (filter, opaque_revision,
                                                        edited.get (), 1, &patch, nullptr, nullptr, &error));
  g_assert_nonnull (error); g_clear_error (&error);
  g_assert_true (gimp_filter_layer_get_definition_revision (filter) == opaque_revision);
  g_assert_cmpint (depth (), ==, 0);
  Bytes retained_opaque (gimp_filter_layer_ref_opaque_arguments (filter));
  retained_raw.reset (gimp_filter_layer_ref_definition (filter));
  g_assert_true (g_bytes_equal (retained_opaque.get (), opaque.get ()));
  g_assert_true (g_bytes_equal (retained_raw.get (), raw.get ()));
}
