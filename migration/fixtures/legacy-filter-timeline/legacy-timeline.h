#ifndef LEGACY_TIMELINE_H
#define LEGACY_TIMELINE_H
/* Observation only: ordinal counts run attempts; legacy has no generation token. */
static inline void legacy_timeline(GObject *o, const char *event) {
 if (!GIMP_IS_ITEM(o)) return;
 g_print("TL time_us=%" G_GINT64_FORMAT " event=%s ptr=%p id=%d ordinal=%d attached=%d name=%s\n",g_get_monotonic_time(),event,o,gimp_item_get_ID(GIMP_ITEM(o)),GPOINTER_TO_INT(g_object_get_data(o,"timeline-ordinal")),gimp_item_is_attached(GIMP_ITEM(o)),gimp_object_get_name(o));
}
#endif
