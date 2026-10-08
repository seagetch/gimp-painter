/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 2026
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef GIMP_PAINTER_XCF_COMPAT_HPP
#define GIMP_PAINTER_XCF_COMPAT_HPP

#include "../painter/gimp-painter-visibility.h"
#include <cstddef>
#include <cstdint>
#include <vector>

namespace gimp { namespace painter { namespace xcf GIMP_PAINTER_PRIVATE {

/* A read-only, caller-owned contiguous input. No pointer is retained in the
 * result: all records are absolute byte ranges into this exact input. Keep it
 * alive and unchanged while inspecting or persisting those ranges. */
struct Bytes { const std::uint8_t *data; std::size_t size; };
struct Range { std::size_t offset = 0; std::size_t size = 0; };

enum class Status { ok, malformed, truncated, limit, unsupported,
                    needs_recovery, allocation_failure };
struct Diagnostic
{
  Status status = Status::ok;
  std::size_t offset = 0;
  const char *message = "ok"; /* static string; never input-controlled */
};
struct Limits
{
  std::size_t max_records = 100000;
  std::size_t max_objects = 16384;
  std::size_t max_string_bytes = 1048576;
  std::size_t max_property_bytes = 67108864;
  std::size_t max_scanned_bytes = 268435456;
};

struct WireString
{
  Range encoded;                 /* length word plus all original bytes */
  Range bytes;                   /* includes on-disk NUL, if any */
  bool has_terminal_nul = false; /* never fix/UTF-8-normalize the input */
};
struct Property
{
  std::uint32_t tag = 0;
  std::uint32_t declared_size = 0;
  Range encoded;
  Range payload;
  bool length_mismatch = false;
};
struct ObjectRecord
{
  Range metadata;
  std::uint32_t width = 0, height = 0, image_type = 0;
  WireString name;
  std::vector<Property> properties; /* ordered, duplicates and END retained */
  std::uint64_t hierarchy_offset = 0, mask_offset = 0;
};
enum class Dialect { legacy_painter, standard };
struct Header
{
  unsigned version = 0;
  std::uint32_t width = 0, height = 0, base_type = 0, precision = 0;
  bool has_precision = false;
  unsigned offset_bytes = 4;
  std::size_t properties_offset = 0;
};
struct Candidate
{
  Dialect dialect = Dialect::standard;
  Diagnostic diagnostic;
  Header header;
  std::vector<Property> image_properties;
  Range layer_offset_table, channel_offset_table;
  std::vector<ObjectRecord> layers, channels;
  bool has_colliding_fields = false;
};
enum class Selection
{
  legacy_candidate, standard_candidate, shared_layout, ambiguous,
  inconclusive, invalid, unsupported
};
struct Probe
{
  Selection selection = Selection::invalid;
  Candidate legacy, standard;
};

/* Metadata-only structural candidates, NOT proof of pixel decodability and NOT
 * permission to select a dialect/create GObjects. Inconclusive/recovery and
 * ambiguous results must not be discarded in favour of the other candidate. */
Probe probe (Bytes input, const Limits &limits = Limits ()) noexcept;
Candidate decode_candidate (Bytes input, Dialect dialect,
                            const Limits &limits = Limits ()) noexcept;

enum class LegacyMode
{
  common_0_to_22, erase, replace, anti_erase,
  src_in, dst_in, src_out, dst_out, unknown
};
/* Returned semantic identities deliberately are NOT GimpLayerMode values. */
LegacyMode decode_legacy_mode (std::uint32_t raw) noexcept;

enum class ArgumentKind
{
  terminator, unsupported, int32_bits, int16_writer_32_bits,
  int8_bits, float32_bits, string, omitted_array, omitted_rgb,
  omitted_drawable, unknown, recovered_writer_string
};
struct Argument
{
  std::uint32_t tag = 0, declared_size = 0, scalar_bits = 0;
  Range encoded, payload;
  ArgumentKind kind = ArgumentKind::unknown;
  WireString string_value;
  bool length_mismatch = false;
};
enum class FilterPolicy
{
  legacy_reader, /* exact byte consumption, including broken scalar GValues */
  writer_string_recovery /* separate suggestion; never selected by probe */
};
struct Extension
{
  Diagnostic diagnostic;
  Range encoded;  /* complete outer payload survives any nested failure */
  WireString name;
  std::vector<Argument> arguments; /* END retained; original order preserved */
  Range tail; /* unconsumed/trailing bytes, including a failed argument */
  bool used_writer_string_recovery = false;
  bool has_ambiguous_null_string = false;
};
Extension decode_filter (Bytes input, Range payload,
                         FilterPolicy policy = FilterPolicy::legacy_reader,
                         const Limits &limits = Limits ()) noexcept;
Extension decode_clone (Bytes input, Range payload,
                        const Limits &limits = Limits ()) noexcept;

} } } // namespace gimp::painter::xcf
#endif
