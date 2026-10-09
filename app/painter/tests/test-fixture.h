/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef PAINTER_TEST_FIXTURE_H
#define PAINTER_TEST_FIXTURE_H
#include <glib-object.h>
G_BEGIN_DECLS
typedef struct _PainterFixture PainterFixture;
GType painter_fixture_get_type (void);
void painter_fixture_binding_init (GObject *owner);
void painter_fixture_binding_constructed (GObject *owner);
void painter_fixture_binding_set (GObject *owner, gint value);
gint painter_fixture_binding_get (GObject *owner);
typedef struct _PainterPropertyFixture PainterPropertyFixture;
GType painter_property_fixture_get_type (void);
GType painter_property_mode_get_type (void);
GType painter_property_flags_get_type (void);
enum {
  PAINTER_PROPERTY_INT = 1, PAINTER_PROPERTY_BOOL, PAINTER_PROPERTY_DOUBLE,
  PAINTER_PROPERTY_STRING, PAINTER_PROPERTY_OBJECT, PAINTER_PROPERTY_BYTES,
  PAINTER_PROPERTY_ENUM, PAINTER_PROPERTY_FLAGS, PAINTER_PROPERTY_UINT64,
  PAINTER_PROPERTY_COUNT
};
void painter_property_binding_init (GObject *owner);
void painter_property_binding_constructed (GObject *owner);
void painter_property_binding_set (GObject *owner, guint id, const GValue *value,
                                   GParamSpec *spec);
void painter_property_binding_get (GObject *owner, guint id, GValue *value,
                                   GParamSpec *spec);
G_END_DECLS
#endif
