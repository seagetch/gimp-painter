/* Convolution Matrix plug-in for GIMP
 * Copyright (C) 1997 Lauri Alanko <la@iki.fi>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef __PAINTER_CONVOLUTION_MATRIX_KERNEL_H__
#define __PAINTER_CONVOLUTION_MATRIX_KERNEL_H__

#include <math.h>

#include <glib.h>

#define PAINTER_CONVOLUTION_SIZE  5
#define PAINTER_CONVOLUTION_CELLS 25

typedef struct
{
  gdouble matrix[PAINTER_CONVOLUTION_CELLS];
  gdouble divisor;
  gdouble offset;
  gfloat  matrix_u8[PAINTER_CONVOLUTION_CELLS];
  gfloat  divisor_u8;
  gfloat  offset_u8;
} PainterConvolution;

static inline gboolean
painter_convolution_validate (PainterConvolution *kernel,
                              gboolean            bytes)
{
  gint i;

  if (! isfinite (kernel->divisor) || kernel->divisor == 0.0 ||
      ! isfinite (kernel->offset))
    return FALSE;

  for (i = 0; i < PAINTER_CONVOLUTION_CELLS; i++)
    if (! isfinite (kernel->matrix[i]))
      return FALSE;

  if (! bytes)
    return TRUE;

  /* The original PDB accepted doubles, then stored its configuration in
   * floats.  Range-check before conversion, including a divisor that would
   * become zero after narrowing.  Underflowed coefficients match the old
   * float configuration and are harmless.
   */
  if (fabs (kernel->divisor) > G_MAXFLOAT ||
      fabs (kernel->offset) > G_MAXFLOAT)
    return FALSE;

  kernel->divisor_u8 = (gfloat) kernel->divisor;
  kernel->offset_u8  = (gfloat) kernel->offset;

  if (kernel->divisor_u8 == 0.0f)
    return FALSE;

  for (i = 0; i < PAINTER_CONVOLUTION_CELLS; i++)
    {
      if (fabs (kernel->matrix[i]) > G_MAXFLOAT)
        return FALSE;

      kernel->matrix_u8[i] = (gfloat) kernel->matrix[i];
    }

  /* Do not reject a configuration using worst-case 255-valued pixels.
   * Large finite coefficients can be well-defined on actual low-valued data;
   * each pixel checks its own intermediates and final integer conversion. */
  return TRUE;
}

static inline gboolean
painter_convolution_pixel_u8 (const PainterConvolution *kernel,
                             const guchar             *rows[5],
                             gsize                     offset,
                             gint                      components,
                             guchar                   *result)
{
  volatile gfloat sum = 0.0f;
  gint            x, y;

  for (y = 0; y < PAINTER_CONVOLUTION_SIZE; y++)
    for (x = 0; x < PAINTER_CONVOLUTION_SIZE; x++)
      {
        /* Keep the original x-major coefficient array and y-major visit
         * order, with a float rounding at each multiply and addition.
         * Volatile prevents an optimizing compiler from fusing the two.
         * check_config() disabled alpha weighting on every old PDB call.
         */
        volatile gfloat temp = kernel->matrix_u8[x * 5 + y];

        temp *= rows[y][offset + x * components];
        sum += temp;

        if (! isfinite (temp) || ! isfinite (sum))
          return FALSE;
      }

  sum /= kernel->divisor_u8;
  sum += kernel->offset_u8;

  if (! isfinite (sum) ||
      (gdouble) sum + 0.5 > G_MAXINT || (gdouble) sum + 0.5 < G_MININT)
    return FALSE;

  /* This deliberately uses ROUND, not signed rounding: the old plug-in
   * added the double constant 0.5, converted to int, then clamped.
   */
  *result = CLAMP ((gint) ((gdouble) sum + 0.5), 0, 255);

  return TRUE;
}

static inline gboolean
painter_convolution_pixel_double (const PainterConvolution *kernel,
                                 const gdouble            *rows[5],
                                 gsize                     offset,
                                 gint                      components,
                                 gdouble                  *result)
{
  gdouble sum = 0.0;
  gint    x, y;

  for (y = 0; y < PAINTER_CONVOLUTION_SIZE; y++)
    for (x = 0; x < PAINTER_CONVOLUTION_SIZE; x++)
      {
        gdouble sample = rows[y][offset + x * components];
        gdouble temp;

        if (! isfinite (sample))
          return FALSE;

        temp = kernel->matrix[x * 5 + y] * sample;
        sum += temp;

        if (! isfinite (temp) || ! isfinite (sum))
          return FALSE;
      }

  sum /= kernel->divisor;
  if (! isfinite (sum))
    return FALSE;

  sum += kernel->offset / 255.0;
  if (! isfinite (sum))
    return FALSE;

  /* Colors retain negative values and highlights beyond 1.0.  The caller
   * clamps only alpha, after deciding whether that channel is enabled.
   */
  *result = sum;

  return TRUE;
}

#endif /* __PAINTER_CONVOLUTION_MATRIX_KERNEL_H__ */
