/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_FILTER_PROGRESS_HPP
#define GIMP_PAINTER_FILTER_PROGRESS_HPP
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <mutex>

namespace GimpPainter {
/* Independent, fixed-size worker data. This object never calls the owner or
 * holds native objects. Reset only after the previous worker has completed. */
class FilterProgress final {
public:
  static constexpr std::size_t text_limit = 512, domain_limit = 128;
  enum class Event { start, end, text, value, pulse, message };
  struct Snapshot {
    std::uint64_t revision = 0, message_revision = 0, pulses = 0;
    bool active = false, cancellable = false;
    double value = 0;
    std::array<char, text_limit + 1> text {}, message_text {};
    std::array<char, domain_limit + 1> message_domain {};
    int message_severity = 0;
  };

  void reset () { std::lock_guard<std::mutex> lock (mutex_); state_ = {}; }
  void start (bool cancellable, const char *message) {
    std::lock_guard<std::mutex> lock (mutex_);
    state_.active = true; state_.cancellable = cancellable; state_.value = 0;
    copy_text (state_.text, message); advance (state_.revision);
  }
  void end () {
    std::lock_guard<std::mutex> lock (mutex_);
    state_.active = false; state_.cancellable = false; advance (state_.revision);
  }
  void set_text (const char *text) {
    std::lock_guard<std::mutex> lock (mutex_);
    copy_text (state_.text, text); advance (state_.revision);
  }
  void set_value (double value) {
    if (!std::isfinite (value)) return;
    std::lock_guard<std::mutex> lock (mutex_);
    state_.value = std::max (0.0, std::min (1.0, value)); advance (state_.revision);
  }
  void pulse (std::uint64_t count = 1) {
    std::lock_guard<std::mutex> lock (mutex_);
    const auto room = std::numeric_limits<std::uint64_t>::max () - state_.pulses;
    state_.pulses += std::min (count, room); advance (state_.revision);
  }
  void message (int severity, const char *domain, const char *text) {
    if (severity < 0 || severity > 4) return;
    std::lock_guard<std::mutex> lock (mutex_);
    state_.message_severity = severity;
    copy_text (state_.message_domain, domain); copy_text (state_.message_text, text);
    advance (state_.message_revision); advance (state_.revision);
  }
  bool try_snapshot (Snapshot& result) const {
    std::unique_lock<std::mutex> lock (mutex_, std::try_to_lock);
    if (!lock.owns_lock ()) return false;
    result = state_; return true;
  }

  /* The scan never exceeds the destination limit, even for hostile strings.
   * Complete valid UTF-8 is preserved; malformed bytes become '?' and a final
   * incomplete sequence is omitted. No allocation or unbounded strlen occurs. */
  template<std::size_t N>
  static void copy_text (std::array<char, N>& output, const char *input) noexcept {
    output.fill (0);
    if (!input) return;
    std::size_t size = 0;
    while (size < N - 1 && input[size]) ++size;
    std::size_t from = 0, to = 0;
    while (from < size) {
      const auto count = utf8_character (reinterpret_cast<const unsigned char *> (input + from), size - from);
      if (count) {
        for (std::size_t i = 0; i < count; ++i) output[to++] = input[from++];
      } else {
        const auto byte = static_cast<unsigned char> (input[from]);
        const auto expected = byte >= 0xc2 && byte <= 0xdf ? 2u :
                              byte >= 0xe0 && byte <= 0xef ? 3u :
                              byte >= 0xf0 && byte <= 0xf4 ? 4u : 1u;
        if (size == N - 1 && expected > size - from) break;
        output[to++] = '?'; ++from;
      }
    }
  }
  static bool valid_utf8 (const std::uint8_t *text, std::size_t size) noexcept {
    for (std::size_t at = 0; at < size;) {
      const auto count = utf8_character (text + at, size - at);
      if (!count) return false;
      at += count;
    }
    return true;
  }
private:
  static std::size_t utf8_character (const unsigned char *s, std::size_t size) noexcept {
    if (!size || !s[0]) return 0;
    if (s[0] < 0x80) return 1;
    const unsigned count = s[0] >= 0xc2 && s[0] <= 0xdf ? 2 :
                           s[0] >= 0xe0 && s[0] <= 0xef ? 3 :
                           s[0] >= 0xf0 && s[0] <= 0xf4 ? 4 : 0;
    if (!count || count > size) return 0;
    for (unsigned i = 1; i < count; ++i) if ((s[i] & 0xc0) != 0x80) return 0;
    if ((s[0] == 0xe0 && s[1] < 0xa0) || (s[0] == 0xed && s[1] >= 0xa0) ||
        (s[0] == 0xf0 && s[1] < 0x90) || (s[0] == 0xf4 && s[1] >= 0x90)) return 0;
    return count;
  }
  static void advance (std::uint64_t& value) noexcept {
    if (value != std::numeric_limits<std::uint64_t>::max ()) ++value;
  }
  mutable std::mutex mutex_;
  Snapshot state_;
};
}
#endif
