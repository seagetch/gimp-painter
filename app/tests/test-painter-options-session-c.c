/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Runtime consumers compiled as C, not C++ with extern "C" declarations. */
#include "config.h"
#include <gtk/gtk.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include "libgimpconfig/gimpconfig.h"
#include "core/core-types.h"
#include "widgets/widgets-types.h"
#include "core/gimp.h"
#include "core/gimpcontext.h"
#include "core/gimppaintermybrush.h"
#include "core/gimpimage.h"
#include "core/gimpimage-undo.h"
#include "core/gimpundostack.h"
#include "core/gimplayer.h"
#include "core/gimplayer-new.h"
#include "paint/painter-mypaint-surface/gimp-painter-options.h"
#include "paint/gimpmypaintoptions.h"
#include "paint/painter-mypaint-surface/gimp-painter-session.h"
#include "painter/gimp-painter-binding.h"
#include "mypaintbrush-enum-settings.h"
#include "tests.h"
#include "gimp-app-test-utils.h"

static Gimp *gimp;

static GimpPainterMybrushOptions *
options_new (void)
{
  GimpPainterMybrushOptions *options = g_object_new (
    GIMP_TYPE_PAINTER_MYBRUSH_OPTIONS, "gimp", gimp, NULL);
  g_assert_false (options->binding_failed);
  return options;
}

static void
count_settings_changed (GimpPainterMybrushOptions *options,
                        gpointer                   data)
{
  guint *count = data;
  (void) options;
  ++*count;
}

static void
assert_draft_name (GimpPainterMybrushOptions *options,
                   const gchar               *expected)
{
  GError *error = NULL;
  gchar *name = gimp_painter_mybrush_options_dup_draft_name (options, &error);
  gchar *property = NULL;
  g_assert_no_error (error);
  g_assert_cmpstr (name, ==, expected);
  g_object_get (options, "painter-draft-name", &property, NULL);
  g_assert_cmpstr (property, ==, expected);
  g_free (property);
  g_free (name);
}

static void
options_draft_name_history_copy (void)
{
  GimpPainterMybrushOptions *options = options_new ();
  GimpPainterMybrushOptions *peer = options_new ();
  GimpPainterMybrushOptions *duplicate;
  GimpPainterMybrush *source = GIMP_PAINTER_MYBRUSH (
    gimp_painter_mybrush_new (NULL, "Actual C source"));
  GimpPainterMybrush *next = GIMP_PAINTER_MYBRUSH (
    gimp_painter_mybrush_new (NULL, "Actual C next"));
  const gchar *pending_name = "Actual C unsaved \316\261 variant";
  const gchar *edited_json =
    "{\"version\":3,\"parent_brush_name\":\"independent parent\","
    "\"settings\":{\"opaque\":{\"base_value\":0.37,"
    "\"inputs\":{\"pressure\":[[0,0.2],[1,0.8]]}}},"
    "\"unknown\":{\"retain\":23}}";
  const gchar invalid_utf8[] = { (gchar) 0xff, 0 };
  GError *error = NULL;
  gchar *source_json, *draft_json, *actual, *history_name, *owned_name, *config;
  gboolean dirty = TRUE, name_dirty = FALSE;
  guint settings_changed = 0, history;
  GParamSpec *spec;

  gimp_context_set_painter_mybrush (GIMP_CONTEXT (options), source);
  source_json = gimp_painter_mybrush_dup_json (source, &error);
  g_assert_no_error (error);
  assert_draft_name (options, "Actual C source");
  spec = g_object_class_find_property (G_OBJECT_GET_CLASS (options), "painter-draft-name");
  g_assert_nonnull (spec);
  g_assert_true ((spec->flags & G_PARAM_READABLE) != 0);
  g_assert_false ((spec->flags & G_PARAM_WRITABLE) != 0);
  g_assert_false ((spec->flags & GIMP_CONFIG_PARAM_SERIALIZE) != 0);
  spec = g_object_class_find_property (G_OBJECT_GET_CLASS (options), "painter-name-dirty");
  g_assert_nonnull (spec);
  g_assert_false ((spec->flags & G_PARAM_WRITABLE) != 0);
  g_signal_connect (options, "settings-changed", G_CALLBACK (count_settings_changed),
                    &settings_changed);

  g_assert_true (gimp_painter_mybrush_options_set_draft_name (options, pending_name, &error));
  g_assert_no_error (error);
  g_assert_cmpuint (settings_changed, ==, 0);
  g_object_get (options, "painter-dirty", &dirty, "painter-name-dirty", &name_dirty, NULL);
  g_assert_false (dirty);
  g_assert_true (name_dirty);
  g_assert_true (gimp_context_get_painter_mybrush (GIMP_CONTEXT (options)) == source);
  g_assert_cmpstr (gimp_object_get_name (source), ==, "Actual C source");
  actual = gimp_painter_mybrush_options_dup_json (options, &error);
  g_assert_no_error (error);
  g_assert_cmpstr (actual, ==, source_json);
  g_free (actual);

  /* Name-only history must not turn into a settings edit on restoration. */
  history = gimp_painter_mybrush_options_history_size (options);
  gimp_context_set_painter_mybrush (GIMP_CONTEXT (options), next);
  g_assert_cmpuint (gimp_painter_mybrush_options_history_size (peer), ==, history + 1);
  g_assert_true (gimp_painter_mybrush_options_restore_history (peer, history, &error));
  g_assert_no_error (error);
  assert_draft_name (peer, pending_name);
  g_object_get (peer, "painter-dirty", &dirty, "painter-name-dirty", &name_dirty, NULL);
  g_assert_false (dirty);
  g_assert_true (name_dirty);

  g_assert_true (gimp_painter_mybrush_options_restore_history (options, history, &error));
  g_assert_no_error (error);
  g_assert_true (gimp_painter_mybrush_options_set_json (options, edited_json, &error));
  g_assert_no_error (error);
  draft_json = gimp_painter_mybrush_options_dup_json (options, &error);
  g_assert_no_error (error);
  assert_draft_name (options, pending_name);
  history = gimp_painter_mybrush_options_history_size (options);
  gimp_context_set_painter_mybrush (GIMP_CONTEXT (options), next);
  assert_draft_name (options, "Actual C next");
  history_name = gimp_painter_mybrush_options_history_name (options, history);
  g_assert_cmpstr (history_name, ==, pending_name);
  g_free (history_name);
  g_assert_true (gimp_painter_mybrush_options_restore_history (peer, history, &error));
  g_assert_no_error (error);
  g_assert_true (gimp_context_get_painter_mybrush (GIMP_CONTEXT (peer)) == source);
  assert_draft_name (peer, pending_name);
  actual = gimp_painter_mybrush_options_dup_json (peer, &error);
  g_assert_no_error (error);
  g_assert_cmpstr (actual, ==, draft_json);
  g_free (actual);
  actual = gimp_painter_mybrush_dup_json (source, &error);
  g_assert_no_error (error);
  g_assert_cmpstr (actual, ==, source_json);
  g_assert_cmpstr (gimp_object_get_name (source), ==, "Actual C source");
  g_free (actual);

  duplicate = GIMP_PAINTER_MYBRUSH_OPTIONS (gimp_config_duplicate (GIMP_CONFIG (peer)));
  g_assert_nonnull (duplicate);
  assert_draft_name (duplicate, pending_name);
  g_assert_true (gimp_config_copy (GIMP_CONFIG (peer), GIMP_CONFIG (options), 0));
  assert_draft_name (options, pending_name);
  actual = gimp_painter_mybrush_options_dup_json (options, &error);
  g_assert_no_error (error);
  g_assert_cmpstr (actual, ==, draft_json);
  g_free (actual);
  config = gimp_config_serialize_to_string (GIMP_CONFIG (peer), NULL);
  g_assert_nonnull (config);
  g_assert_null (strstr (config, "painter-draft-name"));
  g_assert_null (strstr (config, "painter-name-dirty"));
  g_assert_null (strstr (config, pending_name));
  g_free (config);

  /* Reset is a settings operation: keep the independent unsaved name. */
  gimp_config_reset (GIMP_CONFIG (duplicate));
  assert_draft_name (duplicate, pending_name);
  g_object_get (duplicate, "painter-name-dirty", &name_dirty, NULL);
  g_assert_true (name_dirty);
  actual = gimp_painter_mybrush_options_dup_json (peer, &error);
  g_assert_no_error (error);
  g_assert_cmpstr (actual, ==, draft_json);
  g_free (actual);
  actual = gimp_painter_mybrush_dup_json (source, &error);
  g_assert_no_error (error);
  g_assert_cmpstr (actual, ==, source_json);
  g_assert_cmpstr (gimp_object_get_name (source), ==, "Actual C source");
  g_free (actual);

  /* Invalid API input leaves both independently owned draft values intact. */
  g_assert_false (gimp_painter_mybrush_options_set_draft_name (peer, NULL, &error));
  g_assert_nonnull (error);
  g_clear_error (&error);
  g_assert_false (gimp_painter_mybrush_options_set_draft_name (peer, invalid_utf8, &error));
  g_assert_nonnull (error);
  g_clear_error (&error);
  assert_draft_name (peer, pending_name);
  owned_name = gimp_painter_mybrush_options_dup_draft_name (peer, &error);
  g_assert_no_error (error);
  g_assert_true (gimp_painter_mybrush_options_set_draft_name (peer, "Later draft", &error));
  g_assert_no_error (error);
  assert_draft_name (peer, "Later draft");
  g_assert_cmpstr (owned_name, ==, pending_name);
  g_object_unref (duplicate);
  g_object_unref (peer);
  g_object_unref (options);
  g_object_unref (source);
  g_object_unref (next);
  /* Transfer-full C text remains valid after its options owners are gone. */
  g_assert_cmpstr (owned_name, ==, pending_name);
  g_free (owned_name);
  g_free (draft_json);
  g_free (source_json);
}

