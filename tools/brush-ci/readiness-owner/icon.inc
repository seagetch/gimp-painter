/* Diagnostic-only requested-name equality, NOT resolved asset/cache/cold proof.
 * GTK main thread only. Include after image.inc. Begin after image begin; finish
 * before image finish. All widget reads precede original forwarding. No retain,
 * lookup, style context, size request, hash, pathname or name output. Inclusive
 * nested durations overlap; instrumentation changes image/leaf measured time.
 * Fixed dictionary compares exact bytes, not casefold/normalization. Unknown
 * statuses describe aggregates only and NEVER establish name equality.
 */
#include <string.h>
#define ICON_NAME_CAP 256u
#define ICON_BYTE_CAP 128u
#define ICON_GROUP_CAP 128u
#define ICON_SETUP 1u
#define ICON_NAME_BOUND 2u
#define ICON_DICTIONARY_BOUND 4u
#define ICON_GROUP_BOUND 8u
#define ICON_COUNTER_BOUND 16u
#define ICON_CLOCK_INVALID 32u
#define ICON_UNMATCHED 64u
#define ICON_METADATA_INVALID 128u

typedef struct {
  guint status, name_id, valid, storage;
  gint logical, pixel;
  guint scale, state, direction, fallback, ancestor, host, tracked;
} IconConfig;
typedef struct { IconConfig config; ImageMetric metric; } IconGroup;
typedef struct { guint group; gboolean active; gint64 started; } IconCall;
typedef struct {
  gboolean begun, active, enabled, finished;
  guint flags, names, groups, entries, exits, depth, dropped;
  char dictionary[ICON_NAME_CAP][ICON_BYTE_CAP];
  guint lengths[ICON_NAME_CAP];
  IconGroup group[ICON_GROUP_CAP];
  ImageMetric overflow;
  GParamSpec *fallback_spec; /* class-owned; GtkImage class remains registered */
  void (*get_property) (GObject *, guint, GValue *, GParamSpec *);
  GValue boolean_template;
} IconObservation;
static IconObservation icon_observation;

static void
icon_observe_begin (void)
{
  IconObservation *o = &icon_observation;
  GObjectClass *klass;
  if (o->begun) return;
  o->begun = TRUE;
  if (!image_observation.enabled || !image_observation.image_type) {
    o->flags |= ICON_SETUP; return;
  }
  klass = G_OBJECT_CLASS (g_type_class_peek (image_observation.image_type));
  if (!klass) { o->flags |= ICON_SETUP; return; }
  o->fallback_spec = g_object_class_find_property (klass, "use-fallback");
  if (!o->fallback_spec || !klass->get_property ||
      o->fallback_spec->owner_type != image_observation.image_type ||
      G_PARAM_SPEC_VALUE_TYPE (o->fallback_spec) != G_TYPE_BOOLEAN ||
      !(o->fallback_spec->flags & G_PARAM_READABLE) || !o->fallback_spec->param_id) {
    o->flags |= ICON_SETUP; return;
  }
  /* Cache the defining GtkImage getter, not a subclass override. GTK 3.24.49
   * gtkimage.c PROP_USE_FALLBACK simply sets a boolean from icon helper. The
   * immutable boolean GValue template has no owned heap payload: each entry
   * copies it by value, so nested calls have independent storage. */
  o->get_property = klass->get_property;
  g_value_init (&o->boolean_template, G_TYPE_BOOLEAN);
  o->enabled = o->active = TRUE;
}

static guint
icon_name_id (const gchar *name, guint *status)
{
  IconObservation *o = &icon_observation;
  guint length = 0;
  if (!name) { *status = 2; return 0; }
  while (length < ICON_BYTE_CAP && name[length]) ++length;
  if (length == ICON_BYTE_CAP) {
    *status = 4; o->flags |= ICON_NAME_BOUND; return 0;
  }
  for (guint i = 0; i < o->names; ++i)
    if (o->lengths[i] == length && !memcmp (o->dictionary[i], name, length)) {
      *status = 1; return i + 1;
    }
  if (o->names == ICON_NAME_CAP) {
    *status = 5; o->flags |= ICON_DICTIONARY_BOUND; return 0;
  }
  memcpy (o->dictionary[o->names], name, length + 1);
  o->lengths[o->names] = length;
  *status = 1; return ++o->names;
}

static gboolean
icon_config_equal (IconConfig a, IconConfig b)
{
  return a.status == b.status && a.name_id == b.name_id &&
    a.valid == b.valid && a.storage == b.storage && a.logical == b.logical &&
    a.pixel == b.pixel && a.scale == b.scale && a.state == b.state &&
    a.direction == b.direction && a.fallback == b.fallback &&
    a.ancestor == b.ancestor && a.host == b.host && a.tracked == b.tracked;
}

