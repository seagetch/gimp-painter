/* SPDX-License-Identifier: GPL-3.0-or-later
 * GVariant wire layout follows GLib's public serialized-data contract:
 * https://docs.gtk.org/glib/struct.Variant.html#serialized-data-memory
 * This streams the same little-endian format; it introduces no value schema.
 */
#include "painter-xcf-storage.hpp"
#include <algorithm>
#include <atomic>
#include <cstring>
#include <memory>
#include <map>
#include <mutex>
#include <stdexcept>
#include <vector>
#ifdef G_OS_UNIX
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace GimpPainterXcf {
namespace {
constexpr gsize block_size = 65536;
std::atomic<guint> live_files {0};
struct Mappings
{
  std::mutex mutex;
  std::map<std::uintptr_t, gsize> ranges;
};
Mappings& mappings () { static Mappings result; return result; }
struct VariantFree { void operator() (GVariant *p) const { if (p) g_variant_unref (p); } };
using Variant = std::unique_ptr<GVariant, VariantFree>;
[[noreturn]] void fail (const char *message) { throw std::runtime_error (message); }
[[noreturn]] void io_failure (GError *error)
{
  StorageError exception (error ? error->domain : G_IO_ERROR,
                          error ? error->code : G_IO_ERROR_FAILED,
                          error ? error->message : "Painter metadata I/O failure");
  g_clear_error (&error); throw exception;
}
void cancelled (GCancellable *cancel)
{
  GError *error = nullptr;
  if (cancel && g_cancellable_set_error_if_cancelled (cancel, &error)) io_failure (error);
}
struct File
{
  GFile *file = nullptr;
  GFileIOStream *io = nullptr;
  GMappedFile *map = nullptr;
  bool counted = false;
  ~File ()
  {
    if (map)
      {
        { auto& known = mappings (); std::lock_guard<std::mutex> lock (known.mutex);
          known.ranges.erase (reinterpret_cast<std::uintptr_t> (g_mapped_file_get_contents (map))); }
        g_mapped_file_unref (map);
      }
    if (io) { g_io_stream_close (G_IO_STREAM (io), nullptr, nullptr); g_object_unref (io); }
    if (file) { g_file_delete (file, nullptr, nullptr); g_object_unref (file); }
    if (counted) --live_files;
  }
  GOutputStream *create ()
  {
    GError *error = nullptr;
    file = g_file_new_tmp ("gimp-xcf-metadata-XXXXXX", &io, &error);
    if (!file) io_failure (error);
    counted = true; ++live_files;
    return g_io_stream_get_output_stream (G_IO_STREAM (io));
  }
};
void free_file (gpointer p) { delete static_cast<File *> (p); }
class Writer
{
public:
  Writer (GOutputStream *out, GCancellable *cancel, guint64 limit)
    : output_ (out), cancel_ (cancel), limit_ (limit) {}
  guint64 position () const { return position_; }
  GCancellable *cancellable () const { return cancel_; }
  void write (const void *data, gsize size)
  {
    if (size > limit_ - position_) fail ("Painter metadata exceeds the storage work limit");
    const auto *p = static_cast<const guint8 *> (data);
    while (size)
      {
        cancelled (cancel_);
        const gsize part = std::min (size, block_size); GError *error = nullptr;
        if (!g_output_stream_write_all (output_, p, part, nullptr, cancel_, &error)) io_failure (error);
        discard_snapshot_pages (p, part);
        position_ += part; size -= part; p += part;
      }
    cancelled (cancel_);
  }
  void zero (gsize size)
  { const guint8 padding[8] = {}; if (size > sizeof padding) fail ("Invalid GVariant padding"); write (padding, size); }
  void align (guint64 base, unsigned alignment)
  { zero ((alignment - (position_ - base) % alignment) % alignment); }
  void offset (guint64 value, unsigned size)
  { guint8 bytes[8]; for (unsigned i = 0; i < size; ++i) { bytes[i] = value & 255; value >>= 8; } write (bytes, size); }
private:
  GOutputStream *output_;
  GCancellable *cancel_;
  guint64 limit_, position_ = 0;
};
struct Layout { unsigned alignment; guint64 fixed; };
Layout leaf_layout (const GVariantType *type)
{
  switch (*g_variant_type_peek_string (type))
    {
    case 'b': case 'y': return {1,1};
    case 'n': case 'q': return {2,2};
    case 'i': case 'u': case 'h': return {4,4};
    case 'x': case 't': case 'd': return {8,8};
    case 's': case 'o': case 'g': return {1,0};
    case 'v': return {8,0};
    case 'a': case 'm': case '(': case '{': return {0,0};
    default: fail ("Unsupported GVariant type in metadata snapshot");
    }
}
Layout layout (const GVariantType *type, unsigned depth)
{
  struct Frame { const GVariantType *type, *next; Layout result; bool fixed; };
  std::vector<Frame> stack;
  auto push = [&] (const GVariantType *item) {
    if (stack.size () >= depth) fail ("GVariant type exceeds the storage depth limit");
    auto result = leaf_layout (item); const char kind = *g_variant_type_peek_string (item);
    const auto *next = result.alignment ? nullptr : (kind == 'a' || kind == 'm') ?
                       g_variant_type_element (item) : g_variant_type_first (item);
    stack.push_back ({item, next, result.alignment ? result : Layout {1,0}, true});
  };
  push (type);
  for (;;)
    {
      auto& current = stack.back ();
      if (current.next)
        {
          const auto *child = current.next;
          const char kind = *g_variant_type_peek_string (current.type);
          current.next = (kind == 'a' || kind == 'm') ? nullptr : g_variant_type_next (child);
          push (child); continue;
        }
      auto result = current.result;
      const char kind = *g_variant_type_peek_string (current.type);
      if (kind == 'a' || kind == 'm') result.fixed = 0;
      else if (kind == '(' || kind == '{')
        result.fixed = current.fixed ? (result.fixed ? result.fixed +
                       (result.alignment - result.fixed % result.alignment) % result.alignment : 1) : 0;
      stack.pop_back ();
      if (stack.empty ()) return result;
      auto& parent = stack.back (); parent.result.alignment = std::max (parent.result.alignment, result.alignment);
      parent.fixed = parent.fixed && result.fixed;
      const guint64 padding = (result.alignment - parent.result.fixed % result.alignment) % result.alignment;
      if (result.fixed > G_MAXUINT64 - padding || parent.result.fixed > G_MAXUINT64 - padding - result.fixed)
        fail ("GVariant fixed layout overflows");
      parent.result.fixed += padding + result.fixed;
    }
}
unsigned offset_size (guint64 size)
{ return size <= G_MAXUINT8 ? 1 : size <= G_MAXUINT16 ? 2 : size <= G_MAXUINT32 ? 4 : 8; }
/* Variable-container offsets spill after one block. The reverse tuple order
 * must not require a heap table proportional to an unknown field's size. */
class Offsets
{
public:
  explicit Offsets (GCancellable *cancel) : cancel_ (cancel) {}
  void add (guint64 offset)
  {
    values_.push_back (offset); ++count_;
    if (values_.size () == block_size / sizeof (guint64)) flush ();
  }
  void emit (Writer& writer, unsigned width, bool reverse)
  {
    if (!file_) { emit_block (writer, width, reverse); return; }
    flush (); GError *error = nullptr;
    if (!g_output_stream_flush (g_io_stream_get_output_stream (G_IO_STREAM (file_->io)), cancel_, &error)) io_failure (error);
    auto *input = g_io_stream_get_input_stream (G_IO_STREAM (file_->io));
    guint64 remaining = count_, cursor = 0;
    while (remaining)
      {
        cancelled (cancel_);
        const gsize count = std::min (remaining, guint64 (block_size / sizeof (guint64)));
        const guint64 first = reverse ? remaining - count : cursor;
        if (first > G_MAXINT64 / sizeof (guint64)) fail ("GVariant offset spool is too large");
        if (!g_seekable_seek (G_SEEKABLE (file_->io), first * sizeof (guint64), G_SEEK_SET, cancel_, &error)) io_failure (error);
        values_.resize (count); gsize got = 0;
        if (!g_input_stream_read_all (input, values_.data (), count * sizeof (guint64), &got, cancel_, &error)) io_failure (error);
        if (got != count * sizeof (guint64)) fail ("GVariant offset spool is truncated");
        emit_block (writer, width, reverse); remaining -= count; cursor += count;
      }
  }
private:
  void flush ()
  {
    if (!file_) { file_.reset (new File); file_->create (); }
    GError *error = nullptr;
    if (!g_output_stream_write_all (g_io_stream_get_output_stream (G_IO_STREAM (file_->io)), values_.data (),
                                   values_.size () * sizeof (guint64), nullptr, cancel_, &error)) io_failure (error);
    values_.clear ();
  }
  void emit_block (Writer& writer, unsigned width, bool reverse)
  {
    if (reverse) for (auto i = values_.rbegin (); i != values_.rend (); ++i) writer.offset (*i, width);
    else for (auto i : values_) writer.offset (i, width);
  }
  GCancellable *cancel_;
  guint64 count_ = 0;
  std::vector<guint64> values_;
  std::unique_ptr<File> file_;
};
struct Encoder
{
  Writer& writer;
  const StorageLimits& limits;
  gsize values = 0;
  struct Frame
  {
    Variant value;
    Layout shape;
    guint64 start, size;
    gsize count = 0, next = 0;
    char kind;
    bool entered = false, waiting = false;
    std::unique_ptr<Offsets> offsets;
    Frame (GVariant *value, Layout shape, guint64 start, guint64 size, char kind)
      : value (value), shape (shape), start (start), size (size), kind (kind) {}
  };
  void encode (GVariant *value, unsigned depth)
  {
    std::vector<Frame> stack;
    auto push = [&] (GVariant *raw) {
      Variant owned (raw);
      cancelled (writer.cancellable ());
      if (stack.size () >= depth || values >= limits.values)
        fail ("GVariant exceeds the storage value/depth limit");
      ++values;
      const auto *type = g_variant_get_type (raw);
      const guint64 size = g_variant_get_size (raw);
      if (size > limits.bytes) fail ("GVariant exceeds the storage byte limit");
      const auto shape = layout (type, limits.depth);
      Frame next (owned.release (), shape, writer.position (), size, *g_variant_type_peek_string (type));
      stack.push_back (std::move (next));
    };
    push (g_variant_ref (value));
    while (!stack.empty ())
      {
        auto& frame = stack.back ();
        auto *current = frame.value.get ();
        const auto *type = g_variant_get_type (current);
        if (!frame.entered)
          {
            frame.entered = true;
            if (!g_variant_is_container (current))
              {
                if (frame.shape.fixed)
                  {
                    alignas (8) guint8 bytes[8] = {}; g_variant_store (current, bytes);
#if G_BYTE_ORDER == G_BIG_ENDIAN
                    std::reverse (bytes, bytes + frame.shape.fixed);
#endif
                    writer.write (bytes, frame.shape.fixed);
                  }
                else { gsize length; const char *text = g_variant_get_string (current, &length); writer.write (text, length + 1); }
              }
            else if (frame.kind == 'a' &&
                     std::strchr ("bynqiuhxtd", *g_variant_type_peek_string (g_variant_type_element (type))))
              {
                /* Primitive arrays already have contiguous immutable storage.
                 * Do not expand elements or serialize the enclosing dictionary. */
                const gsize width = layout (g_variant_type_element (type), limits.depth).fixed;
                gsize length; const auto *data = static_cast<const guint8 *> (g_variant_get_fixed_array (current, &length, width));
                if (length > limits.bytes / width) fail ("GVariant primitive array exceeds the storage byte limit");
#if G_BYTE_ORDER == G_BIG_ENDIAN
                alignas (8) guint8 block[block_size];
                while (length)
                  {
                    const gsize count = std::min (length, block_size / width);
                    for (gsize i = 0; i < count; ++i)
                      std::reverse_copy (data + i * width, data + (i + 1) * width, block + i * width);
                    writer.write (block, count * width); data += count * width; length -= count;
                  }
#else
                writer.write (data, length * width);
#endif
              }
            else
              {
                frame.count = g_variant_n_children (current);
                if (frame.count > limits.values - values) fail ("GVariant container exceeds the storage value limit");
                if (!frame.shape.fixed && frame.kind != 'v' && frame.kind != 'm')
                  frame.offsets.reset (new Offsets (writer.cancellable ()));
              }
          }
        if (frame.waiting)
          {
            Variant child (g_variant_get_child_value (current, frame.next - 1));
            if (frame.offsets && !layout (g_variant_get_type (child.get ()), limits.depth).fixed &&
                (frame.kind == 'a' || frame.next < frame.count))
              frame.offsets->add (writer.position () - frame.start);
            frame.waiting = false;
          }
        if (frame.next < frame.count)
          {
            Variant child (g_variant_get_child_value (current, frame.next++));
            if (frame.kind != 'v' && frame.kind != 'm')
              writer.align (frame.start, layout (g_variant_get_type (child.get ()), limits.depth).alignment);
            frame.waiting = true; push (child.release ()); continue;
          }
        if (frame.kind == 'v')
          {
            Variant child (g_variant_get_variant (current));
            writer.zero (1); const char *name = g_variant_get_type_string (child.get ()); writer.write (name, std::strlen (name));
          }
        else if (frame.kind == 'm' && frame.count)
          {
            Variant child (g_variant_get_maybe (current));
            if (!layout (g_variant_get_type (child.get ()), limits.depth).fixed) writer.zero (1);
          }
        else if (frame.kind == '(' || frame.kind == '{')
          {
            if (frame.shape.fixed)
              { if (!frame.count) writer.zero (1); else writer.align (frame.start, frame.shape.alignment); }
          }
        if (frame.offsets) frame.offsets->emit (writer, offset_size (frame.size), frame.kind != 'a');
        if (writer.position () - frame.start != frame.size) fail ("GVariant streamed framing disagrees with canonical size");
        stack.pop_back ();
      }
  }
};
Bytes seal (std::unique_ptr<File> file, gsize skip, GCancellable *cancel)
{
  cancelled (cancel); GError *error = nullptr;
  if (!g_output_stream_flush (g_io_stream_get_output_stream (G_IO_STREAM (file->io)), cancel, &error)) io_failure (error);
  /* A delayed temporary-file close error must fail preparation, before any
   * destination replacement. The immutable mapping needs no writable handle. */
  if (!g_io_stream_close (G_IO_STREAM (file->io), cancel, &error)) io_failure (error);
  g_clear_object (&file->io);
  gchar *path = g_file_get_path (file->file);
  file->map = g_mapped_file_new (path, FALSE, &error); g_free (path);
  if (!file->map) io_failure (error);
  { auto& known = mappings (); std::lock_guard<std::mutex> lock (known.mutex);
    const auto address = reinterpret_cast<std::uintptr_t> (g_mapped_file_get_contents (file->map));
    if (address) known.ranges.emplace (address, g_mapped_file_get_length (file->map)); }
  cancelled (cancel);
  if (g_file_delete (file->file, nullptr, nullptr)) g_clear_object (&file->file);
  const gsize size = g_mapped_file_get_length (file->map);
  if (skip > size) fail ("Invalid metadata snapshot alignment");
  const char *data = g_mapped_file_get_contents (file->map);
  auto result = Bytes::adopt (g_bytes_new_with_free_func (data ? data + skip : nullptr, size - skip, free_file, file.get ()));
  file.release (); return result;
}
}
Bytes snapshot_stream (GInputStream *input, GCancellable *cancel, const StorageLimits& limits)
{
  std::unique_ptr<File> file (new File); Writer writer (file->create (), cancel, limits.bytes);
  guint8 bytes[block_size];
  for (;;)
    {
      GError *error = nullptr; const gssize count = g_input_stream_read (input, bytes, sizeof bytes, cancel, &error);
      if (count < 0) io_failure (error);
      if (!count) break;
      writer.write (bytes, count);
    }
  return seal (std::move (file), 0, cancel);
}
Bytes snapshot_parts (const std::vector<Bytes>& parts, gsize padding,
                      GCancellable *cancel, const StorageLimits& limits)
{
  if (padding > 7 || limits.bytes > G_MAXUINT64 - padding) fail ("Invalid snapshot alignment or size");
  std::unique_ptr<File> file (new File); Writer writer (file->create (), cancel, limits.bytes + padding);
  writer.zero (padding);
  for (const auto& part : parts)
    {
      gsize size; const void *data = g_bytes_get_data (part.get (), &size);
      writer.write (data, size);
    }
  return seal (std::move (file), padding, cancel);
}
void stream_variant (GVariant *value, GOutputStream *output, GCancellable *cancel, const StorageLimits& limits)
{
  Writer writer (output, cancel, limits.bytes); Encoder encoder {writer, limits}; encoder.encode (value, limits.depth);
}
Bytes snapshot_variant (const void *prefix, gsize prefix_size, GVariant *value,
                        GCancellable *cancel, const StorageLimits& limits)
{
  if (prefix_size > limits.bytes || g_variant_get_size (value) > limits.bytes - prefix_size)
    fail ("Painter capsule exceeds the storage byte limit");
  const gsize padding = (8 - prefix_size % 8) % 8;
  if (limits.bytes > G_MAXUINT64 - padding) fail ("Invalid metadata storage limit");
  std::unique_ptr<File> file (new File); Writer writer (file->create (), cancel, limits.bytes + padding);
  writer.zero (padding); writer.write (prefix, prefix_size);
  Encoder encoder {writer, limits}; encoder.encode (value, limits.depth);
  return seal (std::move (file), padding, cancel);
}
GVariant *byte_variant (GBytes *bytes)
{
  if (!bytes) return g_variant_new_fixed_array (G_VARIANT_TYPE_BYTE, nullptr, 0, 1);
  return g_variant_new_from_bytes (G_VARIANT_TYPE_BYTESTRING, bytes, TRUE);
}
guint storage_live_files () noexcept { return live_files.load (); }
void discard_snapshot_pages (const void *data, gsize size) noexcept
{
#if defined(G_OS_UNIX) && defined(MADV_DONTNEED)
  if (!data || !size) return;
  const auto address = reinterpret_cast<std::uintptr_t> (data);
  auto& known = mappings (); std::lock_guard<std::mutex> lock (known.mutex);
  auto found = known.ranges.upper_bound (address);
  if (found == known.ranges.begin ()) return;
  --found;
  if (address - found->first > found->second || size > found->second - (address - found->first)) return;
  const long native_page = sysconf (_SC_PAGESIZE);
  if (native_page <= 0) return;
  const auto page = std::uintptr_t (native_page);
  const auto first = address - address % page;
  const auto last = address + size;
  const auto end = last + (page - last % page) % page;
  if (end < last || first < found->first) return;
  /* Every advised page is part of this read-only file mapping. Adjacent data
   * in the same page remains valid and can be faulted back in immediately. */
  madvise (reinterpret_cast<void *> (first), end - first, MADV_DONTNEED);
#else
  (void) data; (void) size;
#endif
}
}
