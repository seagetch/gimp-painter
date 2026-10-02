/* brushlib - The MyPaint Brush Library
 * Copyright (C) 2007-2008 Martin Renold <martinxyz@gmx.ch>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#ifndef GIMP_PAINTER_MYPAINT_MAPPING_HPP
#define GIMP_PAINTER_MYPAINT_MAPPING_HPP
#include <array>
#include <cmath>
#include <stdexcept>
#include <vector>

namespace GimpPainter { namespace MyPaint {
/* Value-owned replacement for the old new[]/delete mapping owner. Evaluation
 * order and extrapolation, including duplicate x coordinates, remain legacy. */
class Mapping
{
  struct Points {
    std::array<float, 8> xvalues{};
    std::array<float, 8> yvalues{};
    int n = 0;
  };
  std::vector<Points> points_;
  int inputs_used_ = 0;
public:
  float base_value = 0;
  explicit Mapping (int inputs = 9) : points_ (inputs) {}
  void set_n (int input, int n)
  {
    if (n < 0 || n > 8 || n == 1) throw std::invalid_argument ("A legacy curve requires zero or 2–8 points");
    auto& p = points_.at (input);
    if (n && !p.n) ++inputs_used_;
    if (!n && p.n) --inputs_used_;
    p.n = n;
  }
  int get_n (int input) const { return points_.at (input).n; }
  void set_point (int input, int index, float x, float y)
  {
    auto& p = points_.at (input);
    if (index < 0 || index >= p.n || !std::isfinite (x) || !std::isfinite (y) ||
        (index && x < p.xvalues[index-1]))
      throw std::invalid_argument ("Invalid or descending legacy curve point");
    p.xvalues[index] = x; p.yvalues[index] = y;
  }
  void get_point (int input, int index, float *x, float *y) const
  {
    const auto& p = points_.at (input);
    if (index < 0 || index >= p.n) throw std::out_of_range ("Legacy curve point");
    *x = p.xvalues[index]; *y = p.yvalues[index];
  }
  bool is_constant () const noexcept { return !inputs_used_; }
  float calculate (const float *data) const
  {
    float result = base_value;
    if (!inputs_used_) return result;
    for (std::size_t j = 0; j < points_.size (); ++j) {
      const auto& p = points_[j];
      if (!p.n) continue;
      const float x = data[j];
      float x0 = p.xvalues[0], y0 = p.yvalues[0];
      float x1 = p.xvalues[1], y1 = p.yvalues[1];
      for (int i = 2; i < p.n && x > x1; ++i) {
        x0 = x1; y0 = y1; x1 = p.xvalues[i]; y1 = p.yvalues[i];
      }
      result += x0 == x1 ? y0 : (y1*(x-x0) + y0*(x1-x)) / (x1-x0);
    }
    return result;
  }
  float calculate_single_input (float input) const
  {
    if (points_.size () != 1) throw std::invalid_argument ("Expected single input mapping");
    return calculate (&input);
  }
};
} }
#endif
