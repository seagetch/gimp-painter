/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef LEGACY_SOBEL_ORACLE_H
#define LEGACY_SOBEL_ORACLE_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Full-drawable native Gray/Gray-alpha/RGB/RGBA raw shadow, zero outside ROI.
 * Returns 1 on execution, 0 on invalid or conservatively unsupported geometry.
 * Horizontal halo repeats ROI endpoints; vertical halo uses the full image.
 * Independent buffers, valid extents and sequential test calls are required.
 * This extracted C algorithm is not an original GIMP runtime observation. */
int legacy_sobel_oracle (const uint8_t *input, size_t width, size_t height,
                         unsigned channels, size_t x, size_t y,
                         size_t roi_width, size_t roi_height,
                         int32_t horizontal, int32_t vertical, int32_t keep_sign,
                         uint8_t *output);
#ifdef __cplusplus
}
#endif
#endif
