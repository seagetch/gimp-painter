/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 2026
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "painter-xcf-compat.hpp"

#include <cstring>
#include <limits>
#include <new>
#include <stdexcept>
#include <utility>

namespace gimp { namespace painter { namespace xcf {
namespace {

struct Failure { Diagnostic diagnostic; };
[[noreturn]] void fail (Status status, std::size_t offset, const char *message)
{
  throw Failure { { status, offset, message } };
}

struct Budget
{
  const Limits &limits;
  std::size_t records = 0, objects = 0, bytes = 0;
  void record (std::size_t at)
  {
    if (records >= limits.max_records)
      fail (Status::limit, at, "record budget exhausted");
    ++records;
  }
  void object (std::size_t at)
  {
    if (objects >= limits.max_objects)
      fail (Status::limit, at, "object budget exhausted");
    ++objects;
  }
  void scan (std::size_t count, std::size_t at)
  {
    if (count > limits.max_scanned_bytes - bytes)
      fail (Status::limit, at, "byte-work budget exhausted");
    bytes += count;
  }
};

class Cursor
{
public:
  Cursor (Bytes input, std::size_t first, std::size_t end, Budget &budget)
    : input_ (input), pos_ (first), end_ (end), budget_ (budget)
  {
    if ((! input.data && input.size) || first > end || end > input.size)
      fail (Status::malformed, first, "invalid input range");
  }
  std::size_t pos () const { return pos_; }
  std::size_t end () const { return end_; }
  void skip (std::size_t size)
  {
    if (size > end_ - pos_)
      fail (Status::truncated, pos_, "record exceeds available bytes");
    budget_.scan (size, pos_);
    pos_ += size;
  }
  std::uint32_t u32 ()
  {
    const std::size_t at = pos_;
    skip (4);
    const auto *p = input_.data + at;
    return (std::uint32_t (p[0]) << 24) | (std::uint32_t (p[1]) << 16) |
           (std::uint32_t (p[2]) << 8) | p[3];
  }
  std::uint64_t offset (unsigned width)
  {
    const std::uint64_t high = u32 ();
    return width == 8 ? (high << 32) | u32 () : high;
  }
  std::uint8_t u8 ()
  {
    const std::size_t at = pos_;
    skip (1);
    return input_.data[at];
  }
  WireString string ()
  {
    WireString s;
    s.encoded.offset = pos_;
    const auto length = u32 ();
    /* An impossible read is structural truncation, not evidence that the
     * candidate could succeed with a larger work budget. This matters when
     * standard scalar bits are examined as a legacy string length. */
    if (length > end_ - pos_)
      fail (Status::truncated, pos_ - 4, "string exceeds available bytes");
    if (length > budget_.limits.max_string_bytes)
      fail (Status::limit, pos_ - 4, "string budget exceeded");
    s.bytes = { pos_, length };
    skip (length);
    s.encoded.size = pos_ - s.encoded.offset;
    s.has_terminal_nul = length && input_.data[pos_ - 1] == 0;
    return s;
  }
private:
  Bytes input_;
  std::size_t pos_, end_;
  Budget &budget_;
};

Header header (Bytes input, Dialect dialect, Budget &budget)
{
  Cursor c (input, 0, input.size, budget);
  c.skip (14);
  if (std::memcmp (input.data, "gimp xcf ", 9) != 0 || input.data[13] != 0)
    fail (Status::malformed, 0, "not an XCF signature");
  Header h;
  if (std::memcmp (input.data + 9, "file", 4) == 0)
    h.version = 0;
  else
    {
      if (input.data[9] != 'v')
        fail (Status::malformed, 9, "invalid XCF version marker");
      for (unsigned i = 10; i != 13; ++i)
        {
          if (input.data[i] < '0' || input.data[i] > '9')
            fail (Status::malformed, i, "invalid XCF version digit");
          h.version = h.version * 10 + input.data[i] - '0';
        }
    }
  if (h.version > (dialect == Dialect::legacy_painter ? 4u : 23u))
    fail (Status::unsupported, 9, "version outside audited reader range");
  h.width = c.u32 (); h.height = c.u32 (); h.base_type = c.u32 ();
  if (h.base_type > 2)
    fail (Status::malformed, 22, "invalid image base type");
  /* Current GIMP repairs invalid image dimensions. Do not overrule it. */
  if (dialect == Dialect::legacy_painter &&
      (! h.width || ! h.height || h.width > 0x7fffffff || h.height > 0x7fffffff))
    fail (Status::malformed, 14, "invalid legacy image dimensions");
  h.has_precision = dialect == Dialect::standard && h.version >= 4;
  h.offset_bytes = dialect == Dialect::standard && h.version >= 11 ? 8 : 4;
  if (h.has_precision)
    {
      h.precision = c.u32 ();
      bool valid = false;
      if (h.version == 4)
        valid = h.precision <= 4;
      else if (h.version <= 6)
        valid = h.precision >= 100 && h.precision <= 550 && h.precision % 50 == 0;
      else
        {
          const std::uint32_t values[] = {
            100, 150, 175, 200, 250, 275, 300, 350, 375,
            500, 550, 575, 600, 650, 675, 700, 750, 775
          };
          for (auto value : values) valid = valid || h.precision == value;
        }
      if (! valid)
        fail (Status::malformed, 26, "invalid standard precision word");
    }
  h.properties_offset = c.pos ();
  return h;
}

enum class Context { image, layer, channel };

/* Fixed-width fields are consumed by both historical readers irrespective of
 * their declared lengths. Keep both lengths. Variable/unknown records retain
 * their declared framing; complex legacy recovery belongs to the app reader. */
std::size_t fixed_size (std::uint32_t tag, Context context, const Header &h,
                        Dialect dialect)
{
  const auto variable = std::numeric_limits<std::size_t>::max ();
  if (tag == 0) return 0;
  if (context == Context::image)
    {
      if (tag == 17) return 1;
      if (tag == 19) return 8;
      if (tag == 20 || tag == 22) return 4;
      return variable;
    }
  if (tag == 6 || tag == 8 || tag == 9 || tag == 20 || tag == 28) return 4;
  if (context == Context::layer)
    {
      if (tag == 2 || tag == 29) return 0;
      if (tag == 5) return h.offset_bytes;
      if (tag == 7 || (tag >= 10 && tag <= 13) || tag == 26 || tag == 31) return 4;
      if (tag == 15) return 8;
      if (dialect == Dialect::standard && (tag == 32 || tag == 33)) return 4;
    }
  else
    {
      if (tag == 3 || tag == 4) return 0;
      if (tag == 14) return 4;
      if (tag == 16) return 3;
      if (dialect == Dialect::standard && (tag == 32 || tag == 33)) return 4;
    }
  return variable;
}

void properties (Cursor &c, Bytes input, Context context, Candidate &result,
                 Budget &budget, std::vector<Property> &records)
{
  while (true)
    {
      budget.record (c.pos ());
      Property p;
      p.encoded.offset = c.pos ();
      p.tag = c.u32 (); p.declared_size = c.u32 ();
      p.payload.offset = c.pos ();
      if (p.declared_size > budget.limits.max_property_bytes)
        fail (Status::limit, p.encoded.offset + 4, "property budget exceeded");
      if (context == Context::image && p.tag == 24)
        {
          /* Legacy writer's ?: precedence gives a wrong outer length. Both
           * readers actually consume factor, digits, then five strings. */
          c.skip (8);
          const unsigned count = result.header.version < 21 ? 5 : 3;
          for (unsigned i = 0; i < count; ++i) c.string ();
        }
      else if (context == Context::image && p.tag == 1)
        {
          const auto n = c.u32 ();
          if (n > 256) fail (Status::malformed, c.pos () - 4, "invalid colormap size");
          c.skip (n * (result.header.version == 0 ? 1u : 3u));
        }
      else
        {
          const auto fixed = fixed_size (p.tag, context, result.header, result.dialect);
          c.skip (fixed == std::numeric_limits<std::size_t>::max () ? p.declared_size : fixed);
        }
      p.payload.size = c.pos () - p.payload.offset;
      p.encoded.size = c.pos () - p.encoded.offset;
      p.length_mismatch = p.declared_size != p.payload.size;
      if (context == Context::layer && (p.tag == 32 || p.tag == 33))
        result.has_colliding_fields = true;
      if (context == Context::layer && p.tag == 7)
        {
          Cursor mode (input, p.payload.offset, c.pos (), budget);
          if (mode.u32 () >= 23) result.has_colliding_fields = true;
        }
      records.push_back (p);
      if (p.tag == 0) return;
    }
}

void check_offset (std::uint64_t offset, Bytes input, std::size_t at)
{
  if (offset && (offset >= input.size || input.size - std::size_t (offset) < 4))
    fail (Status::malformed, at, "referenced offset is outside the input");
}

std::vector<std::uint64_t> offset_table (Cursor &c, unsigned width,
                                        Bytes input, Budget &budget, Range &span)
{
  span.offset = c.pos ();
  std::vector<std::uint64_t> offsets;
  while (true)
    {
      budget.record (c.pos ());
      const auto at = c.pos ();
      const auto offset = c.offset (width);
      if (! offset) break;
      budget.object (at);
      check_offset (offset, input, at);
      offsets.push_back (offset);
    }
  span.size = c.pos () - span.offset;
  return offsets;
}

ObjectRecord object (Bytes input, std::size_t offset, Context context,
                     Candidate &result, Budget &budget)
{
  Cursor c (input, offset, input.size, budget);
  ObjectRecord o;
  o.metadata.offset = offset;
  o.width = c.u32 (); o.height = c.u32 ();
  if (context == Context::layer)
    {
      o.image_type = c.u32 ();
      if (o.image_type > 5)
        fail (Status::malformed, c.pos () - 4, "invalid layer image type");
    }
  o.name = c.string ();
  properties (c, input, context, result, budget, o.properties);
  auto at = c.pos ();
  o.hierarchy_offset = c.offset (result.header.offset_bytes);
  check_offset (o.hierarchy_offset, input, at);
  if (context == Context::layer)
    {
      at = c.pos ();
      o.mask_offset = c.offset (result.header.offset_bytes);
      check_offset (o.mask_offset, input, at);
    }
  o.metadata.size = c.pos () - offset;
  return o;
}

bool unresolved (Status s)
{
  return s == Status::limit || s == Status::needs_recovery ||
         s == Status::allocation_failure;
}

Extension extension (Bytes input, Range span, bool clone, FilterPolicy policy,
                     const Limits &limits) noexcept
{
  Extension result;
  result.encoded = span;
  result.tail = span;
  try
    {
      if (span.offset > input.size || span.size > input.size - span.offset)
        fail (Status::truncated, span.offset, "extension exceeds input");
      if (span.size > limits.max_property_bytes)
        fail (Status::limit, span.offset, "extension budget exceeded");
      Budget budget { limits };
      Cursor c (input, span.offset, span.offset + span.size, budget);
      result.name = c.string ();
      result.tail = { c.pos (), c.end () - c.pos () };
      while (true)
        {
          budget.record (c.pos ());
          Argument a;
          a.encoded.offset = c.pos ();
          a.tag = c.u32 (); a.declared_size = c.u32 ();
          a.payload.offset = c.pos ();
          if (a.tag == 0)
            a.kind = ArgumentKind::terminator;
          else if (clone)
            a.kind = ArgumentKind::unknown; /* old clone reader ignores size */
          else
            switch (a.tag)
              {
              case 1: a.kind = ArgumentKind::unsupported; break;
              case 2: a.kind = ArgumentKind::int32_bits; a.scalar_bits = c.u32 (); break;
              case 3: a.kind = ArgumentKind::int16_writer_32_bits; a.scalar_bits = c.u32 (); break;
              case 4: a.kind = ArgumentKind::int8_bits; a.scalar_bits = c.u8 (); break;
              case 5: a.kind = ArgumentKind::float32_bits; a.scalar_bits = c.u32 (); break;
              case 6: a.kind = ArgumentKind::omitted_array; break;
              case 7:
                a.kind = ArgumentKind::omitted_array;
                if (! a.declared_size) result.has_ambiguous_null_string = true;
                if (a.declared_size && policy == FilterPolicy::writer_string_recovery)
                  {
                    /* Writer uses tag7 followed directly by xcf_write_string:
                     * the word already read as size IS the string length. */
                    Cursor s (input, c.pos () - 4, c.end (), budget);
                    a.string_value = s.string ();
                    c.skip (a.string_value.bytes.size);
                    a.kind = ArgumentKind::recovered_writer_string;
                    result.used_writer_string_recovery = true;
                  }
                break;
              case 8:
                {
                  Cursor s (input, c.pos () - 4, c.end (), budget);
                  a.string_value = s.string ();
                  c.skip (a.string_value.bytes.size);
                  a.kind = ArgumentKind::string;
                  break;
                }
              case 9: a.kind = ArgumentKind::omitted_rgb; break;
              case 10: a.kind = ArgumentKind::omitted_drawable; break;
              default: a.kind = ArgumentKind::unknown; break;
              }
          a.payload.size = c.pos () - a.payload.offset;
          a.encoded.size = c.pos () - a.encoded.offset;
          a.length_mismatch = a.payload.size != a.declared_size;
          result.arguments.push_back (a);
          result.tail = { c.pos (), c.end () - c.pos () };
          if (! a.tag)
            {
              if (result.tail.size)
                result.diagnostic = { Status::needs_recovery, c.pos (),
                                      "bytes remain after extension terminator" };
              return result;
            }
        }
    }
  catch (const Failure &f) { result.diagnostic = f.diagnostic; }
  catch (const std::bad_alloc &) { result.diagnostic = { Status::allocation_failure, span.offset, "allocation failed" }; }
  catch (const std::length_error &) { result.diagnostic = { Status::limit, span.offset, "container capacity exceeded" }; }
  return result;
}

} // namespace

Candidate decode_candidate (Bytes input, Dialect dialect, const Limits &limits) noexcept
{
  Candidate result;
  result.dialect = dialect;
  try
    {
      Budget budget { limits };
      result.header = header (input, dialect, budget);
      Cursor c (input, result.header.properties_offset, input.size, budget);
      properties (c, input, Context::image, result, budget, result.image_properties);
      const auto layer_offsets = offset_table (c, result.header.offset_bytes, input,
                                               budget, result.layer_offset_table);
      const auto channel_offsets = offset_table (c, result.header.offset_bytes, input,
                                                 budget, result.channel_offset_table);
      for (auto offset : layer_offsets)
        result.layers.push_back (object (input, std::size_t (offset), Context::layer, result, budget));
      for (auto offset : channel_offsets)
        result.channels.push_back (object (input, std::size_t (offset), Context::channel, result, budget));
    }
  catch (const Failure &f) { result.diagnostic = f.diagnostic; }
  catch (const std::bad_alloc &) { result.diagnostic = { Status::allocation_failure, 0, "allocation failed" }; }
  catch (const std::length_error &) { result.diagnostic = { Status::limit, 0, "container capacity exceeded" }; }
  return result;
}

Probe probe (Bytes input, const Limits &limits) noexcept
{
  Probe result;
  result.legacy = decode_candidate (input, Dialect::legacy_painter, limits);
  result.standard = decode_candidate (input, Dialect::standard, limits);
  const auto a = result.legacy.diagnostic.status;
  const auto b = result.standard.diagnostic.status;
  if (unresolved (a) || unresolved (b))
    result.selection = Selection::inconclusive;
  else if (a == Status::ok && b == Status::ok)
    result.selection = result.legacy.header.version < 4 &&
                       ! result.legacy.has_colliding_fields &&
                       ! result.standard.has_colliding_fields ?
                       Selection::shared_layout : Selection::ambiguous;
  else if (a == Status::ok) result.selection = Selection::legacy_candidate;
  else if (b == Status::ok) result.selection = Selection::standard_candidate;
  else if (a == Status::unsupported && b == Status::unsupported)
    result.selection = Selection::unsupported;
  else result.selection = Selection::invalid;
  return result;
}

LegacyMode decode_legacy_mode (std::uint32_t raw) noexcept
{
  if (raw <= 22) return LegacyMode::common_0_to_22;
  switch (raw)
    {
    case 23: return LegacyMode::erase;
    case 24: return LegacyMode::replace;
    case 25: return LegacyMode::anti_erase;
    case 26: return LegacyMode::src_in;
    case 27: return LegacyMode::dst_in;
    case 28: return LegacyMode::src_out;
    case 29: return LegacyMode::dst_out;
    default: return LegacyMode::unknown;
    }
}

Extension decode_filter (Bytes input, Range payload, FilterPolicy policy,
                         const Limits &limits) noexcept
{
  return extension (input, payload, false, policy, limits);
}
Extension decode_clone (Bytes input, Range payload, const Limits &limits) noexcept
{
  return extension (input, payload, true, FilterPolicy::legacy_reader, limits);
}

} } } // namespace gimp::painter::xcf
