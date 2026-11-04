/* Diagnostic-only, one-shot GtkImage identity profile. GTK main thread only.
 * Snapshot existing images below existing GimpImageWindow hosts before the
 * original first-wait timer. No icon lookup, size query, prewarming or retain.
 * Ordinals identify snapshot objects, not addresses: weak slots are never
 * reused. Unknown calls are untracked calls, NOT new/distinct image counts.
 * First means first observed entry, not first-ever/cold/outermost request.
 * Call durations are inclusive; nested spans overlap and must not be summed
 * as disjoint work. Snapshot metadata and first/last ENTRY metadata differ.
 * Weak alive and destroy signal observed are independent facts.
 * GTK forall includes internal children but has no early-break API. Stored
 * visits are bounded; callbacks for remaining siblings may still be invoked.
 * gtk_window_list_toplevels allocates outside timing; its allocation is not
 * claimed bounded. No snapshot proves coverage of later/replacement widgets.
 * Public getter review: upstream GTK 3.24.49 gtkimage.c, gtkiconhelper.c and
 * gtkwidget.c. Image storage/size getters do not load a surface. Scale getter
 * may inspect the toplevel/monitor for unrealized widgets. No Debian patch
 * byte-identity claim. Visible/mapped do not establish painted/occlusion state.
 */
#include <stdio.h>
#include <stdint.h>
#define IMAGE_WINDOW_CAP 16u
#define IMAGE_TOPLEVEL_CAP 128u
#define IMAGE_NODE_CAP 4096u
#define IMAGE_CAP 256u
#define IMAGE_TREE_DEPTH_CAP 64u
#define IMAGE_PARENT_CAP 128u
#define IMAGE_COUNT_CAP 1000000000u
#define IMAGE_MISSING_TYPE 1u
#define IMAGE_WINDOW_BOUND 2u
#define IMAGE_NODE_BOUND 4u
#define IMAGE_IMAGE_BOUND 8u
#define IMAGE_TREE_BOUND 16u
#define IMAGE_PARENT_BOUND 32u
#define IMAGE_COUNTER_BOUND 64u
#define IMAGE_CLOCK_INVALID 128u
#define IMAGE_UNMATCHED 256u
#define IMAGE_METADATA_INVALID 512u
#define IMAGE_DESTROY_MISSING 1024u
#define IMAGE_SEMANTIC_MISSING 2048u
#define IMAGE_SNAPSHOT_MUTATED 4096u

typedef struct {
  guint valid, storage;
  gint logical_size, pixel_size;
  guint visible, mapped, child_visible, realized, state, scale, ancestor, host;
} ImageMetadata;
typedef struct {
  guint entries, exits, depth, nested;
  gint64 total, longest;
} ImageMetric;
typedef struct {
  GObject *object; /* weak, never recycled after clearing */
  gulong destroy_handler;
  guint window, destroyed, changed;
  ImageMetadata snapshot, first, last;
  ImageMetric metrics[2];
} ImageIdentity;
typedef struct {
  GtkWidget *widget; /* weak, setup only */
  GtkWidget *initial_parent; /* address comparison only, never dereferenced */
  guint window, depth;
} ImageNode;
typedef struct {
  guint ordinal;
  gboolean active, first;
  gint64 started;
} ImageCall;
typedef struct {
  gboolean begun, active, enabled, finished;
  guint flags, windows, nodes, images, entries, exits, depth;
  GType image_type, container_type, window_type, ancestor_types[5];
  GObject *window_objects[IMAGE_WINDOW_CAP]; /* weak */
  ImageNode pending[IMAGE_NODE_CAP];
  ImageIdentity identities[IMAGE_CAP];
  ImageMetadata unknown_first, unknown_last;
  ImageMetric unknown;
} ImageObservation;
static ImageObservation image_observation;

static ImageMetadata
image_metadata_empty (void)
{
  ImageMetadata m = {0};
  m.logical_size = m.pixel_size = -1;
  return m;
}

