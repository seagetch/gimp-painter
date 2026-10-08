/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_XCF_MULTIPART_HPP
#define GIMP_PAINTER_XCF_MULTIPART_HPP
#include "../painter/gimp-painter-visibility.h"
#include "painter-xcf-storage.hpp"
#include <array>
#include <string>
#include <vector>
namespace GimpPainterXcf GIMP_PAINTER_PRIVATE {
enum class Namespace : guint32 { image, item, origin };
enum class OwnerClass : guint32 { image, layer, channel, layer_mask, path };
struct OwnerIdentity { OwnerClass type; guint32 id; };
struct MultipartManifest
{
  OwnerIdentity owner {OwnerClass::image, 0};
  Namespace space = Namespace::image;
  guint64 size = 0;
  guint32 count = 0;
  std::array<guint8, 32> digest {};
};
struct MultipartResult
{
  std::array<Bytes, 3> capsules;
  std::array<bool, 3> present {{false, false, false}};
  std::array<bool, 3> invalid {{false, false, false}};
  /* True only for records in a completely validated set; all other records
   * must be preserved byte-exact by the native writer, including duplicates. */
  std::vector<bool> consumed;
};
constexpr guint32 multipart_block_size = 65536;
constexpr guint32 multipart_max_chunks = 1048576;
const char *namespace_name (Namespace);
bool reserved_chunk_name (const char *) noexcept;
bool reserved_inert_name (const char *) noexcept;
/* Persist the inactive disposition independently of mutable native owner IDs.
 * Carriers contain only SHA-256 identities of exact original inner records. */
std::vector<bool> multipart_protected_records (const std::vector<Bytes>&, GCancellable *);
std::vector<Bytes> multipart_inert_markers (const std::vector<Bytes>& inert, GCancellable *);
MultipartManifest multipart_manifest (OwnerIdentity, Namespace, GBytes *, GCancellable *);
/* Complete inner XCF parasite framing, not a GimpParasite allocation. */
Bytes multipart_manifest_record (const MultipartManifest&);
Bytes multipart_chunk_record (const MultipartManifest&, guint32 index, GBytes *capsule);
MultipartResult multipart_restore (OwnerIdentity, const std::vector<Bytes>& records, GCancellable *);
}
#endif