static void
assert_mapping_empty (GimpMypaintOptions *options,
                      const gchar        *name,
                      guint               input)
{
  guint count = 999;
  GimpVector2 sentinel = {-23., -51.};
  GimpVector2 *points = &sentinel;
  gchar *key = g_strdup (name);
  gimp_mypaint_options_get_mapping_point (options, key, input, &count, &points);
  g_assert_cmpuint (count, ==, 0);
  g_assert_null (points);
  g_assert_cmpfloat (sentinel.x, ==, -23.);
  g_assert_cmpfloat (sentinel.y, ==, -51.);
  g_assert_cmpstr (key, ==, name);
  g_free (key);
}

/* Read before chaining the native constructed hook: the existing options slot
 * is still constructing and must not be activated by a query. */
typedef struct { GimpPainterMybrushOptions parent_instance; gboolean queried; }
  MappingConstructProbe;
typedef struct { GimpPainterMybrushOptionsClass parent_class; }
  MappingConstructProbeClass;
GType mapping_construct_probe_get_type (void);
G_DEFINE_TYPE (MappingConstructProbe, mapping_construct_probe,
               GIMP_TYPE_PAINTER_MYBRUSH_OPTIONS)

static void
mapping_construct_probe_constructed (GObject *object)
{
  assert_mapping_empty ((GimpMypaintOptions *) object, "opaque_multiply", INPUT_PRESSURE);
  ((MappingConstructProbe *) object)->queried = TRUE;
  G_OBJECT_CLASS (mapping_construct_probe_parent_class)->constructed (object);
}

static void
mapping_construct_probe_class_init (MappingConstructProbeClass *klass)
{
  G_OBJECT_CLASS (klass)->constructed = mapping_construct_probe_constructed;
}

static void
mapping_construct_probe_init (MappingConstructProbe *self)
{
  self->queried = FALSE;
}

static gpointer
mapping_query_wrong_thread (gpointer data)
{
  assert_mapping_empty (data, "opaque", INPUT_PRESSURE);
  return NULL;
}

static void
options_mapping_point_owned_draft (void)
{
  GimpMypaintOptions *options = options_new ();
  GimpPainterMybrush *source = GIMP_PAINTER_MYBRUSH (
    gimp_painter_mybrush_new (NULL, "Actual C mapping source"));
  const GimpVector2 draft[] = {{.1000000000003, .2000000000009},
                              {.9000000000007, .7000000000011}};
  const GimpVector2 replacement[] = {{0., .3}, {1., .6}};
  GError *error = NULL;
  GimpVector2 *owned, *again;
  GArray *modern;
  gchar *before, *source_before, *actual;
  gchar key[] = "opaque-multiply";
  guint count = 999, again_count = 999, history, changed = 0, i;
  gpointer weak = options;
  gboolean dirty = FALSE, name_dirty = FALSE;

  gimp_context_set_painter_mybrush (GIMP_CONTEXT (options), source);
  source_before = gimp_painter_mybrush_dup_json (source, &error);
  g_assert_no_error (error);
  g_assert_true (gimp_painter_mybrush_options_set_curve (
    options, BRUSH_OPAQUE_MULTIPLY, INPUT_PRESSURE, draft, G_N_ELEMENTS (draft), &error));
  g_assert_true (gimp_painter_mybrush_options_set_draft_name (
    options, "Mapping read keeps pending name", &error));
  g_assert_no_error (error);
  before = gimp_painter_mybrush_options_dup_json (options, &error);
  g_assert_no_error (error);
  history = gimp_painter_mybrush_options_history_size (options);
  g_signal_connect (options, "settings-changed", G_CALLBACK (count_settings_changed), &changed);
  gimp_mypaint_options_get_mapping_point (options, key, INPUT_PRESSURE, &count, &owned);
  g_assert_cmpstr (key, ==, "opaque-multiply");
  g_assert_cmpuint (count, ==, G_N_ELEMENTS (draft));
  g_assert_nonnull (owned);
  modern = gimp_painter_mybrush_options_get_curve (
    options, BRUSH_OPAQUE_MULTIPLY, INPUT_PRESSURE, &error);
  g_assert_no_error (error);
  g_assert_nonnull (modern);
  for (i = 0; i < count; ++i)
    {
      g_assert_cmpfloat (owned[i].x, ==, (gdouble) (gfloat) draft[i].x);
      g_assert_cmpfloat (owned[i].y, ==, (gdouble) (gfloat) draft[i].y);
      g_assert_cmpfloat (g_array_index (modern, GimpVector2, i).x, ==, draft[i].x);
      g_assert_cmpfloat (g_array_index (modern, GimpVector2, i).y, ==, draft[i].y);
    }
  g_assert_cmpfloat (owned[0].x, !=, draft[0].x);
  g_array_unref (modern);
  gimp_mypaint_options_get_mapping_point (options, key, INPUT_PRESSURE, &again_count, &again);
  g_assert_cmpuint (again_count, ==, count);
  g_assert_true (again != owned);
  again[0].x = -51.;
  g_assert_cmpfloat (owned[0].x, ==, (gdouble) (gfloat) draft[0].x);
  g_free (again);
  assert_mapping_empty (options, "opaque-multiply", INPUT_RANDOM);
  g_assert_cmpuint (changed, ==, 0);
  g_assert_cmpuint (gimp_painter_mybrush_options_history_size (options), ==, history);
  g_assert_true (gimp_context_get_painter_mybrush (GIMP_CONTEXT (options)) == source);
  assert_draft_name (options, "Mapping read keeps pending name");
  g_object_get (options, "painter-dirty", &dirty, "painter-name-dirty", &name_dirty, NULL);
  g_assert_true (dirty);
  g_assert_true (name_dirty);
  actual = gimp_painter_mybrush_options_dup_json (options, &error);
  g_assert_no_error (error);
  g_assert_cmpstr (actual, ==, before);
  g_free (actual);
  actual = gimp_painter_mybrush_dup_json (source, &error);
  g_assert_no_error (error);
  g_assert_cmpstr (actual, ==, source_before);
  g_assert_cmpstr (gimp_object_get_name (source), ==, "Actual C mapping source");
  g_free (actual);
  /* Subsequent edits and final destruction cannot invalidate transfer-full C data. */
  g_assert_true (gimp_painter_mybrush_options_set_curve (
    options, BRUSH_OPAQUE_MULTIPLY, INPUT_PRESSURE, replacement, G_N_ELEMENTS (replacement), &error));
  g_assert_no_error (error);
  g_object_add_weak_pointer (G_OBJECT (options), &weak);
  g_object_unref (options);
  g_assert_null (weak);
  g_object_unref (source);
  for (i = 0; i < count; ++i)
    {
      g_assert_cmpfloat (owned[i].x, ==, (gdouble) (gfloat) draft[i].x);
      g_assert_cmpfloat (owned[i].y, ==, (gdouble) (gfloat) draft[i].y);
    }
  g_free (owned);
  g_free (before);
  g_free (source_before);
}

