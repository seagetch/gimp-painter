/* SPDX-License-Identifier: GPL-3.0-or-later
 * Legacy plug-in-gauss compatibility, adapted from blur-gauss.c at
 * afa43fae3e920210146abed514f136fd49f671b5.
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis.
 */
/* Shared arithmetic for the vector and spill-backed implementations.
 * Preserve historical operation order and byte rounding in one place. */
#ifndef GIMP_PAINTER_FILTER_GAUSS_KERNEL_PRIVATE_HPP
#define GIMP_PAINTER_FILTER_GAUSS_KERNEL_PRIVATE_HPP
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>
#include <type_traits>
namespace GimpPainter { namespace FilterGaussDetail {
using Bytes = std::vector<std::uint8_t>;

struct Cancelled {};

struct Check
{
  std::atomic<bool>& flag;
  unsigned ticks = 0;
  void now () const
  {
    if (flag.load (std::memory_order_relaxed))
      throw Cancelled {};
  }
  void step ()
  {
    if ((++ticks & 255) == 0)
      now ();
  }
};

/* With a negative disabled axis, old gauss() computes an expansion of
 * 1 + ceil(radius). Values below -1 therefore shrink that axis. For a nonempty
 * region its unwritten shadow border is transparent; an empty region performs
 * no shadow writes and leaves the original drawable unchanged. */
struct Region
{
  std::size_t left, top, right, bottom;
  bool empty, cropped;
  bool contains (std::uint64_t pixel, std::size_t width) const noexcept
  {
    const auto x = pixel % width, y = pixel / width;
    return x >= left && x < right && y >= top && y < bottom;
  }
};
inline Region legacy_region (std::size_t width, std::size_t height, double horizontal, double vertical)
{
  const auto margin = [] (std::size_t length, double radius) {
    const double extra = 1.0 + std::ceil (radius);
    if (radius >= -1.0) return std::size_t (0);
    /* These casts/subtractions were gint arithmetic in the old entry point.
     * Preserve defined negative/empty regions, not out-of-range conversions. */
    if (!std::isfinite (extra) || extra <= std::numeric_limits<int>::min ())
      throw std::invalid_argument ("Gaussian region exceeds defined legacy coordinates");
    const auto value = static_cast<std::uint64_t> (-extra);
    if (value * 2 > std::uint64_t (std::numeric_limits<int>::max ()) + length + 1)
      throw std::invalid_argument ("Gaussian region exceeds defined legacy coordinates");
    return std::size_t (value);
  };
  const auto x = margin (width,horizontal), y = margin (height,vertical);
  const bool empty = x * 2 >= width || y * 2 >= height;
  return {x,y,empty ? 0 : width-x,empty ? 0 : height-y,empty,x != 0 || y != 0};
}

inline void numerical_limit ()
{
  throw std::invalid_argument ("Gaussian radius exceeds defined legacy arithmetic");
}

inline double sigma_for (double radius)
{
  const double expanded = std::fabs (radius) + 1.0;
  const double sigma = std::sqrt (-(expanded * expanded) /
                                  (2 * std::log (1.0 / 255.0)));
  if (!std::isfinite (sigma) || sigma <= 0.0)
    numerical_limit ();
  return sigma;
}

inline void multiply_alpha (Bytes& line, Check& check)
{
  for (std::size_t i = 0; i < line.size (); i += 4)
    {
      check.step ();
      const double alpha = line[i + 3] * (1.0 / 255.0);
      for (int channel = 0; channel < 3; ++channel)
        line[i + channel] = static_cast<std::uint8_t>
          (line[i + channel] * alpha + 0.5);
    }
}

inline void separate_alpha (Bytes& line, Check& check)
{
  for (std::size_t i = 0; i < line.size (); i += 4)
    {
      check.step ();
      const auto alpha = line[i + 3];
      if (alpha == 0 || alpha == 255)
        continue;
      const double reciprocal = 255.0 / alpha;
      for (int channel = 0; channel < 3; ++channel)
        {
          const int value = static_cast<int>
            (line[i + channel] * reciprocal + 0.5);
          line[i + channel] = static_cast<std::uint8_t> (std::min (255, value));
        }
    }
}

inline void
find_iir_constants (double *n_p,
                    double *n_m,
                    double *d_p,
                    double *d_m,
                    double *bd_p,
                    double *bd_m,
                    double  std_dev)
{
  /*  The constants used in the implemenation of a casual sequence
   *  using a 4th order approximation of the gaussian operator
   */

  const double div = sqrt (2 * 3.141592653589793238462643383279502884) * std_dev;
  const double x0 = -1.783 / std_dev;
  const double x1 = -1.723 / std_dev;
  const double x2 = 0.6318 / std_dev;
  const double x3 = 1.997  / std_dev;
  const double x4 = 1.6803 / div;
  const double x5 = 3.735 / div;
  const double x6 = -0.6803 / div;
  const double x7 = -0.2598 / div;
  int          i;

  n_p [0] = x4 + x6;
  n_p [1] = (exp(x1)*(x7*sin(x3)-(x6+2*x4)*cos(x3)) +
             exp(x0)*(x5*sin(x2) - (2*x6+x4)*cos (x2)));
  n_p [2] = (2 * exp(x0+x1) *
             ((x4+x6)*cos(x3)*cos(x2) - x5*cos(x3)*sin(x2) -
              x7*cos(x2)*sin(x3)) +
             x6*exp(2*x0) + x4*exp(2*x1));
  n_p [3] = (exp(x1+2*x0) * (x7*sin(x3) - x6*cos(x3)) +
             exp(x0+2*x1) * (x5*sin(x2) - x4*cos(x2)));
  n_p [4] = 0.0;

  d_p [0] = 0.0;
  d_p [1] = -2 * exp(x1) * cos(x3) -  2 * exp(x0) * cos (x2);
  d_p [2] = 4 * cos(x3) * cos(x2) * exp(x0 + x1) +  exp(2 * x1) + exp(2 * x0);
  d_p [3] = -2 * cos(x2) * exp(x0 + 2*x1) -  2*cos(x3) * exp(x1 + 2*x0);
  d_p [4] = exp(2*x0 + 2*x1);

  for (i = 0; i <= 4; i++)
    d_m[i] = d_p[i];

  n_m[0] = 0.0;

  for (i = 1; i <= 4; i++)
    n_m[i] = n_p[i] - d_p[i] * n_p[0];

  {
    double sum_n_p, sum_n_m, sum_d;
    double a, b;

    sum_n_p = 0.0;
    sum_n_m = 0.0;
    sum_d = 0.0;

    for (i = 0; i <= 4; i++)
      {
        sum_n_p += n_p[i];
        sum_n_m += n_m[i];
        sum_d += d_p[i];
      }

    a = sum_n_p / (1.0 + sum_d);
    b = sum_n_m / (1.0 + sum_d);

    for (i = 0; i <= 4; i++)
      {
        bd_p[i] = d_p[i] * a;
        bd_m[i] = d_m[i] * b;
      }
  }
}


struct Iir
{
  double np[5], nm[5], dp[5], dm[5], bdp[5], bdm[5];
  std::vector<double> positive, negative;

