/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "test-fixture.h"
#include "gimp-painter-binding.h"

typedef struct _PainterFixtureClass { GObjectClass parent_class; } PainterFixtureClass;
struct _PainterFixture { GObject parent_instance; };
G_DEFINE_TYPE (PainterFixture, painter_fixture, G_TYPE_OBJECT)

static void constructed (GObject *owner)
{
  G_OBJECT_CLASS (painter_fixture_parent_class)->constructed (owner);
  painter_fixture_binding_constructed (owner);
}
static void dispose (GObject *owner)
{
  GError *error = NULL;
  g_assert_true (gimp_painter_binding_close (owner, &error));
  g_assert_no_error (error);
  G_OBJECT_CLASS (painter_fixture_parent_class)->dispose (owner);
}
static void set_property (GObject *owner, guint id, const GValue *value, GParamSpec *spec)
{
  if (id == 1) painter_fixture_binding_set (owner, g_value_get_int (value));
  else G_OBJECT_WARN_INVALID_PROPERTY_ID (owner, id, spec);
}
static void get_property (GObject *owner, guint id, GValue *value, GParamSpec *spec)
{
  if (id == 1) g_value_set_int (value, painter_fixture_binding_get (owner));
  else G_OBJECT_WARN_INVALID_PROPERTY_ID (owner, id, spec);
}
static void painter_fixture_class_init (PainterFixtureClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  object_class->constructed = constructed;
  object_class->dispose = dispose;
  object_class->set_property = set_property;
  object_class->get_property = get_property;
  g_object_class_install_property (object_class, 1,
    g_param_spec_int ("value", "value", "value", 0, 99, 7,
                     G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
}
static void painter_fixture_init (PainterFixture *self)
{
  painter_fixture_binding_init (G_OBJECT (self));
}

/* Native C registration and vfuncs; only implementation state lives in C++. */
typedef struct _PainterPropertyFixtureClass { GObjectClass parent_class; } PainterPropertyFixtureClass;
struct _PainterPropertyFixture { GObject parent_instance; };
G_DEFINE_TYPE (PainterPropertyFixture, painter_property_fixture, G_TYPE_OBJECT)

GType painter_property_mode_get_type (void)
{
  static gsize type;
  static const GEnumValue values[] = {{2, "TWO", "two"}, {5, "FIVE", "five"}, {0, NULL, NULL}};
  if (g_once_init_enter (&type))
    g_once_init_leave (&type, g_enum_register_static ("PainterPropertyMode", values));
  return type;
}
GType painter_property_flags_get_type (void)
{
  static gsize type;
  static const GFlagsValue values[] = {{1, "FIRST", "first"}, {4, "FOURTH", "fourth"}, {0, NULL, NULL}};
  if (g_once_init_enter (&type))
    g_once_init_leave (&type, g_flags_register_static ("PainterPropertyFlags", values));
  return type;
}
static void property_constructed (GObject *owner)
{
  G_OBJECT_CLASS (painter_property_fixture_parent_class)->constructed (owner);
  painter_property_binding_constructed (owner);
}
static void property_dispose (GObject *owner)
{
  GError *error = NULL;
  g_assert_true (gimp_painter_binding_close (owner, &error));
  g_assert_no_error (error);
  G_OBJECT_CLASS (painter_property_fixture_parent_class)->dispose (owner);
}
static void property_set (GObject *owner, guint id, const GValue *value, GParamSpec *spec)
{
  if (id > 0 && id < PAINTER_PROPERTY_COUNT)
    painter_property_binding_set (owner, id, value, spec);
  else G_OBJECT_WARN_INVALID_PROPERTY_ID (owner, id, spec);
}
static void property_get (GObject *owner, guint id, GValue *value, GParamSpec *spec)
{
  if (id > 0 && id < PAINTER_PROPERTY_COUNT)
    painter_property_binding_get (owner, id, value, spec);
  else G_OBJECT_WARN_INVALID_PROPERTY_ID (owner, id, spec);
}
static void painter_property_fixture_class_init (PainterPropertyFixtureClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GParamSpec *specs[PAINTER_PROPERTY_COUNT] = {NULL};
  GParamFlags rw = G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS;
  object_class->constructed = property_constructed;
  object_class->dispose = property_dispose;
  object_class->set_property = property_set;
  object_class->get_property = property_get;
  specs[PAINTER_PROPERTY_INT] = g_param_spec_int ("count", "count", "count", -9, 99, 7, rw | G_PARAM_CONSTRUCT);
  specs[PAINTER_PROPERTY_BOOL] = g_param_spec_boolean ("enabled", "enabled", "enabled", TRUE, rw | G_PARAM_EXPLICIT_NOTIFY);
  specs[PAINTER_PROPERTY_DOUBLE] = g_param_spec_double ("ratio", "ratio", "ratio", -2, 2, 0.25, rw);
  specs[PAINTER_PROPERTY_STRING] = g_param_spec_string ("label", "label", "label", "initial", rw);
  specs[PAINTER_PROPERTY_OBJECT] = g_param_spec_object ("peer", "peer", "peer", G_TYPE_OBJECT, rw);
  specs[PAINTER_PROPERTY_BYTES] = g_param_spec_boxed ("bytes", "bytes", "bytes", G_TYPE_BYTES, rw);
  specs[PAINTER_PROPERTY_ENUM] = g_param_spec_enum ("mode", "mode", "mode", painter_property_mode_get_type (), 2, rw);
  specs[PAINTER_PROPERTY_FLAGS] = g_param_spec_flags ("flags", "flags", "flags", painter_property_flags_get_type (), 1, rw);
  specs[PAINTER_PROPERTY_UINT64] = g_param_spec_uint64 ("serial", "serial", "serial", 0, G_MAXUINT64, G_GUINT64_CONSTANT (9007199254740993), rw);
  g_object_class_install_properties (object_class, PAINTER_PROPERTY_COUNT, specs);
}
static void painter_property_fixture_init (PainterPropertyFixture *self)
{
  painter_property_binding_init (G_OBJECT (self));
}
