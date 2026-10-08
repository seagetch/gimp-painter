/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef PAINTER_HEADER_LINKAGE_PROBE_API_H
#define PAINTER_HEADER_LINKAGE_PROBE_API_H

#include <glib-object.h>
#include <stddef.h>

/* These are fixture exports only. Production headers are included separately,
 * outside this block, so a fixture cannot repair a missing production guard. */
G_BEGIN_DECLS
typedef void (*PainterLinkageFunction) (void);
extern PainterLinkageFunction volatile painter_c_references[];
extern PainterLinkageFunction volatile painter_cpp_references[];
extern const size_t painter_reference_count;
void painter_check_cpp_boundary_from_c (void);
void painter_c_layout (size_t values[8]);
G_END_DECLS
#endif