  Iir (double radius, std::size_t length)
    : positive (length * 4), negative (length * 4)
  {
    find_iir_constants (np, nm, dp, dm, bdp, bdm, sigma_for (radius));
    for (int i = 0; i < 5; ++i)
      if (!std::isfinite (np[i]) || !std::isfinite (nm[i]) ||
          !std::isfinite (dp[i]) || !std::isfinite (dm[i]) ||
          !std::isfinite (bdp[i]) || !std::isfinite (bdm[i]))
        numerical_limit ();
  }

  template<class Sample>
  void process (const std::vector<Sample>& src, std::vector<Sample>& dest, Check& check)
  {
    const auto length = src.size () / 4;
    double dc_gain = 1.0;
    if (std::is_floating_point<Sample>::value)
      {
        /* The old fourth-order approximation is not exactly unity at DC.
         * Modern precision removes byte rounding and explicitly normalizes
         * this gain so a constant image does not brighten on every rerun. */
        long double numerator = 0, denominator = 1;
        for (int i = 0; i < 5; ++i) { numerator += np[i] + nm[i]; denominator += dp[i]; }
        dc_gain = static_cast<double> (numerator / denominator);
        if (!std::isfinite (dc_gain) || dc_gain <= 0) numerical_limit ();
      }
    for (std::size_t i = 0; i < src.size (); ++i)
      {
        check.step ();
        positive[i] = negative[i] = 0.0;
      }
    for (std::size_t x = 0; x < length; ++x)
      {
        check.step ();
        const auto reverse = length - 1 - x;
        const int terms = static_cast<int> (std::min<std::size_t> (x, 4));
        for (int channel = 0; channel < 4; ++channel)
          {
            auto& vp = positive[x * 4 + channel];
            auto& vm = negative[reverse * 4 + channel];
            int i;
            for (i = 0; i <= terms; ++i)
              {
                const auto p = (x - i) * 4 + channel;
                const auto m = (reverse + i) * 4 + channel;
                vp += np[i] * src[p] - dp[i] * positive[p];
                vm += nm[i] * src[m] - dm[i] * negative[m];
              }
            for (; i <= 4; ++i)
              {
                vp += (np[i] - bdp[i]) * src[channel];
                vm += (nm[i] - bdm[i]) * src[(length - 1) * 4 + channel];
              }
          }
      }
    for (std::size_t i = 0; i < src.size (); ++i)
      {
        check.step ();
        const double sum = (positive[i] + negative[i]) / dc_gain;
        if (!std::isfinite (sum))
          numerical_limit ();
        /* IIR truncates, in contrast to both alpha conversions and RLE. */
        dest[i] = static_cast<Sample>
          (std::max (0.0, std::min (std::is_integral<Sample>::value ? 255.0 : 1.0, sum)));
      }
  }
};

template<class Sample>
struct RleKernel
{
  int length, total;
  std::vector<int> curve, prefix, repeats;
  std::vector<Sample> pixels;

