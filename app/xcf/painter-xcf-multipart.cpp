/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "painter-xcf-multipart.hpp"
#include <algorithm>
#include <cstring>
#include <memory>
#include <set>
#include <stdexcept>
#include <utility>
namespace GimpPainterXcf {
namespace {
const guint8 manifest_magic[12] = {'G','P','X','C','F',0,0,0,2,0,0,0};
const guint8 chunk_magic[12] = {'G','P','X','C','C',0,0,0,1,0,0,0};
const guint8 logical_magic[12] = {'G','P','X','C','F',0,0,0,1,0,0,0};
const char chunk_prefix[] = "gimp-painter-chunk-v1/";
const char inert_prefix[] = "gimp-painter-inert-v1/";
const guint8 inert_magic[12] = {'G','P','X','C','I',0,0,0,1,0,0,0};
constexpr gsize manifest_size = 76, chunk_header_size = 84;
constexpr guint32 persistent = 1;
[[noreturn]] void fail (const char *message) { throw std::runtime_error (message); }
void cancelled (GCancellable *cancel)
{ if (cancel && g_cancellable_is_cancelled (cancel)) throw StorageError (G_IO_ERROR, G_IO_ERROR_CANCELLED, "Painter metadata operation was cancelled"); }
void word (std::vector<guint8>& bytes, guint64 value, unsigned count, bool little = true)
{
  for (unsigned i = 0; i < count; ++i) bytes.push_back ((value >> (8 * (little ? i : count - i - 1))) & 255);
}
guint64 read (const guint8 *bytes, unsigned count, bool little = true)
{
  guint64 value = 0;
  for (unsigned i = 0; i < count; ++i) value |= guint64 (bytes[little ? i : count - i - 1]) << (8 * i);
  return value;
}
std::string hex (const std::array<guint8, 32>& digest)
{
  static const char digits[] = "0123456789abcdef"; std::string result;
  result.reserve (64); for (guint8 v : digest) { result += digits[v >> 4]; result += digits[v & 15]; } return result;
}
std::string chunk_name (const MultipartManifest& manifest, guint32 index)
{
  gchar *suffix = g_strdup_printf ("%u/%s/%08x", guint32 (manifest.space), hex (manifest.digest).c_str (), index);
  std::string result = std::string (chunk_prefix) + suffix; g_free (suffix); return result;
}
bool valid_identity (OwnerIdentity owner, Namespace space)
{
  if (guint32 (owner.type) > guint32 (OwnerClass::path) || guint32 (space) > 2) return false;
  if (owner.type == OwnerClass::image) return !owner.id && space != Namespace::item;
  return owner.id && space != Namespace::image;
}
std::vector<guint8> header (const MultipartManifest& manifest, const guint8 *magic)
{
  std::vector<guint8> bytes (magic, magic + 12);
  word (bytes, guint32 (manifest.owner.type), 4); word (bytes, manifest.owner.id, 4);
  word (bytes, guint32 (manifest.space), 4); word (bytes, multipart_block_size, 4);
  word (bytes, manifest.size, 8); word (bytes, manifest.count, 4); word (bytes, 0, 4);
  bytes.insert (bytes.end (), manifest.digest.begin (), manifest.digest.end ()); return bytes;
}
bool parse_header (const guint8 *data, gsize size, const guint8 *magic, MultipartManifest& result)
{
  if (size < manifest_size || std::memcmp (data, magic, 12)) return false;
  result.owner = {OwnerClass (read (data + 12, 4)), guint32 (read (data + 16, 4))};
  result.space = Namespace (read (data + 20, 4)); result.size = read (data + 28, 8);
  result.count = read (data + 36, 4); std::copy (data + 44, data + 76, result.digest.begin ());
  return valid_identity (result.owner, result.space) && read (data + 24, 4) == multipart_block_size &&
         !read (data + 40, 4) && result.size >= sizeof logical_magic &&
         result.size <= guint64 (multipart_block_size) * multipart_max_chunks &&
         result.count == (result.size + multipart_block_size - 1) / multipart_block_size;
}
bool same (const MultipartManifest& a, const MultipartManifest& b)
{
  return a.owner.type == b.owner.type && a.owner.id == b.owner.id && a.space == b.space &&
         a.size == b.size && a.count == b.count && a.digest == b.digest;
}
Bytes frame (const char *name, const std::vector<guint8>& data)
{
  const gsize name_size = std::strlen (name) + 1;
  std::vector<guint8> bytes; bytes.reserve (name_size + 12 + data.size ());
  word (bytes, name_size, 4, false); bytes.insert (bytes.end (), name, name + name_size);
  word (bytes, persistent, 4, false); word (bytes, data.size (), 4, false);
  bytes.insert (bytes.end (), data.begin (), data.end ());
  return Bytes::adopt (g_bytes_new (bytes.data (), bytes.size ()));
}
struct Record
{
  const char *name = nullptr;
  const guint8 *data = nullptr;
  gsize size = 0, offset = 0;
  guint32 flags = 0;
};
bool record (GBytes *bytes, Record& result)
{
  gsize size; const auto *data = static_cast<const guint8 *> (g_bytes_get_data (bytes, &size));
  if (size < 13) return false;
  const gsize name_size = read (data, 4, false);
  if (!name_size || name_size > size - 12 || data[3 + name_size] || std::memchr (data + 4, 0, name_size - 1)) return false;
  result.name = reinterpret_cast<const char *> (data + 4); result.flags = read (data + 4 + name_size, 4, false);
  result.size = read (data + 8 + name_size, 4, false); result.offset = 12 + name_size;
  if (result.size != size - result.offset) return false;
  result.data = data + result.offset; return true;
}
using Checksum = std::unique_ptr<GChecksum, decltype (&g_checksum_free)>;
void checksum_bytes (GChecksum *sum, GBytes *bytes, GCancellable *cancel)
{
  gsize size; const auto *data = static_cast<const guint8 *> (g_bytes_get_data (bytes, &size));
  while (size)
    { cancelled (cancel); gsize part = std::min (size, gsize (multipart_block_size)); g_checksum_update (sum, data, part); discard_snapshot_pages (data, part); data += part; size -= part; }
  cancelled (cancel);
}
std::array<guint8, 32> digest (GChecksum *sum)
{ std::array<guint8, 32> result; gsize size = result.size (); g_checksum_get_digest (sum, result.data (), &size); return result; }
}
const char *namespace_name (Namespace space)
{
  switch (space) { case Namespace::image: return "gimp-painter-image"; case Namespace::item: return "gimp-painter-item";
                   case Namespace::origin: return "gimp-painter-origin"; } fail ("Invalid Painter namespace");
}
bool reserved_chunk_name (const char *name) noexcept
{ return name && !std::strncmp (name, chunk_prefix, sizeof chunk_prefix - 1); }
bool reserved_inert_name (const char *name) noexcept
{ return name && !std::strncmp (name, inert_prefix, sizeof inert_prefix - 1); }
namespace {
using Digest = std::array<guint8, 32>;
Digest record_digest (GBytes *bytes, GCancellable *cancel)
{
  Checksum sum (g_checksum_new (G_CHECKSUM_SHA256), g_checksum_free);
  checksum_bytes (sum.get (), bytes, cancel); return digest (sum.get ());
}
bool inert_marker (GBytes *bytes, Digest& identity)
{
  Record raw;
  if (!record (bytes, raw) || !reserved_inert_name (raw.name) || raw.flags != persistent ||
      raw.size != sizeof inert_magic + identity.size () || std::memcmp (raw.data, inert_magic, sizeof inert_magic)) return false;
  std::copy (raw.data + sizeof inert_magic, raw.data + raw.size, identity.begin ());
  return raw.name == std::string (inert_prefix) + hex (identity);
}
std::set<Digest> inert_identities (const std::vector<Bytes>& records)
{
  std::set<Digest> result;
  for (const auto& bytes : records) { Digest identity; if (inert_marker (bytes.get (), identity)) result.insert (identity); }
  return result;
}
}
std::vector<bool> multipart_protected_records (const std::vector<Bytes>& records, GCancellable *cancel)
{
  const auto identities = inert_identities (records);
  std::vector<bool> result (records.size (), false);
  if (identities.empty ()) return result;
  for (gsize i = 0; i < records.size (); ++i)
    {
      Record raw;
      if (record (records[i].get (), raw) && !reserved_inert_name (raw.name))
        result[i] = identities.count (record_digest (records[i].get (), cancel));
    }
  return result;
}
std::vector<Bytes> multipart_inert_markers (const std::vector<Bytes>& records, GCancellable *cancel)
{
  auto identities = inert_identities (records); std::vector<Bytes> result;
  for (const auto& bytes : records)
    {
      Record raw;
      /* Carrier records are intrinsically inactive and never wrap carriers. */
      if (record (bytes.get (), raw) && reserved_inert_name (raw.name)) continue;
      const auto identity = record_digest (bytes.get (), cancel);
      if (!identities.insert (identity).second) continue;
      std::vector<guint8> data (inert_magic, inert_magic + sizeof inert_magic);
      data.insert (data.end (), identity.begin (), identity.end ());
      result.push_back (frame ((std::string (inert_prefix) + hex (identity)).c_str (), data));
    }
  return result;
}
MultipartManifest multipart_manifest (OwnerIdentity owner, Namespace space, GBytes *capsule, GCancellable *cancel)
{
  if (!capsule || !valid_identity (owner, space)) fail ("Invalid Painter multipart identity");
  MultipartManifest result; result.owner = owner; result.space = space; result.size = g_bytes_get_size (capsule);
  if (result.size < sizeof logical_magic || result.size > guint64 (multipart_block_size) * multipart_max_chunks ||
      std::memcmp (g_bytes_get_data (capsule, nullptr), logical_magic, sizeof logical_magic)) fail ("Invalid Painter multipart logical capsule");
  result.count = (result.size + multipart_block_size - 1) / multipart_block_size;
  Checksum sum (g_checksum_new (G_CHECKSUM_SHA256), g_checksum_free); checksum_bytes (sum.get (), capsule, cancel);
  result.digest = digest (sum.get ()); return result;
}
Bytes multipart_manifest_record (const MultipartManifest& manifest)
{ return frame (namespace_name (manifest.space), header (manifest, manifest_magic)); }
Bytes multipart_chunk_record (const MultipartManifest& manifest, guint32 index, GBytes *capsule)
{
  if (!capsule || g_bytes_get_size (capsule) != manifest.size || index >= manifest.count) fail ("Invalid Painter multipart chunk source");
  const guint64 offset = guint64 (index) * multipart_block_size;
  const gsize count = std::min (guint64 (multipart_block_size), manifest.size - offset);
  auto bytes = header (manifest, chunk_magic); word (bytes, index, 4); word (bytes, 0, 4);
  const auto *data = static_cast<const guint8 *> (g_bytes_get_data (capsule, nullptr)) + offset;
  bytes.insert (bytes.end (), data, data + count); discard_snapshot_pages (data, count);
  return frame (chunk_name (manifest, index).c_str (), bytes);
}
MultipartResult multipart_restore (OwnerIdentity owner, const std::vector<Bytes>& records, GCancellable *cancel)
{
  MultipartResult result; result.consumed.resize (records.size (), false);
  if (records.size () > 6 * (gsize (multipart_max_chunks) + 1)) fail ("Painter multipart record-work limit exceeded");
  const auto protected_records = multipart_protected_records (records, cancel);
  std::array<MultipartManifest, 3> manifests;
  std::array<std::vector<std::pair<guint32, gsize>>, 3> entries;
  std::array<gsize, 3> manifest_index {{0,0,0}};
  /* First pass establishes an unambiguous owner-local manifest for each
   * namespace; physical order and PROP_PARASITES grouping carry no authority. */
  for (gsize i = 0; i < records.size (); ++i)
    {
      cancelled (cancel); Record raw; if (!record (records[i].get (), raw)) continue;
      for (guint n = 0; n < 3; ++n)
        if (!std::strcmp (raw.name, namespace_name (Namespace (n))))
          {
            if (result.present[n] || protected_records[i]) result.invalid[n] = true;
            result.present[n] = true; manifest_index[n] = i;
            if (raw.flags != persistent || raw.size != manifest_size ||
                !parse_header (raw.data, raw.size, manifest_magic, manifests[n]) ||
                manifests[n].space != Namespace (n) || manifests[n].owner.type != owner.type || manifests[n].owner.id != owner.id)
              result.invalid[n] = true;
          }
    }
  for (gsize i = 0; i < records.size (); ++i)
    {
      cancelled (cancel); Record raw; if (!record (records[i].get (), raw) || !reserved_chunk_name (raw.name)) continue;
      const char *suffix = raw.name + sizeof chunk_prefix - 1;
      if (suffix[0] < '0' || suffix[0] > '2' || suffix[1] != '/') continue;
      const guint n = suffix[0] - '0';
      MultipartManifest chunk; const guint32 index = raw.size >= chunk_header_size ? read (raw.data + 76, 4) : 0;
      if (protected_records[i] || !result.present[n] || raw.flags != persistent || !parse_header (raw.data, raw.size, chunk_magic, chunk) ||
          raw.size < chunk_header_size || read (raw.data + 80, 4) || !same (chunk, manifests[n]) ||
          index >= chunk.count || raw.name != chunk_name (chunk, index) ||
          raw.size - chunk_header_size != std::min (guint64 (multipart_block_size), chunk.size - guint64 (index) * multipart_block_size))
        { result.invalid[n] = true; continue; }
      entries[n].emplace_back (index, i);
    }
  for (guint n = 0; n < 3; ++n)
    {
      if (!result.present[n] || result.invalid[n]) continue;
      auto& set = entries[n]; const auto& manifest = manifests[n];
      if (set.size () != manifest.count) { result.invalid[n] = true; continue; }
      std::sort (set.begin (), set.end ());
      std::vector<Bytes> parts; parts.reserve (set.size ()); Checksum sum (g_checksum_new (G_CHECKSUM_SHA256), g_checksum_free);
      for (gsize i = 0; i < set.size (); ++i)
        {
          if (set[i].first != i) { result.invalid[n] = true; break; }
          Record raw; record (records[set[i].second].get (), raw);
          parts.emplace_back (Bytes::adopt (g_bytes_new_from_bytes (records[set[i].second].get (), raw.offset + chunk_header_size, raw.size - chunk_header_size)));
          checksum_bytes (sum.get (), parts.back ().get (), cancel);
        }
      if (result.invalid[n] || digest (sum.get ()) != manifest.digest) { result.invalid[n] = true; continue; }
      auto capsule = snapshot_parts (parts, 4, cancel);
      if (g_bytes_get_size (capsule.get ()) != manifest.size || std::memcmp (g_bytes_get_data (capsule.get (), nullptr), logical_magic, sizeof logical_magic))
        { result.invalid[n] = true; continue; }
      result.capsules[n] = std::move (capsule); result.consumed[manifest_index[n]] = true;
      for (const auto& entry : set) result.consumed[entry.second] = true;
    }
  return result;
}
}
