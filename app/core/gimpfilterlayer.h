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
typedef struct
{
  guint32 version;                 /* currently 1 */
  guint64 generation;
  guint64 cache_generation;
  gboolean cache_complete;
  GimpFilterLayerState state;      /* diagnostic saved state; never resumed literally */
} GimpFilterLayerSnapshot;
/* Snapshot performs no evaluation. Restore is a loader-only checkpoint AFTER
 * definition, saved pixels and ALL layer topology have been restored. A complete
 * equal-generation cache stays clean; stale/incomplete cache schedules once.
 * Counters are normalized into a fresh runtime epoch; numerical equality with
 * stored counters is not promised. Their fresh/stale relationship is preserved.
 * In-flight jobs are cancelled/discarded, never resumed or waited for. */
gboolean gimp_filter_layer_get_snapshot_state (GimpFilterLayer *, GimpFilterLayerSnapshot *);
gboolean gimp_filter_layer_restore_snapshot_state (GimpFilterLayer *, const GimpFilterLayerSnapshot *, GError **);
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
/* Immutable typed snapshot for writers. Null root means no converted argument
 * model was supplied; original raw bytes may still contain unsupported arguments.
 * A nonnull snapshot with count zero means an explicitly empty argument array. Descriptor
 * and nested access never requires a live referenced object. Get_value copies
 * only scalar/known non-object values into a zero-initialized GValue; objects,
 * object arrays and nested arrays use the descriptor/nested APIs instead.
 * Every returned snapshot is independently owned and must be freed. Main-thread
 * API; types/values/reference IDs remain immutable after edits. Reference expiration
 * is queried at access time. */
typedef struct _GimpFilterArgumentsSnapshot GimpFilterArgumentsSnapshot;
typedef struct
{
  GType object_type;
  gint64 id;
  gboolean was_set;
  gboolean expired;
} GimpFilterArgumentReference;
/* Recursive import description. Scalar value must have exactly value_type.
 * Object slots require one descriptor; object arrays carry n_references. The
 * optional equally-sized targets array binds explicitly resolved live objects;
 * expired/unset descriptors cannot be bound. Nested arrays use children.
 * is_null preserves null versus empty arrays/strings. Borrowed inputs are never
 * retained; imported object links are weak. Limits: depth 32, total slots plus
 * references 65536. Unknown GTypes must stay in the outer raw metadata. */
typedef struct _GimpFilterArgumentSpec GimpFilterArgumentSpec;
struct _GimpFilterArgumentSpec
{
  GType value_type;
  gboolean is_null;
  const GValue *value;
  guint n_references;
  const GimpFilterArgumentReference *references;
  GObject * const *targets;
  guint n_children;
  const GimpFilterArgumentSpec *children;
};
GimpFilterArgumentsSnapshot *gimp_filter_arguments_snapshot_import (guint n_arguments,
                                                                   const GimpFilterArgumentSpec *, GError **);
gboolean gimp_filter_layer_set_definition_with_snapshot (GimpFilterLayer *, const gchar *procedure,
                                                         GBytes *serialized_definition,
                                                         const GimpFilterArgumentsSnapshot *, GError **);
GimpFilterArgumentsSnapshot *gimp_filter_layer_snapshot_arguments (GimpFilterLayer *);
void     gimp_filter_arguments_snapshot_free (GimpFilterArgumentsSnapshot *);
guint    gimp_filter_arguments_snapshot_count (const GimpFilterArgumentsSnapshot *);
GType    gimp_filter_arguments_snapshot_type (const GimpFilterArgumentsSnapshot *, guint argument);
gboolean gimp_filter_arguments_snapshot_is_null (const GimpFilterArgumentsSnapshot *, guint argument);
gboolean gimp_filter_arguments_snapshot_value (const GimpFilterArgumentsSnapshot *, guint argument, GValue *value);
guint    gimp_filter_arguments_snapshot_reference_count (const GimpFilterArgumentsSnapshot *, guint argument);
gboolean gimp_filter_arguments_snapshot_reference (const GimpFilterArgumentsSnapshot *, guint argument,
                                                   guint element, GimpFilterArgumentReference *);
GimpFilterArgumentsSnapshot *gimp_filter_arguments_snapshot_nested (const GimpFilterArgumentsSnapshot *, guint argument);

void        gimp_filter_layer_mark_as_loaded (GimpFilterLayer *);
void        gimp_filter_layer_invalidate     (GimpFilterLayer *);
/* Stop the current generation without discarding its definition or completed
 * pixels. Independent workers drain asynchronously; a subsequent edit may
 * start a new generation. Owner/main-context only. */
void        gimp_filter_layer_cancel         (GimpFilterLayer *);
GimpFilterLayerState gimp_filter_layer_get_state (GimpFilterLayer *);
gchar *     gimp_filter_layer_dup_error      (GimpFilterLayer *);
/* Session-local token for this layer's definition installations, including
 * explicit NULL args, imports and Undo/Redo. Compare only within the same live
 * object; never persist as lineage. Cache/dependency invalidations do not change
 * it. Zero means no definition installation (or an unavailable closed binding). */
guint64     gimp_filter_layer_get_definition_revision (GimpFilterLayer *);
guint64     gimp_filter_layer_get_generation (GimpFilterLayer *);
guint64     gimp_filter_layer_get_cache_generation (GimpFilterLayer *);
guint64     gimp_filter_layer_get_run_count  (GimpFilterLayer *);
/* Cumulative observed owner-thread maxima; diagnostics, not latency bounds. */
gint64      gimp_filter_layer_get_max_quantum_us (GimpFilterLayer *);
gint64      gimp_filter_layer_get_max_graph_quantum_us (GimpFilterLayer *);
gint64      gimp_filter_layer_get_max_read_quantum_us (GimpFilterLayer *);
/* Opaque argument-model bytes are mutually exclusive with converted arguments.
 * They are retained without inspection and never used for execution. Nonnull
 * zero-length bytes are distinct from no opaque model. Loader setter: no Undo.
 * Ordinary definition/snapshot setters explicitly clear this opaque state.
 * Duplicate and definition Undo/Redo preserve it. Getter returns an owned ref. */
gboolean gimp_filter_layer_set_definition_with_opaque_arguments (GimpFilterLayer *, const gchar *procedure,
                                                                GBytes *serialized_definition,
                                                                GBytes *opaque_arguments, GError **);
GBytes *gimp_filter_layer_ref_opaque_arguments (GimpFilterLayer *);
G_END_DECLS
#endif
