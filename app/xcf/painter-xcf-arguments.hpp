/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_XCF_ARGUMENTS_HPP
#define GIMP_PAINTER_XCF_ARGUMENTS_HPP
/* Include after the GIMP C type/API declarations. Exceptions stay inside the
 * caller's C boundary. Variants returned here own a nonfloating reference. */
namespace GimpPainterXcf {
GVariant *external_reference_origin (GObject *, GType declared_type, const char *role);
GVariant *encode_snapshot (const GimpFilterArgumentsSnapshot *, GimpImage *, GHashTable *saved_ids, GVariant **external_origins = nullptr);
GimpFilterArgumentsSnapshot *decode_snapshot (GVariant *, GimpImage *);
GVariant *encode_arguments (const GimpValueArray *, GimpImage *);
GimpValueArray *decode_arguments (GVariant *, GimpImage *);
}
#endif