static void
options_mapping_point_fail_closed (void)
{
  GimpMypaintOptions *options = options_new ();
  GObject *wrong = g_object_new (G_TYPE_OBJECT, NULL);
  MappingConstructProbe *constructing = g_object_new (
    mapping_construct_probe_get_type (), "gimp", gimp, NULL);
  const gchar *invalid_names[] = {NULL, "", "no-such-setting", "Opaque", "OPAQUE",
                                  "opaqueMultiply", "non-incremental", "brushmark-name"};
  const gchar *bad_coordinates[] = {"1e39", "-1e39", "1e9999", "-1e9999"};
  const gdouble bad_values[] = {NAN, INFINITY, -INFINITY, 1e39, -1e39};
  const GimpVector2 valid[] = {{.1, .2}, {.9, .7}};
  GError *error = NULL;
  GimpVector2 sentinel = {-23., -51.};
  GimpVector2 *points = &sentinel;
  guint count = 999, i, coordinate;
  gchar key[] = "opaque";
  gchar *before, *after;
  GThread *thread;

  g_assert_true (constructing->queried);
  g_assert_false (constructing->parent_instance.binding_failed);
  g_object_unref (constructing);
  g_assert_true (gimp_painter_mybrush_options_set_curve (
    options, BRUSH_OPAQUE, INPUT_PRESSURE, valid, G_N_ELEMENTS (valid), &error));
  g_assert_no_error (error);
  before = gimp_painter_mybrush_options_dup_json (options, &error);
  g_assert_no_error (error);
  for (i = 0; i < G_N_ELEMENTS (invalid_names); ++i)
    assert_mapping_empty (options, invalid_names[i], INPUT_PRESSURE);
  assert_mapping_empty (NULL, "opaque", INPUT_PRESSURE);
  assert_mapping_empty ((GimpMypaintOptions *) wrong, "opaque", INPUT_PRESSURE);
  assert_mapping_empty (options, "opaque", INPUT_COUNT);
  assert_mapping_empty (options, "opaque", G_MAXUINT);
  gimp_mypaint_options_get_mapping_point (options, key, INPUT_PRESSURE, NULL, &points);
  g_assert_null (points);
  gimp_mypaint_options_get_mapping_point (options, key, INPUT_PRESSURE, &count, NULL);
  g_assert_cmpuint (count, ==, 0);
  gimp_mypaint_options_get_mapping_point (options, key, INPUT_PRESSURE, NULL, NULL);
  thread = g_thread_new ("mapping-c-read", mapping_query_wrong_thread, options);
  g_thread_join (thread);
  /* Resource validation rejects these before publication, so invalid coordinates
   * cannot be injected into a live draft through either supported setter. */
  for (i = 0; i < G_N_ELEMENTS (bad_values); ++i)
    for (coordinate = 0; coordinate < 2; ++coordinate)
      {
        GimpVector2 bad[] = {{.1, .2}, {.9, .7}};
        if (coordinate) bad[1].y = bad_values[i];
        else bad[1].x = bad_values[i];
        g_assert_false (gimp_painter_mybrush_options_set_curve (
          options, BRUSH_OPAQUE, INPUT_PRESSURE, bad, G_N_ELEMENTS (bad), &error));
        g_assert_nonnull (error);
        g_clear_error (&error);
      }
  for (i = 0; i < G_N_ELEMENTS (bad_coordinates); ++i)
    for (coordinate = 0; coordinate < 2; ++coordinate)
      {
        gchar *json = g_strdup_printf (
          "{\"version\":3,\"settings\":{\"opaque\":{\"inputs\":{"
          "\"pressure\":[[0.1,0.2],[%s,%s]]}}}}",
          coordinate ? "0.9" : bad_coordinates[i],
          coordinate ? bad_coordinates[i] : "0.7");
        g_assert_false (gimp_painter_mybrush_options_set_json (options, json, &error));
        g_assert_nonnull (error);
        g_clear_error (&error);
        g_free (json);
      }
  after = gimp_painter_mybrush_options_dup_json (options, &error);
  g_assert_no_error (error);
  g_assert_cmpstr (after, ==, before);
  g_free (after);
  g_free (before);
  gimp_mypaint_options_get_mapping_point (options, key, INPUT_PRESSURE, &count, &points);
  g_assert_cmpuint (count, ==, G_N_ELEMENTS (valid));
  g_assert_nonnull (points);
  for (i = 0; i < count; ++i)
    {
      g_assert_cmpfloat (points[i].x, ==, (gdouble) (gfloat) valid[i].x);
      g_assert_cmpfloat (points[i].y, ==, (gdouble) (gfloat) valid[i].y);
    }
  g_free (points);
  g_assert_true (gimp_painter_binding_close (G_OBJECT (options), &error));
  g_assert_no_error (error);
  assert_mapping_empty (options, "opaque", INPUT_PRESSURE);
  g_object_unref (options);
  g_object_unref (wrong);
}

static void
options_owned_roundtrip (void)
{
  GimpPainterMybrushOptions *options = options_new ();
  GimpPainterMybrushOptions *copy = options_new ();
  const GimpVector2 points[] = {{0., 0.25}, {1., 0.75}};
  GError *error = NULL;
  GArray *curve;
  gchar *json, *copied;
  guint i;

  g_assert_true (gimp_painter_mybrush_options_set_curve (
    options, BRUSH_OPAQUE, INPUT_PRESSURE, points, G_N_ELEMENTS (points), &error));
  g_assert_no_error (error);
  json = gimp_painter_mybrush_options_dup_json (options, &error);
  g_assert_no_error (error);
  g_assert_nonnull (json);
  g_assert_true (gimp_painter_mybrush_options_set_json (copy, json, &error));
  g_assert_no_error (error);
  copied = gimp_painter_mybrush_options_dup_json (copy, &error);
  g_assert_no_error (error);
  g_assert_cmpstr (copied, ==, json);
  g_free (copied);
  curve = gimp_painter_mybrush_options_get_curve (
    copy, BRUSH_OPAQUE, INPUT_PRESSURE, &error);
  g_assert_no_error (error);
  g_assert_nonnull (curve);
  g_assert_cmpuint (curve->len, ==, G_N_ELEMENTS (points));
  /* Both returned containers remain owned by C after their owners are gone. */
  g_object_unref (copy);
  g_object_unref (options);
  for (i = 0; i < curve->len; i++)
    {
      GimpVector2 actual = g_array_index (curve, GimpVector2, i);
      g_assert_cmpfloat (actual.x, ==, points[i].x);
      g_assert_cmpfloat (actual.y, ==, points[i].y);
    }
  g_assert_nonnull (strstr (json, "opaque"));
  g_array_unref (curve);
  g_free (json);
}

static void
assert_options_json (GimpPainterMybrushOptions *options,
                     const gchar               *expected)
{
  GError *error = NULL;
  gchar *actual = gimp_painter_mybrush_options_dup_json (options, &error);
  g_assert_no_error (error);
  g_assert_cmpstr (actual, ==, expected);
  g_free (actual);
}

static void
count_mapping_notify (GObject    *object,
                      GParamSpec *spec,
                      gpointer    data)
{
  guint *count = data;
  (void) object;
  (void) spec;
  ++*count;
}

static void
assert_mapping_setter_curve (GimpMypaintOptions *options,
                            gint                setting,
                            guint               input,
                            const GimpVector2  *expected,
                            guint               count)
{
  GError *error = NULL;
  GArray *curve = gimp_painter_mybrush_options_get_curve (
    options, setting, input, &error);
  guint i;

  g_assert_no_error (error);
  g_assert_nonnull (curve);
  g_assert_cmpuint (curve->len, ==, count);
  for (i = 0; i < count; ++i)
    {
      GimpVector2 point = g_array_index (curve, GimpVector2, i);
      g_assert_cmpfloat (point.x, ==, (gdouble) (gfloat) expected[i].x);
      g_assert_cmpfloat (point.y, ==, (gdouble) (gfloat) expected[i].y);
    }
  g_array_unref (curve);
}

static void
options_mapping_setter_constructing (void);

typedef struct
{
  GimpPainterMybrushOptions parent_instance;
  guint changed;
  guint notified;
  gboolean attempted;
} MappingSetterConstructProbe;
typedef struct { GimpPainterMybrushOptionsClass parent_class; }
  MappingSetterConstructProbeClass;
static gboolean mapping_setter_construct_enabled;
GType mapping_setter_construct_probe_get_type (void);
G_DEFINE_TYPE (MappingSetterConstructProbe, mapping_setter_construct_probe,
               GIMP_TYPE_PAINTER_MYBRUSH_OPTIONS)

static void
mapping_setter_construct_probe_constructed (GObject *object)
{
  MappingSetterConstructProbe *self = (MappingSetterConstructProbe *) object;
  GimpMypaintOptions *options = (GimpMypaintOptions *) object;
  if (mapping_setter_construct_enabled)
    {
      GimpVector2 replacement[] = {{0., .25}, {1., .75}};
      gchar key[] = "opaque-multiply";
      guint changed = self->changed, notified = self->notified;
      guint history = gimp_painter_mybrush_options_history_size (options);
      gimp_mypaint_options_set_mapping_point (options, key, INPUT_PRESSURE,
                                             G_N_ELEMENTS (replacement), replacement);
      gimp_mypaint_options_set_mapping_point (options, key, INPUT_PRESSURE, 0, NULL);
      g_assert_cmpuint (self->changed, ==, changed);
      g_assert_cmpuint (self->notified, ==, notified);
      g_assert_cmpuint (gimp_painter_mybrush_options_history_size (options), ==, history);
      self->attempted = TRUE;
    }
  G_OBJECT_CLASS (mapping_setter_construct_probe_parent_class)->constructed (object);
}

static void
mapping_setter_construct_probe_class_init (MappingSetterConstructProbeClass *klass)
{
  G_OBJECT_CLASS (klass)->constructed = mapping_setter_construct_probe_constructed;
}

static void
mapping_setter_construct_probe_init (MappingSetterConstructProbe *self)
{
  self->changed = self->notified = 0;
  self->attempted = FALSE;
  g_signal_connect (self, "settings-changed", G_CALLBACK (count_settings_changed), &self->changed);
  g_signal_connect (self, "notify", G_CALLBACK (count_mapping_notify), &self->notified);
}

static void
options_mapping_setter_constructing (void)
{
  MappingSetterConstructProbe *control, *attempt;
  const GimpVector2 defaults[] = {{0., 0.}, {1., 1.}};
  GError *error = NULL;
  gchar *before;
  guint history;
  gboolean dirty = TRUE;

  mapping_setter_construct_enabled = FALSE;
  control = g_object_new (mapping_setter_construct_probe_get_type (), "gimp", gimp, NULL);
  g_assert_false (control->attempted);
  g_assert_false (control->parent_instance.binding_failed);
  history = gimp_painter_mybrush_options_history_size ((GimpMypaintOptions *) control);
  before = gimp_painter_mybrush_options_dup_json ((GimpMypaintOptions *) control, &error);
  g_assert_no_error (error);
  mapping_setter_construct_enabled = TRUE;
  attempt = g_object_new (mapping_setter_construct_probe_get_type (), "gimp", gimp, NULL);
  mapping_setter_construct_enabled = FALSE;
  g_assert_true (attempt->attempted);
  g_assert_false (attempt->parent_instance.binding_failed);
  /* Compare after construction also, so notifications frozen by GObject
   * cannot hide an illicit setter notification until construction thaws. */
  g_assert_cmpuint (attempt->changed, ==, control->changed);
  g_assert_cmpuint (attempt->notified, ==, control->notified);
  g_assert_cmpuint (gimp_painter_mybrush_options_history_size ((GimpMypaintOptions *) attempt), ==, history);
  g_assert_cmpuint (gimp_painter_mybrush_options_history_size ((GimpMypaintOptions *) control), ==, history);
  assert_options_json ((GimpMypaintOptions *) attempt, before);
  assert_mapping_setter_curve ((GimpMypaintOptions *) attempt, BRUSH_OPAQUE_MULTIPLY,
                               INPUT_PRESSURE, defaults, G_N_ELEMENTS (defaults));
  g_object_get (attempt, "painter-dirty", &dirty, NULL);
  g_assert_false (dirty);
  g_free (before);
  g_object_unref (attempt);
  g_object_unref (control);
}

