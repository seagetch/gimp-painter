/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_PROVENANCE_PRIVATE_H
#define GIMP_PAINTER_PROVENANCE_PRIVATE_H
#include <glib-object.h>
G_BEGIN_DECLS
/* Internal C ownership seam, not a generic state registry. The only accepted
 * child type is GimpPainterProvenance, whose sole payload uses BindingStore. */
GType     gimp_painter_provenance_get_type (void) G_GNUC_CONST;
GObject  *_gimp_object_lease_painter_provenance_owner (GObject *owner, gboolean writing);
GObject  *_gimp_object_ref_painter_provenance (GObject *owner);
gboolean  _gimp_object_attach_painter_provenance (GObject *owner, GObject *child);
G_END_DECLS
#endif
