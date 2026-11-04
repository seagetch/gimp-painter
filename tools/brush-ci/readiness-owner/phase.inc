/* Diagnostic only. Main-thread observation; never included in acceptance TUs.
 * Emission hooks run AFTER RUN_FIRST class closures (state-flags-changed and
 * style-updated), but BEFORE RUN_LAST class closures (check-resize, layout and
 * theme changed). Thus style/state timestamps do not mark class-handler entry
 * and cannot by themselves time or order work inside those class closures.
 * notify disallows emission hooks, so its typed detail handler records
 * notification delivery instead. destroy also disallows hooks and uses a
 * validated zero-argument handler.
 * Clock replacement is sampled at marks/finish, not continuously: a transient
 * replacement between those checkpoints cannot be excluded by this trace. */
#include <stdio.h>
#include <string.h>
#define PHASE_OWNER_CAP 16
#define PHASE_EVENT_CAP 256
#define PHASE_HOOK_CAP (PHASE_OWNER_CAP * 6 + 1)
typedef struct {
  guint role, owner, value;
  gint64 elapsed;
} PhaseEvent;
typedef struct {
  GObject *object; /* weak, never prolong observed lifetime */
  guint signal, role, owner;
  gulong hook, handler;
} PhaseHook;
typedef struct {
  GObject *object, *clock; /* independent weak pointers */
  guint initial_active, initial_flags, final_active, final_flags;
  gboolean destroyed, clock_changed, had_clock;
} PhaseWindow;
typedef struct {
  gboolean active, clipped;
  guint owners, events, hooks, missing;
  gint64 started;
  PhaseWindow windows[PHASE_OWNER_CAP];
  PhaseHook slots[PHASE_HOOK_CAP];
  PhaseEvent records[PHASE_EVENT_CAP];
} PhaseObservation;
static PhaseObservation phase_observation;

