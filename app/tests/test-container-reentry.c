/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gegl.h>
#include <gio/gio.h>
#include "core/core-types.h"
#include "core/gimpobject.h"
#include "core/gimplist.h"

typedef struct { guint count, events; } Mirror;
static void changed (GListModel *m, guint pos, guint removed, guint added, Mirror *s)
{
  guint i;
  g_assert_cmpuint (pos, <=, s->count);
  g_assert_cmpuint (removed, <=, s->count - pos);
  s->count = s->count - removed + added;
  s->events++;
  g_assert_cmpuint (s->count, ==, g_list_model_get_n_items (m));
  for (i = 0; i < s->count; i++) { GObject *o = g_list_model_get_item (m, i); g_assert_nonnull (o); g_object_unref (o); }
}
typedef struct { GimpObject *other; gboolean fired; guint mode; } Action;
static void act (GimpContainer *c, GimpObject *o, Action *a)
{
  if (a->fired) return;
  a->fired = TRUE;
  switch (a->mode) {
  case 0: g_assert_true (gimp_container_remove (c, o)); break;
  case 1: g_assert_true (gimp_container_add (c, o)); break;
  case 2: g_assert_true (gimp_container_add (c, a->other)); break;
  case 3: g_assert_true (gimp_container_remove (c, a->other)); break;
  case 4: gimp_container_freeze (c); g_assert_true (gimp_container_remove (c, o)); gimp_container_thaw (c); break;
  }
}
static void scenario (gconstpointer value)
{
  guint n = GPOINTER_TO_UINT (value);
  gboolean weak = n == 4;
  GimpContainer *c = weak ? gimp_list_new_weak (GIMP_TYPE_OBJECT, FALSE) : gimp_list_new (GIMP_TYPE_OBJECT, FALSE);
  GimpObject *o = g_object_new (GIMP_TYPE_OBJECT, NULL), *other = g_object_new (GIMP_TYPE_OBJECT, NULL);
  gpointer alive = o;
  Mirror mirror = {0};
  Action a = { other, FALSE, n == 4 ? 1 : n == 5 || n == 6 ? 0 : n == 7 ? 4 : n };
  g_object_add_weak_pointer (G_OBJECT (o), &alive);
  g_signal_connect (c, "items-changed", G_CALLBACK (changed), &mirror);
  if (a.mode == 1 || a.mode == 3) gimp_container_add (c, o);
  if (a.mode == 3) gimp_container_add (c, other);
  g_signal_connect (c, a.mode == 1 || a.mode == 3 ? "remove" : "add", G_CALLBACK (act), &a);
  if (n == 6) gimp_container_freeze (c);
  if (n == 5) g_assert_true (gimp_container_insert (c, o, 0));
  else if (a.mode == 1 || a.mode == 3) g_assert_true (gimp_container_remove (c, o));
  else g_assert_true (gimp_container_add (c, o));
  if (n == 6) gimp_container_thaw (c);
  g_assert_true (a.fired);
  g_assert_cmpuint (gimp_container_get_n_children (c), ==, a.mode == 1 ? 1 : a.mode == 2 ? 2 : 0);
  g_assert_cmpuint (mirror.count, ==, gimp_container_get_n_children (c));
  g_signal_handlers_disconnect_by_func (c, act, &a);
  g_object_unref (o);
  if (weak) { g_assert_null (alive); g_assert_cmpuint (gimp_container_get_n_children (c), ==, 0); g_assert_cmpuint (mirror.count, ==, 0); }
  g_object_unref (other);
  g_object_unref (c);
  g_assert_null (alive);
}
/* A deliberately broken subclass keeps the original warning/fallback contract. */
typedef struct { GimpList parent; gboolean nested; GimpObject *other; } BrokenList;
typedef struct { GimpListClass parent; } BrokenListClass;
GType broken_list_get_type (void);
G_DEFINE_TYPE (BrokenList, broken_list, GIMP_TYPE_LIST)
static void broken_add (GimpContainer *c, GimpObject *o)
{
  BrokenList *b = (BrokenList *) c;
  if (o == b->other) GIMP_CONTAINER_CLASS (broken_list_parent_class)->add (c, o);
  else { g_queue_push_tail (GIMP_LIST (c)->queue, o); if (b->nested) gimp_container_add (c, b->other); }
}
static void broken_remove (GimpContainer *c, GimpObject *o) { g_queue_remove (GIMP_LIST (c)->queue, o); }
static void broken_list_class_init (BrokenListClass *k) { GIMP_CONTAINER_CLASS (k)->add = broken_add; GIMP_CONTAINER_CLASS (k)->remove = broken_remove; }
static void broken_list_init (BrokenList *b) { (void)b; }
static void missing_chain (gconstpointer value)
{
  BrokenList *b = g_object_new (broken_list_get_type (), "child-type", GIMP_TYPE_OBJECT, "policy", GIMP_CONTAINER_POLICY_STRONG, NULL);
  GimpContainer *c = GIMP_CONTAINER (b);
  GimpObject *o = g_object_new (GIMP_TYPE_OBJECT, NULL);
  b->other = g_object_new (GIMP_TYPE_OBJECT, NULL); b->nested = GPOINTER_TO_INT (value);
  gimp_container_freeze (c);
  g_test_expect_message ("Gimp-Core", G_LOG_LEVEL_WARNING, "*add()*did not*chain up*");
  gimp_container_add (c, o); g_test_assert_expected_messages ();
  g_assert_cmpint (gimp_container_get_n_children (c), ==, b->nested ? 2 : 1);
  g_test_expect_message ("Gimp-Core", G_LOG_LEVEL_WARNING, "*remove()*did not*chain up*");
  gimp_container_remove (c, o); g_test_assert_expected_messages ();
  if (b->nested) { g_test_expect_message ("Gimp-Core", G_LOG_LEVEL_WARNING, "*remove()*did not*chain up*"); gimp_container_remove (c, b->other); g_test_assert_expected_messages (); }
  g_assert_cmpint (gimp_container_get_n_children (c), ==, 0);
  gimp_container_thaw (c); g_object_unref (o); g_object_unref (b->other); g_object_unref (c);
}
static void reorder_remove (GimpContainer *c, GimpObject *o, gint old, gint now, gpointer data)
{ (void)old; (void)now; (void)data; g_assert_true (gimp_container_remove (c, o)); }
static void freeze_only (GimpContainer *c, GimpObject *o, gpointer data)
{ (void)o; (void)data; gimp_container_freeze (c); }
static void ordering (gconstpointer value)
{
  guint mode = GPOINTER_TO_UINT (value);
  GimpContainer *c = gimp_list_new (GIMP_TYPE_OBJECT, FALSE);
  GimpObject *a = g_object_new (GIMP_TYPE_OBJECT, NULL), *b = g_object_new (GIMP_TYPE_OBJECT, NULL);
  Mirror mirror = {0};
  g_signal_connect (c, "items-changed", G_CALLBACK (changed), &mirror);
  if (mode == 0) {
    gimp_container_add (c, a); gimp_container_add (c, b);
    g_signal_connect (c, "reorder", G_CALLBACK (reorder_remove), NULL);
    g_assert_true (gimp_container_reorder (c, a, 0));
    g_assert_cmpint (gimp_container_get_n_children (c), ==, 1);
  } else if (mode == 1) {
    g_signal_connect (c, "add", G_CALLBACK (freeze_only), NULL);
    gimp_container_add (c, a); g_assert_cmpuint (mirror.count, ==, 0);
    gimp_container_thaw (c); g_assert_cmpuint (mirror.count, ==, 1);
  } else {
    gimp_container_add (c, a); gimp_container_insert (c, b, 0);
    g_assert_true (gimp_container_get_child_by_index (c, 0) == b);
    g_assert_cmpuint (mirror.count, ==, 2);
  }
  g_signal_handlers_disconnect_by_func (c, reorder_remove, NULL);
  g_signal_handlers_disconnect_by_func (c, freeze_only, NULL);
  g_object_unref (a); g_object_unref (b); g_object_unref (c);
}
int main (int argc, char **argv)
{
  guint i; const gchar *names[] = {"add-remove", "remove-readd", "add-other", "remove-other", "weak-readd", "insert-remove", "frozen-add-remove", "callback-freeze-thaw"};
  g_test_init (&argc, &argv, NULL); gegl_init (&argc, &argv);
  for (i=0; i<G_N_ELEMENTS(names); i++) { gchar *path = g_strconcat ("/container/reentry/", names[i], NULL); g_test_add_data_func (path, GUINT_TO_POINTER(i), scenario); g_free(path); }
  g_test_add_data_func ("/container/chain/missing", GINT_TO_POINTER(0), missing_chain);
  g_test_add_data_func ("/container/chain/nested-does-not-hide-missing", GINT_TO_POINTER(1), missing_chain);
  g_test_add_data_func ("/container/reentry/reorder-remove", GUINT_TO_POINTER(0), ordering);
  g_test_add_data_func ("/container/reentry/callback-freeze-later-thaw", GUINT_TO_POINTER(1), ordering);
  g_test_add_data_func ("/container/reentry/ordinary-insert", GUINT_TO_POINTER(2), ordering);
  i = g_test_run (); gegl_exit (); return i;
}
