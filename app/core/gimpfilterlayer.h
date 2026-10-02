/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef __GIMP_FILTER_LAYER_H__
#define __GIMP_FILTER_LAYER_H__
#include "gimplayer.h"
G_BEGIN_DECLS
#define GIMP_TYPE_FILTER_LAYER (gimp_filter_layer_get_type ())
#define GIMP_FILTER_LAYER(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIMP_TYPE_FILTER_LAYER, GimpFilterLayer))
#define GIMP_IS_FILTER_LAYER(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIMP_TYPE_FILTER_LAYER))
struct _GimpFilterLayer { GimpLayer parent_instance; gboolean binding_failed; };
struct _GimpFilterLayerClass { GimpLayerClass parent_class; };
typedef enum {
  GIMP_FILTER_LAYER_CLEAN, GIMP_FILTER_LAYER_WAITING, GIMP_FILTER_LAYER_PREPARING,
  GIMP_FILTER_LAYER_RUNNING, GIMP_FILTER_LAYER_CANCELLING, GIMP_FILTER_LAYER_IMPORTING,
  GIMP_FILTER_LAYER_FAILED, GIMP_FILTER_LAYER_CLOSED
} GimpFilterLayerState;
GType       gimp_filter_layer_get_type       (void) G_GNUC_CONST;
GimpLayer * gimp_filter_layer_new            (GimpImage *, gint, gint, const gchar *, gdouble, GimpLayerMode);
/* Serialized definition is the complete original PROP_FILTER_SPEC payload.
 * It is never normalized from execution_args, which are a separate optional
 * conversion. Callers retain their arguments; this function copies them. */
gboolean    gimp_filter_layer_set_definition (GimpFilterLayer *, const gchar *procedure,
                                              GBytes *serialized_definition,
                                              const GimpValueArray *execution_args, GError **);
/* User edit records a dedicated definition Undo; loader setter above does not. */
gboolean    gimp_filter_layer_edit_definition (GimpFilterLayer *, const gchar *procedure,
                                               GBytes *serialized_definition,
                                               const GimpValueArray *execution_args, GError **);
gchar *     gimp_filter_layer_dup_procedure  (GimpFilterLayer *);
GBytes *    gimp_filter_layer_ref_definition (GimpFilterLayer *);
GimpValueArray *gimp_filter_layer_dup_args   (GimpFilterLayer *);
/* Stable descriptor survives expiration; scalar object uses element 0.
 * Dup_args returns null for expired object arrays instead of dangling pointers. */
gboolean gimp_filter_layer_get_argument_reference (GimpFilterLayer *, guint argument, guint element,
                                                  GType *type, gint64 *id, gboolean *expired);
void        gimp_filter_layer_mark_as_loaded (GimpFilterLayer *);
void        gimp_filter_layer_invalidate     (GimpFilterLayer *);
GimpFilterLayerState gimp_filter_layer_get_state (GimpFilterLayer *);
gchar *     gimp_filter_layer_dup_error      (GimpFilterLayer *);
guint64     gimp_filter_layer_get_generation (GimpFilterLayer *);
guint64     gimp_filter_layer_get_cache_generation (GimpFilterLayer *);
guint64     gimp_filter_layer_get_run_count  (GimpFilterLayer *);
gint64      gimp_filter_layer_get_max_quantum_us (GimpFilterLayer *);
G_END_DECLS
#endif