/* All metadata is collected BEFORE forwarding. No widget pointer in a token. */
static ImageMetadata
image_metadata_read (GtkWidget *widget)
{
  ImageObservation *o = &image_observation;
  ImageMetadata m = image_metadata_empty ();
  GtkIconSize logical = GTK_ICON_SIZE_INVALID;
  GtkWidget *parent;
  gboolean reached_root = FALSE;
  m.storage = gtk_image_get_storage_type (GTK_IMAGE (widget));
  if (m.storage == GTK_IMAGE_ICON_NAME)
    gtk_image_get_icon_name (GTK_IMAGE (widget), NULL, &logical);
  else if (m.storage == GTK_IMAGE_GICON)
    gtk_image_get_gicon (GTK_IMAGE (widget), NULL, &logical);
  else if (m.storage == GTK_IMAGE_STOCK) {
    G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    gtk_image_get_stock (GTK_IMAGE (widget), NULL, &logical);
    G_GNUC_END_IGNORE_DEPRECATIONS
  } else if (m.storage == GTK_IMAGE_ICON_SET) {
    G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    gtk_image_get_icon_set (GTK_IMAGE (widget), NULL, &logical);
    G_GNUC_END_IGNORE_DEPRECATIONS
  }
  if (m.storage == GTK_IMAGE_ICON_NAME || m.storage == GTK_IMAGE_GICON ||
      m.storage == GTK_IMAGE_STOCK || m.storage == GTK_IMAGE_ICON_SET)
    m.logical_size = (gint) logical;
  m.pixel_size = gtk_image_get_pixel_size (GTK_IMAGE (widget));
  m.visible = gtk_widget_get_visible (widget) != FALSE;
  m.mapped = gtk_widget_get_mapped (widget) != FALSE;
  m.child_visible = gtk_widget_get_child_visible (widget) != FALSE;
  m.realized = gtk_widget_get_realized (widget) != FALSE;
  m.state = gtk_widget_get_state_flags (widget);
  parent = widget;
  for (guint depth = 0; depth < IMAGE_PARENT_CAP; ++depth) {
    GtkWidget *next = gtk_widget_get_parent (parent);
    if (parent != widget && !m.ancestor) {
      for (guint kind = 0; kind < 5; ++kind)
        if (o->ancestor_types[kind] &&
            g_type_is_a (G_OBJECT_TYPE (parent), o->ancestor_types[kind])) {
          m.ancestor = kind + 1; break;
        }
    }
    if (!next) {
      reached_root = TRUE;
      if (g_type_is_a (G_OBJECT_TYPE (parent), o->window_type)) {
        m.host = IMAGE_WINDOW_CAP + 1;
        for (guint w = 0; w < o->windows; ++w)
          if (o->window_objects[w] == (GObject *) parent) { m.host = w + 1; break; }
      } else m.host = IMAGE_WINDOW_CAP + 2;
      break;
    }
    parent = next;
  }
  if (!reached_root) {
    o->flags |= IMAGE_PARENT_BOUND;
    return image_metadata_empty ();
  }
  if (!m.ancestor) m.ancestor = 6;
  /* Unrealized GTK scale queries walk to the root internally. Establish the
   * explicit parent bound first; never call that getter on an unbounded chain. */
  m.scale = gtk_widget_get_scale_factor (widget);
  if (m.storage > GTK_IMAGE_SURFACE || m.logical_size < -1 ||
      m.pixel_size < -1 || !m.scale || m.scale > INT32_MAX) {
    o->flags |= IMAGE_METADATA_INVALID;
    return image_metadata_empty ();
  }
  m.valid = TRUE;
  return m;
}

static gboolean
image_metadata_equal (ImageMetadata a, ImageMetadata b)
{
  return a.valid == b.valid && a.storage == b.storage &&
    a.logical_size == b.logical_size && a.pixel_size == b.pixel_size &&
    a.visible == b.visible && a.mapped == b.mapped &&
    a.child_visible == b.child_visible && a.realized == b.realized &&
    a.state == b.state && a.scale == b.scale &&
    a.ancestor == b.ancestor && a.host == b.host;
}

