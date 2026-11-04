/* Diagnostic-only, C-only, one-shot, GTK-main-thread observation.
 * Exact upstream GTK 3.24.49 originals were inspected: image width calls its
 * CSS gadget; text width builds/measures a layout; pixbuf get_size computes
 * helper dimensions. None redispatches its own sizing vfunc. Debian patch
 * byte identity is not asserted. Pixbuf render's direct static get_size call
 * is NOT observed. Pre-vfunc style/resize work is NOT observed. Never wrap
 * height slots: GTK baseline dispatch compares their pointer identities.
 *
 * A dedicated typed thunk forwards its saved base original exactly once.
 * Only pointer-equal inherited slots are installed. Unexpected overrides are
 * left untouched and make coverage incomplete, including parent-chain calls
 * which may measure the base portion but not the overriding method itself.
 * No receiver-type lookup selects an original. No public sizing redispatch.
 *
 * Class audits use peek, never class_ref or class initialization. Static GTK
 * originals remain valid; no class address is retained. Dynamic descendants
 * are omitted. Main-thread-only class mutation is required. Restoration
 * covers audited vfunc slots, not copies privately cached by other code;
 * cached thunks remain safe exact passthroughs after active becomes FALSE.
 * Owner roles: 0 unknown, 1 target top-level, 2 harness top-level,
 * 3 any GimpImageWindow top-level, 4 other. Role 3 is a grouped type role,
 * not proof of one original window; phase observation must establish that.
 * Unknown is an observation count (begin plus finish), not distinct classes.
 * Bucket totals are outermost within that bucket and may overlap; only the
 * union metric is outermost across all buckets. Never sum bucket durations.
 * Unknown overrides alone permit partial capture, never complete coverage.
 * A bounded finish failure is a rejected diagnostic, and the owned child
 * must be terminated by the collector at owner_observe_done, not resumed.
 * g_type_children allocates its returned sibling list outside timing; GTK
 * provides no bounded variant. Visits, storage, output and callback work are
 * bounded, but that GLib allocation is not claimed to be allocation-bounded.
 */
#include <stdio.h>
#include <stdint.h>
#define LEAF_TYPE_CAP 128
#define LEAF_PARENT_CAP 128
#define LEAF_COUNT_CAP 1000000000u
#define LEAF_MISSING_BASE 1u
#define LEAF_AUDIT_BOUND 2u
#define LEAF_UNKNOWN 4u
#define LEAF_DYNAMIC 8u
#define LEAF_RESTORE_CONFLICT 16u
#define LEAF_COUNTER_BOUND 32u
#define LEAF_CLOCK_INVALID 64u
#define LEAF_OWNER_BOUND 128u
#define LEAF_UNMATCHED 256u
#define LEAF_MISSING_OWNER_TYPE 512u
typedef void (*LeafImageWidth) (GtkWidget *, gint *, gint *);
typedef void (*LeafTextWidth) (GtkCellRenderer *, GtkWidget *, gint *, gint *);
typedef void (*LeafPixbufSize) (GtkCellRenderer *, GtkWidget *, const GdkRectangle *,
                                gint *, gint *, gint *, gint *);
typedef struct {
  guint entries, exits, depth, nested, outer;
  gint64 started, total, longest;
} LeafMetric;
typedef struct {
  GType root, installed_types[LEAF_TYPE_CAP];
  guint available, begin_seen, end_seen, installed, restored, late, unknown;
  guint uninit_begin, uninit_end, dynamic;
} LeafFamily;
typedef struct {
  gboolean begun, active, enabled, coverage_complete, restore_complete;
  guint flags;
  GType image_window_type;
  GObject *target, *harness; /* weak, never retain observed widgets */
  LeafImageWidth image_original;
  LeafTextWidth text_original;
  LeafPixbufSize pixbuf_original;
  LeafFamily families[3];
  LeafMetric metrics[3][5], all;
} LeafObservation;
static LeafObservation leaf_observation;
static void leaf_image_width (GtkWidget *, gint *, gint *);
static void leaf_text_width (GtkCellRenderer *, GtkWidget *, gint *, gint *);
static void leaf_pixbuf_size (GtkCellRenderer *, GtkWidget *, const GdkRectangle *,
                              gint *, gint *, gint *, gint *);

static void
leaf_increment (guint *value)
{
  if (*value < LEAF_COUNT_CAP) ++*value;
  else leaf_observation.flags |= LEAF_COUNTER_BOUND;
}

