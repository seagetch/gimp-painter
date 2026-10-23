/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_XCF_PRESERVE_H
#define GIMP_PAINTER_XCF_PRESERVE_H
#include "../painter/gimp-painter-visibility.h"
G_BEGIN_DECLS
GBytes *xcf_painter_snapshot_bytes (GInputStream *, GCancellable *, GError **);
void xcf_painter_discard_snapshot_pages (const void *, gsize);
guint xcf_painter_storage_live_files (void);
/* -1 absent, 0 present but unsupported/noncanonical, 1 recognized standard. */
gint xcf_painter_provenance_in_parasites (GBytes *, gsize, gsize);
/* Opaque, immutable save transaction. Prepare it before replacing any file. */
struct GIMP_PAINTER_PRIVATE _XcfPainterSave;
typedef struct _XcfPainterSave XcfPainterSave;
GIMP_PAINTER_C_ENTRY XcfPainterSave *xcf_painter_prepare_save (GimpImage *, GError **);
GIMP_PAINTER_C_ENTRY XcfPainterSave *xcf_painter_prepare_save_full (GimpImage *, GCancellable *, gboolean, GError **);
GIMP_PAINTER_C_ENTRY void            xcf_painter_free_save (XcfPainterSave *);
GIMP_PAINTER_C_ENTRY GCancellable   *xcf_painter_save_cancellable (XcfPainterSave *);
GIMP_PAINTER_C_ENTRY gboolean        xcf_painter_save_unchanged (XcfPainterSave *);
GIMP_PAINTER_C_ENTRY gboolean        xcf_painter_save_needs_v11 (XcfPainterSave *);
GIMP_PAINTER_C_ENTRY void            xcf_painter_commit_save (XcfPainterSave *);
GIMP_PAINTER_C_ENTRY guint32         xcf_painter_saved_id (XcfPainterSave *, GimpItem *);
GIMP_PAINTER_C_ENTRY guint32         xcf_painter_saved_tattoo_state (XcfPainterSave *, GimpImage *);
GIMP_PAINTER_C_ENTRY GimpParasite   *xcf_painter_image_parasite (XcfPainterSave *);
GIMP_PAINTER_C_ENTRY GimpParasite   *xcf_painter_origin_parasite (XcfPainterSave *, GObject *);
GIMP_PAINTER_C_ENTRY GimpParasite   *xcf_painter_item_parasite (XcfPainterSave *, GimpItem *);
GIMP_PAINTER_C_ENTRY GeglBuffer     *xcf_painter_saved_buffer (XcfPainterSave *, GimpDrawable *);
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