static void
options_mapping_setter_owned_draft (void)
{
  GimpMypaintOptions *options = options_new ();
  GimpPainterMybrush *source = GIMP_PAINTER_MYBRUSH (
    gimp_painter_mybrush_new (NULL, "Actual C mapping setter source"));
  const GimpVector2 expected[] = {{.1000000000003, .2000000000009},
                                 {.9000000000007, .7000000000011}};
  const gchar *aliases[] = {"opaque_multiply", "opaque-multiply",
                            "opaque multiply", "opaque/multiply",
                            "opaque\001multiply"};
  GError *error = NULL;
  GimpVector2 *points = g_new (GimpVector2, G_N_ELEMENTS (expected));
  GimpVector2 *owned = NULL;
  gchar *key = g_strdup ("opaque-multiply");
  gchar *source_before, *actual;
  GArray *native;
  guint changed = 0, notified = 0, history, count = 0, i;
  gboolean dirty = FALSE, name_dirty = FALSE;

  gimp_context_set_painter_mybrush (GIMP_CONTEXT (options), source);
  source_before = gimp_painter_mybrush_dup_json (source, &error);
  g_assert_no_error (error);
  g_assert_true (gimp_painter_mybrush_options_set_draft_name (
    options, "Mapping setter keeps pending name", &error));
  g_assert_no_error (error);
  history = gimp_painter_mybrush_options_history_size (options);
  g_signal_connect (options, "settings-changed", G_CALLBACK (count_settings_changed), &changed);
  g_signal_connect (options, "notify", G_CALLBACK (count_mapping_notify), &notified);
  memcpy (points, expected, sizeof expected);
  gimp_mypaint_options_set_mapping_point (options, key, INPUT_PRESSURE,
                                         G_N_ELEMENTS (expected), points);
  g_assert_cmpstr (key, ==, "opaque-multiply");
  g_assert_cmpmem (points, sizeof expected, expected, sizeof expected);
  g_assert_cmpuint (changed, ==, 1);
  g_assert_cmpuint (notified, >, 0);
  assert_mapping_setter_curve (options, BRUSH_OPAQUE_MULTIPLY, INPUT_PRESSURE,
                               expected, G_N_ELEMENTS (expected));
  /* Neither caller-owned buffer is retained by the draft. */
  memset (points, 0, sizeof expected);
  memset (key, 'x', strlen (key));
  g_free (points);
  g_free (key);
  assert_mapping_setter_curve (options, BRUSH_OPAQUE_MULTIPLY, INPUT_PRESSURE,
                               expected, G_N_ELEMENTS (expected));
  key = g_strdup ("opaque-multiply");
  gimp_mypaint_options_get_mapping_point (options, key, INPUT_PRESSURE, &count, &owned);
  g_assert_cmpuint (count, ==, G_N_ELEMENTS (expected));
  g_assert_nonnull (owned);
  for (i = 0; i < count; ++i)
    {
      g_assert_cmpfloat (owned[i].x, ==, (gdouble) (gfloat) expected[i].x);
      g_assert_cmpfloat (owned[i].y, ==, (gdouble) (gfloat) expected[i].y);
    }
  g_assert_cmpfloat (owned[0].x, !=, expected[0].x);
  g_free (owned);
  g_free (key);

  for (i = 0; i < G_N_ELEMENTS (aliases); ++i)
    {
      GimpVector2 replacement[] = {{-.25, .125}, {1.25, .875}};
      guint previous_notified = notified;
      key = g_strdup (aliases[i]);
      gimp_mypaint_options_set_mapping_point (options, key, INPUT_PRESSURE,
                                             G_N_ELEMENTS (replacement), replacement);
      g_assert_cmpstr (key, ==, aliases[i]);
      g_assert_cmpuint (changed, ==, i + 2);
      g_assert_cmpuint (notified, >, previous_notified);
      assert_mapping_setter_curve (options, BRUSH_OPAQUE_MULTIPLY, INPUT_PRESSURE,
                                   replacement, G_N_ELEMENTS (replacement));
      g_free (key);
    }
  /* The compatibility write does not change native double-valued writes. */
  g_assert_true (gimp_painter_mybrush_options_set_curve (
    options, BRUSH_OPAQUE, INPUT_PRESSURE, expected, G_N_ELEMENTS (expected), &error));
  g_assert_no_error (error);
  native = gimp_painter_mybrush_options_get_curve (
    options, BRUSH_OPAQUE, INPUT_PRESSURE, &error);
  g_assert_no_error (error);
  g_assert_nonnull (native);
  g_assert_cmpuint (native->len, ==, G_N_ELEMENTS (expected));
  for (i = 0; i < native->len; ++i)
    {
      g_assert_cmpfloat (g_array_index (native, GimpVector2, i).x, ==, expected[i].x);
      g_assert_cmpfloat (g_array_index (native, GimpVector2, i).y, ==, expected[i].y);
    }
  g_array_unref (native);
  g_assert_cmpuint (changed, ==, G_N_ELEMENTS (aliases) + 2);
  g_assert_cmpuint (gimp_painter_mybrush_options_history_size (options), ==, history);
  g_assert_true (gimp_context_get_painter_mybrush (GIMP_CONTEXT (options)) == source);
  assert_draft_name (options, "Mapping setter keeps pending name");
  g_object_get (options, "painter-dirty", &dirty, "painter-name-dirty", &name_dirty, NULL);
  g_assert_true (dirty);
  g_assert_true (name_dirty);
  actual = gimp_painter_mybrush_dup_json (source, &error);
  g_assert_no_error (error);
  g_assert_cmpstr (actual, ==, source_before);
  g_assert_cmpstr (gimp_object_get_name (source), ==, "Actual C mapping setter source");
  g_free (actual);
  g_free (source_before);
  g_object_unref (options);
  g_object_unref (source);
}

static void
options_mapping_setter_counts_rounding_clear (void)
{
  GimpMypaintOptions *options = options_new ();
  GimpMypaintOptions *copy = options_new ();
  GimpVector2 points[] = {{-2., -4.}, {-2., 3.}, {-.5, -.2}, {0., .3},
                          {.25, -.4}, {.5, .7}, {1., 2.}, {2., -3.}};
  GimpVector2 collapsed[] = {{.5 + DBL_EPSILON, .1234567890123},
                             {.5, .9876543210987}};
  GimpVector2 limits[] = {{-(gdouble) FLT_MAX, -(gdouble) FLT_MAX},
                          {(gdouble) FLT_MAX, (gdouble) FLT_MAX}};
  GimpVector2 ignored = {NAN, INFINITY};
  GError *error = NULL;
  gchar key[] = "opaque-multiply";
  gchar *json;
  guint count, changed = 0, history;
  gboolean dirty = FALSE;

  gimp_context_set_painter_mybrush (GIMP_CONTEXT (options), NULL);
  g_assert_null (gimp_context_get_painter_mybrush (GIMP_CONTEXT (options)));
  history = gimp_painter_mybrush_options_history_size (options);
  g_signal_connect (options, "settings-changed", G_CALLBACK (count_settings_changed), &changed);
  /* Every supported nonempty count, duplicate x and out-of-UI-range values. */
  for (count = 2; count <= G_N_ELEMENTS (points); ++count)
    {
      gimp_mypaint_options_set_mapping_point (options, key, INPUT_PRESSURE, count, points);
      g_assert_cmpuint (changed, ==, count - 1);
      assert_mapping_setter_curve (options, BRUSH_OPAQUE_MULTIPLY, INPUT_PRESSURE,
                                   points, count);
    }
  g_assert_cmpfloat (collapsed[0].x, >, collapsed[1].x);
  g_assert_cmpfloat ((gfloat) collapsed[0].x, ==, (gfloat) collapsed[1].x);
  gimp_mypaint_options_set_mapping_point (options, key, INPUT_PRESSURE,
                                         G_N_ELEMENTS (collapsed), collapsed);
  g_assert_cmpuint (changed, ==, 8);
  assert_mapping_setter_curve (options, BRUSH_OPAQUE_MULTIPLY, INPUT_PRESSURE,
                               collapsed, G_N_ELEMENTS (collapsed));
  gimp_mypaint_options_set_mapping_point (options, key, INPUT_PRESSURE,
                                         G_N_ELEMENTS (limits), limits);
  g_assert_cmpuint (changed, ==, 9);
  assert_mapping_setter_curve (options, BRUSH_OPAQUE_MULTIPLY, INPUT_PRESSURE,
                               limits, G_N_ELEMENTS (limits));
  /* NULL is valid for an explicit empty mapping, including the special
   * default pressure mapping. Saving and reading cannot resurrect it. */
  gimp_mypaint_options_set_mapping_point (options, key, INPUT_PRESSURE, 0, NULL);
  g_assert_cmpuint (changed, ==, 10);
  assert_mapping_empty (options, key, INPUT_PRESSURE);
  assert_mapping_setter_curve (options, BRUSH_OPAQUE_MULTIPLY, INPUT_PRESSURE, NULL, 0);
  json = gimp_painter_mybrush_options_dup_json (options, &error);
  g_assert_no_error (error);
  g_assert_true (gimp_painter_mybrush_options_set_json (copy, json, &error));
  g_assert_no_error (error);
  assert_mapping_empty (copy, key, INPUT_PRESSURE);
  assert_mapping_setter_curve (copy, BRUSH_OPAQUE_MULTIPLY, INPUT_PRESSURE, NULL, 0);
  assert_options_json (copy, json);
  /* Size zero also accepts a non-NULL buffer without reading its values. */
  gimp_mypaint_options_set_mapping_point (options, key, INPUT_PRESSURE, 0, &ignored);
  g_assert_cmpuint (changed, ==, 11);
  g_assert_true (isnan (ignored.x));
  g_assert_true (isinf (ignored.y));
  assert_options_json (options, json);
  assert_mapping_empty (options, key, INPUT_PRESSURE);
  g_assert_null (gimp_context_get_painter_mybrush (GIMP_CONTEXT (options)));
  g_assert_cmpuint (gimp_painter_mybrush_options_history_size (options), ==, history);
  g_object_get (options, "painter-dirty", &dirty, NULL);
  g_assert_true (dirty);
  g_assert_cmpstr (key, ==, "opaque-multiply");
  g_free (json);
  g_object_unref (copy);
  g_object_unref (options);
}