/* No object access after the original call: reentrant disposal is allowed. */
static guint
leaf_owner (GtkWidget *host)
{
  LeafObservation *o = &leaf_observation;
  if (!host) return 0;
  for (guint i = 0; i < LEAF_PARENT_CAP; ++i) {
    GtkWidget *parent = gtk_widget_get_parent (host);
    if (!parent) {
      if (o->target && G_OBJECT (host) == o->target) return 1;
      if (o->harness && G_OBJECT (host) == o->harness) return 2;
      if (o->image_window_type && g_type_is_a (G_OBJECT_TYPE (host), o->image_window_type)) return 3;
      return 4;
    }
    host = parent;
  }
  o->flags |= LEAF_OWNER_BOUND;
  return 0;
}

static void
leaf_metric_enter (LeafMetric *m, gint64 now)
{
  leaf_increment (&m->entries);
  if (m->depth) leaf_increment (&m->nested);
  else { m->started = now; leaf_increment (&m->outer); }
  ++m->depth; /* caller has checked both depths against the fixed cap */
}

static void
leaf_metric_exit (LeafMetric *m, gint64 now)
{
  leaf_increment (&m->exits);
  if (!m->depth) { leaf_observation.flags |= LEAF_UNMATCHED; return; }
  if (!--m->depth) {
    gint64 elapsed;
    if (now < 0 || m->started < 0 || now < m->started) {
      leaf_observation.flags |= LEAF_CLOCK_INVALID;
      return;
    }
    elapsed = now - m->started;
    if (elapsed > INT64_MAX - m->total) {
      m->total = INT64_MAX;
      leaf_observation.flags |= LEAF_COUNTER_BOUND;
    } else m->total += elapsed;
    if (elapsed > m->longest) m->longest = elapsed;
  }
}

static LeafMetric *
leaf_enter (guint family, GtkWidget *host)
{
  LeafObservation *o = &leaf_observation;
  LeafMetric *m;
  gint64 now;
  if (!o->active) return NULL;
  m = &o->metrics[family][leaf_owner (host)];
  if (m->depth == LEAF_COUNT_CAP || o->all.depth == LEAF_COUNT_CAP) {
    o->flags |= LEAF_COUNTER_BOUND;
    return NULL;
  }
  now = g_get_monotonic_time ();
  leaf_metric_enter (m, now);
  leaf_metric_enter (&o->all, now);
  return m;
}

static void
leaf_exit (LeafMetric *m)
{
  gint64 now;
  if (!m) return;
  now = g_get_monotonic_time ();
  leaf_metric_exit (m, now);
  leaf_metric_exit (&leaf_observation.all, now);
}

static void
leaf_image_width (GtkWidget *widget, gint *minimum, gint *natural)
{
  LeafMetric *m = leaf_enter (0, widget);
  ImageCall image_call = image_observe_enter (widget);
  IconCall icon_call = icon_observe_enter (widget);
  leaf_observation.image_original (widget, minimum, natural);
  icon_observe_exit (icon_call);
  image_observe_exit (image_call);
  leaf_exit (m);
}
static void
leaf_text_width (GtkCellRenderer *cell, GtkWidget *widget, gint *minimum, gint *natural)
{
  LeafMetric *m = leaf_enter (1, widget);
  leaf_observation.text_original (cell, widget, minimum, natural);
  leaf_exit (m);
}
static void
leaf_pixbuf_size (GtkCellRenderer *cell, GtkWidget *widget, const GdkRectangle *area,
                  gint *x, gint *y, gint *width, gint *height)
{
  LeafMetric *m = leaf_enter (2, widget);
  leaf_observation.pixbuf_original (cell, widget, area, x, y, width, height);
  leaf_exit (m);
}

/* Slots: 0 unavailable/override, 1 exact base original, 2 our family thunk. */
static guint
leaf_slot (guint family, gpointer klass)
{
  LeafObservation *o = &leaf_observation;
  if (family == 0) {
    LeafImageWidth fn = ((GtkWidgetClass *) klass)->get_preferred_width;
    return fn == leaf_image_width ? 2 : fn && fn == o->image_original ? 1 : 0;
  }
  if (family == 1) {
    LeafTextWidth fn = ((GtkCellRendererClass *) klass)->get_preferred_width;
    return fn == leaf_text_width ? 2 : fn && fn == o->text_original ? 1 : 0;
  }
  {
    LeafPixbufSize fn = ((GtkCellRendererClass *) klass)->get_size;
    return fn == leaf_pixbuf_size ? 2 : fn && fn == o->pixbuf_original ? 1 : 0;
  }
}

