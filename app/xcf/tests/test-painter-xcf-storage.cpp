/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../painter-xcf-storage.hpp"
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <thread>
#ifdef G_OS_UNIX
#include <sys/mman.h>
#endif
using namespace GimpPainterXcf;
namespace {
struct VariantFree { void operator() (GVariant *p) const { if (p) g_variant_unref (p); } };
using Variant = std::unique_ptr<GVariant, VariantFree>;
const guint8 prefix[12] = {'G','P','X','C','F',0,0,0,1,0,0,0};
void compare (GVariant *raw)
{
  Variant value (g_variant_take_ref (raw));
  const guint before = storage_live_files ();
  {
    auto bytes = snapshot_variant (prefix, sizeof prefix, value.get (), nullptr);
    gsize size; const auto *data = static_cast<const guint8 *> (g_bytes_get_data (bytes.get (), &size));
    g_assert_cmpuint (storage_live_files (), ==, before + 1);
    g_assert_cmpuint (size, ==, sizeof prefix + g_variant_get_size (value.get ()));
    g_assert_cmpmem (data, sizeof prefix, prefix, sizeof prefix);
    g_assert_cmpuint (reinterpret_cast<std::uintptr_t> (data + sizeof prefix) % 8, ==, 0);
#if G_BYTE_ORDER == G_BIG_ENDIAN
    Variant wire (g_variant_byteswap (value.get ()));
#else
    Variant wire (g_variant_ref (value.get ()));
#endif
    g_assert_cmpmem (data + sizeof prefix, size - sizeof prefix, g_variant_get_data (wire.get ()), g_variant_get_size (wire.get ()));
    auto body = Bytes::adopt (g_bytes_new_from_bytes (bytes.get (), sizeof prefix, size - sizeof prefix));
    Variant decoded (g_variant_ref_sink (g_variant_new_from_bytes (g_variant_get_type (value.get ()), body.get (), FALSE)));
    g_assert_true (g_variant_is_normal_form (decoded.get ()));
  }
  g_assert_cmpuint (storage_live_files (), ==, before);
}
void scalar_and_container_types ()
{
  const char *values[] = {
    "true", "byte 0xff", "int16 -301", "uint16 60000", "-300000", "uint32 4000000000",
    "int64 -9223372036854775808", "uint64 18446744073709551615", "handle 77", "-1.2345",
    "'text'", "objectpath '/valid/path'", "signature 'a{sv}'", "()", "(byte 1, int64 -3, uint16 7)",
    "('a', 'bb', 'ccc')", "('a', uint32 9)", "(uint16 7, 'abc', byte 3)",
    "[1, 2, 3]", "['a', 'bb', 'ccc']", "@as []", "@a{sv} {}", "{'key': <uint32 2>, 'more': <[true, false]>}",
    "@ms nothing", "@mi nothing", "@ms just 'yes'", "@mi just 42", "<('x', uint64 9)>",
    "[(uint16 3, 'a', byte 7), (uint16 4, 'bb', byte 8)]", "@aay [[1,2,3], [], [4]]",
    "@m() just ()", "@mmu just nothing", "@mmu just just 55", "@a() [(), ()]"
  };
  for (const char *text : values)
    {
      GError *error = nullptr; GVariant *value = g_variant_parse (nullptr, text, nullptr, nullptr, &error);
      g_assert_no_error (error); g_assert_nonnull (value); compare (value);
    }
}
void offset_boundaries ()
{
  for (gsize length : {gsize (0), gsize (1), gsize (248), gsize (249), gsize (250), gsize (251), gsize (252),
                       gsize (253), gsize (254), gsize (255), gsize (256), gsize (65525), gsize (65530), gsize (65535), gsize (65536)})
    {
      std::string text (length, 'q');
      compare (g_variant_new ("(syts)", text.c_str (), 0xff, G_MAXUINT64, "tail"));
      GVariantBuilder builder; g_variant_builder_init (&builder, G_VARIANT_TYPE ("a{sv}"));
      g_variant_builder_add (&builder, "{sv}", "large", g_variant_new_string (text.c_str ()));
      g_variant_builder_add (&builder, "{sv}", "next", g_variant_new ("(msy)", nullptr, 0x7f));
      compare (g_variant_builder_end (&builder));
      GVariantBuilder array; g_variant_builder_init (&array, G_VARIANT_TYPE ("as"));
      g_variant_builder_add (&array, "s", text.c_str ()); g_variant_builder_add (&array, "s", "b");
      compare (g_variant_builder_end (&array));
    }
}
void full_typed_depth_and_unknown_arrays ()
{
  /* One v1 typed level has array/tuple/variant wire containers. A serializer
   * depth limit of 64 wrongly rejected a valid depth-32 argument tree. */
  Variant nested (g_variant_ref_sink (g_variant_new_string ("leaf")));
  for (guint level = 0; level <= 32; ++level)
    {
      GVariantBuilder builder; g_variant_builder_init (&builder, G_VARIANT_TYPE ("a(sbv)"));
      g_variant_builder_add (&builder, "(sbv)", "GimpValueArray", TRUE, nested.get ());
      nested.reset (g_variant_ref_sink (g_variant_builder_end (&builder)));
    }
  compare (g_variant_new ("a{sv}", nullptr));
  GVariantBuilder root; g_variant_builder_init (&root, G_VARIANT_TYPE ("a{sv}"));
  g_variant_builder_add (&root, "{sv}", "typed-arguments", nested.get ());
  compare (g_variant_builder_end (&root));
  /* Unknown arrays have no v1 aggregate typed-slot cap. The framing table
   * crosses its spill threshold repeatedly and exceeds the old 1M-node cap. */
  GVariantBuilder array; g_variant_builder_init (&array, G_VARIANT_TYPE ("as"));
  for (guint i = 0; i < 1048577; ++i) g_variant_builder_add (&array, "s", i & 1 ? "a" : "bb");
  compare (g_variant_builder_end (&array));
  /* Tuples require the spilled offsets in reverse order. */
  Variant first (g_variant_ref_sink (g_variant_new_string ("a")));
  Variant second (g_variant_ref_sink (g_variant_new_string ("bb")));
  std::vector<GVariant *> members (9000);
  for (gsize i = 0; i < members.size (); ++i) members[i] = i & 1 ? first.get () : second.get ();
  compare (g_variant_new_tuple (members.data (), members.size ()));
}
void byte_snapshot_lifetime ()
{
  const guint count = storage_live_files ();
  const guint8 source[] = {0, 255, 32, 1, 77};
  GInputStream *input = g_memory_input_stream_new_from_data (source, sizeof source, nullptr);
  auto data = snapshot_stream (input, nullptr); g_object_unref (input);
  Variant bytes (g_variant_ref_sink (byte_variant (data.get ()))); data.reset ();
  g_assert_cmpuint (storage_live_files (), ==, count + 1);
  compare (g_variant_ref (bytes.get ()));
  bytes.reset (); g_assert_cmpuint (storage_live_files (), ==, count);
  input = g_memory_input_stream_new ();
  data = snapshot_stream (input, nullptr); g_object_unref (input);
  g_assert_cmpuint (g_bytes_get_size (data.get ()), ==, 0); data.reset ();
  g_assert_cmpuint (storage_live_files (), ==, count);
}
struct Reentry { Bytes *target, *source; GBytes *replacement; guint calls = 0; };
void reenter (gpointer opaque)
{
  auto *state = static_cast<Reentry *> (opaque); ++state->calls;
  g_assert_null (state->source->get ());
  g_assert_cmpmem (g_bytes_get_data (state->target->get (), nullptr), 3, "new", 3);
  *state->target = Bytes::retain (state->replacement);
}
void move_and_finalizer_reentry ()
{
  auto replacement = Bytes::adopt (g_bytes_new_static ("reentered", 9));
  Bytes target, source = Bytes::adopt (g_bytes_new_static ("new", 3));
  Reentry state {&target, &source, replacement.get ()};
  target = Bytes::adopt (g_bytes_new_with_free_func ("old", 3, reenter, &state));
  target = std::move (source);
  g_assert_cmpuint (state.calls, ==, 1); g_assert_null (source.get ());
  g_assert_true (g_bytes_equal (target.get (), replacement.get ()));
  auto *alias = &target;
  target = std::move (*alias); g_assert_true (g_bytes_equal (target.get (), replacement.get ()));
  source = target; target.reset (); g_assert_true (g_bytes_equal (source.get (), replacement.get ()));
}
void mapping_discard_ownership ()
{
  std::vector<guint8> original (3 * 65536, 0x71);
  auto heap = Bytes::adopt (g_bytes_new (original.data (), original.size ()));
  discard_snapshot_pages (g_bytes_get_data (heap.get (), nullptr), original.size ());
  g_assert_cmpmem (g_bytes_get_data (heap.get (), nullptr), original.size (), original.data (), original.size ());
  auto *input = g_memory_input_stream_new_from_bytes (heap.get ());
  auto file = snapshot_stream (input, nullptr); g_object_unref (input);
  const auto address = reinterpret_cast<std::uintptr_t> (g_bytes_get_data (file.get (), nullptr));
  auto retained = file;
  std::thread reader ([retained] {
    gsize size; const auto *data = static_cast<const guint8 *> (g_bytes_get_data (retained.get (), &size));
    for (guint pass = 0; pass < 16; ++pass)
      {
        discard_snapshot_pages (data, size);
        for (gsize i = 0; i < size; i += 4096) g_assert_cmpuint (data[i], ==, 0x71);
      }
  });
  file.reset (); reader.join (); retained.reset ();
  g_assert_cmpuint (storage_live_files (), ==, 0);
#if defined(G_OS_UNIX) && defined(MAP_FIXED_NOREPLACE)
  /* If the address is reused by foreign anonymous storage, a stale mapping
   * identity would zero these pages. Never replace an existing live mapping. */
  void *reused = mmap (reinterpret_cast<void *> (address), original.size (), PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
  if (reused != MAP_FAILED)
    {
      std::memset (reused, 0x39, original.size ());
      discard_snapshot_pages (reused, original.size ());
      const auto *data = static_cast<const guint8 *> (reused);
      for (gsize i = 0; i < original.size (); ++i) g_assert_cmpuint (data[i], ==, 0x39);
      g_assert_cmpint (munmap (reused, original.size ()), ==, 0);
    }
#else
  (void) address;
#endif
}
template<class F> void rejected (F function)
{
  const guint before = storage_live_files (); bool threw = false;
  try { function (); } catch (const std::runtime_error &) { threw = true; }
  g_assert_true (threw); g_assert_cmpuint (storage_live_files (), ==, before);
}
void failures ()
{
  Variant value (g_variant_ref_sink (g_variant_new_string ("long enough to exceed the small limits")));
  GCancellable *cancel = g_cancellable_new (); g_cancellable_cancel (cancel);
  rejected ([&] { snapshot_variant (prefix, sizeof prefix, value.get (), cancel); }); g_object_unref (cancel);
  StorageLimits small; small.bytes = 16;
  rejected ([&] { snapshot_variant (prefix, sizeof prefix, value.get (), nullptr, small); });
  Variant nested (g_variant_ref_sink (g_variant_new_variant (value.get ()))); small = {}; small.depth = 1;
  rejected ([&] { snapshot_variant (prefix, sizeof prefix, nested.get (), nullptr, small); });
  small = {}; small.values = 1;
  rejected ([&] { snapshot_variant (prefix, sizeof prefix, nested.get (), nullptr, small); });
  guint8 buffer[4] = {}; GOutputStream *output = g_memory_output_stream_new (buffer, sizeof buffer, nullptr, nullptr);
  rejected ([&] { stream_variant (value.get (), output, nullptr); }); g_object_unref (output);
  GInputStream *input = g_memory_input_stream_new_from_data ("longer than 4", 13, nullptr); small = {}; small.bytes = 4;
  rejected ([&] { snapshot_stream (input, nullptr, small); }); g_object_unref (input);
}
struct CountingOutput { GOutputStream parent; gsize largest, total; guint calls, cancel_at; GCancellable *cancel; };
struct CountingOutputClass { GOutputStreamClass parent; };
static GType counting_output_get_type ();
G_DEFINE_TYPE (CountingOutput, counting_output, G_TYPE_OUTPUT_STREAM)
gssize count_write (GOutputStream *stream, const void *, gsize count, GCancellable *, GError **)
{
  auto *self = reinterpret_cast<CountingOutput *> (stream);
  self->largest = std::max (self->largest, count); self->total += count;
  if (++self->calls == self->cancel_at) g_cancellable_cancel (self->cancel);
  return count;
}
void counting_output_class_init (CountingOutputClass *klass)
{ G_OUTPUT_STREAM_CLASS (klass)->write_fn = count_write; }
void counting_output_init (CountingOutput *) {}
void bounded_writes_and_midstream_cancel ()
{
  std::string data (4 * 65536 + 1, 'z');
  auto bytes = Bytes::adopt (g_bytes_new (data.data (), data.size ()));
  Variant value (g_variant_ref_sink (byte_variant (bytes.get ())));
  for (guint point = 0; point <= 5; ++point)
    {
      auto *output = reinterpret_cast<CountingOutput *> (g_object_new (counting_output_get_type (), nullptr));
      output->cancel = g_cancellable_new (); output->cancel_at = point;
      if (!point)
        {
          stream_variant (value.get (), G_OUTPUT_STREAM (output), output->cancel);
          g_assert_cmpuint (output->total, ==, data.size ()); g_assert_cmpuint (output->calls, ==, 5);
        }
      else
        {
          bool cancelled = false;
          try { stream_variant (value.get (), G_OUTPUT_STREAM (output), output->cancel); }
          catch (const StorageError& error)
            { g_assert_cmpuint (error.domain, ==, G_IO_ERROR); g_assert_cmpint (error.code, ==, G_IO_ERROR_CANCELLED); cancelled = true; }
          g_assert_true (cancelled); g_assert_cmpuint (output->calls, ==, point);
        }
      g_assert_cmpuint (output->largest, ==, 65536);
      g_object_unref (output->cancel); g_object_unref (output);
    }
}
}
int main (int argc, char **argv)
{
  g_test_init (&argc, &argv, nullptr);
  g_test_add_func ("/painter-xcf-storage/scalars_and_containers", scalar_and_container_types);
  g_test_add_func ("/painter-xcf-storage/offset_boundaries", offset_boundaries);
  g_test_add_func ("/painter-xcf-storage/full_depth_unknown_arrays", full_typed_depth_and_unknown_arrays);
  g_test_add_func ("/painter-xcf-storage/snapshot_lifetime", byte_snapshot_lifetime);
  g_test_add_func ("/painter-xcf-storage/finalizer_reentry", move_and_finalizer_reentry);
  g_test_add_func ("/painter-xcf-storage/mapping_discard_ownership", mapping_discard_ownership);
  g_test_add_func ("/painter-xcf-storage/failures", failures);
  g_test_add_func ("/painter-xcf-storage/bounded_cancel", bounded_writes_and_midstream_cancel);
  return g_test_run ();
}