static gpointer
mapping_setter_wrong_thread (gpointer data)
{
  GimpVector2 points[] = {{0., .8}, {1., .9}};
  gchar key[] = "opaque";
  gimp_mypaint_options_set_mapping_point (data, key, INPUT_PRESSURE,
                                         G_N_ELEMENTS (points), points);
  return NULL;
}

static void
options_mapping_setter_invalid_atomic (void)
{
  GimpMypaintOptions *options = options_new ();
  GObject *wrong = g_object_new (G_TYPE_OBJECT, NULL);
  const gchar *invalid_names[] = {NULL, "", "unknown-setting", "Opaque", "OPAQUE",
                                  "opaqueMultiply", "opaque__multiply",
                                  "opaque\303\251multiply", "non-incremental", "brushmark-name"};
  const gdouble invalid_values[] = {NAN, INFINITY, -INFINITY, DBL_MAX, -DBL_MAX,
                                    1e39, -1e39};
  const guint invalid_counts[] = {1, 9, G_MAXUINT};
  GimpVector2 valid[] = {{.1, .2}, {.9, .7}};
  GimpVector2 descending[] = {{.9, .2}, {.1, .7}};
  GError *error = NULL;
  gchar key[] = "opaque";
  gchar *before;
  guint changed = 0, notified = 0, history, i, coordinate, point;
  gboolean dirty = FALSE, name_dirty = FALSE;
  GThread *thread;

  gimp_mypaint_options_set_mapping_point (options, key, INPUT_PRESSURE,
                                         G_N_ELEMENTS (valid), valid);
  assert_mapping_setter_curve (options, BRUSH_OPAQUE, INPUT_PRESSURE,
                               valid, G_N_ELEMENTS (valid));
  g_assert_true (gimp_painter_mybrush_options_set_draft_name (
    options, "Invalid mapping keeps pending name", &error));
  g_assert_no_error (error);
  before = gimp_painter_mybrush_options_dup_json (options, &error);
  g_assert_no_error (error);
  history = gimp_painter_mybrush_options_history_size (options);
  g_signal_connect (options, "settings-changed", G_CALLBACK (count_settings_changed), &changed);
  g_signal_connect (options, "notify", G_CALLBACK (count_mapping_notify), &notified);
  for (i = 0; i < G_N_ELEMENTS (invalid_names); ++i)
    {
      gchar *name = g_strdup (invalid_names[i]);
      gimp_mypaint_options_set_mapping_point (options, name, INPUT_PRESSURE,
                                             G_N_ELEMENTS (valid), valid);
      g_assert_cmpstr (name, ==, invalid_names[i]);
      g_free (name);
      assert_options_json (options, before);
    }
  for (i = 0; i < G_N_ELEMENTS (invalid_counts); ++i)
    {
      /* Deliberately only two allocated points: reject the count first. */
      gimp_mypaint_options_set_mapping_point (options, key, INPUT_PRESSURE,
                                             invalid_counts[i], valid);
      assert_options_json (options, before);
    }
  for (i = 2; i <= 8; ++i)
    gimp_mypaint_options_set_mapping_point (options, key, INPUT_PRESSURE, i, NULL);
  gimp_mypaint_options_set_mapping_point (options, key, INPUT_COUNT, 2, valid);
  gimp_mypaint_options_set_mapping_point (options, key, G_MAXUINT, 2, valid);
  gimp_mypaint_options_set_mapping_point (options, key, INPUT_PRESSURE, 2, descending);
  for (i = 0; i < G_N_ELEMENTS (invalid_values) + 2; ++i)
    for (coordinate = 0; coordinate < 2; ++coordinate)
      for (point = 0; point < 2; ++point)
        {
          GimpVector2 bad[] = {{.1, .2}, {.9, .7}};
          GimpVector2 unchanged[G_N_ELEMENTS (bad)];
          gdouble value = i < G_N_ELEMENTS (invalid_values) ? invalid_values[i] :
            (i == G_N_ELEMENTS (invalid_values) ? 1. : -1.) *
              nextafter ((gdouble) FLT_MAX, INFINITY);
          if (coordinate) bad[point].y = value;
          else bad[point].x = value;
          memcpy (unchanged, bad, sizeof bad);
          gimp_mypaint_options_set_mapping_point (options, key, INPUT_PRESSURE,
                                                 G_N_ELEMENTS (bad), bad);
          g_assert_cmpmem (bad, sizeof bad, unchanged, sizeof unchanged);
          assert_options_json (options, before);
        }
  gimp_mypaint_options_set_mapping_point (NULL, key, INPUT_PRESSURE, 2, valid);
  gimp_mypaint_options_set_mapping_point ((GimpMypaintOptions *) wrong, key,
                                         INPUT_PRESSURE, 2, valid);
  thread = g_thread_new ("mapping-c-write", mapping_setter_wrong_thread, options);
  g_thread_join (thread);
  assert_options_json (options, before);
  assert_mapping_setter_curve (options, BRUSH_OPAQUE, INPUT_PRESSURE,
                               valid, G_N_ELEMENTS (valid));
  assert_draft_name (options, "Invalid mapping keeps pending name");
  g_assert_cmpuint (changed, ==, 0);
  g_assert_cmpuint (notified, ==, 0);
  g_assert_cmpuint (gimp_painter_mybrush_options_history_size (options), ==, history);
  g_object_get (options, "painter-dirty", &dirty, "painter-name-dirty", &name_dirty, NULL);
  g_assert_true (dirty);
  g_assert_true (name_dirty);
  g_assert_true (gimp_painter_binding_close (G_OBJECT (options), &error));
  g_assert_no_error (error);
  changed = notified = 0;
  gimp_mypaint_options_set_mapping_point (options, key, INPUT_PRESSURE, 2, valid);
  gimp_mypaint_options_set_mapping_point (options, key, INPUT_PRESSURE, 0, NULL);
  g_assert_cmpuint (changed, ==, 0);
  g_assert_cmpuint (notified, ==, 0);
  assert_mapping_empty (options, key, INPUT_PRESSURE);
  g_free (before);
  g_object_unref (wrong);
  g_object_unref (options);
}