  RleKernel (double radius, std::size_t size, Check& check)
  {
    const double sigma = sigma_for (radius);
    const double sigma2 = 2 * sigma * sigma;
    const double extent = std::ceil (std::sqrt
      (-sigma2 * std::log (1.0 / 255.0)));
    /* The old curve evaluates i*i as a signed 32-bit integer. */
    if (!std::isfinite (extent) || extent > 46340.0 || extent < 1.0)
      numerical_limit ();
    length = static_cast<int> (extent);
    curve.resize (length + 1);
    curve[0] = 255;
    for (int i = 1; i <= length; ++i)
      {
        check.step ();
        curve[i] = static_cast<int> (std::exp (-(i * i) / sigma2) * 255);
      }
    prefix.resize (2 * length + 1);
    for (int i = 1; i <= length * 2; ++i)
      {
        check.step ();
        prefix[i] = prefix[i - 1] + curve[std::abs (i - length - 1)];
      }
    total = prefix[2 * length];
    /* Full RLE includes BOTH end weights, but the historical denominator
     * and encoded branch omit the positive endpoint. Keep this asymmetry:
     * its coefficient is sometimes 1 instead of 0 due to exp rounding. */
    const auto bound = static_cast<std::int64_t> (total + curve[length]) * 255
                       + total / 2;
    if (total <= 0 || bound > std::numeric_limits<int>::max () ||
        size > static_cast<std::size_t> (std::numeric_limits<int>::max () -
                                         2 * length))
      numerical_limit ();
    pixels.resize (size + 2 * length);
    repeats.resize (pixels.size ());
  }

  void process (const std::vector<Sample>& src, std::vector<Sample>& dest, Check& check)
  {
    const int size = static_cast<int> (src.size () / 4);
    const int padded = size + 2 * length;
    for (int channel = 0; channel < 4; ++channel)
      {
        int same = 1; // legacy encoder compares the last real pixel to itself
        for (int i = 0; i < size - 1; ++i)
          {
            check.step ();
            if (src[i * 4 + channel] == src[(i + 1) * 4 + channel])
              ++same;
          }
        for (int i = padded - 1; i >= 0; --i)
          {
            check.step ();
            const int x = std::max (0, std::min (size - 1, i - length));
            pixels[i] = src[x * 4 + channel];
            repeats[i] = i + 1 < padded && pixels[i] == pixels[i + 1]
                         ? repeats[i + 1] + 1 : 1;
          }
        const bool encoded = same > (3 * size) / 4;
        for (int x = 0; x < size; ++x)
          {
            check.step ();
            using Sum = typename std::conditional<std::is_integral<Sample>::value, int, double>::type;
            Sum value = std::is_integral<Sample>::value ? total / 2 : 0;
            if (encoded)
              {
                for (int i = 0; i < 2 * length; )
                  {
                    check.step ();
                    const int end = std::min (2 * length, i + repeats[x + i]);
                    value += pixels[x + i] * (prefix[end] - prefix[i]);
                    i = end;
                  }
              }
            else
              {
                value += pixels[x + length] * curve[0];
                for (int i = 1; i <= length; ++i)
                  {
                    check.step ();
                    value += (pixels[x + length + i] +
                              pixels[x + length - i]) * curve[i];
                  }
              }
            dest[x * 4 + channel] = static_cast<Sample>
              (std::max (Sum (0),std::min (Sum (std::is_integral<Sample>::value ? 255 : 1), value / total)));
          }
      }
  }
};

using Rle = RleKernel<std::uint8_t>;
using RleReal = RleKernel<double>;

} } // namespace GimpPainter::FilterGaussDetail
#endif