static ImageCall
image_observe_enter (GtkWidget *widget)
{
  ImageObservation *o = &image_observation;
  ImageCall call = {0};
  ImageIdentity *identity = NULL;
  ImageMetric *metric;
  ImageMetadata metadata;
  if (!o->active) return call;
  for (guint i = 0; i < o->images; ++i)
    if (o->identities[i].object && o->identities[i].object == (GObject *) widget) {
      identity = &o->identities[i]; call.ordinal = i + 1; break;
    }
  call.first = identity && !identity->metrics[0].entries;
  metric = identity ? &identity->metrics[call.first ? 0 : 1] : &o->unknown;
  if (o->entries == IMAGE_COUNT_CAP || o->depth == IMAGE_COUNT_CAP ||
      metric->entries == IMAGE_COUNT_CAP || metric->depth == IMAGE_COUNT_CAP) {
    o->flags |= IMAGE_COUNTER_BOUND; return call;
  }
  metadata = image_metadata_read (widget);
  if (identity) {
    ImageMetadata previous = call.first ? identity->snapshot : identity->last;
    if (!image_metadata_equal (previous, metadata)) ++identity->changed;
    if (call.first) identity->first = metadata;
    identity->last = metadata;
  } else {
    if (!metric->entries) o->unknown_first = metadata;
    o->unknown_last = metadata;
  }
  ++metric->entries; ++metric->depth;
  if (o->depth) ++metric->nested;
  ++o->entries; ++o->depth;
  call.started = g_get_monotonic_time ();
  call.active = TRUE;
  return call;
}

static void
image_observe_exit (ImageCall call)
{
  ImageObservation *o = &image_observation;
  ImageMetric *metric;
  gint64 now, elapsed;
  if (!call.active) return;
  /* Fixed ordinal access only: original may have destroyed/finalized widget. */
  if (call.ordinal > o->images) { o->flags |= IMAGE_UNMATCHED; return; }
  metric = call.ordinal ? &o->identities[call.ordinal - 1].metrics[call.first ? 0 : 1] : &o->unknown;
  if (!o->depth || !metric->depth) { o->flags |= IMAGE_UNMATCHED; return; }
  --o->depth; --metric->depth; ++o->exits; ++metric->exits;
  now = g_get_monotonic_time ();
  if (now < 0 || call.started < 0 || now < call.started) {
    o->flags |= IMAGE_CLOCK_INVALID; return;
  }
  elapsed = now - call.started;
  if (elapsed > INT64_MAX - metric->total) {
    metric->total = INT64_MAX; o->flags |= IMAGE_COUNTER_BOUND;
  } else metric->total += elapsed;
  if (elapsed > metric->longest) metric->longest = elapsed;
}

/* Setup/finish only below, except the fixed destroy notification. */
static void
image_destroyed (GtkWidget *widget, gpointer data)
{
  ImageIdentity *identity = data;
  (void) widget;
  identity->destroyed = TRUE;
}

static void
image_snapshot_child (GtkWidget *widget, gpointer data)
{
  ImageObservation *o = &image_observation;
  ImageNode *parent = data;
  for (guint i = 0; i < o->nodes; ++i)
    if (o->pending[i].widget == widget) return;
  if (parent && parent->depth == IMAGE_TREE_DEPTH_CAP) {
    o->flags |= IMAGE_TREE_BOUND; return;
  }
  if (o->nodes == IMAGE_NODE_CAP) { o->flags |= IMAGE_NODE_BOUND; return; }
  ImageNode *node = &o->pending[o->nodes++];
  node->widget = widget;
  node->initial_parent = gtk_widget_get_parent (widget);
  g_object_add_weak_pointer ((GObject *) widget, (gpointer *) &node->widget);
  node->window = parent ? parent->window : o->windows;
  node->depth = parent ? parent->depth + 1 : 0;
}

