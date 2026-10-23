/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef PAINTER_TEST_HIERARCHY_H
#define PAINTER_TEST_HIERARCHY_H
#include <glib-object.h>
G_BEGIN_DECLS
typedef struct _PainterHierarchyBase PainterHierarchyBase;
typedef struct _PainterHierarchyChild PainterHierarchyChild;
typedef struct _PainterReadable PainterReadable;
typedef struct {
  guint base_set, base_get, child_set, child_get;
} PainterPropertyTrace;
typedef struct _PainterReadableInterface {
  GTypeInterface parent;
  gint (*read) (PainterReadable *, GError **);
} PainterReadableInterface;
GType painter_hierarchy_base_get_type (void);
GType painter_hierarchy_child_get_type (void);
GType painter_readable_get_type (void);
gint painter_readable_read (PainterReadable *self, GError **error);
void painter_hierarchy_init_binding (GObject *owner, gboolean child);
void painter_hierarchy_activate_binding (GObject *owner);
void painter_hierarchy_set (GObject *owner, gboolean child, gint value);
gint painter_hierarchy_get (GObject *owner, gboolean child);
gint painter_hierarchy_read (PainterReadable *owner, GError **error);
void painter_hierarchy_parent_dispose (GObject *owner);
void painter_hierarchy_finalize_phase (gint phase);
void painter_hierarchy_reset_property_trace (void);
PainterPropertyTrace painter_hierarchy_property_trace (void);
G_END_DECLS
#endif