static IconCall
icon_observe_enter (GtkWidget *widget)
{
  IconObservation *o = &icon_observation;
  IconCall call = {0};
  IconConfig c = {0};
  ImageMetadata m;
  ImageMetric *metric;
  GValue fallback;
  guint index;
  const gchar *name = NULL;
  if (!o->active) return call;
  if (o->entries == IMAGE_COUNT_CAP || o->depth == IMAGE_COUNT_CAP) {
    o->flags |= ICON_COUNTER_BOUND;
    if (o->dropped < IMAGE_COUNT_CAP) ++o->dropped;
    return call;
  }
  m = image_metadata_read (widget);
  c.valid = m.valid; c.storage = m.storage; c.logical = m.logical_size;
  c.pixel = m.pixel_size; c.scale = m.scale; c.state = m.state;
  c.ancestor = m.ancestor; c.host = m.host;
  for (guint i = 0; i < image_observation.images; ++i)
    if (image_observation.identities[i].object &&
        image_observation.identities[i].object == (GObject *) widget) {
      c.tracked = 1; break;
    }
  if (!m.valid || G_OBJECT_TYPE (widget) != image_observation.image_type) { c.valid = 0; c.status = 6; o->flags |= ICON_METADATA_INVALID; }
  else {
    c.direction = gtk_widget_get_direction (widget);
    fallback = o->boolean_template;
    o->get_property ((GObject *) widget, o->fallback_spec->param_id,
                     &fallback, o->fallback_spec);
    c.fallback = g_value_get_boolean (&fallback) != FALSE;
    if (c.direction > 2) {
      c.valid = 0; c.status = 6; o->flags |= ICON_METADATA_INVALID;
    } else if (m.storage != GTK_IMAGE_ICON_NAME) c.status = 3;
    else {
      gtk_image_get_icon_name (GTK_IMAGE (widget), &name, NULL);
      c.name_id = icon_name_id (name, &c.status);
    }
  }
  if (c.status == 6) {
    guint tracked = c.tracked;
    memset (&c, 0, sizeof c); c.status = 6; c.tracked = tracked;
    c.logical = c.pixel = -1;
  }
  for (index = 0; index < o->groups; ++index)
    if (icon_config_equal (o->group[index].config, c)) break;
  if (index == o->groups && o->groups < ICON_GROUP_CAP) {
    o->group[index].config = c; ++o->groups;
  }
  if (index == ICON_GROUP_CAP) {
    o->flags |= ICON_GROUP_BOUND; metric = &o->overflow;
  } else { call.group = index + 1; metric = &o->group[index].metric; }
  ++metric->entries; ++metric->depth;
  if (o->depth) ++metric->nested;
  ++o->entries; ++o->depth;
  call.started = g_get_monotonic_time (); call.active = TRUE;
  return call;
}

static void
icon_observe_exit (IconCall call)
{
  IconObservation *o = &icon_observation;
  ImageMetric *m;
  gint64 now, elapsed;
  if (!call.active) return;
  if (!o->active || call.group > o->groups) { o->flags |= ICON_UNMATCHED; return; }
  m = call.group ? &o->group[call.group - 1].metric : &o->overflow;
  if (!o->depth || !m->depth) { o->flags |= ICON_UNMATCHED; return; }
  --o->depth; --m->depth; ++o->exits; ++m->exits;
  now = g_get_monotonic_time ();
  if (now < 0 || call.started < 0 || now < call.started) {
    o->flags |= ICON_CLOCK_INVALID; return;
  }
  elapsed = now - call.started;
  if (elapsed > INT64_MAX - m->total) { m->total = INT64_MAX; o->flags |= ICON_COUNTER_BOUND; }
  else m->total += elapsed;
  if (elapsed > m->longest) m->longest = elapsed;
}

static void
icon_print_metric (ImageMetric m)
{
  printf (" %u %u %u %u %" G_GINT64_FORMAT " %" G_GINT64_FORMAT "\n",
          m.entries, m.exits, m.depth, m.nested, m.total, m.longest);
}

static void
icon_observe_finish (void)
{
  IconObservation *o = &icon_observation;
  if (!o->begun || o->finished) return;
  o->finished = TRUE; o->active = FALSE;
  if (o->depth || o->entries != o->exits) o->flags |= ICON_UNMATCHED;
  for (guint i = 0; i < o->groups; ++i)
    if (o->group[i].metric.depth || o->group[i].metric.entries != o->group[i].metric.exits)
      o->flags |= ICON_UNMATCHED;
  if (o->overflow.depth || o->overflow.entries != o->overflow.exits) o->flags |= ICON_UNMATCHED;
  printf ("ICON_DIAG summary 1 %u %u %u %u %u %u %u %u\n", o->enabled,
          o->flags, o->names, o->groups, o->entries, o->exits, o->depth, o->dropped);
  for (guint i = 0; i < o->groups; ++i) {
    IconConfig c = o->group[i].config;
    printf ("ICON_DIAG group %u %u %u %u %u %d %d %u %u %u %u %u %u %u",
            i + 1, c.status, c.name_id, c.valid, c.storage, c.logical, c.pixel,
            c.scale, c.state, c.direction, c.fallback, c.ancestor, c.host, c.tracked);
    icon_print_metric (o->group[i].metric);
  }
  printf ("ICON_DIAG overflow"); icon_print_metric (o->overflow);
  printf ("ICON_DIAG complete\n"); fflush (stdout);
  /* Erase raw names after diagnostic output; never serialize dictionary. */
  memset (o->dictionary, 0, sizeof o->dictionary);
}