static void
image_observe_begin (void)
{
  static const gchar *const names[5] = {
    "GtkToolButton", "GimpItemTreeView", "GimpEditor", "GimpDockbook", "GimpDisplayShell"
  };
  ImageObservation *o = &image_observation;
  GList *windows;
  guint scanned = 0;
  guint destroy_signal;
  GSignalQuery query = {0};
  if (o->begun) return;
  o->begun = TRUE;
  o->unknown_first = o->unknown_last = image_metadata_empty ();
  o->image_type = g_type_from_name ("GtkImage");
  o->container_type = g_type_from_name ("GtkContainer");
  o->window_type = g_type_from_name ("GimpImageWindow");
  for (guint i = 0; i < 5; ++i) {
    o->ancestor_types[i] = g_type_from_name (names[i]);
    if (!o->ancestor_types[i]) o->flags |= IMAGE_SEMANTIC_MISSING;
  }
  if (!o->image_type || !o->container_type || !o->window_type) {
    o->flags |= IMAGE_MISSING_TYPE; return;
  }
  destroy_signal = g_signal_lookup ("destroy", o->image_type);
  g_signal_query (destroy_signal, &query);
  if (!query.signal_id || query.return_type != G_TYPE_NONE || query.n_params)
    o->flags |= IMAGE_DESTROY_MISSING;
  windows = gtk_window_list_toplevels ();
  for (GList *item = windows; item; item = item->next) {
    GObject *object;
    if (scanned++ == IMAGE_TOPLEVEL_CAP) { o->flags |= IMAGE_WINDOW_BOUND; break; }
    object = (GObject *) item->data;
    if (!g_type_is_a (G_OBJECT_TYPE (object), o->window_type)) continue;
    if (o->windows == IMAGE_WINDOW_CAP) { o->flags |= IMAGE_WINDOW_BOUND; break; }
    o->window_objects[o->windows++] = object;
    g_object_add_weak_pointer (object, (gpointer *) &o->window_objects[o->windows - 1]);
    image_snapshot_child ((GtkWidget *) object, NULL);
  }
  g_list_free (windows);
  for (guint at = 0; at < o->nodes; ++at) {
    ImageNode *node = &o->pending[at];
    GtkWidget *widget = node->widget;
    if (!widget) { o->flags |= IMAGE_SNAPSHOT_MUTATED; continue; }
    if (gtk_widget_get_parent (widget) != node->initial_parent) o->flags |= IMAGE_SNAPSHOT_MUTATED;
    if (g_type_is_a (G_OBJECT_TYPE (widget), o->image_type)) {
      if (o->images == IMAGE_CAP) o->flags |= IMAGE_IMAGE_BOUND;
      else {
        ImageIdentity *identity = &o->identities[o->images++];
        identity->object = (GObject *) widget; identity->window = node->window;
        g_object_add_weak_pointer (identity->object, (gpointer *) &identity->object);
        identity->snapshot = image_metadata_read (widget);
        if (identity->snapshot.host != identity->window) o->flags |= IMAGE_SNAPSHOT_MUTATED;
        identity->first = identity->last = image_metadata_empty ();
        if (!(o->flags & IMAGE_DESTROY_MISSING))
          identity->destroy_handler = g_signal_connect (widget, "destroy", G_CALLBACK (image_destroyed), identity);
        if (!identity->destroy_handler) o->flags |= IMAGE_DESTROY_MISSING;
      }
    }
    if (g_type_is_a (G_OBJECT_TYPE (widget), o->container_type))
      gtk_container_forall (GTK_CONTAINER (widget), image_snapshot_child, node);
  }
  /* Virtual forall can mutate a previously queued sibling. Weak slots avoid
   * stale dereferences and the final audit reports, rather than hides, mutation.
   * All queue registrations are removed before the timed observation starts. */
  for (guint i = 0; i < o->nodes; ++i) {
    ImageNode *node = &o->pending[i];
    if (!node->widget) o->flags |= IMAGE_SNAPSHOT_MUTATED;
    else {
      if (gtk_widget_get_parent (node->widget) != node->initial_parent)
        o->flags |= IMAGE_SNAPSHOT_MUTATED;
      g_object_remove_weak_pointer ((GObject *) node->widget, (gpointer *) &node->widget);
    }
    node->widget = node->initial_parent = NULL;
  }
  o->enabled = o->active = TRUE;
}