static void
phase_record (guint role, guint owner, guint value)
{
  PhaseObservation *o = &phase_observation;
  if (!o->active) return;
  if (o->events == PHASE_EVENT_CAP) { o->clipped = TRUE; return; }
  PhaseEvent *e = &o->records[o->events++];
  e->role = role; e->owner = owner; e->value = value;
  e->elapsed = g_get_monotonic_time () - o->started;
}
static gboolean
phase_emission (GSignalInvocationHint *hint, guint count,
                const GValue *values, gpointer data)
{
  PhaseHook *p = data;
  (void) hint;
  if (count && p->object == g_value_get_object (&values[0])) {
    guint value = 0;
    /* The signal argument is the PREVIOUS flags; record the current flags. */
    if (p->role == 21)
      value = gtk_widget_get_state_flags (GTK_WIDGET (p->object));
    if (p->role == 26)
      phase_observation.windows[p->owner].destroyed = TRUE;
    phase_record (p->role, p->owner, value);
  }
  return TRUE;
}
static void
phase_active (GObject *object, GParamSpec *pspec, gpointer data)
{
  PhaseHook *p = data;
  (void) pspec;
  phase_record (20, p->owner, gtk_window_is_active (GTK_WINDOW (object)));
}
static void
phase_destroy (GObject *object, gpointer data)
{
  PhaseHook *p = data;
  (void) object;
  phase_observation.windows[p->owner].destroyed = TRUE;
  phase_record (26, p->owner, 0);
}
static void
phase_connect (GObject *object, guint owner, guint role, const gchar *name,
               GType parameter)
{
  PhaseObservation *o = &phase_observation;
  if (!object || o->hooks == PHASE_HOOK_CAP) { ++o->missing; return; }
  guint signal = g_signal_lookup (name, G_OBJECT_TYPE (object));
  GSignalQuery q = {0};
  g_signal_query (signal, &q);
  if (!q.signal_id || q.return_type != G_TYPE_NONE ||
      q.n_params != (parameter ? 1u : 0u) ||
      (parameter && (q.param_types[0] & ~G_SIGNAL_TYPE_STATIC_SCOPE) != parameter) ||
      (role != 20 && role != 26 && (q.signal_flags & G_SIGNAL_NO_HOOKS)) ||
      (role == 20 && !(q.signal_flags & G_SIGNAL_DETAILED))) {
    ++o->missing; return;
  }
  if (role == 20) {
    GParamSpec *property = g_object_class_find_property (G_OBJECT_GET_CLASS (object), "is-active");
    if (!property || G_PARAM_SPEC_VALUE_TYPE (property) != G_TYPE_BOOLEAN ||
        !(property->flags & G_PARAM_READABLE)) { ++o->missing; return; }
  }
  PhaseHook *p = &o->slots[o->hooks++];
  p->object = object; p->owner = owner; p->role = role; p->signal = signal;
  g_object_add_weak_pointer (object, (gpointer *) &p->object);
  if (role == 20)
    p->handler = g_signal_connect (object, "notify::is-active", G_CALLBACK (phase_active), p);
  else if (role == 26)
    p->handler = g_signal_connect (object, "destroy", G_CALLBACK (phase_destroy), p);
  else
    p->hook = g_signal_add_emission_hook (signal, 0, phase_emission, p, NULL);
  if (!p->handler && !p->hook) ++o->missing;
}
static void
phase_sample_windows (void)
{
  PhaseObservation *o = &phase_observation;
  for (guint i = 0; i < o->owners; ++i) {
    PhaseWindow *w = &o->windows[i];
    if (!w->object || w->destroyed) continue;
    w->final_active = gtk_window_is_active (GTK_WINDOW (w->object));
    w->final_flags = gtk_widget_get_state_flags (GTK_WIDGET (w->object));
    GObject *current = (GObject *) gtk_widget_get_frame_clock (GTK_WIDGET (w->object));
    if (current != w->clock || (w->had_clock && !w->clock))
      w->clock_changed = TRUE;
  }
}
static void
phase_observe_begin (void)
{
  static gboolean once;
  PhaseObservation *o = &phase_observation;
  if (once) return;
  once = TRUE; o->active = TRUE; o->started = g_get_monotonic_time ();
  GType image_window = g_type_from_name ("GimpImageWindow");
  GList *windows = gtk_window_list_toplevels ();
  for (GList *i = windows; i; i = i->next) {
    GObject *object = G_OBJECT (i->data);
    if (!image_window || !g_type_is_a (G_OBJECT_TYPE (object), image_window)) continue;
    if (o->owners == PHASE_OWNER_CAP) { o->clipped = TRUE; continue; }
    guint n = o->owners++;
    PhaseWindow *w = &o->windows[n];
    w->object = object;
    g_object_add_weak_pointer (object, (gpointer *) &w->object);
    w->initial_active = w->final_active = gtk_window_is_active (GTK_WINDOW (object));
    w->initial_flags = w->final_flags = gtk_widget_get_state_flags (GTK_WIDGET (object));
    w->clock = (GObject *) gtk_widget_get_frame_clock (GTK_WIDGET (object));
    w->had_clock = w->clock != NULL;
    if (w->clock) g_object_add_weak_pointer (w->clock, (gpointer *) &w->clock);
    phase_connect (object, n, 20, "notify", G_TYPE_PARAM);
    phase_connect (object, n, 21, "state-flags-changed", GTK_TYPE_STATE_FLAGS);
    phase_connect (object, n, 22, "style-updated", 0);
    phase_connect (object, n, 23, "check-resize", 0);
    phase_connect (w->clock, n, 24, "layout", 0);
    phase_connect (object, n, 26, "destroy", 0);
  }
  g_list_free (windows);
  if (!o->owners) ++o->missing;
  phase_connect ((GObject *) gtk_icon_theme_get_default (), PHASE_OWNER_CAP, 25, "changed", 0);
}
static void
phase_observe_mark (guint role)
{
  if (!phase_observation.active) return;
  if (role < 1 || role > 12) { ++phase_observation.missing; return; }
  phase_sample_windows ();
  phase_record (role, PHASE_OWNER_CAP, 0);
}
static void
phase_observe_finish (void)
{
  PhaseObservation *o = &phase_observation;
  if (!o->active) return;
  phase_sample_windows ();
  o->active = FALSE;
  for (guint i = 0; i < o->hooks; ++i) {
    PhaseHook *p = &o->slots[i];
    if (p->hook) g_signal_remove_emission_hook (p->signal, p->hook);
    if (p->object) {
      if (p->handler && g_signal_handler_is_connected (p->object, p->handler))
        g_signal_handler_disconnect (p->object, p->handler);
      g_object_remove_weak_pointer (p->object, (gpointer *) &p->object);
    }
  }
  for (guint i = 0; i < o->owners; ++i) {
    PhaseWindow *w = &o->windows[i];
    if (w->object) g_object_remove_weak_pointer (w->object, (gpointer *) &w->object);
    if (w->clock) g_object_remove_weak_pointer (w->clock, (gpointer *) &w->clock);
  }
  printf ("PHASE_DIAG summary %u %u %u %u\n", o->owners, o->events, o->clipped, o->missing);
  for (guint i = 0; i < o->owners; ++i) {
    PhaseWindow *w = &o->windows[i];
    printf ("PHASE_DIAG window %u %u %u %u %u %u %u\n", i,
      w->object != NULL && !w->destroyed, w->initial_active, w->initial_flags,
      w->final_active, w->final_flags, w->clock_changed);
  }
  for (guint i = 0; i < o->events; ++i) {
    PhaseEvent *e = &o->records[i];
    printf ("PHASE_DIAG event %u %u %u %" G_GINT64_FORMAT "\n", e->role, e->owner, e->value, e->elapsed);
  }
  printf ("PHASE_DIAG complete\n");
  fflush (stdout);
}
