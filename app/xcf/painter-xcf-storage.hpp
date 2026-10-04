/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_XCF_STORAGE_HPP
#define GIMP_PAINTER_XCF_STORAGE_HPP
#include <gio/gio.h>
#include "../painter/bytes.hpp"
#include <cstdint>
#include <utility>
#include <vector>
#include <stdexcept>

namespace GimpPainterXcf {
struct StorageError : std::runtime_error
{
  StorageError (GQuark domain, gint code, const char *message)
    : std::runtime_error (message), domain (domain), code (code) {}
  GQuark domain;
  gint code;
};
using GimpPainter::Bytes;
struct StorageLimits
{
  guint64 bytes = guint64 (65536) * 1048576;
  /* The serializer does not impose semantic node/depth caps. Callers may
   * request resource limits explicitly; v1 typed argument bounds stay in the
   * argument codec. In particular one typed level spans several wire nodes. */
  gsize values = G_MAXSIZE;
  unsigned depth = G_MAXUINT;
};
/* All returned snapshots are immutable, file-backed and owned by GBytes.
 * prefix bytes are part of the capsule; its GVariant body is 8-byte aligned.
 * These C++ functions throw on I/O, cancellation, limits or invalid input. */
Bytes snapshot_stream (GInputStream *, GCancellable *, const StorageLimits& = {});
Bytes snapshot_parts (const std::vector<Bytes>&, gsize alignment_padding,
                      GCancellable *, const StorageLimits& = {});
Bytes snapshot_variant (const void *prefix, gsize prefix_size, GVariant *,
                        GCancellable *, const StorageLimits& = {});
void stream_variant (GVariant *, GOutputStream *, GCancellable *,
                     const StorageLimits& = {});
/* Primitive byte arrays may use the immutable snapshot without copying it. */
GVariant *byte_variant (GBytes *);
guint storage_live_files () noexcept;
/* Best-effort resident-page release, only for ranges wholly inside a live
 * read-only snapshot owned by this module. Foreign/heap GBytes are untouched.
 * The caller retains its GBytes/GVariant lease for this call. The mapping
 * mutex prevents unmap/reuse during the advisory operation. This changes
 * residency, never bytes, references or snapshot lifetime. */
void discard_snapshot_pages (const void *, gsize) noexcept;
}
#endif
