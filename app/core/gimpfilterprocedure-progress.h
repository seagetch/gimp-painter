/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_FILTER_PROCEDURE_PROGRESS_H
#define GIMP_FILTER_PROCEDURE_PROGRESS_H
#include <glib-object.h>
G_BEGIN_DECLS
typedef struct _GimpProgress GimpProgress;
/* Only the isolated executor's typed adapter bypasses headless PDB suppression.
 * A generic supplied progress object or environment variable is not authority. */
gboolean gimp_filter_procedure_progress_is_private (GimpProgress *progress);
G_END_DECLS
#endif