static void
leaf_set_slot (guint family, gpointer klass, gboolean install)
{
  LeafObservation *o = &leaf_observation;
  if (family == 0)
    ((GtkWidgetClass *) klass)->get_preferred_width = install ? leaf_image_width : o->image_original;
  else if (family == 1)
    ((GtkCellRendererClass *) klass)->get_preferred_width = install ? leaf_text_width : o->text_original;
  else
    ((GtkCellRendererClass *) klass)->get_size = install ? leaf_pixbuf_size : o->pixbuf_original;
}

/* Breadth-first audit, capped independently for each family. No recursion. */
static guint
leaf_types (GType root, GType types[LEAF_TYPE_CAP], gboolean *complete)
{
  guint count = 0;
  if (!root) return 0;
  types[count++] = root;
  for (guint at = 0; at < count; ++at) {
    guint n = 0;
    GType *children = g_type_children (types[at], &n);
    guint room = LEAF_TYPE_CAP - count;
    if (n > room) { *complete = FALSE; leaf_observation.flags |= LEAF_AUDIT_BOUND; }
    for (guint j = 0; j < n && j < room; ++j) types[count++] = children[j];
    g_free (children);
  }
  return count;
}

static gboolean
leaf_installed (LeafFamily *f, GType type)
{
  for (guint i = 0; i < f->installed; ++i)
    if (f->installed_types[i] == type) return TRUE;
  return FALSE;
}

static void
leaf_observe_begin (GtkWidget *target, GtkWidget *harness)
{
  static const gchar *const names[3] = {
    "GtkImage", "GtkCellRendererText", "GtkCellRendererPixbuf"
  };
  LeafObservation *o = &leaf_observation;
  GType types[3][LEAF_TYPE_CAP];
  guint counts[3];
  gboolean complete = TRUE;
  if (o->begun) return;
  o->begun = TRUE;
  o->restore_complete = TRUE;
  o->image_window_type = g_type_from_name ("GimpImageWindow");
  if (!o->image_window_type) o->flags |= LEAF_MISSING_OWNER_TYPE;
  for (guint family = 0; family < 3; ++family) {
    LeafFamily *f = &o->families[family];
    gpointer klass;
    f->root = g_type_from_name (names[family]);
    klass = f->root ? g_type_class_peek (f->root) : NULL;
    if (!klass || g_type_get_plugin (f->root)) {
      o->flags |= LEAF_MISSING_BASE;
    } else {
      if (family == 0) o->image_original = ((GtkWidgetClass *) klass)->get_preferred_width;
      if (family == 1) o->text_original = ((GtkCellRendererClass *) klass)->get_preferred_width;
      if (family == 2) o->pixbuf_original = ((GtkCellRendererClass *) klass)->get_size;
      f->available = leaf_slot (family, klass) == 1;
      if (!f->available) o->flags |= LEAF_MISSING_BASE;
    }
    counts[family] = leaf_types (f->root, types[family], &complete);
    f->begin_seen = counts[family];
    for (guint j = 0; j < counts[family]; ++j) {
      GType type = types[family][j];
      if (g_type_get_plugin (type)) {
        ++f->dynamic; o->flags |= LEAF_DYNAMIC;
        continue; /* no reading or mutation of an unloadable class slot */
      }
      klass = g_type_class_peek (type);
      if (!klass) ++f->uninit_begin;
      else if (f->available && leaf_slot (family, klass) != 1) {
        ++f->unknown; o->flags |= LEAF_UNKNOWN;
      }
    }
  }
  /* A missing base or clipped pre-audit installs nothing in any family. */
  if (!complete || (o->flags & (LEAF_MISSING_BASE | LEAF_DYNAMIC))) return;
  o->enabled = TRUE;
  for (guint family = 0; family < 3; ++family) {
    LeafFamily *f = &o->families[family];
    for (guint j = 0; j < counts[family]; ++j) {
      GType type = types[family][j];
      gpointer klass = g_type_class_peek (type);
      if (klass && !g_type_get_plugin (type) && leaf_slot (family, klass) == 1) {
        f->installed_types[f->installed++] = type;
        leaf_set_slot (family, klass, TRUE);
      }
    }
  }
  o->target = target ? G_OBJECT (gtk_widget_get_toplevel (target)) : NULL;
  o->harness = harness ? G_OBJECT (gtk_widget_get_toplevel (harness)) : NULL;
  if (o->target) g_object_add_weak_pointer (o->target, (gpointer *) &o->target);
  if (o->harness) g_object_add_weak_pointer (o->harness, (gpointer *) &o->harness);
  o->active = TRUE;
}

