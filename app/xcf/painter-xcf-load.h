/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_XCF_LOAD_H
#define GIMP_PAINTER_XCF_LOAD_H
G_BEGIN_DECLS
/* The explicit variants are recovery choices, never auto-detection hints. */
typedef enum {
  XCF_PAINTER_DIALECT_AUTO,
  XCF_PAINTER_DIALECT_STANDARD,
  XCF_PAINTER_DIALECT_LEGACY
} XcfPainterDialect;
GimpImage *xcf_load_stream_with_dialect (Gimp *, GInputStream *, GFile *,
                                          GimpProgress *, XcfPainterDialect,
                                          GError **);
/* Exact immutable original bytes, including unrecognized records. Caller owns
 * the returned reference. Object offsets identify original owning records. */
GBytes   *xcf_painter_ref_original (GimpImage *);
gboolean  xcf_painter_original_offset (GObject *, guint64 *);

#ifdef __XCF_PRIVATE_H__
gboolean xcf_painter_prepare (XcfInfo *, XcfPainterDialect, GError **);
gboolean xcf_painter_load_extension (XcfInfo *, GimpImage *, GimpLayer **,
                                      guint32, guint32);
gboolean xcf_painter_load_mode (GimpLayer *, guint32);
void     xcf_painter_finish_image (XcfInfo *, GimpImage *);
void     xcf_painter_record_offset (GObject *, goffset);
#endif
G_END_DECLS
#endif
