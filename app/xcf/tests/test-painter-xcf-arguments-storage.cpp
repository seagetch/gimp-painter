/* SPDX-License-Identifier: GPL-3.0-or-later
 * Run with migration/tests/run_xcf_argument_storage_tests.py. The generated
 * include contains the unchanged private production helpers and public
 * GimpArray declaration, rather than a separately maintained implementation.
 */
#include "painter-xcf-storage.hpp"
#include <algorithm>
#include <atomic>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
namespace {
/* This seam changes only recoverable allocation calls in the extracted
 * production helpers. Native resource-limit/reopen tests run separately. */
int allocations_before_failure = -1;
bool allocation_allowed ()
{
  if (!allocations_before_failure) return false;
  if (allocations_before_failure > 0) --allocations_before_failure;
  return true;
}
gpointer controlled_try_malloc (gsize size)
{ return allocation_allowed () ? g_try_malloc (size) : nullptr; }
gpointer controlled_try_malloc0_n (gsize count, gsize size)
{ return allocation_allowed () ? g_try_malloc0_n (count, size) : nullptr; }
}
#define g_try_malloc controlled_try_malloc
#undef g_try_new0
#define g_try_new0(Type, count) static_cast<Type *> (controlled_try_malloc0_n ((count), sizeof (Type)))
#include "painter-xcf-arguments-storage-helpers.hpp"
#undef g_try_malloc
#undef g_try_new0

