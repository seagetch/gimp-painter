/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_SESSION_H
#define GIMP_PAINTER_SESSION_H
#include "gimp-painter-options.h"
#include "core/gimpobject.h"
#include "core/gimpcoords.h"
G_BEGIN_DECLS
#define GIMP_TYPE_PAINTER_SESSION (gimp_painter_session_get_type ())
#define GIMP_PAINTER_SESSION(o) (G_TYPE_CHECK_INSTANCE_CAST ((o),GIMP_TYPE_PAINTER_SESSION,GimpPainterSession))
#define GIMP_IS_PAINTER_SESSION(o) (G_TYPE_CHECK_INSTANCE_TYPE ((o),GIMP_TYPE_PAINTER_SESSION))
typedef struct _GimpPainterSession GimpPainterSession;
typedef struct _GimpPainterSessionClass GimpPainterSessionClass;
struct _GimpPainterSession {GimpObject parent_instance;gboolean binding_failed;};
struct _GimpPainterSessionClass {GimpObjectClass parent_class;};
GType gimp_painter_session_get_type (void) G_GNUC_CONST;
GimpPainterSession *gimp_painter_session_new (GimpPainterMybrushOptions *options,GError **error);
gboolean gimp_painter_session_stroke_to (GimpPainterSession *session,GimpDrawable *drawable,gdouble seconds,const GimpCoords *coords,gboolean *split,GError **error);
/* Zero-pressure inactive input. A preceding press may finish positive-pressure
 * interpolated tail dabs in its existing transaction; settled hover only samples. */
gboolean gimp_painter_session_hover_to (GimpPainterSession *session,GimpDrawable *drawable,gdouble seconds,const GimpCoords *coords,gboolean *split,GError **error);
gboolean gimp_painter_session_finish (GimpPainterSession *session,GError **error);
gboolean gimp_painter_session_cancel (GimpPainterSession *session,GError **error);
/* One atomic generic operation, with independent logical Surface segments. */
gboolean gimp_painter_session_begin_batch (GimpPainterSession *session,gboolean push_undo,GError **error);
gboolean gimp_painter_session_begin_named_batch (GimpPainterSession *session,gboolean push_undo,const gchar *undo_description,GError **error);
gboolean gimp_painter_session_next_segment (GimpPainterSession *session,GError **error);
gboolean gimp_painter_session_end_batch (GimpPainterSession *session,gboolean commit,GError **error);
gboolean gimp_painter_session_is_active (GimpPainterSession *session);
gchar *gimp_painter_session_dup_error (GimpPainterSession *session);
G_END_DECLS
#endif
