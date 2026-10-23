/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef LEGACY_SHARPEN_ORACLE_H
#define LEGACY_SHARPEN_ORACLE_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Raw shadow of a cropped Gray, Gray-alpha, RGB or RGBA rectangle. Sequential
 * test use only. Returns 0 on unsupported input; 1 after complete execution.
 * Tiny/unwritten legacy regions and overflowing legacy arithmetic are rejected.
 * This source-extracted oracle is not a native GIMP runtime observation. */
int legacy_sharpen_oracle (const uint8_t *, size_t, size_t, unsigned, int32_t, uint8_t *);
#ifdef __cplusplus
}
#endif
#endif
