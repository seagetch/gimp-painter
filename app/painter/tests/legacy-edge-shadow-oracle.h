/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef LEGACY_EDGE_SHADOW_ORACLE_H
#define LEGACY_EDGE_SHADOW_ORACLE_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Test-only source-extracted oracle, not an original GIMP runtime capture.
 * Input and caller-initialized output are tightly packed full-drawable native
 * Gray/Gray-alpha/RGB/RGBA bytes. Only the half-open ROI is changed. Valid
 * accessible extents and sequential calls are required; overlapping buffers
 * are rejected. This does not merge the raw shadow into the original image.
 * Returns 1 on execution, including empty ROI, or 0 without output writes for
 * invalid/conservatively unsupported geometry, domain, or allocation failure.
 * edgemode: 0 Sobel, 1 Prewitt, 2 Gradient, 3 Roberts, 4 Differential, 5 Laplace.
 * wrapmode: 1 Wrap, 2 Smear, 3 Black, relative to full-drawable boundaries.
 * Amount must be finite; values below 1 are clamped exactly as in edge(). The
 * supported post-clamp upper bound is (double) INT32_MAX / 2040.0.
 * Conservatively: dimensions <= INT32_MAX; native rowstride <= INT32_MAX -
 * (2 * channels - 1); ROI area <= INT32_MAX; extent plus one padded row <= PTRDIFF_MAX.
 */
int legacy_edge_shadow_oracle (const uint8_t *input,
                               size_t width, size_t height, unsigned channels,
                               size_t x, size_t y,
                               size_t roi_width, size_t roi_height,
                               int32_t edgemode, int32_t wrapmode, double amount,
                               uint8_t *output);
/* Scalar detector with the same amount/domain guard. kernel[3*x+y] is the
 * original x-major 3x3 neighborhood. Returns 1/0; rejected output is untouched.
 */
int legacy_edge_shadow_detect (const uint8_t kernel[9], int32_t edgemode,
                               double amount, uint8_t *output);
/* Counts from the most recent full oracle call (zero for rejection). Empty
 * ROI still performs all three hooks, exactly once. NULL arguments are valid.
 */
void legacy_edge_shadow_oracle_get_counters (unsigned *flushes,
                                             unsigned *merges,
                                             unsigned *updates);
#ifdef __cplusplus
}
#endif
#endif
