/* SPDX-License-Identifier: GPL-3.0-or-later
 * Legacy plug-in-gauss compatibility, adapted from blur-gauss.c at
 * afa43fae3e920210146abed514f136fd49f671b5.
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis.
 */
#include "filter-gauss.hpp"
#include "filter-gauss-kernel-private.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace GimpPainter {
namespace {
using namespace FilterGaussDetail;

template<class Kernel>
void pass (Bytes& image, std::size_t width, std::size_t height,
           bool vertical, Kernel& kernel, Check& check)
{
  const auto length = vertical ? height : width;
  const auto lines = vertical ? width : height;
  Bytes src (length * 4), dest (length * 4);
  for (std::size_t line = 0; line < lines; ++line)
    {
      check.now ();
      for (std::size_t i = 0; i < length; ++i)
        {
          check.step ();
          const auto offset = vertical ? (i * width + line) * 4
                                       : (line * width + i) * 4;
          std::copy_n (image.data () + offset, 4, src.data () + i * 4);
        }
      multiply_alpha (src, check);
      kernel.process (src, dest, check);
      separate_alpha (dest, check);
      for (std::size_t i = 0; i < length; ++i)
        {
          check.step ();
          const auto offset = vertical ? (i * width + line) * 4
                                       : (line * width + i) * 4;
          std::copy_n (dest.data () + i * 4, 4, image.data () + offset);
        }
    }
}

} // namespace

bool filter_gauss (const Bytes& input,
                   std::size_t width,
                   std::size_t height,
                   const GaussOptions& options,
                   std::atomic<bool>& cancel,
                   Bytes& output)
{
  const auto maximum = std::numeric_limits<std::size_t>::max ();
  const auto legacy_maximum = static_cast<std::size_t>
    (std::numeric_limits<int>::max () / 4);
  if (width == 0 || height == 0 || width > legacy_maximum ||
      height > legacy_maximum || height > maximum / (width * 4))
    throw std::invalid_argument ("Invalid legacy Gaussian dimensions");
  if (input.size () != width * height * 4)
    throw std::invalid_argument ("Gaussian input is not tightly packed RGBA8");
  if (!std::isfinite (options.horizontal) || !std::isfinite (options.vertical) ||
      (options.horizontal <= 0.0 && options.vertical <= 0.0) ||
      options.method < 0 || options.method > 1)
    throw std::invalid_argument ("Invalid legacy Gaussian options");

  const auto region = legacy_region (width,height,options.horizontal,options.vertical);
  Check check { cancel };
  try
    {
      check.now ();
      Bytes result (input.size ());
      for (std::size_t i = 0; i < input.size (); ++i)
        {
          check.step ();
          result[i] = input[i];
        }
      const bool iir = options.method == 0 &&
                       options.horizontal > 1.0 && options.vertical > 1.0;
      for (bool vertical : { true, false })
        {
          const double radius = vertical ? options.vertical : options.horizontal;
          if (radius <= 0.0)
            continue;
          const auto length = vertical ? height : width;
          if (iir)
            {
              Iir kernel (radius, length);
              pass (result, width, height, vertical, kernel, check);
            }
          else
            {
              Rle kernel (radius, length, check);
              pass (result, width, height, vertical, kernel, check);
            }
        }
      for (std::size_t i = 0; i < result.size (); i += 4)
        {
          check.step ();
          /* Full-opacity REPLACE_INTEN shadow merge from paint-funcs.c. */
          if (region.empty)
            std::copy_n (input.data () + i, 4, result.data () + i);
          else if (region.cropped && !region.contains (i / 4,width))
            { std::copy_n (input.data () + i, 3, result.data () + i); result[i + 3] = 0; }
          else if (result[i + 3] == 0)
            std::copy_n (input.data () + i, 3, result.data () + i);
        }
      check.now ();
      output.swap (result);
      return true;
    }
  catch (const Cancelled&)
    {
      return false;
    }
}

} // namespace GimpPainter
