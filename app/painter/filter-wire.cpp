/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "filter-wire.hpp"
#include <algorithm>
#include <cstring>
#include <stdexcept>
namespace GimpPainter { namespace FilterWire {
namespace {
void put (std::vector<std::uint8_t>& b, std::size_t at, std::uint64_t n, unsigned count) {
  for (unsigned i = 0; i < count; ++i) b[at + i] = std::uint8_t (n >> (8 * i));
}
std::uint64_t get (const std::uint8_t *b, unsigned count) {
  std::uint64_t n = 0; for (unsigned i = 0; i < count; ++i) n |= std::uint64_t (b[i]) << (8 * i); return n;
}
void validate (Type type, std::uint64_t offset, std::size_t size) {
  switch (type) {
    case Type::input: case Type::output:
      if (!size || size > pixel_limit || size % 4 || offset % 4) throw std::invalid_argument ("Invalid Filter pixel frame");
      break;
    case Type::request:
      if (offset || size != 64) throw std::invalid_argument ("Invalid Filter request frame");
      break;
    case Type::input_end:
      if (size) throw std::invalid_argument ("Invalid Filter terminal frame");
      break;
    case Type::success:
      if (size != 4) throw std::invalid_argument ("Invalid Filter success metadata");
      break;
    case Type::failure:
      if (offset || size > metadata_limit) throw std::invalid_argument ("Invalid Filter failure frame");
      break;
    default: throw std::invalid_argument ("Unknown Filter frame type");
  }
}
}
std::size_t payload_size (const std::uint8_t *h) {
  if (std::memcmp (h, "GPF3", 4) || get (h + 4, 2) != 3 || get (h + 20, 4))
    throw std::invalid_argument ("Invalid Filter wire version or reserved field");
  const auto size = std::size_t (get (h + 8, 4));
  validate (Type (get (h + 6, 2)), get (h + 12, 8), size); return size;
}
std::vector<std::uint8_t> encode (const Frame& f) {
  validate (f.type, f.offset, f.payload.size ());
  std::vector<std::uint8_t> b (header_size + f.payload.size (), 0);
  std::memcpy (b.data (), "GPF3", 4); put (b, 4, 3, 2); put (b, 6, std::uint16_t (f.type), 2);
  put (b, 8, f.payload.size (), 4); put (b, 12, f.offset, 8);
  std::copy (f.payload.begin (), f.payload.end (), b.begin () + header_size); return b;
}
Frame decode (const std::uint8_t *b, std::size_t size) {
  if (size < header_size || payload_size (b) != size - header_size)
    throw std::invalid_argument ("Truncated or trailing Filter frame bytes");
  return {Type (get (b + 6, 2)), get (b + 12, 8), {b + header_size, b + size}};
}
Frame request (const FilterProcedureRequest& r) {
  r.bytes (); Frame f {Type::request, 0, std::vector<std::uint8_t> (64)};
  put (f.payload, 0, std::uint32_t (r.procedure), 4); put (f.payload, 4, r.width, 4); put (f.payload, 8, r.height, 4);
  put (f.payload, 12, std::uint32_t (r.angle), 4); put (f.payload, 16, std::uint32_t (r.segments), 4);
  put (f.payload, 20, std::uint32_t (r.orientation), 4); put (f.payload, 24, std::uint32_t (r.transparent), 4);
  put (f.payload, 28, r.gray ? 1 : 0, 4); std::copy (r.background.begin (), r.background.end (), f.payload.begin () + 32);
  const auto region = r.execution_region ();
  put (f.payload, 36, (r.raw_shadow ? 1 : 0) | (region.selected ? 2 : 0) | (region.intersects ? 4 : 0), 4);
  put (f.payload, 40, std::uint32_t (region.x1), 4); put (f.payload, 44, std::uint32_t (region.y1), 4);
  put (f.payload, 48, std::uint32_t (region.x2), 4); put (f.payload, 52, std::uint32_t (region.y2), 4);
  put (f.payload, 56, std::uint32_t (r.tiles), 4);
  return f;
}
FilterProcedureRequest request (const Frame& f) {
  if (f.type != Type::request) throw std::invalid_argument ("Missing Filter request");
  validate (f.type, f.offset, f.payload.size ()); const auto *b = f.payload.data ();
  FilterProcedureRequest r; r.procedure = FilterProcedure (get (b, 4)); r.width = get (b + 4, 4); r.height = get (b + 8, 4);
  r.angle = std::int32_t (std::uint32_t (get (b + 12, 4))); r.segments = std::int32_t (std::uint32_t (get (b + 16, 4)));
  r.orientation = std::int32_t (std::uint32_t (get (b + 20, 4))); r.transparent = std::int32_t (std::uint32_t (get (b + 24, 4)));
  if (get (b + 28, 4) > 1) throw std::invalid_argument ("Invalid Filter sample type");
  r.gray = get (b + 28, 4) != 0; std::copy (b + 32, b + 36, r.background.begin ());
  const auto flags = get (b + 36, 4);
  if ((flags & ~std::uint64_t (7)) || get (b + 60, 4)) throw std::invalid_argument ("Invalid Filter request flags");
  r.raw_shadow = flags & 1; r.start_region.selected = flags & 2; r.start_region.intersects = flags & 4;
  r.start_region.x1 = std::int32_t (std::uint32_t (get (b + 40, 4)));
  r.start_region.y1 = std::int32_t (std::uint32_t (get (b + 44, 4)));
  r.start_region.x2 = std::int32_t (std::uint32_t (get (b + 48, 4)));
  r.start_region.y2 = std::int32_t (std::uint32_t (get (b + 52, 4)));
  if (!r.start_region.selected && (!r.start_region.intersects || r.start_region.x1 || r.start_region.y1 ||
      r.start_region.x2 != std::int64_t (r.width) || r.start_region.y2 != std::int64_t (r.height)))
    throw std::invalid_argument ("Noncanonical unselected Filter region");
  r.tiles = std::int32_t (std::uint32_t (get (b + 56, 4)));
  r.bytes (); return r;
}
Frame success (std::uint64_t bytes, FilterProcedureDisposition disposition) {
  if (disposition != FilterProcedureDisposition::merged && disposition != FilterProcedureDisposition::shadow &&
      disposition != FilterProcedureDisposition::no_merge) throw std::invalid_argument ("Invalid Filter completion disposition");
  Frame frame {Type::success,bytes,std::vector<std::uint8_t> (4)};
  put (frame.payload,0,std::uint32_t (disposition),4); return frame;
}
void Result::accept (const Frame& f) {
  validate (f.type, f.offset, f.payload.size ());
  if (terminal_) throw std::invalid_argument ("Filter data after terminal frame");
  if (f.type == Type::failure) throw std::runtime_error ("Private Filter procedure failed");
  if (f.type == Type::output) {
    if (f.offset != offset_ || f.payload.size () > bytes_ - offset_) throw std::invalid_argument ("Noncontiguous or oversized Filter result");
    offset_ += f.payload.size ();
  } else if (f.type == Type::success) {
    if (f.offset != bytes_ || offset_ != bytes_) throw std::invalid_argument ("Incomplete Filter result");
    const auto disposition = FilterProcedureDisposition (get (f.payload.data (),4));
    if (disposition != expected_)
      throw std::invalid_argument ("Unexpected Filter completion disposition");
    disposition_ = disposition; terminal_ = true;
  } else throw std::invalid_argument ("Unexpected Filter result frame");
}
void Result::finish () const {
  if (!terminal_ || offset_ != bytes_) throw std::invalid_argument ("Filter stream lacks successful terminal frame");
}
}}