static void
options_prepare_restore_and_saved_json (void)
{
  guint with_icon;

  /* Saved custom drafts cover both an independent icon and intentional absence.
   * Both are deserialized after the same already-selected-source startup path. */
  for (with_icon = 0; with_icon < 2; ++with_icon)
    {
      GimpPainterMybrushOptions *options = options_new ();
      GimpPainterMybrushOptions *saved = options_new ();
      GimpPainterMybrushOptions *copy = options_new ();
      GimpPainterMybrushOptions *duplicate;
      GimpPainterMybrush *source = GIMP_PAINTER_MYBRUSH (
        gimp_painter_mybrush_new (NULL, "Actual C startup source"));
      GimpPainterMybrush *custom = GIMP_PAINTER_MYBRUSH (
        gimp_painter_mybrush_new (NULL, "Actual C saved custom draft"));
      GdkPixbuf *icon = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, 3, 5);
      GError *error = NULL;
      gchar *source_json, *custom_json, *config, *actual;
      const gchar *pending_name = "Startup keeps pending name";
      gboolean dirty = FALSE, conflict = FALSE, name_dirty = FALSE;
      guint history;

      g_assert_true (gimp_painter_mybrush_set_json (source,
        "{\"version\":3,\"group\":\"startup-source-group\","
        "\"unknown\":{\"startup_source\":43},\"settings\":{"
        "\"opaque\":{\"base_value\":0.27,\"inputs\":{\"pressure\":[[0,0.1],[1,0.9]]}},"
        "\"radius_logarithmic\":{\"base_value\":1.7}}}", &error));
      g_assert_no_error (error);
      gdk_pixbuf_fill (icon, 0xa04080ff);
      g_assert_true (gimp_painter_mybrush_set_icon (source, icon, &error));
      g_assert_no_error (error);
      gimp_context_set_painter_mybrush (GIMP_CONTEXT (options), source);
      g_assert_true (gimp_painter_mybrush_options_set_draft_name (options, pending_name, &error));
      g_assert_no_error (error);
      g_object_set (options, "opaque", 0.14, NULL);
      /* A changed selected source makes conflict nontrivial before preparation. */
      gdk_pixbuf_fill (icon, 0xc05090ff);
      g_assert_true (gimp_painter_mybrush_set_icon (source, icon, &error));
      g_assert_no_error (error);
      source_json = gimp_painter_mybrush_dup_json (source, &error);
      g_assert_no_error (error);
      /* Reload publishes the changed encoded source through settings-changed. */
      g_assert_true (gimp_painter_mybrush_set_json (source, source_json, &error));
      g_assert_no_error (error);
      g_object_get (options, "painter-dirty", &dirty, "painter-conflict", &conflict, NULL);
      g_assert_true (dirty);
      g_assert_true (conflict);
      history = gimp_painter_mybrush_options_history_size (options);
      /* Hold the caller's owner across native Reset, including its notify thaw. */
      gimp_config_reset (GIMP_CONFIG (options));
      g_assert_true (gimp_context_get_painter_mybrush (GIMP_CONTEXT (options)) == source);
      g_assert_true (gimp_painter_mybrush_options_prepare_restore (options, &error));
      g_assert_no_error (error);
      assert_options_json (options, source_json);
      assert_draft_name (options, pending_name);
      g_object_get (options, "painter-dirty", &dirty, "painter-conflict", &conflict,
                    "painter-name-dirty", &name_dirty, NULL);
      g_assert_false (dirty);
      g_assert_false (conflict);
      g_assert_true (name_dirty);
      g_assert_cmpuint (gimp_painter_mybrush_options_history_size (options), ==, history);
      /* Context setters intentionally do nothing for an already-current pointer. */
      gimp_context_set_painter_mybrush (GIMP_CONTEXT (options), source);
      assert_options_json (options, source_json);
      assert_draft_name (options, pending_name);
      g_assert_cmpuint (gimp_painter_mybrush_options_history_size (options), ==, history);

      g_assert_true (gimp_painter_mybrush_set_json (custom,
        "{\"version\":3,\"group\":\"saved-custom-group\","
        "\"unknown\":{\"saved_custom\":71},\"settings\":{"
        "\"opaque\":{\"base_value\":0.61,\"inputs\":{\"pressure\":[[0,0.3],[1,0.6]]}}}}",
        &error));
      g_assert_no_error (error);
      if (with_icon)
        {
          GdkPixbuf *custom_icon = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, 3, 5);
          gdk_pixbuf_fill (custom_icon, 0x30b070ff);
          g_assert_true (gimp_painter_mybrush_set_icon (custom, custom_icon, &error));
          g_assert_no_error (error);
          g_object_unref (custom_icon);
        }
      else
        g_assert_null (gimp_painter_mybrush_ref_icon (custom));
      custom_json = gimp_painter_mybrush_dup_json (custom, &error);
      g_assert_no_error (error);
      g_assert_cmpstr (custom_json, !=, source_json);
      g_assert_true (gimp_painter_mybrush_options_set_json (saved, custom_json, &error));
      g_assert_no_error (error);
      /* Save real GObject property values with the native config serializer.
       * No unrelated context selection is needed in this tool-options fixture. */
      gimp_context_set_serialize_properties (GIMP_CONTEXT (saved), 0);
      config = gimp_config_serialize_to_string (GIMP_CONFIG (saved), NULL);
      g_assert_nonnull (config);
      g_assert_nonnull (strstr (config, "(painter-settings \""));
      g_assert_nonnull (strstr (config, "saved_custom"));
      g_assert_true (gimp_config_deserialize_string (GIMP_CONFIG (options), config,
                                                   -1, NULL, &error));
      g_assert_no_error (error);
      assert_options_json (options, custom_json);
      assert_draft_name (options, pending_name);
      g_assert_true (gimp_context_get_painter_mybrush (GIMP_CONTEXT (options)) == source);
      /* Re-selecting the same resource must not erase the explicitly saved JSON. */
      gimp_context_set_painter_mybrush (GIMP_CONTEXT (options), source);
      assert_options_json (options, custom_json);
      duplicate = GIMP_PAINTER_MYBRUSH_OPTIONS (gimp_config_duplicate (GIMP_CONFIG (options)));
      g_assert_nonnull (duplicate);
      assert_options_json (duplicate, custom_json);
      assert_draft_name (duplicate, pending_name);
      g_assert_true (gimp_config_copy (GIMP_CONFIG (options), GIMP_CONFIG (copy), 0));
      assert_options_json (copy, custom_json);
      assert_draft_name (copy, pending_name);
      history = gimp_painter_mybrush_options_history_size (options);
      gimp_context_set_painter_mybrush (GIMP_CONTEXT (options), custom);
      g_assert_cmpuint (gimp_painter_mybrush_options_history_size (options), ==, history + 1);
      g_assert_true (gimp_painter_mybrush_options_restore_history (copy, history, &error));
      g_assert_no_error (error);
      assert_options_json (copy, custom_json);
      assert_draft_name (copy, pending_name);
      g_assert_true (gimp_context_get_painter_mybrush (GIMP_CONTEXT (copy)) == source);
      actual = gimp_painter_mybrush_dup_json (source, &error);
      g_assert_no_error (error);
      g_assert_cmpstr (actual, ==, source_json);
      g_free (actual);
      /* Check the live preview as well as its JSON, so pixbuf aliasing cannot
       * make custom icon B silently repaint selected source icon A. */
      {
        GdkPixbuf *source_icon = gimp_painter_mybrush_ref_icon (source);
        const guchar expected_pixel[] = {0xc0, 0x50, 0x90, 0xff};
        g_assert_nonnull (source_icon);
        g_assert_cmpint (gdk_pixbuf_get_width (source_icon), ==, 3);
        g_assert_cmpint (gdk_pixbuf_get_height (source_icon), ==, 5);
        g_assert_cmpmem (gdk_pixbuf_get_pixels (source_icon), sizeof expected_pixel,
                         expected_pixel, sizeof expected_pixel);
        g_object_unref (source_icon);
      }
      g_free (config);
      g_free (custom_json);
      g_free (source_json);
      g_object_unref (duplicate);
      g_object_unref (icon);
      g_object_unref (options);
      g_object_unref (saved);
      g_object_unref (copy);
      g_object_unref (source);
      g_object_unref (custom);
    }
}

static void
options_reject_and_close (void)
{
  GimpPainterMybrushOptions *options = options_new ();
  const GimpVector2 invalid[] = {{1., 0.}, {0., 1.}};
  GError *error = NULL;
  gchar *before = gimp_painter_mybrush_options_dup_json (options, &error);
  gchar *after;
  g_assert_no_error (error);
  g_assert_false (gimp_painter_mybrush_options_set_curve (
    options, BRUSH_OPAQUE, INPUT_PRESSURE, invalid, 2, &error));
  g_assert_nonnull (error);
  g_clear_error (&error);
  after = gimp_painter_mybrush_options_dup_json (options, &error);
  g_assert_no_error (error);
  g_assert_cmpstr (after, ==, before);
  g_free (after);
  g_assert_false (gimp_painter_mybrush_options_set_json (options, "{", &error));
  g_assert_nonnull (error);
  g_clear_error (&error);
  after = gimp_painter_mybrush_options_dup_json (options, &error);
  g_assert_no_error (error);
  g_assert_cmpstr (after, ==, before);
  g_free (after);
  g_free (before);
  g_assert_true (gimp_painter_binding_close (G_OBJECT (options), &error));
  g_assert_no_error (error);
  g_assert_null (gimp_painter_mybrush_options_dup_json (options, &error));
  g_assert_error (error, GIMP_PAINTER_ERROR, GIMP_PAINTER_ERROR_CLOSED);
  g_clear_error (&error);
  g_assert_false (gimp_painter_mybrush_options_prepare_restore (options, &error));
  g_assert_error (error, GIMP_PAINTER_ERROR, GIMP_PAINTER_ERROR_CLOSED);
  g_clear_error (&error);
  g_assert_null (gimp_painter_mybrush_options_dup_draft_name (options, &error));
  g_assert_error (error, GIMP_PAINTER_ERROR, GIMP_PAINTER_ERROR_CLOSED);
  g_clear_error (&error);
  g_assert_false (gimp_painter_mybrush_options_set_draft_name (options, "Closed", &error));
  g_assert_error (error, GIMP_PAINTER_ERROR, GIMP_PAINTER_ERROR_CLOSED);
  g_clear_error (&error);
  g_assert_false (gimp_painter_mybrush_options_restore_history (options, 0, &error));
  g_assert_error (error, GIMP_PAINTER_ERROR, GIMP_PAINTER_ERROR_CLOSED);
  g_clear_error (&error);
  g_object_unref (options);
}

static void
read_pixels (GimpDrawable *drawable, guchar *pixels)
{
  gegl_buffer_get (gimp_drawable_get_buffer (drawable),
    GEGL_RECTANGLE (0, 0, 64, 48), 1., babl_format ("R'G'B'A u8"), pixels,
    GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);
}

