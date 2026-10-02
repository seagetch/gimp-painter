/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_BINDING_H
#define GIMP_PAINTER_BINDING_H
#include "gimp-painter-error.h"
G_BEGIN_DECLS

/* Explicit dispose/destroy/controller close hook. An owner without a store is
 * already inert. Never creates a store, waits for a job, or destroys the Impl. */
gboolean gimp_painter_binding_close (GObject  *owner,
                                     GError  **error);

G_END_DECLS
#endif
