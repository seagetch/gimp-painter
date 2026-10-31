/* Passive, bounded timing probes. Weak pointers avoid retaining any widget.
 * Draw's TRUE accumulator can suppress an after handler: unmatched entries
 * are reported, never interpreted as a measured draw duration. */
typedef struct {
  guint probe;
  gboolean entering;
  gint64 elapsed;
} TransformPopupTimingEvent;

typedef struct _TransformPopupTiming TransformPopupTiming;
typedef struct {
  TransformPopupTiming *timing;
  GObject *object;
  const gchar *label;
  gulong before, after;
  guint signal;
  guint index, entries, exits, depth, orphan_exits;
  gint64 started, total, longest;
} TransformPopupTimingProbe;

struct _TransformPopupTiming {
  gint64 started;
  guint events;
  TransformPopupTimingEvent ring[32];
  TransformPopupTimingProbe probes[5];
};

static void
transform_popup_timing_record (TransformPopupTimingProbe *probe,
                               gboolean entering)
{
  TransformPopupTiming *timing = probe->timing;
  gint64 now = g_get_monotonic_time ();
  TransformPopupTimingEvent *event = &timing->ring[timing->events++ % G_N_ELEMENTS (timing->ring)];
  event->probe = probe->index; event->entering = entering;
  event->elapsed = now - timing->started;
  if (entering) {
    ++probe->entries;
    if (!probe->depth++) probe->started = now;
  } else {
    ++probe->exits;
    if (!probe->depth) ++probe->orphan_exits;
    else if (!--probe->depth) {
      gint64 duration = now - probe->started;
      probe->total += duration; probe->longest = MAX (probe->longest, duration);
    }
  }
}

/* An emission hook precedes already-connected normal handlers; an ordinary
 * connect() here could miss GTK's earlier paint handler entirely. */
static gboolean
transform_popup_timing_enter (GSignalInvocationHint *hint, guint count,
                              const GValue *values, gpointer data)
{
  TransformPopupTimingProbe *probe = data;
  (void) hint;
  if (count && probe->object == g_value_get_object (&values[0]))
    transform_popup_timing_record (probe, TRUE);
  return TRUE;
}

static gboolean
transform_popup_timing_draw_after (GtkWidget *widget, cairo_t *cr, gpointer data)
{
  (void) widget; (void) cr;
  transform_popup_timing_record (data, FALSE); return FALSE;
}

static void
transform_popup_timing_phase_after (GdkFrameClock *clock, gpointer data)
{
  (void) clock; transform_popup_timing_record (data, FALSE);
}

static void
transform_popup_timing_connect (TransformPopupTiming *timing, guint index,
                                GObject *object, const gchar *label,
                                const gchar *signal, gboolean draw)
{
  TransformPopupTimingProbe *probe = &timing->probes[index];
  probe->timing = timing; probe->index = index; probe->label = label;
  probe->object = object;
  if (!object) return;
  g_object_add_weak_pointer (object, (gpointer *) &probe->object);
  probe->signal = g_signal_lookup (signal, G_OBJECT_TYPE (object));
  GSignalQuery query = {0};
  g_signal_query (probe->signal, &query);
  if (!query.signal_id || (query.signal_flags & G_SIGNAL_NO_HOOKS)) return;
  probe->before = g_signal_add_emission_hook (probe->signal, 0,
                                             transform_popup_timing_enter, probe, NULL);
  if (!probe->before) return;
  probe->after = g_signal_connect_after (object, signal,
    draw ? G_CALLBACK (transform_popup_timing_draw_after) : G_CALLBACK (transform_popup_timing_phase_after), probe);
}

static void
transform_popup_timing_finish (TransformPopupTiming *timing, guint ordinal,
                               gboolean failed)
{
  for (guint i = 0; i < G_N_ELEMENTS (timing->probes); ++i) {
    TransformPopupTimingProbe *probe = &timing->probes[i];
    if (probe->before) g_signal_remove_emission_hook (probe->signal, probe->before);
    if (probe->object) {
      if (probe->after) g_signal_handler_disconnect (probe->object, probe->after);
      g_object_remove_weak_pointer (probe->object, (gpointer *) &probe->object);
    }
    if (failed)
      g_test_message ("Geometry timing wait=%u probe=%s hooked=%d entries=%u exits=%u unmatched=%u orphan-exits=%u total-us=%" G_GINT64_FORMAT " longest-us=%" G_GINT64_FORMAT,
                      ordinal, probe->label, probe->before != 0, probe->entries, probe->exits, probe->depth,
                      probe->orphan_exits, probe->total, probe->longest);
  }
  if (failed) {
    guint count = MIN (timing->events, G_N_ELEMENTS (timing->ring));
    for (guint i = timing->events - count; i < timing->events; ++i) {
      TransformPopupTimingEvent *event = &timing->ring[i % G_N_ELEMENTS (timing->ring)];
      g_test_message ("Geometry timing wait=%u probe=%s event=%s elapsed-us=%" G_GINT64_FORMAT,
                      ordinal, timing->probes[event->probe].label,
                      event->entering ? "enter" : "exit", event->elapsed);
    }
  }
}