static void
session_native_transaction (void)
{
  GimpPainterMybrushOptions *options = options_new ();
  GimpPainterMybrush *source = GIMP_PAINTER_MYBRUSH (
    gimp_painter_mybrush_new (NULL, "Actual C transparent source"));
  GError *error = NULL;
  GimpPainterSession *session;
  GimpImage *image = gimp_image_new (gimp, 64, 48, GIMP_RGB,
                                     GIMP_PRECISION_U8_NON_LINEAR);
  GimpLayer *layer = gimp_layer_new (image, 64, 48, babl_format ("R'G'B'A u8"),
                                    "Actual C session", 1., GIMP_LAYER_MODE_NORMAL);
  GimpDrawable *drawable = GIMP_DRAWABLE (layer);
  GimpCoords coords = GIMP_COORDS_DEFAULT_VALUES;
  guchar before[64 * 48 * 4], painted[sizeof before], actual[sizeof before];
  gboolean split = 123;
  guint i;
  g_assert_true (gimp_image_add_layer (image, layer, NULL, 0, FALSE));
  gegl_buffer_clear (gimp_drawable_get_buffer (drawable), NULL);
  gimp_image_undo_free (image);
  read_pixels (drawable, before);
  /* A source lookup in place of the draft snapshot would paint no pixels. */
  g_assert_true (gimp_painter_mybrush_set_json (source,
    "{\"version\":3,\"settings\":{\"opaque\":{\"base_value\":0}}}", &error));
  g_assert_no_error (error);
  gimp_context_set_painter_mybrush (GIMP_CONTEXT (options), source);
  g_object_set (options, "opaque", 1., "dabs-per-second", 40.,
                "radius-logarithmic", 1.5, NULL);
  session = gimp_painter_session_new (options, &error);
  g_assert_no_error (error);
  g_assert_nonnull (session);
  coords.x = 16.; coords.y = 16.; coords.pressure = 0.;
  g_assert_true (gimp_painter_session_hover_to (
    session, drawable, .01, &coords, &split, &error));
  g_assert_no_error (error);
  /* The output is consumed only after the successful return. */
  g_assert_true (split == FALSE || split == TRUE);
  for (i = 0; i < 9; i++)
    {
      coords.pressure = .85; coords.x += 2.; coords.y += 1.; split = 123;
      g_assert_true (gimp_painter_session_stroke_to (
        session, drawable, .05, &coords, &split, &error));
      g_assert_no_error (error);
      g_assert_true (split == FALSE || split == TRUE);
      if (i == 3)
        {
          g_assert_true (gimp_painter_session_is_active (session));
          g_assert_cmpint (gimp_undo_stack_get_depth (gimp_image_get_undo_stack (image)), ==, 0);
          g_assert_true (gimp_painter_mybrush_options_set_draft_name (
            options, "Named during the same stroke", &error));
          g_assert_no_error (error);
          /* A spurious settings-changed would finish/split this live stroke. */
          g_assert_true (gimp_painter_session_is_active (session));
          g_assert_cmpint (gimp_undo_stack_get_depth (gimp_image_get_undo_stack (image)), ==, 0);
          {
            GimpVector2 *mapping = NULL;
            guint count = 0;
            gchar key[] = "opaque-multiply";
            guchar query_pixels[sizeof actual];
            read_pixels (drawable, actual);
            gimp_mypaint_options_get_mapping_point (options, key, INPUT_PRESSURE, &count, &mapping);
            g_assert_cmpuint (count, ==, 2);
            g_assert_nonnull (mapping);
            g_free (mapping);
            read_pixels (drawable, query_pixels);
            g_assert_cmpmem (actual, sizeof actual, query_pixels, sizeof query_pixels);
            g_assert_true (gimp_painter_session_is_active (session));
            g_assert_cmpint (gimp_undo_stack_get_depth (gimp_image_get_undo_stack (image)), ==, 0);
          }
          g_assert_cmpstr (gimp_object_get_name (source), ==, "Actual C transparent source");
        }
    }
  g_assert_true (gimp_painter_session_finish (session, &error));
  g_assert_no_error (error);
  read_pixels (drawable, painted);
  g_assert_cmpint (memcmp (painted, before, sizeof before), !=, 0);
  g_assert_cmpint (gimp_undo_stack_get_depth (gimp_image_get_undo_stack (image)), ==, 1);
  g_assert_true (gimp_image_undo (image));
  read_pixels (drawable, actual);
  g_assert_cmpmem (actual, sizeof actual, before, sizeof before);
  g_assert_true (gimp_image_redo (image));
  read_pixels (drawable, actual);
  g_assert_cmpmem (actual, sizeof actual, painted, sizeof painted);
  coords.x = 40.; coords.y = 32.;
  g_assert_true (gimp_painter_session_stroke_to (
    session, drawable, .1, &coords, NULL, &error));
  g_assert_no_error (error);
  g_assert_true (gimp_painter_session_cancel (session, &error));
  g_assert_no_error (error);
  read_pixels (drawable, actual);
  g_assert_cmpmem (actual, sizeof actual, painted, sizeof painted);
  g_assert_false (gimp_painter_session_is_active (session));
  g_object_unref (session); g_object_unref (options); g_object_unref (source); g_object_unref (image);
}