static void
leaf_print_metric (const gchar *prefix, LeafMetric *m)
{
  printf ("%s%u %u %u %u %u %" G_GINT64_FORMAT " %" G_GINT64_FORMAT "\n",
          prefix, m->entries, m->exits, m->depth, m->nested, m->outer, m->total, m->longest);
}

static void
leaf_observe_finish (void)
{
  LeafObservation *o = &leaf_observation;
  if (!o->begun) return;
  /* Idempotent finish, even if the pre-audit installed nothing. */
  static gboolean finished;
  if (finished) return;
  finished = TRUE;
  o->active = FALSE;
  for (guint family = 0; family < 3; ++family) {
    LeafFamily *f = &o->families[family];
    GType types[LEAF_TYPE_CAP];
    gboolean complete = TRUE;
    /* Restore every directly installed slot even when descendant audit clips. */
    for (guint j = 0; j < f->installed; ++j) {
      gpointer klass = g_type_class_peek (f->installed_types[j]);
      guint state = klass ? leaf_slot (family, klass) : 0;
      if (state == 2) leaf_set_slot (family, klass, FALSE);
      if (state) ++f->restored;
      else { o->flags |= LEAF_RESTORE_CONFLICT; o->restore_complete = FALSE; }
    }
    f->end_seen = leaf_types (f->root, types, &complete);
    if (!complete && o->enabled) o->restore_complete = FALSE;
    for (guint j = 0; j < f->end_seen; ++j) {
      GType type = types[j];
      gpointer klass;
      guint state;
      if (g_type_get_plugin (type)) {
        ++f->dynamic; o->flags |= LEAF_DYNAMIC;
        if (o->enabled) o->restore_complete = FALSE;
        continue; /* class lifetime is not pinned; do not inspect its slots */
      }
      klass = g_type_class_peek (type);
      if (!klass) { ++f->uninit_end; continue; }
      if (!o->enabled) continue;
      state = leaf_slot (family, klass);
      if (state == 2) {
        leaf_set_slot (family, klass, FALSE);
        ++f->restored; ++f->late;
      } else if (!leaf_installed (f, type)) {
        /* Includes late overrides and late uninstrumented original copies. */
        ++f->unknown; o->flags |= LEAF_UNKNOWN;
      }
    }
  }
  if (o->target) g_object_remove_weak_pointer (o->target, (gpointer *) &o->target);
  if (o->harness) g_object_remove_weak_pointer (o->harness, (gpointer *) &o->harness);
  o->target = o->harness = NULL;
  if (o->all.depth || o->all.entries != o->all.exits) o->flags |= LEAF_UNMATCHED;
  for (guint family = 0; family < 3; ++family)
    for (guint owner = 0; owner < 5; ++owner) {
      LeafMetric *m = &o->metrics[family][owner];
      if (m->depth || m->entries != m->exits) o->flags |= LEAF_UNMATCHED;
    }
  o->coverage_complete = o->enabled && o->flags == 0;
  printf ("LEAF_DIAG summary 1 %u %u %u %u\n", o->enabled, o->coverage_complete,
          o->restore_complete, o->flags);
  for (guint family = 0; family < 3; ++family) {
    LeafFamily *f = &o->families[family];
    printf ("LEAF_DIAG family %u %u %u %u %u %u %u %u %u %u %u\n", family + 1,
            f->available, f->begin_seen, f->end_seen, f->installed, f->restored,
            f->late, f->unknown, f->uninit_begin, f->uninit_end, f->dynamic);
  }
  for (guint family = 0; family < 3; ++family)
    for (guint owner = 0; owner < 5; ++owner) {
      printf ("LEAF_DIAG metric %u %u ", family + 1, owner);
      leaf_print_metric ("", &o->metrics[family][owner]);
    }
  leaf_print_metric ("LEAF_DIAG union ", &o->all);
  printf ("LEAF_DIAG complete\n");
  fflush (stdout);
}
