/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gegl.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
extern "C" {
#include "libgimpbase/gimpbase.h"
#include "core/core-types.h"
#include "core/gimpfilterlayer.h"
}
#include "xcf/painter-xcf-arguments.hpp"
#include "xcf/painter-xcf-multipart.hpp"
#include <cstring>
#include <memory>
#include <vector>
namespace {
using namespace GimpPainterXcf;
struct VariantFree { void operator() (GVariant *v) const { if (v) g_variant_unref (v); } };
using Variant = std::unique_ptr<GVariant, VariantFree>;
Variant arguments (GType type, GBytes *bytes)
{
  GVariantBuilder args; g_variant_builder_init (&args, G_VARIANT_TYPE ("a(sbv)"));
  g_variant_builder_add (&args, "(sbv)", g_type_name (type), FALSE,
                         g_variant_new ("(b@ay)", TRUE, byte_variant (bytes)));
  return Variant (g_variant_ref_sink (g_variant_builder_end (&args)));
}
void word (std::vector<guint8>& out, guint64 value, guint width)
{ for (guint i = width; i; --i) out.push_back ((value >> ((i - 1) * 8)) & 255); }
void write (GOutputStream *out, const void *data, gsize size)
{
  GError *error = nullptr;
  g_assert_true (g_output_stream_write_all (out, data, size, nullptr, nullptr, &error));
  g_assert_no_error (error);
}
void record (GOutputStream *out, GBytes *bytes)
{
  const guint32 header[] = {GUINT32_TO_BE (21), GUINT32_TO_BE (g_bytes_get_size (bytes))};
  write (out, header, sizeof header); write (out, g_bytes_get_data (bytes, nullptr), g_bytes_get_size (bytes));
}
}
/* A real minimal v11 XCF with one valid Filter capsule and a retained typed
 * array just beyond the old inline budget. All large bytes remain file-backed. */
extern "C" void painter_xcf_test_argument_resource_file (GimpImage *image, GVariant *base,
                                                          GBytes *large, GFile *file)
{
  using namespace GimpPainterXcf;
  g_assert_cmpuint (g_bytes_get_size (large), >, 256u * 1024u * 1024u);
  {
    auto value = arguments (G_TYPE_BYTES, large);
    auto *snapshot = decode_snapshot (value.get (), image);
    g_assert_nonnull (snapshot);
    const auto *scalar = gimp_filter_arguments_snapshot_peek_value (snapshot, 0);
    g_assert_nonnull (scalar);
    auto retained = Bytes::retain (static_cast<GBytes *> (g_value_get_boxed (scalar)));
    g_assert_cmpuint (g_bytes_get_size (retained.get ()), ==, g_bytes_get_size (large));
    g_assert_true (g_bytes_get_data (retained.get (), nullptr) == g_bytes_get_data (large, nullptr));
    gimp_filter_arguments_snapshot_free (snapshot); value.reset ();
    g_assert_true (g_bytes_get_data (retained.get (), nullptr) == g_bytes_get_data (large, nullptr));
  }
  auto typed = arguments (GIMP_TYPE_ARRAY, large);
  GVariantDict dict; g_variant_dict_init (&dict, base);
  g_variant_dict_insert_value (&dict, "arguments", typed.get ());
  g_variant_dict_insert_value (&dict, "has-arguments", g_variant_new_boolean (TRUE));
  Variant value (g_variant_ref_sink (g_variant_dict_end (&dict)));
  const guint8 magic[] = {'G','P','X','C','F',0,0,0,1,0,0,0};
  auto capsule = snapshot_variant (magic, sizeof magic, value.get (), nullptr);
  guint32 id = 0; g_assert_true (g_variant_lookup (value.get (), "id", "u", &id));
  const auto manifest = multipart_manifest ({OwnerClass::layer, id}, Namespace::item, capsule.get (), nullptr);
  auto declaration = multipart_manifest_record (manifest);
  auto first = multipart_chunk_record (manifest, 0, capsule.get ());
  auto last = multipart_chunk_record (manifest, manifest.count - 1, capsule.get ());
  const guint64 properties = 12 + 8 + g_bytes_get_size (declaration.get ()) +
    (manifest.count - 1) * (guint64 (8) + g_bytes_get_size (first.get ())) + 8 + g_bytes_get_size (last.get ()) + 8;
  const guint64 hierarchy = 62 + 18 + properties + 16;
  const guint64 level = hierarchy + 28, tile = level + 24;
  GError *error = nullptr;
  std::unique_ptr<GOutputStream, decltype (&g_object_unref)> output (
    G_OUTPUT_STREAM (g_file_replace (file, nullptr, FALSE, G_FILE_CREATE_NONE, nullptr, &error)), g_object_unref);
  g_assert_no_error (error); g_assert_nonnull (output);
  std::vector<guint8> header;
  const guint8 signature[] = "gimp xcf v011";
  header.insert (header.end (), signature, signature + sizeof signature);
  word (header, 1, 4); word (header, 1, 4); word (header, GIMP_RGB, 4); word (header, GIMP_PRECISION_U8_NON_LINEAR, 4);
  word (header, 0, 8); word (header, 62, 8); word (header, 0, 8); word (header, 0, 8);
  g_assert_cmpuint (header.size (), ==, 62);
  word (header, 1, 4); word (header, 1, 4); word (header, 1, 4); word (header, 2, 4); header.push_back ('L'); header.push_back (0);
  word (header, 20, 4); word (header, 4, 4); word (header, id, 4);
  write (output.get (), header.data (), header.size ());
  record (output.get (), declaration.get ());
  for (guint32 i = 0; i < manifest.count; ++i)
    { auto bytes = multipart_chunk_record (manifest, i, capsule.get ()); record (output.get (), bytes.get ()); }
  header.clear (); word (header, 0, 8); word (header, hierarchy, 8); word (header, 0, 8);
  word (header, 1, 4); word (header, 1, 4); word (header, 4, 4); word (header, level, 8); word (header, 0, 8);
  word (header, 1, 4); word (header, 1, 4); word (header, tile, 8); word (header, 0, 8);
  header.insert (header.end (), {9,8,7,255}); write (output.get (), header.data (), header.size ());
  g_assert_true (g_output_stream_close (output.get (), nullptr, &error)); g_assert_no_error (error);
}