static void
session_mapping_setter_transaction (void)
{
  GimpMypaintOptions *options = options_new ();
  GimpPainterMybrush *source = GIMP_PAINTER_MYBRUSH (
    gimp_painter_mybrush_new (NULL, "Actual C setter transparent source"));
  GError *error = NULL;
  GimpPainterSession *session;
  GimpImage *image = gimp_image_new (gimp, 64, 48, GIMP_RGB,
                                     GIMP_PRECISION_U8_NON_LINEAR);
  GimpLayer *layer = gimp_layer_new (image, 64, 48, babl_format ("R'G'B'A u8"),
                                    "Actual C mapping transaction", 1., GIMP_LAYER_MODE_NORMAL);
  GimpDrawable *drawable = GIMP_DRAWABLE (layer);
  GimpCoords coords = GIMP_COORDS_DEFAULT_VALUES;
  GimpVector2 muted[] = {{0., 0.}, {1., 0.}};
  GimpVector2 visible[] = {{0., 1.}, {1., 1.}};
  GimpVector2 invalid[] = {{0., 0.}, {1., NAN}};
  GimpVector2 descending[] = {{1., 0.}, {0., 1.}};
  guchar before[64 * 48 * 4], first[sizeof before], second[sizeof before];
  guchar actual[sizeof before], live_before[sizeof before];
  gchar key[] = "opaque-multiply";
  gchar *source_before, *source_after, *draft_before;
  guint changed = 0, notified = 0, history, i;

  g_assert_true (gimp_image_add_layer (image, layer, NULL, 0, FALSE));
  gegl_buffer_clear (gimp_drawable_get_buffer (drawable), NULL);
  gimp_image_undo_free (image);
  read_pixels (drawable, before);
  g_assert_true (gimp_painter_mybrush_set_json (source,
    "{\"version\":3,\"settings\":{\"opaque\":{\"base_value\":0}}}", &error));
  g_assert_no_error (error);
  source_before = gimp_painter_mybrush_dup_json (source, &error);
  g_assert_no_error (error);
  gimp_context_set_painter_mybrush (GIMP_CONTEXT (options), source);
  g_object_set (options, "opaque", 1., "opaque-multiply", 0.,
                "dabs-per-second", 40., "radius-logarithmic", 1.5, NULL);
  session = gimp_painter_session_new (options, &error);
  g_assert_no_error (error);
  g_assert_nonnull (session);
  g_signal_connect (options, "settings-changed", G_CALLBACK (count_settings_changed), &changed);
  g_signal_connect (options, "notify", G_CALLBACK (count_mapping_notify), &notified);
  history = gimp_painter_mybrush_options_history_size (options);
  coords.x = 12.; coords.y = 12.; coords.pressure = 0.;
  g_assert_true (gimp_painter_session_hover_to (
    session, drawable, .01, &coords, NULL, &error));
  g_assert_no_error (error);
  for (i = 0; i < 9; ++i)
    {
      coords.x += 2.; coords.y += .5; coords.pressure = .85;
      g_assert_true (gimp_painter_session_stroke_to (
        session, drawable, .05, &coords, NULL, &error));
      g_assert_no_error (error);
      if (i == 3)
        {
          g_assert_true (gimp_painter_session_is_active (session));
          g_assert_cmpint (gimp_undo_stack_get_depth (gimp_image_get_undo_stack (image)), ==, 0);
          read_pixels (drawable, live_before);
          g_assert_cmpint (memcmp (live_before, before, sizeof before), !=, 0);
          draft_before = gimp_painter_mybrush_options_dup_json (options, &error);
          g_assert_no_error (error);
          /* Invalid legacy edits must not emit the signal that commits a
           * live session, even when the first point could be published. */
          gimp_mypaint_options_set_mapping_point (options, key, INPUT_PRESSURE, 1, muted);
          gimp_mypaint_options_set_mapping_point (options, key, INPUT_PRESSURE, 2, invalid);
          gimp_mypaint_options_set_mapping_point (options, key, INPUT_PRESSURE, 2, descending);
          g_assert_cmpuint (changed, ==, 0);
          g_assert_cmpuint (notified, ==, 0);
          assert_options_json (options, draft_before);
          g_free (draft_before);
          g_assert_true (gimp_painter_session_is_active (session));
          g_assert_cmpint (gimp_undo_stack_get_depth (gimp_image_get_undo_stack (image)), ==, 0);
          read_pixels (drawable, actual);
          g_assert_cmpmem (actual, sizeof actual, live_before, sizeof live_before);
        }
    }
  g_assert_true (gimp_painter_session_is_active (session));
  read_pixels (drawable, first);
  g_assert_cmpint (memcmp (first, before, sizeof before), !=, 0);
  gimp_mypaint_options_set_mapping_point (options, key, INPUT_PRESSURE, 2, muted);
  g_assert_cmpuint (changed, ==, 1);
  g_assert_cmpuint (notified, >, 0);
  g_assert_false (gimp_painter_session_is_active (session));
  g_assert_cmpint (gimp_undo_stack_get_depth (gimp_image_get_undo_stack (image)), ==, 1);
  read_pixels (drawable, actual);
  g_assert_cmpmem (actual, sizeof actual, first, sizeof first);
  /* The settings notification already committed the prior transaction. */
  g_assert_true (gimp_painter_session_finish (session, &error));
  g_assert_no_error (error);
  g_assert_true (gimp_painter_session_cancel (session, &error));
  g_assert_no_error (error);
  g_assert_cmpint (gimp_undo_stack_get_depth (gimp_image_get_undo_stack (image)), ==, 1);
  read_pixels (drawable, actual);
  g_assert_cmpmem (actual, sizeof actual, first, sizeof first);

  /* The next stroke samples the new zero curve, rather than the old snapshot. */
  coords.x = 34.; coords.y = 32.; coords.pressure = 0.;
  g_assert_true (gimp_painter_session_hover_to (
    session, drawable, .01, &coords, NULL, &error));
  g_assert_no_error (error);
  for (i = 0; i < 9; ++i)
    {
      coords.x += 2.; coords.pressure = .85;
      g_assert_true (gimp_painter_session_stroke_to (
        session, drawable, .05, &coords, NULL, &error));
      g_assert_no_error (error);
    }
  g_assert_true (gimp_painter_session_finish (session, &error));
  g_assert_no_error (error);
  read_pixels (drawable, actual);
  g_assert_cmpmem (actual, sizeof actual, first, sizeof first);
  g_assert_cmpint (gimp_undo_stack_get_depth (gimp_image_get_undo_stack (image)), ==, 1);
  gimp_mypaint_options_set_mapping_point (options, key, INPUT_PRESSURE, 2, visible);
  g_assert_cmpuint (changed, ==, 2);
  g_assert_cmpint (gimp_undo_stack_get_depth (gimp_image_get_undo_stack (image)), ==, 1);

  /* A further setter edit really paints at the same location; cancel rolls
   * back only this new stroke and leaves the earlier committed stroke. */
  coords.x = 34.; coords.y = 32.; coords.pressure = 0.;
  g_assert_true (gimp_painter_session_hover_to (
    session, drawable, .01, &coords, NULL, &error));
  g_assert_no_error (error);
  for (i = 0; i < 9; ++i)
    {
      coords.x += 2.; coords.pressure = .85;
      g_assert_true (gimp_painter_session_stroke_to (
        session, drawable, .05, &coords, NULL, &error));
      g_assert_no_error (error);
    }
  read_pixels (drawable, actual);
  g_assert_cmpint (memcmp (actual, first, sizeof first), !=, 0);
  g_assert_true (gimp_painter_session_is_active (session));
  g_assert_true (gimp_painter_session_cancel (session, &error));
  g_assert_no_error (error);
  g_assert_false (gimp_painter_session_is_active (session));
  read_pixels (drawable, actual);
  g_assert_cmpmem (actual, sizeof actual, first, sizeof first);
  g_assert_cmpint (gimp_undo_stack_get_depth (gimp_image_get_undo_stack (image)), ==, 1);

  coords.x = 34.; coords.y = 32.; coords.pressure = 0.;
  g_assert_true (gimp_painter_session_hover_to (
    session, drawable, .01, &coords, NULL, &error));
  g_assert_no_error (error);
  for (i = 0; i < 9; ++i)
    {
      coords.x += 2.; coords.pressure = .85;
      g_assert_true (gimp_painter_session_stroke_to (
        session, drawable, .05, &coords, NULL, &error));
      g_assert_no_error (error);
    }
  g_assert_true (gimp_painter_session_finish (session, &error));
  g_assert_no_error (error);
  g_assert_false (gimp_painter_session_is_active (session));
  read_pixels (drawable, second);
  g_assert_cmpint (memcmp (second, first, sizeof first), !=, 0);
  g_assert_cmpint (gimp_undo_stack_get_depth (gimp_image_get_undo_stack (image)), ==, 2);
  g_assert_true (gimp_image_undo (image));
  read_pixels (drawable, actual);
  g_assert_cmpmem (actual, sizeof actual, first, sizeof first);
  g_assert_true (gimp_image_undo (image));
  read_pixels (drawable, actual);
  g_assert_cmpmem (actual, sizeof actual, before, sizeof before);
  g_assert_true (gimp_image_redo (image));
  read_pixels (drawable, actual);
  g_assert_cmpmem (actual, sizeof actual, first, sizeof first);
  g_assert_true (gimp_image_redo (image));
  read_pixels (drawable, actual);
  g_assert_cmpmem (actual, sizeof actual, second, sizeof second);
  g_assert_cmpint (gimp_undo_stack_get_depth (gimp_image_get_undo_stack (image)), ==, 2);
  g_assert_cmpuint (changed, ==, 2);
  g_assert_cmpuint (gimp_painter_mybrush_options_history_size (options), ==, history);
  g_assert_true (gimp_context_get_painter_mybrush (GIMP_CONTEXT (options)) == source);
  source_after = gimp_painter_mybrush_dup_json (source, &error);
  g_assert_no_error (error);
  g_assert_cmpstr (source_after, ==, source_before);
  g_assert_cmpstr (gimp_object_get_name (source), ==, "Actual C setter transparent source");
  g_assert_null (gimp_painter_session_dup_error (session));
  g_free (source_after);
  g_free (source_before);
  g_object_unref (session);
  g_object_unref (options);
  g_object_unref (source);
  g_object_unref (image);
}

static void
session_errors_and_close (void)
{
  GimpPainterMybrushOptions *options = options_new ();
  GError *error = NULL;
  GimpPainterSession *session = gimp_painter_session_new (options, &error);
  GimpCoords coords = GIMP_COORDS_DEFAULT_VALUES;
  gboolean split;
  g_assert_no_error (error);
  g_assert_nonnull (session);
  g_assert_false (gimp_painter_session_stroke_to (
    session, NULL, .01, NULL, &split, &error));
  g_assert_nonnull (error);
  g_clear_error (&error);
  /* On FALSE the optional output is unspecified: do not inspect it. */
  g_assert_true (gimp_painter_binding_close (G_OBJECT (session), &error));
  g_assert_no_error (error);
  g_assert_false (gimp_painter_session_hover_to (
    session, NULL, .01, &coords, &split, &error));
  g_assert_error (error, GIMP_PAINTER_ERROR, GIMP_PAINTER_ERROR_CLOSED);
  g_clear_error (&error);
  g_assert_false (gimp_painter_session_finish (session, &error));
  g_assert_error (error, GIMP_PAINTER_ERROR, GIMP_PAINTER_ERROR_CLOSED);
  g_clear_error (&error);
  g_assert_true (gimp_painter_binding_close (G_OBJECT (session), &error));
  g_assert_no_error (error);
  g_object_unref (session); g_object_unref (options);
}

int
main (int argc, char **argv)
{
  gint result;
  g_test_init (&argc, &argv, NULL);
  gimp_test_utils_set_gimp3_directory ("GIMP_TESTING_ABS_TOP_SRCDIR", "app/tests/gimpdir");
  gimp = gimp_init_for_testing ();
  g_test_add_func ("/painter-c/options-owned-roundtrip", options_owned_roundtrip);
  g_test_add_func ("/painter-c/options-mapping-point-owned-draft", options_mapping_point_owned_draft);
  g_test_add_func ("/painter-c/options-mapping-point-fail-closed", options_mapping_point_fail_closed);
  g_test_add_func ("/painter-c/options-mapping-setter-owned-draft", options_mapping_setter_owned_draft);
  g_test_add_func ("/painter-c/options-mapping-setter-counts-rounding-clear", options_mapping_setter_counts_rounding_clear);
  g_test_add_func ("/painter-c/options-mapping-setter-invalid-atomic", options_mapping_setter_invalid_atomic);
  g_test_add_func ("/painter-c/options-mapping-setter-constructing", options_mapping_setter_constructing);
  g_test_add_func ("/painter-c/options-draft-name-history-copy", options_draft_name_history_copy);
  g_test_add_func ("/painter-c/options-prepare-restore-saved-json", options_prepare_restore_and_saved_json);
  g_test_add_func ("/painter-c/options-reject-close", options_reject_and_close);
  g_test_add_func ("/painter-c/session-native-transaction", session_native_transaction);
  g_test_add_func ("/painter-c/session-mapping-setter-transaction", session_mapping_setter_transaction);
  g_test_add_func ("/painter-c/session-errors-close", session_errors_and_close);
  result = g_test_run ();
  gimp_test_utils_set_gimp3_directory ("GIMP_TESTING_ABS_TOP_BUILDDIR", "app/tests/gimpdir-output");
  gimp_exit (gimp, TRUE);
  return result;
}
