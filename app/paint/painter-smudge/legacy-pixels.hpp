/* SPDX-License-Identifier: GPL-3.0-or-later
 * Integer arithmetic from gimp-painter's blend_pixels()/shade_pixels().
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis; subsequent GIMP authors.
 */
#ifndef GIMP_PAINTER_SMUDGE_LEGACY_PIXELS_HPP
#define GIMP_PAINTER_SMUDGE_LEGACY_PIXELS_HPP
#include <cstddef>
#include <cstdint>
#include <stdexcept>
namespace GimpPainter { namespace Smudge {
inline void validate_pixels (const void* source, const void* destination,
                             std::size_t bytes, unsigned channels)
{
  if (channels < 1 || channels > 4 || bytes % channels)
    throw std::invalid_argument ("Invalid legacy Smudge channel layout");
  if (bytes && (!source || !destination))
    throw std::invalid_argument ("Missing legacy Smudge pixels");
}
// The accumulator is both the second input and destination. Its alpha is
// deliberately weighted by blend+1, not by blend; do not replace with lerp().
inline void accumulate (const std::uint8_t* sampled, std::uint8_t* accumulator,
                        std::size_t bytes, unsigned channels,
                        std::uint8_t blend)
{
  validate_pixels (sampled, accumulator, bytes, channels);
  for (std::size_t i = 0; i < bytes; i += channels)
    {
      const unsigned colors = channels - 1;
      if (!(channels & 1))
        {
          const int a1 = (255 - blend) * sampled[i + colors];
          const int a2 = (blend + 1) * accumulator[i + colors];
          const int alpha = a1 + a2;
          if (!alpha)
            for (unsigned c = 0; c < channels; ++c) accumulator[i + c] = 0;
          else
            {
              for (unsigned c = 0; c < colors; ++c)
                accumulator[i + c] = sampled[i + c] +
                  (sampled[i + c] * a1 + accumulator[i + c] * a2 -
                   alpha * sampled[i + c]) / alpha;
              accumulator[i + colors] = alpha >> 8;
            }
        }
      else
        for (unsigned c = 0; c < channels; ++c)
          accumulator[i + c] = sampled[i + c] +
            (sampled[i + c] * (255 - blend) + accumulator[i + c] * blend -
             sampled[i + c] * 255) / 255;
    }
}
// Shading produces paint pixels only. It must not replace the persistent
// accumulation buffer, or foreground paint feeds back into future smudging.
inline void shade (const std::uint8_t* accumulator, std::uint8_t* output,
                   std::size_t bytes, unsigned channels,
                   const std::uint8_t* color, std::uint8_t blend)
{
  validate_pixels (accumulator, output, bytes, channels);
  if (!color) throw std::invalid_argument ("Missing legacy Smudge foreground");
  for (std::size_t i = 0; i < bytes; i += channels)
    {
      const unsigned colors = channels - 1;
      if (!(channels & 1))
        {
          const int a1 = (255 - blend) * accumulator[i + colors];
          const int a2 = (blend + 1) * color[colors];
          const int alpha = a1 + a2;
          if (!alpha)
            for (unsigned c = 0; c < channels; ++c) output[i + c] = 0;
          else
            {
              for (unsigned c = 0; c < colors; ++c)
                output[i + c] = accumulator[i + c] +
                  (accumulator[i + c] * a1 + color[c] * a2 -
                   alpha * accumulator[i + c]) / alpha;
              output[i + colors] = alpha >> 8;
            }
        }
      else
        for (unsigned c = 0; c < channels; ++c)
          output[i + c] = (accumulator[i + c] * (255 - blend) + color[c] * blend) / 255;
    }
}
inline int multiply (int a, int b)
{ const int value = a * b + 128;return ((value >> 8) + value) >> 8; }
inline int multiply3 (int a, int b, int c)
{ const int value = a * b * c + 0x7f5b;return ((value >> 7) + value) >> 16; }
inline void opaque_paint (const std::uint8_t* source, std::uint8_t* destination,
                          const std::uint8_t* mask, const std::uint8_t* selection,
                          std::size_t pixels, unsigned channels,
                          unsigned paint_opacity, unsigned image_opacity,
                          const bool* affect)
{
  if ((channels != 1 && channels != 3) || paint_opacity > 255 || image_opacity > 255 ||
      !affect || (pixels && (!source || !destination || !mask)))
    throw std::invalid_argument ("Invalid legacy Smudge opaque composition");
  for (std::size_t i = 0; i < pixels; ++i)
    {
      const int painted = paint_opacity == 255 ? mask[i] : multiply3 (255, mask[i], paint_opacity);
      const int alpha = selection ? multiply3 (painted, selection[i], image_opacity) : multiply (painted, image_opacity);
      for (unsigned c = 0; c < channels; ++c)
        if (affect[c]) destination[c] = multiply (int (source[c]) - destination[c], alpha) + destination[c];
      source += channels;destination += channels;
    }
}
// PaintCore's old replace_regions route is distinct from the layer Replace
// mode: it uses double alpha interpolation with opacity*mask/65536 and keeps
// meaningful hidden colors even when both alphas are zero.
inline void replace (const std::uint8_t* source, std::uint8_t* destination,
                     const std::uint8_t* mask, std::size_t pixels,
                     unsigned channels, unsigned opacity,
                     const bool* affect)
{
  if ((channels != 2 && channels != 4) || opacity > 255 || !affect ||
      (pixels && (!source || !destination || !mask)))
    throw std::invalid_argument ("Invalid legacy Smudge replacement");
  const unsigned alpha = channels - 1;
  const double normalized = opacity * (1.0 / 65536.0);
  for (std::size_t i = 0; i < pixels; ++i)
    {
      const double coverage = mask[i] * normalized;
      const int first_alpha = destination[alpha], second_alpha = source[alpha];
      const double value = first_alpha + coverage * (second_alpha - first_alpha);
      for (unsigned c = 0; c < alpha; ++c)
        if (affect[c])
          {
            int result = destination[c];
            if (value != 0)
              result = .5 + (1.0 / value) * (destination[c] * first_alpha + coverage *
                (source[c] * second_alpha - destination[c] * first_alpha));
            else if (first_alpha + second_alpha == 0)
              result = .5 + double (destination[c]) + coverage * (double (source[c]) - destination[c]);
            else if (1.0 - coverage + second_alpha == 0)
              result = source[c];
            destination[c] = result > 255 ? 255 : result;
          }
      if (affect[alpha]) destination[alpha] = value + .5;
      source += channels;destination += channels;
    }
}
} }
#endif