static void
image_print_metadata (guint ordinal, guint phase, ImageMetadata m)
{
  printf ("IMAGE_DIAG meta %u %u %u %u %d %d %u %u %u %u %u %u %u %u\n",
          ordinal, phase, m.valid, m.storage, m.logical_size, m.pixel_size,
          m.visible, m.mapped, m.child_visible, m.realized, m.state, m.scale,
          m.ancestor, m.host);
}

static void
image_print_metric (guint ordinal, guint kind, ImageMetric m)
{
  printf ("IMAGE_DIAG metric %u %u %u %u %u %u %" G_GINT64_FORMAT " %" G_GINT64_FORMAT "\n",
          ordinal, kind, m.entries, m.exits, m.depth, m.nested, m.total, m.longest);
}

static void
image_observe_finish (void)
{
  ImageObservation *o = &image_observation;
  guint alive[IMAGE_CAP], window_alive[IMAGE_WINDOW_CAP];
  if (!o->begun || o->finished) return;
  o->finished = TRUE; o->active = FALSE;
  if (o->depth || o->entries != o->exits) o->flags |= IMAGE_UNMATCHED;
  for (guint i = 0; i < o->images; ++i) {
    ImageIdentity *identity = &o->identities[i];
    alive[i] = identity->object != NULL;
    if (identity->object) {
      if (identity->destroy_handler &&
          g_signal_handler_is_connected (identity->object, identity->destroy_handler))
        g_signal_handler_disconnect (identity->object, identity->destroy_handler);
      g_object_remove_weak_pointer (identity->object, (gpointer *) &identity->object);
    }
    identity->object = NULL;
    for (guint kind = 0; kind < 2; ++kind)
      if (identity->metrics[kind].depth ||
          identity->metrics[kind].entries != identity->metrics[kind].exits)
        o->flags |= IMAGE_UNMATCHED;
  }
  if (o->unknown.depth || o->unknown.entries != o->unknown.exits) o->flags |= IMAGE_UNMATCHED;
  for (guint i = 0; i < o->windows; ++i) {
    window_alive[i] = o->window_objects[i] != NULL;
    if (o->window_objects[i])
      g_object_remove_weak_pointer (o->window_objects[i], (gpointer *) &o->window_objects[i]);
    o->window_objects[i] = NULL;
  }
  printf ("IMAGE_DIAG summary 1 %u %u %u %u %u %u %u %u\n", o->enabled,
          o->flags, o->windows, o->nodes, o->images, o->entries, o->exits, o->depth);
  for (guint i = 0; i < o->windows; ++i)
    printf ("IMAGE_DIAG window %u %u\n", i + 1, window_alive[i]);
  for (guint i = 0; i < o->images; ++i) {
    ImageIdentity *identity = &o->identities[i];
    printf ("IMAGE_DIAG image %u %u %u %u %u\n", i + 1, identity->window,
            alive[i], identity->destroyed, identity->changed);
    image_print_metadata (i + 1, 0, identity->snapshot);
    image_print_metadata (i + 1, 1, identity->first);
    image_print_metadata (i + 1, 2, identity->last);
    image_print_metric (i + 1, 1, identity->metrics[0]);
    image_print_metric (i + 1, 2, identity->metrics[1]);
  }
  image_print_metadata (0, 1, o->unknown_first);
  image_print_metadata (0, 2, o->unknown_last);
  image_print_metric (0, 0, o->unknown);
  printf ("IMAGE_DIAG complete\n");
  fflush (stdout);
}
