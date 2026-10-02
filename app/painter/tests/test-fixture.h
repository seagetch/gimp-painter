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
G_END_DECLS
#endif
