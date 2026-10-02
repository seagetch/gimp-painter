/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "legacy-pixels.hpp"
#include <cstdio>
#include <cstdint>
#include <vector>
int main ()
{
  constexpr unsigned width = 19, height = 3;
  for (unsigned channels = 1; channels <= 4; ++channels)
    for (unsigned blend = 0; blend < 256; ++blend)
      {
        std::vector<std::uint8_t> sampled (width * height * channels);
        auto accum = sampled, shaded = sampled;
        std::uint8_t color[4] = {203, 39, 91, 255};
        for (unsigned i = 0; i < sampled.size (); ++i)
          {
            sampled[i] = (i * 37 + blend * 13) % 256;
            accum[i] = (i * 59 + blend * 7 + 103) % 256;
          }
        if (!(channels & 1))
          {
            color[channels - 1] = 255;
            for (unsigned i = 0; i < width * height; ++i)
              {
                if (i % 7 == 0) sampled[i * channels + channels - 1] = 0;
                if (i % 9 == 0) accum[i * channels + channels - 1] = 0;
              }
          }
        GimpPainter::Smudge::accumulate (sampled.data (), accum.data (), sampled.size (), channels, blend);
        GimpPainter::Smudge::shade (accum.data (), shaded.data (), accum.size (), channels, color, blend);
        auto composed = sampled;
        std::vector<std::uint8_t> coverage (width * height), selection (width * height);
        const bool affect[4] = {true, true, true, true};
        for (unsigned n = 0; n < coverage.size (); ++n)
          { coverage[n] = (n * 31 + blend * 17) % 256;selection[n] = (n * 23 + 91) % 256; }
        if (!(channels & 1)) GimpPainter::Smudge::replace (shaded.data (), composed.data (), coverage.data (), coverage.size (), channels, blend, affect);
        else GimpPainter::Smudge::opaque_paint (shaded.data (), composed.data (), coverage.data (), selection.data (), coverage.size (), channels, 173, blend, affect);
        std::printf ("PIXELS %u %u ", channels, blend);
        for (auto value : accum) std::printf ("%02x", value);
        std::putchar (' ');
        for (auto value : shaded) std::printf ("%02x", value);
        std::putchar (' ');
        for (auto value : composed) std::printf ("%02x", value);
        std::putchar ('\n');
      }
  std::puts ("SMUDGE_PIXELS_COMPLETE");
}
