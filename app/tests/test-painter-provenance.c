/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gegl.h>
#include <string.h>
#include "core/core-types.h"
#include "core/gimpobject.h"
#include "core/gimp-painter-provenance.h"
#include "core/gimp-painter-provenance-private.h"

static GObject *owner (void) { return g_object_new (GIMP_TYPE_OBJECT, NULL); }
static GBytes *bytes (const gchar *s) { return g_bytes_new (s, strlen (s)); }
static void assert_bytes (GObject *o, GimpPainterProvenanceBytes field, const gchar *s)
{
  GBytes *actual = gimp_painter_provenance_ref_bytes (o, field), *expected = bytes (s);
  g_assert_nonnull (actual); g_assert_true (g_bytes_equal (actual, expected));
  g_bytes_unref (actual); g_bytes_unref (expected);
}
static void empty_reads_and_type (void)
{
  GObject *o = owner (), *foreign = g_object_new (G_TYPE_OBJECT, NULL);
  guint64 offset = 42;
  g_assert_null (gimp_painter_provenance_ref_bytes (o, GIMP_PAINTER_PROVENANCE_ORIGINAL));
  g_assert_null (gimp_painter_provenance_dup_text (o, GIMP_PAINTER_PROVENANCE_NAME));
  g_assert_null (gimp_painter_provenance_ref_records (o));
  g_assert_null (gimp_painter_provenance_ref_transport (o));
  g_assert_false (gimp_painter_provenance_has_definition (o));
  g_assert_false (gimp_painter_provenance_get_offset (o, &offset));
  g_assert_cmpuint (offset, ==, 42);
  g_assert_null (_gimp_object_ref_painter_provenance (o));
  g_assert_false (_gimp_object_attach_painter_provenance (o, foreign));
  g_assert_false (gimp_painter_provenance_set_dialect (foreign, 1));
  g_assert_false (gimp_painter_provenance_set_bytes (o, (GimpPainterProvenanceBytes) -1, NULL));
  g_assert_null (_gimp_object_ref_painter_provenance (o));
  g_object_unref (foreign); g_object_unref (o);
}
static void values_and_record_snapshots (void)
{
  GObject *o = owner ();
  GBytes *data = bytes ("opaque\377bytes");
  GPtrArray *records = g_ptr_array_new_with_free_func ((GDestroyNotify) g_bytes_unref), *copy;
  gchar *name;
  for (guint i = 0; i < GIMP_PAINTER_PROVENANCE_N_BYTES; ++i)
    g_assert_true (gimp_painter_provenance_set_bytes (o, i, data));
  g_assert_true (gimp_painter_provenance_set_text (o, GIMP_PAINTER_PROVENANCE_NAME, "raw\377name"));
  name = gimp_painter_provenance_dup_text (o, GIMP_PAINTER_PROVENANCE_NAME);
  g_assert_cmpmem (name, 9, "raw\377name", 9); g_free (name);
  g_ptr_array_add (records, g_bytes_ref (data));
  g_assert_true (gimp_painter_provenance_set_records (o, records));
  g_ptr_array_set_size (records, 0); /* caller cannot edit the stored sequence */
  copy = gimp_painter_provenance_ref_records (o); g_assert_cmpuint (copy->len, ==, 1);
  g_ptr_array_set_size (copy, 0); g_ptr_array_unref (copy);
  copy = gimp_painter_provenance_ref_records (o); g_assert_cmpuint (copy->len, ==, 1);
  g_assert_true (g_bytes_equal (g_ptr_array_index (copy, 0), data));
  for (guint i = 0; i < GIMP_PAINTER_PROVENANCE_N_BYTES; ++i) assert_bytes (o, i, "opaque\377bytes");
  g_bytes_unref (data); g_ptr_array_unref (records); g_ptr_array_unref (copy); g_object_unref (o);
}
static void copy_only_provenance (void)
{
  GObject *source = owner (), *target = owner ();
  GBytes *data = bytes ("source"), *old = bytes ("target");
  GVariant *definition = g_variant_ref_sink (g_variant_new_string ("current target definition"));
  GPtrArray *records = g_ptr_array_new_with_free_func ((GDestroyNotify) g_bytes_unref), *copy;
  guint64 offset;
  for (guint i = 0; i < GIMP_PAINTER_PROVENANCE_N_BYTES; ++i)
    { gimp_painter_provenance_set_bytes (source, i, data); gimp_painter_provenance_set_bytes (target, i, old); }
  gimp_painter_provenance_set_text (source, GIMP_PAINTER_PROVENANCE_NAME, "source-name");
  gimp_painter_provenance_set_text (source, GIMP_PAINTER_PROVENANCE_TYPE, "layer:GimpCloneLayer");
  g_ptr_array_add (records, g_bytes_ref (data)); gimp_painter_provenance_set_records (source, records);
  gimp_painter_provenance_set_offset (source, G_MAXUINT64); gimp_painter_provenance_set_offset (target, 13);
  gimp_painter_provenance_set_save_id (source, 44); gimp_painter_provenance_set_save_id (target, 12);
  gimp_painter_provenance_set_dialect (source, 3); gimp_painter_provenance_set_dialect (target, 2);
  gimp_painter_provenance_set_definition (target, definition);
  gimp_painter_copy_provenance (source, target); gimp_painter_copy_provenance (target, target);
  g_object_run_dispose (source); g_object_unref (source);
  for (guint i = 0; i < GIMP_PAINTER_PROVENANCE_ORIGINAL; ++i) assert_bytes (target, i, "source");
  assert_bytes (target, GIMP_PAINTER_PROVENANCE_ORIGINAL, "target");
  g_assert_true (gimp_painter_provenance_get_offset (target, &offset)); g_assert_cmpuint (offset, ==, 13);
  g_assert_cmpuint (gimp_painter_provenance_get_save_id (target), ==, 12);
  g_assert_cmpint (gimp_painter_provenance_get_dialect (target), ==, 2);
  { GVariant *actual = gimp_painter_provenance_ref_definition (target); g_assert_true (g_variant_equal (actual, definition)); g_variant_unref (actual); }
  { gchar *name = gimp_painter_provenance_dup_text (target, GIMP_PAINTER_PROVENANCE_NAME); g_assert_cmpstr (name, ==, "source-name"); g_free (name); }
  copy = gimp_painter_provenance_ref_records (target); g_assert_cmpuint (copy->len, ==, 1); g_ptr_array_unref (copy);
  g_bytes_unref (data); g_bytes_unref (old); g_variant_unref (definition); g_ptr_array_unref (records); g_object_unref (target);
}
static void closed_read_and_write (void)
{
  GObject *o = owner (), *empty = owner (); GBytes *data = bytes ("kept");
  gimp_painter_provenance_set_bytes (o, GIMP_PAINTER_PROVENANCE_HEADER, data);
  g_object_run_dispose (o); g_object_run_dispose (o);
  assert_bytes (o, GIMP_PAINTER_PROVENANCE_HEADER, "kept");
  g_assert_false (gimp_painter_provenance_set_bytes (o, GIMP_PAINTER_PROVENANCE_HEADER, NULL));
  assert_bytes (o, GIMP_PAINTER_PROVENANCE_HEADER, "kept");
  g_object_run_dispose (empty); g_assert_false (gimp_painter_provenance_set_bytes (empty, GIMP_PAINTER_PROVENANCE_HEADER, data));
  g_assert_null (_gimp_object_ref_painter_provenance (empty));
  g_bytes_unref (data); g_object_unref (o); g_object_unref (empty);
}
typedef struct { GObject *owner; GBytes *replacement; guint calls; gboolean release_owner; } Reentry;
static void release_bytes (gpointer p)
{
  Reentry *r = p; ++r->calls;
  g_assert_true (gimp_painter_provenance_set_bytes (r->owner, GIMP_PAINTER_PROVENANCE_HEADER, r->replacement));
  if (r->release_owner) { GObject *o = r->owner; r->owner = NULL; g_object_unref (o); }
}
static void weak_destroy (gpointer p, GObject *o) { (void) o; *(gboolean *) p = TRUE; }
static void replacement_reentry (void)
{
  GObject *o = owner (); GBytes *next = bytes ("outer"), *replacement = bytes ("reentrant");
  Reentry r = {o, replacement, 0, FALSE};
  GBytes *old = g_bytes_new_with_free_func ("old", 3, release_bytes, &r);
  gimp_painter_provenance_set_bytes (o, GIMP_PAINTER_PROVENANCE_HEADER, old); g_bytes_unref (old);
  g_assert_true (gimp_painter_provenance_set_bytes (o, GIMP_PAINTER_PROVENANCE_HEADER, next));
  g_assert_cmpuint (r.calls, ==, 1); assert_bytes (o, GIMP_PAINTER_PROVENANCE_HEADER, "reentrant");
  g_bytes_unref (next); g_bytes_unref (replacement); g_object_unref (o);
}
static void replacement_drops_last_owner (void)
{
  GObject *o = owner (); GBytes *next = bytes ("outer"), *replacement = bytes ("reentrant");
  gboolean destroyed = FALSE; Reentry r = {o, replacement, 0, TRUE};
  GBytes *old = g_bytes_new_with_free_func ("old", 3, release_bytes, &r);
  g_object_weak_ref (o, weak_destroy, &destroyed);
  gimp_painter_provenance_set_bytes (o, GIMP_PAINTER_PROVENANCE_HEADER, old); g_bytes_unref (old);
  g_assert_true (gimp_painter_provenance_set_bytes (o, GIMP_PAINTER_PROVENANCE_HEADER, next));
  g_assert_true (destroyed); g_assert_null (r.owner); g_assert_cmpuint (r.calls, ==, 1);
  g_bytes_unref (next); g_bytes_unref (replacement);
}
typedef struct { GObject *owner; guint calls; } CopyClose;
static void copy_release_close (gpointer data)
{
  CopyClose *state = data; ++state->calls;
  /* All copied fields are published before any old byte finalizer fires. */
  assert_bytes (state->owner, GIMP_PAINTER_PROVENANCE_PROPERTIES, "new");
  assert_bytes (state->owner, GIMP_PAINTER_PROVENANCE_HEADER, "new");
  { gchar *name = gimp_painter_provenance_dup_text (state->owner, GIMP_PAINTER_PROVENANCE_NAME);
    g_assert_cmpstr (name, ==, "new-name"); g_free (name); }
  g_object_run_dispose (state->owner);
  g_assert_false (gimp_painter_provenance_set_save_id (state->owner, 99));
}
static void copy_release_closes_target (void)
{
  GObject *source = owner (), *target = owner ();
  GBytes *next = bytes ("new"); CopyClose state = {target, 0};
  GBytes *old = g_bytes_new_with_free_func ("old", 3, copy_release_close, &state);
  gimp_painter_provenance_set_bytes (source, GIMP_PAINTER_PROVENANCE_PROPERTIES, next);
  gimp_painter_provenance_set_bytes (source, GIMP_PAINTER_PROVENANCE_HEADER, next);
  gimp_painter_provenance_set_text (source, GIMP_PAINTER_PROVENANCE_NAME, "new-name");
  gimp_painter_provenance_set_bytes (target, GIMP_PAINTER_PROVENANCE_PROPERTIES, old);
  g_bytes_unref (old);
  gimp_painter_copy_provenance (source, target);
  g_assert_cmpuint (state.calls, ==, 1);
  assert_bytes (target, GIMP_PAINTER_PROVENANCE_HEADER, "new");
  g_object_unref (source); g_object_unref (target); g_bytes_unref (next);
}
static void finalized_bytes_reentry (gpointer data)
{
  CopyClose *state = data; ++state->calls;
  g_assert_null (gimp_painter_provenance_ref_bytes (state->owner, GIMP_PAINTER_PROVENANCE_HEADER));
  g_assert_false (gimp_painter_provenance_set_save_id (state->owner, 3));
  gimp_painter_copy_provenance (state->owner, state->owner);
}
static void finalizing_owner_reentry (void)
{
  GObject *o = owner (); CopyClose state = {o, 0};
  GBytes *data = g_bytes_new_with_free_func ("old", 3, finalized_bytes_reentry, &state);
  g_assert_true (gimp_painter_provenance_set_bytes (o, GIMP_PAINTER_PROVENANCE_HEADER, data));
  g_bytes_unref (data); g_object_unref (o);
  g_assert_cmpuint (state.calls, ==, 1);
}
static void assert_transport (GObject *o, const gchar *expected)
{
  GimpPainterProvenanceTransport *actual = gimp_painter_provenance_ref_transport (o);
  GBytes *data = bytes (expected);
  g_assert_nonnull (actual);
  g_assert_cmpuint (actual->records->len, ==, 2);
  g_assert_true (g_bytes_equal (g_ptr_array_index (actual->records, 0), data));
  g_assert_true (g_bytes_equal (g_ptr_array_index (actual->records, 1), data));
  g_assert_true (g_bytes_equal (actual->capsules[GIMP_PAINTER_PROVENANCE_NAMESPACE_ITEM], data));
  g_assert_null (actual->capsules[GIMP_PAINTER_PROVENANCE_NAMESPACE_IMAGE]);
  g_assert_true (actual->present[GIMP_PAINTER_PROVENANCE_NAMESPACE_ITEM]);
  g_assert_false (actual->invalid[GIMP_PAINTER_PROVENANCE_NAMESPACE_ITEM]);
  g_assert_false (actual->present[GIMP_PAINTER_PROVENANCE_NAMESPACE_ORIGIN]);
  g_assert_true (actual->invalid[GIMP_PAINTER_PROVENANCE_NAMESPACE_ORIGIN]);
  {
    const guint8 dispositions[] = {GIMP_PAINTER_PROVENANCE_TRANSPORT_ITEM,
                                   GIMP_PAINTER_PROVENANCE_TRANSPORT_INERT};
    gsize length;
    const void *raw = g_bytes_get_data (actual->dispositions, &length);
    g_assert_cmpmem (raw, length, dispositions, sizeof dispositions);
  }
  g_bytes_unref (data);
  gimp_painter_provenance_free_transport (actual);
}
static void set_test_transport (GObject *o, GBytes *data)
{
  const guint8 dispositions[] = {GIMP_PAINTER_PROVENANCE_TRANSPORT_ITEM,
                                 GIMP_PAINTER_PROVENANCE_TRANSPORT_INERT};
  GimpPainterProvenanceTransport state = {0};
  state.records = g_ptr_array_new_with_free_func ((GDestroyNotify) g_bytes_unref);
  g_ptr_array_add (state.records, g_bytes_ref (data));
  g_ptr_array_add (state.records, g_bytes_ref (data));
  state.dispositions = g_bytes_new (dispositions, sizeof dispositions);
  state.capsules[GIMP_PAINTER_PROVENANCE_NAMESPACE_ITEM] = data;
  state.present[GIMP_PAINTER_PROVENANCE_NAMESPACE_ITEM] = TRUE;
  state.invalid[GIMP_PAINTER_PROVENANCE_NAMESPACE_ORIGIN] = TRUE;
  g_assert_true (gimp_painter_provenance_set_transport (o, &state));
  /* Neither the input nor a returned container may mutate the stored order. */
  g_ptr_array_set_size (state.records, 0);
  g_ptr_array_unref (state.records);
  g_bytes_unref (state.dispositions);
}
static void transport_snapshots_and_validation (void)
{
  GObject *o = owner (), *empty = owner ();
  GBytes *data = bytes ("transport");
  GimpPainterProvenanceTransport *copy;
  GimpPainterProvenanceTransport invalid = {0};
  const guint8 unknown = 4, image = GIMP_PAINTER_PROVENANCE_TRANSPORT_IMAGE;
  set_test_transport (o, data);
  assert_transport (o, "transport");
  copy = gimp_painter_provenance_ref_transport (o);
  g_assert_true (g_ptr_array_index (copy->records, 0) == data);
  g_assert_true (copy->capsules[GIMP_PAINTER_PROVENANCE_NAMESPACE_ITEM] == data);
  g_ptr_array_set_size (copy->records, 0);
  copy->invalid[GIMP_PAINTER_PROVENANCE_NAMESPACE_ITEM] = TRUE;
  gimp_painter_provenance_free_transport (copy);
  assert_transport (o, "transport");

  invalid.records = g_ptr_array_new_with_free_func ((GDestroyNotify) g_bytes_unref);
  g_ptr_array_add (invalid.records, g_bytes_ref (data));
  g_assert_false (gimp_painter_provenance_set_transport (empty, &invalid));
  invalid.dispositions = g_bytes_new (&unknown, 1);
  g_assert_false (gimp_painter_provenance_set_transport (empty, &invalid));
  g_bytes_unref (invalid.dispositions);
  invalid.dispositions = g_bytes_new (&image, 1);
  g_assert_false (gimp_painter_provenance_set_transport (empty, &invalid));
  invalid.capsules[GIMP_PAINTER_PROVENANCE_NAMESPACE_IMAGE] = data;
  g_assert_false (gimp_painter_provenance_set_transport (empty, &invalid));
  invalid.present[GIMP_PAINTER_PROVENANCE_NAMESPACE_IMAGE] = TRUE;
  invalid.invalid[GIMP_PAINTER_PROVENANCE_NAMESPACE_IMAGE] = TRUE;
  g_assert_false (gimp_painter_provenance_set_transport (empty, &invalid));
  g_assert_null (_gimp_object_ref_painter_provenance (empty));
  g_assert_false (gimp_painter_provenance_set_transport (o, &invalid));
  assert_transport (o, "transport");
  invalid.invalid[GIMP_PAINTER_PROVENANCE_NAMESPACE_IMAGE] = FALSE;
  g_ptr_array_set_size (invalid.records, 0);
  g_ptr_array_add (invalid.records, NULL);
  g_assert_false (gimp_painter_provenance_set_transport (empty, &invalid));
  g_assert_null (_gimp_object_ref_painter_provenance (empty));
  g_ptr_array_set_free_func (invalid.records, NULL);
  g_ptr_array_unref (invalid.records);
  g_bytes_unref (invalid.dispositions);
  g_assert_true (gimp_painter_provenance_set_transport (o, NULL));
  g_assert_null (gimp_painter_provenance_ref_transport (o));
  gimp_painter_provenance_free_transport (NULL);
  g_bytes_unref (data); g_object_unref (empty); g_object_unref (o);
}
static void transport_copy_and_close (void)
{
  GObject *source = owner (), *target = owner (), *empty = owner ();
  GBytes *data = bytes ("transport");
  set_test_transport (source, data);
  gimp_painter_provenance_set_save_id (source, 77);
  gimp_painter_provenance_set_save_id (target, 22);
  g_object_run_dispose (source);
  assert_transport (source, "transport");
  g_assert_false (gimp_painter_provenance_set_transport (source, NULL));
  gimp_painter_copy_provenance (source, target);
  gimp_painter_copy_provenance (empty, target);
  g_object_unref (source);
  assert_transport (target, "transport");
  g_assert_cmpuint (gimp_painter_provenance_get_save_id (target), ==, 22);
  g_object_run_dispose (target);
  assert_transport (target, "transport");
  g_assert_false (gimp_painter_provenance_set_transport (target, NULL));
  g_object_run_dispose (empty);
  g_assert_false (gimp_painter_provenance_set_transport (empty, NULL));
  g_assert_null (_gimp_object_ref_painter_provenance (empty));
  g_bytes_unref (data); g_object_unref (empty); g_object_unref (target);
}
typedef struct {
  GObject *owner;
  GBytes *replacement;
  guint calls;
  gboolean close_owner, release_owner;
} TransportReentry;
static void transport_release (gpointer data)
{
  TransportReentry *state = data;
  ++state->calls;
  /* Capsule, raw archive, dispositions and flags publish as one value. */
  assert_transport (state->owner, "outer");
  if (state->close_owner)
    {
      g_object_run_dispose (state->owner);
      g_assert_false (gimp_painter_provenance_set_transport (state->owner, NULL));
    }
  else
    set_test_transport (state->owner, state->replacement);
  if (state->release_owner)
    {
      GObject *o = state->owner;
      state->owner = NULL;
      g_object_unref (o);
    }
}
static void transport_replacement_reentry (void)
{
  for (guint field = 0; field < 3; ++field)
    {
      GObject *o = owner ();
      GBytes *outer = bytes ("outer"), *replacement = bytes ("reentrant");
      gboolean destroyed = FALSE;
      TransportReentry state = {o, replacement, 0, FALSE, field == 2};
      GBytes *old = g_bytes_new_with_free_func ("\0", 1, transport_release, &state);
      GimpPainterProvenanceTransport previous = {0};
      previous.records = g_ptr_array_new_with_free_func ((GDestroyNotify) g_bytes_unref);
      if (field == 0)
        {
          g_ptr_array_add (previous.records, g_bytes_ref (old));
          previous.dispositions = g_bytes_new_static ("\0", 1);
        }
      else if (field == 1)
        {
          g_ptr_array_add (previous.records, bytes ("record"));
          previous.dispositions = g_bytes_ref (old);
        }
      else
        {
          previous.capsules[GIMP_PAINTER_PROVENANCE_NAMESPACE_ITEM] = old;
          previous.present[GIMP_PAINTER_PROVENANCE_NAMESPACE_ITEM] = TRUE;
        }
      g_object_weak_ref (o, weak_destroy, &destroyed);
      g_assert_true (gimp_painter_provenance_set_transport (o, &previous));
      g_ptr_array_unref (previous.records);
      if (previous.dispositions) g_bytes_unref (previous.dispositions);
      g_bytes_unref (old);
      set_test_transport (o, outer);
      g_assert_cmpuint (state.calls, ==, 1);
      if (field == 2)
        { g_assert_true (destroyed); g_assert_null (state.owner); }
      else
        { assert_transport (o, "reentrant"); g_object_unref (o); }
      g_bytes_unref (outer); g_bytes_unref (replacement);
    }
}
static void transport_copy_reentry_closes_target (void)
{
  GObject *source = owner (), *target = owner ();
  GBytes *outer = bytes ("outer");
  TransportReentry state = {target, NULL, 0, TRUE, FALSE};
  GBytes *old = g_bytes_new_with_free_func ("old", 3, transport_release, &state);
  set_test_transport (source, outer);
  set_test_transport (target, old);
  g_bytes_unref (old);
  gimp_painter_copy_provenance (source, target);
  g_assert_cmpuint (state.calls, ==, 1);
  assert_transport (target, "outer");
  g_assert_false (gimp_painter_provenance_set_save_id (target, 42));
  g_bytes_unref (outer); g_object_unref (source); g_object_unref (target);
}
int main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/painter-provenance/empty_reads_and_type", empty_reads_and_type);
  g_test_add_func ("/painter-provenance/values_and_record_snapshots", values_and_record_snapshots);
  g_test_add_func ("/painter-provenance/copy_only_provenance", copy_only_provenance);
  g_test_add_func ("/painter-provenance/closed_read_and_write", closed_read_and_write);
  g_test_add_func ("/painter-provenance/replacement_reentry", replacement_reentry);
  g_test_add_func ("/painter-provenance/replacement_drops_last_owner", replacement_drops_last_owner);
  g_test_add_func ("/painter-provenance/copy_release_closes_target", copy_release_closes_target);
  g_test_add_func ("/painter-provenance/finalizing_owner_reentry", finalizing_owner_reentry);
  g_test_add_func ("/painter-provenance/transport_snapshots_and_validation", transport_snapshots_and_validation);
  g_test_add_func ("/painter-provenance/transport_copy_and_close", transport_copy_and_close);
  g_test_add_func ("/painter-provenance/transport_replacement_reentry", transport_replacement_reentry);
  g_test_add_func ("/painter-provenance/transport_copy_reentry_closes_target", transport_copy_reentry_closes_target);
  return g_test_run ();
}
