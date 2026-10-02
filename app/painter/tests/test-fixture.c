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
