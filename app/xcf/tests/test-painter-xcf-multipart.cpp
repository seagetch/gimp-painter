/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../painter-xcf-multipart.hpp"
#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <vector>
using namespace GimpPainterXcf;
namespace {
Bytes capsule (gsize size)
{
  const guint8 magic[] = {'G','P','X','C','F',0,0,0,1,0,0,0};
  std::vector<guint8> bytes (size);
  for (gsize i = 0; i < size; ++i) bytes[i] = (i * 17 + i / 65536) & 255;
  std::copy (magic, magic + sizeof magic, bytes.begin ()); return Bytes::adopt (g_bytes_new (bytes.data (), bytes.size ()));
}
std::vector<Bytes> records (OwnerIdentity owner, Namespace space, GBytes *payload)
{
  const auto manifest = multipart_manifest (owner, space, payload, nullptr);
  std::vector<Bytes> result; result.push_back (multipart_manifest_record (manifest));
  for (guint i = 0; i < manifest.count; ++i) result.push_back (multipart_chunk_record (manifest, i, payload));
  return result;
}
Bytes changed (GBytes *original, gsize at, guint8 value)
{
  gsize size; auto *data = static_cast<const guint8 *> (g_bytes_get_data (original, &size));
  std::vector<guint8> copy (data, data + size); g_assert_cmpuint (at, <, size); copy[at] = value;
  return Bytes::adopt (g_bytes_new (copy.data (), copy.size ()));
}
void identities_and_reordering ()
{
  for (guint type = 0; type <= 4; ++type)
    for (guint space = 0; space < 3; ++space)
      {
        if ((type == 0 && space == 1) || (type != 0 && space == 0)) continue;
        OwnerIdentity owner {OwnerClass (type), type ? 177u : 0u};
        auto payload = capsule (3 * multipart_block_size + 17); auto wire = records (owner, Namespace (space), payload.get ());
        for (guint order = 0; order < 3; ++order)
          {
            if (order == 1) std::reverse (wire.begin (), wire.end ());
            if (order == 2) std::rotate (wire.begin (), wire.begin () + 2, wire.end ());
            const auto before = wire; auto parsed = multipart_restore (owner, wire, nullptr);
            g_assert_true (parsed.present[space]); g_assert_false (parsed.invalid[space]);
            g_assert_true (g_bytes_equal (parsed.capsules[space].get (), payload.get ()));
            for (gsize i = 0; i < wire.size (); ++i)
              { g_assert_true (parsed.consumed[i]); g_assert_true (g_bytes_equal (before[i].get (), wire[i].get ()) ); }
          }
      }
  g_assert_cmpuint (storage_live_files (), ==, 0);
}
void invalid_sets_stay_inert ()
{
  OwnerIdentity owner {OwnerClass::layer, 12}; const guint n = guint (Namespace::item);
  auto payload = capsule (2 * multipart_block_size + 1); const auto original = records (owner, Namespace::item, payload.get ());
  for (guint damage = 0; damage < 20; ++damage)
    {
      auto wire = original; OwnerIdentity actual = owner;
      switch (damage)
        {
        case 0: wire.erase (wire.begin () + 1); break; // missing
        case 1: wire.push_back (wire[1]); break; // duplicate index
        case 2: wire.push_back (wire[0]); break; // duplicate manifest
        case 3: actual.id = 13; break; // foreign native owner
        case 4: actual.type = OwnerClass::channel; break;
        case 5: wire[1] = changed (wire[1].get (), g_bytes_get_size (wire[1].get ()) - 1, 0xff); break; // wrong digest
        case 6: wire[0] = changed (wire[0].get (), g_bytes_get_size (wire[0].get ()) - 76 + 8, 99); break; // future format
        case 7: wire[1] = changed (wire[1].get (), 4 + std::strlen ("gimp-painter-chunk-v1/") , '2'); break; // namespace/name mismatch
        case 8: { auto other = capsule (2 * multipart_block_size + 2); auto other_wire = records (owner, Namespace::item, other.get ()); wire[1] = other_wire[1]; break; }
        case 9: wire[0] = changed (wire[0].get (), g_bytes_get_size (wire[0].get ()) - 76 + 24, 1); break; // block size
        case 10: wire[0] = changed (wire[0].get (), g_bytes_get_size (wire[0].get ()) - 76 + 40, 1); break; // reserved
        case 11: wire[0] = changed (wire[0].get (), g_bytes_get_size (wire[0].get ()) - 76 + 35, 0xff); break; // >64 GiB
        case 12: wire[0] = changed (wire[0].get (), g_bytes_get_size (wire[0].get ()) - 76 + 38, 0xff); break; // chunk count bound
        case 13: { const gsize at = 12 + std::strlen ("gimp-painter-chunk-v1/1/") + 64 + 1 + 8 + 1;
                   wire[1] = changed (wire[1].get (), at + 76, 0xff); break; } // out-of-range index
        case 14: { const gsize at = 12 + std::strlen ("gimp-painter-chunk-v1/1/") + 64 + 1 + 8 + 1;
                   wire[1] = changed (wire[1].get (), at + 80, 1); break; } // reserved chunk word
        case 15: { const gsize name = 4 + std::strlen ("gimp-painter-chunk-v1/1/") + 64 + 1 + 8 + 1;
                   wire[1] = changed (wire[1].get (), name + 3, 3); break; } // flags
        case 16: wire[1] = Bytes::adopt (g_bytes_new_from_bytes (wire[1].get (), 0, g_bytes_get_size (wire[1].get ()) - 1)); break;
        case 17: { const gsize at = 12 + std::strlen ("gimp-painter-chunk-v1/1/") + 64 + 1 + 8 + 1;
                   wire[1] = changed (wire[1].get (), at + 20, 2); break; } // payload namespace
        case 18: { const gsize at = 12 + std::strlen ("gimp-painter-chunk-v1/1/") + 64 + 1 + 8 + 1;
                   wire[1] = changed (wire[1].get (), at + 16, 13); break; } // payload owner
        case 19: wire[1] = changed (wire[1].get (), 4 + std::strlen ("gimp-painter-chunk-v1/1/") + 64 + 1 + 8, 'x'); break; // name terminator
        }
      const auto before = wire; auto parsed = multipart_restore (actual, wire, nullptr);
      g_assert_true (parsed.present[n]); g_assert_true (parsed.invalid[n]); g_assert_false (bool (parsed.capsules[n]));
      for (gsize i = 0; i < wire.size (); ++i)
        { g_assert_false (parsed.consumed[i]); g_assert_true (g_bytes_equal (before[i].get (), wire[i].get ())); }
    }
  g_assert_cmpuint (storage_live_files (), ==, 0);
}
void inert_carriers_survive_identity_changes ()
{
  OwnerIdentity owner {OwnerClass::layer, 12}; auto payload = capsule (65537);
  auto wire = records (owner, Namespace::item, payload.get ());
  auto foreign = multipart_restore ({OwnerClass::layer, 13}, wire, nullptr);
  g_assert_true (foreign.invalid[1]);
  auto carriers = multipart_inert_markers (wire, nullptr);
  g_assert_cmpuint (carriers.size (), ==, wire.size ());
  const auto original = wire;
  wire.insert (wire.end (), carriers.begin (), carriers.end ());
  for (guint order = 0; order < 3; ++order)
    {
      if (order) std::reverse (wire.begin (), wire.end ());
      auto restored = multipart_restore (owner, wire, nullptr);
      g_assert_true (restored.invalid[1]); g_assert_false (bool (restored.capsules[1]));
      for (bool consumed : restored.consumed) g_assert_false (consumed);
      g_assert_true (multipart_inert_markers (wire, nullptr).empty ());
    }
  wire.push_back (carriers[0]);
  g_assert_true (multipart_inert_markers (wire, nullptr).empty ());
  g_assert_true (multipart_restore (owner, wire, nullptr).invalid[1]);
  /* A missing, mismatching or truncated carrier grants no authority. A
   * separately valid original set still has its ordinary validation rules. */
  for (guint damage = 0; damage < 3; ++damage)
    {
      auto input = original;
      if (damage == 1) input.push_back (changed (carriers[0].get (), g_bytes_get_size (carriers[0].get ()) - 1, 0xff));
      if (damage == 2) input.push_back (Bytes::adopt (g_bytes_new_from_bytes (carriers[0].get (), 0, g_bytes_get_size (carriers[0].get ()) - 1)));
      auto restored = multipart_restore (owner, input, nullptr);
      g_assert_false (restored.invalid[1]); g_assert_true (bool (restored.capsules[1]));
    }
}
void multiple_namespaces_and_bounds ()
{
  OwnerIdentity owner {OwnerClass::image, 0}; auto first = capsule (12), second = capsule (65536);
  auto wire = records (owner, Namespace::image, first.get ()); auto extra = records (owner, Namespace::origin, second.get ());
  wire.insert (wire.end (), extra.begin (), extra.end ()); std::reverse (wire.begin (), wire.end ());
  { auto parsed = multipart_restore (owner, wire, nullptr);
    g_assert_true (g_bytes_equal (parsed.capsules[0].get (), first.get ()));
    g_assert_true (g_bytes_equal (parsed.capsules[2].get (), second.get ()));
    g_assert_false (parsed.present[1]); }
  GCancellable *cancel = g_cancellable_new (); g_cancellable_cancel (cancel); bool threw = false;
  try { multipart_restore (owner, wire, cancel); } catch (const std::runtime_error &) { threw = true; }
  g_assert_true (threw); g_object_unref (cancel); g_assert_cmpuint (storage_live_files (), ==, 0);
  for (gsize size : {gsize (12), gsize (65535), gsize (65536), gsize (65537), gsize (131071), gsize (131072), gsize (131073)})
    {
      auto bytes = capsule (size); auto manifest = multipart_manifest (owner, Namespace::image, bytes.get (), nullptr);
      g_assert_cmpuint (manifest.count, ==, (size + 65535) / 65536);
      auto sequence = records (owner, Namespace::image, bytes.get ()); auto parsed = multipart_restore (owner, sequence, nullptr);
      g_assert_true (g_bytes_equal (bytes.get (), parsed.capsules[0].get ()));
    }
}
}
int main (int argc, char **argv)
{
  g_test_init (&argc, &argv, nullptr);
  g_test_add_func ("/painter-xcf-multipart/identity_and_order", identities_and_reordering);
  g_test_add_func ("/painter-xcf-multipart/inert_invalid", invalid_sets_stay_inert);
  g_test_add_func ("/painter-xcf-multipart/namespaces_and_bounds", multiple_namespaces_and_bounds);
  g_test_add_func ("/painter-xcf-multipart/inert-carrier-identity", inert_carriers_survive_identity_changes);
  return g_test_run ();
}
