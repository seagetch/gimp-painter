/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef __GIMP_PAINTER_FLOAT_KERNEL_H__
#define __GIMP_PAINTER_FLOAT_KERNEL_H__
#ifdef __cplusplus
extern "C" {
#endif
/* Private arithmetic identities, NOT public layer-mode or old XCF IDs. */
typedef enum {
  GIMP_PAINTER_FLOAT_ERASE,
  GIMP_PAINTER_FLOAT_REPLACE,
  GIMP_PAINTER_FLOAT_ANTI_ERASE,
  GIMP_PAINTER_FLOAT_SRC_IN,
  GIMP_PAINTER_FLOAT_DST_IN,
  GIMP_PAINTER_FLOAT_SRC_OUT,
  GIMP_PAINTER_FLOAT_DST_OUT,
  GIMP_PAINTER_FLOAT_NORMAL,
  GIMP_PAINTER_FLOAT_MULTIPLY
} GimpPainterFloatKernelMode;
/* Straight nonlinear RGB; continuous extension, not historical byte arithmetic.
 * Inputs may alias output. Invalid mode returns zero and leaves output alone. */
int gimp_painter_float_composite (const float backdrop[4], const float source[4],
                                 float out[4], float opacity, float mask,
                                 GimpPainterFloatKernelMode mode);
#ifdef __cplusplus
}
#endif
#endif
