/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_FILTER_WIRE_HPP
#define GIMP_PAINTER_FILTER_WIRE_HPP
#include "filter-procedure.hpp"
#include <cstddef>
#include <vector>
namespace GimpPainter { namespace FilterWire {
constexpr std::size_t header_size = 24;
constexpr std::size_t metadata_limit = 64 * 1024;
constexpr std::size_t pixel_limit = 128 * 1024;
enum class Type : std::uint16_t { request = 1, input = 2, input_end = 3,
                                  output = 4, success = 5, failure = 6 };
struct Frame { Type type; std::uint64_t offset; std::vector<std::uint8_t> payload; };
std::vector<std::uint8_t> encode (const Frame&);
/* Parses only a complete header and rejects size/version/type before allocation. */
std::size_t payload_size (const std::uint8_t *header);
Frame decode (const std::uint8_t *bytes, std::size_t size);
Frame request (const FilterProcedureRequest&);
FilterProcedureRequest request (const Frame&);
Frame success (std::uint64_t bytes, FilterProcedureDisposition disposition);
/* Stateful exact-size, ordered result validation, independent of I/O. EOF and
 * successful child reap remain mandatory transport checks after finish(). */
class Result {
public:
  explicit Result (std::uint64_t bytes, bool raw_shadow = false, bool no_merge = false, std::size_t alignment = 4)
    : bytes_ (bytes), alignment_ (alignment), expected_ (no_merge ? FilterProcedureDisposition::no_merge :
                                raw_shadow ? FilterProcedureDisposition::shadow :
                                             FilterProcedureDisposition::merged) {}
  void accept (const Frame&);
  void finish () const;
  bool terminal () const noexcept { return terminal_; }
  FilterProcedureDisposition disposition () const noexcept { return disposition_; }
private:
  std::uint64_t bytes_, offset_ = 0;
  std::size_t alignment_ = 4;
  bool terminal_ = false;
  FilterProcedureDisposition expected_;
  FilterProcedureDisposition disposition_ = FilterProcedureDisposition::pending;
};
}}
#endif