using namespace GimpPainterXcf;
namespace {
void arrays ()
{
  for (gsize width : {gsize (1), gsize (4), gsize (8)})
    for (gsize length : {gsize (0), width, gsize (65536) - width, gsize (65536),
                         gsize (65536) + width, gsize (131072) + width})
      {
        /* Deliberately unaligned input must never trigger a whole-array copy
         * through GVariant's native numeric-array alignment machinery. */
        std::vector<guint8> raw (length + 1);
        for (gsize i = 0; i < length; ++i) raw[i + 1] = i % 251;
        GimpArray array {raw.data () + 1, length, TRUE};
        const guint before = storage_live_files ();
        auto *variant = g_variant_ref_sink (array_bytes (&array, width, nullptr));
        g_assert_true (g_variant_is_of_type (variant, G_VARIANT_TYPE_BYTESTRING));
        gsize size = 0;
        const auto *data = static_cast<const guint8 *> (g_variant_get_fixed_array (variant, &size, 1));
        g_assert_cmpuint (size, ==, length);
        for (gsize i = 0; i < length; ++i)
          {
#if defined(PAINTER_TEST_FORCE_BIG_ENDIAN) || G_BYTE_ORDER == G_BIG_ENDIAN
            const gsize source = (i / width) * width + width - 1 - i % width;
#else
            const gsize source = i;
#endif
            g_assert_cmpuint (data[i], ==, raw[1 + source]);
          }
        g_assert_cmpuint (storage_live_files (), ==, before + (length > 65536));
        MaterializationBudget budget;
        auto decoded = materialize_array (variant, width, budget);
        if (length) g_assert_cmpmem (decoded.get (), length, raw.data () + 1, length);
        if (length)
          {
            const guint8 original = data[0];
            std::fill (raw.begin (), raw.end (), original ^ 255);
            g_assert_cmpuint (data[0], ==, original);
          }
        g_variant_unref (variant);
        g_assert_cmpuint (storage_live_files (), ==, before);
      }
  guint8 raw[5] = {};
  GimpArray malformed {raw, sizeof raw, TRUE};
  bool threw = false;
  try { auto *value = g_variant_ref_sink (array_bytes (&malformed, 4, nullptr)); g_variant_unref (value); }
  catch (const std::runtime_error &) { threw = true; }
  g_assert_true (threw);
}
void strings ()
{
  for (gsize length : {gsize (0), gsize (1), gsize (65535), gsize (65536), gsize (131073)})
    {
      std::string text (length, char (0xff));
      auto *variant = g_variant_ref_sink (string_bytes (text.c_str (), nullptr));
      gboolean present; GVariant *raw;
      g_variant_get (variant, "(b@ay)", &present, &raw);
      gsize size;
      const auto *data = static_cast<const guint8 *> (g_variant_get_fixed_array (raw, &size, 1));
      g_assert_true (present);
      g_assert_cmpuint (size, ==, length + 1);
      g_assert_cmpmem (data, size, text.c_str (), length + 1);
      MaterializationBudget budget;
      auto *decoded = read_string (variant, budget);
      g_assert_cmpmem (decoded, size, text.c_str (), length + 1);
      g_free (decoded);
      g_variant_unref (raw); g_variant_unref (variant);
      g_assert_cmpuint (storage_live_files (), ==, 0);
    }
  auto *variant = g_variant_ref_sink (string_bytes (nullptr, nullptr));
  gboolean present; GVariant *raw;
  g_variant_get (variant, "(b@ay)", &present, &raw);
  g_assert_false (present);
  g_assert_cmpuint (g_variant_n_children (raw), ==, 0);
  MaterializationBudget budget;
  g_assert_null (read_string (variant, budget));
  g_variant_unref (raw); g_variant_unref (variant);
}
void materialization ()
{
  const gsize limit = gsize (256) * 1024 * 1024;
  MaterializationBudget budget;
  g_assert_cmpuint (budget.remaining, ==, limit);
  budget.consume (limit - 8);
  Variant text (g_variant_ref_sink (string_bytes ("abc", nullptr)));
  g_free (read_string (text.get (), budget));
  const guint8 data[4] = {0, 1, 2, 3};
  Variant raw (g_variant_ref_sink (bytes (data, sizeof data, nullptr)));
  auto copied = materialize_array (raw.get (), 4, budget);
  g_assert_cmpuint (budget.remaining, ==, 0);
  bool threw = false;
  try { g_free (read_string (text.get (), budget)); }
  catch (const MaterializationError &) { threw = true; }
  g_assert_true (threw);
  MaterializationBudget oversized;
  threw = false;
  try { oversized.consume (limit + 1); }
  catch (const MaterializationError &) { threw = true; }
  g_assert_true (threw);
  g_assert_cmpuint (oversized.remaining, ==, limit);
  for (bool string : {true, false})
    {
      MaterializationBudget available;
      allocations_before_failure = 0; threw = false;
      try
        {
          if (string) g_free (read_string (text.get (), available));
          else { auto result = materialize_array (raw.get (), 4, available); }
        }
      catch (const MaterializationError &) { threw = true; }
      allocations_before_failure = -1;
      g_assert_true (threw);
    }
}
void imported_allocation_failures ()
{
  g_assert_null (ImportedScalarTest::try_argument_copy (nullptr, 0));
  bool malformed = false;
  try { g_free (ImportedScalarTest::try_argument_copy (nullptr, 1)); }
  catch (const std::runtime_error &) { malformed = true; }
  g_assert_true (malformed);
  for (GType type : {G_TYPE_STRING, G_TYPE_STRV, GIMP_TYPE_ARRAY,
                     GIMP_TYPE_INT32_ARRAY, GIMP_TYPE_DOUBLE_ARRAY})
    {
      GValue source = G_VALUE_INIT;
      g_value_init (&source, type);
      const guint8 data[8] = {0, 1, 2, 3, 4, 5, 6, 7};
      if (type == G_TYPE_STRING) g_value_set_string (&source, "preserve me");
      else if (type == G_TYPE_STRV)
        {
          const gchar *strings[] = {"first", "second", nullptr};
          g_value_set_boxed (&source, strings);
        }
      else g_value_take_boxed (&source, gimp_array_new (data, sizeof data, FALSE));
      const int failures = type == G_TYPE_STRV ? 3 : 1;
      for (int fail_at = 0; fail_at < failures; ++fail_at)
        {
          GValue target = G_VALUE_INIT; g_value_init (&target, type);
          allocations_before_failure = fail_at;
          bool threw = false;
          try { ImportedScalarTest::copy_imported_argument_scalar (&source, &target); }
          catch (const std::bad_alloc &) { threw = true; }
          allocations_before_failure = -1;
          g_assert_true (threw);
          if (type == G_TYPE_STRING) g_assert_null (g_value_get_string (&target));
          else g_assert_null (g_value_get_boxed (&target));
          g_value_unset (&target);
        }
      GValue target = G_VALUE_INIT; g_value_init (&target, type);
      ImportedScalarTest::copy_imported_argument_scalar (&source, &target);
      if (type == G_TYPE_STRING) g_assert_cmpstr (g_value_get_string (&target), ==, "preserve me");
      else if (type == G_TYPE_STRV)
        {
          auto **strings = static_cast<gchar **> (g_value_get_boxed (&target));
          g_assert_cmpstr (strings[0], ==, "first"); g_assert_cmpstr (strings[1], ==, "second");
          g_assert_null (strings[2]);
        }
      else
        {
          auto *array = static_cast<GimpArray *> (g_value_get_boxed (&target));
          g_assert_false (array->static_data);
          g_assert_cmpmem (array->data, array->length, data, sizeof data);
          g_assert_true (array->data != static_cast<GimpArray *> (g_value_get_boxed (&source))->data);
        }
      g_value_unset (&target); g_value_unset (&source);
    }
  /* Immutable GBytes bypass mutable allocation failure and retain ownership. */
  GValue source = G_VALUE_INIT, target = G_VALUE_INIT;
  g_value_init (&source, G_TYPE_BYTES); g_value_init (&target, G_TYPE_BYTES);
  g_value_take_boxed (&source, g_bytes_new_static ("abc", 3));
  allocations_before_failure = 0;
  ImportedScalarTest::copy_imported_argument_scalar (&source, &target);
  allocations_before_failure = -1;
  g_assert_true (g_value_get_boxed (&source) == g_value_get_boxed (&target));
  g_value_unset (&source);
  gsize size; const void *data = g_bytes_get_data (static_cast<GBytes *> (g_value_get_boxed (&target)), &size);
  g_assert_cmpmem (data, size, "abc", 3);
  g_value_unset (&target);
}
void immutable_ast_entries ()
{
  std::vector<guint8> source (65537, 0x5a);
  for (bool snapshot : {false, true})
    {
      const guint before = storage_live_files ();
      Bytes retained;
      const void *original;
      {
        Variant raw (g_variant_ref_sink (bytes (source.data (), source.size (), nullptr)));
        original = g_variant_get_data (raw.get ());
        Variant payload (g_variant_ref_sink (g_variant_new ("(b@ay)", TRUE, raw.get ())));
        Variant entry (g_variant_ref_sink (snapshot ?
          g_variant_new ("(sbv)", g_type_name (G_TYPE_BYTES), FALSE, payload.get ()) :
          g_variant_new ("(sv)", g_type_name (G_TYPE_BYTES), payload.get ())));
        ArgumentEntry decoded (entry.get (), snapshot);
        g_assert_cmpstr (decoded.type_name (), ==, g_type_name (G_TYPE_BYTES));
        g_assert_false (decoded.is_null);
        gboolean present; GVariant *decoded_child;
        g_variant_get (decoded.payload.get (), "(b@ay)", &present, &decoded_child);
        g_assert_true (present);
        Variant decoded_raw (decoded_child);
        retained = Bytes::adopt (g_variant_get_data_as_bytes (decoded_raw.get ()));
        g_assert_true (g_bytes_get_data (retained.get (), nullptr) == original);
      }
      /* The extracted bytes alone must keep the original mapping alive. */
      g_assert_cmpuint (storage_live_files (), ==, before + 1);
      gsize length;
      const void *data = g_bytes_get_data (retained.get (), &length);
      g_assert_true (data == original);
      g_assert_cmpmem (data, length, source.data (), source.size ());
      retained.reset ();
      g_assert_cmpuint (storage_live_files (), ==, before);
    }
}
void cancellation ()
{
  auto *cancel = g_cancellable_new ();
  g_cancellable_cancel (cancel);
  bool threw = false;
  try { auto *value = g_variant_ref_sink (string_bytes (nullptr, cancel)); g_variant_unref (value); }
  catch (const StorageError& error) { threw = error.domain == G_IO_ERROR && error.code == G_IO_ERROR_CANCELLED; }
  g_assert_true (threw);
  g_cancellable_reset (cancel);
  std::vector<guint8> raw (64 * 1024 * 1024, 0xa5);
  GimpArray array {raw.data (), raw.size (), TRUE};
  std::atomic<bool> ready {false}, finished {false};
  std::thread cancelling ([&] {
    ready = true;
    while (!finished && !storage_live_files ()) std::this_thread::yield ();
    if (!finished) g_cancellable_cancel (cancel);
  });
  while (!ready) std::this_thread::yield ();
  threw = false;
  try { auto *value = g_variant_ref_sink (array_bytes (&array, 8, cancel)); g_variant_unref (value); }
  catch (const StorageError& error) { threw = error.domain == G_IO_ERROR && error.code == G_IO_ERROR_CANCELLED; }
  finished = true; cancelling.join ();
  g_assert_true (threw);
  g_assert_cmpuint (storage_live_files (), ==, 0);
  g_object_unref (cancel);
}
}
int main (int argc, char **argv)
{
  g_test_init (&argc, &argv, nullptr);
  g_test_add_func ("/painter/xcf/arguments/storage-arrays", arrays);
  g_test_add_func ("/painter/xcf/arguments/storage-strings", strings);
  g_test_add_func ("/painter/xcf/arguments/storage-cancellation", cancellation);
  g_test_add_func ("/painter/xcf/arguments/materialization-budget-and-allocation", materialization);
  g_test_add_func ("/painter/xcf/arguments/imported-allocation-failures", imported_allocation_failures);
  g_test_add_func ("/painter/xcf/arguments/immutable-ast-entries", immutable_ast_entries);
  return g_test_run ();
}
