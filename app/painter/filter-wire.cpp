/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "filter-wire.hpp"
#include <algorithm>
#include <cstring>
#include <stdexcept>
namespace GimpPainter { namespace FilterWire {
namespace {
std::uint32_t native_pixel_encoding () {
  const std::uint16_t word = 1;
  return *reinterpret_cast<const std::uint8_t *> (&word) ? 1 : 2;
}
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
      if (offset || size != 344) throw std::invalid_argument ("Invalid Filter request frame");
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
  if (std::memcmp (h, "GPF5", 4) || get (h + 4, 2) != 5 || get (h + 20, 4))
    throw std::invalid_argument ("Invalid Filter wire version or reserved field");
  const auto size = std::size_t (get (h + 8, 4));
  validate (Type (get (h + 6, 2)), get (h + 12, 8), size); return size;
}
std::vector<std::uint8_t> encode (const Frame& f) {
  validate (f.type, f.offset, f.payload.size ());
  std::vector<std::uint8_t> b (header_size + f.payload.size (), 0);
  std::memcpy (b.data (), "GPF5", 4); put (b, 4, 5, 2); put (b, 6, std::uint16_t (f.type), 2);
  put (b, 8, f.payload.size (), 4); put (b, 12, f.offset, 8);
  std::copy (f.payload.begin (), f.payload.end (), b.begin () + header_size); return b;
}
Frame decode (const std::uint8_t *b, std::size_t size) {
  if (size < header_size || payload_size (b) != size - header_size)
    throw std::invalid_argument ("Truncated or trailing Filter frame bytes");
  return {Type (get (b + 6, 2)), get (b + 12, 8), {b + header_size, b + size}};
}
Frame request (const FilterProcedureRequest& r) {
  r.bytes (); Frame f {Type::request, 0, std::vector<std::uint8_t> (344)};
  put (f.payload, 0, std::uint32_t (r.procedure), 4); put (f.payload, 4, r.width, 4); put (f.payload, 8, r.height, 4);
  put (f.payload, 12, std::uint32_t (r.angle), 4); put (f.payload, 16, std::uint32_t (r.segments), 4);
  put (f.payload, 20, std::uint32_t (r.orientation), 4); put (f.payload, 24, std::uint32_t (r.transparent), 4);
  put (f.payload, 28, r.gray ? 1 : 0, 4); std::copy (r.background.begin (), r.background.end (), f.payload.begin () + 32);
  const auto region = r.execution_region ();
  put (f.payload, 36, (r.raw_shadow ? 1 : 0) | (region.selected ? 2 : 0) | (region.intersects ? 4 : 0), 4);
  put (f.payload, 40, std::uint32_t (region.x1), 4); put (f.payload, 44, std::uint32_t (region.y1), 4);
  put (f.payload, 48, std::uint32_t (region.x2), 4); put (f.payload, 52, std::uint32_t (region.y2), 4);
  put (f.payload, 56, std::uint32_t (r.tiles), 4);
  put (f.payload, 60, r.storage_channels, 4);
  put (f.payload, 64, std::uint32_t (r.scale), 4);
  put (f.payload, 68, std::uint32_t (r.nscales), 4);
  put (f.payload, 72, std::uint32_t (r.scales_mode), 4);
  static_assert (sizeof (double) == 8 && std::numeric_limits<double>::is_iec559, "IEEE754 binary64 required");
  std::uint64_t cvar; std::memcpy (&cvar, &r.cvar, 8); put (f.payload, 80, cvar, 8);
  put (f.payload, 88, r.sample_mode, 4);
  put (f.payload, 92, std::uint32_t (r.alpha_alg), 4);
  put (f.payload, 96, std::uint32_t (r.border), 4);
  put (f.payload, 100, r.sample_mode ? native_pixel_encoding () : 0, 4);
  const auto real = [&] (std::size_t at, double value) {
    std::uint64_t bits; std::memcpy (&bits, &value, 8); put (f.payload, at, bits, 8);
  };
  real (104, r.divisor); real (112, r.offset);
  for (unsigned i = 0; i < 25; ++i) real (120 + i * 8, r.matrix[i]);
  for (unsigned i = 0; i < 5; ++i) put (f.payload, 320 + i * 4, std::uint32_t (r.channels[i]), 4);
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
  if ((flags & ~std::uint64_t (7)) || get (b + 76, 4)) throw std::invalid_argument ("Invalid Filter request flags");
  r.raw_shadow = flags & 1; r.start_region.selected = flags & 2; r.start_region.intersects = flags & 4;
  r.start_region.x1 = std::int32_t (std::uint32_t (get (b + 40, 4)));
  r.start_region.y1 = std::int32_t (std::uint32_t (get (b + 44, 4)));
  r.start_region.x2 = std::int32_t (std::uint32_t (get (b + 48, 4)));
  r.start_region.y2 = std::int32_t (std::uint32_t (get (b + 52, 4)));
  if (!r.start_region.selected && (!r.start_region.intersects || r.start_region.x1 || r.start_region.y1 ||
      r.start_region.x2 != std::int64_t (r.width) || r.start_region.y2 != std::int64_t (r.height)))
    throw std::invalid_argument ("Noncanonical unselected Filter region");
  r.tiles = std::int32_t (std::uint32_t (get (b + 56, 4)));
  r.storage_channels = get (b + 60, 4);
  r.scale = std::int32_t (std::uint32_t (get (b + 64, 4)));
  r.nscales = std::int32_t (std::uint32_t (get (b + 68, 4)));
  r.scales_mode = std::int32_t (std::uint32_t (get (b + 72, 4)));
  const std::uint64_t cvar = get (b + 80, 8); std::memcpy (&r.cvar, &cvar, 8);
  if (get (b + 340, 4))
    throw std::invalid_argument ("Invalid Convolution reserved fields");
  r.sample_mode = get (b + 88, 4);
  if (get (b + 100, 4) != (r.sample_mode ? native_pixel_encoding () : 0))
    throw std::invalid_argument ("Filter native pixel byte order differs from this host");
  r.alpha_alg = std::int32_t (std::uint32_t (get (b + 92, 4)));
  r.border = std::int32_t (std::uint32_t (get (b + 96, 4)));
  const auto real = [&] (std::size_t at) {
    const std::uint64_t bits = get (b + at, 8); double value; std::memcpy (&value, &bits, 8); return value;
  };
  r.divisor = real (104); r.offset = real (112);
  for (unsigned i = 0; i < 25; ++i) r.matrix[i] = real (120 + i * 8);
  for (unsigned i = 0; i < 5; ++i) r.channels[i] = std::int32_t (std::uint32_t (get (b + 320 + i * 4, 4)));
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
    if ((alignment_ != 4 && alignment_ != 32) || f.offset % alignment_ || f.payload.size () % alignment_ ||
        f.offset != offset_ || f.payload.size () > bytes_ - offset_) throw std::invalid_argument ("Noncontiguous or oversized Filter result");
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
