/* Diagnostic-only generated TU include. No production/acceptance inclusion. */
#include <stdio.h>
#define OWNER_CAP 16
#define OWNER_EVENT_CAP 32
#define OWNER_PROBE_CAP (OWNER_CAP * 2)
typedef struct {
  guint probe, entering;
  gint64 elapsed;
} OwnerEvent;
typedef struct {
  GObject *object; /* weak; array address remains stable until finish */
  guint signal, owner, phase, depth, entries, exits, orphan;
  gulong hook, after;
  gint64 started, total, longest;
} OwnerProbe;
typedef struct {
  guint role, clock_slot;
  gboolean mapped, realized;
} OwnerWindow;
typedef struct {
  gboolean active, clipped;
  guint owners, probes, events;
  gint64 started;
  OwnerWindow windows[OWNER_CAP];
  OwnerProbe slots[OWNER_PROBE_CAP];
  OwnerEvent ring[OWNER_EVENT_CAP];
} OwnerObservation;
static OwnerObservation owner_observation;
static volatile guint owner_done;
static __attribute__((noinline)) void owner_observe_done (void) { owner_done = 1; }

static void
owner_record (guint probe, gboolean entering)
{
  OwnerObservation *o = &owner_observation;
  gint64 now = g_get_monotonic_time ();
  OwnerEvent *e = &o->ring[o->events++ % OWNER_EVENT_CAP];
  e->probe = probe; e->entering = entering; e->elapsed = now - o->started;
  if (probe < o->probes) {
    OwnerProbe *p = &o->slots[probe];
    if (entering) {
      ++p->entries;
      if (!p->depth++) p->started = now;
    } else {
      ++p->exits;
      if (!p->depth) ++p->orphan;
      else if (!--p->depth) {
        gint64 duration = now - p->started;
        p->total += duration; p->longest = MAX (p->longest, duration);
      }
    }
  }
}
static gboolean
owner_enter (GSignalInvocationHint *hint, guint count,
             const GValue *values, gpointer data)
{
  OwnerProbe *p = data; (void) hint;
  if (count && p->object == g_value_get_object (&values[0]))
    owner_record ((guint) (p - owner_observation.slots), TRUE);
  return TRUE;
}
static void
owner_exit (GObject *object, gpointer data)
{
  OwnerProbe *p = data; (void) object;
  owner_record ((guint) (p - owner_observation.slots), FALSE);
}
static guint
owner_connect (GObject *object, guint owner, guint phase, const gchar *name)
{
  OwnerObservation *o = &owner_observation;
  guint index = o->probes++;
  OwnerProbe *p = &o->slots[index];
  p->owner = owner; p->phase = phase; p->object = object;
  if (!object) return index;
  g_object_add_weak_pointer (object, (gpointer *) &p->object);
  p->signal = g_signal_lookup (name, G_OBJECT_TYPE (object));
  GSignalQuery query = {0}; g_signal_query (p->signal, &query);
  if (!query.signal_id || (query.signal_flags & G_SIGNAL_NO_HOOKS) ||
      query.n_params != 0 || query.return_type != G_TYPE_NONE) return index;
  p->hook = g_signal_add_emission_hook (p->signal, 0, owner_enter, p, NULL);
  if (p->hook) p->after = g_signal_connect_after (object, name, G_CALLBACK (owner_exit), p);
  return index;
}
static void
owner_observe_begin (GtkWidget *target, GtkWidget *harness)
{
  static gboolean once;
  OwnerObservation *o = &owner_observation;
  if (once) return;
  once = TRUE; o->active = TRUE; o->started = g_get_monotonic_time ();
  GtkWidget *target_window = gtk_widget_get_toplevel (target);
  GList *windows = gtk_window_list_toplevels ();
  /* Put target first so bounded coverage never silently excludes it. */
  windows = g_list_remove (windows, target_window);
  windows = g_list_prepend (windows, target_window);
  for (GList *i = windows; i; i = i->next) {
    if (o->owners == OWNER_CAP) { o->clipped = TRUE; break; }
    GtkWidget *w = i->data;
    guint n = o->owners++;
    OwnerWindow *entry = &o->windows[n];
    entry->role = w == target_window ? 1 : w == harness ? 2 :
                  g_str_equal (G_OBJECT_TYPE_NAME (w), "GimpImageWindow") ? 3 : 4;
    entry->mapped = gtk_widget_get_mapped (w); entry->realized = gtk_widget_get_realized (w);
    owner_connect (G_OBJECT (w), n, 1, "check-resize");
    GdkFrameClock *clock = gtk_widget_get_frame_clock (w);
    guint existing = o->probes;
    for (guint k = 0; clock && k < o->probes; ++k)
      if (o->slots[k].phase == 2 && o->slots[k].object == G_OBJECT (clock)) { existing = k; break; }
    entry->clock_slot = existing < o->probes ? existing :
      owner_connect (clock ? G_OBJECT (clock) : NULL, n, 2, "layout");
  }
  g_list_free (windows);
}
static void
owner_observe_dispatch (gboolean entering)
{
  if (owner_observation.active) owner_record (OWNER_PROBE_CAP, entering);
}
static void
owner_observe_finish (void)
{
  OwnerObservation *o = &owner_observation;
  if (!o->active) return;
  o->active = FALSE;
  /* Disconnect everything before printing; never retain objects or hook data. */
  for (guint i = 0; i < o->probes; ++i) {
    OwnerProbe *p = &o->slots[i];
    if (p->hook) g_signal_remove_emission_hook (p->signal, p->hook);
    if (p->object) {
      if (p->after) g_signal_handler_disconnect (p->object, p->after);
      g_object_remove_weak_pointer (p->object, (gpointer *) &p->object);
    }
  }
  /* Bypass g_test_init's g_print TAP decorator: the strict collector requires
   * raw bounded numeric records, not TAP comments. Never change global handlers. */
  printf ("OWNER_DIAG summary %u %u %u %u\n", o->owners, o->probes, o->events, o->clipped);
  for (guint i = 0; i < o->owners; ++i) {
    OwnerWindow *w = &o->windows[i];
    printf ("OWNER_DIAG window %u %u %u %u %u\n", i, w->role, w->mapped, w->realized, w->clock_slot);
  }
  for (guint i = 0; i < o->probes; ++i) {
    OwnerProbe *p = &o->slots[i];
    printf ("OWNER_DIAG probe %u %u %u %u %u %u %u %u %u %" G_GINT64_FORMAT " %" G_GINT64_FORMAT "\n",
      i, p->owner, p->phase, p->hook != 0, p->object != NULL,
      p->entries, p->exits, p->depth, p->orphan, p->total, p->longest);
  }
  guint count = MIN (o->events, OWNER_EVENT_CAP);
  for (guint i = o->events - count; i < o->events; ++i) {
    OwnerEvent *e = &o->ring[i % OWNER_EVENT_CAP];
    printf ("OWNER_DIAG event %u %u %" G_GINT64_FORMAT "\n", e->probe, e->entering, e->elapsed);
  }
  printf ("OWNER_DIAG complete\n");
  fflush (stdout);
  owner_observe_done ();
}
