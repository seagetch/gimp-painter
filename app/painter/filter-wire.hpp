/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_FILTER_WIRE_HPP
#define GIMP_PAINTER_FILTER_WIRE_HPP
#include "filter-procedure.hpp"
#include "filter-progress.hpp"
#include <array>
#include <chrono>
#include <cstddef>
#include <functional>
#include <utility>
#include <vector>
namespace GimpPainter { namespace FilterWire {
constexpr std::size_t header_size = 24;
constexpr std::size_t metadata_limit = 64 * 1024;
constexpr std::size_t pixel_limit = 128 * 1024;
enum class Type : std::uint16_t { request = 1, input = 2, input_end = 3,
                                  output = 4, success = 5, failure = 6,
                                  progress_start = 7, progress_end = 8,
                                  progress_text = 9, progress_value = 10,
                                  progress_pulse = 11, progress_message = 12 };
struct Frame { Type type; std::uint64_t offset; std::vector<std::uint8_t> payload; };
std::vector<std::uint8_t> encode (const Frame&);
/* Parses only a complete header and rejects size/version/type before allocation. */
std::size_t payload_size (const std::uint8_t *header);
Frame decode (const std::uint8_t *bytes, std::size_t size);
Frame request (const FilterProcedureRequest&);
FilterProcedureRequest request (const Frame&);
Frame success (std::uint64_t bytes, FilterProcedureDisposition disposition);
bool is_progress (Type) noexcept;
Frame progress (FilterProgress::Event, const FilterProgress::Snapshot&);
void publish_progress (const Frame&, FilterProgress&);
/* Child-local emission budget, never measured from parent arrival times:
 * a suspended receiver must be able to drain an accumulated valid backlog. */
class ProgressBudget {
public:
  using Clock = std::chrono::steady_clock;
  static constexpr unsigned burst = 512, per_second = 200;
  explicit ProgressBudget (Clock::time_point now = Clock::now ()) : last_ (now) {}
  bool admit (Clock::time_point now = Clock::now ());
private:
  Clock::time_point last_;
  double tokens_ = burst;
};
/* Used only inside the private child, never by a parent mailbox. Pending
 * updates have a fixed count and bounded payload; end flushes the final value. */
class ProgressWriter {
public:
  using Clock = std::chrono::steady_clock;
  using Sink = std::function<void (const Frame&)>;
  explicit ProgressWriter (Sink sink) : sink_ (std::move (sink)) {}
  void update (FilterProgress::Event, const FilterProgress::Snapshot&,
               Clock::time_point now = Clock::now ());
  void finish ();
private:
  void send (Frame);
  void flush ();
  Sink sink_;
  std::array<Frame, 4> pending_ {};
  std::array<bool, 4> dirty_ {{false,false,false,false}};
  Clock::time_point last_ {};
  std::uint64_t sequence_ = 0, pulses_sent_ = 0, latest_pulses_ = 0;
  bool active_ = false;
  ProgressBudget budget_;
};
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
  bool progress_active_ = false;
  std::uint64_t progress_sequence_ = 0, progress_pulses_ = 0;
  FilterProcedureDisposition expected_;
  FilterProcedureDisposition disposition_ = FilterProcedureDisposition::pending;
};
}}
#endif
