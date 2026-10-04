/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_XCF_PRESERVE_H
#define GIMP_PAINTER_XCF_PRESERVE_H
G_BEGIN_DECLS
GBytes *xcf_painter_snapshot_bytes (GInputStream *, GCancellable *, GError **);
void xcf_painter_discard_snapshot_pages (const void *, gsize);
guint xcf_painter_storage_live_files (void);
/* -1 absent, 0 present but unsupported/noncanonical, 1 recognized standard. */
gint xcf_painter_provenance_in_parasites (GBytes *, gsize, gsize);
/* Opaque, immutable save transaction. Prepare it before replacing any file. */
typedef struct _XcfPainterSave XcfPainterSave;
XcfPainterSave *xcf_painter_prepare_save (GimpImage *, GError **);
XcfPainterSave *xcf_painter_prepare_save_full (GimpImage *, GCancellable *, gboolean, GError **);
void            xcf_painter_free_save (XcfPainterSave *);
GCancellable   *xcf_painter_save_cancellable (XcfPainterSave *);
gboolean        xcf_painter_save_unchanged (XcfPainterSave *);
gboolean        xcf_painter_save_needs_v11 (XcfPainterSave *);
void            xcf_painter_commit_save (XcfPainterSave *);
guint32         xcf_painter_saved_id (XcfPainterSave *, GimpItem *);
guint32         xcf_painter_saved_tattoo_state (XcfPainterSave *, GimpImage *);
GimpParasite   *xcf_painter_image_parasite (XcfPainterSave *);
GimpParasite   *xcf_painter_origin_parasite (XcfPainterSave *, GObject *);
GimpParasite   *xcf_painter_item_parasite (XcfPainterSave *, GimpItem *);
GeglBuffer     *xcf_painter_saved_buffer (XcfPainterSave *, GimpDrawable *);
GimpLayerMode   xcf_painter_standard_mode (GimpLayerMode);
#ifdef __XCF_PRIVATE_H__
gint      xcf_painter_intercept_parasite (XcfInfo *, GPtrArray *, goffset);
gboolean  xcf_painter_finish_transport (XcfInfo *, GObject *, GPtrArray *);
GBytes   *xcf_painter_without_transport (XcfInfo *, GObject *, goffset, goffset);
gboolean  xcf_painter_write_owner_records (XcfInfo *, GObject *, GError **);
gboolean  xcf_painter_replaces_parasite (XcfInfo *, GObject *, const gchar *);
void      xcf_painter_capture_unknown (XcfInfo *, GPtrArray *, goffset, goffset);
void      xcf_painter_set_unknown_records (GObject *, GPtrArray *);
void      xcf_painter_capture_header (XcfInfo *, GObject *, goffset, goffset);
void      xcf_painter_capture_properties (XcfInfo *, GObject *, goffset, goffset);
void      xcf_painter_retarget_layer (XcfInfo *, GimpLayer *, GimpLayer *);
gboolean  xcf_painter_restore_layer (XcfInfo *, GimpImage *, GimpLayer **);
void      xcf_painter_restore_bindings (XcfInfo *, GimpImage *);
#endif
G_END_DECLS
#endif
